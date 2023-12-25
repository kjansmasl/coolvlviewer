/**
 * @file llthrottle.cpp
 * @brief LLThrottle class used for network bandwidth control.
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

#include "llthrottle.h"

#include "lldatapacker.h"
#include "llmath.h"
#include "llmessage.h"

LLThrottle::LLThrottle(F32 rate)
{
	mRate = rate;
	mAvailable = 0.f;
	mLookaheadSecs = 0.25f;
	mLastSendTime = LLMessageSystem::getMessageTimeSeconds(true);
}

void LLThrottle::setRate(F32 rate)
{
	// Need to accumulate available bits when adjusting the rate.
	mAvailable = getAvailable();
	mLastSendTime = LLMessageSystem::getMessageTimeSeconds();
	mRate = rate;
}

F32 LLThrottle::getAvailable()
{
	// Use a temporary bits_available since we do not want to change
	// mBitsAvailable every time
	F32 elapsed_time = (F32)(LLMessageSystem::getMessageTimeSeconds() -
							 mLastSendTime);
	return mAvailable + mRate * elapsed_time;
}

bool LLThrottle::checkOverflow(F32 amount)
{
	bool retval = true;

	F32 lookahead_amount = mRate * mLookaheadSecs;

	// Use a temporary bits_available since we don't want to change
	// mBitsAvailable every time
	F32 elapsed_time = (F32)(LLMessageSystem::getMessageTimeSeconds() -
							 mLastSendTime);
	F32 amount_available = mAvailable + mRate * elapsed_time;

	if (amount_available >= lookahead_amount || amount_available > amount)
	{
		// ...enough space to send this message. Also do if > lookahead so we
		// can use if amount > capped amount.
		retval = false;
	}

	return retval;
}

bool LLThrottle::throttleOverflow(F32 amount)
{
	F32 elapsed_time;
	F32 lookahead_amount;
	bool retval = true;

	lookahead_amount = mRate * mLookaheadSecs;

	F64 mt_sec = LLMessageSystem::getMessageTimeSeconds();
	elapsed_time = (F32)(mt_sec - mLastSendTime);
	mLastSendTime = mt_sec;

	mAvailable += mRate * elapsed_time;

	if (mAvailable >= lookahead_amount)
	{
		// ...channel completely open, so allow send regardless
		// of size.  This allows sends on very low BPS channels.
		mAvailable = lookahead_amount;
		retval = false;
	}
	else if (mAvailable > amount)
	{
		// ...enough space to send this message
		retval = false;
	}

	// We actually already sent the bits.
	mAvailable -= amount;

	// What if bitsavailable goes negative?
	// That's OK, because it means someone is banging on the channel,
	// so we need some time to recover.

	return retval;
}

constexpr F32 THROTTLE_LOOKAHEAD_TIME = 1.f;	// seconds

// Make sure that we do not set above these values, even if the client asks to
// be set higher. Note that these values are replicated on the client side to
// set max bandwidth throttling there, in llviewerthrottle.cpp. These values
// are the sum of the top two tiers of bandwidth there.

F32 gThrottleMaximumBPS[TC_EOF] =
{
	150000.f, // TC_RESEND
	170000.f, // TC_LAND
	34000.f, // TC_WIND
	34000.f, // TC_CLOUD
	446000.f, // TC_TASK
	446000.f, // TC_TEXTURE
	220000.f, // TC_ASSET
};

// Start low until viewer informs us of capability.  Asset and resend get high
// values, since they are not used JUST by the viewer necessarily. This is a
// HACK and should be dealt with more properly on circuit creation.

F32 gThrottleDefaultBPS[TC_EOF] =
{
	100000.f, // TC_RESEND
	4000.f, // TC_LAND
	4000.f, // TC_WIND
	4000.f, // TC_CLOUD
	4000.f, // TC_TASK
	4000.f, // TC_TEXTURE
	100000.f, // TC_ASSET
};

// Do not throttle down lower than this. This potentially wastes 50 kbps, but
// usually would not.
F32 gThrottleMinimumBPS[TC_EOF] =
{
	10000.f,	// TC_RESEND
	10000.f,	// TC_LAND
	 4000.f,	// TC_WIND
	 4000.f,	// TC_CLOUD
	20000.f,	// TC_TASK
	10000.f,	// TC_TEXTURE
	10000.f,	// TC_ASSET
};

LLThrottleGroup::LLThrottleGroup()
{
	for (S32 i = 0; i < TC_EOF; ++i)
	{
		mThrottleTotal[i] = gThrottleDefaultBPS[i];
		mNominalBPS[i] = gThrottleDefaultBPS[i];
	}

	resetDynamicAdjust();
}

void LLThrottleGroup::packThrottle(LLDataPacker& dp) const
{
	for (S32 i = 0; i < TC_EOF; ++i)
	{
		dp.packF32(mThrottleTotal[i], "Throttle");
	}
}

void LLThrottleGroup::unpackThrottle(LLDataPacker& dp)
{
	for (S32 i = 0; i < TC_EOF; ++i)
	{
		F32 temp_throttle;
		dp.unpackF32(temp_throttle, "Throttle");
		temp_throttle = llclamp(temp_throttle, 0.f, 2250000.f);
		mThrottleTotal[i] = temp_throttle;
		if (mThrottleTotal[i] > gThrottleMaximumBPS[i])
		{
			mThrottleTotal[i] = gThrottleMaximumBPS[i];
		}
	}
}

// Call this whenever mNominalBPS changes. Need to reset the measurement
// systems. In the future, we should look into NOT resetting the system.
void LLThrottleGroup::resetDynamicAdjust()
{
	F64 mt_sec = LLMessageSystem::getMessageTimeSeconds();
	for (S32 i = 0; i < TC_EOF; ++i)
	{
		mCurrentBPS[i] = mNominalBPS[i];
		mBitsAvailable[i] = mNominalBPS[i] * THROTTLE_LOOKAHEAD_TIME;
		mLastSendTime[i] = mt_sec;
		mBitsSentThisPeriod[i] = 0;
		mBitsSentHistory[i] = 0;
	}
	mDynamicAdjustTime = mt_sec;
}

bool LLThrottleGroup::setNominalBPS(F32* throttle_vec)
{
	bool changed = false;
	for (S32 i = 0; i < TC_EOF; ++i)
	{
		if (mNominalBPS[i] != throttle_vec[i])
		{
			changed = true;
			mNominalBPS[i] = throttle_vec[i];
		}
	}

	// If we changed the nominal settings, reset the dynamic adjustment
	// subsystem.
	if (changed)
	{
		resetDynamicAdjust();
	}

	return changed;
}

// Return bits available in the channel
S32 LLThrottleGroup::getAvailable(S32 throttle_cat)
{
	S32 retval = 0;

	F32 category_bps = mCurrentBPS[throttle_cat];
	F32 lookahead_bits = category_bps * THROTTLE_LOOKAHEAD_TIME;

	// use a temporary bits_available
	// since we don't want to change mBitsAvailable every time
	F32 elapsed_time = (F32)(LLMessageSystem::getMessageTimeSeconds() -
							 mLastSendTime[throttle_cat]);
	F32 bits_available = mBitsAvailable[throttle_cat] +
						 (category_bps * elapsed_time);

	if (bits_available >= lookahead_bits)
	{
		retval = (S32)gThrottleMaximumBPS[throttle_cat];
	}
	else
	{
		retval = (S32)bits_available;
	}

	return retval;
}

bool LLThrottleGroup::checkOverflow(S32 throttle_cat, F32 bits)
{
	bool retval = true;

	F32 category_bps = mCurrentBPS[throttle_cat];
	F32 lookahead_bits = category_bps * THROTTLE_LOOKAHEAD_TIME;

	// Use a temporary bits_available since we do not want to change
	// mBitsAvailable every time
	F32 elapsed_time = (F32)(LLMessageSystem::getMessageTimeSeconds() -
							 mLastSendTime[throttle_cat]);
	F32 bits_available = mBitsAvailable[throttle_cat] +
						 (category_bps * elapsed_time);

	if (bits_available >= lookahead_bits)
	{
		// ...channel completely open, so allow send regardless
		// of size.  This allows sends on very low BPS channels.
		mBitsAvailable[throttle_cat] = lookahead_bits;
		retval = false;
	}
	else if (bits_available > bits)
	{
		// ...enough space to send this message
		retval = false;
	}

	return retval;
}

bool LLThrottleGroup::throttleOverflow(S32 throttle_cat, F32 bits)
{
	F32 elapsed_time;
	F32 category_bps;
	F32 lookahead_bits;
	bool retval = true;

	category_bps = mCurrentBPS[throttle_cat];
	lookahead_bits = category_bps * THROTTLE_LOOKAHEAD_TIME;

	F64 mt_sec = LLMessageSystem::getMessageTimeSeconds();
	elapsed_time = (F32)(mt_sec - mLastSendTime[throttle_cat]);
	mLastSendTime[throttle_cat] = mt_sec;
	mBitsAvailable[throttle_cat] += category_bps * elapsed_time;

	if (mBitsAvailable[throttle_cat] >= lookahead_bits)
	{
		// ...channel completely open, so allow send regardless
		// of size.  This allows sends on very low BPS channels.
		mBitsAvailable[throttle_cat] = lookahead_bits;
		retval = false;
	}
	else if (mBitsAvailable[throttle_cat] > bits)
	{
		// ...enough space to send this message
		retval = false;
	}

	// We actually already sent the bits.
	mBitsAvailable[throttle_cat] -= bits;

	mBitsSentThisPeriod[throttle_cat] += bits;

	// What if bitsavailable goes negative ? That is OK, because it means
	// someone is banging on the channel, so we need some time to recover.

	return retval;
}

bool LLThrottleGroup::dynamicAdjust()
{
	constexpr F32 DYNAMIC_ADJUST_TIME = 1.f; // second
	// How much weight to give to last period while determining BPS
	// utilization
	constexpr F32 CURRENT_PERIOD_WEIGHT = .25f;
	// If use more than this fraction of BPS, you are busy
	constexpr F32 BUSY_PERCENT = 0.75f;
	// If use less than this fraction, you are "idle"
	constexpr F32 IDLE_PERCENT = 0.70f;
	// How much unused bandwidth to take away each adjustment
	constexpr F32 TRANSFER_PERCENT = 0.90f;
	// How much to give back during recovery phase
	constexpr F32 RECOVER_PERCENT = 0.25f;

	S32 i;

	F64 mt_sec = LLMessageSystem::getMessageTimeSeconds();

	// Only dynamically adjust every few seconds
	if ((mt_sec - mDynamicAdjustTime) < DYNAMIC_ADJUST_TIME)
	{
		return false;
	}
	mDynamicAdjustTime = mt_sec;

	// Update historical information
	for (i = 0; i < TC_EOF; ++i)
	{
		if (mBitsSentHistory[i] == 0)
		{
			// first run, just copy current period
			mBitsSentHistory[i] = mBitsSentThisPeriod[i];
		}
		else
		{
			// have some history, so weight accordingly
			mBitsSentHistory[i] = (1.f - CURRENT_PERIOD_WEIGHT) *
								  mBitsSentHistory[i] +
								  CURRENT_PERIOD_WEIGHT *
								  mBitsSentThisPeriod[i];
		}

		mBitsSentThisPeriod[i] = 0;
	}

	// Look for busy channels. *TODO: Fold into loop above.
	bool channels_busy = false;
	F32  busy_nominal_sum = 0;
	bool channel_busy[TC_EOF];
	bool channel_idle[TC_EOF];
	bool channel_over_nominal[TC_EOF];

	for (i = 0; i < TC_EOF; ++i)
	{
		// Is this a busy channel ?
		if (mBitsSentHistory[i] >=
				BUSY_PERCENT * DYNAMIC_ADJUST_TIME * mCurrentBPS[i])
		{
			// This channel is busy
			channels_busy = true;
			// Use for allocation of pooled idle bandwidth
			busy_nominal_sum += mNominalBPS[i];
			channel_busy[i] = true;
		}
		else
		{
			channel_busy[i] = false;
		}

		// Is this an idle channel ?
		if (mBitsAvailable[i] > 0 &&
			mBitsSentHistory[i] <
				IDLE_PERCENT * DYNAMIC_ADJUST_TIME * mCurrentBPS[i])
		{
			channel_idle[i] = true;
		}
		else
		{
			channel_idle[i] = false;
		}

		// Is this an overpumped channel?
		if (mCurrentBPS[i] > mNominalBPS[i])
		{
			channel_over_nominal[i] = true;
		}
		else
		{
			channel_over_nominal[i] = false;
		}
	}

	if (channels_busy)
	{
		// Some channels are busy. Let's see if we can get them some bandwidth.
		F32 used_bps;
		F32 avail_bps;
		F32 transfer_bps;

		F32 pool_bps = 0;

		for (i = 0; i < TC_EOF; ++i)
		{
			if (channel_idle[i] || channel_over_nominal[i])
			{
				// Either channel i is idle, or has been overpumped.
				// Therefore it's a candidate to give up some bandwidth.
				// Figure out how much bandwidth it has been using, and how
				// much is available to steal.
				used_bps = mBitsSentHistory[i] / DYNAMIC_ADJUST_TIME;

				// CRO make sure to keep a minimum amount of throttle available
				// CRO NB: channels set to < MINIMUM_BPS will never give up
				// bps, which is correct I think.
				if (used_bps < gThrottleMinimumBPS[i])
				{
					used_bps = gThrottleMinimumBPS[i];
				}

				if (channel_over_nominal[i])
				{
					F32 unused_current = mCurrentBPS[i] - used_bps;
					avail_bps = llmax(mCurrentBPS[i] - mNominalBPS[i],
									  unused_current);
				}
				else
				{
					avail_bps = mCurrentBPS[i] - used_bps;
				}

				// Historically, a channel could have used more than its
				// current share, even if it is idle right now. Make sure we
				// do not steal too much.
				if (avail_bps < 0)
				{
					continue;
				}

				// Transfer some bandwidth from this channel into the global
				// pool.
				transfer_bps = avail_bps * TRANSFER_PERCENT;
				mCurrentBPS[i] -= transfer_bps;
				pool_bps += transfer_bps;
			}
		}

		// Now redistribute the bandwidth to busy channels.
		F32 unused_bps = 0.f;

		for (i = 0; i < TC_EOF; ++i)
		{
			if (channel_busy[i])
			{
				F32 add_amount = pool_bps * (mNominalBPS[i] / busy_nominal_sum);
				mCurrentBPS[i] += add_amount;

				// CRO: make sure this does not get too huge
				// JC - Actually, need to let mCurrentBPS go less than nominal,
				// otherwise you are not allowing bandwidth to actually be
				// moved from one channel to another.
				// *TODO: If clamping high end, would be good to re-allocate to
				// other channels in the above code.
				const F32 max_bps = 4 * mNominalBPS[i];
				if (mCurrentBPS[i] > max_bps)
				{
					F32 overage = mCurrentBPS[i] - max_bps;
					mCurrentBPS[i] -= overage;
					unused_bps += overage;
				}

				// Paranoia
				if (mCurrentBPS[i] < gThrottleMinimumBPS[i])
				{
					mCurrentBPS[i] = gThrottleMinimumBPS[i];
				}
			}
		}

		// For fun, add the overage back in to objects
		if (unused_bps > 0.f)
		{
			mCurrentBPS[TC_TASK] += unused_bps;
		}
	}
	else
	{
		// No one is busy.
		// Make the channel allocations seek toward nominal.

		// Look for overpumped channels
		F32 starved_nominal_sum = 0;
		F32 avail_bps = 0;
		F32 transfer_bps = 0;
		F32 pool_bps = 0;
		for (i = 0; i < TC_EOF; ++i)
		{
			if (mCurrentBPS[i] > mNominalBPS[i])
			{
				avail_bps = (mCurrentBPS[i] - mNominalBPS[i]);
				transfer_bps = avail_bps * RECOVER_PERCENT;

				mCurrentBPS[i] -= transfer_bps;
				pool_bps += transfer_bps;
			}
		}

		// Evenly distribute bandwidth to channels currently
		// using less than nominal.
		for (i = 0; i < TC_EOF; ++i)
		{
			if (mCurrentBPS[i] < mNominalBPS[i])
			{
				// We're going to weight allocations by nominal BPS.
				starved_nominal_sum += mNominalBPS[i];
			}
		}

		for (i = 0; i < TC_EOF; ++i)
		{
			if (mCurrentBPS[i] < mNominalBPS[i])
			{
				// Distribute bandwidth according to nominal allocation ratios.
				mCurrentBPS[i] += pool_bps *
								  (mNominalBPS[i] / starved_nominal_sum);
			}
		}
	}

	return true;
}
