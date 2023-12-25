/**
 * @file lllistener.h
 * @brief Description of LISTENER base class abstracting the audio support.
 *
 * $LicenseInfo:firstyear=2000&license=viewergpl$
 *
 * Copyright (c) 2000-2009, Linden Research, Inc.
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

#ifndef LL_LISTENER_H
#define LL_LISTENER_H

#include "llvector3.h"

class LLListener
{
public:
	LLListener();
	virtual ~LLListener() = default;

	virtual void set(const LLVector3& pos, const LLVector3& vel,
					 const LLVector3& up, const LLVector3& at);

	LL_INLINE virtual void setPosition(const LLVector3& pos)
	{
		mPosition = pos;
	}

	LL_INLINE virtual void setVelocity(const LLVector3& vel)
	{
		mVelocity = vel;
	}

	LL_INLINE virtual void orient(const LLVector3& up, const LLVector3& at)
	{
		mListenUp = up;
		mListenAt = at;
	}

	LL_INLINE virtual void translate(const LLVector3& offset)
	{
		mPosition += offset;
	}

	LL_INLINE virtual void setDopplerFactor(F32 factor)
	{
		mDopplerFactor = factor;
	}

	LL_INLINE virtual void setRolloffFactor(F32 factor)
	{
		mRolloffFactor = factor;
	}

	LL_INLINE virtual F32 getDopplerFactor()		{ return mDopplerFactor; }
	LL_INLINE virtual F32 getRolloffFactor()		{ return mRolloffFactor; }

	// No need for virtual methods here.
	LL_INLINE LLVector3 getPosition()				{ return mPosition; }
	LL_INLINE LLVector3 getAt()						{ return mListenAt; }
	LL_INLINE LLVector3 getUp()						{ return mListenUp; }

	LL_INLINE virtual void commitDeferredChanges()	{}

protected:
	LLVector3	mPosition;
	LLVector3	mVelocity;
	LLVector3	mListenAt;
	LLVector3	mListenUp;
	F32			mDopplerFactor;
	F32			mRolloffFactor;
};

#endif	// LL_LISTENER_H

