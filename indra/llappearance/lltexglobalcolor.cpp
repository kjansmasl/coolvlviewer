/**
 * @file lltexlayerglobalcolor.cpp
 * @brief Color for texture layers.
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

#include "linden_common.h"

#include "lltexglobalcolor.h"

#include "llavatarappearance.h"
#include "lltexlayer.h"

class LLWearable;

//-----------------------------------------------------------------------------
// LLTexGlobalColor
//-----------------------------------------------------------------------------

LLTexGlobalColor::LLTexGlobalColor(LLAvatarAppearance* appearance)
:	mAvatarAppearance(appearance),
	mInfo(NULL)
{
}

#if 0	// mParamColorList are LLViewerVisualParam and get deleted with
		// ~LLCharacter()
LLTexGlobalColor::~LLTexGlobalColor()
{
	std::for_each(mParamColorList.begin(), mParamColorList.end(),
				  DeletePointer());
	mParamColorList.clear();
}
#endif

bool LLTexGlobalColor::setInfo(LLTexGlobalColorInfo* info)
{
	llassert(mInfo == NULL);
	mInfo = info;
#if 0	// No ID
	mID = info->mID;
#endif

	mParamGlobalColorList.reserve(mInfo->mParamColorInfoList.size());
	for (param_color_info_list_t::iterator
			iter = mInfo->mParamColorInfoList.begin(),
			end = mInfo->mParamColorInfoList.end();
		 iter != end; ++iter)
	{
		LLTexParamGlobalColor* param_color = new LLTexParamGlobalColor(this);
		if (!param_color->setInfo(*iter, true))
		{
			mInfo = NULL;
			return false;
		}
		mParamGlobalColorList.push_back(param_color);
	}

	return true;
}

// Sum of color params
LLColor4 LLTexGlobalColor::getColor() const
{
	if (mParamGlobalColorList.empty())
	{
		return LLColor4(1.f, 1.f, 1.f, 1.f);
	}

	LLColor4 net_color(0.f, 0.f, 0.f, 0.f);
	LLTexLayer::calculateTexLayerColor(mParamGlobalColorList, net_color);
	return net_color;
}

const std::string& LLTexGlobalColor::getName() const
{
	return mInfo->mName;
}

//-----------------------------------------------------------------------------
// LLTexParamGlobalColor
//-----------------------------------------------------------------------------
LLTexParamGlobalColor::LLTexParamGlobalColor(LLTexGlobalColor* tex_global_color)
:	LLTexLayerParamColor(tex_global_color->getAvatarAppearance()),
	mTexGlobalColor(tex_global_color)
{
}

LLTexParamGlobalColor::LLTexParamGlobalColor(const LLTexParamGlobalColor& other)
:	LLTexLayerParamColor(other),
	mTexGlobalColor(other.mTexGlobalColor)
{
}

//-----------------------------------------------------------------------------
// ~LLTexParamGlobalColor
//-----------------------------------------------------------------------------
//virtual
LLViewerVisualParam* LLTexParamGlobalColor::cloneParam(LLWearable* wearable) const
{
	return new LLTexParamGlobalColor(*this);
}

void LLTexParamGlobalColor::onGlobalColorChanged(bool upload_bake)
{
	mAvatarAppearance->onGlobalColorChanged(mTexGlobalColor, upload_bake);
}

//-----------------------------------------------------------------------------
// LLTexGlobalColorInfo
//-----------------------------------------------------------------------------

LLTexGlobalColorInfo::~LLTexGlobalColorInfo()
{
	for_each(mParamColorInfoList.begin(), mParamColorInfoList.end(),
			 DeletePointer());
	mParamColorInfoList.clear();
}

bool LLTexGlobalColorInfo::parseXml(LLXmlTreeNode* node)
{
	// name attribute
	static LLStdStringHandle name_string = LLXmlTree::addAttributeString("name");
	if (!node->getFastAttributeString(name_string, mName))
	{
		llwarns << "<global_color> element is missing name attribute."
				<< llendl;
		return false;
	}

	// <param> sub-element
	for (LLXmlTreeNode* child = node->getChildByName("param"); child;
		 child = node->getNextNamedChild())
	{
		if (child->getChildByName("param_color"))
		{
			// <param><param_color/></param>
			LLTexLayerParamColorInfo* info = new LLTexLayerParamColorInfo();
			if (!info->parseXml(child))
			{
				delete info;
				return false;
			}
			mParamColorInfoList.push_back(info);
		}
	}
	return true;
}
