/**
 * @file lldrawpoolsimple.h
 * @brief LLDrawPoolSimple class definition
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

#ifndef LL_LLDRAWPOOLSIMPLE_H
#define LL_LLDRAWPOOLSIMPLE_H

#include "lldrawpool.h"

class LLGLSLShader;

class LLDrawPoolSimple final : public LLRenderPass
{
public:
	enum
	{
		VERTEX_DATA_MASK =	LLVertexBuffer::MAP_VERTEX |
							LLVertexBuffer::MAP_NORMAL |
							LLVertexBuffer::MAP_TEXCOORD0 |
							LLVertexBuffer::MAP_COLOR
	};

	LL_INLINE U32 getVertexDataMask() override			{ return VERTEX_DATA_MASK; }

	LLDrawPoolSimple();

	LL_INLINE S32 getNumDeferredPasses() override		{ return 1; }
	void renderDeferred(S32 pass) override;

	// Only for use with the EE renderer

	void prerender() override;
	void render(S32 pass = 0) override;
};

class LLDrawPoolGrass final : public LLRenderPass
{
public:
	enum
	{
		VERTEX_DATA_MASK =	LLVertexBuffer::MAP_VERTEX |
							LLVertexBuffer::MAP_NORMAL |
							LLVertexBuffer::MAP_TEXCOORD0 |
							LLVertexBuffer::MAP_COLOR
	};

	LL_INLINE U32 getVertexDataMask() override			{ return VERTEX_DATA_MASK; }

	LLDrawPoolGrass();

	LL_INLINE S32 getNumDeferredPasses() override		{ return 1; }
	void renderDeferred(S32 pass) override;

	// Only for use with the EE renderer

	void beginRenderPass(S32 pass) override;
	void endRenderPass(S32 pass) override;

	void prerender() override;
	void render(S32 pass = 0) override;
};

class LLDrawPoolAlphaMask final : public LLRenderPass
{
public:
	enum
	{
		VERTEX_DATA_MASK =	LLVertexBuffer::MAP_VERTEX |
							LLVertexBuffer::MAP_NORMAL |
							LLVertexBuffer::MAP_TEXCOORD0 |
							LLVertexBuffer::MAP_COLOR
	};

	LL_INLINE U32 getVertexDataMask() override			{ return VERTEX_DATA_MASK; }

	LLDrawPoolAlphaMask();

	LL_INLINE S32 getNumDeferredPasses() override		{ return 1; }
	void renderDeferred(S32 pass) override;

	// Only for use with the EE renderer

	void prerender() override;
	void render(S32 pass = 0) override;
};

class LLDrawPoolFullbrightAlphaMask final : public LLRenderPass
{
public:
	enum
	{
		VERTEX_DATA_MASK =	LLVertexBuffer::MAP_VERTEX |
							LLVertexBuffer::MAP_TEXCOORD0 |
							LLVertexBuffer::MAP_COLOR
	};

	LL_INLINE U32 getVertexDataMask() override			{ return VERTEX_DATA_MASK; }

	LLDrawPoolFullbrightAlphaMask();

	LL_INLINE S32 getNumPostDeferredPasses() override	{ return 1; }
	void renderPostDeferred(S32 pass) override;

	// Only for use with the EE renderer

	void prerender() override;
	void render(S32 pass = 0) override;
};

class LLDrawPoolFullbright final : public LLRenderPass
{
public:
	enum
	{
		VERTEX_DATA_MASK =	LLVertexBuffer::MAP_VERTEX |
							LLVertexBuffer::MAP_TEXCOORD0 |
							LLVertexBuffer::MAP_COLOR
	};

	LL_INLINE U32 getVertexDataMask() override			{ return VERTEX_DATA_MASK; }

	LLDrawPoolFullbright();

	LL_INLINE S32 getNumPostDeferredPasses() override	{ return 1; }
	void renderPostDeferred(S32 pass) override;

	// Only for use with the EE renderer

	void prerender() override;
	void render(S32 pass = 0) override;
};

class LLDrawPoolGlow final : public LLRenderPass
{
public:
	LLDrawPoolGlow()
	:	LLRenderPass(LLDrawPool::POOL_GLOW)
	{
	}

	enum
	{
		VERTEX_DATA_MASK =	LLVertexBuffer::MAP_VERTEX |
							LLVertexBuffer::MAP_TEXCOORD0 |
							LLVertexBuffer::MAP_EMISSIVE
	};

	LL_INLINE U32 getVertexDataMask() override			{ return VERTEX_DATA_MASK; }

	LL_INLINE S32 getNumPostDeferredPasses() override	{ return 1; }
	void renderPostDeferred(S32 pass) override;

	// Only for use with the EE renderer
	void render(S32 pass = 0) override;

private:
	void render(LLGLSLShader* shaderp);
};

#endif // LL_LLDRAWPOOLSIMPLE_H
