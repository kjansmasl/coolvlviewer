/**
 * @file llfolderview.cpp
 * @brief Implementation of the folder view collection of classes.
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

#include "llfolderview.h"

#include "llcachename.h"
#include "llcallbacklist.h"
#include "llfasttimer.h"
#include "llmenugl.h"
#include "llnotifications.h"
#include "llscrollcontainer.h"
#include "lltrans.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "llagentwearables.h"
#include "llaisapi.h"					// For AISAPI::isAvailable()
#include "llappearancemgr.h"
#include "llappviewer.h"
#include "llfloaterproperties.h"
#include "hbfloaterthumbnail.h"
#include "llinventoryactions.h"			// For init_inventory_panel_actions()
#include "llinventorybridge.h"
#include "hbinventoryclipboard.h"
#include "llinventorymodelfetch.h"
#include "llmarketplacefunctions.h"
#include "llpreview.h"
#include "lltooldraganddrop.h"
#include "llviewercontrol.h"
#include "llviewerfoldertype.h"
#include "llviewermenu.h"				// For gMenuHolderp
#include "llviewerregion.h"
#include "llviewerwindow.h"
#include "llvoavatarself.h"

#if LL_WINDOWS
// Warning C4355: 'this' : used in base member initializer list
#pragma warning(disable : 4355)
#endif

constexpr S32 LEFT_PAD = 5;
constexpr S32 LEFT_INDENTATION = 10;
constexpr S32 ICON_PAD = 2;
constexpr S32 ICON_WIDTH = 16;
constexpr S32 TEXT_PAD = 1;
constexpr S32 ARROW_SIZE = 12;
constexpr S32 RENAME_HEIGHT_PAD = 6;
constexpr S32 AUTO_OPEN_STACK_DEPTH = 16;
constexpr S32 MIN_ITEM_WIDTH_VISIBLE = ICON_WIDTH + ICON_PAD + ARROW_SIZE +
									   TEXT_PAD + /*first few characters*/ 40;
constexpr S32 MINIMUM_RENAMER_WIDTH = 80;
constexpr F32 FOLDER_CLOSE_TIME_CONSTANT = 0.02f;
constexpr F32 FOLDER_OPEN_TIME_CONSTANT = 0.03f;
constexpr S32 MAX_FOLDER_ITEM_OVERLAP = 2;

enum
{
	SIGNAL_NO_KEYBOARD_FOCUS = 1,
	SIGNAL_KEYBOARD_FOCUS = 2
};

// Static meember variables
LLFolderView::instances_map_t LLFolderView::sInstances;
F32 LLFolderView::sAutoOpenTime = 1.f;
LLUUID LLFolderViewFolder::sLastOpenId;

LLUUID LLFolderViewEventListener::sLastDragTipID;
std::string LLFolderViewEventListener::sLastDragTipMsg;
bool LLFolderViewEventListener::sLastDragTipDrop = false;

const LLFontGL* LLFolderViewItem::sFont = NULL;
const LLFontGL* LLFolderViewItem::sFontItalic = NULL;
F32 LLFolderViewItem::sFontLineHeight = 0.f;
S32 LLFolderViewItem::sFontLineHeightRounded = 0;
LLColor4 LLFolderViewItem::sFgColor;
LLColor4 LLFolderViewItem::sHighlightBgColor;
LLColor4 LLFolderViewItem::sHighlightFgColor;
LLColor4 LLFolderViewItem::sContextMenuBgColor;
LLColor4 LLFolderViewItem::sFilterBGColor;
LLColor4 LLFolderViewItem::sFilterTextColor;
LLColor4 LLFolderViewItem::sSuffixColor;
LLColor4 LLFolderViewItem::sSearchStatusColor;
LLUIImagePtr LLFolderViewItem::sArrowImage;
LLWString LLFolderViewItem::sLoadingStr;

// This is used to keep track of existing folder view items and avoid a crash
// bug due to a race condition (see in doIdle()). HB
static fast_hset<LLFolderViewItem*> sFolderViewItems;

static const std::string LL_INVENTORY_PANEL_TAG = "inventory_panel";

//-----------------------------------------------------------------------------
// LLFolderViewItem class
//-----------------------------------------------------------------------------

//static
void LLFolderViewItem::initClass()
{
	sFont = LLFontGL::getFontSansSerifSmall();
	sFontItalic = LLFontGL::getFont("SansSerif", "Little", LLFontGL::ITALIC);
	sFontLineHeight = sFont->getLineHeight();
	if (sFontItalic)
	{
		sFontLineHeight = llmax(sFontLineHeight, sFontItalic->getLineHeight());
	}
	sFontLineHeightRounded = ll_roundp(sFontLineHeight);
	sArrowImage = LLUI::getUIImage("folder_arrow.tga");
	sLoadingStr = LLTrans::getWString("LoadingData");

	connectRefreshCachedSettingsSafe("MenuItemEnabledColor");
	connectRefreshCachedSettingsSafe("MenuItemHighlightBgColor");
	connectRefreshCachedSettingsSafe("MenuItemHighlightFgColor");
	connectRefreshCachedSettingsSafe("MenuPopupBgColor");
	connectRefreshCachedSettingsSafe("FilterBackgroundColor");
	connectRefreshCachedSettingsSafe("FilterTextColor");
	connectRefreshCachedSettingsSafe("InventoryItemSuffixColor");
	connectRefreshCachedSettingsSafe("InventorySearchStatusColor");

	refreshCachedSettings();
}

//static
void LLFolderViewItem::cleanupClass()
{
	sArrowImage = NULL;
}

//static
void LLFolderViewItem::connectRefreshCachedSettingsSafe(const char* name)
{
	LLControlVariable* controlp = gColors.getControl(name);
	if (!controlp)
	{
		llwarns << "Setting name not found: " << name << llendl;
		return;
	}
	controlp->getSignal()->connect(boost::bind(&LLFolderViewItem::refreshCachedSettings));
}

//static
void LLFolderViewItem::refreshCachedSettings()
{
	sFgColor = gColors.getColor("MenuItemEnabledColor");
	sHighlightBgColor = gColors.getColor("MenuItemHighlightBgColor");
	sHighlightFgColor = gColors.getColor("MenuItemHighlightFgColor");
	sContextMenuBgColor = gColors.getColor("MenuPopupBgColor");
	sFilterBGColor = gColors.getColor("FilterBackgroundColor");
	sFilterTextColor = gColors.getColor("FilterTextColor");
	sSuffixColor = gColors.getColor("InventoryItemSuffixColor");
	sSearchStatusColor = gColors.getColor("InventorySearchStatusColor");
}

// Default constructor
// NOTE: Optimize this, we call it a *lot* when opening a large inventory
LLFolderViewItem::LLFolderViewItem(const std::string& name, LLUIImagePtr icon,
								   S32 creation_date,
								   LLFolderView* root,
								   LLFolderViewEventListener* listener)
:	LLUICtrl(name, LLRect(0, 0, 0, 0), true, NULL, NULL,
			 FOLLOWS_LEFT|FOLLOWS_TOP|FOLLOWS_RIGHT),
	mLabel(name),
	mWLabel(utf8str_to_wstring(name)),
	mLabelWidth(0),
	mCreationDate(creation_date),
	mParentFolder(NULL),
	mListener(listener),
	mIsSelected(false),
	mIsCurSelection(false),
	mSelectPending(false),
	mDoubleClickDisabled(false),
	mLabelStyle(LLFontGL::NORMAL),
	mIcon(icon),
	mHasVisibleChildren(false),
	mIndentation(0),
	mFiltered(false),
	mLastFilterGeneration(-1),
	mStringMatchOffset(std::string::npos),
	mControlLabelRotation(0.f),
	mRoot(root),
	mDragAndDropTarget(false),
	mIsLoading(false),
	mHasDescription(true)
{
	sFolderViewItems.insert(this);
	// Possible optimization: only call refreshFromListener():
	refresh();
	setTabStop(false);
}

//virtual
LLFolderViewItem::~LLFolderViewItem()
{
	sFolderViewItems.erase(this);
	delete mListener;
	mListener = NULL;
}

// Returns true if this object is a child (or grandchild, etc.) of
// potential_ancestor.
bool LLFolderViewItem::isDescendantOf(const LLFolderViewFolder* potential_ancestor)
{
	LLFolderViewItem* root = this;
	while (root->mParentFolder)
	{
		if (root->mParentFolder == potential_ancestor)
		{
			return true;
		}
		root = root->mParentFolder;
	}
	return false;
}

LLFolderViewItem* LLFolderViewItem::getNextOpenNode(bool include_children)
{
	if (!mParentFolder)
	{
		return NULL;
	}

	LLFolderViewItem* itemp =
		mParentFolder->getNextFromChild(this, include_children);
	while (itemp && !itemp->getVisible())
	{
		LLFolderViewItem* next_itemp =
			itemp->mParentFolder->getNextFromChild(itemp, include_children);
		if (itemp == next_itemp)
		{
			// Hit last item
			return itemp->getVisible() ? itemp : this;
		}
		itemp = next_itemp;
	}

	return itemp;
}

LLFolderViewItem* LLFolderViewItem::getPreviousOpenNode(bool include_children)
{
	if (!mParentFolder)
	{
		return NULL;
	}

	LLFolderViewItem* itemp =
		mParentFolder->getPreviousFromChild(this, include_children);
	while (itemp && !itemp->getVisible())
	{
		LLFolderViewItem* next_itemp =
			itemp->mParentFolder->getPreviousFromChild(itemp,
													   include_children);
		if (itemp == next_itemp)
		{
			// Hit first item
			return itemp->getVisible() ? itemp : this;
		}
		itemp = next_itemp;
	}

	return itemp;
}

// Is this item something we think we should be showing ?  For example, if we
// have not gotten around to filtering it yet, then the answer is yes until we
// find out otherwise.
//virtual
bool LLFolderViewItem::potentiallyVisible()
{
	// We have not been checked against min required filter or we have and we
	// passed
	return getFiltered() ||
		   mLastFilterGeneration < mRoot->getFilter()->getMinRequiredGeneration();
}

//virtual
bool LLFolderViewItem::getFiltered()
{
	return mFiltered &&
		   mLastFilterGeneration >= mRoot->getFilter()->getMinRequiredGeneration();
}

//virtual
bool LLFolderViewItem::getFiltered(S32 filter_generation)
{
	return mFiltered && mLastFilterGeneration >= filter_generation;
}

//virtual
void LLFolderViewItem::setFiltered(bool filtered, S32 filter_generation)
{
	mFiltered = filtered;
	mLastFilterGeneration = filter_generation;
}

const LLFontGL* LLFolderViewItem::getRenderFont(U32& style)
{
	static LLCachedControl<bool> use_true_italics(gSavedSettings,
												  "InventoryUseItalicsFont");
	style = mLabelStyle;
	if (use_true_italics && (style & LLFontGL::ITALIC) && sFontItalic)
	{
		// Use a true italic font instead of slanting the default font. HB
		style &= ~LLFontGL::ITALIC;
		return sFontItalic;
	}
	return sFont;
}

// Refresh information from the listener
void LLFolderViewItem::refreshFromListener()
{
	if (mListener)
	{
		mHasDescription = false;
		mLabel = mListener->getDisplayName();
		mWLabel = utf8str_to_wstring(mLabel);
		mIcon = mListener->getIcon();
		time_t creation_date = mListener->getCreationDate();
		if ((time_t)mCreationDate != creation_date)
		{
			mCreationDate = mListener->getCreationDate();
			dirtyFilter();
		}
		mLabelStyle = mListener->getLabelStyle();
		mLabelSuffix = mListener->getLabelSuffix();
		mWLabelSuffix = utf8str_to_wstring(mLabelSuffix);

		LLInventoryItem* item = gInventory.getItem(mListener->getUUID());
		if (item)
		{
			std::string desc = item->getDescription();
			if (!desc.empty())
			{
				LLStringUtil::trim(desc);
				if (!desc.empty())
				{
					LLStringUtil::toUpper(desc);
					if (desc != "(NO DESCRIPTION)")
					{
						mHasDescription = true;
					}
				}
			}
		}
	}
}

//virtual
void LLFolderViewItem::refresh()
{
	refreshFromListener();

	std::string searchable_label(mLabel);
	searchable_label.append(mLabelSuffix);
	LLStringUtil::toUpper(searchable_label);

	if (mSearchableLabel.compare(searchable_label))
	{
		mSearchableLabel.assign(searchable_label);
		dirtyFilter();
		// Some part of label has changed, so overall width has potentially
		// changed
		if (mParentFolder)
		{
			mParentFolder->requestArrange();
		}
	}

	U32 style;
	const LLFontGL* fontp = getRenderFont(style);
	S32 label_width = fontp->getWidth(mLabel);
	if (mLabelSuffix.size())
	{
		label_width += fontp->getWidth(mLabelSuffix);
	}

	mLabelWidth = ARROW_SIZE + TEXT_PAD + ICON_WIDTH + ICON_PAD + label_width;
}

void LLFolderViewItem::applyListenerFunctorRecursively(LLFolderViewListenerFunctor& functor)
{
	functor(mListener);
}

// This method is called when items are added or view filters change. It is
// implemented here but called by derived classes when folding the views.
void LLFolderViewItem::filterFromRoot()
{
	LLFolderViewItem* root = getRoot();
	if (root)
	{
		root->filter(*((LLFolderView*)root)->getFilter());
	}
}

// This method is called when the folder view is dirty. It is implemented here
// but called by derived classes when folding the views.
void LLFolderViewItem::arrangeFromRoot()
{
	LLFolderViewItem* root = getRoot();
	if (root)
	{
		root->arrange(NULL, NULL, 0);
	}
}

// This method clears the currently selected item, and records the specified
// selected item appropriately for display and use in the UI. If open is true,
// then folders are opened up along the way to the selection.
void LLFolderViewItem::setSelectionFromRoot(LLFolderViewItem* selection,
										    bool openitem,
										    bool take_keyboard_focus)
{
	getRoot()->setSelection(selection, openitem, take_keyboard_focus);
}

// Helper method to change the selection from the root.
void LLFolderViewItem::changeSelectionFromRoot(LLFolderViewItem* selection,
											   bool selected)
{
	getRoot()->changeSelection(selection, selected);
}

void LLFolderViewItem::extendSelectionFromRoot(LLFolderViewItem* selection)
{
	std::vector<LLFolderViewItem*> selected_items;
	getRoot()->extendSelection(selection, NULL, selected_items);
}

EInventorySortGroup LLFolderViewItem::getSortGroup()  const
{
	return SG_ITEM;
}

//virtual
bool LLFolderViewItem::addToFolder(LLFolderViewFolder* folder,
								   LLFolderView* root)
{
	if (!folder || !root || !getListener())
	{
		return false;
	}

	mParentFolder = folder;
	root->addItemID(getListener()->getUUID(), this);
	return folder->addItem(this);
}

// Finds width and height of this object and its children. Also makes sure that
// this view and its children are the right size.
//virtual
S32 LLFolderViewItem::arrange(S32* width, S32* height, S32 filter_generation)
{
	if (mParentFolder)
	{
		mIndentation = mParentFolder->getIndentation() + LEFT_INDENTATION;
	}
	else
	{
		mIndentation = 0;
	}

	if (width)
	{
		*width = llmax(*width, mLabelWidth + mIndentation);
	}

	S32 item_height = getItemHeight();

	if (height)
	{
		*height = item_height;
	}

	return item_height;
}

//virtual
S32 LLFolderViewItem::getItemHeight()
{
	S32 icon_height = mIcon.isNull() ? 0 : mIcon->getHeight();
	return llmax(icon_height, sFontLineHeightRounded) + ICON_PAD;
}

//virtual
void LLFolderViewItem::filter(LLInventoryFilter& filter)
{
	bool filtered = mListener && filter.check(this);
	// If our visibility will change as a result of this filter, then we need
	// to be rearranged in our parent folder
	if (getVisible() != filtered)
	{
		if (mParentFolder)
		{
			mParentFolder->requestArrange();
		}
	}

	setFiltered(filtered, filter.getCurrentGeneration());
	mStringMatchOffset = filter.getStringMatchOffset();
	filter.decrementFilterCount();
}

//virtual
void LLFolderViewItem::dirtyFilter()
{
	mLastFilterGeneration = -1;
	// Bubble up dirty flag all the way to root
	if (getParentFolder())
	{
		getParentFolder()->setCompletedFilterGeneration(-1, true);
	}
}

// *TODO: This can be optimized a lot by simply recording that it is selected
// in the appropriate places, and assuming that set selection means 'deselect'
// for a leaf item. Do this optimization after multiple selection is
// implemented to make sure it all plays nice together.
bool LLFolderViewItem::setSelection(LLFolderViewItem* selection, bool openitem,
									bool take_keyboard_focus)
{
	if (selection == this && !mIsSelected)
	{
		selectItem();
		if (mListener)
		{
			mListener->selectItem();
		}
	}
	else if (mIsSelected)	// Deselect everything else.
	{
		deselectItem();
	}

	return mIsSelected;
}

bool LLFolderViewItem::changeSelection(LLFolderViewItem* selection,
									   bool selected)
{
	if (selection == this && mIsSelected != selected)
	{
	  	if (mIsSelected)
		{
			deselectItem();
		}
		else
		{
			selectItem();
		}
		if (mListener)
		{
			mListener->selectItem();
		}
		return true;
	}

	return false;
}

void LLFolderViewItem::deselectItem()
{
	llassert(mIsSelected);

	mIsSelected = false;

	// Update ancestors' count of selected descendents.
	LLFolderViewFolder* parent_folder = getParentFolder();
	if (parent_folder)
	{
		parent_folder->recursiveIncrementNumDescendantsSelected(-1);
	}
}

void LLFolderViewItem::selectItem()
{
	llassert(!mIsSelected);

	mIsSelected = true;

	// Update ancestors' count of selected descendents.
	LLFolderViewFolder* parent_folder = getParentFolder();
	if (parent_folder)
	{
		parent_folder->recursiveIncrementNumDescendantsSelected(1);
	}
}

bool LLFolderViewItem::isMovable()
{
	return !mListener || mListener->isItemMovable();
}

bool LLFolderViewItem::isRemovable()
{
	return !mListener || mListener->isItemRemovable();
}

void LLFolderViewItem::destroyView()
{
	if (mParentFolder)
	{
		// removeView() deletes me
		mParentFolder->removeView(this);
	}
}

// Call through to the viewed object and return true if it can be removed.
bool LLFolderViewItem::remove()
{
	if (!isRemovable())
	{
		return false;
	}
	if (mListener)
	{
		return mListener->removeItem();
	}
	return true;
}

// Build an appropriate context menu for the item.
void LLFolderViewItem::buildContextMenu(LLMenuGL& menu, U32 flags)
{
	if (mListener)
	{
		mListener->buildContextMenu(menu, flags);
	}
	LLViewerRegion* regionp = gAgent.getRegion();
	if (!regionp || !regionp->bakesOnMeshEnabled())
	{
		LLMenuItemGL* item = menu.getChild<LLMenuItemGL>("New Universal",
														 true, false);
		if (item)
		{
			item->setVisible(false);
		}
	}
}

void LLFolderViewItem::openItem()
{
	if (mListener)
	{
		mListener->openItem();
	}
}

void LLFolderViewItem::preview()
{
	if (mListener)
	{
		mListener->previewItem();
	}
}

void LLFolderViewItem::rename(const std::string& new_name)
{
	if (!new_name.empty())
	{
		mLabel = new_name;
		mWLabel = utf8str_to_wstring(new_name);
		if (mListener)
		{
			mListener->renameItem(new_name);

			if (mParentFolder)
			{
				mParentFolder->resort(this);
			}
		}
	}
}

std::string LLFolderViewItem::getSearchableData()
{
	std::string searchable;
	U32 flags = mRoot->getSearchType();
	if (flags == 0 || (flags & 1))
	{
		searchable = mSearchableLabel;
	}
	bool want_desc = (flags & 2) != 0 && mHasDescription;
	if (!want_desc)
	{
		// Get rid of cached data to save memory.
		mSearchableDesc.clear();
	}
	bool want_creator = (flags & 4) != 0;
	if (!want_creator)
	{
		// Get rid of cached data to save memory.
		mSearchableCreator.clear();
	}
	if ((want_desc || want_creator) && mListener)
	{
		bool fetch_desc = want_desc && mSearchableDesc.empty();
		bool fetch_creator = want_creator && mSearchableCreator.empty();
		if (fetch_desc || fetch_creator)
		{
			LLInventoryItem* item = gInventory.getItem(mListener->getUUID());
			if (item)
			{
				if (fetch_desc)
				{
					mSearchableDesc = item->getDescription();
					if (mSearchableDesc.empty())
					{
						want_desc = false;
					}
					else
					{
						LLStringUtil::toUpper(mSearchableDesc);
					}
				}
				if (fetch_creator)
				{
					const LLUUID& creator_id = item->getCreatorUUID();
					if (creator_id.isNull())
					{
						mSearchableCreator = '?';
					}
					else if (gCacheNamep &&
							 gCacheNamep->getFullName(creator_id,
													  mSearchableCreator))
					{
						if (mSearchableCreator.empty())
						{
							mSearchableCreator = '?';
						}
						else
						{
							LLStringUtil::toUpper(mSearchableCreator);
						}
					}
					else
					{
						mSearchableCreator.clear();
						want_creator = false;
					}
				}
			}
		}
		if (want_desc)
		{
			if (!searchable.empty())
			{
				searchable += ' ';
			}
			searchable += mSearchableDesc;
		}
		if (want_creator)
		{
			if (!searchable.empty())
			{
				searchable += ' ';
			}
			searchable += mSearchableCreator;
		}
	}

	return searchable;
}

std::string LLFolderViewItem::getName() const
{
	if (mListener)
	{
		return mListener->getName();
	}
	return mLabel;
}

// LLView functionality
bool LLFolderViewItem::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
	if (!mIsSelected)
	{
		setSelectionFromRoot(this, false);
	}
	make_ui_sound("UISndClick");
	return true;
}

bool LLFolderViewItem::handleMouseDown(S32 x, S32 y, MASK mask)
{
	// No handler needed for focus lost since this class has no state that
	// depends on it.
	gFocusMgr.setMouseCapture(this);

	if (!mIsSelected)
	{
		if (mask & MASK_CONTROL)
		{
			changeSelectionFromRoot(this, !mIsSelected);
		}
		else if (mask & MASK_SHIFT)
		{
			extendSelectionFromRoot(this);
		}
		else
		{
			setSelectionFromRoot(this, false);
		}
		mRoot->setGotLeftMouseClick();
		make_ui_sound("UISndClick");
	}
	else
	{
		mSelectPending = true;
	}

	if (isMovable())
	{
		S32 screen_x, screen_y;
		localPointToScreen(x, y, &screen_x, &screen_y);
		gToolDragAndDrop.setDragStart(screen_x, screen_y);
	}
	return true;
}

