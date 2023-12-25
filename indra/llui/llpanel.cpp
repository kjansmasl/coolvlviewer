/**
 * @file llpanel.cpp
 * @brief LLPanel base class
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

// Opaque view with a background and a border.  Can contain LLUICtrls.

#include "linden_common.h"

#include "llpanel.h"

#include "llalertdialog.h"
#include "llbutton.h"
#include "llcontrol.h"
#include "llcriticaldamp.h"		// Used by LLLayoutStack
#include "llkeyboard.h"
#include "llfloater.h"
#include "lliconctrl.h"
#include "lllineeditor.h"
#include "llmenugl.h"
#include "llresizebar.h"		// Used by LLLayoutStack
#include "llstl.h"				// For DeletePointer()
#include "lltextbox.h"
#include "lltimer.h"
#include "lluictrl.h"
#include "lluictrlfactory.h"
#include "llviewborder.h"

constexpr S32 RESIZE_BAR_OVERLAP = 1;
constexpr S32 RESIZE_BAR_HEIGHT = 3;

static const std::string LL_LAYOUT_PANEL_TAG = "layout_panel";

static const std::string LL_PANEL_TAG = "panel";
static LLRegisterWidget<LLPanel> r14(LL_PANEL_TAG);

void LLPanel::init()
{
	// mRectControl
	mBgColorAlpha        = LLUI::sDefaultBackgroundColor;
	mBgColorOpaque       = LLUI::sFocusBackgroundColor;
	mDefaultBtnHighlight = LLUI::sDefaultHighlightLight;
	mBgVisible = false;
	mBgOpaque = false;
	mBorder = NULL;
	mDefaultBtn = NULL;
	setIsChrome(false);	// is this a decorator to a live window or a form ?
	mLastTabGroup = 0;

	setTabStop(false);
}

LLPanel::LLPanel()
{
	init();
	setName(LL_PANEL_TAG);
}

LLPanel::LLPanel(const std::string& name)
:	LLUICtrl(name, LLRect(0, 0, 0, 0), true, NULL, NULL)
{
	init();
}

LLPanel::LLPanel(const std::string& name, const LLRect& rect, bool bordered)
:	LLUICtrl(name, rect, true, NULL, NULL)
{
	init();
	if (bordered)
	{
		addBorder();
	}
}

LLPanel::LLPanel(const std::string& name, const std::string& rect_control,
				 bool bordered)
:	LLUICtrl(name, LLUI::sConfigGroup->getRect(rect_control.c_str()), true,
			 NULL, NULL),
	mRectControl(rect_control)
{
	init();
	if (bordered)
	{
		addBorder();
	}
}

LLPanel::~LLPanel()
{
	storeRectControl();
}

// virtual
bool LLPanel::postBuild()
{
	return true;
}

void LLPanel::addBorder(LLViewBorder::EBevel border_bevel,
						LLViewBorder::EStyle border_style,
						S32 border_thickness)
{
	removeBorder();
	mBorder = new LLViewBorder("panel border",
							   LLRect(0, getRect().getHeight(),
									  getRect().getWidth(), 0),
							   border_bevel, border_style, border_thickness);
	mBorder->setSaveToXML(false);
	addChild(mBorder);
}

void LLPanel::removeBorder()
{
	delete mBorder;
	mBorder = NULL;
}

// virtual
void LLPanel::clearCtrls()
{
	LLView::ctrl_list_t ctrls = getCtrlList();
	for (LLView::ctrl_list_t::iterator ctrl_it = ctrls.begin(), end = ctrls.end();
		 ctrl_it != end; ++ctrl_it)
	{
		LLUICtrl* ctrl = *ctrl_it;
		ctrl->setFocus(false);
		ctrl->setEnabled(false);
		ctrl->clear();
	}
}

void LLPanel::setCtrlsEnabled(bool b)
{
	LLView::ctrl_list_t ctrls = getCtrlList();
	for (LLView::ctrl_list_t::iterator ctrl_it = ctrls.begin(), end = ctrls.end();
		 ctrl_it != end; ++ctrl_it)
	{
		LLUICtrl* ctrl = *ctrl_it;
		ctrl->setEnabled(b);
	}
}

void LLPanel::draw()
{
	// Draw background
	if (mBgVisible)
	{
		// RN: I do not see the point of this
		S32 top = getRect().getHeight();	// - LLPANEL_BORDER_WIDTH;
		S32 right = getRect().getWidth();	// - LLPANEL_BORDER_WIDTH;
		if (mBgOpaque)
		{
			gl_rect_2d(0, top, right, 0, mBgColorOpaque);
		}
		else
		{
			gl_rect_2d(0, top, right, 0, mBgColorAlpha);
		}
	}

	updateDefaultBtn();

	LLView::draw();
}

//virtual
void LLPanel::setAlpha(F32 alpha)
{
	mBgColorOpaque.setAlpha(alpha);
}

void LLPanel::updateDefaultBtn()
{
	// This method does not call LLView::draw() so callers will need
	// to take care of that themselves at the appropriate place in
	// their rendering sequence

	if (mDefaultBtn)
	{
		if (gFocusMgr.childHasKeyboardFocus(this) &&
			mDefaultBtn->getEnabled())
		{
			LLButton* buttonp;
			buttonp = dynamic_cast<LLButton*>(gFocusMgr.getKeyboardFocus());
			bool focus_is_child_button = buttonp && buttonp->getCommitOnReturn();
			// only enable default button when current focus is not a
			// return-capturing button
			mDefaultBtn->setBorderEnabled(!focus_is_child_button);
		}
		else
		{
			mDefaultBtn->setBorderEnabled(false);
		}
	}
}

void LLPanel::refresh()
{
	// do nothing by default
	// but is automatically called in setFocus(true)
}

void LLPanel::setDefaultBtn(LLButton* btn)
{
	if (mDefaultBtn && mDefaultBtn->getEnabled())
	{
		mDefaultBtn->setBorderEnabled(false);
	}
	mDefaultBtn = btn;
	if (mDefaultBtn)
	{
		mDefaultBtn->setBorderEnabled(true);
	}
}

void LLPanel::setDefaultBtn(const char* id)
{
	LLButton* button = NULL;
	if (id[0])
	{
		button = getChild<LLButton>(id, true, false);
	}
	setDefaultBtn(button);
}

void LLPanel::addCtrl(LLUICtrl* ctrl, S32 tab_group)
{
	mLastTabGroup = tab_group;

	LLView::addCtrl(ctrl, tab_group);
}

void LLPanel::addCtrlAtEnd(LLUICtrl* ctrl, S32 tab_group)
{
	mLastTabGroup = tab_group;

	LLView::addCtrlAtEnd(ctrl, tab_group);
}

bool LLPanel::handleKeyHere(KEY key, MASK mask)
{
	bool handled = false;

	LLUICtrl* cur_focus = gFocusMgr.getKeyboardFocusUICtrl();

	// Handle user hitting ESC to defocus
	if (key == KEY_ESCAPE && mask == MASK_NONE)
	{
		gFocusMgr.setKeyboardFocus(NULL);
		return true;
	}
	else if (mask == MASK_SHIFT && KEY_TAB == key)
	{
		// SHIFT-TAB
		if (cur_focus)
		{
			LLUICtrl* focus_root = cur_focus->findRootMostFocusRoot();
			if (focus_root)
			{
				handled = focus_root->focusPrevItem(false);
			}
		}
	}
	else if (mask == MASK_NONE && KEY_TAB == key)
	{
		// TAB
		if (cur_focus)
		{
			LLUICtrl* focus_root = cur_focus->findRootMostFocusRoot();
			if (focus_root)
			{
				handled = focus_root->focusNextItem(false);
			}
		}
	}

	// If we have a default button, click it when return is pressed, unless
	// current focus is a return-capturing button in which case *that* button
	// will handle the return key
	LLButton* focused_button = dynamic_cast<LLButton*>(cur_focus);
	if (cur_focus && !(focused_button && focused_button->getCommitOnReturn()))
	{
		// RETURN key means hit default button in this case
		if (key == KEY_RETURN && mask == MASK_NONE && mDefaultBtn != NULL &&
			mDefaultBtn->getVisible() && mDefaultBtn->getEnabled())
		{
			mDefaultBtn->onCommit();
			handled = true;
		}
	}

	if (key == KEY_RETURN && mask == MASK_NONE)
	{
		// set keyboard focus to self to trigger commitOnFocusLost behavior on
		// current ctrl
		if (cur_focus && cur_focus->acceptsTextInput())
		{
			cur_focus->onCommit();
			handled = true;
		}
	}

	return handled;
}

void LLPanel::setFocus(bool b)
{
	if (b)
	{
		if (!gFocusMgr.childHasKeyboardFocus(this))
		{
#if 0
			refresh();
#endif
			if (!focusFirstItem())
			{
				LLUICtrl::setFocus(true);
			}
			onFocusReceived();
		}
	}
	else
	{
		if (this == gFocusMgr.getKeyboardFocus())
		{
			gFocusMgr.setKeyboardFocus(NULL);
		}
		else
		{
			//RN: why is this here?
			LLView::ctrl_list_t ctrls = getCtrlList();
			for (LLView::ctrl_list_t::iterator ctrl_it = ctrls.begin(),
											   end = ctrls.end();
				 ctrl_it != end; ++ctrl_it)
			{
				LLUICtrl* ctrl = *ctrl_it;
				ctrl->setFocus(false);
			}
		}
	}
}

void LLPanel::setBorderVisible(bool b)
{
	if (mBorder)
	{
		mBorder->setVisible(b);
	}
}

// virtual
LLXMLNodePtr LLPanel::getXML(bool save_children) const
{
	LLXMLNodePtr node = LLUICtrl::getXML();

	node->setName(LL_PANEL_TAG);

	if (mBorder && mBorder->getVisible())
	{
		node->createChild("border", true)->setBoolValue(true);
	}

	if (!mRectControl.empty())
	{
		node->createChild("rect_control", true)->setStringValue(mRectControl);
	}

	if (!mLabel.empty())
	{
		node->createChild("label", true)->setStringValue(mLabel);
	}

	ui_string_map_t::const_iterator i = mUIStrings.begin();
	ui_string_map_t::const_iterator end = mUIStrings.end();
	for ( ; i != end; ++i)
	{
		LLXMLNodePtr child_node = node->createChild("string", false);
		child_node->setStringValue(i->second);
		child_node->createChild("name", true)->setStringValue(i->first);
	}

	if (save_children)
	{
		LLView::child_list_const_reverse_iter_t rit;
		for (rit = getChildList()->rbegin(); rit != getChildList()->rend();
			 ++rit)
		{
			LLView* childp = *rit;

			if (childp->getSaveToXML())
			{
				LLXMLNodePtr xml_node = childp->getXML();

				node->addChild(xml_node);
			}
		}
	}

	return node;
}

LLView* LLPanel::fromXML(LLXMLNodePtr node, LLView* parent,
						 LLUICtrlFactory *factory)
{
	std::string name = LL_PANEL_TAG;
	node->getAttributeString("name", name);

	LLPanel* panelp = factory->createFactoryPanel(name);
	// Fall back on a default panel, if there was no special factory.
	if (!panelp)
	{
		LLRect rect;
		createRect(node, rect, parent, LLRect());
		// create a new panel without a border, by default
		panelp = new LLPanel(name, rect, false);
		panelp->initPanelXML(node, parent, factory);
		// preserve panel's width and height, but override the location
		const LLRect& panelrect = panelp->getRect();
		S32 w = panelrect.getWidth();
		S32 h = panelrect.getHeight();
		rect.setLeftTopAndSize(rect.mLeft, rect.mTop, w, h);
		panelp->setRect(rect);
	}
	else
	{
		panelp->initPanelXML(node, parent, factory);
	}

	return panelp;
}

bool LLPanel::initPanelXML(LLXMLNodePtr node, LLView *parent,
						   LLUICtrlFactory *factory)
{
	std::string name = getName();
	node->getAttributeString("name", name);
	setName(name);

	setPanelParameters(node, parent);

	initChildrenXML(node, factory);

	std::string xml_filename;
	node->getAttributeString("filename", xml_filename);

	bool didPost;

	if (!xml_filename.empty())
	{
		didPost = factory->buildPanel(this, xml_filename, NULL);

		LLRect new_rect = getRect();
		// override rectangle with embedding parameters as provided
		createRect(node, new_rect, parent);
		setOrigin(new_rect.mLeft, new_rect.mBottom);
		reshape(new_rect.getWidth(), new_rect.getHeight());
		// optionally override follows flags from including nodes
		parseFollowsFlags(node);
	}
	else
	{
		didPost = false;
	}

	if (!didPost)
	{
		postBuild();
		didPost = true;
	}

	return didPost;
}

void LLPanel::initChildrenXML(LLXMLNodePtr node, LLUICtrlFactory* factory)
{
	LLXMLNodePtr child;
	for (child = node->getFirstChild(); child.notNull();
		 child = child->getNextSibling())
	{
		// look for string declarations for programmatic text
		if (child->hasName("string"))
		{
			std::string string_name;
			child->getAttributeString("name", string_name);
			if (!string_name.empty())
			{
				mUIStrings[string_name] = child->getTextContents();
			}
		}
		else
		{
			factory->createWidget(this, child);
		}
	}
}

void LLPanel::setPanelParameters(LLXMLNodePtr node, LLView* parent)
{
	/////// Rect, follows, tool_tip, enabled, visible attributes ///////
	initFromXML(node, parent);

	/////// Border attributes ///////
	bool border = mBorder != NULL;
	node->getAttributeBool("border", border);
	if (border)
	{
		LLViewBorder::EBevel bevel_style = LLViewBorder::BEVEL_OUT;
		LLViewBorder::getBevelFromAttribute(node, bevel_style);

		LLViewBorder::EStyle border_style = LLViewBorder::STYLE_LINE;
		std::string border_string;
		node->getAttributeString("border_style", border_string);
		LLStringUtil::toLower(border_string);

		if (border_string == "texture")
		{
			border_style = LLViewBorder::STYLE_TEXTURE;
		}

		S32 border_thickness = LLPANEL_BORDER_WIDTH;
		node->getAttributeS32("border_thickness", border_thickness);

		addBorder(bevel_style, border_style, border_thickness);
	}
	else
	{
		removeBorder();
	}

	/////// Background attributes ///////
	bool background_visible = mBgVisible;
	node->getAttributeBool("background_visible", background_visible);
	setBackgroundVisible(background_visible);

	bool background_opaque = mBgOpaque;
	node->getAttributeBool("background_opaque", background_opaque);
	setBackgroundOpaque(background_opaque);

	LLColor4 color;
	color = mBgColorOpaque;
	LLUICtrlFactory::getAttributeColor(node, "bg_opaque_color", color);
	setBackgroundColor(color);

	color = mBgColorAlpha;
	LLUICtrlFactory::getAttributeColor(node, "bg_alpha_color", color);
	setTransparentColor(color);

	std::string label = getLabel();
	node->getAttributeString("label", label);
	setLabel(label);
}

LLFloater* LLPanel::getParentFloater() const
{
	LLFloater* floater = NULL;

	LLView* parent = getParent();
	while (parent && !floater)
	{
		floater = parent->asFloater();
		parent = parent->getParent();
	}

	return floater;
}

std::string LLPanel::getString(const std::string& name,
							   const LLStringUtil::format_map_t& args) const
{
	LL_DEBUGS("GetStringUI") << "Requested UI string: " << name << LL_ENDL;
	ui_string_map_t::const_iterator found_it = mUIStrings.find(name);
	if (found_it != mUIStrings.end())
	{
		// make a copy as format works in place
		LLUIString formatted_string = LLUIString(found_it->second);
		formatted_string.setArgList(args);
		return formatted_string.getString();
	}
	llwarns << "Failed to find string " << name << " in panel " << getName()
			<< llendl;
	return LLStringUtil::null;
}

std::string LLPanel::getString(const std::string& name) const
{
	LL_DEBUGS("GetStringUI") << "Requested UI string: " << name << LL_ENDL;
	ui_string_map_t::const_iterator found_it = mUIStrings.find(name);
	if (found_it != mUIStrings.end())
	{
		return found_it->second;
	}
	llwarns << "Failed to find string " << name << " in panel " << getName()
			<< llendl;
	return LLStringUtil::null;
}

void LLPanel::childSetVisible(const char* id, bool visible)
{
	LLView* child = getChild<LLView>(id);
	if (child)
	{
		child->setVisible(visible);
	}
}

bool LLPanel::childIsVisible(const char* id) const
{
	LLView* child = getChild<LLView>(id);
	if (child)
	{
		return (bool)child->getVisible();
	}
	return false;
}

void LLPanel::childSetEnabled(const char* id, bool enabled)
{
	LLView* child = getChild<LLView>(id);
	if (child)
	{
		child->setEnabled(enabled);
	}
}

void LLPanel::childSetTentative(const char* id, bool tentative)
{
	LLView* child = getChild<LLView>(id);
	if (child)
	{
		child->setTentative(tentative);
	}
}

bool LLPanel::childIsEnabled(const char* id) const
{
	LLView* child = getChild<LLView>(id);
	if (child)
	{
		return (bool)child->getEnabled();
	}
	return false;
}

void LLPanel::childSetToolTip(const char* id, const std::string& msg)
{
	LLView* child = getChild<LLView>(id);
	if (child)
	{
		child->setToolTip(msg);
	}
}

void LLPanel::childSetRect(const char* id, const LLRect& rect)
{
	LLView* child = getChild<LLView>(id);
	if (child)
	{
		child->setRect(rect);
	}
}

bool LLPanel::childGetRect(const char* id, LLRect& rect) const
{
	LLView* child = getChild<LLView>(id);
	if (child)
	{
		rect = child->getRect();
		return true;
	}
	return false;
}

void LLPanel::childSetFocus(const char* id, bool focus)
{
	LLUICtrl* child = getChild<LLUICtrl>(id, true);
	if (child)
	{
		child->setFocus(focus);
	}
}

bool LLPanel::childHasFocus(const char* id)
{
	LLUICtrl* child = getChild<LLUICtrl>(id, true);
	if (child)
	{
		return child->hasFocus();
	}
	childNotFound(id);
	return false;
}

void LLPanel::childSetFocusChangedCallback(const char* id,
										   void (*cb)(LLFocusableElement*, void*),
										   void* user_data)
{
	LLUICtrl* child = getChild<LLUICtrl>(id, true);
	if (child)
	{
		child->setFocusChangedCallback(cb, user_data);
	}
}

void LLPanel::childSetCommitCallback(const char* id,
									 void (*cb)(LLUICtrl*, void*),
									 void *userdata)
{
	LLUICtrl* child = getChild<LLUICtrl>(id, true);
	if (child)
	{
		child->setCommitCallback(cb);
		child->setCallbackUserData(userdata);
	}
}

void LLPanel::childSetDoubleClickCallback(const char* id,
										  void (*cb)(void*), void* userdata)
{
	LLUICtrl* child = getChild<LLUICtrl>(id, true);
	if (child)
	{
		child->setDoubleClickCallback(cb);
		if (userdata)
		{
			child->setCallbackUserData(userdata);
		}
	}
}

void LLPanel::childSetValidate(const char* id,
							   bool (*cb)(LLUICtrl*, void*))
{
	LLUICtrl* child = getChild<LLUICtrl>(id, true);
	if (child)
	{
		child->setValidateBeforeCommit(cb);
	}
}

void LLPanel::childSetUserData(const char* id, void* userdata)
{
	LLUICtrl* child = getChild<LLUICtrl>(id, true);
	if (child)
	{
		child->setCallbackUserData(userdata);
	}
}

void LLPanel::childSetColor(const char* id, const LLColor4& color)
{
	LLUICtrl* child = getChild<LLUICtrl>(id, true);
	if (child)
	{
		child->setColor(color);
	}
}
void LLPanel::childSetAlpha(const char* id, F32 alpha)
{
	LLUICtrl* child = getChild<LLUICtrl>(id, true);
	if (child)
	{
		child->setAlpha(alpha);
	}
}

void LLPanel::childSetValue(const char* id, LLSD value)
{
	LLView* child = getChild<LLView>(id, true);
	if (child)
	{
		child->setValue(value);
	}
}

LLSD LLPanel::childGetValue(const char* id) const
{
	LLView* child = getChild<LLView>(id, true);
	if (child)
	{
		return child->getValue();
	}
	// Not found => return undefined
	return LLSD();
}

bool LLPanel::childSetTextArg(const char* id, const std::string& key,
							  const std::string& text)
{
	LLUICtrl* child = getChild<LLUICtrl>(id, true);
	if (child)
	{
		return child->setTextArg(key, text);
	}
	return false;
}

bool LLPanel::childSetLabelArg(const char* id, const std::string& key,
							   const std::string& text)
{
	LLView* child = getChild<LLView>(id);
	if (child)
	{
		return child->setLabelArg(key, text);
	}
	return false;
}

bool LLPanel::childSetToolTipArg(const char* id, const std::string& key,
								 const std::string& text)
{
	LLView* child = getChildView(id, true, false);
	if (child)
	{
		return child->setToolTipArg(key, text);
	}
	return false;
}

void LLPanel::childSetBadge(const char* id, Badge badge, bool visible)
{
	LLIconCtrl* child = getChild<LLIconCtrl>(id);
	if (child)
	{
		child->setVisible(visible);
		switch (badge)
		{
			default:
			case BADGE_OK:
				child->setImage("badge_ok.j2c");
				break;

			case BADGE_NOTE:
				child->setImage("badge_note.j2c");
				break;

			case BADGE_WARN:
				child->setImage("badge_warn.j2c");
				break;

			case BADGE_ERROR:
				child->setImage("badge_error.j2c");
		}
	}
}

void LLPanel::childSetMinValue(const char* id, LLSD min_value)
{
	LLUICtrl* child = getChild<LLUICtrl>(id, true);
	if (child)
	{
		child->setMinValue(min_value);
	}
}

void LLPanel::childSetMaxValue(const char* id, LLSD max_value)
{
	LLUICtrl* child = getChild<LLUICtrl>(id, true);
	if (child)
	{
		child->setMaxValue(max_value);
	}
}

void LLPanel::childShowTab(const char* id, const std::string& tabname,
						   bool visible)
{
	LLTabContainer* child = getChild<LLTabContainer>(id);
	if (child)
	{
		child->selectTabByName(tabname);
	}
}

LLPanel *LLPanel::childGetVisibleTab(const char* id) const
{
	LLTabContainer* child = getChild<LLTabContainer>(id);
	if (child)
	{
		return child->getCurrentPanel();
	}
	return NULL;
}

void LLPanel::childSetTabChangeCallback(const char* id,
										const std::string& tabname,
										void (*on_tab_clicked)(void*, bool),
										void *userdata,
										void (*on_precommit)(void*, bool))
{
	LLTabContainer* child = getChild<LLTabContainer>(id);
	if (child)
	{
		LLPanel* panel = child->getPanelByName(tabname);
		if (panel)
		{
			child->setTabChangeCallback(panel, on_tab_clicked);
			child->setTabUserData(panel, userdata);
			if (on_precommit)
			{
				child->setTabPrecommitChangeCallback(panel, on_precommit);
			}
		}
	}
}

void LLPanel::childSetKeystrokeCallback(const char* id,
										void (*keystroke_callback)(LLLineEditor* caller, void* user_data),
										void* user_data)
{
	LLLineEditor* child = getChild<LLLineEditor>(id);
	if (child)
	{
		child->setKeystrokeCallback(keystroke_callback);
		if (user_data)
		{
			child->setCallbackUserData(user_data);
		}
	}
}

void LLPanel::childSetPrevalidate(const char* id,
								  bool (*func)(const LLWString &))
{
	LLLineEditor* child = getChild<LLLineEditor>(id);
	if (child)
	{
		child->setPrevalidate(func);
	}
}

void LLPanel::childSetWrappedText(const char* id,
								  const std::string& text, bool visible)
{
	LLTextBox* child = getChild<LLTextBox>(id);
	if (child)
	{
		child->setVisible(visible);
		child->setWrappedText(text);
	}
}

void LLPanel::childSetAction(const char* id, void(*function)(void*),
							 void* value)
{
	LLButton* button = getChild<LLButton>(id);
	if (button)
	{
		button->setClickedCallback(function, value);
	}
}

void LLPanel::childSetActionTextbox(const char* id,
									void(*function)(void*), void* value)
{
	LLTextBox* textbox = getChild<LLTextBox>(id);
	if (textbox)
	{
		textbox->setClickedCallback(function, value);
	}
}

void LLPanel::childSetControlName(const char* id, const char* control_name)
{
	LLView* view = getChild<LLView>(id);
	if (view)
	{
		view->setControlName(control_name, NULL);
	}
}

//virtual
LLView* LLPanel::getChildView(const char* name, bool recurse,
							  bool create_if_missing) const
{
	// Just get child, do not try to create a dummy one
	LLView* view = LLUICtrl::getChildView(name, recurse, false);
	if (!view && !recurse)
	{
		childNotFound(name);
	}
	if (!view && create_if_missing)
	{
		view = createDummyWidget<LLView>(name);
	}
	return view;
}

void LLPanel::childNotFound(const char* id) const
{
	if (mExpectedMembers.find(id) == mExpectedMembers.end())
	{
		mNewExpectedMembers.emplace(id);
	}
}

void LLPanel::childDisplayNotFound()
{
	if (mNewExpectedMembers.empty())
	{
		return;
	}
	std::string msg;
	expected_members_list_t::iterator itor;
	for (itor = mNewExpectedMembers.begin(); itor != mNewExpectedMembers.end();
		 ++itor)
	{
		msg.append(*itor);
		msg.append("\n");
		mExpectedMembers.emplace(*itor);
	}
	mNewExpectedMembers.clear();
	LLSD args;
	args["CONTROLS"] = msg;
	gNotifications.add("FloaterNotFound", args);
}

void LLPanel::storeRectControl()
{
	if (!mRectControl.empty())
	{
		LLUI::sConfigGroup->setRect(mRectControl.c_str(), getRect());
	}
}

//
// LLLayoutStack
//
struct LLLayoutStack::LLEmbeddedPanel
{
	LLEmbeddedPanel(LLPanel* panelp, eLayoutOrientation orientation,
					S32 min_width, S32 min_height,
					bool auto_resize, bool user_resize)
	:	mPanel(panelp),
		mMinWidth(min_width),
		mMinHeight(min_height),
		mAutoResize(auto_resize),
		mUserResize(user_resize),
		mOrientation(orientation),
		mCollapsed(false),
		mCollapseAmt(0.f),
		mVisibleAmt(1.f)	// default to fully visible
	{
		LLResizeBar::Side side;
		S32 min_dim;
		if (orientation == HORIZONTAL)
		{
			side = LLResizeBar::RIGHT;
			min_dim = mMinHeight;
		}
		else
		{
			side = LLResizeBar::BOTTOM;
			min_dim = mMinWidth;
		}
		mResizeBar = new LLResizeBar("resizer", mPanel, LLRect(), min_dim,
									 S32_MAX, side);
		mResizeBar->setEnableSnapping(false);
		// Panels initialized as hidden should not start out partially visible
		if (!mPanel->getVisible())
		{
			mVisibleAmt = 0.f;
		}
	}

	~LLEmbeddedPanel()
	{
		// Probably not necessary, but...
		delete mResizeBar;
		mResizeBar = NULL;
	}

	F32 getCollapseFactor()
	{
		F32 collapse_amt;
		if (mOrientation == HORIZONTAL)
		{
			collapse_amt =
				clamp_rescale(mCollapseAmt, 0.f, 1.f, 1.f,
							  (F32)mMinWidth /
							  (F32)llmax(1, mPanel->getRect().getWidth()));
		}
		else
		{
			collapse_amt =
				clamp_rescale(mCollapseAmt, 0.f, 1.f, 1.f,
							  llmin(1.f, (F32)mMinHeight /
							  (F32)llmax(1, mPanel->getRect().getHeight())));
		}
		return mVisibleAmt * collapse_amt;
	}

	LLPanel*			mPanel;
	LLResizeBar*		mResizeBar;
	S32					mMinWidth;
	S32					mMinHeight;
	F32					mVisibleAmt;
	F32					mCollapseAmt;
	eLayoutOrientation	mOrientation;
	bool				mAutoResize;
	bool				mUserResize;
	bool				mCollapsed;
};

static const std::string LL_LAYOUT_STACK_TAG = "layout_stack";
static LLRegisterWidget<LLLayoutStack> r15(LL_LAYOUT_STACK_TAG);

LLLayoutStack::LLLayoutStack(eLayoutOrientation orientation)
:	mOrientation(orientation),
	mMinWidth(0),
	mMinHeight(0),
	mPanelSpacing(RESIZE_BAR_HEIGHT)
{
}

//virtual
LLLayoutStack::~LLLayoutStack()
{
	std::for_each(mPanels.begin(), mPanels.end(), DeletePointer());
	mPanels.clear();
}

//virtual
void LLLayoutStack::draw()
{
	updateLayout();

	for (e_panel_list_t::iterator panel_it = mPanels.begin(),
								  end = mPanels.end();
		 panel_it != end; ++panel_it)
	{
		LLPanel* panelp = (*panel_it)->mPanel;
		if (!panelp) continue;	// Paranoia

		// Clip to layout rectangle, not bounding rectangle
		LLRect clip_rect = panelp->getRect();

		// Scale clipping rectangle by visible amount
		if (mOrientation == HORIZONTAL)
		{
			clip_rect.mRight = clip_rect.mLeft +
							   ll_roundp((F32)clip_rect.getWidth() *
										 (*panel_it)->getCollapseFactor());
		}
		else
		{
			clip_rect.mBottom = clip_rect.mTop -
								ll_roundp((F32)clip_rect.getHeight() *
										  (*panel_it)->getCollapseFactor());
		}

		LLLocalClipRect clip(clip_rect);
		// Only force drawing invisible children if visible amount is non-zero
		drawChild(panelp, 0, 0, !clip_rect.isEmpty());
	}
}

void LLLayoutStack::deleteAllChildren()
{
	mPanels.clear();
	LLView::deleteAllChildren();
	mMinWidth = mMinHeight = 0;
}

void LLLayoutStack::removeCtrl(LLUICtrl* ctrl)
{
	LLEmbeddedPanel* embedded_panelp = findEmbeddedPanel((LLPanel*)ctrl);
	if (embedded_panelp)
	{
		mPanels.erase(std::find(mPanels.begin(), mPanels.end(),
					  embedded_panelp));
		LLView::removeChild(ctrl);
	}
	else
	{
		LLView::removeCtrl(ctrl);
	}

	// Need to update resizebars
	calcMinExtents();
}

LLXMLNodePtr LLLayoutStack::getXML(bool save_children) const
{
	LLXMLNodePtr node = LLView::getXML();
	node->setName(LL_LAYOUT_STACK_TAG);

	if (mOrientation == HORIZONTAL)
	{
		node->createChild("orientation", true)->setStringValue("horizontal");
	}
	else
	{
		node->createChild("orientation", true)->setStringValue("vertical");
	}

	if (save_children)
	{
		LLView::child_list_const_reverse_iter_t rit;
		for (rit = getChildList()->rbegin(); rit != getChildList()->rend();
			 ++rit)
		{
			LLView* childp = *rit;

			if (childp->getSaveToXML())
			{
				LLXMLNodePtr xml_node = childp->getXML();

				if (xml_node->hasName(LL_PANEL_TAG))
				{
					xml_node->setName(LL_LAYOUT_PANEL_TAG);
				}

				node->addChild(xml_node);
			}
		}
	}

	return node;
}

//static
LLView* LLLayoutStack::fromXML(LLXMLNodePtr node, LLView* parent,
							   LLUICtrlFactory* factory)
{
	std::string orientation_string("vertical");
	node->getAttributeString("orientation", orientation_string);

	eLayoutOrientation orientation = VERTICAL;

	if (orientation_string == "horizontal")
	{
		orientation = HORIZONTAL;
	}
	else if (orientation_string == "vertical")
	{
		orientation = VERTICAL;
	}
	else
	{
		llwarns << "Unknown orientation " << orientation_string
				<< ", using vertical" << llendl;
	}

	LLLayoutStack* layout_stackp = new LLLayoutStack(orientation);

	node->getAttributeS32("border_size", layout_stackp->mPanelSpacing);
	// don't allow negative spacing values
	layout_stackp->mPanelSpacing = llmax(layout_stackp->mPanelSpacing, 0);

	std::string name = "stack";
	node->getAttributeString("name", name);

	layout_stackp->setName(name);
	layout_stackp->initFromXML(node, parent);

	LLXMLNodePtr child;
	for (child = node->getFirstChild(); child.notNull();
		 child = child->getNextSibling())
	{
		S32 min_width = 0;
		S32 min_height = 0;
		bool auto_resize = true;

		child->getAttributeS32("min_width", min_width);
		child->getAttributeS32("min_height", min_height);
		child->getAttributeBool("auto_resize", auto_resize);

		if (child->hasName("layout_panel"))
		{
			bool user_resize = true;
			child->getAttributeBool("user_resize", user_resize);
			LLPanel* panelp = (LLPanel*)LLPanel::fromXML(child, layout_stackp,
														 factory);
			if (panelp)
			{
				panelp->setFollowsNone();
				layout_stackp->addPanel(panelp, min_width, min_height,
										auto_resize, user_resize);
			}
		}
		else
		{
			bool user_resize = false;
			child->getAttributeBool("user_resize", user_resize);

			LLPanel* panelp = new LLPanel("auto_panel");
			LLView* new_child = factory->createWidget(panelp, child);
			if (new_child)
			{
				// put child in new embedded panel
				layout_stackp->addPanel(panelp, min_width, min_height,
										auto_resize, user_resize);
				// Resize panel to contain widget and move widget to be
				// contained in panel
				panelp->setRect(new_child->getRect());
				new_child->setOrigin(0, 0);
			}
			else
			{
				panelp->die();
			}
		}
	}
	layout_stackp->updateLayout();

	return layout_stackp;
}

S32 LLLayoutStack::getDefaultHeight(S32 cur_height)
{
	// If we are spanning our children (crude upward propagation of size) then
	// do not enforce our size on our children
	if (mOrientation == HORIZONTAL)
	{
		cur_height = llmax(mMinHeight, getRect().getHeight());
	}

	return cur_height;
}

S32 LLLayoutStack::getDefaultWidth(S32 cur_width)
{
	// If we are spanning our children (crude upward propagation of size) then do
	// not enforce our size on our children
	if (mOrientation == VERTICAL)
	{
		cur_width = llmax(mMinWidth, getRect().getWidth());
	}

	return cur_width;
}

void LLLayoutStack::addPanel(LLPanel* panel, S32 min_width, S32 min_height,
							 bool auto_resize, bool user_resize,
							 EAnimate animate, S32 index)
{
	// Panel starts off invisible (collapsed)
	if (animate == ANIMATE)
	{
		panel->setVisible(false);
	}
	LLEmbeddedPanel* embedded_panel = new LLEmbeddedPanel(panel, mOrientation,
														  min_width,
														  min_height,
														  auto_resize,
														  user_resize);

	mPanels.insert(mPanels.begin() + llclamp(index, 0, (S32)mPanels.size()),
				   embedded_panel);

	addChild(panel);
	addChild(embedded_panel->mResizeBar);

	// Bring all resize bars to the front so that they are clickable even over
	// the panels with a bit of overlap
	for (e_panel_list_t::iterator panel_it = mPanels.begin(),
								  end = mPanels.end();
		 panel_it != end; ++panel_it)
	{
		LLResizeBar* resize_barp = (*panel_it)->mResizeBar;
		sendChildToFront(resize_barp);
	}

	// Start expanding panel animation
	if (animate == ANIMATE)
	{
		panel->setVisible(true);
	}
}

void LLLayoutStack::removePanel(LLPanel* panel)
{
	removeChild(panel);
}

void LLLayoutStack::collapsePanel(LLPanel* panel, bool collapsed)
{
	LLEmbeddedPanel* panel_container = findEmbeddedPanel(panel);
	if (panel_container)
	{
		panel_container->mCollapsed = collapsed;
	}
}

void LLLayoutStack::updateLayout(bool force_resize)
{
	calcMinExtents();

	// Calculate current extents
	S32 total_width = 0;
	S32 total_height = 0;

	constexpr F32 ANIM_OPEN_TIME = 0.02f;
	F32 open_interpolant = LLCriticalDamp::getInterpolant(ANIM_OPEN_TIME);
	constexpr F32 ANIM_CLOSE_TIME = 0.03f;
	F32 close_interpolant = LLCriticalDamp::getInterpolant(ANIM_CLOSE_TIME);

	for (e_panel_list_t::iterator panel_it = mPanels.begin(),
								  end = mPanels.end();
		 panel_it != end; ++panel_it)
	{
		LLPanel* panelp = (*panel_it)->mPanel;
		if (!panelp) continue;	// Paranoia

		if (panelp->getVisible())
		{
			(*panel_it)->mVisibleAmt = lerp((*panel_it)->mVisibleAmt, 1.f,
											open_interpolant);
			if ((*panel_it)->mVisibleAmt > 0.99f)
			{
				(*panel_it)->mVisibleAmt = 1.f;
			}
		}
		else // Not visible
		{
			(*panel_it)->mVisibleAmt = lerp((*panel_it)->mVisibleAmt, 0.f,
											close_interpolant);
			if ((*panel_it)->mVisibleAmt < 0.001f)
			{
				(*panel_it)->mVisibleAmt = 0.f;
			}
		}

		if ((*panel_it)->mCollapsed)
		{
			(*panel_it)->mCollapseAmt =	lerp((*panel_it)->mCollapseAmt, 1.f,
											 close_interpolant);
		}
		else
		{
			(*panel_it)->mCollapseAmt = lerp((*panel_it)->mCollapseAmt, 0.f,
											 close_interpolant);
		}

		if (mOrientation == HORIZONTAL)
		{
			S32 min_width = (*panel_it)->mMinWidth;
			// Enforce minimize size constraint by default
			if (panelp->getRect().getWidth() < min_width)
			{
				panelp->reshape(min_width, panelp->getRect().getHeight());
			}
        	total_width += ll_roundp(panelp->getRect().getWidth() *
									 (*panel_it)->getCollapseFactor());
        	// Want n-1 panel gaps for n panels
			if (panel_it != mPanels.begin())
			{
				total_width += mPanelSpacing;
			}
		}
		else // VERTICAL
		{
			S32 min_height = (*panel_it)->mMinHeight;
			// Enforce minimize size constraint by default
			if (panelp->getRect().getHeight() < min_height)
			{
				panelp->reshape(panelp->getRect().getWidth(),
								min_height);
			}
			total_height += ll_roundp(panelp->getRect().getHeight() *
									  (*panel_it)->getCollapseFactor());
			if (panel_it != mPanels.begin())
			{
				total_height += mPanelSpacing;
			}
		}
	}

	S32 num_resizable_panels = 0;
	S32 shrink_headroom_available = 0;
	S32 shrink_headroom_total = 0;
	for (e_panel_list_t::iterator panel_it = mPanels.begin(),
								  end = mPanels.end();
		 panel_it != end; ++panel_it)
	{
		// Panels that are not fully visible do not count towards shrink
		// headroom
		if ((*panel_it)->getCollapseFactor() < 1.f)
		{
			continue;
		}

		LLPanel* panelp = (*panel_it)->mPanel;
		if (!panelp) continue;	// Paranoia

		S32 min_width = (*panel_it)->mMinWidth;
		S32 min_height = (*panel_it)->mMinHeight;

		// If currently resizing a panel or the panel is flagged as not
		// automatically resizing only track total available headroom, but do
		// not use it for automatic resize logic
		if ((*panel_it)->mResizeBar->hasMouseCapture() ||
			(!(*panel_it)->mAutoResize && !force_resize))
		{
			if (mOrientation == HORIZONTAL)
			{
				shrink_headroom_total += panelp->getRect().getWidth() -
										 min_width;
			}
			else // VERTICAL
			{
				shrink_headroom_total += panelp->getRect().getHeight() -
										 min_height;
			}
		}
		else
		{
			++num_resizable_panels;
			if (mOrientation == HORIZONTAL)
			{
				shrink_headroom_available += panelp->getRect().getWidth() -
											 min_width;
				shrink_headroom_total += panelp->getRect().getWidth() -
										 min_width;
			}
			else // VERTICAL
			{
				shrink_headroom_available += panelp->getRect().getHeight() -
											 min_height;
				shrink_headroom_total += panelp->getRect().getHeight() -
										 min_height;
			}
		}
	}

	// Calculate how many pixels need to be distributed among layout panels
	// positive means panels need to grow, negative means shrink
	S32 pixels_to_distribute;
	if (mOrientation == HORIZONTAL)
	{
		pixels_to_distribute = getRect().getWidth() - total_width;
	}
	else // VERTICAL
	{
		pixels_to_distribute = getRect().getHeight() - total_height;
	}

	// Now we distribute the pixels...
	S32 cur_x = 0;
	S32 cur_y = getRect().getHeight();

	for (e_panel_list_t::iterator panel_it = mPanels.begin(),
								  end = mPanels.end();
		 panel_it != end; ++panel_it)
	{
		LLPanel* panelp = (*panel_it)->mPanel;
		if (!panelp) continue;	// Paranoia

		S32 min_width = (*panel_it)->mMinWidth;
		S32 min_height = (*panel_it)->mMinHeight;
		S32 cur_width = panelp->getRect().getWidth();
		S32 cur_height = panelp->getRect().getHeight();
		S32 new_width = llmax(min_width, cur_width);
		S32 new_height = llmax(min_height, cur_height);

		S32 delta_size = 0;

		// If panel can automatically resize (not animating, and resize flag
		// set)...
		if ((*panel_it)->getCollapseFactor() == 1.f &&
			(force_resize || (*panel_it)->mAutoResize) &&
			!(*panel_it)->mResizeBar->hasMouseCapture())
		{
			if (mOrientation == HORIZONTAL)
			{
				if (pixels_to_distribute < 0)	// If we are shrinking
				{
					// Shrink proportionally to amount over minimum so we can
					// do this in one pass
					delta_size = 0;
					if (shrink_headroom_available > 0)
					{
						delta_size =
							ll_roundp((F32)pixels_to_distribute *
									  ((F32)(cur_width - min_width) /
									   (F32)shrink_headroom_available));
					}
					shrink_headroom_available -= cur_width - min_width;
				}
				else	// Grow all elements equally
				{
					delta_size = ll_roundp((F32)pixels_to_distribute /
										   (F32)num_resizable_panels);
					--num_resizable_panels;
				}
				pixels_to_distribute -= delta_size;
				new_width = llmax(min_width, cur_width + delta_size);
			}
			else
			{
				new_width = getDefaultWidth(new_width);
			}

			if (mOrientation == VERTICAL)
			{
				if (pixels_to_distribute < 0)
				{
					// Shrink proportionally to amount over minimum so we can
					// do this in one pass
					delta_size = shrink_headroom_available > 0 ?
									ll_roundp((F32)pixels_to_distribute *
											  ((F32)(cur_height - min_height) /
											   (F32)shrink_headroom_available))
															   : 0;
					shrink_headroom_available -= cur_height - min_height;
				}
				else
				{
					delta_size = ll_roundp((F32)pixels_to_distribute /
										   (F32)num_resizable_panels);
					num_resizable_panels--;
				}
				pixels_to_distribute -= delta_size;
				new_height = llmax(min_height, cur_height + delta_size);
			}
			else
			{
				new_height = getDefaultHeight(new_height);
			}
		}
		else
		{
			if (mOrientation == HORIZONTAL)
			{
				new_height = getDefaultHeight(new_height);
			}
			else // VERTICAL
			{
				new_width = getDefaultWidth(new_width);
			}
		}

		// Adjust running headroom count based on new sizes
		shrink_headroom_total += delta_size;

		panelp->reshape(new_width, new_height);
		panelp->setOrigin(cur_x, cur_y - new_height);

		LLRect panel_rect = panelp->getRect();
		LLRect resize_bar_rect = panel_rect;
		if (mOrientation == HORIZONTAL)
		{
			resize_bar_rect.mLeft = panel_rect.mRight - RESIZE_BAR_OVERLAP;
			resize_bar_rect.mRight = panel_rect.mRight + mPanelSpacing + RESIZE_BAR_OVERLAP;
		}
		else
		{
			resize_bar_rect.mTop = panel_rect.mBottom + RESIZE_BAR_OVERLAP;
			resize_bar_rect.mBottom = panel_rect.mBottom - mPanelSpacing - RESIZE_BAR_OVERLAP;
		}
		(*panel_it)->mResizeBar->setRect(resize_bar_rect);

		if (mOrientation == HORIZONTAL)
		{
			cur_x += ll_roundp(new_width * (*panel_it)->getCollapseFactor()) +
					 mPanelSpacing;
		}
		else // VERTICAL
		{
			cur_y -= ll_roundp(new_height * (*panel_it)->getCollapseFactor()) +
					 mPanelSpacing;
		}
	}

	// Update resize bars with new limits
	LLResizeBar* last_resize_bar = NULL;
	for (e_panel_list_t::iterator panel_it = mPanels.begin(),
								  end = mPanels.end();
		 panel_it != end; ++panel_it)
	{
		LLPanel* panelp = (*panel_it)->mPanel;
		if (!panelp) continue;	// Paranoia

		if (mOrientation == HORIZONTAL)
		{
			S32 min_width = (*panel_it)->mMinWidth;
			(*panel_it)->mResizeBar->setResizeLimits(min_width,
													 min_width +
													 shrink_headroom_total);
		}
		else // VERTICAL
		{
			S32 min_height = (*panel_it)->mMinHeight;
			(*panel_it)->mResizeBar->setResizeLimits(min_height,
													 min_height +
													 shrink_headroom_total);
		}

		// Toggle resize bars based on panel visibility, resizability, etc
		bool resize_bar_enabled = panelp->getVisible() &&
								  (*panel_it)->mUserResize;
		(*panel_it)->mResizeBar->setVisible(resize_bar_enabled);

		if (resize_bar_enabled)
		{
			last_resize_bar = (*panel_it)->mResizeBar;
		}
	}

	// Hide last resize bar as there is nothing past it; resize bars need to be
	// in between two resizable panels.
	if (last_resize_bar)
	{
		last_resize_bar->setVisible(false);
	}

	// Not enough room to fit existing contents
	if (!force_resize &&
		// layout did not complete by reaching target position
		((mOrientation == VERTICAL && cur_y != -mPanelSpacing) ||
		 (mOrientation == HORIZONTAL &&
		  cur_x != getRect().getWidth() + mPanelSpacing)))
	{
		// Do another layout pass with all stacked elements contributing even
		// those that don't usually resize
		updateLayout(true);
	}
}

LLLayoutStack::LLEmbeddedPanel* LLLayoutStack::findEmbeddedPanel(LLPanel* panelp) const
{
	if (!panelp) return NULL;

	for (e_panel_list_t::const_iterator panel_it = mPanels.begin(),
										end = mPanels.end();
		 panel_it != end; ++panel_it)
	{
		if ((*panel_it)->mPanel == panelp)
		{
			return *panel_it;
		}
	}
	return NULL;
}

void LLLayoutStack::calcMinExtents()
{
	mMinWidth = mMinHeight = 0;

	for (e_panel_list_t::iterator panel_it = mPanels.begin(),
								  end = mPanels.end();
		 panel_it != end; ++panel_it)
	{
		if (mOrientation == HORIZONTAL)
		{
			mMinHeight = llmax(mMinHeight, (*panel_it)->mMinHeight);
            mMinWidth += (*panel_it)->mMinWidth;
			if (panel_it != mPanels.begin())
			{
				mMinWidth += mPanelSpacing;
			}
		}
		else // VERTICAL
		{
	        mMinWidth = llmax(mMinWidth, (*panel_it)->mMinWidth);
			mMinHeight += (*panel_it)->mMinHeight;
			if (panel_it != mPanels.begin())
			{
				mMinHeight += mPanelSpacing;
			}
		}
	}
}
