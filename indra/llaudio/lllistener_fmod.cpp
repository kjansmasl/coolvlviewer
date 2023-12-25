/**
 * @file lllistener_fmod.cpp
 * @brief implementation of LISTENER class abstracting the audio support
 * as a FMOD implementation
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

#include "linden_common.h"

#include "fmod.hpp"

#include "lllistener_fmod.h"

#include "llaudioengine.h"

LLListener_FMOD::LLListener_FMOD(FMOD::System* system)
:	LLListener(),
	mSystem(system)
{
}

//virtual
void LLListener_FMOD::translate(const LLVector3& offset)
{
	if (mSystem)
	{
		LLListener::translate(offset);
		mSystem->set3DListenerAttributes(0, (FMOD_VECTOR*)mPosition.mV, NULL,
										 (FMOD_VECTOR*)mListenAt.mV,
										 (FMOD_VECTOR*)mListenUp.mV);
	}
}

//virtual
void LLListener_FMOD::setPosition(const LLVector3& pos)
{
	if (mSystem)
	{
		LLListener::setPosition(pos);
		mSystem->set3DListenerAttributes(0, (FMOD_VECTOR*)mPosition.mV, NULL,
										 (FMOD_VECTOR*)mListenAt.mV,
										 (FMOD_VECTOR*)mListenUp.mV);
	}
}

//virtual
void LLListener_FMOD::setVelocity(const LLVector3& vel)
{
	if (mSystem)
	{
		LLListener::setVelocity(vel);
		mSystem->set3DListenerAttributes(0, NULL, (FMOD_VECTOR*)mVelocity.mV,
										 (FMOD_VECTOR*)mListenAt.mV,
										 (FMOD_VECTOR*)mListenUp.mV);
	}
}

//virtual
void LLListener_FMOD::orient(const LLVector3& up, const LLVector3& at)
{
	if (mSystem)
	{
		LLListener::orient(up, at);
		mSystem->set3DListenerAttributes(0, NULL, NULL, (FMOD_VECTOR*)at.mV,
										 (FMOD_VECTOR*)up.mV);
	}
}

//virtual
void LLListener_FMOD::setRolloffFactor(F32 factor)
{
	// An internal FMOD Studio optimization skips 3D updates if there have not
	// been changes to the 3D sound environment. Sadly, a change in rolloff is
	// not accounted for, thus we must touch the listener properties as well.
	// In short: Changing the position ticks a dirtyflag inside FMOD Studio,
	// which makes it not skip 3D processing next update call.
	if (mSystem)
	{
		if (mRolloffFactor != factor)
		{
			LLVector3 pos = mPosition;
			pos.mV[VZ] -= 0.1f;
			mSystem->set3DListenerAttributes(0, (FMOD_VECTOR*)pos.mV, NULL,
											 NULL, NULL);
			mSystem->set3DListenerAttributes(0, (FMOD_VECTOR*)mPosition.mV,
											 NULL, NULL, NULL);
		}
		mRolloffFactor = factor;
		mSystem->set3DSettings(mDopplerFactor, 1.f, mRolloffFactor);
	}
}

//virtual
void LLListener_FMOD::setDopplerFactor(F32 factor)
{
	if (mSystem)
	{
		mDopplerFactor = factor;
		mSystem->set3DSettings(mDopplerFactor, 1.f, mRolloffFactor);
	}
}

//virtual
void LLListener_FMOD::commitDeferredChanges()
{
	if (mSystem)
	{
		mSystem->update();
	}
}
