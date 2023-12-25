/**
 * @file lltoolbrushland.cpp
 * @brief Implementation of the LLToolBrushLand class
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

#include "lltoolbrushland.h"

#include "llcallbacklist.h"
#include "llgl.h"
#include "llparcel.h"
#include "llrender.h"
#include "llmessage.h"

#include "llagent.h"
#include "llappviewer.h"
#include "llfloatertools.h"
#include "llstatusbar.h"
#include "llsurface.h"
#include "llsurfacepatch.h"
#include "lltoolmgr.h"
#include "lltoolselectland.h"
#include "llviewercontrol.h"
#include "llviewerparcelmgr.h"
#include "llviewerparceloverlay.h"
#include "llviewerregion.h"
#include "llviewerwindow.h"
#include "llworld.h"

constexpr S32 LAND_BRUSH_SIZE_COUNT = 3;
static const F32 LAND_BRUSH_SIZE[LAND_BRUSH_SIZE_COUNT] = { 1.f, 2.f, 4.f };
const LLColor4 OVERLAY_COLOR(1.f, 1.f, 1.f, 1.f);

enum
{
	E_LAND_LEVEL	= 0,
	E_LAND_RAISE	= 1,
	E_LAND_LOWER	= 2,
	E_LAND_SMOOTH	= 3,
	E_LAND_NOISE	= 4,
	E_LAND_REVERT	= 5,
	E_LAND_INVALID 	= 6,
};

///////////////////////////////////////////////////////////////////////////////
// Helper LLRegionPosition class
///////////////////////////////////////////////////////////////////////////////

class LLRegionPosition
{
protected:
	LOG_CLASS(LLRegionPosition);

public:
	LLRegionPosition()
	:	mRegionp(NULL),
		mPositionRegion(LLVector3(0.f, 0.f, 0.f))
	{
	}

	LLRegionPosition(LLViewerRegion* regionp, const LLVector3& position)
	:	mRegionp(regionp),
		mPositionRegion(position)
	{
	}

	// From global coords ONLY !
	LLRegionPosition(const LLVector3d& global_position)
	{
		setPositionGlobal(global_position);
	}

	LL_INLINE LLViewerRegion* getRegion() const		{ return mRegionp; }

	void setPositionGlobal(const LLVector3d& global_pos);
	LLVector3d getPositionGlobal() const;

	LL_INLINE const LLVector3& getPositionRegion() const
	{
		return mPositionRegion;
	}

	LL_INLINE const LLVector3 getPositionAgent() const
	{
		return mRegionp->getPosAgentFromRegion(mPositionRegion);
	}

	LL_INLINE void clear()
	{
		mRegionp = NULL;
		mPositionRegion.clear();
	}

public:
	LLVector3		mPositionRegion;

private:
	LLViewerRegion*	mRegionp;
};

LLVector3d LLRegionPosition::getPositionGlobal() const
{
	if (mRegionp)
	{
		return mRegionp->getPosGlobalFromRegion(mPositionRegion);
	}
	return LLVector3d(mPositionRegion);
}

void LLRegionPosition::setPositionGlobal(const LLVector3d& position_global)
{
	mRegionp = gWorld.getRegionFromPosGlobal(position_global);
	if (!mRegionp)
	{
		mRegionp = gAgent.getRegion();
		if (!mRegionp)
		{
			llwarns << "NULL agent region !  Position not set." << llendl;
			return;
		}
	}
	mPositionRegion = mRegionp->getPosRegionFromGlobal(position_global);
}

///////////////////////////////////////////////////////////////////////////////
// LLToolBrushLand class
///////////////////////////////////////////////////////////////////////////////

LLToolBrushLand gToolBrushLand;

// constructor
LLToolBrushLand::LLToolBrushLand()
:	LLTool("Land"),
	mStartingZ(0.f),
	mMouseX(0),
	mMouseY(0),
	mGotHover(false),
	mBrushSelected(false)
{
}

U8 LLToolBrushLand::getBrushIndex()
{
	static LLCachedControl<F32> land_brush_size(gSavedSettings,
												"LandBrushSize");
	// Find the best index for desired size (compatibility with old sims,
	// brush_index is now depricated - DEV-8252)
	U8 index = 0;
	for (U8 i = 0; i < LAND_BRUSH_SIZE_COUNT; ++i)
	{
		if (land_brush_size > LAND_BRUSH_SIZE[i])
		{
			index = i;
		}
	}

	return index;
}

void LLToolBrushLand::modifyLandAtPointGlobal(const LLVector3d& pos_global,
											  MASK mask)
{
	static LLCachedControl<S32> radio_action(gSavedSettings,
											 "RadioLandBrushAction");
	static LLCachedControl<F32> land_brush_size(gSavedSettings,
												"LandBrushSize");

	mLastAffectedRegions.clear();
	determineAffectedRegions(mLastAffectedRegions, pos_global);
	for (region_list_t::iterator iter = mLastAffectedRegions.begin(),
								 end = mLastAffectedRegions.end();
		 iter != end; ++iter)
	{
		LLViewerRegion* regionp = *iter;
#if 0
		bool is_changed = false;
#endif
		LLVector3 pos_region = regionp->getPosRegionFromGlobal(pos_global);
		LLSurface& land = regionp->getLand();
		char action = E_LAND_LEVEL;
		switch (radio_action)
		{
			case 0:
				// average toward mStartingZ
				action = E_LAND_LEVEL;
				break;

			case 1:
				action = E_LAND_RAISE;
				break;

			case 2:
				action = E_LAND_LOWER;
				break;

			case 3:
				action = E_LAND_SMOOTH;
				break;

			case 4:
				action = E_LAND_NOISE;
				break;

			case 5:
				action = E_LAND_REVERT;
				break;

			default:
				action = E_LAND_INVALID;
		}

#if 0
		// Don't send a message to the region if nothing changed.
		if (!is_changed) continue;
#endif

		// Now to update the patch information so it will redraw correctly.
		LLSurfacePatch *patchp = land.resolvePatchRegion(pos_region);
		if (patchp)
		{
			patchp->dirtyZ();
		}

		// Also force the property lines to update, normals to recompute, etc.
		regionp->forceUpdate();

		// tell the simulator what we've done
		F32 seconds = gSavedSettings.getF32("LandBrushForce") / gFPSClamped;
		F32 x_pos = (F32)pos_region.mV[VX];
		F32 y_pos = (F32)pos_region.mV[VY];
		LLMessageSystem* msg = gMessageSystemp;
		msg->newMessageFast(_PREHASH_ModifyLand);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
		msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
		msg->nextBlockFast(_PREHASH_ModifyBlock);
		msg->addU8Fast(_PREHASH_Action, (U8)action);
		msg->addU8Fast(_PREHASH_BrushSize, getBrushIndex());
		msg->addF32Fast(_PREHASH_Seconds, seconds);
		msg->addF32Fast(_PREHASH_Height, mStartingZ);
		msg->nextBlockFast(_PREHASH_ParcelData);
		msg->addS32Fast(_PREHASH_LocalID, -1);
		msg->addF32Fast(_PREHASH_West, x_pos);
		msg->addF32Fast(_PREHASH_South, y_pos);
		msg->addF32Fast(_PREHASH_East, x_pos);
		msg->addF32Fast(_PREHASH_North, y_pos);
		msg->nextBlock("ModifyBlockExtended");
		msg->addF32("BrushSize", land_brush_size);
		msg->sendMessage(regionp->getHost());
	}
}

void LLToolBrushLand::modifyLandInSelectionGlobal()
{
	static LLCachedControl<F32> land_brush_size(gSavedSettings,
												"LandBrushSize");

	if (gViewerParcelMgr.selectionEmpty() ||
		// Selecting land; do not do anything
		gToolMgr.isCurrentTool(&gToolSelectLand))
	{
		return;
	}

	LLVector3d min;
	LLVector3d max;
	gViewerParcelMgr.getSelection(min, max);

	S32 radio_action = gSavedSettings.getS32("RadioLandBrushAction");

	mLastAffectedRegions.clear();

	determineAffectedRegions(mLastAffectedRegions,
							 LLVector3d(min.mdV[VX], min.mdV[VY], 0));
	determineAffectedRegions(mLastAffectedRegions,
							 LLVector3d(min.mdV[VX], max.mdV[VY], 0));
	determineAffectedRegions(mLastAffectedRegions,
							 LLVector3d(max.mdV[VX], min.mdV[VY], 0));
	determineAffectedRegions(mLastAffectedRegions,
							 LLVector3d(max.mdV[VX], max.mdV[VY], 0));

	LLRegionPosition mid_point_region((min + max) * 0.5);
	LLViewerRegion* center_region = mid_point_region.getRegion();
	if (center_region)
	{
		LLVector3 pos_region = mid_point_region.getPositionRegion();
		U32 grids = center_region->getLand().mGridsPerEdge;
		S32 i = llclamp((S32)pos_region.mV[VX], 0, (S32)grids);
		S32 j = llclamp((S32)pos_region.mV[VY], 0, (S32)grids);
		mStartingZ = center_region->getLand().getZ(i + j * grids);
	}
	else
	{
		mStartingZ = 0.f;
	}

	// Stop if our selection include a no-terraform region
	for (region_list_t::iterator iter = mLastAffectedRegions.begin();
		iter != mLastAffectedRegions.end(); ++iter)
	{
		LLViewerRegion* regionp = *iter;
		if (!canTerraform(regionp))
		{
			alertNoTerraform(regionp);
			return;
		}
	}

	for (region_list_t::iterator iter = mLastAffectedRegions.begin(),
								 end = mLastAffectedRegions.end();
		iter != end; ++iter)
	{
		LLViewerRegion* regionp = *iter;
#if 0
		bool is_changed = false;
#endif
		LLVector3 min_region = regionp->getPosRegionFromGlobal(min);
		LLVector3 max_region = regionp->getPosRegionFromGlobal(max);

		min_region.clamp(0.f, regionp->getWidth());
		max_region.clamp(0.f, regionp->getWidth());
		F32 seconds = gSavedSettings.getF32("LandBrushForce");

		LLSurface &land = regionp->getLand();
		char action = E_LAND_LEVEL;
		switch (radio_action)
		{
			case 0:
				// average toward mStartingZ
				action = E_LAND_LEVEL;
				seconds *= 0.25f;
				break;

			case 1:
				action = E_LAND_RAISE;
				seconds *= 0.25f;
				break;

			case 2:
				action = E_LAND_LOWER;
				seconds *= 0.25f;
				break;

			case 3:
				action = E_LAND_SMOOTH;
				seconds *= 5.0f;
				break;

			case 4:
				action = E_LAND_NOISE;
				seconds *= 0.5f;
				break;

			case 5:
				action = E_LAND_REVERT;
				seconds = 0.5f;
				break;

			default:
#if 0
				action = E_LAND_INVALID;
				seconds = 0.0f;
#endif
				return;
		}

#if 0
		// Don't send a message to the region if nothing changed.
		if (!is_changed) continue;
#endif

		// Now to update the patch information so it will redraw correctly.
		LLSurfacePatch *patchp= land.resolvePatchRegion(min_region);
		if (patchp)
		{
			patchp->dirtyZ();
		}

		// Also force the property lines to update, normals to recompute, etc.
		regionp->forceUpdate();

		// tell the simulator what we've done
		LLMessageSystem* msg = gMessageSystemp;
		msg->newMessageFast(_PREHASH_ModifyLand);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
		msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
		msg->nextBlockFast(_PREHASH_ModifyBlock);
		msg->addU8Fast(_PREHASH_Action, (U8)action);
		msg->addU8Fast(_PREHASH_BrushSize, getBrushIndex());
		msg->addF32Fast(_PREHASH_Seconds, seconds);
		msg->addF32Fast(_PREHASH_Height, mStartingZ);

		bool parcel_selected =
			gViewerParcelMgr.getParcelSelection()->getWholeParcelSelected();
		LLParcel* selected_parcel =
			gViewerParcelMgr.getParcelSelection()->getParcel();

		if (parcel_selected && selected_parcel)
		{
			msg->nextBlockFast(_PREHASH_ParcelData);
			msg->addS32Fast(_PREHASH_LocalID, selected_parcel->getLocalID());
			msg->addF32Fast(_PREHASH_West,  min_region.mV[VX]);
			msg->addF32Fast(_PREHASH_South, min_region.mV[VY]);
			msg->addF32Fast(_PREHASH_East,  max_region.mV[VX]);
			msg->addF32Fast(_PREHASH_North, max_region.mV[VY]);
		}
		else
		{
			msg->nextBlockFast(_PREHASH_ParcelData);
			msg->addS32Fast(_PREHASH_LocalID, -1);
			msg->addF32Fast(_PREHASH_West,  min_region.mV[VX]);
			msg->addF32Fast(_PREHASH_South, min_region.mV[VY]);
			msg->addF32Fast(_PREHASH_East,  max_region.mV[VX]);
			msg->addF32Fast(_PREHASH_North, max_region.mV[VY]);
		}

		msg->nextBlock("ModifyBlockExtended");
		msg->addF32("BrushSize", land_brush_size);

		msg->sendMessage(regionp->getHost());
	}
}

void LLToolBrushLand::brush()
{
	LLVector3d spot;
	if (gKeyboardp &&
		gViewerWindowp->mousePointOnLandGlobal(mMouseX, mMouseY, &spot))
	{
		// Round to nearest X,Y grid
		spot.mdV[VX] = floor(spot.mdV[VX] + 0.5);
		spot.mdV[VY] = floor(spot.mdV[VY] + 0.5);
		modifyLandAtPointGlobal(spot, gKeyboardp->currentMask(true));
	}
}

bool LLToolBrushLand::handleMouseDown(S32 x, S32 y, MASK mask)
{
	bool handled = false;

	LLVector3d spot;
	// Find the z value of the initial click.
	if (gViewerWindowp->mousePointOnLandGlobal(x, y, &spot))
	{
		// Round to nearest X,Y grid
		spot.mdV[VX] = floor(spot.mdV[VX] + 0.5);
		spot.mdV[VY] = floor(spot.mdV[VY] + 0.5);

		LLRegionPosition region_position(spot);
		LLViewerRegion* regionp = region_position.getRegion();

		if (!canTerraform(regionp))
		{
			alertNoTerraform(regionp);
			return true;
		}

		LLVector3 pos_region = region_position.getPositionRegion();
		U32 grids = regionp->getLand().mGridsPerEdge;
		S32 i = llclamp((S32)pos_region.mV[VX], 0, (S32)grids);
		S32 j = llclamp((S32)pos_region.mV[VY], 0, (S32)grids);
		mStartingZ = regionp->getLand().getZ(i + j * grids);
		mMouseX = x;
		mMouseY = y;
		gIdleCallbacks.addFunction(onIdle, this);
		setMouseCapture(true);

		gViewerParcelMgr.setSelectionVisible(false);
		handled = true;
	}

	return handled;
}

bool LLToolBrushLand::handleHover(S32 x, S32 y, MASK mask)
{
	LL_DEBUGS("UserInput") << "hover handled by LLToolBrushLand ("
						   << (hasMouseCapture() ? "active":"inactive") << ")"
						   << LL_ENDL;
	mMouseX = x;
	mMouseY = y;
	mGotHover = true;
	gWindowp->setCursor(UI_CURSOR_TOOLLAND);
	return true;
}

bool LLToolBrushLand::handleMouseUp(S32 x, S32 y, MASK mask)
{
	bool handled = false;
	mLastAffectedRegions.clear();
	if (hasMouseCapture())
	{
		// Release the mouse
		setMouseCapture(false);

		gViewerParcelMgr.setSelectionVisible(true);

		gIdleCallbacks.deleteFunction(onIdle, this);
		handled = true;
	}

	return handled;
}

void LLToolBrushLand::handleSelect()
{
	grabMenuHandler();

	if (gFloaterToolsp)
	{
		gFloaterToolsp->setStatusText("modifyland");
	}

	mBrushSelected = true;
}

void LLToolBrushLand::handleDeselect()
{
	releaseMenuHandler();
	gViewerParcelMgr.setSelectionVisible(true);
	mBrushSelected = false;
}

// Draw the area that will be affected.
void LLToolBrushLand::render()
{
	static LLCachedControl<F32> land_brush_size(gSavedSettings,
												"LandBrushSize");
	if (mGotHover && gAgent.getRegion())
	{
		LLVector3d spot;
		if (gViewerWindowp->mousePointOnLandGlobal(mMouseX, mMouseY, &spot))
		{
			spot.mdV[VX] = floor(spot.mdV[VX] + 0.5);
			spot.mdV[VY] = floor(spot.mdV[VY] + 0.5);

			region_list_t regions;
			determineAffectedRegions(regions, spot);

			// Now, for each region, render the overlay
			LLVector3 pos_world = gAgent.getRegion()->getPosRegionFromGlobal(spot);
			for (region_list_t::iterator iter = regions.begin(),
										 end = regions.end();
				 iter != end; ++iter)
			{
				LLViewerRegion* region = *iter;
				renderOverlay(region->getLand(),
							  region->getPosRegionFromGlobal(spot), pos_world);
			}
		}
		mGotHover = false;
	}
}

/*
 * Draw vertical lines from each vertex straight up in world space
 * with lengths indicating the current "strength" slider.
 * Decorate the tops and bottoms of the lines like this:
 *
 *		Raise        Revert
 *		/|\           ___
 *		 |             |
 *		 |             |
 *
 *		Rough        Smooth
 *		/|\           ___
 *		 |             |
 *		 |             |
 *		\|/..........._|_
 *
 *		Lower        Flatten
 *		 |             |
 *		 |             |
 *		\|/..........._|_
 */
