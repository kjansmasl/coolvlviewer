/**
 * @file llgltexture.cpp
 * @brief OpenGL texture implementation
 *
 * $LicenseInfo:firstyear=2012&license=viewergpl$
 *
 * Copyright (c) 2012, Linden Research, Inc.
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

#include "llgltexture.h"

#include "llimagegl.h"

LLGLTexture::LLGLTexture(bool usemipmaps)
{
	init();
	mUseMipMaps = usemipmaps;
}

LLGLTexture::LLGLTexture(U32 width, U32 height, U8 components, bool usemipmaps)
{
	init();
	mFullWidth = width;
	mFullHeight = height;
	mUseMipMaps = usemipmaps;
	mComponents = components;
	setTexelsPerImage();
}

LLGLTexture::LLGLTexture(const LLImageRaw* rawp, bool usemipmaps)
{
	init();
	mUseMipMaps = usemipmaps;
	// Create an empty image of the specified size and width
	mImageGLp = new LLImageGL(rawp, usemipmaps);
	mImageGLp->setOwner(this);
}

LLGLTexture::~LLGLTexture()
{
	cleanup();
	mImageGLp = NULL;
}

void LLGLTexture::init()
{
	mBoostLevel = BOOST_NONE;

	mFullWidth = 0;
	mFullHeight = 0;
	mTexelsPerImage = 0;
	mUseMipMaps = false;
	mComponents = 0;

	mTextureState = NO_DELETE;
	mDontDiscard = false;
	mNeedsGLTexture = false;
}

void LLGLTexture::cleanup()
{
	if (mImageGLp)
	{
		mImageGLp->cleanup();
	}
}

// virtual
void LLGLTexture::dump()
{
	if (mImageGLp)
	{
		mImageGLp->dump();
	}
}

void LLGLTexture::setBoostLevel(U32 level)
{
	// Do not donwgrade UI textures, ever !  HB
	if (mBoostLevel == BOOST_UI)
	{
		return;
	}
	if (level == BOOST_UI)
	{
		mBoostLevel = level;
		// UI textures must be always kept in memory for the whole duration of
		// the viewer session. HB
		mTextureState = ALWAYS_KEEP;
		// Also, never allow to discard UI textures. HB
		mDontDiscard = true;
		return;
	}
	mBoostLevel = level;
#if LL_IMPLICIT_SETNODELETE
	if (level != BOOST_NONE && level != BOOST_ALM && level != BOOST_SELECTED)
	{
		mTextureState = NO_DELETE;
	}
#else
	// Make map textures no-delete, always.
	if (level == BOOST_MAP)
	{
		mTextureState = NO_DELETE;
	}
#endif
}

void LLGLTexture::generateGLTexture()
{
	if (mImageGLp.isNull())
	{
		mImageGLp = new LLImageGL(mFullWidth, mFullHeight, mComponents,
								  mUseMipMaps);
		mImageGLp->setOwner(this);
	}
}

LLImageGL* LLGLTexture::getGLImage() const
{
	llassert(mImageGLp.notNull());
	return mImageGLp;
}

bool LLGLTexture::createGLTexture()
{
	if (mImageGLp.isNull())
	{
		generateGLTexture();
	}
	return mImageGLp->createGLTexture();
}

bool LLGLTexture::createGLTexture(S32 discard_level, const LLImageRaw* rawimg,
								  S32 usename, bool to_create, bool defer_copy,
								  U32* tex_name)
{
	llassert(mImageGLp.notNull());

	bool ret = mImageGLp->createGLTexture(discard_level, rawimg, usename,
										  to_create, defer_copy, tex_name);
	if (ret)
	{
		mFullWidth = mImageGLp->getCurrentWidth();
		mFullHeight = mImageGLp->getCurrentHeight();
		mComponents = mImageGLp->getComponents();
		setTexelsPerImage();
	}

	return ret;
}

void LLGLTexture::setExplicitFormat(S32 internal_format, U32 primary_format,
									U32 type_format, bool swap_bytes)
{
	llassert(mImageGLp.notNull());

	mImageGLp->setExplicitFormat(internal_format, primary_format, type_format,
								 swap_bytes);
}

void LLGLTexture::setAddressMode(LLTexUnit::eTextureAddressMode mode)
{
	llassert(mImageGLp.notNull());
	mImageGLp->setAddressMode(mode);
}

void LLGLTexture::setFilteringOption(LLTexUnit::eTextureFilterOptions option)
{
	llassert(mImageGLp.notNull());
	mImageGLp->setFilteringOption(option);
}

//virtual
S32	LLGLTexture::getWidth(S32 discard_level) const
{
	llassert(mImageGLp.notNull());
	return mImageGLp->getWidth(discard_level);
}

//virtual
S32	LLGLTexture::getHeight(S32 discard_level) const
{
	llassert(mImageGLp.notNull());
	return mImageGLp->getHeight(discard_level);
}

S32 LLGLTexture::getMaxDiscardLevel() const
{
	llassert(mImageGLp.notNull());
	return mImageGLp->getMaxDiscardLevel();
}
S32 LLGLTexture::getDiscardLevel() const
{
	llassert(mImageGLp.notNull());
	return mImageGLp->getDiscardLevel();
}
S8 LLGLTexture::getComponents() const
{
	llassert(mImageGLp.notNull());
	return mImageGLp->getComponents();
}

U32 LLGLTexture::getTexName() const
{
	return mImageGLp.notNull() ? mImageGLp->getTexName() : 0;
}

bool LLGLTexture::hasGLTexture() const
{
	return mImageGLp.notNull() && mImageGLp->getHasGLTexture();
}

bool LLGLTexture::getBoundRecently() const
{
	return mImageGLp.notNull() && mImageGLp->getBoundRecently();
}

LLTexUnit::eTextureType LLGLTexture::getTarget() const
{
	llassert(mImageGLp.notNull());
	return mImageGLp->getTarget();
}

bool LLGLTexture::setSubImage(const LLImageRaw* rawimg, S32 x_pos, S32 y_pos,
							  S32 width, S32 height, U32 use_name)
{
	llassert(mImageGLp.notNull());
	return mImageGLp->setSubImage(rawimg, x_pos, y_pos, width, height, 0,
								  use_name);
}

bool LLGLTexture::setSubImage(const U8* datap, S32 data_width, S32 data_height,
							  S32 x_pos, S32 y_pos, S32 width, S32 height,
							  U32 use_name)
{
	llassert(mImageGLp.notNull());
	return mImageGLp->setSubImage(datap, data_width, data_height, x_pos, y_pos,
								  width, height, 0, use_name);
}

void LLGLTexture::setGLTextureCreated (bool initialized)
{
	llassert(mImageGLp.notNull());
	mImageGLp->setGLTextureCreated (initialized);
}

void LLGLTexture::setTexName(U32 name)
{
	llassert(mImageGLp.notNull());
	mImageGLp->setTexName(name);
}

void LLGLTexture::setTarget(U32 target, LLTexUnit::eTextureType bind_target)
{
	llassert(mImageGLp.notNull());
	mImageGLp->setTarget(target, bind_target);
}

LLTexUnit::eTextureAddressMode LLGLTexture::getAddressMode() const
{
	llassert(mImageGLp.notNull());
	return mImageGLp->getAddressMode();
}

S32 LLGLTexture::getTextureMemory() const
{
	llassert(mImageGLp.notNull());
	return mImageGLp->mTextureMemory;
}

U32 LLGLTexture::getPrimaryFormat() const
{
	llassert(mImageGLp.notNull());
	return mImageGLp->getPrimaryFormat();
}

bool LLGLTexture::getIsAlphaMask() const
{
	llassert(mImageGLp.notNull());
	return mImageGLp->getIsAlphaMask();
}

bool LLGLTexture::getMask(const LLVector2 &tc)
{
	llassert(mImageGLp.notNull());
	return mImageGLp->getMask(tc);
}

F32 LLGLTexture::getTimePassedSinceLastBound()
{
	llassert(mImageGLp.notNull());
	return mImageGLp->getTimePassedSinceLastBound();
}

bool LLGLTexture::isJustBound() const
{
	llassert(mImageGLp.notNull());
	return mImageGLp->isJustBound();
}

void LLGLTexture::forceUpdateBindStats() const
{
	llassert(mImageGLp.notNull());
	mImageGLp->forceUpdateBindStats();
}

void LLGLTexture::destroyGLTexture()
{
	if (mImageGLp.notNull() && mImageGLp->getHasGLTexture())
	{
		mImageGLp->destroyGLTexture();
	}
	mTextureState = DELETED;
}

void LLGLTexture::setTexelsPerImage()
{
	S32 fullwidth = llmin(mFullWidth, (S32)MAX_IMAGE_SIZE_DEFAULT);
	S32 fullheight = llmin(mFullHeight, (S32)MAX_IMAGE_SIZE_DEFAULT);
	mTexelsPerImage = fullwidth * fullheight;
}
