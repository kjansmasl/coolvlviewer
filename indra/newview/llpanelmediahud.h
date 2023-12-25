/**
 * @file llpanelmediahud.h
 * @brief Media hud panel
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

#ifndef LL_PANELMEDIAHUD_H
#define LL_PANELMEDIAHUD_H

#include "llpanel.h"

#include "llviewermedia.h"

class LLButton;
class LLCoordWindow;
class LLFrameTimer;
class LLSlider;
class LLView;
class LLViewerMediaImpl;
class LLViewerObject;

class LLPanelMediaHUD final : public LLPanel
{
protected:
	LOG_CLASS(LLPanelMediaHUD);

public:
	LLPanelMediaHUD(viewer_media_t media_impl);

	bool postBuild() override;
	void draw() override;
	void setAlpha(F32 alpha) override;
	bool handleScrollWheel(S32 x, S32 y, S32 clicks) override;

	void updateShape();

	bool isMouseOver();
	void setMediaFocus(bool b)						{ mMediaFocus = b; }

	void nextZoomLevel();
	void resetZoomLevel()							{ mCurrentZoom = ZOOM_NONE; }
	bool isZoomed()									{ return mCurrentZoom == ZOOM_MEDIUM; }

	LLHandle<LLPanelMediaHUD> getHandle() const		{ return mPanelHandle; }

	enum EZoomLevel
	{
		ZOOM_NONE = 0,
		ZOOM_MEDIUM = 1,
		ZOOM_END
	};

	enum EScrollDir
	{
		SCROLL_UP = 0,
		SCROLL_DOWN,
		SCROLL_LEFT,
		SCROLL_RIGHT,
		SCROLL_NONE
	};

	void setMediaFace(LLPointer<LLViewerObject> objectp, S32 face = 0,
					  viewer_media_t media_impl = NULL,
					  LLVector3 pick_normal = LLVector3::zero);

private:
	static void onClickClose(void* user_data);
	static void onClickBack(void* user_data);
	static void onClickForward(void* user_data);
	static void onClickHome(void* user_data);
	static void onClickOpen(void* user_data);
	static void onClickReload(void* user_data);
	static void onClickPlay(void* user_data);
	static void onClickPause(void* user_data);
	static void onClickStop(void* user_data);
	static void onClickMediaStop(void* user_data);
	static void onClickZoom(void* user_data);
	static void onClickVolume(void* user_data);
	static void onScrollUp(void* user_data);
	static void onScrollUpHeld(void* user_data);
	static void onScrollLeft(void* user_data);
	static void onScrollLeftHeld(void* user_data);
	static void onScrollRight(void* user_data);
	static void onScrollRightHeld(void* user_data);
	static void onScrollDown(void* user_data);
	static void onScrollDownHeld(void* user_data);
	static void onScrollStop(void* user_data);
	static void onHoverVolume(void* user_data);
	static void onHoverSlider(LLUICtrl* ctrl, void* user_data);
	static void onVolumeChange(LLUICtrl* ctrl, void* user_data);

	LLViewerMediaImpl* getTargetMediaImpl();
	LLViewerObject* getTargetObject();
	LLPluginClassMedia* getTargetMediaPlugin();

private:
	LLButton* mCloseButton;
	LLButton* mBackButton;
	LLButton* mForwardButton;
	LLButton* mHomeButton;
	LLButton* mOpenButton;
	LLButton* mOpenButton2;
	LLButton* mReloadButton;
	LLButton* mPlayButton;
	LLButton* mPauseButton;
	LLButton* mStopButton;
	LLButton* mMediaStopButton;
	LLButton* mMediaVolumeButton;
	LLButton* mMediaMutedButton;
	LLButton* mZoomButton;
	LLButton* mUnzoomButton;
	LLButton* mZoomButton2;

	LLSlider* mVolumeSlider;

	LLPanel* mFocusedControls;
	LLPanel* mHoverControls;

	LLView* mMediaFullView;

	std::string mOpenButtonTooltip;

	LLUUID mTargetObjectID;
	S32 mTargetObjectFace;
	LLUUID mTargetImplID;
	LLVector3 mTargetObjectNormal;
	bool mTargetIsHUDObject;

	bool mMediaFocus;
	bool mLargeControls;
	bool mHasTimeControl;

	EZoomLevel mCurrentZoom;

	EScrollDir mScrollState;
	LLCoordWindow mLastCursorPos;

	F32 mControlFadeTime;
	LLFrameTimer mMouseMoveTimer;
	LLFrameTimer mFadeTimer;
	LLFrameTimer mVolumeSliderTimer;

	F32 mLastVolume;

	static LLUUID sLastTargetImplID;
	static EZoomLevel sLastMediaZoom;

	LLRootHandle<LLPanelMediaHUD> mPanelHandle;
};

#endif // LL_PANELMEDIAHUD_H