bool LLFolderViewItem::handleHover(S32 x, S32 y, MASK mask)
{
	if (!hasMouseCapture() || !isMovable())
	{
		mRoot->setShowSelectionContext(false);
		gViewerWindowp->setCursor(UI_CURSOR_ARROW);
		// Let the parent handle this then...
		return false;
	}

	bool can_drag = true;

	S32 screen_x, screen_y;
	localPointToScreen(x, y, &screen_x, &screen_y);
	if (gToolDragAndDrop.isOverThreshold(screen_x, screen_y))
	{
		if (mRoot->getCurSelectedItem())
		{
			LLToolDragAndDrop::ESource src = LLToolDragAndDrop::SOURCE_WORLD;

			// *TODO: push this into listener and remove dependency on llagent
			if (mListener &&
				gInventory.isObjectDescendentOf(mListener->getUUID(),
												gInventory.getRootFolderID()))
			{
				src = LLToolDragAndDrop::SOURCE_AGENT;
			}
			else if (mListener &&
					 gInventory.isObjectDescendentOf(mListener->getUUID(),
													 gInventory.getLibraryRootFolderID()))
			{
				src = LLToolDragAndDrop::SOURCE_LIBRARY;
			}

			can_drag = mRoot->startDrag(src);
			if (can_drag)
			{
				// if (mListener) mListener->startDrag();
				// RN: when starting drag and drop, clear out last auto-open
				mRoot->autoOpenTest(NULL);
				mRoot->setShowSelectionContext(true);

				// Release keyboard focus, so that if stuff is dropped into the
				// world, pressing the delete key would not blow away the
				// inventory item.
				gFocusMgr.setKeyboardFocus(NULL);

				return gToolDragAndDrop.handleHover(x, y, mask);
			}
		}
	}

	if (can_drag)
	{
		gViewerWindowp->setCursor(UI_CURSOR_ARROW);
	}
	else
	{
		gViewerWindowp->setCursor(UI_CURSOR_NOLOCKED);
	}

	return true;
}

bool LLFolderViewItem::handleDoubleClick(S32 x, S32 y, MASK mask)
{
	if (!mDoubleClickDisabled)
	{
		preview();
	}
	return true;
}

bool LLFolderViewItem::handleScrollWheel(S32 x, S32 y, S32 clicks)
{
	return getParent() && getParent()->handleScrollWheel(x, y, clicks);
}

bool LLFolderViewItem::handleMouseUp(S32 x, S32 y, MASK mask)
{
	// If mouse has not moved since mouse down...
	if (pointInView(x, y) && mSelectPending)
	{
		// ...then select
		if (mask & MASK_CONTROL)
		{
			changeSelectionFromRoot(this, !mIsSelected);
		}
		else if (mask & MASK_SHIFT)
		{
			extendSelectionFromRoot(this);
		}
		else
		{
			setSelectionFromRoot(this, false);
		}
	}

	mSelectPending = false;

	if (hasMouseCapture())
	{
		getRoot()->setShowSelectionContext(false);
		gFocusMgr.setMouseCapture(NULL);
	}

	return true;
}

//virtual
bool LLFolderViewItem::handleDragAndDrop(S32 x, S32 y, MASK mask, bool drop,
										 EDragAndDropType cargo_type,
										 void* cargo_data, EAcceptance* accept,
										 std::string& tooltip_msg)
{
	bool accepted = false;
	bool handled = false;
	if (mListener)
	{
		accepted = mListener->dragOrDrop(mask, drop, cargo_type, cargo_data,
										 tooltip_msg);
		handled = accepted;
		if (accepted)
		{
			mDragAndDropTarget = true;
			*accept = ACCEPT_YES_MULTI;
		}
		else
		{
			*accept = ACCEPT_NO;
		}
		LLFolderViewEventListener::dragOrDropTip(drop, tooltip_msg);
	}
	if (mParentFolder && !handled)
	{
		handled = mParentFolder->handleDragAndDropFromChild(mask, drop,
															cargo_type,
															cargo_data,
															accept,
															tooltip_msg);
	}
	if (handled)
	{
		LL_DEBUGS("UserInput") << "dragAndDrop handled with: drop = "
							   << (drop ? "true" : "false") << " - accepted = "
							   <<  (accepted ? "true" : "false") << LL_ENDL;
	}

	return handled;
}

void LLFolderViewItem::draw()
{
	bool up_to_date = mListener && mListener->isUpToDate();
	if (sArrowImage &&
		// If we fetched our children and some of them have passed the filter...
		((up_to_date && hasVisibleChildren()) ||
		// ... or we know we have children but have not fetched them (does not
		// obey filter)
		 (!up_to_date && mListener && mListener->hasChildren())))
	{
		gl_draw_scaled_rotated_image(mIndentation,
									 getRect().getHeight() - ARROW_SIZE -
									 TEXT_PAD, ARROW_SIZE, ARROW_SIZE,
									 mControlLabelRotation,
									 sArrowImage->getImage(), sFgColor);
	}

	F32 text_left = (F32)(ARROW_SIZE + TEXT_PAD + ICON_WIDTH + ICON_PAD +
						  mIndentation);

	// If we have keyboard focus, draw selection filled
	bool show_context = getRoot()->getShowSelectionContext();
	bool filled = show_context || (gFocusMgr.getKeyboardFocus() == getRoot());

	// Always render "current" item, only render other selected items if
	// mShowSingleSelection is false
	if (mIsSelected)
	{
		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
		LLColor4 bg_color = sHighlightBgColor;
		if (!mIsCurSelection)
		{
			// Do time-based fade of extra objects
			F32 fade_time = getRoot()->getSelectionFadeElapsedTime();
			if (getRoot()->getShowSingleSelection())
			{
				// Fading out
				bg_color.mV[VALPHA] = clamp_rescale(fade_time, 0.f, 0.4f,
													bg_color.mV[VALPHA], 0.f);
			}
			else
			{
				// Fading in
				bg_color.mV[VALPHA] = clamp_rescale(fade_time, 0.f, 0.4f, 0.f,
													bg_color.mV[VALPHA]);
			}
		}

		gl_rect_2d(0, getRect().getHeight(), getRect().getWidth() - 2,
				   llfloor(getRect().getHeight() - sFontLineHeight - ICON_PAD),
				   bg_color, filled);
		if (mIsCurSelection)
		{
			gl_rect_2d(0, getRect().getHeight(), getRect().getWidth() - 2,
					   llfloor(getRect().getHeight() - sFontLineHeight -
							   ICON_PAD),
					   sHighlightFgColor, false);
		}
		if (getRect().getHeight() > sFontLineHeightRounded + ICON_PAD + 2)
		{
			gl_rect_2d(0,
					   llfloor(getRect().getHeight() - sFontLineHeight -
							   ICON_PAD) - 2,
					   getRect().getWidth() - 2, 2, sHighlightFgColor, false);
			if (show_context)
			{
				gl_rect_2d(0,
						   llfloor(getRect().getHeight() -
								   sFontLineHeight - ICON_PAD) - 2,
						   getRect().getWidth() - 2, 2, sHighlightBgColor,
						   true);
			}
		}
	}
	if (mDragAndDropTarget)
	{
		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
		gl_rect_2d(0, getRect().getHeight(), getRect().getWidth() - 2,
				   llfloor(getRect().getHeight() - sFontLineHeight - ICON_PAD),
				   sHighlightBgColor, false);

		if (getRect().getHeight() > sFontLineHeightRounded + ICON_PAD + 2)
		{
			gl_rect_2d(0,
					   llfloor(getRect().getHeight() - sFontLineHeight - ICON_PAD) - 2,
					   getRect().getWidth() - 2, 2, sHighlightBgColor, false);
		}
		mDragAndDropTarget = false;
	}

	if (mIcon)
	{
		mIcon->draw(mIndentation + ARROW_SIZE + TEXT_PAD,
					getRect().getHeight() - mIcon->getHeight());
	}

	if (!mLabel.empty())
	{
		U32 style;
		const LLFontGL* fontp = getRenderFont(style);

		// Highlight filtered text
		LLColor4 color = ((mIsSelected && filled) ? sHighlightFgColor : sFgColor);
		F32 right_x;
		F32 y = (F32)getRect().getHeight() - sFontLineHeight - (F32)TEXT_PAD;

		static LLCachedControl<F32> message_wait_time(gSavedSettings,
													  "FolderLoadingMessageWaitTime");
		if (mIsLoading &&
			mTimeSinceRequestStart.getElapsedTimeF32() >= message_wait_time)
		{
			fontp->render(sLoadingStr, 0, text_left, y, sSearchStatusColor,
						  LLFontGL::LEFT, LLFontGL::BOTTOM, style, S32_MAX,
						  S32_MAX, &right_x, false);
			text_left = right_x;
		}

		fontp->render(mWLabel, 0, text_left, y, color, LLFontGL::LEFT,
					  LLFontGL::BOTTOM, style, S32_MAX, S32_MAX, &right_x,
					  false);
		if (!mLabelSuffix.empty())
		{
			fontp->render(mWLabelSuffix, 0, right_x, y, sSuffixColor,
						  LLFontGL::LEFT, LLFontGL::BOTTOM, style, S32_MAX,
						  S32_MAX, &right_x, false);
		}

		if (mStringMatchOffset != std::string::npos)
		{
			// Do not draw backgrounds for zero-length strings
			std::string combined_string = mLabel + mLabelSuffix;
			S32 filter_string_length = mRoot->getFilterSubString().size();
			std::string combined_string_upper = combined_string;
			LLStringUtil::toUpper(combined_string_upper);
			if (filter_string_length > 0 && (mRoot->getSearchType() & 1) &&
				combined_string_upper.find(mRoot->getFilterSubString()) == mStringMatchOffset)
			{
				S32 left = ll_round(text_left) +
						   fontp->getWidth(combined_string, 0,
										   mStringMatchOffset) - 1;
				S32 right = left +
							fontp->getWidth(combined_string, mStringMatchOffset,
											filter_string_length) + 2;
				S32 bottom = llfloor(getRect().getHeight() - sFontLineHeight - 3);
				S32 top = getRect().getHeight();

				LLRect box_rect(left, top, right, bottom);
				LLUIImage::sRoundedSquare->draw(box_rect, sFilterBGColor);
				F32 match_string_left = text_left +
										fontp->getWidthF32(combined_string, 0,
														   mStringMatchOffset);
				F32 y = (F32)getRect().getHeight() - sFontLineHeight -
						(F32)TEXT_PAD;
				fontp->renderUTF8(combined_string, mStringMatchOffset,
								  match_string_left, y, sFilterTextColor,
								  LLFontGL::LEFT, LLFontGL::BOTTOM, style,
								  filter_string_length, S32_MAX, &right_x,
								  false);
			}
		}
	}

	if (sDebugRects)
	{
		drawDebugRect();
	}
}

//-----------------------------------------------------------------------------
// LLFolderViewFolder class
//-----------------------------------------------------------------------------

// Default constructor
LLFolderViewFolder::LLFolderViewFolder(const std::string& name,
									   LLUIImagePtr icon,
									   LLFolderView* root,
									   LLFolderViewEventListener* listener)
:	LLFolderViewItem(name, icon, 0, root, listener),	// 0 = no create time
	mNumDescendantsSelected(0),
	mIsOpen(false),
	mRegisterLastOpen(true),
	mForceFetched(false),
	mCurHeight(0.f),
	mTargetHeight(0.f),
	mAutoOpenCountdown(0.f),
	mSubtreeCreationDate(0),
	mAmTrash(-1),
	mAmCOF(-1),
	mAmMarket(-1),
	mLastArrangeGeneration(-1),
	mLastCalculatedWidth(0),
	mCompletedFilterGeneration(-1),
	mMostFilteredDescendantGeneration(-1)
{
	mType = std::string("(folder)");
}

//virtual
LLFolderViewFolder::~LLFolderViewFolder()
{
	// The LLView base class takes care of object destruction. make sure that
	// we do not have mouse or keyboard focus
	gFocusMgr.releaseFocusIfNeeded(this);	// Calls onCommit()
}

// Returns true on success
//virtual
bool LLFolderViewFolder::addToFolder(LLFolderViewFolder* folder,
									 LLFolderView* root)
{
	if (!folder || !root || !getListener())
	{
		return false;
	}
	mParentFolder = folder;
	root->addItemID(getListener()->getUUID(), this);
	return folder->addFolder(this);
}

// Finds width and height of this object and its children. Also makes sure that
// this view and its children are the right size.
//virtual
S32 LLFolderViewFolder::arrange(S32* width, S32* height, S32 filter_generation)
{
	mHasVisibleChildren = hasFilteredDescendants(filter_generation);

	LLInventoryFilter::EFolderShow show_folder_state =
		getRoot()->getShowFolderState();

	// Calculate height as a single item (without any children), and reshapes
	// rectangle to match
	LLFolderViewItem::arrange(width, height, filter_generation);

	// Clamp existing animated height so as to never get smaller than a single
	// item
	mCurHeight = llmax((F32)*height, mCurHeight);

	// Initialize running height value as height of single item in case we have
	// no children
	*height = getItemHeight();
	F32 running_height = (F32)*height;
	F32 target_height = (F32)*height;

	// Are my children visible ?
	if (needsArrange())
	{
		// Set last arrange generation first, in case children are animating
		// and need to be arranged again
		mLastArrangeGeneration = mRoot->getArrangeGeneration();
		if (mIsOpen)
		{
			// Add sizes of children
			S32 parent_item_height = getRect().getHeight();

			for (folders_t::iterator fit = mFolders.begin(),
									 end = mFolders.end();
				 fit != end; ++fit)
			{
				LLFolderViewFolder* folderp = (*fit);

				bool visible =
					show_folder_state == LLInventoryFilter::SHOW_ALL_FOLDERS ||
					(folderp->getFiltered(filter_generation) ||
					 // Passed filter or has descendants that passed filter
					 folderp->hasFilteredDescendants(filter_generation));
				folderp->setVisible(visible);
				if (!visible)
				{
					continue;
				}

				S32 child_width = *width;
				S32 child_height = 0;
				S32 child_top = parent_item_height - ll_roundp(running_height);

				target_height += folderp->arrange(&child_width, &child_height,
												  filter_generation);

				running_height += (F32)child_height;
				*width = llmax(*width, child_width);
				folderp->setOrigin(0,
								   child_top - folderp->getRect().getHeight());
			}
			for (items_t::iterator iit = mItems.begin(), end = mItems.end();
				 iit != end; ++iit)
			{
				LLFolderViewItem* itemp = *iit;

				bool visible = itemp->getFiltered(filter_generation);
				itemp->setVisible(visible);
				if (!visible)
				{
					continue;
				}

				S32 child_width = *width;
				S32 child_height = 0;
				S32 child_top = parent_item_height - ll_roundp(running_height);

				target_height += itemp->arrange(&child_width, &child_height,
												filter_generation);
				// Do not change width, as this item is as wide as its parent
				// folder by construction
				itemp->reshape(itemp->getRect().getWidth(), child_height);

				running_height += (F32)child_height;
				*width = llmax(*width, child_width);
				itemp->setOrigin(0, child_top - itemp->getRect().getHeight());
			}
		}

		mTargetHeight = target_height;
		// Cache this width so next time we can just return it
		mLastCalculatedWidth = *width;
	}
	else
	{
		// Just use existing width
		*width = mLastCalculatedWidth;
	}

	// Animate current height towards target height
	if (fabsf(mCurHeight - mTargetHeight) > 1.f)
	{
		mCurHeight =
			lerp(mCurHeight, mTargetHeight,
				 LLCriticalDamp::getInterpolant(mIsOpen ? FOLDER_OPEN_TIME_CONSTANT
														: FOLDER_CLOSE_TIME_CONSTANT));

		requestArrange();

		// Hide child elements that fall out of current animated height
		for (folders_t::iterator iter = mFolders.begin();
			 iter != mFolders.end(); )
		{
			folders_t::iterator fit = iter++;
			// Number of pixels that bottom of folder label is from top of
			// parent folder
			if (getRect().getHeight() - (*fit)->getRect().mTop +
				(*fit)->getItemHeight() > ll_roundp(mCurHeight) +
										  MAX_FOLDER_ITEM_OVERLAP)
			{
				// Hide if beyond current folder height
				(*fit)->setVisible(false);
			}
		}

		for (items_t::iterator iter = mItems.begin();
			 iter != mItems.end(); )
		{
			items_t::iterator iit = iter++;
			// Number of pixels that bottom of item label is from top of parent
			// folder
			if (getRect().getHeight() - (*iit)->getRect().mBottom
				> ll_roundp(mCurHeight) + MAX_FOLDER_ITEM_OVERLAP)
			{
				(*iit)->setVisible(false);
			}
		}
	}
	else
	{
		mCurHeight = mTargetHeight;
	}

	// Do not change width as this item is already as wide as its parent folder
	reshape(getRect().getWidth(), ll_roundp(mCurHeight));

	// Pass current height value back to parent
	*height = ll_roundp(mCurHeight);

	return ll_roundp(mTargetHeight);
}

bool LLFolderViewFolder::needsArrange()
{
	return mLastArrangeGeneration < mRoot->getArrangeGeneration();
}

void LLFolderViewFolder::setCompletedFilterGeneration(S32 generation,
													  bool recurse_up)
{
	mMostFilteredDescendantGeneration =
		llmin(mMostFilteredDescendantGeneration, generation);
	mCompletedFilterGeneration = generation;
	// Only aggregate up if we are a lower (older) value
	if (recurse_up && mParentFolder &&
		generation < mParentFolder->getCompletedFilterGeneration())
	{
		mParentFolder->setCompletedFilterGeneration(generation, true);
	}
}

//virtual
void LLFolderViewFolder::filter(LLInventoryFilter& filter)
{
	S32 filter_generation = filter.getCurrentGeneration();
	// If failed to pass filter newer than must_pass_generation  you will
	// automatically fail this time, so we only check against items that have
	// passed the filter
	S32 must_pass_generation = filter.getMustPassGeneration();

	// If we have already been filtered against this generation, skip out
	if (getCompletedFilterGeneration() >= filter_generation)
	{
		return;
	}

	// Filter folder itself
	if (getLastFilterGeneration() < filter_generation)
	{
			// Folder has been compared to a valid precursor filter...
		if (getLastFilterGeneration() >= must_pass_generation &&
			// ... and did not pass the filter.
			!mFiltered)
		{
			// Go ahead and flag this folder as done
			mLastFilterGeneration = filter_generation;
		}
		else
		{
			// Filter self only on first pass through
			LLFolderViewItem::filter(filter);
		}
	}

	// All descendants have been filtered later than must pass generation but
	// none passed
	if (getCompletedFilterGeneration() >= must_pass_generation &&
		!hasFilteredDescendants(must_pass_generation))
	{
		// Do not traverse children if we've already filtered them since
		// must_pass_generation and came back with nothing
		return;
	}

	// We entered here with at least one filter iteration left check to see if
	// we have any more before continuing on to children
	if (filter.getFilterCount() < 0)
	{
		return;
	}

	// When applying a filter, matching folders get their contents downloaded
	// first
	if (filter.isNotDefault() &&
		getFiltered(filter.getMinRequiredGeneration()) &&
		(mListener && !gInventory.isCategoryComplete(mListener->getUUID())))
	{
		LLInventoryModelFetch::getInstance()->start(mListener->getUUID());
	}

	// Now query children
	for (folders_t::iterator iter = mFolders.begin();
		 iter != mFolders.end(); )
	{
		folders_t::iterator fit = iter++;
		// Have we run out of iterations this frame ?
		if (filter.getFilterCount() < 0)
		{
			break;
		}

		// mMostFilteredDescendantGeneration might have been reset in which
		// case we need to update it even for folders that do not need to be
		// filtered anymore
		if ((*fit)->getCompletedFilterGeneration() >= filter_generation)
		{
			// Track latest generation to pass any child items
			if ((*fit)->getFiltered() ||
				(*fit)->hasFilteredDescendants(filter.getMinRequiredGeneration()))
			{
				mMostFilteredDescendantGeneration = filter_generation;
				if (mRoot->needsAutoSelect())
				{
					(*fit)->setOpenArrangeRecursively(true);
				}
			}
			// Just skip it, it has already been filtered
			continue;
		}

		// Update this folders filter status (and children)
		(*fit)->filter(filter);

		// Track latest generation to pass any child items
		if ((*fit)->getFiltered() ||
			(*fit)->hasFilteredDescendants(filter_generation))
		{
			mMostFilteredDescendantGeneration = filter_generation;
			if (mRoot->needsAutoSelect())
			{
				(*fit)->setOpenArrangeRecursively(true);
			}
		}
	}

	for (items_t::iterator iter = mItems.begin();
		 iter != mItems.end(); )
	{
		items_t::iterator iit = iter++;
		if (filter.getFilterCount() < 0)
		{
			break;
		}
		if ((*iit)->getLastFilterGeneration() >= filter_generation)
		{
			if ((*iit)->getFiltered())
			{
				mMostFilteredDescendantGeneration = filter_generation;
			}
			continue;
		}

		if ((*iit)->getLastFilterGeneration() >= must_pass_generation &&
			!(*iit)->getFiltered(must_pass_generation))
		{
			// Failed to pass an earlier filter that was a subset of the
			// current one go ahead and flag this item as done
			(*iit)->setFiltered(false, filter_generation);
			continue;
		}

		(*iit)->filter(filter);

		if ((*iit)->getFiltered(filter.getMinRequiredGeneration()))
		{
			mMostFilteredDescendantGeneration = filter_generation;
		}
	}

	// If we did not use all filter iterations that means we filtered all of
	// our descendants instead of exhausting the filter count for this frame.
	if (filter.getFilterCount() > 0)
	{
		// Flag this folder as having completed filter pass for all descendants
		// with false = do not recurse up to root
		setCompletedFilterGeneration(filter_generation, false);
	}
}

//virtual
void LLFolderViewFolder::setFiltered(bool filtered, S32 filter_generation)
{
	// If this folder is now filtered, but was not before (it just passed).
	if (filtered && !mFiltered)
	{
		// Reset current height, because last time we drew it it might have had
		// more visible items than now.
		mCurHeight = 0.f;
	}

	LLFolderViewItem::setFiltered(filtered, filter_generation);
}

//virtual
void LLFolderViewFolder::dirtyFilter()
{
	// We are a folder, so invalidate our completed generation
	setCompletedFilterGeneration(-1, false);
	LLFolderViewItem::dirtyFilter();
}

bool LLFolderViewFolder::hasFilteredDescendants(S32 filter_generation)
{
	static LLCachedControl<bool> hide_cof(gSavedSettings,
										  "HideCurrentOutfitFolder");
	if (hide_cof && isCOF())
	{
		return false;
	}

	static LLCachedControl<bool> hide_mp(gSavedSettings,
										 "HideMarketplaceFolder");
	if (hide_mp && isMarketplace())
	{
		return false;
	}

	return mMostFilteredDescendantGeneration >= filter_generation;
}

