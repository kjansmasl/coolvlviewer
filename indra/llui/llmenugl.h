/**
 * @file llmenugl.h
 * @brief Declaration of the opengl based menu system.
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

#ifndef LL_LLMENUGL_H
#define LL_LLMENUGL_H

#include <list>

#include "llevent.h"
#include "llfloater.h"
#include "llframetimer.h"
#include "llkeyboard.h"
#include "lluistring.h"
#include "llview.h"
#include "llcolor4.h"

// These callbacks are used by the LLMenuItemCallGL and LLMenuItemCheckGL
// classes during their work.
typedef void (*menu_callback)(void*);

// These callbacks are used by the LLMenuItemCallGL classes during their work.
typedef void (*on_disabled_callback)(void*);

// This callback is used by the LLMenuItemCallGL and LLMenuItemCheckGL to
// determine if the current menu is enabled.
typedef bool (*enabled_callback)(void*);

// This callback is used by LLMenuItemCheckGL to determine it is 'checked'
// state.
typedef bool (*check_callback)(void*);

// This callback is potentially used by LLMenuItemCallGL. If provided, this
// function is called whenever it's time to determine the label's contents.
// Put the contents of the label in the provided parameter.
typedef void (*label_callback)(std::string&, void*);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLMenuItemGL
//
// The LLMenuItemGL represents a single menu item in a menu.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLMenuItemGL : public LLView
{
public:
	// static functions to control the global color scheme.
	LL_INLINE static void setEnabledColor(const LLColor4& color)	{ sEnabledColor = color; }
	LL_INLINE static const LLColor4& getEnabledColor()				{ return sEnabledColor; }
	LL_INLINE static void setDisabledColor(const LLColor4& color)	{ sDisabledColor = color; }
	LL_INLINE static const LLColor4& getDisabledColor()				{ return sDisabledColor; }
	LL_INLINE static void setHighlightBGColor(const LLColor4& c)	{ sHighlightBackground = c; }
	LL_INLINE static const LLColor4& getHighlightBGColor()			{ return sHighlightBackground; }
	LL_INLINE static void setHighlightFGColor(const LLColor4& c)	{ sHighlightForeground = c; }
	LL_INLINE static const LLColor4& getHighlightFGColor()			{ return sHighlightForeground; }

	LLMenuItemGL(const std::string& name, const std::string& label,
				 KEY key = KEY_NONE, MASK = MASK_NONE);

	LLXMLNodePtr getXML(bool save_children = true) const override;

	LL_INLINE void setValue(const LLSD& v) override	{ setLabel(v.asString()); }

	LL_INLINE virtual std::string getType() const	{ return "item"; }

	// Sets the font used by this item.
	LL_INLINE void setFont(const LLFontGL* font)	{ mFont = font; }
	LL_INLINE const LLFontGL* getFont() const		{ return mFont; }
	LL_INLINE void setFontStyle(U8 style)			{ mStyle = style; }
	LL_INLINE U8 getFontStyle() const				{ return mStyle; }

	// Returns the height in pixels for the current font.
	virtual U32 getNominalHeight() const;

	// Marks item as not needing space for check marks or accelerator keys
	LL_INLINE virtual void setBriefItem(bool b)		{ mBriefItem = b; }
	LL_INLINE virtual bool isBriefItem() const		{ return mBriefItem; }

	void setJumpKey(KEY key);
	LL_INLINE KEY getJumpKey() const				{ return mJumpKey; }

	virtual bool addToAcceleratorList(std::list<LLKeyBinding*>* listp);
	virtual bool handleAcceleratorKey(KEY key, MASK mask);

	LL_INLINE void setAllowKeyRepeat(bool allow)	{ mAllowKeyRepeat = allow; }
	LL_INLINE bool getAllowKeyRepeat() const		{ return mAllowKeyRepeat; }

	// Change the label
	LL_INLINE void setLabel(const std::string& l)	{ mLabel = l; }
	LL_INLINE const std::string& getLabel() const	{ return mLabel.getString(); }
	bool setLabelArg(const std::string& key, const std::string& text) override;

	// Get the parent menu for this item
	virtual class LLMenuGL*	getMenu();

	// Returns the normal width of this control in pixels - this is used for
	// calculating the widest item, as well as for horizontal arrangement.
	virtual U32 getNominalWidth() const;

	// buildDrawLabel() constructs the string used during the draw() function.
	// This reduces the overall string manipulation, but can lead to visual
	// errors if the state of the object changes without the knowledge of the
	// menu item. For example, if a boolean being watched is changed outside of
	// the menu item's doIt() function, the draw buffer will not be updated and
	// will reflect the wrong value. If this ever becomes an issue, there are
	// ways to fix this. Returns the enabled state of the item.
	virtual void buildDrawLabel();

	// For branching menu items, bring sub menus up to root level of menu
	// hierarchy
	LL_INLINE virtual void updateBranchParent(LLView* parentp)
	{
	}

	// Does the primary functionality of the menu item.
	virtual void doIt();

	virtual void setHighlight(bool highlight);
	LL_INLINE virtual bool getHighlight() const		{ return mHighlight; }

	// Determines if this represents an active sub-menu
	LL_INLINE virtual bool isActive() const			{ return false; }

	// Determines if this represents an open sub-menu
	LL_INLINE virtual bool isOpen() const			{ return false; }

	LL_INLINE virtual void setEnabledSubMenus(bool)	{}

	// LLView Functionality
	bool handleHover(S32 x, S32 y, MASK mask) override;
	bool handleKeyHere(KEY key, MASK mask) override;
	bool handleMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleMouseUp(S32 x, S32 y, MASK mask) override;
	void draw() override;

	LL_INLINE bool getHover() const					{ return mGotHover; }

	LL_INLINE void setDrawTextDisabled(bool b)		{ mDrawTextDisabled = b; }
	LL_INLINE bool getDrawTextDisabled() const		{ return mDrawTextDisabled; }

protected:
	LL_INLINE void setHover(bool b)					{ mGotHover = b; }

	// This function appends the character string representation of
	// the current accelerator key and mask to the provided string.
	void appendAcceleratorString(std::string& st) const;

protected:
	KEY				mAcceleratorKey;
	MASK			mAcceleratorMask;

	// mLabel contains the actual label specified by the user.
	LLUIString		mLabel;

	// The draw labels contain some of the labels that we draw during the
	// draw() routine. This optimizes away some of the string manipulation.
	LLUIString		mDrawBoolLabel;
	LLUIString		mDrawAccelLabel;
	LLUIString		mDrawBranchLabel;

	bool			mHighlight;

private:
	static LLColor4	sEnabledColor;
	static LLColor4	sDisabledColor;
	static LLColor4	sHighlightBackground;
	static LLColor4	sHighlightForeground;

	// Keyboard and mouse variables
	bool			mAllowKeyRepeat;
	bool			mGotHover;

	// If true, suppress normal space for check marks on the left and
	// accelerator keys on the right.
	bool			mBriefItem;

	// Font for this item
	const LLFontGL*	mFont;
	U8				mStyle;
	bool			mDrawTextDisabled;

	KEY				mJumpKey;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLMenuItemCallGL
//
// The LLMenuItemCallerGL represents a single menu item in a menu that calls a
// user defined callback.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLMenuItemCallGL : public LLMenuItemGL, public LLOldEvents::LLObservable
{
public:
	// Normal constructors
	LLMenuItemCallGL(const std::string& name,
 					 menu_callback clicked_cb,
					 enabled_callback enabled_cb = NULL,
					 void* user_data = NULL,
					 KEY key = KEY_NONE, MASK mask = MASK_NONE,
					 bool enabled = true,
					 on_disabled_callback on_disabled_cb = NULL);
	LLMenuItemCallGL(const std::string& name,
					 const std::string& label,
 					 menu_callback clicked_cb,
					 enabled_callback enabled_cb = NULL,
					 void* user_data = NULL,
					 KEY key = KEY_NONE, MASK mask = MASK_NONE,
					 bool enabled = true,
					 on_disabled_callback on_disabled_cb = NULL);

	// Constructors for when you want to trap the arrange method.
	LLMenuItemCallGL(const std::string& name,
					 const std::string& label,
					 menu_callback clicked_cb,
					 enabled_callback enabled_cb,
					 label_callback label_cb,
					 void* user_data,
					 KEY key = KEY_NONE, MASK mask = MASK_NONE,
					 bool enabled = true,
					 on_disabled_callback on_disabled_c = NULL);
	LLMenuItemCallGL(const std::string& name,
					 menu_callback clicked_cb,
					 enabled_callback enabled_cb,
					 label_callback label_cb,
					 void* user_data,
					 KEY key = KEY_NONE, MASK mask = MASK_NONE,
					 bool enabled = true,
					 on_disabled_callback on_disabled_c = NULL);

	LLXMLNodePtr getXML(bool save_children = true) const override;

	LL_INLINE std::string getType() const override		{ return "call"; }

	void setEnabledControl(const std::string& enabled_ctrl, LLView* context);
	void setVisibleControl(const std::string& enabled_ctrl, LLView* context);

	LL_INLINE void setMenuCallback(menu_callback callback, void* data)
	{
		mCallback = callback;
		mUserData = data;
	}

	LL_INLINE menu_callback getMenuCallback() const		{ return mCallback; }

	LL_INLINE void setEnabledCallback(enabled_callback cb)
	{
		mEnabledCallback = cb;
	}

	LL_INLINE void setUserData(void* userdata)			{ mUserData = userdata; }
	LL_INLINE void* getUserData() const					{ return mUserData; }

	// Called to rebuild the draw label
	void buildDrawLabel() override;

	// Does the primary funcationality of the menu item.
	void doIt() override;

	bool handleAcceleratorKey(KEY key, MASK mask) override;

private:
	menu_callback			mCallback;
	// mEnabledCallback should return true if the item should be enabled
	enabled_callback		mEnabledCallback;
	label_callback			mLabelCallback;
	void*					mUserData;
	on_disabled_callback	mOnDisabledCallback;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLMenuItemCheckGL
//
// The LLMenuItemCheckGL is an extension of the LLMenuItemCallGL class, by
// allowing another method to be specified which determines if the menu item
// should consider itself checked as true or not. Be careful that the provided
// callback is fast - it needs to be VERY FUCKING EFFICIENT, because it may
// need to be checked a lot.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLMenuItemCheckGL : public LLMenuItemCallGL
{
public:
	LLMenuItemCheckGL(const std::string& name, const std::string& label,
					  menu_callback callback, enabled_callback enabled_cb,
					  check_callback check, void* user_data,
					  KEY key = KEY_NONE, MASK mask = MASK_NONE);
	LLMenuItemCheckGL(const std::string& name, menu_callback callback,
					  enabled_callback enabled_cb, check_callback check,
					  void* user_data, KEY key = KEY_NONE,
					  MASK mask = MASK_NONE);
	LLMenuItemCheckGL(const std::string& name, const std::string& label,
					  menu_callback callback, enabled_callback enabled_cb,
					  const char* control_name, LLView* context, void* data,
					  KEY key = KEY_NONE, MASK mask = MASK_NONE);

	LLXMLNodePtr getXML(bool save_children = true) const override;

	void setCheckedControl(std::string checked_control, LLView* context);

	LL_INLINE void setCheckCallback(check_callback cb)	{ mCheckCallback = cb; }

	void setValue(const LLSD& value) override;

	LL_INLINE std::string getType() const override		{ return "check"; }

	// Called to rebuild the draw label
	void buildDrawLabel() override;

private:
	check_callback mCheckCallback;
	bool mChecked;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLMenuItemToggleGL
//
// The LLMenuItemToggleGL is a menu item that wraps around a user
// specified and controlled boolean.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLMenuItemToggleGL : public LLMenuItemGL
{
public:
	LLMenuItemToggleGL(const std::string& name, const std::string& label,
					   bool* toggle, KEY key = KEY_NONE,
					   MASK mask = MASK_NONE);

	LLMenuItemToggleGL(const std::string& name, bool* toggle,
					   KEY key = KEY_NONE, MASK mask = MASK_NONE);

	// There is no getXML() because we cannot reference the toggled global
	// variable by XML use LLMenuItemCheckGL instead.

	LL_INLINE std::string getType() const override		{ return "toggle"; }

	// called to rebuild the draw label
	void buildDrawLabel() override;

	// Does the primary funcationality of the menu item.
	void doIt() override;

private:
	bool* mToggle;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLMenuGL
//
// The Menu class represents a normal rectangular menu somewhere on screen. A
// Menu can have menu items (described above) or sub-menus attached to it.
// Sub-menus are implemented via a specialized menu-item type known as a
// branch. Since it's easy to do wrong, I've taken the branch functionality out
// of public view, and encapsulate it in the appendMenu() method.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

// *TODO: the menu and menu item classes share a great deal of functionality
// and perhaps should be united. I think it may make the most sense to make
// LLMenuGL be a subclass of LLMenuItemGL. -MG
class LLMenuGL : public LLUICtrl
{
	friend class LLMenuItemBranchGL;

protected:
	LOG_CLASS(LLMenuGL);

public:
	LLMenuGL(const std::string& name, const std::string& label,
			 LLHandle<LLFloater> parent_floater = LLHandle<LLFloater>());
	LLMenuGL(const std::string& label,
			 LLHandle<LLFloater> parent_floater = LLHandle<LLFloater>());

	~LLMenuGL() override;

	LLXMLNodePtr getXML(bool save_children = true) const override;

	static LLView* fromXML(LLXMLNodePtr node, LLView* parent,
						   LLUICtrlFactory* factory);

	void parseChildXML(LLXMLNodePtr child, LLView* parent,
					   LLUICtrlFactory* factory);

	// LLView Functionality
	bool handleUnicodeCharHere(llwchar uni_char) override;
	bool handleHover(S32 x, S32 y, MASK mask) override;
	void setVisible(bool visible) override;
	void draw() override;

	void deleteAllChildren() override;

	virtual void drawBackground(LLMenuItemGL* itemp, LLColor4& color);

	virtual bool handleAcceleratorKey(KEY key, MASK mask);

	LLMenuGL* getChildMenuByName(const char* name, bool recurse) const;

	bool clearHoverItem();

	LL_INLINE const std::string& getLabel() const		{ return mLabel.getString(); }
	LL_INLINE void setLabel(const std::string& label)	{ mLabel = label; }

	// background colors
	static void setDefaultBackgroundColor(const LLColor4& color)
	{
		sDefaultBackgroundColor = color;
	}

	LL_INLINE void setBackgroundColor(const LLColor4& c)
	{
		mBackgroundColor = c;
	}

	LL_INLINE const LLColor4& getBackgroundColor() const
	{
		return mBackgroundColor;
	}

	LL_INLINE void setBackgroundVisible(bool b)			{ mBgVisible = b; }

	void setCanTearOff(bool tear_off,
					   LLHandle<LLFloater> parent_handle = LLHandle<LLFloater>());

	// Adds the menu item to this menu.
	virtual bool append(LLMenuItemGL* item);

	// Removes a menu item from this menu.
	virtual bool remove(LLMenuItemGL* item);

	// *NOTE: Mani - appendNoArrange() should be removed when merging to
	// skinning/viewer 2.0. It's added as a fix to a viewer 1.23 bug that has
	// already been address by skinning work.
	virtual bool appendNoArrange(LLMenuItemGL* item);

	// Adds a separator to this menu
	virtual bool appendSeparator(const std::string& name = LLStringUtil::null);

	// Adds a menu: this will create a cascading menu
	virtual bool appendMenu(LLMenuGL* menu);

	// For branching menu items, bring sub menus up to root level of menu
	// hierarchy
	virtual void updateParent(LLView* parentp);

	// Passes the name and the enable flag for a menu item.
	// true will make sure it is enabled, false will disable it.
	void setItemEnabled(const std::string& name, bool enable);

	// propagate message to submenus
	void setEnabledSubMenus(bool enable);

	void setItemVisible(const std::string& name, bool visible);

	void setItemLabel(const std::string& name, const std::string& label);

	// sets the left,bottom corner of menu, useful for popups
	void setLeftAndBottom(S32 left, S32 bottom);

	virtual bool handleJumpKey(KEY key);

	virtual bool jumpKeysActive();

	virtual bool isOpen();

	// Shape this menu to fit the current state of the children, and adjust the
	// child rects to fit. This is called automatically when you add items.
	// *FIX: We may need to deal with visibility arrangement.
	virtual void arrange();

	// remove all items on the menu
	void empty();

#if 0
	// Rearrange the components, and do the right thing if the menu doesn't fit
	// in the bounds.
	virtual void arrangeWithBounds(LLRect bounds);
#endif

	void setItemLastSelected(LLMenuItemGL* item);	// Must be in menu
	U32 getItemCount();								// Number of menu items

	LLMenuItemGL* getItem(S32 number);				// 0 = first item
	LLMenuItemGL* getItem(const std::string& name);

	LLMenuItemGL* getHighlightedItem();
	LLMenuItemGL* highlightNextItem(LLMenuItemGL* cur_item,
									  bool skip_disabled = true);
	LLMenuItemGL* highlightPrevItem(LLMenuItemGL* cur_item,
									  bool skip_disabled = true);

	void buildDrawLabels();
	void createJumpKeys();

	// Show popup in global screen space based on last mouse location.
	static void showPopup(LLMenuGL* menu);

	// Show popup at a specific location.
	static void showPopup(LLView* spawning_view, LLMenuGL* menu, S32 x, S32 y);

	// Whether to drop shadow menu bar
	LL_INLINE void setDropShadowed(bool b)				{ mDropShadowed = b; }

	LL_INLINE void setParentMenuItem(LLMenuItemGL* p)	{ mParentMenuItem = p; }
	LL_INLINE LLMenuItemGL* getParentMenuItem() const	{ return mParentMenuItem; }

	LL_INLINE void setTornOff(bool b)					{ mTornOff = b; }
	LL_INLINE bool getTornOff()							{ return mTornOff; }

	LL_INLINE bool getCanTearOff()						{ return mTearOffItem != NULL; }

	LL_INLINE KEY getJumpKey() const					{ return mJumpKey; }
	LL_INLINE void setJumpKey(KEY key)					{ mJumpKey = key; }

	LL_INLINE static void setKeyboardMode(bool mode)	{ sKeyboardMode = mode; }
	LL_INLINE static bool getKeyboardMode()				{ return sKeyboardMode; }

protected:
	void createSpilloverBranch();
	void cleanupSpilloverBranch();

public:
	static class LLMenuHolderGL* sMenuContainer;

protected:
	// *TODO: create accessor methods for these ?
	typedef std::list<LLMenuItemGL*> item_list_t;
	item_list_t					mItems;

	typedef std::map<KEY, LLMenuItemGL*> navigation_key_map_t;
	navigation_key_map_t		mJumpKeys;

	S32							mLastMouseX;
	S32							mLastMouseY;
	S32							mMouseVelX;
	S32							mMouseVelY;
	bool						mHorizontalLayout;
	bool						mKeepFixedSize;

private:
	static LLColor4 			sDefaultBackgroundColor;
	static bool					sKeyboardMode;

	LLColor4					mBackgroundColor;

	bool						mBgVisible;
	LLMenuItemGL*				mParentMenuItem;
	LLUIString					mLabel;
	bool 						mDropShadowed;	// Whether to drop shadow
	bool						mHasSelection;
	LLFrameTimer				mFadeTimer;
	bool						mTornOff;
	class LLMenuItemTearOffGL*	mTearOffItem;
	class LLMenuItemBranchGL*	mSpilloverBranch;
	LLMenuGL*					mSpilloverMenu;
	LLHandle<LLFloater>			mParentFloaterHandle;
	KEY							mJumpKey;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLMenuItemBranchGL
//
// The LLMenuItemBranchGL represents a menu item that has a sub-menu. This is
// used to make cascading menus.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLMenuItemBranchGL : public LLMenuItemGL
{
public:
	LLMenuItemBranchGL(const std::string& name, const std::string& label,
					   LLHandle<LLView> branch,
					   KEY key = KEY_NONE, MASK mask = MASK_NONE);

	~LLMenuItemBranchGL() override;

	LLXMLNodePtr getXML(bool save_children = true) const override;

	LL_INLINE std::string getType() const override		{ return "menu"; }

	bool handleMouseUp(S32 x, S32 y, MASK mask) override;

	bool handleAcceleratorKey(KEY key, MASK mask) override;

	// Check if we've used these accelerators already
	bool addToAcceleratorList(std::list <LLKeyBinding*>* listp) override;

	// Called to rebuild the draw label
	void buildDrawLabel() override;

	// Does the primary funcationality of the menu item.
	void doIt() override;

	bool handleKey(KEY key, MASK mask, bool called_from_parent) override;
	bool handleUnicodeChar(llwchar uni_char, bool called_from_parent) override;

	// Sets the hover status (called by it's menu) and if the object is
	// active. This is used for behavior transfer.
	void setHighlight(bool highlight) override;

	bool handleKeyHere(KEY key, MASK mask) override;

	LL_INLINE bool isActive() const override			{ return isOpen() && getBranch()->getHighlightedItem(); }

	LL_INLINE bool isOpen() const override				{ return getBranch() && getBranch()->isOpen(); }

	LL_INLINE LLMenuGL* getBranch() const				{ return (LLMenuGL*)(mBranch.get()); }

	void updateBranchParent(LLView* parentp) override;

	LL_INLINE void setEnabledSubMenus(bool enabled) override
	{
		if (getBranch())
		{
			getBranch()->setEnabledSubMenus(enabled);
		}
	}

	virtual void openMenu();

	// LLView Functionality
	void onVisibilityChange(bool visible) override;
	void draw() override;

	LLView* getChildView(const char* name, bool recurse = true,
						 bool create_if_missing = true) const override;

private:
	LLHandle<LLView> mBranch;
};

//-----------------------------------------------------------------------------
// class LLPieMenu
// A circular menu of items, icons, etc.
//-----------------------------------------------------------------------------

class LLPieMenu : public LLMenuGL
{
public:
	LLPieMenu(const std::string& name, const std::string& label);
	LLPieMenu(const std::string& name);

	LLXMLNodePtr getXML(bool save_children = true) const override;
	void initXML(LLXMLNodePtr node, LLView* context, LLUICtrlFactory* factory);

	// LLView Functionality

	// Cannot set visibility directly: must call show or hide
	void setVisible(bool visible) override;

	bool handleHover(S32 x, S32 y, MASK mask) override;
	bool handleMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleRightMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleRightMouseUp(S32 x, S32 y, MASK mask) override;
	bool handleMouseUp(S32 x, S32 y, MASK mask) override;
	void draw() override;
	void drawBackground(LLMenuItemGL* itemp, LLColor4& color) override;

	bool append(LLMenuItemGL* item) override;
	bool appendSeparator(const std::string& name = LLStringUtil::null) override;

	bool appendPieMenu(LLPieMenu* menu);

	void arrange() override;

	// Display the menu centered on this point on the screen.
	void show(S32 x, S32 y, bool mouse_down);
	void hide(bool item_selected);

private:
	LLMenuItemGL* pieItemFromXY(S32 x, S32 y);
	S32			  pieItemIndexFromXY(S32 x, S32 y);

#if 0	// These cause menu items to be spuriously selected by right-clicks
		// near the window edge at low frame rates. I don't think they are
		// needed unless you shift the menu position in the draw() function. JC
	S32				mShiftHoriz;	// non-zero if menu had to shift this frame
	S32				mShiftVert;		// non-zero if menu had to shift this frame
#endif

	bool			mFirstMouseDown;	// true from show until mouse up
	bool			mRightMouseDown;

	// allow picking pie menu items anywhere outside of center circle:
	bool			mUseInfiniteRadius;

	LLMenuItemGL*	mHoverItem;
	bool			mHoverThisFrame;
	bool			mHoveredAnyItem;
	LLFrameTimer	mShrinkBorderTimer;

	// For rendering pie menus as both bounded and unbounded:
	F32				mOuterRingAlpha;

	F32				mCurRadius;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLMenuBarGL
//
// A menu bar displays menus horizontally.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLMenuBarGL : public LLMenuGL
{
public:
	LLMenuBarGL(const std::string& name);
	~LLMenuBarGL() override;

	LLXMLNodePtr getXML(bool save_children = true) const override;
	static LLView* fromXML(LLXMLNodePtr node, LLView* parent,
						   LLUICtrlFactory* factory);

	// LLView Functionality
	bool handleHover(S32 x, S32 y, MASK mask) override;
	bool handleKeyHere(KEY key, MASK mask) override;
	bool handleMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleRightMouseDown(S32 x, S32 y, MASK mask) override;
	void draw() override;

	bool jumpKeysActive() override;

	bool handleJumpKey(KEY key) override;
	bool handleAcceleratorKey(KEY key, MASK mask) override;

	// Rearranges the child rects so they fit the shape of the menu bar.
	void arrange() override;

	// Add a vertical separator to this menu
	bool appendSeparator(const std::string& name = LLStringUtil::null) override;

	// Adds a menu; this will create a drop down menu.
	bool appendMenu(LLMenuGL* menu) override;

	// Returns x position of rightmost child, usually Help menu
	S32 getRightmostMenuEdge();

	LL_INLINE void resetMenuTrigger()					{ mAltKeyTrigger = false; }

private:
	void checkMenuTrigger();

private:
	std::list <LLKeyBinding*>	mAccelerators;
	bool						mAltKeyTrigger;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLMenuHolderGL
//
// High level view that serves as parent for all menus
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class LLMenuHolderGL : public LLPanel
{
public:
	LLMenuHolderGL();
	LLMenuHolderGL(const std::string& name, const LLRect& rect,
				   bool mouse_opaque, U32 follows = FOLLOWS_NONE);

	void reshape(S32 width, S32 height, bool call_from_parent = true) override;

	virtual bool hideMenus();

	LL_INLINE void setCanHide(bool can_hide)			{ mCanHide = can_hide; }

	// LLView functionality
	void draw() override;
	bool handleMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleRightMouseDown(S32 x, S32 y, MASK mask) override;

	LL_INLINE virtual const LLRect getMenuRect() const	{ return getLocalRect(); }
	virtual bool hasVisibleMenu() const;

	static void setActivatedItem(LLMenuItemGL* item);

private:
	bool					mCanHide;

	static LLHandle<LLView>	sItemLastSelectedHandle;
	static LLFrameTimer		sItemActivationTimer;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLTearOffMenu
//
// Floater that hosts a menu
// https://wiki.lindenlab.com/mediawiki/index.php?title=LLTearOffMenu&oldid=81344
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLTearOffMenu : public LLFloater
{
public:
	static LLTearOffMenu* create(LLMenuGL* menup);

	void onClose(bool app_quitting) override;
	void draw() override;
	void onFocusReceived() override;
	void onFocusLost() override;
	bool handleUnicodeChar(llwchar uni_char, bool called_from_parent) override;
	bool handleKeyHere(KEY key, MASK mask) override;
	void translate(S32 x, S32 y) override;

private:
	LLTearOffMenu(LLMenuGL* menup);

private:
	LLView*		mOldParent;
	LLMenuGL*	mMenu;
	F32			mTargetHeight;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLMenuItemTearOffGL
//
// This class represents a separator.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLMenuItemTearOffGL : public LLMenuItemGL
{
public:
	LLMenuItemTearOffGL(LLHandle<LLFloater> parent_handle = LLHandle<LLFloater>());

	LLXMLNodePtr getXML(bool save_children = true) const override;

	void draw() override;

	LL_INLINE std::string getType() const override		{ return "tearoff_menu"; }

	void doIt() override;
	U32 getNominalHeight() const override;

private:
	LLHandle<LLFloater> mParentHandle;
};

extern const char* LL_PIE_MENU_TAG;

#endif // LL_LLMENUGL_H
