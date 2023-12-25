/**
 * @file llviewerparcelmgr.cpp
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

#include "llviewerprecompiledheaders.h"

#include "llviewerparcelmgr.h"

#include "llaudioengine.h"
#include "llcachename.h"
#include "llcorehttputil.h"
#include "llgl.h"
#include "llnotifications.h"
#include "llregionhandle.h"
#include "llsdutil.h"
#include "llsdutil_math.h"
#include "llmessage.h"
#include "roles_constants.h"

#include "llagent.h"
#include "llappviewer.h"			// For gDisconnected
#include "llenvironment.h"
#include "llfloaterbuyland.h"
#include "llfloatersellland.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llstatusbar.h"
#include "llsurface.h"
#include "llviewercontrol.h"
#include "llviewerparcelmedia.h"
#include "llviewerparceloverlay.h"
#include "llviewerregion.h"
#include "llviewertexturelist.h"
#include "llworld.h"

constexpr F32 PARCEL_COLLISION_DRAW_SECS = 1.f;

constexpr U8 EAST_MASK  = 0x1 << EAST;
constexpr U8 NORTH_MASK = 0x1 << NORTH;
constexpr U8 WEST_MASK  = 0x1 << WEST;
constexpr U8 SOUTH_MASK = 0x1 << SOUTH;

// Global variables
LLViewerParcelMgr gViewerParcelMgr;
LLUUID gCurrentMovieID;

// Static variables
U8* LLViewerParcelMgr::sPackedOverlay = NULL;
LLPointer<LLViewerTexture> sBlockedImage;
LLPointer<LLViewerTexture> sPassImage;

LLViewerParcelMgr::LLViewerParcelMgr()
:	mSelected(false),
	mRequestResult(0),
	mSelectedDwell(DWELL_NAN),
	mAgentParcelSequenceID(-1),
	mHoverRequestResult(0),
	mRenderCollision(false),
	mRenderSelection(true),
	mCollisionBanned(0),
	mParcelsPerEdge((S32)(REGION_WIDTH_METERS / PARCEL_GRID_STEP_METERS)),
	mCurrentParcel(NULL),
	mCurrentParcelSelection(NULL),
	mFloatingParcelSelection(NULL),
	mAgentParcel(NULL),
	mHoverParcel(NULL),
	mCollisionParcel(NULL),
	mHighlightSegments(NULL),
	mCollisionSegments(NULL),
	mBlockedImage(NULL),
	mPassImage(NULL),
	mAgentParcelOverlay(NULL),
	// The initial parcel update is treated like teleport:
	mTeleportInProgress(true)
{
}

void LLViewerParcelMgr::initClass()
{
	mCurrentParcel = new LLParcel();
	mCurrentParcelSelection = new LLParcelSelection(mCurrentParcel);
	mFloatingParcelSelection = new LLParcelSelection(mCurrentParcel);

	mAgentParcel = new LLParcel();
	mHoverParcel = new LLParcel();
	mCollisionParcel = new LLParcel();

	mBlockedImage =
		LLViewerTextureManager::getFetchedTextureFromFile("noentrylines.j2c");
	mPassImage =
		LLViewerTextureManager::getFetchedTextureFromFile("noentrypasslines.j2c");

	// Variable region size support: 8192 is the maximum width for a region, so
	// let's allocate enough room for that...
	mParcelsPerEdge = (S32)(8192.f / PARCEL_GRID_STEP_METERS);
	S32 segments = (mParcelsPerEdge + 1) * (mParcelsPerEdge + 1);

	mHighlightSegments = new U8[segments];
	resetSegments(mHighlightSegments);

	mCollisionSegments = new U8[segments];
	resetSegments(mCollisionSegments);

	segments = mParcelsPerEdge * mParcelsPerEdge;
	sPackedOverlay = new U8[segments / PARCEL_OVERLAY_CHUNKS];

	mAgentParcelOverlay = new U8[segments];
	for (S32 i = 0; i < segments; ++i)
	{
		mAgentParcelOverlay[i] = 0;
	}

	mParcelsPerEdge = S32(REGION_WIDTH_METERS / PARCEL_GRID_STEP_METERS);

	llinfos << "Viewer parcel manager initialized." << llendl;
}

void LLViewerParcelMgr::cleanupClass()
{
	mCurrentParcelSelection->setParcel(NULL);
	mCurrentParcelSelection = NULL;

	mFloatingParcelSelection->setParcel(NULL);
	mFloatingParcelSelection = NULL;

	delete mCurrentParcel;
	mCurrentParcel = NULL;

	delete mAgentParcel;
	mAgentParcel = NULL;

	delete mCollisionParcel;
	mCollisionParcel = NULL;

	delete mHoverParcel;
	mHoverParcel = NULL;

	delete[] mHighlightSegments;
	mHighlightSegments = NULL;

	delete[] mCollisionSegments;
	mCollisionSegments = NULL;

	delete[] sPackedOverlay;
	sPackedOverlay = NULL;

	delete[] mAgentParcelOverlay;
	mAgentParcelOverlay = NULL;

	sBlockedImage = NULL;
	sPassImage = NULL;

	llinfos << "Viewer parcel manager cleaned up." << llendl;
}

// Variable region size support
void LLViewerParcelMgr::setRegionWidth(F32 region_size)
{
	mParcelsPerEdge = (S32)(region_size / PARCEL_GRID_STEP_METERS);
}

void LLViewerParcelMgr::dump()
{
	llinfos << "Parcel manager dump" << llendl;
	llinfos << "Selected: " << (mSelected ?  "true" : "false") << llendl;
	llinfos << "Selected parcel: " << llendl;
	llinfos << mWestSouth << " to " << mEastNorth << llendl;
	mCurrentParcel->dump();
	llinfos << "Ban list size: " << mCurrentParcel->mBanList.size() << llendl;

	for (access_map_t::const_iterator it = mCurrentParcel->mBanList.begin(),
									  end = mCurrentParcel->mBanList.end();
		 it != end; ++it)
	{
		llinfos << "Ban Id: " << it->first << llendl;
	}
	llinfos << "Hover parcel:" << llendl;
	mHoverParcel->dump();
	llinfos << "Agent parcel:" << llendl;
	mAgentParcel->dump();
}

LLViewerRegion* LLViewerParcelMgr::getSelectionRegion()
{
	return gWorld.getRegionFromPosGlobal(mWestSouth);
}

void LLViewerParcelMgr::getDisplayInfo(S32* area_out, S32* claim_out,
									   S32* rent_out, bool* for_sale_out,
									   F32* dwell_out)
{
	S32 area = 0;
	S32 price = 0;
	S32 rent = 0;
	bool for_sale = false;
	F32 dwell = DWELL_NAN;

	if (mSelected)
	{
		if (mCurrentParcelSelection->mSelectedMultipleOwners)
		{
			area = mCurrentParcelSelection->getClaimableArea();
		}
		else
		{
			area = getSelectedArea();
		}

		if (mCurrentParcel->getForSale())
		{
			price = mCurrentParcel->getSalePrice();
			for_sale = true;
		}
		else
		{
			price = area * mCurrentParcel->getClaimPricePerMeter();
			for_sale = false;
		}

		rent = mCurrentParcel->getTotalRent();

		dwell = mSelectedDwell;
	}

	*area_out = area;
	*claim_out = price;
	*rent_out = rent;
	*for_sale_out = for_sale;
	*dwell_out = dwell;
}

S32 LLViewerParcelMgr::getSelectedArea() const
{
	if (mSelected && mCurrentParcel &&
		mCurrentParcelSelection->mWholeParcelSelected)
	{
		return mCurrentParcel->getArea();
	}

	if (mSelected)
	{
		F64 width = mEastNorth.mdV[VX] - mWestSouth.mdV[VX];
		F64 height = mEastNorth.mdV[VY] - mWestSouth.mdV[VY];
		F32 area = (F32)(width * height);
		return ll_round(area);
	}

	return 0;
}

void LLViewerParcelMgr::resetSegments(U8* segments)
{
	for (S32 i = 0, count = (mParcelsPerEdge + 1) * (mParcelsPerEdge + 1);
		 i < count; ++i)
	{
		segments[i] = 0x0;
	}
}

void LLViewerParcelMgr::writeHighlightSegments(F32 west, F32 south, F32 east,
											   F32 north)
{
	S32 min_x = ll_round(west / PARCEL_GRID_STEP_METERS);
	S32 max_x = ll_round(east / PARCEL_GRID_STEP_METERS);
	S32 min_y = ll_round(south / PARCEL_GRID_STEP_METERS);
	S32 max_y = ll_round(north / PARCEL_GRID_STEP_METERS);

	const S32 stride = mParcelsPerEdge + 1;

	// South edge
	for (S32 x = min_x; x < max_x; ++x)
	{
		// Exclusive OR means that writing to this segment twice will turn it
		// off
		mHighlightSegments[x + min_y * stride] ^= SOUTH_MASK;
	}

	// West edge
	for (S32 y = min_y; y < max_y; ++y)
	{
		mHighlightSegments[min_x + y * stride] ^= WEST_MASK;
	}

	// North edge; draw the south border on the y+1'th cell, which given
	// C-style arrays, is item foo[max_y]
	for (S32 x = min_x; x < max_x; ++x)
	{
		mHighlightSegments[x + max_y * stride] ^= SOUTH_MASK;
	}

	// East edge; draw west border on x+1'th cell
	for (S32 y = min_y; y < max_y; ++y)
	{
		mHighlightSegments[max_x + y * stride] ^= WEST_MASK;
	}
}

void LLViewerParcelMgr::writeSegmentsFromBitmap(U8* bitmap, U8* segments)
{
	const S32 in_stride = mParcelsPerEdge;
	const S32 out_stride = in_stride + 1;

	for (S32 y = 0; y < in_stride; ++y)
	{
		S32 x = 0;
		while (x < in_stride)
		{
			U8 byte = bitmap[(x + y * in_stride) / 8];

			for (S32 bit = 0; bit < 8; ++bit)
			{
				if (byte & (1 << bit))
				{
					S32 out = x + y * out_stride;

					// This and one above it
					segments[out] ^= SOUTH_MASK;
					segments[out + out_stride] ^= SOUTH_MASK;

					// This and one to the right
					segments[out] ^= WEST_MASK;
					segments[out + 1] ^= WEST_MASK;
				}
				++x;
			}
		}
	}
}

void LLViewerParcelMgr::writeAgentParcelFromBitmap(U8* bitmap)
{
	const S32 in_stride = mParcelsPerEdge;

	for (S32 y = 0; y < in_stride; ++y)
	{
		S32 x = 0;
		while (x < in_stride)
		{
			U8 byte = bitmap[(x + y * in_stride) / 8];

			for (S32 bit = 0; bit < 8; ++bit)
			{
				if (byte & (1 << bit))
				{
					mAgentParcelOverlay[x + y * in_stride] = 1;
				}
				else
				{
					mAgentParcelOverlay[x + y * in_stride] = 0;
				}
				++x;
			}
		}
	}
}

// Given a point, find the PARCEL_GRID_STEP x PARCEL_GRID_STEP block
// containing it and select that.
LLParcelSelectionHandle LLViewerParcelMgr::selectParcelAt(const LLVector3d& pos)
{
	constexpr F64 witdh = (F64)PARCEL_GRID_STEP_METERS;
	constexpr F64 half_witdh = witdh * 0.5;

	LLVector3d southwest = pos - LLVector3d(half_witdh, half_witdh, 0.0);
	southwest.mdV[VX] = ll_round(southwest.mdV[VX], witdh);
	southwest.mdV[VY] = ll_round(southwest.mdV[VY], witdh);

	LLVector3d northeast = pos + LLVector3d(half_witdh, half_witdh, 0.0);
	northeast.mdV[VX] = ll_round(northeast.mdV[VX], witdh);
	northeast.mdV[VY] = ll_round(northeast.mdV[VY], witdh);

	// Snap to parcel
	return selectLand(southwest, northeast, true);
}

// Tries to select the parcel inside the rectangle
LLParcelSelectionHandle LLViewerParcelMgr::selectParcelInRectangle()
{
	return selectLand(mWestSouth, mEastNorth, true);
}

void LLViewerParcelMgr::resetCollisionSegments()
{
	resetSegments(mCollisionSegments);
}

void LLViewerParcelMgr::selectCollisionParcel()
{
	// *HACK: claim to be in the agent's region
	LLViewerRegion* regionp = gAgent.getRegion();
	if (!regionp) return;

	mWestSouth = regionp->getOriginGlobal();
#if 1	// Variable region size support
	constexpr F64 factor = (F64)PARCEL_GRID_STEP_METERS /
						   (F64)REGION_WIDTH_METERS;
	F64 width = (F64)regionp->getWidth() * factor;
	mEastNorth = mWestSouth + LLVector3d(width, width, 0.0);
#else
	mEastNorth = mWestSouth + LLVector3d(PARCEL_GRID_STEP_METERS,
										 PARCEL_GRID_STEP_METERS, 0.0);
#endif

	// *HACK: must be in the sim you are in
	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessageFast(_PREHASH_ParcelPropertiesRequestByID);
	msg->nextBlockFast(_PREHASH_AgentID);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlockFast(_PREHASH_ParcelData);
	msg->addS32Fast(_PREHASH_SequenceID, SELECTED_PARCEL_SEQ_ID);
	msg->addS32Fast(_PREHASH_LocalID, mCollisionParcel->getLocalID());
	gAgent.sendReliableMessage();

	mRequestResult = PARCEL_RESULT_NO_DATA;

	// *HACK: copy some data over temporarily
	mCurrentParcel->setName(mCollisionParcel->getName());
	mCurrentParcel->setDesc(mCollisionParcel->getDesc());
	mCurrentParcel->setPassPrice(mCollisionParcel->getPassPrice());
	mCurrentParcel->setPassHours(mCollisionParcel->getPassHours());

#if 0
	// Clear the list of segments to prevent flashing
	resetSegments(mHighlightSegments);
#endif

	mFloatingParcelSelection->setParcel(mCurrentParcel);
	mCurrentParcelSelection->setParcel(NULL);
	mCurrentParcelSelection = new LLParcelSelection(mCurrentParcel);

	mSelected = true;
	mCurrentParcelSelection->mWholeParcelSelected = true;
	notifySelectionObservers();
}

// Snap_selection = auto-select the hit parcel, if there is exactly one
LLParcelSelectionHandle LLViewerParcelMgr::selectLand(const LLVector3d& corner1,
													  const LLVector3d& corner2,
													  bool snap_selection)
{
	sanitize_corners(corner1, corner2, mWestSouth, mEastNorth);

	// ...x is not more than one meter away
	F32 delta_x = getSelectionWidth();
	if (delta_x * delta_x <= 1.f)
	{
		mSelected = false;
		notifySelectionObservers();
		return NULL;
	}

	// ...y is not more than one meter away
	F32 delta_y = getSelectionHeight();
	if (delta_y * delta_y <= 1.f)
	{
		mSelected = false;
		notifySelectionObservers();
		return NULL;
	}

	// Cannot select across region boundary. We need to pull in the upper right
	// corner by a little bit to allow selection up to the x = 256 or y = 256
	// edge.
	LLVector3d east_north_region_check(mEastNorth);
	east_north_region_check.mdV[VX] -= 0.5;
	east_north_region_check.mdV[VY] -= 0.5;

	LLViewerRegion* region = gWorld.getRegionFromPosGlobal(mWestSouth);
	if (!region)
	{
		// Just in case they somehow selected no land.
		mSelected = false;
		notifySelectionObservers();
		return NULL;
	}

	LLViewerRegion* region_other =
		gWorld.getRegionFromPosGlobal(east_north_region_check);
	if (region != region_other)
	{
		gNotifications.add("CantSelectLandFromMultipleRegions");
		mSelected = false;
		notifySelectionObservers();
		return NULL;
	}

	// Build region global copies of corners
	LLVector3 wsb_region = region->getPosRegionFromGlobal(mWestSouth);
	LLVector3 ent_region = region->getPosRegionFromGlobal(mEastNorth);

	// Send request message
	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessageFast(_PREHASH_ParcelPropertiesRequest);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlockFast(_PREHASH_ParcelData);
	msg->addS32Fast(_PREHASH_SequenceID, SELECTED_PARCEL_SEQ_ID);
	msg->addF32Fast(_PREHASH_West,  wsb_region.mV[VX]);
	msg->addF32Fast(_PREHASH_South, wsb_region.mV[VY]);
	msg->addF32Fast(_PREHASH_East,  ent_region.mV[VX]);
	msg->addF32Fast(_PREHASH_North, ent_region.mV[VY]);
	msg->addBool(_PREHASH_SnapSelection, snap_selection);
	msg->sendReliable(region->getHost());

	mRequestResult = PARCEL_RESULT_NO_DATA;

	// Clear the list of segments to prevent flashing
	resetSegments(mHighlightSegments);

	mFloatingParcelSelection->setParcel(mCurrentParcel);
	mCurrentParcelSelection->setParcel(NULL);
	mCurrentParcelSelection = new LLParcelSelection(mCurrentParcel);

	mSelected = true;
	mCurrentParcelSelection->mWholeParcelSelected = snap_selection;
	notifySelectionObservers();
	return mCurrentParcelSelection;
}

void LLViewerParcelMgr::deselectUnused()
{
	// No more outstanding references to this selection, other than our own
	if (mCurrentParcelSelection->getNumRefs() == 1 &&
		mFloatingParcelSelection->getNumRefs() == 1)
	{
		deselectLand();
	}
}

void LLViewerParcelMgr::deselectLand()
{
	if (mSelected)
	{
		mSelected = false;

		// Invalidate the selected parcel
		mCurrentParcel->setLocalID(-1);
		mCurrentParcel->mAccessList.clear();
		mCurrentParcel->mBanList.clear();
		//mCurrentParcel->mRenterList.reset();

		mSelectedDwell = DWELL_NAN;

		// Invalidate parcel selection so that existing users of this selection
		// can clean up
		mCurrentParcelSelection->setParcel(NULL);
		mFloatingParcelSelection->setParcel(NULL);
		// Create new parcel selection
		mCurrentParcelSelection = new LLParcelSelection(mCurrentParcel);

		// Notify observers *after* changing the parcel selection
		notifySelectionObservers();
	}
}

void LLViewerParcelMgr::addSelectionObserver(LLParcelSelectionObserver* obs)
{
	mSelectionObservers.insert(obs);
}

void LLViewerParcelMgr::removeSelectionObserver(LLParcelSelectionObserver* obs)
{
	mSelectionObservers.erase(obs);
}

// Call this method when it is time to update everyone on a new state.
void LLViewerParcelMgr::notifySelectionObservers()
{
	LL_DEBUGS("ParcelMgr") << "Notifying observers..." << LL_ENDL;
	for (observers_list_t::iterator it = mSelectionObservers.begin(),
									end = mSelectionObservers.end();
		 it != end; )
	{
		// Note: an observer could respond by removing itself from the list.
		LLParcelSelectionObserver* obs = *it++;
		if (obs)	// Paranoia
		{
			obs->changed();
		}
	}
}

LLParcel* LLViewerParcelMgr::getSelectedOrAgentParcel() const
{
	LLParcel* parcel = mAgentParcel;

	LLParcelSelectionHandle handle = mFloatingParcelSelection;
	if (handle)
	{
		LLParcelSelection* selection = handle.get();
		if (selection)
		{
			parcel = selection->getParcel();
			if (!parcel || parcel->getLocalID() == INVALID_PARCEL_ID)
			{
				parcel = mAgentParcel;
			}
		}
	}

	return parcel;
}

// Return whether the agent can build on the land they are on
bool LLViewerParcelMgr::allowAgentBuild(bool prelude_check) const
{
	if (!mAgentParcel)
	{
		return false;
	}
	if (gAgent.isGodlike())
	{
		return true;
	}
	// *HACK: The "prelude" Help Islands have a build sandbox area, so users
	// need the Edit and Create pie menu options when they are there, thus the
	// prelude_check flag (see LLEnableEdit in llviewermenu.cpp).
	if (prelude_check && gAgent.inPrelude())
	{
		return false;
	}
//MK
	if (gRLenabled &&
		(gRLInterface.mContainsRez || gRLInterface.mContainsEdit))
	{
		return false;
	}
//mk
	return mAgentParcel->getOwnerID() == gAgentID ||
		   mAgentParcel->allowModifyBy(gAgentID, gAgent.getGroupID()) ||
		   gAgent.hasPowerInGroup(mAgentParcel->getGroupID(),
								  GP_LAND_ALLOW_CREATE);
}

// Return whether anyone can build on the given parcel
bool LLViewerParcelMgr::allowAgentBuild(const LLParcel* parcel) const
{
	return parcel->getAllowModify();
}

bool LLViewerParcelMgr::allowAgentVoice() const
{
	return allowAgentVoice(gAgent.getRegion(), mAgentParcel);
}

bool LLViewerParcelMgr::allowAgentVoice(const LLViewerRegion* region,
										const LLParcel* parcel) const
{
	return region && region->isVoiceEnabled() &&
		   parcel && parcel->getParcelFlagAllowVoice();
}

bool LLViewerParcelMgr::allowAgentFly(const LLViewerRegion* region,
									  const LLParcel* parcel) const
{
	return region && !region->getBlockFly() && parcel && parcel->getAllowFly();
}

// Can the agent be pushed around by LLPushObject?
bool LLViewerParcelMgr::allowAgentPush(const LLViewerRegion* region,
									   const LLParcel* parcel) const
{
	return region && !region->getRestrictPushObject() &&
		   parcel && !parcel->getRestrictPushObject();
}

bool LLViewerParcelMgr::allowAgentScripts(const LLViewerRegion* region,
										  const LLParcel* parcel) const
{
	// *NOTE: This code does not take into account group-owned parcels
	// and the flag to allow group-owned scripted objects to run.
	// This mirrors the traditional menu bar parcel icon code, but is not
	// technically correct.
	return region && parcel && parcel->getAllowOtherScripts() &&
		   !region->getRegionFlag(REGION_FLAGS_SKIP_SCRIPTS) &&
		   !region->getRegionFlag(REGION_FLAGS_ESTATE_SKIP_SCRIPTS);
}

bool LLViewerParcelMgr::allowAgentDamage(const LLViewerRegion* region,
										 const LLParcel* parcel) const
{
	return (region && region->getAllowDamage()) ||
		   (parcel && parcel->getAllowDamage());
}

bool LLViewerParcelMgr::isOwnedAt(const LLVector3d& pos_global) const
{
	LLViewerRegion* region = gWorld.getRegionFromPosGlobal(pos_global);
	if (!region) return false;

	LLViewerParcelOverlay* overlay = region->getParcelOverlay();
	if (!overlay) return false;

	LLVector3 pos_region = region->getPosRegionFromGlobal(pos_global);

	return overlay->isOwned(pos_region);
}

bool LLViewerParcelMgr::isOwnedSelfAt(const LLVector3d& pos_global) const
{
	LLViewerRegion* region = gWorld.getRegionFromPosGlobal(pos_global);
	if (!region) return false;

	LLViewerParcelOverlay* overlay = region->getParcelOverlay();
	if (!overlay) return false;

	LLVector3 pos_region = region->getPosRegionFromGlobal(pos_global);

	return overlay->isOwnedSelf(pos_region);
}

bool LLViewerParcelMgr::isOwnedOtherAt(const LLVector3d& pos_global) const
{
	LLViewerRegion* region = gWorld.getRegionFromPosGlobal(pos_global);
	if (!region) return false;

	LLViewerParcelOverlay* overlay = region->getParcelOverlay();
	if (!overlay) return false;

	LLVector3 pos_region = region->getPosRegionFromGlobal(pos_global);

	return overlay->isOwnedOther(pos_region);
}

bool LLViewerParcelMgr::isSoundLocal(const LLVector3d& pos_global) const
{
	LLViewerRegion* region = gWorld.getRegionFromPosGlobal(pos_global);
	if (!region) return false;

	LLViewerParcelOverlay* overlay = region->getParcelOverlay();
	if (!overlay) return false;

	LLVector3 pos_region = region->getPosRegionFromGlobal(pos_global);

	return overlay->isSoundLocal(pos_region);
}

bool LLViewerParcelMgr::canHearSound(const LLVector3d& pos_global) const
{
	if (!inAgentParcel(pos_global))
	{
		static LLCachedControl<bool> neighbor_sims_sounds(gSavedSettings,
														  "NeighborSimsSounds");
		if (!neighbor_sims_sounds &&
			gWorld.getRegionFromPosGlobal(pos_global) != gAgent.getRegion())
		{
			return false;
		}
		if (gViewerParcelMgr.getAgentParcel()->getSoundLocal())
		{
			// Not in same parcel, and agent parcel only has local sound
			return false;
		}
		if (gViewerParcelMgr.isSoundLocal(pos_global))
		{
			// Not in same parcel, and target parcel only has local sound
			return false;
		}
	}

	return true;
}

bool LLViewerParcelMgr::inAgentParcel(const LLVector3d& pos_global) const
{
	LLViewerRegion* region = gWorld.getRegionFromPosGlobal(pos_global);
	if (!gAgent.getRegion() || region != gAgent.getRegion())
	{
		// Cannot be in the agent parcel if you are not in the same region.
		return false;
	}

	LLVector3 pos_region = gAgent.getRegion()->getPosRegionFromGlobal(pos_global);
	S32 row = S32(pos_region.mV[VY] / PARCEL_GRID_STEP_METERS);
	S32 column = S32(pos_region.mV[VX] / PARCEL_GRID_STEP_METERS);

	return mAgentParcelOverlay[row * mParcelsPerEdge + column];
}

//
// UTILITIES
//

void LLViewerParcelMgr::render()
{
	if (mSelected && mRenderSelection && !gDisconnected)
	{
		// Rendering is done in agent-coordinates, so need to supply
		// an appropriate offset to the render code.
		LLViewerRegion* regionp = gWorld.getRegionFromPosGlobal(mWestSouth);
		if (regionp)
		{
			renderHighlightSegments(mHighlightSegments, regionp);
		}
	}
}

void LLViewerParcelMgr::renderParcelCollision()
{
	// Check for expiration
	if (mCollisionTimer.getElapsedTimeF32() > PARCEL_COLLISION_DRAW_SECS)
	{
		mRenderCollision = false;
	}

	static LLCachedControl<bool> show_lines(gSavedSettings, "ShowBanLines");
	// The default behaviour is rather annoying when riding a vehicle, since as
	// soon as your *instantaneous* heading is not any more driving you to the
	// banned parcel, the ban walls disappear (and often reappear when you are
	// too close when you change your heading and are driving fast), making it
	// hard to avoid the said wall when driving at high speed and/or with a
	// high inertia. I therefore added this life saviour setting... HB
	static LLCachedControl<bool> render_always(gSavedSettings,
											   "RenderBanWallAlways");
	if (show_lines && (mRenderCollision || render_always))
	{
		LLViewerRegion* regionp = gAgent.getRegion();
		if (regionp)
		{
			bool use_pass = mCollisionParcel->getParcelFlag(PF_USE_PASS_LIST);
			renderCollisionSegments(mCollisionSegments, use_pass, regionp);
		}
	}
}

void LLViewerParcelMgr::sendParcelAccessListRequest(U32 flags)
{
	if (!mSelected)
	{
		return;
	}

	LLViewerRegion* region = gWorld.getRegionFromPosGlobal(mWestSouth);
	if (!region)
	{
		return;
	}

	LLMessageSystem* msg = gMessageSystemp;

	if (flags & AL_BAN)
	{
		mCurrentParcel->mBanList.clear();
	}
	if (flags & AL_ACCESS)
	{
		mCurrentParcel->mAccessList.clear();
	}
	if (flags & AL_ALLOW_EXPERIENCE)
	{
		mCurrentParcel->clearExperienceKeysByType(EXPERIENCE_KEY_TYPE_ALLOWED);
	}
	if (flags & AL_BLOCK_EXPERIENCE)
	{
		mCurrentParcel->clearExperienceKeysByType(EXPERIENCE_KEY_TYPE_BLOCKED);
	}

	// Only the headers differ
	msg->newMessageFast(_PREHASH_ParcelAccessListRequest);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlockFast(_PREHASH_Data);
	msg->addS32Fast(_PREHASH_SequenceID, 0);
	msg->addU32Fast(_PREHASH_Flags, flags);
	msg->addS32(_PREHASH_LocalID, mCurrentParcel->getLocalID());
	msg->sendReliable(region->getHost());
}

void LLViewerParcelMgr::sendParcelDwellRequest()
{
	if (!mSelected)
	{
		return;
	}

	LLViewerRegion* region = gWorld.getRegionFromPosGlobal(mWestSouth);
	if (!region)
	{
		return;
	}

	LLMessageSystem* msg = gMessageSystemp;

	// Only the headers differ
	msg->newMessage(_PREHASH_ParcelDwellRequest);
	msg->nextBlock(_PREHASH_AgentData);
	msg->addUUID(_PREHASH_AgentID, gAgentID);
	msg->addUUID(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlock(_PREHASH_Data);
	msg->addS32(_PREHASH_LocalID, mCurrentParcel->getLocalID());
	msg->addUUID(_PREHASH_ParcelID, LLUUID::null);	// Filled in on simulator
	msg->sendReliable(region->getHost());
}

bool callback_god_force_owner(const LLSD& notification, const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		LLMessageSystem* msg = gMessageSystemp;
		msg->newMessage(_PREHASH_ParcelGodForceOwner);
		msg->nextBlock(_PREHASH_AgentData);
		msg->addUUID(_PREHASH_AgentID, gAgentID);
		msg->addUUID(_PREHASH_SessionID, gAgentSessionID);
		msg->nextBlock(_PREHASH_Data);
		msg->addUUID(_PREHASH_OwnerID,
					 notification["payload"]["owner_id"].asUUID());
		msg->addS32(_PREHASH_LocalID,
					notification["payload"]["parcel_local_id"].asInteger());
		msg->sendReliable(LLHost(notification["payload"]["region_host"].asString()));
	}

	return false;
}

void LLViewerParcelMgr::sendParcelGodForceOwner(const LLUUID& owner_id)
{
	if (!mSelected)
	{
		gNotifications.add("CannotSetLandOwnerNothingSelected");
		return;
	}

	llinfos << "Claiming " << mWestSouth << " to " << mEastNorth << llendl;

	// BUG: Only works for the region containing mWestSouthBottom
	LLVector3d east_north_region_check(mEastNorth);
	east_north_region_check.mdV[VX] -= 0.5;
	east_north_region_check.mdV[VY] -= 0.5;

	LLViewerRegion* region = gWorld.getRegionFromPosGlobal(mWestSouth);
	if (!region)
	{
		// *TODO: Add a force owner version of this alert.
		gNotifications.add("CannotContentifyNoRegion");
		return;
	}

	// *FIXME: make it work for cross-region selections
	LLViewerRegion* region2 =
		gWorld.getRegionFromPosGlobal(east_north_region_check);
	if (region != region2)
	{
		gNotifications.add("CannotSetLandOwnerMultipleRegions");
		return;
	}

	llinfos << "Region " << region->getOriginGlobal() << llendl;

	LLSD payload;
	payload["owner_id"] = owner_id;
	payload["parcel_local_id"] = mCurrentParcel->getLocalID();
	payload["region_host"] = region->getHost().getIPandPort();
	LLNotification::Params params("ForceOwnerAuctionWarning");
	params.payload(payload).functor(callback_god_force_owner);

	if (mCurrentParcel->getAuctionID())
	{
		gNotifications.add(params);
	}
	else
	{
		gNotifications.forceResponse(params, 0);
	}
}

void LLViewerParcelMgr::sendParcelGodForceToContent()
{
	if (!mSelected)
	{
		gNotifications.add("CannotContentifyNothingSelected");
		return;
	}
	LLViewerRegion* region = gWorld.getRegionFromPosGlobal(mWestSouth);
	if (!region)
	{
		gNotifications.add("CannotContentifyNoRegion");
		return;
	}

	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessage(_PREHASH_ParcelGodMarkAsContent);
	msg->nextBlock(_PREHASH_AgentData);
	msg->addUUID(_PREHASH_AgentID, gAgentID);
	msg->addUUID(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlock(_PREHASH_ParcelData);
	msg->addS32(_PREHASH_LocalID, mCurrentParcel->getLocalID());
	msg->sendReliable(region->getHost());
}

void LLViewerParcelMgr::sendParcelRelease()
{
	if (!mSelected)
	{
        gNotifications.add("CannotReleaseLandNothingSelected");
		return;
	}

	LLViewerRegion* region = gWorld.getRegionFromPosGlobal(mWestSouth);
	if (!region)
	{
		gNotifications.add("CannotReleaseLandNoRegion");
		return;
	}

	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessage(_PREHASH_ParcelRelease);
	msg->nextBlock(_PREHASH_AgentData);
	msg->addUUID(_PREHASH_AgentID, gAgentID);
	msg->addUUID(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlock(_PREHASH_Data);
	msg->addS32(_PREHASH_LocalID, mCurrentParcel->getLocalID());
#if 0
	msg->addU32(_PREHASH_Flags,
				gAgent.isGodlikeWithoutAdminMenuFakery ? PR_GOD_FORCE
													   : PR_NONE);
#endif
	msg->sendReliable(region->getHost());

	// Blitz selection, since the parcel might be non-rectangular, and we would
	// not have appropriate parcel information.
	deselectLand();
}

struct LLViewerParcelMgr::ParcelBuyInfo
{
	LLUUID	mAgent;
	LLUUID	mSession;
	LLUUID	mGroup;
	LLHost	mHost;

	// For parcel buys
	S32		mParcelID;
	S32		mPrice;
	S32		mArea;

	// For land claims
	F32		mWest;
	F32		mSouth;
	F32		mEast;
	F32		mNorth;

	bool	mIsGroupOwned;
	bool	mRemoveContribution;
	bool	mIsClaim;
};

LLViewerParcelMgr::ParcelBuyInfo* LLViewerParcelMgr::setupParcelBuy(const LLUUID& agent_id,
																	const LLUUID& session_id,
																	const LLUUID& group_id,
																	bool is_group_owned,
																	bool is_claim,
																	bool remove_contribution)
{
	if (!mSelected || !mCurrentParcel)
	{
		gNotifications.add("CannotBuyLandNothingSelected");
		return NULL;
	}

	LLViewerRegion* region = gWorld.getRegionFromPosGlobal(mWestSouth);
	if (!region)
	{
		gNotifications.add("CannotBuyLandNoRegion");
		return NULL;
	}

	if (is_claim)
	{
		llinfos << "Claiming " << mWestSouth << " to " << mEastNorth << llendl;
		llinfos << "Region " << region->getOriginGlobal() << llendl;

		// BUG: Only works for the region containing mWestSouthBottom
		LLVector3d east_north_region_check(mEastNorth);
		east_north_region_check.mdV[VX] -= 0.5;
		east_north_region_check.mdV[VY] -= 0.5;

		if (region != gWorld.getRegionFromPosGlobal(east_north_region_check))
		{
			gNotifications.add("CantBuyLandAcrossMultipleRegions");
			return NULL;
		}
	}

	ParcelBuyInfo* info = new ParcelBuyInfo;

	info->mAgent = agent_id;
	info->mSession = session_id;
	info->mGroup = group_id;
	info->mIsGroupOwned = is_group_owned;
	info->mIsClaim = is_claim;
	info->mRemoveContribution = remove_contribution;
	info->mHost = region->getHost();
	info->mPrice = mCurrentParcel->getSalePrice();
	info->mArea = mCurrentParcel->getArea();

	if (!is_claim)
	{
		info->mParcelID = mCurrentParcel->getLocalID();
	}
	else
	{
		// *FIXME: make it work for cross-region selections
		LLVector3 west_south_bottom_region = region->getPosRegionFromGlobal(mWestSouth);
		LLVector3 east_north_top_region = region->getPosRegionFromGlobal(mEastNorth);

		info->mWest		= west_south_bottom_region.mV[VX];
		info->mSouth	= west_south_bottom_region.mV[VY];
		info->mEast		= east_north_top_region.mV[VX];
		info->mNorth	= east_north_top_region.mV[VY];
	}

	return info;
}

void LLViewerParcelMgr::sendParcelBuy(ParcelBuyInfo* info)
{
	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessage(info->mIsClaim ? _PREHASH_ParcelClaim
								   : _PREHASH_ParcelBuy);
	msg->nextBlock(_PREHASH_AgentData);
	msg->addUUID(_PREHASH_AgentID, info->mAgent);
	msg->addUUID(_PREHASH_SessionID, info->mSession);
	msg->nextBlock(_PREHASH_Data);
	msg->addUUID(_PREHASH_GroupID, info->mGroup);
	msg->addBool(_PREHASH_IsGroupOwned, info->mIsGroupOwned);
	if (!info->mIsClaim)
	{
		msg->addBool(_PREHASH_RemoveContribution, info->mRemoveContribution);
		msg->addS32(_PREHASH_LocalID, info->mParcelID);
	}
	msg->addBool(_PREHASH_Final, true);	// Do not allow escrow buys
	if (info->mIsClaim)
	{
		msg->nextBlock(_PREHASH_ParcelData);
		msg->addF32(_PREHASH_West,  info->mWest);
		msg->addF32(_PREHASH_South, info->mSouth);
		msg->addF32(_PREHASH_East,  info->mEast);
		msg->addF32(_PREHASH_North, info->mNorth);
	}
	else // ParcelBuy
	{
		msg->nextBlock(_PREHASH_ParcelData);
		msg->addS32(_PREHASH_Price, info->mPrice);
		msg->addS32(_PREHASH_Area, info->mArea);
	}
	msg->sendReliable(info->mHost);
}

void LLViewerParcelMgr::deleteParcelBuy(ParcelBuyInfo*& info)
{
	delete info;
	info = NULL;
}

void LLViewerParcelMgr::sendParcelDeed(const LLUUID& group_id)
{
	if (!mSelected || !mCurrentParcel)
	{
		gNotifications.add("CannotDeedLandNothingSelected");
		return;
	}
	if (group_id.isNull())
	{
		gNotifications.add("CannotDeedLandNoGroup");
		return;
	}
	LLViewerRegion* region = gWorld.getRegionFromPosGlobal(mWestSouth);
	if (!region)
	{
		gNotifications.add("CannotDeedLandNoRegion");
		return;
	}

	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessage(_PREHASH_ParcelDeedToGroup);
	msg->nextBlock(_PREHASH_AgentData);
	msg->addUUID(_PREHASH_AgentID, gAgentID);
	msg->addUUID(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlock(_PREHASH_Data);
	msg->addUUID(_PREHASH_GroupID, group_id);
	msg->addS32(_PREHASH_LocalID, mCurrentParcel->getLocalID());
	//msg->addU32("JoinNeighbors", join);
	msg->sendReliable(region->getHost());
}

#if 0	// *NOTE: We cannot easily make landmarks at global positions because
		// global positions probably refer to a sim/local combination which can
		// move over time. We could implement this by looking up the region
		// global x,y, but it is easier to take it out for now.
void LLViewerParcelMgr::makeLandmarkAtSelection()
{
	// Do not create for parcels you do not own
	if (gAgentID != mCurrentParcel->getOwnerID())
	{
		return;
	}

	LLVector3d global_center(mWestSouth);
	global_center += mEastNorth;
	global_center *= 0.5f;

	LLViewerRegion* region;
	region = gWorld.getRegionFromPosGlobal(global_center);

	LLVector3 west_south_bottom_region =
		region->getPosRegionFromGlobal(mWestSouth);
	LLVector3 east_north_top_region =
		region->getPosRegionFromGlobal(mEastNorth);

	std::string buffer;
	S32 pos_x = (S32)floor((west_south_bottom_region.mV[VX] +
							east_north_top_region.mV[VX]) * 0.5f);
	S32 pos_y = (S32)floor((west_south_bottom_region.mV[VY] +
							east_north_top_region.mV[VY]) * 0.5f);
	buffer = llformat("%s in %s (%d, %d)", "My land",
					  region->getName().c_str(), pos_x, pos_y);
	name.assign(buffer);

	create_landmark("My land", "Claimed land", global_center);
}
#endif

S32 LLViewerParcelMgr::getAgentParcelId() const
{
	return mAgentParcel ? mAgentParcel->getLocalID() : INVALID_PARCEL_ID;
}

void LLViewerParcelMgr::sendParcelPropertiesUpdate(LLParcel* parcel,
												   bool use_agent_region)
{
	if (!parcel) return;

	LLViewerRegion* region;
	region = use_agent_region ? gAgent.getRegion()
							  : gWorld.getRegionFromPosGlobal(mWestSouth);
	if (!region) return;

	LLSD body;
	const std::string& url = region->getCapability("ParcelPropertiesUpdate");
	if (url.empty())
	{
		LLMessageSystem* msg = gMessageSystemp;
		msg->newMessageFast(_PREHASH_ParcelPropertiesUpdate);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID,	gAgentID);
		msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
		msg->nextBlockFast(_PREHASH_ParcelData);
		msg->addS32Fast(_PREHASH_LocalID, parcel->getLocalID());

		U32 message_flags = 0x01;
		msg->addU32(_PREHASH_Flags, message_flags);

		parcel->packMessage(msg);

		msg->sendReliable(region->getHost());
	}
	else
	{
		// Request new properties update from simulator
		U32 message_flags = 0x01;
		body["flags"] = ll_sd_from_U32(message_flags);
		parcel->packMessage(body);
		llinfos << "Sending parcel properties update via capability to: "
				<< url << llendl;
		LLCoreHttpUtil::HttpCoroutineAdapter::messageHttpPost(url, body,
															  "Parcel Properties sent to sim.",
															  "Parcel Properties failed to send to sim.");
	}

	// If this is the agent parcel, tell to the status bar that its icons need
	// a refresh (this will cause a call to requestParcelProperties() by the
	// status bar 2 seconds or so later, to let some time for the above
	// properties update call to complete first).
	if (gStatusBarp && parcel->getLocalID() == mAgentParcel->getLocalID())
	{
		gStatusBarp->setDirtyAgentParcelProperties();
	}
}

void LLViewerParcelMgr::setHoverParcel(const LLVector3d& pos)
{
	LLViewerRegion* regionp = gWorld.getRegionFromPosGlobal(pos);
	if (!regionp)
	{
		return;
	}

	// Only request parcel info if position has changed outside of the last
	// parcel grid step.
	static S32 last_west, last_south;
	constexpr F64 meters_per_step = 1.0 / PARCEL_GRID_STEP_METERS;
	S32 west_parcel_step = pos.mdV[VX] * meters_per_step;
	S32 south_parcel_step = pos.mdV[VY] * meters_per_step;
	if (west_parcel_step == last_west && south_parcel_step == last_south)
	{
		return;
	}

	static LLUUID last_region;
	const LLUUID& region_id = regionp->getRegionID();
	LLVector3 local_pos = regionp->getPosRegionFromGlobal(pos);
	LLViewerParcelOverlay* overlayp = regionp->getParcelOverlay();

	// Check to see if the new position is in same parcel. This check is not
	// ideal, since it checks by way of straight lines. So sometimes (small
	// parcel in the middle of large one) it can decide that parcel actually
	// changed, but it still allows to reduce amount of requests significantly.
	bool do_request = !overlayp || region_id != last_region;
	if (!do_request)
	{
		S32 west_parcel = local_pos.mV[VX] / PARCEL_GRID_STEP_METERS;
		S32 south_parcel = local_pos.mV[VY] / PARCEL_GRID_STEP_METERS;
		while (!do_request && west_parcel_step < last_west)
		{
			S32 shift = last_west-- - west_parcel_step;
			do_request = PARCEL_WEST_LINE &
						 overlayp->parcelLineFlags(south_parcel,
												   west_parcel + shift);
		}
		while (!do_request && south_parcel_step < last_south)
		{
			S32 shift = last_south-- - south_parcel_step;
			do_request = PARCEL_SOUTH_LINE &
						 overlayp->parcelLineFlags(south_parcel + shift,
												   west_parcel);
		}
		while (!do_request && west_parcel_step > last_west)
		{
			S32 shift = west_parcel_step - last_west++;
			do_request = PARCEL_WEST_LINE &
						 overlayp->parcelLineFlags(south_parcel,
												   west_parcel - shift + 1);
		}
		while (!do_request && south_parcel_step > last_south)
		{
			S32 shift = south_parcel_step - last_south++;
			do_request = PARCEL_SOUTH_LINE &
						 overlayp->parcelLineFlags(south_parcel - shift + 1,
												   west_parcel);
		}
	}
	if (!do_request)
	{
		return;
	}

	// Remember the last requested parcel position
	last_region = region_id;
	last_west = west_parcel_step;
	last_south = south_parcel_step;
	mHoverRequestResult = PARCEL_RESULT_NO_DATA;

	// Send a rectangle around the point. This means the parcel sent back is at
	// least a rectangle around the point, which is more efficient for public
	// land. Fewer requests are sent. JC
	F32 west = PARCEL_GRID_STEP_METERS *
			   floor(local_pos.mV[VX] / PARCEL_GRID_STEP_METERS);
	F32 south = PARCEL_GRID_STEP_METERS *
				floor(local_pos.mV[VY] / PARCEL_GRID_STEP_METERS);
	// Send request message
	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessageFast(_PREHASH_ParcelPropertiesRequest);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlockFast(_PREHASH_ParcelData);
	msg->addS32Fast(_PREHASH_SequenceID, HOVERED_PARCEL_SEQ_ID);
	msg->addF32Fast(_PREHASH_West, west);
	msg->addF32Fast(_PREHASH_South, south);
	msg->addF32Fast(_PREHASH_East, west + PARCEL_GRID_STEP_METERS);
	msg->addF32Fast(_PREHASH_North, south + PARCEL_GRID_STEP_METERS);
	msg->addBool(_PREHASH_SnapSelection, false);
	msg->sendReliable(regionp->getHost());
}

bool LLViewerParcelMgr::requestParcelProperties(const LLVector3d& pos, S32 id)
{
	LLViewerRegion* region = gWorld.getRegionFromPosGlobal(pos);
	if (!region)
	{
		return false;
	}

	// Send a rectangle around the point.
	// This means the parcel sent back is at least a rectangle around the point,
	// which is more efficient for public land. Fewer requests are sent. JC
	LLVector3 wsb_region = region->getPosRegionFromGlobal(pos);

	F32 west = PARCEL_GRID_STEP_METERS * floor(wsb_region.mV[VX] /
			   PARCEL_GRID_STEP_METERS);
	F32 south = PARCEL_GRID_STEP_METERS * floor(wsb_region.mV[VY] /
				PARCEL_GRID_STEP_METERS);

	F32 east = west + PARCEL_GRID_STEP_METERS;
	F32 north = south + PARCEL_GRID_STEP_METERS;

	// Send request message
	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessageFast(_PREHASH_ParcelPropertiesRequest);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlockFast(_PREHASH_ParcelData);
	msg->addS32Fast(_PREHASH_SequenceID, id);
	msg->addF32Fast(_PREHASH_West, west);
	msg->addF32Fast(_PREHASH_South, south);
	msg->addF32Fast(_PREHASH_East, east);
	msg->addF32Fast(_PREHASH_North, north);
	msg->addBool(_PREHASH_SnapSelection, false);
	msg->sendReliable(region->getHost());

	return true;
}

//static
void LLViewerParcelMgr::processParcelOverlay(LLMessageSystem* msg, void**)
{
	// Extract the packed overlay information
	S32 packed_overlay_size = msg->getSizeFast(_PREHASH_ParcelData,
											   _PREHASH_Data);
	if (packed_overlay_size <= 0)
	{
		llwarns << "Overlay size " << packed_overlay_size << llendl;
		return;
	}

	// Variable region size support
#if 0
	S32 parcels_per_edge = gViewerParcelMgr.mParcelsPerEdge;
	S32 expected_size = parcels_per_edge * parcels_per_edge / PARCEL_OVERLAY_CHUNKS;
#else
	S32 expected_size = 1024;
#endif
	if (packed_overlay_size != expected_size)
	{
		llwarns << "Got parcel overlay size " << packed_overlay_size
				<< " expecting " << expected_size << llendl;
		return;
	}

	S32 sequence_id;
	msg->getS32Fast(_PREHASH_ParcelData, _PREHASH_SequenceID, sequence_id);
	msg->getBinaryDataFast(_PREHASH_ParcelData, _PREHASH_Data, sPackedOverlay,
						   expected_size);

	LLHost host = msg->getSender();
	LLViewerRegion* region = gWorld.getRegion(host);
	if (region)
	{
		region->mParcelOverlay->uncompressLandOverlay(sequence_id,
													  sPackedOverlay);
	}
}

//static
void LLViewerParcelMgr::processParcelProperties(LLMessageSystem* msg, void**)
{
	S32 self_count = 0;
	S32 other_count = 0;
	S32 public_count = 0;
	S32 local_id;
	LLUUID owner_id;
	U32 auction_id = 0;
	S32 claim_price_per_meter = 0;
	S32 rent_price_per_meter = 0;
	S32 claim_date = 0;
	LLVector3 aabb_min;
	LLVector3 aabb_max;
	S32 area = 0;
	S32 sw_max_prims = 0;
	S32 sw_total_prims = 0;
	U8 status = 0;
	S32 max_prims = 0;
	S32 total_prims = 0;
	S32 owner_prims = 0;
	S32 group_prims = 0;
	S32 other_prims = 0;
	S32 selected_prims = 0;
	S32 other_clean_time = 0;
	F32 parcel_prim_bonus = 1.f;
	bool is_group_owned = false;
	bool region_push_override = false;
	bool region_deny_anonymous_override = false;
	bool region_deny_identified_override = false; // Deprecated
	bool region_deny_transacted_override = false; // Deprecated
	bool region_deny_age_unverified_override = false;
	bool region_allow_access_override = true;
	bool agent_parcel_update = false;
	bool region_allow_env_override = true;
	S32 parcel_env_version = 0;

	// Variable region size support
	LLViewerRegion* msg_region = gWorld.getRegion(msg->getSender());
	if (msg_region)
	{
		gViewerParcelMgr.mParcelsPerEdge = (S32)(msg_region->getWidth() /
												 PARCEL_GRID_STEP_METERS);
	}
	else
	{
		gViewerParcelMgr.mParcelsPerEdge = (S32)(gAgent.getRegion()->getWidth() /
												 PARCEL_GRID_STEP_METERS);
	}

	S32 request_result;
	msg->getS32Fast(_PREHASH_ParcelData, _PREHASH_RequestResult,
					request_result);
	if (request_result == PARCEL_RESULT_NO_DATA)
	{
		// No valid parcel data
		llinfos << "No valid parcel data" << llendl;
		return;
	}

	// Decide where the data will go.
	LLParcel* parcel = NULL;
	S32 sequence_id;
	msg->getS32Fast(_PREHASH_ParcelData, _PREHASH_SequenceID, sequence_id);
	if (sequence_id == SELECTED_PARCEL_SEQ_ID)
	{
		// ...selected parcels report this sequence id
		gViewerParcelMgr.mRequestResult = PARCEL_RESULT_SUCCESS;
		parcel = gViewerParcelMgr.mCurrentParcel;
	}
	else if (sequence_id == HOVERED_PARCEL_SEQ_ID)
	{
		gViewerParcelMgr.mHoverRequestResult = PARCEL_RESULT_SUCCESS;
		parcel = gViewerParcelMgr.mHoverParcel;
	}
	else if (sequence_id == COLLISION_NOT_IN_GROUP_PARCEL_SEQ_ID ||
			 sequence_id == COLLISION_NOT_ON_LIST_PARCEL_SEQ_ID ||
			 sequence_id == COLLISION_BANNED_PARCEL_SEQ_ID)
	{
		gViewerParcelMgr.mHoverRequestResult = PARCEL_RESULT_SUCCESS;
		parcel = gViewerParcelMgr.mCollisionParcel;
	}
	else if (sequence_id == UPDATE_AGENT_PARCEL_SEQ_ID || sequence_id == 0 ||
			 sequence_id > gViewerParcelMgr.mAgentParcelSequenceID)
	{
		if (sequence_id != UPDATE_AGENT_PARCEL_SEQ_ID)
		{
			gViewerParcelMgr.mAgentParcelSequenceID = sequence_id;
		}
		parcel = gViewerParcelMgr.mAgentParcel;
	}
	else
	{
		llinfos << "Out of order agent parcel sequence id " << sequence_id
				<< " last good " << gViewerParcelMgr.mAgentParcelSequenceID
				<< llendl;
		return;
	}

	LL_DEBUGS("ParcelMgr") << "Sequence id = ";
	switch (sequence_id)
	{
		case UPDATE_AGENT_PARCEL_SEQ_ID:
			LL_CONT << "UPDATE_AGENT_PARCEL_SEQ_ID";
			break;

		case SELECTED_PARCEL_SEQ_ID:
			LL_CONT << "SELECTED_PARCEL_SEQ_ID";
			break;

		case COLLISION_NOT_IN_GROUP_PARCEL_SEQ_ID:
			LL_CONT << "COLLISION_NOT_IN_GROUP_PARCEL_SEQ_ID";
			break;

		case COLLISION_BANNED_PARCEL_SEQ_ID:
			LL_CONT << "COLLISION_BANNED_PARCEL_SEQ_ID";
			break;

		case COLLISION_NOT_ON_LIST_PARCEL_SEQ_ID:
			LL_CONT << "COLLISION_NOT_ON_LIST_PARCEL_SEQ_ID";
			break;

		case HOVERED_PARCEL_SEQ_ID:
			LL_CONT << "HOVERED_PARCEL_SEQ_ID";
			break;

		default:
			LL_CONT << sequence_id;
	}
	LL_CONT << LL_ENDL;

	bool snap_selection = false;
	msg->getBool(_PREHASH_ParcelData, _PREHASH_SnapSelection, snap_selection);
	msg->getS32Fast(_PREHASH_ParcelData, _PREHASH_SelfCount, self_count);
	msg->getS32Fast(_PREHASH_ParcelData, _PREHASH_OtherCount, other_count);
	msg->getS32Fast(_PREHASH_ParcelData, _PREHASH_PublicCount, public_count);
	msg->getS32Fast(_PREHASH_ParcelData, _PREHASH_LocalID, local_id);
	msg->getUUIDFast(_PREHASH_ParcelData, _PREHASH_OwnerID, owner_id);
	msg->getBoolFast(_PREHASH_ParcelData, _PREHASH_IsGroupOwned,
					 is_group_owned);
	msg->getU32Fast(_PREHASH_ParcelData, _PREHASH_AuctionID, auction_id);
	msg->getS32Fast(_PREHASH_ParcelData, _PREHASH_ClaimDate, claim_date);
	msg->getS32Fast(_PREHASH_ParcelData, _PREHASH_ClaimPrice,
					claim_price_per_meter);
	msg->getS32Fast(_PREHASH_ParcelData, _PREHASH_RentPrice,
					rent_price_per_meter);
	msg->getVector3Fast(_PREHASH_ParcelData, _PREHASH_AABBMin, aabb_min);
	msg->getVector3Fast(_PREHASH_ParcelData, _PREHASH_AABBMax, aabb_max);
	msg->getS32Fast(_PREHASH_ParcelData, _PREHASH_Area, area);
#if 0
	LLUUID buyer_id;
	msg->getUUIDFast(_PREHASH_ParcelData, _PREHASH_BuyerID, buyer_id);
#endif
	msg->getU8(_PREHASH_ParcelData, _PREHASH_Status, status);
	msg->getS32(_PREHASH_ParcelData, _PREHASH_SimWideMaxPrims, sw_max_prims);
	msg->getS32(_PREHASH_ParcelData, _PREHASH_SimWideTotalPrims,
				sw_total_prims);
	msg->getS32Fast(_PREHASH_ParcelData, _PREHASH_MaxPrims, max_prims);
	msg->getS32Fast(_PREHASH_ParcelData, _PREHASH_TotalPrims, total_prims);
	msg->getS32Fast(_PREHASH_ParcelData, _PREHASH_OwnerPrims, owner_prims);
	msg->getS32Fast(_PREHASH_ParcelData, _PREHASH_GroupPrims, group_prims);
	msg->getS32Fast(_PREHASH_ParcelData, _PREHASH_OtherPrims, other_prims);
	msg->getS32Fast(_PREHASH_ParcelData, _PREHASH_SelectedPrims,
					selected_prims);
	msg->getF32Fast(_PREHASH_ParcelData, _PREHASH_ParcelPrimBonus,
					parcel_prim_bonus);
	msg->getBoolFast(_PREHASH_ParcelData, _PREHASH_RegionPushOverride,
					 region_push_override);
	msg->getBoolFast(_PREHASH_ParcelData, _PREHASH_RegionDenyAnonymous,
					 region_deny_anonymous_override);
	msg->getBoolFast(_PREHASH_ParcelData, _PREHASH_RegionDenyIdentified,
					 region_deny_identified_override); // Deprecated
	msg->getBoolFast(_PREHASH_ParcelData, _PREHASH_RegionDenyTransacted,
					 region_deny_transacted_override); // Deprecated
	if (msg->getNumberOfBlocksFast(_PREHASH_AgeVerificationBlock))
	{
		// This block was added later and may not be on older sims, so we have
		// to test its existence first
		msg->getBoolFast(_PREHASH_AgeVerificationBlock,
						 _PREHASH_RegionDenyAgeUnverified,
						 region_deny_age_unverified_override);
	}
	if (msg->getNumberOfBlocks(_PREHASH_RegionAllowAccessBlock))
	{
		msg->getBoolFast(_PREHASH_RegionAllowAccessBlock,
						 _PREHASH_RegionAllowAccessOverride,
						 region_allow_access_override);
	}

	// Obscure MOAP
	U32 extended_flags = 0;
	if (msg->getNumberOfBlocks(_PREHASH_ParcelExtendedFlags))
	{
		msg->getU32Fast(_PREHASH_ParcelExtendedFlags, _PREHASH_Flags,
						extended_flags);
	}

	if (msg->getNumberOfBlocks(_PREHASH_ParcelEnvironmentBlock))
	{
		msg->getS32Fast(_PREHASH_ParcelEnvironmentBlock,
						_PREHASH_ParcelEnvironmentVersion,
						parcel_env_version);
		msg->getBoolFast(_PREHASH_ParcelEnvironmentBlock,
						 _PREHASH_RegionAllowEnvironmentOverride,
						 region_allow_env_override);
	}

	msg->getS32(_PREHASH_ParcelData, _PREHASH_OtherCleanTime,
				other_clean_time);

	// Actually extract the data.
	if (parcel)
	{
		if (local_id == gViewerParcelMgr.mAgentParcel->getLocalID())
		{
			// Parcels in different regions can have same Ids.
			LLViewerRegion* parcel_region = gWorld.getRegion(msg->getSender());
			LLViewerRegion* agent_region = gAgent.getRegion();
			agent_parcel_update = parcel_region && agent_region &&
								  parcel_region->getRegionID() ==
									agent_region->getRegionID();
		}

		S32 cur_env_version = parcel->getParcelEnvironmentVersion();

		parcel->init(owner_id, false, false, false, claim_date,
					 claim_price_per_meter, rent_price_per_meter, area,
					 other_prims, parcel_prim_bonus, is_group_owned);
		parcel->setLocalID(local_id);
		parcel->setAABBMin(aabb_min);
		parcel->setAABBMax(aabb_max);

		parcel->setAuctionID(auction_id);
		parcel->setOwnershipStatus((LLParcel::EOwnershipStatus)status);

		parcel->setSimWideMaxPrimCapacity(sw_max_prims);
		parcel->setSimWidePrimCount(sw_total_prims);
		parcel->setMaxPrimCapacity(max_prims);
		parcel->setOwnerPrimCount(owner_prims);
		parcel->setGroupPrimCount(group_prims);
		parcel->setOtherPrimCount(other_prims);
		parcel->setSelectedPrimCount(selected_prims);
		parcel->setParcelPrimBonus(parcel_prim_bonus);

		parcel->setCleanOtherTime(other_clean_time);
		parcel->setRegionPushOverride(region_push_override);
		parcel->setRegionDenyAnonymousOverride(region_deny_anonymous_override);
		parcel->setRegionDenyAgeUnverifiedOverride(region_deny_age_unverified_override);
		parcel->setRegionAllowAccessOverride(region_allow_access_override);
		parcel->setParcelEnvironmentVersion(cur_env_version);
		parcel->setRegionAllowEnvironmentOverride(region_allow_env_override);
		parcel->setObscureMOAP(bool(extended_flags));

		parcel->unpackMessage(msg);

		if (parcel == gViewerParcelMgr.mAgentParcel)
		{
			S32 bitmap_size = gViewerParcelMgr.mParcelsPerEdge *
							  gViewerParcelMgr.mParcelsPerEdge / 8;
			U8* bitmap = new U8[bitmap_size];
			msg->getBinaryDataFast(_PREHASH_ParcelData, _PREHASH_Bitmap,
								   bitmap, bitmap_size);
			gViewerParcelMgr.writeAgentParcelFromBitmap(bitmap);
			delete[] bitmap;

			if (sequence_id != UPDATE_AGENT_PARCEL_SEQ_ID)
			{
				// Let interesting parties know about agent parcel change.
				gViewerParcelMgr.mAgentParcelChangedSignal();
				if (gViewerParcelMgr.mTeleportInProgress)
				{
					gViewerParcelMgr.mTeleportInProgress = false;
					gViewerParcelMgr.mTeleportFinishedSignal(gAgent.getPositionGlobal(),
															 false);
				}
			}

			if (gStatusBarp)
			{
				gStatusBarp->setDirty();
			}
		}
		else if (agent_parcel_update)
		{
			// Updated agent parcel
			gViewerParcelMgr.mAgentParcel->unpackMessage(msg);
			parcel->setParcelEnvironmentVersion(parcel_env_version);
			if (cur_env_version != parcel_env_version &&
				gAgent.hasExtendedEnvironment())
			{
				LL_DEBUGS("Environment") << "Parcel environment version is "
										 << parcel->getParcelEnvironmentVersion()
										 << LL_ENDL;
				gEnvironment.requestParcel(local_id);
			}
		}
	}

	// Handle updating selections, if necessary.
	LLViewerRegion* region = gWorld.getRegion(msg->getSender());
	if (sequence_id == SELECTED_PARCEL_SEQ_ID)
	{
		// Update selected counts
		gViewerParcelMgr.mCurrentParcelSelection->mSelectedSelfCount =
			self_count;
		gViewerParcelMgr.mCurrentParcelSelection->mSelectedOtherCount =
			other_count;
		gViewerParcelMgr.mCurrentParcelSelection->mSelectedPublicCount =
			public_count;
		gViewerParcelMgr.mCurrentParcelSelection->mSelectedMultipleOwners =
			request_result == PARCEL_RESULT_MULTIPLE;

		// Select the whole parcel
		if (region)
		{
			if (!snap_selection)
			{
				// Do not muck with the westsouth and eastnorth, just highlight
				// it
				LLVector3 west_south =
					region->getPosRegionFromGlobal(gViewerParcelMgr.mWestSouth);
				LLVector3 east_north =
					region->getPosRegionFromGlobal(gViewerParcelMgr.mEastNorth);

				gViewerParcelMgr.resetSegments(gViewerParcelMgr.mHighlightSegments);
				gViewerParcelMgr.writeHighlightSegments(west_south.mV[VX],
														west_south.mV[VY],
														east_north.mV[VX],
														east_north.mV[VY]);
				gViewerParcelMgr.mCurrentParcelSelection->mWholeParcelSelected = false;
			}
			else if (local_id == 0)
			{
				// This is public land, just highlight the selection
				gViewerParcelMgr.mWestSouth =
					region->getPosGlobalFromRegion(aabb_min);
				gViewerParcelMgr.mEastNorth =
					region->getPosGlobalFromRegion(aabb_max);

				gViewerParcelMgr.resetSegments(gViewerParcelMgr.mHighlightSegments);
				gViewerParcelMgr.writeHighlightSegments(aabb_min.mV[VX],
														aabb_min.mV[VY],
														aabb_max.mV[VX],
														aabb_max.mV[VY]);
				gViewerParcelMgr.mCurrentParcelSelection->mWholeParcelSelected = true;
			}
			else
			{
				gViewerParcelMgr.mWestSouth =
					region->getPosGlobalFromRegion(aabb_min);
				gViewerParcelMgr.mEastNorth =
					region->getPosGlobalFromRegion(aabb_max);

				// Owned land, highlight the boundaries
				S32 bitmap_size = gViewerParcelMgr.mParcelsPerEdge *
								  gViewerParcelMgr.mParcelsPerEdge / 8;
				U8* bitmap = new U8[bitmap_size];
				msg->getBinaryDataFast(_PREHASH_ParcelData, _PREHASH_Bitmap,
									   bitmap, bitmap_size);

				gViewerParcelMgr.resetSegments(gViewerParcelMgr.mHighlightSegments);
				gViewerParcelMgr.writeSegmentsFromBitmap(bitmap,
														 gViewerParcelMgr.mHighlightSegments);

				delete[] bitmap;
				bitmap = NULL;

				gViewerParcelMgr.mCurrentParcelSelection->mWholeParcelSelected = true;
			}
		}
		else	// Just after login, we may receive a first reply for the agent
				// parcel with a null host (0.0.0.0:0) after opening the "About
				// land" floater...
		{
			LL_DEBUGS("ParcelMgr") << "Unknown region host: "
								   << msg->getSender() << LL_ENDL;
		}

		// Request access list information for this land
		U32 flags = AL_ACCESS | AL_BAN;
		if (gAgent.hasRegionCapability("RegionExperiences"))
		{
			// Only request these flags when experiences are supported
			flags |= AL_ALLOW_EXPERIENCE | AL_BLOCK_EXPERIENCE;
		}
		gViewerParcelMgr.sendParcelAccessListRequest(flags);

		// Request dwell for this land, if it's not public land.
		gViewerParcelMgr.mSelectedDwell = DWELL_NAN;
		if (local_id != 0)
		{
			gViewerParcelMgr.sendParcelDwellRequest();
		}

		gViewerParcelMgr.mSelected = true;
		gViewerParcelMgr.notifySelectionObservers();
	}
	else if (sequence_id == COLLISION_NOT_IN_GROUP_PARCEL_SEQ_ID ||
			 sequence_id == COLLISION_NOT_ON_LIST_PARCEL_SEQ_ID  ||
			 sequence_id == COLLISION_BANNED_PARCEL_SEQ_ID)
	{
		if (region != gAgent.getRegion())
		{
			llwarns << "Received a banned parcel collision message for a non-agent region. Ignoring."
					<< llendl;
			return;
		}
		// We are about to collide with this parcel
		gViewerParcelMgr.mRenderCollision = true;
		gViewerParcelMgr.mCollisionTimer.reset();

		// Differentiate this parcel if we are banned from it.
		if (sequence_id == COLLISION_BANNED_PARCEL_SEQ_ID)
		{
			gViewerParcelMgr.mCollisionBanned = BA_BANNED;
		}
		else if (sequence_id == COLLISION_NOT_IN_GROUP_PARCEL_SEQ_ID)
		{
			gViewerParcelMgr.mCollisionBanned = BA_NOT_IN_GROUP;
		}
		else
		{
			gViewerParcelMgr.mCollisionBanned = BA_NOT_ON_LIST;

		}

		S32 bitmap_size = gViewerParcelMgr.mParcelsPerEdge *
						  gViewerParcelMgr.mParcelsPerEdge / 8;
		U8* bitmap = new U8[bitmap_size];
		msg->getBinaryDataFast(_PREHASH_ParcelData, _PREHASH_Bitmap,
							   bitmap, bitmap_size);

		gViewerParcelMgr.resetSegments(gViewerParcelMgr.mCollisionSegments);
		gViewerParcelMgr.writeSegmentsFromBitmap(bitmap,
												 gViewerParcelMgr.mCollisionSegments);

		LLViewerParcelOverlay* overlay = region ? region->mParcelOverlay
												: NULL;
		if (overlay)
		{
#if 0		// *HACK: do not reset collision parcels when receiving local data
			// so that we can "build history" of collision parcels. This causes
			// false positives sometimes when the parcel data is changing.
			overlay->resetCollisionBitmap();
#endif
			overlay->readCollisionBitmap(bitmap);
		}
		delete[] bitmap;
		bitmap = NULL;
	}
	else if (sequence_id == HOVERED_PARCEL_SEQ_ID)
	{
		LLViewerRegion* region = gWorld.getRegion(msg->getSender());
		if (region)
		{
			gViewerParcelMgr.mHoverWestSouth = region->getPosGlobalFromRegion(aabb_min);
			gViewerParcelMgr.mHoverEastNorth = region->getPosGlobalFromRegion(aabb_max);
		}
		else
		{
			gViewerParcelMgr.mHoverWestSouth.clear();
			gViewerParcelMgr.mHoverEastNorth.clear();
		}
	}
	else if (sequence_id != UPDATE_AGENT_PARCEL_SEQ_ID)
	{
		// Look for music.
		if (gAudiop)
		{
			if (parcel)
			{
				std::string music_url_raw = parcel->getMusicURL();

				// Trim off whitespace from front and back
				std::string music_url = music_url_raw;
				LLStringUtil::trim(music_url);

				// On entering a new parcel, stop the last stream if the new
				// parcel has a different music url (an empty URL counts as
				// different).
				const std::string& stream_url = gAudiop->getInternetStreamURL();
				if (music_url.empty() || music_url != stream_url)
				{
					// URL is different from one currently playing.
					gAudiop->stopInternetStream();

					// If there is a new music URL and it is valid, play it.
					if (music_url.size() > 12 &&
						(music_url.substr(0, 7) == "http://" ||
						 music_url.substr(0, 8) == "https://"))
					{
						// Only play music when you enter a new parcel if the
						// control is in PLAY state. Changed as part of SL-4878
						if (LLViewerParcelMedia::parcelMusicPlaying() &&
							gSavedSettings.getBool("EnableStreamingMusic"))
						{
							LLViewerParcelMedia::playStreamingMusic(parcel);
						}
					}
					else if (!gAudiop->getInternetStreamURL().empty())
					{
						llinfos << "Stopping parcel music" << llendl;
						gAudiop->startInternetStream(LLStringUtil::null);
					}
				}
			}
			else
			{
				// Public land has no music
				gAudiop->stopInternetStream();
			}
		}

		// Now check for video
		LLViewerParcelMedia::update(parcel);
	}
}

//static
void LLViewerParcelMgr::processParcelAccessListReply(LLMessageSystem* msg,
													 void**)
{
	LLParcel* parcel = gViewerParcelMgr.mCurrentParcel;
	if (!parcel)
	{
		return;
	}

	LLUUID agent_id;
	msg->getUUIDFast(_PREHASH_Data, _PREHASH_AgentID, agent_id);
#if 0	// ignored
	S32 sequence_id = 0;
	msg->getS32Fast(_PREHASH_Data, _PREHASH_SequenceID, sequence_id);
#endif
	U32 message_flags = 0x0;
	msg->getU32Fast(_PREHASH_Data, _PREHASH_Flags, message_flags);
	S32 parcel_id = INVALID_PARCEL_ID;
	msg->getS32Fast(_PREHASH_Data, _PREHASH_LocalID, parcel_id);

	S32 local_id = parcel->getLocalID();
	if (parcel_id != local_id && local_id != INVALID_PARCEL_ID)
	{
		llwarns << "Parcel access list reply for parcel " << parcel_id
				<< " which isn't the selected parcel " << local_id
				<< ", ignoring..." << llendl;
		return;
	}

	if (message_flags & AL_ACCESS)
	{
		parcel->unpackAccessEntries(msg, &(parcel->mAccessList));
	}
	else if (message_flags & AL_BAN)
	{
		parcel->unpackAccessEntries(msg, &(parcel->mBanList));
	}
	else if (message_flags & AL_ALLOW_EXPERIENCE)
	{
		parcel->unpackExperienceEntries(msg, EXPERIENCE_KEY_TYPE_ALLOWED);
	}
	else if (message_flags & AL_BLOCK_EXPERIENCE)
	{
		parcel->unpackExperienceEntries(msg, EXPERIENCE_KEY_TYPE_BLOCKED);
	}
#if 0
	else if (message_flags & AL_RENTER)
	{
		parcel->unpackAccessEntries(msg, &(parcel->mRenterList));
	}
#endif

	gViewerParcelMgr.notifySelectionObservers();
}

//static
void LLViewerParcelMgr::processParcelDwellReply(LLMessageSystem* msg, void**)
{
	LLParcel* parcel = gViewerParcelMgr.mCurrentParcel;
	if (!parcel)
	{
		return;
	}

	LLUUID agent_id;
	msg->getUUID(_PREHASH_AgentData, _PREHASH_AgentID, agent_id);

	S32 local_id;
	msg->getS32(_PREHASH_Data, _PREHASH_LocalID, local_id);

	LLUUID parcel_id;
	msg->getUUID(_PREHASH_Data, _PREHASH_ParcelID, parcel_id);

	F32 dwell;
	msg->getF32(_PREHASH_Data, _PREHASH_Dwell, dwell);

	if (local_id == parcel->getLocalID())
	{
		gViewerParcelMgr.mSelectedDwell = dwell;
		gViewerParcelMgr.notifySelectionObservers();
	}
}

void LLViewerParcelMgr::sendParcelAccessListUpdate(U32 which)
{
	if (!mSelected || !mCurrentParcel)
	{
		return;
	}

	LLViewerRegion* region = gWorld.getRegionFromPosGlobal(mWestSouth);
	if (!region)
	{
		return;
	}

	S32 parcel_local_id = mCurrentParcel->getLocalID();
	if (which & AL_ACCESS)
	{
		LL_DEBUGS("ParcelAccess") << "Sending parcel access list update"
								  << LL_ENDL;
		sendParcelAccessListUpdate(AL_ACCESS, mCurrentParcel->mAccessList,
								   region, parcel_local_id);
	}
	if (which & AL_BAN)
	{
		LL_DEBUGS("ParcelAccess") << "Sending parcel ban list update"
								  << LL_ENDL;
		sendParcelAccessListUpdate(AL_BAN, mCurrentParcel->mBanList,
								   region, parcel_local_id);
	}
	if (which & AL_ALLOW_EXPERIENCE)
	{
		sendParcelAccessListUpdate(AL_ALLOW_EXPERIENCE,
								   mCurrentParcel->getExperienceKeysByType(EXPERIENCE_KEY_TYPE_ALLOWED),
								   region, parcel_local_id);
	}
	if (which & AL_BLOCK_EXPERIENCE)
	{
		sendParcelAccessListUpdate(AL_BLOCK_EXPERIENCE,
								   mCurrentParcel->getExperienceKeysByType(EXPERIENCE_KEY_TYPE_BLOCKED),
								   region, parcel_local_id);
	}
}

void LLViewerParcelMgr::sendParcelAccessListUpdate(U32 flags,
												   const access_map_t& entries,
												   LLViewerRegion* region,
												   S32 parcel_local_id)
{
	bool is_access = (flags & (AL_ACCESS | AL_BAN)) != 0;
	S32 count = entries.size();
	S32 num_sections = (S32)ceil(count / PARCEL_MAX_ENTRIES_PER_PACKET);
	S32 sequence_id = 1;

	LLUUID transaction_id;
	transaction_id.generate();

	LLMessageSystem* msg = gMessageSystemp;

	bool start_message = true;
	bool initial = true;
	access_map_t::const_iterator cit = entries.begin();
	access_map_t::const_iterator end = entries.end();
	while (cit != end || initial)
	{
		if (start_message)
		{
			msg->newMessageFast(_PREHASH_ParcelAccessListUpdate);
			msg->nextBlockFast(_PREHASH_AgentData);
			msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
			msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
			msg->nextBlockFast(_PREHASH_Data);
			msg->addU32Fast(_PREHASH_Flags, flags);
			msg->addS32(_PREHASH_LocalID, parcel_local_id);
			msg->addUUIDFast(_PREHASH_TransactionID, transaction_id);
			msg->addS32Fast(_PREHASH_SequenceID, sequence_id);
			msg->addS32Fast(_PREHASH_Sections, num_sections);
			start_message = false;

			if (initial && cit == end)
			{
				// Pack an empty block if there will be no data
				msg->nextBlockFast(_PREHASH_List);
				msg->addUUIDFast(_PREHASH_ID, LLUUID::null);
				msg->addS32Fast(_PREHASH_Time, 0);
				msg->addU32Fast(_PREHASH_Flags,	0);
			}

			initial = false;
			++sequence_id;
		}

		while (cit != end && msg->getCurrentSendTotal() < MTUBYTES)
		{
			const LLAccessEntry& entry = cit->second;
			msg->nextBlockFast(_PREHASH_List);
			msg->addUUIDFast(_PREHASH_ID, entry.mID);
			msg->addS32Fast(_PREHASH_Time, entry.mTime);
			msg->addU32Fast(_PREHASH_Flags, entry.mFlags);
			++cit;
			if (is_access)
			{
				LL_DEBUGS("ParcelAccess") << "Sending data for agent: "
										  << entry.mID << LL_ENDL;
			}
		}

		start_message = true;
		msg->sendReliable(region->getHost());
	}
}

void LLViewerParcelMgr::deedLandToGroup()
{
	if (!gCacheNamep)	// Paranoia
	{
		return;
	}

	std::string group_name;
	gCacheNamep->getGroupName(mCurrentParcel->getGroupID(), group_name);
	LLSD args;
	args["AREA"] = llformat("%d", mCurrentParcel->getArea());
	args["GROUP_NAME"] = group_name;
	if (mCurrentParcel->getContributeWithDeed())
	{
		std::string first_name, last_name;
		gCacheNamep->getName(mCurrentParcel->getOwnerID(), first_name, last_name);
		args["FIRST_NAME"] = first_name;
		args["LAST_NAME"] = last_name;
		gNotifications.add("DeedLandToGroupWithContribution", args, LLSD(),
						   deedAlertCB);
	}
	else
	{
		gNotifications.add("DeedLandToGroup", args, LLSD(), deedAlertCB);
	}
}

//static
bool LLViewerParcelMgr::deedAlertCB(const LLSD& notification,
									const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		LLUUID group_id;
		LLParcel* parcel = gViewerParcelMgr.getParcelSelection()->getParcel();
		if (parcel)
		{
			group_id = parcel->getGroupID();
		}
		gViewerParcelMgr.sendParcelDeed(group_id);
	}

	return false;
}

void LLViewerParcelMgr::startReleaseLand()
{
	if (!mSelected || !mCurrentParcel)
	{
		gNotifications.add("CannotReleaseLandNothingSelected");
		return;
	}

	if (mRequestResult == PARCEL_RESULT_NO_DATA)
	{
		gNotifications.add("CannotReleaseLandWatingForServer");
		return;
	}

	if (mRequestResult == PARCEL_RESULT_MULTIPLE)
	{
		gNotifications.add("CannotReleaseLandSelected");
		return;
	}

	if (!isParcelOwnedByAgent(mCurrentParcel, GP_LAND_RELEASE) &&
		!gAgent.canManageEstate())
	{
		gNotifications.add("CannotReleaseLandDontOwn");
		return;
	}

	LLVector3d parcel_center = (mWestSouth + mEastNorth) / 2.0;
	LLViewerRegion* region = gWorld.getRegionFromPosGlobal(parcel_center);
	if (!region)
	{
		gNotifications.add("CannotReleaseLandRegionNotFound");
		return;
	}
#if 0
	if (!gAgent.isGodlike() &&
		region->getRegionFlag(REGION_FLAGS_BLOCK_LAND_RESELL))
	{
		LLSD args;
		args["REGION"] = region->getName();
		gNotifications.add("CannotReleaseLandNoTransfer", args);
		return;
	}
#endif
	if (!mCurrentParcelSelection->mWholeParcelSelected)
	{
		gNotifications.add("CannotReleaseLandPartialSelection");
		return;
	}

	// Compute claim price
	LLSD args;
	args["AREA"] = llformat("%d",mCurrentParcel->getArea());
	gNotifications.add("ReleaseLandWarning", args, LLSD(), releaseAlertCB);
}

bool LLViewerParcelMgr::canAgentBuyParcel(LLParcel* parcel,
										  bool for_group) const
{
	if (!parcel)
	{
		return false;
	}

	if (mSelected && parcel == mCurrentParcel &&
		mRequestResult == PARCEL_RESULT_NO_DATA)
	{
		return false;
	}

	if (parcel->isPublic())
	{
		return true;	// change this if want to make it gods only
	}

	LLViewerRegion* regionp = gViewerParcelMgr.getSelectionRegion();
	if (regionp)
	{
		// If the region is PG, we are happy already, so do nothing, but if we
		// are set to avoid either mature or adult, get us outta here.
		U8 sim_access = regionp->getSimAccess();
		if ((sim_access == SIM_ACCESS_MATURE && !gAgent.canAccessMature()) ||
			(sim_access == SIM_ACCESS_ADULT && !gAgent.canAccessAdult()))
		{
			return false;
		}
	}

	const LLUUID& authorize_buyer = parcel->getAuthorizedBuyerID();
	if (!parcel->getForSale() ||
		(parcel->getSalePrice() <= 0 && authorize_buyer.isNull()))
	{
		// Parcel not for sale to anyone
		return false;
	}
	if (authorize_buyer.notNull() && authorize_buyer != gAgentID)
	{
		// Parcel is not reserved for buying by this agent
		return false;
	}

	const LLUUID& parcel_owner = parcel->getOwnerID();
	if (for_group)
	{
		if (parcel_owner == gAgent.getGroupID())
		{
			// Already owning this parcel !  We are actually selling it...
			return false;
		}
		if (!gAgent.hasPowerInActiveGroup(GP_LAND_DEED))
		{
			// Agent not empowered with group land deeding
			return false;
		}
	}
	else if (parcel_owner == gAgentID)
	{
		// Already owning this parcel !  We are actually selling it...
		return false;
	}

	return true;
}

void LLViewerParcelMgr::startBuyLand(bool is_for_group)
{
	LLFloaterBuyLand::buyLand(getSelectionRegion(), mCurrentParcelSelection,
							  is_for_group);
}

void LLViewerParcelMgr::startSellLand()
{
	LLFloaterSellLand::sellLand(getSelectionRegion(), mCurrentParcelSelection);
}

void LLViewerParcelMgr::startDivideLand()
{
	if (!mSelected)
	{
		gNotifications.add("CannotDivideLandNothingSelected");
		return;
	}

	if (mCurrentParcelSelection->mWholeParcelSelected)
	{
		gNotifications.add("CannotDivideLandPartialSelection");
		return;
	}

	LLSD payload;
	payload["west_south_border"] = ll_sd_from_vector3d(mWestSouth);
	payload["east_north_border"] = ll_sd_from_vector3d(mEastNorth);

	gNotifications.add("LandDivideWarning", LLSD(), payload,
					   callbackDivideLand);
}

//static
bool LLViewerParcelMgr::callbackDivideLand(const LLSD& notification,
										   const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		LLVector3d west_south_d =
			ll_vector3d_from_sd(notification["payload"]["west_south_border"]);
		LLVector3d east_north_d =
			ll_vector3d_from_sd(notification["payload"]["east_north_border"]);
		LLVector3d parcel_center = (west_south_d + east_north_d) / 2.0;
		LLViewerRegion* region = gWorld.getRegionFromPosGlobal(parcel_center);
		if (!region)
		{
			gNotifications.add("CannotDivideLandNoRegion");
			return false;
		}

		LLVector3 west_south = region->getPosRegionFromGlobal(west_south_d);
		LLVector3 east_north = region->getPosRegionFromGlobal(east_north_d);

		LLMessageSystem* msg = gMessageSystemp;
		msg->newMessage(_PREHASH_ParcelDivide);
		msg->nextBlock(_PREHASH_AgentData);
		msg->addUUID(_PREHASH_AgentID, gAgentID);
		msg->addUUID(_PREHASH_SessionID, gAgentSessionID);
		msg->nextBlock(_PREHASH_ParcelData);
		msg->addF32(_PREHASH_West, west_south.mV[VX]);
		msg->addF32(_PREHASH_South, west_south.mV[VY]);
		msg->addF32(_PREHASH_East, east_north.mV[VX]);
		msg->addF32(_PREHASH_North, east_north.mV[VY]);
		msg->sendReliable(region->getHost());
	}

	return false;
}

void LLViewerParcelMgr::startJoinLand()
{
	if (!mSelected)
	{
		gNotifications.add("CannotJoinLandNothingSelected");
		return;
	}

	if (mCurrentParcelSelection->mWholeParcelSelected)
	{
		gNotifications.add("CannotJoinLandEntireParcelSelected");
		return;
	}

	if (!mCurrentParcelSelection->mSelectedMultipleOwners)
	{
		gNotifications.add("CannotJoinLandSelection");
		return;
	}

	LLSD payload;
	payload["west_south_border"] = ll_sd_from_vector3d(mWestSouth);
	payload["east_north_border"] = ll_sd_from_vector3d(mEastNorth);

	gNotifications.add("JoinLandWarning", LLSD(), payload, callbackJoinLand);
}

//static
bool LLViewerParcelMgr::callbackJoinLand(const LLSD& notification,
										 const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		LLVector3d west_south_d =
			ll_vector3d_from_sd(notification["payload"]["west_south_border"]);
		LLVector3d east_north_d =
			ll_vector3d_from_sd(notification["payload"]["east_north_border"]);
		LLVector3d parcel_center = (west_south_d + east_north_d) / 2.0;
		LLViewerRegion* region = gWorld.getRegionFromPosGlobal(parcel_center);
		if (!region)
		{
			gNotifications.add("CannotJoinLandNoRegion");
			return false;
		}

		LLVector3 west_south = region->getPosRegionFromGlobal(west_south_d);
		LLVector3 east_north = region->getPosRegionFromGlobal(east_north_d);

		LLMessageSystem* msg = gMessageSystemp;
		msg->newMessage(_PREHASH_ParcelJoin);
		msg->nextBlock(_PREHASH_AgentData);
		msg->addUUID(_PREHASH_AgentID, gAgentID);
		msg->addUUID(_PREHASH_SessionID, gAgentSessionID);
		msg->nextBlock(_PREHASH_ParcelData);
		msg->addF32(_PREHASH_West, west_south.mV[VX]);
		msg->addF32(_PREHASH_South, west_south.mV[VY]);
		msg->addF32(_PREHASH_East, east_north.mV[VX]);
		msg->addF32(_PREHASH_North, east_north.mV[VY]);
		msg->sendReliable(region->getHost());
	}

	return false;
}

void LLViewerParcelMgr::startDeedLandToGroup()
{
	if (!mSelected || !mCurrentParcel)
	{
		gNotifications.add("CannotDeedLandNothingSelected");
		return;
	}

	if (mRequestResult == PARCEL_RESULT_NO_DATA)
	{
		gNotifications.add("CannotDeedLandWaitingForServer");
		return;
	}

	if (mRequestResult == PARCEL_RESULT_MULTIPLE)
	{
		gNotifications.add("CannotDeedLandMultipleSelected");
		return;
	}

	LLVector3d parcel_center = (mWestSouth + mEastNorth) / 2.0;
	LLViewerRegion* region = gWorld.getRegionFromPosGlobal(parcel_center);
	if (!region)
	{
		gNotifications.add("CannotDeedLandNoRegion");
		return;
	}

#if 0
	if (!gAgent.isGodlike() &&
		region->getRegionFlag(REGION_FLAGS_BLOCK_LAND_RESELL) &&
		mCurrentParcel->getOwnerID() != region->getOwner())
	{
		LLSD args;
		args["REGION"] = region->getName();
		gNotifications.add("CannotDeedLandNoTransfer", args);
		return;
	}
#endif

	deedLandToGroup();
}

void LLViewerParcelMgr::reclaimParcel()
{
	LLParcel* parcel = gViewerParcelMgr.getParcelSelection()->getParcel();
	LLViewerRegion* regionp = gViewerParcelMgr.getSelectionRegion();
	if (parcel && parcel->getOwnerID().notNull()
		&& parcel->getOwnerID() != gAgentID
		&& regionp && regionp->getOwner() == gAgentID)
	{
		LLMessageSystem* msg = gMessageSystemp;
		msg->newMessage(_PREHASH_ParcelReclaim);
		msg->nextBlock(_PREHASH_AgentData);
		msg->addUUID(_PREHASH_AgentID, gAgentID);
		msg->addUUID(_PREHASH_SessionID, gAgentSessionID);
		msg->nextBlock(_PREHASH_Data);
		msg->addS32(_PREHASH_LocalID, parcel->getLocalID());
		msg->sendReliable(regionp->getHost());
	}
}

//static
bool LLViewerParcelMgr::releaseAlertCB(const LLSD& notification,
									   const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		// Send the release message, not a force
		gViewerParcelMgr.sendParcelRelease();
	}
	return false;
}

void LLViewerParcelMgr::buyPass()
{
	LLParcel* parcel = getParcelSelection()->getParcel();
	if (!parcel) return;

	LLViewerRegion* region = getSelectionRegion();
	if (!region) return;

	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessageFast(_PREHASH_ParcelBuyPass);
	msg->nextBlock(_PREHASH_AgentData);
	msg->addUUID(_PREHASH_AgentID, gAgentID);
	msg->addUUID(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlockFast(_PREHASH_ParcelData);
	msg->addS32Fast(_PREHASH_LocalID, parcel->getLocalID());
	msg->sendReliable(region->getHost());
}

// Tells whether we are allowed to buy a pass or not
bool LLViewerParcelMgr::isCollisionBanned()
{
	return !(mCollisionBanned == BA_ALLOWED ||
			 mCollisionBanned == BA_NOT_ON_LIST ||
			 mCollisionBanned == BA_NOT_IN_GROUP);
}

// This implementation should mirror LLSimParcelMgr::isParcelOwnedBy
//static
bool LLViewerParcelMgr::isParcelOwnedByAgent(const LLParcel* parcelp,
											 U64 group_proxy_power)
{
	if (!parcelp)
	{
		return false;
	}


	if (gAgent.isGodlike() ||	// Gods can always assume ownership.
		// The owner of a parcel automatically gets all powers.
		parcelp->getOwnerID() == gAgentID)
	{
		return true;
	}

	// Only gods can assume 'ownership' of public land.
	if (parcelp->isPublic())
	{
		return false;
	}

	// Return whether or not the agent has group_proxy_power powers in the
	// parcel's group.
	return gAgent.hasPowerInGroup(parcelp->getOwnerID(), group_proxy_power);
}

// This implementation should mirror llSimParcelMgr::isParcelModifiableBy
//static
bool LLViewerParcelMgr::isParcelModifiableByAgent(const LLParcel* parcelp,
												  U64 group_proxy_power)
{
	if (!parcelp)
	{
		return false;
	}

	// If the parcel is not OS_LEASED for agent-owned parcels, then it cannot
	// be modified anyway.
	if (parcelp->getOwnerID() == gAgentID && !gAgent.isGodlike() &&
		parcelp->getOwnershipStatus() != LLParcel::OS_LEASED)
	{
		return false;
	}

	// *NOTE: This should only work for leased parcels, but group owned
	// parcels cannot be OS_LEASED yet. Phoenix 2003-12-15.
	return isParcelOwnedByAgent(parcelp, group_proxy_power);
}

void sanitize_corners(const LLVector3d& corner1, const LLVector3d& corner2,
					  LLVector3d& west_south_bottom, LLVector3d& east_north_top)
{
	west_south_bottom.mdV[VX] = llmin(corner1.mdV[VX], corner2.mdV[VX]);
	west_south_bottom.mdV[VY] = llmin(corner1.mdV[VY], corner2.mdV[VY]);
	west_south_bottom.mdV[VZ] = llmin(corner1.mdV[VZ], corner2.mdV[VZ]);

	east_north_top.mdV[VX] = llmax(corner1.mdV[VX], corner2.mdV[VX]);
	east_north_top.mdV[VY] = llmax(corner1.mdV[VY], corner2.mdV[VY]);
	east_north_top.mdV[VZ] = llmax(corner1.mdV[VZ], corner2.mdV[VZ]);
}

LLViewerTexture* LLViewerParcelMgr::getBlockedImage() const
{
	return sBlockedImage;
}

LLViewerTexture* LLViewerParcelMgr::getPassImage() const
{
	return sPassImage;
}

boost::signals2::connection LLViewerParcelMgr::addAgentParcelChangedCB(parcel_changed_cb_t cb)
{
	return mAgentParcelChangedSignal.connect(cb);
}

// This callback is called without delay (i.e. without waiting for the new
// parcel data) after the agent is teleported; it is used to close TP-related
// floaters on successful teleports.
boost::signals2::connection LLViewerParcelMgr::setTPArrivingCallback(parcel_changed_cb_t cb)
{
	return mTeleportArrivingSignal.connect(cb);
}

// Set finish teleport callback. You can use it to observe all teleport events.
// NOTE: After local teleports we cannot rely on gAgent.getPositionGlobal(), so
// the new position gets passed explicitly. Use args of this callback to get
// global position of avatar after teleport event.
boost::signals2::connection LLViewerParcelMgr::setTPFinishedCallback(tp_finished_cb_t cb)
{
	return mTeleportFinishedSignal.connect(cb);
}

boost::signals2::connection LLViewerParcelMgr::setTPFailedCallback(parcel_changed_cb_t cb)
{
	return mTeleportFailedSignal.connect(cb);
}

// We are notified that the teleport has been finished. We should now propagate
// the notification via mTeleportFinishedSignal to all interested parties.
void LLViewerParcelMgr::onTeleportFinished(bool local,
										   const LLVector3d& new_pos)
{
	mTeleportArrivingSignal();

	// Treat only teleports within the same parcel as local (EXT-3139).
	if (local && inAgentParcel(new_pos))
	{
		// Local teleport. We already have the agent parcel data.
		// Emit the signal immediately.
		mTeleportFinishedSignal(new_pos, local);
	}
	else
	{
		// Non-local teleport (inter-region or between different parcels of the
		// same region). The agent parcel data has not been updated yet. Let's
		// wait for the update and then emit the signal.
		mTeleportInProgress = true;
	}
}

void LLViewerParcelMgr::onTeleportFailed()
{
	mTeleportFailedSignal();
}

///////////////////////////////////////////////////////////////////////////////
// Methods for parcel info observers (implemented in llremoteparcelrequest.cpp
// in v3 viewers).

void LLViewerParcelMgr::addInfoObserver(const LLUUID& parcel_id,
										LLParcelInfoObserver* obs)
{
	if (!obs || parcel_id.isNull()) return;

	for (info_obs_multimap_t::iterator
			it = mInfoObservers.lower_bound(parcel_id),
			end = mInfoObservers.upper_bound(parcel_id);
		 it != end; ++it)
	{
		if (it->second.get() == obs)
		{
			return;
		}
	}

	mInfoObservers.emplace(parcel_id, obs->getObserverHandle());
}

void LLViewerParcelMgr::removeInfoObserver(const LLUUID& parcel_id,
										   LLParcelInfoObserver* obs)
{
	if (!obs || parcel_id.isNull()) return;

	for (info_obs_multimap_t::iterator
			it = mInfoObservers.lower_bound(parcel_id),
			end = mInfoObservers.upper_bound(parcel_id);
		 it != end; ++it)
	{
		if (it->second.get() == obs)
		{
			mInfoObservers.erase(it);
			break;
		}
	}
}

//static
void LLViewerParcelMgr::processParcelInfoReply(LLMessageSystem* msg, void**)
{
	LLParcelData parcel_data;
	msg->getUUID(_PREHASH_Data, _PREHASH_ParcelID, parcel_data.mParcelId);
	msg->getUUID(_PREHASH_Data, _PREHASH_OwnerID, parcel_data.mOwnerId);
	msg->getString(_PREHASH_Data, _PREHASH_Name, parcel_data.mName);
	msg->getString(_PREHASH_Data, _PREHASH_Desc, parcel_data.mDesc);
	msg->getS32(_PREHASH_Data, _PREHASH_ActualArea, parcel_data.mActualArea);
	msg->getS32(_PREHASH_Data, _PREHASH_BillableArea, parcel_data.mBillableArea);
	msg->getU8(_PREHASH_Data, _PREHASH_Flags, parcel_data.mFlags);
	msg->getF32(_PREHASH_Data, _PREHASH_GlobalX, parcel_data.mGlobalX);
	msg->getF32(_PREHASH_Data, _PREHASH_GlobalY, parcel_data.mGlobalY);
	msg->getF32(_PREHASH_Data, _PREHASH_GlobalZ, parcel_data.mGlobalZ);
	msg->getString(_PREHASH_Data, _PREHASH_SimName, parcel_data.mSimName);
	msg->getUUID(_PREHASH_Data, _PREHASH_SnapshotID, parcel_data.mSnapshotId);
	msg->getF32(_PREHASH_Data, _PREHASH_Dwell, parcel_data.mDwell);
	msg->getS32(_PREHASH_Data, _PREHASH_SalePrice, parcel_data.mSalePrice);
	msg->getS32(_PREHASH_Data, _PREHASH_AuctionID, parcel_data.mAuctionId);

	typedef std::vector<info_obs_multimap_t::iterator> deadlist_t;
	deadlist_t dead_iters;

	const LLUUID& parcel_id = parcel_data.mParcelId;
	info_obs_multimap_t::iterator obs_it, obs_end;
	obs_it = gViewerParcelMgr.mInfoObservers.lower_bound(parcel_id);
	obs_end = gViewerParcelMgr.mInfoObservers.upper_bound(parcel_id);
	while (obs_it != obs_end)
	{
		// Increment the loop iterator now since it may become invalid below
		info_obs_multimap_t::iterator cur_it = obs_it++;
		LLParcelInfoObserver* observer = cur_it->second.get();
		if (observer)
		{
			// May invalidate cur_it if the observer removes itself
			observer->processParcelInfo(parcel_data);
		}
		else
		{
			// The handle points to an expired observer, so do not keep it
			dead_iters.push_back(cur_it);
		}
	}

	// Remove iterators correpsonding to dead observers
	for (deadlist_t::iterator it = dead_iters.begin(), end = dead_iters.end();
		 it != end; ++it)
	{
		gViewerParcelMgr.mInfoObservers.erase(*it);
	}
}

void LLViewerParcelMgr::sendParcelInfoRequest(const LLUUID& parcel_id)
{
	LLMessageSystem* msg = gMessageSystemp;
	if (msg)
	{
		msg->newMessage(_PREHASH_ParcelInfoRequest);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
		msg->addUUID(_PREHASH_SessionID, gAgentSessionID);
		msg->nextBlock(_PREHASH_Data);
		msg->addUUID(_PREHASH_ParcelID, parcel_id);
		gAgent.sendReliableMessage();
	}
}

bool LLViewerParcelMgr::requestRegionParcelInfo(const std::string& url,
												const LLUUID& region_id,
												const LLVector3& region_pos,
												const LLVector3d& global_pos,
												LLHandle<LLParcelInfoObserver> obs_handle)
{
	if (url.empty()) return false;

	gCoros.launch("LLViewerParcelMgr::regionParcelInfoCoro",
				  boost::bind(&LLViewerParcelMgr::regionParcelInfoCoro, this,
							  url, region_id, region_pos, global_pos,
							  obs_handle));
	return true;
}

void LLViewerParcelMgr::regionParcelInfoCoro(const std::string& url,
											 LLUUID region_id,
											 LLVector3 pos_region,
											 LLVector3d pos_global,
											 LLHandle<LLParcelInfoObserver> obs_handle)
{
	LLSD body;
	body["location"] = ll_sd_from_vector3(pos_region);
	if (region_id.notNull())
	{
		body["region_id"] = region_id;
	}
	if (!pos_global.isExactlyZero())
	{
		U64 region_handle = to_region_handle(pos_global);
		body["region_handle"] = ll_sd_from_U64(region_handle);
	}

	LLCoreHttpUtil::HttpCoroutineAdapter adapter("regionParcelInfoCoro");
	LLSD result = adapter.postAndSuspend(url, body);

	LLParcelInfoObserver* observer = obs_handle.get();
	if (!observer) return;	// Observer has since been removed

	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	if (status)
	{
		if (result.isMap() && result.has("parcel_id"))
		{
			LLUUID parcel_id = result["parcel_id"];
			observer->setParcelID(parcel_id);
		}
		else
		{
			llwarns << "Malformed response contents fetching info for parcel at: "
					<< pos_region << " - In region: " << region_id << llendl;
		}
	}
	else
	{
		observer->setErrorStatus(status.getType(), status.getMessage());
	}
}

///////////////////////////////////////////////////////////////////////////////
// Methods that used to be in llglsandbox.cpp

// Used by lltoolselectland
void LLViewerParcelMgr::renderRect(const LLVector3d& west_south_bottom_global,
								   const LLVector3d& east_north_top_global)
{
	LLGLSUIDefault gls_ui;
	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
	LLGLDepthTest gls_depth(GL_TRUE);

	LLVector3 west_south_bottom_agent =
		gAgent.getPosAgentFromGlobal(west_south_bottom_global);
	F32 west = west_south_bottom_agent.mV[VX];
	F32 south = west_south_bottom_agent.mV[VY];

	LLVector3 east_north_top_agent = gAgent.getPosAgentFromGlobal(east_north_top_global);
	F32 east = east_north_top_agent.mV[VX];
	F32 north = east_north_top_agent.mV[VY];

#if 0
	F32 top = east_north_top_agent.mV[VZ] + 1.f;
	F32 bottom = west_south_bottom_agent.mV[VZ] - 1.f;
#endif

	// *HACK: at edge of last region of world, we need to make sure the region
	// resolves correctly so we can get a height value.
	constexpr F32 FUDGE = 0.01f;

	F32 sw_bottom = gWorld.resolveLandHeightAgent(LLVector3(west, south, 0.f));
	F32 se_bottom =
		gWorld.resolveLandHeightAgent(LLVector3(east - FUDGE, south, 0.f));
	F32 ne_bottom = gWorld.resolveLandHeightAgent(LLVector3(east - FUDGE,
															north - FUDGE,
															0.f));
	F32 nw_bottom = gWorld.resolveLandHeightAgent(LLVector3(west,
															north - FUDGE,
															0.f));

	F32 sw_top = sw_bottom + PARCEL_POST_HEIGHT;
	F32 se_top = se_bottom + PARCEL_POST_HEIGHT;
	F32 ne_top = ne_bottom + PARCEL_POST_HEIGHT;
	F32 nw_top = nw_bottom + PARCEL_POST_HEIGHT;

	LLUI::setLineWidth(2.f);
	gGL.color4f(1.f, 1.f, 0.f, 1.f);

	// Cheat and give this the same pick-name as land
	gGL.begin(LLRender::LINES);
	{
		gGL.vertex3f(west, north, nw_bottom);
		gGL.vertex3f(west, north, nw_top);

		gGL.vertex3f(east, north, ne_bottom);
		gGL.vertex3f(east, north, ne_top);

		gGL.vertex3f(east, south, se_bottom);
		gGL.vertex3f(east, south, se_top);

		gGL.vertex3f(west, south, sw_bottom);
		gGL.vertex3f(west, south, sw_top);
	}
	gGL.end();

	gGL.color4f(1.f, 1.f, 0.f, 0.2f);
	gGL.begin(LLRender::TRIANGLES);
	{
		gGL.vertex3f(west, north, nw_bottom);
		gGL.vertex3f(west, north, nw_top);
		gGL.vertex3f(east, north, ne_top);
		gGL.vertex3f(west, north, nw_bottom);
		gGL.vertex3f(east, north, ne_top);
		gGL.vertex3f(east, north, ne_bottom);

		gGL.vertex3f(east, north, ne_bottom);
		gGL.vertex3f(east, north, ne_top);
		gGL.vertex3f(east, south, se_top);
		gGL.vertex3f(east, north, ne_bottom);
		gGL.vertex3f(east, south, se_top);
		gGL.vertex3f(east, south, se_bottom);

		gGL.vertex3f(east, south, se_bottom);
		gGL.vertex3f(east, south, se_top);
		gGL.vertex3f(west, south, sw_top);
		gGL.vertex3f(east, south, se_bottom);
		gGL.vertex3f(west, south, sw_top);
		gGL.vertex3f(west, south, sw_bottom);

		gGL.vertex3f(west, south, sw_bottom);
		gGL.vertex3f(west, south, sw_top);
		gGL.vertex3f(west, north, nw_top);
		gGL.vertex3f(west, south, sw_bottom);
		gGL.vertex3f(west, north, nw_top);
		gGL.vertex3f(west, north, nw_bottom);
	}
	gGL.end();

	LLUI::setLineWidth(1.f);
}

// North = a wall going north/south. Need that info to set up texture
// coordinates correctly.
void LLViewerParcelMgr::renderOneSegment(F32 x1, F32 y1, F32 x2, F32 y2,
										 F32 height, U8 direction,
										 LLViewerRegion* regionp)
{
	if (!regionp)
	{
		return;
	}

	// Variable region size support
	F32 border = regionp->getWidth() - 0.1f;

	// *HACK: at edge of last region of world, we need to make sure the region
	// resolves correctly so we can get a height value.
	F32 clamped_x1 = llmin(x1, border);
	F32 clamped_y1 = llmin(y1, border);
	F32 clamped_x2 = llmin(x2, border);
	F32 clamped_y2 = llmin(y2, border);

	F32 z1 = regionp->getLand().resolveHeightRegion(LLVector3(clamped_x1,
															  clamped_y1,
															  0.f));
	F32 z2 = regionp->getLand().resolveHeightRegion(LLVector3(clamped_x2,
															  clamped_y2,
															  0.f));

	// Convert x1 and x2 from region-local to agent coords.
	LLVector3 origin = regionp->getOriginAgent();
	x1 += origin.mV[VX];
	x2 += origin.mV[VX];
	y1 += origin.mV[VY];
	y2 += origin.mV[VY];

	if (height < 1.f)
	{
		F32 z = z1 + height;
		gGL.vertex3f(x1, y1, z);

		gGL.vertex3f(x1, y1, z1);

		z = z2 + height;
		gGL.vertex3f(x2, y2, z);
		gGL.vertex3f(x2, y2, z);
		gGL.vertex3f(x1, y1, z1);
		gGL.vertex3f(x2, y2, z2);
	}
	else
	{
		F32 tex_coord1, tex_coord2;
		if (direction == WEST_MASK)
		{
			tex_coord1 = y1;
			tex_coord2 = y2;
		}
		else if (direction == SOUTH_MASK)
		{
			tex_coord1 = x1;
			tex_coord2 = x2;
		}
		else if (direction == EAST_MASK)
		{
			tex_coord1 = y2;
			tex_coord2 = y1;
		}
		else // if (direction == NORTH_MASK)
		{
			tex_coord1 = x2;
			tex_coord2 = x1;
		}

		gGL.texCoord2f(tex_coord1 * 0.5f + 0.5f, z1 * 0.5f);
		gGL.vertex3f(x1, y1, z1);

		gGL.texCoord2f(tex_coord2 * 0.5f + 0.5f, z2 * 0.5f);
		gGL.vertex3f(x2, y2, z2);

		// Top edge stairsteps
		F32 z = llmax(z2 + height, z1 + height);
		gGL.texCoord2f(tex_coord1 * 0.5f + 0.5f, z * 0.5f);
		gGL.vertex3f(x1, y1, z);
		gGL.texCoord2f(tex_coord1 * 0.5f + 0.5f, z * 0.5f);
		gGL.vertex3f(x1, y1, z);
		gGL.texCoord2f(tex_coord2 * 0.5f+ 0.5f, z * 0.5f);
		gGL.vertex3f(x2, y2, z);
		gGL.texCoord2f(tex_coord1 * 0.5f + 0.5f, z1 * 0.5f);
		gGL.vertex3f(x1, y1, z1);
	}
}

void LLViewerParcelMgr::renderHighlightSegments(const U8* segments,
												LLViewerRegion* regionp)
{
	bool has_segments = false;

	LLGLSUIDefault gls_ui;
	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
	LLGLDepthTest gls_depth(GL_TRUE);

	gGL.color4f(1.f, 1.f, 0.f, 0.2f);

	const S32 stride = mParcelsPerEdge + 1;
	for (S32 y = 0; y < stride; ++y)
	{
		for (S32 x = 0; x < stride; ++x)
		{
			U8 segment_mask = segments[x + y * stride];

			if (segment_mask & SOUTH_MASK)
			{
				F32 x1 = x * PARCEL_GRID_STEP_METERS;
				F32 y1 = y * PARCEL_GRID_STEP_METERS;
				if (!has_segments)
				{
					has_segments = true;
					gGL.begin(LLRender::TRIANGLES);
				}
				renderOneSegment(x1, y1, x1 + PARCEL_GRID_STEP_METERS, y1,
								 PARCEL_POST_HEIGHT, SOUTH_MASK, regionp);
			}

			if (segment_mask & WEST_MASK)
			{
				F32 x1 = x * PARCEL_GRID_STEP_METERS;
				F32 y1 = y * PARCEL_GRID_STEP_METERS;
				if (!has_segments)
				{
					has_segments = true;
					gGL.begin(LLRender::TRIANGLES);
				}
				renderOneSegment(x1, y1, x1, y1 + PARCEL_GRID_STEP_METERS,
								 PARCEL_POST_HEIGHT, WEST_MASK, regionp);
			}
		}
	}

	if (has_segments)
	{
		gGL.end();
	}
}

void LLViewerParcelMgr::renderCollisionSegments(U8* segments, bool use_pass,
												LLViewerRegion* regionp)
{
	LLVector3 pos = gAgent.getPositionAgent();
	F32 pos_x = pos.mV[VX];
	F32 pos_y = pos.mV[VY];

	LLGLSUIDefault gls_ui;
	LLGLDepthTest gls_depth(GL_TRUE, GL_FALSE);
	LLGLDisable cull(GL_CULL_FACE);

	F32 collision_height;
	if (mCollisionBanned == BA_BANNED ||
		regionp->getRegionFlag(REGION_FLAGS_BLOCK_FLYOVER))
	{
		collision_height = BAN_HEIGHT;
	}
	else
	{
		collision_height = PARCEL_HEIGHT;
	}

	if (use_pass && mCollisionBanned == BA_NOT_ON_LIST)
	{
		gGL.getTexUnit(0)->bind(mPassImage);
	}
	else
	{
		gGL.getTexUnit(0)->bind(mBlockedImage);
	}

	gGL.begin(LLRender::TRIANGLES);

	constexpr F32 MAX_ALPHA = 0.95f;
	constexpr F32 MIN_ALPHA = 0.1f;
	constexpr S32 DIST_OFFSET = 5;
	constexpr S32 MIN_DIST_SQ = DIST_OFFSET * DIST_OFFSET;
	// I made this setting configurable, since depending on the conditions
	// (e.g. when riding a vehicle at high speed or with a high inertia), the
	// user may want some more forewarning... HB
	static LLCachedControl<U32> max_dist(gSavedSettings,
										 "RenderBanWallMaxDist");
	const S32 max_dist_sq = llclamp(S32(max_dist * max_dist), 100, 2500);
	const F32 alpha0 = 30.f / 169.f * F32(max_dist_sq);
	const S32 stride = mParcelsPerEdge + 1;
	F32 x1, y1;	// Start point
	F32 x2, y2;	// End point
	F32 alpha;
	F32 dx, dy;
	for (S32 y = 0; y < stride; ++y)
	{
		for (S32 x = 0; x < stride; ++x)
		{
			U8 segment_mask = segments[x + y * stride];

			if (segment_mask & SOUTH_MASK)
			{
				x1 = x * PARCEL_GRID_STEP_METERS;
				y1 = y * PARCEL_GRID_STEP_METERS;

				x2 = x1 + PARCEL_GRID_STEP_METERS;
				y2 = y1;
#if 0			// Silly... This does not take the sign into account ! HB
				dy = pos_y - y1 + DIST_OFFSET;
#else			// Seems (from observed effect) about right ! HB
				dy = fabs(pos_y - y1);
				if (dy >= DIST_OFFSET)
				{
					dy -= DIST_OFFSET;
				}
#endif
				if (pos_x < x1)
				{
					dx = pos_x - x1;
				}
				else if (pos_x > x2)
				{
					dx = pos_x - x2;
				}
				else
				{
					dx = 0.f;
				}

				F32 dist = dx * dx + dy * dy;
				if (dist < MIN_DIST_SQ)
				{
					alpha = MAX_ALPHA;
				}
				else if (dist > max_dist_sq)
				{
					alpha = 0.f;
				}
				else
				{
					alpha = llclamp(alpha0 / dist, MIN_ALPHA, MAX_ALPHA);
				}

				if (alpha > 0.f)
				{
					gGL.color4f(1.f, 1.f, 1.f, alpha);
					U8 direction = pos_y - y1 < 0.f ? SOUTH_MASK : NORTH_MASK;
					// Avoid Z fighting
					renderOneSegment(x1 + 0.1f, y1 + 0.1f,
									 x2 + 0.1f, y2 + 0.1f,
									 collision_height, direction, regionp);
				}
			}

			if (segment_mask & WEST_MASK)
			{
				x1 = x * PARCEL_GRID_STEP_METERS;
				y1 = y * PARCEL_GRID_STEP_METERS;

				x2 = x1;
				y2 = y1 + PARCEL_GRID_STEP_METERS;
#if 0			// Silly... This does not take the sign into account ! HB
				dx = pos_x - x1 + DIST_OFFSET;
#else			// Seems (from observed effect) about right ! HB
				dx = fabs(pos_x - x1);
				if (dx >= DIST_OFFSET)
				{
					dx -= DIST_OFFSET;
				}
#endif
				if (pos_y < y1)
				{
					dy = pos_y - y1;
				}
				else if (pos_y > y2)
				{
					dy = pos_y - y2;
				}
				else
				{
					dy = 0.f;
				}

				F32 dist = dx * dx + dy * dy;
				if (dist < MIN_DIST_SQ)
				{
					alpha = MAX_ALPHA;
				}
				else if (dist > max_dist_sq)
				{
					alpha = 0.f;
				}
				else
				{
					alpha = llclamp(alpha0 / dist, MIN_ALPHA, MAX_ALPHA);
				}
				if (alpha > 0.f)
				{
					gGL.color4f(1.f, 1.f, 1.f, alpha);

					U8 direction = pos_x - x1 > 0.f ? WEST_MASK : EAST_MASK;
					// Avoid Z fighting
					renderOneSegment(x1 + 0.1f, y1 + 0.1f,
									 x2 + 0.1f, y2 + 0.1f,
									 collision_height, direction, regionp);
				}
			}
		}
	}

	gGL.end();
}
