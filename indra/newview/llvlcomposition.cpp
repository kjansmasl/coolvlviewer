/**
 * @file llvlcomposition.cpp
 * @brief Viewer-side representation of a composition layer...
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

#include "llviewerprecompiledheaders.h"

#include "llvlcomposition.h"

#include "imageids.h"
#include "llnoise.h"
#include "llregionhandle.h"			// For from_region_handle()

#include "llsurface.h"
#include "llviewercontrol.h"
#include "llviewerregion.h"
#include "llviewertexturelist.h"

constexpr S32 BASE_SIZE = 128;

// Not sure if this is the right math... Takes the weighted average of all four
// points (bilinear interpolation)
static F32 bilinear(F32 v00, F32 v01, F32 v10, F32 v11, F32 x_frac, F32 y_frac)
{
	F32 inv_x_frac = 1.f - x_frac;
	F32 inv_y_frac = 1.f - y_frac;
	return inv_x_frac * inv_y_frac * v00 + x_frac * inv_y_frac * v10 +
		   inv_x_frac * y_frac * v01 + x_frac * y_frac * v11;
}

///////////////////////////////////////////////////////////////////////////////
// LLViewerLayer class
///////////////////////////////////////////////////////////////////////////////

LLViewerLayer::LLViewerLayer(S32 width, F32 scale)
:	mWidth(width),
	mScale(scale),
	mScaleInv(1.f / scale)
{
	mDatap = new F32[width * width];
	for (S32 i = 0; i < width * width; ++i)
	{
		*(mDatap + i) = 0.f;
	}
}

LLViewerLayer::~LLViewerLayer()
{
	delete[] mDatap;
	mDatap = NULL;
}

F32 LLViewerLayer::getValueScaled(F32 x, F32 y) const
{
	F32 x_frac = x * mScaleInv;
	S32 x1 = llfloor(x_frac);
	S32 x2 = x1 + 1;
	x_frac -= x1;

	F32 y_frac = y * mScaleInv;
	S32 y1 = llfloor(y_frac);
	S32 y2 = y1 + 1;
	y_frac -= y1;

	S32 max = mWidth - 1;
	x1 = llclamp(x1, 0, max);
	x2 = llclamp(x2, 0, max);
	y1 = llclamp(y1, 0, max);
	y2 = llclamp(y2, 0, max);

	// Take weighted average of all four points (bilinear interpolation)
	S32 row1 = y1 * mWidth;
	S32 row2 = y2 * mWidth;

	// Access in squential order in memory, and do not use immediately.
	F32 row1_left  = mDatap[row1 + x1];
	F32 row1_right = mDatap[row1 + x2];
	F32 row2_left  = mDatap[row2 + x1];
	F32 row2_right = mDatap[row2 + x2];

	F32 row1_interp = row1_left - x_frac * (row1_left - row1_right);
	F32 row2_interp = row2_left - x_frac * (row2_left - row2_right);

	return row1_interp - y_frac * (row1_interp - row2_interp);
}

///////////////////////////////////////////////////////////////////////////////
// LLVLComposition class
///////////////////////////////////////////////////////////////////////////////

LLVLComposition::LLVLComposition(LLSurface* surfacep, U32 width, F32 scale)
:	LLViewerLayer(width, scale),
	mSurfacep(surfacep),
	mTexScaleX(16.f),
	mTexScaleY(16.f),
	mTexturesLoaded(false),
	mParamsReady(false)
{
	// Load terrain textures - Original ones
	setDetailTextureID(TERRAIN_DIRT, TERRAIN_DIRT_DETAIL);
	setDetailTextureID(TERRAIN_GRASS, TERRAIN_GRASS_DETAIL);
	setDetailTextureID(TERRAIN_MOUNTAIN, TERRAIN_MOUNTAIN_DETAIL);
	setDetailTextureID(TERRAIN_ROCK, TERRAIN_ROCK_DETAIL);

	static LLCachedControl<F32> color_start_height(gSavedSettings,
												   "TerrainColorStartHeight");
	static LLCachedControl<F32> color_height_range(gSavedSettings,
												   "TerrainColorHeightRange");

	// Initialize the texture matrix to defaults.
	for (S32 i = 0; i < (S32)TERRAIN_COUNT; ++i)
	{
		mStartHeight[i] = color_start_height;
		mHeightRange[i] = color_height_range;
	}
}

void LLVLComposition::setDetailTextureID(S32 terrain, const LLUUID& id)
{
	if (id.isNull())
	{
		return;
	}

	mRawImages[terrain] = NULL;

	mDetailTextures[terrain] = LLViewerTextureManager::getFetchedTexture(id);
	// Non-loading mini-map textures fixes follow...
	// We need the maximum resolution (lowest discard level) to avoid partly
	// loaded textures that would never complete (probably a race condition
	// issue in the fetcher, with loading textures and changing the discard
	// level while they load): the textures will get appropriately discarded
	// anyway, once the composition will have been created from them. HB.
	mDetailTextures[terrain]->setMinDiscardLevel(0);
	// We also need to give this kind of textures the highest (and appropriate)
	// priority from the start... HB.
	mDetailTextures[terrain]->setBoostLevel(LLGLTexture::BOOST_TERRAIN);
#if !LL_IMPLICIT_SETNODELETE
	mDetailTextures[terrain]->setNoDelete();
#endif
}

void LLVLComposition::forceRebuild()
{
	for (S32 i = 0; i < (S32)TERRAIN_COUNT; ++i)
	{
		LLViewerFetchedTexture* texp = mDetailTextures[i];
		if (texp)
		{
			texp->forceRefetch();
		}
	}
}

bool LLVLComposition::generateHeights(F32 x, F32 y, F32 width, F32 height)
{
	if (!mParamsReady)
	{
		// All the parameters have not been set yet (we did not get the message
		// from the sim)
		return false;
	}

	llassert(mSurfacep);

	if (!mSurfacep || !mSurfacep->getRegion())
	{
		// We do not always have the region yet here....
		return false;
	}

	S32 x_begin, y_begin, x_end, y_end;

	x_begin = ll_round(x * mScaleInv);
	y_begin = ll_round(y * mScaleInv);
	x_end = ll_round((x + width) * mScaleInv);
	y_end = ll_round((y + width) * mScaleInv);

	if (x_end > mWidth)
	{
		x_end = mWidth;
	}
	if (y_end > mWidth)
	{
		y_end = mWidth;
	}

	LLVector3d origin_global =
		from_region_handle(mSurfacep->getRegion()->getHandle());

	// For perlin noise generation...
	constexpr F32 slope_squared = 1.5f * 1.5f;

	// Degree to which noise modulates composition layer (versus simple height)
	constexpr F32 noise_magnitude = 2.f;

	// Heights map into textures as 0-1 = first, 1-2 = second, etc.
	// So we need to compress heights into this range.
	constexpr S32 NUM_TEXTURES = 4;

	constexpr F32 xy_scale_inv = 1.f / 4.9215f;
	constexpr F32 z_scale_inv = 1.f / 4.f;
	const F32 inv_width = 1.f / (F32)mWidth;

	// OK, for now, just have the composition value equal the height at the
	// point
	for (S32 j = y_begin; j < y_end; ++j)
	{
		for (S32 i = x_begin; i < x_end; ++i)
		{

			F32 vec[3];
			F32 vec1[3];
			F32 twiddle;

			// Bilinearly interpolate the start height and height range of the
			// textures
			F32 start_height = bilinear(mStartHeight[TERRAIN_DIRT],
										mStartHeight[TERRAIN_GRASS],
										mStartHeight[TERRAIN_MOUNTAIN],
										mStartHeight[TERRAIN_ROCK],
										// These will be bilinearly interpolated
										i * inv_width, j * inv_width);
			F32 height_range = bilinear(mHeightRange[TERRAIN_DIRT],
										mHeightRange[TERRAIN_GRASS],
										mHeightRange[TERRAIN_MOUNTAIN],
										mHeightRange[TERRAIN_ROCK],
										// These will be bilinearly interpolated
										i * inv_width, j * inv_width);

			LLVector3 location(i * mScale, j * mScale, 0.f);

			F32 height = mSurfacep->resolveHeightRegion(location);

			// Step 0: Measure the exact height at this texel
			//  Adjust to non-integer lattice
			vec[0] = (F32)(origin_global.mdV[VX] + location.mV[VX]) *
					 xy_scale_inv;
			vec[1] = (F32)(origin_global.mdV[VY] + location.mV[VY]) *
					 xy_scale_inv;
			vec[2] = height * z_scale_inv;

			// Choose material value by adding to the exact height a random
			// value

			vec1[0] = vec[0] * 0.2222222222f;
			vec1[1] = vec[1] * 0.2222222222f;
			vec1[2] = vec[2] * 0.2222222222f;

			// Low freq component for large divisions
			twiddle = noise2(vec1) * 6.5f;

			// High frequency component
			twiddle += turbulence2(vec, 2) * slope_squared;

			twiddle *= noise_magnitude;

			F32 scaled_noisy_height = (height + twiddle - start_height) *
									   F32(NUM_TEXTURES) / height_range;

			scaled_noisy_height = llmax(0.f, scaled_noisy_height);
			scaled_noisy_height = llmin(3.f, scaled_noisy_height);
			*(mDatap + i + j * mWidth) = scaled_noisy_height;
		}
	}
	return true;
}

bool LLVLComposition::detailTexturesReady()
{
	if (!mParamsReady)
	{
		// All the parameters have not been set yet (we did not get the message
		// from the sim)...
		return false;
	}

	for (S32 i = 0; i < (S32)TERRAIN_COUNT; ++i)
	{
		LLViewerFetchedTexture* tex = mDetailTextures[i];
		S32 discard = tex->getDiscardLevel();
		if (discard < 0)
		{
			tex->setBoostLevel(LLGLTexture::BOOST_TERRAIN);
			tex->addTextureStats(BASE_SIZE * BASE_SIZE);
			return false;
		}
		else if (discard != 0 &&
				 (tex->getWidth() < BASE_SIZE || tex->getHeight() < BASE_SIZE))
		{
			tex->setBoostLevel(LLGLTexture::BOOST_TERRAIN);

			S32 min_dim = llmin(tex->getFullWidth(), tex->getFullHeight());
			S32 ddiscard = 0;
			while (min_dim > BASE_SIZE && ddiscard < MAX_DISCARD_LEVEL)
			{
				++ddiscard;
				min_dim /= 2;
			}
			tex->setMinDiscardLevel(ddiscard);

			return false;
		}
	}

	return true;
}

bool LLVLComposition::generateTexture(F32 x, F32 y, F32 width, F32 height)
{
	if (!mParamsReady)
	{
		// All the parameters have not been set yet (we did not get the message
		// from the sim)...
		return false;
	}

	if (!mSurfacep || x < 0.f || y < 0.f)
	{
		llwarns << "Invalid surface: mSurfacep = "  << std::hex
				<< (intptr_t)mSurfacep << std::dec << " - x = "  << x
					<< " - y = " << y << llendl;
		llassert(false);
		return false;
	}

	LLTimer gen_timer;

	// Generate raw data arrays for surface textures

	U8* st_data[4];
	S32 st_data_size[4];
	for (S32 i = 0; i < (S32)TERRAIN_COUNT; ++i)
	{
		if (mRawImages[i].isNull())
		{
			LLViewerFetchedTexture* tex = mDetailTextures[i];
			// Compute the desired discard
			S32 width = tex->getFullWidth();
			S32 height = tex->getFullHeight();
			S32 min_dim = llmin(width, height);
			S32 ddiscard = 0;
			while (min_dim > BASE_SIZE && ddiscard < MAX_DISCARD_LEVEL)
			{
				++ddiscard;
				min_dim /= 2;
			}
			// Read back a raw image for this discard level, if it exists
			bool delete_raw = tex->reloadRawImage(ddiscard) != NULL;
			S32 cur_discard = tex->getRawImageLevel();
			if ((width == height && cur_discard > ddiscard) ||
				// *FIXME: for some reason, rectangular textures always get
				// stuck one discard level too high... HB
				(width != height && cur_discard > ddiscard + 1))
			{
				// Raw image is not detailed enough...
				LL_DEBUGS("RegionTexture") << "Cached raw data for terrain detail texture is not ready yet: "
										   << tex->getID()
										   << " - Discard level: "
										   << cur_discard
										   << " - Desired discard level: "
										   << ddiscard
										   << " - Full size: "
										   << tex->getFullWidth() << "x"
										   << tex->getFullHeight()
										   << " - Current size: "
										   << tex->getWidth() << "x"
										   << tex->getHeight()
										   << " - Shared raw image: "
										   << (delete_raw ? "false" : "true")
										   << LL_ENDL;
				if (tex->getDecodePriority() <= 0.f &&
					!tex->hasSavedRawImage())
				{
					tex->setBoostLevel(LLGLTexture::BOOST_TERRAIN);
					tex->forceToRefetchTexture(ddiscard);
				}
				if (delete_raw)
				{
					tex->destroyRawImage();
				}
				return false;
			}

			if (tex->getWidth(ddiscard) < BASE_SIZE ||
				tex->getHeight(ddiscard) < BASE_SIZE ||
				tex->getComponents() != 3)
			{
				LLPointer<LLImageRaw> newraw = new LLImageRaw(BASE_SIZE,
															  BASE_SIZE, 3);
				newraw->composite(tex->getRawImage());
				mRawImages[i] = newraw; // Deletes previous raw
			}
			else
			{
				mRawImages[i] = tex->getRawImage(); // Deletes previous raw
			}
			if (delete_raw)
			{
				tex->destroyRawImage();
			}
		}
		st_data[i] = mRawImages[i]->getData();
		st_data_size[i] = mRawImages[i]->getDataSize();
	}

	// Generate and clamp x/y bounding box.

	S32 x_begin = (S32)(x * mScaleInv);
	S32 y_begin = (S32)(y * mScaleInv);
	S32 x_end = ll_round((x + width) * mScaleInv);
	S32 y_end = ll_round((y + width) * mScaleInv);

	if (x_end > mWidth)
	{
		llwarns << "x end > width" << llendl;
		x_end = mWidth;
	}
	if (y_end > mWidth)
	{
		llwarns << "y end > width" << llendl;
		y_end = mWidth;
	}

	// Generate target texture information, stride ratios.

	LLViewerTexture* texturep = mSurfacep->getSTexture();
	S32 tex_width = texturep->getWidth();
	S32 tex_height = texturep->getHeight();
	S32 tex_comps = texturep->getComponents();
	S32 tex_stride = tex_width * tex_comps;

	S32 st_comps = 3;
	S32 st_width = BASE_SIZE;
	S32 st_height = BASE_SIZE;

	if (tex_comps != st_comps)
	{
		llwarns_sparse << "Base texture comps != input texture comps"
					   << llendl;
		return false;
	}

	F32 tex_x_scalef = (F32)tex_width / (F32)mWidth;
	F32 tex_y_scalef = (F32)tex_height / (F32)mWidth;
	F32 tex_x_begin = (S32)((F32)x_begin * tex_x_scalef);
	F32 tex_y_begin = (S32)((F32)y_begin * tex_y_scalef);
	F32 tex_x_end = (S32)((F32)x_end * tex_x_scalef);
	F32 tex_y_end = (S32)((F32)y_end * tex_y_scalef);

	F32 tex_x_ratiof = (F32)mWidth * mScale / (F32)tex_width;
	F32 tex_y_ratiof = (F32)mWidth * mScale / (F32)tex_height;

	LLPointer<LLImageRaw> raw = new LLImageRaw(tex_width, tex_height,
											   tex_comps);
	U8* rawp = raw->getData();

	F32 st_x_stride = ((F32)st_width / (F32)mTexScaleX) *
					  ((F32)mWidth / (F32)tex_width);
	F32 st_y_stride = ((F32)st_height / (F32)mTexScaleY) *
					  ((F32)mWidth / (F32)tex_height);

	llassert(st_x_stride > 0.f && st_y_stride > 0.f);

	// Iterate through the target texture, striding through the sub-textures
	// and interpolating appropriately.

	F32 sti = tex_x_begin * st_x_stride -
			  st_width * llfloor(tex_x_begin * st_x_stride / st_width);
	F32 stj = tex_y_begin * st_y_stride -
			  st_height * llfloor(tex_y_begin * st_y_stride / st_height);

	S32 st_offset = (llfloor(stj * st_width) + llfloor(sti)) * st_comps;
	for (S32 j = tex_y_begin; j < tex_y_end; ++j)
	{
		U32 offset = j * tex_stride + tex_x_begin * tex_comps;
		sti = tex_x_begin * st_x_stride -
			  st_width * ((U32)(tex_x_begin * st_x_stride) / st_width);
		for (S32 i = tex_x_begin; i < tex_x_end; ++i)
		{
			F32 composition = getValueScaled(i * tex_x_ratiof,
											 j * tex_y_ratiof);

			S32 tex0 = llfloor(composition);
			tex0 = llclamp(tex0, 0, 3);
			composition -= tex0;

			S32 tex1 = tex0 + 1;
			tex1 = llclamp(tex1, 0, 3);

			st_offset = (lltrunc(sti) + lltrunc(stj) * st_width) * st_comps;
			for (S32 k = 0; k < tex_comps; ++k)
			{
				// Linearly interpolate based on composition.
				if (st_offset < st_data_size[tex0] &&
					st_offset < st_data_size[tex1])
				{
					F32 a = *(st_data[tex0] + st_offset);
					F32 b = *(st_data[tex1] + st_offset);
					rawp[offset] = (U8)lltrunc(a + composition * (b - a));
				}
				++offset;
				++st_offset;
			}

			sti += st_x_stride;
			if (sti >= st_width)
			{
				sti -= st_width;
			}
		}

		stj += st_y_stride;
		if (stj >= st_height)
		{
			stj -= st_height;
		}
	}

	if (!texturep->hasGLTexture())
	{
		texturep->createGLTexture(0, raw);
	}
	texturep->setSubImage(raw, tex_x_begin, tex_y_begin,
						  tex_x_end - tex_x_begin, tex_y_end - tex_y_begin);
	LLSurface::sTextureUpdateTime += gen_timer.getElapsedTimeF32();
	LLSurface::sTexelsUpdated += (tex_x_end - tex_x_begin) *
								 (tex_y_end - tex_y_begin);

	for (S32 i = 0; i < (S32)TERRAIN_COUNT; ++i)
	{
		// Un-boost detail textures (will get re-boosted if rendering in high
		// detail)
		mDetailTextures[i]->setBoostLevel(LLGLTexture::BOOST_NONE);
		mDetailTextures[i]->setMinDiscardLevel(MAX_DISCARD_LEVEL + 1);
	}

	mTexturesLoaded = true;

	return true;
}
