/**
 * @file llvirtualtrackball.cpp
 * @brief LLVirtualTrackball class definition
 *
 * $LicenseInfo:firstyear=2018&license=viewergpl$
 *
 * Copyright (c) 2018, Linden Research, Inc.
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

#include "linden_common.h"

#include "llvirtualtrackball.h"

#include "llbutton.h"
#include "llpanel.h"
#include "llrender.h"
#include "lltextbox.h"

static const std::string LL_SUN_MOON_TRACKBALL_TAG = "sun_moon_trackball";
static LLRegisterWidget<LLVirtualTrackball> r33(LL_SUN_MOON_TRACKBALL_TAG);

// UI elements constants
constexpr S32 TRACKBALL_WIDTH = 150;
constexpr S32 TRACKBALL_HEIGHT = 150;
constexpr S32 TRACKBALL_BTN_SIZE = 32;		// Width & height
constexpr S32 TRACKBALL_AXIS_OFFSET = 16;	// Offset from left/top sides
const std::string TRACKBALL_LABEL_N = "N";
const std::string TRACKBALL_LABEL_S = "S";
const std::string TRACKBALL_LABEL_W = "E";
const std::string TRACKBALL_LABEL_E = "W";

LLVirtualTrackball::LLVirtualTrackball(const std::string& name,
									   const LLRect& ui_rect,
									   void (*commit_cb)(LLUICtrl*, void*),
									   void* userdata)
:	LLUICtrl(name, ui_rect, true, commit_cb, userdata,
			 FOLLOWS_TOP | FOLLOWS_LEFT),
	mPrevX(0),
	mPrevY(0),
	mIncrementMouse(DEG_TO_RAD * 0.5f),
	mIncrementBtn(DEG_TO_RAD * 3.f),
	mThumbMode(SUN),
	mImgMoonBack(LLUI::getUIImage("track_control_moon_back.png")),
	mImgMoonFront(LLUI::getUIImage("track_control_moon_front.png")),
	mImgSunBack(LLUI::getUIImage("track_control_sun_back.png")),
	mImgSunFront(LLUI::getUIImage("track_control_sun_front.png")),
	mImgSphere(LLUI::getUIImage("track_control_sphere.png"))
{
	LLRect border_rect = getLocalRect();
	mBorder = new LLViewBorder("border", border_rect);
	addChild(mBorder);

	S32 center_x = border_rect.getCenterX();
	S32 center_y = border_rect.getCenterY();
	// Offset from the axis for left/top sides
	S32 axis_offset_rb = TRACKBALL_BTN_SIZE - TRACKBALL_AXIS_OFFSET;

	LLRect rect = LLRect(center_x - TRACKBALL_AXIS_OFFSET, border_rect.mTop,
						 center_x + axis_offset_rb,
						 border_rect.mTop - TRACKBALL_BTN_SIZE);
	mBtnRotateTop = new LLButton("button_rotate_top", rect,
								 "track_control_rotate_top.png",
								 "track_control_rotate_top_active.png",
								 "", onRotateTopClick, this);
	mBtnRotateTop->setHeldDownCallback(onRotateTopClickNoSound);
	addChild(mBtnRotateTop);

	rect.translate(0, -TRACKBALL_BTN_SIZE / 2);
	mLabelN = new LLTextBox("labelN", rect, TRACKBALL_LABEL_N);
	addChild(mLabelN);

	rect = LLRect(border_rect.mRight - TRACKBALL_BTN_SIZE,
				  center_y + TRACKBALL_AXIS_OFFSET,
				  border_rect.mRight, center_y - axis_offset_rb);
	mBtnRotateRight = new LLButton("button_rotate_right", rect,
								 "track_control_rotate_right_side.png",
								 "track_control_rotate_right_side_active.png",
								 "", onRotateRightClick, this);
	mBtnRotateRight->setHeldDownCallback(onRotateRightClickNoSound);
	addChild(mBtnRotateRight);

	mLabelW = new LLTextBox("labelW", rect, TRACKBALL_LABEL_W);
	addChild(mLabelW);

	rect = LLRect(center_x - TRACKBALL_AXIS_OFFSET,
				  border_rect.mBottom + TRACKBALL_BTN_SIZE,
				  center_x + axis_offset_rb, border_rect.mBottom);
	mBtnRotateBottom = new LLButton("button_rotate_bottom", rect,
									"track_control_rotate_bottom.png",
									"track_control_rotate_bottom_active.png",
									"", onRotateBottomClick, this);
	mBtnRotateBottom->setHeldDownCallback(onRotateBottomClickNoSound);
	addChild(mBtnRotateBottom);

	mLabelS = new LLTextBox("labelS", rect, TRACKBALL_LABEL_S);
	addChild(mLabelS);

	rect = LLRect(border_rect.mLeft, center_y + TRACKBALL_AXIS_OFFSET,
				  border_rect.mLeft + TRACKBALL_BTN_SIZE,
				  center_y - axis_offset_rb);
	mBtnRotateLeft = new LLButton("button_rotate_left", rect,
								  "track_control_rotate_left_side.png",
								  "track_control_rotate_left_side_active.png",
								  "", onRotateLeftClick, this);
	mBtnRotateLeft->setHeldDownCallback(onRotateLeftClickNoSound);
	addChild(mBtnRotateLeft);

	rect.translate(TRACKBALL_BTN_SIZE / 2, 0);
	mLabelE = new LLTextBox("labelE", rect, TRACKBALL_LABEL_E);
	addChild(mLabelE);

	S32 half_width = mImgSphere->getWidth() / 2;
	S32 half_height = mImgSphere->getHeight() / 2;
	rect = LLRect(center_x - half_width, center_y + half_height,
				  center_x + half_width, center_y - half_height);
	mTouchArea = new LLPanel("touch area", rect);
	addChild(mTouchArea);
}

bool LLVirtualTrackball::pointInTouchCircle(S32 x, S32 y) const
{
	S32 x1 = x - mTouchArea->getRect().getCenterX();
	S32 y1 = y - mTouchArea->getRect().getCenterY();
	S32 radius = mTouchArea->getRect().getWidth() / 2;
	return x1 * x1 + y1 * y1 <= radius * radius;
}

void LLVirtualTrackball::drawThumb(S32 x, S32 y, EThumbMode mode,
								   bool upper_hemi)
{
	LLUIImage* thumb;
	if (mode == EThumbMode::SUN)
	{
		thumb = upper_hemi ? mImgSunFront : mImgSunBack;
	}
	else
	{
		thumb = upper_hemi ? mImgMoonFront : mImgMoonBack;
	}
	S32 half_width = thumb->getWidth() / 2;
	S32 half_height = thumb->getHeight() / 2;
	thumb->draw(LLRect(x - half_width, y + half_height, x + half_width,
					   y - half_height));
}

//virtual
void LLVirtualTrackball::draw()
{
	const LLRect& rect = mTouchArea->getRect();
	S32 half_width = rect.getWidth() / 2;
	S32 half_height = rect.getHeight() / 2;

	LLVector3 draw_point = LLVector3::x_axis * mValue;
	draw_point.mV[VX] = (draw_point.mV[VX] + 1.f) * half_width + rect.mLeft;
	draw_point.mV[VY] = (draw_point.mV[VY] + 1.f) * half_height + rect.mBottom;

	bool upper = draw_point.mV[VZ] >= 0.f;
	mImgSphere->draw(rect, upper ? UI_VERTEX_COLOR : UI_VERTEX_COLOR % 0.5f);
	drawThumb(draw_point.mV[VX], draw_point.mV[VY], mThumbMode, upper);

	if (sDebugRects)
	{
		gGL.color4fv(LLColor4::red.mV);
		gl_circle_2d(rect.getCenterX(), rect.getCenterY(),
					 mImgSphere->getWidth() / 2, 60, false);
		gl_circle_2d(draw_point.mV[VX], draw_point.mV[VY],
					 mImgSunFront->getWidth() / 2, 12, false);
	}

	bool enabled = isInEnabledChain();
	mLabelN->setVisible(enabled);
	mLabelS->setVisible(enabled);
	mLabelW->setVisible(enabled);
	mLabelE->setVisible(enabled);
	mBtnRotateTop->setVisible(enabled);
	mBtnRotateBottom->setVisible(enabled);
	mBtnRotateLeft->setVisible(enabled);
	mBtnRotateRight->setVisible(enabled);

	LLView::draw();
}

//virtual
bool LLVirtualTrackball::handleKeyHere(KEY key, MASK mask)
{
	switch (key)
	{
		case KEY_DOWN:
			onRotateTopClick(this);
			break;

		case KEY_LEFT:
			onRotateRightClick(this);
			break;

		case KEY_UP:
			onRotateBottomClick(this);
			break;

		case KEY_RIGHT:
			onRotateLeftClick(this);
			break;

		default:
			return false;
	}
	return true;
}

//virtual
bool LLVirtualTrackball::handleHover(S32 x, S32 y, MASK mask)
{
	if (hasMouseCapture())
	{
		if (mDragMode == DRAG_SCROLL)
		{
			// Trackball (move to roll) mode
			LLQuaternion delta;
			F32 rot_x = x - mPrevX;
			F32 rot_y = y - mPrevY;

			F32 abs_rot_x = fabsf(rot_x);
			if (abs_rot_x > 1.f)
			{
				// Changing X: rotate around Y axis
				delta.setAngleAxis(mIncrementMouse * abs_rot_x, 0.f,
								   rot_x < 0.f ? -1.f : 1.f, 0.f);
				mValue *= delta;
			}
			F32 abs_rot_y = fabsf(rot_y);
			if (abs_rot_y > 1.f)
			{
				// Changing Y: rotate around X axis
				delta.setAngleAxis(mIncrementMouse * abs_rot_y,
								   rot_y < 0.f ? 1.f : -1.f, 0.f, 0.f);
				mValue *= delta;
			}
		}
		else
		{
			// Set on click mode
			if (!pointInTouchCircle(x, y))
			{
				// Do not drag outside the circle
				return true;
			}
			const LLRect& rect = mTouchArea->getRect();
			F32 radius = rect.getWidth() / 2;
			F32 xx = x - rect.getCenterX();
			F32 yy = y - rect.getCenterY();
			F32 dist = sqrtf(xx * xx + yy * yy);
			F32 altitude = llclamp(acosf(dist / radius), 0.f, F_PI_BY_TWO);
			F32 azimuth = llclamp(acosf(xx / dist), 0.f, F_PI);
			if (yy < 0.f)
			{
				azimuth = F_TWO_PI - azimuth;
			}

			LLVector3 draw_point = LLVector3::x_axis * mValue;
			if (draw_point.mV[VZ] >= 0.f)
			{
				if (is_approx_zero(altitude)) // do not change the hemisphere
				{
					altitude = -F_APPROXIMATELY_ZERO;
				}
				else
				{
					altitude = -altitude;
				}
			}
			mValue.setAngleAxis(altitude, 0.f, 1.f, 0.f);
			LLQuaternion az_quat;
			az_quat.setAngleAxis(azimuth, 0.f, 0.f, 1.f);
			mValue *= az_quat;
		}

		mValue.normalize();
		mPrevX = x;
		mPrevY = y;
		onCommit();
	}

	return true;
}

//virtual
bool LLVirtualTrackball::handleMouseUp(S32 x, S32 y, MASK mask)
{
	if (hasMouseCapture())
	{
		mPrevX = mPrevY = 0;
		gFocusMgr.setMouseCapture(NULL);
		make_ui_sound("UISndClickRelease");
	}

	return LLView::handleMouseUp(x, y, mask);
}

//virtual
bool LLVirtualTrackball::handleMouseDown(S32 x, S32 y, MASK mask)
{
	if (pointInTouchCircle(x, y))
	{
		mPrevX = x;
		mPrevY = y;
		gFocusMgr.setMouseCapture(this);
		mDragMode = mask == MASK_CONTROL ? DRAG_SCROLL : DRAG_SET;
		make_ui_sound("UISndClick");
	}
	return LLView::handleMouseDown(x, y, mask);
}

//virtual
LLSD LLVirtualTrackball::getValue() const
{
	return mValue.getValue();
}

void LLVirtualTrackball::setValue(F32 x, F32 y, F32 z, F32 w)
{
	mValue.set(x, y, z, w);
}

//virtual
void LLVirtualTrackball::setValue(const LLSD& value)
{
	if (value.isArray() && value.size() == 4)
	{
		mValue.setValue(value);
	}
}

void LLVirtualTrackball::setValueAndCommit(const LLQuaternion& value)
{
	mValue = value;
	onCommit();
}

void LLVirtualTrackball::getAzimuthAndElevationDeg(F32& azim, F32& elev)
{
	mValue.getAzimuthAndElevation(azim, elev);
	azim *= RAD_TO_DEG;
	elev *= RAD_TO_DEG;
}

//static
void LLVirtualTrackball::onRotateTopClick(void* userdata)
{
	LLVirtualTrackball* self = (LLVirtualTrackball*)userdata;
	if (self && self->getEnabled())
	{
		LLQuaternion delta;
		delta.setAngleAxis(self->mIncrementBtn, 1.f, 0.f, 0.f); 
		self->setValueAndCommit(self->mValue * delta);
		make_ui_sound("UISndClick");
	}
}

//static
void LLVirtualTrackball::onRotateBottomClick(void* userdata)
{
	LLVirtualTrackball* self = (LLVirtualTrackball*)userdata;
	if (self && self->getEnabled())
	{
		LLQuaternion delta;
		delta.setAngleAxis(self->mIncrementBtn, -1.f, 0.f, 0.f); 
		self->setValueAndCommit(self->mValue * delta);
		make_ui_sound("UISndClick");
	}
}

//static
void LLVirtualTrackball::onRotateLeftClick(void* userdata)
{
	LLVirtualTrackball* self = (LLVirtualTrackball*)userdata;
	if (self && self->getEnabled())
	{
		LLQuaternion delta;
		delta.setAngleAxis(self->mIncrementBtn, 0.f, 1.f, 0.f); 
		self->setValueAndCommit(self->mValue * delta);
		make_ui_sound("UISndClick");
	}
}

//static
void LLVirtualTrackball::onRotateRightClick(void* userdata)
{
	LLVirtualTrackball* self = (LLVirtualTrackball*)userdata;
	if (self && self->getEnabled())
	{
		LLQuaternion delta;
		delta.setAngleAxis(self->mIncrementBtn, 0.f, -1.f, 0.f); 
		self->setValueAndCommit(self->mValue * delta);
		make_ui_sound("UISndClick");
	}
}

//static
void LLVirtualTrackball::onRotateTopClickNoSound(void* userdata)
{
	LLVirtualTrackball* self = (LLVirtualTrackball*)userdata;
	if (self && self->getEnabled())
	{
		LLQuaternion delta;
		delta.setAngleAxis(self->mIncrementBtn, 1.f, 0.f, 0.f); 
		self->setValueAndCommit(self->mValue * delta);
	}
}

//static
void LLVirtualTrackball::onRotateBottomClickNoSound(void* userdata)
{
	LLVirtualTrackball* self = (LLVirtualTrackball*)userdata;
	if (self && self->getEnabled())
	{
		LLQuaternion delta;
		delta.setAngleAxis(self->mIncrementBtn, -1.f, 0.f, 0.f); 
		self->setValueAndCommit(self->mValue * delta);
	}
}

//static
void LLVirtualTrackball::onRotateLeftClickNoSound(void* userdata)
{
	LLVirtualTrackball* self = (LLVirtualTrackball*)userdata;
	if (self && self->getEnabled())
	{
		LLQuaternion delta;
		delta.setAngleAxis(self->mIncrementBtn, 0.f, 1.f, 0.f); 
		self->setValueAndCommit(self->mValue * delta);
	}
}

//static
void LLVirtualTrackball::onRotateRightClickNoSound(void* userdata)
{
	LLVirtualTrackball* self = (LLVirtualTrackball*)userdata;
	if (self && self->getEnabled())
	{
		LLQuaternion delta;
		delta.setAngleAxis(self->mIncrementBtn, 0.f, -1.f, 0.f); 
		self->setValueAndCommit(self->mValue * delta);
	}
}

//virtual
LLXMLNodePtr LLVirtualTrackball::getXML(bool save_children) const
{
	LLXMLNodePtr node = LLUICtrl::getXML();

	node->setName(LL_SUN_MOON_TRACKBALL_TAG);

	node->createChild("increment_angle_mouse",
					  true)->setIntValue(mIncrementMouse);
	node->createChild("increment_angle_btn", true)->setIntValue(mIncrementBtn);
	node->createChild("thumb_mode",
					  true)->setStringValue(mThumbMode == MOON ? "moon"
															   : "sun");

	return node;
}

//static
LLView* LLVirtualTrackball::fromXML(LLXMLNodePtr node, LLView* parent,
									LLUICtrlFactory* factory)
{
	std::string name = LL_SUN_MOON_TRACKBALL_TAG;
	node->getAttributeString("name", name);

	LLRect rect;
	createRect(node, rect, parent,
			   LLRect(0, TRACKBALL_HEIGHT, TRACKBALL_WIDTH, 0));

	LLUICtrlCallback callback = NULL;
	LLVirtualTrackball* trackball = new LLVirtualTrackball(name, rect,
														   callback, NULL);

	F32 increment_angle_mouse = 0.5f;
	node->getAttributeF32("increment_angle_mouse", increment_angle_mouse);
	trackball->mIncrementMouse = DEG_TO_RAD * increment_angle_mouse;

	F32 increment_angle_btn = 3.f;
	node->getAttributeF32("increment_angle_btn", increment_angle_btn);
	trackball->mIncrementBtn = DEG_TO_RAD * increment_angle_btn;

	std::string thumb_mode;
	node->getAttributeString("thumb_mode", thumb_mode);
	trackball->mThumbMode = thumb_mode == "moon" ? MOON : SUN;

	trackball->initFromXML(node, parent);

	return trackball;
}
