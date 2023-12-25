/**
 * @file llpatch_code.cpp
 * @brief Encode patch DCT data into bitcode.
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

#include "linden_common.h"

#include "llpatch_code.h"

#include "llbitpack.h"
#include "llmath.h"
#include "llvector3.h"

U32 gPatchSize;
U32 gWordBits;

void init_patch_coding(LLBitPack& bitpack)
{
	bitpack.resetBitPacking();
}

void code_patch_group_header(LLBitPack& bitpack, LLGroupHeader* gopp)
{
#if LL_BIG_ENDIAN
	U8* stride = (U8*)&gopp->stride;
	bitpack.bitPack(&(stride[1]), 8);
	bitpack.bitPack(&(stride[0]), 8);
#else
	bitpack.bitPack((U8*)&gopp->stride, 16);
#endif
	bitpack.bitPack((U8*)&gopp->patch_size, 8);
	bitpack.bitPack((U8*)&gopp->layer_type, 8);

	gPatchSize = gopp->patch_size;
}

void code_patch_header(LLBitPack& bitpack, LLPatchHeader* ph, S32* patch)
{
	S32 i, j, temp;
	S32 surface = gPatchSize * gPatchSize;
	S32 wbits = (ph->quant_wbits & 0xf) + 2;
	U32 max_wbits = wbits + 5, min_wbits = wbits>>1;

	wbits = min_wbits;

	for (i = 0; i < surface; ++i)
	{
		temp = patch[i];
		if (temp)
		{
			if (temp < 0)
			{
				temp *= -1;
			}
			for (j = max_wbits; j > (S32)min_wbits; --j)
			{
				if (temp & (1 << j))
				{
					if (j > wbits)
					{
						wbits = j;
					}
					break;
				}
			}
		}
	}

	wbits += 1;

	ph->quant_wbits &= 0xf0;

	if (wbits > 17 || wbits < 2)
	{
		llerrs << "Bits needed per word in code_patch_header out of legal range. Adjust compression quantization."
			   << llendl;
	}

	ph->quant_wbits |= wbits - 2;

	bitpack.bitPack((U8*)&ph->quant_wbits, 8);
#if LL_BIG_ENDIAN
	U8* offset = (U8*)&ph->dc_offset;
	bitpack.bitPack(&(offset[3]), 8);
	bitpack.bitPack(&(offset[2]), 8);
	bitpack.bitPack(&(offset[1]), 8);
	bitpack.bitPack(&(offset[0]), 8);
#else
	bitpack.bitPack((U8*)&ph->dc_offset, 32);
#endif
#if LL_BIG_ENDIAN
	U8* range = (U8*)&ph->range;
	bitpack.bitPack(&(range[1]), 8);
	bitpack.bitPack(&(range[0]), 8);
#else
	bitpack.bitPack((U8*)&ph->range, 16);
#endif
#if LL_BIG_ENDIAN
	U8* ids = (U8*)&ph->patchids;
	bitpack.bitPack(&(ids[1]), 8);
	bitpack.bitPack(&(ids[0]), 2);
#else
	bitpack.bitPack((U8*)&ph->patchids, 10);
#endif

	gWordBits = wbits;
}

void code_end_of_data(LLBitPack& bitpack)
{
	bitpack.bitPack((U8*)&END_OF_PATCHES, 8);
}

void code_patch(LLBitPack& bitpack, S32* patch, S32 postquant)
{
	S32 i, j;
	S32 surface = gPatchSize * gPatchSize;
	S32 wbits = gWordBits;
	S32 temp;
	bool b_eob;

	if (postquant > surface || postquant < 0)
	{
		llerrs << "Bad postquant in code_patch!"  << llendl;
	}

	if (postquant)
	{
		patch[surface - postquant] = 0;
	}

	for (i = 0; i < surface; ++i)
	{
		b_eob = false;
		temp = patch[i];
		if (!temp)
		{
			b_eob = true;
			for (j = i; j < surface - postquant; ++j)
			{
				if (patch[j])
				{
					b_eob = false;
					break;
				}
			}
			if (b_eob)
			{
				bitpack.bitPack((U8*)&ZERO_EOB, 2);
				return;
			}
			else
			{
				bitpack.bitPack((U8*)&ZERO_CODE, 1);
			}
		}
		else
		{
			if (temp < 0)
			{
				temp *= -1;
				if (temp > (1 << wbits))
				{
					temp = 1 << wbits;
				}
				bitpack.bitPack((U8*)&NEGATIVE_VALUE, 3);
				bitpack.bitPack((U8*)&temp, wbits);
			}
			else
			{
				if (temp > (1 << wbits))
				{
					temp = (1 << wbits);
				}
				bitpack.bitPack((U8*)&POSITIVE_VALUE, 3);
				bitpack.bitPack((U8*)&temp, wbits);
			}
		}
	}
}

void end_patch_coding(LLBitPack& bitpack)
{
	bitpack.flushBitPack();
}

void init_patch_decoding(LLBitPack& bitpack)
{
	bitpack.resetBitPacking();
}

void decode_patch_group_header(LLBitPack& bitpack, LLGroupHeader* gopp)
{
	U16 retvalu16 = 0;
#if LL_BIG_ENDIAN
	U8 *ret = (U8*)&retvalu16;
	bitpack.bitUnpack(&(ret[1]), 8);
	bitpack.bitUnpack(&(ret[0]), 8);
#else
	bitpack.bitUnpack((U8*)&retvalu16, 16);
#endif
	gopp->stride = retvalu16;

	U8 retvalu8 = 0;
	bitpack.bitUnpack(&retvalu8, 8);
	gopp->patch_size = retvalu8;

	retvalu8 = 0;
	bitpack.bitUnpack(&retvalu8, 8);
	gopp->layer_type = retvalu8;

	gPatchSize = gopp->patch_size;
}

void decode_patch_header(LLBitPack& bitpack, LLPatchHeader* ph,
						 bool large_patch)
{
	U8 retvalu8 = 0;
	bitpack.bitUnpack(&retvalu8, 8);
	ph->quant_wbits = retvalu8;

	if (END_OF_PATCHES == ph->quant_wbits)
	{
		// End of data, blitz the rest.
		ph->dc_offset = 0;
		ph->range = 0;
		ph->patchids = 0;
		return;
	}

	U32 retvalu32 = 0;
#if LL_BIG_ENDIAN
	U8* ret = (U8*)&retvalu32;
	bitpack.bitUnpack(&(ret[3]), 8);
	bitpack.bitUnpack(&(ret[2]), 8);
	bitpack.bitUnpack(&(ret[1]), 8);
	bitpack.bitUnpack(&(ret[0]), 8);
#else
	bitpack.bitUnpack((U8*)&retvalu32, 32);
#endif
	ph->dc_offset = *(F32*)&retvalu32;

	U16 retvalu16 = 0;
#if LL_BIG_ENDIAN
	ret = (U8*)&retvalu16;
	bitpack.bitUnpack(&(ret[1]), 8);
	bitpack.bitUnpack(&(ret[0]), 8);
#else
	bitpack.bitUnpack((U8*)&retvalu16, 16);
#endif
	ph->range = retvalu16;

	retvalu32 = 0;
#if LL_BIG_ENDIAN
	ret = (U8*)&retvalu32;
	if (large_patch)
	{
		bitpack.bitUnpack(&(ret[3]), 8);
		bitpack.bitUnpack(&(ret[2]), 8);
		bitpack.bitUnpack(&(ret[1]), 8);
		bitpack.bitUnpack(&(ret[0]), 8);
	}
	else
	{
		bitpack.bitUnpack(&(ret[1]), 8);
		bitpack.bitUnpack(&(ret[0]), 2);
	}
#else
	bitpack.bitUnpack((U8*)&retvalu32, large_patch ? 32 : 10);
#endif
	ph->patchids = retvalu32;

	gWordBits = (ph->quant_wbits & 0xf) + 2;
}

void decode_patch(LLBitPack& bitpack, S32* patches)
{
	S32 surface = gPatchSize * gPatchSize;
	S32 wbits = gWordBits;
#if LL_BIG_ENDIAN
	U8 tempu8;
	U16 tempu16;
	U32 tempu32;
	for (S32 i = 0; i < surface; ++i)
	{
		bitpack.bitUnpack((U8*)&tempu8, 1);
		if (tempu8)
		{
			// Either 0 EOB or Value
			bitpack.bitUnpack((U8*)&tempu8, 1);
			if (tempu8)
			{
				// Value
				bitpack.bitUnpack((U8*)&tempu8, 1);
				if (tempu8)
				{
					// Negative
					patches[i] = -1;
				}
				else
				{
					// Positive
					patches[i] = 1;
				}
				if (wbits <= 8)
				{
					bitpack.bitUnpack((U8*)&tempu8, wbits);
					patches[i] *= tempu8;
				}
				else if (wbits <= 16)
				{
					tempu16 = 0;
					U8 *ret = (U8*)&tempu16;
					bitpack.bitUnpack(&(ret[1]), 8);
					bitpack.bitUnpack(&(ret[0]), wbits - 8);
					patches[i] *= tempu16;
				}
				else if (wbits <= 24)
				{
					tempu32 = 0;
					U8 *ret = (U8*)&tempu32;
					bitpack.bitUnpack(&(ret[2]), 8);
					bitpack.bitUnpack(&(ret[1]), 8);
					bitpack.bitUnpack(&(ret[0]), wbits - 16);
					patches[i] *= tempu32;
				}
				else if (wbits <= 32)
				{
					tempu32 = 0;
					U8 *ret = (U8*)&tempu32;
					bitpack.bitUnpack(&(ret[3]), 8);
					bitpack.bitUnpack(&(ret[2]), 8);
					bitpack.bitUnpack(&(ret[1]), 8);
					bitpack.bitUnpack(&(ret[0]), wbits - 24);
					patches[i] *= tempu32;
				}
			}
			else
			{
				for (S32 j = i; j < surface; ++j)
				{
					patches[j] = 0;
				}
				return;
			}
		}
		else
		{
			patches[i] = 0;
		}
	}
#else
	U32 temp;
	for (S32 i = 0; i < surface; ++i)
	{
		temp = 0;
		bitpack.bitUnpack((U8*)&temp, 1);
		if (temp)
		{
			// Either 0 EOB or Value
			temp = 0;
			bitpack.bitUnpack((U8*)&temp, 1);
			if (temp)
			{
				// Value
				temp = 0;
				bitpack.bitUnpack((U8*)&temp, 1);
				if (temp)
				{
					// Negative
					temp = 0;
					bitpack.bitUnpack((U8*)&temp, wbits);
					patches[i] = temp;
					patches[i] *= -1;
				}
				else
				{
					// Positive
					temp = 0;
					bitpack.bitUnpack((U8*)&temp, wbits);
					patches[i] = temp;
				}
			}
			else
			{
				for (S32 j = i; j < surface; ++j)
				{
					patches[j] = 0;
				}
				return;
			}
		}
		else
		{
			patches[i] = 0;
		}
	}
#endif
}
