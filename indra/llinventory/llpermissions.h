/**
 * @file llpermissions.h
 * @brief Permissions structures for objects.
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

#ifndef LL_LLPERMISSIONS_H
#define LL_LLPERMISSIONS_H

#include "llinventorytype.h"
#include "llpermissionsflags.h"
#include "llpreprocessor.h"
#include "llsd.h"
#include "lluuid.h"
#include "llxmlnode.h"

class LLMessageSystem;
class LLPermissions;

extern void mask_to_string(U32 mask, char* str, bool export_support = false);
extern std::string mask_to_string(U32 mask, bool export_support = false);
bool can_set_export(const U32& base, const U32& own, const U32& next);
bool perms_allow_export(const LLPermissions& perms);

enum ExportPolicy
{
  ep_creator_only,	// Used for SecondLife: only allow export when being 
					// creator.
  ep_full_perm,		// Used on OpenSIM grids not supporting the PERM_EXPORT
					// bit: allow exporting of full perm objects.
  ep_export_bit		// Used on OpenSIM grids that support the PERM_EXPORT bit.
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLPermissions
//
// Class which encapsulates object and inventory permissions/ownership/etc.
//
// Permissions where originally a static state creator/owner and set
// of cap bits. Since then, it has grown to include group information,
// last owner, masks for different people. The implementation has been
// chosen such that a uuid is stored for each current/past owner, and
// a bitmask is stored for the base permissions, owner permissions,
// group permissions, and everyone else permissions.
//
// The base permissions represent the most permissive state that the
// permissions can possibly be in. Thus, if the base permissions do
// not allow copying, no one can ever copy the object. The permissions
// also maintain a tree-like hierarchy of permissions, thus, if we
// (for sake of discussions) denote more permissive as '>', then this
// is invariant:
//
// base mask >= owner mask >= group mask
//                         >= everyone mask
//                         >= next owner mask
// NOTE: the group mask does not effect everyone or next, everyone
// does not effect group or next, etc.
//
// It is considered a fair use right to move or delete any object you
// own.  Another fair use right is the ability to give away anything
// which you cannot copy. One way to look at that is that if you have
// a unique item, you can always give that one copy you have to
// someone else.
//
// Most of the bitmask is easy to understand, PERM_COPY means you can
// copy !PERM_TRANSFER means you cannot transfer, etc. Given that we
// now track the concept of 'next owner' inside of the permissions
// object, we can describe some new meta-meaning to the PERM_MODIFY
// flag. PERM_MODIFY is usually meant to note if you can change an
// item, but since we record next owner permissions, we can interpret
// a no-modify object as 'you cannot modify this object and you cannot
// make derivative works.' When evaluating functionality, and
// comparisons against permissions, keep this concept in mind for
// logical consistency.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLPermissions
{
protected:
	LOG_CLASS(LLPermissions);

public:
	static const LLPermissions DEFAULT;

	LLPermissions();						// defaults to created by system
#if 0
	~LLPermissions();
#endif

	// base initialization code
	void init(const LLUUID& creator, const LLUUID& owner,
			  const LLUUID& last_owner, const LLUUID& group);
	void initMasks(PermissionMask base, PermissionMask owner,
				   PermissionMask everyone, PermissionMask group,
				   PermissionMask next);
	// adjust permissions based on inventory type.
	void initMasks(LLInventoryType::EType type);

	//
	// ACCESSORS
	//

	// Returns the agent_id of the agent that created the item
	LL_INLINE const LLUUID& getCreator() const			{ return mCreator; }

	// Returns the agent_id of the owner. returns LLUUID::null if group
	// owned or public (a really big group).
	LL_INLINE const LLUUID& getOwner() const			{ return mOwner; }

	// Returns the group_id of the group associated with the
	// object.
	LL_INLINE const LLUUID& getGroup() const			{ return mGroup; }

	// Returns the agent_id of the last agent owner. Only returns LLUUID::null
	// if there has never been a previous owner (note: this is apparently not
	// true, say for textures in inventory, it may return LLUUID::null even if
	// there was a previous owner).
	LL_INLINE const LLUUID& getLastOwner() const		{ return mLastOwner; }

	LL_INLINE U32 getMaskBase() const					{ return mMaskBase; }
	LL_INLINE U32 getMaskOwner() const					{ return mMaskOwner; }
	LL_INLINE U32 getMaskGroup() const					{ return mMaskGroup; }
	LL_INLINE U32 getMaskEveryone() const				{ return mMaskEveryone; }
	LL_INLINE U32 getMaskNextOwner() const				{ return mMaskNextOwner; }

	LL_INLINE bool unrestricted() const
	{
		return (mMaskBase & PERM_ITEM_UNRESTRICTED) == PERM_ITEM_UNRESTRICTED;
	}

	// Returns true if the object has any owner
	LL_INLINE bool isOwned() const						{ return mOwner.notNull() || mIsGroupOwned; }

	// Returns true if group_id is owner.
	LL_INLINE bool isGroupOwned() const					{ return mIsGroupOwned; }

	// Returns true if the object is owned at all, and false otherwise. If it
	// is owned at all, owner_id is filled with either the owner Id or the
	// group Id, and the is_group_owned parameter is appropriately filled. The
	// values of owner_id and is_group_owned are not changed if the object is
	// not owned.
	bool getOwnership(LLUUID& owner_id, bool& is_group_owned) const;

	// Gets the 'safe' owner.  This should never return LLUUID::null.
	// If no group owned, return the agent owner id normally.
	// If group owned, return the group id.
	// If not owned, return a random uuid which should have no power.
	LLUUID getSafeOwner() const;

	// Returns a cheap CRC.
	// When 'skip_last_owner' is true, do not account for the last owner UUID
	// (which currently gets lost/reset by the server during inventory items
	// copy actions). HB
	U32 getCRC32(bool skip_last_owner = false) const;

	//
	// MANIPULATORS
	//

	// Fixes hierarchy of permissions, applies appropriate permissions at each
	// level to ensure that base permissions are respected, and also ensures
	// that if base cannot transfer, then group and other cannot copy.
	void fix();

	// All of these methods just do exactly what they say. There is no
	// permissions checking to see if the operation is allowed, and do not fix
	// the permissions hierarchy. So please only use these methods when you
	// know what you are doing and coding on behalf of the system, i.e. acting
	// as god.
	void set(const LLPermissions& permissions);
	LL_INLINE void setMaskBase(U32 mask)				{ mMaskBase = mask; }
	LL_INLINE void setMaskOwner(U32 mask)				{ mMaskOwner = mask; }
	LL_INLINE void setMaskEveryone(U32 mask)			{ mMaskEveryone = mask;}
	LL_INLINE void setMaskGroup(U32 mask)				{ mMaskGroup = mask;}
	LL_INLINE void setMaskNext(U32 mask)				{ mMaskNextOwner = mask; }

	// Allows accumulation of permissions. Results in the tightest permissions
	// possible. In the case of clashing UUIDs, it sets the ID to LLUUID::null.
	void accumulate(const LLPermissions& perm);

	//
	// CHECKED MANIPULATORS
	//

	// These methods return true on success. They return false if the given
	// agent is not allowed to make the change. You can pass LLUUID::null as
	// the agent id if the change is being made by the simulator itself, not on
	// behalf of any agent - this will always succeed. Passing in group id of
	// LLUUID:null means no group, and does not offer special permission to do
	// anything.

	// Saves last owner, sets current owner, and sets the group. When is_atomic
	// is true, it means that this permission represents an atomic permission
	// and not a collection of permissions. Currently, the only way to have a
	// collection is when an object has inventory and is then itself rolled up
	// into an inventory item.
	bool setOwnerAndGroup(const LLUUID& agent, const LLUUID& owner,
						  const LLUUID& group, bool is_atomic);

	// Last owner does not have much in the way of permissions so it is not too
	// dangerous to do this.
	void setLastOwner(const LLUUID& last_owner);

	// Saves last owner, sets owner to uuid null, sets group owned. group_id
	// must be the group of the object (that's who it is being deeded to) and
	// the object must be group modify. Technically, the agent id and group id
	// are not necessary, but I wanted this function to look like the other
	// checked manipulators (since that is how it is used.) If the agent is the
	// system or (group == mGroup and group modify and owner transfer) then
	// this function will deed the permissions, set the next owner mask, and
	// Return true. Otherwise, no change is effected, and the function returns
	// false.
	bool deedToGroup(const LLUUID& agent, const LLUUID& group);

	// Attempts to set or clear the given bitmask. Returns true if you are
	// allowed to modify the permissions.  If you attempt to turn on bits not
	// allowed by the base bits, the function will return true, but those bits
	// will not be set.
	bool setBaseBits(const LLUUID& agent, bool set, PermissionMask bits);
	bool setOwnerBits(const LLUUID& agent, bool set, PermissionMask bits);
	bool setGroupBits(const LLUUID& agent, const LLUUID& group, bool set,
					  PermissionMask bits);
	bool setEveryoneBits(const LLUUID& agent, const LLUUID& group, bool set,
						 PermissionMask bits);
	bool setNextOwnerBits(const LLUUID& agent, const LLUUID& group, bool set,
						  PermissionMask bits);

	// This is currently only used in the Viewer to handle calling cards
	// where the creator is actually used to store the target. Use with care.
	LL_INLINE void setCreator(const LLUUID& creator)	{ mCreator = creator; }

	//
	// METHODS
	//

	// All the allow*() methods return true if the given agent or group can
	// perform the function. Prefer using this set of operations to check
	// permissions on an object. These return true if the given agent or group
	// can perform the function. They also return true if the object is not
	// owned, or the requesting agent is a system agent.
	// See llpermissionsflags.h for bits.
	bool allowOperationBy(PermissionBit op, const LLUUID& agent,
						  const LLUUID& group = LLUUID::null) const;

	LL_INLINE bool allowModifyBy(const LLUUID& agent_id) const
	{
		return allowOperationBy(PERM_MODIFY, agent_id);
	}

	LL_INLINE bool allowCopyBy(const LLUUID& agent_id) const
	{
		return allowOperationBy(PERM_COPY, agent_id);
	}

	LL_INLINE bool allowTransferBy(const LLUUID& agent_id) const
	{
		return allowOperationBy(PERM_TRANSFER, agent_id);
	}

	LL_INLINE bool allowMoveBy(const LLUUID& agent_id) const
	{
		return allowOperationBy(PERM_MOVE, agent_id);
	}

	LL_INLINE bool allowModifyBy(const LLUUID& agent_id,
								 const LLUUID& group_id) const
	{
		return allowOperationBy(PERM_MODIFY, agent_id, group_id);
	}

	LL_INLINE bool allowCopyBy(const LLUUID& agent_id,
							   const LLUUID& group_id) const
	{
		return allowOperationBy(PERM_COPY, agent_id, group_id);
	}

	LL_INLINE bool allowMoveBy(const LLUUID& agent_id,
							   const LLUUID& group_id) const
	{
		return allowOperationBy(PERM_MOVE, agent_id, group_id);
	}

	// Returns true if export is allowed.
	bool allowExportBy(const LLUUID& requester, ExportPolicy policy) const;

	// This somewhat specialized function is meant for testing if the current
	// owner is allowed to transfer to the specified agent id.
	LL_INLINE bool allowTransferTo(const LLUUID& agent_id) const
	{
		if (mIsGroupOwned)
		{
			return allowOperationBy(PERM_TRANSFER, mGroup, mGroup);
		}
		return mOwner == agent_id || allowOperationBy(PERM_TRANSFER, mOwner);
	}

	//
	// MISC METHODS and OPERATORS
	//

	LLSD packMessage() const;
	void unpackMessage(LLSD perms);

	// For messaging system support
	void packMessage(LLMessageSystem* msg) const;
	void unpackMessage(LLMessageSystem* msg, const char* block,
					   S32 block_num = 0);

	bool importLegacyStream(std::istream& input_stream);
	bool exportLegacyStream(std::ostream& output_stream) const;

	bool operator==(const LLPermissions& rhs) const;
	bool operator!=(const LLPermissions& rhs) const;

	friend std::ostream& operator<<(std::ostream& s,
									const LLPermissions& perm);

private:
	// Correct for fair use - you can never take away the right to move stuff
	// you own, and you can never take away the right to transfer something you
	// cannot otherwise copy.
	void fixFairUse();

	// Fix internal consistency for group/agent ownership
	void fixOwnership();

private:
	LLUUID			mCreator;		// null if object created by system
	LLUUID			mOwner;			// null if object "unowned" (owned by system)
	LLUUID			mLastOwner;		// object's last owner
	LLUUID			mGroup;			// The group association

	// Initially permissive, progressively AND restricted by each owner
	PermissionMask	mMaskBase;

	// Set by owner, applies to owner only, restricts lower permissions
	PermissionMask	mMaskOwner;

	// Set by owner, applies to everyone else
	PermissionMask	mMaskEveryone;

	// Set by owner, applies to group that is associated with permissions
	PermissionMask	mMaskGroup;

	// Set by owner, applied to base on transfer.
	PermissionMask mMaskNextOwner;

	// Usually set in the fixOwnership() method based on current uuid values.
	bool			mIsGroupOwned;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLAggregatePermissions
//
// Class which encapsulates object and inventory permissions, ownership, etc.
// Currently, it only aggregates PERM_COPY, PERM_MODIFY, and PERM_TRANSFER.
//
// Usually you will construct an instance and hand the object several
// permissions masks to aggregate the copy, modify, and transferability into a
// nice trinary value.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLAggregatePermissions
{
protected:
	LOG_CLASS(LLAggregatePermissions);

public:
	enum EValue
	{
		AP_EMPTY	= 0x00,
		AP_NONE		= 0x01,
		AP_SOME		= 0x02,
		AP_ALL		= 0x03
	};

	// Constructs an empty aggregate permissions
	LLAggregatePermissions();

	// Pass in a PERM_COPY, PERM_TRANSFER, etc and get out a EValue enumeration
	// describing the current aggregate permissions.
	EValue getValue(PermissionBit bit) const;

	// Returns the permissions packed into the 6 LSB of a U8:
	// 00TTMMCC
	// where TT = transfer, MM = modify, and CC = copy
	// LSB is to the right
	U8 getU8() const;

	// Returns true if the aggregate permissions are empty, otherwise false.
	bool isEmpty() const;

	// Given a mask, aggregates the useful permissions.
	void aggregate(PermissionMask mask);

	// Aggregates aggregates
	void aggregate(const LLAggregatePermissions& ag);

	// Message handling
	void packMessage(LLMessageSystem* msg, const char* field) const;
	void unpackMessage(LLMessageSystem* msg, const char* block,
					   const char* field, S32 block_num = 0);

	static const LLAggregatePermissions empty;

	friend std::ostream& operator<<(std::ostream& s,
									const LLAggregatePermissions& perm);

protected:
	enum EPermIndex
	{
		PI_COPY		= 0,
		PI_MODIFY	= 1,
		PI_TRANSFER	= 2,
		PI_END 		= 3,
		PI_COUNT	= 3
	};
	void aggregateBit(EPermIndex idx, bool allowed);
	void aggregateIndex(EPermIndex idx, U8 bits);
	static EPermIndex perm2PermIndex(PermissionBit bit);

protected:
	// Structure used to store the aggregate so far.
	U8 mBits[PI_COUNT];
};

// These functions convert between structured data and permissions as
// appropriate for serialization. The permissions are a map of things like
// 'creator_id', 'owner_id', etc, with the value copied from the permission
// object.
LLSD ll_create_sd_from_permissions(const LLPermissions& perm);
LLPermissions ll_permissions_from_sd(const LLSD& sd_perm);

#endif
