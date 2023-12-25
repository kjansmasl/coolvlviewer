/**
 * @file lljoystickbutton.cpp
 * @brief LLJoystick class implementation
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

#include "lljoystickbutton.h"

#include "llgl.h"
#include "llrender.h"

#include "llagent.h"
#include "llfloatermove.h"
#include "llviewertexturelist.h"

static const std::string LL_JOYSTICK_SLIDE = "joystick_slide";
static LLRegisterWidget<LLJoystickAgentSlide> r1(LL_JOYSTICK_SLIDE);
static const std::string LL_JOYSTICK_TURN = "joystick_turn";
static LLRegisterWidget<LLJoystickAgentTurn> r2(LL_JOYSTICK_TURN);

constexpr F32 NUDGE_TIME = 0.25f;		// In seconds
constexpr F32 ORBIT_NUDGE_RATE = 0.05f; // Fraction of normal speed

LLJoystick::LLJoystick(const std::string& name, LLRect rect,
					   const std::string& default_image,
					   const std::string& selected_image,
					   EJoystickQuadrant initial_quadrant)
:	LLButton(name, rect, default_image, selected_image, NULL, NULL, NULL),
	mInitialQuadrant(initial_quadrant),
	mInitialOffset(0, 0),
	mLastMouse(0, 0),
	mFirstMouse(0, 0),
	mVertSlopNear(0),
	mVertSlopFar(0),
	mHorizSlopNear(0),
	mHorizSlopFar(0),
	mHeldDown(false),
	mHeldDownTimer()
{
	setHeldDownCallback(&LLJoystick::onHeldDown);
	setCallbackUserData(this);
}

void LLJoystick::updateSlop()
{
	mVertSlopNear = getRect().getHeight();
	mVertSlopFar = getRect().getHeight() * 2;

	mHorizSlopNear = getRect().getWidth();
	mHorizSlopFar = getRect().getWidth() * 2;

	// Compute initial mouse offset based on initial quadrant.
	// Place the mouse evenly between the near and far zones.
	switch (mInitialQuadrant)
	{
		case JQ_ORIGIN:
			mInitialOffset.set(0, 0);
			break;

		case JQ_UP:
			mInitialOffset.mX = 0;
			mInitialOffset.mY = (mVertSlopNear + mVertSlopFar) / 2;
			break;

		case JQ_DOWN:
			mInitialOffset.mX = 0;
			mInitialOffset.mY = - (mVertSlopNear + mVertSlopFar) / 2;
			break;

		case JQ_LEFT:
			mInitialOffset.mX = - (mHorizSlopNear + mHorizSlopFar) / 2;
			mInitialOffset.mY = 0;
			break;

		case JQ_RIGHT:
			mInitialOffset.mX = (mHorizSlopNear + mHorizSlopFar) / 2;
			mInitialOffset.mY = 0;
			break;

		default:
			llerrs << "LLJoystick::LLJoystick() - bad switch case" << llendl;
	}
}

bool LLJoystick::handleMouseDown(S32 x, S32 y, MASK mask)
{
	mLastMouse.set(x, y);
	mFirstMouse.set(x, y);
	mMouseDownTimer.reset();

	return LLButton::handleMouseDown(x, y, mask);
}

bool LLJoystick::handleMouseUp(S32 x, S32 y, MASK mask)
{
	if (hasMouseCapture())
	{
		mLastMouse.set(x, y);
		mHeldDown = false;
		onMouseUp();
	}

	return LLButton::handleMouseUp(x, y, mask);
}

bool LLJoystick::handleHover(S32 x, S32 y, MASK mask)
{
	if (hasMouseCapture())
	{
		mLastMouse.set(x, y);
	}

	return LLButton::handleHover(x, y, mask);
}

F32 LLJoystick::getElapsedHeldDownTime()
{
	return mHeldDown ? getHeldDownTime() : 0.f;
}

//static
void LLJoystick::onHeldDown(void* userdata)
{
	LLJoystick* self = (LLJoystick*)userdata;
	if (self)
	{
		self->mHeldDown = true;
		self->onHeldDown();
	}
}

EJoystickQuadrant LLJoystick::selectQuadrant(LLXMLNodePtr node)
{
	EJoystickQuadrant quadrant = JQ_RIGHT;

	if (node->hasAttribute("quadrant"))
	{
		std::string quadrant_name;
		node->getAttributeString("quadrant", quadrant_name);

		quadrant = quadrantFromName(quadrant_name);
	}
	return quadrant;
}

std::string LLJoystick::nameFromQuadrant(EJoystickQuadrant quadrant)
{
	switch (quadrant)
	{
		case JQ_ORIGIN:
			return "origin";

		case JQ_UP:
			return "up";

		case JQ_DOWN:
			return "down";

		case JQ_LEFT:
			return "left";

		case JQ_RIGHT:
			return "right";

		default:
			break;
	}
	return "";
}

EJoystickQuadrant LLJoystick::quadrantFromName(const std::string& quadrant_str)
{
	if (quadrant_str == "up")
	{
		return JQ_UP;
	}
	if (quadrant_str == "down")
	{
		return JQ_DOWN;
	}
	if (quadrant_str == "right")
	{
		return JQ_RIGHT;
	}
	if (quadrant_str == "left")
	{
		return JQ_LEFT;
	}
	return JQ_ORIGIN;
}

LLXMLNodePtr LLJoystick::getXML(bool save_children) const
{
	LLXMLNodePtr node = LLButton::getXML();
	node->createChild("quadrant",
					  true)->setStringValue(nameFromQuadrant(mInitialQuadrant));
	return node;
}

//-----------------------------------------------------------------------------
// LLJoystickAgentTurn
//-----------------------------------------------------------------------------

void LLJoystickAgentTurn::onHeldDown()
{
	F32 time = getElapsedHeldDownTime();
	updateSlop();

	S32 dx = mLastMouse.mX - mFirstMouse.mX + mInitialOffset.mX;
	S32 dy = mLastMouse.mY - mFirstMouse.mY + mInitialOffset.mY;

	F32 m = (F32)dx / abs(dy);

	if (m > 1.f)
	{
		m = 1.f;
	}
	else if (m < -1.f)
	{
		m = -1.f;
	}
	gAgent.moveYaw(-LLFloaterMove::getYawRate(time) * m);

	// Handle forward/back movement
	if (dy > mVertSlopFar)
	{
		// If mouse is forward of run region run forward
		gAgent.moveAt(1);
	}
	else if (dy > mVertSlopNear)
	{
		if (time < NUDGE_TIME)
		{
			gAgent.moveAtNudge(1);
		}
		else
		{
			// If mouse is forward of walk region walk forward
			// JC 9/5/2002 - Always run / move quickly.
			gAgent.moveAt(1);
		}
	}
	else if (dy < -mVertSlopFar)
	{
		// If mouse is behind run region run backward
		gAgent.moveAt(-1);
	}
	else if (dy < -mVertSlopNear)
	{
		if (time < NUDGE_TIME)
		{
			gAgent.moveAtNudge(-1);
		}
		else
		{
			// .If mouse is behind walk region walk backward
			// JC 9/5/2002 - Always run / move quickly.
			gAgent.moveAt(-1);
		}
	}
}

LLXMLNodePtr LLJoystickAgentTurn::getXML(bool save_children) const
{
	LLXMLNodePtr node = LLJoystick::getXML();
	node->setName(LL_JOYSTICK_TURN);
	return node;
}

LLView* LLJoystickAgentTurn::fromXML(LLXMLNodePtr node, LLView* parent,
									 LLUICtrlFactory* factory)
{
	std::string name = "button";
	node->getAttributeString("name", name);

	std::string	image_unselected;
	if (node->hasAttribute("image_unselected"))
	{
		node->getAttributeString("image_unselected", image_unselected);
	}

	std::string	image_selected;
	if (node->hasAttribute("image_selected"))
	{
		node->getAttributeString("image_selected", image_selected);
	}

	EJoystickQuadrant quad = JQ_ORIGIN;
	if (node->hasAttribute("quadrant"))
	{
		quad = selectQuadrant(node);
	}

	LLJoystickAgentTurn* button = new LLJoystickAgentTurn(name, LLRect(),
														  image_unselected,
														  image_selected,
														  quad);

	if (node->hasAttribute("halign"))
	{
		LLFontGL::HAlign halign = selectFontHAlign(node);
		button->setHAlign(halign);
	}

	if (node->hasAttribute("scale_image"))
	{
		bool needs_scale = false;
		node->getAttributeBool("scale_image", needs_scale);
		button->setScaleImage(needs_scale);
	}

	button->initFromXML(node, parent);

	return button;
}

//-----------------------------------------------------------------------------
// LLJoystickAgentSlide
//-----------------------------------------------------------------------------

void LLJoystickAgentSlide::onMouseUp()
{
	F32 time = getElapsedHeldDownTime();
	if (time >= NUDGE_TIME)
	{
		return;
	}
	if (mInitialQuadrant == JQ_LEFT)
	{
		gAgent.moveLeftNudge(1);
	}
	else if (mInitialQuadrant == JQ_RIGHT)
	{
		gAgent.moveLeftNudge(-1);
	}
}

void LLJoystickAgentSlide::onHeldDown()
{
	updateSlop();

	S32 dx = mLastMouse.mX - mFirstMouse.mX + mInitialOffset.mX;
	S32 dy = mLastMouse.mY - mFirstMouse.mY + mInitialOffset.mY;

	// handle left-right sliding
	if (dx > mHorizSlopNear)
	{
		gAgent.moveLeft(-1);
	}
	else if (dx < -mHorizSlopNear)
	{
		gAgent.moveLeft(1);
	}

	// Handle forward/back movement
	if (dy > mVertSlopFar)
	{
		// If mouse is forward of run region run forward
		gAgent.moveAt(1);
	}
	else if (dy > mVertSlopNear)
	{
		// Else if mouse is forward of walk region walk forward
		gAgent.moveAtNudge(1);
	}
	else if (dy < -mVertSlopFar)
	{
		// Else if mouse is behind run region run backward
		gAgent.moveAt(-1);
	}
	else if (dy < -mVertSlopNear)
	{
		// Else if mouse is behind walk region walk backward
		gAgent.moveAtNudge(-1);
	}
}

LLXMLNodePtr LLJoystickAgentSlide::getXML(bool save_children) const
{
	LLXMLNodePtr node = LLJoystick::getXML();
	node->setName(LL_JOYSTICK_SLIDE);
	return node;
}

//static
LLView* LLJoystickAgentSlide::fromXML(LLXMLNodePtr node, LLView* parent,
									  LLUICtrlFactory* factory)
{
	std::string name = "button";
	node->getAttributeString("name", name);

	std::string	image_unselected;
	if (node->hasAttribute("image_unselected"))
	{
		node->getAttributeString("image_unselected", image_unselected);
	}

	std::string	image_selected;
	if (node->hasAttribute("image_selected"))
	{
		node->getAttributeString("image_selected", image_selected);
	}

	EJoystickQuadrant quad = JQ_ORIGIN;
	if (node->hasAttribute("quadrant"))
	{
		quad = selectQuadrant(node);
	}

	LLJoystickAgentSlide* button = new LLJoystickAgentSlide(name, LLRect(),
															image_unselected,
															image_selected,
															quad);

	if (node->hasAttribute("halign"))
	{
		LLFontGL::HAlign halign = selectFontHAlign(node);
		button->setHAlign(halign);
	}

	if (node->hasAttribute("scale_image"))
	{
		bool needs_scale = false;
		node->getAttributeBool("scale_image", needs_scale);
		button->setScaleImage(needs_scale);
	}

	button->initFromXML(node, parent);

	return button;
}

//-----------------------------------------------------------------------------
// LLJoystickCameraRotate
//-----------------------------------------------------------------------------

LLJoystickCameraRotate::LLJoystickCameraRotate(const std::string& name,
											   LLRect rect,
											   const std::string& out_img,
											   const std::string& in_img)
:	LLJoystick(name, rect, out_img, in_img, JQ_ORIGIN),
	mInLeft(false),
	mInTop(false),
	mInRight(false),
	mInBottom(false)
{
}

void LLJoystickCameraRotate::updateSlop()
{
	// Do the initial offset calculation based on mousedown location

	// Small fixed slop region
	mVertSlopNear = 16;
	mVertSlopFar = 32;

	mHorizSlopNear = 16;
	mHorizSlopFar = 32;
}

bool LLJoystickCameraRotate::handleMouseDown(S32 x, S32 y, MASK mask)
{
	updateSlop();

	// Set initial offset based on initial click location
	S32 horiz_center = getRect().getWidth() / 2;
	S32 vert_center = getRect().getHeight() / 2;

	S32 dx = x - horiz_center;
	S32 dy = y - vert_center;

	if (dy > dx && dy > -dx)
	{
		// Top
		mInitialOffset.mX = 0;
		mInitialOffset.mY = (mVertSlopNear + mVertSlopFar) / 2;
		mInitialQuadrant = JQ_UP;
	}
	else if (dy > dx && dy <= -dx)
	{
		// Left
		mInitialOffset.mX = - (mHorizSlopNear + mHorizSlopFar) / 2;
		mInitialOffset.mY = 0;
		mInitialQuadrant = JQ_LEFT;
	}
	else if (dy <= dx && dy <= -dx)
	{
		// Bottom
		mInitialOffset.mX = 0;
		mInitialOffset.mY = - (mVertSlopNear + mVertSlopFar) / 2;
		mInitialQuadrant = JQ_DOWN;
	}
	else
	{
		// Right
		mInitialOffset.mX = (mHorizSlopNear + mHorizSlopFar) / 2;
		mInitialOffset.mY = 0;
		mInitialQuadrant = JQ_RIGHT;
	}

	return LLJoystick::handleMouseDown(x, y, mask);
}

void LLJoystickCameraRotate::onHeldDown()
{
	updateSlop();

	S32 dx = mLastMouse.mX - mFirstMouse.mX + mInitialOffset.mX;
	S32 dy = mLastMouse.mY - mFirstMouse.mY + mInitialOffset.mY;

	// Left-right rotation
	if (dx > mHorizSlopNear)
	{
		gAgent.unlockView();
		gAgent.setOrbitLeftKey(getOrbitRate());
	}
	else if (dx < -mHorizSlopNear)
	{
		gAgent.unlockView();
		gAgent.setOrbitRightKey(getOrbitRate());
	}

	// Over/under rotation
	if (dy > mVertSlopNear)
	{
		gAgent.unlockView();
		gAgent.setOrbitUpKey(getOrbitRate());
	}
	else if (dy < -mVertSlopNear)
	{
		gAgent.unlockView();
		gAgent.setOrbitDownKey(getOrbitRate());
	}
}

F32 LLJoystickCameraRotate::getOrbitRate()
{
	F32 time = getElapsedHeldDownTime();
	if (time >= NUDGE_TIME)
	{
		return 1;
	}
	return ORBIT_NUDGE_RATE + time * (1.f - ORBIT_NUDGE_RATE)/ NUDGE_TIME;
}

// Only used for drawing
void LLJoystickCameraRotate::setToggleState(bool left, bool top, bool right,
											bool bottom)
{
	mInLeft = left;
	mInTop = top;
	mInRight = right;
	mInBottom = bottom;
}

void LLJoystickCameraRotate::draw()
{
	LLGLSUIDefault gls_ui;

	getImageUnselected()->draw(0, 0);

	if (mInTop)
	{
		drawRotatedImage(getImageSelected()->getImage(), 0);
	}

	if (mInRight)
	{
		drawRotatedImage(getImageSelected()->getImage(), 1);
	}

	if (mInBottom)
	{
		drawRotatedImage(getImageSelected()->getImage(), 2);
	}

	if (mInLeft)
	{
		drawRotatedImage(getImageSelected()->getImage(), 3);
	}

	if (sDebugRects)
	{
		drawDebugRect();
	}
}

// Draws image rotated by multiples of 90 degrees
void LLJoystickCameraRotate::drawRotatedImage(LLGLTexture* image,
											  S32 rotations)
{
	S32 width = image->getWidth();
	S32 height = image->getHeight();

	static const F32 uv[][2] =
	{
		{ 1.f, 1.f },
		{ 0.f, 1.f },
		{ 0.f, 0.f },
		{ 1.f, 0.f }
	};

	gGL.getTexUnit(0)->bind(image);

	gGL.color4fv(UI_VERTEX_COLOR.mV);

	gGL.begin(LLRender::TRIANGLES);
	{
		gGL.texCoord2fv(uv[rotations % 4]);
		gGL.vertex2i(width, height);

		gGL.texCoord2fv(uv[(rotations + 1) % 4]);
		gGL.vertex2i(0, height);

		gGL.texCoord2fv(uv[(rotations + 2) % 4]);
		gGL.vertex2i(0, 0);

		gGL.texCoord2fv(uv[rotations % 4]);
		gGL.vertex2i(width, height);

		gGL.texCoord2fv(uv[(rotations + 2) % 4]);
		gGL.vertex2i(0, 0);

		gGL.texCoord2fv(uv[(rotations + 3) % 4]);
		gGL.vertex2i(width, 0);
	}
	gGL.end();
}

//-----------------------------------------------------------------------------
// LLJoystickCameraTrack
//-----------------------------------------------------------------------------

void LLJoystickCameraTrack::onHeldDown()
{
	updateSlop();

	S32 dx = mLastMouse.mX - mFirstMouse.mX + mInitialOffset.mX;
	S32 dy = mLastMouse.mY - mFirstMouse.mY + mInitialOffset.mY;

	if (dx > mVertSlopNear)
	{
		gAgent.unlockView();
		gAgent.setPanRightKey(getOrbitRate());
	}
	else if (dx < -mVertSlopNear)
	{
		gAgent.unlockView();
		gAgent.setPanLeftKey(getOrbitRate());
	}

	if (dy > mVertSlopNear)
	{
		gAgent.unlockView();
		gAgent.setPanUpKey(getOrbitRate());
	}
	else if (dy < -mVertSlopNear)
	{
		gAgent.unlockView();
		gAgent.setPanDownKey(getOrbitRate());
	}
}

//-----------------------------------------------------------------------------
// LLJoystickCameraZoom
//-----------------------------------------------------------------------------

LLJoystickCameraZoom::LLJoystickCameraZoom(const std::string& name,
										   LLRect rect,
										   const std::string& out_img,
										   const std::string& plus_in_img,
										   const std::string& minus_in_img)
:	LLJoystick(name, rect, out_img, LLStringUtil::null, JQ_ORIGIN),
	mInTop(false),
	mInBottom(false)
{
	mPlusInImage = LLUI::getUIImage(plus_in_img);
	mMinusInImage = LLUI::getUIImage(minus_in_img);
}

bool LLJoystickCameraZoom::handleMouseDown(S32 x, S32 y, MASK mask)
{
	bool handled = LLJoystick::handleMouseDown(x, y, mask);
	if (handled)
	{
		if (mFirstMouse.mY > getRect().getHeight() / 2)
		{
			mInitialQuadrant = JQ_UP;
		}
		else
		{
			mInitialQuadrant = JQ_DOWN;
		}
	}
	return handled;
}

void LLJoystickCameraZoom::onHeldDown()
{
	updateSlop();

	constexpr F32 FAST_RATE = 2.5f; // Two and a half times the normal rate

	S32 dy = mLastMouse.mY - mFirstMouse.mY + mInitialOffset.mY;

	if (dy > mVertSlopFar)
	{
		// Zoom in fast
		gAgent.unlockView();
		gAgent.setOrbitInKey(FAST_RATE);
	}
	else if (dy > mVertSlopNear)
	{
		// Zoom in slow
		gAgent.unlockView();
		gAgent.setOrbitInKey(getOrbitRate());
	}
	else if (dy < -mVertSlopFar)
	{
		// Zoom out fast
		gAgent.unlockView();
		gAgent.setOrbitOutKey(FAST_RATE);
	}
	else if (dy < -mVertSlopNear)
	{
		// Zoom out slow
		gAgent.unlockView();
		gAgent.setOrbitOutKey(getOrbitRate());
	}
}

// Only used for drawing
void LLJoystickCameraZoom::setToggleState(bool top, bool bottom)
{
	mInTop = top;
	mInBottom = bottom;
}

void LLJoystickCameraZoom::draw()
{
	if (mInTop)
	{
		mPlusInImage->draw(0,0);
	}
	else if (mInBottom)
	{
		mMinusInImage->draw(0,0);
	}
	else
	{
		getImageUnselected()->draw(0, 0);
	}

	if (sDebugRects)
	{
		drawDebugRect();
	}
}

void LLJoystickCameraZoom::updateSlop()
{
	mVertSlopNear = getRect().getHeight() / 4;
	mVertSlopFar = getRect().getHeight() / 2;

	mHorizSlopNear = getRect().getWidth() / 4;
	mHorizSlopFar = getRect().getWidth() / 2;

	// Compute initial mouse offset based on initial quadrant. Place the mouse
	// evenly between the near and far zones.
	switch (mInitialQuadrant)
	{
		case JQ_ORIGIN:
			mInitialOffset.set(0, 0);
			break;

		case JQ_UP:
			mInitialOffset.mX = 0;
			mInitialOffset.mY = (mVertSlopNear + mVertSlopFar) / 2;
			break;

		case JQ_DOWN:
			mInitialOffset.mX = 0;
			mInitialOffset.mY = - (mVertSlopNear + mVertSlopFar) / 2;
			break;

		case JQ_LEFT:
			mInitialOffset.mX = - (mHorizSlopNear + mHorizSlopFar) / 2;
			mInitialOffset.mY = 0;
			break;

		case JQ_RIGHT:
			mInitialOffset.mX = (mHorizSlopNear + mHorizSlopFar) / 2;
			mInitialOffset.mY = 0;
			break;

		default:
			llerrs << "Bad switch case" << llendl;
	}
}

F32 LLJoystickCameraZoom::getOrbitRate()
{
	F32 time = getElapsedHeldDownTime();
	if (time >= NUDGE_TIME)
	{
		return 1.f;
	}
	return ORBIT_NUDGE_RATE + time * (1.f - ORBIT_NUDGE_RATE) / NUDGE_TIME;
}
