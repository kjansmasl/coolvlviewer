/**
 * @file llworld.h
 * @brief Collection of viewer regions in the vacinity of the user.
 *
 * Represents the whole world, so far as 3D functionality is conserned.
 * Always contains the region that the user's avatar is in along with
 * neighboring regions. As the user crosses region boundaries, new
 * regions are added to the world and distant ones are rolled up.
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

#ifndef LL_LLWORLD_H
#define LL_LLWORLD_H

#include "boost/function.hpp"
#include "boost/signals2.hpp"

#include "llmath.h"
#include "llstring.h"
#include "llvector3.h"

#include "llpatchvertexarray.h"
#include "llviewerpartsim.h"
#include "llviewertexture.h"
#include "llvowater.h"

class LLCamera;
class LLCloudPuff;
class LLCloudGroup;
class LLColor4;
class LLCullResult;
class LLHost;
class LLMessageSystem;
class LLSurfacePatch;
class LLVector3d;
class LLViewerObject;
class LLViewerRegion;
class LLVOAvatar;

// Avoids a static constant. HB
#define EDGE_WATER_OBJECTS_COUNT 8

// LLWorld maintains a stack of unused viewer_regions and an array of pointers
// to viewer regions as simulators are connected to, viewer_regions are popped
// off the stack and connected as required as simulators are removed, they are
// pushed back onto the stack

class LLWorld
{
protected:
	LOG_CLASS(LLWorld);

public:
	LLWorld();

	void initClass();		// Called from LLStartUp::idleStartup()
	void cleanupClass();	// Called from LLAppViewer::disconnectViewer()

	// Safe to call if already present, does the "right thing" if hosts are
	// same, or if hosts are different, etc...
	LLViewerRegion* addRegion(const U64& region_handle, const LLHost& host,
							  // Variable region size support
							  U32 width);
	void removeRegion(const LLHost& host);
	// Send quit messages to all child regions
	void disconnectRegions();

	LLViewerRegion* getRegion(const LLHost& host);
	LLViewerRegion* getRegionFromPosGlobal(const LLVector3d& pos);
	LLViewerRegion* getRegionFromPosAgent(const LLVector3& pos);
	LLViewerRegion* getRegionFromHandle(const U64& handle);
	LLViewerRegion* getRegionFromID(const LLUUID& region_id);

	// 'true' if position is in valid region:
	bool positionRegionValidGlobal(const LLVector3d& pos);

	LLVector3d clipToVisibleRegions(const LLVector3d& start_pos,
									const LLVector3d& end_pos);

	// All of these should be in the agent coordinate frame
	LLViewerRegion* resolveRegionGlobal(LLVector3& localpos,
										const LLVector3d& position);
	LLViewerRegion* resolveRegionAgent(LLVector3& localpos,
									   const LLVector3& position);
	F32 resolveLandHeightGlobal(const LLVector3d& position);
	F32 resolveLandHeightAgent(const LLVector3& position);

	// Return the lowest allowed Z point to prevent objects from being moved
	// underground.
	F32 getMinAllowedZ(LLViewerObject* object);
	F32 getMinAllowedZ(LLViewerObject* object, const LLVector3d& global_pos);

	// Takes a line segment defined by point_a and point_b, then determines the
	// closest (to point_a) point of intersection that is on the land surface
	// or on an object of the world.
	// Stores results in "intersection" and "intersection_normal" and returns a
	// scalar value that is the normalized (by length of line segment) distance
	// along the line from "point_a" to "intersection".
	// Currently assumes point_a and point_b only differ in z-direction, but it
	// may eventually become more general.
	F32 resolveStepHeightGlobal(const LLVOAvatar* avatarp,
								const LLVector3d& pt_a, const LLVector3d& pt_b,
								LLVector3d& intersect, LLVector3& inter_norm,
								LLViewerObject** vobjp = NULL);

	LLSurfacePatch* resolveLandPatchGlobal(const LLVector3d& position);
	// Absolute frame:
	LLVector3 resolveLandNormalGlobal(const LLVector3d& position);

#if 0	// Scales other than 1.f are not currently supported...
	LL_INLINE F32 getRegionScale() const			{ return 1.f; }
#endif

	void updateRegions(F32 max_update_time);
	void updateVisibilities();
	void updateClouds(F32 dt);
	void killClouds();
	LLCloudGroup* findCloudGroup(const LLCloudPuff& puff);

	void renderPropertyLines();

	// Update network statistics for all the regions:
	void updateNetStats();

	void printPacketsLost();
	void requestCacheMisses();

	// deal with map object updates in the world.
	static void	 processCoarseUpdate(LLMessageSystem* msg, void** user_data);

	F32 getLandFarClip() const;
	void setLandFarClip(F32 far_clip);

	LL_INLINE LLViewerTexture* getDefaultWaterTexture()
	{
		return mDefaultWaterTexturep;
	}

	void updateWaterObjects();

	void precullWaterObjects(LLCamera& camera, LLCullResult* cull);

	void waterHeightRegionInfo(std::string const& sim_name, F32 water_height);
	void shiftRegions(const LLVector3& offset);

	void reloadAllSurfacePatches();

	void getInfo(LLSD& info);

	void clearAllVisibleObjects();

	typedef std::list<LLViewerRegion*> region_list_t;
	LL_INLINE const region_list_t& getRegionList() const
	{
		return mActiveRegionList;
	}

	typedef boost::signals2::signal<void(LLViewerRegion*)> region_remove_signal_t;
	boost::signals2::connection setRegionRemovedCallback(const region_remove_signal_t::slot_type& cb);

	// Returns lists of avatar IDs, their world-space positions and mini-map
	// colors within a given distance of a point. All arguments but avatar_ids
	// are optional. Given containers will be emptied and then filled. Not
	// supplying origin or radius input returns data on all avatars in the
	// known regions.
	void getAvatars(uuid_vec_t& avatar_ids,
					std::vector<LLVector3d>* positions = NULL,
					std::vector<LLColor4>* colors = NULL,
					const LLVector3d& relative_to = LLVector3d(),
					F32 radius = FLT_MAX) const;

	// Returns 'true' if the region is in mRegionList, 'false' if the region
	// has been removed due to region change or if the circuit to this
	// simulator had been lost.
	bool isRegionListed(const LLViewerRegion* region) const;

	static void processEnableSimulator(LLMessageSystem* msg, void**);
	static void processRegionHandshake(LLMessageSystem* msg, void**);
	static void processDisableSimulator(LLMessageSystem* msg, void**);
	static void idleDisableQueuedSim();

	static void sendAgentPause();
	static void sendAgentResume();

private:
	void clearHoleWaterObjects();
	void clearEdgeWaterObjects();

private:
	F32							mLastRegionDisabling;
	F32							mLandFarClip;	// Far clip distance for land.

	S32							mLastPacketsIn;
	S32							mLastPacketsOut;
	S32							mLastPacketsLost;
	U64							mLastCurlBytes;

	LLPatchVertexArray			mLandPatch;

	region_list_t				mActiveRegionList;
	region_list_t				mRegionList;
	region_list_t				mVisibleRegionList;
	region_list_t				mCulledRegionList;
	region_list_t				mDisabledRegionList;

	region_remove_signal_t		mRegionRemovedSignal;

	////////////////////////////
	//
	// Data for "Fake" objects
	//

	typedef std::list<LLPointer<LLVOWater> > water_obj_list_t;
	water_obj_list_t			mHoleWaterObjects;
	LLPointer<LLVOWater>		mEdgeWaterObjects[EDGE_WATER_OBJECTS_COUNT];

	LLPointer<LLViewerTexture>	mDefaultWaterTexturep;
};

extern LLWorld gWorld;

#endif
