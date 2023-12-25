/**
 * @file lldrawpoolbump.h
 * @brief LLDrawPoolBump class definition
 *
 * $LicenseInfo:firstyear=2003&license=viewergpl$
 *
 * Copyright (c) 2003-2009, Linden Research, Inc.
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

#ifndef LL_LLDRAWPOOLBUMP_H
#define LL_LLDRAWPOOLBUMP_H

#include "hbfastmap.h"
#include "llstring.h"
#include "llrendertarget.h"
#include "lltextureentry.h"
#include "lluuid.h"

#include "lldrawpool.h"

class LLDrawInfo;
class LLGLSLShader;
class LLImageRaw;
class LLSpatialGroup;
class LLViewerFetchedTexture;

class LLDrawPoolBump : public LLRenderPass
{
protected :
	LL_INLINE LLDrawPoolBump(U32 type)
	:	LLRenderPass(type),
		mShiny(false),
		mRigged(false)
	{
	}

public:
	LL_INLINE LLDrawPoolBump()
	:	LLRenderPass(LLDrawPool::POOL_BUMP),
		mShiny(false),
		mRigged(false)
	{
	}

	// No more in use with the PBR renderer
	LL_INLINE U32 getVertexDataMask() override			{ return sVertexMask; }

	void prerender() override;

	// This method is only used by the EE renderer
	void render(S32 pass = 0) override;

	// Note: in these methods, 'mask' is ignored for the PBR renderer
	void pushBatch(LLDrawInfo& params, U32 mask, bool texture,
				   bool batch_textures = false) override;
	void renderGroup(LLSpatialGroup* groupp, U32 type, U32 mask,
					 bool texture = true) override;

	LL_INLINE S32 getNumDeferredPasses() override		{ return 1; }
	void renderDeferred(S32 pass) override;

	LL_INLINE S32 getNumPostDeferredPasses() override	{ return 1; }
	void renderPostDeferred(S32 pass) override;

private:
	// The three methods are only used by the EE renderer
	void beginShiny();
	void renderShiny();
	void endShiny();

	void beginFullbrightShiny();
	void renderFullbrightShiny();
	void endFullbrightShiny();

	void beginBump();
	void renderBump(U32 pass = PASS_BUMP);
	void endBump();
	// For the EE renderer only.
	void renderBump(U32 type, U32 mask);
	// For the PBR renderer only.
	void pushBumpBatches(U32 type);

	// Note: in this method, 'mask' is ignored for the PBR renderer
	void pushBumpBatch(LLDrawInfo& params, U32 mask, bool texture,
					   bool batch_textures = false);

	static bool bindBumpMap(LLDrawInfo& params, S32 channel = -2);
	static bool bindBumpMap(LLFace* facep, S32 channel = -2);

	// For the EE renderer only
	static void bindCubeMap(LLGLSLShader* shaderp, S32 shader_level,
							S32& diffuse_channel, S32& cube_channel);
	// For the EE renderer only
	static void unbindCubeMap(LLGLSLShader* shaderp, S32 shader_level,
							  S32& diffuse_channel);

	static bool bindBumpMap(U8 bump_code, LLViewerTexture* texp, F32 vsize,
							S32 channel);

public:
	bool		mShiny;
	bool		mRigged;		// When true, doing a rigged pass
	static U32	sVertexMask;
};

enum EBumpEffect
{
	BE_NO_BUMP = 0,
	BE_BRIGHTNESS = 1,
	BE_DARKNESS = 2,
	BE_STANDARD_0 = 3,  // Standard must always be the last one
	BE_COUNT = 4
};

///////////////////////////////////////////////////////////////////////////////
// List of standard bumpmaps that are specificed by LLTextureEntry::mBump's
// lower bits

class LLStandardBumpmap
{
protected:
	LOG_CLASS(LLStandardBumpmap);

public:
	LLStandardBumpmap() = default;

	LL_INLINE LLStandardBumpmap(const std::string& label)
	:	mLabel(label)
	{
	}

	LL_INLINE static void init()		{ add(); }
	LL_INLINE static void shutdown()	{ clear(); }

private:
	static void clear();
	static void add();

public:
	std::string							mLabel;
	LLPointer<LLViewerFetchedTexture>	mImage;

	// Number of valid values in gStandardBumpmapList[]
	static U32							sStandardBumpmapCount;
};

extern LLStandardBumpmap gStandardBumpmapList[TEM_BUMPMAP_COUNT];

///////////////////////////////////////////////////////////////////////////////
// List of one-component bump-maps created from other texures.

struct LLBumpImageEntry;

class LLBumpImageList
{
protected:
	LOG_CLASS(LLBumpImageList);

public:
	LLBumpImageList() = default;

	void destroyGL();
	void restoreGL();
	void updateImages();

	LLViewerTexture* getBrightnessDarknessImage(LLViewerFetchedTexture* src_image,
												U8 bump_code);
	void addTextureStats(U8 bump, const LLUUID& base_image_id,
						 F32 virtual_size);

	static void onSourceBrightnessLoaded(bool success,
										 LLViewerFetchedTexture* src_vi,
										 LLImageRaw* src, LLImageRaw* aux_src,
										 S32 discard_level, bool is_final,
										 void* userdata);
	static void onSourceDarknessLoaded(bool success,
									   LLViewerFetchedTexture* src_vi,
									   LLImageRaw* src, LLImageRaw* aux_src,
									   S32 discard_level, bool is_final,
									   void* userdata);
	static void onSourceStandardLoaded(bool success,
									   LLViewerFetchedTexture* src_vi,
									   LLImageRaw* src, LLImageRaw* aux_src,
									   S32 discard_level, bool is_final,
									   void* userdata);
	static void generateNormalMapFromAlpha(LLImageRaw* src,
										   LLImageRaw* nrm_image);


private:
	static void onSourceLoaded(bool success, LLViewerTexture* src_vi,
							   LLImageRaw* src, LLUUID& source_asset_id,
							   EBumpEffect bump);

private:
	typedef fast_hmap<LLUUID, LLPointer<LLViewerTexture> > bump_image_map_t;
	bump_image_map_t			mBrightnessEntries;
	bump_image_map_t			mDarknessEntries;

	static LLRenderTarget		sRenderTarget;
};

extern LLBumpImageList gBumpImageList;

class LLDrawPoolInvisible final : public LLDrawPoolBump
{
public:
	LL_INLINE LLDrawPoolInvisible()
	:	LLDrawPoolBump(LLDrawPool::POOL_INVISIBLE)
	{
	}

	enum
	{
		VERTEX_DATA_MASK = LLVertexBuffer::MAP_VERTEX
	};

	LL_INLINE U32 getVertexDataMask() override			{ return VERTEX_DATA_MASK; }

	void render(S32 pass = 0) override;

	LL_INLINE S32 getNumDeferredPasses() override		{ return 1; }
	void renderDeferred(S32 pass) override;
};

#endif // LL_LLDRAWPOOLBUMP_H