bool LLFolderViewFolder::hasFilteredDescendants()
{
	return hasFilteredDescendants(mRoot->getFilter()->getCurrentGeneration());
}

void LLFolderViewFolder::recursiveIncrementNumDescendantsSelected(S32 increment)
{
	LLFolderViewFolder* parent_folder = this;
	do
	{
		parent_folder->mNumDescendantsSelected += increment;

		// Make sure we do not have negative values.
		llassert(parent_folder->mNumDescendantsSelected >= 0);

		parent_folder = parent_folder->getParentFolder();
	}
	while (parent_folder);
}

// Passes selection information on to children and record selection
// information if necessary.
//virtual
bool LLFolderViewFolder::setSelection(LLFolderViewItem* selection,
									  bool openitem,
									  bool take_keyboard_focus)
{
	bool rv = false;
	if (selection == this)
	{
	  	if (!isSelected())
		{
			selectItem();
		}
		if (mListener)
		{
			mListener->selectItem();
		}
		rv = true;
	}
	else
	{
	  	if (isSelected())
		{
			deselectItem();
		}
		rv = false;
	}

	bool child_selected = false;
	for (folders_t::iterator iter = mFolders.begin();
		 iter != mFolders.end(); )
	{
		folders_t::iterator fit = iter++;
		if ((*fit)->setSelection(selection, openitem, take_keyboard_focus))
		{
			rv = true;
			child_selected = true;
		}
	}
	for (items_t::iterator iter = mItems.begin();
		 iter != mItems.end(); )
	{
		items_t::iterator iit = iter++;
		if ((*iit)->setSelection(selection, openitem, take_keyboard_focus))
		{
			rv = true;
			child_selected = true;
		}
	}
	if (openitem && child_selected)
	{
		setOpenArrangeRecursively(true);
	}
	return rv;
}

// This method is used to change the selection of an item. Recursively traverse
// all children; if 'selection' is 'this' then change the select status if
// necessary. Returns true if the selection state of this folder or of a child
// was changed.
bool LLFolderViewFolder::changeSelection(LLFolderViewItem* selection,
										 bool selected)
{
	bool rv = false;

	if (selection == this)
	{
		if (isSelected() != selected)
		{
			rv = true;
		 	if (selected)
			{
				selectItem();
			}
			else
			{
			  	deselectItem();
			}
		}
		if (mListener && selected)
		{
			mListener->selectItem();
		}
	}

	for (folders_t::iterator iter = mFolders.begin();
		 iter != mFolders.end(); )
	{
		folders_t::iterator fit = iter++;
		if ((*fit)->changeSelection(selection, selected))
		{
			rv = true;
		}
	}
	for (items_t::iterator iter = mItems.begin();
		 iter != mItems.end(); )
	{
		items_t::iterator iit = iter++;
		if ((*iit)->changeSelection(selection, selected))
		{
			rv = true;
		}
	}

	return rv;
}

void LLFolderViewFolder::extendSelection(LLFolderViewItem* selection,
										 LLFolderViewItem* last_selected,
										 std::vector<LLFolderViewItem*>& selected_items)
{
	// Pass on to child folders first
	for (folders_t::iterator iter = mFolders.begin();
		 iter != mFolders.end(); )
	{
		folders_t::iterator fit = iter++;
		(*fit)->extendSelection(selection, last_selected, selected_items);
	}

	// Handle selection of our immediate children...
	bool reverse_select = false;
	bool found_last_selected = false;
	bool found_selection = false;
	std::vector<LLFolderViewItem*> items_to_select;
	LLFolderViewItem* item;

	// ... Folders first...
	for (folders_t::iterator iter = mFolders.begin();
		 iter != mFolders.end(); )
	{
		folders_t::iterator fit = iter++;
		item = (*fit);
		if (item == selection)
		{
			found_selection = true;
		}
		else if (item == last_selected)
		{
			found_last_selected = true;
			if (found_selection)
			{
				reverse_select = true;
			}
		}

		if (found_selection || found_last_selected)
		{
			// Deselect currently selected items so they can be pushed back on
			// queue
			if (item->isSelected())
			{
				item->changeSelection(item, false);
			}
			items_to_select.push_back(item);
		}

		if (found_selection && found_last_selected)
		{
			break;
		}
	}

	if (!(found_selection && found_last_selected))
	{
		// ,,, Then items
		for (items_t::iterator iter = mItems.begin();
			 iter != mItems.end(); )
		{
			items_t::iterator iit = iter++;
			item = (*iit);
			if (item == selection)
			{
				found_selection = true;
			}
			else if (item == last_selected)
			{
				found_last_selected = true;
				if (found_selection)
				{
					reverse_select = true;
				}
			}

			if (found_selection || found_last_selected)
			{
				// Deselect currently selected items so they can be pushed back
				// on queue
				if (item->isSelected())
				{
					item->changeSelection(item, false);
				}
				items_to_select.push_back(item);
			}

			if (found_selection && found_last_selected)
			{
				break;
			}
		}
	}

	if (found_last_selected && found_selection)
	{
		// We have a complete selection inside this folder
		for (S32 index = reverse_select ? items_to_select.size() - 1 : 0;
			 reverse_select ? index >= 0 : index < (S32)items_to_select.size();
			 reverse_select ? index-- : index++)
		{
			LLFolderViewItem* item = items_to_select[index];
			if (item->changeSelection(item, true))
			{
				selected_items.push_back(item);
			}
		}
	}
	else if (found_selection)
	{
		// Last selection was not in this folder....go ahead and select just
		// the new item
		if (selection->changeSelection(selection, true))
		{
			selected_items.push_back(selection);
		}
	}
}

void LLFolderViewFolder::recursiveDeselect(bool deselect_self)
{
	if (isSelected() && deselect_self)
	{
  		deselectItem();
	}

	if (0 == mNumDescendantsSelected)
	{
		return;
	}

	// Deselect all items in this folder.
	for (items_t::iterator iter = mItems.begin();
		 iter != mItems.end(); )
	{
		items_t::iterator iit = iter++;
		LLFolderViewItem* item = *iit;
		if (item && item->isSelected())
		{
			item->deselectItem();
		}
	}

	// Recursively deselect all folders in this folder.
	for (folders_t::iterator iter = mFolders.begin();
		 iter != mFolders.end(); )
	{
		folders_t::iterator fit = iter++;
		LLFolderViewFolder* folder = *fit;
		if (folder)
		{
			folder->recursiveDeselect(true);
		}
	}
}

void LLFolderViewFolder::destroyView()
{
	if (!getRoot()) return;	// Paranoia

	for (items_t::iterator iter = mItems.begin();
		 iter != mItems.end(); )
	{
		items_t::iterator iit = iter++;
		LLFolderViewItem* item = *iit;
		if (item && item->getListener())
		{
			getRoot()->removeItemID(item->getListener()->getUUID());
		}
	}

	std::for_each(mItems.begin(), mItems.end(), DeletePointer());
	mItems.clear();

	while (!mFolders.empty())
	{
		LLFolderViewFolder* folderp = mFolders.back();
		if (folderp)
		{
			folderp->destroyView(); // Removes entry from mFolders
		}
	}

	deleteAllChildren();

	if (mParentFolder)
	{
		mParentFolder->removeView(this);
	}
}

// Remove the specified item (and any children) if possible. Return true if the
// item was deleted.
bool LLFolderViewFolder::removeItem(LLFolderViewItem* item)
{
	if (item->remove())
	{
#if 0	// RN: this seem unneccessary as remove() moves to trash
		removeView(item);
#endif
		return true;
	}
	return false;
}

// Simply remove the view (and any children). Do not bother telling the
// listeners
void LLFolderViewFolder::removeView(LLFolderViewItem* item)
{
	if (!item || item->getParentFolder() != this)
	{
		return;
	}
	// Deselect without traversing hierarchy
	if (item->isSelected())
	{
		item->deselectItem();
	}
	getRoot()->removeFromSelectionList(item);
	extractItem(item);
	delete item;
}

// Removes the specified item from the folder, but does not delete it.
void LLFolderViewFolder::extractItem(LLFolderViewItem* item)
{
	items_t::iterator it = std::find(mItems.begin(), mItems.end(), item);
	if (it == mItems.end())
	{
		// This is an evil downcast. However, it is only doing pointer
		// comparison to find if (which it should be) the item is in the
		// container, so it is pretty safe.
		LLFolderViewFolder* f = static_cast<LLFolderViewFolder*>(item);
		folders_t::iterator ft;
		ft = std::find(mFolders.begin(), mFolders.end(), f);
		if (ft != mFolders.end())
		{
			if (*ft && (*ft)->numSelected())
			{
				recursiveIncrementNumDescendantsSelected(-(*ft)->numSelected());
			}
			mFolders.erase(ft);
		}
	}
	else
	{
		if (*it && (*it)->isSelected())
		{
			recursiveIncrementNumDescendantsSelected(-1);
		}
		mItems.erase(it);
	}
	// Item has been removed, need to update filter
	dirtyFilter();
	// Because an item is going away regardless of filter status, force
	// rearrange
	requestArrange();
	if (getRoot() && item->getListener())
	{
		getRoot()->removeItemID(item->getListener()->getUUID());
	}
	removeChild(item);
}

// This function is called by a child that needs to be resorted.
// This is only called for renaming an object because it would not work for
// date.
void LLFolderViewFolder::resort(LLFolderViewItem* item)
{
	mItems.sort(mSortFunction);
	mFolders.sort(mSortFunction);
}

bool LLFolderViewFolder::isTrash() const
{
	if (mListener && mAmTrash == -1)
	{
		const LLUUID& trash_id =
			gInventory.findCategoryUUIDForType(LLFolderType::FT_TRASH, false);
		if (trash_id.notNull())
		{
			mAmTrash = mListener->getUUID() == trash_id ? 1 : 0;
		}
	}
	return mAmTrash == 1;
}

bool LLFolderViewFolder::isCOF() const
{
	if (mListener && mAmCOF == -1)
	{
		const LLUUID cof_id = LLAppearanceMgr::getCOF();
		if (cof_id.notNull())
		{
			mAmCOF = mListener->getUUID() == cof_id ? 1 : 0;
		}
	}
	return mAmCOF == 1;
}

bool LLFolderViewFolder::isMarketplace() const
{
	if (mListener && mAmMarket == -1)
	{
		const LLUUID& market_id = LLMarketplace::getMPL();
		if (market_id.notNull())
		{
			mAmMarket = mListener->getUUID() == market_id ? 1 : 0;
		}
	}
	return mAmMarket == 1;
}

void LLFolderViewFolder::sortBy(U32 order)
{
	if (!mSortFunction.updateSort(order))
	{
		// No changes.
		return;
	}

	// Propegate this change to sub folders
	for (folders_t::iterator iter = mFolders.begin(); iter != mFolders.end(); )
	{
		folders_t::iterator fit = iter++;
		(*fit)->sortBy(order);
	}

	mFolders.sort(mSortFunction);
	mItems.sort(mSortFunction);

	if (order & LLInventoryFilter::SO_DATE)
	{
		time_t latest = 0;

		if (!mItems.empty())
		{
			LLFolderViewItem* item = *(mItems.begin());
			latest = item->getCreationDate();
		}

		if (!mFolders.empty())
		{
			LLFolderViewFolder* folder = *(mFolders.begin());
			if (folder->getCreationDate() > latest)
			{
				latest = folder->getCreationDate();
			}
		}
		mSubtreeCreationDate = latest;
	}
}

void LLFolderViewFolder::setItemSortOrder(U32 ordering)
{
	if (mSortFunction.updateSort(ordering))
	{
		for (folders_t::iterator iter = mFolders.begin();
			iter != mFolders.end(); )
		{
			folders_t::iterator fit = iter++;
			(*fit)->setItemSortOrder(ordering);
		}

		mFolders.sort(mSortFunction);
		mItems.sort(mSortFunction);
	}
}

EInventorySortGroup LLFolderViewFolder::getSortGroup() const
{
	if (isTrash())
	{
		return SG_TRASH_FOLDER;
	}

	if (mListener && mListener->getPreferredType() != LLFolderType::FT_NONE)
	{
		return SG_SYSTEM_FOLDER;
	}

	return SG_NORMAL_FOLDER;
}

bool LLFolderViewFolder::isMovable()
{
	if (mListener)
	{
		if (!mListener->isItemMovable())
		{
			return false;
		}

		for (items_t::iterator iter = mItems.begin(); iter != mItems.end(); )
		{
			items_t::iterator iit = iter++;
			if (!(*iit)->isMovable())
			{
				return false;
			}
		}

		for (folders_t::iterator iter = mFolders.begin();
			 iter != mFolders.end(); )
		{
			folders_t::iterator fit = iter++;
			if (!(*fit)->isMovable())
			{
				return false;
			}
		}
	}
	return true;
}

bool LLFolderViewFolder::isRemovable()
{
	if (mListener)
	{
		if (!(mListener->isItemRemovable()))
		{
			return false;
		}

		for (items_t::iterator iter = mItems.begin();
			 iter != mItems.end(); )
		{
			items_t::iterator iit = iter++;
			if (!(*iit)->isRemovable())
			{
				return false;
			}
		}

		for (folders_t::iterator iter = mFolders.begin();
			 iter != mFolders.end(); )
		{
			folders_t::iterator fit = iter++;
			if (!(*fit)->isRemovable())
			{
				return false;
			}
		}
	}
	return true;
}

// This is an internal method used for adding items to folders.
//virtual
bool LLFolderViewFolder::addItem(LLFolderViewItem* item)
{
	if (!item) return false;	// Paranoia

	items_t::iterator it = std::lower_bound(mItems.begin(), mItems.end(),
											item, mSortFunction);
	mItems.insert(it, item);
	if (item->isSelected())
	{
		recursiveIncrementNumDescendantsSelected(1);
	}
	item->setRect(LLRect(0, 0, getRect().getWidth(), 0));
	item->setVisible(false);
	addChild(item);
	item->dirtyFilter();
	requestArrange();

	return true;
}

// This is an internal method used for adding items to folders.
//virtual
bool LLFolderViewFolder::addFolder(LLFolderViewFolder* folder)
{
	folders_t::iterator it = std::lower_bound(mFolders.begin(), mFolders.end(),
											  folder, mSortFunction);
	mFolders.insert(it, folder);
	if (folder->numSelected())
	{
		recursiveIncrementNumDescendantsSelected(folder->numSelected());
	}
	folder->setOrigin(0, 0);
	folder->reshape(getRect().getWidth(), 0);
	folder->setVisible(false);
	addChild(folder);
	folder->dirtyFilter();

	// Rearrange all descendants too, as our indentation level might have
	// changed
	folder->requestArrange(true);

	return true;
}

void LLFolderViewFolder::requestArrange(bool include_descendants)
{
	mLastArrangeGeneration = -1;
	// Flag all items up to root
	if (mParentFolder)
	{
		mParentFolder->requestArrange();
	}

	if (include_descendants)
	{
		for (folders_t::iterator iter = mFolders.begin();
			iter != mFolders.end();
			++iter)
		{
			(*iter)->requestArrange(true);
		}
	}
}

void LLFolderViewFolder::toggleOpen()
{
	if (mRegisterLastOpen && !mIsOpen)
	{
		LLFolderViewEventListener* listenerp = getListener();
		if (listenerp)
		{
			const LLUUID& id = listenerp->getUUID();
			if (id.notNull())
			{
				sLastOpenId = id;
			}
		}
	}

	setOpen(!mIsOpen);

	// *HACK: sadly, folders do not properly retain their thumbnails Id after a
	// relog (the transmitted "inventory skeleton" does not have them, and when
	// the transmitted data leads to discard the cached one, e.g. due to a
	// version mismatch, we loose the thumbnail Id), so we need to refresh the
	// folder data once after a relog; we do so by force-fetching (in a non
	// recursive way) the contents of any newly opened folder. HB
	if (!mForceFetched && mRoot->showThumbnails())
	{
		mForceFetched = true;
		LLFolderViewEventListener* listenerp = getListener();
		if (listenerp)
		{
			LLInventoryModelFetch::forceFetchFolder(listenerp->getUUID());
		}
	}
}

// Force a folder open or closed
void LLFolderViewFolder::setOpen(bool openitem)
{
	setOpenArrangeRecursively(openitem);
}

void LLFolderViewFolder::setOpenArrangeRecursively(bool openitem,
												   ERecurseType recurse)
{
	bool was_open = mIsOpen;
	mIsOpen = openitem;
	if (!was_open && openitem)
	{
		if (mListener)
		{
			mListener->openItem();
		}
	}

	if (recurse == RECURSE_DOWN || recurse == RECURSE_UP_DOWN)
	{
		for (folders_t::iterator iter = mFolders.begin();
			 iter != mFolders.end(); )
		{
			folders_t::iterator fit = iter++;
			(*fit)->setOpenArrangeRecursively(openitem, RECURSE_DOWN);
		}
	}
	if (mParentFolder && (recurse == RECURSE_UP || recurse == RECURSE_UP_DOWN))
	{
		mParentFolder->setOpenArrangeRecursively(openitem, RECURSE_UP);
	}

	if (was_open != mIsOpen)
	{
		requestArrange();
	}
}

bool LLFolderViewFolder::handleDragAndDropFromChild(MASK mask, bool drop,
													EDragAndDropType c_type,
													void* cargo_data,
													EAcceptance* accept,
													std::string& tooltip_msg)
{
	bool accepted = mListener &&
					mListener->dragOrDrop(mask, drop, c_type, cargo_data,
										  tooltip_msg);
	if (accepted)
	{
		mDragAndDropTarget = true;
		*accept = ACCEPT_YES_MULTI;
	}
	else
	{
		*accept = ACCEPT_NO;
	}
	LLFolderViewEventListener::dragOrDropTip(drop, tooltip_msg);

	// Drag and drop to child item, so clear pending auto-opens
	getRoot()->autoOpenTest(NULL);

	return true;
}

void LLFolderViewFolder::openItem()
{
	toggleOpen();
}

void LLFolderViewFolder::applyFunctorRecursively(LLFolderViewFunctor& functor)
{
	functor.doFolder(this);

	for (folders_t::iterator iter = mFolders.begin(); iter != mFolders.end(); )
	{
		folders_t::iterator fit = iter++;
		(*fit)->applyFunctorRecursively(functor);
	}
	for (items_t::iterator iter = mItems.begin(); iter != mItems.end(); )
	{
		items_t::iterator iit = iter++;
		functor.doItem((*iit));
	}
}

void LLFolderViewFolder::applyListenerFunctorRecursively(LLFolderViewListenerFunctor& functor)
{
	functor(mListener);
	for (folders_t::iterator iter = mFolders.begin(); iter != mFolders.end(); )
	{
		folders_t::iterator fit = iter++;
		(*fit)->applyListenerFunctorRecursively(functor);
	}
	for (items_t::iterator iter = mItems.begin(); iter != mItems.end(); )
	{
		items_t::iterator iit = iter++;
		(*iit)->applyListenerFunctorRecursively(functor);
	}
}

// LLView functionality
bool LLFolderViewFolder::handleDragAndDrop(S32 x, S32 y, MASK mask, bool drop,
										   EDragAndDropType cargo_type,
										   void* cargo_data,
										   EAcceptance* accept,
										   std::string& tooltip_msg)
{
	LLFolderView* root_view = getRoot();

	bool handled = false;
	if (mIsOpen)
	{
		handled = childrenHandleDragAndDrop(x, y, mask, drop, cargo_type,
											cargo_data, accept,
											tooltip_msg) != NULL;
	}

	if (!handled)
	{
		bool accepted = mListener &&
						mListener->dragOrDrop(mask, drop, cargo_type,
											  cargo_data,
											  tooltip_msg);
		if (accepted)
		{
			mDragAndDropTarget = true;
			*accept = ACCEPT_YES_MULTI;
		}
		else
		{
			*accept = ACCEPT_NO;
		}
		LLFolderViewEventListener::dragOrDropTip(drop, tooltip_msg);

		if (!drop && accepted)
		{
			root_view->autoOpenTest(this);
		}

		LL_DEBUGS("UserInput") << "dragAndDrop handled with: drop = "
							   << (drop ? "true" : "false") << " - accepted = "
							   <<  (accepted ? "true" : "false") << LL_ENDL;
	}

	return true;
}

bool LLFolderViewFolder::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
	bool handled = false;
	// Fetch contents of this folder, as context menu can depend on contents
	// still, user would have to open context menu again to see the changes
	gInventory.fetchDescendentsOf(mListener->getUUID());

	if (mIsOpen)
	{
		handled = childrenHandleRightMouseDown(x, y, mask) != NULL;
	}
	if (!handled)
	{
		handled = LLFolderViewItem::handleRightMouseDown(x, y, mask);
	}
	return handled;
}

bool LLFolderViewFolder::handleHover(S32 x, S32 y, MASK mask)
{
	bool handled = LLView::handleHover(x, y, mask);
	if (!handled)
	{
		// This does not do child processing
		handled = LLFolderViewItem::handleHover(x, y, mask);
	}

#if 0
	if (x < LEFT_INDENTATION + mIndentation &&
		x > mIndentation - LEFT_PAD && y > getRect().getHeight() -)
	{
		gViewerWindowp->setCursor(UI_CURSOR_ARROW);
		handled = true;
	}
#endif

	return handled;
}

bool LLFolderViewFolder::handleMouseDown(S32 x, S32 y, MASK mask)
{
	bool handled = false;
	if (mIsOpen)
	{
		handled = childrenHandleMouseDown(x, y, mask) != NULL;
	}
	if (!handled)
	{
		if (x < LEFT_INDENTATION + mIndentation && x > mIndentation - LEFT_PAD)
		{
			toggleOpen();
			handled = true;
		}
		else
		{
			// Do normal selection logic
			handled = LLFolderViewItem::handleMouseDown(x, y, mask);
		}
	}

	return handled;
}

bool LLFolderViewFolder::handleDoubleClick(S32 x, S32 y, MASK mask)
{
	bool handled = false;
	if (mIsOpen)
	{
		handled = childrenHandleDoubleClick(x, y, mask) != NULL;
	}
	if (!handled)
	{
		if (x < LEFT_INDENTATION + mIndentation && x > mIndentation - LEFT_PAD)
		{
			// Do not select when user double-clicks plus sign so as not to
			// contradict single-click behavior
			toggleOpen();
		}
		else
		{
			setSelectionFromRoot(this, false);
			toggleOpen();
		}
		handled = true;
	}
	return handled;
}

