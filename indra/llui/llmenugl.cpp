/**
 * @file llmenugl.cpp
 * @brief LLMenuItemGL base class
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

//*****************************************************************************
//
// This file contains the opengl based menu implementation.
//
// NOTES: A menu label is split into 4 columns. The left column, the
// label colum, the accelerator column, and the right column. The left
// column is used for displaying boolean values for toggle and check
// controls. The right column is used for submenus.
//
//*****************************************************************************

#include "linden_common.h"

#include "boost/tokenizer.hpp"

#include "llmenugl.h"

#include "llcriticaldamp.h"
#include "llrender.h"
#include "llstl.h"					// For DeletePointer()
#include "lltrans.h"
#include "lluictrlfactory.h"
#include "llwindow.h"
#include "llvector2.h"

using namespace LLOldEvents;

const char* LL_PIE_MENU_TAG = "pie_menu";
static const std::string LL_MENU_ITEM_TAG = "menu_item";
static const std::string LL_MENU_GL_TAG = "menu";
static const std::string LL_MENU_BAR_GL_TAG = "menu_bar";
static const std::string LL_MENU_ITEM_CALL_GL_TAG = "menu_item_call";
static const std::string LL_MENU_ITEM_CHECK_GL_TAG = "menu_item_check";
static const std::string LL_MENU_ITEM_SEPARATOR_GL_TAG = "menu_item_separator";
static const std::string LL_MENU_ITEM_TEAR_OFF_GL_TAG = "tearoff_menu";

//static
LLMenuHolderGL* LLMenuGL::sMenuContainer = NULL;

//============================================================================
// Local function declarations, constants, enums, and typedefs
//============================================================================

const std::string SEPARATOR_NAME("separator");
const std::string TEAROFF_SEPARATOR_LABEL("~~~~~~~~~~~");
const std::string SEPARATOR_LABEL("-----------");
const std::string VERTICAL_SEPARATOR_LABEL("|");

constexpr S32 LABEL_BOTTOM_PAD_PIXELS = 2;

constexpr U32 LEFT_PAD_PIXELS = 3;
constexpr U32 LEFT_WIDTH_PIXELS = 15;
constexpr U32 LEFT_PLAIN_PIXELS = LEFT_PAD_PIXELS + LEFT_WIDTH_PIXELS;

constexpr U32 RIGHT_PAD_PIXELS = 2;
constexpr U32 RIGHT_WIDTH_PIXELS = 15;
constexpr U32 RIGHT_PLAIN_PIXELS = RIGHT_PAD_PIXELS + RIGHT_WIDTH_PIXELS;

constexpr U32 ACCEL_PAD_PIXELS = 10;
constexpr U32 PLAIN_PAD_PIXELS = LEFT_PAD_PIXELS + LEFT_WIDTH_PIXELS +
								 RIGHT_PAD_PIXELS + RIGHT_WIDTH_PIXELS;

constexpr U32 BRIEF_PAD_PIXELS = 2;

constexpr U32 SEPARATOR_HEIGHT_PIXELS = 8;
constexpr S32 TEAROFF_SEPARATOR_HEIGHT_PIXELS = 10;
constexpr S32 MENU_ITEM_PADDING = 4;

static const std::string BOOLEAN_TRUE_PREFIX("X");
static const std::string BRANCH_SUFFIX(">");
static const std::string ARROW_UP  ("^^^^^^^");
static const std::string ARROW_DOWN("vvvvvvv");

constexpr F32 MAX_MOUSE_SLOPE_SUB_MENU = 0.9f;

LLColor4 LLMenuItemGL::sEnabledColor(0.0f, 0.0f, 0.0f, 1.0f);
LLColor4 LLMenuItemGL::sDisabledColor(0.5f, 0.5f, 0.5f, 1.0f);
LLColor4 LLMenuItemGL::sHighlightBackground(0.0f, 0.0f, 0.7f, 1.0f);
LLColor4 LLMenuItemGL::sHighlightForeground(1.0f, 1.0f, 1.0f, 1.0f);

LLColor4 LLMenuGL::sDefaultBackgroundColor(0.25f, 0.25f, 0.25f, 0.75f);
bool LLMenuGL::sKeyboardMode = false;

LLHandle<LLView> LLMenuHolderGL::sItemLastSelectedHandle;
LLFrameTimer LLMenuHolderGL::sItemActivationTimer;
//LLColor4 LLMenuGL::sBackgroundColor(0.8f, 0.8f, 0.0f, 1.0f);

constexpr S32 PIE_CENTER_SIZE = 20;	// pixels, radius of center hole
// Scale factor for pie menu when mouse is initially down
constexpr F32 PIE_SCALE_FACTOR = 1.7f;
// Time of transition between unbounded and bounded display of pie menu
constexpr F32 PIE_SHRINK_TIME = 0.2f;

constexpr F32 ACTIVATE_HIGHLIGHT_TIME = 0.3f;

//============================================================================
// Class LLMenuItemGL
//============================================================================

// Default constructor
LLMenuItemGL::LLMenuItemGL(const std::string& name, const std::string& label,
						   KEY key, MASK mask)
:	LLView(name, true),
	mJumpKey(KEY_NONE),
	mAcceleratorKey(key),
	mAcceleratorMask(mask),
	mAllowKeyRepeat(false),
	mHighlight(false),
	mGotHover(false),
	mBriefItem(false),
	mFont(LLFontGL::getFontSansSerif()),
	mStyle(LLFontGL::NORMAL),
	mDrawTextDisabled(false)
{
	setLabel(label);
}

//virtual
LLXMLNodePtr LLMenuItemGL::getXML(bool save_children) const
{
	LLXMLNodePtr node = LLView::getXML();

	node->setName(LL_MENU_ITEM_TAG);

	node->createChild("type", true)->setStringValue(getType());

	node->createChild("label", true)->setStringValue(mLabel);

	if (mAcceleratorKey != KEY_NONE)
	{
		std::stringstream out;
		if (mAcceleratorMask & MASK_CONTROL)
		{
			out << "control|";
		}
		if (mAcceleratorMask & MASK_ALT)
		{
			out << "alt|";
		}
		if (mAcceleratorMask & MASK_SHIFT)
		{
			out << "shift|";
		}
		out << LLKeyboard::stringFromKey(mAcceleratorKey);

		node->createChild("shortcut", true)->setStringValue(out.str());

#ifdef LL_DARWIN
		// Write in special tag if this key is really a ctrl combination on the
		// Mac
		if (mAcceleratorMask & MASK_MAC_CONTROL)
		{
			node->createChild("useMacCtrl", true)->setBoolValue(true);
		}
#endif // LL_DARWIN
	}

	return node;
}

bool LLMenuItemGL::handleAcceleratorKey(KEY key, MASK mask)
{
	if (getEnabled() && gKeyboardp &&
		(!gKeyboardp->getKeyRepeated(key) || mAllowKeyRepeat) &&
		key == mAcceleratorKey && mask == (mAcceleratorMask & MASK_NORMALKEYS))
	{
		doIt();
		return true;
	}
	return false;
}

bool LLMenuItemGL::handleHover(S32 x, S32 y, MASK mask)
{
	setHover(true);
	gWindowp->setCursor(UI_CURSOR_ARROW);
	return true;
}

// This function checks to see if the accelerator key is already in use;
// if not, it will be added to the list
bool LLMenuItemGL::addToAcceleratorList(std::list <LLKeyBinding*>* listp)
{
	if (mAcceleratorKey != KEY_NONE)
	{
		LLKeyBinding* accelerator = NULL;
		std::list<LLKeyBinding*>::iterator list_it;
		for (list_it = listp->begin(); list_it != listp->end(); ++list_it)
		{
			accelerator = *list_it;
			if (accelerator->mKey == mAcceleratorKey &&
				accelerator->mMask == (mAcceleratorMask & MASK_NORMALKEYS))
			{

				// *NOTE: get calling code to throw up warning or route
				// warning messages back to app-provided output
				//	std::string warning;
				//	warning.append("Duplicate key binding <");
				//	appendAcceleratorString(warning);
				//	warning.append("> for menu items:\n    ");
				//	warning.append(accelerator->mName);
				//	warning.append("\n    ");
				//	warning.append(mLabel);
				//	LLAlertDialog::modalAlert(warning);
				return false;
			}
		}
		if (!accelerator)
		{
			accelerator = new LLKeyBinding;
			if (accelerator)
			{
				accelerator->mKey = mAcceleratorKey;
				accelerator->mMask = (mAcceleratorMask & MASK_NORMALKEYS);
#if 0
				accelerator->mName = mLabel;
#endif
			}
			listp->push_back(accelerator);
		}
	}
	return true;
}

// This method appends the character string representation of the current
// accelerator key and mask to the provided string.
void LLMenuItemGL::appendAcceleratorString(std::string& st) const
{
	// break early if this is a silly thing to do.
	if (KEY_NONE == mAcceleratorKey)
	{
		return;
	}

	// Append any masks
#ifdef LL_DARWIN
	// Standard Mac names for modifier keys in menu equivalents
	// We could use the symbol characters, but they only exist in certain fonts.
	if (mAcceleratorMask & MASK_CONTROL)
	{
		if (mAcceleratorMask & MASK_MAC_CONTROL)
		{
			static std::string symbol = LLTrans::getUIString("accel-mac-control");
			st.append(symbol);
		}
		else
		{
			// Symbol would be "\xE2\x8C\x98"
			static std::string symbol = LLTrans::getUIString("accel-mac-command");
			st.append(symbol);
		}
	}
	if (mAcceleratorMask & MASK_ALT)
	{
		// Symbol would be "\xE2\x8C\xA5"
		static std::string symbol = LLTrans::getUIString("accel-mac-option");
		st.append(symbol);
	}
	if (mAcceleratorMask & MASK_SHIFT)
	{
		// Symbol would be "\xE2\x8C\xA7"
		static std::string symbol = LLTrans::getUIString("accel-mac-shift");
		st.append(symbol);
	}
#else
	if (mAcceleratorMask & MASK_CONTROL)
	{
		static std::string symbol = LLTrans::getUIString("accel-control");
		st.append(symbol);
	}
	if (mAcceleratorMask & MASK_ALT)
	{
		static std::string symbol = LLTrans::getUIString("accel-alt");
		st.append(symbol);
	}
	if (mAcceleratorMask & MASK_SHIFT)
	{
		static std::string symbol = LLTrans::getUIString("accel-shift");
		st.append(symbol);
	}
#endif

	std::string keystr = LLKeyboard::stringFromKey(mAcceleratorKey);
	if ((mAcceleratorMask & MASK_NORMALKEYS) &&
		(keystr[0] == '-' || keystr[0] == '='))
	{
		st.append(" ");
	}
	st.append(keystr);
}

void LLMenuItemGL::setJumpKey(KEY key)
{
	mJumpKey = LLStringOps::toUpper((char)key);
}

//virtual
U32 LLMenuItemGL::getNominalHeight() const
{
	return ll_roundp(mFont->getLineHeight()) + MENU_ITEM_PADDING;
}

// Get the parent menu for this item
LLMenuGL* LLMenuItemGL::getMenu()
{
	return (LLMenuGL*)getParent();
}

// getNominalWidth() - returns the normal width of this control in pixels:
// this is used for calculating the widest item, as well as for horizontal
// arrangement.
U32 LLMenuItemGL::getNominalWidth() const
{
	U32 width;

	if (mBriefItem)
	{
		width = BRIEF_PAD_PIXELS;
	}
	else
	{
		width = PLAIN_PAD_PIXELS;
	}

	if (KEY_NONE != mAcceleratorKey)
	{
		width += ACCEL_PAD_PIXELS;
		std::string temp;
		appendAcceleratorString(temp);
		width += mFont->getWidth(temp);
	}
	width += mFont->getWidth(mLabel.getWString().c_str());
	return width;
}

// called to rebuild the draw label
void LLMenuItemGL::buildDrawLabel()
{
	mDrawAccelLabel.clear();
	std::string st = mDrawAccelLabel.getString();
	appendAcceleratorString(st);
	mDrawAccelLabel = st;
}

void LLMenuItemGL::doIt()
{
	// Close all open menus by default if parent menu is actually visible (and
	// we are not triggering menu item via accelerator)
	LLMenuGL* menup = getMenu();
	if (!menup)
	{
		llwarns << "NULL menu. Aborted." << llendl;
		return;
	}
	if (!menup->getTornOff() && menup->getVisible() &&
		LLMenuGL::sMenuContainer)
	{
		LLMenuGL::sMenuContainer->hideMenus();
	}
}

// Set the hover status (called by its menu)
void LLMenuItemGL::setHighlight(bool highlight)
{
	LLMenuGL* menup = getMenu();
	if (highlight && menup)
	{
		menup->clearHoverItem();
	}
	mHighlight = highlight;
}

bool LLMenuItemGL::handleKeyHere(KEY key, MASK mask)
{
	LLMenuGL* menup = getMenu();
	if (!menup)
	{
		llwarns << "NULL menu. Aborted." << llendl;
		return false;
	}

	if (getHighlight() && menup->isOpen())
	{
		if (key == KEY_UP)
		{
			// Switch to keyboard navigation mode
			LLMenuGL::setKeyboardMode(true);

			menup->highlightPrevItem(this);
			return true;
		}
		if (key == KEY_DOWN)
		{
			// Switch to keyboard navigation mode
			LLMenuGL::setKeyboardMode(true);

			menup->highlightNextItem(this);
			return true;
		}
		if (key == KEY_RETURN && mask == MASK_NONE)
		{
			// switch to keyboard navigation mode
			LLMenuGL::setKeyboardMode(true);

			doIt();
			return true;
		}
	}

	return false;
}

bool LLMenuItemGL::handleMouseUp(S32 x, S32 y, MASK)
{
	// Switch to mouse navigation mode
	LLMenuGL::setKeyboardMode(false);

	doIt();
	make_ui_sound("UISndClickRelease");
	return true;
}

bool LLMenuItemGL::handleMouseDown(S32 x, S32 y, MASK)
{
	// Switch to mouse navigation mode
	LLMenuGL::setKeyboardMode(false);

	setHighlight(true);
	return true;
}

void LLMenuItemGL::draw()
{
	// *HACK: Brief items do not highlight. Pie menu takes care of it. JC
	// Let disabled items be highlighted, just don't draw them as such.
	if (getEnabled() && getHighlight() && !mBriefItem)
	{
		gGL.color4fv(sHighlightBackground.mV);
		gl_rect_2d(0, getRect().getHeight(), getRect().getWidth(), 0);
	}

	LLColor4 color;

	U8 font_style = mStyle;
	if (getEnabled() && !mDrawTextDisabled)
	{
		font_style |= LLFontGL::DROP_SHADOW_SOFT;
	}

	if (getEnabled() && getHighlight())
	{
		color = sHighlightForeground;
	}
	else if (getEnabled() && !mDrawTextDisabled)
	{
		color = sEnabledColor;
	}
	else
	{
		color = sDisabledColor;
	}

	// Draw the text on top.
	if (mBriefItem)
	{
		mFont->render(mLabel, 0, BRIEF_PAD_PIXELS / 2, 0, color,
					  LLFontGL::LEFT, LLFontGL::BOTTOM, font_style);
	}
	else
	{
		if (!mDrawBoolLabel.empty())
		{
			mFont->render(mDrawBoolLabel.getWString(), 0, (F32)LEFT_PAD_PIXELS,
						  (F32)MENU_ITEM_PADDING * 0.5f + 1.f, color,
						  LLFontGL::LEFT, LLFontGL::BOTTOM, font_style,
						  S32_MAX, S32_MAX, NULL, false);
		}
		mFont->render(mLabel.getWString(), 0, (F32)LEFT_PLAIN_PIXELS,
					  (F32)MENU_ITEM_PADDING * 0.5f + 1.f, color,
					  LLFontGL::LEFT, LLFontGL::BOTTOM, font_style,
					  S32_MAX, S32_MAX, NULL, false);
		if (!mDrawAccelLabel.empty())
		{
			mFont->render(mDrawAccelLabel.getWString(), 0,
						  (F32)getRect().mRight - (F32)RIGHT_PLAIN_PIXELS,
						  (F32)MENU_ITEM_PADDING * 0.5f + 1.f, color,
						  LLFontGL::RIGHT, LLFontGL::BOTTOM, font_style,
						  S32_MAX, S32_MAX, NULL, false);
		}
		if (!mDrawBranchLabel.empty())
		{
			mFont->render(mDrawBranchLabel.getWString(), 0,
						  (F32)getRect().mRight - (F32)RIGHT_PAD_PIXELS,
						  (F32)MENU_ITEM_PADDING * 0.5f + 1.f, color,
						  LLFontGL::RIGHT, LLFontGL::BOTTOM, font_style,
						  S32_MAX, S32_MAX, NULL, false);
		}
	}

	// Underline "jump" key only when keyboard navigation has been initiated
	LLMenuGL* menup = getMenu();
	if (menup && menup->jumpKeysActive() && LLMenuGL::getKeyboardMode())
	{
		std::string upper_case_label = mLabel.getString();
		LLStringUtil::toUpper(upper_case_label);
		std::string::size_type offset = upper_case_label.find(mJumpKey);
		if (offset != std::string::npos)
		{
			S32 x_begin = LEFT_PLAIN_PIXELS + mFont->getWidth(mLabel, 0,
															  offset);
			S32 x_end = LEFT_PLAIN_PIXELS + mFont->getWidth(mLabel, 0,
															offset + 1);
			gl_line_2d(x_begin, MENU_ITEM_PADDING / 2 + 1,
					   x_end, MENU_ITEM_PADDING / 2 + 1);
		}
	}

	// Clear got hover every frame
	setHover(false);
}

bool LLMenuItemGL::setLabelArg(const std::string& key, const std::string& text)
{
	mLabel.setArg(key, text);
	return true;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLMenuItemSeparatorGL
//
// This class represents a separator.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLMenuItemSeparatorGL : public LLMenuItemGL
{
public:
	LLMenuItemSeparatorGL(const std::string& name = SEPARATOR_NAME);

	LLXMLNodePtr getXML(bool save_children = true) const override;

	LL_INLINE std::string getType() const override	{ return "separator"; }

	// Does the primary funcationality of the menu item.
	LL_INLINE void doIt() override					{}

	void draw() override;
	bool handleMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleMouseUp(S32 x, S32 y, MASK mask) override;
	bool handleHover(S32 x, S32 y, MASK mask) override;

	LL_INLINE U32 getNominalHeight() const override	{ return SEPARATOR_HEIGHT_PIXELS; }
};

LLMenuItemSeparatorGL::LLMenuItemSeparatorGL(const std::string& name)
:	LLMenuItemGL(name, SEPARATOR_LABEL)
{
}

LLXMLNodePtr LLMenuItemSeparatorGL::getXML(bool save_children) const
{
	LLXMLNodePtr node = LLMenuItemGL::getXML();
	node->setName(LL_MENU_ITEM_SEPARATOR_GL_TAG);
	return node;
}

void LLMenuItemSeparatorGL::draw()
{
	gGL.color4fv(getDisabledColor().mV);
	const S32 y = getRect().getHeight() / 2;
	constexpr S32 PAD = 6;
	gl_line_2d(PAD, y, getRect().getWidth() - PAD, y);
}

bool LLMenuItemSeparatorGL::handleMouseDown(S32 x, S32 y, MASK mask)
{
	LLMenuGL* menup = getMenu();
	if (!menup)
	{
		llwarns << "NULL menu. Aborted." << llendl;
		return false;
	}
	if (y > getRect().getHeight() / 2)
	{
		return menup->handleMouseDown(x + getRect().mLeft,
									  getRect().mTop + 1, mask);
	}
	return menup->handleMouseDown(x + getRect().mLeft,
								  getRect().mBottom - 1, mask);
}

bool LLMenuItemSeparatorGL::handleMouseUp(S32 x, S32 y, MASK mask)
{
	LLMenuGL* menup = getMenu();
	if (!menup)
	{
		llwarns << "NULL menu. Aborted." << llendl;
		return false;
	}
	if (y > getRect().getHeight() / 2)
	{
		return menup->handleMouseUp(x + getRect().mLeft,
									getRect().mTop + 1, mask);
	}
	return menup->handleMouseUp(x + getRect().mLeft,
								getRect().mBottom - 1, mask);
}

bool LLMenuItemSeparatorGL::handleHover(S32 x, S32 y, MASK mask)
{
	LLMenuGL* menup = getMenu();
	if (menup)
	{
		if (y > getRect().getHeight() / 2)
		{
			menup->highlightPrevItem(this, false);
		}
		else
		{
			menup->highlightNextItem(this, false);
		}
	}
	return false;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLMenuItemVerticalSeparatorGL
//
// This class represents a vertical separator.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLMenuItemVerticalSeparatorGL : public LLMenuItemSeparatorGL
{
public:
	LLMenuItemVerticalSeparatorGL();

	virtual bool handleMouseDown(S32 x, S32 y, MASK mask) { return false; }
};

LLMenuItemVerticalSeparatorGL::LLMenuItemVerticalSeparatorGL()
{
	setLabel(VERTICAL_SEPARATOR_LABEL);
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLMenuItemTearOffGL
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

LLMenuItemTearOffGL::LLMenuItemTearOffGL(LLHandle<LLFloater> parent_floater_handle)
:	LLMenuItemGL("tear off", TEAROFF_SEPARATOR_LABEL),
	mParentHandle(parent_floater_handle)
{
}

LLXMLNodePtr LLMenuItemTearOffGL::getXML(bool save_children) const
{
	LLXMLNodePtr node = LLMenuItemGL::getXML();
	node->setName(LL_MENU_ITEM_TEAR_OFF_GL_TAG);
	return node;
}

void LLMenuItemTearOffGL::doIt()
{
	LLMenuGL* menup = getMenu();
	if (!menup)
	{
		llwarns << "NULL menu. Aborted." << llendl;
		return;
	}

	if (menup->getTornOff())
	{
		LLTearOffMenu* torn_off_menu =
			dynamic_cast<LLTearOffMenu*>(menup->getParent());
		if (torn_off_menu)
		{
			torn_off_menu->close();
		}
	}
	else
	{
		// Transfer keyboard focus and highlight to first real item in list
		if (getHighlight())
		{
			menup->highlightNextItem(this);
		}

		menup->arrange();

		LLFloater* parent_floater = mParentHandle.get();
		LLFloater* tear_off_menu = LLTearOffMenu::create(menup);
		if (tear_off_menu)
		{
			if (parent_floater)
			{
				parent_floater->addDependentFloater(tear_off_menu, false);
			}

			// Give focus to torn off menu because it will have been taken
			// away when parent menu closes
			tear_off_menu->setFocus(true);
		}
	}
	LLMenuItemGL::doIt();
}

void LLMenuItemTearOffGL::draw()
{
	// Disabled items can be highlighted, but shouldn't render as such
	if (getEnabled() && getHighlight() && !isBriefItem())
	{
		gGL.color4fv(getHighlightBGColor().mV);
		gl_rect_2d(0, getRect().getHeight(), getRect().getWidth(), 0);
	}

	if (getEnabled())
	{
		gGL.color4fv(getEnabledColor().mV);
	}
	else
	{
		gGL.color4fv(getDisabledColor().mV);
	}
	const S32 y = getRect().getHeight() / 3;
	constexpr S32 PAD = 6;
	gl_line_2d(PAD, y, getRect().getWidth() - PAD, y);
	gl_line_2d(PAD, y * 2, getRect().getWidth() - PAD, y * 2);
}

U32 LLMenuItemTearOffGL::getNominalHeight() const
{
	return TEAROFF_SEPARATOR_HEIGHT_PIXELS;
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLMenuItemBlankGL
//
// This class represents a blank, non-functioning item.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLMenuItemBlankGL : public LLMenuItemGL
{
public:
	LLMenuItemBlankGL()
	:	LLMenuItemGL(LLStringUtil::null, LLStringUtil::null)
	{
		setEnabled(false);
	}

	LL_INLINE virtual void doIt() {}
	LL_INLINE virtual void draw() {}
};

//============================================================================
// Class LLMenuItemCallGL
//============================================================================

LLMenuItemCallGL::LLMenuItemCallGL(const std::string& name,
								   const std::string& label,
								   menu_callback clicked_cb,
								   enabled_callback enabled_cb,
								   void* user_data, KEY key, MASK mask,
								   bool enabled,
								   on_disabled_callback on_disabled_cb)
:	LLMenuItemGL(name, label, key, mask),
	mCallback(clicked_cb),
	mEnabledCallback(enabled_cb),
	mLabelCallback(NULL),
	mUserData(user_data),
	mOnDisabledCallback(on_disabled_cb)
{
	if (!enabled) setEnabled(false);
}

LLMenuItemCallGL::LLMenuItemCallGL(const std::string& name,
								   menu_callback clicked_cb,
								   enabled_callback enabled_cb,
								   void* user_data, KEY key, MASK mask,
								   bool enabled,
								   on_disabled_callback on_disabled_cb)
:	LLMenuItemGL(name, name, key, mask),
	mCallback(clicked_cb),
	mEnabledCallback(enabled_cb),
	mLabelCallback(NULL),
	mUserData(user_data),
	mOnDisabledCallback(on_disabled_cb)
{
	if (!enabled) setEnabled(false);
}

LLMenuItemCallGL::LLMenuItemCallGL(const std::string& name,
								   const std::string& label,
								   menu_callback clicked_cb,
								   enabled_callback enabled_cb,
								   label_callback label_cb,
								   void* user_data, KEY key, MASK mask,
								   bool enabled,
								   on_disabled_callback on_disabled_cb)
:	LLMenuItemGL(name, label, key, mask),
	mCallback(clicked_cb),
	mEnabledCallback(enabled_cb),
	mLabelCallback(label_cb),
	mUserData(user_data),
	mOnDisabledCallback(on_disabled_cb)
{
	if (!enabled) setEnabled(false);
}

LLMenuItemCallGL::LLMenuItemCallGL(const std::string& name,
								   menu_callback clicked_cb,
								   enabled_callback enabled_cb,
								   label_callback label_cb,
								   void* user_data, KEY key, MASK mask,
								   bool enabled,
								   on_disabled_callback on_disabled_cb)
:	LLMenuItemGL(name, name, key, mask),
	mCallback(clicked_cb),
	mEnabledCallback(enabled_cb),
	mLabelCallback(label_cb),
	mUserData(user_data),
	mOnDisabledCallback(on_disabled_cb)
{
	if (!enabled) setEnabled(false);
}

void LLMenuItemCallGL::setEnabledControl(const std::string& enabled_control,
										 LLView* context)
{
	// Register new listener
	if (!enabled_control.empty())
	{
		LLControlVariable* control = context->findControl(enabled_control);
		if (!control)
		{
			context->addBoolControl(enabled_control, getEnabled());
			control = context->findControl(enabled_control);
			llassert_always(control);
		}
		control->getSignal()->connect(boost::bind(&LLView::controlListener,
												  _2, getHandle(), "enabled"));
		setEnabled(control->getValue());
	}
}

void LLMenuItemCallGL::setVisibleControl(const std::string& visible_control,
										 LLView* context)
{
	// Register new listener
	if (!visible_control.empty())
	{
		LLControlVariable* control = context->findControl(visible_control);
		if (!control)
		{
			context->addBoolControl(visible_control, getVisible());
			control = context->findControl(visible_control);
			llassert_always(control);
		}
		control->getSignal()->connect(boost::bind(&LLView::controlListener,
												  _2, getHandle(), "visible"));
		setVisible(control->getValue());
	}
}

//virtual
LLXMLNodePtr LLMenuItemCallGL::getXML(bool save_children) const
{
	LLXMLNodePtr node = LLMenuItemGL::getXML();

	node->setName(LL_MENU_ITEM_CALL_GL_TAG);

	std::vector<LLListenerEntry> listeners = mDispatcher->getListeners();
	for (std::vector<LLListenerEntry>::iterator itor = listeners.begin(),
												end = listeners.end();
		 itor != end; ++itor)
	{
		std::string listener_name =
			findEventListener((LLSimpleListener*)itor->listener);
		if (!listener_name.empty())
		{
			// *FIXME: It is not always on_click. It could be on_check,
			// on_enable or on_visible, but there is no way to get that from
			// the data that is stored.
			LLXMLNodePtr child_node = node->createChild("on_click", false);
			child_node->createChild("function",
									true)->setStringValue(listener_name);
			child_node->createChild("filter",
									true)->setStringValue(itor->filter.asString());
			child_node->createChild("userdata",
									true)->setStringValue(itor->userdata.asString());
		}
	}

	return node;
}

// Calls the provided callback
void LLMenuItemCallGL::doIt()
{
	LLMenuGL* menup = getMenu();
	if (!menup)
	{
		llwarns << "NULL menu. Aborted." << llendl;
		return;
	}

	// RN: menu item can be deleted in callback, so beware
	menup->setItemLastSelected(this);

	if (mCallback)
	{
		mCallback(mUserData);
	}
	LLPointer<LLEvent> fired_event = new LLEvent(this);
	fireEvent(fired_event, "on_click");
	LLMenuItemGL::doIt();
}

void LLMenuItemCallGL::buildDrawLabel()
{
	LLPointer<LLEvent> fired_event = new LLEvent(this);
	fireEvent(fired_event, "on_build");
	if (mEnabledCallback)
	{
		setEnabled(mEnabledCallback(mUserData));
	}
	if (mLabelCallback)
	{
		std::string label;
		mLabelCallback(label, mUserData);
		mLabel = label;
	}
	LLMenuItemGL::buildDrawLabel();
}

bool LLMenuItemCallGL::handleAcceleratorKey(KEY key, MASK mask)
{
 	if (gKeyboardp &&
		(!gKeyboardp->getKeyRepeated(key) || getAllowKeyRepeat()) &&
		key == mAcceleratorKey && mask == (mAcceleratorMask & MASK_NORMALKEYS))
	{
		LLPointer<LLEvent> fired_event = new LLEvent(this);
		fireEvent(fired_event, "on_build");
		if (mEnabledCallback)
		{
			setEnabled(mEnabledCallback(mUserData));
		}
		if (!getEnabled())
		{
			if (mOnDisabledCallback)
			{
				mOnDisabledCallback(mUserData);
			}
		}
	}
	return LLMenuItemGL::handleAcceleratorKey(key, mask);
}

//============================================================================
// Class LLMenuItemCheckGL
//============================================================================

LLMenuItemCheckGL::LLMenuItemCheckGL(const std::string& name,
									 const std::string& label,
									 menu_callback clicked_cb,
									 enabled_callback enabled_cb,
									 check_callback check_cb,
									 void* user_data, KEY key, MASK mask)
:	LLMenuItemCallGL(name, label, clicked_cb, enabled_cb, user_data, key,
					 mask),
	mCheckCallback(check_cb),
	mChecked(false)
{
}

LLMenuItemCheckGL::LLMenuItemCheckGL(const std::string& name,
									 menu_callback clicked_cb,
									 enabled_callback enabled_cb,
									 check_callback check_cb,
									 void* user_data, KEY key, MASK mask)
:	LLMenuItemCallGL(name, name, clicked_cb, enabled_cb, user_data, key, mask),
	mCheckCallback(check_cb),
	mChecked(false)
{
}

LLMenuItemCheckGL::LLMenuItemCheckGL(const std::string& name,
									 const std::string& label,
									 menu_callback clicked_cb,
									 enabled_callback enabled_cb,
									 const char* control_name,
									 LLView* context, void* user_data,
									 KEY key, MASK mask)
:	LLMenuItemCallGL(name, label, clicked_cb, enabled_cb, user_data, key,
					 mask),
	mCheckCallback(NULL)
{
	setControlName(control_name, context);
}

//virtual
void LLMenuItemCheckGL::setValue(const LLSD& value)
{
	mChecked = value.asBoolean();
	if (mChecked)
	{
		mDrawBoolLabel = BOOLEAN_TRUE_PREFIX;
	}
	else
	{
		mDrawBoolLabel.clear();
	}
}

void LLMenuItemCheckGL::setCheckedControl(std::string checked_control,
										  LLView* context)
{
	// Register new listener
	if (!checked_control.empty())
	{
		LLControlVariable* control = context->findControl(checked_control);
		if (!control)
		{
			context->addBoolControl(checked_control, mChecked);
			control = context->findControl(checked_control);
			llassert_always(control);
		}
		control->getSignal()->connect(boost::bind(&LLView::controlListener,
												  _2, getHandle(), "value"));
		mChecked = control->getValue();
	}
}

//virtual
LLXMLNodePtr LLMenuItemCheckGL::getXML(bool save_children) const
{
	LLXMLNodePtr node = LLMenuItemCallGL::getXML();
	node->setName(LL_MENU_ITEM_CHECK_GL_TAG);
	return node;
}

// Called to rebuild the draw label
void LLMenuItemCheckGL::buildDrawLabel()
{
	if (mChecked || (mCheckCallback && mCheckCallback(getUserData())))
	{
		mDrawBoolLabel = BOOLEAN_TRUE_PREFIX;
	}
	else
	{
		mDrawBoolLabel.clear();
	}
	LLMenuItemCallGL::buildDrawLabel();
}

//============================================================================
// Class LLMenuItemToggleGL
//============================================================================

LLMenuItemToggleGL::LLMenuItemToggleGL(const std::string& name,
									   const std::string& label,
									   bool* toggle, KEY key, MASK mask)
:	LLMenuItemGL(name, label, key, mask),
	mToggle(toggle)
{
}

LLMenuItemToggleGL::LLMenuItemToggleGL(const std::string& name, bool* toggle,
									   KEY key, MASK mask)
:	LLMenuItemGL(name, name, key, mask),
	mToggle(toggle)
{
}

// Called to rebuild the draw label
void LLMenuItemToggleGL::buildDrawLabel()
{
	if (*mToggle)
	{
		mDrawBoolLabel = BOOLEAN_TRUE_PREFIX;
	}
	else
	{
		mDrawBoolLabel.clear();
	}
	mDrawAccelLabel.clear();
	std::string st = mDrawAccelLabel;
	appendAcceleratorString(st);
	mDrawAccelLabel = st;
}

// Does the primary funcationality of the menu item.
void LLMenuItemToggleGL::doIt()
{
	LLMenuGL* menup = getMenu();
	if (!menup)
	{
		llwarns << "NULL menu. Aborted." << llendl;
		return;
	}
	menup->setItemLastSelected(this);

	*mToggle = !(*mToggle);
	buildDrawLabel();
	LLMenuItemGL::doIt();
}

LLMenuItemBranchGL::LLMenuItemBranchGL(const std::string& name,
									   const std::string& label,
									   LLHandle<LLView> branch,
									   KEY key, MASK mask)
:	LLMenuItemGL(name, label, key, mask),
	mBranch(branch)
{
	if (!dynamic_cast<LLMenuGL*>(branch.get()))
	{
		llerrs << "Non-menu handle passed as branch reference." << llendl;
	}

	if (getBranch())
	{
		getBranch()->setVisible(false);
		getBranch()->setParentMenuItem(this);
	}
}

LLMenuItemBranchGL::~LLMenuItemBranchGL()
{
	deleteViewByHandle(mBranch);
}

//virtual
LLView* LLMenuItemBranchGL::getChildView(const char* name, bool recurse,
										 bool create_if_missing) const
{
	// richard: this is redundant with parent, remove
	if (getBranch())
	{
		if (getBranch()->getName() == name)
		{
			return getBranch();
		}

		// Always recurse on branches
		LLView* child = getBranch()->getChildView(name, recurse, false);
		if (child)
		{
			return child;
		}
	}
	return LLView::getChildView(name, recurse, create_if_missing);
}

//virtual
bool LLMenuItemBranchGL::handleMouseUp(S32 x, S32 y, MASK mask)
{
	// switch to mouse navigation mode
	LLMenuGL::setKeyboardMode(false);

	doIt();
	make_ui_sound("UISndClickRelease");
	return true;
}

bool LLMenuItemBranchGL::handleAcceleratorKey(KEY key, MASK mask)
{
	if (getBranch())
	{
		return getBranch()->handleAcceleratorKey(key, mask);
	}
	return false;
}

//virtual
LLXMLNodePtr LLMenuItemBranchGL::getXML(bool save_children) const
{
	if (getBranch())
	{
		return getBranch()->getXML();
	}

	return LLMenuItemGL::getXML();
}

// This method checks to see if the accelerator key is already in use; if not,
// it will be added to the list
bool LLMenuItemBranchGL::addToAcceleratorList(std::list<LLKeyBinding*>* listp)
{
	if (getBranch())
	{
		U32 item_count = getBranch()->getItemCount();
		LLMenuItemGL* item;
		while (item_count--)
		{
			if ((item = getBranch()->getItem(item_count)))
			{
				return item->addToAcceleratorList(listp);
			}
		}
	}
	return false;
}

// Called to rebuild the draw label
void LLMenuItemBranchGL::buildDrawLabel()
{
	mDrawAccelLabel.clear();
	std::string st = mDrawAccelLabel;
	appendAcceleratorString(st);
	mDrawAccelLabel = st;
	mDrawBranchLabel = BRANCH_SUFFIX;
}

// Does the primary functionality of the menu item.
void LLMenuItemBranchGL::doIt()
{
	openMenu();

	// keyboard navigation automatically propagates highlight to sub-menu
	// to facilitate fast menu control via jump keys
	if (getBranch() && LLMenuGL::getKeyboardMode() &&
		!getBranch()->getHighlightedItem())
	{
		getBranch()->highlightNextItem(NULL);
	}
}

bool LLMenuItemBranchGL::handleKey(KEY key, MASK mask, bool called_from_parent)
{
	bool handled = false;
	if (called_from_parent && getBranch())
	{
		handled = getBranch()->handleKey(key, mask, called_from_parent);
	}

	if (!handled)
	{
		handled = LLMenuItemGL::handleKey(key, mask, called_from_parent);
	}

	return handled;
}

bool LLMenuItemBranchGL::handleUnicodeChar(llwchar uni_char,
										   bool called_from_parent)
{
	bool handled = false;
	if (called_from_parent && getBranch())
	{
		handled = getBranch()->handleUnicodeChar(uni_char, true);
	}

	if (!handled)
	{
		handled = LLMenuItemGL::handleUnicodeChar(uni_char,
												  called_from_parent);
	}

	return handled;
}

void LLMenuItemBranchGL::setHighlight(bool highlight)
{
	if (highlight == getHighlight()) return;

	LLMenuGL* menup = getMenu();
	LLMenuGL* branchp = getBranch();
	if (!menup || !branchp)
	{
		return;
	}

	bool torn_off = branchp->getTornOff();
	// Note: do not auto open torn off sub-menus (need to explicitly active
	// menu item to give them focus)
	bool auto_open = !torn_off && getEnabled() && !branchp->getVisible();

	// Torn off menus do not open sub menus on hover unless they have focus
	if (auto_open && menup->getTornOff())
	{
		LLView* mviewp = menup->getParent();
		if (mviewp)
		{
			LLFloater* mparentp = mviewp->asFloater();
			if (mparentp && !mparentp->hasFocus())
			{
				auto_open = false;
			}
		}
	}

	LLMenuItemGL::setHighlight(highlight);
	if (highlight)
	{
		if (auto_open)
		{
			openMenu();
		}
	}
	else if (torn_off)
	{
		LLView* pviewp = branchp->getParent();
		if (pviewp)
		{
			LLFloater* parentp = pviewp->asFloater();
			if (parentp)
			{
				parentp->setFocus(false);
			}
		}
		branchp->clearHoverItem();
	}
	else
	{
		branchp->setVisible(false);
	}
}

void LLMenuItemBranchGL::draw()
{
	LLMenuItemGL::draw();

	LLMenuGL* branch = getBranch();
	if (branch && branch->getVisible() && !branch->getTornOff())
	{
		setHighlight(true);
	}
}

void LLMenuItemBranchGL::updateBranchParent(LLView* parentp)
{
	LLMenuGL* branchp = getBranch();
	if (branchp && !branchp->getParent())
	{
		// Make the branch menu a sibling of my parent menu
		branchp->updateParent(parentp);
	}
}

void LLMenuItemBranchGL::onVisibilityChange(bool new_visibility)
{
	LLMenuGL* branch = getBranch();
	if (!new_visibility && branch && !branch->getTornOff())
	{
		branch->setVisible(false);
	}
	LLMenuItemGL::onVisibilityChange(new_visibility);
}

bool LLMenuItemBranchGL::handleKeyHere(KEY key, MASK mask)
{
	LLMenuGL* menup = getMenu();
	LLMenuGL* branchp = getBranch();
	if (branchp && menup)
	{
		if (branchp->getVisible() && menup->getVisible() && key == KEY_LEFT)
		{
			// Switch to keyboard navigation mode
			LLMenuGL::setKeyboardMode(true);

			bool handled = branchp->clearHoverItem();
			if (branchp->getTornOff())
			{
				LLView* pviewp = branchp->getParent();
				if (pviewp)
				{
					LLFloater* parentp = pviewp->asFloater();
					if (parentp)
					{
						parentp->setFocus(false);
					}
				}
			}
			if (handled && menup->getTornOff())
			{
				LLView* mviewp = menup->getParent();
				if (mviewp)
				{
					LLFloater* mparentp = mviewp->asFloater();
					if (mparentp)
					{
						mparentp->setFocus(true);
					}
				}
			}
			return handled;
		}

		if (getHighlight() && menup->isOpen() && key == KEY_RIGHT &&
			!branchp->getHighlightedItem())
		{
			// Switch to keyboard navigation mode
			LLMenuGL::setKeyboardMode(true);

			LLMenuItemGL* itemp = branchp->highlightNextItem(NULL);
			if (itemp)
			{
				return true;
			}
		}
	}
	return LLMenuItemGL::handleKeyHere(key, mask);
}

void LLMenuItemBranchGL::openMenu()
{
	LLMenuGL* branch = getBranch();
	if (!branch) return;

	if (branch->getTornOff())
	{
		LLView* pviewp = branch->getParent();
		if (pviewp)
		{
			LLFloater* parentp = pviewp->asFloater();
			if (parentp)
			{
				gFloaterViewp->bringToFront(parentp);
				// This might not be necessary, as torn off branches do not get
				// focus and hence no highligth
				branch->highlightNextItem(NULL);
			}
		}
		return;
	}

	if (branch->getVisible() || !LLMenuGL::sMenuContainer)
	{
		return;
	}

	// Get valid rectangle for menus
	const LLRect menu_region_rect = LLMenuGL::sMenuContainer->getMenuRect();

	branch->arrange();

	LLRect rect = branch->getRect();
	// Calculate root-view relative position for branch menu
	S32 left = getRect().mRight;
	S32 top = getRect().mTop - getRect().mBottom;

	LLView* parentp = branch->getParent();
	localPointToOtherView(left, top, &left, &top, parentp);

	rect.setLeftTopAndSize(left, top, rect.getWidth(), rect.getHeight());

	if (branch->getCanTearOff())
	{
		rect.translate(0, TEAROFF_SEPARATOR_HEIGHT_PIXELS);
	}
	branch->setRect(rect);
	S32 x = 0;
	S32 y = 0;
	branch->localPointToOtherView(0, 0, &x, &y, parentp);
	S32 delta_x = 0;
	S32 delta_y = 0;
	if (y < menu_region_rect.mBottom)
	{
		delta_y = menu_region_rect.mBottom - y;
	}

	S32 menu_region_width = menu_region_rect.getWidth();
	if (x - menu_region_rect.mLeft > menu_region_width - rect.getWidth())
	{
		// Move sub-menu over to left side
		delta_x = llmax(-x, -rect.getWidth() - getRect().getWidth());
	}
	branch->translate(delta_x, delta_y);
	branch->setVisible(true);
	if (parentp)
	{
		parentp->sendChildToFront(branch);
	}
}

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLMenuItemBranchDownGL
//
// The LLMenuItemBranchDownGL represents a menu item that has a
// sub-menu. This is used to make menu bar menus.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLMenuItemBranchDownGL : public LLMenuItemBranchGL
{
protected:

public:
	LLMenuItemBranchDownGL(const std::string& name, const std::string& label,
						   LLHandle<LLView> branch, KEY key = KEY_NONE,
						   MASK mask = MASK_NONE);

	virtual std::string getType() const			{ return "menu"; }

	// Returns the normal width of this control in pixels - this is
	// used for calculating the widest item, as well as for horizontal
	// arrangement.
	virtual U32 getNominalWidth() const;

	// Called to rebuild the draw label
	virtual void buildDrawLabel();

	// Handles opening, positioning, and arranging the menu branch associated
	// with this item
	virtual void openMenu();

	// Sets the hover status (called by its menu) and if the object is active.
	// This is used for behavior transfer.
	virtual void setHighlight(bool highlight);

	virtual bool isActive() const;

	// LLView functionality
	virtual bool handleMouseDown(S32 x, S32 y, MASK mask);
	virtual bool handleMouseUp(S32 x, S32 y, MASK mask);
	virtual void draw();
	virtual bool handleKeyHere(KEY key, MASK mask);

	virtual bool handleAcceleratorKey(KEY key, MASK mask);
};

LLMenuItemBranchDownGL::LLMenuItemBranchDownGL(const std::string& name,
											   const std::string& label,
											   LLHandle<LLView> branch,
											   KEY key, MASK mask)
:	LLMenuItemBranchGL(name, label, branch, key, mask)
{
}

// Returns the normal width of this control in pixels: this is used for
// calculating the widest item, as well as for horizontal arrangement.
U32 LLMenuItemBranchDownGL::getNominalWidth() const
{
	U32 width = LEFT_PAD_PIXELS + LEFT_WIDTH_PIXELS + RIGHT_PAD_PIXELS;
	width += getFont()->getWidth(mLabel.getWString().c_str());
	return width;
}

// Called to rebuild the draw label
void LLMenuItemBranchDownGL::buildDrawLabel()
{
	mDrawAccelLabel.clear();
	std::string st = mDrawAccelLabel;
	appendAcceleratorString(st);
	mDrawAccelLabel = st;
}

void LLMenuItemBranchDownGL::openMenu()
{
	LLMenuGL* branch = getBranch();
	if (!branch) return;

	if (branch->getVisible() && !branch->getTornOff())
	{
		branch->setVisible(false);
	}
	else if (branch->getTornOff())
	{
		LLView* pviewp = branch->getParent();
		if (pviewp)
		{
			LLFloater* parentp = pviewp->asFloater();
			if (parentp)
			{
				gFloaterViewp->bringToFront(parentp);
			}
		}
	}
	else
	{
		// We are showing the drop-down menu, so patch up its labels/rects
		branch->arrange();

		LLRect rect = branch->getRect();
		S32 left = 0;
		S32 top = getRect().mBottom;
		LLView* parentp = branch->getParent();
		localPointToOtherView(left, top, &left, &top, parentp);

		rect.setLeftTopAndSize(left, top, rect.getWidth(), rect.getHeight());
		branch->setRect(rect);
		S32 x = 0;
		S32 y = 0;
		branch->localPointToScreen(0, 0, &x, &y);
		S32 delta_x = 0;

		LLCoordScreen window_size;
		gWindowp->getSize(&window_size);

		S32 window_width = window_size.mX;
		if (x > window_width - rect.getWidth())
		{
			delta_x = (window_width - rect.getWidth()) - x;
		}
		branch->translate(delta_x, 0);

		setHighlight(true);
		branch->setVisible(true);
		if (parentp)
		{
			parentp->sendChildToFront(branch);
		}
	}
}

// Sets the hover status (called by its menu)
void LLMenuItemBranchDownGL::setHighlight(bool highlight)
{
	if (highlight == getHighlight()) return;

	LLMenuGL* branch = getBranch();
	if (!branch) return;

	// NOTE: purposely calling all the way to the base to bypass auto-open.
	LLMenuItemGL::setHighlight(highlight);
	if (highlight)
	{
		return;
	}

	if (branch->getTornOff())
	{
		LLView* pviewp = branch->getParent();
		if (pviewp)
		{
			LLFloater* parentp = pviewp->asFloater();
			if (parentp)
			{
				parentp->setFocus(false);
			}
		}
		branch->clearHoverItem();
	}
	else
	{
		branch->setVisible(false);
	}
}

bool LLMenuItemBranchDownGL::isActive() const
{
	// For top level menus, being open is sufficient to be considered active,
	// because clicking on them with the mouse will open them, without moving
	// keyboard focus to them
	return isOpen();
}

bool LLMenuItemBranchDownGL::handleMouseDown(S32 x, S32 y, MASK mask)
{
	// Switch to mouse control mode
	LLMenuGL::setKeyboardMode(false);
	doIt();
	make_ui_sound("UISndClick");
	return true;
}

bool LLMenuItemBranchDownGL::handleMouseUp(S32 x, S32 y, MASK mask)
{
	return true;
}

bool LLMenuItemBranchDownGL::handleAcceleratorKey(KEY key, MASK mask)
{
    bool branch_visible = getBranch()->getVisible();
    bool handled = getBranch()->handleAcceleratorKey(key, mask);
	if (handled && !branch_visible && getVisible())
	{
		// Flash this menu entry because we triggered an invisible menu item
		LLMenuHolderGL::setActivatedItem(this);
	}

	return handled;
}

bool LLMenuItemBranchDownGL::handleKeyHere(KEY key, MASK mask)
{
	LLMenuGL* menup = getMenu();
	LLMenuGL* branchp = getBranch();
	if (!branchp || !menup)
	{
		return false;
	}

	// Do not do keyboard navigation of top-level menus unless in keyboard
	// mode, or menu expanded
	if (getHighlight() && menup->getVisible() &&
		(isActive() || LLMenuGL::getKeyboardMode()))
	{
		if (key == KEY_LEFT)
		{
			// switch to keyboard navigation mode
			LLMenuGL::setKeyboardMode(true);

			LLMenuItemGL* itemp = menup->highlightPrevItem(this);
			// Open new menu only if previous menu was open
			if (itemp && itemp->getEnabled() && branchp->getVisible())
			{
				itemp->doIt();
			}

			return true;
		}
		if (key == KEY_RIGHT)
		{
			// Switch to keyboard navigation mode
			LLMenuGL::setKeyboardMode(true);

			LLMenuItemGL* itemp = menup->highlightNextItem(this);
			// Open new menu only if previous menu was open
			if (itemp && itemp->getEnabled() && branchp->getVisible())
			{
				itemp->doIt();
			}

			return true;
		}
		if (key == KEY_DOWN)
		{
			// Switch to keyboard navigation mode
			LLMenuGL::setKeyboardMode(true);

			if (!isActive())
			{
				doIt();
			}
			branchp->highlightNextItem(NULL);
			return true;
		}
		if (key == KEY_UP)
		{
			// Switch to keyboard navigation mode
			LLMenuGL::setKeyboardMode(true);

			if (!isActive())
			{
				doIt();
			}
			branchp->highlightPrevItem(NULL);
			return true;
		}
	}

	return false;
}

void LLMenuItemBranchDownGL::draw()
{
	// *FIXME: try removing this
	if (getBranch()->getVisible() && !getBranch()->getTornOff())
	{
		setHighlight(true);
	}

	if (getHighlight())
	{
		gGL.color4fv(getHighlightBGColor().mV);
		gl_rect_2d(0, getRect().getHeight(), getRect().getWidth(), 0);
	}

	U8 font_style = getFontStyle();
	if (getEnabled() && !getDrawTextDisabled())
	{
		font_style |= LLFontGL::DROP_SHADOW_SOFT;
	}

	LLColor4 color;
	if (getHighlight())
	{
		color = getHighlightFGColor();
	}
	else if (getEnabled())
	{
		color = getEnabledColor();
	}
	else
	{
		color = getDisabledColor();
	}
	getFont()->render(mLabel.getWString(), 0, (F32)getRect().getWidth() * 0.5f,
					  (F32)LABEL_BOTTOM_PAD_PIXELS, color, LLFontGL::HCENTER,
					  LLFontGL::BOTTOM, font_style);

	// Underline navigation key only when keyboard navigation has been
	// initiated
	LLMenuGL* menup = getMenu();
	if (menup && menup->jumpKeysActive() && LLMenuGL::getKeyboardMode())
	{
		std::string upper_case_label = mLabel.getString();
		LLStringUtil::toUpper(upper_case_label);
		std::string::size_type offset = upper_case_label.find(getJumpKey());
		if (offset != std::string::npos)
		{
			S32 x_offset = ll_round((F32)getRect().getWidth() * 0.5f -
								    getFont()->getWidthF32(mLabel.getString(),
														   0, S32_MAX) * 0.5f);
			S32 x_begin = x_offset + getFont()->getWidth(mLabel, 0, offset);
			S32 x_end = x_offset + getFont()->getWidth(mLabel, 0, offset + 1);
			gl_line_2d(x_begin, LABEL_BOTTOM_PAD_PIXELS, x_end,
					   LABEL_BOTTOM_PAD_PIXELS);
		}
	}

	// Reset every frame so that we only show highlight when we get hover
	// events on that frame
	setHover(false);
}

//============================================================================
// Class LLMenuGL
//============================================================================

static LLRegisterWidget<LLMenuGL> r08(LL_MENU_GL_TAG);

// Default constructor
LLMenuGL::LLMenuGL(const std::string& name, const std::string& label,
				   LLHandle<LLFloater> parent_floater_handle)
:	LLUICtrl(name, LLRect(), false, NULL, NULL),
	mBackgroundColor(sDefaultBackgroundColor),
	mBgVisible(true),
	mParentMenuItem(NULL),
	mLabel(label),
	mDropShadowed(true),
	mHorizontalLayout(false),
	mKeepFixedSize(false),
	mLastMouseX(0),
	mLastMouseY(0),
	mMouseVelX(0),
	mMouseVelY(0),
	mTornOff(false),
	mTearOffItem(NULL),
	mSpilloverBranch(NULL),
	mSpilloverMenu(NULL),
	mParentFloaterHandle(parent_floater_handle),
	mJumpKey(KEY_NONE)
{
	mFadeTimer.stop();
	setCanTearOff(true, parent_floater_handle);
	setTabStop(false);
}

LLMenuGL::LLMenuGL(const std::string& label,
				   LLHandle<LLFloater> parent_floater_handle)
:	LLUICtrl(label, LLRect(), false, NULL, NULL),
	mBackgroundColor(sDefaultBackgroundColor),
	mBgVisible(true),
	mParentMenuItem(NULL),
	mLabel(label),
	mDropShadowed(true),
	mHorizontalLayout(false),
	mKeepFixedSize(false),
	mLastMouseX(0),
	mLastMouseY(0),
	mMouseVelX(0),
	mMouseVelY(0),
	mTornOff(false),
	mTearOffItem(NULL),
	mSpilloverBranch(NULL),
	mSpilloverMenu(NULL),
	mParentFloaterHandle(parent_floater_handle),
	mJumpKey(KEY_NONE)
{
	mFadeTimer.stop();
	setCanTearOff(true, parent_floater_handle);
	setTabStop(false);
}

LLMenuGL::~LLMenuGL()
{
	// Delete the branch, as it might not be in view hierarchy leave the menu,
	// because it is always in view hierarchy
	delete mSpilloverBranch;
	mJumpKeys.clear();
}

void LLMenuGL::setCanTearOff(bool tear_off,
							 LLHandle<LLFloater> parent_floater_handle)
{
	if (tear_off && mTearOffItem == NULL)
	{
		mTearOffItem = new LLMenuItemTearOffGL(parent_floater_handle);
		mItems.insert(mItems.begin(), mTearOffItem);
		addChildAtEnd(mTearOffItem);
		arrange();
	}
	else if (!tear_off && mTearOffItem != NULL)
	{
		mItems.remove(mTearOffItem);
		removeChild(mTearOffItem);
		delete mTearOffItem;
		mTearOffItem = NULL;
		arrange();
	}
}

//virtual
LLXMLNodePtr LLMenuGL::getXML(bool save_children) const
{
	LLXMLNodePtr node = LLView::getXML();

	node->setName(LL_MENU_GL_TAG);

	// Attributes

	node->createChild("opaque", true)->setBoolValue(mBgVisible);

	node->createChild("drop_shadow", true)->setBoolValue(mDropShadowed);

	node->createChild("tear_off", true)->setBoolValue(mTearOffItem != NULL);

	if (mBgVisible)
	{
		// TomY TODO: this should save out the color control name
		node->createChild("color", true)->setFloatValue(4, mBackgroundColor.mV);
	}

	// Contents
	for (item_list_t::const_iterator it = mItems.begin(),
									 end = mItems.end();
		 it != end; ++it)
	{
		LLMenuItemGL* item = *it;
		LLXMLNodePtr child_node = item->getXML();
		node->addChild(child_node);
	}

	return node;
}

void LLMenuGL::parseChildXML(LLXMLNodePtr child, LLView* parent,
							 LLUICtrlFactory* factory)
{
	if (child->hasName(LL_MENU_GL_TAG))
	{
		// SUBMENU
		LLMenuGL* submenu = (LLMenuGL*)LLMenuGL::fromXML(child, parent,
														 factory);
		appendMenu(submenu);
		if (sMenuContainer)
		{
			submenu->updateParent(sMenuContainer);
		}
		else
		{
			submenu->updateParent(parent);
		}
	}
	else if (child->hasName(LL_MENU_ITEM_CALL_GL_TAG) ||
			 child->hasName(LL_MENU_ITEM_CHECK_GL_TAG) ||
			 child->hasName(LL_MENU_ITEM_SEPARATOR_GL_TAG))
	{
		LLMenuItemGL* item = NULL;

		std::string type;
		std::string item_name;
		std::string source_label;
		std::string item_label;
		KEY jump_key = KEY_NONE;

		child->getAttributeString("type", type);
		child->getAttributeString("name", item_name);
		child->getAttributeString("label", source_label);

		// Parse jump key out of label
		typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
		boost::char_separator<char> sep("_");
		tokenizer tokens(source_label, sep);
		S32 token_count = 0;
		for (tokenizer::iterator token_iter = tokens.begin(),
								 end = tokens.end();
			 token_iter != end; ++token_iter)
		{
			item_label += *token_iter;
			if (token_count > 0)
			{
				jump_key = (*token_iter).c_str()[0];
			}
			++token_count;
		}

		if (child->hasName(LL_MENU_ITEM_SEPARATOR_GL_TAG))
		{
			appendSeparator(item_name);
		}
		else
		{
			// ITEM
			if (child->hasName(LL_MENU_ITEM_CALL_GL_TAG) ||
				child->hasName(LL_MENU_ITEM_CHECK_GL_TAG))
			{
				MASK mask = 0;

#ifdef LL_DARWIN
				// See if this Mac accelerator should really use the ctrl key
				// and not get mapped to cmd
				bool useMacCtrl = false;
				child->getAttributeBool("useMacCtrl", useMacCtrl);
#endif // LL_DARWIN

				std::string shortcut;
				child->getAttributeString("shortcut", shortcut);
				if (shortcut.find("control") != shortcut.npos)
				{
#ifdef LL_DARWIN
					if (useMacCtrl)
					{
						mask |= MASK_MAC_CONTROL;
					}
#endif // LL_DARWIN
					mask |= MASK_CONTROL;
				}
				if (shortcut.find("alt") != shortcut.npos)
				{
					mask |= MASK_ALT;
				}
				if (shortcut.find("shift") != shortcut.npos)
				{
					mask |= MASK_SHIFT;
				}
				size_t pipe_pos = shortcut.rfind("|");
				std::string key_str = shortcut.substr(pipe_pos + 1);

				KEY key = KEY_NONE;
				LLKeyboard::keyFromString(key_str.c_str(), &key);

				LLMenuItemCallGL* new_item;
				LLXMLNodePtr call_child;

				if (child->hasName(LL_MENU_ITEM_CHECK_GL_TAG))
				{
					std::string control_name;
					child->getAttributeString("control_name", control_name);

					new_item = new LLMenuItemCheckGL(item_name, item_label, 0,
													 0, control_name.c_str(),
													 parent, 0, key, mask);

					for (call_child = child->getFirstChild();
						 call_child.notNull();
						 call_child = call_child->getNextSibling())
					{
						if (call_child->hasName("on_check"))
						{
							std::string callback_name;
							std::string control_name;
							if (call_child->hasAttribute("function"))
							{
								call_child->getAttributeString("function",
															   callback_name);

								control_name = callback_name;

								std::string callback_data = item_name;
								if (call_child->hasAttribute("userdata"))
								{
									call_child->getAttributeString("userdata",
																   callback_data);
									if (!callback_data.empty())
									{
										control_name = llformat("%s(%s)",
																callback_name.c_str(),
																callback_data.c_str());
									}
								}

								LLSD userdata;
								userdata["control"] = control_name;
								userdata["data"] = callback_data;

								LLSimpleListener* callback;
								callback = parent->getListenerByName(callback_name);
								if (!callback)
								{
									LL_DEBUGS("MenuGL") << "Ignoring \"on_check\" \""
														<< item_name
														<< "\" because \""
														<< callback_name
														<< "\" is not registered"
														<< LL_ENDL;
									continue;
								}

								new_item->addListener(callback, "on_build",
													  userdata);
							}
							else if (call_child->hasAttribute("control"))
							{
								call_child->getAttributeString("control",
															   control_name);
							}
							else
							{
								continue;
							}
							LLControlVariable* control = parent->findControl(control_name);
							if (!control)
							{
								parent->addBoolControl(control_name, false);
							}
							((LLMenuItemCheckGL*)new_item)->setCheckedControl(control_name,
																			  parent);
						}
					}
				}
				else
				{
					new_item = new LLMenuItemCallGL(item_name, item_label, 0,
													0, 0, 0, key, mask);
				}

				for (call_child = child->getFirstChild();
					 call_child.notNull();
					 call_child = call_child->getNextSibling())
				{
					if (call_child->hasName("on_click"))
					{
						std::string callback_name;
						call_child->getAttributeString("function",
													   callback_name);

						std::string callback_data = item_name;
						if (call_child->hasAttribute("userdata"))
						{
							call_child->getAttributeString("userdata",
														   callback_data);
						}

						LLSimpleListener* callback =
							parent->getListenerByName(callback_name);
						if (!callback)
						{
							LL_DEBUGS("MenuGL") << "Ignoring \"on_click\" \""
												<< item_name << "\" because \""
												<< callback_name
												<< "\" is not registered"
												<< LL_ENDL;
							continue;
						}

						new_item->addListener(callback, "on_click",
											  callback_data);
					}
					if (call_child->hasName("on_enable"))
					{
						std::string callback_name;
						std::string control_name;
						if (call_child->hasAttribute("function"))
						{
							call_child->getAttributeString("function",
														   callback_name);

							control_name = callback_name;

							std::string callback_data;
							if (call_child->hasAttribute("userdata"))
							{
								call_child->getAttributeString("userdata",
															   callback_data);
								if (!callback_data.empty())
								{
									control_name = llformat("%s(%s)",
															callback_name.c_str(),
															callback_data.c_str());
								}
							}

							LLSD userdata;
							userdata["control"] = control_name;
							userdata["data"] = callback_data;

							LLSimpleListener* callback;
							callback = parent->getListenerByName(callback_name);
							if (!callback)
							{
								LL_DEBUGS("MenuGL") << "Ignoring \"on_enable\" \""
													<< item_name << "\" because \""
													<< callback_name
													<< "\" is not registered"
													<< LL_ENDL;
								continue;
							}

							new_item->addListener(callback, "on_build",
												  userdata);
						}
						else if (call_child->hasAttribute("control"))
						{
							call_child->getAttributeString("control",
														   control_name);
						}
						else
						{
							continue;
						}
						new_item->setEnabledControl(control_name, parent);
					}
					if (call_child->hasName("on_visible"))
					{
						std::string callback_name;
						std::string control_name;
						if (call_child->hasAttribute("function"))
						{
							call_child->getAttributeString("function",
														   callback_name);

							control_name = callback_name;

							std::string callback_data;
							if (call_child->hasAttribute("userdata"))
							{
								call_child->getAttributeString("userdata",
															   callback_data);
								if (!callback_data.empty())
								{
									control_name = llformat("%s(%s)",
															callback_name.c_str(),
															callback_data.c_str());
								}
							}

							LLSD userdata;
							userdata["control"] = control_name;
							userdata["data"] = callback_data;

							LLSimpleListener* callback =
								parent->getListenerByName(callback_name);
							if (!callback)
							{
								LL_DEBUGS("MenuGL") << "Ignoring \"on_visible\" \""
													<< item_name
													<< "\" because \""
													<< callback_name
													<< "\" is not registered"
													<< LL_ENDL;
								continue;
							}

							new_item->addListener(callback, "on_build",
												  userdata);
						}
						else if (call_child->hasAttribute("control"))
						{
							call_child->getAttributeString("control",
														   control_name);
						}
						else
						{
							continue;
						}
						new_item->setVisibleControl(control_name, parent);
					}
				}
				item = new_item;
				item->setLabel(item_label);
				if (jump_key != KEY_NONE)
				{
					item->setJumpKey(jump_key);
				}
			}

			if (item)
			{
				append(item);
			}
		}
	}
}

// Are we the childmost active menu and hence our jump keys should be enabled ?
// Or are we a free-standing torn-off menu (which uses jump keys too)
bool LLMenuGL::jumpKeysActive()
{
	LLMenuItemGL* highlighted_item = getHighlightedItem();
	if (!getVisible() || !getEnabled())
	{
		return false;
	}

	if (getTornOff())
	{
		// Activation of jump keys on torn off menus controlled by keyboard
		// focus
		LLView* pviewp = getParent();
		if (!pviewp)
		{
			return false;
		}
		LLFloater* parentp = pviewp->asFloater();
		return parentp && parentp->hasFocus();
	}

	// Are we the terminal active menu ?
	// Yes, if parent menu item deems us to be active (just being visible is
	// sufficient for top-level menus) and we do not have a highlighted menu
	// item pointing to an active sub-menu.
			// I have a parent that is active...
	return (!getParentMenuItem() || getParentMenuItem()->isActive()) &&
			//... but no child that is active
		   (!highlighted_item || !highlighted_item->isActive());
}

bool LLMenuGL::isOpen()
{
	if (getTornOff())
	{
		LLMenuItemGL* itemp = getHighlightedItem();
		// If we have an open sub-menu, then we are considered part of the open
		// menu chain even if we don't have focus
		if (itemp && itemp->isOpen())
		{
			return true;
		}

		// Otherwise we are only active if we have keyboard focus
		LLView* pviewp = getParent();
		if (!pviewp)
		{
			return false;
		}
		LLFloater* parentp = pviewp->asFloater();
		return parentp && parentp->hasFocus();
	}

	// Normally, menus are hidden as soon as the user focuses on another menu,
	// so just use the visibility criterion
	return getVisible();
}

//static
LLView* LLMenuGL::fromXML(LLXMLNodePtr node, LLView* parent,
						  LLUICtrlFactory* factory)
{
	std::string name = LL_MENU_GL_TAG;
	node->getAttributeString("name", name);

	std::string label = name;
	node->getAttributeString("label", label);

	// parse jump key out of label
	std::string new_menu_label;

	typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
	boost::char_separator<char> sep("_");
	tokenizer tokens(label, sep);

	KEY jump_key = KEY_NONE;
	S32 token_count = 0;
	for (tokenizer::iterator token_iter = tokens.begin(), end = tokens.end();
		 token_iter != end; ++token_iter)
	{
		new_menu_label += *token_iter;
		if (token_count > 0)
		{
			jump_key = (*token_iter).c_str()[0];
		}
		++token_count;
	}

	bool opaque = false;
	node->getAttributeBool("opaque", opaque);

	LLMenuGL* menu = new LLMenuGL(name, new_menu_label);

	menu->setJumpKey(jump_key);

	bool tear_off = false;
	node->getAttributeBool("tear_off", tear_off);
	menu->setCanTearOff(tear_off);

	if (node->hasAttribute("drop_shadow"))
	{
		bool drop_shadow = false;
		node->getAttributeBool("drop_shadow", drop_shadow);
		menu->setDropShadowed(drop_shadow);
	}

	menu->setBackgroundVisible(opaque);
	LLColor4 color(0, 0, 0, 1);
	if (opaque && LLUICtrlFactory::getAttributeColor(node, "color", color))
	{
		menu->setBackgroundColor(color);
	}

	bool create_jump_keys = false;
	node->getAttributeBool("create_jump_keys", create_jump_keys);

	LLXMLNodePtr child;
	for (child = node->getFirstChild(); child.notNull();
		 child = child->getNextSibling())
	{
		menu->parseChildXML(child, parent, factory);
	}

	if (create_jump_keys)
	{
		menu->createJumpKeys();
	}

	return menu;
}

//virtual
void LLMenuGL::deleteAllChildren()
{
	mItems.clear();
	LLUICtrl::deleteAllChildren();
}

// Rearranges the child rects so they fit the shape of the menu.
//virtual
void LLMenuGL::arrange()
{
	// Calculate the height & width, and set our rect based on that information
	const LLRect& initial_rect = getRect();

	U32 width = 0, height = MENU_ITEM_PADDING;

	cleanupSpilloverBranch();

	if (mItems.size())
	{
		const LLRect menu_region_rect =
			sMenuContainer ? sMenuContainer->getMenuRect()
						   : LLRect(0, S32_MAX, S32_MAX, 0);

		// Torn off menus are not constrained to the size of the screen
		U32 max_width = getTornOff() ? U32_MAX : menu_region_rect.getWidth();
		U32 max_height = getTornOff() ? U32_MAX : menu_region_rect.getHeight();
		// *FIX: create the item first and then ask for its dimensions ?
		static const S32 spillover_item_width =
			PLAIN_PAD_PIXELS + LLFontGL::getFontSansSerif()->getWidth("More");
		static const S32 spillover_item_height =
			MENU_ITEM_PADDING +
			ll_roundp(LLFontGL::getFontSansSerif()->getLineHeight());

		item_list_t::iterator item_iter;
		item_list_t::iterator items_begin = mItems.begin();
		item_list_t::iterator items_end = mItems.end();
		if (mHorizontalLayout)
		{
			for (item_iter = items_begin; item_iter != items_end; ++item_iter)
			{
				if ((*item_iter)->getVisible())
				{
					if (!getTornOff() &&
						// Do not spillover the first item !
						item_iter != items_begin &&
						width + (*item_iter)->getNominalWidth() >
							max_width - spillover_item_width)
					{
						// No room for any more items
						createSpilloverBranch();

						item_list_t::iterator spillover_iter;
						for (spillover_iter = item_iter;
							 spillover_iter != items_end; ++spillover_iter)
						{
							LLMenuItemGL* itemp = (*spillover_iter);
							removeChild(itemp);
							// *NOTE: Mani Favor addChild() in merge with
							// skinning:
							mSpilloverMenu->appendNoArrange(itemp);
						}
						// *NOTE: Mani Remove following two lines in merge with
						// skinning/viewer2.0 branch
						mSpilloverMenu->arrange();
						mSpilloverMenu->updateParent(sMenuContainer);
						mItems.erase(item_iter, items_end);
						mItems.push_back(mSpilloverBranch);
						addChild(mSpilloverBranch);
						height = llmax(height,
									   mSpilloverBranch->getNominalHeight());
						width += mSpilloverBranch->getNominalWidth();
						break;
					}
					else
					{
						// Track our rect
						height = llmax(height,
									   (*item_iter)->getNominalHeight());
						width += (*item_iter)->getNominalWidth();
					}
				}
			}
		}
		else
		{
			for (item_iter = items_begin; item_iter != items_end; ++item_iter)
			{
				if ((*item_iter)->getVisible())
				{
					if (!getTornOff() &&
						// Do not spillover the first item!
						item_iter != items_begin &&
						height + (*item_iter)->getNominalHeight() >
							max_height - spillover_item_height)
					{
						// No room for any more items
						createSpilloverBranch();

						item_list_t::iterator spillover_iter;
						for (spillover_iter= item_iter;
							 spillover_iter != items_end; ++spillover_iter)
						{
							LLMenuItemGL* itemp = (*spillover_iter);
							removeChild(itemp);
	 						// *NOTE:Mani Favor addChild() in merge with
							// skinning:
							mSpilloverMenu->appendNoArrange(itemp);
						}
						// *NOTE: Mani Remove following two lines in merge with
						// skinning/viewer2.0 branch
						mSpilloverMenu->arrange();
						mSpilloverMenu->updateParent(sMenuContainer);
						mItems.erase(item_iter, items_end);
						mItems.push_back(mSpilloverBranch);
						addChild(mSpilloverBranch);
						height += mSpilloverBranch->getNominalHeight();
						width = llmax(width, mSpilloverBranch->getNominalWidth());
						break;
					}
					else
					{
						// Track our rect
						height += (*item_iter)->getNominalHeight();
						width = llmax(width, (*item_iter)->getNominalWidth());
					}
				}
			}
		}

		setRect(LLRect(getRect().mLeft, getRect().mBottom + height,
					   getRect().mLeft + width, getRect().mBottom));

		S32 cur_height = (S32)llmin(max_height, height);
		S32 cur_width = 0;
		items_begin = mItems.begin();
		items_end = mItems.end();
		for (item_iter = items_begin; item_iter != items_end; ++item_iter)
		{
			if ((*item_iter)->getVisible())
			{
				// setup item rect to hold label
				LLRect rect;
				if (mHorizontalLayout)
				{
					rect.setLeftTopAndSize(cur_width, height,
										   (*item_iter)->getNominalWidth(),
										   height);
					cur_width += (*item_iter)->getNominalWidth();
				}
				else
				{
					rect.setLeftTopAndSize(0, cur_height, width,
										   (*item_iter)->getNominalHeight());
					cur_height -= (*item_iter)->getNominalHeight();
				}
				(*item_iter)->setRect(rect);
				(*item_iter)->buildDrawLabel();
			}
		}
	}
	if (mKeepFixedSize)
	{
		reshape(initial_rect.getWidth(), initial_rect.getHeight());
	}
}

void LLMenuGL::createSpilloverBranch()
{
	if (!mSpilloverBranch)
	{
		// Should be NULL but delete anyway
		delete mSpilloverMenu;

		// Technically, you cannot tear off spillover menus, but we are passing
		// the handle along just to be safe
		mSpilloverMenu = new LLMenuGL("More", "More", mParentFloaterHandle);
		mSpilloverMenu->updateParent(sMenuContainer);

		// Inherit colors
		mSpilloverMenu->setBackgroundColor(mBackgroundColor);

		mSpilloverMenu->setCanTearOff(false);

		mSpilloverBranch = new LLMenuItemBranchGL("More", "More",
												  mSpilloverMenu->getHandle());
		mSpilloverBranch->setFontStyle(LLFontGL::ITALIC);
	}
}

void LLMenuGL::cleanupSpilloverBranch()
{
	if (mSpilloverBranch && mSpilloverBranch->getParent() == this)
	{
		// Head-recursion to propagate items back up to root menu
		mSpilloverMenu->cleanupSpilloverBranch();

		removeChild(mSpilloverBranch);

		item_list_t::iterator found_iter;
		found_iter = std::find(mItems.begin(), mItems.end(), mSpilloverBranch);
		if (found_iter != mItems.end())
		{
			mItems.erase(found_iter);
		}

		// Pop off spillover items
		while (mSpilloverMenu->getItemCount())
		{
			LLMenuItemGL* itemp = mSpilloverMenu->getItem(0);
			mSpilloverMenu->removeChild(itemp);
			mSpilloverMenu->mItems.erase(mSpilloverMenu->mItems.begin());
			// put them at the end of our own list
			mItems.push_back(itemp);
			addChild(itemp);
		}

		// Delete the branch, and since the branch will delete the menu,
		// set the menu* to null.
		delete mSpilloverBranch;
		mSpilloverBranch = NULL;
		mSpilloverMenu = NULL;
	}
}

void LLMenuGL::createJumpKeys()
{
	mJumpKeys.clear();

	std::set<std::string> unique_words;
	std::set<std::string> shared_words;

	item_list_t::iterator item_it;
	typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
	boost::char_separator<char> sep(" ");

	for (item_it = mItems.begin(); item_it != mItems.end(); ++item_it)
	{
		std::string uppercase_label = (*item_it)->getLabel();
		LLStringUtil::toUpper(uppercase_label);

		tokenizer tokens(uppercase_label, sep);
		tokenizer::iterator token_iter;
		for (token_iter = tokens.begin(); token_iter != tokens.end();
			 ++token_iter)
		{
			if (unique_words.find(*token_iter) != unique_words.end())
			{
				// This word exists in more than one menu instance
				shared_words.insert(*token_iter);
			}
			else
			{
				// We have a new word, keep track of it
				unique_words.insert(*token_iter);
			}
		}
	}

	// Pre-assign specified jump keys
	for (item_it = mItems.begin(); item_it != mItems.end(); ++item_it)
	{
		KEY jump_key = (*item_it)->getJumpKey();
		if (jump_key != KEY_NONE)
		{
			if (mJumpKeys.find(jump_key) == mJumpKeys.end())
			{
				mJumpKeys.emplace(jump_key, *item_it);
			}
			else
			{
				// this key is already spoken for, so we need to reassign it
				// below
				(*item_it)->setJumpKey(KEY_NONE);
			}
		}
	}

	for (item_it = mItems.begin(); item_it != mItems.end(); ++item_it)
	{
		// Skip over items that already have assigned jump keys
		if ((*item_it)->getJumpKey() != KEY_NONE)
		{
			continue;
		}
		std::string uppercase_label = (*item_it)->getLabel();
		LLStringUtil::toUpper(uppercase_label);

		tokenizer tokens(uppercase_label, sep);
		tokenizer::iterator token_iter;

		bool found_key = false;
		for (token_iter = tokens.begin(); token_iter != tokens.end();
			 ++token_iter)
		{
			std::string uppercase_word = *token_iter;

			// This word is not shared with other menu entries...
			if (shared_words.find(*token_iter) == shared_words.end())
			{
				for (S32 i = 0; i < (S32)uppercase_word.size(); ++i)
				{
					char jump_key = uppercase_word[i];

					if (LLStringOps::isDigit(jump_key) ||
						(LLStringOps::isUpper(jump_key) &&
						 mJumpKeys.find(jump_key) == mJumpKeys.end()))
					{
						mJumpKeys[jump_key] = *item_it;
						(*item_it)->setJumpKey(jump_key);
						found_key = true;
						break;
					}
				}
			}
			if (found_key)
			{
				break;
			}
		}
	}
}

// Removes all items on the menu
void LLMenuGL::empty()
{
	cleanupSpilloverBranch();
	mItems.clear();
	deleteAllChildren();
}

// Adjusts rectangle of the menu
void LLMenuGL::setLeftAndBottom(S32 left, S32 bottom)
{
	setRect(LLRect(left, getRect().mTop, getRect().mRight, bottom));
	arrange();
}

bool LLMenuGL::handleJumpKey(KEY key)
{
	// must perform case-insensitive comparison, so just switch to uppercase
	// input key
	key = toupper(key);
	navigation_key_map_t::iterator found_it = mJumpKeys.find(key);
	if (found_it != mJumpKeys.end() && found_it->second->getEnabled())
	{
		// switch to keyboard navigation mode
		LLMenuGL::setKeyboardMode(true);
		// force highlight to close old menus and open and sub-menus
#if 0
		clearHoverItem();
#endif
		found_it->second->setHighlight(true);
		found_it->second->doIt();

	}

	// If we are navigating the menus, we need to eat the keystroke so rest of
	// UI does not handle it
	return true;
}

// Adds the menu item to this menu.
bool LLMenuGL::append(LLMenuItemGL* item)
{
	mItems.push_back(item);
	addChild(item);
	arrange();
	return true;
}

// *NOTE: should be removed when merging to skinning/viewer2.0 - Mani
// It is added as a fix to a viewer 1.23 bug that has already been addressed
// by skinning work.
bool LLMenuGL::appendNoArrange(LLMenuItemGL* item)
{
	mItems.push_back(item);
	addChild(item);
	return true;
}

// Adds a separator to this menu
bool LLMenuGL::appendSeparator(const std::string& separator_name)
{
	LLMenuItemGL* separator;
	if (separator_name.empty())
	{
		separator = new LLMenuItemSeparatorGL("separator");
	}
	else
	{
		separator = new LLMenuItemSeparatorGL(separator_name);
	}
	return append(separator);
}

// Removes a menu item from this menu.
bool LLMenuGL::remove(LLMenuItemGL* item)
{
	if (mSpilloverMenu)
	{
		cleanupSpilloverBranch();
	}

	item_list_t::iterator found_iter = std::find(mItems.begin(), mItems.end(),
												 item);
	if (found_iter != mItems.end())
	{
		mItems.erase(found_iter);
	}

	removeChild(item);

	if (sMenuContainer)
	{
		// We keep it around in case someone is pointing at it. The caller can
		// delete it if it is safe. Note that getMenu() will still not work
		// since its parent is not a menu.
		sMenuContainer->addChild(item);
	}

	arrange();
	return true;
}

// Adds a menu: this will create a cascading menu
bool LLMenuGL::appendMenu(LLMenuGL* menu)
{
	if (menu == this)
	{
		llerrs << "** Attempt to attach menu to itself. This is certainly "
			   << "a logic error." << llendl;
	}

	LLMenuItemBranchGL* branch = new LLMenuItemBranchGL(menu->getName(),
														menu->getLabel(),
														menu->getHandle());
	branch->setJumpKey(menu->getJumpKey());

	bool success = append(branch);

	// Inherit colors
	menu->setBackgroundColor(mBackgroundColor);

	return success;
}

void LLMenuGL::setEnabledSubMenus(bool enable)
{
	setEnabled(enable);
	for (item_list_t::iterator it = mItems.begin(), end = mItems.end();
		 it != end; ++it)
	{
		(*it)->setEnabledSubMenus(enable);
	}
}

// Pass the label and the enable flag for a menu item. true will make sure it
// is enabled, false will disable it.
void LLMenuGL::setItemEnabled(const std::string& name, bool enable)
{
	LLMenuItemGL* item = getItem(name);
	if (item)
	{
		item->setEnabled(enable);
		item->setEnabledSubMenus(enable);
	}
}

void LLMenuGL::setItemVisible(const std::string& name, bool visible)
{
	LLMenuItemGL* item = getItem(name);
	if (item)
	{
		item->setVisible(visible);
	}
}

void LLMenuGL::setItemLastSelected(LLMenuItemGL* item)
{
	if (getVisible())
	{
		LLMenuHolderGL::setActivatedItem(item);
	}

	// Fix the checkmarks
	item->buildDrawLabel();
}

void LLMenuGL::setItemLabel(const std::string& name, const std::string& label)
{
	LLMenuItemGL* item = getItem(name);
	if (item)
	{
		item->setLabel(label);
	}
}

U32 LLMenuGL::getItemCount()
{
	return mItems.size();
}

LLMenuItemGL* LLMenuGL::getItem(S32 number)
{
	if (number >= 0 && number < (S32)mItems.size())
	{
		for (item_list_t::iterator it = mItems.begin(), end = mItems.end();
			 it != end; ++it)
		{
			if (number == 0)
			{
				return *it;
			}
			--number;
		}
	}
	return NULL;
}

LLMenuItemGL* LLMenuGL::getItem(const std::string& name)
{
	for (item_list_t::iterator it = mItems.begin(), end = mItems.end();
		 it != end; ++it)
	{
		if ((*it)->getName() == name)
		{
			return *it;
		}
	}
	return NULL;
}

LLMenuItemGL* LLMenuGL::getHighlightedItem()
{
	for (item_list_t::iterator it = mItems.begin(), end = mItems.end();
		 it != end; ++it)
	{
		if ((*it)->getHighlight())
		{
			return *it;
		}
	}
	return NULL;
}

LLMenuItemGL* LLMenuGL::highlightNextItem(LLMenuItemGL* cur_item,
										  bool skip_disabled)
{
	// Highlighting first item on a torn off menu is the same as giving focus
	// to it
	if (!cur_item && getTornOff())
	{
		LLView* pviewp = getParent();
		if (pviewp)
		{
			LLFloater* parentp = pviewp->asFloater();
			if (parentp)
			{
				parentp->setFocus(true);
			}
		}
	}

	item_list_t::iterator cur_item_iter;
	item_list_t::iterator items_begin = mItems.begin();
	item_list_t::iterator items_end = mItems.end();
	for (cur_item_iter = items_begin; cur_item_iter != items_end;
		 ++cur_item_iter)
	{
		if (*cur_item_iter == cur_item)
		{
			break;
		}
	}

	item_list_t::iterator next_item_iter;
	if (cur_item_iter == items_end)
	{
		next_item_iter = items_begin;
	}
	else
	{
		next_item_iter = cur_item_iter;
		++next_item_iter;
		if (next_item_iter == items_end)
		{
			next_item_iter = items_begin;
		}
	}

	// When first highlighting a menu, skip over tear off menu item
	if (mTearOffItem && !cur_item)
	{
		// We know the first item is the tear off menu item
		cur_item_iter = items_begin;
		++next_item_iter;
		if (next_item_iter == items_end)
		{
			next_item_iter = items_begin;
		}
	}

	while (true)
	{
		// Skip separators and disabled/invisible items
		if ((*next_item_iter)->getEnabled() &&
			(*next_item_iter)->getVisible() &&
			(*next_item_iter)->getType() != SEPARATOR_NAME)
		{
			if (cur_item)
			{
				cur_item->setHighlight(false);
			}
			(*next_item_iter)->setHighlight(true);
			return *next_item_iter;
		}

		if (!skip_disabled || next_item_iter == cur_item_iter)
		{
			break;
		}

		++next_item_iter;
		if (next_item_iter == items_end)
		{
			if (cur_item_iter == items_end)
			{
				break;
			}
			next_item_iter = items_begin;
		}
	}

	return NULL;
}

LLMenuItemGL* LLMenuGL::highlightPrevItem(LLMenuItemGL* cur_item,
										  bool skip_disabled)
{
	// Highlighting first item on a torn off menu is the same as giving focus
	// to it
	if (!cur_item && getTornOff())
	{
		LLView* pviewp = getParent();
		if (pviewp)
		{
			LLFloater* parentp = pviewp->asFloater();
			if (parentp)
			{
				parentp->setFocus(true);
			}
		}
	}

	item_list_t::reverse_iterator cur_item_iter;
	item_list_t::reverse_iterator items_rbegin = mItems.rbegin();
	item_list_t::reverse_iterator items_rend = mItems.rend();
	for (cur_item_iter = items_rbegin; cur_item_iter != items_rend;
		 ++cur_item_iter)
	{
		if (*cur_item_iter == cur_item)
		{
			break;
		}
	}

	item_list_t::reverse_iterator prev_item_iter;
	if (cur_item_iter == items_rend)
	{
		prev_item_iter = items_rbegin;
	}
	else
	{
		prev_item_iter = cur_item_iter;
		++prev_item_iter;
		if (prev_item_iter == items_rend)
		{
			prev_item_iter = items_rbegin;
		}
	}

	while (true)
	{
		// Skip separators and disabled/invisible items
		if ((*prev_item_iter)->getEnabled() &&
			(*prev_item_iter)->getVisible() &&
			(*prev_item_iter)->getType() != SEPARATOR_NAME)
		{
			(*prev_item_iter)->setHighlight(true);
			return *prev_item_iter;
		}

		if (!skip_disabled || prev_item_iter == cur_item_iter)
		{
			break;
		}

		++prev_item_iter;
		if (prev_item_iter == items_rend)
		{
			if (cur_item_iter == items_rend)
			{
				break;
			}

			prev_item_iter = items_rbegin;
		}
	}

	return NULL;
}

void LLMenuGL::buildDrawLabels()
{
	for (item_list_t::iterator it = mItems.begin(), end = mItems.end();
		 it != end; ++it)
	{
		(*it)->buildDrawLabel();
	}
}

void LLMenuGL::updateParent(LLView* parentp)
{
	if (!parentp) return;

	if (getParent())
	{
		getParent()->removeChild(this);
	}
	parentp->addChild(this);
	for (item_list_t::iterator it = mItems.begin(), end = mItems.end();
		 it != end; ++it)
	{
		(*it)->updateBranchParent(parentp);
	}
}

bool LLMenuGL::handleAcceleratorKey(KEY key, MASK mask)
{
	// Do not handle if not enabled
	if (!getEnabled())
	{
		return false;
	}

	// Pass down even if not visible
	for (item_list_t::iterator it = mItems.begin(), end = mItems.end();
		 it != end; ++it)
	{
		LLMenuItemGL* itemp = *it;
		if (itemp->handleAcceleratorKey(key, mask))
		{
			return true;
		}
	}

	return false;
}

bool LLMenuGL::handleUnicodeCharHere(llwchar uni_char)
{
	if (jumpKeysActive())
	{
		return handleJumpKey((KEY)uni_char);
	}
	return false;
}

bool LLMenuGL::handleHover(S32 x, S32 y, MASK mask)
{
	// Leave submenu in place if slope of mouse < MAX_MOUSE_SLOPE_SUB_MENU
	bool no_mouse_data = mLastMouseX == 0 && mLastMouseY == 0;
	S32 mouse_delta_x = no_mouse_data ? 0 : x - mLastMouseX;
	S32 mouse_delta_y = no_mouse_data ? 0 : y - mLastMouseY;
	LLVector2 mouse_dir((F32)mouse_delta_x, (F32)mouse_delta_y);
	mouse_dir.normalize();
	LLVector2 mouse_avg_dir((F32)mMouseVelX, (F32)mMouseVelY);
	mouse_avg_dir.normalize();
	F32 interp = 0.5f * llclamp(mouse_dir * mouse_avg_dir, 0.f, 1.f);
	mMouseVelX = ll_round(lerp((F32)mouse_delta_x, (F32)mMouseVelX, interp));
	mMouseVelY = ll_round(lerp((F32)mouse_delta_y, (F32)mMouseVelY, interp));
	mLastMouseX = x;
	mLastMouseY = y;

	// Do not change menu focus unless mouse is moving or alt key is not held
	// down
	if ((abs(mMouseVelX) > 0 || abs(mMouseVelY) > 0) &&
		(!mHasSelection || mMouseVelX < 0 ||
		 //(mouse_delta_x == 0 && mouse_delta_y == 0) ||
		 fabsf((F32)mMouseVelY) / fabsf((F32)mMouseVelX) > MAX_MOUSE_SLOPE_SUB_MENU))
	{
		child_list_const_iter_t child_it;
		child_list_const_iter_t child_begin = getChildList()->begin();
		child_list_const_iter_t child_end = getChildList()->end();
		for (child_it = child_begin; child_it != child_end; ++child_it)
		{
			LLView* viewp = *child_it;
			S32 local_x = x - viewp->getRect().mLeft;
			S32 local_y = y - viewp->getRect().mBottom;
			if (!viewp->pointInView(local_x, local_y) &&
				((LLMenuItemGL*)viewp)->getHighlight())
			{
				// moving mouse always highlights new item
				if (mouse_delta_x != 0 || mouse_delta_y != 0)
				{
					((LLMenuItemGL*)viewp)->setHighlight(false);
				}
			}
		}

		for (child_it = child_begin; child_it != child_end; ++child_it)
		{
			LLView* viewp = *child_it;
			S32 local_x = x - viewp->getRect().mLeft;
			S32 local_y = y - viewp->getRect().mBottom;
			// RN: always call handleHover to track mGotHover status but only
			// set highlight when mouse is moving
			if (viewp->getVisible() &&
				// RN: allow disabled items to be highlighted to preserve
				// "active" menus when/ moving mouse through them
				//viewp->getEnabled() &&
				viewp->pointInView(local_x, local_y) &&
				viewp->handleHover(local_x, local_y, mask))
			{
				// moving mouse always highlights new item
				if (mouse_delta_x != 0 || mouse_delta_y != 0)
				{
					((LLMenuItemGL*)viewp)->setHighlight(true);
					LLMenuGL::setKeyboardMode(false);
				}
				mHasSelection = true;
			}
		}
	}
	gWindowp->setCursor(UI_CURSOR_ARROW);

	return true;
}

void LLMenuGL::draw()
{
	if (mDropShadowed && !mTornOff)
	{
		gl_drop_shadow(0, getRect().getHeight(), getRect().getWidth(), 0,
					   LLUI::sColorDropShadow, LLUI::sDropShadowFloater);
	}

	if (mBgVisible)
	{
		gl_rect_2d(0, getRect().getHeight(), getRect().getWidth(), 0,
				   mBackgroundColor);
	}
	LLView::draw();
}

void LLMenuGL::drawBackground(LLMenuItemGL* itemp, LLColor4& color)
{
	gGL.color4fv(color.mV);
	LLRect item_rect = itemp->getRect();
	gl_rect_2d(0, item_rect.getHeight(), item_rect.getWidth(), 0);
}

void LLMenuGL::setVisible(bool visible)
{
	if (visible != getVisible())
	{
		if (!visible)
		{
			mFadeTimer.start();
			clearHoverItem();
			// Reset last known mouse coordinates so we don't spoof a mouse
			// move next time we're opened
			mLastMouseX = 0;
			mLastMouseY = 0;
		}
		else
		{
			mHasSelection = false;
			mFadeTimer.stop();
		}

		LLView::setVisible(visible);
	}
}

LLMenuGL* LLMenuGL::getChildMenuByName(const char* name, bool recurse) const
{
	LLView* view = getChildView(name, recurse, false);
	if (view)
	{
		LLMenuItemBranchGL* branch = dynamic_cast<LLMenuItemBranchGL*>(view);
		if (branch)
		{
			return branch->getBranch();
		}

		LLMenuGL* menup = dynamic_cast<LLMenuGL*>(view);
		if (menup)
		{
			return menup;
		}
	}
	llwarns << "Child Menu " << name << " not found in menu " << getName()
			<< llendl;
	return NULL;
}

bool LLMenuGL::clearHoverItem()
{
	for (child_list_const_iter_t child_it = getChildList()->begin(),
								 end = getChildList()->end();
		 child_it != end; ++child_it)
	{
		LLMenuItemGL* itemp = (LLMenuItemGL*)*child_it;
		if (itemp->getHighlight())
		{
			itemp->setHighlight(false);
			return true;
		}
	}
	return false;
}

void hide_top_view(LLView* view)
{
	if (view) view->setVisible(false);
}

//static
void LLMenuGL::showPopup(LLView* spawning_view, LLMenuGL* menu, S32 x, S32 y)
{
	if (!sMenuContainer) return;

	const LLRect menu_region_rect = sMenuContainer->getMenuRect();

	constexpr S32 HPAD = 2;
	LLRect rect = menu->getRect();
	//LLView* cur_view = spawning_view;
	S32 left = x + HPAD;
	S32 top = y;
	spawning_view->localPointToOtherView(left, top, &left, &top,
										 menu->getParent());
	rect.setLeftTopAndSize(left, top,
							rect.getWidth(), rect.getHeight());

	//rect.setLeftTopAndSize(x + HPAD, y, rect.getWidth(), rect.getHeight());
	menu->setRect(rect);

	S32 bottom;
	left = rect.mLeft;
	bottom = rect.mBottom;
#if 0
	menu->getParent()->localPointToScreen(rect.mLeft, rect.mBottom,
										  &left, &bottom);
#endif
	S32 delta_x = 0;
	S32 delta_y = 0;
	if (bottom < menu_region_rect.mBottom)
	{
		// At this point, we need to move the context menu to the
		// other side of the mouse.
		//delta_y = menu_region_rect.mBottom - bottom;
		delta_y = (rect.getHeight() + 2 * HPAD);
	}

	if (left > menu_region_rect.mRight - rect.getWidth())
	{
		// At this point, we need to move the context menu to the
		// other side of the mouse.
		//delta_x = (window_width - rect.getWidth()) - x;
		delta_x = -rect.getWidth() - 2 * HPAD;
	}
	menu->translate(delta_x, delta_y);
	menu->setVisible(true);
	LLView* parent = menu->getParent();
	if (parent)
	{
		parent->sendChildToFront(menu);
	}
}

//-----------------------------------------------------------------------------
// class LLPieMenuBranch
// A branch to another pie menu
//-----------------------------------------------------------------------------

class LLPieMenuBranch : public LLMenuItemGL
{
public:
	LLPieMenuBranch(const std::string& name, const std::string& label,
					LLPieMenu* branch);

	virtual LLXMLNodePtr getXML(bool save_children = true) const;

	// Called to rebuild the draw label
	virtual void buildDrawLabel();

	// Does the primary funcationality of the menu item.
	virtual void doIt();

	LL_INLINE LLPieMenu* getBranch()				{ return mBranch; }

protected:
	LLPieMenu* mBranch;
};

LLPieMenuBranch::LLPieMenuBranch(const std::string& name,
								 const std::string& label,
								 LLPieMenu* branch)
:	LLMenuItemGL(name, label, KEY_NONE, MASK_NONE),
	mBranch(branch)
{
	mBranch->hide(false);
	mBranch->setParentMenuItem(this);
}

//virtual
LLXMLNodePtr LLPieMenuBranch::getXML(bool save_children) const
{
	if (mBranch)
	{
		return mBranch->getXML();
	}

	return LLMenuItemGL::getXML();
}

// called to rebuild the draw label
void LLPieMenuBranch::buildDrawLabel()
{
	{
		// default enablement is this -- if any of the subitems are
		// enabled, this item is enabled. JC
		U32 sub_count = mBranch->getItemCount();
		U32 i;
		bool any_enabled = false;
		for (i = 0; i < sub_count; ++i)
		{
			LLMenuItemGL* item = mBranch->getItem(i);
			item->buildDrawLabel();
			if (item->getEnabled() && !item->getDrawTextDisabled())
			{
				any_enabled = true;
				break;
			}
		}
		setDrawTextDisabled(!any_enabled);
		setEnabled(true);
	}

	mDrawAccelLabel.clear();
	std::string st = mDrawAccelLabel;
	appendAcceleratorString(st);
	mDrawAccelLabel = st;

	// No special branch suffix
	mDrawBranchLabel.clear();
}

// Does the primary funcationality of the menu item.
void LLPieMenuBranch::doIt()
{
	LLPieMenu* parentp = (LLPieMenu*)getParent();
	if (!parentp)
	{
		llwarns << "NULL parent. Aborted." << llendl;
		return;
	}
	LLRect rect = parentp->getRect();
	S32 center_x;
	S32 center_y;
	parentp->localPointToScreen(rect.getWidth() / 2, rect.getHeight() / 2,
								&center_x, &center_y);

	parentp->hide(false);
	mBranch->show(center_x, center_y, false);
}

//-----------------------------------------------------------------------------
// class LLPieMenu
// A circular menu of items, icons, etc.
//-----------------------------------------------------------------------------

LLPieMenu::LLPieMenu(const std::string& name, const std::string& label)
:	LLMenuGL(name, label),
	mFirstMouseDown(false),
	mUseInfiniteRadius(false),
	mHoverItem(NULL),
	mHoverThisFrame(false),
	mHoveredAnyItem(false),
	mOuterRingAlpha(1.f),
	mCurRadius(0.f),
	mRightMouseDown(false)
{
	LLMenuGL::setVisible(false);
	setCanTearOff(false);
}

LLPieMenu::LLPieMenu(const std::string& name)
:	LLMenuGL(name, name),
	mFirstMouseDown(false),
	mUseInfiniteRadius(false),
	mHoverItem(NULL),
	mHoverThisFrame(false),
	mHoveredAnyItem(false),
	mOuterRingAlpha(1.f),
	mCurRadius(0.f),
	mRightMouseDown(false)
{
	LLMenuGL::setVisible(false);
	setCanTearOff(false);
}

//virtual
LLXMLNodePtr LLPieMenu::getXML(bool save_children) const
{
	LLXMLNodePtr node = LLMenuGL::getXML();
	node->setName(LL_PIE_MENU_TAG);
	return node;
}

void LLPieMenu::initXML(LLXMLNodePtr node, LLView* context, LLUICtrlFactory* factory)
{
	LLXMLNodePtr child;
	for (child = node->getFirstChild(); child.notNull();
		 child = child->getNextSibling())
	{
		if (child->hasName(LL_PIE_MENU_TAG))
		{
			// SUBMENU
			std::string name(LL_MENU_GL_TAG);
			child->getAttributeString("name", name);
			std::string label(name);
			child->getAttributeString("label", label);

			LLPieMenu* submenu = new LLPieMenu(name, label);
			appendPieMenu(submenu);
			submenu->initXML(child, context, factory);
		}
		else
		{
			parseChildXML(child, context, factory);
		}
	}
}

//virtual
void LLPieMenu::setVisible(bool visible)
{
	if (!visible)
	{
		hide(false);
	}
}

bool LLPieMenu::handleHover(S32 x, S32 y, MASK mask)
{
	// This is mostly copied from the llview class, but it continues the hover
	// handle code after a hover handler has been found.
	bool handled = false;

#if 0	// If we got a hover event, we've already moved the cursor for any menu
		// shifts, so subsequent mouseup messages will be in the correct
		// position. No need to correct them.
	mShiftHoriz = 0;
	mShiftVert = 0;
#endif

	// Release mouse capture after short period of visibility if we are using a
	// finite boundary so that right click outside of boundary will trigger new
	// pie menu
	if (hasMouseCapture() && !mRightMouseDown &&
		mShrinkBorderTimer.getStarted() &&
		mShrinkBorderTimer.getElapsedTimeF32() >= PIE_SHRINK_TIME)
	{
		gFocusMgr.setMouseCapture(NULL);
		mUseInfiniteRadius = false;
	}

	LLMenuItemGL* item = pieItemFromXY(x, y);
	if (item && item->getEnabled())
	{
		gWindowp->setCursor(UI_CURSOR_ARROW);
		LL_DEBUGS("UserInput") << "hover handled by " << getName() << LL_ENDL;
		handled = true;

		if (item != mHoverItem)
		{
			if (mHoverItem)
			{
				mHoverItem->setHighlight(false);
			}
			mHoverItem = item;
			mHoverItem->setHighlight(true);

#if 0		// Useless... They are all the same sound anyway !
			switch (pieItemIndexFromXY(x, y))
			{
			case 0:
				make_ui_sound("UISndPieMenuSliceHighlight0");
				break;
			case 1:
				make_ui_sound("UISndPieMenuSliceHighlight1");
				break;
			case 2:
				make_ui_sound("UISndPieMenuSliceHighlight2");
				break;
			case 3:
				make_ui_sound("UISndPieMenuSliceHighlight3");
				break;
			case 4:
				make_ui_sound("UISndPieMenuSliceHighlight4");
				break;
			case 5:
				make_ui_sound("UISndPieMenuSliceHighlight5");
				break;
			case 6:
				make_ui_sound("UISndPieMenuSliceHighlight6");
				break;
			case 7:
				make_ui_sound("UISndPieMenuSliceHighlight7");
				break;
			default:
				make_ui_sound("UISndPieMenuSliceHighlight0");
				break;
			}
#else
			make_ui_sound("UISndPieMenuSliceHighlight");
#endif
		}
		mHoveredAnyItem = true;
	}
	else
	{
		// Clear out our selection
		if (mHoverItem)
		{
			mHoverItem->setHighlight(false);
			mHoverItem = NULL;
		}
	}

	if (!handled && pointInView(x, y))
	{
		gWindowp->setCursor(UI_CURSOR_ARROW);
		LL_DEBUGS("UserInput") << "hover handled by " << getName() << LL_ENDL;
		handled = true;
	}

	mHoverThisFrame = true;

	return handled;
}

bool LLPieMenu::handleMouseDown(S32 x, S32 y, MASK mask)
{
	bool handled = false;

	// The click was somewhere within our rectangle
	LLMenuItemGL* item = pieItemFromXY(x, y);
	if (item)
	{
		// Lie to the item about where the click happened to make sure it is
		// within its rectangle
		handled = item->handleMouseDown(0, 0, mask);
	}
	else if (!mRightMouseDown)
	{
		// Call hidemenus to make sure transient selections get cleared
		((LLMenuHolderGL*)getParent())->hideMenus();
	}

	// Always handle mouse down as mouse up will close open menus
	return handled;
}

bool LLPieMenu::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
	bool handled = false;

	mRightMouseDown = true;

	// The click was somewhere within our rectangle
	LLMenuItemGL* item = pieItemFromXY(x, y);
	S32 delta_x = x - getLocalRect().getCenterX() /*+ mShiftHoriz*/;
	S32 delta_y = y - getLocalRect().getCenterY() /*+ mShiftVert*/;
	bool clicked_in_pie = mUseInfiniteRadius ||
						  delta_x * delta_x + delta_y * delta_y <
								mCurRadius * mCurRadius;

	// Grab mouse if right clicking anywhere within pie (even deadzone in
	// middle), to detect drag outside of pie
	if (clicked_in_pie)
	{
		// Capture mouse cursor as if on initial menu show
		gFocusMgr.setMouseCapture(this);
		mShrinkBorderTimer.stop();
		mUseInfiniteRadius = true;
		handled = true;
	}

	// Lie to the item about where the click happened to make sure it is within
	// its rectangle
	if (item && item->handleMouseDown(0, 0, mask))
	{
		handled = true;
	}

	return handled;
}

