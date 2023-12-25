/**
 * @file llvograss.cpp
 * @brief Not a blade, but a clump of grass
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 *
 * Copyright (c) 2001-2009, Linden Research, Inc.
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

#include "llviewerprecompiledheaders.h"

#include "llvograss.h"

#include "imageids.h"
#include "lldir.h"
#include "llfasttimer.h"
#include "llnotifications.h"
#include "llxmltree.h"

#include "llagent.h"
#include "lldrawable.h"
#include "lldrawpoolalpha.h"
#include "llface.h"
#include "llpipeline.h"
#include "llselectmgr.h"
#include "llsky.h"
#include "llspatialpartition.h"
#include "llsurface.h"
#include "llsurfacepatch.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerregion.h"
#include "llviewertexturelist.h"
#include "llvotree.h"
#include "llvosky.h"
#include "llworld.h"

constexpr S32 GRASS_MAX_BLADES		= 32;
constexpr F32 GRASS_BLADE_BASE		= 0.25f;	// Width of grass at base
constexpr F32 GRASS_BLADE_HEIGHT	= 0.5f;		// In meters
constexpr F32 GRASS_DISTRIBUTION_SD	= 0.15f;	// Empirically defined

F32 exp_x[GRASS_MAX_BLADES];
F32 exp_y[GRASS_MAX_BLADES];
F32 rot_x[GRASS_MAX_BLADES];
F32 rot_y[GRASS_MAX_BLADES];
F32 dz_x [GRASS_MAX_BLADES];
F32 dz_y [GRASS_MAX_BLADES];
//  Factor to modulate wind movement by to randomize appearance
F32 w_mod[GRASS_MAX_BLADES];

LLVOGrass::data_map_t LLVOGrass::sSpeciesTable;
LLVOGrass::species_list_t LLVOGrass::sSpeciesNames;
S32 LLVOGrass::sMaxGrassSpecies = 0;

///////////////////////////////////////////////////////////////////////////////
// LLVOGrass class
///////////////////////////////////////////////////////////////////////////////

LLVOGrass::LLVOGrass(const LLUUID& id, LLViewerRegion* regionp)
:	LLAlphaObject(id, LL_PCODE_LEGACY_GRASS, regionp),
	mPatch(NULL),
	mLastPatchUpdateTime(0),
	mNumBlades(GRASS_MAX_BLADES)
{
	mCanSelect = true;
	setNumTEs(1);
	setTEColor(0, LLColor4(1.f, 1.f, 1.f, 1.f));
}

void LLVOGrass::updateSpecies()
{
	mSpecies = mAttachmentState;

	if (!sSpeciesTable.count(mSpecies))
	{
		llinfos << "Unknown grass type, substituting grass type." << llendl;
		data_map_t::const_iterator it = sSpeciesTable.begin();
		mSpecies = it->first;
	}
	setTEImage(0,
			   LLViewerTextureManager::getFetchedTexture(sSpeciesTable[mSpecies]->mTextureID,
														 FTT_DEFAULT,
														 true,
														 LLGLTexture::BOOST_NONE,
														 LLViewerTexture::LOD_TEXTURE));
}

void LLVOGrass::initClass()
{
	std::string xml_filename =
		gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "grass.xml");
	LLXmlTree grass_def_grass;
	if (!grass_def_grass.parseFile(xml_filename))
	{
		llerrs << "Failed to parse grass file." << llendl;
		return;
	}

	LLXmlTreeNode* rootp = grass_def_grass.getRoot();
	if (!rootp)
	{
		llerrs << "Failed to parse grass file." << llendl;
		return;
	}

	LLUUID id;
	F32 f32_val;
	for (LLXmlTreeNode* grass_def = rootp->getFirstChild(); grass_def;
		 grass_def = rootp->getNextChild())
	{
		if (!grass_def->hasName("grass"))
		{
			llwarns << "Invalid grass definition node " << grass_def->getName()
					<< llendl;
			continue;
		}

		bool success = true;

		S32 species;
		static LLStdStringHandle species_id_string =
			LLXmlTree::addAttributeString("species_id");
		if (!grass_def->getFastAttributeS32(species_id_string, species))
		{
			llwarns << "No species id defined" << llendl;
			continue;
		}

		if (species < 0)
		{
			llwarns << "Invalid species id " << species << llendl;
			continue;
		}

		GrassSpeciesData* new_grass = new GrassSpeciesData();

		static LLStdStringHandle texture_id_string =
			LLXmlTree::addAttributeString("texture_id");
		grass_def->getFastAttributeUUID(texture_id_string, id);
		new_grass->mTextureID = id;

		if (new_grass->mTextureID.isNull())
		{
			static LLStdStringHandle texture_name_string =
				LLXmlTree::addAttributeString("texture_name");

			std::string tex_name;
			success &= grass_def->getFastAttributeString(texture_name_string,
														 tex_name);
			LLViewerTexture* grass_image =
				LLViewerTextureManager::getFetchedTextureFromFile(tex_name);
			new_grass->mTextureID = grass_image->getID();
		}

		static LLStdStringHandle blade_sizex_string =
			LLXmlTree::addAttributeString("blade_size_x");
		success &= grass_def->getFastAttributeF32(blade_sizex_string, f32_val);
		new_grass->mBladeSizeX = f32_val;

		static LLStdStringHandle blade_sizey_string =
			LLXmlTree::addAttributeString("blade_size_y");
		success &= grass_def->getFastAttributeF32(blade_sizey_string, f32_val);
		new_grass->mBladeSizeY = f32_val;

		if (sSpeciesTable.count(species))
		{
			llinfos << "Grass species " << species
					<< " already defined !  Duplicate discarded." << llendl;
			delete new_grass;
			continue;
		}
		else
		{
			sSpeciesTable[species] = new_grass;
		}

		if (species >= sMaxGrassSpecies)
		{
			sMaxGrassSpecies = species + 1;
		}

		std::string name;
		static LLStdStringHandle name_string =
			LLXmlTree::addAttributeString("name");
		success &= grass_def->getFastAttributeString(name_string, name);
		sSpeciesNames[name] = species;

		if (!success)
		{
			llwarns << "Incomplete definition of grass " << name << llendl;
		}
	}

	bool have_all_grass = true;
	std::string err;

	for (S32 i = 0; i < sMaxGrassSpecies; ++i)
	{
		if (!sSpeciesTable.count(i))
		{
			err.append(llformat(" %d", i));
			have_all_grass = false;
		}
	}

	if (!have_all_grass)
	{
		LLSD args;
		args["SPECIES"] = err;
		gNotifications.add("ErrorUndefinedGrasses", args);
	}

	//  Create nifty list of exponential distribution 0-1
	F32 x = 0.f;
	F32 y = 0.f;
	F32 rot;
	for (S32 i = 0; i < GRASS_MAX_BLADES; ++i)
	{
#if 0	// Set to 1 for X blading
		if (i % 2 == 0)
		{
			F32 u = sqrtf(-2.f * logf(ll_frand()));
			F32 v = 2.f * F_PI * ll_frand();

			x = u * sinf(v) * GRASS_DISTRIBUTION_SD;
			y = u * cosf(v) * GRASS_DISTRIBUTION_SD;

			rot = ll_frand(F_PI);
		}
		else
		{
			rot += (F_PI * 0.4f + ll_frand(0.2f * F_PI));
		}
#else
		F32 u = sqrtf(-2.f * logf(ll_frand()));
		F32 v = 2.f * F_PI * ll_frand();

		x = u * sinf(v) * GRASS_DISTRIBUTION_SD;
		y = u * cosf(v) * GRASS_DISTRIBUTION_SD;

		rot = ll_frand(F_PI);
#endif

		exp_x[i] = x;
		exp_y[i] = y;
		rot_x[i] = sinf(rot);
		rot_y[i] = cosf(rot);
		dz_x[i] = ll_frand(GRASS_BLADE_BASE * 0.25f);
		dz_y[i] = ll_frand(GRASS_BLADE_BASE * 0.25f);
		// Degree to which blade is moved by wind
		w_mod[i] = 0.5f + ll_frand();

	}
}

void LLVOGrass::cleanupClass()
{
	for_each(sSpeciesTable.begin(), sSpeciesTable.end(),
			 DeletePairedPointer());
	sSpeciesTable.clear();
}

U32 LLVOGrass::processUpdateMessage(LLMessageSystem* mesgsys,
									void** user_data, U32 block_num,
									EObjectUpdateType update_type,
									LLDataPacker* dp)
{
	// Do base class updates...
	U32 retval = LLViewerObject::processUpdateMessage(mesgsys, user_data,
													  block_num, update_type,
													  dp);
	updateSpecies();

	if (getVelocity().lengthSquared() > 0.f ||
		getAcceleration().lengthSquared() > 0.f ||
		getAngularVelocity().lengthSquared() > 0.f)
	{
		llinfos << "ACK ! Moving grass !" << llendl;
		setVelocity(LLVector3::zero);
		setAcceleration(LLVector3::zero);
		setAngularVelocity(LLVector3::zero);
	}

	if (mDrawable)
	{
		gPipeline.markRebuild(mDrawable);
	}

	return retval;
}

void LLVOGrass::idleUpdate(F64 time)
{
 	if (mDead || !gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_GRASS) ||
		!mDrawable)	// So that drones work.
	{
		return;
	}

	if (LLVOTree::isTreeRenderingStopped())	// Stop rendering grass
	{
		if (mNumBlades)
		{
			mNumBlades = 0;
			gPipeline.markRebuild(mDrawable);
		}
		return;
	}

	if (!mNumBlades)						// Restart grass rendering
	{
		mNumBlades = GRASS_MAX_BLADES;
		gPipeline.markRebuild(mDrawable);
		return;
	}

	if (mPatch && mLastPatchUpdateTime != mPatch->getLastUpdateTime())
	{
		gPipeline.markRebuild(mDrawable, LLDrawable::REBUILD_VOLUME);
	}
}

void LLVOGrass::setPixelAreaAndAngle()
{
	// This should be the camera's center, as soon as we move to all
	// region-local.
	LLVector3 relative_position = getPositionAgent() -
								  gAgent.getCameraPositionAgent();
	F32 range = relative_position.length();

	F32 max_scale = getMaxScale();

	mAppAngle = atan2f(max_scale, range) * RAD_TO_DEG;

	// Compute pixels per meter at the given range
	F32 pixels_per_meter = gViewerCamera.getViewHeightInPixels() /
						   (tanf(gViewerCamera.getView()) * range);

	// Assume grass texture is a 5 meter by 5 meter sprite at the grass
	// object's center
	mPixelArea = pixels_per_meter * pixels_per_meter * 25.f;
}

// *TODO: could speed this up by caching the relative_position and range
// calculations
void LLVOGrass::updateTextures()
{
	if (getTEImage(0))
	{
		if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_TEXTURE_AREA))
		{
			setDebugText(llformat("%4.0f", sqrtf(mPixelArea)));
		}
		getTEImage(0)->addTextureStats(mPixelArea);
	}
}

bool LLVOGrass::updateLOD()
{
	if (mDrawable->getNumFaces() <= 0)
	{
		return false;
	}
	if (LLVOTree::isTreeRenderingStopped())
	{
		if (mNumBlades)
		{
			mNumBlades = 0;
			gPipeline.markRebuild(mDrawable);
		}
		return true;
	}
	if (!mNumBlades)
	{
		mNumBlades = GRASS_MAX_BLADES;
	}

	LLFace* face = mDrawable->getFace(0);
	if (!face)
	{
		return false;
	}

	F32 tan_angle = mScale.mV[0] * mScale.mV[1] /
					mDrawable->mDistanceWRTCamera;
	S32 num_blades = llmin(GRASS_MAX_BLADES, lltrunc(tan_angle * 5.f));
	num_blades = llmax(1, num_blades);
	if (num_blades >= (mNumBlades << 1))
	{
		while (mNumBlades < num_blades)
		{
			mNumBlades <<= 1;
		}

		face->setSize(mNumBlades * 8, mNumBlades * 12);
		gPipeline.markRebuild(mDrawable);
	}
	else if (num_blades <= (mNumBlades >> 1))
	{
		while (mNumBlades > num_blades)
		{
			mNumBlades >>= 1;
		}

		face->setSize(mNumBlades * 8, mNumBlades * 12);
		gPipeline.markRebuild(mDrawable);
		return true;
	}

	return false;
}

LLDrawable* LLVOGrass::createDrawable()
{
	gPipeline.allocDrawable(this);
	mDrawable->setRenderType(LLPipeline::RENDER_TYPE_GRASS);
	return mDrawable;
}

bool LLVOGrass::updateGeometry(LLDrawable* drawable)
{
	LL_FAST_TIMER(FTM_UPDATE_GRASS);

	dirtySpatialGroup();

	if (!mNumBlades)	// Stop rendering grass
	{
		if (mDrawable->getNumFaces() > 0)
		{
			LLFace* facep = mDrawable->getFace(0);
			if (facep)
			{
				facep->setSize(0, 0);
			}
		}
	}
	else
	{
		plantBlades();
	}

	return true;
}

void LLVOGrass::plantBlades()
{
	// It is possible that the species of a grass is not defined. This is bad,
	// but not the end of the world.
	if (!sSpeciesTable.count(mSpecies))
	{
		llinfos << "Unknown grass species " << mSpecies << llendl;
		return;
	}

	if (mDrawable->getNumFaces() < 1)
	{
		mDrawable->setNumFaces(1, NULL, getTEImage(0));
	}

	LLFace* face = mDrawable->getFace(0);
	if (!face)
	{
		return;
	}

	face->setDiffuseMap(getTEImage(0));
	face->setState(LLFace::GLOBAL);
	face->setSize(mNumBlades * 8, mNumBlades * 12);
	face->setVertexBuffer(NULL);
	face->setTEOffset(0);
	face->mCenterLocal = mPosition + mRegionp->getOriginAgent();

	mDepth = (face->mCenterLocal - gViewerCamera.getOrigin()) *
			  gViewerCamera.getAtAxis();
	mDrawable->setPosition(face->mCenterLocal);
	mDrawable->movePartition();
}

void LLVOGrass::getGeometry(S32 idx,
							LLStrider<LLVector4a>& verticesp,
							LLStrider<LLVector3>& normalsp,
							LLStrider<LLVector2>& texcoordsp,
							LLStrider<LLColor4U>& colorsp,
							LLStrider<LLColor4U>& emissivep,
							LLStrider<U16>& indicesp)
{
	if (!mNumBlades)	// stop rendering grass
	{
		return;
	}

	mPatch = mRegionp->getLand().resolvePatchRegion(getPositionRegion());
	if (mPatch)
	{
		mLastPatchUpdateTime = mPatch->getLastUpdateTime();
	}

	LLVector3 position;
	// Create random blades of grass with gaussian distribution
	F32 x, y, xf, yf, dzx, dzy;

	LLColor4U color(255, 255, 255, 255);

	LLFace* face = mDrawable->getFace(idx);
	if (!face)
	{
		return;
	}

	F32 width  = sSpeciesTable[mSpecies]->mBladeSizeX;
	F32 height = sSpeciesTable[mSpecies]->mBladeSizeY;

	U32 index_offset = face->getGeomIndex();

	LLVector3 origin_agent = mRegionp->getOriginAgent();

	for (S32 i = 0;  i < mNumBlades; ++i)
	{
		x   = exp_x[i] * mScale.mV[VX];
		y   = exp_y[i] * mScale.mV[VY];
		xf  = rot_x[i] * GRASS_BLADE_BASE * width * w_mod[i];
		yf  = rot_y[i] * GRASS_BLADE_BASE * width * w_mod[i];
		dzx = dz_x [i];
		dzy = dz_y [i];

		LLVector3 v1, v2, v3;
		F32 blade_height = GRASS_BLADE_HEIGHT * height * w_mod[i];

		*texcoordsp++ = LLVector2(0, 0);
		*texcoordsp++ = LLVector2(0, 0);
		*texcoordsp++ = LLVector2(0, 0.98f);
		*texcoordsp++ = LLVector2(0, 0.98f);
		*texcoordsp++ = LLVector2(1, 0);
		*texcoordsp++ = LLVector2(1, 0);
		*texcoordsp++ = LLVector2(1, 0.98f);
		*texcoordsp++ = LLVector2(1, 0.98f);

		position.mV[0] = mPosition.mV[VX] + x + xf;
		position.mV[1] = mPosition.mV[VY] + y + yf;
		position.mV[2] = mRegionp->getLand().resolveHeightRegion(position);
		v1 = position + origin_agent;
		(*verticesp++).load3(v1.mV);
		(*verticesp++).load3(v1.mV);

		position.mV[0] += dzx;
		position.mV[1] += dzy;
		position.mV[2] += blade_height;
		v2 = position + origin_agent;
		(*verticesp++).load3(v2.mV);
		(*verticesp++).load3(v2.mV);

		position.mV[0] = mPosition.mV[VX] + x - xf;
		position.mV[1] = mPosition.mV[VY] + y - xf;
		position.mV[2] = mRegionp->getLand().resolveHeightRegion(position);
		v3 = position + origin_agent;
		(*verticesp++).load3(v3.mV);
		(*verticesp++).load3(v3.mV);

		LLVector3 normal1 = (v1 - v2) % (v2 - v3);
		normal1.mV[VZ] = 0.75f;
		normal1.normalize();
		LLVector3 normal2 = -normal1;
		normal2.mV[VZ] = -normal2.mV[VZ];

		position.mV[0] += dzx;
		position.mV[1] += dzy;
		position.mV[2] += blade_height;
		v1 = position + origin_agent;
		(*verticesp++).load3(v1.mV);
		(*verticesp++).load3(v1.mV);

		*(normalsp++) = normal1;
		*(normalsp++) = normal2;
		*(normalsp++) = normal1;
		*(normalsp++) = normal2;

		*(normalsp++) = normal1;
		*(normalsp++) = normal2;
		*(normalsp++) = normal1;
		*(normalsp++) = normal2;

		*(colorsp++) = color;
		*(colorsp++) = color;
		*(colorsp++) = color;
		*(colorsp++) = color;
		*(colorsp++) = color;
		*(colorsp++) = color;
		*(colorsp++) = color;
		*(colorsp++) = color;

		*indicesp++ = index_offset;
		*indicesp++ = index_offset + 2;
		*indicesp++ = index_offset + 4;

		*indicesp++ = index_offset + 2;
		*indicesp++ = index_offset + 6;
		*indicesp++ = index_offset + 4;

		*indicesp++ = index_offset + 1;
		*indicesp++ = index_offset + 5;
		*indicesp++ = index_offset + 3;

		*indicesp++ = index_offset + 3;
		*indicesp++ = index_offset + 5;
		*indicesp++ = index_offset + 7;
		index_offset += 8;
	}
}

U32 LLVOGrass::getPartitionType() const
{
	return LLViewerRegion::PARTITION_GRASS;
}

///////////////////////////////////////////////////////////////////////////////
// LLGrassPartition class (declared in llspatialpartition.h)
///////////////////////////////////////////////////////////////////////////////

LLGrassPartition::LLGrassPartition(LLViewerRegion* regionp)
:	LLSpatialPartition(LLDrawPoolAlpha::VERTEX_DATA_MASK |
					   LLVertexBuffer::MAP_TEXTURE_INDEX,
					   true, regionp)
{
	mDrawableType = LLPipeline::RENDER_TYPE_GRASS;
	mPartitionType = LLViewerRegion::PARTITION_GRASS;
	mLODPeriod = 16;
	mDepthMask = true;
	mSlopRatio = 0.1f;
	mRenderPass = LLRenderPass::PASS_GRASS;
}

//virtual
void LLGrassPartition::addGeometryCount(LLSpatialGroup* group,
										U32& vertex_count,
										U32& index_count)
{
	mFaceList.clear();

	for (LLSpatialGroup::element_iter i = group->getDataBegin();
		 i != group->getDataEnd(); ++i)
	{
		LLDrawable* drawablep = (LLDrawable*)(*i)->getDrawable();
		if (!drawablep || drawablep->isDead())
		{
			continue;
		}

		LLViewerObject* vobj = drawablep->getVObj().get();
		if (!vobj)	// Paranoia
		{
			llwarns_once << "NULL viewer object for drawable " << std::hex
						 << drawablep << std::dec << llendl;
			continue;
		}

		LLAlphaObject* obj = vobj->asAlphaObject();
		if (!obj)
		{
			llwarns_once << "Not an alpha object for drawable " << std::hex
						 << drawablep << std::dec << llendl;
			continue;
		}

		obj->mDepth = 0.f;

		U32 count = 0;
		for (S32 j = 0; j < drawablep->getNumFaces(); ++j)
		{
			drawablep->updateFaceSize(j);

			LLFace* facep = drawablep->getFace(j);
			if (!facep || !facep->hasGeometry())
			{
				continue;
			}

			if (facep->getGeomCount() + vertex_count <= 65536)
			{
				++count;
				facep->mDistance = (facep->mCenterLocal -
									gViewerCamera.getOrigin()) *
								   gViewerCamera.getAtAxis();
				obj->mDepth += facep->mDistance;

				mFaceList.push_back(facep);
				vertex_count += facep->getGeomCount();
				index_count += facep->getIndicesCount();
				llassert(facep->getIndicesCount() < 65536);
			}
			else
			{
				facep->clearVertexBuffer();
			}
		}

		obj->mDepth /= count;
	}
}

//virtual
void LLGrassPartition::getGeometry(LLSpatialGroup* group)
{
	LL_FAST_TIMER(FTM_REBUILD_GRASS_VB);

	std::sort(mFaceList.begin(), mFaceList.end(),
			  LLFace::CompareDistanceGreater());

	U32 index_count = 0;
	U32 vertex_count = 0;

	group->clearDrawMap();

	LLVertexBuffer* buffer = group->mVertexBuffer;
	if (!buffer) return;	// Paranoia

	LLStrider<U16> indicesp;
	LLStrider<LLVector4a> verticesp;
	LLStrider<LLVector3> normalsp;
	LLStrider<LLVector2> texcoordsp;
	LLStrider<LLColor4U> colorsp;

	if (!buffer->getVertexStrider(verticesp) ||
		!buffer->getNormalStrider(normalsp) ||
		!buffer->getColorStrider(colorsp) ||
		!buffer->getTexCoord0Strider(texcoordsp) ||
		!buffer->getIndexStrider(indicesp))
	{
		return;
	}

	LLSpatialGroup::drawmap_elem_t& draw_vec = group->mDrawMap[mRenderPass];

	for (U32 i = 0, count = mFaceList.size(); i < count; ++i)
	{
		LLFace* facep = mFaceList[i];

		LLViewerObject* vobj = facep->getViewerObject();
		if (!vobj)	// Paranoia
		{
			llwarns_once << "NULL viewer object for face " << std::hex << facep
						 << std::dec << llendl;
			continue;
		}

		LLAlphaObject* object = vobj->asAlphaObject();
		if (!object)
		{
			llwarns_once << "Not an alpha object for face " << std::hex
						 << facep << std::dec << llendl;
			continue;
		}

		facep->setGeomIndex(vertex_count);
		facep->setIndicesIndex(index_count);
		facep->setVertexBuffer(buffer);
		facep->setPoolType(LLDrawPool::POOL_ALPHA);

		// Dummy parameter (unused by this implementation)
		LLStrider<LLColor4U> emissivep;
		object->getGeometry(facep->getTEOffset(), verticesp, normalsp,
							texcoordsp, colorsp, emissivep, indicesp);

		vertex_count += facep->getGeomCount();
		index_count += facep->getIndicesCount();

		S32 idx = draw_vec.size() - 1;

		bool fullbright = facep->isState(LLFace::FULLBRIGHT);
		F32 vsize = facep->getVirtualSize();

		U16 geomcount = facep->getGeomCount();
		U32 indicescount = facep->getIndicesCount();
		if (idx >= 0 &&
			draw_vec[idx]->mEnd == facep->getGeomIndex() - 1 &&
			draw_vec[idx]->mTexture == facep->getTexture() &&
			(U32)(draw_vec[idx]->mEnd - draw_vec[idx]->mStart + geomcount) <=
				(U32)gGLManager.mGLMaxVertexRange &&
#if 0
			draw_vec[idx]->mCount + indicescount <=
				(U32)gGLManager.mGLMaxIndexRange &&
#endif
			draw_vec[idx]->mEnd - draw_vec[idx]->mStart + geomcount < 4096 &&
			draw_vec[idx]->mFullbright == fullbright)
		{
			draw_vec[idx]->mCount += indicescount;
			draw_vec[idx]->mEnd += geomcount;
			draw_vec[idx]->mVSize = llmax(draw_vec[idx]->mVSize, vsize);
		}
		else
		{
			U32 start = facep->getGeomIndex();
			U32 end = start + geomcount - 1;
			U32 offset = facep->getIndicesStart();
			LLDrawInfo* info = new LLDrawInfo(start, end, indicescount, offset,
											  facep->getTexture(), buffer,
											  fullbright);
			const LLVector4a* exts = group->getObjectExtents();
			info->mExtents[0] = exts[0];
			info->mExtents[1] = exts[1];
			info->mVSize = vsize;
			draw_vec.push_back(info);
			// For alpha sorting
			facep->setDrawInfo(info);
		}
	}

	buffer->unmapBuffer();
	mFaceList.clear();
}

//virtual
void LLVOGrass::updateDrawable(bool force_damped)
{
	// Force an immediate rebuild on any update
	if (mDrawable.notNull())
	{
		mDrawable->updateXform(true);
		gPipeline.markRebuild(mDrawable);
	}
	clearChanged(SHIFTED);
}

//virtual
bool LLVOGrass::lineSegmentIntersect(const LLVector4a& start,
									 const LLVector4a& end,
									 S32 face,
									 bool pick_transparent,
									 bool pick_rigged,
									 S32* face_hitp,
									 LLVector4a* intersection,
									 LLVector2* tex_coord,
									 LLVector4a* normal,
									 LLVector4a* tangent)

{
	bool ret = false;
	if (!mCanSelect || mDrawable.isNull() || mDrawable->isDead() ||
		!gPipeline.hasRenderType(mDrawable->getRenderType()))
	{
		return false;
	}

	LLVector4a dir;
	dir.setSub(end, start);

	mPatch = mRegionp->getLand().resolvePatchRegion(getPositionRegion());

	LLVector3 position;
	// Create random blades of grass with gaussian distribution
	F32 x,y,xf,yf,dzx,dzy;

	LLColor4U color(255, 255, 255, 255);

	F32 width  = sSpeciesTable[mSpecies]->mBladeSizeX;
	F32 height = sSpeciesTable[mSpecies]->mBladeSizeY;

	LLVector2 tc[4];
	LLVector3 v[4];

	F32 closest_t = 1.f;

	LLVector3 origin_agent = mRegionp->getOriginAgent();

	for (S32 i = 0;  i < mNumBlades; ++i)
	{
		x   = exp_x[i] * mScale.mV[VX];
		y   = exp_y[i] * mScale.mV[VY];
		xf  = rot_x[i] * GRASS_BLADE_BASE * width * w_mod[i];
		yf  = rot_y[i] * GRASS_BLADE_BASE * width * w_mod[i];
		dzx = dz_x [i];
		dzy = dz_y [i];

		LLVector3 v1, v2, v3;
		F32 blade_height = GRASS_BLADE_HEIGHT * height * w_mod[i];

		tc[0] = LLVector2(0, 0);
		tc[1] = LLVector2(0, 0.98f);
		tc[2] = LLVector2(1, 0);
		tc[3] = LLVector2(1, 0.98f);

		position.mV[0] = mPosition.mV[VX] + x + xf;
		position.mV[1] = mPosition.mV[VY] + y + yf;
		position.mV[2] = mRegionp->getLand().resolveHeightRegion(position);
		v[0] = v1 = position + origin_agent;

		position.mV[0] += dzx;
		position.mV[1] += dzy;
		position.mV[2] += blade_height;
		v[1] = v2 = position + origin_agent;

		position.mV[0] = mPosition.mV[VX] + x - xf;
		position.mV[1] = mPosition.mV[VY] + y - xf;
		position.mV[2] = mRegionp->getLand().resolveHeightRegion(position);
		v[2] = v3 = position + origin_agent;

		LLVector3 normal1 = (v1 - v2) % (v2 - v3);
		normal1.normalize();

		position.mV[0] += dzx;
		position.mV[1] += dzy;
		position.mV[2] += blade_height;
		v[3] = v1 = position + origin_agent;

		F32 a, b, t;

		bool hit = false;

		U32 idx0 = 0, idx1 = 0, idx2 = 0;

		LLVector4a v0a, v1a, v2a, v3a;
		v0a.load3(v[0].mV);
		v1a.load3(v[1].mV);
		v2a.load3(v[2].mV);
		v3a.load3(v[3].mV);

		if (LLTriangleRayIntersect(v0a, v1a, v2a, start, dir, a, b, t))
		{
			hit = true;
			idx0 = 0; idx1 = 1; idx2 = 2;
		}
		else if (LLTriangleRayIntersect(v1a, v3a, v2a, start, dir, a, b, t))
		{
			hit = true;
			idx0 = 1; idx1 = 3; idx2 = 2;
		}
		else if (LLTriangleRayIntersect(v2a, v1a, v0a, start, dir, a, b, t))
		{
			normal1 = -normal1;
			hit = true;
			idx0 = 2; idx1 = 1; idx2 = 0;
		}
		else if (LLTriangleRayIntersect(v2a, v3a, v1a, start, dir, a, b, t))
		{
			normal1 = -normal1;
			hit = true;
			idx0 = 2; idx1 = 3; idx2 = 1;
		}

		if (hit)
		{
			if (t >= 0.f && t <= 1.f && t < closest_t)
			{
				LLVector2 hit_tc = ((1.f - a - b) * tc[idx0] + a * tc[idx1] +
									b * tc[idx2]);
				if (pick_transparent || getTEImage(0)->getMask(hit_tc))
				{
					closest_t = t;
					if (intersection != NULL)
					{
						dir.mul(closest_t);
						intersection->setAdd(start, dir);
					}

					if (tex_coord != NULL)
					{
						*tex_coord = hit_tc;
					}

					if (normal != NULL)
					{
						normal->load3(normal1.mV);
					}
					ret = true;
				}
			}
		}
	}

	return ret;
}

void LLVOGrass::generateSilhouetteVertices(std::vector<LLVector3>& vertices,
										   std::vector<LLVector3>& normals,
										   const LLVector3& obj_cam_vec,
										   const LLMatrix4& mat,
										   const LLMatrix3& norm_mat)
{
	vertices.clear();
	normals.clear();

	F32 width = sSpeciesTable[mSpecies]->mBladeSizeX;
	F32 height = sSpeciesTable[mSpecies]->mBladeSizeY;

	LLVector3 origin_agent = mRegionp->getOriginAgent();

	for (S32 i = 0; i < mNumBlades; ++i)
	{
		F32 x   = exp_x[i] * mScale.mV[VX];
		F32 y   = exp_y[i] * mScale.mV[VY];
		F32 xf  = rot_x[i] * GRASS_BLADE_BASE * width * w_mod[i];
		F32 yf  = rot_y[i] * GRASS_BLADE_BASE * width * w_mod[i];
		F32 dzx = dz_x [i];
		F32 dzy = dz_y [i];

		F32 blade_height = GRASS_BLADE_HEIGHT * height * w_mod[i];

		LLVector3 position1;

		position1.mV[0] = mPosition.mV[VX] + x + xf;
		position1.mV[1] = mPosition.mV[VY] + y + yf;
		position1.mV[2] = mRegionp->getLand().resolveHeightRegion(position1);

		LLVector3 position2 = position1;

		position2.mV[0] += dzx;
		position2.mV[1] += dzy;
		position2.mV[2] += blade_height;

		LLVector3 position3;

		position3.mV[0] = mPosition.mV[VX] + x - xf;
		position3.mV[1] = mPosition.mV[VY] + y - xf;
		position3.mV[2] = mRegionp->getLand().resolveHeightRegion(position3);

		LLVector3 position4 = position3;

		position4.mV[0] += dzx;
		position4.mV[1] += dzy;
		position4.mV[2] += blade_height;

		LLVector3 normal = (position1 - position2) % (position2 - position3);
		normal.normalize();

		vertices.emplace_back(position1 + origin_agent);
		normals.emplace_back(normal);
		vertices.emplace_back(position2 + origin_agent);
		normals.emplace_back(normal);

		vertices.emplace_back(position2 + origin_agent);
		normals.emplace_back(normal);
		vertices.emplace_back(position4 + origin_agent);
		normals.emplace_back(normal);

		vertices.emplace_back(position4 + origin_agent);
		normals.emplace_back(normal);
		vertices.emplace_back(position3 + origin_agent);
		normals.emplace_back(normal);

		vertices.emplace_back(position3 + origin_agent);
		normals.emplace_back(normal);
		vertices.emplace_back(position1 + origin_agent);
		normals.emplace_back(normal);
	}
}

void LLVOGrass::generateSilhouette(LLSelectNode* nodep)
{
	generateSilhouetteVertices(nodep->mSilhouetteVertices, nodep->mSilhouetteNormals,
							   LLVector3::zero, LLMatrix4(), LLMatrix3());

	nodep->mSilhouetteGenerated = true;
}
