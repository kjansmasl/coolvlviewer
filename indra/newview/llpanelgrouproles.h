/**
 * @file llpanelgrouproles.h
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

#ifndef LL_LLPANELGROUPROLES_H
#define LL_LLPANELGROUPROLES_H

#include "llpanelgroup.h"

class LLCheckBoxCtrl;
class LLNameListCtrl;
class LLPanelGroupActionsSubTab;
class LLPanelGroupMembersSubTab;
class LLPanelGroupRolesSubTab;
class LLPanelGroupSubTab;
class LLScrollListCtrl;
class LLScrollListItem;
class LLTextBox;
class LLTextEditor;
class LLTimer;

typedef std::map<std::string, std::string> icon_map_t;

class LLPanelGroupRoles : public LLPanelGroupTab,
						  public LLPanelGroupTabObserver
{
protected:
	LOG_CLASS(LLPanelGroupRoles);

public:
	LLPanelGroupRoles(const std::string& name, const LLUUID& group_id);
	~LLPanelGroupRoles() override;

	// Allow sub tabs to ask for sibling controls.
	friend class LLPanelGroupMembersSubTab;
	friend class LLPanelGroupRolesSubTab;
	friend class LLPanelGroupActionsSubTab;

	bool postBuild() override;
	bool isVisibleByAgent() override;

	static void* createTab(void* data);
	static void onClickSubTab(void*,bool);
	void handleClickSubTab();

	// Checks if the current tab needs to be applied, and tries to switch to
	// the requested tab.
	bool attemptTransition();

	// Switches to the requested tab (will close() if requested is NULL)
	void transitionToTab();

	// Used by attemptTransition to query the user's response to a tab that
	// needs to apply.
	bool handleNotifyCallback(const LLSD& notification, const LLSD& response);
	bool onModalClose(const LLSD& notification, const LLSD& response);

	// Most of these messages are just passed on to the current sub-tab.
	std::string getHelpText() const override;
	void activate() override;
	void deactivate() override;
	bool needsApply(std::string& mesg) override;
	bool hasModal() override;
	bool apply(std::string& mesg) override;
	void cancel() override;
	void update(LLGroupChange gc) override;

	// PanelGroupTab observer trigger
	void tabChanged() override;

protected:
	LLPanelGroupTab*		mCurrentTab;
	LLPanelGroupTab*		mRequestedTab;
	LLTabContainer*			mSubTabContainer;
	bool					mIgnoreTransition;

	std::string				mDefaultNeedsApplyMesg;
	std::string				mWantApplyMesg;
};

class LLPanelGroupSubTab : public LLPanelGroupTab
{
protected:
	LOG_CLASS(LLPanelGroupSubTab);

public:
	LLPanelGroupSubTab(const std::string& name, const LLUUID& group_id);

	bool postBuild() override;

	// This allows sub-tabs to collect child widgets from a higher level in the
	// view hierarchy.
	virtual bool postBuildSubTab(LLView* root) 		{ return true; }

	static void onSearchKeystroke(LLLineEditor* caller, void* user_data);
	void handleSearchKeystroke(LLLineEditor* caller);

	static void onClickSearch(void*);
	void handleClickSearch();
	static void onClickShowAll(void*);
	void handleClickShowAll();

	void setSearchFilter(const std::string& filter);

	void activate() override;
	void deactivate() override;

	// Helper functions
	bool matchesActionSearchFilter(std::string action);
	void buildActionsList(LLScrollListCtrl* ctrl, U64 allowed_by_some,
						  U64 allowed_by_all, icon_map_t& icons,
						  void (*commit_callback)(LLUICtrl*, void*),
						  bool show_all, bool filter, bool is_owner_role);
	void buildActionCategory(LLScrollListCtrl* ctrl, U64 allowed_by_some,
							 U64 allowed_by_all, LLRoleActionSet* action_set,
							 icon_map_t& icons,
							 void (*commit_callback)(LLUICtrl*, void*),
							 bool show_all, bool filter, bool is_owner_role);

	void setFooterEnabled(bool enable);

protected:
	void setOthersVisible(bool b);

protected:
	LLPanel*		mHeader;
	LLPanel*		mFooter;

	LLLineEditor*	mSearchLineEditor;
	LLButton*		mSearchButton;
	LLButton*		mShowAllButton;

	// Used to communicate between action sets due to the dependency between
	// GP_GROUP_BAN_ACCESS and GP_EJECT_MEMBER and GP_ROLE_REMOVE_MEMBER
	bool mHasGroupBanPower;

	std::string mSearchFilter;

	icon_map_t	mActionIcons;
};

class LLPanelGroupMembersSubTab final : public LLPanelGroupSubTab
{
protected:
	LOG_CLASS(LLPanelGroupMembersSubTab);

public:
	LLPanelGroupMembersSubTab(const std::string& name, const LLUUID& group_id);

	bool postBuildSubTab(LLView* root) override;

	static void* createTab(void* data);

	static void onMemberSelect(LLUICtrl*, void*);
	void handleMemberSelect();

	static void onMemberDoubleClick(void*);

	static void onInviteMember(void*);

	static void onEjectMembers(void*);
	void handleEjectMembers();

	static void onRoleCheck(LLUICtrl* check, void* user_data);
	void handleRoleCheck(const LLUUID& role_id,
						 LLRoleMemberChangeType type);

	static void onBanMember(void* user_data);
	void handleBanMember();

	void applyMemberChanges();
	bool addOwnerCB(const LLSD& notification, const LLSD& response);

	void activate() override;
	void deactivate() override;
	void cancel() override;
	bool needsApply(std::string& mesg) override;
	bool apply(std::string& mesg) override;
	void update(LLGroupChange gc) override;
	void updateMembers();

	void draw() override;

protected:
	typedef fast_hmap<LLUUID, LLRoleMemberChangeType> role_change_data_map_t;
	typedef fast_hmap<LLUUID,
					  role_change_data_map_t*> member_role_changes_map_t;

	bool matchesSearchFilter(std::string fullname);

	U64  getAgentPowersBasedOnRoleChanges(const LLUUID& agent_id);
	bool getRoleChangeType(const LLUUID& member_id,
						   const LLUUID& role_id,
						   LLRoleMemberChangeType& type);

protected:
	LLNameListCtrl*				mMembersList;
	LLScrollListCtrl*			mAssignedRolesList;
	LLScrollListCtrl*			mAllowedActionsList;
	LLButton*           		mEjectBtn;
	LLButton*           		mBanBtn;

	LLTimer						mUpdateTimer;
	F32							mUpdateInterval;
	bool						mSkipNextUpdate;
	bool						mPendingMemberUpdate;
	bool						mChanged;
	bool						mHasMatch;

	member_role_changes_map_t	mMemberRoleChangeData;
	U32							mNumOwnerAdditions;

	LLGroupMgrGroupData::member_list_t::iterator mMemberProgress;
};

class LLPanelGroupRolesSubTab final : public LLPanelGroupSubTab
{
protected:
	LOG_CLASS(LLPanelGroupRolesSubTab);

public:
	LLPanelGroupRolesSubTab(const std::string& name, const LLUUID& group_id);

	bool postBuildSubTab(LLView* root) override;

	static void* createTab(void* data);

	void activate() override;
	void deactivate() override;
	bool needsApply(std::string& mesg) override;
	bool apply(std::string& mesg) override;
	void cancel() override;
	bool matchesSearchFilter(std::string rolename, std::string roletitle);
	void update(LLGroupChange gc) override;

protected:
	static void onRoleSelect(LLUICtrl*, void*);
	void handleRoleSelect();
	void buildMembersList();

	static void onActionCheck(LLUICtrl*, void*);
	void handleActionCheck(LLCheckBoxCtrl*, bool force = false);
	bool addActionCB(const LLSD& notification, const LLSD& response,
					 LLCheckBoxCtrl* check);

	static void onPropertiesKey(LLLineEditor*, void*);

	static void onDescriptionCommit(LLUICtrl*, void*);
	static void onDescriptionFocus(LLFocusableElement*, void*);

	static void onAssignedMemberDoubleClick(void*);

	static void onMemberVisibilityChange(LLUICtrl*, void*);
	void handleMemberVisibilityChange(bool value);

	static void onCreateRole(void*);
	void handleCreateRole();

	static void onDeleteRole(void*);
	void handleDeleteRole();

	void saveRoleChanges();

	LLSD createRoleItem(const LLUUID& role_id,  std::string name,
						std::string title, S32 members);

protected:
	LLScrollListCtrl*	mRolesList;
	LLNameListCtrl*		mAssignedMembersList;
	LLScrollListCtrl*	mAllowedActionsList;

	LLLineEditor*		mRoleName;
	LLLineEditor*		mRoleTitle;
	LLTextEditor*		mRoleDescription;

	LLCheckBoxCtrl*		mMemberVisibleCheck;
	LLButton*       	mDeleteRoleButton;
	LLButton*       	mCreateRoleButton;

	LLUUID				mSelectedRole;
	std::string			mRemoveEveryoneTxt;

	bool				mFirstOpen;
	bool				mHasRoleChange;
};

class LLPanelGroupActionsSubTab final : public LLPanelGroupSubTab
{
protected:
	LOG_CLASS(LLPanelGroupActionsSubTab);

public:
	LLPanelGroupActionsSubTab(const std::string& name, const LLUUID& group_id);

	bool postBuildSubTab(LLView* root) override;

	static void* createTab(void* data);

	void activate() override;
	void deactivate() override;
	bool needsApply(std::string& mesg) override;
	bool apply(std::string& mesg) override;
	void update(LLGroupChange gc) override;

protected:
	static void onActionSelect(LLUICtrl*, void*);
	void handleActionSelect();

protected:
	LLScrollListCtrl*	mActionList;
	LLScrollListCtrl*	mActionRoles;
	LLNameListCtrl*		mActionMembers;

	LLTextEditor*		mActionDescription;
};

class LLPanelGroupBanListSubTab final : public LLPanelGroupSubTab
{
protected:
	LOG_CLASS(LLPanelGroupBanListSubTab);

public:
	LLPanelGroupBanListSubTab(const std::string& name, const LLUUID& group_id);

	bool postBuildSubTab(LLView* root) override;

	static void* createTab(void* data);

	void draw() override;

	void activate() override;
	void update(LLGroupChange gc) override;

	void handleBanEntrySelect();
	void handleCreateBanEntry();
	void handleDeleteBanEntry();
	void handleRefreshBanList();

protected:
	void populateBanList();
	void setBanCount(S32 count);

	static void onBanEntrySelect(LLUICtrl* ctrl, void* user_data);
	static void onCreateBanEntry(void* user_data);
	static void onDeleteBanEntry(void* user_data);
	static void onRefreshBanList(void* user_data);
	static void onBanListMemberDoubleClick(void* user_data);

protected:
	LLNameListCtrl*	mBanList;

	LLButton*	mCreateBanButton;
	LLButton*	mDeleteBanButton;
	LLButton*	mRefreshBanListButton;

	LLTextBox*	mBanNotSupportedText;
	LLTextBox*	mBanCountText;

	F32			mLastUpdate;
	std::string	mBanCountString;
};

#endif // LL_LLPANELGROUPROLES_H
