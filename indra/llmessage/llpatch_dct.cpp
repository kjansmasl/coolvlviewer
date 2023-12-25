/**
 * @file llpatch_dct.cpp
 * @brief DCT patch.
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

#include "llmath.h"
#include "llvector3.h"
#include "llpatch_dct.h"

typedef struct s_patch_compress_global_data
{
	S32 patch_size;
	S32 patch_stride;
	U32 charptr;
	S32 layer_type;
} PCGD;

PCGD gPatchCompressGlobalData;

void reset_patch_compressor()
{
	PCGD* pcp = &gPatchCompressGlobalData;
	pcp->charptr = 0;
}

S32	gCurrentSize = 0;

F32 gPatchQuantizeTable[LARGE_PATCH_SIZE * LARGE_PATCH_SIZE];

void build_patch_quantize_table(S32 size)
{
	for (S32 j = 0; j < size; ++j)
	{
		for (S32 i = 0; i < size; ++i)
		{
			gPatchQuantizeTable[j * size + i] = 1.f / (1.f + 2.f * (i + j));
		}
	}
}

F32	gPatchCosines[LARGE_PATCH_SIZE*LARGE_PATCH_SIZE];

void setup_patch_cosines(S32 size)
{
	F32 oosob = F_PI * 0.5f / size;

	for (S32 u = 0; u < size; ++u)
	{
		for (S32 n = 0; n < size; ++n)
		{
			gPatchCosines[u * size + n] = cosf((2.f * n + 1.f) * u * oosob);
		}
	}
}

S32	gCopyMatrix[LARGE_PATCH_SIZE * LARGE_PATCH_SIZE];

void build_copy_matrix(S32 size)
{
	bool b_diag = false;
	bool b_right = true;

	S32 i = 0;
	S32 j = 0;
	S32 count = 0;

	while (i < size && j < size)
	{
		gCopyMatrix[j * size + i] = count++;

		if (!b_diag)
		{
			if (b_right)
			{
				if (i < size - 1)
				{
					++i;
				}
				else
				{
					++j;
				}
				b_right = false;
				b_diag = true;
			}
			else
			{
				if (j < size - 1)
				{
					++j;
				}
				else
				{
					++i;
				}
				b_right = true;
				b_diag = true;
			}
		}
		else
		{
			if (b_right)
			{
				++i;
				--j;
				if (i == size - 1 || j == 0)
				{
					b_diag = false;
				}
			}
			else
			{
				--i;
				++j;
				if (i == 0 || j == size - 1)
				{
					b_diag = false;
				}
			}
		}
	}
}

void init_patch_compressor(S32 patch_size, S32 patch_stride, S32 layer_type)
{
	PCGD* pcp = &gPatchCompressGlobalData;

	pcp->charptr = 0;

	pcp->patch_size = patch_size;
	pcp->patch_stride = patch_stride;
	pcp->layer_type = layer_type;

	if (patch_size != gCurrentSize)
	{
		gCurrentSize = patch_size;
		build_patch_quantize_table(patch_size);
		setup_patch_cosines(patch_size);
		build_copy_matrix(patch_size);
	}
}

void prescan_patch(F32* patch, LLPatchHeader* php, F32& zmax, F32& zmin)
{
	PCGD*	pcp = &gPatchCompressGlobalData;
	S32		stride = pcp->patch_stride;
	S32		size = pcp->patch_size;
	S32		jstride;

	zmax = -99999999.f;
	zmin = 99999999.f;

	for (S32 j = 0; j < size; ++j)
	{
		jstride = j * stride;
		for (S32 i = 0; i < size; ++i)
		{
			if (*(patch + jstride + i) > zmax)
			{
				zmax = *(patch + jstride + i);
			}
			if (*(patch + jstride + i) < zmin)
			{
				zmin = *(patch + jstride + i);
			}
		}
	}

	php->dc_offset = zmin;
	php->range = (U16)(zmax - zmin + 1.f);
}

void dct_line(F32* linein, F32* lineout, S32 line)
{
	F32 total;
	F32 *pcp = gPatchCosines;
	S32	line_size = line * NORMAL_PATCH_SIZE;

#ifdef _PATCH_SIZE_16_AND_32_ONLY
	F32* tlinein;
	F32* tpcp;

	tlinein = linein + line_size;

	total = *(tlinein++);
	total += *(tlinein++);
	total += *(tlinein++);
	total += *(tlinein++);

	total += *(tlinein++);
	total += *(tlinein++);
	total += *(tlinein++);
	total += *(tlinein++);

	total += *(tlinein++);
	total += *(tlinein++);
	total += *(tlinein++);
	total += *(tlinein++);

	total += *(tlinein++);
	total += *(tlinein++);
	total += *(tlinein++);
	total += *(tlinein);

	*(lineout + line_size) = OO_SQRT2 * total;

	for (S32 u = 1; u < NORMAL_PATCH_SIZE; ++u)
	{
		tlinein = linein + line_size;
		tpcp = pcp + (u<<4);

		total = *(tlinein++) * (*(tpcp++));
		total += *(tlinein++) * (*(tpcp++));
		total += *(tlinein++) * (*(tpcp++));
		total += *(tlinein++) * (*(tpcp++));

		total += *(tlinein++) * (*(tpcp++));
		total += *(tlinein++) * (*(tpcp++));
		total += *(tlinein++) * (*(tpcp++));
		total += *(tlinein++) * (*(tpcp++));

		total += *(tlinein++) * (*(tpcp++));
		total += *(tlinein++) * (*(tpcp++));
		total += *(tlinein++) * (*(tpcp++));
		total += *(tlinein++) * (*(tpcp++));

		total += *(tlinein++) * (*(tpcp++));
		total += *(tlinein++) * (*(tpcp++));
		total += *(tlinein++) * (*(tpcp++));
		total += *(tlinein)*(*tpcp);

		*(lineout + line_size + u) = total;
	}
#else
	S32	size = gPatchCompressGlobalData.patch_size;
	total = 0.f;
	for (S32 n = 0; n < size; ++n)
	{
		total += linein[line_size + n];
	}
	lineout[line_size] = OO_SQRT2*total;

	for (S32 u = 1; u < size; ++u)
	{
		total = 0.f;
		for (S32 n = 0; n < size; ++n)
		{
			total += linein[line_size + n] * pcp[u * size + n];
		}
		lineout[line_size + u] = total;
	}
#endif
}

void dct_line_large(F32* linein, F32* lineout, S32 line)
{
	F32 total;
	F32* pcp = gPatchCosines;
	S32	line_size = line * LARGE_PATCH_SIZE;

	F32* tlinein;
	F32* tpcp;

	tlinein = linein + line_size;

	total = *(tlinein++);
	total += *(tlinein++);
	total += *(tlinein++);
	total += *(tlinein++);

	total += *(tlinein++);
	total += *(tlinein++);
	total += *(tlinein++);
	total += *(tlinein++);

	total += *(tlinein++);
	total += *(tlinein++);
	total += *(tlinein++);
	total += *(tlinein++);

	total += *(tlinein++);
	total += *(tlinein++);
	total += *(tlinein++);
	total += *(tlinein++);

	total += *(tlinein++);
	total += *(tlinein++);
	total += *(tlinein++);
	total += *(tlinein++);

	total += *(tlinein++);
	total += *(tlinein++);
	total += *(tlinein++);
	total += *(tlinein++);

	total += *(tlinein++);
	total += *(tlinein++);
	total += *(tlinein++);
	total += *(tlinein++);

	total += *(tlinein++);
	total += *(tlinein++);
	total += *(tlinein++);
	total += *(tlinein);

	*(lineout + line_size) = OO_SQRT2 * total;

	for (S32 u = 1; u < LARGE_PATCH_SIZE; ++u)
	{
		tlinein = linein + line_size;
		tpcp = pcp + (u << 5);

		total = *(tlinein++) * (*(tpcp++));
		total += *(tlinein++) * (*(tpcp++));
		total += *(tlinein++) * (*(tpcp++));
		total += *(tlinein++) * (*(tpcp++));

		total += *(tlinein++) * (*(tpcp++));
		total += *(tlinein++) * (*(tpcp++));
		total += *(tlinein++) * (*(tpcp++));
		total += *(tlinein++) * (*(tpcp++));

		total += *(tlinein++) * (*(tpcp++));
		total += *(tlinein++) * (*(tpcp++));
		total += *(tlinein++) * (*(tpcp++));
		total += *(tlinein++) * (*(tpcp++));

		total += *(tlinein++) * (*(tpcp++));
		total += *(tlinein++) * (*(tpcp++));
		total += *(tlinein++) * (*(tpcp++));
		total += *(tlinein++) * (*(tpcp++));

		total += *(tlinein++) * (*(tpcp++));
		total += *(tlinein++) * (*(tpcp++));
		total += *(tlinein++) * (*(tpcp++));
		total += *(tlinein++) * (*(tpcp++));

		total += *(tlinein++) * (*(tpcp++));
		total += *(tlinein++) * (*(tpcp++));
		total += *(tlinein++) * (*(tpcp++));
		total += *(tlinein++) * (*(tpcp++));

		total += *(tlinein++) * (*(tpcp++));
		total += *(tlinein++) * (*(tpcp++));
		total += *(tlinein++) * (*(tpcp++));
		total += *(tlinein++) * (*(tpcp++));

		total += *(tlinein++) * (*(tpcp++));
		total += *(tlinein++) * (*(tpcp++));
		total += *(tlinein++) * (*(tpcp++));
		total += *(tlinein) * (*tpcp);

		*(lineout + line_size + u) = total;
	}
}

LL_INLINE void dct_column(F32* linein, S32* lineout, S32 column)
{
	F32 total;
	F32 oosob = 2.f / 16.f;
	F32* pcp = gPatchCosines;
	S32* copy_matrix = gCopyMatrix;
	F32* qt = gPatchQuantizeTable;

#ifdef _PATCH_SIZE_16_AND_32_ONLY
	F32* tlinein;
	F32* tpcp;
	S32 sizeu;

	tlinein = linein + column;

	total = *(tlinein);
	total += *(tlinein += NORMAL_PATCH_SIZE);
	total += *(tlinein += NORMAL_PATCH_SIZE);
	total += *(tlinein += NORMAL_PATCH_SIZE);

	total += *(tlinein += NORMAL_PATCH_SIZE);
	total += *(tlinein += NORMAL_PATCH_SIZE);
	total += *(tlinein += NORMAL_PATCH_SIZE);
	total += *(tlinein += NORMAL_PATCH_SIZE);

	total += *(tlinein += NORMAL_PATCH_SIZE);
	total += *(tlinein += NORMAL_PATCH_SIZE);
	total += *(tlinein += NORMAL_PATCH_SIZE);
	total += *(tlinein += NORMAL_PATCH_SIZE);

	total += *(tlinein += NORMAL_PATCH_SIZE);
	total += *(tlinein += NORMAL_PATCH_SIZE);
	total += *(tlinein += NORMAL_PATCH_SIZE);
	total += *(tlinein += NORMAL_PATCH_SIZE);

	*(lineout + *(copy_matrix + column)) = (S32)(OO_SQRT2 * total * oosob*(*(qt + column)));

	for (S32 u = 1; u < NORMAL_PATCH_SIZE; ++u)
	{
		tlinein = linein + column;
		tpcp = pcp + (u << 4);

		total = *(tlinein)*(*(tpcp++));
		total += *(tlinein += NORMAL_PATCH_SIZE) * (*(tpcp++));
		total += *(tlinein += NORMAL_PATCH_SIZE) * (*(tpcp++));
		total += *(tlinein += NORMAL_PATCH_SIZE) * (*(tpcp++));

		total += *(tlinein += NORMAL_PATCH_SIZE) * (*(tpcp++));
		total += *(tlinein += NORMAL_PATCH_SIZE) * (*(tpcp++));
		total += *(tlinein += NORMAL_PATCH_SIZE) * (*(tpcp++));
		total += *(tlinein += NORMAL_PATCH_SIZE) * (*(tpcp++));

		total += *(tlinein += NORMAL_PATCH_SIZE) * (*(tpcp++));
		total += *(tlinein += NORMAL_PATCH_SIZE) * (*(tpcp++));
		total += *(tlinein += NORMAL_PATCH_SIZE) * (*(tpcp++));
		total += *(tlinein += NORMAL_PATCH_SIZE) * (*(tpcp++));

		total += *(tlinein += NORMAL_PATCH_SIZE) * (*(tpcp++));
		total += *(tlinein += NORMAL_PATCH_SIZE) * (*(tpcp++));
		total += *(tlinein += NORMAL_PATCH_SIZE) * (*(tpcp++));
		total += *(tlinein += NORMAL_PATCH_SIZE) * (*(tpcp));

		sizeu = NORMAL_PATCH_SIZE * u + column;

		*(lineout + *(copy_matrix + sizeu)) = (S32)(total * oosob * (*(qt + sizeu)));
	}
#else
	S32	size = gPatchCompressGlobalData.patch_size;
	F32 oosob = 2.f / size;
	total = 0.f;
	for (S32 n = 0; n < size; ++n)
	{
		total += linein[size * n + column];
	}
	lineout[copy_matrix[column]] = OO_SQRT2 * total * oosob * qt[column];

	for (S32 u = 1; u < size; ++u)
	{
		total = 0.f;
		for (S32 n = 0; n < size; ++n)
		{
			total += linein[size * n + column] * pcp[u * size + n];
		}
		lineout[copy_matrix[size * u + column]] = total * oosob * qt[size * u + column];
	}
#endif
}

LL_INLINE void dct_column_large(F32* linein, S32* lineout, S32 column)
{
	F32 total;
	F32 oosob = 2.f / 32.f;
	F32* pcp = gPatchCosines;
	S32* copy_matrix = gCopyMatrix;
	F32* qt = gPatchQuantizeTable;

	F32* tlinein;
	F32* tpcp;
	S32 sizeu;

	tlinein = linein + column;

	total = *(tlinein);
	total += *(tlinein += LARGE_PATCH_SIZE);
	total += *(tlinein += LARGE_PATCH_SIZE);
	total += *(tlinein += LARGE_PATCH_SIZE);

	total += *(tlinein += LARGE_PATCH_SIZE);
	total += *(tlinein += LARGE_PATCH_SIZE);
	total += *(tlinein += LARGE_PATCH_SIZE);
	total += *(tlinein += LARGE_PATCH_SIZE);

	total += *(tlinein += LARGE_PATCH_SIZE);
	total += *(tlinein += LARGE_PATCH_SIZE);
	total += *(tlinein += LARGE_PATCH_SIZE);
	total += *(tlinein += LARGE_PATCH_SIZE);

	total += *(tlinein += LARGE_PATCH_SIZE);
	total += *(tlinein += LARGE_PATCH_SIZE);
	total += *(tlinein += LARGE_PATCH_SIZE);
	total += *(tlinein += LARGE_PATCH_SIZE);

	total += *(tlinein += LARGE_PATCH_SIZE);
	total += *(tlinein += LARGE_PATCH_SIZE);
	total += *(tlinein += LARGE_PATCH_SIZE);
	total += *(tlinein += LARGE_PATCH_SIZE);

	total += *(tlinein += LARGE_PATCH_SIZE);
	total += *(tlinein += LARGE_PATCH_SIZE);
	total += *(tlinein += LARGE_PATCH_SIZE);
	total += *(tlinein += LARGE_PATCH_SIZE);

	total += *(tlinein += LARGE_PATCH_SIZE);
	total += *(tlinein += LARGE_PATCH_SIZE);
	total += *(tlinein += LARGE_PATCH_SIZE);
	total += *(tlinein += LARGE_PATCH_SIZE);

	total += *(tlinein += LARGE_PATCH_SIZE);
	total += *(tlinein += LARGE_PATCH_SIZE);
	total += *(tlinein += LARGE_PATCH_SIZE);
	total += *(tlinein += LARGE_PATCH_SIZE);

	*(lineout + *(copy_matrix + column)) = (S32)(OO_SQRT2*total*oosob*(*(qt + column)));

	for (S32 u = 1; u < LARGE_PATCH_SIZE; ++u)
	{
		tlinein = linein + column;
		tpcp = pcp + (u << 5);

		total = *(tlinein)*(*(tpcp++));
		total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp++));
		total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp++));
		total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp++));

		total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp++));
		total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp++));
		total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp++));
		total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp++));

		total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp++));
		total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp++));
		total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp++));
		total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp++));

		total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp++));
		total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp++));
		total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp++));
		total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp++));

		total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp++));
		total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp++));
		total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp++));
		total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp++));

		total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp++));
		total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp++));
		total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp++));
		total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp++));

		total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp++));
		total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp++));
		total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp++));
		total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp++));

		total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp++));
		total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp++));
		total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp++));
		total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp));

		sizeu = LARGE_PATCH_SIZE * u + column;

		*(lineout + *(copy_matrix + sizeu)) = (S32)(total * oosob * (*(qt + sizeu)));
	}
}

LL_INLINE void dct_patch(F32* block, S32* cpatch)
{
	F32 temp[NORMAL_PATCH_SIZE * NORMAL_PATCH_SIZE];

#ifdef _PATCH_SIZE_16_AND_32_ONLY
	dct_line(block, temp, 0);
	dct_line(block, temp, 1);
	dct_line(block, temp, 2);
	dct_line(block, temp, 3);

	dct_line(block, temp, 4);
	dct_line(block, temp, 5);
	dct_line(block, temp, 6);
	dct_line(block, temp, 7);

	dct_line(block, temp, 8);
	dct_line(block, temp, 9);
	dct_line(block, temp, 10);
	dct_line(block, temp, 11);

	dct_line(block, temp, 12);
	dct_line(block, temp, 13);
	dct_line(block, temp, 14);
	dct_line(block, temp, 15);

	dct_column(temp, cpatch, 0);
	dct_column(temp, cpatch, 1);
	dct_column(temp, cpatch, 2);
	dct_column(temp, cpatch, 3);

	dct_column(temp, cpatch, 4);
	dct_column(temp, cpatch, 5);
	dct_column(temp, cpatch, 6);
	dct_column(temp, cpatch, 7);

	dct_column(temp, cpatch, 8);
	dct_column(temp, cpatch, 9);
	dct_column(temp, cpatch, 10);
	dct_column(temp, cpatch, 11);

	dct_column(temp, cpatch, 12);
	dct_column(temp, cpatch, 13);
	dct_column(temp, cpatch, 14);
	dct_column(temp, cpatch, 15);
#else
	S32	size = gPatchCompressGlobalData.patch_size;
	for (S32 i = 0; i < size; ++i)
	{
		dct_line(block, temp, i);
	}
	for (S32 i = 0; i < size; ++i)
	{
		dct_column(temp, cpatch, i);
	}
#endif
}

LL_INLINE void dct_patch_large(F32* block, S32* cpatch)
{
	F32 temp[LARGE_PATCH_SIZE * LARGE_PATCH_SIZE];

	dct_line_large(block, temp, 0);
	dct_line_large(block, temp, 1);
	dct_line_large(block, temp, 2);
	dct_line_large(block, temp, 3);

	dct_line_large(block, temp, 4);
	dct_line_large(block, temp, 5);
	dct_line_large(block, temp, 6);
	dct_line_large(block, temp, 7);

	dct_line_large(block, temp, 8);
	dct_line_large(block, temp, 9);
	dct_line_large(block, temp, 10);
	dct_line_large(block, temp, 11);

	dct_line_large(block, temp, 12);
	dct_line_large(block, temp, 13);
	dct_line_large(block, temp, 14);
	dct_line_large(block, temp, 15);

	dct_line_large(block, temp, 16);
	dct_line_large(block, temp, 17);
	dct_line_large(block, temp, 18);
	dct_line_large(block, temp, 19);

	dct_line_large(block, temp, 20);
	dct_line_large(block, temp, 21);
	dct_line_large(block, temp, 22);
	dct_line_large(block, temp, 23);

	dct_line_large(block, temp, 24);
	dct_line_large(block, temp, 25);
	dct_line_large(block, temp, 26);
	dct_line_large(block, temp, 27);

	dct_line_large(block, temp, 28);
	dct_line_large(block, temp, 29);
	dct_line_large(block, temp, 30);
	dct_line_large(block, temp, 31);

	dct_column_large(temp, cpatch, 0);
	dct_column_large(temp, cpatch, 1);
	dct_column_large(temp, cpatch, 2);
	dct_column_large(temp, cpatch, 3);

	dct_column_large(temp, cpatch, 4);
	dct_column_large(temp, cpatch, 5);
	dct_column_large(temp, cpatch, 6);
	dct_column_large(temp, cpatch, 7);

	dct_column_large(temp, cpatch, 8);
	dct_column_large(temp, cpatch, 9);
	dct_column_large(temp, cpatch, 10);
	dct_column_large(temp, cpatch, 11);

	dct_column_large(temp, cpatch, 12);
	dct_column_large(temp, cpatch, 13);
	dct_column_large(temp, cpatch, 14);
	dct_column_large(temp, cpatch, 15);

	dct_column_large(temp, cpatch, 16);
	dct_column_large(temp, cpatch, 17);
	dct_column_large(temp, cpatch, 18);
	dct_column_large(temp, cpatch, 19);

	dct_column_large(temp, cpatch, 20);
	dct_column_large(temp, cpatch, 21);
	dct_column_large(temp, cpatch, 22);
	dct_column_large(temp, cpatch, 23);

	dct_column_large(temp, cpatch, 24);
	dct_column_large(temp, cpatch, 25);
	dct_column_large(temp, cpatch, 26);
	dct_column_large(temp, cpatch, 27);

	dct_column_large(temp, cpatch, 28);
	dct_column_large(temp, cpatch, 29);
	dct_column_large(temp, cpatch, 30);
	dct_column_large(temp, cpatch, 31);
}

void compress_patch(F32* patch, S32* cpatch, LLPatchHeader* php, S32 prequant)
{
	PCGD* pcp = &gPatchCompressGlobalData;
	S32 stride = pcp->patch_stride;
	S32 size = pcp->patch_size;
	F32 block[LARGE_PATCH_SIZE*LARGE_PATCH_SIZE], *tblock;
	F32* tpatch;

	S32 wordsize = prequant;
	F32 oozrange = 1.f / php->range;

	F32 dc = php->dc_offset;

	S32 range = 1 << prequant;
	F32 premult = oozrange*range;
	F32 sub = (F32)(1 << (prequant - 1)) + dc * premult;

	php->quant_wbits = wordsize - 2;
	php->quant_wbits |= (prequant - 2) << 4;

	for (S32 j = 0; j < size; ++j)
	{
		tblock = block + j * size;
		tpatch = patch + j * stride;
		for (S32 i = 0; i < size; ++i)
		{
			*(tblock++) = *(tpatch++)*premult - sub;
		}
	}

	if (size == 16)
	{
		dct_patch(block, cpatch);
	}
	else
	{
		dct_patch_large(block, cpatch);
	}
}

void get_patch_group_header(LLGroupHeader *gopp)
{
	PCGD* pcp = &gPatchCompressGlobalData;
	gopp->stride = pcp->patch_stride;
	gopp->patch_size = pcp->patch_size;
	gopp->layer_type = pcp->layer_type;
}

///////////////////////////////////////////////////////////////////////////////
// Formerly in patch_idct.cpp
///////////////////////////////////////////////////////////////////////////////

LLGroupHeader* gGOPP;

void set_group_of_patch_header(LLGroupHeader* gopp)
{
	gGOPP = gopp;
}

F32 gPatchDequantizeTable[LARGE_PATCH_SIZE * LARGE_PATCH_SIZE];
void build_patch_dequantize_table(S32 size)
{
	for (S32 j = 0; j < size; ++j)
	{
		for (S32 i = 0; i < size; ++i)
		{
			gPatchDequantizeTable[j * size + i] = (1.f + 2.f * (i + j));
		}
	}
}

S32	gCurrentDeSize = 0;

F32	gPatchICosines[LARGE_PATCH_SIZE * LARGE_PATCH_SIZE];

void setup_patch_icosines(S32 size)
{
	F32 oosob = F_PI * 0.5f / size;

	for (S32 u = 0; u < size; ++u)
	{
		for (S32 n = 0; n < size; ++n)
		{
			gPatchICosines[u * size + n] = cosf((2.f * n + 1.f) * u * oosob);
		}
	}
}

S32	gDeCopyMatrix[LARGE_PATCH_SIZE * LARGE_PATCH_SIZE];

void build_decopy_matrix(S32 size)
{
	bool b_diag = false;
	bool b_right = true;

	S32 i = 0;
	S32 j = 0;
	S32 count = 0;

	while (i < size && j < size)
	{
		gDeCopyMatrix[j * size + i] = count++;

		if (!b_diag)
		{
			if (b_right)
			{
				if (i < size - 1)
				{
					++i;
				}
				else
				{
					++j;
				}
				b_right = false;
				b_diag = true;
			}
			else
			{
				if (j < size - 1)
				{
					++j;
				}
				else
				{
					++i;
				}
				b_right = true;
				b_diag = true;
			}
		}
		else
		{
			if (b_right)
			{
				++i;
				--j;
				if (i == size - 1 || j == 0)
				{
					b_diag = false;
				}
			}
			else
			{
				--i;
				++j;
				if (i == 0 || j == size - 1)
				{
					b_diag = false;
				}
			}
		}
	}
}

void init_patch_decompressor(S32 size)
{
	if (size != gCurrentDeSize)
	{
		gCurrentDeSize = size;
		build_patch_dequantize_table(size);
		setup_patch_icosines(size);
		build_decopy_matrix(size);
	}
}

LL_INLINE void idct_line(F32* linein, F32* lineout, S32 line)
{
	F32 total;
	F32* pcp = gPatchICosines;

#ifdef _PATCH_SIZE_16_AND_32_ONLY
	F32 oosob = 2.f / 16.f;
	S32	line_size = line * NORMAL_PATCH_SIZE;
	F32* tlinein;
	F32* tpcp;

	for (S32 n = 0; n < NORMAL_PATCH_SIZE; ++n)
	{
		tpcp = pcp + n;
		tlinein = linein + line_size;

		total = OO_SQRT2 * (*(tlinein++));
		total += *(tlinein++) * (*(tpcp += NORMAL_PATCH_SIZE));
		total += *(tlinein++) * (*(tpcp += NORMAL_PATCH_SIZE));
		total += *(tlinein++) * (*(tpcp += NORMAL_PATCH_SIZE));

		total += *(tlinein++) * (*(tpcp += NORMAL_PATCH_SIZE));
		total += *(tlinein++) * (*(tpcp += NORMAL_PATCH_SIZE));
		total += *(tlinein++) * (*(tpcp += NORMAL_PATCH_SIZE));
		total += *(tlinein++) * (*(tpcp += NORMAL_PATCH_SIZE));

		total += *(tlinein++) * (*(tpcp += NORMAL_PATCH_SIZE));
		total += *(tlinein++) * (*(tpcp += NORMAL_PATCH_SIZE));
		total += *(tlinein++) * (*(tpcp += NORMAL_PATCH_SIZE));
		total += *(tlinein++) * (*(tpcp += NORMAL_PATCH_SIZE));

		total += *(tlinein++) * (*(tpcp += NORMAL_PATCH_SIZE));
		total += *(tlinein++) * (*(tpcp += NORMAL_PATCH_SIZE));
		total += *(tlinein++) * (*(tpcp += NORMAL_PATCH_SIZE));
		total += *(tlinein) * (*(tpcp += NORMAL_PATCH_SIZE));

		*(lineout + line_size + n) = total * oosob;
	}
#else
	F32 oosob = 2.f / size;
	S32 size = gGOPP->patch_size;
	S32 line_size = line * size;
	for (S32 n = 0; n < size; ++n)
	{
		total = OO_SQRT2*linein[line_size];
		for (S32 u = 1; u < size; ++u)
		{
			total += linein[line_size + u] * pcp[u * size + n];
		}
		lineout[line_size + n] = total * oosob;
	}
#endif
}

LL_INLINE void idct_line_large_slow(F32* linein, F32* lineout, S32 line)
{
	F32 total;
	F32* pcp = gPatchICosines;

	F32 oosob = 2.f / 32.f;
	S32	line_size = line * LARGE_PATCH_SIZE;
	F32* tlinein;
	F32* tpcp;

	for (S32 n = 0; n < LARGE_PATCH_SIZE; ++n)
	{
		tpcp = pcp + n;
		tlinein = linein + line_size;

		total = OO_SQRT2 * (*(tlinein++));
		total += *(tlinein++) * (*(tpcp += LARGE_PATCH_SIZE));
		total += *(tlinein++) * (*(tpcp += LARGE_PATCH_SIZE));
		total += *(tlinein++) * (*(tpcp += LARGE_PATCH_SIZE));

		total += *(tlinein++) * (*(tpcp += LARGE_PATCH_SIZE));
		total += *(tlinein++) * (*(tpcp += LARGE_PATCH_SIZE));
		total += *(tlinein++) * (*(tpcp += LARGE_PATCH_SIZE));
		total += *(tlinein++) * (*(tpcp += LARGE_PATCH_SIZE));

		total += *(tlinein++) * (*(tpcp += LARGE_PATCH_SIZE));
		total += *(tlinein++) * (*(tpcp += LARGE_PATCH_SIZE));
		total += *(tlinein++) * (*(tpcp += LARGE_PATCH_SIZE));
		total += *(tlinein++) * (*(tpcp += LARGE_PATCH_SIZE));

		total += *(tlinein++) * (*(tpcp += LARGE_PATCH_SIZE));
		total += *(tlinein++) * (*(tpcp += LARGE_PATCH_SIZE));
		total += *(tlinein++) * (*(tpcp += LARGE_PATCH_SIZE));
		total += *(tlinein++) * (*(tpcp += LARGE_PATCH_SIZE));

		total += *(tlinein++) * (*(tpcp += LARGE_PATCH_SIZE));
		total += *(tlinein++) * (*(tpcp += LARGE_PATCH_SIZE));
		total += *(tlinein++) * (*(tpcp += LARGE_PATCH_SIZE));
		total += *(tlinein++) * (*(tpcp += LARGE_PATCH_SIZE));

		total += *(tlinein++) * (*(tpcp += LARGE_PATCH_SIZE));
		total += *(tlinein++) * (*(tpcp += LARGE_PATCH_SIZE));
		total += *(tlinein++) * (*(tpcp += LARGE_PATCH_SIZE));
		total += *(tlinein++) * (*(tpcp += LARGE_PATCH_SIZE));

		total += *(tlinein++) * (*(tpcp += LARGE_PATCH_SIZE));
		total += *(tlinein++) * (*(tpcp += LARGE_PATCH_SIZE));
		total += *(tlinein++) * (*(tpcp += LARGE_PATCH_SIZE));
		total += *(tlinein++) * (*(tpcp += LARGE_PATCH_SIZE));

		total += *(tlinein++) * (*(tpcp += LARGE_PATCH_SIZE));
		total += *(tlinein++) * (*(tpcp += LARGE_PATCH_SIZE));
		total += *(tlinein++) * (*(tpcp += LARGE_PATCH_SIZE));
		total += *(tlinein)*(*(tpcp += LARGE_PATCH_SIZE));

		*(lineout + line_size + n) = total * oosob;
	}
}

// Nota Bene: assumes that coefficients beyond 128 are 0 !

void idct_line_large(F32* linein, F32* lineout, S32 line)
{
	F32 total;
	F32 *pcp = gPatchICosines;

	F32 oosob = 2.f / 32.f;
	S32	line_size = line*LARGE_PATCH_SIZE;
	F32* tlinein;
	F32* tpcp;
	F32* baselinein = linein + line_size;
	F32* baselineout = lineout + line_size;

	for (S32 n = 0; n < LARGE_PATCH_SIZE; ++n)
	{
		tpcp = pcp++;
		tlinein = baselinein;

		total = OO_SQRT2 * (*(tlinein++));
		total += *(tlinein++) * (*(tpcp += LARGE_PATCH_SIZE));
		total += *(tlinein++) * (*(tpcp += LARGE_PATCH_SIZE));
		total += *(tlinein++) * (*(tpcp += LARGE_PATCH_SIZE));

		total += *(tlinein++) * (*(tpcp += LARGE_PATCH_SIZE));
		total += *(tlinein++) * (*(tpcp += LARGE_PATCH_SIZE));
		total += *(tlinein++) * (*(tpcp += LARGE_PATCH_SIZE));
		total += *(tlinein++) * (*(tpcp += LARGE_PATCH_SIZE));

		total += *(tlinein++) * (*(tpcp += LARGE_PATCH_SIZE));
		total += *(tlinein++) * (*(tpcp += LARGE_PATCH_SIZE));
		total += *(tlinein++) * (*(tpcp += LARGE_PATCH_SIZE));
		total += *(tlinein++) * (*(tpcp += LARGE_PATCH_SIZE));

		total += *(tlinein++) * (*(tpcp += LARGE_PATCH_SIZE));
		total += *(tlinein++) * (*(tpcp += LARGE_PATCH_SIZE));
		total += *(tlinein++) * (*(tpcp += LARGE_PATCH_SIZE));
		total += *(tlinein) * (*(tpcp));

		*baselineout++ = total * oosob;
	}
}

LL_INLINE void idct_column(F32* linein, F32* lineout, S32 column)
{
	F32 total;
	F32 *pcp = gPatchICosines;

#ifdef _PATCH_SIZE_16_AND_32_ONLY
	F32* tlinein;
	F32* tpcp;

	for (S32 n = 0; n < NORMAL_PATCH_SIZE; ++n)
	{
		tpcp = pcp + n;
		tlinein = linein + column;

		total = OO_SQRT2*(*tlinein);
		total += *(tlinein += NORMAL_PATCH_SIZE) * (*(tpcp += NORMAL_PATCH_SIZE));
		total += *(tlinein += NORMAL_PATCH_SIZE) * (*(tpcp += NORMAL_PATCH_SIZE));
		total += *(tlinein += NORMAL_PATCH_SIZE) * (*(tpcp += NORMAL_PATCH_SIZE));

		total += *(tlinein += NORMAL_PATCH_SIZE) * (*(tpcp += NORMAL_PATCH_SIZE));
		total += *(tlinein += NORMAL_PATCH_SIZE) * (*(tpcp += NORMAL_PATCH_SIZE));
		total += *(tlinein += NORMAL_PATCH_SIZE) * (*(tpcp += NORMAL_PATCH_SIZE));
		total += *(tlinein += NORMAL_PATCH_SIZE) * (*(tpcp += NORMAL_PATCH_SIZE));

		total += *(tlinein += NORMAL_PATCH_SIZE) * (*(tpcp += NORMAL_PATCH_SIZE));
		total += *(tlinein += NORMAL_PATCH_SIZE) * (*(tpcp += NORMAL_PATCH_SIZE));
		total += *(tlinein += NORMAL_PATCH_SIZE) * (*(tpcp += NORMAL_PATCH_SIZE));
		total += *(tlinein += NORMAL_PATCH_SIZE) * (*(tpcp += NORMAL_PATCH_SIZE));

		total += *(tlinein += NORMAL_PATCH_SIZE) * (*(tpcp += NORMAL_PATCH_SIZE));
		total += *(tlinein += NORMAL_PATCH_SIZE) * (*(tpcp += NORMAL_PATCH_SIZE));
		total += *(tlinein += NORMAL_PATCH_SIZE) * (*(tpcp += NORMAL_PATCH_SIZE));
		total += *(tlinein += NORMAL_PATCH_SIZE) * (*(tpcp += NORMAL_PATCH_SIZE));

		*(lineout + (n<<4) + column) = total;
	}

#else
	S32	size = gGOPP->patch_size;
	S32 u;
	S32 u_size;

	for (n = 0; n < size; n++)
	{
		total = OO_SQRT2*linein[column];
		for (u = 1; u < size; u++)
		{
			u_size = u*size;
			total += linein[u_size + column]*pcp[u_size+n];
		}
		lineout[size*n + column] = total;
	}
#endif
}

LL_INLINE void idct_column_large_slow(F32* linein, F32* lineout, S32 column)
{
	F32 total;
	F32* pcp = gPatchICosines;

	F32* tlinein;
	F32* tpcp;

	for (S32 n = 0; n < LARGE_PATCH_SIZE; ++n)
	{
		tpcp = pcp + n;
		tlinein = linein + column;

		total = OO_SQRT2 * (*tlinein);
		total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp += LARGE_PATCH_SIZE));
		total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp += LARGE_PATCH_SIZE));
		total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp += LARGE_PATCH_SIZE));

		total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp += LARGE_PATCH_SIZE));
		total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp += LARGE_PATCH_SIZE));
		total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp += LARGE_PATCH_SIZE));
		total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp += LARGE_PATCH_SIZE));

		total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp += LARGE_PATCH_SIZE));
		total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp += LARGE_PATCH_SIZE));
		total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp += LARGE_PATCH_SIZE));
		total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp += LARGE_PATCH_SIZE));

		total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp += LARGE_PATCH_SIZE));
		total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp += LARGE_PATCH_SIZE));
		total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp += LARGE_PATCH_SIZE));
		total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp += LARGE_PATCH_SIZE));

		total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp += LARGE_PATCH_SIZE));
		total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp += LARGE_PATCH_SIZE));
		total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp += LARGE_PATCH_SIZE));
		total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp += LARGE_PATCH_SIZE));

		total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp += LARGE_PATCH_SIZE));
		total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp += LARGE_PATCH_SIZE));
		total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp += LARGE_PATCH_SIZE));
		total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp += LARGE_PATCH_SIZE));

		total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp += LARGE_PATCH_SIZE));
		total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp += LARGE_PATCH_SIZE));
		total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp += LARGE_PATCH_SIZE));
		total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp += LARGE_PATCH_SIZE));

		total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp += LARGE_PATCH_SIZE));
		total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp += LARGE_PATCH_SIZE));
		total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp += LARGE_PATCH_SIZE));
		total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp += LARGE_PATCH_SIZE));

		*(lineout + (n << 5) + column) = total;
	}
}

// Nota Bene: assumes that coefficients beyond 128 are 0!

void idct_column_large(F32* linein, F32* lineout, S32 column)
{
	F32 total;
	F32* pcp = gPatchICosines;

	F32* tlinein;
	F32* tpcp;
	F32* baselinein = linein + column;
	F32* baselineout = lineout + column;

	for (S32 n = 0; n < LARGE_PATCH_SIZE; ++n)
	{
		tpcp = pcp++;
		tlinein = baselinein;

		total = OO_SQRT2 * (*tlinein);
		for (S32 m = 1; m < NORMAL_PATCH_SIZE; ++m)
		{
			total += *(tlinein += LARGE_PATCH_SIZE) * (*(tpcp += LARGE_PATCH_SIZE));
		}

		*(baselineout + (n << 5)) = total;
	}
}

LL_INLINE void idct_patch(F32* block)
{
	F32 temp[LARGE_PATCH_SIZE * LARGE_PATCH_SIZE];

#ifdef _PATCH_SIZE_16_AND_32_ONLY
	idct_column(block, temp, 0);
	idct_column(block, temp, 1);
	idct_column(block, temp, 2);
	idct_column(block, temp, 3);

	idct_column(block, temp, 4);
	idct_column(block, temp, 5);
	idct_column(block, temp, 6);
	idct_column(block, temp, 7);

	idct_column(block, temp, 8);
	idct_column(block, temp, 9);
	idct_column(block, temp, 10);
	idct_column(block, temp, 11);

	idct_column(block, temp, 12);
	idct_column(block, temp, 13);
	idct_column(block, temp, 14);
	idct_column(block, temp, 15);

	idct_line(temp, block, 0);
	idct_line(temp, block, 1);
	idct_line(temp, block, 2);
	idct_line(temp, block, 3);

	idct_line(temp, block, 4);
	idct_line(temp, block, 5);
	idct_line(temp, block, 6);
	idct_line(temp, block, 7);

	idct_line(temp, block, 8);
	idct_line(temp, block, 9);
	idct_line(temp, block, 10);
	idct_line(temp, block, 11);

	idct_line(temp, block, 12);
	idct_line(temp, block, 13);
	idct_line(temp, block, 14);
	idct_line(temp, block, 15);
#else
	S32	size = gGOPP->patch_size;
	for (S32 i = 0; i < size; ++i)
	{
		idct_column(block, temp, i);
	}
	for (S32 i = 0; i < size; ++i)
	{
		idct_line(temp, block, i);
	}
#endif
}

LL_INLINE void idct_patch_large(F32* block)
{
	F32 temp[LARGE_PATCH_SIZE * LARGE_PATCH_SIZE];

	idct_column_large_slow(block, temp, 0);
	idct_column_large_slow(block, temp, 1);
	idct_column_large_slow(block, temp, 2);
	idct_column_large_slow(block, temp, 3);

	idct_column_large_slow(block, temp, 4);
	idct_column_large_slow(block, temp, 5);
	idct_column_large_slow(block, temp, 6);
	idct_column_large_slow(block, temp, 7);

	idct_column_large_slow(block, temp, 8);
	idct_column_large_slow(block, temp, 9);
	idct_column_large_slow(block, temp, 10);
	idct_column_large_slow(block, temp, 11);

	idct_column_large_slow(block, temp, 12);
	idct_column_large_slow(block, temp, 13);
	idct_column_large_slow(block, temp, 14);
	idct_column_large_slow(block, temp, 15);

	idct_column_large_slow(block, temp, 16);
	idct_column_large_slow(block, temp, 17);
	idct_column_large_slow(block, temp, 18);
	idct_column_large_slow(block, temp, 19);

	idct_column_large_slow(block, temp, 20);
	idct_column_large_slow(block, temp, 21);
	idct_column_large_slow(block, temp, 22);
	idct_column_large_slow(block, temp, 23);

	idct_column_large_slow(block, temp, 24);
	idct_column_large_slow(block, temp, 25);
	idct_column_large_slow(block, temp, 26);
	idct_column_large_slow(block, temp, 27);

	idct_column_large_slow(block, temp, 28);
	idct_column_large_slow(block, temp, 29);
	idct_column_large_slow(block, temp, 30);
	idct_column_large_slow(block, temp, 31);

	idct_line_large_slow(temp, block, 0);
	idct_line_large_slow(temp, block, 1);
	idct_line_large_slow(temp, block, 2);
	idct_line_large_slow(temp, block, 3);

	idct_line_large_slow(temp, block, 4);
	idct_line_large_slow(temp, block, 5);
	idct_line_large_slow(temp, block, 6);
	idct_line_large_slow(temp, block, 7);

	idct_line_large_slow(temp, block, 8);
	idct_line_large_slow(temp, block, 9);
	idct_line_large_slow(temp, block, 10);
	idct_line_large_slow(temp, block, 11);

	idct_line_large_slow(temp, block, 12);
	idct_line_large_slow(temp, block, 13);
	idct_line_large_slow(temp, block, 14);
	idct_line_large_slow(temp, block, 15);

	idct_line_large_slow(temp, block, 16);
	idct_line_large_slow(temp, block, 17);
	idct_line_large_slow(temp, block, 18);
	idct_line_large_slow(temp, block, 19);

	idct_line_large_slow(temp, block, 20);
	idct_line_large_slow(temp, block, 21);
	idct_line_large_slow(temp, block, 22);
	idct_line_large_slow(temp, block, 23);

	idct_line_large_slow(temp, block, 24);
	idct_line_large_slow(temp, block, 25);
	idct_line_large_slow(temp, block, 26);
	idct_line_large_slow(temp, block, 27);

	idct_line_large_slow(temp, block, 28);
	idct_line_large_slow(temp, block, 29);
	idct_line_large_slow(temp, block, 30);
	idct_line_large_slow(temp, block, 31);
}

S32	gDitherNoise = 128;

void decompress_patch(F32* patch, S32* cpatch, LLPatchHeader* ph)
{
	F32	block[LARGE_PATCH_SIZE * LARGE_PATCH_SIZE];
	F32* tblock = block;
	F32* tpatch;

	LLGroupHeader* gopp = gGOPP;
	S32 size = gopp->patch_size;
	F32 range = ph->range;
	S32 prequant = (ph->quant_wbits >> 4) + 2;
	S32 quantize = 1<<prequant;
	F32 hmin = ph->dc_offset;
	S32 stride = gopp->stride;

	F32	ooq = 1.f / (F32)quantize;
	F32* dq = gPatchDequantizeTable;
	S32* decopy_matrix = gDeCopyMatrix;

	F32	mult = ooq * range;
	F32	addval = mult * (F32)(1 << (prequant - 1)) + hmin;

	for (S32 i = 0; i < size*size; ++i)
	{
		*(tblock++) = *(cpatch + *(decopy_matrix++)) * (*dq++);
	}

	if (size == 16)
	{
		idct_patch(block);
	}
	else
	{
		idct_patch_large(block);
	}

	for (S32 j = 0; j < size; ++j)
	{
		tpatch = patch + j * stride;
		tblock = block + j * size;
		for (S32 i = 0; i < size; ++i)
		{
			*(tpatch++) = *(tblock++) * mult + addval;
		}
	}
}

void decompress_patchv(LLVector3* v, S32* cpatch, LLPatchHeader* ph)
{
	F32 block[LARGE_PATCH_SIZE * LARGE_PATCH_SIZE];
	F32* tblock = block;
	LLVector3* tvec;

	LLGroupHeader* gopp = gGOPP;
	S32 size = gopp->patch_size;
	F32 range = ph->range;
	S32 prequant = (ph->quant_wbits >> 4) + 2;
	S32 quantize = 1 << prequant;
	F32 hmin = ph->dc_offset;
	S32 stride = gopp->stride;

	F32 ooq = 1.f / (F32)quantize;
	F32* dq = gPatchDequantizeTable;
	S32* decopy_matrix = gDeCopyMatrix;

	F32 mult = ooq * range;
	F32 addval = mult*(F32)(1 << (prequant - 1)) + hmin;

	for (S32 i = 0; i < size * size; ++i)
	{
		*(tblock++) = *(cpatch + *(decopy_matrix++)) * (*dq++);
	}

	if (size == 16)
	{
		idct_patch(block);
	}
	else
	{
		idct_patch_large(block);
	}

	for (S32 j = 0; j < size; ++j)
	{
		tvec = v + j * stride;
		tblock = block + j*size;
		for (S32 i = 0; i < size; ++i)
		{
			(*tvec++).mV[VZ] = *(tblock++) * mult + addval;
		}
	}
}
