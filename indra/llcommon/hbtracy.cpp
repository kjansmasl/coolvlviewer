/**
 * @file hbtracy.h
 * @brief Tracy profiler constants.
 *
 * $LicenseInfo:firstyear=2021&license=viewergpl$
 *
 * Copyright (c) 2021, Henri Beauchamp.
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

#include "hbtracy.h"

#if TRACY_ENABLE == 2 || TRACY_ENABLE == 4

// When TRACY_ENABLE is defined to 2 or 4, we also enable memory logging.
// Memory pools/allocation types must then be flagged with unique pointers to
// C strings. Here they are:
const char* trc_mem_align = "MEM_ALIGNED";
const char* trc_mem_align16 = "MEM_ALIGNED_16";
const char* trc_mem_image = "MEM_IMAGE";
const char* trc_mem_volume = "MEM_VOLUME_16";
const char* trc_mem_volume64 = "MEM_VOLUME_64";
const char* trc_mem_vertex = "MEM_VERTEX_BUFFER";

#else

// To avoid the LNK4221 warning under Windows while compiling...
# if LL_WINDOWS && !LL_CLANG
namespace
{
	void* dummy;
}
# endif

#endif
