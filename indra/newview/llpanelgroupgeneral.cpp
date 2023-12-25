/**
 * @file llpanelgroupgeneral.cpp
 * @brief General information about a group.
 *
 * $LicenseInfo:firstyear=2006&license=viewergpl$
 *
 * Copyright (c) 2006-2009, Linden Research, Inc.
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

#include "llpanelgroupgeneral.h"

#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "lldbstrings.h"
#include "lleconomy.h"
#include "lllineeditor.h"
#include "llnamebox.h"
#include "llnamelistctrl.h"
#include "llspinctrl.h"
#include "lltextbox.h"
#include "lltexteditor.h"
#include "lltexturectrl.h"
#include "lluictrlfactory.h"
#include "roles_constants.h"

#include "llagent.h"
#include "llfloateravatarinfo.h"
#include "llfloatergroupinfo.h"
#include "llmutelist.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llstatusbar.h"		// can_afford_transaction()

// consts
constexpr S32 MATURE_CONTENT = 1;
constexpr S32 NON_MATURE_CONTENT = 2;
constexpr S32 DECLINE_TO_STATE = 0;

// static
void* LLPanelGroupGeneral::createTab(void* data)
{
	LLUUID* group_id = static_cast<LLUUID*>(data);
	return new LLPanelGroupGeneral("panel group general", *group_id);
}

LLPanelGroupGeneral::LLPanelGroupGeneral(const std::string& name,
										 const LLUUID& group_id)
:	LLPanelGroupTab(name, group_id),
	mChanged(false),
	mFirstUse(true),
	mPendingMemberUpdate(false),
	mUpdateInterval(0.5f),
	mSkipNextUpdate(false),
	mGroupNameEditor(NULL),
	mGroupName(NULL),
	mFounderName(NULL),
	mInsignia(NULL),
	mEditCharter(NULL),
	mBtnJoinGroup(NULL),
	mListVisibleMembers(NULL),
	mCtrlShowInGroupList(NULL),
	mComboMature(NULL),
	mCtrlOpenEnrollment(NULL),
	mCtrlEnrollmentFee(NULL),
	mSpinEnrollmentFee(NULL),
	mCtrlReceiveNotices(NULL),
	mCtrlReceiveChat(NULL),
	mCtrlListGroup(NULL),
	mActiveTitleLabel(NULL),
	mComboActiveTitle(NULL)
{
}

bool LLPanelGroupGeneral::postBuild()
{
	// General info
	mGroupNameEditor = getChild<LLLineEditor>("group_name_editor",
											  true, false);
	mGroupName = getChild<LLTextBox>("group_name", true, false);

	mInsignia = getChild<LLTextureCtrl>("insignia", true, false);
	if (mInsignia)
	{
		mInsignia->setCommitCallback(onCommitAny);
		mInsignia->setCallbackUserData(this);
		mDefaultIconID = mInsignia->getImageAssetID();
		mInsignia->setAllowLocalTexture(false);
	}

	mEditCharter = getChild<LLTextEditor>("charter", true, false);
	if (mEditCharter)
	{
		mEditCharter->setCommitCallback(onCommitAny);
		mEditCharter->setFocusReceivedCallback(onFocusEdit, this);
		mEditCharter->setFocusChangedCallback(onFocusEdit, this);
		mEditCharter->setCallbackUserData(this);
	}

	mBtnJoinGroup = getChild<LLButton>("join_button", true, false);
	if (mBtnJoinGroup)
	{
		mBtnJoinGroup->setClickedCallback(onClickJoin);
		mBtnJoinGroup->setCallbackUserData(this);
	}

	mBtnInfo = getChild<LLButton>("info_button", true, false);
	if (mBtnInfo)
	{
		mBtnInfo->setClickedCallback(onClickInfo);
		mBtnInfo->setCallbackUserData(this);
	}

	LLTextBox* founder = getChild<LLTextBox>("founder_name", true, false);
	if (founder)
	{
		mFounderName = new LLNameBox(founder->getName(), founder->getRect(),
									 LLUUID::null, false, founder->getFont(),
									 founder->getMouseOpaque());
		removeChild(founder, true);
		addChild(mFounderName);
	}

	mListVisibleMembers = getChild<LLNameListCtrl>("visible_members",
												   true, false);
	if (mListVisibleMembers)
	{
		mListVisibleMembers->setDoubleClickCallback(openProfile);
		mListVisibleMembers->setCallbackUserData(this);
	}

	// Options
	mCtrlShowInGroupList = getChild<LLCheckBoxCtrl>("show_in_group_list",
													true, false);
	if (mCtrlShowInGroupList)
	{
		mCtrlShowInGroupList->setCommitCallback(onCommitAny);
		mCtrlShowInGroupList->setCallbackUserData(this);
	}

	mComboMature = getChild<LLComboBox>("group_mature_check", true, false);
	if (mComboMature)
	{
		mComboMature->setCurrentByIndex(0);
		mComboMature->setCommitCallback(onCommitAny);
		mComboMature->setCallbackUserData(this);
		if (gAgent.isTeen())
		{
			// Teens don't get to set mature flag. JC
			mComboMature->setVisible(false);
			mComboMature->setCurrentByIndex(NON_MATURE_CONTENT);
		}
	}

	mCtrlOpenEnrollment = getChild<LLCheckBoxCtrl>("open_enrollement",
												   true, false);
	if (mCtrlOpenEnrollment)
	{
		mCtrlOpenEnrollment->setCommitCallback(onCommitAny);
		mCtrlOpenEnrollment->setCallbackUserData(this);
	}

	mCtrlEnrollmentFee = getChild<LLCheckBoxCtrl>("check_enrollment_fee",
												  true, false);
	if (mCtrlEnrollmentFee)
	{
		mCtrlEnrollmentFee->setCommitCallback(onCommitEnrollment);
		mCtrlEnrollmentFee->setCallbackUserData(this);
	}

	mSpinEnrollmentFee = getChild<LLSpinCtrl>("spin_enrollment_fee",
											  true, false);
	if (mSpinEnrollmentFee)
	{
		mSpinEnrollmentFee->setCommitCallback(onCommitAny);
		mSpinEnrollmentFee->setCallbackUserData(this);
		mSpinEnrollmentFee->setPrecision(0);
		mSpinEnrollmentFee->resetDirty();
	}

	bool accept_notices = false;
	bool list_in_profile = false;
	LLGroupData data;
	if (gAgent.getGroupData(mGroupID, data))
	{
		accept_notices = data.mAcceptNotices;
		list_in_profile = data.mListInProfile;
	}

	mCtrlReceiveNotices = getChild<LLCheckBoxCtrl>("receive_notices",
												   true, false);
	if (mCtrlReceiveNotices)
	{
		mCtrlReceiveNotices->setCommitCallback(onCommitUserOnly);
		mCtrlReceiveNotices->setCallbackUserData(this);
		mCtrlReceiveNotices->set(accept_notices);
		mCtrlReceiveNotices->setEnabled(data.mID.notNull());
	}

	mCtrlReceiveChat = getChild<LLCheckBoxCtrl>("receive_chat", true, false);
	if (mCtrlReceiveChat)
	{
		bool receive_chat = !LLMuteList::isMuted(mGroupID, "",
												 LLMute::flagTextChat);
		mCtrlReceiveChat->setCommitCallback(onCommitUserOnly);
		mCtrlReceiveChat->setCallbackUserData(this);
		mCtrlReceiveChat->set(receive_chat);
		mCtrlReceiveChat->setEnabled(data.mID.notNull());
		mCtrlReceiveChat->resetDirty();
	}

	mCtrlListGroup = getChild<LLCheckBoxCtrl>("list_groups_in_profile",
											  true, false);
	if (mCtrlListGroup)
	{
		mCtrlListGroup->setCommitCallback(onCommitUserOnly);
		mCtrlListGroup->setCallbackUserData(this);
		mCtrlListGroup->set(list_in_profile);
		mCtrlListGroup->setEnabled(data.mID.notNull());
		mCtrlListGroup->resetDirty();
	}

	mActiveTitleLabel = getChild<LLTextBox>("active_title_label",
											true, false);

	mComboActiveTitle = getChild<LLComboBox>("active_title", true, false);
	if (mComboActiveTitle)
	{
		mComboActiveTitle->setCommitCallback(onCommitTitle);
		mComboActiveTitle->setCallbackUserData(this);
		mComboActiveTitle->resetDirty();
	}

	mIncompleteMemberDataStr = getString("incomplete_member_data_str");

	LLLineEditor* group_id_line = getChild<LLLineEditor>("group_id_line",
														 true, false);
	// If the group_id is null, then we are creating a new group
	if (mGroupID.isNull())
	{
		if (mGroupNameEditor) mGroupNameEditor->setEnabled(true);
		if (mEditCharter) mEditCharter->setEnabled(true);

		if (mCtrlShowInGroupList) mCtrlShowInGroupList->setEnabled(true);
		if (mComboMature) mComboMature->setEnabled(true);
		if (mCtrlOpenEnrollment) mCtrlOpenEnrollment->setEnabled(true);
		if (mCtrlEnrollmentFee) mCtrlEnrollmentFee->setEnabled(true);
		if (mSpinEnrollmentFee) mSpinEnrollmentFee->setEnabled(true);

		if (mBtnJoinGroup) mBtnJoinGroup->setVisible(false);
		if (mBtnInfo) mBtnInfo->setVisible(false);
		if (mGroupName) mGroupName->setVisible(false);
		if (group_id_line) group_id_line->setVisible(false);
	}
	else if (group_id_line)
	{
		group_id_line->setText(mGroupID.asString());
	}

	return LLPanelGroupTab::postBuild();
}

// static
void LLPanelGroupGeneral::onFocusEdit(LLFocusableElement* ctrl, void* data)
{
	LLPanelGroupGeneral* self = (LLPanelGroupGeneral*)data;
	if (self)
	{
		self->updateChanged();
		self->notifyObservers();
	}
}

// static
void LLPanelGroupGeneral::onCommitAny(LLUICtrl* ctrl, void* data)
{
	LLPanelGroupGeneral* self = (LLPanelGroupGeneral*)data;
	if (self)
	{
		self->updateChanged();
		self->notifyObservers();
	}
}

// static
void LLPanelGroupGeneral::onCommitUserOnly(LLUICtrl* ctrl, void* data)
{
	LLPanelGroupGeneral* self = (LLPanelGroupGeneral*)data;
	if (self)
	{
		self->mChanged = true;
		self->notifyObservers();
	}
}

// static
void LLPanelGroupGeneral::onCommitEnrollment(LLUICtrl* ctrl, void* data)
{
	onCommitAny(ctrl, data);

	LLPanelGroupGeneral* self = (LLPanelGroupGeneral*)data;
	// Make sure both enrollment related widgets are there.
	if (!self || !self->mCtrlEnrollmentFee || !self->mSpinEnrollmentFee)
	{
		return;
	}

	// Make sure the agent can change enrollment info.
	if (!gAgent.hasPowerInGroup(self->mGroupID, GP_MEMBER_OPTIONS) ||
		!self->mAllowEdit)
	{
		return;
	}

	if (self->mCtrlEnrollmentFee->get())
	{
		self->mSpinEnrollmentFee->setEnabled(true);
	}
	else
	{
		self->mSpinEnrollmentFee->setEnabled(false);
		self->mSpinEnrollmentFee->set(0);
	}
}

// static
void LLPanelGroupGeneral::onCommitTitle(LLUICtrl* ctrl, void* data)
{
	LLPanelGroupGeneral* self = (LLPanelGroupGeneral*)data;
	if (!self || self->mGroupID.isNull() || !self->mAllowEdit) return;
	gGroupMgr.sendGroupTitleUpdate(self->mGroupID,
								   self->mComboActiveTitle->getCurrentID());
	self->update(GC_TITLES);
	self->mComboActiveTitle->resetDirty();
}

// static
void LLPanelGroupGeneral::onClickInfo(void* userdata)
{
	LLPanelGroupGeneral* self = (LLPanelGroupGeneral*)userdata;
	if (self)
	{
		LL_DEBUGS("GroupPanel") << "Opening group info for group: "
								<< self->mGroupID << LL_ENDL;
		LLFloaterGroupInfo::showFromUUID(self->mGroupID);
	}
}

// static
void LLPanelGroupGeneral::onClickJoin(void* userdata)
{
	LLPanelGroupGeneral* self = (LLPanelGroupGeneral*)userdata;
	if (!self) return;

	LL_DEBUGS("GroupPanel") << "Joining group: " << self->mGroupID << LL_ENDL;

	LLGroupMgrGroupData* gdatap = gGroupMgr.getGroupData(self->mGroupID);
	if (gdatap)
	{
		S32 cost = gdatap->mMembershipFee;
		LLSD args;
		args["COST"] = llformat("%d", cost);
		LLSD payload;
		payload["group_id"] = self->mGroupID;

		if (can_afford_transaction(cost))
		{
			gNotifications.add("JoinGroupCanAfford", args, payload,
							   LLPanelGroupGeneral::joinDlgCB);
		}
		else
		{
			gNotifications.add("JoinGroupCannotAfford", args, payload);
		}
	}
	else
	{
		llwarns << "getGroupData(" << self->mGroupID << ") was NULL" << llendl;
	}
}

// static
bool LLPanelGroupGeneral::joinDlgCB(const LLSD& notif, const LLSD& response)
{
	if (LLNotification::getSelectedOption(notif, response) == 1)
	{
		// User clicked cancel
		return false;
	}

	gGroupMgr.sendGroupMemberJoin(notif["payload"]["group_id"].asUUID());
	return false;
}

// static
void LLPanelGroupGeneral::openProfile(void* data)
{
	LLPanelGroupGeneral* self = (LLPanelGroupGeneral*)data;

	if (self && self->mListVisibleMembers)
	{
		LLScrollListItem* selected =
			self->mListVisibleMembers->getFirstSelected();
		if (selected)
		{
			LLFloaterAvatarInfo::showFromDirectory(selected->getUUID());
		}
	}
}

bool LLPanelGroupGeneral::needsApply(std::string& mesg)
{
	updateChanged();
	mesg = getString("group_info_unchanged");
	return mChanged || mGroupID.isNull();
}

void LLPanelGroupGeneral::activate()
{
	LLGroupMgrGroupData* gdatap = gGroupMgr.getGroupData(mGroupID);
	if (mGroupID.notNull() && (!gdatap || mFirstUse))
	{
		gGroupMgr.sendGroupTitlesRequest(mGroupID);
		gGroupMgr.sendGroupPropertiesRequest(mGroupID);
#if 0
		if (!gdatap || !gdatap->isMemberDataComplete())
		{
			gGroupMgr.sendCapGroupMembersRequest(mGroupID);
		}
#endif
		mFirstUse = false;
	}
	mChanged = false;

	update(GC_ALL);
}

void LLPanelGroupGeneral::draw()
{
	// Do not update every frame: that would be insane !
	if (mSkipNextUpdate)
	{
		// Compute the time the viewer took to 'digest' the update and come
		// back to us; the name list update takes time, and the avatar name
		// query takes even more time when the name is not cached !
		mUpdateInterval = (mUpdateInterval + 3.f *
						   mUpdateTimer.getElapsedTimeF32()) * 0.5f;
		mSkipNextUpdate = false;
		LL_DEBUGS("GroupPanel") << "Interval for next update = "
								<< mUpdateInterval << "s" << LL_ENDL;
		mUpdateTimer.reset();
	}
	else if (mPendingMemberUpdate &&
			 mUpdateTimer.getElapsedTimeF32() > mUpdateInterval)
	{
		mUpdateTimer.reset();
		updateMembers();
		mSkipNextUpdate = true;
	}

	LLPanelGroupTab::draw();
}

bool LLPanelGroupGeneral::apply(std::string& mesg)
{
	bool has_power_in_group = gAgent.hasPowerInGroup(mGroupID,
													 GP_GROUP_CHANGE_IDENTITY);

	if (has_power_in_group || mGroupID.isNull())
	{
		// Check to make sure mature has been set
		if (mComboMature &&
		    mComboMature->getCurrentIndex() == DECLINE_TO_STATE)
		{
			gNotifications.add("SetGroupMature", LLSD(), LLSD(),
							   boost::bind(&LLPanelGroupGeneral::confirmMatureApply,
										   this, _1, _2));
			return false;
		}

		if (mGroupID.isNull())
		{
			// We need all these for the callback
			if (mGroupNameEditor && mEditCharter && mCtrlShowInGroupList &&
				mInsignia && mCtrlOpenEnrollment && mComboMature)
			{
				// Validate the group name length.
				S32 group_name_len = mGroupNameEditor->getText().size();
				if (group_name_len < DB_GROUP_NAME_MIN_LEN ||
					group_name_len > DB_GROUP_NAME_STR_LEN)
				{
					std::ostringstream temp_error;
					temp_error << "A group name must be between "
							   << DB_GROUP_NAME_MIN_LEN << " and "
							   << DB_GROUP_NAME_STR_LEN << " characters.";
					mesg = temp_error.str();
					return false;
				}

				LLSD args;
				args["COST"] = LLEconomy::getInstance()->getCreateGroupCost();
				gNotifications.add("CreateGroupCost", args, LLSD(),
								   boost::bind(&LLPanelGroupGeneral::createGroupCallback,
											   this, _1, _2));
			}
			else
			{
				mesg = "Missing UI elements in the group panel !";
			}
			return false;
		}

		LLGroupMgrGroupData* gdatap = gGroupMgr.getGroupData(mGroupID);
		if (!gdatap)
		{
			// *TODO: Translate
			mesg = std::string("No group data found for group ");
			mesg.append(mGroupID.asString());
			return false;
		}

		bool can_change_ident = gAgent.hasPowerInGroup(mGroupID,
													   GP_GROUP_CHANGE_IDENTITY);
		if (can_change_ident)
		{
			if (mEditCharter)
			{
				gdatap->mCharter = mEditCharter->getText();
			}
			if (mInsignia)
			{
				gdatap->mInsigniaID = mInsignia->getImageAssetID();
			}
			if (mComboMature)
			{
				if (!gAgent.isTeen())
				{
					gdatap->mMaturePublish =
						(mComboMature->getCurrentIndex() == MATURE_CONTENT);
				}
				else
				{
					gdatap->mMaturePublish = false;
				}
			}
			if (mCtrlShowInGroupList)
			{
				gdatap->mShowInList = mCtrlShowInGroupList->get();
			}
		}

		bool can_change_member_opts = gAgent.hasPowerInGroup(mGroupID,
															 GP_MEMBER_OPTIONS);
		if (can_change_member_opts)
		{
			if (mCtrlOpenEnrollment)
			{
				gdatap->mOpenEnrollment = mCtrlOpenEnrollment->get();
			}
			if (mCtrlEnrollmentFee && mSpinEnrollmentFee)
			{
				gdatap->mMembershipFee = (mCtrlEnrollmentFee->get()) ?
										  (S32)mSpinEnrollmentFee->get() : 0;
				// Set to the used value, and reset initial value used for
				// isdirty check
				mSpinEnrollmentFee->set((F32)gdatap->mMembershipFee);
			}
		}

		if (can_change_ident || can_change_member_opts)
		{
			gGroupMgr.sendUpdateGroupInfo(mGroupID);
		}
	}

	bool receive_notices = false;
	bool list_in_profile = false;
	if (mCtrlReceiveNotices)
	{
		receive_notices = mCtrlReceiveNotices->get();
		mCtrlReceiveNotices->resetDirty();
	}
	if (mCtrlListGroup)
	{
		list_in_profile = mCtrlListGroup->get();
		mCtrlListGroup->resetDirty();
	}

	if (mCtrlReceiveChat)
	{
		LLGroupData data;
		LLGroupMgrGroupData* gdatap = gGroupMgr.getGroupData(mGroupID);
		if (gdatap)
		{
			LLMute mute(mGroupID, gdatap->mName, LLMute::GROUP);
			if (mCtrlReceiveChat->get())
			{
				if (LLMuteList::isMuted(mGroupID, "", LLMute::flagTextChat))
				{
					LLMuteList::remove(mute, LLMute::flagTextChat);
				}
			}
			else
			{
				if (!LLMuteList::isMuted(mGroupID, "", LLMute::flagTextChat))
				{
					LLMuteList::add(mute, LLMute::flagTextChat);
				}
			}
		}
		mCtrlReceiveChat->resetDirty();
	}

	gAgent.setUserGroupFlags(mGroupID, receive_notices, list_in_profile);

	mChanged = false;

	return true;
}

void LLPanelGroupGeneral::cancel()
{
	mChanged = false;

	// Cancel out all of the click changes to, although since we are shifting
	// tabs or closing the floater, this need not be done... yet.
	notifyObservers();
}

// Invoked from callbackConfirmMature
bool LLPanelGroupGeneral::confirmMatureApply(const LLSD& notification,
											 const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	// 0 == Yes
	// 1 == No
	// 2 == Cancel
	switch (option)
	{
		case 0:
			mComboMature->setCurrentByIndex(MATURE_CONTENT);
			break;
		case 1:
			mComboMature->setCurrentByIndex(NON_MATURE_CONTENT);
			break;
		default:
			return false;
	}

	// If we got here it means they set a valid value
	std::string mesg = "";
	apply(mesg);
	return false;
}

// static
bool LLPanelGroupGeneral::createGroupCallback(const LLSD& notification,
											  const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		U32 enrollment_fee = (mCtrlEnrollmentFee->get() ?
							  (U32) mSpinEnrollmentFee->get() : 0);

		gGroupMgr.sendCreateGroupRequest(mGroupNameEditor->getText(),
										 mEditCharter->getText(),
										 mCtrlShowInGroupList->get(),
										 mInsignia->getImageAssetID(),
										 enrollment_fee,
										 mCtrlOpenEnrollment->get(), false,
										 mComboMature->getCurrentIndex() == MATURE_CONTENT);
	}
	return false;
}

// virtual
void LLPanelGroupGeneral::update(LLGroupChange gc)
{
	if (mGroupID.isNull()) return;

	LLGroupMgrGroupData* gdatap = gGroupMgr.getGroupData(mGroupID);
	if (!gdatap) return;

	LLGroupData agent_gdatap;
	bool is_member = false;
	if (gAgent.getGroupData(mGroupID, agent_gdatap)) is_member = true;

	if (mComboActiveTitle)
	{
		mComboActiveTitle->setVisible(is_member);
		mComboActiveTitle->setEnabled(mAllowEdit);

		if (mActiveTitleLabel) mActiveTitleLabel->setVisible(is_member);

		if (is_member)
		{
			LLUUID current_title_role;

			mComboActiveTitle->clear();
			mComboActiveTitle->removeall();
			bool has_selected_title = false;

			if (1 == gdatap->mTitles.size())
			{
				// Only the everyone title.  Don't bother letting them try
				// changing this.
				mComboActiveTitle->setEnabled(false);
			}
			else
			{
				mComboActiveTitle->setEnabled(true);
			}

			for (std::vector<LLGroupTitle>::const_iterator
					citer = gdatap->mTitles.begin(),
					end = gdatap->mTitles.end();
				 citer != end; ++citer)
			{
				mComboActiveTitle->add(citer->mTitle, citer->mRoleID,
									   citer->mSelected ? ADD_TOP
														: ADD_BOTTOM);
				if (citer->mSelected)
				{
					mComboActiveTitle->setCurrentByID(citer->mRoleID);
					has_selected_title = true;
				}
			}

			if (!has_selected_title)
			{
				mComboActiveTitle->setCurrentByID(LLUUID::null);
			}
		}

		mComboActiveTitle->resetDirty();
	}

	// If this was just a titles update, we are done.
	if (gc == GC_TITLES) return;

	bool can_change_ident = false;
	bool can_change_member_opts = false;
	can_change_ident = gAgent.hasPowerInGroup(mGroupID,
											  GP_GROUP_CHANGE_IDENTITY);
	can_change_member_opts = gAgent.hasPowerInGroup(mGroupID,
													GP_MEMBER_OPTIONS);

	if (mCtrlShowInGroupList)
	{
		mCtrlShowInGroupList->set(gdatap->mShowInList);
		mCtrlShowInGroupList->setEnabled(mAllowEdit && can_change_ident);
		mCtrlShowInGroupList->resetDirty();

	}
	if (mComboMature)
	{
		if (gdatap->mMaturePublish)
		{
			mComboMature->setCurrentByIndex(MATURE_CONTENT);
		}
		else
		{
			mComboMature->setCurrentByIndex(NON_MATURE_CONTENT);
		}
		mComboMature->setEnabled(mAllowEdit && can_change_ident);
		mComboMature->setVisible(!gAgent.isTeen());
		mComboMature->resetDirty();
	}

	if (mCtrlOpenEnrollment)
	{
		mCtrlOpenEnrollment->set(gdatap->mOpenEnrollment);
		mCtrlOpenEnrollment->setEnabled(mAllowEdit && can_change_member_opts);
		mCtrlOpenEnrollment->resetDirty();
	}

	if (mCtrlEnrollmentFee)
	{
		mCtrlEnrollmentFee->set(gdatap->mMembershipFee > 0);
		mCtrlEnrollmentFee->setEnabled(mAllowEdit && can_change_member_opts);
		mCtrlEnrollmentFee->resetDirty();
	}

	if (mSpinEnrollmentFee)
	{
		S32 fee = gdatap->mMembershipFee;
		mSpinEnrollmentFee->set((F32)fee);
		mSpinEnrollmentFee->setEnabled(mAllowEdit && fee > 0 &&
									   can_change_member_opts);
		mSpinEnrollmentFee->resetDirty();
	}

	if (mBtnJoinGroup)
	{
		bool visible = !is_member && gdatap->mOpenEnrollment;
//MK
		if (gRLenabled && gRLInterface.contains("setgroup"))
		{
			visible = false;
		}
//mk
		mBtnJoinGroup->setVisible(visible);

		if (visible)
		{
			// *TODO: translate
			std::string fee = llformat("Join (L$%d)", gdatap->mMembershipFee);
			mBtnJoinGroup->setLabelSelected(fee);
			mBtnJoinGroup->setLabelUnselected(fee);
		}
	}

	if (mBtnInfo)
	{
		mBtnInfo->setVisible(is_member && !mAllowEdit);
	}

	if (mCtrlReceiveNotices && gc == GC_ALL)
	{
		mCtrlReceiveNotices->set(agent_gdatap.mAcceptNotices);
		mCtrlReceiveNotices->setVisible(is_member);
		mCtrlReceiveNotices->setEnabled(mAllowEdit && is_member);
		mCtrlReceiveNotices->resetDirty();
	}

	if (mCtrlReceiveChat && gc == GC_ALL)
	{
		bool receive_chat = !LLMuteList::isMuted(mGroupID, "",
												 LLMute::flagTextChat);
		mCtrlReceiveChat->set(receive_chat);
		mCtrlReceiveChat->setVisible(is_member);
		mCtrlReceiveChat->setEnabled(mAllowEdit);
		mCtrlReceiveChat->resetDirty();
	}

	if (mCtrlListGroup && gc == GC_ALL)
	{
		mCtrlListGroup->set(agent_gdatap.mListInProfile);
		mCtrlListGroup->setVisible(is_member);
		mCtrlListGroup->setEnabled(mAllowEdit);
		mCtrlListGroup->resetDirty();
	}

	if (mGroupName)
	{
		mGroupName->setText(gdatap->mName);
	}

	if (mGroupNameEditor)
	{
		mGroupNameEditor->setVisible(false);
	}

	if (mFounderName)
	{
		mFounderName->setNameID(gdatap->mFounderID, false);
	}

	if (mInsignia)
	{
		mInsignia->setEnabled(mAllowEdit && can_change_ident);
		if (gdatap->mInsigniaID.notNull())
		{
			mInsignia->setImageAssetID(gdatap->mInsigniaID);
		}
		else
		{
			mInsignia->setImageAssetID(mDefaultIconID);
		}
	}

	if (mEditCharter)
	{
		mEditCharter->setEnabled(mAllowEdit && can_change_ident);
		mEditCharter->setText(gdatap->mCharter);
		mEditCharter->resetDirty();
	}

	if (mListVisibleMembers)
	{
		mListVisibleMembers->deleteAllItems();

		if (gdatap->isMemberDataComplete())
		{
			mMemberProgress = gdatap->mMembers.begin();
			mPendingMemberUpdate = true;
		}
		else
		{
			std::stringstream pending;
			pending << "Retrieving member list (" << gdatap->mMembers.size()
					<< "\\" << gdatap->mMemberCount  << ")";

			LLSD row;
			row["columns"][0]["value"] = pending.str();

			mListVisibleMembers->setEnabled(false);
			mListVisibleMembers->addElement(row);
		}
	}
}

void LLPanelGroupGeneral::updateMembers()
{
	mPendingMemberUpdate = false;

	LLGroupMgrGroupData* gdatap = gGroupMgr.getGroupData(mGroupID);

	if (!mListVisibleMembers || !gdatap || !gdatap->isMemberDataComplete() ||
		gdatap->mMembers.empty())
	{
		return;
	}

	mListVisibleMembers->setAllowRefresh(false);
	mListVisibleMembers->setLazyUpdateInterval(5.f);

	LLGroupMgrGroupData::member_list_t::iterator end = gdatap->mMembers.end();
	U32 i;
	LLTimer update_time;
	update_time.setTimerExpirySec(UPDATE_MEMBERS_SECONDS_PER_FRAME);
	for (i = 0; mMemberProgress != end && !update_time.hasExpired();
		 ++mMemberProgress, ++i)
	{
		LLGroupMemberData* member = mMemberProgress->second;
		if (!member)
		{
			continue;
		}
		// Owners show up in bold.
		std::string style = "NORMAL";
		if (member->isOwner())
		{
			style = "BOLD";
		}

		LLSD row;
		row["id"] = member->getID();

		LLSD& columns = row["columns"];

		columns[0]["column"] = "name";
		columns[0]["font-style"] = style;
		// value is filled in by name list control

		columns[1]["column"] = "title";
		columns[1]["value"] = member->getTitle();
		columns[1]["font-style"] = style;

		columns[2]["column"] = "online";
		columns[2]["value"] = member->getOnlineStatus();
		columns[2]["font-style"] = style;

		mListVisibleMembers->addElement(row);	//, ADD_SORTED);
	}

	if (mMemberProgress == end)
	{
		mListVisibleMembers->setEnabled(true);
		mListVisibleMembers->setAllowRefresh(true);
		LL_DEBUGS("GroupPanel") << i
								<< " members added to the list. No more member pending."
								<< LL_ENDL;
	}
	else
	{
		mPendingMemberUpdate = true;
		mListVisibleMembers->setEnabled(false);
		mListVisibleMembers->setLazyUpdateInterval(1.f);
		LL_DEBUGS("GroupPanel") << i
								<< " members added to the list. There are still pending members."
								<< LL_ENDL;
	}
}

void LLPanelGroupGeneral::updateChanged()
{
	// List all the controls we want to check for changes...
	LLUICtrl* check_list[] = {
		mGroupNameEditor,
		mGroupName,
		mFounderName,
		mInsignia,
		mEditCharter,
		mCtrlShowInGroupList,
		mComboMature,
		mCtrlOpenEnrollment,
		mCtrlEnrollmentFee,
		mSpinEnrollmentFee,
		mCtrlReceiveNotices,
		mCtrlReceiveChat,
		mCtrlListGroup,
		mActiveTitleLabel,
		mComboActiveTitle
	};
	constexpr S32 num_ctrls = LL_ARRAY_SIZE(check_list);

	mChanged = false;

	for (S32 i = 0; i < num_ctrls; ++i)
	{
		if (check_list[i] && check_list[i]->isDirty())
		{
			mChanged = true;
			break;
		}
	}
}
