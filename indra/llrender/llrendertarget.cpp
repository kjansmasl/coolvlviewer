/**
 * @file llrendertarget.cpp
 * @brief LLRenderTarget implementation
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
 * $/LicenseInfo$
 */

#include "linden_common.h"

#include "llrendertarget.h"

#include "llgl.h"
#include "llimagegl.h"
#include "llrender.h"

// statics
U32 LLRenderTarget::sBytesAllocated = 0;
bool LLRenderTarget::sUseFBO = false;
U32 LLRenderTarget::sCurFBO = 0;
U32 LLRenderTarget::sCurResX = 0;
U32 LLRenderTarget::sCurResY = 0;

static LLRenderTarget* sBoundTarget = NULL;

void check_framebuffer_status()
{
	if (gDebugGL)
	{
		GLenum status = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE)
		{
			llwarns << "Frame buffer failed check with status: " << std::hex
					<< status << std::dec << llendl;
		}
		stop_glerror();
	}
}

LLRenderTarget::LLRenderTarget()
:	mPreviousRT(NULL),
	mResX(0),
	mResY(0),
	mFBO(0),
	mPreviousFBO(0),
	mPreviousResX(0),
	mPreviousResY(0),
	mDepth(0),
	mUsage(LLTexUnit::TT_TEXTURE),
	mGenerateMipMaps(LLTexUnit::TMG_NONE),
	mMipLevels(0),
	mUseDepth(false),
	mStencil(false)
{
}

LLRenderTarget::~LLRenderTarget()
{
	release();
}

//static
void LLRenderTarget::reset()
{
	sCurFBO = 0;
	sCurResX = sCurResY = 0;
	sBoundTarget = NULL;
}

void LLRenderTarget::resize(U32 resx, U32 resy)
{
	// For accounting, get the number of pixels added/subtracted
	S32 pix_diff = resx * resy - mResX * mResY;

	mResX = resx;
	mResY = resy;

	llassert(mInternalFormat.size() == mTex.size());

	U32 internal_type = LLTexUnit::getInternalType(mUsage);
	LLTexUnit* unit0 = gGL.getTexUnit(0);
	for (U32 i = 0, size = mTex.size(); i < size; ++i)
	{
		// Resize color attachments
		unit0->bindManual(mUsage, mTex[i]);
		LLImageGL::setManualImage(internal_type, 0, mInternalFormat[i],
								  mResX, mResY, GL_RGBA, GL_UNSIGNED_BYTE,
								  NULL, false);
		sBytesAllocated += pix_diff * 4;
	}

	if (mDepth)
	{
		// Resize depth attachment
		if (mStencil)
		{
			// Use render buffers where stencil buffers are in play
			glBindRenderbuffer(GL_RENDERBUFFER, mDepth);
			glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8,
								  mResX, mResY);
			glBindRenderbuffer(GL_RENDERBUFFER, 0);
		}
		else
		{
			unit0->bindManual(mUsage, mDepth);
			LLImageGL::setManualImage(internal_type, 0, GL_DEPTH_COMPONENT24,
									  mResX, mResY, GL_DEPTH_COMPONENT,
									  GL_UNSIGNED_INT, NULL, false);
		}

		sBytesAllocated += pix_diff * 4;
	}
}

// Legacy EE renderer version
bool LLRenderTarget::allocate(U32 resx, U32 resy, U32 color_fmt, bool depth,
							  bool stencil, LLTexUnit::eTextureType usage)
{
	resx = llmin(resx, gGLManager.mGLMaxTextureSize);
	resy = llmin(resy, gGLManager.mGLMaxTextureSize);

	release();
	stop_glerror();

	mResX = resx;
	mResY = resy;

	mStencil = stencil;
	mUsage = usage;
	mUseDepth = depth;

	if (sUseFBO)
	{
		if (depth)
		{
			if (!allocateDepth())
			{
				llwarns << "Failed to allocate depth buffer for render target."
						<< llendl;
				return false;
			}
		}

		glGenFramebuffers(1, (GLuint*)&mFBO);

		if (mDepth)
		{
			stop_glerror();
			glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
			if (mStencil)
			{
				glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
										  GL_RENDERBUFFER, mDepth);
				glFramebufferRenderbuffer(GL_FRAMEBUFFER,
										  GL_STENCIL_ATTACHMENT,
										  GL_RENDERBUFFER, mDepth);
			}
			else
			{
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
									   LLTexUnit::getInternalType(mUsage),
									   mDepth, 0);
			}
			glBindFramebuffer(GL_FRAMEBUFFER, sCurFBO);
			stop_glerror();
		}
	}

	return addColorAttachment(color_fmt);
}