void LLToolBrushLand::renderOverlay(LLSurface& land,
									const LLVector3& pos_region,
									const LLVector3& pos_world)
{
	static LLCachedControl<F32> land_brush_size(gSavedSettings,
												"LandBrushSize");
	static LLCachedControl<S32> radio_action(gSavedSettings,
											 "RadioLandBrushAction");
	static LLCachedControl<F32> force(gSavedSettings, "LandBrushForce");

	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
	LLGLDepthTest mDepthTest(GL_TRUE);
	gGL.pushMatrix();
	gGL.color4fv(OVERLAY_COLOR.mV);
	gGL.translatef(0.f, 0.f, 1.f);

	S32 i = (S32) pos_region.mV[VX];
	S32 j = (S32) pos_region.mV[VY];
	S32 half_edge = llfloor(land_brush_size);

	constexpr F32 tic = .075f; // arrowhead size
	gGL.begin(LLRender::LINES);
	for (S32 di = -half_edge; di <= half_edge; ++di)
	{
		if (i + di < 0 || i + di >= (S32)land.mGridsPerEdge) continue;
		for (S32 dj = -half_edge; dj <= half_edge; dj++)
		{
			if (j + dj < 0 || j + dj >= (S32)land.mGridsPerEdge) continue;
			const F32 wx = pos_world.mV[VX] + di;
			const F32 wy = pos_world.mV[VY] + dj;
			const F32 wz = land.getZ(i + di + (j + dj) * land.mGridsPerEdge);
			const F32 norm_dist = sqrtf((F32)(di * di + dj * dj)) / half_edge;
			// 1 at center, 0 at corner
			const F32 force_scale = F_SQRT2 - norm_dist;
			// top vertex
			const F32 wz2 = wz + 0.2f + (0.2f + force * 0.01f) * force_scale;
			// vertical line
			gGL.vertex3f(wx, wy, wz);
			gGL.vertex3f(wx, wy, wz2);
			if (radio_action == E_LAND_RAISE || radio_action == E_LAND_NOISE)
			{
				// up arrow
				gGL.vertex3f(wx, wy, wz2);
				gGL.vertex3f(wx + tic, wy, wz2 - tic);
				gGL.vertex3f(wx, wy, wz2);
				gGL.vertex3f(wx - tic, wy, wz2 - tic);
			}
			if (radio_action == E_LAND_LOWER || radio_action == E_LAND_NOISE)
			{
				// down arrow
				gGL.vertex3f(wx, wy, wz);
				gGL.vertex3f(wx + tic, wy, wz + tic);
				gGL.vertex3f(wx, wy, wz);
				gGL.vertex3f(wx - tic, wy, wz + tic);
			}
			if (radio_action == E_LAND_REVERT || radio_action == E_LAND_SMOOTH)
			{
				// flat top
				gGL.vertex3f(wx - tic, wy, wz2);
				gGL.vertex3f(wx + tic, wy, wz2);
			}
			if (radio_action == E_LAND_LEVEL || radio_action == E_LAND_SMOOTH)
			{
				// flat bottom
				gGL.vertex3f(wx - tic, wy, wz);
				gGL.vertex3f(wx + tic, wy, wz);
			}
		}
	}
	gGL.end();

	gGL.popMatrix();
}

