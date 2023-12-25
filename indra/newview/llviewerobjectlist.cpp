/**
 * @file llviewerobjectlist.cpp
 * @brief Implementation of LLViewerObjectList class.
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 *
 * Copyright (c) 2001-2009, Linden Research, Inc.
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

#include <iterator>
#include <utility>

#include "zlib.h"

#include "llviewerobjectlist.h"

#include "llcommonmath.h"
#include "llcorehttputil.h"
#include "lldatapacker.h"
#include "llfasttimer.h"
#include "llkeyboard.h"
#include "lllocale.h"
#include "llrenderutils.h"
#include "llmessage.h"
#include "object_flags.h"

#include "llagent.h"
#include "llappviewer.h"
#include "lldrawable.h"
#include "llface.h"
#include "llflexibleobject.h"
#include "llhoverview.h"
#include "llhudtext.h"
#include "hbobjectbackup.h"
#include "llpanelminimap.h"
#include "llpipeline.h"
#include "llselectmgr.h"
#include "llsky.h"
#include "llspatialpartition.h"
#include "lltoolmgr.h"
#include "lltoolpie.h"
#include "hbviewerautomation.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewermessage.h"		// For add_newly_created_object()
#include "llviewerobject.h"
#include "llviewerregion.h"
#include "llviewerstats.h"
#include "llviewertextureanim.h"
#include "llviewerwindow.h"
#include "llvoavatarself.h"
#include "llvocache.h"
#include "llworld.h"

extern F32 gMinObjectDistance;

#define LL_IGNORE_DEAD	0

// Global lists of objects - should go away soon.
LLViewerObjectList gObjectList;

extern LLPipeline gPipeline;

// Maximum number of objects per cost/physics flags request (500 is the limit
// seen in SL as of 2016-06-14):
S32 gMaxObjectsPerFetch = 500;

// Statics for object lookup tables.

// Not zero deliberately, to speed up index check:
U32 LLViewerObjectList::sSimulatorMachineIndex = 1;
U64 LLViewerObjectList::sKilledAttachmentsStamp = 0;
uuid_list_t LLViewerObjectList::sKilledAttachments;
LLViewerObjectList::ip_to_idx_map_t LLViewerObjectList::sIPAndPortToIndex;
LLViewerObjectList::idx_to_uuid_map_t LLViewerObjectList::sIndexAndLocalIDToUUID;

// Derendered objects
uuid_list_t LLViewerObjectList::sBlackListedObjects;

LLViewerObjectList::LLViewerObjectList()
:	mWasPaused(false),
	mNumVisCulled(0),
	mNumSizeCulled(0),
	mCurLazyUpdateIndex(0),
	mCurBin(0),
	mNumOrphans(0),
	mNumNewObjects(0),
	mNumDeadObjectUpdates(0),
	mNumUnknownUpdates(0),
	mIdleListSlots(32768)
{
	mIdleList.reserve(mIdleListSlots);
}

void LLViewerObjectList::cleanupClass()
{
	llinfos << "Destroying all the objects in the list..." << llendl;

	killAllObjects();

	mDebugBeacons.clear();
	mUUIDObjectMap.clear();
	mUUIDAvatarMap.clear();

	llinfos << "All objects destroyed." << llendl;
}

void LLViewerObjectList::getUUIDFromLocal(LLUUID& id, U32 local_id, U32 ip,
										  U32 port)
{
	U64 ipport = (((U64)ip) << 32) | (U64)port;

	U32 index;
	ip_to_idx_map_t::iterator it = sIPAndPortToIndex.find(ipport);
	if (it == sIPAndPortToIndex.end())
	{
		index = sSimulatorMachineIndex++;
		sIPAndPortToIndex[ipport] = index;
	}
	else
	{
		index = it->second;
	}
	U64	indexid = (((U64)index) << 32) | (U64)local_id;
	id = get_if_there(sIndexAndLocalIDToUUID, indexid, LLUUID::null);
}

U64 LLViewerObjectList::getIndex(U32 local_id, U32 ip, U32 port)
{
	U64 ipport = (((U64)ip) << 32) | (U64)port;

	U32 index = sIPAndPortToIndex[ipport];
	if (!index)
	{
		return 0;
	}

	return (((U64)index) << 32) | (U64)local_id;
}

bool LLViewerObjectList::removeFromLocalIDTable(const LLViewerObject* objectp)
{
	LL_TRACY_TIMER(TRC_OBJ_REMOVE_LOCAL_ID);

	if (objectp && objectp->getRegion())
	{
		U32 local_id = objectp->mLocalID;
		LLHost region_host = objectp->getRegion()->getHost();
		U32 ip = region_host.getAddress();
		U32 port = region_host.getPort();
		U64 ipport = (((U64)ip) << 32) | (U64)port;
		U32 index = sIPAndPortToIndex[ipport];
		U64	indexid = (((U64)index) << 32) | (U64)local_id;

		idx_to_uuid_map_t::iterator it = sIndexAndLocalIDToUUID.find(indexid);
		if (it == sIndexAndLocalIDToUUID.end())
		{
			return false;
		}

		// Found existing entry
		if (it->second == objectp->getID())
		{
		   // Full UUIDs match, so remove the entry
			sIndexAndLocalIDToUUID.erase(it);
			return true;
		}
		// UUIDs did not match: this would zap a valid entry, so do not erase
		// it
	}

	return false;
}

void LLViewerObjectList::setUUIDAndLocal(const LLUUID& id, U32 local_id,
										 U32 ip, U32 port)
{
	U64 ipport = (((U64)ip) << 32) | (U64)port;

	
	U32 index = sIPAndPortToIndex[ipport];
	if (!index)
	{
		index = sSimulatorMachineIndex++;
		sIPAndPortToIndex[ipport] = index;
	}

	U64	indexid = (((U64)index) << 32) | (U64)local_id;

	sIndexAndLocalIDToUUID[indexid] = id;
	LL_DEBUGS("ObjectCacheSpam") << "Local Id " << local_id
								 << " associated with UUID " << id << LL_ENDL;
}

// Because there is a bug in message_template.msg, which define the "Set" field
// in the ObjectData block of the ObjectPermissions message as an U8 instead of
// a bool (it does not change anything at the message level, but it causes the
// viewers to issue warnings when they set "Set" as a bool, which it should be.
#define MESSAGE_TEMPLATE_FIXED 0

void LLViewerObjectList::processUpdateCore(LLViewerObject* objectp,
										   void** user_data, U32 i,
										   EObjectUpdateType update_type,
										   LLDataPacker* dpp,
										   bool just_created, bool from_cache)
{
	LL_TRACY_TIMER(TRC_PROCESS_OBJECTS_CORE);

	LLMessageSystem* msg = from_cache ? NULL : gMessageSystemp;

	// Ignore returned flags
	objectp->processUpdateMessage(msg, user_data, i, update_type, dpp);

	if (objectp->isDead())
	{
		// The update failed
		return;
	}

	updateActive(objectp);

	if (just_created)
	{
		gPipeline.addObject(objectp);
	}
	else
	{
		HBObjectBackup::primUpdate(objectp);
	}

	// Also sets the approx. pixel area
	objectp->setPixelAreaAndAngle();

	// RN: this must be called after we have a drawable (from
	// gPipeline.addObject) so that the drawable parent is set properly.
	if (msg)
	{
		findOrphans(objectp, msg->getSenderIP(), msg->getSenderPort());
	}
	else
	{
		LLViewerRegion* regionp = objectp->getRegion();
		if (regionp)
		{
			findOrphans(objectp, regionp->getHost().getAddress(),
						regionp->getHost().getPort());
		}
	}

	// If we are just wandering around, do not create new objects selected.
	if (just_created && update_type != OUT_TERSE_IMPROVED &&
		objectp->mCreateSelected)
	{
		if (!gToolMgr.isCurrentTool(&gToolPie))
		{
			LL_DEBUGS("ViewerObject") << "Selecting " << objectp->mID
									  << LL_ENDL;
			gSelectMgr.selectObjectAndFamily(objectp);
			dialog_refresh_all();
		}

		objectp->mCreateSelected = false;
		gWindowp->decBusyCount();
		gWindowp->setCursor(UI_CURSOR_ARROW);

		// Set the object permission to the user-selected default ones
		LLViewerRegion* region = objectp->getRegion();
		if (region)	// Paranoia
		{
#if MESSAGE_TEMPLATE_FIXED
			bool perm_modify = gSavedSettings.getBool("NextOwnerModify");
			bool perm_copy = gSavedSettings.getBool("NextOwnerCopy");
			bool perm_transfer = gSavedSettings.getBool("NextOwnerTransfer");
			bool perm_all_copy = gSavedSettings.getBool("EveryoneCopy");
			bool perm_group = gSavedSettings.getBool("ShareWithGroup");
#else
			U8 perm_modify = (U8)gSavedSettings.getBool("NextOwnerModify");
			U8 perm_copy = (U8)gSavedSettings.getBool("NextOwnerCopy");
			U8 perm_transfer = (U8)gSavedSettings.getBool("NextOwnerTransfer");
			U8 perm_all_copy = (U8)gSavedSettings.getBool("EveryoneCopy");
			U8 perm_group = (U8)gSavedSettings.getBool("ShareWithGroup");
#endif
			U32 local_id = objectp->getLocalID();
			LLMessageSystem* msg = gMessageSystemp;
			msg->newMessageFast(_PREHASH_ObjectPermissions);
			msg->nextBlockFast(_PREHASH_AgentData);
			msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
			msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
			msg->nextBlockFast(_PREHASH_HeaderData);
			msg->addBoolFast(_PREHASH_Override, false);

			msg->nextBlockFast(_PREHASH_ObjectData);
			msg->addU32Fast(_PREHASH_ObjectLocalID, local_id);
			msg->addU8Fast(_PREHASH_Field, PERM_NEXT_OWNER);
#if MESSAGE_TEMPLATE_FIXED
			msg->addBoolFast(_PREHASH_Set, perm_modify);
#else
			msg->addU8Fast(_PREHASH_Set, perm_modify);
#endif
			msg->addU32Fast(_PREHASH_Mask, PERM_MODIFY);

			msg->nextBlockFast(_PREHASH_ObjectData);
			msg->addU32Fast(_PREHASH_ObjectLocalID, local_id);
			msg->addU8Fast(_PREHASH_Field, PERM_NEXT_OWNER);
#if MESSAGE_TEMPLATE_FIXED
			msg->addBoolFast(_PREHASH_Set, perm_copy);
#else
			msg->addU8Fast(_PREHASH_Set, perm_copy);
#endif
			msg->addU32Fast(_PREHASH_Mask, PERM_COPY);

			msg->nextBlockFast(_PREHASH_ObjectData);
			msg->addU32Fast(_PREHASH_ObjectLocalID, local_id);
			msg->addU8Fast(_PREHASH_Field, PERM_NEXT_OWNER);
#if MESSAGE_TEMPLATE_FIXED
			msg->addBoolFast(_PREHASH_Set, perm_transfer);
#else
			msg->addU8Fast(_PREHASH_Set, perm_transfer);
#endif
			msg->addU32Fast(_PREHASH_Mask, PERM_TRANSFER);

			msg->nextBlockFast(_PREHASH_ObjectData);
			msg->addU32Fast(_PREHASH_ObjectLocalID, local_id);
			msg->addU8Fast(_PREHASH_Field, PERM_EVERYONE);
#if MESSAGE_TEMPLATE_FIXED
			msg->addBoolFast(_PREHASH_Set, perm_all_copy);
#else
			msg->addU8Fast(_PREHASH_Set, perm_all_copy);
#endif
			msg->addU32Fast(_PREHASH_Mask, PERM_COPY);

			msg->nextBlockFast(_PREHASH_ObjectData);
			msg->addU32Fast(_PREHASH_ObjectLocalID, local_id);
			msg->addU8Fast(_PREHASH_Field, PERM_GROUP);
#if MESSAGE_TEMPLATE_FIXED
			msg->addBoolFast(_PREHASH_Set, perm_group);
#else
			msg->addU8Fast(_PREHASH_Set, perm_group);
#endif
			msg->addU32Fast(_PREHASH_Mask,
							PERM_MODIFY | PERM_MOVE | PERM_COPY);

			msg->sendReliable(objectp->getRegion()->getHost());
		}

		HBObjectBackup::newPrim(objectp);
	}
}

LLViewerObject* LLViewerObjectList::processObjectUpdateFromCache(LLVOCacheEntry* entry,
																 LLViewerRegion* regionp)
{
	LL_TRACY_TIMER(TRC_PROCESS_OBJECTS);

	LLDataPacker* cached_dpp = entry->getDP();
	if (!cached_dpp)
	{
		return NULL; // Nothing cached.
	}

#if 0
	// Cache Hit.
	entry->recordHit();
#endif
	cached_dpp->reset();

	LLUUID fullid;
	cached_dpp->unpackUUID(fullid, "ID");
	U32 local_id;
	cached_dpp->unpackU32(local_id, "LocalID");
	LLPCode pcode = 0;
	cached_dpp->unpackU8(pcode, "PCode");

	if (sBlackListedObjects.count(fullid))
	{
		// This object was blacklisted/derendered: do not restore it from
		// cache.
		return NULL;
	}

	bool just_created = false;
	LLViewerObject* objectp = findObject(fullid);
	if (objectp)
	{
		if (!objectp->isDead() &&
			(objectp->mLocalID != entry->getLocalID() ||
			 objectp->getRegion() != regionp))
		{
			removeFromLocalIDTable(objectp);
			setUUIDAndLocal(fullid, entry->getLocalID(),
							regionp->getHost().getAddress(),
							regionp->getHost().getPort());

			if (objectp->mLocalID != entry->getLocalID())
			{
				// Update local ID in object with the one sent from the region
				objectp->setlocalID(entry->getLocalID());
			}

			if (objectp->getRegion() != regionp)
			{
				// Object changed region, so update it
				objectp->updateRegion(regionp); // for LLVOAvatar
			}
		}
#if 0	// Should fall through if already loaded because may need to update the
		// object.
		else
		{
			return objectp; // Already loaded.
		}
#endif
	}
	else if (mDeadObjects.count(fullid))
	{
		LL_DEBUGS("ViewerObject") << "Attempt to re-create a dead object for: "
								  << fullid << ". Skipping." << LL_ENDL;
		return NULL;
	}
	else
	{
		objectp = createObjectFromCache(pcode, regionp, fullid,
										entry->getLocalID());
		if (!objectp)
		{
			llinfos << "Failure to create object: " << fullid << llendl;
			return NULL;
		}
		just_created = true;
		++mNumNewObjects;
	}

	if (objectp->isDead())
	{
		llwarns << "Dead object " << objectp->mID << " in UUID map" << llendl;
	}

	processUpdateCore(objectp, NULL, 0, OUT_FULL_CACHED, cached_dpp,
					  just_created, true);
	// Just in case, reload update flags from cache:
	U32 flags = entry->getUpdateFlags();
	objectp->loadFlags(flags);

	if (entry->getHitCount() > 0)
	{
		objectp->setLastUpdateType(OUT_FULL_CACHED);
	}
	else
	{
		objectp->setLastUpdateType(OUT_FULL_COMPRESSED); // Newly cached
		objectp->setLastUpdateCached(true);
	}

	if (objectp)
	{
		regionp->loadCacheMiscExtras(objectp);
	}

	if (objectp->getPCode() == LL_PCODE_LEGACY_AVATAR)
	{
		LLVOAvatar::setAvatarCullingDirty();
	}

	return objectp;
}

void LLViewerObjectList::processObjectUpdate(LLMessageSystem* msg,
											 void** user_data,
											 EObjectUpdateType update_type,
											 bool compressed)
{
	LL_FAST_TIMER(FTM_PROCESS_OBJECTS);

	// Figure out which simulator these are from and get it's index.
	// Coordinates in simulators are region-local. Until we get region-locality
	// working on viewer we have to transform to absolute coordinates.
	S32 num_objects = msg->getNumberOfBlocksFast(_PREHASH_ObjectData);

#if 0
	if (!compressed && update_type != OUT_FULL)
	{
		S32 size;
		if (msg->getReceiveCompressedSize())
		{
			size = msg->getReceiveCompressedSize();
		}
		else
		{
			size = msg->getReceiveSize();
		}
		llinfos << "Received terse " << num_objects << " in " << size
				<< " bytes (" << size / num_objects << ")" << llendl;
	}
	else
	{
		S32 size;
		if (msg->getReceiveCompressedSize())
		{
			size = msg->getReceiveCompressedSize();
		}
		else
		{
			size = msg->getReceiveSize();
		}

		llinfos << "Received " << num_objects << " in " << size
				<< " bytes (" << size / num_objects << ")" << llendl;
	}
#endif

	U64 region_handle;
	msg->getU64Fast(_PREHASH_RegionData, _PREHASH_RegionHandle, region_handle);
	LLViewerRegion* regionp = gWorld.getRegionFromHandle(region_handle);
	if (!regionp)
	{
		llwarns << "Object update from unknown region ! " << region_handle
				<< llendl;
		return;
	}

	U8 compressed_dpbuffer[2048];
	LLDataPackerBinaryBuffer compressed_dp(compressed_dpbuffer, 2048);

	LLPCode pcode = 0;
	U32 local_id;
	LLUUID fullid;
	bool got_avatars = false;
	for (S32 i = 0; i < num_objects; ++i)
	{
		bool just_created = false;
		// Update object cache if it is a full-update or terse update
		bool update_cache = false;

		if (compressed)
		{
			compressed_dp.reset();
			S32 uncompressed_length = msg->getSizeFast(_PREHASH_ObjectData, i,
													   _PREHASH_Data);
			msg->getBinaryDataFast(_PREHASH_ObjectData, _PREHASH_Data,
								   compressed_dpbuffer, 0, i, 2048);
			compressed_dp.assignBuffer(compressed_dpbuffer,
									   uncompressed_length);
			if (update_type != OUT_TERSE_IMPROVED)
			{
				U32 flags = 0;
				msg->getU32Fast(_PREHASH_ObjectData, _PREHASH_UpdateFlags,
								flags, i);

				compressed_dp.unpackUUID(fullid, "ID");
				compressed_dp.unpackU32(local_id, "LocalID");
				compressed_dp.unpackU8(pcode, "PCode");
				if (mDeadObjects.count(fullid))
				{
					LL_DEBUGS("ViewerObject") << "Attempt to update a dead object for: "
											  << fullid << ". Skipping."
											  << LL_ENDL;
					continue;
				}
				if (pcode == 0)
				{
					llwarns_once << "Invalid Pcode (0) for object " << fullid
								 << " (LocalID: " << local_id << llendl;
					continue;

				}
				if ((flags & FLAGS_TEMPORARY_ON_REZ) == 0)
				{
					// Send to object cache
					regionp->cacheFullUpdate(compressed_dp, flags);
					continue;
				}
			}
			else	// OUT_TERSE_IMPROVED
			{
				update_cache = true;
				compressed_dp.unpackU32(local_id, "LocalID");
				getUUIDFromLocal(fullid, local_id, msg->getSenderIP(),
								 msg->getSenderPort());
				if (fullid.isNull())
				{
					LL_DEBUGS("ViewerObject") << "Update for unknown localid: "
											  << local_id << " - Host: "
											  << msg->getSender() << ":"
											  << msg->getSenderPort()
											  << LL_ENDL;
					++mNumUnknownUpdates;
				}
			}
		}
		else if (update_type != OUT_FULL)
		{
			msg->getU32Fast(_PREHASH_ObjectData, _PREHASH_ID, local_id, i);

			getUUIDFromLocal(fullid, local_id, msg->getSenderIP(),
							 msg->getSenderPort());
			if (fullid.isNull())
			{
				LL_DEBUGS("ViewerObject") << "Update for unknown localid: "
										  << local_id << " - Host: "
										  << msg->getSender() << ":"
										  << msg->getSenderPort()
										  << LL_ENDL;
				++mNumUnknownUpdates;
			}
		}
		else
		{
			update_cache = true;
			msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_FullID, fullid, i);
			msg->getU32Fast(_PREHASH_ObjectData, _PREHASH_ID, local_id, i);
			LL_DEBUGS("ViewerObject") << "Full Update, obj: " << local_id
									  << " - Global ID: " << fullid
									  << " - From: " << msg->getSender()
									  << LL_ENDL;
		}

		if (sBlackListedObjects.count(fullid))
		{
			// This object was blaclisted/derendered: do not recreate it
			continue;
		}
		if (mDeadObjects.count(fullid))
		{
			LL_DEBUGS("ViewerObject") << "Attempt to update a dead object for: "
									  << fullid << ". Skipping."
									  << LL_ENDL;
			continue;
		}

		
		LLViewerObject* objectp = findObject(fullid);
		bool new_object = objectp == NULL;
		if (update_cache)
		{
			// Update object cache if the object receives a full-update or
			// terse update
			objectp = regionp->updateCacheEntry(local_id, objectp);
		}
		if (new_object && objectp)
		{
			add_newly_created_object(objectp->mID);
		}

		if (sKilledAttachments.count(fullid))
		{
			bool remove = true;
			LL_DEBUGS("Attachment") << "Update for a killed attachment object: "
									<< fullid;
			if (!objectp && mDeadObjects.count(fullid))
			{
#if LL_IGNORE_DEAD
				LL_CONT << " - Object is in dead list !";
#else
				LL_CONT << " - Object is in dead list and this update will be discarded !";
				remove = false;
#endif
			}
			LL_CONT << LL_ENDL;
			if (remove)
			{
				sKilledAttachments.erase(fullid);
			}
		}

		// This looks like it will break if the local_id of the object does not
		// change upon boundary crossing, but we check for region id matching
		// later...
		// Reset object local id and region pointer if things have changed.
		if (objectp &&
			(objectp->mLocalID != local_id || objectp->getRegion() != regionp))
		{
#if 0
			if (objectp->getRegion())
			{
				llinfos << "Local ID change: Removing object from table, local ID "
						<< objectp->mLocalID << ", id from message "
						<< local_id << ", from "
						<< LLHost(objectp->getRegion()->getHost().getAddress(),
								  objectp->getRegion()->getHost().getPort())
						<< ", full id " << fullid << ", objects id "
						<< objectp->getID() << ", regionp " << (U32)regionp
						<< ", object region " << (U32)objectp->getRegion()
						<< llendl;
			}
#endif
			removeFromLocalIDTable(objectp);
			setUUIDAndLocal(fullid, local_id, msg->getSenderIP(),
							msg->getSenderPort());

			// Update local ID in object with the one sent from the region
			objectp->setlocalID(local_id);

			if (objectp->getRegion() != regionp)
			{
				// Object changed region, so update it
				objectp->updateRegion(regionp); // for LLVOAvatar
			}
		}

		if (!objectp)
		{
			if (compressed)
			{
				if (update_type == OUT_TERSE_IMPROVED)
				{
					LL_DEBUGS("ViewerObject") << "Terse update for an unknown object: "
											  << fullid << LL_ENDL;
					continue;
				}
			}
			else
			{
				if (update_type != OUT_FULL)
				{
					LL_DEBUGS("ViewerObject") << "Terse update for an unknown object: "
											  << fullid << LL_ENDL;
					continue;
				}

				msg->getU8Fast(_PREHASH_ObjectData, _PREHASH_PCode, pcode, i);
			}
#if LL_IGNORE_DEAD
			if (mDeadObjects.count(fullid))
			{
				++mNumDeadObjectUpdates;
				LL_DEBUGS("ViewerObject") << "Update for a dead object: "
										  << fullid << LL_ENDL;
				continue;
			}
#endif

			objectp = createObject(pcode, regionp, fullid, local_id,
								   msg->getSender());
			if (!objectp)
			{
				llwarns << "CreateObject failure for object: " << fullid
						<< llendl;
				continue;
			}
			just_created = true;
			++mNumNewObjects;
		}

		if (objectp->isDead())
		{
			llwarns << "Dead object " << objectp->mID << " in UUID map"
					<< llendl;
		}

		if (compressed)
		{
			if (update_type != OUT_TERSE_IMPROVED)
			{
				objectp->setlocalID(local_id);
			}
			processUpdateCore(objectp, user_data, i, update_type,
							  &compressed_dp, just_created);
#if 1
			if (update_type != OUT_TERSE_IMPROVED)
			{
				U32 flags = 0;
				msg->getU32Fast(_PREHASH_ObjectData, _PREHASH_UpdateFlags,
								flags, i);
				if (!(flags & FLAGS_TEMPORARY_ON_REZ))
				{
					objectp->mRegionp->cacheFullUpdate(objectp, compressed_dp,
													   flags);
					objectp->setLastUpdateCached(true);
				}
			}
#endif
		}
		else
		{
			if (update_type == OUT_FULL)
			{
				objectp->setlocalID(local_id);
			}
			processUpdateCore(objectp, user_data, i, update_type, NULL,
							  just_created);
		}

		objectp->setLastUpdateType(update_type);

		got_avatars |= objectp->getPCode() == LL_PCODE_LEGACY_AVATAR;
	}

	if (got_avatars)
	{
		LLVOAvatar::setAvatarCullingDirty();
	}
}

void LLViewerObjectList::processCompressedObjectUpdate(LLMessageSystem* msg,
													   void** user_data,
													   EObjectUpdateType t)
{
	processObjectUpdate(msg, user_data, t, true);
}

void LLViewerObjectList::processCachedObjectUpdate(LLMessageSystem* msg,
												   void** user_data,
												   EObjectUpdateType t)
{
#if 0
	processObjectUpdate(msg, user_data, t, false);
#endif

	S32 num_objects = msg->getNumberOfBlocksFast(_PREHASH_ObjectData);

	U64 region_handle;
	msg->getU64Fast(_PREHASH_RegionData, _PREHASH_RegionHandle, region_handle);
	LLViewerRegion* regionp = gWorld.getRegionFromHandle(region_handle);
	if (!regionp)
	{
		llwarns << "Object update from unknown region " << region_handle
				<< llendl;
		return;
	}

	for (S32 i = 0; i < num_objects; ++i)
	{
		U32 local_id, crc, flags;
		msg->getU32Fast(_PREHASH_ObjectData, _PREHASH_ID, local_id, i);
		msg->getU32Fast(_PREHASH_ObjectData, _PREHASH_CRC, crc, i);
		msg->getU32Fast(_PREHASH_ObjectData, _PREHASH_UpdateFlags,
							flags, i);
		// Lookup data packer and add this id to cache miss lists if necessary.
		U8 cache_miss_type = LLViewerRegion::CACHE_MISS_TYPE_NONE;
		regionp->probeCache(local_id, crc, flags, cache_miss_type);
	}
}

void LLViewerObjectList::dirtyAllObjectInventory()
{
	for (S32 i = 0, count = mObjects.size(); i < count; ++i)
	{
		LLViewerObject* objectp = mObjects[i];
		if (objectp)
		{
			objectp->dirtyInventory();
		}
	}
}

void LLViewerObjectList::updateApparentAngles()
{
	const S32 objects_size = (S32)mObjects.size();
	// The list can have shrunken down since mCurLazyUpdateIndex was last
	// updated.
	if (mCurLazyUpdateIndex >= objects_size)
	{
		mCurLazyUpdateIndex = 0;
	}

	S32 num_updates, max_value;
	if (mCurBin == NUM_BINS - 1)
	{
		// Remainder (mObjects.size() could have changed)
		num_updates = objects_size - mCurLazyUpdateIndex;
		max_value = objects_size;
	}
	else
	{
		num_updates = objects_size / NUM_BINS + 1;
		max_value = llmin(objects_size, mCurLazyUpdateIndex + num_updates);
	}

	bool got_avatars = false;
	LLViewerObject* objectp;
	if (gHoverViewp)
	{
		// Slam priorities for textures that we care about (hovered, selected,
		// and focused). Hovered. Assumes only one level deep of parenting
		objectp = gHoverViewp->getLastHoverObject();
		if (objectp && !objectp->isDead())
		{
			objectp->boostTexturePriority();
			got_avatars |= objectp->getPCode() == LL_PCODE_LEGACY_AVATAR;
		}
	}

	// Focused
	objectp = gAgent.getFocusObject();
	if (objectp && !objectp->isDead())
	{
		objectp->boostTexturePriority();
		got_avatars |= objectp->getPCode() == LL_PCODE_LEGACY_AVATAR;
	}

	// Selected
	struct f final : public LLSelectedObjectFunctor
	{
		bool apply(LLViewerObject* objectp) override
		{
			if (objectp && !objectp->isDead())
			{
				objectp->boostTexturePriority();
			}
			return true;
		}
	} func;
	gSelectMgr.getSelection()->applyToRootObjects(&func);

	// Iterate through some of the objects and lazy update their texture
	// priorities
#if LL_OPENMP
	objectp = NULL;
	size_t num_objects = max_value - mCurLazyUpdateIndex;
	std::vector<LLViewerObject*> objects(num_objects, objectp);
#	pragma omp parallel for private(objectp) reduction(|:got_avatars)
	for (S32 i = mCurLazyUpdateIndex; i < max_value; ++i)
	{
		objectp = mObjects[i];
		if (objectp && !objectp->isDead())
		{
			got_avatars |= objectp->getPCode() == LL_PCODE_LEGACY_AVATAR;
			// Update distance & gpw
			// Also sets the approx. pixel area:
			objectp->setPixelAreaAndAngle();
			// Store for later textures update
			objects[i - mCurLazyUpdateIndex] = objectp;
		}
	}

	// This CANNOT be called from an OMP thread, because updateTextures() may
	// result in OpenGL calls, that might themselves be threaded via pthread !
	for (S32 i = 0, count = objects.size(); i < count; ++i)
	{
		objectp = objects[i];
		if (objectp && !objectp->isDead())
		{
			// Update the image levels of textures for this object:
			objects[i]->updateTextures();
		}
	}
#else
	for (S32 i = mCurLazyUpdateIndex; i < max_value; ++i)
	{
		objectp = mObjects[i];
		if (objectp && !objectp->isDead())
		{
			got_avatars |= objectp->getPCode() == LL_PCODE_LEGACY_AVATAR;
			// Update distance & gpw
			// Also sets the approx. pixel area:
			objectp->setPixelAreaAndAngle();
			// Update the image levels of textures for this object:
			objectp->updateTextures();
		}
	}
#endif

	mCurLazyUpdateIndex = max_value;
	if ((size_t)mCurLazyUpdateIndex == mObjects.size())
	{
		// Restart
		mCurLazyUpdateIndex = 0;
		// Keep in sync with index (mObjects.size() could have changed)
		mCurBin = 0;
	}
	else
	{
		mCurBin = (mCurBin + 1) % NUM_BINS;
	}

	if (got_avatars || LLVOAvatar::avatarCullingDirty())
	{
		LLVOAvatar::cullAvatarsByPixelArea();
	}
}

void LLViewerObjectList::update()
{
	// Update global timers
	F32 last_time = gFrameTimeSeconds;
	// This will become the new gFrameTime when the update is done
	U64 time = LLTimer::totalTime();
	F64 time_diff = U64_to_F64(time - gFrameTime) / (F64)SEC_TO_MICROSEC;
	if (time_diff < 0.0)
	{
		// Time went backwards... Use last frame interval as an approximation.
		time_diff = gFrameIntervalSeconds;
		// Adjust start time accordingly...
		gStartTime += time - gFrameTime;
	}
	gFrameTime = time;
	F64 time_since_start = U64_to_F64(gFrameTime - gStartTime) /
						   (F64)SEC_TO_MICROSEC;
	gFrameTimeSeconds = (F32)time_since_start;

	gFrameIntervalSeconds = gFrameTimeSeconds - last_time;
	if (gFrameIntervalSeconds < 0.f)
	{
		gFrameIntervalSeconds = 0.f;
	}

	// Clear avatar LOD change counter
	LLVOAvatar::sNumLODChangesThisFrame = 0;

	const F64 frame_time = LLFrameTimer::getElapsedSeconds();

	// Make a copy of the list in case something in idleUpdate() messes with it
	S32 idle_count = 0;
	{
		LL_FAST_TIMER(FTM_OBJECTLIST_COPY);

		S32 count = mActiveObjects.size();
		if (count > mIdleListSlots)
		{
			// Minimize fragmentation and reallocation time overhead:
			mIdleList.clear();
			mIdleListSlots = 125 * count / 100;
			mIdleList.reserve(mIdleListSlots);
		}

		S32 idle_list_old_size = mIdleList.size();
		for (S32 i = 0; i < count; )
		{
			LLViewerObject* objectp = mActiveObjects[i];
			if (!objectp)
			{
				// There should not be any NULL pointer in the list, but they
				// have caused crashes before.
				llwarns << "mActiveObjects has a NULL object. Removing."
						<< llendl;
				if (i != --count)
				{
					mActiveObjects[i] = std::move(mActiveObjects.back());
				}
				mActiveObjects.pop_back();
				continue;
			}
			if (objectp->isDead())
			{
				// There should not be any dead object in the list, but they
				// have caused crashes before.
				llwarns << "mActiveObjects has dead object "
						<< objectp->getID() << ". Removing." << llendl;
				mDeadList.push_back(objectp);
			}
			else if (idle_count >= idle_list_old_size)
			{
				mIdleList.push_back(objectp);
				++idle_count;
			}
			else
			{
				mIdleList[idle_count++] = objectp;
			}
			++i;
		}
	}

	S32 count = mDeadList.size();
	if (count)
	{
		LL_DEBUGS("ViewerObject") << "Removing detected dead objects from the active objects list."
								  << LL_ENDL;
		for (S32 i = 0; i < count; ++i )
		{
			cleanupReferences(mDeadList[i]);
		}
		mDeadList.clear();
	}

	if (LLPipeline::sFreezeTime)
	{
		for (S32 i = 0; i < idle_count; ++i)
		{
			LLViewerObject* objectp = mIdleList[i];
			if (objectp->isAvatar() ||
				objectp->getPCode() == LLViewerObject::LL_VO_CLOUDS)
			{
				objectp->idleUpdate(frame_time);
			}
		}
	}
	else
	{
		for (S32 i = 0; i < idle_count; ++i)
		{
			LLViewerObject* objectp = mIdleList[i];
			objectp->idleUpdate(frame_time);
		}

		// Update flexible objects
		LLVolumeImplFlexible::updateClass();

		if (LLVOVolume::sAnimateTextures)
		{
			// Update animated textures
			LLViewerTextureAnim::updateClass();
		}
	}

	fetchObjectCosts();
	fetchPhysicsFlags();

	mNumSizeCulled = 0;
	mNumVisCulled = 0;

	// Compute all sorts of time-based stats; do not factor frames that were
	// paused into the stats.
	if (!mWasPaused)
	{
		gViewerStats.updateFrameStats(time_diff);
	}

#if 0
	// Debugging code for viewing orphans, and orphaned parents
	LLUUID id;
	std::string id_str;
	for (S32 i = 0, count = mOrphanParents.size(); i < count; ++i)
	{
		id = sIndexAndLocalIDToUUID[mOrphanParents[i]];
		LLViewerObject* objectp = findObject(id);
		if (objectp)
		{
			objectp->mID.toString(id_str);
			std::string tmpstr = "Parent: " + id_str;
			addDebugBeacon(objectp->getPositionAgent(), tmpstr, LLColor4::red,
						   LLColor4::white);
		}
	}

	LLColor4 text_color;
	std::string tmpstr;
	for (S32 i = 0, count = mOrphanChildren.size(); i < count; ++i)
	{
		OrphanInfo oi = mOrphanChildren[i];
		LLViewerObject* objectp = findObject(oi.mChildInfo);
		if (objectp)
		{
			objectp->mID.toString(id_str);
			if (objectp->getParent())
			{
				tmpstr = "ChP: " + id_str;
				text_color = LLColor4::green;
			}
			else
			{
				tmpstr = "ChNoP: " + id_str;
				text_color = LLColor4::red;
			}
			id = sIndexAndLocalIDToUUID[oi.mParentInfo];
			addDebugBeacon(objectp->getPositionAgent() +
						   LLVector3(0.f, 0.f, -0.25f),
						   tmpstr, LLColor4::grey4,
						   text_color);
		}
	}
#endif

	mNumObjectsStat.addValue((S32)(mObjects.size() - mDeadObjects.size()));
	mNumActiveObjectsStat.addValue(idle_count);
	mNumSizeCulledStat.addValue(mNumSizeCulled);
	mNumVisCulledStat.addValue(mNumVisCulled);
}

// Issues HTTP request for stale object physics costs
void LLViewerObjectList::fetchObjectCosts()
{
	if (mStaleObjectCost.empty())
	{
		return;
	}

	const std::string& url = gAgent.getRegionCapability("GetObjectCost");
	if (url.empty())
	{
		mStaleObjectCost.clear();
		mPendingObjectCost.clear();
		return;
	}

	gCoros.launch("LLViewerObjectList::fetchObjectCostsCoro",
				  boost::bind(&LLViewerObjectList::fetchObjectCostsCoro, this,
							  url));
}

void LLViewerObjectList::fetchObjectCostsCoro(const std::string& url)
{
	LLSD object_ids = LLSD::emptyArray();
	S32 count = 0;
	uuid_list_t::iterator it = mStaleObjectCost.begin();
	while (it != mStaleObjectCost.end() && count < gMaxObjectsPerFetch)
	{
		// Check to see if a request for this object has already been made.
		if (mPendingObjectCost.find(*it) == mPendingObjectCost.end())
		{
			mPendingObjectCost.emplace(*it);
			object_ids.append(*it);
			++count;
		}

		mStaleObjectCost.erase(it++);
	}

	if (object_ids.size() < 1)
	{
		return;
	}

	LLSD body = LLSD::emptyMap();
	body["object_ids"] = object_ids;

	LLCoreHttpUtil::HttpCoroutineAdapter adapter("fetchObjectCostsCoro");
	LLSD result = adapter.postAndSuspend(url, body);

	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	if (!status || result.has("error"))
	{
		if (result.has("error"))
		{
			const LLSD& error = result["error"];
			std::string identifier;
			if (error.has("identifier"))
			{
				identifier = error["identifier"].asString();
			}
			llwarns << "Application level error when fetching object cost. Message: "
					<< error["message"].asString()
					<< " - Identifier: " << identifier << llendl;

			if (gMaxObjectsPerFetch > 32 &&
				identifier.find("TooManyObjects") != std::string::npos)
			{
				// Reduce the maximum number of objects per fetch by 25%.
				gMaxObjectsPerFetch = 4 * gMaxObjectsPerFetch / 5;
				llinfos << "Reduced maximum objects limit per fetch to: "
						<< gMaxObjectsPerFetch << llendl;
			}
		}

		// *TODO: No more hard coding
		for (LLSD::array_iterator iter = object_ids.beginArray(),
								  end = object_ids.endArray();
			iter != end; ++iter)
		{
			onObjectCostFetchFailure(iter->asUUID());
		}

		return;
	}

	// Success, grab the resource cost and linked set costs for an object if
	// one was returned
	for (LLSD::array_iterator it = object_ids.beginArray(),
							  end = object_ids.endArray();
		 it != end; ++it)
	{
		LLUUID object_id = it->asUUID();

		// If the object was added to the StaleObjectCost set after it had been
		// added to mPendingObjectCost it would still be in the StaleObjectCost
		// set when we got the response back.
		mStaleObjectCost.erase(object_id);

		// Check to see if the request contains data for the object
		if (result.has(it->asString()))
		{
			LLSD data = result[it->asString()];
			F32 link_cost = data["linked_set_resource_cost"].asReal();
			F32 object_cost = data["resource_cost"].asReal();
			F32 physics_cost = data["physics_cost"].asReal();
			F32 linkset_cost = data["linked_set_physics_cost"].asReal();
			updateObjectCost(object_id, object_cost, link_cost, physics_cost,
							 linkset_cost);
		}
		else
		{
			// *TODO: Give user feedback about the missing data ?
			onObjectCostFetchFailure(object_id);
		}
	}
}

// Issues HTTP request for stale object physics flags
void LLViewerObjectList::fetchPhysicsFlags()
{
	if (mStalePhysicsFlags.empty())
	{
		return;
	}

	const std::string& url =
		gAgent.getRegionCapability("GetObjectPhysicsData");
	if (url.empty())
	{
		mStalePhysicsFlags.clear();
		mPendingPhysicsFlags.clear();
		return;
	}

	gCoros.launch("LLViewerObjectList::fetchPhysicsFlagsCoro",
				  boost::bind(&LLViewerObjectList::fetchPhysicsFlagsCoro, this,
							  url));
}

void LLViewerObjectList::fetchPhysicsFlagsCoro(const std::string& url)
{
	LLSD object_ids;
	S32 object_index = 0;
	uuid_list_t::iterator it = mStalePhysicsFlags.begin();
	while (it != mStalePhysicsFlags.end() &&
		   object_index < gMaxObjectsPerFetch)
	{
		// Check to see if a request for this object has already been made.
		if (mPendingPhysicsFlags.find(*it) == mPendingPhysicsFlags.end())
		{
			mPendingPhysicsFlags.emplace(*it);
			object_ids[object_index++] = *it;
		}

		mStalePhysicsFlags.erase(it++);
	}

	if (object_ids.size() < 1)
	{
		return;
	}

	LLSD body = LLSD::emptyMap();
	body["object_ids"] = object_ids;

	LLCoreHttpUtil::HttpCoroutineAdapter adapter("fetchPhysicsFlagsCoro");
	LLSD result = adapter.postAndSuspend(url, body);

	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	if (!status || result.has("error"))
	{
		if (result.has("error"))
		{
			const LLSD& error = result["error"];
			std::string identifier;
			if (error.has("identifier"))
			{
				identifier = error["identifier"].asString();
			}
			llwarns << "Application level error when fetching object physics flags. Message: "
					<< error["message"].asString()
					<< " - Identifier: " << identifier << llendl;

			if (gMaxObjectsPerFetch > 32 &&
				identifier.find("TooManyObjects") != std::string::npos)
			{
				// Reduce the maximum number of objects per fetch by 25%.
				gMaxObjectsPerFetch = 4 * gMaxObjectsPerFetch / 5;
				llinfos << "Reduced maximum objects limit per fetch to: "
						<< gMaxObjectsPerFetch << llendl;
			}
		}

		for (LLSD::array_iterator iter = object_ids.beginArray(),
								  end = object_ids.endArray();
			iter != end; ++iter)
		{
			onPhysicsFlagsFetchFailure(iter->asUUID());
		}

		return;
	}

	// Success, grab the physics parameters for an object if one was returned
	for (LLSD::array_iterator it = object_ids.beginArray(),
							  end = object_ids.endArray();
		 it != end; ++it)
	{
		LLUUID object_id = it->asUUID();

		// Check to see if the request contains data for the object
		if (result.has(it->asString()))
		{
			const LLSD& data = result[it->asString()];
			S32 shapeType = data["PhysicsShapeType"].asInteger();

			updatePhysicsShapeType(object_id, shapeType);

			if (data.has("Density"))
			{
				F32 density = data["Density"].asReal();
				F32 friction = data["Friction"].asReal();
				F32 restitution = data["Restitution"].asReal();
				F32 gravity = data["GravityMultiplier"].asReal();
				updatePhysicsProperties(object_id, density, friction,
										restitution, gravity);
			}
		}
		else
		{
			// *TODO: Give user feedback about the missing data?
			onPhysicsFlagsFetchFailure(object_id);
		}
	}
}

bool LLViewerObjectList::gotObjectPhysicsFlags(LLViewerObject* objectp)
{
	// This will insert objectp in mStalePhysicsFlags if needed:
	objectp->getPhysicsShapeType();
	// Data has been retrieved if the object is not in either of the two lists:
	const LLUUID& id = objectp->getID();
	return mPendingPhysicsFlags.count(id) == 0 &&
		   mStalePhysicsFlags.count(id) == 0;
}

void LLViewerObjectList::clearDebugText()
{
	for (S32 i = 0, count = mObjects.size(); i < count; ++i)
	{
		LLViewerObject* objectp = mObjects[i];
		if (objectp && !objectp->isDead())
		{
			objectp->setDebugText("");
		}
	}
}

void LLViewerObjectList::cleanupReferences(LLViewerObject* objectp)
{
	LL_TRACY_TIMER(TRC_OBJ_CLEANUP_REF);

	if (!objectp)
	{
		llwarns << "NULL object pointer passed." << llendl;
		return;
	}

	const LLUUID& object_id = objectp->mID;

	// Cleanup any references we have to this object. Remove from object maps
	// so that no one can look it up.
	mUUIDObjectMap.erase(object_id);
	mUUIDAvatarMap.erase(object_id);

	if (mDeadObjects.count(object_id))
	{
		LL_DEBUGS("ViewerObject") << "Object " << object_id
								  << " already on dead list !" << LL_ENDL;
	}
	else
	{
		mDeadObjects.emplace(object_id);
	}

	removeFromLocalIDTable(objectp);

	if (objectp->onActiveList())
	{
		LL_DEBUGS("ViewerObject") << "Removing " << object_id << " "
								  << objectp->getPCodeString()
								  << " from active list." << LL_ENDL;
		objectp->setOnActiveList(false);
		removeFromActiveList(objectp);
	}

	if (objectp->isOnMap())
	{
		removeFromMap(objectp);
	}
}

bool LLViewerObjectList::killObject(LLViewerObject* objectp)
{
	// Do not ever kill gAgentAvatarp, just force it to the agent's region
	// unless region is NULL which is assumed to mean you are logging out.
	if (objectp == gAgentAvatarp && gAgent.getRegion())
	{
		objectp->setRegion(gAgent.getRegion());
		return false;
	}

	// When we are killing objects, all we do is mark them as dead.
	// We clean up the dead objects later.
	if (objectp)
	{
#if 0	// NOT needed for the Cool VL Viewer; the only occurrence of such
		// a self-destructing object in markDead() (puppet avatar unlinking)
		// was already identified and fixed by me in LLViewerObject::markDead()
		// (see the comment at the end of that method)... Plus, smart pointers
		// to killed objects are kept in mObjects till cleanDeadObjects() is
		// called, so the reason given below is bullshit. HB
		// We are going to cleanup a lot of smart pointers to this object; they
		// might be last, and object being NULLed while inside its own function
		// would cause a crash. So, create a smart pointer to make sure the
		// object will stay alive untill markDead() finishes.
		LLPointer<LLViewerObject> objp(objectp);
		objp->markDead(); // Does the right thing if object already dead
#else
		objectp->markDead(); // Does the right thing if object already dead
#endif
		return true;
	}

	return false;
}

void LLViewerObjectList::killObjects(LLViewerRegion* regionp)
{
	LLTimer kill_timer;

	S32 killed = 0;
	for (S32 i = 0, count = mObjects.size(); i < count; ++i)
	{
		LLViewerObject* objectp = mObjects[i];
		if (objectp && objectp->mRegionp == regionp)
		{
			++killed;
			killObject(objectp);
		}
	}

	// Have to clean right away because the region is becoming invalid.
	cleanDeadObjects();
	llinfos << "Removed " << killed << " objects for region "
			<< regionp->getIdentity() << " in "
			<< kill_timer.getElapsedTimeF64() * 1000.0 << "ms" << llendl;
}

// Used only on global destruction.
void LLViewerObjectList::killAllObjects()
{
	llinfos << "Marking all objects dead..." << llendl;
	for (S32 i = 0, count = mObjects.size(); i < count; ++i)
	{
		LLViewerObject* objectp = mObjects[i];
		if (objectp)
		{
			killObject(objectp);
			// Object must be dead, or it is the LLVOAvatarSelf which never
			// dies
			llassert(objectp == gAgentAvatarp || objectp->isDead());
		}
	}

	llinfos << "Cleaning up dead objects..." << llendl;
	cleanDeadObjects();

	if (!mDeadObjects.empty())
	{
		llwarns << "There are still has entries left in mDeadObjects: "
				<< mObjects.size() << llendl;
		mDeadObjects.clear();
	}

	if (!mObjects.empty())
	{
		llwarns << "There are still has entries left in mObjects: "
				<< mObjects.size() << llendl;
		mObjects.clear();
	}

	if (!mActiveObjects.empty())
	{
		llwarns << "Some objects still on active object list !" << llendl;
		mActiveObjects.clear();
	}

	if (!mMapObjects.empty())
	{
		llwarns << "Some objects still on map object list !" << llendl;
		mMapObjects.clear();
	}
}

void LLViewerObjectList::cleanDeadObjects()
{
	U32 num_dead_objects = mDeadObjects.size();
	if (!num_dead_objects)
	{
		// No dead object, so we do not need to scan objects list.
		return;
	}

	U32 num_removed = 0;
	U32 null_objects = 0;
	vobj_list_t::iterator iter = mObjects.begin();
	while (iter != mObjects.end())
	{
		LLViewerObject* objectp = (*iter).get();
		if (!objectp || objectp->isDead())
		{
			if (iter + 1 != mObjects.end())
			{
				*iter = std::move(mObjects.back());
			}
			mObjects.pop_back();
			if (objectp)
			{
				++num_removed;
			}
			else
			{
				++null_objects;
			}
		}
		else
		{
			++iter;
		}
	}

	if (num_removed != num_dead_objects)
	{
		llwarns << "Removed " << num_removed
				<< " dead objects from the list while it was supposed to have "
				<< num_dead_objects << " such objects in it." << llendl;
		llassert(false);
	}

	if (null_objects)
	{
		llwarns << "Found " << null_objects
				<< " NULL objects in the list (now removed)." << llendl;
		llassert(false);
	}

	// Blow away the dead list.
	mDeadObjects.clear();
}

void LLViewerObjectList::removeFromActiveList(LLViewerObject* objectp)
{
	S32 idx = objectp->getListIndex();
	if (idx != -1)
	{
		// Remove by moving last element to this object's position
		llassert(mActiveObjects[idx] == objectp);

		objectp->setListIndex(-1);

		S32 last_index = mActiveObjects.size() - 1;
		if (idx != last_index)
		{
			mActiveObjects[idx] = std::move(mActiveObjects[last_index]);
			mActiveObjects[idx]->setListIndex(idx);
		}

		mActiveObjects.pop_back();
	}
}

void LLViewerObjectList::updateActive(LLViewerObject* objectp)
{
	LL_TRACY_TIMER(TRC_OBJ_UPDATE_ACTIVE);

	if (!objectp || objectp->isDead())
	{
		return; // We do not update dead objects !
	}

	bool active = objectp->isActive();
	if (active != objectp->onActiveList())
	{
		if (active)
		{
			S32 idx = objectp->getListIndex();
			if (idx <= -1)
			{
				mActiveObjects.push_back(objectp);
				objectp->setListIndex(mActiveObjects.size() - 1);
				objectp->setOnActiveList(true);
			}
			else if (idx >= (S32)mActiveObjects.size() ||
					 mActiveObjects[idx] != objectp)
			{
				llwarns << "Invalid object list index detected !"
						<< llendl;
				llassert(false);
			}
		}
		else
		{
			removeFromActiveList(objectp);
			objectp->setOnActiveList(false);
		}
	}

	// Post condition: if object is active, it must be on the active list:
	llassert(!active || std::find(mActiveObjects.begin(),
								  mActiveObjects.end(),
								  objectp) != mActiveObjects.end());

	// Post condition: if object is not active, it must not be on the active
	// list
	llassert(active || std::find(mActiveObjects.begin(),
								 mActiveObjects.end(),
								 objectp) == mActiveObjects.end());
}

void LLViewerObjectList::updateObjectCost(LLViewerObject* object)
{
	if (object && !object->isDead())
	{
		if (!object->isRoot() && object->getParent())
		{
			// Always fetch cost for the parent when fetching cost for children
			mStaleObjectCost.emplace(((LLViewerObject*)object->getParent())->getID());
		}
		mStaleObjectCost.emplace(object->getID());
	}
}

void LLViewerObjectList::updateObjectCost(const LLUUID& object_id,
										  F32 object_cost, F32 link_cost,
										  F32 physics_cost,
										  F32 link_physics_cost)
{
	mPendingObjectCost.erase(object_id);

	LLViewerObject* object = findObject(object_id);
	if (object && !object->isDead())
	{
		object->setObjectCost(object_cost);
		object->setLinksetCost(link_cost);
		object->setPhysicsCost(physics_cost);
		object->setLinksetPhysicsCost(link_physics_cost);
	}
}

void LLViewerObjectList::onObjectCostFetchFailure(const LLUUID& object_id)
{
	mPendingObjectCost.erase(object_id);
}

void LLViewerObjectList::updatePhysicsFlags(const LLViewerObject* object)
{
	mStalePhysicsFlags.emplace(object->getID());
}

void LLViewerObjectList::updatePhysicsShapeType(const LLUUID& object_id,
												S32 type)
{
	mPendingPhysicsFlags.erase(object_id);

	LLViewerObject* object = findObject(object_id);
	if (object && !object->isDead())
	{
		object->setPhysicsShapeType(type);
	}
}

void LLViewerObjectList::updatePhysicsProperties(const LLUUID& object_id,
												 F32 density, F32 friction,
												 F32 restitution,
												 F32 gravity_multiplier)
{
	mPendingPhysicsFlags.erase(object_id);

	LLViewerObject* object = findObject(object_id);
	if (object && !object->isDead())
	{
		object->setPhysicsDensity(density);
		object->setPhysicsFriction(friction);
		object->setPhysicsGravity(gravity_multiplier);
		object->setPhysicsRestitution(restitution);
	}
}

void LLViewerObjectList::onPhysicsFlagsFetchFailure(const LLUUID& object_id)
{
	mPendingPhysicsFlags.erase(object_id);
}

// This is called when we shift our origin when we cross region boundaries...
// We need to update many object caches, I will document this more as I dig
// through the code cleaning things out...
void LLViewerObjectList::shiftObjects(const LLVector3& offset)
{
	if (offset.lengthSquared() == 0.f)
	{
		return;
	}

	LL_FAST_TIMER(FTM_SHIFT_OBJECTS);

	for (S32 i = 0, count = mObjects.size(); i < count; ++i)
	{
		LLViewerObject* objectp = mObjects[i];
		// There could be dead objects on the object list, so do not update
		// stuff if the object is dead.
		if (objectp && !objectp->isDead())
		{
			objectp->updatePositionCaches();
			// NOTE: LLPipeline::markShift() tests for non-NULL and non-dead
			// drawable, so not need to perform these tests here. HB
			gPipeline.markShift(objectp->mDrawable);
		}
	}

	{
		LL_FAST_TIMER(FTM_PIPELINE_SHIFT);
		gPipeline.shiftObjects(offset);
	}

	{
		LL_FAST_TIMER(FTM_REGION_SHIFT);
		gWorld.shiftRegions(offset);
	}
}

void LLViewerObjectList::repartitionObjects()
{
	for (S32 i = 0, count = mObjects.size(); i < count; ++i)
	{
		LLViewerObject* objectp = mObjects[i];
		if (objectp && !objectp->isDead())
		{
			LLDrawable* drawable = objectp->mDrawable;
			if (drawable && !drawable->isDead())
			{
				drawable->updateBinRadius();
				drawable->updateSpatialExtents();
				drawable->movePartition();
			}
		}
	}
}

void LLViewerObjectList::clearAllMapObjectsInRegion(LLViewerRegion* regionp)
{
	for (S32 i = 0, count = mMapObjects.size(); i < count; ++i)
	{
		LLViewerObject* objectp = mMapObjects[i];
		// Clean up the list from invalid objects, while we are at it... HB
		if (!objectp || objectp->isDead() || objectp->isOrphaned() ||
			// ... and remove objects present in the specified region.
			objectp->getRegion() == regionp)
		{
			if (i != --count)
			{
				mMapObjects[i] = std::move(mMapObjects.back());
			}
			mMapObjects.pop_back();
		}
	}
}

void LLViewerObjectList::renderObjectsForMap(LLPanelMiniMap* map)
{
	static LLCachedControl<LLColor4U>
		above_water_color(gColors, "MiniMapOtherOwnAboveWater");
	static LLCachedControl<LLColor4U>
		below_water_color(gColors, "MiniMapOtherOwnBelowWater");
	static LLCachedControl<LLColor4U>
		you_own_above_water_color(gColors, "MiniMapYouOwnAboveWater");
	static LLCachedControl<LLColor4U>
		you_own_below_water_color(gColors, "MiniMapYouOwnBelowWater");
	static LLCachedControl<LLColor4U>
		group_own_above_water_color(gColors, "MiniMapGroupOwnAboveWater");
	static LLCachedControl<LLColor4U>
		group_own_below_water_color(gColors, "MiniMapGroupOwnBelowWater");

	for (S32 i = 0, count = mMapObjects.size(); i < count; ++i)
	{
		LLViewerObject* objectp = mMapObjects[i];
		// Clean up the list from invalid objects, while we are at it. HB
		if (!objectp || objectp->isDead() || !objectp->getRegion() ||
			objectp->isOrphaned())
		{
			if (i != --count)
			{
				mMapObjects[i] = std::move(mMapObjects.back());
			}
			mMapObjects.pop_back();
			continue;
		}

		const LLVector3d pos = objectp->getPositionGlobal();

		if (objectp->flagCharacter())
		{
			if (objectp->isRoot())
			{
				// Path-finding characters are ploted by the mini-map code
				// itself.
				map->addPathFindingCharacter(pos);
			}
			continue;
		}

		const LLVector3& scale = objectp->getScale();
		const F64 water_height = F64(objectp->getRegion()->getWaterHeight());

		// Note: 0.325 = 0.5^2 * 1.3 (1.3 is a fudge)
		F32 approx_radius = (scale.mV[VX] + scale.mV[VY]) * 0.325f;

		// We draw physical objects after all others (and thus above them), to
		// be sure to see them on map...
		if (objectp->flagUsePhysics())
		{
			if (objectp->isRoot())
			{
				// Physcical objects (mobiles) are ploted by the mini-map code
				// itself.
				map->addPhysicalObject(pos);
			}
			continue;
		}

		LLColor4U color = above_water_color;
		if (objectp->permYouOwner())
		{
			constexpr F32 MIN_RADIUS_FOR_OWNED_OBJECTS = 2.f;
			if (approx_radius < MIN_RADIUS_FOR_OWNED_OBJECTS)
			{
				approx_radius = MIN_RADIUS_FOR_OWNED_OBJECTS;
			}

			if (pos.mdV[VZ] >= water_height)
			{
				if (objectp->permGroupOwner())
				{
					color = group_own_above_water_color;
				}
				else
				{
					color = you_own_above_water_color;
				}
			}
			else if (objectp->permGroupOwner())
			{
				color = group_own_below_water_color;
			}
			else
			{
				color = you_own_below_water_color;
			}
		}
		else if (pos.mdV[VZ] < water_height)
		{
			color = below_water_color;
		}

		map->renderScaledPointGlobal(pos, color, approx_radius);
	}
}

void LLViewerObjectList::addDebugBeacon(const LLVector3& pos_agent,
										const std::string& text,
										const LLColor4& color,
										const LLColor4& text_color,
										S32 line_width)
{
	mDebugBeacons.emplace_back(pos_agent, text, text_color, color, line_width);
}

void LLViewerObjectList::resetObjectBeacons()
{
	mDebugBeacons.clear();
}

LLViewerObject* LLViewerObjectList::createObjectViewer(LLPCode pcode,
													   LLViewerRegion* regionp,
													   S32 flags)
{
	LLUUID fullid;
	fullid.generate();

	LLViewerObject* objectp = LLViewerObject::createObject(fullid, pcode,
														   regionp, flags);
	if (!objectp)
	{
		LL_DEBUGS("ViewerObject") << "Could not create object of type "
								  << LLPrimitive::pCodeToString(pcode)
								  << LL_ENDL;
		return NULL;
	}

	mUUIDObjectMap[fullid] = objectp;

	if (objectp->isAvatar())
	{
		LLVOAvatar* avatarp = objectp->asAvatar();
		if (avatarp)
		{
			mUUIDAvatarMap[fullid] = avatarp;
		}
	}

	mObjects.push_back(objectp);

	updateActive(objectp);

	return objectp;
}

LLViewerObject* LLViewerObjectList::createObjectFromCache(LLPCode pcode,
														  LLViewerRegion* regionp,
														  const LLUUID& uuid,
														  U32 local_id)
{
	llassert_always(uuid.notNull());

	LLViewerObject* objectp = LLViewerObject::createObject(uuid, pcode,
														   regionp);
	if (!objectp)
	{
 		LL_DEBUGS("ViewerObject") << "Could not create object of type "
								  << LLPrimitive::pCodeToString(pcode)
								  << " - Id: " << uuid << LL_ENDL;
		return NULL;
	}
	LL_DEBUGS("ObjectCacheSpam") << "Created object " << uuid
								 << " from cache." << LL_ENDL;

	objectp->setlocalID(local_id);
	mUUIDObjectMap[uuid] = objectp;
	setUUIDAndLocal(uuid, local_id, regionp->getHost().getAddress(),
					regionp->getHost().getPort());
	mObjects.push_back(objectp);

	updateActive(objectp);

	return objectp;
}

LLViewerObject* LLViewerObjectList::createObject(LLPCode pcode,
												 LLViewerRegion* regionp,
												 const LLUUID& uuid,
												 U32 local_id,
												 const LLHost& sender)
{
	LL_FAST_TIMER(FTM_CREATE_OBJECT);

	LLUUID fullid;
	if (uuid.isNull())
	{
		fullid.generate();
	}
	else if (mDeadObjects.count(uuid))
	{
		LL_DEBUGS("ViewerObject") << "Attempt to re-create a dead object for: "
								  << uuid << ". Skipping." << LL_ENDL;
		return NULL;
	}
	else
	{
		fullid = uuid;
	}

	LLViewerObject* objectp = LLViewerObject::createObject(fullid, pcode,
														   regionp);
	if (!objectp)
	{
		LL_DEBUGS("ViewerObject") << "Could not create object of type "
								  << LLPrimitive::pCodeToString(pcode)
								  << " id:" << fullid << LL_ENDL;
		return NULL;
	}
	if (regionp)
	{
		regionp->addToCreatedList(local_id);
	}

	mUUIDObjectMap[fullid] = objectp;

	if (objectp->isAvatar())
	{
		LLVOAvatar* avatarp = objectp->asAvatar();
		if (avatarp)
		{
			mUUIDAvatarMap[fullid] = avatarp;
		}
	}

	setUUIDAndLocal(fullid, local_id, gMessageSystemp->getSenderIP(),
					gMessageSystemp->getSenderPort());

	mObjects.push_back(objectp);

	if (gAutomationp && objectp->isAvatar())
	{
		gAutomationp->onAvatarRezzing(fullid);
	}

	updateActive(objectp);

	return objectp;
}

LLViewerObject* LLViewerObjectList::replaceObject(const LLUUID& id,
												  LLPCode pcode,
												  LLViewerRegion* regionp)
{
	LLViewerObject* old_instance = findObject(id);
	if (old_instance)
	{
#if 0
		cleanupReferences(old_instance);
#endif
		old_instance->markDead();

		return createObject(pcode, regionp, id, old_instance->getLocalID(),
							LLHost());
	}
	return NULL;
}

#if LL_DEBUG && 0
S32 LLViewerObjectList::findReferences(LLDrawable* drawablep) const
{
	LLViewerObject* objectp;
	S32 num_refs = 0;

	for (S32 i = 0, count = mObjects.size(); i < count; ++i)
	{
		objectp = mObjects[i];
		if (objectp && objectp->mDrawable.notNull())
		{
			num_refs += objectp->mDrawable->findReferences(drawablep);
		}
	}
	return num_refs;
}
#endif

void LLViewerObjectList::orphanize(LLViewerObject* childp, U32 parent_id,
								   U32 ip, U32 port)
{
	LL_DEBUGS("Orphans") << "Orphaning object " << childp->getID()
						 << " with parent " << parent_id << llendl;

	// We are an orphan, flag things appropriately.
	childp->mOrphaned = true;
	if (childp->mDrawable.notNull())
	{
		bool make_invisible = true;
		LLViewerObject* parentp = (LLViewerObject*)childp->getParent();
		if (parentp)
		{
			if (parentp->getRegion() != childp->getRegion())
			{
				// This is probably an object flying across a region boundary,
				// the object probably ISN'T being reparented, but just got an
				// object update out of order (child update before parent).
				make_invisible = false;
			}
		}

		if (make_invisible)
		{
			// Make sure that this object becomes invisible if it is an orphan.
			childp->mDrawable->setState(LLDrawable::FORCE_INVISIBLE);
		}
	}

	// Unknown parent, add to orpaned child list
	U64 parent_info = getIndex(parent_id, ip, port);

	if (std::find(mOrphanParents.begin(), mOrphanParents.end(),
				  parent_info) == mOrphanParents.end())
	{
		mOrphanParents.push_back(parent_info);
	}

	LLViewerObjectList::OrphanInfo oi(parent_info, childp->mID);
	if (std::find(mOrphanChildren.begin(), mOrphanChildren.end(), oi) ==
			mOrphanChildren.end())
	{
		mOrphanChildren.push_back(oi);
		++mNumOrphans;
	}
}

void LLViewerObjectList::findOrphans(LLViewerObject* objectp, U32 ip, U32 port)
{
	LL_TRACY_TIMER(TRC_OBJ_FIND_ORPHANS);

	if (!objectp)
	{
		return;
	}

	if (objectp->isDead())
	{
		llwarns << "Trying to find orphans for dead obj " << objectp->mID
				<< ":" << objectp->getPCodeString() << llendl;
		return;
	}

	// Search object cache to get orphans
	if (objectp->getRegion())
	{
		objectp->getRegion()->findOrphans(objectp->getLocalID());
	}

	// See if we are a parent of an orphan.
	// Note: this code is fairly inefficient but it should happen very rarely.
	// It can be sped up if this is somehow a performance issue...
	if (mOrphanParents.empty())
	{
		// No known orphan parents
		return;
	}
	if (std::find(mOrphanParents.begin(), mOrphanParents.end(),
				  getIndex(objectp->mLocalID, ip, port)) == mOrphanParents.end())
	{
		// Did not find objectp in OrphanParent list
		return;
	}

	U64 parent_info = getIndex(objectp->mLocalID, ip, port);
	bool orphans_found = false;

	// Iterate through the orphan list, and set parents of matching children.
	for (std::vector<OrphanInfo>::iterator iter = mOrphanChildren.begin();
		 iter != mOrphanChildren.end(); )
	{
		if (iter->mParentInfo != parent_info)
		{
			++iter;
			continue;
		}
		LLViewerObject* childp = findObject(iter->mChildInfo);
		if (childp)
		{
			if (childp == objectp)
			{
				llwarns << objectp->mID << " has self as parent, skipping !"
						<< llendl;
				++iter;
				continue;
			}

			LL_DEBUGS("Orphans") << "Reunited parent " << objectp->mID
								 << " with child " << childp->mID
								 << " - Global position: "
								 << objectp->getPositionGlobal()
								 << " - Position from agent: "
								 << objectp->getPositionAgent();
			addDebugBeacon(objectp->getPositionAgent(), "");
			LL_CONT << LL_ENDL;

			gPipeline.markMoved(objectp->mDrawable);
			objectp->setChanged(LLXform::MOVED | LLXform::SILHOUETTE);

			// Flag the object as no longer orphaned
			childp->mOrphaned = false;
			if (childp->mDrawable.notNull())
			{
				// Make the drawable visible again and set the drawable parent
 				childp->mDrawable->clearState(LLDrawable::FORCE_INVISIBLE);
				childp->setDrawableParent(objectp->mDrawable);
				gPipeline.markRebuild(childp->mDrawable);
			}

			// Make certain particles, icon and HUD aren't hidden
			childp->hideExtraDisplayItems(false);

			objectp->addChild(childp);
			orphans_found = true;
			++iter;
		}
		else
		{
			llinfos << "Missing orphan child, removing from list" << llendl;
			iter = mOrphanChildren.erase(iter);
		}
	}

	// Remove orphan parent and children from lists now that they have been
	// found
	{
		std::vector<U64>::iterator iter = std::find(mOrphanParents.begin(),
													mOrphanParents.end(),
													parent_info);
		if (iter != mOrphanParents.end())
		{
			mOrphanParents.erase(iter);
		}
	}


	for (std::vector<OrphanInfo>::iterator iter = mOrphanChildren.begin();
		 iter != mOrphanChildren.end(); )
	{
		if (iter->mParentInfo == parent_info)
		{
			iter = mOrphanChildren.erase(iter);
			--mNumOrphans;
		}
		else
		{
			++iter;
		}
	}

	if (orphans_found && objectp->isSelected())
	{
		LLSelectNode* nodep = gSelectMgr.getSelection()->findNode(objectp);
		if (nodep && !nodep->mIndividualSelection)
		{
			// Rebuild selection with orphans
			gSelectMgr.deselectObjectAndFamily(objectp);
			gSelectMgr.selectObjectAndFamily(objectp);
		}
	}
}

//static
void LLViewerObjectList::registerKilledAttachment(const LLUUID& id)
{
	U64 handle = 0;
	LLViewerRegion* regionp = gAgent.getRegion();
	if (regionp)
	{
		handle = regionp->getHandle();
	}
	if (sKilledAttachmentsStamp != handle)
	{
		sKilledAttachmentsStamp = handle;
		sKilledAttachments.clear();
	}
	sKilledAttachments.emplace(id);
}

/*****************************************************************************/
// Methods that used to be in llglsandbox.cpp