// New PBR renderer version
bool LLRenderTarget::allocate(U32 resx, U32 resy, U32 color_fmt, bool depth,
							  LLTexUnit::eTextureType usage,
							  LLTexUnit::eMipGeneration mips_generation)
{
	resx = llmin(resx, gGLManager.mGLMaxTextureSize);
	resy = llmin(resy, gGLManager.mGLMaxTextureSize);

	release();
	stop_glerror();

	mResX = resx;
	mResY = resy;

	mUsage = usage;
	mStencil = false;
	mUseDepth = depth;
	mGenerateMipMaps = mips_generation;

	if (mips_generation != LLTexUnit::TMG_NONE)
	{
		mMipLevels = 1 + log2f(F32(llmax(resx, resy)));
	}

	if (depth && !allocateDepth())
	{
		llwarns << "Failed to allocate depth buffer for render target."
				<< llendl;
		return false;
	}

	glGenFramebuffers(1, (GLuint*)&mFBO);

	if (mDepth)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
							   LLTexUnit::getInternalType(mUsage), mDepth, 0);
		glBindFramebuffer(GL_FRAMEBUFFER, sCurFBO);
		stop_glerror();
	}

	return addColorAttachment(color_fmt);
}

void LLRenderTarget::setColorAttachment(LLImageGL* imgp, U32 use_name)
{
	// This method only works when img is not NULL, FBO support is enabled,
	// depth buffers are not in use, mTex is empty (binding target should be
	// done via LLImageGL).
	llassert(imgp && sUseFBO && mDepth == 0 && mTex.empty());

	if (!mFBO)
	{
		glGenFramebuffers(1, (GLuint*)&mFBO);
	}

	mResX = imgp->getWidth();
	mResY = imgp->getHeight();
	mUsage = imgp->getTarget();

	if (!use_name)
	{
		use_name = imgp->getTexName();
	}

	mTex.push_back(use_name);

	stop_glerror();
	glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
						   LLTexUnit::getInternalType(mUsage), use_name, 0);
	check_framebuffer_status();
	glBindFramebuffer(GL_FRAMEBUFFER, sCurFBO);
	stop_glerror();
}

void LLRenderTarget::releaseColorAttachment()
{
	// Cannot use releaseColorAttachment with LLRenderTarget managed color
	// targets
	llassert(mFBO && mTex.size() == 1);

	glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
						   LLTexUnit::getInternalType(mUsage), 0, 0);
	glBindFramebuffer(GL_FRAMEBUFFER, sCurFBO);
	mTex.clear();
}