void LLFolderViewFolder::draw()
{
	if (mAutoOpenCountdown != 0.f)
	{
		mControlLabelRotation = mAutoOpenCountdown * -90.f;
	}
	else if (mIsOpen)
	{
		mControlLabelRotation = lerp(mControlLabelRotation, -90.f,
									 LLCriticalDamp::getInterpolant(0.04f));
	}
	else
	{
		mControlLabelRotation = lerp(mControlLabelRotation, 0.f,
									 LLCriticalDamp::getInterpolant(0.025f));
	}

	bool possibly_has_children = false;
	bool up_to_date = mListener && mListener->isUpToDate();
	// We know we have children but have not fetched them (does not obey
	// filter)
	if (!up_to_date && mListener && mListener->hasChildren())
	{
		possibly_has_children = true;
	}

	bool loading = mIsOpen && possibly_has_children && !up_to_date;
	if (loading && !mIsLoading)
	{
		// Measure how long we have been in the loading state
		mTimeSinceRequestStart.reset();
	}
	mIsLoading = loading;

	LLFolderViewItem::draw();

	// Draw children if root folder, or any other folder that is open or
	// animating to closed state
	if (mIsOpen || mCurHeight != mTargetHeight || getRoot() == this)
	{
		LLView::draw();
	}
}

time_t LLFolderViewFolder::getCreationDate() const
{
	return llmax<time_t>(mCreationDate, mSubtreeCreationDate);
}

//virtual
bool LLFolderViewFolder::potentiallyVisible()
{
	// Folder should be visible by its own filter status..
	return LLFolderViewItem::potentiallyVisible() ||
		   // Or one or more of its descendants have passed the minimum filter
		   // requirement...
		   hasFilteredDescendants(mRoot->getFilter()->getMinRequiredGeneration()) ||
		   // Or not all of its descendants have been checked against minimum
		   // filter requirement
		   getCompletedFilterGeneration() < getRoot()->getFilter()->getMinRequiredGeneration();
}

// This does prefix traversal, as folders are listed above their contents
LLFolderViewItem* LLFolderViewFolder::getNextFromChild(LLFolderViewItem* item,
													   bool include_children)
{
	bool found_item = false;

	LLFolderViewItem* result = NULL;
	// When not starting from a given item, start at beginning
	if (!item)
	{
		found_item = true;
	}

	// Find current item among children
	folders_t::iterator fit = mFolders.begin();
	folders_t::iterator fend = mFolders.end();

	items_t::iterator iit = mItems.begin();
	items_t::iterator iend = mItems.end();

	// If not trivially starting at the beginning, we have to find the current
	// item
	if (!found_item)
	{
		// First, look among folders, since they are always above items
		for ( ; fit != fend; ++fit)
		{
			if (item == (*fit))
			{
				found_item = true;
				// If we are on downwards traversal
				if (include_children && (*fit)->isOpen())
				{
					// Look for first descendant
					return (*fit)->getNextFromChild(NULL, true);
				}
				// Otherwise advance to next folder
				++fit;
				include_children = true;
				break;
			}
		}

		// Did not find in folders ?  Check items...
		if (!found_item)
		{
			for ( ; iit != iend; ++iit)
			{
				if (item == (*iit))
				{
					found_item = true;
					// Point to next item
					++iit;
					break;
				}
			}
		}
	}

	if (!found_item)
	{
		// You should never call this method with an item that is not a child
		// so we should always find something
		llassert(false);
		return NULL;
	}

	// At this point, either iit or fit point to a candidate "next" item if
	// both are out of range, we need to punt up to our parent.

	// Now, starting from found folder, continue through folders searching for
	// next visible folder
	while (fit != fend && !(*fit)->getVisible())
	{
		// Turn on downwards traversal for next folder
		++fit;
	}

	if (fit != fend)
	{
		result = *fit;
	}
	else
	{
		// Otherwise, scan for next visible item
		while (iit != iend && !(*iit)->getVisible())
		{
			++iit;
		}

		// Check to see if we have a valid item
		if (iit != iend)
		{
			result = *iit;
		}
	}

	if (!result && mParentFolder)
	{
		// If there are no siblings or children to go to, recurse up one level
		// in the tree and skip children for this folder, as we've already
		// discounted them
		result = mParentFolder->getNextFromChild(this, false);
	}

	return result;
}

// This does postfix traversal, as folders are listed above their contents
LLFolderViewItem* LLFolderViewFolder::getPreviousFromChild(LLFolderViewItem* item,
														   bool include_children)
{
	bool found_item = false;

	LLFolderViewItem* result = NULL;
	// When not starting from a given item, start at end
	if (!item)
	{
		found_item = true;
	}

	// Find current item among children
	folders_t::reverse_iterator fit = mFolders.rbegin();
	folders_t::reverse_iterator fend = mFolders.rend();

	items_t::reverse_iterator iit = mItems.rbegin();
	items_t::reverse_iterator iend = mItems.rend();

	// If not trivially starting at the end, we have to find the current item.
	if (!found_item)
	{
		// First, look among items, since they are always below the folders
		for ( ; iit != iend; ++iit)
		{
			if (item == (*iit))
			{
				found_item = true;
				// Point to next item
				++iit;
				break;
			}
		}

		// Did not find in items ?  Check folders...
		if (!found_item)
		{
			for ( ; fit != fend; ++fit)
			{
				if (item == (*fit))
				{
					found_item = true;
					// Point to next folder
					++fit;
					break;
				}
			}
		}
	}

	if (!found_item)
	{
		// You should never call this method with an item that is not a child
		// so we should always find something
		llassert(false);
		return NULL;
	}

	// At this point, either iit or fit point to a candidate "next" item if
	// both are out of range, we need to punt up to our parent.

	// Now, starting from found item, continue through items searching for next
	// visible item
	while (iit != iend && !(*iit)->getVisible())
	{
		++iit;
	}

	if (iit != iend)
	{
		// We found an appropriate item
		result = *iit;
	}
	else
	{
		// Otherwise, scan for next visible folder
		while (fit != fend && !(*fit)->getVisible())
		{
			++fit;
		}

		// Check to see if we have a valid folder
		if (fit != fend)
		{
			// Try selecting child element of this folder
			if ((*fit)->isOpen())
			{
				result = (*fit)->getPreviousFromChild(NULL);
			}
			else
			{
				result = *fit;
			}
		}
	}

	if (!result)
	{
		// If there are no siblings or children to go to, recurse up one level
		// in the tree which gets back to this folder, which will only be
		// visited if it is a valid, visible item
		result = this;
	}

	return result;
}

//---------------------------------------------------------------------------

// Tells all folders in a folderview to sort their items
// (and only their items, not folders) by a certain function.
class LLSetItemSortFunction final : public LLFolderViewFunctor
{
public:
	LL_INLINE LLSetItemSortFunction(U32 ordering)
	:	mSortOrder(ordering)
	{
	}

	LL_INLINE ~LLSetItemSortFunction() override
	{
	}

	// Set the sort order.
	LL_INLINE void doFolder(LLFolderViewFolder* folder) override
	{
		folder->setItemSortOrder(mSortOrder);
	}

	// Do nothing.
	LL_INLINE void doItem(LLFolderViewItem*) override	{}

public:
	U32 mSortOrder;
};

//---------------------------------------------------------------------------

//virtual
void LLOpenFolderByID::doFolder(LLFolderViewFolder* folder)
{
	if (folder->getListener() && folder->getListener()->getUUID() == mID)
	{
		folder->setOpenArrangeRecursively(true,
										  LLFolderViewFolder::RECURSE_UP);
	}
}

//---------------------------------------------------------------------------

// Tells all folders in a folderview to close themselves
// For efficiency, calls setOpenArrangeRecursively().
// The calling function must then call:
//	LLFolderView* root = getRoot();
//	if (root)
//	{
//		root->arrange(NULL, NULL);
//		root->scrollToShowSelection();
//	}
// to patch things up.
class LLCloseAllFoldersFunctor : public LLFolderViewFunctor
{
public:
	LL_INLINE LLCloseAllFoldersFunctor(bool close)
	:	mOpen(!close)
	{
	}

	LL_INLINE ~LLCloseAllFoldersFunctor() override		{}

	void doFolder(LLFolderViewFolder* folder) override;
	void doItem(LLFolderViewItem* item) override;

public:
	bool mOpen;
};

// Set the sort order.
void LLCloseAllFoldersFunctor::doFolder(LLFolderViewFolder* folder)
{
	folder->setOpenArrangeRecursively(mOpen);
}

// Do nothing.
void LLCloseAllFoldersFunctor::doItem(LLFolderViewItem* item)
{
}

//-----------------------------------------------------------------------------
// LLFolderView class
//-----------------------------------------------------------------------------

//static
LLFolderView* LLFolderView::getInstance(const LLUUID& id)
{
	instances_map_t::iterator it = sInstances.find(id);
	return it == sInstances.end() ? NULL : it->second;
}

LLFolderView::LLFolderView(const std::string& name, LLUIImagePtr folder_icon,
						   const LLRect& rect, const LLUUID& source_id,
						   LLPanel* parent_panel)
:	LLFolderViewFolder(name, folder_icon, this, NULL),
	mParentPanel(parent_panel),
	mScrollContainer(NULL),
	mContextMenuCreated(false),
	mAllowMultiSelect(true),
	mSourceID(source_id),
	mFolderViewId(source_id),
	mRenameItem(NULL),
	mNeedsScroll(false),
	mLastScrollItem(NULL),
	mCanAutoSelect(true),
	mNeedsAutoSelect(false),
	mNeedsAutoRename(false),
	// This gets overridden by a preference shortly after:
	mSortOrder(LLInventoryFilter::SO_FOLDERS_BY_NAME),
	mSearchType(1),
	mFilter(name),
	mShowSelectionContext(false),
	mShowSingleSelection(false),
	mArrangeGeneration(0),
	mUserData(NULL),
	mSelectCallback(NULL),
	mSignalSelectCallback(0),
	mMinWidth(0),
	mHasCapture(false),
	mDragAndDropThisFrame(false),
	mShowThumbnails(false),
	mGotLeftMouseClick(false)
{
	if (mFolderViewId.isNull())
	{
		mFolderViewId.generate();
	}
	sInstances[mFolderViewId] = this;

	LLRect new_rect(rect.mLeft, rect.mBottom + getRect().getHeight(),
					rect.mLeft + getRect().getWidth(), rect.mBottom);
	setRect(rect);
	reshape(rect.getWidth(), rect.getHeight());
	mIsOpen = true;	// This view is always open.
	mAutoOpenItems.setDepth(AUTO_OPEN_STACK_DEPTH);
	mAutoOpenCandidate = NULL;
	mAutoOpenTimer.stop();
	mKeyboardSelection = false;
	mIndentation = -LEFT_INDENTATION;	// Children start at indentation 0
	gIdleCallbacks.addFunction(idle, this);

	// Clear label. Go ahead and render root folder as usual; just make sure
	// the label ("Inventory Folder") never shows up
	mLabel.clear();
	mWLabel.clear();

	mRenamer = new LLLineEditor("ren", getRect(), LLStringUtil::null, sFont,
								DB_INV_ITEM_NAME_STR_LEN, commitRename, NULL,
								NULL, this,
								LLLineEditor::prevalidatePrintableNotPipe);
	// Escape is handled by reverting the rename, not commiting it (default
	// behaviour)
	mRenamer->setCommitOnFocusLost(true);
	mRenamer->setVisible(false);
	addChild(mRenamer);

	setTabStop(true);
}

//virtual
LLFolderView::~LLFolderView()
{
	sInstances.erase(mFolderViewId);

	// The release focus call can potentially call the scrollcontainer, which
	// can potentially be called with a partly destroyed scollcontainer. Just
	// null it out here, and no worries about calling into the invalid scroll
	// container. Same with the renamer.
	mScrollContainer = NULL;
	mRenameItem = NULL;
	mRenamer = NULL;
	gFocusMgr.releaseFocusIfNeeded(this);

	mAutoOpenItems.removeAllNodes();
	gIdleCallbacks.deleteFunction(idle, this);

	deleteViewByHandle(mPopupMenuHandle);

	if (mRenamer == gFocusMgr.getTopCtrl())
	{
		gFocusMgr.setTopCtrl(NULL);
	}

	mAutoOpenItems.removeAllNodes();
	clearSelection();
	mItems.clear();
	mFolders.clear();
	mItemMap.clear();
}

LLMenuGL* LLFolderView::getContextMenu()
{
	LLMenuGL* menup = (LLMenuGL*)mPopupMenuHandle.get();
	if (menup || mContextMenuCreated)	// Do not re-create a deleted menu.
	{
		return menup;
	}
	mContextMenuCreated = true;

	menup = LLUICtrlFactory::getInstance()->buildMenu("menu_inventory.xml",
													  mParentPanel);
	if (!menup)
	{
		menup = new LLMenuGL(LLStringUtil::null);
	}
	menup->setBackgroundColor(sContextMenuBgColor);
	menup->setVisible(false);
	mPopupMenuHandle = menup->getHandle();
	return menup;
}

void LLFolderView::checkTreeResortForModelChanged()
{
	if (mSortOrder & LLInventoryFilter::SO_DATE &&
		!(mSortOrder & LLInventoryFilter::SO_FOLDERS_BY_NAME))
	{
		// This is the case where something got added or removed. If we are
		// date sorting everything including folders, then we need to rebuild
		// the whole tree. Just set to something not SO_DATE to force the
		// folder most resent date resort.
		mSortOrder = mSortOrder & ~LLInventoryFilter::SO_DATE;
		setSortOrder(mSortOrder | LLInventoryFilter::SO_DATE);
	}
}

void LLFolderView::setSortOrder(U32 order)
{
	if (order != mSortOrder)
	{
		LL_FAST_TIMER(FTM_SORT);
		mSortOrder = order;

		for (folders_t::iterator iter = mFolders.begin();
			 iter != mFolders.end(); )
		{
			folders_t::iterator fit = iter++;
			(*fit)->sortBy(order);
		}

		arrangeAll();
	}
}

U32 LLFolderView::getSortOrder() const
{
	return mSortOrder;
}

U32 LLFolderView::toggleSearchType(std::string toggle)
{
	if (toggle == "name")
	{
		if (mSearchType & 1)
		{
			mSearchType &= 6;
		}
		else
		{
			mSearchType |= 1;
		}
	}
	else if (toggle == "description")
	{
		if (mSearchType & 2)
		{
			mSearchType &= 5;
		}
		else
		{
			mSearchType |= 2;
		}
	}
	else if (toggle == "creator")
	{
		if (mSearchType & 4)
		{
			mSearchType &= 3;
		}
		else
		{
			mSearchType |= 4;
		}
	}
	if (mSearchType == 0)
	{
		mSearchType = 1;
	}

	if (getFilterSubString().length())
	{
		mFilter.setModified(LLInventoryFilter::FILTER_RESTART);
	}

	return mSearchType;
}

U32 LLFolderView::getSearchType() const
{
	return mSearchType;
}

//virtual
bool LLFolderView::addFolder(LLFolderViewFolder* folder)
{
	if (!folder) return false;	// Paranoia

	// Enforce sort order of "My inventory" followed by Library
	if (folder->getListener() &&
		folder->getListener()->getUUID() == gInventory.getLibraryRootFolderID())
	{
		mFolders.push_back(folder);
	}
	else
	{
		mFolders.insert(mFolders.begin(), folder);
	}
	if (folder->numSelected())
	{
		recursiveIncrementNumDescendantsSelected(folder->numSelected());
	}
	folder->setOrigin(0, 0);
	folder->reshape(getRect().getWidth(), 0);
	folder->setVisible(false);
	addChild(folder);
	folder->dirtyFilter();
	folder->requestArrange();

	return true;
}

void LLFolderView::closeAllFolders()
{
	// Close all the folders
	setOpenArrangeRecursively(false, LLFolderViewFolder::RECURSE_DOWN);
}

void LLFolderView::openFolder(const std::string& foldername)
{
	LLFolderViewFolder* inv = getChild<LLFolderViewFolder>(foldername.c_str());
	if (inv)
	{
		setSelection(inv, false, false);
		inv->setOpen(true);
	}
}

void LLFolderView::openFolder(const LLUUID& cat_id)
{
	LLFolderViewFolder* inv =
		dynamic_cast<LLFolderViewFolder*>(getItemByID(cat_id));
	if (inv)
	{
		setSelection(inv, false, false);
		inv->setOpen(true);
	}
}

void LLFolderView::setOpenArrangeRecursively(bool openitem,
											 ERecurseType recurse)
{
	// Call base class to do proper recursion
	LLFolderViewFolder::setOpenArrangeRecursively(openitem, recurse);
	// Make sure root folder is always open
	mIsOpen = true;
}

// This view grows and shinks to enclose all of its children items and folders.
//virtual
S32 LLFolderView::arrange(S32* unused_width, S32* unused_height,
						  S32 filter_generation)
{
	LL_FAST_TIMER(FTM_ARRANGE);

	filter_generation = mFilter.getMinRequiredGeneration();
	mMinWidth = 0;

	mHasVisibleChildren = hasFilteredDescendants(filter_generation);
	// Arrange always finishes, so optimistically set the arrange generation to
	// the most current
	mLastArrangeGeneration = mRoot->getArrangeGeneration();

	LLInventoryFilter::EFolderShow show_folder_state = getRoot()->getShowFolderState();

	S32 total_width = LEFT_PAD;
	S32 running_height = 0;
	S32 target_height = running_height;
	S32 parent_item_height = getRect().getHeight();

	for (folders_t::iterator iter = mFolders.begin();
		 iter != mFolders.end(); )
	{
		folders_t::iterator fit = iter++;
		LLFolderViewFolder* folderp = *fit;
		bool visible =
			// Always show folders ?
			show_folder_state == LLInventoryFilter::SHOW_ALL_FOLDERS ||
			// Passed filter...
			folderp->getFiltered(filter_generation) ||
			// ...or has descendants that passed filter
			folderp->hasFilteredDescendants(filter_generation);
		folderp->setVisible(visible);
		if (visible)
		{
			S32 child_top = parent_item_height - running_height;

			S32 child_height = 0;
			S32 child_width = 0;
			target_height += folderp->arrange(&child_width, &child_height,
											  filter_generation);

			mMinWidth = llmax(mMinWidth, child_width);
			total_width = llmax(total_width, child_width);
			running_height += child_height;

			folderp->setOrigin(ICON_PAD,
							   child_top - (*fit)->getRect().getHeight());
		}
	}

	for (items_t::iterator iter = mItems.begin(); iter != mItems.end(); )
	{
		items_t::iterator iit = iter++;
		LLFolderViewItem* itemp = *iit;
		bool visible = itemp->getFiltered(filter_generation);
		itemp->setVisible(visible);
		if (visible)
		{
			S32 child_top = parent_item_height - running_height;

			S32 child_width = 0;
			S32 child_height = 0;
			target_height += itemp->arrange(&child_width, &child_height,
											filter_generation);

			itemp->reshape(itemp->getRect().getWidth(), child_height);
			mMinWidth = llmax(mMinWidth, child_width);
			total_width = llmax(total_width, child_width);
			running_height += child_height;

			itemp->setOrigin(ICON_PAD,
							 child_top - itemp->getRect().getHeight());
		}
	}

	S32 dummy_s32;
	bool dummy_bool;
	S32 min_width;
	mScrollContainer->calcVisibleSize(&min_width, &dummy_s32, &dummy_bool,
									  &dummy_bool);
	reshape(llmax(min_width, total_width), running_height);

	S32 new_min_width;
	mScrollContainer->calcVisibleSize(&new_min_width, &dummy_s32, &dummy_bool,
									  &dummy_bool);
	if (new_min_width != min_width)
	{
		reshape(llmax(min_width, total_width), running_height);
	}

	mTargetHeight = (F32)target_height;
	return ll_roundp(mTargetHeight);
}

const std::string LLFolderView::getFilterSubString(bool trim)
{
	return mFilter.getFilterSubString(trim);
}

//virtual
void LLFolderView::filter(LLInventoryFilter& filter)
{
	LL_FAST_TIMER(FTM_FILTER);
	static LLCachedControl<S32> filter_items_per_frame(gSavedSettings,
													   "FilterItemsPerFrame");
	filter.setFilterCount(llclamp((S32)filter_items_per_frame, 1, 5000));

	if (getCompletedFilterGeneration() < filter.getCurrentGeneration())
	{
		mFiltered = false;
		mMinWidth = 0;
		LLFolderViewFolder::filter(filter);
	}
}

void LLFolderView::reshape(S32 width, S32 height, bool called_from_parent)
{
	S32 min_width = 0;
	S32 dummy_height;
	bool dummy_bool;
	if (mScrollContainer)
	{
		mScrollContainer->calcVisibleSize(&min_width, &dummy_height,
										  &dummy_bool, &dummy_bool);
	}
	width = llmax(mMinWidth, min_width);
	LLView::reshape(width, height, called_from_parent);
}

void LLFolderView::addToSelectionList(LLFolderViewItem* item)
{
	if (item->isSelected())
	{
		removeFromSelectionList(item);
	}
	if (!mSelectedItems.empty())
	{
		mSelectedItems.back()->setIsCurSelection(false);
	}
	item->setIsCurSelection(true);
	mSelectedItems.push_back(item);
}

void LLFolderView::removeFromSelectionList(LLFolderViewItem* item)
{
	if (mSelectedItems.size())
	{
		mSelectedItems.back()->setIsCurSelection(false);
	}

	selected_items_t::iterator item_iter;
	for (item_iter = mSelectedItems.begin();
		 item_iter != mSelectedItems.end(); )
	{
		if (*item_iter == item)
		{
			item_iter = mSelectedItems.erase(item_iter);
		}
		else
		{
			++item_iter;
		}
	}
	if (mSelectedItems.size())
	{
		mSelectedItems.back()->setIsCurSelection(true);
	}
}

LLFolderViewItem* LLFolderView::getCurSelectedItem()
{
	if (mSelectedItems.size())
	{
		LLFolderViewItem* itemp = mSelectedItems.back();
		llassert(itemp->getIsCurSelection());
		return itemp;
	}
	return NULL;
}

// Record the selected item and pass it down the hierachy.
//virtual
bool LLFolderView::setSelection(LLFolderViewItem* selection, bool openitem,
								bool take_keyboard_focus)
{
	if (selection == this)
	{
		return false;
	}

	if (selection && take_keyboard_focus)
	{
		setFocus(true);
	}

	// Clear selection down here because change of keyboard focus can
	// potentially affect selection
	clearSelection();

	if (selection)
	{
		addToSelectionList(selection);
	}

	bool rv = LLFolderViewFolder::setSelection(selection, openitem,
											   take_keyboard_focus);
	if (openitem && selection)
	{
		selection->getParentFolder()->requestArrange();
	}

	llassert(mSelectedItems.size() <= 1);

	mSignalSelectCallback = take_keyboard_focus ? SIGNAL_KEYBOARD_FOCUS
												: SIGNAL_NO_KEYBOARD_FOCUS;

	return rv;
}