void LLViewerObjectList::renderObjectBeacons()
{
	if (mDebugBeacons.empty())
	{
		return;
	}

	LLGLSUIDefault gls_ui;

	gUIProgram.bind();

	LLTexUnit* unit0 = gGL.getTexUnit(0);

	{
		unit0->unbind(LLTexUnit::TT_TEXTURE);

		S32 last_line_width = -1;
		for (std::vector<LLDebugBeacon>::iterator iter = mDebugBeacons.begin(),
												  end = mDebugBeacons.end();
			 iter != end; ++iter)
		{
			const LLDebugBeacon& debug_beacon = *iter;

			LLColor4 color = debug_beacon.mColor;
			color.mV[3] *= 0.25f;
			if (gUsePBRShaders)
			{
				color = linearColor4(color);
			}

			S32 line_width = debug_beacon.mLineWidth;
			if (line_width != last_line_width)
			{
				gGL.flush();
				gGL.lineWidth((F32)line_width);
				last_line_width = line_width;
			}

			const LLVector3& thisline = debug_beacon.mPositionAgent;
			const F32& x = thisline.mV[VX];
			const F32& y = thisline.mV[VY];
			const F32& z = thisline.mV[VZ];

			gGL.begin(LLRender::LINES);
			gGL.color4fv(color.mV);
			gGL.vertex3f(x, y, z - 50.f);
			gGL.vertex3f(x, y, z + 50.f);
			gGL.vertex3f(x - 2.f, y, z);
			gGL.vertex3f(x + 2.f, y, z);
			gGL.vertex3f(x, y - 2.f, z);
			gGL.vertex3f(x, y + 2.f, z);

			gl_draw_3d_line_cube(0.1f, thisline);

			gGL.end();
		}
	}

	{
		unit0->unbind(LLTexUnit::TT_TEXTURE);
		LLGLDepthTest gls_depth(GL_TRUE);

		S32 last_line_width = -1;
		for (std::vector<LLDebugBeacon>::iterator iter = mDebugBeacons.begin(),
												  end = mDebugBeacons.end();
			 iter != end; ++iter)
		{
			const LLDebugBeacon& debug_beacon = *iter;

			S32 line_width = debug_beacon.mLineWidth;
			if (line_width != last_line_width)
			{
				gGL.flush();
				gGL.lineWidth((F32)line_width);
				last_line_width = line_width;
			}

			const LLVector3& thisline = debug_beacon.mPositionAgent;

			gGL.begin(LLRender::LINES);

			LLColor4 color = debug_beacon.mColor;
			if (gUsePBRShaders)
			{
				color = linearColor4(color);
			}
			gGL.color4fv(color.mV);

			gl_draw_3d_cross_lines(thisline, 0.5f, 0.5f, 0.5f);

			gl_draw_3d_line_cube(0.1f, thisline);

			gGL.end();
		}

		gGL.flush();
		gGL.lineWidth(1.f);

		LLHUDText* hudtp;
		for (std::vector<LLDebugBeacon>::iterator iter = mDebugBeacons.begin(),
												  end = mDebugBeacons.end();
			 iter != end; ++iter)
		{
			LLDebugBeacon& debug_beacon = *iter;
			if (debug_beacon.mString.empty())
			{
				continue;
			}
			hudtp = (LLHUDText*)LLHUDObject::addHUDObject(LLHUDObject::LL_HUD_TEXT);

			hudtp->setZCompare(false);
			LLColor4 color;
			color = debug_beacon.mTextColor;
			color.mV[3] *= 1.f;

			hudtp->setString(utf8str_to_wstring(debug_beacon.mString));
			hudtp->setColor(color);
			hudtp->setPositionAgent(debug_beacon.mPositionAgent);
			debug_beacon.mHUDObject = hudtp;
		}
	}

	stop_glerror();
}

////////////////////////////////////////////////////////////////////////////

LLViewerObjectList::OrphanInfo::OrphanInfo()
:	mParentInfo(0)
{
}

LLViewerObjectList::OrphanInfo::OrphanInfo(U64 parent_info,
										   const LLUUID& child_info)
:	mParentInfo(parent_info),
	mChildInfo(child_info)
{
}

bool LLViewerObjectList::OrphanInfo::operator==(const OrphanInfo& rhs) const
{
	return mParentInfo == rhs.mParentInfo && mChildInfo == rhs.mChildInfo;
}

bool LLViewerObjectList::OrphanInfo::operator!=(const OrphanInfo& rhs) const
{
	return mParentInfo != rhs.mParentInfo || mChildInfo != rhs.mChildInfo;
}

////////////////////////////////////////////////////////////////////////////

LLDebugBeacon::~LLDebugBeacon()
{
	if (mHUDObject.notNull())
	{
		mHUDObject->markDead();
	}
}
