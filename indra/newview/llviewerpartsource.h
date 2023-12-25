/**
 * @file llviewerpartsource.h
 * @brief LLViewerPartSource class header file
 *
 * $LicenseInfo:firstyear=2003&license=viewergpl$
 *
 * Copyright (c) 2003-2009, Linden Research, Inc.
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

#ifndef LL_LLVIEWERPARTSOURCE_H
#define LL_LLVIEWERPARTSOURCE_H

#include "llpartdata.h"
#include "llpointer.h"
#include "llquaternion.h"
#include "llrefcount.h"
#include "llvector3.h"

#include "llmutelist.h"
#include "llviewertexture.h"
#include "llvoavatar.h"

class LLViewerObject;
class LLViewerPart;

// A particle source, subclassed to generate particles with different behaviors
class LLViewerPartSource : public LLRefCount
{
public:
	enum
	{
		LL_PART_SOURCE_NULL,
		LL_PART_SOURCE_SCRIPT,
		LL_PART_SOURCE_SPIRAL,
		LL_PART_SOURCE_BEAM,
		LL_PART_SOURCE_CHAT
	};

	LLViewerPartSource(U32 type);
	virtual ~LLViewerPartSource()						{}

	virtual void update(F32 dt) = 0;

	virtual void setDead();
	LL_INLINE bool isDead() const						{ return mIsDead; }
	LL_INLINE void setSuspended(bool state)				{ mIsSuspended = state; }
	LL_INLINE bool isSuspended() const					{ return mIsSuspended; }
	LL_INLINE U32 getType() const						{ return mType; }

	LL_INLINE void setOwnerUUID(const LLUUID& owner_id)	{ mOwnerUUID = owner_id; }
	LL_INLINE const LLUUID& getOwnerUUID() const		{ return mOwnerUUID; }
	LL_INLINE U32 getID() const							{ return mID; }
	LL_INLINE const LLUUID& getImageUUID() const;
	LL_INLINE void setStart()							{ mDelay = 0; }

	LL_INLINE LLViewerTexture* getImage() const			{ return mImagep; }

	LL_INLINE void incPartUpdates()						{ ++mPartUpdates; }
	void incPartCount();
	U64 getAveragePartCount();

	static void updatePart(LLViewerPart& part, F32 dt);

public:
	LLPointer<LLViewerObject>	mSourceObjectp;

	// Location of the particle source:
	LLVector3					mPosAgent;
	// Location of the target position:
	LLVector3					mTargetPosAgent;
	LLVector3					mLastUpdatePosAgent;
	// Distance from the camera.
	F32							mDistFromCamera;

	U32							mID;

	// Last particle emitted (for making particle ribbons)
	LLViewerPart*				mLastPart;

protected:
	LLPointer<LLVOAvatar>		mOwnerAvatarp;
	LLPointer<LLViewerTexture>	mImagep;

	LLUUID						mOwnerUUID;

	// Particle information
	U64							mPartCount;
	U64							mPartUpdates;
	U32							mPartFlags;	// Flags for the particle
	U32         				mDelay;		// Delay to start particles

	U32							mType;
	F32							mLastUpdateTime;
	F32							mLastPartTime;

	bool						mIsDead;
	bool						mIsSuspended;
};

// LLViewerPartSourceScript
// Particle source that handles the "generic" script-drive particle source
// attached to objects
class LLViewerPartSourceScript final : public LLViewerPartSource,
									   public LLMuteListObserver
{
protected:
	LOG_CLASS(LLViewerPartSourceScript);

public:
	LLViewerPartSourceScript(LLViewerObject* source_objp);
	~LLViewerPartSourceScript() override;

	void update(F32 dt) override;

	void setDead() override;

	// LLMuteListObserver interface
	void onChange() override;

	// Returns a new particle source to attach to an object...
	static LLPointer<LLViewerPartSourceScript> unpackPSS(LLViewerObject* source_objp,
														 LLPointer<LLViewerPartSourceScript> pssp,
														 S32 block_num);
	static LLPointer<LLViewerPartSourceScript> unpackPSS(LLViewerObject* source_objp,
														 LLPointer<LLViewerPartSourceScript> pssp,
														 LLDataPacker& dp,
														 bool legacy);
	static LLPointer<LLViewerPartSourceScript> createPSS(LLViewerObject* source_objp,
														 const LLPartSysData& part_params);

	LL_INLINE void setImage(LLViewerTexture* imagep)	{ mImagep = imagep; }

	LL_INLINE void setTargetObject(LLViewerObject* obj)	{ mTargetObjectp = obj; }

public:
	LLPartSysData				mPartSysData;

protected:
	// Target object for the particle source
	LLPointer<LLViewerObject>	mTargetObjectp;
	// Current rotation for particle source
	LLQuaternion				mRotation;
};

// Particle source for spiral effect (customize avatar, mostly)
class LLViewerPartSourceSpiral final : public LLViewerPartSource
{
public:
	LLViewerPartSourceSpiral(const LLVector3& pos);

	void setDead() override;

	void update(F32 dt) override;

	LL_INLINE void setSourceObject(LLViewerObject* obj)	{ mSourceObjectp = obj; }
	LL_INLINE void setColor(const LLColor4& color)		{ mColor = color; }

	static void updatePart(LLViewerPart& part, F32 dt);

public:
	LLColor4	mColor;

protected:
	LLVector3d	mLKGSourcePosGlobal;
};

// Particle source for tractor (editing) beam
class LLViewerPartSourceBeam final : public LLViewerPartSource
{
public:
	LLViewerPartSourceBeam();

	void setDead() override;

	void update(F32 dt) override;

	LL_INLINE void setSourceObject(LLViewerObject* obj)	{ mSourceObjectp = obj; }
	LL_INLINE void setTargetObject(LLViewerObject* obj)	{ mTargetObjectp = obj; }
	LL_INLINE void setColor(const LLColor4& color)		{ mColor = color; }

	static void updatePart(LLViewerPart& part, F32 dt);

protected:
	LL_INLINE ~LLViewerPartSourceBeam()					{}

public:
	LLPointer<LLViewerObject>	mTargetObjectp;
	LLVector3d					mLKGTargetPosGlobal;
	LLColor4					mColor;
};

// Particle source for chat effect
class LLViewerPartSourceChat final : public LLViewerPartSource
{
public:
	LLViewerPartSourceChat(const LLVector3& pos);

	void setDead() override;

	void update(F32 dt) override;

	LL_INLINE void setSourceObject(LLViewerObject* obj)	{ mSourceObjectp = obj; }
	LL_INLINE void setColor(const LLColor4& color)		{ mColor = color; }

	static void updatePart(LLViewerPart& part, F32 dt);

public:
	LLColor4	mColor;

protected:
	LLVector3d 	mLKGSourcePosGlobal;
};

#endif // LL_LLVIEWERPARTSOURCE_H
