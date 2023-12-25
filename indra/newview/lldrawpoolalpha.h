/**
 * @file lldrawpoolalpha.h
 * @brief LLDrawPoolAlpha class definition
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

#ifndef LL_LLDRAWPOOLALPHA_H
#define LL_LLDRAWPOOLALPHA_H

#include "lldrawpool.h"
#include "llframetimer.h"
#include "llrender.h"

class LLFace;
class LLColor4;
class LLGLSLShader;
class LLTexUnit;

class LLDrawPoolAlpha final : public LLRenderPass
{
protected:
	LOG_CLASS(LLDrawPoolAlpha);

public:
	enum
	{
		VERTEX_DATA_MASK =	LLVertexBuffer::MAP_VERTEX |
							LLVertexBuffer::MAP_NORMAL |
							LLVertexBuffer::MAP_COLOR |
							LLVertexBuffer::MAP_TEXCOORD0
	};
	LL_INLINE U32 getVertexDataMask() override			{ return VERTEX_DATA_MASK; }

	LLDrawPoolAlpha(U32 type);

	LL_INLINE S32 getNumPostDeferredPasses() override	{ return 1; }
	void renderPostDeferred(S32 pass) override;

	// This method is only for EE rendering
	void render(S32 pass = 0) override;

	void prerender() override;

	void forwardRender(bool write_depth = false);

private:
	// PBR variants
	void renderPostDeferredPBR(S32 pass);

	void renderDebugAlpha();

	void renderAlpha(U32 mask, bool depth_only = false, bool rigged = false);
	// Note: 'mask' is not used/ignored for the PBR rendering mode
	void renderAlphaHighlight(U32 mask = 0);

	typedef std::vector<LLDrawInfo*> drawinfo_vec_t;
	void renderEmissives(U32 mask, const drawinfo_vec_t& emissives);
	void renderRiggedEmissives(U32 mask, const drawinfo_vec_t& emissives);
	void renderPbrEmissives(const drawinfo_vec_t& emissives);
	void renderRiggedPbrEmissives(const drawinfo_vec_t& emissives);

	bool texSetup(LLDrawInfo* infop, bool use_material, LLTexUnit* unitp);

public:
	static bool			sShowDebugAlpha;

private:
	LLGLSLShader*		mTargetShader;
	LLGLSLShader*		mSimpleShader;
	LLGLSLShader*		mFullbrightShader;
	LLGLSLShader*		mEmissiveShader;
	LLGLSLShader*		mPBRShader;
	LLGLSLShader*		mPBREmissiveShader;

	// Our 'normal' alpha blend function for this pass
	U32					mColorSFactor;
	U32					mColorDFactor;
	U32					mAlphaSFactor;
	U32					mAlphaDFactor;
};

#endif // LL_LLDRAWPOOLALPHA_H