bool LLFolderView::changeSelection(LLFolderViewItem* selection, bool selected)
{
	// Cannot select the root folder
	if (!selection || selection == this)
	{
		return false;
	}

	if (!mAllowMultiSelect)
	{
		clearSelection();
	}

	selected_items_t::iterator item_iter;
	for (item_iter = mSelectedItems.begin(); item_iter != mSelectedItems.end();
		 ++item_iter)
	{
		if (*item_iter == selection)
		{
			break;
		}
	}

	bool on_list = item_iter != mSelectedItems.end();
	if (selected && !on_list)
	{
		addToSelectionList(selection);
	}
	if (!selected && on_list)
	{
		removeFromSelectionList(selection);
	}

	bool rv = LLFolderViewFolder::changeSelection(selection, selected);

	mSignalSelectCallback = SIGNAL_KEYBOARD_FOCUS;

	return rv;
}

void LLFolderView::extendSelection(LLFolderViewItem* selection,
								   LLFolderViewItem* last_selected,
								   std::vector<LLFolderViewItem*>& items)
{
	// Now store resulting selection
	if (mAllowMultiSelect)
	{
		LLFolderViewItem *cur_selection = getCurSelectedItem();
		LLFolderViewFolder::extendSelection(selection, cur_selection, items);
		for (S32 i = 0, count = items.size(); i < count; ++i)
		{
			addToSelectionList(items[i]);
		}
	}
	else
	{
		setSelection(selection, false, false);
	}

	mSignalSelectCallback = SIGNAL_KEYBOARD_FOCUS;
}

void LLFolderView::sanitizeSelection()
{
	// Store off current item in case it is automatically deselected and we
	// want to preserve context
	LLFolderViewItem* orig_selected = getCurSelectedItem();

	// Cache "Show all folders" filter setting
	bool show_all_folders =
		getRoot()->getShowFolderState() == LLInventoryFilter::SHOW_ALL_FOLDERS;

	std::vector<LLFolderViewItem*> items_to_remove;
	selected_items_t::iterator item_iter;
	for (item_iter = mSelectedItems.begin(); item_iter != mSelectedItems.end();
		 ++item_iter)
	{
		LLFolderViewItem* item = *item_iter;

		// Ensure that each ancestor is open and potentially passes filtering.
		// Initialize from filter state for this item
		bool visible = item->potentiallyVisible();

		// Modify with parent open and filters states
		LLFolderViewFolder* parent_folder = item->getParentFolder();
		if (parent_folder)
		{
			if (show_all_folders)
			{
				// "Show all folders" is on, so this folder is visible
				visible = true;
			}
			else
			{
				// Move up through parent folders and see what is visible
				while (parent_folder)
				{
					visible = visible && parent_folder->isOpen() &&
							  parent_folder->potentiallyVisible();
					parent_folder = parent_folder->getParentFolder();
				}
			}
		}

		// Deselect item if any ancestor is closed or did not pass filter
		// requirements.
		if (!visible)
		{
			items_to_remove.push_back(item);
		}

		// Disallow nested selections (i.e. folder items plus one or more
		// ancestors) could check cached mum selections count and only iterate
		// if there are any but that may be a premature optimization.
		selected_items_t::iterator other_item_iter;
		for (other_item_iter = mSelectedItems.begin();
			 other_item_iter != mSelectedItems.end(); ++other_item_iter)
		{
			LLFolderViewItem* other_item = *other_item_iter;
			for (parent_folder = other_item->getParentFolder(); parent_folder;
				 parent_folder = parent_folder->getParentFolder())
			{
				if (parent_folder == item)
				{
					// This is a descendent of the current folder, remove from
					// list
					items_to_remove.push_back(other_item);
					break;
				}
			}
		}
	}

	std::vector<LLFolderViewItem*>::iterator item_it;
	for (item_it = items_to_remove.begin(); item_it != items_to_remove.end();
		 ++item_it)
	{
		// Toggle selection (also removes from list)
		changeSelection(*item_it, false);
	}

	// If nothing selected after prior constraints...
	if (mSelectedItems.empty())
	{
		// ... select first available parent of original selection, or "My
		// Inventory" otherwise
		LLFolderViewItem* new_selection = NULL;
		if (orig_selected)
		{
			for (LLFolderViewFolder* parent = orig_selected->getParentFolder();
				 parent; parent = parent->getParentFolder())
			{
				if (parent->potentiallyVisible())
				{
					// Give initial selection to first ancestor folder that
					// potentially passes the filter
					if (!new_selection)
					{
						new_selection = parent;
					}

					// If any ancestor folder of original item is closed, move
					// the selection up to the highest closed
					if (!parent->isOpen())
					{
						new_selection = parent;
					}
				}
			}
		}
		else
		{
			// Nothing selected to start with, so pick "My Inventory" as best
			// guess
			new_selection = getItemByID(gInventory.getRootFolderID());
		}

		if (new_selection)
		{
			setSelection(new_selection, false, false);
		}
	}
}

void LLFolderView::clearSelection()
{
	if (mSelectedItems.size() > 0)
	{
		recursiveDeselect(false);
		mSelectedItems.clear();
	}
}

bool LLFolderView::getSelectionList(uuid_list_t& selection)
{
	for (selected_items_t::iterator it = mSelectedItems.begin(),
									end = mSelectedItems.end();
		 it != end; ++it)
	{
		LLFolderViewItem* item = *it;
		if (item && item->getListener())
		{
			selection.emplace(item->getListener()->getUUID());
		}
	}
	return !selection.empty();
}

bool LLFolderView::getSelection(uuid_vec_t& selection)
{
	for (selected_items_t::iterator it = mSelectedItems.begin(),
									end = mSelectedItems.end();
		 it != end; ++it)
	{
		LLFolderViewItem* item = *it;
		if (item && item->getListener())
		{
			selection.emplace_back(item->getListener()->getUUID());
		}
	}
	return !selection.empty();
}

bool LLFolderView::startDrag(LLToolDragAndDrop::ESource source)
{
	bool can_drag = true;
	if (!mSelectedItems.empty())
	{
		std::vector<EDragAndDropType> types;
		uuid_vec_t cargo_ids;
		for (selected_items_t::iterator it = mSelectedItems.begin(),
										end = mSelectedItems.end();
			 it != end; ++it)
		{
			LLFolderViewItem* item = *it;
			EDragAndDropType type = DAD_NONE;
			LLUUID id;
			can_drag = can_drag && item && item->getListener() &&
					   item->getListener()->startDrag(&type, &id);
			types.push_back(type);
			cargo_ids.emplace_back(id);
		}

		gToolDragAndDrop.beginMultiDrag(types, cargo_ids, source, mSourceID);
	}
	return can_drag;
}

void LLFolderView::commitRename(LLUICtrl* renamer, void* user_data)
{
	LLFolderView* root = reinterpret_cast<LLFolderView*>(user_data);
	if (root)
	{
		root->finishRenamingItem();
	}
}

void LLFolderView::draw()
{
	// If cursor has moved off of me during drag and drop, close all auto
	// opened folders
	if (!mDragAndDropThisFrame)
	{
		closeAutoOpenedFolders();
	}
	if (this == gFocusMgr.getKeyboardFocus() && !getVisible())
	{
		gFocusMgr.setKeyboardFocus(NULL);
	}

	// While dragging, update selection rendering to reflect single/multi drag
	// status
	if (gToolDragAndDrop.hasMouseCapture())
	{
		EAcceptance last_accept = gToolDragAndDrop.getLastAccept();
		if (last_accept == ACCEPT_YES_SINGLE ||
			last_accept == ACCEPT_YES_COPY_SINGLE)
		{
			setShowSingleSelection(true);
		}
		else
		{
			setShowSingleSelection(false);
		}
		mHasCapture = true;
	}
	else
	{
		if (mHasCapture)
		{
			// Cancel any drag message tip since we just lost mouse capture
			LLFolderViewEventListener::cancelTip(true);
		}
		mHasCapture = false;

		setShowSingleSelection(false);
	}

	if (mSearchTimer.getElapsedTimeF32() > LLUI::sTypeAheadTimeout)
	{
		mSearchString.clear();
	}

	LLFolderViewFolder::draw();

	mDragAndDropThisFrame = false;
}

void LLFolderView::rememberMarketplaceFolders()
{
	// Clear old data and flags
	mMarketplaceFolders.clear();
	mWillModifyListing = mWillUnlistIfRemoved = mWillDeleteListingIfRemoved =
						 false;

	// Get the Markeplace Listings folder UUID, is any
    const LLUUID& market_id = LLMarketplace::getMPL();
	if (market_id.isNull())
	{
		// No Martketplace Listings folder: do not bother...
		return;
	}

	LLMarketplaceData* marketdata = LLMarketplaceData::getInstance();
	LLUUID id, parent_id;
	for (selected_items_t::iterator it = mSelectedItems.begin(),
									end = mSelectedItems.end();
		 it != end; ++it)
	{
		LLFolderViewItem* item = *it;
		if (!item) continue; 	// Paranoia

		if (item && item->getListener())
		{
			id = item->getListener()->getUUID();
			if (!LLMarketplace::contains(id))
			{
				continue;
			}

			bool in_marketplace = false;
			LLViewerInventoryCategory* cat = gInventory.getCategory(id);
			if (cat)
			{
				in_marketplace = true;
				mMarketplaceFolders.emplace(id);
				parent_id = cat->getParentUUID();
				if (parent_id.notNull() && LLMarketplace::contains(parent_id))
				{
					mMarketplaceFolders.emplace(parent_id);
				}
			}
			else
			{
				LLViewerInventoryItem* item = gInventory.getItem(id);
				if (item)
				{
					id = item->getParentUUID();
					if (id.notNull())
					{
						in_marketplace = true;
						mMarketplaceFolders.emplace(id);
					}
				}
			}
			if (in_marketplace)
			{
				// Check what could happen to our listings...
				if (marketdata->isInActiveFolder(id) ||
					marketdata->isListedAndActive(id))
				{
					mWillModifyListing = true;
					if (marketdata->isListed(id) ||
						marketdata->isVersionFolder(id))
					{
						mWillUnlistIfRemoved = true;
					}
				}
				else if (marketdata->isListed(id))
				{
					mWillDeleteListingIfRemoved = true;
				}
			}
		}
	}
}

void LLFolderView::updateMarketplaceFolders()
{
	for (uuid_list_t::iterator it = mMarketplaceFolders.begin(),
							   end = mMarketplaceFolders.end();
		 it != end; ++it)
	{
		const LLUUID& cat_id = *it;
		LLViewerInventoryCategory* cat = gInventory.getCategory(cat_id);
		if (cat)
		{
			LLMarketplace::updateCategory(cat_id);
			gInventory.notifyObservers();
		}
	}
	mMarketplaceFolders.clear();
}

void LLFolderView::finishRenamingItem()
{
	if (!mRenamer)
	{
		return;
	}
	if (mRenameItem)
	{
		mRenameItem->rename(mRenamer->getText());
	}

	mRenamer->setCommitOnFocusLost(false);
	mRenamer->setFocus(false);
	mRenamer->setVisible(false);
	mRenamer->setCommitOnFocusLost(true);
	gFocusMgr.setTopCtrl(NULL);

	if (mRenameItem)
	{
		setSelectionFromRoot(mRenameItem, true);
		mRenameItem = NULL;
	}

	// List is re-sorted alphabeticly, so scroll to make sure the selected item
	// is visible.
	scrollToShowSelection();

	// Update renamed market place listing folders if any
	updateMarketplaceFolders();
}

void LLFolderView::closeRenamer()
{
	// Will commit current name (which could be same as original name)
	mRenamer->setFocus(false);
	mRenamer->setVisible(false);
	gFocusMgr.setTopCtrl(NULL);

	if (mRenameItem)
	{
		setSelectionFromRoot(mRenameItem, true);
		mRenameItem = NULL;
	}
}

bool removeSelectedItemsCallback(const LLSD& notification,
								 const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) == 0) // Yes
	{
		const LLUUID& id = notification["payload"]["folderview_id"].asUUID();
		LLFolderView* view = LLFolderView::getInstance(id);
		if (view)
		{
			view->removeSelectedItems(false);
		}
	}
	return false;
}

void LLFolderView::removeSelectedItems(bool confirm)
{
	if (getVisible() && getEnabled())
	{
		// Keep track of the selected market place listing folders if any
		rememberMarketplaceFolders();
		if (confirm && (mWillUnlistIfRemoved || mWillDeleteListingIfRemoved))
		{
			LLSD payload;
			payload["folderview_id"] = mFolderViewId;
			std::string type = mWillUnlistIfRemoved ? "ConfirmMerchantUnlist"
													: "ConfirmListingCutOrDelete";
			gNotifications.add(type, LLSD(), payload,
							   removeSelectedItemsCallback);
			return;
		}

		// Just in case we are removing the renaming item.
		mRenameItem = NULL;

		// Create a temporary structure which we will use to remove items,
		// since the removal will futz with internal data structures.
		std::vector<LLFolderViewItem*> items;
		S32 count = mSelectedItems.size();
		if (count <= 0) return;

		for (selected_items_t::iterator it = mSelectedItems.begin(),
										end = mSelectedItems.end();
			 it != end; ++it)
		{
			LLFolderViewItem* item = *it;
			if (item->isRemovable())
			{
				items.push_back(item);
			}
			else
			{
				llinfos << "Cannot delete " << item->getName() << llendl;
				return;
			}
		}

		// Iterate through the new container.
		bool has_focus = gFocusMgr.childHasKeyboardFocus(this);
		count = items.size();
		LLUUID new_selection_id;
		if (count == 1)
		{
			LLFolderViewItem* item_to_delete = items[0];
			LLFolderViewFolder* parent = item_to_delete->getParentFolder();
			LLFolderViewItem* new_selection =
				item_to_delete->getNextOpenNode(false);
			if (!new_selection)
			{
				new_selection = item_to_delete->getPreviousOpenNode(false);
			}
			if (parent && parent->removeItem(item_to_delete))
			{
				// Change selection on successful delete
				if (new_selection)
				{
					setSelectionFromRoot(new_selection,
										 new_selection->isOpen(), has_focus);
				}
				else
				{
					setSelectionFromRoot(NULL, has_focus);
				}
			}
			arrangeAll();
		}
		else if (count > 1)
		{
			std::vector<LLFolderViewEventListener*> listeners;
			LLFolderViewEventListener* listener;
			LLFolderViewItem* last_item = items[count - 1];
			LLFolderViewItem* new_selection =
				last_item->getNextOpenNode(false);
			while (new_selection && new_selection->isSelected())
			{
				new_selection = new_selection->getNextOpenNode(false);
			}
			if (!new_selection)
			{
				new_selection = last_item->getPreviousOpenNode(false);
				while (new_selection && new_selection->isSelected())
				{
					new_selection = new_selection->getPreviousOpenNode(false);
				}
			}
			if (new_selection)
			{
				setSelectionFromRoot(new_selection, new_selection->isOpen(),
									 has_focus);
			}
			else
			{
				setSelectionFromRoot(NULL, has_focus);
			}

			for (S32 i = 0; i < count; ++i)
			{
				LLFolderViewItem* item = items[i];
				if (!item) continue;	// Paranoia

				listener = item->getListener();
				if (listener &&
					std::find(listeners.begin(), listeners.end(),
							  listener) == listeners.end())
				{
					listeners.push_back(listener);
				}
			}
			listener = listeners[0];
			if (listener)
			{
				listener->removeBatch(listeners);
			}
		}
		arrangeAll();
		scrollToShowSelection();

		// Update deleted market place listing folders if any
		updateMarketplaceFolders();
	}
}

void LLFolderView::openSelectedItems()
{
	if (!getVisible() || !getEnabled())
	{
		return;
	}

	if (mSelectedItems.size() == 1)
	{
		mSelectedItems.front()->openItem();
		return;
	}

	S32 left, top;
	gFloaterViewp->getNewFloaterPosition(&left, &top);
	LLMultiPreview* multi_previewp =
		new LLMultiPreview(LLRect(left, top, left + 300, top - 100));
	gFloaterViewp->getNewFloaterPosition(&left, &top);
	LLMultiProperties* multi_propertiesp =
		new LLMultiProperties(LLRect(left, top, left + 300, top - 100));
	{
		LLHostFloater host;
		for (selected_items_t::iterator it = mSelectedItems.begin(),
										end = mSelectedItems.end();
			 it != end; ++it)
		{
			LLFolderViewItem* item = *it;
			if (!item) continue;	// Paranoia

			// IT_{OBJECT,ATTACHMENT} creates LLProperties floaters; others
			// create LLPreviews. Put each one in the right type of container.
			bool is_prop = false;
			LLFolderViewEventListener* listener = item->getListener();
			if (listener)
			{
				LLInventoryType::EType type = listener->getInventoryType();
				is_prop = type == LLInventoryType::IT_OBJECT ||
						  type == LLInventoryType::IT_ATTACHMENT;
			}
			if (is_prop)
			{
				host.set(multi_propertiesp);
			}
			else
			{
				host.set(multi_previewp);
			}
			item->openItem();
		}
	}
	// NOTE: LLMulti* will safely auto-delete when opened without any children
	multi_previewp->open();
	multi_propertiesp->open();
}

void LLFolderView::propertiesSelectedItems()
{
	if (!getVisible() || !getEnabled())
	{
		return;
	}

	if (mSelectedItems.size() == 1)
	{
		LLFolderViewItem* folder_item = mSelectedItems.front();
		if (folder_item && folder_item->getListener())
		{
			folder_item->getListener()->showProperties();
		}
		return;
	}

	S32 left, top;
	gFloaterViewp->getNewFloaterPosition(&left, &top);

	LLMultiProperties* multi_propertiesp =
		new LLMultiProperties(LLRect(left, top, left + 100, top - 100));
	{
		LLHostFloater host(multi_propertiesp);

		for (selected_items_t::iterator it = mSelectedItems.begin(),
										end = mSelectedItems.end();
			 it != end; ++it)
		{
			LLFolderViewItem* item = *it;
			if (item && item->getListener())
			{
				item->getListener()->showProperties();
			}
		}
	}

	multi_propertiesp->open();
}

void LLFolderView::autoOpenItem(LLFolderViewFolder* item)
{
	if (mAutoOpenItems.check() == item ||
		mAutoOpenItems.getDepth() >= (U32)AUTO_OPEN_STACK_DEPTH)
	{
		return;
	}

	// Close auto-opened folders
	LLFolderViewFolder* close_item = mAutoOpenItems.check();
	while (close_item && close_item != item->getParentFolder())
	{
		mAutoOpenItems.pop();
		close_item->setOpenArrangeRecursively(false);
		close_item = mAutoOpenItems.check();
	}

	item->requestArrange();

	mAutoOpenItems.push(item);

	item->setOpen(true);
	scrollToShowItem(item);
}

void LLFolderView::closeAutoOpenedFolders()
{
	while (mAutoOpenItems.check())
	{
		LLFolderViewFolder* close_item = mAutoOpenItems.pop();
		close_item->setOpen(false);
	}

	if (mAutoOpenCandidate)
	{
		mAutoOpenCandidate->setAutoOpenCountdown(0.f);
	}
	mAutoOpenCandidate = NULL;
	mAutoOpenTimer.stop();
}

bool LLFolderView::autoOpenTest(LLFolderViewFolder* folder)
{
	if (folder && mAutoOpenCandidate == folder)
	{
		if (mAutoOpenTimer.getStarted())
		{
			if (!mAutoOpenCandidate->isOpen())
			{
				F32 t = clamp_rescale(mAutoOpenTimer.getElapsedTimeF32(), 0.f,
									  sAutoOpenTime, 0.f, 1.f);
				mAutoOpenCandidate->setAutoOpenCountdown(t);
			}
			if (mAutoOpenTimer.getElapsedTimeF32() > sAutoOpenTime)
			{
				autoOpenItem(folder);
				mAutoOpenTimer.stop();
				return true;
			}
		}
		return false;
	}

	// Otherwise new candidate, restart timer
	if (mAutoOpenCandidate)
	{
		mAutoOpenCandidate->setAutoOpenCountdown(0.f);
	}
	mAutoOpenCandidate = folder;
	mAutoOpenTimer.start();
	return false;
}

bool LLFolderView::canCopy() const
{
	if (!getVisible() || !getEnabled() || mSelectedItems.empty())
	{
		return false;
	}

	for (selected_items_t::const_iterator it = mSelectedItems.begin(),
										  end = mSelectedItems.end();
		 it != end; ++it)
	{
		const LLFolderViewItem* item = *it;
		if (!item || !item->getListener() ||
			!item->getListener()->isItemCopyable())
		{
			return false;
		}
	}
	return true;
}

void LLFolderView::copy()
{
	// Clear the inventory clipboard
	HBInventoryClipboard::reset();

	if (getVisible() && getEnabled() && mSelectedItems.size())
	{
		for (selected_items_t::iterator it = mSelectedItems.begin(),
										end = mSelectedItems.end();
			 it != end; ++it)
		{
			LLFolderViewItem* item = *it;
			if (!item) continue;

			LLFolderViewEventListener* listener = item->getListener();
			if (listener)
			{
				listener->copyToClipboard();
			}
		}
	}

	mSearchString.clear();
}

bool LLFolderView::canCut() const
{
	if (!getVisible() || !getEnabled() || mSelectedItems.size() == 0)
	{
		return false;
	}

	for (selected_items_t::const_iterator it = mSelectedItems.begin(),
										  end = mSelectedItems.end();
		 it != end; ++it)
	{
		LLFolderViewItem* item = *it;
		if (!item || !item->getListener() ||
			!item->getListener()->isItemMovable())
		{
			return false;
		}
	}

	return true;
}

void LLFolderView::cut()
{
	doCut(true);
}

bool cutSelectedItemsCallback(const LLSD& notification, const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) == 0) // yes
	{
		const LLUUID& id = notification["payload"]["folderview_id"].asUUID();
		LLFolderView* view = LLFolderView::getInstance(id);
		if (view)
		{
			view->doCut(false);
		}
	}
	return false;
}

void LLFolderView::doCut(bool confirm)
{
	if (confirm)
	{
		// Check for possible Marketplace Listings changes and warn if needed
		rememberMarketplaceFolders();
		std::string type;
		if (mWillUnlistIfRemoved)
		{
			type = "ConfirmMerchantUnlist";
		}
		else if (mWillDeleteListingIfRemoved)
		{
			type = "ConfirmListingCutOrDelete";
		}
		else if (mWillModifyListing)
		{
			type = "ConfirmMerchantActiveChange";
		}
		if (!type.empty())
		{
			LLSD payload;
			payload["folderview_id"] = mFolderViewId;
			gNotifications.add(type, LLSD(), payload,
							   cutSelectedItemsCallback);
			return;
		}
	}

	// Clear the inventory clipboard
	HBInventoryClipboard::reset();

	if (getVisible() && getEnabled() && mSelectedItems.size())
	{
		for (selected_items_t::iterator it = mSelectedItems.begin(),
										end = mSelectedItems.end();
			 it != end; ++it)
		{
			LLFolderViewItem* item = *it;
			if (!item) continue;

			LLFolderViewEventListener* listener = item->getListener();
			if (listener)
			{
				listener->cutToClipboard();
			}
		}
	}

	mSearchString.clear();
}

