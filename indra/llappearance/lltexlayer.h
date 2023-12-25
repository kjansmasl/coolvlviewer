/**
 * @file lltexlayer.h
 * @brief Texture layer classes. Used for avatars.
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

#ifndef LL_LLTEXLAYER_H
#define LL_LLTEXLAYER_H

#include <deque>
#include <map>

#include "llavatarappearancedefines.h"
#include "hbfastmap.h"
#include "llgl.h"
#include "llgltexture.h"
#include "llpreprocessor.h"
#include "lltexlayerparams.h"

class LLAvatarAppearance;
class LLImageTGA;
class LLImageRaw;
class LLLocalTextureObject;
class LLTexLayerInfo;
class LLTexLayerSet;
class LLTexLayerSetBuffer;
class LLTexLayerSetInfo;
class LLViewerTexLayerSet;
class LLViewerTexLayerSetBuffer;
class LLViewerVisualParam;
class LLWearable;
class LLXmlTreeNode;

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// LLTexLayerInterface
// Interface class to generalize functionality shared by LLTexLayer and
// LLTexLayerTemplate.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLTexLayerInterface
{
public:
	enum ERenderPass
	{
		RP_COLOR,
		RP_BUMP,
		RP_SHINE
	};

	LLTexLayerInterface(LLTexLayerSet* const layer_set);
	LLTexLayerInterface(const LLTexLayerInterface& layer,
						LLWearable* wearable);
	virtual ~LLTexLayerInterface() = default;

	virtual bool render(S32 x, S32 y, S32 width, S32 height) = 0;
	virtual void deleteCaches() = 0;
	virtual bool blendAlphaTexture(S32 x, S32 y, S32 width, S32 height) = 0;
	virtual bool isInvisibleAlphaMask() const = 0;

	LL_INLINE const LLTexLayerInfo* getInfo() const 	{ return mInfo; }

	// sets mInfo, calls initialization functions:
	virtual bool setInfo(const LLTexLayerInfo* info, LLWearable* wearable);

	LLWearableType::EType getWearableType() const;
	LLAvatarAppearanceDefines::ETextureIndex getLocalTextureIndex() const;

	const std::string& getName() const;

	LL_INLINE const LLTexLayerSet* const getTexLayerSet() const
	{
		return mTexLayerSet;
	}

	LL_INLINE LLTexLayerSet* const getTexLayerSet() 	{ return mTexLayerSet; }

	void invalidateMorphMasks();
	LL_INLINE virtual void setHasMorph(bool newval) 	{ mHasMorph = newval; }
	LL_INLINE bool hasMorph() const						{ return mHasMorph; }
	LL_INLINE bool isMorphValid() const					{ return mMorphMasksValid; }

	void requestUpdate();
	virtual void gatherAlphaMasks(U8* datap, S32 origin_x, S32 origin_y,
								  S32 width, S32 height) = 0;
	LL_INLINE bool hasAlphaParams() const 				{ return !mParamAlphaList.empty(); }

	ERenderPass getRenderPass() const;
	bool isVisibilityMask() const;

	LL_INLINE virtual void asLLSD(LLSD& sd) const		{}

protected:
	const std::string& getGlobalColor() const;
	LLViewerVisualParam* getVisualParamPtr(S32 index) const;

protected:
	LLTexLayerSet* const	mTexLayerSet;
	const LLTexLayerInfo*	mInfo;
	bool					mMorphMasksValid;
	bool					mHasMorph;

	// Layers can have either mParamColorList, mGlobalColor, or mFixedColor.
	// They are looked for in that order.
	param_color_list_t		mParamColorList;
	param_alpha_list_t		mParamAlphaList;
	// 						mGlobalColor name stored in mInfo
	// 						mFixedColor value stored in mInfo
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// LLTexLayerTemplate
// Only exists for LLVOAvatarSelf.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLTexLayerTemplate : public LLTexLayerInterface
{
public:
	LLTexLayerTemplate(LLTexLayerSet* const layer_setp,
					   LLAvatarAppearance* const appearance);
	LLTexLayerTemplate(const LLTexLayerTemplate& layer);

	bool render(S32 x, S32 y, S32 width, S32 height) override;

	// This sets mInfo and calls initialization functions:
	bool setInfo(const LLTexLayerInfo* info, LLWearable* wearable) override;

	// Multiplies a single alpha texture against the frame buffer:
	bool blendAlphaTexture(S32 x, S32 y, S32 width, S32 height) override;
	void gatherAlphaMasks(U8* datap, S32 origin_x, S32 origin_y, S32 width,
						  S32 height) override;
	void setHasMorph(bool newval) override;
	void deleteCaches() override;
	bool isInvisibleAlphaMask() const override;

protected:
	U32 updateWearableCache() const;
	LLTexLayer* getLayer(U32 i) const;

	LL_INLINE LLAvatarAppearance* getAvatarAppearance() const
	{
		return mAvatarAppearance;
	}

private:
	// Note: backlink only; do not make this an LLPointer.
	LLAvatarAppearance*	const mAvatarAppearance;

	typedef std::vector<LLWearable*> wearable_cache_t;

	// mutable b/c most get- require updating this cache
	mutable wearable_cache_t mWearableCache;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// LLTexLayer
// A single texture layer. Only exists for LLVOAvatarSelf.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLTexLayer : public LLTexLayerInterface
{
public:
	LLTexLayer(LLTexLayerSet* const layer_setp);
	LLTexLayer(const LLTexLayer& layer, LLWearable* wearablep);
	LLTexLayer(const LLTexLayerTemplate& layer_template,
			   LLLocalTextureObject* ltop, LLWearable* wearablep);

	~LLTexLayer() override;

	// This sets mInfo and calls initialization functions:
	bool setInfo(const LLTexLayerInfo* info, LLWearable* wearable) override;
	bool render(S32 x, S32 y, S32 width, S32 height) override;

	void deleteCaches() override;
	const U8* getAlphaData() const;

	bool findNetColor(LLColor4* color) const;

	// Multiplies a single alpha texture against the frame buffer
	bool blendAlphaTexture(S32 x, S32 y, S32 width, S32 height) override;

	void gatherAlphaMasks(U8* datap, S32 origin_x, S32 origin_y, S32 width,
						  S32 height) override;
	void renderMorphMasks(S32 x, S32 y, S32 width, S32 height,
						  const LLColor4& layer_color, bool force_render);
	void addAlphaMask(U8* datap, S32 origin_x, S32 origin_y, S32 width,
					  S32 height);
	bool isInvisibleAlphaMask() const override;

	LL_INLINE void setLTO(LLLocalTextureObject* ltop) 	{ mLocalTextureObject = ltop; }
	LL_INLINE LLLocalTextureObject* getLTO() 			{ return mLocalTextureObject; }

	void asLLSD(LLSD& sd) const override;

	static void calculateTexLayerColor(const param_color_list_t& param_list,
									   LLColor4& net_color);
protected:
	LLUUID getUUID() const;

protected:
	typedef fast_hmap<U32, U8*> alpha_cache_t;
	alpha_cache_t			mAlphaCache;
	LLLocalTextureObject* 	mLocalTextureObject;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// LLTexLayerSet
// An ordered set of texture layers that gets composited into a single texture.
// Only exists for LLVOAvatarSelf.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLTexLayerSet
{
	friend class LLTexLayerSetBuffer;

public:
	LLTexLayerSet(LLAvatarAppearance* const appearance);
	virtual ~LLTexLayerSet();

	LL_INLINE virtual LLViewerTexLayerSet* asViewerTexLayerSet()
	{
		return NULL;
	}

	LLTexLayerSetBuffer* getComposite();
	// Does not create one if it does not exist:
	const LLTexLayerSetBuffer* getComposite() const;

	virtual void createComposite() = 0;
	void destroyComposite();
	void gatherMorphMaskAlpha(U8* datap, S32 origin_x, S32 origin_y, S32 width,
							  S32 height);

	LL_INLINE const LLTexLayerSetInfo* getInfo() const 	{ return mInfo; }
	// This sets mInfo and calls initialization functions:
	bool setInfo(const LLTexLayerSetInfo* info);

	bool render(S32 x, S32 y, S32 width, S32 height);
	void renderAlphaMaskTextures(S32 x, S32 y, S32 width, S32 height,
								 bool force_Clear = false);

	bool isBodyRegion(const std::string& region) const;
	void applyMorphMask(U8* tex_data, S32 width, S32 height, S32 num_comps);
	bool isMorphValid() const;
	virtual void requestUpdate() = 0;
	void invalidateMorphMasks();
	void deleteCaches();
	LLTexLayerInterface* findLayerByName(const std::string& name);
	void cloneTemplates(LLLocalTextureObject* ltop,
						LLAvatarAppearanceDefines::ETextureIndex tex_index,
						LLWearable* wearablep);

	LL_INLINE LLAvatarAppearance* getAvatarAppearance() const
	{
		return mAvatarAppearance;
	}

	const std::string getBodyRegionName() const;
	LL_INLINE bool hasComposite() const 				{ return mComposite.notNull(); }

	LL_INLINE LLAvatarAppearanceDefines::EBakedTextureIndex getBakedTexIndex() const
	{
		return mBakedTexIndex;
	}

	LL_INLINE void setBakedTexIndex(LLAvatarAppearanceDefines::EBakedTextureIndex index)
	{
		mBakedTexIndex = index;
	}

	LL_INLINE bool isVisible() const 					{ return mIsVisible; }

public:
	static bool						sHasCaches;
	static bool						sAllowFaceWrinkles;

protected:
	typedef std::vector<LLTexLayerInterface*> layer_list_t;
	layer_list_t					mLayerList;
	layer_list_t					mMaskLayerList;

	LLPointer<LLTexLayerSetBuffer>	mComposite;

	// Note: backlink only; do not make this a LLPointer:
	LLAvatarAppearance* const		mAvatarAppearance;

	LLAvatarAppearanceDefines::EBakedTextureIndex mBakedTexIndex;
	const LLTexLayerSetInfo* 		mInfo;

	bool							mIsVisible;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// LLTexLayerSetInfo
// Contains shared layer set data.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLTexLayerSetInfo final
{
	friend class LLTexLayerSet;

public:
	LLTexLayerSetInfo();
	~LLTexLayerSetInfo();

	bool parseXml(LLXmlTreeNode* node);
	void createVisualParams(LLAvatarAppearance* appearance);

	LL_INLINE S32 getWidth() const						{ return mWidth; }
	LL_INLINE S32 getHeight() const						{ return mHeight; }

protected:
	S32					mWidth;
	S32					mHeight;
	std::string			mBodyRegion;
	std::string			mStaticAlphaFileName;
	 // Set alpha to 1 for this layerset (if there is no mStaticAlphaFileName):
	bool				mClearAlpha;

	typedef std::vector<LLTexLayerInfo*> layer_info_list_t;
	layer_info_list_t	mLayerInfoList;

public:
	static bool			sUseLargeBakes;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// LLTexLayerSetBuffer
// The composite image that a LLTexLayerSet writes to.
// Each LLTexLayerSet has one.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLTexLayerSetBuffer : public virtual LLThreadSafeRefCount
{
protected:
	LOG_CLASS(LLTexLayerSetBuffer);

public:
	LLTexLayerSetBuffer(LLTexLayerSet* const owner);

	LL_INLINE virtual LLViewerTexLayerSetBuffer* asViewerTexLayerSetBuffer()
	{
		return NULL;
	}

protected:
	void pushProjection() const;
	void popProjection() const;
	virtual void preRenderTexLayerSet();
	LL_INLINE virtual void midRenderTexLayerSet(bool)	{}
	virtual void postRenderTexLayerSet(bool success);
	virtual S32 getCompositeOriginX() const = 0;
	virtual S32 getCompositeOriginY() const = 0;
	virtual S32 getCompositeWidth() const = 0;
	virtual S32 getCompositeHeight() const = 0;
	bool renderTexLayerSet();

protected:
	LLTexLayerSet* const mTexLayerSet;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// LLTexLayerStaticImageList
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLTexLayerStaticImageList final
{
protected:
	LOG_CLASS(LLTexLayerStaticImageList);

public:
	LLTexLayerStaticImageList();
	~LLTexLayerStaticImageList();

	LLGLTexture* getTexture(const std::string& file_name, bool is_mask);
	LLImageTGA* getImageTGA(const std::string& file_name);
	void deleteCachedImages();
	void dumpByteCount() const;

protected:
	bool loadImageRaw(const std::string& file_name, LLImageRaw* img_raw);

private:
	LLStringTable 	mImageNames;

	typedef std::map<const char*, LLPointer<LLGLTexture> > texture_map_t;
	texture_map_t 	mStaticImageList;

	typedef std::map<const char*, LLPointer<LLImageTGA> > image_tga_map_t;
	image_tga_map_t mStaticImageListTGA;
	S32 			mGLBytes;
	S32 			mTGABytes;
};

extern LLTexLayerStaticImageList gTexLayerStaticImageList;

#endif  // LL_LLTEXLAYER_H
