/**
 * @file llwearable.h
 * @brief LLWearable class header file
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

#ifndef LL_LLWEARABLE_H
#define LL_LLWEARABLE_H

#include "llavatarappearancedefines.h"
#include "lllocaltextureobject.h"
#include "llpermissions.h"
#include "llsaleinfo.h"
#include "llwearabletype.h"

class LLAvatarAppearance;
class LLMD5;
class LLTexGlobalColor;
class LLTexGlobalColorInfo;
class LLViewerWearable;
class LLVisualParam;

// Abstract class.
class LLWearable
{
protected:
	LOG_CLASS(LLWearable);

public:
	LLWearable();
	virtual ~LLWearable();

	LL_INLINE virtual LLViewerWearable* asViewerWearable()
	{
		return NULL;
	}

	LL_INLINE virtual const LLViewerWearable* asViewerWearable() const
	{
		return NULL;
	}

	LL_INLINE LLWearableType::EType getType() const		{ return mType; }
	void setType(LLWearableType::EType type, LLAvatarAppearance* avatarp);

	LL_INLINE const std::string& getName() const		{ return mName; }
	LL_INLINE void setName(const std::string& name)		{ mName = name; }

	LL_INLINE const std::string& getDescription() const	{ return mDescription; }
	LL_INLINE void setDescription(const std::string& d)	{ mDescription = d; }

	LL_INLINE const LLPermissions& getPermissions() const
	{
		return mPermissions;
	}

	LL_INLINE void setPermissions(const LLPermissions& p)
	{
		mPermissions = p;
	}

	LL_INLINE const LLSaleInfo& getSaleInfo() const		{ return mSaleInfo; }
	LL_INLINE void setSaleInfo(const LLSaleInfo& info)	{ mSaleInfo = info; }

	const std::string& getTypeLabel() const;
	const std::string& getTypeName() const;
	LLAssetType::EType getAssetType() const;

	LL_INLINE S32 getDefinitionVersion() const			{ return mDefinitionVersion; }
	LL_INLINE void setDefinitionVersion(S32 version)	{ mDefinitionVersion = version; }

	LL_INLINE static S32 getCurrentDefinitionVersion()	{ return sCurrentDefinitionVersion; }

	LL_INLINE static void setCurrentDefinitionVersion(S32 version)
	{
		sCurrentDefinitionVersion = version;
	}

	typedef std::vector<LLVisualParam*> visual_param_vec_t;

	virtual void writeToAvatar(LLAvatarAppearance* avatarp);

	enum EImportResult
	{
		FAILURE = 0,
		SUCCESS,
		BAD_HEADER
	};

	bool exportFile(const std::string& filename) const;
	EImportResult importFile(const std::string& filename,
							 LLAvatarAppearance* avatarp);
	virtual bool exportStream(std::ostream& output_stream) const;
	virtual EImportResult importStream(std::istream& input_stream,
									   LLAvatarAppearance* avatarp);

	virtual LLUUID getDefaultTextureImageID(LLAvatarAppearanceDefines::ETextureIndex index) = 0;

	LLLocalTextureObject* getLocalTextureObject(S32 index);
	const LLLocalTextureObject* getLocalTextureObject(S32 index) const;
	void getLocalTextureListSeq(std::vector<LLLocalTextureObject*>& ltovec);

	LLLocalTextureObject* setLocalTextureObject(S32 index,
												LLLocalTextureObject& lto);
	void addVisualParam(LLVisualParam* param);
	void setVisualParamWeight(S32 index, F32 value,
										 bool upload_bake);
	F32 getVisualParamWeight(S32 index) const;
	LLVisualParam* getVisualParam(S32 index) const;
	void getVisualParams(visual_param_vec_t& list);
	void animateParams(F32 delta, bool upload_bake);

	LLColor4 getClothesColor(S32 te) const;
	void setClothesColor(S32 te, const LLColor4& new_color, bool upload_bake);

	virtual void revertValues();
	virtual void saveValues();

	// Something happened that requires the wearable to be updated (e.g.
	// worn/unworn).
	virtual void setUpdated() const = 0;

	// Update the baked texture hash.
	virtual void addToBakedTextureHash(LLMD5& hash) const = 0;

	typedef std::map<S32, F32> param_map_t;
	typedef std::map<S32, LLVisualParam*> visual_param_index_map_t;

protected:
	typedef std::map<S32, LLLocalTextureObject*> te_map_t;
	void syncImages(te_map_t& src, te_map_t& dst);
	void destroyTextures();
	void createVisualParams(LLAvatarAppearance* avatarp);
	void createLayers(S32 te, LLAvatarAppearance* avatarp);
	bool getNextPopulatedLine(std::istream& input_stream, char* buffer,
							  U32 buffer_size);

protected:
	std::string				mName;
	std::string				mDescription;
	LLPermissions			mPermissions;
	LLSaleInfo				mSaleInfo;
	LLWearableType::EType	mType;

	// Last saved version of visual params
	param_map_t				mSavedVisualParamMap;

	visual_param_index_map_t mVisualParamIndexMap;

	te_map_t				mTEMap;			// maps TE to LocalTextureObject
	te_map_t				mSavedTEMap;	// last saved version of TEMap

	// Depends on the state of the avatar_lad.xml when this asset was created:
	S32						mDefinitionVersion;

	// Depends on the current state of the avatar_lad.xml:
	static S32				sCurrentDefinitionVersion;

public:
	static fast_hset<LLWearable*> sWearableList;
};

#endif  // LL_LLWEARABLE_H
