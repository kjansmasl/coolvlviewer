/**
 * @file lltoolbrushland.h
 * @brief LLToolBrushLand class header file
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

#ifndef LL_LLTOOLBRUSHLAND_H
#define LL_LLTOOLBRUSHLAND_H

#include "hbfastset.h"
#include "lleditmenuhandler.h"

#include "lltool.h"

class LLSurface;
class LLViewerRegion;

// A toolbrush that modifies the land.

class LLToolBrushLand final : public LLTool, public LLEditMenuHandler
{
protected:
	LOG_CLASS(LLToolBrushLand);

public:
	LLToolBrushLand();

	// x, y in window coords with 0, 0 = left, bottom
	bool handleMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleMouseUp(S32 x, S32 y, MASK mask) override;
	bool handleHover(S32 x, S32 y, MASK mask) override;

	void onMouseCaptureLost() override;

	void handleSelect() override;
	void handleDeselect() override;

	// Returns true if this is a tool that should always be rendered regardless
	// of selection.
	LL_INLINE bool isAlwaysRendered() override	{ return true; }

	// Draws the area that will be affected.
	void render() override;

	// This is where the land modification actually occurs
	static void onIdle(void* brush_tool);

	void modifyLandInSelectionGlobal();

	void undo() override;
	LL_INLINE bool canUndo() const override		{ return true; }

protected:
	void brush();
	void modifyLandAtPointGlobal(const LLVector3d& spot, MASK mask);

	typedef fast_hset<LLViewerRegion*> region_list_t;
	void determineAffectedRegions(region_list_t& regions,
								  const LLVector3d& spot) const;

	void renderOverlay(LLSurface& land, const LLVector3& pos_region,
					   const LLVector3& pos_world);

	// Does region allow terraform, or are we a god ?
	bool canTerraform(LLViewerRegion* regionp) const;

	// Modal dialog alerting you cannot terraform the region
	void alertNoTerraform(LLViewerRegion* regionp);

private:
	U8 getBrushIndex();

protected:
	region_list_t	mLastAffectedRegions;

	F32				mStartingZ;
	S32				mMouseX;
	S32				mMouseY;
	bool			mGotHover;
	bool			mBrushSelected;
};

extern LLToolBrushLand gToolBrushLand;

#endif // LL_LLTOOLBRUSHLAND_H
