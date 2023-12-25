/**
 * @file llviewerdisplay.h
 * @brief LLViewerDisplay class header file
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

#ifndef LL_LLVIEWERDISPLAY_H
#define LL_LLVIEWERDISPLAY_H

#include "llfontgl.h"
#include "llmatrix4a.h"

void display_startup();
void display_cleanup();

void display(bool rebuild = true, F32 zoom_factor = 1.f, S32 subfield = 0,
			 bool for_snapshot = false);

void display_update_camera();	// Also called from LLPipeline

// Uses whole screen to render hud:
bool setup_hud_matrices();

// Specifies portion of screen (in pixels) to render hud attachments from (for
// picking):
bool setup_hud_matrices(const LLRect& screen_region);

bool get_hud_matrices(LLMatrix4a& proj, LLMatrix4a& model);
bool get_hud_matrices(const LLRect& screen_region,
					  LLMatrix4a& proj, LLMatrix4a& model);

// Utility function for rendering HUD elements
void hud_render_text(const LLWString& wstr, const LLVector3& pos_agent,
					 const LLFontGL& font, U8 style,
					 F32 x_offset, F32 y_offset, const LLColor4& color,
					 bool orthographic);

// Also used in llviewerwindow.cpp for snapshots
void render_ui(F32 zoom_factor = 1.f);

// Used by LLViewerWindow::cubeSnapshot() for PBR rendering only.
void display_cube_face();

extern bool gDisplaySwapBuffers;
extern bool gDepthDirty;
extern bool	gTeleportDisplay;
extern LLFrameTimer	gTeleportDisplayTimer;
extern bool gForceRenderLandFence;
extern bool gResizeScreenTexture;
extern bool gResizeShadowTexture;
extern F32  gSavedDrawDistance;
extern bool gUpdateDrawDistance;
extern U32	gLastFPSAverage;
extern bool gShaderProfileFrame;
extern bool gScreenIsDirty;
// IMPORTANT: this MUST always be false while in EE rendering mode. HB
extern bool gCubeSnapshot;

#endif // LL_LLVIEWERDISPLAY_H
