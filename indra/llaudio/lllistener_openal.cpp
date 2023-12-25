/**
 * @file audioengine_openal.cpp
 * @brief implementation of audio engine using OpenAL
 * support as a OpenAL 3D implementation
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
#include "llaudioengine.h"

#include "lllistener_openal.h"

LLListener_OpenAL::LLListener_OpenAL()
:	LLListener()
{
}

//virtual
void LLListener_OpenAL::setDopplerFactor(F32 factor)
{
	mDopplerFactor = factor;
	alDopplerFactor(factor);
}

//virtual
F32 LLListener_OpenAL::getDopplerFactor()
{
	mDopplerFactor = (F32)alGetFloat(AL_DOPPLER_FACTOR);
	return mDopplerFactor;
}

//virtual
void LLListener_OpenAL::commitDeferredChanges()
{
	ALfloat orientation[6];
	orientation[0] = mListenAt.mV[0];
	orientation[1] = mListenAt.mV[1];
	orientation[2] = mListenAt.mV[2];
	orientation[3] = mListenUp.mV[0];
	orientation[4] = mListenUp.mV[1];
	orientation[5] = mListenUp.mV[2];

	ALfloat velocity[3];
	velocity[0] = mVelocity.mV[0];
	velocity[1] = mVelocity.mV[1];
	velocity[2] = mVelocity.mV[2];

	alListenerfv(AL_ORIENTATION, orientation);
	alListenerfv(AL_POSITION, mPosition.mV);
	alListenerfv(AL_VELOCITY, velocity);
}
