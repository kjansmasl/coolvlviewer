/**
 * @file llvoicevisualizer.cpp
 * @brief Draws in-world speaking indicators.
 *
 * $LicenseInfo:firstyear=2000&license=viewergpl$
 *
 * Copyright (c) 2000-2009, JJ Ventrella & Linden Research, Inc.
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

#include "llviewerprecompiledheaders.h"

#include "llvoicevisualizer.h"

#include "llgl.h"
#include "llimagegl.h"
#include "llrender.h"

#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerobject.h"
#include "llviewertexturelist.h"
#include "llvoiceclient.h"

// Brent's wave image: 29de489d-0491-fb00-7dab-f9e686d31e83

// Sound symbol constants

// How many meters vertically above the av's head the voice symbol will appear
constexpr F32 HEIGHT_ABOVE_HEAD = 0.3f;
// Value above which speaking amplitude causes the voice symbol to turn red
constexpr F32 RED_THRESHOLD = OVERDRIVEN_POWER_LEVEL;
// Value above which speaking amplitude causes the voice symbol to turn green
constexpr F32 GREEN_THRESHOLD = 0.2f;
// How many seconds it takes for a pair of waves to fade away
constexpr F32 FADE_OUT_DURATION = 0.4f;
// How many seconds it takes for the waves to expand to twice their orig. size
constexpr F32 EXPANSION_RATE = 1.f;
// Maximum size scale to which the waves can expand before popping back to 1.0
constexpr F32 EXPANSION_MAX = 1.5f;
constexpr F32 WAVE_WIDTH_SCALE = 0.03f;		// Base width of the waves
constexpr F32 WAVE_HEIGHT_SCALE = 0.02f;	// Base height of the waves
// Gray level of the voice indicator when quiet (below green threshold)
constexpr F32 BASE_BRIGHTNESS = 0.7f;
constexpr F32 DOT_SIZE = 0.05f;		// Size of the dot billboard texture
constexpr F32 DOT_OPACITY = 0.7f;	// How opaque the dot is
// Scalar applied to consecutive waves as a function of speaking amplitude
constexpr F32 WAVE_MOTION_RATE = 1.5f;

// Gesticulation constants

constexpr F32 DEFAULT_MINIMUM_GESTICULATION_AMPLITUDE = 0.2f;
constexpr F32 DEFAULT_MAXIMUM_GESTICULATION_AMPLITUDE = 1.f;

// Other constants

// Handles parameter updates
static bool handleVoiceVisualizerPrefsChanged(const LLSD& newvalue)
{
	// Note: Ignore the specific event value, we look up the ones we want
	LLVoiceVisualizer::setPreferences();
	return true;
}

// Statics
bool LLVoiceVisualizer::sPrefsInitialized = false;
bool LLVoiceVisualizer::sLipSyncEnabled = false;
F32* LLVoiceVisualizer::sOoh = NULL;
F32* LLVoiceVisualizer::sAah = NULL;
U32	 LLVoiceVisualizer::sOohs = 0;
U32	 LLVoiceVisualizer::sAahs = 0;
F32	 LLVoiceVisualizer::sOohAahRate = 0.f;
F32* LLVoiceVisualizer::sOohPowerTransfer = NULL;
U32	 LLVoiceVisualizer::sOohPowerTransfers = 0;
F32	 LLVoiceVisualizer::sOohPowerTransfersf = 0.f;
F32* LLVoiceVisualizer::sAahPowerTransfer = NULL;
U32	 LLVoiceVisualizer::sAahPowerTransfers = 0;
F32	 LLVoiceVisualizer::sAahPowerTransfersf = 0.f;

LLVoiceVisualizer::LLVoiceVisualizer(U8 type)
:	LLHUDEffect(type),
	mVoiceEnabled(false),
	mSpeakingAmplitude(0.f),
	mCurrentlySpeaking(false),
	mMinGesticulationAmplitude(DEFAULT_MINIMUM_GESTICULATION_AMPLITUDE),
	mMaxGesticulationAmplitude(DEFAULT_MAXIMUM_GESTICULATION_AMPLITUDE)
{
	mStartTime = mCurrentTime = mPreviousTime = mTimer.getTotalSeconds();
	mTimer.reset();

	static const char* sound_level_img[] =
	{
		"041ee5a0-cb6a-9ac5-6e49-41e9320507d5.j2c",
		"29de489d-0491-fb00-7dab-f9e686d31e83.j2c",
		"29de489d-0491-fb00-7dab-f9e686d31e83.j2c",
		"29de489d-0491-fb00-7dab-f9e686d31e83.j2c",
		"29de489d-0491-fb00-7dab-f9e686d31e83.j2c",
		"29de489d-0491-fb00-7dab-f9e686d31e83.j2c",
		"29de489d-0491-fb00-7dab-f9e686d31e83.j2c"
	};

	mSoundSymbol.mActive = true;
	for (U32 i = 0; i < NUM_VOICE_WAVES; ++i)
	{
		mSoundSymbol.mWaveFadeOutStartTime[i] = mCurrentTime;
		mSoundSymbol.mTexture[i] =
			LLViewerTextureManager::getFetchedTextureFromFile(sound_level_img[i],
															  MIPMAP_NO);
		mSoundSymbol.mWaveActive[i] = false;
		mSoundSymbol.mWaveOpacity[i] = mSoundSymbol.mWaveExpansion[i] = 1.f;
	}

	mSoundSymbol.mTexture[0]->setFilteringOption(LLTexUnit::TFO_ANISOTROPIC);

	// The first instance loads the initial state from prefs.
	if (sPrefsInitialized)
	{
		return;
	}
	sPrefsInitialized = true;

	setPreferences();

	// Set up our listener to get updates on all prefs values we care about.
	gSavedSettings.getControl("LipSyncEnabled")->getSignal()->connect(boost::bind(&handleVoiceVisualizerPrefsChanged, _2));
	gSavedSettings.getControl("LipSyncOohAahRate")->getSignal()->connect(boost::bind(&handleVoiceVisualizerPrefsChanged, _2));
	gSavedSettings.getControl("LipSyncOoh")->getSignal()->connect(boost::bind(&handleVoiceVisualizerPrefsChanged, _2));
	gSavedSettings.getControl("LipSyncAah")->getSignal()->connect(boost::bind(&handleVoiceVisualizerPrefsChanged, _2));
	gSavedSettings.getControl("LipSyncOohPowerTransfer")->getSignal()->connect(boost::bind(&handleVoiceVisualizerPrefsChanged, _2));
	gSavedSettings.getControl("LipSyncAahPowerTransfer")->getSignal()->connect(boost::bind(&handleVoiceVisualizerPrefsChanged, _2));
}

void LLVoiceVisualizer::setStartSpeaking()
{
	mStartTime = mTimer.getTotalSeconds();
	mCurrentlySpeaking = mSoundSymbol.mActive = true;
}

void LLVoiceVisualizer::setStopSpeaking()
{
	mCurrentlySpeaking = false;
	mSpeakingAmplitude = 0.f;
}

void LLVoiceVisualizer::setPreferences()
{
	sLipSyncEnabled = gSavedSettings.getBool("LipSyncEnabled");
	sOohAahRate = gSavedSettings.getF32("LipSyncOohAahRate");

	std::string str = gSavedSettings.getString("LipSyncOoh");
	lipStringToF32s(str, sOoh, sOohs);

	str = gSavedSettings.getString("LipSyncAah");
	lipStringToF32s(str, sAah, sAahs);

	str = gSavedSettings.getString("LipSyncOohPowerTransfer");
	lipStringToF32s(str, sOohPowerTransfer, sOohPowerTransfers);
	sOohPowerTransfersf = (F32)sOohPowerTransfers;

	str = gSavedSettings.getString("LipSyncAahPowerTransfer");
	lipStringToF32s(str, sAahPowerTransfer, sAahPowerTransfers);
	sAahPowerTransfersf = (F32)sAahPowerTransfers;
}

// Converts a string of digits to an array of floats. The result for each digit
// is the value of the digit multiplied by 0.11
void LLVoiceVisualizer::lipStringToF32s(std::string& in_string,
										F32*& out, U32& count)
{
	delete[] out;	// Get rid of the current array

	count = in_string.length();
	if (!count)
	{
		// We do not like zero length arrays
		count = 1;
		out = new F32[1];
		out[0] = 0.f;
		return;
	}

	out = new F32[count];
	for (U32 i = 0; i < count; ++i)
	{
		// We convert the characters 0 to 9 to their numeric value. Anything
		// else we take the low order four bits with a ceiling of 9
	    U8 digit = in_string[i];
		U8 four_bits = digit % 16;
		if (four_bits > 9)
		{
			four_bits = 9;
		}
		out[i] = 0.11f * (F32)four_bits;
	}
}

// Finds the amount to blend the ooh and aah mouth morphs
void LLVoiceVisualizer::lipSyncOohAah(F32& ooh, F32& aah)
{
	if (sLipSyncEnabled && mCurrentlySpeaking)
	{
		U32 transfer_index = U32(sOohPowerTransfersf * mSpeakingAmplitude);
		if (transfer_index >= sOohPowerTransfers)
		{
		   transfer_index = sOohPowerTransfers - 1;
		}
		F32 transfer_ooh = sOohPowerTransfer[transfer_index];

		transfer_index = U32(sAahPowerTransfersf * mSpeakingAmplitude);
		if (transfer_index >= sAahPowerTransfers)
		{
		   transfer_index = sAahPowerTransfers - 1;
		}
		F32 transfer_aah = sAahPowerTransfer[transfer_index];

		F64 current_time   = mTimer.getTotalSeconds();
		F64 elapsed_time   = current_time - mStartTime;
		U32 elapsed_frames = U32(elapsed_time * sOohAahRate);
		U32 elapsed_oohs   = elapsed_frames % sOohs;
		U32 elapsed_aahs   = elapsed_frames % sAahs;

		ooh = transfer_ooh * sOoh[elapsed_oohs];
		aah = transfer_aah * sAah[elapsed_aahs];
	}
	else
	{
		ooh = aah = 0.f;
	}
}

//virtual
void LLVoiceVisualizer::render()
{
	if (!mVoiceEnabled || !mSoundSymbol.mActive)
	{
		return;
	}

	mPreviousTime = mCurrentTime;
	mCurrentTime = mTimer.getTotalSeconds();

	// Set the sound symbol position over the source (avatar's head)
	mSoundSymbol.mPosition = mVoiceSourceWorldPosition +
							 LLVector3::z_axis * HEIGHT_ABOVE_HEAD;

	// Some GL states
	LLGLSPipelineAlpha alpha_blend;
	LLGLDepthTest depth(GL_TRUE, GL_FALSE);

	// Create coordinates of the geometry for the dot
	LLVector3 l	= gViewerCamera.getLeftAxis() * DOT_SIZE;
	LLVector3 u	= gViewerCamera.getUpAxis()   * DOT_SIZE;

	LLVector3 bottom_left = mSoundSymbol.mPosition + l - u;
	LLVector3 bottom_right = mSoundSymbol.mPosition - l - u;
	LLVector3 top_left = mSoundSymbol.mPosition + l + u;
	LLVector3 top_right = mSoundSymbol.mPosition - l + u;

	// Bind texture 0 (the dot)
	LLTexUnit* unit0 = gGL.getTexUnit(0);
	unit0->bind(mSoundSymbol.mTexture[0]);

	// Now render the dot

	gGL.color4fv(LLColor4(1.f, 1.f, 1.f, DOT_OPACITY).mV);

	gGL.begin(LLRender::TRIANGLE_STRIP);
	gGL.texCoord2i(0, 0);
	gGL.vertex3fv(bottom_left.mV);
	gGL.texCoord2i(1, 0);
	gGL.vertex3fv(bottom_right.mV);
	gGL.texCoord2i(0, 1);
	gGL.vertex3fv(top_left.mV);
	gGL.end();

	gGL.begin(LLRender::TRIANGLE_STRIP);
	gGL.texCoord2i(1, 0);
	gGL.vertex3fv(bottom_right.mV);
	gGL.texCoord2i(1, 1);
	gGL.vertex3fv(top_right.mV);
	gGL.texCoord2i(0, 1);
	gGL.vertex3fv(top_left.mV);
	gGL.end();

	// If currently speaking, trigger waves (1 through 6) based on speaking
	// amplitude
	if (mCurrentlySpeaking)
	{
		F32 min = 0.2f;
		F32 max = 0.7f;
		F32 fraction = (mSpeakingAmplitude - min) / (max - min);

		// In case mSpeakingAmplitude > max....
		if (fraction > 1.f)
		{
			fraction = 1.f;
		}

		S32 level = 1 + (S32)(fraction * (NUM_VOICE_WAVES - 2));

		for (S32 i = 0; i < level + 1; ++i)
		{
			mSoundSymbol.mWaveActive[i] = true;
			mSoundSymbol.mWaveOpacity[i] = 1.f;
			mSoundSymbol.mWaveFadeOutStartTime[i] = mCurrentTime;
		}
	}

	// Determine color
	F32 red = 0.f;
	F32 green = 0.f;
	F32 blue = 0.f;
	if (mSpeakingAmplitude < RED_THRESHOLD)
	{
		if (mSpeakingAmplitude < GREEN_THRESHOLD)
		{
			red	= green = blue = BASE_BRIGHTNESS;
		}
		else
		{
			// Fade from gray to bright green
			F32 fraction = (mSpeakingAmplitude - GREEN_THRESHOLD) /
						   (1.f - GREEN_THRESHOLD);
			red = BASE_BRIGHTNESS - fraction * BASE_BRIGHTNESS;
			green = BASE_BRIGHTNESS + fraction * (1.f - BASE_BRIGHTNESS);
			blue = BASE_BRIGHTNESS - fraction * BASE_BRIGHTNESS;
		}
	}
	else
	{
		// Redish
		red = 1.f;
		green = blue = 0.2f;
	}

	for (U32 i = 0; i < NUM_VOICE_WAVES; ++i)
	{
		if (!mSoundSymbol.mWaveActive[i])
		{
			continue;
		}

		F32 fade_out_frac = (F32)(mCurrentTime -
								  mSoundSymbol.mWaveFadeOutStartTime[i]) /
							FADE_OUT_DURATION;

		mSoundSymbol.mWaveOpacity[i] = 1.f - fade_out_frac;

		if (mSoundSymbol.mWaveOpacity[i] < 0.f)
		{
			mSoundSymbol.mWaveFadeOutStartTime[i] = mCurrentTime;
			mSoundSymbol.mWaveOpacity[i] = 0.f;
			mSoundSymbol.mWaveActive[i] = false;
		}

		// This is where we calculate the expansion of the waves, that is, the
		// rate at which they are scaled greater than 1.0 so that they grow
		// over time.
		F32 time_slice = F32(mCurrentTime - mPreviousTime);
		F32 wave_speed = mSpeakingAmplitude * WAVE_MOTION_RATE;
		mSoundSymbol.mWaveExpansion[i] *= 1.f + EXPANSION_RATE * time_slice *
										  wave_speed;

		if (mSoundSymbol.mWaveExpansion[i] > EXPANSION_MAX)
		{
			mSoundSymbol.mWaveExpansion[i] = 1.f;
		}

		// Create geometry for the wave billboard textures

		F32 width = i * WAVE_WIDTH_SCALE * mSoundSymbol.mWaveExpansion[i];
		F32 height = i * WAVE_HEIGHT_SCALE * mSoundSymbol.mWaveExpansion[i];

		LLVector3 l	= gViewerCamera.getLeftAxis() * width;
		LLVector3 u	= gViewerCamera.getUpAxis() * height;

		LLVector3 bottom_left = mSoundSymbol.mPosition + l - u;
		LLVector3 bottom_right = mSoundSymbol.mPosition - l - u;
		LLVector3 top_left = mSoundSymbol.mPosition + l + u;
		LLVector3 top_right = mSoundSymbol.mPosition - l + u;

		gGL.color4fv(LLColor4(red, green, blue,
							  mSoundSymbol.mWaveOpacity[i]).mV);
		unit0->bind(mSoundSymbol.mTexture[i]);

		// Now, render the mofo

		gGL.begin(LLRender::TRIANGLE_STRIP);
		gGL.texCoord2i(0, 0);
		gGL.vertex3fv(bottom_left.mV);
		gGL.texCoord2i(1, 0);
		gGL.vertex3fv(bottom_right.mV);
		gGL.texCoord2i(0, 1);
		gGL.vertex3fv(top_left.mV);
		gGL.end();

		gGL.begin(LLRender::TRIANGLE_STRIP);
		gGL.texCoord2i(1, 0);
		gGL.vertex3fv(bottom_right.mV);
		gGL.texCoord2i(1, 1);
		gGL.vertex3fv(top_right.mV);
		gGL.texCoord2i(0, 1);
		gGL.vertex3fv(top_left.mV);
		gGL.end();
	}
}

VoiceGesticulationLevel LLVoiceVisualizer::getCurrentGesticulationLevel()
{
	VoiceGesticulationLevel level = VOICE_GESTICULATION_LEVEL_OFF;

	// Within the range of gesticulation amplitudes, the sound signal is split
	// into three equal amplitude regimes, each specifying one of three
	// gesticulation levels.
	F32 range = mMaxGesticulationAmplitude - mMinGesticulationAmplitude;

	if (mSpeakingAmplitude > mMinGesticulationAmplitude + range * 0.5f)
	{
		level = VOICE_GESTICULATION_LEVEL_HIGH;
	}
	else if (mSpeakingAmplitude > mMinGesticulationAmplitude + range * 0.25f)
	{
		level = VOICE_GESTICULATION_LEVEL_MEDIUM;
	}
	else if (mSpeakingAmplitude > mMinGesticulationAmplitude)
	{
		level = VOICE_GESTICULATION_LEVEL_LOW;
	}

	return level;
}

//virtual
void LLVoiceVisualizer::packData(LLMessageSystem *mesgsys)
{
	// Pack the default data
	LLHUDEffect::packData(mesgsys);

#if 0	// *TODO: pack the relevant data for voice effects. We will come up
		// with some cool configurations... TBD
	U8 packed_data[41];
	mesgsys->addBinaryDataFast(_PREHASH_TypeData, packed_data, 41);
#endif
	U8 packed_data = 0;
	mesgsys->addBinaryDataFast(_PREHASH_TypeData, &packed_data, 1);
}

//virtual
void LLVoiceVisualizer::unpackData(LLMessageSystem *mesgsys, S32 blocknum)
{
#if 0	// *TODO: find the speaker, unpack binary data, set the properties of
		// this effect
	LLHUDEffect::unpackData(mesgsys, blocknum);
	S32 size = mesgsys->getSizeFast(_PREHASH_Effect, blocknum,
									_PREHASH_TypeData);
	if (size != 1)
	{
		llwarns << "Voice effect with bad size " << size << llendl;
		return;
	}
	mesgsys->getBinaryDataFast(_PREHASH_Effect, _PREHASH_TypeData, packed_data,
							   1, blocknum);
#endif
}

//virtual
void LLVoiceVisualizer::markDead()
{
	mCurrentlySpeaking = mVoiceEnabled = mSoundSymbol.mActive = false;
	LLHUDEffect::markDead();
}
