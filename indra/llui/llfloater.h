/**
 * @file llfloater.h
 * @brief LLFloater base class
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 *
 * Copyright (c) 2002-2009, Linden Research, Inc.
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

// Floating "windows" within the GL display, like the inventory floater,
// mini-map floater, etc.

#ifndef LL_FLOATER_H
#define LL_FLOATER_H

#include <set>

#include "llnotifications.h"
#include "llpanel.h"
#include "lltabcontainer.h"

class LLButton;
class LLDragHandle;
class LLFloater;
class LLMultiFloater;
class LLResizeBar;
class LLResizeHandle;

constexpr S32 LLFLOATER_VPAD = 6;
constexpr S32 LLFLOATER_HPAD = 6;
constexpr S32 LLFLOATER_CLOSE_BOX_SIZE = 16;
constexpr S32 LLFLOATER_HEADER_SIZE = 18;

constexpr bool RESIZE_YES = true;
constexpr bool RESIZE_NO = false;

constexpr S32 DEFAULT_MIN_WIDTH = 100;
constexpr S32 DEFAULT_MIN_HEIGHT = 100;

constexpr bool DRAG_ON_TOP = false;
constexpr bool DRAG_ON_LEFT = true;

constexpr bool MINIMIZE_YES = true;
constexpr bool MINIMIZE_NO = false;

constexpr bool CLOSE_YES = true;
constexpr bool CLOSE_NO = false;

constexpr bool ADJUST_VERTICAL_YES = true;
constexpr bool ADJUST_VERTICAL_NO = false;

// Associates a given notification instance with a particular floater
class LLFloaterNotificationContext : public LLNotificationContext
{
public:
	LLFloaterNotificationContext(LLHandle<LLFloater> handle)
	:	mFloaterHandle(handle)
	{
	}

	LL_INLINE LLFloater* getFloater()					{ return mFloaterHandle.get(); }

private:
	LLHandle<LLFloater> mFloaterHandle;
};

class LLFloater : public LLPanel
{
	friend class LLFloaterView;
	friend class LLHostFloater;

protected:
	LOG_CLASS(LLFloater);

public:
	enum EFloaterButtons
	{
		BUTTON_CLOSE,
		BUTTON_RESTORE,
		BUTTON_MINIMIZE,
		BUTTON_TEAR_OFF,
		BUTTON_COUNT
	};

	LLFloater();

	// Simple constructor for data-driven initialization
 	LLFloater(const std::string& name);

	LLFloater(const std::string& name, const LLRect& rect,
			  const std::string& title, bool resizable = false,
			  S32 min_width = DEFAULT_MIN_WIDTH,
			  S32 min_height = DEFAULT_MIN_HEIGHT,
			  bool drag_on_left = false, bool minimizable = true,
			  bool close_btn = true, bool bordered = BORDER_NO);

	LLFloater(const std::string& name, const std::string& rect_control,
			  const std::string& title, bool resizable = false,
			  S32 min_width = DEFAULT_MIN_WIDTH,
			  S32 min_height = DEFAULT_MIN_HEIGHT,
			  bool drag_on_left = false, bool minimizable = true,
			  bool close_btn = true, bool bordered = BORDER_NO);

	~LLFloater() override;

	LL_INLINE LLFloater* asFloater() override		{ return this; }

	LLXMLNodePtr getXML(bool save_children = true) const override;
	static LLView* fromXML(LLXMLNodePtr node, LLView* parent,
						   LLUICtrlFactory* factory);
	void initFloaterXML(LLXMLNodePtr node, LLView* parent,
						LLUICtrlFactory* factory, bool open_it = true);

	void userSetShape(const LLRect& new_rect) override;
	bool canSnapTo(LLView* other_view) override;
	void snappedTo(LLView* snap_view) override;
	void setFocus(bool b) override;
	void setIsChrome(bool is_chrome) override;

	// Can be called multiple times to reset floater parameters.
	// Deletes all children of the floater.
	virtual void initFloater(const std::string& title, bool resizable,
							 S32 min_width, S32 min_height, bool drag_on_left,
							 bool minimizable, bool close_btn);

	virtual void open();

	// If allowed, close the floater cleanly, releasing focus. 'app_quitting'
	// is passed to onClose() below.
	virtual void close(bool app_quitting = false);

	void reshape(S32 width, S32 height, bool call_from_parent = true) override;

	// Release keyboard and mouse focus
	void releaseFocus();

	// Moves to center of gFloaterViewp
	void center();
	// Applies rectangle stored in mRectControl, if any
	void applyRectControl();

	LL_INLINE LLMultiFloater* getHost()					{ return (LLMultiFloater*)mHostHandle.get(); }

	void applyTitle();
	const std::string& getCurrentTitle() const;
	void setTitle(const std::string& title);
	std::string getTitle();
	// 'false' after the floater title has been changed via setTitle(). HB
	LL_INLINE bool isTitlePristine() const				{ return mTitleIsPristine; }
	void setShortTitle(const std::string& short_title);
	std::string getShortTitle();
	void setTitleVisible(bool visible);
	virtual void setMinimized(bool b);
	void moveResizeHandlesToFront();

	void addDependentFloater(LLFloater* dependent, bool reposition = true);
	void addDependentFloater(LLHandle<LLFloater> dependent_handle,
							 bool reposition = true);

	LL_INLINE LLFloater* getDependee()					{ return (LLFloater*)mDependeeHandle.get(); }

	void removeDependentFloater(LLFloater* dependent);

	LL_INLINE bool isMinimized()						{ return mMinimized; }
	bool isFrontmost();
	LL_INLINE bool isDependent()						{ return !mDependeeHandle.isDead(); }
	void setCanMinimize(bool can_minimize);
	void setCanClose(bool can_close);
	void setCanTearOff(bool can_tear_off);
	virtual void setCanResize(bool can_resize);
	void setCanDrag(bool can_drag);
	void setHost(LLMultiFloater* host);
	LL_INLINE bool isResizable() const					{ return mResizable; }

	void setResizeLimits(S32 min_width, S32 min_height);

	LL_INLINE void getResizeLimits(S32* min_width, S32* min_height)
	{
		*min_width = mMinWidth;
		*min_height = mMinHeight;
	}

	LL_INLINE bool isMinimizeable() const				{ return mButtonsEnabled[BUTTON_MINIMIZE]; }
	// Does this window have a close button, NOT can we close it right now.
	LL_INLINE bool isCloseable() const					{ return mButtonsEnabled[BUTTON_CLOSE]; }
	LL_INLINE bool isDragOnLeft() const					{ return mDragOnLeft; }
	LL_INLINE S32 getMinWidth() const					{ return mMinWidth; }
	LL_INLINE S32 getMinHeight() const					{ return mMinHeight; }

	bool handleMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleRightMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleDoubleClick(S32 x, S32 y, MASK mask) override;
	bool handleMiddleMouseDown(S32 x, S32 y, MASK mask) override;
	void draw() override;

	virtual void onOpen()								{}

	// Call destroy() to free memory, or setVisible(false) to keep it
	// If app_quitting, you might not want to save your visibility.
	// Defaults to destroy().
	virtual void onClose(bool app_quitting)				{ destroy(); }

	// This cannot be "const" until all derived floater canClose() methods are
	// const as well.  JC
	virtual bool canClose()								{ return true; }

	void setVisible(bool visible) override;
	void setFrontmost(bool take_focus = true);

	// Must default to false. Used by the texture preview floater and tested in
	// the viewer menu.
	virtual bool canSaveAs() const						{ return false; }

	virtual void saveAs()								{}

	LL_INLINE void setSnapTarget(LLHandle<LLFloater> h)	{ mSnappedTo = h; }

	LL_INLINE void clearSnapTarget()					{ mSnappedTo.markDead(); }

	LL_INLINE LLHandle<LLFloater> getSnapTarget() const	{ return mSnappedTo; }

	LL_INLINE LLHandle<LLFloater> getHandle() const		{ return getDerivedHandle<LLFloater>(); }

	LL_INLINE U32 getId() const							{ return mId; }

	// Return a closeable floater, if any, given the current focus.
	static LLFloater* getClosableFloaterFromFocus();

	// Close the floater returned by getClosableFloaterFromFocus() and
	// handle refocusing.
	static void closeFocusedFloater();

	LL_INLINE LLNotification::Params contextualNotification(const std::string& name)
	{
	    return LLNotification::Params(name).context(mNotificationContext);
	}

	static void onClickClose(void* userdata);
	static void onClickMinimize(void* userdata);
	static void onClickTearOff(void* userdata);

	// Returns true (and updated size) when a floater resizing is in progress,
	// or false (and untouched size) otherwise. Resets the resizing state as
	// well (will be re-evaluated on next frame only), so this is only to be
	// called once per frame (currently only used in llviewerwindow.cpp, to
	// display the resizing floater size. HB
	static bool resizing(S32& size_x, S32& size_y);

	LL_INLINE static LLMultiFloater* getFloaterHost()	{ return sHostp; }

protected:
	virtual void bringToFront(S32 x, S32 y);
	virtual void setVisibleAndFrontmost(bool take_focus = true);

	// Size when not minimized
	LL_INLINE void setExpandedRect(const LLRect& rect)	{ mExpandedRect = rect; }
	LL_INLINE const LLRect& getExpandedRect() const		{ return mExpandedRect; }

	// Returns true while the floater is being resized via its resize handles
	// (or resize bars). HB
	bool resizedFromHandles() const;

	// Whether to automatically take focus when opened
	LL_INLINE void setAutoFocus(bool focus)				{ mAutoFocus = focus; }

	LL_INLINE LLDragHandle* getDragHandle() const		{ return mDragHandle; }

	 // Do not call this directly. You probably want to call close(). JC
	LL_INLINE void destroy()							{ die(); }

private:
	void setForeground(bool b);		// Called only by floaterview
	void cleanupHandles();			// Removes handles to dead floaters
	void createMinimizeButton();
	void updateButtons();
	void buildButtons();
	bool offerClickToButton(S32 x, S32 y, MASK mask, EFloaterButtons index);

protected:
	LLButton*						mButtons[BUTTON_COUNT];

private:
	LLRect							mExpandedRect;
	LLDragHandle*					mDragHandle;
	LLResizeBar*					mResizeBar[4];
	LLResizeHandle*					mResizeHandle[4];
	LLButton*						mMinimizeButton;
	LLFloaterNotificationContext*	mNotificationContext;

	U32								mId;		// Unique number. HB
	S32								mMinWidth;
	S32								mMinHeight;
	S32								mPreviousMinimizedBottom;
	S32								mPreviousMinimizedLeft;

	F32								mButtonScale;

	LLHandle<LLFloater>				mDependeeHandle;
	LLHandle<LLFloater>				mSnappedTo;
	LLHandle<LLFloater>				mHostHandle;
	LLHandle<LLFloater>				mLastHostHandle;

	std::string						mTitle;
	std::string						mShortTitle;

	typedef std::set<LLHandle<LLFloater> > handle_set_t;
	typedef std::set<LLHandle<LLFloater> >::iterator handle_set_iter_t;
	handle_set_t					mDependents;

	std::vector<LLHandle<LLView> >	mMinimizedHiddenChildren;

	bool							mButtonsEnabled[BUTTON_COUNT];
	bool							mAutoFocus;
	bool							mHasBeenDraggedWhileMinimized;
	bool							mResizable;
	bool							mCanTearOff;
	bool							mMinimized;
	bool							mForeground;
	bool							mDragOnLeft;
	bool							mTitleIsPristine;

	typedef void (*click_callback)(void*);
	static click_callback			sButtonCallbacks[BUTTON_COUNT];

	static LLMultiFloater*			sHostp;
	static std::string				sButtonActiveImageNames[BUTTON_COUNT];
	static std::string				sButtonInactiveImageNames[BUTTON_COUNT];
	static std::string				sButtonPressedImageNames[BUTTON_COUNT];
	static std::string				sButtonNames[BUTTON_COUNT];
	static std::string				sButtonToolTipNames[BUTTON_COUNT];
	// Used to affect an unique number (Id) to each new floater. HB
	static U32						sLastFloaterId;
};

// Use this class in a scope to set the host of floaters you want to open
// inside a multi-floater.
// Declare an instance of the class in a scope, passing it a multi-floater
// pointer or nothing, possibly using the set() method to change your host
// floater. Then, once done opening your children floaters, make sure the
// scope is closed so that the instance gets deleted: the former host will be
// automatically restored. Do make sure you exited the scope before calling
// open() on your host floater. HB
class LLHostFloater
{
public:
	LL_INLINE LLHostFloater(LLMultiFloater* host = NULL)
	:	mPreviousHost(LLFloater::sHostp)
	{
		LLFloater::sHostp = host;
	}

	LL_INLINE ~LLHostFloater()
	{
		LLFloater::sHostp = mPreviousHost;
	}

	LL_INLINE void set(LLMultiFloater* host)
	{
		LLFloater::sHostp = host;
	}

private:
	LLMultiFloater* mPreviousHost;
};

///////////////////////////////////////////////////////////////////////////////
// LLFloaterView class: parent of all floating panels

class LLFloaterView : public LLUICtrl
{
public:
	LLFloaterView(const std::string& name, const LLRect& rect);

	void reshape(S32 width, S32 height, bool call_from_parent = true) override;
	void reshapeFloater(S32 width, S32 height, bool called_from_parent,
						bool adjust_vertical);

	void draw() override;
	void refresh();
	LLRect getSnapRect() const override;

	void getNewFloaterPosition(S32* left, S32* top);
	void resetStartingFloaterPosition();
	LLRect findNeighboringPosition(LLFloater* reference_floater,
								   LLFloater* neighbor);

	// Given a child of gFloaterViewp, make sure this view can fit entirely
	// onscreen.
	void adjustToFitScreen(LLFloater* floater,
						   bool allow_partial_outside = false);

	void getMinimizePosition(S32* left, S32* bottom);
	void restoreAll();		// Un-minimize all floaters

	typedef std::set<LLView*> skip_list_t;
	void pushVisibleAll(bool visible,
						const skip_list_t& skip_list = skip_list_t());
	void popVisibleAll(const skip_list_t& skip_list = skip_list_t());

	// Causes all open and "visible" floaters to be adjusted to fit screen. HB
	void fitAllToScreen();

	LL_INLINE void setCycleMode(bool mode)				{ mFocusCycleMode = mode; }
	LL_INLINE bool getCycleMode() const					{ return mFocusCycleMode; }
	bool bringToFront(LLFloater* child, bool give_focus = true);
	void highlightFocusedFloater();
	void unhighlightFocusedFloater();
	void focusFrontFloater();
	void destroyAllChildren();
	// Attempts to close all floaters:
	void closeAllChildren(bool app_quitting);
	bool allChildrenClosed();

	LLFloater* getFrontmost();
	LLFloater* getBackmost();
	LLFloater* getParentFloater(LLView* viewp);
	LLFloater* getFocusedFloater();
	void syncFloaterTabOrder();

	// Returns z order of child provided. 0 is closest, larger numbers are
	// deeper in the screen. If there is no such child, the return value is not
	// defined.
	S32 getZOrder(LLFloater* child);

	LL_INLINE void setSnapOffsetBottom(S32 offset)		{ mSnapOffsetBottom = offset; }

	LL_INLINE static void setStackMinimizedTopToBottom(bool b)
	{
		sStackMinimizedTopToBottom = b;
	}

	LL_INLINE static void setStackMinimizedRightToLeft(bool b)
	{
		sStackMinimizedRightToLeft = b;
	}

	LL_INLINE static void setStackScreenWidthFraction(U32 f)
	{
		if (f > 0)
		{
			sStackScreenWidthFraction = f;
		}
	}

private:
	S32				mColumn;
	S32				mNextLeft;
	S32				mNextTop;
	S32				mSnapOffsetBottom;
	bool			mFocusCycleMode;

	static U32		sStackScreenWidthFraction;
	static bool		sStackMinimizedTopToBottom;
	static bool		sStackMinimizedRightToLeft;
};

///////////////////////////////////////////////////////////////////////////////
// LLMultiFloater class

class LLMultiFloater : public LLFloater
{
public:
	LLMultiFloater();
	LLMultiFloater(LLTabContainer::TabPosition tab_pos);
	LLMultiFloater(const std::string& name);
	LLMultiFloater(const std::string& name, const LLRect& rect,
				   LLTabContainer::TabPosition tab_pos = LLTabContainer::TOP,
				   bool auto_resize = true);
	LLMultiFloater(const std::string& name, const std::string& rect_control,
				   LLTabContainer::TabPosition tab_pos = LLTabContainer::TOP,
				   bool auto_resize = true);

	LLXMLNodePtr getXML(bool save_children = true) const override;

	bool postBuild() override;

	void open() override;
	void onClose(bool app_quitting) override;
	void draw() override;
	void setVisible(bool visible) override;
	bool handleKeyHere(KEY key, MASK mask) override;

	void setCanResize(bool can_resize) override;

	// New virtual methods
	virtual void growToFit(S32 content_width, S32 content_height);
	virtual void addFloater(LLFloater* floaterp, bool select_added_floater,
							LLTabContainer::eInsertionPoint pos =
								LLTabContainer::END);

	virtual void updateResizeLimits();

	virtual void showFloater(LLFloater* floaterp);
	virtual void removeFloater(LLFloater* floaterp);

	virtual void tabOpen(LLFloater* opened_floater, bool from_click);
	virtual void tabClose();

	virtual bool selectFloater(LLFloater* floaterp);
	virtual void selectNextFloater();
	virtual void selectPrevFloater();

	virtual LLFloater* getActiveFloater();
	virtual bool isFloaterFlashing(LLFloater* floaterp);
	virtual S32 getFloaterCount();

	virtual void setFloaterFlashing(LLFloater* floaterp, bool flashing);

	// Returns false if the floater could not be closed due to pending
	// confirmation dialogs
	virtual bool closeAllFloaters();

	LL_INLINE void setTabContainer(LLTabContainer* tab_container)
	{
		if (!mTabContainer)
		{
			mTabContainer = tab_container;
		}
	}

	static void onTabSelected(void* userdata, bool);

protected:
	struct LLFloaterData
	{
		S32		mWidth;
		S32		mHeight;
		bool	mCanMinimize;
		bool	mCanResize;
	};

	LLTabContainer*		mTabContainer;

	typedef std::map<LLHandle<LLFloater>, LLFloaterData> floater_data_map_t;
	floater_data_map_t	mFloaterDataMap;

	// Logically const but initialized late
	S32					mOrigMinWidth;
	S32					mOrigMinHeight;

	LLTabContainer::TabPosition	mTabPos;
	bool				mAutoResize;
};

// Visibility policy specialized for floaters
template<>
class VisibilityPolicy<LLFloater>
{
public:
	// Visibility methods
	LL_INLINE static bool visible(LLFloater* instance, const LLSD& key)
	{
		if (instance)
		{
			return !instance->isMinimized() && instance->isInVisibleChain();
		}
		return false;
	}

	LL_INLINE static void show(LLFloater* instance, const LLSD& key)
	{
		if (instance)
		{
			instance->open();
			if (instance->getHost())
			{
				instance->getHost()->open();
			}
		}
	}

	LL_INLINE static void hide(LLFloater* instance, const LLSD& key)
	{
		if (instance)
		{
			instance->close();
		}
	}
};

// Singleton implementation for floaters (provides visibility policy)
template <class T> class LLFloaterSingleton
:	public LLUISingleton<T, VisibilityPolicy<LLFloater> >
{
};

extern LLFloaterView* gFloaterViewp;
extern S32 gMenuBarHeight;

#endif  // LL_FLOATER_H
