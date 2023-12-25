/**
 * @file llpanelminimap.cpp (was llnetmap.cpp)
 * @brief Displays agent and surrounding regions, objects, and avatars.
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 *
 * Original code by James Cook. Copyright (c) 2001-2009, Linden Research, Inc.
 * Copyright (c) 2009-2022, Henri Beauchamp.
 * Changes by Henri Beauchamp:
 *  - Rewrote and optimized part of the code.
 *  - Added unknown altitude avatar plot type (the dash-like dot).
 *  - Allowed both old style (T-like dots) and new style (V-like tiny icons)
 *    for above/below-agent avatar plots.
 *  - Added per-avatar dots colouring (via Lua).
 *  - Added animesh, path-finding and physical objects plotting.
 *  - Added agent sim borders drawing.
 *  - _Mostly_ fixed terrain textures never rendering (via changes done in
 *    llvlcomposition.cpp and in the texture cache) and added a terrain texture
 *    manual refreshing feature to deal with the cases when they still fail to
 *    render.
 *  - Backported (and improved) the optional rendering of banned parcels (from
 *    LL's viewer) and parcel borders (from LL's viewer for the nice but slow
 *    algorithm and from Singularity+Catznip for the fast but less pretty one).
 *  - Added parcel info ("About land" floater) for parcels in the context menu.
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

#include "llpanelminimap.h"

#include "llcachename.h"
#include "llmenugl.h"
#include "llparcel.h"
#include "llrender.h"
#include "lltextbox.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "llagentpilot.h"
#include "llappviewer.h"			// For gFrameTimeSeconds and constants
#include "llavatartracker.h"
#include "llfloateravatarinfo.h"
#include "llfloaterland.h"
#include "llfloaterworldmap.h"
#include "llpanelworldmap.h"		// For shared draw code
//MK
#include "mkrlinterface.h"
//mk
#include "llsurface.h"
#include "lltracker.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewermenu.h"
#include "llviewerobjectlist.h"
#include "llviewerparcelmgr.h"
#include "llviewerparceloverlay.h"
#include "llviewerregion.h"
#include "llviewertexturelist.h"
#include "llviewerwindow.h"
#include "llvoavatar.h"
#include "llvlcomposition.h"
#include "llworld.h"

using namespace LLOldEvents;

constexpr F32 MAP_SCALE_MIN = 32;
constexpr F32 MAP_SCALE_MID = 256;
constexpr F32 MAP_SCALE_MAX = 4096;
// Zoom in factor per click of the scroll wheel (10%):
constexpr F32 MAP_SCALE_ZOOM_FACTOR = 1.1f;
constexpr F32 MAP_MINOR_DIR_THRESHOLD = 0.08f;
constexpr F32 MIN_DOT_RADIUS = 3.5f;
constexpr F32 DOT_SCALE = 0.75f;
constexpr F32 MIN_PICK_SCALE = 2.f;
// How far the mouse needs to move before we think it is a drag:
constexpr S32 MOUSE_DRAG_SLOP = 2;

bool LLPanelMiniMap::sMiniMapRotate = true;
S32 LLPanelMiniMap::sMiniMapCenter = 1;

LLPanelMiniMap::LLPanelMiniMap(const std::string& name)
:	LLPanel(name),
	mNorthLabel(NULL),
	mSouthLabel(NULL),
	mWestLabel(NULL),
	mEastLabel(NULL),
	mNorthWestLabel(NULL),
	mNorthEastLabel(NULL),
	mSouthWestLabel(NULL),
	mSouthEastLabel(NULL),
	mScale(128.f),
	mObjectMapTPM(1.f),
	mObjectMapPixels(255.f),
	mTargetPanX(0.f),
	mTargetPanY(0.f),
	mCurPanX(0.f),
	mCurPanY(0.f),
	mUpdateObjectImage(false),
	mUpdateParcelImage(false),
	mHasDrawnParcels(false)
{
	mBackgroundColor = isBackgroundOpaque() ? getBackgroundColor()
											: getTransparentColor();

	mScale = gSavedSettings.getF32("MiniMapScale");

	// Unintuitive and hacky... To support variable region size we must make
	// the mini-map believe regions got a fixed size of 256m...
	mPixelsPerMeter = mScale / REGION_WIDTH_METERS;

	mDotRadius = llmax(DOT_SCALE * mPixelsPerMeter, MIN_DOT_RADIUS);

	sMiniMapCenter = gSavedSettings.getS32("MiniMapCenter");
	sMiniMapRotate = gSavedSettings.getBool("MiniMapRotate");

	mObjectImageCenterGlobal = gAgent.getCameraPositionGlobal();

	// Register event listeners for popup menu
	(new LLScaleMap())->registerListener(this, "MiniMap.ZoomLevel");
	(new LLCenterMap())->registerListener(this, "MiniMap.Center");
	(new LLCheckCenterMap())->registerListener(this, "MiniMap.CheckCenter");
	(new LLRotateMap())->registerListener(this, "MiniMap.Rotate");
	(new LLCheckRotateMap())->registerListener(this, "MiniMap.CheckRotate");

	(new LLDrawObjects())->registerListener(this, "MiniMap.DrawObjects");
	(new LLCheckDrawObjects())->registerListener(this,
												 "MiniMap.CheckDrawObjects");

	(new LLPlotPuppets())->registerListener(this, "MiniMap.PlotPuppets");
	(new LLCheckPlotPuppets())->registerListener(this,
												 "MiniMap.CheckPlotPuppets");

	(new LLPlotChars())->registerListener(this, "MiniMap.PlotChars");
	(new LLCheckPlotChars())->registerListener(this, "MiniMap.CheckPlotChars");
	(new LLEnablePlotChars())->registerListener(this,
												"MiniMap.EnablePlotChars");

	(new LLPlotPhysical())->registerListener(this, "MiniMap.PlotPhysical");
	(new LLCheckPlotPhysical())->registerListener(this,
												  "MiniMap.CheckPlotPhysical");
	(new LLEnablePlotPhysical())->registerListener(this,
												   "MiniMap.EnablePlotPhysical");

	(new LLDrawWater())->registerListener(this, "MiniMap.DrawWater");
	(new LLCheckDrawWater())->registerListener(this, "MiniMap.CheckDrawWater");

	(new LLDrawBorders())->registerListener(this, "MiniMap.DrawBorders");
	(new LLCheckDrawBorders())->registerListener(this,
												 "MiniMap.CheckDrawBorders");
	(new LLDrawBans())->registerListener(this, "MiniMap.DrawBans");
	(new LLCheckDrawBans())->registerListener(this, "MiniMap.CheckDrawBans");
	(new LLDrawParcels())->registerListener(this, "MiniMap.DrawParcels");
	(new LLCheckDrawParcels())->registerListener(this,
												 "MiniMap.CheckDrawParcels");
	(new LLShowParcelInfo())->registerListener(this, "MiniMap.ShowParcelInfo");
	(new LLEnableParcelInfo())->registerListener(this, "MiniMap.EnableParcelInfo");
	(new LLRefreshTerrain())->registerListener(this, "MiniMap.Refresh");

	(new LLStopTracking())->registerListener(this, "MiniMap.StopTracking");
	(new LLEnableTracking())->registerListener(this, "MiniMap.EnableTracking");
	(new LLShowAgentProfile())->registerListener(this, "MiniMap.ShowProfile");
	(new LLEnableProfile())->registerListener(this, "MiniMap.EnableProfile");

	LLUICtrlFactory::getInstance()->buildPanel(this, "panel_mini_map.xml");

	LLMenuGL* menu =
		LLUICtrlFactory::getInstance()->buildMenu("menu_mini_map.xml", this);
	if (!menu)
	{
		menu = new LLMenuGL(LLStringUtil::null);
	}
	menu->setVisible(false);
	mPopupMenuHandle = menu->getHandle();
}

bool LLPanelMiniMap::postBuild()
{
	mNorthLabel = getChild<LLTextBox>("n_label", true, false);
	if (mNorthLabel)
	{
		mSouthLabel = getChild<LLTextBox>("s_label");
		mWestLabel = getChild<LLTextBox>("w_label");
		mEastLabel = getChild<LLTextBox>("e_label");
		mNorthWestLabel = getChild<LLTextBox>("nw_label");
		mNorthEastLabel = getChild<LLTextBox>("ne_label");
		mSouthWestLabel = getChild<LLTextBox>("sw_label");
		mSouthEastLabel = getChild<LLTextBox>("se_label");

		updateMinorDirections();
	}

	mMapToolTip = getToolTip();
	mRegionPrefix = getString("region_prefix") + " ";
	mParcelPrefix = getString("parcel_prefix") + " ";
	mOwnerPrefix = getString("owner_prefix") + " ";

	return true;
}

void LLPanelMiniMap::setScale(F32 scale)
{
	mScale = scale;
	if (mScale == 0.f)
	{
		mScale = 0.1f;
	}
	static F32 old_scale = 0.f;
	if (mScale != old_scale)
	{
		gSavedSettings.setF32("MiniMapScale", mScale);
		old_scale = mScale;
	}

	// Unintuitive and hacky... To support variable region size we must make
	// the mini-map believe regions got a fixed size of 256m...
	constexpr F32 region_width = REGION_WIDTH_METERS;

	if (mObjectImagep.notNull())
	{
		F32 width = (F32)(getRect().getWidth());
		F32 height = (F32)(getRect().getHeight());
		F32 diameter = sqrtf(width * width + height * height);
		F32 meters = diameter * region_width / mScale;
		F32 num_pixels = (F32)mObjectImagep->getWidth();
		mObjectMapTPM = num_pixels / meters;
		mObjectMapPixels = diameter;
	}

	mPixelsPerMeter = mScale / region_width;
	mDotRadius = llmax(DOT_SCALE * mPixelsPerMeter, MIN_DOT_RADIUS);

	mUpdateObjectImage = mUpdateParcelImage = true;
}

//virtual
void LLPanelMiniMap::draw()
{
	if (mObjectImagep.isNull())
	{
		createObjectImage();
	}
	static LLCachedControl<bool> fast_parcels(gSavedSettings,
											  "MinimapFastParcelBorders");
	if (fast_parcels && mParcelImagep.isNull())
	{
		createParcelImage();
	}

	if (sMiniMapCenter != MAP_CENTER_NONE)
	{
		F32 critical_damp = LLCriticalDamp::getInterpolant(0.1f);
		mCurPanX = lerp(mCurPanX, mTargetPanX, critical_damp);
		mCurPanY = lerp(mCurPanY, mTargetPanY, critical_damp);
	}

	mHasDrawnParcels = false;

	F32 rotation = 0;

	LLTexUnit* unit0 = gGL.getTexUnit(0);

	// Prepare a scissor region
	gGL.pushMatrix();
	gGL.pushUIMatrix();

	LLVector3 offset = gGL.getUITranslation();
	LLVector3 scale = gGL.getUIScale();

	gGL.loadIdentity();
	gGL.loadUIIdentity();
	gGL.scalef(scale.mV[0], scale.mV[1], scale.mV[2]);
	gGL.translatef(offset.mV[0], offset.mV[1], offset.mV[2]);
	{
		LLLocalClipRect clip(getLocalRect());
		{
			unit0->unbind(LLTexUnit::TT_TEXTURE);
			gGL.matrixMode(LLRender::MM_MODELVIEW);

			// Draw background rectangle
			gl_rect_2d(0, getRect().getHeight(), getRect().getWidth(), 0,
					   mBackgroundColor);
		}

		// Region 0,0 is in the middle
		S32 center_sw_left = getRect().getWidth() / 2 + llfloor(mCurPanX);
		S32 center_sw_bottom = getRect().getHeight() / 2 + llfloor(mCurPanY);

		gGL.pushMatrix();

		gGL.translatef((F32)center_sw_left, (F32)center_sw_bottom, 0.f);

		if (sMiniMapRotate)
		{
			// Rotate subsequent draws to agent rotation
			rotation = atan2f(gViewerCamera.getAtAxis().mV[VX],
							  gViewerCamera.getAtAxis().mV[VY]);
			gGL.rotatef(rotation * RAD_TO_DEG, 0.f, 0.f, 1.f);
		}

		// Scale in pixels per meter
		F32 scale = mScale / REGION_WIDTH_METERS;

		static LLCachedControl<LLColor4U> map_this(gColors,
												   "MiniMapThisRegion");
		static LLCachedControl<LLColor4U> map_live(gColors,
												   "MiniMapLiveRegion");
		static LLCachedControl<LLColor4U> map_dead(gColors,
												   "MiniMapDeadRegion");
		static LLCachedControl<LLColor4U> map_banned(gColors,
													 "MiniMapBannedParcels");
		static LLCachedControl<LLColor4U> map_parcel(gColors,
													 "MiniMapParcelBorders");
		static LLCachedControl<LLColor4U> map_borders(gColors,
													  "MiniMapRegionBorders");
		const LLColor4 this_region_color = LLColor4(map_this);
		const LLColor4 live_region_color = LLColor4(map_live);
		const LLColor4 dead_region_color = LLColor4(map_dead);

		const LLVector3& cam_pos_agent = gAgent.getCameraPositionAgent();
		LLViewerRegion* agent_regionp = gAgent.getRegion();
		F32 areg_top = 0.f;
		F32 areg_bottom = 0.f;
		F32 areg_left = 0.f;
		F32 areg_right = 0.f;
		for (LLWorld::region_list_t::const_iterator
				iter = gWorld.getRegionList().begin(),
				end = gWorld.getRegionList().end();
			 iter != end; ++iter)
		{
			LLViewerRegion* regionp = *iter;

			if (regionp == agent_regionp)
			{
				gGL.color4fv(this_region_color.mV);
			}
			else if (!regionp->isAlive())
			{
				gGL.color4fv(dead_region_color.mV);
			}
			else
			{
				gGL.color4fv(live_region_color.mV);
			}

			// Find x and y position relative to the center of camera.
			LLVector3 origin_agent = regionp->getOriginAgent();
			LLVector3 rel_region_pos = origin_agent - cam_pos_agent;
			F32 relative_x = rel_region_pos.mV[0] * scale;
			F32 relative_y = rel_region_pos.mV[1] * scale;

			// Background region rectangle
			F32 bottom = relative_y;
			F32 left = relative_x;
			// Variable region size support: scale the tile depending on region
			// actual width here.
			F32 tile_width = regionp->getWidth() * scale;
			F32 top = bottom + tile_width;
			F32 right = left + tile_width;
			if (regionp == agent_regionp)
			{
				areg_top = top;
				areg_bottom = bottom;
				areg_left = left;
				areg_right = right;
			}

			// Draw using texture.
			unit0->bind(regionp->getLand().getSTexture());

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

			static LLCachedControl<bool> draw_water(gSavedSettings,
													"MiniMapDrawWater");
			if (draw_water)
			{
				// Draw water
				LLViewerTexture* water_tex =
					regionp->getLand().getWaterTexture();
				if (water_tex)
				{
					unit0->bind(water_tex);

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
			}
		}

		LLVector3d pos_center = getPosCenterGlobal();

		static LLCachedControl<bool> draw_objects(gSavedSettings,
												  "MiniMapDrawObjects");
		if (draw_objects)
		{
			// Redraw object layer periodically
			static F32 last_redraw = 0.f;
			if (mUpdateObjectImage || gFrameTimeSeconds - last_redraw > 0.5f)
			{
				last_redraw = gFrameTimeSeconds;
				updateObjectImage(pos_center);
			}

			LLVector3 map_center_agent =
				gAgent.getPosAgentFromGlobal(mObjectImageCenterGlobal);
			map_center_agent -= cam_pos_agent;
			F32 agent_x = map_center_agent.mV[VX] * scale;
			F32 agent_y = map_center_agent.mV[VY] * scale;

			unit0->bind(mObjectImagep);

			const F32 image_half_width = 0.5f * mObjectMapPixels;
			const F32 image_half_height = 0.5f * mObjectMapPixels;
			gGL.begin(LLRender::TRIANGLES);
			gGL.texCoord2f(0.f, 1.f);
			gGL.vertex2f(agent_x - image_half_width,
						 image_half_height + agent_y);
			gGL.texCoord2f(0.f, 0.f);
			gGL.vertex2f(agent_x - image_half_width,
						 agent_y - image_half_height);
			gGL.texCoord2f(1.f, 0.f);
			gGL.vertex2f(image_half_width + agent_x,
						 agent_y - image_half_height);
			gGL.texCoord2f(0.f, 1.f);
			gGL.vertex2f(agent_x - image_half_width,
						 image_half_height + agent_y);
			gGL.texCoord2f(1.f, 0.f);
			gGL.vertex2f(image_half_width + agent_x,
						 agent_y - image_half_height);
			gGL.texCoord2f(1.f, 1.f);
			gGL.vertex2f(image_half_width + agent_x,
						 image_half_height + agent_y);
			gGL.end();
		}

		static LLCachedControl<bool> show_parcels(gSavedSettings,
												  "MinimapShowParcelBorders");
		static LLCachedControl<bool> show_banned(gSavedSettings,
												 "MinimapShowBannedParcels");
		mHasDrawnParcels = show_parcels;
		if (fast_parcels && show_parcels)
		{
			if (mUpdateParcelImage ||
				dist_vec_squared2D(pos_center, mParcelImageCenterGlobal) > 9.f)
			{
				updateParcelImage(pos_center, map_parcel);
			}

			LLVector3 map_center_agent =
				gAgent.getPosAgentFromGlobal(mParcelImageCenterGlobal);
			map_center_agent -= cam_pos_agent;
			F32 agent_x = map_center_agent.mV[VX] * scale;
			F32 agent_y = map_center_agent.mV[VY] * scale;

			unit0->bind(mParcelImagep);

			const F32 image_half_width = 0.5f * mObjectMapPixels;
			const F32 image_half_height = 0.5f * mObjectMapPixels;
			gGL.begin(LLRender::TRIANGLES);
			gGL.texCoord2f(0.f, 1.f);
			gGL.vertex2f(agent_x - image_half_width,
						 image_half_height + agent_y);
			gGL.texCoord2f(0.f, 0.f);
			gGL.vertex2f(agent_x - image_half_width,
						 agent_y - image_half_height);
			gGL.texCoord2f(1.f, 0.f);
			gGL.vertex2f(image_half_width + agent_x,
						 agent_y - image_half_height);
			gGL.texCoord2f(0.f, 1.f);
			gGL.vertex2f(agent_x - image_half_width,
						 image_half_height + agent_y);
			gGL.texCoord2f(1.f, 0.f);
			gGL.vertex2f(image_half_width + agent_x,
						 agent_y - image_half_height);
			gGL.texCoord2f(1.f, 1.f);
			gGL.vertex2f(image_half_width + agent_x,
						 image_half_height + agent_y);
			gGL.end();
		}
		if (fast_parcels && show_banned)
		{
			const LLColor4 banned_color = LLColor4(map_banned);
			for (LLWorld::region_list_t::const_iterator
					iter = gWorld.getRegionList().begin(),
					end = gWorld.getRegionList().end();
				 iter != end; ++iter)
			{
				LLViewerRegion* regionp = *iter;
				if (regionp->renderBannedParcels(scale, banned_color.mV))
				{
					mHasDrawnParcels = true;
				}
			}
		}
		if (!fast_parcels && (show_parcels || show_banned))
		{
			const LLColor4 banned_color = LLColor4(map_banned);
			const LLColor4 parcel_color = LLColor4(map_parcel);
			for (LLWorld::region_list_t::const_iterator
					iter = gWorld.getRegionList().begin(),
					end = gWorld.getRegionList().end();
				 iter != end; ++iter)
			{
				LLViewerRegion* regionp = *iter;
				if (show_parcels)
				{
					regionp->renderParcelBorders(scale, parcel_color.mV);
				}
				if (show_banned &&
					regionp->renderBannedParcels(scale, banned_color.mV))
				{
					mHasDrawnParcels = true;
				}
			}
		}

		// Draw agent region borders. HB
		static LLCachedControl<bool> draw_borders(gSavedSettings,
												  "MiniMapDrawBorders");
		if (draw_borders && areg_top != areg_bottom)
		{
			gl_rect_2d(areg_left, areg_top, areg_right, areg_bottom,
					   LLColor4(map_borders), false);
		}

		gGL.popMatrix();

		LLVector3 pos_map;
		LLColor4 avatar_color;

		// Draw physical objects. HB
		static LLCachedControl<bool> plot_physical(gSavedSettings,
												   "MiniMapPlotPhysicalObj");
		if (plot_physical && !mPhysicalObjectsPos.empty())
		{
			static LLCachedControl<LLColor4U> map_phys(gColors,
													   "MiniMapPhysicalObject");
			avatar_color = LLColor4(map_phys);
			for (objs_pos_vec_t::iterator it = mPhysicalObjectsPos.begin(),
										  end = mPhysicalObjectsPos.end();
				 it != end; ++it)
			{
				const LLVector3d& pos = *it;
				pos_map = globalPosToView(pos, sMiniMapRotate);
				drawAvatar(avatar_color, pos_map);
			}
		}

		// Draw path-finding characters. HB
		static LLCachedControl<bool> plot_characters(gSavedSettings,
													 "MiniMapPlotCharacters");
		if (plot_characters && !mPathfindingCharsPos.empty())
		{
			static LLCachedControl<LLColor4U> map_chars(gColors,
														"MiniMapPathFindingChar");
			avatar_color = LLColor4(map_chars);
			for (objs_pos_vec_t::iterator it = mPathfindingCharsPos.begin(),
										  end = mPathfindingCharsPos.end();
				 it != end; ++it)
			{
				const LLVector3d& pos = *it;
				pos_map = globalPosToView(pos, sMiniMapRotate);
				drawAvatar(avatar_color, pos_map);
			}
		}

		static LLCachedControl<U32> unknwown_alt(gSavedSettings,
												 "UnknownAvatarAltitude");

		// Draw puppets. HB
		static LLCachedControl<bool> plot_puppets(gSavedSettings,
												  "MiniMapPlotPuppets");
		if (plot_puppets)
		{
			static LLCachedControl<LLColor4U> map_puppets(gColors,
														  "MiniMapPuppetsColor");
			avatar_color = LLColor4(map_puppets);
			for (S32 i = 0, count = LLCharacter::sInstances.size();
				 i < count; ++i)
			{
				LLVOAvatar* avatarp = (LLVOAvatar*)LLCharacter::sInstances[i];
				if (!avatarp || avatarp->isDead() || avatarp->isOrphaned() ||
					!avatarp->isPuppetAvatar() ||
					(LLXform*)avatarp != avatarp->getRoot())
				{
					continue;
				}
				LLVector3d pos = avatarp->getPositionGlobal();
				pos_map = globalPosToView(pos, sMiniMapRotate);
				if (pos.mdV[VZ] == (F32)unknwown_alt)
				{
					pos_map.mV[VZ] = 16000.f;
				}
				drawAvatar(avatar_color, pos_map);
			}
		}

		// Prepare for "closest avatar to cursor" detection. Note: mouse
		// pointer is in local coordinates.
		S32 local_mouse_x;
		S32 local_mouse_y;
		LLUI::getCursorPositionLocal(this, &local_mouse_x, &local_mouse_y);
		mClosestAgentToCursor.setNull();
		F32 closest_dist = F32_MAX;
		F32 min_pick_dist = mDotRadius * MIN_PICK_SCALE;

		// Draw avatars

		uuid_vec_t avatar_ids;
		std::vector<LLVector3d> positions;
		std::vector<LLColor4> colors;
		gWorld.getAvatars(avatar_ids, &positions, &colors);
		for (U32 i = 0, count = avatar_ids.size(); i < count; ++i)
		{
			const LLUUID& av_id = avatar_ids[i];
			if (av_id == gAgentID) continue;

			const LLVector3d& pos = positions[i];
			pos_map = globalPosToView(pos, sMiniMapRotate);
			if (pos.mdV[VZ] == (F32)unknwown_alt)
			{
				pos_map.mV[VZ] = 16000.f;
			}
//MK
			// Do not show specific colors under @shownames, since it can give
			// away an information about the avatars who are around
			if (gRLenabled &&
				(gRLInterface.mContainsShownames ||
				 gRLInterface.mContainsShowNearby ||
				 gRLInterface.mContainsShownametags))
			{
				static LLCachedControl<LLColor4U> map_avatar(gColors,
															 "MapAvatar");
				avatar_color = LLColor4(map_avatar);
			}
			else
//mk
			{
				avatar_color = colors[i];
			}

			drawAvatar(avatar_color, pos_map);

			F32	dist_to_cursor =
				dist_vec(LLVector2(pos_map.mV[VX], pos_map.mV[VY]),
						 LLVector2(local_mouse_x, local_mouse_y));
			if (dist_to_cursor < min_pick_dist &&
				dist_to_cursor < closest_dist)
			{
				closest_dist = dist_to_cursor;
				mClosestAgentToCursor = av_id;
			}
		}

		// Draw dot for autopilot target
		if (gAgentPilot.isActive())
		{
			drawTracking(gAgentPilot.getAutoPilotTargetGlobal(),
						 sMiniMapRotate, LLUI::sTrackColor);
		}
		else
		{
			LLTracker::ETrackingStatus tracking_status =
				gTracker.getTrackingStatus();
			if (tracking_status == LLTracker::TRACKING_AVATAR)
			{
				drawTracking(gAvatarTracker.getGlobalPos(),
							 sMiniMapRotate, LLUI::sTrackColor);
			}
			else if (tracking_status == LLTracker::TRACKING_LANDMARK ||
					 tracking_status == LLTracker::TRACKING_LOCATION)
			{
				drawTracking(gTracker.getTrackedPositionGlobal(),
							 sMiniMapRotate, LLUI::sTrackColor);
			}
		}

		// Draw dot for self avatar position
		pos_map = globalPosToView(gAgent.getPositionGlobal(), sMiniMapRotate);
		LLUIImagePtr you = LLPanelWorldMap::sAvatarYouLargeImage;
		S32 dot_width = ll_roundp(mDotRadius * 2.f);
		you->draw(ll_round(pos_map.mV[VX] - mDotRadius),
				  ll_round(pos_map.mV[VY] - mDotRadius),
				  dot_width, dot_width);

		// Draw frustum
		F32 horiz_fov = gViewerCamera.getView() * gViewerCamera.getAspect();
		F32 far_clip_meters = gViewerCamera.getFar();
		F32 far_clip_pixels = far_clip_meters * scale;

		F32 half_width_meters = far_clip_meters * tanf(horiz_fov * 0.5f);
		F32 half_width_pixels = half_width_meters * scale;

		F32 ctr_x = (F32)center_sw_left;
		F32 ctr_y = (F32)center_sw_bottom;

		unit0->unbind(LLTexUnit::TT_TEXTURE);

		static LLCachedControl<LLColor4U> frustum(gColors,
												  "MiniMapFrustum");
		static LLCachedControl<LLColor4U> rot_frustum(gColors,
													  "MiniMapFrustumRotating");
		if (sMiniMapRotate)
		{
			gGL.color4fv(LLColor4(frustum).mV);

			gGL.begin(LLRender::TRIANGLES);
			gGL.vertex2f(ctr_x, ctr_y);
			gGL.vertex2f(ctr_x - half_width_pixels, ctr_y + far_clip_pixels);
			gGL.vertex2f(ctr_x + half_width_pixels, ctr_y + far_clip_pixels);
			gGL.end();
		}
		else
		{
			gGL.color4fv(LLColor4(rot_frustum).mV);

			// If we do not rotate the map, we have to rotate the frustum.
			gGL.pushMatrix();

			gGL.translatef(ctr_x, ctr_y, 0.f);
			gGL.rotatef(atan2f(gViewerCamera.getAtAxis().mV[VX],
							   gViewerCamera.getAtAxis().mV[VY]) *
						RAD_TO_DEG, 0.f, 0.f, -1.f);

			gGL.begin(LLRender::TRIANGLES);
			gGL.vertex2f(0.f, 0.f);
			gGL.vertex2f(-half_width_pixels, far_clip_pixels);
			gGL.vertex2f(half_width_pixels, far_clip_pixels);
			gGL.end();

			gGL.popMatrix();
		}
	}

	gGL.popUIMatrix();
	gGL.popMatrix();

	// Rotation of 0 means that North is up
	setDirectionPos(mEastLabel, rotation);
	setDirectionPos(mNorthLabel, rotation + F_PI_BY_TWO);
	setDirectionPos(mWestLabel, rotation + F_PI);
	setDirectionPos(mSouthLabel, rotation + (F_PI + F_PI_BY_TWO));

	constexpr F32 F_PI_BY_FOUR = F_PI_BY_TWO * 0.5f;
	setDirectionPos(mNorthEastLabel, rotation + F_PI_BY_FOUR);
	setDirectionPos(mNorthWestLabel, rotation + (F_PI_BY_TWO + F_PI_BY_FOUR));
	setDirectionPos(mSouthWestLabel, rotation + (F_PI + F_PI_BY_FOUR));
	setDirectionPos(mSouthEastLabel,
					rotation + (F_PI + F_PI_BY_TWO + F_PI_BY_FOUR));

	LLView::draw();
}

//virtual
void LLPanelMiniMap::reshape(S32 width, S32 height, bool called_from_parent)
{
	LLPanel::reshape(width, height, called_from_parent);
	createObjectImage();
	updateMinorDirections();
}

LLVector3 LLPanelMiniMap::globalPosToView(const LLVector3d& global_pos,
										  bool rotated) const
{
	LLVector3d relative_pos_global = global_pos -
									 gAgent.getCameraPositionGlobal();
	LLVector3 pos_local(relative_pos_global); // Convert to floats from doubles

	pos_local.mV[VX] *= mPixelsPerMeter;
	pos_local.mV[VY] *= mPixelsPerMeter;
	// Leave Z component in meters

	if (rotated)
	{
		F32 radians = atan2f(gViewerCamera.getAtAxis().mV[VX],
							 gViewerCamera.getAtAxis().mV[VY]);
		LLQuaternion rot(radians, LLVector3(0.f, 0.f, 1.f));
		pos_local.rotVec(rot);
	}

	pos_local.mV[VX] += getRect().getWidth() / 2 + mCurPanX;
	pos_local.mV[VY] += getRect().getHeight() / 2 + mCurPanY;

	return pos_local;
}

void LLPanelMiniMap::drawAvatar(const LLColor4& color, const LLVector3& pos)
{
	constexpr F32 HEIGHT_THRESHOLD = 7.f;

	const F32& x_pixels = pos.mV[VX];
	const F32& y_pixels = pos.mV[VY];
	const F32& relative_z = pos.mV[VZ];

	LLUIImagePtr dot_image = LLPanelWorldMap::sAvatarSmallImage;

	// Allow the use of old style avatar dots. HB
 	static LLCachedControl<bool> use_old_dots(gSavedSettings,
											  "UseOldTrackingDots");
	if (use_old_dots || relative_z == 16000.f)
	{
		F32 left = x_pixels - mDotRadius;
		F32 right = x_pixels + mDotRadius;
		F32 center = (left + right) * 0.5f;
		F32 top = y_pixels + mDotRadius;
		F32 bottom = y_pixels - mDotRadius;

		if (relative_z == 16000.f)
		{
			// Unknown altitude (0m or > 1020m). HB
			gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
			gGL.color4fv(color.mV);
			LLUI::setLineWidth(1.5f);
			gGL.begin(LLRender::LINES);
				gGL.vertex2f(left, y_pixels);
				gGL.vertex2f(right, y_pixels);
			gGL.end();
			LLUI::setLineWidth(1.f);

		}
		else if (relative_z > HEIGHT_THRESHOLD)
		{
			gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
			gGL.color4fv(color.mV);
			LLUI::setLineWidth(1.5f);
			gGL.begin(LLRender::LINES);
				gGL.vertex2f(left, top);
				gGL.vertex2f(right, top);
				gGL.vertex2f(center, top);
				gGL.vertex2f(center, bottom);
			gGL.end();
			LLUI::setLineWidth(1.f);
		}
		else if (relative_z > -HEIGHT_THRESHOLD)
		{
			dot_image->draw(ll_roundp(x_pixels) - dot_image->getWidth() / 2,
							ll_roundp(y_pixels) - dot_image->getHeight() / 2,
							color);
		}
		else
		{
			gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
			gGL.color4fv(color.mV);
			LLUI::setLineWidth(1.5f);
			gGL.begin(LLRender::LINES);
				gGL.vertex2f(center, top);
				gGL.vertex2f(center, bottom);
				gGL.vertex2f(left, bottom);
				gGL.vertex2f(right, bottom);
			gGL.end();
			LLUI::setLineWidth(1.f);
		}
	}
	else
	{
		if (relative_z < -HEIGHT_THRESHOLD)
		{
			dot_image = LLPanelWorldMap::sAvatarBelowImage;
		}
		else if (relative_z > HEIGHT_THRESHOLD)
		{
			dot_image = LLPanelWorldMap::sAvatarAboveImage;
		}

		S32 dot_width = ll_roundp(mDotRadius * 2.f);
		dot_image->draw(ll_roundp(x_pixels - mDotRadius),
						ll_roundp(y_pixels - mDotRadius),
						dot_width, dot_width, color);
	}
}

void LLPanelMiniMap::drawTracking(const LLVector3d& pos_global, bool rotated,
								  const LLColor4& color, bool draw_arrow)
{
	LLVector3 pos_local = globalPosToView(pos_global, rotated);
	if (pos_local.mV[VX] < 0.f || pos_local.mV[VY] < 0.f ||
		pos_local.mV[VX] >= getRect().getWidth() ||
		pos_local.mV[VY] >= getRect().getHeight())
	{
		if (draw_arrow)
		{
			S32 x = ll_roundp(pos_local.mV[VX]);
			S32 y = ll_roundp(pos_local.mV[VY]);
			LLPanelWorldMap::drawTrackingCircle(getRect(), x, y, color, 1, 10);
			LLPanelWorldMap::drawTrackingArrow(getRect(), x, y, color);
		}
		return;
	}

	const LLUIImagePtr& dot_image = LLPanelWorldMap::sTrackCircleImage;

	const F32& x_pixels = pos_local.mV[VX];
	const F32& y_pixels = pos_local.mV[VY];
	const F32& relative_z = pos_local.mV[VZ];

	constexpr F32 HEIGHT_THRESHOLD = 7.f;
	if (relative_z >= -HEIGHT_THRESHOLD && relative_z <= HEIGHT_THRESHOLD)
	{
		S32 x = ll_roundp(x_pixels) - dot_image->getWidth() / 2;
		S32 y = ll_roundp(y_pixels) - dot_image->getHeight() / 2;
		dot_image->draw(x, y, color);
	}
	else
	{
		// Draw V indicator for above or below
		// *TODO: Replace this vector drawing with icons
		F32 left = x_pixels - mDotRadius;
		F32 right = x_pixels + mDotRadius;
		F32 center = (left + right) * 0.5f;
		F32 top = y_pixels + mDotRadius;
		F32 bottom = y_pixels - mDotRadius;

		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
		gGL.color4fv(color.mV);
		LLUI::setLineWidth(3.f);
		// Y pos of the point of the V
		F32 point = relative_z > HEIGHT_THRESHOLD ? top : bottom;
		// Y pos of the ends of the V
		F32 back = relative_z > HEIGHT_THRESHOLD ? bottom : top;
		gGL.begin(LLRender::LINES);
			gGL.vertex2f(left, back);
			gGL.vertex2f(center, point);
			gGL.vertex2f(center, point);
			gGL.vertex2f(right, back);
		gGL.end();
		LLUI::setLineWidth(1.f);
	}
}

LLVector3d LLPanelMiniMap::viewPosToGlobal(S32 x, S32 y, bool rotated) const
{
	x -= ll_round(getRect().getWidth() / 2 + mCurPanX);
	y -= ll_round(getRect().getHeight() / 2 + mCurPanY);
	LLVector3 pos_local((F32)x, (F32)y, 0.f);

	if (rotated)
	{
		F32 radians = -atan2f(gViewerCamera.getAtAxis().mV[VX],
						 	  gViewerCamera.getAtAxis().mV[VY]);
		LLQuaternion rot(radians, LLVector3::z_axis);
		pos_local.rotVec(rot);
	}

	// Unintuitive and hacky... To support variable region size we must make
	// the mini-map believe regions got a fixed size of 256m...
	pos_local *= REGION_WIDTH_METERS / mScale;

	return LLVector3d(pos_local) + gAgent.getCameraPositionGlobal();
}

void LLPanelMiniMap::setDirectionPos(LLTextBox* text_box, F32 rotation)
{
	// Rotation is in radians.
	// Rotation of 0 means x = 1, y = 0 on the unit circle.

	F32 half_height = (F32)((getRect().getHeight() -
							 text_box->getRect().getHeight()) / 2);
	F32 half_width  = (F32)((getRect().getWidth() -
							 text_box->getRect().getWidth()) / 2);
	F32 radius = llmin(half_height, half_width);

	// Inset by a little to account for position display.
	radius -= 8.f;

	text_box->setOrigin(ll_round(half_width  + radius * cosf(rotation)),
						ll_round(half_height + radius * sinf(rotation)));
}

void LLPanelMiniMap::updateMinorDirections()
{
	if (!mNorthEastLabel)
	{
		return;
	}

	// Hide minor directions if they cover too much of the map
	bool show_minors = mNorthEastLabel->getRect().getHeight() <
						MAP_MINOR_DIR_THRESHOLD * llmin(getRect().getWidth(),
														getRect().getHeight());

	mNorthEastLabel->setVisible(show_minors);
	mNorthWestLabel->setVisible(show_minors);
	mSouthEastLabel->setVisible(show_minors);
	mSouthWestLabel->setVisible(show_minors);
}

void LLPanelMiniMap::renderScaledPointGlobal(const LLVector3d& pos,
											 const LLColor4U& color,
											 F32 radius_meters)
{
	static LLCachedControl<F32> max_radius(gSavedSettings,
										   "MiniMapPrimMaxRadius");
	// Limit the size of megaprims so they do not blot out everything on
	// the mini-map. Attempting to draw very large megaprims also causes
	// client lag. See DEV-17370 and SNOW-79 for details.
	if (radius_meters > (F32)max_radius)
	{
		radius_meters = max_radius;
	}
	S32 diameter_pixels = ll_roundp(2 * radius_meters * mObjectMapTPM);

	LLVector3 local_pos;
	local_pos.set(pos - mObjectImageCenterGlobal);

	renderPoint(local_pos, color, diameter_pixels);
}

void LLPanelMiniMap::renderPoint(const LLVector3& pos_local,
								 const LLColor4U& color,
								 S32 diameter, S32 relative_height)
{
	if (diameter <= 0)
	{
		return;
	}

	const S32 image_width = (S32)mObjectImagep->getWidth();
	S32 x_offset = ll_round(pos_local.mV[VX] * mObjectMapTPM +
							image_width / 2);
	if (x_offset < 0 || x_offset >= image_width)
	{
		return;
	}

	const S32 image_height = (S32)mObjectImagep->getHeight();
	S32 y_offset = ll_round(pos_local.mV[VY] * mObjectMapTPM +
							image_height / 2);
	if (y_offset < 0 || y_offset >= image_height)
	{
		return;
	}

	U8* datap = mObjectRawImagep->getData();

	S32 neg_radius = diameter / 2;
	S32 pos_radius = diameter - neg_radius;
	S32 x, y;

	if (relative_height > 0)
	{
		// Point above agent
		S32 px, py;

		// Vertical line
		px = x_offset;
		for (y = -neg_radius; y < pos_radius; ++y)
		{
			py = y_offset + y;
			if (py >= 0 && py < image_height)
			{
				S32 offset = px + py * image_width;
				((U32*)datap)[offset] = color.asRGBA();
			}
		}

		// Top line
		py = y_offset + pos_radius - 1;
		for (x = -neg_radius; x < pos_radius; ++x)
		{
			px = x_offset + x;
			if (px >= 0 && px < image_width)
			{
				S32 offset = px + py * image_width;
				((U32*)datap)[offset] = color.asRGBA();
			}
		}
	}
	else
	{
		// Point level with agent
		for (x = -neg_radius; x < pos_radius; ++x)
		{
			S32 p_x = x_offset + x;
			if (p_x > 0 && p_x < image_width)
			{
				for (y = -neg_radius; y < pos_radius; ++y)
				{
					S32 p_y = y_offset + y;
					if (p_y >= 0 && p_y < image_height)
					{
						S32 offset = p_x + p_y * image_width;
						((U32*)datap)[offset] = color.asRGBA();
					}
				}
			}
		}
	}
}

LLVector3d LLPanelMiniMap::getPosCenterGlobal() const
{
	// Locate the centre of the object layer, accounting for panning
	LLVector3 center = globalPosToView(gAgent.getCameraPositionGlobal(),
									   sMiniMapRotate);
	center.mV[0] -= mCurPanX;
	center.mV[1] -= mCurPanY;
	center.mV[2] = 0.f;
	return viewPosToGlobal(ll_round(center.mV[0]), ll_round(center.mV[1]),
						   sMiniMapRotate);
}

bool LLPanelMiniMap::createRawImage(LLPointer<LLImageRaw>& rawimagep)
{
	// Find the size of the side of a square that surrounds the circle that
	// surrounds getRect()... which is, the diagonal of the rect.
	F32 width = getRect().getWidth();
	F32 height = getRect().getHeight();
	S32 square_size = ll_roundp(sqrtf(width * width + height * height));

	// Find the least power of two >= the minimum size.
	constexpr S32 MIN_SIZE = 64;
	constexpr S32 MAX_SIZE = 256;
	S32 img_size = MIN_SIZE;
	while (img_size * 2 < square_size && img_size < MAX_SIZE)
	{
		img_size <<= 1;
	}

	if (rawimagep.isNull() || rawimagep->getWidth() != img_size ||
		rawimagep->getHeight() != img_size)
	{
		rawimagep = new LLImageRaw(img_size, img_size, 4);
		U8* data = rawimagep->getData();
		if (data)
		{
			memset(data, 0, img_size * img_size * 4);
			return true;
		}
	}
	return false;
}

void LLPanelMiniMap::createObjectImage()
{
	if (createRawImage(mObjectRawImagep))
	{
		mObjectImagep =
			LLViewerTextureManager::getLocalTexture(mObjectRawImagep.get(),
													false);
	}
	setScale(mScale);
	mUpdateObjectImage = true;
}

void LLPanelMiniMap::createParcelImage()
{
	if (createRawImage(mParcelRawImagep))
	{
		mParcelImagep =
			LLViewerTextureManager::getLocalTexture(mParcelRawImagep.get(),
													false);
	}
	setScale(mScale);
	mUpdateParcelImage = true;
}

void LLPanelMiniMap::updateObjectImage(const LLVector3d& pos_center_global)
{
	mUpdateObjectImage = false;
	mObjectImageCenterGlobal = pos_center_global;

	// Create the base texture.
	S32 img_width = mObjectImagep->getWidth();
	S32 img_height = mObjectImagep->getHeight();
	U8* default_texture = mObjectRawImagep->getData();
	memset((void*)default_texture, 0,
		   img_width * img_height * mObjectImagep->getComponents());

	// Clear the cached positions for pathfinding characters and physical
	// objects since they will be re-filled by the renderObjectsForMap()
	// method. HB
	mPathfindingCharsPos.clear();
	mPhysicalObjectsPos.clear();
	// Draw objects
	gObjectList.renderObjectsForMap(this);

	mObjectImagep->setSubImage(mObjectRawImagep, 0, 0, img_width, img_height);
}

void LLPanelMiniMap::updateParcelImage(const LLVector3d& pos_center_global,
									   LLColor4U color)
{
	mUpdateParcelImage = false;
	mParcelImageCenterGlobal = pos_center_global;

	// Make the borders color opaque since the image is already rendered as a
	// semi-transparent overlay on the mini-map... HB
	color.mV[VW] = 255;

	// Create the base texture.
	S32 img_width = mParcelImagep->getWidth();
	S32 img_height = mParcelImagep->getHeight();
	U8* default_texture = mParcelRawImagep->getData();
	memset((void*)default_texture, 0,
		   img_width * img_height * mParcelRawImagep->getComponents());

	for (LLWorld::region_list_t::const_iterator
			iter = gWorld.getRegionList().begin(),
			end = gWorld.getRegionList().end();
		 iter != end; ++iter)
	{
		renderParcelBorders(*iter, color, img_width, img_height);
	}

	mParcelImagep->setSubImage(mParcelRawImagep, 0, 0, img_width, img_height);
}

// Backported from Singularity, itself derived from Catznip's code. HB
void LLPanelMiniMap::renderParcelBorders(const LLViewerRegion* regionp,
										 const LLColor4U& color,
										 S32 img_width, S32 img_height)
{
	LLViewerParcelOverlay* overlayp = regionp->getParcelOverlay();
	if (!overlayp)
	{
		return;	// Cannot draw anything at this point !
	}

	LLVector3 local_origin(regionp->getOriginGlobal() -
						   mParcelImageCenterGlobal);
	S32 x0 = ll_round(local_origin.mV[VX] * mObjectMapTPM + img_width / 2);
	S32 y0 = ll_round(local_origin.mV[VY] * mObjectMapTPM + img_width / 2);

	U32* tex_data = (U32*)mParcelRawImagep->getData();

	// Draw the North and East region borders
	S32 width = ll_round(regionp->getWidth() * mObjectMapTPM);
	S32 y = y0 + width;
	if (y >= 0 && y < img_height)
	{
		S32 line_offset = y * img_width;
		for (S32 x = llclamp(x0, 0, img_width),
				 end = llclamp(x0 + width, 0, img_width);
			 x < end; ++x)
		{
			tex_data[line_offset + x] = color.mAll;
		}
	}
	S32 x = x0 + width;
	if (x >= 0 && x < img_width)
	{
		for (S32 y = llclamp(y0, 0, img_height),
				 end = llclamp(y0 + width, 0, img_height);
			 y < end; ++y)
		{
			tex_data[y * img_width + x] = color.mAll;
		}
	}

	// Render South and West parcel lines
	S32 grids_per_edge = overlayp->getParcelGridsPerEdge();
	width = ll_round(PARCEL_GRID_STEP_METERS * mObjectMapTPM);
	F32 scale = PARCEL_GRID_STEP_METERS * mObjectMapTPM;
	for (S32 row = 0; row < grids_per_edge; ++row)
	{
		S32 pos_y = y0 + ll_round(scale * row);
		S32 line_offset = pos_y * img_width;
		for (S32 col = 0; col < grids_per_edge; ++col)
		{
			S32 pos_x = x0 + ll_round(scale * col);
			S32 overlay = overlayp->parcelLineFlags(row, col);
			if ((overlay & PARCEL_SOUTH_LINE) && pos_y >= 0 &&
				pos_y < img_height)
			{
				for (S32 x = llclamp(pos_x, 0, img_width),
						 end = llclamp(pos_x + width, 0, img_width);
					 x < end; ++x)
				{
					tex_data[line_offset + x] = color.mAll;
				}
			}
			if ((overlay & PARCEL_WEST_LINE) && pos_x >= 0 &&
				pos_x < img_width)
			{
				for (S32 y = llclamp(pos_y, 0, img_height),
						 end = llclamp(pos_y + width, 0, img_height);
					 y < end; ++y)
				{
					tex_data[y * img_width + pos_x] = color.mAll;
				}
			}
		}
	}
}

//virtual
bool LLPanelMiniMap::handleToolTip(S32 x, S32 y, std::string& msg,
								   LLRect* sticky_rect_screen)
{
	msg.clear();
	if (gDisconnected || LLApp::isExiting())
	{
		return false;
	}

	LLVector3d pos = viewPosToGlobal(x, y, sMiniMapRotate);
	LLViewerRegion*	regionp = gWorld.getRegionFromPosGlobal(pos);
	if (!regionp)
	{
		return LLPanel::handleToolTip(x, y, msg, sticky_rect_screen);
	}

	// Set the sticky_rect
	S32 SLOP = 4;
	localPointToScreen(x - SLOP, y - SLOP, &(sticky_rect_screen->mLeft),
					   &(sticky_rect_screen->mBottom));
	sticky_rect_screen->mRight = sticky_rect_screen->mLeft + 2 * SLOP;
	sticky_rect_screen->mTop = sticky_rect_screen->mBottom + 2 * SLOP;

	std::string fullname;
	if (mClosestAgentToCursor.notNull() && gCacheNamep &&
		gCacheNamep->getFullName(mClosestAgentToCursor, fullname))
	{
//MK	
		if (gRLenabled &&
			(gRLInterface.mContainsShownames ||
			 gRLInterface.mContainsShowNearby ||
			 gRLInterface.mContainsShownametags))
		{
			fullname = gRLInterface.getDummyName(fullname);
		}
		else
//mk
		if (LLAvatarNameCache::useDisplayNames())
		{
			LLAvatarName avatar_name;
			if (LLAvatarNameCache::get(mClosestAgentToCursor, &avatar_name))
			{
				if (LLAvatarNameCache::useDisplayNames() == 2)
				{
					fullname = avatar_name.mDisplayName;
				}
				else
				{
					fullname = avatar_name.getNames(true);
				}
			}
		}

		msg = fullname + "\n";
	}

//MK
	if (!gRLenabled || !gRLInterface.mContainsShowloc)
//mk
	{
		msg += mRegionPrefix + regionp->getName() + "\n";
		// Show parcel name and owner, when appropriate
		static LLCachedControl<bool> show_land_tip(gSavedSettings,
												   "ShowLandHoverTip");
		static LLCachedControl<bool> show_poperty(gSavedSettings,
												  "ShowPropertyLines");
		if (mHasDrawnParcels || show_land_tip || show_poperty)
		{
			gViewerParcelMgr.setHoverParcel(pos);
			LLParcel* parcelp = gViewerParcelMgr.getHoverParcel();
			if (parcelp)
			{
				fullname = parcelp->getName();
				LLStringUtil::trim(fullname);
				if (!fullname.empty())
				{
					msg += mParcelPrefix + fullname + "\n";
				}
			}
			if (parcelp && parcelp->getOwnerID().notNull() && gCacheNamep &&
//MK
				(!gRLenabled || !gRLInterface.mContainsShownames) &&
//mk
				gCacheNamep->getFullName(parcelp->getOwnerID(), fullname))
			{
				if (!fullname.empty())	// Can be empty for group-owned parcels
				{
					msg += mOwnerPrefix + fullname + "\n";
				}
			}
		}
	}

	msg += mMapToolTip;

	return true;
}

//virtual
bool LLPanelMiniMap::handleScrollWheel(S32 x, S32 y, S32 clicks)
{
	// Note that clicks are reversed from what you would think, i.e. > 0  means
	// zoom out and < 0 means zoom in.
	F32 scale = mScale * powf(MAP_SCALE_ZOOM_FACTOR, -clicks);
	setScale(llclamp(scale, MAP_SCALE_MIN, MAP_SCALE_MAX));
	return true;
}

//virtual
bool LLPanelMiniMap::handleMouseDown(S32 x, S32 y, MASK mask)
{
	if (!(mask & MASK_SHIFT))
	{
		return false;
	}

	// Start panning
	gFocusMgr.setMouseCapture(this);

	mMouseDownPanX = ll_round(mCurPanX);
	mMouseDownPanY = ll_round(mCurPanY);
	mMouseDownX = x;
	mMouseDownY = y;

	return true;
}

//virtual
bool LLPanelMiniMap::handleMouseUp(S32 x, S32 y, MASK mask)
{
	if (!hasMouseCapture())
	{
		return false;
	}

	if (mPanning)
	{
		// Restore mouse cursor
		S32 local_x = mMouseDownX + llfloor(mCurPanX - mMouseDownPanX);
		S32 local_y = mMouseDownY + llfloor(mCurPanY - mMouseDownPanY);
		LLRect clip_rect = getRect();
		clip_rect.stretch(-8);
		clip_rect.clipPointToRect(mMouseDownX, mMouseDownY, local_x, local_y);
		LLUI::setCursorPositionLocal(this, local_x, local_y);

		// Finish the pan
		mPanning = false;
		mMouseDownX = mMouseDownY =0;

		// Auto-centre
		mTargetPanX = mTargetPanY = 0;
	}

	gViewerWindowp->showCursor();
	gFocusMgr.setMouseCapture(NULL);

	return true;
}

//static
bool LLPanelMiniMap::outsideSlop(S32 x, S32 y, S32 start_x, S32 start_y,
								 S32 slop)
{
	S32 dx = x - start_x;
	S32 dy = y - start_y;

	return dx <= -slop || slop <= dx || dy <= -slop || slop <= dy;
}

//virtual
bool LLPanelMiniMap::handleHover(S32 x, S32 y, MASK mask)
{
	if (hasMouseCapture())
	{
		if (mPanning ||
			outsideSlop(x, y, mMouseDownX, mMouseDownY, MOUSE_DRAG_SLOP))
		{
			if (!mPanning)
			{
				// just started panning, so hide cursor
				mPanning = true;
				gViewerWindowp->hideCursor();
			}

			F32 delta_x = (F32)gViewerWindowp->getCurrentMouseDX();
			F32 delta_y = (F32)gViewerWindowp->getCurrentMouseDY();

			// Set pan to value at start of drag + offset
			mCurPanX += delta_x;
			mCurPanY += delta_y;
			mTargetPanX = mCurPanX;
			mTargetPanY = mCurPanY;

			gViewerWindowp->moveCursorToCenter();
		}

		// It does not really matter: cursor should be hidden
		gViewerWindowp->setCursor(UI_CURSOR_TOOLPAN);
	}
	else if (mask & MASK_SHIFT)
	{
		// If shift is held, change the cursor to hint that the map can be
		// dragged
		gViewerWindowp->setCursor(UI_CURSOR_TOOLPAN);
	}
	else
	{
		gViewerWindowp->setCursor(UI_CURSOR_CROSS);
	}

	return true;
}

//virtual
bool LLPanelMiniMap::handleDoubleClick(S32 x, S32 y, MASK mask)
{
	LLVector3d pos_global = viewPosToGlobal(x, y, sMiniMapRotate);
	bool new_target = false;
	if (!gTracker.isTracking() && gFloaterWorldMapp)
	{
		gFloaterWorldMapp->trackLocation(pos_global);
		new_target = true;
	}

	if (mask == MASK_CONTROL
//MK
		&& !(gRLenabled && gRLInterface.contains("tploc")))
//mk
	{
		gAgent.teleportViaLocationLookAt(pos_global);
	}
	else
	{
		LLFloaterWorldMap::show(NULL, new_target);
	}

	return true;
}

//virtual
bool LLPanelMiniMap::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
	mClosestAgentAtLastRightClick = mClosestAgentToCursor;
	mPosGlobaltAtLastRightClick = viewPosToGlobal(x, y, sMiniMapRotate);
	LLMenuGL* menu = (LLMenuGL*)mPopupMenuHandle.get();
	if (menu)
	{
		menu->buildDrawLabels();
		menu->updateParent(gMenuHolderp);
		LLMenuGL::showPopup(this, menu, x, y);
	}
	return true;
}

bool LLPanelMiniMap::LLScaleMap::handleEvent(LLPointer<LLEvent>,
											 const LLSD& userdata)
{
	LLPanelMiniMap* self = mPtr;

	S32 level = userdata.asInteger();
	switch (level)
	{
		case 0:
			self->setScale(MAP_SCALE_MIN);
			break;

		case 1:
			self->setScale(MAP_SCALE_MID);
			break;

		case 2:
			self->setScale(MAP_SCALE_MAX);
			break;

		default:
			break;
	}

	return true;
}

bool LLPanelMiniMap::LLCenterMap::handleEvent(LLPointer<LLEvent>,
											  const LLSD& userdata)
{
	EMiniMapCenter center = (EMiniMapCenter)userdata.asInteger();

	if (gSavedSettings.getS32("MiniMapCenter") == center)
	{
		gSavedSettings.setS32("MiniMapCenter", MAP_CENTER_NONE);
	}
	else
	{
		gSavedSettings.setS32("MiniMapCenter", userdata.asInteger());
	}

	return true;
}

bool LLPanelMiniMap::LLCheckCenterMap::handleEvent(LLPointer<LLEvent>,
												   const LLSD& userdata)
{
	LLPanelMiniMap* self = mPtr;
	EMiniMapCenter center = (EMiniMapCenter)userdata["data"].asInteger();
	bool enabled = gSavedSettings.getS32("MiniMapCenter") == center;
	self->findControl(userdata["control"].asString())->setValue(enabled);
	return true;
}

bool LLPanelMiniMap::LLRotateMap::handleEvent(LLPointer<LLEvent>, const LLSD&)
{
	gSavedSettings.setBool("MiniMapRotate",
						   !gSavedSettings.getBool("MiniMapRotate"));
	return true;
}

bool LLPanelMiniMap::LLCheckRotateMap::handleEvent(LLPointer<LLEvent>,
												   const LLSD& userdata)
{
	LLPanelMiniMap* self = mPtr;
	bool enabled = gSavedSettings.getBool("MiniMapRotate");
	self->findControl(userdata["control"].asString())->setValue(enabled);
	return true;
}

bool LLPanelMiniMap::LLDrawWater::handleEvent(LLPointer<LLEvent>, const LLSD&)
{
	gSavedSettings.setBool("MiniMapDrawWater",
						   !gSavedSettings.getBool("MiniMapDrawWater"));
	return true;
}

bool LLPanelMiniMap::LLCheckDrawWater::handleEvent(LLPointer<LLEvent>,
												   const LLSD& userdata)
{
	LLPanelMiniMap* self = mPtr;
	bool enabled = gSavedSettings.getBool("MiniMapDrawWater");
	self->findControl(userdata["control"].asString())->setValue(enabled);
	return true;
}

bool LLPanelMiniMap::LLDrawObjects::handleEvent(LLPointer<LLEvent>,
												const LLSD&)
{
	gSavedSettings.setBool("MiniMapDrawObjects",
						   !gSavedSettings.getBool("MiniMapDrawObjects"));
	LLPanelMiniMap* self = mPtr;
	self->mUpdateObjectImage = true;
	return true;
}

bool LLPanelMiniMap::LLCheckDrawObjects::handleEvent(LLPointer<LLEvent>,
													 const LLSD& userdata)
{
	LLPanelMiniMap* self = mPtr;
	bool enabled = gSavedSettings.getBool("MiniMapDrawObjects");
	self->findControl(userdata["control"].asString())->setValue(enabled);
	return true;
}

bool LLPanelMiniMap::LLPlotPuppets::handleEvent(LLPointer<LLEvent>,
												const LLSD&)
{
	gSavedSettings.setBool("MiniMapPlotPuppets",
						   !gSavedSettings.getBool("MiniMapPlotPuppets"));
	LLPanelMiniMap* self = mPtr;
	self->mUpdateObjectImage = true;
	return true;
}

bool LLPanelMiniMap::LLCheckPlotPuppets::handleEvent(LLPointer<LLEvent>,
													 const LLSD& userdata)
{
	LLPanelMiniMap* self = mPtr;
	bool enabled = gSavedSettings.getBool("MiniMapPlotPuppets");
	self->findControl(userdata["control"].asString())->setValue(enabled);
	return true;
}

bool LLPanelMiniMap::LLPlotChars::handleEvent(LLPointer<LLEvent>, const LLSD&)
{
	gSavedSettings.setBool("MiniMapPlotCharacters",
						   !gSavedSettings.getBool("MiniMapPlotCharacters"));
	LLPanelMiniMap* self = mPtr;
	self->mUpdateObjectImage = true;
	return true;
}

bool LLPanelMiniMap::LLCheckPlotChars::handleEvent(LLPointer<LLEvent>,
												   const LLSD& userdata)
{
	LLPanelMiniMap* self = mPtr;
	bool enabled = gSavedSettings.getBool("MiniMapPlotCharacters") &&
				   gSavedSettings.getBool("MiniMapDrawObjects");
	self->findControl(userdata["control"].asString())->setValue(enabled);
	return true;
}

bool LLPanelMiniMap::LLEnablePlotChars::handleEvent(LLPointer<LLEvent>,
													const LLSD& userdata)
{
	LLPanelMiniMap* self = mPtr;
	bool enable = gSavedSettings.getBool("MiniMapDrawObjects");
	self->findControl(userdata["control"].asString())->setValue(enable);
	return true;
}

bool LLPanelMiniMap::LLPlotPhysical::handleEvent(LLPointer<LLEvent>,
												 const LLSD&)
{
	gSavedSettings.setBool("MiniMapPlotPhysicalObj",
						   !gSavedSettings.getBool("MiniMapPlotPhysicalObj"));
	LLPanelMiniMap* self = mPtr;
	self->mUpdateObjectImage = true;
	return true;
}

bool LLPanelMiniMap::LLCheckPlotPhysical::handleEvent(LLPointer<LLEvent>,
													  const LLSD& userdata)
{
	LLPanelMiniMap* self = mPtr;
	bool enabled = gSavedSettings.getBool("MiniMapPlotPhysicalObj") &&
				   gSavedSettings.getBool("MiniMapDrawObjects");
	self->findControl(userdata["control"].asString())->setValue(enabled);
	return true;
}

bool LLPanelMiniMap::LLEnablePlotPhysical::handleEvent(LLPointer<LLEvent>,
													   const LLSD& userdata)
{
	LLPanelMiniMap* self = mPtr;
	bool enable = gSavedSettings.getBool("MiniMapDrawObjects");
	self->findControl(userdata["control"].asString())->setValue(enable);
	return true;
}

bool LLPanelMiniMap::LLDrawBorders::handleEvent(LLPointer<LLEvent>,
												const LLSD&)
{
	gSavedSettings.setBool("MiniMapDrawBorders",
						   !gSavedSettings.getBool("MiniMapDrawBorders"));
	return true;
}

bool LLPanelMiniMap::LLCheckDrawBorders::handleEvent(LLPointer<LLEvent>,
													 const LLSD& userdata)
{
	LLPanelMiniMap* self = mPtr;
	bool enabled = gSavedSettings.getBool("MiniMapDrawBorders");
	self->findControl(userdata["control"].asString())->setValue(enabled);
	return true;
}

bool LLPanelMiniMap::LLDrawBans::handleEvent(LLPointer<LLEvent>, const LLSD&)
{
	gSavedSettings.setBool("MinimapShowBannedParcels",
						   !gSavedSettings.getBool("MinimapShowBannedParcels"));
	return true;
}

bool LLPanelMiniMap::LLCheckDrawBans::handleEvent(LLPointer<LLEvent>,
													 const LLSD& userdata)
{
	LLPanelMiniMap* self = mPtr;
	bool enabled = gSavedSettings.getBool("MinimapShowBannedParcels");
	self->findControl(userdata["control"].asString())->setValue(enabled);
	return true;
}

bool LLPanelMiniMap::LLDrawParcels::handleEvent(LLPointer<LLEvent>, const LLSD&)
{
	gSavedSettings.setBool("MinimapShowParcelBorders",
						   !gSavedSettings.getBool("MinimapShowParcelBorders"));
	LLPanelMiniMap* self = mPtr;
	self->mUpdateParcelImage = true;
	return true;
}

bool LLPanelMiniMap::LLCheckDrawParcels::handleEvent(LLPointer<LLEvent>,
													 const LLSD& userdata)
{
	LLPanelMiniMap* self = mPtr;
	bool enabled = gSavedSettings.getBool("MinimapShowParcelBorders");
	self->findControl(userdata["control"].asString())->setValue(enabled);
	return true;
}

bool LLPanelMiniMap::LLShowParcelInfo::handleEvent(LLPointer<LLEvent>,
												   const LLSD&)
{
	LLPanelMiniMap* self = mPtr;
	if (!self->mPosGlobaltAtLastRightClick.isExactlyZero() &&
//MK
		(!gRLenabled || !gRLInterface.mContainsShowloc))
//mk
	{
		gViewerParcelMgr.selectParcelAt(self->mPosGlobaltAtLastRightClick);
		LLFloaterLand::showInstance();
	}
	return true;
}

bool LLPanelMiniMap::LLEnableParcelInfo::handleEvent(LLPointer<LLEvent>,
													 const LLSD& userdata)
{
	LLPanelMiniMap* self = mPtr;
//MK
	if (gRLenabled && gRLInterface.mContainsShowloc)
	{
		self->findControl(userdata["control"].asString())->setValue(false);
	}
	else
	{
//mk
		bool enabled = self->mHasDrawnParcels;
		self->findControl(userdata["control"].asString())->setValue(enabled);
//MK
	}
//mk
	return true;
}

bool LLPanelMiniMap::LLRefreshTerrain::handleEvent(LLPointer<LLEvent>,
												   const LLSD&)
{
	gWorld.reloadAllSurfacePatches();
	return true;
}

bool LLPanelMiniMap::LLStopTracking::handleEvent(LLPointer<LLEvent>,
												 const LLSD&)
{
	gTracker.stopTracking();
	return true;
}

bool LLPanelMiniMap::LLEnableTracking::handleEvent(LLPointer<LLEvent>,
												   const LLSD& userdata)
{
	LLPanelMiniMap* self = mPtr;
	bool enabled = gTracker.isTracking();
	self->findControl(userdata["control"].asString())->setValue(enabled);
	return true;
}

bool LLPanelMiniMap::LLShowAgentProfile::handleEvent(LLPointer<LLEvent>,
													 const LLSD&)
{
	LLPanelMiniMap* self = mPtr;
	LLFloaterAvatarInfo::show(self->mClosestAgentAtLastRightClick);
	return true;
}

bool LLPanelMiniMap::LLEnableProfile::handleEvent(LLPointer<LLEvent>,
												  const LLSD& userdata)
{
	LLPanelMiniMap* self = mPtr;
//MK
	if (gRLenabled &&
		(gRLInterface.mContainsShownames ||
		 gRLInterface.mContainsShowNearby ||
		 gRLInterface.mContainsShownametags))
	{
		self->findControl(userdata["control"].asString())->setValue(false);
	}
	else
	{
//mk
		bool enabled = self->isAgentUnderCursor();
		self->findControl(userdata["control"].asString())->setValue(enabled);
//MK
	}
//mk
	return true;
}
