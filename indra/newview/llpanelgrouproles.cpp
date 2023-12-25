/**
 * @file llpanelgrouproles.cpp
 * @brief Panel for roles information about a particular group.
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

#include "llpanelgrouproles.h"

#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "lliconctrl.h"
#include "lllineeditor.h"
#include "llnamelistctrl.h"
#include "llnotifications.h"
#include "llscrolllistctrl.h"
#include "lltabcontainer.h"
#include "lltextbox.h"
#include "lltexteditor.h"
#include "roles_constants.h"

#include "llagent.h"
#include "llappviewer.h"			// For gFrameTimeSeconds
#include "llfloateravatarinfo.h"
#include "llfloatergroupbulkban.h"
#include "llfloatergroupinvite.h"
#include "llviewertexturelist.h"

bool agentCanRemoveFromRole(const LLUUID& group_id,
							const LLUUID& role_id)
{
	return gAgent.hasPowerInGroup(group_id, GP_ROLE_REMOVE_MEMBER);
}

////////////////////////////
// LLPanelGroupRoles
////////////////////////////

//static
void* LLPanelGroupRoles::createTab(void* data)
{
	LLUUID* group_id = (LLUUID*)data;
	return new LLPanelGroupRoles("panel group roles", *group_id);
}

LLPanelGroupRoles::LLPanelGroupRoles(const std::string& name,
									 const LLUUID& group_id)
:	LLPanelGroupTab(name, group_id),
	mCurrentTab(NULL),
	mRequestedTab(NULL),
	mSubTabContainer(NULL),
	mIgnoreTransition(false)
{
}

LLPanelGroupRoles::~LLPanelGroupRoles()
{
	if (mSubTabContainer)
	{
		for (S32 i = 0; i < mSubTabContainer->getTabCount(); ++i)
		{
			LLPanelGroupSubTab* tabp =
				(LLPanelGroupSubTab*)mSubTabContainer->getPanelByIndex(i);
			tabp->removeObserver(this);
		}
	}
}

bool LLPanelGroupRoles::postBuild()
{
	mSubTabContainer = getChild<LLTabContainer>("roles_tab_container",
												true, false);
	if (!mSubTabContainer) return false;

	// Hook up each sub-tab callback and widgets.
	for (S32 i = 0; i < mSubTabContainer->getTabCount(); ++i)
	{
		LLPanelGroupSubTab* tabp =
			(LLPanelGroupSubTab*)mSubTabContainer->getPanelByIndex(i);

		// Add click callbacks to all the tabs.
		mSubTabContainer->setTabChangeCallback(tabp, onClickSubTab);
		mSubTabContainer->setTabUserData(tabp, this);

		// Hand the subtab a pointer to this LLPanelGroupRoles, so that it can
		// look around for the widgets it is interested in.
		if (!tabp->postBuildSubTab(this)) return false;

		tabp->addObserver(this);
	}

	// Set the current tab to whatever is currently being shown.
	mCurrentTab = (LLPanelGroupTab*)mSubTabContainer->getCurrentPanel();
	if (!mCurrentTab)
	{
		// Need to select a tab.
		mSubTabContainer->selectFirstTab();
		mCurrentTab = (LLPanelGroupTab*)mSubTabContainer->getCurrentPanel();
	}
	if (!mCurrentTab) return false;

	// Act as though this tab was just activated.
	mCurrentTab->activate();

	// Read apply text from the xml file.
	mDefaultNeedsApplyMesg = getString("default_needs_apply_text");
	mWantApplyMesg = getString("want_apply_text");

	return LLPanelGroupTab::postBuild();
}

bool LLPanelGroupRoles::isVisibleByAgent()
{
#if 0	// This power was removed to make group roles simpler
	return agentp && gAgent.hasPowerInGroup(mGroupID,
											GP_ROLE_CREATE |
											GP_ROLE_DELETE |
											GP_ROLE_PROPERTIES |
											GP_ROLE_VIEW |
											GP_ROLE_ASSIGN_MEMBER |
											GP_ROLE_REMOVE_MEMBER |
											GP_ROLE_CHANGE_ACTIONS |
											GP_MEMBER_INVITE |
											GP_MEMBER_EJECT |
											GP_MEMBER_OPTIONS);
#else
	return mAllowEdit && gAgent.isInGroup(mGroupID);
#endif
}

//static
void LLPanelGroupRoles::onClickSubTab(void* userdata, bool from_click)
{
	LLPanelGroupRoles* self = (LLPanelGroupRoles*)userdata;
	if (self)
	{
		self->handleClickSubTab();
	}
}

void LLPanelGroupRoles::handleClickSubTab()
{
	// If we are already handling a transition, ignore this.
	if (mIgnoreTransition)
	{
		return;
	}

	mRequestedTab = (LLPanelGroupTab*)mSubTabContainer->getCurrentPanel();

	// Make sure they aren't just clicking the same tab...
	if (mRequestedTab == mCurrentTab)
	{
		return;
	}

	// Try to switch from the current panel to the panel the user selected.
	attemptTransition();
}

bool LLPanelGroupRoles::attemptTransition()
{
	// Check if the current tab needs to be applied.
	std::string mesg;
	if (!mCurrentTab || !mCurrentTab->needsApply(mesg))
	{
		// The current panel did not have anything it needed to apply.
		if (mRequestedTab)
		{
			transitionToTab();
		}
		return true;
	}

	// If no message was provided, give a generic one.
	if (mesg.empty())
	{
		mesg = mDefaultNeedsApplyMesg;
	}
	// Create a notify box, telling the user about the unapplied tab.
	LLSD args;
	args["NEEDS_APPLY_MESSAGE"] = mesg;
	args["WANT_APPLY_MESSAGE"] = mWantApplyMesg;
	gNotifications.add("PanelGroupApply", args, LLSD(),
					   boost::bind(&LLPanelGroupRoles::handleNotifyCallback,
								   this, _1, _2));
	mHasModal = true;

	// We need to reselect the current tab, since it isn't finished.
	if (mSubTabContainer)
	{
		mIgnoreTransition = true;
		mSubTabContainer->selectTabPanel(mCurrentTab);
		mIgnoreTransition = false;
	}

	// Returning false will block a close action from finishing until we get a
	// response back from the user.
	return false;
}

void LLPanelGroupRoles::transitionToTab()
{
	// Tell the current panel that it is being deactivated.
	if (mCurrentTab)
	{
		mCurrentTab->deactivate();
	}

	// Tell the new panel that it is being activated.
	if (mRequestedTab)
	{
		// This is now the current tab;
		mCurrentTab = mRequestedTab;
		mCurrentTab->activate();
	}
}

bool LLPanelGroupRoles::handleNotifyCallback(const LLSD& notification,
											 const LLSD& response)
{
	mHasModal = false;

	S32 option = LLNotification::getSelectedOption(notification, response);

	if (option >= 2 || option < 0) // "Cancel" or unknown option
	{
		return false;
	}

	if (option == 1) // "Ignore changes"
	{
		// Switch to the requested panel without applying changes
		cancel();
		mIgnoreTransition = true;
		mSubTabContainer->selectTabPanel(mRequestedTab);
		mIgnoreTransition = false;
		transitionToTab();
		return false;
	}

	// option == 0 ("Apply changes"): try to apply changes, and switch to the
	// requested tab.

	std::string apply_mesg;
	if (!apply(apply_mesg))
	{
		// There was a problem doing the apply.
		if (!apply_mesg.empty())
		{
			mHasModal = true;
			LLSD args;
			args["MESSAGE"] = apply_mesg;
			gNotifications.add("GenericAlert", args, LLSD(),
							   boost::bind(&LLPanelGroupRoles::onModalClose,
										   this, _1, _2));
		}
		// Skip switching tabs.
		return false;
	}

	// This panel's info successfully applied switch to the next panel
	mIgnoreTransition = true;
	mSubTabContainer->selectTabPanel(mRequestedTab);
	mIgnoreTransition = false;
	transitionToTab();

	return false;
}

bool LLPanelGroupRoles::onModalClose(const LLSD& notification,
									 const LLSD& response)
{
	mHasModal = false;
	return false;
}

bool LLPanelGroupRoles::apply(std::string& mesg)
{
	// Pass this along to the currently visible sub tab.
	LLPanelGroupTab* panelp = NULL;
	if (mSubTabContainer)
	{
		panelp = (LLPanelGroupTab*)mSubTabContainer->getCurrentPanel();
	}
	if (!panelp)
	{
		return false;
	}

	// Ignore the needs apply message.
	std::string ignore_mesg;
	if (!panelp->needsApply(ignore_mesg))
	{
		// We do not need to apply anything: we are done.
		return true;
	}

	// Try to do the actual apply.
	return panelp->apply(mesg);
}

void LLPanelGroupRoles::cancel()
{
	// Pass this along to the currently visible sub tab.
	LLPanelGroupTab* panelp = NULL;
	if (mSubTabContainer)
	{
		panelp = (LLPanelGroupTab*)mSubTabContainer->getCurrentPanel();
	}
	if (panelp)
	{
		panelp->cancel();
	}
}

// Pass all of these messages to the currently visible sub tab.
std::string LLPanelGroupRoles::getHelpText() const
{
	LLPanelGroupTab* panelp = NULL;
	if (mSubTabContainer)
	{
		panelp = (LLPanelGroupTab*)mSubTabContainer->getCurrentPanel();
	}
	return panelp ? panelp->getHelpText() : mHelpText;
}

void LLPanelGroupRoles::update(LLGroupChange gc)
{
	if (mGroupID.isNull() || !mSubTabContainer)
	{
		return;
	}

	LLPanelGroupTab* panelp =
		(LLPanelGroupTab*)mSubTabContainer->getCurrentPanel();
	if (panelp)
	{
		panelp->update(gc);
	}
	else
	{
		llwarns << "No subtab to update !" << llendl;
	}
}

void LLPanelGroupRoles::activate()
{
	if (!mSubTabContainer || !gAgent.isInGroup(mGroupID))
	{
		return;
	}

	// Start requesting member and role data if needed.
	LLGroupMgrGroupData* gdatap = gGroupMgr.getGroupData(mGroupID);
#if 0
	// Check member data.
	if (!gdatap || !gdatap->isMemberDataComplete())
	{
		gGroupMgr.sendCapGroupMembersRequest(mGroupID);
	}
#endif
	// Check role data.
	if (!gdatap || !gdatap->isRoleDataComplete())
	{
		// Mildly hackish - clear all pending changes
		cancel();
		gGroupMgr.sendGroupRoleDataRequest(mGroupID);
	}
#if 0
	// Check role-member mapping data.
	if (!gdatap || !gdatap->isRoleMemberDataComplete())
	{
		gGroupMgr.sendGroupRoleMembersRequest(mGroupID);
	}
#endif
	// Need this to get base group member powers
	if (!gdatap || !gdatap->isGroupPropertiesDataComplete())
	{
		gGroupMgr.sendGroupPropertiesRequest(mGroupID);
	}

	LLPanelGroupTab* panelp =
		(LLPanelGroupTab*)mSubTabContainer->getCurrentPanel();
	if (panelp)
	{
		panelp->activate();
	}
}

void LLPanelGroupRoles::deactivate()
{
	if (!mSubTabContainer) return;

	LLPanelGroupTab* panelp =
		(LLPanelGroupTab*)mSubTabContainer->getCurrentPanel();
	if (panelp)
	{
		panelp->deactivate();
	}
}

bool LLPanelGroupRoles::needsApply(std::string& mesg)
{
	if (!mSubTabContainer)
	{
		return false;
	}

	LLPanelGroupTab* panelp =
		(LLPanelGroupTab*)mSubTabContainer->getCurrentPanel();
	return panelp && panelp->needsApply(mesg);
}

bool LLPanelGroupRoles::hasModal()
{
	if (mHasModal)
	{
		return true;
	}

	if (!mSubTabContainer)
	{
		return false;
	}

	LLPanelGroupTab* panelp =
		(LLPanelGroupTab*)mSubTabContainer->getCurrentPanel();
	return panelp && panelp->hasModal();
}

// PanelGroupTab observer trigger
void LLPanelGroupRoles::tabChanged()
{
	notifyObservers();
}

////////////////////////////
// LLPanelGroupSubTab
////////////////////////////
LLPanelGroupSubTab::LLPanelGroupSubTab(const std::string& name,
									   const LLUUID& group_id)
:	LLPanelGroupTab(name, group_id),
	mHeader(NULL),
	mFooter(NULL),
	mHasGroupBanPower(false),
	mSearchLineEditor(NULL),
	mSearchButton(NULL),
	mShowAllButton(NULL)
{
}

bool LLPanelGroupSubTab::postBuild()
{
	// Hook up the search widgets.
	mSearchLineEditor = getChild<LLLineEditor>("search_text", true, false);
	if (!mSearchLineEditor) return false;
	mSearchLineEditor->setKeystrokeCallback(onSearchKeystroke);
	mSearchLineEditor->setCallbackUserData(this);

	mSearchButton = getChild<LLButton>("search_button", true, false);
	if (!mSearchButton) return false;
	mSearchButton->setClickedCallback(onClickSearch);
	mSearchButton->setCallbackUserData(this);
	mSearchButton->setEnabled(false);

	mShowAllButton = getChild<LLButton>("show_all_button", true, false);
	if (!mShowAllButton) return false;
	mShowAllButton->setClickedCallback(onClickShowAll);
	mShowAllButton->setCallbackUserData(this);
	mShowAllButton->setEnabled(false);

	// Get icons for later use.
	mActionIcons.clear();

	LLIconCtrl* icon = getChild<LLIconCtrl>("power_folder_icon", false, false);
	if (icon && !icon->getImageName().empty())
	{
		mActionIcons["folder"] = icon->getImageName();
		removeChild(icon, true);
	}

	icon = getChild<LLIconCtrl>("power_all_have_icon", false, false);
	if (icon && !icon->getImageName().empty())
	{
		mActionIcons["full"] = icon->getImageName();
		removeChild(icon, true);
	}

	icon = getChild<LLIconCtrl>("power_partial_icon", false, false);
	if (icon && !icon->getImageName().empty())
	{
		mActionIcons["partial"] = icon->getImageName();
		removeChild(icon, true);
	}

	return LLPanelGroupTab::postBuild();
}

//static
void LLPanelGroupSubTab::onSearchKeystroke(LLLineEditor* caller, void* userdata)
{
	LLPanelGroupSubTab* self = (LLPanelGroupSubTab*)userdata;
	if (self)
	{
		self->handleSearchKeystroke(caller);
	}
}

void LLPanelGroupSubTab::handleSearchKeystroke(LLLineEditor* caller)
{
	if (!mSearchButton) return;

	if (caller->getText().size())
	{
		setDefaultBtn(mSearchButton);
		mSearchButton->setEnabled(true);
	}
	else
	{
		setDefaultBtn((LLButton*)NULL);
		mSearchButton->setEnabled(false);
	}
}

//static
void LLPanelGroupSubTab::onClickSearch(void* userdata)
{
	LLPanelGroupSubTab* self = (LLPanelGroupSubTab*)userdata;
	if (self)
	{
		self->handleClickSearch();
	}
}

void LLPanelGroupSubTab::handleClickSearch()
{
	if (!mSearchLineEditor || !mSearchButton || !mShowAllButton)
	{
		return;
	}

	if (mSearchLineEditor->getText().empty())
	{
		// No search text (this should not happen; the search button should
		// have been disabled).
		llwarns << "No search text !" << llendl;
		mSearchButton->setEnabled(false);
		return;
	}

	setSearchFilter(mSearchLineEditor->getText());
	mShowAllButton->setEnabled(true);
}

//static
void LLPanelGroupSubTab::onClickShowAll(void* userdata)
{
	LLPanelGroupSubTab* self = (LLPanelGroupSubTab*)userdata;
	if (self)
	{
		self->handleClickShowAll();
	}
}

void LLPanelGroupSubTab::handleClickShowAll()
{
	if (mShowAllButton)
	{
		setSearchFilter(LLStringUtil::null);
		mShowAllButton->setEnabled(false);
	}
}

void LLPanelGroupSubTab::setSearchFilter(const std::string& filter)
{
	LL_DEBUGS("GroupPanel") << "New search filter: '" << filter << "'"
							<< LL_ENDL;
	mSearchFilter = filter;
	LLStringUtil::toLower(mSearchFilter);
	update(GC_ALL);
}

void LLPanelGroupSubTab::activate()
{
	setOthersVisible(true);
}

void LLPanelGroupSubTab::deactivate()
{
	setOthersVisible(false);
}

void LLPanelGroupSubTab::setOthersVisible(bool b)
{
	if (mHeader)
	{
		mHeader->setVisible(b);
	}
	else
	{
		llwarns << "LLPanelGroupSubTab missing header !" << llendl;
	}

	if (mFooter)
	{
		mFooter->setVisible(b);
	}
	else
	{
		llwarns << "LLPanelGroupSubTab missing footer !" << llendl;
	}
}

bool LLPanelGroupSubTab::matchesActionSearchFilter(std::string action)
{
	// If the search filter is empty, everything passes.
	if (mSearchFilter.empty())
	{
		return true;
	}

	LLStringUtil::toLower(action);
	return action.find(mSearchFilter) != std::string::npos;
}

void LLPanelGroupSubTab::buildActionsList(LLScrollListCtrl* ctrl,
										  U64 allowed_by_some,
										  U64 allowed_by_all,
										  icon_map_t& icons,
										  void (*commit_callback)(LLUICtrl*, void*),
										  bool show_all,
										  bool filter,
										  bool is_owner_role)
{
	if (gGroupMgr.mRoleActionSets.empty())
	{
		llwarns << "Can't build action list - no actions found." << llendl;
		return;
	}

	mHasGroupBanPower = false;

	for (U32 i = 0, count = gGroupMgr.mRoleActionSets.size(); i < count; ++i)
	{
		buildActionCategory(ctrl, allowed_by_some, allowed_by_all,
							gGroupMgr.mRoleActionSets[i], icons,
							commit_callback, show_all, filter, is_owner_role);
	}
}

void LLPanelGroupSubTab::buildActionCategory(LLScrollListCtrl* ctrl,
											 U64 allowed_by_some,
											 U64 allowed_by_all,
											 LLRoleActionSet* action_set,
											 icon_map_t& icons,
											 void (*commit_callback)(LLUICtrl*, void*),
											 bool show_all,
											 bool filter,
											 bool is_owner_role)
{
	LL_DEBUGS("GroupPanel") << "Building role list for: "
							<< action_set->mActionSetData->mName << LL_ENDL;

	// See if the allow mask matches anything in this category.
	if (!show_all && !(allowed_by_some & action_set->mActionSetData->mPowerBit))
	{
		return;
	}

	// List all the actions in this category that at least some members have.
	LLSD row;
	LLSD& columns = row["columns"];

	columns[0]["column"] = "icon";
	icon_map_t::iterator iter = icons.find("folder");
	if (iter != icons.end())
	{
		columns[0]["type"] = "icon";
		columns[0]["value"] = iter->second;
	}

	columns[1]["column"] = "action";
	columns[1]["value"] = action_set->mActionSetData->mName;
	columns[1]["font-style"] = "BOLD";

	LLScrollListItem* title_row = ctrl->addElement(row, ADD_BOTTOM,
												   action_set->mActionSetData);

	bool category_matches_filter =
		!filter ||
		matchesActionSearchFilter(action_set->mActionSetData->mName);

	bool items_match_filter = false;
	bool can_change_actions = !is_owner_role &&
							  gAgent.hasPowerInGroup(mGroupID,
													 GP_ROLE_CHANGE_ACTIONS);

	for (U32 i = 0, count = action_set->mActions.size(); i < count; ++i)
	{
		LLRoleAction* rap = action_set->mActions[i];
		// See if anyone has these action.
		if (!show_all && !(allowed_by_some & rap->mPowerBit))
		{
			continue;
		}

		// See if we are filtering out these actions; if we are not using
		// filters, category_matches_filter will be true.
		if (!category_matches_filter &&
			!matchesActionSearchFilter(rap->mDescription))
		{
			continue;
		}

		items_match_filter = true;

		// See if everyone has these actions.
		bool show_full_strength = (allowed_by_some & rap->mPowerBit) ==
									(allowed_by_all & rap->mPowerBit);

		LLSD row;
		LLSD& columns = row["columns"];

		S32 column_index = 0;
		columns[column_index++]["column"] = "icon";

		S32 check_box_index = -1;

		if (commit_callback)
		{
			columns[column_index]["column"] = "checkbox";
			columns[column_index]["type"] = "checkbox";
			check_box_index = column_index++;
		}
		else if (show_full_strength)
		{
			icon_map_t::iterator iter = icons.find("full");
			if (iter != icons.end())
			{
				columns[column_index]["column"] = "checkbox";
				columns[column_index]["type"] = "icon";
				columns[column_index++]["value"] = iter->second;
			}
		}
		else
		{
			icon_map_t::iterator iter = icons.find("partial");
			if (iter != icons.end())
			{
				columns[column_index]["column"] = "checkbox";
				columns[column_index]["type"] = "icon";
				columns[column_index++]["value"] = iter->second;
			}
			row["enabled"] = false;
		}

		columns[column_index]["column"] = "action";
		columns[column_index]["value"] = rap->mDescription;
		columns[column_index]["font"] = "SANSSERIF_SMALL";

		if (mHasGroupBanPower)
		{
			// The ban ability is being set. Prevent these abilities from being
			// manipulated
			if (rap->mPowerBit == GP_MEMBER_EJECT ||
				rap->mPowerBit == GP_ROLE_REMOVE_MEMBER)
			{
				row["enabled"] = false;
			}
		}
		else
		{
			// The ban ability is not set. Allow these abilities to be
			// manipulated
			if (rap->mPowerBit == GP_MEMBER_EJECT ||
				rap->mPowerBit == GP_ROLE_REMOVE_MEMBER)
			{
					row["enabled"] = true;
			}
		}

		if (check_box_index == -1)
		{
			continue;
		}

		// Extract the checkbox that was created.
		LLScrollListItem* item = ctrl->addElement(row, ADD_BOTTOM, rap);
		LLScrollListCheck* check_cell =
			(LLScrollListCheck*)item->getColumn(check_box_index);
		LLCheckBoxCtrl* check = check_cell->getCheckBox();
		check->setEnabled(can_change_actions);
		check->setCommitCallback(commit_callback);
		check->setCallbackUserData(ctrl->getCallbackUserData());
		check->setToolTip(check->getLabel());

		if (show_all)
		{
			check->setTentative(false);
			check->set((allowed_by_some & rap->mPowerBit) != 0);
		}
		else
		{
			check->set(true);
			check->setTentative(!show_full_strength);
		}

		// Regardless of whether or not this ability is allowed by all or some,
		// we want to prevent the group managers from accidentally disabling
		// either of the 2 additional abilities tied with GP_GROUP_BAN_ACCESS.
		if ((allowed_by_all & GP_GROUP_BAN_ACCESS) == GP_GROUP_BAN_ACCESS ||
			(allowed_by_some & GP_GROUP_BAN_ACCESS) == GP_GROUP_BAN_ACCESS)
		{
			mHasGroupBanPower = true;
		}
	}

	if (!items_match_filter)
	{
		S32 title_index = ctrl->getItemIndex(title_row);
		ctrl->deleteSingleItem(title_index);
	}
}

void LLPanelGroupSubTab::setFooterEnabled(bool enable)
{
	if (mFooter)
	{
		mFooter->setAllChildrenEnabled(enable);
	}
}

////////////////////////////
// LLPanelGroupMembersSubTab
////////////////////////////

//static
void* LLPanelGroupMembersSubTab::createTab(void* data)
{
	LLUUID* group_id = (LLUUID*)data;
	return new LLPanelGroupMembersSubTab("panel group members sub tab",
										 *group_id);
}

LLPanelGroupMembersSubTab::LLPanelGroupMembersSubTab(const std::string& name,
													 const LLUUID& group_id)
: 	LLPanelGroupSubTab(name, group_id),
	mMembersList(NULL),
	mAssignedRolesList(NULL),
	mAllowedActionsList(NULL),
	mChanged(false),
	mPendingMemberUpdate(false),
	mUpdateInterval(0.5f),
	mSkipNextUpdate(false),
	mHasMatch(false),
	mNumOwnerAdditions(0)
{
}

bool LLPanelGroupMembersSubTab::postBuildSubTab(LLView* root)
{
	// Upcast parent so we can ask it for sibling controls.
	LLPanelGroupRoles* parent = (LLPanelGroupRoles*)root;

	mHeader = parent->getChild<LLPanel>("members_header", true, false);
	mFooter = parent->getChild<LLPanel>("members_footer", true, false);

	mMembersList = parent->getChild<LLNameListCtrl>("member_list",
													true, false);
	mAssignedRolesList =
		parent->getChild<LLScrollListCtrl>("member_assigned_roles", true,
										   false);
	mAllowedActionsList =
		parent->getChild<LLScrollListCtrl>("member_allowed_actions", true,
										   false);

	if (!mMembersList || !mAssignedRolesList || !mAllowedActionsList)
	{
		return false;
	}

	// We want to be notified whenever a member is selected.
	mMembersList->setCommitOnSelectionChange(true);
	mMembersList->setCommitCallback(onMemberSelect);
	// Show the member's profile on double click.
	mMembersList->setDoubleClickCallback(onMemberDoubleClick);
	mMembersList->setCallbackUserData(this);

	LLButton* button = parent->getChild<LLButton>("member_invite",
												  true, false);
	if (button)
	{
		button->setClickedCallback(onInviteMember);
		button->setCallbackUserData(this);
		button->setEnabled(gAgent.hasPowerInGroup(mGroupID, GP_MEMBER_INVITE));
	}

	mEjectBtn = parent->getChild<LLButton>("member_eject", true, false);
	if (mEjectBtn)
	{
		mEjectBtn->setClickedCallback(onEjectMembers);
		mEjectBtn->setCallbackUserData(this);
		mEjectBtn->setEnabled(false);
	}

	mBanBtn = parent->getChild<LLButton>("member_ban", true, false);
	if (mBanBtn)
	{
		mBanBtn->setClickedCallback(onBanMember);
		mBanBtn->setCallbackUserData(this);
		mBanBtn->setEnabled(false);
	}

	return true;
}

//static
void LLPanelGroupMembersSubTab::onMemberSelect(LLUICtrl* ctrl, void* userdata)
{
	LLPanelGroupMembersSubTab* self = (LLPanelGroupMembersSubTab*)userdata;
	if (self)
	{
		self->handleMemberSelect();
	}
}

void LLPanelGroupMembersSubTab::handleMemberSelect()
{
	if (!mAssignedRolesList || !mAllowedActionsList || !mMembersList)
	{
		return;
	}

	mAssignedRolesList->deleteAllItems();
	mAllowedActionsList->deleteAllItems();

	LLGroupMgrGroupData* gdatap = gGroupMgr.getGroupData(mGroupID);
	if (!gdatap)
	{
		llwarns << "No group data !" << llendl;
		return;
	}

	// Check if there is anything selected.
	std::vector<LLScrollListItem*> selection = mMembersList->getAllSelected();
	if (selection.empty())
	{
		return;
	}

	// Build a vector of all selected members, and gather allowed actions.
	uuid_vec_t selected_members;
	U64 allowed_by_all = GP_ALL_POWERS;
	U64 allowed_by_some = 0;
	for (U32 i = 0, count = selection.size(); i < count; ++i)
	{
		const LLUUID& item_id = selection[i]->getUUID();
		selected_members.emplace_back(item_id);

		// Get this member's power mask including any unsaved changes
		U64 powers = getAgentPowersBasedOnRoleChanges(item_id);
		allowed_by_all &= powers;
		allowed_by_some |= powers;
	}
	std::sort(selected_members.begin(), selected_members.end());

	// Build the allowed actions list.
	buildActionsList(mAllowedActionsList, allowed_by_some, allowed_by_all,
					 mActionIcons, NULL, false, false, false);

	// Build the assigned roles list.

	// Add each role to the assigned roles list.
	bool can_ban_members = gAgent.hasPowerInGroup(mGroupID,
												  GP_GROUP_BAN_ACCESS);
	bool can_eject_members = gAgent.hasPowerInGroup(mGroupID,
													GP_MEMBER_EJECT);
	bool member_is_owner = false;
	for (LLGroupMgrGroupData::role_list_t::iterator
			iter = gdatap->mRoles.begin(), end = gdatap->mRoles.end();
		 iter != end; ++iter)
	{
		// Count how many selected users are in this role.
		const LLUUID& role_id = iter->first;
		LLGroupRoleData* group_role_data = iter->second;
		if (!group_role_data)
		{
			// This could happen if changes are not synced right on sub-panel
			// change.
			llwarns << "No group role data for " << iter->second << llendl;
			continue;
		}

		constexpr bool needs_sort = false;
		S32 count = group_role_data->getMembersInRole(selected_members,
													  needs_sort);
		// Check if the user has permissions to assign/remove members to/from
		// the role (but the ability to add/remove should only be based on the
		// "saved" changes to the role not in the temp/meta data. -jwolk
		bool cb_enable = count > 0 ? agentCanRemoveFromRole(mGroupID, role_id)
								   : gGroupMgr.agentCanAddToRole(mGroupID,
																 role_id);
		// Owner role has special enabling permissions for removal.
		if (cb_enable && count > 0 && role_id == gdatap->mOwnerRole)
		{
			// Check if any owners besides this agent are selected.
			for (U32 i = 0, mcount = selected_members.size(); i < mcount; ++i)
			{
				const LLUUID& mid = selected_members[i];
				// Do not count the agent.
				if (mid == gAgentID)
				{
					continue;
				}

				// Look up the member data.
				LLGroupMgrGroupData::member_list_t::iterator mi =
					gdatap->mMembers.find(mid);
				if (mi == gdatap->mMembers.end())
				{
					continue;
				}
				LLGroupMemberData* member_data = mi->second;

				// Is the member an owner ?
				if (member_data && member_data->isInRole(gdatap->mOwnerRole))
				{
					// Cannot remove other owners.
					cb_enable = can_ban_members = false;
					break;
				}
			}
		}

		// Now see if there are any role changes for the selected members and
		// remember to include them
		for (U32 i = 0, mcount = selected_members.size(); i < mcount; ++i)
		{
			const LLUUID& mid = selected_members[i];
			LLRoleMemberChangeType type;
			if (getRoleChangeType(mid, role_id, type))
			{
				if (type == RMC_ADD)
				{
					++count;
				}
				else if (type == RMC_REMOVE)
				{
					--count;
				}
			}
		}

		// If anyone selected is in any role besides 'Everyone' then they
		// cannot be ejected.
		if (count > 0 && role_id.notNull())
		{
			can_eject_members = false;
			if (role_id == gdatap->mOwnerRole)
			{
				member_is_owner = true;
			}
		}

		LLRoleData rd;
		if (gdatap->getRoleData(role_id, rd))
		{
			std::ostringstream label;
			label << rd.mRoleName;
			// Do not bother showing a count, if there is only 0 or 1.
			if (count > 1)
			{
				label << ": " << count;
			}

			LLSD row;
			row["id"] = role_id;
			LLSD& columns = row["columns"];

			columns[0]["column"] = "checkbox";
			columns[0]["type"] = "checkbox";

			columns[1]["column"] = "role";
			columns[1]["value"] = label.str();

			if (row["id"].asUUID().isNull())
			{
				// This is the everyone role, you cannot take people out of the
				// "Everyone" role !
				row["enabled"] = false;
			}

			LLScrollListItem* item = mAssignedRolesList->addElement(row);

			// Extract the checkbox that was created.
			LLScrollListCheck* check_cell;
			check_cell = (LLScrollListCheck*)item->getColumn(0);
			LLCheckBoxCtrl* check = check_cell->getCheckBox();
			check->setCommitCallback(onRoleCheck);
			check->setCallbackUserData(this);
			check->set(count > 0);
			check->setTentative(count != 0 &&
								selected_members.size() != (uuid_vec_t::size_type)count);

			// NOTE: as of right now a user can break the group by removing
			// themselves from a role if he is the last owner. We should
			// check for this special case -jwolk
			check->setEnabled(cb_enable);
		}
	}
	mAssignedRolesList->setEnabled(true);

	if (gAgent.isGodlikeWithoutAdminMenuFakery())
	{
		can_eject_members = true;
#if 0
		can_ban_members = true;
#endif
	}

	if (!can_eject_members && !member_is_owner)
	{
		// Maybe we can eject them because we are an owner...
		LLGroupMgrGroupData::member_list_t::iterator mi;
		mi = gdatap->mMembers.find(gAgentID);
		if (mi != gdatap->mMembers.end())
		{
			LLGroupMemberData* member_data = mi->second;
			if (member_data && member_data->isInRole(gdatap->mOwnerRole))
			{
				can_eject_members = true;
#if 0
				can_ban_members = true;
#endif
			}
		}
	}

	// ... or we can eject them because we have all the requisite powers...
	if (!member_is_owner &&
		gAgent.hasPowerInGroup(mGroupID, GP_ROLE_REMOVE_MEMBER))
	{
		if (gAgent.hasPowerInGroup(mGroupID, GP_MEMBER_EJECT))
		{
			can_eject_members = true;
		}
		if (gAgent.hasPowerInGroup(mGroupID, GP_GROUP_BAN_ACCESS))
		{
			can_ban_members = true;
		}
	}

	for (uuid_vec_t::const_iterator it = selected_members.begin(),
									end = selected_members.end();
		 it != end; ++it)
	{
		// Do not count the agent.
		if (*it == gAgentID)
		{
			can_eject_members = false;
			can_ban_members = false;
		}
	}

	if (mBanBtn)
	{
		mBanBtn->setEnabled(can_ban_members);
	}
	if (mEjectBtn)
	{
		mEjectBtn->setEnabled(can_eject_members);
	}
}

//static
void LLPanelGroupMembersSubTab::onMemberDoubleClick(void* userdata)
{
	LLPanelGroupMembersSubTab* self = (LLPanelGroupMembersSubTab*)userdata;
	if (self && self->mMembersList)
	{
		LLScrollListItem* selected = self->mMembersList->getFirstSelected();
		if (selected)
		{
			LLFloaterAvatarInfo::showFromDirectory(selected->getUUID());
		}
	}
}

//static
void LLPanelGroupMembersSubTab::onInviteMember(void* userdata)
{
	LLPanelGroupMembersSubTab* self = (LLPanelGroupMembersSubTab*)userdata;
	if (self)
	{
		LLFloaterGroupInvite::showForGroup(self->mGroupID, NULL, self);
	}
}

void LLPanelGroupMembersSubTab::onEjectMembers(void* userdata)
{
	LLPanelGroupMembersSubTab* self = (LLPanelGroupMembersSubTab*)userdata;
	if (self)
	{
		self->handleEjectMembers();
	}
}

// Sends an eject message
void LLPanelGroupMembersSubTab::handleEjectMembers()
{
	if (!mMembersList) return;

	std::vector<LLScrollListItem*> selection = mMembersList->getAllSelected();
	if (selection.empty()) return;

	uuid_vec_t selected_members;
	for (U32 i = 0, count = selection.size(); i < count; ++i)
	{
		selected_members.emplace_back(selection[i]->getUUID());
	}

	mMembersList->deleteSelectedItems();

	gGroupMgr.sendGroupMemberEjects(mGroupID, selected_members);
}

void LLPanelGroupMembersSubTab::handleRoleCheck(const LLUUID& role_id,
												LLRoleMemberChangeType type)
{
	if (!mMembersList) return;

	std::vector<LLScrollListItem*> selection = mMembersList->getAllSelected();
	if (selection.empty())
	{
		return;
	}

	LLGroupMgrGroupData* gdatap = gGroupMgr.getGroupData(mGroupID);
	if (!gdatap)
	{
		return;
	}

	// Add that the user is requesting to change the roles for selected members
	U64 powers_all_have = GP_ALL_POWERS;
	U64 powers_some_have = 0;

	bool is_owner_role = gdatap->mOwnerRole == role_id;

	for (U32 i = 0, count = selection.size(); i < count; ++i)
	{
		const LLUUID& mid = selection[i]->getUUID();

		// See if we requested a change for this member before
		if (!mMemberRoleChangeData.count(mid))
		{
			mMemberRoleChangeData[mid] = new role_change_data_map_t;
		}
		role_change_data_map_t* rc_datap = mMemberRoleChangeData[mid];

		// Now check to see if the selected group member had changed their
		// association with the selected role before

		role_change_data_map_t::iterator role = rc_datap->find(role_id);
		if (role != rc_datap->end())
		{
			// See if the new change type cancels out the previous change
			if (role->second != type)
			{
				rc_datap->erase(role_id);
				if (is_owner_role)
				{
					--mNumOwnerAdditions;
				}
			}
			// Else do nothing

			if (rc_datap->empty())
			{
				// The current member now has no role changes, so erase the
				// role change and erase the member's entry
				delete rc_datap;
                rc_datap = NULL;

				mMemberRoleChangeData.erase(mid);
			}
		}
		else
		{
			// A previously unchanged role is being changed
			(*rc_datap)[role_id] = type;
			if (is_owner_role && type == RMC_ADD)
			{
				++mNumOwnerAdditions;
			}
		}

		// We need to calculate what powers the selected members have
		// (including the role changes we are making) so that we can rebuild
		// the action list
		U64 new_powers = getAgentPowersBasedOnRoleChanges(mid);

		powers_all_have  &= new_powers;
		powers_some_have |= new_powers;
	}

	mChanged = !mMemberRoleChangeData.empty();
	notifyObservers();

	// Now we need to update the actions list to reflect the changes
	mAllowedActionsList->deleteAllItems();
	buildActionsList(mAllowedActionsList, powers_some_have, powers_all_have,
					 mActionIcons, NULL, false, false, false);
}

//static
void LLPanelGroupMembersSubTab::onRoleCheck(LLUICtrl* ctrl, void* userdata)
{
	LLPanelGroupMembersSubTab* self = (LLPanelGroupMembersSubTab*)userdata;
	LLCheckBoxCtrl* check_box = (LLCheckBoxCtrl*)ctrl;
	if (!check_box || !self || !self->mAssignedRolesList)
	{
		return;
	}

	LLScrollListItem* itemp = self->mAssignedRolesList->getFirstSelected();
	if (itemp)
	{
		self->handleRoleCheck(itemp->getUUID(),
							  check_box->get() ? RMC_ADD : RMC_REMOVE);
	}
}

void LLPanelGroupMembersSubTab::activate()
{
	LLPanelGroupSubTab::activate();

	LLGroupMgrGroupData* gdatap = gGroupMgr.getGroupData(mGroupID);
	if (!gdatap || !gdatap->isMemberDataComplete())
	{
		gGroupMgr.sendCapGroupMembersRequest(mGroupID);
	}

	if (!gdatap || !gdatap->isRoleMemberDataComplete())
	{
		gGroupMgr.sendGroupRoleMembersRequest(mGroupID);
	}

	update(GC_ALL);
}

void LLPanelGroupMembersSubTab::deactivate()
{
	LLPanelGroupSubTab::deactivate();
}

bool LLPanelGroupMembersSubTab::needsApply(std::string&)
{
	return mChanged;
}

void LLPanelGroupMembersSubTab::cancel()
{
	if (!mChanged)
	{
		return;	// Nothing to do !
	}

	for (member_role_changes_map_t::iterator
			it = mMemberRoleChangeData.begin(),
			end = mMemberRoleChangeData.end();
		 it != end; ++it)
	{
		delete it->second;
	}
	mMemberRoleChangeData.clear();

	mChanged = false;
	notifyObservers();
}

bool LLPanelGroupMembersSubTab::apply(std::string& mesg)
{
	LLGroupMgrGroupData* gdatap = gGroupMgr.getGroupData(mGroupID);
	if (!gdatap)
	{
		llwarns << "Unable to get group data for group " << mGroupID << llendl;

		mesg.assign("Unable to save member data. Try again later.");
		return false;
	}

	if (!mChanged)
	{
		return true;
	}

	// Figure out if we are somehow adding an owner or not and alert the user.
	// Possibly make it ignorable.
	if (!mNumOwnerAdditions)
	{
		applyMemberChanges();
		return true;
	}

	LLRoleData rd;
	if (!gdatap->getRoleData(gdatap->mOwnerRole, rd))
	{
		llwarns << "Unable to get role information for the owner role in group "
				<< mGroupID << llendl;
		// *TODO: translate
		mesg.assign("Unable to retried specific group information. Try again later");
		return false;
	}

	LLSD args;
	mHasModal = true;
	args["ROLE_NAME"] = rd.mRoleName;
	gNotifications.add("AddGroupOwnerWarning", args, LLSD(),
					   boost::bind(&LLPanelGroupMembersSubTab::addOwnerCB,
								   this, _1, _2));
	return true;
}

bool LLPanelGroupMembersSubTab::addOwnerCB(const LLSD& notification,
										   const LLSD& response)
{
	mHasModal = false;
	if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		// User clicked "Yes"
		applyMemberChanges();
	}
	return false;
}

void LLPanelGroupMembersSubTab::applyMemberChanges()
{
	// Sucks to do a find again here, but it is in constant time, so, could be
	// worse
	LLGroupMgrGroupData* gdatap = gGroupMgr.getGroupData(mGroupID);
	if (!gdatap)
	{
		llwarns << "Unable to get group data for group " << mGroupID << llendl;
		return;
	}

	// We need to add all of the changed roles data for each member whose role
	// changed
	for (member_role_changes_map_t::iterator mit = mMemberRoleChangeData.begin();
		 mit != mMemberRoleChangeData.end(); ++mit)
	{
		for (role_change_data_map_t::iterator rit = mit->second->begin();
			 rit != mit->second->end(); ++rit)
		{
			gdatap->changeRoleMember(rit->first,	// role_id
									 mit->first,	// member_id
									 rit->second);	// add/remove
		}

		mit->second->clear();
		delete mit->second;
	}
	mMemberRoleChangeData.clear();

	gGroupMgr.sendGroupRoleMemberChanges(mGroupID);
	// Force an UI update
	handleMemberSelect();

	mChanged = false;
	mNumOwnerAdditions = 0;
	notifyObservers();
}

bool LLPanelGroupMembersSubTab::matchesSearchFilter(std::string fullname)
{
	// If the search filter is empty, everything passes.
	if (mSearchFilter.empty())
	{
		return true;
	}

	// Compare full name to the search filter.
	LLStringUtil::toLower(fullname);
	return fullname.find(mSearchFilter) != std::string::npos;
}

U64 LLPanelGroupMembersSubTab::getAgentPowersBasedOnRoleChanges(const LLUUID& agent_id)
{
	// We loop over all of the changes if we are adding a role, then we simply
	// add the role's powers, if we are removing a role, we store that role Id
	// away and then we have to build the powers up bases on the roles the
	// agent is in.

	LLGroupMgrGroupData* gdatap = gGroupMgr.getGroupData(mGroupID);
	if (!gdatap)
	{
		llwarns << "No group data !" << llendl;
		return GP_NO_POWERS;
	}

	LLGroupMemberData* member_data = gdatap->mMembers[agent_id];
	if (!member_data)
	{
		llwarns << "No member data for member with UUID " << agent_id
				<< llendl;
		return GP_NO_POWERS;
	}

	// See if there are unsaved role changes for this agent
	role_change_data_map_t* rc_datap = NULL;
	member_role_changes_map_t::iterator member =
		mMemberRoleChangeData.find(agent_id);
	if (member != mMemberRoleChangeData.end())
	{
		// this member has unsaved role changes, so grab them
		rc_datap = member->second;
	}

	U64 new_powers = GP_NO_POWERS;

	if (rc_datap)
	{
		uuid_list_t roles_to_be_removed;
		for (role_change_data_map_t::iterator it = rc_datap->begin(),
											  end = rc_datap->end();
			 it != end; ++it)
		{
			if (it->second == RMC_ADD)
			{
				new_powers |= gdatap->getRolePowers(it->first);
			}
			else
			{
				roles_to_be_removed.insert(it->first);
			}
		}

		// Loop over the member's current roles, summing up the powers (not
		// including the role we are removing)
		for (LLGroupMemberData::role_list_t::iterator
				it = member_data->roleBegin(), end = member_data->roleEnd();
			 it != end; ++it)
		{
			if (!roles_to_be_removed.count(it->second->getID()))
			{
				new_powers |= it->second->getRoleData().mRolePowers;
			}
		}
	}
	else
	{
		// There is no change for this member the member's powers are just the
		// ones stored in the group manager
		new_powers = member_data->getAgentPowers();
	}

	return new_powers;
}

// If there is no change, returns false be sure to verify that there is a role
// change before attempting to get it or else the data will make no sense.
// Stores the role change type.
bool LLPanelGroupMembersSubTab::getRoleChangeType(const LLUUID& member_id,
												  const LLUUID& role_id,
												  LLRoleMemberChangeType& type)
{
	member_role_changes_map_t::iterator mit =
		mMemberRoleChangeData.find(member_id);
	if (mit != mMemberRoleChangeData.end())
	{
		role_change_data_map_t::iterator rit = mit->second->find(role_id);
		if (rit != mit->second->end())
		{
			type = rit->second;
			return true;
		}
	}
	return false;
}

void LLPanelGroupMembersSubTab::draw()
{
	// Do not update every frame: that would be insane !
	if (mSkipNextUpdate)
	{
		// Compute the time the viewer took to 'digest' the update and come
		// back to us; the name list update takes time, and the avatar name
		// query takes even more time when the name is not cached !
		mUpdateInterval = (mUpdateInterval +
						   3.f * mUpdateTimer.getElapsedTimeF32()) * 0.5f;
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

	LLPanelGroupSubTab::draw();
}

void LLPanelGroupMembersSubTab::update(LLGroupChange gc)
{
	if (mGroupID.isNull() || !mMembersList) return;

	if (gc == GC_TITLES || gc == GC_PROPERTIES)
	{
		// Do not care about title or general group properties updates.
		return;
	}

	LLGroupMgrGroupData* gdatap = gGroupMgr.getGroupData(mGroupID);
	if (!gdatap)
	{
		llwarns << "No group data !" << llendl;
		return;
	}

	// Rebuild the members list.
	mMembersList->deleteAllItems();

	// Wait for both all data to be retrieved before displaying anything.
	if (gdatap->isMemberDataComplete() && gdatap->isRoleDataComplete() &&
		gdatap->isRoleMemberDataComplete())
	{
		mMemberProgress = gdatap->mMembers.begin();
		mPendingMemberUpdate = true;
		mHasMatch = false;
	}
	else
	{
		// Build a string with info on retrieval progress.
		std::ostringstream retrieved;
		if (gdatap->isRoleDataComplete() &&
			gdatap->isMemberDataComplete() && !gdatap->mMembers.size())
		{
			// MAINT-5237
			retrieved << "Member list not available.";
		}
		else if (!gdatap->isMemberDataComplete())
		{
			// Still busy retreiving member list.
			retrieved << "Retrieving member list (" << gdatap->mMembers.size()
					  << " / " << gdatap->mMemberCount << ")...";
		}
		else if (!gdatap->isRoleDataComplete())
		{
			// Still busy retreiving role list.
			retrieved << "Retrieving role list (" << gdatap->mRoles.size()
					  << " / " << gdatap->mRoleCount << ")...";
		}
		else // (!gdatap->isRoleMemberDataComplete())
		{
			// Still busy retreiving role/member mappings.
			retrieved << "Retrieving role member mappings...";
		}
		mMembersList->setEnabled(false);
		mMembersList->addCommentText(retrieved.str());
	}
}

void LLPanelGroupMembersSubTab::updateMembers()
{
	if (!mMembersList) return;

	mPendingMemberUpdate = false;

	LLGroupMgrGroupData* gdatap = gGroupMgr.getGroupData(mGroupID);
	if (!gdatap)
	{
		llwarns << "No group data !" << llendl;
		return;
	}

	// Make sure all data is still complete. Incomplete data may occur if we
	// refresh.
	if (!gdatap->isMemberDataComplete() || !gdatap->isRoleDataComplete() ||
		!gdatap->isRoleMemberDataComplete())
	{
		return;
	}

	mMembersList->setAllowRefresh(false);
	mMembersList->setLazyUpdateInterval(5.f);

	LLGroupMgrGroupData::member_list_t::iterator end = gdatap->mMembers.end();
	U32 i;
	LLTimer update_time;
	update_time.setTimerExpirySec(UPDATE_MEMBERS_SECONDS_PER_FRAME);
	for (i = 0; mMemberProgress != end && !update_time.hasExpired();
		 ++mMemberProgress, ++i)
	{
		if (!mMemberProgress->second) continue;
		// Do filtering on name if it is already in the cache.
		bool add_member = true;

		LLAvatarName av_name;
		if (LLAvatarNameCache::get(mMemberProgress->first, &av_name))
		{
			// We are only using legacy names here
			std::string fullname = av_name.getLegacyName();
			if (!matchesSearchFilter(fullname))
			{
				add_member = false;
			}
		}

		if (add_member)
		{
			// Build the donated tier string.
			std::ostringstream donated;
			donated << mMemberProgress->second->getContribution() << " m2";

			LLSD row;
			row["id"] = mMemberProgress->first;
			LLSD& columns = row["columns"];

			columns[0]["column"] = "name";
			// value is filled in by name list control

			columns[1]["column"] = "donated";
			columns[1]["value"] = donated.str();

			columns[2]["column"] = "online";
			columns[2]["value"] = mMemberProgress->second->getOnlineStatus();
			columns[2]["font"] = "SANSSERIF_SMALL";

			mMembersList->addElement(row);
			mHasMatch = true;
		}
	}

	if (mMemberProgress == end)
	{
		if (mHasMatch)
		{
			mMembersList->setEnabled(true);
		}
		else if (gdatap->mMembers.size())
		{
			mMembersList->setEnabled(false);
			mMembersList->addCommentText(std::string("No match."));
		}
		mMembersList->setAllowRefresh(true);
		mMembersList->setLazyUpdateInterval(1.f);
		LL_DEBUGS("GroupPanel") << i
								<< " members added to the list. No more member pending."
								<< LL_ENDL;
	}
	else
	{
		LL_DEBUGS("GroupPanel") << i
								<< " members added to the list. There are still pending members."
								<< LL_ENDL;
		mPendingMemberUpdate = true;
	}

	// This should clear the other two lists, since nothing is selected.
	handleMemberSelect();
}

void LLPanelGroupMembersSubTab::onBanMember(void* userdata)
{
	LLPanelGroupMembersSubTab* self = (LLPanelGroupMembersSubTab*)userdata;
	if (self)
	{
		self->handleBanMember();
	}
}

void LLPanelGroupMembersSubTab::handleBanMember()
{
	LLGroupMgrGroupData* gdatap = gGroupMgr.getGroupData(mGroupID);
	if (!gdatap)
	{
		llwarns << "Unable to get group data for group " << mGroupID << llendl;
		return;
	}

	std::vector<LLScrollListItem*> selection = mMembersList->getAllSelected();
	if (selection.empty())
	{
		return;
	}

	uuid_vec_t ban_ids;
	for (U32 i = 0, count = selection.size(); i < count; ++i)
	{
		const LLUUID& ban_id = selection[i]->getUUID();
		ban_ids.emplace_back(ban_id);

		LLGroupBanData ban_data;
		gdatap->createBanEntry(ban_id, ban_data);
	}
	gGroupMgr.sendGroupBanRequest(LLGroupMgr::REQUEST_POST, mGroupID,
								  LLGroupMgr::BAN_CREATE, ban_ids);
	handleEjectMembers();
}

////////////////////////////
// LLPanelGroupRolesSubTab
////////////////////////////

//static
void* LLPanelGroupRolesSubTab::createTab(void* data)
{
	LLUUID* group_id = (LLUUID*)data;
	return new LLPanelGroupRolesSubTab("panel group roles sub tab", *group_id);
}

LLPanelGroupRolesSubTab::LLPanelGroupRolesSubTab(const std::string& name,
												 const LLUUID& group_id)
:	LLPanelGroupSubTab(name, group_id),
	mRolesList(NULL),
	mAssignedMembersList(NULL),
	mAllowedActionsList(NULL),
	mRoleName(NULL),
	mRoleTitle(NULL),
	mRoleDescription(NULL),
	mMemberVisibleCheck(NULL),
	mDeleteRoleButton(NULL),
	mCreateRoleButton(NULL),
	mFirstOpen(true),
	mHasRoleChange(false)
{
}

bool LLPanelGroupRolesSubTab::postBuildSubTab(LLView* root)
{
	// Upcast parent so we can ask it for sibling controls.
	LLPanelGroupRoles* parent = (LLPanelGroupRoles*)root;
	if (!parent) return false;

	mHeader = parent->getChild<LLPanel>("roles_header", true, false);
	mFooter = parent->getChild<LLPanel>("roles_footer", true, false);

	mRolesList = parent->getChild<LLScrollListCtrl>("role_list", true, false);
	mAssignedMembersList =
		parent->getChild<LLNameListCtrl>("role_assigned_members", true, false);
	mAllowedActionsList	=
		parent->getChild<LLScrollListCtrl>("role_allowed_actions", true,
										   false);

	mRoleName = parent->getChild<LLLineEditor>("role_name", true, false);
	mRoleTitle = parent->getChild<LLLineEditor>("role_title", true, false);
	mRoleDescription = parent->getChild<LLTextEditor>("role_description",
													  true, false);

	mMemberVisibleCheck =
		parent->getChild<LLCheckBoxCtrl>("role_visible_in_list", true, false);

	if (!mRolesList || !mAssignedMembersList || !mAllowedActionsList ||
		!mRoleName || !mRoleTitle || !mRoleDescription || !mMemberVisibleCheck)
	{
		llwarns << "Missing UI element(s). Aborting panel build." << llendl;
		return false;
	}

	mRemoveEveryoneTxt = getString("cant_delete_role");

	mCreateRoleButton = parent->getChild<LLButton>("role_create", true, false);
	if (mCreateRoleButton)
	{
		mCreateRoleButton->setCallbackUserData(this);
		mCreateRoleButton->setClickedCallback(onCreateRole);
		mCreateRoleButton->setEnabled(false);
	}

	mDeleteRoleButton = parent->getChild<LLButton>("role_delete", true, false);
	if (mDeleteRoleButton)
	{
		mDeleteRoleButton->setCallbackUserData(this);
		mDeleteRoleButton->setClickedCallback(onDeleteRole);
		mDeleteRoleButton->setEnabled(false);
	}

	// Show the member's profile on double click.
	mAssignedMembersList->setDoubleClickCallback(onAssignedMemberDoubleClick);
	mAssignedMembersList->setCallbackUserData(this);

	mRolesList->setCommitOnSelectionChange(true);
	mRolesList->setCommitCallback(onRoleSelect);
	mRolesList->setCallbackUserData(this);

	mMemberVisibleCheck->setCommitCallback(onMemberVisibilityChange);
	mMemberVisibleCheck->setCallbackUserData(this);

	mAllowedActionsList->setCommitOnSelectionChange(true);
	mAllowedActionsList->setCallbackUserData(this);

	mRoleName->setCommitOnFocusLost(true);
	mRoleName->setKeystrokeCallback(onPropertiesKey);
	mRoleName->setCallbackUserData(this);

	mRoleTitle->setCommitOnFocusLost(true);
	mRoleTitle->setKeystrokeCallback(onPropertiesKey);
	mRoleTitle->setCallbackUserData(this);

	mRoleDescription->setCommitOnFocusLost(true);
	mRoleDescription->setCommitCallback(onDescriptionCommit);
	mRoleDescription->setCallbackUserData(this);
	mRoleDescription->setFocusReceivedCallback(onDescriptionFocus, this);

	setFooterEnabled(false);

	return true;
}

void LLPanelGroupRolesSubTab::activate()
{
	LLPanelGroupSubTab::activate();

	if (mRolesList) mRolesList->deselectAllItems();
	if (mAssignedMembersList) mAssignedMembersList->deleteAllItems();
	if (mAllowedActionsList) mAllowedActionsList->deleteAllItems();
	if (mRoleName) mRoleName->clear();
	if (mRoleDescription) mRoleDescription->clear();
	if (mRoleTitle) mRoleTitle->clear();

	setFooterEnabled(false);

	mHasRoleChange = false;
	update(GC_ALL);
}

void LLPanelGroupRolesSubTab::deactivate()
{
	LLPanelGroupSubTab::deactivate();
	mFirstOpen = false;
}

bool LLPanelGroupRolesSubTab::needsApply(std::string& mesg)
{
	LLGroupMgrGroupData* gdatap = gGroupMgr.getGroupData(mGroupID);

	return mHasRoleChange ||	// Text changed in current role...
		   // ... or pending role changed in the group
		   (gdatap && gdatap->pendingRoleChanges());
}

bool LLPanelGroupRolesSubTab::apply(std::string& mesg)
{
	saveRoleChanges();
	mFirstOpen = false;
	gGroupMgr.sendGroupRoleChanges(mGroupID);

	notifyObservers();

	return true;
}

void LLPanelGroupRolesSubTab::cancel()
{
	mHasRoleChange = false;
	gGroupMgr.cancelGroupRoleChanges(mGroupID);

	notifyObservers();
}

LLSD LLPanelGroupRolesSubTab::createRoleItem(const LLUUID& role_id,
											 std::string name,
											 std::string title,
											 S32 members)
{
	LLSD row;
	row["id"] = role_id;

	LLSD& columns = row["columns"];

	columns[0]["column"] = "name";
	columns[0]["value"] = name;

	columns[1]["column"] = "title";
	columns[1]["value"] = title;

	columns[2]["column"] = "members";
	columns[2]["value"] = members;

	return row;
}

bool LLPanelGroupRolesSubTab::matchesSearchFilter(std::string rolename,
												  std::string roletitle)
{
	// If the search filter is empty, everything passes.
	if (mSearchFilter.empty()) return true;

	LLStringUtil::toLower(rolename);
	LLStringUtil::toLower(roletitle);
	std::string::size_type match_name = rolename.find(mSearchFilter);
	std::string::size_type match_title = roletitle.find(mSearchFilter);

	if (std::string::npos == match_name && std::string::npos == match_title)
	{
		// not found
		return false;
	}
	else
	{
		return true;
	}
}

void LLPanelGroupRolesSubTab::update(LLGroupChange gc)
{
	if (mGroupID.isNull() || !mRolesList) return;

	LLGroupMgrGroupData* gdatap = gGroupMgr.getGroupData(mGroupID);
	if (!gdatap || !gdatap->isRoleDataComplete())
	{
		gGroupMgr.sendGroupRoleDataRequest(mGroupID);
	}
	else
	{
		bool had_selection = false;
		LLUUID last_selected;
		if (mRolesList->getFirstSelected())
		{
			last_selected = mRolesList->getFirstSelected()->getUUID();
			had_selection = true;
		}
		mRolesList->deleteAllItems();

		LLScrollListItem* item = NULL;

		LLGroupMgrGroupData::role_list_t::iterator rit = gdatap->mRoles.begin();
		LLGroupMgrGroupData::role_list_t::iterator end = gdatap->mRoles.end();
		for ( ; rit != end; ++rit)
		{
			LLRoleData rd;
			if (gdatap->getRoleData(rit->first,rd))
			{
				if (matchesSearchFilter(rd.mRoleName, rd.mRoleTitle))
				{
					// If this is the everyone role, then EVERYONE is in it.
					S32 members_in_role = rit->first.isNull() ? gdatap->mMembers.size()
															  : rit->second->getTotalMembersInRole();
					LLSD row = createRoleItem(rit->first, rd.mRoleName,
											  rd.mRoleTitle, members_in_role);
					item = mRolesList->addElement(row,
												  rit->first.isNull() ? ADD_TOP
																	  : ADD_BOTTOM,
												  this);
					if (had_selection && (rit->first == last_selected))
					{
						item->setSelected(true);
					}
				}
			}
			else
			{
				llwarns << "No role data for role " << rit->first << llendl;
			}
		}

		mRolesList->sortByColumn(std::string("name"), true);

		if (mCreateRoleButton)
		{
			if (gdatap->mRoles.size() < (U32)MAX_ROLES &&
				gAgent.hasPowerInGroup(mGroupID, GP_ROLE_CREATE))
			{
				mCreateRoleButton->setEnabled(true);
			}
			else
			{
				mCreateRoleButton->setEnabled(false);
			}
		}

		if (had_selection)
		{
			handleRoleSelect();
		}
		else
		{
			if (mAssignedMembersList) mAssignedMembersList->deleteAllItems();
			if (mAllowedActionsList) mAllowedActionsList->deleteAllItems();
			if (mRoleName) mRoleName->clear();
			if (mRoleDescription) mRoleDescription->clear();
			if (mRoleTitle) mRoleTitle->clear();
			setFooterEnabled(false);
			if (mDeleteRoleButton) mDeleteRoleButton->setEnabled(false);
		}
	}
#if 0
	if (!mFirstOpen)
	{
		if (!gdatap || !gdatap->isMemberDataComplete())
		{
			gGroupMgr.sendCapGroupMembersRequest(mGroupID);
		}

		if (!gdatap || !gdatap->isRoleMemberDataComplete())
		{
			gGroupMgr.sendGroupRoleMembersRequest(mGroupID);
		}
	}
#endif
	if ((GC_ROLE_MEMBER_DATA == gc || GC_MEMBER_DATA == gc) &&
		gdatap && gdatap->isMemberDataComplete() &&
		gdatap->isRoleMemberDataComplete())
	{
		buildMembersList();
	}
}

//static
void LLPanelGroupRolesSubTab::onAssignedMemberDoubleClick(void* userdata)
{
	LLPanelGroupRolesSubTab* self = (LLPanelGroupRolesSubTab*)userdata;
	if (self && self->mAssignedMembersList)
	{
		LLScrollListItem* selected =
			self->mAssignedMembersList->getFirstSelected();
		if (selected)
		{
			LLFloaterAvatarInfo::showFromDirectory(selected->getUUID());
		}
	}
}

//static
void LLPanelGroupRolesSubTab::onRoleSelect(LLUICtrl* ctrl, void* userdata)
{
	LLPanelGroupRolesSubTab* self = (LLPanelGroupRolesSubTab*)userdata;
	if (self)
	{
		self->handleRoleSelect();
	}
}

void LLPanelGroupRolesSubTab::handleRoleSelect()
{
	if (!mAssignedMembersList || !mAllowedActionsList) return;

	mAssignedMembersList->deleteAllItems();
	mAllowedActionsList->deleteAllItems();

	LLGroupMgrGroupData* gdatap = gGroupMgr.getGroupData(mGroupID);
	if (!gdatap)
	{
		llwarns << "No group data !" << llendl;
		return;
	}

	saveRoleChanges();

	// Check if there is anything selected.
	LLScrollListItem* item = mRolesList->getFirstSelected();
	if (!item)
	{
		setFooterEnabled(false);
		return;
	}

	setFooterEnabled(true);

	bool can_delete = true;
	LLRoleData rd;
	if (gdatap->getRoleData(item->getUUID(), rd))
	{
		bool is_owner_role = (gdatap->mOwnerRole == item->getUUID());
		if (is_owner_role)
		{
			// you can't delete the owner role
			can_delete = false;
		}

		if (mRoleName)
		{
			mRoleName->setText(rd.mRoleName);
			mRoleName->setEnabled(!is_owner_role &&
								  gAgent.hasPowerInGroup(mGroupID,
														 GP_ROLE_PROPERTIES));
		}
		if (mRoleTitle)
		{
			mRoleTitle->setText(rd.mRoleTitle);
			mRoleTitle->setEnabled(gAgent.hasPowerInGroup(mGroupID,
														  GP_ROLE_PROPERTIES));
		}
		if (mRoleDescription)
		{
			mRoleDescription->setText(rd.mRoleDescription);
			mRoleDescription->setEnabled(gAgent.hasPowerInGroup(mGroupID,
																GP_ROLE_PROPERTIES));
		}

		if (mAllowedActionsList)
		{
			mAllowedActionsList->setEnabled(gAgent.hasPowerInGroup(mGroupID,
																   GP_ROLE_CHANGE_ACTIONS));
			buildActionsList(mAllowedActionsList, rd.mRolePowers, 0LL,
							 mActionIcons, onActionCheck, true, false,
							 is_owner_role);
		}

		if (mMemberVisibleCheck)
		{
			mMemberVisibleCheck->set((rd.mRolePowers & GP_MEMBER_VISIBLE_IN_DIR) == GP_MEMBER_VISIBLE_IN_DIR);
			if (is_owner_role)
			{
				mMemberVisibleCheck->setEnabled(false);
			}
			else
			{
				mMemberVisibleCheck->setEnabled(gAgent.hasPowerInGroup(mGroupID,
																	   GP_ROLE_PROPERTIES));
			}
		}

		if (item->getUUID().isNull())
		{
			// Everyone role, can't edit description or name or delete
			if (mRoleDescription) mRoleDescription->setEnabled(false);
			if (mRoleName) mRoleName->setEnabled(false);
			can_delete = false;
		}
	}
	else
	{
		mAssignedMembersList->deleteAllItems();
		mAllowedActionsList->deleteAllItems();
		if (mRolesList) mRolesList->deselectAllItems();
		if (mRoleName) mRoleName->clear();
		if (mRoleDescription) mRoleDescription->clear();
		if (mRoleTitle) mRoleTitle->clear();
		setFooterEnabled(false);

		can_delete = false;
	}
	mSelectedRole = item->getUUID();
	buildMembersList();

	can_delete = can_delete && gAgent.hasPowerInGroup(mGroupID,
													  GP_ROLE_DELETE);
	if (mDeleteRoleButton) mDeleteRoleButton->setEnabled(can_delete);
}

void LLPanelGroupRolesSubTab::buildMembersList()
{
	if (!mAssignedMembersList || !mRolesList) return;

	mAssignedMembersList->deleteAllItems();

	LLGroupMgrGroupData* gdatap = gGroupMgr.getGroupData(mGroupID);
	if (!gdatap)
	{
		llwarns << "No group data !" << llendl;
		return;
	}

	// Check if there is anything selected.
	LLScrollListItem* item = mRolesList->getFirstSelected();
	if (!item) return;

	if (item->getUUID().isNull())
	{
		// Special cased 'Everyone' role
		LLGroupMgrGroupData::member_list_t::iterator mit = gdatap->mMembers.begin();
		LLGroupMgrGroupData::member_list_t::iterator end = gdatap->mMembers.end();
		for ( ; mit != end; ++mit)
		{
			mAssignedMembersList->addNameItem(mit->first);
		}
	}
	else
	{
		LLGroupMgrGroupData::role_list_t::iterator rit;
		rit = gdatap->mRoles.find(item->getUUID());
		if (rit != gdatap->mRoles.end())
		{
			LLGroupRoleData* rdatap = rit->second;
			if (rdatap)
			{
				uuid_vec_t::const_iterator mit = rdatap->getMembersBegin();
				uuid_vec_t::const_iterator end = rdatap->getMembersEnd();
				for ( ; mit != end; ++mit)
				{
					mAssignedMembersList->addNameItem(*mit);
				}
			}
		}
	}
}

//static
void LLPanelGroupRolesSubTab::onActionCheck(LLUICtrl* ctrl, void* userdata)
{
	LLPanelGroupRolesSubTab* self = (LLPanelGroupRolesSubTab*)userdata;
	LLCheckBoxCtrl* check = (LLCheckBoxCtrl*)ctrl;
	if (check && self)
	{
		self->handleActionCheck(check);
	}
}

struct ActionCBData
{
	LLPanelGroupRolesSubTab*	mSelf;
	LLCheckBoxCtrl* 			mCheck;
};

void LLPanelGroupRolesSubTab::handleActionCheck(LLCheckBoxCtrl* check,
												bool force)
{
	if (!mAssignedMembersList || !mRolesList) return;

	LLGroupMgrGroupData* gdatap = gGroupMgr.getGroupData(mGroupID);
	if (!gdatap)
	{
		llwarns << "No group data !" << llendl;
		return;
	}

	LLScrollListItem* action_item = mAllowedActionsList->getFirstSelected();
	if (!action_item)
	{
		return;
	}

	LLScrollListItem* role_item = mRolesList->getFirstSelected();
	if (!role_item)
	{
		return;
	}
	LLUUID role_id = role_item->getUUID();

	LLRoleAction* rap = (LLRoleAction*)action_item->getUserdata();
	U64 power = rap->mPowerBit;

	LLRoleData rd;
	LLSD args;
	bool is_enabling_ability = check->get();
	if (is_enabling_ability && !force &&
		(GP_ROLE_ASSIGN_MEMBER == power || GP_ROLE_CHANGE_ACTIONS == power))
	{
		// Uncheck the item, for now. It will be checked if they click 'Yes',
		// below.
		check->set(false);

		if (gdatap->getRoleData(role_id, rd))
		{
			args["ACTION_NAME"] = rap->mDescription;
			args["ROLE_NAME"] = rd.mRoleName;
			mHasModal = true;
			std::string warning = "AssignDangerousActionWarning";
			if (GP_ROLE_CHANGE_ACTIONS == power)
			{
				warning = "AssignDangerousAbilityWarning";
			}
			gNotifications.add(warning, args, LLSD(),
							   boost::bind(&LLPanelGroupRolesSubTab::addActionCB,
										   this, _1, _2, check));
		}
		else
		{
			llwarns << "Unable to look up role information for role id: "
					<< role_id << llendl;
		}
	}

	if (power == GP_GROUP_BAN_ACCESS)
	{
		std::string warning = is_enabling_ability ? "AssignBanAbilityWarning"
												  : "RemoveBanAbilityWarning";

		// Get role data for both GP_ROLE_REMOVE_MEMBER and GP_MEMBER_EJECT.
		// Add description and role name to LLSD. Pop up dialog saying "You
		// also granted these other abilities when you did this!"
		if (gdatap->getRoleData(role_id, rd))
		{
			args["ACTION_NAME"] = rap->mDescription;
			args["ROLE_NAME"] = rd.mRoleName;
			mHasModal = true;

			std::vector<LLScrollListItem*> all_data =
				mAllowedActionsList->getAllData();
			for (U32 i = 0, count = all_data.size(); i < count; ++i)
			{
				LLRoleAction* adp = (LLRoleAction*)all_data[i]->getUserdata();
				if (!adp) continue;	// Paranoia

				if (adp->mPowerBit == GP_MEMBER_EJECT)
				{
					args["ACTION_NAME_2"] = adp->mDescription;
				}
				else if(adp->mPowerBit == GP_ROLE_REMOVE_MEMBER)
				{
					args["ACTION_NAME_3"] = adp->mDescription;
				}
			}

			gNotifications.add(warning, args);
		}
		else
		{
			llwarns << "Unable to look up role information for role id: "
					<< role_id << llendl;
		}

		LLGroupMgrGroupData::role_list_t::iterator rit;
		rit = gdatap->mRoles.find(role_id);
		U64 current_role_powers = GP_NO_POWERS;
		if (rit != gdatap->mRoles.end())
		{
			current_role_powers = rit->second->getRoleData().mRolePowers;
		}

		if (is_enabling_ability)
		{
			power |= GP_ROLE_REMOVE_MEMBER | GP_MEMBER_EJECT;
			current_role_powers |= power;
		}
		else
		{
			current_role_powers &= ~GP_GROUP_BAN_ACCESS;
		}

		mAllowedActionsList->deleteAllItems();
		buildActionsList(mAllowedActionsList,
						 current_role_powers, current_role_powers,
						 mActionIcons, onActionCheck, true, false, false);

	}

	// Adding non-specific ability to role
	if (is_enabling_ability)
	{
		gdatap->addRolePower(role_id, power);
	}
	else
	{
		gdatap->removeRolePower(role_id, power);
	}

	mHasRoleChange = true;
	notifyObservers();
}

bool LLPanelGroupRolesSubTab::addActionCB(const LLSD& notification,
										  const LLSD& response,
										  LLCheckBoxCtrl* check)
{
	if (!check) return false;

	mHasModal = false;

	S32 option = LLNotification::getSelectedOption(notification, response);
	if (0 == option)
	{
		// User clicked "Yes"
		check->set(true);
		constexpr bool force_add = true;
		handleActionCheck(check, force_add);
	}
	return false;
}

//static
void LLPanelGroupRolesSubTab::onPropertiesKey(LLLineEditor* ctrl,
											  void* userdata)
{
	LLPanelGroupRolesSubTab* self = (LLPanelGroupRolesSubTab*)userdata;
	if (self)
	{
		self->mHasRoleChange = true;
		self->notifyObservers();
	}
}

//static
void LLPanelGroupRolesSubTab::onDescriptionFocus(LLFocusableElement* ctrl,
												 void* userdata)
{
	LLPanelGroupRolesSubTab* self = (LLPanelGroupRolesSubTab*)userdata;
	if (self)
	{
		self->mHasRoleChange = true;
		self->notifyObservers();
	}
}

//static
void LLPanelGroupRolesSubTab::onDescriptionCommit(LLUICtrl* ctrl,
												  void* userdata)
{
	LLPanelGroupRolesSubTab* self = (LLPanelGroupRolesSubTab*)userdata;
	if (self)
	{
		self->mHasRoleChange = true;
		self->notifyObservers();
	}
}

//static
void LLPanelGroupRolesSubTab::onMemberVisibilityChange(LLUICtrl* ctrl,
													   void* userdata)
{
	LLPanelGroupRolesSubTab* self = (LLPanelGroupRolesSubTab*)userdata;
	LLCheckBoxCtrl* check = (LLCheckBoxCtrl*)ctrl;
	if (check && self)
	{
		self->handleMemberVisibilityChange(check->get());
	}
}

void LLPanelGroupRolesSubTab::handleMemberVisibilityChange(bool value)
{
	LLGroupMgrGroupData* gdatap = gGroupMgr.getGroupData(mGroupID);
	if (!gdatap)
	{
		llwarns << "No group data !" << llendl;
		return;
	}

	LLScrollListItem* role_item = mRolesList->getFirstSelected();
	if (!role_item)
	{
		return;
	}

	if (value)
	{
		gdatap->addRolePower(role_item->getUUID(), GP_MEMBER_VISIBLE_IN_DIR);
	}
	else
	{
		gdatap->removeRolePower(role_item->getUUID(),
								GP_MEMBER_VISIBLE_IN_DIR);
	}
}

//static
void LLPanelGroupRolesSubTab::onCreateRole(void* userdata)
{
	LLPanelGroupRolesSubTab* self = (LLPanelGroupRolesSubTab*)userdata;
	if (self)
	{
		self->handleCreateRole();
	}
}

void LLPanelGroupRolesSubTab::handleCreateRole()
{
	LLGroupMgrGroupData* gdatap = gGroupMgr.getGroupData(mGroupID);
	if (!gdatap || !mRolesList) return;

	LLUUID new_role_id;
	new_role_id.generate();

	LLRoleData rd;
	rd.mRoleName = "New Role";
	gdatap->createRole(new_role_id,rd);

	mRolesList->deselectAllItems(true);
	LLSD row;
	row["id"] = new_role_id;
	row["columns"][0]["column"] = "name";
	row["columns"][0]["value"] = rd.mRoleName;
	mRolesList->addElement(row, ADD_BOTTOM, this);
	mRolesList->selectByID(new_role_id);

	// put focus on name field and select its contents
	if (mRoleName)
	{
		mRoleName->setFocus(true);
		mRoleName->onTabInto();
		gFocusMgr.triggerFocusFlash();
	}

	notifyObservers();
}

//static
void LLPanelGroupRolesSubTab::onDeleteRole(void* userdata)
{
	LLPanelGroupRolesSubTab* self = (LLPanelGroupRolesSubTab*)userdata;
	if (self)
	{
		self->handleDeleteRole();
	}
}

void LLPanelGroupRolesSubTab::handleDeleteRole()
{
	LLGroupMgrGroupData* gdatap = gGroupMgr.getGroupData(mGroupID);
	if (!gdatap || !mRolesList) return;

	LLScrollListItem* role_item = mRolesList->getFirstSelected();
	if (!role_item)
	{
		return;
	}

	if (role_item->getUUID().isNull() ||
		role_item->getUUID() == gdatap->mOwnerRole)
	{
		LLSD args;
		args["MESSAGE"] = mRemoveEveryoneTxt;
		gNotifications.add("GenericAlert", args);
		return;
	}

	gdatap->deleteRole(role_item->getUUID());
	mRolesList->deleteSingleItem(mRolesList->getFirstSelectedIndex());
	mRolesList->selectFirstItem();

	notifyObservers();
}

void LLPanelGroupRolesSubTab::saveRoleChanges()
{
	LLGroupMgrGroupData* gdatap = gGroupMgr.getGroupData(mGroupID);
	if (gdatap && mRolesList && mHasRoleChange)
	{
		LLRoleData rd;
		if (!gdatap->getRoleData(mSelectedRole,rd)) return;

		rd.mRoleName = mRoleName->getText();
		rd.mRoleDescription = mRoleDescription->getText();
		rd.mRoleTitle = mRoleTitle->getText();

		gdatap->setRoleData(mSelectedRole,rd);

		mRolesList->deleteSingleItem(mRolesList->getItemIndex(mSelectedRole));

		LLSD row = createRoleItem(mSelectedRole,rd.mRoleName,rd.mRoleTitle,0);
		LLScrollListItem* item = mRolesList->addElement(row, ADD_BOTTOM, this);
		item->setSelected(true);

		mHasRoleChange = false;
	}
}

////////////////////////////
// LLPanelGroupActionsSubTab
////////////////////////////

//static
void* LLPanelGroupActionsSubTab::createTab(void* data)
{
	LLUUID* group_id = (LLUUID*)data;
	return new LLPanelGroupActionsSubTab("panel group actions sub tab",
										 *group_id);
}

LLPanelGroupActionsSubTab::LLPanelGroupActionsSubTab(const std::string& name,
													 const LLUUID& group_id)
:	LLPanelGroupSubTab(name, group_id)
{
}

bool LLPanelGroupActionsSubTab::postBuildSubTab(LLView* root)
{
	// Upcast parent so we can ask it for sibling controls.
	LLPanelGroupRoles* parent = (LLPanelGroupRoles*) root;

	mHeader = parent->getChild<LLPanel>("actions_header", true, false);
	mFooter = parent->getChild<LLPanel>("actions_footer", true, false);

	mActionDescription = parent->getChild<LLTextEditor>("action_description",
														true, false);
	if (!mActionDescription) return false;

	mActionList = parent->getChild<LLScrollListCtrl>("action_list",
													 true, false);
	if (!mActionList) return false;

	mActionRoles = parent->getChild<LLScrollListCtrl>("action_roles",
													  true, false);
	if (!mActionRoles) return false;

	mActionMembers  = parent->getChild<LLNameListCtrl>("action_members",
													   true, false);
	if (!mActionMembers) return false;

	mActionList->setCallbackUserData(this);
	mActionList->setCommitOnSelectionChange(true);
	mActionList->setCommitCallback(onActionSelect);

	mActionMembers->setCallbackUserData(this);
	mActionRoles->setCallbackUserData(this);

	update(GC_ALL);

	return true;
}

void LLPanelGroupActionsSubTab::activate()
{
	LLPanelGroupSubTab::activate();
	if (mActionList) mActionList->deselectAllItems();
	if (mActionMembers) mActionMembers->deleteAllItems();
	if (mActionRoles) mActionRoles->deleteAllItems();
	if (mActionDescription) mActionDescription->clear();
}

void LLPanelGroupActionsSubTab::deactivate()
{
	LLPanelGroupSubTab::deactivate();
}

bool LLPanelGroupActionsSubTab::needsApply(std::string& mesg)
{
	return false;
}

bool LLPanelGroupActionsSubTab::apply(std::string& mesg)
{
	return true;
}

void LLPanelGroupActionsSubTab::update(LLGroupChange gc)
{
	if (mGroupID.isNull()) return;

	if (mActionList) mActionList->deselectAllItems();
	if (mActionMembers) mActionMembers->deleteAllItems();
	if (mActionRoles) mActionRoles->deleteAllItems();
	if (mActionDescription) mActionDescription->clear();

	mActionList->deleteAllItems();
	buildActionsList(mActionList, GP_ALL_POWERS, GP_ALL_POWERS, mActionIcons,
					 NULL, false, true, false);
}

//static
void LLPanelGroupActionsSubTab::onActionSelect(LLUICtrl* scroll, void* data)
{
	LLPanelGroupActionsSubTab* self = (LLPanelGroupActionsSubTab*)data;
	if (self)
	{
		self->handleActionSelect();
	}
}

void LLPanelGroupActionsSubTab::handleActionSelect()
{
	if (!mActionMembers || !mActionRoles || !mActionDescription) return;

	mActionMembers->deleteAllItems();
	mActionRoles->deleteAllItems();

	setFooterEnabled(true);

	std::vector<LLScrollListItem*> selection = mActionList->getAllSelected();
	if (selection.empty()) return;

	U64 power_mask = GP_NO_POWERS;
	LLRoleAction* rap;

	for (U32 i = 0, count = selection.size(); i < count; ++i)
	{
		rap = (LLRoleAction*)selection[i]->getUserdata();
		power_mask |= rap->mPowerBit;
	}

	if (selection.size() == 1)
	{
		LLScrollListItem* item = selection[0];
		rap = (LLRoleAction*)(item->getUserdata());

		if (rap->mLongDescription.empty())
		{
			mActionDescription->setText(rap->mDescription);
		}
		else
		{
			mActionDescription->setText(rap->mLongDescription);
		}
	}
	else
	{
		mActionDescription->clear();
	}

	LLGroupMgrGroupData* gdatap = gGroupMgr.getGroupData(mGroupID);
	if (!gdatap) return;

	if (gdatap->isMemberDataComplete())
	{
		LLGroupMemberData* gmd;
		for (LLGroupMgrGroupData::member_list_t::iterator
				it = gdatap->mMembers.begin(),
				end = gdatap->mMembers.end();
			 it != end; ++it)
		{
			gmd = it->second;
			if (gmd && (gmd->getAgentPowers() & power_mask) == power_mask)
			{
				mActionMembers->addNameItem(gmd->getID());
			}
		}
	}
	else
	{
		gGroupMgr.sendCapGroupMembersRequest(mGroupID);
	}

	if (gdatap->isRoleDataComplete())
	{
		LLGroupRoleData* rmd;
		for (LLGroupMgrGroupData::role_list_t::iterator
				it = gdatap->mRoles.begin(),
				end = gdatap->mRoles.end();
			 it != end; ++it)
		{
			rmd = it->second;
			if (rmd &&
				(rmd->getRoleData().mRolePowers & power_mask) == power_mask)
			{
				mActionRoles->addSimpleElement(rmd->getRoleData().mRoleName);
			}
		}
	}
	else
	{
		gGroupMgr.sendGroupRoleDataRequest(mGroupID);
	}
}

////////////////////////////
// LLPanelGroupBanListSubTab
////////////////////////////

//static
void* LLPanelGroupBanListSubTab::createTab(void* data)
{
	LLUUID* group_id = (LLUUID*)data;
	return new LLPanelGroupBanListSubTab("panel_group_banlist_subtab",
										 *group_id);
}

LLPanelGroupBanListSubTab::LLPanelGroupBanListSubTab(const std::string& name,
													 const LLUUID& group_id)
:	LLPanelGroupSubTab(name, group_id),
	mBanList(NULL),
	mCreateBanButton(NULL),
	mDeleteBanButton(NULL),
	mRefreshBanListButton(NULL),
	mBanNotSupportedText(NULL),
	mBanCountText(NULL),
	mLastUpdate(0.f)
{
}

bool LLPanelGroupBanListSubTab::postBuildSubTab(LLView* root)
{
	// Upcast parent so we can ask it for sibling controls.
	LLPanelGroupRoles* parent = (LLPanelGroupRoles*) root;

	mHeader = parent->getChild<LLPanel>("banlist_header", true, false);
	mFooter = parent->getChild<LLPanel>("banlist_footer", true, false);

	mBanList = parent->getChild<LLNameListCtrl>("ban_list", true, false);

	mCreateBanButton = parent->getChild<LLButton>("ban_create", true, false);
	mDeleteBanButton = parent->getChild<LLButton>("ban_delete", true, false);
	mRefreshBanListButton = parent->getChild<LLButton>("ban_refresh",
													   true, false);

	if (!mBanList || !mCreateBanButton || !mDeleteBanButton ||
		!mRefreshBanListButton)
	{
		return false;
	}

	mBanList->setCommitCallback(onBanEntrySelect);
	mBanList->setCallbackUserData(this);
	mBanList->setCommitOnSelectionChange(true);
	// Show the member's profile on double click.
	mBanList->setDoubleClickCallback(onBanListMemberDoubleClick);
	mBanList->setCallbackUserData(this);

	mCreateBanButton->setClickedCallback(onCreateBanEntry);
	mCreateBanButton->setCallbackUserData(this);
	mCreateBanButton->setEnabled(false);

	mDeleteBanButton->setClickedCallback(onDeleteBanEntry);
	mDeleteBanButton->setCallbackUserData(this);
	mDeleteBanButton->setEnabled(false);

	mRefreshBanListButton->setClickedCallback(onRefreshBanList);
	mRefreshBanListButton->setCallbackUserData(this);
	mRefreshBanListButton->setEnabled(false);

	mBanNotSupportedText = parent->getChild<LLTextBox>("ban_not_supported",
													   true, false);
	mBanCountText = parent->getChild<LLTextBox>("ban_count", true, false);
	if (mBanCountText)
	{
		mBanCountString = mBanCountText->getText();
	}

	setBanCount(0);
	populateBanList();

	setFooterEnabled(false);

	return true;
}

void LLPanelGroupBanListSubTab::draw()
{
	constexpr F32 UPDATE_INTERVAL = 2.f;
	if (gFrameTimeSeconds - mLastUpdate > UPDATE_INTERVAL)
	{
		bool got_cap = gAgent.hasRegionCapability("GroupAPIv1");
		if (mBanList && got_cap != mBanList->getEnabled())
		{
			populateBanList();
		}
		if (mBanNotSupportedText)
		{
			mBanNotSupportedText->setVisible(!got_cap);
			mBanNotSupportedText->setEnabled(!got_cap);
		}
		if (mBanCountText)
		{
			mBanCountText->setVisible(got_cap);
			mBanCountText->setEnabled(got_cap);
		}
		mLastUpdate = gFrameTimeSeconds;
	}

	LLPanelGroupSubTab::draw();
}

void LLPanelGroupBanListSubTab::activate()
{
	LLPanelGroupSubTab::activate();

	if (mBanList)
	{
		mBanList->deselectAllItems();
	}
	if (mDeleteBanButton)
	{
		mDeleteBanButton->setEnabled(false);
	}
	if (mCreateBanButton)
	{
		mCreateBanButton->setEnabled(gAgent.hasPowerInGroup(mGroupID,
															GP_GROUP_BAN_ACCESS));
	}

	// BAKER: Should I really request everytime activate() is called ?
	//		  Perhaps I should only do it on a force refresh, or if an action
	//        on the list happens...
	//		  Because it's not going to live-update the list anyway... You'd
	//        have to refresh if you wanted to see someone else's additions
	//        anyway...
	gGroupMgr.sendGroupBanRequest(LLGroupMgr::REQUEST_GET, mGroupID);

	setFooterEnabled(false);
	update(GC_ALL);
}

void LLPanelGroupBanListSubTab::update(LLGroupChange gc)
{
	populateBanList();
}

void LLPanelGroupBanListSubTab::onBanEntrySelect(LLUICtrl* ctrl,
												 void* user_data)
{
	LLPanelGroupBanListSubTab* self = (LLPanelGroupBanListSubTab*)user_data;
	if (self)
	{
		self->handleBanEntrySelect();
	}
}

void LLPanelGroupBanListSubTab::handleBanEntrySelect()
{
	if (mDeleteBanButton &&
		gAgent.hasPowerInGroup(mGroupID, GP_GROUP_BAN_ACCESS))
	{
		mDeleteBanButton->setEnabled(true);
	}
}

void LLPanelGroupBanListSubTab::onCreateBanEntry(void* user_data)
{
	LLPanelGroupBanListSubTab* self = (LLPanelGroupBanListSubTab*)user_data;
	if (self)
	{
		self->handleCreateBanEntry();
	}
}

void LLPanelGroupBanListSubTab::handleCreateBanEntry()
{
	LLFloaterGroupBulkBan::showForGroup(mGroupID, NULL, this);
}

void LLPanelGroupBanListSubTab::onDeleteBanEntry(void* user_data)
{
	LLPanelGroupBanListSubTab* self = (LLPanelGroupBanListSubTab*)user_data;
	if (self)
	{
		self->handleDeleteBanEntry();
	}
}

void LLPanelGroupBanListSubTab::handleDeleteBanEntry()
{
	LLGroupMgrGroupData* gdatap = gGroupMgr.getGroupData(mGroupID);
	if (!gdatap)
	{
		llwarns << "Unable to get group data for group " << mGroupID << llendl;
		return;
	}

	std::vector<LLScrollListItem*> selection = mBanList->getAllSelected();
	if (selection.empty())
	{
		return;
	}

	bool can_ban_members = false;
	if (gAgent.isGodlikeWithoutAdminMenuFakery() ||
		gAgent.hasPowerInGroup(mGroupID, GP_GROUP_BAN_ACCESS))
	{
		can_ban_members	= true;
	}

	// Owners can ban anyone in the group.
	LLGroupMgrGroupData::member_list_t::iterator mi;
	mi = gdatap->mMembers.find(gAgentID);
	if (mi != gdatap->mMembers.end())
	{
		LLGroupMemberData* member_data = mi->second;
		if (member_data && member_data->isInRole(gdatap->mOwnerRole))
		{
			can_ban_members	= true;
		}
	}

	if (!can_ban_members)
	{
		return;
	}

	uuid_vec_t ban_ids;
	for (U32 i = 0, count = selection.size(); i < count; ++i)
	{
		const LLUUID& ban_id = selection[i]->getUUID();
		ban_ids.emplace_back(ban_id);

		gdatap->removeBanEntry(ban_id);
		if (mBanList)
		{
			mBanList->removeNameItem(ban_id);
		}

		// Removing an item removes the selection, we should not be able to
		// click the button anymore until we reselect another entry.
		if (mDeleteBanButton)
		{
			mDeleteBanButton->setEnabled(false);
		}
	}

	gGroupMgr.sendGroupBanRequest(LLGroupMgr::REQUEST_POST, mGroupID,
								  LLGroupMgr::BAN_DELETE, ban_ids);
	setBanCount(gdatap->mBanList.size());
}

void LLPanelGroupBanListSubTab::onRefreshBanList(void* user_data)
{
	LLPanelGroupBanListSubTab* self = (LLPanelGroupBanListSubTab*)user_data;
	if (self)
	{
		self->handleRefreshBanList();
	}
}

void LLPanelGroupBanListSubTab::handleRefreshBanList()
{
	mRefreshBanListButton->setEnabled(false);
	gGroupMgr.sendGroupBanRequest(LLGroupMgr::REQUEST_GET, mGroupID);
}

void LLPanelGroupBanListSubTab::populateBanList()
{
	if (!gAgent.hasRegionCapability("GroupAPIv1"))
	{
		if (mRefreshBanListButton)
		{
			mRefreshBanListButton->setEnabled(false);
		}
		if (mBanList)
		{
			mBanList->deleteAllItems();
			mBanList->setEnabled(false);
		}
		if (mCreateBanButton)
		{
			mCreateBanButton->setEnabled(false);
		}
		if (mDeleteBanButton)
		{
			mDeleteBanButton->setEnabled(false);
		}
		return;
	}

	if (mCreateBanButton)
	{
		mCreateBanButton->setEnabled(gAgent.hasPowerInGroup(mGroupID,
															GP_GROUP_BAN_ACCESS));
	}
	if (mRefreshBanListButton)
	{
		mRefreshBanListButton->setEnabled(true);
	}

	LLGroupMgrGroupData* gdatap = gGroupMgr.getGroupData(mGroupID);
	if (!gdatap)
	{
		llwarns << "Unable to get group data for group " << mGroupID << llendl;
		return;
	}

	if (!mBanList) return;
	mBanList->setEnabled(true);
	mBanList->deleteAllItems();

	for (LLGroupMgrGroupData::ban_list_t::const_iterator
			it = gdatap->mBanList.begin(), end = gdatap->mBanList.end();
		 it != end; ++it)
	{
		LLSD row;
		row["id"] = it->first;
		LLSD& columns = row["columns"];

		columns[0]["column"] = "name";
		// Value is filled in by name list control

		LLGroupBanData bd = it->second;
		columns[1]["column"] = "bandate";
		columns[1]["value"] = bd.mBanDate.asTimeStamp();

		mBanList->addElement(row);
	}

#if 0
	mMembersList->setAllowRefresh(true);
	mMembersList->setLazyUpdateInterval(2.f);
#endif

	setBanCount(gdatap->mBanList.size());
}

void LLPanelGroupBanListSubTab::setBanCount(S32 count)
{
	if (mBanCountText)
	{
		mBanCountText->setText(mBanCountString +
							   llformat(" %d/%d", count,
										GB_MAX_BANNED_AGENTS));
	}
}

//static
void LLPanelGroupBanListSubTab::onBanListMemberDoubleClick(void* userdata)
{
	LLPanelGroupBanListSubTab* self = (LLPanelGroupBanListSubTab*)userdata;
	if (self && self->mBanList)
	{
		LLScrollListItem* itemp = self->mBanList->getFirstSelected();
		if (itemp)
		{
			LLFloaterAvatarInfo::showFromDirectory(itemp->getUUID());
		}
	}
}