bool LLRenderTarget::addColorAttachment(U32 color_fmt)
{
	if (color_fmt == 0)
	{
		return true;
	}

	U32 offset = mTex.size();
	if (offset >= 4)
	{
		llwarns << "Too many color attachments !" << llendl;
		return false;
	}
	if (offset > 0 && !mFBO)
	{
		llwarns << "FBO not in use, aborting." << llendl;
		return false;
	}

	U32 tex;
	LLImageGL::generateTextures(1, &tex);
	LLTexUnit* unit0 = gGL.getTexUnit(0);
	unit0->bindManual(mUsage, tex);

	clear_glerror();
	U32 internal_type = LLTexUnit::getInternalType(mUsage);
	LLImageGL::setManualImage(internal_type, 0, color_fmt, mResX, mResY,
							  GL_RGBA, GL_UNSIGNED_BYTE, NULL, false);
	if (glGetError() != GL_NO_ERROR)
	{
		llwarns << "Could not allocate color buffer for render target."
				<< llendl;
		return false;
	}

	sBytesAllocated += mResX * mResY * 4;

	if (offset == 0)
	{
		// Use bilinear filtering on single texture render targets that are not
		// multisampled
		unit0->setTextureFilteringOption(LLTexUnit::TFO_BILINEAR);
	}
	else
	{
		// Do not filter data attachments
		unit0->setTextureFilteringOption(LLTexUnit::TFO_POINT);
	}
	if (mUsage != LLTexUnit::TT_RECT_TEXTURE)
	{
		unit0->setTextureAddressMode(LLTexUnit::TAM_MIRROR);
	}
	else
	{
		// ATI does not support mirrored repeat for rectangular textures.
		unit0->setTextureAddressMode(LLTexUnit::TAM_CLAMP);
	}
	stop_glerror();

	if (mFBO)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + offset,
							   internal_type, tex, 0);
		check_framebuffer_status();
		glBindFramebuffer(GL_FRAMEBUFFER, sCurFBO);
		stop_glerror();
	}

	mTex.push_back(tex);
	mInternalFormat.push_back(color_fmt);

	if (gDebugGL)
	{
		// Bind and unbind to validate target
		bindTarget();
		flush();
	}

	return true;
}

bool LLRenderTarget::allocateDepth()
{
	if (mStencil)
	{
		// Use render buffers where stencil buffers are in play
		glGenRenderbuffers(1, (GLuint*)&mDepth);
		glBindRenderbuffer(GL_RENDERBUFFER, mDepth);
		clear_glerror();
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8,
							  mResX, mResY);
		glBindRenderbuffer(GL_RENDERBUFFER, 0);
	}
	else
	{
		LLImageGL::generateTextures(1, &mDepth);
		LLTexUnit* unit0 = gGL.getTexUnit(0);
		unit0->bindManual(mUsage, mDepth);
		clear_glerror();
		LLImageGL::setManualImage(LLTexUnit::getInternalType(mUsage), 0,
								  GL_DEPTH_COMPONENT24, mResX, mResY,
								  GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, NULL,
								  false);
		unit0->setTextureFilteringOption(LLTexUnit::TFO_POINT);
	}

	if (glGetError() != GL_NO_ERROR)
	{
		llwarns << "Unable to allocate depth buffer for render target."
				<< llendl;
		return false;
	}

	sBytesAllocated += mResX * mResY * 4;

	return true;
}

void LLRenderTarget::shareDepthBuffer(LLRenderTarget& target)
{
	if (!mFBO || !target.mFBO)
	{
		llerrs << "Cannot share depth buffer between non FBO render targets."
			   << llendl;
	}

	if (target.mDepth)
	{
		llerrs << "Attempting to override existing depth buffer. Detach existing buffer first."
			   << llendl;
	}

	if (target.mUseDepth)
	{
		llerrs << "Attempting to override existing shared depth buffer. Detach existing buffer first."
			   << llendl;
	}

	if (mDepth)
	{
		stop_glerror();
		glBindFramebuffer(GL_FRAMEBUFFER, target.mFBO);

		if (mStencil)
		{
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
									  GL_RENDERBUFFER, mDepth);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT,
									  GL_RENDERBUFFER, mDepth);
			target.mStencil = true;
		}
		else
		{
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
								   LLTexUnit::getInternalType(mUsage),
								   mDepth, 0);
		}

		check_framebuffer_status();
		glBindFramebuffer(GL_FRAMEBUFFER, sCurFBO);
		stop_glerror();

		target.mUseDepth = true;
	}
}