bool LLFolderView::canPaste() const
{
	if (!getVisible() || !getEnabled() || mSelectedItems.size() == 0)
	{
		return false;
	}

	for (selected_items_t::const_iterator it = mSelectedItems.begin(),
										  end = mSelectedItems.end();
		 it != end; ++it)
	{
		const LLFolderViewItem* item = *it;
		if (!item)
		{
			return false;
		}
		const LLFolderViewEventListener* listener = item->getListener();
		if (!listener || !listener->isClipboardPasteable())
		{
			const LLFolderViewFolder* folderp = item->getParentFolder();
			if (!folderp)
			{
				return false;
			}
			listener = folderp->getListener();
			if (!listener || !listener->isClipboardPasteable())
			{
				return false;
			}
		}
	}

	return true;
}

void LLFolderView::paste()
{
	doPaste(true);
}

bool pasteSelectedItemCallback(const LLSD& notification, const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) == 0) // Yes
	{
		const LLUUID& id = notification["payload"]["folderview_id"].asUUID();
		LLFolderView* view = LLFolderView::getInstance(id);
		if (view)
		{
			view->doPaste(false);
		}
	}
	return false;
}

void LLFolderView::doPaste(bool confirm)
{
	// Keep track of the selected market place listing folders if any
	rememberMarketplaceFolders();
	if (confirm && mWillModifyListing)
	{
		LLSD payload;
		payload["folderview_id"] = mFolderViewId;
		gNotifications.add("ConfirmMerchantActiveChange", LLSD(), payload,
						   pasteSelectedItemCallback);
		return;
	}

	if (getVisible() && getEnabled())
	{
		// Find a set of unique folders to paste into
		fast_hset<LLFolderViewItem*> folder_set;

		for (selected_items_t::iterator it = mSelectedItems.begin(),
										end = mSelectedItems.end();
			 it != end; ++it)
		{
			LLFolderViewItem* item = *it;
			if (!item) continue; // Paranoia

			LLFolderViewEventListener* listener = item->getListener();
			if (listener->getInventoryType() != LLInventoryType::IT_CATEGORY)
			{
				item = item->getParentFolder();
			}
			folder_set.insert(item);
		}

		for (fast_hset<LLFolderViewItem*>::iterator it = folder_set.begin(),
													end = folder_set.end();
			 it != end; ++it)
		{
			LLFolderViewItem* item = *it;
			if (item) // Paranoia
			{
				LLFolderViewEventListener* listener = item->getListener();
				if (listener && listener->isClipboardPasteable())
				{
					listener->pasteFromClipboard();
				}
			}
		}
	}

	mSearchString.clear();

	// Update deleted market place listing folders if any
	updateMarketplaceFolders();
}

bool startRenamingSelectedItemCallback(const LLSD& notification,
									   const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) == 0) // Yes
	{
		const LLUUID& id = notification["payload"]["folderview_id"].asUUID();
		LLFolderView* view = LLFolderView::getInstance(id);
		if (view)
		{
			view->startRenamingSelectedItem(false);
		}
	}
	return false;
}

// Public rename functionality: can only start the process
void LLFolderView::startRenamingSelectedItem(bool confirm)
{
	// Keep track of the selected market place listing folders if any
	rememberMarketplaceFolders();
	if (confirm && mWillModifyListing)
	{
		LLSD payload;
		payload["folderview_id"] = mFolderViewId;
		gNotifications.add("ConfirmMerchantActiveChange", LLSD(), payload,
						   startRenamingSelectedItemCallback);
		return;
	}

	// Make sure selection is visible
	scrollToShowSelection();

	S32 count = mSelectedItems.size();
	LLFolderViewItem* item = NULL;
	if (count > 0)
	{
		item = mSelectedItems.front();
	}
	if (getVisible() && getEnabled() && count == 1 && item &&
		item->getListener() && item->getListener()->isItemRenameable())
	{
		mRenameItem = item;

		S32 x = ARROW_SIZE + TEXT_PAD + ICON_WIDTH + ICON_PAD - 1 +
				item->getIndentation();
		S32 y = llfloor(item->getRect().getHeight() - sFontLineHeight - 2);
		item->localPointToScreen(x, y, &x, &y);
		screenPointToLocal(x, y, &x, &y);
		mRenamer->setOrigin(x, y);

		S32 scroller_height = 0;
		S32 scroller_width = gViewerWindowp->getWindowWidth();
		bool dummy_bool;
		if (mScrollContainer)
		{
			mScrollContainer->calcVisibleSize(&scroller_width,
											  &scroller_height,
											  &dummy_bool, &dummy_bool);
		}

		S32 width = llmax(llmin(item->getRect().getWidth() - x,
								scroller_width - x - getRect().mLeft),
						  MINIMUM_RENAMER_WIDTH);
		S32 height = llfloor(sFontLineHeight + RENAME_HEIGHT_PAD);
		mRenamer->reshape(width, height);

		mRenamer->setText(item->getName());
		mRenamer->selectAll();
		mRenamer->setVisible(true);
		// Set focus will fail unless item is visible
		mRenamer->setFocus(true);
		mRenamer->setLostTopCallback(onRenamerLost);
		gFocusMgr.setTopCtrl(mRenamer);
	}
}

void LLFolderView::setFocus(bool focus)
{
	if (focus && !hasFocus())
	{
		grabMenuHandler();
	}
	LLFolderViewFolder::setFocus(focus);
}

bool LLFolderView::handleKeyHere(KEY key, MASK mask)
{
	bool handled = false;

	// SL-51858: key presses are not being passed to the pop-up menu. A proper
	// fix is non-trivial, so instead just close the menu.
	LLMenuGL* menup = (LLMenuGL*)mPopupMenuHandle.get();
	if (menup && menup->isOpen())
	{
		gMenuHolderp->hideMenus();
	}

	switch (key)
	{
		case KEY_F2:
			mSearchString.clear();
			startRenamingSelectedItem();
			handled = true;
			break;

		case KEY_RETURN:
			if (mask == MASK_NONE)
			{
				if (mRenameItem && mRenamer->getVisible())
				{
					finishRenamingItem();
					mSearchString.clear();
					handled = true;
				}
				else
				{
					LLFolderView::openSelectedItems();
					handled = true;
				}
			}
			break;

		case KEY_ESCAPE:
			if (mask == MASK_NONE)
			{
				if (mRenameItem && mRenamer->getVisible())
				{
					closeRenamer();
					handled = true;
				}
				mSearchString.clear();
			}
			break;

		case KEY_PAGE_UP:
			mSearchString.clear();
			mScrollContainer->pageUp(30);
			handled = true;
			break;

		case KEY_PAGE_DOWN:
			mSearchString.clear();
			mScrollContainer->pageDown(30);
			handled = true;
			break;

		case KEY_HOME:
			mSearchString.clear();
			mScrollContainer->goToTop();
			handled = true;
			break;

		case KEY_END:
			mSearchString.clear();
			mScrollContainer->goToBottom();
			break;

		case KEY_DOWN:
			if (mScrollContainer && mSelectedItems.size() > 0)
			{
				LLFolderViewItem* last_selected = getCurSelectedItem();

				if (!mKeyboardSelection)
				{
					setSelection(last_selected, false, true);
					mKeyboardSelection = true;
				}

				LLFolderViewItem* next = NULL;
				if (mask & MASK_SHIFT)
				{
					// Do not shift select down to children of folders (they
					// are implicitly selected through parent)
					next = last_selected->getNextOpenNode(false);
					if (next)
					{
						if (next->isSelected())
						{
							// Shrink selection
							changeSelectionFromRoot(last_selected, false);
						}
						else if (last_selected->getParentFolder() ==
									next->getParentFolder())
						{
							// Grow selection
							changeSelectionFromRoot(next, true);
						}
					}
				}
				else
				{
					next = last_selected->getNextOpenNode();
					if (next)
					{
						if (next == last_selected)
						{
							return false;
						}
						setSelection(next, false, true);
					}
				}
				scrollToShowSelection();
				mSearchString.clear();
				handled = true;
			}
			break;

		case KEY_UP:
			if ((mSelectedItems.size() > 0) && mScrollContainer)
			{
				LLFolderViewItem* last_selected = mSelectedItems.back();

				if (!mKeyboardSelection)
				{
					setSelection(last_selected, false, true);
					mKeyboardSelection = true;
				}

				LLFolderViewItem* prev = NULL;
				if (mask & MASK_SHIFT)
				{
					// Do not shift select down to children of folders (they
					// are implicitly selected through parent)
					prev = last_selected->getPreviousOpenNode(false);
					if (prev)
					{
						if (prev->isSelected())
						{
							// Shrink selection
							changeSelectionFromRoot(last_selected, false);
						}
						else if (last_selected->getParentFolder() ==
									prev->getParentFolder())
						{
							// Grow selection
							changeSelectionFromRoot(prev, true);
						}
					}
				}
				else
				{
					prev = last_selected->getPreviousOpenNode();
					if (prev)
					{
						if (prev == this)
						{
							return false;
						}
						setSelection(prev, false, true);
					}
				}
				scrollToShowSelection();
				mSearchString.clear();

				handled = true;
			}
			break;

		case KEY_RIGHT:
			if (mSelectedItems.size())
			{
				LLFolderViewItem* last_selected = getCurSelectedItem();
				last_selected->setOpen(true);
				mSearchString.clear();
				handled = true;
			}
			break;

		case KEY_LEFT:
			if (mSelectedItems.size())
			{
				LLFolderViewItem* last_selected = getCurSelectedItem();
				LLFolderViewItem* parent_folder =
					last_selected->getParentFolder();
				if (!last_selected->isOpen() &&
					parent_folder && parent_folder->getParentFolder())
				{
					setSelection(parent_folder, false, true);
				}
				else
				{
					last_selected->setOpen(false);
				}
				mSearchString.clear();
				scrollToShowSelection();
				handled = true;
			}
	}

	if (!handled && hasFocus() && key == KEY_BACKSPACE)
	{
		mSearchTimer.reset();
		if (mSearchString.size())
		{
			mSearchString.erase(mSearchString.size() - 1, 1);
		}
		search(getCurSelectedItem(), mSearchString);
		handled = true;
	}

	return handled;
}

bool LLFolderView::handleUnicodeCharHere(llwchar uni_char)
{
	if (uni_char < 0x20 || uni_char == 0x7F) // Control character or DEL
	{
		return false;
	}

	if (uni_char > 0x7f)
	{
		llwarns << "Cannot handle non-ASCII yet, aborting" << llendl;
		return false;
	}

	bool handled = false;
	if (gFocusMgr.childHasKeyboardFocus(getRoot()))
	{
		// SL-51858: key presses are not being passed to the pop-up menu. A
		// proper fix is non-trivial so instead just close the menu.
		LLMenuGL* menup = (LLMenuGL*)mPopupMenuHandle.get();
		if (menup && menup->isOpen())
		{
			gMenuHolderp->hideMenus();
		}

		// Do text search
		if (mSearchTimer.getElapsedTimeF32() > LLUI::sTypeAheadTimeout)
		{
			mSearchString.clear();
		}
		mSearchTimer.reset();
		if (mSearchString.size() < 128)
		{
			mSearchString += uni_char;
		}
		search(getCurSelectedItem(), mSearchString);

		handled = true;
	}

	return handled;
}

bool LLFolderView::canDoDelete() const
{
	if (mSelectedItems.empty())
	{
		return false;
	}

	for (selected_items_t::const_iterator it = mSelectedItems.begin(),
										  end = mSelectedItems.end();
		 it != end; ++it)
	{
		LLFolderViewItem* item = *it;
		if (!item || !item->getListener() ||
			!item->getListener()->isItemRemovable())
		{
			return false;
		}
	}

	return true;
}

void LLFolderView::doDelete()
{
	if (mSelectedItems.size() > 0)
	{
		removeSelectedItems();
	}
}

bool LLFolderView::handleMouseDown(S32 x, S32 y, MASK mask)
{
	mKeyboardSelection = false;
	mSearchString.clear();

	setFocus(true);

	return LLView::handleMouseDown(x, y, mask);
}

void LLFolderView::onFocusLost()
{
	releaseMenuHandler();
	LLUICtrl::onFocusLost();
}

bool LLFolderView::search(LLFolderViewItem* first_item,
						  const std::string& search_string, bool backward)
{
	// Get first selected item
	LLFolderViewItem* search_item = first_item;

	// Make sure search string is upper case
	std::string upper_case_string = search_string;
	LLStringUtil::toUpper(upper_case_string);

	// If nothing selected, select first item in folder
	if (!search_item)
	{
		// Start from first item
		search_item = getNextFromChild(NULL);
	}

	// Search over all open nodes for first substring match (with wrapping)
	bool found = false;
	LLFolderViewItem* original_search_item = search_item;
	do
	{
		// Wrap at end
		if (!search_item)
		{
			if (backward)
			{
				search_item = getPreviousFromChild(NULL);
			}
			else
			{
				search_item = getNextFromChild(NULL);
			}
			if (!search_item || search_item == original_search_item)
			{
				break;
			}
		}

		std::string current_item_label(search_item->getSearchableData());
		S32 search_string_length = llmin(upper_case_string.size(),
										 current_item_label.size());
		if (!current_item_label.compare(0, search_string_length,
										upper_case_string))
		{
			found = true;
			break;
		}
		if (backward)
		{
			search_item = search_item->getPreviousOpenNode();
		}
		else
		{
			search_item = search_item->getNextOpenNode();
		}

	}
	while (search_item != original_search_item);

	if (found)
	{
		setSelection(search_item, false, true);
		scrollToShowSelection();
	}

	return found;
}

bool LLFolderView::handleDoubleClick(S32 x, S32 y, MASK mask)
{
	// Skip LLFolderViewFolder::handleDoubleClick()
	return LLView::handleDoubleClick(x, y, mask);
}

bool LLFolderView::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
	// If the context menu has not yet been created for this item, this call
	// will create it now. HB
	LLMenuGL* menup = getContextMenu();

	// All user operations move keyboard focus to inventory this way, we know
	// when to stop auto-updating a search
	setFocus(true);

	bool handled = childrenHandleRightMouseDown(x, y, mask) != NULL;
	S32 count = mSelectedItems.size();
	if (handled && count > 0 && menup)
	{
		const LLView::child_list_t* list = menup->getChildList();
		for (LLView::child_list_t::const_iterator it = list->begin(),
												  end = list->end();
			 it != end; ++it)
		{
			(*it)->setVisible(true);
			(*it)->setEnabled(true);
		}

		// Successively filter out invalid options
		U32 flags = FIRST_SELECTED_ITEM;
		U32 multi_select_flag = 0x0;
		if (mSelectedItems.size() > 1)
		{
			 multi_select_flag = ITEM_IN_MULTI_SELECTION;
		}
		for (selected_items_t::iterator it = mSelectedItems.begin(),
										end = mSelectedItems.end();
			 it != end; ++it)
		{
			(*it)->buildContextMenu(*menup, flags);
			flags = multi_select_flag;
		}

		menup->arrange();
		menup->updateParent(gMenuHolderp);
		LLMenuGL::showPopup(this, menup, x, y);
	}
	else
	{
		if (menup && menup->getVisible())
		{
			menup->setVisible(false);
		}
		setSelection(NULL, false, true);
	}
	return handled;
}

bool LLFolderView::handleHover(S32 x, S32 y, MASK mask)
{
	return LLView::handleHover(x, y, mask);
}

bool LLFolderView::handleDragAndDrop(S32 x, S32 y, MASK mask, bool drop,
									 EDragAndDropType cargo_type,
									 void* cargo_data, EAcceptance* accept,
									 std::string& tooltip_msg)
{
	mDragAndDropThisFrame = true;
	bool handled = LLView::handleDragAndDrop(x, y, mask, drop, cargo_type,
											 cargo_data, accept, tooltip_msg);
	if (handled)
	{
		LL_DEBUGS("UserInput") << "dragAndDrop handled with: drop = "
							   << (drop ? "true" : "false") << " - accepted = "
							   <<  (*accept != ACCEPT_NO ? "true" : "false")
							   << LL_ENDL;
	}

	return handled;
}

bool LLFolderView::handleScrollWheel(S32 x, S32 y, S32 clicks)
{
	return mScrollContainer &&
		   mScrollContainer->handleScrollWheel(x, y, clicks);
}

void LLFolderView::deleteAllChildren()
{
	if (mRenamer == gFocusMgr.getTopCtrl())
	{
		gFocusMgr.setTopCtrl(NULL);
	}
	if (deleteViewByHandle(mPopupMenuHandle))
	{
		mPopupMenuHandle = LLHandle<LLView>();
	}
	mRenamer = NULL;
	mRenameItem = NULL;
	clearSelection();
	LLView::deleteAllChildren();
}

void LLFolderView::scrollToShowSelection()
{
	if (mSelectedItems.size())
	{
		mNeedsScroll = true;
	}
}

// If the parent is scroll containter, scrolls it to make the selection is
// maximally visible.
void LLFolderView::scrollToShowItem(LLFolderViewItem* item)
{
	// Do not scroll to items when mouse is being used to scroll/drag and drop
	if (gFocusMgr.childHasMouseCapture(mScrollContainer))
	{
		mNeedsScroll = false;
		return;
	}
	if (item && mScrollContainer)
	{
		LLRect local_rect = item->getRect();
		// Item position relative to display area of scroller:
		LLRect item_scrolled_rect;

		S32 icon_height = mIcon.isNull() ? 0 : mIcon->getHeight();
		// When navigating with keyboard, only move top of folders on screen,
		// otherwise show whole folder
		S32 max_height_to_show =
			gFocusMgr.childHasKeyboardFocus(this) ?
				llmax(icon_height, sFontLineHeightRounded) + ICON_PAD :
				local_rect.getHeight();
		item->localPointToOtherView(item->getIndentation(),
									llmax(0,
										  local_rect.getHeight() -
										  max_height_to_show),
									&item_scrolled_rect.mLeft,
									&item_scrolled_rect.mBottom,
									mScrollContainer);
		item->localPointToOtherView(local_rect.getWidth(),
									local_rect.getHeight(),
									&item_scrolled_rect.mRight,
									&item_scrolled_rect.mTop,
									mScrollContainer);

		item_scrolled_rect.mRight =
			llmin(item_scrolled_rect.mLeft + MIN_ITEM_WIDTH_VISIBLE,
				  item_scrolled_rect.mRight);
		LLCoordGL scroll_offset(-mScrollContainer->getBorderWidth() -
								item_scrolled_rect.mLeft,
								mScrollContainer->getRect().getHeight() -
								item_scrolled_rect.mTop - 1);

		S32 max_scroll_offset = getVisibleRect().getHeight() -
								item_scrolled_rect.getHeight();
			// If we are scrolling to focus on a new item
		if (item != mLastScrollItem ||
			// Or the item has just appeared on screen and it was not on screen
			// before
			(scroll_offset.mY > 0 && scroll_offset.mY < max_scroll_offset &&
			 (mLastScrollOffset.mY < 0 ||
			  mLastScrollOffset.mY > max_scroll_offset)))
		{
			// We now have a position on screen that we want to keep stable
			// offset of selection relative to top of visible area
			mLastScrollOffset = scroll_offset;
			mLastScrollItem = item;
		}

		mScrollContainer->scrollToShowRect(item_scrolled_rect,
										   mLastScrollOffset);

		// After scrolling, store new offset; in case we do not have room to
		// maintain the original position.
		LLCoordGL new_item_left_top;
		item->localPointToOtherView(item->getIndentation(),
									item->getRect().getHeight(),
									&new_item_left_top.mX,
									&new_item_left_top.mY,
									mScrollContainer);
		mLastScrollOffset.set(-mScrollContainer->getBorderWidth() -
							  new_item_left_top.mX,
							  mScrollContainer->getRect().getHeight() -
							  new_item_left_top.mY - 1);
	}
}

LLRect LLFolderView::getVisibleRect()
{
	S32 visible_height = mScrollContainer->getRect().getHeight();
	S32 visible_width = mScrollContainer->getRect().getWidth();
	LLRect visible_rect;
	visible_rect.setLeftTopAndSize(-getRect().mLeft,
								   visible_height - getRect().mBottom,
								   visible_width, visible_height);
	return visible_rect;
}

bool LLFolderView::getShowSelectionContext()
{
	if (mShowSelectionContext)
	{
		return true;
	}
	LLMenuGL* menup = (LLMenuGL*)mPopupMenuHandle.get();
	return menup && menup->getVisible();
}

void LLFolderView::setShowSingleSelection(bool show)
{
	if (show != mShowSingleSelection)
	{
		mMultiSelectionFadeTimer.reset();
		mShowSingleSelection = show;
	}
}

void LLFolderView::addItemID(const LLUUID& id, LLFolderViewItem* itemp)
{
	mItemMap[id] = itemp;
}

void LLFolderView::removeItemID(const LLUUID& id)
{
	mItemMap.erase(id);
}

LLFolderViewItem* LLFolderView::getItemByID(const LLUUID& id)
{
	if (id.isNull())
	{
		return this;
	}

	item_map_t::iterator map_it = mItemMap.find(id);
	if (map_it != mItemMap.end())
	{
		return map_it->second;
	}

	return NULL;
}

