/**
 * @file llprimitive.h
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

#ifndef LL_LLPRIMITIVE_H
#define LL_LLPRIMITIVE_H

#include "llprimtexturelist.h"
#include "lltextureentry.h"
#include "llmessage.h"
#include "llxform.h"
#include "indra_constants.h"

#define MAX_TES 45U
#define MAX_TE_BUFFER 4096U

class LLColor3;
class LLColor4U;
class LLDataPacker;
class LLMessageSystem;
class LLTextureEntry;
class LLVolumeParams;

enum LLGeomType // Note: same values as GL Ids
{
	LLInvalid   = 0,
	LLLineLoop  = 2,
	LLLineStrip = 3,
	LLTriangles = 4,
	LLTriStrip  = 5,
	LLTriFan    = 6,
	LLQuads     = 7,
	LLQuadStrip = 8
};

// Old inverted texture: "7595d345-a24c-e7ef-f0bd-78793792133e";
extern const char* SCULPT_DEFAULT_TEXTURE;

// Texture rotations are sent over the wire as a S16. This is used to scale the
// actual float value to a S16. Do not use 7FFF as it introduces some odd
// rounding with 180 since it can't be divided by 2. See DEV-19108
constexpr F32 TEXTURE_ROTATION_PACK_FACTOR = ((F32)0x08000);

//============================================================================

// Macros used (in llviewerobject.cpp only, but keeping the macros here in case
// the extra params enum is modified in the future) to transform a parameter
// type into an index (starting from 0) in a vector/array, and vice versa.
// This works because LLNetworkData::PARAMS_* below are a series starting from
// 0x10 with a 0x10 increment. Should this change, these macros should change !
#define LL_EPARAM_INDEX(param_type) ((param_type >> 4) - 1)
#define LL_EPARAM_TYPE(index) ((index + 1) << 4)
// Number of LLNetworkData::PARAMS_* entries in the enum below
#define LL_EPARAMS_COUNT 9

// TomY: base class for things that pack & unpack themselves
class LLNetworkData
{
public:
	// Extra parameter IDs
	enum
	{
		PARAMS_FLEXIBLE			= 0x10,
		PARAMS_LIGHT			= 0x20,
		PARAMS_SCULPT			= 0x30,
		PARAMS_LIGHT_IMAGE		= 0x40,
		PARAMS_RESERVED			= 0x50,	// Used on server-side
		PARAMS_MESH				= 0x60,
		PARAMS_EXTENDED_MESH	= 0x70,
		PARAMS_RENDER_MATERIAL	= 0x80,
		PARAMS_REFLECTION_PROBE	= 0x90,
	};

	virtual ~LLNetworkData()							{}
	virtual bool pack(LLDataPacker& dp) const = 0;
	virtual bool unpack(LLDataPacker& dp) = 0;
	virtual bool operator==(const LLNetworkData& data) const = 0;
	virtual void copy(const LLNetworkData& data) = 0;
	static bool isValid(U16 param_type, U32 size);

public:
	U16 mType;
};

class LLLightParams : public LLNetworkData
{
public:
	LLLightParams();
	bool pack(LLDataPacker& dp) const override;
	bool unpack(LLDataPacker& dp) override;
	bool operator==(const LLNetworkData& data) const override;
	void copy(const LLNetworkData& data) override;
	// LLSD implementations here are provided by Eddy Stryker.
	// NOTE: there are currently unused in protocols
	LLSD asLLSD() const;
	LL_INLINE operator LLSD() const						{ return asLLSD(); }
	bool fromLLSD(LLSD& sd);

	LL_INLINE void setLinearColor(const LLColor4& color)
	{
		mColor = color;
		mColor.clamp();
	}

	LL_INLINE void setSRGBColor(const LLColor4& color)	{ setLinearColor(linearColor4(color)); }

	LL_INLINE void setRadius(F32 radius)				{ mRadius = llclamp(radius, LIGHT_MIN_RADIUS, LIGHT_MAX_RADIUS); }
	LL_INLINE void setFalloff(F32 falloff)				{ mFalloff = llclamp(falloff, LIGHT_MIN_FALLOFF, LIGHT_MAX_FALLOFF); }
	LL_INLINE void setCutoff(F32 cutoff)				{ mCutoff = llclamp(cutoff, LIGHT_MIN_CUTOFF, LIGHT_MAX_CUTOFF); }

	LL_INLINE LLColor4 getLinearColor() const			{ return mColor; }
	LL_INLINE LLColor4 getSRGBColor() const				{ return srgbColor4(mColor); }

	LL_INLINE F32 getRadius() const						{ return mRadius; }
	LL_INLINE F32 getFalloff() const					{ return mFalloff; }
	LL_INLINE F32 getCutoff() const						{ return mCutoff; }

private:
	// Linear color (not gamma corrected), with alpha = intensity
	LLColor4	mColor;

	F32			mRadius;
	F32			mFalloff;
	F32			mCutoff;
};

// These were made into enums so that they could be used as fixed size array
// bounds.
enum EFlexibleObjectConst
{
	// "Softness" => [0,3], increments of 1
	// Represents powers of 2: 0 -> 1, 3 -> 8
	FLEXIBLE_OBJECT_MIN_SECTIONS = 0,
	FLEXIBLE_OBJECT_DEFAULT_NUM_SECTIONS = 2,
	FLEXIBLE_OBJECT_MAX_SECTIONS = 3
};

class LLFlexibleObjectData : public LLNetworkData
{
public:
	LLFlexibleObjectData();

	LL_INLINE void setSimulateLOD(S32 lod)				{ mSimulateLOD = llclamp(lod, (S32)FLEXIBLE_OBJECT_MIN_SECTIONS, (S32)FLEXIBLE_OBJECT_MAX_SECTIONS); }
	LL_INLINE void setGravity(F32 gravity)				{ mGravity = llclamp(gravity, FLEXIBLE_OBJECT_MIN_GRAVITY, FLEXIBLE_OBJECT_MAX_GRAVITY); }
	LL_INLINE void setAirFriction(F32 friction)			{ mAirFriction = llclamp(friction, FLEXIBLE_OBJECT_MIN_AIR_FRICTION, FLEXIBLE_OBJECT_MAX_AIR_FRICTION); }
	LL_INLINE void setWindSensitivity(F32 wind)			{ mWindSensitivity = llclamp(wind, FLEXIBLE_OBJECT_MIN_WIND_SENSITIVITY, FLEXIBLE_OBJECT_MAX_WIND_SENSITIVITY); }
	LL_INLINE void setTension(F32 tension)				{ mTension = llclamp(tension, FLEXIBLE_OBJECT_MIN_TENSION, FLEXIBLE_OBJECT_MAX_TENSION); }
	LL_INLINE void setUserForce(LLVector3& force)		{ mUserForce = force; }

	LL_INLINE S32 getSimulateLOD() const				{ return mSimulateLOD; }
	LL_INLINE F32 getGravity() const					{ return mGravity; }
	LL_INLINE F32 getAirFriction() const				{ return mAirFriction; }
	LL_INLINE F32 getWindSensitivity() const			{ return mWindSensitivity; }
	LL_INLINE F32 getTension() const					{ return mTension; }
	LL_INLINE LLVector3 getUserForce() const			{ return mUserForce; }

	bool pack(LLDataPacker& dp) const;
	bool unpack(LLDataPacker& dp);

	bool operator==(const LLNetworkData& data) const;

	void copy(const LLNetworkData& data);

	LLSD asLLSD() const;
	LL_INLINE operator LLSD() const						{ return asLLSD(); }
	bool fromLLSD(LLSD& sd);

protected:
	S32			mSimulateLOD;		// 2^n = number of simulated sections
	F32			mGravity;
	F32			mAirFriction;		// higher is more stable, but too much looks like it's underwater
	F32			mWindSensitivity;	// interacts with tension, air friction, and gravity
	F32			mTension;			// interacts in complex ways with other parameters
	LLVector3	mUserForce;			// custom user-defined force vector
#if 0
	bool		mUsingCollisionSphere;
	bool		mRenderingCollisionSphere;
#endif
};

class LLSculptParams : public LLNetworkData
{
public:
	LLSculptParams();
	bool pack(LLDataPacker& dp) const override;
	bool unpack(LLDataPacker& dp) override;
	bool operator==(const LLNetworkData& data) const override;
	void copy(const LLNetworkData& data) override;
	LLSD asLLSD() const;
	LL_INLINE operator LLSD() const						{ return asLLSD(); }
	bool fromLLSD(LLSD& sd);

	void setSculptTexture(const LLUUID& id, U8 sculpt_type);

	LL_INLINE const LLUUID& getSculptTexture() const	{ return mSculptTexture; }
	LL_INLINE U8 getSculptType() const					{ return mSculptType; }

protected:
	LLUUID	mSculptTexture;
	U8		mSculptType;
};

class LLLightImageParams : public LLNetworkData
{
public:
	LLLightImageParams();
	bool pack(LLDataPacker& dp) const override;
	bool unpack(LLDataPacker& dp) override;
	bool operator==(const LLNetworkData& data) const override;
	void copy(const LLNetworkData& data) override;
	LLSD asLLSD() const;
	LL_INLINE operator LLSD() const						{ return asLLSD(); }
	bool fromLLSD(LLSD& sd);

	LL_INLINE void setLightTexture(const LLUUID& id)	{ mLightTexture = id; }
	LL_INLINE const LLUUID& getLightTexture() const		{ return mLightTexture; }
	LL_INLINE bool isLightSpotlight() const				{ return mLightTexture.notNull(); }
	LL_INLINE void setParams(const LLVector3& params)	{ mParams = params; }
	LL_INLINE LLVector3 getParams() const				{ return mParams; }

protected:
	LLUUID		mLightTexture;
	LLVector3	mParams;
};

class LLExtendedMeshParams : public LLNetworkData
{
public:
	LLExtendedMeshParams();

	bool pack(LLDataPacker& dp) const override;
	bool unpack(LLDataPacker& dp) override;
	bool operator==(const LLNetworkData& data) const override;
	void copy(const LLNetworkData& data) override;

	LL_INLINE operator LLSD() const						{ return asLLSD(); }
	LLSD asLLSD() const;
	bool fromLLSD(LLSD& sd);

	LL_INLINE void setFlags(U32 flags)					{ mFlags = flags; }
	LL_INLINE U32 getFlags() const						{ return mFlags; }

public:
	static constexpr U32 ANIMATED_MESH_ENABLED_FLAG = 0x1 << 0;

protected:
	U32 mFlags;
};

// Relfection probes constants:
extern const F32 REFLECTION_PROBE_MIN_AMBIANCE;
extern const F32 REFLECTION_PROBE_MAX_AMBIANCE;
extern const F32 REFLECTION_PROBE_DEFAULT_AMBIANCE;
extern const F32 REFLECTION_PROBE_MIN_CLIP_DISTANCE;
extern const F32 REFLECTION_PROBE_MAX_CLIP_DISTANCE;
extern const F32 REFLECTION_PROBE_DEFAULT_CLIP_DISTANCE;

class LLReflectionProbeParams : public LLNetworkData
{
public:
	LLReflectionProbeParams();

	bool pack(LLDataPacker& dp) const override;
	bool unpack(LLDataPacker& dp) override;
	bool operator==(const LLNetworkData& data) const override;
	void copy(const LLNetworkData& data) override;

	LL_INLINE operator LLSD() const						{ return asLLSD(); }
	LLSD asLLSD() const;
	bool fromLLSD(LLSD& sd);

	enum EFlags : U8
	{
		FLAG_BOX_VOLUME	= 0x01,	// Use a box influence volume
		FLAG_DYNAMIC	= 0x02,	// Render dynamic objects (avatars)
								// into this reflection probe.
	};

	LL_INLINE void setAmbiance(F32 val)
	{
		mAmbiance = llclamp(val, REFLECTION_PROBE_MIN_AMBIANCE,
							REFLECTION_PROBE_MAX_AMBIANCE);
	}

	LL_INLINE F32 getAmbiance() const					{ return mAmbiance; }

	LL_INLINE void setClipDistance(F32 dist)
	{
		mClipDistance = llclamp(dist, REFLECTION_PROBE_MIN_CLIP_DISTANCE,
								REFLECTION_PROBE_MAX_CLIP_DISTANCE);
	}

	LL_INLINE F32 getClipDistance() const				{ return mClipDistance; }

	LL_INLINE void setIsBox(bool is_box)
	{
		if (is_box)
		{
			mFlags |= FLAG_BOX_VOLUME;
		}
		else
		{
			mFlags &= ~FLAG_BOX_VOLUME;
		}
	}

	LL_INLINE bool getIsBox() const
	{
		return (mFlags & FLAG_BOX_VOLUME) != 0;
	}

	LL_INLINE void setIsDynamic(bool is_dynamic)
	{
		if (is_dynamic)
		{
			mFlags |= FLAG_DYNAMIC;
		}
		else
		{
			mFlags &= ~FLAG_DYNAMIC;
		}
	}

	LL_INLINE bool getIsDynamic() const
	{
		return (mFlags & FLAG_DYNAMIC) != 0;
	}

protected:
	F32	mAmbiance;
	F32	mClipDistance;
	U8	mFlags;
};

class LLRenderMaterialParams : public LLNetworkData
{
public:
	LLRenderMaterialParams();

	bool pack(LLDataPacker& dp) const override;
	bool unpack(LLDataPacker& dp) override;
	bool operator==(const LLNetworkData& data) const override;
	void copy(const LLNetworkData& data) override;

#if 0	// Not used
	LL_INLINE operator LLSD() const						{ return asLLSD(); }
	LLSD asLLSD() const;
	bool fromLLSD(LLSD& sd);
#endif

	void setMaterial(U8 te_idx, const LLUUID& id);
	const LLUUID& getMaterial(U8 te_idx) const;

	LL_INLINE bool isEmpty() const						{ return mEntries.empty(); }

protected:
	struct Entry
	{
		Entry() = default;

		Entry(U8 idx, LLUUID uuid)
		:	te_idx(idx),
			id(uuid)
		{
		}

		LLUUID	id;
		U8		te_idx;
	};

	std::vector<Entry> mEntries;
};

// This code is not naming-standards compliant. Leaving it like this for now to
// make the connection to code in packTEMessage(LLDataPacker&) more obvious.
// This should be refactored to remove the duplication, at which point we can
// fix the names as well. - Vir
struct LLTEContents
{
	LLUUID			image_data[MAX_TES];
	LLColor4U		colors[MAX_TES];
	F32				scale_s[MAX_TES];
	F32				scale_t[MAX_TES];
	S16				offset_s[MAX_TES];
	S16				offset_t[MAX_TES];
	S16				image_rot[MAX_TES];
	U8				bump[MAX_TES];
	U8				media_flags[MAX_TES];
    U8				glow[MAX_TES];
	LLMaterialID	material_ids[MAX_TES];
	U8				packed_buffer[MAX_TE_BUFFER];
	U32				size;
	U32				face_count;
};

class LLPrimitive : public LLXform
{
protected:
	LOG_CLASS(LLPrimitive);

public:
	// Allows to change the limits for prim parameters for SL or OpenSim
	static void setLimits(bool for_secondlife);

	// These flags influence how the RigidBody representation is built
	static constexpr U32 PRIM_FLAG_PHANTOM 				= 0x1 << 0;
	static constexpr U32 PRIM_FLAG_VOLUME_DETECT 		= 0x1 << 1;
	static constexpr U32 PRIM_FLAG_DYNAMIC 				= 0x1 << 2;
	static constexpr U32 PRIM_FLAG_AVATAR 				= 0x1 << 3;
	static constexpr U32 PRIM_FLAG_SCULPT 				= 0x1 << 4;
	// Not used yet, but soon
	static constexpr U32 PRIM_FLAG_COLLISION_CALLBACK 	= 0x1 << 5;
	static constexpr U32 PRIM_FLAG_CONVEX 				= 0x1 << 6;
	static constexpr U32 PRIM_FLAG_DEFAULT_VOLUME		= 0x1 << 7;
	static constexpr U32 PRIM_FLAG_SITTING				= 0x1 << 8;
	// Set along with PRIM_FLAG_SITTING
	static constexpr U32 PRIM_FLAG_SITTING_ON_GROUND	= 0x1 << 9;

	LLPrimitive();
	~LLPrimitive() override;

	// *HACK: for Windoze confusion about ostream operator in LLVolume:
	LL_INLINE const LLVolume* getVolumeConst() const	{ return mVolumep; }
	LL_INLINE LLVolume* getVolume() const				{ return mVolumep; }
	virtual bool setVolume(const LLVolumeParams& volume_params,
						   S32 detail, bool unique_volume = false);

	// Modify texture entry properties
	LL_INLINE bool validTE(U8 te_num) const				{ return mNumTEs && te_num < mNumTEs; }

	LLTextureEntry* getTE(U8 te_num) const;

	virtual void setNumTEs(U8 num_tes);
	virtual void setAllTESelected(bool sel);
	virtual void setAllTETextures(const LLUUID& tex_id);
	virtual void setTE(U8 index, const LLTextureEntry& te);
	virtual S32 setTEColor(U8 te, const LLColor4& color);
	virtual S32 setTEColor(U8 te, const LLColor3& color);
	virtual S32 setTEAlpha(U8 te, F32 alpha);
	virtual S32 setTETexture(U8 te, const LLUUID& tex_id);
	virtual S32 setTEScale(U8 te, F32 s, F32 t);
	virtual S32 setTEScaleS(U8 te, F32 s);
	virtual S32 setTEScaleT(U8 te, F32 t);
	virtual S32 setTEOffset(U8 te, F32 s, F32 t);
	virtual S32 setTEOffsetS(U8 te, F32 s);
	virtual S32 setTEOffsetT(U8 te, F32 t);
	virtual S32 setTERotation(U8 te, F32 r);
	virtual S32 setTEBumpShinyFullbright(U8 te, U8 bump);
	virtual S32 setTEBumpShiny(U8 te, U8 bump);
	virtual S32 setTEMediaTexGen(U8 te, U8 media);
	virtual S32 setTEBumpmap(U8 te, U8 bump);
	virtual S32 setTETexGen(U8 te, U8 texgen);
	virtual S32 setTEShiny(U8 te, U8 shiny);
	virtual S32 setTEFullbright(U8 te, U8 fullbright);
	virtual S32 setTEMediaFlags(U8 te, U8 flags);
	virtual S32 setTEGlow(U8 te, F32 glow);
	virtual S32 setTEMaterialID(U8 te, const LLMaterialID& matidp);
	virtual S32 setTEMaterialParams(U8 index, const LLMaterialPtr paramsp);

	// Returns true if material changed:
	virtual bool setMaterial(U8 material);

	void setTESelected(U8 te, bool sel);

	LLMaterialPtr getTEMaterialParams(U8 index);

	void copyTEs(const LLPrimitive* primitive);
	S32 packTEField(U8* cur_ptr, U8* data_ptr, U8 data_size,
					U8 last_face_index, EMsgVariableType type) const;
	void packTEMessage(LLMessageSystem* mesgsys) const;
	void packTEMessage(LLDataPacker& dp) const;
	S32 unpackTEMessage(LLMessageSystem* mesgsys, char const* block_name,
						S32 block_num); // Variable num of blocks
	S32 unpackTEMessage(LLDataPacker& dp);
	S32 parseTEMessage(LLMessageSystem* mesgsys, char const* block_name,
					   S32 block_num, LLTEContents& tec);
	S32 applyParsedTEMessage(LLTEContents& tec);

	LL_INLINE void setAngularVelocity(const LLVector3& avel)
	{
		mAngularVelocity = avel;
	}

	LL_INLINE void setAngularVelocity(F32 x, F32 y, F32 z)
	{
		mAngularVelocity.set(x, y, z);
	}

	LL_INLINE void setVelocity(const LLVector3& vel)	{ mVelocity = vel; }

	LL_INLINE void setVelocity(F32 x, F32 y, F32 z)
	{
		mVelocity.set(x, y, z);
	}

	LL_INLINE void setVelocityX(F32 x)					{ mVelocity.mV[VX] = x; }
	LL_INLINE void setVelocityY(F32 y)					{ mVelocity.mV[VY] = y; }
	LL_INLINE void setVelocityZ(F32 z)					{ mVelocity.mV[VZ] = z; }
	LL_INLINE void addVelocity(const LLVector3& vel)	{ mVelocity += vel; }

	LL_INLINE void setAcceleration(const LLVector3& a)	{ mAcceleration = a; }

	LL_INLINE void setAcceleration(F32 x, F32 y, F32 z)
	{
		mAcceleration.set(x, y, z);
	}

	LL_INLINE LLPCode getPCode() const					{ return mPrimitiveCode; }
	LL_INLINE std::string getPCodeString() const		{ return pCodeToString(mPrimitiveCode); }

	LL_INLINE const LLVector3& getAngularVelocity() const
	{
		return mAngularVelocity;
	}

	LL_INLINE const LLVector3& getVelocity() const		{ return mVelocity; }
	LL_INLINE const LLVector3& getAcceleration() const	{ return mAcceleration; }
	LL_INLINE U8 getNumTEs() const						{ return mTextureList.size(); }
	LL_INLINE U8 getExpectedNumTEs() const;

	LL_INLINE U8 getMaterial() const					{ return mMaterial; }

	U8 getVolumeType();

	// Clears existing textures; copies the contents of other_list into
	// mEntryList
	void copyTextureList(const LLPrimTextureList& other_list);

	// Clears existing textures; takes the contents of other_list and clears it
	void takeTextureList(LLPrimTextureList& other_list);

	LL_INLINE bool hasBumpmap() const					{ return mNumBumpmapTEs > 0; }

	LL_INLINE void setFlags(U32 flags)					{ mMiscFlags = flags; }
	LL_INLINE void addFlags(U32 flags)					{ mMiscFlags |= flags; }
	LL_INLINE void removeFlags(U32 flags)				{ mMiscFlags &= ~flags; }
	LL_INLINE U32 getFlags() const						{ return mMiscFlags; }

	static std::string pCodeToString(LLPCode pcode);
	static bool getTESTAxes(U8 face, U32* s_axis, U32* t_axis);

	LL_INLINE static bool isPrimitive(LLPCode pcode)
	{
		LLPCode base_type = pcode & LL_PCODE_BASE_MASK;
		return base_type && base_type < LL_PCODE_APP;
	}

	LL_INLINE static bool isApp(LLPCode pcode)
	{
		LLPCode base_type = pcode & LL_PCODE_BASE_MASK;
		return base_type == LL_PCODE_APP;
	}

protected:
	void setPCode(LLPCode p_code);

private:
	void updateNumBumpmap(U8 index, U8 bump);

protected:
	LLPointer<LLVolume> mVolumep;
	LLVector3			mVelocity;			// Moving speed
	LLVector3			mAcceleration;		// Constant acceleration
	LLVector3			mAngularVelocity;	// Angular velocity
	LLPrimTextureList	mTextureList;		// List of textures data
	U32 				mMiscFlags;			// Home for misc bools
	LLPCode				mPrimitiveCode;		// Primitive code
	U8					mMaterial;			// Material code
	U8					mNumTEs;			// Number of faces on the primitve
	U8                  mNumBumpmapTEs;     // Number of bumpmap TEs.
};

#endif