void LLRenderTarget::release()
{
	// New PBR renderer version
	if (gUsePBRShaders)
	{
		if (mDepth)
		{
			LLImageGL::deleteTextures(1, &mDepth);
			mDepth = 0;
			sBytesAllocated -= mResX * mResY * 4;
		}
		else if (mFBO)
		{
			glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
			if (mUseDepth)
			{
				// Detach shared depth buffer
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
									   LLTexUnit::getInternalType(mUsage),
									   0, 0);
				mUseDepth = false;
			}
			glBindFramebuffer(GL_FRAMEBUFFER, sCurFBO);
		}

		size_t tsize = mTex.size();
		// Detach any extra color buffers (e.g. SRGB spec buffers)
		if (tsize > 1 && mFBO)
		{
			glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
			U32 internal_type = LLTexUnit::getInternalType(mUsage);
			for (size_t z = tsize - 1; z > 0 ; --z)
			{
				glFramebufferTexture2D(GL_FRAMEBUFFER,
									   GL_COLOR_ATTACHMENT0 + z,
									   internal_type, 0, 0);
			}
			LLImageGL::deleteTextures(tsize - 1, &mTex[1]);
			sBytesAllocated -= mResX * mResY * 4 * (tsize - 1);
			glBindFramebuffer(GL_FRAMEBUFFER, sCurFBO);
		}
		if (mFBO)
		{
			if (mFBO == sCurFBO)
			{
				sCurFBO = 0;
				glBindFramebuffer(GL_FRAMEBUFFER, 0);
			}
			glDeleteFramebuffers(1, (GLuint*)&mFBO);
			mFBO = 0;
		}
		if (tsize)
		{
			LLImageGL::deleteTextures(1, &mTex[0]);
			sBytesAllocated -= mResX * mResY * 4;
		}

		mTex.clear();
		mInternalFormat.clear();

		mResX = mResY = 0;
		return;
	}

	// Legacy EE renderer version
	if (mDepth)
	{
		if (mStencil)
		{
			glDeleteRenderbuffers(1, (GLuint*)&mDepth);
		}
		else
		{
			if (mFBO)
			{
				// Release before delete.
				glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
									   LLTexUnit::getInternalType(mUsage),
									   0, 0);
				glBindFramebuffer(GL_FRAMEBUFFER, 0);
			}
			LLImageGL::deleteTextures(1, &mDepth);
		}
		mDepth = 0;

		sBytesAllocated -= mResX * mResY * 4;
	}
	else if (mUseDepth && mFBO)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, mFBO);

		// Detach shared depth buffer
		if (mStencil)
		{
			// Attached as a renderbuffer
			glFramebufferRenderbuffer(GL_FRAMEBUFFER,
									  GL_STENCIL_ATTACHMENT,
									  GL_RENDERBUFFER, 0);
			glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
									  GL_RENDERBUFFER, 0);
			mStencil = false;
		}
		else
		{
			// Attached as a texture
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
								   LLTexUnit::getInternalType(mUsage), 0, 0);
		}
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		mUseDepth = false;
	}

	if (mFBO)
	{
		glDeleteFramebuffers(1, (GLuint*)&mFBO);
		mFBO = 0;
	}
	stop_glerror();

	size_t tsize = mTex.size();
	if (tsize > 0)
	{
		sBytesAllocated -= mResX * mResY * 4 * tsize;
		LLImageGL::deleteTextures(tsize, &mTex[0]);
		mTex.clear();
		mInternalFormat.clear();
	}

	mResX = mResY = 0;
}

void LLRenderTarget::bindTarget()
{
	static const GLenum drawbuffers[] =
	{
		GL_COLOR_ATTACHMENT0,
		GL_COLOR_ATTACHMENT1,
		GL_COLOR_ATTACHMENT2,
		GL_COLOR_ATTACHMENT3
	};

	// New PBR renderer version
	if (gUsePBRShaders)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
		sCurFBO = mFBO;

		// Setup multiple render targets
		glDrawBuffers(mTex.size(), drawbuffers);

		if (mTex.empty())
		{
			// No color buffer to draw to
			glDrawBuffer(GL_NONE);
			glReadBuffer(GL_NONE);
		}
		stop_glerror();

		check_framebuffer_status();

		glViewport(0, 0, mResX, mResY);
		sCurResX = mResX;
		sCurResY = mResY;

		mPreviousRT = sBoundTarget;
		sBoundTarget = this;
		return;
	}

	// Legacy EE renderer version
	if (mFBO)
	{
		mPreviousFBO = sCurFBO;
		glBindFramebuffer(GL_FRAMEBUFFER, mFBO);
		sCurFBO = mFBO;

		// Setup multiple render targets
		glDrawBuffers(mTex.size(), drawbuffers);

		if (mTex.empty())
		{
			// No color buffer to draw to
			glDrawBuffer(GL_NONE);
			glReadBuffer(GL_NONE);
		}
		stop_glerror();

		check_framebuffer_status();
	}

	mPreviousResX = sCurResX;
	mPreviousResY = sCurResY;
	glViewport(0, 0, sCurResX = mResX, sCurResY = mResY);
}

