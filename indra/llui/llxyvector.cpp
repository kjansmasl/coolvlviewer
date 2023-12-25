/**
 * @file llxyvector.cpp
 * @brief LLXYVector class definition
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

#include <math.h>			// For fabs()

#include "llxyvector.h"

#include "lllineeditor.h"
#include "llpanel.h"
#include "llrender.h"
#include "lltextbox.h"

static const std::string LL_XY_VECTOR_TAG = "xy_vector";
static LLRegisterWidget<LLXYVector> r34(LL_XY_VECTOR_TAG);

// UI elements constants
constexpr S32 EDIT_BAR_HEIGHT = 20;
constexpr S32 XY_VECTOR_PADDING = 4;
constexpr S32 XY_VECTOR_LABEL_WIDTH = 16;
constexpr S32 XY_VECTOR_WIDTH = 120;
constexpr S32 XY_VECTOR_HEIGHT = 140;

// Drawing constants
constexpr F32 CENTER_CIRCLE_RADIUS = 2.f;
constexpr F32 ARROW_ANGLE = 30.f * DEG_TO_RAD;
constexpr S32 ARROW_LENGTH_LONG = 10;
constexpr S32 ARROW_LENGTH_SHORT = 6;

// Helper function
void draw_arrow(S32 tail_x, S32 tail_y, S32 tip_x, S32 tip_y,
				const LLColor4& color)
{
	gl_line_2d(tail_x, tail_y, tip_x, tip_y, color);

	S32 dx = tip_x - tail_x;
	S32 dy = tip_y - tail_y;
	S32 length;
	if (fabs(dx) < ARROW_LENGTH_LONG && fabs(dy) < ARROW_LENGTH_LONG)
	{
		length = ARROW_LENGTH_SHORT;
	}
	else
	{
		length = ARROW_LENGTH_LONG;
	}
	F32 theta = atan2f(dy, dx);
	F32 x = tip_x - length * cosf(theta + ARROW_ANGLE);
	F32 y = tip_y - length * sinf(theta + ARROW_ANGLE);
	F32 x2 = tip_x - length * cosf(theta - ARROW_ANGLE);
	F32 y2 = tip_y - length * sinf(theta - ARROW_ANGLE);

	gl_triangle_2d(tip_x, tip_y, x, y, x2, y2, color, true);
}

LLXYVector::LLXYVector(const std::string& name, const LLRect& ui_rect,
					   void (*commit_cb)(LLUICtrl*, void*), void* userdata)
:	LLUICtrl(name, ui_rect, true, commit_cb, userdata,
			 FOLLOWS_TOP | FOLLOWS_LEFT),
	mGhostX(0),
	mGhostY(0),
	mValueX(0.f),
	mValueY(0.f),
	mMinValueX(-1.f),
	mMinValueY(-1.f),
	mMaxValueX(1.f),
	mMaxValueY(1.f),
	mLogScaleX(1.f),
	mLogScaleY(1.f),
	mIncrementX(0.05f),
	mIncrementY(0.05f),
	mArrowColor(LLColor4::white),
	mAreaColor(LLColor4::grey4),
	mGridColor(LLColor4::grey % 0.25f),
	mGhostColor(LLColor4::white % 0.3f),
	mLogarithmic(false)
{
	LLRect border_rect = getLocalRect();
	mBorder = new LLViewBorder("border", border_rect);
	addChild(mBorder);

	LLRect rect = LLRect(XY_VECTOR_PADDING,
						 border_rect.mTop - XY_VECTOR_PADDING,
						 XY_VECTOR_LABEL_WIDTH,
						 border_rect.getHeight() - EDIT_BAR_HEIGHT);
	mXLabel = new LLTextBox("x_label", rect, "X:");
	addChild(mXLabel);

	rect = LLRect(XY_VECTOR_PADDING + XY_VECTOR_LABEL_WIDTH,
				  border_rect.mTop - XY_VECTOR_PADDING,
				  border_rect.getCenterX(),
				  border_rect.getHeight() - EDIT_BAR_HEIGHT);
	mXEntry = new LLLineEditor("x_entry", rect);
	mXEntry->setPrevalidate(LLLineEditor::prevalidateFloat);
	mXEntry->setCommitCallback(onEditChange);
	mXEntry->setCallbackUserData(this);
	addChild(mXEntry);

	rect = LLRect(border_rect.getCenterX() + XY_VECTOR_PADDING,
				  border_rect.mTop - XY_VECTOR_PADDING,
				  border_rect.getCenterX() + XY_VECTOR_LABEL_WIDTH,
				  border_rect.getHeight() - EDIT_BAR_HEIGHT);
	mYLabel = new LLTextBox("y_label", rect, "Y:");
	addChild(mYLabel);

	rect = LLRect(border_rect.getCenterX() + XY_VECTOR_PADDING +
				  XY_VECTOR_LABEL_WIDTH,
				  border_rect.getHeight() - XY_VECTOR_PADDING,
				  border_rect.getWidth() - XY_VECTOR_PADDING,
				  border_rect.getHeight() - EDIT_BAR_HEIGHT);
	mYEntry = new LLLineEditor("y_entry", rect);
	mYEntry->setPrevalidate(LLLineEditor::prevalidateFloat);
	mYEntry->setCommitCallback(onEditChange);
	mYEntry->setCallbackUserData(this);
	addChild(mYEntry);

	rect = LLRect(XY_VECTOR_PADDING,
				  border_rect.mTop - EDIT_BAR_HEIGHT - XY_VECTOR_PADDING,
				  border_rect.getWidth() - XY_VECTOR_PADDING,
				  XY_VECTOR_PADDING);
	mTouchArea = new LLPanel("touch area", rect);
	addChild(mTouchArea);
}

//virtual
bool LLXYVector::postBuild()
{
	if (mMaxValueX != 0.f && mMaxValueY != 0.f)
	{
		mLogScaleX = 2.f * logf(mMaxValueX) /
					 (F32)mTouchArea->getRect().getWidth();
		mLogScaleY = 2.f * logf(mMaxValueY) /
					 (F32)mTouchArea->getRect().getHeight();
	}
	return true;
}

//virtual
void LLXYVector::draw()
{
	const LLRect& rect = mTouchArea->getRect();
	S32 center_x = rect.getCenterX();
	S32 center_y = rect.getCenterY();

	S32 point_x = center_x;
	S32 point_y = center_y;
	if (mMaxValueX != 0.f && mMaxValueY != 0.f)
	{
		if (mLogarithmic)
		{
			if (mValueX >= 0.f)
			{
				point_x += logf(1.f + mValueX) / mLogScaleX;
			}
			else
			{
				point_x += -logf(1.f - mValueX) / mLogScaleX;
			}
			if (mValueY >= 0.f)
			{
				point_y += logf(1.f + mValueY) / mLogScaleY;
			}
			else
			{
				point_y += -logf(1.f - mValueY) / mLogScaleY;
			}
		}
		else
		{
			point_x += mValueX * rect.getWidth() / (2.f * mMaxValueX);
			point_y += mValueY * rect.getHeight() / (2.f * mMaxValueY);
		}
	}

	// Fill up touch area
	gl_rect_2d(rect, mAreaColor, true);

	// Draw the grid
	gl_line_2d(center_x, rect.mTop, center_x, rect.mBottom, mGridColor);
	gl_line_2d(rect.mLeft, center_y, rect.mRight, center_y, mGridColor);

	// Draw the ghost
	if (hasMouseCapture())
	{
		draw_arrow(center_x, center_y, mGhostX, mGhostY, mGhostColor);
	}
	else
	{
		mGhostX = point_x;
		mGhostY = point_y;
	}

	if (fabs(mValueX) >= mIncrementX || fabs(mValueY) >= mIncrementY)
	{
		// Draw the vector arrow
		draw_arrow(center_x, center_y, point_x, point_y, mArrowColor);
	}
	else
	{
		// Skip the arrow and set the color for the center circle
		gGL.color4fv(mArrowColor.mV);
	}

	// Draw the center circle
	gl_circle_2d(center_x, center_y, CENTER_CIRCLE_RADIUS, 12, true);

	bool enabled = isInEnabledChain();
	mXEntry->setEnabled(enabled);
	mYEntry->setEnabled(enabled);

	LLView::draw();
}

//virtual
bool LLXYVector::handleHover(S32 x, S32 y, MASK mask)
{
	if (hasMouseCapture())
	{
		const LLRect& rect = mTouchArea->getRect();
		F32 value_x, value_y;
		if (mLogarithmic)
		{
			value_x = llfastpow(F_E,
							    mLogScaleX * abs(x - rect.getCenterX())) - 1.f;
			if (x < mTouchArea->getRect().getCenterX())
			{
				value_x = -value_x;
			}
			value_y = llfastpow(F_E,
								mLogScaleY * abs(y - rect.getCenterY())) - 1.f;
			if (y < mTouchArea->getRect().getCenterY())
			{
				value_y = -value_y;
			}
		}
		else
		{
			value_x = 2.f * mMaxValueX * F32(x - rect.getCenterX()) /
					  F32(rect.getWidth());
			value_y = 2.f * mMaxValueY * F32(y - rect.getCenterY()) /
					  F32(rect.getHeight());
		}
		setValueAndCommit(value_x, value_y);
	}
	return true;
}

//virtual
bool LLXYVector::handleMouseUp(S32 x, S32 y, MASK mask)
{
	if (hasMouseCapture())
	{
		gFocusMgr.setMouseCapture(NULL);
		make_ui_sound("UISndClickRelease");
	}

	if (mTouchArea->getRect().pointInRect(x, y))
	{
		return true;
	}

	return LLUICtrl::handleMouseUp(x, y, mask);
}

//virtual
bool LLXYVector::handleMouseDown(S32 x, S32 y, MASK mask)
{
	if (mTouchArea->getRect().pointInRect(x, y))
	{
		gFocusMgr.setMouseCapture(this);
		make_ui_sound("UISndClick");
		return true;
	}

	return LLUICtrl::handleMouseDown(x, y, mask);
}

//virtual
LLSD LLXYVector::getValue() const
{
	LLSD value;
	value.append(mValueX);
	value.append(mValueY);
	return value;
}

void LLXYVector::update()
{
	mXEntry->setValue(mValueX);
	mYEntry->setValue(mValueY);
}

void LLXYVector::setValue(F32 x, F32 y)
{
	mValueX = ll_round(llclamp(x, mMinValueX, mMaxValueX), mIncrementX);
	mValueY = ll_round(llclamp(y, mMinValueY, mMaxValueY), mIncrementY);
	update();
}

//virtual
void LLXYVector::setValue(const LLSD& value)
{
	if (value.isArray())
	{
		setValue(value[0].asReal(), value[1].asReal());
	}
}

void LLXYVector::setValueAndCommit(F32 x, F32 y)
{
	if (mValueX != x || mValueY != y)
	{
		setValue(x, y);
		onCommit();
	}
}

//static
void LLXYVector::onEditChange(LLUICtrl*, void* userdata)
{
	LLXYVector* self = (LLXYVector*)userdata;
	if (self && self->getEnabled())
	{
		self->setValueAndCommit(self->mXEntry->getValue().asReal(),
								self->mYEntry->getValue().asReal());
	}
}

//virtual
LLXMLNodePtr LLXYVector::getXML(bool save_children) const
{
	LLXMLNodePtr node = LLUICtrl::getXML();

	node->setName(LL_XY_VECTOR_TAG);

	node->createChild("min_val_x", true)->setFloatValue(mMinValueX);
	node->createChild("max_val_x", true)->setFloatValue(mMaxValueX);
	node->createChild("increment_x", true)->setFloatValue(mIncrementX);
	node->createChild("min_val_y", true)->setFloatValue(mMinValueY);
	node->createChild("max_val_y", true)->setFloatValue(mMaxValueY);
	node->createChild("increment_y", true)->setFloatValue(mIncrementY);
	node->createChild("logarithmic", true)->setBoolValue(mLogarithmic);

	return node;
}

//static
LLView* LLXYVector::fromXML(LLXMLNodePtr node, LLView* parent,
							LLUICtrlFactory* factory)
{
	std::string name = LL_XY_VECTOR_TAG;
	node->getAttributeString("name", name);

	LLRect rect;
	createRect(node, rect, parent,
			   LLRect(0, XY_VECTOR_HEIGHT, XY_VECTOR_WIDTH, 0));

	LLUICtrlCallback callback = NULL;
	LLXYVector* xy_vector = new LLXYVector(name, rect, callback, NULL);

	F32 min_val_x = -1.f;
	node->getAttributeF32("min_val_x", min_val_x);
	xy_vector->mMinValueX = min_val_x;

	F32 max_val_x = 1.f;
	node->getAttributeF32("max_val_x", max_val_x);
	xy_vector->mMaxValueX = max_val_x;
	if (max_val_x == 0.f)
	{
		llerrs << "Zero max X value for: " << name << llendl;
	}

	F32 min_val_y = -1.f;
	node->getAttributeF32("min_val_y", min_val_y);
	xy_vector->mMinValueY = min_val_y;

	F32 max_val_y = 1.f;
	node->getAttributeF32("max_val_y", max_val_y);
	xy_vector->mMaxValueY = max_val_y;
	if (max_val_y == 0.f)
	{
		llerrs << "Zero max Y value for: " << name << llendl;
	}

	F32 increment_x = 0.05f;
	node->getAttributeF32("increment_x", increment_x);
	xy_vector->mIncrementX = increment_x;

	F32 increment_y = 0.05f;
	node->getAttributeF32("increment_y", increment_y);
	xy_vector->mIncrementY = increment_y;

	bool logarithmic = false;
	node->getAttributeBool("logarithmic", logarithmic);
	xy_vector->mLogarithmic = logarithmic;

	xy_vector->initFromXML(node, parent);
	xy_vector->postBuild();

	return xy_vector;
}
