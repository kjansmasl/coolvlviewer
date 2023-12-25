/**
 * @file lloverlaybar.cpp
 * @brief LLOverlayBar class implementation
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 *
 * Copyright (c) 2002-2009, Linden Research, Inc.
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

// Temporary buttons that appear at the bottom of the screen when you
// are in a mode.

#include "llviewerprecompiledheaders.h"

#include "lloverlaybar.h"

#include "llaudioengine.h"
#include "llbutton.h"
#include "llnotifications.h"
#include "llparcel.h"
#include "llrender.h"
#include "lltextbox.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "llfloatercustomize.h"
#include "llimmgr.h"
#include "llmediactrl.h"
#include "llmediaremotectrl.h"
#include "llpanelaudiovolume.h"
#include "llpathfindingmanager.h"
#include "llpathfindingnavmeshstatus.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llselectmgr.h"
#include "hbviewerautomation.h"
#include "llviewerjoystick.h"
#include "llviewermedia.h"
#include "llviewermenu.h"				// For handle_reset_view()
#include "llviewerparcelmedia.h"
#include "llviewerparcelmgr.h"
#include "llviewertexturelist.h"
#include "llviewerwindow.h"				// For gBottomPanelp
#include "llvoavatarself.h"
#include "llviewercontrol.h"
#include "llvoiceclient.h"
#include "llvoiceremotectrl.h"

//
// Globals
//

//Instance created in LLViewerWindow::initWorldUI()
LLOverlayBar* gOverlayBarp = NULL;

constexpr S32 MAX_BUTTON_WIDTH = 128;	// Maximum buttons width

// Number of media controls (parcel music + parcel media + shared media +
// master volume):
constexpr S32 NUM_MEDIA_CONTROLS = 4;

// Do not refresh the overlay bar layout and icons visibility more than 5 times
// per second to avoid wasting precious CPU cycles for nothing...).
constexpr F32 OVERLAYBAR_REFRESH_INTERVAL = 0.2f;

//
// Functions
//

//static
void* LLOverlayBar::createMasterRemote(void* userdata)
{
	LLOverlayBar* self = (LLOverlayBar*)userdata;
	self->mMasterRemote =
		new LLMediaRemoteCtrl("master_volume", LLRect(),
							  "panel_master_volume.xml",
							  LLMediaRemoteCtrl::REMOTE_MASTER_VOLUME);
	return self->mMasterRemote;
}

//static
void* LLOverlayBar::createParcelMediaRemote(void* userdata)
{
	LLOverlayBar* self = (LLOverlayBar*)userdata;
	self->mParcelMediaRemote =
		new LLMediaRemoteCtrl("parcel_media_remote", LLRect(),
							  "panel_media_remote.xml",
							  LLMediaRemoteCtrl::REMOTE_PARCEL_MEDIA);
	return self->mParcelMediaRemote;
}

//static
void* LLOverlayBar::createSharedMediaRemote(void* userdata)
{
	LLOverlayBar* self = (LLOverlayBar*)userdata;
	self->mSharedMediaRemote =
		new LLMediaRemoteCtrl("shared_media_remote", LLRect(),
							  "panel_shared_media_remote.xml",
							  LLMediaRemoteCtrl::REMOTE_SHARED_MEDIA);
	return self->mSharedMediaRemote;
}

//static
void* LLOverlayBar::createParcelMusicRemote(void* userdata)
{
	LLOverlayBar* self = (LLOverlayBar*)userdata;
	self->mParcelMusicRemote =
		new LLMediaRemoteCtrl("parcel_music_remote", LLRect(),
							  "panel_music_remote.xml",
							  LLMediaRemoteCtrl::REMOTE_PARCEL_MUSIC);
	return self->mParcelMusicRemote;
}

//static
void* LLOverlayBar::createVoiceRemote(void* userdata)
{
	LLOverlayBar* self = (LLOverlayBar*)userdata;
	self->mVoiceRemote = new LLVoiceRemoteCtrl("voice_remote");
	return self->mVoiceRemote;
}

LLOverlayBar::LLOverlayBar(const LLRect& rect)
:	LLPanel("overlay_bar", rect, BORDER_NO),
	mMasterRemote(NULL),
	mParcelMusicRemote(NULL),
	mParcelMediaRemote(NULL),
	mSharedMediaRemote(NULL),
	mVoiceRemote(NULL),
	mStatusBarPad(LLCachedControl<S32>(gSavedSettings, "StatusBarPad")),
	mLastIMsCount(0),
	mCanRebakeRegion(false),
	mRebakeNavMeshMode(kRebakeNavMesh_Default),
	mNavMeshSlot(),
	mRegionCrossingSlot(),
	mAgentStateSlot(),
	mRebakingNotificationID(LLUUID::null),
	mBuilt(false),
	mDirty(false)
{
	llassert_always(gOverlayBarp == NULL);	// Only one instance allowed

	setMouseOpaque(false);
	setIsChrome(true);

	LLCallbackMap::map_t factory_map;
	factory_map["master_volume"] = LLCallbackMap(createMasterRemote, this);
	factory_map["parcel_music_remote"] =
		LLCallbackMap(createParcelMusicRemote, this);
	factory_map["parcel_media_remote"] =
		LLCallbackMap(createParcelMediaRemote, this);
	factory_map["shared_media_remote"] =
		LLCallbackMap(createSharedMediaRemote, this);
	factory_map["voice_remote"] = LLCallbackMap(createVoiceRemote, this);

	LLUICtrlFactory::getInstance()->buildPanel(this, "panel_overlaybar.xml",
											   &factory_map);

	mBtnIMReceiced = getChild<LLButton>("IM Received");
	mBtnIMReceiced->setClickedCallback(onClickIMReceived, this);
	mIMReceivedlabel = mBtnIMReceiced->getLabelUnselected();

	mBtnSetNotBusy = getChild<LLButton>("Set Not Busy");
	mBtnSetNotBusy->setClickedCallback(onClickSetNotBusy, this);

	mBtnFlyCam = getChild<LLButton>("Flycam");
	mBtnFlyCam->setClickedCallback(onClickFlycam, this);

	mBtnMouseLook = getChild<LLButton>("Mouselook");
	mBtnMouseLook->setClickedCallback(onClickMouselook, this);

	mBtnStandUp = getChild<LLButton>("Stand Up");
	mBtnStandUp->setClickedCallback(onClickStandUp, this);

	mBtnPublicBaking = getChild<LLButton>("Public Baking");
	mBtnPublicBaking->setClickedCallback(onClickPublicBaking, this);

	mBtnRebakeRegion = getChild<LLButton>("Rebake Region");
	mBtnRebakeRegion->setClickedCallback(onClickRebakeRegion, this);

	mBtnLuaFunction = getChild<LLButton>("Lua function");
	mBtnLuaFunction->setClickedCallback(onClickLuaFunction, this);

	// Navmesh stuff

	createNavMeshStatusListenerForCurrentRegion();

	if (!mRegionCrossingSlot.connected())
	{
		mRegionCrossingSlot =
			gAgent.addRegionChangedCB(boost::bind(&LLOverlayBar::handleRegionBoundaryCrossed,
												  this));
	}

	LLPathfindingManager* pfmgr = LLPathfindingManager::getInstance();
	if (!mAgentStateSlot.connected())
	{
		mAgentStateSlot =
			pfmgr->registerAgentStateListener(boost::bind(&LLOverlayBar::handleAgentState,
														  this, _1));
	}
	pfmgr->requestGetAgentState();

	setFocusRoot(true);
	mBuilt = true;

	// Make overlay bar conform to window size
	setRect(rect);
	layoutButtons();

	mDirty = true;

	gOverlayBarp = this;
}

//virtual
LLOverlayBar::~LLOverlayBar()
{
	// LLView destructor cleans up children
	gOverlayBarp = NULL;
}

//virtual
void LLOverlayBar::reshape(S32 width, S32 height, bool called_from_parent)
{
	LLView::reshape(width, height, called_from_parent);

	if (mBuilt)
	{
		layoutButtons();
	}
}

//virtual
void LLOverlayBar::setVisible(bool visible)
{
	mDirty = visible;
	LLPanel::setVisible(visible);
}

void LLOverlayBar::layoutButtons()
{
	S32 width = getRect().getWidth();
	S32 count = getChildCount();

	if (mVoiceRemote)
	{
		mVoiceRemoteWidth = mVoiceRemote->getRect().getWidth();
	}
	else
	{
		mVoiceRemoteWidth = 0;
	}
	if (mParcelMediaRemote)
	{
		mParcelMediaRemoteWidth = mParcelMediaRemote->getRect().getWidth();
	}
	else
	{
		mParcelMediaRemoteWidth = 0;
	}
	if (mSharedMediaRemote)
	{
		mSharedMediaRemoteWidth = mSharedMediaRemote->getRect().getWidth();
	}
	else
	{
		mSharedMediaRemoteWidth = 0;
	}
	if (mParcelMusicRemote)
	{
		mParcelMusicRemoteWidth = mParcelMusicRemote->getRect().getWidth();
	}
	else
	{
		mParcelMusicRemoteWidth = 0;
	}
	if (mMasterRemote)
	{
		mMasterRemoteWidth = mMasterRemote->getRect().getWidth();
	}
	else
	{
		mMasterRemoteWidth = 0;
	}

	// total reserved width for all media remotes
	constexpr S32 ENDPAD = 8;
	S32 remote_total_width = mParcelMediaRemoteWidth + mStatusBarPad +
							 mSharedMediaRemoteWidth + mStatusBarPad +
							 mParcelMusicRemoteWidth + mStatusBarPad +
							 mVoiceRemoteWidth + mStatusBarPad +
							 mMasterRemoteWidth + ENDPAD;

	// calculate button widths
	F32 segment_width = (F32)(width - remote_total_width) /
						(F32)(count - NUM_MEDIA_CONTROLS);
	S32 btn_width = llmin(lltrunc(segment_width - mStatusBarPad),
						  MAX_BUTTON_WIDTH);

	// Evenly space all views
	LLRect r;
	S32 i = 0;
	for (child_list_const_iter_t child_iter = getChildList()->begin(),
								 end = getChildList()->end();
		 child_iter != end; ++child_iter)
	{
		LLView* view = *child_iter;
		r = view->getRect();
		r.mLeft = width - ll_round(remote_total_width +
								   (i++ - NUM_MEDIA_CONTROLS + 1) *
								   segment_width);
		r.mRight = r.mLeft + btn_width;
		view->setRect(r);
	}

	// Fix up remotes to have constant width because they cannot shrink
	S32 right = getRect().getWidth() - ENDPAD;
	if (mMasterRemote)
	{
		r = mMasterRemote->getRect();
		r.mRight = right;
		r.mLeft = right - mMasterRemoteWidth;
		right = r.mLeft - mStatusBarPad;
		mMasterRemote->setRect(r);
	}
	if (mParcelMusicRemote)
	{
		r = mParcelMusicRemote->getRect();
		r.mRight = right;
		r.mLeft = right - mParcelMusicRemoteWidth;
		right = r.mLeft - mStatusBarPad;
		mParcelMusicRemote->setRect(r);
	}
	if (mParcelMediaRemote)
	{
		r = mParcelMediaRemote->getRect();
		r.mRight = right;
		r.mLeft = right - mParcelMediaRemoteWidth;
		right = r.mLeft - mStatusBarPad;
		mParcelMediaRemote->setRect(r);
	}
	if (mSharedMediaRemote)
	{
		r = mSharedMediaRemote->getRect();
		r.mRight = right;
		r.mLeft = right - mSharedMediaRemoteWidth;
		right = r.mLeft - mStatusBarPad;
		mSharedMediaRemote->setRect(r);
	}
	if (mVoiceRemote)
	{
		r = mVoiceRemote->getRect();
		r.mRight = right;
		r.mLeft = right - mVoiceRemoteWidth;
		mVoiceRemote->setRect(r);
	}

	updateBoundingRect();
}

//virtual
void LLOverlayBar::draw()
{
	// Do not run all the refresh() cruft every frame: it is pointless !
	if (mDirty ||
		mUpdateTimer.getElapsedTimeF32() >= OVERLAYBAR_REFRESH_INTERVAL)
	{
		refresh();
		mDirty = false;
		mUpdateTimer.reset();
	}

	if (gBottomPanelp)
	{
		static const S32 tex_width = LLUIImage::sRoundedSquareWidth;
		static const S32 tex_height = LLUIImage::sRoundedSquareHeight;
		gGL.getTexUnit(0)->bind(LLUIImage::sRoundedSquare->getImage());

		// Draw rounded rect tabs behind all children
		LLRect r;
		// Focus highlights
		LLColor4 color = LLUI::sFloaterFocusBorderColor;
		gGL.color4fv(color.mV);
		if (gFocusMgr.childHasKeyboardFocus(gBottomPanelp))
		{
			for (child_list_const_iter_t child_iter = getChildList()->begin(),
										 end = getChildList()->end();
				 child_iter != end; ++child_iter)
			{
				LLView* view = *child_iter;
				if (view->getEnabled() && view->getVisible())
				{
					r = view->getRect();
					gl_segmented_rect_2d_tex(r.mLeft - mStatusBarPad / 3 - 1,
											 r.mTop + 3,
											 r.mRight + mStatusBarPad / 3 + 1,
											 r.mBottom, tex_width, tex_height,
											 16, ROUNDED_RECT_TOP);
				}
			}
		}

		// Main tabs
		for (child_list_const_iter_t child_iter = getChildList()->begin(),
									 end = getChildList()->end();
			 child_iter != end; ++child_iter)
		{
			LLView* view = *child_iter;
			if (view->getEnabled() && view->getVisible())
			{
				r = view->getRect();
				// Draw a nice little pseudo-3D outline
				color = LLUI::sDefaultShadowDark;
				gGL.color4fv(color.mV);
				gl_segmented_rect_2d_tex(r.mLeft - mStatusBarPad / 3 + 1,
										 r.mTop + 2,
										 r.mRight + mStatusBarPad / 3,
										 r.mBottom, tex_width, tex_height, 16,
										 ROUNDED_RECT_TOP);

				color = LLUI::sDefaultHighlightLight;
				gGL.color4fv(color.mV);
				gl_segmented_rect_2d_tex(r.mLeft - mStatusBarPad / 3,
										 r.mTop + 2,
										 r.mRight + mStatusBarPad / 3 - 3,
										 r.mBottom, tex_width, tex_height,
										 16, ROUNDED_RECT_TOP);

				// Here is the main background. Note that it overhangs on the
				// bottom so as to hide the focus highlight on the bottom
				// panel, thus producing the illusion that the focus highlight
				// continues around the tabs.
				color = LLUI::sFocusBackgroundColor;
				gGL.color4fv(color.mV);
				gl_segmented_rect_2d_tex(r.mLeft - mStatusBarPad / 3 + 1,
										 r.mTop + 1,
										 r.mRight + mStatusBarPad / 3 - 1,
										 r.mBottom - 1, tex_width, tex_height,
										 16, ROUNDED_RECT_TOP);
			}
		}
	}

	// draw children on top
	LLPanel::draw();
}

//virtual
void LLOverlayBar::refresh()
{
	U32 ims_received = gIMMgrp ? gIMMgrp->getIMsReceived() : 0U;
	if (ims_received != mLastIMsCount)
	{
		mLastIMsCount = ims_received;
		std::string label = mIMReceivedlabel;
		if (gIMMgrp && gIMMgrp->isPrivateIMReceived())
		{
			label += llformat(" (%d*)", ims_received);
		}
		else if (ims_received > 0)
		{
			label += llformat(" (%d)", ims_received);
		}
		mBtnIMReceiced->setLabel(label);
	}
	bool visible = ims_received > 0;
	mBtnIMReceiced->setVisible(visible);
	mBtnIMReceiced->setEnabled(visible);

	static bool old_busy = false;
	static bool old_auto_reply = false;
	bool busy = gAgent.getBusy();
	bool auto_reply = gAgent.getAutoReply();
	if (busy != old_busy || auto_reply != old_auto_reply)
	{
		if (auto_reply)
		{
			mBtnSetNotBusy->setLabel(getString("no_auto_reply_label"));
			mBtnSetNotBusy->setToolTip(getString("no_auto_reply_tooltip"));
		}
		else
		{
			mBtnSetNotBusy->setLabel(getString("set_not_busy_label"));
			mBtnSetNotBusy->setToolTip(getString("set_not_busy_tooltip"));
		}
		old_busy = busy;
		old_auto_reply = auto_reply;
	}
	visible = busy || auto_reply;
	mBtnSetNotBusy->setVisible(visible);
	mBtnSetNotBusy->setEnabled(visible);

	visible = LLViewerJoystick::getInstance()->getOverrideCamera();
	mBtnFlyCam->setVisible(visible);
	mBtnFlyCam->setEnabled(visible);

	visible = gAgent.isControlGrabbed(CONTROL_ML_LBUTTON_DOWN_INDEX) ||
			  gAgent.isControlGrabbed(CONTROL_ML_LBUTTON_UP_INDEX);
	mBtnMouseLook->setVisible(visible);
	mBtnMouseLook->setEnabled(visible);

	if (isAgentAvatarValid())
	{
//MK
		if (gRLenabled && gRLInterface.mContainsUnsit)
		{
			visible = false;
		}
		else
//mk
		{
			visible = gAgentAvatarp->mIsSitting;
		}
		mBtnStandUp->setVisible(visible);
		mBtnStandUp->setEnabled(visible);
	}

	visible = !LLFloaterCustomize::isVisible() && isAgentAvatarValid() &&
			  gAgentAvatarp->isEditingAppearance();
	mBtnPublicBaking->setVisible(visible);
	mBtnPublicBaking->setEnabled(visible);

	visible = mCanRebakeRegion &&
			  mRebakeNavMeshMode == kRebakeNavMesh_Available;
	mBtnRebakeRegion->setVisible(visible);
	mBtnRebakeRegion->setEnabled(visible);

	visible = !mLuaCommand.empty();
	mBtnLuaFunction->setVisible(visible);
	mBtnLuaFunction->setEnabled(visible);

	constexpr S32 ENDPAD = 8;
	S32 right = getRect().getWidth() - mMasterRemoteWidth - mStatusBarPad -
				ENDPAD;
	LLRect r;

	static LLCachedControl<bool> hide_master_remote(gSavedSettings,
													"HideMasterRemote");
	visible = !hide_master_remote;

	LLParcel* parcel = gViewerParcelMgr.getAgentParcel();

	if (mParcelMusicRemote)
	{
		static LLCachedControl<bool> audio_streaming_music(gSavedSettings,
														   "EnableStreamingMusic");
		if (gAudiop && audio_streaming_music && parcel &&
			!parcel->getMusicURL().empty())
		{
			mParcelMusicRemote->setEnabled(true);
			r = mParcelMusicRemote->getRect();
			r.mRight = right;
			r.mLeft = right - mParcelMusicRemoteWidth;
			right = r.mLeft - mStatusBarPad;
			mParcelMusicRemote->setRect(r);
			mParcelMusicRemote->setVisible(true);
			visible = true;
		}
		else
		{
			mParcelMusicRemote->setVisible(false);
			mParcelMusicRemote->setEnabled(false);
		}
	}

	static LLCachedControl<bool> enable_streaming_media(gSavedSettings,
													    "EnableStreamingMedia");
	if (mParcelMediaRemote)
	{
		if (gAudiop && enable_streaming_media && parcel &&
			!parcel->getMediaURL().empty())
		{
			// display remote control
			mParcelMediaRemote->setEnabled(true);
			r = mParcelMediaRemote->getRect();
			r.mRight = right;
			r.mLeft = right - mParcelMediaRemoteWidth;
			right = r.mLeft - mStatusBarPad;
			mParcelMediaRemote->setRect(r);
			mParcelMediaRemote->setVisible(true);
			visible = true;
		}
		else
		{
			mParcelMediaRemote->setVisible(false);
			mParcelMediaRemote->setEnabled(false);
		}
	}

	if (mSharedMediaRemote)
	{
		static LLCachedControl<bool> enable_shared_media(gSavedSettings,
														 "PrimMediaMasterEnabled");
		if (gAudiop && enable_streaming_media && enable_shared_media &&
			(LLViewerMedia::isAnyMediaEnabled() ||
			 LLViewerMedia::isAnyMediaDisabled()))
		{
			// display remote control
			mSharedMediaRemote->setEnabled(true);
			r = mSharedMediaRemote->getRect();
			r.mRight = right;
			r.mLeft = right - mSharedMediaRemoteWidth;
			right = r.mLeft - mStatusBarPad;
			mSharedMediaRemote->setRect(r);
			mSharedMediaRemote->setVisible(true);
			visible = true;
		}
		else
		{
			mSharedMediaRemote->setVisible(false);
			mSharedMediaRemote->setEnabled(false);
		}
	}

	if (mVoiceRemote)
	{
		if (LLVoiceClient::voiceEnabled())
		{
			r = mVoiceRemote->getRect();
			r.mRight = right;
			r.mLeft = right - mVoiceRemoteWidth;
			mVoiceRemote->setRect(r);
			mVoiceRemote->setVisible(true);
			visible = true;
		}
		else
		{
			mVoiceRemote->setVisible(false);
		}
	}

	mMasterRemote->setVisible(visible);
	mMasterRemote->setEnabled(visible);

	updateBoundingRect();
}

void LLOverlayBar::setLuaFunctionButton(const std::string& label,
										const std::string& command,
										const std::string& tooltip)
{
	mLuaCommand = command;
	mBtnLuaFunction->setLabel(label);
	mBtnLuaFunction->setToolTip(tooltip);
	mDirty = true;
}

//static
void LLOverlayBar::onClickLuaFunction(void* data)
{
	LLOverlayBar* self = (LLOverlayBar*)data;
	if (self && !self->mLuaCommand.empty())
	{
		HBViewerAutomation::eval(self->mLuaCommand);
		self->mDirty = true;
	}
}

//static
void LLOverlayBar::onClickIMReceived(void* data)
{
	LLOverlayBar* self = (LLOverlayBar*)data;
	if (self && gIMMgrp)
	{
		gIMMgrp->setFloaterOpen(true);
		self->mDirty = true;
	}
}

//static
void LLOverlayBar::onClickSetNotBusy(void* data)
{
	LLOverlayBar* self = (LLOverlayBar*)data;
	if (self)
	{
		gAgent.clearBusy();
		gAgent.clearAutoReply();
		self->mDirty = true;
	}
}

//static
void LLOverlayBar::onClickFlycam(void* data)
{
	LLOverlayBar* self = (LLOverlayBar*)data;
	if (self)
	{
		LLViewerJoystick::getInstance()->toggleFlycam();
		self->mDirty = true;
	}
}

//static
void LLOverlayBar::onClickResetView(void* data)
{
	LLOverlayBar* self = (LLOverlayBar*)data;
	if (self)
	{
		handle_reset_view();
		self->mDirty = true;
	}
}

//static
void LLOverlayBar::onClickMouselook(void* data)
{
	LLOverlayBar* self = (LLOverlayBar*)data;
	if (self)
	{
		gAgent.changeCameraToMouselook();
		self->mDirty = true;
	}
}

//static
void LLOverlayBar::onClickStandUp(void* data)
{
	LLOverlayBar* self = (LLOverlayBar*)data;
	if (!self) return;

//MK
	if (gRLenabled && gRLInterface.mContainsUnsit &&
		isAgentAvatarValid() && gAgentAvatarp->mIsSitting)
	{
		return;
	}
//mk
	gSelectMgr.deselectAllForStandingUp();
	LL_DEBUGS("AgentSit") << "Sending agent unsit request" << LL_ENDL;
	gAgent.setControlFlags(AGENT_CONTROL_STAND_UP);
//MK
	if (gRLenabled && gRLInterface.mContainsStandtp)
	{
		gRLInterface.backToLastStandingLoc();
	}
//mk

	self->mDirty = true;
}

//static
void LLOverlayBar::onClickPublicBaking(void* data)
{
	LLOverlayBar* self = (LLOverlayBar*)data;
	if (!self) return;

	if (isAgentAvatarValid() && gAgentAvatarp->isEditingAppearance() &&
		!LLFloaterCustomize::isVisible())
	{
		LLVOAvatarSelf::onCustomizeEnd();
	}

	self->mDirty = true;
}

///////////////////////////////////////////////////////////////////////////////
// Navmesh stuff

void LLOverlayBar::setRebakeMode(ERebakeNavMeshMode mode)
{
	if (mode == kRebakeNavMesh_Available)
	{
		gNotifications.add("PathfindingRebakeNavmesh");
	}
	else if (mode == kRebakeNavMesh_RequestSent)
	{
		LLNotificationPtr n = gNotifications.add("RebakeNavmeshSent");
		if (n)
		{
			mRebakingNotificationID = n->getID();
		}
	}
	else if (mode != kRebakeNavMesh_InProgress &&
			 mRebakingNotificationID.notNull())
	{
		LLNotificationPtr n = gNotifications.find(mRebakingNotificationID);
		if (n)
		{
			gNotifications.cancel(n);
		}
		mRebakingNotificationID.setNull();
	}

	mRebakeNavMeshMode = mode;
	mDirty = true;
}

void LLOverlayBar::handleAgentState(bool can_rebake_region)
{
	LL_DEBUGS("NavMesh") << "Received agent state. Rebake region flag: "
						 << can_rebake_region << LL_ENDL;

	mCanRebakeRegion = can_rebake_region;
	mDirty = true;
}

void LLOverlayBar::handleRebakeNavMeshResponse(bool status_response)
{
	if (mRebakeNavMeshMode == kRebakeNavMesh_RequestSent)
	{
		setRebakeMode(status_response ? kRebakeNavMesh_InProgress
									  : kRebakeNavMesh_Default);
	}
	LL_DEBUGS("NavMesh") << "Received rebake navmesh response. New rebake mode is: "
						 << mRebakeNavMeshMode << LL_ENDL;

	if (!status_response)
	{
		gNotifications.add("PathfindingCannotRebakeNavmesh");
	}
}

void LLOverlayBar::handleNavMeshStatus(const LLPathfindingNavMeshStatus& statusp)
{
	ERebakeNavMeshMode mode = kRebakeNavMesh_Default;
	if (statusp.isValid())
	{
		switch (statusp.getStatus())
		{
			case LLPathfindingNavMeshStatus::kPending:
			case LLPathfindingNavMeshStatus::kRepending:
				mode = kRebakeNavMesh_Available;
				break;

			case LLPathfindingNavMeshStatus::kBuilding:
				mode = kRebakeNavMesh_InProgress;
				break;

			case LLPathfindingNavMeshStatus::kComplete:
				mode = kRebakeNavMesh_NotAvailable;
				break;

			default:
				mode = kRebakeNavMesh_Default;
				llwarns << "Invalid rebake mode: " << statusp.getStatus()
						<< llendl;
				llassert(false);
		}
	}

	setRebakeMode(mode);

	LL_DEBUGS("NavMesh") << "Received navmesh status. New rebake mode: "
						 << mode << LL_ENDL;
}

void LLOverlayBar::handleRegionBoundaryCrossed()
{
	createNavMeshStatusListenerForCurrentRegion();
	mCanRebakeRegion = false;
	mDirty = true;
	LLPathfindingManager::getInstance()->requestGetAgentState();
}

void LLOverlayBar::createNavMeshStatusListenerForCurrentRegion()
{
	if (mNavMeshSlot.connected())
	{
		mNavMeshSlot.disconnect();
	}

	LLViewerRegion* region = gAgent.getRegion();
	if (!region) return;

	LLPathfindingManager* pfmgr = LLPathfindingManager::getInstance();
	mNavMeshSlot =
		pfmgr->registerNavMeshListenerForRegion(region,
												boost::bind(&LLOverlayBar::handleNavMeshStatus,
															this, _2));
	pfmgr->requestGetNavMeshForRegion(region, true);
}

//static
void LLOverlayBar::onClickRebakeRegion(void* userdata)
{
	LLOverlayBar* self = (LLOverlayBar*)userdata;
	if (!self) return;

	self->setRebakeMode(kRebakeNavMesh_RequestSent);
	LLPathfindingManager* pfmgr = LLPathfindingManager::getInstance();
	pfmgr->requestRebakeNavMesh(boost::bind(&LLOverlayBar::handleRebakeNavMeshResponse,
											self, _1));
}

///////////////////////////////////////////////////////////////////////////////
// static media helpers

//static
void LLOverlayBar::toggleAudioVolumeFloater(void* user_data)
{
	LLFloaterAudioVolume::toggleInstance(LLSD());
}
