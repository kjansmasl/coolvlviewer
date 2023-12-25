/**
 * @file llparcel.h
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 *
 * Copyright (c) 2002-2009, Linden Research, Inc.
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

#ifndef LL_LLPARCEL_H
#define LL_LLPARCEL_H

#include <iostream>
#include <time.h>

#include "hbfastmap.h"
#include "llparcelflags.h"
#include "llpermissions.h"
#include "llpreprocessor.h"
#include "lluuid.h"
#include "llvector3.h"

// Grid out of which parcels taken is stepped every 4 meters.
constexpr F32 PARCEL_GRID_STEP_METERS = 4.f;

// Area of one "square" of parcel
constexpr S32 PARCEL_UNIT_AREA = 16;

// Height _above_ground_ that parcel boundary ends
constexpr F32 PARCEL_HEIGHT = 50.f;

// Height above ground which parcel boundries exist for explicitly banned
// avatars
constexpr F32 BAN_HEIGHT = 5000.f;

// Maximum number of entries in an access list
constexpr S32 PARCEL_MAX_ACCESS_LIST = 300;
// Maximum number of entires in an update packet for access/ban lists.
constexpr F32 PARCEL_MAX_ENTRIES_PER_PACKET = 48.f;

// Maximum number of experiences
constexpr S32 PARCEL_MAX_EXPERIENCE_LIST = 24;

// Weekly charge for listing a parcel in the directory
constexpr S32 PARCEL_DIRECTORY_FEE = 30;

constexpr S32 PARCEL_PASS_PRICE_DEFAULT = 10;
constexpr F32 PARCEL_PASS_HOURS_DEFAULT = 1.f;

// Number of "chunks" in which parcel overlay data is sent.
// Chunk 0 = southern rows, entire width
constexpr S32 PARCEL_OVERLAY_CHUNKS = 4;

// Bottom three bits are a color index for the land overlay
constexpr U8 PARCEL_COLOR_MASK	= 0x07;
constexpr U8 PARCEL_PUBLIC		= 0x00;
constexpr U8 PARCEL_OWNED		= 0x01;
constexpr U8 PARCEL_GROUP		= 0x02;
constexpr U8 PARCEL_SELF		= 0x03;
constexpr U8 PARCEL_FOR_SALE	= 0x04;
constexpr U8 PARCEL_AUCTION		= 0x05;
// Avatars not visible outside of parcel. Used for 'see avs' feature, but must
// be off for compatibility.
constexpr U8 PARCEL_HIDDENAVS   = 0x10;
constexpr U8 PARCEL_SOUND_LOCAL = 0x20;
constexpr U8 PARCEL_WEST_LINE	= 0x40;	// Flag, property line on west edge
constexpr U8 PARCEL_SOUTH_LINE	= 0x80;	// Flag, property line on south edge

// Transmission results for parcel properties
constexpr S32 PARCEL_RESULT_NO_DATA = -1;
constexpr S32 PARCEL_RESULT_SUCCESS = 0;	// Got exactly one parcel
constexpr S32 PARCEL_RESULT_MULTIPLE = 1;	// Got multiple parcels

constexpr S32 UPDATE_AGENT_PARCEL_SEQ_ID			= -1000;
constexpr S32 SELECTED_PARCEL_SEQ_ID				= -10000;
constexpr S32 COLLISION_NOT_IN_GROUP_PARCEL_SEQ_ID	= -20000;
constexpr S32 COLLISION_BANNED_PARCEL_SEQ_ID		= -30000;
constexpr S32 COLLISION_NOT_ON_LIST_PARCEL_SEQ_ID	= -40000;
constexpr S32 HOVERED_PARCEL_SEQ_ID					= -50000;

constexpr U32 RT_NONE	= 0x1 << 0;
constexpr U32 RT_OWNER	= 0x1 << 1;
constexpr U32 RT_GROUP	= 0x1 << 2;
constexpr U32 RT_OTHER	= 0x1 << 3;
constexpr U32 RT_LIST	= 0x1 << 4;
constexpr U32 RT_SELL	= 0x1 << 5;

constexpr S32 INVALID_PARCEL_ID = -1;

constexpr S32 INVALID_PARCEL_ENVIRONMENT_VERSION = -2;
// When region settings are used, parcel environment version is -1
constexpr S32 UNSET_PARCEL_ENVIRONMENT_VERSION = -1;

// Timeouts for parcels.
// Default is 21 days * 24h/d * 60m/h * 60s/m *1000000 usec/s = 1814400000000
constexpr U64 DEFAULT_USEC_CONVERSION_TIMEOUT = 1814400000000UL;

// Group is 60 days * 24h/d * 60m/h * 60s/m *1000000 usec/s = 5184000000000
constexpr U64 GROUP_USEC_CONVERSION_TIMEOUT = 5184000000000UL;

// Default sale timeout is 2 days -> 172800000000
constexpr U64 DEFAULT_USEC_SALE_TIMEOUT = 172800000000UL;

// More grace period extensions.
constexpr U64 SEVEN_DAYS_IN_USEC = 604800000000UL;

// If more than 100000s before sale revert, and no extra extension has been
// given, go ahead and extend it more (about 1.2 days).
constexpr S32 EXTEND_GRACE_IF_MORE_THAN_SEC = 100000;

class LLMessageSystem;
class LLSD;

class LLAccessEntry
{
public:
	LL_INLINE LLAccessEntry()
	:	mTime(0),
		mFlags(0)
	{
	}

	LL_INLINE LLAccessEntry(const LLUUID& id, S32 time, U32 flags)
	:	mID(id),
		mTime(time),
		mFlags(flags)
	{
	}

public:
	LLUUID	mID;	// Avatar ID
	S32		mTime;	// Time (unix seconds) when entry expires
	U32		mFlags;	// Not used - currently should always be zero
};

typedef fast_hmap<LLUUID, LLAccessEntry> access_map_t;

class LLParcel final
{
protected:
	LOG_CLASS(LLParcel);

public:
	enum EOwnershipStatus
	{
		OS_LEASED = 0,
		OS_LEASE_PENDING = 1,
		OS_ABANDONED = 2,
		OS_COUNT = 3,
		OS_NONE = -1
	};

	enum ECategory
	{
		C_NONE = 0,
		C_LINDEN,
		C_ADULT,
		C_ARTS,			// "Arts & culture"
		C_BUSINESS,		// Was "store"
		C_EDUCATIONAL,
		C_GAMING,		// Was "game"
		C_HANGOUT,		// Was "gathering place"
		C_NEWCOMER,
		C_PARK,			// "Parks & nature"
		C_RESIDENTIAL,	// Was "homestead"
		C_SHOPPING,
		C_STAGE,
		C_OTHER,
		C_RENTAL,
		C_COUNT,
		C_ANY = -1		// Only useful in queries
	};

	enum EAction
	{
		A_CREATE = 0,
		A_RELEASE = 1,
		A_ABSORB = 2,
		A_ABSORBED = 3,
		A_DIVIDE = 4,
		A_DIVISION = 5,
		A_ACQUIRE = 6,
		A_RELINQUISH = 7,
		A_CONFIRM = 8,
		A_COUNT = 9,
		A_UNKNOWN = -1
	};

	enum ELandingType
	{
		L_NONE = 0,
		L_LANDING_POINT = 1,
		L_DIRECT = 2
	};

	LLParcel();
	LLParcel(const LLUUID& owner_id, bool modify, bool terraform, bool damage,
			 time_t claim_date, S32 claim_price, S32 rent_price, S32 area,
			 S32 sim_object_limit, F32 parcel_object_bonus,
			 bool is_group_owned = false);

	void init(const LLUUID& owner_id, bool modify, bool terraform, bool damage,
			  time_t claim_date, S32 claim_price, S32 rent_price, S32 area,
			  S32 sim_object_limit, F32 parcel_object_bonus,
			  bool is_group_owned = false);

	// MANIPULATORS
	void setName(const std::string& name);
	void setDesc(const std::string& desc);
	void setMusicURL(const std::string& url);
	void setMediaURL(const std::string& url);
	void setMediaType(const std::string& type);
	void setMediaDesc(const std::string& desc);
	LL_INLINE void setMediaID(const LLUUID& id)				{ mMediaID = id; }
	LL_INLINE void setMediaAutoScale(bool b)				{ mMediaAutoScale = b; }
	LL_INLINE void setMediaLoop(bool b)						{ mMediaLoop = b; }
	LL_INLINE void setMediaWidth(S32 width)					{ mMediaWidth = width; }
	LL_INLINE void setMediaHeight(S32 height)				{ mMediaHeight = height; }
	void setMediaCurrentURL(const std::string& url);
	LL_INLINE void setMediaAllowNavigate(U8 enable)			{ mMediaAllowNavigate = enable; }
	LL_INLINE void setMediaURLTimeout(F32 timeout)			{ mMediaURLTimeout = timeout; }

	LL_INLINE void setLocalID(S32 local_id)					{ mLocalID = local_id; }

	LL_INLINE void setAuthorizedBuyerID(const LLUUID& id)	{ mAuthBuyerID = id; }
	LL_INLINE void setCategory(ECategory category)			{ mCategory = category; }
	LL_INLINE void setSnapshotID(const LLUUID& id)			{ mSnapshotID = id; }
	LL_INLINE void setUserLocation(const LLVector3& pos)	{ mUserLocation = pos; }
	LL_INLINE void setUserLookAt(const LLVector3& rot)		{ mUserLookAt = rot; }
	LL_INLINE void setLandingType(const ELandingType type)	{ mLandingType = type; }
	LL_INLINE void setSeeAVs(bool see_avs)					{ mSeeAVs = see_avs; }
	// Remove this once hidden AV feature is fully available grid-wide (rather
	// keep it for now and until OpenSim grids can cope... HB):
	LL_INLINE void setHaveNewParcelLimitData(bool b)		{ mHaveNewParcelLimitData = b; }

	LL_INLINE void setAuctionID(U32 auction_id)				{ mAuctionID = auction_id;}

	void setParcelFlag(U32 flag, bool b);

	LL_INLINE void setContributeWithDeed(bool b)			{ setParcelFlag(PF_CONTRIBUTE_WITH_DEED, b); }
	LL_INLINE void setForSale(bool b)						{ setParcelFlag(PF_FOR_SALE, b); }
	LL_INLINE void setSoundOnly(bool b)						{ setParcelFlag(PF_SOUND_LOCAL, b); }
	LL_INLINE void setAllowGroupAVSounds(bool b)			{ mAllowGroupAVSounds = b; }
	LL_INLINE void setAllowAnyAVSounds(bool b)				{ mAllowAnyAVSounds = b; }
	LL_INLINE void setObscureMOAP(bool b)					{ mObscureMOAP = b; }

	LL_INLINE void setSalePrice(S32 price)					{ mSalePrice = price; }
	LL_INLINE void setGroupID(const LLUUID& id)				{ mGroupID = id; }
	LL_INLINE void setPassPrice(S32 price)					{ mPassPrice = price; }
	LL_INLINE void setPassHours(F32 hours)					{ mPassHours = hours; }

	void packMessage(LLMessageSystem* msg);
	void packMessage(LLSD& msg);
	void unpackMessage(LLMessageSystem* msg);

	void unpackAccessEntries(LLMessageSystem* msg, access_map_t* list);

	// Experience tools support
	void unpackExperienceEntries(LLMessageSystem* msg, U32 type);
	void setExperienceKeyType(const LLUUID& experience_key, U32 type);
	U32 countExperienceKeyType(U32 type) const;
	U32 getExperienceKeyType(const LLUUID& experience_key) const;
	access_map_t getExperienceKeysByType(U32 type)const;
	void clearExperienceKeysByType(U32 type);

	LL_INLINE void setAABBMin(const LLVector3& min)			{ mAABBMin = min; }
	LL_INLINE void setAABBMax(const LLVector3& max)			{ mAABBMax = max; }

	void dump();

	// Add to list, suppressing duplicates. Return true if succesful.
	bool addToAccessList(const LLUUID& agent_id, S32 time);
	bool addToBanList(const LLUUID& agent_id, S32 time);
	// Remove from list. Return true if succesful.
	bool removeFromAccessList(const LLUUID& agent_id);
	bool removeFromBanList(const LLUUID& agent_id);

	// ACCESSORS
	LL_INLINE const LLUUID&	getID() const					{ return mID; }
	LL_INLINE const std::string& getName() const			{ return mName; }
	LL_INLINE const std::string& getDesc() const			{ return mDesc; }
	LL_INLINE const std::string& getMusicURL() const		{ return mMusicURL; }
	LL_INLINE const std::string& getMediaURL() const		{ return mMediaURL; }
	LL_INLINE const std::string& getMediaDesc() const		{ return mMediaDesc; }
	LL_INLINE const std::string& getMediaType() const		{ return mMediaType; }
	LL_INLINE const LLUUID&	getMediaID() const				{ return mMediaID; }
	LL_INLINE S32 getMediaWidth() const						{ return mMediaWidth; }
	LL_INLINE S32 getMediaHeight() const					{ return mMediaHeight; }
	LL_INLINE bool getMediaAutoScale() const				{ return mMediaAutoScale; }
	LL_INLINE bool getMediaLoop() const						{ return mMediaLoop; }
	LL_INLINE const std::string& getMediaCurrentURL() const	{ return mMediaCurrentURL; }
	LL_INLINE U8 getMediaAllowNavigate() const				{ return mMediaAllowNavigate; }
	LL_INLINE F32 getMediaURLTimeout() const				{ return mMediaURLTimeout; }
	LL_INLINE U8 getMediaPreventCameraZoom() const			{ return mMediaPreventCameraZoom; }

	LL_INLINE S32 getLocalID() const						{ return mLocalID; }
	LL_INLINE const LLUUID&	getOwnerID() const				{ return mOwnerID; }
	LL_INLINE const LLUUID&	getGroupID() const				{ return mGroupID; }
	LL_INLINE S32 getPassPrice() const						{ return mPassPrice; }
	LL_INLINE F32 getPassHours() const						{ return mPassHours; }
	LL_INLINE bool getIsGroupOwned() const					{ return mGroupOwned; }

	LL_INLINE U32 getAuctionID() const						{ return mAuctionID; }

	LL_INLINE bool isPublic() const							{ return mOwnerID.isNull(); }

	// Region-local user-specified position
	LL_INLINE const LLVector3& getUserLocation() const		{ return mUserLocation; }
	LL_INLINE const LLVector3& getUserLookAt() const		{ return mUserLookAt; }
	LL_INLINE ELandingType getLandingType() const			{ return mLandingType; }
	LL_INLINE bool getSeeAVs() const						{ return mSeeAVs; }
	LL_INLINE bool getHaveNewParcelLimitData() const		{ return mHaveNewParcelLimitData; }

	// User-specified snapshot
	LL_INLINE const LLUUID& getSnapshotID() const			{ return mSnapshotID; }

	// The authorized buyer id is the person who is the only agent/group that
	// has authority to purchase. (i.e. UI specified a particular agent could
	// buy the plot).
	LL_INLINE const LLUUID& getAuthorizedBuyerID() const	{ return mAuthBuyerID; }

	// helper function
	LL_INLINE bool isBuyerAuthorized(const LLUUID& buyer_id) const
	{
		return mAuthBuyerID.isNull() || mAuthBuyerID == buyer_id;
	}

	// Methods to deal with ownership status.
	LL_INLINE EOwnershipStatus getOwnershipStatus() const	{ return mStatus; }
	static const std::string& getOwnershipStatusString(EOwnershipStatus status);
	LL_INLINE void setOwnershipStatus(EOwnershipStatus s)	{ mStatus = s; }

	// Dealing with parcel category information
	LL_INLINE ECategory getCategory() const					{ return mCategory; }
	static const std::string& getCategoryString(ECategory category);
	static const std::string& getCategoryUIString(ECategory category);
	static ECategory getCategoryFromString(const std::string& string);
	static ECategory getCategoryFromUIString(const std::string& string);

	// functions for parcel action (used for logging)
	static const std::string& getActionString(EAction action);

	LL_INLINE U32 getParcelFlags() const					{ return mParcelFlags; }

	LL_INLINE bool getParcelFlag(U32 flag) const			{ return (mParcelFlags & flag) != 0; }

	// Objects can be added or modified by anyone (only parcel owner if
	// disabled)
	LL_INLINE bool getAllowModify() const					{ return (mParcelFlags & PF_CREATE_OBJECTS) != 0; }

	// Objects can be added or modified by group members
	LL_INLINE bool getAllowGroupModify() const				{ return (mParcelFlags & PF_CREATE_GROUP_OBJECTS) != 0; }

	// The parcel can be deeded to the group
	LL_INLINE bool getAllowDeedToGroup() const				{ return (mParcelFlags & PF_ALLOW_DEED_TO_GROUP) != 0; }

	// Does the owner want to make a contribution along with the deed.
	LL_INLINE bool getContributeWithDeed() const			{ return (mParcelFlags & PF_CONTRIBUTE_WITH_DEED) != 0; }

	// Heightfield can be modified
	LL_INLINE bool getAllowTerraform() const				{ return (mParcelFlags & PF_ALLOW_TERRAFORM) != 0; }

	// Avatars can be hurt here
	LL_INLINE bool getAllowDamage() const					{ return (mParcelFlags & PF_ALLOW_DAMAGE) != 0; }

	LL_INLINE bool getAllowFly() const						{ return (mParcelFlags & PF_ALLOW_FLY) != 0; }

	// For now kept, just in case of OpenSim compatibility issues. We should
	// eventually remove this flag completely.
	LL_INLINE bool getAllowLandmark() const					{ return (mParcelFlags & PF_ALLOW_LANDMARK) != 0; }

	LL_INLINE bool getAllowGroupScripts() const				{ return (mParcelFlags & PF_ALLOW_GROUP_SCRIPTS) != 0; }

	LL_INLINE bool getAllowOtherScripts() const				{ return (mParcelFlags & PF_ALLOW_OTHER_SCRIPTS) != 0; }

	LL_INLINE bool getAllowAllObjectEntry() const			{ return (mParcelFlags & PF_ALLOW_ALL_OBJECT_ENTRY) != 0; }

	LL_INLINE bool getAllowGroupObjectEntry() const			{ return (mParcelFlags & PF_ALLOW_GROUP_OBJECT_ENTRY) != 0; }

	LL_INLINE bool getForSale() const						{ return (mParcelFlags & PF_FOR_SALE) != 0; }
	LL_INLINE bool getSoundLocal() const					{ return (mParcelFlags & PF_SOUND_LOCAL) != 0; }
	LL_INLINE bool getParcelFlagAllowVoice() const			{ return (mParcelFlags & PF_ALLOW_VOICE_CHAT) != 0; }

	LL_INLINE bool getParcelFlagUseEstateVoiceChannel() const
	{
		return (mParcelFlags & PF_USE_ESTATE_VOICE_CHAN) != 0;
	}

	LL_INLINE bool getAllowPublish() const					{ return (mParcelFlags & PF_ALLOW_PUBLISH) != 0; }
	LL_INLINE bool getMaturePublish() const					{ return (mParcelFlags & PF_MATURE_PUBLISH) != 0; }
	LL_INLINE bool getRestrictPushObject() const			{ return (mParcelFlags & PF_RESTRICT_PUSHOBJECT) != 0; }
	LL_INLINE bool getRegionPushOverride() const			{ return mRegionPushOverride; }
	LL_INLINE bool getRegionDenyAnonymousOverride() const	{ return mRegionDenyAnonymousOverride; }

	LL_INLINE bool getRegionDenyAgeUnverifiedOverride() const
	{
		return mRegionDenyAgeUnverifiedOverride;
	}

	LL_INLINE bool getRegionAllowAccessOverride() const
	{
		return mRegionAllowAccessOverride;
	}

	LL_INLINE bool getRegionAllowEnvironmentOverride() const
	{
		return mRegionAllowEnvironmentOverride;
	}

	LL_INLINE S32 getParcelEnvironmentVersion() const
	{
		return mCurrentEnvironmentVersion;
	}

	LL_INLINE bool getAllowGroupAVSounds()	const			{ return mAllowGroupAVSounds; }
	LL_INLINE bool getAllowAnyAVSounds() const				{ return mAllowAnyAVSounds; }
	LL_INLINE bool getObscureMOAP() const					{ return mObscureMOAP; }

	LL_INLINE S32 getSalePrice() const						{ return mSalePrice; }
	LL_INLINE time_t getClaimDate() const					{ return mClaimDate; }
	LL_INLINE S32 getClaimPricePerMeter() const				{ return mClaimPricePerMeter; }
	LL_INLINE S32 getRentPricePerMeter() const				{ return mRentPricePerMeter; }

	LL_INLINE S32 getArea() const							{ return mArea; }

	LL_INLINE S32 getClaimPrice() const						{ return mClaimPricePerMeter * mArea; }

	// Can this agent create objects here?
	bool allowModifyBy(const LLUUID& agent_id, const LLUUID& group_id) const;

	// Can this agent change the shape of the land?
	bool allowTerraformBy(const LLUUID& agent_id) const;

	bool operator==(const LLParcel& rhs) const;

	// Calculate rent - area * rent * discount rate
	S32 getTotalRent() const;
	F32 getAdjustedRentPerMeter() const;

	LL_INLINE const LLVector3& getAABBMin() const			{ return mAABBMin; }
	LL_INLINE const LLVector3& getAABBMax() const			{ return mAABBMax; }
	LLVector3 getCenterpoint() const;

	// Sim-wide
	LL_INLINE S32 getSimWideMaxPrimCapacity() const			{ return mSimWideMaxPrimCapacity; }
	LL_INLINE S32 getSimWidePrimCount() const				{ return mSimWidePrimCount; }

	// This parcel only (not sim-wide)

	// Does not include prim bonus
	LL_INLINE S32 getMaxPrimCapacity() const				{ return mMaxPrimCapacity; }

	LL_INLINE S32 getPrimCount() const
	{
		return mOwnerPrimCount + mGroupPrimCount + mOtherPrimCount +
			   mSelectedPrimCount;
	}

	LL_INLINE S32 getOwnerPrimCount() const					{ return mOwnerPrimCount; }
	LL_INLINE S32 getGroupPrimCount() const					{ return mGroupPrimCount; }
	LL_INLINE S32 getOtherPrimCount() const					{ return mOtherPrimCount; }
	LL_INLINE S32 getSelectedPrimCount() const				{ return mSelectedPrimCount; }
	LL_INLINE S32 getTempPrimCount() const					{ return mTempPrimCount; }
	LL_INLINE F32 getParcelPrimBonus() const				{ return mParcelPrimBonus; }

	LL_INLINE S32 getCleanOtherTime() const					{ return mCleanOtherTime; }

	// Does not include prim bonus
	LL_INLINE void setMaxPrimCapacity(S32 max)				{ mMaxPrimCapacity = max; }

	// Sim-wide
	LL_INLINE void setSimWideMaxPrimCapacity(S32 c)			{ mSimWideMaxPrimCapacity = c; }
	LL_INLINE void setSimWidePrimCount(S32 c)				{ mSimWidePrimCount = c; }

	// This parcel only (not sim-wide)
	LL_INLINE void setOwnerPrimCount(S32 c)					{ mOwnerPrimCount = c; }
	LL_INLINE void setGroupPrimCount(S32 c)					{ mGroupPrimCount = c; }
	LL_INLINE void setOtherPrimCount(S32 c)					{ mOtherPrimCount = c; }
	LL_INLINE void setSelectedPrimCount(S32 c)				{ mSelectedPrimCount = c; }
	LL_INLINE void setTempPrimCount(S32 c)					{ mTempPrimCount = c; }
	LL_INLINE void setParcelPrimBonus(F32 b)				{ mParcelPrimBonus = b; }

	LL_INLINE void setCleanOtherTime(S32 time)				{ mCleanOtherTime = time; }
	LL_INLINE void setRegionPushOverride(bool b)			{ mRegionPushOverride = b; }
	LL_INLINE void setRegionDenyAnonymousOverride(bool b)	{ mRegionDenyAnonymousOverride = b; }

	LL_INLINE void setRegionDenyAgeUnverifiedOverride(bool b)
	{
		mRegionDenyAgeUnverifiedOverride = b;
	}

	LL_INLINE void setRegionAllowAccessOverride(bool b)
	{
		mRegionAllowAccessOverride = b;
	}

	LL_INLINE void setRegionAllowEnvironmentOverride(bool b)
	{
		mRegionAllowEnvironmentOverride = b;
	}

	LL_INLINE void setParcelEnvironmentVersion(S32 version)
	{
		mCurrentEnvironmentVersion = version;
	}

	// Accessors for parcel sellWithObjects
	LL_INLINE void setPreviousOwnerID(LLUUID id)			{ mPreviousOwnerID = id; }
	LL_INLINE void setPreviouslyGroupOwned(bool b)			{ mPreviouslyGroupOwned = b; }
	LL_INLINE void setSellWithObjects(bool b)				{ setParcelFlag(PF_SELL_PARCEL_OBJECTS, b); }

	LL_INLINE LLUUID getPreviousOwnerID() const				{ return mPreviousOwnerID; }
	LL_INLINE bool getPreviouslyGroupOwned() const			{ return mPreviouslyGroupOwned; }
	LL_INLINE bool getSellWithObjects() const				{ return (mParcelFlags & PF_SELL_PARCEL_OBJECTS) != 0; }

public:
	S32					mLocalID;
	LLUUID			    mBanListTransactionID;
	LLUUID			    mAccessListTransactionID;
	access_map_t		mAccessList;
	access_map_t		mBanList;
	access_map_t		mTempBanList;
	access_map_t		mTempAccessList;

protected:
	LLUUID				mID;
	LLUUID				mOwnerID;
	LLUUID				mGroupID;
	LLUUID				mPreviousOwnerID;
	LLUUID				mAuthBuyerID;
	LLUUID				mSnapshotID;
	LLVector3			mUserLocation;
	LLVector3			mUserLookAt;
	ELandingType		mLandingType;

	std::string			mName;
	std::string			mDesc;
	std::string			mMusicURL;
	std::string			mMediaURL;
	std::string			mMediaDesc;
	std::string 		mMediaType;

	typedef safe_hmap<LLUUID, U32> xp_type_map_t;
	xp_type_map_t		mExperienceKeys;

	EOwnershipStatus	mStatus;
	ECategory			mCategory;

	S32					mGraceExtension;

	// This value is non-zero if there is an auction associated with
	// the parcel.
	U32					mAuctionID;

	time_t				mClaimDate;				// UTC Unix-format time
	S32					mClaimPricePerMeter;	// meter squared
	S32					mRentPricePerMeter;		// meter squared
	S32					mArea;					// meter squared
	F32					mDiscountRate;			// 0.0-1.0
	U32					mParcelFlags;
	S32					mSalePrice;				// linden dollars
	S32					mMediaWidth;
	S32					mMediaHeight;
	U8					mMediaAllowNavigate;
	U8					mMediaPreventCameraZoom;
	LLUUID				mMediaID;
	std::string			mMediaCurrentURL;
	F32					mMediaURLTimeout;
	S32					mPassPrice;
	F32					mPassHours;
	LLVector3			mAABBMin;
	LLVector3			mAABBMax;
	// Prims allowed on parcel, does not include prim bonus:
	S32					mMaxPrimCapacity;
	S32					mSimWidePrimCount;
	S32					mSimWideMaxPrimCapacity;
	S32					mOwnerPrimCount;
	S32					mGroupPrimCount;
	S32					mOtherPrimCount;
	S32					mSelectedPrimCount;
	S32					mTempPrimCount;
	F32					mParcelPrimBonus;
	S32					mCleanOtherTime;
	S32					mCurrentEnvironmentVersion;

	bool				mGroupOwned; // true if mOwnerID is a group_id
	bool				mPreviouslyGroupOwned;
	// Avatars on this parcel are visible from outside it:
	bool				mSeeAVs;
	// Remove once hidden AV feature is enabled in all OpenSim grids:
	bool				mHaveNewParcelLimitData;

	bool				mMediaAutoScale;
	bool				mMediaLoop;
	bool				mRegionPushOverride;
	bool				mRegionDenyAnonymousOverride;
	bool				mRegionDenyAgeUnverifiedOverride;
	bool				mRegionAllowAccessOverride;
	bool				mRegionAllowEnvironmentOverride;
	bool				mIsDefaultDayCycle;
	bool				mAllowGroupAVSounds;
	bool				mAllowAnyAVSounds;
	bool				mObscureMOAP;
};

const std::string& ownership_status_to_string(LLParcel::EOwnershipStatus status);
LLParcel::EOwnershipStatus ownership_string_to_status(const std::string& s);
LLParcel::ECategory category_string_to_category(const std::string& s);
const std::string& category_to_string(LLParcel::ECategory category);

#endif
