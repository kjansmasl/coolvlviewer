/**
 * @file llpanelgroupbulk.cpp
 * @brief Implementation of llpanelgroupbulk
 * @author Baker@lindenlab.com
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

#include "llpanelgroupbulk.h"

#include "llavatarnamecache.h"
#include "llbutton.h"
#include "llcombobox.h"
#include "llnamelistctrl.h"
#include "llnotifications.h"
#include "llscrolllistctrl.h"
#include "lltextbox.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "llavatartracker.h"
#include "llfloateravatarpicker.h"
#include "llgroupmgr.h"
#include "llviewerobjectlist.h"

static fast_hset<LLPanelGroupBulkImpl*> sImplList;

//////////////////////////////////////////////////////////////////////////
// LLPanelGroupBulkImpl
//////////////////////////////////////////////////////////////////////////
LLPanelGroupBulkImpl::LLPanelGroupBulkImpl(const LLUUID& group_id,
										   LLFloater* parent)
:	mGroupID(group_id),
	mParentFloater(parent),
	mBulkAgentList(NULL),
	mOKButton(NULL),
	mRemoveButton(NULL),
	mGroupName(NULL),
	mLoadingText(),
	mTooManySelected(),
	mRoleNames(NULL),
	mOwnerWarning(),
	mAlreadyInGroup(),
	mConfirmedOwnerInvite(false),
	mListFullNotificationSent(false)
{
	sImplList.insert(this);
}

LLPanelGroupBulkImpl::~LLPanelGroupBulkImpl()
{
	sImplList.erase(this);
}

//static
void LLPanelGroupBulkImpl::callbackClickAdd(void* userdata)
{
	LLPanelGroupBulk* panelp = (LLPanelGroupBulk*)userdata;
	if (panelp && panelp->mImplementation)
	{
		LLFloaterAvatarPicker* picker;
		picker = LLFloaterAvatarPicker::show(callbackAddUsers,
											 panelp->mImplementation,
											 true, false);
		if (picker && gFloaterViewp)
		{
			LLFloater* parentp = gFloaterViewp->getParentFloater(panelp);
			if (parentp)
			{
				parentp->addDependentFloater(picker);
			}
		}
		gGroupMgr.sendCapGroupMembersRequest(panelp->mImplementation->mGroupID);
	}
}

//static
void LLPanelGroupBulkImpl::callbackClickRemove(void* userdata)
{
	LLPanelGroupBulkImpl* selfp = (LLPanelGroupBulkImpl*)userdata;
	if (selfp)
	{
		selfp->handleRemove();
	}
}

//static
void LLPanelGroupBulkImpl::callbackClickCancel(void* userdata)
{
	LLPanelGroupBulkImpl* selfp = (LLPanelGroupBulkImpl*)userdata;
	if (selfp && selfp->mParentFloater)
	{
		selfp->mParentFloater->close();
	}
}

//static
void LLPanelGroupBulkImpl::callbackSelect(LLUICtrl* ctrl, void* userdata)
{
	LLPanelGroupBulkImpl* selfp = (LLPanelGroupBulkImpl*)userdata;
	if (selfp)
	{
		selfp->handleSelection();
	}
}

//static
void LLPanelGroupBulkImpl::callbackAddUsers(const std::vector<std::string>&,
											const uuid_vec_t& agent_ids,
											void* user_data)
{
	LLPanelGroupBulkImpl* selfp = (LLPanelGroupBulkImpl*)user_data;
	if (!sImplList.count(selfp))
	{
		return;
	}
	std::vector<std::string> names;
	for (S32 i = 0, count = agent_ids.size(); i < count; ++i)
	{
		LLAvatarName av_name;
		if (LLAvatarNameCache::get(agent_ids[i], &av_name))
		{
			onAvatarNameCache(agent_ids[i], av_name, selfp);
		}
		else
		{
			// *TODO : Add a callback per avatar name being fetched.
			LLAvatarNameCache::get(agent_ids[i],
								   boost::bind(onAvatarNameCache, _1, _2,
											   selfp));
		}
	}
}

//static
void LLPanelGroupBulkImpl::onAvatarNameCache(const LLUUID& agent_id,
											 const LLAvatarName& av_name,
											 void* user_data)
{
	LLPanelGroupBulkImpl* selfp = (LLPanelGroupBulkImpl*)user_data;
	if (sImplList.count(selfp))
	{
		std::vector<std::string> names;
		uuid_vec_t agent_ids;
		agent_ids.emplace_back(agent_id);
		names.emplace_back(av_name.getCompleteName());
		selfp->addUsers(names, agent_ids);
	}
}

void LLPanelGroupBulkImpl::handleRemove()
{
	if (!mBulkAgentList) return;

	std::vector<LLScrollListItem*> selection = mBulkAgentList->getAllSelected();
	if (selection.empty())
	{
		return;
	}

	for (std::vector<LLScrollListItem*>::iterator iter = selection.begin(),
												  end = selection.end();
		 iter != end; ++iter)
	{
		mInviteeIDs.erase((*iter)->getUUID());
	}

	mBulkAgentList->deleteSelectedItems();
	if (mRemoveButton)
	{
		mRemoveButton->setEnabled(false);
	}

	if (mOKButton && mOKButton->getEnabled() && mBulkAgentList->isEmpty())
	{
		mOKButton->setEnabled(false);
	}
}

void LLPanelGroupBulkImpl::handleSelection()
{
	if (!mBulkAgentList) return;

	std::vector<LLScrollListItem*> selection = mBulkAgentList->getAllSelected();
	if (mRemoveButton)
	{
		mRemoveButton->setEnabled(!selection.empty());
	}
}

void LLPanelGroupBulkImpl::addUsers(const std::vector<std::string>& names,
									const uuid_vec_t& agent_ids)
{
	if (mListFullNotificationSent || !mBulkAgentList)
	{
		return;
	}

	if (!mListFullNotificationSent &&
		(S32)(names.size() + mInviteeIDs.size()) > MAX_GROUP_INVITES)
	{
		mListFullNotificationSent = true;

		// Fail !  Show a warning and don't add any names.
		LLSD msg;
		msg["MESSAGE"] = mTooManySelected;
		gNotifications.add("GenericAlert", msg);
		return;
	}

	for (S32 i = 0, count = names.size(); i < count; ++i)
	{
		const LLUUID& id = agent_ids[i];
		if (mInviteeIDs.count(id))
		{
			continue;
		}

		// add the name to the names list
		LLSD row;
		row["id"] = id;
		row["columns"][0]["value"] = names[i];

		mBulkAgentList->addElement(row);
		mInviteeIDs.emplace(id);

		// We've successfully added someone to the list.
		if (mOKButton && !mOKButton->getEnabled())
		{
			mOKButton->setEnabled(true);
		}
	}
}

void LLPanelGroupBulkImpl::setGroupName(std::string name)
{
	if (mGroupName)
	{
		mGroupName->setText(name);
	}
}

//////////////////////////////////////////////////////////////////////////
// LLPanelGroupBulk
//////////////////////////////////////////////////////////////////////////
LLPanelGroupBulk::LLPanelGroupBulk(const LLUUID& group_id, LLFloater* parent)
:	LLPanel(),
	mImplementation(new LLPanelGroupBulkImpl(group_id, parent)),
	mPendingGroupPropertiesUpdate(false),
	mPendingRoleDataUpdate(false),
	mPendingMemberDataUpdate(false)
{
}

LLPanelGroupBulk::~LLPanelGroupBulk()
{
	if (mImplementation)
	{
		delete mImplementation;
		mImplementation = NULL;
	}
}

void LLPanelGroupBulk::clear()
{
	mImplementation->mInviteeIDs.clear();

	if (mImplementation->mBulkAgentList)
	{
		mImplementation->mBulkAgentList->deleteAllItems();
	}

	if (mImplementation->mOKButton)
	{
		mImplementation->mOKButton->setEnabled(false);
	}
}

void LLPanelGroupBulk::update()
{
	updateGroupName();
	updateGroupData();
}

void LLPanelGroupBulk::draw()
{
	LLPanel::draw();
	update();
}

void LLPanelGroupBulk::updateGroupName()
{
	LLGroupMgrGroupData* gdatap =
		gGroupMgr.getGroupData(mImplementation->mGroupID);
	if (gdatap && gdatap->isGroupPropertiesDataComplete())
	{
		// Only do work if the current group name differs
		if (mImplementation->mGroupName &&
			mImplementation->mGroupName->getText().compare(gdatap->mName) != 0)
		{
			mImplementation->setGroupName(gdatap->mName);
		}
	}
	else
	{
		mImplementation->setGroupName(mImplementation->mLoadingText);
	}
}

void LLPanelGroupBulk::updateGroupData()
{
	LLGroupMgrGroupData* gdatap =
		gGroupMgr.getGroupData(mImplementation->mGroupID);
	if (gdatap && gdatap->isGroupPropertiesDataComplete())
	{
		mPendingGroupPropertiesUpdate = false;
	}
	else if (!mPendingGroupPropertiesUpdate)
	{
		mPendingGroupPropertiesUpdate = true;
		gGroupMgr.sendGroupPropertiesRequest(mImplementation->mGroupID);
	}

	if (gdatap && gdatap->isRoleDataComplete())
	{
		mPendingRoleDataUpdate = false;
	}
	else if (!mPendingRoleDataUpdate)
	{
		mPendingRoleDataUpdate = true;
		gGroupMgr.sendGroupRoleDataRequest(mImplementation->mGroupID);
	}

	if (gdatap && gdatap->isMemberDataComplete())
	{
		mPendingMemberDataUpdate = false;
	}
	else if (!mPendingMemberDataUpdate)
	{
		mPendingMemberDataUpdate = true;
		gGroupMgr.sendCapGroupMembersRequest(mImplementation->mGroupID);
	}
}

void LLPanelGroupBulk::addUserCallback(const LLUUID& id,
									   const LLAvatarName& av_name)
{
	std::vector<std::string> names;
	uuid_vec_t agent_ids;
	agent_ids.emplace_back(id);
	names.emplace_back(av_name.getLegacyName());
	mImplementation->addUsers(names, agent_ids);
}

void LLPanelGroupBulk::addUsers(uuid_vec_t& agent_ids)
{
	std::vector<std::string> names;

	for (S32 i = 0, count = agent_ids.size(); i < count; ++i)
	{
		std::string fullname;
		LLUUID agent_id = agent_ids[i];
		LLViewerObject* dest = gObjectList.findObject(agent_id);
		if (dest && dest->isAvatar())
		{
			LLNameValue* nvfirst = dest->getNVPair("FirstName");
			LLNameValue* nvlast = dest->getNVPair("LastName");
			if (nvfirst && nvlast)
			{
				fullname = LLCacheName::buildFullName(nvfirst->getString(),
													  nvlast->getString());

			}
			if (!fullname.empty())
			{
				names.emplace_back(fullname);
			}
			else
			{
				llwarns << "Selected avatar has no name: " << dest->getID()
						<< llendl;
				names.emplace_back("(Unknown)");
			}
		}
		else
		{
			// It looks like the user tries to invite offline friend; for
			// offline avatar_id gObjectList.findObject() will return null so
			// we need to do this additional search in avatar tracker, see
			// EXT-4732
			if (LLAvatarTracker::isAgentFriend(agent_id))
			{
				LLAvatarName av_name;
				if (!LLAvatarNameCache::get(agent_id, &av_name))
				{
					// actually it should happen, just in case
					LLAvatarNameCache::get(LLUUID(agent_id),
										   boost::bind(&LLPanelGroupBulk::addUserCallback,
													   this, _1, _2));
					// For this special case !
					// When there is no cached name we should remove resident
					// from agent_ids list to avoid breaking of sequence
					// removed id will be added in callback.
					agent_ids.erase(agent_ids.begin() + i);
				}
				else
				{
					names.emplace_back(av_name.getLegacyName());
				}
			}
		}
	}

	mImplementation->mListFullNotificationSent = false;
	mImplementation->addUsers(names, agent_ids);
}
