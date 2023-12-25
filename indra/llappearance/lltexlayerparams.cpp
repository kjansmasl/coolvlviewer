/**
 * @file lltexlayerparams.cpp
 * @brief Texture layer parameters
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
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

#include "lltexlayerparams.h"

#include "llavatarappearance.h"
#include "llimagetga.h"
#include "llquantize.h"
#include "lltexlayer.h"
#include "lltexturemanagerbridge.h"
#include "llrenderutils.h"
#include "hbtracy.h"
#include "llwearable.h"

//-----------------------------------------------------------------------------
// LLTexLayerParam
//-----------------------------------------------------------------------------

LLTexLayerParam::LLTexLayerParam(LLTexLayerInterface* layer)
:	LLViewerVisualParam(),
	mTexLayer(layer),
	mAvatarAppearance(NULL)
{
	if (mTexLayer)
	{
		mAvatarAppearance = mTexLayer->getTexLayerSet()->getAvatarAppearance();
	}
	else
	{
		llerrs << "LLTexLayerParam constructor passed with NULL reference for layer !"
			   << llendl;
	}
}

LLTexLayerParam::LLTexLayerParam(LLAvatarAppearance* appearance)
:	LLViewerVisualParam(),
	mTexLayer(NULL),
	mAvatarAppearance(appearance)
{
}

LLTexLayerParam::LLTexLayerParam(const LLTexLayerParam& other)
:	LLViewerVisualParam(other),
	mTexLayer(other.mTexLayer),
	mAvatarAppearance(other.mAvatarAppearance)
{
}

bool LLTexLayerParam::setInfo(LLViewerVisualParamInfo* info,
							  bool add_to_appearance)
{
	LLViewerVisualParam::setInfo(info);

	if (add_to_appearance)
	{
		mAvatarAppearance->addVisualParam( this);
		this->setParamLocation(mAvatarAppearance->isSelf() ? LOC_AV_SELF
														   : LOC_AV_OTHER);
	}

	return true;
}

//-----------------------------------------------------------------------------
// LLTexLayerParamAlpha
//-----------------------------------------------------------------------------

// static
LLTexLayerParamAlpha::param_alpha_ptr_list_t LLTexLayerParamAlpha::sInstances;

// static
void LLTexLayerParamAlpha::dumpCacheByteCount()
{
	S32 gl_bytes = 0;
	getCacheByteCount( &gl_bytes);
	llinfos << "Processed Alpha Texture Cache GL:" << gl_bytes / 1024 << "KB"
			<< llendl;
}

// static
void LLTexLayerParamAlpha::getCacheByteCount(S32* gl_bytes)
{
	*gl_bytes = 0;

	for (param_alpha_ptr_list_t::iterator iter = sInstances.begin(),
										  end = sInstances.end();
		 iter != end; ++iter)
	{
		LLTexLayerParamAlpha* instance = *iter;
		LLGLTexture* tex = instance->mCachedProcessedTexture;
		if (tex)
		{
			S32 bytes = (S32)tex->getWidth() * tex->getHeight() * tex->getComponents();

			if (tex->hasGLTexture())
			{
				*gl_bytes += bytes;
			}
		}
	}
}

LLTexLayerParamAlpha::LLTexLayerParamAlpha(LLTexLayerInterface* layer)
:	LLTexLayerParam(layer),
	mCachedProcessedTexture(NULL),
	mStaticImageTGA(),
	mStaticImageRaw(),
	mNeedsCreateTexture(false),
	mStaticImageInvalid(false),
	mAvgDistortionVec(1.f, 1.f, 1.f),
	mCachedEffectiveWeight(0.f)
{
	sInstances.push_front(this);
}

LLTexLayerParamAlpha::LLTexLayerParamAlpha(LLAvatarAppearance* appearance)
:	LLTexLayerParam(appearance),
	mCachedProcessedTexture(NULL),
	mStaticImageTGA(),
	mStaticImageRaw(),
	mNeedsCreateTexture(false),
	mStaticImageInvalid(false),
	mAvgDistortionVec(1.f, 1.f, 1.f),
	mCachedEffectiveWeight(0.f)
{
	sInstances.push_front(this);
}

LLTexLayerParamAlpha::LLTexLayerParamAlpha(const LLTexLayerParamAlpha& other)
:	LLTexLayerParam(other),
	mCachedProcessedTexture(other.mCachedProcessedTexture),
	mStaticImageTGA(other.mStaticImageTGA),
	mStaticImageRaw(other.mStaticImageRaw),
	mNeedsCreateTexture(other.mNeedsCreateTexture),
	mStaticImageInvalid(other.mStaticImageInvalid),
	mAvgDistortionVec(other.mAvgDistortionVec),
	mCachedEffectiveWeight(other.mCachedEffectiveWeight)
{
	sInstances.push_front(this);
}

LLTexLayerParamAlpha::~LLTexLayerParamAlpha()
{
	deleteCaches();
	sInstances.remove(this);
}

//virtual
LLViewerVisualParam* LLTexLayerParamAlpha::cloneParam(LLWearable* wearable) const
{
	return new LLTexLayerParamAlpha(*this);
}

void LLTexLayerParamAlpha::deleteCaches()
{
	mStaticImageTGA = NULL; // deletes image
	mCachedProcessedTexture = NULL;
	mStaticImageRaw = NULL;
	mNeedsCreateTexture = false;
}

bool LLTexLayerParamAlpha::getMultiplyBlend() const
{
	return ((LLTexLayerParamAlphaInfo*)getInfo())->mMultiplyBlend;
}

void LLTexLayerParamAlpha::setWeight(F32 weight, bool upload_bake)
{
	if (mIsAnimating || mTexLayer == NULL)
	{
		return;
	}
	F32 min_weight = getMinWeight();
	F32 max_weight = getMaxWeight();
	F32 new_weight = llclamp(weight, min_weight, max_weight);
	U8 cur_u8 = F32_to_U8(mCurWeight, min_weight, max_weight);
	U8 new_u8 = F32_to_U8(new_weight, min_weight, max_weight);
	if (cur_u8 != new_u8)
	{
		mCurWeight = new_weight;

		// only trigger a baked texture update if we're changing a wearable's
		// visual param.
		if ((mAvatarAppearance->getSex() & getSex()) &&
			mAvatarAppearance->isSelf() && !mIsDummy)
		{
			mAvatarAppearance->invalidateComposite(mTexLayer->getTexLayerSet(),
												   upload_bake);
			mTexLayer->invalidateMorphMasks();
		}
	}
}

void LLTexLayerParamAlpha::setAnimationTarget(F32 target_value,
											  bool upload_bake)
{
	// do not animate dummy parameters
	if (mIsDummy)
	{
		setWeight(target_value, upload_bake);
		return;
	}

	mTargetWeight = target_value;
	setWeight(target_value, upload_bake);
	mIsAnimating = true;
	if (mNext)
	{
		mNext->setAnimationTarget(target_value, upload_bake);
	}
}

void LLTexLayerParamAlpha::animate(F32 delta, bool upload_bake)
{
	if (mNext)
	{
		mNext->animate(delta, upload_bake);
	}
}

bool LLTexLayerParamAlpha::getSkip() const
{
	if (!mTexLayer)
	{
		return true;
	}

	const LLAvatarAppearance* appearance = mTexLayer->getTexLayerSet()->getAvatarAppearance();

	if (((LLTexLayerParamAlphaInfo*)getInfo())->mSkipIfZeroWeight)
	{
		F32 effective_weight = (appearance->getSex() & getSex()) ? mCurWeight
																 : getDefaultWeight();
		if (is_approx_zero(effective_weight))
		{
			return true;
		}
	}

	LLWearableType::EType type = (LLWearableType::EType)getWearableType();
	if (type != LLWearableType::WT_INVALID &&
		!appearance->isWearingWearableType(type))
	{
		return true;
	}

	return false;
}

bool LLTexLayerParamAlpha::render(S32 x, S32 y, S32 width, S32 height)
{
	LL_TRACY_TIMER(TRC_TEX_LAYER_PARAM_ALPHA);
	bool success = true;

	if (!mTexLayer)
	{
		return success;
	}

	F32 effective_weight = (mTexLayer->getTexLayerSet()->getAvatarAppearance()->getSex() & getSex()) ? mCurWeight
																									 : getDefaultWeight();
	bool weight_changed = effective_weight != mCachedEffectiveWeight;
	if (getSkip())
	{
		return success;
	}

	gGL.flush();

	LLTexLayerParamAlphaInfo* info = (LLTexLayerParamAlphaInfo*)getInfo();
	if (info->mMultiplyBlend)
	{
		// Multiplication: approximates a min() function
		gGL.blendFunc(LLRender::BF_DEST_ALPHA, LLRender::BF_ZERO);
	}
	else
	{
		// Addition: approximates a max() function
		gGL.setSceneBlendType(LLRender::BT_ADD);
	}

	if (!info->mStaticImageFileName.empty() && !mStaticImageInvalid)
	{
		if (mStaticImageTGA.isNull())
		{
			// Don't load the image file until we actually need it the first
			// time. Like now.
			mStaticImageTGA = gTexLayerStaticImageList.getImageTGA(info->mStaticImageFileName);
			if (mStaticImageTGA.notNull())
			{
				// We now have something in one of our caches
				LLTexLayerSet::sHasCaches = true;
			}
			else
			{
				llwarns << "Unable to load static file: "
						<< info->mStaticImageFileName << llendl;
				mStaticImageInvalid = true; // don't try again.
				return false;
			}
		}

		const S32 image_tga_width = mStaticImageTGA->getWidth();
		const S32 image_tga_height = mStaticImageTGA->getHeight();
		if (weight_changed || mCachedProcessedTexture.isNull() ||
			mCachedProcessedTexture->getWidth() != image_tga_width ||
			mCachedProcessedTexture->getHeight() != image_tga_height)
		{
			mCachedEffectiveWeight = effective_weight;

			if (!mCachedProcessedTexture)
			{
				mCachedProcessedTexture = NULL;
				if (gTextureManagerBridgep)
				{
					mCachedProcessedTexture = gTextureManagerBridgep->getLocalTexture(image_tga_width,
																					  image_tga_height,
																					  1, false);
				}
				if (mCachedProcessedTexture.notNull())
				{
					// We now have something in one of our caches
					LLTexLayerSet::sHasCaches = true;
					mCachedProcessedTexture->setExplicitFormat(GL_ALPHA8,
															   GL_ALPHA);
				}
				else
				{
					llwarns << "Unable to get local texture for: "
							<< info->mStaticImageFileName << llendl;
					mStaticImageTGA = NULL;
					mStaticImageInvalid = true; // don't try again.
					return false;
				}
			}

			// Applies domain and effective weight to data as it is decoded.
			// Also resizes the raw image if needed.
			mStaticImageRaw = NULL;
			mStaticImageRaw = new LLImageRaw;
			mStaticImageTGA->decodeAndProcess(mStaticImageRaw, info->mDomain,
											  effective_weight);
			mNeedsCreateTexture = true;
			LL_DEBUGS("TexLayerParams") << "Built Cached Alpha: "
										<< info->mStaticImageFileName << ": ("
										<< mStaticImageRaw->getWidth() << ", "
										<< mStaticImageRaw->getHeight()
										<< ") - Domain: " << info->mDomain
										<< " - Weight: " << effective_weight
										<< LL_ENDL;
		}

		if (mCachedProcessedTexture)
		{
			LLTexUnit* unit0 = gGL.getTexUnit(0);
			// Create the GL texture, and then hang onto it for future use.
			if (mNeedsCreateTexture)
			{
				mCachedProcessedTexture->createGLTexture(0, mStaticImageRaw);
				mNeedsCreateTexture = false;
				unit0->bind(mCachedProcessedTexture);
				mCachedProcessedTexture->setAddressMode(LLTexUnit::TAM_CLAMP);
			}

			unit0->bind(mCachedProcessedTexture);
			gl_rect_2d_simple_tex(width, height);
			unit0->unbind(LLTexUnit::TT_TEXTURE);
		}

		// Do not keep the cache for other people's avatars
		// (it is not really a "cache" in that case, but the logic is the same)
		if (!mAvatarAppearance->isSelf())
		{
			mCachedProcessedTexture = NULL;
		}
	}
	else
	{
		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
		gGL.color4f(0.f, 0.f, 0.f, effective_weight);
		gl_rect_2d_simple(width, height);
	}

	stop_glerror();

	return success;
}

//-----------------------------------------------------------------------------
// LLTexLayerParamAlphaInfo
//-----------------------------------------------------------------------------

LLTexLayerParamAlphaInfo::LLTexLayerParamAlphaInfo()
:	mMultiplyBlend(false),
	mSkipIfZeroWeight(false),
	mDomain(0.f)
{
}

bool LLTexLayerParamAlphaInfo::parseXml(LLXmlTreeNode* node)
{
	llassert(node->hasName("param") && node->getChildByName("param_alpha"));

	if (!LLViewerVisualParamInfo::parseXml(node))
	{
		return false;
	}

	LLXmlTreeNode* param_alpha_node = node->getChildByName("param_alpha");
	if (!param_alpha_node)
	{
		return false;
	}

	// NOTE: don't load the image file until it's actually needed.
	static LLStdStringHandle tga_file_string = LLXmlTree::addAttributeString("tga_file");
	param_alpha_node->getFastAttributeString(tga_file_string,
											 mStaticImageFileName);

	static LLStdStringHandle multiply_blend_string = LLXmlTree::addAttributeString("multiply_blend");
	param_alpha_node->getFastAttributeBool(multiply_blend_string,
										   mMultiplyBlend);

	static LLStdStringHandle skip_if_zero_string = LLXmlTree::addAttributeString("skip_if_zero");
	param_alpha_node->getFastAttributeBool(skip_if_zero_string,
										   mSkipIfZeroWeight);

	static LLStdStringHandle domain_string = LLXmlTree::addAttributeString("domain");
	param_alpha_node->getFastAttributeF32(domain_string, mDomain);

	return true;
}

LLTexLayerParamColor::LLTexLayerParamColor(LLTexLayerInterface* layer)
:	LLTexLayerParam(layer),
	mAvgDistortionVec(1.f, 1.f, 1.f)
{
}

LLTexLayerParamColor::LLTexLayerParamColor(LLAvatarAppearance* appearance)
:	LLTexLayerParam(appearance),
	mAvgDistortionVec(1.f, 1.f, 1.f)
{
}

LLTexLayerParamColor::LLTexLayerParamColor(const LLTexLayerParamColor& other)
:	LLTexLayerParam(other),
	mAvgDistortionVec(other.mAvgDistortionVec)
{
}

//virtual
LLViewerVisualParam* LLTexLayerParamColor::cloneParam(LLWearable* wearable) const
{
	return new LLTexLayerParamColor(*this);
}

LLColor4 LLTexLayerParamColor::getNetColor() const
{
	const LLTexLayerParamColorInfo* info = (LLTexLayerParamColorInfo*)getInfo();

	llassert(info->mNumColors >= 1);

	F32 effective_weight = mAvatarAppearance && (mAvatarAppearance->getSex() & getSex()) ? mCurWeight
																						 : getDefaultWeight();

	S32 index_last = info->mNumColors - 1;
	F32 scaled_weight = effective_weight * index_last;
	S32 index_start = (S32) scaled_weight;
	S32 index_end = index_start + 1;
	if (index_start == index_last)
	{
		return info->mColors[index_last];
	}
	else
	{
		F32 weight = scaled_weight - index_start;
		const LLColor4 *start = &info->mColors[index_start];
		const LLColor4 *end   = &info->mColors[index_end];
		return LLColor4((1.f - weight) * start->mV[VX] + weight * end->mV[VX],
						(1.f - weight) * start->mV[VY] + weight * end->mV[VY],
						(1.f - weight) * start->mV[VZ] + weight * end->mV[VZ],
						(1.f - weight) * start->mV[VW] + weight * end->mV[VW]);
	}
}

void LLTexLayerParamColor::setWeight(F32 weight, bool upload_bake)
{
	if (mIsAnimating)
	{
		return;
	}

	const LLTexLayerParamColorInfo* info = (LLTexLayerParamColorInfo*)getInfo();
	F32 min_weight = getMinWeight();
	F32 max_weight = getMaxWeight();
	F32 new_weight = llclamp(weight, min_weight, max_weight);
	U8 cur_u8 = F32_to_U8(mCurWeight, min_weight, max_weight);
	U8 new_u8 = F32_to_U8(new_weight, min_weight, max_weight);
	if (cur_u8 != new_u8)
	{
		mCurWeight = new_weight;

		if (info->mNumColors <= 0)
		{
			// This will happen when we set the default weight the first time.
			return;
		}

		// only trigger a baked texture update if we're changing a wearable's
		// visual param.
		if ((mAvatarAppearance->getSex() & getSex()) &&
			mAvatarAppearance->isSelf() && !mIsDummy)
		{
			onGlobalColorChanged(upload_bake);
			if (mTexLayer)
			{
				mAvatarAppearance->invalidateComposite(mTexLayer->getTexLayerSet(),
													   upload_bake);
			}
		}
	}
}

void LLTexLayerParamColor::setAnimationTarget(F32 target_value,
											  bool upload_bake)
{
	// set value first then set interpolating flag to ignore further updates
	mTargetWeight = target_value;
	setWeight(target_value, upload_bake);
	mIsAnimating = true;
	if (mNext)
	{
		mNext->setAnimationTarget(target_value, upload_bake);
	}
}

void LLTexLayerParamColor::animate(F32 delta, bool upload_bake)
{
	if (mNext)
	{
		mNext->animate(delta, upload_bake);
	}
}

//-----------------------------------------------------------------------------
// LLTexLayerParamColorInfo
//-----------------------------------------------------------------------------

LLTexLayerParamColorInfo::LLTexLayerParamColorInfo()
:	mOperation(LLTexLayerParamColor::OP_ADD),
	mNumColors(0)
{
}

bool LLTexLayerParamColorInfo::parseXml(LLXmlTreeNode* node)
{
	llassert(node->hasName("param") && node->getChildByName("param_color"));

	if (!LLViewerVisualParamInfo::parseXml(node))
	{
		return false;
	}

	LLXmlTreeNode* param_color_node = node->getChildByName("param_color");
	if (!param_color_node)
	{
		return false;
	}

	std::string op_string;
	static LLStdStringHandle operation_string = LLXmlTree::addAttributeString("operation");
	if (param_color_node->getFastAttributeString(operation_string, op_string))
	{
		LLStringUtil::toLower(op_string);
		if (op_string == "add")
		{
	 		mOperation = LLTexLayerParamColor::OP_ADD;
		}
		else if	(op_string == "multiply")
		{
			mOperation = LLTexLayerParamColor::OP_MULTIPLY;
		}
		else if	(op_string == "blend")
		{
		    mOperation = LLTexLayerParamColor::OP_BLEND;
		}
	}

	mNumColors = 0;

	LLColor4U color4u;
	for (LLXmlTreeNode* child = param_color_node->getChildByName("value");
		 child; child = param_color_node->getNextNamedChild())
	{
		if (mNumColors < MAX_COLOR_VALUES)
		{
			static LLStdStringHandle color_string = LLXmlTree::addAttributeString("color");
			if (child->getFastAttributeColor4U(color_string, color4u))
			{
				mColors[mNumColors++].set(color4u);
			}
		}
	}
	if (!mNumColors)
	{
		llwarns << "<param_color> is missing <value> sub-elements" << llendl;
		return false;
	}

	if (mOperation == LLTexLayerParamColor::OP_BLEND && mNumColors != 1)
	{
		llwarns << "<param_color> with operation\"blend\" must have exactly one <value>"
				<< llendl;
		return false;
	}

	return true;
}
