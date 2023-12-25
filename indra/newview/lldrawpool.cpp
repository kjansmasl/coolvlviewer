/**
 * @file lldrawpool.cpp
 * @brief LLDrawPool class implementation
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

#include "lldrawpool.h"

#include "llglslshader.h"
#include "llmodel.h"
#include "llrender.h"
#include "hbtracy.h"

#include "lldrawable.h"
#include "lldrawpoolalpha.h"
#include "lldrawpoolavatar.h"
#include "lldrawpoolbump.h"
#include "lldrawpoolmaterials.h"
#include "lldrawpoolsimple.h"
#include "lldrawpoolsky.h"
#include "lldrawpooltree.h"
#include "lldrawpoolterrain.h"
#include "lldrawpoolwater.h"
#include "lldrawpoolwlsky.h"
#include "llface.h"
#include "llpipeline.h"
#include "llspatialpartition.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerobjectlist.h"		// For debug listing.
#include "llviewershadermgr.h"
#include "llvoavatar.h"

///////////////////////////////////////////////////////////////////////////////
// LLDrawPool class
///////////////////////////////////////////////////////////////////////////////

//static
S32 LLDrawPool::sNumDrawPools = 0;

LLDrawPool* LLDrawPool::createPool(U32 type, LLViewerTexture* tex0)
{
	LLDrawPool* poolp = NULL;
	switch (type)
	{
		case POOL_SIMPLE:
			poolp = new LLDrawPoolSimple();
			break;

		case POOL_GRASS:
			poolp = new LLDrawPoolGrass();
			break;

		case POOL_ALPHA_MASK:
			poolp = new LLDrawPoolAlphaMask();
			break;

		case POOL_FULLBRIGHT_ALPHA_MASK:
			poolp = new LLDrawPoolFullbrightAlphaMask();
			break;

		case POOL_FULLBRIGHT:
			poolp = new LLDrawPoolFullbright();
			break;

		// For the EE renderer only
		case POOL_INVISIBLE:
			poolp = new LLDrawPoolInvisible();
			break;

		case POOL_GLOW:
			poolp = new LLDrawPoolGlow();
			break;

		// For the EE renderer only
		case POOL_ALPHA:
			poolp = new LLDrawPoolAlpha(POOL_ALPHA);
			break;

		// For the PBR renderer only
		case POOL_ALPHA_PRE_WATER:
			poolp = new LLDrawPoolAlpha(POOL_ALPHA_PRE_WATER);
			break;

		// For the PBR renderer only
		case POOL_ALPHA_POST_WATER:
			poolp = new LLDrawPoolAlpha(POOL_ALPHA_POST_WATER);
			break;

		case POOL_AVATAR:
		case POOL_PUPPET:
			poolp = new LLDrawPoolAvatar(type);
			break;

		case POOL_TREE:
			poolp = new LLDrawPoolTree(tex0);
			break;

		case POOL_TERRAIN:
			poolp = new LLDrawPoolTerrain(tex0);
			break;

		case POOL_SKY:
			poolp = new LLDrawPoolSky();
			break;

		case POOL_WL_SKY:
			poolp = new LLDrawPoolWLSky();
			break;

		case POOL_VOIDWATER:
		case POOL_WATER:
			poolp = new LLDrawPoolWater();
			break;

		case POOL_BUMP:
			poolp = new LLDrawPoolBump();
			break;

		case POOL_MATERIALS:
			poolp = new LLDrawPoolMaterials();
			break;

		// For the PBR renderer only
		case POOL_MAT_PBR:
			poolp = new LLDrawPoolMatPBR(POOL_MAT_PBR);
			break;

		// For the PBR renderer only
		case POOL_MAT_PBR_ALPHA_MASK:
			poolp = new LLDrawPoolMatPBR(POOL_MAT_PBR_ALPHA_MASK);
			break;

		default:
			llerrs << "Unknown draw pool type !" << llendl;
	}

	llassert(poolp->mType == type);
	return poolp;
}

LLDrawPool::LLDrawPool(U32 type)
:	mType(type),
	mId(++sNumDrawPools),
	mShaderLevel(0)
{
}

// Forward rendering only works with the EE renderer.
//virtual
S32 LLDrawPool::getNumPasses()
{
	return gUsePBRShaders ? 0 : 1;
}

//virtual
void LLDrawPool::endRenderPass(S32)
{
	// Make sure channel 0 is active channel
	gGL.getTexUnit(0)->activate();
}

///////////////////////////////////////////////////////////////////////////////
// LLFacePool class
///////////////////////////////////////////////////////////////////////////////

LLFacePool::LLFacePool(U32 type)
:	LLDrawPool(type)
{
	resetDrawOrders();
}

LLFacePool::~LLFacePool()
{
	destroy();
}

void LLFacePool::destroy()
{
	if (!mReferences.empty())
	{
		llinfos << mReferences.size()
				<< " references left on deletion of draw pool !" << llendl;
	}
}

void LLFacePool::enqueue(LLFace* facep)
{
	mDrawFace.push_back(facep);
}

//virtual
bool LLFacePool::addFace(LLFace* facep)
{
	addFaceReference(facep);
	return true;
}

//virtual
void LLFacePool::pushFaceGeometry()
{
	for (U32 i = 0, count = mDrawFace.size(); i < count; ++i)
	{
		mDrawFace[i]->renderIndexed();
	}
}

//virtual
bool LLFacePool::removeFace(LLFace* facep)
{
	removeFaceReference(facep);
	vector_replace_with_last(mDrawFace, facep);
	return true;
}

// Not absolutely sure if we should be resetting all of the chained pools as
// well - djs
void LLFacePool::resetDrawOrders()
{
	mDrawFace.resize(0);
}

void LLFacePool::removeFaceReference(LLFace* facep)
{
	if (facep->getReferenceIndex() != -1)
	{
		if (facep->getReferenceIndex() != (S32)mReferences.size())
		{
			LLFace* lastp = mReferences.back();
			mReferences[facep->getReferenceIndex()] = lastp;
			lastp->setReferenceIndex(facep->getReferenceIndex());
		}
		mReferences.pop_back();
	}
	facep->setReferenceIndex(-1);
}

void LLFacePool::addFaceReference(LLFace* facep)
{
	if (facep->getReferenceIndex() == -1)
	{
		facep->setReferenceIndex(mReferences.size());
		mReferences.push_back(facep);
	}
}

bool LLFacePool::verify() const
{
	bool ok = true;

	for (U32 i = 0, count = mDrawFace.size(); i < count; ++i)
	{
		const LLFace* facep = mDrawFace[i];
		if (facep->getPool() != this)
		{
			llwarns_once << "Face " << std::hex << intptr_t(facep) << std::dec
						 << " in wrong pool !" << llendl;
			facep->printDebugInfo();
			ok = false;
		}
		else if (!facep->verify())
		{
			ok = false;
		}
	}

	return ok;
}

void LLFacePool::printDebugInfo() const
{
	llinfos << "Pool: " << this << " - Type: " << getType() << llendl;
}

bool LLFacePool::LLOverrideFaceColor::sOverrideFaceColor = false;

void LLFacePool::LLOverrideFaceColor::setColor(const LLColor4& color)
{
	gGL.diffuseColor4fv(color.mV);
}

void LLFacePool::LLOverrideFaceColor::setColor(const LLColor4U& color)
{
	gGL.diffuseColor4ubv(color.mV);
}

void LLFacePool::LLOverrideFaceColor::setColor(F32 r, F32 g, F32 b, F32 a)
{
	gGL.diffuseColor4f(r, g, b, a);
}

///////////////////////////////////////////////////////////////////////////////
// LLRenderPass class
///////////////////////////////////////////////////////////////////////////////

void LLRenderPass::renderGroup(LLSpatialGroup* groupp, U32 type, U32 mask,
							   bool texture)
{
	LLSpatialGroup::drawmap_elem_t& draw_info = groupp->mDrawMap[type];
	for (U32 i = 0, count = draw_info.size(); i < count; ++i)
	{
		LLDrawInfo* paramsp = draw_info[i].get();
		if (paramsp)
		{
			pushBatch(*paramsp, mask, texture);
		}
	}
}

void LLRenderPass::renderRiggedGroup(LLSpatialGroup* groupp, U32 type, U32 mask,
									 bool texture)
{
	LL_TRACY_TIMER(TRC_RENDER_RIGGED_GROUP);

	LLVOAvatar* last_avatarp = NULL;
	U64 last_hash = 0;
	// NOTE: does not impact PBR rendering (mask ignored). HB
	mask |= LLVertexBuffer::MAP_WEIGHT4;

	LLSpatialGroup::drawmap_elem_t& draw_info = groupp->mDrawMap[type];
	for (U32 i = 0, count = draw_info.size(); i < count; ++i)
	{
		LLDrawInfo* paramsp = draw_info[i].get();
		if (!paramsp) continue;

		if (paramsp->mAvatar.notNull() && paramsp->mSkinInfo.notNull() &&
			(paramsp->mAvatar.get() != last_avatarp ||
			 paramsp->mSkinInfo->mHash != last_hash))
		{
#if 0
			if (!uploadMatrixPalette(*paramsp))
			{
				continue;
			}
#else
			uploadMatrixPalette(*paramsp);
#endif
			last_avatarp = paramsp->mAvatar.get();
			last_hash = paramsp->mSkinInfo->mHash;
		}

		pushBatch(*paramsp, mask, texture);
	}
}

void LLRenderPass::pushBatches(U32 type, U32 mask, bool texture,
							   bool batch_textures)
{
	if (!texture && gUsePBRShaders)
	{
		pushUntexturedBatches(type);
		return;
	}

	if (!gPipeline.sCull)
	{
		// Paranoia (sCull != NULL needed for getRenderMap())
		return;
	}

	LLCullResult::drawinfo_list_t& draw_list = gPipeline.getRenderMap(type);
	for (U32 i = 0, count = draw_list.size(); i < count; )
	{
		LLDrawInfo* paramsp = draw_list[i++];

		// Draw info cache prefetching optimization.
		if (i < count)
		{
			_mm_prefetch((char*)draw_list[i]->mVertexBuffer.get(),
						 _MM_HINT_NTA);
			if (i + 1 < count)
			{
				_mm_prefetch((char*)draw_list[i + 1], _MM_HINT_NTA);
			}
		}

		pushBatch(*paramsp, mask, texture, batch_textures);
	}
}

void LLRenderPass::pushRiggedBatches(U32 type, U32 mask, bool texture,
									 bool batch_textures)
{
	if (!texture && gUsePBRShaders)
	{
		pushUntexturedRiggedBatches(type);
		return;
	}

	if (!gPipeline.sCull)
	{
		// Paranoia (sCull != NULL needed for getRenderMap())
		return;
	}

	LLVOAvatar* last_avatarp = NULL;
	U64 last_hash = 0;
	mask |= LLVertexBuffer::MAP_WEIGHT4;

	LLCullResult::drawinfo_list_t& draw_list = gPipeline.getRenderMap(type);
	for (U32 i = 0, count = draw_list.size(); i < count; )
	{
		LLDrawInfo* paramsp = draw_list[i++];

		// Draw info cache prefetching optimization.
		if (i < count)
		{
			_mm_prefetch((char*)draw_list[i]->mVertexBuffer.get(),
						 _MM_HINT_NTA);
			if (i + 1 < count)
			{
				_mm_prefetch((char*)draw_list[i + 1], _MM_HINT_NTA);
			}
		}

		if (paramsp->mAvatar.notNull() && paramsp->mSkinInfo.notNull() &&
			(paramsp->mAvatar.get() != last_avatarp ||
			 paramsp->mSkinInfo->mHash != last_hash))
		{
#if 0
			if (!uploadMatrixPalette(*paramsp))
			{
				continue;
			}
#else
			uploadMatrixPalette(*paramsp);
#endif
			last_avatarp = paramsp->mAvatar.get();
			last_hash = paramsp->mSkinInfo->mHash;
		}

		pushBatch(*paramsp, mask, texture, batch_textures);
	}
}

void LLRenderPass::pushMaskBatches(U32 type, U32 mask, bool texture,
								   bool batch_textures)
{
	if (!gPipeline.sCull)
	{
		// Paranoia (sCull != NULL needed for getRenderMap())
		return;
	}

	LLGLSLShader* shaderp = LLGLSLShader::sCurBoundShaderPtr;
	if (!shaderp)	// Paranoia
	{
		llwarns_sparse << "sCurBoundShaderPtr is NULL !" << llendl;
		llassert(false);
		return;
	}

	LLCullResult::drawinfo_list_t& draw_list = gPipeline.getRenderMap(type);
	for (U32 i = 0, count = draw_list.size(); i < count; )
	{
		LLDrawInfo* paramsp = draw_list[i++];

		// Draw info cache prefetching optimization.
		if (i < count)
		{
			_mm_prefetch((char*)draw_list[i]->mVertexBuffer.get(),
						 _MM_HINT_NTA);
			if (i + 1 < count)
			{
				_mm_prefetch((char*)draw_list[i + 1], _MM_HINT_NTA);
			}
		}

		shaderp->setMinimumAlpha(paramsp->mAlphaMaskCutoff);
		pushBatch(*paramsp, mask, texture, batch_textures);
	}
}

void LLRenderPass::pushRiggedMaskBatches(U32 type, U32 mask, bool texture,
										 bool batch_textures)
{
	if (!gPipeline.sCull)
	{
		// Paranoia (sCull != NULL needed for getRenderMap())
		return;
	}

	LLGLSLShader* shaderp = LLGLSLShader::sCurBoundShaderPtr;
	if (!shaderp)
	{
		gGL.flush();
	}

	LLVOAvatar* last_avatarp = NULL;
	U64 last_hash = 0;
	mask |= LLVertexBuffer::MAP_WEIGHT4;

	LLCullResult::drawinfo_list_t& draw_list = gPipeline.getRenderMap(type);
	for (U32 i = 0, count = draw_list.size(); i < count; )
	{
		LLDrawInfo* paramsp = draw_list[i++];

		// Draw info cache prefetching optimization.
		if (i < count)
		{
			_mm_prefetch((char*)draw_list[i]->mVertexBuffer.get(),
						 _MM_HINT_NTA);
			if (i + 1 < count)
			{
				_mm_prefetch((char*)draw_list[i + 1], _MM_HINT_NTA);
			}
		}

		if (shaderp)
		{
			shaderp->setMinimumAlpha(paramsp->mAlphaMaskCutoff);
		}

		if (paramsp->mAvatar.notNull() && paramsp->mSkinInfo.notNull() &&
			(paramsp->mAvatar.get() != last_avatarp ||
			 paramsp->mSkinInfo->mHash != last_hash))
		{
#if 0
			if (!uploadMatrixPalette(*paramsp))
			{
				continue;
			}
#else
			uploadMatrixPalette(*paramsp);
#endif
			last_avatarp = paramsp->mAvatar.get();
			last_hash = paramsp->mSkinInfo->mHash;
		}

		pushBatch(*paramsp, mask, texture, batch_textures);
	}
}

//virtual
void LLRenderPass::pushBatch(LLDrawInfo& params, U32 mask, bool texture,
							 bool batch_textures)
{
	if (!params.mCount)
	{
		return;
	}

	applyModelMatrix(params);

	bool tex_setup = false;

	if (texture || gUsePBRShaders)
	{
		U32 count;
		if (batch_textures && (count = params.mTextureList.size()) > 1)
		{
			for (U32 i = 0; i < count; ++i)
			{
				const LLPointer<LLViewerTexture>& tex = params.mTextureList[i];
				if (tex.notNull())
				{
					gGL.getTexUnit(i)->bindFast(tex);
				}
			}
		}
		// Not batching textures or batch has only 1 texture: might need a
		// texture matrix.
		else if (params.mTexture.notNull())
		{
			LLTexUnit* unit0 = gGL.getTexUnit(0);
			unit0->bindFast(params.mTexture);
			if (params.mTextureMatrix)
			{
				tex_setup = true;
				unit0->activate();
				gGL.matrixMode(LLRender::MM_TEXTURE);
				gGL.loadMatrix(params.mTextureMatrix->getF32ptr());
				++gPipeline.mTextureMatrixOps;
			}
		}
		else
		{
			gGL.getTexUnit(0)->unbindFast(LLTexUnit::TT_TEXTURE);
		}
	}

	// Note: mask is ignored for the PBR renderer
	params.mVertexBuffer->setBufferFast(mask);
	params.mVertexBuffer->drawRange(LLRender::TRIANGLES, params.mStart,
									params.mEnd, params.mCount,
									params.mOffset);

	if (tex_setup)
	{
		gGL.matrixMode(LLRender::MM_TEXTURE0);
		gGL.loadIdentity();
		gGL.matrixMode(LLRender::MM_MODELVIEW);
	}
}

// Used only by the PBR renderer
void LLRenderPass::pushUntexturedBatches(U32 type)
{
	if (!gPipeline.sCull)
	{
		// Paranoia (sCull != NULL needed for getRenderMap())
		return;
	}

	LLCullResult::drawinfo_list_t& draw_list = gPipeline.getRenderMap(type);
	for (U32 i = 0, count = draw_list.size(); i < count; )
	{
		LLDrawInfo* paramsp = draw_list[i++];

		// Draw info cache prefetching optimization.
		if (i < count)
		{
			_mm_prefetch((char*)draw_list[i]->mVertexBuffer.get(),
						 _MM_HINT_NTA);
			if (i + 1 < count)
			{
				_mm_prefetch((char*)draw_list[i + 1], _MM_HINT_NTA);
			}
		}
		pushUntexturedBatch(*paramsp);
	}
}

// Used only by the PBR renderer
void LLRenderPass::pushUntexturedRiggedBatches(U32 type)
{
	if (!gPipeline.sCull)
	{
		// Paranoia (sCull != NULL needed for getRenderMap())
		return;
	}

	LLVOAvatar* last_avatarp = NULL;
	U64 last_hash = 0;

	LLCullResult::drawinfo_list_t& draw_list = gPipeline.getRenderMap(type);
	for (U32 i = 0, count = draw_list.size(); i < count; )
	{
		LLDrawInfo* paramsp = draw_list[i++];

		// Draw info cache prefetching optimization.
		if (i < count)
		{
			_mm_prefetch((char*)draw_list[i]->mVertexBuffer.get(),
						 _MM_HINT_NTA);
			if (i + 1 < count)
			{
				_mm_prefetch((char*)draw_list[i + 1], _MM_HINT_NTA);
			}
		}

		if (paramsp->mAvatar.notNull() && paramsp->mSkinInfo.notNull() &&
			(paramsp->mAvatar.get() != last_avatarp ||
			 paramsp->mSkinInfo->mHash != last_hash))
		{
#if 0
			if (!uploadMatrixPalette(*paramsp))
			{
				continue;
			}
#else
			uploadMatrixPalette(*paramsp);
#endif
			last_avatarp = paramsp->mAvatar.get();
			last_hash = paramsp->mSkinInfo->mHash;
		}

		pushUntexturedBatch(*paramsp);
	}
}

// Used only by the PBR renderer
void LLRenderPass::pushUntexturedBatch(LLDrawInfo& params)
{
	if (params.mCount)
	{
		applyModelMatrix(params);
		params.mVertexBuffer->setBuffer();
		params.mVertexBuffer->drawRange(LLRender::TRIANGLES, params.mStart,
										params.mEnd, params.mCount,
										params.mOffset);
	}
}

// Used only by the PBR renderer
void LLRenderPass::pushUntexturedGLTFBatches(U32 type)
{
	if (!gPipeline.sCull)
	{
		// Paranoia (sCull != NULL needed for getRenderMap())
		return;
	}

	LLCullResult::drawinfo_list_t& draw_list = gPipeline.getRenderMap(type);
	for (U32 i = 0, count = draw_list.size(); i < count; )
	{
		LLDrawInfo* paramsp = draw_list[i++];

		// Draw info cache prefetching optimization.
		if (i < count)
		{
			_mm_prefetch((char*)draw_list[i]->mVertexBuffer.get(),
						 _MM_HINT_NTA);
			if (i + 1 < count)
			{
				_mm_prefetch((char*)draw_list[i + 1], _MM_HINT_NTA);
			}
		}

		pushUntexturedGLTFBatch(*paramsp);
	}	
}

// Used only by the PBR renderer
void LLRenderPass::pushGLTFBatches(U32 type)
{
	if (!gPipeline.sCull)
	{
		// Paranoia (sCull != NULL needed for getRenderMap())
		return;
	}

	LLCullResult::drawinfo_list_t& draw_list = gPipeline.getRenderMap(type);
	for (U32 i = 0, count = draw_list.size(); i < count; )
	{
		LLDrawInfo* paramsp = draw_list[i++];

		// Draw info cache prefetching optimization.
		if (i < count)
		{
			_mm_prefetch((char*)draw_list[i]->mVertexBuffer.get(),
						 _MM_HINT_NTA);
			if (i + 1 < count)
			{
				_mm_prefetch((char*)draw_list[i + 1], _MM_HINT_NTA);
			}
		}

		pushGLTFBatch(*paramsp);
	}
}

// Used only by the PBR renderer
void LLRenderPass::pushGLTFBatch(LLDrawInfo& params)
{
	LLPointer<LLFetchedGLTFMaterial>& matp = params.mGLTFMaterial;
	matp->bind(params.mTexture, params.mVSize);

	LLGLDisable cull_face(matp->mDoubleSided ? GL_CULL_FACE : 0);

	bool has_tex_matrix = params.mTextureMatrix != NULL;
	if (has_tex_matrix)
	{
		// Special case implementation of texture animation here because of
		// special handling of textures for PBR batches.
		gGL.getTexUnit(0)->activate();
		gGL.matrixMode(LLRender::MM_TEXTURE);
		gGL.loadMatrix(params.mTextureMatrix->getF32ptr());
		++gPipeline.mTextureMatrixOps;
	}

	applyModelMatrix(params);

	params.mVertexBuffer->setBuffer();
	params.mVertexBuffer->drawRange(LLRender::TRIANGLES, params.mStart,
									params.mEnd, params.mCount,
									params.mOffset);
									
	if (has_tex_matrix)
	{
		gGL.matrixMode(LLRender::MM_TEXTURE0);
		gGL.loadIdentity();
		gGL.matrixMode(LLRender::MM_MODELVIEW);
	}
}

// Used only by the PBR renderer
void LLRenderPass::pushUntexturedGLTFBatch(LLDrawInfo& params)
{
	LLFetchedGLTFMaterial* matp = params.mGLTFMaterial.get();
	if (!matp) return;	// Paranoia

	LLGLDisable cull_face(matp->mDoubleSided ? GL_CULL_FACE : 0);

	applyModelMatrix(params);

	params.mVertexBuffer->setBuffer();
	params.mVertexBuffer->drawRange(LLRender::TRIANGLES, params.mStart,
									params.mEnd, params.mCount,
									params.mOffset);
}

// Used only by the PBR renderer
void LLRenderPass::pushRiggedGLTFBatches(U32 type)
{
	if (!gPipeline.sCull)
	{
		// Paranoia (sCull != NULL needed for getRenderMap())
		return;
	}

	LLVOAvatar* last_avatarp = NULL;
	U64 last_hash = 0;

	LLCullResult::drawinfo_list_t& draw_list = gPipeline.getRenderMap(type);
	for (U32 i = 0, count = draw_list.size(); i < count; )
	{
		LLDrawInfo* paramsp = draw_list[i++];

		// Draw info cache prefetching optimization.
		if (i < count)
		{
			_mm_prefetch((char*)draw_list[i]->mVertexBuffer.get(),
						 _MM_HINT_NTA);
			if (i + 1 < count)
			{
				_mm_prefetch((char*)draw_list[i + 1], _MM_HINT_NTA);
			}
		}

		if (paramsp->mAvatar.notNull() && paramsp->mSkinInfo.notNull() &&
			(paramsp->mAvatar.get() != last_avatarp ||
			 paramsp->mSkinInfo->mHash != last_hash))
		{
#if 0
			if (!uploadMatrixPalette(*paramsp))
			{
				continue;
			}
#else
			uploadMatrixPalette(*paramsp);
#endif
			last_avatarp = paramsp->mAvatar.get();
			last_hash = paramsp->mSkinInfo->mHash;
		}

		pushGLTFBatch(*paramsp);
	}
}

// Used only by the PBR renderer
void LLRenderPass::pushUntexturedRiggedGLTFBatches(U32 type)
{
	if (!gPipeline.sCull)
	{
		// Paranoia (sCull != NULL needed for getRenderMap())
		return;
	}

	LLVOAvatar* last_avatarp = NULL;
	U64 last_hash = 0;

	LLCullResult::drawinfo_list_t& draw_list = gPipeline.getRenderMap(type);
	for (U32 i = 0, count = draw_list.size(); i < count; )
	{
		LLDrawInfo* paramsp = draw_list[i++];

		// Draw info cache prefetching optimization.
		if (i < count)
		{
			_mm_prefetch((char*)draw_list[i]->mVertexBuffer.get(),
						 _MM_HINT_NTA);
			if (i + 1 < count)
			{
				_mm_prefetch((char*)draw_list[i + 1], _MM_HINT_NTA);
			}
		}

		if (paramsp->mAvatar.notNull() && paramsp->mSkinInfo.notNull() &&
			(paramsp->mAvatar.get() != last_avatarp ||
			 paramsp->mSkinInfo->mHash != last_hash))
		{
#if 0
			if (!uploadMatrixPalette(*paramsp))
			{
				continue;
			}
#else
			uploadMatrixPalette(*paramsp);
#endif
			last_avatarp = paramsp->mAvatar.get();
			last_hash = paramsp->mSkinInfo->mHash;
		}

		pushUntexturedGLTFBatch(*paramsp);
	}
}

//static
void LLRenderPass::applyModelMatrix(LLDrawInfo& params)
{
	if (params.mModelMatrix != gGLLastMatrix)
	{
		gGLLastMatrix = params.mModelMatrix;
		gGL.matrixMode(LLRender::MM_MODELVIEW);
		gGL.loadMatrix(gGLModelView.getF32ptr());
		if (params.mModelMatrix)
		{
			gGL.multMatrix(params.mModelMatrix->getF32ptr());
		}
		++gPipeline.mMatrixOpCount;
	}
}

//static
bool LLRenderPass::uploadMatrixPalette(const LLDrawInfo& params)
{
	return uploadMatrixPalette(params.mAvatar, params.mSkinInfo);
}

//static
bool LLRenderPass::uploadMatrixPalette(LLVOAvatar* avp, LLMeshSkinInfo* skinp)
{
	if (!skinp || !avp || avp->isDead())
	{
		return false;
	}

	U32 count = 0;
	const F32* mp = avp->getRiggedMatrix(skinp, count);
	if (!count)	// Render only after skin info has loaded
	{
		return false;
	}

	LLGLSLShader* shaderp = LLGLSLShader::sCurBoundShaderPtr;
	if (!shaderp) // Paranoia
	{
		llwarns_sparse << "sCurBoundShaderPtr is NULL !" << llendl;
		llassert(false);
		return false;
	}
	shaderp->uniformMatrix3x4fv(LLShaderMgr::AVATAR_MATRIX, count, false, mp);
	return true;
}
