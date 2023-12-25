/**
 * @file llfasttimer.h
 * @brief Declaration of a fast timer.
 *
 * $LicenseInfo:firstyear=2004&license=viewergpl$
 *
 * Copyright (c) 2004-2009, Linden Research, Inc.
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

#ifndef LL_LLFASTTIMER_H
#define LL_LLFASTTIMER_H

#include "hbtracy.h"

#if LL_FAST_TIMERS_ENABLED

#include "llthread.h"		// For is_main_thread()

// We only support the TSC for x86 CPUs, for now...
#if SSE2NEON
# define LL_FASTTIMER_USE_RDTSC 0
#else
# define LL_FASTTIMER_USE_RDTSC 1
#endif

# if LL_FASTTIMER_USE_RDTSC
// Include the right header for __rdtsc()
#  if LL_WINDOWS
#   include <intrin.h>
#  else
// Saddly, this header includes a shitload of other headers, which slows down
// the compilation of modules including llfasttimer.h: worked around, for cmake
// v3.16 and newer, by including <llfasttimer.h> in target_precompile_headers
// for all projects making use of llfasttimer.h in 2 or more of their modules.
#   include <x86intrin.h>
#  endif
# else
#  include "lltimer.h"		// For getCurrentClockCount()
# endif

// Define to 1 to determine the maximum depth (last check done in July 2021
// with 14 as the result). HB
# define LL_FAST_TIMERS_CHECK_MAX_DEPTH 0

// Maximum fast timers nesting (see the comment above)
constexpr S32 FTM_MAX_DEPTH = 20;

// Number of records in history
constexpr S32 FTM_HISTORY_NUM = 256;

class LLFastTimer
{
public:
	enum EFastTimerType
	{
		// High level
		FTM_FRAME,
		FTM_MEMORY_CHECK,
		FTM_RENDER,
		FTM_IDLE,
		FTM_POST_DISPLAY,

		// Common messaging components
		FTM_PUMP,
		FTM_PUMP_EVENT,
		FTM_PUMP_SERVICE,
		FTM_PUMP_IO,
		FTM_PROCESS_SOCKET_READER,
		FTM_PROCESS_SOCKET_WRITER,
		FTM_PROCESS_SERVER_SOCKET,
		FTM_PUMP_CALLBACK_CHAIN,

		// Common simulation components
		FTM_UPDATE_ANIMATION,
		FTM_UPDATE_HIDDEN_ANIMATION,
		FTM_UPDATE_MOTIONS,
		FTM_MOTION_ON_UPDATE,
		FTM_APPLY_MORPH_TARGET,
		FTM_POLYSKELETAL_DISTORTION_APPLY,
		FTM_UPDATE_TERRAIN,
		FTM_UPDATE_PRIMITIVES,
		FTM_UPDATE_PARTICLES,
		FTM_SIMULATE_PARTICLES,
		FTM_SIM_PART_SORT,
		FTM_UPDATE_SKY,
		FTM_UPDATE_TEXTURES,
		FTM_UPDATE_WLPARAM,
		FTM_UPDATE_WATER,
		FTM_UPDATE_CLOUDS,
		FTM_UPDATE_GRASS,
		FTM_UPDATE_TREE,

		// Common render components
		FTM_IMPOSTORS_UPDATE,
		FTM_IMPOSTOR_MARK_VISIBLE,
		FTM_IMPOSTOR_SETUP,
		FTM_IMPOSTOR_ALLOCATE,
		FTM_IMPOSTOR_RESIZE,
		FTM_IMPOSTOR_BACKGROUND,

		FTM_GEN_SUN_SHADOW,
		FTM_BIND_DEFERRED,
		FTM_RENDER_DEFERRED,
		FTM_ATMOSPHERICS,
		FTM_SUN_SHADOW,
		FTM_SOFTEN_SHADOW,
		FTM_LOCAL_LIGHTS,
		FTM_PROJECTORS,
		FTM_FULLSCREEN_LIGHTS,
		FTM_SHADOW_RENDER,
		FTM_SHADOW_TERRAIN,
		FTM_SHADOW_AVATAR,
		FTM_SHADOW_SIMPLE,
		FTM_SHADOW_ALPHA,
		FTM_SHADOW_TREE,

		FTM_RENDER_GEOMETRY,
		FTM_RENDER_TERRAIN,
		FTM_AVATAR_FACE,
		FTM_RENDER_SIMPLE,
		FTM_RENDER_FULLBRIGHT,
		FTM_RENDER_GLOW,
		FTM_RENDER_GRASS,
		FTM_RENDER_INVISIBLE,
		FTM_RENDER_SHINY,
		FTM_RENDER_BUMP,
		FTM_RENDER_MATERIALS,
		FTM_RENDER_TREES,
		FTM_VOLUME_GEOM,
		FTM_FACE_GET_GEOM,
		FTM_FACE_GEOM_INDEX,
		FTM_FACE_GEOM_POSITION,
		FTM_FACE_GEOM_COLOR,
		FTM_FACE_GEOM_EMISSIVE,
		FTM_FACE_GEOM_NORMAL,
		FTM_FACE_GEOM_TANGENT,
		FTM_FACE_GEOM_WEIGHTS,
		FTM_FACE_GEOM_TEXTURE,
		FTM_RENDER_CHARACTERS,
		FTM_RENDER_AVATARS,
		FTM_RIGGED_VBO,
		FTM_RENDER_OCCLUSION,
		FTM_OCCLUSION_ALLOCATE,
		FTM_PUSH_OCCLUSION_VERTS,
		FTM_OCCLUSION_BEGIN_QUERY,
		FTM_OCCLUSION_DRAW_WATER,
		FTM_OCCLUSION_DRAW,
		FTM_OCCLUSION_END_QUERY,
		FTM_RENDER_ALPHA,
		FTM_RENDER_CLOUDS,
		FTM_RENDER_WATER,
		FTM_RENDER_WL_SKY,
		FTM_VISIBLE_CLOUD,
		FTM_RENDER_TIMER,
		FTM_RENDER_UI,
		FTM_RENDER_SPELLCHECK,
		FTM_REBUILD_GROUPS,
		FTM_RESET_VB,
		FTM_RENDER_BLOOM,
		FTM_RENDER_FONTS_BATCHED,
		FTM_RENDER_FONTS_SERIALIZED,
		FTM_RESIZE_SCREEN_TEXTURE,
		FTM_UPDATE_GL,

		// newview specific
		FTM_MESSAGES,
		FTM_MOUSEHANDLER,
		FTM_KEYHANDLER,
		FTM_STATESORT,
		FTM_STATESORT_DRAWABLE,
		FTM_STATESORT_POSTSORT,
		FTM_REBUILD_PRIORITY_GROUPS,
		FTM_REBUILD_MESH,
		FTM_REBUILD_VBO,
		FTM_ADD_GEOMETRY_COUNT,
		FTM_CREATE_VB,
		FTM_GET_GEOMETRY,
		FTM_REBUILD_VOLUME_FACE_LIST,
		FTM_VOLUME_TEXTURES,
		FTM_REBUILD_VOLUME_GEN_DRAW_INFO,
		FTM_GEN_DRAW_INFO_SORT,
		FTM_GEN_DRAW_INFO_FACE_SIZE,
		FTM_REGISTER_FACE,
		FTM_REBUILD_GRASS_VB,
		FTM_REBUILD_TERRAIN_VB,
		FTM_REBUILD_PARTICLE_VBO,
		FTM_REBUILD_PARTICLE_GEOM,
		FTM_POOLS,
		FTM_POOLRENDER,
		FTM_IDLE_CB,
		FTM_MEDIA_UPDATE,
		FTM_MEDIA_UPDATE_INTEREST,
		FTM_MEDIA_CALCULATE_INTEREST,
		FTM_MEDIA_SORT,
		FTM_MEDIA_MISC,
		FTM_MEDIA_SORT2,
		FTM_MEDIA_GET_DATA,
		FTM_MEDIA_SET_SUBIMAGE,
		FTM_MEDIA_DO_UPDATE,
		FTM_MATERIALS_IDLE,
		FTM_IDLE_CB_RADAR,
		FTM_WORLD_UPDATE,
		FTM_UPDATE_MOVE,
		FTM_OCTREE_BALANCE,
		FTM_CULL,
		FTM_CULL_VOCACHE,
		FTM_CULL_REBOUND,
		FTM_FRUSTUM_CULL,
		FTM_OCCLUSION_EARLY_FAIL,
		FTM_DISPLAY_UPDATE_GEOM,
		FTM_GEO_UPDATE,
		FTM_GEO_SKY,
		FTM_GEN_VOLUME,
		FTM_GEN_TRIANGLES,
		FTM_GEN_FLEX,
		FTM_DO_FLEXIBLE_UPDATE,
		FTM_FLEXIBLE_REBUILD,
		FTM_PROCESS_PARTITIONQ,
		FTM_PIPELINE_CREATE,
		FTM_AUDIO_UPDATE,
		FTM_RESET_DRAWORDER,
		FTM_OBJECTLIST_UPDATE,
		FTM_OBJECTLIST_COPY,
		FTM_AVATAR_UPDATE,
		FTM_AV_CHECK_TEX_LOADING,
		FTM_AV_RELEASE_OLD_TEX,
		FTM_AV_UPDATE_TEXTURES,
		FTM_JOINT_UPDATE,
		FTM_PHYSICS_UPDATE,
		FTM_ATTACHMENT_UPDATE,
		FTM_LOD_UPDATE,
		FTM_CULL_AVATARS,
		FTM_UPDATE_RIGGED_VOLUME,
		FTM_RIGGED_OCTREE,
		FTM_AREASEARCH_UPDATE,
		FTM_REGION_UPDATE,
		FTM_UPD_LANDPATCHES,
		FTM_UPD_PARCELOVERLAY,
		FTM_UPD_CACHEDOBJECTS,
		FTM_CLEANUP,
		FTM_CLEANUP_DRAWABLE,
		FTM_UNLINK,
		FTM_REMOVE_FROM_LIGHT_SET,
		FTM_REMOVE_FROM_MOVE_LIST,
		FTM_REMOVE_FROM_SPATIAL_PARTITION,
		FTM_RLV,
		FTM_IDLE_LUA_THREAD,
		FTM_NETWORK,
		FTM_IDLE_NETWORK,
		FTM_CREATE_OBJECT,
//		FTM_LOAD_AVATAR,
		FTM_PROCESS_MESSAGES,
		FTM_PROCESS_OBJECTS,
		FTM_PROCESS_IMAGES,
		FTM_SHIFT_OBJECTS,
		FTM_PIPELINE_SHIFT,
		FTM_SHIFT_DRAWABLE,
		FTM_SHIFT_OCTREE,
		FTM_SHIFT_HUD,
		FTM_REGION_SHIFT,
		FTM_IMAGE_UPDATE,
		FTM_IMAGE_UPDATE_CLASS,
		FTM_IMAGE_UPDATE_BUMP,
		FTM_IMAGE_UPDATE_LIST,
		FTM_IMAGE_CALLBACKS,
		FTM_BUMP_SOURCE_STANDARD_LOADED,
		FTM_BUMP_GEN_NORMAL,
		FTM_BUMP_CREATE_TEXTURE,
		FTM_BUMP_SOURCE_LOADED,
		FTM_BUMP_SOURCE_ENTRIES_UPDATE,
		FTM_BUMP_SOURCE_MIN_MAX,
		FTM_BUMP_SOURCE_RGB2LUM,
		FTM_BUMP_SOURCE_RESCALE,
		FTM_BUMP_SOURCE_CREATE,
		FTM_BUMP_SOURCE_GEN_NORMAL,
		FTM_IMAGE_CREATE,
		FTM_IMAGE_UPDATE_PRIO,
		FTM_IMAGE_FETCH,
		FTM_IMAGE_MARK_DIRTY,
		FTM_IMAGE_STATS,
		FTM_TEXTURE_UNBIND,

		FTM_VFILE_WAIT,
		FTM_FLEXIBLE_UPDATE,
		FTM_OCCLUSION_WAIT,
		FTM_OCCLUSION_READBACK,
		FTM_SET_OCCLUSION_STATE,
		FTM_HUD_UPDATE,
		FTM_HUD_EFFECTS,
		FTM_HUD_OBJECTS,
		FTM_SWAP,
		FTM_INVENTORY,
		FTM_AUTO_SELECT,
		FTM_ARRANGE,
		FTM_FILTER,
		FTM_REFRESH,
		FTM_SORT,
		FTM_PICK,
		FTM_TEXTURE_CACHE,
		FTM_DECODE,
		FTM_SLEEP,
		FTM_FPS_LIMITING,
		FTM_FETCH,

		FTM_OTHER,			// Special, used by display code

		FTM_NUM_TYPES
	};

public:
	LLFastTimer() = delete;
	LLFastTimer(const LLFastTimer&) = delete;

	LL_INLINE LLFastTimer(EFastTimerType type) noexcept
	{
		mActive = sFastTimersEnabled && is_main_thread() &&
				  sCurDepth < FTM_MAX_DEPTH;
		if (mActive)
		{
			sType[sCurDepth] = sCurType;	// Store the previous type
			sCurType = type;
			sStart[sCurDepth++] = getCPUClockCount();
#if LL_FAST_TIMERS_CHECK_MAX_DEPTH
			checkMaxDepth();
#endif
		}
	}

	LL_INLINE ~LLFastTimer()
	{
		if (mActive)
		{
			U64 delta = getCPUClockCount() - sStart[--sCurDepth];
			sCounter[sCurType] += delta;
			++sCalls[sCurType];
			sCurType = sType[sCurDepth];	// Restore the previous type

			// Subtract delta from parents
			for (S32 i = 0; i < sCurDepth; ++i)
			{
				sStart[i] += delta;
			}
		}
	}

	static void enabledFastTimers(bool enable);
	static bool fastTimersEnabled()				{ return sFastTimersEnabled; }

	static void reset();

	static U64 countsPerSecond()				{ return sClockResolution; }

private:
	LL_INLINE static U64 getCPUClockCount()
	{
#if LL_FASTTIMER_USE_RDTSC	// Fast, TSC-based implementation.
# if 0	// Useless for the ~1µs resolution we need.
		_mm_lfence();
# endif
		return (U64)__rdtsc();
#else						// Slower, non TSC function.
		return LLTimer::getCurrentClockCount();
#endif
	}

#if LL_FAST_TIMERS_CHECK_MAX_DEPTH
	LL_NO_INLINE static void checkMaxDepth();
#endif

private:
	bool					mActive;

	static U64				sClockResolution;
	static EFastTimerType	sType[FTM_MAX_DEPTH];
	static EFastTimerType	sCurType;
	static S32				sCurDepth;
	static U64				sStart[FTM_MAX_DEPTH];
#if LL_FAST_TIMERS_CHECK_MAX_DEPTH
	static S32				sMaxDepth;
#endif
	static bool				sFastTimersEnabled;

public:
	static bool				sPauseHistory;
	static bool				sResetHistory;
	static S32				sCurFrameIndex;
	static S32				sLastFrameIndex;
	static U64				sCounter[FTM_NUM_TYPES];
	static U64				sCalls[FTM_NUM_TYPES];
	static U64				sCountAverage[FTM_NUM_TYPES];
	static U64				sCallAverage[FTM_NUM_TYPES];
	static U64				sCountHistory[FTM_HISTORY_NUM][FTM_NUM_TYPES];
	static U64				sCallHistory[FTM_HISTORY_NUM][FTM_NUM_TYPES];
};

#endif 	// LL_FAST_TIMERS_ENABLED

#endif // LL_LLFASTTIMER_H
