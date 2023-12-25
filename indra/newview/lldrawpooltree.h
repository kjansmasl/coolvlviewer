/**
 * @file lldrawpooltree.h
 * @brief LLDrawPoolTree class definition
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

#ifndef LL_LLDRAWPOOLTREE_H
#define LL_LLDRAWPOOLTREE_H

#include "lldrawpool.h"

class LLDrawPoolTree final : public LLFacePool
{
public:
	enum
	{
		VERTEX_DATA_MASK =	LLVertexBuffer::MAP_VERTEX |
							LLVertexBuffer::MAP_NORMAL |
							LLVertexBuffer::MAP_COLOR |
							LLVertexBuffer::MAP_TEXCOORD0
	};

	LL_INLINE U32 getVertexDataMask() override
	{
		return VERTEX_DATA_MASK;
	}

	LLDrawPoolTree(LLViewerTexture* texturep);

	void prerender() override;

	// These three methods are for EE rendering only
	void beginRenderPass(S32) override;
	void endRenderPass(S32) override;
	LL_INLINE void render(S32 pass = 0) override		{ renderDeferred(pass); }

	LL_INLINE S32 getNumDeferredPasses() override		{ return 1; }
	void beginDeferredPass(S32) override;
	void endDeferredPass(S32) override;
	void renderDeferred(S32 pass) override;

	LL_INLINE S32 getNumShadowPasses() override			{ return 1; }
	void beginShadowPass(S32 pass) override;
	void endShadowPass(S32 pass) override;
	LL_INLINE void renderShadow(S32 pass) override		{ render(pass); }

	LL_INLINE bool verify() const override				{ return true; }

	LL_INLINE LLViewerTexture* getTexture() override	{ return mTexturep; }

private:
	void renderTree();

private:
	LLPointer<LLViewerTexture>	mTexturep;
};

#endif // LL_LLDRAWPOOLTREE_H
