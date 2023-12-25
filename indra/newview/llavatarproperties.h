/**
 * @file llavatarproperties.h
 * @brief Class for requesting and storing avatar properties.
 *
 * $LicenseInfo:firstyear=2004&license=viewergpl$
 *
 * Copyright (c) 2004-2022, Linden Research, Inc.
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

#ifndef LL_LLAVATARPROPERTIES_H
#define LL_LLAVATARPROPERTIES_H

#include <list>
#include <map>
#include <utility>

#include "hbfastmap.h"
#include "hbfastset.h"
#include "lluuid.h"
#include "llvector3d.h"

class LLMessageSystem;
struct LLGroupData;

struct LLAvatarInfo
{
	LLUUID		mAvatarId;
	LLUUID		mImageId;
	LLUUID		mFLImageId;
	LLUUID		mPartnerId;
	std::string	mBirthDate;
	std::string	mAbout;
	std::string	mFLAbout;
	std::string	mProfileUrl;
	std::string	mCaptionText;
	U32			mFlags;
	U8			mCaptionIndex;
	bool		mAllowPublish;
	bool		mReceivedViaCap;
};

struct LLAvatarGroups
{
	LLUUID	mAvatarId;
	typedef std::list<LLGroupData> list_t;
	list_t	mGroups;
};

struct LLAvatarInterests
{
	LLUUID		mAvatarId;
	std::string	mLanguages;
	std::string	mWantsText;
	std::string	mSkillsText;
	U32			mWantsMask;
	U32			mSkillsMask;
};

struct LLAvatarPicks
{
	LLUUID	mAvatarId;
	// Picks UUID to name map.
	typedef fast_hmap<LLUUID, std::string> map_t;
	map_t	mMap;
	bool	mReceivedViaCap;
};

struct LLAvatarPickInfo
{
	LLUUID		mAvatarId;
	LLUUID		mPickId;
	LLUUID		mSnapshotId;
	LLUUID		mParcelId;
	std::string	mName;
	std::string	mDesc;
	std::string	mUserName;
	std::string	mSimName;
	std::string	mParcelName;
	LLVector3d	mPosGlobal;
	S32			mSortOrder;
	bool		mTopPick;
	bool		mEnabled;
};

struct LLAvatarClassifieds
{
	LLUUID	mAvatarId;
	// Classifed UUID to name map.
	typedef fast_hmap<LLUUID, std::string> map_t;
	map_t	mMap;
};

struct LLAvatarClassifiedInfo
{
	LLUUID		mAvatarId;
	LLUUID		mClassifiedId;
	LLUUID		mSnapshotId;
	LLUUID		mParcelId;
	std::string	mName;
	std::string	mDesc;
	std::string	mSimName;
	std::string	mParcelName;
	LLVector3d	mPosGlobal;
	U32			mParentEstate;
	S32			mListingPrice;
	U32			mCreationDate;
	U32			mExpirationDate;
	U32			mCategory;
	U8			mFlags;
};

struct LLAvatarNotes
{
	LLUUID		mAvatarId;
	std::string	mNotes;
	bool		mReceivedViaCap;
};

enum EAvatarPropertiesReplyFlags
{
	// Whether profile is externally visible or not
	AVATAR_ALLOW_PUBLISH	= 0x1 << 0,
	// Whether profile is "mature" (not used)
	AVATAR_MATURE_PUBLISH	= 0x1 << 1,
	// Whether avatar has provided payment info
	AVATAR_IDENTIFIED		= 0x1 << 2,
	// Whether avatar has actively used payment info
	AVATAR_TRANSACTED		= 0x1 << 3,
	// The online status of this avatar, if known.
	AVATAR_ONLINE			= 0x1 << 4,
	// Whether avatar has been age-verified: not currently reported by
	// servers for privacy considerations.
	AVATAR_AGEVERIFIED		= 0x1 << 5,
};

enum EAvatarPropertiesUpdateType
{
	APT_ALL = -1,			// Use to observe all types.
	APT_NONE = 0,			// Use to disable the observer without removing it.
	APT_AVATAR_INFO,
	APT_GROUPS,				// } Types requested via send_generic_message():
	APT_PICKS,				// } they must be kept in this order or the
	APT_CLASSIFIEDS,		// } sendGenericRequest() method would need
	APT_NOTES,				// } modifications. HB
	APT_INTERESTS,
	APT_PICK_INFO,
	APT_CLASSIFIED_INFO,
};

// Observer class to register to properties updates from server for a given
// avatar (or all avatars, when passed a null UUID) and a given type (or all
// types when omitted).
class LLAvatarPropertiesObserver
{
public:
	LL_INLINE LLAvatarPropertiesObserver(const LLUUID& id, S32 type = APT_ALL)
	:	mObservedAvatarId(id),
		mObservedUpdate(type)
	{
	}

	virtual ~LLAvatarPropertiesObserver() = default;

	virtual void processProperties(S32 type, void* data) = 0;

	LL_INLINE const LLUUID& getAvatarId() const
	{
		return mObservedAvatarId;
	}

	LL_INLINE S32 getUpdateType() const
	{
		return mObservedUpdate;
	}

protected:
	LL_INLINE void setObservedAvatarId(const LLUUID& av_id)
	{
		mObservedAvatarId = av_id;
	}

	LL_INLINE void setObservedUpdateType(S32 type)
	{
		mObservedUpdate = type;
	}

private:
	LLUUID	mObservedAvatarId;
	S32		mObservedUpdate;
};

// Purely static class (a singleton in LL's code, because they like making it
// pointlessly slow and complex). HB
class LLAvatarProperties
{
protected:
	LOG_CLASS(LLAvatarProperties);

public:
	LLAvatarProperties() = delete;
	~LLAvatarProperties() = delete;

	LL_INLINE static void addObserver(LLAvatarPropertiesObserver* observerp)
	{
		if (observerp)
		{
			sObservers.insert(observerp);
		}
	}

	LL_INLINE static void removeObserver(LLAvatarPropertiesObserver* observerp)
	{
		sObservers.erase(observerp);
	}

	// The following two methods request various types of avatar data.
	// Duplicate requests are suppressed while waiting for a response from the
	// server.

	// The only allowed types for this method are APT_AVATAR_INFO, APT_GROUPS,
	// APT_PICKS, APT_CLASSIFIEDS and APT_NOTES: any other type would trigger a
	// llerrs !
	// For all but APT_CLASSIFIEDS requests, this method may use the new
	// "AgentProfile" capabilities, if available and the corresponding debug
	// setting ("UseAgentProfileCap") is set to TRUE. HB
	static void sendGenericRequest(const LLUUID& avatar_id, S32 type);
	// Method to request APT_AVATAR_INFO info via UDP messaging; which
	// imposes a limit on the SL/FL About text fields size, but also requests
	// the avatar interests data (which the capability does not provide). HB
	static void sendAvatarPropertiesRequest(const LLUUID& avatar_id);

	// Pick and classified detailed info requests (not tracked for duplicates).

	static void sendPickInfoRequest(const LLUUID& avatar_id,
									const LLUUID& pick_id);
	static void sendClassifiedInfoRequest(const LLUUID& classified_id);

	// These method send updates to our agent's data.

	static void sendAvatarPropertiesUpdate(const LLAvatarInfo& data);
	static void sendInterestsInfoUpdate(const LLAvatarInterests& data);
	static void sendPickInfoUpdate(const LLAvatarPickInfo& data);
	static void sendPickDelete(const LLUUID& avatar_id, const LLUUID& pick_id);
	static void sendClassifiedInfoUpdate(const LLAvatarClassifiedInfo& data);
	static void sendClassifiedDelete(const LLUUID& classified_id);
	static void sendAvatarNotesUpdate(const LLUUID& avatar_id,
									  const std::string& notes);

	// These are callback methods for server replies, wired to the messaging
	// system in llstartup.cpp.

	static void processAvatarPropertiesReply(LLMessageSystem* msg, void**);
	static void processAvatarGroupsReply(LLMessageSystem* msg, void**);
	static void processAvatarInterestsReply(LLMessageSystem* msg, void**);
	static void processAvatarPicksReply(LLMessageSystem* msg, void**);
	static void processPickInfoReply(LLMessageSystem* msg, void**);
	static void processAvatarClassifiedReply(LLMessageSystem* msg, void**);
	static void processClassifiedInfoReply(LLMessageSystem* msg, void**);
	static void processAvatarNotesReply(LLMessageSystem* msg, void**);

private:
	static void requestAvatarPropertiesCoro(LLUUID avatar_id,
											const std::string& cap_url);
	static void sendAvatarPropertiesUpdateCoro(LLSD data,
											   const std::string& cap_url);

	static void notifyObservers(const LLUUID& id, S32 type, void* data);

	static void addPendingRequest(const LLUUID& id, S32 type);
	static void removePendingRequest(const LLUUID& id, S32 type);
	static bool isPendingRequest(const LLUUID& id, S32 type);

private:
	typedef fast_hset<LLAvatarPropertiesObserver*> observers_set_t;
	static observers_set_t	sObservers;

	// Keeps track of pending requests for data by avatar Id and type,
	// storing a timestamp for each request. Maps avatar_id+request_type to
	// an F32 timestamp in seconds.
	typedef std::map<std::pair<LLUUID, U32>, F32> pending_map_t;
	static pending_map_t	sPendingRequests;
};

#endif // LL_LLAVATARPROPERTIES_H
