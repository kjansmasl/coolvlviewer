/**
 * @file llpanel.h
 * @author James Cook, Tom Yedwab
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

#ifndef LL_LLPANEL_H
#define LL_LLPANEL_H

#include "llbutton.h"
#include "llcallbackmap.h"
#include "lllineeditor.h"
#include "lluictrl.h"
#include "lluistring.h"
#include "llviewborder.h"
#include "llcolor4.h"

constexpr S32 LLPANEL_BORDER_WIDTH = 1;
constexpr bool BORDER_YES = true;
constexpr bool BORDER_NO = false;

class LLFloater;

// General purpose concrete view base class, transparent or opaque, with or
// without border, can contain LLUICtrls.

class LLPanel : public LLUICtrl
{
protected:
	LOG_CLASS(LLPanel);

public:
	// Minimal constructor for data-driven initialization
	LLPanel();
	LLPanel(const std::string& name);

	// Position and size not saved
	LLPanel(const std::string& name, const LLRect& rect, bool bordered = true);

	// Position and size are saved to rect_control
	LLPanel(const std::string& name, const std::string& rect_control,
			bool bordered = true);

	~LLPanel() override;

	LLXMLNodePtr getXML(bool save_children = true) const override;

	// LLView interface

	LL_INLINE LLPanel* asPanel() override					{ return this; }

	void draw() override;
	bool handleKeyHere(KEY key, MASK mask) override;

	// Override to set not found list:
	LLView* getChildView(const char* name, bool recurse = true,
						 bool create_if_missing = true) const override;

	// LLFocusableElement interface
	void setFocus(bool b) override;

	// LLUICtrl interface
	void setAlpha(F32 alpha) override;

	// New virtuals

	virtual void refresh();		// Called in setFocus()
	virtual bool postBuild();

	// Overridden in LLPanelObject and LLPanelVolume
	virtual void clearCtrls();

	// Border controls
	void addBorder(LLViewBorder::EBevel bevel = LLViewBorder::BEVEL_OUT,
				   LLViewBorder::EStyle style = LLViewBorder::STYLE_LINE,
				   S32 border_thickness = LLPANEL_BORDER_WIDTH);
	void removeBorder();
	LL_INLINE bool hasBorder() const						{ return mBorder != NULL; }
	void setBorderVisible(bool b);

	LL_INLINE void setBackgroundColor(const LLColor4& c)	{ mBgColorOpaque = c; }
	LL_INLINE const LLColor4& getBackgroundColor() const	{ return mBgColorOpaque; }
	LL_INLINE void setTransparentColor(const LLColor4& c)	{ mBgColorAlpha = c; }
	LL_INLINE const LLColor4& getTransparentColor() const	{ return mBgColorAlpha; }
	LL_INLINE void setBackgroundVisible(bool b)				{ mBgVisible = b; }
	LL_INLINE bool isBackgroundVisible() const				{ return mBgVisible; }
	LL_INLINE void setBackgroundOpaque(bool b)				{ mBgOpaque = b; }
	LL_INLINE bool isBackgroundOpaque() const				{ return mBgOpaque; }
	void setDefaultBtn(LLButton* btn = NULL);
	void setDefaultBtn(const char* id);
	void updateDefaultBtn();
	LL_INLINE void setLabel(const std::string& label)		{ mLabel = label; }
	LL_INLINE std::string getLabel() const					{ return mLabel; }

	LL_INLINE void setRectControl(const std::string& rc)	{ mRectControl.assign(rc); }
	LL_INLINE const std::string& getRectControl() const		{ return mRectControl; }
	void storeRectControl();

	void setCtrlsEnabled(bool b);

	LL_INLINE LLHandle<LLPanel> getHandle() const			{ return getDerivedHandle<LLPanel>(); }

	LL_INLINE S32 getLastTabGroup() const					{ return mLastTabGroup; }

	const LLCallbackMap::map_t& getFactoryMap() const		{ return mFactoryMap; }

	bool initPanelXML(LLXMLNodePtr node, LLView* parent,
					  LLUICtrlFactory* factory);
	void initChildrenXML(LLXMLNodePtr node, LLUICtrlFactory* factory);
	void setPanelParameters(LLXMLNodePtr node, LLView* parentp);

	LLFloater* getParentFloater() const;

	std::string getString(const std::string& name,
						  const LLStringUtil::format_map_t& args) const;
	std::string getString(const std::string& name) const;

	// ** Wrappers for setting child properties by name ** -TomY

	// LLView
	void childSetVisible(const char* name, bool visible);
	LL_INLINE void childShow(const char* name)				{ childSetVisible(name, true); }
	LL_INLINE void childHide(const char* name)				{ childSetVisible(name, false); }
	bool childIsVisible(const char* id) const;
	void childSetTentative(const char* name, bool tentative);

	void childSetEnabled(const char* name, bool enabled);
	LL_INLINE void childEnable(const char* name)			{ childSetEnabled(name, true); }
	LL_INLINE void childDisable(const char* name)			{ childSetEnabled(name, false); }
	bool childIsEnabled(const char* id) const;

	void childSetToolTip(const char* id, const std::string& msg);
	void childSetRect(const char* id, const LLRect& rect);
	bool childGetRect(const char* id, LLRect& rect) const;

	void childSetFocus(const char* id, bool focus = true);
	bool childHasFocus(const char* id);
	void childSetFocusChangedCallback(const char* id,
									  void (*cb)(LLFocusableElement*, void*),
									  void* user_data = NULL);

	void childSetCommitCallback(const char* id,
								void (*cb)(LLUICtrl*, void*),
								void* userdata = NULL);
	void childSetDoubleClickCallback(const char* id, void (*cb)(void*),
									 void* userdata = NULL);
	void childSetValidate(const char* id, bool (*cb)(LLUICtrl*, void*));
	void childSetUserData(const char* id, void* userdata);

	void childSetColor(const char* id, const LLColor4& color);
	void childSetAlpha(const char* id, F32 alpha);

	// This is the magic bullet for data-driven UI
	void childSetValue(const char* id, LLSD value);
	LLSD childGetValue(const char* id) const;

	// For setting text / label replacement params, e.g. "Hello [NAME]"
	// Not implemented for all types, defaults to noop, return false if not
	// applicaple
	bool childSetTextArg(const char* id, const std::string& key,
						 const std::string& text);
	bool childSetLabelArg(const char* id, const std::string& key,
						  const std::string& text);
	bool childSetToolTipArg(const char* id, const std::string& key,
							const std::string& text);

	// LLIconCtrl
	enum Badge { BADGE_OK, BADGE_NOTE, BADGE_WARN, BADGE_ERROR };
	void childSetBadge(const char* id, Badge b, bool visible = true);

	// LLSlider / LLMultiSlider / LLSpinCtrl
	void childSetMinValue(const char* id, LLSD min_value);
	void childSetMaxValue(const char* id, LLSD max_value);

	// LLTabContainer
	void childShowTab(const char* id, const std::string& tabname,
					  bool visible = true);
	LLPanel* childGetVisibleTab(const char* id) const;
	void childSetTabChangeCallback(const char* id, const std::string& tabname,
								   void (*on_tab_clicked)(void*, bool),
								   void* userdata,
								   void (*on_precommit)(void*, bool) = NULL);

	// LLTextBox
	void childSetWrappedText(const char* id, const std::string& text,
							 bool visible = true);

	// LLTextBox/LLTextEditor/LLLineEditor
	LL_INLINE void childSetText(const char* id, const std::string& text)
	{
		childSetValue(id, LLSD(text));
	}

	LL_INLINE std::string childGetText(const char* id) const
	{
		return childGetValue(id).asString();
	}

	// LLLineEditor
	void childSetKeystrokeCallback(const char* id,
								   void (*keystroke_callback)(LLLineEditor*,
															  void*),
								   void* user_data);
	void childSetPrevalidate(const char* id, bool (*func)(const LLWString&));

	// LLButton
	void childSetAction(const char* id, void(*function)(void*), void* value);
	void childSetActionTextbox(const char* id, void(*function)(void*),
							   void* value = NULL);
	void childSetControlName(const char* id, const char* ctrl_name);

	// Error reporting
	void childNotFound(const char* id) const;
	void childDisplayNotFound();

	static LLView* fromXML(LLXMLNodePtr node, LLView* parent,
						   LLUICtrlFactory* factory);

protected:
	// Override to set not found list
	LL_INLINE LLButton* getDefaultButton()					{ return mDefaultBtn; }

private:
	// Common construction logic
	void init();

	// From LLView
	void addCtrl(LLUICtrl* ctrl, S32 tab_group) override;
	void addCtrlAtEnd(LLUICtrl* ctrl, S32 tab_group) override;

protected:
	LLCallbackMap::map_t			mFactoryMap;

private:
	// Unified error reporting for the child* functions
	typedef std::set<std::string> expected_members_list_t;
	mutable expected_members_list_t	mExpectedMembers;
	mutable expected_members_list_t	mNewExpectedMembers;

	std::string						mRectControl;

	LLColor4						mBgColorAlpha;
	LLColor4						mBgColorOpaque;
	LLColor4						mDefaultBtnHighlight;

	LLViewBorder*					mBorder;
	LLButton*						mDefaultBtn;

	std::string						mLabel;

	typedef std::map<std::string, std::string> ui_string_map_t;
	ui_string_map_t					mUIStrings;

	S32								mLastTabGroup;

	bool							mBgVisible;
	bool							mBgOpaque;
};

class LLLayoutStack : public LLView
{
protected:
	LOG_CLASS(LLLayoutStack);

public:
	typedef enum e_layout_orientation
	{
		HORIZONTAL,
		VERTICAL
	} eLayoutOrientation;

	LLLayoutStack(eLayoutOrientation orientation);
	~LLLayoutStack() override;

	LLXMLNodePtr getXML(bool save_children = true) const override;
	void removeCtrl(LLUICtrl* ctrl) override;

	static LLView* fromXML(LLXMLNodePtr node, LLView* parent,
						   LLUICtrlFactory* factory);

	void draw() override;

	LL_INLINE S32 getMinWidth() const						{ return mMinWidth; }
	LL_INLINE S32 getMinHeight() const						{ return mMinHeight; }

	typedef enum e_animate
	{
		NO_ANIMATE,
		ANIMATE
	} EAnimate;

	void addPanel(LLPanel* panel, S32 min_width, S32 min_height,
				  bool auto_resize, bool user_resize,
				  EAnimate animate = NO_ANIMATE, S32 index = S32_MAX);
	void removePanel(LLPanel* panel);
	void collapsePanel(LLPanel* panel, bool collapsed = true);
	LL_INLINE S32 getNumPanels()							{ return mPanels.size(); }

	void deleteAllChildren() override;

private:
	void updateLayout(bool force_resize = false);
	void calcMinExtents();
	S32 getDefaultHeight(S32 cur_height);
	S32 getDefaultWidth(S32 cur_width);

	struct LLEmbeddedPanel;
	LLEmbeddedPanel* findEmbeddedPanel(LLPanel* panelp) const;

private:
	const eLayoutOrientation	mOrientation;

	typedef std::vector<LLEmbeddedPanel*> e_panel_list_t;
	e_panel_list_t				mPanels;

	S32							mMinWidth;
	S32							mMinHeight;
	S32							mPanelSpacing;
};

#endif
