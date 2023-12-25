/**
 * @file hbprefscool.cpp
 * @author Henri Beauchamp
 * @brief Cool VL Viewer preferences panel
 *
 * $LicenseInfo:firstyear=2008&license=viewergpl$
 *
 * Copyright (c) 2008, Henri Beauchamp.
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
 * online at http://secondlifegrid.net/programs/open_source/licensing/flossexception
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

#include "hbprefscool.h"

#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "llcolorswatch.h"
#include "llcorehttputil.h"
#include "lldir.h"
#include "llnotifications.h"
#include "llradiogroup.h"
#include "llsdserialize.h"
#include "llsliderctrl.h"
#include "llspellcheck.h"
#include "lltabcontainer.h"
#include "lltextbox.h"
#include "lltexteditor.h"
#include "lluictrlfactory.h"

#include "llenvironment.h"
#include "hbfloaterrlv.h"
#include "llinventorymodel.h"
#include "mkrlinterface.h"
#include "llstartup.h"
#include "llviewercontrol.h"
#include "llvoavatarself.h"

class HBPrefsCoolImpl final : public LLPanel
{
protected:
	LOG_CLASS(HBPrefsCoolImpl);

public:
	HBPrefsCoolImpl();
	~HBPrefsCoolImpl() override;

	void refresh() override;
	void draw() override;

	void apply();
	void cancel();

	void setDirty()								{ mIsDirty = true; }
	static void setQueryActive(bool active);

	std::string getDictName(const std::string& language);
	std::string getDictLanguage(const std::string& name);

	static HBPrefsCoolImpl* getInstance()		{ return sInstance; }

private:
	void refreshValues();
	void setSunPositionLabel(F32 value);
	void refreshRestrainedLove(bool enable);
	void updateRestrainedLoveUserProfile();
	std::string getCategoryPath(LLFolderType::EType cat_type);
	std::string getCategoryPath(const LLUUID& cat_id);

	static void onTabChanged(void* user_data, bool);

	static void onCommitCheckBoxShowButton(LLUICtrl* ctrl, void* user_data);
	static void onCommitCheckBoxSpellCheck(LLUICtrl* ctrl, void* user_data);
	static void onCommitCheckBoxRestrainedLove(LLUICtrl* ctrl, void* user_data);
	static void onCommitCheckBoxSpeedRez(LLUICtrl* ctrl, void* user_data);
	static void onCommitSliderSunPositionAtLogin(LLUICtrl* ctrl, void* user_data);
	static void onCommitUserProfile(LLUICtrl* ctrl, void* user_data);
	static void onCommitCheckBoxAfterRestart(LLUICtrl* ctrl, void* user_data);
	static void onClickCustomBlackList(void* user_data);
	static void onClickDownloadDict(void* user_data);
	static void onClickResetSetting(void* user_data);

	static void fetchDictionaryCoro(std::string dictname);

private:
	static HBPrefsCoolImpl*	sInstance;
	static S32				sQueries;

	LLTabContainer*			mTabContainer;

	LLTextBox*				mSunPositionLabel;
	std::string				mSunPositionSim;

	LLSD					mDictsList;
	std::set<std::string>	mInstalledDicts;

	std::set<std::string>	mWarnedAfterRestart;

	S32						mRestrainedLoveUserProfile;

	bool					mFirstRun;
	bool					mIsDirty;

	bool					mWatchBlackListFloater;

	// Saved values of the settings touched by this panel:
	bool mShowChatButton;
	bool mShowIMButton;
	bool mShowFriendsButton;
	bool mShowGroupsButton;
	bool mShowFlyButton;
	bool mShowSnapshotButton;
	bool mShowSearchButton;
	bool mShowBuildButton;
	bool mShowRadarButton;
	bool mShowMiniMapButton;
	bool mShowMapButton;
	bool mShowInventoryButton;
	bool mHideMasterRemote;
	bool mUseOldChatHistory;
	bool mAutoOpenTextInput;
	bool mIMTabsVerticalStacking;
	bool mUseOldStatusBarIcons;
	bool mUseOldTrackingDots;
	bool mHideTeleportProgress;
	bool mStackMinimizedTopToBottom;
	bool mStackMinimizedRightToLeft;
	bool mAllowMUpose;
	bool mAutoCloseOOC;
	bool mHighlightOwnNameInChat;
	bool mHighlightOwnNameInIM;
	bool mHighlightDisplayName;
	bool mSpellCheck;
	bool mSpellCheckShow;
	bool mAddAvatarNamesToIgnore;
	bool mAllowMultipleViewers;
	bool mSpeedRez;
	bool mTeleportHistoryDeparture;
	bool mMuteListIgnoreServer;
	bool mPreviewAnimInWorld;
	bool mAppearanceAnimation;
	bool mRevokePermsOnStandUp;
	bool mRevokePermsOnStopAnimation;
	bool mResetAnimOverrideOnStopAnimation;
	bool mTurnTowardsSelectedObject;
	bool mRezWithLandGroup;
	bool mNoMultipleShoes;
	bool mNoMultipleSkirts;
	bool mNoMultiplePhysics;
	bool mRenderBanWallAlways;
	bool mAutoShowInventoryThumbnails;
	bool mOSUseCOF;
	bool mOSAllowBakeOnMeshUploads;
	bool mOSWorldMapHasTerrain;
	bool mRestrainedLove;
	bool mRestrainedLoveNoSetEnv;
	bool mRestrainedLoveAllowWear;
	bool mRestrainedLoveForbidGiveToRLV;
	bool mRestrainedLoveShowEllipsis;
	bool mRestrainedLoveUntruncatedEmotes;
	bool mRestrainedLoveCanOoc;
	F32 mSunPositionAtLogin;
	F32 mTaskBarButtonFlashTime;
	U32 mFadeMouselookExitTip;
	U32 mDecimalsForTools;
	U32 mStackScreenWidthFraction;
	U32 mTimeFormat;
	U32 mDateFormat;
	U32 mThumbnailViewTimeout;
	U32 mBackgroundYieldTime;
	U32 mFrameRateLimit;
	U32 mSpeedRezInterval;
	U32 mNumImageDecodeThreads;
	U32 mFetchBoostAfterTPDuration;
	U32 mRenderBanWallMaxDist;
	U32 mDoubleClickInventorySoundAction;
	U32 mRestrainedLoveReattachDelay;
	LLColor4 mOwnNameChatColor;
	std::string mHighlightNicknames;
	std::string mSpellCheckLanguage;
	std::string mExternalEditor;
	std::string mRestrainedLoveRecvimMessage;
	std::string mRestrainedLoveSendimMessage;
	std::string mRestrainedLoveBlacklist;
};

HBPrefsCoolImpl* HBPrefsCoolImpl::sInstance = NULL;
S32 HBPrefsCoolImpl::sQueries = 0;

HBPrefsCoolImpl::HBPrefsCoolImpl()
:	LLPanel(std::string("Cool Preferences Panel")),
	mWatchBlackListFloater(false),
	mFirstRun(true),
	mIsDirty(true)
{
	sInstance = this;

	LLUICtrlFactory::getInstance()->buildPanel(this,
											   "panel_preferences_cool.xml");

	childSetCommitCallback("show_chat_button_check",
						   onCommitCheckBoxShowButton, this);
	childSetCommitCallback("show_im_button_check",
						   onCommitCheckBoxShowButton, this);
	childSetCommitCallback("show_friends_button_check",
						   onCommitCheckBoxShowButton, this);
	childSetCommitCallback("show_group_button_check",
						   onCommitCheckBoxShowButton, this);
	childSetCommitCallback("show_fly_button_check",
						   onCommitCheckBoxShowButton, this);
	childSetCommitCallback("show_snapshot_button_check",
						   onCommitCheckBoxShowButton, this);
	childSetCommitCallback("show_search_button_check",
						   onCommitCheckBoxShowButton, this);
	childSetCommitCallback("show_build_button_check",
						   onCommitCheckBoxShowButton, this);
	childSetCommitCallback("show_radar_button_check",
						   onCommitCheckBoxShowButton, this);
	childSetCommitCallback("show_minimap_button_check",
						   onCommitCheckBoxShowButton, this);
	childSetCommitCallback("show_map_button_check",
						   onCommitCheckBoxShowButton, this);
	childSetCommitCallback("show_inventory_button_check",
						   onCommitCheckBoxShowButton, this);
	childSetCommitCallback("use_old_chat_history_check",
						   onCommitCheckBoxAfterRestart, this);
	childSetCommitCallback("im_tabs_vertical_stacking_check",
						   onCommitCheckBoxAfterRestart, this);
	childSetCommitCallback("spell_check_check",
						   onCommitCheckBoxSpellCheck, this);
	childSetCommitCallback("speed_rez_check",
						   onCommitCheckBoxSpeedRez, this);
	childSetCommitCallback("num_image_decode_threads",
						   onCommitCheckBoxAfterRestart, this);
	childSetCommitCallback("sun_position_at_login",
						   onCommitSliderSunPositionAtLogin, this);
	childSetCommitCallback("os_terrain_map_check",
						   onCommitCheckBoxAfterRestart, this);
	childSetCommitCallback("restrained_love_check",
						   onCommitCheckBoxRestrainedLove, this);
	childSetCommitCallback("user_profile",
						   onCommitUserProfile, this);
	childSetCommitCallback("restrained_love_no_setenv_check",
						   onCommitCheckBoxAfterRestart, this);
	childSetCommitCallback("restrained_love_emotes_check",
						   onCommitCheckBoxAfterRestart, this);
	childSetCommitCallback("restrained_love_can_ooc_check",
						   onCommitCheckBoxAfterRestart, this);

	childSetAction("dict_download_button", onClickDownloadDict, this);
	childSetAction("custom_profile_button", onClickCustomBlackList, this);

	LLLineEditor* line = getChild<LLLineEditor>("external_editor_cmd");
	line->setCommitOnFocusLost(true);
	// Examples of external editor command line for each OS:
#if LL_LINUX
# define EXT_EDIT "/usr/bin/gedit %s"
#elif LL_DARWIN
# define EXT_EDIT "/Applications/TextMate.app/Contents/Resources/mate %s"
#elif LL_WINDOWS
# define EXT_EDIT "\"C:\\Program Files\\Notepad++\\notepad++.exe\" %s"
#endif
	line->setLabelArg("[CMD]", EXT_EDIT);

	mSunPositionLabel = getChild<LLTextBox>("sun_position_text");
	mSunPositionSim = mSunPositionLabel->getText();

	mTabContainer = getChild<LLTabContainer>("Cool Prefs");
	LLPanel* tab = mTabContainer->getChild<LLPanel>("User Interface");
	mTabContainer->setTabChangeCallback(tab, onTabChanged);
	mTabContainer->setTabUserData(tab, this);
	tab = mTabContainer->getChild<LLPanel>("Chat/IM");
	mTabContainer->setTabChangeCallback(tab, onTabChanged);
	mTabContainer->setTabUserData(tab, this);
	tab = mTabContainer->getChild<LLPanel>("Inventory");
	mTabContainer->setTabChangeCallback(tab, onTabChanged);
	mTabContainer->setTabUserData(tab, this);
	tab = mTabContainer->getChild<LLPanel>("Animations");
	mTabContainer->setTabChangeCallback(tab, onTabChanged);
	mTabContainer->setTabUserData(tab, this);
	tab = mTabContainer->getChild<LLPanel>("Miscellaneous");
	mTabContainer->setTabChangeCallback(tab, onTabChanged);
	mTabContainer->setTabUserData(tab, this);
	tab = mTabContainer->getChild<LLPanel>("RestrainedLove");
	mTabContainer->setTabChangeCallback(tab, onTabChanged);
	mTabContainer->setTabUserData(tab, this);

	if (!LLStartUp::isLoggedIn())
	{
		LLCheckBoxCtrl* rl = getChild<LLCheckBoxCtrl>("restrained_love_check");
		std::string tooltip = rl->getToolTip();
		tooltip += " " + getString("when_logged_in");
		rl->setToolTip(tooltip);
	}

	LLControlVariable* control =
		gSavedPerAccountSettings.getControl("UploadAnimationFolder");
	control->getSignal()->connect(boost::bind(&HBPrefsCoolImpl::setDirty,
											  this));
	childSetAction("reset_animation_folder", onClickResetSetting, control);

	control = gSavedPerAccountSettings.getControl("UploadSoundFolder");
	control->getSignal()->connect(boost::bind(&HBPrefsCoolImpl::setDirty,
											  this));
	childSetAction("reset_sound_folder", onClickResetSetting, control);

	control = gSavedPerAccountSettings.getControl("UploadTextureFolder");
	control->getSignal()->connect(boost::bind(&HBPrefsCoolImpl::setDirty,
											  this));
	childSetAction("reset_texture_folder", onClickResetSetting, control);

	control = gSavedPerAccountSettings.getControl("UploadMaterialFolder");
	control->getSignal()->connect(boost::bind(&HBPrefsCoolImpl::setDirty,
											  this));
	childSetAction("reset_material_folder", onClickResetSetting, control);

	control = gSavedPerAccountSettings.getControl("UploadModelFolder");
	control->getSignal()->connect(boost::bind(&HBPrefsCoolImpl::setDirty,
											  this));
	childSetAction("reset_model_folder", onClickResetSetting, control);

	control = gSavedPerAccountSettings.getControl("NewOutfitFolder");
	control->getSignal()->connect(boost::bind(&HBPrefsCoolImpl::setDirty,
											  this));
	childSetAction("reset_new_outfits_folder", onClickResetSetting, control);

	refresh();
}

HBPrefsCoolImpl::~HBPrefsCoolImpl()
{
	sInstance = NULL;
}

void HBPrefsCoolImpl::draw()
{
	if (mFirstRun)
	{
		mFirstRun = false;
		mTabContainer->selectTab(gSavedSettings.getS32("LastCoolPrefTab"));
	}

	if (mIsDirty)
	{
		// First get the list of all installed dictionaries
		mInstalledDicts.clear();
		mInstalledDicts = LLSpellCheck::getInstance()->getBaseDicts();

		// Then get the list of all existing dictionaries
		mDictsList.clear();
		std::string dict_list;
		dict_list = gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS,
												   "dictionaries",
												   "dict_list.xml");
		llifstream inputstream(dict_list.c_str(), std::ios::binary);
		if (inputstream.is_open())
		{
			LLSDSerialize::fromXMLDocument(mDictsList, inputstream);
			inputstream.close();
		}
		if (mDictsList.size() == 0)
		{
			llwarns << "Failure to load the list of all existing dictionaries."
					<< llendl;
		}

		bool found_one = false;
		std::string name = LLSpellCheck::getInstance()->getCurrentDict();
		std::string language;
		S32 selection = -1;
		LLComboBox* combo = getChild<LLComboBox>("installed_dicts_combo");
		combo->removeall();
		for (std::set<std::string>::iterator it = mInstalledDicts.begin();
			 it != mInstalledDicts.end(); it++)
		{
			language = getDictLanguage(*it);
			if (language.empty())
			{
				language = *it;
			}
			combo->add(language);
			found_one = true;
			if (*it == name)
			{
				selection = combo->getItemCount() - 1;
			}
		}
		if (!found_one)
		{
			combo->add(LLStringUtil::null);
		}
		if (selection >= 0)
		{
			combo->setCurrentByIndex(selection);
		}

		found_one = false;
		combo = getChild<LLComboBox>("download_dict_combo");
		combo->removeall();
		for (LLSD::array_const_iterator it = mDictsList.beginArray();
			 it != mDictsList.endArray(); it++)
		{
			const LLSD& entry = *it;
			name = entry["name"].asString();
			if (name.empty())
			{
				llwarns << "Invalid dictionary list entry: no name." << llendl;
				continue;
			}
			LLStringUtil::toLower(name);
			if (!mInstalledDicts.count(name))
			{
				language = entry["language"].asString();
				if (language.empty())
				{
					llwarns << "Invalid dictionary list entry. No language for: "
							<< name << llendl;
					language = name;
				}
				combo->add(language);
				found_one = true;
			}
		}
		if (!found_one)
		{
			combo->add(LLStringUtil::null);
		}

		bool visible = (sQueries == 0);
		childSetVisible("download_dict_combo", visible);
		childSetVisible("dict_download_button", visible);
		childSetVisible("downloading", !visible);

		bool usable = gInventory.isInventoryUsable();
		if (usable)
		{
			LLLineEditor* line =
				getChild<LLLineEditor>("upload_folder_animation");
			line->setText(getCategoryPath(LLFolderType::FT_ANIMATION));
			line = getChild<LLLineEditor>("upload_folder_sound");
			line->setText(getCategoryPath(LLFolderType::FT_SOUND));
			line = getChild<LLLineEditor>("upload_folder_texture");
			line->setText(getCategoryPath(LLFolderType::FT_TEXTURE));
			line = getChild<LLLineEditor>("upload_material_folder");
			line->setText(getCategoryPath(LLFolderType::FT_MATERIAL));
			line = getChild<LLLineEditor>("upload_folder_model");
			line->setText(getCategoryPath(LLFolderType::FT_OBJECT));
			line = getChild<LLLineEditor>("new_outfits_folder");
			line->setText(getCategoryPath(LLFolderType::FT_MY_OUTFITS));
		}
		childSetEnabled("reset_animation_folder", usable);
		childSetEnabled("reset_sound_folder", usable);
		childSetEnabled("reset_texture_folder", usable);
		childSetEnabled("reset_material_folder", usable);
		childSetEnabled("reset_model_folder", usable);
		childSetEnabled("reset_new_outfits_folder", usable);

		mIsDirty = false;
	}

	if (mWatchBlackListFloater && !HBFloaterBlacklistRLV::instanceVisible())
	{
		mWatchBlackListFloater = false;
		updateRestrainedLoveUserProfile();
	}

	LLPanel::draw();
}

void HBPrefsCoolImpl::refreshRestrainedLove(bool enable)
{
	// Enable/disable all children in the RestrainedLove panel
	LLPanel* panel = getChild<LLPanel>("RestrainedLove");
	LLView* child = panel->getFirstChild();
	while (child)
	{
		child->setEnabled(enable);
		child = panel->findNextSibling(child);
	}

	// RestrainedLove check box enabled only when logged in.
	childSetEnabled("restrained_love_check", LLStartUp::isLoggedIn());
}

void HBPrefsCoolImpl::refreshValues()
{
	bool logged_in = LLStartUp::isLoggedIn();

	// User Interface
	mShowChatButton						= gSavedSettings.getBool("ShowChatButton");
	mShowIMButton						= gSavedSettings.getBool("ShowIMButton");
	mShowFriendsButton					= gSavedSettings.getBool("ShowFriendsButton");
	mShowGroupsButton					= gSavedSettings.getBool("ShowGroupsButton");
	mShowFlyButton						= gSavedSettings.getBool("ShowFlyButton");
	mShowSnapshotButton					= gSavedSettings.getBool("ShowSnapshotButton");
	mShowSearchButton					= gSavedSettings.getBool("ShowSearchButton");
	mShowBuildButton					= gSavedSettings.getBool("ShowBuildButton");
	mShowRadarButton					= gSavedSettings.getBool("ShowRadarButton");
	mShowMiniMapButton					= gSavedSettings.getBool("ShowMiniMapButton");
	mShowMapButton						= gSavedSettings.getBool("ShowMapButton");
	mShowInventoryButton				= gSavedSettings.getBool("ShowInventoryButton");
	mHideMasterRemote					= gSavedSettings.getBool("HideMasterRemote");
	mFadeMouselookExitTip				= gSavedSettings.getU32("FadeMouselookExitTip");
	mUseOldChatHistory					= gSavedSettings.getBool("UseOldChatHistory");
	mAutoOpenTextInput					= gSavedSettings.getBool("AutoOpenTextInput");
	mIMTabsVerticalStacking				= gSavedSettings.getBool("IMTabsVerticalStacking");
	mUseOldStatusBarIcons				= gSavedSettings.getBool("UseOldStatusBarIcons");
	mUseOldTrackingDots					= gSavedSettings.getBool("UseOldTrackingDots");
	mDecimalsForTools					= gSavedSettings.getU32("DecimalsForTools");
	mHideTeleportProgress				= gSavedSettings.getBool("HideTeleportProgress");
	mStackMinimizedTopToBottom			= gSavedSettings.getBool("StackMinimizedTopToBottom");
	mStackMinimizedRightToLeft			= gSavedSettings.getBool("StackMinimizedRightToLeft");
	mStackScreenWidthFraction			= gSavedSettings.getU32("StackScreenWidthFraction");

	// Chat, IM & Text
	mAllowMUpose						= gSavedSettings.getBool("AllowMUpose");
	mAutoCloseOOC						= gSavedSettings.getBool("AutoCloseOOC");
	mHighlightOwnNameInChat				= gSavedSettings.getBool("HighlightOwnNameInChat");
	mHighlightOwnNameInIM				= gSavedSettings.getBool("HighlightOwnNameInIM");
	mOwnNameChatColor					= gSavedSettings.getColor4("OwnNameChatColor");
	if (logged_in)
	{
		mHighlightNicknames				= gSavedPerAccountSettings.getString("HighlightNicknames");
		mHighlightDisplayName			= gSavedPerAccountSettings.getBool("HighlightDisplayName");
	}
	mSpellCheck							= gSavedSettings.getBool("SpellCheck");
	mSpellCheckShow						= gSavedSettings.getBool("SpellCheckShow");
	mAddAvatarNamesToIgnore				= gSavedSettings.getBool("AddAvatarNamesToIgnore");
	mSpellCheckLanguage					= gSavedSettings.getString("SpellCheckLanguage");
	mTaskBarButtonFlashTime				= gSavedSettings.getF32("TaskBarButtonFlashTime");

	// Inventory
	mRezWithLandGroup					= gSavedSettings.getBool("RezWithLandGroup");
	mDoubleClickInventorySoundAction	= gSavedSettings.getU32("DoubleClickInventorySoundAction");
	mNoMultipleShoes					= gSavedSettings.getBool("NoMultipleShoes");
	mNoMultipleSkirts					= gSavedSettings.getBool("NoMultipleSkirts");
	mNoMultiplePhysics					= gSavedSettings.getBool("NoMultiplePhysics");
	mAutoShowInventoryThumbnails		= gSavedSettings.getBool("AutoShowInventoryThumbnails");
	mThumbnailViewTimeout				= gSavedSettings.getU32("ThumbnailViewTimeout");
	mOSUseCOF							= gSavedSettings.getBool("OSUseCOF");

	// Miscellaneous
	mAllowMultipleViewers				= gSavedSettings.getBool("AllowMultipleViewers");
	mBackgroundYieldTime				= gSavedSettings.getU32("BackgroundYieldTime");
	mFrameRateLimit						= gSavedSettings.getU32("FrameRateLimit");
	mSpeedRez							= gSavedSettings.getBool("SpeedRez");
	mSpeedRezInterval					= gSavedSettings.getU32("SpeedRezInterval");
	mNumImageDecodeThreads				= gSavedSettings.getU32("NumImageDecodeThreads");
	mFetchBoostAfterTPDuration			= gSavedSettings.getU32("TextureFetchBoostTimeAfterTP");
	mPreviewAnimInWorld					= gSavedSettings.getBool("PreviewAnimInWorld");
	mAppearanceAnimation				= gSavedSettings.getBool("AppearanceAnimation");
	mRevokePermsOnStandUp				= gSavedSettings.getBool("RevokePermsOnStandUp");
	mRevokePermsOnStopAnimation			= gSavedSettings.getBool("RevokePermsOnStopAnimation");
	mResetAnimOverrideOnStopAnimation	= gSavedSettings.getBool("ResetAnimOverrideOnStopAnimation");
	mTurnTowardsSelectedObject			= gSavedSettings.getBool("TurnTowardsSelectedObject");
	mTeleportHistoryDeparture			= gSavedSettings.getBool("TeleportHistoryDeparture");
	mMuteListIgnoreServer				= gSavedSettings.getBool("MuteListIgnoreServer");
	mSunPositionAtLogin					= gSavedSettings.getF32("SunPositionAtLogin");
	mRenderBanWallAlways				= gSavedSettings.getBool("RenderBanWallAlways");
	mRenderBanWallMaxDist				= gSavedSettings.getU32("RenderBanWallMaxDist");
	mExternalEditor						= gSavedSettings.getString("ExternalEditor");
	mOSAllowBakeOnMeshUploads			= gSavedSettings.getBool("OSAllowBakeOnMeshUploads");
	mOSWorldMapHasTerrain				= gSavedSettings.getBool("OSWorldMapHasTerrain");

	// RestrainedLove
	mRestrainedLove						= gSavedSettings.getBool("RestrainedLove");
	mRestrainedLoveBlacklist			= gSavedSettings.getString("RestrainedLoveBlacklist");
	mRestrainedLoveNoSetEnv				= gSavedSettings.getBool("RestrainedLoveNoSetEnv");
	mRestrainedLoveAllowWear			= gSavedSettings.getBool("RestrainedLoveAllowWear");
	mRestrainedLoveReattachDelay		= gSavedSettings.getU32("RestrainedLoveReattachDelay");
	mRestrainedLoveForbidGiveToRLV		= gSavedSettings.getBool("RestrainedLoveForbidGiveToRLV");
	mRestrainedLoveShowEllipsis			= gSavedSettings.getBool("RestrainedLoveShowEllipsis");
	mRestrainedLoveUntruncatedEmotes	= gSavedSettings.getBool("RestrainedLoveUntruncatedEmotes");
	mRestrainedLoveCanOoc				= gSavedSettings.getBool("RestrainedLoveCanOoc");
	if (logged_in)
	{
		mRestrainedLoveRecvimMessage	= gSavedPerAccountSettings.getString("RestrainedLoveRecvimMessage");
		mRestrainedLoveSendimMessage	= gSavedPerAccountSettings.getString("RestrainedLoveSendimMessage");
	}
}

void HBPrefsCoolImpl::updateRestrainedLoveUserProfile()
{
	std::string blacklist = gSavedSettings.getString("RestrainedLoveBlacklist");
	if (blacklist.empty())
	{
		mRestrainedLoveUserProfile = 0;
	}
	else if (blacklist == RLInterface::sRolePlayBlackList)
	{
		mRestrainedLoveUserProfile = 1;
	}
	else if (blacklist == RLInterface::sVanillaBlackList)
	{
		mRestrainedLoveUserProfile = 2;
	}
	else
	{
		mRestrainedLoveUserProfile = 3;
	}
	LLRadioGroup* radio = getChild<LLRadioGroup>("user_profile");
	radio->selectNthItem(mRestrainedLoveUserProfile);
}

void HBPrefsCoolImpl::setSunPositionLabel(F32 value)
{
	if (value < 0.f)
	{
		mSunPositionLabel->setText(mSunPositionSim);
	}
	else
	{
		F32 time = 24.f * value;
		U32 hours = (U32)time;
		U32 minutes = (U32)((time - (F32)hours) * 60.f);
		mSunPositionLabel->setText(llformat("%02d:%02d", hours, minutes));
	}
}

void HBPrefsCoolImpl::refresh()
{
	refreshValues();

	// User Interface

	std::string format = gSavedSettings.getString("ShortTimeFormat");
	if (format.find("%p") == std::string::npos)
	{
		mTimeFormat = 0;
	}
	else
	{
		mTimeFormat = 1;
	}

	format = gSavedSettings.getString("ShortDateFormat");
	if (format.find("%m/%d/%") != std::string::npos)
	{
		mDateFormat = 2;
	}
	else if (format.find("%d/%m/%") != std::string::npos)
	{
		mDateFormat = 1;
	}
	else
	{
		mDateFormat = 0;
	}

	// time format combobox
	LLComboBox* combo = getChild<LLComboBox>("time_format_combobox");
	if (combo)
	{
		combo->setCurrentByIndex(mTimeFormat);
	}

	// date format combobox
	combo = getChild<LLComboBox>("date_format_combobox");
	if (combo)
	{
		combo->setCurrentByIndex(mDateFormat);
	}

	bool logged_in = LLStartUp::isLoggedIn();
	if (logged_in)
	{
		childSetValue("highlight_nicknames_text", mHighlightNicknames);
		childSetValue("highlight_display_name_check", mHighlightDisplayName);
	}
	else
	{
		childDisable("highlight_nicknames_text");
		childDisable("highlight_display_name_check");
	}

	// Spell checking
	childSetEnabled("spell_check_show_check", mSpellCheck);
	childSetEnabled("add_avatar_names_to_ignore_check", mSpellCheck);
	childSetEnabled("installed_dicts_combo", mSpellCheck);
	childSetEnabled("download_dict_combo", mSpellCheck);
	childSetEnabled("dict_download_button", mSpellCheck);

	// Miscellaneous
	childSetEnabled("speed_rez_interval", mSpeedRez);
	childSetEnabled("speed_rez_seconds", mSpeedRez);
	setSunPositionLabel(mSunPositionAtLogin);

	// RestrainedLove
	refreshRestrainedLove(mRestrainedLove);
	updateRestrainedLoveUserProfile();
	if (logged_in)
	{
		LLWString message = utf8str_to_wstring(gSavedPerAccountSettings.getString("RestrainedLoveRecvimMessage"));
		LLWStringUtil::replaceChar(message, '^', '\n');
		childSetText("receive_im_message_editor", wstring_to_utf8str(message));

		message = utf8str_to_wstring(gSavedPerAccountSettings.getString("RestrainedLoveSendimMessage"));
		LLWStringUtil::replaceChar(message, '^', '\n');
		childSetText("send_im_message_editor", wstring_to_utf8str(message));
	}
	else
	{
		std::string text = getString("when_logged_in");
		childSetText("receive_im_message_editor", text);
		childDisable("receive_im_message_editor");
		childSetText("send_im_message_editor", text);
		childDisable("send_im_message_editor");
	}
}

void HBPrefsCoolImpl::cancel()
{
	bool logged_in = LLStartUp::isLoggedIn();

	// User Interface
	gSavedSettings.setBool("ShowChatButton",					mShowChatButton);
	gSavedSettings.setBool("ShowIMButton",						mShowIMButton);
	gSavedSettings.setBool("ShowFriendsButton",					mShowFriendsButton);
	gSavedSettings.setBool("ShowGroupsButton",					mShowGroupsButton);
	gSavedSettings.setBool("ShowFlyButton",						mShowFlyButton);
	gSavedSettings.setBool("ShowSnapshotButton",				mShowSnapshotButton);
	gSavedSettings.setBool("ShowSearchButton",					mShowSearchButton);
	gSavedSettings.setBool("ShowBuildButton",					mShowBuildButton);
	gSavedSettings.setBool("ShowRadarButton",					mShowRadarButton);
	gSavedSettings.setBool("ShowMiniMapButton",					mShowMiniMapButton);
	gSavedSettings.setBool("ShowMapButton",						mShowMapButton);
	gSavedSettings.setBool("ShowInventoryButton",				mShowInventoryButton);
	gSavedSettings.setBool("HideMasterRemote",					mHideMasterRemote);
	gSavedSettings.setU32("FadeMouselookExitTip",				mFadeMouselookExitTip);
	gSavedSettings.setBool("UseOldChatHistory",					mUseOldChatHistory);
	gSavedSettings.setBool("AutoOpenTextInput",					mAutoOpenTextInput);
	gSavedSettings.setBool("IMTabsVerticalStacking",			mIMTabsVerticalStacking);
	gSavedSettings.setBool("UseOldStatusBarIcons",				mUseOldStatusBarIcons);
	gSavedSettings.setBool("UseOldTrackingDots",				mUseOldTrackingDots);
	gSavedSettings.setU32("DecimalsForTools",					mDecimalsForTools);
	gSavedSettings.setBool("HideTeleportProgress",				mHideTeleportProgress);
	gSavedSettings.setBool("StackMinimizedTopToBottom",			mStackMinimizedTopToBottom);
	gSavedSettings.setBool("StackMinimizedRightToLeft",			mStackMinimizedRightToLeft);
	gSavedSettings.setU32("StackScreenWidthFraction",			mStackScreenWidthFraction);

	// Chat, IM & Text
	gSavedSettings.setBool("AllowMUpose",						mAllowMUpose);
	gSavedSettings.setBool("AutoCloseOOC",						mAutoCloseOOC);
	gSavedSettings.setBool("HighlightOwnNameInChat",			mHighlightOwnNameInChat);
	gSavedSettings.setBool("HighlightOwnNameInIM",				mHighlightOwnNameInIM);
	gSavedSettings.setColor4("OwnNameChatColor",				mOwnNameChatColor);
	if (logged_in)
	{
		gSavedPerAccountSettings.setString("HighlightNicknames", mHighlightNicknames);
		gSavedPerAccountSettings.setBool("HighlightDisplayName", mHighlightDisplayName);
	}
	gSavedSettings.setBool("SpellCheck",						mSpellCheck);
	gSavedSettings.setBool("SpellCheckShow",					mSpellCheckShow);
	gSavedSettings.setBool("AddAvatarNamesToIgnore",			mAddAvatarNamesToIgnore);
	gSavedSettings.setString("SpellCheckLanguage",				mSpellCheckLanguage);
	gSavedSettings.setF32("TaskBarButtonFlashTime",				mTaskBarButtonFlashTime);

	// Inventory
	gSavedSettings.setBool("RezWithLandGroup",					mRezWithLandGroup);
	gSavedSettings.setU32("DoubleClickInventorySoundAction",	mDoubleClickInventorySoundAction);
	gSavedSettings.setBool("NoMultipleShoes",					mNoMultipleShoes);
	gSavedSettings.setBool("NoMultipleSkirts",					mNoMultipleSkirts);
	gSavedSettings.setBool("NoMultiplePhysics",					mNoMultiplePhysics);
	gSavedSettings.setBool("AutoShowInventoryThumbnails",		mAutoShowInventoryThumbnails);
	gSavedSettings.setU32("ThumbnailViewTimeout",				mThumbnailViewTimeout);
	gSavedSettings.setBool("OSUseCOF",							mOSUseCOF);

	// Miscellaneous
	gSavedSettings.setBool("AllowMultipleViewers",				mAllowMultipleViewers);
	gSavedSettings.setU32("BackgroundYieldTime",				mBackgroundYieldTime);
	gSavedSettings.setU32("FrameRateLimit",						mFrameRateLimit);
	gSavedSettings.setBool("SpeedRez",							mSpeedRez);
	gSavedSettings.setU32("SpeedRezInterval",					mSpeedRezInterval);
	gSavedSettings.setU32("NumImageDecodeThreads",				mNumImageDecodeThreads);
	gSavedSettings.setU32("TextureFetchBoostTimeAfterTP",		mFetchBoostAfterTPDuration);
	gSavedSettings.setBool("PreviewAnimInWorld",				mPreviewAnimInWorld);
	gSavedSettings.setBool("AppearanceAnimation",				mAppearanceAnimation);
	gSavedSettings.setBool("RevokePermsOnStandUp",				mRevokePermsOnStandUp);
	gSavedSettings.setBool("RevokePermsOnStopAnimation",		mRevokePermsOnStopAnimation);
	gSavedSettings.setBool("ResetAnimOverrideOnStopAnimation",	mResetAnimOverrideOnStopAnimation);
	gSavedSettings.setBool("TurnTowardsSelectedObject",			mTurnTowardsSelectedObject);
	gSavedSettings.setBool("TeleportHistoryDeparture",			mTeleportHistoryDeparture);
	gSavedSettings.setBool("MuteListIgnoreServer",				mMuteListIgnoreServer);
	gSavedSettings.setF32("SunPositionAtLogin",					mSunPositionAtLogin);
	gSavedSettings.setBool("RenderBanWallAlways",				mRenderBanWallAlways);
	gSavedSettings.setU32("RenderBanWallMaxDist",				mRenderBanWallMaxDist);
	gSavedSettings.setString("ExternalEditor",					mExternalEditor);
	gSavedSettings.setBool("OSAllowBakeOnMeshUploads",			mOSAllowBakeOnMeshUploads);
	gSavedSettings.setBool("OSWorldMapHasTerrain",				mOSWorldMapHasTerrain);

	// RestrainedLove
	gSavedSettings.setBool("RestrainedLove",					mRestrainedLove);
	gSavedSettings.setString("RestrainedLoveBlacklist",			mRestrainedLoveBlacklist);
	gSavedSettings.setBool("RestrainedLoveNoSetEnv",			mRestrainedLoveNoSetEnv);
	gSavedSettings.setBool("RestrainedLoveAllowWear",			mRestrainedLoveAllowWear);
	gSavedSettings.setU32("RestrainedLoveReattachDelay",		mRestrainedLoveReattachDelay);
	gSavedSettings.setBool("RestrainedLoveForbidGiveToRLV",		mRestrainedLoveForbidGiveToRLV);
	gSavedSettings.setBool("RestrainedLoveShowEllipsis",		mRestrainedLoveShowEllipsis);
	gSavedSettings.setBool("RestrainedLoveUntruncatedEmotes",	mRestrainedLoveUntruncatedEmotes);
	gSavedSettings.setBool("RestrainedLoveCanOoc",				mRestrainedLoveCanOoc);
	if (logged_in)
	{
		gSavedPerAccountSettings.setString("RestrainedLoveRecvimMessage",
										   mRestrainedLoveRecvimMessage);
		gSavedPerAccountSettings.setString("RestrainedLoveSendimMessage",
										   mRestrainedLoveSendimMessage);
	}
}

void HBPrefsCoolImpl::apply()
{
	// User Interface

	std::string short_date, long_date, short_time, long_time, timestamp;

	LLComboBox* combo = getChild<LLComboBox>("time_format_combobox");
	if (combo)
	{
		mTimeFormat = combo->getCurrentIndex();
	}

	combo = getChild<LLComboBox>("date_format_combobox");
	if (combo)
	{
		mDateFormat = combo->getCurrentIndex();
	}

	if (mTimeFormat == 0)
	{
		short_time = "%H:%M";
		long_time  = "%H:%M:%S";
		timestamp  = " %H:%M:%S";
	}
	else
	{
		short_time = "%I:%M %p";
		long_time  = "%I:%M:%S %p";
		timestamp  = " %I:%M %p";
	}

	if (mDateFormat == 0)
	{
		short_date = "%Y-%m-%d";
		long_date  = "%A %d %B %Y";
		timestamp  = "%a %d %b %Y" + timestamp;
	}
	else if (mDateFormat == 1)
	{
		short_date = "%d/%m/%Y";
		long_date  = "%A %d %B %Y";
		timestamp  = "%a %d %b %Y" + timestamp;
	}
	else
	{
		short_date = "%m/%d/%Y";
		long_date  = "%A, %B %d %Y";
		timestamp  = "%a %b %d %Y" + timestamp;
	}

	gSavedSettings.setString("ShortDateFormat",	short_date);
	gSavedSettings.setString("LongDateFormat",	long_date);
	gSavedSettings.setString("ShortTimeFormat",	short_time);
	gSavedSettings.setString("LongTimeFormat",	long_time);
	gSavedSettings.setString("TimestampFormat",	timestamp);

	// Chat, IM & Text
	combo = getChild<LLComboBox>("installed_dicts_combo");
	std::string dict_name = getDictName(combo->getSelectedItemLabel());
	if (!dict_name.empty())
	{
		gSavedSettings.setString("SpellCheckLanguage", dict_name);
	}

	if (LLStartUp::isLoggedIn())
	{
		gSavedPerAccountSettings.setString("HighlightNicknames",
										   childGetValue("highlight_nicknames_text"));
		gSavedPerAccountSettings.setBool("HighlightDisplayName",
										 childGetValue("highlight_display_name_check"));

		// RestrainedLove

		LLTextEditor* text = getChild<LLTextEditor>("receive_im_message_editor");
		LLWString message = text->getWText();
		LLWStringUtil::replaceTabsWithSpaces(message, 4);
		LLWStringUtil::replaceChar(message, '\n', '^');
		gSavedPerAccountSettings.setString("RestrainedLoveRecvimMessage",
										   std::string(wstring_to_utf8str(message)));

		text = getChild<LLTextEditor>("send_im_message_editor");
		message = text->getWText();
		LLWStringUtil::replaceTabsWithSpaces(message, 4);
		LLWStringUtil::replaceChar(message, '\n', '^');
		gSavedPerAccountSettings.setString("RestrainedLoveSendimMessage",
										   std::string(wstring_to_utf8str(message)));
	}

	refreshValues();
}

std::string HBPrefsCoolImpl::getDictName(const std::string& language)
{
	std::string result;
	for (LLSD::array_const_iterator it = mDictsList.beginArray();
		 it != mDictsList.endArray(); it++)
	{
		const LLSD& entry = *it;
		if (entry["language"].asString() == language)
		{
			result = entry["name"].asString();
			break;
		}
	}
	return result;
}

std::string HBPrefsCoolImpl::getDictLanguage(const std::string& name)
{
	std::string result;
	for (LLSD::array_const_iterator it = mDictsList.beginArray();
		 it != mDictsList.endArray(); it++)
	{
		const LLSD& entry = *it;
		if (entry["name"].asString() == name)
		{
			result = entry["language"].asString();
			break;
		}
	}
	return result;
}

std::string HBPrefsCoolImpl::getCategoryPath(const LLUUID& cat_id)
{
	LLViewerInventoryCategory* cat = gInventory.getCategory(cat_id);
	if (!cat)
	{
		llwarns_once << "Could not find category for Id: " << cat_id << llendl;
		return "";
	}

	const LLUUID& parent_id = cat->getParentUUID();
	if (parent_id.notNull())
	{
		// Should be " \u25ba ", buy M$' Visual Shit does not accept it...
		static const std::string separator(" \xE2\x96\xBA ");
		return getCategoryPath(parent_id) + separator + cat->getName();
	}

	return cat->getName();
}

std::string HBPrefsCoolImpl::getCategoryPath(LLFolderType::EType cat_type)
{
	LLUUID cat_id = gInventory.findChoosenCategoryUUIDForType(cat_type);
	return getCategoryPath(cat_id);
}

//static
void HBPrefsCoolImpl::onTabChanged(void* user_data, bool)
{
	HBPrefsCoolImpl* self = (HBPrefsCoolImpl*)user_data;
	if (self && self->mTabContainer)
	{
		gSavedSettings.setS32("LastCoolPrefTab",
							  self->mTabContainer->getCurrentPanelIndex());
	}
}

//static
void HBPrefsCoolImpl::setQueryActive(bool active)
{
	if (active)
	{
		++sQueries;
	}
	else
	{
		if (--sQueries < 0)
		{
			llwarns << "Lost the count of the dictionary download queries !"
					<< llendl;
			sQueries = 0;
		}
		if (sQueries == 0)
		{
			gNotifications.add("DownloadDictFinished");
		}
	}
	if (HBPrefsCoolImpl::getInstance())
	{
		HBPrefsCoolImpl::getInstance()->setDirty();
	}
}

//static
void HBPrefsCoolImpl::onCommitCheckBoxAfterRestart(LLUICtrl* ctrl,
												   void* user_data)
{
	HBPrefsCoolImpl* self = (HBPrefsCoolImpl*)user_data;
	LLCheckBoxCtrl* check = (LLCheckBoxCtrl*)ctrl;
	if (!self || !check) return;

	std::string control = check->getControlName();

	if (!self->mWarnedAfterRestart.count(control))
	{
		self->mWarnedAfterRestart.emplace(control);
		gNotifications.add("InEffectAfterRestart");
	}
}

//static
void HBPrefsCoolImpl::onCommitCheckBoxShowButton(LLUICtrl* ctrl,
												 void* user_data)
{
	HBPrefsCoolImpl* self = (HBPrefsCoolImpl*)user_data;
	LLCheckBoxCtrl* check = (LLCheckBoxCtrl*)ctrl;
	if (!self || !check) return;

	bool enabled = check->get();
	if (enabled && !gSavedSettings.getBool("ShowToolBar"))
	{
		gSavedSettings.setBool("ShowToolBar", true);
	}
}

//static
void HBPrefsCoolImpl::onCommitCheckBoxSpellCheck(LLUICtrl* ctrl,
												 void* user_data)
{
	HBPrefsCoolImpl* self = (HBPrefsCoolImpl*)user_data;
	LLCheckBoxCtrl* check = (LLCheckBoxCtrl*)ctrl;
	if (!self || !check) return;

	bool enabled = check->get();
	self->childSetEnabled("spell_check_show_check", enabled);
	self->childSetEnabled("add_avatar_names_to_ignore_check", enabled);
	self->childSetEnabled("installed_dicts_combo", enabled);
	self->childSetEnabled("download_dict_combo", enabled);
	self->childSetEnabled("dict_download_button", enabled);
}

//static
void HBPrefsCoolImpl::onClickDownloadDict(void* user_data)
{
	HBPrefsCoolImpl* self = (HBPrefsCoolImpl*)user_data;
	if (self)
	{
		LLComboBox* combo = self->getChild<LLComboBox>("download_dict_combo");
		if (combo)
		{
			std::string label = combo->getSelectedItemLabel();
			if (!label.empty())
			{
				std::string dict_name = self->getDictName(label);
				gCoros.launch("HBPrefsCoolImpl::fetchDictionaryCoro(aff)",
							  boost::bind(&HBPrefsCoolImpl::fetchDictionaryCoro,
										  dict_name + ".aff"));
				gCoros.launch("HBPrefsCoolImpl::fetchDictionaryCoro(dic)",
							  boost::bind(&HBPrefsCoolImpl::fetchDictionaryCoro,
										  dict_name + ".dic"));
			}
		}
	}
}

//static
void HBPrefsCoolImpl::fetchDictionaryCoro(std::string dictname)
{
	std::string url = gSavedSettings.getString("SpellCheckDictDownloadURL");
	url += dictname;

	setQueryActive(true);
	llinfos << "Fetching dictionary file from: " << url << llendl;

	LLCoreHttpUtil::HttpCoroutineAdapter adapter("fetchDictionaryCoro");
	LLSD result = adapter.getRawAndSuspend(url);
	setQueryActive(false);

	LLSD args;
	args["NAME"] = dictname;

	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	if (!status)
	{
		args["STATUS"] = llformat("%d", status.getType());
		args["REASON"] = status.toString();
		gNotifications.add("DownloadDictFailure", args);
		return;
	}

	const LLSD::Binary& raw =
		result[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS_RAW].asBinary();
	S32 size = raw.size();
	if (size <= 0)
	{
		gNotifications.add("DownloadDictEmpty", args);
		return;
	}

	std::string filename;
	filename = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS,
											  "dictionaries",
											  dictname.c_str());
	LLFile output(filename, "wb");
	if (!output || output.write(raw.data(), size) != size)
	{
		gNotifications.add("DictWriteFailure", args);
	}
}

//static
void HBPrefsCoolImpl::onCommitCheckBoxSpeedRez(LLUICtrl* ctrl, void* user_data)
{
	HBPrefsCoolImpl* self = (HBPrefsCoolImpl*)user_data;
	LLCheckBoxCtrl* check = (LLCheckBoxCtrl*)ctrl;
	if (!self || !check) return;

	bool enabled = check->get();
	self->childSetEnabled("speed_rez_interval", enabled);
	self->childSetEnabled("speed_rez_seconds", enabled);
}

//static
void HBPrefsCoolImpl::onCommitSliderSunPositionAtLogin(LLUICtrl* ctrl,
													   void* user_data)
{
	HBPrefsCoolImpl* self = (HBPrefsCoolImpl*)user_data;
	LLSliderCtrl* slider = (LLSliderCtrl*)ctrl;
	if (!self || !slider) return;

	F32 value = slider->getValueF32();

	self->setSunPositionLabel(value);

	if (!LLStartUp::isLoggedIn())
	{
		return;
	}

	gSavedSettings.setBool("UseParcelEnvironment", false);
	gEnvironment.setLocalEnvFromDefaultWindlightDay(value);
}

//static
void HBPrefsCoolImpl::onCommitCheckBoxRestrainedLove(LLUICtrl* ctrl,
													 void* user_data)
{
	HBPrefsCoolImpl* self = (HBPrefsCoolImpl*)user_data;
	LLCheckBoxCtrl* check = (LLCheckBoxCtrl*)ctrl;
	if (!self || !check) return;

	bool enable = check->get();
	self->refreshRestrainedLove(enable);
	if ((bool)self->mRestrainedLove != enable)
	{
		gNotifications.add("InEffectAfterRestart");
	}
}

//static
void HBPrefsCoolImpl::onCommitUserProfile(LLUICtrl* ctrl, void* user_data)
{
	HBPrefsCoolImpl* self = (HBPrefsCoolImpl*)user_data;
	LLRadioGroup* radio = (LLRadioGroup*)ctrl;
	if (!self || !radio) return;

	std::string blacklist;
	S32 profile = radio->getSelectedIndex();
	switch (profile)
	{
		case 0:
			blacklist.clear();
			break;

		case 1:
			blacklist = RLInterface::sRolePlayBlackList;
			break;

		case 2:
			blacklist = RLInterface::sVanillaBlackList;
			break;

		default:
			blacklist = gSavedSettings.getString("RestrainedLoveBlacklist");
	}
	gSavedSettings.setString("RestrainedLoveBlacklist",	blacklist);

	if (self->mRestrainedLoveUserProfile != profile)
	{
		gNotifications.add("InEffectAfterRestart");
	}
	self->mRestrainedLoveUserProfile = profile;
}

//static
void HBPrefsCoolImpl::onClickCustomBlackList(void* user_data)
{
	HBPrefsCoolImpl* self = (HBPrefsCoolImpl*)user_data;
	if (self)
	{
		HBFloaterBlacklistRLV::showInstance();
		self->mWatchBlackListFloater = true;
	}
}

//static
void HBPrefsCoolImpl::onClickResetSetting(void* user_data)
{
	LLControlVariable* control = (LLControlVariable*)user_data;
	if (control)
	{
		control->resetToDefault(true);
	}
}

//---------------------------------------------------------------------------

HBPrefsCool::HBPrefsCool()
:	impl(*new HBPrefsCoolImpl())
{
}

HBPrefsCool::~HBPrefsCool()
{
	delete &impl;
}

void HBPrefsCool::apply()
{
	HBFloaterBlacklistRLV::hideInstance();	// Actually a closing
	impl.apply();
}

void HBPrefsCool::cancel()
{
	HBFloaterBlacklistRLV::hideInstance();	// Actually a closing
	impl.cancel();
}

LLPanel* HBPrefsCool::getPanel()
{
	return &impl;
}
