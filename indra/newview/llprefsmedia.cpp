/** 
 * @file llprefsmedia.cpp
 * @brief Media preference implementation
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

#include "llprefsmedia.h"

#include "llaudioengine.h"
#include "llcheckboxctrl.h"
#include "lluictrlfactory.h"

#include "llpanelaudiovolume.h"
#include "llviewercontrol.h"

class LLPrefsMediaImpl : public LLPanel
{
public:
	LLPrefsMediaImpl();
	~LLPrefsMediaImpl() override		{}

	void refresh() override;
	void draw() override;

	void apply()						{ refreshValues(); }
	void cancel();

private:
	void refreshValues();

	static void* createVolumePanel(void* user_data);

	static void onTabChanged(void* data, bool from_click);

	static void onCommitCheckBoxFilter(LLUICtrl* ctrl, void* user_data);
	static void onCommitCheckBoxAudio(LLUICtrl* ctrl, void* user_data);
	static void onCommitCheckBoxMedia(LLUICtrl* ctrl, void* user_data);
	static void onCommitCheckBoxMediaHUD(LLUICtrl* ctrl, void* user_data);
	static void onCommitCheckBoxSharedMedia(LLUICtrl* ctrl, void* user_data);
	static void onCommitChecIncomingIMSession(LLUICtrl* ctrl, void* user_data);

private:
	LLTabContainer*	mTabContainer;

	F32		mVolume;
	F32		mSounds;
	F32		mAmbient;
	F32		mMusic;
	F32		mMedia;
	F32		mVoice;
	F32		mUI;
	F32		mWind;
	F32		mDoppler;
	F32		mRolloff;

	F32		mHealthReductionThreshold;
	F32		mMoneyChangeThreshold;

	U32		mMediaAutoZoom;

	bool	mMuteAudio;
	bool	mMuteSounds;
	bool	mMuteUI;
	bool	mMuteAmbient;
	bool	mMuteWind;
	bool	mMuteMusic;
	bool	mMuteMedia;
	bool	mMuteVoice;
	bool	mMuteWhenMinimized;
	bool	mEnableGestureSounds;
	bool	mEnableAttachmentSounds;
	bool	mNeighborSimsSounds;

	bool	mUISndAlertEnable;
	bool	mUISndBadKeystrokeEnable;
	bool	mUISndClickEnable;
	bool	mUISndClickReleaseEnable;
	bool	mUISndInvalidOpEnable;
	bool	mUISndMoneyChangeDownEnable;
	bool	mUISndMoneyChangeUpEnable;
	bool	mUISndNewIncomingIMSessionEnable;
	bool	mUISndNewIncomingPlayForGroup;
	bool	mUISndObjectCreateEnable;
	bool	mUISndObjectDeleteEnable;
	bool	mUISndObjectRezInEnable;
	bool	mUISndObjectRezOutEnable;
	bool	mUISndPieMenuAppearEnable;
	bool	mUISndPieMenuHideEnable;
	bool	mUISndPieMenuSliceHighlightEnable;
	bool	mUISndSnapshotEnable;
	bool	mUISndStartIMEnable;
	bool	mUISndTeleportOutEnable;
	bool	mUISndTypingEnable;
	bool	mUISndWindowCloseEnable;
	bool	mUISndWindowOpenEnable;
	bool	mUISndHealthReductionFEnable;
	bool	mUISndHealthReductionMEnable;
	bool	mEnableCollisionSounds;

	bool	mMediaEnableFilter;
	bool	mMediaLookupIP;
	bool	mStreamingMusic;
	bool	mNotifyStreamChanges;
	bool	mStreamingMedia;
	bool	mParcelMediaAutoPlay;
	bool	mMediaOnAPrimUI;
	bool	mPrimMediaMaster;
	bool	mMediaShowOnOthers;
	bool	mMediaShowWithinParcel;
	bool	mMediaShowOutsideParcel;

	bool	mRunningFMOD;

	bool	mFirstRun;
};

//static
void* LLPrefsMediaImpl::createVolumePanel(void* user_data)
{
	LLPanelAudioVolume* panel = new LLPanelAudioVolume();
	return panel;
}

LLPrefsMediaImpl::LLPrefsMediaImpl()
:	mFirstRun(true)
{
	mFactoryMap["Volume Panel"]	= LLCallbackMap(createVolumePanel, NULL);
	LLUICtrlFactory::getInstance()->buildPanel(this,
											   "panel_preferences_media.xml",
											   &getFactoryMap());

	mTabContainer = getChild<LLTabContainer>("Audio and Media");
	LLPanel* tab = mTabContainer->getChild<LLPanel>("Audio");
	mTabContainer->setTabChangeCallback(tab, onTabChanged);
	mTabContainer->setTabUserData(tab, this);
	tab = mTabContainer->getChild<LLPanel>("Media");
	mTabContainer->setTabChangeCallback(tab, onTabChanged);
	mTabContainer->setTabUserData(tab, this);

	childSetCommitCallback("media_enable_filter", onCommitCheckBoxFilter, this);
	childSetCommitCallback("streaming_music", onCommitCheckBoxAudio, this);
	childSetCommitCallback("streaming_video", onCommitCheckBoxMedia, this);
	childSetCommitCallback("media_hud", onCommitCheckBoxMediaHUD, this);
	childSetCommitCallback("shared_media", onCommitCheckBoxSharedMedia, this);
	childSetCommitCallback("enable_UISndNewIncomingIMSessionEnable",
						   onCommitChecIncomingIMSession, this);

	refresh();
}

void LLPrefsMediaImpl::draw()
{
	if (mFirstRun)
	{
		mFirstRun = false;
		mTabContainer->selectTab(gSavedSettings.getS32("LastMediaPrefTab"));
	}

	LLPanel::draw();
}

void LLPrefsMediaImpl::refreshValues()
{
	mVolume = gSavedSettings.getF32("AudioLevelMaster");
	mMuteAudio = gSavedSettings.getBool("MuteAudio");
	mUI = gSavedSettings.getF32("AudioLevelUI");
	mMuteUI = gSavedSettings.getBool("MuteUI");
	mSounds = gSavedSettings.getF32("AudioLevelSFX");
	mMuteSounds = gSavedSettings.getBool("MuteSounds");
	mAmbient = gSavedSettings.getF32("AudioLevelAmbient");
	mMuteAmbient = gSavedSettings.getBool("MuteAmbient");
	mWind = gSavedSettings.getF32("AudioLevelWind");
	mMuteWind = gSavedSettings.getBool("DisableWindAudio");
	mMusic = gSavedSettings.getF32("AudioLevelMusic");
	mMuteMusic = gSavedSettings.getBool("MuteMusic");
	mMedia = gSavedSettings.getF32("AudioLevelMedia");
	mMuteMedia = gSavedSettings.getBool("MuteMedia");
	mVoice = gSavedSettings.getF32("AudioLevelVoice");
	mMuteVoice = gSavedSettings.getBool("MuteVoice");
	mMediaAutoZoom = gSavedSettings.getU32("MediaAutoZoom");
	mMuteWhenMinimized = gSavedSettings.getBool("MuteWhenMinimized");
	mEnableGestureSounds = gSavedSettings.getBool("EnableGestureSounds");
	mEnableAttachmentSounds = gSavedSettings.getBool("EnableAttachmentSounds");
	mNeighborSimsSounds = gSavedSettings.getBool("NeighborSimsSounds");
	mDoppler = gSavedSettings.getF32("AudioLevelDoppler");
	mRolloff = gSavedSettings.getF32("AudioLevelRolloff");

	mMoneyChangeThreshold = gSavedSettings.getF32("UISndMoneyChangeThreshold");
	mHealthReductionThreshold =
		gSavedSettings.getF32("UISndHealthReductionThreshold");

	mMediaEnableFilter = gSavedSettings.getBool("MediaEnableFilter");
	mMediaLookupIP = gSavedSettings.getBool("MediaLookupIP");
	mStreamingMusic = gSavedSettings.getBool("EnableStreamingMusic");
	mNotifyStreamChanges = gSavedSettings.getBool("NotifyStreamChanges");
	mStreamingMedia = gSavedSettings.getBool("EnableStreamingMedia");
	mMediaOnAPrimUI = gSavedSettings.getBool("MediaOnAPrimUI");
	mParcelMediaAutoPlay = gSavedSettings.getBool("ParcelMediaAutoPlayEnable");
	mPrimMediaMaster = gSavedSettings.getBool("PrimMediaMasterEnabled");
	mMediaShowOnOthers = gSavedSettings.getBool("MediaShowOnOthers");
	mMediaShowWithinParcel = gSavedSettings.getBool("MediaShowWithinParcel");
	mMediaShowOutsideParcel = gSavedSettings.getBool("MediaShowOutsideParcel");

	mUISndAlertEnable = gSavedSettings.getBool("UISndAlertEnable");
	mUISndBadKeystrokeEnable =
		gSavedSettings.getBool("UISndBadKeystrokeEnable");
	mUISndClickEnable = gSavedSettings.getBool("UISndClickEnable");
	mUISndClickReleaseEnable =
		gSavedSettings.getBool("UISndClickReleaseEnable");
	mUISndInvalidOpEnable = gSavedSettings.getBool("UISndInvalidOpEnable");
	mUISndMoneyChangeDownEnable =
		gSavedSettings.getBool("UISndMoneyChangeDownEnable");
	mUISndMoneyChangeUpEnable =
		gSavedSettings.getBool("UISndMoneyChangeUpEnable");
	mUISndNewIncomingIMSessionEnable =
		gSavedSettings.getBool("UISndNewIncomingIMSessionEnable");
	mUISndNewIncomingPlayForGroup =
		gSavedSettings.getBool("UISndNewIncomingPlayForGroup");
	mUISndObjectCreateEnable =
		gSavedSettings.getBool("UISndObjectCreateEnable");
	mUISndObjectDeleteEnable =
		gSavedSettings.getBool("UISndObjectDeleteEnable");
	mUISndObjectRezInEnable = gSavedSettings.getBool("UISndObjectRezInEnable");
	mUISndObjectRezOutEnable =
		gSavedSettings.getBool("UISndObjectRezOutEnable");
	mUISndPieMenuAppearEnable =
		gSavedSettings.getBool("UISndPieMenuAppearEnable");
	mUISndPieMenuHideEnable = gSavedSettings.getBool("UISndPieMenuHideEnable");
	mUISndPieMenuSliceHighlightEnable =
		gSavedSettings.getBool("UISndPieMenuSliceHighlightEnable");
	mUISndSnapshotEnable = gSavedSettings.getBool("UISndSnapshotEnable");
	mUISndStartIMEnable = gSavedSettings.getBool("UISndStartIMEnable");
	mUISndTeleportOutEnable = gSavedSettings.getBool("UISndTeleportOutEnable");
	mUISndTypingEnable = gSavedSettings.getBool("UISndTypingEnable");
	mUISndWindowCloseEnable = gSavedSettings.getBool("UISndWindowCloseEnable");
	mUISndWindowOpenEnable = gSavedSettings.getBool("UISndWindowOpenEnable");
	mUISndHealthReductionFEnable =
		gSavedSettings.getBool("UISndHealthReductionFEnable");
	mUISndHealthReductionMEnable = 
		gSavedSettings.getBool("UISndHealthReductionMEnable");
	mEnableCollisionSounds = gSavedSettings.getBool("EnableCollisionSounds");
}

void LLPrefsMediaImpl::refresh()
{
	refreshValues();

	// Disable sub-settings check boxes when needed
#if LL_FMOD
	mRunningFMOD = gAudiop && gAudiop->getDriverName(false) == "FMODStudio";
#else
	mRunningFMOD = false;
#endif
	childSetEnabled("notify_stream_changes",
					mRunningFMOD && mStreamingMusic);
	if (!mRunningFMOD)
	{
		gSavedSettings.setBool("NotifyStreamChanges", false);
	}
	childSetEnabled("media_hud", mStreamingMedia);
	childSetEnabled("text_box_zoom", mStreamingMedia && mMediaOnAPrimUI);
	childSetEnabled("auto_zoom", mStreamingMedia && mMediaOnAPrimUI);
	childSetEnabled("auto_streaming_video", mStreamingMedia);
	childSetEnabled("shared_media", mStreamingMedia);
	childSetEnabled("within_parcel", mStreamingMedia);
	childSetEnabled("outside_parcel", mStreamingMedia);
	if (!mStreamingMedia)
	{
		gSavedSettings.setBool("ParcelMediaAutoPlayEnable", false);
		mParcelMediaAutoPlay = false;
	}
	childSetEnabled("media_lookup_ip", mMediaEnableFilter);

	bool shared_media = mStreamingMedia && mPrimMediaMaster;
	childSetEnabled("on_others", shared_media);
	childSetEnabled("within_parcel", shared_media);
	childSetEnabled("outside_parcel", shared_media);

	childSetEnabled("enable_UISndNewIncomingPlayForGroup",
					mUISndNewIncomingIMSessionEnable);
	if (!mUISndNewIncomingIMSessionEnable)
	{
		gSavedSettings.setBool("UISndNewIncomingPlayForGroup", false);
	}
}

void LLPrefsMediaImpl::cancel()
{
	gSavedSettings.setF32("AudioLevelMaster", mVolume);
	gSavedSettings.setBool("MuteAudio", mMuteAudio);
	gSavedSettings.setF32("AudioLevelUI", mUI);
	gSavedSettings.setBool("MuteUI", mMuteUI);
	gSavedSettings.setF32("AudioLevelSFX", mSounds);
	gSavedSettings.setBool("MuteSounds", mMuteSounds);
	gSavedSettings.setF32("AudioLevelAmbient", mAmbient);
	gSavedSettings.setBool("MuteAmbient", mMuteAmbient);
	gSavedSettings.setF32("AudioLevelWind", mWind);
	gSavedSettings.setBool("DisableWindAudio", mMuteWind);
	gSavedSettings.setF32("AudioLevelMusic", mMusic);
	gSavedSettings.setBool("MuteMusic", mMuteMusic);
	gSavedSettings.setF32("AudioLevelMedia", mMedia);
	gSavedSettings.setBool("MuteMedia", mMuteMedia);
	gSavedSettings.setF32("AudioLevelVoice", mVoice);
	gSavedSettings.setBool("MuteVoice", mMuteVoice);
	gSavedSettings.setBool("MuteWhenMinimized", mMuteWhenMinimized);
	gSavedSettings.setBool("EnableGestureSounds", mEnableGestureSounds);
	gSavedSettings.setBool("EnableAttachmentSounds", mEnableAttachmentSounds);
	gSavedSettings.setBool("NeighborSimsSounds", mNeighborSimsSounds);
	gSavedSettings.setF32("AudioLevelDoppler", mDoppler);
	gSavedSettings.setF32("AudioLevelRolloff", mRolloff);

	gSavedSettings.setF32("UISndMoneyChangeThreshold", mMoneyChangeThreshold);
	gSavedSettings.setF32("UISndHealthReductionThreshold",
						  mHealthReductionThreshold);

	gSavedSettings.setBool("MediaEnableFilter", mMediaEnableFilter);
	gSavedSettings.setBool("MediaLookupIP", mMediaLookupIP);
	gSavedSettings.setBool("EnableStreamingMusic", mStreamingMusic);
	gSavedSettings.setBool("NotifyStreamChanges", mNotifyStreamChanges);
	gSavedSettings.setBool("EnableStreamingMedia", mStreamingMedia);
	gSavedSettings.setBool("MediaOnAPrimUI", mMediaOnAPrimUI);
	gSavedSettings.setU32("MediaAutoZoom", mMediaAutoZoom);
	gSavedSettings.setBool("ParcelMediaAutoPlayEnable", mParcelMediaAutoPlay);
	gSavedSettings.setBool("PrimMediaMasterEnabled", mPrimMediaMaster);
	gSavedSettings.setBool("MediaShowOnOthers", mMediaShowOnOthers);
	gSavedSettings.setBool("MediaShowWithinParcel", mMediaShowWithinParcel);
	gSavedSettings.setBool("MediaShowOutsideParcel", mMediaShowOutsideParcel);

	gSavedSettings.setBool("UISndAlertEnable", mUISndAlertEnable);
	gSavedSettings.setBool("UISndBadKeystrokeEnable",
						   mUISndBadKeystrokeEnable);
	gSavedSettings.setBool("UISndClickEnable", mUISndClickEnable);
	gSavedSettings.setBool("UISndClickReleaseEnable",
						   mUISndClickReleaseEnable);
	gSavedSettings.setBool("UISndInvalidOpEnable", mUISndInvalidOpEnable);
	gSavedSettings.setBool("UISndMoneyChangeDownEnable",
						   mUISndMoneyChangeDownEnable);
	gSavedSettings.setBool("UISndMoneyChangeUpEnable",
						   mUISndMoneyChangeUpEnable);
	gSavedSettings.setBool("UISndNewIncomingIMSessionEnable",
						   mUISndNewIncomingIMSessionEnable);
	gSavedSettings.setBool("UISndNewIncomingPlayForGroup",
						   mUISndNewIncomingPlayForGroup);
	gSavedSettings.setBool("UISndObjectCreateEnable",
						   mUISndObjectCreateEnable);
	gSavedSettings.setBool("UISndObjectDeleteEnable",
						   mUISndObjectDeleteEnable);
	gSavedSettings.setBool("UISndObjectRezInEnable", mUISndObjectRezInEnable);
	gSavedSettings.setBool("UISndObjectRezOutEnable",
						   mUISndObjectRezOutEnable);
	gSavedSettings.setBool("UISndPieMenuAppearEnable",
						   mUISndPieMenuAppearEnable);
	gSavedSettings.setBool("UISndPieMenuHideEnable", mUISndPieMenuHideEnable);
	gSavedSettings.setBool("UISndPieMenuSliceHighlightEnable",
						   mUISndPieMenuSliceHighlightEnable);
	gSavedSettings.setBool("UISndSnapshotEnable", mUISndSnapshotEnable);
	gSavedSettings.setBool("UISndStartIMEnable", mUISndStartIMEnable);
	gSavedSettings.setBool("UISndTeleportOutEnable", mUISndTeleportOutEnable);
	gSavedSettings.setBool("UISndTypingEnable", mUISndTypingEnable);
	gSavedSettings.setBool("UISndWindowCloseEnable", mUISndWindowCloseEnable);
	gSavedSettings.setBool("UISndWindowOpenEnable", mUISndWindowOpenEnable);
	gSavedSettings.setBool("UISndHealthReductionFEnable",
						   mUISndHealthReductionFEnable);
	gSavedSettings.setBool("UISndHealthReductionMEnable",
						   mUISndHealthReductionMEnable);
	gSavedSettings.setBool("EnableCollisionSounds", mEnableCollisionSounds);
}

//static
void LLPrefsMediaImpl::onTabChanged(void* data, bool from_click)
{
	LLPrefsMediaImpl* self = (LLPrefsMediaImpl*)data;
	if (self && self->mTabContainer)
	{
		gSavedSettings.setS32("LastMediaPrefTab",
							  self->mTabContainer->getCurrentPanelIndex());
	}
}

//static
void LLPrefsMediaImpl::onCommitCheckBoxMedia(LLUICtrl* ctrl, void* user_data)
{
	LLPrefsMediaImpl* self = (LLPrefsMediaImpl*)user_data;
	LLCheckBoxCtrl* check = (LLCheckBoxCtrl*)ctrl;
	if (!self || !check) return;

	bool enabled = check->get();
	if (!enabled)
	{
		gSavedSettings.setBool("ParcelMediaAutoPlayEnable", false);
	}
	self->refresh();
}

//static
void LLPrefsMediaImpl::onCommitCheckBoxMediaHUD(LLUICtrl* ctrl,
												void* user_data)
{
	LLPrefsMediaImpl* self = (LLPrefsMediaImpl*)user_data;
	LLCheckBoxCtrl* check = (LLCheckBoxCtrl*)ctrl;
	if (self && check)
	{
		self->refresh();
		bool enable = check->get();
		self->childSetEnabled("text_box_zoom", enable);
		self->childSetEnabled("auto_zoom", enable);
	}
}

//static
void LLPrefsMediaImpl::onCommitCheckBoxSharedMedia(LLUICtrl* ctrl,
												   void* user_data)
{
	LLPrefsMediaImpl* self = (LLPrefsMediaImpl*)user_data;
	LLCheckBoxCtrl* check = (LLCheckBoxCtrl*)ctrl;
	if (self && check)
	{
		bool enable = check->get();
		self->childSetEnabled("on_others", enable);
		self->childSetEnabled("within_parcel", enable);
		self->childSetEnabled("outside_parcel", enable);
	}
}

//static
void LLPrefsMediaImpl::onCommitChecIncomingIMSession(LLUICtrl* ctrl,
													 void* user_data)
{
	LLPrefsMediaImpl* self = (LLPrefsMediaImpl*)user_data;
	LLCheckBoxCtrl* check = (LLCheckBoxCtrl*)ctrl;
	if (self && check)
	{
		bool enable = check->get();
		gSavedSettings.setBool("UISndNewIncomingPlayForGroup", enable);
		self->childSetEnabled("enable_UISndNewIncomingPlayForGroup", enable);
	}
}

//static
void LLPrefsMediaImpl::onCommitCheckBoxAudio(LLUICtrl* ctrl, void* user_data)
{
	LLPrefsMediaImpl* self = (LLPrefsMediaImpl*)user_data;
	LLCheckBoxCtrl* check = (LLCheckBoxCtrl*)ctrl;
	if (!self || !check) return;

	self->childSetEnabled("notify_stream_changes",
						  self->mRunningFMOD && check->get());
}

//static
void LLPrefsMediaImpl::onCommitCheckBoxFilter(LLUICtrl* ctrl, void* user_data)
{
	LLPrefsMediaImpl* self = (LLPrefsMediaImpl*)user_data;
	LLCheckBoxCtrl* check = (LLCheckBoxCtrl*)ctrl;
	if (self && check)
	{
		self->childSetEnabled("media_lookup_ip", check->get());
	}
}

//---------------------------------------------------------------------------

LLPrefsMedia::LLPrefsMedia()
:	impl(* new LLPrefsMediaImpl())
{
}

LLPrefsMedia::~LLPrefsMedia()
{
	delete &impl;
}

void LLPrefsMedia::apply()
{
	impl.apply();
}

void LLPrefsMedia::cancel()
{
	impl.cancel();
}

LLPanel* LLPrefsMedia::getPanel()
{
	return &impl;
}
