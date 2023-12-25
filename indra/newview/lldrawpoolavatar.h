/**
 * @file lldrawpoolavatar.h
 * @brief LLDrawPoolAvatar class definition
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

#ifndef LL_LLDRAWPOOLAVATAR_H
#define LL_LLDRAWPOOLAVATAR_H

#include "lldrawpool.h"

class LLGLSLShader;
class LLFace;
class LLVolume;
class LLVolumeFace;
class LLVOVolume;

class LLDrawPoolAvatar final : public LLFacePool
{
public:
	LLDrawPoolAvatar(U32 type);
	~LLDrawPoolAvatar() override;

	enum
	{
		SHADER_LEVEL_BUMP = 2,
		SHADER_LEVEL_CLOTH = 3
	};

	enum
	{
		VERTEX_DATA_MASK =	LLVertexBuffer::MAP_VERTEX |
							LLVertexBuffer::MAP_NORMAL |
							LLVertexBuffer::MAP_TEXCOORD0 |
							LLVertexBuffer::MAP_WEIGHT |
							LLVertexBuffer::MAP_CLOTHWEIGHT
	};

	typedef enum
	{
		SHADOW_PASS_AVATAR_OPAQUE,
		SHADOW_PASS_AVATAR_ALPHA_BLEND,
		SHADOW_PASS_AVATAR_ALPHA_MASK,
		NUM_SHADOW_PASSES
	} eShadowPass;

	U32 getVertexDataMask() override					{ return VERTEX_DATA_MASK; }

	static LLMatrix4& getModelView();

	LL_INLINE S32 getNumPasses() override				{ return 3; }
	void beginRenderPass(S32 pass) override;
	void endRenderPass(S32 pass) override;
	void prerender() override;
	void render(S32 pass = 0) override;

	LL_INLINE S32 getNumDeferredPasses() override		{ return 3; }
	void beginDeferredPass(S32 pass) override;
	void endDeferredPass(S32 pass) override;
	LL_INLINE void renderDeferred(S32 pass) override	{ render(pass); }

	LL_INLINE S32 getNumPostDeferredPasses() override	{ return 1; }
	void beginPostDeferredPass(S32 pass) override;
	void endPostDeferredPass(S32 pass) override;
	void renderPostDeferred(S32 pass) override;

	LL_INLINE S32 getNumShadowPasses() override			{ return NUM_SHADOW_PASSES; }
	void beginShadowPass(S32 pass) override;
	void endShadowPass(S32 pass) override;
	void renderShadow(S32 pass) override;

	void beginRigid();
	void beginImpostor();
	void beginSkinned();

	void endRigid();
	void endImpostor();
	void endSkinned();

	void beginDeferredImpostor();
	void beginDeferredRigid();
	void beginDeferredSkinned();

	void endDeferredImpostor();
	void endDeferredRigid();
	void endDeferredSkinned();

	// Renders only one avatar if single_avatar is not null.
	void renderAvatars(LLVOAvatar* single_avatar, S32 pass = -1);

public:
	static F32				sMinimumAlpha;
	static S32				sDiffuseChannel;
	static S32				sShadowPass;
	static bool				sSkipOpaque;
	static bool				sSkipTransparent;

	static LLGLSLShader*	sVertexProgram;
};

#endif // LL_LLDRAWPOOLAVATAR_H