void LLToolBrushLand::determineAffectedRegions(region_list_t& regions,
											   const LLVector3d& spot) const
{
	static LLCachedControl<F32> land_brush_size(gSavedSettings,
												"LandBrushSize");
	LLVector3d corner(spot);
	corner.mdV[VX] -= land_brush_size * 0.5f;
	corner.mdV[VY] -= land_brush_size * 0.5f;
	LLViewerRegion* region = NULL;
	region = gWorld.getRegionFromPosGlobal(corner);
	if (region && regions.find(region) == regions.end())
	{
		regions.insert(region);
	}
	corner.mdV[VY] += land_brush_size;
	region = gWorld.getRegionFromPosGlobal(corner);
	if (region && regions.find(region) == regions.end())
	{
		regions.insert(region);
	}
	corner.mdV[VX] += land_brush_size;
	region = gWorld.getRegionFromPosGlobal(corner);
	if (region && regions.find(region) == regions.end())
	{
		regions.insert(region);
	}
	corner.mdV[VY] -= land_brush_size;
	region = gWorld.getRegionFromPosGlobal(corner);
	if (region && regions.find(region) == regions.end())
	{
		regions.insert(region);
	}
}

//static
void LLToolBrushLand::onIdle(void* user_data)
{
	LLToolBrushLand* self = (LLToolBrushLand*)user_data;
	if (gToolMgr.isCurrentTool(self))
	{
		self->brush();
	}
	else
	{
		gIdleCallbacks.deleteFunction(onIdle, self);
	}
}

