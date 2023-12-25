/**
 * @file lllocalgltfmaterials.cpp
 * @brief LLLocalGLTFMaterial and HBFloaterLocalMaterial classes implementation
 *
 * $LicenseInfo:firstyear=2022&license=viewergpl$
 *
 * Copyright (c) 2022, Linden Research, Inc. (LLLocalGLTFMaterial)
 * Copyright (c) 2023, Henri Beauchamp. (HBFloaterLocalMaterial)
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

#include <time.h>
#include <ctime>

#include "lllocalgltfmaterials.h"

#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "lldir.h"
#include "llimage.h"
#include "llnotifications.h"
#include "llscrolllistctrl.h"
#include "lltextureentry.h"
#include "lluictrlfactory.h"

#include "llinventoryicon.h"
#include "llmaterialmgr.h"
#include "llpreviewmaterial.h"
#include "lltinygltfhelper.h"
#include "llviewertexture.h"

constexpr F32 LL_LOCAL_TIMER_HEARTBEAT = 3.f;
constexpr U32 LL_LOCAL_UPDATE_RETRIES = 5;

///////////////////////////////////////////////////////////////////////////////
// LLLocalGLTFMaterial class
///////////////////////////////////////////////////////////////////////////////

// Static members
LLLocalGLTFMaterial::list_t LLLocalGLTFMaterial::sMaterialList;
S32 LLLocalGLTFMaterial::sMaterialsListVersion = 0;
LLLocalGLTFMaterialTimer LLLocalGLTFMaterial::sTimer;

LLLocalGLTFMaterial::LLLocalGLTFMaterial(const std::string& fname, S32 index)
:	mFilename(fname),
	mLastModified(0),
	mLinkStatus(LS_ON),
	mUpdateRetries(LL_LOCAL_UPDATE_RETRIES),
	mMaterialIndex(index)
{
	mTrackingID.generate();
	if (!gDirUtilp)	// Paranoia
	{
		return;
	}
	mShortName = gDirUtilp->getBaseFileName(fname, true);
	std::string ext = gDirUtilp->getExtension(fname);
	if (ext == "gltf")
	{
		mExtension = ET_MATERIAL_GLTF;
	}
	else if (ext == "glb")
	{
		mExtension = ET_MATERIAL_GLB;
	}
	else
	{
		llwarns << "Not a valid file extension for GTLF material file: "
				<< fname << " - Aborted." << llendl;
	}
}

//static
void LLLocalGLTFMaterial::cleanupClass()
{
	sMaterialList.clear();
}

bool LLLocalGLTFMaterial::updateSelf()
{
	if (mLinkStatus != LS_ON)
	{
		return false;
	}
	if (!LLFile::exists(mFilename))
	{
		mLinkStatus = LS_BROKEN;
		LLSD args;
		args["FNAME"] = mFilename;
		gNotifications.add("LocalBitmapsUpdateFileNotFound", args);
		return false;
	}

	// Verifying that the file has indeed been modified
	time_t new_last_modified = LLFile::lastModidied(mFilename);
	if (mLastModified == new_last_modified)
	{
		return false;
	}

	if (loadMaterial())
	{
		// Decode is successful, we can safely proceed.
		if (mWorldID.isNull())
		{
			mWorldID.generate();
		}
		mLastModified = new_last_modified;

		// addMaterial() will replace material with a new pointer if value
		// already exists but we are reusing existing pointer, so it should
		// add only.
		gGLTFMaterialList.addMaterial(mWorldID, this);

		mUpdateRetries = LL_LOCAL_UPDATE_RETRIES;

		for (LLTextureEntry* entry : mTextureEntries)
		{
			// Normally a change in applied material id is supposed to drop
			// overrides thus reset material, but local materials currently
			// reuse their existing asset id, and purpose is to preview how
			// material will work in-world, overrides included, so do an
			// override to render update instead.
			LLGLTFMaterial* override_mat = entry->getGLTFMaterialOverride();
			if (override_mat)
			{
				// Do not create a new material, reuse existing pointer
				LLFetchedGLTFMaterial* render_mat =
					(LLFetchedGLTFMaterial*)entry->getGLTFRenderMaterial();
				if (render_mat)
				{
					*render_mat = *this;
					render_mat->applyOverride(*override_mat);
				}
			}
		}

		return true;
	}

	// If decoding failed, we get here and it will attempt to decode it in the
	// next cycles/ until mUpdateRetries runs out. this is done because some
	// software lock the material while writing to it
	if (mUpdateRetries)
	{
		--mUpdateRetries;
	}
	else
	{
		mLinkStatus = LS_BROKEN;
		LLSD args;
		args["FNAME"] = mFilename;
		args["NRETRIES"] = LL_LOCAL_UPDATE_RETRIES;
		gNotifications.add("LocalBitmapsUpdateFailedFinal", args);
	}
	return false;
}

bool LLLocalGLTFMaterial::loadMaterial()
{
	if (mExtension != ET_MATERIAL_GLTF && mExtension != ET_MATERIAL_GLB)
	{
		mLinkStatus = LS_BROKEN;
		return false;
	}

	std::string filename_lc = mFilename;
	LLStringUtil::toLower(filename_lc);
	tinygltf::Model model;
	if (!LLTinyGLTFHelper::loadModel(mFilename, model))
	{
		return false;
	}

	// Might be a good idea to make these textures into local textures
	std::string mat_name;
	if (!LLTinyGLTFHelper::getMaterialFromModel(mFilename, model,
												mMaterialIndex, this,
												mat_name))
	{
		return false;
	}
	if (!mat_name.empty())
	{
		mShortName = gDirUtilp->getBaseFileName(filename_lc, true) + " (" +
					 mat_name + ")";
	}
	return true;
}

S32 LLLocalGLTFMaterial::addUnit(const std::string& filename)
{
	tinygltf::Model model;
	LLTinyGLTFHelper::loadModel(filename, model);

	S32 materials_in_file = model.materials.size();
	if (materials_in_file <= 0)
	{
		return 0;
	}

	S32 loaded_materials = 0;
	for (S32 i = 0; i < materials_in_file; ++i)
	{
		// *TODO: this is rather inefficient: files will be spammed with
		// separate loads and date checks. Find a way to improve this. Maybe
		// doUpdates() should be checking individual files.
		LLPointer<LLLocalGLTFMaterial> matp =
			new LLLocalGLTFMaterial(filename, i);
		// Load material from file
		if (matp && matp->updateSelf())
		{
			sMaterialList.emplace_back(matp);
			++loaded_materials;
		}
		else
		{
			matp = NULL;
			LLSD args;
			args["FNAME"] = filename;
			gNotifications.add("LocalGLTFVerifyFail", args);
		}
	}
	return loaded_materials;
}

//static
void LLLocalGLTFMaterial::addUnitsCallback(HBFileSelector::ELoadFilter type,
										   std::deque<std::string>& files,
										   void*)
{
	bool updated = false;

	std::string filename;
	while (!files.empty())
	{
		filename = files.front();
		files.pop_front();
		if (!filename.empty())
		{
			sTimer.stopTimer();
			if (addUnit(filename))
			{
				updated = true;
			}
			sTimer.startTimer();
		}
	}

	if (updated)
	{
		++sMaterialsListVersion;
	}
}

//static
void LLLocalGLTFMaterial::addUnits()
{
	HBFileSelector::loadFiles(HBFileSelector::FFLOAD_GLTF, addUnitsCallback);
}

//static
void LLLocalGLTFMaterial::delUnit(const LLUUID& tracking_id)
{
	bool updated = false;

	for (list_t::iterator iter = sMaterialList.begin(),
						  end = sMaterialList.end();
		 iter != end; )
	{
		list_t::iterator curiter = iter++;
		LLLocalGLTFMaterial* matp = *curiter;
		if (matp->getTrackingID() == tracking_id)
		{
			// std::list:erase() preserves all iterators but curiter
			sMaterialList.erase(curiter);
			delete matp;
			updated = true;
		}
	}

	if (updated)
	{
		++sMaterialsListVersion;
	}
}

//static
const LLUUID& LLLocalGLTFMaterial::getWorldID(const LLUUID& tracking_id)
{
	for (list_t::const_iterator it = sMaterialList.begin(),
								end = sMaterialList.end();
		 it != end; ++it)
	{
		const LLLocalGLTFMaterial* matp = (*it).get();
		if (matp && matp->getTrackingID() == tracking_id)
		{
			return matp->getWorldID();
		}
	}
	return LLUUID::null;
}

//static
bool LLLocalGLTFMaterial::isLocal(const LLUUID& world_id)
{
	for (list_t::const_iterator it = sMaterialList.begin(),
								end = sMaterialList.end();
		 it != end; ++it)
	{
		const LLLocalGLTFMaterial* matp = (*it).get();
		if (matp && matp->getWorldID() == world_id)
		{
			return true;
		}
	}
	return false;
}

//static
const std::string& LLLocalGLTFMaterial::getFilenameAndIndex(const LLUUID& tid,
															S32& index)
{
	for (list_t::const_iterator it = sMaterialList.begin(),
								end = sMaterialList.end();
		 it != end; ++it)
	{
		const LLLocalGLTFMaterial* matp = (*it).get();
		if (matp && matp->getTrackingID() == tid)
		{
			index = matp->getIndexInFile();
			return matp->getFilename();
		}
	}
	// Not found.
	index = 0;
	return LLStringUtil::null;
}

//static
void LLLocalGLTFMaterial::doUpdates()
{
	// Preventing theoretical overlap in cases with huge number of loaded
	// images.
	sTimer.stopTimer();

	for (list_t::iterator it = sMaterialList.begin(),
						  end = sMaterialList.end();
		 it != end; ++it)
	{
	   	(*it)->updateSelf();
	}

	sTimer.startTimer();
}

///////////////////////////////////////////////////////////////////////////////
// LLLocalGLTFMaterialTimer class
///////////////////////////////////////////////////////////////////////////////

LLLocalGLTFMaterialTimer::LLLocalGLTFMaterialTimer()
:	LLEventTimer(LL_LOCAL_TIMER_HEARTBEAT)
{
}

void LLLocalGLTFMaterialTimer::startTimer()
{
	mEventTimer.start();
}

void LLLocalGLTFMaterialTimer::stopTimer()
{
	mEventTimer.stop();
}

bool LLLocalGLTFMaterialTimer::isRunning()
{
	return mEventTimer.getStarted();
}

bool LLLocalGLTFMaterialTimer::tick()
{
	LLLocalGLTFMaterial::doUpdates();
	return false;
}

///////////////////////////////////////////////////////////////////////////////
// HBFloaterLocalMaterial class
///////////////////////////////////////////////////////////////////////////////

HBFloaterLocalMaterial::HBFloaterLocalMaterial(LLView* ownerp, callback_t cb,
											   void* userdata)
:	mCallback(cb),
	mCallbackUserdata(userdata),
	mLastListVersion(-1)
{
	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_local_material.xml");
	// Note: at this point postBuild() has been called and returned.
	LLView* parentp = ownerp;
	// Search for our owner's parent floater and register as dependent of
	// it if found.
	while (parentp)
	{
		LLFloater* floaterp = parentp->asFloater();
		if (floaterp)
		{
			floaterp->addDependentFloater(this);
			break;
		}
		parentp = parentp->getParent();
	}
	if (!parentp)
	{
		// Place ourselves in a smart way, like preview floaters...
		S32 left, top;
		gFloaterViewp->getNewFloaterPosition(&left, &top);
		translate(left - getRect().mLeft, top - getRect().mTop);
		gFloaterViewp->adjustToFitScreen(this);
	}
}

//virtual
HBFloaterLocalMaterial::~HBFloaterLocalMaterial()
{
	gFocusMgr.releaseFocusIfNeeded(this);
}

//virtual
bool HBFloaterLocalMaterial::postBuild()
{
	childSetAction("add_btn", onBtnAdd, this);

	mRemoveButton = getChild<LLButton>("remove_btn");
	mRemoveButton->setClickedCallback(onBtnRemove, this);
	mRemoveButton->setEnabled(false);

	mUploadButton = getChild<LLButton>("upload_btn");
	mUploadButton->setClickedCallback(onBtnUpload, this);
	mUploadButton->setEnabled(false);

	mSelectButton = getChild<LLButton>("select_btn");
	mSelectButton->setClickedCallback(onBtnSelect, this);
	mSelectButton->setEnabled(false);

	childSetAction("cancel_btn", onBtnCancel, this);

	mMaterialsList = getChild<LLScrollListCtrl>("materials_list");
	mMaterialsList->setCommitCallback(onMaterialListCommit);
	mMaterialsList->setCallbackUserData(this);
	mMaterialsList->setCommitOnSelectionChange(true);

	mApplyImmediatelyCheck = getChild<LLCheckBoxCtrl>("apply_immediate_check");

	return true;
}

//virtual
void HBFloaterLocalMaterial::draw()
{
	if (mLastListVersion != LLLocalGLTFMaterial::getMaterialListVersion())
	{
		mLastListVersion = LLLocalGLTFMaterial::getMaterialListVersion();

		mMaterialsList->clearRows();
		mRemoveButton->setEnabled(false);
		mUploadButton->setEnabled(false);
		mSelectButton->setEnabled(false);

		const LLLocalGLTFMaterial::list_t& mats =
			LLLocalGLTFMaterial::getMaterialList();
		for (LLLocalGLTFMaterial::list_t::const_iterator it = mats.begin(),
														 end = mats.end();
			 it != end; ++it)
		{
			const LLLocalGLTFMaterial* matp = (*it).get();
			if (!matp) continue;	// Paranoia

			LLSD element;
			element["id"] = matp->getTrackingID();

			element["columns"][0]["column"] = "name";
			element["columns"][0]["type"]   = "text";
			element["columns"][0]["value"]  = matp->getShortName();
			mMaterialsList->addElement(element);
		}
	}

	LLFloater::draw();
}

//static
void HBFloaterLocalMaterial::onMaterialListCommit(LLUICtrl*, void* userdata)
{
	HBFloaterLocalMaterial* self = (HBFloaterLocalMaterial*)userdata;
	if (!self) return;

	uuid_vec_t ids = self->mMaterialsList->getSelectedIDs();
	S32 items = ids.size();
	bool has_selection = items > 0;
	bool has_one_selection = items == 1;

	// Enable/disable buttons based on selection
	self->mRemoveButton->setEnabled(has_selection);
	self->mUploadButton->setEnabled(has_selection);
	self->mSelectButton->setEnabled(has_one_selection);

	// When applying immediately, send the selected Id via the callback.
	if (has_one_selection && self->mApplyImmediatelyCheck->get())
	{
		const LLUUID& world_id = LLLocalGLTFMaterial::getWorldID(ids[0]);
		if (world_id.notNull())
		{
			self->mCallback(world_id, self->mCallbackUserdata);
		}
	}
}

//static
void HBFloaterLocalMaterial::onBtnSelect(void* userdata)
{
	HBFloaterLocalMaterial* self = (HBFloaterLocalMaterial*)userdata;
	if (!self) return;

	const LLUUID& world_id =
		LLLocalGLTFMaterial::getWorldID(self->mMaterialsList->getCurrentID());
	if (world_id.notNull())
	{
		self->mCallback(world_id, self->mCallbackUserdata);
	}
	self->close();
}

//static
void HBFloaterLocalMaterial::onBtnCancel(void* userdata)
{
	HBFloaterLocalMaterial* self = (HBFloaterLocalMaterial*)userdata;
	if (self)
	{
		// Send a cancel selection/revert material event.
		self->mCallback(LLUUID::null, self->mCallbackUserdata);
		self->close();
	}
}

//static
void HBFloaterLocalMaterial::onBtnAdd(void*)
{
	LLLocalGLTFMaterial::addUnits();
}

//static
void HBFloaterLocalMaterial::onBtnRemove(void* userdata)
{
	HBFloaterLocalMaterial* self = (HBFloaterLocalMaterial*)userdata;
	if (!self) return;

	uuid_vec_t ids = self->mMaterialsList->getSelectedIDs();
	if (ids.empty())
	{
		return;
	}

	for (U32 i = 0, count = ids.size(); i < count; ++i)
	{
		LLLocalGLTFMaterial::delUnit(ids[i]);
	}

	self->mRemoveButton->setEnabled(false);
	self->mUploadButton->setEnabled(false);
	self->mSelectButton->setEnabled(false);
}

//static
void HBFloaterLocalMaterial::onBtnUpload(void* userdata)
{
	HBFloaterLocalMaterial* self = (HBFloaterLocalMaterial*)userdata;
	if (!self)
	{
		return;
	}

	uuid_vec_t ids = self->mMaterialsList->getSelectedIDs();
	if (ids.empty())
	{
		return;
	}

	S32 index;
	std::string filename;
	for (U32 i = 0, count = ids.size(); i < count; ++i)
	{
		filename = LLLocalGLTFMaterial::getFilenameAndIndex(ids[i], index);
		LLPreviewMaterial::loadFromFile(filename, index);
	}
}
