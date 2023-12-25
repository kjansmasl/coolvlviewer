/**
 * @file llviewerwindow.cpp
 * @brief Implementation of the LLViewerWindow class.
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

#include "llviewerprecompiledheaders.h"

// system library includes
#include <stdio.h>
#include <iostream>
#include <fstream>
#if LL_WINDOWS
#include <tchar.h>							// For Unicode conversion methods
#endif

#include "llviewerwindow.h"

#include "llapp.h"
#include "llaudioengine.h"					// gAudiop
#include "llconsole.h"
#include "llcubemaparray.h"
#include "lldir.h"
#include "llfontfreetype.h"
#include "llimagebmp.h"
#include "llimagedecodethread.h"
#include "llimagegl.h"
#include "llimagej2c.h"
#include "llmenugl.h"
#include "llmemory.h"
#include "llmodaldialog.h"
#include "llmousehandler.h"
#include "llrender.h"
#include "llrenderutils.h"					// For gBox, gSphere
#include "llrootview.h"
#include "lltextbox.h"
#include "lltrans.h"
#include "lluictrlfactory.h"
#include "llmessage.h"
#include "llraytrace.h"
#if LL_LINUX
# include "llwindowsdl.h"					// For gUseFullDesktop
#endif

#include "llagent.h"
#include "llappviewer.h"
#include "llchatbar.h"
#include "lldebugview.h"
#include "lldrawable.h"
#include "lldrawpoolalpha.h"
#include "lldrawpoolbump.h"
#include "lldrawpoolwater.h"
#include "llface.h"
#include "llfeaturemanager.h"
#include "llfloaterchat.h"
#include "llfloaterchatterbox.h"
#include "llfloatercustomize.h"
#include "llfloatereditui.h"				// HACK for UI editor
#include "llfloaternotificationsconsole.h"
#include "llfloatersnapshot.h"
#include "hbfloaterteleporthistory.h"
#include "llfloatertools.h"
#include "llfloaterworldmap.h"
#include "llgesturemgr.h"
#include "llhoverview.h"
#include "llhudtext.h"
#include "llhudview.h"
#include "llimmgr.h"
#include "llmaniptranslate.h"
#include "llmeshrepository.h"
#include "llmorphview.h"
#include "llnotify.h"
#include "lloverlaybar.h"
#include "llpanellogin.h"
#include "llpanelworldmap.h"
#include "llpipeline.h"
#include "llpreviewnotecard.h"				// refreshCachedSettings()
#include "llpreviewscript.h"				// refreshCachedSettings()
#include "llprogressview.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llselectmgr.h"
#include "llsky.h"							// gSky
#include "llstartup.h"
#include "llstatusbar.h"
#include "llsurface.h"
#include "lltexturecache.h"
#include "lltexturefetch.h"
#include "lltoolbar.h"
#include "lltoolcomp.h"
#include "lltooldraganddrop.h"
#if LL_DARWIN
# include "lltoolfocus.h"
#endif
#include "lltoolmgr.h"
#include "lltoolpie.h"
#include "llurldispatcher.h"				// SLURL from other app instance
#include "llvelocitybar.h"
#include "llvieweraudio.h"					// audio_update_volume()
#include "hbviewerautomation.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerdisplay.h"
#include "llviewergesture.h"				// gGestureList
#include "llviewerkeyboard.h"
#include "llviewerjoystick.h"
#include "llviewermediafocus.h"
#include "llviewermenu.h"
#include "llviewermessage.h"				// send_sound_trigger()
#include "llviewerobjectlist.h"
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"
#include "llviewershadermgr.h"
#include "llviewerstats.h"
#include "llviewertexturelist.h"
#include "llvisualparamhint.h"
#include "llvoavatarself.h"
#include "llvoiceclient.h"
#include "llvopartgroup.h"
#include "llvovolume.h"
#include "llwearablelist.h"
#include "llworld.h"

//
// Globals
//
LLBottomPanel*	gBottomPanelp = NULL;

LLViewerWindow*	gViewerWindowp = NULL;

LLFrameTimer	gMouseIdleTimer;
LLFrameTimer	gAwayTimer;
LLFrameTimer	gAwayTriggerTimer;
LLFrameTimer	gAlphaFadeTimer;

LLViewerObject*	gDebugRaycastObject = NULL;
LLVOPartGroup*	gDebugRaycastParticle = NULL;
LLVector4a		gDebugRaycastParticleIntersection;
LLVector4a		gDebugRaycastIntersection;
LLVector2		gDebugRaycastTexCoord;
LLVector4a		gDebugRaycastNormal;
LLVector4a		gDebugRaycastTangent;
S32				gDebugRaycastFaceHit;
LLVector4a		gDebugRaycastStart;
LLVector4a		gDebugRaycastEnd;

// HUD display lines in lower right
bool gDisplayWindInfo = false;
bool gDisplayCameraPos = false;
bool gDisplayFOV = false;

bool gSnapshotNoPost = false;

constexpr U8 NO_FACE = 255;
bool gQuietSnapshot = false;

// Minimum time after setting away state before coming back:
constexpr F32 MIN_AFK_TIME = 2.f;

constexpr F32 MIN_DISPLAY_SCALE = 0.75f;

// Static members
LLStat LLViewerWindow::sMouseVelocityStat;
std::string LLViewerWindow::sSnapshotBaseName;
std::string LLViewerWindow::sSnapshotDir;
std::string LLViewerWindow::sMovieBaseName;

////////////////////////////////////////////////////////////////////////////
// *HACK: easy way for console.cpp to retrieve the window size...

S32 viewer_window_width()
{
	return gViewerWindowp ? gViewerWindowp->getWindowWidth() : 100;
}

S32 viewer_window_height()
{
	return gViewerWindowp ? gViewerWindowp->getWindowHeight() : 100;
}

// *HACK: prevent double handling of accelerator keys...
static KEY sLastAcceleratorKey = 0;

////////////////////////////////////////////////////////////////////////////
// LLDebugText class
////////////////////////////////////////////////////////////////////////////

class LLDebugText
{
public:
	LLDebugText()
	:	mFont(LLFontGL::getFontMonospace()),
		// Draw the statistics in a light gray and in a thin font
		mTextColor(LLColor4(0.86f, 0.86f, 0.86f, 1.f)),
		mMinX(U32_MAX)
	{
		mLineHeight = mFont->getLineHeight();
		mIncY = 16 * mLineHeight / 10 + 1;
		mMaginX = 16 * mFont->getWidth(std::string("0")) / 10 + 1;
	}

	LL_INLINE void addText(U32 x, U32 y, const std::string& text)
	{
		mLineList.emplace_back(text, x, y);
		if (x < mMinX)
		{
			mMinX = x;
		}
	}

	void update()
	{
		if (!gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_UI))
		{
			// Do not display debug info when not rendering UI (important for
			// the "snapshot to disk" feature).
			return;
		}

		// Draw stuff growing up from right lower corner of screen
		U32 xpos = 0;
		U32 window_width = gViewerWindowp->getWindowWidth();
		static LLCachedControl<U32> hud_right_margin(gSavedSettings,
													 "HUDInfoRightMargin");
		U32 right_margin = llmax((U32)hud_right_margin, 256U);
		if (window_width > right_margin)
		{
			xpos = window_width - right_margin;
		}
		mMaxX = window_width - mMaginX;

		U32 ypos = 20;
		if (gToolBarp && gToolBarp->getVisible())
		{
			ypos += TOOL_BAR_HEIGHT;
		}
		if (gChatBarp && gChatBarp->getVisible())
		{
			ypos += CHAT_BAR_HEIGHT;
		}
		if (gOverlayBarp && gOverlayBarp->getVisible())
		{
			ypos += OVERLAY_BAR_HEIGHT;
		}
		mMinY = ypos - mLineHeight - 4;

		static LLCachedControl<bool> debug_show_size(gSavedSettings,
													 "DebugShowResizing");
		S32 size_x = 0;
		S32 size_y = 0;
		if (debug_show_size && LLFloater::resizing(size_x, size_y))
		{
			addText(window_width - 168, ypos,
					llformat("Floater size: %d x %d", size_x, size_y));
			ypos += mIncY;
		}

		static LLCachedControl<bool> debug_show_fps(gSavedSettings,
													"DebugShowFPS");
		if (debug_show_fps)
		{
			addText(window_width - 60, ypos,
					llformat("%d fps",
							 (S32)(gViewerStats.mFPSStat.getMeanPerSec() +
								   0.5f)));
			ypos += mIncY;
		}

		// Avoid text collision with the velocity bar...
		mVelocityBarShown = gVelocityBarp && gVelocityBarp->getVisible();
		if (mVelocityBarShown)
		{
			ypos = VELOCITY_TOP;
		}

		static LLCachedControl<bool> debug_show_time(gSavedSettings,
													 "DebugShowTime");
		if (debug_show_time)
		{
			F32 time = gTextureTimer.getElapsedTimeF32();
			S32 thours = (S32)(time / 3600);
			S32 tmins = (S32)((time - thours * 3600) / 60);
			S32 tsecs = (S32)(time - thours * 3600 - tmins * 60);
			time = gFrameTimeSeconds;
			S32 hours = (S32)(time / 3600);
			S32 mins = (S32)((time - hours * 3600) / 60);
			S32 secs = (S32)(time - hours * 3600 - mins * 60);
			addText(xpos, ypos,
					llformat("Online time: %d:%02d:%02d - Texture fecthing time: %d:%02d:%02d",
							 hours, mins, secs, thours, tmins, tsecs));
			ypos += mIncY;
		}

		static LLCachedControl<bool> debug_poll_age(gSavedSettings,
													"DebugShowPollRequestAge");
		if (debug_poll_age)
		{
			LLViewerRegion* regionp = gAgent.getRegion();
			if (regionp)
			{
				mTempStr = llformat("Poll request age: %.1fs",
									regionp->getEventPollRequestAge());
				if (!regionp->isEventPollInFlight())
				{
					mTempStr.append(" *");
				}
				addText(window_width - 172, ypos, mTempStr);
				ypos += mIncY;
			}
		}

		if (gDisplayCameraPos)
		{
			// Update camera center, camera view, wind info every other frame
			LLVector3d tvector = gAgent.getPositionGlobal();
			mTempStr = llformat("AgentCenter %f %f %f",
								(F32)tvector.mdV[VX], (F32)tvector.mdV[VY],
								(F32)tvector.mdV[VZ]);
			addText(xpos, ypos, mTempStr);
			ypos += mIncY;

			if (isAgentAvatarValid())
			{
				tvector =
					gAgent.getPosGlobalFromAgent(gAgentAvatarp->mRoot->getWorldPosition());
				mTempStr = llformat("AgentRootCenter %f %f %f",
									(F32)tvector.mdV[VX], (F32)tvector.mdV[VY],
									(F32)tvector.mdV[VZ]);
			}
			else
			{
				mTempStr = "---";
			}
			addText(xpos, ypos, mTempStr);
			ypos += mIncY;

			tvector = LLVector4(gAgent.getFrameAgent().getAtAxis());
			mTempStr = llformat("AgentAtAxis %f %f %f",
								(F32)tvector.mdV[VX], (F32)tvector.mdV[VY],
								(F32)tvector.mdV[VZ]);
			addText(xpos, ypos, mTempStr);
			ypos += mIncY;

			tvector = LLVector4(gAgent.getFrameAgent().getLeftAxis());
			mTempStr = llformat("AgentLeftAxis %f %f %f",
								(F32)tvector.mdV[VX], (F32)tvector.mdV[VY],
								(F32)tvector.mdV[VZ]);
			addText(xpos, ypos, mTempStr);
			ypos += mIncY;

			tvector = gAgent.getCameraPositionGlobal();
			mTempStr = llformat("CameraCenter %f %f %f",
								(F32)tvector.mdV[VX], (F32)tvector.mdV[VY],
								(F32)tvector.mdV[VZ]);
			addText(xpos, ypos, mTempStr);
			ypos += mIncY;

			tvector = LLVector4(gViewerCamera.getAtAxis());
			mTempStr = llformat("CameraAtAxis %f %f %f",
								(F32)tvector.mdV[VX], (F32)tvector.mdV[VY],
								(F32)tvector.mdV[VZ]);
			addText(xpos, ypos, mTempStr);
			ypos += mIncY;

			mTempStr = llformat("Near clip: %f - Far clip: %f",
								gViewerCamera.getNear(),
								gViewerCamera.getFar());
			addText(xpos, ypos, mTempStr);
			ypos += mIncY;

			mTempStr = "Camera mode: ";
			#define SETENUM(E) case E: mTempStr += #E; break
			switch (gAgent.getCameraMode())
			{
				SETENUM(CAMERA_MODE_THIRD_PERSON);
				SETENUM(CAMERA_MODE_MOUSELOOK);
				SETENUM(CAMERA_MODE_CUSTOMIZE_AVATAR);
				SETENUM(CAMERA_MODE_FOLLOW);
				default:
					mTempStr += llformat("%d", gAgent.getCameraMode());
			}
			#undef SETENUM
			addText(xpos, ypos, mTempStr);
			ypos += mIncY;
		}

		if (gDisplayWindInfo)
		{
			mTempStr = llformat("Wind velocity %.2f m/s", gWindVec.length());
			addText(xpos, ypos, mTempStr);
			ypos += mIncY;

			mTempStr = llformat("Wind vector %.2f %.2f %.2f",
								gWindVec.mV[0], gWindVec.mV[1],
								gWindVec.mV[2]);
			addText(xpos, ypos, mTempStr);
			ypos += mIncY;

			mTempStr = llformat("RWind vel %.2f m/s",
								gRelativeWindVec.length());
			addText(xpos, ypos, mTempStr);
			ypos += mIncY;

			mTempStr = llformat("RWind vec %.2f %.2f %.2f",
								gRelativeWindVec.mV[0], gRelativeWindVec.mV[1],
								gRelativeWindVec.mV[2]);
			addText(xpos, ypos, mTempStr);
			ypos += mIncY;

			if (gAudiop)
			{
				mTempStr = llformat("Audio for wind: %d",
									gAudiop->isWindEnabled());
			}
			addText(xpos, ypos, mTempStr);
			ypos += mIncY;
		}

		if (gDisplayFOV)
		{
			addText(xpos, ypos,
					llformat("FOV: %2.1f deg",
							 RAD_TO_DEG * gViewerCamera.getView()));
			ypos += mIncY;
		}

#if 0
		if (LLViewerJoystick::getInstance()->getOverrideCamera())
		{
			addText(xpos + 250, ypos, "Flycam");
			ypos += mIncY;
		}
#endif

		static LLCachedControl<bool> debug_show_render_info(gSavedSettings,
															"DebugShowRenderInfo");
		if (debug_show_render_info)
		{
			if (!gPipeline.canUseShaders())
			{
				addText(xpos, ypos, "Shaders disabled");
				ypos += mIncY;
			}

			if (gGLManager.mHasATIMemInfo)
			{
				S32 meminfo[4];
				glGetIntegerv(GL_TEXTURE_FREE_MEMORY_ATI, meminfo);

				addText(xpos, ypos, llformat("%.2f MB texture memory free",
											 meminfo[0] / 1024.f));
				ypos += mIncY;

				glGetIntegerv(GL_VBO_FREE_MEMORY_ATI, meminfo);
				addText(xpos, ypos, llformat("%.2f MB VBO memory free",
											 meminfo[0] / 1024.f));
				ypos += mIncY;
			}
			else if (gGLManager.mHasNVXMemInfo)
			{
				S32 free_memory;
				glGetIntegerv(GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX,
							  &free_memory);
				addText(xpos, ypos, llformat("%.2f MB video memory free",
											 free_memory / 1024.f));
				ypos += mIncY;
			}

			// Show streaming cost/triangle count of known prims in current
			// region OR selection
			{
				F32 cost = 0.f;
				S32 count = 0;
				S32 vcount = 0;
				S32 object_count = 0;
				S32 total_bytes = 0;
				S32 visible_bytes = 0;

				LLObjectSelectionHandle selected_objects =
					gSelectMgr.getSelection();
				if (selected_objects->getObjectCount() == 0)
				{
					LLViewerRegion* regionp = gAgent.getRegion();
					if (regionp)
					{
						for (U32 i = 0, count = gObjectList.getNumObjects();
							 i < count; ++i)
						{
							LLViewerObject* objectp = gObjectList.getObject(i);
							if (objectp && objectp->getRegion() == regionp &&
								objectp->getVolume())
							{
								++object_count;
								S32 bytes = 0;
								S32 visible = 0;
								cost += objectp->getStreamingCost(&bytes,
																  &visible);
								S32 vt = 0;
								count += objectp->getTriangleCount(&vt);
								vcount += vt;
								total_bytes += bytes;
								visible_bytes += visible;
							}
						}
					}
					addText(xpos, ypos,
							llformat("Region streaming cost: %.1f", cost));
				}
				else
				{
					cost =
						selected_objects->getSelectedObjectStreamingCost(&total_bytes,
																		 &visible_bytes);
					count =
						selected_objects->getSelectedObjectTriangleCount(&vcount);
					object_count = selected_objects->getObjectCount();

					addText(xpos, ypos,
							llformat("Selection streaming cost: %.1f", cost));
				}
				ypos += mIncY;

				addText(xpos, ypos,
						llformat("%.1f KTris, %.3f KVerts, %.1f/%.1f KB, %d objects",
								 count / 1000.f, vcount / 1000.f,
								 visible_bytes / 1024.f, total_bytes / 1024.f,
								 object_count));
				ypos += mIncY;
			}

			addText(xpos, ypos,
					llformat("%d vertex buffers",
							 LLVertexBuffer::getGLCount()));
			ypos += mIncY;

			addText(xpos, ypos,
					llformat("%d vertex buffer binds",
							 LLVertexBuffer::getBindCount()));
			ypos += mIncY;

			addText(xpos, ypos,
					llformat("%d vertex buffer sets",
							 LLVertexBuffer::getSetCount()));
			ypos += mIncY;

			addText(xpos, ypos,
					llformat("%d texture binds", LLImageGL::sBindCount));
			ypos += mIncY;

			addText(xpos, ypos,
					llformat("%d unique textures", LLImageGL::sUniqueCount));
			ypos += mIncY;

			addText(xpos, ypos,
					llformat("%d render calls", gPipeline.mBatchCount));
			ypos += mIncY;

			addText(xpos, ypos, llformat("Batch min/max/mean: %d/%d/%d",
										 gPipeline.mMinBatchSize,
										 gPipeline.mMaxBatchSize,
										 gPipeline.mTrianglesDrawn /
										 gPipeline.mBatchCount));
			ypos += mIncY;
			gPipeline.mMinBatchSize = gPipeline.mMaxBatchSize = 0;
			gPipeline.mBatchCount = 0;

			addText(xpos, ypos,
					llformat("%d/%d objects active",
							 gObjectList.getNumActiveObjects(),
							 gObjectList.getNumObjects()));
			ypos += mIncY;

			addText(xpos, ypos,
					llformat("%d matrix ops", gPipeline.mMatrixOpCount));
			ypos += mIncY;
			gPipeline.mMatrixOpCount = 0;

			addText(xpos, ypos,
					llformat("%d texture matrix ops",
							 gPipeline.mTextureMatrixOps));
			ypos += mIncY;
			gPipeline.mTextureMatrixOps = 0;

			addText(xpos, ypos,
					llformat("%d/%d nodes visible", gPipeline.mNumVisibleNodes,
							 LLSpatialGroup::sNodeCount));
			ypos += mIncY;

			addText(xpos, ypos,
					llformat("%d avatars visible",
							 LLVOAvatar::sNumVisibleAvatars));
			ypos += mIncY;

			addText(xpos, ypos,
					llformat("%d lights visible",
							 LLPipeline::sVisibleLightCount));
			ypos += mIncY;

			if (gMeshRepo.meshRezEnabled())
			{
				constexpr F32 MEGABYTE = 1048576.f;
				addText(xpos, ypos,
						llformat("%.3f MB mesh data received",
								 LLMeshRepository::sBytesReceived / MEGABYTE));
				ypos += mIncY;

				addText(xpos, ypos,
						llformat("%d/%d mesh HTTP requests/retries",
								 LLMeshRepository::sHTTPRequestCount,
								 LLMeshRepository::sHTTPRetryCount));
				ypos += mIncY;

				addText(xpos, ypos,
						llformat("%d/%d mesh LOD pending/processing",
								 (S32)LLMeshRepository::sLODPending,
								 (S32)LLMeshRepository::sLODProcessing));
				ypos += mIncY;

				addText(xpos, ypos,
						llformat("%.3f/%.3f MB mesh cache read/write ",
								 LLMeshRepository::sCacheBytesRead / MEGABYTE,
								 LLMeshRepository::sCacheBytesWritten / MEGABYTE));
				ypos += mIncY;
			}

			// Reset per-frame statistics.
			LLVertexBuffer::resetPerFrameStats();
			LLImageGL::sBindCount = LLImageGL::sUniqueCount =
									gPipeline.mNumVisibleNodes =
									LLPipeline::sVisibleLightCount = 0;
		}

		static LLCachedControl<bool> debug_show_avatars_info(gSavedSettings,
															 "DebugShowAvatarRenderInfo");
		if (debug_show_avatars_info)
		{
			std::map<std::string, LLVOAvatar*> sorted_avs;
			for (S32 i = 0, count = LLCharacter::sInstances.size(); i < count;
				 ++i)
			{
				LLVOAvatar* avatarp = (LLVOAvatar*)LLCharacter::sInstances[i];
				if (avatarp && !avatarp->isDead() && !avatarp->mIsDummy &&
					!avatarp->isOrphaned() && avatarp->isFullyLoaded(true))
				{
					sorted_avs[avatarp->getFullname(true)] = avatarp;
				}
			}
//MK
			bool hide_names = gRLenabled && gRLInterface.mContainsShownames;
			if (hide_names)
			{
				mTempStr = "(Hidden)";
			}
//mk
			for (std::map<std::string, LLVOAvatar*>::reverse_iterator
					rit = sorted_avs.rbegin(), rend = sorted_avs.rend();
				 rit != rend; ++rit)
			{
//MK
				if (!hide_names)
//mk
				{
					mTempStr = utf8str_truncate(rit->first, 16);
				}
				LLVOAvatar* avatarp = rit->second;
				addText(xpos, ypos,
						llformat("%s: complexity %d, %d m2, %.1f MB",
								 mTempStr.c_str(),
								 avatarp->getVisualComplexity(),
								 (S32)avatarp->getAttachmentSurfaceArea(),
								 (F32)avatarp->getAttachmentSurfaceBytes() /
								 1048576.f));
				ypos += mIncY;
			}
		}

		static LLCachedControl<bool> debug_show_render_matrices(gSavedSettings,
																"DebugShowRenderMatrices");
		if (debug_show_render_matrices)
		{
			F32* m = gGLProjection.getF32ptr();
			addText(xpos, ypos, llformat("%.4f	.%4f	%.4f	%.4f",
										 m[12], m[13], m[14], m[15]));
			ypos += mIncY;

			addText(xpos, ypos, llformat("%.4f	.%4f	%.4f	%.4f",
										 m[8], m[9], m[10], m[11]));
			ypos += mIncY;

			addText(xpos, ypos, llformat("%.4f	.%4f	%.4f	%.4f",
										 m[4], m[5], m[6], m[7]));
			ypos += mIncY;

			addText(xpos, ypos, llformat("%.4f	.%4f	%.4f	%.4f",
										 m[0], m[1], m[2], m[3]));
			ypos += mIncY;

			m = gGLModelView.getF32ptr();
			addText(xpos, ypos, "Projection matrix");
			ypos += mIncY;

			addText(xpos, ypos, llformat("%.4f	.%4f	%.4f	%.4f",
										 m[12], m[13], m[14], m[15]));
			ypos += mIncY;

			addText(xpos, ypos, llformat("%.4f	.%4f	%.4f	%.4f",
										 m[8], m[9], m[10], m[11]));
			ypos += mIncY;

			addText(xpos, ypos, llformat("%.4f	.%4f	%.4f	%.4f",
										 m[4], m[5], m[6], m[7]));
			ypos += mIncY;

			addText(xpos, ypos, llformat("%.4f	.%4f	%.4f	%.4f",
										 m[0], m[1], m[2], m[3]));
			ypos += mIncY;

			addText(xpos, ypos, "View Matrix");
			ypos += mIncY;
		}

		static LLCachedControl<bool> debug_show_color(gSavedSettings,
													  "DebugShowColor");
		if (debug_show_color)
		{
			static U8 color[4];
			LLCoordGL coord = gViewerWindowp->getCurrentMouse();
			const LLVector2& scaler = gViewerWindowp->getDisplayScale();
			S32 x = ll_round((F32)coord.mX * scaler.mV[VX]);
			S32 y = ll_round((F32)coord.mY * scaler.mV[VY]);
			glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, color);
			addText(xpos, ypos, llformat("Pixel <%1d, %1d> R:%d G:%d B:%d A:%d",
										 x, y, color[0], color[1], color[2],
										 color[3]));
			ypos += mIncY;
		}

		// Only display these messages if we are actually rendering beacons at
		// this moment
		static LLCachedControl<bool> beacons_always_on(gSavedSettings,
													   "BeaconAlwaysOn");
		if (LLPipeline::sRenderBeacons &&
//MK
			!(gRLenabled && gRLInterface.mContainsEdit) &&
//mk
			(LLPipeline::sRenderBeaconsFloaterOpen || beacons_always_on))
		{
			if (LLPipeline::sRenderScriptedBeacons)
			{
				addText(xpos, ypos, "Viewing scripted object beacons (red)");
				ypos += mIncY;
			}
			else if (LLPipeline::sRenderScriptedTouchBeacons)
			{
				addText(xpos, ypos,
						"Viewing scripted object with touch function beacons (red)");
				ypos += mIncY;
			}

			if (LLPipeline::sRenderPhysicalBeacons)
			{
				addText(xpos, ypos, "Viewing physical object beacons (green)");
				ypos += mIncY;
			}

			if (LLPipeline::sRenderPermanentBeacons)
			{
				addText(xpos, ypos,
						"Viewing navmesh affecting object beacons (cyan)");
				ypos += mIncY;
			}

			if (LLPipeline::sRenderCharacterBeacons)
			{
				addText(xpos, ypos,
						"Viewing pathfinding character object beacons (grey)");
				ypos += mIncY;
			}

			if (LLPipeline::sRenderSoundBeacons)
			{
				addText(xpos, ypos, "Viewing sound beacons (yellow)");
				ypos += mIncY;
			}

			if (LLPipeline::sRenderParticleBeacons)
			{
				addText(xpos, ypos, "Viewing particle beacons (light blue)");
				ypos += mIncY;
				if (LLPipeline::toggleRenderTypeControlNegated((void*)LLPipeline::RENDER_TYPE_PARTICLES))
				{
					addText(xpos, ypos, "  (note: particles hidden)");
					ypos += mIncY;
				}
			}

			if (LLPipeline::sRenderMOAPBeacons)
			{
				addText(xpos, ypos, "Viewing shared media beacons (white)");
				ypos += mIncY;
			}
		}
		static LLCachedControl<bool> sun_beacon(gSavedSettings, "sunbeacon");
		if (sun_beacon)
		{
			addText(xpos, ypos, "Viewing Sun direction beacon (orange)");
			ypos += mIncY;
		}
		static LLCachedControl<bool> moon_beacon(gSavedSettings, "moonbeacon");
		if (moon_beacon)
		{
			addText(xpos, ypos, "Viewing Moon direction beacon (purple)");
			ypos += mIncY;
		}

		static LLCachedControl<bool> debug_show_mesh_queue(gSavedSettings,
														   "DebugShowMeshQueue");
		if (debug_show_mesh_queue)
		{
			if (!gMeshRepo.mUploads.empty())
			{
				for (std::vector<LLMeshUploadThread*>::iterator
						iter = gMeshRepo.mUploads.begin(),
						end = gMeshRepo.mUploads.end();
					 iter != end; ++iter)
				{
					LLMeshUploadThread* thread = *iter;
					addText(xpos, ypos,
							llformat("Mesh uploads: %d",
									 thread->mPendingUploads));
					ypos += mIncY;
				}
			}
			S32 pending = 0;
			S32 delayed = 0;
			S32 header = 0;
			S32 lod = 0;
			S32 ahead = 0;
			S32 alod = 0;
			LLMeshRepoThread* mthread = gMeshRepo.mThread;
			if (mthread)
			{
				// Note: no need to lock the mesh repository mutexes here: we
				// do not care if the (fast changing) numbers are inaccurate
				// once in a blue moon... HB
				pending = gMeshRepo.mPendingRequests.size();
#if !LL_PENDING_MESH_REQUEST_SORTING
				delayed = gMeshRepo.mDelayedPendingRequests.size();
#endif
				header = mthread->mHeaderReqQ.size();
				lod = mthread->mLODReqQ.size();
				ahead = LLMeshRepoThread::sActiveHeaderRequests;
				alod = LLMeshRepoThread::sActiveLODRequests;
			}
			if (delayed)
			{
				addText(xpos, ypos,
						llformat("Mesh queue: %d pending + %d delayed (%d:%d header | %d:%d LOD)",
								 pending, delayed, ahead, header, alod, lod));
			}
			else if (pending || header || lod || ahead || alod)
			{
				addText(xpos, ypos,
						llformat("Mesh queue: %d pending (%d:%d header | %d:%d LOD)",
								 pending, ahead, header, alod, lod));
				ypos += mIncY;
			}
		}
		mMaxY = ypos + 4 - mIncY;
	}

	void draw()
	{
		if (mLineList.empty())
		{
			return;
		}

		// Note: do not show the background while the velocity bar is shown
		static LLCachedControl<bool> hud_info_bg(gSavedSettings,
												 "HUDInfoBackground");
		if (!mVelocityBarShown && hud_info_bg)
		{
			mMinX -= mMaginX;
			LLUIImage::sRoundedSquare->drawSolid(mMinX, mMinY,
												 mMaxX - mMinX, mMaxY - mMinY,
												 LLConsole::getBackground());
		}

		for (line_list_t::iterator iter = mLineList.begin(),
								   end = mLineList.end();
			 iter != end; ++iter)
		{
			const Line& line = *iter;
			mFont->renderUTF8(line.text, 0, (F32)line.x, (F32)line.y,
							  mTextColor, LLFontGL::LEFT, LLFontGL::TOP,
							  LLFontGL::NORMAL, S32_MAX, S32_MAX, NULL, false);
		}
		mLineList.clear();
		mMinX = S32_MAX;
	}

private:
	LLFontGL*	mFont;
	LLColor4	mTextColor;
	U32			mLineHeight;
	U32			mMaginX;
	U32			mIncY;
	U32			mMinX;
	U32			mMaxX;
	U32			mMinY;
	U32			mMaxY;
	std::string	mTempStr;

	struct Line
	{
		Line(const std::string& in_text, U32 in_x, U32 in_y)
		:	text(in_text),
			x(in_x),
			y(in_y)
		{
		}

		U32			x;
		U32			y;
		std::string	text;
	};
	typedef std::vector<Line> line_list_t;
	line_list_t	mLineList;

	bool		mVelocityBarShown;
};

////////////////////////////////////////////////////////////////////////////
// LLViewerWindow class
////////////////////////////////////////////////////////////////////////////

LLViewerWindow::LLViewerWindow(const std::string& title, S32 x, S32 y,
							   U32 width, U32 height, bool fullscreen)
:	mActive(true),
	mWindowRect(0, height, width, 0),
	mVirtualWindowRect(0, height, width, 0),
	mLeftMouseDown(false),
	mMiddleMouseDown(false),
	mRightMouseDown(false),
#if LL_DARWIN
	mAllowMouseDragging(true),
#endif
	mDebugText(NULL),
	mToolTip(NULL),
	mToolTipBlocked(false),
	mMouseInWindow(false),
	mLastMask(MASK_NONE),
	mToolStored(NULL),
	mSuppressToolbox(false),
	mCursorHidden(false),
	mIgnoreActivate(false),
	mHoverPick(),
	mResDirty(false),
	mStatesDirty(false),
	mCurrResolutionIndex(0)
{
	gNotifications.initClass();
	LLNotificationChannel::buildChannel("VW_alerts", "Visible",
										LLNotificationFilters::filterBy<std::string>(&LLNotification::getType,
																					 "alert"));
	LLNotificationChannel::buildChannel("VW_alertmodal", "Visible",
										LLNotificationFilters::filterBy<std::string>(&LLNotification::getType,
																					 "alertmodal"));

	gNotifications.getChannel("VW_alerts")->connectChanged(&LLViewerWindow::onAlert);
	gNotifications.getChannel("VW_alertmodal")->connectChanged(&LLViewerWindow::onAlert);

	// Default to application directory.
	LLViewerWindow::sSnapshotBaseName = "Snapshot";
	LLViewerWindow::sMovieBaseName = "SLmovie";
	resetSnapshotLoc();

	// Create window
	LLWindow::createWindow(title, x, y, width, height, 0, fullscreen,
						   gSavedSettings.getBool("DisableVerticalSync"),
						   gSavedSettings.getU32("RenderFSAASamples"));

	if (!gAppViewerp->restoreErrorTrap())
	{
		llwarns << " Someone took over my signal/exception handler !"
				<< llendl;
	}

	if (!gWindowp)
	{
		llwarns << "Unable to create window, be sure screen is set at 32 bits color."
				<< llendl;
		gAppViewerp->forceExit();
	}
#if LL_DEBUG || LL_NO_FORCE_INLINE
	gWindowp->setWindowTitle(title + " [DEVEL]");
#endif

	// Immediately create the shader manager.
	LLViewerShaderMgr::createInstance();

	// Get the real window rect the window was created with (since there are
	// various OS-dependent reasons why the size of a window or fullscreen
	// context may have been adjusted slightly...)
	F32 ui_scale_factor = gSavedSettings.getF32("UIScaleFactor") *
						  gWindowp->getSystemUISize();
	// HiDPI scaling can be 4x. UI scaling in prefs is up to 2x, so max is 8x
	ui_scale_factor = llclamp(ui_scale_factor, 0.75f, 8.f);

	mDisplayScale.set(llmax(1.f / gWindowp->getPixelAspectRatio(), 1.f),
					  llmax(gWindowp->getPixelAspectRatio(), 1.f));
	mDisplayScale *= ui_scale_factor;
	F32 divisor_x = 1.f / mDisplayScale.mV[VX];
	F32 divisor_y = 1.f / mDisplayScale.mV[VY];
	mDisplayScaleDivisor.set(divisor_x, divisor_y);
	LLUI::sGLScaleFactor = mDisplayScale;

	LLCoordWindow size;
	gWindowp->getSize(&size);
	mWindowRect.set(0, size.mY, size.mX, 0);
	mVirtualWindowRect.set(0, ll_roundp((F32)size.mY * divisor_y),
						   ll_roundp((F32)size.mX * divisor_x), 0);

	LLFontManager::initClass();

	// We want to set this stuff up BEFORE we initialize the pipeline, so we
	// can turn off stuff like AGP if we think that it will crash the viewer.
	LL_DEBUGS("Window") << "Loading feature tables." << LL_ENDL;
	gFeatureManager.init();

	// Initialize OpenGL Renderer
	LLVertexBuffer::initClass();
	llinfos << "LLVertexBuffer initialization done." << llendl;
	gGL.init();

	if (gFeatureManager.isSafe() ||
		gSavedSettings.getS32("LastFeatureVersion") != gFeatureManager.getVersion() ||
		gSavedSettings.getBool("ProbeHardwareOnStartup"))
	{
		gFeatureManager.applyRecommendedSettings();
		gSavedSettings.setBool("ProbeHardwareOnStartup", false);
	}

	// If we crashed while initializng GL stuff last time, disable certain features
	if (gSavedSettings.getBool("RenderInitError"))
	{
		mInitAlert = "DisplaySettingsNoShaders";
		gFeatureManager.setGraphicsLevel(0, false);
		gSavedSettings.setU32("RenderQualityPerformance", 0);
	}

	// Set callbacks
	gWindowp->setCallbacks(this);

	LLImageGL::initThread(gWindowp, gSavedSettings.getS32("GLWorkerThreads"));

	// Init the image list. Must happen after GL is initialized and before the
	// images that LLViewerWindow needs are requested.
	gTextureList.init();
	LLViewerTextureManager::init();

	// Init default fonts
	initFonts();

	// Create container for all sub-views
	mRootView = new LLRootView("root", mVirtualWindowRect, false);

	// Make avatar head look forward at start
	mCurrentMousePoint.mX = getWindowWidth() / 2;
	mCurrentMousePoint.mY = getWindowHeight() / 2;

	// Sync the keyboard setting with the saved setting
	gSavedSettings.getControl("NumpadControl")->firePropertyChanged();

	mDebugText = new LLDebugText();
}

void LLViewerWindow::initGLDefaults()
{
	gGL.setSceneBlendType(LLRender::BT_ALPHA);

	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	gGL.setAmbientLightColor(LLColor4::black);

	glCullFace(GL_BACK);

	// RN: need this for translation and stretch manip.
	gBox.prerender();
}

void LLViewerWindow::initBase()
{
	// Set the gamma
	F32 gamma = gSavedSettings.getF32("DisplayGamma");
	if (!gWindowp->setGamma(gamma))
	{
		llwarns << "Failed to set the display gamma to " << gamma
				<< ". Restoring the default gamma." << llendl;
		gWindowp->restoreGamma();
	}

	// Create global views

	// Create the floater view at the start so that other views can add
	// children to it (but wait to add it as a child of the root view so that
	// it will be in front of the other views).

	// Constrain floaters to inside the menu and status bar regions.
	S32 height = getWindowHeight();
	S32 width = getWindowWidth();
	LLRect full_window(0, height, width, 0);
	LLRect floater_view_rect = full_window;
	// Make space for the menu bar...
	floater_view_rect.mTop -= gMenuBarHeight;
	// ... and for the tool bar, the chat bar and the overlay bar...
	if (gSavedSettings.getBool("ShowToolBar"))
	{
		floater_view_rect.mBottom += TOOL_BAR_HEIGHT;
	}
	if (gSavedSettings.getBool("ChatVisible"))
	{
		floater_view_rect.mBottom += CHAT_BAR_HEIGHT;
	}
	floater_view_rect.mBottom += OVERLAY_BAR_HEIGHT;

	// Check for non-first startup
	S32 floater_view_bottom = gSavedSettings.getS32("FloaterViewBottom");
	if (floater_view_bottom >= 0)
	{
		floater_view_rect.mBottom = floater_view_bottom;
	}

	gFloaterViewp = new LLFloaterView("Floater View", floater_view_rect);
	gFloaterViewp->setVisible(true);

	gSnapshotFloaterViewp = new LLSnapshotFloaterView("Snapshot Floater View",
													  full_window);
	// Snapshot floater must start invisible otherwise it eats all the tooltips
	gSnapshotFloaterViewp->setVisible(false);

	// Console
	llassert(!gConsolep);
	gConsolep = new LLConsole("console", getChatConsoleRect(),
							  gSavedSettings.getS32("ChatFontSize"),
							  gSavedSettings.getU32("ChatConsoleMaxLines"),
							  gSavedSettings.getF32("ChatPersistTime"));
	gConsolep->setFollows(FOLLOWS_LEFT | FOLLOWS_RIGHT | FOLLOWS_BOTTOM);
	mRootView->addChild(gConsolep);

	// Debug view over the console
	gDebugViewp = new LLDebugView("gDebugViewp", full_window);
	gDebugViewp->setFollowsAll();
	gDebugViewp->setVisible(true);
	mRootView->addChild(gDebugViewp);

	// HUD elements just below floaters
	LLRect hud_rect = full_window;
	hud_rect.mTop -= 24;
	hud_rect.mBottom += gStatusBarHeight;
	gHUDViewp = new LLHUDView("hud_view", hud_rect);
	gHUDViewp->setFollowsAll();
	mRootView->addChild(gHUDViewp);

	// Add floater view at the end so it will be on top, and give it tab
	// priority over others
	mRootView->addChild(gFloaterViewp, -1);
	mRootView->addChild(gSnapshotFloaterViewp);

	// Notify above floaters !
	LLRect notify_rect = full_window;
#if 0
	notify_rect.mTop -= 24;
#endif
	notify_rect.mBottom += gStatusBarHeight;
	gNotifyBoxViewp = new LLNotifyBoxView("notify_container", notify_rect,
										  false, FOLLOWS_ALL);
	mRootView->addChild(gNotifyBoxViewp, -2);

	// Tooltips go above floaters
	mToolTip = new LLTextBox(std::string("tool tip"), LLRect(0, 1, 1, 0));
	mToolTip->setHPad(4);
	mToolTip->setVPad(2);
	mToolTip->setColor(gColors.getColor("ToolTipTextColor"));
	mToolTip->setBorderColor(gColors.getColor("ToolTipBorderColor"));
	mToolTip->setBorderVisible(false);
	mToolTip->setBackgroundColor(gColors.getColor("ToolTipBgColor"));
	mToolTip->setBackgroundVisible(true);
	mToolTip->setFontStyle(LLFontGL::NORMAL);
	mToolTip->setBorderDropshadowVisible(true);
	mToolTip->setVisible(false);

	// Add the progress bar view (startup view), which overrides everything
	mProgressView = new LLProgressView("ProgressView", full_window);
	mRootView->addChild(mProgressView);
	setShowProgress(false);
	setProgressCancelButtonVisible(false);
#if LL_DARWIN
	// *HACK: to get a redraw and take into account Retina mode (or not)...
	mResDirty = true;
#endif
}

void adjust_rect_top_left(const char* control, const LLRect& view)
{
	LLRect r = gSavedSettings.getRect(control);
	if (r.mLeft || r.mBottom)
	{
		return;
	}
	r.setLeftTopAndSize(0, view.getHeight(), r.getWidth(), r.getHeight());
	gSavedSettings.setRect(control, r);
}

void adjust_rect_top_center(const char* control, const LLRect& view)
{
	LLRect r = gSavedSettings.getRect(control);
	if (r.mLeft || r.mBottom)
	{
		return;
	}
	r.setLeftTopAndSize((view.getWidth() - r.getWidth()) / 2, view.getHeight(),
						r.getWidth(), r.getHeight());
	gSavedSettings.setRect(control, r);
}

void adjust_rect_top_right(const char* control, const LLRect& view,
						   S32 delta_y = 0)
{
	LLRect r = gSavedSettings.getRect(control);
	if (r.mLeft || r.mBottom)
	{
		return;
	}
	r.setLeftTopAndSize(view.getWidth() - r.getWidth(),
						view.getHeight() - delta_y,
						r.getWidth(), r.getHeight());
	gSavedSettings.setRect(control, r);
}

void adjust_rect_center(const char* control, const LLRect& view)
{
	LLRect r = gSavedSettings.getRect(control);
	if (r.mLeft || r.mBottom)
	{
		return;
	}
	r.setLeftTopAndSize((view.getWidth() - r.getWidth()) / 2,
						view.getHeight() -
						(view.getHeight() - r.getHeight()) / 2,
						r.getWidth(), r.getHeight());
	gSavedSettings.setRect(control, r);
}

void adjust_rect_left_center(const char* control, const LLRect& view)
{
	LLRect r = gSavedSettings.getRect(control);
	if (r.mLeft || r.mBottom)
	{
		return;
	}
	r.setLeftTopAndSize(0,
						view.getHeight() -
						(view.getHeight() - r.getHeight()) / 2,
						r.getWidth(), r.getHeight());
	gSavedSettings.setRect(control, r);
}

void adjust_rect_right_center(const char* control, const LLRect& view)
{
	LLRect r = gSavedSettings.getRect(control);
	if (r.mLeft || r.mBottom)
	{
		return;
	}
	r.setLeftTopAndSize(view.getWidth() - r.getWidth(),
						view.getHeight() -
						(view.getHeight() - r.getHeight()) / 2,
						r.getWidth(), r.getHeight());
	gSavedSettings.setRect(control, r);
}

void adjust_rect_bottom_left(const char* control, const LLRect& view)
{
	LLRect r = gSavedSettings.getRect(control);
	if (r.mLeft || r.mBottom)
	{
		return;
	}
	r.setOriginAndSize(0, view.mBottom, r.getWidth(), r.getHeight());
	gSavedSettings.setRect(control, r);
}

void adjust_rect_bottom_center(const char* control, const LLRect& view)
{
	LLRect r = gSavedSettings.getRect(control);
	if (r.mLeft || r.mBottom)
	{
		return;
	}
	r.setOriginAndSize((view.getWidth() - r.getWidth()) / 2, view.mBottom,
					   r.getWidth(), r.getHeight());
	gSavedSettings.setRect(control, r);
}

void adjust_rect_bottom_right(const char* control, const LLRect& view)
{
	LLRect r = gSavedSettings.getRect(control);
	if (r.mLeft || r.mBottom)
	{
		return;
	}
	r.setOriginAndSize(view.getWidth() - r.getWidth(), view.mBottom,
					   r.getWidth(), r.getHeight());
	gSavedSettings.setRect(control, r);
}

// Many rectangles cannot be placed until we know the screen size. These
// rectangles have their bottom-left corner as 0,0 in the default settings.
void LLViewerWindow::adjustRectanglesForFirstUse()
{
	if (!gFloaterViewp) return;

	const LLRect& view_rect = gFloaterViewp->getRect();

	// *NOTE: the width and height of non-resizable floaters must be identical
	// in settings.xml and their relevant floater.xml files, otherwise the
	// adjustment will not work properly.

	// The camera controls floater goes at the top right corner...
	adjust_rect_top_right("FloaterCameraRect3a", view_rect);

	// ... then, just under, the movements controls floater...
	LLRect r = gSavedSettings.getRect("FloaterCameraRect3a");
	S32 delta_y = r.getHeight();
	adjust_rect_top_right("FloaterMoveRect2", view_rect, delta_y);

	// ... then, yet under, the mini-map...
	r = gSavedSettings.getRect("FloaterMoveRect2");
	delta_y += r.getHeight();
	adjust_rect_top_right("FloaterMiniMapRect", view_rect, delta_y);

	// ... finally, under the mini-map, all three friends list, groups list
	// and radar floaters, at the same level...
	r = gSavedSettings.getRect("FloaterMiniMapRect");
	delta_y += r.getHeight();
	adjust_rect_top_right("FloaterFriendsRect", view_rect, delta_y);
	adjust_rect_top_right("FloaterGroupsRect", view_rect, delta_y);
	adjust_rect_top_right("FloaterRadarRect", view_rect, delta_y);

	// The inventory floater goes at the bottom right
	adjust_rect_bottom_right("FloaterInventoryRect", view_rect);

	// Chat history at the bottom left (replaces the console when opened)
	adjust_rect_bottom_left("FloaterChatRect", view_rect);

	// Communicate window at the top left (keeps the console visible while
	// IMing)
	adjust_rect_top_left("ChatterboxRect", view_rect);

	// Chat and IM text input editor
	adjust_rect_bottom_center("ChatInputEditorRect", view_rect);
	adjust_rect_top_center("IMInputEditorRect", view_rect);

	// Active speakers at the bottom right, above the voice controls
	adjust_rect_bottom_right("FloaterActiveSpeakersRect", view_rect);

	// Audio volume at the bottom right, above the master volume toggle
	adjust_rect_bottom_right("FloaterAudioVolumeRect", view_rect);

	// Same thing for the nearby media floater, above the media controls...
	adjust_rect_bottom_right("FloaterNearbyMediaRect", view_rect);

	adjust_rect_right_center("FloaterStatisticsRect", view_rect);

	adjust_rect_right_center("FloaterPostcardRect", view_rect);

	adjust_rect_bottom_right("FloaterLagMeter", view_rect);

	// Build floater, top left
	adjust_rect_top_left("ToolboxRect", view_rect);

	// Script queue floater, top left
	adjust_rect_top_left("CompileOutputRect", view_rect);

	adjust_rect_top_left("FloaterCustomizeAppearanceRect", view_rect);

	// Land/region/parcel related floaters go on top centre, below the status
	// bar that shows the region and parcel names

	adjust_rect_top_center("FloaterLandRect5", view_rect);

	adjust_rect_top_center("FloaterRegionInfoRect", view_rect);

	adjust_rect_top_left("FloaterLandHoldingsRect", view_rect);

	adjust_rect_top_center("FloaterRegionDebugConsoleRect", view_rect);

	adjust_rect_top_center("FloaterBumpRect", view_rect);

	adjust_rect_top_center("FloaterWindlightRect", view_rect);

	adjust_rect_top_center("FloaterObjectBackuptRect", view_rect);

	adjust_rect_top_center("FloaterTeleportHistoryRect", view_rect);

	adjust_rect_top_center("FloaterInspectAvatarRect", view_rect);

	adjust_rect_top_center("FloaterInspectRect", view_rect);

	adjust_rect_top_left("FloaterRLVRect", view_rect);

	adjust_rect_top_left("FloaterDebugSettingsRect", view_rect);

	adjust_rect_center("FloaterFindRect2", view_rect);

	adjust_rect_center("FloaterLocalEnvEditorRect", view_rect);

	adjust_rect_top_left("FloaterExperienceProfileRect", view_rect);

	adjust_rect_center("FloaterExperiencesRect", view_rect);

	adjust_rect_center("FloaterAreaSearchRect", view_rect);

	adjust_rect_center("FloaterWorldMapRect2", view_rect);

	adjust_rect_center("FloaterGroupTitlesRect", view_rect);

	adjust_rect_center("MediaFilterRect", view_rect);

	adjust_rect_center("FloaterSoundsListRect", view_rect);

	adjust_rect_center("DirSelectorRect", view_rect);

	adjust_rect_center("FileSelectorRect", view_rect);

	adjust_rect_center("FloaterMarketplaceAssociationRect", view_rect);

	adjust_rect_center("FloaterMarketplaceValidationRect", view_rect);

	adjust_rect_left_center("FloaterAvatarProfileRect", view_rect);

	adjust_rect_left_center("FloaterBeaconsRect", view_rect);

	adjust_rect_left_center("FloaterMuteRect3", view_rect);

	adjust_rect_left_center("FloaterGestureRect2", view_rect);

	adjust_rect_center("PathFindingCharactersRect", view_rect);

	adjust_rect_center("PathFindingLinksetsRect", view_rect);

	adjust_rect_center("FloaterLuaDialogRect", view_rect);
}

void LLViewerWindow::initWorldUI()
{
	pre_init_menus();

	S32 height = mRootView->getRect().getHeight();
	S32 width = mRootView->getRect().getWidth();
	LLRect full_window(0, height, width, 0);

	if (!gToolBarp)	// Do not re-enter if objects are alreay created
	{
		if (gAudiop)
		{
			// Do not play the floaters opening sound
			gAudiop->setMuted(true);
		}

		LLRect bar_rect(-1, gStatusBarHeight, width + 1, -1);
		new LLToolBar(bar_rect);

		LLRect chat_bar_rect(-1, CHAT_BAR_HEIGHT, width + 1, -1);
		chat_bar_rect.translate(0, gStatusBarHeight - 1);
		gChatBarp = new LLChatBar("chat", chat_bar_rect);

		bar_rect.translate(0, gStatusBarHeight - 1);
		bar_rect.translate(0, CHAT_BAR_HEIGHT - 1);
		new LLOverlayBar(bar_rect);

		// Panel containing chatbar, toolbar, and overlay, over floaters
		LLRect bottom_rect(-1, 2 * gStatusBarHeight + CHAT_BAR_HEIGHT,
						   width + 1, -1);
		gBottomPanelp = new LLBottomPanel(bottom_rect);

		// The order here is important
		gBottomPanelp->addChild(gChatBarp);
		gBottomPanelp->addChild(gToolBarp);
		gBottomPanelp->addChild(gOverlayBarp);
		mRootView->addChild(gBottomPanelp);

		mRootView->addChild(new HBLuaSideBar());
		mRootView->sendChildToBack(gLuaSideBarp);

		// View for hover information
		gHoverViewp = new LLHoverView(full_window);
		gHoverViewp->setVisible(true);
		mRootView->addChild(gHoverViewp);

		new LLIMMgr();

		LLRect morph_view_rect = full_window;
		morph_view_rect.stretch(-gStatusBarHeight);
		morph_view_rect.mTop = full_window.mTop - 32;
		gMorphViewp = new LLMorphView(morph_view_rect);
		mRootView->addChild(gMorphViewp);
		gMorphViewp->setVisible(false);

		LLPanelWorldMap::initClass();

		gFloaterWorldMapp = new LLFloaterWorldMap();

		// Open teleport history floater and hide it initially
		gFloaterTeleportHistoryp = new HBFloaterTeleportHistory();

		//
		// Tools for building
		//

		// Toolbox floater
		init_menus();

		gFloaterToolsp = new LLFloaterTools();

		// Status bar
		S32 menu_bar_height = gMenuBarViewp->getRect().getHeight();
		LLRect root_rect = mRootView->getRect();
		LLRect status_rect(0, root_rect.getHeight(), root_rect.getWidth(),
						   root_rect.getHeight() - menu_bar_height);
		gStatusBarp = new LLStatusBar(status_rect);
		gStatusBarp->setFollows(FOLLOWS_LEFT | FOLLOWS_RIGHT | FOLLOWS_TOP);
		gStatusBarp->reshape(root_rect.getWidth(),
							 gStatusBarp->getRect().getHeight(), true);
		gStatusBarp->translate(0,
							   root_rect.getHeight() -
							   gStatusBarp->getRect().getHeight());
		// Sync bg color with menu bar
		gStatusBarp->setBackgroundColor(gMenuBarViewp->getBackgroundColor());

		LLFloaterChatterBox::createInstance(LLSD());

		mRootView->addChild(gStatusBarp);

		// Menu holder must be a child of the root view as well
		mRootView->addChild(gMenuHolderp);
		// Menu holder appears on top to get first pass at all mouse events
		mRootView->sendChildToFront(gMenuHolderp);

		if (gAudiop)
		{
			gAudiop->setMuted(false);
		}
	}
}

// Destroy the UI
void LLViewerWindow::shutdownViews()
{
	gSavedSettings.setS32("FloaterViewBottom",
						  gFloaterViewp->getRect().mBottom);

	gFocusMgr.unlockFocus();
	gFocusMgr.setMouseCapture(NULL);
	gFocusMgr.setKeyboardFocus(NULL);
	gFocusMgr.setTopCtrl(NULL);
	if (gWindowp)
	{
		gWindowp->allowLanguageTextInput(NULL, false);
	}

	// Cleanup global views
	if (gMorphViewp)
	{
		gMorphViewp->setVisible(false);
	}

	// DEV-40930: clear sModalStack. Otherwise, any LLModalDialog left open
	// will crump with llerrs.
	LLModalDialog::shutdownModals();
	llinfos << "LLModalDialog shut down." << llendl;

	cleanup_menus();
	llinfos << "Menus destroyed" << llendl;

	delete gFloaterTeleportHistoryp;
	delete gFloaterWorldMapp;
	delete gFloaterToolsp;

	// Delete all child views.
	if (mRootView)
	{
		delete mRootView;
		mRootView = NULL;
		// Automatically deleted as children of mRootView:
		mProgressView = NULL;
		gFloaterViewp = NULL;
		gSnapshotFloaterViewp = NULL;
		gConsolep = NULL;
		gChatBarp = NULL;
		llinfos << "Root view and children destroyed." << llendl;
	}
	else
	{
		llwarns << "Root view was already destroyed." << llendl;
	}

	llinfos << "Destroying IM manager." << llendl;
	delete gIMMgrp;
}

// Shuts down GL cleanly. Order is very important here.
void LLViewerWindow::shutdownGL()
{
	stop_glerror();

	LLFontGL::destroyDefaultFonts();
	LLFontManager::cleanupClass();
	llinfos << "Fonts destroyed" << llendl;

	gSky.cleanup();
	stop_glerror();
	llinfos << "Sky cleaned up" << llendl;

	gPipeline.cleanup();
	stop_glerror();
	llinfos << "Pipeline cleaned up" << llendl;

	// MUST clean up pipeline before cleaning up wearables
	LLWearableList::getInstance()->cleanup();
	llinfos << "Wearables cleaned up" << llendl;

	gTextureList.shutdown();
	stop_glerror();
	llinfos << "Texture list shut down" << llendl;

	gBumpImageList.destroyGL();
	stop_glerror();
	llinfos << "Cleaned up bump map images" << llendl;

	LLViewerTextureManager::cleanup();
	llinfos << "Cleaned up textures and GL images" << llendl;

	gSelectMgr.cleanup();
	llinfos << "Cleaned up select manager" << llendl;

	llinfos << "Stopping GL during shutdown" << llendl;
	stopGL(false);

	gGL.shutdown();
	llinfos << "GL shutdown" << llendl;

	LLVertexBuffer::cleanupClass();
	llinfos << "LLVertexBuffer cleaned up." << llendl;

	stop_glerror();
}

// Note: shutdownViews() and shutdownGL() need to be called first
LLViewerWindow::~LLViewerWindow()
{
	llinfos << "Destroying Window" << llendl;
	destroyWindow();

	if (mDebugText)
	{
		delete mDebugText;
		mDebugText = NULL;
		llinfos << "Debug text deleted." << llendl;
	}

	if (mToolTip)
	{
		delete mToolTip;
		mToolTip = NULL;
		llinfos << "Tool tip deleted." << llendl;
	}

	LLViewerShaderMgr::releaseInstance();
	llinfos << "LLViewerShaderMgr instance released." << llendl;

	LLImageGL::stopThread();
}

void LLViewerWindow::setCursor(ECursorType c)
{
	gWindowp->setCursor(c);
}

void LLViewerWindow::showCursor()
{
	gWindowp->showCursor();
	mCursorHidden = false;
}

void LLViewerWindow::hideCursor()
{
	// Hide tooltips
	if (mToolTip)
	{
		mToolTip->setVisible(false);
	}

	// Also hide hover info
	if (gHoverViewp)
	{
		gHoverViewp->cancelHover();
	}

	// And hide the cursor
	gWindowp->hideCursor();
	mCursorHidden = true;
}

void LLViewerWindow::sendShapeToSim()
{
	LLMessageSystem* msg = gMessageSystemp;
	if (!msg) return;

	msg->newMessageFast(_PREHASH_AgentHeightWidth);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->addU32Fast(_PREHASH_CircuitCode, msg->mOurCircuitCode);
	msg->nextBlockFast(_PREHASH_HeightWidthBlock);
	msg->addU32Fast(_PREHASH_GenCounter, 0);
	U16 height16 = (U16)mWindowRect.getHeight();
	U16 width16 = (U16)mWindowRect.getWidth();
	msg->addU16Fast(_PREHASH_Height, height16);
	msg->addU16Fast(_PREHASH_Width, width16);
	gAgent.sendReliableMessage();
}

// Must be called after the window is created to set up agent camera variables
// and UI variables.
void LLViewerWindow::reshape(S32 width, S32 height)
{
	// Destroying the window at quit time generates spurious reshape messages.
	// We do not care about these, and we do not want to send messages because
	// the message system may have been destructed.
	if (!LLApp::isExiting())
	{
		if (gMenuHolderp)
		{
			gMenuHolderp->hideMenus();
		}

		glViewport(0, 0, width, height);

		if (height > 0)
		{
			gViewerCamera.setViewHeightInPixels(height);
			if (gWindowp->getFullscreen())
			{
				// Force to 4:3 aspect for odd resolutions
				gViewerCamera.setAspect(getDisplayAspectRatio());
			}
			else
			{
				gViewerCamera.setAspect(width / (F32) height);
			}
		}

		// Update our window rectangle
		mWindowRect.mRight = mWindowRect.mLeft + width;
		mWindowRect.mTop = mWindowRect.mBottom + height;
		calcDisplayScale();

		bool display_scale_changed = mDisplayScale != LLUI::sGLScaleFactor;
		LLUI::sGLScaleFactor = mDisplayScale;

		// Update our window rectangle
		F32 divisor_x = mDisplayScaleDivisor.mV[VX];
		F32 divisor_y = mDisplayScaleDivisor.mV[VY];
		mVirtualWindowRect.mRight = mVirtualWindowRect.mLeft +
									ll_roundp((F32)width * divisor_x);
		mVirtualWindowRect.mTop = mVirtualWindowRect.mBottom +
								  ll_roundp((F32)height * divisor_y);

		setupViewport();

		// Inform lower views of the change; round up when converting
		// coordinates to make sure there are no gaps at edge of window.
		LLView::sForceReshape = display_scale_changed;
		mRootView->reshape(llceil((F32)width * divisor_x),
						   llceil((F32)height * divisor_y));
		LLView::sForceReshape = false;

		// Clear font width caches
		if (display_scale_changed)
		{
			LLHUDText::reshape();
		}

		sendShapeToSim();

		// Store new settings for the mode we are in, regardless
		if (!gWindowp->getFullscreen())
		{
			// Only save size if not maximized
			bool maximized = gWindowp->getMaximized();
			gSavedSettings.setBool("WindowMaximized", maximized);

			LLCoordScreen window_size;
			if (!maximized && gWindowp->getSize(&window_size))
			{
				gSavedSettings.setS32("WindowWidth", window_size.mX);
				gSavedSettings.setS32("WindowHeight", window_size.mY);
			}
		}

		gViewerStats.setStat(LLViewerStats::ST_WINDOW_WIDTH, (F64)width);
		gViewerStats.setStat(LLViewerStats::ST_WINDOW_HEIGHT, (F64)height);
		gResizeScreenTexture = gScreenIsDirty = true;
	}
}

// Hide normal UI when a logon fails
void LLViewerWindow::setNormalControlsVisible(bool visible)
{
	if (gBottomPanelp)
	{
		gBottomPanelp->setVisible(visible);
		gBottomPanelp->setEnabled(visible);
	}

	if (gMenuBarViewp)
	{
		gMenuBarViewp->setVisible(visible);
		gMenuBarViewp->setEnabled(visible);

		// ...and set the menu color appropriately.
		setMenuBackgroundColor();
	}

	if (gStatusBarp)
	{
		gStatusBarp->setVisible(visible);
		gStatusBarp->setEnabled(visible);
	}
}

void LLViewerWindow::setMenuBackgroundColor()
{
	LLColor4 new_bg_color;
	if (gAgent.getGodLevel() > GOD_NOT)
	{
		if (gIsInProductionGrid)
		{
			new_bg_color = gColors.getColor("MenuBarGodBgColor");
		}
		else
		{
			new_bg_color = gColors.getColor("MenuNonProductionGodBgColor");
		}
	}
	else if (gIsInProductionGrid)
	{
		new_bg_color = gColors.getColor("MenuBarBgColor");
	}
	else
	{
		new_bg_color = gColors.getColor("MenuNonProductionBgColor");
	}

	if (gMenuBarViewp)
	{
		gMenuBarViewp->setBackgroundColor(new_bg_color);
	}

	if (gStatusBarp)
	{
		gStatusBarp->setBackgroundColor(new_bg_color);
	}
}

void LLViewerWindow::updateDebugText()
{
	if (mDebugText)
	{
		mDebugText->update();
	}
}

void LLViewerWindow::drawDebugText()
{
	gUIProgram.bind();

	gGL.color4f(1.f, 1.f, 1.f, 1.f);
	gGL.pushMatrix();
	gGL.pushUIMatrix();

	// Scale view by UI global scale factor and aspect ratio correction factor
	gGL.scaleUI(mDisplayScale.mV[VX], mDisplayScale.mV[VY], 1.f);
	if (mDebugText)
	{
		mDebugText->draw();
	}

	gGL.popUIMatrix();
	gGL.popMatrix();

	gGL.flush();

	gUIProgram.unbind();
}

void LLViewerWindow::draw()
{
	if (!mRootView) return;

	LLUI::setLineWidth(1.f);

	LLUI::setLineWidth(1.f);
	// Reset any left-over transforms
	gGL.matrixMode(LLRender::MM_MODELVIEW);

	gGL.loadIdentity();

	// *HACK: for timecode debugging
	static LLCachedControl<bool> display_timecode(gSavedSettings,
												  "DisplayTimecode");
	if (display_timecode)
	{
		// Draw timecode block
		std::string text;

		gGL.loadIdentity();

		microsecondsToTimecodeString(gFrameTime,text);
		static const LLFontGL* font = LLFontGL::getFontSansSerif();
		font->renderUTF8(text, 0,
						 ll_roundp(getWindowWidth() / 2 - 100.f),
						 ll_roundp(getWindowHeight() - 60.f),
						 LLColor4(1.f, 1.f, 1.f, 1.f),
						 LLFontGL::LEFT, LLFontGL::TOP);
	}

	// Draw all nested UI views.
	// No translation needed, this view is glued to 0,0

	gUIProgram.bind();
	gGL.color4f(1.f, 1.f, 1.f, 1.f);

	gGL.pushMatrix();
	LLUI::pushMatrix();
	{
		// Scale view by UI global scale factor and aspect ratio correction
		// factor
		gGL.scaleUI(mDisplayScale.mV[VX], mDisplayScale.mV[VY], 1.f);

		LLVector2 old_scale_factor = LLUI::sGLScaleFactor;
		// Apply camera zoom transform (for high res screenshots)
		F32 zoom_factor = gViewerCamera.getZoomFactor();
		S16 sub_region = gViewerCamera.getZoomSubRegion();
		if (zoom_factor > 1.f)
		{
			// Decompose subregion number to x and y values
			S32 pos_y = sub_region / llceil(zoom_factor);
			S32 pos_x = sub_region - pos_y * llceil(zoom_factor);
			// offset for this tile
			gGL.translatef((F32)getWindowWidth() * -(F32)pos_x,
						   (F32)getWindowHeight() * -(F32)pos_y, 0.f);
			gGL.scalef(zoom_factor, zoom_factor, 1.f);
			LLUI::sGLScaleFactor *= zoom_factor;
		}

		// Draw tool specific overlay on world
		gToolMgr.getCurrentTool()->draw();

		if (gAgent.cameraMouselook())
		{
			drawMouselookInstructions();
		}

		// Draw all nested UI views.
		// No translation needed, this view is glued to 0,0
		mRootView->draw();

		// Draw optional on-top-of-everyone view
		LLUICtrl* top_ctrl = gFocusMgr.getTopCtrl();
		if (top_ctrl && top_ctrl->getVisible())
		{
			S32 screen_x, screen_y;
			top_ctrl->localPointToScreen(0, 0, &screen_x, &screen_y);

			gGL.matrixMode(LLRender::MM_MODELVIEW);
			LLUI::pushMatrix();
			LLUI::translate((F32) screen_x, (F32) screen_y, 0.f);
			top_ctrl->draw();
			LLUI::popMatrix();
		}

		// Draw tooltips; adjust their rectangle so they do not go off the top
		// or bottom of the screen.
		if (mToolTip && mToolTip->getVisible() && !mToolTipBlocked)
		{
			gGL.matrixMode(LLRender::MM_MODELVIEW);
			LLUI::pushMatrix();
			{
				S32 tip_height = mToolTip->getRect().getHeight();

				S32 screen_x, screen_y;
				mToolTip->localPointToScreen(0, -24 - tip_height,
											 &screen_x, &screen_y);

				// If tooltip would draw off the bottom of the screen, show it
				// from the cursor tip position.
				if (screen_y < tip_height)
				{
					mToolTip->localPointToScreen(0, 0, &screen_x, &screen_y);
				}
				LLUI::translate((F32) screen_x, (F32) screen_y, 0);
				mToolTip->draw();
			}
			LLUI::popMatrix();
		}

		LLUI::sGLScaleFactor = old_scale_factor;
	}
	LLUI::popMatrix();
	gGL.popMatrix();

	gUIProgram.unbind();

	stop_glerror();
}

static bool focus_chatbar_if_needed()
{
	if (!gChatBarp || gFocusMgr.childHasKeyboardFocus(gChatBarp) ||
		gAgent.cameraMouselook() || !gSavedSettings.getBool("AutoFocusChat"))
	{
		return false;
	}

	if (gChatBarp->getVisible() || LLFloaterChat::isFocused())
	{
		if (LLView::sDebugKeys)
		{
			llinfos << "Printable character detected, focusing chat bar"
					<< llendl;
		}
		LLChatBar::startChat(NULL);
		return gFocusMgr.childHasKeyboardFocus(gChatBarp);
	}

	return false;
}

// Takes a single keydown event, usually when UI is visible
//virtual
bool LLViewerWindow::handleKey(KEY key, MASK mask)
{
	sLastAcceleratorKey = 0;

	if (LLView::sDebugKeys)
	{
		llinfos << "key = " << std::hex << (U32)key << std::dec << " - mask = "
				<< mask << llendl;
	}

	// Hide tooltips on keypress. Block until next time mouse is moved.
	mToolTipBlocked = true;

	// Also hide hover info on keypress
	if (gHoverViewp)
	{
		gHoverViewp->cancelHover();
		gHoverViewp->setTyping(true);
	}

	LLFocusableElement* keyboard_focus = gFocusMgr.getKeyboardFocus();
	if (keyboard_focus && !(mask & (MASK_CONTROL | MASK_ALT)) &&
		!gFocusMgr.getKeystrokesOnly())
	{
		if (keyboard_focus->wantsKeyUpKeyDown()) // Media element
		{
			if (LLView::sDebugKeys)
			{
				llinfos << "Key handling passed to the focused media element"
						<< llendl;
			}
			return keyboard_focus->handleKey(key, mask, false);
		}

		if (key < 0x80)
		{
			// We have keyboard focus, and it is not an accelerator neither a
			// special key, so likely (we hope) to generate a character. Let it
			// fall through to character handler first.
			if (LLView::sDebugKeys)
			{
				llinfos << "Key handling passed to the keyboard character handler"
						<< llendl;
			}
			return true;
		}
	}

	// *HACK: look for UI editing keys
	if (LLView::sEditingUI && LLFloaterEditUI::processKeystroke(key, mask))
	{
		if (LLView::sDebugKeys)
		{
			llinfos << "Key handled by the UI editor" << llendl;
		}
		return true;
	}

	// Handle shift-escape key (reset camera view)
	if (key == KEY_ESCAPE && mask == MASK_SHIFT)
	{
		if (LLView::sDebugKeys)
		{
			llinfos << "Key handling for SHIFT ESC: resetting view" << llendl;
		}
		handle_reset_view();
		return true;
	}

	// Let menus handle navigation keys
	if (gLoginMenuBarViewp && gLoginMenuBarViewp->handleKey(key, mask, true))
	{
		if (LLView::sDebugKeys)
		{
			llinfos << "Key handled by the login menu bar" << llendl;
		}
		sLastAcceleratorKey = key;
		return true;
	}
	if (gMenuBarViewp && gMenuBarViewp->handleKey(key, mask, true))
	{
		if (LLView::sDebugKeys)
		{
			llinfos << "Key handled by the menu bar" << llendl;
		}
		sLastAcceleratorKey = key;
		return true;
	}

	// Traverses up the hierarchy
	if (keyboard_focus)
	{
		// Arrow keys move avatar while chatting hack
		if (gChatBarp && gChatBarp->inputEditorHasFocus())
		{
			if (gChatBarp->hasTextEditor() ||
				gChatBarp->getCurrentChat().empty() ||
				gSavedSettings.getBool("ArrowKeysMoveAvatar"))
			{
				switch (key)
				{
					case KEY_LEFT:
					case KEY_RIGHT:
					case KEY_UP:
					case KEY_DOWN:
						// Let CTRL-key pass through for chat line history
						if (MASK_CONTROL == mask)
						{
							break;
						}
					case KEY_PAGE_UP:
					case KEY_PAGE_DOWN:
					case KEY_HOME:
						// When chatbar is empty or ArrowKeysMoveAvatar set,
						// pass arrow keys on to avatar...
						if (LLView::sDebugKeys)
						{
							llinfos << "Key handling aborted as per ArrowKeysMoveAvatar"
									<< llendl;
						}
						return false;

					default:
						break;
				}
			}
		}

		if (keyboard_focus->handleKey(key, mask, false))
		{
			if (LLView::sDebugKeys)
			{
				llinfos << "Key handled by the keyboard focus holder"
						<< llendl;
			}
			return true;
		}
	}

	if (gToolMgr.getCurrentTool()->handleKey(key, mask))
	{
		if (LLView::sDebugKeys)
		{
			llinfos << "Key handled by the tool manager" << llendl;
		}
		return true;
	}

	// Try for a new-format gesture
	if (gGestureManager.triggerGesture(key, mask))
	{
		if (LLView::sDebugKeys)
		{
			llinfos << "Key handled by the gesture manager (1)" << llendl;
		}
		return true;
	}

	// See if this is a gesture trigger. If so, eat the key and do not pass it
	// down to the menus.
	if (gGestureList.trigger(key, mask))
	{
		if (LLView::sDebugKeys)
		{
			llinfos << "Key handled by the gesture manager (2)" << llendl;
		}
		return true;
	}

	// Give floaters first chance to handle TAB key so that frontmost floater
	// gets focus. If nothing has focus, go to first or last UI element as
	// appropriate.
	if (key == KEY_TAB && ((mask & MASK_CONTROL) || !keyboard_focus))
	{
		if (LLView::sDebugKeys)
		{
			llinfos << "Key handling of the TAB key for focus cycling"
					<< llendl;
		}
		if (gMenuHolderp)
		{
			gMenuHolderp->hideMenus();
		}

		// If CTRL-tabbing (and not just TAB with no focus), go into window
		// cycle mode
		if (gFloaterViewp)
		{
			gFloaterViewp->setCycleMode((mask & MASK_CONTROL) != 0);
		}

		// Do CTRL-TAB and CTRL-SHIFT-TAB logic
		if (mRootView)
		{
			if (mask & MASK_SHIFT)
			{
				mRootView->focusPrevRoot();
			}
			else
			{
				mRootView->focusNextRoot();
			}
			return true;
		}
	}

	// Give menus a chance to handle accelerator keys
	if (gLoginMenuBarViewp &&
		gLoginMenuBarViewp->handleAcceleratorKey(key, mask))
	{
		if (LLView::sDebugKeys)
		{
			llinfos << "Key handled by the login menu accelerators" << llendl;
		}
		sLastAcceleratorKey = key;
		return true;
	}
	if (gMenuBarViewp && gMenuBarViewp->handleAcceleratorKey(key, mask))
	{
		if (LLView::sDebugKeys)
		{
			llinfos << "Key handled by the menu accelerators" << llendl;
		}
		sLastAcceleratorKey = key;
		return true;
	}

	// See if chat bar needs to be auto-focused.
	if (key > 31 && key < 127 && (mask == MASK_NONE || mask == MASK_SHIFT))
	{
		if (focus_chatbar_if_needed())
		{
			keyboard_focus = gFocusMgr.getKeyboardFocus();
			if (keyboard_focus->handleKey(key, mask, false))
			{
				if (LLView::sDebugKeys)
				{
					llinfos << "Key handled by the chat bar"<< llendl;
				}
				return true;
			}
		}
	}

	// Do not pass keys on to world when something in UI has focus
	return gFocusMgr.childHasKeyboardFocus(mRootView) ||
		   LLMenuGL::getKeyboardMode() ||
		   (gMenuBarViewp && gMenuBarViewp->getHighlightedItem() &&
			gMenuBarViewp->getHighlightedItem()->isActive());
}

//virtual
bool LLViewerWindow::handleKeyUp(KEY key, MASK mask)
{
	LLFocusableElement* keyboard_focus = gFocusMgr.getKeyboardFocus();
	if (keyboard_focus && !(mask & (MASK_CONTROL | MASK_ALT)) &&
		!gFocusMgr.getKeystrokesOnly())
	{
		if (keyboard_focus->wantsKeyUpKeyDown())
		{
			if (LLView::sDebugKeys)
			{
				llinfos << "Key Up handling passed to the media plugin"
						<< llendl;
			}
			return keyboard_focus->handleKeyUp(key, mask, false);
		}
		if (key < 0x80)
		{
			// We have keyboard focus, and it is not an accelerator neither a
			// special key, so likely (we hope) to generate a character. Let it
			// fall through to character handler first.
			if (LLView::sDebugKeys)
			{
				llinfos << "Key Up handling passed to the keyboard character handler"
						<< llendl;
			}
			return true;
		}
	}

	if (keyboard_focus)
	{
		if (keyboard_focus->handleKeyUp(key, mask, false))
		{
			if (LLView::sDebugKeys)
			{
				llinfos << "Key Up handled by the keyboard focus holder"
						<< llendl;
			}
			return true;
		}
	}

	// Do not pass keys on to world when something in UI has focus
	return gFocusMgr.childHasKeyboardFocus(mRootView) ||
		   LLMenuGL::getKeyboardMode() ||
		   (gMenuBarViewp && gMenuBarViewp->getHighlightedItem() &&
			gMenuBarViewp->getHighlightedItem()->isActive());
}

//virtual
bool LLViewerWindow::handleUnicodeChar(llwchar uni_char, MASK mask)
{
	if (!gKeyboardp) return true;

	if (LLView::sDebugKeys)
	{
		llinfos << "key = " << std::hex << (U32)uni_char << std::dec
				<< " - mask = " << mask << " - Last accelerator key = "
				<< std::hex << (U32)sLastAcceleratorKey << std::dec << llendl;
	}

	// Do not eat-up accelerator keys: give menus a chance to handle keys.
	if (mask & (MASK_CONTROL | MASK_ALT))
	{
		// *HACK: do not process twice the same key, when it was already
		// accounted for as an accelerator key in handleKey()... HB
		if (sLastAcceleratorKey)
		{
			if (LLView::sDebugKeys)
			{
				llinfos << "Key already handled by the menu accelerators in handleKey(), ignoring..."
						<< llendl;
			}
			sLastAcceleratorKey = 0;
			return true;
		}

		if (gLoginMenuBarViewp)
		{
			KEY key = uni_char & 0xFFFF;
			if (gLoginMenuBarViewp->handleAcceleratorKey(key, mask))
			{
				if (LLView::sDebugKeys)
				{
					llinfos << "Key handled by the login menu accelerators"
							<< llendl;
				}
				sLastAcceleratorKey = 0;
				return true;
			}
			if (gLoginMenuBarViewp->handleUnicodeChar(uni_char, true))
			{
				if (LLView::sDebugKeys)
				{
					llinfos << "Key handled as a login menu jump key"
							<< llendl;
				}
				sLastAcceleratorKey = 0;
				return true;
			}
		}

		if (gMenuBarViewp)
		{
			KEY key = uni_char & 0xFFFF;
			if (gMenuBarViewp->handleAcceleratorKey(key, mask))
			{
				if (LLView::sDebugKeys)
				{
					llinfos << "Key handled by the menu accelerators" << llendl;
				}
				sLastAcceleratorKey = 0;
				return true;
			}
			if (gMenuBarViewp->handleUnicodeChar(uni_char, true))
			{
				if (LLView::sDebugKeys)
				{
					llinfos << "Key handled as a menu jump key" << llendl;
				}
				sLastAcceleratorKey = 0;
				return true;
			}
		}
	}

	sLastAcceleratorKey = 0;

	// *HACK: We delay processing of return keys until they arrive as a Unicode
	// char, so that if you are typing chat text at low frame rate, we do not
	// send the chat until all keystrokes have been entered. JC
	// *HACK: Numeric keypad <enter> on Mac is Unicode 3
	// *HACK: Control-M on Windows is Unicode 13
	if ((uni_char == 13 && mask != MASK_CONTROL) ||
		(uni_char == 3 && mask == MASK_NONE))
	{
		return gViewerKeyboard.handleKey(KEY_RETURN, mask,
										 gKeyboardp->getKeyRepeated(KEY_RETURN));
	}

	// Traverse up the hierarchy
	LLFocusableElement* keyboard_focus = gFocusMgr.getKeyboardFocus();
	if (keyboard_focus)
	{
		if (LLView::sDebugKeys)
		{
			llinfos << "Traversing up the focused view hierarchy..." << llendl;
		}
		if (keyboard_focus->handleUnicodeChar(uni_char, false))
		{
			if (LLView::sDebugKeys)
			{
				llinfos << "Key got handled up in the hierarchy." << llendl;
			}
			return true;
		}
		else if (LLView::sDebugKeys)
		{
			llinfos << "Key was not handled up in the hierarchy." << llendl;
		}
	}

	// See if the chat bar needs to be auto-focused. HB
	bool is_media = keyboard_focus && keyboard_focus->wantsKeyUpKeyDown();
	if (!is_media && uni_char > 31 && uni_char < 256 && uni_char != 127 &&
		(mask == MASK_NONE || mask == MASK_SHIFT))
	{
		if (focus_chatbar_if_needed())
		{
			keyboard_focus = gFocusMgr.getKeyboardFocus();
			if (keyboard_focus->handleUnicodeChar(uni_char, false))
			{
				if (LLView::sDebugKeys)
				{
					llinfos << "Key handled by the chat bar" << llendl;
				}
				return true;
			}
		}
	}

	return false;
}

//virtual
void LLViewerWindow::handleScrollWheel(S32 clicks)
{
	LLView::sMouseHandlerMessage.clear();

	gMouseIdleTimer.reset();

	// Hide tooltips
	if (mToolTip)
	{
		mToolTip->setVisible(false);
	}

	LLMouseHandler* mouse_captor = gFocusMgr.getMouseCapture();
	if (mouse_captor)
	{
		S32 local_x;
		S32 local_y;
		mouse_captor->screenPointToLocal(mCurrentMousePoint.mX,
										 mCurrentMousePoint.mY,
										 &local_x, &local_y);
		mouse_captor->handleScrollWheel(local_x, local_y, clicks);
		if (LLView::sDebugMouseHandling)
		{
			llinfos << "Scroll wheel handled by captor "
					<< mouse_captor->getName() << llendl;
		}
		return;
	}

	LLUICtrl* top_ctrl = gFocusMgr.getTopCtrl();
	if (top_ctrl)
	{
		S32 local_x;
		S32 local_y;
		top_ctrl->screenPointToLocal(mCurrentMousePoint.mX,
									 mCurrentMousePoint.mY,
									 &local_x, &local_y);
		if (top_ctrl->handleScrollWheel(local_x, local_y, clicks))
		{
			return;
		}
	}

	if (mRootView->handleScrollWheel(mCurrentMousePoint.mX,
									 mCurrentMousePoint.mY, clicks))
	{
		if (LLView::sDebugMouseHandling)
		{
			llinfos << "Scroll wheel" << LLView::sMouseHandlerMessage
					<< llendl;
		}
		return;
	}

	if (LLView::sDebugMouseHandling)
	{
		llinfos << "Scroll wheel not handled by view" << llendl;
	}
	// Zoom the camera in and out behavior
	gAgent.handleScrollWheel(clicks);
}

void LLViewerWindow::moveCursorToCenter()
{
	S32 x = mVirtualWindowRect.getWidth() / 2;
	S32 y = mVirtualWindowRect.getHeight() / 2;

	// On a forced move, all deltas get zeroed out to prevent jumping
	mCurrentMousePoint.set(x, y);
	mLastMousePoint.set(x, y);
	mCurrentMouseDelta.set(0, 0);

	LLUI::setCursorPositionScreen(x, y);
}

bool LLViewerWindow::shouldShowToolTipFor(LLMouseHandler* mh)
{
	if (mToolTip && mh)
	{
		LLMouseHandler::EShowToolTip showlevel = mh->getShowToolTip();

		return (showlevel == LLMouseHandler::SHOW_ALWAYS ||
				(showlevel == LLMouseHandler::SHOW_IF_NOT_BLOCKED &&
				 !mToolTipBlocked));
	}
	return false;
}

//virtual
bool LLViewerWindow::handleAnyMouseClick(LLWindow* window, LLCoordGL pos,
										 MASK mask,
										 LLMouseHandler::EClickType clicktype,
										 bool down)
{
	std::string buttonname;
	std::string buttonstatestr = down ? "down" : "up";
	bool handled = false;
	S32 x = pos.mX;
	S32 y = pos.mY;
	x = ll_round((F32)x * mDisplayScaleDivisor.mV[VX]);
	y = ll_round((F32)y * mDisplayScaleDivisor.mV[VY]);

	switch (clicktype)
	{
		case LLMouseHandler::CLICK_LEFT:
		{
			mLeftMouseDown = down;
			buttonname = "Left";
			break;
		}

		case LLMouseHandler::CLICK_RIGHT:
		{
			mRightMouseDown = down;
			buttonname = "Right";
			break;
		}

		case LLMouseHandler::CLICK_MIDDLE:
		{
			mMiddleMouseDown = down;
			buttonname = "Middle";
			break;
		}

		case LLMouseHandler::CLICK_DOUBLELEFT:
		{
			mLeftMouseDown = down;
			buttonname = "Left Double Click";
		}
	}

	LLView::sMouseHandlerMessage.clear();

	if (gMenuBarViewp)
	{
		// Stop ALT-key access to menu
		gMenuBarViewp->resetMenuTrigger();
	}

	if (gDebugClicks)
	{
		llinfos << "ViewerWindow " << buttonname << " mouse " << buttonstatestr
				<< " at " << x << "," << y << llendl;
	}

	// Make sure we get a corresponding mouse-up event, even if the mouse
	// leaves the window
	if (down)
	{
		gWindowp->captureMouse();
	}
	else
	{
		gWindowp->releaseMouse();
	}

	// Indicate mouse was active
	gMouseIdleTimer.reset();

	// Hide tooltips on mousedown
	if (mToolTip && down)
	{
		mToolTipBlocked = true;
		mToolTip->setVisible(false);
	}

	// Also hide hover info on mousedown/mouseup
	if (gHoverViewp)
	{
		gHoverViewp->cancelHover();
	}

	// Do not let the user move the mouse out of the window until mouse up.
	if (gToolMgr.getCurrentTool()->clipMouseWhenDown())
	{
		gWindowp->setMouseClipping(down);
	}

	LLMouseHandler* mouse_captor = gFocusMgr.getMouseCapture();
	if (mouse_captor)
	{
		S32 local_x;
		S32 local_y;
		mouse_captor->screenPointToLocal(x, y, &local_x, &local_y);
		if (LLView::sDebugMouseHandling)
		{
			llinfos << buttonname << " Mouse " << buttonstatestr
					<< " handled by captor " << mouse_captor->getName()
					<< llendl;
		}
		return mouse_captor->handleAnyMouseClick(local_x, local_y, mask,
												 clicktype, down);
	}

	// Topmost view gets a chance before the hierarchy
	LLUICtrl* top_ctrl = gFocusMgr.getTopCtrl();
	if (top_ctrl)
	{
		S32 local_x, local_y;
		top_ctrl->screenPointToLocal(x, y, &local_x, &local_y);
		if (down)
		{
			if (top_ctrl->pointInView(local_x, local_y))
			{
				return top_ctrl->handleAnyMouseClick(local_x, local_y, mask,
													 clicktype, down);
			}
			else
			{
				gFocusMgr.setTopCtrl(NULL);
			}
		}
		else
		{
			handled = top_ctrl->pointInView(local_x, local_y) &&
					  top_ctrl->handleMouseUp(local_x, local_y, mask);
		}
	}

	// Give the UI views a chance to process the click
	if (mRootView->handleAnyMouseClick(x, y, mask, clicktype, down))
	{
		if (LLView::sDebugMouseHandling)
		{
			llinfos << buttonname << " Mouse " << buttonstatestr << " "
					<< LLView::sMouseHandlerMessage << llendl;
		}
		return true;
	}
	else if (LLView::sDebugMouseHandling)
	{
		llinfos << buttonname << " Mouse " << buttonstatestr
				<< " not handled by view" << llendl;
	}

	if (down)
	{
		if (gDisconnected)
		{
			return false;
		}

		if (gToolMgr.getCurrentTool()->handleAnyMouseClick(x, y, mask,
														   clicktype, down))
		{
			// This is necessary to force clicks in the world to cause edit
			// boxes that might have keyboard focus to relinquish it, and hence
			// cause a commit to update their value.  JC
			gFocusMgr.setKeyboardFocus(NULL);
			return true;
		}
	}
	else
	{
		gWindowp->releaseMouse();

		LLTool* tool = gToolMgr.getCurrentTool();
		if (!handled)
		{
			handled = mRootView->handleAnyMouseClick(x, y, mask, clicktype, down);
		}
		if (!handled && tool)
		{
			handled = tool->handleAnyMouseClick(x, y, mask, clicktype, down);
		}
	}

	return !down;
}

//virtual
bool LLViewerWindow::handleMouseDown(LLWindow* window, LLCoordGL pos,
									 MASK mask)
{
#if LL_DARWIN
	mAllowMouseDragging = false;
	if (!mMouseDownTimer.getStarted())
	{
		mMouseDownTimer.start();
	}
	else
	{
		mMouseDownTimer.reset();
	}
#endif
	return handleAnyMouseClick(window, pos, mask, LLMouseHandler::CLICK_LEFT,
							   true);	// down
}

//virtual
bool LLViewerWindow::handleDoubleClick(LLWindow* window, LLCoordGL pos,
									   MASK mask)
{
	// try handling as a double-click first, then a single-click if that wasn't
	// handled.
	return handleAnyMouseClick(window, pos, mask,
							   LLMouseHandler::CLICK_DOUBLELEFT, true) ||
		   handleMouseDown(window, pos, mask);
}

//virtual
bool LLViewerWindow::handleMouseUp(LLWindow* window, LLCoordGL pos, MASK mask)
{
#if LL_DARWIN
	if (mMouseDownTimer.getStarted())
	{
		mMouseDownTimer.stop();
	}
#endif
	return handleAnyMouseClick(window, pos, mask, LLMouseHandler::CLICK_LEFT,
							   false);	// up
}

//virtual
bool LLViewerWindow::handleRightMouseDown(LLWindow* window, LLCoordGL pos,
										  MASK mask)
{
	S32 x = pos.mX;
	S32 y = pos.mY;
	x = ll_round((F32)x * mDisplayScaleDivisor.mV[VX]);
	y = ll_round((F32)y * mDisplayScaleDivisor.mV[VY]);

	LLView::sMouseHandlerMessage.clear();

	if (handleAnyMouseClick(window, pos, mask, LLMouseHandler::CLICK_RIGHT,
							true))
	{
		return true;
	}

	// *HACK: this should be rolled into the composite tool logic, not
	// hardcoded at the top level.
	if (gAgent.getCameraMode() != CAMERA_MODE_CUSTOMIZE_AVATAR &&
		!gToolMgr.isCurrentTool(&gToolPie))
	{
		// If the current tool did not process the click, we should show the
		// pie menu. This can be done by passing the event to the pie menu
		// tool.
		gToolPie.handleRightMouseDown(x, y, mask);
	}

	return true;
}

//virtual
bool LLViewerWindow::handleRightMouseUp(LLWindow* window, LLCoordGL pos,
										MASK mask)
{
	return handleAnyMouseClick(window,pos,mask,LLMouseHandler::CLICK_RIGHT,
							   false);	// Up
}

//virtual
bool LLViewerWindow::handleMiddleMouseDown(LLWindow* window, LLCoordGL pos,
										   MASK mask)
{
	if (LLVoiceClient::sInitDone)
	{
		gVoiceClient.middleMouseState(true);
	}

	handleAnyMouseClick(window, pos, mask, LLMouseHandler::CLICK_MIDDLE, true);

	// Always handled as far as the OS is concerned.
	return true;
}

//virtual
bool LLViewerWindow::handleMiddleMouseUp(LLWindow* window, LLCoordGL pos,
										 MASK mask)
{
	if (LLVoiceClient::sInitDone)
	{
		gVoiceClient.middleMouseState(false);
	}

	handleAnyMouseClick(window, pos, mask, LLMouseHandler::CLICK_MIDDLE,
						false);

	// Always handled as far as the OS is concerned.
	return true;
}

// WARNING: this is potentially called multiple times per frame
//virtual
void LLViewerWindow::handleMouseMove(LLWindow* window, LLCoordGL pos,
									 MASK mask)
{
	S32 x = pos.mX;
	S32 y = pos.mY;

	x = ll_round((F32)x * mDisplayScaleDivisor.mV[VX]);
	y = ll_round((F32)y * mDisplayScaleDivisor.mV[VY]);

	mMouseInWindow = true;

	// Save mouse point for access during idle() and display()
	LLCoordGL prev_saved_mouse_point = mCurrentMousePoint;
	LLCoordGL mouse_point(x, y);
	saveLastMouse(mouse_point);

	bool actually_moved = // mouse is not currenty captured:
						  !gFocusMgr.getMouseCapture() &&
						  // mouse moved from last recorded position:
						  (prev_saved_mouse_point.mX != mCurrentMousePoint.mX ||
						   prev_saved_mouse_point.mY != mCurrentMousePoint.mY);

	gMouseIdleTimer.reset();

	gWindowp->showCursorFromMouseMove();

	if (gAwayTimer.getElapsedTimeF32() > MIN_AFK_TIME)
	{
		gAgent.clearAFK();
	}

	if (actually_moved)
	{
		mToolTipBlocked = false;
	}

	// Activate the hover picker on mouse move.
	if (gHoverViewp)
	{
		gHoverViewp->setTyping(false);
	}
}

#if LL_DARWIN
//virtual
void LLViewerWindow::handleMouseDragged(LLWindow* window, LLCoordGL pos,
										MASK mask)
{
	if (mMouseDownTimer.getStarted())
	{
		if (mMouseDownTimer.getElapsedTimeF32() > 0.1)
		{
			mAllowMouseDragging = true;
			mMouseDownTimer.stop();
		}
	}
	if (mAllowMouseDragging || !gToolFocus.hasMouseCapture())
	{
		handleMouseMove(window, pos, mask);
	}
}
#endif

//virtual
void LLViewerWindow::handleMouseLeave(LLWindow* window)
{
	// Note: we would not get this if we had captured the mouse.
	llassert(gFocusMgr.getMouseCapture() == NULL);
	mMouseInWindow = false;
	if (mToolTip)
	{
		mToolTip->setVisible(false);
	}
}

//virtual
bool LLViewerWindow::handleCloseRequest(LLWindow* window)
{
	// User has indicated they want to close, but we may need to ask about
	// modified documents.
	gAppViewerp->userQuit();

	// Do not quit immediately
	return false;
}

//virtual
void LLViewerWindow::handleQuit(LLWindow* window)
{
	llinfos << "Quit window event received." << llendl;
	gAppViewerp->forceQuit();
}

//virtual
void LLViewerWindow::handleResize(LLWindow* window, S32 width, S32 height)
{
	reshape(width, height);
	mResDirty = true;
}

// The top-level window has gained focus (e.g. via ALT-TAB)
//virtual
void LLViewerWindow::handleFocus(LLWindow* window)
{
	gFocusMgr.setAppHasFocus(true);
	LLModalDialog::onAppFocusGained();

	gAgent.onAppFocusGained();
	gToolMgr.onAppFocusGained();

	gShowTextEditCursor = true;

	// See if we are coming in with modifier keys held down
	if (gKeyboardp)
	{
		gKeyboardp->resetMaskKeys();
	}
}

// The top-level window has lost focus (e.g. via ALT-TAB)
//virtual
void LLViewerWindow::handleFocusLost(LLWindow* window)
{
	gFocusMgr.setAppHasFocus(false);
	gToolMgr.onAppFocusLost();
	gFocusMgr.setMouseCapture(NULL);

	if (gMenuBarViewp)
	{
		// Stop ALT-key access to menu
		gMenuBarViewp->resetMenuTrigger();
	}

	// Restore mouse cursor
	showCursor();
	gWindowp->setMouseClipping(false);

	gShowTextEditCursor = false;

	// If losing focus while keys are down, reset them.
	if (gKeyboardp)
	{
		gKeyboardp->resetKeys();
	}
}

//virtual
bool LLViewerWindow::handleTranslatedKeyDown(KEY key, MASK mask, bool repeated)
{
	// Let the voice chat code check for its PTT key. Note that this never
	// affects event processing.
	if (LLVoiceClient::sInitDone)
	{
		gVoiceClient.keyDown(key, mask);
	}

	if (gAwayTimer.getElapsedTimeF32() > MIN_AFK_TIME)
	{
		gAgent.clearAFK();
	}

	// *NOTE: we want to interpret KEY_RETURN later when it arrives as a
	// Unicode char, not as a keydown. Otherwise when client frame rate is
	// really low, hitting return sends your chat text before it is all
	// entered/processed.
	if (key == KEY_RETURN && mask == MASK_NONE)
	{
		// RIDER: although, at times some of the controls (in particular the
		// CEF viewer would like to know about the KEYDOWN for an enter key...
		// So ask and pass it along.
		LLFocusableElement* keyboard_focus = gFocusMgr.getKeyboardFocus();
		if (!keyboard_focus || !keyboard_focus->wantsReturnKey())
		{
			return false;
		}
	}

	return gViewerKeyboard.handleKey(key, mask, repeated);
}

//virtual
bool LLViewerWindow::handleTranslatedKeyUp(KEY key, MASK mask)
{
	// Let the voice chat code check for its PTT key. Note that this never
	// affects event processing.
	if (LLVoiceClient::sInitDone)
	{
		gVoiceClient.keyUp(key, mask);
	}

	return gViewerKeyboard.handleKeyUp(key, mask);
}

//virtual
void LLViewerWindow::handleScanKey(KEY key, bool key_down, bool key_up,
								   bool key_level)
{
	LLViewerJoystick::getInstance()->setCameraNeedsUpdate(true);
	return gViewerKeyboard.scanKey(key, key_down, key_up, key_level);
}

//virtual
bool LLViewerWindow::handleActivate(LLWindow* window, bool activated)
{
	if (activated)
	{
		mActive = true;
		LLWorld::sendAgentResume();
		gAgent.clearAFK();
		if (gWindowp->getFullscreen() && !mIgnoreActivate)
		{
			if (!LLApp::isExiting())
			{
				if (LLStartUp::isLoggedIn())
				{
					// If we are in world, show a progress bar to hide
					// reloading of textures
					llinfos << "Restoring GL during activate" << llendl;
					restoreGL("Restoring...");
				}
				else
				{
					// Otherwise restore immediately
					restoreGL();
				}
			}
			else
			{
				llwarns << "Activating while quitting" << llendl;
			}
		}

		// Unmute audio
		audio_update_volume();
	}
	else
	{
		mActive = false;
		gAppViewerp->idleAFKCheck(true);

		if (gAgent.cameraMouselook())
		{
			// Switch back to mouselook toolset
			gToolMgr.setCurrentToolset(gMouselookToolset);
			gSelectMgr.deselectAll();
			gViewerWindowp->hideCursor();
			gViewerWindowp->moveCursorToCenter();
		}

		LLWorld::sendAgentPause();

		if (gWindowp->getFullscreen() && !mIgnoreActivate)
		{
			llinfos << "Stopping GL during deactivation" << llendl;
			stopGL();
		}
		// Mute audio
		audio_update_volume();
	}

	return true;
}

//virtual
bool LLViewerWindow::handleActivateApp(LLWindow* window, bool activating)
{
	LLViewerJoystick::getInstance()->setNeedsReset(true);
	return false;
}

//virtual
void LLViewerWindow::handleMenuSelect(LLWindow* window, S32 menu_item)
{
}

//virtual
bool LLViewerWindow::handlePaint(LLWindow* window, S32 x, S32 y, S32 width,
								 S32 height)
{
	return false;
}

//virtual
void LLViewerWindow::handleScrollWheel(LLWindow* window, S32 clicks)
{
	handleScrollWheel(clicks);
}

//virtual
void LLViewerWindow::handleWindowBlock(LLWindow* window)
{
	LLWorld::sendAgentPause();
}

//virtual
void LLViewerWindow::handleWindowUnblock(LLWindow* window)
{
	LLWorld::sendAgentResume();
}

//virtual
void LLViewerWindow::handleDataCopy(LLWindow* window, S32 data_type,
									void* data)
{
	constexpr S32 SLURL_MESSAGE_TYPE = 0;
	if (data_type == SLURL_MESSAGE_TYPE)
	{
		// received URL
		std::string url = (const char*)data;
		LLMediaCtrl* web = NULL;
		if (LLURLDispatcher::dispatch(url, "", web, false))
		{
			// bring window to foreground, as it has just been "launched" from
			// a URL
			gWindowp->bringToFront();
		}
	}
}

#if LL_WINDOWS
//virtual
bool LLViewerWindow::handleTimerEvent(LLWindow* window)
{
	LLViewerJoystick* joystick = LLViewerJoystick::getInstance();
	if (joystick->getOverrideCamera())
	{
		joystick->updateStatus();
		return true;
	}
	return false;
}

//virtual
bool LLViewerWindow::handleDeviceChange(LLWindow* window)
{
	// Give a chance to use a joystick after startup (hot-plugging)
	LLViewerJoystick* joystick = LLViewerJoystick::getInstance();
	if (!joystick->isJoystickInitialized())
	{
		joystick->init(true);
		return true;
	}
	return false;
}

//virtual
bool LLViewerWindow::handleDPIChanged(LLWindow* window, F32 ui_scale_factor,
									  S32 window_width, S32 window_height)
{
	if (LLApp::isExiting())
	{
		LL_DEBUGS("Window") << "Application is exiting, not reshaping the window."
							<< LL_ENDL;
		return false;
	}
	// HiDPI scaling can be 4x. UI scaling in prefs is up to 2x, so max is 8x
	if (ui_scale_factor < 0.75f || ui_scale_factor > 8.f)
	{
		llwarns << "DPI change caused UI scale to go out of bounds: "
				<< ui_scale_factor << ". Not reshaping window." << llendl;
		return false;
	}
	LL_DEBUGS("Window") << "Reshaping the window..." << LL_ENDL;
	reshape(window_width, window_height);
	mResDirty = true;
	return true;
}
#endif

//virtual
bool LLViewerWindow::handleWindowDidChangeScreen(LLWindow* window)
{
	LLCoordScreen size;
	gWindowp->getSize(&size);
	reshape(size.mX, size.mY);
	return true;
}

///////////////////////////////////////////////////////////////////////////////
//
// Hover handlers
//

// Update UI based on stored mouse position from mouse-move event processing.
bool LLViewerWindow::handlePerFrameHover()
{
	static std::string last_handle_msg;

	LLView::sMouseHandlerMessage.clear();

	if (!gFloaterViewp || !gKeyboardp) return true;

	const S32 x = mCurrentMousePoint.mX;
	const S32 y = mCurrentMousePoint.mY;
	MASK mask = gKeyboardp->currentMask(true);

	// RN: fix for asynchronous notification of mouse leaving window not
	// working
	LLCoordWindow mouse_pos;
	gWindowp->getCursorPosition(&mouse_pos);
	if (mouse_pos.mX < 0 || mouse_pos.mY < 0 ||
		mouse_pos.mX > mWindowRect.getWidth() ||
		mouse_pos.mY > mWindowRect.getHeight())
	{
		mMouseInWindow = false;
	}
	else
	{
		mMouseInWindow = true;
	}

	S32 dx = lltrunc((F32)(mCurrentMousePoint.mX - mLastMousePoint.mX) *
						   LLUI::sGLScaleFactor.mV[VX]);
	S32 dy = lltrunc((F32)(mCurrentMousePoint.mY - mLastMousePoint.mY) *
						   LLUI::sGLScaleFactor.mV[VY]);

	LLVector2 mouse_vel;

	static LLCachedControl<bool> mouse_smooth(gSavedSettings, "MouseSmooth");
	if (mouse_smooth)
	{
		static F32 fdx = 0.f;
		static F32 fdy = 0.f;
		F32 amount = llmin(gFrameIntervalSeconds * 16.f, 1.f);
		fdx = fdx + ((F32)dx - fdx) * amount;
		fdy = fdy + ((F32)dy - fdy) * amount;
		mCurrentMouseDelta.set(ll_round(fdx), ll_round(fdy));
		mouse_vel.set(fdx, fdy);
	}
	else
	{
		mCurrentMouseDelta.set(dx, dy);
		mouse_vel.set((F32)dx, (F32)dy);
	}

	sMouseVelocityStat.addValue(mouse_vel.length());

	// Clean up current focus
	LLUICtrl* cur_focus = gFocusMgr.getKeyboardFocusUICtrl();
	if (cur_focus)
	{
		if (!cur_focus->isInVisibleChain() || !cur_focus->isInEnabledChain())
		{
			gFocusMgr.releaseFocusIfNeeded(cur_focus);

			LLUICtrl* parent = cur_focus->getParentUICtrl();
			const LLUICtrl* focus_root = cur_focus->findRootMostFocusRoot();
			while (parent)
			{
				if (parent->isCtrl() &&
					(parent->hasTabStop() || parent == focus_root) &&
					!parent->getIsChrome() &&
					parent->isInVisibleChain() &&
					parent->isInEnabledChain())
				{
					if (!parent->focusFirstItem())
					{
						parent->setFocus(true);
					}
					break;
				}
				parent = parent->getParentUICtrl();
			}
		}
		else if (cur_focus->isFocusRoot())
		{
			// Focus roots keep trying to delegate focus to their first valid
			// descendant; this assumes that focus roots are not valid focus
			// holders on their own.
			cur_focus->focusFirstItem();
		}
	}

	bool handled = false;
	bool handled_by_top_ctrl = false;
	LLUICtrl* top_ctrl = gFocusMgr.getTopCtrl();

	LLMouseHandler* mouse_captor = gFocusMgr.getMouseCapture();
	if (mouse_captor)
	{
		// Pass hover events to object capturing mouse events.
		S32 local_x;
		S32 local_y;
		mouse_captor->screenPointToLocal(x, y, &local_x, &local_y);
		handled = mouse_captor->handleHover(local_x, local_y, mask);
		if (LLView::sDebugMouseHandling)
		{
			llinfos << "Hover handled by captor " << mouse_captor->getName()
					<< llendl;
		}

		if (!handled)
		{
			LL_DEBUGS("UserInput") << "hover not handled by mouse captor"
								   << LL_ENDL;
		}
	}
	else
	{
		if (top_ctrl)
		{
			S32 local_x, local_y;
			top_ctrl->screenPointToLocal(x, y, &local_x, &local_y);
			handled = top_ctrl->pointInView(local_x, local_y) &&
					  top_ctrl->handleHover(local_x, local_y, mask);
			handled_by_top_ctrl = true;
		}

		if (!handled)
		{
			// x and y are from last time mouse was in window
			// mMouseInWindow tracks *actual* mouse location
			if (mMouseInWindow && mRootView->handleHover(x, y, mask))
			{
				if (LLView::sDebugMouseHandling &&
					LLView::sMouseHandlerMessage != last_handle_msg)
				{
					last_handle_msg = LLView::sMouseHandlerMessage;
					llinfos << "Hover" << LLView::sMouseHandlerMessage
							<< llendl;
				}
				handled = true;
			}
			else if (LLView::sDebugMouseHandling)
			{
				if (last_handle_msg != LLStringUtil::null)
				{
					last_handle_msg.clear();
					llinfos << "Hover not handled by view" << llendl;
				}
			}
		}

		if (!handled)
		{
			LL_DEBUGS("UserInput") << "hover not handled by top view or root"
								   << LL_ENDL;
		}
	}

	LLToolPie* toolpie = &gToolPie;

	// *NOTE: sometimes tools handle the mouse as a captor, so this logic is a
	// little confusing
	LLTool* tool = NULL;
	if (gHoverViewp)
	{
		tool = gToolMgr.getCurrentTool();

		if (!handled && tool)
		{
			handled = tool->handleHover(x, y, mask);

			if (!gWindowp->isCursorHidden())
			{
				gHoverViewp->updateHover(tool);
			}
		}
		else
		{
			// Cancel hovering if any UI element handled the event.
			gHoverViewp->cancelHover();
		}
		// Suppress the toolbox view if our source tool was the pie tool and
		// we have overridden to something else.
		mSuppressToolbox = gToolMgr.getBaseTool() == toolpie &&
						   gToolMgr.getCurrentTool() != toolpie;

	}

	// Show a new tool tip (or update one that is alrady shown)
	bool tool_tip_handled = false;
	std::string tool_tip_msg;
	static LLCachedControl<F32> normal_tool_tip_delay(gSavedSettings,
													  "ToolTipDelay");
	static LLCachedControl<F32> dad_tool_tip_delay(gSavedSettings,
												   "DragAndDropToolTipDelay");
	F32 tooltip_delay = normal_tool_tip_delay;
	// *HACK: hack for tool-based tooltips which need to pop up more quickly
	// Also for show xui names as tooltips debug mode
	if ((mouse_captor && !mouse_captor->isView()) || LLUI::sShowXUINames)
	{
		tooltip_delay = dad_tool_tip_delay;
	}
	if (handled && !gWindowp->isCursorHidden() &&
		gMouseIdleTimer.getElapsedTimeF32() > tooltip_delay)
	{
		LLRect screen_sticky_rect;
		LLMouseHandler *mh;
		S32 local_x, local_y;
		if (mouse_captor)
		{
			mouse_captor->screenPointToLocal(x, y, &local_x, &local_y);
			mh = mouse_captor;
		}
		else if (handled_by_top_ctrl)
		{
			top_ctrl->screenPointToLocal(x, y, &local_x, &local_y);
			mh = top_ctrl;
		}
		else
		{
			local_x = x; local_y = y;
			mh = mRootView;
		}

		bool tooltip_vis = false;
		if (shouldShowToolTipFor(mh))
		{
			tool_tip_handled = mh->handleToolTip(local_x, local_y,
												 tool_tip_msg,
												 &screen_sticky_rect);
			if (mToolTip && tool_tip_handled && !tool_tip_msg.empty())
			{
				mToolTipStickyRect = screen_sticky_rect;
				mToolTip->setWrappedText(tool_tip_msg, 200);
				mToolTip->reshapeToFitText();
				mToolTip->setOrigin(x, y);
				LLRect virtual_window_rect(0, getWindowHeight(),
										   getWindowWidth(), 0);
				mToolTip->translateIntoRect(virtual_window_rect, false);
				tooltip_vis = true;
			}
		}

		if (mToolTip)
		{
			mToolTip->setVisible(tooltip_vis);
		}
	}

	if (gFloaterToolsp && tool && tool != gToolNull &&
		tool != &gToolCompInspect && tool != &gToolDragAndDrop &&
		!LLPipeline::sFreezeTime)
	{
		LLMouseHandler* captor = gFocusMgr.getMouseCapture();
		// With the null, inspect, or drag and drop tool, do not muck with
		// visibility.
		if (gFloaterToolsp->isMinimized() ||
			(tool != toolpie &&	// Not default tool
			 // Not coming out of mouselook
			 tool != &gToolCompGun &&
			 // Not override in third person
			 !mSuppressToolbox &&
			 // Not in a special mode
			 gToolMgr.getCurrentToolset() != gFaceEditToolset &&
			 gToolMgr.getCurrentToolset() != gMouselookToolset &&
			 // Not dragging
			 (!captor || captor->isView())))
		{
			// Force floater tools to be visible (unless minimized)
			if (!LLFloaterTools::isVisible())
			{
				gFloaterToolsp->open();
			}
			// Update the location of the blue box tool popup
			LLCoordGL select_center_screen;
			gFloaterToolsp->updatePopup(select_center_screen, mask);
		}
		else
		{
			gFloaterToolsp->setVisible(false);
		}
	}

	if (gToolBarp)
	{
		gToolBarp->refresh();
	}

	if (gChatBarp)
	{
		gChatBarp->refresh();
	}

	if (gOverlayBarp)
	{
		if (gOverlayBarp->getVisible())
		{
			if (gAgent.cameraMouselook())
			{
				// Turn off the whole bar in mouselook
				gOverlayBarp->setVisible(false);
			}
		}
		else if (!gAgent.cameraMouselook())
		{
			// Turn on the bar when no more in mouse-look
			gOverlayBarp->setVisible(true);
		}
	}

	if (gLuaSideBarp)
	{
		if (gLuaSideBarp->getVisible())
		{
			if (gAgent.cameraMouselook())
			{
				// Turn off the whole bar in mouselook
				gLuaSideBarp->setVisible(false);
			}
		}
		else if (!gAgent.cameraMouselook())
		{
			// Turn on the bar when no more in mouse-look
			gLuaSideBarp->setVisible(true);
		}
	}

	// Update rectangles for the various toolbars
	if (gOverlayBarp && gNotifyBoxViewp && gFloaterViewp && gConsolep &&
		gToolBarp && gChatBarp)
	{
		LLRect bar_rect(-1, gStatusBarHeight, getWindowWidth() + 1, -1);
		if (gToolBarp->getVisible())
		{
			gToolBarp->setRect(bar_rect);
			bar_rect.translate(0, gStatusBarHeight - 1);
		}

		if (gChatBarp->getVisible())
		{
			// Fix up the height
			LLRect chat_bar_rect = bar_rect;
			chat_bar_rect.mTop = chat_bar_rect.mBottom + CHAT_BAR_HEIGHT + 1;
			gChatBarp->setRect(chat_bar_rect);
			bar_rect.translate(0, CHAT_BAR_HEIGHT - 1);
		}

		LLRect notify_box_rect = gNotifyBoxViewp->getRect();
		notify_box_rect.mBottom = bar_rect.mBottom;
		gNotifyBoxViewp->reshape(notify_box_rect.getWidth(),
								 notify_box_rect.getHeight());
		gNotifyBoxViewp->setRect(notify_box_rect);

		// Make sure floaters snap to visible rect by adjusting floater view
		// rect
		LLRect floater_rect = gFloaterViewp->getRect();
		if (floater_rect.mBottom != bar_rect.mBottom + 1)
		{
			floater_rect.mBottom = bar_rect.mBottom + 1;
			// Don't bounce the floaters up and down.
			gFloaterViewp->reshapeFloater(floater_rect.getWidth(),
										  floater_rect.getHeight(),
										  true, ADJUST_VERTICAL_NO);
			gFloaterViewp->setRect(floater_rect);
		}

		if (gOverlayBarp->getVisible())
		{
			LLRect overlay_rect = bar_rect;
			overlay_rect.mTop = overlay_rect.mBottom + OVERLAY_BAR_HEIGHT;

			// Fitt's Law: push buttons flush with bottom of screen if nothing
			// else visible.
			if (!gToolBarp->getVisible() && !gChatBarp->getVisible())
			{
				// *NOTE: this is highly depenent on the XML describing the
				// position of the buttons
				overlay_rect.translate(0, 0);
			}

			gOverlayBarp->setRect(overlay_rect);
			gOverlayBarp->updateBoundingRect();
			bar_rect.translate(0, gOverlayBarp->getRect().getHeight());

			gFloaterViewp->setSnapOffsetBottom(OVERLAY_BAR_HEIGHT);
		}
		else
		{
			gFloaterViewp->setSnapOffsetBottom(0);
		}

		// Fix rectangle of bottom panel focus indicator
		if (gBottomPanelp && gBottomPanelp->getFocusIndicator())
		{
			LLRect focus_rect = gBottomPanelp->getFocusIndicator()->getRect();
			focus_rect.mTop = (gToolBarp->getVisible() ? gStatusBarHeight : 0) +
							  (gChatBarp->getVisible() ? CHAT_BAR_HEIGHT : 0) - 2;
			gBottomPanelp->getFocusIndicator()->setRect(focus_rect);
		}

		// Always update console
		LLRect console_rect = getChatConsoleRect();
		console_rect.mBottom = bar_rect.mBottom - 8;
		gConsolep->reshape(console_rect.getWidth(), console_rect.getHeight());
		gConsolep->setRect(console_rect);
	}

	mLastMousePoint = mCurrentMousePoint;

	// Last ditch force of edit menu to selection manager
	if (!gEditMenuHandlerp && gSelectMgr.getSelection()->getObjectCount())
	{
		gEditMenuHandlerp = &gSelectMgr;
	}

	if (gFloaterViewp->getCycleMode())
	{
		// sync all floaters with their focus state
		gFloaterViewp->highlightFocusedFloater();
		gSnapshotFloaterViewp->highlightFocusedFloater();
		// When user is holding down CTRL, do not update tab order of floaters
		if ((gKeyboardp->currentMask(true) & MASK_CONTROL) == 0)
		{
			// Control key no longer held down, finish cycle mode
			gFloaterViewp->setCycleMode(false);

			gFloaterViewp->syncFloaterTabOrder();
		}
	}
	else
	{
		// Update focused floater
		gFloaterViewp->highlightFocusedFloater();
		gSnapshotFloaterViewp->highlightFocusedFloater();
		// Make sure floater visible order is in sync with tab order
		gFloaterViewp->syncFloaterTabOrder();
	}

	static LLCachedControl<bool> chat_bar_steals_focus(gSavedSettings,
													   "ChatBarStealsFocus");
	if (chat_bar_steals_focus && gChatBarp &&
		gFocusMgr.getKeyboardFocus() == NULL &&
		gChatBarp->isInVisibleChain())
	{
		LLChatBar::startChat(NULL);
	}

	// Cleanup unused selections when no modal dialogs are open
	if (LLModalDialog::activeCount() == 0)
	{
		gViewerParcelMgr.deselectUnused();
	}

	if (LLModalDialog::activeCount() == 0)
	{
		gSelectMgr.deselectUnused();
	}

	if (gPipeline.hasRenderDebugMask(LLPipeline::RENDER_DEBUG_RAYCAST))
	{
		gDebugRaycastFaceHit = -1;
		gDebugRaycastObject = cursorIntersect(-1, -1, 512.f, NULL, -1,
											  false, false,
											  &gDebugRaycastFaceHit,
											  &gDebugRaycastIntersection,
											  &gDebugRaycastTexCoord,
											  &gDebugRaycastNormal,
											  &gDebugRaycastTangent,
											  &gDebugRaycastStart,
											  &gDebugRaycastEnd);

		gDebugRaycastParticle =
			gPipeline.lineSegmentIntersectParticle(gDebugRaycastStart,
												   gDebugRaycastEnd,
												   &gDebugRaycastParticleIntersection,
												   NULL);
	}

	// Per frame picking for tooltips and changing cursor over interactive
	// objects
	static S32 previous_x = -1;
	static S32 previous_y = -1;
	bool mouse_moved_since_pick = previous_x != x || previous_y != y;

	bool do_pick = false;

	static LLCachedControl<F32> picks_moving(gSavedSettings,
											 "PicksPerSecondMouseMoving");
	if (mouse_moved_since_pick && picks_moving > 0.f &&
		mPickTimer.getElapsedTimeF32() > 1.f / picks_moving)
	{
		do_pick = true;
	}

	static LLCachedControl<F32> picks_stationary(gSavedSettings,
												 "PicksPerSecondMouseStationary");
	if (!mouse_moved_since_pick && picks_stationary > 0.f &&
		mPickTimer.getElapsedTimeF32() > 1.f / picks_stationary)
	{
		do_pick = true;
	}

	if (getCursorHidden())
	{
		do_pick = false;
	}

	if (LLViewerMediaFocus::getInstance()->getFocus())
	{
		// When in-world media is in focus, pick every frame so that browser
		// mouse-overs, dragging scrollbars, etc. work properly.
		do_pick = true;
	}

	if (do_pick)
	{
		mPickTimer.reset();
		static LLCachedControl<bool> pick_transparent(gSavedSettings,
													  "AllowPickTransparent");
		pickAsync(x, y, mask, hoverPickCallback, pick_transparent,
				  false, false, true);
	}

	previous_x = x;
	previous_y = y;

	return handled;
}

//static
void LLViewerWindow::hoverPickCallback(const LLPickInfo& pick_info)
{
	gViewerWindowp->mHoverPick = pick_info;
}

void LLViewerWindow::saveLastMouse(const LLCoordGL& point)
{
	// Store last mouse location; if the mouse leaves the window, pretend last
	// point was on edge of window.
	if (point.mX < 0)
	{
		mCurrentMousePoint.mX = 0;
	}
	else if (point.mX > getWindowWidth())
	{
		mCurrentMousePoint.mX = getWindowWidth();
	}
	else
	{
		mCurrentMousePoint.mX = point.mX;
	}

	if (point.mY < 0)
	{
		mCurrentMousePoint.mY = 0;
	}
	else if (point.mY > getWindowHeight())
	{
		mCurrentMousePoint.mY = getWindowHeight();
	}
	else
	{
		mCurrentMousePoint.mY = point.mY;
	}
}

// Draws the selection outlines for the currently selected objects. Must be
// called after displayObjects is called, which sets the mGLName parameter
// NOTE: This function gets called 3 times:
//  render_ui_3d: 			false, false, true
//  renderObjectsForSelect:	true, pick_parcel_wall, false
//  render_hud_elements:	false, false, false
void LLViewerWindow::renderSelections(bool for_gl_pick, bool pick_parcel_walls,
									  bool for_hud)
{
	if (!for_hud && !for_gl_pick)
	{
		// Call this once and only once
		gSelectMgr.updateSilhouettes();
	}

	// Draw fence around land selections
	if (for_gl_pick)
	{
		if (pick_parcel_walls)
		{
			gViewerParcelMgr.renderParcelCollision();
		}
		stop_glerror();
		return;
	}

	LLObjectSelectionHandle selection = gSelectMgr.getSelection();

	bool is_hud = selection->getSelectType() == SELECT_TYPE_HUD;
	if (for_hud != is_hud)
	{
		return;
	}

	gSelectMgr.renderSilhouettes(for_hud);

	bool in_edit = gToolMgr.inEdit();

	// *FIXME: this is a total hack (borrowed from Firestorm, Beq's code). The
	// proper fix to the 0 LOD on some edited mesh objects would be to find
	// why in the first place that low LOD gets wrongly used. So far, I did not
	// find where this happens. HB
	static LLCachedControl<S32> edit_lod(gSavedSettings, "EditedMeshLOD");
	if (in_edit && !is_hud && edit_lod >= 0)
	{
		struct LLFunctorApplyLOD : public LLSelectedObjectFunctor
		{
			LLFunctorApplyLOD(S32 lod)
			:	mLOD(lod)
			{
			}

			bool apply(LLViewerObject* objectp) override
			{
				if (objectp && objectp->isMesh())
				{
					((LLVOVolume*)objectp)->tempSetLOD(mLOD);
				}
				return true;
			}

			S32 mLOD;
		};
		LLFunctorApplyLOD func(llmin((S32)edit_lod, 3));
		selection->applyToObjects(&func);
	}

	// Setup HUD render
	if (for_hud && gSelectMgr.getSelection()->getObjectCount())
	{
		LLBBox hud_bbox = gAgentAvatarp->getHUDBBox();

		// Set-up transform to encompass bounding box of HUD
		gGL.matrixMode(LLRender::MM_PROJECTION);
		gGL.pushMatrix();
		gGL.loadIdentity();
		F32 depth = llmax(1.f, hud_bbox.getExtentLocal().mV[VX] * 1.1f);
		F32 aspect = gViewerCamera.getAspect();
		gGL.ortho(-0.5f * aspect, 0.5f * aspect, -0.5f, 0.5f, 0.f, depth);

		gGL.matrixMode(LLRender::MM_MODELVIEW);
		gGL.pushMatrix();
		gGL.loadIdentity();
		// Load Cory's favorite reference frame
		gGL.loadMatrix(OGL_TO_CFR_ROT4A);
		gGL.translatef(-hud_bbox.getCenterLocal().mV[VX] + depth * 0.5f, 0.f,
					   0.f);
	}

	// Render light for editing
	if (in_edit && LLSelectMgr::sRenderLightRadius)
	{
		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
		LLGLEnable gls_blend(GL_BLEND);
		LLGLEnable gls_cull(GL_CULL_FACE);
		LLGLDepthTest gls_depth(GL_TRUE, GL_FALSE);
		gGL.matrixMode(LLRender::MM_MODELVIEW);
		gGL.pushMatrix();
		if (selection->getSelectType() == SELECT_TYPE_HUD)
		{
			F32 zoom = gAgent.mHUDCurZoom;
			gGL.scalef(zoom, zoom, zoom);
		}

		struct f final : public LLSelectedObjectFunctor
		{
			bool apply(LLViewerObject* object) override
			{
				LLDrawable* drawable = object->mDrawable;
				if (drawable && drawable->isLight())
				{
					LLVOVolume* vovolume = drawable->getVOVolume();
					gGL.pushMatrix();

					LLVector3 center = drawable->getPositionAgent();
					gGL.translatef(center[0], center[1], center[2]);
					F32 scale = vovolume->getLightRadius();
					gGL.scalef(scale, scale, scale);

					LLColor4 color(vovolume->getLightSRGBColor(), 0.5f);
					gGL.color4fv(color.mV);

					// Render Outside
					gSphere.render();

					// Render Inside
					glCullFace(GL_FRONT);
					gSphere.render();
					glCullFace(GL_BACK);

					gGL.popMatrix();
				}
				return true;
			}
		} func;
		gSelectMgr.getSelection()->applyToObjects(&func);

		gGL.popMatrix();
	}

	// NOTE: The average position for the axis arrows of the selected objects
	// should not be recalculated at this time. If they are, then group
	// rotations will break.

	// Draw arrows at average center of all selected objects
	LLTool* tool = gToolMgr.getCurrentTool();
	if (!tool)
	{
		stop_glerror();
		return;
	}

	if (tool->isAlwaysRendered())
	{
		tool->render();
	}
	else if (!gSelectMgr.getSelection()->isEmpty())
	{
		bool sel_can_move, sel_is_mod_ok;
		// *TODO: This might be costly to do on each frame and when a lot of
		// objects are selected. We might be better off with some kind of
		// memory for selection and/or states: consider optimizing, perhaps
		// even some kind of selection generation at level of LLSelectMgr to
		// make whole viewer benefit.
		gSelectMgr.selectGetEditMoveLinksetPermissions(sel_can_move,
													   sel_is_mod_ok);
		bool draw_handles = true;
		if (!sel_is_mod_ok && tool == &gToolCompScale)
		{
			draw_handles = false;
		}
		else if (!sel_can_move &&
				 (tool == &gToolCompTranslate || tool == &gToolCompRotate))
		{
			draw_handles = false;
		}
		if (draw_handles)
		{
			tool->render();
		}
	}
	if (is_hud && selection->getObjectCount())
	{
		gGL.matrixMode(LLRender::MM_PROJECTION);
		gGL.popMatrix();

		gGL.matrixMode(LLRender::MM_MODELVIEW);
		gGL.popMatrix();
	}
	stop_glerror();
}

// Return a point near the clicked object representative of the place the
// object was clicked.
LLVector3d LLViewerWindow::clickPointInWorldGlobal(S32 x, S32 y_from_bot,
												   LLViewerObject* clicked_object) const
{
	// Create a normalized vector pointing from the camera center into the
	// world at the location of the mouse click
	LLVector3 mouse_direction_global = mouseDirectionGlobal(x, y_from_bot);

	LLVector3d relative_object = clicked_object->getPositionGlobal() -
								 gAgent.getCameraPositionGlobal();

	// Make mouse vector as long as object vector, so it touchs a point near
	// where the user clicked on the object
	mouse_direction_global *= (F32) relative_object.length();

	LLVector3d new_pos;
	new_pos.set(mouse_direction_global);
	// Transform mouse vector back to world coords
	new_pos += gAgent.getCameraPositionGlobal();

	return new_pos;
}

void LLViewerWindow::pickAsync(S32 x, S32 y_from_bot, MASK mask,
							   void (*callback)(const LLPickInfo& info),
							   bool pick_transparent, bool pick_rigged,
							   bool pick_particle, bool get_surface_info)
{
	// Push back pick info object
	if (LLFloaterTools::isVisible() || LLDrawPoolAlpha::sShowDebugAlpha)
	{
		// Build mode allows interaction with all transparent objects
		// "Show Debug Alpha" means no object actually transparent
		pick_transparent = true;
	}

	LLPickInfo pick_info(LLCoordGL(x, y_from_bot), mask, pick_transparent,
						 pick_rigged, pick_particle, get_surface_info,
						 callback);
	schedulePick(pick_info);
}

void LLViewerWindow::schedulePick(LLPickInfo& pick_info)
{
	if (mPicks.size() >= 1024 || gWindowp->getMinimized())
	{
		// Something went wrong, picks are being scheduled but not processed
		if (pick_info.mPickCallback)
		{
			pick_info.mPickCallback(pick_info);
		}
		return;
	}
	mPicks.emplace_back(pick_info);

	// Delay further event processing until we receive results of pick
	gWindowp->delayInputProcessing();
}

void LLViewerWindow::performPick()
{
	if (!mPicks.empty())
	{
		for (pick_info_list_t::iterator it = mPicks.begin(),
										end = mPicks.end();
			 it != end; ++it)
		{
			it->fetchResults();
		}

		mLastPick = mPicks.back();
		mPicks.clear();
	}
}

void LLViewerWindow::returnEmptyPicks()
{
	for (pick_info_list_t::iterator it = mPicks.begin(), end = mPicks.end();
		 it != end; ++it)
	{
		mLastPick = *it;
		// Just trigger callback with empty results
		if (it->mPickCallback)
		{
			it->mPickCallback(*it);
		}
	}
	mPicks.clear();
}

// Performs the GL object/land pick.
LLPickInfo LLViewerWindow::pickImmediate(S32 x, S32 y_from_bot,
										 bool pick_transparent)
{
	if (LLFloaterTools::isVisible() || LLDrawPoolAlpha::sShowDebugAlpha)
	{
		// Build mode allows interaction with all transparent objects
		// "Show Debug Alpha" means no object actually transparent
		pick_transparent = true;
	}

	// Shortcut queueing in mPicks and just update mLastPick in place
	MASK key_mask = gKeyboardp ? gKeyboardp->currentMask(true) : 0;
	mLastPick = LLPickInfo(LLCoordGL(x, y_from_bot), key_mask,
						   pick_transparent, false, false, true, NULL);
	mLastPick.fetchResults();

	return mLastPick;
}

LLHUDIcon* LLViewerWindow::cursorIntersectIcon(S32 mouse_x, S32 mouse_y,
											   F32 depth,
											   LLVector4a* intersection)
{
	S32 x = mouse_x;
	S32 y = mouse_y;

	if (mouse_x == -1 && mouse_y == -1) // use current mouse position
	{
		x = getCurrentMouseX();
		y = getCurrentMouseY();
	}

	// World coordinates of mouse
	// *TODO: VECTORIZE THIS
	LLVector3 mouse_direction_global = mouseDirectionGlobal(x,y);
	LLVector3 mouse_point_global = gViewerCamera.getOrigin();
	LLVector3 mouse_world_start = mouse_point_global;
	LLVector3 mouse_world_end = mouse_point_global +
								mouse_direction_global * depth;

	LLVector4a start, end;
	start.load3(mouse_world_start.mV);
	end.load3(mouse_world_end.mV);

	return LLHUDIcon::lineSegmentIntersectAll(start, end, intersection);
}

LLViewerObject* LLViewerWindow::cursorIntersect(S32 mouse_x, S32 mouse_y,
												F32 depth,
												LLViewerObject* this_object,
												S32 this_face,
												bool pick_transparent,
												bool pick_rigged,
												S32* face_hit,
												LLVector4a* intersection,
												LLVector2* uv,
												LLVector4a* normal,
												LLVector4a* tangent,
												LLVector4a* start,
												LLVector4a* end)
{
	S32 x = mouse_x;
	S32 y = mouse_y;

	if (mouse_x == -1 && mouse_y == -1) // use current mouse position
	{
		x = getCurrentMouseX();
		y = getCurrentMouseY();
	}

	// HUD coordinates of mouse
	LLVector3 mouse_point_hud = mousePointHUD(x, y);
	LLVector3 mouse_hud_start = mouse_point_hud - LLVector3(depth, 0, 0);
	LLVector3 mouse_hud_end   = mouse_point_hud + LLVector3(depth, 0, 0);

	// World coordinates of mouse
	LLVector3 mouse_direction_global = mouseDirectionGlobal(x,y);
	LLVector3 mouse_point_global = gViewerCamera.getOrigin();

	// Get near clip plane
	LLVector3 n = gViewerCamera.getAtAxis();
	LLVector3 p = mouse_point_global + n * gViewerCamera.getNear();

	// Project mouse point onto plane
	LLVector3 pos;
	line_plane(mouse_point_global, mouse_direction_global, p, n, pos);
	mouse_point_global = pos;

	LLVector3 mouse_world_start = mouse_point_global;
	LLVector3 mouse_world_end   = mouse_point_global +
								  mouse_direction_global * depth;

	if (!LLViewerJoystick::getInstance()->getOverrideCamera())
	{
		// Always set raycast intersection to mouse_world_end unless flycam is
		// on (for DoF effect)
		gDebugRaycastIntersection.load3(mouse_world_end.mV);
	}

	LLVector4a mw_start;
	mw_start.load3(mouse_world_start.mV);
	LLVector4a mw_end;
	mw_end.load3(mouse_world_end.mV);

	LLVector4a mh_start;
	mh_start.load3(mouse_hud_start.mV);
	LLVector4a mh_end;
	mh_end.load3(mouse_hud_end.mV);

	if (start)
	{
		*start = mw_start;
	}

	if (end)
	{
		*end = mw_end;
	}

	LLViewerObject* found = NULL;

	if (this_object)  // Check only this object
	{
		if (this_object->isHUDAttachment()) // Is it a HUD object ?
		{
			if (this_object->lineSegmentIntersect(mh_start, mh_end, this_face,
												  pick_transparent,
												  pick_rigged, face_hit,
												  intersection, uv, normal,
												  tangent))
			{
				found = this_object;
			}
		}
		else // It is a world object
		{
			if (this_object->lineSegmentIntersect(mw_start, mw_end, this_face,
												  pick_transparent,
												  pick_rigged, face_hit,
												  intersection, uv, normal,
												  tangent))
			{
				found = this_object;
			}
//MK
			if (gRLenabled && gRLInterface.mContainsInteract)
			{
				found = NULL;
			}
//mk
		}
	}
	else // Check ALL objects
	{
		found = gPipeline.lineSegmentIntersectInHUD(mh_start, mh_end,
													pick_transparent,
													face_hit, intersection,
													uv, normal, tangent);
//MK
		// *HACK: do not allow focusing on HUDs while we are right-clicking on
		// something while not in mouse look: useful for "blinding" HUDs that
		// cover the whole screen, even when transparent.
		if (gRLenabled && !gAgent.cameraMouselook() &&
			gRLInterface.mHasLockedHuds)
		{
			MASK mask = gKeyboardp ? gKeyboardp->currentMask(true) : 0;
			if (mask & MASK_ALT)
			{
				found = NULL;
			}
		}
//mk
		if (!found) // If not found in HUD, look in world:
		{
			found = gPipeline.lineSegmentIntersectInWorld(mw_start, mw_end,
														  pick_transparent,
														  pick_rigged,
														  face_hit,
														  intersection,
														  uv, normal, tangent);
			if (found && !pick_transparent)
			{
				gDebugRaycastIntersection = *intersection;
			}
		}
	}

	return found;
}

// Returns unit vector relative to camera
// indicating direction of point on screen x,y
LLVector3 LLViewerWindow::mouseDirectionGlobal(S32 x, S32 y) const
{
	// Find vertical field of view
	F32	fov = gViewerCamera.getView();

	// Find screen resolution
	S32 height = getWindowHeight();
	S32 width = getWindowWidth();

	// Calculate pixel distance to screen
	F32 t = 2.f * tanf(fov * 0.5f);
	F32 distance = t == 0.f ? F32_MAX : height / t;

	// Calculate click point relative to middle of screen
	F32	click_x = x - width * 0.5f;
	F32 click_y = y - height * 0.5f;

	// Compute mouse vector
	LLVector3 mouse_vector = distance * gViewerCamera.getAtAxis() -
							 click_x * gViewerCamera.getLeftAxis() +
							 click_y * gViewerCamera.getUpAxis();
	mouse_vector.normalize();

	return mouse_vector;
}

LLVector3 LLViewerWindow::mousePointHUD(S32 x, S32 y) const
{
	// Find screen resolution
	S32 height = getWindowHeight();
	S32 width = getWindowWidth();

	// Remap with uniform scale (1/height) so that top is -0.5, bottom is +0.5
	F32 hud_x = -((F32)x - (F32)width * 0.5f) / height;
	F32 hud_y = ((F32)y - (F32)height * 0.5f) / height;

	return LLVector3(0.f, hud_x / gAgent.mHUDCurZoom,
					 hud_y / gAgent.mHUDCurZoom);
}

// Returns unit vector relative to camera in camera space indicating direction
// of point on screen x,y
LLVector3 LLViewerWindow::mouseDirectionCamera(S32 x, S32 y) const
{
	// Find vertical field of view
	F32 fov_height = gViewerCamera.getView();
	F32 fov_width = fov_height * gViewerCamera.getAspect();

	// Find screen resolution
	S32 height = getWindowHeight();
	S32 width = getWindowWidth();

	// Calculate click point relative to middle of screen
	F32 click_x = ((F32)x / (F32)width - 0.5f) * fov_width * -1.f;
	F32 click_y = ((F32)y / (F32)height - 0.5f) * fov_height;

	// compute mouse vector
	LLVector3 mouse_vector = LLVector3(0.f, 0.f, -1.f);
	LLQuaternion mouse_rotate;
	mouse_rotate.setEulerAngles(click_y, click_x, 0.f);

	mouse_vector = mouse_vector * mouse_rotate;
	// project to z = -1 plane;
	mouse_vector = mouse_vector * (-1.f / mouse_vector.mV[VZ]);

	return mouse_vector;
}

bool LLViewerWindow::mousePointOnPlaneGlobal(LLVector3d& point, S32 x, S32 y,
											 const LLVector3d& plane_point_global,
											 const LLVector3& plane_normal_global)
{
	LLVector3d	mouse_direction_global_d;

	mouse_direction_global_d.set(mouseDirectionGlobal(x, y));
	LLVector3d plane_normal_global_d;
	plane_normal_global_d.set(plane_normal_global);
	F64 plane_mouse_dot = plane_normal_global_d * mouse_direction_global_d;
	LLVector3d plane_origin_camera_rel = plane_point_global -
										 gAgent.getCameraPositionGlobal();
	F64	mouse_look_at_scale = plane_normal_global_d * plane_origin_camera_rel /
							  plane_mouse_dot;
	if (fabs(plane_mouse_dot) < 0.00001)
	{
		// If mouse is parallel to plane, return closest point on line through
		// plane origin that is parallel to camera plane by scaling mouse
		// direction vector by distance to plane origin, modulated by deviation
		// of mouse direction from plane origin
		LLVector3d plane_origin_dir = plane_origin_camera_rel;
		plane_origin_dir.normalize();

		mouse_look_at_scale = plane_origin_camera_rel.length() /
							  (plane_origin_dir * mouse_direction_global_d);
	}

	point = gAgent.getCameraPositionGlobal() +
			mouse_look_at_scale * mouse_direction_global_d;

	return mouse_look_at_scale > 0.0;
}

// Returns global position
bool LLViewerWindow::mousePointOnLandGlobal(S32 x, S32 y,
											LLVector3d* land_position_global)
{
	LLVector3 mouse_direction_global = mouseDirectionGlobal(x, y);
	F32 mouse_dir_scale;
	bool hit_land = false;
	LLViewerRegion* regionp;
	F32 land_z;
	constexpr F32 FIRST_PASS_STEP = 1.f;	// meters
	constexpr F32 SECOND_PASS_STEP = 0.1f;	// meters
	LLVector3d camera_pos_global;

	camera_pos_global = gAgent.getCameraPositionGlobal();
	LLVector3d probe_point_global;
	LLVector3 probe_point_region;

	F32 max_distance = gAgent.noCameraConstraints() ? 1024.f
													: gAgent.mDrawDistance;

	// Walk forwards to find the point
	for (mouse_dir_scale = FIRST_PASS_STEP;
		 mouse_dir_scale < max_distance;
		 mouse_dir_scale += FIRST_PASS_STEP)
	{
		LLVector3d mouse_direction_global_d;
		mouse_direction_global_d.set(mouse_direction_global * mouse_dir_scale);
		probe_point_global = camera_pos_global + mouse_direction_global_d;

		regionp = gWorld.resolveRegionGlobal(probe_point_region,
											 probe_point_global);

		if (!regionp)
		{
			// ...we are outside the world somehow
			continue;
		}

		S32 i = (S32)(probe_point_region.mV[VX] /
				regionp->getLand().getMetersPerGrid());
		S32 j = (S32)(probe_point_region.mV[VY] /
				regionp->getLand().getMetersPerGrid());
		S32 grids_per_edge = (S32)regionp->getLand().mGridsPerEdge;
		if (i >= grids_per_edge || j >= grids_per_edge)
		{
			continue;
		}

		land_z = regionp->getLand().resolveHeightRegion(probe_point_region);
		if (probe_point_region.mV[VZ] < land_z)
		{
			hit_land = true;
			break;
		}
	}

	if (hit_land)
	{
		// Do not go more than one step beyond where we stopped above. This
		// cannot just be "mouse_vec_scale" because floating point error will
		// stop the loop before the last increment...
		// X - 1.0 + 0.1 + 0.1 + ... + 0.1 != X
		F32 stop_mouse_dir_scale = mouse_dir_scale + FIRST_PASS_STEP;

		// Take a step backwards, then walk forwards again to refine position
		for (mouse_dir_scale -= FIRST_PASS_STEP;
			 mouse_dir_scale <= stop_mouse_dir_scale;
			 mouse_dir_scale += SECOND_PASS_STEP)
		{
			LLVector3d mouse_direction_global_d;
			mouse_direction_global_d.set(mouse_direction_global *
										 mouse_dir_scale);
			probe_point_global = camera_pos_global + mouse_direction_global_d;

			regionp = gWorld.resolveRegionGlobal(probe_point_region,
												 probe_point_global);

			if (!regionp)
			{
				// ...we are outside the world somehow
				continue;
			}

#if 0
			i = (S32)(local_probe_point.mV[VX] /
					  regionp->getLand().getMetersPerGrid());
			j = (S32)(local_probe_point.mV[VY] /
					  regionp->getLand().getMetersPerGrid());
			if (i >= regionp->getLand().mGridsPerEdge ||
				j >= regionp->getLand().mGridsPerEdge)
			{
				llwarns << "probe_point is out of region" << llendl;
				continue;
			}
			land_z =
				regionp->getLand().mSurfaceZ[i + j *
											 (regionp->getLand().mGridsPerEdge)];
#endif

			land_z =
				regionp->getLand().resolveHeightRegion(probe_point_region);
			if (probe_point_region.mV[VZ] < land_z)
			{
				// ...just went under land again
				*land_position_global = probe_point_global;
				return true;
			}
		}
	}

	return false;
}

void LLViewerWindow::setSnapshotLoc(std::string filepath)
{
	LLViewerWindow::sSnapshotBaseName = gDirUtilp->getBaseFileName(filepath,
																   true);
	LLViewerWindow::sSnapshotDir = gDirUtilp->getDirName(filepath);
}

// Saves an image to the harddrive as "SnapshotX" where X >= 1.
bool LLViewerWindow::saveImageNumbered(LLImageFormatted* image)
{
	if (!image || !isSnapshotLocSet())
	{
		return false;
	}

	// Look for an unused file name
	const std::string extension = "." + image->getExtension();
	std::string filepath;
	const std::string base_path = sSnapshotDir + LL_DIR_DELIM_STR +
								  sSnapshotBaseName;
	S32 i = 1;
	do
	{
		filepath = base_path + llformat("_%.3d", i++) + extension;
	}
	// Search until the file is not found
	while (LLFile::isfile(filepath));

	bool result = image->save(filepath);
	if (result)
	{
		playSnapshotAnimAndSound();
	}
	return result;
}

void LLViewerWindow::resetSnapshotLoc()
{
	sSnapshotDir.clear();
}

void LLViewerWindow::resizeWindow(S32 new_width, S32 new_height)
{
	static S32 border_width = 0;
	static S32 border_height = 0;

	LLCoordScreen size;
	gWindowp->getSize(&size);
	if (size.mX != new_width + border_width ||
		size.mY != new_height + border_height)
	{
		// Use the actual display dimensions, not the virtual UI dimensions
		border_width = size.mX - getWindowDisplayWidth();
		border_height = size.mY - getWindowDisplayHeight();
		LLCoordScreen new_size(new_width + border_width,
							   new_height + border_height);
		bool disable_sync = gSavedSettings.getBool("DisableVerticalSync");
		if (gWindowp->getFullscreen())
		{
			changeDisplaySettings(new_size, disable_sync, true);
		}
		else
		{
			gWindowp->setSize(new_size);
		}
	}

	mResDirty = true;
}

bool LLViewerWindow::saveSnapshot(const std::string& filepath,
								  S32 image_width, S32 image_height,
								  bool show_ui, bool do_rebuild,
								  U32 type)
{
	llinfos << "Saving snapshot to: " << filepath << llendl;

	LLPointer<LLImageRaw> raw = new LLImageRaw;
	bool success = rawSnapshot(raw, image_width, image_height, true, false,
							   show_ui, do_rebuild);
	if (success)
	{
		LLPointer<LLImageBMP> bmp_image = new LLImageBMP;
		success = bmp_image->encode(raw);
		if (success)
		{
			success = bmp_image->save(filepath);
		}
		else
		{
			llwarns << "Unable to encode bmp snapshot" << llendl;
		}
	}
	else
	{
		llwarns << "Unable to capture raw snapshot" << llendl;
	}

	return success;
}

void LLViewerWindow::playSnapshotAnimAndSound()
{
	if (!gSavedSettings.getBool("QuietSnapshotsToDisk"))
	{
		gAgent.sendAnimationRequest(ANIM_AGENT_SNAPSHOT, ANIM_REQUEST_START);
		if (gSavedSettings.getBool("UISndSnapshotEnable"))
		{
			send_sound_trigger(LLUUID(gSavedSettings.getString("UISndSnapshot")),
							   1.f);
		}
	}
}

bool LLViewerWindow::thumbnailSnapshot(LLImageRaw* raw, S32 preview_width,
									   S32 preview_height, bool show_ui,
									   bool do_rebuild, U32 type)
{
	return rawSnapshot(raw, preview_width, preview_height, false, false,
					   show_ui, do_rebuild, type);
}

// Saves the image from the screen to the specified filename and path.
bool LLViewerWindow::rawSnapshot(LLImageRaw* raw,
								 S32 image_width, S32 image_height,
								 bool keep_window_aspect, bool is_texture,
								 bool show_ui, bool do_rebuild,
								 U32 type, S32 max_size)
{
	if (!raw)
	{
		return false;
	}

	// Check if there is enough memory for the snapshot image
	if (LLMemory::gotFailedAllocation())
	{
		llwarns << "Snapshots disabled due to past memory allocation falures."
				<< llendl;
		return false;
	}

#if LL_LINUX
	// Avoids unrefreshed rectangles in screen shots when other applications
	// windows are overlapping ours. HB
	gWindowp->bringToFront();
	// Let some time to the window manager to bring us back to front.
	ms_sleep(100);
#endif

	// PRE SNAPSHOT
	gDisplaySwapBuffers = false;

	if (gUsePBRShaders)
	{
		gSnapshotNoPost = gSavedSettings.getBool("RenderSnapshotNoPost");
		glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
	}
	else
	{
		glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT |
				GL_STENCIL_BUFFER_BIT);
	}

	setCursor(UI_CURSOR_WAIT);

	// Hide all the UI widgets first and draw a frame
	bool prev_draw_ui =
		gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_UI);
	if (prev_draw_ui != show_ui)
	{
		LLPipeline::toggleRenderDebugFeature((void*)LLPipeline::RENDER_DEBUG_FEATURE_UI);
	}

//MK
	if (gRLenabled && gRLInterface.mHasLockedHuds)
	{
		gSavedSettings.setBool("RenderHUDInSnapshot", true);
	}
//mk

	bool hide_hud = LLPipeline::sShowHUDAttachments &&
					!gSavedSettings.getBool("RenderHUDInSnapshot");
	if (hide_hud)
	{
		LLPipeline::sShowHUDAttachments = false;
	}

	// Copy screen to a buffer; crop sides or top and bottom, if taking a
	// snapshot of different aspect ratio from window
	S32 snapshot_width = mWindowRect.getWidth();
	S32 snapshot_height = mWindowRect.getHeight();
	S32 window_width = mWindowRect.getWidth();
	S32 window_height = mWindowRect.getHeight();

	// Note: Scaling of the UI is currently *not* supported so we limit the
	// output size if UI is requested
	if (show_ui)
	{
		// If the user wants the UI, limit the output size to the available
		// screen size
		image_width  = llmin(image_width, window_width);
		image_height = llmin(image_height, window_height);
	}

	F32 scale_factor = 1.f;
	S32 max_width = llmin(window_width, gGLManager.mGLMaxTextureSize);
	S32 max_height = llmin(window_height, gGLManager.mGLMaxTextureSize);
	if (!keep_window_aspect || image_width > max_width ||
		image_height > max_height)
	{
		// If image cropping or need to enlarge the scene, compute a
		// scale_factor
		F32 ratio = llmin((F32)max_width / image_width ,
						  (F32)max_height / image_height);
		snapshot_width = (S32)(ratio * image_width);
		snapshot_height = (S32)(ratio * image_height);
		scale_factor = llmax(1.f, 1.f / ratio);
	}

	if (show_ui && scale_factor > 1.f)
	{
		// Note: we should never get there...
		llwarns << "Over scaling UI not supported." << llendl;
	}

	S32 buffer_x_offset = llfloor((window_width - snapshot_width) *
								  scale_factor * 0.5f);
	S32 buffer_y_offset = llfloor((window_height - snapshot_height) *
								  scale_factor * 0.5f);

	S32 image_buffer_x = llfloor(snapshot_width*scale_factor);
	S32 image_buffer_y = llfloor(snapshot_height *scale_factor);
	// Boundary check to avoid memory overflow
	if (image_buffer_x > max_size || image_buffer_y > max_size)
	{
		scale_factor *= llmin((F32)max_size / image_buffer_x,
							  (F32)max_size / image_buffer_y);
		image_buffer_x = llfloor(snapshot_width*scale_factor);
		image_buffer_y = llfloor(snapshot_height *scale_factor);
	}
	if (image_buffer_x > 0 && image_buffer_y > 0)
	{
		raw->resize(image_buffer_x, image_buffer_y, 3);
	}
	else
	{
		gSnapshotNoPost = false;
		return false;
	}
	if (raw->isBufferInvalid())
	{
		gSnapshotNoPost = false;
		return false;
	}

	bool high_res = scale_factor > 1.f;
#if 0
	if (high_res)
	{
		LLWorld::sendAgentPause();
		if (show_ui || !hide_hud)
		{
			// Rescale fonts
			initFonts(scale_factor);
			LLHUDText::reshape();
		}
	}
#endif

	S32 output_buffer_offset_y = 0;

	F32 dnear = gViewerCamera.getNear();
	F32 dfar = gViewerCamera.getFar();
	F32 divisor = 2.f * dnear * dfar;
	F32 depth_conv_factor_1 = (dfar + dnear) / divisor;
	F32 depth_conv_factor_2 = (dfar - dnear) / divisor;

	// Sub-images are in fact partial rendering of the final view. This happens
	// when the final view is bigger than the screen. In most common cases,
	// scale_factor is 1 and there is no more than 1 iteration on x and y
	for (S32 subimage_y = 0; subimage_y < scale_factor; ++subimage_y)
	{
		S32 subimage_y_offset =
			llclamp(buffer_y_offset - subimage_y * window_height, 0,
					window_height);
		// Handle fractional columns
		U32 read_height = llmax(0,
								window_height - subimage_y_offset -
						  		llmax(0,
									  window_height * (subimage_y + 1) -
									  buffer_y_offset - raw->getHeight()));

		S32 output_buffer_offset_x = 0;
		for (S32 subimage_x = 0; subimage_x < scale_factor; ++subimage_x)
		{
			gDisplaySwapBuffers = false;
			gDepthDirty = true;

			S32 subimage_x_offset =
				llclamp(buffer_x_offset - subimage_x * window_width, 0,
						window_width);
			// Handle fractional rows
			U32 read_width =
				llmax(0,
					  window_width - subimage_x_offset -
					  llmax(0,
							window_width * (subimage_x + 1) -
							buffer_x_offset - raw->getWidth()));

			// Skip rendering and sampling altogether if either width or height
			// is degenerated to 0 (common in cropping cases)
			if (read_width && read_height)
			{
				const U32 subfield = subimage_x +
									 subimage_y * llceil(scale_factor);
				display(do_rebuild, scale_factor, subfield, true);

				if (!LLPipeline::sRenderDeferred)
				{
					// Required for showing the GUI in snapshots and performing
					// bloom composite overlay. Call even if show_ui is false
					render_ui(scale_factor);
				}

				glFinish();	// Ensure everything got drawn

				for (U32 out_y = 0; out_y < read_height; ++out_y)
				{
					S32 output_buffer_offset =
						raw->getComponents() *
						// iterated y...
						(out_y * raw->getWidth() +
						 // ...plus subimage start in x...
						 window_width * subimage_x +
						 // ...plus subimage start in y...
						 raw->getWidth() * window_height * subimage_y -
						 // ...minus buffer padding x...
						 output_buffer_offset_x -
						 // ...minus buffer padding y...
						 output_buffer_offset_y * raw->getWidth());
					if (type == SNAPSHOT_TYPE_COLOR)
					{
						glReadPixels(subimage_x_offset,
									 out_y + subimage_y_offset, read_width, 1,
									 GL_RGB, GL_UNSIGNED_BYTE,
									 raw->getData() + output_buffer_offset);
					}
					else	// SNAPSHOT_TYPE_DEPTH
					{
						LLPointer<LLImageRaw> depth_line_buffer;
						depth_line_buffer = new LLImageRaw(read_width, 1,
														   // need to store floats
														   sizeof(GL_FLOAT));
						glReadPixels(subimage_x_offset,
									 out_y + subimage_y_offset, read_width, 1,
									 GL_DEPTH_COMPONENT, GL_FLOAT,
									 // current output pixel is beginning
									 // of buffer
									 depth_line_buffer->getData());

						for (S32 i = 0; i < (S32)read_width; ++i)
						{
							F32 depth_float =
								*(F32*)(depth_line_buffer->getData() +
										i * sizeof(F32));

							F32 linear_depth_float =
								1.f / (depth_conv_factor_1 -
									   depth_float * depth_conv_factor_2);
							U8 depth_byte = F32_to_U8(linear_depth_float,
													  dnear, dfar);
							// Write converted scanline out to result image
							for (S32 j = 0, count = raw->getComponents();
								 j < count; ++j)
							{
								*(raw->getData() + output_buffer_offset +
								  i * raw->getComponents() + j) = depth_byte;
							}
						}
					}
				}
			}
			output_buffer_offset_x += subimage_x_offset;
		}
		output_buffer_offset_y += subimage_y_offset;
	}

	gDisplaySwapBuffers = false;
	gDepthDirty = true;
	gSnapshotNoPost = false;

	// Post snapshot
	if (prev_draw_ui &&
		!gPipeline.hasRenderDebugFeatureMask(LLPipeline::RENDER_DEBUG_FEATURE_UI))
	{
		LLPipeline::toggleRenderDebugFeature((void*)LLPipeline::RENDER_DEBUG_FEATURE_UI);
	}

	if (hide_hud)
	{
		LLPipeline::sShowHUDAttachments = true;
	}

#if 0
	if (high_res)
	{
		initFonts(1.f);
		LLHUDObject::reshapeAll();
	}
#endif

	// Pre-pad image to number of pixels such that the line length is a
	// multiple of 4 bytes (for BMP encoding). Note: this formula depends on
	// the number of components being 3. Not obvious, but it's correct.
	image_width += (image_width * 3) % 4;

	bool ret = true;
	// Resize image
	if (abs(image_width - image_buffer_x) > 4 ||
		abs(image_height - image_buffer_y) > 4)
	{
		ret = raw->scale(image_width, image_height);
	}
	else if (image_width != image_buffer_x || image_height != image_buffer_y)
	{
		ret = raw->scale(image_width, image_height, false);
	}

	setCursor(UI_CURSOR_ARROW);

	if (do_rebuild)
	{
		// If we had to do a rebuild, that means that the lists of drawables to
		// be rendered was empty before we started. Need to reset these,
		// otherwise we call state sort on it again when render gets called the
		// next time and we stand a good chance of crashing on rebuild because
		// the render drawable arrays have multiple copies of objects on them.
		gPipeline.resetDrawOrders();
	}

	if (high_res)
	{
		LLWorld::sendAgentResume();
	}

	stop_glerror();

	return ret;
}

void LLViewerWindow::cubeSnapshot(const LLVector3& origin,
								  LLCubeMapArray* cubemapp, S32 face,
								  F32 near_clip, bool dynamic_render)
{
	if (!gUsePBRShaders)
	{
		return;
	}

	LLDisableOcclusionCulling no_occlusion;

	// Store current projection/modelview matrix
	const LLMatrix4a saved_proj = gGLProjection;
	const LLMatrix4a saved_view = gGLModelView;

	U32 res = gPipeline.mRT->mDeferredScreen.getWidth();

	LLViewerCamera saved_camera = gViewerCamera;

	// Camera constants for the square, cube map capture image

	// We must set aspect ratio first to avoid undesirable clamping of vertical
	// FoV.
	gViewerCamera.setAspect(1.f);
	gViewerCamera.setViewNoBroadcast(F_PI_BY_TWO);
	gViewerCamera.yaw(0.f);
	gViewerCamera.setOrigin(origin);
	gViewerCamera.setNear(near_clip);

	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

	static const U32 dynamic_render_types[] =
	{
		LLPipeline::RENDER_TYPE_AVATAR,
		LLPipeline::RENDER_TYPE_PUPPET,
		LLPipeline::RENDER_TYPE_PARTICLES
	};
	static const U32 render_types_count = LL_ARRAY_SIZE(dynamic_render_types);
	bool prev_dynamic_render_type[render_types_count];
	if (!dynamic_render)
	{
		for (U32 i = 0; i < render_types_count; ++i)
		{
			bool enabled = gPipeline.hasRenderType(dynamic_render_types[i]);
			prev_dynamic_render_type[i] = enabled;
			if (enabled)
			{
				gPipeline.toggleRenderType(dynamic_render_types[i]);
			}
		}
	}

	constexpr U32 ui_mask = LLPipeline::RENDER_DEBUG_FEATURE_UI;
	bool draw_ui = gPipeline.hasRenderDebugFeatureMask(ui_mask);
	if (draw_ui)
	{
		LLPipeline::toggleRenderDebugFeature((void*)ui_mask);
	}

	bool show_huds = LLPipeline::sShowHUDAttachments;
	if (show_huds)
	{
		LLPipeline::sShowHUDAttachments = false;
	}

	LLRect window_rect = mWindowRect;
	mWindowRect.set(0, res, res, 0);

	// See LLCubeMapArray::sTargets
	static const LLVector3 look_dirs[6] =
	{
		LLVector3::x_axis,
		LLVector3::x_axis_neg,
		LLVector3::y_axis,
		LLVector3::y_axis_neg,
		LLVector3::z_axis,
		LLVector3::z_axis_neg
	};
	static const LLVector3 look_upvecs[6] =
	{
		LLVector3::y_axis_neg,
		LLVector3::y_axis_neg,
		LLVector3::z_axis,
		LLVector3::z_axis_neg,
		LLVector3::y_axis_neg,
		LLVector3::y_axis_neg
	};
	// Set up camera to look at the right direction
	gViewerCamera.lookDir(look_dirs[face], look_upvecs[face]);

	// Turning this flag off here prohibits the screen swap to present the new
	// frame to the viewer: this avoids a black flash in between captures when
	// the number of render passes is more than 1. We need to also set it here
	// because code in llviewerdisplay.cpp resets it to true each time.
	gDisplaySwapBuffers = false;
	gCubeSnapshot = true;
	display_cube_face();
	gCubeSnapshot = false;
	gDisplaySwapBuffers = true;

	mWindowRect = window_rect;
	setupViewport();

	if (draw_ui && !gPipeline.hasRenderDebugFeatureMask(ui_mask))
	{
		LLPipeline::toggleRenderDebugFeature((void*)ui_mask);
	}

	if (!dynamic_render)
	{
		for (size_t i = 0; i < render_types_count; ++i)
		{
			if (prev_dynamic_render_type[i])
			{
				gPipeline.toggleRenderType(dynamic_render_types[i]);
			}
		}
	}

	if (show_huds)
	{
		LLPipeline::sShowHUDAttachments = true;
	}

	gPipeline.resetDrawOrders();

	gViewerCamera = saved_camera;

	gGLProjection = saved_proj;
	gGLModelView = saved_view;
}

void LLViewerWindow::destroyWindow()
{
	LLWindow::destroyWindow();
}

void LLViewerWindow::drawMouselookInstructions()
{
	// Draw instructions for mouselook ("Press SHIFT ESC to leave Mouselook"
	// in a box at the top of the screen).
	static const LLWString instructions = LLTrans::getWString("mouselook");
	static const LLFontGL* font = LLFontGL::getFontSansSerif();
	constexpr S32 INSTRUCTIONS_PAD = 5;
	static const S32 inst_width = font->getWidth(instructions.c_str()) +
								  2 * INSTRUCTIONS_PAD;
	static const S32 inst_height = font->getLineHeight() +
								   2 * INSTRUCTIONS_PAD;

	static LLCachedControl<U32> fade_ml_exit_tip(gSavedSettings,
												 "FadeMouselookExitTip");
	F32 opaque_time = (F32)fade_ml_exit_tip;
	if (opaque_time != 0.f && opaque_time < 5.f)
	{
		opaque_time = 5.f;
	}
	constexpr F32 INSTRUCTIONS_FADE_TIME = 5.f;

	F32 timer = mMouselookTipFadeTimer.getElapsedTimeF32();

	if (opaque_time && timer >= opaque_time + INSTRUCTIONS_FADE_TIME)
	{
		// Faded out already
		return;
	}

	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

	F32 alpha = 1.f;
	if (opaque_time && timer >= opaque_time)
	{
		// Instructions are fading
		alpha = 1.f - (timer - opaque_time) / INSTRUCTIONS_FADE_TIME;
	}
	gGL.color4f(0.9f, 0.9f, 0.9f, alpha);

	LLRect rect;
	rect.setLeftTopAndSize(INSTRUCTIONS_PAD,
						   getWindowHeight() - INSTRUCTIONS_PAD,
						   inst_width, inst_height);
	gl_rect_2d(rect);

	font->render(instructions, 0,
				 rect.mLeft + INSTRUCTIONS_PAD, rect.mTop - INSTRUCTIONS_PAD,
				 LLColor4(0.f, 0.f, 0.f, alpha),
				 LLFontGL::LEFT, LLFontGL::TOP);
}

void LLViewerWindow::setupViewport(S32 x_offset, S32 y_offset)
{
#if 0	// mWindowRect.mLeft = mWindowRect.mBottom = 0
	gGLViewport[0] = mWindowRect.mLeft + x_offset;
	gGLViewport[1] = mWindowRect.mBottom + y_offset;
	gGLViewport[2] = mWindowRect.getWidth();
	gGLViewport[3] = mWindowRect.getHeight();
#else
	gGLViewport[0] = x_offset;
	gGLViewport[1] = y_offset;
	gGLViewport[2] = mWindowRect.mRight;
	gGLViewport[3] = mWindowRect.mTop;
#endif
	glViewport(gGLViewport[0], gGLViewport[1], gGLViewport[2], gGLViewport[3]);
}

void LLViewerWindow::setup3DRender()
{
	gViewerCamera.setPerspective(NOT_FOR_SELECTION, 0, 0,
								 mWindowRect.getWidth(),
								 mWindowRect.getHeight(), false,
								 gViewerCamera.getNear(), MAX_FAR_CLIP * 2.f);
#if 0	// Redundant
	setupViewport();
#endif
}

void LLViewerWindow::setup2DRender()
{
	gl_state_for_2d(mWindowRect.getWidth(), mWindowRect.getHeight());
#if 0	// Redundant
	setupViewport();
#endif
}

void LLViewerWindow::setShowProgress(bool show)
{
	if (mProgressView)
	{
		mProgressView->setVisible(show);
	}
}

void LLViewerWindow::moveProgressViewToFront()
{
	if (mProgressView && mRootView)
	{
		mRootView->removeChild(mProgressView);
		mRootView->addChild(mProgressView);
	}
}

void LLViewerWindow::setProgressString(const std::string& string)
{
	if (mProgressView)
	{
		mProgressView->setText(string);
	}
}

void LLViewerWindow::setProgressMessage(const std::string& msg)
{
	if (mProgressView)
	{
		mProgressView->setMessage(msg);
	}
}

void LLViewerWindow::setProgressPercent(F32 percent)
{
	if (mProgressView)
	{
		mProgressView->setPercent(percent);
	}
}

void LLViewerWindow::setProgressCancelButtonVisible(bool show,
													const std::string& label)
{
	if (mProgressView)
	{
		mProgressView->setCancelButtonVisible(show, label);
	}
}

void LLViewerWindow::dumpState()
{
	llinfos << "LLViewerWindow active: " << (mActive ? "true": "false")
			<< " - gWindowp visible: "
			<< (gWindowp->getVisible() ? "true": "false")
			<< " - minimized: "
			<< (gWindowp->getMinimized() ? "true": "false") << llendl;
}

// Note: if not necessary, do not change the order of the function calls in
// this function. If you change something, make sure it will not break
// anything; be especially careful to put anything behind
// LLViewerTextureList::destroyGL(save_state)
void LLViewerWindow::stopGL(bool save_state)
{
	if (!gGLManager.mIsDisabled)
	{
		llinfos << "Shutting down GL..." << llendl;

		// *HACK: That flag *MUST* be set before stopping GL and can only be
		// reset after GL is restarted. Else, you will crash because the GL
		// textures will have their size set to weird numbers and/or will be
		// recreated with GL stopped !  HB
		LLImageGL::sPreserveDiscard = true;

		// Pause texture decode threads (will get unpaused during main loop)
		LLAppViewer::pauseTextureFetch();

		gSky.destroyGL();
		stop_glerror();

		LLManipTranslate::destroyGL();
		stop_glerror();

		gBumpImageList.destroyGL();
		stop_glerror();

		LLFontGL::destroyAllGL();
		stop_glerror();

		LLVOAvatar::destroyGL();
		stop_glerror();

		if (gPipeline.isInit())
		{
			gPipeline.destroyGL();
		}

		gBox.cleanupGL();

		LLViewerTextureList::destroyGL(save_state);
		stop_glerror();

		LLImageGL::stopThread();

		gGLManager.mIsDisabled = true;
		stop_glerror();

		while (LLGLSLShader::sInstances.size())
		{
			LLGLSLShader* shader = *(LLGLSLShader::sInstances.begin());
			shader->unload();
		}
		stop_glerror();

		llinfos << "Remaining allocated texture memory: "
				<< LLImageGL::sGlobalTexMemBytes << " bytes." << llendl;
	}
}

// Note: if not necessary, do not change the order of the function calls in
// this function. When changing something, make sure it will not break
// anything. Be especially careful when putting something before
// LLViewerTextureList::restoreGL()
void LLViewerWindow::restoreGL(const std::string& progress_message)
{
	if (gGLManager.mIsDisabled)
	{
		llinfos << "Restoring GL..." << llendl;
		gGLManager.mIsDisabled = false;

		initGLDefaults();
		LLGLState::restoreGL();
		bool aniso = gSavedSettings.getBool("RenderAnisotropic");
		if (LLImageGL::sGlobalUseAnisotropic != aniso)
		{
			LLImageGL::sGlobalUseAnisotropic = aniso;
			LLImageGL::dirtyTexOptions();
		}
		LLImageGL::initThread(gWindowp,
							  gSavedSettings.getS32("GLWorkerThreads"));
		LLViewerTextureList::restoreGL();

#if 0	// For future support of non-square pixels, and fonts that are properly
		// stretched
		LLFontGL::destroyDefaultFonts();
#endif
		initFonts();

		gPipeline.restoreGL();
		gSky.restoreGL();
		LLDrawPoolWater::restoreGL();
		LLManipTranslate::restoreGL();

		gBumpImageList.restoreGL();
		LLVOAvatar::restoreGL();

		gResizeScreenTexture = true;

		if (LLFloaterCustomize::isVisible())
		{
			LLVisualParamHint::requestHintUpdates();
		}

		if (!progress_message.empty())
		{
			gRestoreGLTimer.reset();
			gRestoreGL = true;
			setShowProgress(true);
			setProgressString(progress_message);
		}

		// *HACK: now that GL is restarted, we can reset that flag. HB
		LLImageGL::sPreserveDiscard = false;

		llinfos << "...Restoring GL done" << llendl;
		if (!gAppViewerp->restoreErrorTrap())
		{
			llwarns << "Someone took over my signal/exception handler !"
					<< llendl;
		}
	}
}

void LLViewerWindow::initFonts(F32 zoom_factor)
{
	LLFontGL::destroyAllGL();

	// Initialize with possibly different zoom factor
	LLFontManager::initClass();

	LLFontGL::initClass(gSavedSettings.getF32("FontScreenDPI"),
						mDisplayScale.mV[VX] * zoom_factor,
						mDisplayScale.mV[VY] * zoom_factor,
						LLUICtrlFactory::getXUIPaths());

	// Force font reloads, which can be very slow
	LLFontGL::loadDefaultFonts();

	// Setup custom fonts.
	LLPreviewNotecard::refreshCachedSettings();
	LLPreviewScript::refreshCachedSettings();
}

void LLViewerWindow::getTargetWindow(bool full_screen, U32& width, U32& height)
{
	// Sadly, width and height settings have been historically stored as signed
	// integers, where it does not make any sense... HB
	S32 signed_width, signed_height;
	if (full_screen)
	{
		signed_width = gSavedSettings.getS32("FullScreenWidth");
		signed_height = gSavedSettings.getS32("FullScreenHeight");
	}
	else
	{
		signed_width = gSavedSettings.getS32("WindowWidth");
		signed_height = gSavedSettings.getS32("WindowHeight");
	}
	width = signed_width >= 0 ? (U32)signed_width : 800;
	height = signed_height >= 0 ? (U32)signed_height : 600;
}

void LLViewerWindow::requestResolutionUpdate()
{
	mResDirty = true;
}

bool LLViewerWindow::checkSettings()
{
	if (mStatesDirty)
	{
		gGL.refreshState();
		gViewerShaderMgrp->setShaders();
		mStatesDirty = false;
	}

	// We want to update the resolution AFTER the states getting refreshed not
	// before.
	if (mResDirty)
	{
		if (gSavedSettings.getBool("FullScreenAutoDetectAspectRatio"))
		{
			gWindowp->setNativeAspectRatio(0.f);
		}
		else
		{
			gWindowp->setNativeAspectRatio(gSavedSettings.getF32("FullScreenAspectRatio"));
		}

		reshape(getWindowDisplayWidth(), getWindowDisplayHeight());

		// Force aspect ratio
		if (gWindowp->getFullscreen())
		{
			gViewerCamera.setAspect(getDisplayAspectRatio());
		}

		mResDirty = false;
	}

	return false;
}

void LLViewerWindow::restartDisplay()
{
	llinfos << "Restarting GL" << llendl;
	stopGL();
	if (LLStartUp::isLoggedIn())
	{
		restoreGL("Changing resolution...");
	}
	else
	{
		restoreGL();
		if (LLPanelLogin::getInstance())
		{
			// Force a refresh of the fonts and GL images
			LLPanelLogin::getInstance()->refresh();
		}
	}
}

bool LLViewerWindow::changeDisplaySettings(LLCoordScreen size,
										   bool disable_vsync,
										   bool show_progress_bar)
{
	bool was_maximized = gSavedSettings.getBool("WindowMaximized");
	bool fullscreen = gWindowp->getFullscreen();

	gResizeScreenTexture = true;

	U32 fsaa = gSavedSettings.getU32("RenderFSAASamples");
	U32 old_fsaa = gWindowp->getFSAASamples();
	if (!fullscreen)
	{
		// If not maximized, use the request size
		if (!gWindowp->getMaximized())
		{
			gWindowp->setSize(size);
		}

		if (fsaa == old_fsaa)
		{
			return true;
		}
	}

	// Close floaters that do not handle settings change
	LLFloaterSnapshot::hide(NULL);

	bool result_first_try = false;
	bool result_second_try = false;

	LLFocusableElement* keyboard_focus = gFocusMgr.getKeyboardFocus();
	LLWorld::sendAgentPause();
	llinfos << "Stopping GL during changeDisplaySettings" << llendl;
	stopGL();
	mIgnoreActivate = true;
	LLCoordScreen old_size;
	gWindowp->getSize(&old_size);

	gWindowp->setFSAASamples(fsaa);

	result_first_try = gWindowp->switchContext(fullscreen, size,
											   disable_vsync);
	if (!result_first_try)
	{
		// Try to switch back
		gWindowp->setFSAASamples(old_fsaa);
		result_second_try = gWindowp->switchContext(fullscreen, old_size,
													disable_vsync);
		if (!result_second_try)
		{
			// We are stuck... try once again with a minimal resolution ?
			LLWorld::sendAgentResume();
			mIgnoreActivate = false;
			gFocusMgr.setKeyboardFocus(keyboard_focus);
			return false;
		}
	}
	LLWorld::sendAgentResume();

	llinfos << "Restoring GL during resolution change" << llendl;
	if (show_progress_bar)
	{
		restoreGL("Changing resolution...");
	}
	else
	{
		restoreGL();
	}

	if (!result_first_try)
	{
		LLSD args;
		args["RESX"] = llformat("%d", size.mX);
		args["RESY"] = llformat("%d", size.mY);
		gNotifications.add("ResolutionSwitchFail", args);
		size = old_size;	// For reshape below
	}

	bool success = result_first_try || result_second_try;
	if (success)
	{
#if LL_WINDOWS
		// Only trigger a reshape after switching to fullscreen; otherwise rely
		// on the windows callback (otherwise size is wrong; this is the entire
		// window size, reshape wants the visible window size)
		if (fullscreen && result_first_try)
#endif
		{
			reshape(size.mX, size.mY);
		}
	}

	if (!fullscreen && success)
	{
		// Maximize window if was maximized, else reposition
		if (was_maximized)
		{
			gWindowp->maximize();
		}
		else
		{
			S32 window_x = gSavedSettings.getS32("WindowX");
			S32 window_y = gSavedSettings.getS32("WindowY");
			gWindowp->setPosition(LLCoordScreen(window_x, window_y));
		}
	}

	mIgnoreActivate = false;
	gFocusMgr.setKeyboardFocus(keyboard_focus);

	return success;
}

F32 LLViewerWindow::getDisplayAspectRatio() const
{
	if (gWindowp->getFullscreen())
	{
		if (gSavedSettings.getBool("FullScreenAutoDetectAspectRatio"))
		{
			return gWindowp->getNativeAspectRatio();
		}
		else
		{
			return gSavedSettings.getF32("FullScreenAspectRatio");
		}
	}
	return gWindowp->getNativeAspectRatio();
}

void LLViewerWindow::calcDisplayScale()
{
	F32 ui_scale_factor = gSavedSettings.getF32("UIScaleFactor") *
						  gWindowp->getSystemUISize();
	// HiDPI scaling can be 4x. UI scaling in prefs is up to 2x, so max is 8x
	ui_scale_factor = llclamp(ui_scale_factor, 0.75f, 8.f);
	LLVector2 display_scale;
	display_scale.set(llmax(1.f / gWindowp->getPixelAspectRatio(), 1.f),
					  llmax(gWindowp->getPixelAspectRatio(), 1.f));
	F32 height_normalization = 1.f;
	if (gSavedSettings.getBool("UIAutoScale"))
	{
		height_normalization = (F32)mWindowRect.getHeight() /
							   display_scale.mV[VY] / 768.f;
	}
	if (gWindowp->getFullscreen())
	{
		display_scale *= (ui_scale_factor * height_normalization);
	}
	else
	{
		display_scale *= ui_scale_factor;
	}

	// Limit minimum display scale
	if (display_scale.mV[VX] < MIN_DISPLAY_SCALE ||
		display_scale.mV[VY] < MIN_DISPLAY_SCALE)
	{
		display_scale *= MIN_DISPLAY_SCALE / llmin(display_scale.mV[VX],
												   display_scale.mV[VY]);
	}

	if (gWindowp->getFullscreen())
	{
		display_scale.mV[0] = ll_round(display_scale.mV[0],
									   2.f / (F32)mWindowRect.getWidth());
		display_scale.mV[1] = ll_round(display_scale.mV[1],
									   2.f / (F32)mWindowRect.getHeight());
	}

	if (display_scale != mDisplayScale)
	{
		llinfos << "Setting display scale to " << display_scale << llendl;

		mDisplayScale = display_scale;
		mDisplayScaleDivisor.set(1.f / mDisplayScale.mV[VX],
								 1.f / mDisplayScale.mV[VY]);
		// Init default fonts
		initFonts();
	}
}

S32 LLViewerWindow::getChatConsoleBottomPad()
{
	S32 offset = 0;
	if (gToolBarp && gToolBarp->getVisible())
	{
		offset += TOOL_BAR_HEIGHT;
	}
	return offset;
}

LLRect LLViewerWindow::getChatConsoleRect()
{
	LLRect full_window(0, getWindowHeight(), getWindowWidth(), 0);
	LLRect console_rect = full_window;

	constexpr S32 CONSOLE_PADDING_TOP = 24;
	constexpr S32 CONSOLE_PADDING_BOTTOM = 24;

	console_rect.mTop -= CONSOLE_PADDING_TOP;
	console_rect.mBottom += getChatConsoleBottomPad() + CONSOLE_PADDING_BOTTOM;
	console_rect.mLeft += CONSOLE_PADDING_LEFT;

	static LLCachedControl<bool> chat_full_width(gSavedSettings,
												 "ChatFullWidth");
	if (chat_full_width)
	{
		console_rect.mRight -= CONSOLE_PADDING_RIGHT;
	}
	else
	{
		// Make console rect somewhat narrow so having inventory open is
		// less of a problem.
		console_rect.mRight  = console_rect.mLeft + 2 * getWindowWidth() / 3;
	}

	return console_rect;
}

//static
bool LLViewerWindow::onAlert(const LLSD& notify)
{
	LLNotificationPtr notification =
		gNotifications.find(notify["id"].asUUID());

	// If we are in mouselook, the mouse is hidden and so the user cannot click
	// the dialog buttons. In that case, change to First Person instead.
	if (gAgent.cameraMouselook())
	{
		gAgent.changeCameraToDefault();
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////

LLBottomPanel::LLBottomPanel(const LLRect& rect)
:	LLPanel("bottom panel", rect, false),
	mIndicator(NULL)
{
	// Bottom panel is focus root, so Tab moves through the toolbar and button
	// bar, and overlay
	setFocusRoot(true);
 	// Do not capture mouse clicks that do not hit a child
 	setMouseOpaque(false);
 	setFollows(FOLLOWS_LEFT | FOLLOWS_RIGHT | FOLLOWS_BOTTOM);
	// Flag this panel as chrome so buttons do not grab keyboard focus
	setIsChrome(true);
}

LLBottomPanel::~LLBottomPanel()
{
	gBottomPanelp = NULL;
}

void LLBottomPanel::setFocusIndicator(LLView* indicator)
{
	mIndicator = indicator;
}

void LLBottomPanel::draw()
{
	if (mIndicator)
	{
		bool hasFocus = gFocusMgr.childHasKeyboardFocus(this);
		mIndicator->setVisible(hasFocus);
		mIndicator->setEnabled(hasFocus);
	}
	LLPanel::draw();
}

//
// LLPickInfo
//
LLPickInfo::LLPickInfo()
:	mKeyMask(MASK_NONE),
	mPickCallback(NULL),
	mPickType(PICK_INVALID),
	mWantSurfaceInfo(false),
	mObjectFace(-1),
	mUVCoords(-1.f, -1.f),
	mSTCoords(-1.f, -1.f),
	mXYCoords(-1, -1),
	mIntersection(),
	mNormal(),
	mTangent(),
	mBinormal(),
	mHUDIcon(NULL),
	mPickTransparent(false),
	mPickRigged(false),
	mPickParticle(false)
{
}

LLPickInfo::LLPickInfo(const LLCoordGL& mouse_pos,
					   MASK keyboard_mask,
					   bool pick_transparent,
					   bool pick_rigged,
					   bool pick_particle,
					   bool pick_uv_coords,
					   void (*pick_callback)(const LLPickInfo&))
:	mMousePt(mouse_pos),
	mKeyMask(keyboard_mask),
	mPickCallback(pick_callback),
	mPickType(PICK_INVALID),
	mWantSurfaceInfo(pick_uv_coords),
	mObjectFace(-1),
	mUVCoords(-1.f, -1.f),
	mSTCoords(-1.f, -1.f),
	mXYCoords(-1, -1),
	mNormal(),
	mTangent(),
	mBinormal(),
	mHUDIcon(NULL),
	mPickTransparent(pick_transparent),
	mPickRigged(pick_rigged),
	mPickParticle(pick_particle)
{
}

void LLPickInfo::fetchResults()
{
	static LLVector4a intersection;
	LLHUDIcon* hit_icon =
		gViewerWindowp->cursorIntersectIcon(mMousePt.mX, mMousePt.mY, 512.f,
											&intersection);
	static LLVector4a origin;
	origin.load3(gViewerCamera.getOrigin().mV);

	static LLVector4a delta;
	F32 icon_dist = 0.f;
	if (hit_icon)
	{
		delta.setSub(intersection, origin);
		icon_dist = delta.getLength3().getF32();
	}

	S32 face_hit = -1;
	static LLVector4a normal;
	static LLVector4a tangent;
	static LLVector4a start;
	static LLVector4a end;
	static LLVector4a particle_end;
	static LLVector2 uv;
	LLViewerObject* hit_object =
		gViewerWindowp->cursorIntersect(mMousePt.mX, mMousePt.mY, 512.f, NULL,
										-1, mPickTransparent, mPickRigged,
										&face_hit, &intersection, &uv, &normal,
										&tangent, &start, &end);
	mPickPt = mMousePt;

	U32 te_offset = face_hit > -1 ? face_hit : 0;

	if (mPickParticle)
	{
		// Get the end point of line segment to use for particle raycast
		if (hit_object)
		{
			particle_end = intersection;
		}
		else
		{
			particle_end = end;
		}
	}

	// Un-project relative clicked coordinate from window coordinate using GL

	LLViewerObject* objectp = hit_object;

	delta.setSub(origin, intersection);
	if (hit_icon && (!objectp || icon_dist < delta.getLength3().getF32()))
	{
		// Was this name referring to a hud icon ?
		mHUDIcon = hit_icon;
		mPickType = PICK_ICON;
		mPosGlobal = mHUDIcon->getPositionGlobal();
	}
	else if (objectp)
	{
		if (objectp->getPCode() == LLViewerObject::LL_VO_SURFACE_PATCH)
		{
			// Hit land
			mPickType = PICK_LAND;
			mObjectID.setNull(); // land has no id

			// put global position into land_pos
			static LLVector3d land_pos;
			if (!gViewerWindowp->mousePointOnLandGlobal(mPickPt.mX, mPickPt.mY,
														&land_pos))
			{
				// The selected point is beyond the draw distance or is
				// otherwise not selectable. Return before calling
				// mPickCallback().
				return;
			}

			// Fudge the land focus a little bit above ground.
			mPosGlobal = land_pos + LLVector3d::z_axis * 0.1f;
		}
		else
		{
			if (isFlora(objectp))
			{
				mPickType = PICK_FLORA;
			}
			else
			{
				mPickType = PICK_OBJECT;
			}

			LLVector3 v_intersection(intersection.getF32ptr());

			mObjectOffset = gAgent.calcFocusOffset(objectp, v_intersection,
												   mPickPt.mX, mPickPt.mY);
			mObjectID = objectp->mID;
			mObjectFace = te_offset == NO_FACE ? -1 : (S32)te_offset;

			mPosGlobal = gAgent.getPosGlobalFromAgent(v_intersection);

			if (mWantSurfaceInfo)
			{
				getSurfaceInfo();
			}
		}
	}

	if (mPickParticle)
	{
		// Search for closest particle to click origin out to intersection
		// point
		S32 part_face = -1;
		LLVOPartGroup* group =
			gPipeline.lineSegmentIntersectParticle(start, particle_end, NULL,
												   &part_face);
		if (group)
		{
			mParticleOwnerID = group->getPartOwner(part_face);
			mParticleSourceID = group->getPartSource(part_face);
		}
	}

	if (mPickCallback)
	{
		mPickCallback(*this);
	}
}

LLPointer<LLViewerObject> LLPickInfo::getObject() const
{
	return gObjectList.findObject(mObjectID);
}

void LLPickInfo::updateXYCoords()
{
	if (mObjectFace > -1)
	{
		const LLTextureEntry* tep = getObject()->getTE(mObjectFace);
		if (!tep) return;

		LLPointer<LLViewerTexture> imagep =
			LLViewerTextureManager::getFetchedTexture(tep->getID());
		if (imagep.notNull() && mUVCoords.mV[VX] >= 0.f &&
			mUVCoords.mV[VY] >= 0.f)
		{
			mXYCoords.mX = ll_round(mUVCoords.mV[VX] *
									(F32)imagep->getWidth());
			mXYCoords.mY = ll_round((1.f - mUVCoords.mV[VY]) *
									(F32)imagep->getHeight());
		}
	}
}

void LLPickInfo::getSurfaceInfo()
{
	// Set values to uninitialized: this is what we return if no intersection
	// is found
	mObjectFace = -1;
	mUVCoords = LLVector2(-1.f, -1.f);
	mSTCoords = LLVector2(-1.f, -1.f);
	mXYCoords = LLCoordScreen(-1.f, -1.f);
	mIntersection.setZero();
	mNormal.setZero();
	mBinormal.setZero();
	mTangent.setZero();

	LLViewerObject* objectp = getObject();
	if (!objectp) return;

	static LLVector4a tangent;
	static LLVector4a intersection;
	static LLVector4a normal;
	tangent.clear();
	normal.clear();
	intersection.clear();
	if (gViewerWindowp->cursorIntersect(ll_round((F32)mMousePt.mX),
										ll_round((F32)mMousePt.mY), 1024.f,
										objectp, -1, mPickTransparent,
										mPickRigged, &mObjectFace,
										&intersection, &mSTCoords, &normal,
										&tangent))
	{
		// If we succeeded with the intersect above, compute the texture
		// coordinates:
		if (objectp->mDrawable.notNull() && mObjectFace > -1)
		{
			LLFace* facep = objectp->mDrawable->getFace(mObjectFace);
			if (facep)
			{
				mUVCoords = facep->surfaceToTexture(mSTCoords, intersection,
													normal);
			}
		}

		mIntersection.set(intersection.getF32ptr());
		mNormal.set(normal.getF32ptr());
		mTangent.set(tangent.getF32ptr());

		// Extrapoloate binormal from normal and tangent
		static LLVector4a binormal;
		binormal.setCross3(normal, tangent);
		binormal.mul(tangent.getF32ptr()[3]);
		mBinormal.set(binormal.getF32ptr());

		mBinormal.normalize();
		mNormal.normalize();
		mTangent.normalize();

		// and XY coords:
		updateXYCoords();
	}
}

//static
bool LLPickInfo::isFlora(LLViewerObject* object)
{
	if (!object) return false;

	LLPCode pcode = object->getPCode();
	return pcode == LL_PCODE_LEGACY_GRASS || pcode == LL_PCODE_LEGACY_TREE;
}

///////////////////////////////////////////////////////////////////////////////
// HBTempWindowTitle class
///////////////////////////////////////////////////////////////////////////////

HBTempWindowTitle::HBTempWindowTitle(const std::string& message)
{
	if (gWindowp && !message.empty())
	{
		std::string title = gSecondLife + " - " + message;
		LLStringUtil::truncate(title, 255);
		gWindowp->setWindowTitle(title);
	}
}

HBTempWindowTitle::~HBTempWindowTitle()
{
	if (gWindowp)
	{
		gWindowp->setWindowTitle(gWindowTitle);
	}
}
