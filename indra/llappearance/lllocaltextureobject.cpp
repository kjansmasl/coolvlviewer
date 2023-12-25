/**
 * @file lllocaltextureobject.cpp
 * @brief LLLocalTextureObject class implementation
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

#include "linden_common.h"

#include "lllocaltextureobject.h"

#include "llgltexture.h"
#include "llimage.h"
#include "llrender.h"
#include "lltexlayer.h"
#include "llwearable.h"

//static
bool LLLocalTextureObject::sMarkNoDelete = false;


LLLocalTextureObject::LLLocalTextureObject()
:	mIsBakedReady(false),
	mDiscard(MAX_DISCARD_LEVEL + 1),
	mImage(NULL)
{
}

LLLocalTextureObject::LLLocalTextureObject(LLGLTexture* texp, const LLUUID& id)
:	mID(id),
	mIsBakedReady(false),
	mDiscard(MAX_DISCARD_LEVEL + 1),
	mImage(texp)
{
	if (texp)
	{
		// Make sure this texture (which could belong to the agent's outfit)
		// will not get deleted via LLImageGL::activateStaleTextures() before
		// it gets baked. This is only necessary in OpenSim. *TODO: track
		// LLLocalTextureObject instances belonging to the agent avatar, and
		// only mark those as not deletable. HB
		if (sMarkNoDelete)
		{
			texp->setBoostLevel(LLGLTexture::BOOST_AVATAR_SELF);
#if !LL_IMPLICIT_SETNODELETE
			texp->setNoDelete();
#endif
		}
		gGL.getTexUnit(0)->bind(texp);
	}
}

LLLocalTextureObject::LLLocalTextureObject(const LLLocalTextureObject& lto)
:	mImage(lto.mImage),
	mID(lto.mID),
	mIsBakedReady(lto.mIsBakedReady),
	mDiscard(lto.mDiscard)
{
	U32 num_layers = lto.mTexLayers.size();
	mTexLayers.reserve(num_layers);
	for (U32 index = 0; index < num_layers; ++index)
	{
		LLTexLayer* layerp = lto.getTexLayer(index);
		if (!layerp)
		{
			llerrs << "Could not clone Local Texture Object: unable to extract texlayer !"
				   << llendl;
			continue;
		}

		LLTexLayer* new_layerp = new LLTexLayer(*layerp);
		new_layerp->setLTO(this);
		mTexLayers.push_back(new_layerp);
	}
}

LLLocalTextureObject::~LLLocalTextureObject()
{
	for (U32 i = 0, count = mTexLayers.size(); i < count; ++i)
	{
		delete mTexLayers[i];
	}
	mTexLayers.clear();
}

void LLLocalTextureObject::setImage(LLGLTexture* texp)
{
	mImage = texp;
	if (texp && sMarkNoDelete)
	{
		// Make sure this texture (which could belong to the agent's outfit)
		// will not get deleted via LLImageGL::activateStaleTextures() before
		// it gets baked. This is only necessary in OpenSim. *TODO: track
		// LLLocalTextureObject instances belonging to the agent avatar, and
		// only mark those as not deletable. HB
		texp->setBoostLevel(LLGLTexture::BOOST_AVATAR_SELF);
#if !LL_IMPLICIT_SETNODELETE
		texp->setNoDelete();
#endif
	}
}

LLTexLayer* LLLocalTextureObject::getTexLayer(U32 index) const
{
	return index < (U32)mTexLayers.size() ? mTexLayers[index] : NULL;
}

LLTexLayer* LLLocalTextureObject::getTexLayer(const std::string& name)
{
	for (U32 i = 0, count = mTexLayers.size(); i < count; ++i)
	{
		LLTexLayer* layerp = mTexLayers[i];
		if (layerp && layerp->getName().compare(name) == 0)
		{
			return layerp;
		}
	}
	return NULL;
}

bool LLLocalTextureObject::setTexLayer(LLTexLayer* layerp, U32 index)
{
	if (index >= mTexLayers.size())
	{
		return false;
	}

	if (!layerp)
	{
		return removeTexLayer(index);
	}

	LLTexLayer* new_layerp = new LLTexLayer(*layerp);
	new_layerp->setLTO(this);

	if (mTexLayers[index])
	{
		delete mTexLayers[index];
	}
	mTexLayers[index] = new_layerp;

	return true;
}

bool LLLocalTextureObject::addTexLayer(LLTexLayer* layerp,
									   LLWearable* wearablep)
{
	if (!layerp)
	{
		return false;
	}

	LLTexLayer* new_layerp = new LLTexLayer(*layerp, wearablep);
	new_layerp->setLTO(this);
	mTexLayers.push_back(new_layerp);
	return true;
}

bool LLLocalTextureObject::addTexLayer(LLTexLayerTemplate* layerp,
									   LLWearable* wearablep)
{
	if (!layerp)
	{
		return false;
	}

	LLTexLayer* new_layerp = new LLTexLayer(*layerp, this, wearablep);
	new_layerp->setLTO(this);
	mTexLayers.push_back(new_layerp);
	return true;
}

bool LLLocalTextureObject::removeTexLayer(U32 index)
{
	if (index >= mTexLayers.size())
	{
		return false;
	}
	tex_layer_vec_t::iterator iter = mTexLayers.begin();
	iter += index;

	if (*iter)
	{
		delete *iter;
	}
	mTexLayers.erase(iter);
	return true;
}
