/**
 * @file llprimitive.cpp
 * @brief LLPrimitive base class
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

#include "llprimitive.h"

#include "llcolor4u.h"
#include "lldatapacker.h"
#include "llmaterialid.h"
#include "llmaterialtable.h"
#include "llmessage.h"
#include "llprimtexturelist.h"
#include "llsdutil_math.h"
#include "llstring.h"
#include "llvolume.h"
#include "llvolumemgr.h"

// gcc 13 sees array bound issues where tehre are none... HB
#if defined(GCC_VERSION) && GCC_VERSION >= 130000
# pragma GCC diagnostic ignored "-Warray-bounds"
# pragma GCC diagnostic ignored "-Wstringop-overflow"
#endif

// Exported (not so) "constants" (with default value, for SL)
F32 OBJECT_MIN_HOLE_SIZE = 0.05f;
F32 OBJECT_HOLLOW_MAX = 0.95f;

// Old inverted texture: "7595d345-a24c-e7ef-f0bd-78793792133e";
const char* SCULPT_DEFAULT_TEXTURE = "be293869-d0d9-0a69-5989-ad27f1946fd4";

//static
void LLPrimitive::setLimits(bool for_secondlife)
{
	if (for_secondlife)
	{
		OBJECT_HOLLOW_MAX = 0.95f;
		OBJECT_MIN_HOLE_SIZE = 0.05f;
	}
	else
	{
		OBJECT_HOLLOW_MAX = 0.99f;
		OBJECT_MIN_HOLE_SIZE = 0.01f;
	}
}

LLPrimitive::LLPrimitive()
:	mNumTEs(0),
	mMiscFlags(0),
	mNumBumpmapTEs(0),
	mPrimitiveCode(0),
	mMaterial(LL_MCODE_STONE)
{
	mChanged = UNCHANGED;
	mScale.set(1.f, 1.f, 1.f);
	mRotation.loadIdentity();
}

LLPrimitive::~LLPrimitive()
{
	// Cleanup handled by volume manager
	if (mVolumep && gVolumeMgrp)
	{
		gVolumeMgrp->unrefVolume(mVolumep);
	}
	mVolumep = NULL;
}

void LLPrimitive::setPCode(LLPCode p_code)
{
	mPrimitiveCode = p_code;
	setAvatar(p_code == LL_PCODE_LEGACY_AVATAR);
}

LLTextureEntry* LLPrimitive::getTE(U8 index) const
{
	return index != 255 ? mTextureList.getTexture(index) : NULL;
}

//virtual
void LLPrimitive::setNumTEs(U8 num_tes)
{
	mTextureList.setSize(num_tes);
}

//virtual
void  LLPrimitive::setAllTETextures(const LLUUID& tex_id)
{
	mTextureList.setAllIDs(tex_id);
}

//virtual
void LLPrimitive::setTE(U8 index, const LLTextureEntry& te)
{
	if (index != 255 &&
		mTextureList.copyTexture(index, &te) != TEM_CHANGE_NONE &&
		te.getBumpmap() > 0)
	{
		++mNumBumpmapTEs;
	}
}

//virtual
S32 LLPrimitive::setTETexture(U8 index, const LLUUID& id)
{
	return index != 255 ? mTextureList.setID(index, id) : TEM_CHANGE_NONE;
}

//virtual
S32 LLPrimitive::setTEColor(U8 index, const LLColor4& color)
{
	return index != 255 ? mTextureList.setColor(index, color)
						: TEM_CHANGE_NONE;
}

//virtual
S32 LLPrimitive::setTEColor(U8 index, const LLColor3& color)
{
	return index != 255 ? mTextureList.setColor(index, color)
						: TEM_CHANGE_NONE;
}

//virtual
S32 LLPrimitive::setTEAlpha(U8 index, F32 alpha)
{
	return index != 255 ? mTextureList.setAlpha(index, alpha)
						: TEM_CHANGE_NONE;
}

//virtual
S32 LLPrimitive::setTEScale(U8 index, F32 s, F32 t)
{
	return index != 255 ? mTextureList.setScale(index, s, t)
						: TEM_CHANGE_NONE;
}

// Slow: done this way because texture entries have some voodoo related to
// texture coords
//virtual
S32 LLPrimitive::setTEScaleS(U8 index, F32 s)
{
	return index != 255 ? mTextureList.setScaleS(index, s)
						: TEM_CHANGE_NONE;
}

// Slow: done this way because texture entries have some voodoo related to
// texture coords
//virtual
S32 LLPrimitive::setTEScaleT(U8 index, F32 t)
{
	return index != 255 ? mTextureList.setScaleT(index, t)
						: TEM_CHANGE_NONE;
}

//virtual
S32 LLPrimitive::setTEOffset(U8 index, F32 s, F32 t)
{
	return index != 255 ? mTextureList.setOffset(index, s, t)
						: TEM_CHANGE_NONE;
}

// Slow: done this way because texture entries have some voodoo related to
// texture coords
//virtual
S32 LLPrimitive::setTEOffsetS(U8 index, F32 s)
{
	return index != 255 ? mTextureList.setOffsetS(index, s)
						: TEM_CHANGE_NONE;
}

// Slow: done this way because texture entries have some voodoo related to
// texture coords
//virtual
S32 LLPrimitive::setTEOffsetT(U8 index, F32 t)
{
	return index != 255 ? mTextureList.setOffsetT(index, t)
						: TEM_CHANGE_NONE;
}

//virtual
S32 LLPrimitive::setTERotation(U8 index, F32 r)
{
	return index != 255 ? mTextureList.setRotation(index, r)
						: TEM_CHANGE_NONE;
}

LLMaterialPtr LLPrimitive::getTEMaterialParams(U8 index)
{
	return mTextureList.getMaterialParams(index);
}

//virtual
S32 LLPrimitive::setTEBumpShinyFullbright(U8 index, U8 bump)
{
	if (index != 255)
	{
		updateNumBumpmap(index, bump);
		return mTextureList.setBumpShinyFullbright(index, bump);
	}
	return TEM_CHANGE_NONE;
}

S32 LLPrimitive::setTEMediaTexGen(U8 index, U8 media)
{
	return index != 255 ? mTextureList.setMediaTexGen(index, media)
						: TEM_CHANGE_NONE;
}

//virtual
S32 LLPrimitive::setTEBumpmap(U8 index, U8 bump)
{
	if (index != 255)
	{
		updateNumBumpmap(index, bump);
		return mTextureList.setBumpMap(index, bump);
	}
	return TEM_CHANGE_NONE;
}

//virtual
S32 LLPrimitive::setTEBumpShiny(U8 index, U8 bump_shiny)
{
	if (index != 255)
	{
		updateNumBumpmap(index, bump_shiny);
		return mTextureList.setBumpShiny(index, bump_shiny);
	}
	return TEM_CHANGE_NONE;
}

//virtual
S32 LLPrimitive::setTETexGen(U8 index, U8 texgen)
{
	return index != 255 ? mTextureList.setTexGen(index, texgen)
						: TEM_CHANGE_NONE;
}

//virtual
S32 LLPrimitive::setTEShiny(U8 index, U8 shiny)
{
	return index != 255 ? mTextureList.setShiny(index, shiny)
						: TEM_CHANGE_NONE;
}

//virtual
S32 LLPrimitive::setTEFullbright(U8 index, U8 fullbright)
{
	return index != 255 ? mTextureList.setFullbright(index, fullbright)
						: TEM_CHANGE_NONE;
}

//virtual
S32 LLPrimitive::setTEMediaFlags(U8 index, U8 media_flags)
{
	return index != 255 ? mTextureList.setMediaFlags(index, media_flags)
						: TEM_CHANGE_NONE;
}

//virtual
S32 LLPrimitive::setTEGlow(U8 index, F32 glow)
{
	return index != 255 ? mTextureList.setGlow(index, glow)
						: TEM_CHANGE_NONE;
}

//virtual
S32 LLPrimitive::setTEMaterialID(U8 index, const LLMaterialID& matidp)
{
	return index != 255 ? mTextureList.setMaterialID(index, matidp)
						: TEM_CHANGE_NONE;
}

//virtual
S32 LLPrimitive::setTEMaterialParams(U8 index, const LLMaterialPtr paramsp)
{
	return index != 255 ? mTextureList.setMaterialParams(index, paramsp)
						: TEM_CHANGE_NONE;
}

//virtual
void LLPrimitive::setAllTESelected(bool sel)
{
	for (S32 i = 0, count = getNumTEs(); i < count; ++i)
	{
		setTESelected(i, sel);
	}
}

void LLPrimitive::setTESelected(U8 te, bool sel)
{
	LLTextureEntry* tep = getTE(te);
	bool was_selected = tep && tep->setSelected(sel);
	if (was_selected && !sel && tep->hasPendingMaterialUpdate())
	{
		LLMaterialID material_id = tep->getMaterialID();
		setTEMaterialID(te, material_id);
	}
}

// Do not crash or llerrs here !  This function is used for debug strings.
//static
std::string LLPrimitive::pCodeToString(LLPCode pcode)
{
	std::string pcode_string;

	U8 base_code = pcode & LL_PCODE_BASE_MASK;
	if (!pcode)
	{
		pcode_string = "null";
	}
	else if (base_code == LL_PCODE_LEGACY)
	{
		// It is a legacy object
		switch (pcode)
		{
		case LL_PCODE_LEGACY_GRASS:
			pcode_string = "grass";
			break;

		case LL_PCODE_LEGACY_PART_SYS:
			pcode_string = "particle system";
			break;

		case LL_PCODE_LEGACY_AVATAR:
			pcode_string = "avatar";
			break;

		case LL_PCODE_LEGACY_TREE:
			pcode_string = "tree";
			break;

		default:
			pcode_string = llformat("unknown legacy pcode %i", (U32)pcode);
		}
	}
	else
	{
		std::string shape;
		std::string mask;
		if (base_code == LL_PCODE_CUBE)
		{
			shape = "cube";
		}
		else if (base_code == LL_PCODE_CYLINDER)
		{
			shape = "cylinder";
		}
		else if (base_code == LL_PCODE_CONE)
		{
			shape = "cone";
		}
		else if (base_code == LL_PCODE_PRISM)
		{
			shape = "prism";
		}
		else if (base_code == LL_PCODE_PYRAMID)
		{
			shape = "pyramid";
		}
		else if (base_code == LL_PCODE_SPHERE)
		{
			shape = "sphere";
		}
		else if (base_code == LL_PCODE_TETRAHEDRON)
		{
			shape = "tetrahedron";
		}
		else if (base_code == LL_PCODE_VOLUME)
		{
			shape = "volume";
		}
		else if (base_code == LL_PCODE_APP)
		{
			shape = "app";
		}
		else
		{
			llwarns << "Unknown base mask for pcode: " << base_code << llendl;
		}

		U8 mask_code = pcode & (~LL_PCODE_BASE_MASK);
		if (base_code == LL_PCODE_APP)
		{
			mask = llformat("%x", mask_code);
		}
		else if (mask_code & LL_PCODE_HEMI_MASK)
		{
			mask = "hemi";
		}
		else
		{
			mask = llformat("%x", mask_code);
		}

		if (mask[0])
		{
			pcode_string = llformat("%s-%s", shape.c_str(), mask.c_str());
		}
		else
		{
			pcode_string = llformat("%s", shape.c_str());
		}
	}

	return pcode_string;
}

void LLPrimitive::copyTEs(const LLPrimitive* primitivep)
{
	if (primitivep->getExpectedNumTEs() != getExpectedNumTEs())
	{
		llwarns << "Primitives do not have same expected number of TE's"
				<< llendl;
	}
	U32 num_tes = llmin(primitivep->getExpectedNumTEs(), getExpectedNumTEs());
	if (mTextureList.size() < getExpectedNumTEs())
	{
		mTextureList.setSize(getExpectedNumTEs());
	}
	for (U32 i = 0; i < num_tes; ++i)
	{
		mTextureList.copyTexture(i, primitivep->getTE(i));
	}
}

S32 face_index_from_id(LLFaceID face_ID,
					   const std::vector<LLProfile::Face>& faceArray)
{
	for (U32 i = 0, count = faceArray.size(); i < count; ++i)
	{
		if (faceArray[i].mFaceID == face_ID)
		{
			return i;
		}
	}
	return -1;
}

//virtual
bool LLPrimitive::setVolume(const LLVolumeParams& volume_params,
							S32 detail, bool unique_volume)
{
	if (detail < 0 || detail >= LLVolumeLODGroup::NUM_LODS)
	{
		llwarns << "Attempt to set volume with out of range LOD: " << detail
				<< llendl;
		return false;
	}

	if (!gVolumeMgrp)
	{
		llwarns << "Attempt to set a volume while the volume manager is not initialized !"
				<< llendl;
		return false;
	}

	LLVolume* volumep;
	if (unique_volume)
	{
		F32 volume_detail = LLVolumeLODGroup::getVolumeScaleFromDetail(detail);
		if (mVolumep.notNull() && volume_params == mVolumep->getParams() &&
			volume_detail == mVolumep->getDetail())
		{
			return false;
		}
		volumep = new LLVolume(volume_params, volume_detail, false, true);
	}
	else
	{
		if (mVolumep.notNull())
		{
			F32 volume_detail =
				LLVolumeLODGroup::getVolumeScaleFromDetail(detail);
			if (volume_params == mVolumep->getParams() &&
				volume_detail == mVolumep->getDetail())
			{
				return false;
			}
		}

		volumep = gVolumeMgrp->refVolume(volume_params, detail);
		if (volumep == mVolumep)
		{
			// LLVolumeMgr::refVolume() creates a reference, but we do not need
			// a second one.
			gVolumeMgrp->unrefVolume(volumep);
			return true;
		}
	}

	setChanged(GEOMETRY);

	if (!mVolumep)
	{
		mVolumep = volumep;
		setNumTEs(mVolumep->getNumFaces());
		return true;
	}

	// Build the new object
	gVolumeMgrp->unrefVolume(mVolumep);
	mVolumep = volumep;

	setNumTEs(mVolumep->getNumFaces());

	return true;
}

//virtual
bool LLPrimitive::setMaterial(U8 material)
{
	if (material != mMaterial)
	{
		mMaterial = material;
		return true;
	}
	return false;
}

S32 LLPrimitive::packTEField(U8* cur_ptr, U8* data_ptr, U8 data_size,
							 U8 last_face_index, EMsgVariableType type) const
{
	U8* start_loc = cur_ptr;
	htonmemcpy(cur_ptr, data_ptr + last_face_index * data_size, type,
			   data_size);
	cur_ptr += data_size;

	U64 exception_faces;
	for (S32 face_index = last_face_index - 1; face_index >= 0; --face_index)
	{
		bool already_sent = false;
		for (S32 i = face_index+1; i <= last_face_index; ++i)
		{
			if (!memcmp(data_ptr + data_size * face_index,
						data_ptr + data_size * i, data_size))
			{
				already_sent = true;
				break;
			}
		}

		if (!already_sent)
		{
			exception_faces = 0;
			for (S32 i = face_index; i >= 0; --i)
			{
				if (!memcmp(data_ptr+ data_size * face_index,
							data_ptr + data_size * i, data_size))
				{
					exception_faces |= ((U64)1 << i);
				}
			}

			// Assign exception faces to cur_ptr
			if (exception_faces >= ((U64)1 << 7))
			{
				if (exception_faces >= ((U64)1 << 14))
				{
					if (exception_faces >= ((U64)1 << 21))
					{
						if (exception_faces >= ((U64)1 << 28))
						{
							if (exception_faces >= ((U64)1 << 35))
							{
								if (exception_faces >= ((U64)1 << 42))
								{
									if (exception_faces >= ((U64)1 << 49))
									{
										*cur_ptr++ = (U8)(((exception_faces >> 49) & 0x7F) | 0x80);
									}
									*cur_ptr++ = (U8)(((exception_faces >> 42) & 0x7F) | 0x80);
								}
								*cur_ptr++ = (U8)(((exception_faces >> 35) & 0x7F) | 0x80);
							}
							*cur_ptr++ = (U8)(((exception_faces >> 28) & 0x7F) | 0x80);
						}
						*cur_ptr++ = (U8)(((exception_faces >> 21) & 0x7F) | 0x80);
					}
					*cur_ptr++ = (U8)(((exception_faces >> 14) & 0x7F) | 0x80);
				}
				*cur_ptr++ = (U8)(((exception_faces >> 7) & 0x7F) | 0x80);
			}

			*cur_ptr++ = (U8)(exception_faces & 0x7F);

			htonmemcpy(cur_ptr, data_ptr + face_index * data_size, type,
					   data_size);
			cur_ptr += data_size;
   		}
	}
	return (S32)(cur_ptr - start_loc);
}

// Pack information about all texture entries into container:
// { TextureEntry Variable 2 }
// Includes information about image ID, color, scale S,T, offset S,T and
// rotation
void LLPrimitive::packTEMessage(LLMessageSystem* mesgsys) const
{
	U8 image_ids[MAX_TES * UUID_BYTES];
	U8 colors[MAX_TES * 4];
	F32 scale_s[MAX_TES];
	F32 scale_t[MAX_TES];
	S16 offset_s[MAX_TES];
	S16 offset_t[MAX_TES];
	S16 image_rot[MAX_TES];
	U8 bump[MAX_TES];
	U8 media_flags[MAX_TES];
    U8 glow[MAX_TES];
	U8 material_data[MAX_TES * UUID_BYTES];
	U8 packed_buffer[MAX_TE_BUFFER];
	U8* cur_ptr = packed_buffer;

	S32 last_face_index = llmin((U32)getNumTEs(), MAX_TES) - 1;

	if (last_face_index > -1)
	{
		// ...if we hit the front, send one image Id
		S8 face_index;
		LLColor4U coloru;
		for (face_index = 0; face_index <= last_face_index; ++face_index)
		{
			// Directly sending image_ids is not safe !
			memcpy(&image_ids[face_index * UUID_BYTES],
				   getTE(face_index)->getID().mData, UUID_BYTES);

			// Cast LLColor4 to LLColor4U
			coloru.set(getTE(face_index)->getColor());

			// Note: this is an optimization to send common colors (white) as
			// all zeros. However, the subtraction and addition must be done in
			// unsigned byte space, not in float space, otherwise off-by-one
			// errors occur. JC
			colors[4 * face_index]     = 255 - coloru.mV[0];
			colors[4 * face_index + 1] = 255 - coloru.mV[1];
			colors[4 * face_index + 2] = 255 - coloru.mV[2];
			colors[4 * face_index + 3] = 255 - coloru.mV[3];

			const LLTextureEntry* te = getTE(face_index);
			scale_s[face_index] = (F32)te->getScaleS();
			scale_t[face_index] = (F32)te->getScaleT();
			offset_s[face_index] = (S16)ll_round(llclamp(te->getOffsetS(),
														  -1.f, 1.f) *
												 (F32)0x7FFF);
			offset_t[face_index] = (S16)ll_round(llclamp(te->getOffsetT(),
														 -1.f, 1.f) *
												 (F32)0x7FFF);
			image_rot[face_index] = (S16)ll_round((fmodf(te->getRotation(),
														 F_TWO_PI) /
												   F_TWO_PI) *
												  TEXTURE_ROTATION_PACK_FACTOR);
			bump[face_index] = te->getBumpShinyFullbright();
			media_flags[face_index] = te->getMediaTexGen();
			glow[face_index] = (U8)ll_round(llclamp(te->getGlow(), 0.f, 1.f) *
											(F32)0xFF);
			// Directly sending material_ids is not safe !
			memcpy(&material_data[face_index * UUID_BYTES],
				   getTE(face_index)->getMaterialID().get(), UUID_BYTES);
		}

		cur_ptr += packTEField(cur_ptr, (U8*)image_ids, sizeof(LLUUID),
							   last_face_index, MVT_LLUUID);
		*cur_ptr++ = 0;
		cur_ptr += packTEField(cur_ptr, (U8*)colors, 4, last_face_index,
							   MVT_U8);
		*cur_ptr++ = 0;
		cur_ptr += packTEField(cur_ptr, (U8*)scale_s, 4, last_face_index,
							   MVT_F32);
		*cur_ptr++ = 0;
		cur_ptr += packTEField(cur_ptr, (U8*)scale_t, 4, last_face_index,
							   MVT_F32);
		*cur_ptr++ = 0;
		cur_ptr += packTEField(cur_ptr, (U8*)offset_s, 2, last_face_index,
							   MVT_S16Array);
		*cur_ptr++ = 0;
		cur_ptr += packTEField(cur_ptr, (U8*)offset_t, 2, last_face_index,
							   MVT_S16Array);
		*cur_ptr++ = 0;
		cur_ptr += packTEField(cur_ptr, (U8*)image_rot, 2, last_face_index,
							   MVT_S16Array);
		*cur_ptr++ = 0;
		cur_ptr += packTEField(cur_ptr, (U8*)bump, 1, last_face_index, MVT_U8);
		*cur_ptr++ = 0;
		cur_ptr += packTEField(cur_ptr, (U8*)media_flags, 1, last_face_index,
							   MVT_U8);
		*cur_ptr++ = 0;
		cur_ptr += packTEField(cur_ptr, (U8*)glow, 1, last_face_index, MVT_U8);
		*cur_ptr++ = 0;
		cur_ptr += packTEField(cur_ptr, (U8*)material_data, UUID_BYTES,
							   last_face_index, MVT_LLUUID);
	}
   	mesgsys->addBinaryDataFast(_PREHASH_TextureEntry, packed_buffer,
							   (S32)(cur_ptr - packed_buffer));
}

void LLPrimitive::packTEMessage(LLDataPacker& dp) const
{
	U8 image_ids[MAX_TES * UUID_BYTES];
	U8 colors[MAX_TES * 4];
	F32 scale_s[MAX_TES];
	F32 scale_t[MAX_TES];
	S16 offset_s[MAX_TES];
	S16 offset_t[MAX_TES];
	S16 image_rot[MAX_TES];
	U8 bump[MAX_TES];
	U8 media_flags[MAX_TES];
    U8 glow[MAX_TES];
	U8 material_data[MAX_TES * UUID_BYTES];
	U8 packed_buffer[MAX_TE_BUFFER];
	U8* cur_ptr = packed_buffer;

	S32 last_face_index = llmin((U32)getNumTEs(), MAX_TES) - 1;

	if (last_face_index > -1)
	{
		// ...if we hit the front, send one image id
		S8 face_index;
		LLColor4U coloru;
		for (face_index = 0; face_index <= last_face_index; ++face_index)
		{
			// Directly sending image_ids is not safe!
			memcpy(&image_ids[face_index * UUID_BYTES],
				   getTE(face_index)->getID().mData, UUID_BYTES);

			// Cast LLColor4 to LLColor4U
			coloru.set(getTE(face_index)->getColor());

			// Note: this is an optimization to send common colors (white) as
			// all zeros. However, the subtraction and addition must be done in
			// unsigned byte space, not in float space, otherwise off-by-one
			// errors occur. JC
			colors[4 * face_index]     = 255 - coloru.mV[0];
			colors[4 * face_index + 1] = 255 - coloru.mV[1];
			colors[4 * face_index + 2] = 255 - coloru.mV[2];
			colors[4 * face_index + 3] = 255 - coloru.mV[3];

			const LLTextureEntry* te = getTE(face_index);
			scale_s[face_index] = (F32)te->getScaleS();
			scale_t[face_index] = (F32)te->getScaleT();
			offset_s[face_index] = (S16)ll_round(llclamp(te->getOffsetS(),
														   -1.f, 1.f) *
												 (F32)0x7FFF);
			offset_t[face_index] = (S16)ll_round(llclamp(te->getOffsetT(),
														 -1.f, 1.f) *
												 (F32)0x7FFF);
			image_rot[face_index] = (S16)ll_round((fmodf(te->getRotation(),
														 F_TWO_PI) /
												   F_TWO_PI) *
												  TEXTURE_ROTATION_PACK_FACTOR);
			bump[face_index] = te->getBumpShinyFullbright();
			media_flags[face_index] = te->getMediaTexGen();
            glow[face_index] = (U8)ll_round(llclamp(te->getGlow(), 0.f, 1.f) *
										   (F32)0xFF);
			// Directly sending material_ids is not safe!
			memcpy(&material_data[face_index * UUID_BYTES],
				   getTE(face_index)->getMaterialID().get(), UUID_BYTES);
		}

		cur_ptr += packTEField(cur_ptr, (U8*)image_ids, sizeof(LLUUID),
							   last_face_index, MVT_LLUUID);
		*cur_ptr++ = 0;
		cur_ptr += packTEField(cur_ptr, (U8*)colors, 4, last_face_index,
							   MVT_U8);
		*cur_ptr++ = 0;
		cur_ptr += packTEField(cur_ptr, (U8*)scale_s, 4, last_face_index,
							   MVT_F32);
		*cur_ptr++ = 0;
		cur_ptr += packTEField(cur_ptr, (U8*)scale_t, 4, last_face_index,
							   MVT_F32);
		*cur_ptr++ = 0;
		cur_ptr += packTEField(cur_ptr, (U8*)offset_s, 2, last_face_index,
							   MVT_S16Array);
		*cur_ptr++ = 0;
		cur_ptr += packTEField(cur_ptr, (U8*)offset_t, 2, last_face_index,
							   MVT_S16Array);
		*cur_ptr++ = 0;
		cur_ptr += packTEField(cur_ptr, (U8*)image_rot, 2, last_face_index,
							   MVT_S16Array);
		*cur_ptr++ = 0;
		cur_ptr += packTEField(cur_ptr, (U8*)bump, 1, last_face_index, MVT_U8);
		*cur_ptr++ = 0;
		cur_ptr += packTEField(cur_ptr, (U8*)media_flags, 1, last_face_index,
							   MVT_U8);
		*cur_ptr++ = 0;
		cur_ptr += packTEField(cur_ptr, (U8*)glow, 1, last_face_index, MVT_U8);
		*cur_ptr++ = 0;
		cur_ptr += packTEField(cur_ptr, (U8*)material_data, UUID_BYTES,
							   last_face_index, MVT_LLUUID);
	}

	dp.packBinaryData(packed_buffer, (S32)(cur_ptr - packed_buffer),
					  "TextureEntry");
}

namespace LLTEField
{
	template<typename T>
	bool unpack(T dest[], U8 dest_count, U8*& source, U8* source_end,
				EMsgVariableType type)
	{
		constexpr size_t size = sizeof(T);

		// We add 1 to take into account the byte that we know must follow the
		// value.
		if (source + size + 1 > source_end)
		{
			llwarns << "Buffer exhausted: " << size + 1
					<< " bytes needed and only " << source_end - source
					<< " bytes remaining." << llendl;
			source = source_end;
			return false;
		}

		// Extract the default value and fill up the array with it.
		htonmemcpy(dest, source, type, size);
		source += size;
		for (S32 i = 1; i < dest_count; ++i)
		{
			dest[i] = dest[0];
		}

		while (source < source_end)
		{
			// Unpack the variable length bitfield. Each bit represents
			// whether the following value will be placed at the corresponding
			// array index.
			U64 index_flags = 0;
			U8 sbit = 0;
			do
			{
				if (source >= source_end)
				{
					llwarns << "Buffer exhausted while reading index flags."
							<< llendl;
					source = source_end;
					return false;
				}
				sbit = *source++;
				index_flags <<= 7;
				index_flags |= sbit & 0X7F;
			}
			while (sbit & 0x80);

			if (!index_flags)
			{
				// We have hit the terminating 0 byte.
				break;
			}

			if (source + size + 1 > source_end)
			{
				llwarns << "Buffer exhausted: " << size + 1
						<< " bytes needed and only " << source_end - source
						<< " bytes remaining." << llendl;
				source = source_end;
				return false;
			}

			// Get the value for indexes
			T value;
			htonmemcpy(&value, source, type, size);
			source += size;
			U64 mask = 1;
			for (S32 i = 0; i < dest_count; ++i)
			{
				if (index_flags & mask)
				{
					dest[i] = value;
				}
				mask <<= 1;
			}
		}

		return true;
	}
}

S32 LLPrimitive::parseTEMessage(LLMessageSystem* mesgsys,
								char const* block_name, S32 block_num,
								LLTEContents& tec)
{
	if (block_num < 0)
	{
		tec.size = mesgsys->getSizeFast(block_name, _PREHASH_TextureEntry);
	}
	else
	{
		tec.size = mesgsys->getSizeFast(block_name, block_num,
										_PREHASH_TextureEntry);
	}

	if (tec.size == 0)
	{
		tec.face_count = 0;
		return 0;
	}

	if (tec.size >= MAX_TE_BUFFER)
	{
		llwarns << "Excessive buffer size detected in texture entry; truncating."
				<< llendl;
		tec.size = MAX_TE_BUFFER - 1;
	}

	S32 block = 0;
	if (block_num > 0)
	{
		block = block_num;
	}
	mesgsys->getBinaryDataFast(block_name, _PREHASH_TextureEntry,
							   tec.packed_buffer, 0, block, MAX_TE_BUFFER - 1);

	// The last field is not zero-terminated. Rather than a special case for
	// unpack functions, just add the missing null byte.
	tec.packed_buffer[tec.size++] = 0x00;

	tec.face_count = llmin((U32)getNumTEs(), MAX_TES);

	U8* cur_ptr = tec.packed_buffer;
	U8* buffer_end = tec.packed_buffer + tec.size;
	const U8 dest_count = tec.face_count;
	bool success =
		LLTEField::unpack<LLUUID>(tec.image_data, dest_count, cur_ptr,
								  buffer_end, MVT_LLUUID) &&
		LLTEField::unpack<LLColor4U>(tec.colors, dest_count, cur_ptr,
									 buffer_end, MVT_U8) &&
		LLTEField::unpack<F32>(tec.scale_s, dest_count, cur_ptr, buffer_end,
							   MVT_F32) &&
		LLTEField::unpack<F32>(tec.scale_t, dest_count, cur_ptr, buffer_end,
							   MVT_F32) &&
		LLTEField::unpack<S16>(tec.offset_s, dest_count, cur_ptr, buffer_end,
							   MVT_S16) &&
		LLTEField::unpack<S16>(tec.offset_t, dest_count, cur_ptr, buffer_end,
							   MVT_S16) &&
		LLTEField::unpack<S16>(tec.image_rot, dest_count, cur_ptr, buffer_end,
							   MVT_S16) &&
		LLTEField::unpack<U8>(tec.bump, dest_count, cur_ptr, buffer_end,
							  MVT_U8) &&
		LLTEField::unpack<U8>(tec.media_flags, dest_count, cur_ptr, buffer_end,
							  MVT_U8) &&
		LLTEField::unpack<U8>(tec.glow, dest_count, cur_ptr, buffer_end,
							  MVT_U8);
	if (!success)
	{
		llwarns << "Failure parsing texture entry message due to malformed TE field."
				<< llendl;
		return 0;
	}

	// Buffer for material ID processing. Static to avoid memory reallocations.
	static LLUUID material_data[MAX_TES];
	if (cur_ptr >= buffer_end ||
		!LLTEField::unpack<LLUUID>(material_data, dest_count, cur_ptr,
								   buffer_end, MVT_LLUUID))
	{
		memset((void*)material_data, 0, sizeof(material_data));
	}
	for (U32 i = 0; i < dest_count; ++i)
	{
		tec.material_ids[i].set(&material_data[i]);
	}

	return 1;
}

S32 LLPrimitive::applyParsedTEMessage(LLTEContents& tec)
{
	S32 retval = 0;

	LLColor4 color;
	for (U32 i = 0; i < tec.face_count; ++i)
	{
		LLUUID& req_id = ((LLUUID*)tec.image_data)[i];
		retval |= setTETexture(i, req_id);
		retval |= setTEScale(i, tec.scale_s[i], tec.scale_t[i]);
		retval |= setTEOffset(i, (F32)tec.offset_s[i] / (F32)0x7FFF,
							  (F32)tec.offset_t[i] / (F32)0x7FFF);
		retval |= setTERotation(i, ((F32)tec.image_rot[i] /
									TEXTURE_ROTATION_PACK_FACTOR) * F_TWO_PI);
		retval |= setTEBumpShinyFullbright(i, tec.bump[i]);
		retval |= setTEMediaTexGen(i, tec.media_flags[i]);
		retval |= setTEGlow(i, (F32)tec.glow[i] / (F32)0xFF);
		retval |= setTEMaterialID(i, tec.material_ids[i]);

		// Note: this is an optimization to send common colors (white) as all
		// zeros. However, the subtraction and addition must be done in
		// unsigned byte space, not in float space, otherwise off-by-one errors
		// occur. JC
		const LLColor4U& coloru = tec.colors[i];
		color.mV[VRED] = F32(255 - coloru.mV[VRED]) / 255.f;
		color.mV[VGREEN] = F32(255 - coloru.mV[VGREEN]) / 255.f;
		color.mV[VBLUE] = F32(255 - coloru.mV[VBLUE]) / 255.f;
		color.mV[VALPHA] = F32(255 - coloru.mV[VALPHA]) / 255.f;

		retval |= setTEColor(i, color);
	}

	return retval;
}

S32 LLPrimitive::unpackTEMessage(LLMessageSystem* mesgsys,
								 char const* block_name, S32 block_num)
{
	LLTEContents tec;
	S32 retval = parseTEMessage(mesgsys, block_name, block_num, tec);
	if (!retval)
	{
		return retval;
	}
	return applyParsedTEMessage(tec);
}

S32 LLPrimitive::unpackTEMessage(LLDataPacker& dp)
{
	// Avoid construction of 90 UUIDs + 45 LLColor4U + 90 F32 + 135 S16 +
	// 135 U8 + a 4096 bytes buffer per call...
	static LLTEContents data;
	memset((void*)&data, 0, sizeof(data));

	S32 size;
	if (!dp.unpackBinaryData(data.packed_buffer, size, "TextureEntry"))
	{
		llwarns << "Bad texture entry block !  Aborted !" << llendl;
		return TEM_INVALID;
	}
	if (size == 0)
	{
		return 0;
	}
	if ((U32)size > MAX_TE_BUFFER)
	{
		llwarns << "Excessive buffer size detected in texture entry; truncating."
				<< llendl;
		size = MAX_TE_BUFFER - 1;
	}
	// The last field is not zero-terminated. Rather than a special case for
	// unpack functions, just add the missing null byte.
	data.packed_buffer[size++] = 0x00;

	U32 face_count = llmin((U32)getNumTEs(), MAX_TES);

	U8* cur_ptr = data.packed_buffer;
	U8* buffer_end = cur_ptr + size;

	bool success =
		LLTEField::unpack<LLUUID>(data.image_data, face_count, cur_ptr,
								  buffer_end, MVT_LLUUID) &&
		LLTEField::unpack<LLColor4U>(data.colors, face_count, cur_ptr,
									 buffer_end, MVT_U8) &&
		LLTEField::unpack<F32>(data.scale_s, face_count, cur_ptr, buffer_end,
							   MVT_F32) &&
		LLTEField::unpack<F32>(data.scale_t, face_count, cur_ptr, buffer_end,
							   MVT_F32) &&
		LLTEField::unpack<S16>(data.offset_s, face_count, cur_ptr, buffer_end,
							   MVT_S16) &&
		LLTEField::unpack<S16>(data.offset_t, face_count, cur_ptr, buffer_end,
							   MVT_S16) &&
		LLTEField::unpack<S16>(data.image_rot, face_count, cur_ptr, buffer_end,
							   MVT_S16) &&
		LLTEField::unpack<U8>(data.bump, face_count, cur_ptr, buffer_end,
							  MVT_U8) &&
		LLTEField::unpack<U8>(data.media_flags, face_count, cur_ptr, buffer_end,
							  MVT_U8) &&
		LLTEField::unpack<U8>(data.glow, face_count, cur_ptr, buffer_end,
							  MVT_U8);
	if (!success)
	{
		llwarns << "Failure parsing texture entry message due to malformed TE field."
				<< llendl;
		return 0;
	}

	// Buffer for material ID processing. Static to avoid memory reallocations.
	static LLUUID material_data[MAX_TES];
	if (cur_ptr >= buffer_end ||
		!LLTEField::unpack<LLUUID>(material_data, face_count, cur_ptr,
								   buffer_end, MVT_LLUUID))
	{
		memset((void*)material_data, 0, sizeof(material_data));
	}
	for (U32 i = 0; i < face_count; ++i)
	{
		data.material_ids[i].set(&material_data[i]);
	}

	S32 retval = 0;
	LLColor4 color;
	for (U32 i = 0; i < face_count; ++i)
	{
		retval |= setTETexture(i, data.image_data[i]);
		retval |= setTEScale(i, data.scale_s[i], data.scale_t[i]);
		retval |= setTEOffset(i, (F32)data.offset_s[i] / (F32)0x7FFF,
							  (F32)data.offset_t[i] / (F32)0x7FFF);
		retval |= setTERotation(i,
								((F32)data.image_rot[i] /
								 TEXTURE_ROTATION_PACK_FACTOR) * F_TWO_PI);
		retval |= setTEBumpShinyFullbright(i, data.bump[i]);
		retval |= setTEMediaTexGen(i, data.media_flags[i]);
		retval |= setTEGlow(i, (F32)data.glow[i] / (F32)0xFF);
		retval |= setTEMaterialID(i, data.material_ids[i]);

		// Note: this is an optimization to send common colors (white) as all
		// zeros. However, the subtraction and addition must be done in
		// unsigned byte space, not in float space, otherwise off-by-one
		// errors occur. JC
		const LLColor4U& coloru = data.colors[i];
		color.mV[VRED] = F32(255 - coloru.mV[VRED])   / 255.f;
		color.mV[VGREEN] = F32(255 - coloru.mV[VGREEN]) / 255.f;
		color.mV[VBLUE] = F32(255 - coloru.mV[VBLUE])  / 255.f;
		color.mV[VALPHA] = F32(255 - coloru.mV[VALPHA]) / 255.f;

		retval |= setTEColor(i, color);
	}
	return retval;
}

U8 LLPrimitive::getExpectedNumTEs() const
{
	U8 expected_face_count = 0;
	if (mVolumep)
	{
		expected_face_count = mVolumep->getNumFaces();
	}
	return expected_face_count;
}

void LLPrimitive::copyTextureList(const LLPrimTextureList& other_list)
{
	mTextureList.copy(other_list);
}

void LLPrimitive::takeTextureList(LLPrimTextureList& other_list)
{
	mTextureList.take(other_list);
}

void LLPrimitive::updateNumBumpmap(U8 index, U8 bump)
{
	LLTextureEntry* te = getTE(index);
	if (!te)
	{
		return;
	}

	U8 old_bump = te->getBumpmap();
	if (old_bump > 0)
	{
		--mNumBumpmapTEs;
	}
	if ((bump & TEM_BUMP_MASK) > 0)
	{
		++mNumBumpmapTEs;
	}
}

//============================================================================

// Moved from llselectmgr.cpp
// Limitation: only works for boxes.
// Face numbering for flex boxes as of 1.14.2

//static
bool LLPrimitive::getTESTAxes(U8 face, U32* s_axis, U32* t_axis)
{
	if (face == 0)
	{
		*s_axis = VX; *t_axis = VY;
		return true;
	}
	else if (face == 1)
	{
		*s_axis = VX; *t_axis = VZ;
		return true;
	}
	else if (face == 2)
	{
		*s_axis = VY; *t_axis = VZ;
		return true;
	}
	else if (face == 3)
	{
		*s_axis = VX; *t_axis = VZ;
		return true;
	}
	else if (face == 4)
	{
		*s_axis = VY; *t_axis = VZ;
		return true;
	}
	else if (face == 5)
	{
		*s_axis = VX; *t_axis = VY;
		return true;
	}
	else if (face == 6)
	{
		*s_axis = VX; *t_axis = VY;
		return true;
	}
	else
	{
		// Unknown face
		return false;
	}
}

//============================================================================

//static
bool LLNetworkData::isValid(U16 param_type, U32 size)
{
	switch (param_type)
	{
		case PARAMS_FLEXIBLE:
			return size == 16;

		case PARAMS_LIGHT:
			return size == 16;

		case PARAMS_SCULPT:
			return size == 17;

		case PARAMS_LIGHT_IMAGE:
			return size == 28;

		case PARAMS_EXTENDED_MESH:
			return size == 4;

		case PARAMS_RENDER_MATERIAL:
			return size > 1;

		case PARAMS_REFLECTION_PROBE:
			return size == 9;
	}

	return false;
}

//============================================================================

LLLightParams::LLLightParams()
{
	mColor.setToWhite();
	mRadius = 10.f;
	mCutoff = 0.f;
	mFalloff = 0.75f;

	mType = PARAMS_LIGHT;
}

bool LLLightParams::pack(LLDataPacker& dp) const
{
	LLColor4U color4u(mColor);
	dp.packColor4U(color4u, "color");
	dp.packF32(mRadius, "radius");
	dp.packF32(mCutoff, "cutoff");
	dp.packF32(mFalloff, "falloff");
	return true;
}

bool LLLightParams::unpack(LLDataPacker& dp)
{
	LLColor4U color;
	dp.unpackColor4U(color, "color");
	setLinearColor(LLColor4(color));

	F32 radius;
	dp.unpackF32(radius, "radius");
	setRadius(radius);

	F32 cutoff;
	dp.unpackF32(cutoff, "cutoff");
	setCutoff(cutoff);

	F32 falloff;
	dp.unpackF32(falloff, "falloff");
	setFalloff(falloff);

	return true;
}

bool LLLightParams::operator==(const LLNetworkData& data) const
{
	if (data.mType != PARAMS_LIGHT)
	{
		return false;
	}
	const LLLightParams* param = (const LLLightParams*)&data;
	if (param->mColor != mColor || param->mRadius != mRadius ||
		param->mCutoff != mCutoff || param->mFalloff != mFalloff)
	{
		return false;
	}
	return true;
}

void LLLightParams::copy(const LLNetworkData& data)
{
	const LLLightParams* param = (LLLightParams*)&data;
	mType = param->mType;
	mColor = param->mColor;
	mRadius = param->mRadius;
	mCutoff = param->mCutoff;
	mFalloff = param->mFalloff;
}

LLSD LLLightParams::asLLSD() const
{
	LLSD sd;

	sd["color"] = ll_sd_from_color4(getLinearColor());
	sd["radius"] = getRadius();
	sd["falloff"] = getFalloff();
	sd["cutoff"] = getCutoff();

	return sd;
}

bool LLLightParams::fromLLSD(LLSD& sd)
{
	const char* w;

	w = "color";
	if (sd.has(w))
	{
		setLinearColor(ll_color4_from_sd(sd[w]));
	}
	else
	{
		return false;
	}

	w = "radius";
	if (sd.has(w))
	{
		setRadius((F32)sd[w].asReal());
	}
	else
	{
		return false;
	}

	w = "falloff";
	if (sd.has(w))
	{
		setFalloff((F32)sd[w].asReal());
	}
	else
	{
		return false;
	}

	w = "cutoff";
	if (sd.has(w))
	{
		setCutoff((F32)sd[w].asReal());
	}
	else
	{
		return false;
	}

	return true;
}

//============================================================================

LLFlexibleObjectData::LLFlexibleObjectData()
{
	mSimulateLOD				= FLEXIBLE_OBJECT_DEFAULT_NUM_SECTIONS;
	mGravity					= FLEXIBLE_OBJECT_DEFAULT_GRAVITY;
	mAirFriction				= FLEXIBLE_OBJECT_DEFAULT_AIR_FRICTION;
	mWindSensitivity			= FLEXIBLE_OBJECT_DEFAULT_WIND_SENSITIVITY;
	mTension					= FLEXIBLE_OBJECT_DEFAULT_TENSION;
	mUserForce					= LLVector3(0.f, 0.f, 0.f);
	mType						= PARAMS_FLEXIBLE;
#if 0
	mUsingCollisionSphere		= FLEXIBLE_OBJECT_DEFAULT_USING_COLLISION_SPHERE;
	mRenderingCollisionSphere	= FLEXIBLE_OBJECT_DEFAULT_RENDERING_COLLISION_SPHERE;
#endif
}

bool LLFlexibleObjectData::pack(LLDataPacker& dp) const
{
	// Custom, uber-svelte pack "softness" in upper bits of tension & drag
	U8 bit1 = (mSimulateLOD & 2) << 6;
	U8 bit2 = (mSimulateLOD & 1) << 7;
	dp.packU8((U8)(mTension * 10.01f) + bit1, "tension");
	dp.packU8((U8)(mAirFriction * 10.01f) + bit2, "drag");
	dp.packU8((U8)((mGravity + 10.f) * 10.01f), "gravity");
	dp.packU8((U8)(mWindSensitivity * 10.01f), "wind");
	dp.packVector3(mUserForce, "userforce");
	return true;
}

bool LLFlexibleObjectData::unpack(LLDataPacker& dp)
{
	U8 tension, friction, gravity, wind;
	U8 bit1, bit2;

	dp.unpackU8(tension, "tension");
	bit1 = (tension >> 6) & 2;
	mTension = (F32)(tension & 0x7f) / 10.f;

	dp.unpackU8(friction, "drag");
	bit2 = (friction >> 7) & 1;
	mAirFriction = (F32)(friction & 0x7f) / 10.f;

	mSimulateLOD = bit1 | bit2;

	dp.unpackU8(gravity, "gravity");
	mGravity = (F32)gravity / 10.f - 10.f;

	dp.unpackU8(wind, "wind");
	mWindSensitivity = (F32)wind / 10.f;

	if (dp.hasNext())
	{
		dp.unpackVector3(mUserForce, "userforce");
	}
	else
	{
		mUserForce.set(0.f, 0.f, 0.f);
	}

	return true;
}

bool LLFlexibleObjectData::operator==(const LLNetworkData& data) const
{
	if (data.mType != PARAMS_FLEXIBLE)
	{
		return false;
	}
	LLFlexibleObjectData *flex_data = (LLFlexibleObjectData*)&data;
	return (mSimulateLOD == flex_data->mSimulateLOD &&
			mGravity == flex_data->mGravity &&
			mAirFriction == flex_data->mAirFriction &&
			mWindSensitivity == flex_data->mWindSensitivity &&
			mTension == flex_data->mTension &&
			mUserForce == flex_data->mUserForce);
#if 0
			mUsingCollisionSphere == flex_data->mUsingCollisionSphere &&
			mRenderingCollisionSphere == flex_data->mRenderingCollisionSphere;
#endif
}

void LLFlexibleObjectData::copy(const LLNetworkData& data)
{
	const LLFlexibleObjectData *flex_data = (LLFlexibleObjectData*)&data;
	mSimulateLOD = flex_data->mSimulateLOD;
	mGravity = flex_data->mGravity;
	mAirFriction = flex_data->mAirFriction;
	mWindSensitivity = flex_data->mWindSensitivity;
	mTension = flex_data->mTension;
	mUserForce = flex_data->mUserForce;
#if 0
	mUsingCollisionSphere = flex_data->mUsingCollisionSphere;
	mRenderingCollisionSphere = flex_data->mRenderingCollisionSphere;
#endif
}

LLSD LLFlexibleObjectData::asLLSD() const
{
	LLSD sd;

	sd["air_friction"] = getAirFriction();
	sd["gravity"] = getGravity();
	sd["simulate_lod"] = getSimulateLOD();
	sd["tension"] = getTension();
	sd["user_force"] = getUserForce().getValue();
	sd["wind_sensitivity"] = getWindSensitivity();

	return sd;
}

bool LLFlexibleObjectData::fromLLSD(LLSD& sd)
{
	const char* w;

	w = "air_friction";
	if (sd.has(w))
	{
		setAirFriction((F32)sd[w].asReal());
	}
	else
	{
		return false;
	}

	w = "gravity";
	if (sd.has(w))
	{
		setGravity((F32)sd[w].asReal());
	}
	else
	{
		return false;
	}

	w = "simulate_lod";
	if (sd.has(w))
	{
		setSimulateLOD(sd[w].asInteger());
	}
	else
	{
		return false;
	}

	w = "tension";
	if (sd.has(w))
	{
		setTension((F32)sd[w].asReal());
	}
	else
	{
		return false;
	}

	w = "user_force";
	if (sd.has(w))
	{
		LLVector3 user_force = ll_vector3_from_sd(sd[w], 0);
		setUserForce(user_force);
	}
	else
	{
		return false;
	}

	w = "wind_sensitivity";
	if (sd.has(w))
	{
		setWindSensitivity((F32)sd[w].asReal());
	}
	else
	{
		return false;
	}

	return true;
}

//============================================================================

LLSculptParams::LLSculptParams()
{
	mType = PARAMS_SCULPT;
	mSculptTexture.set(SCULPT_DEFAULT_TEXTURE);
	mSculptType = LL_SCULPT_TYPE_SPHERE;
}

bool LLSculptParams::pack(LLDataPacker& dp) const
{
	dp.packUUID(mSculptTexture, "texture");
	dp.packU8(mSculptType, "type");
	return true;
}

bool LLSculptParams::unpack(LLDataPacker& dp)
{
	LLUUID id;
	dp.unpackUUID(id, "texture");
	U8 type;
	dp.unpackU8(type, "type");
	setSculptTexture(id, type);
	return true;
}

bool LLSculptParams::operator==(const LLNetworkData& data) const
{
	if (data.mType != PARAMS_SCULPT)
	{
		return false;
	}

	const LLSculptParams* param = (const LLSculptParams*)&data;
	return param->mSculptTexture == mSculptTexture &&
		   param->mSculptType == mSculptType;
}

void LLSculptParams::copy(const LLNetworkData& data)
{
	const LLSculptParams* param = (LLSculptParams*)&data;
	setSculptTexture(param->mSculptTexture, param->mSculptType);
}

LLSD LLSculptParams::asLLSD() const
{
	LLSD sd;
	sd["texture"] = mSculptTexture;
	sd["type"] = mSculptType;
	return sd;
}

bool LLSculptParams::fromLLSD(LLSD& sd)
{
	if (sd.has("type") && sd.has("texture"))
	{
		setSculptTexture(sd["texture"].asUUID(), sd["type"].asInteger());
		return true;
	}
	return false;
}

void LLSculptParams::setSculptTexture(const LLUUID& texture_id, U8 sculpt_type)
{
	U8 type = sculpt_type & LL_SCULPT_TYPE_MASK;
	U8 flags = sculpt_type & LL_SCULPT_FLAG_MASK;
	if (sculpt_type != (type | flags) || type > LL_SCULPT_TYPE_MAX)
	{
		mSculptTexture.set(SCULPT_DEFAULT_TEXTURE);
		mSculptType = LL_SCULPT_TYPE_SPHERE;
	}
	else
	{
		mSculptTexture = texture_id;
		mSculptType = sculpt_type;
	}
}

//============================================================================

LLLightImageParams::LLLightImageParams()
{
	mType = PARAMS_LIGHT_IMAGE;
	mParams.set(F_PI * 0.5f, 0.f, 0.f);
}

bool LLLightImageParams::pack(LLDataPacker& dp) const
{
	dp.packUUID(mLightTexture, "texture");
	dp.packVector3(mParams, "params");
	return true;
}

bool LLLightImageParams::unpack(LLDataPacker& dp)
{
	dp.unpackUUID(mLightTexture, "texture");
	dp.unpackVector3(mParams, "params");
	return true;
}

bool LLLightImageParams::operator==(const LLNetworkData& data) const
{
	if (data.mType != PARAMS_LIGHT_IMAGE)
	{
		return false;
	}

	const LLLightImageParams* param = (const LLLightImageParams*)&data;
	return param->mLightTexture == mLightTexture && param->mParams == mParams;
}

void LLLightImageParams::copy(const LLNetworkData& data)
{
	const LLLightImageParams* param = (LLLightImageParams*)&data;
	mLightTexture = param->mLightTexture;
	mParams = param->mParams;
}

LLSD LLLightImageParams::asLLSD() const
{
	LLSD sd;

	sd["texture"] = mLightTexture;
	sd["params"] = mParams.getValue();

	return sd;
}

bool LLLightImageParams::fromLLSD(LLSD& sd)
{
	if (sd.has("texture"))
	{
		setLightTexture(sd["texture"]);
		setParams(LLVector3(sd["params"]));
		return true;
	}

	return false;
}

//============================================================================

LLExtendedMeshParams::LLExtendedMeshParams()
{
	mType = PARAMS_EXTENDED_MESH;
	mFlags = 0;
}

bool LLExtendedMeshParams::pack(LLDataPacker& dp) const
{
	dp.packU32(mFlags, "flags");
	return true;
}

bool LLExtendedMeshParams::unpack(LLDataPacker& dp)
{
	dp.unpackU32(mFlags, "flags");
	return true;
}

bool LLExtendedMeshParams::operator==(const LLNetworkData& data) const
{
	if (data.mType != PARAMS_EXTENDED_MESH)
	{
		return false;
	}

	const LLExtendedMeshParams* param = (const LLExtendedMeshParams*)&data;
	if (param->mFlags != mFlags)
	{
		return false;
	}

	return true;
}

void LLExtendedMeshParams::copy(const LLNetworkData& data)
{
	const LLExtendedMeshParams* param = (LLExtendedMeshParams*)&data;
	mFlags = param->mFlags;
}

LLSD LLExtendedMeshParams::asLLSD() const
{
	LLSD sd;
	sd["flags"] = LLSD::Integer(mFlags);
	return sd;
}

bool LLExtendedMeshParams::fromLLSD(LLSD& sd)
{
	if (sd.has("flags"))
	{
		setFlags(sd["flags"].asInteger());
		return true;
	}
	return false;
}

//============================================================================

// Relfection probes constants:
const F32 REFLECTION_PROBE_MIN_AMBIANCE = 0.f;
const F32 REFLECTION_PROBE_MAX_AMBIANCE = 100.f;
const F32 REFLECTION_PROBE_DEFAULT_AMBIANCE = 0.f;
// Note: clip distances are clamped in LLCamera::setNear. The max clip distance
// is currently limited by the skybox.
const F32 REFLECTION_PROBE_MIN_CLIP_DISTANCE = 0.f;
const F32 REFLECTION_PROBE_MAX_CLIP_DISTANCE = 1024.f;
const F32 REFLECTION_PROBE_DEFAULT_CLIP_DISTANCE = 0.f;

LLReflectionProbeParams::LLReflectionProbeParams()
:	mAmbiance(REFLECTION_PROBE_DEFAULT_AMBIANCE),
	mClipDistance(REFLECTION_PROBE_DEFAULT_CLIP_DISTANCE),
	mFlags(0)
{
	mType = PARAMS_REFLECTION_PROBE;
}

bool LLReflectionProbeParams::pack(LLDataPacker& dp) const
{
	dp.packF32(mAmbiance, "ambiance");
	dp.packF32(mClipDistance, "clip_distance");
	dp.packU8(mFlags, "flags");
	return true;
}

bool LLReflectionProbeParams::unpack(LLDataPacker& dp)
{
	F32 ambiance;
	dp.unpackF32(ambiance, "ambiance");
	setAmbiance(ambiance);

	F32 clip_distance;
	dp.unpackF32(clip_distance, "clip_distance");
	setClipDistance(clip_distance);

	dp.unpackU8(mFlags, "flags");

	return true;
}

bool LLReflectionProbeParams::operator==(const LLNetworkData& data) const
{
	if (data.mType != PARAMS_REFLECTION_PROBE)
	{
		return false;
	}

	const LLReflectionProbeParams* paramp =
		(const LLReflectionProbeParams*)&data;
	if (paramp->mAmbiance != mAmbiance)
	{
		return false;
	}
	if (paramp->mClipDistance != mClipDistance)
	{
		return false;
	}
	if (paramp->mFlags != mFlags)
	{
		return false;
	}

	return true;
}

void LLReflectionProbeParams::copy(const LLNetworkData& data)
{
	const LLReflectionProbeParams* paramp = (LLReflectionProbeParams*)&data;
	mType = paramp->mType;
	mAmbiance = paramp->mAmbiance;
	mClipDistance = paramp->mClipDistance;
	mFlags = paramp->mFlags;
}

LLSD LLReflectionProbeParams::asLLSD() const
{
	LLSD sd;
	sd["ambiance"] = getAmbiance();
	sd["clip_distance"] = getClipDistance();
	sd["flags"] = LLSD::Integer(mFlags);
	return sd;
}

bool LLReflectionProbeParams::fromLLSD(LLSD& sd)
{
	if (sd.has("ambiance") && sd.has("clip_distance") && sd.has("flags"))
	{
		setAmbiance((F32)sd["ambiance"].asReal());
		setClipDistance((F32)sd["clip_distance"].asReal());
		mFlags = (U8)sd["flags"].asInteger();
		return true;
	}
	return false;
}

//============================================================================

LLRenderMaterialParams::LLRenderMaterialParams()
{
	mType = PARAMS_RENDER_MATERIAL;
}

bool LLRenderMaterialParams::pack(LLDataPacker& dp) const
{
	// Limited to 255 bytes, no more than 14 material Ids
	U8 count = (U8)llmin((S32)mEntries.size(), 14);
	dp.packU8(count, "count");

	for (U8 i = 0; i < count; ++i)
	{
		const Entry& entry = mEntries[i];
		dp.packU8(entry.te_idx, "te_idx");
		dp.packUUID(entry.id, "id");
	}

	return true;
}

bool LLRenderMaterialParams::unpack(LLDataPacker& dp)
{
	U8 count;
	dp.unpackU8(count, "count");

	mEntries.clear();
	mEntries.reserve(count);
	U8 te_idx;
	LLUUID id;
	for (U32 i = 0; i < count; ++i)
	{
		dp.unpackU8(te_idx, "te_idx");
		dp.unpackUUID(id, "te_id");
		mEntries.emplace_back(te_idx, id);
	}

	return true;
}

bool LLRenderMaterialParams::operator==(const LLNetworkData& data) const
{
	if (data.mType != PARAMS_RENDER_MATERIAL)
	{
		return false;
	}

	const LLRenderMaterialParams* paramp =
		(const LLRenderMaterialParams*)&data;
	size_t count = mEntries.size();
	if (paramp->mEntries.size() != count)
	{
		return false;
	}

	for (size_t i = 0; i < count; ++i)
	{
		const Entry& entry = mEntries[i];
		if (paramp->getMaterial(entry.te_idx) != entry.id)
		{
			return false;
		}
	}

	return true;
}

void LLRenderMaterialParams::copy(const LLNetworkData& data)
{
	mEntries = ((const LLRenderMaterialParams*)&data)->mEntries;
}

#if 0	// Not used
LLSD LLRenderMaterialParams::asLLSD() const
{
	LLSD sd;
	for (U32 i = 0, count = mEntries.size(); i < count; ++i)
	{
		const Entry& entry = mEntries[i];
		sd[i]["te_idx"] = entry.te_idx;
		sd[i]["id"] = entry.id;
	}
	return sd;
}

bool LLRenderMaterialParams::fromLLSD(LLSD& sd)
{
    if (!sd.isArray())
    {
		return false;
	}
	U32 count = sd.size();
	mEntries.clear();
	mEntries.reserve(count);
	for (U32 i = 0; i < count; ++i)
	{
		if (!sd[i].has("te_idx") || !sd[i].has("id"))
		{
			return false;
		}
		mEntries.emplace_back(sd[i]["te_idx"].asInteger(),
							  sd[i]["id"].asUUID());
	}
	return true;
}
#endif

void LLRenderMaterialParams::setMaterial(U8 te, const LLUUID& id)
{
	bool erase_entry = id.isNull();
	for (U32 i = 0, count = mEntries.size(); i < count; ++i)
	{
		Entry& entry = mEntries[i];
		if (entry.te_idx == te)
		{
			if (erase_entry)
			{
				mEntries.erase(mEntries.begin() + i);
			}
			else
			{
				entry.id = id;
			}
			return;
		}
	}
	// This is a new te entry.
	mEntries.emplace_back(te, id);
}

const LLUUID& LLRenderMaterialParams::getMaterial(U8 te) const
{
	for (U32 i = 0, count = mEntries.size(); i < count; ++i)
	{
		const Entry& entry = mEntries[i];
		if (entry.te_idx == te)
		{
			return entry.id;
		}
	}
	return LLUUID::null;
}
