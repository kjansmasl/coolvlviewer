/**
 * @file lltool.h
 * @brief LLTool class header file
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

#ifndef LL_LLTOOL_H
#define LL_LLTOOL_H

#include "llcoord.h"
#include "llfocusmgr.h"
#include "llkeyboard.h"
#include "llmousehandler.h"
#include "llvector3.h"
#include "llvector3d.h"

class LLPanel;
class LLToolComposite;
class LLView;
class LLViewerObject;

class LLTool : public LLMouseHandler
{
protected:
	LOG_CLASS(LLTool);

public:
	LLTool(const std::string& name, LLToolComposite* composite = NULL);
	~LLTool() override;

	// *HACK: to support LLFocusMgr
	LL_INLINE virtual bool isView() const override			{ return false; }

	// Virtual functions inherited from LLMouseHandler

	bool handleHover(S32 x, S32 y, MASK mask) override;
	bool handleMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleMouseUp(S32 x, S32 y, MASK mask) override;

	LL_INLINE bool handleMiddleMouseDown(S32, S32, MASK) override
	{
		// By default, do not handle it
		return false;
	}

	LL_INLINE bool handleMiddleMouseUp(S32, S32, MASK) override
	{
		// By default, do not handle it
		return false;
	}

	LL_INLINE bool handleScrollWheel(S32, S32, S32) override
	{
		// By default, do not handle it
		return false;
	}

	LL_INLINE bool handleDoubleClick(S32, S32, MASK) override
	{
		// By default, do not handle it
		return false;
	}

	LL_INLINE bool handleRightMouseDown(S32, S32, MASK) override
	{
		// By default, do not handle it
		return false;
	}

	LL_INLINE bool handleRightMouseUp(S32, S32, MASK) override
	{
		// By default, do not handle it
		return false;
	}

	LL_INLINE bool handleToolTip(S32, S32, std::string&, LLRect*) override
	{
		// By default, do not handle it
		return false;
	}

	// Tools should permit tips even when the mouse is down, as that is pretty
	// normal for tools
	LL_INLINE EShowToolTip getShowToolTip() override		{ return SHOW_ALWAYS; }

	LL_INLINE void screenPointToLocal(S32 screen_x, S32 screen_y, S32* local_x,
									  S32* local_y) const override
	{
		*local_x = screen_x;
		*local_y = screen_y;
	}

	LL_INLINE void localPointToScreen(S32 local_x, S32 local_y, S32* screen_x,
									  S32* screen_y) const override
	{
		*screen_x = local_x;
		*screen_y = local_y;
	}

	LL_INLINE std::string getName() const override			{ return mName; }

	// New virtual functions

	// Override to return true whenever this tool is meant to edit objects.
	// Used by LLFloaterTools. HB
	LL_INLINE virtual bool isObjectEditTool() const			{ return false; }

	LL_INLINE virtual LLViewerObject* getEditingObject()	{ return NULL; }
	LL_INLINE virtual LLVector3d getEditingPointGlobal()	{ return LLVector3d(); }
	LL_INLINE virtual bool isEditing()						{ return getEditingObject() != NULL; }
	LL_INLINE virtual void stopEditing()					{}

	LL_INLINE virtual bool clipMouseWhenDown()				{ return true; }

	// Does stuff when your tool is selected
	LL_INLINE virtual void handleSelect()					{}
	// Cleans up when your tool is deselected
	LL_INLINE virtual void handleDeselect()					{}

	virtual LLTool* getOverrideTool(MASK mask);

	// Returns true if this is a tool that should always be rendered regardless
	// of selection.
	LL_INLINE virtual bool isAlwaysRendered()				{ return false; }

	// Draws tool specific 3D content in world
	LL_INLINE virtual void render()							{}

	// Draws tool specific 2D overlay
	LL_INLINE virtual void draw()							{}

	LL_INLINE virtual bool handleKey(KEY key, MASK mask)	{ return false; }

	// Note: NOT virtual. Subclasses should call this version.
	void setMouseCapture(bool b);

	LL_INLINE bool hasMouseCapture() override
	{
		return gFocusMgr.getMouseCapture() ==
					(mComposite ? (LLTool*)mComposite : this);
	}

	// Override this one as needed.
	LL_INLINE void onMouseCaptureLost() override			{}

protected:
	// Composite will handle mouse captures.
	LLToolComposite*			mComposite;

	std::string					mName;

public:
	static const std::string	sNameNull;
};

#endif
