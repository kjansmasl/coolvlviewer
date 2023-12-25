/** 
 * @file lltexglobalcolor.h
 * @brief This is global texture color info used by llavatarappearance.
 *
 * $LicenseInfo:firstyear=2008&license=viewergpl$
 * 
 * Copyright (c) 2010, Linden Research, Inc.
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

#ifndef LL_LLTEXGLOBALCOLOR_H
#define LL_LLTEXGLOBALCOLOR_H

#include "llpreprocessor.h"
#include "lltexlayer.h"
#include "lltexlayerparams.h"

class LLAvatarAppearance;
class LLTexGlobalColorInfo;
class LLWearable;

class LLTexGlobalColor
{
public:
	LLTexGlobalColor(LLAvatarAppearance* appearance);
#if 0
	~LLTexGlobalColor();
#endif

	LL_INLINE LLTexGlobalColorInfo* getInfo() const				{ return mInfo; }
	// This sets mInfo and calls initialization functions
	bool setInfo(LLTexGlobalColorInfo* info);
	
	LL_INLINE LLAvatarAppearance* getAvatarAppearance() const	{ return mAvatarAppearance; }
	LLColor4 getColor() const;
	const std::string& getName() const;

private:
	param_color_list_t		mParamGlobalColorList;
	LLAvatarAppearance*		mAvatarAppearance;  // just backlink, don't LLPointer 
	LLTexGlobalColorInfo*	mInfo;
};

// Used by llavatarappearance to determine skin/eye/hair color.
class LLTexGlobalColorInfo
{
	friend class LLTexGlobalColor;

protected:
	LOG_CLASS(LLTexGlobalColorInfo);

public:
	LLTexGlobalColorInfo() = default;
	~LLTexGlobalColorInfo();

	bool parseXml(LLXmlTreeNode* node);

private:
	param_color_info_list_t	mParamColorInfoList;
	std::string				mName;
};

class alignas(16) LLTexParamGlobalColor : public LLTexLayerParamColor
{
public:
	LLTexParamGlobalColor(LLTexGlobalColor* tex_color);

	LLViewerVisualParam* cloneParam(LLWearable* wearable) const override;

protected:
	LLTexParamGlobalColor(const LLTexParamGlobalColor& other);

	void onGlobalColorChanged(bool upload_bake) override;

private:
	LLTexGlobalColor* mTexGlobalColor;
};

#endif
