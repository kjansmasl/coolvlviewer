/**
 * @file llfloaternearbymedia.h
 * @brief Management interface for muting and controlling nearby media
 *
 * $LicenseInfo:firstyear=2010&license=viewergpl$
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

#ifndef LL_LLFLOATERNEARBYMEDIA_H
#define LL_LLFLOATERNEARBYMEDIA_H

#include "llfloater.h"

#include "llviewercontrol.h"

class LLButton;
class LLComboBox;
class LLLineEditor;
class LLScrollListCtrl;
class LLScrollListItem;
class LLSlider;
class LLTextBox;
class LLViewerMediaImpl;

class LLFloaterNearByMedia final
:	public LLFloater,
	public LLFloaterSingleton<LLFloaterNearByMedia>
{
	friend class LLUISingleton<LLFloaterNearByMedia,
							   VisibilityPolicy<LLFloater> >;

public:
	~LLFloaterNearByMedia() override;

	bool postBuild() override;
	void draw() override;

private:
	// Open only via LLFloaterSingleton interface, i.e. showInstance() or
	// toggleInstance().
	LLFloaterNearByMedia(const LLSD&);

	enum ColumnIndex {
		CHECKBOX_COLUMN = 0,
		PROXIMITY_COLUMN = 1,
		VISIBILITY_COLUMN = 2,
		CLASS_COLUMN = 3,
		NAME_COLUMN = 4
	};

	// Media "class" enumeration
	enum MediaClass {
		MEDIA_CLASS_ALL = 0,
		MEDIA_CLASS_WITHIN_PARCEL = 1,
		MEDIA_CLASS_OUTSIDE_PARCEL = 2,
		MEDIA_CLASS_ON_OTHERS = 3,
		MEDIA_CLASS_ON_HUD_OBJECT = 4
	};

	void updateControls();
	void showBasicControls(bool playing, bool include_zoom, bool is_zoomed,
						   bool muted, F32 volume);
	void showTimeBasedControls(bool playing, bool paused,
							   bool include_zoom, bool is_zoomed,
							   bool muted, F32 volume);
	void showDisabledControls();

	void refreshParcelItems();
	void refreshMediaURL();
	void refreshList(bool fresh = false);

	bool shouldShow(LLViewerMediaImpl* impl);

	LLScrollListItem* addListItem(const LLUUID& id);
	void updateListItem(LLScrollListItem* item, LLViewerMediaImpl* impl);
	void updateListItem(LLScrollListItem* item,
						const std::string& item_name,
						const std::string& item_tooltip,
						S32 proximity,
						bool is_disabled,
						bool has_media,
						bool is_time_based_and_playing,
						MediaClass media_class,
						bool focused);
	void removeListItem(const LLUUID& id);

	void mediaEnable(const LLUUID& row_id, bool enable = true);

	static void getNameAndUrlHelper(LLViewerMediaImpl* impl,
									std::string& name,
									std::string& url,
									const std::string& default_name);

	// Callbacks
	static void onClickEnableAll(void*);
	static void onClickDisableAll(void*);
	static void onOpenPrefs(void*);
	static void onClickSelectedMediaPlay(void* user_data);
	static void onClickSelectedMediaPause(void* user_data);
	static void onClickSelectedMediaStop(void* user_data);
	static void onClickSelectedMediaMute(void* user_data);
	static void onClickSelectedMediaUnmute(void* user_data);
	static void onClickSelectedMediaZoom(void* user_data);
	static void onClickSelectedMediaUnzoom(void*);
	static void onCheckItem(LLUICtrl* ctrl, void* user_data);
	static void onCommitMediaType(LLUICtrl*, void* user_data);
	static void onCommitMediaURL(LLUICtrl* ctrl, void* user_data);
	static void onSelectNewMedia(LLUICtrl* ctrl, void* data);
	static void onCommitSelectedMediaVolume(LLUICtrl*, void* user_data);

private:
	LLButton*				mEnableAllButton;
	LLButton*				mDisableAllButton;
	LLButton*				mOpenPrefsButton;
	LLButton*				mPlayButton;
	LLButton*				mPauseButton;
	LLButton*				mStopButton;
	LLButton*				mZoomButton;
	LLButton*				mUnzoomButton;
	LLButton*				mMuteButton;
	LLButton*				mUnmuteButton;
	LLSlider*				mVolumeSlider;
	LLTextBox*				mMediaURLText;
	LLComboBox*				mMediaTypeCombo;
	LLLineEditor*			mMediaURLEditor;
	LLScrollListCtrl*		mMediaList;
	LLScrollListItem*		mParcelMediaItem;
	LLScrollListItem*		mParcelAudioItem;

	std::string				mEmptyNameString;
	std::string				mPlayingString;
	std::string				mParcelMediaName;
	std::string				mParcelAudioName;
	std::string				mPreviousURL;

	LLCachedControl<bool>	mStreamingMusic;
	LLCachedControl<bool>	mSteamingMedia;
	LLCachedControl<bool>	mSharedMedia;
};

#endif	// LL_LLFLOATERNEARBYMEDIA_H