bool LLPieMenu::handleRightMouseUp(S32 x, S32 y, MASK mask)
{
	// Release mouse capture when right mouse button released, and we're past
	// the shrink time
	if (mShrinkBorderTimer.getStarted() &&
		mShrinkBorderTimer.getElapsedTimeF32() > PIE_SHRINK_TIME)
	{
		mUseInfiniteRadius = false;
		gFocusMgr.setMouseCapture(NULL);
	}

	S32 delta_x = x /*+ mShiftHoriz*/ - getLocalRect().getCenterX();
	S32 delta_y = y /*+ mShiftVert*/ - getLocalRect().getCenterY();
	if (!mHoveredAnyItem && !mFirstMouseDown &&
		delta_x * delta_x + delta_y * delta_y < PIE_CENTER_SIZE * PIE_CENTER_SIZE)
	{
		// User released right mouse button in middle of pie, interpret this as
		// closing the menu
		if (sMenuContainer)
		{
			sMenuContainer->hideMenus();
		}
		return true;
	}

	bool result = handleMouseUp(x, y, mask);
	mRightMouseDown = false;
	mHoveredAnyItem = false;

	return result;
}

bool LLPieMenu::handleMouseUp(S32 x, S32 y, MASK mask)
{
	bool handled = false;

	// The click was somewhere within our rectangle
	LLMenuItemGL* item = pieItemFromXY(x, y);

	if (item)
	{
		// Lie to the item about where the click happened to make sure it is
		// within the item's rectangle
		if (item->getEnabled())
		{
			handled = item->handleMouseUp(0, 0, mask);
			hide(true);
		}
	}
	else if (!mRightMouseDown)
	{
		// Call hidemenus to make sure transient selections get cleared
		((LLMenuHolderGL*)getParent())->hideMenus();
	}

	if (handled)
	{
		make_ui_sound("UISndClickRelease");
	}

	if (!handled && !mUseInfiniteRadius && sMenuContainer)
	{
		// Call hidemenus to make sure transient selections get cleared
		sMenuContainer->hideMenus();
	}

	if (mFirstMouseDown)
	{
		make_ui_sound("UISndPieMenuAppear");
		mFirstMouseDown = false;
	}

	// *FIXME: is this necessary ?
	if (!mShrinkBorderTimer.getStarted())
	{
		mShrinkBorderTimer.start();
	}

	return handled;
}

