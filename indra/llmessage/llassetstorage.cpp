/**
 * @file llassetstorage.cpp
 * @brief Implementation of the base asset storage system.
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

#include "linden_common.h"

#include <sys/types.h>
#include <sys/stat.h>

#include "llassetstorage.h"

#include "lldbstrings.h"
#include "lldir.h"
#include "llfilesystem.h"
#include "llframetimer.h"
#include "llmath.h"
#include "llsd.h"
#include "llstring.h"
#include "lltransfertargetvfile.h"
#include "llxfermanager.h"
#include "llmessage.h"

LLAssetStorage* gAssetStoragep = NULL;

const LLUUID CATEGORIZE_LOST_AND_FOUND_ID(std::string("00000000-0000-0000-0000-000000000010"));

constexpr U64 TOXIC_ASSET_LIFETIME = 120 * 1000000;	// In microseconds

//-----------------------------------------------------------------------------
// LLAssetInfo class
//-----------------------------------------------------------------------------

LLAssetInfo::LLAssetInfo()
:	mDescription(),
	mName(),
	mUuid(),
	mCreatorID(),
	mType(LLAssetType::AT_NONE)
{
}

LLAssetInfo::LLAssetInfo(const LLUUID& object_id, const LLUUID& creator_id,
						  LLAssetType::EType type, const char* name,
						  const char* desc)
:	mUuid(object_id),
	mCreatorID(creator_id),
	mType(type)
{
	setName(name);
	setDescription(desc);
}

LLAssetInfo::LLAssetInfo(const LLNameValue& nv)
{
	setFromNameValue(nv);
}

// Make sure the name is short enough, and strip all pipes since they are
// reserved characters in our inventory tracking system.
void LLAssetInfo::setName(const std::string& name)
{
	if (!name.empty())
	{
		mName.assign(name, 0, llmin((U32)name.size(),
									(U32)DB_INV_ITEM_NAME_STR_LEN));
		mName.erase(std::remove(mName.begin(), mName.end(), '|'), mName.end());
	}
}

// Make sure the name is short enough, and strip all pipes since they are
// reserved characters in our inventory tracking system.
void LLAssetInfo::setDescription(const std::string& desc)
{
	if (!desc.empty())
	{
		mDescription.assign(desc, 0, llmin((U32)desc.size(),
										   (U32)DB_INV_ITEM_DESC_STR_LEN));
		mDescription.erase(std::remove(mDescription.begin(),
									   mDescription.end(), '|'),
							mDescription.end());
	}
}

// Assets (aka potential inventory items) can be applied to an object in the
// world. We'll store that as a string name value pair where the name encodes
// part of asset info, and the value the rest. LLAssetInfo objects will be
// responsible for parsing the meaning out froman LLNameValue object. See the
// inventory design docs for details. Briefly:
//   name=<inv_type>|<uuid>
//   value=<creatorid>|<name>|<description>|
void LLAssetInfo::setFromNameValue(const LLNameValue& nv)
{
	std::string str;
	std::string buf;
	std::string::size_type pos1;
	std::string::size_type pos2;

	// convert the name to useful information
	str.assign(nv.mName);
	pos1 = str.find('|');
	buf.assign(str, 0, pos1++);
	mType = LLAssetType::lookup(buf);
	buf.assign(str, pos1, std::string::npos);
	mUuid.set(buf);

	// convert the value to useful information
	str.assign(nv.getAsset());
	pos1 = str.find('|');
	buf.assign(str, 0, pos1++);
	mCreatorID.set(buf);
	pos2 = str.find('|', pos1);
	buf.assign(str, pos1, (pos2++) - pos1);
	setName(buf);
	buf.assign(str, pos2, std::string::npos);
	setDescription(buf);
	llinfos << "uuid: " << mUuid << " - creator: " << mCreatorID << llendl;
}

//-----------------------------------------------------------------------------
// LLBaseDownloadRequest class
//-----------------------------------------------------------------------------

LLBaseDownloadRequest::LLBaseDownloadRequest(const LLUUID& uuid,
											 const LLAssetType::EType type)
:	mUUID(uuid),
	mType(type),
	mDownCallback(NULL),
	mUserData(NULL),
	mHost(),
	mIsTemp(false),
	mIsPriority(false),
	mDataSentInFirstPacket(false),
	mDataIsInCache(false)
{
	// Need to guarantee that this time is up to date, we may be creating a
	// circuit even though we haven't been running a message system loop.
	mTime = LLMessageSystem::getMessageTimeSeconds(true);
}

//virtual
LLBaseDownloadRequest* LLBaseDownloadRequest::getCopy()
{
	return new LLBaseDownloadRequest(*this);
}

//-----------------------------------------------------------------------------
// LLAssetRequest class
//-----------------------------------------------------------------------------

LLAssetRequest::LLAssetRequest(const LLUUID& uuid,
							   const LLAssetType::EType type)
:	LLBaseDownloadRequest(uuid, type),
	mUpCallback(NULL),
	mInfoCallback(NULL),
	mIsLocal(false),
	mTimeout(LL_ASSET_STORAGE_TIMEOUT)
{
}

//virtual
LLSD LLAssetRequest::getTerseDetails() const
{
	LLSD sd;
	sd["asset_id"] = getUUID();
	sd["type_long"] = LLAssetType::lookupHumanReadable(getType());
	sd["type"] = LLAssetType::lookup(getType());
	sd["time"] = mTime;
	time_t timestamp = (time_t)mTime;
	std::ostringstream time_string;
	time_string << ctime(&timestamp);
	sd["time_string"] = time_string.str();
	return sd;
}

//virtual
LLSD LLAssetRequest::getFullDetails() const
{
	LLSD sd = getTerseDetails();
	sd["host"] = mHost.getIPandPort();
	sd["requesting_agent"] = mRequestingAgentID;
	sd["is_temp"] = mIsTemp;
	sd["is_local"] = mIsLocal;
	sd["is_priority"] = mIsPriority;
	sd["data_send_in_first_packet"] = mDataSentInFirstPacket;
	// Note: cannot change this (easily) for "data_is_in_cache" since it is
	// consumed by server...
	sd["data_is_in_vfs"] = mDataIsInCache;

	return sd;
}

//virtual
LLBaseDownloadRequest* LLAssetRequest::getCopy()
{
	return new LLAssetRequest(*this);
}

//-----------------------------------------------------------------------------
// LLInvItemRequest class
//-----------------------------------------------------------------------------

LLInvItemRequest::LLInvItemRequest(const LLUUID& uuid,
								   const LLAssetType::EType type)
:	LLBaseDownloadRequest(uuid, type)
{
}

//virtual
LLBaseDownloadRequest* LLInvItemRequest::getCopy()
{
	return new LLInvItemRequest(*this);
}

//-----------------------------------------------------------------------------
// LLEstateAssetRequest class
//-----------------------------------------------------------------------------

LLEstateAssetRequest::LLEstateAssetRequest(const LLUUID& uuid,
										   const LLAssetType::EType atype,
										   EstateAssetType etype)
:	LLBaseDownloadRequest(uuid, atype),
	mEstateAssetType(etype)
{
}

LLBaseDownloadRequest* LLEstateAssetRequest::getCopy()
{
	return new LLEstateAssetRequest(*this);
}

//-----------------------------------------------------------------------------
// LLAssetStorage class
//-----------------------------------------------------------------------------

// Since many of these functions are called by the messaging and xfer systems,
// they are declared as static and are passed a "this" handle it's a C/C++
// mish-mash !

// *TODO: permissions on modifications: maybe do not allow at all ?
// *TODO: verify that failures get propogated down
// *TODO: rework tempfile handling ?

LLAssetStorage::LLAssetStorage(LLMessageSystem* msg, LLXferManager* xfer,
							   const LLHost& upstream_host)
{
	init(msg, xfer, upstream_host);
}

LLAssetStorage::LLAssetStorage(LLMessageSystem* msg, LLXferManager* xfer)
{
	init(msg, xfer, LLHost());
}

void LLAssetStorage::init(LLMessageSystem* msg, LLXferManager* xfer,
						  const LLHost& upstream_host)
{
	mShutDown = false;
	mMessageSys = msg;
	mXferManager = xfer;

	setUpstream(upstream_host);
	msg->setHandlerFuncFast(_PREHASH_AssetUploadComplete,
							processUploadComplete, (void **)this);
}

LLAssetStorage::~LLAssetStorage()
{
	mShutDown = true;

	cleanupRequests(true, LL_ERR_CIRCUIT_GONE);

	if (gMessageSystemp)
	{
		// Unregister our callbacks with the message system. Warning: this
		// would not work if there is more than one asset storage !
		gMessageSystemp->setHandlerFuncFast(_PREHASH_AssetUploadComplete, NULL,
											NULL);
	}

	// Clear the toxic asset map
	mToxicAssetMap.clear();
}

void LLAssetStorage::setUpstream(const LLHost &upstream_host)
{
	LL_DEBUGS("AppInit") << "AssetStorage: Setting upstream provider to "
						 << upstream_host << LL_ENDL;

	mUpstreamHost = upstream_host;
}

//virtual
void LLAssetStorage::checkForTimeouts()
{
	cleanupRequests(false, LL_ERR_TCP_TIMEOUT);
}

void LLAssetStorage::cleanupRequests(bool all, S32 error)
{
	F64 mt_secs = LLMessageSystem::getMessageTimeSeconds();

	request_list_t timed_out;
	for (S32 rt = 0; rt < RT_COUNT; ++rt)
	{
		request_list_t* requests = getRequestList((ERequestType)rt);
		for (request_list_t::iterator iter = requests->begin(),
									  end = requests->end();
			 iter != end; )
		{
			request_list_t::iterator curiter = iter++;
			LLAssetRequest* tmp = *curiter;
			// If all is true, we want to clean up everything otherwise just
			// check for timed out requests EXCEPT for upload timeouts
			if (all ||
				(rt == RT_DOWNLOAD &&
				 LL_ASSET_STORAGE_TIMEOUT < mt_secs - tmp->mTime))
			{
				llwarns << "Asset " << getRequestName((ERequestType)rt)
						<< " request " << (all ? "aborted" : "timed out")
						<< " for " << tmp->getUUID() << "."
						<< LLAssetType::lookup(tmp->getType())
						<< llendl;

				timed_out.push_front(tmp);
				requests->erase(curiter);
			}
		}
	}

	LLAssetInfo	info;
	for (request_list_t::iterator iter = timed_out.begin(),
								  end = timed_out.end();
		 iter != end; ++iter)
	{
		LLAssetRequest* tmp = *iter;
		if (tmp->mUpCallback)
		{
			tmp->mUpCallback(tmp->getUUID(), tmp->mUserData, error,
							 LLExtStat::NONE);
		}
		if (tmp->mDownCallback)
		{
			tmp->mDownCallback(tmp->getUUID(), tmp->getType(), tmp->mUserData,
							   error, LLExtStat::NONE);
		}
		if (tmp->mInfoCallback)
		{
			tmp->mInfoCallback(&info, tmp->mUserData, error);
		}
		delete tmp;
	}
}

bool LLAssetStorage::hasLocalAsset(const LLUUID& uuid, LLAssetType::EType)
{
	return LLFileSystem::getExists(uuid);
}

// UUID is passed by value to avoid side effects, please do not re-add &
void LLAssetStorage::getAssetData(const LLUUID uuid, LLAssetType::EType type,
								  LLGetAssetCallback callback,
								  void* user_data, bool is_priority)
{
	LL_DEBUGS("AssetStorage") << "Called for asset: " << uuid << "."
							  << LLAssetType::lookup(type) << LL_ENDL;

	if (user_data)
	{
		// The *user_data should not be passed without a callback to clean it
		// up.
		llassert(callback != NULL);
	}

	if (mShutDown)
	{
		LL_DEBUGS("AssetStorage") << "ASSET_TRACE cancelled (shutting down)"
								  << LL_ENDL;
		if (callback)
		{
			callback(uuid, type, user_data, LL_ERR_ASSET_REQUEST_FAILED,
					 LLExtStat::NONE);
		}
		return;
	}

	if (uuid.isNull())
	{
		// Special case early out for NULL uuid and for shutting down
		if (callback)
		{
			callback(uuid, type, user_data,
					 LL_ERR_ASSET_REQUEST_NOT_IN_DATABASE,
					 LLExtStat::NULL_UUID);
		}
		return;
	}

	LLFileSystem file(uuid);

	if (file.getSize() > 0)
	{
		// We have already got the file. Theoretically, partial files without a
		// pending request should not happen unless there is a weird error.
		if (callback)
		{
			callback(uuid, type, user_data, LL_ERR_NOERR,
					 LLExtStat::CACHE_CACHED);
		}
		return;
	}

	if (file.exists())
	{
		llwarns << "Asset cache file " << file.getName()
				<< " found with zero size, removing." << llendl;
		file.remove();
	}

	// Check to see if there is a pending download of this uuid already
	bool duplicate = false;
	for (request_list_t::iterator iter = mPendingDownloads.begin(),
								  end = mPendingDownloads.end();
		 iter != end; ++iter)
	{
		LLAssetRequest* tmp = *iter;
		if (type == tmp->getType() && uuid == tmp->getUUID())
		{
			if (callback == tmp->mDownCallback && user_data == tmp->mUserData)
			{
				// This is a duplicate from the same subsystem - throw it away
				llwarns << "Discarding duplicate request for asset " << uuid
						<< "." << LLAssetType::lookup(type) << llendl;
				return;
			}

			// This is a duplicate request queue the request, but do not
			// actually ask for it again
			duplicate = true;
		}
	}
	if (duplicate)
	{
		llinfos_once << "Adding additional non-duplicate request for asset "
					 << uuid << "." << LLAssetType::lookup(type) << llendl;
	}

	queueDataRequest(uuid, type, callback, user_data, duplicate, is_priority);
}

// Finds and calls back ALL pending requests for passed UUID
//static
void LLAssetStorage::removeAndCallbackPendingDownloads(const LLUUID& file_id,
													   LLAssetType::EType file_type,
                                                       const LLUUID& callback_id,
													   LLAssetType::EType callback_type,
                                                       S32 result_code,
													   LLExtStat ext_status)
{
	request_list_t requests;
	for (request_list_t::iterator iter = gAssetStoragep->mPendingDownloads.begin();
		 iter != gAssetStoragep->mPendingDownloads.end();  )
	{
		request_list_t::iterator curiter = iter++;
		LLAssetRequest* tmp = *curiter;
		if (tmp && tmp->getUUID() == file_id && tmp->getType() == file_type)
		{
			if (tmp->mDownCallback)
			{
				requests.push_front(tmp);
			}
			iter = gAssetStoragep->mPendingDownloads.erase(curiter);
		}
	}
	for (request_list_t::iterator iter = requests.begin(),
								  end = requests.end();
		 iter != end; ++iter)
	{
		LLAssetRequest* tmp = *iter;
		if (tmp->mDownCallback)
		{
			tmp->mDownCallback(callback_id, callback_type, tmp->mUserData,
							   result_code, ext_status);
		}
		delete tmp;
 	}
}

void LLAssetStorage::downloadCompleteCallback(S32 result,
											  const LLUUID& file_id,
											  LLAssetType::EType file_type,
											  LLBaseDownloadRequest* user_data,
											  LLExtStat ext_status)
{
	LL_DEBUGS("AssetStorage") << "Download complete callback for " << file_id
							  << "." << LLAssetType::lookup(file_type)
							  << LL_ENDL;
	LLAssetRequest* req = (LLAssetRequest*)user_data;
	if (!req)
	{
		llwarns << "Call done without a valid request." << llendl;
		return;
	}
	if (!gAssetStoragep)
	{
		llwarns << "Call done without any asset system, aborting !" << llendl;
		return;
	}

	LLUUID callback_id;
	LLAssetType::EType callback_type;
	// Inefficient since we're doing a find through a list that may have
	// thousands of elements. This is due for refactoring; we will probably
	// change mPendingDownloads into a set.
	request_list_t::iterator pending_begin, pending_end, iter;
	pending_begin = gAssetStoragep->mPendingDownloads.begin();
	pending_end = gAssetStoragep->mPendingDownloads.end();
	iter = std::find(pending_begin, pending_end, req);
	if (iter != pending_end)
	{
		callback_id = file_id;
		callback_type = file_type;
	}
	else
	{
		// Either has already been deleted by cleanupRequests() or it is a
		// transfer.
		callback_id = req->getUUID();
		callback_type = req->getType();
	}

	if (result == LL_ERR_NOERR)
	{
		LLFileSystem vfile(callback_id);
		// We might have gotten a zero-size file
		if (vfile.getSize() <= 0)
		{
			llwarns << "Non-existent or zero-size asset " << callback_id
					<< llendl;

			result = LL_ERR_ASSET_REQUEST_NOT_IN_DATABASE;
			vfile.remove();
		}
	}

	removeAndCallbackPendingDownloads(file_id, file_type, callback_id,
									  callback_type, result, ext_status);
}

void LLAssetStorage::getEstateAsset(const LLHost& object_sim,
									const LLUUID& agent_id,
									const LLUUID& session_id,
									const LLUUID& asset_id,
									LLAssetType::EType atype,
									EstateAssetType etype,
									LLGetAssetCallback callback,
									void* user_data,
									bool is_priority)
{
	LL_DEBUGS("AssetStorage") << "Asset: " << asset_id << "."
							  << LLAssetType::lookup(atype)
							  << " - estate type: " << etype << LL_ENDL;

	//
	// Probably will get rid of this early out?
	//
	if (asset_id.isNull())
	{
		// Special case early out for NULL uuid
		if (callback)
		{
			callback(asset_id, atype, user_data,
					 LL_ERR_ASSET_REQUEST_NOT_IN_DATABASE,
					 LLExtStat::NULL_UUID);
		}
		return;
	}

	LLFileSystem file(asset_id);

	if (file.getSize() > 0)
	{
		// We have already got the file. Theoretically, partial files without a
		// pending request should not happen unless there is a weird error.
		if (callback)
		{
			callback(asset_id, atype, user_data, LL_ERR_NOERR,
					 LLExtStat::CACHE_CACHED);
		}
		return;
	}

	if (file.exists())
	{
		llwarns << "Asset cache file " << file.getName()
				<< " found with zero size, removing." << llendl;
		file.remove();
	}

	// See whether we should talk to the object's originating sim, or the
	// upstream provider.
	LLHost source_host;
	if (object_sim.isOk())
	{
		source_host = object_sim;
	}
	else
	{
		source_host = mUpstreamHost;
	}
	if (source_host.isOk())
	{
		// Stash the callback info so we can find it after we get the response
		// message
		LLEstateAssetRequest req(asset_id, atype, etype);
		req.mDownCallback = callback;
		req.mUserData = user_data;
		req.mIsPriority = is_priority;

		// Send request message to our upstream data provider. Create a new
		// asset transfer.
		LLTransferSourceParamsEstate spe;
		spe.setAgentSession(agent_id, session_id);
		spe.setEstateAssetType(etype);

		// Set our destination file, and the completion callback.
		LLTransferTargetParamsVFile tpvf;
		tpvf.setAsset(asset_id, atype);
		tpvf.setCallback(downloadEstateAssetCompleteCallback, req);

		LL_DEBUGS("AssetStorage") << "Starting transfer for " << asset_id
								  << LL_ENDL;
		LLTransferTargetChannel* ttcp =
			gTransferManager.getTargetChannel(source_host, LLTCT_ASSET);
		ttcp->requestTransfer(spe, tpvf, 100.f + (is_priority ? 1.f : 0.f));
	}
	else
	{
		// Uh-oh, we should not have gotten here
		llwarns << "Attempt to move asset data request upstream without valid upstream provider"
				<< llendl;
		if (callback)
		{
			callback(asset_id, atype, user_data, LL_ERR_CIRCUIT_GONE,
					 LLExtStat::NO_UPSTREAM);
		}
	}
}

void LLAssetStorage::downloadEstateAssetCompleteCallback(S32 result,
														 const LLUUID& file_id,
														 LLAssetType::EType file_type,
														 LLBaseDownloadRequest* user_data,
														 LLExtStat ext_status)
{
	LLEstateAssetRequest* req = (LLEstateAssetRequest*)user_data;
	if (!req)
	{
		llwarns << "Call done without a valid request." << llendl;
		return;
	}
	if (!gAssetStoragep)
	{
		llwarns << "Call done without any asset system, aborting." << llendl;
		return;
	}

	req->setUUID(file_id);
	req->setType(file_type);
	if (LL_ERR_NOERR == result)
	{
		// We might have gotten a zero-size file
		LLFileSystem vfile(req->getUUID());
		if (vfile.getSize() <= 0)
		{
			llwarns << "Non-existent or zero-size asset found !" << llendl;
			result = LL_ERR_ASSET_REQUEST_NOT_IN_DATABASE;
			vfile.remove();
		}
	}

	req->mDownCallback(req->getUUID(), req->getAType(), req->mUserData, result,
					   ext_status);
}

void LLAssetStorage::getInvItemAsset(const LLHost& object_sim,
									 const LLUUID& agent_id,
									 const LLUUID& session_id,
									 const LLUUID& owner_id,
									 const LLUUID& task_id,
									 const LLUUID& item_id,
									 const LLUUID& asset_id,
									 LLAssetType::EType atype,
									 LLGetAssetCallback callback,
									 void* user_data,
									 bool is_priority)
{
	LL_DEBUGS("AssetStorage") << "Asset: " << asset_id << "."
							  << LLAssetType::lookup(atype) << LL_ENDL;
#if 0
	// Probably will get rid of this early out?
	if (asset_id.isNull())
	{
		// Special case early out for NULL uuid
		if (callback)
		{
			callback(asset_id, atype, user_data,
					 LL_ERR_ASSET_REQUEST_NOT_IN_DATABASE);
		}
		return;
	}
#endif

	bool exists = false;
	U32 size = 0;

	if (asset_id.notNull())
	{
		LLFileSystem file(asset_id);
		exists = file.exists();
		size = file.getSize();
		if (exists && !size)
		{
			llwarns << "Asset cache file " << file.getName()
					<< " found with zero size, removing." << llendl;
			file.remove();
		}
	}

	if (size > 0)
	{
		// We have already got the file. Theoretically, partial files without a
		// pending request should not happen unless there is a weird error.
		if (callback)
		{
			callback(asset_id, atype, user_data, LL_ERR_NOERR,
					 LLExtStat::CACHE_CACHED);
		}
		return;
	}

	// See whether we should talk to the object's originating sim, or the
	// upstream provider.
	LLHost source_host;
	if (object_sim.isOk())
	{
		source_host = object_sim;
	}
	else
	{
		source_host = mUpstreamHost;
	}
	if (source_host.isOk())
	{
		// Stash the callback info so we can find it after we get the response
		// message
		LLInvItemRequest req(asset_id, atype);
		req.mDownCallback = callback;
		req.mUserData = user_data;
		req.mIsPriority = is_priority;

		// Send request message to our upstream data provider. Create a new
		// asset transfer.
		LLTransferSourceParamsInvItem spi;
		spi.setAgentSession(agent_id, session_id);
		spi.setInvItem(owner_id, task_id, item_id);
		spi.setAsset(asset_id, atype);

		// Set our destination file, and the completion callback.
		LLTransferTargetParamsVFile tpvf;
		tpvf.setAsset(asset_id, atype);
		tpvf.setCallback(downloadInvItemCompleteCallback, req);

		LL_DEBUGS("AssetStorage") << "Starting transfer for inventory asset "
								  << item_id << " - owned by: " << owner_id
								  << " - task id:" << task_id << LL_ENDL;
		LLTransferTargetChannel* ttcp =
			gTransferManager.getTargetChannel(source_host, LLTCT_ASSET);
		ttcp->requestTransfer(spi, tpvf, 100.f + (is_priority ? 1.f : 0.f));
	}
	else
	{
		// Uh-oh, we should not have gotten here
		llwarns << "Attempt to move asset data request upstream without valid upstream provider"
				<< llendl;
		if (callback)
		{
			callback(asset_id, atype, user_data, LL_ERR_CIRCUIT_GONE,
					 LLExtStat::NO_UPSTREAM);
		}
	}
}

void LLAssetStorage::downloadInvItemCompleteCallback(S32 result,
													 const LLUUID& file_id,
													 LLAssetType::EType file_type,
													 LLBaseDownloadRequest* user_data,
													 LLExtStat ext_status)
{
	LLInvItemRequest* req = (LLInvItemRequest*)user_data;
	if (!req)
	{
		llwarns << "Call done without a valid request." << llendl;
		return;
	}
	if (!gAssetStoragep)
	{
		llwarns << "Call done without any asset system, aborting." << llendl;
		return;
	}

	req->setUUID(file_id);
	req->setType(file_type);
	if (LL_ERR_NOERR == result)
	{
		// We might have gotten a zero-size file
		LLFileSystem vfile(req->getUUID());
		if (vfile.getSize() <= 0)
		{
			llwarns << "Non-existent or zero-size asset found !" << llendl;
			result = LL_ERR_ASSET_REQUEST_NOT_IN_DATABASE;
			vfile.remove();
		}
	}

	req->mDownCallback(req->getUUID(), req->getType(), req->mUserData, result,
					   ext_status);
}

///////////////////////////////////////////////////////////////////////////////
// Store routines
///////////////////////////////////////////////////////////////////////////////

// StoreAssetData callback (fixed)
//static
void LLAssetStorage::uploadCompleteCallback(const LLUUID& uuid,
											void* user_data,
											S32 result,
											LLExtStat ext_status)
{
	if (!gAssetStoragep)
	{
		llwarns << "No asset storage !" << llendl;
		return;
	}
	LLAssetRequest* req = (LLAssetRequest*)user_data;

	bool success = true;
	if (result)
	{
		llwarns << "Upload complete callback: " << result << " - "
				<< getErrorString(result)
				<< ". Trying to upload file to upstream provider" << llendl;
		success = false;
	}

	// We are done grabbing the file, tell the client
	gAssetStoragep->mMessageSys->newMessageFast(_PREHASH_AssetUploadComplete);
	gAssetStoragep->mMessageSys->nextBlockFast(_PREHASH_AssetBlock);
	gAssetStoragep->mMessageSys->addUUIDFast(_PREHASH_UUID, uuid);
	gAssetStoragep->mMessageSys->addS8Fast(_PREHASH_Type, req->getType());
	gAssetStoragep->mMessageSys->addBoolFast(_PREHASH_Success, success);
	gAssetStoragep->mMessageSys->sendReliable(req->mHost);

	delete req;
}

void LLAssetStorage::processUploadComplete(LLMessageSystem* msg,
										   void** user_data)
{
	LLAssetStorage* this_ptr = (LLAssetStorage*)user_data;
	LLUUID uuid;
	S8 asset_type_s8;
	LLAssetType::EType asset_type;
	bool success = false;

	msg->getUUIDFast(_PREHASH_AssetBlock, _PREHASH_UUID, uuid);
	msg->getS8Fast(_PREHASH_AssetBlock, _PREHASH_Type, asset_type_s8);
	msg->getBoolFast(_PREHASH_AssetBlock, _PREHASH_Success, success);

	asset_type = (LLAssetType::EType)asset_type_s8;
	this_ptr->callUploadCallbacks(uuid, asset_type, success, LLExtStat::NONE);
}

void LLAssetStorage::callUploadCallbacks(const LLUUID& uuid,
										 LLAssetType::EType asset_type,
										 bool success, LLExtStat ext_status)
{
	// SJB: we process the callbacks in reverse order, I do not know if this is
	// important, but I did not want to mess with it.
	request_list_t requests;
	for (request_list_t::iterator iter = mPendingUploads.begin(),
								  end = mPendingUploads.end();
		 iter != end; )
	{
		request_list_t::iterator curiter = iter++;
		LLAssetRequest* req = *curiter;
		if (req->getUUID() == uuid && req->getType() == asset_type)
		{
			requests.push_front(req);
			mPendingUploads.erase(curiter);
		}
	}
	for (request_list_t::iterator iter = mPendingLocalUploads.begin(),
								  end = mPendingLocalUploads.end();
		 iter != end; )
	{
		request_list_t::iterator curiter = iter++;
		LLAssetRequest* req = *curiter;
		if (req->getUUID() == uuid && req->getType() == asset_type)
		{
			requests.push_front(req);
			mPendingLocalUploads.erase(curiter);
		}
	}
	for (request_list_t::iterator iter = requests.begin(),
								  end = requests.end();
		 iter != end; ++iter)
	{
		LLAssetRequest* req = *iter;
		if (req->mUpCallback)
		{
			req->mUpCallback(uuid, req->mUserData,
							 success ? LL_ERR_NOERR
									 : LL_ERR_ASSET_REQUEST_FAILED,
							 ext_status);
		}
		delete req;
	}
}

LLAssetStorage::request_list_t* LLAssetStorage::getRequestList(LLAssetStorage::ERequestType rt)
{
	switch (rt)
	{
		case RT_DOWNLOAD:
			return &mPendingDownloads;

		case RT_UPLOAD:
			return &mPendingUploads;

		case RT_LOCALUPLOAD:
			return &mPendingLocalUploads;

		default:
			llwarns << "Unable to find request list for request type: " << rt
					<< llendl;
			return NULL;
	}
}

const LLAssetStorage::request_list_t* LLAssetStorage::getRequestList(LLAssetStorage::ERequestType rt) const
{
	switch (rt)
	{
		case RT_DOWNLOAD:
			return &mPendingDownloads;

		case RT_UPLOAD:
			return &mPendingUploads;

		case RT_LOCALUPLOAD:
			return &mPendingLocalUploads;

		default:
			llwarns << "Unable to find request list for request type: " << rt
					<< llendl;
			return NULL;
	}
}

//static
std::string LLAssetStorage::getRequestName(LLAssetStorage::ERequestType rt)
{
	switch (rt)
	{
		case RT_DOWNLOAD:
			return "download";

		case RT_UPLOAD:
			return "upload";

		case RT_LOCALUPLOAD:
			return "localupload";

		default:
			llwarns << "Unable to find request name for request type: " << rt
					<< llendl;
			return "";
	}
}

S32 LLAssetStorage::getNumPending(LLAssetStorage::ERequestType rt) const
{
	const request_list_t* requests = getRequestList(rt);
	return requests ? (S32)requests->size() : -1;
}

S32 LLAssetStorage::getNumPendingDownloads() const
{
	return getNumPending(RT_DOWNLOAD);
}

S32 LLAssetStorage::getNumPendingUploads() const
{
	return getNumPending(RT_UPLOAD);
}

S32 LLAssetStorage::getNumPendingLocalUploads()
{
	return getNumPending(RT_LOCALUPLOAD);
}

LLSD LLAssetStorage::getPendingDetails(LLAssetStorage::ERequestType rt,
									   LLAssetType::EType asset_type,
									   const std::string& detail_prefix) const
{
	const request_list_t* requests = getRequestList(rt);
	LLSD sd;
	sd["requests"] = getPendingDetailsImpl(requests, asset_type,
										   detail_prefix);
	return sd;
}

LLSD LLAssetStorage::getPendingDetailsImpl(const LLAssetStorage::request_list_t* requests,
										   LLAssetType::EType asset_type,
										   const std::string& detail_prefix) const
{
	LLSD details;
	if (requests)
	{
		for (request_list_t::const_iterator it = requests->begin(),
											end = requests->end();
			 it != end; ++it)
		{
			LLAssetRequest* req = *it;
			if (asset_type == LLAssetType::AT_NONE ||
				asset_type == req->getType())
			{
				LLSD row = req->getTerseDetails();

				std::ostringstream detail;
				detail << detail_prefix << "/"
					   << LLAssetType::lookup(req->getType()) << "/"
					   << req->getUUID();
				row["detail"] = LLURI(detail.str());

				details.append(row);
			}
		}
	}
	return details;
}

//static
const LLAssetRequest* LLAssetStorage::findRequest(const LLAssetStorage::request_list_t* requests,
												  LLAssetType::EType asset_type,
												  const LLUUID& asset_id)
{
	if (requests)
	{
		// Search the requests list for the asset.
		for (request_list_t::const_iterator iter = requests->begin(),
											end = requests->end();
			 iter != end; ++iter)
		{
			const LLAssetRequest* req = *iter;
			if (asset_type == req->getType() && asset_id == req->getUUID())
			{
				return req;
			}
		}
	}
	return NULL;
}

//static
LLAssetRequest* LLAssetStorage::findRequest(LLAssetStorage::request_list_t* requests,
											LLAssetType::EType asset_type,
											const LLUUID& asset_id)
{
	if (requests)
	{
		// Search the requests list for the asset.
		for (request_list_t::iterator iter = requests->begin(),
									  end = requests->end();
			 iter != end; ++iter)
		{
			LLAssetRequest* req = *iter;
			if (asset_type == req->getType() && asset_id == req->getUUID())
			{
				return req;
			}
		}
	}
	return NULL;
}

LLSD LLAssetStorage::getPendingRequest(LLAssetStorage::ERequestType rt,
									   LLAssetType::EType asset_type,
									   const LLUUID& asset_id) const
{
	const request_list_t* requests = getRequestList(rt);
	return getPendingRequestImpl(requests, asset_type, asset_id);
}

LLSD LLAssetStorage::getPendingRequestImpl(const LLAssetStorage::request_list_t* requests,
										   LLAssetType::EType asset_type,
										   const LLUUID& asset_id) const
{
	LLSD sd;
	const LLAssetRequest* req = findRequest(requests, asset_type, asset_id);
	if (req)
	{
		sd = req->getFullDetails();
	}
	return sd;
}

bool LLAssetStorage::deletePendingRequest(LLAssetStorage::ERequestType rt,
										  LLAssetType::EType asset_type,
										  const LLUUID& asset_id)
{
	request_list_t* requests = getRequestList(rt);
	if (deletePendingRequestImpl(requests, asset_type, asset_id))
	{
		llinfos << "Asset " << getRequestName(rt) << " request for "
				<< asset_id << "." << LLAssetType::lookup(asset_type)
				<< " removed from pending queue." << llendl;
		return true;
	}
	return false;
}

bool LLAssetStorage::deletePendingRequestImpl(LLAssetStorage::request_list_t* requests,
											  LLAssetType::EType asset_type,
											  const LLUUID& asset_id)
{
	LLAssetRequest* req = findRequest(requests, asset_type, asset_id);
	if (req)
	{
		// Remove the request from this list.
		requests->remove(req);
		S32 error = LL_ERR_TCP_TIMEOUT;
		// Run callbacks.
		if (req->mUpCallback)
		{
			req->mUpCallback(req->getUUID(), req->mUserData, error,
							 LLExtStat::REQUEST_DROPPED);
		}
		if (req->mDownCallback)
		{
			req->mDownCallback(req->getUUID(), req->getType(), req->mUserData,
							   error, LLExtStat::REQUEST_DROPPED);
		}
		if (req->mInfoCallback)
		{
			LLAssetInfo info;
			req->mInfoCallback(&info, req->mUserData, error);
		}
		delete req;
		return true;
	}

	return false;
}

//static
const char* LLAssetStorage::getErrorString(S32 status)
{
	switch (status)
	{
		case LL_ERR_NOERR:
			return "No error";

		case LL_ERR_ASSET_REQUEST_FAILED:
			return "Asset request: failed";

		case LL_ERR_ASSET_REQUEST_NONEXISTENT_FILE:
			return "Asset request: non-existent file";

		case LL_ERR_ASSET_REQUEST_NOT_IN_DATABASE:
			return "Asset request: asset not found in database";

		case LL_ERR_EOF:
			return "End of file";

		case LL_ERR_CANNOT_OPEN_FILE:
			return "Cannot open file";

		case LL_ERR_FILE_NOT_FOUND:
			return "File not found";

		case LL_ERR_TCP_TIMEOUT:
			return "File transfer timeout";

		case LL_ERR_CIRCUIT_GONE:
			return "Circuit gone";

		case LL_ERR_PRICE_MISMATCH:
			return "Viewer and server do not agree on price";

		default:
			return "Unknown status";
	}
}

void LLAssetStorage::getAssetData(const LLUUID uuid, LLAssetType::EType type,
								  void (*callback)(const char*, const LLUUID&,
												   void*, S32, LLExtStat),
								  void* user_data, bool is_priority)
{
	// Check for duplicates here, since we're about to fool the normal
	// duplicate checker
	for (request_list_t::iterator iter = mPendingDownloads.begin(),
								  end = mPendingDownloads.end();
		 iter != end; ++iter)
	{
		LLAssetRequest* tmp = *iter;
		if (type == tmp->getType() && uuid == tmp->getUUID() &&
			legacyGetDataCallback == tmp->mDownCallback &&
			callback == ((LLLegacyAssetRequest*)tmp->mUserData)->mDownCallback &&
			user_data == ((LLLegacyAssetRequest*)tmp->mUserData)->mUserData)
		{
			// this is a duplicate from the same subsystem - throw it away
			llinfos << "Discarding duplicate request for UUID " << uuid
					<< llendl;
			return;
		}
	}

	LLLegacyAssetRequest* legacy = new LLLegacyAssetRequest;
	legacy->mDownCallback = callback;
	legacy->mUserData = user_data;

	getAssetData(uuid, type, legacyGetDataCallback, (void**)legacy,
				 is_priority);
}

//static
void LLAssetStorage::legacyGetDataCallback(const LLUUID& uuid,
										   LLAssetType::EType type,
										   void* user_data, S32 status,
										   LLExtStat ext_status)
{
	if (!gAssetStoragep)
	{
		llwarns << "No asset storage !" << llendl;
		return;
	}

	std::string filename;

	// Check if the asset is marked toxic, and don't load bad stuff
	bool toxic = gAssetStoragep->isAssetToxic(uuid);
	if (!status && !toxic)
	{
		LLFileSystem file(uuid);

		std::string uuid_str;
		uuid.toString(uuid_str);
		filename = llformat("%s.%s",
							gDirUtilp->getExpandedFilename(LL_PATH_CACHE,
														   uuid_str).c_str(),
							LLAssetType::lookup(type));

		LLFILE* fp = LLFile::open(filename, "wb");
		if (fp)
		{
			constexpr S32 BUF_SIZE = 65536;
			U8 copy_buf[BUF_SIZE];
			while (file.read(copy_buf, BUF_SIZE))
			{
				if (fwrite(copy_buf, file.getLastBytesRead(), 1, fp) < 1)
				{
					// Return a bad file error if we cannot write the whole
					// thing
					status = LL_ERR_CANNOT_OPEN_FILE;
				}
			}
			LLFile::close(fp);
		}
		else
		{
			status = LL_ERR_CANNOT_OPEN_FILE;
		}
	}

	LLLegacyAssetRequest* legacy = (LLLegacyAssetRequest*)user_data;
	legacy->mDownCallback(filename.c_str(), uuid, legacy->mUserData, status,
						  ext_status);
	delete legacy;
}

//static
void LLAssetStorage::legacyStoreDataCallback(const LLUUID& uuid,
											 void* user_data,
											 S32 status,
											 LLExtStat ext_status)
{
	LLLegacyAssetRequest* legacy = (LLLegacyAssetRequest *)user_data;
	if (legacy && legacy->mUpCallback)
	{
		legacy->mUpCallback(uuid, legacy->mUserData, status, ext_status);
	}
	delete legacy;
}

// Checks if an asset is in the toxic map. If it is, the entry is updated
bool LLAssetStorage::isAssetToxic(const LLUUID& uuid)
{
	if (uuid.notNull())
	{
		toxic_asset_map_t::iterator iter = mToxicAssetMap.find(uuid);
		if (iter != mToxicAssetMap.end())
		{
			// Found toxic asset
			iter->second = LLFrameTimer::getTotalTime() + TOXIC_ASSET_LIFETIME;
			return true;
		}
	}
	return false;
}

// Cleans the toxic asset list, remove old entries
void LLAssetStorage::flushOldToxicAssets(bool force_it)
{
	// Scan and look for old entries
	U64 now = LLFrameTimer::getTotalTime();
	toxic_asset_map_t::iterator iter = mToxicAssetMap.begin();
	while (iter != mToxicAssetMap.end())
	{
		if (force_it || iter->second < now)
		{
			// Too old, remove it
			mToxicAssetMap.hmap_erase(iter++);
		}
		else
		{
			++iter;
		}
	}
}

// Adds an item to the toxic asset map
void LLAssetStorage::markAssetToxic(const LLUUID& id)
{
	if (id.notNull())
	{
		// Set the value to the current time. Creates a new entry if needed.
		U64 expires = LLFrameTimer::getTotalTime() + TOXIC_ASSET_LIFETIME;
		mToxicAssetMap.emplace(id, expires);
	}
}
