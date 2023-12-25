/**
 * @file lltextureview.h
 * @brief LLTextureView class header file
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

#ifndef LL_LLTEXTUREVIEW_H
#define LL_LLTEXTUREVIEW_H

#include <vector>

#include "llcontainerview.h"

class LLViewerFetchedTexture;
class LLTextureBar;
class LLGLTexMemBar;

class LLTextureView final : public LLContainerView
{
	friend class LLTextureBar;
	friend class LLGLTexMemBar;

protected:
	LOG_CLASS(LLTextureView);

public:
	LLTextureView(const std::string& name);
	~LLTextureView() override;

	void draw() override;
	bool handleMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleMouseUp(S32 x, S32 y, MASK mask) override;
	bool handleKey(KEY key, MASK mask, bool called_from_parent) override;

private:
	bool addBar(LLViewerFetchedTexture* image, S32 hilight = 0);
	void removeAllBars();

private:
    bool                        mFreezeView;
    bool                        mOrderFetch;
    bool                        mPrintList;

	LLTextBox*					mInfoTextp;

	std::vector<LLTextureBar*>	mTextureBars;
	U32							mNumTextureBars;

	LLGLTexMemBar*				mGLTexMemBar;
};

extern LLTextureView* gTextureViewp;

#endif // LL_TEXTURE_VIEW_H
