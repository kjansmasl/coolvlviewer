/**
 * @file lllocaltextureobject.h
 * @brief LLLocalTextureObject class header file
 *
 * $LicenseInfo:firstyear=2009&license=viewergpl$
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

#ifndef LL_LOCALTEXTUREOBJECT_H
#define LL_LOCALTEXTUREOBJECT_H

#include "llpointer.h"
#include "llgltexture.h"
#include "lluuid.h"

class LLTexLayer;
class LLTexLayerTemplate;
class LLWearable;

// Stores all relevant information for a single texture assumed to have
// ownership of all objects referred to - will delete objects when being
// replaced or if object is destroyed.
class LLLocalTextureObject
{
public:
	LLLocalTextureObject();
	LLLocalTextureObject(LLGLTexture* texp, const LLUUID& id);
	LLLocalTextureObject(const LLLocalTextureObject& lto);
	~LLLocalTextureObject();

	LL_INLINE const LLUUID& getID() const		{ return mID; }
	LL_INLINE void setID(const LLUUID& id)		{ mID = id; }

	LL_INLINE LLGLTexture* getImage() const		{ return mImage.get(); }
	void setImage(LLGLTexture* texp);

	LL_INLINE S32 getDiscard() const			{ return mDiscard; }
	LL_INLINE void setDiscard(S32 discard)		{ mDiscard = discard; }

	LL_INLINE bool getBakedReady() const		{ return mIsBakedReady; }
	LL_INLINE void setBakedReady(bool ready)	{ mIsBakedReady = ready; }

	LLTexLayer* getTexLayer(U32 index) const;
	LLTexLayer* getTexLayer(const std::string& name);
	bool setTexLayer(LLTexLayer* layerp, U32 index);
	bool addTexLayer(LLTexLayer* layerp, LLWearable* wearablep);

	bool addTexLayer(LLTexLayerTemplate* layerp, LLWearable* wearablep);
	bool removeTexLayer(U32 index);

	LL_INLINE U32 getNumTexLayers() const		{ return mTexLayers.size(); }

private:
	LLPointer<LLGLTexture>	mImage;
	LLUUID					mID;

	// NOTE: LLLocalTextureObject should be the exclusive owner of mTexEntry
	// and mTexLayer; using shared pointers here only for smart assignment &
	// cleanup. Do NOT create new shared pointers to these objects, or keep
	// pointers to them around
	typedef std::vector<LLTexLayer*> tex_layer_vec_t;
	tex_layer_vec_t			mTexLayers;

	S32						mDiscard;
	bool					mIsBakedReady;

public:
	// *HACK: in OpenSim, we need to make sure textures used for viewer-side
	// baking do not get deleted before the bake happens. It means we cannot
	// remove those textures when not rendered. This flag is set to true when
	// logged on an OpenSim grid for this purpose... HB
	static bool				sMarkNoDelete;
};

#endif // LL_LOCALTEXTUREOBJECT_H
