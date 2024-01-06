/**
 * @file llfloatermodelpreview.cpp
 * @brief LLFloaterModelPreview class implementation
 *
 * $LicenseInfo:firstyear=2004&license=viewergpl$
 *
 * Copyright (c) 2010, Linden Research, Inc.
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

#include "boost/algorithm/string.hpp"
#include "tinygltf/tiny_gltf.h"

#include "llfloatermodelpreview.h"

#include "llanimationstates.h"
#include "llbutton.h"
#include "llcallbacklist.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "llcorehttputil.h"
#include "lldaeloader.h"
#include "lldir.h"
#include "hbfileselector.h"
#include "llgltfloader.h"
#include "lliconctrl.h"
#include "llimagegl.h"
#include "lljoint.h"
#include "llmatrix4a.h"
#include "llmeshoptimizer.h"
#include "llnotifications.h"
#include "llrender.h"
#include "llscrolllistctrl.h"
#include "llsdserialize.h"
#include "llsdutil_math.h"
#include "llsliderctrl.h"
#include "llspinctrl.h"
#include "lltabcontainer.h"
#include "lltextbox.h"
#include "lltexteditor.h"
#include "lluictrlfactory.h"

// Do not move upwards because conflicting definitions in that header !
#include "glod/glod.h"

#include "llagent.h"
#include "lldrawable.h"
#include "llface.h"
#include "llgridmanager.h"
#include "llmanipscale.h"
#include "llpipeline.h"
#include "llskinningutil.h"
#include "lltoolmgr.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerobjectlist.h"
#include "llviewershadermgr.h"
#include "llviewerregion.h"
#include "llviewertexturelist.h"
#include "llviewerwindow.h"
#include "llvoavatarself.h"
#include "llweb.h"

//static
S32 LLFloaterModelPreview::sUploadAmount = 10;

static const LLColor4 PREVIEW_CANVAS_COL(0.169f, 0.169f, 0.169f, 1.f);
static const LLColor4 PREVIEW_EDGE_COL(0.4f, 0.4f, 0.4f, 1.0);
static const LLColor4 PREVIEW_BASE_COL(1.f, 1.f, 1.f, 1.f);
static const LLColor3 PREVIEW_BRIGHTNESS(0.9f, 0.9f, 0.9f);
static const LLColor4 PREVIEW_PHYS_EDGE_COL(0.f, 0.25f, 0.5f, 0.25f);
static const LLColor4 PREVIEW_PHYS_FILL_COL(0.f, 0.5f, 1.0f, 0.5f);
static const LLColor4 PREVIEW_DEG_EDGE_COL(1.f, 0.f, 0.f, 1.f);
static const LLColor4 PREVIEW_DEG_FILL_COL(1.f, 0.f, 0.f, 0.5f);
// Note: this name must match the name of the physics shape found in 
// app_settings/meshes/cube.dae
static const std::string DEFAULT_PHYSICS_MESH_NAME = "default_physics_shape";

constexpr F32 PREVIEW_DEG_EDGE_WIDTH = 3.f;
constexpr F32 PREVIEW_DEG_POINT_SIZE = 8.f;
constexpr F32 PREVIEW_ZOOM_LIMIT = 20.f;

constexpr U32 LIMIT_TRIANGLES = 0;

// "Retain%" decomp parameter has values from 0.0 to 1.0 by 0.01
// But according to the UI spec for upload model floater, this parameter
// should be represented by Retain spinner with values from 1 to 100 by 1.
// To achieve this, RETAIN_COEFFICIENT is used while creating spinner
// and when value is requested from spinner.
constexpr double RETAIN_COEFFICIENT = 100;

// "Cosine%" decomp parameter has values from 0.9 to 1 by 0.001
// But according to the UI spec for upload model floater, this parameter
// should be represented by Smooth combobox with only 10 values.
// So this const is used as a size of Smooth combobox list.
constexpr S32 SMOOTH_VALUES_NUMBER = 10;

// mCameraDistance. Also see: mCameraZoom
constexpr F32 SKIN_WEIGHT_CAMERA_DISTANCE = 16.f;

void drawBoxOutline(const LLVector3& pos, const LLVector3& size);

const std::string lod_name[NUM_LOD + 1] =
{
	"lowest",
	"low",
	"medium",
	"high",
	"Went off the end of the lod_name array."
};

const char* lod_triangles_name[NUM_LOD + 1] =
{
	"lowest_triangles",
	"low_triangles",
	"medium_triangles",
	"high_triangles",
	"Went off the end of the lod_triangles_name array."
};

const char* lod_vertices_name[NUM_LOD + 1] =
{
	"lowest_vertices",
	"low_vertices",
	"medium_vertices",
	"high_vertices",
	"Went off the end of the lod_vertices_name array."
};

const char* lod_status_name[NUM_LOD + 1] =
{
	"lowest_status",
	"low_status",
	"medium_status",
	"high_status",
	"Went off the end of the lod_status_name array."
};

const char* lod_icon_name[NUM_LOD + 1] =
{
	"status_icon_lowest",
	"status_icon_low",
	"status_icon_medium",
	"status_icon_high",
	"Went off the end of the lod_icon_name array."
};

const char* lod_status_image[NUM_LOD + 1] =
{
	"green_checkmark.png",
	"lag_status_warning.tga",
	"red_x.png",
	"Went off the end of the lod_status_image array."
};

const char* lod_label_name[NUM_LOD + 1] =
{
	"lowest_label",
	"low_label",
	"medium_label",
	"high_label",
	"Went off the end of the lod_label_name array."
};

static bool sHasGlodError = false;

static bool stop_gloderror(const char* comment = NULL)
{
	GLuint error = glodGetError();

	if (error != GLOD_NO_ERROR)
	{
		llwarns << "GLOD error " << std::hex << error << std::dec << ". "
				<< (comment ? comment : "") << llendl;
		// Do not set the error flag when this is just a "clear GLOD errors"
		// call (i.e. a call without any comment). HB
		sHasGlodError = comment != NULL;
		return true;
	}

	return false;
}

static void model_error(const char* message)
{
	LLSD args;
	args["MESSAGE"] = LLSD::String(message);
	gNotifications.add("GenericAlert", args);
}

static LLViewerFetchedTexture* bind_mat_diffuse_tex(const LLImportMaterial& mat)
{
	LLViewerFetchedTexture* texp =
		LLViewerTextureManager::getFetchedTexture(mat.getDiffuseMap(),
												  FTT_DEFAULT, true,
												  LLGLTexture::BOOST_PREVIEW);
	if (texp && texp->getDiscardLevel() > -1)
	{
		gGL.getTexUnit(0)->bind(texp);
		return texp;
	}

	return NULL;
}

static std::string strip_lod_suffix(std::string name)
{
	if (name.find("_LOD") != std::string::npos ||
		name.find("_PHYS") != std::string::npos)
	{
		return name.substr(0, name.rfind('_'));
	}
	return name;
}

static std::string get_lod_suffix(S32 lod)
{
	std::string suffix;
	switch (lod)
	{
		case LLModel::LOD_IMPOSTOR:	suffix = "_LOD0";	break;
		case LLModel::LOD_LOW:		suffix = "_LOD1";	break;
		case LLModel::LOD_MEDIUM:	suffix = "_LOD2";	break;
		case LLModel::LOD_PHYSICS:	suffix = "_PHYS";	break;
		case LLModel::LOD_HIGH:							break;
	}
	return suffix;
}

static void find_model(LLModelLoader::scene& scene,
					   const std::string& name_to_match,
					   LLModel*& base_model_out, LLMatrix4& mat_out)
{
    for (LLModelLoader::scene::iterator scene_it = scene.begin(),
										base_end = scene.end();
		 scene_it != base_end; ++scene_it)
    {
        mat_out = scene_it->first;
		for (LLModelLoader::model_instance_list_t::iterator
				it = scene_it->second.begin(), end = scene_it->second.end();
			 it != end; ++it)
		{
		    LLModelInstance& base_instance = *it;
            LLModel* base_model = base_instance.mModel;
            if (base_model && base_model->mLabel == name_to_match)
            {
                base_model_out = base_model;
                return;
            }
        }
    }
}

//-----------------------------------------------------------------------------
// LLFloaterModelUploadBase()
//-----------------------------------------------------------------------------

LLFloaterModelUploadBase::LLFloaterModelUploadBase()
:	mHasUploadPerm(false)
{
}

void LLFloaterModelUploadBase::requestAgentUploadPermissions()
{
	const std::string& url = gAgent.getRegionCapability("MeshUploadFlag");
	if (url.empty())
	{
		LLSD args;
		args["CAPABILITY"] = "MeshUploadFlag";
		gNotifications.add("RegionCapabilityRequestError", args);
		// *HACK: avoid being blocked by broken server side stuff
		mHasUploadPerm = true;
		return;
	}
	llinfos << "Requesting upload model permissions from: " << url
			<< llendl;
	gCoros.launch("LLFloaterModelUploadBase::requestUploadPermCoro",
				  boost::bind(&LLFloaterModelUploadBase::requestUploadPermCoro,
							  this, url, getPermObserverHandle()));
}

void LLFloaterModelUploadBase::requestUploadPermCoro(std::string url,
													 LLHandle<LLUploadPermissionsObserver> handle)
{
	LLCoreHttpUtil::HttpCoroutineAdapter adapter("MeshUploadFlag");
	LLSD result = adapter.getAndSuspend(url);

	LLUploadPermissionsObserver* observer = handle.get();
	if (!observer)
	{
		llwarns << "Unable to get observer after call to '" << url
				<< "' aborting." << llendl;
		return;
	}

	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	if (status)
	{
		LL_DEBUGS("MeshUpload") << "Upload permissions received, calling observer."
								<< LL_ENDL;
		result.erase(LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS);
		observer->onPermissionsReceived(result);
	}
	else
	{
		LL_DEBUGS("MeshUpload") << "Upload permissions error received, calling observer."
								<< LL_ENDL;
		observer->setPermissonsErrorStatus(status.getStatus(),
										   status.getMessage());
	}
}

//-----------------------------------------------------------------------------
// LLFloaterModelPreview()
//-----------------------------------------------------------------------------

LLFloaterModelPreview::LLFloaterModelPreview(const LLSD&)
:	LLFloaterModelUploadBase(),
	mModelPreview(NULL),
	mStatusLock(new LLMutex()),
	mLibIsHACD(false),
	mSentFeeRequest(false),
	mSentUploadRequest(false),
	mLastMouseX(0),
	mLastMouseY(0)
{
	mLODMode[LLModel::LOD_HIGH] = LLModelPreview::LOD_FROM_FILE;
	for (U32 i = 0; i < LLModel::LOD_HIGH; ++i)
	{
		mLODMode[i] = LLModelPreview::GENERATE;
	}

	if (!gIsInSecondLife)
	{
		// Let's point to a known valid website page for OpenSim grids...
		LLGridManager* gm = LLGridManager::getInstance();
		mValidateURL = gm->getAccountURL();	// Sounds a reasonable default...
		if (mValidateURL.empty())
		{
			// Then try support
			mValidateURL = gm->getSupportURL();
			if (mValidateURL.empty())
			{
				// Then try website
				mValidateURL = gm->getWebsiteURL();
				if (mValidateURL.empty())
				{
					// Last chance !
					mValidateURL = gm->getLoginPageURI();
				}
			}
		}
	}
	else if (gIsInProductionGrid)
	{
		mValidateURL = AGNI_VALIDATE_MESH_UPLOAD_PAGE_URL;
	}
	else
	{
		mValidateURL = ADITI_VALIDATE_MESH_UPLOAD_PAGE_URL;
	}

	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_model_preview.xml",
												 NULL,
												 false);	// Do not open now
}

bool LLFloaterModelPreview::postBuild()
{
	if (!LLFloater::postBuild())
	{
		return false;
	}

	mTabContainer = getChild<LLTabContainer>("import_tab");

	LLPanel* tabp = mTabContainer->getChild<LLPanel>("lod_panel");
	mTabContainer->setTabChangeCallback(tabp, onTabChanged);
	mTabContainer->setTabUserData(tabp, this);

	tabp = mTabContainer->getChild<LLPanel>("physics_panel");
	mTabContainer->setTabChangeCallback(tabp, onTabChanged);
	mTabContainer->setTabUserData(tabp, this);

	mModifiersPanel = mTabContainer->getChild<LLPanel>("modifiers_panel");
	mTabContainer->setTabChangeCallback(mModifiersPanel, onTabChanged);
	mTabContainer->setTabUserData(mModifiersPanel, this);
	mConflictsText =
		mModifiersPanel->getChild<LLTextBox>("conflicts_description");
	mOverridesLabel =
		mModifiersPanel->getChild<LLTextBox>("position_overrides_label");
	mJointsList = mModifiersPanel->getChild<LLScrollListCtrl>("joints_list");
	mJointsList->setCommitOnSelectionChange(true);
	mJointsList->setCommitCallback(onJointListSelection);
	mJointsList->setCallbackUserData(this);
	mJointsOverrides =
		mModifiersPanel->getChild<LLScrollListCtrl>("overrides_list");
	clearSkinningInfo();

	mLogPanel = mTabContainer->getChild<LLPanel>("log_panel");
	mTabContainer->setTabChangeCallback(mLogPanel, onTabChanged);
	mTabContainer->setTabUserData(mLogPanel, this);

	childSetCommitCallback("crease_angle", onGenerateNormalsCommit, this);
	childSetCommitCallback("gen_normals", toggleGenerateNormals, this);

	LLComboBox* lod_source_combo;
	std::string lodstr, widget_name;
	for (S32 lod = 0; lod <= LLModel::LOD_HIGH; ++lod)
	{
		lodstr = lod_name[lod];
		widget_name = "lod_source_" + lodstr;
		lod_source_combo = getChild<LLComboBox>(widget_name.c_str());
		lod_source_combo->setCommitCallback(onLoDSourceCommit);
		lod_source_combo->setCallbackUserData((void*)(intptr_t)lod);
		lod_source_combo->setCurrentByIndex(mLODMode[lod]);

		widget_name = "lod_browse_" + lodstr;
		childSetAction(widget_name.c_str(), onBrowseLOD,
					   // *HACK: lod + 1 to avoid passing a NULL user data,
					   // that causes to skip setting the callback data.
					   (void*)(intptr_t)(lod + 1));
		widget_name = "lod_mode_" + lodstr;
		childSetCommitCallback(widget_name.c_str(), onLODParamCommit,
							   (void*)(intptr_t)lod);
		widget_name = "lod_error_threshold_" + lodstr;
		childSetCommitCallback(widget_name.c_str(), onLODParamCommit,
							   (void*)(intptr_t)lod);
		widget_name = "lod_triangle_limit_" + lodstr;
		childSetCommitCallback(widget_name.c_str(),
							   onLODParamCommitEnforceTriLimit,
							   (void*)(intptr_t)lod);
	}

	childSetTextArg("status", "[STATUS]", "");
	childSetValue("physics_status_message_text", "");

	mUploadBtn = getChild<LLButton>("ok_btn");
	mUploadBtn->setClickedCallback(onUpload, this);
	mUploadBtn->setEnabled(false);

	childSetAction("reset_btn", onReset, this);
	childSetAction("cancel_btn", onCancel, this);

	childSetCommitCallback("preview_lod_combo", onPreviewLODCommit, this);

	childSetCommitCallback("upload_skin", onUploadSkinCommit, this);
	childSetCommitCallback("upload_joints", onUploadJointsCommit, this);
	childSetCommitCallback("lock_scale_if_joint_position",
						   onUploadJointsCommit, this);
	childSetCommitCallback("upload_textures", toggleCalculateButtonCallBack,
						   this);

	childSetCommitCallback("import_scale", onImportScaleCommit, this);
	childSetCommitCallback("pelvis_offset", onPelvisOffsetCommit, this);

	childSetCommitCallback("show_edges", onViewOptionChecked, this);
	childSetCommitCallback("show_physics", onViewOptionChecked, this);
	childSetCommitCallback("show_textures", onViewOptionChecked, this);
	childSetCommitCallback("show_skin_weight", onViewOptionChecked, this);
	childSetCommitCallback("show_joint_overrides", onViewOptionChecked, this);
	childSetCommitCallback("show_joint_positions", onViewOptionChecked, this);
	childSetCommitCallback("show_collision_volumes", onViewOptionChecked, this);

	childDisable("upload_skin");
	childDisable("upload_joints");
	childDisable("lock_scale_if_joint_position");

	initDecompControls();

	LLView* preview_panel = getChild<LLView>("preview_panel");

	mPreviewRect = preview_panel->getRect();

	initModelPreview();

	// Set callbacks for left click on line editor rows
	for (U32 i = 0; i <= LLModel::LOD_HIGH; ++i)
	{
		LLTextBox* text = getChild<LLTextBox>(lod_label_name[i]);
		if (text)
		{
			text->setClickedCallback(onClickTextLOD, (void*)(intptr_t)i);
		}

		text = getChild<LLTextBox>(lod_triangles_name[i]);
		if (text)
		{
			text->setClickedCallback(onClickTextLOD, (void*)(intptr_t)i);
		}

		text = getChild<LLTextBox>(lod_vertices_name[i]);
		if (text)
		{
			text->setClickedCallback(onClickTextLOD, (void*)(intptr_t)i);
		}

		text = getChild<LLTextBox>(lod_status_name[i]);
		if (text)
		{
			text->setClickedCallback(onClickTextLOD, (void*)(intptr_t)i);
		}
	}

	mUploadLogText = getChild<LLTextEditor>("log_text");

	LLTextBox* warning = getChild<LLTextBox>("validate_url");
	warning->setClickedCallback(onClickValidateURL, this);

	mCalculateBtn = getChild<LLButton>("calculate_btn");
	mCalculateBtn->setClickedCallback(onClickCalculateBtn, this);

	toggleCalculateButton(true);

	return true;
}

LLFloaterModelPreview::~LLFloaterModelPreview()
{
	if (mModelPreview)
	{
		delete mModelPreview;
		mModelPreview = NULL;
	}

	delete mStatusLock;
	mStatusLock = NULL;
}

void LLFloaterModelPreview::initModelPreview()
{
	if (mModelPreview)
	{
		delete mModelPreview;
	}

	mModelPreview = new LLModelPreview(512, 512, this);
	mModelPreview->setPreviewTarget(SKIN_WEIGHT_CAMERA_DISTANCE);
	mModelPreview->setDetailsCallback(boost::bind(&LLFloaterModelPreview::setDetails,
												  this, _1, _2, _3));
	mModelPreview->setModelUpdatedCallback(boost::bind(&LLFloaterModelPreview::modelUpdated,
													   this, _1));
}

void LLFloaterModelPreview::onViewOptionChecked(LLUICtrl* ctrl, void* userdata)
{
	LLFloaterModelPreview* self = (LLFloaterModelPreview*)userdata;
	if (!self) return;

	LLModelPreview* mp = self->mModelPreview;
	if (mp)
	{
		const std::string& name = ctrl->getName();
		bool new_value = !mp->mViewOption[name];
		mp->mViewOption[name] = new_value;
		if (new_value)
		{
			// Cannot display both physics and skin weights... HB
			if (name == "show_physics")
			{
				self->childSetValue("show_skin_weight", false);
				mp->mViewOption["show_skin_weight"] = false;
			}
			else if (name == "show_skin_weight")
			{
				self->childSetValue("show_physics", false);
				mp->mViewOption["show_physics"] = false;
			}
		}
		mp->refresh();
	}
}

bool LLFloaterModelPreview::isViewOptionChecked(const LLSD& userdata)
{
	return mModelPreview && mModelPreview->mViewOption[userdata.asString()];
}

bool LLFloaterModelPreview::isViewOptionEnabled(const LLSD& userdata)
{
	return childIsEnabled(userdata.asString().c_str());
}

void LLFloaterModelPreview::setViewOptionEnabled(const char* option,
												 bool enabled)
{
	childSetEnabled(option, enabled);
}

void LLFloaterModelPreview::enableViewOption(const char* option)
{
	setViewOptionEnabled(option, true);
}

void LLFloaterModelPreview::disableViewOption(const char* option)
{
	setViewOptionEnabled(option, false);
}

struct LLMeshFileSelectorData
{
	LLModelPreview*	mMP;
	S32				mLOD;
};

void model_load_callback(HBFileSelector::ELoadFilter type,
						 std::string& filename, void* user_data)
{
	LLMeshFileSelectorData* data = (LLMeshFileSelectorData*)user_data;
	if (data)
	{
		data->mMP->loadModel(filename, data->mLOD);
		delete data;
	}
}

bool LLFloaterModelPreview::prepareToLoadModel(S32 lod)
{
	if (!mModelPreview)
	{
		return false;
	}

	mModelPreview->mLoading = true;

	if (lod == LLModel::LOD_PHYSICS)
	{
		// Loading physics from file
		mModelPreview->mPhysicsSearchLOD = lod;
		mModelPreview->mWarnPhysModel = false;
	}

	return true;
}

void LLFloaterModelPreview::loadModel(S32 lod)
{
	if (!prepareToLoadModel(lod))
	{
		return;
	}

	LLMeshFileSelectorData* data = new LLMeshFileSelectorData;
	data->mMP = mModelPreview;
	data->mLOD = lod;
	HBFileSelector::loadFile(HBFileSelector::FFLOAD_MODEL, model_load_callback,
							 data);
}

void LLFloaterModelPreview::loadModel(S32 lod, const std::string& file_name,
									  bool force_disable_slm)
{
	if (prepareToLoadModel(lod))
	{
		mModelPreview->loadModel(file_name, lod, force_disable_slm);
	}
}

//static
LLModelPreview* LLFloaterModelPreview::getModelPreview()
{
	LLModelPreview* preview = NULL;
	LLFloaterModelPreview* self = findInstance();
	if (self)
	{
		preview = self->mModelPreview;
	}
	return preview;
}

//static
void LLFloaterModelPreview::onTabChanged(void* userdata, bool)
{
	LLFloaterModelPreview* self = (LLFloaterModelPreview*)userdata;
	if (self && self->mTabContainer->getCurrentPanel() == self->mLogPanel)
	{
		self->mTabContainer->setTabPanelFlashing(self->mLogPanel, false);
	}
}

//static
void LLFloaterModelPreview::onClickCalculateBtn(void* userdata)
{
	LLFloaterModelPreview* self = (LLFloaterModelPreview*)userdata;
	if (!self || !self->mModelPreview) return;

	self->clearLog();

	self->mSentFeeRequest = true;
	self->mModelPreview->rebuildUploadData();

	bool upload_skinweights = self->childGetValue("upload_skin").asBoolean();
	bool upload_joint_pos = self->childGetValue("upload_joints").asBoolean();
	bool lock_scale_if_joint_pos =
		self->childGetValue("lock_scale_if_joint_position").asBoolean();

	self->mUploadModelUrl.clear();
	self->mModelPhysicsFee.clear();

	gMeshRepo.uploadModel(self->mModelPreview->mUploadData,
						  self->mModelPreview->mPreviewScale,
						  self->childGetValue("upload_textures").asBoolean(),
						  upload_skinweights, upload_joint_pos,
						  lock_scale_if_joint_pos, self->mUploadModelUrl,
						  false, self->getWholeModelFeeObserverHandle());

	self->toggleCalculateButton(false);
	self->mUploadBtn->setEnabled(false);
}

//static
void LLFloaterModelPreview::onImportScaleCommit(LLUICtrl*, void* userdata)
{
	LLFloaterModelPreview* self = (LLFloaterModelPreview*)userdata;
	if (self && self->mModelPreview)
	{
		self->mModelPreview->mDirty = true;
		self->toggleCalculateButton(true);
		self->mModelPreview->refresh();
	}
}

//static
void LLFloaterModelPreview::onPelvisOffsetCommit(LLUICtrl*, void* userdata)
{
	LLFloaterModelPreview* self = (LLFloaterModelPreview*)userdata;
	if (self && self->mModelPreview)
	{
		self->mModelPreview->mDirty = true;
		self->toggleCalculateButton(true);
		self->mModelPreview->refresh();
	}
}

//static
void LLFloaterModelPreview::onUploadJointsCommit(LLUICtrl*, void* userdata)
{
	LLFloaterModelPreview* self = (LLFloaterModelPreview*)userdata;
	if (self && self->mModelPreview)
	{
		self->mModelPreview->refresh();
	}
}

//static
void LLFloaterModelPreview::onUploadSkinCommit(LLUICtrl*, void* userdata)
{
	LLFloaterModelPreview* self = (LLFloaterModelPreview*)userdata;
	if (self && self->mModelPreview)
	{
		self->mModelPreview->refresh();
		self->mModelPreview->resetPreviewTarget();
		self->mModelPreview->clearBuffers();
	}
}

//static
void LLFloaterModelPreview::onClickTextLOD(void* userdata)
{
	LLFloaterModelPreview* self = findInstance();
	if (self && self->mModelPreview)
	{
		S32 lod = (S32)(intptr_t)userdata;
		self->mModelPreview->setPreviewLOD(lod);
	}
}

//static
void LLFloaterModelPreview::onPreviewLODCommit(LLUICtrl* ctrl, void* userdata)
{
	LLFloaterModelPreview* self = (LLFloaterModelPreview*)userdata;
	if (self && ctrl && self->mModelPreview)
	{
		LLComboBox* combo = (LLComboBox*)ctrl;
		// Combo box list of lods is in reverse order
		S32 which_mode = NUM_LOD - 1 - combo->getFirstSelectedIndex();
		self->mModelPreview->setPreviewLOD(which_mode);
	}
}

//static
void LLFloaterModelPreview::onGenerateNormalsCommit(LLUICtrl*, void* userdata)
{
	LLFloaterModelPreview* self = (LLFloaterModelPreview*)userdata;
	if (self && self->mModelPreview)
	{
		self->mModelPreview->generateNormals();
	}
}

//static
void LLFloaterModelPreview::toggleGenerateNormals(LLUICtrl*, void* userdata)
{
	LLFloaterModelPreview* self = (LLFloaterModelPreview*)userdata;
	if (self && self->mModelPreview)
	{
		bool enabled = self->childGetValue("gen_normals").asBoolean();
		self->childSetEnabled("crease_label", enabled);
		self->childSetEnabled("crease_angle", enabled);
		if (enabled)
		{
			self->mModelPreview->generateNormals();
		}
		else
		{
			self->mModelPreview->restoreNormals();
		}
	}
}

//static
void LLFloaterModelPreview::onExplodeCommit(LLUICtrl*, void* userdata)
{
	LLFloaterModelPreview* self = (LLFloaterModelPreview*)userdata;
	if (self && self->mModelPreview)
	{
		self->mModelPreview->refresh();
	}
}

//static
void LLFloaterModelPreview::onAutoFillCommit(LLUICtrl*, void* userdata)
{
	LLFloaterModelPreview* self = (LLFloaterModelPreview*)userdata;
	if (self && self->mModelPreview)
	{
		self->mModelPreview->queryLODs();
	}
}

//static
void LLFloaterModelPreview::onLODParamCommit(LLUICtrl*, void* userdata)
{
	LLFloaterModelPreview* self = findInstance();
	if (self && self->mModelPreview)
	{
		S32 lod = (S32)(intptr_t)userdata;
		self->mModelPreview->onLODParamCommit(lod, false);

		// Refresh LoDs that reference this one
		std::string cname;
		for (S32 i = lod - 1; i >= 0; --i)
		{
			cname = "lod_source_" + lod_name[i];
			LLComboBox* lod_combo = self->getChild<LLComboBox>(cname.c_str());
			if (lod_combo->getCurrentIndex() == LLModelPreview::USE_LOD_ABOVE)
			{
				onLoDSourceCommit(NULL, (void*)(intptr_t)i);
			}
			else
			{
				break;
			}
		}
	}
}

//static
void LLFloaterModelPreview::onLODParamCommitEnforceTriLimit(LLUICtrl*,
															void* userdata)
{
	LLFloaterModelPreview* self = findInstance();
	if (self && self->mModelPreview)
	{
		S32 lod = (S32)(intptr_t)userdata;
		self->mModelPreview->onLODParamCommit(lod, true);

		// Refresh LoDs that reference this one
		std::string cname;
		for (S32 i = lod - 1; i >= 0; --i)
		{
			cname = "lod_source_" + lod_name[i];
			LLComboBox* lod_combo = self->getChild<LLComboBox>(cname.c_str());
			if (lod_combo->getCurrentIndex() != LLModelPreview::USE_LOD_ABOVE)
			{
				break;
			}
			onLoDSourceCommit(NULL, (void*)(intptr_t)i);
		}
	}
}

//virtual
void LLFloaterModelPreview::draw()
{
	LLFloater::draw();

	if (!mModelPreview) return;

	mModelPreview->update();

	if (!mModelPreview->mLoading)
	{
		if (mSentFeeRequest)
		{
			childSetTextArg("status", "[STATUS]",
							getString("status_waiting_server"));
		}
		else if (mSentUploadRequest)
		{
			childSetTextArg("status", "[STATUS]",
							getString("status_uploading"));
		}
		else if (mModelPreview->mLoadState == LLModelLoader::ERROR_MATERIALS)
		{
			childSetTextArg("status", "[STATUS]",
							getString("mesh_status_invalid_material_list"));
		}
		else if (mModelPreview->mLoadState > LLModelLoader::ERROR_MODEL)
		{
			childSetTextArg("status", "[STATUS]",
							getString(LLModel::getStatusString(mModelPreview->mLoadState -
															   LLModelLoader::ERROR_MODEL)));
		}
		else if (mModelPreview->mLoadState == LLModelLoader::ERROR_PARSING)
		{
			childSetTextArg("status", "[STATUS]",
							getString("status_parse_error"));
			toggleCalculateButton(false);
		}
		else if (mModelPreview->mLoadState ==
					LLModelLoader::ERROR_HIGH_LOD_MODEL_MISSING)
		{
			childSetTextArg("status", "[STATUS]",
							getString("status_high_lod_model_missing"));
			toggleCalculateButton(false);
		}
		else if (mModelPreview->mLoadState ==
					LLModelLoader::ERROR_LOD_MODEL_MISMATCH)
		{
			childSetTextArg("status", "[STATUS]",
							getString("status_lod_model_mismatch"));
			toggleCalculateButton(false);
		}
		else if (mModelPreview->mLoadState ==
					LLModelLoader::WARNING_BIND_SHAPE_ORIENTATION)
		{
			childSetTextArg("status", "[STATUS]",
							getString("status_bind_shape_orientation"));
		}
		else
		{
			childSetTextArg("status", "[STATUS]", "");
		}
	}

	if (!mModelPreview->lodsReady())
	{
		return;
	}

	gGL.color3f(1.f, 1.f, 1.f);

	LLTexUnit* unit0 = gGL.getTexUnit(0);
	unit0->bind(mModelPreview);

	LLView* preview_panel = getChild<LLView>("preview_panel");

	LLRect rect = preview_panel->getRect();
	if (rect != mPreviewRect)
	{
		mModelPreview->refresh();
		mPreviewRect = preview_panel->getRect();
	}

	gGL.begin(LLRender::TRIANGLES);
	{
		gGL.texCoord2f(0.f, 1.f);
		gGL.vertex2i(mPreviewRect.mLeft, mPreviewRect.mTop - 1);
		gGL.texCoord2f(0.f, 0.f);
		gGL.vertex2i(mPreviewRect.mLeft, mPreviewRect.mBottom);
		gGL.texCoord2f(1.f, 0.f);
		gGL.vertex2i(mPreviewRect.mRight - 1, mPreviewRect.mBottom);
		gGL.texCoord2f(0.f, 1.f);
		gGL.vertex2i(mPreviewRect.mLeft, mPreviewRect.mTop - 1);
		gGL.texCoord2f(1.f, 0.f);
		gGL.vertex2i(mPreviewRect.mRight - 1, mPreviewRect.mBottom);
		gGL.texCoord2f(1.f, 1.f);
		gGL.vertex2i(mPreviewRect.mRight - 1, mPreviewRect.mTop - 1);
	}
	gGL.end();

	unit0->unbind(LLTexUnit::TT_TEXTURE);
}

//virtual
void LLFloaterModelPreview::refresh()
{
	if (mModelPreview)
	{
		mModelPreview->mDirty = true;
	}
}

//virtual
bool LLFloaterModelPreview::handleMouseDown(S32 x, S32 y, MASK mask)
{
	if (mPreviewRect.pointInRect(x, y))
	{
		bringToFront(x, y);
		gFocusMgr.setMouseCapture(this);
		gViewerWindowp->hideCursor();
		mLastMouseX = x;
		mLastMouseY = y;
		return true;
	}

	return LLFloater::handleMouseDown(x, y, mask);
}

//virtual
bool LLFloaterModelPreview::handleMouseUp(S32 x, S32 y, MASK mask)
{
	gFocusMgr.setMouseCapture(NULL);
	gViewerWindowp->showCursor();
	return LLFloater::handleMouseUp(x, y, mask);
}

//virtual
bool LLFloaterModelPreview::handleHover(S32 x, S32 y, MASK mask)
{
	MASK local_mask = mask & ~MASK_ALT;

	if (mModelPreview && hasMouseCapture())
	{
		if (local_mask == MASK_PAN)
		{
			// pan here
			mModelPreview->pan((F32)(x - mLastMouseX) * -0.005f,
							   (F32)(y - mLastMouseY) * -0.005f);
		}
		else if (local_mask == MASK_ORBIT)
		{
			F32 yaw_radians = (F32)(x - mLastMouseX) * -0.01f;
			F32 pitch_radians = (F32)(y - mLastMouseY) * 0.02f;

			mModelPreview->rotate(yaw_radians, pitch_radians);
		}
		else
		{

			F32 yaw_radians = (F32)(x - mLastMouseX) * -0.01f;
			F32 zoom_amt = (F32)(y - mLastMouseY) * 0.02f;

			mModelPreview->rotate(yaw_radians, 0.f);
			mModelPreview->zoom(zoom_amt);
		}

		mModelPreview->refresh();

		LLUI::setCursorPositionLocal(this, mLastMouseX, mLastMouseY);
	}

	if (!mPreviewRect.pointInRect(x, y) || !mModelPreview)
	{
		return LLFloater::handleHover(x, y, mask);
	}
	else if (local_mask == MASK_ORBIT)
	{
		gViewerWindowp->setCursor(UI_CURSOR_TOOLCAMERA);
	}
	else if (local_mask == MASK_PAN)
	{
		gViewerWindowp->setCursor(UI_CURSOR_TOOLPAN);
	}
	else
	{
		gViewerWindowp->setCursor(UI_CURSOR_TOOLZOOMIN);
	}

	return true;
}

//virtual
bool LLFloaterModelPreview::handleScrollWheel(S32 x, S32 y, S32 clicks)
{
	if (mPreviewRect.pointInRect(x, y) && mModelPreview)
	{
		mModelPreview->zoom((F32)clicks * -0.2f);
		mModelPreview->refresh();
		return true;
	}
	return LLFloaterModelUploadBase::handleScrollWheel(x, y, clicks);
}

//virtual
void LLFloaterModelPreview::onOpen()
{
	requestAgentUploadPermissions();
}

//static
void LLFloaterModelPreview::onPhysicsParamCommit(LLUICtrl* ctrl,
												 void* userdata)
{
	if (LLConvexDecomposition::getInstance() == NULL)
	{
		llwarns << "Convex decomposition tool is a stub on this platform. Cannot get decomp."
				<< llendl;
		return;
	}

	LLFloaterModelPreview* self = findInstance();
	if (self)
	{
		LLCDParam* param = (LLCDParam*) userdata;
		std::string name(param->mName);

		LLSD value = ctrl->getValue();

		if ("Retain%" == name)
		{
			value = ctrl->getValue().asReal() / RETAIN_COEFFICIENT;
		}

		self->mDecompParams[name] = value;

		if (name == "Simplify Method")
		{
			bool show_retain = false;
			bool show_detail = true;

			if (ctrl->getValue().asInteger() == 0)
			{
				 show_retain = true;
				 show_detail = false;
			}

			self->childSetVisible("Retain%", show_retain);
			self->childSetVisible("Retain%_label", show_retain);

			self->childSetVisible("Detail Scale", show_detail);
			self->childSetVisible("Detail Scale label", show_detail);
		}
	}
}

//static
void LLFloaterModelPreview::onPhysicsStageExecute(LLUICtrl*, void* userdata)
{
	LLFloaterModelPreview* self = findInstance();
	if (self && userdata)
	{
		LLCDStageData* stage_data = (LLCDStageData*)userdata;
		std::string stage = stage_data->mName;

		if (!self->mCurRequest.empty())
		{
			llinfos << "Decomposition request still pending." << llendl;
			return;
		}

		if (self->mModelPreview)
		{
			for (S32 i = 0,
					 count = self->mModelPreview->mModel[LLModel::LOD_PHYSICS].size();
				 i < count; ++i)
			{
				LLModel* mdl =
					self->mModelPreview->mModel[LLModel::LOD_PHYSICS][i];
				DecompRequest* request = new DecompRequest(stage, mdl);
				self->mCurRequest.insert(request);
				gMeshRepo.mDecompThread->submitRequest(request);
			}
		}

		if (stage == "Decompose")
		{
			self->setStatusMessage(self->getString("decomposing"));
			self->childSetVisible("Decompose", false);
			self->childSetVisible("decompose_cancel", true);
			self->childDisable("Simplify");
		}
		else if (stage == "Simplify")
		{
			self->setStatusMessage(self->getString("simplifying"));
			self->childSetVisible("Simplify", false);
			self->childSetVisible("simplify_cancel", true);
			self->childDisable("Decompose");
		}
	}
}

//static
void LLFloaterModelPreview::onPhysicsBrowse(void* userdata)
{
	LLFloaterModelPreview* self = (LLFloaterModelPreview*)userdata;
	if (self)
	{
		self->loadModel(LLModel::LOD_PHYSICS);
	}
}

//static
void LLFloaterModelPreview::onPhysicsUseLOD(LLUICtrl*, void* userdata)
{
	LLFloaterModelPreview* self = (LLFloaterModelPreview*)userdata;
	if (!self) return;

	LLModelPreview* mp = self->mModelPreview;
	if (!mp) return;

	// 0 = no physics hull/SL default, 1 = highest LOD, ... 4 = lowest LOD,
	// 5 = bounding box, 6 = from file.
	static S32 previous_mode = 0;

	S32 which_mode =
		self->getChild<LLComboBox>("physics_lod_combo")->getCurrentIndex();
	if (which_mode == 5)
	{
		std::string filename =
			gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS, "meshes",
										   "cube.dae");
		mp->loadModel(filename, LLModel::LOD_PHYSICS, true, false);
		mp->refresh();
		mp->updateStatusMessages();
	}
	else
	{
		bool lod_to_file = previous_mode != 6 && which_mode == 6;
		bool file_to_lod = previous_mode >= 5 && which_mode < 5;
		if (!lod_to_file)
		{
			mp->setPhysicsFromLOD(4 - which_mode);
			mp->refresh();
		}
		if (lod_to_file || file_to_lod)
		{
			mp->refresh();
			mp->updateStatusMessages();
		}
	}

	previous_mode = which_mode;
}

//static
void LLFloaterModelPreview::onCancel(void* userdata)
{
	LLFloaterModelPreview* self = (LLFloaterModelPreview*)userdata;
	if (self)
	{
		onPhysicsStageCancel(self);
		self->close();
	}
}

//static
void LLFloaterModelPreview::onPhysicsStageCancel(void* userdata)
{
	LLFloaterModelPreview* self = (LLFloaterModelPreview*)userdata;
	if (self)
	{
		for (std::set<LLPointer<DecompRequest> >::iterator
					it = self->mCurRequest.begin(),
					end = self->mCurRequest.end();
			 it != end; ++it)
		{
			DecompRequest* req = *it;
			req->mContinue = 0;
		}

		self->mCurRequest.clear();

		if (self->mModelPreview)
		{
			self->mModelPreview->updateStatusMessages();
		}
	}
}

void LLFloaterModelPreview::initDecompControls()
{
	LLSD key;

	childSetAction("simplify_cancel", onPhysicsStageCancel, this);
	childSetAction("decompose_cancel", onPhysicsStageCancel, this);

	childSetCommitCallback("physics_lod_combo", onPhysicsUseLOD, this);
	childSetAction("physics_browse", onPhysicsBrowse, this);

	static const LLCDStageData* stage = NULL;
	static S32 stage_count = 0;

	LLConvexDecomposition* decomp = LLConvexDecomposition::getInstance();
	if (decomp)
	{
		stage_count = decomp->getStages(&stage);
	}
	LL_DEBUGS("MeshUpload") << "stage_count = " << stage_count << LL_ENDL;

	static const LLCDParam* param = NULL;
	static S32 param_count = 0;
	if (!param && decomp)
	{
		param_count = decomp->getParameters(&param);
	}
	LL_DEBUGS("MeshUpload") << "param_count = " << param_count << LL_ENDL;

	for (S32 j = stage_count - 1; j >= 0; --j)
	{
		LLUICtrl* ctrl = getChild<LLUICtrl>(stage[j].mName, true, false);
		if (ctrl)
		{
			ctrl->setCommitCallback(onPhysicsStageExecute);
			ctrl->setCallbackUserData((void*)&stage[j]);
		}

		gMeshRepo.mDecompThread->mStageID[stage[j].mName] = j;
		// Protected against stub by stage_count being 0 for stub above
		if (decomp)
		{
			decomp->registerCallback(j, LLPhysicsDecomp::llcdCallback);
		}

		LL_DEBUGS("MeshUpload") << "Physics decomp stage " << stage[j].mName
								<< " (" << j << ") parameters:" << LL_ENDL;
		LL_DEBUGS("MeshUpload") << "------------------------------------"
								<< LL_ENDL;

		for (S32 i = 0; i < param_count; ++i)
		{
			if (param[i].mStage != j)
			{
				continue;
			}

			std::string name(param[i].mName ? param[i].mName : "");
			std::string description(param[i].mDescription ? param[i].mDescription
														  : "");

			std::string type = "unknown";

			LL_DEBUGS("MeshUpload") << name << " - " << description << LL_ENDL;

			if (param[i].mType == LLCDParam::LLCD_FLOAT)
			{
				mDecompParams[param[i].mName] = LLSD(param[i].mDefault.mFloat);
				LL_DEBUGS("MeshUpload") << "Type: float - Default: "
										<< param[i].mDefault.mFloat << LL_ENDL;

				LLUICtrl* ctrl = getChild<LLUICtrl>(name.c_str());
				if (LLSliderCtrl* slider = dynamic_cast<LLSliderCtrl*>(ctrl))
				{
					LL_DEBUGS("MeshUpload") << name
											<< " corresponds to a slider."
											<< LL_ENDL;
					slider->setMinValue(param[i].mDetails.mRange.mLow.mFloat);
					slider->setMaxValue(param[i].mDetails.mRange.mHigh.mFloat);
					slider->setIncrement(param[i].mDetails.mRange.mDelta.mFloat);
					slider->setValue(param[i].mDefault.mFloat);
					slider->setCommitCallback(onPhysicsParamCommit);
					slider->setCallbackUserData((void*) &param[i]);
				}
				else if (LLSpinCtrl* spinner = dynamic_cast<LLSpinCtrl*>(ctrl))
				{
					LL_DEBUGS("MeshUpload") << name
											<< " corresponds to a spinner."
											<< LL_ENDL;
					bool is_retain_ctrl = "Retain%" == name;
					double coefficient = is_retain_ctrl ? RETAIN_COEFFICIENT : 1.f;

					spinner->setMinValue(param[i].mDetails.mRange.mLow.mFloat * coefficient);
					spinner->setMaxValue(param[i].mDetails.mRange.mHigh.mFloat * coefficient);
					spinner->setIncrement(param[i].mDetails.mRange.mDelta.mFloat * coefficient);
					spinner->setValue(param[i].mDefault.mFloat * coefficient);
					spinner->setCommitCallback(onPhysicsParamCommit);
					spinner->setCallbackUserData((void*) &param[i]);
				}
				else if (LLComboBox* combo_box = dynamic_cast<LLComboBox*>(ctrl))
				{
					LL_DEBUGS("MeshUpload") << name
											<< " corresponds to a combo box."
											<< LL_ENDL;
					F32 min = param[i].mDetails.mRange.mLow.mFloat;
					F32 max = param[i].mDetails.mRange.mHigh.mFloat;
					F32 delta = param[i].mDetails.mRange.mDelta.mFloat;

					if (name == "Cosine%")
					{
						createSmoothComboBox(combo_box, min, max);
						combo_box->setValue(0);
					}
					else
					{
						for (F32 value = min; value <= max; value += delta)
						{
							std::string label = llformat("%.1f", value);
							combo_box->add(label, value, ADD_BOTTOM, true);
						}
						combo_box->setValue(param[i].mDefault.mFloat);
					}

					combo_box->setCommitCallback(onPhysicsParamCommit);
					combo_box->setCallbackUserData((void*)&param[i]);
				}
				else
				{
					LL_DEBUGS("MeshUpload") << "WARNING: " << name
											<< " does not correspond to any widget !"
											<< LL_ENDL;
				}
			}
			else if (param[i].mType == LLCDParam::LLCD_INTEGER)
			{
				mDecompParams[param[i].mName] = LLSD(param[i].mDefault.mIntOrEnumValue);
				LL_DEBUGS("MeshUpload") << "Type: integer - Default: "
										<< param[i].mDefault.mIntOrEnumValue
										<< LL_ENDL;

				LLUICtrl* ctrl = getChild<LLUICtrl>(name.c_str());
				if (LLSliderCtrl* slider = dynamic_cast<LLSliderCtrl*>(ctrl))
				{
					slider->setMinValue(param[i].mDetails.mRange.mLow.mIntOrEnumValue);
					slider->setMaxValue(param[i].mDetails.mRange.mHigh.mIntOrEnumValue);
					slider->setIncrement(param[i].mDetails.mRange.mDelta.mIntOrEnumValue);
					slider->setValue(param[i].mDefault.mIntOrEnumValue);
					slider->setCommitCallback(onPhysicsParamCommit);
					slider->setCallbackUserData((void*) &param[i]);
				}
				else if (LLComboBox* combo_box = dynamic_cast<LLComboBox*>(ctrl))
				{
					for (S32 k = param[i].mDetails.mRange.mLow.mIntOrEnumValue;
						 k <= param[i].mDetails.mRange.mHigh.mIntOrEnumValue;
						 k += param[i].mDetails.mRange.mDelta.mIntOrEnumValue)
					{
						std::string name = llformat("%.1d", k);
						combo_box->add(name, k, ADD_BOTTOM, true);
					}
					combo_box->setValue(param[i].mDefault.mIntOrEnumValue);
					combo_box->setCommitCallback(onPhysicsParamCommit);
					combo_box->setCallbackUserData((void*) &param[i]);
				}
				else
				{
					LL_DEBUGS("MeshUpload") << "WARNING: " << name
											<< " does not correspond to any widget !"
											<< LL_ENDL;
				}
			}
			else if (param[i].mType == LLCDParam::LLCD_BOOLEAN)
			{
				mDecompParams[param[i].mName] = LLSD(param[i].mDefault.mBool);
				LL_DEBUGS("MeshUpload") << "Type: boolean - Default: "
										<< (param[i].mDefault.mBool ? "TRUE" : "FALSE")
										<< LL_ENDL;

				LLCheckBoxCtrl* check_box =
					getChild<LLCheckBoxCtrl>(name.c_str(), true, false);
				if (check_box)
				{
					check_box->setValue(param[i].mDefault.mBool);
					check_box->setCommitCallback(onPhysicsParamCommit);
					check_box->setCallbackUserData((void*)&param[i]);
				}
				else if (name == "nd_AlwaysNeedTriangles")
				{
					if (!mLibIsHACD)
					{
						llinfos << "HACD convex decomposition library detected. Some features will not be available."
								<< llendl;
						mLibIsHACD = true;
					}
				}
				else
				{
					LL_DEBUGS("MeshUpload") << "WARNING: " << name
											<< " does not correspond to any widget !"
											<< LL_ENDL;
				}
			}
			else if (param[i].mType == LLCDParam::LLCD_ENUM)
			{
				mDecompParams[param[i].mName] = LLSD(param[i].mDefault.mIntOrEnumValue);
				LL_DEBUGS("MeshUpload") << "Type: enum - Default: "
										<< param[i].mDefault.mIntOrEnumValue
										<< LL_ENDL;

				// Plug into combo box
				LL_DEBUGS("MeshUpload") << "Accepted values: " << LL_ENDL;
				LLComboBox* combo_box = getChild<LLComboBox>(name.c_str(),
															 true, false);
				if (combo_box)
				{
					for (S32 k = 0; k < param[i].mDetails.mEnumValues.mNumEnums; ++k)
					{
						LL_DEBUGS("MeshUpload") << param[i].mDetails.mEnumValues.mEnumsArray[k].mValue
												<< " - "
												<< param[i].mDetails.mEnumValues.mEnumsArray[k].mName
												<< LL_ENDL;

						std::string name(param[i].mDetails.mEnumValues.mEnumsArray[k].mName);
						combo_box->add(name,
									   LLSD::Integer(param[i].mDetails.mEnumValues.mEnumsArray[k].mValue));
					}
					combo_box->setValue(param[i].mDefault.mIntOrEnumValue);
					combo_box->setCommitCallback(onPhysicsParamCommit);
					combo_box->setCallbackUserData((void*) &param[i]);
				}
				else
				{
					LL_DEBUGS("MeshUpload") << "WARNING: " << name
											<< " does not correspond to any widget !"
											<< LL_ENDL;
				}

				LL_DEBUGS("MeshUpload") << "----" << LL_ENDL;
			}
			LL_DEBUGS("MeshUpload") << "-----------------------------" << LL_ENDL;
		}
	}

	childSetCommitCallback("physics_explode", onExplodeCommit, this);
}

void LLFloaterModelPreview::createSmoothComboBox(LLComboBox* combo_box,
												 F32 min, F32 max)
{
	F32 delta = (max - min) / SMOOTH_VALUES_NUMBER;
	S32 ilabel = 0;

	combo_box->add("0 (none)", ADD_BOTTOM, true);

	std::string label;
	for (F32 value = min + delta; value < max; value += delta)
	{
		label = (++ilabel == SMOOTH_VALUES_NUMBER ? "10 (max)"
												  : llformat("%.1d", ilabel));
		combo_box->add(label, value, ADD_BOTTOM, true);
	}
}

void LLFloaterModelPreview::setDetails(F32 x, F32 y, F32 z)
{
	assert_main_thread();
	childSetTextArg("import_dimensions", "[X]", llformat("%.3f", x));
	childSetTextArg("import_dimensions", "[Y]", llformat("%.3f", y));
	childSetTextArg("import_dimensions", "[Z]", llformat("%.3f", z));
}

//virtual
void LLFloaterModelPreview::onPermissionsReceived(const LLSD& result)
{
	dump_llsd_to_file(result, "perm_received.xml");
	std::string upload_status = result["mesh_upload_status"].asString();
	// *HACK: handle "" for case that  MeshUploadFlag cap is broken.
	mHasUploadPerm = ("" == upload_status || "valid" == upload_status);

	// isModelUploadAllowed() includes mHasUploadPerm
	mUploadBtn->setEnabled(isModelUploadAllowed());
	childSetVisible("warning_message", !mHasUploadPerm);
	childSetVisible("validate_url", !mHasUploadPerm && !mValidateURL.empty());
}

void LLFloaterModelPreview::setPermissonsErrorStatus(S32 status,
													 const std::string& reason)
{
	llwarns << "LLFloaterModelPreview::setPermissonsErrorStatus(" << status
			<< " : " << reason << ")" << llendl;

	gNotifications.add("MeshUploadPermError");
}

void LLFloaterModelPreview::addMessageToLog(const std::string& msg,
											const LLSD& args, S32 lod,
											bool flash)
{
	if (msg.empty())
	{
		return;
	}

	std::string line;
	switch (lod)
	{
		case LLModel::LOD_IMPOSTOR:
			line = "LOD0 ";
			break;

		case LLModel::LOD_LOW:
			line = "LOD1 ";
			break;

		case LLModel::LOD_MEDIUM:
			line = "LOD2 ";
			break;

		case LLModel::LOD_HIGH:
			line = "LOD3 ";
			break;

		case LLModel::LOD_PHYSICS:
			line = "PHYS ";
			break;

		default:
			break;
	}

	LLStringUtil::format_map_t args_msg;
	for (LLSD::map_const_iterator it = args.beginMap(), end = args.endMap();
		 it != end; ++it)
	{
		args_msg[it->first] = it->second.asString();
	}
	line += getString(msg, args_msg);

	addLineToLog(line, flash);
}

void LLFloaterModelPreview::addLineToLog(const std::string& line, bool flash)
{
	if (line.empty())
	{
		return;
	}

	LLWString text = utf8str_to_wstring(line);
	S32 add_text_len = text.length() + 1; // newline
	S32 editor_max_len = mUploadLogText->getMaxLength();
	if (add_text_len > editor_max_len)
	{
		return;
	}

	// Make sure we have space for the new string
	S32 editor_text_len = mUploadLogText->getLength();
	while (editor_max_len < editor_text_len + add_text_len)
	{
		S32 shift = mUploadLogText->removeFirstLine();
		if (shift <= 0)
		{
			mUploadLogText->clear();
			break;
		}
		editor_text_len -= shift;
	}

	LL_DEBUGS("MeshUpload") << "Adding log line: " << line << LL_ENDL;

	mUploadLogText->appendText(line, false, true);

	if (flash)
	{
		if (mTabContainer->getCurrentPanel() != mLogPanel)
		{
			mTabContainer->setTabPanelFlashing(mLogPanel, true);
		}
	}
}

void LLFloaterModelPreview::clearLog()
{
	mUploadLogText->clear();
	mTabContainer->setTabPanelFlashing(mLogPanel, false);
}

void LLFloaterModelPreview::clearSkinningInfo()
{
	mSelectedJointName.clear();

	mJointsList->deleteAllItems();
	mJointsList->setVisible(false);

	mJointsOverrides->deleteAllItems();
	mJointsOverrides->setVisible(false);

	for (U32 i = 0; i < LLModel::NUM_LODS; ++i)
	{
		mJointOverrides[i].clear();
	}

	mOverridesLabel->setVisible(false);
	mConflictsText->setVisible(false);
	childSetVisible("skin_too_many_joints", false);
	childSetVisible("skin_unknown_joint", false);
}

void LLFloaterModelPreview::updateSkinningInfo(bool highlight_overrides)
{
	if (!mModelPreview) return;

	S32 display_lod = mModelPreview->mPreviewLOD;

	if (mModelPreview->mModel[display_lod].empty())
	{
		mSelectedJointName.clear();
		return;
	}

	// Joints are listed as long as they exist in mAlternateBindMatrix, even
	// if they are for some reason identical to defaults.
	// *TODO: are overrides always identical for all lods ?   They should, but
	// there might be situations where they are not.
	if (mJointOverrides[display_lod].empty())
	{
		for (LLModelLoader::scene::iterator
				it = mModelPreview->mScene[display_lod].begin(),
				end = mModelPreview->mScene[display_lod].end();
			 it != end; ++it)
		{
			for (LLModelLoader::model_instance_list_t::iterator
					mit = it->second.begin(), mend = it->second.end();
				 mit != mend; ++mit)
			{
				LLModelInstance& instance = *mit;
				LLModel* model = instance.mModel;
				const LLMeshSkinInfo* skin = &model->mSkinInfo;
				U32 joint_count = llmin(LL_MAX_JOINTS_PER_MESH_OBJECT,
										(U32)skin->mJointKeys.size());
				U32 bind_count = 0;
				if (highlight_overrides)
				{
					bind_count = skin->mAlternateBindMatrix.size();
				}
				if (bind_count && bind_count != joint_count)
				{
					std::ostringstream out;
					out << "Invalid joint overrides for model: "
						<< model->getName() << " - Amount of joints "
						<< joint_count << " is different from amount of overrides "
						<< bind_count;
					llwarns << out.str() << llendl;
					addLineToLog(out.str(), true);
					bind_count = 0;	// Disable overrides for this model
				}
				if (bind_count)
				{
					constexpr F32 MAX_SQUARED_OFFSET =
						LL_JOINT_TRESHOLD_POS_OFFSET *
						LL_JOINT_TRESHOLD_POS_OFFSET;
					for (U32 j = 0; j < joint_count; ++j)
					{
						const LLVector3& joint_pos =
							skin->mAlternateBindMatrix[j].getTranslation();
						const std::string& jname = skin->mJointNames[j];
						JointOverrideData& data =
							mJointOverrides[display_lod][jname];
						LLJoint* jointp =
							LLModelPreview::lookupJointByName(jname,
															  mModelPreview);
						if (!jointp) continue;

						if (jointp->aboveJointPosThreshold(joint_pos))
						{
							// Valid override
							if (data.mPosOverrides.size() &&
								(data.mPosOverrides.begin()->second -
								 joint_pos).lengthSquared() > MAX_SQUARED_OFFSET)
							{
								// File contains multiple meshes with
								// conflicting joint offsets;  preview may be
								// incorrect, upload result might wary (depends
								// onto mesh_id that is not yet generated).
								data.mHasConflicts = true;
							}
							data.mPosOverrides[model->getName()] = joint_pos;
						}
						else
						{
							// Default value which would nott be accounted for
							data.mModelsNoOverrides.insert(model->getName());
						}
					}
				}
				else
				{
					for (U32 j = 0; j < joint_count; ++j)
					{
						JointOverrideData& data =
							mJointOverrides[display_lod][skin->mJointNames[j]];
						data.mModelsNoOverrides.insert(model->getName());
					}
				}
			}
		}
	}

	U32 conflicts = 0;
	if (mJointsList->isEmpty())
	{
		mJointsList->setVisible(true);
		mJointsOverrides->setVisible(true);
		JointMap joint_alias_map;
		mModelPreview->getJointAliases(joint_alias_map);

		for (overrides_map_t::iterator
				it = mJointOverrides[display_lod].begin(),
				end = mJointOverrides[display_lod].end();
			 it != end; ++it)
		{
			const std::string& jname = it->first;

			LLSD element;
			element["id"] = jname;
			LLSD& column = element["columns"][0];
			column["column"] = "name";
			column["value"] = jname;
			column["type"] = "text";
			column["font"] = "SANSSERIF";

			if (joint_alias_map.find(jname) == joint_alias_map.end())
			{
				// Missing joint name
				column["color"] = LLColor4::red2.getValue();
			}
			if (it->second.mHasConflicts)
			{
				column["color"] = LLColor4::orange2.getValue();
				++conflicts;
			}
			if (highlight_overrides && it->second.mPosOverrides.size() > 0)
			{
				column["font-style"] = "BOLD";
			}
			else
			{
				column["font-style"] = "NORMAL";
			}

			mJointsList->addElement(element);
		}

		mJointsList->selectFirstItem();
		LLScrollListItem* selected = mJointsList->getFirstSelected();
		if (selected)
		{
			 mSelectedJointName = selected->getValue().asString();
		}
	}
	if (conflicts)
	{
		mConflictsText->setVisible(true);
		mConflictsText->setTextArg("[CONFLICTS]", llformat("%d", conflicts));
	}
}

//static
void LLFloaterModelPreview::onJointListSelection(LLUICtrl*, void* userdata)
{
	LLFloaterModelPreview* self = (LLFloaterModelPreview*)userdata;
	if (!self || !self->mModelPreview) return;

	self->mJointsOverrides->deleteAllItems();

	LLScrollListItem* selected = self->mJointsList->getFirstSelected();
	if (!selected)
	{
		self->mSelectedJointName.clear();
		self->mOverridesLabel->setVisible(false);
		return;
	}

	std::string label = selected->getValue().asString();
	self->mSelectedJointName = label;
	self->mOverridesLabel->setTextArg("[JOINT_NAME]", label);
	self->mOverridesLabel->setVisible(true);

	S32 lod = self->mModelPreview->mPreviewLOD;
	const JointOverrideData& data = self->mJointOverrides[lod][label];
	if (data.mModelsNoOverrides.empty() && data.mPosOverrides.empty())
	{
		return;
	}

	bool upload_joint_pos = self->childGetValue("upload_joints").asBoolean();

	// This is the constant part for every row of the list
	LLSD element;
	element["columns"][0]["column"] = "name";
	element["columns"][0]["type"] = "text";
	element["columns"][0]["font"] = "SANSSERIF";
	element["columns"][1]["column"] = "x";
	element["columns"][1]["type"] = "text";
	element["columns"][1]["font"] = "SANSSERIF";
	element["columns"][2]["column"] = "y";
	element["columns"][2]["type"] = "text";
	element["columns"][2]["font"] = "SANSSERIF";
	element["columns"][3]["column"] = "z";
	element["columns"][3]["type"] = "text";
	element["columns"][3]["font"] = "SANSSERIF";

	for (std::map<std::string, LLVector3>::const_iterator
			it = data.mPosOverrides.begin(), end = data.mPosOverrides.end();
		 it != end; ++it)
	{
		element["id"] = it->first;
		element["columns"][0]["value"] = it->first;
		if (upload_joint_pos)
		{
			element["columns"][1]["value"] = llformat("%f", it->second.mV[VX]);
			element["columns"][2]["value"] = llformat("%f", it->second.mV[VY]);
			element["columns"][3]["value"] = llformat("%f", it->second.mV[VZ]);
		}
		else
		{
			element["columns"][1]["value"] = "-";
			element["columns"][2]["value"] = "-";
			element["columns"][3]["value"] = "-";
		}
		self->mJointsOverrides->addElement(element);
	}

	element["columns"][1]["value"] = "-";
	element["columns"][2]["value"] = "-";
	element["columns"][3]["value"] = "-";
	for (std::set<std::string>::const_iterator
			it = data.mModelsNoOverrides.begin(),
			end = data.mModelsNoOverrides.end();
		 it != end; ++it)
	{
		element["id"] = *it;
		element["columns"][0]["value"] = *it;
		self->mJointsOverrides->addElement(element);
	}
}

//static
void LLFloaterModelPreview::onMouseCaptureLostModelPreview(LLMouseHandler*)
{
	gViewerWindowp->showCursor();
}

//static
void LLFloaterModelPreview::onBrowseLOD(void* userdata)
{
	LLFloaterModelPreview* self = findInstance();
	if (self)
	{
		assert_main_thread();
		S32 lod = (S32)(intptr_t)userdata - 1;
		self->loadModel(lod);
	}
}

//static
void LLFloaterModelPreview::onReset(void* userdata)
{
	LLFloaterModelPreview* self = (LLFloaterModelPreview*)userdata;
	if (!self) return;

	LLModelPreview* mp = self->mModelPreview;
	if (!mp) return;

	assert_main_thread();

	self->clearLog();
	self->childDisable("reset_btn");

	// Make sure the physics LOD combo is reset.
	LLComboBox* phys_combop = self->getChild<LLComboBox>("physics_lod_combo");
	LLScrollListItem* itemp = phys_combop->getItemByIndex(0);
	if (itemp)
	{
		itemp->setEnabled(true);
	}
	phys_combop->setCurrentByIndex(0);

	std::string filename = mp->mLODFile[LLModel::LOD_HIGH];

	self->resetDisplayOptions();
	// Reset model preview
	self->initModelPreview();

	phys_combop->selectFirstItem();
	self->childSetText("physics_file", "");

	// Refesh from new model preview
	mp = self->mModelPreview;
	mp->loadModel(filename, LLModel::LOD_HIGH, true);
}

//static
void LLFloaterModelPreview::onUpload(void* userdata)
{
	LLFloaterModelPreview* self = (LLFloaterModelPreview*)userdata;
	if (!self || !self->mModelPreview) return;

	assert_main_thread();

	self->clearLog();
	self->mUploadBtn->setEnabled(false);
	self->mSentUploadRequest = true;

	self->mModelPreview->rebuildUploadData();

	bool upload_skinweights = self->childGetValue("upload_skin").asBoolean();
	bool upload_joint_pos = self->childGetValue("upload_joints").asBoolean();
	bool lock_scale_if_joint_pos =
		self->childGetValue("lock_scale_if_joint_position").asBoolean();
	if (gSavedSettings.getBool("MeshImportUseSLM"))
	{
		self->mModelPreview->saveUploadData(upload_skinweights,
											upload_joint_pos,
											lock_scale_if_joint_pos);
	}

	gMeshRepo.uploadModel(self->mModelPreview->mUploadData,
						  self->mModelPreview->mPreviewScale,
						  self->childGetValue("upload_textures").asBoolean(),
						  upload_skinweights, upload_joint_pos,
						  lock_scale_if_joint_pos, self->mUploadModelUrl,
						  true, LLHandle<LLWholeModelFeeObserver>(),
						  self->getWholeModelUploadObserverHandle());
}

void LLFloaterModelPreview::setStatusMessage(const std::string& msg)
{
	mStatusLock->lock();
	mStatusMessage = msg;
	mStatusLock->unlock();
}

//static
void LLFloaterModelPreview::toggleCalculateButtonCallBack(LLUICtrl*,
														  void* userdata)
{
	LLFloaterModelPreview* self = (LLFloaterModelPreview*)userdata;
	if (self)
	{
		self->toggleCalculateButton(true);
	}
}

void LLFloaterModelPreview::toggleCalculateButton(bool visible)
{
	mCalculateBtn->setVisible(visible);

	if (childGetValue("upload_skin").asBoolean())
	{
		// Disable the calculate button *if* the rig is invalid, which is
		// determined during the critiquing process
		if (childGetValue("upload_joints").asBoolean() && mModelPreview &&
			!mModelPreview->isRigValidForJointPositionUpload())
		{
			mCalculateBtn->setEnabled(false);
		}
	}

	mUploadBtn->setVisible(!visible);
	mUploadBtn->setEnabled(isModelUploadAllowed());

	if (visible)
	{
		std::string tbd = getString("tbd");
		childSetTextArg("prim_weight", "[EQ]", tbd);
		childSetTextArg("download_weight", "[ST]", tbd);
		childSetTextArg("server_weight", "[SIM]", tbd);
		childSetTextArg("physics_weight", "[PH]", tbd);
		childSetToolTip("physics_weight", "");
		if (!mModelPhysicsFee.size() || !mModelPhysicsFee.isMap())
		{
			childSetTextArg("upload_fee", "[FEE]", tbd);
			childSetToolTip("upload_fee", "");
		}
	}
}

void LLFloaterModelPreview::modelUpdated(bool visible)
{
	mModelPhysicsFee.clear();
	toggleCalculateButton(visible);
}

//static
void LLFloaterModelPreview::onLoDSourceCommit(LLUICtrl*, void* userdata)
{
	LLFloaterModelPreview* self = findInstance();
	if (self && self->mModelPreview)
	{
		S32 lod = (S32)(intptr_t)userdata;
		self->mModelPreview->updateLodControls(lod);
		self->refresh();
		std::string cname = "lod_source_" + lod_name[lod];
		LLComboBox* lod_combo = self->getChild<LLComboBox>(cname.c_str());
		S32 index = lod_combo->getCurrentIndex();
		if (index >= LLModelPreview::GENERATE &&
			index < LLModelPreview::USE_LOD_ABOVE)
		{
			// Rebuild LoD to update triangle counts
			onLODParamCommitEnforceTriLimit(lod_combo, (void*)(intptr_t)lod);
		}
	}
}

//static
void LLFloaterModelPreview::onClickValidateURL(void* userdata)
{
	LLFloaterModelPreview* self = (LLFloaterModelPreview*)userdata;
	if (self && !self->mValidateURL.empty())
	{
		LLWeb::loadURLExternal(self->mValidateURL);
	}
}

void LLFloaterModelPreview::resetDisplayOptions()
{
	if (!mModelPreview) return;

	std::map<std::string, bool>::iterator option_it =
		mModelPreview->mViewOption.begin();
	for ( ; option_it != mModelPreview->mViewOption.end(); ++option_it)
	{
		LLUICtrl* ctrl = getChild<LLUICtrl>(option_it->first.c_str());
		ctrl->setValue(false);
	}
}

//virtual
void LLFloaterModelPreview::onModelPhysicsFeeReceived(const LLSD& result,
													  std::string upload_url)
{
	mModelPhysicsFee = result;
	mModelPhysicsFee["url"] = upload_url;

	doOnIdleOneTime(boost::bind(&LLFloaterModelPreview::handleModelPhysicsFeeReceived,
								this));
}

void LLFloaterModelPreview::handleModelPhysicsFeeReceived()
{
	const LLSD& result = mModelPhysicsFee;
	mUploadModelUrl = result["url"].asString();
	mSentFeeRequest = false;

	childSetTextArg("prim_weight", "[EQ]",
					llformat("%0.3f", result["resource_cost"].asReal()));
	childSetTextArg("download_weight", "[ST]",
					llformat("%0.3f", result["model_streaming_cost"].asReal()));
	childSetTextArg("server_weight", "[SIM]",
					llformat("%0.3f", result["simulation_cost"].asReal()));
	childSetTextArg("physics_weight", "[PH]",
					llformat("%0.3f", result["physics_cost"].asReal()));
	childSetTextArg("upload_fee", "[FEE]",
					llformat("%d", result["upload_price"].asInteger()));

	if (result.has("upload_price_breakdown"))
	{
		const LLSD& price = result["upload_price_breakdown"];
		LLUIString tooltip = getString("price_breakdown");
		tooltip.setArg("[STREAMING]",
					   llformat("%d", price["mesh_streaming"].asInteger()));
		tooltip.setArg("[PHYSICS]",
					   llformat("%d", price["mesh_physics"].asInteger()));
		tooltip.setArg("[INSTANCES]",
					   llformat("%d", price["mesh_instance"].asInteger()));
		tooltip.setArg("[TEXTURES]",
					   llformat("%d", price["texture"].asInteger()));
		tooltip.setArg("[MODEL]",
					   llformat("%d", price["model"].asInteger()));
		childSetToolTip("upload_fee", tooltip.getString());
	}

	if (result.has("model_physics_cost"))
	{
		const LLSD& costs = result["model_physics_cost"];
		LLUIString tooltip = getString("physics_breakdown");
		tooltip.setArg("[PCH]", llformat("%d", costs["hull"].asInteger()));
		tooltip.setArg("[PM]", llformat("%d", costs["mesh"].asInteger()));
		tooltip.setArg("[PHU]",
					   llformat("%d", costs["decomposition"].asInteger()));
		childSetToolTip("physics_weight", tooltip.getString());
	}

	// "Streaming breakdown numbers are available but not fully understood"...
	// Dixit LL, and these numbers are not shown in their viewer. Kept as a
	// debug message only. HB
	if (result.has("streaming_cost") && result.has("streaming_params"))
	{
		const LLSD& sp = result["streaming_params"];
		LL_DEBUGS("MeshUpload") << "Streaming cost breakdown: total = "
								<< result["streaming_cost"].asInteger()
								<< ", LOD3 = " << sp["high_lod"].asInteger()
								<< ", LOD2 = " << sp["medium_lod"].asInteger()
								<< ", LOD1 = " << sp["low_lod"].asInteger()
								<< ", LOD0 = " << sp["lowest_lod"].asInteger()
								<< LL_ENDL;
	}
		
	mUploadBtn->setEnabled(isModelUploadAllowed());
}

//virtual
void LLFloaterModelPreview::setModelPhysicsFeeErrorStatus(S32 status,
														  const std::string& reason,
														  const LLSD& result)
{
	llwarns << "LLFloaterModelPreview::setModelPhysicsFeeErrorStatus(" << status
			<< " : " << reason << ")" << llendl;
	mSentFeeRequest = false;
	doOnIdleOneTime(boost::bind(&LLFloaterModelPreview::toggleCalculateButton,
								this, true));
	if (result.has("upload_price"))
	{
		mModelPhysicsFee = result;
		childSetTextArg("upload_fee", "[FEE]",
						llformat("%d", result["upload_price"].asInteger()));
	}
	else
	{
		mModelPhysicsFee.clear();
	}
}

//virtual
void LLFloaterModelPreview::onModelUploadSuccess()
{
	mSentUploadRequest = false;
	assert_main_thread();
	close();
}

//virtual
void LLFloaterModelPreview::onModelUploadFailure()
{
	assert_main_thread();
	mSentUploadRequest = false;
	toggleCalculateButton(true);
	mUploadBtn->setEnabled(true);
}

bool LLFloaterModelPreview::isModelUploadAllowed()
{
	bool allow_upload = mHasUploadPerm && !mUploadModelUrl.empty();
	if (mModelPreview)
	{
		allow_upload &= mModelPreview->mModelNoErrors;
	}
	LL_DEBUGS("MeshUpload") << "mHasUploadPerm = " << mHasUploadPerm
							<< " - mUploadModelUrl = " << mUploadModelUrl
							<< " - mModelNoErrors = "
							<< (mModelPreview ? mModelPreview->mModelNoErrors : -1)
							<< LL_ENDL;
	return allow_upload;
}

//-----------------------------------------------------------------------------
// LLFloaterModelPreview::DecompRequest
//-----------------------------------------------------------------------------

LLFloaterModelPreview::DecompRequest::DecompRequest(const std::string& stage,
													LLModel* mdl)
{
	LLFloaterModelPreview* self = findInstance();
	if (self && mdl)
	{
		mStage = stage;
		mContinue = 1;
		mModel = mdl;
		mDecompID = &mdl->mDecompID;
		mParams = self->mDecompParams;

		// Copy out positions and indices
		assignData(mdl);
	}
}

S32 LLFloaterModelPreview::DecompRequest::statusCallback(const char* status,
														 S32 p1, S32 p2)
{
	if (mContinue)
	{
		setStatusMessage(llformat("%s: %d/%d", status, p1, p2));
	}

	return mContinue;
}

// Called from the main thread
void LLFloaterModelPreview::DecompRequest::completed()
{
	LLFloaterModelPreview* self = LLFloaterModelPreview::findInstance();
	if (mContinue)
	{
		mModel->setConvexHullDecomposition(mHull);

		if (self)
		{
			if (mContinue)
			{
				if (self->mModelPreview)
				{
					self->mModelPreview->mDirty = true;
					self->mModelPreview->refresh();
				}
			}

			self->mCurRequest.erase(this);
		}
	}
	else if (self)
	{
		llassert(self->mCurRequest.find(this) == self->mCurRequest.end());
	}
}

//-----------------------------------------------------------------------------
// LLModelPreview
//-----------------------------------------------------------------------------

LLModelPreview::LLModelPreview(S32 width, S32 height,
							   LLFloaterModelPreview* fmp)
:	LLViewerDynamicTexture(width, height, 3, ORDER_MIDDLE, false),
	mFMP(fmp),
	mModelLoader(NULL),
	mDefaultPhysModel(NULL),
	mPreviewLOD(0),
	mMaxTriangleLimit(0),
	mTextureName(0),
	mGroup(0),
	mPelvisZOffset(0.f),
	mCameraZoom(1.f),
	mCameraDistance(0.f),
	mCameraYaw(0.f),
	mCameraPitch(0.f),
	mBuildShareTolerance(0.f),
	mBuildQueueMode(GLOD_QUEUE_GREEDY),
	mBuildBorderMode(GLOD_BORDER_UNLOCK),
	mBuildOperator(GLOD_OPERATOR_EDGE_COLLAPSE),
	mLegacyRigFlags(LEGACY_RIG_FLAG_INVALID),
	mPhysicsSearchLOD(LLModel::LOD_PHYSICS),
	mLoadState(LLModelLoader::STARTING),
	mLoading(false),
	mNeedsUpdate(true),
	mDirty(false),
	mGenLOD(false),
	mLODFrozen(false),
	mHasPivot(false),
	mRigValidJointUpload(false),
	mFirstSkinUpdate(true),
	mResetJoints(false),
	mLastJointUpdate(false),
	mHasDegenerate(false),
	mWarnPhysModel(false),
	mImporterDebug(LLCachedControl<bool>(gSavedSettings, "MeshImporterDebug"))
{
	for (U32 i = 0; i < LLModel::NUM_LODS; ++i)
	{
		mRequestedTriangleCount[i] = 0;
		mRequestedCreaseAngle[i] = -1.f;
		mRequestedLoDMode[i] = 0;
		mRequestedErrorThreshold[i] = 0.f;
		mRequestedBuildOperator[i] = 0;
		mRequestedQueueMode[i] = 0;
		mRequestedBorderMode[i] = 0;
		mRequestedShareTolerance[i] = 0.f;
	}

	mViewOption["show_textures"] = false;

	glodInit();

	createPreviewAvatar();
}

LLModelPreview::~LLModelPreview()
{
	if (mModelLoader)
	{
		mModelLoader->shutdown();
		mModelLoader = NULL;
	}

	if (mPreviewAvatar)
	{
		mPreviewAvatar->markDead();
	}

	mUploadData.clear();
	mTextureSet.clear();

	for (S32 i = 0; i < LLModel::NUM_LODS; ++i)
	{
		clearModel(i);
	}
	mBaseModel.clear();
	mBaseScene.clear();

	// Must call clearGLODGroup() before shutting GLOD down, else we get
	// crashes later on in LLVOCachePartition/LLOctreeNode ! HB
	clearGLODGroup();
	// Note: I fixed GLOD to avoid yet another crash when calling this... HB
	glodShutdown();
}

void LLModelPreview::updateDimentionsAndOffsets()
{
	assert_main_thread();

	if (!mFMP)
	{
		return;
	}

	rebuildUploadData();

	mPelvisZOffset = mFMP->childGetValue("pelvis_offset").asReal();
	if (mPreviewAvatar && mFMP->childGetValue("upload_joints").asBoolean())
	{
		// *FIXME: if preview avatar ever gets reused, this fake mesh Id stuff
		// will fail. See also call to addAttachmentPosOverride().
		LLUUID fake_mesh_id;
		fake_mesh_id.generate();
		mPreviewAvatar->addPelvisFixup(mPelvisZOffset, fake_mesh_id);
	}

	std::set<LLModel*> accounted;
	for (U32 i = 0; i < mUploadData.size(); ++i)
	{
		LLModelInstance& instance = mUploadData[i];
		if (accounted.find(instance.mModel) == accounted.end())
		{
			accounted.insert(instance.mModel);
			// Update instance skin info for each lods pelvisZoffset
			for (S32 j = 0; j < LLModel::NUM_LODS; ++j)
			{
				if (instance.mLOD[j])
				{
					instance.mLOD[j]->mSkinInfo.mPelvisOffset = mPelvisZOffset;
				}
			}
		}
	}

	F32 scale = mFMP->childGetValue("import_scale").asReal() * 2.f;
	mDetailsSignal(mPreviewScale[0] * scale, mPreviewScale[1] * scale,
				   mPreviewScale[2] * scale);

	updateStatusMessages();
}

bool LLModelPreview::matchMaterialOrder(LLModel* lod, LLModel* ref,
										S32& ref_face_cnt, S32& model_face_cnt)
{
	// Is this a subset ?
	// LODs cannot currently add new materials, e.g.
	// 1. ref = a,b,c lod1 = d,e => This is not permitted
	// 2. ref = a,b,c lod1 = c => This would be permitted
	if (!lod->isMaterialListSubset(ref))
	{
		std::ostringstream out;
		out << "Materials of LOD model '" << lod->mLabel
			<< "' are not a subset of the High LOD (reference) model '"
			<< ref->mLabel << "'";
		llwarns << out.str() << llendl;
		mFMP->addLineToLog(out.str());
		return false;
	}

	LL_DEBUGS("MeshUpload") << "Subset check passed." << LL_ENDL;

	// Build a map of material slot names to face indexes
	std::map<std::string, U32> index_map;
	bool reorder = false;
	auto max_lod_mats =  lod->mMaterialList.size();
	for (U32 i = 0, count = ref->mMaterialList.size(); i < count; ++i)
	{
		// Create the reference map for later
		index_map[ref->mMaterialList[i]] = i;
		LL_DEBUGS("MeshUpload") << "Setting reference material "
								<< ref->mMaterialList[i] << " as index " << i
								<< LL_ENDL;
		if (i >= max_lod_mats ||
			lod->mMaterialList[i] != ref->mMaterialList[i])
		{
			// i is already out of range of the original material sets in this
			// LOD or is not matching.
			LL_DEBUGS("MeshUpload") << "Mismatch at " << i << " "
									<< ref->mMaterialList[i] << " != "
									<< (i >= max_lod_mats ? "Out-of-range"
														  : lod->mMaterialList[i])
									<< LL_ENDL;

			// We have a misalignment/ordering; check that ref[i] is in cur and
			// if not add a blank.
			U32 j = 0;
			for ( ; j < max_lod_mats; ++j)
			{
				if (i != j && lod->mMaterialList[j] == ref->mMaterialList[i])
				{
					LL_DEBUGS("MeshUpload") << "Material "
											<< ref->mMaterialList[i]
											<< " found at " << j << LL_ENDL;
					// We found it but in the wrong place.
					reorder = true;
					break;
				}
			}
			if (j >= max_lod_mats)
			{
				std::ostringstream out;
				out << "Material " << ref->mMaterialList[i]
					<< " not found in lod adding placeholder.";
				LL_DEBUGS("MeshUpload") << out.str() << LL_ENDL;
				if (mImporterDebug)
				{
					mFMP->addLineToLog(out.str());
				}
				// The material is not in the sub-mesh, add a placeholder.
				// This is appended to the existing data so we will need to
				// reorder. Note that this placeholder will be eliminated on
				// upload and replaced with "NoGeometry" in the LLSD.
				reorder = true;
				LLVolumeFace face;

				face.resizeIndices(3);
				face.resizeVertices(1);
				face.mPositions->clear();
				face.mNormals->clear();
				face.mTexCoords->clear();
				memset((void*)face.mIndices, 0, sizeof(U16)*3);
				lod->addFace(face);
				lod->mMaterialList.push_back(ref->mMaterialList[i]);
			}
		}
		// If any material name does not match reference, we need to reorder
	}

	LL_DEBUGS("MeshUpload") << "Finished parsing materials";
	for (U32 i = 0, count = lod->mMaterialList.size(); i < count; ++i)
	{
		LL_CONT << "LOD material " << lod->mMaterialList[i] << " has index "
				<< i;
	}
	LL_CONT << LL_ENDL;

	// Sanity check. We have added placeholders for any mats in ref that are
	// not in this. The mat count MUST be equal now.
	if (lod->mMaterialList.size() != ref->mMaterialList.size())
	{
		std::ostringstream out;
		out << "Materials of LOD model '" << lod->mLabel
			<< "' has more materials than the reference '"
			<< ref->mLabel << "'";
		llwarns << out.str() << llendl;
		mFMP->addLineToLog(out.str());
		return false;
	}

	if (reorder)
	{
		LL_DEBUGS("MeshUpload") << "Re-ordering." << LL_ENDL;
		lod->sortVolumeFacesByMaterialName();
		lod->mMaterialList = ref->mMaterialList;
	}

	return true;
}

void LLModelPreview::rebuildUploadData()
{
	assert_main_thread();

	if (!mFMP)
	{
		llwarns << "Model Preview floater is gone !  Aborted." << llendl;
		return;
	}

	mUploadData.clear();
	mTextureSet.clear();

	// Fill uploaddata instance vectors from scene data

	std::string requested_name =
		mFMP->getChild<LLUICtrl>("description_form")->getValue().asString();

	LLSpinCtrl* scale_spinner = mFMP->getChild<LLSpinCtrl>("import_scale");

	F32 scale = scale_spinner->getValue().asReal();
	LLMatrix4 scale_mat;
	scale_mat.initScale(LLVector3(scale, scale, scale));

	F32 max_scale = 0.f;

	U32 load_state = 0;
	mFMP->mCalculateBtn->setEnabled(true);

	bool legacy_matching = gSavedSettings.getBool("ImporterLegacyMatching");

	for (LLModelLoader::scene::iterator iter = mBaseScene.begin();
		 iter != mBaseScene.end(); ++iter)
	{
		// For each transform in scene
		LLMatrix4 mat = iter->first;

		// Compute position
		LLVector3 position = LLVector3::zero * mat;

		// Compute scale
		LLVector3 x_tf = LLVector3::x_axis * mat - position;
		LLVector3 y_tf = LLVector3::y_axis * mat - position;
		LLVector3 z_tf = LLVector3::z_axis * mat - position;
		F32 x_length = x_tf.normalize();
		if (x_length > max_scale)
		{
			max_scale = x_length;
		}
		F32 y_length = y_tf.normalize();
		if (y_length > max_scale)
		{
			max_scale = y_length;
		}
		F32 z_length = z_tf.normalize();
		if (z_length > max_scale)
		{
			max_scale = z_length;
		}

		mat *= scale_mat;

		for (LLModelLoader::model_instance_list_t::iterator
				mit = iter->second.begin();
			 mit != iter->second.end(); )
		{
			// For each instance with said transform applied
			LLModelInstance instance = *mit++;

			LLModel* base_model = instance.mModel;

			if (base_model && !requested_name.empty())
			{
				base_model->mRequestedLabel = requested_name;
			}

			for (S32 i = LLModel::NUM_LODS - 1; i >= LLModel::LOD_IMPOSTOR;
				 --i)
			{
				LLModel* lod_model = NULL;
				if (!legacy_matching)
				{
					// Fill LOD slots by finding matching meshes by label with
					// name extensions in the appropriate scene for each LOD.
					// This fixes all kinds of issues where the indexed method
					// below fails in spectacular fashion. If you do not take
					// the time to name your LOD and PHYS meshes with the name
					// of their corresponding mesh in the HIGH LOD, then the
					// indexed method will be attempted below.

					std::string name_to_match = instance.mLabel;
					llassert(!name_to_match.empty());

					S32 extension_lod;
					if (i != LLModel::LOD_PHYSICS ||
						mModel[LLModel::LOD_PHYSICS].empty())
					{
						extension_lod = i;
					}
					else
					{
						// Physics can be inherited from other LODs or loaded,
						// so we need to adjust what extension we are searching
						// for
						extension_lod = mPhysicsSearchLOD;
					}

					std::string suffix = get_lod_suffix(extension_lod);
					if (name_to_match.find(suffix) == std::string::npos)
					{
						name_to_match += suffix;
					}

					LLMatrix4 transform;
					find_model(mScene[i], name_to_match, lod_model, transform);

					if (!lod_model && i != LLModel::LOD_PHYSICS)
					{
						if (mImporterDebug)
						{
							std::ostringstream out;
							out << "Search of " << name_to_match
								<< " in LOD" << i
								<< " list failed. Searching for alternative among LOD lists.";
							llinfos << out.str() << llendl;
							mFMP->addLineToLog(out.str());
						}

						S32 search_lod = i > LLModel::LOD_HIGH ? LLModel::LOD_HIGH
															   : i;
						while (search_lod <= LLModel::LOD_HIGH && !lod_model)
						{
							std::string name_to_match = instance.mLabel;
							llassert(!name_to_match.empty());

							suffix = get_lod_suffix(search_lod);
							if (name_to_match.find(suffix) == std::string::npos)
							{
								name_to_match += suffix;
							}

							// See if we can find an appropriately named model
							// in LOD 'search_lod'
							find_model(mScene[search_lod], name_to_match,
									   lod_model, transform);
							++search_lod;
						}
					}
				}
				else
				{
					// Use old method of index-based association
					S32 idx = 0;
					S32 count = mBaseModel.size();
					for (idx = 0; idx < count; ++idx)
					{
						// Find reference instance for this model
						if (mBaseModel[idx] == base_model)
						{
							if (mImporterDebug)
							{
								std::ostringstream out;
								out << "Attempting to use model index "
									<< idx << " for LOD " << i << " of "
									<< instance.mLabel;
								llinfos << out.str() << llendl;
								mFMP->addLineToLog(out.str());
							}
							break;
						}
					}

					// If the model list for the current LOD includes that index...
					if ((S32)mModel[i].size() > idx)
					{
						// Assign that index from the model list for our LOD as
						// the LOD model for this instance
						lod_model = mModel[i][idx];
						if (mImporterDebug)
						{
							std::ostringstream out;
							out << "Indexed match of model index " << idx
								<< " at LOD " << i << " to model named "
								<< lod_model->mLabel;
							llinfos << out.str() << llendl;
							mFMP->addLineToLog(out.str());
						}
					}
					else if (mImporterDebug)
					{
						std::ostringstream out;
						out << "List of models does not include index " << idx;
						llinfos << out.str() << llendl;
						mFMP->addLineToLog(out.str());
					}
				}

				if (!lod_model && i == LLModel::LOD_PHYSICS && mWarnPhysModel)
				{
					// Despite the various strategies above, if we do not now
					// have a physics model, we are going to end up with the
					// decomposition. It is OK, but might not be what they
					// wanted. Use default_physics_shape instead if found.
					std::ostringstream out;
					out << "No physics model specified for: "
						<< instance.mLabel;
					if (mDefaultPhysModel)
					{
						out << ". Using: " << DEFAULT_PHYSICS_MESH_NAME;
						lod_model = mDefaultPhysModel;
					}
					llwarns << out.str() << llendl;
					// Flash log tab if no default available.
					mFMP->addLineToLog(out.str(), !mDefaultPhysModel);
				}

				if (lod_model)
				{
					if (mImporterDebug)
					{
						std::ostringstream out;
						if (i == LLModel::LOD_PHYSICS)
						{
							out << "Assigning collision for "
								<< instance.mLabel << " to match "
								<< lod_model->mLabel;
						}
						else
						{
							out << "Assigning LOD" << i << " for "
								<< instance.mLabel << " to found match "
								<< lod_model->mLabel;
						}
						llinfos << out.str() << llendl;
						mFMP->addLineToLog(out.str());
					}
					instance.mLOD[i] = lod_model;
				}
				else
				{
					if (i < LLModel::LOD_HIGH && !lodsReady())
					{
						// Assign a placeholder from previous LOD until lod
						// generation is complete. Note: we might need to
						// assign it regardless of conditions like named search
						// does, to prevent crashes.
						instance.mLOD[i] = instance.mLOD[i + 1];
					}
					if (mImporterDebug)
					{
						std::ostringstream out;
						out << "List of models does not include "
							<< instance.mLabel;
						llinfos << out.str() << llendl;
						mFMP->addLineToLog(out.str());
					}
				}
			}

			LLModel* high_lod_model = instance.mLOD[LLModel::LOD_HIGH];
			if (!high_lod_model)
			{
				load_state = LLModelLoader::ERROR_HIGH_LOD_MODEL_MISSING;
				mFMP->mCalculateBtn->setEnabled(false);
				mFMP->addLineToLog("Model " + instance.mLabel +
								   " has no High Lod (LOD3).", true);
			}
			else
			{
				for (S32 i = 0; i < LLModel::NUM_LODS - 1; ++i)
				{
					if (!instance.mLOD[i])
					{
						llwarns << "NULL LOD" << i << " found !  Skipping."
								<< llendl;
						llassert(false);
						continue;
					}

					S32 ref_face_cnt = 0;
					S32 model_face_cnt = 0;
					if (!matchMaterialOrder(instance.mLOD[i], high_lod_model,
											ref_face_cnt, model_face_cnt))
					{
						load_state = LLModelLoader::ERROR_MATERIALS;
						mFMP->mCalculateBtn->setEnabled(false);
						mFMP->addLineToLog("Model " + instance.mLabel +
										   " has mismatching materials between LODs.",
										   true);
					}
				}
			}
			if (mFMP->childGetValue("upload_skin").asBoolean() &&
				high_lod_model->mSkinInfo.mJointNames.size() > 0)
			{
				const LLMatrix4& bind_shape_mat =
					high_lod_model->mSkinInfo.mBindShapeMatrix;
				LLQuaternion bind_rot =
					LLSkinningUtil::getUnscaledQuaternion(bind_shape_mat);
				LLQuaternion identity;
				if (!bind_rot.isEqualEps(identity, 0.01f))
				{
					std::ostringstream out;
					out << "Non-identity bind shape rotation matrix is: "
						<< bind_shape_mat << " - bind_rot = " << bind_rot;
					mFMP->addLineToLog(out.str(), true);
					llwarns << out.str() << llendl;
					load_state = LLModelLoader::WARNING_BIND_SHAPE_ORIENTATION;
				}
			}
			instance.mTransform = mat;
			mUploadData.push_back(instance);
		}
	}

	for (S32 lod = 0; lod < LLModel::NUM_LODS - 1; ++lod)
	{
		// Search for models that are not included into upload data. If we find
		// any, that means something we loaded is not a sub-model.
		for (S32 model_ind = 0, model_cnt = mModel[lod].size();
			 model_ind < model_cnt; ++model_ind)
		{
			bool found_model = false;
			for (LLMeshUploadThread::instance_list_t::iterator
					iter = mUploadData.begin(), end = mUploadData.end();
				 iter != end; ++iter)
			{
				LLModelInstance& instance = *iter;
				if (instance.mLOD[lod] == mModel[lod][model_ind])
				{
					found_model = true;
					break;
				}
			}
			if (!found_model && mModel[lod][model_ind] &&
				!mModel[lod][model_ind]->mSubmodelID)
			{
				if (mImporterDebug)
				{
					std::ostringstream out;
					out << "Model " << mModel[lod][model_ind]->mLabel
						<< " was not used; mismatching lod models.";
					llinfos << out.str() << llendl;
					mFMP->addLineToLog(out.str());
				}
				load_state = LLModelLoader::ERROR_MATERIALS;
				mFMP->mCalculateBtn->setEnabled(false);
			}
		}
	}

	F32 max_import_scale;
	if (max_scale > 0.f)
	{
		max_import_scale = (LLManipScale::maxPrimScale() - 0.1f) / max_scale;
	}
	else
	{
		max_import_scale = 1.f;
	}

	F32 max_axis = llmax(mPreviewScale.mV[0], mPreviewScale.mV[1]);
	max_axis = llmax(max_axis, mPreviewScale.mV[2]);
	max_axis *= 2.f;

	// Clamp scale so that total imported model bounding box is smaller than
	// 240m on a side
	max_import_scale = llmin(max_import_scale, 240.f / max_axis);

	scale_spinner->setMaxValue(max_import_scale);

	if (max_import_scale < scale)
	{
		scale_spinner->setValue(max_import_scale);
	}

	if (load_state)
	{
		// We Encountered an issue during this call
		setLoadState(load_state);
		//updateStatusMessages();
	}
	else //if (mModelNoErrors)
	{
		// No issue now: was there an issue last time ?
		load_state = getLoadState();
		if (load_state == LLModelLoader::ERROR_MATERIALS ||
			load_state == LLModelLoader::ERROR_HIGH_LOD_MODEL_MISSING ||
			load_state == LLModelLoader::ERROR_LOD_MODEL_MISMATCH ||
			load_state == LLModelLoader::WARNING_BIND_SHAPE_ORIENTATION)
		{
			// In these specific cases, this should now be fixed since no
			// error was detected at this call...
			setLoadState(LLModelLoader::DONE);
			//updateStatusMessages();
		}
	}
}

void LLModelPreview::saveUploadData(bool save_skinweights,
									bool save_joint_positions,
									bool lock_scale_if_joint_pos)
{
	if (!mLODFile[LLModel::LOD_HIGH].empty())
	{
		std::string filename = mLODFile[LLModel::LOD_HIGH];
		std::string slm_filename;
		if (LLModelLoader::getSLMFilename(filename, slm_filename))
		{
			saveUploadData(slm_filename, save_skinweights,
						   save_joint_positions, lock_scale_if_joint_pos);
		}
	}
}

void LLModelPreview::saveUploadData(const std::string& filename,
									bool save_skinweights,
									bool save_joint_positions,
									bool lock_scale_if_joint_pos)
{
	std::set<LLPointer<LLModel> > meshes;
	std::map<LLModel*, std::string> mesh_binary;

	LLModel::hull empty_hull;

	LLSD data;

	data["version"] = SLM_SUPPORTED_VERSION;
	if (!mBaseModel.empty())
	{
		data["name"] = mBaseModel[0]->getName();
	}

	S32 mesh_id = 0;

	// Build list of unique models and initialize local id
	for (U32 i = 0; i < mUploadData.size(); ++i)
	{
		LLModelInstance& instance = mUploadData[i];

		if (meshes.find(instance.mModel) == meshes.end())
		{
			instance.mModel->mLocalID = mesh_id++;
			meshes.emplace(instance.mModel);

			std::stringstream str;

			LLModel::Decomposition& decomp =
				instance.mLOD[LLModel::LOD_PHYSICS].notNull() ?
				instance.mLOD[LLModel::LOD_PHYSICS]->mPhysics :
				instance.mModel->mPhysics;

			LLModel::writeModel(str,
								instance.mLOD[LLModel::LOD_PHYSICS],
								instance.mLOD[LLModel::LOD_HIGH],
								instance.mLOD[LLModel::LOD_MEDIUM],
								instance.mLOD[LLModel::LOD_LOW],
								instance.mLOD[LLModel::LOD_IMPOSTOR],
								decomp, save_skinweights, save_joint_positions,
								lock_scale_if_joint_pos, false, true,
								instance.mModel->mSubmodelID);

			data["mesh"][instance.mModel->mLocalID] = str.str();
		}

		data["instance"][i] = instance.asLLSD();
	}

	llofstream out(filename.c_str(),
				   std::ios_base::out | std::ios_base::binary);
	if (out.is_open())
	{
		LLSDSerialize::toBinary(data, out);
		out.flush();
		out.close();
	}
	else
	{
		llwarns << "Could not open file '" << filename << "' for writing."
				<< llendl;
	}
}

void LLModelPreview::clearModel(S32 lod)
{
	if (lod >= 0 && lod <= LLModel::LOD_PHYSICS)
	{
		mVertexBuffer[lod].clear();
		mModel[lod].clear();
		mScene[lod].clear();
	}
}

// Gets all standard skeleton joints from the preview avatar.
void LLModelPreview::getJointAliases(JointMap& joint_map)
{
	if (!mPreviewAvatar)
	{
		joint_map.clear();
		return;
	}

	joint_map = mPreviewAvatar->getJointAliases();

	std::vector<std::string> joint_names;
	mPreviewAvatar->getSortedJointNames(1, joint_names);
	for (S32 i = 0, count = joint_names.size(); i < count; ++i)
	{
		const std::string& name = joint_names[i];
		joint_map[name] = name;
	}

	mPreviewAvatar->getSortedJointNames(2, joint_names);
	for (S32 i = 0, count = joint_names.size(); i < count; ++i)
	{
		const std::string& name = joint_names[i];
		joint_map[name] = name;
	}
}

void LLModelPreview::loadModel(std::string filename, S32 lod,
							   bool force_disable_slm, bool allow_preprocess)
{
	assert_main_thread();

	if (!mFMP)
	{
		llwarns << "Model Preview floater is gone !  Aborted." << llendl;
		return;
	}

	if (!gDirUtilp)
	{
		return;	// Viewer shutting down
	}

	LLMutexLock lock(this);

	if (lod < LLModel::LOD_IMPOSTOR || lod > LLModel::NUM_LODS - 1)
	{
		std::ostringstream out;
		out << "Invalid level of detail: " << lod;
		llwarns << out.str() << llendl;
		llassert(false);
		mFMP->addLineToLog(out.str());
		return;
	}

	bool init_decomp = mBaseModel.empty();

	// This triggers if you bring up the file selector and then hit CANCEL.
	// Just use the previous model (if any) and ignore that you brought up the
	// file selector.

	if (filename.empty())
	{
		if (init_decomp)
		{
			// This is the initial file picking. Close the whole floater if we
			// do not have a base model to show for high LOD.
			mFMP->close();
			mLoading = false;
		}
		return;
	}

	if (mModelLoader)
	{
		llwarns << "Incompleted model load operation pending." << llendl;
		return;
	}

	mLODFile[lod] = filename;

	if (lod == LLModel::LOD_HIGH)
	{
		clearGLODGroup();
	}

	std::map<std::string, std::string> joint_alias_map;
	getJointAliases(joint_alias_map);

	if (gDirUtilp->getExtension(filename) == "dae")
	{
		bool preprocess = allow_preprocess &&
						  gSavedSettings.getBool("ImporterPreprocessDAE");
		U32 model_limit = gSavedSettings.getU32("ImporterModelLimit");
		mModelLoader = new LLDAELoader(filename, lod, loadedCallback,
									   lookupJointByName, loadTextures,
									   stateChangedCallback, this,
									   mJointTransformMap, mJointsFromNode,
									   joint_alias_map,
									   LL_MAX_JOINTS_PER_MESH_OBJECT,
									   model_limit, preprocess);
	}
	else
	{
		mModelLoader = new LLGLTFLoader(filename, lod, loadedCallback,
										lookupJointByName, loadTextures,
										stateChangedCallback, this,
										mJointTransformMap, mJointsFromNode,
										joint_alias_map,
										LL_MAX_JOINTS_PER_MESH_OBJECT);
	}

	if (force_disable_slm)
	{
		mModelLoader->mTrySLM = false;
	}
	else
	{
		// Only try to load from slm if viewer is configured to do so and this
		// is the initial model load (not an LoD or physics shape).
		// Note: trying to re-use SLM files has never worked properly; in
		// particular, it tends to force the UI into strange checkbox options
		// which cannot be altered. So better keeping MeshImportUseSLM false...
		mModelLoader->mTrySLM = gSavedSettings.getBool("MeshImportUseSLM") &&
								mUploadData.empty();
	}

	mModelLoader->start();

	mFMP->childSetTextArg("status", "[STATUS]",
						  mFMP->getString("status_reading_file"));

	setPreviewLOD(lod);

	if (mLoadState >= LLModelLoader::ERROR_PARSING)
	{
		mFMP->mUploadBtn->setEnabled(false);
		mFMP->mCalculateBtn->setEnabled(false);
	}

	if (lod == mPreviewLOD)
	{
		std::string wname = "lod_file_" + lod_name[lod];
		mFMP->childSetText(wname.c_str(), mLODFile[lod]);
	}
	else if (lod == LLModel::LOD_PHYSICS)
	{
		mFMP->childSetText("physics_file", mLODFile[lod]);
	}

	// Pre-fill a default name for the uploaded model
	if (lod != LLModel::LOD_PHYSICS)
	{
		LLLineEditor* desc = mFMP->getChild<LLLineEditor>("description_form");
		desc->setValue(gDirUtilp->getBaseFileName(filename, true));
	}

	mFMP->open();
}

void LLModelPreview::setPhysicsFromLOD(S32 lod)
{
	assert_main_thread();
	if (!mFMP) return;

#if 0	// *TODO: find a way to get rid of the loaded physics decomp when
		// reverting to no LOD ("Choose" entry in the physics combo). The
		// following code is insufficient.
	if (lod < 0)
	{
		mPhysicsSearchLOD = LLModel::LOD_PHYSICS;
		mModel[LLModel::LOD_PHYSICS].clear();
		mScene[LLModel::LOD_PHYSICS].clear();
		mVertexBuffer[LLModel::LOD_PHYSICS].clear();
		mFMP->childSetText("physics_file", "");
		rebuildUploadData();
		refresh();
		updateStatusMessages();
	}
	else if (lod <= 3)
#else
	if (lod >= 0 && lod <= 3)
#endif
	{
		mPhysicsSearchLOD = lod;
		mModel[LLModel::LOD_PHYSICS] = mModel[lod];
		mScene[LLModel::LOD_PHYSICS] = mScene[lod];
		mLODFile[LLModel::LOD_PHYSICS].clear();
		mFMP->childSetText("physics_file", mLODFile[LLModel::LOD_PHYSICS]);
		mVertexBuffer[LLModel::LOD_PHYSICS].clear();
		rebuildUploadData();
		refresh();
		updateStatusMessages();
	}
}

void LLModelPreview::clearIncompatible(S32 lod)
{
	// Do not discard models if specified model is the physic rep
	if (lod == LLModel::LOD_PHYSICS)
	{
		return;
	}

	bool replaced_base_model = lod == LLModel::LOD_HIGH;

	// At this point we do not care about sub-models, different amount of
	// sub-models means face count mismatch, not incompatibility
	U32 lod_size = countRootModels(mModel[lod]);
	for (S32 i = 0; i <= LLModel::LOD_HIGH; ++i)
	{
		// Clear out any entries that are not compatible with this model
		if (i != lod)
		{
			if (countRootModels(mModel[i]) != lod_size)
			{
				mModel[i].clear();
				mScene[i].clear();
				mVertexBuffer[i].clear();

				if (i == LLModel::LOD_HIGH)
				{
					mBaseModel = mModel[lod];
					clearGLODGroup();
					mBaseScene = mScene[lod];
					mVertexBuffer[5].clear();
					replaced_base_model = true;
				}
			}
		}
	}

	if (!replaced_base_model || mGenLOD)
	{
		return;
	}

	// Remove any previously scheduled work
	mLodsQuery.clear();

	std::string cname;
	for (S32 i = LLModel::LOD_HIGH; i >= 0; --i)
	{
		if ((replaced_base_model && i != lod) ||
			(!replaced_base_model && mModel[i].empty()))
		{
			// Base model was replaced, regenerate this lod if applicable
			cname = "lod_source_" + lod_name[i];
			S32 lod_mode =
				mFMP->getChild<LLComboBox>(cname.c_str())->getCurrentIndex();
			if (lod_mode != LOD_FROM_FILE)
			{
				mLodsQuery.push_back(i);
			}
		}
	}

	if (!mLodsQuery.empty())
	{
		doOnIdleRepeating(lodQueryCallback);
	}
}

void LLModelPreview::clearGLODGroup()
{
	if (!mGroup) return;

	for (std::map<LLPointer<LLModel>, U32>::iterator it = mObject.begin(),
													 end = mObject.end();
			 it != end; ++it)
	{
		glodDeleteObject(it->second);
		stop_gloderror();
	}
	mObject.clear();

	glodDeleteGroup(mGroup);
	stop_gloderror();
	mGroup = 0;
}

void LLModelPreview::loadModelCallback(S32 lod)
{
	assert_main_thread();

	if (!mFMP)
	{
		llwarns << "Model Preview floater is gone !  Aborted." << llendl;
		return;
	}

	LLMutexLock lock(this);
	if (!mModelLoader)
	{
		mLoading = false;
		return;
	}

	const LLSD& log = mModelLoader->logOut();
	for (LLSD::array_const_iterator it = log.beginArray(),
		 							end = log.endArray();
		 it != end; ++it)
	{
		if (it->has("Message"))
		{
			mFMP->addMessageToLog(it->get("Message"), *it, lod, true);
		}
	}
	mModelLoader->clearLog();

	if (mLoadState >= LLModelLoader::ERROR_PARSING)
	{
		mLoading = false;
		mModelLoader = NULL;
		mLodsWithParsingError.push_back(lod);
		return;
	}

	mLodsWithParsingError.erase(std::remove(mLodsWithParsingError.begin(),
											mLodsWithParsingError.end(), lod),
								mLodsWithParsingError.end());
	if (mLodsWithParsingError.empty())
	{
		mFMP->mCalculateBtn->setEnabled(true);
	}

	// Copy determinations about rig so UI will reflect them
	mRigValidJointUpload = mModelLoader->isRigValidForJointPositionUpload();
	mLegacyRigFlags = mModelLoader->getLegacyRigFlags();

	mModelLoader->loadTextures();

	if (lod == -1)
	{
		// Populate all LoDs from model loader scene
		mBaseModel.clear();
		mBaseScene.clear();

		bool skin_weights = false;
		bool joint_positions = false;
		bool lock_scale_if_joint_pos = false;

		// For each LoD
		for (S32 lod = 0; lod < LLModel::NUM_LODS; ++lod)
		{
			// Clear scene and model info
			mScene[lod].clear();
			mModel[lod].clear();
			mVertexBuffer[lod].clear();

			if (mModelLoader->mScene.begin()->second[0].mLOD[lod].notNull())
			{
				// If this LoD exists in the loaded scene, copy scene to
				// current LoD
				mScene[lod] = mModelLoader->mScene;

				// Touch up copied scene to look like current LoD
				for (LLModelLoader::scene::iterator iter = mScene[lod].begin();
					 iter != mScene[lod].end(); ++iter)
				{
					LLModelLoader::model_instance_list_t& list = iter->second;

					for (LLModelLoader::model_instance_list_t::iterator
							list_iter = list.begin();
						 list_iter != list.end(); ++list_iter)
					{
						// Override displayed model with current LoD
						list_iter->mModel = list_iter->mLOD[lod];
						if (!list_iter->mModel) continue;

						// Add current model to current LoD's model list
						// (LLModel::mLocalID makes a good vector index)
						S32 idx = list_iter->mModel->mLocalID;
						if ((S32)mModel[lod].size() <= idx)
						{
							// Stretch model list to fit model at given index
							mModel[lod].resize(idx + 1);
						}

						mModel[lod][idx] = list_iter->mModel;
						if (!list_iter->mModel->mSkinWeights.empty())
						{
							skin_weights = true;

							if (!list_iter->mModel->mSkinInfo.mAlternateBindMatrix.empty())
							{
								joint_positions = true;
							}
							if (list_iter->mModel->mSkinInfo.mLockScaleIfJointPosition)
							{
								lock_scale_if_joint_pos = true;
							}
						}
					}
				}
			}
		}

		if (skin_weights)
		{
			// Enable uploading/previewing of skin weights if present in the
			// .slm file
			mFMP->enableViewOption("show_skin_weight");
			mViewOption["show_skin_weight"] = true;
			mFMP->childSetValue("upload_skin", true);
		}
		if (joint_positions)
		{
			mFMP->enableViewOption("show_joint_overrides");
			mViewOption["show_joint_overrides"] = true;
			mFMP->enableViewOption("show_joint_positions");
			mViewOption["show_joint_positions"] = true;
			mFMP->enableViewOption("show_collision_volumes");
			mViewOption["show_collision_volumes"] = true;
			mFMP->childSetValue("upload_joints", true);
		}
		if (lock_scale_if_joint_pos)
		{
			mFMP->enableViewOption("lock_scale_if_joint_position");
			mViewOption["lock_scale_if_joint_position"] = true;
			mFMP->childSetValue("lock_scale_if_joint_position", true);
		}

		// Copy high lod to base scene for LoD generation
		mBaseScene = mScene[LLModel::LOD_HIGH];
		mBaseModel = mModel[LLModel::LOD_HIGH];

		mDirty = true;
		resetPreviewTarget();
	}
	else
	{
		// Only replace given LoD
		mModel[lod] = mModelLoader->mModelList;
		mScene[lod] = mModelLoader->mScene;
		mVertexBuffer[lod].clear();

		setPreviewLOD(lod);

		if (lod == LLModel::LOD_HIGH)
		{
			// Save a copy of the highest LOD for automatic LOD manipulation
			if (mBaseModel.empty())
			{
				// First time we have loaded a model, auto-gen LoD
				mGenLOD = true;
			}

			mBaseModel = mModel[lod];
			clearGLODGroup();

			mBaseScene = mScene[lod];
			mVertexBuffer[5].clear();
		}
		else
		{
			LLMatrix4 t;	// For ignored transform matrix

			if (lod == LLModel::LOD_PHYSICS)
			{
				// Explicitly loading physics. See if there is a default mesh.
				mDefaultPhysModel = NULL;
				std::string name = DEFAULT_PHYSICS_MESH_NAME +
								   get_lod_suffix(lod);
				find_model(mScene[lod], name, mDefaultPhysModel, t);
				mWarnPhysModel = true;
			}

			if (!mBaseModel.empty() &&
				!gSavedSettings.getBool("ImporterLegacyMatching"))
			{
				bool name_based = false;
				bool has_submodels = false;
				for (S32 idx = 0, cnt = mBaseModel.size(); idx < cnt; ++idx)
				{
					if (mBaseModel[idx]->mSubmodelID)
					{
						// Do not do index-based renaming when the base model
						// has sub-models
						has_submodels = true;
						if (mImporterDebug)
						{
							llinfos << "High LOD has submodels" << llendl;
							mFMP->addLineToLog("High LOD has submodels");
						}
						break;
					}
				}

				for (S32 idx = 0, cnt = mModel[lod].size(); idx < cnt; ++idx)
				{
					std::string loaded_name =
						strip_lod_suffix(mModel[lod][idx]->mLabel);

					LLModel* found_model = NULL;
					find_model(mBaseScene, loaded_name, found_model, t);
					if (found_model)
					{
						// Do not rename correctly named models (even if they
						// are placed in a wrong order)
						name_based = true;
					}

					if (mModel[lod][idx]->mSubmodelID)
					{
						// Do not rename the models when loaded LOD model has
						// sub-models
						has_submodels = true;
					}
				}

				if (mImporterDebug)
				{
					std::ostringstream out;
					out << "Loaded LOD" << lod << ": correct names"
						<< (name_based ? "" : "NOT ") << "found; submodels "
						<< (has_submodels ? "" : "NOT ") << "found.";
					llinfos << out.str() << llendl;
					mFMP->addLineToLog(out.str());
				}

				if (!name_based && !has_submodels)
				{
					// Replace the name of the model loaded for any non HIGH
					// LOD to match the others (MAINT-5601); this actually
					// works like "ImporterLegacyMatching" for this particular
					// LOD
					std::string name, loaded_name;
					for (size_t idx = 0;
						 idx < mModel[lod].size() && idx < mBaseModel.size();
						 ++idx)
					{
						name = mBaseModel[idx]->mLabel;
						loaded_name =
							strip_lod_suffix(mModel[lod][idx]->mLabel);
						if (loaded_name != name)
						{
							name += get_lod_suffix(lod);

							if (mImporterDebug)
							{
								std::ostringstream out;
								out << "Loded model name "
									<< mModel[lod][idx]->mLabel
									<< " for LOD" << lod
									<< " does not match the base model. Renaming to "
									<< name;
								llinfos << out.str() << llendl;
								mFMP->addLineToLog(out.str());
							}

							mModel[lod][idx]->mLabel = name;
						}
					}
				}
			}
		}

		clearIncompatible(lod);

		mDirty = true;

		if (lod == LLModel::LOD_HIGH)
		{
			resetPreviewTarget();
		}
	}

	mLoading = false;

	if (!mBaseModel.empty())
	{
		// Add info to log that loading is complete (purpose: separator
		// between loading and other logs).
		LLSD args;
		args["MODEL_NAME"] = mBaseModel[0]->getName();
		mFMP->addMessageToLog("ModelLoaded", args, lod);
	}

	refresh();
	mModelLoadedSignal();

	mModelLoader = NULL;
}

void LLModelPreview::resetPreviewTarget()
{
	if (mModelLoader)
	{
		mPreviewTarget = (mModelLoader->mExtents[0] +
						  mModelLoader->mExtents[1]) * 0.5f;
		mPreviewScale = (mModelLoader->mExtents[1] -
						 mModelLoader->mExtents[0]) * 0.5f;
	}

	setPreviewTarget(mPreviewScale.length() * 10.f);
}

void LLModelPreview::generateNormals()
{
	assert_main_thread();

	S32 which_lod = mPreviewLOD;
	if (!mFMP || which_lod > 4 || which_lod < 0 || mModel[which_lod].empty())
	{
		return;
	}

	F32 angle_cutoff = mFMP->childGetValue("crease_angle").asReal();
	mRequestedCreaseAngle[which_lod] = angle_cutoff;
	angle_cutoff *= DEG_TO_RAD;

	if (which_lod == 3 && !mBaseModel.empty())
	{
		if (mBaseModelFacesCopy.empty())
		{
			mBaseModelFacesCopy.reserve(mBaseModel.size());
			for (LLModelLoader::model_list::iterator it = mBaseModel.begin(),
													 end = mBaseModel.end();
				 it != end; ++it)
			{
				v_LLVolumeFace_t faces;
				(*it)->copyFacesTo(faces);
				mBaseModelFacesCopy.emplace_back(faces);
			}
		}

		for (LLModelLoader::model_list::iterator it = mBaseModel.begin(),
												 end = mBaseModel.end();
			 it != end; ++it)
		{
			(*it)->generateNormals(angle_cutoff);
		}

		mVertexBuffer[5].clear();
	}

	bool perform_copy = mModelFacesCopy[which_lod].empty();
	if (perform_copy)
	{
		mModelFacesCopy[which_lod].reserve(mModel[which_lod].size());
	}

	for (LLModelLoader::model_list::iterator it = mModel[which_lod].begin(),
											 end = mModel[which_lod].end();
		 it != end; ++it)
	{
		if (perform_copy)
		{
			v_LLVolumeFace_t faces;
			(*it)->copyFacesTo(faces);
			mModelFacesCopy[which_lod].emplace_back(faces);
		}

		(*it)->generateNormals(angle_cutoff);
	}

	mVertexBuffer[which_lod].clear();
	refresh();
	updateStatusMessages();
}

void LLModelPreview::restoreNormals()
{
	S32 which_lod = mPreviewLOD;

	if (which_lod > 4 || which_lod < 0 || mModel[which_lod].empty())
	{
		return;
	}

	if (!mBaseModelFacesCopy.empty())
	{
		llassert(mBaseModelFacesCopy.size() == mBaseModel.size());

		vv_LLVolumeFace_t::const_iterator itf = mBaseModelFacesCopy.begin();
		for (LLModelLoader::model_list::iterator it = mBaseModel.begin(),
												 end = mBaseModel.end();
			 it != end; ++it, ++itf)
		{
			(*it)->copyFacesFrom((*itf));
		}

		mBaseModelFacesCopy.clear();
	}

	if (!mModelFacesCopy[which_lod].empty())
	{
		vv_LLVolumeFace_t::const_iterator itf = mModelFacesCopy[which_lod].begin();
		for (LLModelLoader::model_list::iterator
				it = mModel[which_lod].begin(), end = mModel[which_lod].end();
			 it != end; ++it, ++itf)
		{
			(*it)->copyFacesFrom((*itf));
		}

		mModelFacesCopy[which_lod].clear();
	}

	mVertexBuffer[which_lod].clear();
	refresh();
	updateStatusMessages();
}

// Helper class for GLOD setup and error recovery. HB
class HBGlodHelper
{
public:
	HBGlodHelper(LLModelPreview* previewp)
	:	mPreviewp(previewp),
		mShaderp(LLGLSLShader::sCurBoundShaderPtr)
	{
		LLVertexBuffer::unbind();
		if (mShaderp)
		{
			mShaderp->unbind();
		}
	}

	~HBGlodHelper()
	{
		LLVertexBuffer::unbind();
		if (mShaderp)
		{
			mShaderp->bind();
		}
		// *HACK: in case of error, cleanup GLOD and reinitialize it. HB
		if (sHasGlodError)
		{
			// Must call clearGLODGroup() before shutting GLOD down, else we
			// get crashes later on in LLVOCachePartition/LLOctreeNode ! HB
			mPreviewp->clearGLODGroup();
			// Note: I fixed GLOD to avoid yet another crash when calling
			// this... HB
			glodShutdown();
			// Re-initialize GLOD
			glodInit();
		}
	}

private:
	LLModelPreview*	mPreviewp;
	LLGLSLShader*	mShaderp;
};

// Helper function to setup the vertex buffer used by Nicky Dasmijn's modified
// (non-fixed GL functions) GLOD library. Returns true when successful, false
// otherwise. HB
bool setup_glob_vbo(glodVBO& vbo, LLVertexBuffer* buff,
					LLStrider<U16>& index_strider,
					LLStrider<LLVector3>& vertex_strider,
					LLStrider<LLVector3>& normal_strider,
					LLStrider<LLVector2>& tc_strider)
{
				
	if (!buff->getIndexStrider(index_strider))
	{
		llwarns << "Failed to get index strider. Aborted." << llendl;
		return false;
	}

	if (buff->hasDataType(LLVertexBuffer::TYPE_VERTEX))
	{
		if (!buff->getVertexStrider(vertex_strider))
		{
			llwarns << "Failed to get vertex strider. Aborted." << llendl;
			return false;
		}
		vbo.mV.p = vertex_strider.get();
		vbo.mV.size = 3;
		vbo.mV.stride = LLVertexBuffer::sTypeSize[LLVertexBuffer::TYPE_VERTEX];
		vbo.mV.type = GL_FLOAT;
	}
	if (buff->hasDataType(LLVertexBuffer::TYPE_NORMAL))
	{
		if (!buff->getNormalStrider(normal_strider))
		{
			llwarns << "Failed to get normal strider. Aborted." << llendl;
			return false;
		}
		vbo.mN.p = normal_strider.get();
		vbo.mN.stride = LLVertexBuffer::sTypeSize[LLVertexBuffer::TYPE_NORMAL];
		vbo.mN.type = GL_FLOAT;
	}
	if (buff->hasDataType(LLVertexBuffer::TYPE_TEXCOORD0))
	{
		if (!buff->getTexCoord0Strider(tc_strider))
		{
			llwarns << "Failed to get texcoord strider. Aborted." << llendl;
			return false;
		}
		vbo.mT.p = tc_strider.get();
		vbo.mT.size = 2;
		vbo.mT.stride =
			LLVertexBuffer::sTypeSize[LLVertexBuffer::TYPE_TEXCOORD0];
		vbo.mT.type = GL_FLOAT;
	}
	return true;
}

bool LLModelPreview::genGlodLODs(S32 which_lod, U32 decimation,
								 bool enforce_tri_limit)
{
	if (!mFMP)
	{
		llwarns << "Model Preview floater is gone !  Aborted." << llendl;
		return true; // Do not try the meshoptimizer method !
	}

	llinfos << "Generating lod " << which_lod << " using GLOD." << llendl;

	// Allow LoD from -1 to LLModel::LOD_PHYSICS
	if (which_lod < -1 || which_lod > LLModel::NUM_LODS - 1)
	{
		std::ostringstream out;
		out << "Invalid level of detail: " << which_lod;
		llwarns << out.str() << llendl;
		llassert(false);
		mFMP->addLineToLog(out.str());
		return true; // Do not try the meshoptimizer method !
	}

	if (mBaseModel.empty())
	{
		return true; // Do not try the meshoptimizer method !
	}

	stop_gloderror();

	HBGlodHelper helper(this);

	static U32 cur_name = 1;

	S32 limit = -1;

	U32 triangle_count = 0;

	U32 instanced_triangle_count = 0;

	// Get the triangle count for the whole scene
	for (LLModelLoader::scene::iterator iter = mBaseScene.begin(),
										endIter = mBaseScene.end();
		 iter != endIter; ++iter)
	{
		for (LLModelLoader::model_instance_list_t::iterator
				instance = iter->second.begin(),
				end_instance = iter->second.end();
			 instance != end_instance; ++instance)
		{
			LLModel* mdl = instance->mModel;
			if (mdl)
			{
				instanced_triangle_count += mdl->getNumTriangles();
			}
		}
	}

	// Get the triangle count for the non-instanced set of models
	for (U32 i = 0; i < mBaseModel.size(); ++i)
	{
		triangle_count += mBaseModel[i]->getNumTriangles();
	}

	// Get ratio of uninstanced triangles to instanced triangles
	F32 triangle_ratio = (F32)triangle_count / (F32)instanced_triangle_count;

	U32 base_triangle_count = triangle_count;

	U32 type_mask = LLVertexBuffer::MAP_VERTEX | LLVertexBuffer::MAP_NORMAL |
					LLVertexBuffer::MAP_TEXCOORD0;

	U32 lod_mode = LIMIT_TRIANGLES;

	F32 lod_err_thres = 0.f;

	// The LoD should be in range from Lowest to High
	if (which_lod > -1 && which_lod < NUM_LOD)
	{
		std::string cname = "lod_mode_" + lod_name[which_lod];
		lod_mode =
			mFMP->getChild<LLComboBox>(cname.c_str())->getCurrentIndex();
		cname = "lod_error_threshold_" + lod_name[which_lod];
		lod_err_thres = mFMP->childGetValue(cname.c_str()).asReal();
	}

	if (which_lod != -1)
	{
		mRequestedLoDMode[which_lod] = lod_mode;
	}

	if (lod_mode == LIMIT_TRIANGLES)
	{
		lod_mode = GLOD_TRIANGLE_BUDGET;

		// The LoD should be in range from Lowest to High
		if (which_lod > -1 && which_lod < NUM_LOD)
		{
			std::string wname = "lod_triangle_limit_" + lod_name[which_lod];
			limit = mFMP->childGetValue(wname.c_str()).asInteger();
			// Convert from "scene wide" to "non-instanced" triangle limit
			limit = (S32)((F32)limit * triangle_ratio);
		}
	}
	else
	{
		lod_mode = GLOD_ERROR_THRESHOLD;
	}

	bool object_dirty = false;

	if (mGroup == 0)
	{
		object_dirty = true;
		mGroup = cur_name++;
		glodNewGroup(mGroup);
	}

	if (object_dirty)
	{
		for (LLModelLoader::model_list::iterator iter = mBaseModel.begin();
			 iter != mBaseModel.end(); ++iter)
		{
			// Build GLOD objects for each model in base model list
			LLModel* mdl = *iter;

			if (mObject[mdl] != 0)
			{
				glodDeleteObject(mObject[mdl]);
			}

			mObject[mdl] = cur_name++;

			glodNewObject(mObject[mdl], mGroup, GLOD_DISCRETE);
			if (stop_gloderror("glodNewObject"))
			{
				return false;
			}

			if (iter == mBaseModel.begin() && !mdl->mSkinWeights.empty())
			{
				// Regenerate vertex buffer for skinned models to prevent
				// animation feedback during LOD generation
				mVertexBuffer[5].clear();
			}

			if (mVertexBuffer[5].empty())
			{
				genBuffers(5, false);
			}

			for (U32 i = 0; i < mVertexBuffer[5][mdl].size(); ++i)
			{
				LLVertexBuffer* buff = mVertexBuffer[5][mdl][i];
				buff->setBufferNoShader(type_mask & buff->getTypeMask());

				U32 num_indices = buff->getNumIndices();
				if (num_indices < 2)
				{
					continue;
				}

				// Vertex buffer based code for Nicky Dasmijn's modified
				// GLOD library.
				glodVBO vbo = {};
				LLStrider<LLVector3> vertex_strider, normal_strider;
				LLStrider<LLVector2> tc_strider;
				LLStrider<U16> index_strider;
				if (!setup_glob_vbo(vbo, buff, index_strider, vertex_strider,
									normal_strider, tc_strider))
				{
					return false;
				}
				glodInsertElements(mObject[mdl], i, GL_TRIANGLES, num_indices,
								   GL_UNSIGNED_SHORT,
								   (U8*)index_strider.get(), 0, 0.f, &vbo);
				if (stop_gloderror("glodInsertElements"))
				{
					return false;
				}
			}

			glodBuildObject(mObject[mdl]);
			if (stop_gloderror("glodBuildObject"))
			{
				return false;
			}
		}
	}

	mMaxTriangleLimit = base_triangle_count;

	S32 start = LLModel::LOD_HIGH;
	S32 end = 0;
	if (which_lod != -1)
	{
		start = end = which_lod;
	}
	for (S32 lod = start; lod >= end; --lod)
	{
		if (which_lod == -1)
		{
			if (lod < start)
			{
				triangle_count /= decimation;
			}
		}
		else
		{
			if (enforce_tri_limit)
			{
				triangle_count = limit;
			}
			else
			{
				for (S32 j = LLModel::LOD_HIGH; j > which_lod; --j)
				{
					triangle_count /= decimation;
				}
			}
		}

		mModel[lod].clear();
		mModel[lod].resize(mBaseModel.size());
		mVertexBuffer[lod].clear();

		mRequestedTriangleCount[lod] = F32(triangle_count) / triangle_ratio;
		mRequestedErrorThreshold[lod] = lod_err_thres;

		glodGroupParameteri(mGroup, GLOD_ADAPT_MODE, lod_mode);
		if (stop_gloderror("glodGroupParameteri - ADAPT_MODE"))
		{
			return false;
		}

		glodGroupParameteri(mGroup, GLOD_ERROR_MODE, GLOD_OBJECT_SPACE_ERROR);
		if (stop_gloderror("glodGroupParameteri - ERROR_MODE"))
		{
			return false;
		}

		glodGroupParameterf(mGroup, GLOD_OBJECT_SPACE_ERROR_THRESHOLD,
							lod_err_thres);
		if (stop_gloderror("glodGroupParameterf - SPACE_ERROR_THRESHOLD"))
		{
			return false;
		}

		if (lod_mode != GLOD_TRIANGLE_BUDGET)
		{
			glodGroupParameteri(mGroup, GLOD_MAX_TRIANGLES, 0);
		}
		else
		{
			// SH-632: always add 1 to desired amount to avoid decimating below
			// desired amount
			glodGroupParameteri(mGroup, GLOD_MAX_TRIANGLES,
								triangle_count + 1);
		}
		if (stop_gloderror("glodGroupParameterf - MAX_TRIANGLES"))
		{
			return false;
		}

		glodAdaptGroup(mGroup);
		if (stop_gloderror("glodAdaptGroup"))
		{
			return false;
		}

		for (U32 mdl_idx = 0; mdl_idx < mBaseModel.size(); ++mdl_idx)
		{
			LLModel* base = mBaseModel[mdl_idx];

			GLint patch_count = 0;
			glodGetObjectParameteriv(mObject[base], GLOD_NUM_PATCHES,
									 &patch_count);
			if (stop_gloderror("glodGetObjectParameteriv - NUM_PATCHES"))
			{
				return false;
			}

			LLVolumeParams volume_params;
			volume_params.setType(LL_PCODE_PROFILE_SQUARE, LL_PCODE_PATH_LINE);
			mModel[lod][mdl_idx] = new LLModel(volume_params, 0.f);

			std::string name = base->mLabel + get_lod_suffix(lod);
			mModel[lod][mdl_idx]->mLabel = name;
			mModel[lod][mdl_idx]->mSubmodelID = base->mSubmodelID;

			GLint* sizes = new GLint[patch_count * 2];
			glodGetObjectParameteriv(mObject[base], GLOD_PATCH_SIZES, sizes);
			if (stop_gloderror("glodGetObjectParameteriv - PATCH_SIZES"))
			{
				delete[] sizes;
				return false;
			}

			GLint* names = new GLint[patch_count];
			glodGetObjectParameteriv(mObject[base], GLOD_PATCH_NAMES, names);
			if (stop_gloderror("glodGetObjectParameteriv - PATCH_NAMES"))
			{
				delete[] sizes;
				delete[] names;
				return false;
			}

			mModel[lod][mdl_idx]->setNumVolumeFaces(patch_count);

			LLModel* target_model = mModel[lod][mdl_idx];

			for (GLint i = 0; i < patch_count; ++i)
			{
				type_mask = mVertexBuffer[5][base][i]->getTypeMask();

				LLPointer<LLVertexBuffer> buff = new LLVertexBuffer(type_mask);
				if (buff.isNull())
				{
					llwarns << "Failure to allocate a new vertex buffer !"
							<< llendl;
					delete[] sizes;
					delete[] names;
					return false;
				}

				if (sizes[i * 2 + 1] > 0 && sizes[i * 2] > 0)
				{
					if (!buff->allocateBuffer(sizes[i * 2 + 1], sizes[i * 2]))
					{
						llwarns << "Failed buffer allocation during preview "
								<< " LOD generation for " << sizes[i * 2 + 1]
								<< " vertices and " << sizes[i * 2]
								<< " indices" << llendl;
						delete[] sizes;
						delete[] names;
						mFMP->close();
						return true; // Do not try the meshoptimizer method !
					}
					buff->setBufferNoShader(type_mask);

					// Vertex buffer based code for Nicky Dasmijn's modified
					// GLOD library.
					glodVBO vbo = {};
					LLStrider<LLVector3> vertex_strider, normal_strider;
					LLStrider<LLVector2> tc_strider;
					LLStrider<U16> index_strider;
					if (!setup_glob_vbo(vbo, buff, index_strider, vertex_strider,
										normal_strider, tc_strider))
					{
						return true; // Do not try the meshoptimizer method !
					}
					glodFillElements(mObject[base], names[i],
									 GL_UNSIGNED_SHORT,
									 (U8*)index_strider.get(), &vbo);
					if (stop_gloderror("glodFillElements"))
					{
						delete[] names;
						delete[] sizes;
						return false;
					}
				}
				else
				{
					// This face was eliminated, create a dummy triangle (one
					// vertex, 3 indices, all 0)
					buff->allocateBuffer(1, 3);
					buff->resetVertexData();
					buff->resetIndexData();
					LLStrider<U16> index_strider;
					if (!buff->getIndexStrider(index_strider))
					{
						llwarns << "Failed to get index strider range, aborted !"
								<< llendl;
						delete[] names;
						delete[] sizes;
						return false;
					}
				}

				if (!buff->validateRange(0, buff->getNumVerts() - 1,
										 buff->getNumIndices(), 0))
				{
					llwarns << "Invalid range, aborted !" << llendl;
					delete[] sizes;
					delete[] names;
					mFMP->close();
					return true; // Do not try the meshoptimizer method !
				}

				LLStrider<LLVector3> pos, norm;
				LLStrider<LLVector2> tc;
				LLStrider<U16> index;

				if (!buff->getVertexStrider(pos))
				{
					llwarns << "Could not allocate vertex strider, aborting !"
							<< llendl;
					delete[] sizes;
					delete[] names;
					mFMP->close();
					return true; // Do not try the meshoptimizer method !
				}
				if (type_mask & LLVertexBuffer::MAP_NORMAL)
				{
					if (!buff->getNormalStrider(norm))
					{
						llwarns << "Could not allocate normal strider, aborting !"
								<< llendl;
						delete[] sizes;
						delete[] names;
						mFMP->close();
						return true; // Do not try the meshoptimizer method !
					}
				}
				if (type_mask & LLVertexBuffer::MAP_TEXCOORD0)
				{
					if (!buff->getTexCoord0Strider(tc))
					{
						llwarns << "Could not allocate coord strider, aborting !"
								<< llendl;
						delete[] sizes;
						delete[] names;
						mFMP->close();
						return true; // Do not try the meshoptimizer method !
					}
				}

				if (!buff->getIndexStrider(index))
				{
					llwarns << "Could not allocate index strider, aborting !"
							<< llendl;
					delete[] sizes;
					delete[] names;
					mFMP->close();
					return true;	// Do not try the meshoptimizer method !
				}
				if (type_mask & LLVertexBuffer::MAP_NORMAL)

				target_model->setVolumeFaceData(names[i], pos, norm, tc, index,
												buff->getNumVerts(),
												buff->getNumIndices());

				if (!target_model->getVolumeFace(names[i]).validate(true))
				{
					model_error("Invalid face generated during LOD generation.");
					delete[] sizes;
					delete[] names;
					return false;
				}
			}

			// Blind copy skin weights and just take closest skin weight to
			// point on decimated mesh for now (auto-generating LODs with skin
			// weights is still a bit of an open problem).
			target_model->mPosition = base->mPosition;
			target_model->mSkinWeights = base->mSkinWeights;
			target_model->mSkinInfo.clone(base->mSkinInfo);
			// Copy material list
			target_model->mMaterialList = base->mMaterialList;

			delete[] sizes;
			delete[] names;

			if (!target_model->validate())
			{
				model_error("Invalid GLOD model generated when creating LODs.");
				return false;
			}
		}

		// Rebuild scene based on mBaseScene
		mScene[lod].clear();
		mScene[lod] = mBaseScene;

		for (U32 i = 0; i < mBaseModel.size(); ++i)
		{
			LLModel* mdl = mBaseModel[i];

			LLModel* target = mModel[lod][i];
			if (!target) continue;

			for (LLModelLoader::scene::iterator iter = mScene[lod].begin(),
												end = mScene[lod].end();
				 iter != end; ++iter)
			{
				for (U32 j = 0, count = iter->second.size(); j < count; ++j)
				{
					if (iter->second[j].mModel == mdl)
					{
						iter->second[j].mModel = target;
					}
				}
			}
		}
	}

	updateDimentionsAndOffsets();

	return true;
}

F32 LLModelPreview::genMeshOptimizerPerModel(LLModel* base_model,
											 LLModel* target_model,
											 F32 indices_decim,
											 F32 error_threshold,
											 S32 simplification_mode)
{
	U32 num_vol_faces = base_model->getNumVolumeFaces();

	// Figure out buffer size
	S32 size_indices = 0;
	S32 size_vertices = 0;
	for (U32 i = 0; i < num_vol_faces; ++i)
	{
		const LLVolumeFace& face = base_model->getVolumeFace(i);
		size_indices += face.mNumIndices;
		size_vertices += face.mNumVertices;
	}

	if (size_indices < 3)
	{
		return -1.f;
	}

	// Allocate buffers; note that we are using U32 buffer instead of U16.
	size_t indices_bytes = size_indices * sizeof(U32);
	U32* output_indices = (U32*)allocate_volume_mem(indices_bytes);
	U32* combined_indices = (U32*)allocate_volume_mem(indices_bytes);

	// Extra space for normals and text coords
	S32 tc_bytes_size = (size_vertices * sizeof(LLVector2) + 0xF) & ~0xF;
	LLVector4a* combined_positions =
		(LLVector4a*)allocate_volume_mem_64(sizeof(LLVector4a) * 3 *
											size_vertices +	tc_bytes_size);
	LLVector4a* combined_normals = combined_positions + size_vertices;
	LLVector2* combined_tex_coords = (LLVector2*)(combined_normals +
												  size_vertices);

	// Copy indices and vertices into new buffers
	S32 combined_positions_shift = 0;
	S32 indices_idx_shift = 0;
	S32 combined_indices_shift = 0;
	for (U32 i = 0; i < num_vol_faces; ++i)
	{
		const LLVolumeFace& face = base_model->getVolumeFace(i);

		// Vertices
		S32 copy_bytes = face.mNumVertices * sizeof(LLVector4a);
		LLVector4a::memcpyNonAliased16((F32*)(combined_positions +
											  combined_positions_shift),
									   (F32*)face.mPositions, copy_bytes);

		// Normals
		LLVector4a::memcpyNonAliased16((F32*)(combined_normals +
											  combined_positions_shift),
									   (F32*)face.mNormals, copy_bytes);

		// Texture coords
		copy_bytes = face.mNumVertices * sizeof(LLVector2);
		memcpy((void*)(combined_tex_coords + combined_positions_shift),
			   (void*)face.mTexCoords, copy_bytes);

		combined_positions_shift += face.mNumVertices;

		// Sadly, indices cannot use a simple memcpy; we need to adjust each
		// value...
		for (U32 j = 0, count = face.mNumIndices; j < count; ++j)
		{
			combined_indices[combined_indices_shift++] = face.mIndices[j] +
														 indices_idx_shift;
		}

		indices_idx_shift += face.mNumVertices;
	}

	// Generate a shadow buffer if necessary. Welds vertices together if
	// possible.
	U32* shadow_indices = NULL;
	// If MESH_OPTIMIZER_FULL, just leave as is, since model was remaped on a
	// per face basis. Similar for MESH_OPTIMIZER_NO_TOPOLOGY, it is pointless
	// since sloppy simplification ignores all topology, including normals and
	// UVs (which can be significantly affected).
	if (simplification_mode == MESH_OPTIMIZER_NO_NORMALS)
	{
		// Strip normals, reflections should restore relatively correctly.
		shadow_indices = (U32*)allocate_volume_mem(indices_bytes);
		LLMeshOptimizer::generateShadowIndexBuffer32(shadow_indices,
													 combined_indices,
													 size_indices,
													 combined_positions, NULL,
													 combined_tex_coords,
													 size_vertices);
	}
	else if (simplification_mode == MESH_OPTIMIZER_NO_UVS)
	{
		// Strip UVs, which can heavily affect textures
		shadow_indices = (U32*)allocate_volume_mem(indices_bytes);
		LLMeshOptimizer::generateShadowIndexBuffer32(shadow_indices,
												     combined_indices,
												     size_indices,
												     combined_positions,
												     NULL, NULL,
													 size_vertices);
	}
	U32* source_indices = shadow_indices ? shadow_indices : combined_indices;

	//  Now that we have buffers, optimize

	// How far from original the model is, 1.f == 100%
	F32 result_code = 0.f;

	S32 target_indices;
	if (indices_decim > 0.f)
	{
		// Leave at least one triangle
		target_indices = llmax(3, llfloor(size_indices / indices_decim));
	}
	else
	{
		// Indices_decimator can be zero for error_threshold based calculations
		target_indices = 3;
	}

	S32 type_size = LLVertexBuffer::sTypeSize[LLVertexBuffer::TYPE_VERTEX];
	bool sloppy = simplification_mode == MESH_OPTIMIZER_NO_TOPOLOGY;
	S32 new_indices = LLMeshOptimizer::simplify32(output_indices,
												  source_indices,
												  size_indices,
												  combined_positions,
												  size_vertices, type_size,
												  target_indices,
												  error_threshold, sloppy,
												  &result_code);
	if (result_code < 0)
	{
		llwarns << "Negative result code from meshoptimizer for model: "
				<< target_model->mLabel << " - Target indices: "
				<< target_indices << " - New indices: " << new_indices
				<< " - Original count: " << size_indices << llendl;
	}

	// Free unused buffers
	free_volume_mem(combined_indices);
	free_volume_mem(shadow_indices);
	combined_indices = shadow_indices = NULL;

	if (new_indices < 3)
	{
		// Model should have at least one visible triangle
		free_volume_mem(output_indices);
		free_volume_mem_64(combined_positions);
		return -1.f;
	}

	// Repack back into individual faces

	LLVector4a* buffer_positions =
		(LLVector4a*)allocate_volume_mem_64(sizeof(LLVector4a) * 3 *
											size_vertices + tc_bytes_size);
	LLVector4a* buffer_normals = buffer_positions + size_vertices;
	LLVector2* buffer_tex_coords = (LLVector2*)(buffer_normals +
												size_vertices);
	size_t buffer_idx_size = (size_indices * sizeof(U16) + 0xF) & ~0xF;
	U16* buffer_indices = (U16*)allocate_volume_mem(buffer_idx_size);
	S32* old_to_new_positions_map = new S32[size_vertices];

	indices_idx_shift = 0;
	U32 valid_faces = 0;

	// Crude method to copy indices back into face
	for (U32 i = 0; i < num_vol_faces; ++i)
	{
		const LLVolumeFace& face = base_model->getVolumeFace(i);

		S32 range = indices_idx_shift + face.mNumVertices;
		S32 buf_positions_copied = 0;
		S32 buf_indices_copied = 0;
		bool copy_triangle = false;

		for (S32 j = 0; j < size_vertices; ++j)
		{
			old_to_new_positions_map[j] = -1;
		}

		// Copy relevant indices and vertices
		for (S32 j = 0; j < new_indices; ++j)
		{
			U32 idx = output_indices[j];
			if (j % 3 == 0)
			{
				copy_triangle = idx >= (U32)indices_idx_shift &&
								idx < (U32)range;
			}
			if (!copy_triangle)
			{
				continue;
			}
			// If it is a new position, we need to copy it
			if (old_to_new_positions_map[idx] == -1)
			{
				// Validate size
				if (buf_positions_copied >= (S32)U16_MAX)
				{
					llwarns << "Over triangle limit. Failed to optimize in 'per object' mode, falling back to per face variant for model: "
							<< target_model->mLabel << " - Target indices: "
							<< target_indices << " - New indices: "
							<< new_indices << " - Original count: "
							<< size_indices << " - Error threshold: "
							<< error_threshold << llendl;
					// Abort as cleanly as possible (i.e. properly release
					// temp buffers, unlike what happens in LL's code). HB
					new_indices = -1;	// Forces a 'return -1;' at the end.
					// This will force a clean exit from the outer loop. HB
					buf_positions_copied = U16_MAX;
					break;
				}

				// Copy everything
				buffer_positions[buf_positions_copied] =
					combined_positions[idx];
				buffer_normals[buf_positions_copied] = combined_normals[idx];
				buffer_tex_coords[buf_positions_copied] =
					combined_tex_coords[idx];

				old_to_new_positions_map[idx] = buf_positions_copied;
				buffer_indices[buf_indices_copied++] = buf_positions_copied++;
			}
			else	// Existing position
			{
				buffer_indices[buf_indices_copied++] =
					old_to_new_positions_map[idx];
			}
		}

		if (buf_positions_copied >= U16_MAX)
		{
			break;
		}

		LLVolumeFace& new_face = target_model->getVolumeFace(i);

		if (buf_indices_copied < 3)
		{
			// Face was optimized away
			new_face.resizeIndices(3);
			new_face.resizeVertices(1);
			memset((void*)new_face.mIndices, 0, sizeof(U16) * 3);
			new_face.mPositions[0].clear(); // Set first vertice to 0
			new_face.mNormals[0].clear();
			new_face.mTexCoords[0].clear();
		}
		else
		{
			new_face.resizeIndices(buf_indices_copied);
			new_face.resizeVertices(buf_positions_copied);
			new_face.allocateTangents(buf_positions_copied);

			S32 idx_size = (buf_indices_copied * sizeof(U16) + 0xF) & ~0xF;
			LLVector4a::memcpyNonAliased16((F32*)new_face.mIndices,
										   (F32*)buffer_indices, idx_size);

			S32 vert_size = buf_positions_copied * sizeof(LLVector4a);
			LLVector4a::memcpyNonAliased16((F32*)new_face.mPositions,
										   (F32*)buffer_positions, vert_size);
			LLVector4a::memcpyNonAliased16((F32*)new_face.mNormals,
										   (F32*)buffer_normals, vert_size);

			U32 tex_size = (buf_positions_copied * sizeof(LLVector2) +
							0xF) & ~0xF;
			LLVector4a::memcpyNonAliased16((F32*)new_face.mTexCoords,
										   (F32*)buffer_tex_coords, tex_size);
			++valid_faces;
		}

		indices_idx_shift += face.mNumVertices;
	}

	delete[] old_to_new_positions_map;
	free_volume_mem(output_indices);
	free_volume_mem_64(combined_positions);
	free_volume_mem_64(buffer_positions);
	free_volume_mem(buffer_indices);

	if (new_indices < 3 || !valid_faces)
	{
		// Model should have at least one visible triangle
		if (!sloppy)
		{
			// Should only happen with sloppy; non sloppy should not be capable
			// of optimizing mesh away.
			llwarns << "Failed to generate triangles for model: "
					<< target_model->mLabel << " - Target Indices: "
					<< target_indices << " - Original count: " << size_indices
					<< " - Error treshold: " << error_threshold << llendl;
		}
		return -1.f;
	}

	return (F32)size_indices / (F32)new_indices;
}

F32 LLModelPreview::genMeshOptimizerPerFace(LLModel* base_model,
											LLModel* target_model,
											U32 face_idx, F32 indices_ratio,
											F32 err_threshold,
											S32 simplification_mode)
{
	const LLVolumeFace& face = base_model->getVolumeFace(face_idx);
	S32 size_indices = face.mNumIndices;
	if (size_indices < 3)
	{
		return -1.f;
	}

	size_t size = (size_indices * sizeof(U16) + 0xF) & ~0xF;
	U16* output = (U16*)allocate_volume_mem(size);

	// Generate a shadow buffer if necessary. Welds vertices together if
	// possible.
	U16* shadow_indices = NULL;
	// If MESH_OPTIMIZER_FULL, just leave as is, since model was remaped on a
	// per face basis. Similar for MESH_OPTIMIZER_NO_TOPOLOGY, it is pointless
	// since sloppy simplification ignores all topology, including normals and
	// UVs (which can be significantly affected).
	if (simplification_mode == MESH_OPTIMIZER_NO_NORMALS)
	{
		// Strip normals, reflections should restore relatively correctly.
		shadow_indices = (U16*)allocate_volume_mem(size);
		LLMeshOptimizer::generateShadowIndexBuffer16(shadow_indices,
													 face.mIndices,
													 size_indices,
													 face.mPositions, NULL,
													 face.mTexCoords,
													 face.mNumVertices);
	}
	else if (simplification_mode == MESH_OPTIMIZER_NO_UVS)
	{
		// Strip UVs, which can heavily affect textures
		shadow_indices = (U16*)allocate_volume_mem(size);
		LLMeshOptimizer::generateShadowIndexBuffer16(shadow_indices,
													 face.mIndices,
													 size_indices,
													 face.mPositions, NULL,
													 NULL, face.mNumVertices);
	}
	U16* source_indices = shadow_indices ? shadow_indices : face.mIndices;

	// How far from original the model is, with 1.f == 100%.
	F32 result_code = 0.f;
	S32 target_indices;
	if (indices_ratio > 0.f)
	{
		// Leave at least one triangle
		target_indices = llmax(3, llfloor(size_indices / indices_ratio));
	}
	else
	{
		target_indices = 3;
	}
	S32 type_size = LLVertexBuffer::sTypeSize[LLVertexBuffer::TYPE_VERTEX];
	bool sloppy = simplification_mode == MESH_OPTIMIZER_NO_TOPOLOGY;
	S32 new_indices = LLMeshOptimizer::simplify16(output, source_indices,
												  size_indices,
												  face.mPositions,
												  face.mNumVertices, type_size,
												  target_indices,
												  err_threshold, sloppy,
												  &result_code);
	if (result_code < 0)
	{
		llwarns << "Negative result code from meshoptimizer for face "
				<< face_idx << " of model: " << target_model->mLabel
				<< " - Target indices: " << target_indices
				<< " - New indices: " << new_indices << " - Original count: "
				<< size_indices << " - Error treshold: " << err_threshold
				<< llendl;
	}

	LLVolumeFace& new_face = target_model->getVolumeFace(face_idx);
	new_face = face;  // Copy old values

	if (new_indices < 3)
	{
		if (!sloppy)
		{
			// meshopt_optimizeSloppy() can optimize triangles away even if
			// target_indices is > 2, but optimize() is not supposed to...
			std::ostringstream out;
			out << "No indices generated by meshoptimizer for face "
				<< face_idx << " of model: " << target_model->mLabel
				<< " - Target indices: " << target_indices
				<< " - Original count: " << size_indices
				<< " - Error treshold: " << err_threshold;
			llinfos << out.str() << llendl;
			mFMP->addLineToLog(out.str());
			// Face got optimized away; generate an empty triangle.
			new_face.resizeIndices(3);
			new_face.resizeVertices(1);
			memset((void*)new_face.mIndices, 0, sizeof(U16) * 3);
			new_face.mPositions[0].clear();
			new_face.mNormals[0].clear();
			new_face.mTexCoords[0].clear();
		}
	}
	else	// Assign new values
	{
		// Wipes out mIndices, so new_face cannot substitute output
		new_face.resizeIndices(new_indices);
		S32 idx_size = (new_indices * sizeof(U16) + 0xF) & ~0xF;
		LLVector4a::memcpyNonAliased16((F32*)new_face.mIndices, (F32*)output,
									   idx_size);
		// Clear unused values
		new_face.optimize();
	}

	free_volume_mem(output);
	free_volume_mem(shadow_indices);

	return new_indices < 3 ? -1.f : (F32)size_indices / (F32)new_indices;
}

void LLModelPreview::genMeshOptimizerLODs(S32 which_lod, S32 meshopt_mode,
										  U32 decimation, bool with_tri_limit)
{
	if (!mFMP)
	{
		llwarns << "Model Preview floater is gone !  Aborted." << llendl;
		return;
	}

	llinfos << "Generating lod " << which_lod << " using meshoptimizer."
			<< llendl;

	// Allow LoD from -1 to LLModel::LOD_PHYSICS
	if (which_lod < -1 || which_lod > LLModel::NUM_LODS - 1)
	{
		std::ostringstream out;
		out << "Invalid level of detail: " << which_lod;
		llwarns << out.str() << llendl;
		llassert(false);
		mFMP->addLineToLog(out.str());
		return;
	}

	if (mBaseModel.empty())
	{
		return;
	}

	// Get the triangle count for all base models
	S32 base_triangle_count = 0;
	for (U32 i = 0, count = mBaseModel.size(); i < count; ++i)
	{
		base_triangle_count += mBaseModel[i]->getNumTriangles();
	}

	U32 lod_mode = LIMIT_TRIANGLES;
	F32 indices_decim = 0.f;
	F32 tri_limit = 0.f;
	F32 lod_err_thres = 1.f;	// 100%

	// If requesting a single LOD
	if (which_lod > -1 && which_lod < NUM_LOD)
	{
		std::string cname = "lod_mode_" + lod_name[which_lod];
		lod_mode =
			mFMP->getChild<LLComboBox>(cname.c_str())->getCurrentIndex();
		if (lod_mode == LIMIT_TRIANGLES)
		{
			if (with_tri_limit)
			{
				std::string wname = "lod_triangle_limit_" +
									lod_name[which_lod];
				tri_limit = mFMP->childGetValue(wname.c_str()).asInteger();
			}
			else
			{
				tri_limit = base_triangle_count /
							powf((F32)decimation,
								 (F32)(LLModel::LOD_HIGH - which_lod));
			}
			if (tri_limit <= 0.f)
			{
				tri_limit = 1.f;
			}
			// meshoptimizer does not use triangle limit but indices limit, so
			// convert it to an aproximate ratio. Also, tri_limit can be 0.
			indices_decim = (F32)base_triangle_count / llmax(tri_limit, 1.f);
		}
		else
		{
			cname = "lod_error_threshold_" + lod_name[which_lod];
			lod_err_thres =
				// UI shows 0 to 100%, but meshoptimizer works with 0.f to 1.f
				mFMP->childGetValue(cname.c_str()).asReal() * 0.01f;
		}
	}
	else
	{
		// We are generating all LODs and each LOD will get its own
		// indices_decim
		indices_decim = 1.f;
		tri_limit = base_triangle_count;
	}

	mMaxTriangleLimit = base_triangle_count;

	LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;
	if (shader)
	{
		shader->unbind();
	}

	// Build models
	S32 start = LLModel::LOD_HIGH;
	S32 end = 0;
	if (which_lod != -1)
	{
		start = end = which_lod;
	}
	for (S32 lod = start; lod >= end; --lod)
	{
		if (which_lod == -1)
		{
			// We are generating all LODs, each with its own indices_ratio
			indices_decim *= decimation;
			tri_limit /= decimation;
		}

		mRequestedTriangleCount[lod] = tri_limit;
		mRequestedErrorThreshold[lod] = lod_err_thres * 100.f;
		mRequestedLoDMode[lod] = lod_mode;

		mModel[lod].clear();
		mModel[lod].resize(mBaseModel.size());
		mVertexBuffer[lod].clear();

		constexpr F32 allowed_ratio_drift = 1.8f;

		for (U32 mdl_idx = 0; mdl_idx < mBaseModel.size(); ++mdl_idx)
		{
			LLModel* base = mBaseModel[mdl_idx];

			LLVolumeParams volume_params;
			volume_params.setType(LL_PCODE_PROFILE_SQUARE, LL_PCODE_PATH_LINE);
			mModel[lod][mdl_idx] = new LLModel(volume_params, 0.f);

			std::string name = base->mLabel + get_lod_suffix(lod);
			mModel[lod][mdl_idx]->mLabel = name;
			mModel[lod][mdl_idx]->mSubmodelID = base->mSubmodelID;
			mModel[lod][mdl_idx]->setNumVolumeFaces(base->getNumVolumeFaces());

			LLModel* target_model = mModel[lod][mdl_idx];

			// Carry over normalized transform into simplified model
			for (U32 i = 0, count = base->getNumVolumeFaces(); i < count; ++i)
			{
				LLVolumeFace& src = base->getVolumeFace(i);
				LLVolumeFace& dst = target_model->getVolumeFace(i);
				dst.mNormalizedScale = src.mNormalizedScale;
			}

			S32 model_meshopt_mode = meshopt_mode;

			std::ostringstream out;
			out << "Model " << target_model->mLabel << " - LOD" << lod;

			F32 ratio = 0.f;

			// Ideally this should run not per model, but combine all sub-
			// models with origin model as well.
			if (model_meshopt_mode == MESH_OPTIMIZER_PRECISE)
			{
				// Run meshoptimizer for each face
				for (U32 face_idx = 0, count = base->getNumVolumeFaces();
					 face_idx < count; ++face_idx)
				{
					ratio = genMeshOptimizerPerFace(base, target_model,
													face_idx, indices_decim,
													lod_err_thres,
													MESH_OPTIMIZER_FULL);
					if (ratio < 0.f)
					{
						break;
					}
				}
				if (ratio < 0.f)
				{
					model_meshopt_mode = MESH_OPTIMIZER_AUTO;
				}
				else
				{
					out << " simplified using per face method.";
				}
			}

			if (model_meshopt_mode == MESH_OPTIMIZER_AUTO)
			{
				// Remove progressively more data if we cannot reach the target
				// Run meshoptimizer for each model/object, up to 8 faces in
				// one model.
				ratio = genMeshOptimizerPerModel(base, target_model,
												 indices_decim, lod_err_thres,
												 MESH_OPTIMIZER_FULL);
				bool done = ratio * allowed_ratio_drift >= indices_decim;
				if (done)
				{
					out << " simplified using per model method.";
				}
				else
				{
					ratio =
						genMeshOptimizerPerModel(base, target_model,
												 indices_decim, lod_err_thres,
												 MESH_OPTIMIZER_NO_NORMALS);
					done = ratio * allowed_ratio_drift >= indices_decim;
					if (done)
					{
						out << " simplified using per model method without normals.";
					}
				}
				if (!done)
				{
					ratio =
						genMeshOptimizerPerModel(base, target_model,
												 indices_decim, lod_err_thres,
												 MESH_OPTIMIZER_NO_UVS);
					done = ratio * allowed_ratio_drift >= indices_decim;
					if (done)
					{
						out << " simplified using per model method without UVs.";
					}
				}
				if (!done)
				{
					// Try sloppy variant if normal one failed to simplify
					// model enough. Use per-model, sloppy optimization
					ratio = genMeshOptimizerPerModel(base, target_model,
													 indices_decim,
													 lod_err_thres,
													 MESH_OPTIMIZER_NO_TOPOLOGY);
					// Sloppy has a tendency to error into lower side, so a
					// request for 100 triangles turns into ~70; check for
					// significant difference from target decimation.
					constexpr F32 sloppy_ratio_drift = 1.4f;
					if (lod_mode == LIMIT_TRIANGLES &&
						(ratio < 0.f ||
						 ratio > indices_decim * sloppy_ratio_drift))
					{
						// Apply a correction to compensate.
						// (indices_decim / res_ratio) by itself is likely
						// to overshoot to a different side due to overal lack
						// of precision, and we do not need an ideal result,
						// which likely does not exist, just a better one, so a
						// partial correction is enough.
						F32 sloppy_decimator = indices_decim * 0.5f *
											   (indices_decim / ratio + 1.f);
						ratio = genMeshOptimizerPerModel(base, target_model,
														 sloppy_decimator,
														 lod_err_thres,
														 MESH_OPTIMIZER_NO_TOPOLOGY);
					}
					if (ratio < 0.f)
					{
						// Sloppy variant failed to generate triangles. Can
						// happen with models that are too simple as is.
						// Fallback to normal method.
						if (genMeshOptimizerPerModel(base, target_model,
													 indices_decim,
													 lod_err_thres,
													 MESH_OPTIMIZER_FULL) < 0.f)
						{
							// Failed again !  Fall back to sloppy per face
							// method
							model_meshopt_mode = MESH_OPTIMIZER_SLOPPY;
						}
						else
						{
							out << " simplified using per model sloppy method.";
						}
					}
				}
			}

			if (model_meshopt_mode == MESH_OPTIMIZER_SLOPPY)
			{
				for (U32 face_idx = 0, count = base->getNumVolumeFaces();
					 face_idx < count; ++face_idx)
				{
					if (genMeshOptimizerPerFace(base, target_model, face_idx,
												indices_decim, lod_err_thres,
												MESH_OPTIMIZER_NO_TOPOLOGY) < 0)
					{
						genMeshOptimizerPerFace(base, target_model, face_idx,
												indices_decim, lod_err_thres,
												MESH_OPTIMIZER_FULL);
					}
				}
				out << " simplified using per face sloppy method.";
			}

			llinfos << out.str() << llendl;
			mFMP->addLineToLog(out.str());

			// Blind-copy skin weights and just take closest skin weight to
			// point on decimated mesh for now (auto-generating LODs with skin
			// weights is still a bit of an open problem).
			target_model->mPosition = base->mPosition;
			target_model->mSkinWeights = base->mSkinWeights;
			target_model->mSkinInfo.clone(base->mSkinInfo);
			// Copy materials list
			target_model->mMaterialList = base->mMaterialList;

			if (!target_model->validate())
			{
				model_error("Invalid meshoptimizer model generated when creating LODs.");
				mFMP->close();
				return;
			}
		}

		// Rebuild scene based on mBaseScene
		mScene[lod].clear();
		mScene[lod] = mBaseScene;

		for (U32 i = 0; i < mBaseModel.size(); ++i)
		{
			LLModel* mdl = mBaseModel[i];

			LLModel* target = mModel[lod][i];
			if (!target) continue;

			for (LLModelLoader::scene::iterator iter = mScene[lod].begin(),
												end = mScene[lod].end();
				 iter != end; ++iter)
			{
				for (U32 j = 0, count = iter->second.size(); j < count; ++j)
				{
					if (iter->second[j].mModel == mdl)
					{
						iter->second[j].mModel = target;
					}
				}
			}
		}
	}

	updateDimentionsAndOffsets();

	LLVertexBuffer::unbind();
	if (shader)
	{
		shader->bind();
	}
}

void LLModelPreview::updateStatusMessages()
{
	assert_main_thread();

	if (!mFMP)
	{
		llwarns << "Model Preview floater is gone !  Aborted." << llendl;
		return;
	}

	// Triangle/vertex/submesh count for each mesh asset for each lod
	std::vector<S32> tris[LLModel::NUM_LODS];
	std::vector<S32> verts[LLModel::NUM_LODS];
	std::vector<S32> submeshes[LLModel::NUM_LODS];

	// Total triangle/vertex/submesh count for each lod
	S32 total_tris[LLModel::NUM_LODS];
	S32 total_verts[LLModel::NUM_LODS];
	S32 total_submeshes[LLModel::NUM_LODS];

	for (U32 i = 0; i < LLModel::NUM_LODS - 1; ++i)
	{
		total_tris[i] = 0;
		total_verts[i] = 0;
		total_submeshes[i] = 0;
	}

	mFMP->mCalculateBtn->setEnabled(true);

    for (LLMeshUploadThread::instance_list_t::iterator
			iter = mUploadData.begin(), end = mUploadData.end();
		 iter != end; ++iter)
	{
		LLModelInstance& instance = *iter;

		LLModel* model_high_lod = instance.mLOD[LLModel::LOD_HIGH];
		if (!model_high_lod)
		{
			setLoadState(LLModelLoader::ERROR_HIGH_LOD_MODEL_MISSING);
			mFMP->mCalculateBtn->setEnabled(false);
			continue;
		}

		for (S32 i = 0; i < LLModel::NUM_LODS - 1; ++i)
		{
			LLModel* lod_model = instance.mLOD[i];
			if (!lod_model)
			{
				setLoadState(LLModelLoader::ERROR_LOD_MODEL_MISMATCH);
				mFMP->mCalculateBtn->setEnabled(false);
				continue;
			}

			// For each model in the lod
			S32 cur_tris = 0;
			S32 cur_verts = 0;
			S32 cur_submeshes = lod_model->getNumVolumeFaces();

			for (S32 j = 0; j < cur_submeshes; ++j)
			{
				// For each submesh (face), add triangles and vertices to
				// current total
				const LLVolumeFace& face = lod_model->getVolumeFace(j);
				cur_tris += face.mNumIndices / 3;
				cur_verts += face.mNumVertices;
			}

			// Useful for debugging generalized complaints below about total
			// sub-meshes which do not have enough context to address exactly
			// what needs to be fixed to move towards compliance with the rules
			if (mImporterDebug)
			{
				std::ostringstream out;
				out << "Instance: " << lod_model->mLabel << " - LOD" << i
					<< " - Verts: " << cur_verts << " - Tris: " << cur_tris
					<< " - Faces: " << cur_submeshes;
				for (LLModel::material_list::iterator
						it = lod_model->mMaterialList.begin(),
						end = lod_model->mMaterialList.end();
					 it != end; ++it)
				{
					out << " - Material: " << *it;
				}
				llinfos << out.str() << llendl;
				mFMP->addLineToLog(out.str());
			}

			// Add this model to the lod total
			total_tris[i] += cur_tris;
			total_verts[i] += cur_verts;
			total_submeshes[i] += cur_submeshes;

			// Store this model's counts to asset data
			tris[i].push_back(cur_tris);
			verts[i].push_back(cur_verts);
			submeshes[i].push_back(cur_submeshes);
		}
	}

	if (mMaxTriangleLimit == 0)
	{
		mMaxTriangleLimit = total_tris[LLModel::LOD_HIGH];
	}

	mHasDegenerate = false;

	// Check for degenerate triangles in physics mesh
	U32 lod = LLModel::LOD_PHYSICS;
	const LLVector4a scale(0.5f);
	LLVector4a v1, v2, v3;
	for (U32 i = 0; i < mModel[lod].size() && !mHasDegenerate; ++i)
	{
		// For each model in the lod
		if (!mModel[lod][i] || !mModel[lod][i]->mPhysics.mHull.empty())
		{
			continue;
		}
		// No decomp exists
		S32 cur_submeshes = mModel[lod][i]->getNumVolumeFaces();
		for (S32 j = 0; j < cur_submeshes && !mHasDegenerate; ++j)
		{
			// For each submesh (face), add triangles and vertices to current
			// total
			LLVolumeFace& face = mModel[lod][i]->getVolumeFace(j);
			for (S32 k = 0; k < face.mNumIndices; k += 3)
			{
				U16 index_a = face.mIndices[k];
				v1.setMul(face.mPositions[index_a], scale);
				U16 index_b = face.mIndices[k + 1];
				v2.setMul(face.mPositions[index_b], scale);
				U16 index_c = face.mIndices[k + 2];
				v3.setMul(face.mPositions[index_c], scale);
				if (LLVolumeFace::isDegenerate(v1, v2, v3))
				{
					mHasDegenerate = true;
					break;
				}
			}
		}
	}

	std::string mesh_status_na = mFMP->getString("mesh_status_na");

	S32 upload_status[LLModel::LOD_HIGH + 1];

	mModelNoErrors = true;

	constexpr S32 lod_high = LLModel::LOD_HIGH;
	U32 high_submodel_count = mModel[lod_high].size() -
							  countRootModels(mModel[lod_high]);

	for (S32 lod = 0; lod <= lod_high; ++lod)
	{
		upload_status[lod] = 0;

		std::string message = "mesh_status_good";

		if (total_tris[lod] > 0)
		{
			mFMP->childSetText(lod_triangles_name[lod],
							   llformat("%d", total_tris[lod]));
			mFMP->childSetText(lod_vertices_name[lod],
							   llformat("%d", total_verts[lod]));
		}
		else
		{
			if (lod == lod_high)
			{
				upload_status[lod] = 2;
				message = "mesh_status_missing_lod";
			}
			else
			{
				for (S32 i = lod - 1; i >= 0; --i)
				{
					if (total_tris[i] > 0)
					{
						upload_status[lod] = 2;
						message = "mesh_status_missing_lod";
					}
				}
			}

			mFMP->childSetText(lod_triangles_name[lod], mesh_status_na);
			mFMP->childSetText(lod_vertices_name[lod], mesh_status_na);
		}

		if (lod != lod_high)
		{
			if (total_submeshes[lod] &&
				total_submeshes[lod] != total_submeshes[lod_high])
			{
				// Number of submeshes is different
				message = "mesh_status_submesh_mismatch";
				upload_status[lod] = 2;
			}
			else if (mModel[lod].size() -
					 countRootModels(mModel[lod]) != high_submodel_count)
			{
				// Number of submodels is different, not all faces are matched
				// correctly.
				message = "mesh_status_submesh_mismatch";
				upload_status[lod] = 2;
				// Note: Submodels in instance were loaded from higher LOD and
				// as result face count returns same value and
				// total_submeshes[lod] is identical to high_lod one.
			}
			else if (!tris[lod].empty() &&
					 tris[lod].size() != tris[lod_high].size())
			{
				// Number of meshes is different
				message = "mesh_status_mesh_mismatch";
				upload_status[lod] = 2;
			}
			else if (!verts[lod].empty())
			{
				S32 sum_verts_higher_lod = 0;
				S32 sum_verts_this_lod = 0;
				S32 max = verts[lod + 1].size();
				for (S32 i = 0, count = verts[lod].size(); i < count; ++i)
				{
					if (i < max)
					{
						sum_verts_higher_lod += verts[lod + 1][i];
					}
					sum_verts_this_lod += verts[lod][i];
				}

				if (sum_verts_higher_lod > 0 &&
					sum_verts_this_lod > sum_verts_higher_lod)
				{
					// Too many vertices in this lod
					message = "mesh_status_too_many_vertices";
					upload_status[lod] = 1;
				}
			}
		}

		std::string img = lod_status_image[upload_status[lod]];
		LLIconCtrl* icon = mFMP->getChild<LLIconCtrl>(lod_icon_name[lod]);
		icon->setVisible(true);
		icon->setImage(img);

		if (upload_status[lod] >= 2)
		{
			mModelNoErrors = false;
		}

		if (lod == mPreviewLOD)
		{
			mFMP->childSetText("lod_status_message_text",
							   mFMP->getString(message));
			icon = mFMP->getChild<LLIconCtrl>("lod_status_message_icon");
			icon->setImage(img);
		}

		updateLodControls(lod);
	}

	// Warn if hulls have more than 256 points in them
	bool physics_off_limit = false;
	for (U32 i = 0, count1 = mModel[LLModel::LOD_PHYSICS].size();
		 mModelNoErrors && i < count1; ++i)
	{
		LLModel* mdl = mModel[LLModel::LOD_PHYSICS][i];
		if (mdl)
		{
			for (U32 j = 0, count2 = mdl->mPhysics.mHull.size(); j < count2;
				 ++j)
			{
				if (mdl->mPhysics.mHull[j].size() > 256)
				{
					physics_off_limit = true;
					llwarns << "Physical model " << mdl->mLabel
							<< " exceeds vertex per hull limitations."
							<< llendl;
					break;
				}
			}
		}
	}

	std::string phys_status;
	if (mHasDegenerate)
	{
		phys_status = mFMP->getString("phys_status_degenerate_triangles");
	}
	else if (physics_off_limit)
	{
		phys_status = mFMP->getString("phys_status_vertex_limit_exceeded");
	}
	mFMP->childSetValue("physics_status_message_text", phys_status);

	if (mLoadState >= LLModelLoader::ERROR_PARSING)
	{
		mModelNoErrors = false;
		llinfos << "Loader returned errors, model cannot be uploaded"
				<< llendl;
	}

	if (mFMP->childGetValue("upload_skin").asBoolean() &&
		mFMP->childGetValue("upload_joints").asBoolean() &&
		!mRigValidJointUpload)
	{
		mModelNoErrors = false;
		llinfos << "Invalid rig; there might be issues with uploading joint positions"
				<< llendl;
	}

	if (mModelNoErrors && mModelLoader && !mModelLoader->areTexturesReady() &&
		mFMP->childGetValue("upload_textures").asBoolean())
	{
		mModelNoErrors = false;
	}

	// *TODO: investigate use of mHasDegenerate and include into mModelNoErrors
	// upload blocking mechanics; current use of mHasDegenerate would not block
	// upload permanently: later checks will restore the button.
	if (!mModelNoErrors || mHasDegenerate)
	{
		mFMP->mUploadBtn->setEnabled(false);
	}

	mFMP->mCalculateBtn->setEnabled(mModelNoErrors && !mHasDegenerate &&
									mLodsWithParsingError.empty());

	// Add up physics triangles etc
	S32 phys_tris = 0;
	S32 phys_hulls = 0;
	S32 phys_points = 0;

	// Get the triangle count for the whole scene
	for (LLModelLoader::scene::iterator
			iter = mScene[LLModel::LOD_PHYSICS].begin(),
			end = mScene[LLModel::LOD_PHYSICS].end();
		 iter != end; ++iter)
	{
		for (LLModelLoader::model_instance_list_t::iterator
				instance = iter->second.begin(),
				end_instance = iter->second.end();
			 instance != end_instance; ++instance)
		{
			LLModel* model = instance->mModel;
			if (!model)
			{
				continue;
			}

			S32 cur_submeshes = model->getNumVolumeFaces();

			LLModel::hull_decomp& decomp = model->mPhysics.mHull;
			if (decomp.empty())
			{
				// Choose physics shape OR decomposition, cannot use both
				for (S32 j = 0; j < cur_submeshes; ++j)
				{
					// For each submesh (face), add triangles and vertices to
					// current total
					const LLVolumeFace& face = model->getVolumeFace(j);
					phys_tris += face.mNumIndices / 3;
				}
			}
			else
			{
				phys_hulls += decomp.size();
				for (U32 i = 0, count = decomp.size(); i < count; ++i)
				{
					phys_points += decomp[i].size();
				}
			}
		}
	}

	if (phys_tris > 0)
	{
		mFMP->childSetTextArg("physics_triangles", "[TRIANGLES]",
							  llformat("%d", phys_tris));
	}
	else
	{
		mFMP->childSetTextArg("physics_triangles", "[TRIANGLES]",
							  mesh_status_na);
	}

	if (phys_hulls > 0)
	{
		mFMP->childSetTextArg("physics_hulls", "[HULLS]",
							  llformat("%d", phys_hulls));
		mFMP->childSetTextArg("physics_points", "[POINTS]",
							  llformat("%d", phys_points));
	}
	else
	{
		mFMP->childSetTextArg("physics_hulls", "[HULLS]", mesh_status_na);
		mFMP->childSetTextArg("physics_points", "[POINTS]", mesh_status_na);
	}

	if (phys_tris > 0 || phys_hulls > 0)
	{
		if (!mFMP->isViewOptionEnabled("show_physics"))
		{
			mFMP->enableViewOption("show_physics");
			// Cannot display both physics and skin weights... HB
			if (!mFMP->childGetValue("show_skin_weight"))
			{
				mViewOption["show_physics"] = true;
				mFMP->childSetValue("show_physics", true);
			}
		}
	}
	else
	{
		mFMP->disableViewOption("show_physics");
		mViewOption["show_physics"] = false;
		mFMP->childSetValue("show_physics", false);
	}

	// See *TODO in setPhysicsFromLOD(). Since we cannot remove any loaded
	// physics hull mode, we must disable the default phys hull item in the
	// combo if one got loaded at any point...
	LLScrollListItem* itemp =
		mFMP->getChild<LLComboBox>("physics_lod_combo")->getItemByIndex(0);
	if (itemp && (phys_tris || phys_hulls))
	{
		itemp->setEnabled(false);
	}

	// Enable/disable "analysis" UI
	bool enable = phys_tris > 0 || phys_hulls > 0;
	bool enable_full = enable && !mFMP->mLibIsHACD;
	LLPanel* panel = mFMP->getChild<LLPanel>("physics analysis");
	LLView* child = panel->getFirstChild();
	while (child)
	{
		child->setEnabled(enable_full);
		child = panel->findNextSibling(child);
	}
	mFMP->childSetEnabled("physics_explode_label", enable);
	mFMP->childSetEnabled("physics_explode", enable);
	if (enable_full != enable)
	{
		mFMP->childSetEnabled("second_step_label", enable);
		mFMP->childSetEnabled("Decompose", enable);
	}

	// Enable/disable "simplification" UI
	enable = phys_hulls > 0 && mFMP->mCurRequest.empty() && !mFMP->mLibIsHACD;
	panel = mFMP->getChild<LLPanel>("physics simplification");
	child = panel->getFirstChild();
	while (child)
	{
		child->setEnabled(enable);
		child = panel->findNextSibling(child);
	}

	mFMP->childSetVisible("hacd_limits", mFMP->mLibIsHACD);

	if (mFMP->mCurRequest.empty())
	{
		mFMP->childSetVisible("Simplify", true);
		mFMP->childSetVisible("Decompose", true);
		if (!mFMP->mLibIsHACD && phys_hulls > 0)
		{
			mFMP->childEnable("Simplify");
		}
		if (phys_tris || phys_hulls > 0)
		{
			mFMP->childEnable("Decompose");
		}
		mFMP->childSetVisible("simplify_cancel", false);
		mFMP->childSetVisible("decompose_cancel", false);
	}
	else
	{
		if (!mFMP->mLibIsHACD)
		{
			mFMP->childEnable("simplify_cancel");
		}
		mFMP->childEnable("decompose_cancel");
	}

	S32 which_mode =
		mFMP->getChild<LLComboBox>("physics_lod_combo")->getCurrentIndex();
	if (which_mode == 6)
	{
		mFMP->childEnable("physics_file");
		mFMP->childEnable("physics_browse");
	}
	else
	{
		mFMP->childDisable("physics_file");
		mFMP->childDisable("physics_browse");
	}

	LLSpinCtrl* crease = mFMP->getChild<LLSpinCtrl>("crease_angle");

	if (mRequestedCreaseAngle[mPreviewLOD] == -1.f)
	{
		mFMP->childSetColor("crease_label", LLColor4::grey);
		crease->forceSetValue(75.f);
	}
	else
	{
		mFMP->childSetColor("crease_label", LLColor4::white);
		crease->forceSetValue(mRequestedCreaseAngle[mPreviewLOD]);
	}

	mModelUpdatedSignal(true);
}

void LLModelPreview::updateLodControls(S32 lod)
{
	if (!mFMP)
	{
		llwarns << "Model Preview floater is gone !  Aborted." << llendl;
		return;
	}

	if (lod < LLModel::LOD_IMPOSTOR || lod > LLModel::LOD_HIGH)
	{
		llwarns << "Invalid level of detail: " << lod << llendl;
		llassert(false);
		return;
	}

	static const char* lod_controls[] =
	{
		"lod_mode_",
		"lod_triangle_limit_",
		"lod_error_threshold_"
	};
	constexpr U32 num_lod_controls = LL_ARRAY_SIZE(lod_controls);

	static const char* file_controls[] =
	{
		"lod_browse_",
		"lod_file_",
	};
	constexpr U32 num_file_controls = LL_ARRAY_SIZE(file_controls);

	if (!mFMP) return;

	std::string lodstr = lod_name[lod];

	std::string wname = "lod_source_" + lodstr;
	LLComboBox* lod_combo = mFMP->getChild<LLComboBox>(wname.c_str(),
													   true, false);
	if (!lod_combo) return;

	S32 lod_mode = lod_combo->getCurrentIndex();
	if (lod_mode == LOD_FROM_FILE) // LoD from file
	{
		mFMP->mLODMode[lod] = lod_mode;
		for (U32 i = 0; i < num_file_controls; ++i)
		{
			wname = file_controls[i] + lodstr;
			mFMP->childShow(wname.c_str());
		}

		for (U32 i = 0; i < num_lod_controls; ++i)
		{
			wname = lod_controls[i] + lodstr;
			mFMP->childHide(wname.c_str());
		}
	}
	else if (lod_mode == USE_LOD_ABOVE) // use LoD above
	{
		mFMP->mLODMode[lod] = lod_mode;
		for (U32 i = 0; i < num_file_controls; ++i)
		{
			wname = file_controls[i] + lodstr;
			mFMP->childHide(wname.c_str());
		}

		for (U32 i = 0; i < num_lod_controls; ++i)
		{
			wname = lod_controls[i] + lodstr;
			mFMP->childHide(wname.c_str());
		}

		if (lod < LLModel::LOD_HIGH)
		{
			mModel[lod] = mModel[lod + 1];
			mScene[lod] = mScene[lod + 1];
			mVertexBuffer[lod].clear();

			// Also update lower LoD
			if (lod > LLModel::LOD_IMPOSTOR)
			{
				updateLodControls(lod - 1);
			}
		}
	}
	else	// Auto generate, the default case for all LoDs except High
	{
		mFMP->mLODMode[lod] = 1;

		// Do not actually regenerate lod when refreshing UI
		mLODFrozen = true;

		for (U32 i = 0; i < num_file_controls; ++i)
		{
			wname = file_controls[i] + lodstr;
			mFMP->childHide(wname.c_str());
		}

		for (U32 i = 0; i < num_lod_controls; ++i)
		{
			wname = lod_controls[i] + lodstr;
			mFMP->childShow(wname.c_str());
		}

		std::string wname = "lod_error_threshold_" + lodstr;
		LLSpinCtrl* threshold = mFMP->getChild<LLSpinCtrl>(wname.c_str());
		wname = "lod_triangle_limit_" + lodstr;
		LLSpinCtrl* limit = mFMP->getChild<LLSpinCtrl>(wname.c_str());

		limit->setMaxValue(mMaxTriangleLimit);
		limit->forceSetValue(mRequestedTriangleCount[lod]);

		threshold->forceSetValue(mRequestedErrorThreshold[lod]);

		wname = "lod_mode_" + lodstr;
		LLComboBox* combo = mFMP->getChild<LLComboBox>(wname.c_str());
		combo->selectNthItem(mRequestedLoDMode[lod]);

		if (mRequestedLoDMode[lod] == 0)
		{
			limit->setVisible(true);
			threshold->setVisible(false);

			limit->setMaxValue(mMaxTriangleLimit);
			limit->setIncrement(llmax(1U, mMaxTriangleLimit / 32U));
		}
		else
		{
			limit->setVisible(false);
			threshold->setVisible(true);
		}

		mLODFrozen = false;
	}
}

void LLModelPreview::setPreviewTarget(F32 distance)
{
	mCameraDistance = distance;
	mCameraZoom = 1.f;
	mCameraPitch = 0.f;
	mCameraYaw = 0.f;
	mCameraOffset.clear();
}

void LLModelPreview::clearBuffers()
{
	for (U32 i = 0; i < 6; ++i)
	{
		mVertexBuffer[i].clear();
	}
}

#if defined(GCC_VERSION)
// Work-around for a spurious "mat_normal maybe uninitialized", with warning,
// even though mat_normal is never used when not actually initialized (see
// the 'skinned' boolean test. HB
# pragma GCC diagnostic ignored "-Wuninitialized"
# pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

void LLModelPreview::genBuffers(S32 lod, bool include_skin_weights)
{
	LLModelLoader::model_list* model = NULL;

	if (lod < 0 || lod > 4)
	{
		model = &mBaseModel;
		lod = 5;
	}
	else
	{
		model = &(mModel[lod]);
	}

	mVertexBuffer[lod].clear();

	LLModelLoader::model_list::iterator base_iter = mBaseModel.begin();

	for (LLModelLoader::model_list::iterator iter = model->begin();
		 iter != model->end(); ++iter)
	{
		LLModel* mdl = *iter;
		if (!mdl) continue;

		++base_iter;

		bool skinned = include_skin_weights && !mdl->mSkinWeights.empty();

		LLMatrix4a mat_normal;
		if (skinned)
		{
			mat_normal.loadu(mdl->mSkinInfo.mBindShapeMatrix);
			mat_normal.invert();
			mat_normal.transpose();
		}

		for (S32 i = 0, count = mdl->getNumVolumeFaces(); i < count; ++i)
		{
			const LLVolumeFace& vf = mdl->getVolumeFace(i);
			U32 num_vertices = vf.mNumVertices;
			U32 num_indices = vf.mNumIndices;

			if (!num_vertices || !num_indices)
			{
				continue;
			}

			U32 mask = LLVertexBuffer::MAP_VERTEX |
					   LLVertexBuffer::MAP_NORMAL |
					   LLVertexBuffer::MAP_TEXCOORD0;
			if (skinned)
			{
				mask |= LLVertexBuffer::MAP_WEIGHT4;
			}

			LLPointer<LLVertexBuffer> vb = new LLVertexBuffer(mask);
			if (!vb->allocateBuffer(num_vertices, num_indices))
			{
				llwarns << "Failed to allocate vertex buffer with "
						<< num_vertices << " vertices and " << num_indices
						<< " indices" << llendl;
				return;
			}

			LLStrider<LLVector4a> vertex_strider, normal_strider,
								  weights_strider;
			LLStrider<LLVector2> tc_strider;
			LLStrider<U16> index_strider;

			if (!vb->getVertexStrider(vertex_strider) ||
				!vb->getIndexStrider(index_strider))
			{
				llwarns << "Could not get vertex and index striders."
						<< llendl;
				return;
			}

			if (skinned && !vb->getWeight4Strider(weights_strider))
			{
				llwarns << "Could not get weight strider." << llendl;
				return;
			}

			LLVector4a::memcpyNonAliased16((F32*)vertex_strider.get(),
										   (F32*)vf.mPositions,
										   num_vertices * 4 * sizeof(F32));
			if (skinned)
			{
				LLMatrix4a bind_shape_matrix;
				bind_shape_matrix.loadu(mdl->mSkinInfo.mBindShapeMatrix);
				for (U32 k = 0; k < num_vertices; ++k)
				{
					LLVector4a* v = vertex_strider.get();
					bind_shape_matrix.affineTransform(*v, *v);
					vertex_strider++;
				}
			}

			if (vf.mTexCoords)
			{
				if (!vb->getTexCoord0Strider(tc_strider))
				{
					llwarns << "Could not get coord strider." << llendl;
					return;
				}
				S32 tex_size = (num_vertices * 2 * sizeof(F32) + 0xF) & ~0xF;
				LLVector4a::memcpyNonAliased16((F32*)tc_strider.get(),
											   (F32*)vf.mTexCoords, tex_size);
			}

			if (vf.mNormals)
			{
				if (!vb->getNormalStrider(normal_strider))
				{
					llwarns << "Could not get normal strider." << llendl;
					return;
				}
				if (skinned)
				{
					LLVector4a* src = vf.mNormals;
					LLVector4a* end = src + num_vertices;
					while (src < end)
					{
						LLVector4a* n = normal_strider.get();
						mat_normal.rotate(*src++, *n);
						n->normalize3fast();
						normal_strider++;
					}
				}
				else
				{
					LLVector4a::memcpyNonAliased16((F32*)normal_strider.get(),
												   (F32*)vf.mNormals,
												   num_vertices * 4 * sizeof(F32));
				}
			}

			if (skinned)
			{
				bool fp_prec_error = false;
				for (U32 i = 0; i < num_vertices; ++i)
				{
					// Find closest weight to vf.mVertices[i].mPosition
					LLVector3 pos(vf.mPositions[i].getF32ptr());

					const LLModel::weight_list& weight_list =
						mdl->getJointInfluences(pos);
					// LLModel::loadModel() should guarantee this:
					if (weight_list.size() <= 0 || weight_list.size() > 4)
					{
						return;
					}

					LLVector4 w(0.f, 0.f, 0.f, 0.f);

					for (U32 i = 0, count2 = weight_list.size();
						 i < count2; ++i)
					{
						F32 wght = llclamp(weight_list[i].mWeight,
										   .001f, 0.999f);
						F32 joint = (F32)weight_list[i].mJointIdx;
						w.mV[i] = joint + wght;
						if (w.mV[i] - (S32)w.mV[i] <= 0.f)
						{
							// Because weights are non-zero, and range of
							// weight values should not cause floating point
							// precision issues.
							fp_prec_error = true;
						}
					}

					(*weights_strider++).loadua(w.mV);
				}
				if (fp_prec_error)
				{
					llwarns << "Floating point precision error detected."
							<< llendl;
				}
			}

			// Build indices
			for (U32 i = 0; i < num_indices; ++i)
			{
				*index_strider++ = vf.mIndices[i];
			}

			mVertexBuffer[lod][mdl].emplace_back(vb);

			vb->unmapBuffer();	// Requiered to get all the changes saved ! HB
		}
	}

	// A redraw will be needed. HB
	mNeedsUpdate = true;
}

void LLModelPreview::update()
{
	if (mGenLOD)
	{
		bool subscribe_for_generation = mLodsQuery.empty();
		mGenLOD = false;
		mDirty = true;
		mLodsQuery.clear();

		for (S32 lod = LLModel::LOD_HIGH; lod >= 0; --lod)
		{
			// Adding all lods into query for generation
			mLodsQuery.push_back(lod);
		}

		if (subscribe_for_generation)
		{
			doOnIdleRepeating(lodQueryCallback);
		}
	}

	if (mDirty && mLodsQuery.empty())
	{
		mDirty = false;
		updateDimentionsAndOffsets();
		updateStatusMessages();
		refresh();
	}
}

void LLModelPreview::createPreviewAvatar()
{
	mPreviewAvatar =
		(LLVOAvatar*)gObjectList.createObjectViewer(LL_PCODE_LEGACY_AVATAR,
													gAgent.getRegion(),
													LLViewerObject::CO_FLAG_UI_AVATAR);
	if (mPreviewAvatar)
	{
		mPreviewAvatar->createDrawable();
		mPreviewAvatar->mSpecialRenderMode = 1;
		mPreviewAvatar->startMotion(ANIM_AGENT_STAND);
		mPreviewAvatar->hideSkirt();
	}
	else
	{
		llwarns << "Failed to create preview avatar for upload model window"
				<< llendl;
	}
}

//static
U32 LLModelPreview::countRootModels(LLModelLoader::model_list models)
{
	U32 root_models = 0;
	model_list::iterator model_iter = models.begin();
	while (model_iter != models.end())
	{
		LLModel* mdl = *model_iter;
		if (mdl && mdl->mSubmodelID == 0)
		{
			++root_models;
		}
		++model_iter;
	}
	return root_models;
}

//static
void LLModelPreview::loadedCallback(LLModelLoader::scene& scene,
									LLModelLoader::model_list& model_list,
									S32 lod, void* userdata)
{
	LLModelPreview* preview = (LLModelPreview*)userdata;
	if (preview && preview == LLFloaterModelPreview::getModelPreview())
	{
		preview->loadModelCallback(lod);

		const LLVOAvatar* avatarp = preview->mPreviewAvatar;
		if (avatarp && avatarp->mDrawable)
		{
			// Set up ground plane for possible rendering
			const LLVector3 root_pos = avatarp->mRoot->getPosition();
			const LLVector4a* ext = avatarp->mDrawable->getSpatialExtents();
			const LLVector4a min = ext[0], max = ext[1];
			const F32 center = (max[2] - min[2]) * 0.5f;
			const F32 ground = root_pos[2] - center;
			preview->mGroundPlane[0].set(min[0], min[1], ground);
			preview->mGroundPlane[1].set(max[0], min[1], ground);
			preview->mGroundPlane[2].set(max[0], max[1], ground);
			preview->mGroundPlane[3].set(min[0], max[1], ground);
		}
	}
}

//static
void LLModelPreview::stateChangedCallback(U32 state, void* userdata)
{
	LLModelPreview* preview = (LLModelPreview*)userdata;
	if (preview && preview == LLFloaterModelPreview::getModelPreview())
	{
		preview->setLoadState(state);
	}
}

//static
LLJoint* LLModelPreview::lookupJointByName(const std::string& str,
										   void* userdata)
{
	LLModelPreview* preview = (LLModelPreview*)userdata;
	if (preview && preview == LLFloaterModelPreview::getModelPreview() &&
		preview->mPreviewAvatar)
	{
		U32 joint_key = LLJoint::getKey(str, false);
		return preview->mPreviewAvatar->getJoint(joint_key);
	}
	return NULL;
}

//static
U32 LLModelPreview::loadTextures(LLImportMaterial& material, void* userdata)
{
	LLModelPreview* preview = (LLModelPreview*)userdata;
	if (preview && preview == LLFloaterModelPreview::getModelPreview() &&
		!material.mDiffuseMapFilename.empty())
	{
		material.mUserData = new LLPointer<LLViewerFetchedTexture>;
		LLPointer<LLViewerFetchedTexture>& tex =
			(*reinterpret_cast<LLPointer<LLViewerFetchedTexture>*>(material.mUserData));
		tex = LLViewerTextureManager::getFetchedTextureFromUrl("file://" +
															   material.mDiffuseMapFilename,
															   FTT_LOCAL_FILE,
															   true,
															   LLGLTexture::BOOST_PREVIEW);
		tex->setLoadedCallback(LLModelPreview::textureLoadedCallback,
							   0, true, false, userdata, NULL, false);
		tex->forceToSaveRawImage(0, F32_MAX);
		material.setDiffuseMap(tex->getID());	// Record tex ID
		return 1;
	}

	material.mUserData = NULL;
	return 0;
}

void LLModelPreview::addEmptyFace(LLModel* modelp)
{
	if (!modelp)
	{
		llwarns << "NULL model pointer passed !" << llendl;
		return;
	}

	U32 type_mask = LLVertexBuffer::MAP_VERTEX | LLVertexBuffer::MAP_NORMAL |
					LLVertexBuffer::MAP_TEXCOORD0;

	LLPointer<LLVertexBuffer> buff = new LLVertexBuffer(type_mask);

	buff->allocateBuffer(1, 3);
	buff->resetVertexData();

	LLStrider<U16> index_strider;
	if (!buff->getIndexStrider(index_strider))
	{
		llwarns << "Failed to get index strider range, aborted !" << llendl;
		return;
	}
	buff->resetIndexData();

	if (!buff->validateRange(0, buff->getNumVerts() - 1,
							 buff->getNumIndices(), 0))
	{
		llwarns << "Invalid range, aborted !" << llendl;
		return;
	}

	LLStrider<LLVector3> pos;
	LLStrider<LLVector3> norm;
	LLStrider<LLVector2> tc;
	LLStrider<U16> index;

	if (!buff->getVertexStrider(pos))
	{
		llwarns << "Could not allocate vertex strider, aborting !" << llendl;
		return;
	}

	if (type_mask & LLVertexBuffer::MAP_NORMAL)
	{
		if (!buff->getNormalStrider(norm))
		{
			llwarns << "Could not allocate normal strider, aborting !"
					<< llendl;
			return;
		}
	}
	if (type_mask & LLVertexBuffer::MAP_TEXCOORD0)
	{
		if (!buff->getTexCoord0Strider(tc))
		{
			llwarns << "Could not allocate texcoord strider, aborting !"
					<< llendl;
			return;
		}
	}

	if (!buff->getIndexStrider(index))
	{
		llwarns << "Could not allocate index strider, aborting !" << llendl;
		return;
	}

	// Resize face array
	S32 face_cnt = modelp->getNumVolumeFaces();
	modelp->setNumVolumeFaces(face_cnt + 1);
	modelp->setVolumeFaceData(face_cnt + 1, pos, norm, tc, index,
							  buff->getNumVerts(), buff->getNumIndices());
}

// For PBR rendering only.
static void upload_matrix_palette(LLVOAvatar* avp, LLMeshSkinInfo* skinp)
{
	static LLMatrix4a mat4a[LL_MAX_JOINTS_PER_MESH_OBJECT];
	static F32 mp[LL_MAX_JOINTS_PER_MESH_OBJECT * 12];

	skinp->updateHash(true);	// true = force

	// Only upload the matrix palette if not yet previously done. HB
	static LLVOAvatar* last_avp = NULL;
	static U64 last_hash = 0;
	static LLGLSLShader* last_shaderp = NULL;
	if (last_avp == avp && last_hash == skinp->mHash &&
		last_shaderp == LLGLSLShader::sCurBoundShaderPtr)
	{
		return;
	}
	last_avp = avp;
	last_hash = skinp->mHash;
	last_shaderp = LLGLSLShader::sCurBoundShaderPtr;

	U32 count = LLSkinningUtil::initSkinningMatrixPalette(mat4a, skinp, avp);
	U32 idx = 0;
	for (U32 i = 0; i < count; ++i)
	{
		const F32* m = mat4a[i].mMatrix[0].getF32ptr();

		mp[idx++] = m[0];
		mp[idx++] = m[1];
		mp[idx++] = m[2];
		mp[idx++] = m[12];

		mp[idx++] = m[4];
		mp[idx++] = m[5];
		mp[idx++] = m[6];
		mp[idx++] = m[13];

		mp[idx++] = m[8];
		mp[idx++] = m[9];
		mp[idx++] = m[10];
		mp[idx++] = m[14];
	}

	LLGLSLShader* shaderp = LLGLSLShader::sCurBoundShaderPtr;
	shaderp->uniformMatrix3x4fv(LLShaderMgr::AVATAR_MATRIX, count, false, mp);
}

bool LLModelPreview::render()
{
	assert_main_thread();

	if (!mFMP)
	{
		llwarns << "Model Preview floater is gone !  Aborted." << llendl;
		return false;
	}

	LLMutexLock lock(this);
	mNeedsUpdate = false;

	bool edges = mViewOption["show_edges"];
	bool joint_overrides = mViewOption["show_joint_overrides"];
	bool joint_positions = mViewOption["show_joint_positions"];
	bool collision_volumes = mViewOption["show_collision_volumes"];
	bool skin_weight = mViewOption["show_skin_weight"];
	bool textures = mViewOption["show_textures"];
	bool physics = mViewOption["show_physics"];

	S32 width = getWidth();
	S32 height = getHeight();

	LLGLSUIDefault def;
	LLGLDisable no_blend(GL_BLEND);
	LLGLEnable cull(GL_CULL_FACE);
	// SL-12781 disable Z-buffer to render background color
	LLGLDepthTest depth(GL_FALSE);

	{
		gUIProgram.bind();

		// Clear background to grey
		gGL.matrixMode(LLRender::MM_PROJECTION);
		gGL.pushMatrix();
		gGL.loadIdentity();
		gGL.ortho(0.f, width, 0.f, height, -1.f, 1.f);

		gGL.matrixMode(LLRender::MM_MODELVIEW);
		gGL.pushMatrix();
		gGL.loadIdentity();

		gGL.color4fv(PREVIEW_CANVAS_COL.mV);
		gl_rect_2d_simple(width, height);

		gGL.matrixMode(LLRender::MM_PROJECTION);
		gGL.popMatrix();

		gGL.matrixMode(LLRender::MM_MODELVIEW);
		gGL.popMatrix();

		gUIProgram.unbind();
	}

	stop_glerror();

	bool has_skin_weights = false;
	bool upload_skin = mFMP->childGetValue("upload_skin").asBoolean();
	bool upload_joints = mFMP->childGetValue("upload_joints").asBoolean();

	if (upload_joints != mLastJointUpdate)
	{
		mLastJointUpdate = upload_joints;
		mFMP->clearSkinningInfo();
	}

	for (LLModelLoader::scene::iterator iter = mScene[mPreviewLOD].begin(),
										end = mScene[mPreviewLOD].end();
		 iter != end; ++iter)
	{
		for (LLModelLoader::model_instance_list_t::iterator
				model_iter = iter->second.begin(),
				model_end = iter->second.end();
			 model_iter != model_end; ++model_iter)
		{
			LLModelInstance& instance = *model_iter;
			LLModel* model = instance.mModel;
			model->mPelvisOffset = mPelvisZOffset;
			if (!model->mSkinWeights.empty())
			{
				has_skin_weights = true;
			}
		}
	}

	if (has_skin_weights && lodsReady())
	{
		// Model has skin weights: enable view options for skin weights and
		// joint positions
		if (!mLegacyRigFlags)
		{
#if 1
			if (mFirstSkinUpdate)
			{
				mFirstSkinUpdate = false;
				// Auto-enable weight upload if weights are present
				skin_weight = mViewOption["show_skin_weight"] = true;
				mFMP->childSetValue("upload_skin", true);
			}
#endif
			mFMP->enableViewOption("show_skin_weight");
			mFMP->setViewOptionEnabled("show_joint_overrides", skin_weight);
			mFMP->setViewOptionEnabled("show_joint_positions", skin_weight);
			mFMP->setViewOptionEnabled("show_collision_volumes", skin_weight);
			mFMP->childSetValue("show_skin_weight", skin_weight);
			if (skin_weight)
			{
				// Cannot display both physics and skin weights... HB
				mFMP->childSetValue("show_physics", false);
				mViewOption["show_physics"] = false;
			}
			mFMP->childEnable("upload_skin");
		}
		else if (mLegacyRigFlags & LEGACY_RIG_FLAG_NO_JOINT)
		{
			mFMP->childSetVisible("skin_no_joint", true);
		}
		else if (mLegacyRigFlags & LEGACY_RIG_FLAG_TOO_MANY_JOINTS)
		{
			mFMP->childSetVisible("skin_too_many_joints", true);
		}
		else if (mLegacyRigFlags & LEGACY_RIG_FLAG_UNKNOWN_JOINT)
		{
			mFMP->childSetVisible("skin_unknown_joint", true);
		}
	}
	else
	{
		mFMP->childDisable("upload_skin");
		mViewOption["show_skin_weight"] = false;
		mFMP->setViewOptionEnabled("show_skin_weight", false);
		mFMP->disableViewOption("show_skin_weight");
		mFMP->childSetValue("show_skin_weight", false);
		mFMP->disableViewOption("show_joint_overrides");
		mFMP->disableViewOption("show_joint_positions");
		mFMP->disableViewOption("show_collision_volumes");
		skin_weight = false;
	}

	if (upload_skin && !has_skin_weights)
	{
		// Cannot upload skin weights if model has no skin weights
		mFMP->childSetValue("upload_skin", false);
		upload_skin = false;
	}

	if (!upload_skin && upload_joints)
	{
		// Cannot upload joints if not uploading skin weights
		mFMP->childSetValue("upload_joints", false);
		upload_joints = false;
	}

	if (upload_skin && upload_joints)
	{
		mFMP->childEnable("lock_scale_if_joint_position");
	}
	else
	{
		mFMP->childDisable("lock_scale_if_joint_position");
		mFMP->childSetValue("lock_scale_if_joint_position", false);
	}

	// Only enable joint offsets if it passed the earlier critiquing
	if (mRigValidJointUpload)
	{
		mFMP->childSetEnabled("upload_joints", upload_skin);
	}

	if (upload_skin)
	{
		mFMP->updateSkinningInfo(upload_joints);
	}
	else
	{
		mFMP->clearSkinningInfo();
	}

	F32 explode = mFMP->childGetValue("physics_explode").asReal();

	// SL-12781 re-enable Z-buffer for 3D model preview
	LLGLDepthTest gls_depth(GL_TRUE);

	LLRect preview_rect = mFMP->getChildView("preview_panel")->getRect();
	F32 aspect = (F32)preview_rect.getWidth() / preview_rect.getHeight();
	gViewerCamera.setAspect(aspect);
	gViewerCamera.setViewNoBroadcast(gViewerCamera.getDefaultFOV() /
									 mCameraZoom);

	LLVector3 offset = mCameraOffset;
	LLVector3 target_pos = mPreviewTarget + offset;

	F32 z_near = 0.001f;
	F32 z_far = mCameraDistance * 10.0f + mPreviewScale.length() +
				mCameraOffset.length();

	if (skin_weight && mPreviewAvatar)
	{
		target_pos = mPreviewAvatar->getPositionAgent();
		z_near = 0.01f;
		z_far = 1024.f;

		// Render avatar previews every frame
		mNeedsUpdate = true;
	}

	if (gUsePBRShaders)
	{
		gObjectPreviewProgram.bind(skin_weight);
	}
	else
	{
		gObjectPreviewProgram.bind();
	}

	// Do not let environment settings influence our scene lighting.
	LLPreviewLighting preview_light;

	gGL.loadIdentity();

	LLQuaternion camera_rot = LLQuaternion(mCameraPitch, LLVector3::y_axis) *
							  LLQuaternion(mCameraYaw, LLVector3::z_axis);

	LLQuaternion av_rot = camera_rot;
	F32 cam_dist = skin_weight ? SKIN_WEIGHT_CAMERA_DISTANCE : mCameraDistance;
	gViewerCamera.setOriginAndLookAt(target_pos +
									 // Camera position
									 (LLVector3(cam_dist, 0.f, 0.f) + offset) *
									 av_rot,
									 // Up axis
									 LLVector3::z_axis,
									 // Point of interest
									 target_pos);

	z_near = llclamp(z_far * 0.001f, 0.001f, 0.1f);

	gViewerCamera.setPerspective(false, mOrigin.mX, mOrigin.mY, width, height,
								 false, z_near, z_far);

	gGL.pushMatrix();
	gGL.color4fv(PREVIEW_EDGE_COL.mV);

	constexpr U32 type_mask = LLVertexBuffer::MAP_VERTEX |
							  LLVertexBuffer::MAP_NORMAL |
							  LLVertexBuffer::MAP_TEXCOORD0;

	if (!mBaseModel.empty() && mVertexBuffer[5].empty())
	{
		genBuffers(-1, skin_weight);
	}

	if (!mModel[mPreviewLOD].empty())
	{
		LLTexUnit* unit0 = gGL.getTexUnit(0);

		mFMP->childEnable("reset_btn");

		bool regen = mVertexBuffer[mPreviewLOD].empty();
		if (!regen)
		{
			const std::vector<LLPointer<LLVertexBuffer> >& vb_vec =
				mVertexBuffer[mPreviewLOD].begin()->second;
			if (!vb_vec.empty())
			{
				const LLVertexBuffer* buff = vb_vec[0];
				regen = buff->hasDataType(LLVertexBuffer::TYPE_WEIGHT4) !=
							skin_weight;
			}
		}
		else
		{
			llinfos << "Vertex Buffer[" << mPreviewLOD << "]"
					<< " is empty; regenerating it..." << llendl;
			regen = true;
		}

		if (regen)
		{
			genBuffers(mPreviewLOD, skin_weight);
		}

		if (!skin_weight)
		{
			for (LLMeshUploadThread::instance_list_t::iterator
					iter = mUploadData.begin(), end = mUploadData.end();
				 iter != end; ++iter)
			{
				LLModelInstance& instance = *iter;

				LLModel* model = instance.mLOD[mPreviewLOD];
				if (!model)
				{
					continue;
				}

				gGL.pushMatrix();
				LLMatrix4 mat = instance.mTransform;
				gGL.multMatrix(mat.getF32ptr());

				for (U32 i = 0,
						 count = mVertexBuffer[mPreviewLOD][model].size();
					 i < count; ++i)
				{
					LLVertexBuffer* buffer =
						mVertexBuffer[mPreviewLOD][model][i];
					// Note: mask ignored in PBR rendering mode
					buffer->setBuffer(type_mask & buffer->getTypeMask());

					if (textures)
					{
						U32 mat_count = instance.mModel->mMaterialList.size();
						if (i < mat_count)
						{
							const std::string& binding =
								instance.mModel->mMaterialList[i];
							const LLImportMaterial& material =
								instance.mMaterial[binding];

							gGL.diffuseColor4fv(material.mDiffuseColor.mV);

							// Find the tex for this material, bind it, and add
							// it to our set
							LLViewerFetchedTexture* tex =
								bind_mat_diffuse_tex(material);
							if (tex)
							{
								mTextureSet.insert(tex);
							}
						}
					}
					else
					{
						gGL.diffuseColor4fv(PREVIEW_BASE_COL.mV);
					}

					buffer->drawRange(LLRender::TRIANGLES, 0,
									  buffer->getNumVerts() - 1,
									  buffer->getNumIndices(), 0);
					unit0->unbind(LLTexUnit::TT_TEXTURE);
					gGL.diffuseColor4fv(PREVIEW_EDGE_COL.mV);

					if (edges)
					{
						gGL.lineWidth(1.f);
						glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
						buffer->drawRange(LLRender::TRIANGLES, 0,
										  buffer->getNumVerts() - 1,
										  buffer->getNumIndices(), 0);
						glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
					}

					buffer->unmapBuffer();
				}
				gGL.popMatrix();
			}

			stop_glerror();

			if (physics)
			{
				glClear(GL_DEPTH_BUFFER_BIT);

				for (U32 pass = 0; pass < 2; ++pass)
				{
					if (pass == 0)
					{
						// Depth only pass
						gGL.setColorMask(false, false);
					}
					else
					{
						gGL.setColorMask(true, true);
					}

					// Enable alpha blending on second pass but not first pass
					LLGLState blend(GL_BLEND, pass);

					gGL.blendFunc(LLRender::BF_SOURCE_ALPHA,
								  LLRender::BF_ONE_MINUS_SOURCE_ALPHA);

					for (LLMeshUploadThread::instance_list_t::iterator
							iter = mUploadData.begin(),
							end = mUploadData.end();
						 iter != end; ++iter)
					{
						LLModelInstance& instance = *iter;

						LLModel* model = instance.mLOD[LLModel::LOD_PHYSICS];
						if (!model)
						{
							continue;
						}

						gGL.pushMatrix();
						LLMatrix4 mat = instance.mTransform;
						gGL.multMatrix(mat.getF32ptr());

						bool render_mesh = true;

						LLPhysicsDecomp* decomp = gMeshRepo.mDecompThread;
						if (decomp)
						{
							decomp->mMutex.lock();

							LLModel::Decomposition& physics = model->mPhysics;
							if (!physics.mHull.empty())
							{
								render_mesh = false;

								if (physics.mMesh.empty())
								{
									// Build vertex buffer for physics mesh
									gMeshRepo.buildPhysicsMesh(physics);
								}

								if (!physics.mMesh.empty())
								{
									LLGLSLShader* shaderp = NULL;
									if (gUsePBRShaders)
									{
										// Note: this could be either of
										// gObjectPreviewProgram or
										// gSkinnedObjectPreviewProgram. HB
										shaderp =
											LLGLSLShader::sCurBoundShaderPtr;
										if (shaderp)	// Paranoia
										{
											shaderp->unbind();
										}
										gPhysicsPreviewProgram.bind();
									}
									// Render hull instead of mesh
									for (U32 i = 0, count = physics.mMesh.size();
										 i < count; ++i)
									{
										if (explode > 0.f)
										{
											gGL.pushMatrix();

											LLVector3 offset =
												model->mHullCenter[i]-model->mCenterOfHullCenters;
											offset *= explode;

											gGL.translatef(offset.mV[0],
														   offset.mV[1],
														   offset.mV[2]);
										}

										static std::vector<LLColor4U> hull_colors;

										if (i + 1 >= hull_colors.size())
										{
											hull_colors.emplace_back(rand() % 128 + 127,
																	 rand() % 128 + 127,
																	 rand() % 128 + 127,
																	 128);
										}

										gGL.diffuseColor4ubv(hull_colors[i].mV);
										LLVertexBuffer::drawArrays(LLRender::TRIANGLES,
																   physics.mMesh[i].mPositions,
																   physics.mMesh[i].mNormals);

										if (explode > 0.f)
										{
											gGL.popMatrix();
										}
									}
									if (gUsePBRShaders)
									{
										gPhysicsPreviewProgram.unbind();
										if (shaderp)	// Paranoia
										{
											shaderp->bind();
										}
									}
								}
							}

							decomp->mMutex.unlock();
						}

						if (render_mesh)
						{
							if (mVertexBuffer[LLModel::LOD_PHYSICS].empty())
							{
								genBuffers(LLModel::LOD_PHYSICS, false);
							}
							if (pass > 0)
							{
								for (U32 i = 0,
										 count = mVertexBuffer[LLModel::LOD_PHYSICS][model].size();
									 i < count; ++i)
								{
									LLVertexBuffer* buffer =
										mVertexBuffer[LLModel::LOD_PHYSICS][model][i];

									unit0->unbind(LLTexUnit::TT_TEXTURE);
									gGL.diffuseColor4fv(PREVIEW_PHYS_FILL_COL.mV);

									// Note: mask ignored in PBR rendering mode
									buffer->setBuffer(type_mask &
													  buffer->getTypeMask());
									buffer->drawRange(LLRender::TRIANGLES, 0,
													  buffer->getNumVerts() - 1,
													  buffer->getNumIndices(), 0);

									gGL.diffuseColor4fv(PREVIEW_PHYS_EDGE_COL.mV);

									gGL.lineWidth(1.f);
									glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
									buffer->drawRange(LLRender::TRIANGLES, 0,
													  buffer->getNumVerts() - 1,
													  buffer->getNumIndices(), 0);

									glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

									buffer->unmapBuffer();
								}
							}
						}

						gGL.popMatrix();
					}

					// Show degenerate triangles, if any
					if (mHasDegenerate)
					{
						gGL.lineWidth(PREVIEW_DEG_EDGE_WIDTH);
						glPointSize(PREVIEW_DEG_POINT_SIZE);

						LLGLDepthTest depth(GL_TRUE, GL_TRUE, GL_ALWAYS);
						LLGLDisable cull(GL_CULL_FACE);
						gGL.diffuseColor4f(1.f, 0.f, 0.f, 1.f);
						const LLVector4a scale(0.5f);

						for (LLMeshUploadThread::instance_list_t::iterator
								iter = mUploadData.begin(),
								end = mUploadData.end();
							 iter != end; ++iter)
						{
							LLModelInstance& instance = *iter;

							LLModel* model = instance.mLOD[LLModel::LOD_PHYSICS];
							if (!model) continue;

							LLPhysicsDecomp* decomp = gMeshRepo.mDecompThread;
							if (!decomp) continue;

							gGL.pushMatrix();
							LLMatrix4 mat = instance.mTransform;
							gGL.multMatrix(mat.getF32ptr());

							decomp->mMutex.lock();

							LLModel::Decomposition& physics = model->mPhysics;
							if (physics.mHull.empty())
							{
								if (mVertexBuffer[LLModel::LOD_PHYSICS].empty())
								{
									genBuffers(LLModel::LOD_PHYSICS, false);
								}

								for (U32 i = 0,
										 count = mVertexBuffer[LLModel::LOD_PHYSICS][model].size();
									 i < count; ++i)
								{
									LLVertexBuffer* buffer =
										mVertexBuffer[LLModel::LOD_PHYSICS][model][i];

									// Note: mask ignored in PBR rendering mode
									buffer->setBuffer(type_mask &
													  buffer->getTypeMask());

									LLStrider<LLVector3> pos_strider;
									LLStrider<U16> idx;
									if (!buffer->getVertexStrider(pos_strider, 0) ||
										!buffer->getIndexStrider(idx, 0))
									{
										decomp->mMutex.unlock();
										gObjectPreviewProgram.bind();
										gGL.popMatrix();
										return false;
									}
									LLVector4a* pos = (LLVector4a*)pos_strider.get();

									for (S32 i = 0, count = buffer->getNumIndices();
										 i < count; i += 3)
									{
										LLVector4a v1; v1.setMul(pos[*idx++], scale);
										LLVector4a v2; v2.setMul(pos[*idx++], scale);
										LLVector4a v3; v3.setMul(pos[*idx++], scale);

										if (LLVolumeFace::isDegenerate(v1, v2, v3))
										{
											buffer->draw(LLRender::LINE_LOOP, 3, i);
											buffer->draw(LLRender::POINTS, 3, i);
										}
									}

									buffer->unmapBuffer();
								}
							}

							decomp->mMutex.unlock();

							gGL.popMatrix();
						}

						gGL.lineWidth(1.f);
						glPointSize(1.f);
						gGL.setSceneBlendType(LLRender::BT_ALPHA);
					}
					stop_glerror();
				}
			}
		}
		else if (mPreviewAvatar)
		{
			target_pos = mPreviewAvatar->getPositionAgent();

			mPreviewAvatar->clearAttachmentOverrides(); // Removes pelvis fixup
			LLUUID fake_mesh_id;
			fake_mesh_id.generate();
			mPreviewAvatar->addPelvisFixup(mPelvisZOffset, fake_mesh_id);
			bool pelvis_recalc = false;

			gViewerCamera.setOriginAndLookAt(target_pos +
											 (LLVector3(cam_dist, 0.f, 0.f) + offset) *
											 av_rot,
											 LLVector3::z_axis, target_pos);

			for (LLModelLoader::scene::iterator
					iter = mScene[mPreviewLOD].begin(),
					end = mScene[mPreviewLOD].end();
				 iter != end; ++iter)
			{
				for (LLModelLoader::model_instance_list_t::iterator
						model_iter = iter->second.begin(),
						model_end = iter->second.end();
					 model_iter != model_end; ++model_iter)
				{
					LLModelInstance& instance = *model_iter;

					LLModel* model = instance.mModel;
					if (!model || model->mSkinWeights.empty())
					{
						continue;
					}

					const LLMeshSkinInfo* skin = &model->mSkinInfo;

					U32 joint_count = llmin(LL_MAX_JOINTS_PER_MESH_OBJECT,
											(U32)skin->mJointKeys.size());
					U32 bind_count = skin->mAlternateBindMatrix.size();
					if (joint_overrides && bind_count &&
						joint_count == bind_count)
					{
						// Mesh Id is used to determine which mesh gets to set
						// the joint offset in the event of a conflict. Since
						// we do not know the mesh id yet, we cannot guarantee
						// that joint offsets will be applied with the same
						// priority as in the uploaded model. If the file
						// contains multiple meshes with conflicting joint
						// offsets, preview may be incorrect.
						LLUUID fake_mesh_id;
						fake_mesh_id.generate();
						for (U32 j = 0; j < joint_count; ++j)
						{
							LLJoint* joint =
								mPreviewAvatar->getJoint(skin->mJointKeys[j]);
							if (!joint) continue;

							const LLVector3& jpos =
								skin->mAlternateBindMatrix[j].getTranslation();
							if (!joint->aboveJointPosThreshold(jpos))
							{
								continue;
							}

							bool changed;
							joint->addAttachmentPosOverride(jpos, fake_mesh_id,
															"model", &changed);

							// If joint is a pelvis then handle old/new pelvis
							// to foot values
							if (changed &&
								skin->mJointKeys[j] == LL_JOINT_KEY_PELVIS)
							{
								pelvis_recalc = true;
							}

							if (skin->mLockScaleIfJointPosition)
							{
								// Note that unlike positions, there is no
								// threshold check here, just a lock at the
								// default value.
								joint->addAttachmentScaleOverride(joint->getDefaultScale(),
																  fake_mesh_id,
																  "model");
							}
						}
					}

					for (U32 i = 0,
							 count = mVertexBuffer[mPreviewLOD][model].size();
						 i < count; ++i)
					{
						LLVertexBuffer* buffer =
							mVertexBuffer[mPreviewLOD][model][i];
						if (gUsePBRShaders)
						{
							model->mSkinInfo.updateHash(true);	// true = force
							upload_matrix_palette(mPreviewAvatar,
												  &model->mSkinInfo);

							unit0->unbind(LLTexUnit::TT_TEXTURE);

							if (textures)
							{
								U32 mat_count =
									instance.mModel->mMaterialList.size();
								if (i < mat_count)
								{
									const std::string& binding =
										instance.mModel->mMaterialList[i];
									const LLImportMaterial& material =
										instance.mMaterial[binding];

									gGL.diffuseColor4fv(material.mDiffuseColor.mV);

									// Find the tex for this material, bind it,
									// and add it to our set
									LLViewerFetchedTexture* tex =
										bind_mat_diffuse_tex(material);
									if (tex)
									{
										mTextureSet.insert(tex);
									}
								}
							}
							else
							{
								gGL.diffuseColor4fv(PREVIEW_BASE_COL.mV);
							}
							buffer->setBuffer();
						}
						else
						{
							const LLVolumeFace& face = model->getVolumeFace(i);

							LLStrider<LLVector3> position;
							LLStrider<LLVector4a> weight;
							if (!buffer->getVertexStrider(position) ||
								!buffer->getWeight4Strider(weight))
							{
								gObjectPreviewProgram.bind();
								gGL.popMatrix();
								return false;
							}

							// Build matrix palette
							LLMatrix4a mat[LL_MAX_JOINTS_PER_MESH_OBJECT];
							LLSkinningUtil::initSkinningMatrixPalette(mat, skin,
																	  mPreviewAvatar);
							LLMatrix4a bind_shape_matrix;
							bind_shape_matrix.loadu(skin->mBindShapeMatrix);
							for (U32 j = 0, count2 = buffer->getNumVerts();
								 j < count2; ++j)
							{
								LLMatrix4a final_mat;
								LLSkinningUtil::getPerVertexSkinMatrix(weight[j],
																	   mat,
																	   final_mat,
																	   true);

								LLVector4a& v = face.mPositions[j];
								LLVector4a t;
								LLVector4a dst;
								bind_shape_matrix.affineTransform(v, t);
								final_mat.affineTransform(t, dst);
								position[j][0] = dst[0];
								position[j][1] = dst[1];
								position[j][2] = dst[2];
							}

							// Make sure there is a material set before
							// dereferencing it; if none, set the buffer type and
							// unbind the texture.
							if (instance.mModel->mMaterialList.size() > i &&
								instance.mMaterial.find(instance.mModel->mMaterialList[i]) !=
									instance.mMaterial.end())
							{
								const std::string& binding =
									instance.mModel->mMaterialList[i];
								const LLImportMaterial& material =
									instance.mMaterial[binding];

								// Note: mask ignored in PBR rendering mode
								buffer->setBuffer(type_mask &
												  buffer->getTypeMask());
								gGL.diffuseColor4fv(material.mDiffuseColor.mV);
								unit0->unbind(LLTexUnit::TT_TEXTURE);

								// Find the tex for this material, bind it and add
								// it to our set
								LLViewerFetchedTexture* tex =
									bind_mat_diffuse_tex(material);
								if (tex)
								{
									mTextureSet.insert(tex);
								}
							}
							else
							{
								buffer->setBuffer(type_mask &
												  buffer->getTypeMask());
								unit0->unbind(LLTexUnit::TT_TEXTURE);
							}
						}
						buffer->draw(LLRender::TRIANGLES,
									 buffer->getNumIndices(), 0);

						if (edges)
						{
							if (gUsePBRShaders)
							{
								unit0->unbind(LLTexUnit::TT_TEXTURE);
							}
							gGL.diffuseColor4fv(PREVIEW_EDGE_COL.mV);
							gGL.lineWidth(1.f);
							glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
							buffer->draw(LLRender::TRIANGLES,
										 buffer->getNumIndices(), 0);
							glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
						}

						buffer->unmapBuffer();
					}
				}
			}

			stop_glerror();

			if (joint_positions || collision_volumes)
			{
				LLGLSLShader* shaderp = LLGLSLShader::sCurBoundShaderPtr;
				if (shaderp)
				{
					gDebugProgram.bind();
				}
				if (collision_volumes)
				{
					mPreviewAvatar->renderCollisionVolumes();
				}
				if (joint_positions)
				{
					if (mFMP->mTabContainer->getCurrentPanel() ==
							mFMP->mModifiersPanel)
					{
						mPreviewAvatar->renderBones(mFMP->mSelectedJointName);
					}
					else
					{
						mPreviewAvatar->renderBones();
					}
					renderGroundPlane(mPelvisZOffset);
				}
				if (shaderp)
				{
					shaderp->bind();
				}
			}

			if (pelvis_recalc)
			{
				// Size/scale re-calculation
				mPreviewAvatar->postPelvisSetRecalc();
			}
		}
	}

	// Note: this could be either of gObjectPreviewProgram or its rigged
	// variant (gSkinnedObjectPreviewProgram). HB
	if (LLGLSLShader::sCurBoundShaderPtr)	// Paranoia
	{
		LLGLSLShader::sCurBoundShaderPtr->unbind();
	}

	gGL.popMatrix();

	return true;
}

void LLModelPreview::renderGroundPlane(F32 z_offset)
{
	gGL.diffuseColor3f(1.f, 0.f, 1.f);

	gGL.begin(LLRender::LINES);

	gGL.vertex3fv(mGroundPlane[0].mV);
	gGL.vertex3fv(mGroundPlane[1].mV);

	gGL.vertex3fv(mGroundPlane[1].mV);
	gGL.vertex3fv(mGroundPlane[2].mV);

	gGL.vertex3fv(mGroundPlane[2].mV);
	gGL.vertex3fv(mGroundPlane[3].mV);

	gGL.vertex3fv(mGroundPlane[3].mV);
	gGL.vertex3fv(mGroundPlane[0].mV);

	gGL.end();
}

void LLModelPreview::rotate(F32 yaw_radians, F32 pitch_radians)
{
	mCameraYaw = mCameraYaw + yaw_radians;
	constexpr F32 limit = 0.8f * F_PI_BY_TWO;
	mCameraPitch = llclamp(mCameraPitch + pitch_radians, -limit, limit);
}

void LLModelPreview::zoom(F32 zoom_amt)
{
	F32 new_zoom = mCameraZoom + zoom_amt;
	mCameraZoom	= llclamp(new_zoom, 1.f, PREVIEW_ZOOM_LIMIT);
}

void LLModelPreview::pan(F32 right, F32 up)
{
	bool skin_weight = mViewOption["show_skin_weight"];
	F32 cam_dist = skin_weight ? SKIN_WEIGHT_CAMERA_DISTANCE : mCameraDistance;
	mCameraOffset.mV[VY] = llclamp(mCameraOffset.mV[VY] +
								   right * cam_dist / mCameraZoom,
								   -1.f, 1.f);
	mCameraOffset.mV[VZ] = llclamp(mCameraOffset.mV[VZ] +
								   up * cam_dist / mCameraZoom,
								   -1.f, 1.f);
}

void LLModelPreview::setPreviewLOD(S32 lod)
{
	lod = llclamp(lod, 0, (S32)LLModel::LOD_HIGH);

	if (lod != mPreviewLOD && mFMP)
	{
		mPreviewLOD = lod;

		LLComboBox* combo_box =
			mFMP->getChild<LLComboBox>("preview_lod_combo");
		// Combo box list of lods is in reverse order
		combo_box->setCurrentByIndex(NUM_LOD - 1 - mPreviewLOD);
		std::string cname = "lod_file_" + lod_name[mPreviewLOD];
		mFMP->childSetText(cname.c_str(), mLODFile[mPreviewLOD]);

		LLColor4 highlight_color =
			gColors.getColor("MeshImportTableHighlightColor");
		LLColor4 normal_color = gColors.getColor("MeshImportTableNormalColor");

		for (S32 i = 0; i <= LLModel::LOD_HIGH; ++i)
		{
			const LLColor4& color = i == lod ? highlight_color : normal_color;

			mFMP->childSetColor(lod_status_name[i], color);
			mFMP->childSetColor(lod_label_name[i], color);
			mFMP->childSetColor(lod_triangles_name[i], color);
			mFMP->childSetColor(lod_vertices_name[i], color);
		}

		// Make preview repopulate info
		mFMP->clearSkinningInfo();
	}
	refresh();
}

//static
void LLModelPreview::textureLoadedCallback(bool success,
										   LLViewerFetchedTexture* src_vi,
										   LLImageRaw* src,
										   LLImageRaw* src_aux,
										   S32 discard_level, bool is_final,
										   void* userdata)
{
	// Not the best solution, but the model preview belongs to the floater, so
	// it is an easy way to check that the preview still exists.
	LLFloaterModelPreview* fmp = LLFloaterModelPreview::findInstance();
	if (!fmp) return;	// Floater gone !

	LLModelPreview* self = (LLModelPreview*)userdata;
	if (!self || self != fmp->mModelPreview) return;	// Preview changed

	self->refresh();

	if (is_final && self->mModelLoader)
	{
		if (self->mModelLoader->mNumOfFetchingTextures > 0)
		{
			--self->mModelLoader->mNumOfFetchingTextures;
		}
	}
}

//static
bool LLModelPreview::lodQueryCallback()
{
	// Not the best solution, but the model preview belongs to the floater, so
	// it is an easy way to check that the preview still exists.
	LLFloaterModelPreview* fmp = LLFloaterModelPreview::findInstance();
	if (!fmp) return true;	// Floater gone !

	LLModelPreview* self = fmp->mModelPreview;
	if (self)
	{
		if (!self->mLodsQuery.empty())
		{
			S32 lod = self->mLodsQuery.back();
			// *HACK: when GLOD fails, try genMeshOptimizerLODs()... HB
			if (!self->genGlodLODs(lod))
			{
				llwarns << "GLOD failed, trying with meshoptimizer." << llendl;
				self->genMeshOptimizerLODs(lod, MESH_OPTIMIZER_AUTO);
			}
			self->mLodsQuery.pop_back();

			// Return false to continue the LOD generation cycle when
			// mLodsQuery is not empty
			return self->mLodsQuery.empty();
		}
	}

	// Nothing left to process
	return true;
}

void LLModelPreview::onLODParamCommit(S32 lod, bool enforce_tri_limit)
{
	if (!mFMP || mLODFrozen)
	{
		return;
	}

	std::string cname = "lod_source_" + lod_name[lod];
	S32 lod_mode =
		mFMP->getChild<LLComboBox>(cname.c_str())->getCurrentIndex();
	if (lod_mode == GENERATE)
	{
		// *HACK: when GLOD fails, try genMeshOptimizerLODs()... HB
		if (!genGlodLODs(lod, 3, enforce_tri_limit))
		{
			llwarns << "GLOD failed, trying with meshoptimizer." << llendl;
			genMeshOptimizerLODs(lod, MESH_OPTIMIZER_AUTO, 3,
								 enforce_tri_limit);
		}	
		refresh();
	}
	else if (lod_mode > GENERATE && lod_mode < USE_LOD_ABOVE)
	{
		genMeshOptimizerLODs(lod, lod_mode, 3, enforce_tri_limit);
		refresh();
	}
}
