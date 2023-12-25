/**
 * @file lltoolface.cpp
 * @brief A tool to manipulate faces
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

#include "lltoolface.h"

#include "llfloatertools.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llselectmgr.h"
#include "lltoolview.h"
#include "llviewercontrol.h"
#include "llviewerobject.h"
#include "llviewerwindow.h"
#include "llvoavatarself.h"

LLToolFace gToolFace;

LLToolFace::LLToolFace()
:	LLTool("Texture")
{
}

//virtual
bool LLToolFace::handleDoubleClick(S32 x, S32 y, MASK mask)
{
	if (!gSelectMgr.getSelection()->isEmpty())
	{
		// You should already have an object selected from the mousedown.
		// If so, show its properties
		if (gFloaterToolsp)
		{
			gFloaterToolsp->showPanel(LLFloaterTools::PANEL_FACE);
		}
		return true;
	}
	// Nothing selected means the first mouse click was probably bad, so try
	// again.
	return false;
}

//virtual
bool LLToolFace::handleMouseDown(S32 x, S32 y, MASK mask)
{
	gViewerWindowp->pickAsync(x, y, mask, pickCallback);
	return true;
}

//static
void LLToolFace::pickCallback(const LLPickInfo& pick_info)
{
	LLViewerObject* hit_obj	= pick_info.getObject();
	if (!hit_obj)
	{
		if (pick_info.mKeyMask != MASK_SHIFT)
		{
			gSelectMgr.deselectAll();
		}
		return;
	}

	if (hit_obj->isAvatar())
	{
		// Clicked on an avatar, so do not do anything
		return;
	}

//MK
	if (gRLenabled && !gRLInterface.canTouch(hit_obj) &&
		!hit_obj->isAttachment())
	{
		return;
	}
//mk

	// Clicked on a world object, try to pick the appropriate face

	S32 hit_face = pick_info.mObjectFace;

	if (pick_info.mKeyMask & MASK_SHIFT)
	{
		// If object not selected, need to inform sim
		if ( !hit_obj->isSelected() )
		{
			// Object was not selected so add the object and face
			gSelectMgr.selectObjectOnly(hit_obj, hit_face);
		}
		else if (!gSelectMgr.getSelection()->contains(hit_obj, hit_face))
		{
			// Object is selected, but not this face, so add it.
			gSelectMgr.addAsIndividual(hit_obj, hit_face);
		}
		else
		{
			// Object is selected, as is this face, so remove the face.
			gSelectMgr.remove(hit_obj, hit_face);

			// BUG: If you remove the last face, the simulator won't know
			// about it.
		}
	}
	else
	{
		// Clicked without modifiers, select only this face
		gSelectMgr.deselectAll();
		gSelectMgr.selectObjectOnly(hit_obj, hit_face);
	}
}

//virtual
void LLToolFace::handleSelect()
{
	// From now on, draw faces
	gSelectMgr.setTEMode(true);
}

//virtual
void LLToolFace::handleDeselect()
{
	// Stop drawing faces
	gSelectMgr.setTEMode(false);
}

//virtual
void LLToolFace::render()
{
	// For now, do nothing
}
