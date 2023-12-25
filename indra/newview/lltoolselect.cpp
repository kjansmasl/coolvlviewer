/**
 * @file lltoolselect.cpp
 * @brief LLToolSelect class implementation
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

#include "lltoolselect.h"

#include "llagent.h"
#include "llagentpilot.h"
#include "lldrawable.h"
#include "llmanip.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llselectmgr.h"
#include "lltoolmgr.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewermenu.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llvoavatarself.h"
#include "llworld.h"

// Globals
constexpr F32 SELECTION_ROTATION_TRESHOLD = 0.1f;
constexpr F32 SELECTION_SITTING_ROTATION_TRESHOLD = 3.2f;

LLToolSelect::LLToolSelect(LLToolComposite* composite)
:	LLTool("Select", composite),
	mIgnoreGroup(false)
{
}

// True if you selected an object.
bool LLToolSelect::handleMouseDown(S32 x, S32 y, MASK mask)
{
	// Do immediate pick query
	mPick = gViewerWindowp->pickImmediate(x, y, true);
//MK
	LLViewerObject* object = mPick.getObject();
	if (gRLenabled && object)
	{
		if (!gRLInterface.canEdit(object))
		{
			return false;
		}
		if (!gRLInterface.canTouchFar(object, mPick.mIntersection))
		{
			return false;
		}
	}
//mk
	// Pass mousedown to agent
	LLTool::handleMouseDown(x, y, mask);

	return mPick.getObject().notNull();
}

// static
LLObjectSelectionHandle LLToolSelect::handleObjectSelection(const LLPickInfo& pick,
															bool ignore_group,
															bool temp_select,
															bool select_root)
{
	LLViewerObject* object = pick.getObject();
	if (select_root)
	{
		object = object->getRootEdit();
	}
//MK
	if (gRLenabled && object)
	{
		if (!temp_select && !gRLInterface.canEdit(object))
		{
			return NULL;
		}
		if (!gRLInterface.canTouchFar(object))
		{
			return NULL;
		}
	}
//mk
	bool select_owned = gSavedSettings.getBool("SelectOwnedOnly");
	bool select_movable = gSavedSettings.getBool("SelectMovableOnly");

	// *NOTE: These settings must be cleaned up at bottom of function.
	if (temp_select || gSelectMgr.mAllowSelectAvatar)
	{
		gSavedSettings.setBool("SelectOwnedOnly", false);
		gSavedSettings.setBool("SelectMovableOnly", false);
		gSelectMgr.setForceSelection(true);
	}

	bool extend_select = pick.mKeyMask == MASK_SHIFT ||
						 pick.mKeyMask == MASK_CONTROL;

	// If no object, check for icon, then just deselect
	if (!object)
	{
		LLViewerObject* src_obj = NULL;
		LLHUDIcon* last_hit_hud_icon = pick.mHUDIcon;
		if (last_hit_hud_icon)
		{
			src_obj = last_hit_hud_icon->getSourceObject();
		}
		if (src_obj)
		{
			const LLUUID& object_id = src_obj->getID();
			last_hit_hud_icon->fireClickedCallback(object_id);
		}
		else if (!extend_select)
		{
			gSelectMgr.deselectAll();
		}
	}
	else
	{
		bool already_selected = object->isSelected();

		if (extend_select)
		{
			if (already_selected)
			{
				if (ignore_group)
				{
					gSelectMgr.deselectObjectOnly(object);
				}
				else
				{
					gSelectMgr.deselectObjectAndFamily(object, true, true);
				}
			}
			else if (ignore_group)
			{
				gSelectMgr.selectObjectOnly(object, SELECT_ALL_TES);
			}
			else
			{
				gSelectMgr.selectObjectAndFamily(object);
			}
		}
		else
		{
			// Save the current zoom values because deselect resets them.
			F32 target_zoom;
			F32 current_zoom;
			gAgent.getHUDZoom(target_zoom, current_zoom);

			// JC - Change behavior to make it easier to select children
			// of linked sets. 9/3/2002
			if (!already_selected || ignore_group)
			{
				// ...lose current selection in favor of just this object
				gSelectMgr.deselectAll();
			}

			if (ignore_group)
			{
				gSelectMgr.selectObjectOnly(object, SELECT_ALL_TES);
			}
			else
			{
				gSelectMgr.selectObjectAndFamily(object);
			}

			// Restore the zoom to the previously stored values.
			gAgent.setHUDZoom(target_zoom, current_zoom);
		}

		static LLCachedControl<bool> do_turn(gSavedSettings,
											 "TurnTowardsSelectedObject");
		if (do_turn &&
			// If object is not an avatar
			!object->isAvatar() &&
			// If camera not glued to avatar
			!gAgent.getFocusOnAvatar() &&
			// And it is not one of our attachments
			isAgentAvatarValid() &&
			LLVOAvatar::findAvatarFromAttachment(object) != gAgentAvatarp)
		{
			// Have avatar turn to face the selected object(s)
			LLVector3 selection_dir(gSelectMgr.getSelectionCenterGlobal() -
									gAgent.getPositionGlobal());
			selection_dir.mV[VZ] = 0.f;
			selection_dir.normalize();
			if (gAgent.getAtAxis() * selection_dir < 0.6f)
			{
				LLQuaternion target_rot;
				target_rot.shortestArc(LLVector3::x_axis, selection_dir);

				F32 rot_threshold;
				if (gAgentAvatarp->mIsSitting)
				{
					rot_threshold = SELECTION_SITTING_ROTATION_TRESHOLD;
				}
				else
				{
					rot_threshold = SELECTION_ROTATION_TRESHOLD;
				}

				gAgentPilot.startAutoPilotGlobal(gAgent.getPositionGlobal(),
												 "", &target_rot, NULL, NULL,
												 MAX_FAR_CLIP, rot_threshold,
												 gAgent.getFlying());
			}
		}

		if (temp_select && !already_selected)
		{
			LLViewerObject* root_obj = (LLViewerObject*)object->getRootEdit();
			LLObjectSelectionHandle selection = gSelectMgr.getSelection();

			// This is just a temporary selection
			LLSelectNode* select_node = selection->findNode(root_obj);
			if (select_node)
			{
				select_node->setTransient(true);
			}

			LLViewerObject::const_child_list_t& child_list =
				root_obj->getChildren();
			for (LLViewerObject::child_list_t::const_iterator
					iter = child_list.begin(), end = child_list.end();
				 iter != end; iter++)
			{
				LLViewerObject* child = *iter;
				select_node = selection->findNode(child);
				if (select_node)
				{
					select_node->setTransient(true);
				}
			}
		}
	}

	// Cleanup temp select settings above.
	if (temp_select || gSelectMgr.mAllowSelectAvatar)
	{
		gSavedSettings.setBool("SelectOwnedOnly", select_owned);
		gSavedSettings.setBool("SelectMovableOnly", select_movable);
		gSelectMgr.setForceSelection(false);
	}

	return gSelectMgr.getSelection();
}

bool LLToolSelect::handleMouseUp(S32 x, S32 y, MASK mask)
{
	mIgnoreGroup = gSavedSettings.getBool("EditLinkedParts");

	handleObjectSelection(mPick, mIgnoreGroup, false);

	return LLTool::handleMouseUp(x, y, mask);
}

void LLToolSelect::handleDeselect()
{
	if (hasMouseCapture())
	{
		setMouseCapture(false);  // Calls onMouseCaptureLost() indirectly
	}
}

void LLToolSelect::stopEditing()
{
	if (hasMouseCapture())
	{
		setMouseCapture(false);  // Calls onMouseCaptureLost() indirectly
	}
}

void LLToolSelect::onMouseCaptureLost()
{
	gSelectMgr.enableSilhouette(true);

	// Clean up drag-specific variables
	mIgnoreGroup = false;
}
