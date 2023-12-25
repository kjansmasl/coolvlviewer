/**
 * @file llviewerdisplay.cpp
 * @brief LLViewerDisplay class implementation
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

#include "llviewerdisplay.h"

#include "llapp.h"
#include "llcubemap.h"
#include "lldir.h"
#include "llfasttimer.h"
#include "llimagebmp.h"
#include "llimagedecodethread.h"
#include "llimagegl.h"
#include "llrender.h"

#include "llagent.h"
#include "llappviewer.h"
#include "lldynamictexture.h"
#include "lldrawpoolalpha.h"
#include "lldrawpoolbump.h"
#include "lldrawpoolwater.h"
#include "llenvironment.h"
#include "llfeaturemanager.h"
#include "llfloatertools.h"
#include "llgltfmateriallist.h"
#include "llgridmanager.h"			// For gIsInProductionGrid
#include "llhudmanager.h"
#include "llpipeline.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llselectmgr.h"
#include "llsky.h"
#include "llspatialpartition.h"
#include "llstartup.h"
#include "lltooldraganddrop.h"
#include "lltoolfocus.h"
#include "lltoolmgr.h"
#include "lltoolpie.h"
#include "lltracker.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerobjectlist.h"
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"
#include "llviewershadermgr.h"
#include "llviewertexturelist.h"
#include "llviewerwindow.h"
#include "llvoavatarself.h"
#include "llvograss.h"
#include "llworld.h"

extern LLPointer<LLViewerTexture> gStartTexture;

LLPointer<LLViewerTexture> gDisconnectedImagep = NULL;

// This is how long the sim will try to teleport you before giving up.
constexpr F32 TELEPORT_EXPIRY = 15.f;
// Additional time (in seconds) to wait per attachment
constexpr F32 TELEPORT_EXPIRY_PER_ATTACHMENT = 3.f;

// Constants used to toggle renderer back on after teleport

// Time to preload the world before raising the curtain after we've actually
// already arrived.
constexpr F32 TELEPORT_ARRIVAL_DELAY = 2.f;
// Delay to prevent teleports after starting an in-sim teleport.
constexpr F32 TELEPORT_LOCAL_DELAY = 1.f;

// Wait this long while reloading textures before we raise the curtain
constexpr F32 RESTORE_GL_TIME = 5.f;

// Globals

LLFrameTimer gTeleportDisplayTimer;
LLFrameTimer gTeleportArrivalTimer;
bool gTeleportDisplay = false;
bool gUpdateDrawDistance = false;
F32 gSavedDrawDistance = 0.f;

bool gForceRenderLandFence = false;
bool gDisplaySwapBuffers = false;
bool gDepthDirty = false;
bool gResizeScreenTexture = false;
bool gResizeShadowTexture = false;
bool gSnapshot = false;
bool gCubeSnapshot = false;
bool gShaderProfileFrame = false;
bool gScreenIsDirty = false;

U32 gRecentFrameCount = 0; // number of 'recent' frames
U32 gLastFPSAverage = 0;
LLFrameTimer gRecentFPSTime;
LLFrameTimer gRecentMemoryTime;

// Rendering stuff
void render_hud_attachments();
void render_ui_3d();
void render_ui_2d();
void render_disconnected_background();

void display_startup()
{
	if (!gViewerWindowp || !gViewerWindowp->getActive() ||
		!gWindowp->getVisible() || gWindowp->getMinimized())
	{
		return;
	}

	gPipeline.updateGL();

	if (LLViewerFetchedTexture::sWhiteImagep.notNull())
	{
		LLTexUnit::sWhiteTexture =
			LLViewerFetchedTexture::sWhiteImagep->getTexName();
	}

	LLGLSDefault gls_default;

	// Required for HTML update in login screen
	static S32 frame_count = 0;

	LL_GL_CHECK_STATES;

	if (frame_count++ > 1) // Make sure we have rendered a frame first
	{
		LLViewerDynamicTexture::updateAllInstances();
	}

	LL_GL_CHECK_STATES;

#if 0	// Only enable this should the EE renderer be gutted out. HB
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
#else
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
#endif

	LLGLSUIDefault gls_ui;
	gPipeline.disableLights();

	gViewerWindowp->setup2DRender();

	gViewerWindowp->draw();
	gGL.flush();

	LLVertexBuffer::unbind();

	LL_GL_CHECK_STATES;

	gWindowp->swapBuffers();
	glClear(GL_DEPTH_BUFFER_BIT);
}

void display_update_camera()
{
	F32 farclip;
	if (gCubeSnapshot)
	{
		static LLCachedControl<F32> dist(gSavedSettings,
										 "RenderReflectionProbeDrawDistance");
		farclip = llclamp(F32(dist), 32.f, 1024.f);
	}
	else
	{
		farclip = gAgent.mDrawDistance;
	}

	gViewerCamera.setFar(farclip);

	gViewerWindowp->setup3DRender();

	if (!gCubeSnapshot)
	{
		// Update land visibility
		gWorld.setLandFarClip(farclip);
	}
}

// Write some stats to llinfos
void display_stats()
{
	static LLCachedControl<S32> background_yield_time(gSavedSettings,
													  "BackgroundYieldTime");
	if (!gWindowp->getVisible() ||
		(background_yield_time > 0 && !gFocusMgr.getAppHasFocus()))
	{
		// Do not keep FPS statistics while yielding cooperatively or not
		// visible.
		gRecentFrameCount = 0;
		gRecentFPSTime.reset();
	}
	static LLCachedControl<F32> fps_log_freq(gSavedSettings,
											 "FPSLogFrequency");
	if (fps_log_freq > 0.f)
	{
		F32 ellapsed = gRecentFPSTime.getElapsedTimeF32();
		if (ellapsed >= fps_log_freq)
		{
			F32 fps = gRecentFrameCount / ellapsed;
			llinfos << llformat("FPS: %.02f", fps) << llendl;
			gLastFPSAverage = (U32)fps;
			gRecentFrameCount = 0;
			gRecentFPSTime.reset();
		}
		else if (ellapsed >= 10.f)
		{
			gLastFPSAverage = (U32)gRecentFrameCount / ellapsed;
		}
	}
	static LLCachedControl<F32> mem_log_freq(gSavedSettings,
											 "MemoryLogFrequency");
	if (mem_log_freq > 0.f &&
		gRecentMemoryTime.getElapsedTimeF32() >= mem_log_freq)
	{
		gMemoryAllocated = LLMemory::getCurrentRSS();
		U32 memory = (U32)(gMemoryAllocated / 1048576UL);
		llinfos << llformat("MEMORY: %d MB", memory) << llendl;
		gRecentMemoryTime.reset();
	}
}

static void update_tp_display(bool minimized)
{
	S32 attach_count = 0;
	if (isAgentAvatarValid())
	{
		attach_count = gAgentAvatarp->getNumAttachments();
	}
	F32 teleport_save_time = TELEPORT_EXPIRY +
							 TELEPORT_EXPIRY_PER_ATTACHMENT * attach_count;
	F32 teleport_elapsed = gTeleportDisplayTimer.getElapsedTimeF32();
	F32 teleport_percent = teleport_elapsed * 100.f / teleport_save_time;
	LLAgent::ETeleportState tp_state = gAgent.getTeleportState();
	if (teleport_percent > 100.f && tp_state != LLAgent::TELEPORT_START &&
		tp_state != LLAgent::TELEPORT_QUEUED)
	{
		// Give up. Do not keep the UI locked forever.
		LL_DEBUGS("Teleport") << "TP timeout ?... Resetting to TELEPORT_NONE"
							  << LL_ENDL;
		gAgent.setTeleportState(LLAgent::TELEPORT_NONE);
		gAgent.setTeleportMessage("");
	}

	if (minimized)
	{
		gViewerWindowp->setShowProgress(false);
	}

	static LLCachedControl<bool> hide_tp_progress(gSavedSettings,
												  "HideTeleportProgress");
	bool show_tp_progress = !hide_tp_progress && !minimized;
	const std::string& message = gAgent.getTeleportMessage();
	switch (gAgent.getTeleportState())
	{
		case LLAgent::TELEPORT_NONE:
			// No teleport in progress
			gViewerWindowp->setShowProgress(false);
			gTeleportDisplay = false;
			gTeleportArrivalTimer.reset();
			break;

		case LLAgent::TELEPORT_START:
			// Transition to REQUESTED. Viewer has sent some kind of
			// TeleportRequest to the source simulator.
			gTeleportDisplayTimer.reset();
			if (show_tp_progress)
			{
				gViewerWindowp->setShowProgress(true);
				gViewerWindowp->setProgressPercent(0);
				gAgent.setTeleportMessage(LLAgent::sTeleportProgressMessages["requesting"]);
			}
			// Release geometry from old location
			gPipeline.resetVertexBuffers();
			LLSpatialPartition::sTeleportRequested = true;
			gAgent.setTeleportState(LLAgent::TELEPORT_REQUESTED);
			break;

		case LLAgent::TELEPORT_REQUESTED:
			// Waiting for source simulator to respond
			if (show_tp_progress)
			{
				gViewerWindowp->setProgressPercent(llmin(teleport_percent,
														 37.5f));
				gViewerWindowp->setProgressString(message);
			}
			break;

		case LLAgent::TELEPORT_MOVING:
			// Viewer has received destination location from source simulator
			if (show_tp_progress)
			{
				gViewerWindowp->setProgressPercent(llmin(teleport_percent,
														 75.f));
				gViewerWindowp->setProgressString(message);
			}
			break;

		case LLAgent::TELEPORT_START_ARRIVAL:
			// Transition to ARRIVING. Viewer has received avatar update, etc,
			// from destination simulator
			gTeleportArrivalTimer.reset();
			if (show_tp_progress)
			{
				gViewerWindowp->setProgressCancelButtonVisible(false);
				gViewerWindowp->setProgressPercent(75.f);
				gAgent.setTeleportMessage(LLAgent::sTeleportProgressMessages["arriving"]);
			}
			gAgent.setTeleportState(LLAgent::TELEPORT_ARRIVING);
			if (gSavedSettings.getBool("DisablePrecacheDelayAfterTP"))
			{
				LL_DEBUGS("Teleport") << "No pre-caching, switching to TELEPORT_NONE"
									  << LL_ENDL;
				gAgent.setTeleportState(LLAgent::TELEPORT_NONE);
			}
			break;

		case LLAgent::TELEPORT_ARRIVING:
		{
			// Make the user wait while content "pre-caches"
			F32 percent = gTeleportArrivalTimer.getElapsedTimeF32() /
						  TELEPORT_ARRIVAL_DELAY;
			if (!show_tp_progress || percent > 1.f)
			{
				percent = 1.f;
				LL_DEBUGS("Teleport") << "Arrived. Switching to TELEPORT_NONE"
									  << LL_ENDL;
				gAgent.setTeleportState(LLAgent::TELEPORT_NONE);
			}
			if (show_tp_progress)
			{
				gViewerWindowp->setProgressCancelButtonVisible(false);
				gViewerWindowp->setProgressPercent(percent * 25.f + 75.f);
				gViewerWindowp->setProgressString(message);
			}
			break;
		}

		case LLAgent::TELEPORT_LOCAL:
			// Short delay when teleporting in the same sim (progress screen
			// active but not shown; did not fall-through from TELEPORT_START)
			if (gTeleportDisplayTimer.getElapsedTimeF32() > TELEPORT_LOCAL_DELAY)
			{
				LL_DEBUGS("Teleport") << "Local TP done, switching to TELEPORT_NONE"
									  << LL_ENDL;
				gAgent.setTeleportState(LLAgent::TELEPORT_NONE);
			}
			break;

		case LLAgent::TELEPORT_QUEUED:
			gTeleportDisplayTimer.reset();
			if (show_tp_progress)
			{
				gViewerWindowp->setShowProgress(true);
				gViewerWindowp->setProgressPercent(0);
				gAgent.setTeleportMessage(LLAgent::sTeleportProgressMessages["requesting"]);
			}
			gAgent.fireQueuedTeleport();

		default:
			 break;
	}
}

// Paint the display
void display(bool rebuild, F32 zoom_factor, S32 subfield, bool for_snapshot)
{
	LL_FAST_TIMER(FTM_RENDER);

	if (!gViewerWindowp) return;

	static LLCachedControl<bool> use_pbr(gSavedSettings, "RenderUsePBR");
	if (gUsePBRShaders != use_pbr)
	{
		gPipeline.toggleRenderer();
	}

	stop_glerror();

	if (gResizeScreenTexture)
	{
		// Skip render on frames where window has been resized
		gGL.flush();
		glClear(GL_COLOR_BUFFER_BIT);
		gWindowp->swapBuffers();
		gPipeline.resizeScreenTexture();
		return;
	}
	if (gResizeShadowTexture)
	{
		gPipeline.resizeShadowTexture();
	}

	if (LLPipeline::sRenderDeferred)
	{
		// *HACK: to make sky show up in deferred snapshots
		for_snapshot = false;
	}

	if (LLPipeline::sRenderFrameTest)
	{
		LLWorld::sendAgentPause();
	}

	gSnapshot = for_snapshot;

	LLGLSDefault gls_default;
	LLGLDepthTest gls_depth(GL_TRUE, GL_TRUE, GL_LEQUAL);

	LLVertexBuffer::unbind();

	LL_GL_CHECK_STATES;

	gPipeline.disableLights();

	// Reset vertex buffers if needed
	gPipeline.doResetVertexBuffers();

	stop_glerror();

	// Do not draw if the window is hidden or minimized. In fact, we must
	// explicitly check the minimized state before drawing. Attempting to draw
	// into a minimized window causes a GL error. JC
	if (!gViewerWindowp->getActive() || !gWindowp->getVisible() ||
		gWindowp->getMinimized())
	{
		// Clean up memory the pools may have allocated
		if (rebuild)
		{
			gPipeline.rebuildPools();
		}

		// Avoid accumulating HUD objects while minimized. HB
		LLHUDObject::removeExpired();

		gViewerWindowp->returnEmptyPicks();

		// We still need to update the teleport progress (to get changes done
		// in TP states, else the sim does not get the messages signaling the
		// agent's arrival). Of course, we do not show/update the TP screen.
		// This fixes BUG-230616. HB
		if (gTeleportDisplay)
		{
			update_tp_display(true);
		}

		return;
	}

	gViewerWindowp->checkSettings();

	{
		LL_FAST_TIMER(FTM_PICK);
		gViewerWindowp->performPick();
	}

	LL_GL_CHECK_STATES;

	//////////////////////////////////////////////////////////
	// Logic for forcing window updates if we are in drone mode.

	// Bail out if we are in the startup state and do not want to try to render
	// the world.
	if (!LLStartUp::isLoggedIn())
	{
		display_startup();
		gScreenIsDirty = false;
		return;
	}

	if (gShaderProfileFrame)
	{
		LLGLSLShader::initProfile();
	}

#if 0
	LLGLState::verify(false);
#endif

	/////////////////////////////////////////////////
	// Update GL Texture statistics (used for discard logic ?)

	stop_glerror();

	LLImageGL::updateStats(gFrameTimeSeconds);

	static LLCachedControl<S32> render_name(gSavedSettings, "RenderName");
	static LLCachedControl<bool> hide_all_titles(gSavedSettings,
												 "RenderHideGroupTitleAll");
	LLVOAvatar::sRenderName = render_name;
	LLVOAvatar::sRenderGroupTitles = !hide_all_titles;

	gPipeline.mBackfaceCull = true;
	++gRecentFrameCount;
	gGL.cleanupVertexBufferCache(++gFrameCount);

	//////////////////////////////////////////////////////////
	// Display start screen if we are teleporting, and skip render

	if (gTeleportDisplay)
	{
		update_tp_display(false);
	}
    else if (gAppViewerp->logoutRequestSent())
	{
		F32 percent_done = gLogoutTimer.getElapsedTimeF32() * 100.f /
						   gLogoutMaxTime;
		if (percent_done > 100.f)
		{
			percent_done = 100.f;
		}

		if (LLApp::isExiting())
		{
			percent_done = 100.f;
		}

		gViewerWindowp->setProgressPercent(percent_done);
	}
	else if (gRestoreGL)
	{
		F32 percent_done = gRestoreGLTimer.getElapsedTimeF32() * 100.f /
						   RESTORE_GL_TIME;
		if (percent_done > 100.f || LLApp::isExiting())
		{
			gViewerWindowp->setShowProgress(false);
			gRestoreGL = false;
		}
		else
		{
			gViewerWindowp->setProgressPercent(percent_done);
		}
	}
	// Progressively increase draw distance after TP when required and when
	// possible (enough available memory). HB
	else if (gSavedDrawDistance > 0.f && !gAgent.teleportInProgress() &&
			 LLViewerTexture::sDesiredDiscardBias <= 2.5f)
	{
		static LLCachedControl<U32> speed_rez_interval(gSavedSettings,
													   "SpeedRezInterval");
		if (gTeleportArrivalTimer.getElapsedTimeF32() >= (F32)speed_rez_interval)
		{
			gTeleportArrivalTimer.reset();
			F32 current = gSavedSettings.getF32("RenderFarClip");
			if (gSavedDrawDistance > current)
			{
				current *= 2.f;
				if (current > gSavedDrawDistance)
				{
					current = gSavedDrawDistance;
				}
				gSavedSettings.setF32("RenderFarClip", current);
			}
			if (current >= gSavedDrawDistance)
			{
				gSavedDrawDistance = 0.f;
				gSavedSettings.setF32("SavedRenderFarClip", 0.f);
			}
		}
	}

	// We do this here instead of inside of handleRenderFarClipChanged() in
	// llviewercontrol.cpp to ensure this is not done during rendering, which
	// would cause drawables to get destroyed while LLSpatialGroup::sNoDelete
	// is true and would therefore cause a mess. HB
	if (gUpdateDrawDistance)
	{
		gUpdateDrawDistance = false;
		F32 draw_distance = gSavedSettings.getF32("RenderFarClip");
		gAgent.mDrawDistance = draw_distance;
		gWorld.setLandFarClip(draw_distance);
		LLVOCacheEntry::updateSettings();
	}

	//////////////////////////
	// Prepare for the next frame

	// Update the camera
	gViewerCamera.setZoomParameters(zoom_factor, subfield);
	gViewerCamera.setNear(MIN_NEAR_PLANE);

	if (gDisconnected)
	{
		render_ui();
	}

	//////////////////////////
	// Set rendering options

	stop_glerror();

	///////////////////////////////////////
	// Slam lighting parameters back to our defaults.
	// Note that these are not the same as GL defaults...

	gGL.setAmbientLightColor(LLColor4::white);

	/////////////////////////////////////
	// Render
	//
	// Actually push all of our triangles to the screen.

	// Do render-to-texture stuff here
	if (gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_DYNAMIC_TEXTURES))
	{
		LL_FAST_TIMER(FTM_UPDATE_TEXTURES);
		if (LLViewerDynamicTexture::updateAllInstances())
		{
			gGL.setColorMask(true, true);
			glClear(GL_DEPTH_BUFFER_BIT);
		}
	}

	gViewerWindowp->setupViewport();

	// Reset per-frame statistics.
	gPipeline.resetFrameStats();
	LLViewerTextureList::resetFrameStats();

	if (!gDisconnected)
	{
		if (gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_HUD))
		{
			// Do not draw hud objects in this frame
			gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_HUD);
		}

		if (gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_HUD_PARTICLES))
		{
			// Do not draw hud particles in this frame
			gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_HUD_PARTICLES);
		}

		display_update_camera();

		// Update all the sky/atmospheric/water settings
		gEnvironment.update();
		stop_glerror();

		{
			LL_FAST_TIMER(FTM_HUD_UPDATE);
			LLHUDManager::updateEffects();
			LLHUDObject::updateAll();
			stop_glerror();
		}

		{
			LL_FAST_TIMER(FTM_DISPLAY_UPDATE_GEOM);
			// 50 ms/second update time:
			const F32 max_geom_update_time = 0.05f * gFrameIntervalSeconds;
			gPipeline.createObjects(max_geom_update_time);
			gPipeline.processPartitionQ();
			gPipeline.updateGeom(max_geom_update_time);
			stop_glerror();
		}

		gPipeline.updateGL();
		stop_glerror();

		// Increment drawable frame counter
		LLDrawable::incrementVisible();

		LLSpatialGroup::sNoDelete = true;
		if (LLViewerFetchedTexture::sWhiteImagep.notNull())
		{
			LLTexUnit::sWhiteTexture =
				LLViewerFetchedTexture::sWhiteImagep->getTexName();
		}

		S32 occlusion = LLPipeline::sUseOcclusion;
		if (gDepthDirty)
		{
			// Depth buffer is invalid, do not overwrite occlusion state
			LLPipeline::sUseOcclusion = llmin(occlusion, 1);
			gDepthDirty = false;
		}

		LL_GL_CHECK_STATES;

		static LLCullResult result;
		LLViewerCamera::sCurCameraID = LLViewerCamera::CAMERA_WORLD;
		LLPipeline::sUnderWaterRender = gViewerCamera.cameraUnderWater();
		gPipeline.updateCull(gViewerCamera, result);

		LL_GL_CHECK_STATES;

		{
			if (gResizeScreenTexture)
			{
				gPipeline.resizeScreenTexture();
			}

			gGL.setColorMask(true, true);
			glClearColor(0.f, 0.f, 0.f, 0.f);

			LL_GL_CHECK_STATES;

			if (!for_snapshot)
			{
				if (gFrameCount > 1)
				{
					// For some reason, ATI 4800 series will error out if you
					// try to generate a shadow before the first frame is
					// through
					gPipeline.generateSunShadow();
				}

				LLVertexBuffer::unbind();

				LL_GL_CHECK_STATES;

				const LLMatrix4a proj = gGLProjection;
				const LLMatrix4a mod = gGLModelView;
				glViewport(0, 0, 512, 512);

				{
					LL_FAST_TIMER(FTM_IMPOSTORS_UPDATE);
					LLViewerCamera::sCurCameraID =
						LLViewerCamera::CAMERA_WORLD;
					LLVOAvatar::updateImpostors();
				}

				gGLProjection = proj;
				gGLModelView = mod;
				gGL.matrixMode(LLRender::MM_PROJECTION);
				gGL.loadMatrix(proj);
				gGL.matrixMode(LLRender::MM_MODELVIEW);
				gGL.loadMatrix(mod);
				gViewerWindowp->setupViewport();

				LL_GL_CHECK_STATES;
			}

			if (gUsePBRShaders)
			{
				glClear(GL_DEPTH_BUFFER_BIT);
			}
			else
			{
				glClear(GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
			}
		}

		if (!gUsePBRShaders)
		{
			gPipeline.generateWaterReflection();
			if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_PHYSICS_SHAPES))
			{
				gPipeline.renderPhysicsDisplay();
			}
			LL_GL_CHECK_STATES;
		}

		//////////////////////////////////////
		// Update images, using the image stats generated during object update
		// and culling. This may put objects onto the retextured list. Doing
		// this here gives hardware occlusion queries extra time to complete...
		{
			LL_FAST_TIMER(FTM_IMAGE_UPDATE);

			{
				LL_FAST_TIMER(FTM_IMAGE_UPDATE_CLASS);
				LLViewerTexture::updateClass();
			}

			{
				LL_FAST_TIMER(FTM_IMAGE_UPDATE_BUMP);
				// Must be called before gTextureList version so that its
				// textures are thrown out first.
				gBumpImageList.updateImages();
			}

			{
				LL_FAST_TIMER(FTM_IMAGE_UPDATE_LIST);
				F32 max_image_decode_time = 0.2f * gFrameIntervalSeconds;
				// Min 2ms/frame, max 20ms/frame)
				max_image_decode_time = llclamp(max_image_decode_time, 0.002f,
												0.02f);
				gTextureList.updateImages(max_image_decode_time);
			}

			{
				// Remove dead gltf materials
				gGLTFMaterialList.flushMaterials();
			}
		}

		///////////////////////////////////
		// StateSort
		//
		// Responsible for taking visible objects, and adding them to the
		// appropriate draw orders. In the case of alpha objects, z-sorts them
		// first. Also creates special lists for outlines and selected face
		// rendering.

		{
			LLViewerCamera::sCurCameraID = LLViewerCamera::CAMERA_WORLD;
			gPipeline.stateSort(gViewerCamera, result);
			if (rebuild)
			{
				// Rebuild pools
				gPipeline.rebuildPools();
			}
		}

		LL_GL_CHECK_STATES;

		LLPipeline::sUseOcclusion = occlusion;

		{
			LL_FAST_TIMER(FTM_UPDATE_SKY);
			gSky.updateSky();
		}

		if (gUseWireframe)
		{
			glClearColor(0.5f, 0.5f, 0.5f, 0.f);
			glClear(GL_COLOR_BUFFER_BIT);
			if (!gUsePBRShaders)
			{
				glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
			}
		}

		LLPipeline::sUnderWaterRender = gViewerCamera.cameraUnderWater();

		LL_GL_CHECK_STATES;

		gGL.setColorMask(true, true);

		if (LLPipeline::sRenderDeferred)
		{
			gPipeline.mRT->mDeferredScreen.bindTarget();
			if (gUsePBRShaders && gUseWireframe)
			{
				glClearColor(0.5f, 0.5f, 0.5f, 1.f);
			}
			else
			{
				glClearColor(1.f, 0.f, 1.f, 1.f);
			}
			gPipeline.mRT->mDeferredScreen.clear();
		}
		else
		{
			gPipeline.mRT->mScreen.bindTarget();
			if (LLPipeline::sUnderWaterRender &&
				!gPipeline.canUseWindLightShaders())
			{
				const LLColor4& col = LLDrawPoolWater::sWaterFogColor;
				glClearColor(col.mV[0], col.mV[1], col.mV[2], 0.f);
			}
			gPipeline.mRT->mScreen.clear();
		}

		gGL.setColorMask(true, false);

		if (!gRestoreGL &&
			!(gAppViewerp->logoutRequestSent() &&
			  gAppViewerp->hasSavedFinalSnapshot()))
		{
			LLViewerCamera::sCurCameraID = LLViewerCamera::CAMERA_WORLD;

			if (gUsePBRShaders)
			{
				gGL.setColorMask(true, true);
				gPipeline.renderGeomDeferred(gViewerCamera, true);
			}
			else
			{
				gGL.setColorMask(true, false);
				if (LLPipeline::sRenderDeferred)
				{
					gPipeline.renderGeomDeferred(gViewerCamera);
				}
				else
				{
					gPipeline.renderGeom(gViewerCamera);
				}
				gGL.setColorMask(true, true);

				// Store this frame's modelview matrix for use when rendering
				// next frame's occlusion queries
				gGLLastModelView = gGLModelView;
				gGLLastProjection = gGLProjection;
			}

			stop_glerror();
		}

		{
			LL_FAST_TIMER(FTM_TEXTURE_UNBIND);
			for (S32 i = 0; i < gGLManager.mNumTextureImageUnits; ++i)
			{
				// Dummy cleanup of any currently bound textures
				LLTexUnit* unitp = gGL.getTexUnit(i);
				if (!unitp) continue;	// Paranoia

				LLTexUnit::eTextureType type = unitp->getCurrType();
				if (type != LLTexUnit::TT_NONE)
				{
					unitp->unbind(type);
					unitp->disable();
				}
			}
		}

		LLRenderTarget& rt =
			LLPipeline::sRenderDeferred ? gPipeline.mRT->mDeferredScreen
										: gPipeline.mRT->mScreen;
		rt.flush();
		if (!gUsePBRShaders && rt.getFBO() && LLRenderTarget::sUseFBO)
		{
			LLRenderTarget::copyContentsToFramebuffer(rt, 0, 0, rt.getWidth(),
													  rt.getHeight(), 0, 0,
													  rt.getWidth(),
													  rt.getHeight(),
													  GL_DEPTH_BUFFER_BIT,
													  GL_NEAREST);
		}

		if (LLPipeline::sRenderDeferred)
		{
			gPipeline.renderDeferredLighting();
		}

		LLPipeline::sUnderWaterRender = false;

		if (!for_snapshot)
		{
			render_ui();
		}

		LLSpatialGroup::sNoDelete = false;
		gPipeline.clearReferences();
	}

	stop_glerror();

	if (LLPipeline::sRenderFrameTest)
	{
		LLWorld::sendAgentResume();
		LLPipeline::sRenderFrameTest = false;
	}

	display_stats();

	gShiftFrame = gScreenIsDirty = false;

	if (gShaderProfileFrame)
	{
		gShaderProfileFrame = false;
		LLGLSLShader::finishProfile();
	}
}

// For use by the PBR renderer only.
void display_cube_face()
{
	if (gRestoreGL || gSnapshot || gTeleportDisplay || !gPipeline.isInit() ||
		gAppViewerp->logoutRequestSent())
	{
		return;
	}

	LLGLSDefault gls_default;
	LLGLDepthTest gls_depth(GL_TRUE, GL_TRUE, GL_LEQUAL);

	LLVertexBuffer::unbind();

	gPipeline.disableLights();
	gPipeline.mBackfaceCull = true;

	gViewerWindowp->setupViewport();

	// Do not render HUDs in this frame
	if (gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_HUD))
	{
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_HUD);
	}
	if (gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_HUD_PARTICLES))
	{
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_HUD_PARTICLES);
	}

	display_update_camera();

	// We need to update environment related uniforms and mark them dirty in
	// shaders used during the cube snapshot.
	gEnvironment.updateSettingsUniforms();
	gEnvironment.dirtyUniforms();

	LLSpatialGroup::sNoDelete = true;

	{	// Occlusion data is from main camera point of view, do not read or
		// write it during cube snapshots.
		LLDisableOcclusionCulling no_occlusion;

		static LLCullResult result;
		LLViewerCamera::sCurCameraID = LLViewerCamera::CAMERA_WORLD;
		LLPipeline::sUnderWaterRender = gViewerCamera.cameraUnderWater();
		gPipeline.updateCull(gViewerCamera, result);

		gGL.setColorMask(true, true);
		glClearColor(0.f, 0.f, 0.f, 0.f);
		gPipeline.generateSunShadow();

		glClear(GL_DEPTH_BUFFER_BIT);

		LLViewerCamera::sCurCameraID = LLViewerCamera::CAMERA_WORLD;
		gPipeline.stateSort(gViewerCamera, result);
	}

	LLPipeline::sUnderWaterRender = gViewerCamera.cameraUnderWater();
	gGL.setColorMask(true, true);

	gPipeline.mRT->mDeferredScreen.bindTarget();
	if (gUseWireframe)
	{
		glClearColor(0.5f, 0.5f, 0.5f, 1.f);
	}
	else
	{
		glClearColor(1.f, 0.f, 1.f, 1.f);
	}
	gPipeline.mRT->mDeferredScreen.clear();
	LLViewerCamera::sCurCameraID = LLViewerCamera::CAMERA_WORLD;
	gPipeline.renderGeomDeferred(gViewerCamera, false);
	gPipeline.mRT->mDeferredScreen.flush();

	gPipeline.renderDeferredLighting();

	LLPipeline::sUnderWaterRender = false;
	LLSpatialGroup::sNoDelete = false;
	gPipeline.clearReferences();
}

void render_hud_attachments()
{
	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.pushMatrix();
	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.pushMatrix();

	const LLMatrix4a current_proj = gGLProjection;
	const LLMatrix4a current_mod = gGLModelView;

	F32 current_zoom = gAgent.mHUDCurZoom;
	F32 target_zoom = gAgent.getHUDTargetZoom();
	if (current_zoom != target_zoom)
	{
		// Smoothly interpolate current zoom level
		gAgent.mHUDCurZoom = lerp(current_zoom, target_zoom,
								  LLCriticalDamp::getInterpolant(0.03f));
	}

	if (LLPipeline::sShowHUDAttachments && !gDisconnected &&
		setup_hud_matrices())
	{
		LLPipeline::sRenderingHUDs = true;
		LLCamera hud_cam = gViewerCamera;
		hud_cam.setOrigin(-1.f, 0.f, 0.f);
		hud_cam.setAxes(LLVector3::x_axis, LLVector3::y_axis,
						LLVector3::z_axis);
		LLViewerCamera::updateFrustumPlanes(hud_cam, true);

		static LLCachedControl<bool> render_hud_particles(gSavedSettings,
														  "RenderHUDParticles");
		bool render_particles =
			render_hud_particles &&
			gPipeline.hasRenderType(LLPipeline::RENDER_TYPE_PARTICLES);

		// Only render hud objects
		gPipeline.pushRenderTypeMask();

		// Turn off everything
		gPipeline.andRenderTypeMask(LLPipeline::END_RENDER_TYPES);
		// Turn on HUD
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_HUD);
		// Turn on HUD particles
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_HUD_PARTICLES);

		// If particles are off, turn off hud-particles as well
		if (!render_particles)
		{
			// Turn back off HUD particles
			gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_HUD_PARTICLES);
		}

		bool has_ui =
			gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_UI);
		if (has_ui)
		{
			gPipeline.toggleRenderDebugFeature((void*)LLPipeline::RENDER_DEBUG_FEATURE_UI);
		}

		// Disable occlusion from now on and until end of context
		LLDisableOcclusionCulling no_occlusion;

		// Cull, sort, and render hud objects
		static LLCullResult result;
		LLSpatialGroup::sNoDelete = true;

		LLViewerCamera::sCurCameraID = LLViewerCamera::CAMERA_WORLD;
		gPipeline.updateCull(hud_cam, result, NULL, true);

		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_BUMP);
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_SIMPLE);
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_VOLUME);
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_ALPHA);
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_ALPHA_MASK);
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_FULLBRIGHT_ALPHA_MASK);
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_FULLBRIGHT);
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_PASS_ALPHA);
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_PASS_ALPHA_MASK);
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_PASS_BUMP);
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_PASS_MATERIAL);
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_PASS_FULLBRIGHT);
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_PASS_FULLBRIGHT_ALPHA_MASK);
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_PASS_FULLBRIGHT_SHINY);
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_PASS_SHINY);
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_PASS_INVISIBLE);
		gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_PASS_INVISI_SHINY);
		if (gUsePBRShaders)
		{
			gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_ALPHA_PRE_WATER);
			gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_MAT_PBR);
			gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_MAT_PBR_ALPHA_MASK);
			gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_PASS_MAT_PBR);
			gPipeline.toggleRenderType(LLPipeline::RENDER_TYPE_PASS_MAT_ALPHA_MASK_PBR);
		}

		gPipeline.stateSort(hud_cam, result);

		if (gUsePBRShaders)
		{
			gPipeline.renderGeomPostDeferred(hud_cam);
		}
		else
		{
			gPipeline.renderGeom(hud_cam);
		}

		LLSpatialGroup::sNoDelete = false;
#if 0
		gPipeline.clearReferences();
#endif
		render_hud_elements();

		// Restore type mask
		gPipeline.popRenderTypeMask();

		if (has_ui)
		{
			gPipeline.toggleRenderDebugFeature((void*)LLPipeline::RENDER_DEBUG_FEATURE_UI);
		}
		LLPipeline::sRenderingHUDs = false;
	}
	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.popMatrix();
	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.popMatrix();

	gGLProjection = current_proj;
	gGLModelView = current_mod;
}

bool setup_hud_matrices()
{
	LLRect whole_screen = gViewerWindowp->getVirtualWindowRect();

	// Apply camera zoom transform (for high res screenshots)
	F32 zoom_factor = gViewerCamera.getZoomFactor();
	S16 sub_region = gViewerCamera.getZoomSubRegion();
	if (zoom_factor > 1.f)
	{
		S32 num_horizontal_tiles = llceil(zoom_factor);
		S32 tile_width = ll_roundp((F32)gViewerWindowp->getWindowWidth() /
								   zoom_factor);
		S32 tile_height = ll_roundp((F32)gViewerWindowp->getWindowHeight() /
								    zoom_factor);
		S32 tile_y = sub_region / num_horizontal_tiles;
		S32 tile_x = sub_region - (tile_y * num_horizontal_tiles);

		whole_screen.setLeftTopAndSize(tile_x * tile_width,
									   gViewerWindowp->getWindowHeight() -
									   (tile_y * tile_height),
									   tile_width, tile_height);
	}

	return setup_hud_matrices(whole_screen);
}

bool setup_hud_matrices(const LLRect& screen_region)
{
	LLMatrix4a proj, model;
	if (!get_hud_matrices(screen_region, proj, model))
	{
		return false;
	}

	// Set up transform to keep HUD objects in front of camera
	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.loadMatrix(proj);
	gGLProjection = proj;

	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.loadMatrix(model);
	gGLModelView = model;

	return true;
}

bool get_hud_matrices(LLMatrix4a& proj, LLMatrix4a& model)
{
	LLRect whole_screen = gViewerWindowp->getVirtualWindowRect();
	return get_hud_matrices(whole_screen, proj, model);
}

bool get_hud_matrices(const LLRect& screen_region,
					  LLMatrix4a& proj, LLMatrix4a& model)
{
	if (!isAgentAvatarValid() || !gAgentAvatarp->hasHUDAttachment())
	{
		return false;
	}

	LLBBox hud_bbox = gAgentAvatarp->getHUDBBox();

	F32 aspect_ratio = gViewerCamera.getAspect();
	F32 hud_depth = llmax(1.f, hud_bbox.getExtentLocal().mV[VX] * 1.1f);
	proj = gl_ortho(-0.5f * aspect_ratio, 0.5f * aspect_ratio, -0.5f, 0.5f,
					0.f, hud_depth);
	proj.getRow<2>().copyComponent<2>(LLVector4a(-0.01f));

	F32 wwidth = (F32)gViewerWindowp->getWindowWidth();
	F32 wheight = (F32)gViewerWindowp->getWindowHeight();
	F32 scale_x = wwidth / (F32)screen_region.getWidth();
	F32 scale_y = wheight /(F32)screen_region.getHeight();
	F32 delta_x = screen_region.getCenterX() - screen_region.mLeft;
	F32 delta_y = screen_region.getCenterY() - screen_region.mBottom;
	proj.applyTranslationAffine(clamp_rescale(delta_x, 0.f, wwidth,
											  0.5f * scale_x * aspect_ratio,
											  -0.5f * scale_x * aspect_ratio),
								clamp_rescale(delta_y, 0.f, wheight,
											  0.5f * scale_y,
											  -0.5f * scale_y), 0.f);
	proj.applyScaleAffine(scale_x, scale_y, 1.f);

	model = OGL_TO_CFR_ROT4A;

	model.applyTranslationAffine(LLVector3(hud_depth * 0.5f -
										   hud_bbox.getCenterLocal().mV[VX],
										   0.f, 0.f));
	model.applyScaleAffine(gAgent.mHUDCurZoom);

	return true;
}

void render_ui(F32 zoom_factor)
{
	gGL.flush();
	{
		LL_FAST_TIMER(FTM_RENDER_UI);

		LL_GL_CHECK_STATES;

		const LLMatrix4a saved_view = gGLModelView;

		bool not_snaphot = !gSnapshot;
		if (not_snaphot)
		{
			gGL.pushMatrix();
			gGL.loadMatrix(gGLLastModelView);
			gGLModelView = gGLLastModelView;
		}

		// Finalize scene
		gPipeline.renderFinalize();

//MK
		{
			LL_TRACY_TIMER(TRC_RLV_RENDER_LIMITS);
			// Possibly draw a big black sphere around our avatar if the camera
			// render is limited
			if (gRLenabled && !gRLInterface.mRenderLimitRenderedThisFrame &&
				!(isAgentAvatarValid() && gAgentAvatarp->isFullyLoaded()))
			{
				gRLInterface.drawRenderLimit(true);
			}
		}
//mk

		render_hud_elements();
		render_hud_attachments();

		LLGLSDefault gls_default;
		LLGLSUIDefault gls_ui;

		gPipeline.disableLights();

		gGL.color4f(1.f, 1.f, 1.f, 1.f);
		if (gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_UI))
		{
			if (!gDisconnected)
			{
				render_ui_3d();
				LL_GL_CHECK_STATES;
			}
			else
			{
				render_disconnected_background();
			}

			if (gUsePBRShaders)
			{
				LLHUDObject::renderAll();
			}
			render_ui_2d();
			LL_GL_CHECK_STATES;
		}
		if (!gUsePBRShaders)
		{
			gGL.flush();
		}

		gViewerWindowp->setup2DRender();
		gViewerWindowp->updateDebugText();
		gViewerWindowp->drawDebugText();

		if (!gUsePBRShaders)
		{
			LLVertexBuffer::unbind();
		}

		if (not_snaphot)
		{
			gGLModelView = saved_view;
			gGL.popMatrix();
		}
		gGL.flush();
	}

	// Do not include this in FTM_RENDER_UI, since during the swap all sorts
	// non-UI stuff will be drawn... HB
	if (gDisplaySwapBuffers)
	{
		gWindowp->swapBuffers();
	}
	gDisplaySwapBuffers = true;
}

void renderCoordinateAxes()
{
	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
	gGL.begin(LLRender::LINES);
		gGL.color3f(1.f, 0.f, 0.f);   // i direction = X-Axis = red
		gGL.vertex3f(0.f, 0.f, 0.f);
		gGL.vertex3f(2.f, 0.f, 0.f);
		gGL.vertex3f(3.f, 0.f, 0.f);
		gGL.vertex3f(5.f, 0.f, 0.f);
		gGL.vertex3f(6.f, 0.f, 0.f);
		gGL.vertex3f(8.f, 0.f, 0.f);
		// Make an X
		gGL.vertex3f(11.f, 1.f, 1.f);
		gGL.vertex3f(11.f, -1.f, -1.f);
		gGL.vertex3f(11.f, 1.f, -1.f);
		gGL.vertex3f(11.f, -1.f, 1.f);

		gGL.color3f(0.f, 1.f, 0.f);   // j direction = Y-Axis = green
		gGL.vertex3f(0.f, 0.f, 0.f);
		gGL.vertex3f(0.f, 2.f, 0.f);
		gGL.vertex3f(0.f, 3.f, 0.f);
		gGL.vertex3f(0.f, 5.f, 0.f);
		gGL.vertex3f(0.f, 6.f, 0.f);
		gGL.vertex3f(0.f, 8.f, 0.f);
		// Make a Y
		gGL.vertex3f(1.f, 11.f, 1.f);
		gGL.vertex3f(0.f, 11.f, 0.f);
		gGL.vertex3f(-1.f, 11.f, 1.f);
		gGL.vertex3f(0.f, 11.f, 0.f);
		gGL.vertex3f(0.f, 11.f, 0.f);
		gGL.vertex3f(0.f, 11.f, -1.f);

		gGL.color3f(0.f, 0.f, 1.f);   // Z-Axis = blue
		gGL.vertex3f(0.f, 0.f, 0.f);
		gGL.vertex3f(0.f, 0.f, 2.f);
		gGL.vertex3f(0.f, 0.f, 3.f);
		gGL.vertex3f(0.f, 0.f, 5.f);
		gGL.vertex3f(0.f, 0.f, 6.f);
		gGL.vertex3f(0.f, 0.f, 8.f);
		// Make a Z
		gGL.vertex3f(-1.f, 1.f, 11.f);
		gGL.vertex3f(1.f, 1.f, 11.f);
		gGL.vertex3f(1.f, 1.f, 11.f);
		gGL.vertex3f(-1.f, -1.f, 11.f);
		gGL.vertex3f(-1.f, -1.f, 11.f);
		gGL.vertex3f(1.f, -1.f, 11.f);
	gGL.end();
}

void draw_axes()
{
	LLGLSUIDefault gls_ui;
	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
	// A vertical white line at origin
	LLVector3 v = gAgent.getPositionAgent();
	gGL.begin(LLRender::LINES);
		gGL.color3f(1.f, 1.f, 1.f);
		gGL.vertex3f(0.f, 0.f, 0.f);
		gGL.vertex3f(0.f, 0.f, 40.f);
	gGL.end();
	// Some coordinate axes
	gGL.pushMatrix();
		gGL.translatef(v.mV[VX], v.mV[VY], v.mV[VZ]);
		renderCoordinateAxes();
	gGL.popMatrix();
}

void render_ui_3d()
{
	LLGLSPipeline gls_pipeline;

	//////////////////////////////////////
	// Render 3D UI elements
	// NOTE: zbuffer is cleared before we get here by LLDrawPoolHUD,
	//		 so 3d elements requiring Z buffer are moved to LLDrawPoolHUD

	/////////////////////////////////////////////////////////////
	// Render 2.5D elements (2D elements in the world)
	// Stuff without z writes

	// Debugging stuff goes before the UI.

	gUIProgram.bind();
	if (gUsePBRShaders)
	{
		gGL.color4f(1.f, 1.f, 1.f, 1.f);
	}

	// Coordinate axes
	static LLCachedControl<bool> show_axes(gSavedSettings, "ShowAxes");
	if (show_axes)
	{
		draw_axes();
	}

	// Non HUD call in render_hud_elements
	gViewerWindowp->renderSelections(false, false, true);

	if (gUsePBRShaders &&
		gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_UI))
	{
		gObjectList.renderObjectBeacons();
		gObjectList.resetObjectBeacons();
		gSky.addSunMoonBeacons();
	}

	stop_glerror();
}

// Renders 2D UI elements that overlay the world (no z compare)
void render_ui_2d()
{
	LLGLSUIDefault gls_ui;

	//  Disable wireframe mode below here, as this is HUD/menus
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	// Menu overlays, HUD, etc
	gViewerWindowp->setup2DRender();

	F32 zoom_factor = gViewerCamera.getZoomFactor();
	S16 sub_region = gViewerCamera.getZoomSubRegion();

	if (zoom_factor > 1.f)
	{
		// Decompose subregion number to x and y values
		S32 pos_y = sub_region / llceil(zoom_factor);
		S32 pos_x = sub_region - pos_y * llceil(zoom_factor);
		// offset for this tile
		LLFontGL::sCurOrigin.mX -=
			ll_round((F32)gViewerWindowp->getWindowWidth() * (F32)pos_x /
					 zoom_factor);
		LLFontGL::sCurOrigin.mY -=
			ll_round((F32)gViewerWindowp->getWindowHeight() * (F32)pos_y /
					 zoom_factor);
	}

	// Render outline for HUD
	if (isAgentAvatarValid() && gAgent.mHUDCurZoom < 0.98f)
	{
		if (gUsePBRShaders)
		{
			gUIProgram.bind();
		}
		gGL.pushMatrix();
		S32 half_width = gViewerWindowp->getWindowWidth() / 2;
		S32 half_height = gViewerWindowp->getWindowHeight() / 2;
		gGL.scalef(LLUI::sGLScaleFactor.mV[0], LLUI::sGLScaleFactor.mV[1],
				   1.f);
		gGL.translatef((F32)half_width, (F32)half_height, 0.f);
		F32 zoom = gAgent.mHUDCurZoom;
		gGL.scalef(zoom, zoom, 1.f);
		gGL.color4fv(LLColor4::white.mV);
		gl_rect_2d(-half_width, half_height, half_width, -half_height, false);
		gGL.popMatrix();
		if (gUsePBRShaders)
		{
			gUIProgram.unbind();
		}
	}
	gViewerWindowp->draw();

	// Reset current origin for font rendering, in case of tiling render
	LLFontGL::sCurOrigin.set(0, 0);

	stop_glerror();
}

void render_disconnected_background()
{
	gUIProgram.bind();

	LLTexUnit* unit0 = gGL.getTexUnit(0);

	gGL.color4f(1.f, 1.f, 1.f, 1.f);
	if (!gDisconnectedImagep && gDisconnected)
	{
		std::string temp = gDirUtilp->getLindenUserDir() + LL_DIR_DELIM_STR;
		if (gIsInProductionGrid)
		{
			 temp += SCREEN_LAST_FILENAME;
		}
		else
		{
			 temp += SCREEN_LAST_BETA_FILENAME;
		}
		LLPointer<LLImageBMP> image_bmp = new LLImageBMP;
		if (!image_bmp->load(temp))
		{
			return;
		}
		llinfos << "Loaded last bitmap: " << temp << llendl;

		LLPointer<LLImageRaw> raw = new LLImageRaw;
		if (!image_bmp->decode(raw))
		{
			llwarns << "Bitmap decode failed" << llendl;
			gDisconnectedImagep = NULL;
			return;
		}

		U8* rawp = raw->getData();
		S32 npixels = (S32)image_bmp->getWidth() * (S32)image_bmp->getHeight();
		for (S32 i = 0; i < npixels; ++i)
		{
			S32 sum = 0;
			sum = *rawp + *(rawp + 1) + *(rawp + 2);
			sum /= 3;
			*rawp = ((S32)sum * 6 + *rawp) / 7;
			rawp++;
			*rawp = ((S32)sum * 6 + *rawp) / 7;
			rawp++;
			*rawp = ((S32)sum * 6 + *rawp) / 7;
			rawp++;
		}

		raw->expandToPowerOfTwo();
		gDisconnectedImagep = LLViewerTextureManager::getLocalTexture(raw.get(),
																	  false);
		gStartTexture = gDisconnectedImagep;
		unit0->unbind(LLTexUnit::TT_TEXTURE);
	}

	// Make sure the progress view always fills the entire window.
	S32 width = gViewerWindowp->getWindowWidth();
	S32 height = gViewerWindowp->getWindowHeight();

	if (gDisconnectedImagep)
	{
		LLGLSUIDefault gls_ui;
		gViewerWindowp->setup2DRender();
		gGL.pushMatrix();
		{
			// scale ui to reflect UIScaleFactor
			// this can't be done in setup2DRender because it requires a
			// pushMatrix/popMatrix pair
			const LLVector2& display_scale = gViewerWindowp->getDisplayScale();
			gGL.scalef(display_scale.mV[VX], display_scale.mV[VY], 1.f);

			unit0->bind(gDisconnectedImagep);
			gGL.color4f(1.f, 1.f, 1.f, 1.f);
			gl_rect_2d_simple_tex(width, height);
			unit0->unbind(LLTexUnit::TT_TEXTURE);
		}
		gGL.popMatrix();
	}
	gGL.flush();

	gUIProgram.unbind();
}

void display_cleanup()
{
	gDisconnectedImagep = NULL;
}

void hud_render_text(const LLWString& wstr, const LLVector3& pos_agent,
					 const LLFontGL& font, U8 style, F32 x_offset,
					 F32 y_offset, const LLColor4& color, bool orthographic)
{
	// Do cheap plane culling
	LLVector3 dir_vec = pos_agent - gViewerCamera.getOrigin();
	dir_vec /= dir_vec.length();

	if (wstr.empty() ||
		(!orthographic && dir_vec * gViewerCamera.getAtAxis() <= 0.f))
	{
		return;
	}

	LLVector3 right_axis;
	LLVector3 up_axis;
	if (orthographic)
	{
		F32 height_inv = 1.f / (F32)gViewerWindowp->getWindowHeight();
		right_axis.set(0.f, -height_inv, 0.f);
		up_axis.set(0.f, 0.f, height_inv);
	}
	else
	{
		gViewerCamera.getPixelVectors(pos_agent, up_axis, right_axis);
	}
	LLQuaternion rot;
	if (!orthographic)
	{
		rot = gViewerCamera.getQuaternion();
		rot = rot * LLQuaternion(-F_PI_BY_TWO, gViewerCamera.getYAxis());
		rot = rot * LLQuaternion(F_PI_BY_TWO, gViewerCamera.getXAxis());
	}
	else
	{
		rot = LLQuaternion(-F_PI_BY_TWO, LLVector3::z_axis);
		rot = rot * LLQuaternion(-F_PI_BY_TWO, LLVector3::y_axis);
	}
	F32 angle;
	LLVector3 axis;
	rot.getAngleAxis(&angle, axis);

	LLVector3 render_pos = pos_agent + floorf(x_offset) * right_axis +
						   floorf(y_offset) * up_axis;

	// Get the render_pos in screen space
	LLVector3 win_coord;
	LLRect viewport(gGLViewport[0], gGLViewport[1] + gGLViewport[3],
					gGLViewport[0] + gGLViewport[2], gGLViewport[1]);
	gGL.projectf(render_pos, gGLModelView, gGLProjection, viewport, win_coord);

	// Fonts all render orthographically, set up projection
	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.pushMatrix();
	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.pushMatrix();
	LLUI::pushMatrix();

	gl_state_for_2d(gViewerWindowp->getWindowDisplayWidth(),
					gViewerWindowp->getWindowDisplayHeight());
	gViewerWindowp->setupViewport();

	LLUI::loadIdentity();
	gGL.loadIdentity();
	LLUI::translate(win_coord.mV[VX] / LLFontGL::sScaleX,
					win_coord.mV[VY] / LLFontGL::sScaleY,
					-2.f * win_coord.mV[VZ] + 1.f);
	F32 right_x;
	font.render(wstr, 0, 0, 0, color, LLFontGL::LEFT, LLFontGL::BASELINE,
				style, wstr.length(), 1000, &right_x);

	LLUI::popMatrix();
	gGL.popMatrix();

	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.popMatrix();
	gGL.matrixMode(LLRender::MM_MODELVIEW);
}
