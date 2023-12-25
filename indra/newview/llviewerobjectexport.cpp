/**
 * @file llviewerobjectexport.cpp
 * @authors Latif Khalifa (DAE exporter) / Apelsin & Lirusaito (wavefront
 *          exporter). Backported/adapted/optimized by Henri Beauchamp.
 *
 * $LicenseInfo:firstyear=2013&license=viewergpl$
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

#include "dae.h"
#include "dom/domCOLLADA.h"
#include "dom/domMatrix.h"

#include "llviewerobjectexport.h"

#include "llavatarappearancedefines.h"
#include "llbutton.h"
#include "llcallbacklist.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "lldir.h"
#include "llimagebmp.h"
#include "llimagepng.h"
#include "llimagej2c.h"
#include "llimagejpeg.h"
#include "llimagetga.h"
#include "llnotifications.h"
#include "lltextbox.h"
#include "lluictrlfactory.h"
#include "llvolume.h"

#include "llagent.h"
#include "llface.h"
#include "llfloatertools.h"
#include "hbobjectbackup.h"
#include "llselectmgr.h"
#include "lltexturecache.h"
#include "lltoolcomp.h"
#include "lltoolmgr.h"
#include "llviewercontrol.h"
#include "llviewerobject.h"
#include "llvoavatarself.h"
#include "llvovolume.h"

///////////////////////////////////////////////////////////////////////////////
// Utility class used by the two exporters
///////////////////////////////////////////////////////////////////////////////

class v4adapt
{
public:
	v4adapt(LLVector4a* vp)
	{
		mV4aStrider = vp;
	}

	LL_INLINE LLVector3 operator[](const size_t i)
	{
		return LLVector3((F32*)&mV4aStrider[i]);
	}

private:
	LLStrider<LLVector4a> mV4aStrider;
};

///////////////////////////////////////////////////////////////////////////////
// Texture cache read responder for the Collada exporter & floater
///////////////////////////////////////////////////////////////////////////////

class ExporterCacheReadResponder : public LLTextureCache::ReadResponder
{
protected:
	LOG_CLASS(ExporterCacheReadResponder);

public:
	ExporterCacheReadResponder(const LLUUID& id, LLImageFormatted* image,
							   std::string name, S32 img_type)
	:	mFormattedImage(image),
		mID(id),
		mName(name),
		mImageType(img_type)
	{
		setImage(image);
	}

	void setData(U8* data, S32 datasize, S32 imagesize, S32 imageformat,
				 bool imagelocal) override
	{
		if (imageformat == IMG_CODEC_TGA &&
			mFormattedImage->getCodec() == IMG_CODEC_J2C)
		{
			llwarns << "FAILED: texture " << mID
					<< " is formatted as TGA. Not saving." << llendl;
			mFormattedImage = NULL;
			mImageSize = 0;
			return;
		}

		if (mFormattedImage.notNull())
		{
			if (mFormattedImage->getCodec() != imageformat)
			{
				llwarns << "FAILED: texture " << mID
						<< " is formatted as " << mFormattedImage->getCodec()
						<< " while expecting " << imageformat
						<< ". Not saving." << llendl;
				mFormattedImage = NULL;
				mImageSize = 0;
				return;
			}
			mFormattedImage->appendData(data, datasize);
		}
		else
		{
			mFormattedImage = LLImageFormatted::createFromType(imageformat);
			mFormattedImage->setData(data, datasize);
		}
		mImageSize = imagesize;
		mImageLocal = imagelocal;
	}

	void started() override
	{
	}

	void completed(bool success) override
	{
		if (success && mFormattedImage.notNull() && mImageSize > 0)
		{
			bool ok = false;

			// If we are saving jpeg2000, no need to do anything, just write
			// to disk
			if (mImageType == LKDAESaver::ft_j2c)
			{
				mName += "." + mFormattedImage->getExtension();
				ok = mFormattedImage->save(mName);
			}
			// For other formats we need to decode first
			else if (mFormattedImage->updateData() &&
					 mFormattedImage->getWidth() > 0 &&
					 mFormattedImage->getHeight() > 0 &&
					 mFormattedImage->getComponents() > 0)
			{
				LLPointer<LLImageRaw> raw = new LLImageRaw;
				raw->resize(mFormattedImage->getWidth(),
							mFormattedImage->getHeight(),
							mFormattedImage->getComponents());

				if (mFormattedImage->decode(raw))
				{
					LLPointer<LLImageFormatted> img = NULL;
					switch (mImageType)
					{
						case LKDAESaver::ft_tga:
							img = new LLImageTGA;
							break;

						case LKDAESaver::ft_png:
							img = new LLImagePNG;
							break;

						case LKDAESaver::ft_bmp:
							img = new LLImageBMP;
							break;

						case LKDAESaver::ft_jpg:
							img = new LLImageJPEG;
					}

					if (img.notNull() && img->encode(raw))
					{
						mName += "." + img->getExtension();
						ok = img->save(mName);
					}
				}
			}

			if (ok)
			{
				llinfos << "Saved texture to " << mName << llendl;
			}
			else
			{
				llwarns << "FAILED to save texture " << mID << llendl;
			}
		}
		else
		{
			llwarns << "FAILED to save texture " << mID << llendl;
		}
	}

private:
	LLPointer<LLImageFormatted>	mFormattedImage;
	LLUUID						mID;
	S32							mImageType;
	std::string					mName;
};

///////////////////////////////////////////////////////////////////////////////
// Floater for the Collada exporter
///////////////////////////////////////////////////////////////////////////////

LKFloaterColladaExport::LKFloaterColladaExport(const LLSD&)
:	mTotal(0),
	mNumTextures(0),
	mNumExportableTextures(0)
{
	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_dae_export.xml");
	HBObjectBackup::setDefaultTextures();
}

//virtual
LKFloaterColladaExport::~LKFloaterColladaExport()
{
	gIdleCallbacks.deleteFunction(saveTexturesWorker, this);
}

//virtual
bool LKFloaterColladaExport::postBuild()
{
	addSelectedObjects();

	mTextureExportCheck = getChild<LLCheckBoxCtrl>("export_texture_check");
	mTextureExportCheck->setCommitCallback(onTextureExportCheck);
	mTextureExportCheck->setCallbackUserData(this);
	mTextureTypeCombo = getChild<LLComboBox>("texture_type_combo");
	const std::string& name = mTextureTypeCombo->getControlName();
	mTextureTypeCombo->setValue(gSavedSettings.getS32(name.c_str()));

	mExportButton = getChild<LLButton>("export_btn");
	mExportButton->setClickedCallback(onClickExport, this),

	mTitle = getString("export_progress");

	LLTextBox* text = getChild<LLTextBox>("object_name");
	text->setText(mObjectName);
	text = getChild<LLTextBox>("prims_count");
	text->setText(llformat("%d/%d", mSaver.mObjects.size(), mTotal));
	text = getChild<LLTextBox>("textures_count");
	text->setText(llformat("%d/%d", mNumExportableTextures, mNumTextures));

	onTextureExportCheck(mTextureExportCheck, this);

	return true;
}

void LKFloaterColladaExport::addSelectedObjects()
{
	if (mSaver.addSelectedObjects(mObjectName, mTotal))
	{
		mNumTextures = mSaver.mTextures.size();
		mNumExportableTextures = 0;
		for (LKDAESaver::string_list_t::const_iterator
				it = mSaver.mTextureNames.begin(),
				end = mSaver.mTextureNames.end();
			 it != end; ++it)
		{
			std::string name = *it;
			if (!name.empty())
			{
				++mNumExportableTextures;
			}
		}
	}
	else
	{
		gNotifications.add("ExportFailed");
		close();
	}
}

void LKFloaterColladaExport::updateTitleProgress()
{
	std::string title = llformat(mTitle.c_str(), mTexturesToSave.size());
	setTitle(title);
}

void LKFloaterColladaExport::saveDAE()
{
	if (mSaver.saveDAE(mFilename))
	{
		llinfos << "DAE file saved to: " << mFilename << llendl;
		gNotifications.add("ExportSuccessful");
	}
	else
	{
		llwarns << "Failed to save the DAE file to: " << mFilename
				<< llendl;
		gNotifications.add("ExportFailed");
	}
	close();
}

void LKFloaterColladaExport::saveTextures()
{
	llinfos << "Saving textures..." << llendl;

	mTexturesToSave.clear();
	for (S32 i = 0, count = mSaver.mTextures.size(); i < count; ++i)
	{
		if (!mSaver.mTextureNames[i].empty())
		{
			mTexturesToSave[mSaver.mTextures[i]] = mSaver.mTextureNames[i];
		}
	}

	S32 img_format = mTextureTypeCombo->getValue().asInteger();
	mSaver.mImageFormat = LKDAESaver::image_format_ext[img_format];

	constexpr F32 TEXTURE_DOWNLOAD_TIMEOUT = 60.f;
	mTimer.setTimerExpirySec(TEXTURE_DOWNLOAD_TIMEOUT);
	mTimer.start();

	updateTitleProgress();

	gIdleCallbacks.addFunction(saveTexturesWorker, this);
}

//static
void LKFloaterColladaExport::onTextureExportCheck(LLUICtrl* ctrl, void* data)
{
	LKFloaterColladaExport* self = (LKFloaterColladaExport*)data;
	if (self && ctrl)
	{
		self->mTextureTypeCombo->setEnabled(ctrl->getValue().asBoolean());
	}
}

//static
void LKFloaterColladaExport::onClickExport(void* data)
{
	LKFloaterColladaExport* self = (LKFloaterColladaExport*)data;
	if (self)
	{
		std::string suggestion =
			LLDir::getScrubbedFileName(self->mObjectName) + ".dae";
		HBFileSelector::saveFile(HBFileSelector::FFSAVE_DAE, suggestion,
								 filePickerCallback, data);
	}
}

//static
void LKFloaterColladaExport::filePickerCallback(HBFileSelector::ESaveFilter type,
												std::string& filename,
												void* data)
{
	LKFloaterColladaExport* self = (LKFloaterColladaExport*)data;
	if (self && !filename.empty())
	{
		llinfos << "Saving: " << filename << llendl;
		self->mFilename = filename;
		self->mFolder = gDirUtilp->getDirName(filename) + LL_DIR_DELIM_STR;
		self->mExportButton->setEnabled(false);
		if (self->mTextureExportCheck->get())
		{
			self->saveTextures();
		}
		else
		{
			self->saveDAE();
		}
	}
}

//static
void LKFloaterColladaExport::saveTexturesWorker(void* data)
{
	LKFloaterColladaExport* self = (LKFloaterColladaExport*)data;
	if (self->mTexturesToSave.size() == 0)
	{
		llinfos << "Done saving textures" << llendl;
		self->updateTitleProgress();
		gIdleCallbacks.deleteFunction(saveTexturesWorker, self);
		self->mTimer.stop();
		self->saveDAE();
		return;
	}

	LLUUID id = self->mTexturesToSave.begin()->first;
	LLViewerTexture* imagep = LLViewerTextureManager::findTexture(id);
	if (!imagep)
	{
		self->mTexturesToSave.erase(id);
		self->updateTitleProgress();
		self->mTimer.reset();
	}
	else if (imagep->getDiscardLevel() == 0) // Image download is complete
	{
		llinfos << "Saving texture " << id << llendl;
		LLImageFormatted* img = new LLImageJ2C;
		S32 img_type = self->mTextureTypeCombo->getValue();
		std::string name = self->mFolder + self->mTexturesToSave[id];
		ExporterCacheReadResponder* responder =
			new ExporterCacheReadResponder(id, img, name, img_type);
		gTextureCachep->readFromCache(id, 0, 999999, responder);
		self->mTexturesToSave.erase(id);
		self->updateTitleProgress();
		self->mTimer.reset();
	}
	else if (self->mTimer.hasExpired())
	{
		llwarns << "Timed out downloading texture " << id << llendl;
		self->mTexturesToSave.erase(id);
		self->updateTitleProgress();
		self->mTimer.reset();
	}
}

///////////////////////////////////////////////////////////////////////////////
// Collada exporter
///////////////////////////////////////////////////////////////////////////////

const std::string LKDAESaver::image_format_ext[] =
{
	"tga", "png", "j2c", "bmp", "jpg"
};

void LKDAESaver::add(LLViewerObject* prim, const std::string& name)
{
	mObjects.emplace_back(prim, name);
}

void LKDAESaver::updateTextureInfo()
{
	mTextures.clear();
	mTextureNames.clear();

	for (obj_info_t::iterator it = mObjects.begin(), end = mObjects.end();
		 it != end; ++it)
	{
		LLViewerObject* obj = it->first;
		S32 num_faces = obj->getVolume()->getNumVolumeFaces();
		for (S32 f = 0; f < num_faces; ++f)
		{
			LLTextureEntry* te = obj->getTE(f);
			const LLUUID& id = te->getID();
			uuid_vec_t::iterator texend = mTextures.end();
			if (std::find(mTextures.begin(), texend, id) == texend)
			{
				mTextures.emplace_back(id);
				if (HBObjectBackup::validateAssetPerms(id))
				{
					mTextureNames.emplace_back(id.asString());
				}
				else
				{
					mTextureNames.emplace_back(LLStringUtil::null);
				}
			}
		}
	}
}

void LKDAESaver::addSource(daeElement* mesh, const char* src_id,
						   std::string params, const std::vector<F32>& vals)
{
	daeElement* source = mesh->add("source");
	source->setAttribute("id", src_id);
	daeElement* src_array = source->add("float_array");

	src_array->setAttribute("id", llformat("%s-array", src_id).c_str());
	src_array->setAttribute("count", llformat("%d", vals.size()).c_str());

	domFloat_array* float_array = (domFloat_array*)src_array;
	for (S32 i = 0, count = vals.size(); i < count; ++i)
	{
		float_array->getValue().append(vals[i]);
	}

	domAccessor* acc =
		daeSafeCast<domAccessor>(source->add("technique_common accessor"));
	acc->setSource(llformat("#%s-array", src_id).c_str());
	acc->setCount(vals.size() / params.size());
	acc->setStride(params.size());

	for (std::string::iterator it = params.begin(), end = params.end();
		 it != end; ++it)
	{
		domElement* px = acc->add("param");
		px->setAttribute("name", llformat("%c", *it).c_str());
		px->setAttribute("type", "float");
	}
}

void LKDAESaver::addPolygons(daeElement* mesh, const char* geom_id,
							 const char* mat_id, LLViewerObject* obj,
							 int_list_t* faces_to_include)
{
	domPolylist* polylist = daeSafeCast<domPolylist>(mesh->add("polylist"));
	polylist->setMaterial(mat_id);

	// Vertices semantic
	domInputLocalOffset* input =
		daeSafeCast<domInputLocalOffset>(polylist->add("input"));
	input->setSemantic("VERTEX");
	input->setOffset(0);
	input->setSource(llformat("#%s-vertices", geom_id).c_str());

	// Normals semantic
	input = daeSafeCast<domInputLocalOffset>(polylist->add("input"));
	input->setSemantic("NORMAL");
	input->setOffset(0);
	input->setSource(llformat("#%s-normals", geom_id).c_str());

	static LLCachedControl<bool> single_uv_map(gSavedSettings,
											   "DAEExportSingleUVMap");
	// UV semantic
	input = daeSafeCast<domInputLocalOffset>(polylist->add("input"));
	input->setSemantic("TEXCOORD");
	input->setOffset(0);
	if (single_uv_map)
	{
		input->setSource("#unified-map0");
	}
	else
	{
		input->setSource(llformat("#%s-map0", geom_id).c_str());
	}

	// Save indices
	domP* p = daeSafeCast<domP>(polylist->add("p"));
	domPolylist::domVcount* vcount =
		daeSafeCast<domPolylist::domVcount>(polylist->add("vcount"));
	S32 index_offset = 0;
	S32 num_tris = 0;
	for (S32 f = 0; f < obj->getVolume()->getNumVolumeFaces(); ++f)
	{
		if (skipFace(obj->getTE(f))) continue;

		const LLVolumeFace* face =
			(LLVolumeFace*)&obj->getVolume()->getVolumeFace(f);
		if (face &&
			(!faces_to_include ||
			 (std::find(faces_to_include->begin(), faces_to_include->end(),
						f) != faces_to_include->end())))
		{
			for (S32 i = 0; i < face->mNumIndices; ++i)
			{
				U16 index = index_offset + face->mIndices[i];
				(p->getValue()).append(index);
				if (i % 3 == 0)
				{
					(vcount->getValue()).append(3);
					++num_tris;
				}
			}
		}
		index_offset += face->mNumVertices;
	}
	polylist->setCount(num_tris);
}

void LKDAESaver::transformTexCoord(S32 num_vert, LLVector2* coord,
								   LLVector3* positions, LLVector3* normals,
								   LLTextureEntry* te, LLVector3 scale)
{
	F32 cosine = cosf(te->getRotation());
	F32 sine = sinf(te->getRotation());

	for (S32 ii = 0; ii < num_vert; ++ii)
	{
		if (LLTextureEntry::TEX_GEN_PLANAR == te->getTexGen())
		{
			LLVector3 normal = normals[ii];
			LLVector3 pos = positions[ii];
			LLVector3 binormal;
			F32 d = normal * LLVector3::x_axis;
			if (d >= 0.5f || d <= -0.5f)
			{
				binormal = LLVector3::y_axis;
				if (normal.mV[0] < 0.f)
				{
					binormal *= -1.f;
				}
			}
			else
			{
				binormal = LLVector3::x_axis;
				if (normal.mV[1] > 0.f)
				{
					binormal *= -1.f;
				}
			}
			LLVector3 tangent = binormal % normal;
			LLVector3 scaled_pos = pos.scaledVec(scale);
			coord[ii].mV[0] = 1.f + ((binormal * scaled_pos) * 2.f - 0.5f);
			coord[ii].mV[1] = -((tangent * scaled_pos) * 2.f - 0.5f);
		}

		F32 repeatU;
		F32 repeatV;
		te->getScale(&repeatU, &repeatV);
		F32 tX = coord[ii].mV[0] - 0.5f;
		F32 tY = coord[ii].mV[1] - 0.5f;

		F32 offsetU;
		F32 offsetV;
		te->getOffset(&offsetU, &offsetV);

		coord[ii].mV[0] = (tX * cosine + tY * sine) * repeatU + offsetU + 0.5f;
		coord[ii].mV[1] = (-tX * sine + tY * cosine) * repeatV + offsetV + 0.5f;
	}
}

bool LKDAESaver::saveDAE(std::string filename)
{
	mAllMaterials.clear();
	mTotalNumMaterials = 0;
	DAE dae;
	// First set the filename to save
	daeElement* root = dae.add(filename);

	// Obligatory elements in header
	daeElement* asset = root->add("asset");
	// Get ISO format time
	time_t rawtime;
	time(&rawtime);
	struct tm* utc_time = gmtime(&rawtime);
	std::string date = llformat("%04d-%02d-%02dT%02d:%02d:%02d",
								utc_time->tm_year + 1900, utc_time->tm_mon + 1,
								utc_time->tm_mday, utc_time->tm_hour,
								utc_time->tm_min, utc_time->tm_sec);
	daeElement* created = asset->add("created");
	created->setCharData(date);
	daeElement* modified = asset->add("modified");
	modified->setCharData(date);
	daeElement* unit = asset->add("unit");
	unit->setAttribute("name", "meter");
	unit->setAttribute("value", "1");
	daeElement* up_axis = asset->add("up_axis");
	up_axis->setCharData("Z_UP");

	// File creator
	daeElement* contributor = asset->add("contributor");
	std::string name;
	gAgent.getName(name);
	contributor->add("author")->setCharData(name);
	contributor->add("authoring_tool")->setCharData(gSecondLife +
													" Collada Export");

	daeElement* images = root->add("library_images");
	daeElement* geom_lib = root->add("library_geometries");
	daeElement* effects = root->add("library_effects");
	daeElement* materials = root->add("library_materials");
	daeElement* scene = root->add("library_visual_scenes visual_scene");
	scene->setAttribute("id", "Scene");
	scene->setAttribute("name", "Scene");

	if (gSavedSettings.getBool("DAEExportTextures"))
	{
		generateImagesSection(images);
	}

	bool apply_tex_coord = gSavedSettings.getBool("DAEExportTextureParams");
	bool consolidate = gSavedSettings.getBool("DAEExportConsolidateMaterials");
	bool single_uv_map = gSavedSettings.getBool("DAEExportSingleUVMap");
	S32 prim_nr = 0;
	std::string matname;
	for (obj_info_t::iterator it = mObjects.begin(), end = mObjects.end();
		 it != end; ++it)
	{
		LLViewerObject* obj = it->first;

		name = llformat("prim%d", prim_nr++);

		const char* geom_id = name.c_str();

		daeElement* geom = geom_lib->add("geometry");
		geom->setAttribute("id", llformat("%s-mesh", geom_id).c_str());
		daeElement* mesh = geom->add("mesh");

		std::vector<F32> position_data, normal_data, uv_data;

		S32 num_faces = obj->getVolume()->getNumVolumeFaces();
		for (S32 f = 0; f < num_faces; ++f)
		{
			if (skipFace(obj->getTE(f))) continue;

			const LLVolumeFace* face =
				(LLVolumeFace*)&obj->getVolume()->getVolumeFace(f);

			v4adapt verts(face->mPositions);
			v4adapt norms(face->mNormals);

			LLVector2* new_coord = NULL;

			if (apply_tex_coord)
			{
				new_coord = new LLVector2[face->mNumVertices];
				LLVector3* new_pos = new LLVector3[face->mNumVertices];
				LLVector3* new_norm = new LLVector3[face->mNumVertices];
				for (S32 i = 0; i < face->mNumVertices; ++i)
				{
					new_pos[i] = verts[i];
					new_norm[i] = norms[i];
					new_coord[i] = face->mTexCoords[i];
				}
				transformTexCoord(face->mNumVertices, new_coord, new_pos,
								  new_norm, obj->getTE(f), obj->getScale());
				delete[] new_pos;
				delete[] new_norm;
			}

			for (S32 i = 0, count = face->mNumVertices; i < count; ++i)
			{
				const LLVector3& v = verts[i];
				position_data.push_back(v.mV[VX]);
				position_data.push_back(v.mV[VY]);
				position_data.push_back(v.mV[VZ]);

				const LLVector3& n = norms[i];
				normal_data.push_back(n.mV[VX]);
				normal_data.push_back(n.mV[VY]);
				normal_data.push_back(n.mV[VZ]);

				const LLVector2& uv = apply_tex_coord ? new_coord[i]
													  : face->mTexCoords[i];
				uv_data.push_back(uv.mV[VX]);
				uv_data.push_back(uv.mV[VY]);
			}

			if (apply_tex_coord)
			{
				delete[] new_coord;
			}
		}

		addSource(mesh, llformat("%s-positions", geom_id).c_str(), "XYZ",
				  position_data);
		addSource(mesh, llformat("%s-normals", geom_id).c_str(), "XYZ",
				  normal_data);
		if (single_uv_map)
		{
			addSource(mesh, "unified-map0", "ST", uv_data);
		}
		else
		{
			addSource(mesh, llformat("%s-map0", geom_id).c_str(), "ST",
					  uv_data);
		}

		// Add the <vertices> element
		daeElement*	vert_node = mesh->add("vertices");
		vert_node->setAttribute("id",
								llformat("%s-vertices", geom_id).c_str());
		daeElement* vert_input = vert_node->add("input");
		vert_input->setAttribute("semantic", "POSITION");
		vert_input->setAttribute("source",
								 llformat("#%s-positions", geom_id).c_str());

		material_list_t obj_mats;
		getMaterials(obj, &obj_mats);

		// Add triangles
		if (consolidate)
		{
			for (U32 mat = 0; mat < obj_mats.size(); ++mat)
			{
				int_list_t faces;
				getFacesWithMaterial(obj, obj_mats[mat], &faces);
				matname = obj_mats[mat].mName + "-material";
				addPolygons(mesh, geom_id, matname.c_str(), obj, &faces);
			}
		}
		else
		{
			S32 mat_nr = 0;
			for (S32 f = 0; f < num_faces; ++f)
			{
				if (!skipFace(obj->getTE(f)))
				{
					int_list_t faces;
					faces.push_back(f);
					matname = obj_mats[mat_nr++].mName + "-material";
					addPolygons(mesh, geom_id, matname.c_str(), obj, &faces);
				}
			}
		}

		daeElement* node = scene->add("node");
		node->setAttribute("type", "NODE");
		node->setAttribute("id", geom_id);
		node->setAttribute("name", geom_id);

		// Set tranform matrix (node position, rotation and scale)
		domMatrix* matrix = (domMatrix*)node->add("matrix");
		LLXform srt;
		srt.setScale(obj->getScale());
		srt.setPosition(obj->getRenderPosition() + mOffset);
		srt.setRotation(obj->getRenderRotation());
		LLMatrix4 m4;
		srt.getLocalMat4(m4);
		for (S32 i = 0; i < 4; ++i)
		{
			for (S32 j = 0; j < 4; ++j)
			{
				(matrix->getValue()).append(m4.mMatrix[j][i]);
			}
		}

		// Geometry of the node
		daeElement* node_geom = node->add("instance_geometry");

		// Bind materials
		daeElement* tq = node_geom->add("bind_material technique_common");
		
		for (S32 i = 0, count = obj_mats.size(); i < count; ++i)
		{
			matname = obj_mats[i].mName + "-material";
			daeElement* mat = tq->add("instance_material");
			mat->setAttribute("symbol", matname.c_str());
			mat->setAttribute("target", ("#" + matname).c_str());
		}

		node_geom->setAttribute("url", llformat("#%s-mesh", geom_id).c_str());
	}

	// Effects (face texture, color, alpha)
	generateEffects(effects);

	// Materials
	for (S32 i = 0, count = mAllMaterials.size(); i < count; ++i)
	{
		daeElement* mat = materials->add("material");
		matname = mAllMaterials[i].mName;
		mat->setAttribute("id", (matname + "-material").c_str());
		daeElement* effect = mat->add("instance_effect");
		effect->setAttribute("url", ("#" + matname + "-fx").c_str());
	}

	root->add("scene instance_visual_scene")->setAttribute("url", "#Scene");

	return dae.writeAll();
}

bool LKDAESaver::skipFace(LLTextureEntry* te)
{
	static LLCachedControl<bool> no_trans(gSavedSettings,
										  "DAEExportSkipTransparent");
	return no_trans &&
		   (te->isTransparent() || te->getID() == gTextureTransparent);
}

LKDAESaver::MaterialInfo LKDAESaver::getMaterial(LLTextureEntry* te)
{
	static LLCachedControl<bool> consolidate(gSavedSettings,
											 "DAEExportConsolidateMaterials");
	if (consolidate)
	{
		for (S32 i = 0, count = mAllMaterials.size(); i < count; ++i)
		{
			if (mAllMaterials[i].matches(te))
			{
				return mAllMaterials[i];
			}
		}
	}

	std::string name = llformat("Material%d", mAllMaterials.size());
	mAllMaterials.emplace_back(te->getID(), te->getColor(), name);

	return mAllMaterials[mAllMaterials.size() - 1];
}

void LKDAESaver::getMaterials(LLViewerObject* obj, material_list_t* ret)
{
	static LLCachedControl<bool> consolidate(gSavedSettings,
											 "DAEExportConsolidateMaterials");
	S32 num_faces = obj->getVolume()->getNumVolumeFaces();
	for (S32 f = 0; f < num_faces; ++f)
	{
		LLTextureEntry* te = obj->getTE(f);
		if (!te || skipFace(te))
		{
			continue;
		}

		MaterialInfo mat = getMaterial(te);
		if (!consolidate ||
			std::find(ret->begin(), ret->end(), mat) == ret->end())
		{
			ret->emplace_back(mat);
		}
	}
}

void LKDAESaver::getFacesWithMaterial(LLViewerObject* obj,
									  MaterialInfo& mat, int_list_t* ret)
{
	S32 num_faces = obj->getVolume()->getNumVolumeFaces();
	for (S32 f = 0; f < num_faces; ++f)
	{
		if (mat == getMaterial(obj->getTE(f)))
		{
			ret->push_back(f);
		}
	}
}

void LKDAESaver::generateEffects(daeElement* effects)
{
	// Effects (face color, alpha)
	static LLCachedControl<bool> export_textures(gSavedSettings,
												 "DAEExportTextures");

	std::string dae_name;
	for (S32 mat = 0, count = mAllMaterials.size(); mat < count; ++mat)
	{
		LLColor4 color = mAllMaterials[mat].mColor;
		domEffect* effect = (domEffect*)effects->add("effect");
		effect->setId((mAllMaterials[mat].mName + "-fx").c_str());
		daeElement* profile = effect->add("profile_COMMON");

		if (export_textures)
		{
			LLUUID tex_id;
			S32 i = 0;
			S32 count = mTextures.size();
			for ( ; i < count; ++i)
			{
				if (mAllMaterials[mat].mTextureID == mTextures[i])
				{
					tex_id = mTextures[i];
					break;
				}
			}

			if (!tex_id.isNull() && !mTextureNames[i].empty())
			{
				dae_name = mTextureNames[i] + "_" + mImageFormat;
				daeElement* newparam = profile->add("newparam");
				newparam->setAttribute("sid", (dae_name + "-surface").c_str());
				daeElement* surface = newparam->add("surface");
				surface->setAttribute("type", "2D");
				surface->add("init_from")->setCharData(dae_name.c_str());
				newparam = profile->add("newparam");
				newparam->setAttribute("sid", (dae_name + "-sampler").c_str());
				newparam->add("sampler2D source")->setCharData((dae_name +
																"-surface").c_str());
			}
		}

		daeElement* t = profile->add("technique");
		t->setAttribute("sid", "common");
		domElement* phong = t->add("phong");
		domElement* diffuse = phong->add("diffuse");
		// Only one <color> or <texture> can appear inside diffuse element
		if (!dae_name.empty())
		{
			daeElement* tex = diffuse->add("texture");
			tex->setAttribute("texture", (dae_name + "-sampler").c_str());
			tex->setAttribute("texcoord", dae_name.c_str());
		}
		else
		{
			daeElement* diff_color = diffuse->add("color");
			diff_color->setAttribute("sid", "diffuse");
			diff_color->setCharData(llformat("%f %f %f %f", color.mV[0],
											 color.mV[1], color.mV[2],
											 color.mV[3]).c_str());
			phong->add("transparency float")->setCharData(llformat("%f",
																   color.mV[3]).c_str());
		}
	}
}

void LKDAESaver::generateImagesSection(daeElement* images)
{
	std::string dae_name;
	for (S32 i = 0, count = mTextureNames.size(); i < count; ++i)
	{
		std::string name = mTextureNames[i];
		if (name.empty()) continue;

		dae_name = name + "_" + mImageFormat;
		daeElement* image = images->add("image");
		image->setAttribute("id", dae_name.c_str());
		image->setAttribute("name", dae_name.c_str());
		image->add("init_from")->setCharData(LLURI::escape(name + "." +
														   mImageFormat));
	}
}

bool LKDAESaver::addSelectedObjects(std::string& root_name, U32& total)
{
	LLObjectSelectionHandle selection = gSelectMgr.getSelection();
	if (selection && selection->getFirstRootObject())
	{
		mOffset = -selection->getFirstRootObject()->getRenderPosition();
		root_name = selection->getFirstRootNode()->mName;

		total = 0;
		for (LLObjectSelection::iterator iter = selection->begin(),
										 end = selection->end();
			 iter != end; ++iter)
		{
			++total;
			LLSelectNode* node = *iter;
			if (node->getObject()->getVolume() &&
				HBObjectBackup::validateNode(node))
			{
				add(node->getObject(), node->mName);
			}
		}

		if (mObjects.empty())
		{
			return false;
		}

		updateTextureInfo();

		return true;
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////////
// Wavefront exporter
///////////////////////////////////////////////////////////////////////////////

// Same typedef as the protected one in ../llappearance/llavatarappearance.cpp
typedef std::vector<LLAvatarJoint*> avatar_joint_list_t;

ALWavefront::ALWavefront(vert_t v, tri_t t)
:	mVertices(v),
	mTriangles(t)
{
}

ALWavefront::ALWavefront(const LLVolumeFace* face, const LLXform* transform,
						 const LLXform* transform_normals)
{
	v4adapt verts(face->mPositions);
	for (S32 i = 0, count = face->mNumVertices; i < count; ++i)
	{
		LLVector3 v = verts[i];
		mVertices.emplace_back(v, face->mTexCoords[i]);
	}

	if (transform)
	{
		Transform(mVertices, transform);
	}

	v4adapt norms(face->mNormals);
	for (S32 i = 0, count = face->mNumVertices; i < count; ++i)
	{
		mNormals.push_back(norms[i]);
	}

	if (transform_normals)
	{
		Transform(mNormals, transform_normals);
	}

	for (S32 i = 0, count = face->mNumIndices / 3; i < count; ++i)
	{
		mTriangles.emplace_back(face->mIndices[i * 3],
								face->mIndices[i * 3 + 1],
								face->mIndices[i * 3 + 2]);
	}
}

ALWavefront::ALWavefront(LLFace* face, LLPolyMesh* mesh,
						 const LLXform* transform,
						 const LLXform* transform_normals)
{
	LLVertexBuffer* vb = face->getVertexBuffer();
	if (!mesh && !vb) return;

	LLStrider<LLVector3> getVerts;
	LLStrider<LLVector3> getNorms;
	LLStrider<LLVector2> getCoord;
	LLStrider<U16> getIndices;
	face->getGeometry(getVerts, getNorms, getCoord, getIndices);

	const S32 start = face->getGeomStart();
	const S32 end = start + (mesh ? mesh->getNumVertices()
								  : vb->getNumVerts()) - 1;
	for (S32 i = start; i <= end; ++i)
	{
		mVertices.emplace_back(getVerts[i], getCoord[i]);
	}

	if (transform)
	{
		Transform(mVertices, transform);
	}

	for (S32 i = start; i <= end; ++i)
	{
		mNormals.push_back(getNorms[i]);
	}

	if (transform_normals)
	{
		Transform(mNormals, transform_normals);
	}

	const S32 pcount = mesh ? mesh->getNumFaces() : vb->getNumIndices() / 3;
	const S32 offset = face->getIndicesStart();
	for (S32 i = 0; i < pcount; ++i)
	{
		mTriangles.emplace_back(getIndices[i * 3  + offset] + start,
								getIndices[i * 3 + 1 + offset] + start,
								getIndices[i * 3 + 2 + offset] + start);
	}
}

// recursive
void ALWavefront::Transform(vert_t& v, const LLXform* x)
{
	LLMatrix4 m;
	x->getLocalMat4(m);

	for (vert_t::iterator it = v.begin(), end = v.end(); it != end; ++it)
	{
		it->first = it->first * m;
	}

	if (const LLXform* xp = x->getParent())
	{
		Transform(v, xp);
	}
}

// Recursive
void ALWavefront::Transform(vec3_t& v, const LLXform* x)
{
	LLMatrix4 m;
	x->getLocalMat4(m);
	for (vec3_t::iterator it = v.begin(), end = v.end(); it != end; ++it)
	{
		*it = *it * m;
	}

	if (const LLXform* xp = x->getParent())
	{
		Transform(v, xp);
	}
}

void ALWavefrontSaver::add(const ALWavefront& obj)
{
	mWavefrontObjects.push_back(obj);
}

void ALWavefrontSaver::add(const LLVolume* vol, const LLXform* transform,
						   const LLXform* transform_normals)
{
	for (S32 i = 0, faces = vol->getNumVolumeFaces(); i < faces; ++i)
	{
		add(ALWavefront(&vol->getVolumeFace(i), transform, transform_normals));
	}
}

void ALWavefrontSaver::add(const LLViewerObject* some_vo)
{
	LLXform v_form;
	v_form.setScale(some_vo->getScale());
	v_form.setPosition(some_vo->getRenderPosition());
	v_form.setRotation(some_vo->getRenderRotation());

	LLXform normfix;
	normfix.setRotation(v_form.getRotation());		// Should work...
	add(some_vo->getVolume(), &v_form, &normfix);
}

#if LL_EXPORT_AVATAR_OBJ
bool ALWavefrontSaver::add(const LLVOAvatar* av_vo, bool with_attachments)
{
	mOffset = -av_vo->getRenderPosition();
	const avatar_joint_list_t vjv = const_cast<LLVOAvatar*>(av_vo)->getMeshLOD();
	for (avatar_joint_list_t::const_iterator it = vjv.begin(), end = vjv.end();
		 it != end; ++it)
	{
		const LLViewerJoint* vj = dynamic_cast<LLViewerJoint*>(*it);
		if (!vj || vj->mMeshParts.empty()) continue;

		// 0 = highest LOD
		LLViewerJointMesh* vjm = dynamic_cast<LLViewerJointMesh*>(vj->mMeshParts[0]);
		if (!vjm) continue;

		vjm->updateJointGeometry();
		LLFace* face = vjm->getFace();
		if (!face) continue;

		// Beware: this is a hack because LLFace has multiple LODs; 'pm'
		// supplies the right number of vertices and triangles !
		LLPolyMesh* pm = vjm->getMesh();
		if (!pm) continue;

		LLXform normfix;
		normfix.setRotation(pm->getRotation());

		// Special case for eyeballs
		static const std::string eyeLname =
			gAvatarAppDictp->getMeshEntry(LLAvatarAppearanceDefines::MESH_ID_EYEBALL_LEFT)->mName;
		static const std::string eyeRname =
			gAvatarAppDictp->getMeshEntry(LLAvatarAppearanceDefines::MESH_ID_EYEBALL_RIGHT)->mName;
		const std::string name = vj->getName();
		LL_DEBUGS("OBJExporter") << "Exporting joint: " << name << LL_ENDL;
		if (name == eyeLname || name == eyeRname)
		{
			LLXform lol;
			lol.setPosition(-mOffset);
			add(ALWavefront(face, pm, &lol, &normfix));
		}
		else
		{
			add(ALWavefront(face, pm, NULL, &normfix));
		}
	}

	if (!with_attachments) return true;

	// Open the edit tools floater so that we can select objects
	gFloaterToolsp->open();
	gToolMgr.setCurrentToolset(gBasicToolset);
	gFloaterToolsp->setEditTool(&gToolCompTranslate);

	struct ff final : public LLSelectedNodeFunctor
	{
		bool apply(LLSelectNode* node) override
		{
			if (!node->mValid)
			{
				llwarns_once << "Invalid extra data for node: " << node
							 << llendl;
			}
			return HBObjectBackup::validateNode(node);
		}
	} func;

	bool success = true;
	for (S32 i = 0, count = av_vo->mAttachedObjectsVector.size(); i < count;
		 ++i)
	{
		LLViewerObject* obj = av_vo->mAttachedObjectsVector[i].first;
		if (!obj || obj->isHUDAttachment()) continue;

		bool perm_ok = true;

		// Select our attachment
		gSelectMgr.selectObjectAndFamily(obj->getRootEdit());
		//gSelectMgr.getSelection()->ref();

		if (!gSelectMgr.getSelection()->applyToNodes(&func, false))
		{
			llwarns << "Incorrect permission to export attachment: "
					<< obj->getID() << llendl;
			success = perm_ok = false;
		}

		//gSelectMgr.getSelection()->unref();
		gSelectMgr.deselectAll();

		if (perm_ok)
		{
			LL_DEBUGS("OBJExporter") << "Exporting attachment: "
									 << obj->getID() << LL_ENDL;
			std::vector<LLViewerObject*> prims;
			obj->addThisAndAllChildren(prims);
			for (std::vector<LLViewerObject*>::iterator
					it3 = prims.begin(), end3 = prims.end();
				 it3 != end3; ++it3)
			{
				LLViewerObject* pobj = *it3;
				if (!pobj) continue;

				const LLVolume* vol = pobj->getVolume();
				if (!vol) continue;

				LLXform v_form;
				v_form.setScale(pobj->getScale());
				v_form.setPosition(pobj->getRenderPosition());
				v_form.setRotation(pobj->getRenderRotation());

				LLXform normfix;
				normfix.setRotation(v_form.getRotation());

				add(vol, &v_form, &normfix);
			}
		}
	}

	return success;
}
#endif

static bool write_or_bust(LLFILE* fp, const std::string outstring)
{
	const size_t size = outstring.length();
	if (fwrite(outstring.c_str(), 1, size, fp) != size)
	{
		llwarns << "ALWavefrontSaver::saveToFile(): short write" << llendl;
		return false;
	}
	return true;
}

bool ALWavefrontSaver::saveToFile(LLFILE* fp)
{
	if (!fp) return false;

	static LLCachedControl<bool> swap_yz(gSavedSettings, "OBJExportSwapYZ");

	S32 num = 0;
	S32 index = 0;
	for (std::vector<ALWavefront>::iterator iter = mWavefrontObjects.begin(),
											wend = mWavefrontObjects.end();
		 iter != wend; ++iter)
	{
		S32 count = 0;

		std::string name = iter->mName;
		if (name.empty())
		{
			name = llformat("%d", num++);
		}

		// Write Object
		if (!write_or_bust(fp, "o " + name + "\n"))
		{
			return false;
		}

		// Write vertices; swap axes if necessary
		const F64 xm = swap_yz ? -1.0 : 1.0;
		const S32 y = swap_yz ? 2 : 1;
		const S32 z = swap_yz ? 1 : 2;
		for (vert_t::iterator it = iter->mVertices.begin(),
							  end = iter->mVertices.end();
			 it != end; ++it)
		{
			++count;
			const LLVector3 v = it->first + mOffset;
			if (!write_or_bust(fp, llformat("v %f %f %f\n",
											v[0] * xm, v[y], v[z])))
			{
				return false;
			}
		}

		for (vec3_t::iterator it = iter->mNormals.begin(),
								   end = iter->mNormals.end();
			 it != end; ++it)
		{
			const LLVector3 n = *it;
			if (!write_or_bust(fp, llformat("vn %f %f %f\n",
											n[0] * xm, n[y], n[z])))
			{
				return false;
			}
		}

		for (vert_t::iterator it = iter->mVertices.begin(),
							  end = iter->mVertices.end();
			 it != end; ++it)
		{
			if (!write_or_bust(fp, llformat("vt %f %f\n",
											it->second[0], it->second[1])))
			{
				return false;
			}
		}

		// Write triangles
		for (tri_t::iterator it = iter->mTriangles.begin(),
							 end = iter->mTriangles.end();
			 it != end; ++it)
		{
			const S32 f1 = it->v0 + index + 1;
			const S32 f2 = it->v1 + index + 1;
			const S32 f3 = it->v2 + index + 1;
			if (!write_or_bust(fp, llformat("f %d/%d/%d %d/%d/%d %d/%d/%d\n",
											f1, f1, f1, f2, f2, f2,
											f3, f3, f3)))
			{
				return false;
			}
		}
		index += count;
	}

	return true;
}

//static
void ALWavefrontSaver::exportSelection()
{
	LLObjectSelectionHandle selection = gSelectMgr.getSelection();
	if (selection && selection->getFirstRootObject())
	{
		// Destroyed in callback
		ALWavefrontSaver* wfsaver = new ALWavefrontSaver;

		wfsaver->mOffset =
			-selection->getFirstRootObject()->getRenderPosition();

		S32 total = 0;
		S32 included = 0;
		for (LLObjectSelection::iterator iter = selection->begin(),
										 end = selection->end();
			 iter != end; ++iter)
		{
			++total;
			LLSelectNode* node = *iter;
			if (node && HBObjectBackup::validateNode(node))
			{
				++included;
				wfsaver->add(node->getObject());
			}
		}

		if (wfsaver->mWavefrontObjects.empty())
		{
			gNotifications.add("ExportFailed");
			delete wfsaver;
			return;
		}

		std::string suggestion = selection->getFirstRootNode()->mName;
		suggestion = LLDir::getScrubbedFileName(suggestion) + ".obj";
		if (total != included)
		{
			LLSD args;
			args["TOTAL"] = total;
			args["FAILED"] = total - included;
			gNotifications.add("WavefrontExportPartial", args, LLSD(),
							   boost::bind(&saveNotificationCallback, _1, _2,
										   wfsaver, suggestion));
		}
		else
		{
			saveOpenPicker(wfsaver, suggestion);
		}
	}
}

#if LL_EXPORT_AVATAR_OBJ
//static
void ALWavefrontSaver::exportAvatar(bool with_attachments)
{
	LLVOAvatar* avatar = (LLVOAvatar*)gAgentAvatarp;
	if (!avatar) return;

	ALWavefrontSaver* wfsaver = new ALWavefrontSaver; // Destroyed in callback
	bool full = wfsaver->add(avatar, with_attachments);
	if (wfsaver->mWavefrontObjects.empty())
	{
		gNotifications.add("ExportFailed");
		delete wfsaver;
		return;
	}

	std::string suggestion = avatar->getFullname(true);
	suggestion = LLDir::getScrubbedFileName(suggestion) + ".obj";
	if (full)
	{
		saveOpenPicker(wfsaver, suggestion);
	}
	else
	{
		gNotifications.add("WavefrontAvatarExportPartial", LLSD(), LLSD(),
						   boost::bind(&saveNotificationCallback, _1, _2,
									   wfsaver, suggestion));
	}
}
#endif

//static
void ALWavefrontSaver::saveNotificationCallback(const LLSD& notification,
												const LLSD& response,
												ALWavefrontSaver* wfsaver,
												std::string name)
{
	if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		saveOpenPicker(wfsaver, name);
	}
	else
	{
		delete wfsaver;
	}
}

//static
void ALWavefrontSaver::saveOpenPicker(ALWavefrontSaver* wfsaver,
									  std::string name)
{
	HBFileSelector::saveFile(HBFileSelector::FFSAVE_OBJ, name,
							 savePickerCallback, (void*)wfsaver);
}

//static
void ALWavefrontSaver::savePickerCallback(HBFileSelector::ESaveFilter type,
										  std::string& filename,
										  void* userdata)
{
	ALWavefrontSaver* wfsaver = (ALWavefrontSaver*)userdata;
	if (!wfsaver) return;

	if (userdata && !filename.empty())
	{
		if (LLFILE* fp = LLFile::open(filename, "wb"))
		{
			wfsaver->saveToFile(fp);
			llinfos << "OBJ file saved to: " << filename << llendl;
			gNotifications.add("ExportSuccessful");
			LLFile::close(fp);
		}
		else
		{
			llwarns << "Could not write to file: " << filename
					<< "- Export process failed." << llendl;
			gNotifications.add("ExportFailed");
		}
	}

	delete wfsaver;
}
