/**
 * @file llassetstorage.h
 * @brief definition of LLAssetStorage class which allows simple
 * up/downloads of uuid,type asets
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

#ifndef LL_LLASSETSTORAGE_H
#define LL_LLASSETSTORAGE_H

#include "llassettype.h"
#include "llextendedstatus.h"
#include "hbfastmap.h"
#include "llhost.h"
#include "llnamevalue.h"
#include "llpreprocessor.h"
#include "llstring.h"
#include "lltimer.h"
#include "lltransfermanager.h"	// For LLTSCode enum
#include "lluuid.h"
#include "llxfer.h"

// Forward declarations
class LLAssetStorage;
class LLSD;
class LLMessageSystem;
class LLXferManager;

// Anything that takes longer than this to download will abort. HTTP Uploads
// also timeout if they take longer than this.
constexpr F32 LL_ASSET_STORAGE_TIMEOUT = 5 * 60.0f;

// Specific error codes
constexpr int LL_ERR_ASSET_REQUEST_FAILED = -1;
//constexpr int LL_ERR_ASSET_REQUEST_INVALID = -2;
constexpr int LL_ERR_ASSET_REQUEST_NONEXISTENT_FILE = -3;
constexpr int LL_ERR_ASSET_REQUEST_NOT_IN_DATABASE = -4;
constexpr int LL_ERR_INSUFFICIENT_PERMISSIONS = -5;
constexpr int LL_ERR_PRICE_MISMATCH = -23018;

class LLAssetInfo
{
protected:
	LOG_CLASS(LLAssetInfo);

public:
	LLAssetInfo();
	LLAssetInfo(const LLUUID& object_id, const LLUUID& creator_id,
				LLAssetType::EType type, const char* name, const char* desc);
	LLAssetInfo(const LLNameValue& nv);

	LL_INLINE const std::string& getName() const		{ return mName; }
	LL_INLINE const std::string& getDescription() const	{ return mDescription; }
	void setName(const std::string& name);
	void setDescription(const std::string& desc);

	// Assets (aka potential inventory items) can be applied to an object in
	// the world. We'll store that as a string name value pair where the name
	// encodes part of asset info, and the value the rest. LLAssetInfo objects
	// will be responsible for parsing the meaning out froman LLNameValue
	// object. See the inventory design docs for details.
	void setFromNameValue(const LLNameValue& nv);

public:
	LLUUID				mUuid;
	LLTransactionID		mTransactionID;
	LLUUID				mCreatorID;
	LLAssetType::EType	mType;

protected:
	std::string			mDescription;
	std::string			mName;
};

class LLBaseDownloadRequest
{
public:
	LLBaseDownloadRequest(const LLUUID& uuid, const LLAssetType::EType at);
	virtual ~LLBaseDownloadRequest() = default;

	LL_INLINE LLUUID getUUID() const					{ return mUUID; }
	LL_INLINE LLAssetType::EType getType() const		{ return mType; }

	LL_INLINE void setUUID(const LLUUID& id)			{ mUUID = id; }
	LL_INLINE void setType(LLAssetType::EType type)		{ mType = type; }

	virtual LLBaseDownloadRequest* getCopy();

	void (*mDownCallback)(const LLUUID&, LLAssetType::EType, void*, S32,
						  LLExtStat);

protected:
	LLUUID				mUUID;
	LLAssetType::EType	mType;

public:
	void*	mUserData;
	LLHost  mHost;
	F64		mTime;				// Message system time
	bool	mIsTemp;
	bool    mIsPriority;
	bool	mDataSentInFirstPacket;
	bool	mDataIsInCache;
};

class LLAssetRequest final : public LLBaseDownloadRequest
{
public:
	LLAssetRequest(const LLUUID& uuid, const LLAssetType::EType at);

	LLBaseDownloadRequest* getCopy() override;

	virtual LLSD getTerseDetails() const;
	virtual LLSD getFullDetails() const;

	LL_INLINE void setTimeout(F64 timeout)				{ mTimeout = timeout; }

	void (*mUpCallback)(const LLUUID&, void*, S32, LLExtStat);
	void (*mInfoCallback)(LLAssetInfo*, void*, S32);

public:
	LLUUID	mRequestingAgentID;	// Only valid for uploads from an agent
	F64		mTimeout;			// Amount of time before timing out.
	bool	mIsLocal;
};

template <class T>
struct ll_asset_request_equal : public std::equal_to<T>
{
	bool operator()(const T& x, const T& y) const
	{
		return x->getType() == y->getType() && x->getUUID() == y->getUUID();
	}
};

class LLInvItemRequest final : public LLBaseDownloadRequest
{
public:
	LLInvItemRequest(const LLUUID& uuid, const LLAssetType::EType at);

	LLBaseDownloadRequest* getCopy() override;
};

class LLEstateAssetRequest final : public LLBaseDownloadRequest
{
public:
	LLEstateAssetRequest(const LLUUID& uuid, const LLAssetType::EType at,
						 EstateAssetType et);

	LLBaseDownloadRequest* getCopy() override;

	LL_INLINE LLAssetType::EType getAType() const		{ return mType; }

protected:
	EstateAssetType		mEstateAssetType;
};

// Map of known bad assets
typedef fast_hmap<LLUUID, U64> toxic_asset_map_t;

typedef void (*LLGetAssetCallback)(const LLUUID& asset_id,
								   LLAssetType::EType type, void* user_data,
								   S32 status, LLExtStat ext_status);

class LLAssetStorage
{
protected:
	LOG_CLASS(LLAssetStorage);

public:
	typedef void (*LLStoreAssetCallback)(const LLUUID& asset_id,
										 void* user_data, S32 status,
										 LLExtStat ext_status);
	typedef std::list<LLAssetRequest*> request_list_t;

	enum ERequestType
	{
		RT_INVALID = -1,
		RT_DOWNLOAD = 0,
		RT_UPLOAD = 1,
		RT_LOCALUPLOAD = 2,
		RT_COUNT = 3
	};

	LLAssetStorage(LLMessageSystem* msg, LLXferManager* xfer,
				   const LLHost& upstream_host);

	LLAssetStorage(LLMessageSystem* msg, LLXferManager* xfer);
	virtual ~LLAssetStorage();

	void setUpstream(const LLHost& upstream_host);

	bool hasLocalAsset(const LLUUID& uuid, LLAssetType::EType type);

	// public interface methods
	// note that your callback may get called BEFORE the function returns

	void getAssetData(const LLUUID uuid, LLAssetType::EType atype,
					  LLGetAssetCallback cb, void* user_data,
					  bool is_priority = false);

	// TransactionID version. Viewer needs the store_local.
	virtual void storeAssetData(const LLTransactionID& tid,
								LLAssetType::EType atype,
								LLStoreAssetCallback callback, void* user_data,
								bool temp_file = false,
								bool is_priority = false,
								bool store_local = false,
								bool user_waiting = false,
								F64 timeout = LL_ASSET_STORAGE_TIMEOUT) = 0;

	virtual void checkForTimeouts();

	void getEstateAsset(const LLHost& object_sim, const LLUUID& agent_id,
						const LLUUID& session_id, const LLUUID& asset_id,
						LLAssetType::EType atype, EstateAssetType etype,
						LLGetAssetCallback callback, void* user_data,
						bool is_priority);

	// Get a particular inventory item.
	void getInvItemAsset(const LLHost& object_sim, const LLUUID& agent_id,
						 const LLUUID& session_id, const LLUUID& owner_id,
						 const LLUUID& task_id, const LLUUID& item_id,
						 const LLUUID& asset_id, LLAssetType::EType atype,
						 LLGetAssetCallback cb, void* user_data,
						 bool is_priority = false);

	// Check if an asset is in the toxic map. If it is, the entry is updated.
	bool isAssetToxic(const LLUUID& uuid);

	// Clean the toxic asset list, remove old entries
	void flushOldToxicAssets(bool force_it);

	// Add an item to the toxic asset map
	void markAssetToxic(const LLUUID& uuid);

protected:
	LLSD getPendingDetailsImpl(const request_list_t* requests,
	 						   LLAssetType::EType asset_type,
	 						   const std::string& detail_prefix) const;

	LLSD getPendingRequestImpl(const request_list_t* requests,
							   LLAssetType::EType asset_type,
							   const LLUUID& asset_id) const;

	bool deletePendingRequestImpl(request_list_t* requests,
								  LLAssetType::EType asset_type,
								  const LLUUID& asset_id);

public:
	static const LLAssetRequest* findRequest(const request_list_t* requests,
										LLAssetType::EType asset_type,
										const LLUUID& asset_id);
	static LLAssetRequest* findRequest(request_list_t* requests,
										LLAssetType::EType asset_type,
										const LLUUID& asset_id);

	request_list_t* getRequestList(ERequestType rt);
	const request_list_t* getRequestList(ERequestType rt) const;
	static std::string getRequestName(ERequestType rt);

	S32 getNumPendingDownloads() const;
	S32 getNumPendingUploads() const;
	S32 getNumPendingLocalUploads();
	S32 getNumPending(ERequestType rt) const;

	LLSD getPendingDetails(ERequestType rt, LLAssetType::EType asset_type,
	 					   const std::string& detail_prefix) const;

	LLSD getPendingRequest(ERequestType rt, LLAssetType::EType asset_type,
	 					   const LLUUID& asset_id) const;

	bool deletePendingRequest(ERequestType rt, LLAssetType::EType asset_type,
	 						  const LLUUID& asset_id);

	// download process callbacks
	static void downloadCompleteCallback(S32 result,
										 const LLUUID& file_id,
										 LLAssetType::EType file_type,
										 LLBaseDownloadRequest* user_data,
										 LLExtStat ext_status);
	static void downloadEstateAssetCompleteCallback(S32 result,
													const LLUUID& file_id,
													LLAssetType::EType file_type,
													LLBaseDownloadRequest* user_data,
													LLExtStat ext_status);
	static void downloadInvItemCompleteCallback(S32 result,
												const LLUUID& file_id,
												LLAssetType::EType file_type,
												LLBaseDownloadRequest* user_data,
												LLExtStat ext_status);

	static void removeAndCallbackPendingDownloads(const LLUUID& file_id,
												  LLAssetType::EType file_type,
												  const LLUUID& callback_id,
												  LLAssetType::EType callback_type,
                                                  S32 result_code,
												  LLExtStat ext_status);

	// upload process callbacks
	static void uploadCompleteCallback(const LLUUID&, void* user_data,
									   S32 result, LLExtStat ext_status);
	static void processUploadComplete(LLMessageSystem* msg,
									  void** this_handle);

	// Debugging
	static const char* getErrorString(S32 status);

	// Deprecated file-based methods - Not overriden.
	void getAssetData(const LLUUID uuid, LLAssetType::EType type,
					  void (*callback)(const char*, const LLUUID&, void*, S32,
									   LLExtStat), void* user_data,
					  bool is_priority = false);

	// TransactionID version
	virtual void storeAssetData(const std::string& filename,
								const LLTransactionID& transaction_id,
								LLAssetType::EType type,
								LLStoreAssetCallback callback,
								void* user_data,
								bool temp_file = false,
								bool is_priority = false,
								bool user_waiting = false,
								F64 timeout = LL_ASSET_STORAGE_TIMEOUT) = 0;

	static void legacyGetDataCallback(const LLUUID& uuid,
									  LLAssetType::EType, void* user_data,
									  S32 status, LLExtStat ext_status);
	static void legacyStoreDataCallback(const LLUUID& uuid, void* user_data,
										S32 status, LLExtStat ext_status);

protected:
	void cleanupRequests(bool all, S32 error);
	void callUploadCallbacks(const LLUUID& uuid, LLAssetType::EType asset_type,
							 bool success, LLExtStat ext_status);

	virtual void queueDataRequest(const LLUUID& uuid, LLAssetType::EType type,
								  LLGetAssetCallback callback, void* user_data,
								  bool duplicate, bool is_priority) = 0;

private:
	void init(LLMessageSystem* msg, LLXferManager* xfer,
			  const LLHost& upstream_host);

protected:
	LLHost				mUpstreamHost;

	LLMessageSystem*	mMessageSys;
	LLXferManager*		mXferManager;

	bool				mShutDown;

	request_list_t		mPendingDownloads;
	request_list_t		mPendingUploads;
	request_list_t		mPendingLocalUploads;

	// Map of toxic assets: these caused problems when recently rezzed, so
	// avoid loading them.
	toxic_asset_map_t	mToxicAssetMap;
};

////////////////////////////////////////////////////////////////////////
// Wrappers to replicate deprecated API
////////////////////////////////////////////////////////////////////////

class LLLegacyAssetRequest
{
public:
	void (*mDownCallback)(const char*, const LLUUID&, void*, S32, LLExtStat);
	LLAssetStorage::LLStoreAssetCallback mUpCallback;
	void* mUserData;
};

extern LLAssetStorage* gAssetStoragep;
extern const LLUUID CATEGORIZE_LOST_AND_FOUND_ID;

#endif
