/**
 * @file llfasttimerview.cpp
 * @brief LLFastTimerView class implementation
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

#include "llviewerprecompiledheaders.h"

#include "llfasttimerview.h"

#if TRACY_ENABLE
# include "lldir.h"
# include "llprocesslauncher.h"
#endif

#if LL_FAST_TIMERS_ENABLED

#include "llgl.h"
#include "llrender.h"

#include "llappviewer.h"
#include "llviewercontrol.h"
#include "llviewertexturelist.h"
#include "llviewerwindow.h"

LLFastTimerView* gFastTimerViewp = NULL;

constexpr S32 MAX_VISIBLE_HISTORY = 10;
constexpr S32 LINE_GRAPH_HEIGHT = 240;

constexpr S32 FASTTIMERVIEW_LEFT = 50;
constexpr S32 FASTTIMERVIEW_TOP_DELTA = 50;

struct ft_display_info
{
	S32				timer;
	const char*		desc;
	const LLColor4*	color;		// Auto-initialized
	S32				disabled;	// Auto-initialized
	S32				level;		// Calculated based on leading spaces in desc
	S32				parent;		// Calculated
};

static struct ft_display_info ft_display_table[] =
{
	{ LLFastTimer::FTM_FRAME,							"Frame" },
	{ LLFastTimer::FTM_MEMORY_CHECK,					" Memory check" },
	{ LLFastTimer::FTM_MESSAGES,						" System Messages" },
#if LL_WINDOWS	// Not used for Linux and Darwin
	{ LLFastTimer::FTM_MOUSEHANDLER,					"  Mouse" },
	{ LLFastTimer::FTM_KEYHANDLER,						"  Keyboard" },
#endif
	{ LLFastTimer::FTM_POST_DISPLAY,					" Post-display" },
	{ LLFastTimer::FTM_FETCH,							"  Texture Fetch" },
	{ LLFastTimer::FTM_TEXTURE_CACHE,					"  Texture Cache" },
	{ LLFastTimer::FTM_DECODE,							"  Texture Decode" },
	{ LLFastTimer::FTM_FPS_LIMITING,					"  FPS limiting" },
	{ LLFastTimer::FTM_SLEEP,							"  Sleep" },
	{ LLFastTimer::FTM_IDLE,							" Idle" },
	{ LLFastTimer::FTM_RLV,								"  Restrained Love" },
	{ LLFastTimer::FTM_IDLE_LUA_THREAD,					"  Lua threads" },
	{ LLFastTimer::FTM_INVENTORY,						"  Inventory Update" },
	{ LLFastTimer::FTM_AUTO_SELECT,						"   Open and Select" },
	{ LLFastTimer::FTM_FILTER,							"   Filter" },
	{ LLFastTimer::FTM_ARRANGE,							"   Arrange" },
	{ LLFastTimer::FTM_REFRESH,							"   Refresh" },
	{ LLFastTimer::FTM_SORT,							"   Sort" },
	{ LLFastTimer::FTM_RESET_DRAWORDER,					"  ResetDrawOrder" },
	{ LLFastTimer::FTM_WORLD_UPDATE,					"  World Update" },
	{ LLFastTimer::FTM_UPDATE_MOVE,						"   Move Objects" },
	{ LLFastTimer::FTM_OCTREE_BALANCE,					"    Octree Balance" },
	{ LLFastTimer::FTM_SIMULATE_PARTICLES,				"   Particle Sim" },
	{ LLFastTimer::FTM_SIM_PART_SORT,					"    Particle Sim Sort" },
	{ LLFastTimer::FTM_OBJECTLIST_UPDATE,				"  Object List Update" },
	{ LLFastTimer::FTM_OBJECTLIST_COPY,					"   Object List Copy" },
	{ LLFastTimer::FTM_AVATAR_UPDATE,					"   Avatars" },
	{ LLFastTimer::FTM_AV_CHECK_TEX_LOADING,			"    Check Loading Tex." },
	{ LLFastTimer::FTM_AV_RELEASE_OLD_TEX,				"     Release Old Tex." },
	{ LLFastTimer::FTM_AV_UPDATE_TEXTURES,				"     Update Textures" },
	{ LLFastTimer::FTM_JOINT_UPDATE,					"    Joints" },
	{ LLFastTimer::FTM_PHYSICS_UPDATE,					"    Physics" },
	{ LLFastTimer::FTM_ATTACHMENT_UPDATE,				"    Attachments" },
	{ LLFastTimer::FTM_UPDATE_ANIMATION,				"    Animation" },
	{ LLFastTimer::FTM_UPDATE_MOTIONS,					"     Motions" },
	{ LLFastTimer::FTM_MOTION_ON_UPDATE,				"      On Update" },
	{ LLFastTimer::FTM_APPLY_MORPH_TARGET,				"       Apply Morph" },
	{ LLFastTimer::FTM_POLYSKELETAL_DISTORTION_APPLY,	"       Skel Distortion" },
	{ LLFastTimer::FTM_UPDATE_HIDDEN_ANIMATION,			"    Hidden Anim" },
	{ LLFastTimer::FTM_FLEXIBLE_UPDATE,					"   Flex Update" },
	{ LLFastTimer::FTM_LOD_UPDATE,						"   LOD Update" },
	{ LLFastTimer::FTM_CULL_AVATARS,					"    Cull Avatars" },
	{ LLFastTimer::FTM_UPDATE_RIGGED_VOLUME,			"   Update Rigged" },
	{ LLFastTimer::FTM_RIGGED_OCTREE,					"    Octree" },
	{ LLFastTimer::FTM_CLEANUP,							"  Cleanup" },
	{ LLFastTimer::FTM_CLEANUP_DRAWABLE,				"   Cleanup Drawable" },
	{ LLFastTimer::FTM_UNLINK,							"    Unlink" },
	{ LLFastTimer::FTM_REMOVE_FROM_LIGHT_SET,			"     Light Set" },
	{ LLFastTimer::FTM_REMOVE_FROM_MOVE_LIST,			"     MoveList" },
	{ LLFastTimer::FTM_REMOVE_FROM_SPATIAL_PARTITION,	"     Spatial Part." },
	{ LLFastTimer::FTM_AREASEARCH_UPDATE,				"  Area Search Update" },
	{ LLFastTimer::FTM_REGION_UPDATE,					"  Region Update" },
	{ LLFastTimer::FTM_UPD_LANDPATCHES,					"   Land Patches" },
	{ LLFastTimer::FTM_UPD_PARCELOVERLAY,				"   Parcel Overlays" },
	{ LLFastTimer::FTM_UPD_CACHEDOBJECTS,				"   Cached Objects" },
	{ LLFastTimer::FTM_NETWORK,							"  Network" },
	{ LLFastTimer::FTM_IDLE_NETWORK,					"   Decode Msgs" },
	{ LLFastTimer::FTM_PROCESS_MESSAGES,				"    Process Msgs" },
	{ LLFastTimer::FTM_PROCESS_OBJECTS,					"     Process Objects" },
	{ LLFastTimer::FTM_CREATE_OBJECT,					"      Create Obj" },
//	{ LLFastTimer::FTM_LOAD_AVATAR,						"       Load Avatar" },
	{ LLFastTimer::FTM_PROCESS_IMAGES,					"     Image Updates" },
	{ LLFastTimer::FTM_SHIFT_OBJECTS,					"     Shift Objects" },
	{ LLFastTimer::FTM_PIPELINE_SHIFT,					"      Pipeline Shift" },
	{ LLFastTimer::FTM_SHIFT_DRAWABLE,					"       Shift Drawable" },
	{ LLFastTimer::FTM_SHIFT_OCTREE,					"       Shift Octree" },
	{ LLFastTimer::FTM_SHIFT_HUD,						"       Shift HUD" },
	{ LLFastTimer::FTM_REGION_SHIFT,					"      Region Shift" },
	{ LLFastTimer::FTM_PUMP,							"  Pump" },
	{ LLFastTimer::FTM_PUMP_EVENT,						"   Events" },
	{ LLFastTimer::FTM_PUMP_SERVICE,					"   Service" },
	{ LLFastTimer::FTM_PUMP_IO,							"  Pump IO" },
	{ LLFastTimer::FTM_PROCESS_SOCKET_READER,			"   Socket Reader" },
	{ LLFastTimer::FTM_PROCESS_SOCKET_WRITER,			"   Socket Writer" },
	{ LLFastTimer::FTM_PROCESS_SERVER_SOCKET,			"   Server Socket" },
	{ LLFastTimer::FTM_PUMP_CALLBACK_CHAIN,				"   Chain" },
	{ LLFastTimer::FTM_AUDIO_UPDATE,					"  Audio Update" },
	{ LLFastTimer::FTM_VFILE_WAIT,						"  VFile Wait" },
	{ LLFastTimer::FTM_IDLE_CB,							"  Callbacks" },
	{ LLFastTimer::FTM_MEDIA_UPDATE,					"   Media Updates" },
	{ LLFastTimer::FTM_MEDIA_UPDATE_INTEREST,			"    Update Interest" },
	{ LLFastTimer::FTM_MEDIA_DO_UPDATE,					"     Impl. Update" },
	{ LLFastTimer::FTM_MEDIA_GET_DATA,					"      Get Data" },
	{ LLFastTimer::FTM_MEDIA_SET_SUBIMAGE,				"      Set Sub-image" },
	{ LLFastTimer::FTM_MEDIA_CALCULATE_INTEREST,		"     Compute Interest" },
	{ LLFastTimer::FTM_MEDIA_SORT,						"    Priority Sorting" },
	{ LLFastTimer::FTM_MEDIA_MISC,						"    Miscellaneous" },
	{ LLFastTimer::FTM_MEDIA_SORT2,						"    Distance Sorting" },
	{ LLFastTimer::FTM_MATERIALS_IDLE,					"   Materials Updates" },
	{ LLFastTimer::FTM_IDLE_CB_RADAR,					"   Radar Updates" },
	{ LLFastTimer::FTM_RENDER,							" Render" },
	{ LLFastTimer::FTM_PICK,							"  Pick" },
	{ LLFastTimer::FTM_HUD_UPDATE,						"  HUD Update" },
	{ LLFastTimer::FTM_HUD_EFFECTS,						"   HUD Effects" },
	{ LLFastTimer::FTM_HUD_OBJECTS,						"   HUD Objects" },
	{ LLFastTimer::FTM_IMPOSTORS_UPDATE,				"  Impostors Update" },
	{ LLFastTimer::FTM_IMPOSTOR_MARK_VISIBLE,			"   Imp. Mark Visible" },
	{ LLFastTimer::FTM_IMPOSTOR_SETUP,					"   Impostor Setup" },
	{ LLFastTimer::FTM_IMPOSTOR_ALLOCATE,				"    Impostor Allocate" },
	{ LLFastTimer::FTM_IMPOSTOR_RESIZE,					"    Impostor Resize" },
	{ LLFastTimer::FTM_IMPOSTOR_BACKGROUND,				"   Impostor Background" },
	{ LLFastTimer::FTM_UPDATE_SKY,						"  Sky Update" },
	{ LLFastTimer::FTM_UPDATE_TEXTURES,					"  Textures" },
	{ LLFastTimer::FTM_DISPLAY_UPDATE_GEOM,				"  Update Geometry" },
	{ LLFastTimer::FTM_GEO_UPDATE,						"   Geo Update" },
	{ LLFastTimer::FTM_UPDATE_PRIMITIVES,				"    Volumes" },
	{ LLFastTimer::FTM_GEN_VOLUME,						"     Gen Volume" },
	{ LLFastTimer::FTM_GEN_FLEX,						"     Flexible" },
	{ LLFastTimer::FTM_DO_FLEXIBLE_UPDATE,				"      Update" },
	{ LLFastTimer::FTM_FLEXIBLE_REBUILD,				"      Rebuild" },
	{ LLFastTimer::FTM_GEN_TRIANGLES,					"     Triangles" },
	{ LLFastTimer::FTM_UPDATE_TREE,						"    Tree" },
	{ LLFastTimer::FTM_UPDATE_TERRAIN,					"    Terrain" },
	{ LLFastTimer::FTM_UPDATE_CLOUDS,					"    Clouds" },
	{ LLFastTimer::FTM_UPDATE_GRASS,					"    Grass" },
	{ LLFastTimer::FTM_UPDATE_WATER,					"    Water" },
	{ LLFastTimer::FTM_UPDATE_PARTICLES,				"    Particles" },
	{ LLFastTimer::FTM_GEO_SKY,							"    Sky" },
	{ LLFastTimer::FTM_PROCESS_PARTITIONQ,				"   PartitionQ" },
	{ LLFastTimer::FTM_PIPELINE_CREATE,					"   Pipeline Create" },
	{ LLFastTimer::FTM_UPDATE_WLPARAM,					"  Windlight Param" },
	{ LLFastTimer::FTM_CULL,							"  Object Cull" },
	{ LLFastTimer::FTM_CULL_VOCACHE,					"   Cull VO Cache" },
	{ LLFastTimer::FTM_CULL_REBOUND,					"   Rebound" },
	{ LLFastTimer::FTM_FRUSTUM_CULL,					"   Frustum Cull" },
	{ LLFastTimer::FTM_OCCLUSION_EARLY_FAIL,			"    Occl. Early Fail" },
	{ LLFastTimer::FTM_OCCLUSION_WAIT,					"   Occlusion Wait" },
	{ LLFastTimer::FTM_OCCLUSION_READBACK,				"   Occlusion Read" },
	{ LLFastTimer::FTM_SET_OCCLUSION_STATE,				"   Occlusion State" },
	{ LLFastTimer::FTM_IMAGE_UPDATE,					"  Image Update" },
	{ LLFastTimer::FTM_IMAGE_UPDATE_CLASS,				"   Image Class" },
	{ LLFastTimer::FTM_IMAGE_UPDATE_BUMP,				"   Image Bump" },
	{ LLFastTimer::FTM_IMAGE_UPDATE_LIST,				"   Image List" },
	{ LLFastTimer::FTM_IMAGE_CALLBACKS,					"    Image Callbacks" },
	{ LLFastTimer::FTM_BUMP_SOURCE_STANDARD_LOADED,		"     Bump Std Loaded" },
	{ LLFastTimer::FTM_BUMP_GEN_NORMAL,					"      Gen. Normal Map" },
	{ LLFastTimer::FTM_BUMP_CREATE_TEXTURE,				"      Create GL N. Map" },
	{ LLFastTimer::FTM_BUMP_SOURCE_LOADED,				"     Bump Src Loaded" },
	{ LLFastTimer::FTM_BUMP_SOURCE_ENTRIES_UPDATE,		"      Entries Update" },
	{ LLFastTimer::FTM_BUMP_SOURCE_MIN_MAX,				"      Min/Max" },
	{ LLFastTimer::FTM_BUMP_SOURCE_RGB2LUM,				"      RGB to Luminance" },
	{ LLFastTimer::FTM_BUMP_SOURCE_RESCALE,				"      Rescale" },
	{ LLFastTimer::FTM_BUMP_SOURCE_CREATE,				"      Create" },
	{ LLFastTimer::FTM_BUMP_SOURCE_GEN_NORMAL,			"      Generate Normal" },
	{ LLFastTimer::FTM_IMAGE_CREATE,					"   Image CreateGL" },
	{ LLFastTimer::FTM_IMAGE_UPDATE_PRIO,				"   Prioritize Images" },
	{ LLFastTimer::FTM_IMAGE_FETCH,						"   Fetch Images" },
	{ LLFastTimer::FTM_IMAGE_MARK_DIRTY,				"   Dirty Images" },
	{ LLFastTimer::FTM_IMAGE_STATS,						"   Image Stats" },
	{ LLFastTimer::FTM_TEXTURE_UNBIND,					"  Texture Unbind" },
	{ LLFastTimer::FTM_STATESORT,						"  State Sort" },
	{ LLFastTimer::FTM_STATESORT_DRAWABLE,				"   Drawable" },
	{ LLFastTimer::FTM_STATESORT_POSTSORT,				"   Post Sort" },
	{ LLFastTimer::FTM_REBUILD_PRIORITY_GROUPS,			"    Rebuild Prio. Grps" },
	{ LLFastTimer::FTM_REBUILD_MESH,					"     Rebuild Mesh Obj." },
	{ LLFastTimer::FTM_REBUILD_VBO,						"    VBO Rebuild" },
	{ LLFastTimer::FTM_ADD_GEOMETRY_COUNT,				"     Add Geometry" },
	{ LLFastTimer::FTM_CREATE_VB,						"     Create VB" },
	{ LLFastTimer::FTM_GET_GEOMETRY,					"     Get Geometry" },
	{ LLFastTimer::FTM_REBUILD_VOLUME_FACE_LIST,		"      Build Face List" },
	{ LLFastTimer::FTM_VOLUME_TEXTURES,					"       Volume Textures" },
	{ LLFastTimer::FTM_REBUILD_VOLUME_GEN_DRAW_INFO,	"      Gen Draw Info" },
	{ LLFastTimer::FTM_GEN_DRAW_INFO_SORT,				"       Face Sort" },
	{ LLFastTimer::FTM_GEN_DRAW_INFO_FACE_SIZE,			"       Face Sizing" },
	{ LLFastTimer::FTM_REGISTER_FACE,					"       Register Face" },
	{ LLFastTimer::FTM_REBUILD_TERRAIN_VB,				"      Terrain" },
	{ LLFastTimer::FTM_REBUILD_GRASS_VB,				"      Grass" },
	{ LLFastTimer::FTM_REBUILD_PARTICLE_VBO,			"     Particle VB0" },
	{ LLFastTimer::FTM_REBUILD_PARTICLE_GEOM,			"      Get Geometry" },
 	{ LLFastTimer::FTM_GEN_SUN_SHADOW,					"  Gen Sun Shadow" },
 	{ LLFastTimer::FTM_BIND_DEFERRED,					"  Bind Deferred" },
 	{ LLFastTimer::FTM_RENDER_DEFERRED,					"  Deferred Shading" },
 	{ LLFastTimer::FTM_ATMOSPHERICS,					"   Atmospherics" },
 	{ LLFastTimer::FTM_SUN_SHADOW,						"   Shadow Map" },
 	{ LLFastTimer::FTM_SOFTEN_SHADOW,					"   Shadow Soften" },
 	{ LLFastTimer::FTM_LOCAL_LIGHTS,					"   Local Lights" },
 	{ LLFastTimer::FTM_PROJECTORS,						"   Projectors" },
 	{ LLFastTimer::FTM_FULLSCREEN_LIGHTS,				"   Full Screen Lights" },
 	{ LLFastTimer::FTM_SHADOW_RENDER,					"  Shadow" },
	{ LLFastTimer::FTM_SHADOW_SIMPLE,					"   Simple" },
	{ LLFastTimer::FTM_SHADOW_ALPHA,					"   Alpha" },
	{ LLFastTimer::FTM_SHADOW_TERRAIN,					"   Terrain" },
	{ LLFastTimer::FTM_SHADOW_AVATAR,					"   Avatar" },
	{ LLFastTimer::FTM_SHADOW_TREE,						"   Tree" },
	{ LLFastTimer::FTM_RENDER_GEOMETRY,					"  Geometry" },
	{ LLFastTimer::FTM_POOLS,							"   Pools" },
	{ LLFastTimer::FTM_POOLRENDER,						"    RenderPool" },
	{ LLFastTimer::FTM_VOLUME_GEOM,						"     Volume Geometry" },
	{ LLFastTimer::FTM_FACE_GET_GEOM,					"     Face Geom" },
	{ LLFastTimer::FTM_FACE_GEOM_INDEX,					"      Index" },
	{ LLFastTimer::FTM_FACE_GEOM_POSITION,				"      Position" },
	{ LLFastTimer::FTM_FACE_GEOM_COLOR,					"      Color" },
	{ LLFastTimer::FTM_FACE_GEOM_EMISSIVE,				"      Emissive" },
	{ LLFastTimer::FTM_FACE_GEOM_NORMAL,				"      Normal" },
	{ LLFastTimer::FTM_FACE_GEOM_TANGENT,				"      Tangent" },
	{ LLFastTimer::FTM_FACE_GEOM_WEIGHTS,				"      Weights" },
	{ LLFastTimer::FTM_FACE_GEOM_TEXTURE,				"      Texture" },
	{ LLFastTimer::FTM_RENDER_OCCLUSION,				"     Occlusion" },
	{ LLFastTimer::FTM_OCCLUSION_ALLOCATE,				"      Allocate" },
	{ LLFastTimer::FTM_PUSH_OCCLUSION_VERTS,			"      Push Occlusion" },
	{ LLFastTimer::FTM_OCCLUSION_BEGIN_QUERY,			"       Begin Query" },
	{ LLFastTimer::FTM_OCCLUSION_DRAW_WATER,			"       Draw Water" },
	{ LLFastTimer::FTM_OCCLUSION_DRAW,					"       Draw" },
	{ LLFastTimer::FTM_OCCLUSION_END_QUERY,				"       End Query" },
	{ LLFastTimer::FTM_AVATAR_FACE,						"     Avatar Face" },
	{ LLFastTimer::FTM_RENDER_CHARACTERS,				"     Avatars" },
	{ LLFastTimer::FTM_RENDER_AVATARS,					"      renderAvatars" },
	{ LLFastTimer::FTM_RIGGED_VBO,						"       Rigged VBO" },
	{ LLFastTimer::FTM_RENDER_SIMPLE,					"     Simple" },
	{ LLFastTimer::FTM_RENDER_TERRAIN,					"     Terrain" },
	{ LLFastTimer::FTM_RENDER_GRASS,					"     Grass" },
	{ LLFastTimer::FTM_RENDER_WATER,					"     Water" },
	{ LLFastTimer::FTM_RENDER_TREES,					"     Trees" },
	{ LLFastTimer::FTM_RENDER_CLOUDS,					"     Clouds" },
	{ LLFastTimer::FTM_RENDER_WL_SKY,					"     WL Sky" },
	{ LLFastTimer::FTM_VISIBLE_CLOUD,					"      Visible Cloud" },
	{ LLFastTimer::FTM_RENDER_INVISIBLE,				"     Invisible" },
	{ LLFastTimer::FTM_RENDER_FULLBRIGHT,				"     Fullbright" },
	{ LLFastTimer::FTM_RENDER_GLOW,						"     Glow" },
	{ LLFastTimer::FTM_RENDER_SHINY,					"     Shiny" },
	{ LLFastTimer::FTM_RENDER_BUMP,						"     Bump" },
	{ LLFastTimer::FTM_RENDER_MATERIALS,				"     Materials" },
	{ LLFastTimer::FTM_RENDER_ALPHA,					"     Alpha" },
	{ LLFastTimer::FTM_RENDER_BLOOM,					"   Bloom" },
	{ LLFastTimer::FTM_UPDATE_GL,						"  Update GL" },
	{ LLFastTimer::FTM_REBUILD_GROUPS,					"  Rebuild Groups" },
	{ LLFastTimer::FTM_RESET_VB,						"  Reset VB" },
	{ LLFastTimer::FTM_RENDER_UI,						"  UI" },
	{ LLFastTimer::FTM_RENDER_TIMER,					"   Fast Timers View" },
	{ LLFastTimer::FTM_RENDER_FONTS_BATCHED,			"   Batched font glyphs" },
	{ LLFastTimer::FTM_RENDER_FONTS_SERIALIZED,			"   Serialized font glyphs" },
	{ LLFastTimer::FTM_RENDER_SPELLCHECK,				"   Mispell. Highlight" },
	{ LLFastTimer::FTM_RESIZE_SCREEN_TEXTURE,			"  Resize Screen Tex." },
	{ LLFastTimer::FTM_SWAP,							"  Swap" },
	{ LLFastTimer::FTM_OTHER,							" Other" }
};
constexpr S32 FTV_DISPLAY_NUM = LL_ARRAY_SIZE(ft_display_table);

// line of table entry for display purposes (for collapse)
S32 ft_display_idx[FTV_DISPLAY_NUM];

static const LLColor4* level1_colors[] = {	&LLColor4::cyan1,
											&LLColor4::grey1,
											&LLColor4::yellow1,
											&LLColor4::blue0,
											&LLColor4::green0,
											&LLColor4::red0,
											&LLColor4::black
};

constexpr S32 FTV_LEVEL1_COLORS = LL_ARRAY_SIZE(level1_colors);

static const LLColor4* level2_colors[] = {	&LLColor4::red1,
											&LLColor4::blue1,
											&LLColor4::green1,
											&LLColor4::orange1,
											&LLColor4::purple1,
											&LLColor4::cyan2,
											&LLColor4::magenta1,
											&LLColor4::yellow2,
											&LLColor4::grey2,
											&LLColor4::pink1,
											&LLColor4::red2,
											&LLColor4::blue2,
											&LLColor4::green2,
											&LLColor4::orange2,
											&LLColor4::purple2,
											&LLColor4::cyan3,
											&LLColor4::magenta2,
											&LLColor4::yellow3,
											&LLColor4::grey3,
											&LLColor4::pink2,
											&LLColor4::cyan4,
											&LLColor4::purple3,
											&LLColor4::yellow4,
											&LLColor4::green3,
											&LLColor4::orange3
};
constexpr S32 FTV_LEVEL2_COLORS = LL_ARRAY_SIZE(level2_colors);

static const LLColor4* levelN_colors[] = {	&LLColor4::red4,
											&LLColor4::blue3,
											&LLColor4::green4,
											&LLColor4::orange4,
											&LLColor4::purple4,
											&LLColor4::cyan5,
											&LLColor4::magenta3,
											&LLColor4::yellow5,
											&LLColor4::grey4,
											&LLColor4::red5,
											&LLColor4::blue4,
											&LLColor4::green5,
											&LLColor4::orange5,
											&LLColor4::purple4,
											&LLColor4::cyan6,
											&LLColor4::magenta4,
											&LLColor4::yellow6,
											&LLColor4::purple5,
											&LLColor4::green6,
											&LLColor4::yellow7,
											&LLColor4::blue6,
											&LLColor4::orange6,
											&LLColor4::green8,
											&LLColor4::blue7,
											&LLColor4::yellow8,
											&LLColor4::green7,
											&LLColor4::yellow9,
											&LLColor4::green9
};
constexpr S32 FTV_LEVELN_COLORS = LL_ARRAY_SIZE(levelN_colors);

LLFastTimerView::LLFastTimerView(const std::string& name)
:	LLFloater(name, LLRect(0, 100, 100, 0), std::string(),
			  false, 1, 1, false, false, true),
	mDisplayMode(0),
	mAvgCountTotal(0),
	mMaxCountTotal(0),
	mDisplayCenter(0),
	mDisplayCalls(0),
	mDisplayHz(0),
	mScrollIndex(0),
	mHoverIndex(-1),
	mHoverBarIndex(-1),
	mSubtractHidden(0),
	mPrintStats(-1),
	mWindowHeight(0),
	mWindowWidth(0),
	mFirstDrawLoop(true)
{
	llassert_always(!gFastTimerViewp);
	gFastTimerViewp = this;

	mFont = LLFontGL::getFontMonospace();
	if (!mFont)
	{
		llerrs << "No monospace font !" << llendl;
	}

	setVisible(false);

	setFollowsTop();
	setFollowsLeft();
	resize();

	S32 count = (MAX_VISIBLE_HISTORY + 1) * FTV_DISPLAY_NUM;
	mBarStart = new S32[count];
	memset(mBarStart, 0, count * sizeof(S32));

	mBarEnd = new S32[count];
	memset(mBarEnd, 0, count * sizeof(S32));

	setDisplayModeText();
	setCenterModeText();

	// One-time setup
	static bool ft_display_didcalc = false;
	if (!ft_display_didcalc)
	{
		S32 pidx[FTV_DISPLAY_NUM];
		S32 level;
		S32 color_idx1 = 0, color_idx2 = 0, color_idxN = 0;
		for (S32 i = 0; i < FTV_DISPLAY_NUM; ++i)
		{
			level = 0;
			const char* text = ft_display_table[i].desc;
			while (text[0] == ' ')
			{
				++text;
				++level;
			}
			llassert(level < FTV_DISPLAY_NUM);
			ft_display_table[i].desc = text;
			ft_display_table[i].level = level;
			ft_display_table[i].disabled = 0;
			if (level > 0)
			{
				ft_display_table[i].parent = pidx[level - 1];
				ft_display_table[i].disabled = level == 1 ? 1 : 3;
				if (level == 1)
				{
					ft_display_table[i].color = level1_colors[color_idx1];
					if (++color_idx1 >= FTV_LEVEL1_COLORS)
					{
						color_idx1 = 0;
					}
				}
				else if (level == 2)
				{
					ft_display_table[i].color = level2_colors[color_idx2];
					if (++color_idx2 >= FTV_LEVEL2_COLORS)
					{
						color_idx2 = 0;
					}
				}
				else
				{
					ft_display_table[i].color = levelN_colors[color_idxN];
					if (++color_idxN >= FTV_LEVELN_COLORS)
					{
						color_idxN = 0;
					}
				}
			}
			else
			{
				ft_display_table[i].parent = -1;
				ft_display_table[i].disabled = 0;
				ft_display_table[i].color = &LLColor4::white;
			}
			ft_display_idx[i] = i;
			pidx[level] = i;
		}
		ft_display_didcalc = true;
	}
}

LLFastTimerView::~LLFastTimerView()
{
	delete[] mBarStart;
	delete[] mBarEnd;
	gFastTimerViewp = NULL;
}

void LLFastTimerView::setDisplayModeText()
{
	static const char modedesc[][16] =
	{
		"2 x average ",
		"Max         ",
		"Recent max  ",
		"100 ms      "
	};
	static const char* fullbar =
		"Full bar = %s    [Click to pause/reset] [SHIFT-click to toggle]";
	mDisplayModeText =
		utf8str_to_wstring(llformat(fullbar, modedesc[mDisplayMode]));
	mDisplayModeTextWidth = mFont->getWidth(mDisplayModeText.c_str());
}

void LLFastTimerView::setCenterModeText()
{
	static const char centerdesc[][16] =
	{
		"Left      ",
		"Centered  ",
		"Ordered   "
	};
	static const char* justify = "Justification = %s [CTRL-click to toggle]";
	mCenterModeText =
		utf8str_to_wstring(llformat(justify, centerdesc[mDisplayCenter]));
}

void LLFastTimerView::resize()
{
	S32 height = gViewerWindowp->getVirtualWindowRect().getHeight();
	S32 width = gViewerWindowp->getVirtualWindowRect().getWidth();
	mWindowHeight = 3 * height / 4;
	mWindowWidth = 3 * width / 4;
	reshape(mWindowWidth, mWindowHeight);	// Necessary for the close button !
	LLRect rect;
	rect.setLeftTopAndSize(FASTTIMERVIEW_LEFT,
						   height - FASTTIMERVIEW_TOP_DELTA,
						   mWindowWidth, mWindowHeight);
	setRect(rect);
}

bool LLFastTimerView::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
	if (mBarRect.pointInRect(x, y))
	{
		S32 bar_idx = MAX_VISIBLE_HISTORY -
					  (y - mBarRect.mBottom) * (MAX_VISIBLE_HISTORY + 2) /
					  mBarRect.getHeight();
		bar_idx = llclamp(bar_idx, 0, MAX_VISIBLE_HISTORY);
		mPrintStats = bar_idx;
		return true;
	}
	return false;
}

S32 LLFastTimerView::getLegendIndex(S32 y)
{
	static const S32 line_height =
		LLFontGL::getFontMonospace()->getLineHeight() + 2;
	S32 idx = (getRect().getHeight() - y) / line_height - 5;
	return idx >= 0 && idx < FTV_DISPLAY_NUM ? ft_display_idx[idx] : -1;
}

bool LLFastTimerView::handleMouseDown(S32 x, S32 y, MASK mask)
{
	{
		S32 local_x = x - mButtons[BUTTON_CLOSE]->getRect().mLeft;
		S32 local_y = y - mButtons[BUTTON_CLOSE]->getRect().mBottom;
		if (mButtons[BUTTON_CLOSE]->getVisible() &&
			mButtons[BUTTON_CLOSE]->pointInView(local_x, local_y))
		{
			return LLFloater::handleMouseDown(x, y, mask);
		}
	}
	if (x < mBarRect.mLeft)
	{
		S32 legend_index = getLegendIndex(y);
		if (legend_index >= 0 && legend_index < FTV_DISPLAY_NUM)
		{
			S32 disabled = ft_display_table[legend_index].disabled;
			disabled = (disabled + 1) % 3;
			ft_display_table[legend_index].disabled = disabled;
			S32 level = ft_display_table[legend_index].level;

			// Propagate enable/disable to all children
			disabled = disabled ? 3 : 0;
			++legend_index;
			while (legend_index < FTV_DISPLAY_NUM &&
				   ft_display_table[legend_index].level > level)
			{
				ft_display_table[legend_index++].disabled = disabled;
			}
		}
	}
	else if (mask & MASK_ALT)
	{
		if (mask & MASK_SHIFT)
		{
			mSubtractHidden = !mSubtractHidden;
		}
		else if (mask & MASK_CONTROL)
		{
			mDisplayHz = !mDisplayHz;
		}
		else
		{
			mDisplayCalls = !mDisplayCalls;
		}
	}
	else if (mask & MASK_SHIFT)
	{
		if (++mDisplayMode > 3)
		{
			mDisplayMode = 0;
		}
		setDisplayModeText();
	}
	else if (mask & MASK_CONTROL)
	{
		if (++mDisplayCenter > 2)
		{
			mDisplayCenter = 0;
		}
		setCenterModeText();
	}
	else
	{
		// Pause/unpause
		LLFastTimer::sPauseHistory = !LLFastTimer::sPauseHistory;
		// Reset scroll to bottom when unpausing
		if (!LLFastTimer::sPauseHistory)
		{
			mScrollIndex = 0;
		}
	}

	// SJB: Don't pass mouse clicks through the display
	return true;
}

bool LLFastTimerView::handleMouseUp(S32 x, S32 y, MASK mask)
{
	S32 local_x = x - mButtons[BUTTON_CLOSE]->getRect().mLeft;
	S32 local_y = y - mButtons[BUTTON_CLOSE]->getRect().mBottom;
	if (mButtons[BUTTON_CLOSE]->getVisible() &&
		mButtons[BUTTON_CLOSE]->pointInView(local_x, local_y))
	{
		return LLFloater::handleMouseUp(x, y, mask);
	}
	return false;
}

bool LLFastTimerView::handleHover(S32 x, S32 y, MASK mask)
{
	if (LLFastTimer::sPauseHistory && mBarRect.pointInRect(x, y))
	{
		mHoverIndex = -1;
		mHoverBarIndex = MAX_VISIBLE_HISTORY -
						 (y - mBarRect.mBottom) * (MAX_VISIBLE_HISTORY + 2) /
						 mBarRect.getHeight();
		if (mHoverBarIndex == 0)
		{
			return true;
		}
		else if (mHoverBarIndex < 0)
		{
			mHoverBarIndex = 0;
		}
		for (S32 i = 0; i < FTV_DISPLAY_NUM; ++i)
		{
			if (x > mBarStart[mHoverBarIndex * FTV_DISPLAY_NUM + i] &&
				x < mBarEnd[mHoverBarIndex * FTV_DISPLAY_NUM + i] &&
				ft_display_table[i].disabled <= 1)
			{
				mHoverIndex = i;
			}
		}
	}
	else if (x < mBarRect.mLeft)
	{
		S32 legend_index = getLegendIndex(y);
		if (legend_index >= 0 && legend_index < FTV_DISPLAY_NUM)
		{
			mHoverIndex = legend_index;
		}
	}

	return false;
}

bool LLFastTimerView::handleScrollWheel(S32 x, S32 y, S32 clicks)
{
	LLFastTimer::sPauseHistory = true;
	mScrollIndex = llclamp(mScrollIndex - clicks, 0,
						   llmin(LLFastTimer::sLastFrameIndex,
								 FTM_HISTORY_NUM - MAX_VISIBLE_HISTORY));
	return true;
}

//virtual
void LLFastTimerView::setVisible(bool visible)
{
	LLFloater::setVisible(visible);

	if (!visible)
	{
		mFirstDrawLoop = true;	// Reset this for next opening.
		if (!gSavedSettings.getBool("FastTimersAlwaysEnabled"))
		{
			gEnableFastTimers = false;
			llinfos << "Fast timers disabled." << llendl;
		}
	}
}

//virtual
void LLFastTimerView::onClose(bool app_quitting)
{
	if (app_quitting)
	{
		LLFloater::close(app_quitting);
	}
	else
	{
		setVisible(false);
	}
}

void LLFastTimerView::draw()
{
	LL_FAST_TIMER(FTM_RENDER_TIMER);

	if (!gEnableFastTimers)
	{
		gEnableFastTimers = true;
		llinfos << "Fast timers enabled." << llendl;
	}
	else if (mFirstDrawLoop)
	{
		// When the floater just got opened while FastTimersAlwaysEnabled was
		// TRUE, pause immediately after we draw the first loop. This way, the
		// user may see the timer stats before the fast timer floater drawing
		// time would start and pollute it...
		LLFastTimer::sPauseHistory = true;
	}

	S32 height = 3 * gViewerWindowp->getVirtualWindowRect().getHeight() / 4;
	S32 width = 3 * gViewerWindowp->getVirtualWindowRect().getWidth() / 4;
	if (mWindowHeight != height || mWindowWidth != width)
	{
		resize();
	}

	F64 clock_freq = (F64)LLFastTimer::countsPerSecond();
	F64 iclock_freq = 1000.0 / clock_freq;

	// Make sure all timers are accounted for: set 'FTM_OTHER' to unaccounted
	// ticks last frame.
	static S32 display_timer[LLFastTimer::FTM_NUM_TYPES];
	memset((void*)display_timer, 0, LLFastTimer::FTM_NUM_TYPES * sizeof(S32));
	for (S32 i = 0; i < FTV_DISPLAY_NUM; ++i)
	{
		S32 tidx = ft_display_table[i].timer;
		display_timer[tidx] = 1;
	}
	S32 hidx = LLFastTimer::sLastFrameIndex % FTM_HISTORY_NUM;
	LLFastTimer::sCountHistory[hidx][LLFastTimer::FTM_OTHER] = 0;
	LLFastTimer::sCallHistory[hidx][LLFastTimer::FTM_OTHER] = 0;
	for (S32 tidx = 0; tidx < LLFastTimer::FTM_NUM_TYPES; ++tidx)
	{
		U64 counts = LLFastTimer::sCountHistory[hidx][tidx];
		if (counts > 0 && display_timer[tidx] == 0)
		{
			LLFastTimer::sCountHistory[hidx][LLFastTimer::FTM_OTHER] += counts;
			LLFastTimer::sCallHistory[hidx][LLFastTimer::FTM_OTHER] += 1;
		}
	}
	LLFastTimer::sCountAverage[LLFastTimer::FTM_OTHER] = 0;
	LLFastTimer::sCallAverage[LLFastTimer::FTM_OTHER] = 0;
	for (S32 h = 0; h < FTM_HISTORY_NUM; ++h)
	{
		LLFastTimer::sCountAverage[LLFastTimer::FTM_OTHER] +=
			LLFastTimer::sCountHistory[h][LLFastTimer::FTM_OTHER];
		LLFastTimer::sCallAverage[LLFastTimer::FTM_OTHER] +=
			LLFastTimer::sCallHistory[h][LLFastTimer::FTM_OTHER];
	}
	LLFastTimer::sCountAverage[LLFastTimer::FTM_OTHER] /= FTM_HISTORY_NUM;
	LLFastTimer::sCallAverage[LLFastTimer::FTM_OTHER] /= FTM_HISTORY_NUM;

	LLTexUnit* unit0 = gGL.getTexUnit(0);

	// Draw the window background
	unit0->unbind(LLTexUnit::TT_TEXTURE);
	gl_rect_2d(0, height, width, 0, LLColor4(0.f, 0.f, 0.f, 0.25f));

	constexpr S32 margin = 10;
	S32 xleft = margin;
	S32 ytop = margin;

	// Draw some help

	S32 x = xleft;
	S32 y = height - ytop;

	mFont->render(mDisplayModeText, 0, x, y, LLColor4::white, LLFontGL::LEFT,
				  LLFontGL::TOP);
	S32 textw = mDisplayModeTextWidth;
	x = xleft;
	static const S32 texth = (S32)mFont->getLineHeight();
	y -= texth + 2;

	mFont->render(mCenterModeText, 0, x, y, LLColor4::white, LLFontGL::LEFT,
				  LLFontGL::TOP);
	y -= texth + 2;

	static const LLWString cmds =
		utf8str_to_wstring(std::string("[Right-click log selected] [ALT-click toggle counts] [ALT-SHIFT-click sub hidden]"));
	mFont->render(cmds, 0, x, y, LLColor4::white, LLFontGL::LEFT,
				  LLFontGL::TOP);
	y -= texth + 2;

	// Calc the total ticks

	S32 histmax = llmin(LLFastTimer::sLastFrameIndex + 1, MAX_VISIBLE_HISTORY);
	U64 ticks_sum[FTM_HISTORY_NUM + 1][FTV_DISPLAY_NUM];
	for (S32 j = -1; j < FTM_HISTORY_NUM; ++j)
	{
		S32 hidx;
		if (j >= 0)
		{
			hidx = (LLFastTimer::sLastFrameIndex + j) % FTM_HISTORY_NUM;
		}
		else
		{
			hidx = -1;
		}

		// Calculate tick info by adding child ticks to parents
		for (S32 i = 0; i < FTV_DISPLAY_NUM; ++i)
		{
			if (mSubtractHidden && ft_display_table[i].disabled > 1)
			{
				continue;
			}
			// Get ticks
			S32 tidx = ft_display_table[i].timer;
			if (hidx >= 0)
			{
				ticks_sum[j + 1][i] = LLFastTimer::sCountHistory[hidx][tidx];
			}
			else
			{
				ticks_sum[j + 1][i] = LLFastTimer::sCountAverage[tidx];
			}
			S32 pidx = ft_display_table[i].parent;
			// Add ticks to parents
			while (pidx >= 0)
			{
				ticks_sum[j + 1][pidx] += ticks_sum[j + 1][i];
				pidx = ft_display_table[pidx].parent;
			}
		}
	}

	// Draw the legend

	S32 legendwidth = 0;
	xleft = margin;
	ytop = y;

	y -= texth + 2;

	S32 cur_line = 0;
	S32 display_line[FTV_DISPLAY_NUM];
	std::string line;
	for (S32 i = 0; i < FTV_DISPLAY_NUM; ++i)
	{
		S32 disabled = ft_display_table[i].disabled;
		if (disabled == 3)
		{
			continue; // skip row
		}
		display_line[i] = cur_line;
		ft_display_idx[cur_line++] = i;
		S32 level = ft_display_table[i].level;
		S32 parent = ft_display_table[i].parent;

		x = xleft;

		S32 left = x;
		S32 right = x + texth;
		S32 top = y;
		S32 bottom = y - texth;
		S32 scale_offset = 0;
		if (y > 3 * texth)
		{
			if (i == mHoverIndex)
			{
				scale_offset =
					llfloor(sinf(mHighlightTimer.getElapsedTimeF32() * 6.f) *
							2.f);
			}
			gl_rect_2d(left - scale_offset, top + scale_offset,
					   right + scale_offset, bottom - scale_offset,
					   *ft_display_table[i].color);
		}

		S32 tidx = ft_display_table[i].timer;
		F32 ms = 0;
		S32 calls = 0;
		if (mHoverBarIndex > 0 && mHoverIndex >= 0)
		{
			S32 hidx = (LLFastTimer::sLastFrameIndex + mHoverBarIndex - 1 -
						mScrollIndex) % FTM_HISTORY_NUM;
			S32 bidx = FTM_HISTORY_NUM - mScrollIndex - mHoverBarIndex;
			U64 ticks = ticks_sum[bidx + 1][i];
			ms = (F32)((F64)ticks * iclock_freq);
			calls = (S32)LLFastTimer::sCallHistory[hidx][tidx];
		}
		else
		{
			U64 ticks = ticks_sum[0][i];
			ms = (F32)((F64)ticks * iclock_freq);
			calls = (S32)LLFastTimer::sCallAverage[tidx];
		}
		if (mDisplayCalls)
		{
			line = llformat("%s (%d)", ft_display_table[i].desc, calls);
		}
		else
		{
			line = llformat("%s [%.1f]", ft_display_table[i].desc, ms);
		}
		S32 dx = texth + 4 + level * 8;

		LLColor4 color = disabled > 1 ? LLColor4::grey : LLColor4::white;
		if (level > 0 && y > 3 * texth)
		{
			S32 line_start_y = (top + bottom) / 2;
			S32 line_end_y = line_start_y + (texth + 2) *
							 (display_line[i] - display_line[parent]) -
							 texth / 2;
			gl_line_2d(x + dx - 8, line_start_y, x + dx, line_start_y, color);
			S32 line_x = x + (texth + 4) + ((level - 1) * 8);
			gl_line_2d(line_x, line_start_y, line_x, line_end_y, color);
			if (disabled == 1)
			{
				gl_line_2d(line_x + 4, line_start_y - 3, line_x + 4,
						   line_start_y + 4, color);
			}
		}

		x += dx;
		bool is_child_of_hover_item = (i == mHoverIndex);
		S32 next_parent = ft_display_table[i].parent;
		while (!is_child_of_hover_item && next_parent >= 0)
		{
			is_child_of_hover_item = (mHoverIndex == next_parent);
			next_parent = ft_display_table[next_parent].parent;
		}

		if (y > 3 * texth)
		{
			if (is_child_of_hover_item)
			{
				mFont->renderUTF8(line, 0, x, y, color, LLFontGL::LEFT,
								  LLFontGL::TOP, LLFontGL::BOLD);
			}
			else
			{
				mFont->renderUTF8(line, 0, x, y, color, LLFontGL::LEFT,
								  LLFontGL::TOP);
			}
		}
		y -= texth + 2;

		textw = dx + 40 +
				mFont->getWidth(std::string(ft_display_table[i].desc));
		if (textw > legendwidth)
		{
			legendwidth = textw;
		}
	}
	if (y <= 3 * texth)
	{
		static const LLWString truncated =
			utf8str_to_wstring(std::string("<list truncated>"));
		mFont->render(truncated, 0, 3 * texth, 2 * texth, LLColor4::white,
					  LLFontGL::LEFT, LLFontGL::TOP, LLFontGL::BOLD);
	}

	for (S32 i = cur_line; i < FTV_DISPLAY_NUM; ++i)
	{
		ft_display_idx[i] = -1;
	}
	xleft += legendwidth + 8;

	// Update rectangle that includes timer bars
	mBarRect.mLeft = xleft;
	mBarRect.mRight = getRect().mRight - xleft;
	mBarRect.mTop = ytop - (texth + 4);
	mBarRect.mBottom = margin + LINE_GRAPH_HEIGHT;

	y = ytop;
	S32 barh = (ytop - margin - LINE_GRAPH_HEIGHT) / (MAX_VISIBLE_HISTORY + 2);
	S32 dy = barh >> 2;	// Spacing between bars
	if (dy < 1) dy = 1;
	barh -= dy;
	S32 barw = width - xleft - margin;

	// Draw the history bars
	if (LLFastTimer::sLastFrameIndex >= 0)
	{
		U64 totalticks;
		if (mFirstDrawLoop || !LLFastTimer::sPauseHistory)
		{
			U64 ticks = 0;
			S32 hidx = (LLFastTimer::sLastFrameIndex - mScrollIndex) %
					   FTM_HISTORY_NUM;
			for (S32 i = 0; i < FTV_DISPLAY_NUM; ++i)
			{
				if (mSubtractHidden && ft_display_table[i].disabled > 1)
				{
					continue;
				}
				S32 tidx = ft_display_table[i].timer;
				ticks += LLFastTimer::sCountHistory[hidx][tidx];
			}
			if (LLFastTimer::sCurFrameIndex >= 10)
			{
				U64 framec = LLFastTimer::sCurFrameIndex;
				U64 avg = (U64)mAvgCountTotal;
				mAvgCountTotal = (avg * framec + ticks) / (framec + 1);
				if (ticks > mMaxCountTotal)
				{
					mMaxCountTotal = ticks;
				}
			}
#if 1
			if (ticks < mAvgCountTotal / 100 || ticks > mAvgCountTotal * 100)
			{
				LLFastTimer::sResetHistory = true;
			}
#endif
			if (LLFastTimer::sCurFrameIndex < 10 || LLFastTimer::sResetHistory)
			{
				mAvgCountTotal = ticks;
				mMaxCountTotal = ticks;
			}
		}

		if (mDisplayMode == 0)
		{
			totalticks = mAvgCountTotal * 2;
		}
		else if (mDisplayMode == 1)
		{
			totalticks = mMaxCountTotal;
		}
		else if (mDisplayMode == 2)
		{
			// Calculate the max total ticks for the current history
			totalticks = 0;
			for (S32 j = 0; j < histmax; ++j)
			{
				U64 ticks = 0;
				for (S32 i = 0; i < FTV_DISPLAY_NUM; ++i)
				{
					if (mSubtractHidden && ft_display_table[i].disabled > 1)
					{
						continue;
					}
					S32 tidx = ft_display_table[i].timer;
					ticks += LLFastTimer::sCountHistory[j][tidx];
				}
				if (ticks > totalticks)
				{
					totalticks = ticks;
				}
			}
		}
		else
		{
			totalticks = (U64)(clock_freq * .1); // 100 ms
		}

		// Draw MS ticks
		{
			U32 ms = (U32)((F64)totalticks * iclock_freq);

			line = llformat("%.1f ms |", (F32)ms * .25f);
			x = xleft + barw / 4 - mFont->getWidth(line);
			mFont->renderUTF8(line, 0, x, y, LLColor4::white, LLFontGL::LEFT,
							  LLFontGL::TOP);

			line = llformat("%.1f ms |", (F32)ms * .50f);
			x = xleft + barw / 2 - mFont->getWidth(line);
			mFont->renderUTF8(line, 0, x, y, LLColor4::white, LLFontGL::LEFT,
							  LLFontGL::TOP);

			line = llformat("%.1f ms |", (F32)ms * .75f);
			x = xleft + 3 * barw / 4 - mFont->getWidth(line);
			mFont->renderUTF8(line, 0, x, y, LLColor4::white, LLFontGL::LEFT,
							  LLFontGL::TOP);

			line = llformat("%d ms |", ms);
			x = xleft + barw - mFont->getWidth(line);
			mFont->renderUTF8(line, 0, x, y, LLColor4::white, LLFontGL::LEFT,
							  LLFontGL::TOP);
		}

		LLRect graph_rect;
		// Draw borders
		{
			unit0->unbind(LLTexUnit::TT_TEXTURE);
			gGL.color4f(0.5f, 0.5f, 0.5f, 0.5f);
			S32 width = getRect().getWidth() - 5;
			S32 by = y + 2;

			y -= (texth + 4);

			// Heading
			gl_rect_2d(xleft - 5, by, width, y + 5, false);

			// Tree view
			gl_rect_2d(5, by, xleft - 10, 5, false);

			by = y + 5;
			// Average bar
			gl_rect_2d(xleft - 5, by, width, by - barh - dy - 5, false);

			by -= barh * 2 + dy;

			// Current frame bar
			gl_rect_2d(xleft - 5, by, width, by - barh - dy - 2, false);

			by -= barh + dy + 1;

			// History bars
			gl_rect_2d(xleft - 5, by, width, LINE_GRAPH_HEIGHT - barh - dy - 2, false);

			by = LINE_GRAPH_HEIGHT - barh - dy - 7;

			// Line graph
			graph_rect = LLRect(xleft - 5, by, width, 5);

			gl_rect_2d(graph_rect, false);
		}

		// Draw bars for each history entry. Special: -1 = show running average
		static const S32 tex_width = LLUIImage::sRoundedSquareWidth;
		static const S32 tex_height = LLUIImage::sRoundedSquareHeight;
		unit0->bind(LLUIImage::sRoundedSquare->getImage());
		for (S32 j = -1; j < histmax && y > LINE_GRAPH_HEIGHT; ++j)
		{
			S32 sublevel_dx[FTV_DISPLAY_NUM + 1];
			S32 sublevel_left[FTV_DISPLAY_NUM + 1];
			S32 sublevel_right[FTV_DISPLAY_NUM + 1];
			S32 tidx;
			if (j >= 0)
			{
				tidx = FTM_HISTORY_NUM - j - 1 - mScrollIndex;
			}
			else
			{
				tidx = -1;
			}

			x = xleft;

			// Draw the bars for each stat
			S32 xpos[FTV_DISPLAY_NUM + 1];
			S32 deltax[FTV_DISPLAY_NUM + 1];
			xpos[0] = xleft;

			for (S32 i = 0; i < FTV_DISPLAY_NUM; ++i)
			{
				if (ft_display_table[i].disabled > 1)
				{
					continue;
				}

				F32 frac = (F32)ticks_sum[tidx + 1][i] / (F32)totalticks;

				S32 dx = ll_round(frac * (F32)barw);
				deltax[i] = dx;

				S32 level = ft_display_table[i].level;
				S32 parent = ft_display_table[i].parent;
				llassert(level < FTV_DISPLAY_NUM);
				llassert(parent < FTV_DISPLAY_NUM);

				S32 left = xpos[level];

				S32 prev_idx = i - 1;
				while (prev_idx > 0 &&
					   ft_display_table[prev_idx].disabled > 1)
				{
					--prev_idx;
				}
				S32 next_idx = i + 1;
				while (next_idx < FTV_DISPLAY_NUM &&
					   ft_display_table[next_idx].disabled > 1)
				{
					++next_idx;
				}

				if (level == 0)
				{
					sublevel_left[level] = xleft;
					sublevel_dx[level] = dx;
					sublevel_right[level] = sublevel_left[level] +
											sublevel_dx[level];
				}
				else if (i == 0 || ft_display_table[prev_idx].level < level)
				{
					// If we are the first entry at a new sublevel block, calc
					// the total width of this sublevel and modify left to
					// align block.
					U64 sublevelticks = ticks_sum[tidx + 1][i];
					for (S32 k = i + 1; k < FTV_DISPLAY_NUM; ++k)
					{
						if (ft_display_table[k].level < level)
						{
							break;
						}
						if (ft_display_table[k].disabled <= 1 &&
							ft_display_table[k].level == level)
						{
							sublevelticks += ticks_sum[tidx + 1][k];
						}
					}
					F32 subfrac = (F32)sublevelticks / (F32)totalticks;
					sublevel_dx[level] = (S32)(subfrac * (F32)barw + .5f);

					if (mDisplayCenter == 1)		// Center aligned
					{
						left += (deltax[parent] - sublevel_dx[level]) / 2;
					}
					else if (mDisplayCenter == 2)	// Right aligned
					{
						left += deltax[parent] - sublevel_dx[level];
					}

					sublevel_left[level] = left;
					sublevel_right[level] = sublevel_left[level] +
											sublevel_dx[level];
				}

				S32 right = left + dx;
				xpos[level] = right;
				xpos[level + 1] = left;

				mBarStart[(j + 1) * FTV_DISPLAY_NUM + i] = left;
				mBarEnd[(j + 1) * FTV_DISPLAY_NUM + i] = right;

				S32 top = y;
				S32 bottom = y - barh;

				if (right > left)
				{
					LLColor4 color = *ft_display_table[i].color;
					S32 scale_offset = 0;

					bool is_child_of_hover_item = (i == mHoverIndex);
					S32 next_parent = ft_display_table[i].parent;
					while (!is_child_of_hover_item && next_parent >= 0)
					{
						is_child_of_hover_item = (mHoverIndex == next_parent);
						next_parent = ft_display_table[next_parent].parent;
					}

					if (i == mHoverIndex)
					{
						scale_offset =
							llfloor(sinf(mHighlightTimer.getElapsedTimeF32() *
										 6.f) * 3.f);
					}
					else if (mHoverIndex >= 0 && !is_child_of_hover_item)
					{
						color = lerp(color, LLColor4::grey, 0.8f);
					}

					gGL.color4fv(color.mV);
					F32 start_fragment =
						llclamp((F32)(left - sublevel_left[level]) /
								(F32)sublevel_dx[level], 0.f, 1.f);
					F32 end_fragment =
						llclamp((F32)(right - sublevel_left[level]) /
								(F32)sublevel_dx[level], 0.f, 1.f);
					gl_segmented_rect_2d_fragment_tex(sublevel_left[level],
													  top - level + scale_offset,
													  sublevel_right[level],
													  bottom + level - scale_offset,
													  tex_width, tex_height,
													  16, start_fragment,
													  end_fragment);
				}
			}
			y -= barh + dy;
			if (j < 0)
			{
				y -= barh;
			}
		}

		// Draw line graph history
		{
			unit0->unbind(LLTexUnit::TT_TEXTURE);
			LLLocalClipRect clip(graph_rect);

			// Normalize based on last frame's maximum
			static U64 last_max = 0;
			static F32 alpha_interp = 0.f;
			U64 max_ticks = llmax(last_max, (U64)1);
			F32 ms = (F32)((F64)max_ticks * iclock_freq);

			// Display y-axis range
			std::string line;
			if (mDisplayCalls)
			{
				line = llformat("%d calls", (S32)max_ticks);
			}
			else if (mDisplayHz)
			{
				line = llformat("%d Hz", (S32)max_ticks);
			}
			else
			{
				line = llformat("%4.2f ms", ms);
			}

			x = graph_rect.mRight - mFont->getWidth(line) - 5;
			y = graph_rect.mTop - texth;

			mFont->renderUTF8(line, 0, x, y, LLColor4::white, LLFontGL::LEFT,
							  LLFontGL::TOP);

			// Highlight visible range
			{
				S32 first_frame = FTM_HISTORY_NUM - mScrollIndex;
				S32 last_frame = first_frame - MAX_VISIBLE_HISTORY;

				F32 frame_delta = (F32)graph_rect.getWidth() /
								  F32(FTM_HISTORY_NUM - 1);

				F32 right = (F32)graph_rect.mLeft + frame_delta * first_frame;
				F32 left = (F32)graph_rect.mLeft + frame_delta * last_frame;

				gGL.color4f(0.5f, 0.5f, 0.5f, 0.3f);
				gl_rect_2d((S32)left, graph_rect.mTop, (S32)right,
						   graph_rect.mBottom);

				if (mHoverBarIndex >= 0)
				{
					S32 bar_frame = first_frame - mHoverBarIndex;
					F32 bar = (F32)graph_rect.mLeft + frame_delta * bar_frame;

					gGL.color4f(0.5f, 0.5f, 0.5f, 1.f);

					gGL.begin(LLRender::LINES);
					gGL.vertex2i((S32)bar, graph_rect.mBottom);
					gGL.vertex2i((S32)bar, graph_rect.mTop);
					gGL.end();
				}
			}

			U64 cur_max = 0;
			for (S32 idx = 0; idx < FTV_DISPLAY_NUM; ++idx)
			{
				if (ft_display_table[idx].disabled > 1)
				{
					// Skip disabled timers
					continue;
				}

				// Fatten highlighted timer
				if (mHoverIndex == idx)
				{
					gGL.flush();
					gGL.lineWidth(3.f);
				}

				const F32* col = ft_display_table[idx].color->mV;

				F32 alpha = 1.f;

				if (mHoverIndex >= 0 && idx != mHoverIndex)
				{
					// Fade out non-hihglighted timers
					if (ft_display_table[idx].parent != mHoverIndex)
					{
						alpha = alpha_interp;
					}
				}

				gGL.color4f(col[0], col[1], col[2], alpha);
				gGL.begin(LLRender::LINE_STRIP);
				for (U32 j = 0; j < FTM_HISTORY_NUM; ++j)
				{
					U64 ticks = ticks_sum[j + 1][idx];

					if (mDisplayHz)
					{
						F64 tc = (F64)(ticks + 1) * iclock_freq;
						tc = 1000.f / tc;
						ticks = llmin((U64)tc, (U64)1024);
					}
					else if (mDisplayCalls)
					{
						S32 tidx = ft_display_table[idx].timer;
						S32 hidx = (LLFastTimer::sLastFrameIndex + j) %
								   FTM_HISTORY_NUM;
						ticks = (S32)LLFastTimer::sCallHistory[hidx][tidx];
					}

					if (alpha == 1.f)
					{
						// Normalize to highlighted timer
						cur_max = llmax(cur_max, ticks);
					}
					F32 x = graph_rect.mLeft +
							F32(graph_rect.getWidth()) /
							F32(FTM_HISTORY_NUM - 1) * j;
					F32 y = graph_rect.mBottom +
							F32(graph_rect.getHeight()) / max_ticks * ticks;
					gGL.vertex2f(x, y);
				}
				gGL.end();

				if (mHoverIndex == idx)
				{
					gGL.flush();
					gGL.lineWidth(1.f);
				}
			}

			// Interpolate towards new maximum
			F32 dt = gFrameIntervalSeconds * 3.f;
			last_max = (U64)((F32)last_max +
							 ((F32)cur_max - (F32)last_max) * dt);
			F32 alpha_target = last_max > cur_max ?
				llmin((F32)last_max / (F32)cur_max - 1.f, 1.f) :
				llmin((F32)cur_max / (F32)last_max - 1.f, 1.f);

			alpha_interp = alpha_interp + (alpha_target - alpha_interp) * dt;

			if (mHoverIndex >= 0)
			{
				x = (graph_rect.mRight + graph_rect.mLeft) / 2;
				y = graph_rect.mBottom + 8;
				mFont->renderUTF8(std::string(ft_display_table[mHoverIndex].desc),
								  0, x, y, LLColor4::white, LLFontGL::LEFT,
								  LLFontGL::BOTTOM);
			}
		}
	}

	// Output stats for clicked bar to log
	if (mPrintStats >= 0)
	{
		std::string legend_stat;
		S32 stat_num;
		S32 first = 1;
		for (stat_num = 0; stat_num < FTV_DISPLAY_NUM; ++stat_num)
		{
			if (ft_display_table[stat_num].disabled > 1)
			{
				continue;
			}
			if (!first)
			{
				legend_stat += ", ";
			}
			first = 0;
			legend_stat += ft_display_table[stat_num].desc;
		}
		llinfos << legend_stat << llendl;

		std::string timer_stat;
		first = 1;
		for (stat_num = 0; stat_num < FTV_DISPLAY_NUM; ++stat_num)
		{
			S32 disabled = ft_display_table[stat_num].disabled;
			if (disabled > 1)
			{
				continue;
			}
			if (!first)
			{
				timer_stat += ", ";
			}
			first = 0;
			U64 ticks;
			S32 tidx = ft_display_table[stat_num].timer;
			if (mPrintStats > 0)
			{
				S32 hidx = (LLFastTimer::sLastFrameIndex + mPrintStats - 1 -
							mScrollIndex) % FTM_HISTORY_NUM;
				ticks = disabled >= 1 ? ticks_sum[mPrintStats][stat_num]
									  : LLFastTimer::sCountHistory[hidx][tidx];
			}
			else
			{
				ticks = disabled >= 1 ? ticks_sum[0][stat_num]
									  : LLFastTimer::sCountAverage[tidx];
			}
			F32 ms = (F32)((F64)ticks * iclock_freq);

			timer_stat += llformat("%.1f", ms);
		}
		llinfos << timer_stat << llendl;
		mPrintStats = -1;
	}

	mHoverIndex = -1;
	mHoverBarIndex = -1;

	mFirstDrawLoop = false;

	LLView::draw();
}

F64 LLFastTimerView::getTime(LLFastTimer::EFastTimerType tidx)
{
	// Find table index
	S32 i;
	for (i = 0; i < FTV_DISPLAY_NUM; ++i)
	{
		if (tidx == ft_display_table[i].timer)
		{
			break;
		}
	}

	if (i == FTV_DISPLAY_NUM)
	{
		// Walked off the end of ft_display_table without finding the desired
		// timer type
		llwarns << "Timer type " << tidx << " not known." << llendl;
		return 0.0;
	}

	S32 table_idx = i;

	// Add child ticks to parent
	U64 ticks = LLFastTimer::sCountAverage[tidx];
	S32 level = ft_display_table[table_idx].level;
	for (i = table_idx + 1; i < FTV_DISPLAY_NUM; ++i)
	{
		if (ft_display_table[i].level <= level)
		{
			break;
		}
		ticks += LLFastTimer::sCountAverage[ft_display_table[i].timer];
	}

	return (F64)ticks / (F64)LLFastTimer::countsPerSecond();
}

#endif	// LL_FAST_TIMERS_ENABLED

#if TRACY_ENABLE

//static
LLProcessLauncher* HBTracyProfiler::sProcess = NULL;

//static
bool HBTracyProfiler::running()
{
	return sProcess && sProcess->isRunning();
}

//static
void HBTracyProfiler::launch()
{
	if (running() || !gDirUtilp)
	{
		return;
	}

	std::string exe_path = gDirUtilp->getExecutableDir();
#if LL_DARWIN
	exe_path += "/../Resources/tracy";
#elif LL_WINDOWS
	exe_path += "\\Tracy.exe";
#else // LL_LINUX
	exe_path += "/tracy";
#endif
	if (!LLFile::isfile(exe_path))
	{
		llwarns << "Tracy profiler executable not found. Cannot launch it."
				<< llendl;
		return;
	}

	if (sProcess)
	{
		sProcess->kill();
		sProcess->clearArguments();
	}
	else
	{
		sProcess = new LLProcessLauncher();
	}
	if (gDirUtilp)
	{
		sProcess->setWorkingDirectory(gDirUtilp->getOSUserDir());
	}
	sProcess->setExecutable(exe_path);
	sProcess->addArgument("-a");
	sProcess->addArgument("127.0.0.1");
	if (sProcess->launch() != 0)
	{
		llwarns << "Failed to launch the Tracy profiler executable." << llendl;
	}
}

//static
void HBTracyProfiler::detach()
{
	if (sProcess)
	{
		sProcess->orphan();
		delete sProcess;
		sProcess = NULL;
	}
}

//static
void HBTracyProfiler::kill()
{
	if (sProcess)
	{
		delete sProcess;
		sProcess = NULL;
	}
}

#endif	// TRACY_ENABLE
