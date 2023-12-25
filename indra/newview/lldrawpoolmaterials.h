/**
 * @file lldrawpoolmaterials.h
 * @brief LLDrawPoolMaterials and LLDrawPoolMatPBR class definitions
 *
 * $LicenseInfo:firstyear=2012&license=viewergpl$
 *
 * Copyright (c) 2012-2022, Linden Research, Inc.
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

#ifndef LL_LLDRAWPOOLMATERIALS_H
#define LL_LLDRAWPOOLMATERIALS_H

#include "llvector2.h"
#include "llvector3.h"
#include "llcolor4u.h"

#include "lldrawpool.h"

class LLViewerTexture;
class LLDrawInfo;
class LLGLSLShader;

///////////////////////////////////////////////////////////////////////////////
// LLDrawPoolMaterials class
///////////////////////////////////////////////////////////////////////////////

class LLDrawPoolMaterials final : public LLRenderPass
{
public:
	LLDrawPoolMaterials();

	enum
	{
		VERTEX_DATA_MASK =	LLVertexBuffer::MAP_VERTEX |
							LLVertexBuffer::MAP_NORMAL |
							LLVertexBuffer::MAP_TEXCOORD0 |
							LLVertexBuffer::MAP_TEXCOORD1 |
							LLVertexBuffer::MAP_TEXCOORD2 |
							LLVertexBuffer::MAP_COLOR |
							LLVertexBuffer::MAP_TANGENT
	};

	LL_INLINE U32 getVertexDataMask() override		{ return VERTEX_DATA_MASK; }

	void prerender() override;

	// Not used by the EE forward renderer.
	LL_INLINE S32 getNumPasses() override			{ return 0; }

	// 12 render passes times 2 (one for each rigged and non rigged)
	LL_INLINE S32 getNumDeferredPasses() override	{ return 24; }
	void beginDeferredPass(S32 pass) override;
	void endDeferredPass(S32 pass) override;
	void renderDeferred(S32 pass) override;

	// The following methods are for EE rendering only

	void bindSpecularMap(LLViewerTexture* texp);
	void bindNormalMap(LLViewerTexture* texp);

private:
	// For EE rendering only
	void pushMaterialsBatch(LLDrawInfo& params, U32 mask);

	// For PBR rendering only
	void renderDeferredPBR(S32 pass);

private:
	LLGLSLShader* mShader;
};

///////////////////////////////////////////////////////////////////////////////
// LLDrawPoolMatPBR class
//
// In LL's original code, this class is named LLDrawPoolGLTFPBR and held in a
// separate lldrawpoolpbropaque.h/cpp module. I renamed it for consistency and
// moved it here, where it logically belongs to, since it is used to render PBR
// *materials*. HB
///////////////////////////////////////////////////////////////////////////////

class LLDrawPoolMatPBR final : public LLRenderPass
{
public:
	LLDrawPoolMatPBR(U32 type);

	// This value returned by this method is ignored by the PBR renderer.
	LL_INLINE U32 getVertexDataMask() override		{ return 0; }

	// Not used by the EE forward renderer. HB
	LL_INLINE S32 getNumPasses() override			{ return 0; }

	// Returns 0 in EE rendering mode, or 1 in PBR mode. HB
	S32 getNumDeferredPasses() override;
	void renderDeferred(S32 pass) override;

	LL_INLINE S32 getNumPostDeferredPasses() override
	{
		return getNumDeferredPasses();
	}
	void renderPostDeferred(S32 pass) override;

public:
	U32 mRenderType;
};

#endif	// LL_LLDRAWPOOLMATERIALS_H