//virtual
void LLPieMenu::draw()
{
	// Clear hover if mouse moved away
	if (!mHoverThisFrame && mHoverItem)
	{
		mHoverItem->setHighlight(false);
		mHoverItem = NULL;
	}

	// correct for non-square pixels
	F32 center_x = (F32)getRect().getWidth() * 0.5f;
	F32 center_y = (F32)getRect().getHeight() * 0.5f;
	S32 steps = 100;

	mCurRadius = PIE_SCALE_FACTOR * llmax(center_x, center_y);

	mOuterRingAlpha = mUseInfiniteRadius ? 0.f : 1.f;
	if (mShrinkBorderTimer.getStarted())
	{
		mOuterRingAlpha = clamp_rescale(mShrinkBorderTimer.getElapsedTimeF32(),
										0.f, PIE_SHRINK_TIME, 0.f, 1.f);
		mCurRadius *= clamp_rescale(mShrinkBorderTimer.getElapsedTimeF32(),
									0.f, PIE_SHRINK_TIME,
									1.f, 1.f / PIE_SCALE_FACTOR);
	}

	gGL.pushUIMatrix();
	gGL.translateUI(center_x, center_y, 0.f);
	{
		// Main body
		LLColor4 outer_color = LLUI::sPieMenuBgColor;
		outer_color.mV[VALPHA] *= mOuterRingAlpha;
		gl_washer_2d(mCurRadius, (F32)PIE_CENTER_SIZE, steps,
					 LLUI::sPieMenuBgColor, outer_color);

		// Selected wedge
		S32 i = 0;
		for (item_list_t::iterator it = mItems.begin(), end = mItems.end();
			 it != end; ++it)
		{
			if ((*it)->getHighlight())
			{
				F32 arc_size = F_PI * 0.25f;

				F32 start_radians = ((F32)i - 0.5f) * arc_size;
				F32 end_radians = start_radians + arc_size;

				LLColor4 outer_color = LLUI::sPieMenuSelectedColor;
				outer_color.mV[VALPHA] *= mOuterRingAlpha;
				gl_washer_segment_2d(mCurRadius, (F32)PIE_CENTER_SIZE,
									 start_radians, end_radians, steps / 8,
									 LLUI::sPieMenuSelectedColor, outer_color);
			}
			++i;
		}

		LLUI::setLineWidth(LLUI::sPieMenuLineWidth);

		// Inner lines
		outer_color = LLUI::sPieMenuLineColor;
		outer_color.mV[VALPHA] *= mOuterRingAlpha;
		gl_washer_spokes_2d(mCurRadius, (F32)PIE_CENTER_SIZE, 8,
							LLUI::sPieMenuLineColor, outer_color);

		// Inner circle
		gGL.color4fv(LLUI::sPieMenuLineColor.mV);
		gl_circle_2d(0, 0, (F32)PIE_CENTER_SIZE, steps, false);

		// Outer circle
		gGL.color4fv(outer_color.mV);
		gl_circle_2d(0, 0, mCurRadius, steps, false);

		LLUI::setLineWidth(1.0f);
	}
	gGL.popUIMatrix();

	mHoverThisFrame = false;

	LLView::draw();
}

