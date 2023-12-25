/**
 * @file llcolorswatch.cpp
 * @brief LLColorSwatch class implementation
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

#include "llcolorswatch.h"

#include "llbutton.h"
#include "llrender.h"
#include "lltextbox.h"
#include "llviewborder.h"
#include "llwindow.h"

#include "llagent.h"
#include "llfloatercolorpicker.h"
#include "llviewertexturelist.h"

static const std::string LL_COLOR_SWATCH_CTRL_TAG = "color_swatch";
static LLRegisterWidget<LLColorSwatchCtrl> r(LL_COLOR_SWATCH_CTRL_TAG);

LLColorSwatchCtrl::LLColorSwatchCtrl(const std::string& name,
									 const LLRect& rect,
									 const LLColor4& color,
									 void (*commit_callback)(LLUICtrl* ctrl,
															 void* userdata),
									 void* userdata)
:	LLUICtrl(name, rect, true, commit_callback, userdata,
			 FOLLOWS_LEFT | FOLLOWS_TOP),
	mValid(true),
	mColor(color),
	mBorderColor(LLUI::sDefaultHighlightLight),
	mCanApplyImmediately(false),
	mOnCancelCallback(NULL),
	mOnSelectCallback(NULL)
{
	mCaption = new LLTextBox(name,LLRect(0, gBtnHeightSmall,
										 getRect().getWidth(), 0),
							 name, LLFontGL::getFontSansSerifSmall());
	mCaption->setFollows(FOLLOWS_LEFT | FOLLOWS_RIGHT | FOLLOWS_BOTTOM);
	addChild(mCaption);

	// Scalable UI made this off-by-one, I do not know why. JC
	LLRect border_rect(0, getRect().getHeight() - 1,
					   getRect().getWidth() - 1, 0);
	border_rect.mBottom += gBtnHeightSmall;
	mBorder = new LLViewBorder(std::string("border"), border_rect,
							   LLViewBorder::BEVEL_IN);
	addChild(mBorder);

	mAlphaGradientImage = LLUI::getUIImage("color_swatch_alpha.tga");
}

LLColorSwatchCtrl::LLColorSwatchCtrl(const std::string& name,
									 const LLRect& rect,
									 const std::string& label,
									 const LLColor4& color,
									 void (*commit_callback)(LLUICtrl* ctrl,
															 void* userdata),
									 void* userdata)
:	LLUICtrl(name, rect, true, commit_callback, userdata,
			 FOLLOWS_LEFT | FOLLOWS_TOP),
	mValid(true),
	mColor(color),
	mBorderColor(LLUI::sDefaultHighlightLight),
	mCanApplyImmediately(false),
	mOnCancelCallback(NULL),
	mOnSelectCallback(NULL)
{
	mCaption = new LLTextBox(label, LLRect(0, gBtnHeightSmall,
										   getRect().getWidth(), 0),
							 label, LLFontGL::getFontSansSerifSmall());
	mCaption->setFollows(FOLLOWS_LEFT | FOLLOWS_RIGHT | FOLLOWS_BOTTOM);
	addChild(mCaption);

	// Scalable UI made this off-by-one, I do not know why. JC
	LLRect border_rect(0, getRect().getHeight() - 1,
					   getRect().getWidth() - 1, 0);
	border_rect.mBottom += gBtnHeightSmall;
	mBorder = new LLViewBorder(std::string("border"), border_rect,
							   LLViewBorder::BEVEL_IN);
	addChild(mBorder);

	mAlphaGradientImage = LLUI::getUIImage("color_swatch_alpha.tga");
}

//virtual
LLColorSwatchCtrl::~LLColorSwatchCtrl()
{
	// Parent dialog is destroyed so we are too and we need to cancel selection
	LLFloaterColorPicker* pickerp = (LLFloaterColorPicker*)mPickerHandle.get();
	if (pickerp)
	{
		pickerp->cancelSelection();
		pickerp->close();
	}
	mAlphaGradientImage = NULL;
}

//virtual
bool LLColorSwatchCtrl::handleDoubleClick(S32 x, S32 y, MASK mask)
{
	return handleMouseDown(x, y, mask);
}

//virtual
bool LLColorSwatchCtrl::handleHover(S32 x, S32 y, MASK mask)
{
	gWindowp->setCursor(UI_CURSOR_HAND);
	return true;
}

//virtual
bool LLColorSwatchCtrl::handleUnicodeCharHere(llwchar uni_char)
{
	if (uni_char == ' ')
	{
		showPicker(true);
	}
	return LLUICtrl::handleUnicodeCharHere(uni_char);
}

// Forces color of this swatch and any associated floater to the input value,
// if currently invalid
void LLColorSwatchCtrl::setOriginal(const LLColor4& color)
{
	mColor = color;
	LLFloaterColorPicker* pickerp = (LLFloaterColorPicker*)mPickerHandle.get();
	if (pickerp)
	{
		pickerp->setOrigRgb(mColor.mV[VRED], mColor.mV[VGREEN],
							mColor.mV[VBLUE]);
	}
}

void LLColorSwatchCtrl::set(const LLColor4& color, bool update_picker,
							bool from_event)
{
	mColor = color;
	LLFloaterColorPicker* pickerp = (LLFloaterColorPicker*)mPickerHandle.get();
	if (pickerp && update_picker)
	{
		pickerp->setCurRgb(mColor.mV[VRED], mColor.mV[VGREEN],
						   mColor.mV[VBLUE]);
	}
	if (!from_event)
	{
		setControlValue(mColor.getValue());
	}
}

void LLColorSwatchCtrl::setLabel(const std::string& label)
{
	mCaption->setText(label);
}

void LLColorSwatchCtrl::setFallbackImageName(const std::string& image_name)
{
	mFallbackImage =
		LLViewerTextureManager::getFetchedTextureFromFile(image_name,
														  MIPMAP_YES,
														  LLGLTexture::BOOST_PREVIEW,
														  LLViewerTexture::LOD_TEXTURE);
}

//virtual
bool LLColorSwatchCtrl::handleMouseDown(S32 x, S32 y, MASK mask)
{
	// Route future Mouse messages here preemptively (release on mouse up).
	// No handler is needed for capture lost since this object has no state
	// that depends on it.
	gFocusMgr.setMouseCapture(this);

	return true;
}

//virtual
bool LLColorSwatchCtrl::handleMouseUp(S32 x, S32 y, MASK mask)
{
	// We only handle the click if the click both started and ended within us
	if (hasMouseCapture())
	{
		// Release the mouse
		gFocusMgr.setMouseCapture(NULL);

		// If mouseup in the widget, it has been clicked
		if (pointInView(x, y))
		{
			llassert(getEnabled());
			llassert(getVisible());

			showPicker(false);
		}
	}

	return true;
}

// Assumes GL state is set for 2D
//virtual
void LLColorSwatchCtrl::draw()
{
	mBorder->setKeyboardFocusHighlight(hasFocus());
	// Draw border
	LLRect border(0, getRect().getHeight(), getRect().getWidth(),
				  gBtnHeightSmall);
	gl_rect_2d(border, mBorderColor, false);

	LLRect interior = border;
	interior.stretch(-1);

	// Check state
	if (mValid || gAgent.isGodlikeWithoutAdminMenuFakery())
	{
		// Draw the color swatch
		gl_rect_2d_checkerboard(interior);
		gl_rect_2d(interior, mColor, true);
		LLColor4 opaque_color = mColor;
		opaque_color.mV[VALPHA] = 1.f;
		gGL.color4fv(opaque_color.mV);
		if (mAlphaGradientImage.notNull())
		{
			gGL.pushMatrix();
			{
				mAlphaGradientImage->draw(interior, mColor);
			}
			gGL.popMatrix();
		}
	}
	else if (mFallbackImage.notNull())
	{
		if (mFallbackImage->getComponents() == 4)
		{
			gl_rect_2d_checkerboard(interior);
		}
		gl_draw_scaled_image(interior.mLeft, interior.mBottom,
							 interior.getWidth(), interior.getHeight(),
							 mFallbackImage);
		mFallbackImage->addTextureStats((F32)(interior.getWidth() *
											  interior.getHeight()));
	}
	else
	{
		// Draw grey and an X
		gl_rect_2d(interior, LLColor4::grey, true);
		gl_draw_x(interior, LLColor4::black);
	}

	LLUICtrl::draw();
}

//virtual
void LLColorSwatchCtrl::setEnabled(bool enabled)
{
	mCaption->setEnabled(enabled);
	LLView::setEnabled(enabled);

	if (!enabled)
	{
		LLFloaterColorPicker* pickerp = (LLFloaterColorPicker*)mPickerHandle.get();
		if (pickerp)
		{
			pickerp->cancelSelection();
			pickerp->close();
		}
	}
}

//virtual
void LLColorSwatchCtrl::setValue(const LLSD& value)
{
	set(LLColor4(value), true, true);
}

// Called (infrequently) when the color changes so the subject of the swatch
// can be updated.
void LLColorSwatchCtrl::onColorChanged(void* data, EColorPickOp pick_op)
{
	LLColorSwatchCtrl* subject = (LLColorSwatchCtrl*)data;
	if (!subject)
	{
		return;
	}

	LLFloaterColorPicker* pickerp =
		(LLFloaterColorPicker*)subject->mPickerHandle.get();
	if (!pickerp)
	{
		return;
	}

	// Move color across from selector to internal widget storage, keeping
	// current alpha.
	LLColor4 new_color(pickerp->getCurR(), pickerp->getCurG(),
					   pickerp->getCurB(), subject->mColor.mV[VALPHA]);
	subject->mColor = new_color;
	subject->setControlValue(new_color.getValue());

	if (pick_op == COLOR_CANCEL && subject->mOnCancelCallback)
	{
		subject->mOnCancelCallback(subject, subject->mCallbackUserData);
	}
	else if (pick_op == COLOR_SELECT && subject->mOnSelectCallback)
	{
		subject->mOnSelectCallback(subject, subject->mCallbackUserData);
	}
	else
	{
		// Just commit the change
		subject->onCommit();
	}
}

void LLColorSwatchCtrl::setValid(bool valid)
{
	mValid = valid;

	LLFloaterColorPicker* pickerp = (LLFloaterColorPicker*)mPickerHandle.get();
	if (pickerp)
	{
		pickerp->setActive(valid);
	}
}

void LLColorSwatchCtrl::showPicker(bool take_focus)
{
	LLFloaterColorPicker* pickerp = (LLFloaterColorPicker*)mPickerHandle.get();
	if (!pickerp)
	{
		pickerp = new LLFloaterColorPicker(this, mCanApplyImmediately);
		if (gFloaterViewp)
		{
			LLFloater* parentp = gFloaterViewp->getParentFloater(this);
			if (parentp)
			{
				parentp->addDependentFloater(pickerp);
			}
		}
		mPickerHandle = pickerp->getHandle();
	}

	// Initialize picker with current color
	pickerp->initUI(mColor.mV[VRED], mColor.mV[VGREEN], mColor.mV[VBLUE]);

	// Display it
	pickerp->showUI();

	if (take_focus)
	{
		pickerp->setFocus(true);
	}
}

//virtual
LLXMLNodePtr LLColorSwatchCtrl::getXML(bool save_children) const
{
	LLXMLNodePtr node = LLUICtrl::getXML();

	node->setName(LL_COLOR_SWATCH_CTRL_TAG);

	node->createChild("initial_color", true)->setFloatValue(4, mColor.mV);

	node->createChild("border_color", true)->setFloatValue(4, mBorderColor.mV);

	if (mCaption)
	{
		node->createChild("label", true)->setStringValue(mCaption->getText());
	}

	node->createChild("can_apply_immediately", true)->setBoolValue(mCanApplyImmediately);

	return node;
}

//static
LLView* LLColorSwatchCtrl::fromXML(LLXMLNodePtr node, LLView* parent,
								   LLUICtrlFactory* factory)
{
	std::string name = "colorswatch";
	node->getAttributeString("name", name);

	std::string label;
	node->getAttributeString("label", label);

	LLColor4 color = LLColor4::white;
	node->getAttributeColor("initial_color", color);

	LLRect rect;
	createRect(node, rect, parent, LLRect());

	bool can_apply_immediately = false;
	node->getAttributeBool("can_apply_immediately", can_apply_immediately);

	LLUICtrlCallback callback = NULL;

	if (label.empty())
	{
		label.assign(node->getValue());
	}

	LLColorSwatchCtrl* self = new LLColorSwatchCtrl(name, rect, label, color,
													callback, NULL);

	self->setCanApplyImmediately(can_apply_immediately);
	self->initFromXML(node, parent);

	return self;
}
