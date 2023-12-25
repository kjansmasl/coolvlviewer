/**
 * @file llrand.h
 * @brief Information, functions, and typedefs for randomness.
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

#ifndef LL_LLRAND_H
#define LL_LLRAND_H

#include "llpreprocessor.h"

#include "stdtypes.h"

// *NOTE: The system rand implementation is probably not correct.
#define LL_USE_SYSTEM_RAND 0

/**
 * Use the boost random number generators if you want a stateful
 * random numbers. If you want more random numbers, use the
 * C-functions since they will generate faster/better randomness
 * across the process.
 *
 * I tested some of the boost random engines, and picked a good double
 * generator and a good integer generator. I also took some timings
 * for them on Linux using gcc 3.3.5. The harness also did some other
 * fairly trivial operations to try to limit compiler optimizations,
 * so these numbers are only good for relative comparisons.
 *
 * usec/inter		algorithm
 * 0.21				boost::minstd_rand0
 * 0.039			boost:lagged_fibonacci19937
 * 0.036			boost:lagged_fibonacci607
 * 0.44				boost::hellekalek1995
 * 0.44				boost::ecuyer1988
 * 0.042			boost::rand48
 * 0.043			boost::mt11213b
 * 0.028			stdlib random()
 * 0.05				stdlib lrand48()
 * 0.034			stdlib rand()
 * 0.020			the old & lame LLRand
 */

// Generates a float from [0, RAND_MAX).
S32 ll_rand();

// Generates a float from [0, val) or (val, 0].
S32 ll_rand(S32 val);

// Generates a float from [0, 1.0).
F32 ll_frand();

// Generates a float from [0, val) or (val, 0].
F32 ll_frand(F32 val);

// Generates a double from [0, 1.0).
F64 ll_drand();

// Generates a double from [0, val) or (val, 0].
F64 ll_drand(F64 val);

#endif
