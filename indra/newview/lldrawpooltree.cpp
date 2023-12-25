/**
 * @file lldrawpooltree.cpp
 * @brief LLDrawPoolTree class implementation
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

#include "lldrawpooltree.h"

#include "llfasttimer.h"
#include "llrender.h"

#include "lldrawable.h"
#include "llenvironment.h"
#include "llface.h"
#include "llpipeline.h"
#include "llviewercontrol.h"
#include "llviewerregion.h"
#include "llviewershadermgr.h"
#include "llvotree.h"

static LLGLSLShader* sShaderp = NULL;

LLDrawPoolTree::LLDrawPoolTree(LLViewerTexture* texturep)
:	LLFacePool(POOL_TREE),
	mTexturep(texturep)
{
	mTexturep->setAddressMode(LLTexUnit::TAM_WRAP);
}

//virtual
void LLDrawPoolTree::prerender()
{
	mShaderLevel =
		gViewerShaderMgrp->getShaderLevel(LLViewerShaderMgr::SHADER_OBJECT);
}

// For EE rendering only
//virtual
void LLDrawPoolTree::beginRenderPass(S32)
{
	LL_FAST_TIMER(FTM_RENDER_TREES);

	if (LLPipeline::sUnderWaterRender)
	{
		sShaderp = &gTreeWaterProgram;
	}
	else
	{
		sShaderp = &gTreeProgram;
	}

	if (gPipeline.shadersLoaded())
	{
		sShaderp->bind();
		sShaderp->setMinimumAlpha(0.5f);
		gGL.diffuseColor4f(1.f, 1.f, 1.f, 1.f);
	}
	else
	{
		gPipeline.enableLightsDynamic();
		gGL.flush();
	}
}

// For EE rendering only
//virtual
void LLDrawPoolTree::endRenderPass(S32)
{
	LL_FAST_TIMER(FTM_RENDER_TREES);

	if (gPipeline.canUseWindLightShaders())
	{
		sShaderp->unbind();
	}
	if (mShaderLevel <= 0)
	{
		gGL.flush();
	}
}

//virtual
void LLDrawPoolTree::renderDeferred(S32)
{
	LL_FAST_TIMERS(LLPipeline::sShadowRender, FTM_SHADOW_TREE,
				   FTM_RENDER_TREES);

	if (mDrawFace.empty())
	{
		return;
	}

	if (LLVOTree::sRenderAnimateTrees)
	{
		renderTree();
		return;
	}

	gGL.getTexUnit(0)->bindFast(mTexturep);
	// Keep Linden tree textures at full res
	constexpr F32 MAX_AREA = 1024.f * 1024.f;
	gPipeline.touchTexture(mTexturep, MAX_AREA);

	for (std::vector<LLFace*>::iterator iter = mDrawFace.begin(),
										end = mDrawFace.end();
		 iter != end; ++iter)
	{
		LLFace* facep = *iter;
		if (!facep) continue;	// Paranoia

		LLVertexBuffer* buffp = facep->getVertexBuffer();
		LLDrawable* drawablep = facep->getDrawable();
		if (!buffp || !drawablep) continue;

		LLViewerRegion* regionp = drawablep->getRegion();
		if (!regionp) continue;

		LLMatrix4* model_matrix = &(regionp->mRenderMatrix);
		if (model_matrix != gGLLastMatrix)
		{
			gGLLastMatrix = model_matrix;
			gGL.loadMatrix(gGLModelView);
			if (model_matrix)
			{
				llassert(gGL.getMatrixMode() == LLRender::MM_MODELVIEW);
				gGL.multMatrix(model_matrix->getF32ptr());
			}
			++gPipeline.mMatrixOpCount;
		}

		buffp->setBufferFast(LLDrawPoolTree::VERTEX_DATA_MASK);
		buffp->drawRangeFast(0, buffp->getNumVerts() - 1,
							 buffp->getNumIndices(), 0);
	}
}

//virtual
void LLDrawPoolTree::beginDeferredPass(S32)
{
	LL_FAST_TIMER(FTM_RENDER_TREES);

	sShaderp = &gDeferredTreeProgram;
	sShaderp->bind();
	sShaderp->setMinimumAlpha(0.5f);
}

//virtual
void LLDrawPoolTree::endDeferredPass(S32)
{
	LL_FAST_TIMER(FTM_RENDER_TREES);
	sShaderp->unbind();
}

//virtual
void LLDrawPoolTree::beginShadowPass(S32)
{
	LL_FAST_TIMER(FTM_SHADOW_TREE);

	static LLCachedControl<F32> shadow_offset(gSavedSettings,
											  "RenderDeferredTreeShadowOffset");
	static LLCachedControl<F32> shadow_bias(gSavedSettings,
											"RenderDeferredTreeShadowBias");
	glPolygonOffset(shadow_offset, shadow_bias);

	gDeferredTreeShadowProgram.bind();
	S32 sun_up = gEnvironment.getIsSunUp() ? 1 : 0;
	gDeferredTreeShadowProgram.uniform1i(LLShaderMgr::SUN_UP_FACTOR, sun_up);
	gDeferredTreeShadowProgram.setMinimumAlpha(0.5f);
}

//virtual
void LLDrawPoolTree::endShadowPass(S32)
{
	LL_FAST_TIMER(FTM_SHADOW_TREE);

	static LLCachedControl<F32> shadow_offset(gSavedSettings,
											  "RenderDeferredSpotShadowOffset");
	static LLCachedControl<F32> shadow_bias(gSavedSettings,
											"RenderDeferredSpotShadowBias");
	glPolygonOffset(shadow_offset, shadow_bias);
	gDeferredTreeShadowProgram.unbind();
}

void LLDrawPoolTree::renderTree()
{
	// Bind the texture for this tree.
	gGL.getTexUnit(0)->bind(mTexturep.get());

	gGL.matrixMode(LLRender::MM_MODELVIEW);

	static const LLColor4U color(255, 255, 255, 255);
	LLFacePool::LLOverrideFaceColor clr(this, color);

	for (std::vector<LLFace*>::iterator iter = mDrawFace.begin(),
										end = mDrawFace.end();
		 iter != end; ++iter)
	{
		LLFace* face = *iter;
		if (!face || !face->getVertexBuffer())
		{
			continue;
		}

		LLDrawable* drawablep = face->getDrawable();
		if (!drawablep || drawablep->isDead())
		{
			continue;
		}

		face->getVertexBuffer()->setBuffer(LLDrawPoolTree::VERTEX_DATA_MASK);

		// Render each of the trees
		LLVOTree* treep = (LLVOTree*)drawablep->getVObj().get();
		if (!treep) continue;

		gGLLastMatrix = NULL;
		gGL.loadMatrix(gGLModelView.getF32ptr());

		LLMatrix4 matrix(gGLModelView.getF32ptr());

		// Translate to tree base.
		const LLVector3& pos_agent = treep->getPositionAgent();
		LLMatrix4 trans_mat;
		// *HACK: adjustment in Z plants tree underground
		trans_mat.setTranslation(pos_agent.mV[VX], pos_agent.mV[VY],
								 pos_agent.mV[VZ] - 0.1f);
		trans_mat *= matrix;

		// Rotate to tree position and bend for current trunk/wind. Note that
		// trunk stiffness controls the amount of bend at the trunk as opposed
		// to the crown of the tree
		static const LLVector4 z_axis(0.f, 0.f, 1.f, 1.f);
		static const LLQuaternion qz(F_PI_BY_TWO, z_axis);
		LLQuaternion rot(treep->mTrunkBend.length() * TRUNK_STIFF,
						 LLVector4(treep->mTrunkBend.mV[VX],
								   treep->mTrunkBend.mV[VY], 0.f));
		LLMatrix4 rot_mat(rot * qz * treep->getRotation());
		rot_mat *= trans_mat;

		F32 radius = treep->getScale().length() * 0.05f;
		LLMatrix4 scale_mat;
		scale_mat.mMatrix[0][0] = scale_mat.mMatrix[1][1] =
			scale_mat.mMatrix[2][2] = radius;
		scale_mat *= rot_mat;

		F32 droop = treep->mDroop + 25.f * (1.f - treep->mTrunkBend.length());

		F32 app_angle = treep->getAppAngle() * LLVOTree::sTreeFactor;

		for (U32 lod = 0; lod < MAX_NUM_TREE_LOD_LEVELS; ++lod)
		{
			if (app_angle > LLVOTree::sLODAngles[lod])
			{
				treep->drawBranchPipeline(scale_mat, NULL, lod, 0,
										  treep->mDepth, treep->mTrunkDepth,
										  1.f, treep->mTwist, droop,
										  treep->mBranches, 1.f);
				break;
			}
		}
	}
}
