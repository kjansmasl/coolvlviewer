/**
 * @file llfolderview.h
 * @brief Definition of the folder view collection of classes.
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

// The folder view collection of classes provides an interface for making a
// 'folder view' similar to the way the a single pane file folder interface
// works.

#ifndef LL_LLFOLDERVIEW_H
#define LL_LLFOLDERVIEW_H

#include <deque>
#include <vector>

#include "llcolor4.h"
#include "lleditmenuhandler.h"
#include "llfoldertype.h"
#include "llpanel.h"

#include "lltooldraganddrop.h"
#include "llviewertexture.h"

class LLFolderView;
class LLFolderViewFolder;
class LLFolderViewItem;
class LLFontGL;
class LLInventoryModel;
class LLInventoryObserver;
class LLInventoryPanel;
class LLInvFVBridge;
class LLLineEditor;
class LLMenuGL;
class LLScrollableContainer;
class LLUICtrl;
class LLUICtrlFactory;

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLFolderViewEventListener
//
// This is an abstract base class that users of the folderview classes would
// use to catch the useful events emitted from the folder views.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLFolderViewEventListener
{
protected:
	LOG_CLASS(LLFolderViewEventListener);

public:
	virtual ~LLFolderViewEventListener() = default;

	virtual const std::string& getName() const = 0;
	virtual const std::string& getDisplayName() const = 0;
	virtual const LLUUID& getUUID() const = 0;
	virtual time_t getCreationDate() const = 0;	// UTC seconds
	virtual PermissionMask getPermissionMask() const = 0;
	virtual LLFolderType::EType getPreferredType() const = 0;
	virtual LLUIImagePtr getIcon() const = 0;
	virtual LLFontGL::StyleFlags getLabelStyle() const = 0;
	virtual std::string getLabelSuffix() const = 0;
	virtual void openItem() = 0;
	virtual void previewItem() = 0;
	virtual void selectItem() = 0;
	virtual void showProperties() = 0;
	virtual bool isItemRenameable() const = 0;
	virtual bool renameItem(const std::string& new_name) = 0;
	virtual bool isItemMovable() = 0;	// Can be moved to another folder
	virtual bool isItemRemovable() = 0;	// Can be destroyed
	virtual bool removeItem() = 0;
	virtual void removeBatch(std::vector<LLFolderViewEventListener*>& batch) = 0;
	virtual void move(LLFolderViewEventListener* parent_listener) = 0;
	virtual bool isItemCopyable() const = 0;
	virtual bool copyToClipboard() const = 0;
	virtual bool cutToClipboard() const = 0;
	virtual bool isClipboardPasteable() const = 0;
	virtual void pasteFromClipboard() = 0;
	virtual void pasteLinkFromClipboard() = 0;
	virtual void buildContextMenu(LLMenuGL& menu, U32 flags) = 0;
	virtual bool isUpToDate() const = 0;
	LL_INLINE virtual bool hasChildren() const 			{ return false; }
	virtual LLInventoryType::EType getInventoryType() const = 0;
	virtual S32 getSubType() const = 0;

	LL_INLINE virtual void performAction(LLFolderView* folderp,
										 LLInventoryModel* modelp,
										 const std::string& action)
	{
	}

	// This method should be called when a drag begins. Returns true if the
	// drag can begin, otherwise false.
	virtual bool startDrag(EDragAndDropType* type, LLUUID* id) const = 0;

	// This method will be called to determine if a drop can be performed, and
	// will set drop to true if a drop is requested. Returns true if a drop is
	// possible/happened, otherwise false.
	virtual bool dragOrDrop(MASK mask, bool drop,
							EDragAndDropType cargo_type,
							void* cargo_data, std::string& tooltip_msg) = 0;

	// This method accesses the parent and arranges and sets it as specified.
	void arrangeAndSet(LLFolderViewItem* focusp, bool set_selection,
					   bool take_keyboard_focus = true);

	// Cancels any existing tip. May be called with not_drop_msg = true to
	// cancel only drag-related message tips (but not drop-related ones).
	static void cancelTip(bool not_drop_msg = false);

	// This static method should be called after each call to dragOrDrop() so
	// to deal with tooltip_msg displaying (as notification tips) at the
	// folder view level.
	static void dragOrDropTip(bool drop, const std::string& tooltip_msg);

private:
	static LLUUID		sLastDragTipID;
	static std::string	sLastDragTipMsg;
	// true when last message came from a drop event:
	static bool			sLastDragTipDrop;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLFolderViewListenerFunctor
//
// This simple abstract base class can be used to applied to all listeners in a
// hierarchy.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLFolderViewListenerFunctor
{
public:
	virtual ~LLFolderViewListenerFunctor() = default;
	virtual void operator()(LLFolderViewEventListener* listener) = 0;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLFolderViewFunctor
//
// Simple abstract base class for applying a functor to folders and items in a
// folder view hierarchy. This is suboptimal for algorithms that only work
// folders or only work on items, but I'll worry about that later when it's
// determined to be too slow.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLFolderViewFunctor
{
public:
	virtual ~LLFolderViewFunctor() = default;
	virtual void doFolder(LLFolderViewFolder* folder) = 0;
	virtual void doItem(LLFolderViewItem* item) = 0;
};

class LLSelectFirstFilteredItem final : public LLFolderViewFunctor
{
public:
	LL_INLINE LLSelectFirstFilteredItem()
	:	mItemSelected(false)
	{
	}

	void doFolder(LLFolderViewFolder* folder) override;
	void doItem(LLFolderViewItem* item) override;

	LL_INLINE bool wasItemSelected()					{ return mItemSelected; }

protected:
	bool mItemSelected;
};

class LLSaveFolderState final : public LLFolderViewFunctor
{
public:
	LL_INLINE LLSaveFolderState()
	:	mApply(false)
	{
	}

	void doFolder(LLFolderViewFolder* folder) override;
	LL_INLINE void doItem(LLFolderViewItem*) override	{}

	void setApply(bool apply);
	LL_INLINE void clearOpenFolders()					{ mOpenFolders.clear(); }

protected:
	std::set<LLUUID> mOpenFolders;
	bool mApply;
};

class LLOpenFilteredFolders final : public LLFolderViewFunctor
{
public:
	LLOpenFilteredFolders() = default;
	void doFolder(LLFolderViewFolder* folder) override;
	void doItem(LLFolderViewItem* item) override;
};

class LLOpenFoldersWithSelection final : public LLFolderViewFunctor
{
public:
	LLOpenFoldersWithSelection() = default;
	void doFolder(LLFolderViewFolder* folder) override;
	void doItem(LLFolderViewItem* item) override;
};

class LLOpenFolderByID final : public LLFolderViewFunctor
{
public:
	LL_INLINE LLOpenFolderByID(const LLUUID& id)
	: mID(id)
	{
	}

	void doFolder(LLFolderViewFolder* folder) override;

	// Do nothing
	LL_INLINE void doItem(LLFolderViewItem*) override	{}

protected:
	const LLUUID& mID;
};

class LLInventoryFilter
{
protected:
	LOG_CLASS(LLInventoryFilter);

public:
	typedef enum e_folder_show
	{
		SHOW_ALL_FOLDERS,
		SHOW_NON_EMPTY_FOLDERS,
		SHOW_NO_FOLDERS
	} EFolderShow;

	typedef enum e_filter_behavior
	{
		// Nothing to do, already filtered
		FILTER_NONE,
		// Restart filtering from scratch
		FILTER_RESTART,
		// Existing filtered items will certainly pass this filter
		FILTER_LESS_RESTRICTIVE,
		// If you did not pass the previous filter, you definitely woiuld not
		// pass this one
		FILTER_MORE_RESTRICTIVE
	} EFilterBehavior;

	static constexpr U32 SO_DATE = 1;
	static constexpr U32 SO_FOLDERS_BY_NAME = 2;
	static constexpr U32 SO_SYSTEM_FOLDERS_TO_TOP = 4;

	LLInventoryFilter(const std::string& name);
	virtual ~LLInventoryFilter() = default;

	void setFilterTypes(U32 types);
	LL_INLINE U32 getFilterTypes() const				{ return mFilterOps.mFilterTypes; }

	void setFilterSubString(const std::string& string);

	LL_INLINE const std::string getFilterSubString(bool trim = false)
	{
		return mFilterSubString;
	}

	LL_INLINE void setFilterHideLibrary(bool hide)		{ mHideLibrary = hide; }

	LL_INLINE void setFilterSubType(S32 subtype)		{ mFilterSubType = subtype; }
	LL_INLINE bool getFilterSubType() const				{ return mFilterSubType; }

	LL_INLINE void setFilterWorn(bool worn)				{ mFilterWorn = worn; }
	LL_INLINE bool getFilterWorn() const				{ return mFilterWorn; }

	LL_INLINE void setFilterLastOpen(bool b)			{ mFilterLastOpen = b; }
	LL_INLINE bool getFilterLastOpen() const			{ return mFilterLastOpen; }

	LL_INLINE void setFilterShowLinks(bool b)			{ mFilterShowLinks = b; }
	LL_INLINE bool getFilterShowLinks() const			{ return mFilterShowLinks; }

	void setFilterPermissions(PermissionMask perms);

	LL_INLINE PermissionMask getFilterPermissions() const
	{
		return mFilterOps.mPermissions;
	}

	void setDateRange(time_t min_date, time_t max_date);
	void setDateRangeLastLogoff(bool sl);
	LL_INLINE time_t getMinDate() const					{ return mFilterOps.mMinDate; }
	LL_INLINE time_t getMaxDate() const					{ return mFilterOps.mMaxDate; }

	void setHoursAgo(U32 hours);
	LL_INLINE U32 getHoursAgo() const					{ return mFilterOps.mHoursAgo; }

	void setShowFolderState(EFolderShow state);
	LL_INLINE EFolderShow getShowFolderState()			{ return mFilterOps.mShowFolderState; }

	void setSortOrder(U32 order);
	LL_INLINE U32 getSortOrder()						{ return mOrder; }

	bool check(LLFolderViewItem* item);
	LL_INLINE size_t getStringMatchOffset() const		{ return mSubStringMatchOffset; }

	bool isActive();
	bool isNotDefault();
	LL_INLINE bool isModified()							{ return mModified; }

	LL_INLINE void clearModified()
	{
		mModified = false;
		mFilterBehavior = FILTER_NONE;
	}

	LL_INLINE bool isModifiedAndClear()
	{
		bool ret = mModified;
		mModified = false;
		return ret;
	}

	bool isSinceLogoff();

	LL_INLINE const std::string getName() const			{ return mName; }
	std::string getFilterText();

	void setFilterCount(S32 count)						{ mFilterCount = count; }
	LL_INLINE S32 getFilterCount()						{ return mFilterCount; }
	LL_INLINE void decrementFilterCount()				{ --mFilterCount; }

	void markDefault();
	void resetDefault();

	bool isFilterWith(LLInventoryType::EType t);

	LL_INLINE S32 getCurrentGeneration() const			{ return mFilterGeneration; }
	LL_INLINE S32 getMinRequiredGeneration() const		{ return mMinRequiredGeneration; }
	LL_INLINE S32 getMustPassGeneration() const			{ return mMustPassGeneration; }

	// RN: this is public to allow system to externally force a global
	// re-filter
	void setModified(EFilterBehavior behavior = FILTER_RESTART);
	void setLastOpenID(const LLUUID& folder_id)			{ mLastOpenID = folder_id; }

	void toLLSD(LLSD& data);
	void fromLLSD(LLSD& data);

private:
	LLUUID				mLastOpenID;
	std::string			mFilterText;
	U32					mLastLogoff;
	bool				mModified;
	bool				mNeedTextRebuild;

protected:
	U32					mOrder;
	S32					mFilterGeneration;
	S32					mMustPassGeneration;
	S32					mMinRequiredGeneration;
	S32					mFilterCount;
	S32					mNextFilterGeneration;
	EFilterBehavior		mFilterBehavior;

	struct filter_ops
	{
		U32				mFilterTypes;
		time_t			mMinDate;
		time_t			mMaxDate;
		U32				mHoursAgo;
		EFolderShow		mShowFolderState;
		PermissionMask	mPermissions;
	};
	filter_ops			mFilterOps;
	filter_ops			mDefaultFilterOps;

	size_t				mSubStringMatchOffset;
	std::string			mFilterSubString;

	const std::string	mName;

	S32					mFilterSubType;

	bool				mHideLibrary;
	bool				mFilterWorn;
	bool				mFilterLastOpen;
	bool				mFilterShowLinks;
};

// These are grouping of inventory types. Order matters when sorting system
// folders to the top.
enum EInventorySortGroup
{
	SG_SYSTEM_FOLDER,
	SG_TRASH_FOLDER,
	SG_NORMAL_FOLDER,
	SG_ITEM
};

class LLInventorySort
{
public:
	LLInventorySort()
	:	mSortOrder(0),
		mByDate(false),
		mSystemToTop(false),
		mFoldersByName(false)
	{
	}

	// Returns true if order has changed
	bool updateSort(U32 order);
	LL_INLINE U32 getSort()								{ return mSortOrder; }

	bool operator()(const LLFolderViewItem* const& a,
					const LLFolderViewItem* const& b);

private:
	U32  mSortOrder;
	bool mByDate;
	bool mSystemToTop;
	bool mFoldersByName;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLFolderViewItem
//
// An instance of this class represents a single item in a folder view
// such as an inventory item or a file.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLFolderViewItem : public LLUICtrl
{
	friend class LLFolderViewEventListener;

protected:
	LOG_CLASS(LLFolderViewItem);

	// Helper method to change the selection from the root.
	void changeSelectionFromRoot(LLFolderViewItem* selection, bool selected);

	// Helper method to change the selection from the root.
	void extendSelectionFromRoot(LLFolderViewItem* selection);

	// This is an internal method used for adding items to folders. A no-op at
	// this level, but reimplemented in derived classes.
	virtual bool addItem(LLFolderViewItem*)				{ return false; }
	virtual bool addFolder(LLFolderViewFolder*)			{ return false; }

public:
	// This method clears the currently selected item and records the specified
	// selected item appropriately for display and use in the UI. If 'openitem'
	// is true, then folders are opened up along the way to the selection.
	void setSelectionFromRoot(LLFolderViewItem* selection, bool openitem,
							  bool take_keyboard_focus = true);

	static void initClass();
	static void cleanupClass();
	static void refreshCachedSettings();

	// This method is called when the folder view is dirty. It is implemented
	// here but called by derived classes when folding the views.
	void arrangeFromRoot();
	void filterFromRoot();

	// Note: 'creation_date' is in UTC seconds
	LLFolderViewItem(const std::string& name, LLUIImagePtr icon,
					 S32 creation_date, LLFolderView* root,
					 LLFolderViewEventListener* listener);

	virtual ~LLFolderViewItem();

	// Returns true on success, false otherwise
	virtual bool addToFolder(LLFolderViewFolder* folder, LLFolderView* root);

	virtual EInventorySortGroup getSortGroup() const;

	// Finds width and height of this object and it's children. Also makes sure
	// that this view and it's children are the right size.
	virtual S32 arrange(S32* width, S32* height, S32 filter_generation);
	virtual S32 getItemHeight();

	// Applies filters to control visibility of inventory items
	virtual void filter(LLInventoryFilter& filter);

	// Updates filter serial number and optionally propagated value up to root
	LL_INLINE S32 getLastFilterGeneration()				{ return mLastFilterGeneration; }

	virtual void dirtyFilter();

	// If 'selection' is 'this' then note that otherwise ignore.
	// Returns true if this item ends up being selected.
	virtual bool setSelection(LLFolderViewItem* selection, bool openitem,
							  bool take_keyboard_focus);

	// This method is used to set the selection state of an item.
	// If 'selection' is 'this' then note selection.
	// Returns true if the selection state of this item was changed.
	virtual bool changeSelection(LLFolderViewItem* selection, bool selected);

	// This method is used to group select items
	virtual void extendSelection(LLFolderViewItem* selection,
								 LLFolderViewItem* last_selected,
								 std::vector<LLFolderViewItem*>& items)
	{
	}

	// This method is used to deselect this element
	void deselectItem();

	// This method is used to select this element
	void selectItem();

	// Gets multiple-element selection in an UUID set
	LL_INLINE virtual bool getSelectionList(uuid_list_t& selection)
	{
		return true;
	}
	// Gets multiple-element selection in an UUID vector
	LL_INLINE virtual bool getSelection(uuid_vec_t& selection)
	{
		return true;
	}

	// Returns true is this object and all of its children can be removed
	// (deleted by user)
	virtual bool isRemovable();

	// Returns true is this object and all of its children can be moved
	virtual bool isMovable();

	// destroys this item recursively
	virtual void destroyView();

	LL_INLINE bool isSelected() const					{ return mIsSelected; }

	LL_INLINE void setIsCurSelection(bool select)		{ mIsCurSelection = select; }

	LL_INLINE bool getIsCurSelection()					{ return mIsCurSelection; }

	LL_INLINE void disableDoubleClick(bool b = true)	{ mDoubleClickDisabled = b; }

	LL_INLINE bool hasVisibleChildren()					{ return mHasVisibleChildren; }

#if 0
	// Call through to the viewed object and return true if it can be removed.
	// Returns true if it is removed.
	virtual bool removeRecursively(bool single_item);
#endif
	bool remove();

	// Build an appropriate context menu for the item.	Flags unused.
	void buildContextMenu(LLMenuGL& menu, U32 flags);

	// This method returns the actual name of the thing being viewed. This
	// method will ask the viewed object itself.
	std::string getName() const override;

	std::string getSearchableData();

	// This method returns the label displayed on the view. This
	// method was primarily added to allow sorting on the folder
	// contents possible before the entire view has been constructed.
	LL_INLINE const std::string& getLabel() const		{ return mLabel; }

	// Used for sorting, like getLabel() above.
	LL_INLINE virtual time_t getCreationDate() const	{ return mCreationDate; }

	LL_INLINE LLFolderViewFolder* getParentFolder()		{ return mParentFolder; }

	LL_INLINE const LLFolderViewFolder* getParentFolder() const
	{
		return mParentFolder;
	}

	LLFolderViewItem* getNextOpenNode(bool include_children = true);
	LLFolderViewItem* getPreviousOpenNode(bool include_children = true);

	LL_INLINE const LLFolderViewEventListener* getListener() const
	{
		return mListener;
	}

	LL_INLINE LLFolderViewEventListener* getListener()	{ return mListener; }

	// Renames the object.
	void rename(const std::string& new_name);

	virtual void openItem();
	virtual void preview();

	// Show children (unfortunate that this is called "open")
	LL_INLINE virtual void setOpen(bool open = true)	{}

	LL_INLINE virtual bool isOpen()						{ return false; }

	LL_INLINE LLFolderView* getRoot()					{ return mRoot; }

	bool isDescendantOf(const LLFolderViewFolder* potential_ancestor);
	LL_INLINE S32 getIndentation()						{ return mIndentation; }

	// Do we know for a fact that this item has been filtered out ?
	virtual bool potentiallyVisible();

	virtual bool getFiltered();
	virtual bool getFiltered(S32 filter_generation);
	virtual void setFiltered(bool filtered, S32 filter_generation);

	// Changes the icon
	LL_INLINE void setIcon(LLUIImagePtr icon)			{ mIcon = icon; }

	// Refreshes information from the object being viewed.
	void refreshFromListener();
	virtual void refresh();

	virtual void applyListenerFunctorRecursively(LLFolderViewListenerFunctor& functor);

	// LLView functionality
	bool handleRightMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleHover(S32 x, S32 y, MASK mask) override;
	bool handleMouseUp(S32 x, S32 y, MASK mask) override;
	bool handleDoubleClick(S32 x, S32 y, MASK mask) override;
	bool handleScrollWheel(S32 x, S32 y, S32 clicks) override;

	void draw() override;
	bool handleDragAndDrop(S32 x, S32 y, MASK mask, bool drop,
						   EDragAndDropType cargo_type, void* cargo_data,
						   EAcceptance* accept, std::string& tooltip) override;

private:
	static void connectRefreshCachedSettingsSafe(const char* name);

	// Returns the font to use to render and adjust the style if necessary
	// (avoids making up artificially, from the default font, bold and italic
	// fonts when they exist). HB
	const LLFontGL* getRenderFont(U32& style);

protected:
	LLFolderView*				mRoot;
	LLFolderViewFolder*			mParentFolder;
	LLFolderViewEventListener*	mListener;

	LLUIImagePtr				mIcon;

	LLTimer						mTimeSinceRequestStart;

	size_t						mStringMatchOffset;

	S32							mIndentation;
	S32							mLastFilterGeneration;
	U32							mCreationDate;

	F32							mControlLabelRotation;
	S32							mLabelWidth;

	LLFontGL::StyleFlags		mLabelStyle;

	std::string					mLabel;
	LLWString					mWLabel;
	std::string					mSearchableLabel;
	std::string					mSearchableDesc;
	std::string					mSearchableCreator;
	std::string					mType;
	std::string					mLabelSuffix;
	LLWString					mWLabelSuffix;

	bool						mHasDescription;
	bool						mIsCurSelection;
	bool						mSelectPending;
	bool						mHasVisibleChildren;
	bool						mFiltered;
	bool						mDragAndDropTarget;
	bool						mIsLoading;

	static const LLFontGL*		sFont;
	static const LLFontGL*		sFontItalic;
	static F32					sFontLineHeight;
	static S32					sFontLineHeightRounded;
	static LLColor4				sFgColor;
	static LLColor4				sHighlightBgColor;
	static LLColor4				sHighlightFgColor;
	static LLColor4				sContextMenuBgColor;
	static LLColor4				sFilterBGColor;
	static LLColor4				sFilterTextColor;
	static LLColor4				sSuffixColor;
	static LLColor4				sSearchStatusColor;
	static LLUIImagePtr			sArrowImage;
	static LLWString			sLoadingStr;

private:
	bool						mIsSelected;
	bool						mDoubleClickDisabled;
};

// function used for sorting.
typedef bool (*sort_order_f)(LLFolderViewItem* a, LLFolderViewItem* b);

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLFolderViewFolder
//
// An instance of an LLFolderViewFolder represents a collection of
// more folders and items. This is used to build the hierarchy of
// items found in the folder view. :)
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLFolderViewFolder : public LLFolderViewItem
{
protected:
	LOG_CLASS(LLFolderViewFolder);

public:
	typedef enum e_recurse_type
	{
		RECURSE_NO,
		RECURSE_UP,
		RECURSE_DOWN,
		RECURSE_UP_DOWN
	} ERecurseType;

	LLFolderViewFolder(const std::string& name, LLUIImagePtr icon,
					   LLFolderView* rootp,
					   LLFolderViewEventListener* listenerp);

	~LLFolderViewFolder() override;

	bool potentiallyVisible() override;

	LLFolderViewItem* getNextFromChild(LLFolderViewItem*,
									   bool include_children = true);
	LLFolderViewItem* getPreviousFromChild(LLFolderViewItem*,
										   bool include_children = true);

	// Returns true on success, false otherwise
	bool addToFolder(LLFolderViewFolder* folderp,
					 LLFolderView* rootp) override;

	// Finds width and height of this object and it's children. Also makes sure
	// that this view and it's children are the right size.
	S32 arrange(S32* width, S32* height, S32 filter_generation) override;

	bool needsArrange();

	// Returns the sort group (system, trash, folder) for this folder.
	EInventorySortGroup getSortGroup() const override;

	virtual void setCompletedFilterGeneration(S32 generation, bool recurse_up);

	LL_INLINE virtual S32 getCompletedFilterGeneration()
	{
		return mCompletedFilterGeneration;
	}

	bool hasFilteredDescendants(S32 filter_generation);
	bool hasFilteredDescendants();

	void recursiveIncrementNumDescendantsSelected(S32 increment);

	LL_INLINE S32 numSelected() const
	{
		return mNumDescendantsSelected + (isSelected() ? 1 : 0);
	}

	// Applies filters to control visibility of inventory items
	void filter(LLInventoryFilter& filter) override;
	void setFiltered(bool filtered, S32 filter_generation) override;
	void dirtyFilter() override;

	// Passes selection information on to children and record selection
	// information if necessary. Returns true if this object (or a child) ends
	// up being selected. If 'openitem' is true then folders are opened up
	// along the way to the selection.
	bool setSelection(LLFolderViewItem* selection, bool openitem,
					  bool take_keyboard_focus) override;

	// This method is used to change the selection of an item. Recursively
	// traverses all children; if 'selection' is 'this' then change the select
	// status if necessary. Returns true if the selection state of this folder,
	// or of a child, was changed.
	bool changeSelection(LLFolderViewItem* selection, bool selected) override;

	// This method is used to group select items
	void extendSelection(LLFolderViewItem* selection,
						 LLFolderViewItem* last_selected,
						 std::vector<LLFolderViewItem*>& items) override;

	// Deselects this folder and all folder/items it contains recursively.
	void recursiveDeselect(bool deselect_self);

	// Returns true is this object and all of its children can be removed.
	bool isRemovable() override;

	// Returns true is this object and all of its children can be moved
	bool isMovable() override;

	// Destroys this folder, and all children
	void destroyView() override;

#if 0
	// If this folder can be removed, remove all children that can be removed,
	// returns true if this is empty after the operation and its viewed folder
	// object can be removed.
	virtual bool removeRecursively(bool single_item);
	virtual bool remove();
#endif

	// Removee the specified item (and any children) if possible. Returns true
	// if the item was deleted.
	bool removeItem(LLFolderViewItem* item);

	// Simply removes the view (and any children) Doesn't bother telling the
	// listeners.
	void removeView(LLFolderViewItem* item);

	// Removes the specified item from the folder, but does not delete it.
	void extractItem(LLFolderViewItem* item);

	// Called by a child that needs to be resorted.
	void resort(LLFolderViewItem* item);

	void setItemSortOrder(U32 ordering);
	void sortBy(U32);

	LL_INLINE void setRegisterLastOpen(bool b)			{ mRegisterLastOpen = b; }

	LL_INLINE void setAutoOpenCountdown(F32 countdown)	{ mAutoOpenCountdown = countdown; }

	// Folders can be opened. This will usually be called by internal methods.
	virtual void toggleOpen();

	// Forces a folder open or closed
	void setOpen(bool openitem = true) override;

	// Called when a child is refreshed but does not rearrange child folder
	// contents unless explicitly requested
	virtual void requestArrange(bool include_descendants = false);

	// Internal method which doesn't update the entire view. This method was
	// written because the list iterators destroy the state of other
	// iterations, thus, we cannot arrange while iterating through the children
	// (such as when setting which is selected.
	virtual void setOpenArrangeRecursively(bool openitem,
										   ERecurseType recurse = RECURSE_NO);

	// Gets the current state of the folder.
	LL_INLINE bool isOpen() override					{ return mIsOpen; }

	// Special case if an object is dropped on the child.
	bool handleDragAndDropFromChild(MASK mask, bool drop,
									EDragAndDropType cargo_type,
									void* cargo_data, EAcceptance* accept,
									std::string& tooltip_msg);

	void applyFunctorRecursively(LLFolderViewFunctor& functor);
	void applyListenerFunctorRecursively(LLFolderViewListenerFunctor& functor) override;

	void openItem() override;
	bool addItem(LLFolderViewItem* item) override;
	bool addFolder(LLFolderViewFolder* folder) override;

	// LLView functionality
	bool handleHover(S32 x, S32 y, MASK mask) override;
	bool handleRightMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleDoubleClick(S32 x, S32 y, MASK mask) override;
	bool handleDragAndDrop(S32 x, S32 y, MASK mask, bool drop,
						   EDragAndDropType cargo_type, void* cargo_data,
						   EAcceptance* accept, std::string& tooltip) override;
	void draw() override;

	time_t getCreationDate() const override;

	bool isTrash() const;
	bool isCOF() const;
	bool isMarketplace() const;

	LL_INLINE S32 getNumSelectedDescendants() const		{ return mNumDescendantsSelected; }

public:
	static LLUUID	sLastOpenId;

private:
	S32				mNumDescendantsSelected;

protected:
	typedef std::list<LLFolderViewItem*> items_t;
	items_t			mItems;

	typedef std::list<LLFolderViewFolder*> folders_t;
	folders_t		mFolders;

	LLInventorySort	mSortFunction;

	F32				mCurHeight;
	F32				mTargetHeight;
	F32				mAutoOpenCountdown;
	time_t			mSubtreeCreationDate;
	S32				mLastArrangeGeneration;
	S32				mLastCalculatedWidth;
	S32				mCompletedFilterGeneration;
	S32				mMostFilteredDescendantGeneration;

	mutable S32		mAmTrash;
	mutable S32		mAmCOF;
	mutable S32		mAmMarket;

	bool			mIsOpen;
	bool			mRegisterLastOpen;
	bool			mForceFetched;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// LLDepthStack template. Used to be in its own llcommon/lldepthstack.h header,
// but is only used here... HB
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

template <class DATA_TYPE> class LLDepthStack
{
public:
	LL_INLINE LLDepthStack()
	:	mCurrentDepth(0),
		mMaxDepth(0)
	{
	}

	LL_INLINE void setDepth(U32 depth)					{ mMaxDepth = depth; }

	LL_INLINE U32 getDepth() const						{ return mCurrentDepth; }

	LL_INLINE void push(DATA_TYPE* data)
	{
		if (mCurrentDepth < mMaxDepth)
		{
			mStack.push_back(data);
			++mCurrentDepth;
		}
		else
		{
			// The last item falls off stack and is deleted
			if (!mStack.empty())
			{
				mStack.pop_front();
			}
			mStack.push_back(data);
		}
	}

	LL_INLINE DATA_TYPE* pop()
	{
		DATA_TYPE* tempp = NULL;
		if (!mStack.empty())
		{
			tempp = mStack.back();
			mStack.pop_back();
			--mCurrentDepth;
		}
		return tempp;
	}

	LL_INLINE DATA_TYPE* check()
	{
		return mStack.empty() ? NULL : mStack.back();
	}

	LL_INLINE void removeAllNodes()
	{
		mCurrentDepth = 0;
		mStack.clear();
	}

private:
	std::deque<DATA_TYPE*>	mStack;
	U32						mCurrentDepth;
	U32						mMaxDepth;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLFolderView
//
// The LLFolderView represents the root level folder view object. It manages
// the screen region of the folder view.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLFolderView : public LLFolderViewFolder, LLEditMenuHandler
{
protected:
	LOG_CLASS(LLFolderView);

public:
	typedef std::deque<LLFolderViewItem*> selected_items_t;
	typedef void (*selection_cb_t)(LLFolderView* folderp, bool user_action,
								   void* userdata);

	LLFolderView(const std::string& name, LLUIImagePtr root_folder_icon,
				 const LLRect& rect, const LLUUID& source_id,
				 LLPanel* parent_panel);

	~LLFolderView() override;

	static LLFolderView* getInstance(const LLUUID& id);

	LL_INLINE bool canFocusChildren() const override	{ return false; }

	// FolderViews default to sort by name. This will change that and resort
	// the items if necessary.
	void setSortOrder(U32 order);

	void checkTreeResortForModelChanged();

	LL_INLINE void setFilterPermMask(PermissionMask filter_perm_mask)
	{
		mFilter.setFilterPermissions(filter_perm_mask);
	}

	LL_INLINE void setSelectCallback(selection_cb_t callback, void* userdata)
	{
		mSelectCallback = callback;
		mUserData = userdata;
	}

	LL_INLINE void setAllowMultiSelect(bool allow)		{ mAllowMultiSelect = allow; }

	LL_INLINE LLInventoryFilter* getFilter()			{ return &mFilter; }
	const std::string getFilterSubString(bool trim = false);
	LL_INLINE bool getFilterWorn() const				{ return mFilter.getFilterWorn(); }
	LL_INLINE bool getFilterLastOpen() const			{ return mFilter.getFilterLastOpen(); }
	LL_INLINE bool getFilterShowLinks() const			{ return mFilter.getFilterShowLinks(); }
	LL_INLINE U32 getFilterTypes() const				{ return mFilter.getFilterTypes(); }

	LL_INLINE PermissionMask getFilterPermissions() const
	{
		return mFilter.getFilterPermissions();
	}

	LL_INLINE LLInventoryFilter::EFolderShow getShowFolderState()
	{
		return mFilter.getShowFolderState();
	}

	U32 getSortOrder() const;
	LL_INLINE bool isFilterModified()					{ return mFilter.isNotDefault(); }
	LL_INLINE bool getAllowMultiSelect()				{ return mAllowMultiSelect; }

	U32 toggleSearchType(std::string toggle);
	U32 getSearchType() const;

	// Closes all folders in the view
	void closeAllFolders();

	void openFolder(const std::string& foldername);
	void openFolder(const LLUUID& cat_id);

	LL_INLINE void toggleOpen() override				{}
	void setOpenArrangeRecursively(bool open, ERecurseType recurse) override;
	bool addFolder(LLFolderViewFolder* folder) override;

	// Finds width and height of this object and it's children.  Also
	// makes sure that this view and it's children are the right size.
	S32 arrange(S32* width, S32* height, S32 filter_generation) override;

	LL_INLINE void arrangeAll()							{ ++mArrangeGeneration; }
	LL_INLINE S32 getArrangeGeneration()				{ return mArrangeGeneration; }

	// Applies filters to control visibility of inventory items
	void filter(LLInventoryFilter& filter) override;

	// Get the last selected item
	virtual LLFolderViewItem* getCurSelectedItem();

	// Record the selected item and pass it down the hierachy.
	bool setSelection(LLFolderViewItem* selection, bool openitem,
					  bool take_keyboard_focus) override;

	// This method is used to toggle the selection of an item. Walks children
	// and keeps track of selected objects.
	bool changeSelection(LLFolderViewItem* selection, bool selected) override;

	void extendSelection(LLFolderViewItem* selection,
						 LLFolderViewItem* last_selected,
						 std::vector<LLFolderViewItem*>& items) override;

	bool getSelectionList(uuid_list_t& selection) override;
	bool getSelection(uuid_vec_t& selection) override;

	// Makes sure if ancestor is selected, descendents are not:
	void sanitizeSelection();
	void clearSelection();
	void addToSelectionList(LLFolderViewItem* item);
	void removeFromSelectionList(LLFolderViewItem* item);

	bool startDrag(LLToolDragAndDrop::ESource source);
	LL_INLINE void setDragAndDropThisFrame()			{ mDragAndDropThisFrame = true; }

	LL_INLINE void setShowThumbnails(bool b = true)		{ mShowThumbnails = b; }
	LL_INLINE bool showThumbnails() const				{ return mShowThumbnails; }

	LL_INLINE void setGotLeftMouseClick()				{ mGotLeftMouseClick = true; }

	// Deletion functionality
 	void removeSelectedItems(bool confirm = true);

	// Opens the selected item:
	void openSelectedItems();

	void propertiesSelectedItems();

	void autoOpenItem(LLFolderViewFolder* item);
	void closeAutoOpenedFolders();
	bool autoOpenTest(LLFolderViewFolder* item);

	// Copy & paste
	void copy() override;
	bool canCopy() const override;

	void doCut(bool confirm);
	void cut() override;
	bool canCut() const override;

	void doPaste(bool confirm);
	void paste() override;
	bool canPaste() const override;

	void doDelete() override;
	bool canDoDelete() const override;

	// Public rename functionality: can only start the process
	void startRenamingSelectedItem(bool confirm = true);

	// Marketplace Listing upkeeping
	void rememberMarketplaceFolders();
	void updateMarketplaceFolders();

	// LLUICtrl Functionality
	void setFocus(bool focus) override;

	// LLView functionality
#if 0
	bool handleKey(KEY key, MASK mask, bool called_from_parent) override;
#endif
	bool handleKeyHere(KEY key, MASK mask) override;
	bool handleUnicodeCharHere(llwchar uni_char) override;
	bool handleMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleDoubleClick(S32 x, S32 y, MASK mask) override;
	bool handleRightMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleHover(S32 x, S32 y, MASK mask) override;
	bool handleDragAndDrop(S32 x, S32 y, MASK mask, bool drop,
						   EDragAndDropType cargo_type, void* cargo_data,
						   EAcceptance* accept, std::string& tooltip) override;
	void reshape(S32 width, S32 height, bool call_from_parent = true) override;
	void onFocusLost() override;
	bool handleScrollWheel(S32 x, S32 y, S32 clicks) override;
	void draw() override;
	void deleteAllChildren() override;

	void scrollToShowSelection();
	void scrollToShowItem(LLFolderViewItem* item);

	LL_INLINE void setScrollContainer(LLScrollableContainer* parent)
	{
		mScrollContainer = parent;
	}

	LLRect getVisibleRect();

	bool search(LLFolderViewItem* first_item, const std::string& search_string,
				bool backward = false);

	LL_INLINE void setShowSelectionContext(bool show)	{ mShowSelectionContext = show; }
	bool getShowSelectionContext();
	void setShowSingleSelection(bool show);
	LL_INLINE bool getShowSingleSelection()				{ return mShowSingleSelection; }
	LL_INLINE F32 getSelectionFadeElapsedTime()			{ return mMultiSelectionFadeTimer.getElapsedTimeF32(); }

	void addItemID(const LLUUID& id, LLFolderViewItem* itemp);
	void removeItemID(const LLUUID& id);
	LLFolderViewItem* getItemByID(const LLUUID& id);

	void doIdle();						// Real idle routine
	static void idle(void* user_data);	// static glue to doIdle()

	LL_INLINE void setCanAutoSelect(bool b)				{ mCanAutoSelect = b; }
	LL_INLINE bool needsAutoSelect()					{ return mNeedsAutoSelect; }
	LL_INLINE bool needsAutoRename()					{ return mNeedsAutoRename; }
	LL_INLINE void setNeedsAutoRename(bool b)			{ mNeedsAutoRename = b; }

	LL_INLINE const selected_items_t& getSelectedItems() const
	{
		return mSelectedItems;
	}

	LL_INLINE LLPanel* getParentPanel() const			{ return mParentPanel; }

protected:
	static void commitRename(LLUICtrl* renamer, void* user_data);
	static void onRenamerLost(LLUICtrl* renamer, void* user_data);

	void finishRenamingItem();
	void closeRenamer();

	// Gets the context menu pointer, creating the menu if not already done.
	// May return NULL after the menu has been deleted for this folder view. HB
	LLMenuGL* getContextMenu();

public:
	static F32				sAutoOpenTime;

protected:
	LLPanel*				mParentPanel;

	LLHandle<LLView>		mPopupMenuHandle;

	// NULL if this is not a child of a scroll container.
	LLScrollableContainer*	mScrollContainer;

	// Set at creation time. It is the task ID for in-world objects folder views
	// or LLUUID::null for the all the inventory floaters. This ID is also used
	// by LLToolDragAndDrop (which itself sends messages to the server with
	// that ID) and should therefore not be touched.
	LLUUID					mSourceID;

	// Used by notification static callbacks to find which folder view it
	// relates to. It is either the task UUID for inworld object (== mSourceID)
	// or a randomly generated UUID for each main inventory folder views.
	LLUUID					mFolderViewId;

	// Renaming variables and methods
	LLFolderViewItem*		mRenameItem;  // The item being renamed
	LLLineEditor*			mRenamer;

	LLFolderViewItem*		mLastScrollItem;
	LLCoordGL				mLastScrollOffset;

	U32						mSortOrder;
	U32						mSearchType;
	LLFolderViewFolder*		mAutoOpenCandidate;
	LLFrameTimer			mAutoOpenTimer;
	LLFrameTimer			mSearchTimer;
	std::string				mSearchString;
	LLInventoryFilter		mFilter;
	LLFrameTimer			mMultiSelectionFadeTimer;
	S32						mArrangeGeneration;

	S32						mSignalSelectCallback;
	void*					mUserData;
	selection_cb_t			mSelectCallback;
	S32						mMinWidth;

	typedef LLDepthStack<LLFolderViewFolder> auto_open_stack_t;
	auto_open_stack_t		mAutoOpenItems;

	typedef fast_hmap<LLUUID, LLFolderViewItem*> item_map_t;
	item_map_t				mItemMap;

	selected_items_t		mSelectedItems;

	// Marketplace listings upkeeping
	uuid_list_t				mMarketplaceFolders;
	bool					mWillModifyListing;
	bool					mWillUnlistIfRemoved;
	bool					mWillDeleteListingIfRemoved;

	bool					mContextMenuCreated;
	bool					mKeyboardSelection;
	bool					mAllowMultiSelect;
	bool					mNeedsScroll;
	bool					mCanAutoSelect;
	bool					mNeedsAutoSelect;
	bool					mNeedsAutoRename;
	bool					mShowSelectionContext;
	bool					mShowSingleSelection;
	bool					mHasCapture;
	bool					mDragAndDropThisFrame;
	bool					mShowThumbnails;
	bool					mGotLeftMouseClick;

	typedef fast_hmap<LLUUID, LLFolderView*> instances_map_t;
	static instances_map_t	sInstances;
};

class LLInventoryPanel : public LLPanel
{
protected:
	LOG_CLASS(LLInventoryPanel);

public:
	LLInventoryPanel(const std::string& name, const std::string& sort_order,
					 const LLRect& rect, LLInventoryModel* inventory,
					 bool allow_multi_select, bool disable_double_click,
					 bool show_thumbnails);
	~LLInventoryPanel();

	LL_INLINE LLInventoryModel* getModel()				{ return mInventory; }

	bool postBuild();

	virtual LLXMLNodePtr getXML(bool save_children = true) const;
	static LLView* fromXML(LLXMLNodePtr node, LLView* parentp,
						   LLUICtrlFactory*);

	// LLView methods
	void draw();
	bool handleHover(S32 x, S32 y, MASK mask);
	bool handleDragAndDrop(S32 x, S32 y, MASK mask, bool drop,
						   EDragAndDropType cargo_type, void* cargo_data,
						   EAcceptance* accept, std::string& tooltip_msg);

	// Call this method to set the selection.
	void openAllFolders();
	void closeAllFolders();
	void openDefaultFolderForType(LLAssetType::EType);
	void setSelection(const LLUUID& obj_id, bool take_keyboard_focus);

	LL_INLINE void setSelectCallback(LLFolderView::selection_cb_t callback,
									 void* user_data)
	{
		if (mFolders)
		{
			mFolders->setSelectCallback(callback, user_data);
		}
	}

	void clearSelection();
	LL_INLINE LLInventoryFilter* getFilter()			{ return mFolders->getFilter(); }
	void setFilterTypes(U32 filter);
	LL_INLINE U32 getFilterTypes() const				{ return mFolders->getFilterTypes(); }
	void setFilterHideLibrary(bool hide = true);
	void setFilterSubType(S32 subtype);
	void setFilterPermMask(PermissionMask filter_perm_mask);
	LL_INLINE U32 getFilterPermMask() const				{ return mFolders->getFilterPermissions(); }
	void setFilterSubString(const std::string& string);
	LL_INLINE const std::string getFilterSubString()	{ return mFolders->getFilterSubString(); }
	void setFilterWorn(bool worn);
	LL_INLINE bool getFilterWorn() const				{ return mFolders->getFilterWorn(); }
	void setFilterLastOpen(bool b);
	LL_INLINE bool getFilterLastOpen() const			{ return mFolders->getFilterLastOpen(); }
	void setFilterShowLinks(bool b);
	LL_INLINE bool getFilterShowLinks() const			{ return mFolders->getFilterShowLinks(); }

	void setSortOrder(U32 order);
	LL_INLINE U32 getSortOrder()						{ return mFolders->getSortOrder(); }
	void setSinceLogoff(bool sl);
	void setHoursAgo(U32 hours);
	LL_INLINE bool getSinceLogoff()						{ return mFolders->getFilter()->isSinceLogoff(); }

	void setShowFolderState(LLInventoryFilter::EFolderShow show);
	LLInventoryFilter::EFolderShow getShowFolderState();

	LL_INLINE void setAllowMultiSelect(bool allow)		{ mFolders->setAllowMultiSelect(allow); }

	// This method is called when something has changed about the inventory.
	void modelChanged(U32 mask);

	LL_INLINE LLFolderView* getRootFolder()				{ return mFolders; }

	LL_INLINE LLScrollableContainer* getScrollableContainer()
	{
		return mScroller;
	}

	void openSelected();

	LL_INLINE void unSelectAll()						{ mFolders->setSelection(NULL, false, false); }

	LL_INLINE void setLastOpenLocked(bool b)			{ mLastOpenLocked = b; }

	// Used to keep track of the last open folder in the "Last Open" tab:
	bool makeLastOpenCurrent();
	LL_INLINE const LLUUID& getLastOpenID()				{ return mLastOpenID; }

protected:
	// Given the id and the parent, build all of the folder views.
	void rebuildViewsFor(const LLUUID& id);
	void buildNewViews(const LLUUID& id);

protected:
	LLUUID					mSelectThisID; // If non null, select this item
	LLUUID					mLastOpenID;
	LLInventoryModel*		mInventory;
	LLInventoryObserver*	mInventoryObserver;
	LLFolderView*			mFolders;
	LLScrollableContainer*	mScroller;
	const std::string		mSortOrderSetting;
	bool 					mAllowMultiSelect;
	bool					mLastOpenLocked;
	bool					mDoubleClickDisabled;
	bool					mShowThumbnails;
};

bool sort_item_name(LLFolderViewItem* a, LLFolderViewItem* b);
bool sort_item_date(LLFolderViewItem* a, LLFolderViewItem* b);

// Flags for buildContextMenu()
constexpr U32 SUPPRESS_OPEN_ITEM = 0x1;
constexpr U32 FIRST_SELECTED_ITEM = 0x2;
constexpr U32 ITEM_IN_MULTI_SELECTION = 0x4;

#endif // LL_LLFOLDERVIEW_H