void LLToolBrushLand::onMouseCaptureLost()
{
	gIdleCallbacks.deleteFunction(onIdle, this);
}

//static
void LLToolBrushLand::undo()
{
	LLMessageSystem* msg = gMessageSystemp;
	for (region_list_t::iterator iter = mLastAffectedRegions.begin();
		 iter != mLastAffectedRegions.end(); ++iter)
	{
		LLViewerRegion* regionp = *iter;
		msg->newMessageFast(_PREHASH_UndoLand);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
		msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
		msg->sendMessage(regionp->getHost());
	}
}

#if 0
//static
void LLToolBrushLand::redo()
{
	LLMessageSystem* msg = gMessageSystemp;
	for (region_list_t::iterator iter = mLastAffectedRegions.begin();
		iter != mLastAffectedRegions.end(); ++iter)
	{
		LLViewerRegion* regionp = *iter;
		msg->newMessageFast(_PREHASH_RedoLand);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
		msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
		msg->sendMessage(regionp->getHost());
	}
}
#endif

//static
bool LLToolBrushLand::canTerraform(LLViewerRegion* regionp) const
{
	if (!regionp) return false;
	if (regionp->canManageEstate()) return true;
	return !regionp->getRegionFlag(REGION_FLAGS_BLOCK_TERRAFORM);
}

//static
void LLToolBrushLand::alertNoTerraform(LLViewerRegion* regionp)
{
	if (!regionp) return;

	LLSD args;
	args["REGION"] = regionp->getName();
	gNotifications.add("RegionNoTerraforming", args);
}