void LLPieMenu::drawBackground(LLMenuItemGL* itemp, LLColor4& color)
{
	F32 center_x = (F32)getRect().getWidth() * 0.5f;
	F32 center_y = (F32)getRect().getHeight() * 0.5f;
	S32 steps = 100;

	gGL.color4fv(color.mV);
	gGL.pushUIMatrix();
	{
		gGL.translateUI(center_x - itemp->getRect().mLeft,
					    center_y - itemp->getRect().mBottom, 0.f);

		S32 i = 0;
		for (item_list_t::iterator it = mItems.begin(), end = mItems.end();
			 it != end; ++it)
		{
			if (*it == itemp)
			{
				F32 arc_size = F_PI * 0.25f;
				F32 start_radians = i * arc_size - arc_size * 0.5f;
				F32 end_radians = start_radians + arc_size;

				LLColor4 outer_color = color;
				outer_color.mV[VALPHA] *= mOuterRingAlpha;
				gl_washer_segment_2d(mCurRadius, (F32)PIE_CENTER_SIZE,
									 start_radians, end_radians, steps / 8,
									 color, outer_color);
			}
			++i;
		}
	}
	gGL.popUIMatrix();
}

//virtual
bool LLPieMenu::append(LLMenuItemGL* item)
{
	item->setBriefItem(true);
	item->setFont(LLFontGL::getFontSansSerifSmall());
	return LLMenuGL::append(item);
}

