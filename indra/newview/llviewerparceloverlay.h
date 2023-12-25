/**
 * @file llviewerparceloverlay.h
 * @brief LLViewerParcelOverlay class header file
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

#ifndef LL_LLVIEWERPARCELOVERLAY_H
#define LL_LLVIEWERPARCELOVERLAY_H

// The ownership data for land parcels.
// One of these structures per region.

#include <vector>

#include "llbbox.h"
#include "llframetimer.h"
#include "llgl.h"
#include "lluuid.h"

#include "llviewertexture.h"

class LLViewerRegion;
class LLVector3;
class LLColor4U;
class LLVector2;

class LLViewerParcelOverlay : public LLGLUpdate
{
public:
	LLViewerParcelOverlay(LLViewerRegion* region, F32 region_width_meters);
	~LLViewerParcelOverlay();

	// ACCESS
	LL_INLINE LLViewerTexture* getTexture() const	{ return mTexture; }

	bool isOwned(const LLVector3& pos) const;
	bool isOwnedSelf(const LLVector3& pos) const;
	bool isOwnedGroup(const LLVector3& pos) const;
	bool isOwnedOther(const LLVector3& pos) const;

	// "Encroaches" means the prim hangs over the parcel, but its center might
	// be in another parcel. for now, we simply test axis aligned bounding
	// boxes which isn't perfect, but is close.
	bool encroachesOwned(const std::vector<LLBBox>& boxes) const;
	bool encroachesOnUnowned(const std::vector<LLBBox>& boxes) const;
	bool encroachesOnNearbyParcel(const std::vector<LLBBox>& boxes) const;

	F32 getOwnedRatio() const;

	void renderPropertyLines() const;

	U8 ownership(const LLVector3& pos) const;
	U8 parcelLineFlags(S32 row, S32 col) const;

	bool isSoundLocal(const LLVector3& pos) const;

	LL_INLINE S32 getParcelGridsPerEdge() const		{ return mParcelGridsPerEdge; }

	// MANIPULATE
	void uncompressLandOverlay(S32 chunk, U8* compressed_overlay);

	// Indicate property lines and overlay texture need to be rebuilt.
	LL_INLINE void setDirty()						{ mDirty = true; }

	void idleUpdate(bool update_now = false);
	LL_INLINE void updateGL()						{ updateOverlayTexture(); }

	void resetCollisionBitmap();
	void readCollisionBitmap(U8* bitmap);

	void renderParcelBorders(F32 scale, const F32* color) const;
	// Returns true when at least one banned parcel got drawn
	bool renderBannedParcels(F32 scale, const F32* color) const;

private:
	U8 parcelFlags(S32 row, S32 col, U8 mask) const;

	// This is in parcel rows and columns, not grid rows and columns. Stored in
	// bottom three bits.
	LL_INLINE U8 ownership(S32 row, S32 col) const
	{
		return parcelFlags(row, col, (U8)0x7);
	}

	void addPropertyLine(std::vector<LLVector3>& vertex_array,
						 std::vector<LLColor4U>& color_array,
						 std::vector<LLVector2>& coord_array,
						 F32 start_x, F32 start_y, U32 edge,
						 const LLColor4U& color);

	void updateOverlayTexture();
	void updatePropertyLines();

private:
	// Back pointer to the region that owns this structure.
	LLViewerRegion*				mRegion;

	S32							mParcelGridsPerEdge;
	S32							mRegionSize;	// Variable region size support

	S32							mVertexCount;
	F32*						mVertexArray;
	U8*							mColorArray;

	LLPointer<LLViewerTexture>	mTexture;
	LLPointer<LLImageRaw>		mImageRaw;

	// Size: mParcelGridsPerEdge * mParcelGridsPerEdge
	// Each value is 0-3, PARCEL_AVAIL to PARCEL_SELF in the two low bits
	// and other flags in the upper bits.
	U8*							mOwnership;

	std::vector<bool>			mCollisionBitmap;

	// Update propery lines and overlay texture
	LLFrameTimer				mTimeSinceLastUpdate;
	S32							mOverlayTextureIdx;
	bool						mDirty;
	// This is set to true whenever mCollisionBitmap contains at least one true
	// entry (i.e. when info about a banned parcel has been received). Used to
	// speed-up the mini-map rendering when there is nothing to render... HB
	bool						mHasCollisions;
};

#endif
