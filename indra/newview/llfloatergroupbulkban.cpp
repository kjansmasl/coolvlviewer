/**
 * @file llfloatergroupbulkban.cpp
 * @brief Floater to ban Residents from a group.
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

#include "boost/foreach.hpp"

#include "llfloatergroupbulkban.h"

#include "llavatarnamecache.h"
#include "llbutton.h"
#include "llnamelistctrl.h"
#include "llnotifications.h"
#include "llscrolllistctrl.h"
#include "lltextbox.h"
#include "lltrans.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "llavataractions.h"
#include "llavatartracker.h"
#include "llfloateravatarpicker.h"
#include "llpanelgroupbulk.h"
#include "llgroupmgr.h"
#include "llpanelgroupbulk.h"
#include "llviewerobjectlist.h"

// Globals

LLFloaterGroupBulkBan::instances_map_t LLFloaterGroupBulkBan::sInstances;

class LLFloaterGroupBulkBanData
{
public:
	LLFloaterGroupBulkBanData(LLFloater* self, const LLUUID& group_id)
	:	mSelf(self),
		mGroupId(group_id),
		mPanel(NULL)
	{
	}

	~LLFloaterGroupBulkBanData()
	{
	}

	LLFloater*				mSelf;
	LLUUID					mGroupId;
	LLPanelGroupBulkBan*	mPanel;
};

//////////////////////////////////////////////////////////////////////
// LLPanelGroupBulkBan
//////////////////////////////////////////////////////////////////////

class LLPanelGroupBulkBan final : public LLPanelGroupBulk
{
public:
	LLPanelGroupBulkBan(const LLUUID& group_id, LLFloater* parent);
	~LLPanelGroupBulkBan() {}

	bool postBuild() override;

	static void callbackClickSubmit(void* userdata);

	void submit() override;

private:
	std::string buildAvListArgument(std::vector<LLAvatarName> av_names,
									const std::string& format);
};

LLPanelGroupBulkBan::LLPanelGroupBulkBan(const LLUUID& group_id,
										 LLFloater* parent)
:	LLPanelGroupBulk(group_id, parent)
{
}

bool LLPanelGroupBulkBan::postBuild()
{
	if (!mImplementation) return false;

	mImplementation->mLoadingText = getString("loading");
	mImplementation->mGroupName = getChild<LLTextBox>("group_name_text",
													  true, false);
	LLNameListCtrl* list = getChild<LLNameListCtrl>("banned_agent_list",
													true, false);
	mImplementation->mBulkAgentList = list;
	if (list)
	{
		list->setCommitOnSelectionChange(true);
		list->setCommitCallback(LLPanelGroupBulkImpl::callbackSelect);
		list->setCallbackUserData(mImplementation);
	}

	LLButton* button = getChild<LLButton>("add_button", true, false);
	if (button)
	{
		// default to opening avatarpicker automatically
		// (*impl::callbackClickAdd)((void*)this);
		button->setClickedCallback(LLPanelGroupBulkImpl::callbackClickAdd,
								   this);
	}

	button = getChild<LLButton>("remove_button", true, false);
	mImplementation->mRemoveButton = button;
	if (button)
	{
		button->setClickedCallback(LLPanelGroupBulkImpl::callbackClickRemove,
								   mImplementation);
		button->setEnabled(false);
	}

	button = getChild<LLButton>("ban_button", true, false);
	mImplementation->mOKButton = button;
	if (button)
	{
		button->setClickedCallback(LLPanelGroupBulkBan::callbackClickSubmit,
													   this);
		button->setEnabled(false);
	}

	button = getChild<LLButton>("cancel_button", true, false);
	if (button)
	{
		button->setClickedCallback(LLPanelGroupBulkImpl::callbackClickCancel,
								   mImplementation);
	}

	mImplementation->mTooManySelected = getString("ban_selection_too_large");

	update();

	return true;
}

void LLPanelGroupBulkBan::callbackClickSubmit(void* userdata)
{
	LLPanelGroupBulkBan* selfp = (LLPanelGroupBulkBan*)userdata;
	if (selfp)
	{
		selfp->submit();
	}
}

void LLPanelGroupBulkBan::submit()
{
	if (!mImplementation || !mImplementation->mBulkAgentList) return;

	if (!gAgent.hasPowerInGroup(mImplementation->mGroupID,
								GP_GROUP_BAN_ACCESS))
	{
		// Fail !  Agent no longer has ban rights (permissions could have
		// changed after button was pressed).
		LLSD msg;
		msg["MESSAGE"] = getString("ban_not_permitted");
		gNotifications.add("GenericAlert", msg);
		if (mImplementation->mParentFloater)	// Paranoia
		{
			mImplementation->mParentFloater->close();
		}
		return;
	}

	LLGroupMgrGroupData* gdatap =
		gGroupMgr.getGroupData(mImplementation->mGroupID);

	if (gdatap && gdatap->mBanList.size() >= GB_MAX_BANNED_AGENTS)
	{
		// Fail !  Size limit exceeded. List could have updated after button
		// was pressed.
		LLSD msg;
		msg["MESSAGE"] = getString("ban_limit_fail");
		gNotifications.add("GenericAlert", msg);
		if (mImplementation->mParentFloater)	// Paranoia
		{
			mImplementation->mParentFloater->close();
		}
		return;
	}

	uuid_vec_t banned_agent_list;
	std::vector<LLScrollListItem*> agents = mImplementation->mBulkAgentList->getAllData();
	for (std::vector<LLScrollListItem*>::iterator iter = agents.begin(),
												  end = agents.end();
		 iter != end; ++iter)
	{
		LLScrollListItem* agent = *iter;
		if (agent)	// Paranoia
		{
			banned_agent_list.emplace_back(agent->getUUID());
		}
	}

	// Max bans (= max invites) per request to match server cap.
	if ((S32)banned_agent_list.size() > LLPanelGroupBulkImpl::MAX_GROUP_INVITES)
	{
		// Fail!
		LLSD msg;
		msg["MESSAGE"] = mImplementation->mTooManySelected;
		gNotifications.add("GenericAlert", msg);
		if (mImplementation->mParentFloater)	// Paranoia
		{
			mImplementation->mParentFloater->close();
		}
		return;
	}

	// Remove already banned users and yourself from request.
	bool banning_self = false;
	uuid_vec_t::iterator reject = std::find(banned_agent_list.begin(),
											banned_agent_list.end(), gAgentID);
	if (reject != banned_agent_list.end())
	{
		banned_agent_list.erase(reject);
		banning_self = true;
	}

	// Will hold already banned avatars and avatars not banned because of the
	// bans number limit
	std::vector<LLAvatarName> already_banned_avnames;
	std::vector<LLAvatarName> out_of_limit_avnames;

	if (gdatap)
	{
		BOOST_FOREACH(const LLGroupMgrGroupData::ban_list_t::value_type& group_ban_pair,
					  gdatap->mBanList)
		{
			const LLUUID& group_ban_agent_id = group_ban_pair.first;
			reject = std::find(banned_agent_list.begin(),
							   banned_agent_list.end(), group_ban_agent_id);
			if (reject != banned_agent_list.end())
			{
				LLAvatarName av_name;
				LLAvatarNameCache::get(group_ban_agent_id, &av_name);
				already_banned_avnames.emplace_back(av_name);
				banned_agent_list.erase(reject);
				if (banned_agent_list.size() == 0)
				{
					break;
				}
			}

			// This check should always be the last one before we send the
			// request, otherwise we have a possibility of cutting more then we
			// need to.
			if (banned_agent_list.size() >
					GB_MAX_BANNED_AGENTS - gdatap->mBanList.size())
			{
				reject = banned_agent_list.begin() + GB_MAX_BANNED_AGENTS -
						 gdatap->mBanList.size();
				uuid_vec_t::iterator list_end = banned_agent_list.end();
				for (uuid_vec_t::iterator it = reject; it != list_end; ++it)
				{
					LLAvatarName av_name;
					LLAvatarNameCache::get(*it, &av_name);
					out_of_limit_avnames.emplace_back(av_name);
				}
				banned_agent_list.erase(reject, banned_agent_list.end());
			}
		}
	}

	// Send the request and eject the members
	if (banned_agent_list.size())
	{
		gGroupMgr.sendGroupBanRequest(LLGroupMgr::REQUEST_POST,
									  mImplementation->mGroupID,
									  LLGroupMgr::BAN_CREATE |
									  LLGroupMgr::BAN_UPDATE,
									  banned_agent_list);
		gGroupMgr.sendGroupMemberEjects(mImplementation->mGroupID,
										banned_agent_list);
	}

	// Build and issue the notification if needed
	bool already_banned = already_banned_avnames.size() > 0;
	bool out_limit = out_of_limit_avnames.size() > 0;
	if (already_banned || banning_self || out_limit)
	{
		std::string reasons;
		if (already_banned)
		{
			reasons += "\n " + buildAvListArgument(already_banned_avnames,
												   "already_banned");
		}
		if (banning_self)
		{
			reasons += "\n " + getString("cant_ban_yourself");
		}
		if (out_limit)
		{
			reasons += "\n " + buildAvListArgument(out_of_limit_avnames,
												   "ban_limit_reached");
		}

		LLStringUtil::format_map_t msg_args;
		msg_args["[REASONS]"] = reasons;
		LLSD msg;
		if (already_banned)
		{
			msg["MESSAGE"] = getString("partial_ban", msg_args);
		}
		else
		{
			msg["MESSAGE"] = getString("ban_failed", msg_args);
		}

		gNotifications.add("GenericAlert", msg);
	}

	// then close
	if (mImplementation->mParentFloater)	// Paranoia
	{
		mImplementation->mParentFloater->close();
	}
}

std::string LLPanelGroupBulkBan::buildAvListArgument(std::vector<LLAvatarName> av_names,
													 const std::string& format)
{
	std::string names_string;
	LLAvatarActions::buildAvatarsList(av_names, names_string, true);
	LLStringUtil::format_map_t args;
	args["[RESIDENTS]"] = names_string;
	return getString(format, args);

}

///////////////////////////////////////////////////////////////////////////////
// LLFloaterGroupBulkBan
///////////////////////////////////////////////////////////////////////////////

// static
void* LLFloaterGroupBulkBan::createPanel(void* userdata)
{
	LLFloaterGroupBulkBanData* data = (LLFloaterGroupBulkBanData*)userdata;
	if (data)
	{
		data->mPanel = new LLPanelGroupBulkBan(data->mGroupId, data->mSelf);
		return data->mPanel;
	}
	else
	{
		return NULL;
	}
}

LLFloaterGroupBulkBan::LLFloaterGroupBulkBan(const LLUUID& group_id)
:	LLFloater(group_id.asString()),
	mGroupID(group_id),
	mBulkBanPanelp(NULL)
{
	// Create the group bulk ban panel together with this floater
	LLFloaterGroupBulkBanData* data = new LLFloaterGroupBulkBanData(this,
																	group_id);
	LLCallbackMap::map_t factory_map;
	factory_map["bulk_ban_panel"] = LLCallbackMap(createPanel, data);
	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_group_ban.xml",
												 &factory_map);
	mBulkBanPanelp = data->mPanel;
	delete data;
}

//virtual
LLFloaterGroupBulkBan::~LLFloaterGroupBulkBan()
{
	if (mGroupID.notNull())
	{
		sInstances.erase(mGroupID);
	}

	if (mBulkBanPanelp)
	{
		delete mBulkBanPanelp;
		mBulkBanPanelp = NULL;
	}
}

//static
void LLFloaterGroupBulkBan::showForGroup(const LLUUID& group_id,
										 uuid_vec_t* agent_ids,
										 LLView* parent)
{
	// Make sure group_id isn't null
	if (group_id.isNull())
	{
		llwarns << "Null group_id passed !  Aborting." << llendl;
		return;
	}

	// If we do not have a floater for this group, create one.
	LLFloaterGroupBulkBan* fgb = get_ptr_in_map(sInstances, group_id);
	if (!fgb)
	{
		fgb = new LLFloaterGroupBulkBan(group_id);
		if (!fgb || !fgb->mBulkBanPanelp)
		{
			llwarns << "Could not create the floater !  Aborting." << llendl;
			return;
		}
		if (parent && gFloaterViewp && gFloaterViewp->getParentFloater(parent))
		{
			gFloaterViewp->getParentFloater(parent)->addDependentFloater(fgb);
		}

		sInstances[group_id] = fgb;
		fgb->mBulkBanPanelp->clear();
	}

	if (!fgb->mBulkBanPanelp)	// Paranoia
	{
		llwarns << "NULL panel in floater !  Aborting." << llendl;
		return;
	}

	if (agent_ids)
	{
		fgb->mBulkBanPanelp->addUsers(*agent_ids);
	}

	fgb->open();
	fgb->mBulkBanPanelp->update();
}
