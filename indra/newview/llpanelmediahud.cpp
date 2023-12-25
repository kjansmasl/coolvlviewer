/**
 * @file llpanelmediahud.cpp
 * @brief Media HUD panel
 *
 * $LicenseInfo:firstyear=2003&license=viewergpl$
 *
 * Copyright (c) 2003-2009, Linden Research, Inc.
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

#include "llpanelmediahud.h"

#include "llbutton.h"
#include "lliconctrl.h"
#include "llmediaentry.h"
#include "llpanel.h"
#include "llparcel.h"
#include "llpluginclassmedia.h"
#include "llrender.h"
#include "llslider.h"
#include "lltextureentry.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "lldrawable.h"
#include "llface.h"
#include "llfloatertools.h"
#include "llhudview.h"
#include "llpipeline.h"
#include "llselectmgr.h"
#include "lltoolpie.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerdisplay.h"		// For get_hud_matrices()
#include "llviewermediafocus.h"
#include "llviewerobjectlist.h"
#include "llviewerparcelmgr.h"
#include "llviewerwindow.h"
#include "llvovolume.h"
#include "llweb.h"

constexpr F32 ZOOM_MEDIUM_PADDING = 1.2f;

//static
LLUUID LLPanelMediaHUD::sLastTargetImplID;
LLPanelMediaHUD::EZoomLevel LLPanelMediaHUD::sLastMediaZoom = LLPanelMediaHUD::ZOOM_NONE;

//
// LLPanelMediaHUD
//

LLPanelMediaHUD::LLPanelMediaHUD(viewer_media_t media_impl)
:	mTargetObjectFace(0),
	mTargetIsHUDObject(false),
	mControlFadeTime(3.f),
	mLastVolume(0.f),
	mMediaFocus(false),
	mLargeControls(false),
	mHasTimeControl(false),
	mCurrentZoom(ZOOM_NONE),
	mScrollState(SCROLL_NONE)
{
	LLUICtrlFactory::getInstance()->buildPanel(this, "panel_media_hud.xml");
	mMouseMoveTimer.reset();
	LL_DEBUGS("MediaHUD") << "Stopping the fading timer." << LL_ENDL;
	mFadeTimer.stop();
	mPanelHandle.bind(this);
}

//virtual
bool LLPanelMediaHUD::postBuild()
{
	mMediaFullView = getChild<LLView>("media_full_view");

	mFocusedControls = getChild<LLPanel>("media_focused_controls");
	mHoverControls = getChild<LLPanel>("media_hover_controls");

	mCloseButton = getChild<LLButton>("close");
	mCloseButton->setClickedCallback(onClickClose, this);

	mBackButton = getChild<LLButton>("back");
	mBackButton->setClickedCallback(onClickBack, this);

	mForwardButton = getChild<LLButton>("fwd");
	mForwardButton->setClickedCallback(onClickForward, this);

	mHomeButton = getChild<LLButton>("home");
	mHomeButton->setClickedCallback(onClickHome, this);

	mStopButton = getChild<LLButton>("stop");
	mStopButton->setClickedCallback(onClickStop, this);

	mMediaStopButton = getChild<LLButton>("media_stop");
	mMediaStopButton->setClickedCallback(onClickMediaStop, this);

	mReloadButton = getChild<LLButton>("reload");
	mReloadButton->setClickedCallback(onClickReload, this);

	mPlayButton = getChild<LLButton>("play");
	mPlayButton->setClickedCallback(onClickPlay, this);

	mPauseButton = getChild<LLButton>("pause");
	mPauseButton->setClickedCallback(onClickPause, this);

	mOpenButton = getChild<LLButton>("new_window");
	mOpenButton->setClickedCallback(onClickOpen, this);
	mOpenButtonTooltip = mOpenButton->getToolTip();

	mMediaVolumeButton = getChild<LLButton>("volume");
	mMediaVolumeButton->setClickedCallback(onClickVolume, this);
	mMediaVolumeButton->setMouseHoverCallback(onHoverVolume);

	mMediaMutedButton = getChild<LLButton>("muted");
	mMediaMutedButton->setClickedCallback(onClickVolume, this);
	mMediaMutedButton->setMouseHoverCallback(onHoverVolume);

	mVolumeSlider = getChild<LLSlider>("volume_slider");
	mVolumeSlider->setCommitCallback(onVolumeChange);
	mVolumeSlider->setCallbackUserData(this);
	mVolumeSlider->setMouseHoverCallback(onHoverSlider);

	mZoomButton = getChild<LLButton>("zoom_frame");
	mZoomButton->setClickedCallback(onClickZoom, this);

	mUnzoomButton = getChild<LLButton>("unzoom_frame");
	mUnzoomButton->setClickedCallback(onClickZoom, this);

	mOpenButton2 = getChild<LLButton>("new_window_hover");
	mOpenButton2->setClickedCallback(onClickOpen, this);

	mZoomButton2 = getChild<LLButton>("zoom_frame_hover");
	mZoomButton2->setClickedCallback(onClickZoom, this);

	LLButton* scroll_up_btn = getChild<LLButton>("scrollup");
	scroll_up_btn->setClickedCallback(onScrollUp, this);
	scroll_up_btn->setHeldDownCallback(onScrollUpHeld);
	scroll_up_btn->setMouseUpCallback(onScrollStop);
	LLButton* scroll_left_btn = getChild<LLButton>("scrollleft");
	scroll_left_btn->setClickedCallback(onScrollLeft, this);
	scroll_left_btn->setHeldDownCallback(onScrollLeftHeld);
	scroll_left_btn->setMouseUpCallback(onScrollStop);
	LLButton* scroll_right_btn = getChild<LLButton>("scrollright");
	scroll_right_btn->setClickedCallback(onScrollRight, this);
	scroll_right_btn->setHeldDownCallback(onScrollLeftHeld);
	scroll_right_btn->setMouseUpCallback(onScrollStop);
	LLButton* scroll_down_btn = getChild<LLButton>("scrolldown");
	scroll_down_btn->setClickedCallback(onScrollDown, this);
	scroll_down_btn->setHeldDownCallback(onScrollDownHeld);
	scroll_down_btn->setMouseUpCallback(onScrollStop);

	// Clicks on HUD buttons do not remove keyboard focus from media
	setIsChrome(true);

	return true;
}

void LLPanelMediaHUD::setMediaFace(LLPointer<LLViewerObject> objectp,
								   S32 face, viewer_media_t media_impl,
								   LLVector3 pick_normal)
{
	if (media_impl.notNull() && objectp.notNull())
	{
		mTargetImplID = media_impl->getMediaTextureID();
		mTargetObjectID = objectp->getID();
		mTargetObjectFace = face;
		mTargetObjectNormal = pick_normal;

		if (sLastTargetImplID != mTargetImplID)
		{
			sLastTargetImplID = mTargetImplID;
			sLastMediaZoom = mCurrentZoom;
			mVolumeSlider->setValue(media_impl->getVolume());
		}
		else
		{
			mCurrentZoom = sLastMediaZoom;
		}

		updateShape();
		if (mTargetIsHUDObject)
		{
			// Set the zoom to none
			sLastMediaZoom = mCurrentZoom = ZOOM_NONE;
		}
	}
	else
	{
		mTargetImplID.setNull();
		mTargetObjectID.setNull();
		mTargetObjectFace = 0;
	}
}

LLViewerMediaImpl* LLPanelMediaHUD::getTargetMediaImpl()
{
	return LLViewerMedia::getMediaImplFromTextureID(mTargetImplID);
}

LLViewerObject* LLPanelMediaHUD::getTargetObject()
{
	return gObjectList.findObject(mTargetObjectID);
}

LLPluginClassMedia* LLPanelMediaHUD::getTargetMediaPlugin()
{
	LLViewerMediaImpl* impl = getTargetMediaImpl();
	return impl && impl->hasMedia() ? impl->getMediaPlugin() : NULL;
}

void LLPanelMediaHUD::updateShape()
{
	constexpr S32 MIN_HUD_WIDTH = 235;
	constexpr S32 MIN_HUD_HEIGHT = 120;

	LLParcel* parcel = gViewerParcelMgr.getAgentParcel();
	LLViewerMediaImpl* media_impl = getTargetMediaImpl();
	LLViewerObject* objectp = getTargetObject();
	if (!parcel || !media_impl || !objectp || LLFloaterTools::isVisible())
	{
		setVisible(false);
		return;
	}

	mTargetIsHUDObject = objectp->isHUDAttachment();
	if (mTargetIsHUDObject)
	{
		// Make sure the "used on HUD" flag is set for this impl
		media_impl->setUsedOnHUD(true);
	}

	bool can_navigate = parcel->getMediaAllowNavigate();

	LLPluginClassMedia* media_plugin = NULL;
	if (media_impl->hasMedia())
	{
		media_plugin = getTargetMediaPlugin();
	}

	LLVOVolume* vobj = objectp->asVolume();

	mLargeControls = false;
	// Do not show the media HUD if we do not have permissions
	LLTextureEntry* tep = objectp->getTE(mTargetObjectFace);
	LLMediaEntry* media_data = tep ? tep->getMediaData() : NULL;
	if (media_data)
	{
		mLargeControls = media_data->getControls() == LLMediaEntry::STANDARD;

		if (vobj && !vobj->hasMediaPermission(media_data,
											  LLVOVolume::MEDIA_PERM_CONTROL))
		{
			setVisible(false);
			return;
		}
	}
	mLargeControls = mLargeControls || mMediaFocus;

	// Set the state of the buttons
	mBackButton->setVisible(true);
	mForwardButton->setVisible(true);
	mReloadButton->setVisible(true);
	mStopButton->setVisible(false);
	mHomeButton->setVisible(true);
	mCloseButton->setVisible(true);
	mZoomButton->setVisible(!isZoomed());
	mUnzoomButton->setVisible(isZoomed());
	mZoomButton->setEnabled(!mTargetIsHUDObject);
	mUnzoomButton->setEnabled(!mTargetIsHUDObject);
	mZoomButton2->setEnabled(!mTargetIsHUDObject);

	std::string tooltip = media_impl->getMediaURL();
	if (tooltip.empty())
	{
		tooltip = mOpenButtonTooltip + ".";
	}
	else
	{
		tooltip = mOpenButtonTooltip + ": " + tooltip;
	}
	mOpenButton->setToolTip(tooltip);
	mOpenButton2->setToolTip(tooltip);

	if (mLargeControls)
	{
		mBackButton->setEnabled(media_impl->canNavigateBack() && can_navigate);
		mForwardButton->setEnabled(media_impl->canNavigateForward() && can_navigate);
		mStopButton->setEnabled(can_navigate);
		mHomeButton->setEnabled(can_navigate);

		F32 media_volume = media_impl->getVolume();
		bool muted = media_volume <= 0.f;
		mMediaVolumeButton->setVisible(!muted);
		mMediaMutedButton->setVisible(muted);
		mVolumeSlider->setValue((F32)media_volume);

		LLPluginClassMediaOwner::EMediaStatus result =
			media_plugin ? media_plugin->getStatus()
						 : LLPluginClassMediaOwner::MEDIA_NONE;
		mHasTimeControl = media_plugin &&
						  media_plugin->pluginSupportsMediaTime();
		if (mHasTimeControl)
		{
			mReloadButton->setEnabled(false);
			mReloadButton->setVisible(false);
			mMediaStopButton->setVisible(true);
			mHomeButton->setVisible(false);
			mBackButton->setEnabled(true);
			mForwardButton->setEnabled(true);
			if (result == LLPluginClassMediaOwner::MEDIA_PLAYING)
			{
				mPlayButton->setEnabled(false);
				mPlayButton->setVisible(false);
				mPauseButton->setEnabled(true);
				mPauseButton->setVisible(true);
				mMediaStopButton->setEnabled(true);
			}
			else
			{
				mPlayButton->setEnabled(true);
				mPlayButton->setVisible(true);
				mPauseButton->setEnabled(false);
				mPauseButton->setVisible(false);
				mMediaStopButton->setEnabled(false);
			}
		}
		else
		{
			mPlayButton->setVisible(false);
			mPauseButton->setVisible(false);
			mMediaStopButton->setVisible(false);
			if (result == LLPluginClassMediaOwner::MEDIA_LOADING)
			{
				mReloadButton->setEnabled(false);
				mReloadButton->setVisible(false);
				mStopButton->setEnabled(true);
				mStopButton->setVisible(true);
			}
			else
			{
				mReloadButton->setEnabled(true);
				mReloadButton->setVisible(true);
				mStopButton->setEnabled(false);
				mStopButton->setVisible(false);
			}
		}
	}

	mVolumeSlider->setVisible(mLargeControls &&
							  !mVolumeSliderTimer.hasExpired());

	mFocusedControls->setVisible(mLargeControls);
	mHoverControls->setVisible(!mLargeControls);

	// Handle Scrolling
	switch (mScrollState)
	{
		case SCROLL_UP:
			media_impl->scrollWheel(0, 0, 0, -1, MASK_NONE);
			break;
		case SCROLL_DOWN:
			media_impl->scrollWheel(0, 0, 0, 1, MASK_NONE);
			break;
		case SCROLL_LEFT:
			media_impl->scrollWheel(0, 0, 1, 0, MASK_NONE);
			//media_impl->handleKeyHere(KEY_LEFT, MASK_NONE);
			break;
		case SCROLL_RIGHT:
			media_impl->scrollWheel(0, 0, -1, 0, MASK_NONE);
			//media_impl->handleKeyHere(KEY_RIGHT, MASK_NONE);
			break;
		case SCROLL_NONE:
		default:
			break;
	}

	LLBBox screen_bbox;
	LLMatrix4a mat;
	if (mTargetIsHUDObject)
	{
		LLMatrix4a proj, modelview;
		if (get_hud_matrices(proj, modelview))
		{
			mat.setMul(proj, modelview);
		}
		else
		{
			llwarns_sparse << "Cannot get HUD matrices" << llendl;
			setVisible(false);
			return;
		}
	}
	else
	{
		mat.setMul(gGLProjection, gGLModelView);
	}

	std::vector<LLVector3> vect_face;
	LLVolume* volume = objectp->getVolume();
	if (volume)
	{
		const LLVolumeFace& vf = volume->getVolumeFace(mTargetObjectFace);

		LLVector3 ext[2];
		ext[0].set(vf.mExtents[0].getF32ptr());
		ext[1].set(vf.mExtents[1].getF32ptr());

		LLVector3 center = (ext[0] + ext[1]) * 0.5f;
		LLVector3 size = (ext[1] - ext[0]) * 0.5f;
		LLVector3 vert[] = {
			center + size.scaledVec(LLVector3(1.f, 1.f, 1.f)),
			center + size.scaledVec(LLVector3(-1.f, 1.f, 1.f)),
			center + size.scaledVec(LLVector3(1.f, -1.f, 1.f)),
			center + size.scaledVec(LLVector3(-1.f, -1.f, 1.f)),
			center + size.scaledVec(LLVector3(1.f, 1.f, -1.f)),
			center + size.scaledVec(LLVector3(-1.f, 1.f, -1.f)),
			center + size.scaledVec(LLVector3(1.f, -1.f, -1.f)),
			center + size.scaledVec(LLVector3(-1.f, -1.f, -1.f)),
		};

		for (U32 i = 0; i < 8; ++i)
		{
			vect_face.emplace_back(vobj->volumePositionToAgent(vert[i]));
		}
	}

	LLVector4a min, max, screen_vert;;
	min.splat(1.f);
	max.splat(-1.f);
	for (std::vector<LLVector3>::iterator vert_it = vect_face.begin(),
										  vert_end = vect_face.end();
		 vert_it != vert_end; ++vert_it)
	{
		// Project silhouette vertices into screen space
		screen_vert.load3(vert_it->mV, 1.f);
		mat.perspectiveTransform(screen_vert, screen_vert);

		// Add to screenspace bounding box
		min.setMin(screen_vert, min);
		max.setMax(screen_vert, max);
	}

	LLCoordGL screen_min;
	screen_min.mX = ll_round((F32)gViewerWindowp->getWindowWidth() *
							 (min.getF32ptr()[VX] + 1.f) * 0.5f);
	screen_min.mY = ll_round((F32)gViewerWindowp->getWindowHeight() *
							 (min.getF32ptr()[VY] + 1.f) * 0.5f);

	LLCoordGL screen_max;
	screen_max.mX = ll_round((F32)gViewerWindowp->getWindowWidth() *
							 (max.getF32ptr()[VX] + 1.f) * 0.5f);
	screen_max.mY = ll_round((F32)gViewerWindowp->getWindowHeight() *
							 (max.getF32ptr()[VY] + 1.f) * 0.5f);

	// Grow panel so that screenspace bounding box fits inside the
	// "media_full_view" element of the HUD
	LLRect media_hud_rect;
	getParent()->screenRectToLocal(LLRect(screen_min.mX, screen_max.mY,
										  screen_max.mX, screen_min.mY),
								   &media_hud_rect);
	media_hud_rect.mLeft -= mMediaFullView->getRect().mLeft;
	media_hud_rect.mBottom -= mMediaFullView->getRect().mBottom;
	media_hud_rect.mTop += getRect().getHeight() -
						   mMediaFullView->getRect().mTop;
	media_hud_rect.mRight += getRect().getWidth() -
							 mMediaFullView->getRect().mRight;

	// Keep all parts of HUD on-screen
	media_hud_rect.intersectWith(getParent()->getLocalRect());

	// If we had to clip the rect, don't display the border
	//childSetVisible("bg_image", false);

	// Clamp to minimum size, keeping centered
	media_hud_rect.setCenterAndSize(media_hud_rect.getCenterX(),
									media_hud_rect.getCenterY(),
									llmax(MIN_HUD_WIDTH,
										  media_hud_rect.getWidth()),
									llmax(MIN_HUD_HEIGHT,
										  media_hud_rect.getHeight()));

	userSetShape(media_hud_rect);

	setVisible(true);

	if (!mMediaFocus)
	{
		// Test mouse position to see if the cursor is stationary
		LLCoordWindow cursor_pos_window;
		gWindowp->getCursorPosition(&cursor_pos_window);

		// If last pos is not equal to current pos, the mouse has moved
		// We need to reset the timer, and make sure the panel is visible
		if (cursor_pos_window.mX != mLastCursorPos.mX ||
			cursor_pos_window.mY != mLastCursorPos.mY ||
			mScrollState != SCROLL_NONE)
		{
			mMouseMoveTimer.start();
			mLastCursorPos = cursor_pos_window;
		}

		// Mouse has been stationary, but not for long enough to fade the UI
		static LLCachedControl<F32> control_timeout(gSavedSettings,
													"MediaControlTimeout");
		static LLCachedControl<F32> fade_time(gSavedSettings,
											  "MediaControlFadeTime");
		mControlFadeTime = llmax(0.5f, (F32)fade_time);
		if (mMouseMoveTimer.getElapsedTimeF32() < control_timeout ||
			(mLargeControls && !mVolumeSliderTimer.hasExpired()))
		{
			// If we have started fading, stop and reset the alpha values
			if (mFadeTimer.getStarted())
			{
				LL_DEBUGS("MediaHUD") << "Stopping the fading timer (mouse moved, media scrolled or volume slider shown)."
									  << LL_ENDL;
				mFadeTimer.stop();
				setAlpha(1.f);
			}
		}
		// If we need to start fading the UI (and we have not already started)
		else if (!mFadeTimer.getStarted())
		{
			LL_DEBUGS("MediaHUD") << "Starting the fading timer." << LL_ENDL;
			mFadeTimer.reset();
			mFadeTimer.start();
		}
		else if (mFadeTimer.getElapsedTimeF32() >= mControlFadeTime)
		{
			setVisible(false);
		}
	}
	else if (mFadeTimer.getStarted())
	{
		LL_DEBUGS("MediaHUD") << "Focused: stopping the fading timer."
							  << LL_ENDL;
		mFadeTimer.stop();
		setAlpha(1.f);
	}
}

//virtual
void LLPanelMediaHUD::draw()
{
	if (mFadeTimer.getStarted())
	{
		if (mFadeTimer.getElapsedTimeF32() >= mControlFadeTime)
		{
			setVisible(false);
		}
		else
		{
			F32 time = mFadeTimer.getElapsedTimeF32();
			F32 alpha = llmax(lerp(1.f, 0.f, time / mControlFadeTime), 0.f);
			setAlpha(alpha);
			setVisible(true);
		}
	}

	LLPanel::draw();
}

void LLPanelMediaHUD::setAlpha(F32 alpha)
{
	LLViewQuery query;

	LLView* query_view = mLargeControls ? mFocusedControls : mHoverControls;
	child_list_t children = query(query_view);
	for (child_list_iter_t it = children.begin(), end = children.end();
		 it != end; ++it)
	{
		LLView* viewp = *it;
		if (viewp && viewp->isUICtrl())
		{
			((LLUICtrl*)viewp)->setAlpha(alpha);
		}
	}

	LLPanel::setAlpha(alpha);
}

//virtual
bool LLPanelMediaHUD::handleScrollWheel(S32 x, S32 y, S32 clicks)
{
	return LLViewerMediaFocus::getInstance()->handleScrollWheel(x, y, clicks);
}

bool LLPanelMediaHUD::isMouseOver()
{
	if (!getVisible())
	{
		return false;
	}

	LLCoordWindow cursor_pos_window;
	gWindowp->getCursorPosition(&cursor_pos_window);

	LLRect screen_rect;
	localRectToScreen(getLocalRect(), &screen_rect);

	return screen_rect.pointInRect(cursor_pos_window.mX, cursor_pos_window.mY);
}

//static
void LLPanelMediaHUD::onClickClose(void* user_data)
{
	LLViewerMediaFocus::getInstance()->setFocusFace(false, NULL, 0, NULL);
	LLPanelMediaHUD* self = (LLPanelMediaHUD*)user_data;
	if (self)
	{
		if (self->mCurrentZoom != ZOOM_NONE)
		{
#if 0
			gAgent.setFocusOnAvatar();
#endif
			sLastMediaZoom = self->mCurrentZoom = ZOOM_NONE;
		}
		self->setVisible(false);
	}
}

//static
void LLPanelMediaHUD::onClickBack(void* user_data)
{
	LLPanelMediaHUD* self = (LLPanelMediaHUD*)user_data;
	if (self)
	{
		LLViewerMediaImpl* impl = self->getTargetMediaImpl();
		if (impl)
		{
			if (self->mHasTimeControl)
			{
				impl->skipBack(0.2f);
			}
			else
			{
				impl->navigateForward();
			}
		}
	}
}

//static
void LLPanelMediaHUD::onClickForward(void* user_data)
{
	LLPanelMediaHUD* self = (LLPanelMediaHUD*)user_data;
	if (self)
	{
		LLViewerMediaImpl* impl = self->getTargetMediaImpl();
		if (impl)
		{
			if (self->mHasTimeControl)
			{
				impl->skipForward(0.2f);
			}
			else
			{
				impl->navigateForward();
			}
		}
	}
}

//static
void LLPanelMediaHUD::onClickHome(void* user_data)
{
	LLPanelMediaHUD* self = (LLPanelMediaHUD*)user_data;
	if (self)
	{
		LLViewerMediaImpl* impl = self->getTargetMediaImpl();
		if (impl)
		{
			impl->navigateHome();
		}
	}
}

//static
void LLPanelMediaHUD::onClickOpen(void* user_data)
{
	LLPanelMediaHUD* self = (LLPanelMediaHUD*)user_data;
	if (self)
	{
		LLViewerMediaImpl* impl = self->getTargetMediaImpl();
		if (impl)
		{
			LLWeb::loadURL(impl->getCurrentMediaURL());
		}
	}
}

//static
void LLPanelMediaHUD::onClickReload(void* user_data)
{
	LLPanelMediaHUD* self = (LLPanelMediaHUD*)user_data;
	if (self)
	{
		LLViewerMediaImpl* impl = self->getTargetMediaImpl();
		if (impl)
		{
			impl->navigateReload();
		}
	}
}

//static
void LLPanelMediaHUD::onClickPlay(void* user_data)
{
	LLPanelMediaHUD* self = (LLPanelMediaHUD*)user_data;
	if (self)
	{
		LLViewerMediaImpl* impl = self->getTargetMediaImpl();
		if (impl)
		{
			impl->play();
		}
	}
}

//static
void LLPanelMediaHUD::onClickPause(void* user_data)
{
	LLPanelMediaHUD* self = (LLPanelMediaHUD*)user_data;
	if (self)
	{
		LLViewerMediaImpl* impl = self->getTargetMediaImpl();
		if (impl)
		{
			impl->pause();
		}
	}
}

//static
void LLPanelMediaHUD::onClickStop(void* user_data)
{
	LLPanelMediaHUD* self = (LLPanelMediaHUD*)user_data;
	if (self)
	{
		LLViewerMediaImpl* impl = self->getTargetMediaImpl();
		if (impl)
		{
			impl->navigateStop();
		}
	}
}

//static
void LLPanelMediaHUD::onClickMediaStop(void* user_data)
{
	LLPanelMediaHUD* self = (LLPanelMediaHUD*)user_data;
	if (self)
	{
		LLViewerMediaImpl* impl = self->getTargetMediaImpl();
		if (impl)
		{
			impl->stop();
		}
	}
}

//static
void LLPanelMediaHUD::onClickVolume(void* user_data)
{
	LLPanelMediaHUD* self = (LLPanelMediaHUD*)user_data;
	if (self)
	{
		LLViewerMediaImpl* impl = self->getTargetMediaImpl();
		if (impl)
		{
			F32 volume = impl->getVolume();
			if (volume > 0.f)
			{
				self->mLastVolume = volume;
				impl->setVolume(0.f);
			}
			else
			{
				volume = self->mLastVolume;
				if (volume <= 0.f)
				{
					volume = gSavedSettings.getF32("AudioLevelMedia");
				}
				impl->setVolume(volume);
			}
			self->mVolumeSlider->setValue(volume);
		}
	}
}

//static
void LLPanelMediaHUD::onClickZoom(void* user_data)
{
	LLPanelMediaHUD* self = (LLPanelMediaHUD*)user_data;
	if (self)
	{
		self->nextZoomLevel();
	}
}

void LLPanelMediaHUD::nextZoomLevel()
{
	if (mTargetIsHUDObject)
	{
		// Do not try to zoom on HUD objects...
		sLastMediaZoom = mCurrentZoom = ZOOM_NONE;
		return;
	}

	F32 zoom_padding = 0.f;
	S32 last_zoom_level = (S32)mCurrentZoom;
	sLastMediaZoom = mCurrentZoom = (EZoomLevel)((last_zoom_level + 1) %
												 (S32)ZOOM_END);

	switch (mCurrentZoom)
	{
		case ZOOM_NONE:
		{
			gAgent.setFocusOnAvatar();
			break;
		}
		case ZOOM_MEDIUM:
		{
			zoom_padding = ZOOM_MEDIUM_PADDING;
			break;
		}
		default:
		{
			gAgent.setFocusOnAvatar();
			break;
		}
	}

	if (zoom_padding > 0.f)
	{
		LLViewerMediaFocus::getInstance()->setCameraZoom(getTargetObject(),
														 mTargetObjectNormal,
														 zoom_padding, true);
	}
}

//static
void LLPanelMediaHUD::onScrollUp(void* user_data)
{
	LLPanelMediaHUD* self = (LLPanelMediaHUD*)user_data;
	if (self)
	{
		LLViewerMediaImpl* impl = self->getTargetMediaImpl();
		if (impl)
		{
			impl->scrollWheel(0, 0, 0, -1, MASK_NONE);
		}
	}
}

//static
void LLPanelMediaHUD::onScrollUpHeld(void* user_data)
{
	LLPanelMediaHUD* self = (LLPanelMediaHUD*)user_data;
	if (self)
	{
		self->mScrollState = SCROLL_UP;
	}
}

//static
void LLPanelMediaHUD::onScrollRight(void* user_data)
{
	LLPanelMediaHUD* self = (LLPanelMediaHUD*)user_data;
	if (self)
	{
		LLViewerMediaImpl* impl = self->getTargetMediaImpl();
		if (impl)
		{
			impl->scrollWheel(0, 0, -1, 0, MASK_NONE);
		}
	}
}

//static
void LLPanelMediaHUD::onScrollRightHeld(void* user_data)
{
	LLPanelMediaHUD* self = (LLPanelMediaHUD*)user_data;
	if (self)
	{
		self->mScrollState = SCROLL_RIGHT;
	}
}

//static
void LLPanelMediaHUD::onScrollLeft(void* user_data)
{
	LLPanelMediaHUD* self = (LLPanelMediaHUD*)user_data;
	if (self)
	{
		LLViewerMediaImpl* impl = self->getTargetMediaImpl();
		if (impl)
		{
			impl->scrollWheel(0, 0, 1, 0, MASK_NONE);
		}
	}
}

//static
void LLPanelMediaHUD::onScrollLeftHeld(void* user_data)
{
	LLPanelMediaHUD* self = (LLPanelMediaHUD*)user_data;
	if (self)
	{
		self->mScrollState = SCROLL_LEFT;
	}
}

//static
void LLPanelMediaHUD::onScrollDown(void* user_data)
{
	LLPanelMediaHUD* self = (LLPanelMediaHUD*)user_data;
	if (self)
	{
		LLViewerMediaImpl* impl = self->getTargetMediaImpl();
		if (impl)
		{
			impl->scrollWheel(0, 0, 0, 1, MASK_NONE);
		}
	}
}

//static
void LLPanelMediaHUD::onScrollDownHeld(void* user_data)
{
	LLPanelMediaHUD* self = (LLPanelMediaHUD*)user_data;
	if (self)
	{
		self->mScrollState = SCROLL_DOWN;
	}
}

//static
void LLPanelMediaHUD::onScrollStop(void* user_data)
{
	LLPanelMediaHUD* self = (LLPanelMediaHUD*)user_data;
	if (self)
	{
		self->mScrollState = SCROLL_NONE;
	}
}

//static
void LLPanelMediaHUD::onVolumeChange(LLUICtrl* ctrl, void* user_data)
{
	LLPanelMediaHUD* self = (LLPanelMediaHUD*)user_data;
	if (self && self->mVolumeSlider)
	{
		LLViewerMediaImpl* impl = self->getTargetMediaImpl();
		if (impl)
		{
			impl->setVolume(self->mVolumeSlider->getValueF32());
		}
	}
}

//static
void LLPanelMediaHUD::onHoverSlider(LLUICtrl* ctrl, void* user_data)
{
	onHoverVolume(user_data);
}

//static
void LLPanelMediaHUD::onHoverVolume(void* user_data)
{
	static LLCachedControl<F32> control_timeout(gSavedSettings,
												"MediaControlTimeout");
	LLPanelMediaHUD* self = (LLPanelMediaHUD*)user_data;
	if (self)
	{
		self->mVolumeSliderTimer.reset();
		self->mVolumeSliderTimer.setTimerExpirySec(control_timeout);
	}
}
