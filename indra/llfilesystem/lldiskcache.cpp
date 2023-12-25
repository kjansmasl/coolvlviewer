/**
 * @file lldiskcache.cpp
 * @brief Implementation of the disk cache.
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

#include "linden_common.h"

#include "boost/filesystem.hpp"

#include "lldiskcache.h"

#include "llcallbacklist.h"
#include "lldir.h"
#include "lldiriterator.h"
#include "llrand.h"
#include "llthread.h"
#include "lltimer.h"
#include "hbtracy.h"

using namespace boost::filesystem;

// Threshold in time_t units that is used to decide if the last access time of
// the file is updated or not. Added as a precaution for the concern outlined
// in SL-14582 about frequent writes on SSDs reducing their lifespan. Let's
// start with half an hour in time_t units and see how that unfolds.
constexpr time_t TIME_THRESHOLD = 1800;
// ... reduced to only one minute when we are currently purging the cache. HB
constexpr time_t TIME_THRESHOLD_PURGE = 60;

// Interval of time between consecutive checks for the stopping of the purging
// thread (1 second).
constexpr F32 INTERVAL_BETWEEN_CHECKS = 1.f;

// Static variable members
LLCachePurgeThread* LLDiskCache::sPurgeThread = NULL;
std::string LLDiskCache::sCacheDir;
U64 LLDiskCache::sNominalSizeBytes = 0;
U64 LLDiskCache::sMaxSizeBytes = 0;
LLAtomicU64 LLDiskCache::sCurrentSizeBytes(0);
LLAtomicBool LLDiskCache::sPurging(false);
bool LLDiskCache::sCacheValid = false;

// Subdirectory names 0...9a...f, concatenated in a string
static std::string sDigits = "0123456789abcdef";

///////////////////////////////////////////////////////////////////////////////
// LLCachePurgeThread class
///////////////////////////////////////////////////////////////////////////////

class LLCachePurgeThread final : public LLThread
{
protected:
	LOG_CLASS(LLCachePurgeThread);

public:
	LL_INLINE LLCachePurgeThread()
	:	LLThread("Disk cache purging thread")
	{
		start();
	}

	LL_INLINE void run() override
	{
		LLDiskCache::purge();
	}
};

///////////////////////////////////////////////////////////////////////////////
// LLDiskCache class
///////////////////////////////////////////////////////////////////////////////

//static
void LLDiskCache::init(U64 nominal_size_bytes, bool second_instance)
{
	llinfos << "Initializing cache..." << llendl;

	sNominalSizeBytes = nominal_size_bytes;
	sMaxSizeBytes = 15UL * sNominalSizeBytes / 10UL;
	if (second_instance)
	{
		// Add 50 to 150 Mb (in random steps of 5Mb) to the maximum size for
		// the second and further instances, so that the various instances do
		// not attempt to purge the cache at the same time (even though, since
		// they only account each for their own cache file writes, they will
		// not see the same apparent cache size at the same time)... HB
		sMaxSizeBytes += (50UL + 5UL * U64(ll_frand(20.f))) * 1048576UL;
	}

	// We enforce the storage of our files in an "assets" sub-directory, which
	// save us from worrying about deleting files that do not belong to our
	// cache (no need to test for a file prefix or extension, meaning faster
	// operations when purging, clearing, or calculating the cache size). HB
	sCacheDir = gDirUtilp->getExpandedFilename(LL_PATH_CACHE, "assets");

	sCacheValid = LLFile::mkdir(sCacheDir);
	if (sCacheValid)
	{
		sCacheDir += LL_DIR_DELIM_CHR;
		// We use sub-directories to lower the number of file entries per
		// directory (which can easily count in hundred of thousands when
		// using a large cache in a single directory). This avoids hitting
		// any file-system limitation, and helps speeding up the opening of
		// cache files. HB
		for (U32 i = 0; i < 16; ++i)
		{
			sCacheValid &= LLFile::mkdir(sCacheDir + sDigits[i]);
		}
	}
	if (sCacheValid)
	{
#if LL_WINDOWS
		if (!second_instance)
		{
			// Do not call cacheDirSize() on startup from the main thread under
			// Windows when the cache directory has not already been scanned
			// (i.e. after boot, from the first viewer instance): it causes
			// minutes-long delays for large caches on hard disks (obviously a
			// problem with "SuperFetch", but even after disabling it, scanning
			// the cache can take a couple dozens seconds, when the same cache
			// takes at most a few seconds to get scanned under Linux) !
			// LLDiskCache::threadedPurge() will instead set sCurrentSizeBytes
			// for us, and in a non-blocking thread... HB
			llinfos << "Nominal cache size: " << sNominalSizeBytes
					<< " bytes. Maximal cache size: " << sMaxSizeBytes
					<< " bytes. Cache directory: " << sCacheDir << llendl;
			return;
		}
#endif
		sCurrentSizeBytes = cacheDirSize();
		llinfos << "Nominal cache size: " << sNominalSizeBytes
				<< " bytes. Maximal cache size: " << sMaxSizeBytes
				<< " bytes. Current cache size: " << sCurrentSizeBytes
				<< " bytes. Cache directory: " << sCacheDir << llendl;
	}
	else
	{
		llwarns << "Cache path is invalid: " << sCacheDir << llendl;
	}
}

//static
void LLDiskCache::shutdown()
{
	// Stop changing the cache now !
	sCacheValid = false;

	if (sPurgeThread)
	{
		U32 loops = 0;
		while (loops++ < 100 && !sPurgeThread->isStopped())
		{
			ms_sleep(10);	// Give it some more time...
		}
		if (sPurgeThread->isStopped())
		{
			llinfos << "Cache purging thread stopped." << llendl;
		}
		else
		{
			llwarns << "Timeout waiting for the cache purging thread to stop. Force-removing it."
					<< llendl;
		}
		delete sPurgeThread;
		sPurgeThread = NULL;
		sPurging = false;
	}
}

//static
U64 LLDiskCache::cacheDirSize()
{
	U64 total_file_size = 0;
	std::string subdir, filename;
	for (U32 i = 0; i < 16; ++i)
	{
		subdir = sCacheDir + sDigits[i];
		if (LLFile::isdir(subdir))
		{
			LLDirIterator iter(subdir, NULL, DI_SIZE);
			while (iter.next(filename))
			{
				total_file_size += iter.getSize();
			}
		}
	}
	return total_file_size;
}

//static
void LLDiskCache::clear()
{
	if (LLFile::isdir(sCacheDir))
	{
		std::string subdir;
		for (U32 i = 0; i < 16; ++i)
		{
			subdir = sCacheDir + sDigits[i];
			if (LLFile::isdir(subdir))
			{
				LLDirIterator::deleteFilesInDir(subdir);
			}
		}
	}
	else
	{
		llinfos << "No cache directory: nothing to clear." << llendl;
	}
	sCurrentSizeBytes = 0;
}

//static
void LLDiskCache::purge()
{
	if (!LLFile::isdir(sCacheDir))
	{
		llinfos << "No cache directory: nothing to purge." << llendl;
		return;
	}

	sPurging = true;

	typedef std::pair<time_t, std::pair<U64, std::string> > file_info_t;
	std::vector<file_info_t> file_info;

	LLTimer purge_timer;
	purge_timer.reset();

	std::string subdir, filename;
	for (U32 i = 0; i < 16; ++i)
	{
		subdir = sCacheDir + sDigits[i];
		if (!LLFile::isdir(subdir))
		{
			llwarns << "Missing cache sub-directory: " << subdir << llendl;
			continue;
		}
		LLDirIterator iter(subdir, NULL, DI_ISFILE | DI_SIZE | DI_TIMESTAMP);
		while (iter.next(filename))
		{
			if (iter.isFile())
			{
				file_info.emplace_back(iter.getTimeStamp(),
									   std::make_pair(iter.getSize(),
													  iter.getPath() +
													  filename));
			}
		}
	}

	std::sort(file_info.begin(), file_info.end(),
			  [](file_info_t& x, file_info_t& y)
			  {
					return x.first > y.first;
			  });

	U32 count = file_info.size();

	llinfos << count
			<< " files found in cache. Checking the total size and possibly purging old files..."
			<< llendl;

	U64 files_size_total = 0;
	U64 removed_bytes = 0;
	U32 purged_files = 0;
	for (U32 i = 0; i < count; ++i)
	{
		const file_info_t& entry = file_info[i];
		files_size_total += entry.second.first;
		bool removed = files_size_total > sNominalSizeBytes;
		if (removed)
		{
			try
			{
				// Verify that the file did not get touched by another thread
				// or viewer instance since we last checked its time stamp !
				if (last_write_time(entry.second.second) <= entry.first)
				{
					remove(entry.second.second);
					++purged_files;
					removed_bytes += entry.second.first;
				}
				else
				{
					LL_DEBUGS("DiskCache") << "Skipped updated file: "
										   << entry.second.second << LL_ENDL;
					removed = false;
				}
			}
			catch (const filesystem_error& e)
			{
				removed = false;
				llwarns << "Failure to remove \"" << entry.second.second
						<< "\". Reason: " << e.what() << llendl;
			}
		}
		LL_DEBUGS("DiskCache") << (removed ? " Removed " : "Kept ")
							   << entry.second.second << LL_ENDL;
	}

	sPurging = false;

#if 0	// This would be more accurate for a single running viewer instance (no
		// risk of a race condition with another thread writing into the cache
		// while we counted the files size in it), but with multiple running
		// instances of the viewer, sCurrentSizeBytes does not account for
		// files written by those instances, so we must reset the actual cache
		// size (as we just counted it) to account for them... HB
		// Plus, for Windoze, we cannot do that (sCurrentSizeBytes not being
		// initialized to the actual cache size on login: see the comment in
		// init()).
	sCurrentSizeBytes -= removed_bytes;
#else
	sCurrentSizeBytes = files_size_total - removed_bytes;
#endif

	U32 ms = (U32)(purge_timer.getElapsedTimeF32() * 1000.f);
	if (purged_files)
	{
		llinfos << "Cache purge took " << ms << "ms to execute. "
				<< purged_files << " purged files and " << removed_bytes
				<< " bytes removed. " << sCurrentSizeBytes
				<< " bytes now in cache." << llendl;
	}
	else
	{
		llinfos << "Cache check took " << ms << "ms to execute. Cache size: "
				<< sCurrentSizeBytes << " bytes." << llendl;
	}
	LL_DEBUGS("DiskCache") << "Current cache size: " << cacheDirSize()
						   << " bytes." << LL_ENDL;
}

// Must be called from the main thread only !
//static
void LLDiskCache::threadedPurge()
{
	if (!sCacheValid)
	{
		return;
	}
	if (sPurgeThread)	// Called via doAfterInterval()
	{
		if (sPurgeThread->isStopped())
		{
			LL_DEBUGS("DiskCache") << "Purge thread stopped, deleting it."
								   << LL_ENDL;
			delete sPurgeThread;
			sPurgeThread = NULL;
		}
		else
		{
			LL_DEBUGS("DiskCache") << "Purge thread still running..."
								   << LL_ENDL;
			// Check again later to see if the thread has stopped
			doAfterInterval(threadedPurge, INTERVAL_BETWEEN_CHECKS);
		}
	}
	else				// Called by addBytesWritten() or from llappviewer.cpp
	{
		// Start a new thread.
		LL_DEBUGS("DiskCache") << "Starting a new purge thread..." << LL_ENDL;
		sPurgeThread = new LLCachePurgeThread;
		// Check again later to see if the thread has stopped
		doAfterInterval(threadedPurge, INTERVAL_BETWEEN_CHECKS);
	}
}

//static
std::string LLDiskCache::getFilePath(const LLUUID& id, const char* extra_info)
{
	std::string filename = id.asString();
	if (extra_info && *extra_info)
	{
		filename += '_';
		filename += extra_info;
	}
	filename += ".asset";
	return ((sCacheDir + filename[0]) + LL_DIR_DELIM_STR) + filename;
}

//static
void LLDiskCache::addBytesWritten(S32 bytes)
{
	LL_TRACY_TIMER(TRC_DISKCACHE_UPDSIZE);

	sCurrentSizeBytes += bytes;

	// If not called by the main thread, or a threaded purging is in progress,
	// bail out now.
	if (!is_main_thread() || sPurgeThread)
	{
		return;
	}

	LL_DEBUGS("DiskCache") << "Cache size: " << sCurrentSizeBytes << " bytes."
						   << LL_ENDL;

	// Start purging the cache if needed.
	if (sCurrentSizeBytes > sMaxSizeBytes)
	{
		threadedPurge();
	}
}

//static
void LLDiskCache::updateFileAccessTime(const std::string& filename)
{
	LL_TRACY_TIMER(TRC_DISKCACHE_ACCESSTIME);

	// Current time
	const time_t cur_time = computer_time();

	// Last write time
	time_t last_write = LLFile::lastModidied(filename);

	// We only write the new value if 'threshold' has elapsed since the last
	// write.
	time_t threshold = sPurging ? TIME_THRESHOLD_PURGE : TIME_THRESHOLD;
	if (cur_time - last_write > threshold)
	{
		boost::system::error_code ec;
#if LL_WINDOWS
		last_write_time(ll_convert_string_to_wide(filename), cur_time, ec);
#else
		last_write_time(filename, cur_time, ec);
#endif
		if (ec.failed())
		{
			llwarns << "Failure to touch \"" << filename
					<< "\". Reason: " << ec.message() << llendl;
		}
	}
}