void LLRenderTarget::clear(U32 mask_in)
{
	U32 mask = GL_COLOR_BUFFER_BIT;
	if (mUseDepth)
	{
		if (gUsePBRShaders)
		{
			mask |= GL_DEPTH_BUFFER_BIT;
		}
		else
		{
			mask |= GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
		}
	}
	if (mFBO)
	{
		check_framebuffer_status();
		glClear(mask & mask_in);
	}
	else
	{
		LLGLEnable scissor(GL_SCISSOR_TEST);
		glScissor(0, 0, mResX, mResY);
		glClear(mask & mask_in);
	}
	stop_glerror();
}

U32 LLRenderTarget::getTexture(U32 attachment) const
{
	if (attachment > mTex.size() - 1)
	{
		llerrs << "Invalid attachment index." << llendl;
	}
	return mTex.empty() ? 0 : mTex[attachment];
}

void LLRenderTarget::bindTexture(U32 index, S32 channel,
								 LLTexUnit::eTextureFilterOptions filter_opt)
{
	LLTexUnit* unitp = gGL.getTexUnit(channel);
	bool has_mips = gUsePBRShaders &&
					(filter_opt == LLTexUnit::TFO_TRILINEAR ||
					 filter_opt == LLTexUnit::TFO_ANISOTROPIC);
	unitp->bindManual(mUsage, getTexture(index), has_mips);

	unitp->setTextureFilteringOption(filter_opt);

	if (index < mInternalFormat.size())
	{
		U32 format = mInternalFormat[index];
		LLTexUnit::eTextureColorSpace space;
		if (format == GL_SRGB || format == GL_SRGB8 ||
			format == GL_SRGB_ALPHA || format == GL_SRGB8_ALPHA8)
		{
			space = LLTexUnit::TCS_SRGB;
		}
		else
		{
			space = LLTexUnit::TCS_LINEAR;
		}
		unitp->setTextureColorSpace(space);
	}
	else
	{
		llwarns << "Out of range 'index': " << index << " (max is "
				<< mInternalFormat.size() - 1 << ")" << llendl;
		llassert_always(!gDebugGL);
		unitp->setTextureColorSpace(LLTexUnit::TCS_LINEAR);
	}
}

void LLRenderTarget::flush(bool fetch_depth)
{
	gGL.flush();

	// New PBR renderer version
	if (gUsePBRShaders)
	{
		if (mGenerateMipMaps == LLTexUnit::TMG_AUTO)
		{
			bindTexture(0, 0, LLTexUnit::TFO_TRILINEAR);
			glGenerateMipmap(GL_TEXTURE_2D);
		}

		if (mPreviousRT)
		{
			// *HACK: pop the RT stack back two frames and push the previous
			// frame back on to play nice with the GL state machine.
			sBoundTarget = mPreviousRT->mPreviousRT;
			mPreviousRT->bindTarget();
		}
		else
		{
			sBoundTarget = NULL;
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			sCurFBO = 0;
			sCurResX = gGLViewport[2];
			sCurResY = gGLViewport[3];
			glViewport(gGLViewport[0], gGLViewport[1], sCurResX, sCurResY);
		}
		return;
	}

	// Legacy EE renderer version
	if (!mFBO)
	{
		LLTexUnit* unit0 = gGL.getTexUnit(0);
		unit0->bind(this);

		U32 internal_type = LLTexUnit::getInternalType(mUsage);
		glCopyTexSubImage2D(internal_type, 0, 0, 0, 0, 0, mResX, mResY);

		if (fetch_depth)
		{
			if (!mDepth)
			{
				allocateDepth();
			}
			unit0->bind(this, true);
			glCopyTexSubImage2D(internal_type, 0, 0, 0, 0, 0, mResX, mResY);
		}

		unit0->disable();
	}
	else
	{
		glBindFramebuffer(GL_FRAMEBUFFER, mPreviousFBO);
		sCurFBO = mPreviousFBO;

		if (mPreviousFBO)
		{
			glViewport(0, 0, sCurResX = mPreviousResX, sCurResY = mPreviousResY);
			mPreviousFBO = 0;
		}
		else
		{
			glViewport(gGLViewport[0], gGLViewport[1],
					   sCurResX = gGLViewport[2], sCurResY = gGLViewport[3]);
		}
	}
	stop_glerror();
}

