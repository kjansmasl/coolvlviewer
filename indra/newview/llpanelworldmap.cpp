/**
 * @file llpanelworldmap.cpp
 * @brief LLPanelWorldMap class implementation
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 *
 * Copyright (c) 2001-2009, Linden Research, Inc.
 * Copyright (c) 2009-2021, Henri Beauchamp.
 * Changes by Henri Beauchamp:
 *  - Reorganized (was LLWorldMapView), cleaned up and optimized the code.
 *  - Backported web map tiles support from v2/3 viewers while keeping the old
 *    terrain-only map support.
 *  - Adapted the code for OpenSim variable region size support.
 *  - Allowed to keep both objects and terrain map tiles in memory (avoids
 *    seeing the map tiles fully reloaded at each world map tab change).
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

#include "llpanelworldmap.h"

#include "llgl.h"
#include "llregionhandle.h"
#include "llrender.h"
#include "lltextbox.h"
#include "lltrans.h"

#include "llagent.h"
#include "llappviewer.h"			// Only for constants !
#include "llavatartracker.h"
#include "hbfloatersearch.h"
#include "llfloaterworldmap.h"
#include "lltexturefetch.h"
#include "lltracker.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewermenu.h"
#include "llviewerparceloverlay.h"
#include "llviewerregion.h"
#include "llviewertexturelist.h"
#include "llviewerwindow.h"
#include "llworldmap.h"

constexpr F32 GODLY_TELEPORT_HEIGHT = 200.f;
constexpr F32 BIG_DOT_RADIUS = 5.f;

F32 LLPanelWorldMap::sDefaultZ = -1.f;
bool LLPanelWorldMap::sHandledLastClick = false;

LLUIImagePtr LLPanelWorldMap::sAvatarSmallImage = NULL;
LLUIImagePtr LLPanelWorldMap::sAvatarYouImage = NULL;
LLUIImagePtr LLPanelWorldMap::sAvatarYouLargeImage = NULL;
LLUIImagePtr LLPanelWorldMap::sAvatarLevelImage = NULL;
LLUIImagePtr LLPanelWorldMap::sAvatarAboveImage = NULL;
LLUIImagePtr LLPanelWorldMap::sAvatarBelowImage = NULL;

LLUIImagePtr LLPanelWorldMap::sTelehubImage = NULL;
LLUIImagePtr LLPanelWorldMap::sInfohubImage = NULL;
LLUIImagePtr LLPanelWorldMap::sHomeImage = NULL;
LLUIImagePtr LLPanelWorldMap::sEventImage = NULL;
LLUIImagePtr LLPanelWorldMap::sEventMatureImage = NULL;
LLUIImagePtr LLPanelWorldMap::sEventAdultImage = NULL;

LLUIImagePtr LLPanelWorldMap::sTrackCircleImage = NULL;
LLUIImagePtr LLPanelWorldMap::sTrackArrowImage = NULL;

LLUIImagePtr LLPanelWorldMap::sClassifiedsImage = NULL;
LLUIImagePtr LLPanelWorldMap::sForSaleImage = NULL;
LLUIImagePtr LLPanelWorldMap::sForSaleAdultImage = NULL;

F32 LLPanelWorldMap::sThreshold = 96.f;
F32 LLPanelWorldMap::sPanX = 0.f;
F32 LLPanelWorldMap::sPanY = 0.f;
F32 LLPanelWorldMap::sTargetPanX = 0.f;
F32 LLPanelWorldMap::sTargetPanY = 0.f;
S32 LLPanelWorldMap::sTrackingArrowX = 0;
S32 LLPanelWorldMap::sTrackingArrowY = 0;
F32 LLPanelWorldMap::sPixelsPerMeter = 1.f;
F32 LLPanelWorldMap::sMapScale = 128.f;
F32 CONE_SIZE = 0.6f;

// Width in pixels, where we start drawing "null" sims
#define SIM_NULL_MAP_SCALE 1.f
// Width in pixels, where we start drawing agents
#define SIM_MAP_AGENT_SCALE 2.f
// Width in pixels, where we start drawing sim tiles
#define SIM_MAP_SCALE 1.f

// Updates for agent locations.
#define AGENTS_UPDATE_TIME 60.0 // in seconds
#define AGENTS_FAST_UPDATE_TIME 5.0 // in seconds

// Helper function
LL_INLINE bool is_agent_in_region(LLViewerRegion* region, LLSimInfo* info)
{
	return region && info && info->mName == region->getName();
}

///////////////////////////////////////////////////////////////////////////////

void LLPanelWorldMap::initClass()
{
	sAvatarSmallImage = LLUI::getUIImage("map_avatar_8.tga");
	sAvatarYouImage = LLUI::getUIImage("map_avatar_16.tga");
	sAvatarYouLargeImage = LLUI::getUIImage("map_avatar_you_32.tga");
	sAvatarLevelImage = LLUI::getUIImage("map_avatar_32.tga");
	sAvatarAboveImage = LLUI::getUIImage("map_avatar_above_32.tga");
	sAvatarBelowImage = LLUI::getUIImage("map_avatar_below_32.tga");

	sHomeImage = LLUI::getUIImage("map_home.tga");
	sTelehubImage = LLUI::getUIImage("map_telehub.tga");
	sInfohubImage = LLUI::getUIImage("map_infohub.tga");
	sEventImage = LLUI::getUIImage("map_event.tga");
	sEventMatureImage = LLUI::getUIImage("map_event_mature.tga");
	// To Do: update the image resource for adult events.
	sEventAdultImage = LLUI::getUIImage("map_event_adult.tga");

	sTrackCircleImage = LLUI::getUIImage("map_track_16.tga");
	sTrackArrowImage = LLUI::getUIImage("direction_arrow.tga");
	sClassifiedsImage = LLUI::getUIImage("icon_top_pick.tga");
	sForSaleImage = LLUI::getUIImage("icon_for_sale.tga");
	// To Do: update the image resource for adult lands on sale.
	sForSaleAdultImage = LLUI::getUIImage("icon_for_sale_adult.tga");
}

//static
void LLPanelWorldMap::cleanupClass()
{
	sAvatarSmallImage = sAvatarYouImage = sAvatarYouLargeImage = NULL;
	sAvatarLevelImage = sAvatarAboveImage = sAvatarBelowImage = NULL;
	sTelehubImage = sInfohubImage = sHomeImage = NULL;
	sEventImage = sEventMatureImage = sEventAdultImage = NULL;
	sTrackCircleImage = sTrackArrowImage = NULL;
	sClassifiedsImage = sForSaleImage = sForSaleAdultImage = NULL;
}

LLPanelWorldMap::LLPanelWorldMap(const std::string& name, const LLRect& rect,
								 U32 layer)
:	LLPanel(name, rect, BORDER_NO),
	mLayer(layer),
	mBackgroundColor(LLColor4(4.f / 255.f, 4.f / 255.f, 75.f / 255.f, 1.f)),
	mItemPicked(false),
	mPanning(false),
	mMouseDownPanX(0),
	mMouseDownPanY(0),
	mMouseDownX(0),
	mMouseDownY(0),
	mSelectIDStart(0)
{
	sDefaultZ = -1.f;	// Reset default altitude

	sPixelsPerMeter = sMapScale / REGION_WIDTH_METERS;
	clearLastClick();

	constexpr S32 DIR_WIDTH = 10;
	constexpr S32 DIR_HEIGHT = 10;
	LLRect major_dir_rect(0, DIR_HEIGHT, DIR_WIDTH, 0);

	mTextBoxNorth = new LLTextBox("N", major_dir_rect);
	addChild(mTextBoxNorth);

	LLColor4 minor_color(1.f, 1.f, 1.f, .7f);

	mTextBoxEast =	new LLTextBox("E", major_dir_rect);
	mTextBoxEast->setColor(minor_color);
	addChild(mTextBoxEast);

	major_dir_rect.mRight += 1 ;
	mTextBoxWest =	new LLTextBox("W", major_dir_rect);
	mTextBoxWest->setColor(minor_color);
	addChild(mTextBoxWest);
	major_dir_rect.mRight -= 1 ;

	mTextBoxSouth = new LLTextBox("S", major_dir_rect);
	mTextBoxSouth->setColor(minor_color);
	addChild(mTextBoxSouth);

	LLRect minor_dir_rect( 0, DIR_HEIGHT, DIR_WIDTH * 2, 0);

	mTextBoxSouthEast =	new LLTextBox("SE", minor_dir_rect);
	mTextBoxSouthEast->setColor(minor_color);
	addChild(mTextBoxSouthEast);

	mTextBoxNorthEast = new LLTextBox("NE", minor_dir_rect);
	mTextBoxNorthEast->setColor(minor_color);
	addChild(mTextBoxNorthEast);

	mTextBoxSouthWest =	new LLTextBox("SW", minor_dir_rect);
	mTextBoxSouthWest->setColor(minor_color);
	addChild(mTextBoxSouthWest);

	mTextBoxNorthWest = new LLTextBox("NW", minor_dir_rect);
	mTextBoxNorthWest->setColor(minor_color);
	addChild(mTextBoxNorthWest);
}

//static
void LLPanelWorldMap::setScale(F32 scale)
{
	if (scale != sMapScale)
	{
		F32 old_scale = sMapScale;
		sMapScale = llmax(scale, 0.1f);

		F32 ratio = sMapScale / old_scale;
		sPanX *= ratio;
		sPanY *= ratio;
		sTargetPanX = sPanX;
		sTargetPanY = sPanY;

		sPixelsPerMeter = sMapScale / REGION_WIDTH_METERS;
	}
}

//static
void LLPanelWorldMap::setPan(S32 x, S32 y, bool snap)
{
	sTargetPanX = (F32)x;
	sTargetPanY = (F32)y;
	if (snap)
	{
		sPanX = sTargetPanX;
		sPanY = sTargetPanY;
	}
}

void LLPanelWorldMap::draw()
{
	F64 current_time = LLTimer::getElapsedSeconds();

	mVisibleRegions.clear();

	// Animate pan if necessary
	F32 critical_damp = LLCriticalDamp::getInterpolant(0.1f);
	sPanX = lerp(sPanX, sTargetPanX, critical_damp);
	sPanY = lerp(sPanY, sTargetPanY, critical_damp);

	const S32 width = getRect().getWidth();
	const S32 height = getRect().getHeight();
	const F32 half_width = F32(width) * 0.5f;
	const F32 half_height = F32(height) * 0.5f;
	LLVector3d camera_global = gAgent.getCameraPositionGlobal();


	LLLocalClipRect clip(getLocalRect());

	LLTexUnit* unit0 = gGL.getTexUnit(0);
	unit0->unbind(LLTexUnit::TT_TEXTURE);

	gGL.matrixMode(LLRender::MM_MODELVIEW);

	if (gUsePBRShaders)
	{
		// Draw background rectangle
#if 0	// Already done above... Why again ?  HB
		unit0->unbind(LLTexUnit::TT_TEXTURE);
#endif
		gGL.color4fv(mBackgroundColor.mV);
		gl_rect_2d(0, height, width, 0);
	}
	else
	{
		// Clear the background alpha to 0
		gGL.setColorMask(false, true);
		gGL.flush();
		gGL.setSceneBlendType(LLRender::BT_REPLACE);
		gGL.color4f(0.f, 0.f, 0.f, 0.f);
		gl_rect_2d(0, height, width, 0);

		gGL.setColorMask(true, true);
		gGL.flush();
		gGL.setSceneBlendType(LLRender::BT_ALPHA);
	}

	F32 layer_alpha = 1.f;
	F32 ui_scale_x = LLUI::sGLScaleFactor.mV[VX];
	F32 ui_scale_y = LLUI::sGLScaleFactor.mV[VY];
	// Draw one image per layer
	for (U32 layer_idx = 0, count = gWorldMap.mMapLayers[mLayer].size();
		 layer_idx < count; ++layer_idx)
	{
		if (!gWorldMap.mMapLayers[mLayer][layer_idx].mLayerDefined)
		{
			continue;
		}
		LLWorldMapLayer* layerp = &gWorldMap.mMapLayers[mLayer][layer_idx];
		LLViewerTexture* curr_texp = layerp->mLayerImage;
		if (!curr_texp || curr_texp->isMissingAsset())
		{
			continue; // Better to draw nothing than the missing asset image
		}

		LLVector3d origin_global((F64)layerp->mLayerExtents.mLeft *
								 REGION_WIDTH_METERS,
								 (F64)layerp->mLayerExtents.mBottom *
								 REGION_WIDTH_METERS,
								 0.f);

		// Find x and y position relative to the center of the camera.
		LLVector3d rel_region_pos = origin_global - camera_global;
		F32 relative_x = rel_region_pos.mdV[0] / REGION_WIDTH_METERS *
						 sMapScale;
		F32 relative_y = rel_region_pos.mdV[1] / REGION_WIDTH_METERS *
						 sMapScale;

		F32 pix_width = sMapScale * (layerp->mLayerExtents.getWidth() + 1);
		F32 pix_height = sMapScale * (layerp->mLayerExtents.getHeight() + 1);

		// When the view is not panned, 0,0 = center of rectangle
		F32 bottom = sPanY + half_height + relative_y;
		F32 left = sPanX + half_width + relative_x;
		F32 top = bottom + pix_height;
		F32 right = left + pix_width;
		F32 pixel_area = pix_width * pix_height;
		// Discard small layers and layers that are outside the rectangle
		if (top < 0.f || bottom > height || right < 0.f || left > width ||
			pixel_area < 16)
		{
			curr_texp->setBoostLevel(0);
			continue;
		}

		curr_texp->setBoostLevel(LLGLTexture::BOOST_MAP);
		curr_texp->setKnownDrawSize(ll_roundp(pix_width * ui_scale_x),
									ll_roundp(pix_height * ui_scale_y));

		if (!curr_texp->hasGLTexture())
		{
			continue; // Better to draw nothing than the default image
		}

		// Draw using the texture. Not clamping would cause artifacts at the
		// edge.
		unit0->bind(curr_texp);

		// Draw map image into RGB
		gGL.setColorMask(true, false);
		gGL.color4f(1.f, 1.f, 1.f, layer_alpha);

		gGL.begin(LLRender::TRIANGLES);
			gGL.texCoord2f(0.f, 1.f);
			gGL.vertex3f(left, top, -1.f);
			gGL.texCoord2f(0.f, 0.f);
			gGL.vertex3f(left, bottom, -1.f);
			gGL.texCoord2f(1.f, 0.f);
			gGL.vertex3f(right, bottom, -1.f);
			gGL.texCoord2f(0.f, 1.f);
			gGL.vertex3f(left, top, -1.f);
			gGL.texCoord2f(1.f, 0.f);
			gGL.vertex3f(right, bottom, -1.f);
			gGL.texCoord2f(1.f, 1.f);
			gGL.vertex3f(right, top, -1.f);
		gGL.end();

		// Draw an alpha of 1 where the sims are visible
		gGL.setColorMask(false, true);
		gGL.color4f(1.f, 1.f, 1.f, 1.f);

		gGL.begin(LLRender::TRIANGLES);
			gGL.texCoord2f(0.f, 1.f);
			gGL.vertex2f(left, top);
			gGL.texCoord2f(0.f, 0.f);
			gGL.vertex2f(left, bottom);
			gGL.texCoord2f(1.f, 0.f);
			gGL.vertex2f(right, bottom);
			gGL.texCoord2f(0.f, 1.f);
			gGL.vertex2f(left, top);
			gGL.texCoord2f(1.f, 0.f);
			gGL.vertex2f(right, bottom);
			gGL.texCoord2f(1.f, 1.f);
			gGL.vertex2f(right, top);
		gGL.end();
	}

	gGL.flush();
	gGL.setColorMask(true, true);

	// Draw one image per region, centered on the camera position.
	constexpr U32 MAX_SIMULTANEOUS_TEX = 100;
	constexpr U32 MAX_REQUEST_PER_TICK = 5;
	constexpr U32 MIN_REQUEST_PER_TICK = 1;
	U32 textures_requested_this_tick = 0;

	bool use_web_map_tiles = LLWorldMap::useWebMapTiles();

	static LLCachedControl<bool> map_show_land_for_sale(gSavedSettings,
														"MapShowLandForSale");
	static LLFontGL* font = LLFontGL::getFontSansSerifSmall();
	critical_damp = LLCriticalDamp::getInterpolant(0.15f);
	for (LLWorldMap::sim_info_map_t::iterator
			it = gWorldMap.mSimInfoMap.begin(),
			end = gWorldMap.mSimInfoMap.end();
		 it != end; ++it)
	{
		U64 handle = it->first;
		LLSimInfo* info = it->second;

		LLViewerTexture* simtexp = info->mCurrentImage[mLayer];
		LLViewerTexture* overlaytexp = info->mOverlayImage;

		if (sMapScale < SIM_MAP_SCALE)
		{
			if (simtexp)
			{
				simtexp->setBoostLevel(0);
			}
			if (overlaytexp)
			{
				overlaytexp->setBoostLevel(0);
			}
			continue;
		}

		LLVector3d origin_global = from_region_handle(handle);

		// Find x and y position relative to camera's center.
		LLVector3d rel_region_pos = origin_global - camera_global;
		F32 relative_x = (rel_region_pos.mdV[0] / REGION_WIDTH_METERS) *
						 sMapScale;
		F32 relative_y = (rel_region_pos.mdV[1] / REGION_WIDTH_METERS) *
						 sMapScale;

		// When the view is not panned, 0,0 = center of rectangle
		F32 bottom = sPanY + half_height + relative_y;
		F32 left = sPanX + half_width + relative_x;
		// Variable region size support: sMapScale is further scaled.
		F32 top = bottom + sMapScale * (info->mSizeY / REGION_WIDTH_METERS);
		F32 right = left + sMapScale * (info->mSizeX / REGION_WIDTH_METERS);

		// Switch to world map texture (if available for this region) if either:
		// 1. Tiles are zoomed out small enough, or
		// 2. Sim's texture has not been loaded yet
		F32 map_scale_cutoff = SIM_MAP_SCALE;
#if 0	// REGION_FLAGS_NULL_LAYER is deprecated in SL and reused by
		// REGION_FLAGS_ALLOW_ENVIRONMENT_OVERRIDE...
		if ((info->mRegionFlags & REGION_FLAGS_NULL_LAYER) > 0)
		{
			map_scale_cutoff = SIM_NULL_MAP_SCALE;
		}
#endif
		info->mShowAgentLocations = (sMapScale >= SIM_MAP_AGENT_SCALE);

		bool sim_visible = (sMapScale >= map_scale_cutoff &&
							simtexp && simtexp->hasGLTexture());

		if (sim_visible)
		{
			// Fade in
			if (info->mAlpha < 0.f)
			{
				info->mAlpha = 1.f; // Do not fade initially
			}
			else
			{
				info->mAlpha = lerp(info->mAlpha, 1.f, critical_damp);
			}
		}
		// Fade out
		else if (info->mAlpha < 0.f)
		{
			info->mAlpha = 0.f; // Do not fade initially
		}
		else
		{
			info->mAlpha = lerp(info->mAlpha, 0.f, critical_damp);
		}

		// Discard regions that are outside the rectangle and discard small
		// regions
		if (top < 0.f || bottom > height || right < 0.f || left > width)
		{
			if (simtexp)
			{
				simtexp->setBoostLevel(0);
			}
			if (overlaytexp)
			{
				overlaytexp->setBoostLevel(0);
			}
			continue;
		}

		if (!simtexp &&
			(textures_requested_this_tick < MIN_REQUEST_PER_TICK ||
			 (textures_requested_this_tick < MAX_REQUEST_PER_TICK &&
			  gTextureFetchp->getApproxNumRequests() < MAX_SIMULTANEOUS_TEX)))
		{
			++textures_requested_this_tick;
			if (use_web_map_tiles)
			{
				LLVector3d region_pos = info->getGlobalOrigin();
				info->mCurrentImage[mLayer] =
					LLWorldMap::loadObjectsTile((U32)(region_pos.mdV[VX] /
													  REGION_WIDTH_UNITS),
													  (U32)(region_pos.mdV[VY] /
															REGION_WIDTH_UNITS));
			}
			else
			{
				info->mCurrentImage[mLayer] =
					LLViewerTextureManager::getFetchedTexture(info->mMapImageID[mLayer],
															  FTT_MAP_TILE);
			}
			simtexp = info->mCurrentImage[mLayer];
		}
		if (!overlaytexp && info->mMapImageID[2].notNull() &&
			(textures_requested_this_tick < MIN_REQUEST_PER_TICK ||
			 (textures_requested_this_tick < MAX_REQUEST_PER_TICK &&
			  gTextureFetchp->getApproxNumRequests() < MAX_SIMULTANEOUS_TEX)))
		{
			++textures_requested_this_tick;
			info->mOverlayImage =
				LLViewerTextureManager::getFetchedTexture(info->mMapImageID[2],
														  FTT_MAP_TILE);
			overlaytexp = info->mOverlayImage;
		}

		mVisibleRegions.push_back(handle);
		// See if the agents need updating
		F64 delta = current_time - info->mAgentsUpdateTime;
		if (delta > AGENTS_UPDATE_TIME || 
			// In case of TP failure, increase the update rate
			(delta > AGENTS_FAST_UPDATE_TIME && 
			 handle == gAgent.getTeleportedSimHandle()))
		{
			info->mAgentsUpdateTime = current_time;
			if (info->mAccess == SIM_ACCESS_DOWN)
			{
				gWorldMap.sendHandleRegionRequest(handle);
			}
			else
			{
				gWorldMap.sendItemRequest(MAP_ITEM_AGENT_LOCATIONS,
										  handle);
			}
		}

		// Bias the priority escalation for images nearer

		// Variable region size support: draw_size = ll_roundp(sMapScale) is
		// replaced with scaled x_draw_size and y_draw_size
		S32 x_draw_size = ll_roundp(sMapScale * info->mSizeX /
									REGION_WIDTH_METERS);
		S32 y_draw_size = ll_roundp(sMapScale * info->mSizeY /
									REGION_WIDTH_METERS);
		if (simtexp)
		{
			simtexp->setBoostLevel(LLGLTexture::BOOST_MAP);
			simtexp->setKnownDrawSize(ll_roundp(x_draw_size * ui_scale_x),
									   ll_roundp(y_draw_size * ui_scale_y));
		}

		if (overlaytexp)
		{
			overlaytexp->setBoostLevel(LLGLTexture::BOOST_MAP);
			overlaytexp->setKnownDrawSize(ll_roundp(x_draw_size * ui_scale_x),
										  ll_roundp(y_draw_size * ui_scale_y));
		}

		if (sim_visible && info->mAlpha > 0.001f)
		{
			// Draw using the texture. Not clamping would cause artifacts at
			// the edges.
			LLGLSUIDefault gls_ui;
			if (!gUsePBRShaders)
			{
				gGL.setSceneBlendType(LLRender::BT_ALPHA);
			}
			F32 alpha = info->mAlpha;
			if (simtexp && simtexp->hasGLTexture())
			{
				unit0->bind(simtexp);
				simtexp->setAddressMode(LLTexUnit::TAM_CLAMP);
				gGL.color4f(1.f, 1.f, 1.f, alpha);
				gGL.begin(LLRender::TRIANGLES);
					gGL.texCoord2f(0.f, 1.f);
					gGL.vertex3f(left, top, 0.f);
					gGL.texCoord2f(0.f, 0.f);
					gGL.vertex3f(left, bottom, 0.f);
					gGL.texCoord2f(1.f, 0.f);
					gGL.vertex3f(right, bottom, 0.f);
					gGL.texCoord2f(0.f, 1.f);
					gGL.vertex3f(left, top, 0.f);
					gGL.texCoord2f(1.f, 0.f);
					gGL.vertex3f(right, bottom, 0.f);
					gGL.texCoord2f(1.f, 1.f);
					gGL.vertex3f(right, top, 0.f);
				gGL.end();
			}

			if (map_show_land_for_sale && overlaytexp &&
				overlaytexp->hasGLTexture())
			{
				unit0->bind(overlaytexp);
				overlaytexp->setAddressMode(LLTexUnit::TAM_CLAMP);
				gGL.color4f(1.f, 1.f, 1.f, alpha);
				gGL.begin(LLRender::TRIANGLES);
					gGL.texCoord2f(0.f, 1.f);
					gGL.vertex3f(left, top, -0.5f);
					gGL.texCoord2f(0.f, 0.f);
					gGL.vertex3f(left, bottom, -0.5f);
					gGL.texCoord2f(1.f, 0.f);
					gGL.vertex3f(right, bottom, -0.5f);
					gGL.texCoord2f(0.f, 1.f);
					gGL.vertex3f(left, top, -0.5f);
					gGL.texCoord2f(1.f, 0.f);
					gGL.vertex3f(right, bottom, -0.5f);
					gGL.texCoord2f(1.f, 1.f);
					gGL.vertex3f(right, top, -0.5f);
				gGL.end();
			}
		}

		if (info->mAccess == SIM_ACCESS_DOWN)
		{
			// Draw a transparent red square over down sims
			if (!gUsePBRShaders)
			{
				gGL.blendFunc(LLRender::BF_DEST_ALPHA,
							  LLRender::BF_SOURCE_ALPHA);
			}
			gGL.color4f(0.2f, 0.f, 0.f, 0.4f);
			unit0->unbind(LLTexUnit::TT_TEXTURE);
			gGL.begin(LLRender::TRIANGLES);
				gGL.vertex2f(left, top);
				gGL.vertex2f(left, bottom);
				gGL.vertex2f(right, bottom);
				gGL.vertex2f(left, top);
				gGL.vertex2f(right, bottom);
				gGL.vertex2f(right, top);
			gGL.end();
			if (!gUsePBRShaders)
			{
				gGL.blendFunc(LLRender::BF_SOURCE_ALPHA,
							  LLRender::BF_ONE_MINUS_SOURCE_ALPHA);
			}
		}

		// Draw the region name in the lower left corner
		std::string mesg;
		if (sMapScale >= sThreshold)
		{
			static const std::string offline =
				LLTrans::getString("worldmap_offline");
			std::string access;
			switch (info->mAccess)
			{
				case SIM_ACCESS_DOWN:
					access = offline;
					break;
				case SIM_ACCESS_PG:
					access = "PG";
					break;
				case SIM_ACCESS_MATURE:
					access = "M";
					break;
				case SIM_ACCESS_ADULT:
					access = "A";
					break;
				default:
					break;
			}
			if (access.empty())
			{
				mesg = info->mName;
			}
			else
			{
				mesg = info->mName + " (" + access + ")";
			}
		}

		if (!mesg.empty())
		{
			font->renderUTF8(mesg, 0, llfloor(left + 3), llfloor(bottom + 2),
							 LLColor4::white, LLFontGL::LEFT,
							 LLFontGL::BASELINE, LLFontGL::DROP_SHADOW);

			// If map texture is still loading, display "Loading" placeholder
			// text.
			if (simtexp && simtexp->getDiscardLevel() != 1 &&
				simtexp->getDiscardLevel() != 0)
			{
				static const LLWString loading =
					LLTrans::getWString("texture_loading");
				font->render(loading, 0, llfloor(left + 18), llfloor(top - 25),
							 LLColor4::white, LLFontGL::LEFT,
							 LLFontGL::BASELINE, LLFontGL::DROP_SHADOW);
			}
		}
	}

	if (!gUsePBRShaders)
	{
		// Draw background rectangle
		LLGLSUIDefault gls_ui;

		unit0->unbind(LLTexUnit::TT_TEXTURE);
		gGL.blendFunc(LLRender::BF_ONE_MINUS_DEST_ALPHA,
					  LLRender::BF_DEST_ALPHA);
		gGL.flush();
		gGL.color4fv(mBackgroundColor.mV);
		gl_rect_2d(0, height, width, 0);

		gGL.flush();
		gGL.setSceneBlendType(LLRender::BT_ALPHA);
	}

	// Draw one image per region, centered on the camera position.	// Infohubs
	static LLCachedControl<bool> map_show_info_hubs(gSavedSettings,
													"MapShowInfohubs");
	if (map_show_info_hubs) // && sMapScale >= sThreshold)
	{
		drawGenericItems(gWorldMap.mInfohubs, sInfohubImage);
	}

	// Telehubs
	static LLCachedControl<bool> map_show_telehubs(gSavedSettings,
												   "MapShowTelehubs");
	if (map_show_telehubs) // && sMapScale >= sThreshold)
	{
		drawGenericItems(gWorldMap.mTelehubs, sTelehubImage);
	}

	// Home Sweet Home
	LLVector3d home;
	if (gAgent.getHomePosGlobal(&home))
	{
		drawImage(home, sHomeImage);
	}

	// Land for sale
	if (map_show_land_for_sale)
	{
		drawGenericItems(gWorldMap.mLandForSale, sForSaleImage);
		// We are showing normal land and adult land in the same UI; you do not
		// get a choice about which ones you want. If you are currently asking
		// for adult content and land you will get the adult land.
		if (gAgent.canAccessAdult())
		{
			drawGenericItems(gWorldMap.mLandForSaleAdult,
							 sForSaleAdultImage);
		}
	}

	// Events
	drawEvents();

	// Now draw your avatar after all that other stuff.
	LLVector3d pos_global = gAgent.getPositionGlobal();
	drawImage(pos_global, sAvatarYouImage);

	LLVector3 pos_map = globalPosToView(pos_global);
	if (!pointInView(ll_round(pos_map.mV[VX]), ll_round(pos_map.mV[VY])))
	{
		// Offset vertically by 1 line to avoid overlap with target tracking
		static S32 font_height = ll_round(font->getLineHeight());

		drawTracking(pos_global,
					 lerp(LLColor4::yellow, LLColor4::orange, 0.4f),
					 true, "You are here", "", font_height);
	}

	// Show your viewing angle
	drawFrustum();

	// Draw icons for the avatars in each region. Drawn after your avatar so
	// you can see nearby people.
	static LLCachedControl<bool> map_show_people(gSavedSettings,
												 "MapShowPeople");
	if (map_show_people)
	{
		drawAgents();
	}

	// Always draw tracking information
	LLTracker::ETrackingStatus tracking_status = gTracker.getTrackingStatus();
	if (LLTracker::TRACKING_AVATAR == tracking_status)
	{
		drawTracking(gAvatarTracker.getGlobalPos(), LLUI::sTrackColor, true,
					 gTracker.getLabel(), "");
	}
	else if (LLTracker::TRACKING_LANDMARK == tracking_status ||
			 LLTracker::TRACKING_LOCATION == tracking_status)
	{
		// While fetching landmarks, will have 0,0,0 location for a while,
		// so do not draw. JC
		LLVector3d pos_global = gTracker.getTrackedPositionGlobal();
		if (!pos_global.isExactlyZero())
		{
			drawTracking(pos_global, LLUI::sTrackColor, true,
						 gTracker.getLabel(), gTracker.getToolTip());
		}
	}
	else if (gWorldMap.mIsTrackingUnknownLocation)
	{
		if (gWorldMap.mInvalidLocation)
		{
			// We know this location to be invalid
			LLColor4 loading_color(0.f, 0.5f, 1.f, 1.f);
			drawTracking(gWorldMap.mUnknownLocation, loading_color, true,
						 "Invalid Location", "");
		}
		else
		{
			F32 value = fmodf(current_time, 2);
			value = 0.5f + 0.5f * cosf(value * F_PI);
			LLColor4 loading_color(0.f, value * 0.5f, value, 1.f);
			drawTracking(gWorldMap.mUnknownLocation, loading_color, true,
						 "Loading...", "");
		}
	}

	// Turn off the scissor
	LLGLDisable no_scissor(GL_SCISSOR_TEST);

	updateDirections();

	LLView::draw();

	updateVisibleBlocks();

	gGL.flush();
}

//virtual
void LLPanelWorldMap::setVisible(bool visible)
{
	LLPanel::setVisible(visible);
	if (!visible)
	{
		for (S32 map = 0; map < MAP_SIM_IMAGE_TYPES; ++map)
		{
			for (U32 i = 0, count = gWorldMap.mMapLayers[map].size();
				 i < count; ++i)
			{
				LLWorldMapLayer& layer = gWorldMap.mMapLayers[map][i];
				if (layer.mLayerDefined)
				{
					layer.mLayerImage->setBoostLevel(0);
				}
			}
		}
		for (LLWorldMap::sim_info_map_t::iterator
				it = gWorldMap.mSimInfoMap.begin(),
				end = gWorldMap.mSimInfoMap.end();
			 it != end; ++it)
		{
			LLSimInfo* info = it->second;
			if (info->mCurrentImage[mLayer].notNull())
			{
				info->mCurrentImage[mLayer]->setBoostLevel(0);
			}
			if (info->mOverlayImage.notNull())
			{
				info->mOverlayImage->setBoostLevel(0);
			}
		}
	}
}

void LLPanelWorldMap::drawGenericItems(const LLWorldMap::item_info_list_t& items,
									   LLUIImagePtr image)
{
	for (LLWorldMap::item_info_list_t::const_iterator it = items.begin(),
													  end = items.end();
		 it != end; ++it)
	{
		drawGenericItem(*it, image);
	}
}

void LLPanelWorldMap::drawGenericItem(const LLItemInfo& item,
									  LLUIImagePtr image)
{
	drawImage(item.mPosGlobal, image);
}

void LLPanelWorldMap::drawImage(const LLVector3d& global_pos,
								LLUIImagePtr image, const LLColor4& color)
{
	LLVector3 pos_map = globalPosToView(global_pos);
	image->draw(ll_round(pos_map.mV[VX] - image->getWidth() * 0.5f),
				ll_round(pos_map.mV[VY] - image->getHeight() * 0.5f), color);
}

void LLPanelWorldMap::drawImageStack(const LLVector3d& global_pos,
									 LLUIImagePtr image, U32 count, F32 offset,
									 const LLColor4& color)
{
	LLVector3 pos_map = globalPosToView(global_pos);
	for (U32 i = 0; i < count; ++i)
	{
		image->draw(ll_round(pos_map.mV[VX] - image->getWidth() * 0.5f),
					ll_round(pos_map.mV[VY] - image->getHeight() * 0.5f +
							 i * offset),
					color);
	}
}

void LLPanelWorldMap::drawAgents()
{
	F32 agents_scale = sMapScale * (0.9f / 256.f);

	static LLCachedControl<LLColor4U> map_avatar(gColors, "MapAvatar");
	LLColor4 avatar_color = LLColor4(map_avatar);

	LLWorldMap::agent_list_map_t::iterator end_agent_locations =
		gWorldMap.mAgentLocationsMap.end();
	for (handle_list_t::iterator iter = mVisibleRegions.begin(),
								 end = mVisibleRegions.end();
		 iter != end; ++iter)
	{
		U64 handle = *iter;
		LLSimInfo* siminfo = gWorldMap.simInfoFromHandle(handle);
		if (!siminfo || siminfo->mAccess == SIM_ACCESS_DOWN)
		{
			continue;
		}
		LLWorldMap::agent_list_map_t::iterator counts_iter =
			gWorldMap.mAgentLocationsMap.find(handle);
		if (siminfo->mShowAgentLocations && counts_iter != end_agent_locations)
		{
			// Show individual agents (or little stacks where real agents are)
			LLWorldMap::item_info_list_t& agentcounts = counts_iter->second;
			S32 sim_agent_count = 0;
			for (LLWorldMap::item_info_list_t::iterator
					iter = agentcounts.begin(), end2 = agentcounts.end();
				 iter != end2; ++iter)
			{
				const LLItemInfo& info = *iter;
				S32 agent_count = info.mExtra;
				sim_agent_count += agent_count;
				drawImageStack(info.mPosGlobal, sAvatarSmallImage, agent_count,
							   3.f, avatar_color);
			}
			// Override number of agents for this sim
			siminfo->mAgentsCount = sim_agent_count;
		}
		else
		{
			S32 sim_agent_count = siminfo->mAgentsCount;
			if (sim_agent_count <= 0) continue;

			// Show agent 'stack' at center of sim
			LLVector3d region_center = from_region_handle(handle);
			region_center[VX] += REGION_WIDTH_METERS / 2;
			region_center[VY] += REGION_WIDTH_METERS / 2;
			// Reduce the stack size as you zoom out - always display at least
			// one agent where there is one or more
			S32 agent_count = (S32)(((sim_agent_count - 1) * agents_scale +
									 (sim_agent_count - 1) * 0.1f) + 0.1f) + 1;
			drawImageStack(region_center, sAvatarSmallImage, agent_count,
						   3.f, avatar_color);
		}
	}
}

void LLPanelWorldMap::drawEvents()
{
	static LLCachedControl<bool> map_show_pg_events(gSavedSettings,
													"MapShowPGEvents");
	static LLCachedControl<bool> map_show_mature_events(gSavedSettings,
														"MapShowMatureEvents");
	static LLCachedControl<bool> map_show_adult_events(gSavedSettings,
													   "MapShowAdultEvents");

	bool show_pg = map_show_pg_events;
	bool show_mature = map_show_mature_events && gAgent.canAccessMature();
	bool show_adult = map_show_adult_events && gAgent.canAccessAdult();

	if (!show_pg && !show_mature && !show_adult)
	{
		return;
	}

    // First the non-selected events
	if (show_pg)
	{
		for (LLWorldMap::item_info_list_t::const_iterator
				it = gWorldMap.mPGEvents.begin(),
				end = gWorldMap.mPGEvents.end();
			 it != end; ++it)
		{
			if (!it->mSelected)
			{
				drawGenericItem(*it, sEventImage);
			}
		}
	}
    if (show_mature)
    {
		for (LLWorldMap::item_info_list_t::const_iterator
				it = gWorldMap.mMatureEvents.begin(),
				end = gWorldMap.mMatureEvents.end();
			 it != end; ++it)
        {
            if (!it->mSelected)
            {
                drawGenericItem(*it, sEventMatureImage);
            }
        }
    }
	if (show_adult)
    {
		for (LLWorldMap::item_info_list_t::const_iterator
				it = gWorldMap.mAdultEvents.begin(),
				end = gWorldMap.mAdultEvents.end();
			 it != end; ++it)
        {
            if (!it->mSelected)
            {
                drawGenericItem(*it, sEventAdultImage);
            }
        }
    }

    // Then the selected events
	if (show_pg)
	{
		for (LLWorldMap::item_info_list_t::const_iterator
				it = gWorldMap.mPGEvents.begin(),
				end = gWorldMap.mPGEvents.end();
			 it != end; ++it)
		{
			if (it->mSelected)
			{
				drawGenericItem(*it, sEventImage);
			}
		}
	}
    if (show_mature)
    {
		for (LLWorldMap::item_info_list_t::const_iterator
				it = gWorldMap.mMatureEvents.begin(),
				end = gWorldMap.mMatureEvents.end();
			 it != end; ++it)
        {
            if (it->mSelected)
            {
                drawGenericItem(*it, sEventMatureImage);
            }
        }
    }
	if (show_adult)
    {
		for (LLWorldMap::item_info_list_t::const_iterator
				it = gWorldMap.mAdultEvents.begin(),
				end = gWorldMap.mAdultEvents.end();
			 it != end; ++it)
        {
            if (it->mSelected)
            {
                drawGenericItem(*it, sEventAdultImage);
            }
        }
    }
}

void LLPanelWorldMap::drawFrustum()
{
	// Draw frustum
	F32 meters_to_pixels = sMapScale / REGION_WIDTH_METERS;

	F32 horiz_fov = gViewerCamera.getView() * gViewerCamera.getAspect();
	F32 far_clip_meters = gViewerCamera.getFar();
	F32 far_clip_pixels = far_clip_meters * meters_to_pixels;

	F32 half_width_meters = far_clip_meters * tanf(horiz_fov * 0.5f);
	F32 half_width_pixels = half_width_meters * meters_to_pixels;

	// Compute the frustum coordinates. Take the UI scale into account.
	static LLCachedControl<F32> ui_scale(gSavedSettings, "UIScaleFactor");
	F32 ctr_x = (getRect().getWidth() * 0.5f + sPanX) * ui_scale;
	F32 ctr_y = (getRect().getHeight() * 0.5f + sPanY) * ui_scale;

	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

	gGL.pushMatrix();
	gGL.translatef(ctr_x, ctr_y, 0.f);
	{
		LLVector3 at_axis = gViewerCamera.getAtAxis();
		LLVector3 left_axis = gViewerCamera.getLeftAxis();

		// Grab components along XY plane
		LLVector2 cam_lookat(at_axis.mV[VX], at_axis.mV[VY]);
		LLVector2 cam_left(left_axis.mV[VX], left_axis.mV[VY]);
		// But, when looking near straight up or down...
		if (is_approx_zero(cam_lookat.lengthSquared()))
		{
			// ...just fall back to looking down the x axis
			cam_lookat = LLVector2(1.f, 0.f);	// x axis
			cam_left = LLVector2(0.f, 1.f);		// y axis
		}

		// Normalize to unit length
		cam_lookat.normalize();
		cam_left.normalize();

		// Draw triangle with more alpha in far pixels to make it fade out in
		// distance.
		gGL.begin(LLRender::TRIANGLES);
			gGL.color4f(1.f, 1.f, 1.f, 0.25f);
			gGL.vertex2f(0.f, 0.f);

			gGL.color4f(1.f, 1.f, 1.f, 0.02f);
			// use 2d camera vectors to render frustum triangle
			LLVector2 vert = cam_lookat * far_clip_pixels +
							 cam_left * half_width_pixels;
			gGL.vertex2f(vert.mV[VX], vert.mV[VY]);

			vert = cam_lookat * far_clip_pixels - cam_left * half_width_pixels;
			gGL.vertex2f(vert.mV[VX], vert.mV[VY]);
		gGL.end();
	}
	gGL.popMatrix();
}

LLVector3 LLPanelWorldMap::globalPosToView(const LLVector3d& global_pos)
{
	LLVector3d relative_pos_global = global_pos -
									 gAgent.getCameraPositionGlobal();
	LLVector3 pos_local;
	pos_local.set(relative_pos_global);  // convert to floats from doubles

	pos_local.mV[VX] *= sPixelsPerMeter;
	pos_local.mV[VY] *= sPixelsPerMeter;
	// leave Z component in meters

	pos_local.mV[VX] += getRect().getWidth() / 2 + sPanX;
	pos_local.mV[VY] += getRect().getHeight() / 2 + sPanY;

	return pos_local;
}

void LLPanelWorldMap::drawTracking(const LLVector3d& pos_global,
								   const LLColor4& color, bool draw_arrow,
								   const std::string& label,
								   const std::string& tooltip, S32 vert_offset)
{
	static LLFontGL* font = LLFontGL::getFontSansSerifSmall();
	static F32 font_height = font->getLineHeight();

	LLVector3 pos_local = globalPosToView(pos_global);
	S32 x = ll_roundp(pos_local.mV[VX]);
	S32 y = ll_roundp(pos_local.mV[VY]);
	S32 text_x = x;
	S32 text_y = (S32)(y - sTrackCircleImage->getHeight() / 2 - font_height);

	if (x < 0 || y < 0 || x >= getRect().getWidth() ||
		y >= getRect().getHeight())
	{
		if (draw_arrow)
		{
			drawTrackingCircle(getRect(), x, y, color, 3, 15);
			drawTrackingArrow(getRect(), x, y, color);
			text_x = sTrackingArrowX;
			text_y = sTrackingArrowY;
		}
	}
	else if (gTracker.getTrackingStatus() == LLTracker::TRACKING_LOCATION &&
			 gTracker.getTrackedLocationType() != LLTracker::LOCATION_NOTHING)
	{
		drawTrackingCircle(getRect(), x, y, color, 3, 15);
	}
	else
	{
		drawImage(pos_global, sTrackCircleImage, color);
	}

	if (label.empty())
	{
		return;
	}
	// Clamp text position to on-screen
	constexpr S32 TEXT_PADDING = DEFAULT_TRACKING_ARROW_SIZE + 2;
	S32 half_text_width = llfloor(font->getWidthF32(label) * 0.5f);
	text_x = llclamp(text_x, half_text_width + TEXT_PADDING,
					 getRect().getWidth() - half_text_width - TEXT_PADDING);
	text_y = llclamp(text_y + vert_offset, TEXT_PADDING + vert_offset,
					 getRect().getHeight() - ll_roundp(font_height) -
					 TEXT_PADDING - vert_offset);

	font->renderUTF8(label, 0, text_x, text_y, LLColor4::white,
					 LLFontGL::HCENTER, LLFontGL::BASELINE,
					 LLFontGL::DROP_SHADOW);

	if (tooltip.empty())
	{
		return;
	}
	font->renderUTF8(tooltip, 0, text_x, text_y - (S32)font_height,
					 LLColor4::white, LLFontGL::HCENTER,
					 LLFontGL::BASELINE, LLFontGL::DROP_SHADOW);
}

// If you change this, then you need to change
// gTracker.getTrackedPositionGlobal() as well
LLVector3d LLPanelWorldMap::viewPosToGlobal(S32 x, S32 y)
{
	x -= llfloor(getRect().getWidth() / 2 + sPanX);
	y -= llfloor(getRect().getHeight() / 2 + sPanY);

	LLVector3 pos_local((F32)x, (F32)y, 0.f);

	pos_local *= (REGION_WIDTH_METERS / sMapScale);

	LLVector3d pos_global;
	pos_global.set(pos_local);
	pos_global += gAgent.getCameraPositionGlobal();
	if (sDefaultZ >= 0.f)
	{
		// Use the last Z position when available
		pos_global.mdV[VZ] = sDefaultZ;
	}
	else if (gAgent.isGodlike())
	{
		// Godly height should always be 200.
		pos_global.mdV[VZ] = GODLY_TELEPORT_HEIGHT;
	}
	else
	{
		// Want agent's height, not camera's
		pos_global.mdV[VZ] = gAgent.getPositionAgent().mV[VZ];
	}

	return pos_global;
}

bool LLPanelWorldMap::handleToolTip(S32 x, S32 y, std::string& msg,
									LLRect* sticky_rect_screen)
{
	LLVector3d pos_global = viewPosToGlobal(x, y);

	LLSimInfo* info = gWorldMap.simInfoFromPosGlobal(pos_global);
	if (!info)
	{
		return true;
	}
	LLViewerRegion* region = gAgent.getRegion();
	if (!region)
	{
		return true;
	}

	std::string message =
		llformat("%s (%s)", info->mName.c_str(),
				 LLViewerRegion::accessToString(info->mAccess).c_str());

	if (info->mAccess != SIM_ACCESS_DOWN)
	{
		S32 agent_count = info->mAgentsCount;
		// We may not have an agent count when the map is really zoomed out, so
		// do not display anything about the count. JC
		if (agent_count >= 0)
		{
			if (region->getHandle() == info->mHandle)
			{
				++agent_count; // Bump by 1 if we are here
			}
			if (agent_count > 0)
			{
				message += llformat("\n%d resident", agent_count);
				if (agent_count > 1)
				{
					message += "s";
				}
			}
		}
	}
	msg.assign(message);

	// Optionally show region flags
	if (gFloaterWorldMapp)	// Paranoia
	{
		message.clear();
		if (info->mRegionFlags & REGION_FLAGS_SANDBOX)
		{
			message = gFloaterWorldMapp->getString("sandbox");
		}
		if (info->mRegionFlags & REGION_FLAGS_ALLOW_DAMAGE)
		{
			if (!message.empty())
			{
				message += " - ";
			}
			message += gFloaterWorldMapp->getString("not_safe");
		}
		if (!message.empty())
		{
			msg += '\n';
			msg += message;
		}
	}

	constexpr S32 SLOP = 4;
	localPointToScreen(x - SLOP, y - SLOP, &(sticky_rect_screen->mLeft),
					   &(sticky_rect_screen->mBottom));
	sticky_rect_screen->mRight = sticky_rect_screen->mLeft + 2 * SLOP;
	sticky_rect_screen->mTop = sticky_rect_screen->mBottom + 2 * SLOP;

	return true;
}

// Pass relative Z of 0 to draw at same level.
//static
void LLPanelWorldMap::drawIconName(F32 x_pixels, F32 y_pixels,
								   const LLColor4& color,
								   const std::string& first_line,
								   const std::string& second_line)
{
	static LLFontGL* font = LLFontGL::getFontSansSerif();
	static F32 font_height = font->getLineHeight();
	constexpr S32 VERT_PAD = 8;

	S32 text_x = ll_roundp(x_pixels);
	S32 text_y = ll_roundp(y_pixels - BIG_DOT_RADIUS - VERT_PAD);

	// Render first line of text
	font->renderUTF8(first_line, 0, text_x, text_y, color, LLFontGL::HCENTER,
					 LLFontGL::TOP, LLFontGL::DROP_SHADOW);

	text_y -= ll_roundp(font_height);

	// Render second line of text
	font->renderUTF8(second_line, 0, text_x, text_y, color, LLFontGL::HCENTER,
					 LLFontGL::TOP, LLFontGL::DROP_SHADOW);
}

//static
void LLPanelWorldMap::drawTrackingCircle(const LLRect& rect, S32 x, S32 y,
										 const LLColor4& color,
										 S32 min_thickness, S32 overlap)
{
	F32 start_theta = 0.f;
	F32 end_theta = F_TWO_PI;
	F32 x_delta = 0.f;
	F32 y_delta = 0.f;

	if (x < 0)
	{
		x_delta = 0.f - (F32)x;
		start_theta = F_PI + F_PI_BY_TWO;
		end_theta = F_TWO_PI + F_PI_BY_TWO;
	}
	else if (x > rect.getWidth())
	{
		x_delta = (F32)(x - rect.getWidth());
		start_theta = F_PI_BY_TWO;
		end_theta = F_PI + F_PI_BY_TWO;
	}

	if (y < 0)
	{
		y_delta = 0.f - (F32)y;
		if (x < 0)
		{
			start_theta = 0.f;
			end_theta = F_PI_BY_TWO;
		}
		else if (x > rect.getWidth())
		{
			start_theta = F_PI_BY_TWO;
			end_theta = F_PI;
		}
		else
		{
			start_theta = 0.f;
			end_theta = F_PI;
		}
	}
	else if (y > rect.getHeight())
	{
		y_delta = (F32)(y - rect.getHeight());
		if (x < 0)
		{
			start_theta = F_PI + F_PI_BY_TWO;
			end_theta = F_TWO_PI;
		}
		else if (x > rect.getWidth())
		{
			start_theta = F_PI;
			end_theta = F_PI + F_PI_BY_TWO;
		}
		else
		{
			start_theta = F_PI;
			end_theta = F_TWO_PI;
		}
	}

	F32 distance = llmax(0.1f, sqrtf(x_delta * x_delta + y_delta * y_delta));

	F32 outer_radius = distance +
					   (1.f + 9.f * sqrtf(x_delta * y_delta) / distance) *
					   (F32)overlap;

	F32 inner_radius = outer_radius - (F32)min_thickness;

	F32 angle_adjust_x = asinf(x_delta / outer_radius);
	F32 angle_adjust_y = asinf(y_delta / outer_radius);

	if (angle_adjust_x)
	{
		if (angle_adjust_y)
		{
			F32 angle_adjust = llmin(angle_adjust_x, angle_adjust_y);
			start_theta += angle_adjust;
			end_theta -= angle_adjust;
		}
		else
		{
			start_theta += angle_adjust_x;
			end_theta -= angle_adjust_x;
		}
	}
	else if (angle_adjust_y)
	{
		start_theta += angle_adjust_y;
		end_theta -= angle_adjust_y;
	}

	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.pushMatrix();
	gGL.translatef((F32)x, (F32)y, 0.f);
	gl_washer_segment_2d(inner_radius, outer_radius, start_theta, end_theta,
						 40, color, color);
	gGL.popMatrix();
}

//static
void LLPanelWorldMap::drawTrackingArrow(const LLRect& rect, S32 x, S32 y,
										const LLColor4& color, S32 arrow_size)
{
	F32 x_center = (F32)rect.getWidth() * 0.5f;
	F32 y_center = (F32)rect.getHeight() * 0.5f;

	F32 x_clamped = (F32)llclamp(x, 0, rect.getWidth() - arrow_size);
	F32 y_clamped = (F32)llclamp(y, 0, rect.getHeight() - arrow_size);

	F32 slope = (F32)(y - y_center) / (F32)(x - x_center);
	F32 window_ratio = (F32)rect.getHeight() / (F32)rect.getWidth();

	if (fabsf(slope) > window_ratio && y_clamped != (F32)y)
	{
		// Clamp by y
		x_clamped = (y_clamped - y_center) / slope + x_center;
		// Adjust for arrow size
		x_clamped  = llclamp(x_clamped , 0.f,
							 (F32)(rect.getWidth() - arrow_size));
	}
	else if (x_clamped != (F32)x)
	{
		// Clamp by x
		y_clamped = (x_clamped - x_center) * slope + y_center;
		// Adjust for arrow size
		y_clamped = llclamp(y_clamped, 0.f,
							(F32)(rect.getHeight() - arrow_size));
	}

	S32 half_arrow_size = (S32)(0.5f * arrow_size);
	F32 angle = atan2f(y + half_arrow_size - y_center,
					   x + half_arrow_size - x_center);

	sTrackingArrowX = llfloor(x_clamped);
	sTrackingArrowY = llfloor(y_clamped);

	gl_draw_scaled_rotated_image(sTrackingArrowX, sTrackingArrowY, arrow_size,
								 arrow_size, RAD_TO_DEG * angle,
								 sTrackArrowImage->getImage(), color);
}

// Note: 'rotation' is in radians (0 means x = 1, y = 0 on the unit circle)
void LLPanelWorldMap::setDirectionPos(LLTextBox* text_box, F32 rotation)
{
	F32 map_half_height = getRect().getHeight() * 0.5f;
	F32 map_half_width = getRect().getWidth() * 0.5f;
	F32 text_half_height = text_box->getRect().getHeight() * 0.5f;
	F32 text_half_width = text_box->getRect().getWidth() * 0.5f;
	F32 radius = llmin(map_half_height - text_half_height,
					   map_half_width - text_half_width);

	text_box->setOrigin(ll_round(map_half_width - text_half_width +
								 radius * cosf(rotation)),
						ll_round(map_half_height - text_half_height +
								 radius * sinf(rotation)));
}

void LLPanelWorldMap::updateDirections()
{
	S32 width = getRect().getWidth();
	S32 height = getRect().getHeight();

	S32 text_height = mTextBoxNorth->getRect().getHeight();
	S32 text_width = mTextBoxNorth->getRect().getWidth();

	constexpr S32 PAD = 2;
	S32 top = height - text_height - PAD;
	S32 left = PAD * 2;
	S32 bottom = PAD;
	S32 right = width - text_width - PAD;
	S32 center_x = width / 2 - text_width / 2;
	S32 center_y = height / 2 - text_height / 2;

	mTextBoxNorth->setOrigin(center_x, top);
	mTextBoxEast->setOrigin(right, center_y);
	mTextBoxSouth->setOrigin(center_x, bottom);
	mTextBoxWest->setOrigin(left, center_y);

	// These have wider text boxes
	text_width = mTextBoxNorthWest->getRect().getWidth();
	right = width - text_width - PAD;

	mTextBoxNorthWest->setOrigin(left, top);
	mTextBoxNorthEast->setOrigin(right, top);
	mTextBoxSouthWest->setOrigin(left, bottom);
	mTextBoxSouthEast->setOrigin(right, bottom);
}

void LLPanelWorldMap::reshape(S32 width, S32 height, bool called_from_parent)
{
	LLView::reshape(width, height, called_from_parent);
}

bool LLPanelWorldMap::checkItemHit(S32 x, S32 y, LLItemInfo& item, LLUUID& id,
								   bool track)
{
	if (!gFloaterWorldMapp) return true;

	LLVector3 pos_view = globalPosToView(item.mPosGlobal);
	S32 item_x = ll_round(pos_view.mV[VX]);
	S32 item_y = ll_round(pos_view.mV[VY]);

	if (x < item_x - BIG_DOT_RADIUS) return false;
	if (x > item_x + BIG_DOT_RADIUS) return false;
	if (y < item_y - BIG_DOT_RADIUS) return false;
	if (y > item_y + BIG_DOT_RADIUS) return false;

	LLSimInfo* sim_info = gWorldMap.simInfoFromHandle(item.mRegionHandle);
	if (sim_info && track)
	{
		gFloaterWorldMapp->trackLocation(item.mPosGlobal);
	}

	if (track)
	{
		gFloaterWorldMapp->trackGenericItem(item);
	}

	item.mSelected = true;
	id = item.mID;

	return true;
}

// Handle a click, which might be on a dot
void LLPanelWorldMap::handleClick(S32 x, S32 y, MASK mask, S32& hit_type,
								  LLUUID& id)
{
	if (!gFloaterWorldMapp) return;

	LLVector3d pos_global = viewPosToGlobal(x, y);

	// *HACK: Adjust Z values automatically for liaisons & gods so we swoop
	// down when they click on the map.
	if (gAgent.isGodlike())
	{
		pos_global.mdV[VZ] = 200.0;
	}

	hit_type = 0; // Hit nothing

	gWorldMap.mIsTrackingUnknownLocation = false;
	gWorldMap.mIsTrackingDoubleClick = false;
	gWorldMap.mIsTrackingCommit = false;

	// Clear old selected stuff
	for (LLWorldMap::item_info_list_t::iterator
			it = gWorldMap.mPGEvents.begin(),
			end = gWorldMap.mPGEvents.end();
		 it != end; ++it)
	{
		it->mSelected = false;
	}
	for (LLWorldMap::item_info_list_t::iterator
			it = gWorldMap.mMatureEvents.begin(),
			end = gWorldMap.mMatureEvents.end();
		 it != end; ++it)
	{
		it->mSelected = false;
	}
	for (LLWorldMap::item_info_list_t::iterator
			it = gWorldMap.mAdultEvents.begin(),
			end = gWorldMap.mAdultEvents.end();
		 it != end; ++it)
	{
		it->mSelected = false;
	}
	for (LLWorldMap::item_info_list_t::iterator
			it = gWorldMap.mLandForSale.begin(),
			end = gWorldMap.mLandForSale.end();
		 it != end; ++it)
	{
		it->mSelected = false;
	}

	// Select event you clicked on
	if (gSavedSettings.getBool("MapShowPGEvents"))
	{
		for (LLWorldMap::item_info_list_t::iterator
				it = gWorldMap.mPGEvents.begin(),
				end = gWorldMap.mPGEvents.end();
			 it != end; ++it)
		{
			LLItemInfo& event = *it;

			if (checkItemHit(x, y, event, id, false))
			{
				hit_type = MAP_ITEM_PG_EVENT;
				mItemPicked = true;
				gFloaterWorldMapp->trackEvent(event);
				return;
			}
		}
	}
	if (gSavedSettings.getBool("MapShowMatureEvents"))
	{
		for (LLWorldMap::item_info_list_t::iterator
				it = gWorldMap.mMatureEvents.begin(),
				end = gWorldMap.mMatureEvents.end();
			 it != end; ++it)
		{
			LLItemInfo& event = *it;

			if (checkItemHit(x, y, event, id, false))
			{
				hit_type = MAP_ITEM_MATURE_EVENT;
				mItemPicked = true;
				gFloaterWorldMapp->trackEvent(event);
				return;
			}
		}
	}
	if (gSavedSettings.getBool("MapShowAdultEvents"))
	{
		for (LLWorldMap::item_info_list_t::iterator
				it = gWorldMap.mAdultEvents.begin(),
				end = gWorldMap.mAdultEvents.end();
			 it != end; ++it)
		{
			LLItemInfo& event = *it;

			if (checkItemHit(x, y, event, id, false))
			{
				hit_type = MAP_ITEM_ADULT_EVENT;
				mItemPicked = true;
				gFloaterWorldMapp->trackEvent(event);
				return;
			}
		}
	}

	if (gSavedSettings.getBool("MapShowLandForSale"))
	{
		for (LLWorldMap::item_info_list_t::iterator
				it = gWorldMap.mLandForSale.begin(),
				end = gWorldMap.mLandForSale.end();
			 it != end; ++it)
		{
			LLItemInfo& land = *it;

			if (checkItemHit(x, y, land, id, true))
			{
				hit_type = MAP_ITEM_LAND_FOR_SALE;
				mItemPicked = true;
				return;
			}
		}

		for (LLWorldMap::item_info_list_t::iterator
				it = gWorldMap.mLandForSaleAdult.begin(),
				end = gWorldMap.mLandForSaleAdult.end();
			 it != end; ++it)
		{
			LLItemInfo& land = *it;

			if (checkItemHit(x, y, land, id, true))
			{
				hit_type = MAP_ITEM_LAND_FOR_SALE_ADULT;
				mItemPicked = true;
				return;
			}
		}
	}

	// If we get here, we have not clicked on an icon
	gFloaterWorldMapp->trackLocation(pos_global);
	mItemPicked = false;
	id.setNull();
}

bool outside_slop(S32 x, S32 y, S32 start_x, S32 start_y)
{
	S32 dx = x - start_x;
	S32 dy = y - start_y;
	return dx <= -2 || 2 <= dx || dy <= -2 || 2 <= dy;
}

bool LLPanelWorldMap::handleMouseDown(S32 x, S32 y, MASK mask)
{
	gFocusMgr.setMouseCapture(this);

	mMouseDownPanX = ll_round(sPanX);
	mMouseDownPanY = ll_round(sPanY);
	mMouseDownX = x;
	mMouseDownY = y;
	sHandledLastClick = true;
	return true;
}

bool LLPanelWorldMap::handleMouseUp(S32 x, S32 y, MASK mask)
{
	if (hasMouseCapture())
	{
		if (mPanning)
		{
			// Restore mouse cursor
			S32 local_x, local_y;
			local_x = mMouseDownX + llfloor(sPanX - mMouseDownPanX);
			local_y = mMouseDownY + llfloor(sPanY - mMouseDownPanY);
			LLRect clip_rect = getRect();
			clip_rect.stretch(-8);
			clip_rect.clipPointToRect(mMouseDownX, mMouseDownY, local_x,
									  local_y);
			LLUI::setCursorPositionLocal(this, local_x, local_y);

			// Finish the pan
			mPanning = false;

			mMouseDownX = 0;
			mMouseDownY = 0;
		}
		else
		{
			// Ignore whether we hit an event or not
			S32 hit_type = 0;
			LLUUID id;
			handleClick(x, y, mask, hit_type, id);
		}
		gViewerWindowp->showCursor();
		gFocusMgr.setMouseCapture(NULL);
		return true;
	}
	return false;
}

U32 LLPanelWorldMap::updateVisibleBlocks()
{
	if (sMapScale < SIM_MAP_SCALE)
	{
		// We do not care what is loaded if we are zoomed out
		return 0;
	}

	LLVector3d camera_global = gAgent.getCameraPositionGlobal();
	const S32 half_width = 0.5f * getRect().getWidth();
	const S32 half_height = 0.5f * getRect().getHeight();

	// Compute center into sim grid coordinates
	S32 world_center_x = S32(-sPanX / sMapScale +
							 camera_global.mdV[0] / REGION_WIDTH_METERS);
	S32 world_center_y = S32(-sPanY / sMapScale +
							 camera_global.mdV[1] / REGION_WIDTH_METERS);

	// Find the corresponding 8x8 block
	S32 world_left = world_center_x - S32(half_width / sMapScale) - 1;
	S32 world_right = world_center_x + S32(half_width / sMapScale) + 1;
	S32 world_bottom = world_center_y - S32(half_height / sMapScale) - 1;
	S32 world_top = world_center_y + S32(half_height / sMapScale) + 1;

	return gWorldMap.updateRegions(world_left, world_bottom, world_right,
								   world_top);
}

bool LLPanelWorldMap::handleHover(S32 x, S32 y, MASK mask)
{
	if (hasMouseCapture())
	{
		if (mPanning || outside_slop(x, y, mMouseDownX, mMouseDownY))
		{
			// Just started panning, so hide cursor
			if (!mPanning)
			{
				mPanning = true;
				gViewerWindowp->hideCursor();
			}

			F32 delta_x = (F32)(gViewerWindowp->getCurrentMouseDX());
			F32 delta_y = (F32)(gViewerWindowp->getCurrentMouseDY());

			// Set pan to value at start of drag + offset
			sPanX += delta_x;
			sPanY += delta_y;
			sTargetPanX = sPanX;
			sTargetPanY = sPanY;

			gViewerWindowp->moveCursorToCenter();
		}

		// It does not matter, cursor should be hidden
		gViewerWindowp->setCursor(UI_CURSOR_CROSS);
		return true;
	}
	else
	{
		// While we are waiting for data from the tracker, we are busy. JC
		LLVector3d pos_global = gTracker.getTrackedPositionGlobal();
		if (gTracker.isTracking() && pos_global.isExactlyZero())
		{
			gViewerWindowp->setCursor(UI_CURSOR_WAIT);
		}
		else
		{
			gViewerWindowp->setCursor(UI_CURSOR_CROSS);
		}
		LL_DEBUGS("UserInput") << "Hover handled by LLPanelWorldMap"
							   << LL_ENDL;
		return true;
	}
}

bool LLPanelWorldMap::handleDoubleClick(S32 x, S32 y, MASK mask)
{
	if (!gFloaterWorldMapp) return true;

	if (sHandledLastClick)
	{
		S32 hit_type = 0;
		LLUUID id;
		handleClick(x, y, mask, hit_type, id);

		switch (hit_type)
		{
			case MAP_ITEM_PG_EVENT:
			case MAP_ITEM_MATURE_EVENT:
			case MAP_ITEM_ADULT_EVENT:
			{
				gFloaterWorldMapp->close();
				// This is an ungainly hack
				std::string uuid_str;
				S32 event_id;
				id.toString(uuid_str);
				uuid_str = uuid_str.substr(28);
				sscanf(uuid_str.c_str(), "%X", &event_id);
				HBFloaterSearch::showEvents(event_id);
				break;
			}

			case MAP_ITEM_LAND_FOR_SALE:
			case MAP_ITEM_LAND_FOR_SALE_ADULT:
			{
				gFloaterWorldMapp->close();
				HBFloaterSearch::showLandForSale(id);
				break;
			}

			case MAP_ITEM_CLASSIFIED:
			{
				gFloaterWorldMapp->close();
				HBFloaterSearch::showClassified(id);
				break;
			}

			default:
			{
				if (gWorldMap.mIsTrackingUnknownLocation)
				{
					gWorldMap.mIsTrackingDoubleClick = true;
				}
				else
				{
					// Teleport if we got a valid location
					LLVector3d pos_global = viewPosToGlobal(x, y);
					LLSimInfo* sim_info =
						gWorldMap.simInfoFromPosGlobal(pos_global);
					if (sim_info && sim_info->mAccess != SIM_ACCESS_DOWN)
					{
						gAgent.teleportViaLocation(pos_global);
					}
				}
			}
		}

		return true;
	}
	return false;
}
