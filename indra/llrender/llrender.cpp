/**
 * @file llrender.cpp
 * @brief LLRender implementation
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
 * ----------------------------------------------------------------------------
 * LLRender::projectf & LLRender::unprojectf adapted from gluProject &
 * gluUnproject in Mesa's GLU 9.0 library. License/Copyright Statement:
 * SGI FREE SOFTWARE LICENSE B (Version 2.0, Sept. 18, 2008)
 * Copyright (C) 1991-2000 Silicon Graphics, Inc. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice including the dates of first publication and
 * either this permission notice or a reference to
 * http://oss.sgi.com/projects/FreeB/
 * shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * SILICON GRAPHICS, INC. BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Except as contained in this notice, the name of Silicon Graphics, Inc.
 * shall not be used in advertising or otherwise to promote the sale, use or
 * other dealings in this Software without prior written authorization from
 * Silicon Graphics, Inc.
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include "llrender.h"

#include "llcubemap.h"
#include "hbfastmap.h"
#include "llglslshader.h"
#include "llgltexture.h"
#include "llimagegl.h"
#include "llmatrix4a.h"
#include "llrendertarget.h"
#include "llshadermgr.h"
#include "llthread.h"				// For *_main_thread()
#include "hbtracy.h"
#include "llvertexbuffer.h"
#include "hbxxh.h"

#if !GL_ARB_vertex_array_object && LL_CLANG
// mDummyVAO is unused when there is no support for GL_ARB_vertex_array_object
// (happens with macOS). So disable the corresponding warning in clang. HB
# pragma clang diagnostic ignored "-Wunused-private-field"
#endif

thread_local LLRender gGL;

// Handy copies of last good GL matrices
LLMatrix4a gGLModelView;
LLMatrix4a gGLLastModelView;
LLMatrix4a gGLDeltaModelView;
LLMatrix4a gGLInverseDeltaModelView;
LLMatrix4a gGLLastProjection;
LLMatrix4a gGLProjection;
S32	gGLViewport[4];

U32 LLTexUnit::sWhiteTexture = 0;
U32 LLRender::sCurrentFrame = 0;
bool LLRender::sGLCoreProfile = false;
bool LLRender::sUseBufferCache = false;

static const GLenum sGLTextureType[] =
{
	GL_TEXTURE_2D,
	GL_TEXTURE_RECTANGLE,
	GL_TEXTURE_CUBE_MAP,
	GL_TEXTURE_CUBE_MAP_ARRAY,
	GL_TEXTURE_2D_MULTISAMPLE,
};

static const S32 sGLAddressMode[] =
{
	GL_REPEAT,
	GL_MIRRORED_REPEAT,
	GL_CLAMP_TO_EDGE
};

constexpr U32 immediate_mask = LLVertexBuffer::MAP_VERTEX |
							   LLVertexBuffer::MAP_COLOR |
							   LLVertexBuffer::MAP_TEXCOORD0;

static const GLenum sGLBlendFactor[] =
{
	GL_ONE,
	GL_ZERO,
	GL_DST_COLOR,
	GL_SRC_COLOR,
	GL_ONE_MINUS_DST_COLOR,
	GL_ONE_MINUS_SRC_COLOR,
	GL_DST_ALPHA,
	GL_SRC_ALPHA,
	GL_ONE_MINUS_DST_ALPHA,
	GL_ONE_MINUS_SRC_ALPHA,

	GL_ZERO // 'BF_UNDEF'
};

///////////////////////////////////////////////////////////////////////////////
// LLVBCache class: a cache for vertex buffers
///////////////////////////////////////////////////////////////////////////////

class LLVBCache
{
public:
	LLVBCache(LLVertexBuffer* vb)
	:	mVB(vb),
		mLastFrameSeen(LLRender::sCurrentFrame)
	{
	}

	LL_INLINE LLVertexBuffer* get()
	{
		mLastFrameSeen = LLRender::sCurrentFrame;
		return mVB.get();
	}

	LL_INLINE bool expired() const
	{
		constexpr U32 MAX_FRAME_AGE = 120;
		return LLRender::sCurrentFrame - mLastFrameSeen > MAX_FRAME_AGE;
	}

private:
	LLPointer<LLVertexBuffer>	mVB;
	U32							mLastFrameSeen;
};

static flat_hmap<U64, LLVBCache> sVBCache;

///////////////////////////////////////////////////////////////////////////////
// LLTexUnit class
///////////////////////////////////////////////////////////////////////////////

LLTexUnit::LLTexUnit(S32 index)
:	mCurrTexType(TT_NONE),
	mCurrColorScale(1),
	mCurrAlphaScale(1),
	mCurrTexture(0),
	mTexColorSpace(TCS_LINEAR),
	mHasMipMaps(false),
	mIndex(index)
{
	llassert_always(index < (S32)LL_NUM_TEXTURE_LAYERS);
}

//static
U32 LLTexUnit::getInternalType(eTextureType type)
{
	return sGLTextureType[type];
}

void LLTexUnit::refreshState()
{
	// We set dirty to true so that the tex unit knows to ignore caching and we
	// reset the cached tex unit state

	gGL.flush();

	glActiveTexture(GL_TEXTURE0 + mIndex);

	if (mCurrTexType != TT_NONE)
	{
		glBindTexture(sGLTextureType[mCurrTexType], mCurrTexture);
	}
	else
	{
		glBindTexture(GL_TEXTURE_2D, 0);
	}

	setTextureColorSpace(mTexColorSpace);
}

void LLTexUnit::activate()
{
	if (mIndex < 0) return;

	if ((S32)gGL.mCurrTextureUnitIndex != mIndex || gGL.mDirty)
	{
#if 0
		gGL.flush();
#endif
		glActiveTexture(GL_TEXTURE0 + mIndex);
		gGL.mCurrTextureUnitIndex = mIndex;
	}
}

void LLTexUnit::enable(eTextureType type)
{
	if (mIndex < 0) return;

	if (type != TT_NONE && (mCurrTexType != type || gGL.mDirty))
	{
		activate();
		if (mCurrTexType != TT_NONE && !gGL.mDirty)
		{
			// Force a disable of a previous texture type if it is enabled:
			disable();
		}
		mCurrTexType = type;

		gGL.flush();
	}
}

void LLTexUnit::disable()
{
	if (mIndex < 0) return;

	if (mCurrTexType != TT_NONE)
	{
		activate();
		unbind(mCurrTexType);
		gGL.flush();
		setTextureColorSpace(TCS_LINEAR);
		mCurrTexType = TT_NONE;
	}
}

void LLTexUnit::bindFast(LLGLTexture* gltexp)
{
	LLImageGL* glimagep = gltexp->getGLImage();
	gltexp->setActive();
	glActiveTexture(GL_TEXTURE0 + mIndex);
	gGL.mCurrTextureUnitIndex = mIndex;
	mCurrTexture = glimagep->getTexName();
	mHasMipMaps = glimagep->mHasMipMaps;
	// Note: LL's code is bogus here, since when mCurrTexture is 0, it binds
	// the texture twice (and the second time to mCurrTexture which is 0)...
	// This is why I reverted the test and added the return (also favouring
	// tail call optimization in the process). HB
	if (mCurrTexture)
	{
		glBindTexture(sGLTextureType[glimagep->getTarget()], mCurrTexture);
		return;
	}
	// If deleted, will re-generate it immediately
	gltexp->forceImmediateUpdate();
	glimagep->forceUpdateBindStats();
	gltexp->bindDefaultImage(mIndex);
}

bool LLTexUnit::bind(LLGLTexture* gltexp, bool force_bind)
{
	stop_glerror();
	if (mIndex < 0) return false;

	LLImageGL* glimagep = NULL;
	if (!gltexp || !(glimagep = gltexp->getGLImage()))
	{
		if (gltexp)
		{
			llwarns << "getGLImage() returned NULL" << llendl;
		}
		else
		{
			llwarns << "NULL texture (1)" << llendl;
		}
		return false;
	}

	if (glimagep->getTexName()) // If texture exists
	{
		// In audit, replace the selected texture by the default one.
		if (force_bind || mCurrTexture != glimagep->getTexName())
		{
			gGL.flush();
			activate();
			enable(glimagep->getTarget());
			mCurrTexture = glimagep->getTexName();
			glBindTexture(sGLTextureType[glimagep->getTarget()], mCurrTexture);
			if (glimagep->updateBindStats())
			{
				gltexp->setActive();
			}
			mHasMipMaps = glimagep->mHasMipMaps;
			if (glimagep->mTexOptionsDirty)
			{
				glimagep->mTexOptionsDirty = false;
				setTextureAddressMode(glimagep->mAddressMode);
				setTextureFilteringOption(glimagep->mFilterOption);
			}
			setTextureColorSpace(mTexColorSpace);
		}

		return true;
	}

	// If deleted, will re-generate it immediately
	gltexp->forceImmediateUpdate();
	glimagep->forceUpdateBindStats();
	return gltexp->bindDefaultImage(mIndex);
}

bool LLTexUnit::bind(LLImageGL* glimagep, bool force_bind, S32 usename)
{
	stop_glerror();
	if (mIndex < 0) return false;

	U32 texname = usename ? usename : glimagep->getTexName();
	if (!glimagep)
	{
		llwarns << "NULL texture (2)" << llendl;
		return false;
	}

	if (!texname)
	{
		if (LLImageGL::sDefaultGLImagep &&
			LLImageGL::sDefaultGLImagep->getTexName())
		{
			return bind(LLImageGL::sDefaultGLImagep);
		}
		return false;
	}

	if (force_bind || mCurrTexture != texname)
	{
		gGL.flush();
		activate();
		enable(glimagep->getTarget());
		mCurrTexture = texname;
		glBindTexture(sGLTextureType[glimagep->getTarget()], mCurrTexture);
		glimagep->updateBindStats();
		mHasMipMaps = glimagep->mHasMipMaps;
		if (glimagep->mTexOptionsDirty)
		{
			glimagep->mTexOptionsDirty = false;
			setTextureAddressMode(glimagep->mAddressMode);
			setTextureFilteringOption(glimagep->mFilterOption);
		}
		setTextureColorSpace(mTexColorSpace);
	}

	stop_glerror();

	return true;
}

bool LLTexUnit::bind(LLCubeMap* cube_map)
{
	if (mIndex < 0) return false;

	if (!cube_map)
	{
		llwarns << "NULL cubemap" << llendl;
		return false;
	}

	if (mCurrTexture == cube_map->mImages[0]->getTexName())
	{
		return true;
	}

	gGL.flush();
	activate();
	enable(LLTexUnit::TT_CUBE_MAP);
	mCurrTexture = cube_map->mImages[0]->getTexName();
	glBindTexture(GL_TEXTURE_CUBE_MAP, mCurrTexture);
	mHasMipMaps = cube_map->mImages[0]->mHasMipMaps;
	cube_map->mImages[0]->updateBindStats();
	if (cube_map->mImages[0]->mTexOptionsDirty)
	{
		cube_map->mImages[0]->mTexOptionsDirty = false;
		setTextureAddressMode(cube_map->mImages[0]->mAddressMode);
		setTextureFilteringOption(cube_map->mImages[0]->mFilterOption);
	}
	setTextureColorSpace(mTexColorSpace);

	return true;
}

bool LLTexUnit::bind(LLRenderTarget* targetp, bool bind_depth)
{
	if (mIndex < 0) return false;

	if (bind_depth)
	{
		if (targetp->hasStencil())
		{
			llwarns_sparse << "Cannot bind. Allocate render target without a stencil buffer."
						   << llendl;
			llassert_always(!gDebugGL);
			return false;
		}
		U32 depth = targetp->getDepth();
		if (!depth)
		{
			llwarns_sparse << "Cannot bind. Allocate render target with a depth buffer."
						   << llendl;
			llassert_always(!gDebugGL);
			return false;
		}
		bindManual(targetp->getUsage(), depth);
	}
	else
	{
		bindManual(targetp->getUsage(), targetp->getTexture());
	}

	return true;
}

bool LLTexUnit::bindManual(eTextureType type, U32 texture, bool has_mips)
{
	if (mIndex < 0)
	{
		return false;
	}

	if (mCurrTexture != texture)
	{
		gGL.flush();
		activate();
		enable(type);
		mCurrTexture = texture;
		glBindTexture(sGLTextureType[type], texture);
		mHasMipMaps = has_mips;
		setTextureColorSpace(mTexColorSpace);
	}
	return true;
}

void LLTexUnit::unbind(eTextureType type)
{
	stop_glerror();

	if (mIndex < 0) return;

#if 0	// Always flush and activate for consistency since some code paths
		// assume unbind always flushes and sets the active texture.
	if (gGL.mCurrTextureUnitIndex != (U32)mIndex || gGL.mDirty)
#endif
	{
		gGL.flush();
		activate();
	}

	// Disabled caching of binding state.
	if (mCurrTexType == type && mCurrTexture != 0)
	{
		mCurrTexture = 0;

		// Always make sure our texture color space is reset to linear. SRGB
		// sampling should be opt-in in the vast majority of cases. This also
		// prevents color space "popping".
		mTexColorSpace = TCS_LINEAR;

		if (type == LLTexUnit::TT_TEXTURE)
		{
			glBindTexture(sGLTextureType[type], sWhiteTexture);
		}
		else
		{
			glBindTexture(sGLTextureType[type], 0);
		}
		stop_glerror();
	}
}

void LLTexUnit::unbindFast(eTextureType type)
{
	activate();
	// Disabled caching of binding state.
	if (mCurrTexType == type)
	{
		mCurrTexture = 0;

		// Always make sure our texture color space is reset to linear.
		// SRGB sampling should be opt-in in the vast majority of cases.
		// Also prevents color space "popping".
		mTexColorSpace = TCS_LINEAR;
		if (type == TT_TEXTURE)
		{
			glBindTexture(sGLTextureType[type], sWhiteTexture);
		}
		else
		{
			glBindTexture(sGLTextureType[type], 0);
		}
	}
}

void LLTexUnit::setTextureAddressMode(eTextureAddressMode mode)
{
	if (mIndex < 0 || mCurrTexture == 0) return;

	gGL.flush();
	activate();

	glTexParameteri(sGLTextureType[mCurrTexType], GL_TEXTURE_WRAP_S,
					sGLAddressMode[mode]);
	glTexParameteri(sGLTextureType[mCurrTexType], GL_TEXTURE_WRAP_T,
					sGLAddressMode[mode]);
	if (mCurrTexType == TT_CUBE_MAP)
	{
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R,
						sGLAddressMode[mode]);
	}
}

void LLTexUnit::setTextureFilteringOption(LLTexUnit::eTextureFilterOptions option)
{
	if (mIndex < 0 || mCurrTexture == 0 ||
		mCurrTexType == TT_MULTISAMPLE_TEXTURE)
	{
		return;
	}

	gGL.flush();

	if (option == TFO_POINT)
	{
		glTexParameteri(sGLTextureType[mCurrTexType], GL_TEXTURE_MAG_FILTER,
						GL_NEAREST);
	}
	else
	{
		glTexParameteri(sGLTextureType[mCurrTexType], GL_TEXTURE_MAG_FILTER,
						GL_LINEAR);
	}

	if (option >= TFO_TRILINEAR && mHasMipMaps)
	{
		glTexParameteri(sGLTextureType[mCurrTexType],
						GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	}
	else if (option >= TFO_BILINEAR)
	{
		if (mHasMipMaps)
		{
			glTexParameteri(sGLTextureType[mCurrTexType],
							GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_NEAREST);
		}
		else
		{
			glTexParameteri(sGLTextureType[mCurrTexType],
							GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		}
	}
	else if (mHasMipMaps)
	{
		glTexParameteri(sGLTextureType[mCurrTexType], GL_TEXTURE_MIN_FILTER,
						GL_NEAREST_MIPMAP_NEAREST);
	}
	else
	{
		glTexParameteri(sGLTextureType[mCurrTexType], GL_TEXTURE_MIN_FILTER,
						GL_NEAREST);
	}

	if (gGLManager.mHasAnisotropic)
	{
		F32 anisotropy = 1.f;
		if (LLImageGL::sGlobalUseAnisotropic && option == TFO_ANISOTROPIC)
		{
			anisotropy = gGLManager.mMaxAnisotropy;
		}
		glTexParameterf(sGLTextureType[mCurrTexType],
						GL_TEXTURE_MAX_ANISOTROPY, anisotropy);
	}
}

S32 LLTexUnit::getTextureSource(eTextureBlendSrc src)
{
	switch (src)
	{
		// All four cases should return the same value.
		case TBS_PREV_COLOR:
		case TBS_PREV_ALPHA:
		case TBS_ONE_MINUS_PREV_COLOR:
		case TBS_ONE_MINUS_PREV_ALPHA:
			return GL_PREVIOUS;

		// All four cases should return the same value.
		case TBS_TEX_COLOR:
		case TBS_TEX_ALPHA:
		case TBS_ONE_MINUS_TEX_COLOR:
		case TBS_ONE_MINUS_TEX_ALPHA:
			return GL_TEXTURE;

		// All four cases should return the same value.
		case TBS_VERT_COLOR:
		case TBS_VERT_ALPHA:
		case TBS_ONE_MINUS_VERT_COLOR:
		case TBS_ONE_MINUS_VERT_ALPHA:
			return GL_PRIMARY_COLOR;

		// All four cases should return the same value.
		case TBS_CONST_COLOR:
		case TBS_CONST_ALPHA:
		case TBS_ONE_MINUS_CONST_COLOR:
		case TBS_ONE_MINUS_CONST_ALPHA:
			return GL_CONSTANT;

		default:
			llwarns << "Unknown eTextureBlendSrc: " << src
					<< ". Using Vertex Color instead." << llendl;
			return GL_PRIMARY_COLOR;
	}
}

S32 LLTexUnit::getTextureSourceType(eTextureBlendSrc src, bool is_alpha)
{
	switch (src)
	{
		// All four cases should return the same value.
		case TBS_PREV_COLOR:
		case TBS_TEX_COLOR:
		case TBS_VERT_COLOR:
		case TBS_CONST_COLOR:
			return is_alpha ? GL_SRC_ALPHA: GL_SRC_COLOR;

		// All four cases should return the same value.
		case TBS_PREV_ALPHA:
		case TBS_TEX_ALPHA:
		case TBS_VERT_ALPHA:
		case TBS_CONST_ALPHA:
			return GL_SRC_ALPHA;

		// All four cases should return the same value.
		case TBS_ONE_MINUS_PREV_COLOR:
		case TBS_ONE_MINUS_TEX_COLOR:
		case TBS_ONE_MINUS_VERT_COLOR:
		case TBS_ONE_MINUS_CONST_COLOR:
			return is_alpha ? GL_ONE_MINUS_SRC_ALPHA : GL_ONE_MINUS_SRC_COLOR;

		// All four cases should return the same value.
		case TBS_ONE_MINUS_PREV_ALPHA:
		case TBS_ONE_MINUS_TEX_ALPHA:
		case TBS_ONE_MINUS_VERT_ALPHA:
		case TBS_ONE_MINUS_CONST_ALPHA:
			return GL_ONE_MINUS_SRC_ALPHA;

		default:
			llwarns << "Unknown eTextureBlendSrc: " << src
					<< ". Using Source Color or Alpha instead." << llendl;
			return is_alpha ? GL_SRC_ALPHA: GL_SRC_COLOR;
	}
}

void LLTexUnit::setColorScale(S32 scale)
{
	if (mCurrColorScale != scale || gGL.mDirty)
	{
		mCurrColorScale = scale;
		gGL.flush();
		glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE, scale);
	}
}

void LLTexUnit::setAlphaScale(S32 scale)
{
	if (mCurrAlphaScale != scale || gGL.mDirty)
	{
		mCurrAlphaScale = scale;
		gGL.flush();
		glTexEnvi(GL_TEXTURE_ENV, GL_ALPHA_SCALE, scale);
	}
}

// Useful for debugging that you have manually assigned a texture operation to
// the correct texture unit based on the currently set active texture in
// openGL.
void LLTexUnit::debugTextureUnit()
{
	if (mIndex < 0) return;

	GLint active_texture;
	glGetIntegerv(GL_ACTIVE_TEXTURE, &active_texture);
	if (GL_TEXTURE0 + mIndex != active_texture)
	{
		U32 set_unit = (active_texture - GL_TEXTURE0);
		llwarns << "Incorrect Texture Unit!  Expected: " << set_unit
				<< " Actual: " << mIndex << llendl;
	}
}

///////////////////////////////////////////////////////////////////////////////
// LLLightState class
///////////////////////////////////////////////////////////////////////////////

LLLightState::LLLightState(S32 index)
:	mIndex(index),
	mEnabled(false),
	mSunIsPrimary(true),
	mConstantAtten(1.f),
	mLinearAtten(0.f),
	mQuadraticAtten(0.f),
	mSpotExponent(0.f),
	mSpotCutoff(180.f),
	mSize(0.f),
	mFalloff(0.f)
{
	if (mIndex == 0)
	{
		mDiffuse.set(1.f, 1.f, 1.f, 1.f);
		mDiffuseB.set(0.f, 0.f, 0.f, 0.f);
		mSpecular.set(1.f, 1.f, 1.f, 1.f);
	}

	mAmbient.set(0.f, 0.f, 0.f, 1.f);
	mPosition.set(0.f, 0.f, 1.f, 0.f);
	mSpotDirection.set(0.f, 0.f, -1.f);
}

void LLLightState::enable()
{
	if (!mEnabled)
	{
		mEnabled = true;
	}
}

void LLLightState::disable()
{
	if (mEnabled)
	{
		mEnabled = false;
	}
}

void LLLightState::setDiffuse(const LLColor4& diffuse)
{
	if (mDiffuse != diffuse)
	{
		++gGL.mLightHash;
		mDiffuse = diffuse;
	}
}

void LLLightState::setDiffuseB(const LLColor4& diffuse)
{
	if (mDiffuseB != diffuse)
	{
		++gGL.mLightHash;
		mDiffuseB = diffuse;
	}
}

void LLLightState::setSunPrimary(bool b)
{
	if (mSunIsPrimary != b)
	{
		++gGL.mLightHash;
		mSunIsPrimary = b;
	}
}

void LLLightState::setSize(F32 size)
{
	if (mSize != size)
	{
		++gGL.mLightHash;
		mSize = size;
	}
}

void LLLightState::setFalloff(F32 falloff)
{
	if (mFalloff != falloff)
	{
		++gGL.mLightHash;
		mFalloff = falloff;
	}
}

void LLLightState::setAmbient(const LLColor4& ambient)
{
	if (mAmbient != ambient)
	{
		++gGL.mLightHash;
		mAmbient = ambient;
	}
}

void LLLightState::setSpecular(const LLColor4& specular)
{
	if (mSpecular != specular)
	{
		++gGL.mLightHash;
		mSpecular = specular;
	}
}

void LLLightState::setPosition(const LLVector4& position)
{
	++gGL.mLightHash;

	// Always set position because modelview matrix may have changed
	mPosition = position;
	LLVector4a pos;
	pos.loadua(position.mV);

	// Transform position by current modelview matrix
	gGL.getModelviewMatrix().rotate4(pos, pos);

	mPosition.set(pos.getF32ptr());
}

void LLLightState::setConstantAttenuation(F32 atten)
{
	if (mConstantAtten != atten)
	{
		mConstantAtten = atten;
		++gGL.mLightHash;
	}
}

void LLLightState::setLinearAttenuation(F32 atten)
{
	if (mLinearAtten != atten)
	{
		++gGL.mLightHash;
		mLinearAtten = atten;
	}
}

void LLLightState::setQuadraticAttenuation(F32 atten)
{
	if (mQuadraticAtten != atten)
	{
		++gGL.mLightHash;
		mQuadraticAtten = atten;
	}
}

void LLLightState::setSpotExponent(F32 exponent)
{
	if (mSpotExponent != exponent)
	{
		++gGL.mLightHash;
		mSpotExponent = exponent;
	}
}

void LLLightState::setSpotCutoff(F32 cutoff)
{
	if (mSpotCutoff != cutoff)
	{
		++gGL.mLightHash;
		mSpotCutoff = cutoff;
	}
}

void LLLightState::setSpotDirection(const LLVector3& direction)
{
	++gGL.mLightHash;

	// Always set direction because modelview matrix may have changed
	mSpotDirection = direction;

	// Transform direction by current modelview matrix
	LLVector4a dir;
	dir.load3(direction.mV);

	gGL.getModelviewMatrix().rotate(dir, dir);

	mSpotDirection.set(dir.getF32ptr());
}

///////////////////////////////////////////////////////////////////////////////
// LLRender class
///////////////////////////////////////////////////////////////////////////////

LLRender::LLRender()
:	mDummyVAO(0),
	mValid(false),
	mDirty(false),
	mCacheMissCount(0),
	mCount(0),
	mMode(TRIANGLES),
    mCurrTextureUnitIndex(0),
	mLightHash(0),
	mFrozenLights(false),
	mCurrBlendColorSFactor(BF_UNDEF),
	mCurrBlendAlphaSFactor(BF_UNDEF),
	mCurrBlendColorDFactor(BF_UNDEF),
	mCurrBlendAlphaDFactor(BF_UNDEF),
	mMatrixMode(MM_MODELVIEW)
{
	mTexUnits.reserve(LL_NUM_TEXTURE_LAYERS);
	for (U32 i = 0; i < LL_NUM_TEXTURE_LAYERS; ++i)
	{
		mTexUnits.push_back(new LLTexUnit(i));
	}
	mDummyTexUnit = new LLTexUnit(-1);

	for (U32 i = 0; i < LL_NUM_LIGHT_UNITS; ++i)
	{
		mLightState.push_back(new LLLightState(i));
	}

	for (U32 i = 0; i < 4; ++i)
	{
		mCurrColorMask[i] = true;
	}

	for (U32 i = 0; i < NUM_MATRIX_MODES; ++i)
	{
		mMatIdx[i] = 0;
		mMatHash[i] = 0;
		mCurMatHash[i] = 0xFFFFFFFF;
	}

	// Init base matrix for each mode
	for (U32 i = 0; i < NUM_MATRIX_MODES; ++i)
	{
		mMatrix[i][0].setIdentity();
	}

	gGLModelView.setIdentity();
	gGLLastModelView.setIdentity();
	gGLProjection.setIdentity();
	gGLLastProjection.setIdentity();
}

LLRender::~LLRender()
{
	shutdown();
}

//static
void APIENTRY LLRender::debugCallback(GLenum, GLenum type, GLuint id,
									  GLenum severity, GLsizei,
									  const GLchar* message, GLvoid*)
{
	// We only care about GL errors, and only while debugging GL
	if (gDebugGL && severity == GL_DEBUG_SEVERITY_HIGH)
	{
		llwarns << "GL error type: " << std::hex << type << " - Id: " << id
				<< std::dec << " - Message: " << message << llendl;
	}
}

void LLRender::init()
{
#if GL_ARB_debug_output
	// Note: since init() is called at viewer startup, this callback will only
	// be setup when gDebugGL is initialized to true via the "DebugGLOnRestart"
	// debug setting, set in the previous viewer session or via the command
	// line of the current session. HB
	if (gDebugGL && gGLManager.mHasDebugOutput)
	{
		llinfos << "Setting up GL debug callback." << llendl;
		glDebugMessageCallback((GLDEBUGPROC)LLRender::debugCallback, NULL);
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	}
#endif

	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	gGL.setSceneBlendType(LLRender::BT_ALPHA);
	gGL.setAmbientLightColor(LLColor4::black);
	glCullFace(GL_BACK);
	if (gGLManager.mGLVersion >= 3.2f)
	{
		// Necessary for reflection maps
		glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
	}

	// Vertex buffers (and the VBO pools they use) are not thread-safe, so we
	// *cannot* use mBuffer from GL threads !  Should we need it in the future
	// in shared GL profiles, then both LLVertexBuffer and LLVBOPool would need
	// to have their static variables turned into thread_local ones. HB
	if (is_main_thread())
	{
#if GL_ARB_vertex_array_object
		if (sGLCoreProfile && gGLManager.mHasVertexArrayObject)
		{
			if (mDummyVAO)
			{
				glBindVertexArray(0);
				glDeleteVertexArrays(1, &mDummyVAO);
				mDummyVAO = 0;
			}
			// Bind a dummy vertex array object so that we are compliant with
			// the core GL profile.
			glGenVertexArrays(1, &mDummyVAO);
			glBindVertexArray(mDummyVAO);
		}
#endif
		initVertexBuffer();
	}

	stop_glerror();

	mValid = true;
}

void LLRender::initVertexBuffer()
{
	assert_main_thread();

	llassert_always(mBuffer.isNull());
	mBuffer = new LLVertexBuffer(immediate_mask);
	mBuffer->allocateBuffer(4096, 0);
	mBuffer->getVertexStrider(mVerticesp);
	mBuffer->getTexCoord0Strider(mTexcoordsp);
	mBuffer->getColorStrider(mColorsp);
#if LL_DEBUG_VB_ALLOC
	mBuffer->setOwner("LLRender");
#endif
}

void LLRender::resetVertexBuffer()
{
	mBuffer = NULL;
	if (is_main_thread())
	{
		sVBCache.clear();
	}
}

void LLRender::shutdown()
{
	stop_glerror();

	mValid = false;

	for (U32 i = 0, count = mTexUnits.size(); i < count; ++i)
	{
		delete mTexUnits[i];
	}
	mTexUnits.clear();
	delete mDummyTexUnit;
	mDummyTexUnit = NULL;

	for (U32 i = 0, count = mLightState.size(); i < count; ++i)
	{
		delete mLightState[i];
	}
	mLightState.clear();

	resetVertexBuffer();

#if GL_ARB_vertex_array_object
	if (mDummyVAO)
	{
		glBindVertexArray(0);
		glDeleteVertexArrays(1, &mDummyVAO);
		mDummyVAO = 0;
	}
#endif
}

void LLRender::refreshState()
{
	mDirty = true;

	U32 active_unit = mCurrTextureUnitIndex;

	for (U32 i = 0, count = mTexUnits.size(); i < count; ++i)
	{
		mTexUnits[i]->refreshState();
	}

	mTexUnits[active_unit]->activate();

	setColorMask(mCurrColorMask[0], mCurrColorMask[1], mCurrColorMask[2],
				 mCurrColorMask[3]);

	mDirty = false;
}

void LLRender::syncLightState()
{
	LLGLSLShader* shaderp = LLGLSLShader::sCurBoundShaderPtr;
	if (!shaderp || shaderp->mLightHash == mLightHash ||
		// *HACK: to prevent lighting changes in preview shaders. HB
		(mFrozenLights && shaderp->mLightHash == 0))
	{
		return;
	}

	if (mFrozenLights)
	{
		// *HACK: to prevent lighting changes in preview shaders. HB
		shaderp->mLightHash = 0;
	}
	else
	{
		shaderp->mLightHash = mLightHash;
	}

	static LLVector4 position[LL_NUM_LIGHT_UNITS];
	static LLVector3 direction[LL_NUM_LIGHT_UNITS];
	static LLVector4 attenuation[LL_NUM_LIGHT_UNITS];
	static LLVector3 diffuse[LL_NUM_LIGHT_UNITS];
	static LLVector2 size[LL_NUM_LIGHT_UNITS];
	for (U32 i = 0; i < LL_NUM_LIGHT_UNITS; ++i)
	{
		LLLightState* lightp = mLightState[i];

		position[i] = lightp->mPosition;
		direction[i] = lightp->mSpotDirection;
		attenuation[i].set(lightp->mLinearAtten, lightp->mQuadraticAtten,
						   lightp->mSpecular.mV[2], lightp->mSpecular.mV[3]);
		diffuse[i].set(lightp->mDiffuse.mV);
		if (gUsePBRShaders)
		{
			size[i].set(lightp->mSize, lightp->mFalloff);
		}
	}

	shaderp->uniform4fv(LLShaderMgr::LIGHT_POSITION, LL_NUM_LIGHT_UNITS,
						position[0].mV);
	shaderp->uniform3fv(LLShaderMgr::LIGHT_DIRECTION, LL_NUM_LIGHT_UNITS,
						direction[0].mV);
	shaderp->uniform4fv(LLShaderMgr::LIGHT_ATTENUATION, LL_NUM_LIGHT_UNITS,
						attenuation[0].mV);
	shaderp->uniform3fv(LLShaderMgr::LIGHT_DIFFUSE, LL_NUM_LIGHT_UNITS,
						diffuse[0].mV);
	LLLightState* sunlightp = mLightState[0];
	shaderp->uniform1i(LLShaderMgr::SUN_UP_FACTOR,
					   sunlightp->mSunIsPrimary ? 1 : 0);
	if (gUsePBRShaders)
	{
		shaderp->uniform2fv(LLShaderMgr::LIGHT_DEFERRED_ATTENUATION,
							LL_NUM_LIGHT_UNITS, size[0].mV);
		shaderp->uniform3fv(LLShaderMgr::LIGHT_AMBIENT, 1,
							mAmbientLightColor.mV);
	}
	else
	{
		shaderp->uniform4fv(LLShaderMgr::LIGHT_AMBIENT, 1,
							mAmbientLightColor.mV);
		shaderp->uniform4fv(LLShaderMgr::AMBIENT, 1, mAmbientLightColor.mV);
		shaderp->uniform4fv(LLShaderMgr::SUNLIGHT_COLOR, 1,
							sunlightp->mDiffuse.mV);
		shaderp->uniform4fv(LLShaderMgr::MOONLIGHT_COLOR, 1,
							sunlightp->mDiffuseB.mV);
	}
}

void LLRender::syncMatrices()
{
	static const U32 name[] =
	{
		LLShaderMgr::MODELVIEW_MATRIX,
		LLShaderMgr::PROJECTION_MATRIX,
		LLShaderMgr::TEXTURE_MATRIX0,
		LLShaderMgr::TEXTURE_MATRIX1,
		LLShaderMgr::TEXTURE_MATRIX2,
		LLShaderMgr::TEXTURE_MATRIX3,
	};

	static LLMatrix4a cached_mvp;
	static LLMatrix4a cached_inv_mdv;
	static U32 cached_mvp_mdv_hash = 0xFFFFFFFF;
	static U32 cached_mvp_proj_hash = 0xFFFFFFFF;
	static LLMatrix4a cached_normal;
	static U32 cached_normal_hash = 0xFFFFFFFF;

	LLGLSLShader* shaderp = LLGLSLShader::sCurBoundShaderPtr;
	if (!shaderp)
	{
		return;
	}

	bool mvp_done = false;
	S32 loc;

	if (mMatHash[MM_MODELVIEW] != shaderp->mMatHash[MM_MODELVIEW])
	{
		// Update modelview, normal, and MVP
		const LLMatrix4a& mat = mMatrix[MM_MODELVIEW][mMatIdx[MM_MODELVIEW]];

		// If model view has changed, update the cached inverse as well
		if (cached_mvp_mdv_hash != mMatHash[MM_MODELVIEW])
		{
			cached_inv_mdv = mat;
			cached_inv_mdv.invert();
		}

		shaderp->uniformMatrix4fv(name[MM_MODELVIEW], 1, GL_FALSE,
								  mat.getF32ptr());
		shaderp->mMatHash[MM_MODELVIEW] = mMatHash[MM_MODELVIEW];

		// Update normal matrix
		loc = shaderp->getUniformLocation(LLShaderMgr::NORMAL_MATRIX);
		if (loc > -1)
		{
			if (cached_normal_hash != mMatHash[MM_MODELVIEW])
			{
				cached_normal = cached_inv_mdv;
				cached_normal.transpose();
				cached_normal_hash = mMatHash[MM_MODELVIEW];
			}

			const LLMatrix4a& norm = cached_normal;
			LLVector3 norms[3];
			norms[0].set(norm.getRow<0>().getF32ptr());
			norms[1].set(norm.getRow<1>().getF32ptr());
			norms[2].set(norm.getRow<2>().getF32ptr());
			shaderp->uniformMatrix3fv(LLShaderMgr::NORMAL_MATRIX, 1, GL_FALSE,
									  norms[0].mV);
		}

		if (shaderp->getUniformLocation(LLShaderMgr::INVERSE_MODELVIEW_MATRIX) > -1)
		{
			shaderp->uniformMatrix4fv(LLShaderMgr::INVERSE_MODELVIEW_MATRIX, 1,
									  GL_FALSE, cached_inv_mdv.getF32ptr());
		}

		// Update MVP matrix
		mvp_done = true;
		loc =
			shaderp->getUniformLocation(LLShaderMgr::MODELVIEW_PROJECTION_MATRIX);
		if (loc > -1)
		{
			if (cached_mvp_mdv_hash != mMatHash[MM_MODELVIEW] ||
				cached_mvp_proj_hash != mMatHash[MM_PROJECTION])
			{
				cached_mvp.setMul(mMatrix[MM_PROJECTION][mMatIdx[MM_PROJECTION]],
								  mat);
				cached_mvp_mdv_hash = mMatHash[MM_MODELVIEW];
				cached_mvp_proj_hash = mMatHash[MM_PROJECTION];
			}

			shaderp->uniformMatrix4fv(LLShaderMgr::MODELVIEW_PROJECTION_MATRIX,
									  1, GL_FALSE, cached_mvp.getF32ptr());
		}
	}

	if (mMatHash[MM_PROJECTION] != shaderp->mMatHash[MM_PROJECTION])
	{
		// Update projection matrix, normal, and MVP
		const LLMatrix4a& mat = mMatrix[MM_PROJECTION][mMatIdx[MM_PROJECTION]];

		if (gUsePBRShaders)
		{
			loc =
				shaderp->getUniformLocation(LLShaderMgr::INVERSE_PROJECTION_MATRIX);
			if (loc > -1)
			{
				LLMatrix4a inv_proj = mat;
				inv_proj.invert();
				shaderp->uniformMatrix4fv(LLShaderMgr::INVERSE_PROJECTION_MATRIX,
										  1, GL_FALSE, inv_proj.getF32ptr());
			}
		}

		shaderp->uniformMatrix4fv(name[MM_PROJECTION], 1, GL_FALSE,
								  mat.getF32ptr());
		shaderp->mMatHash[MM_PROJECTION] = mMatHash[MM_PROJECTION];

		if (!mvp_done)
		{
			// Update MVP matrix
			loc =
				shaderp->getUniformLocation(LLShaderMgr::MODELVIEW_PROJECTION_MATRIX);
			if (loc > -1)
			{
				if (cached_mvp_mdv_hash != mMatHash[MM_PROJECTION] ||
					cached_mvp_proj_hash != mMatHash[MM_PROJECTION])
				{
					cached_mvp.setMul(mat,
									  mMatrix[MM_MODELVIEW][mMatIdx[MM_MODELVIEW]]);
					cached_mvp_mdv_hash = mMatHash[MM_MODELVIEW];
					cached_mvp_proj_hash = mMatHash[MM_PROJECTION];
				}

				shaderp->uniformMatrix4fv(LLShaderMgr::MODELVIEW_PROJECTION_MATRIX,
										  1, GL_FALSE, cached_mvp.getF32ptr());
			}
		}
	}

	for (U32 i = MM_TEXTURE0; i < NUM_MATRIX_MODES; ++i)
	{
		U32 hash = mMatHash[i];
		if (shaderp->mMatHash[i] != hash)
		{
			shaderp->uniformMatrix4fv(name[i], 1, GL_FALSE,
									  mMatrix[i][mMatIdx[i]].getF32ptr());
			shaderp->mMatHash[i] = hash;
		}
	}

	if (shaderp->mFeatures.hasLighting ||
		shaderp->mFeatures.calculatesLighting ||
		shaderp->mFeatures.calculatesAtmospherics)
	{
		// Also sync light state
		syncLightState();
	}

	stop_glerror();
}

void LLRender::translatef(F32 x, F32 y, F32 z)
{
	if (fabsf(x) > F_APPROXIMATELY_ZERO || fabsf(y) > F_APPROXIMATELY_ZERO ||
		fabsf(z) > F_APPROXIMATELY_ZERO)
	{
		flush();

		mMatrix[mMatrixMode][mMatIdx[mMatrixMode]].applyTranslationAffine(x,
																		  y,
																		  z);
		++mMatHash[mMatrixMode];
	}
}

void LLRender::scalef(F32 x, F32 y, F32 z)
{
	if (fabsf(x - 1.f) > F_APPROXIMATELY_ZERO ||
		fabsf(y - 1.f) > F_APPROXIMATELY_ZERO ||
		fabsf(z - 1.f) > F_APPROXIMATELY_ZERO)
	{
		flush();

		mMatrix[mMatrixMode][mMatIdx[mMatrixMode]].applyScaleAffine(x, y, z);
		++mMatHash[mMatrixMode];
	}
}

void LLRender::ortho(F32 left, F32 right, F32 bottom, F32 top, F32 z_near,
					 F32 z_far)
{
	flush();

	LLMatrix4a ortho_mat;
	ortho_mat.setRow<0>(LLVector4a(2.f / (right - left), 0.f, 0.f));
	ortho_mat.setRow<1>(LLVector4a(0.f, 2.f / (top - bottom), 0.f));
	ortho_mat.setRow<2>(LLVector4a(0.f, 0.f, -2.f / (z_far - z_near)));
	ortho_mat.setRow<3>(LLVector4a((left + right) / (left - right),
								   (bottom + top) / (bottom - top),
								   (z_near + z_far) / (z_near - z_far), 1.f));

	mMatrix[mMatrixMode][mMatIdx[mMatrixMode]].mulAffine(ortho_mat);
	++mMatHash[mMatrixMode];
}

void LLRender::rotatef(const LLMatrix4a& rot)
{
	flush();

	mMatrix[mMatrixMode][mMatIdx[mMatrixMode]].mulAffine(rot);
	++mMatHash[mMatrixMode];
}

void LLRender::rotatef(F32 a, F32 x, F32 y, F32 z)
{
	if (fabsf(fmodf(a, 360.f)) > F_APPROXIMATELY_ZERO)
	{
		rotatef(gl_gen_rot(a, x, y, z));
	}
}

bool LLRender::projectf(const LLVector3& object, const LLMatrix4a& modelview,
						const LLMatrix4a& projection, const LLRect& viewport,
						LLVector3& window_coord)
{
	// Declare locals
	const LLVector4a obj_vector(object.mV[VX], object.mV[VY], object.mV[VZ]);
	const LLVector4a one(1.f);
	LLVector4a temp_vec;		// Scratch vector
	LLVector4a w;				// Splatted W-component.

	// temp_vec = modelview * obj_vector;
	modelview.affineTransform(obj_vector, temp_vec);

	// Passing temp_matrix as v and res is safe. res not altered until after
	// all other calculations
	// temp_vec = projection * temp_vec
	projection.rotate4(temp_vec, temp_vec);

	// w = temp_vec.wwww
	w.splat<3>(temp_vec);

	// If w == 0.f, use 1.f instead. Defer return if temp_vec.w == 0.f until
	// after all SSE intrinsics.

	LLVector4a div;
	// float div = (w[N] == 0.f ? 1.f : w[N]);
	div.setSelectWithMask(w.equal(_mm_setzero_ps()), one, w);
	// temp_vec /= div;
	temp_vec.div(div);

	// Map x, y to range 0-1
	temp_vec.mul(.5f);
	temp_vec.add(.5f);

	LLVector4Logical mask = temp_vec.equal(_mm_setzero_ps());
	if (mask.areAllSet(LLVector4Logical::MASK_W))
	{
		return false;
	}

	// Window coordinates
	window_coord[0] = temp_vec[VX] * viewport.getWidth() + viewport.mLeft;
	window_coord[1] = temp_vec[VY] * viewport.getHeight() + viewport.mBottom;
	// This is only correct when glDepthRange(0.f, 1.f)
	window_coord[2] = temp_vec[VZ];

	return true;
}

bool LLRender::unprojectf(const LLVector3& window_coord,
						  const LLMatrix4a& modelview,
						  const LLMatrix4a& projection,
						  const LLRect& viewport, LLVector3& object)
{
	static const LLVector4a one(1.f);
	static const LLVector4a two(2.f);
	LLVector4a norm_view((window_coord.mV[VX] - (F32)viewport.mLeft) /
						 (F32)viewport.getWidth(),
						 (window_coord.mV[VY] - (F32)viewport.mBottom) /
						 (F32)viewport.getHeight(),
						 window_coord.mV[VZ], 1.f);

	LLMatrix4a inv_mat;		// Inverse transformation matrix
	LLVector4a temp_vec;	// Scratch vector
	LLVector4a w;			// Splatted W-component.

	// inv_mat = projection * modelview
	inv_mat.setMul(projection, modelview);

	F32 det = inv_mat.invert();

	// Normalize. -1.0 : +1.0
	norm_view.mul(two);						// norm_view *= vec4(.2f)
	norm_view.sub(one);						// norm_view -= vec4(1.f)

	inv_mat.rotate4(norm_view, temp_vec);	// inv_mat * norm_view

	w.splat<3>(temp_vec);					// w = temp_vec.wwww

	// If w == 0.f, use 1.f instead. Defer return if temp_vec.w == 0.f until
	// after all SSE intrinsics.
	LLVector4a div;
	// float div = (w[N] == 0.f ? 1.f : w[N]);
	div.setSelectWithMask(w.equal(_mm_setzero_ps()), one, w);

	temp_vec.div(div);						// temp_vec /= div;

	LLVector4Logical mask = temp_vec.equal(_mm_setzero_ps());
	if (mask.areAllSet(LLVector4Logical::MASK_W))
	{
		return false;
	}

	if (det == 0.f)
	{
		return false;
	}

	object.set(temp_vec.getF32ptr());

	return true;
}

void LLRender::pushMatrix()
{
#if 0
	flush();
#endif
	if (mMatIdx[mMatrixMode] < LL_MATRIX_STACK_DEPTH - 1)
	{
		mMatrix[mMatrixMode][mMatIdx[mMatrixMode] + 1] =
			mMatrix[mMatrixMode][mMatIdx[mMatrixMode]];
		++mMatIdx[mMatrixMode];
	}
	else
	{
		llwarns << "Matrix stack overflow." << llendl;
	}
}

void LLRender::popMatrix()
{
	flush();

	if (mMatIdx[mMatrixMode] > 0)
	{
		--mMatIdx[mMatrixMode];
		++mMatHash[mMatrixMode];
	}
	else
	{
		llwarns << "Matrix stack underflow." << llendl;
	}
}

void LLRender::loadMatrix(const F32* m)
{
	flush();

	mMatrix[mMatrixMode][mMatIdx[mMatrixMode]].loadu(m);
	++mMatHash[mMatrixMode];
}

void LLRender::loadMatrix(const LLMatrix4a& mat)
{
	flush();

	mMatrix[mMatrixMode][mMatIdx[mMatrixMode]] = mat;
	++mMatHash[mMatrixMode];
}

void LLRender::multMatrix(const F32* m)
{
	flush();

	LLMatrix4a mat;
	mat.loadu(m);
	mMatrix[mMatrixMode][mMatIdx[mMatrixMode]].mulAffine(mat);
	++mMatHash[mMatrixMode];
}

void LLRender::multMatrix(const LLMatrix4a& mat)
{
	flush();

	mMatrix[mMatrixMode][mMatIdx[mMatrixMode]].mulAffine(mat);
	++mMatHash[mMatrixMode];
}

void LLRender::matrixMode(U32 mode)
{
	// Normally, mode == MM_TEXTURE or < NUM_MATRIX_MODES; consider any mode
	// above NUM_MATRIX_MODES as being MM_TEXTURE.
	if (mode >= NUM_MATRIX_MODES)
	{
		mode = MM_TEXTURE0 + gGL.getCurrentTexUnitIndex();
		if (mode > MM_TEXTURE3)
		{
			llwarns << "Attempt to set matrix mode above MM_TEXTURE3. "
					<< "Current texture unit index is: " << mode - MM_TEXTURE0
					<< ". Setting matrix mode to MM_TEXTURE3, expect a render glitch..."
					<< llendl;
			llassert(false);
			mode = MM_TEXTURE0;
		}
	}

	mMatrixMode = mode;
}

U32 LLRender::getMatrixMode()
{
	if (mMatrixMode >= MM_TEXTURE0 && mMatrixMode <= MM_TEXTURE3)
	{
		// Always return MM_TEXTURE if current matrix mode points at any
		// texture matrix
		return MM_TEXTURE;
	}

	return mMatrixMode;
}

void LLRender::loadIdentity()
{
	flush();

	mMatrix[mMatrixMode][mMatIdx[mMatrixMode]].setIdentity();
	++mMatHash[mMatrixMode];
}

const LLMatrix4a& LLRender::getModelviewMatrix()
{
	return mMatrix[MM_MODELVIEW][mMatIdx[MM_MODELVIEW]];
}

void LLRender::translateUI(F32 x, F32 y, F32 z)
{
	if (mUIOffset.empty())
	{
		llerrs << "Need to push a UI translation frame before offsetting"
			   << llendl;
	}

	mUIOffset.back().mV[0] += x;
	mUIOffset.back().mV[1] += y;
	mUIOffset.back().mV[2] += z;
}

void LLRender::scaleUI(F32 x, F32 y, F32 z)
{
	if (mUIScale.empty())
	{
		llerrs << "Need to push a UI transformation frame before scaling."
			   << llendl;
	}

	mUIScale.back().scaleVec(LLVector3(x, y, z));
}

void LLRender::pushUIMatrix()
{
	if (mUIOffset.empty())
	{
		mUIOffset.emplace_back(0.f, 0.f, 0.f);
	}
	else
	{
		mUIOffset.emplace_back(mUIOffset.back());
	}

	if (mUIScale.empty())
	{
		mUIScale.emplace_back(1.f, 1.f, 1.f);
	}
	else
	{
		mUIScale.emplace_back(mUIScale.back());
	}
}

void LLRender::popUIMatrix()
{
	if (mUIOffset.empty())
	{
		llerrs << "UI offset stack blown." << llendl;
	}
	mUIOffset.pop_back();
	mUIScale.pop_back();
}

LLVector3 LLRender::getUITranslation()
{
	if (mUIOffset.empty())
	{
		return LLVector3::zero;
	}
	return mUIOffset.back();
}

LLVector3 LLRender::getUIScale()
{
	if (mUIScale.empty())
	{
		return LLVector3::all_one;
	}
	return mUIScale.back();
}

void LLRender::loadUIIdentity()
{
	if (mUIOffset.empty() || mUIScale.empty())
	{
		llerrs << "Need to push UI translation frame before clearing offset."
			   << llendl;
	}
	mUIOffset.back().clear();
	mUIScale.back().set(1.f, 1.f, 1.f);
}

void LLRender::setColorMask(bool write_color, bool write_alpha)
{
	setColorMask(write_color, write_color, write_color, write_alpha);
}

void LLRender::setColorMask(bool write_red, bool write_green,
							bool write_blue, bool write_alpha)
{
	if (mCurrColorMask[0] != write_red || mCurrColorMask[1] != write_green ||
		mCurrColorMask[2] != write_blue || mCurrColorMask[3] != write_alpha)
	{
		mCurrColorMask[0] = write_red;
		mCurrColorMask[1] = write_green;
		mCurrColorMask[2] = write_blue;
		mCurrColorMask[3] = write_alpha;

		flush();
		glColorMask(write_red ? GL_TRUE : GL_FALSE,
					write_green ? GL_TRUE : GL_FALSE,
					write_blue ? GL_TRUE : GL_FALSE,
					write_alpha ? GL_TRUE : GL_FALSE);
	}
}

void LLRender::setSceneBlendType(U32 type)
{
	switch (type)
	{
		case BT_ALPHA:
			blendFunc(BF_SOURCE_ALPHA, BF_ONE_MINUS_SOURCE_ALPHA);
			break;

		case BT_ADD:
			blendFunc(BF_ONE, BF_ONE);
			break;

		case BT_ADD_WITH_ALPHA:
			blendFunc(BF_SOURCE_ALPHA, BF_ONE);
			break;

		case BT_MULT:
			blendFunc(BF_DEST_COLOR, BF_ZERO);
			break;

		case BT_MULT_ALPHA:
			blendFunc(BF_DEST_ALPHA, BF_ZERO);
			break;

		case BT_MULT_X2:
			blendFunc(BF_DEST_COLOR, BF_SOURCE_COLOR);
			break;

		case BT_REPLACE:
			blendFunc(BF_ONE, BF_ZERO);
			break;

		default:
			llerrs << "Unknown scene blend type: " << type << llendl;
	}
}

void LLRender::blendFunc(U32 sfactor, U32 dfactor)
{
	llassert(sfactor < BF_UNDEF && dfactor < BF_UNDEF);
	if (mCurrBlendColorSFactor != sfactor ||
		mCurrBlendColorDFactor != dfactor ||
	    mCurrBlendAlphaSFactor != sfactor ||
		mCurrBlendAlphaDFactor != dfactor)
	{
		mCurrBlendColorSFactor = sfactor;
		mCurrBlendAlphaSFactor = sfactor;
		mCurrBlendColorDFactor = dfactor;
		mCurrBlendAlphaDFactor = dfactor;
		flush();
		glBlendFunc(sGLBlendFactor[sfactor], sGLBlendFactor[dfactor]);
	}
}

void LLRender::blendFunc(U32 color_sfactor,
						 U32 color_dfactor,
						 U32 alpha_sfactor,
						 U32 alpha_dfactor)
{
	llassert(color_sfactor < BF_UNDEF && color_dfactor < BF_UNDEF &&
			 alpha_sfactor < BF_UNDEF && alpha_dfactor < BF_UNDEF);
	if (mCurrBlendColorSFactor != color_sfactor ||
		mCurrBlendColorDFactor != color_dfactor ||
		mCurrBlendAlphaSFactor != alpha_sfactor ||
		mCurrBlendAlphaDFactor != alpha_dfactor)
	{
		mCurrBlendColorSFactor = color_sfactor;
		mCurrBlendAlphaSFactor = alpha_sfactor;
		mCurrBlendColorDFactor = color_dfactor;
		mCurrBlendAlphaDFactor = alpha_dfactor;
		flush();
		glBlendFuncSeparate(sGLBlendFactor[color_sfactor],
							sGLBlendFactor[color_dfactor],
							sGLBlendFactor[alpha_sfactor],
							sGLBlendFactor[alpha_dfactor]);
	}
}

LLTexUnit* LLRender::getTexUnit(U32 index)
{
	if (index < mTexUnits.size())
	{
		return mTexUnits[index];
	}
	llwarns_sparse << "Non-existing texture unit layer requested: " << index
				   << llendl;
	return mDummyTexUnit;
}

LLLightState* LLRender::getLight(U32 index)
{
	if (index < mLightState.size())
	{
		return mLightState[index];
	}
	return NULL;
}

void LLRender::setAmbientLightColor(const LLColor4& color)
{
	if (color != mAmbientLightColor)
	{
		++mLightHash;
		mAmbientLightColor = color;
	}
}

bool LLRender::verifyTexUnitActive(U32 unit_to_verify)
{
	if (mCurrTextureUnitIndex == unit_to_verify)
	{
		return true;
	}

	llwarns << "TexUnit currently active: " << mCurrTextureUnitIndex
			<< " (expecting " << unit_to_verify << ")" << llendl;
	return false;
}

void LLRender::begin(U32 mode)
{
	if (mode != mMode)
	{
		if (mMode == LINES || mMode == POINTS || mMode == TRIANGLES)
		{
			flush();
		}
		else if (mCount)
		{
			llerrs << "gGL.begin() called redundantly." << llendl;
		}
		mMode = mode;
	}
}

void LLRender::end(bool force_flush)
{
	if (force_flush || mCount > 2048 ||
		(mCount && mMode != LINES && mMode != POINTS && mMode != TRIANGLES))
	{
		flush();
	}
}

void LLRender::flush()
{
	LL_TRACY_TIMER(TRC_RENDER_FLUSH);

	if (mCount > 0)
	{
		// Store mCount in a local variable to avoid re-entrance (drawArrays
		// may call flush)
		U32 count = mCount;
		mCount = 0;

		if (mMode == TRIANGLES)
		{
			if (count % 3 != 0)
			{
				count -= count % 3;
				llwarns << "Incomplete triangle render attempt. Skipping."
						<< llendl;
			}
		}
		else if (mMode == LINES)
		{
			if (count % 2 != 0)
			{
				count -= count % 2;
				llwarns << "Incomplete line render attempt. Skipping."
						<< llendl;
			}
		}

		if (mBuffer)
		{
			LLGLSLShader* shaderp = LLGLSLShader::sCurBoundShaderPtr;
			if ((sUseBufferCache || gUsePBRShaders) && shaderp)
			{
				U32 attribute_mask = shaderp->mAttributeMask;
				U64 digest;
				{
					LL_TRACY_TIMER(TRC_RENDER_FLUSH_HASH);

					HBXXH64 hash((const void*)mVerticesp.get(),
								 count * sizeof(LLVector4a), false);
					if (attribute_mask & LLVertexBuffer::MAP_TEXCOORD0)
					{
						hash.update((const void*)mTexcoordsp.get(),
									count * sizeof(LLVector2));
					}
					if (attribute_mask & LLVertexBuffer::MAP_COLOR)
					{
						hash.update((const void*)mColorsp.get(),
									count * sizeof(LLColor4U));
					}
					digest = hash.digest();
				}

				LL_TRACY_TIMER(TRC_RENDER_FLUSH_DRAW);

				LLPointer<LLVertexBuffer> vb;
				auto it = sVBCache.find(digest);
				if (it != sVBCache.end())
				{
					vb = it->second.get();
				}
				else
				{
					vb = new LLVertexBuffer(attribute_mask);
					vb->allocateBuffer(count, 0);
					if (gUsePBRShaders)
					{
						vb->setBuffer();
					}
					vb->setPositionData((LLVector4a*)mVerticesp.get());
					if (attribute_mask & LLVertexBuffer::MAP_TEXCOORD0)
					{
						vb->setTexCoordData(mTexcoordsp.get());
					}
					if (attribute_mask & LLVertexBuffer::MAP_COLOR)
					{
						vb->setColorData(mColorsp.get());
					}
					vb->unbind();
					++mCacheMissCount;
					sVBCache.emplace(digest, vb.get());
				}
				vb->setBuffer(attribute_mask);
				vb->drawArrays(mMode, 0, count);
			}
			else
			{
				LL_TRACY_TIMER(TRC_RENDER_FLUSH_DRAW);

				if (gUsePBRShaders)
				{
					// If we arrived here, then shaderp is NULL, and we cannot
					// use the legacy code to work arount it... This should not
					// happen. HB
					llwarns << "No bound shader !" << llendl;
					llassert(false);
					return;
				}

				if (!mBuffer->isLocked())
				{
					// Hack to only flush the part of the buffer that was
					// updated (relies on stream draw using buffersubdata)
					mBuffer->getVertexStrider(mVerticesp, 0, count);
					mBuffer->getTexCoord0Strider(mTexcoordsp, 0, count);
					mBuffer->getColorStrider(mColorsp, 0, count);
				}

				mBuffer->unmapBuffer();
				mBuffer->setBuffer(immediate_mask);

				mBuffer->drawArrays(mMode, 0, count);
			}

			mVerticesp[0] = mVerticesp[count];
			mTexcoordsp[0] = mTexcoordsp[count];
			mColorsp[0] = mColorsp[count];
		}
	}
}

void LLRender::vertex3f(F32 x, F32 y, F32 z)
{
	LL_TRACY_TIMER(TRC_RENDER_VERTEX);

	// The range of mVerticesp, mColorsp and mTexcoordsp is [0, 4095]
	if (mCount > 2048)
	{
		// Break when buffer gets reasonably full to keep GL command buffers
		// happy and avoid overflow below
		switch (mMode)
		{
			case POINTS:
				flush();
				break;

			case LINES:
				if (mCount % 2 == 0)
				{
					flush();
				}
				break;

			case TRIANGLES:
				if (mCount % 3 == 0)
				{
					flush();
				}
		}
	}

	if (mCount > 4094)
	{
		if (gDebugGL)
		{
			llwarns_sparse << "GL immediate mode overflow. Some geometry not drawn. mMode = "
						   << mMode << llendl;
			llassert(false);
		}
		return;
	}

	if (mUIOffset.empty())
	{
		mVerticesp[mCount++].set(x, y, z);
	}
	else
	{
		LLVector3 vert = (LLVector3(x, y, z) +
						  mUIOffset.back()).scaledVec(mUIScale.back());
		mVerticesp[mCount++] = vert;
	}

	mVerticesp[mCount] = mVerticesp[mCount - 1];
	mColorsp[mCount] = mColorsp[mCount - 1];
	mTexcoordsp[mCount] = mTexcoordsp[mCount - 1];
}

void LLRender::vertexBatchPreTransformed(LLVector3* verts, S32 vert_count)
{
	LL_TRACY_TIMER(TRC_RENDER_VERTEX_BATCH);

	if (mCount + vert_count > 4094)
	{
		if (gDebugGL)
		{
			llwarns_sparse << "GL immediate mode overflow. Some geometry not drawn."
						   << llendl;
			llassert(false);
		}
		return;
	}

	S32 i = 0;
	while (i < vert_count)
	{
		mVerticesp[mCount++] = verts[i++];
		mTexcoordsp[mCount] = mTexcoordsp[mCount - 1];
		mColorsp[mCount] = mColorsp[mCount - 1];
	}

	if (mCount > 0)
	{
		mVerticesp[mCount] = mVerticesp[mCount - 1];
	}
}

void LLRender::vertexBatchPreTransformed(LLVector3* verts, LLVector2* uvs,
										 S32 vert_count)
{
	LL_TRACY_TIMER(TRC_RENDER_VERTEX_BATCH);

	if (mCount + vert_count > 4094)
	{
		if (gDebugGL)
		{
			llwarns_sparse << "GL immediate mode overflow. Some geometry not drawn."
						   << llendl;
			llassert(false);
		}
		return;
	}

	S32 i = 0;
	while (i < vert_count)
	{
		mVerticesp[mCount] = verts[i];
		mTexcoordsp[mCount++] = uvs[i++];
		mColorsp[mCount] = mColorsp[mCount - 1];
	}

	if (mCount > 0)
	{
		mVerticesp[mCount] = mVerticesp[mCount - 1];
		mTexcoordsp[mCount] = mTexcoordsp[mCount - 1];
	}
}

void LLRender::vertexBatchPreTransformed(LLVector3* verts, LLVector2* uvs,
										 LLColor4U* colors, S32 vert_count)
{
	LL_TRACY_TIMER(TRC_RENDER_VERTEX_BATCH);

	if (mCount + vert_count > 4094)
	{
		if (gDebugGL)
		{
			llwarns_sparse << "GL immediate mode overflow. Some geometry not drawn."
						   << llendl;
			llassert(false);
		}
		return;
	}

	S32 i = 0;
	while (i < vert_count)
	{
		mVerticesp[mCount] = verts[i];
		mTexcoordsp[mCount] = uvs[i];
		mColorsp[mCount++] = colors[i++];
	}

	if (mCount > 0)
	{
		mVerticesp[mCount] = mVerticesp[mCount - 1];
		mTexcoordsp[mCount] = mTexcoordsp[mCount - 1];
		mColorsp[mCount] = mColorsp[mCount - 1];
	}
}

void LLRender::color4ub(U8 r, U8 g, U8 b, U8 a)
{
	if (!LLGLSLShader::sCurBoundShaderPtr ||
		(LLGLSLShader::sCurBoundShaderPtr->mAttributeMask &
		 LLVertexBuffer::MAP_COLOR))
	{
		mColorsp[mCount] = LLColor4U(r, g, b, a);
	}
	else
	{
		// Not using shaders or shader reads color from a uniform
		diffuseColor4ub(r, g, b, a);
	}
}

void LLRender::diffuseColor3f(F32 r, F32 g, F32 b)
{
	LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;
	llassert(shader != NULL);
	if (shader)
	{
		shader->uniform4f(LLShaderMgr::DIFFUSE_COLOR, r, g, b, 1.f);
	}
}

void LLRender::diffuseColor3fv(const F32* c)
{
	LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;
	llassert(shader != NULL);
	if (shader)
	{
		shader->uniform4f(LLShaderMgr::DIFFUSE_COLOR, c[0], c[1], c[2], 1.f);
	}
}

void LLRender::diffuseColor4f(F32 r, F32 g, F32 b, F32 a)
{
	LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;
	llassert(shader != NULL);
	if (shader)
	{
		shader->uniform4f(LLShaderMgr::DIFFUSE_COLOR, r, g, b, a);
	}
}

void LLRender::diffuseColor4fv(const F32* c)
{
	LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;
	llassert(shader != NULL);
	if (shader)
	{
		shader->uniform4fv(LLShaderMgr::DIFFUSE_COLOR, 1, c);
	}
}

void LLRender::diffuseColor4ubv(const U8* c)
{
	LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;
	llassert(shader != NULL);
	if (shader)
	{
		shader->uniform4f(LLShaderMgr::DIFFUSE_COLOR, c[0] / 255.f,
						  c[1] / 255.f, c[2] / 255.f, c[3] / 255.f);
	}
}

void LLRender::diffuseColor4ub(U8 r, U8 g, U8 b, U8 a)
{
	LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;
	llassert(shader != NULL);
	if (shader)
	{
		shader->uniform4f(LLShaderMgr::DIFFUSE_COLOR,
						  r / 255.f, g / 255.f, b / 255.f, a / 255.f);
	}
}

void LLRender::lineWidth(F32 width)
{
	if (sGLCoreProfile)
	{
		// Always 1.f (or less, but we never use less) under core GL profile !
		// So, this call is actually a no-operation...
		return;
	}
	glLineWidth(width);
}

void LLRender::debugTexUnits()
{
	llinfos << "Active TexUnit: " << mCurrTextureUnitIndex << llendl;
	std::string active_enabled = "false";
	for (U32 i = 0; i < mTexUnits.size(); ++i)
	{
		LLTexUnit* unit = getTexUnit(i);
		if (unit->mCurrTexType != LLTexUnit::TT_NONE)
		{
			if (i == mCurrTextureUnitIndex)
			{
				active_enabled = "true";
			}
			llinfos << "TexUnit " << i << " enabled as: ";
			switch (unit->mCurrTexType)
			{
				case LLTexUnit::TT_TEXTURE:
					LL_CONT << "2D texture";
					break;

				case LLTexUnit::TT_RECT_TEXTURE:
					LL_CONT << "texture rectangle";
					break;

				case LLTexUnit::TT_CUBE_MAP:
					LL_CONT << "cube map";
					break;

				case LLTexUnit::TT_CUBE_MAP_ARRAY:
					LL_CONT << "cube map array";
					break;

				default:
					LL_CONT << "ARGH !!!  NONE !";
			}
			LL_CONT << ", with bound texture: " << unit->mCurrTexture
					<< llendl;
		}
	}
	llinfos << "Active TexUnit enabled: " << active_enabled << llendl;
}

void LLRender::cleanupVertexBufferCache(U32 current_frame)
{
	LL_TRACY_TIMER(TRC_RENDER_VB_CACHE_CLEANUP);

	sCurrentFrame = current_frame;

	// Cleanup when enough misses occurred
	constexpr U32 MISS_COUNT_LIMIT = 1024;
	U32 erased = 0;
	if (mCacheMissCount > MISS_COUNT_LIMIT)
	{
		for (auto it = sVBCache.begin(); it != sVBCache.end(); )
		{
			if (it->second.expired())
			{
				it = sVBCache.erase(it);
				++erased;
			}
			else
			{
				++it;
			}
		}
		LL_DEBUGS("VertexBuffer") << "Erased " << erased
								  << " expired cached buffers. "
								  << sVBCache.size() << " buffers in cache."
								  << LL_ENDL;
		mCacheMissCount = 0;
	}

	LLVertexBuffer::cleanupVBOPool();
}

LLMatrix4a gl_gen_rot(F32 a, const LLVector4a& axis)
{
	F32 r = a * DEG_TO_RAD;
	F32 c = cosf(r);
	F32 s = sinf(r);
	F32 ic = 1.f - c;

	const LLVector4a add1(c, axis[VZ] * s, -axis[VY] * s);	// 1,z,-y
	const LLVector4a add2(-axis[VZ] * s, c, axis[VX] * s);	// -z,1,x
	const LLVector4a add3(axis[VY] * s, -axis[VX] * s, c);	// y,-x,1

	LLVector4a axis_x;
	axis_x.splat<0>(axis);
	LLVector4a axis_y;
	axis_y.splat<1>(axis);
	LLVector4a axis_z;
	axis_z.splat<2>(axis);

	LLVector4a c_axis;
	c_axis.setMul(axis, ic);

	LLMatrix4a rot_mat;
	rot_mat.getRow<0>().setMul(c_axis, axis_x);
	rot_mat.getRow<0>().add(add1);
	rot_mat.getRow<1>().setMul(c_axis, axis_y);
	rot_mat.getRow<1>().add(add2);
	rot_mat.getRow<2>().setMul(c_axis, axis_z);
	rot_mat.getRow<2>().add(add3);
	rot_mat.setRow<3>(LLVector4a(0.f, 0.f, 0.f, 1.f));

	return rot_mat;
}

LLMatrix4a gl_ortho(F32 left, F32 right, F32 bottom, F32 top, F32 z_near,
					F32 z_far)
{
	LLMatrix4a ret;
	ret.setRow<0>(LLVector4a(2.f / (right - left), 0.f, 0.f));
	ret.setRow<1>(LLVector4a(0.f, 2.f / (top - bottom), 0.f));
	ret.setRow<2>(LLVector4a(0.f, 0.f, -2.f / (z_far - z_near)));
	ret.setRow<3>(LLVector4a((left + right) / (left - right),
							 (bottom + top) / (bottom - top),
							 (z_near + z_far) / (z_near - z_far), 1.f));
	return ret;
}

LLMatrix4a gl_perspective(F32 fovy, F32 aspect, F32 z_near, F32 z_far)
{
	constexpr F32 factor = DEG_TO_RAD * 0.5f;
	F32 f = 1.f / tanf(factor * fovy);

	LLMatrix4a ret;
	ret.setRow<0>(LLVector4a(f / aspect, 0.f, 0.f));
	ret.setRow<1>(LLVector4a(0.f, f, 0.f));
	ret.setRow<2>(LLVector4a(0.f, 0.f, (z_far + z_near) / (z_near - z_far),
							 -1.f));
	ret.setRow<3>(LLVector4a(0.f, 0.f,
							 (2.f * z_far * z_near) / (z_near - z_far), 0.f));
	return ret;
}