//virtual
bool LLPieMenu::appendSeparator(const std::string&)
{
	LLMenuItemGL* separator = new LLMenuItemBlankGL();
	separator->setFont(LLFontGL::getFontSansSerifSmall());
	return append(separator);
}

bool LLPieMenu::appendPieMenu(LLPieMenu* menu)
{
	if (menu == this)
	{
		llerrs << "Cannot attach a pie menu to itself !" << llendl;
	}

	LLPieMenuBranch* item = new LLPieMenuBranch(menu->getName(),
												menu->getLabel(), menu);
	getParent()->addChild(item->getBranch());
	item->setFont(LLFontGL::getFontSansSerifSmall());

	return append(item);
}

//virtual
void LLPieMenu::arrange()
{
	constexpr S32 rect_height = 190;
	constexpr S32 rect_width = 190;

	// All divide by 6
	constexpr S32 CARD_X = 60;
	constexpr S32 DIAG_X = 48;
	constexpr S32 CARD_Y = 76;
	constexpr S32 DIAG_Y = 42;

	static const S32 ITEM_CENTER_X[] =
	{
		 CARD_X,	 DIAG_X,	0,	-DIAG_X,
		-CARD_X,	-DIAG_X,	0,	 DIAG_X
	};
	static const S32 ITEM_CENTER_Y[] =
	{
		0,	 DIAG_Y,	 CARD_Y,	 DIAG_Y,
		0,	-DIAG_Y,	-CARD_Y,	-DIAG_Y
	};

	// *TODO: Compute actual bounding rect for menu

	LLRect rect;
	// *HACK: casting away const. Should use setRect or some helper function
	// instead.
	const_cast<LLRect&>(getRect()).setOriginAndSize(getRect().mLeft,
													getRect().mBottom,
													rect_width, rect_height);

	S32 font_height = 0;
	if (mItems.size())
	{
		font_height = (*mItems.begin())->getNominalHeight();
	}

	// Place items around a circle, with item 0 at positive X, rotating
	// counter-clockwise
	S32 item_width = 0;
	S32 i = 0;
	for (item_list_t::iterator it = mItems.begin(), end = mItems.end();
		 it != end; ++it)
	{
		LLMenuItemGL* item = *it;

		item_width = item->getNominalWidth();

		// Put in the right place around a circle centered at 0,0
		rect.setCenterAndSize(ITEM_CENTER_X[i], ITEM_CENTER_Y[i],
							  item_width, font_height);

		// Correct for the actual rectangle size
		rect.translate(rect_width / 2, rect_height / 2);

		item->setRect(rect);

		// Make sure enablement is correct
		item->buildDrawLabel();
		++i;
	}
}

