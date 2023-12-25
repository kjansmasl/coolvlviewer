/**
 * @file llview.h
 * @brief Container for other views, anything that draws.
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

#ifndef LL_LLVIEW_H
#define LL_LLVIEW_H

// A view is an area in a window that can draw. It might represent the HUD or a
// dialog box or a button. It can also contain sub-views and child widgets.

#include "boost/function.hpp"
#include "boost/signals2.hpp"

#include "llassettype.h"
#include "llcoord.h"
#include "llcursortypes.h"
#include "llerror.h"
#include "llevent.h"
#include "hbfastmap.h"
#include "llfocusmgr.h"
#include "llfontgl.h"
#include "llhandle.h"
#include "llmortician.h"
#include "llmousehandler.h"
#include "llrect.h"
#include "llui.h"
#include "lluistring.h"
#include "llviewquery.h"
#include "llxmlnode.h"

class LLFloater;
class LLPanel;
class LLUICrtl;
class LLUICtrlFactory;

constexpr U8 FOLLOWS_NONE	= 0x00;
constexpr U8 FOLLOWS_LEFT	= 0x01;
constexpr U8 FOLLOWS_RIGHT	= 0x02;
constexpr U8 FOLLOWS_TOP	= 0x10;
constexpr U8 FOLLOWS_BOTTOM	= 0x20;
constexpr U8 FOLLOWS_ALL	= 0x33;

constexpr U32 GL_NAME_UI_RESERVED = 2;

// Maps XML strings to widget classes
class LLWidgetClassRegistry : public LLSingleton<LLWidgetClassRegistry>
{
	friend class LLSingleton<LLWidgetClassRegistry>;

public:
	typedef LLView* (*factory_func_t)(LLXMLNodePtr node, LLView* parent,
									  LLUICtrlFactory* factory);
	typedef fast_hmap<std::string, factory_func_t> factory_map_t;

	LL_INLINE void registerCtrl(const std::string& tag, factory_func_t func)
	{
		mCreatorFunctions.emplace(tag, func);
	}

	LL_INLINE bool isTagRegistered(const std::string& xml_tag)
	{
		return mCreatorFunctions.count(xml_tag) > 0;
	}

	LL_INLINE factory_func_t getCreatorFunc(const std::string& xml_tag)
	{
		factory_map_t::const_iterator it = mCreatorFunctions.find(xml_tag);
		return it != mCreatorFunctions.end() ? it->second : NULL;
	}

	// Gets (first) xml tag for a given class
	template<class T> const std::string& getTag()
	{
		for (factory_map_t::iterator it = mCreatorFunctions.begin(),
									 end = mCreatorFunctions.end();
			 it != end; ++it)
		{
			if (it->second == T::fromXML)
			{
				return it->first;
			}
		}
		return LLStringUtil::null;
	}

private:
	LLWidgetClassRegistry() = default;
	virtual ~LLWidgetClassRegistry() = default;

private:
	// Map of XML tags to widget creator functions
	factory_map_t 	mCreatorFunctions;
};

template<class T>
class LLRegisterWidget
{
public:
	LLRegisterWidget(const std::string& tag)
	{
		LLWidgetClassRegistry* registry = LLWidgetClassRegistry::getInstance();
		if (registry->isTagRegistered(tag))
		{
			llerrs << "Widget named " << tag << " is already registered !"
				   << llendl;
		}
		else
		{
			registry->registerCtrl(tag, T::fromXML);
		}
	}
};

class LLView : public LLMouseHandler, public LLFocusableElement,
			   public LLMortician, public LLHandleProvider<LLView>,
			   public boost::signals2::trackable
{
protected:
	LOG_CLASS(LLView);

public:
	enum ESoundFlags
	{
		SILENT = 0,
		MOUSE_DOWN = 1,
		MOUSE_UP = 2
	};

	enum ESnapType
	{
		SNAP_PARENT,
		SNAP_SIBLINGS,
		SNAP_PARENT_AND_SIBLINGS
	};

	enum ESnapEdge
	{
		SNAP_LEFT,
		SNAP_TOP,
		SNAP_RIGHT,
		SNAP_BOTTOM
	};

	typedef std::list<LLView*> child_list_t;
	typedef child_list_t::iterator child_list_iter_t;
	typedef child_list_t::const_iterator child_list_const_iter_t;
	typedef child_list_t::reverse_iterator child_list_reverse_iter_t;
	typedef child_list_t::const_reverse_iterator child_list_const_reverse_iter_t;

	typedef std::vector<class LLUICtrl*> ctrl_list_t;

	typedef std::pair<S32, S32> tab_order_t;
	typedef std::pair<LLUICtrl*, tab_order_t> tab_order_pair_t;

	// This container primarily sorts by the tab group, secondarily by the
	// insertion ordinal (lastly by the value of the pointer)
	typedef std::map<const LLUICtrl*, tab_order_t> child_tab_order_t;
	typedef child_tab_order_t::iterator child_tab_order_iter_t;
	typedef child_tab_order_t::const_iterator child_tab_order_const_iter_t;

	LLView();
	LLView(const std::string& name, bool mouse_opaque);
	LLView(const std::string& name, const LLRect& rect, bool mouse_opaque,
		   U8 follows = FOLLOWS_NONE);
	// UI elements are not copyable
	LLView(const LLView&) = delete;

	~LLView() override;

	// Hack to support LLFocusMgr (from LLMouseHandler)
	LL_INLINE bool isView() const override				{ return true; }

	// Some UI widgets need to be added as controls. Others need to be added as
	// regular view children. isCtrl() should return true if a widget needs to
	// be added as a ctrl.
	LL_INLINE virtual bool isCtrl() const				{ return false; }

	LL_INLINE virtual LLFloater* asFloater()			{ return NULL; }
	LL_INLINE virtual LLPanel* asPanel()				{ return NULL; }

	LL_INLINE void setMouseOpaque(bool b)				{ mMouseOpaque = b; }
	LL_INLINE bool getMouseOpaque() const				{ return mMouseOpaque; }
	void setToolTip(const std::string& msg);
	bool setToolTipArg(const std::string& key, const std::string& text);
	void setToolTipArgs(const LLStringUtil::format_map_t& args);

	virtual void setRect(const LLRect& rect);

	void setFollows(U8 flags)							{ mReshapeFlags = flags; }
	LL_INLINE void setFollowsNone()						{ mReshapeFlags = FOLLOWS_NONE; }
	LL_INLINE void setFollowsLeft()						{ mReshapeFlags |= FOLLOWS_LEFT; }
	LL_INLINE void setFollowsTop()						{ mReshapeFlags |= FOLLOWS_TOP; }
	LL_INLINE void setFollowsRight()					{ mReshapeFlags |= FOLLOWS_RIGHT; }
	LL_INLINE void setFollowsBottom()					{ mReshapeFlags |= FOLLOWS_BOTTOM; }
	LL_INLINE void setFollowsAll()						{ mReshapeFlags = FOLLOWS_ALL; }

	LL_INLINE void setSoundFlags(U8 flags)				{ mSoundFlags = flags; }
	LL_INLINE void setName(const std::string& name)		{ mName = name; }
	void setUseBoundingRect(bool use_bounding_rect);
	LL_INLINE bool getUseBoundingRect()					{ return mUseBoundingRect; }

	const std::string& getToolTip() const;

	void sendChildToFront(LLView* child);
	void sendChildToBack(LLView* child);
	void moveChildToFrontOfTabGroup(LLUICtrl* child);
	void moveChildToBackOfTabGroup(LLUICtrl* child);

	void addChild(LLView* view, S32 tab_group = 0);
	void addChildAtEnd(LLView* view,  S32 tab_group = 0);
	// Removes the specified child from the view, and sets its parent to NULL.
	void removeChild(LLView* view, bool delete_it = false);

	virtual void addCtrl(LLUICtrl* ctrl, S32 tab_group);
	virtual void addCtrlAtEnd(LLUICtrl* ctrl, S32 tab_group);
	virtual void removeCtrl(LLUICtrl* ctrl);

	LL_INLINE child_tab_order_t getCtrlOrder() const	{ return mCtrlOrder; }
	ctrl_list_t getCtrlList() const;
	ctrl_list_t getCtrlListSorted() const;

	void setDefaultTabGroup(S32 d)						{ mDefaultTabGroup = d; }
	LL_INLINE S32 getDefaultTabGroup() const			{ return mDefaultTabGroup; }

	bool isInVisibleChain() const;
	bool isInEnabledChain() const;

	LL_INLINE void setFocusRoot(bool b)					{ mIsFocusRoot = b; }
	LL_INLINE bool isFocusRoot() const					{ return mIsFocusRoot; }
	LL_INLINE virtual bool canFocusChildren() const		{ return true; }

	bool focusNextRoot();
	bool focusPrevRoot();

	// Deletes all children. Override this function if you need to perform any
	// extra clean up such as cached pointers to selected children, etc.
	virtual void deleteAllChildren();

	LL_INLINE virtual void setTentative(bool b)			{}
	LL_INLINE virtual bool getTentative() const			{ return false; }

	void setAllChildrenEnabled(bool b);

	virtual void setVisible(bool visible);
	LL_INLINE bool getVisible() const					{ return mVisible; }

	LL_INLINE virtual void setEnabled(bool enabled)		{ mEnabled = enabled; }
	LL_INLINE bool getEnabled() const					{ return mEnabled; }

	LL_INLINE U8 getSoundFlags() const					{ return mSoundFlags; }

	LL_INLINE virtual bool setLabelArg(const std::string& key,
									   const std::string& text)
	{
		return false;
	}

	virtual void onVisibilityChange(bool cur_visibility_in);

	LL_INLINE void pushVisible(bool visible)
	{
		mLastVisible = mVisible;
		setVisible(visible);
	}

	LL_INLINE void popVisible()
	{
		setVisible(mLastVisible);
		mLastVisible = true;
	}

	LL_INLINE U8 getFollows() const						{ return mReshapeFlags; }
	LL_INLINE bool followsLeft() const					{ return (mReshapeFlags & FOLLOWS_LEFT) != 0; }
	LL_INLINE bool followsRight() const					{ return (mReshapeFlags & FOLLOWS_RIGHT) != 0; }
	LL_INLINE bool followsTop() const					{ return (mReshapeFlags & FOLLOWS_TOP) != 0; }
	LL_INLINE bool followsBottom() const				{ return (mReshapeFlags & FOLLOWS_BOTTOM) != 0; }
	LL_INLINE bool followsAll() const					{ return mReshapeFlags == FOLLOWS_ALL; }

	LL_INLINE const LLRect& getRect() const				{ return mRect; }
	LL_INLINE const LLRect& getBoundingRect() const		{ return mBoundingRect; }
	LLRect getLocalBoundingRect() const;
	LLRect getScreenRect() const;
	LLRect getLocalRect() const;
	LL_INLINE virtual LLRect getSnapRect() const		{ return mRect; }

	LLRect getLocalSnapRect() const;
	// Override and return required size for this object. 0 for width/height
	// means do not care.
	LL_INLINE virtual LLRect getRequiredRect()			{ return mRect; }

	void updateBoundingRect();

	LLView* getRootView();
	LL_INLINE LLView* getParent() const					{ return mParentView; }

	LL_INLINE LLView* getFirstChild() const
	{
		return mChildList.empty() ? NULL : *(mChildList.begin());
	}

	LLView* findPrevSibling(LLView* child);
	LLView* findNextSibling(LLView* child);

	LL_INLINE S32 getChildCount() const					{ return mChildListSize; }

	template<class T> void sortChildren(T compare_fn)	{ mChildList.sort(compare_fn); }

	bool hasAncestor(const LLView* parentp) const;

	bool childHasKeyboardFocus(const char* childname) const;

	// Default behavior is to use reshape flags to resize child views
	virtual void reshape(S32 width, S32 height, bool from_parent = true);
	virtual void translate(S32 x, S32 y);

	LL_INLINE void setOrigin(S32 x, S32 y)
	{
		mRect.translate(x - mRect.mLeft, y - mRect.mBottom);
	}

	bool translateIntoRect(const LLRect& constraint, bool allow_part_outside);
	void centerWithin(const LLRect& bounds);

	virtual void userSetShape(const LLRect& new_rect);
	virtual LLView* findSnapRect(LLRect& new_rect, const LLCoordGL& mouse_dir,
								 LLView::ESnapType snap_type, S32 threshold,
								 S32 padding = 0);
	virtual LLView*	findSnapEdge(S32& new_edge_val, const LLCoordGL& mouse_dir,
								 ESnapEdge snap_edge, ESnapType snap_type,
								 S32 threshold, S32 padding = 0);

	virtual bool canSnapTo(LLView* other_viewp);

	LL_INLINE virtual void snappedTo(LLView* viewp)		{}

	LL_INLINE virtual bool getIsChrome() const			{ return false; }

	// Inherited from LLFocusableElement
	LL_INLINE bool isUICtrl() override 					{ return false; }
	bool handleKey(KEY key, MASK mask, bool from_parent) override;
	bool handleKeyUp(KEY key, MASK mask, bool from_parent) override;

	bool handleUnicodeChar(llwchar uni_char, bool from_parent) override;

	virtual bool handleDragAndDrop(S32 x, S32 y, MASK mask, bool drop,
								   EDragAndDropType cargo_type,
								   void* cargo_data, EAcceptance* accept,
								   std::string& tooltip_msg);

	std::string getShowNamesToolTip();

	virtual void draw();

	virtual LLXMLNodePtr getXML(bool save_children = true) const;
	//FIXME: make LLView non-instantiable from XML
	static LLView* fromXML(LLXMLNodePtr node, LLView* parent,
						   class LLUICtrlFactory* factory);
	virtual void initFromXML(LLXMLNodePtr node, LLView* parent);
	void parseFollowsFlags(LLXMLNodePtr node);

	// Some widgets, like close box buttons, don't need to be saved
	LL_INLINE bool getSaveToXML() const					{ return mSaveToXML; }
	LL_INLINE void setSaveToXML(bool b)					{ mSaveToXML = b; }

	// Inherited from LLFocusableElement
	LL_INLINE void onFocusLost() override				{}
	LL_INLINE void onFocusReceived() override			{}

	typedef enum e_hit_test_type
	{
		HIT_TEST_USE_BOUNDING_RECT,
		HIT_TEST_IGNORE_BOUNDING_RECT
	} EHitTestType;

	bool parentPointInView(S32 x, S32 y,
						   EHitTestType type = HIT_TEST_USE_BOUNDING_RECT) const;
	bool pointInView(S32 x, S32 y,
					 EHitTestType type = HIT_TEST_USE_BOUNDING_RECT) const;

	bool blockMouseEvent(S32 x, S32 y) const;

	// See LLMouseHandler virtuals for screenPointToLocal and localPointToScreen
	bool localPointToOtherView(S32 x, S32 y, S32* other_x, S32* other_y,
							   LLView* other_view) const;
	bool localRectToOtherView(const LLRect& local, LLRect* other,
							  LLView* other_view) const;
	void screenRectToLocal(const LLRect& screen, LLRect* local) const;
	void localRectToScreen(const LLRect& local, LLRect* screen) const;

	// Listener dispatching functions (dispatcher deletes pointers to listeners
	// on deregistration or destruction)
	LLOldEvents::LLSimpleListener* getListenerByName(const std::string& cb_name);
	void registerEventListener(const std::string& name,
							   LLOldEvents::LLSimpleListener* function);
	void deregisterEventListener(const std::string& name);
	std::string findEventListener(LLOldEvents::LLSimpleListener* listener) const;
	void addListenerToControl(LLOldEvents::LLEventDispatcher* observer,
							  const std::string& name, LLSD filter,
							  LLSD userdata);

	void addBoolControl(const std::string& name, bool initial_value);
	LLControlVariable* getControl(const std::string& name);
	LLControlVariable* findControl(const std::string& name);

	bool setControlValue(const LLSD& value);
	virtual void setControlName(const char* control, LLView* contextp);

	LL_INLINE virtual const std::string& getControlName() const
	{
		return mControlName;
	}

#if 0
	virtual bool handleEvent(LLPointer<LLOldEvents::LLEvent> event,
							 const LLSD& userdata);
#endif

	LL_INLINE virtual void setValue(const LLSD& value)	{}
	LL_INLINE virtual LLSD getValue() const				{ return LLSD(); }


	LL_INLINE const child_list_t* getChildList() const	{ return &mChildList; }

	// LLMouseHandler functions
	//  Default behavior is to pass events to children
	bool handleHover(S32 x, S32 y, MASK mask) override;
	bool handleMouseUp(S32 x, S32 y, MASK mask) override;
	bool handleMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleMiddleMouseUp(S32 x, S32 y, MASK mask) override;
	bool handleMiddleMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleDoubleClick(S32 x, S32 y, MASK mask) override;
	bool handleScrollWheel(S32 x, S32 y, S32 clicks) override;
	bool handleRightMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleRightMouseUp(S32 x, S32 y, MASK mask) override;
	// Display tooltip if no child handles it.
	bool handleToolTip(S32 x, S32 y, std::string& msg, LLRect* rect) override;

	std::string getName() const override;

	LL_INLINE void onMouseCaptureLost() override		{}
	bool hasMouseCapture() override;

	void screenPointToLocal(S32 screen_x, S32 screen_y,
							S32* local_x, S32* local_y) const override;
	void localPointToScreen(S32 local_x, S32 local_y,
							S32* screen_x, S32* screen_y) const override;

	template<class T> T* getChild(const char* name, bool recurse = true,
								  bool create_if_missing = true) const
	{
		LLView* child = getChildView(name, recurse, false);
		T* result = dynamic_cast<T*>(child);
		if (!result)
		{
			// Did we find *something* with that name ?
			if (child)
			{
				llwarns << "Found child named " << name
						<< " but of wrong type " << typeid(child).name()
						<< ", expecting " << typeid(T).name() << llendl;
			}
			if (create_if_missing)
			{
				// Create dummy widget instance here
				result = createDummyWidget<T>(name);
			}
		}
		return result;
	}

	virtual LLView* getChildView(const char* name, bool recurse = true,
								 bool create_if_missing = true) const;

	template<class T> T* createDummyWidget(const char* name) const
	{
		T* widget = getDummyWidget<T>(name);
		if (!widget)
		{
			// Get xml tag name corresponding to requested widget type (e.g.
			// "button")
			std::string xml_tag =
				LLWidgetClassRegistry::getInstance()->getTag<T>();
			if (xml_tag.empty())
			{
				llwarns << "No xml tag registered for this class " << llendl;
				return NULL;
			}
			// Create dummy xml node (<button name="foo"/>)
			LLXMLNodePtr new_node_ptr = new LLXMLNode(xml_tag.c_str(), false);
			new_node_ptr->createChild("name", true)->setStringValue(name);

			widget = dynamic_cast<T*>(createWidget(new_node_ptr));
			if (widget)
			{
				// Need non-const to update private dummy widget cache
				llwarns << "Making dummy " << xml_tag << " named '" << name
						<< "' in " << getName() << llendl;
				mDummyWidgets.emplace(name, widget);
			}
			else
			{
				// Dynamic cast will fail if T::fromXML only registered for
				// base class
				llwarns << "Failed to create dummy widget of requested type ("
						<< xml_tag << ") named '" << name << "'" << " in "
						<< getName() << llendl;
				return NULL;
			}
		}
		return widget;
	}

	template<class T> T* getDummyWidget(const char* wname) const
	{
		std::string name(wname);
		widget_map_t::const_iterator it = mDummyWidgets.find(name);
		if (it == mDummyWidgets.end())
		{
			return NULL;
		}
		return dynamic_cast<T*>(it->second);
	}

	LLView* createWidget(LLXMLNodePtr xml_node) const;

	static U32 createRect(LLXMLNodePtr node, LLRect& rect, LLView* parent_view,
						  const LLRect& required_rect = LLRect());

	static LLFontGL* selectFont(LLXMLNodePtr node);
	static LLFontGL::HAlign selectFontHAlign(LLXMLNodePtr node);
	static LLFontGL::VAlign selectFontVAlign(LLXMLNodePtr node);
	static LLFontGL::StyleFlags selectFontStyle(LLXMLNodePtr node);

	// Only saves color if different from default setting.
	static void addColorXML(LLXMLNodePtr node, const LLColor4& color,
							const char* xml_name, const char* control_name);
	// Escapes " (quot) ' (apos) & (amp) < (lt) > (gt)
	static LLWString escapeXML(const LLWString& xml);

	// Same as above, but wraps multiple lines in quotes and prepends indent as
	// leading white space on each line
	static std::string escapeXML(const std::string& xml, std::string& indent);

	// Focuses the item in the list after the currently-focused item, wrapping
	// if necessary
	static  bool focusNext(LLView::child_list_t& result);
	// Focuses the item in the list before the currently-focused item, wrapping
	// if necessary
	static  bool focusPrev(LLView::child_list_t& result);

	// Returns query for iterating over controls in tab order
	static const LLCtrlQuery& getTabOrderQuery();
	// Returns query for iterating over focus roots in tab order
	static const LLCtrlQuery& getFocusRootsQuery();

	static bool deleteViewByHandle(LLHandle<LLView> handle);

protected:
	LL_INLINE virtual bool handleKeyHere(KEY key, MASK mask)
	{
		// Checking parents and children happens in handleKey()
		return false;
	}

	LL_INLINE virtual bool handleKeyUpHere(KEY key, MASK mask)
	{
		return false;
	}

	LL_INLINE virtual bool handleUnicodeCharHere(llwchar uni_char)
	{
		return false;
	}

	void drawDebugRect();
	void drawChild(LLView* childp, S32 x_offset = 0, S32 y_offset = 0,
				   bool force_draw = false);

	LLView*	childrenHandleKey(KEY key, MASK mask);
	LLView*	childrenHandleKeyUp(KEY key, MASK mask);
	LLView* childrenHandleUnicodeChar(llwchar uni_char);
	LLView*	childrenHandleDragAndDrop(S32 x, S32 y, MASK mask, bool drop,
									  EDragAndDropType type, void* data,
									  EAcceptance* accept,
									  std::string& tooltip_msg);

	LLView*	childrenHandleHover(S32 x, S32 y, MASK mask);
	LLView* childrenHandleMouseUp(S32 x, S32 y, MASK mask);
	LLView* childrenHandleMouseDown(S32 x, S32 y, MASK mask);
	LLView* childrenHandleMiddleMouseUp(S32 x, S32 y, MASK mask);
	LLView* childrenHandleMiddleMouseDown(S32 x, S32 y, MASK mask);
	LLView* childrenHandleDoubleClick(S32 x, S32 y, MASK mask);
	LLView*	childrenHandleScrollWheel(S32 x, S32 y, S32 clicks);
	LLView* childrenHandleRightMouseDown(S32 x, S32 y, MASK mask);
	LLView* childrenHandleRightMouseUp(S32 x, S32 y, MASK mask);

	static bool controlListener(const LLSD& newvalue, LLHandle<LLView> handle,
								const std::string& type);

protected:
	typedef fast_hmap<std::string, LLControlVariable*> control_map_t;
	control_map_t		mControls;

private:
	LLView*				mParentView;

	LLUIString*			mToolTipMsgPtr;

	std::string			mName;
	std::string			mControlName;

	child_list_t		mChildList;
	child_tab_order_t	mCtrlOrder;

	typedef boost::signals2::connection signal_connection_t;
	signal_connection_t	 mControlConnection;

	typedef fast_hmap<std::string,
					  LLPointer<LLOldEvents::LLSimpleListener> > dispatch_list_t;
	dispatch_list_t		mDispatchList;

	typedef fast_hmap<std::string, LLView*> widget_map_t;
	mutable widget_map_t mDummyWidgets;

	// Location in pixels, relative to surrounding structure, bottom,left=0,0
	LLRect				mRect;
	LLRect				mBoundingRect;

	ECursorType			mHoverCursor;

	S32					mDefaultTabGroup;
	S32					mChildListSize;

	S32					mNextInsertionOrdinal;

	U8					mReshapeFlags;

	U8					mSoundFlags;
	bool				mSaveToXML;

	bool				mIsFocusRoot;

	 // Hit test against bounding rectangle that includes all child elements
	bool				mUseBoundingRect;

	bool				mLastVisible;
	bool				mVisible;

	// Enabled means 'accepts input that has an effect on the state of the
	// application.' A disabled view, for example, may still have a scrollbar
	// that responds to mouse events.
	bool				mEnabled;

	// Opaque views handle all mouse events that are over their rect.
	bool				mMouseOpaque;

	// All root views must know about their window.
	static LLWindow*	sWindow;

public:
	static LLView*		sEditingUIView;
	static std::string	sMouseHandlerMessage;
	static S32			sDepth;
	static S32			sSelectID;
	static S32			sLastLeftXML;
	static S32			sLastBottomXML;
	static bool			sEditingUI;
	static bool			sDebugRects;	// Draw debug rects behind everything.
	static bool			sDebugKeys;
	static bool			sDebugMouseHandling;
	static bool			sForceReshape;
};

class LLCompareByTabOrder
{
public:
	LL_INLINE LLCompareByTabOrder(LLView::child_tab_order_t order)
	:	mTabOrder(order)
	{
	}

	virtual ~LLCompareByTabOrder() = default;

	bool operator()(const LLView* const a, const LLView* const b) const;

private:
	LL_INLINE virtual bool compareTabOrders(const LLView::tab_order_t& a,
											const LLView::tab_order_t& b) const
	{
		return a < b;
	}

private:
	LLView::child_tab_order_t mTabOrder;
};

#endif //LL_LLVIEW_H
