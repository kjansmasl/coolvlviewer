/**
 * @file llviewerparcelmgr.h
 * @brief Viewer-side representation of owned land
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

#ifndef LL_LLVIEWERPARCELMGR_H
#define LL_LLVIEWERPARCELMGR_H

#include "boost/function.hpp"
#include "boost/signals2.hpp"

#include "llframetimer.h"
#include "llhandle.h"
#include "llparcel.h"
#include "llvector3d.h"

#include "llparcelselection.h"

class LLMessageSystem;
class LLViewerTexture;
class LLViewerRegion;

// A dwell having this value will be displayed as Loading...
constexpr F32 DWELL_NAN = -1.f;

constexpr F32 PARCEL_POST_HEIGHT = 0.666f;

// Base class for people who want to "observe" changes in the viewer parcel
// selection.
class LLParcelSelectionObserver
{
public:
	virtual ~LLParcelSelectionObserver() = default;
	virtual void changed() = 0;
};

struct LLParcelData
{
	S32			mActualArea;
	S32			mBillableArea;
	S32			mSalePrice;
	S32			mAuctionId;
	F32			mGlobalX;
	F32			mGlobalY;
	F32			mGlobalZ;
	F32			mDwell;
	LLUUID		mParcelId;
	LLUUID		mOwnerId;
	LLUUID		mSnapshotId;
	std::string	mName;
	std::string	mDesc;
	std::string	mSimName;
	U8			mFlags;
};

class LLParcelInfoObserver
{
public:
	LLParcelInfoObserver()
	{
		mObserverHandle.bind(this);
	}

	virtual ~LLParcelInfoObserver()
	{
	}

	virtual void processParcelInfo(const LLParcelData& parcel_data) = 0;
	virtual void setParcelID(const LLUUID& parcel_id) = 0;
	virtual void setErrorStatus(S32 status, const std::string& reason) = 0;

	LL_INLINE LLHandle<LLParcelInfoObserver> getObserverHandle() const
	{
		return mObserverHandle;
	}

protected:
	LLRootHandle<LLParcelInfoObserver> mObserverHandle;
};

class LLViewerParcelMgr
{
protected:
	LOG_CLASS(LLViewerParcelMgr);

public:
	typedef boost::function<void (const LLVector3d&,
								  const bool& local)> tp_finished_cb_t;
	typedef boost::signals2::signal<void(const LLVector3d&,
										 const bool&)> tp_finished_sig_t;
	typedef boost::function<void()> parcel_changed_cb_t;
	typedef boost::signals2::signal<void()> parcel_changed_sig_t;

	LLViewerParcelMgr();

	void initClass();
	void cleanupClass();

	// Variable region size support
	void setRegionWidth(F32 region_size);

	LL_INLINE bool selectionEmpty() const			{ return !mSelected; }
	LL_INLINE F32 getSelectionWidth() const			{ return F32(mEastNorth.mdV[VX] -
																 mWestSouth.mdV[VX]); }
	LL_INLINE F32 getSelectionHeight() const		{ return F32(mEastNorth.mdV[VY] -
																 mWestSouth.mdV[VY]); }

	LL_INLINE bool getSelection(LLVector3d& min, LLVector3d& max)
	{
		min = mWestSouth;
		max = mEastNorth;
		return !selectionEmpty();
	}

	LLViewerRegion* getSelectionRegion();

	LL_INLINE F32 getDwelling() const				{ return mSelectedDwell;}

	void getDisplayInfo(S32* area, S32* claim, S32* rent, bool* for_sale,
						F32* dwell);

	// Returns selected area
	S32 getSelectedArea() const;

	void resetSegments(U8* segments);

	// write a rectangle's worth of line segments into the highlight array
	void writeHighlightSegments(F32 west, F32 south, F32 east, F32 north);

	// Write highlight segments from a packed bitmap of the appropriate
	// parcel.
	void writeSegmentsFromBitmap(U8* bitmap, U8* segments);

	void writeAgentParcelFromBitmap(U8* bitmap);

	// Select the collision parcel
	void selectCollisionParcel();

	// Select the parcel at a specific point
	LLParcelSelectionHandle selectParcelAt(const LLVector3d& pos_global);

	// Take the current rectangle select, and select the parcel contained
	// within it.
	LLParcelSelectionHandle selectParcelInRectangle();

	// Select a piece of land
	LLParcelSelectionHandle selectLand(const LLVector3d& corner1,
									   const LLVector3d& corner2,
									   bool snap_to_parcel);

	// Clear the selection, and stop drawing the highlight.
	void deselectLand();
	void deselectUnused();

	void addSelectionObserver(LLParcelSelectionObserver* obs);
	void removeSelectionObserver(LLParcelSelectionObserver* obs);
	void notifySelectionObservers();

	LL_INLINE void setSelectionVisible(bool b)		{ mRenderSelection = b; }

	bool isOwnedAt(const LLVector3d& pos_global) const;
	bool isOwnedSelfAt(const LLVector3d& pos_global) const;
	bool isOwnedOtherAt(const LLVector3d& pos_global) const;
	bool isSoundLocal(const LLVector3d& pos_global) const;
	bool canHearSound(const LLVector3d& pos_global) const;

	// Returns a reference counted pointer to current parcel selection.
	// Selection does not change to reflect new selections made by user.
	// Use this when implementing a task UI that refers to a specific
	// selection.
	LL_INLINE LLParcelSelectionHandle getParcelSelection() const
	{
		return mCurrentParcelSelection;
	}

	// Returns a reference counted pointer to current parcel selection.
	// Pointer tracks whatever the user has currently selected.
	// Use this when implementing an inspector UI.
	// http://en.wikipedia.org/wiki/Inspector_window
	LL_INLINE LLParcelSelectionHandle getFloatingParcelSelection() const
	{
		return mFloatingParcelSelection;
	}

	LL_INLINE LLParcel*	getAgentParcel() const		{ return mAgentParcel; }
	LLParcel* getSelectedOrAgentParcel() const;

	bool inAgentParcel(const LLVector3d& pos_global) const;

	LL_INLINE LLParcel* getHoverParcel() const
	{
		return mHoverRequestResult == PARCEL_RESULT_SUCCESS ? mHoverParcel
															: NULL;
	}

	LL_INLINE LLParcel* getCollisionParcel() const
	{
		return mRenderCollision ? mCollisionParcel : NULL;
	}

	// Can this agent build on the parcel he is on ?
	// Used for parcel property icons in status bar.
	bool allowAgentBuild(bool prelude_check = true) const;
	bool allowAgentBuild(const LLParcel* parcel) const;

	// Can this agent speak on the parcel he is on ?
	// Used for parcel property icons in status bar.
	bool allowAgentVoice() const;
	bool allowAgentVoice(const LLViewerRegion* region,
						 const LLParcel* parcel) const;

	// Can this agent start flying on this parcel ?
	// Used for parcel property icons in status bar.
	bool allowAgentFly(const LLViewerRegion* region,
					   const LLParcel* parcel) const;

	// Can this agent be pushed by llPushObject() on this parcel?
	// Used for parcel property icons in status bar.
	bool allowAgentPush(const LLViewerRegion* region,
						const LLParcel* parcel) const;

	// Can scripts written by non-parcel-owners run on the agent's current
	// parcel ?  Used for parcel property icons in status bar.
	bool allowAgentScripts(const LLViewerRegion* region,
						   const LLParcel* parcel) const;

	// Can the agent be damaged here ?
	// Used for parcel property icons in status bar.
	bool allowAgentDamage(const LLViewerRegion* region,
						  const LLParcel* parcel) const;

	LL_INLINE F32 getHoverParcelWidth() const
	{
		return F32(mHoverEastNorth.mdV[VX] - mHoverWestSouth.mdV[VX]);
	}

	LL_INLINE F32 getHoverParcelHeight() const
	{
		return F32(mHoverEastNorth.mdV[VY] - mHoverWestSouth.mdV[VY]);
	}

	// UTILITIES
	void render();
	void renderParcelCollision();

	void renderRect(const LLVector3d& west_south_bottom,
					const LLVector3d& east_north_top);
	void renderOneSegment(F32 x1, F32 y1, F32 x2, F32 y2, F32 height,
						  U8 direction, LLViewerRegion* regionp);
	void renderHighlightSegments(const U8* segments, LLViewerRegion* regionp);
	void renderCollisionSegments(U8* segments, bool use_pass,
								 LLViewerRegion* regionp);

	void sendParcelGodForceOwner(const LLUUID& owner_id);

	// Makes the selected parcel a content parcel.
	void sendParcelGodForceToContent();

	// Packs information about this parcel and send it to the region containing
	// the southwest corner of the selection.
	void sendParcelPropertiesUpdate(LLParcel* parcel,
									bool use_agent_region = false);

	// Takes an Access List flag, like AL_ACCESS or AL_BAN
	void sendParcelAccessListUpdate(U32 which);

	// Takes an Access List flag, like AL_ACCESS or AL_BAN
	void sendParcelAccessListRequest(U32 flags);

	// Dwell is not part of the usual parcel update information because the
	// simulator does not actually know the per-parcel dwell. Ack ! We have
	// to get it out of the database.
	void sendParcelDwellRequest();

	// If the point is outside the current hover parcel, request more data
	void setHoverParcel(const LLVector3d& pos_global);

	// Used to re-request agent parcel properties (with id omitted).
	bool requestParcelProperties(const LLVector3d& pos_global,
								 S32 id = UPDATE_AGENT_PARCEL_SEQ_ID);

	bool canAgentBuyParcel(LLParcel* parcel, bool for_group) const;

	void startBuyLand(bool is_for_group = false);
	void startSellLand();
	void startReleaseLand();
	void startDivideLand();
	void startJoinLand();
	void startDeedLandToGroup();
	void reclaimParcel();

	void buyPass();

	// Buying Land

	struct ParcelBuyInfo;
	ParcelBuyInfo* setupParcelBuy(const LLUUID& agent_id,
								  const LLUUID& session_id,
								  const LLUUID& group_id,
								  bool is_group_owned, bool is_claim,
								  bool remove_contribution);
	// Callers responsibility to call deleteParcelBuy() on return value
	void sendParcelBuy(ParcelBuyInfo*);
	void deleteParcelBuy(ParcelBuyInfo*&);

	void sendParcelDeed(const LLUUID& group_id);

	// Send the ParcelRelease message
	void sendParcelRelease();

	// Accessors for mAgentParcel
	LL_INLINE const std::string& getAgentParcelName() const
	{
		return mAgentParcel->getName();
	}

	S32 getAgentParcelId() const;

	LL_INLINE bool waitingForParcelInfo() const
	{
		return mTeleportInProgress;
	}

#if 0	// *NOTE: Taken out 2005-03-21. Phoenix.
	// Create a landmark at the "appropriate" location for the currently
	// selected parcel.
	void makeLandmarkAtSelection();
#endif

	void resetCollisionSegments();

	static void processParcelOverlay(LLMessageSystem* msg, void** data);
	static void processParcelProperties(LLMessageSystem* msg, void** data);
	static void processParcelAccessListReply(LLMessageSystem* msg,
											 void** data);
	static void processParcelDwellReply(LLMessageSystem* msg, void** data);

	void dump();

	// Whether or not the collision border around the parcel is there because
	// the agent is banned or not in the allowed group
	bool isCollisionBanned();

	boost::signals2::connection addAgentParcelChangedCB(parcel_changed_cb_t cb);
	boost::signals2::connection setTPArrivingCallback(parcel_changed_cb_t cb);
	boost::signals2::connection setTPFinishedCallback(tp_finished_cb_t cb);
	boost::signals2::connection setTPFailedCallback(parcel_changed_cb_t cb);
	void onTeleportFinished(bool local, const LLVector3d& new_pos);
	void onTeleportFailed();

	static bool isParcelOwnedByAgent(const LLParcel* parcelp,
									 U64 group_proxy_power);
	static bool isParcelModifiableByAgent(const LLParcel* parcelp,
										  U64 group_proxy_power);

	// Methods for parcel info observers (declared in llremoteparcelrequest.h
	// in v3 viewers).

	void addInfoObserver(const LLUUID& parcel_id, LLParcelInfoObserver* obs);
	void removeInfoObserver(const LLUUID& parcel_id,
							LLParcelInfoObserver* obs);

	void sendParcelInfoRequest(const LLUUID& parcel_id);

	bool requestRegionParcelInfo(const std::string& url,
								 const LLUUID& region_id,
								 const LLVector3& region_pos,
								 const LLVector3d& global_pos,
								 LLHandle<LLParcelInfoObserver> obs_handle);

	static void processParcelInfoReply(LLMessageSystem* msg, void**);

private:
	static bool releaseAlertCB(const LLSD& notification, const LLSD& response);
	static void sendParcelAccessListUpdate(U32 flags,
										   const access_map_t& entries,
										   LLViewerRegion* region,
										   S32 parcel_local_id);

	// Moves land from current owner to its group.
	void deedLandToGroup();

	static bool deedAlertCB(const LLSD& notification, const LLSD& response);

	static bool callbackDivideLand(const LLSD& notification,
								   const LLSD& response);
	static bool callbackJoinLand(const LLSD& notification,
								 const LLSD& response);

	LLViewerTexture* getBlockedImage() const;
	LLViewerTexture* getPassImage() const;

    void regionParcelInfoCoro(const std::string& url, LLUUID region_id,
							  LLVector3 pos_region, LLVector3d pos_global,
							  LLHandle<LLParcelInfoObserver> obs_handle);

private:
	LLParcel*					mCurrentParcel;	// Selected parcel info
	LLParcelSelectionHandle		mCurrentParcelSelection;
	LLParcelSelectionHandle		mFloatingParcelSelection;
	S32							mRequestResult;	// Last parcel request result
	LLVector3d					mWestSouth;
	LLVector3d					mEastNorth;
	F32							mSelectedDwell;

	// Info for parcel agent is in
	LLParcel*					mAgentParcel;
	// Incrementing counter to suppress out of order updates
	S32							mAgentParcelSequenceID;

	LLParcel*					mHoverParcel;
	S32							mHoverRequestResult;
	LLVector3d					mHoverWestSouth;
	LLVector3d					mHoverEastNorth;

	typedef safe_hset<LLParcelSelectionObserver*> observers_list_t;
	observers_list_t			mSelectionObservers;

	bool						mSelected;

	bool						mTeleportInProgress;
	tp_finished_sig_t			mTeleportFinishedSignal;
	parcel_changed_sig_t		mTeleportArrivingSignal;
	parcel_changed_sig_t		mTeleportFailedSignal;
	parcel_changed_sig_t		mAgentParcelChangedSignal;

	// Array of pieces of parcel edges to potentially draw.
	// Has (parcels_per_edge + 1) * (parcels_per_edge + 1) elements so that we
	// can represent edges of the grid.
	// WEST_MASK = draw west edge
	// SOUTH_MASK = draw south edge
	S32							mParcelsPerEdge;
	U8*							mHighlightSegments;
	U8*							mAgentParcelOverlay;

	// Raw data buffer for unpacking parcel overlay chunks
	// Size = parcels_per_edge * parcels_per_edge / parcel_overlay_chunks
	static U8*					sPackedOverlay;

	// Watch for pending collisions with a parcel you can't access.
	// If it's coming, draw the parcel's boundaries.
	LLParcel*					mCollisionParcel;
	U8*							mCollisionSegments;
	bool						mRenderCollision;
	bool						mRenderSelection;
	S32							mCollisionBanned;
	LLFrameTimer				mCollisionTimer;
	LLViewerTexture* 			mBlockedImage;
	LLViewerTexture*			mPassImage;

	typedef std::multimap<LLUUID,
						  LLHandle<LLParcelInfoObserver> > info_obs_multimap_t;
	info_obs_multimap_t			mInfoObservers;
};

extern LLViewerParcelMgr gViewerParcelMgr;

void sanitize_corners(const LLVector3d& corner1,
					  const LLVector3d& corner2,
					  LLVector3d& west_south_bottom,
					  LLVector3d& east_north_top);

#endif