LLMenuItemGL* LLPieMenu::pieItemFromXY(S32 x, S32 y)
{
#if 0
	// We might have shifted this menu on draw. If so, we need to shift over
	// mouseup events until we get a hover event.
	x += mShiftHoriz;
	y += mShiftVert;
#endif

	// An arc of the pie menu is 45 degrees
	constexpr F32 ARC_DEG = 45.f;
	S32 delta_x = x - getRect().getWidth() / 2;
	S32 delta_y = y - getRect().getHeight() / 2;

	// circle safe zone in the center
	S32 dist_squared = delta_x * delta_x + delta_y * delta_y;
	if (dist_squared < PIE_CENTER_SIZE * PIE_CENTER_SIZE)
	{
		return NULL;
	}

	// Infinite radius is only used with right clicks
	S32 radius = llmax(getRect().getWidth() / 2, getRect().getHeight() / 2);
	if (!(mUseInfiniteRadius && mRightMouseDown) &&
		dist_squared > radius * radius)
	{
		return NULL;
	}

	F32 angle = RAD_TO_DEG * atan2f((F32)delta_y, (F32)delta_x);

	// Rotate marks CCW so that east = [0, ARC_DEG) instead of
	// [-ARC_DEG/2, ARC_DEG/2)
	angle += ARC_DEG * 0.5f;

	// Make sure we are only using positive angles
	if (angle < 0.f) angle += 360.f;

	S32 which = S32(angle / ARC_DEG);

	if (which >= 0 && which < (S32)mItems.size())
	{
		for (item_list_t::iterator it = mItems.begin(), end = mItems.end();
			 it != end; ++it)
		{
			if (which == 0)
			{
				return *it;
			}
			--which;
		}
	}

	return NULL;
}

