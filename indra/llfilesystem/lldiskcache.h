/**
 * @file lldiskcache.h
 * @brief Definition of the disk cache implementation.
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 *
 * Copyright (c) 2020, Linden Research, Inc. (c) 2021 Henri Beauchamp.
 *
 * Modifications by Henri Beauchamp:
 *  - Pointless per-asset-type file naming removed.
 *  - Use of LLFile faster operations and of the extended LLDiriterator where
 *    possible.
 *  - Cache structure changed to speed up file opening and reduce the risk of
 *    hitting file-systems limitations (such as the max number of files per
 *    directory).
 *  - Proper cache validation and shutdown.
 *  - Proper catching of throw()s and boost::filesystem errors.
 *  - Track cache files size in real time (lock-less: just via an atomic
 *    variable).
 *  - Proper and threaded auto-purging of the cache when it exceeds 150% of
 *    its nominal size.
 *  - Multiple threads and multiple viewer instances deconfliction.
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

#ifndef LL_LLDISKCACHE_H
#define LL_LLDISKCACHE_H

#include <utility>

#include "llatomic.h"
#include "lluuid.h"

class LLCachePurgeThread;

// Purely static class
class LLDiskCache
{
protected:
	LOG_CLASS(LLDiskCache);

	LLDiskCache() = delete;
	~LLDiskCache() = delete;

public:
	// Note: when 'second_instance' is true, the cache is purged only after
	// reaching a higher size, so that the first running instance of the viewer
	// will purge it before this second instance would, and this as long as it
	// is running; should the first instance vanish, this second instance will
	// then automatically take over the cache purging task.
	// Of course, with three or more viewer instances, and should the first one
	// vanish, the remaining instances will still fight over the cache purging,
	// but there is an additionnal randomization of the max cache size for
	// these instances...
	static void init(U64 nominal_size_bytes, bool second_instance);

	// Clears the cache by removing all the files in the specified cache
	// directory individually.
	static void clear();

	// Purges the oldest items in the cache so that the combined size of all
	// files is no bigger than sNominalSizeBytes. May be internally threaded.
	static void purge();

	// Threaded cache purging. Must be called only from the main thread.
	// Called from llappviewer.cpp or internally from addBytesWritten().
	static void threadedPurge();

	// Shuts down the cache when the viewer closes.
	static void shutdown();

	// Returns true when the cache is initialized and valid
	LL_INLINE static bool isValid()					{ return sCacheValid; }

	// IMPORTANT: the three following methods are called both from the main
	// thead and from other threads (e.g. the mesh repository, the texture
	// cache worker, etc).

	// Constructs a file name and path based on the asset UUID and optional
	// extra info.
	static std::string getFilePath(const LLUUID& id,
								   const char* extra_info = NULL);

	// Updates the "last write time" of a file to "now". This must be called
	// whenever a file in the cache is (going to be) opened (for either reads
	// or writes, since we must guard against a possibly ongoing purge before
	// an actual read/write would happen) so that the last time the file was
	// accessed is up to date; this time stamp is used in the mechanism for
	// purging the cache.
	static void updateFileAccessTime(const std::string& file_path);

	// Used to update the disk cache about file writes ('bytes' may be negative
	// when removing or truncating a file).
	static void addBytesWritten(S32 bytes);

private:
	// Utility method to gather the total size (in bytes) occupied by the cache
	// files.
	static U64 cacheDirSize();

private:
	// Contains the pointer to the cache purging thread.
	static LLCachePurgeThread*	sPurgeThread;
	// Cache directory path
	static std::string			sCacheDir;
	// The nominal size of the cache in bytes. After purging, the total size of
	// the cache files in the cache directory will be less than this value.
	static U64					sNominalSizeBytes;
	// Maximal amount of data in cache beyond which the cache gets purged.
	static U64					sMaxSizeBytes;
	// Current size of the cache. This is an atomic variable, since it can get
	// updated by various threads concurrently, via addBytesWritten() and
	// threaded purge().
	static LLAtomicU64			sCurrentSizeBytes;
	// true while purging, and atomic since used in various threads
	static LLAtomicBool			sPurging;
	// true when the cache directory is valid
	static bool					sCacheValid;
};

#endif	// LL_LLDISKCACHE_H
