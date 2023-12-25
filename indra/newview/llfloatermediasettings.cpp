/**
 * @file llfloatermediasettings.cpp
 * @brief Media settings floater - class implementation
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

#include "llviewerprecompiledheaders.h"

#include "llfloatermediasettings.h"

#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "lllineeditor.h"
#include "llmediaentry.h"
#include "llnameeditor.h"
#include "llscrolllistctrl.h"
#include "llsdserialize.h"
#include "llsdutil.h"
#include "llspinctrl.h"
#include "lltabcontainer.h"
#include "lltextbox.h"
#include "lltextureentry.h"
#include "lluictrl.h"
#include "lluictrlfactory.h"

#include "llappviewer.h"		// For gFrameTimeSeconds
#include "llfloatertools.h"
#include "llmediactrl.h"
#include "llselectmgr.h"
#include "llviewercontrol.h"
#include "llviewermedia.h"
#include "llvovolume.h"

bool LLFloaterMediaSettings::sIdenticalHasMediaInfo = true;
bool LLFloaterMediaSettings::sMultipleMedia = false;
bool LLFloaterMediaSettings::sMultipleValidMedia = false;

const char* CHECKERBOARD_DATA_URL =
	"data:image/svg+xml,%3Csvg xmlns=%22http://www.w3.org/2000/svg%22 width=%22100%%22 height=%22100%%22 %3E%3Cdefs%3E%3Cpattern id=%22checker%22 patternUnits=%22userSpaceOnUse%22 x=%220%22 y=%220%22 width=%22128%22 height=%22128%22 viewBox=%220 0 128 128%22 %3E%3Crect x=%220%22 y=%220%22 width=%2264%22 height=%2264%22 fill=%22#ddddff%22 /%3E%3Crect x=%2264%22 y=%2264%22 width=%2264%22 height=%2264%22 fill=%22#ddddff%22 /%3E%3C/pattern%3E%3C/defs%3E%3Crect x=%220%22 y=%220%22 width=%22100%%22 height=%22100%%22 fill=%22url(#checker)%22 /%3E%3C/svg%3E";

LLFloaterMediaSettings::LLFloaterMediaSettings(const LLSD&)
:	mFirstRun(true),
	mGroupId(LLUUID::null),
	mMediaEditable(false),
	mHomeUrlCommitted(false),
	mTabContainer(NULL),
	mOKBtn(NULL),
	mCancelBtn(NULL),
	mApplyBtn(NULL),
	mResetCurrentUrlBtn(NULL),
	mAutoLoop(NULL),
	mFirstClick(NULL),
	mAutoZoom(NULL),
	mAutoPlay(NULL),
	mAutoScale(NULL),
	mWidthPixels(NULL),
	mHeightPixels(NULL),
	mHomeURL(NULL),
	mControls(NULL),
    mPermsOwnerInteract(NULL),
    mPermsOwnerControl(NULL),
	mPermsGroupName(NULL),
    mPermsGroupInteract(NULL),
    mPermsGroupControl(NULL),
    mPermsWorldInteract(NULL),
    mPermsWorldControl(NULL),
	mCurrentUrlLabel(NULL),
	mCurrentURL(NULL),
	mEnableWhiteList(NULL),
	mNewWhiteListPattern(NULL),
	mWhiteListList(NULL),
	mHomeUrlFailsWhiteListText(NULL),
	mDeleteBtn(NULL)
{
	sIdenticalHasMediaInfo = true;
	sMultipleMedia = false;
	sMultipleValidMedia = false;

	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_media_settings.xml",
												 NULL, false);	// do not open
}

//virtual
bool LLFloaterMediaSettings::postBuild()
{
	mTabContainer = getChild<LLTabContainer>("tabs");
	LLPanel* tab = mTabContainer->getChild<LLPanel>("media_settings_general");
	mTabContainer->setTabChangeCallback(tab, onTabChanged);
	mTabContainer->setTabUserData(tab, this);
	tab = mTabContainer->getChild<LLPanel>("media_settings_permissions");
	mTabContainer->setTabChangeCallback(tab, onTabChanged);
	mTabContainer->setTabUserData(tab, this);
	tab = mTabContainer->getChild<LLPanel>("media_settings_security");
	mTabContainer->setTabChangeCallback(tab, onTabChanged);
	mTabContainer->setTabUserData(tab, this);

	mApplyBtn = getChild<LLButton>("Apply");
	mApplyBtn->setClickedCallback(onBtnApply, this);

	mCancelBtn = getChild<LLButton>("Cancel");
	mCancelBtn->setClickedCallback(onBtnCancel, this);

	mOKBtn = getChild<LLButton>("OK");
	mOKBtn->setClickedCallback(onBtnOK, this);

	// General panel UI elements:

	mCurrentUrlLabel = getChild<LLTextBox>("current_url_label");

	mCurrentURL = getChild<LLTextBox>(LLMediaEntry::CURRENT_URL_KEY);

	mAutoLoop = getChild<LLCheckBoxCtrl>(LLMediaEntry::AUTO_LOOP_KEY);

	mAutoPlay = getChild<LLCheckBoxCtrl>(LLMediaEntry::AUTO_PLAY_KEY);

	mAutoScale = getChild<LLCheckBoxCtrl>(LLMediaEntry::AUTO_SCALE_KEY);

	mAutoZoom = getChild<LLCheckBoxCtrl>(LLMediaEntry::AUTO_ZOOM_KEY);

	mFirstClick =
		getChild<LLCheckBoxCtrl>(LLMediaEntry::FIRST_CLICK_INTERACT_KEY);

	mHeightPixels = getChild<LLSpinCtrl>(LLMediaEntry::HEIGHT_PIXELS_KEY);

	mHomeURL = getChild<LLLineEditor>(LLMediaEntry::HOME_URL_KEY);
	mHomeURL->setCommitCallback(onCommitHomeURL);
	mHomeURL->setCallbackUserData(this);

	mWidthPixels = getChild<LLSpinCtrl>(LLMediaEntry::WIDTH_PIXELS_KEY);

	mPreviewMedia = getChild<LLMediaCtrl>("preview_media");

	mFailWhiteListText = getChild<LLTextBox>("home_fails_whitelist_label");

	mResetCurrentUrlBtn = getChild<LLButton>("current_url_reset_btn");
	mResetCurrentUrlBtn->setClickedCallback(onBtnResetCurrentUrl, this);

	// interrogates controls and updates widgets as required
	updateMediaPreview();

	// Permissions tab UI elements:

	mControls = getChild<LLComboBox>(LLMediaEntry::CONTROLS_KEY);

    mPermsOwnerInteract =
		getChild<LLCheckBoxCtrl>(LLMediaEntry::PERMS_OWNER_INTERACT_KEY);

    mPermsOwnerControl =
		getChild<LLCheckBoxCtrl>(LLMediaEntry::PERMS_OWNER_CONTROL_KEY);

    mPermsGroupInteract =
		getChild<LLCheckBoxCtrl>(LLMediaEntry::PERMS_GROUP_INTERACT_KEY);

    mPermsGroupControl =
		getChild<LLCheckBoxCtrl>(LLMediaEntry::PERMS_GROUP_CONTROL_KEY);

    mPermsWorldInteract =
		getChild<LLCheckBoxCtrl>(LLMediaEntry::PERMS_ANYONE_INTERACT_KEY);

    mPermsWorldControl =
		getChild<LLCheckBoxCtrl>(LLMediaEntry::PERMS_ANYONE_CONTROL_KEY);

	mPermsGroupName = getChild<LLNameEditor>("perms_group_name");

	// Security tab UI elements:

	mEnableWhiteList =
		getChild<LLCheckBoxCtrl>(LLMediaEntry::WHITELIST_ENABLE_KEY);

	mNewWhiteListPattern = getChild<LLLineEditor>("new_pattern");
	mNewWhiteListPattern->setCommitCallback(onCommitNewPattern);
	mNewWhiteListPattern->setCallbackUserData(this);

	mWhiteListList = getChild<LLScrollListCtrl>(LLMediaEntry::WHITELIST_KEY);

	mHomeUrlFailsWhiteListText =
		getChild<LLTextBox>("home_url_fails_whitelist");

	mDeleteBtn = getChild<LLButton>("whitelist_del");
	mDeleteBtn->setClickedCallback(onBtnDel, this);

	return true;
}

//virtual
void LLFloaterMediaSettings::close(bool app_quitting)
{
	if (app_quitting || !LLFloaterTools::isVisible())
	{
		LLFloater::close(app_quitting);
	}
	else
	{
		setVisible(false);
	}
}

//virtual
void LLFloaterMediaSettings::draw()
{
	// Tab selection is delayed here because it would not work in postBuild()
	if (mFirstRun)
	{
		mFirstRun = false;
		mTabContainer->selectTab(gSavedSettings.getS32("LastMediaSettingsTab"));
	}

	// Do not perform the following operations every frame because they are
	// time consuming and do not change often.
	static F32 last_update = 0.f;
	if (gFrameTimeSeconds - last_update >= 0.25f)
	{
		// Floater:
		mApplyBtn->setEnabled(mMediaEditable && haveValuesChanged());

		// General tab:
		checkHomeUrlPassesWhitelist();
		updateCurrentUrl();

		// Enable/disable pixel values image entry based on auto scale checkbox
		bool custom_scale = !mAutoScale->getValue().asBoolean();
		mWidthPixels->setEnabled(custom_scale);
		mHeightPixels->setEnabled(custom_scale);

		// Enable/disable UI based on type of media
		bool reset_button_is_active = true;
		LLPluginClassMedia* media_plugin = mPreviewMedia->getMediaPlugin();
		if (media_plugin)
		{
			// Turn off volume (if we can) for preview.
			media_plugin->setVolume(0.f);

			// Some controls are only appropriate for time or browser type
			// plugins so we selectively enable/disable them; we need to do it
			// in draw because the information from plugins arrives
			// assynchronously
			if (media_plugin->pluginSupportsMediaTime())
			{
				reset_button_is_active = false;
				mCurrentURL->setEnabled(false);
				mCurrentUrlLabel->setEnabled(false);
				mAutoLoop->setEnabled(true);
			}
			else
			{
				reset_button_is_active = true;
				mCurrentURL->setEnabled(true);
				mCurrentUrlLabel->setEnabled(true);
				mAutoLoop->setEnabled(false);
			}
		}

		// Several places modify this widget so we must collect states in one
		// place
		if (reset_button_is_active)
		{
			// User has perms to press reset button and it is active
			mResetCurrentUrlBtn->setEnabled(mMediaEditable);
		}
		else
		{
			// Reset button is inactive so we just slam it to off
			mResetCurrentUrlBtn->setEnabled(false);
		}

		// Permissions tab:
		LLUUID group_id;
		bool groups_identical = gSelectMgr.selectGetGroup(group_id);
		if (group_id != mGroupId)
		{
			if (groups_identical)
			{
				mPermsGroupName->setNameID(group_id, true);
			}
			else
			{
				mPermsGroupName->setNameID(LLUUID::null, true);
				mPermsGroupName->refresh(LLUUID::null, "", true);
			}
			mGroupId = group_id;
		}

		last_update = gFrameTimeSeconds;
	}

	LLFloater::draw();
}

void LLFloaterMediaSettings::getValues(LLSD& fill_me_in,
									   bool include_tentative)
{
	// General tab settings:

	if (include_tentative || !mAutoLoop->getTentative())
	{
		fill_me_in[LLMediaEntry::AUTO_LOOP_KEY] =
			(LLSD::Boolean)mAutoLoop->getValue();
	}
	if (include_tentative || !mAutoPlay->getTentative())
	{
		fill_me_in[LLMediaEntry::AUTO_PLAY_KEY] =
			(LLSD::Boolean)mAutoPlay->getValue();
	}
	if (include_tentative || !mAutoScale->getTentative())
	{
		fill_me_in[LLMediaEntry::AUTO_SCALE_KEY] =
			(LLSD::Boolean)mAutoScale->getValue();
	}
	if (include_tentative || !mAutoZoom->getTentative())
	{
		fill_me_in[LLMediaEntry::AUTO_ZOOM_KEY] =
			(LLSD::Boolean)mAutoZoom->getValue();
	}
#if 0	// Do not fill in current URL: this is only supposed to get changed via
		// navigate
	if (include_tentative || !mCurrentURL->getTentative())
	{
		fill_me_in[LLMediaEntry::CURRENT_URL_KEY] = mCurrentURL->getValue();
	}
#endif
	if (include_tentative || !mHeightPixels->getTentative())
	{
		fill_me_in[LLMediaEntry::HEIGHT_PIXELS_KEY] =
			(LLSD::Integer)mHeightPixels->getValue();
	}
	// Do not fill in the home URL if it is the special "Multiple Media" string
	if ((include_tentative || mHomeUrlCommitted) &&
		mHomeURL->getValue().asString() != "Multiple Media")
	{
		fill_me_in[LLMediaEntry::HOME_URL_KEY] =
			(LLSD::String)mHomeURL->getValue();
	}
	if (include_tentative || !mFirstClick->getTentative())
	{
		fill_me_in[LLMediaEntry::FIRST_CLICK_INTERACT_KEY] =
			(LLSD::Boolean)mFirstClick->getValue();
	}
	if (include_tentative || !mWidthPixels->getTentative())
	{
		fill_me_in[LLMediaEntry::WIDTH_PIXELS_KEY] =
			(LLSD::Integer)mWidthPixels->getValue();
	}

	// Permissions tab settings:

	if (include_tentative || !mControls->getTentative())
	{
		fill_me_in[LLMediaEntry::CONTROLS_KEY] =
			(LLSD::Integer)mControls->getCurrentIndex();
	}

	// *NOTE: For some reason, gcc does not like these symbol references in the
	// expressions below (inside the static_casts).	I have NO idea why :(.
	// For some reason, assigning them to const temp vars here fixes the link
	// error. Bizarre.
	constexpr U8 none = LLMediaEntry::PERM_NONE;
	constexpr U8 owner = LLMediaEntry::PERM_OWNER;
	constexpr U8 group = LLMediaEntry::PERM_GROUP;
	constexpr U8 anyone = LLMediaEntry::PERM_ANYONE;
	const LLSD::Integer control = static_cast<LLSD::Integer>(
		(mPermsOwnerControl->getValue() ? owner : none) |
		(mPermsGroupControl->getValue() ? group: none) |
		(mPermsWorldControl->getValue() ? anyone : none ));
	const LLSD::Integer interact = static_cast<LLSD::Integer>(
		(mPermsOwnerInteract->getValue() ? owner: none) |
		(mPermsGroupInteract->getValue() ? group : none) |
		(mPermsWorldInteract->getValue() ? anyone : none));

	// *TODO: This will fill in the values of all permissions values, even if
	// one or more is tentative. This is not quite the user expectation... What
	// it should do is only change the bit that was made "untentative", but in
	// a multiple-selection situation, this isn't possible given the
	// architecture for how settings are applied.
	if (include_tentative ||
		!mPermsOwnerControl->getTentative() ||
		!mPermsGroupControl->getTentative() ||
		!mPermsWorldControl->getTentative())
	{
		fill_me_in[LLMediaEntry::PERMS_CONTROL_KEY] = control;
	}
	if (include_tentative ||
		!mPermsOwnerInteract->getTentative() ||
		!mPermsGroupInteract->getTentative() ||
		!mPermsWorldInteract->getTentative())
	{
		fill_me_in[LLMediaEntry::PERMS_INTERACT_KEY] = interact;
	}

	// Security tab settings:

    if (include_tentative || !mEnableWhiteList->getTentative())
	{
		fill_me_in[LLMediaEntry::WHITELIST_ENABLE_KEY] =
			(LLSD::Boolean)mEnableWhiteList->getValue();
	}

	if (include_tentative || !mWhiteListList->getTentative())
	{
		// iterate over white list and extract items
		std::vector<LLScrollListItem*> whitelist_items =
			mWhiteListList->getAllData();
		std::vector<LLScrollListItem*>::iterator iter =
			whitelist_items.begin();

		// *NOTE: need actually set the key to be an emptyArray(), or the merge
		// we do with this LLSD will think there's nothing to change.
		fill_me_in[LLMediaEntry::WHITELIST_KEY] = LLSD::emptyArray();
		while (iter != whitelist_items.end())
		{
			LLScrollListCell* cell = (*iter++)->getColumn(0);
			std::string whitelist_url = cell->getValue().asString();
			fill_me_in[LLMediaEntry::WHITELIST_KEY].append(whitelist_url);
		}
	}

	LL_DEBUGS("MediaSettings") << "Media settings:\n";
	std::stringstream str;
	LLSDSerialize::toPrettyXML(fill_me_in, str);
	LL_CONT << str.str() << LL_ENDL;
}

bool LLFloaterMediaSettings::haveValuesChanged()
{
	// *NOTE: The code below is very inefficient. Better to do this only when
	// data change.
	LLSD settings;
	getValues(settings, true);
	for (LLSD::map_const_iterator iter = settings.beginMap(),
								  end = settings.endMap();
		 iter != end; ++iter)
	{
		const std::string& current_key = iter->first;
		const LLSD& current_value = iter->second;
		if (!llsd_equals(current_value, mInitialValues[current_key]))
		{
			LL_DEBUGS("MediaSettings") << "Value for '" << current_key
									   << "' has changed to: "
									   << current_value.asString()
									   << LL_ENDL;
			return true;
		}
	}
	LL_DEBUGS("MediaSettings") << "Values didn't change." << LL_ENDL;
	return false;
}

void LLFloaterMediaSettings::commitFields()
{
	if (hasFocus())
	{
		LLUICtrl* cur_focus = gFocusMgr.getKeyboardFocusUICtrl();
		if (cur_focus && cur_focus->acceptsTextInput())
		{
			cur_focus->onCommit();
		}
	}
}

void LLFloaterMediaSettings::apply()
{
	// Pre-apply: make sure the home URL entry is committed
	mHomeURL->onCommit();

	if (haveValuesChanged())
	{
		LLSD settings;
		getValues(settings, false);
		gSelectMgr.selectionSetMedia(LLTextureEntry::MF_HAS_MEDIA, settings);

		// Post-apply: make sure to navigate to the home URL if the current URL
		// is empty and autoplay is on
		navigateHomeSelectedFace(true);
	}
}

void LLFloaterMediaSettings::updateMediaPreview()
{
	if (mHomeURL->getValue().asString().length() > 0)
	{
		if (mPreviewMedia->getCurrentNavUrl() != mHomeURL->getValue().asString())
		{
			mPreviewMedia->navigateTo(mHomeURL->getValue().asString());
			// Mute the audio of the media while previewing it
			LLViewerMediaImpl* impl = mPreviewMedia->getMediaSource();
			if (impl)
			{
				impl->setVolume(0.f);
			}
		}
	}
	// New home URL will be empty if media is deleted so display a "preview
	// goes here" data url page
	else if (mPreviewMedia->getCurrentNavUrl() != CHECKERBOARD_DATA_URL)
	{
		mPreviewMedia->navigateTo(CHECKERBOARD_DATA_URL);
	}
}

bool LLFloaterMediaSettings::navigateHomeSelectedFace(bool only_if_current_is_empty)
{
	struct functor_navigate_media : public LLSelectedTEGetFunctor<bool>
	{
		functor_navigate_media(bool flag)
		:	only_if_current_is_empty(flag)
		{
		}

		bool get(LLViewerObject* objectp, S32 face) override
		{
			if (!objectp)
			{
				return false;
			}

			LLTextureEntry* tep = objectp->getTE(face);
			if (!tep || !objectp->permModify())
			{
				return false;
			}

			const LLMediaEntry* mdatap = tep->getMediaData();
			if (!mdatap)
			{
				return false;
			}

			if (!only_if_current_is_empty ||
				(mdatap->getCurrentURL().empty() && mdatap->getAutoPlay()))
			{
				viewer_media_t media_impl =
					LLViewerMedia::getMediaImplFromTextureID(mdatap->getMediaID());
				if (media_impl)
				{
					media_impl->navigateHome();
					if (!only_if_current_is_empty)
					{
						LLSD media_data;
						media_data[LLMediaEntry::CURRENT_URL_KEY] = "";
						tep->mergeIntoMediaData(media_data);
					}
					return true;
				}
			}
			return false;
		}

		bool only_if_current_is_empty;
	} functor_navigate_media(only_if_current_is_empty);

	bool all_face_media_navigated = false;
	LLObjectSelectionHandle selected_objects = gSelectMgr.getSelection();
	selected_objects->getSelectedTEValue(&functor_navigate_media,
										 all_face_media_navigated);
	if (all_face_media_navigated)
	{
		struct functor_sync_to_server : public LLSelectedObjectFunctor
		{
			bool apply(LLViewerObject* objectp) override
			{
				LLVOVolume* vobjp = objectp->asVolume();
				if (vobjp)
				{
					vobjp->sendMediaDataUpdate();
				}
				return true;
			}
		} sendfunc;
		selected_objects->applyToObjects(&sendfunc);
	}

	// Note: we do not update the 'current URL' field until the media data
	// itself changes

	return all_face_media_navigated;
}

void LLFloaterMediaSettings::updateCurrentUrl()
{
	// Get the current URL from the selection
	const LLMediaEntry default_media_data;
	std::string value_str = default_media_data.getCurrentURL();
	struct functor_getter_current_url
	:	public LLSelectedTEGetFunctor<std::string>
	{
		functor_getter_current_url(const LLMediaEntry& entry)
		:	mMediaEntry(entry)
		{
		}

		std::string get(LLViewerObject* object, S32 face) override
		{
			LLTextureEntry* tep = object ? object->getTE(face) : NULL;
			if (tep && tep->getMediaData())
			{
				return object->getTE(face)->getMediaData()->getCurrentURL();
			}
			return mMediaEntry.getCurrentURL();
		}

		const LLMediaEntry& mMediaEntry;

	} func_current_url(default_media_data);
	bool identical =
		gSelectMgr.getSelection()->getSelectedTEValue(&func_current_url,
													  value_str);
	mCurrentURL->setText(value_str);
	mCurrentURL->setTentative(identical);

	if (isMultiple())
	{
		mCurrentURL->setText(std::string("Multiple Media"));
	}
}

const std::string LLFloaterMediaSettings::makeValidUrl(const std::string& src_url)
{
	// Use LLURI to determine if we have a valid scheme
	LLURI candidate_url(src_url);
	if (candidate_url.scheme().empty())
	{
		// Build an URL comprised of default scheme and the original fragment
		const std::string default_scheme("http://");
		return default_scheme + src_url;
	}

	// We *could* test the "default scheme" + "original fragment" URL again
	// using LLURI to see if it is valid but I think the outcome is the same
	// in either case - our only option is to return the original URL

	// We *think* the original url passed in was valid
	return src_url;
}

// Wrapper for testing an URL against the whitelist. We grab entries from white
// list list box widget and build a list to test against.
bool LLFloaterMediaSettings::urlPassesWhiteList(const std::string& test_url)
{
	// If the whitlelist list is tentative, it means we have multiple settings.
	// In that case, we have no choice but to return true
	if (mWhiteListList->getTentative())
	{
		return true;
	}

	// The checkUrlAgainstWhitelist(..) function works on a vector of strings
	// for the white list entries - in this panel, the white list is stored in
	// the widgets themselves so we need to build something compatible.
	std::vector<std::string> whitelist_strings;

	// Step through whitelist widget entries and grab them as strings
    std::vector<LLScrollListItem*> whitelist_items =
		mWhiteListList->getAllData();
    std::vector<LLScrollListItem*>::iterator iter = whitelist_items.begin();
	while (iter != whitelist_items.end())
    {
		LLScrollListCell* cell = (*iter++)->getColumn(0);
		std::string whitelist_url = cell->getValue().asString();
		whitelist_strings.emplace_back(whitelist_url);
    }

	// possible the URL is just a fragment so we validize it
	const std::string valid_url = makeValidUrl(test_url);

	// indicate if the URL passes whitelist
	return LLMediaEntry::checkUrlAgainstWhitelist(valid_url,
												  whitelist_strings);
}

void LLFloaterMediaSettings::updateWhitelistEnableStatus()
{
	// Get the value for home URL and make it a valid URL
	const std::string valid_url = makeValidUrl(getHomeUrl());

	// Now check to see if the home url passes the whitelist in its entirity
	if (urlPassesWhiteList(valid_url))
	{
		mEnableWhiteList->setEnabled(true);
		mHomeUrlFailsWhiteListText->setVisible(false);
	}
	else
	{
		mEnableWhiteList->set(false);
		mEnableWhiteList->setEnabled(false);
		mHomeUrlFailsWhiteListText->setVisible(true);
	}
}

void LLFloaterMediaSettings::addWhiteListEntry(const std::string& entry)
{
	// Grab the home url
	std::string home_url = getHomeUrl();

	// Try to make a valid URL based on what the user entered - missing scheme
	// for example
	const std::string valid_url = makeValidUrl(home_url);

	// Check the home url against this single whitelist entry
	std::vector<std::string> whitelist_entries;
	whitelist_entries.emplace_back(entry);
	bool passes = LLMediaEntry::checkUrlAgainstWhitelist(valid_url,
														 whitelist_entries);

	LLSD row;
	row["columns"][0]["type"] = "text";
	row["columns"][0]["value"] = entry;
	if (!passes && ! home_url.empty())
	{
		row["columns"][0]["color"] = LLColor4::red2.getValue();
	}

	// Add to the white list scroll box
	mWhiteListList->addElement(row);
}

bool LLFloaterMediaSettings::checkHomeUrlPassesWhitelist()
{
	std::string home_url;
	if (mHomeURL)
	{
		home_url = mHomeURL->getValue().asString();
	}
	bool fail = !home_url.empty() && !urlPassesWhiteList(home_url);
	mFailWhiteListText->setVisible(fail);

	return !fail;
}

//static
const std::string LLFloaterMediaSettings::getHomeUrl()
{
	// This will create a new instance if needed:
	LLFloaterMediaSettings* self = getInstance();

	if (self->mHomeURL)
	{
		return self->mHomeURL->getValue().asString();
	}
	else
	{
		return LLStringUtil::null;
	}
}

//static
bool LLFloaterMediaSettings::isMultiple()
{
	if (sIdenticalHasMediaInfo)
	{
		if (sMultipleMedia)
		{
			return true;
		}
	}
	else
	{
		if (sMultipleValidMedia)
		{
			return true;
		}
	}
	return false;
}

//static
void LLFloaterMediaSettings::initValues(LLSD& media_settings,
										bool editable)
{
	// This will create a new instance if needed:
	LLFloaterMediaSettings* self = getInstance();

	if (self->hasFocus()) return;

	self->clearValues(editable);
	self->mMediaEditable = editable;

	// Update all panels with values from simulator

	if (isMultiple())
	{
		// *HACK: "edit" the incoming media_settings
		media_settings[LLMediaEntry::CURRENT_URL_KEY] = "Multiple Media";
		media_settings[LLMediaEntry::HOME_URL_KEY] = "Multiple Media";
	}

	std::string base_key, tentative_key;
	LLUICtrl* uictrlp;
	struct
	{
		std::string key_name;
		LLUICtrl* ctrl_ptr;
		std::string ctrl_type;
	} data_set [] =
	{
        { LLMediaEntry::AUTO_LOOP_KEY,				self->mAutoLoop,			"LLCheckBoxCtrl" },
		{ LLMediaEntry::AUTO_PLAY_KEY,				self->mAutoPlay,			"LLCheckBoxCtrl" },
		{ LLMediaEntry::AUTO_SCALE_KEY,				self->mAutoScale,			"LLCheckBoxCtrl" },
		{ LLMediaEntry::AUTO_ZOOM_KEY,				self->mAutoZoom,			"LLCheckBoxCtrl" },
		{ LLMediaEntry::CURRENT_URL_KEY,			self->mCurrentURL,			"LLTextBox" },
		{ LLMediaEntry::HEIGHT_PIXELS_KEY,			self->mHeightPixels,		"LLSpinCtrl" },
		{ LLMediaEntry::HOME_URL_KEY,				self->mHomeURL,				"LLLineEditor" },
		{ LLMediaEntry::FIRST_CLICK_INTERACT_KEY,	self->mFirstClick,			"LLCheckBoxCtrl" },
		{ LLMediaEntry::WIDTH_PIXELS_KEY,			self->mWidthPixels,			"LLSpinCtrl" },
		{ LLMediaEntry::CONTROLS_KEY,				self->mControls,			"LLComboBox" },
        { LLMediaEntry::PERMS_OWNER_INTERACT_KEY,	self->mPermsOwnerInteract,	"LLCheckBoxCtrl" },
        { LLMediaEntry::PERMS_OWNER_CONTROL_KEY,	self->mPermsOwnerControl,	"LLCheckBoxCtrl" },
        { LLMediaEntry::PERMS_GROUP_INTERACT_KEY,	self->mPermsGroupInteract,	"LLCheckBoxCtrl" },
        { LLMediaEntry::PERMS_GROUP_CONTROL_KEY,	self->mPermsGroupControl,	"LLCheckBoxCtrl" },
        { LLMediaEntry::PERMS_ANYONE_INTERACT_KEY,	self->mPermsWorldInteract,	"LLCheckBoxCtrl" },
        { LLMediaEntry::PERMS_ANYONE_CONTROL_KEY,	self->mPermsWorldControl,	"LLCheckBoxCtrl" },
		{ LLMediaEntry::WHITELIST_ENABLE_KEY,		self->mEnableWhiteList,		"LLCheckBoxCtrl" },
		{ LLMediaEntry::WHITELIST_KEY,				self->mWhiteListList,		"LLScrollListCtrl" },
		{ "", NULL , "" }
	};

	for (U32 i = 0; data_set[i].key_name.length() > 0; ++i)
	{
		bool enabled_overridden = false;
		base_key = std::string(data_set[i].key_name);
		tentative_key = base_key + std::string(LLMediaEntry::TENTATIVE_SUFFIX);
		uictrlp = data_set[i].ctrl_ptr;
		if (uictrlp && media_settings[base_key].isDefined())
		{
			if (data_set[i].ctrl_type == "LLLineEditor")
			{
				((LLLineEditor*)uictrlp)->setText(media_settings[base_key].asString());
			}
			else if (data_set[i].ctrl_type == "LLCheckBoxCtrl")
			{
				((LLCheckBoxCtrl*)uictrlp)->setValue(media_settings[base_key].asBoolean());
			}
			else if (data_set[i].ctrl_type == "LLComboBox")
			{
				((LLComboBox*)uictrlp)->setCurrentByIndex(media_settings[base_key].asInteger());
			}
			else if (data_set[i].ctrl_type == "LLSpinCtrl")
			{
				((LLSpinCtrl*)uictrlp)->setValue(media_settings[base_key].asInteger());
			}
			else if (data_set[i].ctrl_type == "LLScrollListCtrl")
			{
				LLScrollListCtrl* list = (LLScrollListCtrl*)uictrlp;
				list->deleteAllItems();

				// Points to list of white list URLs
				LLSD& url_list = media_settings[base_key];

				// If tentative, don't add entries
				if (media_settings[tentative_key].asBoolean())
				{
					self->mWhiteListList->setEnabled(false);
					enabled_overridden = true;
				}
				else
				{
					// Iterate over them and add to scroll list
					LLSD::array_iterator iter = url_list.beginArray();
					while (iter != url_list.endArray())
					{
						std::string entry = *iter++;
						self->addWhiteListEntry(entry);
					}
				}
			}

			if (!enabled_overridden)
			{
				uictrlp->setEnabled(editable);
			}
			uictrlp->setTentative(media_settings[tentative_key].asBoolean());
		}
	}

	// General tab specific init actions:

	// Interrogates controls and updates widgets as required
	self->updateMediaPreview();

	// Permissions tab specific init actions:

	// *NOTE: If any of a particular flavor is tentative, we have to disable
	// them all because of an architectural issue: namely that we represent
	// these as a bit field, and we can't selectively apply only one bit to
	// all selected faces if they don't match.
	if (self->mPermsOwnerInteract->getTentative() ||
		self->mPermsGroupInteract->getTentative() ||
		self->mPermsWorldInteract->getTentative())
	{
		self->mPermsOwnerInteract->setEnabled(false);
		self->mPermsGroupInteract->setEnabled(false);
		self->mPermsWorldInteract->setEnabled(false);
	}
	if (self->mPermsOwnerControl->getTentative() ||
		self->mPermsGroupControl->getTentative() ||
		self->mPermsWorldControl->getTentative())
	{
		self->mPermsOwnerControl->setEnabled(false);
		self->mPermsGroupControl->setEnabled(false);
		self->mPermsWorldControl->setEnabled(false);
	}

	self->childSetEnabled("controls_label", editable);
	self->childSetEnabled("owner_label", editable);
	self->childSetEnabled("group_label", editable);
	self->childSetEnabled("anyone_label", editable);

	// Security tab specific init actions:

	// initial update - hides/shows status messages etc.
	self->updateWhitelistEnableStatus();

	// Squirrel away initial values
	self->mInitialValues.clear();
	self->getValues(self->mInitialValues, true);

	self->mApplyBtn->setEnabled(editable);
	self->mOKBtn->setEnabled(editable);
}

//static
void LLFloaterMediaSettings::clearValues(bool editable)
{
	LLFloaterMediaSettings* self = findInstance();
	if (!self)
	{
		return;
	}

	self->mGroupId.setNull();

	// General tab:

	self->mAutoLoop->clear();
	self->mAutoPlay->clear();
	self->mAutoScale->clear();
	self->mAutoZoom->clear();
	self->mCurrentURL->clear();
	self->mFirstClick->clear();
	self->mHeightPixels->clear();
	self->mHomeURL->clear();
	self->mHomeUrlCommitted = false;
	self->mWidthPixels->clear();
	self->mAutoLoop->setEnabled(editable);
	self->mAutoPlay->setEnabled(editable);
	self->mAutoScale->setEnabled(editable);
	self->mAutoZoom->setEnabled(editable);
	self->mCurrentURL->setEnabled(editable);
	self->mFirstClick->setEnabled(editable);
	self->mHeightPixels->setEnabled(editable);
	self->mHomeURL->setEnabled(editable);
	self->mWidthPixels->setEnabled(editable);
	self->updateMediaPreview();

	// Permissions tab:

	self->mControls->clear();
	self->mPermsOwnerInteract->clear();
	self->mPermsOwnerControl->clear();
	self->mPermsGroupInteract->clear();
	self->mPermsGroupControl->clear();
	self->mPermsWorldInteract->clear();
	self->mPermsWorldControl->clear();

	self->mControls->setEnabled(editable);
	self->mPermsOwnerInteract->setEnabled(editable);
	self->mPermsOwnerControl->setEnabled(editable);
	self->mPermsGroupInteract->setEnabled(editable);
	self->mPermsGroupControl->setEnabled(editable);
	self->mPermsWorldInteract->setEnabled(editable);
	self->mPermsWorldControl->setEnabled(editable);

	self->childSetEnabled("controls_label", editable);
	self->childSetEnabled("owner_label", editable);
	self->childSetEnabled("group_label", editable);
	self->childSetEnabled("anyone_label", editable);

	// Security tab:

	self->mEnableWhiteList->clear();
	self->mWhiteListList->deleteAllItems();
	self->mEnableWhiteList->setEnabled(editable);
	self->mWhiteListList->setEnabled(editable);
}

//static
void LLFloaterMediaSettings::onTabChanged(void* userdata, bool from_click)
{
	LLFloaterMediaSettings* self = (LLFloaterMediaSettings*)userdata;
	if (self && self->mTabContainer)
	{
		gSavedSettings.setS32("LastMediaSettingsTab",
							  self->mTabContainer->getCurrentPanelIndex());
	}
}

//static
void LLFloaterMediaSettings::onCommitHomeURL(LLUICtrl* ctrl, void* userdata)
{
	LLFloaterMediaSettings* self = (LLFloaterMediaSettings*)userdata;
	if (self)
	{
		// check home url passes whitelist and display warning if not
		self->mHomeUrlCommitted = self->checkHomeUrlPassesWhitelist();
		self->updateMediaPreview();
	}
}

//static
void LLFloaterMediaSettings::onCommitNewPattern(LLUICtrl* ctrl, void* userdata)
{
	LLFloaterMediaSettings* self = (LLFloaterMediaSettings*)userdata;
	if (self && self->mNewWhiteListPattern)
	{
		std::string entry = self->mNewWhiteListPattern->getText();
		if (!entry.empty())
		{
			self->addWhiteListEntry(entry);
			self->mNewWhiteListPattern->clear();
		}
	}
}

//static
void LLFloaterMediaSettings::onBtnResetCurrentUrl(void* userdata)
{
	LLFloaterMediaSettings* self = (LLFloaterMediaSettings*)userdata;
	if (self)
	{
		self->navigateHomeSelectedFace(false);
	}
}

// static
void LLFloaterMediaSettings::onBtnDel(void* userdata)
{
	LLFloaterMediaSettings* self = (LLFloaterMediaSettings*)userdata;
	if (self)
	{
		self->mWhiteListList->deleteSelectedItems();

		// contents of whitelist changed so recheck it against home url
		self->updateWhitelistEnableStatus();
	}
}

//static
void LLFloaterMediaSettings::onBtnOK(void* userdata)
{
	LLFloaterMediaSettings* self = (LLFloaterMediaSettings*)userdata;
	if (self)
	{
		self->commitFields();
		self->apply();
		self->close();
	}
}

//static
void LLFloaterMediaSettings::onBtnApply(void* userdata)
{
	LLFloaterMediaSettings* self = (LLFloaterMediaSettings*)userdata;
	if (self)
	{
		self->commitFields();
		self->apply();
		self->mInitialValues.clear();
		self->getValues(self->mInitialValues, true);
	}
}

//static
void LLFloaterMediaSettings::onBtnCancel(void* userdata)
{
	LLFloaterMediaSettings* self = (LLFloaterMediaSettings*)userdata;
	if (self)
	{
		self->close();
	}
}