S32 LLPieMenu::pieItemIndexFromXY(S32 x, S32 y)
{
	// An arc of the pie menu is 45 degrees
	constexpr F32 ARC_DEG = 45.f;

	// Correct for non-square pixels
	S32 delta_x = x - getRect().getWidth() / 2;
	S32 delta_y = y - getRect().getHeight() / 2;

	// Circle safe zone in the center
	if (delta_x * delta_x + delta_y * delta_y <
			PIE_CENTER_SIZE * PIE_CENTER_SIZE)
	{
		return -1;
	}

	F32 angle = RAD_TO_DEG * atan2f((F32)delta_y, (F32)delta_x);

	// Rotate marks CCW so that east = [0, ARC_DEG) instead of
	// [-ARC_DEG/2, ARC_DEG/2)
	angle += ARC_DEG * 0.5f;

	// Make sure we are only using positive angles
	if (angle < 0.f)
	{
		angle += 360.f;
	}

	S32 which = S32(angle / ARC_DEG);
	return which;
}

void LLPieMenu::show(S32 x, S32 y, bool mouse_down)
{
	if (!sMenuContainer) return;

	S32 width = getRect().getWidth();
	S32 height = getRect().getHeight();

	const LLRect menu_region_rect = sMenuContainer->getMenuRect();

	LLView* parent_view = getParent();
	S32 local_x, local_y;
	parent_view->screenPointToLocal(x, y, &local_x, &local_y);

	// *HACK: casting away const. Should use setRect or some helper function
	// instead.
	const_cast<LLRect&>(getRect()).setCenterAndSize(local_x, local_y,
													width, height);
	arrange();

	bool moved = false;

	// Adjust the pie rectangle to keep it on screen
	if (getRect().mLeft < menu_region_rect.mLeft)
	{
		// *HACK: casting away const. Should use setRect or some helper
		// function instead.
		const_cast<LLRect&>(getRect()).translate(menu_region_rect.mLeft -
												 getRect().mLeft, 0);
		moved = true;
	}

	if (getRect().mRight > menu_region_rect.mRight)
	{
		// *HACK: casting away const. Should use setRect or some helper
		// function instead.
		const_cast<LLRect&>(getRect()).translate(menu_region_rect.mRight -
												 getRect().mRight, 0);
		moved = true;
	}

	if (getRect().mBottom < menu_region_rect.mBottom)
	{
		// *HACK: casting away const. Should use setRect or some helper
		// function instead.
		const_cast<LLRect&>(getRect()).translate(0, menu_region_rect.mBottom -
													getRect().mBottom);
		moved = true;
	}

	if (getRect().mTop > menu_region_rect.mTop)
	{
		// *HACK: casting away const. Should use setRect or some helper
		// function instead.
		const_cast<LLRect&>(getRect()).translate(0, menu_region_rect.mTop -
													getRect().mTop);
		moved = true;
	}

	// If we had to relocate the pie menu, put the cursor in the center of its
	// rectangle
	if (moved)
	{
		LLCoordGL center;
		center.mX = (getRect().mLeft + getRect().mRight) / 2;
		center.mY = (getRect().mTop + getRect().mBottom) / 2;

		LLUI::setCursorPositionLocal(getParent(), center.mX, center.mY);
	}

	// *FIX: what happens when mouse buttons reversed?
	mRightMouseDown = mouse_down;
	mFirstMouseDown = mouse_down;
	mUseInfiniteRadius = true;
	mHoveredAnyItem = false;

	if (!mFirstMouseDown)
	{
		make_ui_sound("UISndPieMenuAppear");
	}

	LLView::setVisible(true);

	// We want all mouse events in case user does quick right click again off
	// of pie menu rectangle, to support gestural menu traversal
	gFocusMgr.setMouseCapture(this);

	if (mouse_down)
	{
		mShrinkBorderTimer.stop();
	}
	else
	{
		mShrinkBorderTimer.start();
	}
}

void LLPieMenu::hide(bool item_selected)
{
	if (!getVisible()) return;

	if (mHoverItem)
	{
		mHoverItem->setHighlight(false);
		mHoverItem = NULL;
	}

	make_ui_sound("UISndPieMenuHide");

	mFirstMouseDown = false;
	mRightMouseDown = false;
	mUseInfiniteRadius = false;
	mHoveredAnyItem = false;

	LLView::setVisible(false);

	gFocusMgr.setMouseCapture(NULL);
}

//============================================================================
// Class LLMenuBarGL
//============================================================================

static LLRegisterWidget<LLMenuBarGL> r09(LL_MENU_BAR_GL_TAG);

// Default constructor
LLMenuBarGL::LLMenuBarGL(const std::string& name)
:	LLMenuGL(name, name)
{
	mHorizontalLayout = true;
	setCanTearOff(false);
	mKeepFixedSize = true;
	mAltKeyTrigger = false;
}

LLMenuBarGL::~LLMenuBarGL()
{
	std::for_each(mAccelerators.begin(), mAccelerators.end(), DeletePointer());
	mAccelerators.clear();
}

//virtual
LLXMLNodePtr LLMenuBarGL::getXML(bool save_children) const
{
	// Sorty of hacky: reparent items to this and then back at the end of the
	// export
	LLView* orig_parent = NULL;
	item_list_t::const_iterator it;
	for (it = mItems.begin(); it != mItems.end(); ++it)
	{
		LLMenuItemGL* child = *it;
		LLMenuItemBranchGL* branch = (LLMenuItemBranchGL*)child;
		LLMenuGL* menu = branch->getBranch();
		orig_parent = menu->getParent();
		menu->updateParent((LLView*)this);
	}

	LLXMLNodePtr node = LLMenuGL::getXML();

	node->setName(LL_MENU_BAR_GL_TAG);

	for (it = mItems.begin(); it != mItems.end(); ++it)
	{
		LLMenuItemGL* child = *it;
		LLMenuItemBranchGL* branch = (LLMenuItemBranchGL*)child;
		LLMenuGL* menu = branch->getBranch();
		menu->updateParent(orig_parent);
	}

	return node;
}

LLView* LLMenuBarGL::fromXML(LLXMLNodePtr node, LLView* parent,
							 LLUICtrlFactory* factory)
{
	std::string name = LL_MENU_BAR_GL_TAG;
	node->getAttributeString("name", name);

	bool opaque = false;
	node->getAttributeBool("opaque", opaque);

	LLMenuBarGL* menubar = new LLMenuBarGL(name);

	LLHandle<LLFloater> parent_handle;
	LLFloater* floaterp = parent->asFloater();
	if (floaterp)
	{
		parent_handle = floaterp->getHandle();
	}

	// We need to have the rect early so that it is around when building the
	// menu items
	LLRect view_rect;
	createRect(node, view_rect, parent, menubar->getRequiredRect());
	menubar->setRect(view_rect);

	if (node->hasAttribute("drop_shadow"))
	{
		bool drop_shadow = false;
		node->getAttributeBool("drop_shadow", drop_shadow);
		menubar->setDropShadowed(drop_shadow);
	}

	menubar->setBackgroundVisible(opaque);
	LLColor4 color(0, 0, 0, 0);
	if (opaque && LLUICtrlFactory::getAttributeColor(node,"color", color))
	{
		menubar->setBackgroundColor(color);
	}

	LLXMLNodePtr child;
	for (child = node->getFirstChild(); child.notNull();
		 child = child->getNextSibling())
	{
		if (child->hasName("menu"))
		{
			LLMenuGL* menu =
				(LLMenuGL*)LLMenuGL::fromXML(child, parent, factory);
			// Because of lazy initialization, have to disable tear off
			// functionality and then re-enable with proper parent handle
			if (menu->getCanTearOff())
			{
				menu->setCanTearOff(false);
				menu->setCanTearOff(true, parent_handle);
			}
			menubar->appendMenu(menu);
			if (sMenuContainer)
			{
				menu->updateParent(sMenuContainer);
			}
			else
			{
				menu->updateParent(parent);
			}
		}
	}

	menubar->initFromXML(node, parent);

	bool create_jump_keys = false;
	node->getAttributeBool("create_jump_keys", create_jump_keys);
	if (create_jump_keys)
	{
		menubar->createJumpKeys();
	}

	return menubar;
}

