/**
 * @file llinventorymodelfetch.h
 * @brief LLInventoryModelFetch class header file
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 *
 * Copyright (c) 2010, Linden Research, Inc.
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

#ifndef LL_LLINVENTORYMODELFETCH_H
#define LL_LLINVENTORYMODELFETCH_H

#include <deque>

#include "llsingleton.h"
#include "lluuid.h"

class LLInventoryItem;
class LLTimer;

// This class handles background fetches, which are fetches of inventory
// folder. Fetches can be recursive or not.

class LLInventoryModelFetch final : public LLSingleton<LLInventoryModelFetch>
{
	friend class LLSingleton<LLInventoryModelFetch>;

protected:
	LOG_CLASS(LLInventoryModelFetch);

public:
	LLInventoryModelFetch();

	// Start and stop background breadth-first fetching of inventory contents.
	// This gets triggered when performing a filter-search.
	void start(const LLUUID& cat_id = LLUUID::null, bool recursive = true);
	void scheduleFolderFetch(const LLUUID& cat_id, bool force = false);
	void scheduleItemFetch(const LLUUID& item_id, bool force = false);

	LL_INLINE bool backgroundFetchActive() const
	{
		return mBackgroundFetchActive;
	}

	// Completing the fetch once per session should be sufficient:
	LL_INLINE bool isEverythingFetched() const
	{
		return mAllRecursiveFoldersFetched;
	}

	LL_INLINE bool libraryFetchStarted() const
	{
		return mRecursiveLibraryFetchStarted;
	}

	bool libraryFetchCompleted() const;

	LL_INLINE bool libraryFetchInProgress() const
	{
		return mRecursiveLibraryFetchStarted && !libraryFetchCompleted();
	}

	LL_INLINE bool inventoryFetchStarted() const
	{
		return mRecursiveInventoryFetchStarted;
	}

	bool inventoryFetchCompleted() const;

	LL_INLINE bool inventoryFetchInProgress() const
	{
		return mRecursiveInventoryFetchStarted && !inventoryFetchCompleted();
	}

    void findLostItems();

	void incrFetchCount(S32 fetching);
	void incrFetchFolderCount(S32 fetching);

	bool isBulkFetchProcessingComplete() const;
	bool isFolderFetchProcessingComplete() const;
	void setAllFoldersFetched();

	void addRequestAtFront(const LLUUID& id, bool recursive, bool is_category);
	void addRequestAtBack(const LLUUID& id, bool recursive, bool is_category);

	void onAISContentsCallback(const uuid_vec_t& content_ids,
							  const LLUUID& response_id);
	void onAISFolderCallback(const LLUUID& cat_id,
							 const LLUUID& response_id, U32 fetch_type);

	LL_INLINE static void setUseAISFetching(bool b)	{ sUseAISFetching = b; }
	static bool useAISFetching();

	// Helpers for force-fetching inventory items and folders. HB
	static void forceFetchFolder(const LLUUID& cat_id);
	static void forceFetchItem(const LLUUID& item_id);
	// Use this when you got the item pointer (faster).
	static void forceFetchItem(const LLInventoryItem* itemp);

private:
	typedef enum : U32
	{
		FT_DEFAULT = 0,
		FT_FORCED,			  	// Non-recursively even if already loaded
		FT_CONTENT_RECURSIVE,	// Request content recursively
		FT_FOLDER_AND_CONTENT,	// Request folder, then content recursively
		FT_RECURSIVE,			// Request everything recursively
	} EFetchType;

	struct FetchQueueInfo
	{
		FetchQueueInfo(const LLUUID& id, U32 fetch_type, bool is_category)
		:	mUUID(id),
			mFetchType(fetch_type),
			mIsCategory(is_category)
		{
		}

		LLUUID	mUUID;
		U32		mFetchType;
		bool	mIsCategory;
	};

	void bulkFetch(const std::string& url);
	void bulkFetchAIS();
	void bulkFetchAIS(const FetchQueueInfo& fetch_info);

	void backgroundFetch();
	static void backgroundFetchCB(void*);	// Background fetch idle method

	bool fetchQueueContainsNoDescendentsOf(const LLUUID& cat_id) const;

private:
	typedef std::deque<FetchQueueInfo> fetch_queue_t;
	fetch_queue_t	mFetchFolderQueue;
	fetch_queue_t	mFetchItemQueue;

	uuid_list_t		mExpectedFolderIds;

	LLTimer			mFetchTimer;

#if 0	// Not yet used by the Cool VL Viewer. HB
	typedef boost::signals2::signal<void()> signal_t;
	signal_t		mFoldersFetchedSignal;
#endif

	S32				mFetchCount;
	S32				mLastFetchCount;
	S32				mFetchFolderCount;

 	bool			mRecursiveInventoryFetchStarted;
	bool			mRecursiveLibraryFetchStarted;
	bool			mAllRecursiveFoldersFetched;
	bool			mBackgroundFetchActive;
	bool			mFolderFetchActive;

	static bool		sUseAISFetching;
};

#endif // LL_LLINVENTORYMODELFETCH_H