// Main idle routine
void LLFolderView::doIdle()
{
	LL_FAST_TIMER(FTM_INVENTORY);

	mFilter.clearModified();
	bool filter_modified_and_active =
		mFilter.isNotDefault() &&
		mCompletedFilterGeneration < mFilter.getCurrentGeneration();
	mNeedsAutoSelect = mCanAutoSelect && filter_modified_and_active &&
					   !gFocusMgr.childHasKeyboardFocus(this) &&
					   !gFocusMgr.getMouseCapture();

	// Filter to determine visiblity before arranging
	filterFromRoot();

	// Automatically show matching items, and select first one. Do this every
	// frame until user puts keyboard focus into the inventory window signaling
	// the end of the automatic update.
	// Only do this when mNeedsFilter is set, meaning filtered items have
	// potentially changed
	if (mNeedsAutoSelect)
	{
		LL_FAST_TIMER(FTM_AUTO_SELECT);
		// Select new item only if a filtered item not currently selected
		LLFolderViewItem* selected_itemp =
			mSelectedItems.empty() ? NULL : mSelectedItems.back();
		if (selected_itemp && !sFolderViewItems.count(selected_itemp))
		{
			// There is a crash bug due to a race condition: when a folder view
			// item is destroyed, its address may still appear in
			// mSelectedItems a couple of doIdle()s later, even if you
			// explicitely clear this list and dirty the filters in the
			// destructor... This code avoids the crash bug. HB
			llwarns << "Invalid folder view item (" << selected_itemp
					<< ") in selection: clearing the latter." << llendl;
			dirtyFilter();
			clearSelection();
			requestArrange();
		}
		else if (!selected_itemp || !selected_itemp->getFiltered())
		{
			// Select first filtered item
			LLSelectFirstFilteredItem filter;
			applyFunctorRecursively(filter);
		}
		scrollToShowSelection();
	}

	bool is_visible = isInVisibleChain();
	if (is_visible)
	{
		sanitizeSelection();
		if (needsArrange())
		{
			arrangeFromRoot();
		}
	}

	if (mSelectedItems.size() && mNeedsScroll)
	{
		scrollToShowItem(mSelectedItems.back());
		// Continue scrolling until animated layout change is done
		if ((!needsArrange() || !is_visible) &&
			getCompletedFilterGeneration() >=
				mFilter.getMinRequiredGeneration())
		{
			mNeedsScroll = false;
		}
	}

	if (!mSignalSelectCallback)
	{
		mGotLeftMouseClick = false;
		return;
	}

	static LLCachedControl<bool> thumbnails(gSavedSettings,
											"AutoShowInventoryThumbnails");
	if (thumbnails && mGotLeftMouseClick && mShowThumbnails &&
		mSelectedItems.size() == 1 && AISAPI::isAvailable())
	{
		LLFolderViewItem* itemp = mSelectedItems.back();
		if (itemp && itemp->getListener())
		{
			HBFloaterThumbnail::showInstance(itemp->getListener()->getUUID(),
											 mSourceID, true);
		}
	}
	mGotLeftMouseClick = false;

	if (mSelectCallback)
	{
		// RN: we use keyboard focus as a proxy for user-explicit actions
		bool take_focus = mSignalSelectCallback == SIGNAL_KEYBOARD_FOCUS;
		mSelectCallback(this, take_focus, mUserData);
	}

	mSignalSelectCallback = 0;
}

//static
void LLFolderView::idle(void* user_data)
{
	LLFolderView* self = (LLFolderView*)user_data;
	if (self)
	{
		// Do the real idle
		self->doIdle();
	}
}

bool LLInventorySort::updateSort(U32 order)
{
	if (order == mSortOrder)
	{
		return false;
	}
	mSortOrder = order;
	mByDate = order & LLInventoryFilter::SO_DATE;
	mSystemToTop = order & LLInventoryFilter::SO_SYSTEM_FOLDERS_TO_TOP;
	mFoldersByName = order & LLInventoryFilter::SO_FOLDERS_BY_NAME;
	return true;
}

bool LLInventorySort::operator()(const LLFolderViewItem* const& a,
								 const LLFolderViewItem* const& b)
{
	// We sort by name if we are not sorting by date OR if these are folders
	// and we are sorting folders by name.
	bool by_name = !mByDate ||
				   (mFoldersByName && a->getSortGroup() != SG_ITEM);

	if (a->getSortGroup() != b->getSortGroup())
	{
		if (mSystemToTop)
		{
			// Group order is System Folders, Trash, Normal Folders, Items
			return a->getSortGroup() < b->getSortGroup();
		}
		else if (mByDate)
		{
			// Trash needs to go to the bottom if we are sorting by date
			if (a->getSortGroup() == SG_TRASH_FOLDER ||
				b->getSortGroup() == SG_TRASH_FOLDER)
			{
				return b->getSortGroup() == SG_TRASH_FOLDER;
			}
		}
	}

	if (by_name)
	{
		S32 compare = LLStringUtil::compareDict(a->getLabel(), b->getLabel());
		return compare < 0 ||
			   (compare == 0 && a->getCreationDate() > b->getCreationDate());
	}

	// This is very very slow. The getCreationDate() is log(n) in number of
	// inventory items.
	time_t first_create = a->getCreationDate();
	time_t second_create = b->getCreationDate();
	if (first_create == second_create)
	{
		return LLStringUtil::compareDict(a->getLabel(), b->getLabel()) < 0;
	}
	return first_create > second_create;
}

//static
void LLFolderView::onRenamerLost(LLUICtrl* renamer, void* user_data)
{
	renamer->setVisible(false);
}

//-----------------------------------------------------------------------------
// LLFolderViewEventListener class
//-----------------------------------------------------------------------------

void LLFolderViewEventListener::arrangeAndSet(LLFolderViewItem* focusp,
											  bool set_selection,
											  bool take_keyboard_focus)
{
	if (!focusp) return;

	LLFolderView* rootp = focusp->getRoot();
	if (focusp->getParentFolder())
	{
		focusp->getParentFolder()->requestArrange();
	}
	if (set_selection)
	{
		focusp->setSelectionFromRoot(focusp, true, take_keyboard_focus);
		if (rootp)
		{
			rootp->scrollToShowSelection();
		}
	}
}

//static
void LLFolderViewEventListener::cancelTip(bool not_drop_msg)
{
	if (sLastDragTipID.notNull() && !(not_drop_msg && sLastDragTipDrop))
	{
		LLNotificationPtr n = gNotifications.find(sLastDragTipID);
		if (n)
		{
			gNotifications.cancel(n);
		}
		sLastDragTipID.setNull();
		sLastDragTipMsg.clear();
		sLastDragTipDrop = false;
	}
}

//static
void LLFolderViewEventListener::dragOrDropTip(bool drop,
											  const std::string& tooltip_msg)
{
	if (tooltip_msg.empty())
	{
		if (drop)
		{
			cancelTip();	// Drag and drop action ended without warning
		}
		return;
	}

	if (!sLastDragTipMsg.empty())
	{
		// If the last notification has expired, clear its data so to display
		// it again now if needed.
		LLNotificationPtr n = gNotifications.find(sLastDragTipID);
		if (!n || n->isExpired() || n->isCancelled())
		{
			sLastDragTipMsg.clear();
			sLastDragTipID.setNull();
			sLastDragTipDrop = false;
		}
	}
	if (sLastDragTipMsg != tooltip_msg)
	{
		cancelTip();
		LLSD subs;
		subs["MESSAGE"] = tooltip_msg;
		LLNotificationPtr n = gNotifications.add("LongGenericMessageTip",
												 subs);
		if (n)
		{
			sLastDragTipID = n->getID();
			sLastDragTipMsg = tooltip_msg;
			sLastDragTipDrop = drop;
		}
	}
}

//-----------------------------------------------------------------------------
// LLInventoryFilter class
//-----------------------------------------------------------------------------

LLInventoryFilter::LLInventoryFilter(const std::string& name)
:	mName(name),
	mModified(false),
	mNeedTextRebuild(true),
	mOrder(SO_FOLDERS_BY_NAME),	// This gets overridden by a pref immediately
	mFilterGeneration(0),
	mNextFilterGeneration(1),
	mMinRequiredGeneration(0),
	mMustPassGeneration(S32_MAX),
	mFilterBehavior(FILTER_NONE),
	mFilterCount(0),
	mFilterSubType(-1),
	mHideLibrary(false),
	mFilterWorn(false),
	mFilterLastOpen(false),
	mFilterShowLinks(false),
	mSubStringMatchOffset(0)
{
	mFilterOps.mFilterTypes = 0xffffffff;
	mFilterOps.mMinDate = time_min();
	mFilterOps.mMaxDate = time_max();
	mFilterOps.mHoursAgo = 0;
	mFilterOps.mShowFolderState = SHOW_NON_EMPTY_FOLDERS;
	mFilterOps.mPermissions = PERM_NONE;

	mLastLogoff = gSavedPerAccountSettings.getU32("LastLogoff");

	// Copy mFilterOps into mDefaultFilterOps
	markDefault();
}

bool LLInventoryFilter::check(LLFolderViewItem* item)
{
	if (!item) return false;

	LLFolderViewEventListener* listener = item->getListener();
	if (!listener) return false;

	const LLUUID& item_id = listener->getUUID();

	// When filtering is active and we do not explicitly search for a link,
	// omit links.
	if (!mFilterShowLinks)
	{
		const LLInventoryObject* obj = gInventory.getObject(item_id);
		if (isActive() && obj && obj->getIsLinkType() &&
			mFilterSubString.find("(LINK)") == std::string::npos)
		{
			return false;
		}
	}

	static LLCachedControl<bool> hide_cof(gSavedSettings,
										  "HideCurrentOutfitFolder");
	if (hide_cof && !LLFolderType::getCanDeleteCOF() &&
		gInventory.isInCOF(item_id))
	{
		return false;
	}

	static LLCachedControl<bool> hide_mp(gSavedSettings,
										 "HideMarketplaceFolder");
	if (hide_mp && gInventory.isInMarketPlace(item_id))
	{
		return false;
	}

	if (mHideLibrary &&
		gInventory.isObjectDescendentOf(item_id,
										gInventory.getLibraryRootFolderID()))
	{
		return false;
	}

	LLInventoryType::EType object_type = listener->getInventoryType();
	if (object_type != LLInventoryType::IT_NONE &&
		!(0x1 << object_type & mFilterOps.mFilterTypes))
	{
		return false;
	}

	if (mFilterSubString.empty())
	{
		mSubStringMatchOffset = std::string::npos;
	}
	else
	{
		std::string search_string = mFilterSubString;
		if (search_string != "(LINK)" && search_string.find("(LINK)"))
		{
			LLStringUtil::replaceString(search_string, "(LINK)", "");
		}
		mSubStringMatchOffset = item->getSearchableData().find(search_string);
		if (mSubStringMatchOffset == std::string::npos)
		{
			return false;
		}
	}

	if (mFilterSubType != -1 && listener->getSubType() != mFilterSubType)
	{
		return false;
	}

	if (mFilterWorn && !gAgentWearables.isWearingItem(item_id) &&
		!(isAgentAvatarValid() && gAgentAvatarp->isWearingAttachment(item_id)))
	{
		return false;
	}

	if (mFilterLastOpen && mLastOpenID.notNull() &&
		!gInventory.isObjectDescendentOf(item_id, mLastOpenID))
	{
		return false;
	}

	if ((listener->getPermissionMask() & mFilterOps.mPermissions) !=
			mFilterOps.mPermissions)
	{
		return false;
	}

	time_t earliest = time_corrected() - mFilterOps.mHoursAgo * 3600;
	if (mFilterOps.mMinDate > time_min() && mFilterOps.mMinDate < earliest)
	{
		earliest = mFilterOps.mMinDate;
	}
	else if (!mFilterOps.mHoursAgo)
	{
		earliest = 0;
	}
	if (listener->getCreationDate() < earliest ||
		listener->getCreationDate() > mFilterOps.mMaxDate)
	{
		return false;
	}

	static LLCachedControl<bool> hide_empty_folders(gSavedSettings,
													"HideEmptySystemFolders");
	if (object_type == LLInventoryType::IT_CATEGORY && hide_empty_folders)
	{
		if (LLViewerFolderType::lookupIsHiddenIfEmpty(listener->getPreferredType()))
		{
			// Force the fetching of those folders so they are hidden if they
			// really are empty...
			gInventory.fetchDescendentsOf(item_id);
			return false;
		}
	}

	return true;
}

// Has user modified default filter params ?
bool LLInventoryFilter::isNotDefault()
{
	return mFilterOps.mFilterTypes != mDefaultFilterOps.mFilterTypes ||
		   mFilterSubType != -1 || mFilterWorn || mFilterLastOpen ||
		   mFilterSubString.size() || mHideLibrary ||
		   mFilterOps.mPermissions != mDefaultFilterOps.mPermissions ||
		   mFilterOps.mMinDate != mDefaultFilterOps.mMinDate ||
		   mFilterOps.mMaxDate != mDefaultFilterOps.mMaxDate ||
		   mFilterOps.mHoursAgo != mDefaultFilterOps.mHoursAgo;
}

bool LLInventoryFilter::isActive()
{
	return mFilterOps.mFilterTypes != 0xffffffff || mFilterWorn ||
		   mFilterLastOpen || mFilterSubString.size() ||
		   mFilterOps.mPermissions != PERM_NONE ||
		   mFilterOps.mMinDate != time_min() ||
		   mFilterOps.mMaxDate != time_max() ||
		   mFilterOps.mHoursAgo != 0;
}

void LLInventoryFilter::setFilterTypes(U32 types)
{
	if (mFilterOps.mFilterTypes != types)
	{
		// Keep current items only if no type bits getting turned off
		bool fewer_bits_set = (mFilterOps.mFilterTypes & ~types) != 0;
		bool more_bits_set = (~mFilterOps.mFilterTypes & types) != 0;

		mFilterOps.mFilterTypes = types;
		if (more_bits_set && fewer_bits_set)
		{
			// Neither less or more restrive, both simultaneously, so we need
			// to filter from scratch
			setModified(FILTER_RESTART);
		}
		else if (more_bits_set)
		{
			// Target is only one of all requested types so more type
			// bits == less restrictive
			setModified(FILTER_LESS_RESTRICTIVE);
		}
		else if (fewer_bits_set)
		{
			setModified(FILTER_MORE_RESTRICTIVE);
		}
	}
}

void LLInventoryFilter::setFilterSubString(const std::string& str)
{
	if (mFilterSubString != str)
	{
		std::string upper = str;
		LLStringUtil::toUpper(upper);

		// Check whether the new search string contains "(LINK)" and not the
		// old one, or vice-versa...
		bool had_link = mFilterSubString.find("(LINK)") != std::string::npos;
		bool has_link = upper.find("(LINK)") != std::string::npos;

		size_t old_size = mFilterSubString.size();
		size_t new_size = upper.size();
		// Hitting BACKSPACE, for example
		bool looser = had_link == has_link && old_size >= new_size &&
					  !mFilterSubString.substr(0, new_size).compare(upper);
		// Appending new characters
		bool stricter = had_link == has_link && old_size < new_size &&
						!upper.substr(0, old_size).compare(mFilterSubString);

		mFilterSubString = upper;
		LLStringUtil::trimHead(mFilterSubString);

		if (looser)
		{
			setModified(FILTER_LESS_RESTRICTIVE);
		}
		else if (stricter)
		{
			setModified(FILTER_MORE_RESTRICTIVE);
		}
		else
		{
			setModified(FILTER_RESTART);
		}
	}
}

void LLInventoryFilter::setFilterPermissions(PermissionMask perms)
{
	if (mFilterOps.mPermissions != perms)
	{
		// Keep current items only if no perm bits getting turned off
		bool fewer_bits_set = (mFilterOps.mPermissions & ~perms) != 0;
		bool more_bits_set = (~mFilterOps.mPermissions & perms) != 0;
		mFilterOps.mPermissions = perms;

		if (more_bits_set && fewer_bits_set)
		{
			setModified(FILTER_RESTART);
		}
		else if (more_bits_set)
		{
			// Target must have all requested permission bits, so more bits
			// means more restrictive
			setModified(FILTER_MORE_RESTRICTIVE);
		}
		else if (fewer_bits_set)
		{
			setModified(FILTER_LESS_RESTRICTIVE);
		}
	}
}

void LLInventoryFilter::setDateRange(time_t min_date, time_t max_date)
{
	mFilterOps.mHoursAgo = 0;
	if (mFilterOps.mMinDate != min_date)
	{
		mFilterOps.mMinDate = min_date;
		setModified();
	}
	if (mFilterOps.mMaxDate != llmax(mFilterOps.mMinDate, max_date))
	{
		mFilterOps.mMaxDate = llmax(mFilterOps.mMinDate, max_date);
		setModified();
	}
}

void LLInventoryFilter::setDateRangeLastLogoff(bool sl)
{
	if (sl && !isSinceLogoff())
	{
		setDateRange(mLastLogoff, time_max());
		setModified();
	}
	if (!sl && isSinceLogoff())
	{
		setDateRange(0, time_max());
		setModified();
	}
}

bool LLInventoryFilter::isSinceLogoff()
{
	return mFilterOps.mMinDate == (time_t)mLastLogoff &&
		   mFilterOps.mMaxDate == (time_t)time_max();
}

void LLInventoryFilter::setHoursAgo(U32 hours)
{
	if (mFilterOps.mHoursAgo != hours)
	{
		// *NOTE: need to cache last filter time, in case filter goes stale
		bool looser = mFilterOps.mMinDate == time_min() &&
					  mFilterOps.mMaxDate == time_max() &&
					  hours > mFilterOps.mHoursAgo;
		bool stricter = mFilterOps.mMinDate == time_min() &&
						mFilterOps.mMaxDate == time_max() &&
						hours <= mFilterOps.mHoursAgo;
		mFilterOps.mHoursAgo = hours;
		mFilterOps.mMinDate = time_min();
		mFilterOps.mMaxDate = time_max();
		if (looser)
		{
			setModified(FILTER_LESS_RESTRICTIVE);
		}
		else if (stricter)
		{
			setModified(FILTER_MORE_RESTRICTIVE);
		}
		else
		{
			setModified(FILTER_RESTART);
		}
	}
}

void LLInventoryFilter::setShowFolderState(EFolderShow state)
{
	if (mFilterOps.mShowFolderState != state)
	{
		mFilterOps.mShowFolderState = state;
		if (state == SHOW_NON_EMPTY_FOLDERS)
		{
			// Showing fewer folders than before
			setModified(FILTER_MORE_RESTRICTIVE);
		}
		else if (state == SHOW_ALL_FOLDERS)
		{
			// Showing same folders as before and then some
			setModified(FILTER_LESS_RESTRICTIVE);
		}
		else
		{
			setModified();
		}
	}
}

void LLInventoryFilter::setSortOrder(U32 order)
{
	if (mOrder != order)
	{
		mOrder = order;
		setModified();
	}
}

void LLInventoryFilter::markDefault()
{
	mDefaultFilterOps = mFilterOps;
}

void LLInventoryFilter::resetDefault()
{
	mFilterOps = mDefaultFilterOps;
	setModified();
}

void LLInventoryFilter::setModified(EFilterBehavior behavior)
{
	mModified = true;
	mNeedTextRebuild = true;
	mFilterGeneration = mNextFilterGeneration++;

	if (mFilterBehavior == FILTER_NONE)
	{
		mFilterBehavior = behavior;
	}
	else if (mFilterBehavior != behavior)
	{
		// Trying to do both less restrictive and more restrictive filter
		// basically means restart from scratch
		mFilterBehavior = FILTER_RESTART;
	}

	if (isNotDefault())
	{
		// Tf not keeping current filter results, update last valid as well
		switch (mFilterBehavior)
		{
			case FILTER_RESTART:
				mMustPassGeneration = mFilterGeneration;
				mMinRequiredGeneration = mFilterGeneration;
				break;

			case FILTER_LESS_RESTRICTIVE:
				mMustPassGeneration = mFilterGeneration;
				break;

			case FILTER_MORE_RESTRICTIVE:
				mMinRequiredGeneration = mFilterGeneration;
				// Must have passed either current filter generation
				// (meaningless, as it has not been run yet) or some older
				// generation, so keep the value
				mMustPassGeneration = llmin(mMustPassGeneration, mFilterGeneration);
				break;

			default:
				llerrs << "Bad filter behavior specified" << llendl;
		}
	}
	else
	{
		// Shortcut disabled filters to show everything immediately
		mMinRequiredGeneration = 0;
		mMustPassGeneration = S32_MAX;
	}
}

bool LLInventoryFilter::isFilterWith(LLInventoryType::EType t)
{
	return (mFilterOps.mFilterTypes & (0x01 << t)) != 0;
}

std::string LLInventoryFilter::getFilterText()
{
	if (!mNeedTextRebuild)
	{
		return mFilterText;
	}

	mNeedTextRebuild = false;
	std::string filtered_types;
	std::string not_filtered_types;
    bool filtered_by_type = false;
    bool filtered_by_all_types = true;
	S32 num_filter_types = 0;
	mFilterText.clear();

	if (isFilterWith(LLInventoryType::IT_ANIMATION))
	{
		filtered_types += " Animations,";
		filtered_by_type = true;
		++num_filter_types;
	}
	else
	{
		not_filtered_types += " Animations,";
		filtered_by_all_types = false;
	}

	if (isFilterWith(LLInventoryType::IT_CALLINGCARD))
	{
		filtered_types += " Calling Cards,";
		filtered_by_type = true;
		++num_filter_types;
	}
	else
	{
		not_filtered_types += " Calling Cards,";
		filtered_by_all_types = false;
	}

	if (isFilterWith(LLInventoryType::IT_WEARABLE))
	{
		filtered_types += " Clothing,";
		filtered_by_type = true;
		++num_filter_types;
	}
	else
	{
		not_filtered_types += " Clothing,";
		filtered_by_all_types = false;
	}

	if (isFilterWith(LLInventoryType::IT_GESTURE))
	{
		filtered_types += " Gestures,";
		filtered_by_type = true;
		++num_filter_types;
	}
	else
	{
		not_filtered_types += " Gestures,";
		filtered_by_all_types = false;
	}

	if (isFilterWith(LLInventoryType::IT_LANDMARK))
	{
		filtered_types += " Landmarks,";
		filtered_by_type = true;
		++num_filter_types;
	}
	else
	{
		not_filtered_types += " Landmarks,";
		filtered_by_all_types = false;
	}

	if (isFilterWith(LLInventoryType::IT_NOTECARD))
	{
		filtered_types += " Notecards,";
		filtered_by_type = true;
		++num_filter_types;
	}
	else
	{
		not_filtered_types += " Notecards,";
		filtered_by_all_types = false;
	}

	if (isFilterWith(LLInventoryType::IT_OBJECT) &&
		isFilterWith(LLInventoryType::IT_ATTACHMENT))
	{
		filtered_types += " Objects,";
		filtered_by_type = true;
		++num_filter_types;
	}
	else
	{
		not_filtered_types += " Objects,";
		filtered_by_all_types = false;
	}

	if (isFilterWith(LLInventoryType::IT_LSL))
	{
		filtered_types += " Scripts,";
		filtered_by_type = true;
		++num_filter_types;
	}
	else
	{
		not_filtered_types += " Scripts,";
		filtered_by_all_types = false;
	}

	if (isFilterWith(LLInventoryType::IT_SOUND))
	{
		filtered_types += " Sounds,";
		filtered_by_type = true;
		++num_filter_types;
	}
	else
	{
		not_filtered_types += " Sounds,";
		filtered_by_all_types = false;
	}

	if (isFilterWith(LLInventoryType::IT_TEXTURE))
	{
		filtered_types += " Textures,";
		filtered_by_type = true;
		++num_filter_types;
	}
	else
	{
		not_filtered_types += " Textures,";
		filtered_by_all_types = false;
	}

	if (isFilterWith(LLInventoryType::IT_SNAPSHOT))
	{
		filtered_types += " Snapshots,";
		filtered_by_type = true;
		++num_filter_types;
	}
	else
	{
		not_filtered_types += " Snapshots,";
		filtered_by_all_types = false;
	}

	if (isFilterWith(LLInventoryType::IT_SETTINGS))
	{
		filtered_types += " Settings,";
		filtered_by_type = true;
		++num_filter_types;
	}
	else
	{
		not_filtered_types += " Settings,";
		filtered_by_all_types = false;
	}

	if (!LLInventoryModelFetch::getInstance()->backgroundFetchActive() &&
		filtered_by_type && !filtered_by_all_types)
	{
		mFilterText += " - ";
		if (num_filter_types < 5)
		{
			mFilterText += filtered_types;
		}
		else
		{
			mFilterText += "No ";
			mFilterText += not_filtered_types;
		}
		// Remove the ',' at the end
		mFilterText.erase(mFilterText.size() - 1, 1);
	}

	if (isSinceLogoff())
	{
		mFilterText += " - Since Logoff";
	}

	if (getFilterWorn())
	{
		mFilterText += " - Worn";
	}

	if (getFilterLastOpen())
	{
		mFilterText += " - Last Open";
	}

	return mFilterText;
}

