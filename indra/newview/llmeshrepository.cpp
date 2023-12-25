/**
 * @file llmeshrepository.cpp
 * @brief Mesh repository implementation.
 *
 * $LicenseInfo:firstyear=2004&license=viewergpl$
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

#include "llviewerprecompiledheaders.h"

#ifndef LL_WINDOWS
# include <netdb.h>
#endif

#include <utility>

#include "boost/iostreams/device/array.hpp"
#include "boost/iostreams/stream.hpp"
#include "boost/lexical_cast.hpp"

#include "llmeshrepository.h"

#include "imageids.h"
#include "llapp.h"
#include "llcallbacklist.h"
#include "llcorebufferarray.h"
#include "llcorebufferstream.h"
#include "llcorehttputil.h"
#include "lldatapacker.h"
#include "lleconomy.h"
#include "llfilesystem.h"
#include "llfoldertype.h"
#include "llhttpconstants.h"
#include "llhost.h"
#include "llimagej2c.h"
#include "llnotifications.h"
#include "llsd.h"
#include "llsdserialize.h"
#include "llsdutil_math.h"
#include "llthread.h"
#include "hbtracy.h"
#include "lltrans.h"
#include "llvolumemgr.h"

#include "llagent.h"
#include "llfloaterperms.h"
#include "llinventorymodel.h"
#include "llpipeline.h"
#include "llviewerassetupload.h"
#include "llviewercontrol.h"
#include "llviewerinventory.h"
#include "llviewermenu.h"
#include "llviewerobject.h"
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"
#include "llviewertexturelist.h"
#include "llvovolume.h"
#include "llworld.h"

// Purpose
//
// The purpose of this module is to provide access between the viewer and the
// asset system as regards to mesh objects.
// (for structural details, see:
//  http://wiki.secondlife.com/wiki/Mesh/Mesh_Asset_Format)
//
//  * High-throughput download of mesh assets from servers while following
//    best industry practices for network profile.
//  * Reliable expensing and upload of new mesh assets.
//  * Recovery and retry from errors when appropriate.
//  * Decomposition of mesh assets for preview and uploads.
//  * And most important: all of the above without exposing the main thread to
//    stalls due to deep processing or thread locking actions. In particular,
//    the following operations on LLMeshRepository are very averse to any
//    stalls:
//     * loadMesh
//     * notifyLoadedMeshes
//     * getSkinInfo
//
// Threads
//
//   main     Main rendering thread, very sensitive to locking and other stalls
//   repo     Overseeing worker thread associated with the LLMeshRepoThread
//            class
//   decom    Worker thread for mesh decomposition requests
//   core     HTTP worker thread: does the work but doesn't intrude here
//   uploadN  0-N temporary mesh upload threads (0-1 in practice)
//
// Sequence of Operations
//
// What follows is a description of the retrieval of one LOD for a new mesh
// object. Work is performed by a series of short, quick actions distributed
// over a number of threads. Each is meant to proceed without stalling and the
// whole forms a deep request pipeline to achieve throughput. Ellipsis
// indicates a return or break in processing which is resumed elsewhere.
//
//         main thread repo thread (run() method)
//
//         loadMesh() invoked to request LOD
//           append LODRequest to mPendingRequests
//         ...
//         other mesh requests may be made
//         ...
//         notifyLoadedMeshes() invoked to stage work
//           append HeaderRequest to mHeaderReqQ
//         ...
//                             scan mHeaderReqQ
//                             issue 4096-byte GET for header
//                             ...
//                             onCompleted() invoked for GET
//                               data copied
//                               headerReceived() invoked
//                                 LLSD parsed
//                                 mMeshHeaders updated
//                                 scan mPendingLOD for LOD request
//                                 push LODRequest to mLODReqQ
//                             ...
//                             scan mLODReqQ
//                             fetchMeshLOD() invoked
//                               issue Byte-Range GET for LOD
//                             ...
//                             onCompleted() invoked for GET
//                               data copied
//                               lodReceived() invoked
//                                 unpack data into LLVolume
//                                 append LoadedMesh to mLoadedMeshes
//                             ...
//         notifyLoadedMeshes() invoked again
//           scan mLoadedMeshes
//           notifyMeshLoaded() for LOD
//             setMeshAssetLoaded() invoked for system volume
//             notifyMeshLoaded() invoked for each interested object
//         ...
//
// Mutexes
//
//   LLMeshRepository::mMeshMutex
//   LLMeshRepoThread::mMutex
//   LLMeshRepoThread::mHeaderMutex
//   LLMeshRepoThread::mSignal (LLCondition)
//   LLPhysicsDecomp::mSignal (LLCondition)
//   LLPhysicsDecomp::mMutex
//   LLMeshUploadThread::mMutex
//
// Mutex Order Rules
//
//   1.  LLMeshRepoThread::mMutex before LLMeshRepoThread::mHeaderMutex
//   2.  LLMeshRepository::mMeshMutex before LLMeshRepoThread::mMutex
//   (there are more rules, have not been extracted)
//
// Data Member Access/Locking
//
// Description of how shared access to static and instance data members is
// performed. Each member is followed by the name of the mutex, if any,
// covering the data and then a list of data access models each of which is a
// triplet of the following form:
//
//     {ro, wo, rw}.{main, repo, any}.{mutex, none}
//     Type of access: read-only, write-only, read-write.
//     Accessing thread or 'any'
//     Relevant mutex held during access (several may be held) or 'none'
//
// A careful eye will notice some unsafe operations. Many of these have an
// alibi of some form. Several types of alibi are identified and listed here:
//
//     [1] Single-writer, self-consistent readers. Old data must be tolerated
//         by any reader but data will come true eventually.
//     [2] Like [1] but provides a hint about thread state. These may be
//         unsafe.
//     [3] empty() check outside of lock. Can me made safish when done in
//         double-check lock style. But this depends on std:: implementation
//         and memory model.
//     [4] Appears to be covered by a mutex but does not need one.
//     [5] Read of a double-checked lock.
//
// So, in addition to documentation, take this as a to-do/review list and see
// if you can improve things. For porters to non-x86 architectures, the weaker
// memory models will make these platforms probabilistically more susceptible
// to hitting race conditions. True here and in other multi-thread code such as
// texture fetching.
//
// LLMeshRepository:
//
//     sBytesReceived                  none            rw.repo.none, ro.main.none [1]
//     sMeshRequestCount               "
//     sHTTPRequestCount               "
//     sHTTPLargeRequestCount          "
//     sHTTPRetryCount                 "
//     sHTTPErrorCount                 "
//     sLODPending                     atomic
//     sLODProcessing                  atomic
//     sCacheBytesRead                 none            rw.repo.none, ro.main.none [1]
//     sCacheBytesWritten              "
//     sCacheReads                     "
//     sCacheWrites                    "
//     mLoadingMeshes                  mMeshMutex [4]  rw.main.none, rw.any.mMeshMutex
//     mSkinMap                        none            rw.main.none
//     mDecompositionMap               none            rw.main.none
//     mPendingRequests                mMeshMutex [4]  rw.main.mMeshMutex
//     mLoadingSkins                   mMeshMutex [4]  rw.main.mMeshMutex
//     mPendingSkinRequests            mMeshMutex [4]  rw.main.mMeshMutex
//     mLoadingDecompositions          mMeshMutex [4]  rw.main.mMeshMutex
//     mPendingDecompositionRequests   mMeshMutex [4]  rw.main.mMeshMutex
//     mLoadingPhysicsShapes           mMeshMutex [4]  rw.main.mMeshMutex
//     mPendingPhysicsShapeRequests    mMeshMutex [4]  rw.main.mMeshMutex
//     mUploads                        none            rw.main.none (upload thread accessing objects)
//     mUploadWaitList                 none            rw.main.none (upload thread accessing objects)
//     mInventoryQ                     mMeshMutex [4]  rw.main.mMeshMutex, ro.main.none [5]
//     mUploadErrorQ                   mMeshMutex      rw.main.mMeshMutex, rw.any.mMeshMutex
//
//   LLMeshRepoThread:
//
//     sActiveHeaderRequests    atomic
//     sActiveLODRequests       atomic
//     sMaxConcurrentRequests   mMutex        wo.main.none, ro.repo.none, ro.main.mMutex
//     mMeshHeaders             mHeaderMutex  rw.repo.mHeaderMutex, ro.main.mHeaderMutex
//     mSkinRequests            mMutex        rw.repo.mMutex, ro.repo.none [5]
//     mSkinInfos               mMutex        rw.repo.mMutex, rw.main.mMutex [5]
//     mDecompositionRequests   mMutex        rw.repo.mMutex, ro.repo.none [5]
//     mPhysicsShapeRequests    mMutex        rw.repo.mMutex, ro.repo.none [5]
//     mDecompositions          mMutex        rw.repo.mMutex, rw.main.mMutex [5]
//     mHeaderReqQ              mMutex        ro.repo.none [5], rw.repo.mMutex, rw.any.mMutex
//     mLODReqQ                 mMutex        ro.repo.none [5], rw.repo.mMutex, rw.any.mMutex
//     mUnavailableLODs         mMutex        rw.repo.mMutex, ro.main.none [5], rw.main.mMutex
//     mLoadedMeshes            mMutex        rw.repo.mMutex, ro.main.none [5], rw.main.mMutex
//     mPendingLOD              mMutex        rw.repo.mMutex, rw.any.mMutex
//     mGetMeshCapability       mMutex        rw.main.mMutex, ro.repo.mMutex
//     mGetMeshVersion          mMutex        rw.main.mMutex, ro.repo.mMutex
//     mHttp*                   none          rw.repo.none
//
//   LLMeshUploadThread:
//
//     mDiscarded               mMutex        rw.main.mMutex, ro.uploadN.none [1]
//     ... more ...
//
// *TODO: work list for followup actions:
//   * Review anything marked as unsafe above, verify if there are real issues.
//   * See if we can put ::run() into a hard sleep. May not actually perform
//     better than the current scheme so be prepared for disappointment. You
//     will likely need to introduce a condition variable class that references
//     a mutex in methods rather than derives from mutex which is not correct.
//   * On upload failures, make more information available to the alerting
//     dialog. Get the structured information going into the log into a tree
//     there.
//   * Header parse failures come without much explanation. Elaborate.
//   * Work queue for uploads ?  Any need for this or is the current scheme
//     good enough ?
//   * Move data structures holding mesh data used by main thread into main-
//     thread-only access so that no locking is needed. May require duplication
//     of some data so that worker thread has a minimal data set to guide
//     operations.

LLMeshRepository gMeshRepo;

// Important: assumption is that headers fit in this space
constexpr S32 MESH_HEADER_SIZE = 4096;
// Limits for GetMesh regions
constexpr S32 REQUEST_HIGH_WATER_MIN = 32;
constexpr S32 REQUEST_HIGH_WATER_MAX = 150;	// Should remain under 2X throttle
constexpr S32 REQUEST_LOW_WATER_MIN = 16;
constexpr S32 REQUEST_LOW_WATER_MAX = 75;
// Limits for GetMesh2 regions
constexpr S32 REQUEST2_HIGH_WATER_MIN = 32;
constexpr S32 REQUEST2_HIGH_WATER_MAX = 100;
constexpr S32 REQUEST2_LOW_WATER_MIN = 16;
constexpr S32 REQUEST2_LOW_WATER_MAX = 50;
// Size at which requests goes to narrow/slow queue
constexpr U32 LARGE_MESH_FETCH_THRESHOLD = 1U << 21;
// Seconds to complete xfer, small mesh downloads
constexpr long SMALL_MESH_XFER_TIMEOUT = 120L;
// Seconds to complete xfer, large downloads
constexpr long LARGE_MESH_XFER_TIMEOUT = 600L;

// Would normally like to retry on uploads as some retryable failures would be
// recoverable. Unfortunately, the mesh service is using 500 (retryable) rather
// than 400/bad request (permanent) for a bad payload and retrying that just
// leads to revocation of the one-shot cap which then produces a 404 on retry
// destroying some (occasionally) useful error information. We'll leave upload
// retries to the user as in the past. SH-4667.
constexpr long UPLOAD_RETRY_LIMIT = 0L;

// Maximum mesh version to support. Three least significant digits are reserved
// for the minor version, with major version changes indicating a format change
// that is not backwards compatible and should not be parsed by viewers that
// don't specifically support that version. For example, if the integer "1" is
// present, the version is 0.001. A viewer that can parse version 0.001 can
// also parse versions up to 0.999, but not 1.0 (integer 1000).
// See the wiki at https://wiki.secondlife.com/wiki/Mesh/Mesh_Asset_Format
constexpr S32 MAX_MESH_VERSION = 999;

U32 LLMeshRepository::sBytesReceived = 0;
U32 LLMeshRepository::sMeshRequestCount = 0;
U32 LLMeshRepository::sHTTPRequestCount = 0;
U32 LLMeshRepository::sHTTPLargeRequestCount = 0;
U32 LLMeshRepository::sHTTPRetryCount = 0;
U32 LLMeshRepository::sHTTPErrorCount = 0;
LLAtomicU32 LLMeshRepository::sLODProcessing(0);
LLAtomicU32 LLMeshRepository::sLODPending(0);

U32 LLMeshRepository::sCacheBytesRead = 0;
U32 LLMeshRepository::sCacheBytesWritten = 0;
U32 LLMeshRepository::sCacheReads = 0;
U32 LLMeshRepository::sCacheWrites = 0;
U32 LLMeshRepository::sMaxLockHoldoffs = 0;

LLAtomicS32 LLMeshRepoThread::sActiveHeaderRequests(0);
LLAtomicS32 LLMeshRepoThread::sActiveLODRequests(0);
U32	LLMeshRepoThread::sMaxConcurrentRequests = 1;
S32 LLMeshRepoThread::sRequestLowWater = REQUEST2_LOW_WATER_MIN;
S32 LLMeshRepoThread::sRequestHighWater = REQUEST2_HIGH_WATER_MIN;
S32 LLMeshRepoThread::sRequestWaterLevel = 0;

namespace {
	// The NoOpDeletor is used when passing certain objects (generally the
	// LLMeshUploadThread) in a smart pointer below for passage into the
	// LLCore::Http libararies. When the smart pointer is destroyed, no action
	// will be taken since we do not in these cases want the object to be
	// destroyed at the end of the call.
    void NoOpDeletor(LLCore::HttpHandler*)	{}
}

// Helper functions

static S32 sDumpNum = 0;
static std::string make_dump_name(const std::string& prefix, S32 num)
{
	return prefix + boost::lexical_cast<std::string>(num) + ".xml";
}

// Also used by llfloatermodelpreview.cpp, therefore not static
void dump_llsd_to_file(const LLSD& content, const std::string& filename)
{
	if (gSavedSettings.getBool("MeshUploadLogXML"))
	{
		std::ofstream of(filename.c_str());
		LLSDSerialize::toPrettyXML(content, of);
	}
}

#if 0	// Not used
static LLSD llsd_from_file(const std::string& filename)
{
	std::ifstream ifs(filename.c_str());
	LLSD result;
	LLSDSerialize::fromXML(result, ifs);
	return result;
}

// Returns the number of bytes resident in memory for given volume
static U32 get_volume_memory_size(const LLVolume* volume)
{
	U32 indices = 0;
	U32 vertices = 0;

	for (S32 i = 0, count = volume->getNumVolumeFaces(); i < count; ++i)
	{
		const LLVolumeFace& face = volume->getVolumeFace(i);
		indices += face.mNumIndices;
		vertices += face.mNumVertices;
	}

	return indices * 2 + vertices * 11 + sizeof(LLVolume) +
		   sizeof(LLVolumeFace) * volume->getNumVolumeFaces();
}
#endif

static void get_vertex_buffer_from_mesh(LLCDMeshData& mesh,
										LLModel::PhysicsMesh& res,
										F32 scale = 1.f)
{
	res.mPositions.clear();
	res.mNormals.clear();

	const F32* v = mesh.mVertexBase;

	if (mesh.mIndexType == LLCDMeshData::INT_16)
	{
		U16* idx = (U16*) mesh.mIndexBase;
		for (S32 j = 0; j < mesh.mNumTriangles; ++j)
		{
			F32* mp0 = (F32*)((U8*)v + idx[0] * mesh.mVertexStrideBytes);
			F32* mp1 = (F32*)((U8*)v + idx[1] * mesh.mVertexStrideBytes);
			F32* mp2 = (F32*)((U8*)v + idx[2] * mesh.mVertexStrideBytes);

			idx = (U16*)(((U8*)idx) + mesh.mIndexStrideBytes);

			LLVector3 v0(mp0);
			LLVector3 v1(mp1);
			LLVector3 v2(mp2);

			LLVector3 n = (v1 - v0) % (v2 - v0);
			n.normalize();

			res.mPositions.emplace_back(v0 * scale);
			res.mPositions.emplace_back(v1 * scale);
			res.mPositions.emplace_back(v2 * scale);

			res.mNormals.emplace_back(n);
			res.mNormals.emplace_back(n);
			res.mNormals.emplace_back(n);
		}
	}
	else
	{
		U32* idx = (U32*)mesh.mIndexBase;
		for (S32 j = 0; j < mesh.mNumTriangles; ++j)
		{
			F32* mp0 = (F32*)((U8*)v + idx[0] * mesh.mVertexStrideBytes);
			F32* mp1 = (F32*)((U8*)v + idx[1] * mesh.mVertexStrideBytes);
			F32* mp2 = (F32*)((U8*)v + idx[2] * mesh.mVertexStrideBytes);

			idx = (U32*)(((U8*)idx) + mesh.mIndexStrideBytes);

			LLVector3 v0(mp0);
			LLVector3 v1(mp1);
			LLVector3 v2(mp2);

			LLVector3 n = (v1 - v0) % (v2 - v0);
			n.normalize();

			res.mPositions.emplace_back(v0 * scale);
			res.mPositions.emplace_back(v1 * scale);
			res.mPositions.emplace_back(v2 * scale);

			res.mNormals.emplace_back(n);
			res.mNormals.emplace_back(n);
			res.mNormals.emplace_back(n);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
// LLMeshHeader class
///////////////////////////////////////////////////////////////////////////////

LLMeshHeader::LLMeshHeader() noexcept
{
	reset();
}

void LLMeshHeader::reset()
{
	mValid = false;
	mHeaderSize = 0;
	mLodOffset[0] = mLodOffset[1] = mLodOffset[2] = mLodOffset[3] = 0;
	mLodSize[0] = mLodSize[1] = mLodSize[2] = mLodSize[3] = 0;
	mSkinOffset = mSkinSize = 0;
	mPhysicsConvexOffset = mPhysicsConvexSize = 0;
	mPhysicsMeshOffset = mPhysicsMeshSize = 0;
}

void LLMeshHeader::init(const LLSD& header, U32 size)
{
	LL_TRACY_TIMER(TRC_MESH_HEADER_INIT);

	mHeaderSize = size;

	mValid = size && !header.has("404");
	if (!mValid)
	{
		// Invalid mesh header
		return;
	}

	if (header.has("version"))
	{
		mValid = header["version"].asInteger() <= MAX_MESH_VERSION;
		if (!mValid)
		{
			// Invalid mesh header
			return;
		}
	}

	if (header.has("lowest_lod"))
	{
		const LLSD& lod = header["lowest_lod"];
		if (lod.has("offset"))
		{
			mLodOffset[0] = lod["offset"].asInteger();
		}
		mLodOffset[0] += size;
		if (lod.has("size"))
		{
			mLodSize[0] = lod["size"].asInteger();
		}
	}
	if (header.has("low_lod"))
	{
		const LLSD& lod = header["low_lod"];
		if (lod.has("offset"))
		{
			mLodOffset[1] = lod["offset"].asInteger();
		}
		mLodOffset[1] += size;
		if (lod.has("size"))
		{
			mLodSize[1] = lod["size"].asInteger();
		}
	}
	if (header.has("medium_lod"))
	{
		const LLSD& lod = header["medium_lod"];
		if (lod.has("offset"))
		{
			mLodOffset[2] = lod["offset"].asInteger();
		}
		mLodOffset[2] += size;
		if (lod.has("size"))
		{
			mLodSize[2] = lod["size"].asInteger();
		}
	}
	if (header.has("high_lod"))
	{
		const LLSD& lod = header["high_lod"];
		if (lod.has("offset"))
		{
			mLodOffset[3] = lod["offset"].asInteger();
		}
		mLodOffset[3] += size;
		if (lod.has("size"))
		{
			mLodSize[3] = lod["size"].asInteger();
		}
	}

	if (header.has("skin"))
	{
		const LLSD& lod = header["skin"];
		if (lod.has("offset"))
		{
			mSkinOffset = lod["offset"].asInteger();
		}
		mSkinOffset += size;
		if (lod.has("size"))
		{
			mSkinSize = lod["size"].asInteger();
		}
	}

	if (header.has("physics_convex"))
	{
		const LLSD& lod = header["physics_convex"];
		if (lod.has("offset"))
		{
			mPhysicsConvexOffset = lod["offset"].asInteger();
		}
		mPhysicsConvexOffset += size;
		if (lod.has("size"))
		{
			mPhysicsConvexSize = lod["size"].asInteger();
		}
	}

	if (header.has("physics_mesh"))
	{
		const LLSD& lod = header["physics_mesh"];
		if (lod.has("offset"))
		{
			mPhysicsMeshOffset = lod["offset"].asInteger();
		}
		mPhysicsMeshOffset += size;
		if (lod.has("size"))
		{
			mPhysicsMeshSize = lod["size"].asInteger();
		}
	}

	// Header is valid if we found at least one valid LOD in it
	mValid = mLodSize[0] || mLodSize[1] || mLodSize[2] || mLodSize[3];
}

///////////////////////////////////////////////////////////////////////////////
// LLMeshCostData class
///////////////////////////////////////////////////////////////////////////////

LLMeshCostData::LLMeshCostData() noexcept
:	mSizeTotal(0),
	mEstTrisMax(0.f),
	mChargedTris(-1.f)
{
	mSizeByLOD.resize(4, 0);
	mEstTrisByLOD.resize(4, 0.f);
}

bool LLMeshCostData::init(LLMeshHeader* header)
{
	return header &&
		   init(header->mLodSize[0], header->mLodSize[1], header->mLodSize[2],
				header->mLodSize[3]);
}

bool LLMeshCostData::init(const LLSD& header)
{
	LL_TRACY_TIMER(TRC_MESH_COST_INIT);

	S32 bytes_lowest = 0;
	if (header.has("lowest_lod"))
	{
		bytes_lowest = header["lowest_lod"]["size"].asInteger();
	}

	S32 bytes_low = 0;
	if (header.has("low_lod"))
	{
		bytes_low = header["low_lod"]["size"].asInteger();
	}

	S32 bytes_med = 0;
	if (header.has("medium_lod"))
	{
		bytes_med = header["medium_lod"]["size"].asInteger();
	}

	S32 bytes_high = 0;
	if (header.has("high_lod"))
	{
		bytes_high = header["high_lod"]["size"].asInteger();
	}

	return init(bytes_lowest, bytes_low, bytes_med, bytes_high);
}

bool LLMeshCostData::init(S32 bytes_lowest, S32 bytes_low, S32 bytes_med,
						  S32 bytes_high)
{
	if (bytes_high <= 0)
	{
		bytes_high = llmax(0, bytes_med, bytes_low, bytes_lowest);
	}
	if (bytes_high == 0)
	{
		return false;
	}
	mSizeByLOD[3] = bytes_high;

	if (bytes_med <= 0)
	{
		bytes_med = bytes_high;
	}
	mSizeByLOD[2] = bytes_med;

	if (bytes_low <= 0)
	{
		bytes_low = bytes_med;
	}
	mSizeByLOD[1] = bytes_low;

	if (bytes_lowest <= 0)
	{
		bytes_lowest = bytes_low;
	}
	mSizeByLOD[0] = bytes_lowest;

	mSizeTotal = bytes_high + bytes_med + bytes_low + bytes_lowest;

	static LLCachedControl<U32> discount(gSavedSettings,
										 "MeshMetaDataDiscount");
	static LLCachedControl<U32> min_size(gSavedSettings,
										 "MeshMinimumByteSize");
	static LLCachedControl<U32> tri_bytes(gSavedSettings,
										  "MeshBytesPerTriangle");
	F32 max = 0.f;
	F32 bytes_per_tri = (F32)tri_bytes;
	for (S32 i = 0; i < 4; ++i)
	{
		S32 size = mSizeByLOD[i] - discount;
		if (size < (S32)min_size)
		{
			size = min_size;
		}
		F32 tris = (F32)size / bytes_per_tri;
		if (tris > max)
		{
			max = tris;
		}
		mEstTrisByLOD[i] = tris;
	}
	mEstTrisMax = max;

	mChargedTris = -1.f;

	return true;
}

F32 LLMeshCostData::getRadiusWeightedTris(F32 radius)
{
	constexpr F32 MAX_DISTANCE = 512.f;
	constexpr F32 K1 = 1.f / 0.03f;
	constexpr F32 K2 = 1.f / 0.06f;
	constexpr F32 K3 = 1.f / 0.24f;
	F32 dlowest = llmin(radius * K1, MAX_DISTANCE);
	F32 dlow = llmin(radius * K2, MAX_DISTANCE);
	F32 dmid = llmin(radius * K3, MAX_DISTANCE);

	// Area of a circle that encompasses region (see MAINT-6559):
	constexpr F32 MAX_AREA = 102944.f;
	constexpr F32 MIN_AREA = 1.f;

	F32 high_area = llmin(F_PI * dmid * dmid, MAX_AREA);
	F32 mid_area = llmin(F_PI * dlow * dlow, MAX_AREA);
	F32 low_area = llmin(F_PI * dlowest * dlowest, MAX_AREA);
	F32 lowest_area = MAX_AREA;

	lowest_area -= low_area;
	low_area -= mid_area;
	mid_area -= high_area;

	high_area = llclamp(high_area, MIN_AREA, MAX_AREA);
	mid_area = llclamp(mid_area, MIN_AREA, MAX_AREA);
	low_area = llclamp(low_area, MIN_AREA, MAX_AREA);
	lowest_area = llclamp(lowest_area, MIN_AREA, MAX_AREA);

	F32 inv_total_area = 1.f / (high_area + mid_area + low_area + lowest_area);
	high_area *= inv_total_area;
	mid_area *= inv_total_area;
	low_area *= inv_total_area;
	lowest_area *= inv_total_area;

	return mEstTrisByLOD[3] * high_area + mEstTrisByLOD[2] * mid_area +
		   mEstTrisByLOD[1] * low_area + mEstTrisByLOD[0] * lowest_area;
}

F32 LLMeshCostData::getEstTrisForStreamingCost()
{
	if (mChargedTris < 0.f)
	{
		mChargedTris = mEstTrisByLOD[3];
		F32 allowed_tris = mChargedTris;
		constexpr F32 ENFORCE_FLOOR = 64.f;
		for (S32 i = 2; i >= 0; --i)
		{
			// How many tris can we have in this LOD without affecting land
			// impact ?
			// - normally a LOD should be at most half the size of the previous
			//   one
			// - once we reach a floor of ENFORCE_FLOOR, do not require LODs to
			//   get any smaller.
			allowed_tris = llclamp(allowed_tris * 0.5f, ENFORCE_FLOOR,
								   mEstTrisByLOD[i]);
			F32 excess_tris = mEstTrisByLOD[i] - allowed_tris;
			if (excess_tris > 0.f)
			{
				mChargedTris += excess_tris;
			}
		}
	}
	return mChargedTris;
}

F32 LLMeshCostData::getRadiusBasedStreamingCost(F32 radius)
{
	LL_TRACY_TIMER(TRC_MESH_COST_RADIUS);

	static LLCachedControl<U32> budget(gSavedSettings, "MeshTriangleBudget");
	F32 triangle_budget = budget > 0 ? (F32)budget : 250000.f;
	return getRadiusWeightedTris(radius) * 15000.f / triangle_budget;
}

F32 LLMeshCostData::getTriangleBasedStreamingCost()
{
	LL_TRACY_TIMER(TRC_MESH_COST_TRI);

	return ANIMATED_OBJECT_COST_PER_KTRI * 0.001f *
		   getEstTrisForStreamingCost();
}

///////////////////////////////////////////////////////////////////////////////
// LLMeshHandlerBase class
//
// Base handler class for all mesh users of llcorehttp. This is roughly
// equivalent to a Responder class in traditional LL code. The base is going to
// perform common response/data handling in the inherited onCompleted() method.
// Derived classes, one for each type of HTTP action, define processData() and
// processFailure() methods to customize handling and error messages.
//
// LLCore::HttpHandler
//   LLMeshHandlerBase
//     LLMeshHeaderHandler
//     LLMeshLODHandler
//     LLMeshSkinInfoHandler
//     LLMeshDecompositionHandler
//     LLMeshPhysicsShapeHandler
//   LLMeshUploadThread

class LLMeshHandlerBase : public LLCore::HttpHandler,
						  public std::enable_shared_from_this<LLMeshHandlerBase>
{
protected:
	LOG_CLASS(LLMeshHandlerBase);

public:
	typedef std::shared_ptr<LLMeshHandlerBase> ptr_t;

	LLMeshHandlerBase(U32 offset, U32 requested_bytes)
	:	LLCore::HttpHandler(),
		mMeshParams(),
		mProcessed(false),
		mHttpHandle(LLCORE_HTTP_HANDLE_INVALID),
		mOffset(offset),
		mRequestedBytes(requested_bytes)
	{
	}

	~LLMeshHandlerBase() override
	{
	}

	LLMeshHandlerBase(const LLMeshHandlerBase&) = delete;
	void operator=(const LLMeshHandlerBase&) = delete;

public:
	void onCompleted(LLCore::HttpHandle handle,
					 LLCore::HttpResponse* response) override;

	// New virtual methods
	virtual void processData(LLCore::BufferArray* body, S32 body_offset,
							 U8* data, S32 data_size) = 0;
	virtual void processFailure(LLCore::HttpStatus status) = 0;

public:
	LLCore::HttpHandle	mHttpHandle;
	S32					mOffset;
	U32					mRequestedBytes;
	LLVolumeParams		mMeshParams;
	bool				mProcessed;
};

// Subclass for header fetches.
//
// Thread: repo
class LLMeshHeaderHandler final : public LLMeshHandlerBase
{
protected:
	LOG_CLASS(LLMeshHeaderHandler);

public:
	LLMeshHeaderHandler(const LLVolumeParams& mesh_params, U32 offset,
						U32 requested_bytes)
	:	LLMeshHandlerBase(offset, requested_bytes)
	{
		mMeshParams = mesh_params;
		++LLMeshRepoThread::sActiveHeaderRequests;
	}

	~LLMeshHeaderHandler() override;

	LLMeshHeaderHandler(const LLMeshHeaderHandler&) = delete;
	void operator=(const LLMeshHeaderHandler&) = delete;

	void processData(LLCore::BufferArray* body, S32 body_offset, U8* data,
					 S32 data_size) override;
	void processFailure(LLCore::HttpStatus status) override;
};

// Subclass for LOD fetches.
//
// Thread: repo
class LLMeshLODHandler final : public LLMeshHandlerBase
{
protected:
	LOG_CLASS(LLMeshLODHandler);

public:
	LLMeshLODHandler(const LLVolumeParams & mesh_params, S32 lod, U32 offset,
					 U32 requested_bytes)
	:	LLMeshHandlerBase(offset, requested_bytes),
		mLOD(lod)
	{
		mMeshParams = mesh_params;
		++LLMeshRepoThread::sActiveLODRequests;
	}

	~LLMeshLODHandler() override;

	LLMeshLODHandler(const LLMeshLODHandler&) = delete;
	void operator=(const LLMeshLODHandler&) = delete;

	void processData(LLCore::BufferArray* body, S32 body_offset, U8* data,
					 S32 data_size) override;
	void processFailure(LLCore::HttpStatus status) override;

public:
	S32 mLOD;
};

// Subclass for skin info fetches.
//
// Thread: repo
class LLMeshSkinInfoHandler final : public LLMeshHandlerBase
{
protected:
	LOG_CLASS(LLMeshSkinInfoHandler);

public:
	LLMeshSkinInfoHandler(const LLUUID& id, U32 offset, U32 requested_bytes)
	:	LLMeshHandlerBase(offset, requested_bytes),
		mMeshID(id)
	{
	}

	~LLMeshSkinInfoHandler() override;

	LLMeshSkinInfoHandler(const LLMeshSkinInfoHandler&) = delete;
	void operator=(const LLMeshSkinInfoHandler&) = delete;

	void processData(LLCore::BufferArray* body, S32 body_offset, U8* data,
					 S32 data_size) override;
	void processFailure(LLCore::HttpStatus status) override;

public:
	LLUUID mMeshID;
};

// Subclass for decomposition fetches.
//
// Thread: repo
class LLMeshDecompositionHandler final : public LLMeshHandlerBase
{
protected:
	LOG_CLASS(LLMeshDecompositionHandler);

public:
	LLMeshDecompositionHandler(const LLUUID& id, U32 offset,
							   U32 requested_bytes)
	:	LLMeshHandlerBase(offset, requested_bytes),
		mMeshID(id)
	{
	}

	~LLMeshDecompositionHandler() override;

	LLMeshDecompositionHandler(const LLMeshDecompositionHandler&) = delete;
	void operator=(const LLMeshDecompositionHandler&) = delete;

	void processData(LLCore::BufferArray* body, S32 body_offset, U8* data,
					 S32 data_size) override;
	void processFailure(LLCore::HttpStatus status) override;

public:
	LLUUID mMeshID;
};

// Subclass for physics shape fetches.
//
// Thread: repo
class LLMeshPhysicsShapeHandler final : public LLMeshHandlerBase
{
protected:
	LOG_CLASS(LLMeshPhysicsShapeHandler);

public:
	LLMeshPhysicsShapeHandler(const LLUUID& id, U32 offset,
							  U32 requested_bytes)
	:	LLMeshHandlerBase(offset, requested_bytes),
		mMeshID(id)
	{
	}

	~LLMeshPhysicsShapeHandler() override;

	LLMeshPhysicsShapeHandler(const LLMeshPhysicsShapeHandler&) = delete;
	void operator=(const LLMeshPhysicsShapeHandler&) = delete;

	void processData(LLCore::BufferArray* body, S32 body_offset, U8* data,
					 S32 data_size) override;
	void processFailure(LLCore::HttpStatus status) override;

public:
	LLUUID mMeshID;
};

LLMeshRepoThread::LLMeshRepoThread()
:	LLThread("Mesh repository"),
	mHttpPolicyClass(LLCore::HttpRequest::DEFAULT_POLICY_ID),
	mHttpLegacyPolicyClass(LLCore::HttpRequest::DEFAULT_POLICY_ID),
	mHttpLargePolicyClass(LLCore::HttpRequest::DEFAULT_POLICY_ID),
	mGetMeshVersion(2)
{
	mHttpRequest = new LLCore::HttpRequest;
	mHttpOptions = DEFAULT_HTTP_OPTIONS;
	mHttpOptions->setTransferTimeout(SMALL_MESH_XFER_TIMEOUT);
	bool use_retyr_after = gSavedSettings.getBool("MeshUseHttpRetryAfter");
	mHttpOptions->setUseRetryAfter(use_retyr_after);
	mHttpLargeOptions = DEFAULT_HTTP_OPTIONS;
	mHttpLargeOptions->setTransferTimeout(LARGE_MESH_XFER_TIMEOUT);
	mHttpLargeOptions->setUseRetryAfter(use_retyr_after);
	mHttpHeaders = DEFAULT_HTTP_HEADERS;
	mHttpHeaders->append(HTTP_OUT_HEADER_ACCEPT, HTTP_CONTENT_VND_LL_MESH);
	LLAppCoreHttp& app_core_http= gAppViewerp->getAppCoreHttp();
	mHttpPolicyClass = app_core_http.getPolicy(LLAppCoreHttp::AP_MESH2);
	mHttpLegacyPolicyClass = app_core_http.getPolicy(LLAppCoreHttp::AP_MESH1);
	mHttpLargePolicyClass =
		app_core_http.getPolicy(LLAppCoreHttp::AP_LARGE_MESH);
}

LLMeshRepoThread::~LLMeshRepoThread()
{
	llinfos << "Small GETs issued: " << LLMeshRepository::sHTTPRequestCount
			<< " - Large GETs issued: "
			<< LLMeshRepository::sHTTPLargeRequestCount
			<< " - Max lock holdoffs: " << LLMeshRepository::sMaxLockHoldoffs
			<< " - Total mesh headers stored: " << mMeshHeaders.size()
			<< llendl;

	mHttpRequestSet.clear();
	mHttpHeaders.reset();

	for (skin_info_list_t::iterator it = mSkinInfos.begin(),
									end = mSkinInfos.end();
		 it != end; ++it)
	{
		delete *it;
	}
	mSkinInfos.clear();

	for (decomp_list_t::iterator it = mDecompositions.begin(),
								 end = mDecompositions.end();
		 it != end; ++it)
	{
		delete *it;
	}
	mDecompositions.clear();

	delete mHttpRequest;
	mHttpRequest = NULL;

	for (auto it = mMeshHeaders.begin(), end = mMeshHeaders.end(); it != end;
		 ++it)
	{
		delete it->second;
	}
	mMeshHeaders.clear();
}

// Note: this method is written in such a way that it holds mMutex for the
// shortest possible amount of time. HB
void LLMeshRepoThread::insertRequests(base_requests_set_t& dest,
									  base_requests_set_t& remaining,
									  base_requests_set_t& incomplete)
{
	if (!remaining.empty())
	{
		if (incomplete.empty())
		{
			incomplete.swap(remaining);	// Fastest !
		}
		else
		{
			// No range-emplacing: iterate so to use emplace()
			for (base_requests_set_t::iterator it = remaining.begin(),
											   end = remaining.end();
				 it != end; ++it)
			{
				incomplete.emplace(*it);
			}
			remaining.clear();
		}
	}

	if (incomplete.empty())
	{
		return;
	}

	mMutex.lock();
	if (dest.empty())
	{
		dest.swap(incomplete);	// Fastest !
		mMutex.unlock();
	}
	else
	{
		// No range-emplacing: iterate so to use emplace()
		for (base_requests_set_t::iterator it = incomplete.begin(),
										   end = incomplete.end();
			 it != end; ++it)
		{
			dest.emplace(*it);
		}
		mMutex.unlock();
		incomplete.clear();
	}
}

void LLMeshRepoThread::run()
{
	if (LLConvexDecomposition::initThread() != LLCD_OK)
	{
		llwarns << "Unable to start the convex decomposition thread" << llendl;
	}

	lod_req_queue_t incomplete_lod, lodq_copy;
	header_req_queue_t incomplete_hdr, hdrq_copy;
	base_requests_set_t incomplete_req, requests_copy;
	bool can_req;
	while (!LLApp::isExiting())
	{
		// *TODO: Revise sleep/wake strategy and try to move away from polling
		// operations in this thread. We can sleep this thread hard when:
		// * All Http requests are serviced
		// * LOD request queue empty
		// * Header request queue empty
		// * Skin info request queue empty
		// * Decomposition request queue empty
		// * Physics shape request queue empty
		// We wake the thread when any of the above become untrue. Will likely
		// need a correctly-implemented condition variable to do this. On the
		// other hand, this may actually be an effective and efficient scheme.

		mSignal.wait();

		if (LLApp::isExiting())
		{
			break;
		}

		{
			LL_TRACY_TIMER(TRC_MESH_THREAD_UDPATE);

			if (!mHttpRequestSet.empty())
			{
				// Dispatch all HttpHandler notifications
				mHttpRequest->update(0L);
			}
			// Stats data update
			sRequestWaterLevel = mHttpRequestSet.size();
			can_req = sRequestWaterLevel < sRequestHighWater;
		}

		// NOTE: order of queue processing intentionally favors skin requests
		// over LOD requests, so to avoid as much as possible the "floating
		// body parts" during rigged meshes rezzing. HB

		if (can_req && !mSkinRequests.empty())
		{
			LL_TRACY_TIMER(TRC_MESH_THREAD_SKIN);

			// Swap the container with an empty local copy to avoid locking at
			// each iteration and slowing down the main thread as a result. HB
			mMutex.lock();
			requests_copy.swap(mSkinRequests);
			mMutex.unlock();

			do
			{
				base_requests_set_t::iterator iter = requests_copy.begin();
				UUIDBasedRequest req = *iter;
				if (req.isDelayed())
				{
					incomplete_req.insert(req);
				}
				else
				{
					bool can_retry = req.canRetry();
					if (!fetchMeshSkinInfo(req.mId, can_retry))
					{
						if (can_retry)
						{
							req.updateTime();
							incomplete_req.insert(req);
						}
						else
						{
							// Note: at this point, the skin UUID has already
							// been added to mUnavailableSkins during the above
							// fetchMeshSkinInfo() call. HB
							llwarns << "Skin request failed for " << req.mId
									<< llendl;
						}
					}
				}
				can_req = (S32)mHttpRequestSet.size() < sRequestHighWater;
				requests_copy.erase(iter);
			}
			while (can_req && !requests_copy.empty());

			// Note: this always leaves requests_copy and incomplete_req empty
			insertRequests(mSkinRequests, requests_copy, incomplete_req);
		}

		// NOTE: order of queue processing intentionally favors LOD requests
		// over header requests

		if (can_req && !mLODReqQ.empty())
		{
			LL_TRACY_TIMER(TRC_MESH_THREAD_LOD);

			// Swap the container with an empty local copy to avoid locking at
			// each iteration and slowing down the main thread as a result. HB
			mMutex.lock();
			lodq_copy.swap(mLODReqQ);
			mMutex.unlock();

			do
			{
				LODRequest& req = lodq_copy.front();
				if (req.isDelayed())
				{
					// Still need to wait a bit before retrying
					incomplete_lod.emplace_back(req);
				}
				else
				{
					--LLMeshRepository::sLODProcessing;
					bool can_retry = req.canRetry();
					if (!fetchMeshLOD(req.mMeshParams, req.mLOD, can_retry))
					{
						if (can_retry)
						{
							// Failed, resubmit
							req.updateTime();
							incomplete_lod.emplace_back(req);
							++LLMeshRepository::sLODProcessing;
						}
						else
						{
							// Note: at this point, the request has already
							// been added to mUnavailableLODs during the above
							// fetchMeshLOD() call. HB
							llwarns << "Failed to load " << req.mMeshParams
									<< ", skipping." << llendl;
						}
					}
				}
				lodq_copy.pop_front();
				can_req = (S32)mHttpRequestSet.size() < sRequestHighWater;
			}
			while (can_req && !lodq_copy.empty());

			// If we could not process all the requests; push them into the
			// incomplete queue.
			if (!lodq_copy.empty())
			{
				if (incomplete_lod.empty())
				{
					incomplete_lod.swap(lodq_copy);	// Fastest !
				}
				else
				{
					// Note: there is no range-emplacing for deque, so let's
					// iterate to emplace_front()
					for (lod_req_queue_t::reverse_iterator
							rit = lodq_copy.rend(),
							begin = lodq_copy.rbegin();
						 rit != begin; --rit)
					{
						incomplete_lod.emplace_front(*rit);
					}
					lodq_copy.clear();
				}
			}

			if (!incomplete_lod.empty())
			{
				mMutex.lock();
				if (mLODReqQ.empty())
				{
					mLODReqQ.swap(incomplete_lod);	// Fastest !
					mMutex.unlock();
				}
				else
				{
					// Note: there is no range-emplacing for deque, so let's
					// iterate to emplace_front()
					for (lod_req_queue_t::reverse_iterator
							rit = incomplete_lod.rend(),
							begin = incomplete_lod.rbegin();
						 rit != begin; --rit)
					{
						mLODReqQ.emplace_front(*rit);
					}
					mMutex.unlock();
					incomplete_lod.clear();
				}
			}
		}

		if (can_req && !mHeaderReqQ.empty())
		{
			LL_TRACY_TIMER(TRC_MESH_THREAD_HEADER);

			// Swap the container with an empty local copy to avoid locking at
			// each iteration and slowing down the main thread as a result. HB
			mMutex.lock();
			hdrq_copy.swap(mHeaderReqQ);
			mMutex.unlock();

			do
			{
				HeaderRequest& req = hdrq_copy.front();
				if (req.isDelayed())
				{
					// Still need to wait a bit before retrying
					incomplete_hdr.emplace_back(req);
				}
				else
				{
					bool can_retry = req.canRetry();
					if (!fetchMeshHeader(req.mMeshParams, can_retry))
					{
						if (can_retry)
						{
							// Failed, resubmit
							req.updateTime();
							incomplete_hdr.emplace_back(req);
						}
						else
						{
							llwarns << "Failed to load header "
									<< req.mMeshParams << ", skipping."
									<< llendl;
						}
					}
				}
				hdrq_copy.pop_front();
				can_req = (S32)mHttpRequestSet.size() < sRequestHighWater;
			}
			while (can_req && !hdrq_copy.empty());

			// If we could not process all the requests; push them into the
			// incomplete queue.
			if (!hdrq_copy.empty())
			{
				if (incomplete_hdr.empty())
				{
					incomplete_hdr.swap(hdrq_copy);	// Fastest !
				}
				else
				{
					// Note: there is no range-emplacing for deque, so let's
					// iterate to emplace_front()
					for (header_req_queue_t::reverse_iterator
							rit = hdrq_copy.rend(),
							begin = hdrq_copy.rbegin();
						 rit != begin; --rit)
					{
						incomplete_hdr.emplace_front(*rit);
					}
					hdrq_copy.clear();
				}
			}

			if (!incomplete_hdr.empty())
			{
				mMutex.lock();
				if (mHeaderReqQ.empty())
				{
					mHeaderReqQ.swap(incomplete_hdr);	// Fastest !
					mMutex.unlock();
				}
				else
				{
					// Note: there is no range-emplacing for deque, so let's
					// iterate to emplace_front()
					for (header_req_queue_t::reverse_iterator
							rit = incomplete_hdr.rend(),
							begin = incomplete_hdr.rbegin();
						 rit != begin; --rit)
					{
						mHeaderReqQ.emplace_front(*rit);
					}
					mMutex.unlock();
					incomplete_hdr.clear();
				}
			}
		}

		// For the final two request sets, similar goal to above but slightly
		// different queue structures. Stay off the mutex when performing long
		// duration actions.

		if (can_req && !mDecompositionRequests.empty())
		{
			LL_TRACY_TIMER(TRC_MESH_THREAD_DECOMP);

			// Swap the container with an empty local copy to avoid locking at
			// each iteration and slowing down the main thread as a result. HB
			mMutex.lock();
			requests_copy.swap(mDecompositionRequests);
			mMutex.unlock();

			do
			{
				base_requests_set_t::iterator iter = requests_copy.begin();
				UUIDBasedRequest req = *iter;
				if (req.isDelayed())
				{
					incomplete_req.insert(req);
				}
				else if (!fetchMeshDecomposition(req.mId))
				{
					if (req.canRetry())
					{
						req.updateTime();
						incomplete_req.insert(req);
					}
					else
					{
						llwarns << "Decomp request failed for " << req.mId
								<< llendl;
					}
				}
				requests_copy.erase(iter);
				can_req = (S32)mHttpRequestSet.size() < sRequestHighWater;
			}
			while (can_req && !requests_copy.empty());

			// Note: this always leaves requests_copy and incomplete_req empty
			insertRequests(mDecompositionRequests, requests_copy,
						   incomplete_req);
		}

		if (can_req && !mPhysicsShapeRequests.empty())
		{
			LL_TRACY_TIMER(TRC_MESH_THREAD_PHYSICS);

			// Swap the container with an empty local copy to avoid locking at
			// each iteration and slowing down the main thread as a result. HB
			mMutex.lock();
			requests_copy.swap(mPhysicsShapeRequests);
			mMutex.unlock();

			do
			{
				base_requests_set_t::iterator iter = requests_copy.begin();
				UUIDBasedRequest req = *iter;
				if (req.isDelayed())
				{
					incomplete_req.insert(req);
				}
				else if (!fetchMeshPhysicsShape(req.mId))
				{
					if (req.canRetry())
					{
						req.updateTime();
						incomplete_req.insert(req);
					}
					else
					{
						llwarns << "Physics shape request failed for "
								<< req.mId << llendl;
					}
				}
				requests_copy.erase(iter);
				can_req = (S32)mHttpRequestSet.size() < sRequestHighWater;
			}
			while (can_req && !requests_copy.empty());

			// Note: this always leaves requests_copy and incomplete_req empty
			insertRequests(mPhysicsShapeRequests, requests_copy,
						   incomplete_req);
		}
	}

	if (mSignal.isLocked())
	{
		// Make sure to let go off the mutex associated with the given signal
		// before shutting down
		mSignal.unlock();
	}

	if (LLConvexDecomposition::quitThread() != LLCD_OK)
	{
		llwarns << "Unable to quit convex decomposition thread" << llendl;
	}
}

void LLMeshRepoThread::loadMeshSkinInfo(const LLUUID& mesh_id)
{
	mSkinRequests.emplace(mesh_id);
}

void LLMeshRepoThread::loadMeshDecomposition(const LLUUID& mesh_id)
{
	mDecompositionRequests.emplace(mesh_id);
}

void LLMeshRepoThread::loadMeshPhysicsShape(const LLUUID& mesh_id)
{
	mPhysicsShapeRequests.emplace(mesh_id);
}

void LLMeshRepoThread::lockAndLoadMeshLOD(const LLVolumeParams& mesh_params,
										  S32 lod)
{
	if (!LLAppViewer::isExiting())
	{
		// Could be called from any thread
		mMutex.lock();
		loadMeshLOD(mesh_params, lod);
		mMutex.unlock();
	}
}

void LLMeshRepoThread::loadMeshLOD(const LLVolumeParams& mesh_params, S32 lod)
{
	const LLUUID& mesh_id = mesh_params.getSculptID();
	mHeaderMutex.lock();
	bool exists = mMeshHeaders.count(mesh_id) > 0;
	mHeaderMutex.unlock();

	if (exists)
	{
		// If we have the header, request LOD byte range
		mLODReqQ.emplace_back(mesh_params, lod);
		++LLMeshRepository::sLODProcessing;
	}
	else
	{
		HeaderRequest req(mesh_params);
		pending_lod_map_t::iterator pending = mPendingLOD.find(mesh_id);
		if (pending != mPendingLOD.end())
		{
			// Append this lod request to existing header request
			pending->second.push_back(lod);
			llassert(pending->second.size() <= LLModel::NUM_LODS);
		}
		else
		{
			// If no header request is pending, fetch header
			mHeaderReqQ.emplace_back(req);
			mPendingLOD[mesh_id].push_back(lod);
		}
	}
}

// Constructs a capability URL for the mesh.
// Mutex: acquires mMutex
std::string LLMeshRepoThread::constructUrl(const LLUUID& mesh_id, U32* version)
{
	mMutex.lock();
	const std::string& http_url = mGetMeshCapability;
	*version = mGetMeshVersion;
	mMutex.unlock();

	if (http_url.empty())
	{
		llwarns << "Current region does not have GetMesh capability, cannot fetch mesh Id: "
				<< mesh_id << llendl;
		return "";
	}

	return http_url + "?mesh_id=" + mesh_id.asString();
}

// Issue an HTTP GET request with byte range using the right policy class.
// Large requests go to the large request class. If the current region supports
// GetMesh2, we prefer that for smaller requests otherwise we try to use the
// traditional GetMesh capability and connection concurrency.
//
// @return	Valid handle or LLCORE_HTTP_HANDLE_INVALID.
//			If the latter, actual status is found in mHttpStatus member which
//			is valid until the next call to this method.
//
// Thread: repo
LLCore::HttpHandle LLMeshRepoThread::getByteRange(const std::string& url,
												  U32 cap_version,
												  size_t offset, size_t len,
												  const LLCore::HttpHandler::ptr_t& handler)
{
	static LLCachedControl<bool> disable_range_req(gSavedSettings,
												   "HttpRangeRequestsDisable");
	size_t req_offset = disable_range_req ? 0 : offset;
	size_t req_len = disable_range_req ? 0 : len;

	LLCore::HttpHandle handle = LLCORE_HTTP_HANDLE_INVALID;

	if (len < LARGE_MESH_FETCH_THRESHOLD)
	{
		handle = mHttpRequest->requestGetByteRange(cap_version == 2 ? mHttpPolicyClass
																	: mHttpLegacyPolicyClass,
												   url, req_offset, req_len,
												   mHttpOptions, mHttpHeaders,
												   handler);
		if (handle != LLCORE_HTTP_HANDLE_INVALID)
		{
			++LLMeshRepository::sHTTPRequestCount;
		}
	}
	else
	{
		handle = mHttpRequest->requestGetByteRange(mHttpLargePolicyClass, url,
												   req_offset, req_len,
												   mHttpLargeOptions,
												   mHttpHeaders, handler);
		if (handle != LLCORE_HTTP_HANDLE_INVALID)
		{
			++LLMeshRepository::sHTTPLargeRequestCount;
		}
	}
	if (handle != LLCORE_HTTP_HANDLE_INVALID)
	{
		// Something went wrong, capture the error code for caller.
		mHttpStatus = mHttpRequest->getStatus();
	}
	return handle;
}

bool LLMeshRepoThread::fetchMeshSkinInfo(const LLUUID& mesh_id, bool can_retry)
{
	mHeaderMutex.lock();

	mesh_header_map_t::iterator iter = mMeshHeaders.find(mesh_id);
	if (iter == mMeshHeaders.end())
	{
		// We have no header info for this mesh, do nothing
		mHeaderMutex.unlock();
		return false;
	}
	LLMeshHeader* header = iter->second;

	++LLMeshRepository::sMeshRequestCount;

	if (!header->mHeaderSize)
	{
		mHeaderMutex.unlock();
		// Early out was not hit, effectively fetched
		return true;
	}

	bool valid = header->mValid;
	S32 offset = header->mSkinOffset;
	S32 size = header->mSkinSize;

	mHeaderMutex.unlock();

	if (valid && offset >= 0 && size > 0)
	{
		// Check cache for mesh skin info
		LLFileSystem file(mesh_id);
		if (file.getSize() >= offset + size)
		{
			U8* buffer = new(std::nothrow) U8[size];
			if (!buffer)
			{
				LLMemory::allocationFailed(size);
				llwarns << "Could not allocate enough memory. Aborted."
						<< llendl;
				return false;
			}
			LLMeshRepository::sCacheBytesRead += size;
			++LLMeshRepository::sCacheReads;
			file.seek(offset);
			file.read(buffer, size);

			// Make sure the buffer is not all zeros by checking the first 128
			// bytes (reserved block but not written)
			bool zero = true;
			for (S32 i = 0, count = llmin(size, 128); i < count && zero; ++i)
			{
				zero = buffer[i] == 0;
			}

			if (!zero)
			{
				// Attempt to parse
				if (skinInfoReceived(mesh_id, buffer, size))
				{
					delete[] buffer;
					return true;
				}
			}

			delete[] buffer;
		}

		// Reading from cache failed for whatever reason, fetch from server
		U32 cap_version = 2;
		std::string http_url = constructUrl(mesh_id, &cap_version);
		if (http_url.empty())
		{
			mMutex.lock();
			mUnavailableSkins.emplace_back(mesh_id);
			mMutex.unlock();
		}
		else
		{
			LLMeshHandlerBase::ptr_t handler(new LLMeshSkinInfoHandler(mesh_id,
																	   offset,
																	   size));
			LLCore::HttpHandle handle =
				getByteRange(http_url, cap_version, offset, size, handler);
			if (handle == LLCORE_HTTP_HANDLE_INVALID)
			{
				llwarns << "HTTP GET request failed for skin info on mesh "
						<< mID << ". Reason: " << mHttpStatus.toString()
						<< " (" << mHttpStatus.toTerseString() << ")"
						<< llendl;
				if (!can_retry)
				{
					mMutex.lock();
					mUnavailableSkins.emplace_back(mesh_id);
					mMutex.unlock();
				}
				return false;
			}

			handler->mHttpHandle = handle;
			// Do not use handler after this line !
			mHttpRequestSet.emplace(std::move(handler));
		}
	}

	return true;
}

bool LLMeshRepoThread::fetchMeshDecomposition(const LLUUID& mesh_id)
{
	mHeaderMutex.lock();

	mesh_header_map_t::iterator iter = mMeshHeaders.find(mesh_id);
	if (iter == mMeshHeaders.end())
	{
		// We have no header info for this mesh, do nothing
		mHeaderMutex.unlock();
		return false;
	}
	LLMeshHeader* header = iter->second;

	++LLMeshRepository::sMeshRequestCount;

	if (!header->mHeaderSize)
	{
		mHeaderMutex.unlock();
		// Early out was not hit, effectively fetched
		return true;
	}

	bool valid = header->mValid;
	S32 offset = header->mPhysicsConvexOffset;
	S32 size = header->mPhysicsConvexSize;

	mHeaderMutex.unlock();

	if (valid && offset >= 0 && size > 0)
	{
		// Check cache for mesh skin info
		LLFileSystem file(mesh_id);
		if (file.getSize() >= offset + size)
		{
			U8* buffer = new(std::nothrow) U8[size];
			if (!buffer)
			{
				LLMemory::allocationFailed(size);
				llwarns << "Could not allocate enough memory. Aborted."
						<< llendl;
				return false;
			}
			LLMeshRepository::sCacheBytesRead += size;
			++LLMeshRepository::sCacheReads;
			file.seek(offset);
			file.read(buffer, size);

			// Make sure the buffer is not all zeros by checking the first 128
			// bytes (reserved block but not written)
			bool zero = true;
			for (S32 i = 0, count = llmin(size, 128); i < count && zero; ++i)
			{
				zero = buffer[i] == 0;
			}

			if (!zero)
			{
				// Attempt to parse
				if (decompositionReceived(mesh_id, buffer, size))
				{
					delete[] buffer;
					return true;
				}
			}

			delete[] buffer;
		}

		// Reading from cache failed for whatever reason, fetch from sim
		U32 cap_version = 2;
		std::string http_url = constructUrl(mesh_id, &cap_version);
		if (!http_url.empty())
		{
			LLMeshHandlerBase::ptr_t
				handler(new LLMeshDecompositionHandler(mesh_id, offset, size));
			LLCore::HttpHandle handle =
				getByteRange(http_url, cap_version, offset, size, handler);
			if (handle == LLCORE_HTTP_HANDLE_INVALID)
			{
				llwarns << "HTTP GET request failed for decomposition mesh "
						<< mID << " - Reason: " << mHttpStatus.toString()
						<< " (" << mHttpStatus.toTerseString() << ")"
							<< llendl;
				return false;
			}

			handler->mHttpHandle = handle;
			// Do not use handler after this line !
			mHttpRequestSet.emplace(std::move(handler));
		}
	}

	return true;
}

bool LLMeshRepoThread::fetchMeshPhysicsShape(const LLUUID& mesh_id)
{
	mHeaderMutex.lock();

	mesh_header_map_t::iterator iter = mMeshHeaders.find(mesh_id);
	if (iter == mMeshHeaders.end())
	{
		// We have no header info for this mesh, do nothing
		mHeaderMutex.unlock();
		return false;
	}
	LLMeshHeader* header = iter->second;

	++LLMeshRepository::sMeshRequestCount;

	if (!header->mHeaderSize)
	{
		mHeaderMutex.unlock();
		// Early out was not hit, effectively fetched
		return true;
	}

	bool valid = header->mValid;
	S32 offset = header->mPhysicsMeshOffset;
	S32 size = header->mPhysicsMeshSize;

	mHeaderMutex.unlock();

	if (valid && offset >= 0 && size > 0)
	{
		// Check cache for mesh physics shape info
		LLFileSystem file(mesh_id);
		if (file.getSize() >= offset + size)
		{
			U8* buffer = new(std::nothrow) U8[size];
			if (!buffer)
			{
				LLMemory::allocationFailed(size);
				llwarns << "Could not allocate enough memory. Aborted."
						<< llendl;
				return false;
			}
			LLMeshRepository::sCacheBytesRead += size;
			++LLMeshRepository::sCacheReads;
			file.seek(offset);
			file.read(buffer, size);

			// Make sure the buffer is not all zeros by checking the first 128
			// bytes (reserved block but not written)
			bool zero = true;
			for (S32 i = 0, count = llmin(size, 128); i < count && zero; ++i)
			{
				zero = buffer[i] == 0;
			}

			if (!zero)
			{
				// Attempt to parse
				if (physicsShapeReceived(mesh_id, buffer, size))
				{
					delete[] buffer;
					return true;
				}
			}

			delete[] buffer;
		}

		// Reading from cache failed for whatever reason, fetch from sim
		U32 cap_version = 2;
		std::string http_url = constructUrl(mesh_id, &cap_version);
		if (!http_url.empty())
		{
			LLMeshHandlerBase::ptr_t
				handler(new LLMeshPhysicsShapeHandler(mesh_id, offset, size));
			LLCore::HttpHandle handle =
				getByteRange(http_url, cap_version, offset, size, handler);
			if (handle == LLCORE_HTTP_HANDLE_INVALID)
			{
				llwarns << "HTTP GET request failed for physics shape on mesh "
						<< mID << " - Reason: " << mHttpStatus.toString()
						<< " (" << mHttpStatus.toTerseString() << ")"
						<< llendl;
				return false;
			}

			handler->mHttpHandle = handle;
			// Do not use handler after this line !
			mHttpRequestSet.emplace(std::move(handler));
		}
	}
	else
	{
		// No physics shape whatsoever, report back NULL
		physicsShapeReceived(mesh_id, NULL, 0);
	}

	return true;
}

// Returns false if failed to get header
bool LLMeshRepoThread::fetchMeshHeader(const LLVolumeParams& mesh_params,
									   bool can_retry)
{
	++LLMeshRepository::sMeshRequestCount;

	// Look for mesh in asset in cache
	LLFileSystem file(mesh_params.getSculptID());
	S32 size = file.getSize();
	if (size > 0)
	{
		// NOTE: if the header size is ever more than 4KB, this will break
		U8 buffer[MESH_HEADER_SIZE];
		S32 bytes = llmin(size, MESH_HEADER_SIZE);
		LLMeshRepository::sCacheBytesRead += bytes;
		++LLMeshRepository::sCacheReads;
		file.read(buffer, bytes);
		if (headerReceived(mesh_params, buffer, bytes))
		{
			// Found mesh in cache
			return true;
		}
	}

	// Either cache entry does not exist or is corrupt, request header from
	// simulator
	U32 cap_version = 2;
	std::string http_url = constructUrl(mesh_params.getSculptID(),
										&cap_version);
	if (!http_url.empty())
	{
		// Grab first 4KB if we are going to bother with a fetch. Cache will
		// prevent future fetches if a full mesh fits within the first 4KB
		// NOTE: this will break of headers ever exceed 4KB
		LLMeshHandlerBase::ptr_t
			handler(new LLMeshHeaderHandler(mesh_params, 0, MESH_HEADER_SIZE));
		LLCore::HttpHandle handle =
			getByteRange(http_url, cap_version, 0, MESH_HEADER_SIZE, handler);
		if (handle == LLCORE_HTTP_HANDLE_INVALID)
		{
			llwarns << "HTTP GET request failed for mesh header " << mID
					<< " - Reason: " << mHttpStatus.toString() << " ("
					<< mHttpStatus.toTerseString() << ")" << llendl;
			return false;
		}

		if (can_retry)
		{
			handler->mHttpHandle = handle;
			// Do not use handler after this line !
			mHttpRequestSet.emplace(std::move(handler));
		}
	}

	return true;
}

// Returns false if failed to get mesh lod.
bool LLMeshRepoThread::fetchMeshLOD(const LLVolumeParams& mesh_params, S32 lod,
									bool can_retry)
{
	if (lod < 0)
	{
		return false;
	}

	mHeaderMutex.lock();

	++LLMeshRepository::sMeshRequestCount;

	const LLUUID& mesh_id = mesh_params.getSculptID();
	mesh_header_map_t::iterator iter = mMeshHeaders.find(mesh_id);
	if (iter == mMeshHeaders.end())
	{
		// We have no header info for this mesh, do nothing
		mHeaderMutex.unlock();
		return false;
	}
	LLMeshHeader* header = iter->second;

	if (!header->mHeaderSize)
	{
		mHeaderMutex.unlock();
		// Early out was not hit, effectively fetched
		return true;
	}

	bool valid = header->mValid;
	S32 offset = header->mLodOffset[lod];
	S32 size = header->mLodSize[lod];

	mHeaderMutex.unlock();

	bool available_lod = valid && offset >= 0 && size > 0;
	if (available_lod)
	{
		// Check cache for mesh asset
		LLFileSystem file(mesh_id);
		if (file.getSize() >= offset + size)
		{
			U8* buffer = new(std::nothrow) U8[size];
			if (!buffer)
			{
				LLMemory::allocationFailed(size);
				llwarns << "Could not allocate enough memory. Aborted."
						<< llendl;
				return false;
			}
			LLMeshRepository::sCacheBytesRead += size;
			++LLMeshRepository::sCacheReads;
			file.seek(offset);
			file.read(buffer, size);

			// Make sure the buffer is not all zeros by checking the first 128
			// bytes (reserved block but not written)
			bool zero = true;
			for (S32 i = 0, count = llmin(size, 128); i < count && zero; ++i)
			{
				zero = buffer[i] == 0;
			}

			if (!zero)
			{
				// Attempt to parse
				if (lodReceived(mesh_params, lod, buffer, size))
				{
					delete[] buffer;
					return true;
				}
			}

			delete[] buffer;
		}

		// Reading from cache failed for whatever reason, fetch from sim
		U32 cap_version = 2;
		std::string http_url = constructUrl(mesh_id, &cap_version);
		available_lod = !http_url.empty();
		if (available_lod)
		{
			LLMeshHandlerBase::ptr_t handler(new LLMeshLODHandler(mesh_params,
																  lod, offset,
																  size));
			LLCore::HttpHandle handle =
				getByteRange(http_url, cap_version, offset, size, handler);
			if (handle == LLCORE_HTTP_HANDLE_INVALID)
			{
				llwarns << "HTTP GET request failed for LOD on mesh " << mID
						<< " - Reason: " << mHttpStatus.toString() << " ("
						<< mHttpStatus.toTerseString() << ")" << llendl;
				return false;
			}

			if (can_retry)
			{
				handler->mHttpHandle = handle;
				// Do not use handler after this line !
				mHttpRequestSet.emplace(std::move(handler));
			}
			else
			{
				available_lod = false;
			}
		}
	}
	if (!available_lod)
	{
		mMutex.lock();
		mUnavailableLODs.emplace_back(mesh_params, lod);
		mMutex.unlock();
	}

	return true;
}

bool LLMeshRepoThread::headerReceived(const LLVolumeParams& mesh_params,
									  U8* data, S32 data_size)
{
	const LLUUID& mesh_id = mesh_params.getSculptID();

	LLSD header;

	U32 header_size = 0;
	if (data && data_size > 0)
	{
		static char deprecated_header[] = "<? LLSD/Binary ?>";
		static size_t deprecated_header_size = strlen(deprecated_header);
		if (!strncmp((char*)data, deprecated_header, deprecated_header_size))
		{
			data += deprecated_header_size;
			data_size -= deprecated_header_size;
		}
		boost::iostreams::stream<boost::iostreams::array_source>
			stream((char*)data, data_size);
		if (!LLSDSerialize::fromBinary(header, stream, data_size))
		{
			llwarns << "Parse error for header of mesh " << mesh_id
					<< ". Not a valid mesh asset !" << llendl;
			return false;
		}

#if 0	// OpenSIM servers do not serve a 'version' for meshes...
		if (!header.isMap() || !header.has("version"))
#else
		if (!header.isMap())
#endif
		{
			llwarns << "Mesh header is invalid for mesh: " << mesh_id
					<< llendl;
			return false;
		}

		header_size += stream.tellg();
	}
	else
	{
		llwarns << "Marking header for mesh " << mesh_id
				<< " as non-existent, will not retry." << llendl;
		header["404"] = 1;
	}

	mHeaderMutex.lock();
	LLMeshHeader* mesh_header;
	mesh_header_map_t::iterator it = mMeshHeaders.find(mesh_id);
	if (it == mMeshHeaders.end())
	{
		mesh_header = new LLMeshHeader();
		mMeshHeaders.emplace(mesh_id, mesh_header);
	}
	else // This should not happen, but just in case...
	{
		LL_DEBUGS("MeshCost") << "Refreshing mesh header data for mesh Id: "
							  << mesh_id << LL_ENDL;
		mesh_header = it->second;
		mesh_header->reset();
	}
	mesh_header->init(header, header_size);
	LLMeshRepository::mesh_costs_map_t::iterator cit =
		gMeshRepo.mCostsMap.find(mesh_id);
	if (cit != gMeshRepo.mCostsMap.end())
	{
		// This should not happen, but just in case...
		LL_DEBUGS("MeshCost") << "Refreshing mesh costs data for mesh Id: "
							  << mesh_id << LL_ENDL;
		cit->second->init(header);
	}
	mHeaderMutex.unlock();

	// Make sure only one thread access mPendingLOD at the same time:
	mMutex.lock();
	// Check for pending requests
	pending_lod_map_t::iterator iter = mPendingLOD.find(mesh_id);
	if (iter != mPendingLOD.end())
	{
		for (U32 i = 0; i < iter->second.size(); ++i)
		{
			mLODReqQ.emplace_back(mesh_params, iter->second[i]);
			++LLMeshRepository::sLODProcessing;
		}
		mPendingLOD.erase(iter);
	}
	mMutex.unlock();

	return true;
}

// IMPORTANT: must be called with mHeaderMutex locked !
LLMeshHeader* LLMeshRepoThread::getMeshHeader(const LLUUID& mesh_id)
{
	if (mesh_id.notNull())
	{
		mesh_header_map_t::iterator iter = mMeshHeaders.find(mesh_id);
		if (iter != mMeshHeaders.end() && iter->second->mValid)
		{
			return iter->second;
		}
	}
	return NULL;
}

bool LLMeshRepoThread::lodReceived(const LLVolumeParams& mesh_params,
								   S32 lod, U8* data, S32 data_size)
{
	LL_DEBUGS("Mesh") << "Processing LOD " << lod << " for mesh Id: "
					  << mesh_params.getSculptID() << LL_ENDL;
	if (!data || !data_size)
	{
		llwarns << "No data received for mesh Id: "
				<< mesh_params.getSculptID() << " - LOD: " << lod << llendl;
		return false;
	}

	LLPointer<LLVolume> volume =
		new LLVolume(mesh_params,
					 LLVolumeLODGroup::getVolumeScaleFromDetail(lod));
	if (volume.notNull() && volume->unpackVolumeFaces(data, data_size))
	{
		if (volume->getNumFaces() > 0)
		{
			mMutex.lock();
			mLoadedMeshes.emplace_back(std::move(volume), mesh_params, lod);
			// NOTE: the std::move() above should ensure 'volume' got already
			// dereferenced (via LLPointer move constructor), but make sure
			// it is (in case the compiler would not obey us and perform a copy
			// instead !) so that the reference counter on the LLPointer is
			// decremented while mLoadedMeshes is still locked... HB
			volume = NULL;
			mMutex.unlock();
			return true;
		}
	}

	return false;
}

bool LLMeshRepoThread::skinInfoReceived(const LLUUID& mesh_id, U8* data,
										S32 data_size)
{
	LLSD skin;

	if (data_size > 0)
	{
		if (!unzip_llsd(skin, data, data_size))
		{
			llwarns << "Mesh skin decompression error." << llendl;
			return false;
		}
	}

	mMutex.lock();
	mSkinInfos.push_back(new LLMeshSkinInfo(skin, mesh_id));
	mMutex.unlock();

	return true;
}

bool LLMeshRepoThread::decompositionReceived(const LLUUID& mesh_id, U8* data,
											 S32 data_size)
{
	LLSD decomp;

	if (data_size > 0)
	{
		if (!unzip_llsd(decomp, data, data_size))
		{
			llwarns << "Mesh decomposition decompression error." << llendl;
			return false;
		}
	}

	LLModel::Decomposition* d = new LLModel::Decomposition(decomp, mesh_id);
	mMutex.lock();
	mDecompositions.push_back(d);
	mMutex.unlock();

	return true;
}

bool LLMeshRepoThread::physicsShapeReceived(const LLUUID& mesh_id, U8* data,
											S32 data_size)
{
	LLSD physics_shape;

	LLModel::Decomposition* d = new LLModel::Decomposition();
	d->mMeshID = mesh_id;

	if (!data)
	{
		// No data, no physics shape exists
		d->mPhysicsShapeMesh.clear();
	}
	else
	{
		LLVolumeParams volume_params;
		volume_params.setType(LL_PCODE_PROFILE_SQUARE, LL_PCODE_PATH_LINE);
		volume_params.setSculptID(mesh_id, LL_SCULPT_TYPE_MESH);

		LLPointer<LLVolume> volume = new LLVolume(volume_params, 0);
		if (volume->unpackVolumeFaces(data, data_size))
		{
			d->mPhysicsShapeMesh.clear();

			// Load volume faces into decomposition buffer
			std::vector<LLVector3>& pos = d->mPhysicsShapeMesh.mPositions;
			std::vector<LLVector3>& norm = d->mPhysicsShapeMesh.mNormals;

			for (S32 i = 0, count = volume->getNumVolumeFaces(); i < count;
				 i++)
			{
				const LLVolumeFace& face = volume->getVolumeFace(i);

				for (S32 i = 0; i < face.mNumIndices; ++i)
				{
					U16 idx = face.mIndices[i];

					pos.emplace_back(face.mPositions[idx].getF32ptr());
					norm.emplace_back(face.mNormals[idx].getF32ptr());
				}
			}
		}
	}

	mMutex.lock();
	mDecompositions.push_back(d);
	mMutex.unlock();

	return true;
}

LLMeshUploadThread::LLMeshUploadThread(instance_list_t& data, LLVector3& scale,
									   bool upload_textures, bool upload_skin,
									   bool upload_joints,
									   bool lock_scale_if_joint_position,
									   const std::string& upload_url,
									   bool do_upload,
									   LLHandle<LLWholeModelFeeObserver> fee_observer,
									   LLHandle<LLWholeModelUploadObserver> upload_observer)
:	LLThread("Mesh upload"),
	LLCore::HttpHandler(),
	mDiscarded(false),
	mDoUpload(do_upload),
	mWholeModelUploadURL(upload_url),
	mFeeObserverHandle(fee_observer),
	mUploadObserverHandle(upload_observer)
{
	mInstanceList = data;
	mUploadTextures = upload_textures;
	mUploadSkin = upload_skin;
	mUploadJoints = upload_joints;
	mLockScaleIfJointPosition = lock_scale_if_joint_position;
	mPendingUploads = 0;
	mFinished = false;
	mOrigin = gAgent.getPositionAgent();
	mHost = gAgent.getRegionHost();

	mOrigin += gAgent.getAtAxis() * scale.length();

	mMeshUploadTimeOut = gSavedSettings.getS32("MeshUploadTimeOut");

	mHttpRequest = new LLCore::HttpRequest;
	mHttpOptions = DEFAULT_HTTP_OPTIONS;
	mHttpOptions->setTransferTimeout(mMeshUploadTimeOut);
	mHttpOptions->setUseRetryAfter(gSavedSettings.getBool("MeshUseHttpRetryAfter"));
	mHttpOptions->setRetries(UPLOAD_RETRY_LIMIT);
	mHttpHeaders = DEFAULT_HTTP_HEADERS;
	mHttpHeaders->append(HTTP_OUT_HEADER_CONTENT_TYPE,
						 HTTP_CONTENT_LLSD_XML);
	mHttpPolicyClass =
		gAppViewerp->getAppCoreHttp().getPolicy(LLAppCoreHttp::AP_UPLOADS);
}

LLMeshUploadThread::~LLMeshUploadThread()
{
	delete mHttpRequest;
	mHttpRequest = NULL;
}

LLMeshUploadThread::DecompRequest::DecompRequest(LLModel* mdl,
												 LLModel* base_model,
												 LLMeshUploadThread* thread)
{
	mStage = "single_hull";
	mModel = mdl;
	mDecompID = &mdl->mDecompID;
	mBaseModel = base_model;
	mThread = thread;

	// Copy out positions and indices
	assignData(mdl);

	mThread->mFinalDecomp = this;
	mThread->mPhysicsComplete = false;
}

void LLMeshUploadThread::DecompRequest::completed()
{
	if (mThread->mFinalDecomp == this)
	{
		mThread->mPhysicsComplete = true;
	}

	llassert(mHull.size() == 1);

	mThread->mHullMap[mBaseModel] = mHull[0];

	LL_DEBUGS("MeshUpload") << "Decomposition request completed." << LL_ENDL;
}

// Called in the main thread.
void LLMeshUploadThread::preStart()
{
	// Build map of LLModel refs to instances for callbacks
	for (instance_list_t::iterator iter = mInstanceList.begin(),
								   end = mInstanceList.end();
		 iter != end; ++iter)
	{
		mInstance[iter->mModel].emplace_back(*iter);
	}
}

void LLMeshUploadThread::discard()
{
	mMutex.lock();
	mDiscarded = true;
	mMutex.unlock();
}

bool LLMeshUploadThread::isDiscarded()
{
	mMutex.lock();
	bool discarded = mDiscarded;
	mMutex.unlock();
	return discarded;
}

void LLMeshUploadThread::run()
{
	if (mDoUpload)
	{
		doWholeModelUpload();
	}
	else
	{
		requestWholeModelFee();
	}
}

LLViewerFetchedTexture* LLMeshUploadThread::findViewerTexture(const LLImportMaterial& mat)
{
	LLPointer<LLViewerFetchedTexture>* tex =
		(LLPointer<LLViewerFetchedTexture>*)mat.mUserData;
	return tex ? (*tex).get() : NULL;
}

struct LLMeshUploadData
{
	LLMeshUploadData()
	:	mRetries(0)
	{
	}

	U32					mRetries;
	LLPointer<LLModel>	mBaseModel;
	LLPointer<LLModel>	mModel[5];
	LLUUID				mUUID;
	std::string			mRSVP;
	std::string			mAssetData;
	LLSD				mPostData;
};

void LLMeshUploadThread::wholeModelToLLSD(LLSD& dest, bool include_textures)
{
	LLSD result;

	LLSD res;
	result["folder_id"] =
		gInventory.findChoosenCategoryUUIDForType(LLFolderType::FT_OBJECT);
	result["texture_folder_id"] =
		gInventory.findChoosenCategoryUUIDForType(LLFolderType::FT_TEXTURE);
	result["asset_type"] = "mesh";
	result["inventory_type"] = "object";
	result["description"] = "(No Description)";
	result["next_owner_mask"] =
		LLSD::Integer(LLFloaterPerms::getNextOwnerPerms());
	result["group_mask"] = LLSD::Integer(LLFloaterPerms::getGroupPerms());
	result["everyone_mask"] =
		LLSD::Integer(LLFloaterPerms::getEveryonePerms());

	res["mesh_list"] = LLSD::emptyArray();
	res["texture_list"] = LLSD::emptyArray();
	res["instance_list"] = LLSD::emptyArray();
	S32 mesh_num = 0;
	S32 texture_num = 0;

	std::set<LLViewerTexture*> textures;
	std::map<LLViewerTexture*, S32> texture_index;

	std::map<LLModel*, S32> mesh_index;
	std::string model_name;

	S32 instance_num = 0;

	for (instance_map_t::iterator iter = mInstance.begin(),
								  end = mInstance.end();
		 iter != end; ++iter)
	{
		LLMeshUploadData data;
		data.mBaseModel = iter->first;

		if (data.mBaseModel->mSubmodelID)
		{
			// These are handled below to insure correct parenting order on
			// creation due to map walking being based on model address (aka
			// random)
			continue;
		}

		LLModelInstance& first_instance = *(iter->second.begin());
		for (S32 i = 0; i < 5; ++i)
		{
			data.mModel[i] = first_instance.mLOD[i];
		}

		if (mesh_index.find(data.mBaseModel) == mesh_index.end())
		{
			// Have not seen this model before - create a new mesh_list entry
			// for it.
			if (model_name.empty())
			{
				model_name = data.mBaseModel->getName();
			}

			std::stringstream ostr;

			LLModel::Decomposition& decomp =
				data.mModel[LLModel::LOD_PHYSICS].notNull() ?
					data.mModel[LLModel::LOD_PHYSICS]->mPhysics :
					data.mBaseModel->mPhysics;
			decomp.mBaseHull = mHullMap[data.mBaseModel];

			LLSD mesh_header =
				LLModel::writeModel(ostr, data.mModel[LLModel::LOD_PHYSICS],
									data.mModel[LLModel::LOD_HIGH],
									data.mModel[LLModel::LOD_MEDIUM],
									data.mModel[LLModel::LOD_LOW],
									data.mModel[LLModel::LOD_IMPOSTOR],
									decomp, mUploadSkin, mUploadJoints,
									mLockScaleIfJointPosition, false, false,
									data.mBaseModel->mSubmodelID);

			data.mAssetData = ostr.str();
			std::string str = ostr.str();

			res["mesh_list"][mesh_num] = LLSD::Binary(str.begin(), str.end());
			mesh_index[data.mBaseModel] = mesh_num++;
		}

		// For all instances that use this model
		for (instance_list_t::iterator instance_iter = iter->second.begin(),
									   instance_end = iter->second.end();
			 instance_iter != instance_end; ++instance_iter)
		{
			LLModelInstance& instance = *instance_iter;

			LLSD instance_entry;

			for (S32 i = 0; i < 5; ++i)
			{
				data.mModel[i] = instance.mLOD[i];
			}

			LLVector3 pos, scale;
			LLQuaternion rot;
			LLMatrix4 transformation = instance.mTransform;
			decomposeMeshMatrix(transformation, pos, rot, scale);
			instance_entry["position"] = ll_sd_from_vector3(pos);
			instance_entry["rotation"] = ll_sd_from_quaternion(rot);
			instance_entry["scale"] = ll_sd_from_vector3(scale);

			instance_entry["material"] = LL_MCODE_WOOD;

			U8 shape_type;
			if (data.mModel[LLModel::LOD_PHYSICS].notNull())
			{
				shape_type = (U8)(LLViewerObject::PHYSICS_SHAPE_PRIM);
			}
			else
			{
				shape_type = (U8)(LLViewerObject::PHYSICS_SHAPE_CONVEX_HULL);
			}
			instance_entry["physics_shape_type"] = shape_type;

			instance_entry["mesh"] = mesh_index[data.mBaseModel];
			instance_entry["mesh_name"] = instance.mLabel;
			instance_entry["face_list"] = LLSD::emptyArray();

			// We want to be able to allow more than 8 materials...
			S32 end = llmin((S32)instance.mMaterial.size(),
							instance.mModel->getNumVolumeFaces());
			for (S32 face_num = 0; face_num < end; ++face_num)
			{
				LLImportMaterial& material =
					instance.mMaterial[data.mBaseModel->mMaterialList[face_num]];
				LLSD face_entry = LLSD::emptyMap();

				LLViewerFetchedTexture* texture = NULL;

				if (material.mDiffuseMapFilename.size())
				{
					texture = findViewerTexture(material);
				}

				if (texture && textures.find(texture) == textures.end())
				{
					textures.insert(texture);
				}

				std::stringstream texture_str;
				if (texture && include_textures && mUploadTextures)
				{
					if (texture->hasSavedRawImage())
					{
						LLPointer<LLImageJ2C> upload_file =
							LLViewerTextureList::convertToUploadFile(texture->getSavedRawImage());
						if (upload_file.notNull() &&
							upload_file->getDataSize())
						{
							texture_str.write((const char*)upload_file->getData(),
											  upload_file->getDataSize());
						}
					}
				}

				if (texture && mUploadTextures &&
					texture_index.find(texture) == texture_index.end())
				{
					texture_index[texture] = texture_num;
					std::string str = texture_str.str();
					res["texture_list"][texture_num++] = LLSD::Binary(str.begin(),
																	  str.end());
				}

				// Subset of TextureEntry fields.
				if (texture && mUploadTextures)
				{
					face_entry["image"] = texture_index[texture];
					face_entry["scales"] = 1.0;
					face_entry["scalet"] = 1.0;
					face_entry["offsets"] = 0.0;
					face_entry["offsett"] = 0.0;
					face_entry["imagerot"] = 0.0;
				}
				face_entry["diffuse_color"] =
					ll_sd_from_color4(material.mDiffuseColor);
				face_entry["fullbright"] = material.mFullbright;
				instance_entry["face_list"][face_num] = face_entry;
		    }

			res["instance_list"][instance_num++] = instance_entry;
		}
	}

	for (instance_map_t::iterator iter = mInstance.begin(),
								  the_end = mInstance.end();
		 iter != the_end; ++iter)
	{
		LLMeshUploadData data;
		data.mBaseModel = iter->first;

		if (!data.mBaseModel->mSubmodelID)
		{
			// These were handled above already...
			continue;
		}

		LLModelInstance& first_instance = *(iter->second.begin());
		for (S32 i = 0; i < 5; ++i)
		{
			data.mModel[i] = first_instance.mLOD[i];
		}

		if (mesh_index.find(data.mBaseModel) == mesh_index.end())
		{
			// Have not seen this model before; create a new mesh_list entry
			// for it.
			if (model_name.empty())
			{
				model_name = data.mBaseModel->getName();
			}

			std::stringstream ostr;

			LLModel::Decomposition& decomp =
				data.mModel[LLModel::LOD_PHYSICS].notNull() ?
					 data.mModel[LLModel::LOD_PHYSICS]->mPhysics :
					 data.mBaseModel->mPhysics;

			decomp.mBaseHull = mHullMap[data.mBaseModel];

			LLSD mesh_header =
				LLModel::writeModel(ostr, data.mModel[LLModel::LOD_PHYSICS],
									data.mModel[LLModel::LOD_HIGH],
									data.mModel[LLModel::LOD_MEDIUM],
									data.mModel[LLModel::LOD_LOW],
									data.mModel[LLModel::LOD_IMPOSTOR],
									decomp, mUploadSkin, mUploadJoints,
									mLockScaleIfJointPosition, false, false,
									data.mBaseModel->mSubmodelID);
			data.mAssetData = ostr.str();
			std::string str = ostr.str();

			res["mesh_list"][mesh_num] = LLSD::Binary(str.begin(), str.end());
			mesh_index[data.mBaseModel] = mesh_num++;
		}

		// For all instances that use this model
		for (instance_list_t::iterator instance_iter = iter->second.begin(),
									   instance_end = iter->second.end();
			 instance_iter != instance_end; ++instance_iter)
		{
			LLModelInstance& instance = *instance_iter;
			LLSD instance_entry;

			for (S32 i = 0; i < 5; ++i)
			{
				data.mModel[i] = instance.mLOD[i];
			}

			LLVector3 pos, scale;
			LLQuaternion rot;
			LLMatrix4 transformation = instance.mTransform;
			decomposeMeshMatrix(transformation,pos,rot,scale);
			instance_entry["position"] = ll_sd_from_vector3(pos);
			instance_entry["rotation"] = ll_sd_from_quaternion(rot);
			instance_entry["scale"] = ll_sd_from_vector3(scale);

			instance_entry["material"] = LL_MCODE_WOOD;
			instance_entry["physics_shape_type"] =
				(U8)(LLViewerObject::PHYSICS_SHAPE_NONE);
			instance_entry["mesh"] = mesh_index[data.mBaseModel];

			instance_entry["face_list"] = LLSD::emptyArray();

			// We want to be able to allow more than 8 materials...
			S32 end = llmin((S32)instance.mMaterial.size(),
							instance.mModel->getNumVolumeFaces());

			for (S32 face_num = 0; face_num < end; ++face_num)
			{
				LLImportMaterial& material =
					instance.mMaterial[data.mBaseModel->mMaterialList[face_num]];
				LLSD face_entry = LLSD::emptyMap();

				LLViewerFetchedTexture* tex = NULL;

				if (material.mDiffuseMapFilename.size())
				{
					tex = findViewerTexture(material);
				}

				if (tex && textures.find(tex) == textures.end())
				{
					textures.insert(tex);
				}

				std::stringstream tex_str;
				if (tex && include_textures && mUploadTextures)
				{
					if (tex->hasSavedRawImage())
					{
						LLPointer<LLImageJ2C> upload_file =
							LLViewerTextureList::convertToUploadFile(tex->getSavedRawImage());

						if (!upload_file.isNull() &&
							upload_file->getDataSize())
						{
							tex_str.write((const char*)upload_file->getData(),
										  upload_file->getDataSize());
						}
					}
				}

				if (tex && mUploadTextures &&
					texture_index.find(tex) == texture_index.end())
				{
					texture_index[tex] = texture_num;
					std::string str = tex_str.str();
					res["texture_list"][texture_num++] =
						LLSD::Binary(str.begin(), str.end());
				}

				// Subset of TextureEntry fields.
				if (tex && mUploadTextures)
				{
					face_entry["image"] = texture_index[tex];
					face_entry["scales"] = 1.0;
					face_entry["scalet"] = 1.0;
					face_entry["offsets"] = 0.0;
					face_entry["offsett"] = 0.0;
					face_entry["imagerot"] = 0.0;
				}
				face_entry["diffuse_color"] =
					ll_sd_from_color4(material.mDiffuseColor);
				face_entry["fullbright"] = material.mFullbright;
				instance_entry["face_list"][face_num] = face_entry;
		    }

			res["instance_list"][instance_num++] = instance_entry;
		}
	}

	if (model_name.empty())
	{
		model_name = "mesh model";
	}
	result["name"] = model_name;

	res["metric"] = "MUT_Unspecified";
	result["asset_resources"] = res;

	dump_llsd_to_file(result, make_dump_name("whole_model_", sDumpNum));

	dest = result;
}

void LLMeshUploadThread::generateHulls()
{
	bool no_valid_request = true;

	for (instance_map_t::iterator iter = mInstance.begin(),
								  end = mInstance.end();
		 iter != end; ++iter)
	{
		LLMeshUploadData data;
		data.mBaseModel = iter->first;

		LLModelInstance& instance = *(iter->second.begin());

		for (S32 i = 0; i < 5; ++i)
		{
			data.mModel[i] = instance.mLOD[i];
		}

		// Queue up models for hull generation
		LLModel* physics = NULL;
		if (data.mModel[LLModel::LOD_PHYSICS].notNull())
		{
			physics = data.mModel[LLModel::LOD_PHYSICS];
		}
		else if (data.mModel[LLModel::LOD_LOW].notNull())
		{
			physics = data.mModel[LLModel::LOD_LOW];
		}
		else if (data.mModel[LLModel::LOD_MEDIUM].notNull())
		{
			physics = data.mModel[LLModel::LOD_MEDIUM];
		}
		else
		{
			physics = data.mModel[LLModel::LOD_HIGH];
		}

		DecompRequest* request = new DecompRequest(physics, data.mBaseModel,
												   this);
		if (request->isValid())
		{
			gMeshRepo.mDecompThread->submitRequest(request);
			no_valid_request = false;
		}
	}

	if (no_valid_request)
	{
		return;
	}

	// *NOTE: interesting live-lock condition on shutdown; if there is an
	// upload request in generateHulls() when shutdown starts, the main thread
	// is not available to manage communication between the decomposition
	// thread and the upload thread and this loop would not complete in turn
	// stalling the main thread. The check on isDiscarded() prevents that.
	LL_DEBUGS("MeshUpload") << "Sleeping after hulls generation till the physics decomp request is honored."
							<< LL_ENDL;
	while (!mPhysicsComplete && !isDiscarded())
	{
		ms_sleep(1);
	}
	LL_DEBUGS("MeshUpload") << "Physics decomp request is honored. Sleep state exited."
							<< LL_ENDL;
}

void LLMeshUploadThread::doWholeModelUpload()
{
	LL_DEBUGS("MeshUpload") << "Starting model upload. Instances: "
							<< mInstance.size() << LL_ENDL;
	if (mWholeModelUploadURL.empty())
	{
		llinfos << "Unable to upload, fee request failed" << llendl;
	}
	else
	{
		generateHulls();
		LL_DEBUGS("MeshUpload") << "Hull generation completed." << LL_ENDL;

		mModelData = LLSD::emptyMap();
		wholeModelToLLSD(mModelData, true);
		LLSD body = mModelData["asset_resources"];
		dump_llsd_to_file(body, make_dump_name("whole_model_body_", sDumpNum));

		LLCore::HttpHandle handle =
			LLCoreHttpUtil::requestPostWithLLSD(mHttpRequest, mHttpPolicyClass,
												mWholeModelUploadURL, body,
												mHttpOptions, mHttpHeaders,
												LLCore::HttpHandler::ptr_t(this,
																		   &NoOpDeletor));
		if (handle == LLCORE_HTTP_HANDLE_INVALID)
		{
			mHttpStatus = mHttpRequest->getStatus();
			llwarns << "Could not issue request for full model upload. Reason: "
					<< mHttpStatus.toString() << " ("
					<< mHttpStatus.toTerseString() << ")" << llendl;
		}
		else
		{
			U32 sleep_time = 10;

			LL_DEBUGS("MeshUpload") << "POST request issued." << LL_ENDL;

			mHttpRequest->update(0);
			while (!LLApp::isExiting() && !finished() && !isDiscarded())
			{
				ms_sleep(sleep_time);
				sleep_time = llmin(250U, sleep_time + sleep_time);
				mHttpRequest->update(0);
			}

			LL_DEBUGS("MeshUpload") << "Mesh upload operation "
									<< (isDiscarded() ? "discarded."
													  : "completed.")
									<< LL_ENDL;
		}
	}
}

void LLMeshUploadThread::requestWholeModelFee()
{
	++sDumpNum;
	generateHulls();

	mModelData = LLSD::emptyMap();
	wholeModelToLLSD(mModelData, false);
	dump_llsd_to_file(mModelData,
					  make_dump_name("whole_model_fee_request_", sDumpNum));

	const std::string& whole_model_fee_cap_url =
		gAgent.getRegionCapability("NewFileAgentInventory");

	LLCore::HttpHandle handle =
		LLCoreHttpUtil::requestPostWithLLSD(mHttpRequest, mHttpPolicyClass,
											whole_model_fee_cap_url,
											mModelData, mHttpOptions,
											mHttpHeaders,
											LLCore::HttpHandler::ptr_t(this,
																	   &NoOpDeletor));
	if (handle == LLCORE_HTTP_HANDLE_INVALID)
	{
		mHttpStatus = mHttpRequest->getStatus();
		llwarns << "Could not issue request for model fee. Reason: "
				<< mHttpStatus.toString() << " ("
				<< mHttpStatus.toTerseString() << ")" << llendl;
	}
	else
	{
		U32 sleep_time = 10;

		mHttpRequest->update(0);
		while (!LLApp::isExiting() && !finished() && !isDiscarded())
		{
			ms_sleep(sleep_time);
			sleep_time = llmin(250U, 2 * sleep_time);
			mHttpRequest->update(0);
		}
		LL_DEBUGS("MeshUpload") << "Mesh fee query operation "
								<< (isDiscarded() ? "discarded" : "completed")
								<< LL_ENDL;
	}
}

// Helper function
static void log_upload_error(LLCore::HttpStatus status, const LLSD& content,
							 const char* const stage,
							 const std::string& model_name)
{
	// Add notification popup.
	LLSD args;
	std::string message = content["error"]["message"].asString();
	std::string identifier = content["error"]["identifier"].asString();
	args["MESSAGE"] = message;
	args["IDENTIFIER"] = identifier;
	args["LABEL"] = model_name;

	// Log details.
	llwarns << "Error in stage: " << stage << " - Reason: "
			<< status.toString() << " (" <<  status.toTerseString()<< ")"
			<< llendl;

	std::ostringstream details;
	typedef std::set<std::string> mav_errors_set_t;
	mav_errors_set_t mav_errors;

	if (content.has("error"))
	{
		const LLSD& err = content["error"];
		llwarns << "Error: " << err << " - Mesh upload failed at stage "
				<< stage << " with error: " << err["error"].asString()
				<< " - Message: " << err["message"].asString() << " - Id: "
				<< err["identifier"].asString() << llendl;

		if (err.has("errors"))
		{
			details << std::endl << std::endl;

			S32 error_num = 0;
			const LLSD& err_list = err["errors"];
			for (LLSD::array_const_iterator it = err_list.beginArray();
				 it != err_list.endArray(); ++it)
			{
				const LLSD& err_entry = *it;
				std::string message = err_entry["message"];
				if (message.length() > 0)
				{
					mav_errors.emplace(message);
				}

				llwarns << "error[" << error_num << "]:" << llendl;
				for (LLSD::map_const_iterator map_it = err_entry.beginMap();
					 map_it != err_entry.endMap(); ++map_it)
				{
					llwarns << "    " << map_it->first << ": "
							<< map_it->second << llendl;
				}
				++error_num;
			}
		}
	}
	else
	{
		llwarns << "Bad response to mesh, no error information available"
				<< llendl;
	}

	for (mav_errors_set_t::iterator it = mav_errors.begin(),
									end = mav_errors.end();
		 it != end; ++it)
	{
		details << "Message: '" << *it << "': "
				<< LLTrans::getString("Mav_Details_" + *it)
				<< std::endl << std::endl;
	}

	args["DETAILS"] = details.str();

	gMeshRepo.uploadError(args);
}

// Does completion duty for both fee queries and actual uploads.
void LLMeshUploadThread::onCompleted(LLCore::HttpHandle handle,
									 LLCore::HttpResponse* response)
{
	LLCore::HttpStatus status = response->getStatus();
	std::string reason = status.toString();
	LLSD body;

	mFinished = true;

	if (mDoUpload)	// Model upload case
	{
		LLWholeModelUploadObserver* observer(mUploadObserverHandle.get());

		if (!status)
		{
			llwarns << "Upload failed. Reason: " << reason << " ("
					<< status.toTerseString() << ")" << llendl;

			// Build a fake body for the alert generator
			body["error"] = LLSD::emptyMap();
			body["error"]["message"] = reason;
			body["error"]["identifier"] = "NetworkError";
			log_upload_error(status, body, "upload",
							 mModelData["name"].asString());
			if (observer)
			{
				doOnIdleOneTime(boost::bind(&LLWholeModelUploadObserver::onModelUploadFailure,
											observer));
			}
		}
		else
		{
			// *TODO: handle error in conversion process
			LLCoreHttpUtil::responseToLLSD(response, true, body);
			dump_llsd_to_file(body,
							  make_dump_name("whole_model_upload_response_",
											 sDumpNum));

			if (body["state"].asString() == "complete")
			{
				// Requested "mesh" asset type is not actually the type of the
				// resultant object, fix it up here.
				mModelData["asset_type"] = "object";
				gMeshRepo.updateInventory(LLMeshRepository::InventoryData(mModelData,
																		   body));

				if (observer)
				{
					doOnIdleOneTime(boost::bind(&LLWholeModelUploadObserver::onModelUploadSuccess,
												observer));
				}
			}
			else
			{
				llwarns << "Upload failed. Not in expected 'complete' state."
						<< llendl;
				log_upload_error(status, body, "upload",
								 mModelData["name"].asString());

				if (observer)
				{
					doOnIdleOneTime(boost::bind(&LLWholeModelUploadObserver::onModelUploadFailure,
												observer));
				}
			}
		}
	}
	else			// Model fee case
	{
		LLWholeModelFeeObserver* observer = mFeeObserverHandle.get();
		mWholeModelUploadURL.clear();

		if (!status)
		{
			llwarns << "Fee request failed. Reason: " << reason << " ("
					<< status.toTerseString() << ")" << llendl;

			// Build a fake body for the alert generator
			body["error"] = LLSD::emptyMap();
			body["error"]["message"] = reason;
			body["error"]["identifier"] = "NetworkError";
			log_upload_error(status, body, "fee",
							 mModelData["name"].asString());

			if (observer)
			{
				observer->setModelPhysicsFeeErrorStatus(status.toULong(),
														reason, body["error"]);
			}
		}
		else
		{
			// *TODO: handle error in conversion process
			LLCoreHttpUtil::responseToLLSD(response, true, body);
			dump_llsd_to_file(body, make_dump_name("whole_model_fee_response_",
												   sDumpNum));

			if (body["state"].asString() == "upload")
			{
				mWholeModelUploadURL = body["uploader"].asString();

				if (observer)
				{
					body["data"]["upload_price"] = body["upload_price"];
					observer->onModelPhysicsFeeReceived(body["data"],
														mWholeModelUploadURL);
				}
			}
			else
			{
				llwarns << "Fee request failed. Not in expected 'upload' state."
						<< llendl;
				log_upload_error(status, body, "fee",
								 mModelData["name"].asString());

				if (observer)
				{
					observer->setModelPhysicsFeeErrorStatus(status.toULong(),
															reason,
															body["error"]);
				}
			}
		}
	}
}

void LLMeshRepoThread::notifyLoadedMeshes()
{
	LL_TRACY_TIMER(TRC_MESH_THREAD_NOTIFY_LOADED);

	if (!mLoadedMeshes.empty())
	{
		loaded_mesh_list_t list_copy;
		mMutex.lock();
		list_copy.swap(mLoadedMeshes);
		mMutex.unlock();

		for (loaded_mesh_list_t::iterator it = list_copy.begin(),
										  end = list_copy.end();
			 it != end; ++it)
		{
			const LoadedMesh& mesh = *it;
			if (mesh.mVolume && mesh.mVolume->getNumVolumeFaces() > 0)
			{
				gMeshRepo.notifyMeshLoaded(mesh.mMeshParams, mesh.mVolume);
			}
			else
			{
				gMeshRepo.notifyMeshUnavailable(mesh.mMeshParams,
												LLVolumeLODGroup::getVolumeDetailFromScale(mesh.mVolume->getDetail()));
			}
		}
	}

	if (!mUnavailableLODs.empty())
	{
		lod_req_list_t list_copy;
		mMutex.lock();
		list_copy.swap(mUnavailableLODs);
		mMutex.unlock();

		for (lod_req_list_t::iterator it = list_copy.begin(),
									  end = list_copy.end();
			 it != end; ++it)
		{
			const LODRequest& req = *it;
			gMeshRepo.notifyMeshUnavailable(req.mMeshParams, req.mLOD);
		}
	}

	bool no_skin = mSkinInfos.empty();
	bool no_unavailable_skin = mUnavailableSkins.empty();
	bool no_decomp = mDecompositions.empty();
	if (no_skin && no_unavailable_skin && no_decomp)
	{
		return;
	}

	if (!mMutex.trylock())
	{
		return;
	}
	skin_info_list_t skin_info_list;
	if (!no_skin)
	{
		skin_info_list.swap(mSkinInfos);
	}
	uuid_vec_t skin_info_vec;
	if (!no_unavailable_skin)
	{
		skin_info_vec.swap(mUnavailableSkins);
	}
	decomp_list_t decomp_list;
	if (!no_decomp)
	{
		decomp_list.swap(mDecompositions);
	}
	mMutex.unlock();

	// Process the elements free of the lock
	for (skin_info_list_t::iterator it = skin_info_list.begin(),
									end = skin_info_list.end();
		 it != end; ++it)
	{
		gMeshRepo.notifySkinInfoReceived(*it);
	}

	for (U32 i = 0, count = skin_info_vec.size(); i < count; ++i)
	{
		gMeshRepo.notifySkinInfoUnavailable(skin_info_vec[i]);
	}

	for (decomp_list_t::iterator it = decomp_list.begin(),
								 end = decomp_list.end();
		 it != end; ++it)
	{
		gMeshRepo.notifyDecompositionReceived(*it);
	}
}

// Only ever called from main thread
S32 LLMeshRepoThread::getActualMeshLOD(const LLVolumeParams& mesh_params,
									   S32 lod)
{
	mHeaderMutex.lock();

	mesh_header_map_t::iterator iter =
		mMeshHeaders.find(mesh_params.getSculptID());
	if (iter != mMeshHeaders.end())
	{
		lod = LLMeshRepository::getActualMeshLOD(iter->second, lod);
	}

	mHeaderMutex.unlock();

	return lod;
}

//static
S32 LLMeshRepository::getActualMeshLOD(LLMeshHeader* header, S32 lod)
{
	lod = llclamp(lod, 0, 3);

	if (!header || !header->mValid)
	{
		return -1;
	}

	if (header->mLodSize[lod] > 0)
	{
		return lod;
	}

	static LLCachedControl<bool> higher_first(gSavedSettings,
											  "SearchHigherMeshLODFirst");

	if (higher_first)
	{
		// Search up to find then next available higher lod
		for (S32 i = lod + 1; i < 4; ++i)
		{
			if (header->mLodSize[i] > 0)
			{
				return i;
			}
		}
	}

	// Search down to find the next available lower lod
	for (S32 i = lod - 1; i >= 0; --i)
	{
		if (header->mLodSize[i] > 0)
		{
			return i;
		}
	}

	if (!higher_first)
	{
		// Search up to find then next available higher lod
		for (S32 i = lod + 1; i < 4; ++i)
		{
			if (header->mLodSize[i] > 0)
			{
				return i;
			}
		}
	}

	// Should never get there... Header valid and no good LOD found.
	llassert(false);
	return -1;
}

// Handle failed or successful requests for mesh assets.
//
// Support for 200 responses was added for several reasons. One, a service or
// cache can ignore range headers and give us a 200 with full asset should it
// elect to. We also support a debug flag which disables range requests for
// those very few users that have some sort of problem with their networking
// services. But the 200 response handling is suboptimal: rather than cache the
// whole asset, we just extract the part that would have been sent in a 206 and
// process that. Inefficient but these are cases far off the norm.
void LLMeshHandlerBase::onCompleted(LLCore::HttpHandle handle,
									LLCore::HttpResponse* response)
{
	LL_TRACY_TIMER(TRC_MESH_HANDLER_COMPLETED);

	// Thread could have already been destroyed during logout
	if (!gMeshRepo.mThread)
	{
		return;
	}

	mProcessed = true;

	do	// Single-pass do-while used for common exit handling
	{
		LLCore::HttpStatus status = response->getStatus();
		if (!status)
		{
			processFailure(status);
			++LLMeshRepository::sHTTPErrorCount;
			break;
		}

		// From texture fetch code and may apply here:
		//
		// A warning about partial (HTTP 206) data. Some grid services do *not*
		// return a 'Content-Range' header in the response to Range requests
		// with a 206 status. We're forced to assume we get what we asked for
		// in these cases until we can fix the services.
		//
		// May also need to deal with 200 status (full asset returned rather
		// than partial) and 416 (request completely unsatisfyable). Always
		// been exposed to these but are less likely here where speculative
		// loads aren't done.
		LLCore::BufferArray* body = response->getBody();
		S32 body_offset = 0;
		U8* data = NULL;
		S32 data_size = body ? body->size() : 0;

		if (data_size > 0)
		{
			unsigned int offset = 0;
			unsigned int length = 0;
			unsigned int full_length = 0;

			if (status == gStatusPartialContent)
			{
				// 206 case
				response->getRange(&offset, &length, &full_length);
				if (!offset && !length)
				{
					// This is the case where we receive a 206 status but there
					// was not a useful Content-Range header in the response.
					// This could be because it was badly formatted but is more
					// likely due to capabilities services which scrub headers
					// from responses. Assume we got what we asked for...
					offset = mOffset;
				}
			}
			else
			{
				// 200 case, typically
				offset = 0;
			}

			// Validate that what we think we received is consistent with
			// what we've asked for.  I.e. first byte we wanted lies somewhere
			// in the response.
			if ((S32)offset > mOffset || (S32)offset + data_size <= mOffset ||
				mOffset - (S32)offset >= data_size)
			{
				// No overlap with requested range. Fail request with suitable
				// error. Should not happen unless server/cache/ISP is doing
				// something awful.
				llwarns << "Mesh response (bytes [" << offset << ", "
						<< offset + length - 1
						<< "]) didn't overlap with request's origin (bytes ["
						<< mOffset << ", " << mOffset + mRequestedBytes - 1
						<< "])." << llendl;
				processFailure(LLCore::HttpStatus(LLCore::HttpStatus::LLCORE,
												  LLCore::HE_INV_CONTENT_RANGE_HDR));
				++LLMeshRepository::sHTTPErrorCount;
				break;
			}

			LLMeshRepository::sBytesReceived += data_size;

			// *TODO: Try to get rid of data copying and add interfaces that
			// support BufferArray directly. Introduce a two-phase handler,
			// optional first that takes a body, fallback second that requires
			// a temporary allocation and data copy.
			body_offset = mOffset - offset;
			data_size -= body_offset;
			data = new(std::nothrow) U8[data_size];
			if (!data)
			{
				LLMemory::allocationFailed(data_size);
				llwarns << "Could not allocate enough memory. Aborted."
						<< llendl;
				break;
			}
			body->read(body_offset, (char*)data, data_size);
		}

		processData(body, body_offset, data, data_size);

		if (data)
		{
			delete[] data;
		}
	}
	while (false);

	// Release handler
	gMeshRepo.mThread->mHttpRequestSet.erase(this->shared_from_this());
}

LLMeshHeaderHandler::~LLMeshHeaderHandler()
{
	if (!LLApp::isExiting())
	{
		if (!mProcessed)
		{
			// Something went wrong, retry
			llwarns << "Mesh header fetch cancelled unexpectedly, retrying."
					<< llendl;
			LLMutex& mutex = gMeshRepo.mThread->mMutex;
			mutex.lock();
			gMeshRepo.mThread->mHeaderReqQ.emplace_back(mMeshParams);
			mutex.unlock();
		}
		--LLMeshRepoThread::sActiveHeaderRequests;
	}
}

void LLMeshHeaderHandler::processFailure(LLCore::HttpStatus status)
{
	llwarns << "Error during mesh header handling. ID: "
			<< mMeshParams.getSculptID() << " - Reason: " << status.toString()
		   << " (" << status.toTerseString() << "). Not retrying." << llendl;

	// Cannot get the header so none of the LODs will be available
	LLMutex& mutex = gMeshRepo.mThread->mMutex;
	mutex.lock();
	for (S32 i = 0; i < 4; ++i)
	{
		gMeshRepo.mThread->mUnavailableLODs.emplace_back(mMeshParams, i);
	}
	mutex.unlock();
}

void LLMeshHeaderHandler::processData(LLCore::BufferArray*, S32, U8* data,
									  S32 data_size)
{
	LL_TRACY_TIMER(TRC_MESH_PROCESS_HEADER);

	const LLUUID& mesh_id = mMeshParams.getSculptID();
	bool success = gMeshRepo.mThread->headerReceived(mMeshParams, data,
													 data_size);
	if (!success)
	{
		// *TODO: Get real reason for parse failure here. Might we want to
		// retry ?
		llwarns << "Unable to parse mesh header. ID: " << mesh_id
				<< " - Unknown reason. Not retrying." << llendl;

		// Cannot get the header, so none of the LODs will be available
		LLMutex& mutex = gMeshRepo.mThread->mMutex;
		mutex.lock();
		for (S32 i = 0; i < 4; ++i)
		{
			gMeshRepo.mThread->mUnavailableLODs.emplace_back(mMeshParams, i);
		}
		mutex.unlock();
	}
	else if (data && data_size > 0)
	{
		// Header was successfully retrieved from sim, cache it
		LLMeshHeader* header = NULL;
		LLMutex& mutex = gMeshRepo.mThread->mHeaderMutex;
		mutex.lock();
		LLMeshRepoThread::mesh_header_map_t::iterator iter =
			gMeshRepo.mThread->mMeshHeaders.find(mesh_id);
		if (iter != gMeshRepo.mThread->mMeshHeaders.end())
		{
			header = iter->second;
		}
		mutex.unlock();

		if (header && header->mValid)
		{
			U32 header_bytes = header->mHeaderSize;
			U32 lod_bytes = 0;
			U32 lod_size = 0;
			for (U32 i = 0; i < 4; ++i)
			{
				// Figure out how many bytes we will need to reserve in the
				// file
				lod_size = header->mLodSize[i];
				if (lod_size > 0)
				{
					lod_bytes = llmax(lod_bytes,
									  header->mLodOffset[i] + lod_size);
				}
			}

			// Just in case skin info or decomposition is at the end of the
			// file (which it should not be)
			lod_size = header->mSkinSize;
			if (lod_size > 0)
			{
				lod_bytes = llmax(lod_bytes,
								  header->mSkinOffset + lod_size);
			}
			lod_size = header->mPhysicsConvexSize;
			if (lod_size > 0)
			{
				lod_bytes = llmax(lod_bytes,
								  header->mPhysicsConvexOffset + lod_size);
			}

			S32 bytes = llmax(lod_bytes, header_bytes);

			// It is possible for the remote asset to have more data than is
			// needed for the local cache; only allocate as much space in the
			// cache as is needed for the local cache.
			data_size = llmin(data_size, bytes);

			LLMeshRepository::sCacheBytesWritten += data_size;
			++LLMeshRepository::sCacheWrites;

			LLFileSystem file(mesh_id, LLFileSystem::OVERWRITE);
			file.write(data, data_size);
		}
		else
		{
			llwarns << "Trying to cache nonexistent mesh, mesh id: " << mesh_id
					<< llendl;
			// headerReceived() parsed the header, but its data is invalid so
			// none of the LODs will be available
			LLMutex& mutex = gMeshRepo.mThread->mMutex;
			mutex.lock();
			for (S32 i = 0; i < 4; ++i)
			{
				gMeshRepo.mThread->mUnavailableLODs.emplace_back(mMeshParams,
																 i);
			}
			mutex.unlock();
		}
	}
}

LLMeshLODHandler::~LLMeshLODHandler()
{
	if (!LLApp::isExiting())
	{
		if (!mProcessed)
		{
			llwarns << "Mesh LOD fetch cancelled unexpectedly, retrying."
					<< llendl;
			gMeshRepo.mThread->lockAndLoadMeshLOD(mMeshParams, mLOD);
		}
		--LLMeshRepoThread::sActiveLODRequests;
	}
}

void LLMeshLODHandler::processFailure(LLCore::HttpStatus status)
{
	llwarns << "Error during mesh LOD handling. ID: "
			<< mMeshParams.getSculptID() << " - Reason: "
			<< status.toString() << " (" << status.toTerseString()
			<< "). Not retrying." << llendl;

	LLMutex& mutex = gMeshRepo.mThread->mMutex;
	mutex.lock();
	gMeshRepo.mThread->mUnavailableLODs.emplace_back(mMeshParams, mLOD);
	mutex.unlock();
}

void LLMeshLODHandler::processData(LLCore::BufferArray*, S32, U8* data,
								   S32 data_size)
{
	LL_TRACY_TIMER(TRC_MESH_PROCESS_LOD);

	if (data && data_size > 0 &&
		gMeshRepo.mThread->lodReceived(mMeshParams, mLOD, data, data_size))
	{
		// Good fetch from sim, write to cache
		LLFileSystem file(mMeshParams.getSculptID(), LLFileSystem::WRITE);

		S32 offset = mOffset;
		S32 size = mRequestedBytes;
		if (file.getSize() >= MESH_HEADER_SIZE)
		{
			file.seek(offset);	// Note: pads data if necessary. HB
			file.write(data, size);
			LLMeshRepository::sCacheBytesWritten += size;
			++LLMeshRepository::sCacheWrites;
		}
	}
	else
	{
		llwarns << "Failed to unpack volume faces for mesh Id: "
				<< mMeshParams.getSculptID() << " - LOD: " << mLOD
				<< ". Not retrying." << llendl;
		LLMutex& mutex = gMeshRepo.mThread->mMutex;
		mutex.lock();
		gMeshRepo.mThread->mUnavailableLODs.emplace_back(mMeshParams, mLOD);
		mutex.unlock();
	}
}

LLMeshSkinInfoHandler::~LLMeshSkinInfoHandler()
{
	llassert(mProcessed || LLApp::isExiting());
}

void LLMeshSkinInfoHandler::processFailure(LLCore::HttpStatus status)
{
	llwarns << "Error during mesh skin info handling. ID: " << mMeshID
			<< " - Reason: " << status.toString() << " ("
			<< status.toTerseString() << "). Not retrying." << llendl;

	LLMutex& mutex = gMeshRepo.mThread->mMutex;
	mutex.lock();
	gMeshRepo.mThread->mUnavailableSkins.emplace_back(mMeshID);
	mutex.unlock();
}

void LLMeshSkinInfoHandler::processData(LLCore::BufferArray*, S32, U8* data,
										S32 data_size)
{
	LL_TRACY_TIMER(TRC_MESH_PROCESS_SKIN);

	if (data && data_size > 0 &&
		gMeshRepo.mThread->skinInfoReceived(mMeshID, data, data_size))
	{
		// Good fetch from sim, write to cache
		LLFileSystem file(mMeshID, LLFileSystem::WRITE);

		S32 offset = mOffset;
		S32 size = mRequestedBytes;
		if (file.getSize() >= MESH_HEADER_SIZE)
		{
			LLMeshRepository::sCacheBytesWritten += size;
			++LLMeshRepository::sCacheWrites;
			file.seek(offset);	// Note: pads data if necessary. HB
			file.write(data, size);
		}
	}
	else
	{
		llwarns << "Error during mesh skin info processing. ID: " << mMeshID
				<< " - Unknown reason. Not retrying." << llendl;
		LLMutex& mutex = gMeshRepo.mThread->mMutex;
		mutex.lock();
		gMeshRepo.mThread->mUnavailableSkins.emplace_back(mMeshID);
		mutex.unlock();
	}
}

LLMeshDecompositionHandler::~LLMeshDecompositionHandler()
{
	llassert(mProcessed || LLApp::isExiting());
}

void LLMeshDecompositionHandler::processFailure(LLCore::HttpStatus status)
{
	llwarns << "Error during mesh decomposition handling. ID: " << mMeshID
			<< ", Reason: " << status.toString() << " ("
			<< status.toTerseString() << "). Not retrying." << llendl;
	// *TODO: mark mesh unavailable on error. For now, simply leave the request
	// unfulfilled rather than retrying forever.
}

void LLMeshDecompositionHandler::processData(LLCore::BufferArray*, S32,
											 U8* data, S32 data_size)
{
	LL_TRACY_TIMER(TRC_MESH_PROCESS_DECOMP);

	if (data && data_size > 0 &&
		gMeshRepo.mThread->decompositionReceived(mMeshID, data, data_size))
	{
		// Good fetch from sim, write to cache
		LLFileSystem file(mMeshID, LLFileSystem::WRITE);

		S32 offset = mOffset;
		S32 size = mRequestedBytes;
		if (file.getSize() >= MESH_HEADER_SIZE)
		{
			LLMeshRepository::sCacheBytesWritten += size;
			++LLMeshRepository::sCacheWrites;
			file.seek(offset);	// Note: pads data if necessary. HB
			file.write(data, size);
		}
	}
	else
	{
		llwarns << "Error during mesh decomposition processing. ID: "
				<< mMeshID << " - Unknown reason. Not retrying." << llendl;
		// *TODO: Mark mesh unavailable on error
	}
}

LLMeshPhysicsShapeHandler::~LLMeshPhysicsShapeHandler()
{
	llassert(mProcessed || LLApp::isExiting());
}

void LLMeshPhysicsShapeHandler::processFailure(LLCore::HttpStatus status)
{
	llwarns << "Error during mesh physics shape handling. ID: " << mMeshID
			<< ", Reason: " << status.toString() << " ("
			<< status.toTerseString() << "). Not retrying." << llendl;
	// *TODO: Mark mesh unavailable on error
}

void LLMeshPhysicsShapeHandler::processData(LLCore::BufferArray*, S32,
											U8* data, S32 data_size)
{
	LL_TRACY_TIMER(TRC_MESH_PROCESS_PHYSICS);

	if (data && data_size > 0 &&
		gMeshRepo.mThread->physicsShapeReceived(mMeshID, data, data_size))
	{
		// Good fetch from sim, write to cache
		LLFileSystem file(mMeshID, LLFileSystem::WRITE);

		S32 offset = mOffset;
		S32 size = mRequestedBytes;
		if (file.getSize() >= MESH_HEADER_SIZE)
		{
			LLMeshRepository::sCacheBytesWritten += size;
			++LLMeshRepository::sCacheWrites;
			file.seek(offset);	// Note: pads data if necessary. HB
			file.write(data, size);
		}
	}
	else
	{
		llwarns << "Error during mesh physics shape processing. ID: "
				<< mMeshID << " - Unknown reason. Not retrying." << llendl;
		// *TODO: mark mesh unavailable on error
	}
}

LLMeshRepository::LLMeshRepository()
:	mDecompThread(NULL),
	mThread(NULL)
{
}

void LLMeshRepository::init()
{
	LLConvexDecomposition::getInstance()->initSystem();

	mDecompThread = new LLPhysicsDecomp();
	mDecompThread->start();

	// Wait for physics decomp thread to init
	while (!mDecompThread->mInited)
	{
		ms_sleep(1);
	}

	mThread = new LLMeshRepoThread();
	mThread->start();
}

void LLMeshRepository::shutdown()
{
	llinfos << "Shutting down mesh repository." << llendl;

	if (!mThread)
	{
		llwarns << "NULL thread pointer: repository already shut down ?"
				<< llendl;
		llassert(false);
		return;
	}

	for (U32 i = 0; i < mUploads.size(); ++i)
	{
		llinfos << "Discard the pending mesh uploads " << llendl;
		mUploads[i]->discard();	// discard the uploading requests.
	}

	mThread->mSignal.broadcast();

	while (!mThread->isStopped())
	{
		ms_sleep(1);
	}
	delete mThread;
	mThread = NULL;

	for (U32 i = 0; i < mUploads.size(); ++i)
	{
		llinfos << "Waiting for pending mesh upload " << i + 1 << "/"
				<< mUploads.size() << llendl;
		while (!mUploads[i]->isStopped())
		{
			ms_sleep(1);
		}
		delete mUploads[i];
	}

	mUploads.clear();

	llinfos << "Shutting down decomposition system." << llendl;

	if (mDecompThread)
	{
		mDecompThread->shutdown();
		delete mDecompThread;
		mDecompThread = NULL;
	}

	LLConvexDecomposition::quitSystem();

	llinfos << "Clearing " << mSkinMap.size() << " cached skin info entries."
			<< llendl;
	mSkinMap.clear();

	llinfos << "Clearing " << mCostsMap.size() << " cached cost data entries."
			<< llendl;
	mCostsMap.clear();
}

// Called in the main thread.
S32 LLMeshRepository::update()
{
	if (mUploadWaitList.empty())
	{
		return 0;
	}

	S32 size = mUploadWaitList.size();
	for (S32 i = 0; i < size; ++i)
	{
		mUploads.push_back(mUploadWaitList[i]);
		mUploadWaitList[i]->preStart();
		mUploadWaitList[i]->start();
	}
	mUploadWaitList.clear();

	return size;
}

void LLMeshRepository::unregisterVolume(LLVOVolume* volp, bool has_mesh,
										bool has_skin)
{
	LL_TRACY_TIMER(TRC_MESH_UNREGISTER_VOLUME);

	mMeshMutex.lock();

	if (has_mesh)
	{
		for (U32 lod = 0; lod < 4; ++lod)
		{
			for (mesh_load_map_t::iterator it = mLoadingMeshes[lod].begin(),
										   end = mLoadingMeshes[lod].end();
				 it != end; )
			{
				it->second.erase(volp);
				if (it->second.empty())
				{
					it = mLoadingMeshes[lod].erase(it);
				}
				else
				{
					++it;
				}
			}
		}
	}

	if (has_skin)
	{
		for (skin_load_map_t::iterator it = mLoadingSkins.begin(),
									   end = mLoadingSkins.end();
				 it != end; )
		{
			it->second.erase(volp);
			if (it->second.empty())
			{
				it = mLoadingSkins.erase(it);
			}
			else
			{
				++it;
			}
		}		
	}

	mMeshMutex.unlock();
}

S32 LLMeshRepository::loadMesh(LLVOVolume* vobj,
							   const LLVolumeParams& mesh_params,
							   S32 detail, S32 last_lod)
{
	detail = llclamp(detail, 0, 3);
	LL_DEBUGS("MeshQueue") << "Requested LOD for mesh object "
						   << vobj->getID() << " = " << detail << LL_ENDL;

	// Mark this object address as registered in the mesh repository. HB
	vobj->setInMeshCache();

	mMeshMutex.lock();
	// Add volume to list of loading meshes
	const LLUUID& mesh_id = mesh_params.getSculptID();
	mesh_load_map_t::iterator iter = mLoadingMeshes[detail].find(mesh_id);
	if (iter != mLoadingMeshes[detail].end())
	{
		// Request pending for this mesh, append volume id to list
		LL_DEBUGS("MeshQueue") << "Adding object to pending requests for the associated mesh"
							   << LL_ENDL;
		iter->second.insert(vobj);
	}
	else
	{
		// First request for this mesh
		LL_DEBUGS("MeshQueue") << "Initiating request for the associated mesh"
							   << LL_ENDL;
		mLoadingMeshes[detail][mesh_id].insert(vobj);
		mPendingRequests.emplace_back(mesh_params, detail);
		++sLODPending;
	}
	mMeshMutex.unlock();

	// Do a quick search to see if we cannot display something while we wait
	// for this mesh to load
	LLVolume* volume = vobj->getVolume();
	if (volume)
	{
		LLVolumeParams params = volume->getParams();

		LLVolumeLODGroup* group = gVolumeMgrp->getGroup(params);
		if (group)
		{
			// First, see if last_lod is available (do not transition down to
			// avoid funny popping à la SH-641)
			if (last_lod >= 0 && last_lod <= LLModel::LOD_HIGH)
			{
				LLVolume* lod = group->refLOD(last_lod);
				if (lod && lod->isMeshAssetLoaded() &&
					lod->getNumVolumeFaces() > 0)
				{
					group->derefLOD(lod);
					LL_DEBUGS("Mesh") << "Using last LOD (" << last_lod
									  << ") for mesh object" << vobj->getID()
									  << LL_ENDL;
					return last_lod;
				}
				group->derefLOD(lod);
			}

			// Next, see what the next higher LOD available might be
			for (S32 i = detail + 1; i <= LLModel::LOD_HIGH; ++i)
			{
				LLVolume* lod = group->refLOD(i);
				if (lod && lod->isMeshAssetLoaded() &&
					lod->getNumVolumeFaces() > 0)
				{
					group->derefLOD(lod);
					LL_DEBUGS("Mesh") << "Using higher LOD = " << i
									  << " for mesh object" << vobj->getID()
									  << LL_ENDL;
					return i;
				}
				group->derefLOD(lod);
			}

			// No higher LOD is a available, is a lower lod available ?
			for (S32 i = detail - 1; i >= 0; --i)
			{
				LLVolume* lod = group->refLOD(i);
				if (lod && lod->isMeshAssetLoaded() &&
					lod->getNumVolumeFaces() > 0)
				{
					group->derefLOD(lod);
					LL_DEBUGS("Mesh") << "Using lower LOD = " << i
									  << " for mesh object" << vobj->getID()
									  << LL_ENDL;
					return i;
				}
				group->derefLOD(lod);
			}
		}
	}

	return detail;
}

// Called from main thread
void LLMeshRepository::notifyLoadedMeshes()
{
	LL_TRACY_TIMER(TRC_MESH_NOTIFY_LOADED);

	// Clean up completed upload threads
	for (std::vector<LLMeshUploadThread*>::iterator iter = mUploads.begin();
		 iter != mUploads.end(); )
	{
		LLMeshUploadThread* thread = *iter;

		if (thread->isStopped() && thread->finished())
		{
			iter = mUploads.erase(iter);
			delete thread;
		}
		else
		{
			++iter;
		}
	}

	// Update inventory
	if (!mInventoryQ.empty())
	{
		const LLUUID parent_id =
			gInventory.findChoosenCategoryUUIDForType(LLFolderType::FT_TEXTURE);

		mMeshMutex.lock();
		while (!mInventoryQ.empty())
		{
			InventoryData& data = mInventoryQ.front();

			LLAssetType::EType asset_type =
				LLAssetType::lookup(data.mPostData["asset_type"].asString());
			LLInventoryType::EType inventory_type =
				LLInventoryType::lookup(data.mPostData["inventory_type"].asString());

			// Handle addition of texture, if any.
			if (data.mResponse.has("new_texture_folder_id"))
			{
				LLUUID folder_id =
					data.mResponse["new_texture_folder_id"].asUUID();
				if (folder_id.notNull())
				{
					std::string name;
					// Check if the server built a different name for the
					// texture folder
					if (data.mResponse.has("new_texture_folder_name"))
					{
						name = data.mResponse["new_texture_folder_name"].asString();
					}
					else
					{
						name = data.mPostData["name"].asString();
					}

					// Add the category to the internal representation
					LLPointer<LLViewerInventoryCategory> catp =
						new LLViewerInventoryCategory(folder_id, parent_id,
													  LLFolderType::FT_NONE,
													  name, gAgentID);
					catp->setVersionUnknown();

					LLInventoryModel::LLCategoryUpdate u(catp->getParentUUID(),
														 1);
					gInventory.accountForUpdate(u);
					gInventory.updateCategory(catp);
				}
			}

			on_new_single_inventory_upload_complete(asset_type,
													inventory_type,
													data.mPostData["asset_type"].asString(),
													data.mPostData["folder_id"].asUUID(),
													data.mPostData["name"],
													data.mPostData["description"],
													data.mResponse,
													data.mResponse["upload_price"]);

			mInventoryQ.pop();
		}
		mMeshMutex.unlock();
	}

	// Call completed callbacks on finished decompositions
	mDecompThread->notifyCompleted();

	// For major operations, attempt to get the required locks without blocking
	// and punt if they are not available. The longest run of holdoffs is kept
	// in sMaxLockHoldoffs just to collect the data. In testing, I have never
	// seen a value greater than 2 (written to log on exit).
	{
		LLMutexTrylock lock1(mMeshMutex);
		LLMutexTrylock lock2(mThread->mMutex);

		static U32 hold_offs = 0;
		if (!lock1.isLocked() || !lock2.isLocked())
		{
			// If we cannot get the locks, skip and pick this up later.
			++hold_offs;
			sMaxLockHoldoffs = llmax(sMaxLockHoldoffs, hold_offs);
			return;
		}
		hold_offs = 0;

		static LLUUID region_id;
		LLViewerRegion* regionp = gAgent.getRegion();
		if (regionp && regionp->getRegionID() != region_id &&
			regionp->capabilitiesReceived())
		{
			// Update the mesh capability url
			region_id = regionp->getRegionID();
			bool is_v2 = false;
			mThread->mGetMeshCapability = regionp->getMeshUrl(&is_v2);
			mThread->mGetMeshVersion = is_v2 ? 2 : 1;
			S32 scale = 5;
			if (is_v2)
			{
				// GetMesh2 operation with keepalives, etc. With pipelining, we
				// will increase this. See llappcorehttp and llcorehttp for
				// discussion on connection strategies.
				LLAppCoreHttp& app_core_http = gAppViewerp->getAppCoreHttp();
				if (app_core_http.isPipelined(LLAppCoreHttp::AP_MESH2))
				{
					scale = 2 * LLAppCoreHttp::PIPELINING_DEPTH;
				}

				static LLCachedControl<U32> max_requests2(gSavedSettings,
														  "Mesh2MaxConcurrentRequests");
				LLMeshRepoThread::sMaxConcurrentRequests = max_requests2;
				LLMeshRepoThread::sRequestHighWater =
					llclamp(scale * (S32)max_requests2,
							REQUEST2_HIGH_WATER_MIN, REQUEST2_HIGH_WATER_MAX);
				LLMeshRepoThread::sRequestLowWater =
					llclamp(LLMeshRepoThread::sRequestHighWater / 2,
							REQUEST2_LOW_WATER_MIN, REQUEST2_LOW_WATER_MAX);
			}
			else
			{
				static LLCachedControl<U32> max_requests(gSavedSettings,
														 "MeshMaxConcurrentRequests");
				LLMeshRepoThread::sMaxConcurrentRequests = max_requests;
				LLMeshRepoThread::sRequestHighWater =
					llclamp(scale * (S32)max_requests,
							REQUEST_HIGH_WATER_MIN, REQUEST_HIGH_WATER_MAX);
				LLMeshRepoThread::sRequestLowWater =
					llclamp(LLMeshRepoThread::sRequestHighWater / 2,
							REQUEST_LOW_WATER_MIN, REQUEST_LOW_WATER_MAX);
			}
		}

		// Popup queued error messages from background threads
		while (!mUploadErrorQ.empty())
		{
			gNotifications.add("MeshUploadError", mUploadErrorQ.front());
			mUploadErrorQ.pop();
		}
		S32 active_count = LLMeshRepoThread::sActiveHeaderRequests +
						   LLMeshRepoThread::sActiveLODRequests;
		if (active_count < LLMeshRepoThread::sRequestLowWater)
		{
			S32 push_count = LLMeshRepoThread::sRequestHighWater -
							 active_count;
#if LL_PENDING_MESH_REQUEST_SORTING
			if ((S32)mPendingRequests.size() > push_count)
			{
				// More requests than the high-water limit allows so sort and
				// forward the most important. Calculate "score" for pending
				// requests.

				// Create score map
				fast_hmap<LLUUID, F32> score_map;
				for (U32 i = 0; i < 4; ++i)
				{
					for (mesh_load_map_t::iterator
							iter = mLoadingMeshes[i].begin(),
							end = mLoadingMeshes[i].end();
						 iter != end; ++iter)
					{
						F32 max_score = 0.f;
						for (auto obj_iter = iter->second.begin(),
											 end2 = iter->second.end();
							 obj_iter != end2; ++obj_iter)
						{
							LLViewerObject* object = *obj_iter;
							if (object)
							{
								LLDrawable* drawable = object->mDrawable;
								if (drawable)
								{
									F32 cur_score =
										drawable->getRadius() /
										llmax(drawable->mDistanceWRTCamera,
											  1.f);
									max_score = llmax(max_score, cur_score);
								}
							}
						}
						score_map.emplace(iter->first, max_score);
					}
				}

				// Set "score" for pending requests
				for (std::vector<LLMeshRepoThread::LODRequest>::iterator
						iter = mPendingRequests.begin(),
						end = mPendingRequests.end();
					 iter != end; ++iter)
				{
					iter->mScore = score_map[iter->mMeshParams.getSculptID()];
				}

				// Sort by "score"
				std::partial_sort(mPendingRequests.begin(),
								  mPendingRequests.begin() + push_count,
								  mPendingRequests.end(),
								  LLMeshRepoThread::CompareScoreGreater());
			}
			while (--push_count >= 0 && !mPendingRequests.empty())
			{
				LLMeshRepoThread::LODRequest& request =
					mPendingRequests.front();
				mThread->loadMeshLOD(request.mMeshParams, request.mLOD);
				mPendingRequests.erase(mPendingRequests.begin());
				--sLODPending;
			}
#else
			if (mPendingRequests.empty() && !mDelayedPendingRequests.empty())
			{
				// When all urgent requests have been processed, add back some
				// of the delayed requests into the queue...
				for (S32 i = 0;
					 i < LLMeshRepoThread::sRequestLowWater &&
					 !mDelayedPendingRequests.empty();
					 ++i)
				{
					mPendingRequests.emplace_back(mDelayedPendingRequests.front());
					mDelayedPendingRequests.pop_front();
				}
				LL_DEBUGS("MeshQueue") << "Re-inserted "
									   << mPendingRequests.size()
									   << " delayed mesh requests into the queue."
									   << LL_ENDL;
			}
			while (--push_count >= 0 && !mPendingRequests.empty())
			{
				LLMeshRepoThread::LODRequest& request =
					mPendingRequests.front();
				mThread->loadMeshLOD(request.mMeshParams, request.mLOD);
				mPendingRequests.pop_front();
				--sLODPending;
			}
#endif
		}

		// Send skin info requests
		while (!mPendingSkinRequests.empty())
		{
			mThread->loadMeshSkinInfo(mPendingSkinRequests.front());
			mPendingSkinRequests.pop();
		}

		// Send decomposition requests
		while (!mPendingDecompositionRequests.empty())
		{
			mThread->loadMeshDecomposition(mPendingDecompositionRequests.front());
			mPendingDecompositionRequests.pop();
		}

		// Send physics shapes decomposition requests
		while (!mPendingPhysicsShapeRequests.empty())
		{
			mThread->loadMeshPhysicsShape(mPendingPhysicsShapeRequests.front());
			mPendingPhysicsShapeRequests.pop();
		}

		mThread->notifyLoadedMeshes();
	}

	// Skins cache periodic culling, based on .
	static F32 last_culling = 0.f;
	constexpr F32 SKININFO_CULL_DELAY = 10.f;	// In seconds
	if (gFrameTimeSeconds - last_culling >= SKININFO_CULL_DELAY)
	{
		last_culling = gFrameTimeSeconds;
		for (skin_map_t::iterator it = mSkinMap.begin(), end = mSkinMap.end();
			 it != end; )
		{
			if (it->second->getNumRefs() == 1) // Just one ref (in mSkinMap) ?
			{
				it = mSkinMap.erase(it);
			}
			else
			{
				++it;
			}
		}
	}

	// There is only one wait(), but broadcast, just in case...
	mThread->mSignal.broadcast();
}

void LLMeshRepository::notifySkinInfoReceived(LLMeshSkinInfo* info)
{
	mSkinMap.emplace(info->mMeshID, info);

	mMeshMutex.lock();

	skin_load_map_t::const_iterator iter = mLoadingSkins.find(info->mMeshID);
	if (iter == mLoadingSkins.end())
	{
		// This may happen after far TPs, for example.
		mMeshMutex.unlock();
		LL_DEBUGS("MeshQueue") << "Received notification for a skin no more in the loading list: "
							   << info->mMeshID << LL_ENDL;
		return;
	}

	for (auto oit = iter->second.begin(), end = iter->second.end();
		 oit != end; ++oit)
	{
		LLVOVolume* vobj = *oit;
		if (vobj)
		{
			vobj->notifySkinInfoLoaded(info);
		}
	}
	mLoadingSkins.erase(iter);

	mMeshMutex.unlock();
}

void LLMeshRepository::notifySkinInfoUnavailable(const LLUUID& mesh_id)
{
	mMeshMutex.lock();

	skin_load_map_t::iterator iter = mLoadingSkins.find(mesh_id);
	if (iter != mLoadingSkins.end())
	{
		for (auto oit = iter->second.begin(), end = iter->second.end();
			 oit != end; ++oit)
		{
			LLVOVolume* vobj = *oit;
			if (vobj)
			{
				vobj->notifySkinInfoUnavailable();
			}
		}
		mLoadingSkins.erase(iter);
	}

	mMeshMutex.unlock();
}

#if !LL_PENDING_MESH_REQUEST_SORTING
void LLMeshRepository::delayCurrentRequests()
{
	mMeshMutex.lock();
	if (!mPendingRequests.empty())
	{
		LL_DEBUGS("MeshQueue") << "Delaying " << mPendingRequests.size()
							   << " pending mesh requests." << LL_ENDL;
		// Very likely (and super-fast) when we did not TP for a while...
		if (mDelayedPendingRequests.empty())
		{
			mDelayedPendingRequests.swap(mPendingRequests);
		}
		// May happen (especially with a slow network) when TPing shortly after
		// a previous TP has finished and meshes are still loading.
		else
		{
			// NOTE: the current pending meshes are emplaced at the end of
			// the queue of the delayed ones, in case we would be TPing back to
			// the previous region where the already delayed meshes were queued
			for (LLMeshRepoThread::lod_req_queue_t::iterator
					it = mPendingRequests.begin(),
					end = mPendingRequests.end();
				 it != end; ++it)
			{
				mDelayedPendingRequests.emplace_back(*it);
			}
			mPendingRequests.clear();
		}
		LL_DEBUGS("MeshQueue") << mDelayedPendingRequests.size()
							   << " pending mesh requests are in the delayed queue."
							   << LL_ENDL;
	}
	mMeshMutex.unlock();
}
#endif

void LLMeshRepository::notifyDecompositionReceived(LLModel::Decomposition* decomp)
{
	const LLUUID& mesh_id = decomp->mMeshID;
	decomp_map_t::iterator it = mDecompositionMap.find(mesh_id);
	if (it == mDecompositionMap.end())
	{
		// Just insert decomp into map
		mDecompositionMap.emplace(mesh_id, decomp);
		mLoadingDecompositions.erase(mesh_id);
	}
	else
	{
		// Merge decomp with existing entry
		it->second->merge(decomp);
		mLoadingDecompositions.erase(mesh_id);
		delete decomp;
	}
}

// Called from main thread
void LLMeshRepository::notifyMeshLoaded(const LLVolumeParams& mesh_params,
										LLVolume* volume)
{
	LL_TRACY_TIMER(TRC_MESH_NOTIFY_LOADED);

	if (!volume) return;

	mMeshMutex.lock();

	// Get the list of objects waiting to be notified this mesh is loaded
	S32 lod = LLVolumeLODGroup::getVolumeDetailFromScale(volume->getDetail());
	const LLUUID& mesh_id = mesh_params.getSculptID();
	mesh_load_map_t::iterator obj_iter = mLoadingMeshes[lod].find(mesh_id);
	if (obj_iter == mLoadingMeshes[lod].end())
	{
		mMeshMutex.unlock();
		return;
	}

	// Make sure target volume is still valid
	if (volume->getNumVolumeFaces() <= 0)
	{
		llwarns << "Mesh loading returned empty volume for mesh " << mesh_id
				 << llendl;
	}

	// Update system volume
	LLVolume* sys_volume = gVolumeMgrp->refVolume(mesh_params, lod);
	if (sys_volume)
	{
		sys_volume->copyVolumeFaces(volume);
		sys_volume->setMeshAssetLoaded(true);
		gVolumeMgrp->unrefVolume(sys_volume);
	}
	else
	{
		llwarns << "Could not find system volume for mesh " << mesh_id
				<< llendl;
	}

	// Notify waiting LLVOVolume instances that their requested mesh is
	// available
	for (auto it = obj_iter->second.begin(), end = obj_iter->second.end();
		 it != end; ++it)
	{
		LLVOVolume* vobj = *it;
		if (vobj)
		{
			vobj->notifyMeshLoaded();
		}
	}

	mLoadingMeshes[lod].erase(obj_iter);

	mMeshMutex.unlock();
}

// Called from main thread
void LLMeshRepository::notifyMeshUnavailable(const LLVolumeParams& mesh_params,
											 S32 lod)
{
	mMeshMutex.lock();

	// Get the list of objects waiting to be notified this mesh is loaded
	const LLUUID& mesh_id = mesh_params.getSculptID();
	mesh_load_map_t::iterator it = mLoadingMeshes[lod].find(mesh_id);
	if (it == mLoadingMeshes[lod].end())
	{
		mMeshMutex.unlock();
		return;
	}

	F32 detail = LLVolumeLODGroup::getVolumeScaleFromDetail(lod);

	for (auto oit = it->second.begin(), end = it->second.end();
		 oit != end; ++oit)
	{
		LLVOVolume* vobj = *oit;
		if (vobj)
		{
			LLVolume* obj_volume = vobj->getVolume();
			if (obj_volume && obj_volume->getDetail() == detail &&
				obj_volume->getParams() == mesh_params)
			{
				// Should force volume to find most appropriate LOD
				vobj->setVolume(obj_volume->getParams(), lod);
			}
		}
	}

	mLoadingMeshes[lod].erase(it);

	mMeshMutex.unlock();
}

S32 LLMeshRepository::getActualMeshLOD(const LLVolumeParams& mesh_params,
									   S32 lod)
{
	return mThread ? mThread->getActualMeshLOD(mesh_params, lod) : -1;
}

const LLMeshSkinInfo* LLMeshRepository::getSkinInfo(const LLUUID& mesh_id,
													LLVOVolume* req_obj)
{
	if (mesh_id.isNull())
	{
		return NULL;
	}

	skin_map_t::iterator iter = mSkinMap.find(mesh_id);
	if (iter != mSkinMap.end())
	{
		return iter->second;
	}

	// Mark this object address as registered in the skin repository. HB
	req_obj->setInSkinCache();

	// No skin info known about given mesh, try to fetch it
	mMeshMutex.lock();
	// Add volume to list of loading meshes
	if (!mLoadingSkins.count(mesh_id))
	{
		// No request pending for this skin info
		mPendingSkinRequests.emplace(mesh_id);
	}
	auto& obj_set = mLoadingSkins[mesh_id];
	obj_set.insert(req_obj);
	mMeshMutex.unlock();

	return NULL;
}

void LLMeshRepository::fetchPhysicsShape(const LLUUID& mesh_id)
{
	if (mesh_id.notNull())
	{
		LLModel::Decomposition* decomp = NULL;
		decomp_map_t::iterator iter = mDecompositionMap.find(mesh_id);
		if (iter != mDecompositionMap.end())
		{
			decomp = iter->second;
		}

		// Decomposition block has not been fetched yet
		if (!decomp || decomp->mPhysicsShapeMesh.empty())
		{
			mMeshMutex.lock();
			// Add volume to list of loading meshes
			if (!mLoadingPhysicsShapes.count(mesh_id))
			{
				// No request pending for this skin info
				mLoadingPhysicsShapes.emplace(mesh_id);
				mPendingPhysicsShapeRequests.emplace(mesh_id);
			}
			mMeshMutex.unlock();
		}
	}
}

LLModel::Decomposition* LLMeshRepository::getDecomposition(const LLUUID& mesh_id)
{
	LLModel::Decomposition* ret = NULL;

	if (mesh_id.notNull())
	{
		decomp_map_t::iterator iter = mDecompositionMap.find(mesh_id);
		if (iter != mDecompositionMap.end())
		{
			ret = iter->second;
		}

		// Decomposition block has not been fetched yet
		if (!ret || ret->mBaseHullMesh.empty())
		{
			mMeshMutex.lock();
			// Add volume to list of loading meshes
			if (!mLoadingDecompositions.count(mesh_id))
			{
				// No request pending for this skin info
				mLoadingDecompositions.emplace(mesh_id);
				mPendingDecompositionRequests.emplace(mesh_id);
			}
			mMeshMutex.unlock();
		}
	}

	return ret;
}

void LLMeshRepository::buildHull(const LLVolumeParams& params, S32 detail)
{
	LLVolume* volume = gVolumeMgrp->refVolume(params, detail);
#if 0
	if (volume && !volume->mHullPoints)
	{
		// all default params
		// execute first stage
		// set simplify mode to retain
		// set retain percentage to zero
		// run second stage
	}
#endif
	if (volume)
	{
		gVolumeMgrp->unrefVolume(volume);
	}
}

bool LLMeshRepository::hasPhysicsShape(const LLUUID& mesh_id)
{
	bool physics_shape = false;

	if (mThread)
	{
		mThread->mHeaderMutex.lock();
		LLMeshHeader* header = mThread->getMeshHeader(mesh_id);
		physics_shape = header && header->mPhysicsMeshSize > 0;
		mThread->mHeaderMutex.unlock();
	}

	if (!physics_shape)
	{
		LLModel::Decomposition* decomp = getDecomposition(mesh_id);
		physics_shape = decomp && !decomp->mHull.empty();
	}

	return physics_shape;
}

void LLMeshRepository::uploadModel(std::vector<LLModelInstance>& data,
								   LLVector3& scale, bool upload_textures,
								   bool upload_skin, bool upload_joints,
								   bool lock_scale_if_joint_position,
								   std::string upload_url, bool do_upload,
								   LLHandle<LLWholeModelFeeObserver> fee_observer,
								   LLHandle<LLWholeModelUploadObserver> upload_observer)
{
	LLMeshUploadThread* thread =
		new LLMeshUploadThread(data, scale, upload_textures, upload_skin,
							   upload_joints, lock_scale_if_joint_position,
							   upload_url, do_upload, fee_observer,
							   upload_observer);
	mUploadWaitList.push_back(thread);
}

S32 LLMeshRepository::getMeshSize(const LLUUID& mesh_id, S32 lod)
{
	S32 size = -1;

	if (mThread && mesh_id.notNull() && lod >= 0 && lod < 4)
	{
		mThread->mHeaderMutex.lock();
		LLMeshRepoThread::mesh_header_map_t::iterator iter =
			mThread->mMeshHeaders.find(mesh_id);
		if (iter != mThread->mMeshHeaders.end())
		{
			LLMeshHeader* header = iter->second;
			if (header->mValid)
			{
				size = header->mLodSize[lod];
			}
		}
		mThread->mHeaderMutex.unlock();
	}

	return size;
}

void LLMeshRepository::updateInventory(InventoryData data)
{
	mMeshMutex.lock();
	dump_llsd_to_file(data.mPostData,
					  make_dump_name("update_inventory_post_data_", sDumpNum));
	dump_llsd_to_file(data.mResponse,
					  make_dump_name("update_inventory_response_", sDumpNum));
	mInventoryQ.emplace(data);
	mMeshMutex.unlock();
}

void LLMeshRepository::uploadError(const LLSD& args)
{
	mMeshMutex.lock();
	mUploadErrorQ.emplace(args);
	mMeshMutex.unlock();
}

LLMeshCostData* LLMeshRepository::getCostData(const LLUUID& mesh_id)
{
    if (!mThread || mesh_id.isNull())
    {
		return NULL;
	}

	mesh_costs_map_t::iterator it = mCostsMap.find(mesh_id);
	if (it != mCostsMap.end())
	{
		LL_DEBUGS("MeshCost") << "Returning cached costs for mesh Id: "
							  << mesh_id << LL_ENDL;
		return it->second.get();
	}

	LLMeshCostData* cost_data = NULL;

	mThread->mHeaderMutex.lock();

	LLMeshRepoThread::mesh_header_map_t::iterator iter =
		mThread->mMeshHeaders.find(mesh_id);
	if (iter != mThread->mMeshHeaders.end())
	{
		LLMeshHeader* header = iter->second;
		if (header->mValid)
		{
			cost_data = new LLMeshCostData();
			if (cost_data->init(header))
			{
				LL_DEBUGS("MeshCost") << "Caching costs for mesh Id: "
									  << mesh_id << LL_ENDL;
				mCostsMap.emplace(mesh_id, cost_data);
			}
			else
			{
				llwarns << "Failed to compute costs for mesh Id: " << mesh_id
						<< llendl;
				delete cost_data;
				cost_data = NULL;
			}
		}
	}

	mThread->mHeaderMutex.unlock();

	return cost_data;
}

F32 LLMeshRepository::getEstTrianglesMax(const LLUUID& mesh_id)
{
	LLMeshCostData* cost_data = getCostData(mesh_id);
	return cost_data ? cost_data->getEstTrisMax() : 0.f;
}

F32 LLMeshRepository::getEstTrianglesStreamingCost(const LLUUID& mesh_id)
{
	LLMeshCostData* cost_data = getCostData(mesh_id);
	return cost_data ? cost_data->getEstTrisForStreamingCost() : 0.f;
}

void LLMeshRepository::buildPhysicsMesh(LLModel::Decomposition& decomp)
{
	decomp.mMesh.resize(decomp.mHull.size());

	LLConvexDecomposition* decompinst = LLConvexDecomposition::getInstance();

	for (U32 i = 0; i < decomp.mHull.size(); ++i)
	{
		LLCDHull hull;
		hull.mNumVertices = decomp.mHull[i].size();
		hull.mVertexBase = decomp.mHull[i][0].mV;
		hull.mVertexStrideBytes = 12;

		LLCDMeshData mesh;
		LLCDResult res = LLCD_OK;
		if (decompinst)
		{
			res = decompinst->getMeshFromHull(&hull, &mesh);
		}
		if (res == LLCD_OK)
		{
			get_vertex_buffer_from_mesh(mesh, decomp.mMesh[i]);
		}
	}

	if (!decomp.mBaseHull.empty() && decomp.mBaseHullMesh.empty())
	{
		// Get mesh for base hull
		LLCDHull hull;
		hull.mNumVertices = decomp.mBaseHull.size();
		hull.mVertexBase = decomp.mBaseHull[0].mV;
		hull.mVertexStrideBytes = 12;

		LLCDMeshData mesh;
		LLCDResult res = LLCD_OK;
		if (decompinst)
		{
			res = decompinst->getMeshFromHull(&hull, &mesh);
		}
		if (res == LLCD_OK)
		{
			get_vertex_buffer_from_mesh(mesh, decomp.mBaseHullMesh);
		}
	}
}

bool LLMeshRepository::meshUploadEnabled()
{
	LLViewerRegion* region = gAgent.getRegion();
	return region && region->meshUploadEnabled();
}

bool LLMeshRepository::meshRezEnabled()
{
	LLViewerRegion* region = gAgent.getRegion();
	return region && region->meshRezEnabled();
}

void LLMeshUploadThread::decomposeMeshMatrix(LLMatrix4& transformation,
											 LLVector3& result_pos,
											 LLQuaternion& result_rot,
											 LLVector3& result_scale)
{
	// Check for reflection
	bool reflected = transformation.determinant() < 0;

	// Compute position
	LLVector3 position = LLVector3::zero * transformation;

	// Compute scale
	LLVector3 x_transformed = LLVector3::x_axis * transformation - position;
	LLVector3 y_transformed = LLVector3::y_axis * transformation - position;
	LLVector3 z_transformed = LLVector3::z_axis * transformation - position;
	F32 x_length = x_transformed.normalize();
	F32 y_length = y_transformed.normalize();
	F32 z_length = z_transformed.normalize();
	LLVector3 scale = LLVector3(x_length, y_length, z_length);

    // Adjust for "reflected" geometry
	LLVector3 x_transformed_reflected = x_transformed;
	if (reflected)
	{
		x_transformed_reflected *= -1.0;
	}

	// Compute rotation
	LLMatrix3 rotation_matrix;
	rotation_matrix.setRows(x_transformed_reflected, y_transformed,
							z_transformed);
	LLQuaternion quat_rotation = rotation_matrix.quaternion();
	// The rotation_matrix might not have been orthoginal, make it so here:
	quat_rotation.normalize();
	LLVector3 euler_rotation;
	quat_rotation.getEulerAngles(&euler_rotation.mV[VX],
								 &euler_rotation.mV[VY],
								 &euler_rotation.mV[VZ]);

	result_pos = position + mOrigin;
	result_scale = scale;
	result_rot = quat_rotation;

	LL_DEBUGS("MeshUpload") << "Mesh matrix decomposed." << LL_ENDL;
}

///////////////////////////////////////////////////////////////////////////////
// LLPhysicsDecomp class
///////////////////////////////////////////////////////////////////////////////

LLPhysicsDecomp::LLPhysicsDecomp()
:	LLThread("Physics decomposition"),
	mInited(false),
	mQuitting(false),
	mDone(false)
{
}

LLPhysicsDecomp::~LLPhysicsDecomp()
{
	shutdown();
}

void LLPhysicsDecomp::shutdown()
{
	mQuitting = true;
	mSignal.signal();

	while (!isStopped())
	{
		ms_sleep(1);
	}
}

void LLPhysicsDecomp::submitRequest(LLPhysicsDecomp::Request* request)
{
	mMutex.lock();
	mRequestQ.push(request);
	mSignal.signal();
	mMutex.unlock();
}

//static
S32 LLPhysicsDecomp::llcdCallback(const char* status, S32 p1, S32 p2)
{
	if (!gMeshRepo.mDecompThread ||
		 gMeshRepo.mDecompThread->mCurRequest.isNull())
	{
		return 1;
	}
	return gMeshRepo.mDecompThread->mCurRequest->statusCallback(status, p1,
																p2);
}

// Helper function
static bool need_triangles(LLConvexDecomposition* decomp)
{
	if (!decomp)
	{
		return false;
	}

	const LLCDParam* params = NULL;
	S32 n_params = decomp->getParameters(&params);
	if (n_params <= 0)
	{
		return false;
	}

	for (S32 i = 0; i < n_params; ++i)
	{
		if (params[i].mName &&
			strcmp("nd_AlwaysNeedTriangles", params[i].mName) == 0)
		{
			return LLCDParam::LLCD_BOOLEAN == params[i].mType &&
				   params[i].mDefault.mBool;
		}
	}

	return false;
}

void LLPhysicsDecomp::setMeshData(LLCDMeshData& mesh, bool vertex_based)
{
	LLConvexDecomposition* decomp = LLConvexDecomposition::getInstance();

	if (vertex_based)
	{
		vertex_based = !need_triangles(decomp);
	}

	mesh.mVertexBase = mCurRequest->mPositions[0].mV;
	mesh.mVertexStrideBytes = 12;
	mesh.mNumVertices = mCurRequest->mPositions.size();

	if (!vertex_based)
	{
		mesh.mIndexType = LLCDMeshData::INT_16;
		mesh.mIndexBase = &(mCurRequest->mIndices[0]);
		mesh.mIndexStrideBytes = 6;

		mesh.mNumTriangles = mCurRequest->mIndices.size() / 3;
	}

	if ((vertex_based || mesh.mNumTriangles > 0) && mesh.mNumVertices > 2)
	{
		LLCDResult ret = LLCD_OK;
		if (decomp)
		{
			ret  = decomp->setMeshData(&mesh, vertex_based);
		}
		if (ret)
		{
			llerrs << "Convex Decomposition thread valid but could not set mesh data"
				   << llendl;
		}
	}
}

void LLPhysicsDecomp::doDecomposition()
{
	LLConvexDecomposition* decomp = LLConvexDecomposition::getInstance();
	if (!decomp)
	{
		// Stub library, do nothing.
		return;
	}

	LLCDMeshData mesh;
	S32 stage = mStageID[mCurRequest->mStage];
	// Load data into LLCD
	if (stage == 0)
	{
		setMeshData(mesh, false);
	}

	// Build parameter map
	std::map<std::string, const LLCDParam*> param_map;

	static const LLCDParam* params = NULL;
	static S32 param_count = 0;
	if (!params)
	{
		param_count = decomp->getParameters(&params);
	}

	for (S32 i = 0; i < param_count; ++i)
	{
		param_map[params[i].mName] = params + i;
	}

	U32 ret = LLCD_OK;
	// Set parameter values
	for (decomp_params_t::iterator iter = mCurRequest->mParams.begin(),
								   end = mCurRequest->mParams.end();
		 iter != end; ++iter)
	{
		const std::string& name = iter->first;
		const LLSD& value = iter->second;

		const LLCDParam* param = param_map[name];

		if (!param)
		{
			// Could not find a valid parameter
			continue;
		}

		if (param->mType == LLCDParam::LLCD_FLOAT)
		{
			ret = decomp->setParam(param->mName, (F32)value.asReal());
		}
		else if (param->mType == LLCDParam::LLCD_INTEGER ||
				 param->mType == LLCDParam::LLCD_ENUM)
		{
			ret = decomp->setParam(param->mName, value.asInteger());
		}
		else if (param->mType == LLCDParam::LLCD_BOOLEAN)
		{
			ret = decomp->setParam(param->mName, value.asBoolean());
		}
	}

	mCurRequest->setStatusMessage("Executing.");

	ret = decomp->executeStage(stage);
	if (ret)
	{
		llwarns << "Convex decomposition thread valid but could not execute stage "
				<< stage << llendl;
		mMutex.lock();
		mCurRequest->mHull.clear();
		mCurRequest->mHullMesh.clear();
		mCurRequest->setStatusMessage("FAIL");
		mCompletedQ.push(std::move(mCurRequest));
		mCurRequest = NULL;
		mMutex.unlock();
	}
	else
	{
		mCurRequest->setStatusMessage("Reading results");

		S32 num_hulls = decomp->getNumHullsFromStage(stage);

		mMutex.lock();
		mCurRequest->mHull.clear();
		mCurRequest->mHull.resize(num_hulls);
		mCurRequest->mHullMesh.clear();
		mCurRequest->mHullMesh.resize(num_hulls);
		mMutex.unlock();

		for (S32 i = 0; i < num_hulls; ++i)
		{
			std::vector<LLVector3> p;
			LLCDHull hull;
			decomp->getHullFromStage(stage, i, &hull);

			const F32* v = hull.mVertexBase;

			for (S32 j = 0; j < hull.mNumVertices; ++j)
			{
				p.emplace_back(v[0], v[1], v[2]);
				v = (F32*) (((U8*) v) + hull.mVertexStrideBytes);
			}

			LLCDMeshData mesh;
			decomp->getMeshFromStage(stage, i, &mesh);

			get_vertex_buffer_from_mesh(mesh, mCurRequest->mHullMesh[i]);

			mMutex.lock();
			mCurRequest->mHull[i] = std::move(p);
			mMutex.unlock();
		}

		mMutex.lock();
		mCurRequest->setStatusMessage("FAIL");
		mCompletedQ.push(std::move(mCurRequest));
		mCurRequest = NULL;
		mMutex.unlock();
	}
}

void LLPhysicsDecomp::notifyCompleted()
{
	if (!mCompletedQ.empty())
	{
		mMutex.lock();
		while (!mCompletedQ.empty())
		{
			Request* req = mCompletedQ.front();
			req->completed();
			mCompletedQ.pop();
		}
		mMutex.unlock();
	}
}

void make_box(LLPhysicsDecomp::Request* request)
{
	LLVector3 min = request->mPositions[0];
	LLVector3 max(min);

	for (U32 i = 0; i < request->mPositions.size(); ++i)
	{
		update_min_max(min, max, request->mPositions[i]);
	}

	request->mHull.clear();

	LLModel::hull box;
	box.emplace_back(min[0], min[1], min[2]);
	box.emplace_back(max[0], min[1], min[2]);
	box.emplace_back(min[0], max[1], min[2]);
	box.emplace_back(max[0], max[1], min[2]);
	box.emplace_back(min[0], min[1], max[2]);
	box.emplace_back(max[0], min[1], max[2]);
	box.emplace_back(min[0], max[1], max[2]);
	box.emplace_back(max[0], max[1], max[2]);

	request->mHull.emplace_back(box);
}

void LLPhysicsDecomp::doDecompositionSingleHull()
{
	LLConvexDecomposition* decomp = LLConvexDecomposition::getInstance();
	if (!decomp)
	{
		// Stub. Do nothing.
		return;
	}

	LLCDMeshData mesh;
	setMeshData(mesh, true);

	LLCDResult ret = decomp->buildSingleHull();
	if (ret)
	{
		llwarns << "Could not execute decomposition stage when attempting to create single hull."
				<< llendl;
		make_box(mCurRequest);
	}
	else
	{
		mMutex.lock();
		mCurRequest->mHull.clear();
		mCurRequest->mHull.resize(1);
		mCurRequest->mHullMesh.clear();
		mMutex.unlock();

		std::vector<LLVector3> p;
		LLCDHull hull;
		decomp->getSingleHull(&hull);

		const F32* v = hull.mVertexBase;

		for (S32 j = 0; j < hull.mNumVertices; ++j)
		{
			p.emplace_back(v[0], v[1], v[2]);
			v = (F32*)(((U8*)v) + hull.mVertexStrideBytes);
		}

		mMutex.lock();
		mCurRequest->mHull[0] = std::move(p);
		mMutex.unlock();
	}

	mMutex.lock();
	mCompletedQ.push(std::move(mCurRequest));
	mCurRequest = NULL;
	mMutex.unlock();
}

void LLPhysicsDecomp::run()
{
	LLConvexDecomposition* decomp = LLConvexDecomposition::getInstance();
	if (!decomp)
	{
		// Stub library. Set init to true so the main thread does not wait for
		// this to finish.
		mInited = true;
		return;
	}

	decomp->initThread();
	mInited = true;

	static const LLCDStageData* stages = NULL;
	static S32 num_stages = 0;

	if (!stages)
	{
		num_stages = decomp->getStages(&stages);
	}

	for (S32 i = 0; i < num_stages; ++i)
	{
		mStageID[stages[i].mName] = i;
	}

	while (!mQuitting)
	{
		mSignal.wait();
		while (!mQuitting && !mRequestQ.empty())
		{
			mMutex.lock();
			mCurRequest = mRequestQ.front();
			mRequestQ.pop();
			mMutex.unlock();

			S32& id = *(mCurRequest->mDecompID);
			if (id == -1)
			{
				decomp->genDecomposition(id);
			}
			decomp->bindDecomposition(id);

			if (mCurRequest->mStage == "single_hull")
			{
				doDecompositionSingleHull();
			}
			else
			{
				doDecomposition();
			}
		}
	}

	decomp->quitThread();

	if (mSignal.isLocked())
	{
		// Let go of mSignal's associated mutex
		mSignal.unlock();
	}

	mDone = true;
}

void LLPhysicsDecomp::Request::assignData(LLModel* mdl)
{
	if (!mdl)
	{
		return;
	}

	U16 index_offset = 0;
	U16 tri[3];

	mPositions.clear();
	mIndices.clear();
	mBBox[1] = LLVector3(F32_MIN, F32_MIN, F32_MIN);
	mBBox[0] = LLVector3(F32_MAX, F32_MAX, F32_MAX);

	// Queue up vertex positions and indices
	for (S32 i = 0, count = mdl->getNumVolumeFaces(); i < count; ++i)
	{
		const LLVolumeFace& face = mdl->getVolumeFace(i);
		if (mPositions.size() + face.mNumVertices > 65535)
		{
			continue;
		}

		for (S32 j = 0; j < face.mNumVertices; ++j)
		{
			mPositions.emplace_back(face.mPositions[j].getF32ptr());
			for (U32 k = 0; k < 3; ++k)
			{
				mBBox[0].mV[k] = llmin(mBBox[0].mV[k], mPositions[j].mV[k]);
				mBBox[1].mV[k] = llmax(mBBox[1].mV[k], mPositions[j].mV[k]);
			}
		}

		updateTriangleAreaThreshold();

		for (S32 j = 0; j + 2 < face.mNumIndices; )
		{
			tri[0] = face.mIndices[j++] + index_offset;
			tri[1] = face.mIndices[j++] + index_offset;
			tri[2] = face.mIndices[j++] + index_offset;

			if (isValidTriangle(tri[0], tri[1], tri[2]))
			{
				mIndices.push_back(tri[0]);
				mIndices.push_back(tri[1]);
				mIndices.push_back(tri[2]);
			}
		}

		index_offset += face.mNumVertices;
	}
}

void LLPhysicsDecomp::Request::updateTriangleAreaThreshold()
{
	F32 range = mBBox[1].mV[0] - mBBox[0].mV[0];
	range = llmin(range, mBBox[1].mV[1] - mBBox[0].mV[1]);
	range = llmin(range, mBBox[1].mV[2] - mBBox[0].mV[2]);

	mTriangleAreaThreshold = llmin(0.0002f, range * 0.000002f);
}

// Checks if the triangle area is large enough to qualify for a valid triangle
bool LLPhysicsDecomp::Request::isValidTriangle(U16 idx1, U16 idx2, U16 idx3)
{
	LLVector3 a = mPositions[idx2] - mPositions[idx1];
	LLVector3 b = mPositions[idx3] - mPositions[idx1];
	F32 c = a * b;
	return (a * a) * (b * b) - c * c > mTriangleAreaThreshold;
}

void LLPhysicsDecomp::Request::setStatusMessage(const std::string& msg)
{
	mStatusMessage = msg;
}
