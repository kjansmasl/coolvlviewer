/** 
 * @file lltoolselectrect.cpp
 * @brief A tool to select multiple objects with a screen-space rectangle.
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

#include "lltoolselectrect.h"

#include "llgl.h"
#include "llrender.h"

#include "llagent.h"
#include "lldrawable.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llselectmgr.h"
#include "lltoolmgr.h"
#include "lltoolview.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llviewerwindow.h"
#include "llworld.h"

// Globals
constexpr S32 SLOP_RADIUS = 5;

//
// Member functions
//

LLToolSelectRect::LLToolSelectRect(LLToolComposite* composite)
:	LLToolSelect(composite),
	mDragStartX(0),
	mDragStartY(0),
	mDragEndX(0),
	mDragEndY(0),
	mDragLastWidth(0),
	mDragLastHeight(0),
	mMouseOutsideSlop(false)

{
}

bool LLToolSelectRect::handleMouseDown(S32 x, S32 y, MASK mask)
{
	handlePick(gViewerWindowp->pickImmediate(x, y, true));

	LLTool::handleMouseDown(x, y, mask);

	return mPick.getObject().notNull();
}

void LLToolSelectRect::handlePick(const LLPickInfo& pick)
{
	mPick = pick;

	// start dragging rectangle
	setMouseCapture(true);

	mDragStartX = pick.mMousePt.mX;
	mDragStartY = pick.mMousePt.mY;
	mDragEndX = pick.mMousePt.mX;
	mDragEndY = pick.mMousePt.mY;

	mMouseOutsideSlop = false;
}

bool LLToolSelectRect::handleMouseUp(S32 x, S32 y, MASK mask)
{
	setMouseCapture(false);

	if (mMouseOutsideSlop)
	{
		mDragLastWidth = 0;
		mDragLastHeight = 0;

		mMouseOutsideSlop = false;
		
		if (mask == MASK_CONTROL)
		{
			gSelectMgr.deselectHighlightedObjects();
		}
		else
		{
			gSelectMgr.selectHighlightedObjects();
		}
		return true;
	}
	else
	{
		return LLToolSelect::handleMouseUp(x, y, mask);
	}
}

bool LLToolSelectRect::handleHover(S32 x, S32 y, MASK mask)
{
	if (hasMouseCapture())
	{
		if (mMouseOutsideSlop || outsideSlop(x, y, mDragStartX, mDragStartY))
		{
			if (!mMouseOutsideSlop && !(mask & MASK_SHIFT) &&
				!(mask & MASK_CONTROL))
			{
				// just started rect select, and not adding to current
				// selection
				gSelectMgr.deselectAll();
			}
			mMouseOutsideSlop = true;
			mDragEndX = x;
			mDragEndY = y;

			handleRectangleSelection(x, y, mask);
		}
		else
		{
			return LLToolSelect::handleHover(x, y, mask);
		}

		LL_DEBUGS("UserInput") << "hover handled by LLToolSelectRect (active)"
							   << LL_ENDL;		
	}
	else
	{
		LL_DEBUGS("UserInput") << "hover handled by LLToolSelectRect (inactive)"
							   << LL_ENDL;		
	}

	gWindowp->setCursor(UI_CURSOR_ARROW);

	return true;
}

void LLToolSelectRect::draw()
{
	if (hasMouseCapture() && mMouseOutsideSlop)
	{
		bool has_ctrl_mask = gKeyboardp &&
							 gKeyboardp->currentMask(true) == MASK_CONTROL;
		if (has_ctrl_mask)
		{
			gGL.color4f(1.f, 0.f, 0.f, 1.f);
		}
		else
		{
			gGL.color4f(1.f, 1.f, 0.f, 1.f);
		}
		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
		gl_rect_2d(llmin(mDragStartX, mDragEndX),
				   llmax(mDragStartY, mDragEndY),
				   llmax(mDragStartX, mDragEndX),
				   llmin(mDragStartY, mDragEndY), false);
		if (has_ctrl_mask)
		{
			gGL.color4f(1.f, 0.f, 0.f, 0.1f);
		}
		else
		{
			gGL.color4f(1.f, 1.f, 0.f, 0.1f);
		}
		gl_rect_2d(llmin(mDragStartX, mDragEndX),
				   llmax(mDragStartY, mDragEndY),
				   llmax(mDragStartX, mDragEndX),
				   llmin(mDragStartY, mDragEndY));
	}
}

// true if x,y outside small box around start_x,start_y
bool LLToolSelectRect::outsideSlop(S32 x, S32 y, S32 start_x, S32 start_y)
{
	S32 dx = x - start_x;
	S32 dy = y - start_y;

	return dx <= -SLOP_RADIUS || SLOP_RADIUS <= dx || dy <= -SLOP_RADIUS ||
		   SLOP_RADIUS <= dy;
}

// Returns true if you got at least one object (used to be in llglsandbox.cpp).
void LLToolSelectRect::handleRectangleSelection(S32 x, S32 y, MASK mask)
{
//MK
	if (gRLenabled && gRLInterface.mContainsInteract) return;
//mk

	LLVector3 av_pos = gAgent.getPositionAgent();
	static LLCachedControl<F32> max_select_distance(gSavedSettings, "MaxSelectDistance");
	F32 select_dist_squared = max_select_distance * max_select_distance;

	bool deselect = (mask == MASK_CONTROL);
	S32 left =	llmin(x, mDragStartX);
	S32 right =	llmax(x, mDragStartX);
	S32 top =	llmax(y, mDragStartY);
	S32 bottom =llmin(y, mDragStartY);

	left = ll_round((F32) left * LLUI::sGLScaleFactor.mV[VX]);
	right = ll_round((F32) right * LLUI::sGLScaleFactor.mV[VX]);
	top = ll_round((F32) top * LLUI::sGLScaleFactor.mV[VY]);
	bottom = ll_round((F32) bottom * LLUI::sGLScaleFactor.mV[VY]);

	F32 old_far_plane = gViewerCamera.getFar();
	F32 old_near_plane = gViewerCamera.getNear();

	S32 width = right - left + 1;
	S32 height = top - bottom + 1;

	bool grow_selection = false;
	bool shrink_selection = false;

	if (height > mDragLastHeight || width > mDragLastWidth)
	{
		grow_selection = true;
	}
	if (height < mDragLastHeight || width < mDragLastWidth)
	{
		shrink_selection = true;
	}

	if (!grow_selection && !shrink_selection)
	{
		// nothing to do
		return;
	}

	mDragLastHeight = height;
	mDragLastWidth = width;

	S32 center_x = (left + right) / 2;
	S32 center_y = (top + bottom) / 2;

	// save drawing mode
	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.pushMatrix();

	static LLCachedControl<bool> limit_select_distance(gSavedSettings,
													   "LimitSelectDistance");
	if (limit_select_distance)
	{
		// ...select distance from control
		LLVector3 relative_av_pos = av_pos;
		relative_av_pos -= gViewerCamera.getOrigin();

		F32 new_far = relative_av_pos * gViewerCamera.getAtAxis() +
					  max_select_distance;
		F32 new_near = relative_av_pos * gViewerCamera.getAtAxis() -
					   max_select_distance;

		new_near = llmax(new_near, 0.1f);

		gViewerCamera.setFar(new_far);
		gViewerCamera.setNear(new_near);
	}
//MK
	if (gRLenabled && gRLInterface.mFartouchMax < EXTREMUM)
	{
		// don't allow select by rectangle while under fartouch
		gViewerCamera.setFar(0.0f);
		gViewerCamera.setNear(0.0f);
	}
//mk
	gViewerCamera.setPerspective(FOR_SELECTION, center_x - width / 2,
						   center_y - height / 2, width, height,
						   limit_select_distance);

	if (shrink_selection)
	{
		struct f final : public LLSelectedObjectFunctor
		{
			bool apply(LLViewerObject* vobjp) override
			{
				LLDrawable* drawable = vobjp->mDrawable;
				if (!drawable || vobjp->isAttachment() ||
					(vobjp->getPCode() != LL_PCODE_VOLUME &&
					 vobjp->getPCode() != LL_PCODE_LEGACY_TREE &&
					 vobjp->getPCode() != LL_PCODE_LEGACY_GRASS))
				{
					return true;
				}
//MK
				if (gRLenabled && !gRLInterface.canEdit(vobjp))
				{
					return true;
				}
//mk
				S32 result = gViewerCamera.sphereInFrustum(drawable->getPositionAgent(),
															drawable->getRadius());
				switch (result)
				{
				  case 0:
					gSelectMgr.unhighlightObjectOnly(vobjp);
					break;
				  case 1:
					// check vertices
					if (!gViewerCamera.areVertsVisible(vobjp,
														LLSelectMgr::sRectSelectInclusive))
					{
						gSelectMgr.unhighlightObjectOnly(vobjp);
					}
					break;
				  default:
					break;
				}
				return true;
			}
		} func;
		gSelectMgr.getHighlightedObjects()->applyToObjects(&func);
	}

	if (grow_selection)
	{
		std::vector<LLDrawable*> potentials;

		for (LLWorld::region_list_t::const_iterator
				 iter = gWorld.getRegionList().begin(),
				 end = gWorld.getRegionList().end();
			 iter != end; ++iter)
		{
			LLViewerRegion* region = *iter;
			for (U32 i = 0; i < LLViewerRegion::PARTITION_VO_CACHE; ++i)
			{
				LLSpatialPartition* part = region->getSpatialPartition(i);
				// None of the partitions under PARTITION_VO_CACHE can be NULL
				llassert(part);
				part->cull(gViewerCamera, &potentials, true);
			}
		}

		for (std::vector<LLDrawable*>::iterator iter = potentials.begin(),
												end = potentials.end();
			 iter != end; ++iter)
		{
			LLDrawable* drawable = *iter;
			LLViewerObject* vobjp = drawable->getVObj();

			if (!drawable || !vobjp || vobjp->isAttachment() ||
				(vobjp->getPCode() != LL_PCODE_VOLUME &&
				 vobjp->getPCode() != LL_PCODE_LEGACY_TREE &&
				 vobjp->getPCode() != LL_PCODE_LEGACY_GRASS) ||
				(deselect && !vobjp->isSelected()))
			{
				continue;
			}
//MK
			if (gRLenabled && !gRLInterface.canEdit(vobjp))
			{
				continue;
			}
//mk
			if (limit_select_distance &&
				dist_vec_squared(drawable->getWorldPosition(),
								 av_pos) > select_dist_squared)
			{
				continue;
			}

			S32 result = gViewerCamera.sphereInFrustum(drawable->getPositionAgent(),
														drawable->getRadius());
			if (result)
			{
				switch (result)
				{
				case 1:
					// check vertices
					if (gViewerCamera.areVertsVisible(vobjp,
													   LLSelectMgr::sRectSelectInclusive))
					{
						gSelectMgr.highlightObjectOnly(vobjp);
					}
					break;
				case 2:
					gSelectMgr.highlightObjectOnly(vobjp);
					break;
				default:
					break;
				}
			}
		}
	}

	// restore drawing mode
	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.popMatrix();
	gGL.matrixMode(LLRender::MM_MODELVIEW);

	// restore camera
	gViewerCamera.setFar(old_far_plane);
	gViewerCamera.setNear(old_near_plane);
	gViewerWindowp->setup3DRender();
}
