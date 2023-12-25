/**
 * @file llvoicevisualizer.h
 * @brief Draws in-world speaking indicators.
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

//-----------------------------------------------------------------------------
// VOICE VISUALIZER (latest update to this info: Jan 18, 2007)
// author: JJ Ventrella, Linden Lab
//
// The Voice Visualizer is responsible for taking realtime signals from actual
// users speaking and visualizing this speech in two forms:
//
// 1.- as a dynamic sound symbol (also referred to as the "voice indicator"
//     that appears over the avatar's head);
// 2.- as gesticulation events that are used to trigger avatr gestures.
//
// The input for the voice visualizer is a continual stream of voice amplitudes.
//-----------------------------------------------------------------------------

#ifndef LL_LLVOICEVISUALIZER_H
#define LL_LLVOICEVISUALIZER_H

#include "llhudeffect.h"

class LLViewerTexture;

//-----------------------------------------------------------------------------
// The values of voice gesticulation represent energy levels for avatar
// animation, based on amplitude surge events parsed from the voice signal.
// These are made available so that the appropriate kind of avatar animation
// can be triggered, and thereby simulate the physical motion effects of
// speech. It is recommended that multiple body parts be animated as well as
// lips, such as head, shoulders, and hands, with large gestures used when the
// energy level is high.
//-----------------------------------------------------------------------------
enum VoiceGesticulationLevel
{
	VOICE_GESTICULATION_LEVEL_OFF = -1,
	VOICE_GESTICULATION_LEVEL_LOW = 0,
	VOICE_GESTICULATION_LEVEL_MEDIUM,
	VOICE_GESTICULATION_LEVEL_HIGH,
	NUM_VOICE_GESTICULATION_LEVELS
};

constexpr U32 NUM_VOICE_WAVES = 7;

class LLVoiceVisualizer final : public LLHUDEffect
{
	friend class LLHUDObject;

protected:
	LOG_CLASS(LLVoiceVisualizer);

public:
	LLVoiceVisualizer(U8 type);

	// Whether or not the user is voice enabled
	LL_INLINE void setVoiceEnabled(bool b)		{ mVoiceEnabled = b; }

	// 'pos' should be the position of the speaking avatar's head
	LL_INLINE void setVoiceSourceWorldPosition(const LLVector3& pos)
	{
		mVoiceSourceWorldPosition = pos;
	}

	// Called when the avatar starts speaking
	void setStartSpeaking();
	// Called when the avatar stops speaking
	void setStopSpeaking();
	// Is the avatar currently speaking ?
	LL_INLINE bool getCurrentlySpeaking()		{ return mCurrentlySpeaking; }

	// The lower range of meaningful amplitude for setting gesticulation level
	LL_INLINE void setMinGesticulationAmplitude(F32 a)
	{
		mMinGesticulationAmplitude = a;
	}

	// The upper range of meaningful amplitude for setting gesticulation level
	LL_INLINE void setMaxGesticulationAmplitude(F32 a)
	{
		mMaxGesticulationAmplitude = a;
	}

	// How loud the avatar is speaking (ranges from 0 to 1)
	LL_INLINE void setSpeakingAmplitude(F32 a)	{ mSpeakingAmplitude = a; }

	// Based on voice amplitude, returns the current "energy level" of the
	// avatar's speech.
	VoiceGesticulationLevel getCurrentGesticulationLevel();

	void lipSyncOohAah(F32& ooh, F32& aah);

	// LLHUDEffect overrides
	void markDead() override;
	void packData(LLMessageSystem* mesgsys) override;
	void unpackData(LLMessageSystem *mesgsys, S32 blocknum) override;
	// LLHUDObject override
	void render() override;

	//-------------------------------------------------------------------------
	// "setMaxGesticulationAmplitude" and "setMinGesticulationAmplitude" allow
	// for the tuning of the gesticulation level detector to be responsive to
	// different kinds of signals. For instance, we may find that the average
	// voice amplitude rarely exceeds 0.7 (in a range from 0 to 1), and
	// therefore we may want to set 0.7 as the max, so we can more easily catch
	// all the variance within that range. Also, we may find that there is
	// often noise below a certain range like 0.1 and so we would want to set
	// 0.1 as the min so as not to accidentally use this as signal.
	//-------------------------------------------------------------------------
	void setMaxGesticulationAmplitude();
	void setMinGesticulationAmplitude();

	static void setPreferences();

	// Converts a string of digits to an array of floats
	static void lipStringToF32s(std::string& in_string, F32*& out, U32& count);

private:
	struct SoundSymbol
	{
		LLPointer<LLViewerTexture>	mTexture[NUM_VOICE_WAVES];
		LLVector3					mPosition;
		F64							mWaveFadeOutStartTime[NUM_VOICE_WAVES];
		F32							mWaveExpansion[NUM_VOICE_WAVES];
		F32							mWaveOpacity[NUM_VOICE_WAVES];
		bool						mWaveActive[NUM_VOICE_WAVES];
		bool						mActive;
	};
	// The sound symbol that appears over the avatar's head
	SoundSymbol		mSoundSymbol;

	// Needed at every step to update the sound symbol
	LLVector3		mVoiceSourceWorldPosition;

	// Current (frame) time in seconds
	LLFrameTimer	mTimer;
	// Time stamp in seconds when speaking started
	F64				mStartTime;
	// Current time in seconds, captured every step
	F64				mCurrentTime;
	// Copy of "current time" from last frame
	F64				mPreviousTime;
	// This should be set as often as possible when the user is speaking
	F32				mSpeakingAmplitude;
	// This is the upper-limit of the envelope of detectable gesticulation leves
	F32				mMaxGesticulationAmplitude;
	// This is the lower-limit of the envelope of detectable gesticulation leves
	F32				mMinGesticulationAmplitude;
	// When false, no rendering happens
	bool			mVoiceEnabled;
	// Is the user currently speaking ?
	bool			mCurrentlySpeaking;

	// The babble loop of amplitudes for the ooh morph
	static F32*		sOoh;
	// The babble loop of amplitudes for the ooh morph
	static F32*		sAah;
	// The number of entries in the ooh loop
	static U32		sOohs;
	// The number of entries in the aah loop
	static U32		sAahs;
	// Frames per second for the babble loop
	static F32		sOohAahRate;
	// The power transfer characteristics for the ooh amplitude
	static F32*		sOohPowerTransfer;
	// The number of entries in the ooh transfer characteristics
	static U32		sOohPowerTransfers;
	// The number of entries in the ooh transfer characteristics as a float
	static F32		sOohPowerTransfersf;
	// The power transfer characteristics for the aah amplitude
	static F32*		sAahPowerTransfer;
	// The number of entries in the aah transfer characteristics
	static U32		sAahPowerTransfers;
	// The number of entries in the aah transfer characteristics as a float
	static F32		sAahPowerTransfersf;
	// 'true' when in babble loop, 'false' when disabled
	static bool		sLipSyncEnabled;
	// The first instance will initialize the static members
	static bool		sPrefsInitialized;
};

#endif // LL_LLVOICEVISUALIZER_H