void LLInventoryFilter::toLLSD(LLSD& data)
{
	data["filter_types"] = (LLSD::Integer)getFilterTypes();
	data["min_date"] = (LLSD::Integer)getMinDate();
	data["max_date"] = (LLSD::Integer)getMaxDate();
	data["hours_ago"] = (LLSD::Integer)getHoursAgo();
	data["show_folder_state"] = (LLSD::Integer)getShowFolderState();
	data["permissions"] = (LLSD::Integer)getFilterPermissions();
	data["substring"] = (LLSD::String)getFilterSubString();
	data["sort_order"] = (LLSD::Integer)getSortOrder();
	data["since_logoff"] = (LLSD::Boolean)isSinceLogoff();
}

void LLInventoryFilter::fromLLSD(LLSD& data)
{
	if (data.has("filter_types"))
	{
		setFilterTypes((U32)data["filter_types"].asInteger());
	}

	if (data.has("min_date") && data.has("max_date"))
	{
		setDateRange(data["min_date"].asInteger(), data["max_date"].asInteger());
	}

	if (data.has("hours_ago"))
	{
		setHoursAgo((U32)data["hours_ago"].asInteger());
	}

	if (data.has("show_folder_state"))
	{
		setShowFolderState((EFolderShow)data["show_folder_state"].asInteger());
	}

	if (data.has("permissions"))
	{
		setFilterPermissions((PermissionMask)data["permissions"].asInteger());
	}

	if (data.has("substring"))
	{
		setFilterSubString(std::string(data["substring"].asString()));
	}

	if (data.has("sort_order"))
	{
		setSortOrder((U32)data["sort_order"].asInteger());
	}

	if (data.has("since_logoff"))
	{
		setDateRangeLastLogoff((bool)data["since_logoff"].asBoolean());
	}
}

//-----------------------------------------------------------------------------
// LLSelectFirstFilteredItem class
//-----------------------------------------------------------------------------

void LLSelectFirstFilteredItem::doItem(LLFolderViewItem* item)
{
	if (item && item->getFiltered() && !mItemSelected)
	{
		LLFolderView* rootp = item->getRoot();
		if (rootp)
		{
			rootp->setSelection(item, false, false);
		}
		LLFolderViewFolder* parentp = item->getParentFolder();
		if (parentp)
		{
			parentp->setOpenArrangeRecursively(true,
											   LLFolderViewFolder::RECURSE_UP);
		}
		if (rootp)
		{
			rootp->scrollToShowSelection();
		}
		mItemSelected = true;
	}
}

void LLSelectFirstFilteredItem::doFolder(LLFolderViewFolder* folder)
{
	if (folder && folder->getFiltered() && !mItemSelected)
	{
		LLFolderView* rootp = folder->getRoot();
		if (rootp)
		{
			rootp->setSelection(folder, false, false);
		}
		LLFolderViewFolder* parentp = folder->getParentFolder();
		if (parentp)
		{
			parentp->setOpenArrangeRecursively(true,
											   LLFolderViewFolder::RECURSE_UP);
		}
		if (rootp)
		{
			rootp->scrollToShowSelection();
		}
		mItemSelected = true;
	}
}

//-----------------------------------------------------------------------------
// LLInventoryPanel class
//-----------------------------------------------------------------------------

// Bridge to support knowing when the inventory has changed.
class LLInventoryPanelObserver final : public LLInventoryObserver
{
public:
	LLInventoryPanelObserver(LLInventoryPanel* ip)
	:	mIP(ip)
	{
	}

	~LLInventoryPanelObserver() override
	{
	}

	LL_INLINE void changed(U32 mask) override
	{
		mIP->modelChanged(mask);
	}

protected:
	LLInventoryPanel* mIP;
};

LLInventoryPanel::LLInventoryPanel(const std::string& name,
								   const std::string& sort_order_setting,
								   const LLRect& rect,
								   LLInventoryModel* inventory,
								   bool allow_multi_select,
								   bool disable_double_click,
								   bool show_thumbnails)
:	LLPanel(name, rect, true),
	mInventory(inventory),
	mInventoryObserver(NULL),
	mFolders(NULL),
	mScroller(NULL),
	mAllowMultiSelect(allow_multi_select),
	mDoubleClickDisabled(disable_double_click),
	mSortOrderSetting(sort_order_setting),
	mLastOpenLocked(false),
	mShowThumbnails(show_thumbnails)
{
	setBackgroundColor(gColors.getColor("InventoryBackgroundColor"));
	setBackgroundVisible(true);
	setBackgroundOpaque(true);
}

bool LLInventoryPanel::postBuild()
{
	init_inventory_panel_actions(this);

	LLRect folder_rect(0, 0, getRect().getWidth(), 0);
	mFolders = new LLFolderView(getName(), NULL, folder_rect, LLUUID::null,
								this);
	mFolders->setAllowMultiSelect(mAllowMultiSelect);
	mFolders->setShowThumbnails(mShowThumbnails);

	// Scroller
	LLRect scroller_view_rect = getRect();
	scroller_view_rect.translate(-scroller_view_rect.mLeft,
								 -scroller_view_rect.mBottom);
	mScroller = new LLScrollableContainer("Inventory Scroller",
										  scroller_view_rect, mFolders);
	mScroller->setFollowsAll();
	mScroller->setReserveScrollCorner(true);
	addChild(mScroller);
	mFolders->setScrollContainer(mScroller);

	// Set up the callbacks from the inventory we are viewing, and then build
	// everything.
	mInventoryObserver = new LLInventoryPanelObserver(this);
	mInventory->addObserver(mInventoryObserver);
	rebuildViewsFor(LLUUID::null);

	// A bit of a hack to make sure the inventory is open.
	mFolders->openFolder("My Inventory");

	if (!mSortOrderSetting.empty())
	{
		setSortOrder(gSavedSettings.getU32(mSortOrderSetting.c_str()));
	}
	else
	{
		setSortOrder(gSavedSettings.getU32("InventorySortOrder"));
	}
	mFolders->setSortOrder(mFolders->getFilter()->getSortOrder());

	return true;
}

//virtual
LLInventoryPanel::~LLInventoryPanel()
{
	// Should this be a global setting ?
	U32 sort_order = mFolders->getSortOrder();
	if (!mSortOrderSetting.empty())
	{
		gSavedSettings.setU32(mSortOrderSetting.c_str(), sort_order);
	}

	// LLView destructor will take care of the sub-views.
	mInventory->removeObserver(mInventoryObserver);
	delete mInventoryObserver;
	mScroller = NULL;

	if (mShowThumbnails)
	{
		// Close the temporary thumbnail view floater, if open.
		HBFloaterThumbnail::hideInstance();
	}
}

//virtual
LLXMLNodePtr LLInventoryPanel::getXML(bool save_children) const
{
	LLXMLNodePtr node = LLPanel::getXML(false); // Do not print out children

	node->setName(LL_INVENTORY_PANEL_TAG);

	node->createChild("allow_multi_select",
					  true)->setBoolValue(mFolders->getAllowMultiSelect());

	return node;
}

LLView* LLInventoryPanel::fromXML(LLXMLNodePtr node, LLView* parentp,
								  LLUICtrlFactory*)
{
	std::string name = LL_INVENTORY_PANEL_TAG;
	node->getAttributeString("name", name);

	LLRect rect;
	createRect(node, rect, parentp, LLRect());

	std::string sort_order;
	node->getAttributeString("sort_order", sort_order);

	bool allow_multi_select = true;
	node->getAttributeBool("allow_multi_select", allow_multi_select);

	bool disable_double_click = false;
	node->getAttributeBool("disable_double_click", disable_double_click);

	bool show_thumbnails = false;
	node->getAttributeBool("show_thumbnails", show_thumbnails);

	LLInventoryPanel* self = new LLInventoryPanel(name, sort_order, rect,
												  &gInventory,
												  allow_multi_select,
												  disable_double_click,
												  show_thumbnails);
	self->initFromXML(node, parentp);
	self->postBuild();

	return self;
}

void LLInventoryPanel::draw()
{
	// Select the desired item (in case it was not loaded when the selection
	// was requested)
	if (mSelectThisID.notNull())
	{
		setSelection(mSelectThisID, false);
	}
	LLPanel::draw();
}

void LLInventoryPanel::setFilterTypes(U32 filter_types)
{
	mFolders->getFilter()->setFilterTypes(filter_types);
}

void LLInventoryPanel::setFilterHideLibrary(bool hide)
{
	mFolders->getFilter()->setFilterHideLibrary(hide);
}

void LLInventoryPanel::setFilterSubType(S32 subtype)
{
	mFolders->getFilter()->setFilterSubType(subtype);
}

void LLInventoryPanel::setFilterPermMask(PermissionMask filter_perm_mask)
{
	mFolders->getFilter()->setFilterPermissions(filter_perm_mask);
}

void LLInventoryPanel::setFilterSubString(const std::string& string)
{
	mFolders->getFilter()->setFilterSubString(string);
}

void LLInventoryPanel::setFilterWorn(bool worn)
{
	mFolders->getFilter()->setFilterWorn(worn);
}

void LLInventoryPanel::setFilterLastOpen(bool b)
{
	mFolders->getFilter()->setFilterLastOpen(b);
}

void LLInventoryPanel::setFilterShowLinks(bool b)
{
	mFolders->getFilter()->setFilterShowLinks(b);
}

void LLInventoryPanel::setSortOrder(U32 order)
{
	mFolders->getFilter()->setSortOrder(order);
	if (mFolders->getFilter()->isModified())
	{
		mFolders->setSortOrder(order);
		// Try to keep selection onscreen, even if it was not to start with
		mFolders->scrollToShowSelection();
	}
}

void LLInventoryPanel::setSinceLogoff(bool sl)
{
	mFolders->getFilter()->setDateRangeLastLogoff(sl);
}

void LLInventoryPanel::setHoursAgo(U32 hours)
{
	mFolders->getFilter()->setHoursAgo(hours);
}

void LLInventoryPanel::setShowFolderState(LLInventoryFilter::EFolderShow show)
{
	mFolders->getFilter()->setShowFolderState(show);
}

LLInventoryFilter::EFolderShow LLInventoryPanel::getShowFolderState()
{
	return mFolders->getFilter()->getShowFolderState();
}

void LLInventoryPanel::modelChanged(U32 mask)
{
	LL_FAST_TIMER(FTM_REFRESH);

	uuid_list_t::const_iterator id_it, id_end;

	bool handled = false;
	if (mask & LLInventoryObserver::LABEL)
	{
		handled = true;
		// Label change: empty out the display name for each object in this
		// change set.
		const uuid_list_t& changed_items = gInventory.getChangedIDs();
		LLFolderViewItem* view = NULL;
		LLInvFVBridge* bridge = NULL;
		for (id_it = changed_items.begin(), id_end = changed_items.end();
			 id_it != id_end; ++id_it)
		{
			view = mFolders->getItemByID(*id_it);
			if (view)
			{
				// Request refresh on this item (also flags for filtering)
				bridge = (LLInvFVBridge*)view->getListener();
				if (bridge)
				{
					// Clear the display name first, so it gets properly
					// re-built during refresh()
					bridge->clearDisplayName();
				}
				view->refresh();
			}
		}
	}
	if (mask & LLInventoryObserver::REBUILD)
	{
		handled = true;
		// Icon change for each object in this change set.
		const uuid_list_t& changed_items = gInventory.getChangedIDs();
		for (id_it = changed_items.begin(), id_end = changed_items.end();
			 id_it != id_end; ++id_it)
		{
			// Sync view with model
			LLInventoryModel* model = getModel();
			if (model)
			{
				LLInventoryObject* model_item = model->getObject(*id_it);
				LLFolderViewItem* view_item = mFolders->getItemByID(*id_it);
				if (model_item && view_item)
				{
					view_item->destroyView();
				}
				buildNewViews(*id_it);
			}
		}
	}
	if ((mask & (LLInventoryObserver::STRUCTURE | LLInventoryObserver::ADD |
				 LLInventoryObserver::REMOVE)) != 0)
	{
		handled = true;
		// Record which folders are open by uuid.
		LLInventoryModel* model = getModel();
		if (model)
		{
			const uuid_list_t& changed_items = gInventory.getChangedIDs();
			for (id_it = changed_items.begin(), id_end = changed_items.end();
				 id_it != id_end; ++id_it)
			{
				// Sync view with model
				LLInventoryObject* model_item = model->getObject(*id_it);
				LLFolderViewItem* view_item = mFolders->getItemByID(*id_it);

				if (model_item)
				{
					if (!view_item)
					{
						// This object was just created, need to build a view
						// for it
						if ((mask & LLInventoryObserver::ADD) !=
								LLInventoryObserver::ADD)
						{
							llwarns << *id_it
									<< " is in model but not in view, but ADD flag not set"
									<< llendl;
						}
						buildNewViews(*id_it);

						// Select any newly created object that has the auto
						// rename at top of folder root set
						if (mFolders->getRoot()->needsAutoRename())
						{
							setSelection(*id_it, false);
						}
					}
					else
					{
						// This object was probably moved, check its parent

#if 0					// Inventory links cause many such warnings while they
						// do not correspond to an actual issue...
						if ((mask & LLInventoryObserver::STRUCTURE) !=
								LLInventoryObserver::STRUCTURE)
						{
							llwarns << *id_it
									<< " is in model and in view, but STRUCTURE flag not set"
									<< llendl;
						}
#endif
						LLFolderViewFolder* new_parent =
							(LLFolderViewFolder*)mFolders->getItemByID(model_item->getParentUUID());
						if (new_parent)
						{
							if (view_item->getParentFolder() != new_parent)
							{
								view_item->getParentFolder()->extractItem(view_item);
								view_item->addToFolder(new_parent, mFolders);
							}
						}
						else
						{
							llwarns << model_item->getParentUUID()
									<< ": parent folder gone !  Destroying orphan view."
									<< llendl;
							view_item->destroyView();
						}
					}
				}
				else if (view_item)
				{
					if ((mask & LLInventoryObserver::REMOVE) !=
							LLInventoryObserver::REMOVE)
					{
						llwarns << *id_it
								<< " is not in model but in view, but REMOVE flag not set"
								<< llendl;
					}
					// Item in view but not model, need to delete view
					view_item->destroyView();
				}
				else
				{
					llwarns << *id_it
							<< ": Item does not exist in either view or model, but notification triggered"
							<< llendl;
				}
			}
		}
	}

	if (!handled)
	{
		// It is a small change that only requires a refresh.
		// *TODO: figure out a more efficient way to do the refresh since it is
		// expensive on large inventories.
		mFolders->refresh();
	}
}

void LLInventoryPanel::rebuildViewsFor(const LLUUID& id)
{
	LLFolderViewItem* old_view = mFolders->getItemByID(id);
	if (old_view && id.notNull())
	{
		old_view->destroyView();
	}

	buildNewViews(id);
}

void LLInventoryPanel::buildNewViews(const LLUUID& id)
{
	LLFolderViewFolder* parent_folder = NULL;
	LLInventoryObject* objectp = gInventory.getObject(id);
	if (mFolders->getListener() && id == mFolders->getListener()->getUUID())
	{
		parent_folder = mFolders;
	}
	else if (objectp)
	{
		const LLUUID& parent_id = objectp->getParentUUID();
		parent_folder = (LLFolderViewFolder*)mFolders->getItemByID(parent_id);
		if (parent_folder)
		{
			LLFolderViewItem* itemp = NULL;
			if (objectp->getType() <= LLAssetType::AT_NONE ||
				objectp->getType() >= LLAssetType::AT_COUNT)
			{
				llwarns_once << "Called with unsupported asset type: "
							 << (S32)objectp->getType() << llendl;
			}
			else if (objectp->getType() == LLAssetType::AT_CATEGORY &&
					 objectp->getActualType() != LLAssetType::AT_LINK_FOLDER)
			{
				// Build new view for category
				LLInvFVBridge* new_listener =
					LLInvFVBridge::createBridge(objectp->getType(),
												objectp->getType(),
												LLInventoryType::IT_CATEGORY,
												this, objectp->getUUID());
				if (new_listener)
				{
					LLFolderViewFolder* folderp =
						new LLFolderViewFolder(new_listener->getDisplayName(),
											   new_listener->getIcon(),
											   mFolders, new_listener);
					folderp->setItemSortOrder(mFolders->getSortOrder());
					itemp = folderp;
				}
			}
			else
			{
				// Build new view for item
				LLInventoryItem* item = (LLInventoryItem*)objectp;
				LLInvFVBridge* new_listener =
					LLInvFVBridge::createBridge(item->getType(),
												item->getActualType(),
												item->getInventoryType(), this,
												item->getUUID(),
												item->getFlags());
				if (new_listener)
				{
					itemp = new LLFolderViewItem(new_listener->getDisplayName(),
												 new_listener->getIcon(),
												 new_listener->getCreationDate(),
												 mFolders,
												 new_listener);
					if (itemp && mDoubleClickDisabled)
					{
						itemp->disableDoubleClick();
					}
				}
			}
			if (itemp)
			{
				itemp->addToFolder(parent_folder, mFolders);
			}
		}
	}

	// If this is a folder, recursively add all the children
	if (id.isNull() ||
		(objectp && objectp->getType() == LLAssetType::AT_CATEGORY))
	{
		LLViewerInventoryCategory::cat_array_t* categories;
		LLViewerInventoryItem::item_array_t* items;
#if LL_DEBUG
		mInventory->lockDirectDescendentArrays(id, categories, items);
#else
		mInventory->getDirectDescendentsOf(id, categories, items);
#endif
		if (categories)
		{
			for (S32 i = 0, count = categories->size(); i < count; ++i)
			{
				LLInventoryCategory* cat = (*categories)[i];
				if (cat)	// Paranoia
				{
					buildNewViews(cat->getUUID());
				}
			}
		}

		if (items && parent_folder)
		{
			for (S32 i = 0, count = items->size(); i < count; ++i)
			{
				LLInventoryItem* item = (*items)[i];
				if (item)	// Paranoia
				{
					buildNewViews(item->getUUID());
				}
			}
		}
#if LL_DEBUG
		mInventory->unlockDirectDescendentArrays(id);
#endif
	}
}

void LLInventoryPanel::openSelected()
{
	LLFolderViewItem* folder_item = mFolders->getCurSelectedItem();
	if (folder_item)
	{
		LLInvFVBridge* bridge = (LLInvFVBridge*)folder_item->getListener();
		if (bridge)
		{
			bridge->openItem();
		}
	}
}

bool LLInventoryPanel::handleHover(S32 x, S32 y, MASK mask)
{
	if (!gWindowp) return true;	// Paranoia

	// Has everything been fetched in the inventory ?
	bool fetched = LLInventoryModelFetch::getInstance()->isEverythingFetched();
	if (!fetched)
	{
		// Force the hourglass cursor
		gWindowp->setCursor(UI_CURSOR_WORKING);
		// Prevent any changes to cursor done by LLView::handleHover() to avoid
		// occasionnal flickering. HB
		gWindowp->freezeCursor(true);
	}
	bool handled = LLView::handleHover(x, y, mask);
	if (fetched)
	{
		if (!handled)
		{
			// Restore the arrow cursor
			gWindowp->setCursor(UI_CURSOR_ARROW);
		}
	}
	else
	{
		gWindowp->freezeCursor(false);
	}

	return handled;
}

bool LLInventoryPanel::handleDragAndDrop(S32 x, S32 y, MASK mask, bool drop,
										 EDragAndDropType cargo_type,
										 void* cargo_data, EAcceptance* accept,
										 std::string& tooltip_msg)
{

	bool handled = LLPanel::handleDragAndDrop(x, y, mask, drop, cargo_type,
											  cargo_data, accept, tooltip_msg);
	if (handled)
	{
		mFolders->setDragAndDropThisFrame();
	}

	return handled;
}

void LLInventoryPanel::openAllFolders()
{
	mFolders->setOpenArrangeRecursively(true,
										LLFolderViewFolder::RECURSE_DOWN);
	mFolders->arrangeAll();
}

void LLInventoryPanel::closeAllFolders()
{
	mFolders->setOpenArrangeRecursively(false,
										LLFolderViewFolder::RECURSE_DOWN);
	mFolders->arrangeAll();
}

void LLInventoryPanel::openDefaultFolderForType(LLAssetType::EType type)
{
	const LLUUID& category_id =
		mInventory->findCategoryUUIDForType(LLFolderType::assetTypeToFolderType(type));
	LLOpenFolderByID opener(category_id);
	mFolders->applyFunctorRecursively(opener);
}

void LLInventoryPanel::setSelection(const LLUUID& obj_id,
									bool take_keyboard_focus)
{
	LLFolderViewItem* itemp = mFolders->getItemByID(obj_id);
	if (itemp && itemp->getListener())
	{
		itemp->getListener()->arrangeAndSet(itemp, true, take_keyboard_focus);
		mSelectThisID.setNull();
		return;
	}

	// Save the desired item to be selected later (if/when ready)
	mSelectThisID = obj_id;
}

void LLInventoryPanel::clearSelection()
{
	mFolders->clearSelection();
	mSelectThisID.setNull();
}

bool LLInventoryPanel::makeLastOpenCurrent()
{
	if ((mLastOpenID.notNull() && gInventory.getCategory(mLastOpenID)) &&
		(mLastOpenLocked || LLFolderViewFolder::sLastOpenId.isNull() ||
		 mLastOpenID == LLFolderViewFolder::sLastOpenId ||
		 !gInventory.getCategory(LLFolderViewFolder::sLastOpenId)))
	{
		return false;
	}

	mLastOpenID = LLFolderViewFolder::sLastOpenId;
	mFolders->openFolder(mLastOpenID);

	return true;
}
