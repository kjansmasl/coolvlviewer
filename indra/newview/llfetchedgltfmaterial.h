/**
 * @file llfetchedgltfmaterial.h
 *
 * $LicenseInfo:firstyear=2022&license=viewergpl$
 *
 * Copyright (c) 2022, Linden Research, Inc.
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

#ifndef LL_LLFETCHEDGLTFMATERIAL_H
#define LL_LLFETCHEDGLTFMATERIAL_H

#include <functional>

#include "hbfastset.h"
#include "llgltfmaterial.h"
#include "llpointer.h"

class LLViewerTexture;
class LLViewerFetchedTexture;

class LLFetchedGLTFMaterial : public LLGLTFMaterial
{
	friend class LLGLTFMaterialList;

protected:
	LOG_CLASS(LLFetchedGLTFMaterial);

public:
	LL_INLINE LLFetchedGLTFMaterial()
	:	mExpectedFlushTime(0.f),
		mActive(true),
		mFetching(false)
	{
	}

	LLFetchedGLTFMaterial& operator=(const LLFetchedGLTFMaterial& rhs);

	LLFetchedGLTFMaterial* asFecthed() override	{ return this; }

	void onMaterialComplete(std::function<void()> mat_complete_callback);

	// Bind this material for rendering. media_texp is an optional media
	// texture that may override the base color texture.
	void bind(LLViewerTexture* media_texp, F32 vsize);

	LL_INLINE bool isFetching() const			{ return mFetching; }

	void addTextureEntry(LLTextureEntry* tep) override;
	void removeTextureEntry(LLTextureEntry* tep) override;
	bool replaceLocalTexture(const LLUUID& tracking_id, const LLUUID& old_id,
							 const LLUUID& new_id) override;
	void updateTextureTracking() override;

	typedef fast_hset<LLTextureEntry*> te_list_t;
	LL_INLINE const te_list_t& getTexEntries() const
	{
		return mTextureEntries;
	}

protected:
	// Lifetime management
	LL_INLINE void materialBegin()				{ mFetching = true; }
	void materialComplete();

public:
	// Textures used for fetching/rendering
	LLPointer<LLViewerFetchedTexture>	mBaseColorTexture;
	LLPointer<LLViewerFetchedTexture>	mNormalTexture;
	LLPointer<LLViewerFetchedTexture>	mMetallicRoughnessTexture;
	LLPointer<LLViewerFetchedTexture>	mEmissiveTexture;

protected:
	te_list_t							mTextureEntries;

	// Lifetime management
	std::vector<std::function<void()> >	mCompleteCallbacks;
	F32									mExpectedFlushTime;
	bool								mActive;
	bool								mFetching;
};

#endif	// LL_LLFETCHEDGLTFMATERIAL_H