bool LLMenuBarGL::handleAcceleratorKey(KEY key, MASK mask)
{
	bool has_higlight = getHighlightedItem() != NULL;
	if (has_higlight && mask == MASK_NONE)
	{
		// Unmodified key accelerators are ignored when navigating menu (but
		// are used as jump keys so will still work when appropriate menu is
		// up)
		return false;
	}
	bool result = LLMenuGL::handleAcceleratorKey(key, mask);
	if (result && mask & MASK_ALT)
	{
		// ALT key used to trigger hotkey, do not use as shortcut to open menu
		mAltKeyTrigger = false;
	}
#if 1
	if (result && has_higlight && sMenuContainer &&
		sMenuContainer->hasVisibleMenu())
	{
		// Close menus originating from other menu bars
		sMenuContainer->hideMenus();
	}
#endif
	return result;
}

bool LLMenuBarGL::handleKeyHere(KEY key, MASK mask)
{
	if (key == KEY_ALT && gKeyboardp && !gKeyboardp->getKeyRepeated(key) &&
		LLUI::sUseAltKeyForMenus)
	{
		mAltKeyTrigger = true;
	}
	else // if any key other than ALT hit, clear out waiting for Alt key mode
	{
		mAltKeyTrigger = false;
	}

	if (key == KEY_ESCAPE && mask == MASK_NONE)
	{
		LLMenuGL::setKeyboardMode(false);
		// If any menus are visible, this will return true, stopping further
		// processing of ESCAPE key
		return sMenuContainer && sMenuContainer->hideMenus();
	}

	// Before processing any other key, check to see if ALT key has triggered
	// menu access
	checkMenuTrigger();

	return LLMenuGL::handleKeyHere(key, mask);
}

bool LLMenuBarGL::handleJumpKey(KEY key)
{
	// Perform case-insensitive comparison
	key = toupper(key);
	navigation_key_map_t::iterator found_it = mJumpKeys.find(key);
	if (found_it != mJumpKeys.end() && found_it->second->getEnabled())
	{
		// Switch to keyboard navigation mode
		LLMenuGL::setKeyboardMode(true);

		found_it->second->setHighlight(true);
		found_it->second->doIt();
	}
	return true;
}

bool LLMenuBarGL::handleMouseDown(S32 x, S32 y, MASK mask)
{
	// Clicks on menu bar closes existing menus from other contexts but leave
	// own menu open so that we get toggle behavior
	if ((!getHighlightedItem() || !getHighlightedItem()->isActive()) &&
		sMenuContainer)
	{
		sMenuContainer->hideMenus();
	}

	return LLMenuGL::handleMouseDown(x, y, mask);
}

bool LLMenuBarGL::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
	// Clicks on menu bar closes existing menus from other contexts but leave
	// own menu open so that we get toggle behavior
	if ((!getHighlightedItem() || !getHighlightedItem()->isActive()) &&
		sMenuContainer)
	{
		sMenuContainer->hideMenus();
	}

	return LLMenuGL::handleMouseDown(x, y, mask);
}

void LLMenuBarGL::draw()
{
	LLMenuItemGL* itemp = getHighlightedItem();
	// If we are in mouse-control mode and the mouse cursor is not hovering
	// over the current highlighted menu item and it is not open, then remove
	// the highlight. This is done via a polling mechanism here, as we do not
	// receive notifications when the mouse cursor moves off of us
	if (itemp && !itemp->isOpen() && !itemp->getHover() &&
		!LLMenuGL::getKeyboardMode())
	{
		clearHoverItem();
	}

	checkMenuTrigger();

	LLMenuGL::draw();
}

void LLMenuBarGL::checkMenuTrigger()
{
	// Has the ALT key been pressed and subsequently released ?
	if (mAltKeyTrigger && gKeyboardp && !gKeyboardp->getKeyDown(KEY_ALT))
	{
		// If alt key was released quickly, treat it as a menu access key
		// otherwise it was probably an Alt-zoom or similar action
		if (gKeyboardp->getKeyElapsedFrameCount(KEY_ALT) < 2 ||
			gKeyboardp->getKeyElapsedTime(KEY_ALT) <= LLUI::sMenuAccessKeyTime)
		{
			if (getHighlightedItem())
			{
				clearHoverItem();
			}
			else if (sMenuContainer)
			{
				// Close menus originating from other menu bars
				sMenuContainer->hideMenus();
				highlightNextItem(NULL);
				LLMenuGL::setKeyboardMode(true);
			}
		}
		mAltKeyTrigger = false;
	}
}

bool LLMenuBarGL::jumpKeysActive()
{
	// Require user to be in keyboard navigation mode to activate key triggers
	// as menu bars are always visible and it is easy to leave the mouse cursor
	// over them
	return LLMenuGL::getKeyboardMode() && getHighlightedItem() &&
		   LLMenuGL::jumpKeysActive();
}

// Rearranges the child rects so they fit the shape of the menu bar.
void LLMenuBarGL::arrange()
{
	U32 pos = 0;
	LLRect rect(0, getRect().getHeight(), 0, 0);
	for (item_list_t::const_iterator it = mItems.begin(), end = mItems.end();
		 it != end; ++it)
	{
		LLMenuItemGL* item = *it;
		if (item->getVisible())
		{
			rect.mLeft = pos;
			pos += item->getNominalWidth();
			rect.mRight = pos;
			item->setRect(rect);
			item->buildDrawLabel();
		}
	}
	reshape(rect.mRight, rect.getHeight());
}

S32 LLMenuBarGL::getRightmostMenuEdge()
{
	// Find the last visible menu
	for (item_list_t::reverse_iterator rit = mItems.rbegin(),
									   rend = mItems.rend();
		 rit != rend; ++rit)
	{
		if ((*rit)->getVisible())
		{
			return (*rit)->getRect().mRight;
		}
	}
	return 0;
}

// Adds a vertical separator to this menu
bool LLMenuBarGL::appendSeparator(const std::string& separator_name)
{
	LLMenuItemGL* separator = new LLMenuItemVerticalSeparatorGL();
	return append(separator);
}

// Adds a menu: this will create a drop down menu.
bool LLMenuBarGL::appendMenu(LLMenuGL* menu)
{
	if (menu == this)
	{
		llerrs << "** Attempt to attach menu to itself. This is certainly "
			   << "a logic error." << llendl;
	}

	LLMenuItemBranchGL* branch = new LLMenuItemBranchDownGL(menu->getName(),
															menu->getLabel(),
															menu->getHandle());
	bool success = branch->addToAcceleratorList(&mAccelerators);
	success &= append(branch);
	branch->setJumpKey(branch->getJumpKey());

	return success;
}

bool LLMenuBarGL::handleHover(S32 x, S32 y, MASK mask)
{
	bool handled = false;
	LLView* active_menu = NULL;

	bool no_mouse_data = mLastMouseX == 0 && mLastMouseY == 0;
	S32 mouse_delta_x = no_mouse_data ? 0 : x - mLastMouseX;
	S32 mouse_delta_y = no_mouse_data ? 0 : y - mLastMouseY;
	mMouseVelX = (mMouseVelX / 2) + (mouse_delta_x / 2);
	mMouseVelY = (mMouseVelY / 2) + (mouse_delta_y / 2);
	mLastMouseX = x;
	mLastMouseY = y;

	// If nothing currently selected or mouse has moved since last call, pick
	// menu item via mouse otherwise let keyboard control it
	if (!getHighlightedItem() || !LLMenuGL::getKeyboardMode() ||
		abs(mMouseVelX) > 0 || abs(mMouseVelY) > 0)
	{
		// Find current active menu
		for (child_list_const_iter_t child_it = getChildList()->begin();
			 child_it != getChildList()->end(); ++child_it)
		{
			LLView* viewp = *child_it;
			if (((LLMenuItemGL*)viewp)->isOpen())
			{
				active_menu = viewp;
			}
		}

		// Check for new active menu
		for (child_list_const_iter_t child_it = getChildList()->begin();
			 child_it != getChildList()->end(); ++child_it)
		{
			LLView* viewp = *child_it;
			S32 local_x = x - viewp->getRect().mLeft;
			S32 local_y = y - viewp->getRect().mBottom;
			if (viewp->getVisible() && viewp->getEnabled() &&
				viewp->pointInView(local_x, local_y) &&
				viewp->handleHover(local_x, local_y, mask))
			{
				((LLMenuItemGL*)viewp)->setHighlight(true);
				handled = true;
				if (active_menu && active_menu != viewp)
				{
					((LLMenuItemGL*)viewp)->doIt();
				}
				LLMenuGL::setKeyboardMode(false);
			}
		}

		if (handled)
		{
			// Set hover false on inactive menus
			for (child_list_const_iter_t child_it = getChildList()->begin();
				 child_it != getChildList()->end(); ++child_it)
			{
				LLView* viewp = *child_it;
				S32 local_x = x - viewp->getRect().mLeft;
				S32 local_y = y - viewp->getRect().mBottom;
				if (!viewp->pointInView(local_x, local_y) &&
					((LLMenuItemGL*)viewp)->getHighlight())
				{
					((LLMenuItemGL*)viewp)->setHighlight(false);
				}
			}
		}
	}

	gWindowp->setCursor(UI_CURSOR_ARROW);

	return true;
}

//============================================================================
// Class LLMenuHolderGL
//============================================================================

LLMenuHolderGL::LLMenuHolderGL()
:	LLPanel("Menu Holder")
{
	setMouseOpaque(false);
	sItemActivationTimer.stop();
	mCanHide = true;
}

LLMenuHolderGL::LLMenuHolderGL(const std::string& name, const LLRect& rect,
							   bool mouse_opaque, U32 follows)
:	LLPanel(name, rect, false)
{
	setMouseOpaque(mouse_opaque);
	sItemActivationTimer.stop();
	mCanHide = true;
}

void LLMenuHolderGL::draw()
{
	LLView::draw();

	// Now draw last selected item as overlay
	LLMenuItemGL* selecteditem = (LLMenuItemGL*)sItemLastSelectedHandle.get();
	if (selecteditem && sItemActivationTimer.getStarted() &&
		sItemActivationTimer.getElapsedTimeF32() < ACTIVATE_HIGHLIGHT_TIME)
	{
		// Make sure toggle items, for example, show the proper state when
		// fading out
		selecteditem->buildDrawLabel();

		LLRect item_rect;
		selecteditem->localRectToOtherView(selecteditem->getLocalRect(),
										   &item_rect, this);

		F32 interpolant = sItemActivationTimer.getElapsedTimeF32() /
						  ACTIVATE_HIGHLIGHT_TIME;
		F32 alpha = lerp(LLMenuItemGL::getHighlightBGColor().mV[VALPHA],
						 0.f, interpolant);
		LLColor4 bg_color(LLMenuItemGL::getHighlightBGColor().mV[VRED],
						  LLMenuItemGL::getHighlightBGColor().mV[VGREEN],
						  LLMenuItemGL::getHighlightBGColor().mV[VBLUE],
						  alpha);

		LLUI::pushMatrix();
		LLMenuGL* menup = selecteditem->getMenu();
		if (menup)
		{
			LLUI::translate((F32)item_rect.mLeft, (F32)item_rect.mBottom, 0.f);
			menup->drawBackground(selecteditem, bg_color);
			selecteditem->draw();
		}
		LLUI::popMatrix();
	}
}

bool LLMenuHolderGL::handleMouseDown(S32 x, S32 y, MASK mask)
{
	bool handled = LLView::childrenHandleMouseDown(x, y, mask) != NULL;
	if (!handled)
	{
		// Clicked off of menu, hide them all
		hideMenus();
	}
	return handled;
}

bool LLMenuHolderGL::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
	bool handled = LLView::childrenHandleRightMouseDown(x, y, mask) != NULL;
	if (!handled)
	{
		// Clicked off of menu, hide them all
		hideMenus();
	}
	return handled;
}

void LLMenuHolderGL::reshape(S32 width, S32 height, bool called_from_parent)
{
	if (width != getRect().getWidth() || height != getRect().getHeight())
	{
		hideMenus();
	}
	LLView::reshape(width, height, called_from_parent);
}

bool LLMenuHolderGL::hasVisibleMenu() const
{
	for (child_list_const_iter_t child_it = getChildList()->begin();
		 child_it != getChildList()->end(); ++child_it)
	{
		LLView* viewp = *child_it;
		if (viewp->getVisible() && dynamic_cast<LLMenuBarGL*>(viewp) == NULL)
		{
			return true;
		}
	}
	return false;
}

bool LLMenuHolderGL::hideMenus()
{
	if (!mCanHide)
	{
		return false;
	}

	bool menu_visible = hasVisibleMenu();
	if (menu_visible)
	{
		LLMenuGL::setKeyboardMode(false);
		// Clicked off of menu, hide them all
		for (child_list_const_iter_t child_it = getChildList()->begin();
			 child_it != getChildList()->end(); ++child_it)
		{
			LLView* viewp = *child_it;
			// Clicks off of menu do not hide menu bar
			if (viewp && viewp->getVisible() &&
				!dynamic_cast<LLMenuBarGL*>(viewp))
			{
				viewp->setVisible(false);
			}
		}
	}

#if 0
	if (gFocusMgr.childHasKeyboardFocus(this))
	{
		gFocusMgr.setKeyboardFocus(NULL);
	}
#endif

	return menu_visible;
}

void LLMenuHolderGL::setActivatedItem(LLMenuItemGL* item)
{
	sItemLastSelectedHandle = item->getHandle();
	sItemActivationTimer.start();
}

//============================================================================
// Class LLTearOffMenu
//============================================================================

LLTearOffMenu::LLTearOffMenu(LLMenuGL* menup)
:	LLFloater(menup->getName(), LLRect(0, 100, 100, 0), menup->getLabel(),
			  false, DEFAULT_MIN_WIDTH, DEFAULT_MIN_HEIGHT, false, false)
{
	// Flag menu as being torn off
	menup->setTornOff(true);
	// Update menu layout as torn off menu (no spillover menus)
	menup->arrange();

	LLRect rect;
	menup->localRectToOtherView(LLRect(-1, menup->getRect().getHeight(),
									   menup->getRect().getWidth() + 3, 0),
								&rect, gFloaterViewp);
	// Make sure this floater is big enough for menu
	mTargetHeight = (F32)(rect.getHeight() + LLFLOATER_HEADER_SIZE + 5);
	reshape(rect.getWidth(), rect.getHeight());
	setRect(rect);

	// Attach menu to floater
	menup->setFollowsAll();
	mOldParent = menup->getParent();
	addChild(menup);
	menup->setVisible(true);
	menup->translate(-menup->getRect().mLeft + 1,
					 -menup->getRect().mBottom + 1);
	menup->setDropShadowed(false);

	mMenu = menup;

	// Highlight first item (tear off item will be disabled)
	mMenu->highlightNextItem(NULL);
}

void LLTearOffMenu::draw()
{
	mMenu->setBackgroundVisible(isBackgroundOpaque());
	mMenu->arrange();

	if (getRect().getHeight() != mTargetHeight)
	{
		// Animate towards target height
		reshape(getRect().getWidth(),
				llceil(lerp((F32)getRect().getHeight(), mTargetHeight,
							LLCriticalDamp::getInterpolant(0.05f))));
	}
	else
	{
		// When in stasis, remain big enough to hold menu contents
		mTargetHeight = (F32)(mMenu->getRect().getHeight() +
							  LLFLOATER_HEADER_SIZE + 4);
		reshape(mMenu->getRect().getWidth() + 3,
				mMenu->getRect().getHeight() + LLFLOATER_HEADER_SIZE + 5);
	}
	LLFloater::draw();
}

void LLTearOffMenu::onFocusReceived()
{
	// If nothing is highlighted, just highlight first item
	if (!mMenu->getHighlightedItem())
	{
		mMenu->highlightNextItem(NULL);
	}

	// Parent menu items get highlights so navigation logic keeps working
	LLMenuItemGL* parent_menu_item = mMenu->getParentMenuItem();
	while (parent_menu_item)
	{
		LLMenuGL* menup = parent_menu_item->getMenu();
		if (!menup || !menup->getVisible())
		{
			break;
		}
		parent_menu_item->setHighlight(true);
		parent_menu_item = menup->getParentMenuItem();
	}
	LLFloater::onFocusReceived();
}

void LLTearOffMenu::onFocusLost()
{
	// Remove highlight from parent item and our own menu
	mMenu->clearHoverItem();
	LLFloater::onFocusLost();
}

bool LLTearOffMenu::handleUnicodeChar(llwchar uni_char, bool called_from_parent)
{
	// Pass keystrokes down to menu
	return mMenu->handleUnicodeChar(uni_char, true);
}

bool LLTearOffMenu::handleKeyHere(KEY key, MASK mask)
{
	if (!mMenu->getHighlightedItem())
	{
		if (key == KEY_UP)
		{
			mMenu->highlightPrevItem(NULL);
			return true;
		}
		else if (key == KEY_DOWN)
		{
			mMenu->highlightNextItem(NULL);
			return true;
		}
	}

	// Pass keystrokes down to menu
	return mMenu->handleKey(key, mask, true);
}

void LLTearOffMenu::translate(S32 x, S32 y)
{
	if (x != 0 && y != 0)
	{
		// Hide open sub-menus by clearing current hover item
		mMenu->clearHoverItem();
	}
	LLFloater::translate(x, y);
}

//static
LLTearOffMenu* LLTearOffMenu::create(LLMenuGL* menup)
{
	LLTearOffMenu* tearoffp = new LLTearOffMenu(menup);
	// Keep onscreen
	gFloaterViewp->adjustToFitScreen(tearoffp);
	tearoffp->open();

	return tearoffp;
}

void LLTearOffMenu::onClose(bool app_quitting)
{
	removeChild(mMenu);
	mOldParent->addChild(mMenu);
	mMenu->clearHoverItem();
	mMenu->setFollowsNone();
	mMenu->setBackgroundVisible(true);
	mMenu->setVisible(false);
	mMenu->setTornOff(false);
	mMenu->setDropShadowed(true);
	destroy();
}
