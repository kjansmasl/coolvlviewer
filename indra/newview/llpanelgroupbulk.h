/**
 * @file   llpanelgroupbulk.h
 * @brief  Header file for llpanelgroupbulk
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

#ifndef LL_LLPANELGROUPBULK_H
#define LL_LLPANELGROUPBULK_H

#include "llerror.h"
#include "llpanel.h"
#include "lluuid.h"

class LLAvatarName;
class LLComboBox;
class LLFloater;
class LLGroupMgrGroupData;
class LLNameListCtrl;
class LLPanelGroupBulkImpl;
class LLTextBox;

// Base panel class for bulk group invite / ban floaters
class LLPanelGroupBulk : public LLPanel
{
protected:
	LOG_CLASS(LLPanelGroupBulk);

public:
	LLPanelGroupBulk(const LLUUID& group_id, LLFloater* parent);
	~LLPanelGroupBulk() override;

	void clear() override;
	virtual void update();

	void draw() override;

	static void callbackClickSubmit(void* userdata)	{}
	virtual void submit() = 0;

	// this callback is being used to add a user whose fullname isn't been
	// loaded before invoking of addUsers().
	virtual void addUserCallback(const LLUUID& id,
								 const LLAvatarName& av_name);

	virtual void addUsers(uuid_vec_t& agent_ids);

protected:
	virtual void updateGroupName();
	virtual void updateGroupData();

public:
	LLPanelGroupBulkImpl* mImplementation;

protected:
	bool mPendingGroupPropertiesUpdate;
	bool mPendingRoleDataUpdate;
	bool mPendingMemberDataUpdate;
};

class LLPanelGroupBulkImpl
{
protected:
	LOG_CLASS(LLPanelGroupBulkImpl);

public:
	LLPanelGroupBulkImpl(const LLUUID& group_id, LLFloater* parent);
	~LLPanelGroupBulkImpl();

	static void callbackClickAdd(void* userdata);
	static void callbackClickRemove(void* userdata);

	static void callbackClickCancel(void* userdata);

	static void callbackSelect(LLUICtrl* ctrl, void* userdata);
	static void callbackAddUsers(const std::vector<std::string>&,
								 const uuid_vec_t& agent_ids, void* user_data);

	static void onAvatarNameCache(const LLUUID& agent_id,
								  const LLAvatarName& av_name,
								  void* user_data);

	void handleRemove();
	void handleSelection();

	void addUsers(const std::vector<std::string>& names,
				  const uuid_vec_t& agent_ids);
	void setGroupName(std::string name);

public:
	// Max invites per request. 100 to match server cap.
	static constexpr S32 MAX_GROUP_INVITES = 100;

	LLFloater*			mParentFloater;
	LLUUID				mGroupID;

	LLNameListCtrl*		mBulkAgentList;
	LLButton*			mOKButton;
	LLButton*			mRemoveButton;
	LLTextBox*			mGroupName;

	std::string			mLoadingText;
	std::string			mTooManySelected;

	uuid_list_t			mInviteeIDs;

	// The following are for the LLPanelGroupInvite subclass only. These aren't
	// needed for LLPanelGroupBulkBan, but if we have to add another group bulk
	// floater for some reason, we'll have these objects too.
	LLComboBox*		mRoleNames;
	bool			mConfirmedOwnerInvite;
	bool			mListFullNotificationSent;
	std::string		mOwnerWarning;
	std::string		mAlreadyInGroup;
};

#endif // LL_LLPANELGROUPBULK_H