void LLRenderTarget::copyContents(LLRenderTarget& source,
								  S32 src_x0, S32 src_y0,
								  S32 src_x1, S32 src_y1,
								  S32 dst_x0, S32 dst_y0,
								  S32 dst_x1, S32 dst_y1,
								  U32 mask, U32 filter)
{
	GLboolean write_depth = mask & GL_DEPTH_BUFFER_BIT ? GL_TRUE : GL_FALSE;
	LLGLDepthTest depth(write_depth, write_depth);

	gGL.flush();
	if (!source.mFBO || !mFBO)
	{
		llwarns << "Cannot copy framebuffer contents for non FBO render targets."
				<< llendl;
		return;
	}

	stop_glerror();
	if (mask == GL_DEPTH_BUFFER_BIT && source.mStencil != mStencil)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, source.mFBO);
		check_framebuffer_status();
		gGL.getTexUnit(0)->bind(this, true);
		glCopyTexSubImage2D(LLTexUnit::getInternalType(mUsage), 0,
							src_x0, src_y0, dst_x0, dst_y0, dst_x1, dst_y1);
		glBindFramebuffer(GL_FRAMEBUFFER, sCurFBO);
	}
	else
	{
		glBindFramebuffer(GL_READ_FRAMEBUFFER, source.mFBO);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, mFBO);
		check_framebuffer_status();
		glBlitFramebuffer(src_x0, src_y0, src_x1, src_y1, dst_x0, dst_y0,
						  dst_x1, dst_y1, mask, filter);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		glBindFramebuffer(GL_FRAMEBUFFER, sCurFBO);
	}
	stop_glerror();
}

//static
void LLRenderTarget::copyContentsToFramebuffer(LLRenderTarget& source,
											   S32 src_x0, S32 src_y0,
											   S32 src_x1, S32 src_y1,
											   S32 dst_x0, S32 dst_y0,
											   S32 dst_x1, S32 dst_y1,
											   U32 mask, U32 filter)
{
	if (!source.mFBO)
	{
		llwarns << "Cannot copy framebuffer contents for non FBO render targets."
				<< llendl;
		return;
 	}

	GLboolean write_depth = mask & GL_DEPTH_BUFFER_BIT ? TRUE : FALSE;
	LLGLDepthTest depth(write_depth, write_depth);

	stop_glerror();
	glBindFramebuffer(GL_READ_FRAMEBUFFER, source.mFBO);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	check_framebuffer_status();
	glBlitFramebuffer(src_x0, src_y0, src_x1, src_y1, dst_x0, dst_y0, dst_x1,
					  dst_y1, mask, filter);
	glBindFramebuffer(GL_FRAMEBUFFER, sCurFBO);
	stop_glerror();
}

bool LLRenderTarget::isComplete() const
{
	return !mTex.empty() || mDepth;
}

void LLRenderTarget::getViewport(S32* viewport)
{
	viewport[0] = viewport[1] = 0;
	viewport[2] = mResX;
	viewport[3] = mResY;
}
