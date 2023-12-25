/**
 * @file lldaeloader.cpp
 * @brief LLDAELoader class implementation
 *
 * $LicenseInfo:firstyear=2013&license=viewergpl$
 *
 * Copyright (c) 2013, Linden Research, Inc.
 *
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
 *
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at
 * http://secondlifegrid.net/programs/open_source/licensing/flossexception
 *
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 *
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include <regex>

#include "boost/algorithm/string/replace.hpp"
#include "boost/lexical_cast.hpp"

// gcc 13 does not like the colladom headers...
#if defined(GCC_VERSION) && GCC_VERSION >= 130000
# pragma GCC diagnostic ignored "-Woverloaded-virtual"
#endif

#include "dae.h"
#include "dae/daeErrorHandler.h"
#include "dom/domAsset.h"
#include "dom/domBind_material.h"
#include "dom/domCOLLADA.h"
#include "dom/domConstants.h"
#include "dom/domController.h"
#include "dom/domEffect.h"
#include "dom/domGeometry.h"
#include "dom/domInstance_geometry.h"
#include "dom/domInstance_material.h"
#include "dom/domInstance_node.h"
#include "dom/domInstance_effect.h"
#include "dom/domMaterial.h"
#include "dom/domMatrix.h"
#include "dom/domNode.h"
#include "dom/domProfile_COMMON.h"
#include "dom/domRotate.h"
#include "dom/domScale.h"
#include "dom/domTranslate.h"
#include "dom/domVisual_scene.h"

#include "lldaeloader.h"

#include "llmatrix4a.h"
#include "lljoint.h"
#include "llsdserialize.h"
#include "llstring.h"
#include "lluri.h"

std::string colladaVersion[VERSIONTYPE_COUNT+1] =
{
	"1.4.0",
	"1.4.1",
	"Unsupported"
};

static const std::string lod_suffix[LLModel::NUM_LODS] =
{
	"_LOD0",
	"_LOD1",
	"_LOD2",
	"",
	"_PHYS",
};

constexpr U32 LIMIT_MATERIALS_OUTPUT = 12;

//-----------------------------------------------------------------------------
// DAE error logger
//-----------------------------------------------------------------------------

class LLDaeErrorHandler : public daeErrorHandler
{
protected:
	LOG_CLASS(LLDaeErrorHandler);

public:
	virtual void handleError(daeString msg)
	{
		llwarns << "Error in DAE file: " << msg << llendl;
	}

	virtual void handleWarning(daeString msg)
	{
		llwarns << msg << llendl;
	}
};

LLDaeErrorHandler gDaeErrorHandler;

class LLSetDaeErrorHandler
{
public:
	LLSetDaeErrorHandler()
	{
		daeErrorHandler::setErrorHandler(&gDaeErrorHandler);
	}

	~LLSetDaeErrorHandler()
	{
		daeErrorHandler::setErrorHandler(NULL);
	}
};

//-----------------------------------------------------------------------------

bool get_dom_sources(const domInputLocalOffset_Array& inputs, S32& pos_offset,
					 S32& tc_offset, S32& norm_offset, S32& idx_stride,
					 domSource*& pos_source, domSource*& tc_source,
					 domSource*& norm_source)
{
	idx_stride = 0;

	for (U32 j = 0; j < inputs.getCount(); ++j)
	{
		idx_stride = llmax((S32)inputs[j]->getOffset(), idx_stride);

		if (strcmp(COMMON_PROFILE_INPUT_VERTEX, inputs[j]->getSemantic()) == 0)
		{
			// Found vertex array
			const domURIFragmentType& uri = inputs[j]->getSource();
			daeElementRef elem = uri.getElement();
			domVertices* vertices = (domVertices*)elem.cast();
			if (!vertices)
			{
				return false;
			}

			domInputLocal_Array& v_inp = vertices->getInput_array();

			for (U32 k = 0; k < v_inp.getCount(); ++k)
			{
				if (strcmp(COMMON_PROFILE_INPUT_POSITION,
						   v_inp[k]->getSemantic()) == 0)
				{
					pos_offset = inputs[j]->getOffset();

					const domURIFragmentType& uri = v_inp[k]->getSource();
					daeElementRef elem = uri.getElement();
					pos_source = (domSource*)elem.cast();
				}

				if (strcmp(COMMON_PROFILE_INPUT_NORMAL,
						   v_inp[k]->getSemantic()) == 0)
				{
					norm_offset = inputs[j]->getOffset();

					const domURIFragmentType& uri = v_inp[k]->getSource();
					daeElementRef elem = uri.getElement();
					norm_source = (domSource*)elem.cast();
				}
			}
		}

		if (strcmp(COMMON_PROFILE_INPUT_NORMAL, inputs[j]->getSemantic()) == 0)
		{
			// Found normal array for this triangle list
			norm_offset = inputs[j]->getOffset();
			const domURIFragmentType& uri = inputs[j]->getSource();
			daeElementRef elem = uri.getElement();
			norm_source = (domSource*)elem.cast();
		}
		else if (strcmp(COMMON_PROFILE_INPUT_TEXCOORD,
						inputs[j]->getSemantic()) == 0)
		{
			// Found texCoords
			tc_offset = inputs[j]->getOffset();
			const domURIFragmentType& uri = inputs[j]->getSource();
			daeElementRef elem = uri.getElement();
			tc_source = (domSource*)elem.cast();
		}
	}

	++idx_stride;

	return true;
}

LLModel::EModelStatus load_face_from_dom_tris(std::vector<LLVolumeFace>& face_list,
											  std::vector<std::string>& materials,
											  domTrianglesRef& tri,
											  LLSD& log_msg)
{
	LLVolumeFace face;
	std::vector<LLVolumeFace::VertexData> verts;
	std::vector<U16> indices;

	const domInputLocalOffset_Array& inputs = tri->getInput_array();

	S32 pos_offset = -1;
	S32 tc_offset = -1;
	S32 norm_offset = -1;

	domSource* pos_source = NULL;
	domSource* tc_source = NULL;
	domSource* norm_source = NULL;

	S32 idx_stride = 0;

	if (!get_dom_sources(inputs, pos_offset, tc_offset, norm_offset,
						 idx_stride, pos_source, tc_source, norm_source))
	{
		llwarns << "Could not find dom sources for basic geometry data. Invalid model."
				<< llendl;
		LLSD args;
		args["Message"] = "ParsingErrorBadElement";
		log_msg.append(args);
		return LLModel::BAD_ELEMENT;
	}

	if (!pos_source || !pos_source->getFloat_array())
	{
		llwarns << "Unable to process mesh without position data. Invalid model."
				<< llendl;
		LLSD args;
		args["Message"] = "ParsingErrorPositionInvalidModel";
		log_msg.append(args);
		return LLModel::BAD_ELEMENT;
	}

	domPRef p = tri->getP();
	domListOfUInts& idx = p->getValue();

	domListOfFloats dummy;
	domListOfFloats& v = pos_source && pos_source->getFloat_array() ?
		pos_source->getFloat_array()->getValue() : dummy;
	domListOfFloats& tc = tc_source && tc_source->getFloat_array() ?
		tc_source->getFloat_array()->getValue() : dummy;
	domListOfFloats& n = norm_source && norm_source->getFloat_array() ?
		norm_source->getFloat_array()->getValue() : dummy;

	U32 index_count = idx.getCount();
	U32 vertex_count = v.getCount();
	U32 tc_count = tc.getCount();
	U32 norm_count = n.getCount();

	if (pos_source)
	{
		if (vertex_count == 0)
		{
			llwarns << "Unable to process mesh with empty position array. Invalid model."
					<< llendl;
			return LLModel::BAD_ELEMENT;
		}

		face.mExtents[0].set(v[0], v[1], v[2]);
		face.mExtents[1].set(v[0], v[1], v[2]);
	}

	LLVolumeFace::VertexMapData::PointMap point_map;

	for (U32 i = 0; i < index_count; i += idx_stride)
	{
		LLVolumeFace::VertexData cv;
		if (pos_source)
		{
			if (i + pos_offset >= index_count)
			{
				return LLModel::BAD_ELEMENT;
			}
			U32 index = 3 * idx[i + pos_offset];
			if (index + 2 >= vertex_count)
			{
				llwarns << "Out of range index data. Invalid model." << llendl;
				return LLModel::BAD_ELEMENT;
			}
			cv.setPosition(LLVector4a(v[index], v[index + 1], v[index + 2]));
			if (!cv.getPosition().isFinite3())
			{
				llwarns << "Found NaN while loading position coords from DAE model. Invalid model."
						<< llendl;
				return LLModel::BAD_ELEMENT;
			}
		}

		if (tc_source)
		{
			if (i + tc_offset >= index_count)
			{
				return LLModel::BAD_ELEMENT;
			}
			U32 index = 2 * idx[i + tc_offset];
			if (index + 1 >= tc_count)
			{
				llwarns << "Out of range tex coords indices. Invalid model." << llendl;
				return LLModel::BAD_ELEMENT;
			}
			cv.mTexCoord.set(tc[index], tc[index + 1]);
			if (!cv.mTexCoord.isFinite())
			{
				llwarns << "Found NaN while loading tex coords from DAE model. Invalid model."
						<< llendl;
				return LLModel::BAD_ELEMENT;
			}
		}

		if (norm_source)
		{
			if (i + norm_offset >= index_count)
			{
				return LLModel::BAD_ELEMENT;
			}
			U32 index = 3 * idx[i + norm_offset];
			if (index + 2 >= norm_count)
			{
				llwarns << "Out of range normals indices. Invalid model." << llendl;
				return LLModel::BAD_ELEMENT;
			}
			cv.setNormal(LLVector4a(n[index], n[index + 1], n[index + 2]));
			if (!cv.getNormal().isFinite3())
			{
				llwarns << "Found NaN while loading normals from DAE model. Invalid model."
						<< llendl;
				return LLModel::BAD_ELEMENT;
			}
		}

		bool found = false;

		LLVolumeFace::VertexMapData::PointMap::iterator point_iter;
		point_iter = point_map.find(LLVector3(cv.getPosition().getF32ptr()));
		if (point_iter != point_map.end())
		{
			for (U32 j = 0, cnt = point_iter->second.size(); j < cnt; ++j)
			{
				// We have a matching loc
				if ((point_iter->second)[j] == cv)
				{
					U16 shared_index = (point_iter->second)[j].mIndex;

					// Do not share verts within the same tri, degenerate
					U32 indx_size = indices.size();
					U32 verts_new_tri = indx_size % 3;
					if ((verts_new_tri < 1 ||
						 indices[indx_size - 1] != shared_index) &&
						(verts_new_tri < 2 ||
						 indices[indx_size - 2] != shared_index))
					{
						found = true;
						indices.push_back(shared_index);
					}
					break;
				}
			}
		}

		if (!found)
		{
			update_min_max(face.mExtents[0], face.mExtents[1], cv.getPosition());
			verts.emplace_back(cv);
			if (verts.size() >= 65535)
			{
				llwarns << "Attempted to write model exceeding 16-bit index buffer limitation."
						<< llendl;
				return LLModel::VERTEX_NUMBER_OVERFLOW;
			}
			U16 index = (U16)(verts.size() - 1);
			indices.push_back(index);

			LLVolumeFace::VertexMapData d;
			d.setPosition(cv.getPosition());
			d.mTexCoord = cv.mTexCoord;
			d.setNormal(cv.getNormal());
			d.mIndex = index;
			if (point_iter != point_map.end())
			{
				point_iter->second.emplace_back(d);
			}
			else
			{
				point_map[LLVector3(d.getPosition().getF32ptr())].emplace_back(d);
			}
		}

		if (indices.size() % 3 == 0 && verts.size() >= 65532)
		{
			std::string material;
			if (tri->getMaterial())
			{
				material = std::string(tri->getMaterial());
			}
			materials.emplace_back(material);

			face_list.push_back(face);

			face_list.rbegin()->fillFromLegacyData(verts, indices);
			LLVolumeFace& new_face = *face_list.rbegin();
			if (!norm_source)
			{
				// NOTE: normals are part of the same buffer as mPositions, do
				// not free them separately.
				new_face.mNormals = NULL;
			}

			if (!tc_source)
			{
				// NOTE: texture coordinates are part of the same buffer as
				// mPositions, do not free them separately.
				new_face.mTexCoords = NULL;
			}

			face = LLVolumeFace();
			face.mExtents[0].set(v[0], v[1], v[2]);
			face.mExtents[1].set(v[0], v[1], v[2]);
			verts.clear();
			indices.clear();
			point_map.clear();
		}
	}

	if (!verts.empty())
	{
		std::string material;
		if (tri->getMaterial())
		{
			material = std::string(tri->getMaterial());
		}

		materials.emplace_back(material);

		face_list.push_back(face);

		face_list.rbegin()->fillFromLegacyData(verts, indices);
		LLVolumeFace& new_face = *face_list.rbegin();
		if (!norm_source)
		{
			// NOTE: normals are part of the same buffer as mPositions, do not
			// free them separately.
			new_face.mNormals = NULL;
		}

		if (!tc_source)
		{
			// NOTE: texture coordinates are part of the same buffer as
			// mPositions, do not free them separately.
			new_face.mTexCoords = NULL;
		}
	}

	return LLModel::NO_ERRORS;
}

LLModel::EModelStatus load_face_from_dom_polylist(std::vector<LLVolumeFace>& face_list,
												  std::vector<std::string>& materials,
												  domPolylistRef& poly,
												  LLSD& log_msg)
{
	domPRef p = poly->getP();
	domListOfUInts& idx = p->getValue();

	if (idx.getCount() == 0)
	{
		return LLModel::NO_ERRORS;
	}

	const domInputLocalOffset_Array& inputs = poly->getInput_array();

	domListOfUInts& vcount = poly->getVcount()->getValue();

	S32 pos_offset = -1;
	S32 tc_offset = -1;
	S32 norm_offset = -1;
	domSource* pos_source = NULL;
	domSource* tc_source = NULL;
	domSource* norm_source = NULL;
	S32 idx_stride = 0;
	if (!get_dom_sources(inputs, pos_offset, tc_offset, norm_offset,
						 idx_stride, pos_source, tc_source,
						 norm_source))
	{
		llwarns << "Could not get DOM sources for basic geometry data. Invalid model."
				<< llendl;
		LLSD args;
		args["Message"] = "ParsingErrorBadElement";
		log_msg.append(args);
		return LLModel::BAD_ELEMENT;
	}

	LLVolumeFace face;

	std::vector<U16> indices;
	std::vector<LLVolumeFace::VertexData> verts;

	domListOfFloats v;
	domListOfFloats tc;
	domListOfFloats n;

	U32 index_count = idx.getCount();
	U32 vertex_count = 0;
	U32 tc_count = 0;
	U32 norm_count = 0;

	if (pos_source)
	{
		v = pos_source->getFloat_array()->getValue();
		face.mExtents[0].set(v[0], v[1], v[2]);
		face.mExtents[1].set(v[0], v[1], v[2]);
		vertex_count = v.getCount();
	}

	if (tc_source)
	{
		tc = tc_source->getFloat_array()->getValue();
		tc_count = tc.getCount();
	}

	if (norm_source)
	{
		n = norm_source->getFloat_array()->getValue();
		norm_count = n.getCount();
	}

	LLVolumeFace::VertexMapData::PointMap point_map;

	U32 cur_idx = 0;
	bool log_tc_msg = true;
	for (U32 i = 0; i < vcount.getCount(); ++i)
	{
		// For each polygon
		U32 first_index = 0;
		U32 last_index = 0;
		for (U32 j = 0; j < vcount[i]; ++j)
		{
			// For each vertex
			LLVolumeFace::VertexData cv;

			if (pos_source)
			{
				if (cur_idx + pos_offset >= index_count)
				{
					llwarns << "Out of range position indices. Invalid model."
							<< llendl;
					return LLModel::BAD_ELEMENT;
				}
				U32 index = 3 * idx[cur_idx + pos_offset];
				if (index + 2 >= vertex_count)
				{
					llwarns << "Out of range position indices. Invalid model."
							<< llendl;
					return LLModel::BAD_ELEMENT;
				}
				cv.getPosition().set(v[index], v[index + 1], v[index + 2]);
				if (!cv.getPosition().isFinite3())
				{
					llwarns << "Found NaN while loading positions from DAE model. Invalid model."
							<< llendl;
					LLSD args;
					args["Message"] = "PositionNaN";
					log_msg.append(args);
					return LLModel::BAD_ELEMENT;
				}
			}

			if (tc_source)
			{
				if (cur_idx + tc_offset >= index_count)
				{
					llwarns << "Out of range text coords indices. Invalid model."
							<< llendl;
					return LLModel::BAD_ELEMENT;
				}
				U32 index = 2 * idx[cur_idx + tc_offset];
				if (index + 1 < tc_count)
				{
					cv.mTexCoord.set(tc[index], tc[index + 1]);
					if (!cv.mTexCoord.isFinite())
					{
						llwarns << "Found NaN while loading texture coordinates from DAE model. Invalid model."
								<< llendl;
						return LLModel::BAD_ELEMENT;
					}
				}
				else if (log_tc_msg)
				{
					log_tc_msg = false;
					llwarns << "Texture coordinates data is not complete."
							<< llendl;
					LLSD args;
					args["Message"] = "IncompleteTC";
					log_msg.append(args);
				}
			}

			if (norm_source)
			{
				if (cur_idx + norm_offset >= index_count)
				{
					llwarns << "Out of range normals indices. Invalid model."
							<< llendl;
					return LLModel::BAD_ELEMENT;
				}
				U32 index = 3 * idx[cur_idx + norm_offset];
				if (index + 2 >= norm_count)
				{
					llwarns << "Out of range normals indices. Invalid model."
							<< llendl;
					return LLModel::BAD_ELEMENT;
				}
				cv.getNormal().set(n[index], n[index + 1], n[index + 2]);
				if (!cv.getNormal().isFinite3())
				{
					llwarns << "Found NaN while loading normals from DAE model. Invalid model."
							<< llendl;
					LLSD args;
					args["Message"] = "NormalsNaN";
					log_msg.append(args);
					return LLModel::BAD_ELEMENT;
				}
			}

			cur_idx += idx_stride;

			bool found = false;

			LLVolumeFace::VertexMapData::PointMap::iterator point_iter;
			LLVector3 pos3(cv.getPosition().getF32ptr());
			point_iter = point_map.find(pos3);

			if (point_iter != point_map.end())
			{
				for (U32 k = 0; k < point_iter->second.size(); ++k)
				{
					if ((point_iter->second)[k] == cv)
					{
						found = true;
						U32 index = (point_iter->second)[k].mIndex;
						if (j == 0)
						{
							first_index = index;
						}
						else if (j == 1)
						{
							last_index = index;
						}
						else
						{
							indices.push_back(first_index);
							indices.push_back(last_index);
							indices.push_back(index);
							last_index = index;
						}

						break;
					}
				}
			}

			if (!found)
			{
				update_min_max(face.mExtents[0], face.mExtents[1],
							   cv.getPosition());
				verts.emplace_back(cv);
				if (verts.size() >= 65535)
				{
					llwarns << "Attempted to write model exceeding 16-bit index buffer limitation."
							<< llendl;
					return LLModel::VERTEX_NUMBER_OVERFLOW ;
				}
				U16 index = (U16)(verts.size() - 1);

				if (j == 0)
				{
					first_index = index;
				}
				else if (j == 1)
				{
					last_index = index;
				}
				else
				{
					indices.push_back(first_index);
					indices.push_back(last_index);
					indices.push_back(index);
					last_index = index;
				}

				LLVolumeFace::VertexMapData d;
				d.setPosition(cv.getPosition());
				d.mTexCoord = cv.mTexCoord;
				d.setNormal(cv.getNormal());
				d.mIndex = index;
				if (point_iter != point_map.end())
				{
					point_iter->second.emplace_back(d);
				}
				else
				{
					point_map[pos3].emplace_back(d);
				}
			}

			if (indices.size() % 3 == 0 && indices.size() >= 65532)
			{
				std::string material;
				if (poly->getMaterial())
				{
					material = std::string(poly->getMaterial());
				}
				materials.emplace_back(material);

				face_list.push_back(face);
				face_list.rbegin()->fillFromLegacyData(verts, indices);

				LLVolumeFace& new_face = *face_list.rbegin();

				if (!norm_source)
				{
					// NOTE: normals are part of the same buffer as mPositions,
					// do not free them separately.
					new_face.mNormals = NULL;
				}

				if (!tc_source)
				{
					// NOTE: texture coordinates are part of the same buffer as
					// mPositions, do not free them separately.
					new_face.mTexCoords = NULL;
				}

				face = LLVolumeFace();
				face.mExtents[0].set(v[0], v[1], v[2]);
				face.mExtents[1].set(v[0], v[1], v[2]);
				verts.clear();
				indices.clear();
				point_map.clear();
			}
		}
	}

	if (!verts.empty())
	{
		std::string material;
		if (poly->getMaterial())
		{
			material = std::string(poly->getMaterial());
		}
		materials.emplace_back(material);

		face_list.push_back(face);
		face_list.rbegin()->fillFromLegacyData(verts, indices);

		LLVolumeFace& new_face = *face_list.rbegin();

		if (!norm_source)
		{
			// NOTE: normals are part of the same buffer as mPositions, do not
			// free them separately.
			new_face.mNormals = NULL;
		}

		if (!tc_source)
		{
			// NOTE: texture coordinates are part of the same buffer as
			// mPositions, do not free them separately.
			new_face.mTexCoords = NULL;
		}
	}

	return LLModel::NO_ERRORS;
}

LLModel::EModelStatus load_face_from_dom_polygons(std::vector<LLVolumeFace>& face_list,
												  std::vector<std::string>& materials,
												  domPolygonsRef& poly)
{
	LLVolumeFace face;
	std::vector<U16> indices;
	std::vector<LLVolumeFace::VertexData> verts;

	const domInputLocalOffset_Array& inputs = poly->getInput_array();

	S32 v_offset = -1;
	S32 n_offset = -1;
	S32 t_offset = -1;

	domListOfFloats* v = NULL;
	domListOfFloats* n = NULL;
	domListOfFloats* t = NULL;

	U32 stride = 0;
	for (U32 i = 0; i < inputs.getCount(); ++i)
	{
		stride = llmax((U32)inputs[i]->getOffset() + 1, stride);

		if (strcmp(COMMON_PROFILE_INPUT_VERTEX, inputs[i]->getSemantic()) == 0)
		{
			// Found vertex array
			v_offset = inputs[i]->getOffset();

			const domURIFragmentType& uri = inputs[i]->getSource();
			daeElementRef elem = uri.getElement();
			domVertices* vertices = (domVertices*)elem.cast();
			if (!vertices)
			{
				llwarns << "Could not find vertex source. Invalid model."
						<< llendl;
				return LLModel::BAD_ELEMENT;
			}
			domInputLocal_Array& v_inp = vertices->getInput_array();

			for (U32 k = 0; k < v_inp.getCount(); ++k)
			{
				if (strcmp(COMMON_PROFILE_INPUT_POSITION,
						   v_inp[k]->getSemantic()) == 0)
				{
					const domURIFragmentType& uri = v_inp[k]->getSource();
					daeElementRef elem = uri.getElement();
					domSource* src = (domSource*)elem.cast();
					if (!src)
					{
						llwarns << "Could not find DOM source. Invalid model."
								<< llendl;
						return LLModel::BAD_ELEMENT;
					}
					v = &(src->getFloat_array()->getValue());
				}
			}
		}
		else if (strcmp(COMMON_PROFILE_INPUT_NORMAL,
						inputs[i]->getSemantic()) == 0)
		{
			n_offset = inputs[i]->getOffset();
			// Found normal array for this triangle list
			const domURIFragmentType& uri = inputs[i]->getSource();
			daeElementRef elem = uri.getElement();
			domSource* src = (domSource*)elem.cast();
			if (!src)
			{
				llwarns << "Could not find DOM source. Invalid model."
						<< llendl;
				return LLModel::BAD_ELEMENT;
			}
			n = &(src->getFloat_array()->getValue());
		}
		else if (strcmp(COMMON_PROFILE_INPUT_TEXCOORD,
						inputs[i]->getSemantic()) == 0 &&
				 inputs[i]->getSet() == 0)
		{
			// Found texCoords
			t_offset = inputs[i]->getOffset();
			const domURIFragmentType& uri = inputs[i]->getSource();
			daeElementRef elem = uri.getElement();
			domSource* src = (domSource*)elem.cast();
			if (!src)
			{
				llwarns << "Could not find DOM source. Invalid model."
						<< llendl;
				return LLModel::BAD_ELEMENT;
			}
			t = &(src->getFloat_array()->getValue());
		}
	}

	domP_Array& ps = poly->getP_array();

	// Make a triangle list in <verts>
	for (U32 i = 0; i < ps.getCount(); ++i)
	{
		// For each polygon
		domListOfUInts& idx = ps[i]->getValue();
		for (U32 j = 0; j < idx.getCount() / stride; ++j)
		{
			// For each vertex
			if (j > 2)
			{
				U32 size = verts.size();
				verts.emplace_back(verts[size - 3]);
				verts.emplace_back(verts[size - 1]);
			}

			LLVolumeFace::VertexData vert;

			if (v)
			{
				U32 v_idx = idx[j * stride + v_offset] * 3;
				v_idx = llclamp(v_idx, 0U, (U32)v->getCount());
				vert.getPosition().set(v->get(v_idx), v->get(v_idx + 1),
									   v->get(v_idx + 2));
				if (!vert.getPosition().isFinite3())
				{
					llwarns << "Found NaN while loading position data from DAE model. Invalid model."
							<< llendl;
					return LLModel::BAD_ELEMENT;
				}
			}

			// Bound-check n and t lookups because some FBX to DAE converters
			// use negative indices and empty arrays to indicate data does not
			// exist for a particular channel
			if (n && n->getCount() > 0)
			{
				U32 n_idx = idx[j * stride + n_offset] * 3;
				n_idx = llclamp(n_idx, 0U, (U32)n->getCount());
				vert.getNormal().set(n->get(n_idx), n->get(n_idx + 1),
									 n->get(n_idx + 2));
				if (!vert.getNormal().isFinite3())
				{
					llwarns << "Found NaN while loading normals from DAE model. Invalid model."
							<< llendl;
					return LLModel::BAD_ELEMENT;
				}
			}
			else
			{
				vert.getNormal().clear();
			}

			if (t && t->getCount() > 0)
			{
				U32 t_idx = idx[j * stride + t_offset] * 2;
				t_idx = llclamp(t_idx, 0U, (U32)t->getCount());
				vert.mTexCoord.set(t->get(t_idx), t->get(t_idx + 1));
				if (!vert.mTexCoord.isFinite())
				{
					llwarns << "Found NaN while loading tex coords from DAE model. Invalid model."
							<< llendl;
					return LLModel::BAD_ELEMENT;
				}
			}
			else
			{
				vert.mTexCoord.clear();
			}

			verts.emplace_back(vert);
		}
	}

	if (verts.empty())
	{
		return LLModel::NO_ERRORS;
	}

	face.mExtents[0] = verts[0].getPosition();
	face.mExtents[1] = verts[0].getPosition();

	// Create a map of unique vertices to indices
	std::map<LLVolumeFace::VertexData, U32> vert_idx;

	U32 cur_idx = 0;
	for (U32 i = 0; i < verts.size(); ++i)
	{
		std::map<LLVolumeFace::VertexData, U32>::iterator iter =
			vert_idx.find(verts[i]);
		if (iter == vert_idx.end())
		{
			vert_idx[verts[i]] = cur_idx++;
		}
	}

	// Build vertex array from map
	std::vector<LLVolumeFace::VertexData> new_verts;
	size_t vert_count = vert_idx.size();
	if (vert_count >= (size_t)U16_MAX)
	{
		llwarns << "Too many vertices: " << vert_count << "- Max is: "
				<< U16_MAX << llendl;
		llassert(false);
	}
	new_verts.resize(vert_count);

	for (std::map<LLVolumeFace::VertexData, U32>::iterator iter = vert_idx.begin();
		 iter != vert_idx.end(); ++iter)
	{
		new_verts[iter->second] = iter->first;
		update_min_max(face.mExtents[0], face.mExtents[1],
					   iter->first.getPosition());
	}

	// Build index array from map
	indices.resize(verts.size());

	for (U32 i = 0; i < verts.size(); ++i)
	{
		indices[i] = vert_idx[verts[i]];
		// Assume GL_TRIANGLES: compare 0-1, 1-2, 3-4, 4-5 but not 2-3 or 5-6
		if (i % 3 != 0 && indices[i - 1] != indices[i])
		{
			llwarns << "Detected degenerate triangle at index: " << i
					<< llendl;
		}
	}

#if 0	// DEBUG just build an expanded triangle list
	for (U32 i = 0; i < verts.size(); ++i)
	{
		indices.push_back((U16)i);
		update_min_max(face.mExtents[0], face.mExtents[1],
					   verts[i].getPosition());
	}
#endif

    if (!new_verts.empty())
	{
		std::string material;
		if (poly->getMaterial())
		{
			material = std::string(poly->getMaterial());
		}
		materials.emplace_back(material);

		face_list.push_back(face);
		face_list.rbegin()->fillFromLegacyData(new_verts, indices);

		LLVolumeFace& new_face = *face_list.rbegin();

		if (!n)
		{
			// NOTE: normals are part of the same buffer as mPositions, do not
			// free them separately.
			new_face.mNormals = NULL;
		}

		if (!t)
		{
			// NOTE: texture coordinates are part of the same buffer as
			// mPositions, do not free them separately.
			new_face.mTexCoords = NULL;
		}
	}

	return LLModel::NO_ERRORS;
}

//-----------------------------------------------------------------------------
// LLDAELoader
//-----------------------------------------------------------------------------
LLDAELoader::LLDAELoader(const std::string& filename, S32 lod,
						 load_callback_t load_cb,
						 joint_lookup_func_t joint_lookup_func,
						 texture_load_func_t texture_load_func,
						 state_callback_t state_cb, void* userdata,
						 JointTransformMap& joint_transform_map,
						 JointNameSet& joints_from_nodes,
						 std::map<std::string, std::string>& joint_alias_map,
						 U32 max_joints_per_mesh, U32 model_limit,
						 bool preprocess)
:	LLModelLoader(filename, lod, load_cb, joint_lookup_func, texture_load_func,
				  state_cb, userdata, joint_transform_map, joints_from_nodes,
				  joint_alias_map, max_joints_per_mesh),
	mGeneratedModelLimit(model_limit),
	mPreprocessDAE(preprocess)
{
}

struct ModelSort
{
	bool operator()(const LLPointer<LLModel>& lhs,
					const LLPointer<LLModel>& rhs)
	{
        if (lhs->mSubmodelID < rhs->mSubmodelID)
        {
            return true;
        }
		return LLStringUtil::compareInsensitive(lhs->mLabel, rhs->mLabel) < 0;
	}
};

bool LLDAELoader::openFile(const std::string& filename)
{
	setLoadState(READING_FILE);

	// Setup a DAE error handler
	LLSetDaeErrorHandler dae_error_handler;

	const std::string allowed =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789%-._~:\"|\\/";
	std::string uri_filename = LLURI::escape(filename, allowed);

	// No suitable slm exists, load from the .dae file
	DAE dae;
	domCOLLADA* dom;
	if (mPreprocessDAE)
	{
		dom = dae.openFromMemory(uri_filename,
								 preprocessDAE(filename).c_str());
	}
	else
	{
		llinfos << "Skipping pre-processing of DAE file: " << filename
				<< llendl;
		dom = dae.open(uri_filename);
	}
	if (!dom)
	{
		llwarns <<" Error with dae; traditionally indicates a corrupt file."
				<< llendl;
		LLSD args;
		args["Message"] = "ParsingErrorCorrupt";
		mWarningsArray.append(args);
		setLoadState(ERROR_PARSING);
		return false;
	}

	// Dom version
	daeString dom_version = dae.getDomVersion();
	std::string sldom(dom_version);
	llinfos << "Collada importer version: " << sldom << llendl;

	// Dae version
	domVersionType doc_version = dom->getVersion();
	// 0 = v1.4, 1 = v1.4.1, 2 = currently unsupported, however may work
	if (doc_version > 1)
	{
		doc_version = VERSIONTYPE_COUNT;
	}
	llinfos << "Dae version: " << colladaVersion[doc_version] << llendl;

	daeDatabase* db = dae.getDatabase();
	if (!db)
	{
		llwarns << "NULL database !  Aborted." << llendl;
		return false;
	}

	daeInt count = db->getElementCount(NULL, COLLADA_TYPE_MESH);

	daeDocument* doc = dae.getDoc(uri_filename);
	if (!doc)
	{
		LLSD args;
		args["Message"] = "ParsingErrorNoDoc";
		mWarningsArray.append(args);
		llwarns << "Cannot find internal DAE doc" << llendl;
		return false;
	}

	daeElement* root = doc->getDomRoot();
	if (!root)
	{
		llwarns << "Document has no root" << llendl;
		return false;
	}

	// Verify some basic properties of the dae
	// 1. Basic validity check on controller
	U32 controller_count = (U32)db->getElementCount(NULL, "controller");
	bool result = false;
	for (U32 i = 0; i < controller_count; ++i)
	{
		domController* controllerp = NULL;
		db->getElement((daeElement**)&controllerp, i, NULL, "controller");
		result = verifyController(controllerp);
		if (!result)
		{
			LLSD args;
			args["Message"] = "ParsingErrorBadElement";
			mWarningsArray.append(args);
			llinfos << "Could not verify controller" << llendl;
			setLoadState(ERROR_PARSING);
			return true;
		}
	}

	// Get unit scale
	mTransform.setIdentity();

	domAsset::domUnit* unit =
		daeSafeCast<domAsset::domUnit>(root->getDescendant(daeElement::matchType(domAsset::domUnit::ID())));
	if (unit)
	{
		F32 meter = unit->getMeter();
		mTransform.mMatrix[0][0] = meter;
		mTransform.mMatrix[1][1] = meter;
		mTransform.mMatrix[2][2] = meter;
	}

	// Get up axis rotation
	LLMatrix4 rotation;

	domUpAxisType up = UPAXISTYPE_Y_UP;  // Default is Y_UP
	domAsset::domUp_axis* up_axis =
		daeSafeCast<domAsset::domUp_axis>(root->getDescendant(daeElement::matchType(domAsset::domUp_axis::ID())));
	if (up_axis)
	{
		up = up_axis->getValue();
	}
	if (up == UPAXISTYPE_X_UP)
	{
		rotation.initRotation(0.f, 90.f * DEG_TO_RAD, 0.f);
	}
	else if (up == UPAXISTYPE_Y_UP)
	{
		rotation.initRotation(90.f * DEG_TO_RAD, 0.f, 0.f);
	}

	rotation *= mTransform;
	mTransform = rotation;
	mTransform.condition();

	U32 submodel_limit = count > 0 ? mGeneratedModelLimit / count : 0;
	for (daeInt idx = 0; idx < count; ++idx)
	{
		// Build map of domEntities to LLModel
		domMesh* mesh = NULL;
		db->getElement((daeElement**)&mesh, idx, NULL, COLLADA_TYPE_MESH);
		if (mesh)
		{
			std::vector<LLModel*> models;
			loadModelsFromDomMesh(mesh, models, submodel_limit);

			std::vector<LLModel*>::iterator i;
			i = models.begin();
			while (i != models.end())
			{
				LLModel* mdl = *i;
				if (mdl->getStatus() != LLModel::NO_ERRORS)
				{
					setLoadState(ERROR_MODEL + mdl->getStatus());
					return false;
				}

				if (mdl && mdl->validate(true))
				{
					mModelList.push_back(mdl);
					mModelsMap[mesh].push_back(mdl);
				}
				++i;
			}
		}
	}

	std::sort(mModelList.begin(), mModelList.end(), ModelSort());

#if LL_NORMALIZE_ALL_MODELS
	if (!mNoNormalize)
	{
		LLModel::normalizeModels(mModelList);
	}
#endif

	model_list::iterator model_iter = mModelList.begin();
	std::vector<std::string>::iterator mat_iter, end_iter;
	while (model_iter != mModelList.end())
	{
		LLModel* mdl = *model_iter;
		if (!mdl) continue;	// Paranoia

		U32 material_count = mdl->mMaterialList.size();
		llinfos << "Importing " << mdl->mLabel << " model with "
				<< material_count << " material references" << llendl;

		mat_iter = mdl->mMaterialList.begin();
		end_iter = material_count > LIMIT_MATERIALS_OUTPUT ?
				mat_iter + LIMIT_MATERIALS_OUTPUT :
				mdl->mMaterialList.end();
		while (mat_iter != end_iter)
		{
			llinfos << " - " << mdl->mLabel << " references " << (*mat_iter)
					<< llendl;
			++mat_iter;
		}
		++model_iter;
	}

	domGeometry* geom;
	count = db->getElementCount(NULL, COLLADA_TYPE_SKIN);
	for (daeInt idx = 0; idx < count; ++idx)
	{
		// Add skinned meshes as instances
		domSkin* skin = NULL;
		db->getElement((daeElement**)&skin, idx, NULL, COLLADA_TYPE_SKIN);
		if (skin)
		{
			geom = daeSafeCast<domGeometry>(skin->getSource().getElement());
			if (geom)
			{
				domMesh* mesh = geom->getMesh();
				if (mesh)
				{
					std::vector<LLPointer<LLModel> >::iterator it;
					it = mModelsMap[mesh].begin();
					while (it != mModelsMap[mesh].end())
					{
						LLPointer<LLModel> mdl = *it;
						LLDAELoader::processDomModel(mdl, &dae, root, mesh,
													 skin);
						++it;
					}
				}
			}
		}
	}

	llinfos << "Collada skins processed: " << count << llendl;

	daeElement* scene = root->getDescendant("visual_scene");
	if (!scene)
	{
		llwarns << "Document has no visual_scene" << llendl;
		LLSD args;
		args["Message"] = "ParsingErrorNoScene";
		mWarningsArray.append(args);
		setLoadState(ERROR_PARSING);
		return true;
	}

	setLoadState(DONE);

	bool bad_element = false;
	processElement(scene, bad_element, &dae);
	if (bad_element)
	{
		llwarns << "Scene could not be parsed" << llendl;
		LLSD args;
		args["Message"] = "ParsingErrorCantParseScene";
		mWarningsArray.append(args);
		setLoadState(ERROR_PARSING);
	}

	return true;
}

// Open a DAE file for some preprocessing (like removing space characters in
// IDs), see MAINT-5678.
std::string LLDAELoader::preprocessDAE(const std::string& filename)
{
	llinfos << "Preprocessing dae file '" << filename
			<< "' to remove spaces from the names, ids, etc." << llendl;

	llifstream in_file;
	in_file.open(filename.c_str());
	std::stringstream str_stream;
	str_stream << in_file.rdbuf();
	std::string buffer = str_stream.str();

	try
	{
		std::regex re("\"[\\w\\.@#$-]*(\\s[\\w\\.@#$-]*)+\"");
		std::sregex_iterator next(buffer.begin(), buffer.end(), re);
		std::sregex_iterator end;
		while (next != end)
		{
			std::smatch match = *next;
			std::string s = match.str();
			LL_DEBUGS("MeshUpload") << "Found: '" << s << "'" << LL_ENDL;
			LLStringUtil::replaceChar(s, ' ', '_');
			LL_DEBUGS("MeshUpload") << "Replacing with: '" << s << "'"
									<< LL_ENDL;
			LLStringUtil::replaceString(buffer, match.str(), s);
			++next;
		}
	}
	catch (std::regex_error& e)
	{
		llwarns << "Regex error: " << e.what() << llendl;
	}

	return buffer;
}

void LLDAELoader::processDomModel(LLModel* model, DAE* dae, daeElement* root,
								  domMesh* mesh, domSkin* skin)
{
	if (!model || !dae || !root || !model || !skin)
	{
		llwarns << "NULL pointer passed !" << llendl;
		llassert(false);
		return;
	}

	LLVector3 mesh_scale_vector;
	LLVector3 mesh_translation_vector;
	model->getNormalizedScaleTranslation(mesh_scale_vector,
										 mesh_translation_vector);

	LLMatrix4 normalized_transformation;
	normalized_transformation.setTranslation(mesh_translation_vector);

	LLMatrix4 mesh_scale;
	mesh_scale.initScale(mesh_scale_vector);
	mesh_scale *= normalized_transformation;
	normalized_transformation = mesh_scale;

	LLMatrix4a inv_mat;
	inv_mat.loadu(normalized_transformation);
	inv_mat.invert();
	LLMatrix4 inv_norm_trans(inv_mat.getF32ptr());

	domSkin::domBind_shape_matrix* bind_mat = skin->getBind_shape_matrix();
	if (bind_mat)
	{
		// Get bind shape matrix
		domFloat4x4& dom_value = bind_mat->getValue();

		LLMeshSkinInfo& skin_info = model->mSkinInfo;
		for (S32 i = 0; i < 4; ++i)
		{
			for (S32 j = 0; j < 4; ++j)
			{
				skin_info.mBindShapeMatrix.mMatrix[i][j] = dom_value[i + j * 4];
			}
		}

		LLMatrix4 trans = normalized_transformation;
		trans *= skin_info.mBindShapeMatrix;
		skin_info.mBindShapeMatrix = trans;
	}

	// Some collada setup for accessing the skeleton
	U32 skeleton_count = dae->getDatabase()->getElementCount(NULL, "skeleton");
	std::vector<domInstance_controller::domSkeleton*> skeletons;
	for (U32 i = 0; i < skeleton_count; ++i)
	{
		daeElement* elementp = 0;
		dae->getDatabase()->getElement(&elementp, i, 0, "skeleton");

		// Try to get at the skeletal instance controller
		domInstance_controller::domSkeleton* skeletonp =
			daeSafeCast<domInstance_controller::domSkeleton>(elementp);
		if (skeletonp)
		{
			daeElement* skeletonrootnodep = skeletonp->getValue().getElement();
			if (skeletonrootnodep)
			{
				skeletons.push_back(skeletonp);
			}
		}
	}

	bool missing_skel_or_scene = false;
	skeleton_count = skeletons.size();
	if (skeleton_count)
	{
		// Get at least one skeleton
		for (U32 i = 0; i < skeleton_count; ++i)
		{
			domInstance_controller::domSkeleton* skeletonp = skeletons[i];
			if (!skeletonp) continue;	// Paranoia

			daeElement* skeletonrootnodep = skeletonp->getValue().getElement();
			if (!skeletonrootnodep) continue;	// Paranoia

			// Once we have the root node, start acccessing it's joint
			// components
			const size_t joint_count = mJointMap.size();
			JointMap::const_iterator joint_it = mJointMap.begin();

			// Loop over all the possible joints within the .dae using the
			// allowed joint list in the ctor.
			char str[65];
			str[64] = '\0';
			for (size_t i = 0; i < joint_count; ++i, ++joint_it)
			{
				// Build a joint for the resolver to work with
				snprintf(str, 64, "./%s", joint_it->first.c_str());

				// Setup the resolver
				daeSIDResolver resolver(skeletonrootnodep, str);

				// Look for the joint
				domNode* jointp = daeSafeCast<domNode>(resolver.getElement());
				if (jointp)
				{
					// Pull out the translate id and store it in the
					// jointTranslations map
					daeSIDResolver joint_rsv_a(jointp, "./translate");
					domTranslate* trans_a =
						daeSafeCast<domTranslate>(joint_rsv_a.getElement());
					daeSIDResolver joint_rsv_b(jointp, "./location");
					domTranslate* trans_b =
						daeSafeCast<domTranslate>(joint_rsv_b.getElement());

					LLMatrix4 working_transform;

					// Translation via SID
					if (trans_a)
					{
						extractTranslation(trans_a, working_transform);
					}
					else if (trans_b)
					{
						extractTranslation(trans_b, working_transform);
					}
					else
					{
						// Translation via child from element
						daeElement* translate_elemp;
						translate_elemp = getChildFromElement(jointp, "translate");
						if (translate_elemp &&
							translate_elemp->typeID() != domTranslate::ID())
						{
							llwarns << "The found element is not a translate node"
									<< llendl;
							missing_skel_or_scene = true;
						}
						else if (translate_elemp)
						{
							extractTranslationViaElement(translate_elemp,
														 working_transform);
						}
						else
						{
							extractTranslationViaSID(jointp,
													 working_transform);
						}
					}

					// Store the joint transform w/respect to it's name.
					mJointList[joint_it->second.c_str()] = working_transform;
				}
			}

			// If anything failed in regards to extracting the skeleton, joints
			// or translation id, mention it
			if (missing_skel_or_scene)
			{
				llwarns << "Partial jointmap found in asset: did you mean to just have a partial map ?"
						<< llendl;
			}
		}
	}
	else
	{
		// If no skeleton, do a breadth-first search to get at specific joints
		daeElement* scenep = root->getDescendant("visual_scene");
		if (!scenep)
		{
			llwarns << "No visual scene; unable to parse bone offsets."
					<< llendl;
			missing_skel_or_scene = true;
		}
		else
		{
			// Get the children at this level
			daeTArray<daeSmartRef<daeElement> > children = scenep->getChildren();
			// Process any children that are joints; not all children are
			// joints, some could be ambient lights, cameras, geometry etc..
			for (S32 i = 0, child_count = children.getCount();
				 i < child_count; ++i)
			{
				domNode* nodep = daeSafeCast<domNode>(children[i]);
				if (nodep)
				{
					processJointNode(nodep, mJointList);
				}
			}
		}
	}

	domSkin::domJoints* joints = skin->getJoints();
	if (!joints)
	{
		llwarns << "NULL skin joints pointer !  Aborting." << llendl;
		return;
	}

	domInputLocal_Array& joint_input = joints->getInput_array();
	for (size_t i = 0; i < joint_input.getCount(); ++i)
	{
		domInputLocal* input = joint_input.get(i);
		if (!input) continue;	// Paranoia

		xsNMTOKEN semantic = input->getSemantic();
		if (strcmp(semantic, COMMON_PROFILE_INPUT_JOINT) == 0)
		{
			// Found joint source, fill model->mJointMap and
			// model->mSkinInfo.mJointNames
			daeElement* elem = input->getSource().getElement();
			domSource* source = daeSafeCast<domSource>(elem);
			if (source)
			{
				domName_array* names_source = source->getName_array();
				if (names_source)
				{
					domListOfNames& names = names_source->getValue();
					for (size_t j = 0; j < names.getCount(); ++j)
					{
						std::string name(names.get(j));
						if (mJointMap.find(name) != mJointMap.end())
						{
							name = mJointMap[name];
						}
						model->mSkinInfo.mJointNames.emplace_back(name);
						model->mSkinInfo.mJointKeys.push_back(LLJoint::getKey(name));
					}
				}
				else
				{
					domIDREF_array* names_source = source->getIDREF_array();
					if (names_source)
					{
						xsIDREFS& names = names_source->getValue();
						for (size_t j = 0; j < names.getCount(); ++j)
						{
							std::string name(names.get(j).getID());
							if (mJointMap.find(name) != mJointMap.end())
							{
								name = mJointMap[name];
							}
							model->mSkinInfo.mJointNames.emplace_back(name);
							model->mSkinInfo.mJointKeys.push_back(LLJoint::getKey(name));
						}
					}
				}
			}
		}
		else if (strcmp(semantic, COMMON_PROFILE_INPUT_INV_BIND_MATRIX) == 0)
		{
			// Found inv_bind_matrix array, fill model->mInvBindMatrix
			domSource* source =
				daeSafeCast<domSource>(input->getSource().getElement());
			if (source)
			{
				domFloat_array* t = source->getFloat_array();
				if (t)
				{
					domListOfFloats& transform = t->getValue();
					S32 count = transform.getCount() / 16;
					for (S32 k = 0; k < count; ++k)
					{
						LLMatrix4 mat;
						for (S32 i = 0; i < 4; ++i)
						{
							for (S32 j = 0; j < 4; ++j)
							{
								mat.mMatrix[i][j] = transform[i + 4 * j + 16 * k];
							}
						}
						model->mSkinInfo.mInvBindMatrix.emplace_back(mat);
					}
				}
			}
		}
	}

	auto& inv_bind_vec = model->mSkinInfo.mInvBindMatrix;
	size_t mat_size = llmin(inv_bind_vec.size(),
							LL_CHARACTER_MAX_ANIMATED_JOINTS);
	model->mSkinInfo.mInvBindShapeMatrix.resize(mat_size);
	if (mat_size)
	{
		LLMatrix4a bind_shape, inv_bind, mat;
		bind_shape.loadu(model->mSkinInfo.mBindShapeMatrix);
		for (size_t i = 0; i < mat_size; ++i)
		{
			inv_bind.loadu(inv_bind_vec[i]);
			mat.matMul(bind_shape, inv_bind);
			model->mSkinInfo.mInvBindShapeMatrix[i].set(mat.getF32ptr());
		}
	}


	// Now that we have parsed the joint array, let's determine if we have a
	// full rig (which means we have all the joint sthat are required for an
	// avatar versus a skinned asset attached to a node in a file that contains
	// an entire skeleton, but does not use the skeleton).
	buildJointToNodeMappingFromScene(root);
	critiqueRigForUploadApplicability(model->mSkinInfo.mJointNames);

	if (!missing_skel_or_scene)
	{
		// *FIXME: mesh_id is used to determine which mesh gets to set the
		// joint offset, in the event of a conflict. Since we do not know the
		// mesh id yet, we cannot/ guarantee that joint offsets will be applied
		// with the same priority as in the uploaded model. If the file
		// contains multiple meshes with conflicting joint offsets, preview may
		// be incorrect.
		LLUUID fake_mesh_id;
		fake_mesh_id.generate();

		// Set the joint translations on the avatar
		for (JointMap::const_iterator it = mJointMap.begin(),
									  end = mJointMap.end();
			 it != end; ++it)
		{
			const std::string& joint_name = it->first;
			if (mJointList.find(joint_name) == mJointList.end())
			{
				continue;
			}

			LLJoint* jointp = mJointLookupFunc(joint_name, mUserData);
			if (!jointp)
			{
				// Most likely an error in the asset.
				llwarns << "Tried to apply joint position from .dae for joint "
						<< joint_name << ", but it did not exist in the avatar rig."
						<< llendl;
				continue;
			}

			LLMatrix4 joint_tf = mJointList[joint_name];
			const LLVector3& joint_pos = joint_tf.getTranslation();
			if (jointp->aboveJointPosThreshold(joint_pos))
			{
				jointp->addAttachmentPosOverride(joint_pos, fake_mesh_id, "");
				if (model->mSkinInfo.mLockScaleIfJointPosition)
				{
					jointp->addAttachmentScaleOverride(jointp->getDefaultScale(),
													   fake_mesh_id, "");
				}
			}
		}
	}

	// We need to construct the alternate bind matrix (which contains the new
	// joint positions) in the same order as they were stored in the joint
	// buffer. The joints associated with the skeleton are not stored in the
	// same order as they are in the exported joint buffer. This remaps the
	// skeletal joints to be in the same order as the joints stored in the
	// model.
	std::vector<std::string>::const_iterator joint_it =
		model->mSkinInfo.mJointNames.begin();
	const size_t joint_count = model->mSkinInfo.mJointNames.size();
	const size_t inv_mat_size = model->mSkinInfo.mInvBindMatrix.size();
	if (inv_mat_size < joint_count)
	{
		llwarns << "Joint count (" << joint_count
				<< ") is greater than in bing matrix size (" << inv_mat_size
				<< "): some joint will not have an alternate bind matrix "
				<< llendl;
	}
	for (size_t i = 0, count = llmin(joint_count, inv_mat_size); i < count;
		 ++i, ++joint_it)
	{
		const std::string& joint_name = *joint_it;
		if (mJointMap.find(joint_name) == mJointMap.end())
		{
			LL_DEBUGS("MeshUpload") << "Possibly misnamed/missing joint: "
									<< joint_name << LL_ENDL;
			continue;
		}
		// Look for the joint xform that we extracted from the skeleton, using
		// the joint_it as the key and store it in the alternate bind matrix
		LLMatrix4 new_inverse = model->mSkinInfo.mInvBindMatrix[i];
		new_inverse.setTranslation(mJointList[joint_name].getTranslation());
		model->mSkinInfo.mAlternateBindMatrix.emplace_back(new_inverse);
	}

	U32 bind_count = model->mSkinInfo.mAlternateBindMatrix.size();
	if (bind_count > 0 && bind_count != joint_count)
	{
		llwarns << "Model " << model->mLabel
				<< " has invalid joint bind matrix list." << llendl;
	}

	// Grab raw position array
	domVertices* verts = mesh->getVertices();
	if (verts)
	{
		domInputLocal_Array& inputs = verts->getInput_array();
		for (size_t i = 0; i < inputs.getCount() && model->mPosition.empty();
			 ++i)
		{
			if (strcmp(inputs[i]->getSemantic(),
					   COMMON_PROFILE_INPUT_POSITION) != 0)
			{
				continue;
			}

			domSource* pos_source =
				daeSafeCast<domSource>(inputs[i]->getSource().getElement());
			if (!pos_source)
			{
				continue;
			}

			domFloat_array* pos_array = pos_source->getFloat_array();
			if (!pos_array)
			{
				continue;
			}

			domListOfFloats& pos = pos_array->getValue();
			for (size_t j = 0; j < pos.getCount(); j += 3)
			{
				if (pos.getCount() <= j + 2)
				{
					llwarns << "Invalid position array size - Skipping"
							<< llendl;
					llassert(false);
					continue;
				}

				LLVector3 v(pos[j], pos[j + 1], pos[j + 2]);
				// Transform from COLLADA space to volume space
				model->mPosition.emplace_back(v * inv_norm_trans);
			}
		}
	}

	// Grab skin weights array
	domSkin::domVertex_weights* weights = skin->getVertex_weights();
	if (weights)
	{
		domInputLocalOffset_Array& inputs = weights->getInput_array();
		domFloat_array* vertex_weights = NULL;
		for (size_t i = 0; i < inputs.getCount(); ++i)
		{
			if (strcmp(inputs[i]->getSemantic(),
					   COMMON_PROFILE_INPUT_WEIGHT) != 0)
			{
				continue;
			}

			domSource* weight_source =
				daeSafeCast<domSource>(inputs[i]->getSource().getElement());
			if (weight_source)
			{
				vertex_weights = weight_source->getFloat_array();
			}
		}

		if (vertex_weights)
		{
			domListOfFloats& w = vertex_weights->getValue();
			domListOfUInts& vcount = weights->getVcount()->getValue();
			domListOfInts& v = weights->getV()->getValue();

			U32 c_idx = 0;
			for (size_t vc_idx = 0; vc_idx < vcount.getCount(); ++vc_idx)
			{
				daeUInt count = vcount[vc_idx];

				// Create list of weights that influence this vertex
				LLModel::weight_list weight_list;

				for (daeUInt i = 0; i < count; ++i)
				{
					// For each weight
					daeInt joint_idx = v[c_idx++];
					daeInt weight_idx = v[c_idx++];

					if (joint_idx == -1)
					{
						// Ignore bindings to bind_shape_matrix
						continue;
					}

					F32 weight_value = w[weight_idx];

					weight_list.emplace_back(joint_idx, weight_value);
				}

				// Sort by joint weight
				std::sort(weight_list.begin(), weight_list.end(),
						  LLModel::CompareWeightGreater());

				std::vector<LLModel::JointWeight> wght;
				F32 total = 0.f;
				for (size_t i = 0, wcount = llmin(4, (S32)weight_list.size());
					 i < wcount; ++i)
				{
					// Take up to 4 most significant weights
					if (weight_list[i].mWeight > 0.f)
					{
						wght.emplace_back(weight_list[i]);
						total += weight_list[i].mWeight;
					}
				}

				if (total == 0.f)
				{
					llwarns << "Null total weight !  Cannot normalize weights."
							<< llendl;
					continue;
				}

				F32 scale = 1.f / total;
				if (scale != 1.f)
				{
					// Normalize weights
					for (U32 i = 0; i < wght.size(); ++i)
					{
						wght[i].mWeight *= scale;
					}
				}

				model->mSkinWeights[model->mPosition[vc_idx]] = wght;
			}
		}
	}

	// Add instance to scene for this model

	LLMatrix4 transformation;
	transformation.initScale(mesh_scale_vector);
	transformation.setTranslation(mesh_translation_vector);
	transformation *= mTransform;

	std::map<std::string, LLImportMaterial> materials;
	for (U32 i = 0; i < model->mMaterialList.size(); ++i)
	{
		materials[model->mMaterialList[i]] = LLImportMaterial();
	}
	mScene[transformation].emplace_back(model, model->mLabel, transformation,
										materials);
	stretch_extents(model, transformation, mExtents[0], mExtents[1],
					mFirstTransform);
}

void LLDAELoader::buildJointToNodeMappingFromScene(daeElement* rootp)
{
	daeElement* scenep = rootp ? rootp->getDescendant("visual_scene") : NULL;
	if (scenep)
	{
		daeTArray<daeSmartRef<daeElement> > children = scenep->getChildren();
		for (S32 i = 0, count = children.getCount(); i < count; ++i)
		{
			domNode* nodep = daeSafeCast<domNode>(children[i]);
			processJointToNodeMapping(nodep);
		}
	}
}

void LLDAELoader::processJointToNodeMapping(domNode* nodep)
{
	if (!nodep)
	{
		llwarns << "NULL node pointer passed" << llendl;
		return;
	}

	if (isNodeAJoint(nodep))
	{
		// Store the parent
		std::string nodeName = nodep->getName();
		if (!nodeName.empty())
		{
			mJointsFromNode.push_front(nodep->getName());
		}
	}

	// Process the children, if any.
	processChildJoints(nodep);
}

void LLDAELoader::processChildJoints(domNode* parent_node)
{
	if (parent_node)
	{
		daeTArray<daeSmartRef<daeElement> > grand_child =
			parent_node->getChildren();
		for (S32 i = 0, count = grand_child.getCount(); i < count; ++i)
		{
			domNode* nodep = daeSafeCast<domNode>(grand_child[i]);
			if (nodep)
			{
				processJointToNodeMapping(nodep);
			}
		}
	}
}

bool LLDAELoader::isNodeAJoint(domNode* nodep)
{
	return nodep && LLModelLoader::isNodeAJoint(nodep->getName());
}

bool LLDAELoader::verifyCount(S32 expected, S32 result)
{
	if (expected != result)
	{
		llwarns << "Error. Expected: "<< expected << " - Got: " << result
				<< "vertice" << llendl;
		return false;
	}
	return true;
}

bool LLDAELoader::verifyController(domController* controllerp)
{
	bool result = true;

	domSkin* skinp = controllerp->getSkin();
	if (skinp)
	{
		xsAnyURI& uri = skinp->getSource();
		domElement* elementp = uri.getElement();
		if (!elementp)
		{
			llinfos << "Cannot resolve skin source" << llendl;
			return false;
		}

		daeString type_str = elementp->getTypeName();
		if (stricmp(type_str, "geometry") == 0)
		{
			// Skin is referenced directly by geometry; get the vertice count
			// from skin
			domSkin::domVertex_weights* vertweightp =
				skinp->getVertex_weights();
			if (!vertweightp)
			{
				llwarns << "No weigths !" << llendl;
				return false;
			}

			S32 vert_weights_count = vertweightp->getCount();
			domGeometry* geometryp =
				(domGeometry*)((domElement*)uri.getElement());
			if (!geometryp)
			{
				llwarns << "No geometry !" << llendl;
				return false;
			}

			domMesh* meshp = geometryp->getMesh();
			if (meshp)
			{
				// Get vertice count from geometry
				domVertices* verticesp = meshp->getVertices();
				if (!verticesp)
				{
					llwarns << "No vertex !" << llendl;
					return false;
				}

				xsAnyURI src = verticesp->getInput_array()[0]->getSource();
				domSource* sourcep =
					(domSource*)((domElement*)src.getElement());
				if (!sourcep)
				{
					llwarns << "No source !" << llendl;
					return false;
				}
				U32 vert_count =
					sourcep->getTechnique_common()->getAccessor()->getCount();
				result = verifyCount(vert_count, vert_weights_count);
				if (!result)
				{
					return result;
				}
			}

			S32 vcnt_count = vertweightp->getVcount()->getValue().getCount();
			result = verifyCount(vcnt_count, vert_weights_count);
			if (!result)
			{
				return result;
			}

			domInputLocalOffset_Array& inputs = vertweightp->getInput_array();
			S32 sum = 0;
			for (S32 i = 0; i < vcnt_count; ++i)
			{
				sum += vertweightp->getVcount()->getValue()[i];
			}
			result = verifyCount(sum * inputs.getCount(),
								 (S32)vertweightp->getV()->getValue().getCount());
		}
	}

	return result;
}

void LLDAELoader::extractTranslation(domTranslate* translatep,
									 LLMatrix4& transform)
{
	domFloat3 joint_trans = translatep->getValue();
	LLVector3 single_joint_trans(joint_trans[0], joint_trans[1],
									 joint_trans[2]);
	transform.setTranslation(single_joint_trans);
}

void LLDAELoader::extractTranslationViaElement(daeElement* translate_elemp,
											   LLMatrix4& transform)
{
	if (translate_elemp)
	{
		domTranslate* trans_childp =
			dynamic_cast<domTranslate*>(translate_elemp);
		if (trans_childp)
		{
			domFloat3 translate_child = trans_childp->getValue();
			LLVector3 single_joint_trans(translate_child[0],
										 translate_child[1],
										 translate_child[2]);
			transform.setTranslation(single_joint_trans);
		}
	}
}

void LLDAELoader::extractTranslationViaSID(daeElement* elementp,
										   LLMatrix4& transform)
{
	if (elementp)
	{
		daeSIDResolver resolver(elementp, "./transform");
		domMatrix* matrixp = daeSafeCast<domMatrix>(resolver.getElement());
		// We are only extracting out the translational component atm
		LLMatrix4 working_transform;
		if (matrixp)
		{
			domFloat4x4 domArray = matrixp->getValue();
			for (S32 i = 0; i < 4; ++i)
			{
				for (S32 j = 0; j < 4; ++j)
				{
					working_transform.mMatrix[i][j] = domArray[i + j * 4];
				}
			}
			LLVector3 trans = working_transform.getTranslation();
			transform.setTranslation(trans);
		}
	}
	else
	{
		llwarns << "Element is nonexistent; empty/unsupported node." << llendl;
	}
}

void LLDAELoader::processJointNode(domNode* nodep,
								   JointTransformMap& jointTransforms)
{
	if (!nodep->getName())
	{
		llwarns << "Nameless node, cannot process" << llendl;
		return;
	}

	// 1. handle the incoming node - extract out translation via SID or element
	if (isNodeAJoint(nodep))
	{
		LLMatrix4 working_transform;

		// Pull out the translate id and store it in the jointTranslations map
		daeSIDResolver joint_rsv_a(nodep, "./translate");
		domTranslate* trans_a =
			daeSafeCast<domTranslate>(joint_rsv_a.getElement());
		daeSIDResolver joint_rsv_b(nodep, "./location");
		domTranslate* trans_b =
			daeSafeCast<domTranslate>(joint_rsv_b.getElement());

		// Translation via SID was successful
		if (trans_a)
		{
			extractTranslation(trans_a, working_transform);
		}
		else if (trans_b)
		{
			extractTranslation(trans_b, working_transform);
		}
		else
		{
			// Translation via child from element
			daeElement* translate_elemp = getChildFromElement(nodep,
															  "translate");
			if (!translate_elemp ||
				translate_elemp->typeID() != domTranslate::ID())
			{
				daeSIDResolver jointResolver(nodep, "./matrix");
				domMatrix* matrixp =
					daeSafeCast<domMatrix>(jointResolver.getElement());
				if (matrixp)
				{
					domFloat4x4 domArray = matrixp->getValue();
					for (S32 i = 0; i < 4; ++i)
					{
						for (S32 j = 0; j < 4; ++j)
						{
							working_transform.mMatrix[i][j] =
								domArray[i + j * 4];
						}
					}
				}
				else
				{
					llwarns << "The element found is not translate or matrix node; most likely a corrupt export !"
							<< llendl;
				}
			}
			else
			{
				extractTranslationViaElement(translate_elemp,
											 working_transform);
			}
		}

		// Store the working transform relative to the nodes name.
		jointTransforms[nodep->getName()] = working_transform;
	}

	// 2. handle the nodes children

	// Gather and handle the incoming nodes children
	daeTArray<daeSmartRef<daeElement> > grand_child = nodep->getChildren();
	S32 grand_child_count = grand_child.getCount();
	for (S32 i = 0; i < grand_child_count; ++i)
	{
		domNode* child_nodep = daeSafeCast<domNode>(grand_child[i]);
		if (child_nodep)
		{
			processJointNode(child_nodep, jointTransforms);
		}
	}
}

daeElement* LLDAELoader::getChildFromElement(daeElement* elementp,
											 const std::string& name)
{
    daeElement* element_chilp = elementp->getChild(name.c_str());
	if (element_chilp)
	{
		return element_chilp;
	}
	LL_DEBUGS("MeshUpload") << "Could not find child '" << name
							<< "' for element '"
							<< elementp->getAttribute("id") << "'" << llendl;
    return NULL;
}

void LLDAELoader::processElement(daeElement* element, bool& bad_element,
								 DAE* dae)
{
	LLMatrix4 saved_transform;
	bool pushed_mat = false;

	domNode* node = daeSafeCast<domNode>(element);
	if (node)
	{
		pushed_mat = true;
		saved_transform = mTransform;
	}

	domTranslate* translate = daeSafeCast<domTranslate>(element);
	if (translate)
	{
		domFloat3 dom_value = translate->getValue();

		LLMatrix4 translation;
		translation.setTranslation(LLVector3(dom_value[0], dom_value[1],
											 dom_value[2]));
		translation *= mTransform;
		mTransform = translation;
		mTransform.condition();
	}

	domRotate* rotate = daeSafeCast<domRotate>(element);
	if (rotate)
	{
		domFloat4 dom_value = rotate->getValue();

		LLMatrix4 rotation;
		rotation.initRotTrans(dom_value[3] * DEG_TO_RAD,
							  LLVector3(dom_value[0], dom_value[1],
										dom_value[2]),
							  LLVector3(0.f, 0.f, 0.f));
		rotation *= mTransform;
		mTransform = rotation;
		mTransform.condition();
	}

	domScale* scale = daeSafeCast<domScale>(element);
	if (scale)
	{
		domFloat3 dom_value = scale->getValue();

		LLVector3 scale_vector = LLVector3(dom_value[0], dom_value[1],
										   dom_value[2]);

		// Set all values positive, since we do not currently support mirrored
		// meshes
		scale_vector.abs();

		LLMatrix4 scaling;
		scaling.initScale(scale_vector);
		scaling *= mTransform;
		mTransform = scaling;
		mTransform.condition();
	}

	domMatrix* matrix = daeSafeCast<domMatrix>(element);
	if (matrix)
	{
		domFloat4x4 dom_value = matrix->getValue();
		LLMatrix4 matrix_transform;
		for (S32 i = 0; i < 4; i++)
		{
			for (S32 j = 0; j < 4; j++)
			{
				matrix_transform.mMatrix[i][j] = dom_value[i + j * 4];
			}
		}

		matrix_transform *= mTransform;
		mTransform = matrix_transform;
		mTransform.condition();
	}

	domInstance_geometry* instance_geo =
		daeSafeCast<domInstance_geometry>(element);
	if (instance_geo)
	{
		domGeometry* geo =
			daeSafeCast<domGeometry>(instance_geo->getUrl().getElement());
		if (geo)
		{
			domMesh* mesh =
				daeSafeCast<domMesh>(geo->getDescendant(daeElement::matchType(domMesh::ID())));
			if (mesh)
			{
				std::vector<LLPointer<LLModel> >::iterator i = mModelsMap[mesh].begin();
				while (i != mModelsMap[mesh].end())
				{
					LLModel* model = *i;

					LLMatrix4 transformation = mTransform;

					if (mTransform.determinant() < 0)
					{
						// Negative scales are not supported
						llwarns << "Negative scale detected, unsupported transform. domInstance_geometry: "
								<< getElementLabel(instance_geo) << llendl;
						LLSD args;
						args["Message"] = "NegativeScaleTrans";
						args["LABEL"] = getElementLabel(instance_geo);
						mWarningsArray.append(args);
						bad_element = true;
					}

					LLModelLoader::material_map materials = getMaterials(model,
																		 instance_geo,
																		 dae);

					// Adjust the transformation to compensate for mesh
					// normalization
					LLVector3 mesh_scale_vector;
					LLVector3 mesh_translation_vector;
					model->getNormalizedScaleTranslation(mesh_scale_vector,
														 mesh_translation_vector);

					LLMatrix4 mesh_translation;
					mesh_translation.setTranslation(mesh_translation_vector);
					mesh_translation *= transformation;
					transformation = mesh_translation;

					LLMatrix4 mesh_scale;
					mesh_scale.initScale(mesh_scale_vector);
					mesh_scale *= transformation;
					transformation = mesh_scale;

					if (transformation.determinant() < 0)
					{
						// Negative scales are not supported
						llwarns << "Negative scale detected, unsupported post-normalization transform. domInstance_geometry: "
								<< getElementLabel(instance_geo) << llendl;
						LLSD args;
						args["Message"] = "NegativeScaleNormTrans";
						args["LABEL"] = getElementLabel(instance_geo);
						mWarningsArray.append(args);
						bad_element = true;
					}

					std::string label;
					if (model->mLabel.empty())
					{
						label = getLodlessLabel(instance_geo);

						llassert(!label.empty());

						if (model->mSubmodelID)
						{
							label += (char)((int)'a' + model->mSubmodelID);
						}

						model->mLabel = label + lod_suffix[mLod];
					}
					else
					{
						// Do not change model's name if possible, it will play
						// havoc with scenes that already use said model.
						size_t ext_pos = getSuffixPosition(model->mLabel);
						if (ext_pos != std::string::npos)
						{
							label = model->mLabel.substr(0, ext_pos);
						}
						else
						{
							label = model->mLabel;
						}
					}

					mScene[transformation].emplace_back(model, label,
														transformation,
														materials);
					stretch_extents(model, transformation, mExtents[0],
									mExtents[1], mFirstTransform);
					++i;
				}
			}
		}
		else
		{
			llwarns << "Unable to resolve geometry URL." << llendl;
			LLSD args;
			args["Message"] = "CantResolveGeometryUrl";
			mWarningsArray.append(args);
			bad_element = true;
		}
	}

	domInstance_node* instance_node = daeSafeCast<domInstance_node>(element);
	if (instance_node)
	{
		daeElement* instance = instance_node->getUrl().getElement();
		if (instance)
		{
			processElement(instance,bad_element, dae);
		}
	}

	// Process children
	daeTArray<daeSmartRef<daeElement> > children = element->getChildren();
	for (S32 i = 0, child_count = children.getCount(); i < child_count; ++i)
	{
		processElement(children[i], bad_element, dae);
	}

	if (pushed_mat)
	{
		// This element was a node, restore transform before processing
		// siblings
		mTransform = saved_transform;
	}
}

std::map<std::string, LLImportMaterial> LLDAELoader::getMaterials(LLModel* model,
																  domInstance_geometry* instance_geo,
																  DAE* dae)
{
	std::map<std::string, LLImportMaterial> materials;
	for (size_t i = 0; i < model->mMaterialList.size(); ++i)
	{
		LLImportMaterial import_material;
		domInstance_material* instance_mat = NULL;

		domBind_material::domTechnique_common* technique =
			daeSafeCast<domBind_material::domTechnique_common>(instance_geo->getDescendant(daeElement::matchType(domBind_material::domTechnique_common::ID())));
		if (technique)
		{
			daeTArray<daeSmartRef<domInstance_material> > inst_materials = technique->getChildrenByType<domInstance_material>();
			for (S32 j = 0, cnt = inst_materials.getCount(); j < cnt; ++j)
			{
				std::string symbol(inst_materials[j]->getSymbol());
				if (symbol == model->mMaterialList[i])
				{
					// Found the binding
					instance_mat = inst_materials[j];
					break;
				}
			}
		}

		if (instance_mat)
		{
			domMaterial* material =
				daeSafeCast<domMaterial>(instance_mat->getTarget().getElement());
			if (material)
			{
				domInstance_effect* instance_effect =
					daeSafeCast<domInstance_effect>(material->getDescendant(daeElement::matchType(domInstance_effect::ID())));
				if (instance_effect)
				{
					domEffect* effect =
						daeSafeCast<domEffect>(instance_effect->getUrl().getElement());
					if (effect)
					{
						domProfile_COMMON* profile =
							daeSafeCast<domProfile_COMMON>(effect->getDescendant(daeElement::matchType(domProfile_COMMON::ID())));
						if (profile)
						{
							import_material = profileToMaterial(profile, dae);
						}
					}
				}
			}
		}

		import_material.mBinding = model->mMaterialList[i];
		materials[model->mMaterialList[i]] = import_material;
	}

	return materials;
}

LLImportMaterial LLDAELoader::profileToMaterial(domProfile_COMMON* material,
												DAE* dae)
{
	LLImportMaterial mat;
	mat.mFullbright = false;

	daeElement* diffuse = material->getDescendant("diffuse");
	if (diffuse)
	{
		domCommon_color_or_texture_type_complexType::domTexture* texture =
			daeSafeCast<domCommon_color_or_texture_type_complexType::domTexture>(diffuse->getDescendant("texture"));
		if (texture)
		{
			domCommon_newparam_type_Array newparams = material->getNewparam_array();
			if (newparams.getCount())
			{
				for (S32 i = 0, cnt = newparams.getCount(); i < cnt; ++i)
				{
					domFx_surface_common* surface = newparams[i]->getSurface();
					if (surface)
					{
						domFx_surface_init_common* init =
							surface->getFx_surface_init_common();
						if (init)
						{
							domFx_surface_init_from_common_Array init_from =
								init->getInit_from_array();
							if ((S32)init_from.getCount() > i)
							{
								domImage* image =
									daeSafeCast<domImage>(init_from[i]->getValue().getElement());
								if (image)
								{
									// We only support init_from now - embedded
									// data will come later
									domImage::domInit_from* initfm;
									initfm = image->getInit_from();
									if (initfm)
									{
										mat.mDiffuseMapFilename =
											cdom::uriToNativePath(initfm->getValue().str());
										mat.mDiffuseMapLabel =
											getElementLabel(material);
									}
								}
							}
						}
					}
				}
			}
			else if (texture->getTexture())
			{
				domImage* image = NULL;
				dae->getDatabase()->getElement((daeElement**)&image, 0,
											   texture->getTexture(),
											   COLLADA_TYPE_IMAGE);
				if (image)
				{
					// We only support init_from now - embedded data will come
					// later
					domImage::domInit_from* init = image->getInit_from();
					if (init)
					{
						std::string img_path_val =
							cdom::uriToNativePath(init->getValue().str());

#if LL_WINDOWS
						// Work-around DOM tendency to resort to UNC names
						// which are only confusing for downstream...
						std::string::iterator i = img_path_val.begin();
						while (*i == '\\')
						{
							++i;
						}
						mat.mDiffuseMapFilename.assign(i, img_path_val.end());
#else
						mat.mDiffuseMapFilename = img_path_val;
#endif
						mat.mDiffuseMapLabel = getElementLabel(material);
					}
				}
			}
		}

		domCommon_color_or_texture_type_complexType::domColor* color =
			daeSafeCast<domCommon_color_or_texture_type_complexType::domColor>(diffuse->getDescendant("color"));
		if (color)
		{
			domFx_color_common domfx_color = color->getValue();
			LLColor4 value = LLColor4(domfx_color[0], domfx_color[1],
									  domfx_color[2], domfx_color[3]);
			mat.mDiffuseColor = value;
		}
	}

	daeElement* emission = material->getDescendant("emission");
	if (emission)
	{
		LLColor4 emission_color = getDaeColor(emission);
		if (((emission_color[0] + emission_color[1] +
			  emission_color[2]) / 3.f) > 0.25f)
		{
			mat.mFullbright = true;
		}
	}

	return mat;
}

// Try to get a decent label for this element
std::string LLDAELoader::getElementLabel(daeElement* element)
{
	std::string name;
	if (!element)
	{
		return name;
	}

	// If we have a name attribute, use it
	name = element->getAttribute("name");
	if (name.length())
	{
		return name;
	}

	// If we have an ID attribute, use it
	if (element->getID())
	{
		return std::string(element->getID());
	}

	// If we have a parent, use it
	daeElement* parent = element->getParent();
	std::string index_string;
	if (parent)
	{
		// Retrieve index to distinguish items inside same parent
		size_t ind = 0;
		parent->getChildren().find(element, ind);
		if (ind > 0)
		{
			index_string = "_" + boost::lexical_cast<std::string>(ind);
		}

		// If parent has a name or ID, use it
		std::string name = parent->getAttribute("name");
		if (!name.length())
		{
			name = std::string(parent->getID());
		}

		if (name.length())
		{
			// Make sure that index will not mix up with pre-named LOD
			// extensions
			size_t ext_pos = getSuffixPosition(name);
			if (ext_pos == std::string::npos)
			{
				return name + index_string;
			}
			else
			{
				return name.insert(ext_pos, index_string);
			}
		}
	}

	// Try to use our type
	daeString element_name = element->getElementName();
	if (element_name)
	{
		return std::string(element_name) + index_string;
	}

	// If all else fails, use "object"
	return "object" + index_string;
}

//static
size_t LLDAELoader::getSuffixPosition(const std::string& label)
{
	if (label.find("_LOD") != std::string::npos ||
		label.find("_PHYS") != std::string::npos)
	{
		return label.rfind('_');
	}
	return std::string::npos;
}

//static
std::string LLDAELoader::getLodlessLabel(daeElement* element)
{
	std::string label = getElementLabel(element);
	size_t ext_pos = getSuffixPosition(label);
	if (ext_pos != std::string::npos)
	{
		return label.substr(0, ext_pos);
	}
	return label;
}

LLColor4 LLDAELoader::getDaeColor(daeElement* element)
{
	LLColor4 value;

	domCommon_color_or_texture_type_complexType::domColor* color =
		daeSafeCast<domCommon_color_or_texture_type_complexType::domColor>(element->getDescendant("color"));
	if (color)
	{
		domFx_color_common domfx_color = color->getValue();
		value = LLColor4(domfx_color[0], domfx_color[1], domfx_color[2],
						 domfx_color[3]);
	}

	return value;
}

bool LLDAELoader::addVolumeFacesFromDomMesh(LLModel* modelp, domMesh* meshp,
											LLSD& log_msg)
{
	if (!modelp || !meshp) return false;

	LLModel::EModelStatus status = LLModel::NO_ERRORS;
	domTriangles_Array& tris = meshp->getTriangles_array();

	for (U32 i = 0, count = tris.getCount(); i < count; ++i)
	{
		domTrianglesRef& tri = tris.get(i);
		status = load_face_from_dom_tris(modelp->getVolumeFaces(),
										 modelp->getMaterialList(), tri,
										 log_msg);
		modelp->mStatus = status;
		if (status != LLModel::NO_ERRORS)
		{
			modelp->clearFacesAndMaterials();
			return false;
		}
	}

	domPolylist_Array& polys = meshp->getPolylist_array();
	for (U32 i = 0, count = polys.getCount(); i < count; ++i)
	{
		domPolylistRef& poly = polys.get(i);
		status = load_face_from_dom_polylist(modelp->getVolumeFaces(),
											 modelp->getMaterialList(), poly,
											 log_msg);
		if (status != LLModel::NO_ERRORS)
		{
			modelp->clearFacesAndMaterials();
			return false;
		}
	}

	domPolygons_Array& polygons = meshp->getPolygons_array();
	for (U32 i = 0, poly_count = polygons.getCount(); i < poly_count; ++i)
	{
		domPolygonsRef& poly = polygons.get(i);
		status = load_face_from_dom_polygons(modelp->getVolumeFaces(),
											 modelp->getMaterialList(), poly);
		if (status != LLModel::NO_ERRORS)
		{
			modelp->clearFacesAndMaterials();
			return false;
		}
	}

	// If we are missing normals, do a quick and dirty calculation of them.
	// Use the normals of each vertex' connected faces and sum them up. Should
	// the user select "Generate normals" from the mesh upload floater, more
	// accurate normals will replace these.
	LLVolume::face_list_t vol_faces = modelp->getVolumeFaces();
	for (LLVolume::face_list_t::iterator it = vol_faces.begin(),
										 end = vol_faces.end();
		 it != end; ++it)
	{
		LLVolumeFace& face = *it;
		if (face.mNormals || !face.mIndices || face.mNumIndices % 3)
		{
			continue;
		}

		face.mNormals = face.mPositions + face.mNumVertices;
		for (S32 i = 0, count = face.mNumVertices; i < count; ++i)
		{
			face.mNormals[i].clear();
		}

		for (S32 i = 0, count = face.mNumIndices; i < count; )
		{
			LLVector4a v0(face.mPositions[face.mIndices[i]]);
			LLVector4a v1(face.mPositions[face.mIndices[i + 1]]);
			LLVector4a v2(face.mPositions[face.mIndices[i + 2]]);

			LLVector4a normal;
			v2.sub(v1);
			v1.sub(v0);
			normal.setCross3(v1, v2);
			normal.normalize3();

			face.mNormals[face.mIndices[i++]].add(normal);
			face.mNormals[face.mIndices[i++]].add(normal);
			face.mNormals[face.mIndices[i++]].add(normal);
		}

		for (S32 i = 0, count = face.mNumVertices; i < count; ++i)
		{
			face.mNormals[i].normalize3();
		}
	}

	return status == LLModel::NO_ERRORS;
}

// Diff version supports creating multiple models when material counts spill
// over the 8 face server-side limit
//static
bool LLDAELoader::loadModelsFromDomMesh(domMesh* mesh,
										std::vector<LLModel*>& models_out,
										U32 submodel_limit)
{
	LLVolumeParams volume_params;
	volume_params.setType(LL_PCODE_PROFILE_SQUARE, LL_PCODE_PATH_LINE);

	models_out.clear();

	LLModel* ret = new LLModel(volume_params, 0.f);

	std::string model_name = getLodlessLabel(mesh);
	ret->mLabel = model_name + lod_suffix[mLod];

	llassert(!ret->mLabel.empty());

	ret->clearFacesAndMaterials();

	// Get the whole set of volume faces
	addVolumeFacesFromDomMesh(ret, mesh, mWarningsArray);

	U32 volume_faces = ret->getNumVolumeFaces();

	// Side-steps all manner of issues when splitting models and matching lower
	// LOD materials to base models
	ret->sortVolumeFacesByMaterialName();

#if !LL_NORMALIZE_ALL_MODELS
	bool normalized = false;
#endif
    S32 submodel_id = 0;

	// Remove all faces that definitely would not fit into one model and
	// sub-model limit.
	U32 face_limit = (submodel_limit + 1) * LL_SCULPT_MESH_MAX_FACES;
	if (face_limit < volume_faces)
	{
		ret->setNumVolumeFaces(face_limit);
	}

	LLVolume::face_list_t remainder;
	do
	{
		// Ensure we do this once with the whole gang and not per-model
#if !LL_NORMALIZE_ALL_MODELS
		if (!normalized && !mNoNormalize)
		{
			normalized = true;
			ret->normalizeVolumeFaces();
		}
#endif
		ret->trimVolumeFacesToSize(LL_SCULPT_MESH_MAX_FACES, &remainder);

		// Remove unused/redundant vertices after normalizing
		if (!mNoOptimize)
		{
			ret->remapVolumeFaces();
		}

		volume_faces = remainder.size();

		models_out.push_back(ret);

		// If we have left-over volume faces, create another model to absorb
		// them...
		if (volume_faces)
		{
			LLModel* next = new LLModel(volume_params, 0.f);
			next->mSubmodelID = ++submodel_id;
			next->mLabel = model_name + (char)((int)'a' + next->mSubmodelID) +
						   lod_suffix[mLod];
			next->getVolumeFaces() = remainder;
			next->mNormalizedScale = ret->mNormalizedScale;
			next->mNormalizedTranslation = ret->mNormalizedTranslation;
			if ((S32)ret->mMaterialList.size() > LL_SCULPT_MESH_MAX_FACES)
			{
				next->mMaterialList.assign(ret->mMaterialList.begin() +
										   LL_SCULPT_MESH_MAX_FACES,
										   ret->mMaterialList.end());
			}
			ret = next;
		}

		remainder.clear();

	}
	while (volume_faces);

	return true;
}
