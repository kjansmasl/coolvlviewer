/**
 * @file llfloaterexperienceprofile.cpp
 * @brief llfloaterexperienceprofile and related class definitions
 *
 * $LicenseInfo:firstyear=2013&license=viewergpl$
 *
 * Copyright (c) 2013, Linden Research, Inc.
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

#include "llfloaterexperienceprofile.h"

#include "llbutton.h"
#include "llcachename.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "llevents.h"
#include "llexperiencecache.h"
#include "lllineeditor.h"
#include "llnotifications.h"
#include "llsdserialize.h"
#include "lltabcontainer.h"
#include "lltextbox.h"
#include "lltexteditor.h"
#include "lltrans.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "llappviewer.h"
#include "llcommandhandler.h"
#include "llexperiencelog.h"		// For PUMP_EXPERIENCE
#include "llfloateravatarinfo.h"
#include "llfloatergroups.h"
#include "llfloatergroupinfo.h"
#include "llfloaterreporter.h"
#include "llmediactrl.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llslurl.h"
#include "llurldispatcher.h"
#include "lltexturectrl.h"
#include "llviewerregion.h"
#include "llviewerwindow.h"			// For gViewerWindowp
#include "llweb.h"

//static
LLFloaterExperienceProfile::instances_map_t LLFloaterExperienceProfile::sInstances;

// Command handler

class LLExperienceHandler final : public LLCommandHandler
{
public:
	LLExperienceHandler()
	:	LLCommandHandler("experience", UNTRUSTED_THROTTLE)
	{
	}

	bool handle(const LLSD& params, const LLSD&, LLMediaCtrl*) override
	{
		if (params.size() != 2 || params[1].asString() != "profile")
		{
			return false;
		}

		LLExperienceCache* exp = LLExperienceCache::getInstance();
		exp->get(params[0].asUUID(),
				 boost::bind(&LLExperienceHandler::experienceCallback, this,
							 _1));
		return true;
	}

	void experienceCallback(const LLSD& exp_details)
	{
		if (!exp_details.has(LLExperienceCache::MISSING))
		{
			LLUUID id = exp_details[LLExperienceCache::EXPERIENCE_ID].asUUID();
			if (id.notNull())
			{
				LLFloaterExperienceProfile::show(id);
			}
		}
	}
};

LLExperienceHandler gExperienceHandler;

// LLFloaterExperienceProfile class

//static
LLFloaterExperienceProfile* LLFloaterExperienceProfile::show(const LLUUID& id)
{
	LLFloaterExperienceProfile* self = NULL;

	instances_map_t::iterator it = sInstances.find(id);
	if (it == sInstances.end())
	{
		self = new LLFloaterExperienceProfile(id);
	}
	else
	{
		self = it->second;
	}

	if (self)
	{
		self->open();
		self->setFocus(true);
	}

	return self;
}

LLFloaterExperienceProfile::LLFloaterExperienceProfile(const LLUUID& exp_id)
:	LLFloater(exp_id.asString()),
	mExperienceId(exp_id),
	mSaveCompleteAction(NOTHING),
	mDirty(false),
	mForceClose(false)
{
	sInstances[exp_id] = this;
	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_experienceprofile.xml");
}

LLFloaterExperienceProfile::~LLFloaterExperienceProfile()
{
	sInstances.erase(mExperienceId);
}

bool LLFloaterExperienceProfile::postBuild()
{
	mEditBtn = getChild<LLButton>("edit_btn");
	mEditBtn->setClickedCallback(onClickEdit, this);
	mEditBtn->setVisible(false);

	mAllowBtn = getChild<LLButton>("allow_btn");
	mAllowBtn->setClickedCallback(onClickAllow, this);

	mForgetBtn = getChild<LLButton>("forget_btn");
	mForgetBtn->setClickedCallback(onClickForget, this);

	mBlockBtn = getChild<LLButton>("block_btn");
	mBlockBtn->setClickedCallback(onClickBlock, this);

	childSetAction("cancel_btn", onClickCancel, this);

	mSaveBtn = getChild<LLButton>("save_btn");
	mSaveBtn->setClickedCallback(onClickSave, this);

	childSetAction("location_btn", onClickLocation, this);
	childSetAction("clear_btn", onClickClear, this);

	LLColor4 links_color = LLTextEditor::getLinksColor();

	mOwnerText = getChild<LLTextBox>("OwnerText");
	mOwnerText->setClickedCallback(onOwnerProfile, this);
	mOwnerText->setColor(links_color);

	mGroupBtn = getChild<LLButton>("group_btn");
	mGroupBtn->setClickedCallback(onPickGroup, this);

	mGroupText = getChild<LLTextBox>("GroupText");
	mGroupText->setClickedCallback(onShowGroupInfo, this);
	mGroupText->setColor(links_color);

	mEditGroupText = getChild<LLTextBox>("edit_GroupText");
	mEditGroupText->setClickedCallback(onShowGroupInfo, this);
	mEditGroupText->setColor(links_color);

	childSetAction("report_btn", onReportExperience, this);

	mExperienceDescEditor = getChild<LLTextEditor>("edit_experience_description");
	mExperienceDescEditor->setKeystrokeCallback(onTextKeystroke, this);
	mExperienceDescEditor->setCommitOnFocusLost(true);

	mRatingCombo = getChild<LLComboBox>("edit_ContentRatingText");
	mRatingCombo->setCommitCallback(onFieldChanged);
	mRatingCombo->setCallbackUserData(this);

	mRatingText = getChild<LLTextBox>("ContentRatingText");

	mMarketplaceText = getChild<LLTextBox>("marketplace");
	mMarketplaceText->setClickedCallback(onOpenMarketplaceURL, this);
	mMarketplaceText->setColor(links_color);

	mMarketplaceEditor = getChild<LLLineEditor>("edit_marketplace");
	mMarketplaceEditor->setKeystrokeCallback(onLineKeystroke);
	mMarketplaceEditor->setCallbackUserData(this);

	mExperienceTitleText = getChild<LLTextBox>("experience_title");
	mExperienceTitleText->setClickedCallback(onClickExperienceTitle, this);

	mExperienceTitleEditor = getChild<LLLineEditor>("edit_experience_title");
	mExperienceTitleEditor->setKeystrokeCallback(onLineKeystroke);
	mExperienceTitleEditor->setCallbackUserData(this);

	mEnableCheck = getChild<LLCheckBoxCtrl>("edit_enable_btn");
	mEnableCheck->setCommitCallback(onFieldChanged);
	mEnableCheck->setCallbackUserData(this);

	mPrivateCheck = getChild<LLCheckBoxCtrl>("edit_private_btn");
	mPrivateCheck->setCommitCallback(onFieldChanged);
	mPrivateCheck->setCallbackUserData(this);

	mLogoTexture = getChild<LLTextureCtrl>("logo");
	mLogoTexture->setFallbackImageName("default_land_picture.j2c");

	mEditLogoTexture = getChild<LLTextureCtrl>("edit_logo");
	mEditLogoTexture->setCommitCallback(onFieldChanged);
	mEditLogoTexture->setCallbackUserData(this);
	mEditLogoTexture->setFallbackImageName("default_land_picture.j2c");

	mLocationText = getChild<LLTextBox>("LocationTextText");
	mLocationText->setClickedCallback(onShowLocation, this);
	mLocationText->setColor(links_color);

	mEditLocationText = getChild<LLTextBox>("edit_LocationTextText");
	mEditLocationText->setClickedCallback(onShowLocation, this);
	mEditLocationText->setColor(links_color);

	if (mExperienceId.notNull())
	{
		LLExperienceCache* expcache = LLExperienceCache::getInstance();
		expcache->fetch(mExperienceId, true);
		expcache->get(mExperienceId,
					  boost::bind(&LLFloaterExperienceProfile::experienceCallback,
								  getDerivedHandle<LLFloaterExperienceProfile>(),
								  _1));

		if (gAgent.hasRegionCapability("IsExperienceAdmin"))
		{
			expcache->getExperienceAdmin(mExperienceId,
										 boost::bind(&LLFloaterExperienceProfile::experienceIsAdmin,
													 getDerivedHandle<LLFloaterExperienceProfile>(),
													 _1));
		}
	}

	LLEventPump& pump = gEventPumps.obtain(PUMP_EXPERIENCE);
	pump.listen(mExperienceId.asString() + "-profile",
				boost::bind(&LLFloaterExperienceProfile::experiencePermission,
							getDerivedHandle<LLFloaterExperienceProfile>(this),
							_1));

	return true;
}

void LLFloaterExperienceProfile::experienceCallback(LLHandle<LLFloaterExperienceProfile> handle,
													const LLSD& experience)
{
	LLFloaterExperienceProfile* self = handle.get();
	if (self)
	{
		self->refreshExperience(experience);
	}
}

bool LLFloaterExperienceProfile::experiencePermission(LLHandle<LLFloaterExperienceProfile> handle,
													  const LLSD& permission)
{
	LLFloaterExperienceProfile* self = handle.get();
	if (self)
	{
		self->updatePermission(permission);
	}
	return false;
}

void LLFloaterExperienceProfile::setPermission(const char* perm)
{
	if (gAgent.hasRegionCapability("ExperiencePreferences"))
	{
		LLExperienceCache* exp = LLExperienceCache::getInstance();
		exp->setExperiencePermission(mExperienceId, perm,
									 boost::bind(&LLFloaterExperienceProfile::experiencePermissionResults,
									 mExperienceId, _1));
	}
}

bool LLFloaterExperienceProfile::setMaturityString(S32 maturity)
{
	std::string access;
	if (maturity <= (S32)SIM_ACCESS_PG)
	{
		access = LLTrans::getString("SIM_ACCESS_PG");
		mRatingCombo->setCurrentByIndex(2);
	}
	else if (maturity <= (S32)SIM_ACCESS_MATURE)
	{
		access = LLTrans::getString("SIM_ACCESS_MATURE");
		mRatingCombo->setCurrentByIndex(1);
	}
	else if (maturity <= (S32)SIM_ACCESS_ADULT)
	{
		access = LLTrans::getString("SIM_ACCESS_ADULT");
		mRatingCombo->setCurrentByIndex(0);
	}
	else
	{
		return false;
	}
	mRatingText->setText(access);

	return true;
}

void LLFloaterExperienceProfile::refreshExperience(const LLSD& experience)
{
	mExperienceDetails = experience;
	mPackage = experience;

	LLPanel* imagePanel = getChild<LLPanel>("image_panel");
	LLPanel* locationPanel = getChild<LLPanel>("location panel");
	LLPanel* marketplacePanel = getChild<LLPanel>("marketplace panel");
	LLPanel* groupPanel = getChild<LLPanel>("group_panel");

	imagePanel->setVisible(false);
	locationPanel->setVisible(false);
	marketplacePanel->setVisible(false);

	mExperienceTitleText->setText(experience[LLExperienceCache::NAME].asString());
	LLSLURL exp_slurl("experience",
					  experience[LLExperienceCache::EXPERIENCE_ID], "profile");
	mExperienceSLURL = exp_slurl.getSLURLString();
	mExperienceTitleEditor->setText(experience[LLExperienceCache::NAME].asString());

	std::string value = experience[LLExperienceCache::DESCRIPTION].asString();
	getChild<LLTextEditor>("experience_description")->setText(value);

	mExperienceDescEditor->setText(value);

	mLocationSLURL = experience[LLExperienceCache::SLURL].asString();
//MK
	if (gRLenabled && gRLInterface.mContainsShowloc)
	{
		mLocationSLURL.clear();
	}
//mk
	bool has_slurl = mLocationSLURL.length() > 0;
	locationPanel->setVisible(has_slurl);
	if (has_slurl)
	{
		std::string loc_str;
		// Depending on the experience, experience[LLExperienceCache::SLURL]
		// can be either an actual SLURL, or a region name followed with
		// coordinates between parenthesis... Let's convert it into an actual
		// SLURL, always.
		size_t i = mLocationSLURL.find('(');
		if (i != std::string::npos)
		{
			llwarns << "Invalid SLURL (apparently got a region name and position instead): "
					<< mLocationSLURL << " - Converting to valid SLURL..."
					<< llendl;
			std::string region = mLocationSLURL.substr(0, i);
			LLStringUtil::trimTail(region);	// Remove any trailing space
			std::string loc = mLocationSLURL.substr(i + 1,
													mLocationSLURL.find(')'));
			LLStringUtil::trimHead(loc);	// Remove any leading space
			LLStringUtil::trimTail(loc);	// Remove any trailing space
			S32 x, y, z;
			S32 matched = sscanf(loc.c_str(), "%i,%i,%i", &x, &y, &z);
			if (matched != 3)
			{
				llwarns << "... no valid position found, using center sim..."
						<< llendl;
				// Use the center sim instead...
				x = y = 128;
				z = 0;
			}
			LLVector3 pos = LLVector3(x, y, z);
			LLSLURL loc_slurl(region, pos);
			mLocationSLURL = loc_slurl.getSLURLString();
			loc_str = loc_slurl.getLocationString();
			llinfos << "... converted to SLURL: " << mLocationSLURL << llendl;
		}
		else
		{
			LLSLURL loc_slurl(mLocationSLURL);
			mLocationSLURL = loc_slurl.getSLURLString();
			loc_str = loc_slurl.getLocationString();
		}
		mLocationText->setText(loc_str);
		mEditLocationText->setText(loc_str);
	}
	else
	{
		mLocationSLURL.clear();
		mLocationText->setText("");
		mEditLocationText->setText("");
	}

	setMaturityString(experience[LLExperienceCache::MATURITY].asInteger());

	LLUUID id = experience[LLExperienceCache::AGENT_ID].asUUID();
	setOwnerId(id, this);

	id = experience[LLExperienceCache::GROUP_ID].asUUID();
	groupPanel->setVisible(!id.isNull());
	setEditGroup(id, this);

	mGroupBtn->setEnabled(experience[LLExperienceCache::AGENT_ID].asUUID() == gAgentID);

	S32 properties = mExperienceDetails[LLExperienceCache::PROPERTIES].asInteger();
	mEnableCheck->set(!(properties & LLExperienceCache::PROPERTY_DISABLED));
	mPrivateCheck->set(properties & LLExperienceCache::PROPERTY_PRIVATE);

	LLTextBox* child = getChild<LLTextBox>("grid_wide");
	child->setVisible(true);
	if (properties & LLExperienceCache::PROPERTY_GRID)
	{
		child->setText(LLTrans::getString("Grid-Scope"));
	}
	else
	{
		child->setText(LLTrans::getString("Land-Scope"));
	}

	if (properties & LLExperienceCache::PROPERTY_PRIVILEGED)
	{
		child = getChild<LLTextBox>("privileged");
		child->setVisible(true);
	}
	else if (gAgent.hasRegionCapability("ExperiencePreferences"))
	{
		LLExperienceCache* exp = LLExperienceCache::getInstance();
		exp->getExperiencePermission(mExperienceId,
									 boost::bind(&LLFloaterExperienceProfile::experiencePermissionResults,
												 mExperienceId, _1));
	}

	value = experience[LLExperienceCache::METADATA].asString();
	if (value.empty())
	{
		return;
	}

	LLPointer<LLSDParser> parser = new LLSDXMLParser();
	LLSD data;
	std::istringstream is(value);
	if (LLSDParser::PARSE_FAILURE != parser->parse(is, data, value.size()))
	{
		mMarketplaceURL.clear();
		if (data.has("marketplace"))
		{
			mMarketplaceURL = data["marketplace"].asString();
		}
		mMarketplaceEditor->setText(mMarketplaceURL);
		mMarketplaceText->setText(mMarketplaceURL);
		marketplacePanel->setVisible(!mMarketplaceURL.empty());

		if (data.has("logo"))
		{
			LLUUID id = data["logo"].asUUID();
			mLogoTexture->setImageAssetID(id);
			mEditLogoTexture->setImageAssetID(id);
			imagePanel->setVisible(id.notNull());
		}
	}
	else
	{
		marketplacePanel->setVisible(false);
		imagePanel->setVisible(false);
	}

	mDirty = false;
	mForceClose = false;
	mSaveBtn->setEnabled(mDirty);
}

void LLFloaterExperienceProfile::setPreferences(const LLSD& content)
{
	S32 properties = mExperienceDetails[LLExperienceCache::PROPERTIES].asInteger();
	if (properties & LLExperienceCache::PROPERTY_PRIVILEGED)
	{
		return;
	}

	const LLSD& experiences = content["experiences"];
	const LLSD& blocked = content["blocked"];

	for (LLSD::array_const_iterator it = experiences.beginArray(),
									end = experiences.endArray();
		 it != end; ++it)
	{
		if (it->asUUID() == mExperienceId)
		{
			experienceAllowed();
			return;
		}
	}

	for (LLSD::array_const_iterator it = blocked.beginArray(),
									end = blocked.endArray();
		 it != end; ++it)
	{
		if (it->asUUID() == mExperienceId)
		{
			experienceBlocked();
			return;
		}
	}

	experienceForgotten();
}

bool LLFloaterExperienceProfile::canClose()
{
	if (mForceClose || !mDirty)
	{
		return true;
	}
	else
	{
		// Bring up view-modal dialog: Save changes? Yes, No, Cancel
		gNotifications.add("SaveChanges", LLSD(), LLSD(),
						   boost::bind(&LLFloaterExperienceProfile::handleSaveChangesDialog,
									   this, _1, _2, CLOSE));
		return false;
	}
}

bool LLFloaterExperienceProfile::handleSaveChangesDialog(const LLSD& notification,
														 const LLSD& response,
														 PostSaveAction action)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	switch (option)
	{
		case 0:  // "Yes"
			// close after saving
			doSave(action);
			break;

		case 1:  // "No"
			if (action != NOTHING)
			{
				mForceClose = true;
				if (action==CLOSE)
				{
					close();
				}
				else
				{
					changeToView();
				}
			}
			break;

		case 2: // "Cancel"
		default:
			// If we were quitting, we didn't really mean it.
			gAppViewerp->abortQuit();
	}
	return false;
}

void LLFloaterExperienceProfile::doSave(S32 success_action)
{
	mSaveCompleteAction = success_action;

	if (gAgent.hasRegionCapability("UpdateExperience"))
	{
		LLExperienceCache* exp = LLExperienceCache::getInstance();
		exp->updateExperience(mPackage,
							  boost::bind(&LLFloaterExperienceProfile::experienceUpdateResult,
										  getDerivedHandle<LLFloaterExperienceProfile>(),
										  _1));
	}
}

void LLFloaterExperienceProfile::onSaveComplete(const LLSD& content)
{
	LLUUID id = getExperienceId();

	if (content.has("removed"))
	{
		const LLSD& removed = content["removed"];
		for (LLSD::map_const_iterator it = removed.beginMap(),
									  end = removed.endMap();
			 it != end; ++it)
		{
			const std::string& field = it->first;
			if (field == LLExperienceCache::EXPERIENCE_ID)
			{
				// this message should be removed by the experience api
				continue;
			}
			const LLSD& data = it->second;
			std::string error_tag = data["error_tag"].asString() +
									"ExperienceProfileMessage";
			LLSD fields;
			if (gNotifications.getTemplate(error_tag))
			{
				fields["FIELD"] = field;
				fields["EXTRA_INFO"] = data["extra_info"];
				gNotifications.add(error_tag, fields);
			}
			else
			{
				fields["MESSAGE"] = data["en"];
				gNotifications.add("GenericAlert", fields);
			}
		}
	}

	if (!content.has("experience_keys"))
	{
		llwarns << "Call done with bad content" << llendl;
		return;
	}

	const LLSD& experiences = content["experience_keys"];

	LLSD::array_const_iterator it = experiences.beginArray();
	if (it == experiences.endArray())
	{
		llwarns << "Call done with empty content" << llendl;
		return;
	}

	if (!it->has(LLExperienceCache::EXPERIENCE_ID) ||
		((*it)[LLExperienceCache::EXPERIENCE_ID].asUUID() != id))
	{
		llwarns << "Call done with unexpected experience id" << llendl;
		return;
	}

	refreshExperience(*it);
	LLExperienceCache* expcache = LLExperienceCache::getInstance();
	expcache->insert(*it);
	expcache->fetch(id, true);

	if (mSaveCompleteAction == VIEW)
	{
		LLTabContainer* tabs = getChild<LLTabContainer>("tab_container");
		tabs->selectTabByName("panel_experience_info");
	}
	else if (mSaveCompleteAction == CLOSE)
	{
		close();
	}
}

void LLFloaterExperienceProfile::changeToView()
{
	if (mForceClose || !mDirty)
	{
		refreshExperience(mExperienceDetails);
		LLTabContainer* tabs = getChild<LLTabContainer>("tab_container");

		tabs->selectTabByName("panel_experience_info");
	}
	else
	{
		// Bring up view-modal dialog: Save changes? Yes, No, Cancel
		gNotifications.add("SaveChanges", LLSD(), LLSD(),
						   boost::bind(&LLFloaterExperienceProfile::handleSaveChangesDialog,
									   this, _1, _2, VIEW));
	}
}

void LLFloaterExperienceProfile::updatePermission(const LLSD& permission)
{
	if (permission.has("experience"))
	{
		if (permission["experience"].asUUID() != mExperienceId)
		{
			return;
		}

		std::string str = permission[mExperienceId.asString()]["permission"].asString();
		if (str == "Allow")
		{
			experienceAllowed();
		}
		else if (str == "Block")
		{
			experienceBlocked();
		}
		else if (str == "Forget")
		{
			experienceForgotten();
		}
	}
	else
	{
		setPreferences(permission);
	}
}

void LLFloaterExperienceProfile::experienceAllowed()
{
	mAllowBtn->setEnabled(false);
	mForgetBtn->setEnabled(true);
	mBlockBtn->setEnabled(true);
}

void LLFloaterExperienceProfile::experienceForgotten()
{
	mAllowBtn->setEnabled(true);
	mForgetBtn->setEnabled(false);
	mBlockBtn->setEnabled(true);
}

void LLFloaterExperienceProfile::experienceBlocked()
{
	mAllowBtn->setEnabled(true);
	mForgetBtn->setEnabled(true);
	mBlockBtn->setEnabled(false);
}

void LLFloaterExperienceProfile::onClose(bool app_quitting)
{
	LLEventPump& pump = gEventPumps.obtain(PUMP_EXPERIENCE);
	pump.stopListening(mExperienceId.asString() + "-profile");
	LLFloater::onClose(app_quitting);
}

void LLFloaterExperienceProfile::updatePackage()
{
	mPackage[LLExperienceCache::NAME] = mExperienceTitleEditor->getText();
	mPackage[LLExperienceCache::DESCRIPTION] = mExperienceDescEditor->getText();
	if (mLocationSLURL.empty())
	{
		mPackage[LLExperienceCache::SLURL] = LLStringUtil::null;
	}
	else
	{
		mPackage[LLExperienceCache::SLURL] = mLocationSLURL;
	}

	mPackage[LLExperienceCache::MATURITY] = mRatingCombo->getSelectedValue().asInteger();

	LLSD metadata;
	metadata["marketplace"] = mMarketplaceEditor->getText();
	metadata["logo"] = mEditLogoTexture->getImageAssetID();

	LLPointer<LLSDXMLFormatter> formatter = new LLSDXMLFormatter();
	std::ostringstream os;
	if (formatter->format(metadata, os))
	{
		mPackage[LLExperienceCache::METADATA] = os.str();
	}

	S32 properties = mPackage[LLExperienceCache::PROPERTIES].asInteger();
	if (mEnableCheck->get())
	{
		properties &= ~LLExperienceCache::PROPERTY_DISABLED;
	}
	else
	{
		properties |= LLExperienceCache::PROPERTY_DISABLED;
	}

	if (mPrivateCheck->get())
	{
		properties |= LLExperienceCache::PROPERTY_PRIVATE;
	}
	else
	{
		properties &= ~LLExperienceCache::PROPERTY_PRIVATE;
	}

	mPackage[LLExperienceCache::PROPERTIES] = properties;
}

//static
LLUUID LLFloaterExperienceProfile::getInstanceId(LLFloaterExperienceProfile* instance)
{
	for (instances_map_t::const_iterator it = sInstances.begin(),
										 end = sInstances.end();
		 it != end; ++it)
	{
		if (it->second == instance)
		{
			return it->first;
		}
	}
	return LLUUID::null;
}

//static
void LLFloaterExperienceProfile::nameCallback(const LLUUID& id,
											  const std::string& name,
											  bool is_group,
											  LLFloaterExperienceProfile* self)
{
	if (self && getInstanceId(self).notNull())
	{
		if (is_group)
		{
			if (id == self->mGroupId)
			{
				self->mGroupText->setText(name);
				self->mEditGroupText->setText(name);
			}
		}
		else if (id == self->mOwnerId)
		{
			self->mOwnerText->setText(name);
		}
	}
}

//static
void LLFloaterExperienceProfile::setOwnerId(LLUUID agent_id, void* data)
{
	LLFloaterExperienceProfile* self = (LLFloaterExperienceProfile*)data;
	if (self && getInstanceId(self).notNull())
	{
		self->mOwnerId = agent_id;

		std::string value;
		if (agent_id.notNull() && gCacheNamep)
		{
			if (!gCacheNamep->getFullName(agent_id, value))
			{
				gCacheNamep->get(agent_id, false,
								 boost::bind(&LLFloaterExperienceProfile::nameCallback,
								 _1, _2, _3, self));
			}
		}
		self->mOwnerText->setText(value);
	}
}

//static
void LLFloaterExperienceProfile::setEditGroup(LLUUID group_id, void* data)
{
	LLFloaterExperienceProfile* self = (LLFloaterExperienceProfile*)data;
	if (self && getInstanceId(self).notNull())
	{
		self->mGroupId = group_id;

		std::string value;
		if (group_id.notNull() && gCacheNamep)
		{
			if (!gCacheNamep->getGroupName(group_id, value))
			{
				gCacheNamep->get(group_id, true,
								 boost::bind(&LLFloaterExperienceProfile::nameCallback,
								 _1, _2, _3, self));
			}
		}
		self->mGroupText->setText(value);
		self->mEditGroupText->setText(value);

		self->mPackage[LLExperienceCache::GROUP_ID] = group_id;

		onFieldChanged(NULL, data);
	}
}

//static
void LLFloaterExperienceProfile::onClickEdit(void* data)
{
	LLFloaterExperienceProfile* self = (LLFloaterExperienceProfile*)data;
	if (self)
	{
		LLTabContainer* tabs = self->getChild<LLTabContainer>("tab_container");
		tabs->selectTabByName("edit_panel_experience_info");
	}
}

//static
void LLFloaterExperienceProfile::onClickCancel(void* data)
{
	LLFloaterExperienceProfile* self = (LLFloaterExperienceProfile*)data;
	if (self)
	{
		self->changeToView();
	}
}

//static
void LLFloaterExperienceProfile::onClickSave(void* data)
{
	LLFloaterExperienceProfile* self = (LLFloaterExperienceProfile*)data;
	if (self)
	{
		self->doSave(NOTHING);
	}
}

//static
void LLFloaterExperienceProfile::onClickAllow(void* data)
{
	LLFloaterExperienceProfile* self = (LLFloaterExperienceProfile*)data;
	if (self)
	{
		self->setPermission("Allow");
	}
}

//static
void LLFloaterExperienceProfile::onClickBlock(void* data)
{
	LLFloaterExperienceProfile* self = (LLFloaterExperienceProfile*)data;
	if (self)
	{
		self->setPermission("Block");
	}
}

//static
void LLFloaterExperienceProfile::onClickForget(void* data)
{
	LLFloaterExperienceProfile* self = (LLFloaterExperienceProfile*)data;
	if (self && gAgent.hasRegionCapability("ExperiencePreferences"))
	{
		LLExperienceCache* exp = LLExperienceCache::getInstance();
		exp->forgetExperiencePermission(self->mExperienceId,
										boost::bind(&LLFloaterExperienceProfile::experiencePermissionResults,
													self->mExperienceId, _1));
	}
}

//static
void LLFloaterExperienceProfile::onLineKeystroke(LLLineEditor*, void* data)
{
	onFieldChanged(NULL, data);
}

//static
void LLFloaterExperienceProfile::onTextKeystroke(LLTextEditor*, void* data)
{
	onFieldChanged(NULL, data);
}

//static
void LLFloaterExperienceProfile::onFieldChanged(LLUICtrl*, void* data)
{
	LLFloaterExperienceProfile* self = (LLFloaterExperienceProfile*)data;
	if (self)
	{
		self->updatePackage();

		if (!self->mEditBtn->getVisible())
		{
			return;
		}
		LLSD::map_const_iterator st = self->mExperienceDetails.beginMap();
		LLSD::map_const_iterator dt = self->mPackage.beginMap();

		self->mDirty = false;
		while (!self->mDirty && st != self->mExperienceDetails.endMap() &&
			   dt != self->mPackage.endMap())
		{
			self->mDirty = st->first != dt->first ||
						   st->second.asString() != dt->second.asString();
			++st;
			++dt;
		}

		if (!self->mDirty &&
			(st != self->mExperienceDetails.endMap() ||
			 dt != self->mPackage.endMap()))
		{
			self->mDirty = true;
		}

		self->mSaveBtn->setEnabled(self->mDirty);
	}
}

//static
void LLFloaterExperienceProfile::onClickLocation(void* data)
{
//MK
	if (gRLenabled && gRLInterface.mContainsShowloc)
	{
		return;
	}
//mk
	LLFloaterExperienceProfile* self = (LLFloaterExperienceProfile*)data;
	if (self)
	{
		LLViewerRegion* region = gAgent.getRegion();
		if (region)
		{
			LLSLURL loc_slurl(region->getName(), gAgent.getPositionGlobal());
			self->mLocationSLURL = loc_slurl.getSLURLString();
			self->mEditLocationText->setText(loc_slurl.getLocationString());
			onFieldChanged(NULL, self);
		}
	}
}

//static
void LLFloaterExperienceProfile::onClickClear(void* data)
{
	LLFloaterExperienceProfile* self = (LLFloaterExperienceProfile*)data;
	if (self)
	{
		self->mEditLocationText->setText("");
		self->mLocationSLURL.clear();
		onFieldChanged(NULL, self);
	}
}

//static
void LLFloaterExperienceProfile::onPickGroup(void* data)
{
	LLFloaterExperienceProfile* self = (LLFloaterExperienceProfile*)data;
	if (self && gFloaterViewp)
	{
		LLFloaterGroupPicker* widget= LLFloaterGroupPicker::show(setEditGroup,
																 self);
		LLFloater* parentp = gFloaterViewp->getParentFloater(self);
		if (widget && parentp)
		{
			LLRect new_rect = gFloaterViewp->findNeighboringPosition(parentp,
																	 widget);
			widget->setOrigin(new_rect.mLeft, new_rect.mBottom);
			parentp->addDependentFloater(widget);
		}
	}
}

//static
void LLFloaterExperienceProfile::onClickExperienceTitle(void* data)
{
	LLFloaterExperienceProfile* self = (LLFloaterExperienceProfile*)data;
	if (self && !self->mExperienceSLURL.empty())
	{
		gWindowp->copyTextToClipboard(utf8str_to_wstring(self->mExperienceSLURL));
		gNotifications.add("SLURLCopiedtoClipboard");
	}
}

//static
void LLFloaterExperienceProfile::onOwnerProfile(void* data)
{
	LLFloaterExperienceProfile* self = (LLFloaterExperienceProfile*)data;
	if (self && self->mOwnerId.notNull())
	{
		LLFloaterAvatarInfo::show(self->mOwnerId);
	}
}

//static
void LLFloaterExperienceProfile::onShowGroupInfo(void* data)
{
	LLFloaterExperienceProfile* self = (LLFloaterExperienceProfile*)data;
	if (self && self->mGroupId.notNull())
	{
		LLFloaterGroupInfo::showFromUUID(self->mGroupId);
	}
}

//static
void LLFloaterExperienceProfile::onShowLocation(void* data)
{
	LLFloaterExperienceProfile* self = (LLFloaterExperienceProfile*)data;
	if (self && !self->mLocationSLURL.empty())
	{
		LLMediaCtrl* web = NULL;
		LLURLDispatcher::dispatch(self->mLocationSLURL, "clicked", web, true);
	}
}

//static
void LLFloaterExperienceProfile::onOpenMarketplaceURL(void* data)
{
	LLFloaterExperienceProfile* self = (LLFloaterExperienceProfile*)data;
	if (self && !self->mMarketplaceURL.empty())
	{
		LLWeb::loadURL(self->mMarketplaceURL);
	}
}

//static
void LLFloaterExperienceProfile::onReportExperience(void* data)
{
	LLFloaterExperienceProfile* self = (LLFloaterExperienceProfile*)data;
	if (self)
	{
		LLFloaterReporter::showFromExperience(self->mExperienceId);
	}
}

//static
bool LLFloaterExperienceProfile::hasPermission(const LLSD& content,
											   const std::string& name,
											   const LLUUID& test)
{
	if (!content.has(name)) return false;

	const LLSD& list = content[name];
	for (LLSD::array_const_iterator it = list.beginArray(),
									end = list.endArray();
		 it != end; ++it)
	{
		if (it->asUUID() == test)
		{
			return true;
		}
	}

	return false;
}

//static
void LLFloaterExperienceProfile::experiencePermissionResults(LLUUID exp_id,
															 LLSD result)
{
	std::string permission;
	if (hasPermission(result, "experiences", exp_id))
	{
		permission = "Allow";
	}
	else if (hasPermission(result, "blocked", exp_id))
	{
		permission = "Block";
	}
	else
	{
		permission = "Forget";
	}

	LLSD experience;
	experience["permission"] = permission;
	LLSD message;
	message["experience"] = exp_id;
	message[exp_id.asString()] = experience;
	gEventPumps.obtain(PUMP_EXPERIENCE).post(message);
}

//static
void LLFloaterExperienceProfile::experienceIsAdmin(LLHandle<LLFloaterExperienceProfile> handle,
												   const LLSD& result)
{
	LLFloaterExperienceProfile* self = handle.get();
	if (self && result["status"].asBoolean() &&
		gAgent.hasRegionCapability("UpdateExperience"))
	{
		self->mEditBtn->setVisible(true);
	}
}

//static
void LLFloaterExperienceProfile::experienceUpdateResult(LLHandle<LLFloaterExperienceProfile> handle,
														const LLSD& result)
{
	LLFloaterExperienceProfile* self = handle.get();
	if (self)
	{
		self->onSaveComplete(result);
	}
}
