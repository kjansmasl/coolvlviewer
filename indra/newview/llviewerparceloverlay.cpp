/**
 * @file llviewerparceloverlay.cpp
 * @brief LLViewerParcelOverlay class implementation
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

#include "llviewerparceloverlay.h"

#include "llgl.h"
#include "llparcel.h"
#include "llrender.h"

#include "llagent.h"
#include "llfloatertools.h"
#include "llselectmgr.h"
#include "llsurface.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerregion.h"
#include "llviewertexturelist.h"

constexpr U8 OVERLAY_IMG_COMPONENTS = 4;
constexpr F32 MAX_COORD = REGION_WIDTH_METERS - 1.f;
constexpr F32 STEP_FACTOR = 1.f / PARCEL_GRID_STEP_METERS;

LLViewerParcelOverlay::LLViewerParcelOverlay(LLViewerRegion* region,
											 F32 region_width_meters)
:	mRegion(region),
	mParcelGridsPerEdge(S32(region_width_meters * STEP_FACTOR)),
	mRegionSize(S32(region_width_meters)),	// Variable region size support
	mDirty(false),
	mHasCollisions(false),
	mTimeSinceLastUpdate(),
	mOverlayTextureIdx(-1),
	mVertexCount(0),
	mVertexArray(NULL),
	mColorArray(NULL)
{
	// Create a texture to hold color information; 4 components,
	// use mipmaps = false, clamped, NEAREST filter, for sharp edges.
	mImageRaw = new LLImageRaw(mParcelGridsPerEdge, mParcelGridsPerEdge,
							   OVERLAY_IMG_COMPONENTS);
	mTexture = LLViewerTextureManager::getLocalTexture(mImageRaw.get(), false);
	mTexture->setAddressMode(LLTexUnit::TAM_CLAMP);
	mTexture->setFilteringOption(LLTexUnit::TFO_POINT);

	//
	// Initialize the GL texture with empty data.
	//
	// Create the base texture.
	U8* raw = mImageRaw->getData();
	if (raw)
	{
		S32 count = mParcelGridsPerEdge * mParcelGridsPerEdge *
					OVERLAY_IMG_COMPONENTS;
		for (S32 i = 0; i < count; ++i)
		{
			raw[i] = 0;
		}
		mTexture->setSubImage(mImageRaw, 0, 0, mParcelGridsPerEdge,
							  mParcelGridsPerEdge);

	}

	// Create storage for ownership information from simulator
	// and initialize it.
	mOwnership = new (std::nothrow) U8[mParcelGridsPerEdge * mParcelGridsPerEdge];
	if (mOwnership)
	{
		for (S32 i = 0; i < mParcelGridsPerEdge * mParcelGridsPerEdge; ++i)
		{
			mOwnership[i] = PARCEL_PUBLIC;
		}
	}

	resetCollisionBitmap();

	gPipeline.markGLRebuild(this);
	LL_DEBUGS("MarkGLRebuild") << "Marked for GL rebuild: " << std::hex
							   << (intptr_t)this << std::dec << LL_ENDL;
}

LLViewerParcelOverlay::~LLViewerParcelOverlay()
{
	delete[] mOwnership;
	mOwnership = NULL;

	delete[] mVertexArray;
	mVertexArray = NULL;

	delete[] mColorArray;
	mColorArray = NULL;

	mImageRaw = NULL;
}

bool LLViewerParcelOverlay::isOwned(const LLVector3& pos) const
{
	return ownership(pos.mV[VY] * STEP_FACTOR,
					 pos.mV[VX] * STEP_FACTOR) != PARCEL_PUBLIC;
}

bool LLViewerParcelOverlay::isOwnedSelf(const LLVector3& pos) const
{
	return ownership(pos.mV[VY] * STEP_FACTOR,
					 pos.mV[VX] * STEP_FACTOR) == PARCEL_SELF;
}

bool LLViewerParcelOverlay::isOwnedGroup(const LLVector3& pos) const
{
	return ownership(pos.mV[VY] * STEP_FACTOR,
					 pos.mV[VX] * STEP_FACTOR) == PARCEL_GROUP;
}

bool LLViewerParcelOverlay::isOwnedOther(const LLVector3& pos) const
{
	U8 overlay = ownership(pos.mV[VY] * STEP_FACTOR, pos.mV[VX] * STEP_FACTOR);
	return overlay == PARCEL_OWNED || overlay == PARCEL_FOR_SALE;
}

bool LLViewerParcelOverlay::encroachesOwned(const std::vector<LLBBox>& boxes) const
{
	// Boxes are expected to already be axis aligned
	for (U32 i = 0; i < boxes.size(); ++i)
	{
		LLVector3 min = boxes[i].getMinAgent();
		LLVector3 max = boxes[i].getMaxAgent();

		S32 left = S32(llclamp(min.mV[VX] * STEP_FACTOR, 0.f, MAX_COORD));
		S32 right = S32(llclamp(max.mV[VX] * STEP_FACTOR, 0.f, MAX_COORD));
		S32 top = S32(llclamp(min.mV[VY] * STEP_FACTOR, 0.f, MAX_COORD));
		S32 bottom = S32(llclamp(max.mV[VY] * STEP_FACTOR, 0.f, MAX_COORD));

		for (S32 row = top; row <= bottom; ++row)
		{
			for (S32 column = left; column <= right; ++column)
			{
				U8 type = ownership(row, column);
				if (PARCEL_SELF == type || PARCEL_GROUP == type)
				{
					return true;
				}
			}
		}
	}
	return false;
}

bool LLViewerParcelOverlay::encroachesOnUnowned(const std::vector<LLBBox>& boxes) const
{
	// Boxes are expected to already be axis aligned
	for (U32 i = 0; i < boxes.size(); ++i)
	{
		LLVector3 min = boxes[i].getMinAgent();
		LLVector3 max = boxes[i].getMaxAgent();

		S32 left = S32(llclamp((min.mV[VX] * STEP_FACTOR), 0.f, MAX_COORD));
		S32 right = S32(llclamp((max.mV[VX] * STEP_FACTOR), 0.f, MAX_COORD));
		S32 top = S32(llclamp((min.mV[VY] * STEP_FACTOR), 0.f, MAX_COORD));
		S32 bottom = S32(llclamp((max.mV[VY] * STEP_FACTOR), 0.f, MAX_COORD));

		for (S32 row = top; row <= bottom; ++row)
		{
			for (S32 column = left; column <= right; ++column)
			{
				U8 type = ownership(row, column);
				if (PARCEL_SELF != type)
				{
					return true;
				}
			}
		}
	}
	return false;
}

bool LLViewerParcelOverlay::encroachesOnNearbyParcel(const std::vector<LLBBox>& boxes) const
{
	// Boxes are expected to already be axis aligned
	for (U32 i = 0; i < boxes.size(); ++i)
	{
		LLVector3 min = boxes[i].getMinAgent();
		LLVector3 max = boxes[i].getMaxAgent();

		// If an object crosses region borders it crosses a parcel
		if (min.mV[VX] < 0 || min.mV[VY] < 0 ||
			max.mV[VX] > REGION_WIDTH_METERS ||
			max.mV[VY] > REGION_WIDTH_METERS)
		{
			return true;
		}

		S32 left = S32(llclamp((min.mV[VX] * STEP_FACTOR), 0.f, MAX_COORD));
		S32 right = S32(llclamp((max.mV[VX] * STEP_FACTOR), 0.f, MAX_COORD));
		S32 bottom = S32(llclamp((min.mV[VY] * STEP_FACTOR), 0.f, MAX_COORD));
		S32 top = S32(llclamp((max.mV[VY] * STEP_FACTOR), 0.f, MAX_COORD));

		const S32 GRIDS_PER_EDGE = mParcelGridsPerEdge;

		for (S32 row = bottom; row <= top; ++row)
		{
			for (S32 col = left; col <= right; ++col)
			{
				// This is not the rightmost column
				if (col < GRIDS_PER_EDGE - 1)
				{
					U8 east_overlay =
						mOwnership ? mOwnership[row * GRIDS_PER_EDGE + col + 1]
								   : PARCEL_PUBLIC;
					// If the column to the east of the current one marks
					// the other parcel's west edge and the box extends
					// to the west it crosses the parcel border.
					if ((east_overlay & PARCEL_WEST_LINE) && col < right)
					{
						return true;
					}
				}

				// This is not the topmost column
				if (row < GRIDS_PER_EDGE - 1)
				{
					U8 north_overlay =
						mOwnership ? mOwnership[(row + 1) * GRIDS_PER_EDGE + col]
								   : PARCEL_PUBLIC;
					// If the row to the north of the current one marks
					// the other parcel's south edge and the box extends
					// to the south it crosses the parcel border.
					if ((north_overlay & PARCEL_SOUTH_LINE) && row < top)
					{
						return true;
					}
				}
			}
		}
	}
	return false;
}

U8 LLViewerParcelOverlay::ownership(const LLVector3& pos) const
{
	return ownership(pos.mV[VY] * STEP_FACTOR, pos.mV[VX] * STEP_FACTOR);
}

U8 LLViewerParcelOverlay::parcelFlags(S32 row, S32 col, U8 mask) const
{
	if (!mOwnership)
	{
		LL_DEBUGS_ONCE("ParcelOverlay") << "No ownership data for overlay "
										<< std::hex << (intptr_t)this
										<< std::dec << llendl;
		return mask;
	}
	if (row < 0 || col < 0 || row >= mParcelGridsPerEdge ||
		col >= mParcelGridsPerEdge)
	{
		LL_DEBUGS_ONCE("ParcelOverlay") << "Out of range coordinates for overlay "
										<< std::hex << (intptr_t)this
										<< std::dec << " - row: " << row
										<< " - col: " << col << llendl;
		return mask;
	}
	return mOwnership[row * mParcelGridsPerEdge + col] & mask;
}

U8 LLViewerParcelOverlay::parcelLineFlags(S32 row, S32 col) const
{
	constexpr U8 mask = PARCEL_WEST_LINE | PARCEL_SOUTH_LINE;
	return parcelFlags(row, col, mask);
}

bool LLViewerParcelOverlay::isSoundLocal(const LLVector3& pos) const
{
	return parcelFlags(S32(pos.mV[VY] * STEP_FACTOR),
					   S32(pos.mV[VX] * STEP_FACTOR), PARCEL_SOUND_LOCAL);
}

F32 LLViewerParcelOverlay::getOwnedRatio() const
{
	S32	size = mParcelGridsPerEdge * mParcelGridsPerEdge;
	S32 total = 0;

	if (mOwnership)
	{
		for (S32 i = 0; i < size; ++i)
		{
			if ((mOwnership[i] & PARCEL_COLOR_MASK) != PARCEL_PUBLIC)
			{
				++total;
			}
		}
	}

	return (F32)total / (F32)size;
}

// Color tables for owned land
// Available = index 0
// Other     = index 1
// Group     = index 2
// Self      = index 3

// Make sure the texture colors match the ownership data.
void LLViewerParcelOverlay::updateOverlayTexture()
{
	if (mOverlayTextureIdx < 0 && mDirty)
	{
		mOverlayTextureIdx = 0;
	}
	if (mOverlayTextureIdx < 0)
	{
		return;
	}

	U8* raw = mImageRaw->getData();
	if (!raw || !mTexture)
	{
		return;
	}

	// Can do this because gColors are actually stored as LLColor4U
	static LLCachedControl<LLColor4U> color_avail(gColors,
												  "PropertyColorAvail");
	static LLCachedControl<LLColor4U> color_other(gColors,
												  "PropertyColorOther");
	static LLCachedControl<LLColor4U> color_group(gColors,
												  "PropertyColorGroup");
	static LLCachedControl<LLColor4U> color_self(gColors,
												 "PropertyColorSelf");
	static LLCachedControl<LLColor4U> color_for_sale(gColors,
													 "PropertyColorForSale");
	static LLCachedControl<LLColor4U> color_auction(gColors,
													"PropertyColorAuction");

	// Create the base texture.
	LLColor4U color;
	S32 count = mParcelGridsPerEdge * mParcelGridsPerEdge;
	S32 max = mOverlayTextureIdx + mParcelGridsPerEdge;
	if (max > count)
	{
		max = count;
	}
	S32 pixel_index = mOverlayTextureIdx * OVERLAY_IMG_COMPONENTS;
	S32 i;
	for (i = mOverlayTextureIdx; i < max; ++i)
	{
		U8 ownership = mOwnership ? mOwnership[i] : PARCEL_PUBLIC;

		// Color stored in low three bits
		switch (ownership & 0x7)
		{
			case PARCEL_PUBLIC:
				color = color_avail;
				break;
			case PARCEL_OWNED:
				color = color_other;
				break;
			case PARCEL_GROUP:
				color = color_group;
				break;
			case PARCEL_FOR_SALE:
				color = color_for_sale;
				break;
			case PARCEL_AUCTION:
				color = color_auction;
				break;
			case PARCEL_SELF:
			default:
				color = color_self;
				break;
		}

		raw[pixel_index] = color.mV[VRED];
		raw[pixel_index + 1] = color.mV[VGREEN];
		raw[pixel_index + 2] = color.mV[VBLUE];
		raw[pixel_index + 3] = color.mV[VALPHA];

		pixel_index += OVERLAY_IMG_COMPONENTS;
	}

	// Copy data into GL texture from raw data
	if (i >= count)
	{
		if (!mTexture->hasGLTexture())
		{
			mTexture->createGLTexture(0, mImageRaw);
		}
		mTexture->setSubImage(mImageRaw, 0, 0, mParcelGridsPerEdge,
							  mParcelGridsPerEdge);
		mOverlayTextureIdx = -1;
	}
	else
	{
		mOverlayTextureIdx = i;
	}
}

void LLViewerParcelOverlay::uncompressLandOverlay(S32 chunk,
												  U8* packed_overlay)
{
	// Unpack the message data into the ownership array
	S32	size = mParcelGridsPerEdge * mParcelGridsPerEdge;

	// Variable region size support
	S32 parcel_overlay_chunks = mRegionSize * mRegionSize / (128 * 128);
	S32 chunk_size = size / parcel_overlay_chunks;

	if (mOwnership)
	{
		memcpy(mOwnership + chunk * chunk_size, packed_overlay, chunk_size);
	}

	// Force property lines and overlay texture to update
	setDirty();
}

void LLViewerParcelOverlay::updatePropertyLines()
{
	static LLCachedControl<bool> show_property_lines(gSavedSettings,
													 "ShowPropertyLines");
	static LLCachedControl<bool> show_parcels(gSavedSettings,
											  "MinimapShowParcelBorders");
	if (!show_property_lines && !show_parcels)
	{
		return;
	}

	// Can do this because gColors are actually stored as LLColor4U
	static LLCachedControl<LLColor4U> self_coloru(gColors,
												  "PropertyColorSelf");
	static LLCachedControl<LLColor4U> other_coloru(gColors,
												   "PropertyColorOther");
	static LLCachedControl<LLColor4U> group_coloru(gColors,
												   "PropertyColorGroup");
	static LLCachedControl<LLColor4U> for_sale_coloru(gColors,
													  "PropertyColorForSale");
	static LLCachedControl<LLColor4U> auction_coloru(gColors,
													 "PropertyColorAuction");

	LLColor4U color;

	// Build into vectors, then copy into static arrays.
	std::vector<LLVector3> new_vertex_array;
	new_vertex_array.reserve(256);
	std::vector<LLColor4U> new_color_array;
	new_color_array.reserve(256);
	std::vector<LLVector2> new_coord_array;
	new_coord_array.reserve(256);

	U8 overlay = 0;
	bool add_edge = false;
	constexpr F32 GRID_STEP = PARCEL_GRID_STEP_METERS;
	const S32 GRIDS_PER_EDGE = mParcelGridsPerEdge;

	for (S32 row = 0; row < GRIDS_PER_EDGE; ++row)
	{
		for (S32 col = 0; col < GRIDS_PER_EDGE; ++col)
		{
			overlay = mOwnership ? mOwnership[row * GRIDS_PER_EDGE + col]
								 : PARCEL_PUBLIC;

			switch (overlay & PARCEL_COLOR_MASK)
			{
				case PARCEL_SELF:
					color = self_coloru;
					break;
				case PARCEL_GROUP:
					color = group_coloru;
					break;
				case PARCEL_OWNED:
					color = other_coloru;
					break;
				case PARCEL_FOR_SALE:
					color = for_sale_coloru;
					break;
				case PARCEL_AUCTION:
					color = auction_coloru;
					break;
				default:
					continue;
			}

			F32 left = col * GRID_STEP;
			F32 right = left + GRID_STEP;

			F32 bottom = row * GRID_STEP;
			F32 top = bottom + GRID_STEP;

			// West edge
			if (overlay & PARCEL_WEST_LINE)
			{
				addPropertyLine(new_vertex_array, new_color_array,
								new_coord_array, left, bottom, WEST, color);
			}

			// East edge
			if (col < GRIDS_PER_EDGE - 1)
			{
				U8 east_overlay =
					mOwnership ? mOwnership[row * GRIDS_PER_EDGE + col + 1]
							   : PARCEL_PUBLIC;
				add_edge = (east_overlay & PARCEL_WEST_LINE) != 0;
			}
			else
			{
				add_edge = true;
			}
			if (add_edge)
			{
				addPropertyLine(new_vertex_array, new_color_array,
								new_coord_array, right, bottom, EAST, color);
			}

			// South edge
			if (overlay & PARCEL_SOUTH_LINE)
			{
				addPropertyLine(new_vertex_array, new_color_array,
								new_coord_array, left, bottom, SOUTH, color);
			}

			// North edge
			if (row < GRIDS_PER_EDGE - 1)
			{
				U8 north_overlay =
					mOwnership ? mOwnership[(row + 1) * GRIDS_PER_EDGE + col]
							   : PARCEL_PUBLIC;
				add_edge = (north_overlay & PARCEL_SOUTH_LINE) != 0;
			}
			else
			{
				add_edge = true;
			}

			if (add_edge)
			{
				addPropertyLine(new_vertex_array, new_color_array,
								new_coord_array, left, top, NORTH, color);
			}
		}
	}

	// Now copy into static arrays for faster rendering.
	// Attempt to recycle old arrays if possible to avoid memory shuffling.
	S32 new_vertex_count = new_vertex_array.size();

	if (!(mVertexArray && mColorArray && new_vertex_count == mVertexCount))
	{
		// ...need new arrays
		delete[] mVertexArray;
		mVertexArray = NULL;
		delete[] mColorArray;
		mColorArray = NULL;

		mVertexCount = new_vertex_count;

		if (new_vertex_count > 0)
		{
			mVertexArray = new F32[3 * mVertexCount];
			mColorArray = new U8[4 * mVertexCount];
		}
	}

	// Copy the new data into the arrays
	F32* vertex = mVertexArray;
	for (S32 i = 0; i < mVertexCount; ++i)
	{
		const LLVector3& point = new_vertex_array[i];
		*vertex++ = point.mV[VX];
		*vertex++ = point.mV[VY];
		*vertex++ = point.mV[VZ];
	}

	U8* colorp = mColorArray;
	for (S32 i = 0; i < mVertexCount; ++i)
	{
		const LLColor4U& color = new_color_array[i];
		*colorp++ = color.mV[VRED];
		*colorp++ = color.mV[VGREEN];
		*colorp++ = color.mV[VBLUE];
		*colorp++ = color.mV[VALPHA];
	}

	// Everything is clean now
	mDirty = false;
}

void LLViewerParcelOverlay::addPropertyLine(std::vector<LLVector3>& vertex_array,
											std::vector<LLColor4U>& color_array,
											std::vector<LLVector2>& coord_array,
											F32 start_x, F32 start_y, U32 edge,
											const LLColor4U& color)
{
	static LLCachedControl<bool> at_surface(gSavedSettings,
											"ShowPropLinesAtWaterSurface");

	LLColor4U underwater(color);
	if (!at_surface)
	{
		underwater.mV[VALPHA] *= 0.5f;
	}

	vertex_array.reserve(16);
	color_array.reserve(16);
	coord_array.reserve(16);

	LLSurface& land = mRegion->getLand();
	F32 water_height = mRegion->getWaterHeight();

	F32 dx;
	F32 dy;
	F32 tick_dx;
	F32 tick_dy;
	constexpr F32 LINE_WIDTH = 0.0625f;

	switch(edge)
	{
	case WEST:
		dx = 0.f;
		dy = 1.f;
		tick_dx = LINE_WIDTH;
		tick_dy = 0.f;
		break;

	case EAST:
		dx = 0.f;
		dy = 1.f;
		tick_dx = -LINE_WIDTH;
		tick_dy = 0.f;
		break;

	case NORTH:
		dx = 1.f;
		dy = 0.f;
		tick_dx = 0.f;
		tick_dy = -LINE_WIDTH;
		break;

	case SOUTH:
		dx = 1.f;
		dy = 0.f;
		tick_dx = 0.f;
		tick_dy = LINE_WIDTH;
		break;

	default:
		llerrs << "Invalid edge in addPropertyLine" << llendl;
		return;
	}

	F32 outside_x = start_x;
	F32 outside_y = start_y;
	F32 outside_z = 0.f;
	F32 inside_x  = start_x + tick_dx;
	F32 inside_y  = start_y + tick_dy;
	F32 inside_z  = 0.f;

	// First part, only one vertex
	outside_z = land.resolveHeightRegion(outside_x, outside_y);

	if (outside_z > water_height)
	{
		color_array.emplace_back(color);
	}
	else
	{
		color_array.emplace_back(underwater);
		if (at_surface)
		{
			outside_z = water_height;
		}
	}

	vertex_array.emplace_back(outside_x, outside_y, outside_z);
	coord_array.emplace_back(outside_x - start_x, 0.f);

	inside_x += dx * LINE_WIDTH;
	inside_y += dy * LINE_WIDTH;

	outside_x += dx * LINE_WIDTH;
	outside_y += dy * LINE_WIDTH;

	// Then the "actual edge"
	inside_z = land.resolveHeightRegion(inside_x, inside_y);
	outside_z = land.resolveHeightRegion(outside_x, outside_y);

	if (inside_z > water_height)
	{
		color_array.emplace_back(color);
	}
	else
	{
		color_array.emplace_back(underwater);
		if (at_surface)
		{
			inside_z = water_height;
		}
	}

	if (outside_z > water_height)
	{
		color_array.emplace_back(color);
	}
	else
	{
		color_array.emplace_back(underwater);
		if (at_surface)
		{
			outside_z = water_height;
		}
	}

	vertex_array.emplace_back(inside_x, inside_y, inside_z);
	vertex_array.emplace_back(outside_x, outside_y, outside_z);

	coord_array.emplace_back(outside_x - start_x, 1.f);
	coord_array.emplace_back(outside_x - start_x, 0.f);

	inside_x += dx * (dx - LINE_WIDTH);
	inside_y += dy * (dy - LINE_WIDTH);

	outside_x += dx * (dx - LINE_WIDTH);
	outside_y += dy * (dy - LINE_WIDTH);

	// Middle part, full width
	S32 i;
	constexpr S32 GRID_STEP = S32(PARCEL_GRID_STEP_METERS);
	for (i = 1; i < GRID_STEP; i++)
	{
		inside_z = land.resolveHeightRegion(inside_x, inside_y);
		outside_z = land.resolveHeightRegion(outside_x, outside_y);

		if (inside_z > water_height)
		{
			color_array.emplace_back(color);
		}
		else
		{
			color_array.emplace_back(underwater);
			if (at_surface)
			{
				inside_z = water_height;
			}
		}

		if (outside_z > water_height)
		{
			color_array.emplace_back(color);
		}
		else
		{
			color_array.emplace_back(underwater);
			if (at_surface)
			{
				outside_z = water_height;
			}
		}

		vertex_array.emplace_back(inside_x, inside_y, inside_z);
		vertex_array.emplace_back(outside_x, outside_y, outside_z);

		coord_array.emplace_back(outside_x - start_x, 1.f);
		coord_array.emplace_back(outside_x - start_x, 0.f);

		inside_x += dx;
		inside_y += dy;

		outside_x += dx;
		outside_y += dy;
	}

	// Extra buffer for edge
	inside_x -= dx * LINE_WIDTH;
	inside_y -= dy * LINE_WIDTH;

	outside_x -= dx * LINE_WIDTH;
	outside_y -= dy * LINE_WIDTH;

	inside_z = land.resolveHeightRegion(inside_x, inside_y);
	outside_z = land.resolveHeightRegion(outside_x, outside_y);

	if (inside_z > water_height)
	{
		color_array.emplace_back(color);
	}
	else
	{
		color_array.emplace_back(underwater);
		if (at_surface)
		{
			inside_z = water_height;
		}
	}

	if (outside_z > water_height)
	{
		color_array.emplace_back(color);
	}
	else
	{
		color_array.emplace_back(underwater);
		if (at_surface)
		{
			outside_z = water_height;
		}
	}

	vertex_array.emplace_back(inside_x, inside_y, inside_z);
	vertex_array.emplace_back(outside_x, outside_y, outside_z);

	coord_array.emplace_back(outside_x - start_x, 1.f);
	coord_array.emplace_back(outside_x - start_x, 0.f);

	inside_x += dx * LINE_WIDTH;
	inside_y += dy * LINE_WIDTH;

	outside_x += dx * LINE_WIDTH;
	outside_y += dy * LINE_WIDTH;

	// Last edge is not drawn to the edge
	outside_z = land.resolveHeightRegion(outside_x, outside_y);

	if (outside_z > water_height)
	{
		color_array.emplace_back(color);
		if (at_surface)
		{
			inside_z = water_height;
		}
	}
	else
	{
		color_array.emplace_back(underwater);
		if (at_surface)
		{
			outside_z = water_height;
		}
	}

	vertex_array.emplace_back(outside_x, outside_y, outside_z);
	coord_array.emplace_back(outside_x - start_x, 0.f);
}

void LLViewerParcelOverlay::idleUpdate(bool force_update)
{
	if (gGLManager.mIsDisabled)
	{
		return;
	}
	if (mOverlayTextureIdx >= 0 && !(mDirty && force_update))
	{
		// We are in the middle of updating the overlay texture
		gPipeline.markGLRebuild(this);
		LL_DEBUGS("MarkGLRebuild") << "Marked for GL rebuild: " << std::hex
								   << (intptr_t)this << std::dec << LL_ENDL;
		return;
	}
	// Only if we are dirty and it has been a while since the last update.
	if (mDirty)
	{
		if (force_update || mTimeSinceLastUpdate.getElapsedTimeF32() > 4.f)
		{
			updateOverlayTexture();
			updatePropertyLines();
			mTimeSinceLastUpdate.reset();
		}
	}
}

void LLViewerParcelOverlay::resetCollisionBitmap()
{
	mHasCollisions = false;
	mCollisionBitmap.clear();
	mCollisionBitmap.resize(mParcelGridsPerEdge * mParcelGridsPerEdge, 0);
}

void LLViewerParcelOverlay::readCollisionBitmap(U8* bitmap)
{
	for (S32 y = 0; y < mParcelGridsPerEdge; ++y)
	{
		S32 line_offset = y * mParcelGridsPerEdge;
		S32 x = 0;
		while (x < mParcelGridsPerEdge)
		{
			U8 byte = bitmap[(x + line_offset) / 8];
			U8 mask = 1;
			for (S32 bit = 0; bit < 8; ++bit)
			{
				if (byte & mask)
				{
					mCollisionBitmap[x + line_offset] = true;
					mHasCollisions = true;
				}
				mask <<= 1;
				++x;
			}
		}
	}
}

void LLViewerParcelOverlay::renderPropertyLines() const
{
	if (!mVertexArray || !mColorArray)
	{
		return;
	}

	LLSurface& land = mRegion->getLand();

	LLGLSUIDefault gls_ui; // called from pipeline
	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
	LLGLDepthTest mDepthTest(GL_TRUE);

	// Find camera height off the ground (not from zero)
	F32 ground_height_at_camera =
		land.resolveHeightGlobal(gAgent.getCameraPositionGlobal());
	F32 camera_z = gViewerCamera.getOrigin().mV[VZ];
	F32 camera_height = camera_z - ground_height_at_camera;

	camera_height = llclamp(camera_height, 0.f, 100.f);

	// Pull lines toward camera by 1 cm per meter off the ground.
	const LLVector3& CAMERA_AT = gViewerCamera.getAtAxis();
	F32 pull_toward_camera_scale = 0.01f * camera_height;
	LLVector3 pull_toward_camera = CAMERA_AT;
	pull_toward_camera *= -pull_toward_camera_scale;

	// Always fudge a little vertically.
	pull_toward_camera.mV[VZ] += 0.01f;

	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.pushMatrix();

	// Move to appropriate region coords
	LLVector3 origin = mRegion->getOriginAgent();
	gGL.translatef(origin.mV[VX], origin.mV[VY], origin.mV[VZ]);

	gGL.translatef(pull_toward_camera.mV[VX], pull_toward_camera.mV[VY],
				   pull_toward_camera.mV[VZ]);

	// Include +1 because vertices are fenceposts.
	// *2 because it is a quad strip
	constexpr S32 GRID_STEP = S32(PARCEL_GRID_STEP_METERS);
	constexpr S32 vertex_per_edge = 3 + 2 * (GRID_STEP - 1) + 3;

	// Stomp the camera into two dimensions
	LLVector3 camera_region =
		mRegion->getPosRegionFromGlobal(gAgent.getCameraPositionGlobal());

	// Set up a cull plane 2 * PARCEL_GRID_STEP_METERS behind
	// the camera.  The cull plane normal is the camera's at axis.
	LLVector3 cull_plane_point = gViewerCamera.getAtAxis();
	cull_plane_point *= -2.f * PARCEL_GRID_STEP_METERS;
	cull_plane_point += camera_region;

	LLVector3 vertex;

	constexpr S32 BYTES_PER_COLOR = 4;
	constexpr S32 FLOATS_PER_VERTEX = 3;
	constexpr F32 PROPERTY_LINE_CLIP_DIST = 256.f;

	for (S32 i = 0; i < mVertexCount; i += vertex_per_edge)
	{
		U8* colorp = mColorArray + BYTES_PER_COLOR * i;
		F32* vertexp = mVertexArray + FLOATS_PER_VERTEX * i;

		vertex.mV[VX] = *(vertexp);
		vertex.mV[VY] = *(vertexp + 1);
		vertex.mV[VZ] = *(vertexp + 2);

		if (dist_vec_squared2D(vertex, camera_region) >
				PROPERTY_LINE_CLIP_DIST * PROPERTY_LINE_CLIP_DIST)
		{
			continue;
		}

		// Destroy vertex, transform to plane-local.
		vertex -= cull_plane_point;

		// Negative dot product means it is in back of the plane
		if (vertex * CAMERA_AT < 0.f)
		{
			continue;
		}

		gGL.begin(LLRender::TRIANGLE_STRIP);

		for (S32 j = 0; j < vertex_per_edge; ++j)
		{
			gGL.color4ubv(colorp);
			gGL.vertex3fv(vertexp);

			colorp  += BYTES_PER_COLOR;
			vertexp += FLOATS_PER_VERTEX;
		}

		gGL.end();

		if (LLSelectMgr::renderHiddenSelection() &&
			LLFloaterTools::isVisible())
		{
			LLGLDepthTest depth(GL_TRUE, GL_FALSE, GL_GREATER);

			colorp = mColorArray + BYTES_PER_COLOR * i;
			vertexp = mVertexArray + FLOATS_PER_VERTEX * i;

			gGL.begin(LLRender::TRIANGLE_STRIP);

			for (S32 j = 0; j < vertex_per_edge; ++j)
			{
				U8 color[4];
				color[0] = colorp[0];
				color[1] = colorp[1];
				color[2] = colorp[2];
				color[3] = colorp[3] / 4;

				gGL.color4ubv(color);
				gGL.vertex3fv(vertexp);

				colorp += BYTES_PER_COLOR;
				vertexp += FLOATS_PER_VERTEX;
			}

			gGL.end();
		}
	}

	gGL.popMatrix();

	stop_glerror();
}

void LLViewerParcelOverlay::renderParcelBorders(F32 scale,
												const F32* color) const
{
	LLVector3 origin_agent = mRegion->getOriginAgent();
	LLVector3 rel_region_pos = origin_agent - gAgent.getCameraPositionAgent();
	F32 region_left = rel_region_pos.mV[0] * scale;
	F32 region_bottom = rel_region_pos.mV[1] * scale;
	F32 map_parcel_width = PARCEL_GRID_STEP_METERS * scale;

	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
	gGL.lineWidth(1.f);
	gGL.color4fv(color);

	gGL.begin(LLRender::LINES);
	F32 bottom = region_bottom;
	S32 line_offset = 0;
	for (S32 i = 0; i <= mParcelGridsPerEdge; ++i)
	{
		F32 left = region_left;
		U8 overlay;
		for (S32 j = 0; j <= mParcelGridsPerEdge; ++j)
		{
			bool south_limit = i == mParcelGridsPerEdge;
			bool west_limit = j == mParcelGridsPerEdge;
			if (south_limit || west_limit)
			{
				// This is the region boundary (out of mOwnership limits).
				overlay = 0;
			}
			else
			{
				overlay = mOwnership[line_offset + j];
			}
			// The property line vertices are three-dimensional, but here we
			// only care about the x and y coordinates, as we are drawing on a
			// 2D map.
			if (!south_limit && (west_limit || (overlay & PARCEL_WEST_LINE)))
			{
				// We have a left border: draw it
				gGL.vertex2f(left, bottom);
				gGL.vertex2f(left, bottom + map_parcel_width);
			}
			if (!west_limit && (south_limit || (overlay & PARCEL_SOUTH_LINE)))
			{
				// We have a bottom border: draw it
				gGL.vertex2f(left, bottom);
				gGL.vertex2f(left + map_parcel_width, bottom);
			}
			left += map_parcel_width;
		}
		bottom += map_parcel_width;
		line_offset += mParcelGridsPerEdge;
	}
	gGL.end();
}

bool LLViewerParcelOverlay::renderBannedParcels(F32 scale,
												const F32* color) const
{
	if (!mHasCollisions)
	{
		// Nothing to render (no banned parcel info received so far). HB
		return false;
	}

	LLVector3 origin_agent = mRegion->getOriginAgent();
	LLVector3 rel_region_pos = origin_agent - gAgent.getCameraPositionAgent();
	F32 region_left = rel_region_pos.mV[0] * scale;
	F32 region_bottom = rel_region_pos.mV[1] * scale;
	F32 map_parcel_width = PARCEL_GRID_STEP_METERS * scale;

	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
	gGL.color4fv(color);

	gGL.begin(LLRender::TRIANGLES);
	for (S32 y = 0; y < mParcelGridsPerEdge; ++y)
	{
		S32 line_offset = y * map_parcel_width;
		for (S32 x = 0; x < mParcelGridsPerEdge; ++x)
		{
			if (mCollisionBitmap[x + y * mParcelGridsPerEdge])
			{
				F32 left = region_left + x * map_parcel_width;
				F32 bottom = region_bottom + line_offset;
				F32 right = left + map_parcel_width;
				F32 top = bottom + map_parcel_width;
				gGL.vertex2f(left, top);
				gGL.vertex2f(left, bottom);
				gGL.vertex2f(right, top);
				gGL.vertex2f(right, top);
				gGL.vertex2f(left, bottom);
				gGL.vertex2f(right, bottom);
			}
		}
	}
	gGL.end();

	return true;
}
