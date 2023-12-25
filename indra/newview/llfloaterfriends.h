/**
 * @file llfloaterfriends.h
 * @author Phoenix
 * @date 2005-01-13
 * @brief Declaration of class for displaying the local agent's friends.
 *
 * $LicenseInfo:firstyear=2005&license=viewergpl$
 *
 * Copyright (c) 2005-2009, Linden Research, Inc.
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

#ifndef LL_LLFLOATERFRIENDS_H
#define LL_LLFLOATERFRIENDS_H

#include "lleventtimer.h"
#include "llfloater.h"

#include "llavatartracker.h"

class LLButton;
class LLFriendObserver;
class LLScrollListCtrl;
class LLScrollListItem;

/**
 * @class LLFloaterFriends
 * @brief An instance of this class is used for displaying your friends
 * and gives you quick access to all agents which a user relationship.
 *
 * @sa LLFloater
 */
class LLFloaterFriends final : public LLFloater,
							   public LLFloaterSingleton<LLFloaterFriends>,
							   public LLEventTimer
{
	friend class LLUISingleton<LLFloaterFriends, VisibilityPolicy<LLFloater> >;
	friend class LLLocalFriendsObserver;

protected:
	LOG_CLASS(LLFloaterFriends);

public:
	~LLFloaterFriends() override;

	bool postBuild() override;

	// LLEventTimer interface
	bool tick() override;

private:
	// This method is also called by LLLocalFriendsObserver
	void updateFriends(U32 changed_mask, const uuid_list_t& buddies);

	// Open only via LLFloaterSingleton interface, i.e. showInstance() or
	// toggleInstance().
	LLFloaterFriends(const LLSD&);

	enum FRIENDS_COLUMN_ORDER
	{
		LIST_ONLINE_STATUS,
		LIST_FRIEND_NAME,
		LIST_VISIBLE_ONLINE,
		LIST_VISIBLE_MAP,
		LIST_EDIT_MINE,
		LIST_ONLINE_OR_MAP_THEIRS,
		LIST_EDIT_THEIRS,
		LIST_FRIEND_UPDATE_GEN
	};

	void refreshNames();
	void refreshUI();
	void refreshRightsChangeList();
	void applyRightsToFriends();
	bool addFriend(const LLUUID& agent_id);
	bool updateFriendItem(const LLUUID& agent_id);

	typedef enum
	{
		GRANT,
		REVOKE
	} EGrantRevoke;
	typedef fast_hmap<LLUUID, S32> rights_map_t;
	void confirmModifyRights(rights_map_t& ids, EGrantRevoke command);
	void sendRightsGrant(rights_map_t& ids);

	// Returns LLUUID::null if nothing is selected
	uuid_vec_t getSelectedIDs();

	// Callback methods
	static void onSelectName(LLUICtrl* ctrl, void* data);
	static bool callbackAddFriendWithMessage(const LLSD& notification,
											 const LLSD& response);
	static void onPickAvatar(const std::vector<std::string>& names,
							 const std::vector<LLUUID>& ids, void* data);

	static void onMaximumSelect(void* data);
	static void onClickClose(void* data);
	static void onClickProfile(void* data);
	static void onClickIM(void* data);
	static void onClickOfferTeleport(void* data);
	static void onClickRequestTeleport(void* data);
	static void onClickPay(void* data);
	static void onClickAddFriend(void* data);
	static void onClickRemove(void* data);
	static void onClickModifyStatus(LLUICtrl* ctrl, void* data);

	static bool handleRemove(const LLSD& notification, const LLSD& response);
	bool modifyRightsConfirmation(const LLSD& notification,
								  const LLSD& response, rights_map_t* rights);

private:
	LLButton*					mIMButton;
	LLButton*					mProfileButton;
	LLButton*					mOfferTPButton;
	LLButton*					mRequestTPButton;
	LLButton*					mPayButton;
	LLButton*					mRemoveButton;
	LLScrollListCtrl*			mFriendsList;
	LLScrollListItem*			mListComment;

	LLFriendObserver*			mObserver;

	S32							mNumRightsChanged;
	LLUUID						mAddFriendID;
	std::string					mAddFriendName;
};

#endif // LL_LLFLOATERFRIENDS_H
