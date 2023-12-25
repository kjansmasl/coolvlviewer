/**
 * @file llmeshoptimizer.h
 * @brief Wrapper around the meshoptimizer library
 *
 * $LicenseInfo:firstyear=2021&license=viewergpl$
 *
 * Copyright (c) 2021, Linden Research, Inc.
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

#ifndef LL_MESHOPTIMIZER_H
#define LL_MESHOPTIMIZER_H

#include "stdtypes.h"

class LLVector2;
class LLVector4a;

// Purely static class
class LLMeshOptimizer
{
public:
	LLMeshOptimizer() = delete;
	~LLMeshOptimizer() = delete;

	static void generateShadowIndexBuffer16(U16* dest, const U16* indices,
											U64 idx_count,
											const LLVector4a* vert_pos,
											const LLVector4a* normals,
											const LLVector2* tex_coords,
											U64 vert_count);

	static void generateShadowIndexBuffer32(U32* dest, const U32* indices,
											U64 idx_count,
											const LLVector4a* vert_pos,
											const LLVector4a* normals,
											const LLVector2* tex_coords,
											U64 vert_count);

	// Remap methods welding indentical vertexes together and removing unused
	// vertices if indices were provided.

	static size_t generateRemapMulti16(U32* remap,
									   const U16* indices, U64 index_count,
									   const LLVector4a* vert_pos,
									   const LLVector4a* normals,
									   const LLVector2* tex_coords,
									   U64 vert_count);

	static size_t generateRemapMulti32(U32* remap,
									   const U32* indices, U64 index_count,
									   const LLVector4a* vert_pos,
									   const LLVector4a* normals,
									   const LLVector2* tex_coords,
									   U64 vert_count);

	static void remapIndexBuffer16(U16* dest, const U16* indices,
								   U64 index_count, const U32* remap);

	static void remapIndexBuffer32(U32* dest, const U32* indices,
								   U64 index_count, const U32* remap);

	// Works for both positions and normals vertex buffers.
	static void remapVertsBuffer(LLVector4a* dest, const LLVector4a* verts,
								 U64 count, const U32* remap);

	static void remapTexCoordsBuffer(LLVector2* dest, const LLVector2* tc,
									 U64 tc_count, const U32* remap);

	// Simplification methods returning the amount of indices in 'dest';
	// 'sloppy' engages a variant of an algorithm that does not respect
	// topology as much but is much more effective for simpler models. When
	// not NULL, 'result_error' returns how far from original the model is in
	// percents.

	static size_t simplify16(U16* dest, const U16* indices, U64 idx_count,
							 const LLVector4a* vert_pos, U64 vert_count,
							 U64 vert_pos_stride, U64 target_idx_count,
							 F32 target_error, bool sloppy, F32* result_error);

	static size_t simplify32(U32* dest, const U32* indices, U64 idx_count,
							 const LLVector4a* vert_pos, U64 vert_count,
							 U64 vert_pos_stride, U64 target_idx_count,
							 F32 target_error, bool sloppy, F32* result_error);
};

#endif	// LL_MESHOPTIMIZER_H
