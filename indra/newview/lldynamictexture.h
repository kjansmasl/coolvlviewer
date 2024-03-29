/**
 * @file lldynamictexture.h
 * @brief Implementation of LLDynamicTexture class
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

#ifndef LL_LLDYNAMICTEXTURE_H
#define LL_LLDYNAMICTEXTURE_H

#include "llcoord.h"

#include "llviewertexture.h"

class LLViewerDynamicTexture : public LLViewerTexture
{
public:
	enum
	{
		LL_VIEWER_DYNAMIC_TEXTURE = LLViewerTexture::DYNAMIC_TEXTURE,
		LL_TEX_LAYER_SET_BUFFER = LLViewerTexture::INVALID_TEXTURE_TYPE + 1,
		LL_VISUAL_PARAM_HINT,
		LL_VISUAL_PARAM_RESET,
		LL_PREVIEW_ANIMATION,
		LL_IMAGE_PREVIEW_SCULPTED,
		LL_IMAGE_PREVIEW_AVATAR,
		INVALID_DYNAMIC_TEXTURE
	};

protected:
	~LLViewerDynamicTexture() override;

public:
	enum EOrder
	{
		ORDER_FIRST = 0,
		ORDER_MIDDLE = 1,
		ORDER_LAST = 2,
		ORDER_RESET = 3,
		ORDER_COUNT = 4
	};

	LLViewerDynamicTexture(S32 width, S32 height,
					 	   S32 components,		// = 4,
					 	   EOrder order,		// = ORDER_MIDDLE,
					 	   bool clamp);

	S8 getType() const override;

	LL_INLINE S32 getOriginX() const			{ return mOrigin.mX; }
	LL_INLINE S32 getOriginY() const			{ return mOrigin.mY; }

	LL_INLINE S32 getSize()						{ return mFullWidth * mFullHeight * mComponents; }

	LL_INLINE virtual bool needsRender()		{ return true; }
	virtual void preRender(bool clear_depth = true);
	virtual bool render()						{ return false; }
	virtual void postRender(bool success);

	static bool	updateAllInstances();

protected:
	void generateGLTexture();
	void generateGLTexture(S32 internal_format, U32 primary_format,
						   U32 type_format, bool swap_bytes = false);

protected:
	alignas(16) LLCamera	mCamera;

	LLCoordGL				mOrigin;

	bool					mClamp;

	typedef std::set<LLViewerDynamicTexture*> instance_list_t;
	static instance_list_t	sInstances[ORDER_COUNT];

	static S32				sNumRenders;
};

#endif
