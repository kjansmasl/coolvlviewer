/**
 * @file lldrawpoolterrain.h
 * @brief LLDrawPoolTerrain class definition
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

#ifndef LL_LLDRAWPOOLTERRAIN_H
#define LL_LLDRAWPOOLTERRAIN_H

#include "lldrawpool.h"

class LLDrawPoolTerrain final : public LLFacePool
{
public:
	LLDrawPoolTerrain(LLViewerTexture* texp);


	enum
	{
		VERTEX_DATA_MASK =	LLVertexBuffer::MAP_VERTEX |
							LLVertexBuffer::MAP_NORMAL |
							LLVertexBuffer::MAP_TEXCOORD0 |
							LLVertexBuffer::MAP_TEXCOORD1 |
							LLVertexBuffer::MAP_TEXCOORD2 |
							LLVertexBuffer::MAP_TEXCOORD3
	};

	U32 getVertexDataMask() override;

	LL_INLINE S32 getNumDeferredPasses() override	{ return 1; }
	void beginDeferredPass(S32) override;
	void endDeferredPass(S32 pass) override;
	void renderDeferred(S32 pass) override;

	LL_INLINE S32 getNumShadowPasses() override		{ return 1; }
	void beginShadowPass(S32) override;
	void endShadowPass(S32 pass) override;
	void renderShadow(S32 pass) override;

	void prerender() override;

	// These three methods are used for EE rendering only
	void render(S32 pass = 0) override;
	void beginRenderPass(S32) override;
	void endRenderPass(S32 pass) override;

	// Only terrain pool got a need for a dirtyTextures() method. HB
	LL_INLINE bool isTerrainPool() override			{ return true; }
	void dirtyTextures(const LLViewerTextureList::dirty_list_t& tex);

	LL_INLINE LLViewerTexture* getTexture() override
	{
		return mTexturep;
	}

#if 0
	// Failed attempt at properly restoring terrain after GL restart with core
	// GL profile enabled. HB
	void rebuildPatches();
#endif

protected:
	void renderSimple();
	void renderOwnership();
	void hilightParcelOwners();
	void renderFull2TU();
	void renderFull4TU();
	void renderFullShader();
	void drawLoop();
	void boostTerrainDetailTextures();

public:
	LLPointer<LLViewerTexture> mTexturep;
	LLPointer<LLViewerTexture> mAlphaRampImagep;
	LLPointer<LLViewerTexture> m2DAlphaRampImagep;
	LLPointer<LLViewerTexture> mAlphaNoiseImagep;

private:
	static S32 sDetailMode;
	static F32 sDetailScale;	// Meters per texture
};

#endif // LL_LLDRAWPOOLSIMPLE_H
