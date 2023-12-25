/**
 * @file llfloateranimpreview.cpp
 * @brief LLFloaterAnimPreview class implementation
 *
 * $LicenseInfo:firstyear=2004&license=viewergpl$
 *
 * Copyright (c) 2004-2009, Linden Research, Inc.
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

#include "llfloateranimpreview.h"

#include "llanimationstates.h"
#include "llbbox.h"
#include "llbboxlocal.h"
#include "llbutton.h"
#include "llbvhloader.h"
#include "lldatapacker.h"
#include "lldir.h"
#include "llfilesystem.h"
#include "llkeyframemotion.h"
#include "llrender.h"
#include "llresizehandle.h"			// For RESIZE_HANDLE_WIDTH
#include "lluictrlfactory.h"

#include "llagent.h"
#include "lldrawable.h"
#include "lldrawpoolavatar.h"
#include "llface.h"
#include "llfloaterperms.h"
#include "llpipeline.h"
#include "lltoolmgr.h"				// For MASK_* constants
#include "llviewerassetupload.h"	// For upload_new_resource()
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerobjectlist.h"
#include "llviewershadermgr.h"
#include "llviewerwindow.h"
#include "llvoavatarself.h"

constexpr S32 PREVIEW_BORDER_WIDTH = 2;
constexpr S32 PREVIEW_RESIZE_HANDLE_SIZE = S32(RESIZE_HANDLE_WIDTH * OO_SQRT2) +
										   PREVIEW_BORDER_WIDTH;
constexpr S32 PREVIEW_HPAD = PREVIEW_RESIZE_HANDLE_SIZE;
constexpr S32 PREF_BUTTON_HEIGHT = 16;
constexpr S32 PREVIEW_TEXTURE_HEIGHT = 300;

constexpr F32 PREVIEW_CAMERA_DISTANCE = 4.f;

constexpr F32 MIN_CAMERA_ZOOM = 0.5f;
constexpr F32 MAX_CAMERA_ZOOM = 10.f;

constexpr F32 BASE_ANIM_TIME_OFFSET = 5.f;

static std::string STATUS[] =
{
	"E_ST_OK",
	"E_ST_EOF",
	"E_ST_NO_CONSTRAINT",
	"E_ST_NO_FILE",
	"E_ST_NO_HIER",
	"E_ST_NO_JOINT",
	"E_ST_NO_NAME",
	"E_ST_NO_OFFSET",
	"E_ST_NO_CHANNELS",
	"E_ST_NO_ROTATION",
	"E_ST_NO_AXIS",
	"E_ST_NO_MOTION",
	"E_ST_NO_FRAMES",
	"E_ST_NO_FRAME_TIME",
	"E_ST_NO_POS",
	"E_ST_NO_ROT",
	"E_ST_NO_XLT_FILE",
	"E_ST_NO_XLT_HEADER",
	"E_ST_NO_XLT_NAME",
	"E_ST_NO_XLT_IGNORE",
	"E_ST_NO_XLT_RELATIVE",
	"E_ST_NO_XLT_OUTNAME",
	"E_ST_NO_XLT_MATRIX",
	"E_ST_NO_XLT_MERGECHILD",
	"E_ST_NO_XLT_MERGEPARENT",
	"E_ST_NO_XLT_PRIORITY",
	"E_ST_NO_XLT_LOOP",
	"E_ST_NO_XLT_EASEIN",
	"E_ST_NO_XLT_EASEOUT",
	"E_ST_NO_XLT_HAND",
	"E_ST_NO_XLT_EMOTE",
	"E_ST_BAD_ROOT"
};

//-----------------------------------------------------------------------------
// LLFloaterAnimPreview()
//-----------------------------------------------------------------------------
LLFloaterAnimPreview::LLFloaterAnimPreview(const std::string& filename)
:	HBFloaterUploadAsset(filename, LLInventoryType::IT_ANIMATION),
	mAnimPreview(NULL),
	mInWorld(false),
	mBadAnimation(false)
{
	mLastMouseX = 0;
	mLastMouseY = 0;

	mIDList["Standing"] = ANIM_AGENT_STAND;
	mIDList["Walking"] = ANIM_AGENT_FEMALE_WALK;
	mIDList["Sitting"] = ANIM_AGENT_SIT_FEMALE;
	mIDList["Flying"] = ANIM_AGENT_HOVER;

	mIDList["[None]"] = LLUUID::null;
	mIDList["Aaaaah"] = ANIM_AGENT_EXPRESS_OPEN_MOUTH;
	mIDList["Afraid"] = ANIM_AGENT_EXPRESS_AFRAID;
	mIDList["Angry"] = ANIM_AGENT_EXPRESS_ANGER;
	mIDList["Big Smile"] = ANIM_AGENT_EXPRESS_TOOTHSMILE;
	mIDList["Bored"] = ANIM_AGENT_EXPRESS_BORED;
	mIDList["Cry"] = ANIM_AGENT_EXPRESS_CRY;
	mIDList["Disdain"] = ANIM_AGENT_EXPRESS_DISDAIN;
	mIDList["Embarrassed"] = ANIM_AGENT_EXPRESS_EMBARRASSED;
	mIDList["Frown"] = ANIM_AGENT_EXPRESS_FROWN;
	mIDList["Kiss"] = ANIM_AGENT_EXPRESS_KISS;
	mIDList["Laugh"] = ANIM_AGENT_EXPRESS_LAUGH;
	mIDList["Plllppt"] = ANIM_AGENT_EXPRESS_TONGUE_OUT;
	mIDList["Repulsed"] = ANIM_AGENT_EXPRESS_REPULSED;
	mIDList["Sad"] = ANIM_AGENT_EXPRESS_SAD;
	mIDList["Shrug"] = ANIM_AGENT_EXPRESS_SHRUG;
	mIDList["Smile"] = ANIM_AGENT_EXPRESS_SMILE;
	mIDList["Surprise"] = ANIM_AGENT_EXPRESS_SURPRISE;
	mIDList["Wink"] = ANIM_AGENT_EXPRESS_WINK;
	mIDList["Worry"] = ANIM_AGENT_EXPRESS_WORRY;

	mPlayImage = LLUI::getUIImage("button_anim_play.tga");
	mPlaySelectedImage = LLUI::getUIImage("button_anim_play_selected.tga");
	mPauseImage = LLUI::getUIImage("button_anim_pause.tga");
	mPauseSelectedImage = LLUI::getUIImage("button_anim_pause_selected.tga");

	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_animation_preview.xml");
}

//virtual
LLFloaterAnimPreview::~LLFloaterAnimPreview()
{
	if (mInWorld && isAgentAvatarValid())
	{
		if (mMotionID.notNull())
		{
			gAgentAvatarp->stopMotion(mMotionID, true);
			gAgentAvatarp->removeMotion(mMotionID);
		}
		gAgentAvatarp->deactivateAllMotions();
		gAgentAvatarp->startMotion(ANIM_AGENT_STAND, BASE_ANIM_TIME_OFFSET);
		gAgentAvatarp->startDefaultMotions();
	}
	mAnimPreview = NULL;

	setEnabled(false);
}

//virtual
bool LLFloaterAnimPreview::postBuild()
{
	LLRect r;
	LLBVHLoader* loaderp = NULL;

	if (!HBFloaterUploadAsset::postBuild())
	{
		return false;
	}

	mInWorld = isAgentAvatarValid() &&
			   gSavedSettings.getBool("PreviewAnimInWorld");

	childSetCommitCallback("name_form", onCommitName, this);

	setDefaultBtn();

	if (mInWorld)
	{
		r = getRect();
		translate(0, 230);
		reshape(r.getWidth(), r.getHeight() - 230);
		childSetValue("bad_animation_text", getString("in_world"));
		childShow("bad_animation_text");
	}
	else
	{
		childHide("bad_animation_text");
	}

	mPreviewRect.set(PREVIEW_HPAD, PREVIEW_TEXTURE_HEIGHT,
					 getRect().getWidth() - PREVIEW_HPAD,
					 PREVIEW_HPAD + PREF_BUTTON_HEIGHT + PREVIEW_HPAD);
	mPreviewImageRect.set(0.f, 1.f, 1.f, 0.f);

	S32 y = mPreviewRect.mTop + gBtnHeight;
	S32 btn_left = PREVIEW_HPAD;

	r.set(btn_left, y, btn_left + 32, y - gBtnHeight);
	mPlayButton = getChild<LLButton>("play_btn");
	mPlayButton->setClickedCallback(onBtnPlay);
	mPlayButton->setCallbackUserData(this);
	mPlayButton->setImageUnselected(mPlayImage);
	mPlayButton->setImageSelected(mPlaySelectedImage);
	mPlayButton->setDisabledImages(LLStringUtil::null, LLStringUtil::null);
	mPlayButton->setScaleImage(true);

	mStopButton = getChild<LLButton>("stop_btn");
	mStopButton->setClickedCallback(onBtnStop);
	mStopButton->setCallbackUserData(this);
	mStopButton->setImages("button_anim_stop.tga",
						   "button_anim_stop_selected.tga");
	mStopButton->setDisabledImages(LLStringUtil::null, LLStringUtil::null);
	mStopButton->setScaleImage(true);

	r.set(r.mRight + PREVIEW_HPAD, y, getRect().getWidth() - PREVIEW_HPAD,
		  y - gBtnHeight);

	if (!mInWorld)
	{
		mAnimPreview = new LLPreviewAnimation(256, 256);
	}

	std::string exten = gDirUtilp->getExtension(mFilename);
	if (exten == "bvh")
	{
		// Loading a bvh file
		S64 file_size = 0;
		LLFile infile(mFilenameAndPath, "rb", &file_size);
		if (infile)
		{
			char* file_buffer = new char[file_size + 1];

			if (infile.read((U8*)file_buffer, file_size) == file_size)
			{
				file_buffer[file_size] = '\0';
				ELoadStatus load_status = E_ST_OK;
				S32 line_number = 0;
				llinfos << "Loading BVH file " << mFilename << llendl;
				std::map<std::string, std::string> joint_alias_map =
					getJointAliases();
				loaderp = new LLBVHLoader(file_buffer, load_status,
										  line_number, joint_alias_map);
				if (load_status == E_ST_NO_XLT_FILE)
				{
					llwarns << "NOTE: No translation table found." << llendl;
				}
				else if (load_status != E_ST_OK)
				{
					llwarns << "ERROR: [line: " << line_number << "] "
							<< getString(STATUS[load_status]) << llendl;
				}
			}

			delete[] file_buffer;
		}
		else
		{
			llwarns << "Cannot open BVH file: " << mFilename << llendl;
		}
	}

	F32 max_anim_duration =
		llclamp(gSavedSettings.getF32("AnimationsMaxDuration"), 30.f,
				ABSOLUTE_MAX_ANIM_DURATION);
	if (loaderp && loaderp->isInitialized() &&
		loaderp->getDuration() <= max_anim_duration)
	{
		// Generate an unique Id for this motion
		mTransactionID.generate();
		mMotionID = mTransactionID.makeAssetID(gAgent.getSecureSessionID());
		// Motion will be returned, but it will be in a load-pending state, as
		// this is a new motion this motion will not request an asset transfer
		// until next update, so we have a chance to load the keyframe data
		// locally
		LLKeyframeMotion* motionp =
			mInWorld ?
			(LLKeyframeMotion*)gAgentAvatarp->createMotion(mMotionID) :
			(LLKeyframeMotion*)mAnimPreview->getDummyAvatar()->createMotion(mMotionID);

		// Create data buffer for keyframe initialization
		S32 buffer_size = loaderp->getOutputSize();
		U8* buffer = new U8[buffer_size];

		LLDataPackerBinaryBuffer dp(buffer, buffer_size);

		// Pass animation data through memory buffer
		llinfos << "Serializing loader..." << llendl;
		loaderp->serialize(dp);
		dp.reset();
		llinfos << "De-serializing motions..." << llendl;
		bool success = motionp && motionp->deserialize(dp, mMotionID, false);
		llinfos << "Done." << llendl;

		delete[] buffer;

		if (success)
		{
			setAnimCallbacks();

			if (!mInWorld)
			{
				const LLBBoxLocal& pelvis_bbox = motionp->getPelvisBBox();

				LLVector3 temp = pelvis_bbox.getCenter();
#if 0			// only consider XY ?
				temp.mV[VZ] = 0.f;
#endif
				F32 pelvis_offset = temp.length();
				temp = pelvis_bbox.getExtent();
#if 0
				temp.mV[VZ] = 0.f;
#endif
				F32 pelvis_max_displacement =
					pelvis_offset + 0.5f * temp.length() + 1.f;

				F32 camera_zoom =
					gViewerCamera.getDefaultFOV() * 0.5f /
					tanf(pelvis_max_displacement / PREVIEW_CAMERA_DISTANCE);

				mAnimPreview->setZoom(camera_zoom);
			}

			motionp->setName(childGetValue("name_form").asString());
			if (!mInWorld)
			{
				mAnimPreview->getDummyAvatar()->startMotion(mMotionID);
			}
			childSetMinValue("playback_slider", 0.0);
			childSetMaxValue("playback_slider", 1.0);

			childSetValue("loop_check", LLSD(motionp->getLoop()));
			childSetValue("loop_in_point",
						  LLSD(motionp->getLoopIn() /
							   motionp->getDuration() * 100.f));
			childSetValue("loop_out_point",
						  LLSD(motionp->getLoopOut() /
							   motionp->getDuration() * 100.f));
			childSetMaxValue("priority",
							 (F32)llclamp(gSavedSettings.getU32("AnimationsMaxPriority"),
										  4U, 5U));
			childSetValue("priority", LLSD((F32)motionp->getPriority()));
			childSetValue("hand_pose_combo",
						  LLHandMotion::getHandPoseName(motionp->getHandPose()));
			childSetValue("ease_in_time", LLSD(motionp->getEaseInDuration()));
			childSetValue("ease_out_time", LLSD(motionp->getEaseOutDuration()));
			setEnabled(true);
			std::string seconds_string;
			seconds_string = llformat(" - %.2f seconds", motionp->getDuration());

			setTitle(mFilename + std::string(seconds_string));
		}
		else
		{
			mAnimPreview = NULL;
			mMotionID.setNull();
			childSetValue("bad_animation_text",
						  getString("failed_to_initialize"));
		}
	}
	else
	{
		if (loaderp)
		{
			if (loaderp->getDuration() > max_anim_duration)
			{
				LLUIString out_str = getString("anim_too_long");
				out_str.setArg("[LENGTH]",
							   llformat("%.1f", loaderp->getDuration()));
				out_str.setArg("[MAX_LENGTH]",
							   llformat("%.1f", max_anim_duration));
				childSetValue("bad_animation_text", out_str.getString());
			}
			else
			{
				LLUIString out_str = getString("failed_file_read");
				// *TODO:Translate
				out_str.setArg("[STATUS]",
							   getString(STATUS[loaderp->getStatus()]));
				childSetValue("bad_animation_text", out_str.getString());
				mBadAnimation = true;
				mUploadButton->setEnabled(false);
			}
		}

		//setEnabled(false);
		mMotionID.setNull();
		mAnimPreview = NULL;
	}

	refresh();

	delete loaderp;

	return true;
}

void LLFloaterAnimPreview::setAnimCallbacks()
{
	childSetCommitCallback("playback_slider", onSliderMove, this);

	childSetCommitCallback("preview_base_anim", onCommitBaseAnim, this);
	childSetValue("preview_base_anim", "Standing");

	childSetCommitCallback("priority", onCommitPriority, this);
	childSetCommitCallback("loop_check", onCommitLoop, this);
	childSetCommitCallback("loop_in_point", onCommitLoopIn, this);
	childSetValidate("loop_in_point", validateLoopIn);
	childSetCommitCallback("loop_out_point", onCommitLoopOut, this);
	childSetValidate("loop_out_point", validateLoopOut);

	childSetCommitCallback("hand_pose_combo", onCommitHandPose, this);

	childSetCommitCallback("emote_combo", onCommitEmote, this);
	childSetValue("emote_combo", "[None]");

	childSetCommitCallback("ease_in_time", onCommitEaseIn, this);
	childSetValidate("ease_in_time", validateEaseIn);
	childSetCommitCallback("ease_out_time", onCommitEaseOut, this);
	childSetValidate("ease_out_time", validateEaseOut);
}

//virtual
void LLFloaterAnimPreview::draw()
{
	refresh();
	LLFloater::draw();

	if (!mInWorld && mAnimPreview && mMotionID.notNull())
	{
		gGL.color3f(1.f, 1.f, 1.f);

		LLTexUnit* unit0 = gGL.getTexUnit(0);
		unit0->bind(mAnimPreview);

		gGL.begin(LLRender::TRIANGLES);
		{
			S32 right = getRect().getWidth() - PREVIEW_HPAD;
			gGL.texCoord2f(0.f, 1.f);
			gGL.vertex2i(PREVIEW_HPAD, PREVIEW_TEXTURE_HEIGHT);
			gGL.texCoord2f(0.f, 0.f);
			gGL.vertex2i(PREVIEW_HPAD,
						 PREVIEW_HPAD + PREF_BUTTON_HEIGHT + PREVIEW_HPAD);
			gGL.texCoord2f(1.f, 0.f);
			gGL.vertex2i(right,
						 PREVIEW_HPAD + PREF_BUTTON_HEIGHT + PREVIEW_HPAD);
			gGL.texCoord2f(0.f, 1.f);
			gGL.vertex2i(PREVIEW_HPAD, PREVIEW_TEXTURE_HEIGHT);
			gGL.texCoord2f(1.f, 0.f);
			gGL.vertex2i(right,
						 PREVIEW_HPAD + PREF_BUTTON_HEIGHT + PREVIEW_HPAD);
			gGL.texCoord2f(1.f, 1.f);
			gGL.vertex2i(right, PREVIEW_TEXTURE_HEIGHT);
		}
		gGL.end();

		unit0->unbind(LLTexUnit::TT_TEXTURE);
	}
}

//virtual
void LLFloaterAnimPreview::refresh()
{
	if (mBadAnimation ||
		(!mAnimPreview && !(mInWorld && isAgentAvatarValid())))
	{
		childShow("bad_animation_text");
		mPlayButton->setEnabled(false);
		mStopButton->setEnabled(false);
		mUploadButton->setEnabled(false);
	}
	else
	{
		if (!mInWorld)
		{
			childHide("bad_animation_text");
		}
		mPlayButton->setEnabled(true);

		LLVOAvatar* avatarp = getAvatar();
		if (!avatarp)
		{
			return;
		}

		if (avatarp->isMotionActive(mMotionID))
		{
			mStopButton->setEnabled(true);
			if (avatarp->areAnimationsPaused())
			{
				mPlayButton->setImageUnselected(mPlayImage);
				mPlayButton->setImageSelected(mPlaySelectedImage);
			}
			else
			{
				LLKeyframeMotion* motionp = getMotion();
				if (motionp)
				{
					F32 fraction_complete = motionp->getLastUpdateTime() /
											motionp->getDuration();
					childSetValue("playback_slider", fraction_complete);
				}
				mPlayButton->setImageUnselected(mPauseImage);
				mPlayButton->setImageSelected(mPauseSelectedImage);
			}
		}
		else
		{
			mPauseRequest = avatarp->requestPause();
			mPlayButton->setImageUnselected(mPlayImage);
			mPlayButton->setImageSelected(mPlaySelectedImage);

			mStopButton->setEnabled(true); // stop also resets, leave enabled.
		}
		mUploadButton->setEnabled(true);
	}
}

//virtual
bool LLFloaterAnimPreview::handleMouseDown(S32 x, S32 y, MASK mask)
{
	if (!mInWorld && mPreviewRect.pointInRect(x, y))
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
bool LLFloaterAnimPreview::handleMouseUp(S32 x, S32 y, MASK mask)
{
	if (!mInWorld)
	{
		gFocusMgr.setMouseCapture(NULL);
		gViewerWindowp->showCursor();
	}
	return LLFloater::handleMouseUp(x, y, mask);
}

//virtual
bool LLFloaterAnimPreview::handleHover(S32 x, S32 y, MASK mask)
{
	if (mInWorld)
	{
		return true;
	}

	MASK local_mask = mask & ~MASK_ALT;

	if (mAnimPreview && hasMouseCapture())
	{
		if (local_mask == MASK_PAN)
		{
			// pan here
			mAnimPreview->pan((F32)(x - mLastMouseX) * -0.005f,
							  (F32)(y - mLastMouseY) * -0.005f);
		}
		else if (local_mask == MASK_ORBIT)
		{
			F32 yaw_radians = (F32)(x - mLastMouseX) * -0.01f;
			F32 pitch_radians = (F32)(y - mLastMouseY) * 0.02f;

			mAnimPreview->rotate(yaw_radians, pitch_radians);
		}
		else
		{
			F32 yaw_radians = (F32)(x - mLastMouseX) * -0.01f;
			F32 zoom_amt = (F32)(y - mLastMouseY) * 0.02f;

			mAnimPreview->rotate(yaw_radians, 0.f);
			mAnimPreview->zoom(zoom_amt);
		}

		LLUI::setCursorPositionLocal(this, mLastMouseX, mLastMouseY);
	}

	if (!mPreviewRect.pointInRect(x, y) || !mAnimPreview)
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
bool LLFloaterAnimPreview::handleScrollWheel(S32 x, S32 y, S32 clicks)
{
	if (!mInWorld)
	{
		mAnimPreview->zoom((F32)clicks * -0.2f);
	}
	return true;
}

//virtual
void LLFloaterAnimPreview::onMouseCaptureLost()
{
	if (!mInWorld)
	{
		gViewerWindowp->showCursor();
	}
}

std::map <std::string, std::string> LLFloaterAnimPreview::getJointAliases()
{
	LLPointer<LLVOAvatar> av = mInWorld ? (LLVOAvatar*)gAgentAvatarp
										: mAnimPreview->getDummyAvatar();
	return av->getJointAliases();
}

LLKeyframeMotion* LLFloaterAnimPreview::getMotion()
{
	LLMotion* motionp = NULL;
	LLVOAvatar* avatarp = getAvatar();
	if (avatarp)
	{
		motionp = avatarp->findMotion(mMotionID);
	}
	return motionp ? motionp->asKeyframeMotion() : NULL;
}

void LLFloaterAnimPreview::resetMotion()
{
	LLVOAvatar* avatarp = getAvatar();
	if (!avatarp)
	{
		return;
	}

	bool paused = avatarp->areAnimationsPaused();

	LLKeyframeMotion* motionp = getMotion();
	if (motionp)
	{
		// Set emotion
		std::string emote = childGetValue("emote_combo").asString();
		motionp->setEmote(mIDList[emote]);
	}

	LLUUID base_id = mIDList[childGetValue("preview_base_anim").asString()];
	avatarp->deactivateAllMotions();
	avatarp->startMotion(base_id, BASE_ANIM_TIME_OFFSET);
	avatarp->startMotion(mMotionID, 0.0f);
	childSetValue("playback_slider", 0.0f);

	// Set pose
	std::string handpose = childGetValue("hand_pose_combo").asString();
	avatarp->startMotion(ANIM_AGENT_HAND_MOTION, 0.f);
	if (motionp)
	{
		motionp->setHandPose(LLHandMotion::getHandPose(handpose));
	}

	if (paused)
	{
		mPauseRequest = avatarp->requestPause();
	}
	else
	{
		mPauseRequest = NULL;
	}
}

LLVOAvatar* LLFloaterAnimPreview::getAvatar()
{
	LLVOAvatar* avatarp = NULL;
	if (mInWorld)
	{
		if (isAgentAvatarValid())
		{
			avatarp = gAgentAvatarp.get();
		}
	}
	else if (mAnimPreview)
	{
		avatarp = mAnimPreview->getDummyAvatar();
	}
	return avatarp;
}

//virtual
void LLFloaterAnimPreview::uploadAsset()
{
	if (!getEnabled()) return;

	LLKeyframeMotion* motionp = getMotion();
	if (!motionp)
	{
		return;
	}

	// Make sure the animation is stopped since we sill destroy the motion once
	// uploaded
	LLVOAvatar* avatarp = getAvatar();
	avatarp->stopMotion(mMotionID, true);

	S32 file_size = motionp->getFileSize();
	U8* buffer = new U8[file_size];

	LLDataPackerBinaryBuffer dp(buffer, file_size);
	if (motionp->serialize(dp))
	{
		LLFileSystem file(motionp->getID(), LLFileSystem::APPEND);

		S32 size = dp.getCurrentSize();
		if (file.write((U8*)buffer, size))
		{
			LLResourceUploadInfo::ptr_t
				info(new LLResourceUploadInfo(mTransactionID,
											  LLAssetType::AT_ANIMATION,
											  mNameEditor->getText(),
											  mDescEditor->getText(), 0,
											  LLFolderType::FT_ANIMATION,
											  LLInventoryType::IT_ANIMATION,
											  LLFloaterPerms::getNextOwnerPerms(),
											  LLFloaterPerms::getGroupPerms(),
											  LLFloaterPerms::getEveryonePerms(),
											  mCost));
			upload_new_resource(info);
		}
		else
		{
			llwarns << "Failure writing animation data." << llendl;
			gNotifications.add("WriteAnimationFail");
		}
	}

	delete[] buffer;

	// Clear out cache for motion data
	avatarp->removeMotion(mMotionID);
	if (mInWorld)
	{
		avatarp->deactivateAllMotions();
	}
	LLKeyframeDataCache::removeKeyframeData(mMotionID);
}

//static
void LLFloaterAnimPreview::onBtnPlay(void* userdata)
{
	LLFloaterAnimPreview* self = (LLFloaterAnimPreview*)userdata;
	if (!self || !self->getEnabled())
	{
		return;
	}

	if (self->mMotionID.notNull())
	{
		LLVOAvatar* avatarp = self->getAvatar();
		if (avatarp)
		{
			if (!avatarp->isMotionActive(self->mMotionID))
			{
				self->resetMotion();
				self->mPauseRequest = NULL;
			}
			else
			{
				if (avatarp->areAnimationsPaused())
				{
					self->mPauseRequest = NULL;
				}
				else
				{
					self->mPauseRequest = avatarp->requestPause();
				}
			}
		}
	}
}

//static
void LLFloaterAnimPreview::onBtnStop(void* userdata)
{
	LLFloaterAnimPreview* self = (LLFloaterAnimPreview*)userdata;
	if (!self || !self->getEnabled())
	{
		return;
	}

	if (self->mMotionID.notNull())
	{
		LLVOAvatar* avatarp = self->getAvatar();
		if (avatarp)
		{
#if 0
			self->resetMotion();
			self->mPauseRequest = avatarp->requestPause();
#else
			F32 loop_in = self->childGetValue("loop_in_point").asReal();
			F32 playback = self->childGetValue("playback_slider").asReal();
			bool stop_now = !self->childGetValue("loop_check").asBoolean() ||
							loop_in > playback * 100.f;
			avatarp->stopMotion(self->mMotionID, stop_now);
#endif
		}
	}
}

//static
void LLFloaterAnimPreview::onSliderMove(LLUICtrl* ctrl, void* userdata)
{
	LLFloaterAnimPreview* self = (LLFloaterAnimPreview*)userdata;
	if (!self || !self->getEnabled())
	{
		return;
	}

	LLVOAvatar* avatarp = self->getAvatar();
	if (!avatarp)
	{
		return;
	}

	LLUUID base_id =
		self->mIDList[self->childGetValue("preview_base_anim").asString()];
	LLMotion* motionp = avatarp->findMotion(self->mMotionID);
	if (!motionp)
	{
		return;

	}
	F32 duration = motionp->getDuration();// + motionp->getEaseOutDuration();
	F32 delta_time = duration *
					 (F32)self->childGetValue("playback_slider").asReal();
	avatarp->deactivateAllMotions();
	avatarp->startMotion(base_id, delta_time + BASE_ANIM_TIME_OFFSET);
	avatarp->startMotion(self->mMotionID, delta_time);
	self->mPauseRequest = avatarp->requestPause();
	self->refresh();
}

//static
void LLFloaterAnimPreview::onCommitBaseAnim(LLUICtrl*, void* userdata)
{
	LLFloaterAnimPreview* self = (LLFloaterAnimPreview*)userdata;
	if (!self || !self->getEnabled())
	{
		return;
	}

	LLVOAvatar* avatarp = self->getAvatar();
	if (!avatarp)
	{
		return;
	}

	bool paused = avatarp->areAnimationsPaused();

	// stop all other possible base motions
	avatarp->stopMotion(ANIM_AGENT_STAND, true);
	avatarp->stopMotion(ANIM_AGENT_WALK, true);
	avatarp->stopMotion(ANIM_AGENT_SIT, true);
	avatarp->stopMotion(ANIM_AGENT_HOVER, true);

	self->resetMotion();

	if (!paused)
	{
		self->mPauseRequest = NULL;
	}
}

//static
void LLFloaterAnimPreview::onCommitLoop(LLUICtrl*, void* userdata)
{
	LLFloaterAnimPreview* self = (LLFloaterAnimPreview*)userdata;
	if (!self || !self->getEnabled())
	{
		return;
	}

	LLKeyframeMotion* motionp = self->getMotion();
	if (motionp)
	{
		motionp->setLoop(self->childGetValue("loop_check").asBoolean());
		motionp->setLoopIn((F32)self->childGetValue("loop_in_point").asReal() *
						   0.01f * motionp->getDuration());
		motionp->setLoopOut((F32)self->childGetValue("loop_out_point").asReal() *
							0.01f * motionp->getDuration());
	}
}

//static
void LLFloaterAnimPreview::onCommitLoopIn(LLUICtrl* ctrlp, void* userdata)
{
	LLFloaterAnimPreview* self = (LLFloaterAnimPreview*)userdata;
	if (!self || !ctrlp || !self->getEnabled())
	{
		return;
	}

	LLKeyframeMotion* motionp = self->getMotion();
	if (motionp)
	{
		motionp->setLoopIn((F32)self->childGetValue("loop_in_point").asReal() *
						   0.01f);
		self->resetMotion();
		self->childSetValue("loop_check", LLSD(true));
		onCommitLoop(ctrlp, userdata);
	}
}

//static
void LLFloaterAnimPreview::onCommitLoopOut(LLUICtrl* ctrlp, void* userdata)
{
	LLFloaterAnimPreview* self = (LLFloaterAnimPreview*)userdata;
	if (!self || !ctrlp || !self->getEnabled())
	{
		return;
	}

	LLKeyframeMotion* motionp = self->getMotion();
	if (motionp)
	{
		motionp->setLoopOut((F32)self->childGetValue("loop_out_point").asReal() *
							0.01f * motionp->getDuration());
		self->resetMotion();
		self->childSetValue("loop_check", LLSD(true));
		onCommitLoop(ctrlp, userdata);
	}
}

//static
void LLFloaterAnimPreview::onCommitName(LLUICtrl*, void* userdata)
{
	LLFloaterAnimPreview* self = (LLFloaterAnimPreview*)userdata;
	if (!self || !self->getEnabled())
	{
		return;
	}

	LLKeyframeMotion* motionp = self->getMotion();
	if (motionp)
	{
		motionp->setName(self->childGetValue("name_form").asString());
	}
}

//static
void LLFloaterAnimPreview::onCommitHandPose(LLUICtrl*, void* userdata)
{
	LLFloaterAnimPreview* self = (LLFloaterAnimPreview*)userdata;
	if (self && self->getEnabled())
	{
		self->resetMotion(); // Sets hand pose
	}
}

//static
void LLFloaterAnimPreview::onCommitEmote(LLUICtrl*, void* userdata)
{
	LLFloaterAnimPreview* self = (LLFloaterAnimPreview*)userdata;
	if (self && self->getEnabled())
	{
		self->resetMotion(); // Sets emote
	}
}

//static
void LLFloaterAnimPreview::onCommitPriority(LLUICtrl*, void* userdata)
{
	LLFloaterAnimPreview* self = (LLFloaterAnimPreview*)userdata;
	if (!self || !self->getEnabled())
	{
		return;
	}

	LLKeyframeMotion* motionp = self->getMotion();
	if (motionp)
	{
		motionp->setPriority(llfloor((F32)self->childGetValue("priority").asReal()));
	}
}

//static
void LLFloaterAnimPreview::onCommitEaseIn(LLUICtrl*, void* userdata)
{
	LLFloaterAnimPreview* self = (LLFloaterAnimPreview*)userdata;
	if (!self || !self->getEnabled())
	{
		return;
	}

	LLKeyframeMotion* motionp = self->getMotion();
	if (motionp)
	{
		motionp->setEaseIn((F32)self->childGetValue("ease_in_time").asReal());
	}

	self->resetMotion();
}

//static
void LLFloaterAnimPreview::onCommitEaseOut(LLUICtrl*, void* userdata)
{
	LLFloaterAnimPreview* self = (LLFloaterAnimPreview*)userdata;
	if (!self || !self->getEnabled())
	{
		return;
	}

	LLKeyframeMotion* motionp = self->getMotion();
	if (motionp)
	{
		motionp->setEaseOut((F32)self->childGetValue("ease_out_time").asReal());
	}

	self->resetMotion();
}

//static
bool LLFloaterAnimPreview::validateEaseIn(LLUICtrl*, void* userdata)
{
	LLFloaterAnimPreview* self = (LLFloaterAnimPreview*)userdata;
	if (!self || !self->getEnabled())
	{
		return false;
	}

	LLKeyframeMotion* motionp = self->getMotion();
	if (!motionp)
	{
		return false;
	}

	if (!motionp->getLoop())
	{
		F32 new_ease_in = llclamp((F32)self->childGetValue("ease_in_time").asReal(),
								  0.f,
								  motionp->getDuration() -
								  motionp->getEaseOutDuration());
		self->childSetValue("ease_in_time", LLSD(new_ease_in));
	}

	return true;
}

//static
bool LLFloaterAnimPreview::validateEaseOut(LLUICtrl*, void* userdata)
{
	LLFloaterAnimPreview* self = (LLFloaterAnimPreview*)userdata;
	if (!self || !self->getEnabled())
	{
		return false;
	}

	LLKeyframeMotion* motionp = self->getMotion();
	if (!motionp)
	{
		return false;
	}

	if (!motionp->getLoop())
	{
		F32 new_ease_out = llclamp((F32)self->childGetValue("ease_out_time").asReal(),
								   0.f,
								   motionp->getDuration() -
								   motionp->getEaseInDuration());
		self->childSetValue("ease_out_time", LLSD(new_ease_out));
	}

	return true;
}

//static
bool LLFloaterAnimPreview::validateLoopIn(LLUICtrl*, void* userdata)
{
	LLFloaterAnimPreview* self = (LLFloaterAnimPreview*)userdata;
	if (!self || !self->getEnabled())
	{
		return false;
	}

	F32 loop_in_val = (F32)self->childGetValue("loop_in_point").asReal();
	F32 loop_out_val = (F32)self->childGetValue("loop_out_point").asReal();

	if (loop_in_val < 0.f)
	{
		loop_in_val = 0.f;
	}
	else if (loop_in_val > 100.f)
	{
		loop_in_val = 100.f;
	}
	else if (loop_in_val > loop_out_val)
	{
		loop_in_val = loop_out_val;
	}

	self->childSetValue("loop_in_point", LLSD(loop_in_val));
	return true;
}

//static
bool LLFloaterAnimPreview::validateLoopOut(LLUICtrl*, void* userdata)
{
	LLFloaterAnimPreview* self = (LLFloaterAnimPreview*)userdata;
	if (!self || !self->getEnabled())
	{
		return false;
	}

	F32 loop_out_val =(F32)self->childGetValue("loop_out_point").asReal();
	F32 loop_in_val = (F32)self->childGetValue("loop_in_point").asReal();

	if (loop_out_val < 0.f)
	{
		loop_out_val = 0.f;
	}
	else if (loop_out_val > 100.f)
	{
		loop_out_val = 100.f;
	}
	else if (loop_out_val < loop_in_val)
	{
		loop_out_val = loop_in_val;
	}

	self->childSetValue("loop_out_point", LLSD(loop_out_val));
	return true;
}

//-----------------------------------------------------------------------------
// LLPreviewAnimation
//-----------------------------------------------------------------------------
LLPreviewAnimation::LLPreviewAnimation(S32 width, S32 height)
:	LLViewerDynamicTexture(width, height, 3, ORDER_MIDDLE, false)
{
	mCameraDistance = PREVIEW_CAMERA_DISTANCE;
	mCameraYaw = 0.f;
	mCameraPitch = 0.f;
	mCameraZoom = 1.f;

	mDummyAvatar =
		(LLVOAvatar*)gObjectList.createObjectViewer(LL_PCODE_LEGACY_AVATAR,
													gAgent.getRegion(),
													LLViewerObject::CO_FLAG_UI_AVATAR);
	if (!mDummyAvatar)
	{
		llwarns << "Cannot create a dummy avatar !" << llendl;
		return;
	}
	mDummyAvatar->createDrawable();
	mDummyAvatar->mSpecialRenderMode = 1;
	mDummyAvatar->startMotion(ANIM_AGENT_STAND, BASE_ANIM_TIME_OFFSET);
	mDummyAvatar->hideHair();
	mDummyAvatar->hideSkirt();

	// Give a default texture to the avatar body parts. HB
	U32 texname = LLViewerFetchedTexture::sDefaultImagep->getTexName();
	LLAvatarJoint* rootp = mDummyAvatar->mRoot;
	LLViewerJointMesh* meshp =
		dynamic_cast<LLViewerJointMesh*>(rootp->findJoint("mHairMesh0"));
	meshp->setTestTexture(texname);
	meshp = dynamic_cast<LLViewerJointMesh*>(rootp->findJoint("mHeadMesh0"));
	meshp->setTestTexture(texname);
	meshp =
		dynamic_cast<LLViewerJointMesh*>(rootp->findJoint("mUpperBodyMesh0"));
	meshp->setTestTexture(texname);
	meshp =
		dynamic_cast<LLViewerJointMesh*>(rootp->findJoint("mLowerBodyMesh0"));
	meshp->setTestTexture(texname);

	// Stop extraneous animations
	mDummyAvatar->stopMotion(ANIM_AGENT_HEAD_ROT, true);
	mDummyAvatar->stopMotion(ANIM_AGENT_EYE, true);
	mDummyAvatar->stopMotion(ANIM_AGENT_BODY_NOISE, true);
	mDummyAvatar->stopMotion(ANIM_AGENT_BREATHE_ROT, true);
	mDummyAvatar->stopMotion(ANIM_AGENT_PUPPET_MOTION, true);
	mDummyAvatar->stopMotion(ANIM_AGENT_PHYSICS_MOTION, true);
}

LLPreviewAnimation::~LLPreviewAnimation()
{
	if (mDummyAvatar)
	{
		mDummyAvatar->markDead();
	}
}

//virtual
S8 LLPreviewAnimation::getType() const
{
	return LLViewerDynamicTexture::LL_PREVIEW_ANIMATION;
}

bool LLPreviewAnimation::render()
{
	LLVOAvatar* avatarp = mDummyAvatar.get();
	if (!avatarp || avatarp->mDrawable.isNull())
	{
		return true;
	}

	gGL.pushUIMatrix();
	gGL.loadUIIdentity();

	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.pushMatrix();
	gGL.loadIdentity();
	gGL.ortho(0.f, mFullWidth, 0.f, mFullHeight, -1.f, 1.f);

	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.pushMatrix();
	gGL.loadIdentity();

	LLGLSUIDefault def;

	gUIProgram.bind();

	gGL.color4f(0.15f, 0.2f, 0.3f, 1.f);
	gl_rect_2d_simple(mFullWidth, mFullHeight);

	gGL.color4f(1.f, 1.f, 1.f, 1.f);

	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.popMatrix();

	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.popMatrix();

	gGL.flush();

	LLVector3 target_pos = avatarp->mRoot->getWorldPosition();

	LLQuaternion camera_rot = LLQuaternion(mCameraPitch, LLVector3::y_axis) *
							  LLQuaternion(mCameraYaw, LLVector3::z_axis);

	LLQuaternion av_rot = avatarp->mRoot->getWorldRotation() * camera_rot;
	gViewerCamera.setOriginAndLookAt(target_pos +
									 ((LLVector3(mCameraDistance, 0.f, 0.f) +
									   mCameraOffset) * av_rot), // Camera
									 LLVector3::z_axis,			 // Up
									 // point of interest
									 target_pos + mCameraOffset * av_rot);

	gViewerCamera.setAspect((F32)mFullWidth / (F32)mFullHeight);
	gViewerCamera.setViewNoBroadcast(gViewerCamera.getDefaultFOV() /
									 mCameraZoom);
	gViewerCamera.setPerspective(false, mOrigin.mX, mOrigin.mY, mFullWidth,
								 mFullHeight, false);

	avatarp->updateLOD();
	avatarp->dirtyMesh();

	LLVertexBuffer::unbind();

	// Do not let environment settings influence our scene lighting.
	LLPreviewLighting preview_light;

	// *FIXME: find out why only previewAvatar() seems to (more or less) work in
	// PBR mode, while LL's PBR viewer can do renderAvatars() here. HB
	if (gUsePBRShaders)
	{
		gPipeline.previewAvatar(avatarp);
		gGL.popUIMatrix();
		return true;
	}

	LLGLDepthTest gls_depth(GL_TRUE);
	// Make sure alpha=0 shows avatar material color
	LLGLDisable no_blend(GL_BLEND);

	LLFace* facep = avatarp->mDrawable->getFace(0);
	if (facep)	// Paranoia
	{
		LLDrawPoolAvatar* poolp = (LLDrawPoolAvatar*)facep->getPool();
		if (poolp)	// More paranoia !
		{
			// Render only our dummy avatar
			poolp->renderAvatars(avatarp);
		}
	}

	gGL.popUIMatrix();

	return true;
}

void LLPreviewAnimation::rotate(F32 yaw_radians, F32 pitch_radians)
{
	mCameraYaw = mCameraYaw + yaw_radians;

	mCameraPitch = llclamp(mCameraPitch + pitch_radians, F_PI_BY_TWO * -0.8f,
						   F_PI_BY_TWO * 0.8f);
}

void LLPreviewAnimation::zoom(F32 zoom_delta)
{
	setZoom(mCameraZoom + zoom_delta);
}

void LLPreviewAnimation::setZoom(F32 zoom_amt)
{
	mCameraZoom	= llclamp(zoom_amt, MIN_CAMERA_ZOOM, MAX_CAMERA_ZOOM);
}

void LLPreviewAnimation::pan(F32 right, F32 up)
{
	mCameraOffset.mV[VY] = llclamp(mCameraOffset.mV[VY] + right * mCameraDistance / mCameraZoom,
								   -1.f, 1.f);
	mCameraOffset.mV[VZ] = llclamp(mCameraOffset.mV[VZ] + up * mCameraDistance / mCameraZoom,
								   -1.f, 1.f);
}
