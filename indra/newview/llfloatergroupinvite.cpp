/** 
 * @file llfloatergroupinvite.cpp
 * @brief Floater to invite new members into a group.
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

#include "llfloatergroupinvite.h"

#include "llbutton.h"
#include "llcombobox.h"
#include "llnamelistctrl.h"
#include "lltextbox.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "llfloateravatarpicker.h"
#include "llgroupmgr.h"
#include "llviewerobjectlist.h"
#include "llvoavatar.h"

//
// Globals
//
LLFloaterGroupInvite::instances_map_t LLFloaterGroupInvite::sInstances;

class LLFloaterGroupInviteData
{
public:
	LLFloaterGroupInviteData(LLFloater* self, const LLUUID& group_id)
	:	mSelf(self),
		mGroupId(group_id),
		mPanel(NULL)
	{
	}

	~LLFloaterGroupInviteData()
	{
	}

	LLFloater*			mSelf;
	LLUUID				mGroupId;
	LLPanelGroupInvite*	mPanel;
};

//////////////////////////////////////////////////////////////////////
// LLPanelGroupInvite (forward declaration)
//////////////////////////////////////////////////////////////////////

class LLPanelGroupInvite final : public LLPanel
{
protected:
	LOG_CLASS(LLPanelGroupInvite);

	void updateLists();

public:
	LLPanelGroupInvite(const LLUUID& group_id, LLFloater* parent);
	~LLPanelGroupInvite() override;

	void draw() override;
	bool postBuild() override;

	void clear() override;
	
	void addUsers(uuid_vec_t& agent_ids);
	void update();

protected:
	class impl;
	impl*	mImplementation;

	bool	mPendingUpdate;
	LLUUID	mStoreSelected;
};

//////////////////////////////////////////////////////////////////////
// LLPanelGroupInvite::impl (*TODO: use LLPanelGroupBulk instead)
//////////////////////////////////////////////////////////////////////

class LLPanelGroupInvite::impl
{
public:
	impl(const LLUUID& group_id, LLFloater* parent);

	void addUsers(const std::vector<std::string>& names,
				  const uuid_vec_t& agent_ids);
	void submitInvitations();
	void addRoleNames(LLGroupMgrGroupData* gdatap);
	void handleRemove();
	void handleSelection();

	static void callbackClickCancel(void* userdata);
	static void callbackClickOK(void* userdata);
	static void callbackClickAdd(void* userdata);
	static void callbackClickRemove(void* userdata);
	static void callbackSelect(LLUICtrl* ctrl, void* userdata);
	static void callbackAddUsers(const std::vector<std::string>& names,
								 const uuid_vec_t& agent_ids,
								 void* user_data);
	bool inviteOwnerCallback(const LLSD& notification, const LLSD& response);

public:
	LLFloater*		mParentFloater;
	LLUUID			mGroupID;

	LLNameListCtrl*	mInvitees;
	LLComboBox*		mRoleNames;
	LLButton*		mOKButton;
 	LLButton*		mRemoveButton;
	LLTextBox*		mGroupName;

	std::string		mLoadingText;
	std::string		mOwnerWarning;
	std::string		mTooManySelected;

	bool			mConfirmedOwnerInvite;

	uuid_list_t		mInviteeIDs;
};

LLPanelGroupInvite::impl::impl(const LLUUID& group_id, LLFloater* parent)
:	mGroupID(group_id),
	mParentFloater(parent),
	mLoadingText(),
	mInvitees(NULL),
	mRoleNames(NULL),
	mOKButton(NULL),
	mRemoveButton(NULL),
	mGroupName(NULL),
	mConfirmedOwnerInvite(false)
{
}

void LLPanelGroupInvite::impl::addUsers(const std::vector<std::string>& names,
										const uuid_vec_t& agent_ids)
{
	if ((U32)(names.size() + mInviteeIDs.size()) > MAX_GROUP_INVITES)
	{
		// Fail !... Show a warning and don't add any names.
		LLSD msg;
		msg["MESSAGE"] = mTooManySelected;
		gNotifications.add("GenericAlert", msg);
		return;
	}

	for (S32 i = 0, count = names.size(); i < count; ++i)
	{
		const std::string& name = names[i];
		const LLUUID& id = agent_ids[i];

		if (mInviteeIDs.count(id))
		{
			continue;	// Already in list, skip...
		}

		// add the name to the names list
		LLSD row;
		row["id"] = id;
		row["columns"][0]["value"] = name;

		mInvitees->addElement(row);
		mInviteeIDs.emplace(id);
	}
}

void LLPanelGroupInvite::impl::submitInvitations()
{
	LLGroupMgrGroupData* gdatap = gGroupMgr.getGroupData(mGroupID);
	if (!gdatap) return;

	// Default to everyone role.
	LLUUID role_id;
	if (mRoleNames)
	{
		role_id = mRoleNames->getCurrentID();

		// Owner role: display confirmation and wait for callback
		if (role_id == gdatap->mOwnerRole && !mConfirmedOwnerInvite)
		{
			LLSD args;
			args["MESSAGE"] = mOwnerWarning;
			gNotifications.add("GenericAlertYesCancel", args, LLSD(),
							   boost::bind(&LLPanelGroupInvite::impl::inviteOwnerCallback,
										   this, _1, _2));
			return; // We will be called again if user confirms
		}
	}

	// Loop over the users
	LLGroupMgr::role_member_pairs_t role_member_pairs;
	std::vector<LLScrollListItem*> items = mInvitees->getAllData();
	for (S32 i = 0, count = items.size(); i < count; ++i)
	{
		LLScrollListItem* item = items[i];
		role_member_pairs[item->getUUID()] = role_id;
	}

	if ((U32)role_member_pairs.size() > MAX_GROUP_INVITES)
	{
		// Fail !
		LLSD msg;
		msg["MESSAGE"] = mTooManySelected;
		gNotifications.add("GenericAlert", msg);
		if (mParentFloater)	// Paranoia
		{
			mParentFloater->close();
		}
		return;
	}

	gGroupMgr.sendGroupMemberInvites(mGroupID, role_member_pairs);

	// Then close
	if (mParentFloater)	// Paranoia
	{
		mParentFloater->close();
	}
}

bool LLPanelGroupInvite::impl::inviteOwnerCallback(const LLSD& notification,
												   const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		// User confirmed that they really want a new group owner
		mConfirmedOwnerInvite = true;
		submitInvitations();
	}
	return false;
}

void LLPanelGroupInvite::impl::addRoleNames(LLGroupMgrGroupData* gdatap)
{
	if (!gdatap) return;

	// Loop over the agent's roles in the group then add those roles to the
	// list of roles that the agent can invite people to be. If the user is the
	// owner then we add all of the roles in the group, else if they have the
	// "add to roles" power we add every role but owner, else if they have the
	// limited add to roles power we add every role the user is in, else we
	// just add to everyone.
	bool is_owner = false;
	bool can_assign_any = gAgent.hasPowerInGroup(mGroupID,
												 GP_ROLE_ASSIGN_MEMBER);
	bool can_assign_limited =
		gAgent.hasPowerInGroup(mGroupID, GP_ROLE_ASSIGN_MEMBER_LIMITED);

	LLGroupMemberData* member_data = NULL;

	// Get the member data for the agent if it exists
	LLGroupMgrGroupData::member_list_t::iterator agent_iter =
		gdatap->mMembers.find(gAgentID);
	if (agent_iter != gdatap->mMembers.end())
	{
		member_data = agent_iter->second;
		if (member_data && mRoleNames)
		{
			is_owner = member_data->isOwner();
		}
	}

	// Populate the role list
	for (LLGroupMgrGroupData::role_list_t::iterator
			it = gdatap->mRoles.begin(), end = gdatap->mRoles.end();
		 it != end; ++it)
	{
		const LLUUID& role_id = it->first;
		LLRoleData rd;
		if (gdatap->getRoleData(role_id, rd))
		{
			// Owners can add any role.
			if (is_owner ||
				// Even 'can_assign_any' cannot add owner role.
				(can_assign_any && role_id != gdatap->mOwnerRole) ||
				// Add all roles user is in
				(can_assign_limited && member_data &&
				  member_data->isInRole(role_id)) ||
				// Everyone role.
				role_id.isNull())
			{
				mRoleNames->add(rd.mRoleName, role_id, ADD_BOTTOM);
			}
		}
	}
}

//static
void LLPanelGroupInvite::impl::callbackClickAdd(void* userdata)
{
	LLPanelGroupInvite* self = (LLPanelGroupInvite*)userdata;
	if (self)
	{
		LLFloater* childp = LLFloaterAvatarPicker::show(callbackAddUsers,
														self->mImplementation);
		if (gFloaterViewp)
		{
			LLFloater* parentp = gFloaterViewp->getParentFloater(self);
			if (parentp)
			{
				parentp->addDependentFloater(childp);
			}
		}
	}
}

//static
void LLPanelGroupInvite::impl::callbackClickRemove(void* userdata)
{
	impl* self = (impl*)userdata;
	if (self)
	{
		self->handleRemove();
	}
}

void LLPanelGroupInvite::impl::handleRemove()
{
	// Check if there is anything selected.
	std::vector<LLScrollListItem*> selection = mInvitees->getAllSelected();
	if (selection.empty()) return;

	for (S32 i = 0, count = selection.size(); i < count; ++i)
	{
		mInviteeIDs.erase(selection[i]->getUUID());
	}

	// Remove all selected invitees.
	mInvitees->deleteSelectedItems();
	mRemoveButton->setEnabled(false);
	mInviteeIDs.clear();
}

// static
void LLPanelGroupInvite::impl::callbackSelect(LLUICtrl* ctrl, void* userdata)
{
	impl* self = (impl*)userdata;
	if (self)
	{
		self->handleSelection();
	}
}

void LLPanelGroupInvite::impl::handleSelection()
{
	// Check if there is anything selected.
	mRemoveButton->setEnabled(mInvitees->getFirstSelected() != NULL);
}

void LLPanelGroupInvite::impl::callbackClickCancel(void* userdata)
{
	impl* self = (impl*)userdata;
	if (self && self->mParentFloater)
	{
		self->mParentFloater->close();
	}
}

void LLPanelGroupInvite::impl::callbackClickOK(void* userdata)
{
	impl* self = (impl*)userdata;
	if (self)
	{
		self->submitInvitations();
	}
}

//static
void LLPanelGroupInvite::impl::callbackAddUsers(const std::vector<std::string>& names,
												const uuid_vec_t& ids,
												void* user_data)
{
	impl* self = (impl*)user_data;
	if (self)
	{
		self->addUsers(names, ids);
	}
}

//////////////////////////////////////////////////////////////////////
// LLPanelGroupInvite
//////////////////////////////////////////////////////////////////////

LLPanelGroupInvite::LLPanelGroupInvite(const LLUUID& group_id,
									   LLFloater* parent)
:	LLPanel(group_id.asString()),
	mImplementation(new impl(group_id, parent)),
	mPendingUpdate(false),
	mStoreSelected()
{
}

LLPanelGroupInvite::~LLPanelGroupInvite()
{
	if (mImplementation)
	{
		delete mImplementation;
		mImplementation = NULL;
	}
}

bool LLPanelGroupInvite::postBuild()
{
	if (!mImplementation) return false;

	mImplementation->mLoadingText = getString("loading");
	mImplementation->mRoleNames = getChild<LLComboBox>("role_name",
													   true, false);
	mImplementation->mGroupName = getChild<LLTextBox>("group_name_text",
													  true, false);

	LLNameListCtrl* list = getChild<LLNameListCtrl>("invitee_list",
													true, false);
	mImplementation->mInvitees = list;
	if (list)
	{
		list->setCallbackUserData(mImplementation);
		list->setCommitOnSelectionChange(true);
		list->setCommitCallback(impl::callbackSelect);
	}

	LLButton* button = getChild<LLButton>("add_button");
	// Default to opening avatarpicker automatically
	// (*impl::callbackClickAdd)((void*)this);
	button->setClickedCallback(impl::callbackClickAdd);
	button->setCallbackUserData(this);

	button = getChild<LLButton>("cancel_button");
	button->setClickedCallback(impl::callbackClickCancel);
	button->setCallbackUserData(mImplementation);

	button = getChild<LLButton>("remove_button", true, false);
	mImplementation->mRemoveButton = button;
	if (button)
	{
		button->setClickedCallback(impl::callbackClickRemove);
		button->setCallbackUserData(mImplementation);
		button->setEnabled(false);
	}

	button = getChild<LLButton>("invite_button", true, false);
	mImplementation->mOKButton = button;
	if (button)
 	{
		button->setClickedCallback(impl::callbackClickOK);
		button->setCallbackUserData(mImplementation);
		button->setEnabled(false);
 	}

	mImplementation->mOwnerWarning = getString("confirm_invite_owner_str");
	mImplementation->mTooManySelected =
		getString("invite_selection_too_large");

	update();

	return true;
}

void LLPanelGroupInvite::clear()
{
	mStoreSelected.setNull();

	if (!mImplementation) return;

	if (mImplementation->mInvitees)
	{
		mImplementation->mInvitees->deleteAllItems();
	}
	if (mImplementation->mRoleNames)
	{
		mImplementation->mRoleNames->clear();
		mImplementation->mRoleNames->removeall();
	}
	if (mImplementation->mOKButton)
	{
		mImplementation->mOKButton->setEnabled(false);
	}
}

void LLPanelGroupInvite::addUsers(uuid_vec_t& agent_ids)
{
	std::vector<std::string> names;
	for (S32 i = 0, count = agent_ids.size(); i < count; ++i)
	{
		LLUUID agent_id = agent_ids[i];
		LLVOAvatar* avatarp = gObjectList.findAvatar(agent_id);
		if (avatarp)
		{
			std::string fullname;
			LLSD args;
			LLNameValue* nvfirst = avatarp->getNVPair("FirstName");
			LLNameValue* nvlast = avatarp->getNVPair("LastName");
			if (nvfirst && nvlast)
			{
				args["FIRST"] = std::string(nvfirst->getString());
				args["LAST"] = std::string(nvlast->getString());
				fullname = std::string(nvfirst->getString()) + " " +
						   std::string(nvlast->getString());
			}
			if (!fullname.empty())
			{
				names.emplace_back(fullname);
			}
			else
			{
				llwarns << "Selected avatar has no name: " << avatarp->getID()
						<< llendl;
				names.emplace_back("(Unknown)");
			}
		}
	}
	mImplementation->addUsers(names, agent_ids);
}

void LLPanelGroupInvite::draw()
{
	if (mPendingUpdate)
	{
		updateLists();
	}
	LLPanel::draw();
}

void LLPanelGroupInvite::update()
{
	mPendingUpdate = false;

	if (!mImplementation) return;

	if (mImplementation->mGroupName)
	{
		mImplementation->mGroupName->setText(mImplementation->mLoadingText);
	}
	if (mImplementation->mRoleNames)
	{
		mStoreSelected = mImplementation->mRoleNames->getCurrentID();
		mImplementation->mRoleNames->clear();
		mImplementation->mRoleNames->removeall();
		mImplementation->mRoleNames->add(mImplementation->mLoadingText,
										 LLUUID::null, ADD_BOTTOM);
		mImplementation->mRoleNames->setCurrentByID(LLUUID::null);
	}

	updateLists();
}

void LLPanelGroupInvite::updateLists()
{
	if (!mImplementation) return;

	bool waiting = false;
	LLGroupMgrGroupData* gdatap =
		gGroupMgr.getGroupData(mImplementation->mGroupID);
	if (gdatap)
	{
		if (gdatap->isGroupPropertiesDataComplete())
		{
			if (mImplementation->mGroupName)
			{
				mImplementation->mGroupName->setText(gdatap->mName);
			}
		}
		else
		{
			waiting = true;
		}
		if (gdatap->isRoleDataComplete() && gdatap->isMemberDataComplete() &&
			(gdatap->isRoleMemberDataComplete() ||
			  // MAINT-5270: large groups receives an empty members list
			  // without some powers, so RoleMemberData would not be complete
			  // for them.
			 !gdatap->mMembers.size()))
		{
			if (mImplementation->mRoleNames)
			{
				mImplementation->mRoleNames->clear();
				mImplementation->mRoleNames->removeall();

				// Add the role names and select the everybody role by default
				mImplementation->addRoleNames(gdatap);
				mImplementation->mRoleNames->setCurrentByID(mStoreSelected);
			}
		}
		else
		{
			waiting = true;
		}
	}
	else
	{
		waiting = true;
	}

	if (waiting)
	{
		if (!mPendingUpdate)
		{
			// NOTE: this will partially fail if some requests are already in
			// progress
			gGroupMgr.sendGroupPropertiesRequest(mImplementation->mGroupID);
			gGroupMgr.sendGroupRoleDataRequest(mImplementation->mGroupID);
			gGroupMgr.sendGroupRoleMembersRequest(mImplementation->mGroupID);
			gGroupMgr.sendCapGroupMembersRequest(mImplementation->mGroupID);
		}
		else if (gdatap)
		{
			// Restart requests that were interrupted/dropped/failed to start
			if (!gdatap->isRoleDataPending() && !gdatap->isRoleDataComplete())
			{
				gGroupMgr.sendGroupRoleDataRequest(mImplementation->mGroupID);
			}
			if (!gdatap->isRoleMemberDataPending() &&
				!gdatap->isRoleMemberDataComplete())
			{
				gGroupMgr.sendGroupRoleMembersRequest(mImplementation->mGroupID);
			}
			// sendCapGroupMembersRequest() has a per frame send limitation
			// that could have interrupted previous request
			if (!gdatap->isMemberDataPending() && !gdatap->isMemberDataComplete())
			{
				gGroupMgr.sendCapGroupMembersRequest(mImplementation->mGroupID);
			}
		}
		mPendingUpdate = true;
	}
	else
	{
		mPendingUpdate = false;
		if (mImplementation->mOKButton &&
			mImplementation->mRoleNames->getItemCount())
		{
			mImplementation->mOKButton->setEnabled(true);
		}
	}
}

//////////////////////////////////////////////////////////////////////
// LLFloaterGroupInvite
//////////////////////////////////////////////////////////////////////

// static
void* LLFloaterGroupInvite::createPanel(void* userdata)
{
	LLFloaterGroupInviteData* data = (LLFloaterGroupInviteData*)userdata;
	if (data)
	{
		data->mPanel = new LLPanelGroupInvite(data->mGroupId, data->mSelf);
		return data->mPanel;
	}
	else
	{
		return NULL;
	}
}

LLFloaterGroupInvite::LLFloaterGroupInvite(const LLUUID& group_id)
:	LLFloater(group_id.asString()),
	mGroupID(group_id),
	mInvitePanelp(NULL)
{
	// Create the group bulk ban panel together with this floater
	LLFloaterGroupInviteData* data;
	data = new LLFloaterGroupInviteData(this, group_id);
	LLCallbackMap::map_t factory_map;
	factory_map["invite_panel"] = LLCallbackMap(createPanel, data);
	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_group_invite.xml",
												 &factory_map);
	mInvitePanelp = data->mPanel;
	delete data;
}

//virtual
LLFloaterGroupInvite::~LLFloaterGroupInvite()
{
	if (mGroupID.notNull())
	{
		sInstances.erase(mGroupID);
	}

	if (mInvitePanelp)
	{
		delete mInvitePanelp;
		mInvitePanelp = NULL;
	}
}

//static
void LLFloaterGroupInvite::showForGroup(const LLUUID& group_id,
										uuid_vec_t* agent_ids,
										LLView* parent)
{
	if (group_id.isNull())
	{
		llwarns << "Null group_id passed !  Aborting." << llendl;
		return;
	}

	// If we do not have a floater for this group, create one.
	LLFloaterGroupInvite* fgi = get_ptr_in_map(sInstances, group_id);
	if (!fgi)
	{
		fgi = new LLFloaterGroupInvite(group_id);
		if (!fgi || !fgi->mInvitePanelp)
		{
			llwarns << "Could not create the floater !  Aborting." << llendl;
			return;
		}
		if (parent && gFloaterViewp && gFloaterViewp->getParentFloater(parent))
		{
			gFloaterViewp->getParentFloater(parent)->addDependentFloater(fgi);
		}

		sInstances[group_id] = fgi;
		fgi->mInvitePanelp->clear();
	}

	if (!fgi->mInvitePanelp)	// Paranoia
	{
		llwarns << "NULL panel in floater !  Aborting." << llendl;
		return;
	}

	if (agent_ids)
	{
		fgi->mInvitePanelp->addUsers(*agent_ids);
	}

	fgi->open();
	fgi->mInvitePanelp->update();
}
