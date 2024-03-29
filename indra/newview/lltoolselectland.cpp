/**
 * @file lltoolselectland.cpp
 * @brief LLToolSelectLand class implementation
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 *
 * Copyright (c) 2002-2009, Linden Research, Inc.
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

#include "lltoolselectland.h"

// indra includes
#include "llparcel.h"

// Viewer includes
#include "llagent.h"
#include "llviewercontrol.h"
#include "llfloatertools.h"
#include "llselectmgr.h"
#include "llstatusbar.h"
#include "lltoolview.h"
#include "llviewerparcelmgr.h"
#include "llviewerwindow.h"

LLToolSelectLand gToolSelectLand;

LLToolSelectLand::LLToolSelectLand()
:	LLTool("Parcel"),
	mDragEndValid(false),
	mMouseOutsideSlop(false),
	mDragStartX(0),
	mDragStartY(0),
	mDragEndX(0),
	mDragEndY(0)
{
}

bool LLToolSelectLand::handleMouseDown(S32 x, S32 y, MASK mask)
{
	bool hit_land = gViewerWindowp->mousePointOnLandGlobal(x, y,
														  &mDragStartGlobal);
	if (hit_land)
	{
		setMouseCapture(true);

		mDragStartX = x;
		mDragStartY = y;
		mDragEndX = x;
		mDragEndY = y;

		mDragEndValid = true;
		mDragEndGlobal = mDragStartGlobal;

		sanitize_corners(mDragStartGlobal, mDragEndGlobal, mWestSouthBottom,
						 mEastNorthTop);

		mWestSouthBottom -= LLVector3d(PARCEL_GRID_STEP_METERS / 2,
									   PARCEL_GRID_STEP_METERS / 2, 0);
		mEastNorthTop += LLVector3d(PARCEL_GRID_STEP_METERS / 2,
									PARCEL_GRID_STEP_METERS / 2, 0);

		roundXY(mWestSouthBottom);
		roundXY(mEastNorthTop);

		mMouseOutsideSlop = true; //false;

		gViewerParcelMgr.deselectLand();
	}

	return hit_land;
}

bool LLToolSelectLand::handleDoubleClick(S32 x, S32 y, MASK mask)
{
	LLVector3d pos_global;
	bool hit_land = gViewerWindowp->mousePointOnLandGlobal(x, y, &pos_global);
	if (hit_land)
	{
		// Auto-select this parcel
		gViewerParcelMgr.selectParcelAt(pos_global);
		return true;
	}
	return false;
}

bool LLToolSelectLand::handleMouseUp(S32 x, S32 y, MASK mask)
{
	if (hasMouseCapture())
	{
		setMouseCapture(false);

		if (mMouseOutsideSlop && mDragEndValid)
		{
			// Take the drag start and end locations, then map the southwest
			// point down to the next grid location, and the northeast point up
			// to the next grid location.

			sanitize_corners(mDragStartGlobal, mDragEndGlobal,
							 mWestSouthBottom, mEastNorthTop);

			mWestSouthBottom -= LLVector3d(PARCEL_GRID_STEP_METERS / 2,
										   PARCEL_GRID_STEP_METERS / 2, 0);
			mEastNorthTop += LLVector3d(PARCEL_GRID_STEP_METERS / 2,
										PARCEL_GRID_STEP_METERS / 2, 0);

			roundXY(mWestSouthBottom);
			roundXY(mEastNorthTop);

			// Don't auto-select entire parcel.
			mSelection = gViewerParcelMgr.selectLand(mWestSouthBottom,
													 mEastNorthTop, false);
		}

		mMouseOutsideSlop = false;
		mDragEndValid = false;

		return true;
	}
	return false;
}

bool LLToolSelectLand::handleHover(S32 x, S32 y, MASK mask)
{
	if (hasMouseCapture())
	{
		if (mMouseOutsideSlop || outsideSlop(x, y, mDragStartX, mDragStartY))
		{
			mMouseOutsideSlop = true;

			// Must do this every frame, in case the camera moved or the land
			// moved since last frame.

			// If doesn't hit land, doesn't change old value
			LLVector3d land_global;
			bool hit_land = gViewerWindowp->mousePointOnLandGlobal(x, y,
																  &land_global);
			if (hit_land)
			{
				mDragEndValid = true;
				mDragEndGlobal = land_global;

				sanitize_corners(mDragStartGlobal, mDragEndGlobal,
								 mWestSouthBottom, mEastNorthTop);

				mWestSouthBottom -= LLVector3d(PARCEL_GRID_STEP_METERS / 2,
											   PARCEL_GRID_STEP_METERS / 2, 0);
				mEastNorthTop += LLVector3d(PARCEL_GRID_STEP_METERS / 2,
											PARCEL_GRID_STEP_METERS / 2, 0);

				roundXY(mWestSouthBottom);
				roundXY(mEastNorthTop);

				LL_DEBUGS("UserInput") << "hover handled by LLToolSelectLand (active, land)"
									   << LL_ENDL;
				gWindowp->setCursor(UI_CURSOR_ARROW);
			}
			else
			{
				mDragEndValid = false;
				LL_DEBUGS("UserInput") << "hover handled by LLToolSelectLand (active, no land)"
									   << LL_ENDL;
				gWindowp->setCursor(UI_CURSOR_NO);
			}

			mDragEndX = x;
			mDragEndY = y;
		}
		else
		{
			LL_DEBUGS("UserInput") << "hover handled by LLToolSelectLand (active, in slop)"
								   << LL_ENDL;
			gWindowp->setCursor(UI_CURSOR_ARROW);
		}
	}
	else
	{
		LL_DEBUGS("UserInput") << "hover handled by LLToolSelectLand (inactive)"
							   << LL_ENDL;
		gWindowp->setCursor(UI_CURSOR_ARROW);
	}

	return true;
}

void LLToolSelectLand::render()
{
	if (hasMouseCapture() && /*mMouseOutsideSlop &&*/ mDragEndValid)
	{
		gViewerParcelMgr.renderRect(mWestSouthBottom, mEastNorthTop);
	}
}

void LLToolSelectLand::handleSelect()
{
	if (gFloaterToolsp)
	{
		gFloaterToolsp->setStatusText("selectland");
	}
}

void LLToolSelectLand::handleDeselect()
{
	mSelection = NULL;
}

void LLToolSelectLand::roundXY(LLVector3d &vec)
{
	vec.mdV[VX] = ll_round(vec.mdV[VX], (F64)PARCEL_GRID_STEP_METERS);
	vec.mdV[VY] = ll_round(vec.mdV[VY], (F64)PARCEL_GRID_STEP_METERS);
}

// true if x,y outside small box around start_x,start_y
bool LLToolSelectLand::outsideSlop(S32 x, S32 y, S32 start_x, S32 start_y)
{
	S32 dx = x - start_x;
	S32 dy = y - start_y;
	return dx <= -2 || 2 <= dx || dy <= -2 || 2 <= dy;
}
