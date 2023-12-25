/**
 * @file llgltexture.h
 * @brief Object for managing OpenGL textures
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

#ifndef LL_GL_TEXTURE_H
#define LL_GL_TEXTURE_H

#include "llgl.h"
#include "llrefcount.h"
#include "llrender.h"

// LL's code implicitely sets texture NO_DELETE when calling dontDiscard()
// or setBoostLevel() with most level values. This is a BOGUS thing to do,
// since it causes many textures that do not deserve/require it to stay forever
// in memory. To reenable this bogus behaviour, set this to 1. HB
#define LL_IMPLICIT_SETNODELETE 0

class LLFontGL;
class LLImageGL;
class LLImageRaw;
class LLUUID;
class LLViewerFetchedTexture;

// This is the parent for the LLViewerTexture class. Via its virtual methods,
// the LLViewerTexture class can be reached from llrender.

// Note: LLGLTexture must now derive from LLThreadSafeRefCount instead of
// LLRefCount because ref() and unref() are used in GL threads. HB
class LLGLTexture : public virtual LLThreadSafeRefCount
{
	friend class LLFontGL;
	friend class LLTexUnit;

protected:
	LOG_CLASS(LLGLTexture);

public:
	enum
	{
		MAX_IMAGE_SIZE_DEFAULT = 1024,
		INVALID_DISCARD_LEVEL = 0x7fff
	};

	enum EBoostLevel : U32
	{
		BOOST_NONE = 0,
		// Equivalent to BOOST_NONE when ALM is on, max discard when off. Not
		// used any more by LL's PBR viewer (since it always downloads all
		// materials).
		BOOST_ALM,
		BOOST_AVATAR,
		BOOST_CLOUDS,
		BOOST_HIGH = 10,
		BOOST_SCULPTED,		// Has to be high prio to rez fast enough. HB
		BOOST_TERRAIN,		// Has to be high prio for minimap/low detail
		BOOST_SELECTED,
		// Textures higher than this need to be downloaded at the required
		// resolution without delay.
		BOOST_SUPER_HIGH,
		// Textures bearing these priorities and set NO_DELETE are never forced
		// back to the ACTIVE state by LLImageGL::activateStaleTextures(). HB
		BOOST_AVATAR_SELF,
		BOOST_HUD,
		BOOST_UI,			// Automatically set ALWAYS_KEEP & no discard. HB
		BOOST_BUMP,
		BOOST_MEDIA,
		BOOST_PREVIEW,
		BOOST_MAP,			// Always implicitely set NO_DELETE. HB
		BOOST_MAX_LEVEL
	};

	typedef enum
	{
		// After the GL image (mImageGLp) has been removed from memory.
		DELETED = 0,
		// Ready to be removed from memory
		DELETION_CANDIDATE,
		// Set when not having been used for a certain period (30 seconds).
		INACTIVE,
		// Just being used, can become inactive if not being used for a certain
		// time (10 seconds).
		ACTIVE,
		// Stays in memory, cannot be removed, unless set forceActive().
		NO_DELETE = 99,
		// Stays in memory, cannot be removed at all. Only for UI textures. HB
		ALWAYS_KEEP = 100
	} eState;

protected:
	~LLGLTexture() override;

public:
	LLGLTexture(bool usemipmaps = true);
	LLGLTexture(const LLImageRaw* raw, bool usemipmaps);
	LLGLTexture(U32 width, U32 height, U8 components, bool usemipmaps);

	// Logs debug info
	virtual void dump();

	virtual const LLUUID& getID() const = 0;

	virtual void setBoostLevel(U32 level);
	LL_INLINE U32 getBoostLevel()						{ return mBoostLevel; }

	LL_INLINE S32 getFullWidth() const					{ return mFullWidth; }
	LL_INLINE S32 getFullHeight() const					{ return mFullHeight; }

	void generateGLTexture();
	void destroyGLTexture();

	LL_INLINE virtual LLViewerFetchedTexture* asFetched()
	{
		return NULL;
	}

	LL_INLINE void setActive()
	{
		if (mTextureState < NO_DELETE)
		{
			mTextureState = ACTIVE;
		}
	}

	LL_INLINE void forceActive()
	{
		if (mTextureState != ALWAYS_KEEP)
		{
			mTextureState = ACTIVE;
		}
	}

	LL_INLINE void setNoDelete()
	{
		if (mTextureState != ALWAYS_KEEP)
		{
			mTextureState = NO_DELETE;
		}
	}

	LL_INLINE bool isNoDelete() const
	{
		return mTextureState == NO_DELETE;
	}

	LL_INLINE void dontDiscard()
	{
		mDontDiscard = true;
#if LL_IMPLICIT_SETNODELETE
		setNoDelete();
#endif
	}

	LL_INLINE bool getDontDiscard() const				{ return mDontDiscard; }

	//-------------------------------------------------------------------------
	// Methods to access LLImageGL
	//-------------------------------------------------------------------------

	bool hasGLTexture() const;
	U32 getTexName() const;
	bool createGLTexture();
	// Creates a GL texture from a raw image. With:
    //  - discard_level: mip level, 0 for highest resolution mip
    // - imageraw: the image to copy from
    // - usename: explicit GL name override
    // - to_create: false to force GL texture to not be created
    // - defer_copy: true to allocate GL texture but NOT initialize with
	//               imageraw data
    // - tex_name: if not null, will be set to the GL name of the texture
	//             created
	bool createGLTexture(S32 discard_level, const LLImageRaw* imageraw,
						 S32 usename = 0, bool to_create = true,
						 bool defer_copy = false, U32* tex_name = NULL);

	void setFilteringOption(LLTexUnit::eTextureFilterOptions option);

	void setExplicitFormat(S32 internal_format, U32 primary_format,
						   U32 type_format = 0, bool swap_bytes = false);

	void setAddressMode(LLTexUnit::eTextureAddressMode mode);

	bool setSubImage(const LLImageRaw* imageraw, S32 x_pos, S32 y_pos,
					 S32 width, S32 height, U32 use_name = 0);
	bool setSubImage(const U8* datap, S32 data_width, S32 data_height,
					 S32 x_pos, S32 y_pos, S32 width, S32 height,
					 U32 use_name = 0);

	void setGLTextureCreated (bool initialized);
	// For forcing with externally created textures only:
	void setTexName(U32 name);
	void setTarget(U32 target, LLTexUnit::eTextureType bind_target);

	LLTexUnit::eTextureAddressMode getAddressMode() const;
	S32 getMaxDiscardLevel() const;
	S32 getDiscardLevel() const;
	S8 getComponents() const;
	bool getBoundRecently() const;
	S32 getTextureMemory() const;
	U32 getPrimaryFormat() const;
	bool getIsAlphaMask() const;
	LLTexUnit::eTextureType getTarget() const;
	bool getMask(const LLVector2 &tc);
	F32 getTimePassedSinceLastBound();
	bool isJustBound()const;
	void forceUpdateBindStats() const;

	LL_INLINE S32 getTextureState() const				{ return mTextureState; }

	//-------------------------------------------------------------------------
	// Virtual interface to access LLViewerTexture
	//-------------------------------------------------------------------------

	virtual S32 getWidth(S32 discard_level = -1) const;
	virtual S32 getHeight(S32 discard_level = -1) const;

	virtual S8 getType() const = 0;
	virtual void setKnownDrawSize(S32 width, S32 height) = 0;
	virtual bool bindDefaultImage(S32 stage = 0) = 0;
	virtual void forceImmediateUpdate() = 0;

	LLImageGL* getGLImage() const;

private:
	void cleanup();
	void init();

protected:
	void setTexelsPerImage();

protected:
	// Note: the first member variable is 32 bits in order to align on 64 bits
	// for the next variable, counting the (assumed/normally, for lock-free
	// std::atomic implementations) 32 bits atomic counter from
	// LLThreadSafeRefCount. HB
	S32						mTextureState;

	LLPointer<LLImageGL>	mImageGLp;

	U32						mBoostLevel;	// enum describing priority level
	S32						mFullWidth;
	S32						mFullHeight;
	S32						mTexelsPerImage;
	S8						mComponents;

	bool					mUseMipMaps;

	// Set to true to keep full resolution version of this image (for UI, etc)
	bool					mDontDiscard;

	mutable bool			mNeedsGLTexture;
};

#endif // LL_GL_TEXTURE_H
