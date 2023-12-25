/**
 * @file llrendertarget.h
 * @brief LLRenderTarget declaration
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

#ifndef LL_LLRENDERTARGET_H
#define LL_LLRENDERTARGET_H

#include "llgl.h"
#include "llrender.h"

// Wrapper around OpenGL frame buffer objects for use in render-to-texture.
// SAMPLE USAGE:
//
//	LLRenderTarget target;
//
//	.../..
//
//	// Allocate a 256x256 RGBA render target with depth buffer
//	target.allocate(256, 256, GL_RGBA, true);
//
//	// Render to contents of offscreen buffer
//	target.bindTarget();
//	target.clear();
//	... <issue drawing commands> ...
//	target.flush();
//	.../...
//
//	// Use target as a texture
//	gGL.getTexUnit(INDEX)->bind(&target);
//	... <issue drawing commands> ...

class LLRenderTarget
{
protected:
	LOG_CLASS(LLRenderTarget);

public:
	LLRenderTarget();
	~LLRenderTarget();

	// Allocates resources for rendering; must be called before use.
	// Multiple calls will release previously allocated resources.
	// Legacy method for EE rendering only.
	bool allocate(U32 resx, U32 resy, U32 color_fmt, bool depth, bool stencil,
				  LLTexUnit::eTextureType usage = LLTexUnit::TT_TEXTURE);
	// New method for PBR rendering only.
	bool allocate(U32 resx, U32 resy, U32 color_fmt, bool depth = false,
				  LLTexUnit::eTextureType usage = LLTexUnit::TT_TEXTURE,
				  LLTexUnit::eMipGeneration generation = LLTexUnit::TMG_NONE);

	// Resizes existing attachments to use new resolution and color format.
	// CAUTION: if GL runs out of memory attempting to resize, this render
	// target will be undefined. DO NOT use for screen space buffers or for
	// scratch space for an image that might be uploaded. DO use for render
	// targets that resize often and aren't likely to ruin someone's day if
	// they break
	void resize(U32 resx, U32 resy);

	// Points this render target at a particular LLImageGL.
	// Intended usage:
	//		LLRenderTarget target;
	//		target.setColorAttachment(attachment, use_name);
	//		target.bindTarget();
	//		<Issue GL calls>
	//		target.flush();
	//		target.releaseColorAttachment();
	// With: 'attachment' the LLImageGL to render into, 'use_name' an optional
	// texture name to target instead of attachment->getTexName().
	// Note: setColorAttachment() and releaseColorAttachment() cannot be used
	// in conjuction with addColorAttachment(), allocateDepth(), resize(), etc.
	void setColorAttachment(LLImageGL* attachmentp, U32 use_name = 0);

	// Adds a color buffer attachment with a limit of 4 color attachments per
	// render target.
	bool addColorAttachment(U32 color_fmt);

	// Detaches from current color attachment
	void releaseColorAttachment();

	// Allocates a depth texture
	bool allocateDepth();

	// Shares depth buffer with provided render target
	void shareDepthBuffer(LLRenderTarget& target);

	// Frees any allocated resources; safe to call redundantly.
	void release();

	// Binds target for rendering; applies appropriate viewport.
	void bindTarget();

	// Clears render targer, clears depth buffer if present, uses scissor rect
	// if in copy-to-texture mode
	void clear(U32 mask = 0xFFFFFFFF);

	// Gets the applied viewport
	void getViewport(S32* viewportp);

	// Returns X resolution
	LL_INLINE U32 getWidth() const						{ return mResX; }

	// Returns Y resolution
	LL_INLINE U32 getHeight() const						{ return mResY; }

	LL_INLINE LLTexUnit::eTextureType getUsage() const	{ return mUsage; }

	U32 getTexture(U32 attachment = 0) const;
	LL_INLINE U32 getNumTextures() const				{ return mTex.size(); }

	LL_INLINE U32 getDepth() const						{ return mDepth; }
	// For EE rendering only
	LL_INLINE bool hasStencil() const					{ return mStencil; }
	LL_INLINE U32 getFBO() const						{ return mFBO; }

	void bindTexture(U32 index, S32 channel,
					 LLTexUnit::eTextureFilterOptions filter_options =
						LLTexUnit::TFO_BILINEAR);

	// Flushes rendering operations. Must be called when rendering is complete.
	// Should be used 1:1 with bindTarget call bindTarget once, do all your
	// rendering, call flush once. If fetch_depth is true, every effort will be
	// made to copy the depth buffer into the current depth texture. A depth
	// texture will be allocated if needed. Note: 'fetch_depth' is ignored by
	// the PBR renderer.
	void flush(bool fetch_depth = false);

	// Returns true if target is ready to be rendered into, that is if the
	// target has been allocated with at least one renderable attachment (i.e.
	// color buffer, depth buffer).
	bool isComplete() const;

	// The following copyContents*() methods are only used by the EE renderer.
	// HB

	void copyContents(LLRenderTarget& source, S32 src_x0, S32 src_y0,
					  S32 src_x1, S32 src_y1, S32 dst_x0, S32 dst_y0,
					  S32 dst_x1, S32 dst_y1, U32 mask, U32 filter);

	static void copyContentsToFramebuffer(LLRenderTarget& source,
										  S32 src_x0, S32 src_y0,
										  S32 src_x1, S32 src_y1,
										  S32 dst_x0, S32 dst_y0,
										  S32 dst_x1, S32 dst_y1,
										  U32 mask, U32 filter);

	// To call when toggling between EE and PBR rendering. HB
	static void reset();

protected:
	std::vector<U32>			mTex;
	std::vector<U32>			mInternalFormat;
	LLRenderTarget*				mPreviousRT;		// For PBR rendering only
	U32							mResX;
	U32							mResY;
	U32							mFBO;
	U32							mPreviousFBO;		// For EE rendering only
	U32							mPreviousResX;		// For EE rendering only
	U32							mPreviousResY;		// For EE rendering only
	U32							mDepth;
	LLTexUnit::eTextureType		mUsage;
	LLTexUnit::eMipGeneration	mGenerateMipMaps;	// For PBR rendering only
	U32							mMipLevels;			// For PBR rendering only
	bool						mUseDepth;
	bool						mStencil;			// For EE rendering only

public:
	// Whether or not to use FBO implementation
	static bool					sUseFBO;

	static U32					sBytesAllocated;
	static U32					sCurFBO;
	static U32					sCurResX;
	static U32					sCurResY;
};

#endif
