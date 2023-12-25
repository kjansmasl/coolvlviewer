/**
 * @file lltoolcomp.cpp
 * @brief Composite tools
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

#include "lltoolcomp.h"

#include "llmenugl.h"			// For right-click menu hack
#include "llgl.h"

#include "llagent.h"
#include "llfloatertools.h"
#include "llmanip.h"
#include "llmaniprotate.h"
#include "llmanipscale.h"
#include "llmaniptranslate.h"
#include "llselectmgr.h"
#include "lltoolfocus.h"
#include "lltoolgrab.h"
#include "lltoolmgr.h"
#include "lltoolplacer.h"
#include "lltoolselectrect.h"
#include "llviewercontrol.h"
#include "llviewermenu.h"
#include "llviewerobject.h"
#include "llviewerwindow.h"

LLToolCompInspect gToolCompInspect;
LLToolCompTranslate gToolCompTranslate;
LLToolCompScale gToolCompScale;
LLToolCompRotate gToolCompRotate;
LLToolCompCreate gToolCompCreate;
LLToolCompGun gToolCompGun;

//-----------------------------------------------------------------------------
// LLToolGun
// Used to be in its own lltoolgun.h/cpp module, but is only used by LLToolComp
// and therefore moved here. HB
//-----------------------------------------------------------------------------
class LLToolGun final : public LLTool
{
public:
	LLToolGun(LLToolComposite* composite)
	:	LLTool("gun", composite),
		mIsSelected(false)
	{
	}

	void draw() override;

	void handleSelect() override;
	void handleDeselect() override;

	bool handleMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleHover(S32 x, S32 y, MASK mask) override;

	LL_INLINE LLTool* getOverrideTool(MASK) override	{ return NULL; }
	LL_INLINE bool clipMouseWhenDown() override			{ return false; }

private:
	bool mIsSelected;
};

void LLToolGun::handleSelect()
{
	if (gViewerWindowp)
	{
		gViewerWindowp->hideCursor();
		gViewerWindowp->moveCursorToCenter();
		gWindowp->setMouseClipping(true);
	}
	mIsSelected = true;
}

void LLToolGun::handleDeselect()
{
	if (gViewerWindowp)
	{
		gViewerWindowp->moveCursorToCenter();
		gViewerWindowp->showCursor();
		gWindowp->setMouseClipping(false);
	}
	mIsSelected = false;
}

bool LLToolGun::handleMouseDown(S32 x, S32 y, MASK mask)
{
	gGrabTransientTool = this;
	gToolMgr.getCurrentToolset()->selectTool(&gToolGrab);
	return gToolGrab.handleMouseDown(x, y, mask);
}

bool LLToolGun::handleHover(S32 x, S32 y, MASK mask)
{
	if (!gViewerWindowp) return false;

	if (mIsSelected && gAgent.cameraMouselook())
	{
		constexpr F32 NOMINAL_MOUSE_SENSITIVITY = 0.0025f;

		static LLCachedControl<F32> sensitivity(gSavedSettings,
												"MouseSensitivity");
		F32 mouse_sensitivity = clamp_rescale((F32)sensitivity,
											  0.f, 15.f, 0.5f, 2.75f)
								* NOMINAL_MOUSE_SENSITIVITY;

		// Move the view with the mouse

		// Get mouse movement delta
		S32 dx = -gViewerWindowp->getCurrentMouseDX();
		S32 dy = -gViewerWindowp->getCurrentMouseDY();

		if (dx != 0 || dy != 0)
		{
			// ...actually moved off center
			static LLCachedControl<bool> invert_mouse(gSavedSettings,
													  "InvertMouse");
			if (invert_mouse)
			{
				gAgent.pitch(mouse_sensitivity * -dy);
			}
			else
			{
				gAgent.pitch(mouse_sensitivity * dy);
			}
			LLVector3 skyward = gAgent.getReferenceUpVector();
			gAgent.rotate(mouse_sensitivity * dx, skyward.mV[VX],
						  skyward.mV[VY], skyward.mV[VZ]);

			gViewerWindowp->moveCursorToCenter();
			gViewerWindowp->hideCursor();
		}

		LL_DEBUGS("UserInput") << "hover handled by LLToolGun (mouselook)"
							   << LL_ENDL;
	}
	else
	{
		LL_DEBUGS("UserInput") << "hover handled by LLToolGun (not mouselook)"
							   << LL_ENDL;
	}

	// *HACK to avoid assert: error checking system makes sure that the cursor
	// is set during every handleHover. This is actually a no-op since the
	// cursor is hidden.
	gViewerWindowp->setCursor(UI_CURSOR_ARROW);

	return true;
}

void LLToolGun::draw()
{
	if (!gViewerWindowp) return;

	static LLUIImagePtr crosshair = LLUI::getUIImage("UIImgCrosshairsUUID");
	if (crosshair.isNull())
	{
		llerrs << "Missing cross-hair image: verify the viewer installation !"
			   << llendl;
	}
	static S32 image_width = crosshair->getWidth();
	static S32 image_height = crosshair->getHeight();

	static LLCachedControl<bool> show_crosshairs(gSavedSettings,
												 "ShowCrosshairs");
	if (show_crosshairs)
	{
		crosshair->draw((gViewerWindowp->getWindowWidth() - image_width) / 2,
						(gViewerWindowp->getWindowHeight() -
						 image_height) / 2);
	}
}

//-----------------------------------------------------------------------------
// LLToolComposite
//-----------------------------------------------------------------------------

//static
void LLToolComposite::setCurrentTool(LLTool* new_tool)
{
	if (mCur != new_tool)
	{
		if (mSelected)
		{
			mCur->handleDeselect();
			mCur = new_tool;
			mCur->handleSelect();
		}
		else
		{
			mCur = new_tool;
		}
	}
}

LLToolComposite::LLToolComposite(const std::string& name)
:	LLTool(name),
	mCur(gToolNull),
	mDefault(gToolNull),
	mSelected(false),
	mMouseDown(false),
	mManip(NULL),
	mSelectRect(NULL)
{
}

// Returns to the default tool
bool LLToolComposite::handleMouseUp(S32 x, S32 y, MASK mask)
{
	bool handled = mCur->handleMouseUp(x, y, mask);
	if (handled)
	{
		setCurrentTool(mDefault);
	}
	return handled;
}

void LLToolComposite::onMouseCaptureLost()
{
	mCur->onMouseCaptureLost();
	setCurrentTool(mDefault);
}

bool LLToolComposite::isSelecting()
{
	return mCur == mSelectRect;
}

void LLToolComposite::handleSelect()
{
	if (!gSavedSettings.getBool("EditLinkedParts"))
	{
		gSelectMgr.promoteSelectionToRoot();
	}
	mCur = mDefault;
	mCur->handleSelect();
	mSelected = true;
}

//-----------------------------------------------------------------------------
// LLToolCompInspect
//-----------------------------------------------------------------------------

LLToolCompInspect::LLToolCompInspect()
:	LLToolComposite("Inspect")
{
	mSelectRect = new LLToolSelectRect(this);
	mDefault = mSelectRect;
}

LLToolCompInspect::~LLToolCompInspect()
{
	delete mSelectRect;
	mSelectRect = NULL;
}

bool LLToolCompInspect::handleMouseDown(S32 x, S32 y, MASK mask)
{
	mMouseDown = true;
	gViewerWindowp->pickAsync(x, y, mask, pickCallback);
	return true;
}

void LLToolCompInspect::pickCallback(const LLPickInfo& pick_info)
{
	LLToolCompInspect* self = &gToolCompInspect;

	LLViewerObject* hit_obj = pick_info.getObject();

	if (!self->mMouseDown)
	{
		static LLCachedControl<bool> linked_parts(gSavedSettings,
												  "EditLinkedParts");
		// Fast click on object, but mouse is already up... just do select
		self->mSelectRect->handleObjectSelection(pick_info, linked_parts,
												 false);
		return;
	}

	if (hit_obj)
	{
		if (gSelectMgr.getSelection()->getObjectCount())
		{
			gEditMenuHandlerp = &gSelectMgr;
		}
		self->setCurrentTool(self->mSelectRect);
		self->mSelectRect->handlePick(pick_info);

	}
	else
	{
		self->setCurrentTool(self->mSelectRect);
		self->mSelectRect->handlePick(pick_info);
	}
}

bool LLToolCompInspect::handleDoubleClick(S32 x, S32 y, MASK mask)
{
	return true;
}

//-----------------------------------------------------------------------------
// LLToolCompTranslate
//-----------------------------------------------------------------------------

LLToolCompTranslate::LLToolCompTranslate()
:	LLToolComposite("Move")
{
	mManip = new LLManipTranslate(this);
	mSelectRect = new LLToolSelectRect(this);

	mCur = mManip;
	mDefault = mManip;
}

LLToolCompTranslate::~LLToolCompTranslate()
{
	delete mManip;
	mManip = NULL;

	delete mSelectRect;
	mSelectRect = NULL;
}

bool LLToolCompTranslate::handleHover(S32 x, S32 y, MASK mask)
{
	if (!mCur->hasMouseCapture())
	{
		setCurrentTool(mManip);
	}
	return mCur->handleHover(x, y, mask);
}

bool LLToolCompTranslate::handleMouseDown(S32 x, S32 y, MASK mask)
{
	mMouseDown = true;
	gViewerWindowp->pickAsync(x, y, mask, pickCallback, true);
	return true;
}

void LLToolCompTranslate::pickCallback(const LLPickInfo& pick_info)
{
	LLToolCompTranslate* self = &gToolCompTranslate;

	LLViewerObject* hit_obj = pick_info.getObject();

	self->mManip->highlightManipulators(pick_info.mMousePt.mX,
										pick_info.mMousePt.mY);
	if (!self->mMouseDown)
	{
		static LLCachedControl<bool> linked_parts(gSavedSettings,
												  "EditLinkedParts");
		// Fast click on object, but mouse is already up...just do select
		self->mSelectRect->handleObjectSelection(pick_info, linked_parts,
												 false);
		return;
	}

	if (hit_obj || self->mManip->getHighlightedPart() != LLManip::LL_NO_PART)
	{
		if (self->mManip->getSelection()->getObjectCount())
		{
			gEditMenuHandlerp = &gSelectMgr;
		}

		bool can_move = self->mManip->canAffectSelection();

		if (can_move &&
			LLManip::LL_NO_PART != self->mManip->getHighlightedPart())
		{
			self->setCurrentTool(self->mManip);
			self->mManip->handleMouseDownOnPart(pick_info.mMousePt.mX,
												pick_info.mMousePt.mY,
												pick_info.mKeyMask);
		}
		else
		{
			self->setCurrentTool(self->mSelectRect);
			self->mSelectRect->handlePick(pick_info);
#if 0		// *TODO: add toggle to trigger old click-drag functionality
			self->mManip->handleMouseDownOnPart(XY_part, x, y, mask);
#endif
		}
	}
	else
	{
		self->setCurrentTool(self->mSelectRect);
		self->mSelectRect->handlePick(pick_info);
	}
}

bool LLToolCompTranslate::handleMouseUp(S32 x, S32 y, MASK mask)
{
	mMouseDown = false;
	return LLToolComposite::handleMouseUp(x, y, mask);
}

LLTool* LLToolCompTranslate::getOverrideTool(MASK mask)
{
	if (mask == MASK_CONTROL)
	{
		return &gToolCompRotate;
	}
	if (mask == (MASK_CONTROL | MASK_SHIFT))
	{
		return &gToolCompScale;
	}
	return LLToolComposite::getOverrideTool(mask);
}

bool LLToolCompTranslate::handleDoubleClick(S32 x, S32 y, MASK mask)
{
	if (mManip->getSelection()->isEmpty() &&
		mManip->getHighlightedPart() == LLManip::LL_NO_PART)
	{
		// You should already have an object selected from the mousedown.
		// If so, show its properties
		if (gFloaterToolsp)
		{
			gFloaterToolsp->showPanel(LLFloaterTools::PANEL_CONTENTS);
		}
		return true;
	}
	// Nothing selected means the first mouse click was probably bad, so try
	// again. This also consumes the event to prevent things like double-click
	// teleport from triggering.
	return handleMouseDown(x, y, mask);
}

void LLToolCompTranslate::render()
{
	mCur->render(); // removing this will not draw the RGB arrows and guidelines

	if (mCur != mManip)
	{
		LLGLDepthTest gls_depth(GL_TRUE, GL_FALSE);
		mManip->renderGuidelines();
	}
}

//-----------------------------------------------------------------------------
// LLToolCompScale
//-----------------------------------------------------------------------------

LLToolCompScale::LLToolCompScale()
:	LLToolComposite("Stretch")
{
	mManip = new LLManipScale(this);
	mSelectRect = new LLToolSelectRect(this);

	mCur = mManip;
	mDefault = mManip;
}

LLToolCompScale::~LLToolCompScale()
{
	delete mManip;
	delete mSelectRect;
}

bool LLToolCompScale::handleHover(S32 x, S32 y, MASK mask)
{
	if (!mCur->hasMouseCapture())
	{
		setCurrentTool(mManip);
	}
	return mCur->handleHover(x, y, mask);
}

bool LLToolCompScale::handleMouseDown(S32 x, S32 y, MASK mask)
{
	mMouseDown = true;
	gViewerWindowp->pickAsync(x, y, mask, pickCallback);
	return true;
}

void LLToolCompScale::pickCallback(const LLPickInfo& pick_info)
{
	LLToolCompScale* self = &gToolCompScale;
	LLViewerObject* hit_obj = pick_info.getObject();

	self->mManip->highlightManipulators(pick_info.mMousePt.mX,
										pick_info.mMousePt.mY);
	if (!self->mMouseDown)
	{
		static LLCachedControl<bool> linked_parts(gSavedSettings,
												  "EditLinkedParts");
		// Fast click on object, but mouse is already up... just do select
		self->mSelectRect->handleObjectSelection(pick_info, linked_parts,
												 false);

		return;
	}

	if (hit_obj || self->mManip->getHighlightedPart() != LLManip::LL_NO_PART)
	{
		if (self->mManip->getSelection()->getObjectCount())
		{
			gEditMenuHandlerp = &gSelectMgr;
		}
		if (LLManip::LL_NO_PART != self->mManip->getHighlightedPart())
		{
			self->setCurrentTool(self->mManip);
			self->mManip->handleMouseDownOnPart(pick_info.mMousePt.mX,
												pick_info.mMousePt.mY,
												pick_info.mKeyMask);
		}
		else
		{
			self->setCurrentTool(self->mSelectRect);
			self->mSelectRect->handlePick(pick_info);
		}
	}
	else
	{
		self->setCurrentTool(self->mSelectRect);
		self->mSelectRect->handlePick(pick_info);
	}
}

bool LLToolCompScale::handleMouseUp(S32 x, S32 y, MASK mask)
{
	mMouseDown = false;
	return LLToolComposite::handleMouseUp(x, y, mask);
}

LLTool* LLToolCompScale::getOverrideTool(MASK mask)
{
	if (mask == MASK_CONTROL)
	{
		return &gToolCompRotate;
	}
	return LLToolComposite::getOverrideTool(mask);
}

bool LLToolCompScale::handleDoubleClick(S32 x, S32 y, MASK mask)
{
	if (!mManip->getSelection()->isEmpty() &&
		mManip->getHighlightedPart() == LLManip::LL_NO_PART)
	{
		// You should already have an object selected from the mousedown.
		// If so, show its properties
		if (gFloaterToolsp)
		{
			gFloaterToolsp->showPanel(LLFloaterTools::PANEL_CONTENTS);
		}
		return true;
	}
	// Nothing selected means the first mouse click was probably bad, so try
	// again.
	return handleMouseDown(x, y, mask);
}

void LLToolCompScale::render()
{
	mCur->render();

	if (mCur != mManip)
	{
		LLGLDepthTest gls_depth(GL_TRUE, GL_FALSE);
		mManip->renderGuidelines();
	}
}

//-----------------------------------------------------------------------------
// LLToolCompCreate
//-----------------------------------------------------------------------------

LLToolCompCreate::LLToolCompCreate()
:	LLToolComposite("Create")
{
	mPlacer = new LLToolPlacer();
	mSelectRect = new LLToolSelectRect(this);

	mCur = mPlacer;
	mDefault = mPlacer;
	mObjectPlacedOnMouseDown = false;
}

LLToolCompCreate::~LLToolCompCreate()
{
	delete mPlacer;
	delete mSelectRect;
}

bool LLToolCompCreate::handleMouseDown(S32 x, S32 y, MASK mask)
{
	bool handled = false;
	mMouseDown = true;

	if (mask == MASK_SHIFT || mask == MASK_CONTROL)
	{
		gViewerWindowp->pickAsync(x, y, mask, pickCallback);
		handled = true;
	}
	else
	{
		setCurrentTool(mPlacer);
		handled = mPlacer->placeObject(x, y, mask);
	}

	mObjectPlacedOnMouseDown = true;

	return handled;
}

void LLToolCompCreate::pickCallback(const LLPickInfo& pick_info)
{
	// *NOTE: We mask off shift and control, so you cannot select multiple
	// objects at once with the create tool.
	MASK mask = (pick_info.mKeyMask & ~MASK_SHIFT);
	mask = (mask & ~MASK_CONTROL);

	LLToolCompCreate* self = &gToolCompCreate;
	self->setCurrentTool(self->mSelectRect);
	self->mSelectRect->handlePick(pick_info);
}

bool LLToolCompCreate::handleDoubleClick(S32 x, S32 y, MASK mask)
{
	return handleMouseDown(x, y, mask);
}

bool LLToolCompCreate::handleMouseUp(S32 x, S32 y, MASK mask)
{
	bool handled = false;

	if (mMouseDown && !mObjectPlacedOnMouseDown && mask != MASK_SHIFT &&
		mask != MASK_CONTROL)
	{
		setCurrentTool(mPlacer);
		handled = mPlacer->placeObject(x, y, mask);
	}

	mObjectPlacedOnMouseDown = false;
	mMouseDown = false;

	if (!handled)
	{
		handled = LLToolComposite::handleMouseUp(x, y, mask);
	}

	return handled;
}

//-----------------------------------------------------------------------------
// LLToolCompRotate
//-----------------------------------------------------------------------------

LLToolCompRotate::LLToolCompRotate()
:	LLToolComposite("Rotate")
{
	mManip = new LLManipRotate(this);
	mSelectRect = new LLToolSelectRect(this);

	mCur = mManip;
	mDefault = mManip;
}

LLToolCompRotate::~LLToolCompRotate()
{
	delete mManip;
	delete mSelectRect;
}

bool LLToolCompRotate::handleHover(S32 x, S32 y, MASK mask)
{
	if (!mCur->hasMouseCapture())
	{
		setCurrentTool(mManip);
	}
	return mCur->handleHover(x, y, mask);
}

bool LLToolCompRotate::handleMouseDown(S32 x, S32 y, MASK mask)
{
	mMouseDown = true;
	gViewerWindowp->pickAsync(x, y, mask, pickCallback);
	return true;
}

void LLToolCompRotate::pickCallback(const LLPickInfo& pick_info)
{
	LLToolCompRotate* self = &gToolCompRotate;

	LLViewerObject* hit_obj = pick_info.getObject();

	self->mManip->highlightManipulators(pick_info.mMousePt.mX,
										pick_info.mMousePt.mY);
	if (!self->mMouseDown)
	{
		// Fast click on object, but mouse is already up... Just do select
		static LLCachedControl<bool> linked_parts(gSavedSettings,
												  "EditLinkedParts");
		self->mSelectRect->handleObjectSelection(pick_info, linked_parts,
												 false);
		return;
	}

	if (hit_obj || self->mManip->getHighlightedPart() != LLManip::LL_NO_PART)
	{
		if (self->mManip->getSelection()->getObjectCount())
		{
			gEditMenuHandlerp = &gSelectMgr;
		}
		if (LLManip::LL_NO_PART != self->mManip->getHighlightedPart())
		{
			self->setCurrentTool(self->mManip);
			self->mManip->handleMouseDownOnPart(pick_info.mMousePt.mX,
												pick_info.mMousePt.mY,
												pick_info.mKeyMask);
		}
		else
		{
			self->setCurrentTool(self->mSelectRect);
			self->mSelectRect->handlePick(pick_info);
		}
	}
	else
	{
		self->setCurrentTool(self->mSelectRect);
		self->mSelectRect->handlePick(pick_info);
	}
}

bool LLToolCompRotate::handleMouseUp(S32 x, S32 y, MASK mask)
{
	mMouseDown = false;
	return LLToolComposite::handleMouseUp(x, y, mask);
}

LLTool* LLToolCompRotate::getOverrideTool(MASK mask)
{
	if (mask == (MASK_CONTROL | MASK_SHIFT))
	{
		return &gToolCompScale;
	}
	return LLToolComposite::getOverrideTool(mask);
}

bool LLToolCompRotate::handleDoubleClick(S32 x, S32 y, MASK mask)
{
	if (!mManip->getSelection()->isEmpty() &&
		mManip->getHighlightedPart() == LLManip::LL_NO_PART)
	{
		// You should already have an object selected from the mousedown.
		// If so, show its properties
		if (gFloaterToolsp)
		{
			gFloaterToolsp->showPanel(LLFloaterTools::PANEL_CONTENTS);
		}
		return true;
	}
	// Nothing selected means the first mouse click was probably bad, so try
	// again.
	return handleMouseDown(x, y, mask);
}

void LLToolCompRotate::render()
{
	mCur->render();

	if (mCur != mManip)
	{
		LLGLDepthTest gls_depth(GL_TRUE, GL_FALSE);
		mManip->renderGuidelines();
	}
}

//-----------------------------------------------------------------------------
// LLToolCompGun
//-----------------------------------------------------------------------------

LLToolCompGun::LLToolCompGun()
:	LLToolComposite("Mouselook")
{
	mGun = new LLToolGun(this);
	mGrab = new LLToolGrabBase(this);

	setCurrentTool(mGun);
	mDefault = mGun;
}

LLToolCompGun::~LLToolCompGun()
{
	delete mGun;
	mGun = NULL;

	delete mGrab;
	mGrab = NULL;
}

bool LLToolCompGun::handleHover(S32 x, S32 y, MASK mask)
{
	// Note: if the tool changed, we can't delegate the current mouse event
	// after the change because tools can modify the mouse during selection and
	// deselection.
	// Instead we let the current tool handle the event and then make the change.
	// The new tool will take effect on the next frame.

	mCur->handleHover(x, y, mask);

	// If mouse button not down...
	if (!gViewerWindowp->getLeftMouseDown())
	{
		// Let ALT switch from gun to grab
		if (mCur == mGun && (mask & MASK_ALT))
		{
			setCurrentTool((LLTool*)mGrab);
		}
		else if (mCur == mGrab && !(mask & MASK_ALT))
		{
			setCurrentTool((LLTool*)mGun);
			setMouseCapture(true);
		}
	}

	return true;
}

bool LLToolCompGun::handleMouseDown(S32 x, S32 y, MASK mask)
{
	// if the left button is grabbed, don't put up the pie menu
	if (gAgent.leftButtonGrabbed())
	{
		gAgent.setControlFlags(AGENT_CONTROL_ML_LBUTTON_DOWN);
		return false;
	}

	// On mousedown, start grabbing
	gGrabTransientTool = this;
	gToolMgr.getCurrentToolset()->selectTool((LLTool*)mGrab);

	return gToolGrab.handleMouseDown(x, y, mask);
}

bool LLToolCompGun::handleDoubleClick(S32 x, S32 y, MASK mask)
{
	// if the left button is grabbed, don't put up the pie menu
	if (gAgent.leftButtonGrabbed())
	{
		gAgent.setControlFlags(AGENT_CONTROL_ML_LBUTTON_DOWN);
		return false;
	}

	// On mousedown, start grabbing
	gGrabTransientTool = this;
	gToolMgr.getCurrentToolset()->selectTool((LLTool*)mGrab);

	return gToolGrab.handleDoubleClick(x, y, mask);
}

bool LLToolCompGun::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
#if 0	// JC - suppress context menu 8/29/2002

	// On right mouse, go through some convoluted steps to make the build menu
	// appear.
	setCurrentTool(gToolNull);

	// This should return false, meaning the context menu will be shown.
	return false;
#else
	// Returning true suppresses the context menu
	return true;
#endif
}

bool LLToolCompGun::handleMouseUp(S32 x, S32 y, MASK mask)
{
	gAgent.setControlFlags(AGENT_CONTROL_ML_LBUTTON_UP);
	setCurrentTool((LLTool*)mGun);
	return true;
}

void LLToolCompGun::onMouseCaptureLost()
{
	if (mComposite)
	{
		mComposite->onMouseCaptureLost();
		return;
	}
	mCur->onMouseCaptureLost();
}

void LLToolCompGun::handleSelect()
{
	LLToolComposite::handleSelect();
	setMouseCapture(true);
}

void LLToolCompGun::handleDeselect()
{
	LLToolComposite::handleDeselect();
	setMouseCapture(false);
}

bool LLToolCompGun::handleScrollWheel(S32 x, S32 y, S32 clicks)
{
	if (clicks > 0)
	{
		gAgent.changeCameraToDefault();
	}
	return true;
}
