/**
 * @file lltextureentry.cpp
 * @brief LLTextureEntry base class
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

#include "linden_common.h"

#include "lltextureentry.h"

#include "imageids.h"
#include "llcolor4.h"
#include "llmediaentry.h"
#include "llsdutil_math.h"

constexpr U8 DEFAULT_BUMP_CODE = 0;  // no bump or shininess

const LLTextureEntry LLTextureEntry::null;

// Some LLSD keys.  Do not change these!
#define OBJECT_ID_KEY_STR "object_id"
#define TEXTURE_INDEX_KEY_STR "texture_index"
#define OBJECT_MEDIA_VERSION_KEY_STR "object_media_version"
#define OBJECT_MEDIA_DATA_KEY_STR "object_media_data"
#define TEXTURE_MEDIA_DATA_KEY_STR "media_data"

// Static member constants
const char* LLTextureEntry::OBJECT_ID_KEY = OBJECT_ID_KEY_STR;
const char* LLTextureEntry::OBJECT_MEDIA_DATA_KEY = OBJECT_MEDIA_DATA_KEY_STR;
const char* LLTextureEntry::MEDIA_VERSION_KEY = OBJECT_MEDIA_VERSION_KEY_STR;
const char* LLTextureEntry::TEXTURE_INDEX_KEY = TEXTURE_INDEX_KEY_STR;
const char* LLTextureEntry::TEXTURE_MEDIA_DATA_KEY = TEXTURE_MEDIA_DATA_KEY_STR;

static const std::string MEDIA_VERSION_STRING_PREFIX = "x-mv:";

//static
LLTextureEntry* LLTextureEntry::newTextureEntry()
{
	return new LLTextureEntry();
}

LLTextureEntry::LLTextureEntry()
{
	init(LLUUID::null, 1.f, 1.f, 0.f, 0.f, 0.f, DEFAULT_BUMP_CODE);
}

LLTextureEntry::LLTextureEntry(const LLUUID& tex_id)
{
	init(tex_id, 1.f, 1.f, 0.f, 0.f, 0.f, DEFAULT_BUMP_CODE);
}

LLTextureEntry::LLTextureEntry(const LLTextureEntry& rhs)
:	mID(rhs.mID),
	mIsDefaultTexture(rhs.mIsDefaultTexture),
	mIsBlankTexture(rhs.mIsBlankTexture),
	mScaleS(rhs.mScaleS),
	mScaleT(rhs.mScaleT),
	mOffsetS(rhs.mOffsetS),
	mOffsetT(rhs.mOffsetT),
	mRotation(rhs.mRotation),
	mColor(rhs.mColor),
	mBump(rhs.mBump),
	mGlow(rhs.mGlow),
	mMaterialID(rhs.mMaterialID),
	mMaterial(rhs.mMaterial),
	mMaterialUpdatePending(rhs.mMaterialUpdatePending),
	mGLTFMaterial(rhs.mGLTFMaterial),
	mMediaFlags(rhs.mMediaFlags),
	mSelected(rhs.mSelected)
{
	if (mGLTFMaterial.notNull())
	{
		mGLTFMaterial->addTextureEntry(this);
	}
	if (rhs.mGLTFMaterialOverrides.notNull())
	{
		mGLTFMaterialOverrides =
			new LLGLTFMaterial(*rhs.mGLTFMaterialOverrides);
	}

	if (rhs.mMediaEntry)
	{
		// Make a copy
		mMediaEntry = new LLMediaEntry(*rhs.mMediaEntry);
	}
	else
	{
		mMediaEntry = NULL;
	}
}

LLTextureEntry& LLTextureEntry::operator=(const LLTextureEntry& rhs)
{
	if (this != &rhs)
	{
		mID = rhs.mID;
		mIsDefaultTexture = rhs.mIsDefaultTexture;
		mIsBlankTexture = rhs.mIsBlankTexture;
		mScaleS = rhs.mScaleS;
		mScaleT = rhs.mScaleT;
		mOffsetS = rhs.mOffsetS;
		mOffsetT = rhs.mOffsetT;
		mRotation = rhs.mRotation;
		mColor = rhs.mColor;
		mBump = rhs.mBump;
		mGlow = rhs.mGlow;
		mMediaFlags = rhs.mMediaFlags;
		mMaterialID = rhs.mMaterialID;
		mMaterial = rhs.mMaterial;
		if (mGLTFMaterial.notNull())
		{
			mGLTFMaterial->removeTextureEntry(this);
		}
		mGLTFMaterial = rhs.mGLTFMaterial;
		if (mGLTFMaterial.notNull())
		{
			mGLTFMaterial->addTextureEntry(this);
		}
		if (rhs.mGLTFMaterialOverrides.notNull())
		{
			mGLTFMaterialOverrides =
				new LLGLTFMaterial(*rhs.mGLTFMaterialOverrides);
		}
		else
		{
			mGLTFMaterialOverrides = NULL;
		}

		mSelected = rhs.mSelected;
		mMaterialUpdatePending = rhs.mMaterialUpdatePending;

		if (mMediaEntry)
		{
			delete mMediaEntry;
		}
		if (rhs.mMediaEntry)
		{
			// Make a copy
			mMediaEntry = new LLMediaEntry(*rhs.mMediaEntry);
		}
		else
		{
			mMediaEntry = NULL;
		}
	}

	return *this;
}

void LLTextureEntry::init(const LLUUID& tex_id, F32 scale_s, F32 scale_t,
						  F32 offset_s, F32 offset_t, F32 rotation, U8 bump)
{
	setID(tex_id);

	mScaleS = scale_s;
	mScaleT = scale_t;
	mOffsetS = offset_s;
	mOffsetT = offset_t;
	mRotation = rotation;
	mBump = bump;
    mGlow = 0;
	mMediaFlags = 0x0;
	mMediaEntry = NULL;
	mMaterialID.clear();
	mMaterialUpdatePending = false;
	mSelected = false;

	mColor.set(1.f, 1.f, 1.f, 1.f);
}

LLTextureEntry::~LLTextureEntry()
{
	if (mMediaEntry)
	{
		delete mMediaEntry;
		mMediaEntry = NULL;
	}
	if (mGLTFMaterial.notNull())
	{
		mGLTFMaterial->removeTextureEntry(this);
		mGLTFMaterial = NULL;
	}
}

bool LLTextureEntry::operator!=(const LLTextureEntry& rhs) const
{
	if (mID != rhs.mID) return true;
	if (mScaleS != rhs.mScaleS) return true;
	if (mScaleT != rhs.mScaleT) return true;
	if (mOffsetS != rhs.mOffsetS) return true;
	if (mOffsetT != rhs.mOffsetT) return true;
	if (mRotation != rhs.mRotation) return true;
	if (mColor != rhs.mColor) return true;
	if (mBump != rhs.mBump) return true;
	if (mMediaFlags != rhs.mMediaFlags) return true;
	if (mGlow != rhs.mGlow) return true;
	if (mMaterialID != rhs.mMaterialID) return true;
	return false;
}

bool LLTextureEntry::operator==(const LLTextureEntry& rhs) const
{
	if (mID != rhs.mID) return false;
	if (mScaleS != rhs.mScaleS) return false;
	if (mScaleT != rhs.mScaleT) return false;
	if (mOffsetS != rhs.mOffsetS) return false;
	if (mOffsetT != rhs.mOffsetT) return false;
	if (mRotation != rhs.mRotation) return false;
	if (mColor != rhs.mColor) return false;
	if (mBump != rhs.mBump) return false;
	if (mMediaFlags != rhs.mMediaFlags) return false;
	if (mGlow != rhs.mGlow) return false;
	if (mMaterialID != rhs.mMaterialID) return false;
	return true;
}

LLSD LLTextureEntry::asLLSD() const
{
	LLSD sd;
	asLLSD(sd);
	return sd;
}

void LLTextureEntry::asLLSD(LLSD& sd) const
{
	sd["imageid"] = mID;
	sd["colors"] = ll_sd_from_color4(mColor);
	sd["scales"] = mScaleS;
	sd["scalet"] = mScaleT;
	sd["offsets"] = mOffsetS;
	sd["offsett"] = mOffsetT;
	sd["imagerot"] = mRotation;
	sd["bump"] = getBumpShiny();
	sd["fullbright"] = getFullbright();
	sd["media_flags"] = mMediaFlags;

	if (hasMedia())
	{
		LLSD mediaData;
        if (getMediaData())
		{
            getMediaData()->asLLSD(mediaData);
        }
		sd[TEXTURE_MEDIA_DATA_KEY] = mediaData;
	}

	sd["glow"] = mGlow;

	if (mGLTFMaterialOverrides.notNull())
	{
		sd["gltf_override"] = mGLTFMaterialOverrides->asJSON();
	}
}

bool LLTextureEntry::fromLLSD(const LLSD& sd)
{
	const char *w, *x;
	w = "imageid";
	if (sd.has(w))
	{
		setID(sd[w]);
	}
	else
	{
		return false;
	}

	w = "colors";
	if (sd.has(w))
	{
		mColor = ll_color4_from_sd(sd["colors"]);
	}
	else
	{
		return false;
	}

	w = "scales";
	x = "scalet";
	if (sd.has(w) && sd.has(x))
	{
		setScale((F32)sd[w].asReal(), (F32)sd[x].asReal());
	}
	else
	{
		return false;
	}

	w = "offsets";
	x = "offsett";
	if (sd.has(w) && sd.has(x))
	{
		setOffset((F32)sd[w].asReal(), (F32)sd[x].asReal());
	}
	else
	{
		return false;
	}

	w = "imagerot";
	if (sd.has(w))
	{
		setRotation((F32)sd[w].asReal());
	}
	else
	{
		return false;
	}

	w = "bump";
	if (sd.has(w))
	{
		setBumpShiny(sd[w].asInteger());
	}
	else
	{
		return false;
	}

	w = "fullbright";
	if (sd.has(w))
	{
		setFullbright(sd[w].asInteger());
	}
	else
	{
		return false;
	}

	w = "media_flags";
	if (sd.has(w))
	{
		setMediaTexGen(sd[w].asInteger());
	}
	else
	{
		return false;
	}

	// If the "has media" flag doesn't match the fact that media data exists,
	// updateMediaData will "fix" it by either clearing or setting the flag.
	w = TEXTURE_MEDIA_DATA_KEY;
	if (hasMedia() != sd.has(w))
	{
		llwarns << "media_flags (" << hasMedia()
				<< ") does not match presence of media_data (" << sd.has(w)
				<< "). Fixing." << llendl;
	}
	updateMediaData(sd[w]);

	w = "glow";
	if (sd.has(w))
	{
		setGlow((F32)sd[w].asReal());
	}
	else
	{
		setGlow(0.f);
	}

	w = "gltf_override";
	if (sd.has(w))
	{
		if (mGLTFMaterialOverrides.isNull())
		{
			mGLTFMaterialOverrides = new LLGLTFMaterial();
		}
		std::string warn_msg, error_msg;
		if (!mGLTFMaterialOverrides->fromJSON(sd[w].asString(), warn_msg,
											  error_msg))
		{
			llwarns << "Failed to parse GLTF json: "
					<< (error_msg.empty() ? warn_msg : error_msg) << llendl;
			mGLTFMaterialOverrides = NULL;
		}
	}

	return true;
}

S32 LLTextureEntry::setID(const LLUUID& tex_id)
{
	if (mID != tex_id)
	{
		mID = tex_id;
		// Cache the "blank" texture and "default texture" status for speed
		// during rendering. HB
		mIsBlankTexture = tex_id == IMG_BLANK;
		mIsDefaultTexture = mIsBlankTexture || tex_id == IMG_PLYWOOD;
		return TEM_CHANGE_TEXTURE;
	}
	return TEM_CHANGE_NONE;
}

S32 LLTextureEntry::setScale(F32 s, F32 t)
{
	S32 retval = 0;
	if (mScaleS != s || mScaleT != t)
	{
		mScaleS = s;
		mScaleT = t;

		retval = TEM_CHANGE_TEXTURE;
	}
	return retval;
}

S32 LLTextureEntry::setScaleS(F32 s)
{
	S32 retval = TEM_CHANGE_NONE;
	if (mScaleS != s)
	{
		mScaleS = s;
		retval = TEM_CHANGE_TEXTURE;
	}
	return retval;
}

S32 LLTextureEntry::setScaleT(F32 t)
{
	S32 retval = TEM_CHANGE_NONE;
	if (mScaleT != t)
	{
		mScaleT = t;
		retval = TEM_CHANGE_TEXTURE;
	}
	return retval;
}

S32 LLTextureEntry::setColor(const LLColor4& color)
{
	if (mColor != color)
	{
		mColor = color;
		return TEM_CHANGE_COLOR;
	}
	return TEM_CHANGE_NONE;
}

S32 LLTextureEntry::setColor(const LLColor3& color)
{
	if (mColor != color)
	{
		// This preserves alpha.
		mColor.set(color);
		return TEM_CHANGE_COLOR;
	}
	return TEM_CHANGE_NONE;
}

S32 LLTextureEntry::setAlpha(F32 alpha)
{
	if (mColor.mV[VW] != alpha)
	{
		mColor.mV[VW] = alpha;
		return TEM_CHANGE_COLOR;
	}
	return TEM_CHANGE_NONE;
}

S32 LLTextureEntry::setOffset(F32 s, F32 t)
{
	S32 retval = 0;

	if (mOffsetS != s || mOffsetT != t)
	{
		mOffsetS = s;
		mOffsetT = t;

		retval = TEM_CHANGE_TEXTURE;
	}
	return retval;
}

S32 LLTextureEntry::setOffsetS(F32 s)
{
	S32 retval = 0;
	if (mOffsetS != s)
	{
		mOffsetS = s;
		retval = TEM_CHANGE_TEXTURE;
	}
	return retval;
}

S32 LLTextureEntry::setOffsetT(F32 t)
{
	S32 retval = 0;
	if (mOffsetT != t)
	{
		mOffsetT = t;
		retval = TEM_CHANGE_TEXTURE;
	}
	return retval;
}

S32 LLTextureEntry::setRotation(F32 theta)
{
	if (mRotation != theta && llfinite(theta))
	{
		mRotation = theta;
		return TEM_CHANGE_TEXTURE;
	}
	return TEM_CHANGE_NONE;
}

S32 LLTextureEntry::setBumpShinyFullbright(U8 bump)
{
	if (mBump != bump)
	{
		mBump = bump;
		return TEM_CHANGE_TEXTURE;
	}
	return TEM_CHANGE_NONE;
}

S32 LLTextureEntry::setMediaTexGen(U8 media)
{
	S32 result = TEM_CHANGE_NONE;
	result |= setTexGen(media & TEM_TEX_GEN_MASK);
	result |= setMediaFlags(media & TEM_MEDIA_MASK);
	return result;
}

S32 LLTextureEntry::setBumpmap(U8 bump)
{
	bump &= TEM_BUMP_MASK;
	if (getBumpmap() != bump)
	{
		mBump &= ~TEM_BUMP_MASK;
		mBump |= bump;
		return TEM_CHANGE_TEXTURE;
	}
	return TEM_CHANGE_NONE;
}

S32 LLTextureEntry::setFullbright(U8 fullbright)
{
	fullbright &= TEM_FULLBRIGHT_MASK;
	if (getFullbright() != fullbright)
	{
		mBump &= ~(TEM_FULLBRIGHT_MASK<<TEM_FULLBRIGHT_SHIFT);
		mBump |= fullbright << TEM_FULLBRIGHT_SHIFT;
		return TEM_CHANGE_TEXTURE;
	}
	return TEM_CHANGE_NONE;
}

S32 LLTextureEntry::setShiny(U8 shiny)
{
	shiny &= TEM_SHINY_MASK;
	if (getShiny() != shiny)
	{
		mBump &= ~(TEM_SHINY_MASK<<TEM_SHINY_SHIFT);
		mBump |= shiny << TEM_SHINY_SHIFT;
		return TEM_CHANGE_TEXTURE;
	}
	return TEM_CHANGE_NONE;
}

S32 LLTextureEntry::setBumpShiny(U8 bump_shiny)
{
	bump_shiny &= TEM_BUMP_SHINY_MASK;
	if (getBumpShiny() != bump_shiny)
	{
		mBump &= ~TEM_BUMP_SHINY_MASK;
		mBump |= bump_shiny;
		return TEM_CHANGE_TEXTURE;
	}
	return TEM_CHANGE_NONE;
}

S32 LLTextureEntry::setMediaFlags(U8 media_flags)
{
	media_flags &= TEM_MEDIA_MASK;
	if (getMediaFlags() != media_flags)
	{
		mMediaFlags &= ~TEM_MEDIA_MASK;
		mMediaFlags |= media_flags;

		// Special code for media handling
		if (hasMedia() && !mMediaEntry)
		{
			mMediaEntry = new LLMediaEntry;
		}
        else if (!hasMedia() && mMediaEntry)
        {
            delete mMediaEntry;
            mMediaEntry = NULL;
        }

		return TEM_CHANGE_MEDIA;
	}
	return TEM_CHANGE_NONE;
}

S32 LLTextureEntry::setTexGen(U8 tex_gen)
{
	tex_gen &= TEM_TEX_GEN_MASK;
	if (getTexGen() != tex_gen)
	{
		mMediaFlags &= ~TEM_TEX_GEN_MASK;
		mMediaFlags |= tex_gen;
		return TEM_CHANGE_TEXTURE;
	}
	return TEM_CHANGE_NONE;
}

S32 LLTextureEntry::setGlow(F32 glow)
{
	if (glow < 0.f)
	{
		glow = 0.f;
	}
	if (mGlow != glow)
	{
		mGlow = glow;
		return TEM_CHANGE_TEXTURE;
	}
	return TEM_CHANGE_NONE;
}

S32 LLTextureEntry::setMaterialID(const LLMaterialID& matidp)
{
	if (mMaterialID != matidp || (mMaterialUpdatePending && !mSelected))
	{
		if (mSelected)
		{
			mMaterialUpdatePending = true;
			mMaterialID = matidp;
			return TEM_CHANGE_TEXTURE;
		}

		mMaterialUpdatePending = false;
		mMaterialID = matidp;
		return TEM_CHANGE_TEXTURE;
	}
	return TEM_CHANGE_NONE;
}

S32 LLTextureEntry::setMaterialParams(const LLMaterialPtr paramsp)
{
	if (mSelected)
	{
		mMaterialUpdatePending = true;
	}
	mMaterial = paramsp;
	return TEM_CHANGE_TEXTURE;
}

void LLTextureEntry::setMediaData(const LLMediaEntry& media_entry)
{
    mMediaFlags |= MF_HAS_MEDIA;
    if (mMediaEntry)
    {
        delete mMediaEntry;
    }
    mMediaEntry = new LLMediaEntry(media_entry);
}

bool LLTextureEntry::updateMediaData(const LLSD& media_data)
{
	if (media_data.isUndefined())
	{
		// Clear the media data
        clearMediaData();
		return false;
	}

	mMediaFlags |= MF_HAS_MEDIA;
	if (!mMediaEntry)
	{
		mMediaEntry = new LLMediaEntry;
	}
	// *NOTE: this will *clobber* all of the fields in mMediaEntry with
	//whatever fields are present (or not present) in media_data !
	mMediaEntry->fromLLSD(media_data);
	return true;
}

void LLTextureEntry::clearMediaData()
{
    mMediaFlags &= ~MF_HAS_MEDIA;
    if (mMediaEntry)
	{
        delete mMediaEntry;
    }
    mMediaEntry = NULL;
}

void LLTextureEntry::mergeIntoMediaData(const LLSD& media_fields)
{
    mMediaFlags |= MF_HAS_MEDIA;
    if (!mMediaEntry)
    {
        mMediaEntry = new LLMediaEntry;
    }
    // *NOTE: this will *merge* the data in media_fields with the data in our
	// media entry
    mMediaEntry->mergeFromLLSD(media_fields);
}

//static
std::string LLTextureEntry::touchMediaVersionString(const std::string& in_ver,
													const LLUUID& agent_id)
{
    // *TODO: make media version string binary (base64-encoded ?)
    // Media "URL" is a representation of a version and the last-touched agent
    // x-mv:nnnnn/agent-id
    // where "nnnnn" is version number
    // *NOTE: not the most efficient code in the world...
    U32 current_version = getVersionFromMediaVersionString(in_ver) + 1;
    const size_t MAX_VERSION_LEN = 10; // 2^32 fits in 10 decimal digits
    char buf[MAX_VERSION_LEN + 1];
	 // Note: added int cast to fix warning/breakage on mac:
    snprintf(buf, (int)MAX_VERSION_LEN + 1, "%0*u", (int)MAX_VERSION_LEN,
			 current_version);
    return MEDIA_VERSION_STRING_PREFIX + buf + "/" + agent_id.asString();
}

//static
U32 LLTextureEntry::getVersionFromMediaVersionString(const std::string& ver)
{
    U32 version = 0;
    if (!ver.empty())
    {
        size_t found = ver.find(MEDIA_VERSION_STRING_PREFIX);
        if (found != std::string::npos)
        {
            found = ver.find_first_of("/", found);
            std::string v = ver.substr(MEDIA_VERSION_STRING_PREFIX.length(),
									   found);
            version = strtoul(v.c_str(), NULL, 10);
        }
    }
    return version;
}

//static
LLUUID LLTextureEntry::getAgentIDFromMediaVersionString(const std::string& ver)
{
    LLUUID id;
    if (!ver.empty())
    {
        size_t found = ver.find(MEDIA_VERSION_STRING_PREFIX);
        if (found != std::string::npos)
        {
            found = ver.find_first_of("/", found);
            if (found != std::string::npos)
            {
                std::string v = ver.substr(found + 1);
                id.set(v);
            }
        }
    }
    return id;
}

//static
bool LLTextureEntry::isMediaVersionString(const std::string& ver)
{
	return std::string::npos != ver.find(MEDIA_VERSION_STRING_PREFIX);
}

// New methods for PBR support

void LLTextureEntry::setGLTFMaterial(LLGLTFMaterial* matp, bool local_origin)
{
	if (matp == mGLTFMaterial.get())
	{
		return;
	}
	if (local_origin && mGLTFMaterialOverrides.get() &&
		!mGLTFMaterialOverrides->isClearedForBaseMaterial())
	{
		// If this warning triggers, try to make sure calling code is using
		// LLViewerObject::setRenderMaterialID.
		llwarns << "isClearedForBaseMaterial() returned false for a local material"
				<< llendl;
		llassert(false);
	}
	if (mGLTFMaterial.notNull())
	{
		// Local materials have to keep track due to update mechanics
		mGLTFMaterial->removeTextureEntry(this);
	}
	mGLTFMaterial = matp;
	if (mGLTFMaterial.notNull())
	{
		mGLTFMaterial->addTextureEntry(this);
	}
	else
	{
		setGLTFRenderMaterial(NULL);
	}
}

S32 LLTextureEntry::setGLTFMaterialOverride(LLGLTFMaterial* matp)
{
	// If override is not NULL, base material must not be NULL
	llassert(!matp || mGLTFMaterial.notNull());
	if (matp == mGLTFMaterialOverrides.get())
	{
		return TEM_CHANGE_NONE;
	}
	mGLTFMaterialOverrides = matp;
	return TEM_CHANGE_TEXTURE;
}

S32 LLTextureEntry::setBaseMaterial()
{
	S32 changed = TEM_CHANGE_NONE;
	if (mGLTFMaterialOverrides.notNull())
	{
		if (mGLTFMaterialOverrides->setBaseMaterial())
		{
			changed = TEM_CHANGE_TEXTURE;
		}
		if (LLGLTFMaterial::sDefault == *(mGLTFMaterialOverrides.get()))
		{
			mGLTFMaterialOverrides = NULL;
			changed = TEM_CHANGE_TEXTURE;
		}
	}
	return changed;
}

LLGLTFMaterial* LLTextureEntry::getGLTFRenderMaterial() const
{
	LLGLTFMaterial* matp = mGLTFRenderMaterial.get();
	if (matp)
	{
		return matp;
	}
	llassert(mGLTFMaterialOverrides.isNull() ||
			 mGLTFMaterialOverrides->isClearedForBaseMaterial());
	return mGLTFMaterial.get();
}

S32 LLTextureEntry::setGLTFRenderMaterial(LLGLTFMaterial* matp)
{
	if (mGLTFRenderMaterial.get() == matp)
	{
		return TEM_CHANGE_NONE;
	}
	mGLTFRenderMaterial = matp;
	return TEM_CHANGE_TEXTURE;
}
