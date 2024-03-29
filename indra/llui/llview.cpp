/**
 * @file llview.cpp
 * @author James Cook
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

#include "linden_common.h"

#include <utility>

#include "boost/tokenizer.hpp"

#include "llview.h"

#include "llcolor3.h"
#include "llrender.h"
#include "llstl.h"
#include "lluictrl.h"
#include "lluictrlfactory.h"
#include "llwindow.h"

// For UI edit hack
#include "llbutton.h"
#include "lllineeditor.h"
#include "llmultislider.h"
#include "llscrolllistctrl.h"
#include "llslider.h"
#include "lltextbox.h"
#include "lltexteditor.h"
#include "llvirtualtrackball.h"
#include "llxyvector.h"

using namespace LLOldEvents;

// *HACK: this allows you to instantiate LLView from xml with "<view/>" which
// we do not want
static const std::string LL_VIEW_TAG = "view";
static LLRegisterWidget<LLView> r32(LL_VIEW_TAG);

bool LLView::sDebugRects = false;
bool LLView::sDebugKeys = false;
S32 LLView::sDepth = 0;
bool LLView::sDebugMouseHandling = false;
std::string LLView::sMouseHandlerMessage;
bool LLView::sEditingUI = false;
bool LLView::sForceReshape = false;
LLView* LLView::sEditingUIView = NULL;
S32 LLView::sLastLeftXML = S32_MIN;
S32 LLView::sLastBottomXML = S32_MIN;

constexpr S32 FLOATER_H_MARGIN = 15;
constexpr S32 MIN_WIDGET_HEIGHT = 10;

LLView::LLView()
:	mParentView(NULL),
	mReshapeFlags(FOLLOWS_NONE),
	mDefaultTabGroup(0),
	mEnabled(true),
	mMouseOpaque(true),
	mSoundFlags(MOUSE_UP), // Default to only make sound on mouse up
	mSaveToXML(true),
	mIsFocusRoot(false),
	mVisible(true),
	mLastVisible(true),
	mUseBoundingRect(false),
	mNextInsertionOrdinal(0),
	mHoverCursor(UI_CURSOR_ARROW),
	mChildListSize(0),
	mToolTipMsgPtr(NULL)
{
}

LLView::LLView(const std::string& name, bool mouse_opaque)
:	mParentView(NULL),
	mName(name),
	mReshapeFlags(FOLLOWS_NONE),
	mDefaultTabGroup(0),
	mEnabled(true),
	mMouseOpaque(mouse_opaque),
	mSoundFlags(MOUSE_UP), // Default to only make sound on mouse up
	mSaveToXML(true),
	mIsFocusRoot(false),
	mVisible(true),
	mLastVisible(true),
	mUseBoundingRect(false),
	mNextInsertionOrdinal(0),
	mHoverCursor(UI_CURSOR_ARROW),
	mChildListSize(0),
	mToolTipMsgPtr(NULL)
{
}

LLView::LLView(const std::string& name, const LLRect& rect, bool mouse_opaque,
			   U8 reshape)
:	mParentView(NULL),
	mName(name),
	mRect(rect),
	mBoundingRect(rect),
	mReshapeFlags(reshape),
	mDefaultTabGroup(0),
	mEnabled(true),
	mMouseOpaque(mouse_opaque),
	mSoundFlags(MOUSE_UP),		// Default to only make sound on mouse up
	mSaveToXML(true),
	mIsFocusRoot(false),
	mVisible(true),
	mLastVisible(true),
	mUseBoundingRect(false),
	mNextInsertionOrdinal(0),
	mHoverCursor(UI_CURSOR_ARROW),
	mChildListSize(0),
	mToolTipMsgPtr(NULL)
{
}

LLView::~LLView()
{
	LL_DEBUGS("View") << "Deleting view " << mName << " : " << (void*)this
					  << LL_ENDL;

	if (hasMouseCapture())
	{
		llwarns << "View holding mouse capture deleted: " << getName()
				<< ".  Mouse capture removed." << llendl;
		gFocusMgr.removeMouseCaptureWithoutCallback(this);
	}

	deleteAllChildren();

	if (mParentView != NULL)
	{
		mParentView->removeChild(this);
	}

	dispatch_list_t::iterator it;
	for (it = mDispatchList.begin(); it != mDispatchList.end(); ++it)
	{
		it->second->clearDispatchers();
	}

	if (mToolTipMsgPtr)
	{
		delete mToolTipMsgPtr;
		mToolTipMsgPtr = NULL;
	}

	for (auto it = mControls.begin(), end = mControls.end();
		 it != end; ++it)
	{
		delete it->second;
	}
	mControls.clear();

	for (auto it = mDummyWidgets.begin(), end = mDummyWidgets.end();
		 it != end; ++it)
	{
		delete it->second;
	}
	mDummyWidgets.clear();
}

//virtual
void LLView::setToolTip(const std::string& msg)
{
	if (!mToolTipMsgPtr)
	{
		mToolTipMsgPtr = new LLUIString(msg);
	}
	else
	{
		mToolTipMsgPtr->assign(msg);
	}
}

bool LLView::setToolTipArg(const std::string& key, const std::string& text)
{
	if (!mToolTipMsgPtr)
	{
		mToolTipMsgPtr = new LLUIString("");
	}
	mToolTipMsgPtr->setArg(key, text);
	return true;
}

void LLView::setToolTipArgs(const LLStringUtil::format_map_t& args)
{
	if (!mToolTipMsgPtr)
	{
		mToolTipMsgPtr = new LLUIString("");
	}
	mToolTipMsgPtr->setArgList(args);
}

const std::string& LLView::getToolTip() const
{
	return mToolTipMsgPtr ? mToolTipMsgPtr->getString() : LLStringUtil::null;
}

//virtual
void LLView::setRect(const LLRect& rect)
{
	mRect = rect;
	updateBoundingRect();
}

void LLView::setUseBoundingRect(bool use_bounding_rect)
{
	if (mUseBoundingRect != use_bounding_rect)
	{
        mUseBoundingRect = use_bounding_rect;
		updateBoundingRect();
	}
}

//virtual
std::string LLView::getName() const
{
	return mName.empty() ? "(no name)" : mName;
}

void LLView::sendChildToFront(LLView* child)
{
	if (child && child->getParent() == this)
	{
		mChildList.remove(child);
		mChildList.push_front(child);
		// Paranoia: in case child was not in mChildList or was listed several
		// times in it...
		mChildListSize = mChildList.size();
	}
}

void LLView::sendChildToBack(LLView* child)
{
	if (child && child->getParent() == this)
	{
		mChildList.remove(child);
		mChildList.push_back(child);
		// Paranoia: in case child was not in mChildList or was listed several
		// times in it...
		mChildListSize = mChildList.size();
	}
}

void LLView::moveChildToFrontOfTabGroup(LLUICtrl* child)
{
	if (child && mCtrlOrder.find(child) != mCtrlOrder.end())
	{
		mCtrlOrder[child].second = -1 * mNextInsertionOrdinal++;
	}
}

void LLView::moveChildToBackOfTabGroup(LLUICtrl* child)
{
	if (child && mCtrlOrder.find(child) != mCtrlOrder.end())
	{
		mCtrlOrder[child].second = mNextInsertionOrdinal++;
	}
}

void LLView::addChild(LLView* child, S32 tab_group)
{
	if (!child)
	{
		llwarns << "Trying to add a NULL child" << llendl;
		return;
	}

	if (mParentView == child)
	{
		llerrs << "Adding view " << child->getName() << " as child of itself"
			   << llendl;
	}
	// Remove from current parent
	if (child->mParentView)
	{
		child->mParentView->removeChild(child);
	}

	// Add to front of child list, as normal
	mChildList.push_front(child);
	// Paranoia: use this instead of mChildListSize++ in case child was already
	// parented to this view...
	mChildListSize = mChildList.size();

	// Add to control list if it is LLUICtrl
	if (child->isCtrl())
	{
		// Controls are stored in reverse order from render order
		addCtrlAtEnd((LLUICtrl*)child, tab_group);
	}

	child->mParentView = this;

	// If child is not visible it would not affect bounding rect; if current
	// view is not visible it will be recalculated on visibility change.
	if (getVisible() && child->getVisible())
	{
		updateBoundingRect();
	}
}

void LLView::addChildAtEnd(LLView* child, S32 tab_group)
{
	if (!child)
	{
		llwarns << "Trying to add a NULL child at end" << llendl;
		return;
	}

	if (mParentView == child)
	{
		llerrs << "Adding view " << child->getName() << " as child of itself"
			   << llendl;
	}
	// remove from current parent
	if (child->mParentView)
	{
		child->mParentView->removeChild(child);
	}

	// Add to back of child list
	mChildList.push_back(child);
	// Paranoia: use this instead of mChildListSize++ in case child was already
	// parented to this view...
	mChildListSize = mChildList.size();

	// Add to control list if it is LLUICtrl
	if (child->isCtrl())
	{
		// Controls are stored in reverse order from render order
		addCtrl((LLUICtrl*) child, tab_group);
	}

	child->mParentView = this;
	updateBoundingRect();
}

// Remove the specified child from the view, and set its parent to NULL.
void LLView::removeChild(LLView* child, bool delete_it)
{
	if (!child)
	{
		llwarns << "Trying to remove a NULL child" << llendl;
		return;
	}

	if (child->mParentView == this)
	{
		mChildList.remove(child);
		// Paranoia: use this instead of mChildListSize-- in case child was
		// not in mChildList or was listed several times in it...
		mChildListSize = mChildList.size();
		child->mParentView = NULL;
		if (child->isCtrl())
		{
			removeCtrl((LLUICtrl*)child);
		}
		if (delete_it)
		{
			delete child;
		}
	}
	else
	{
		llwarns << "Call done with non-child. Ignored." << llendl;
	}
	updateBoundingRect();
}

void LLView::addCtrlAtEnd(LLUICtrl* ctrl, S32 tab_group)
{
	if (!ctrl)
	{
		llwarns << "Trying to add a NULL control at end" << llendl;
		return;
	}

	mCtrlOrder.insert(tab_order_pair_t(ctrl,
									  tab_order_t(tab_group,
												  mNextInsertionOrdinal++)));
}

void LLView::addCtrl(LLUICtrl* ctrl, S32 tab_group)
{
	if (!ctrl)
	{
		llwarns << "Trying to add a NULL control" << llendl;
		return;
	}

	// add to front of list by using negative ordinal, which monotonically
	// increases
	mCtrlOrder.insert(tab_order_pair_t(ctrl,
									   tab_order_t(tab_group,
												   -1 * mNextInsertionOrdinal++)));
}

void LLView::removeCtrl(LLUICtrl* ctrl)
{
	if (!ctrl)
	{
		llwarns << "Trying to remove a NULL control" << llendl;
		return;
	}

	child_tab_order_t::iterator found = mCtrlOrder.find(ctrl);
	if (found != mCtrlOrder.end())
	{
		mCtrlOrder.erase(found);
	}
}

LLView::ctrl_list_t LLView::getCtrlList() const
{
	ctrl_list_t controls;
	for (child_list_const_iter_t iter = mChildList.begin(),
								 end = mChildList.end();
		 iter != end; ++iter)
	{
		LLView* childp = *iter;
		if (childp && childp->isCtrl())
		{
			controls.push_back((LLUICtrl*)childp);
		}
	}
	return controls;
}

LLView::ctrl_list_t LLView::getCtrlListSorted() const
{
	ctrl_list_t controls = getCtrlList();
	std::sort(controls.begin(), controls.end(),
			  LLCompareByTabOrder(mCtrlOrder));
	return controls;
}

// This method compares two LLViews by the tab order specified in the
// comparator object. The code for this is a little convoluted because each
// argument can have four states:
// 1) not a control, 2) a control but not in the tab order,
// 3) a control in the tab order, 4) null
bool LLCompareByTabOrder::operator() (const LLView* const a,
									  const LLView* const b) const
{
	if (!a) return false;
	if (!b) return true;
	S32 a_score = a->isCtrl() ? -1 : 0;
	S32 b_score = b->isCtrl() ? -1 : 0;
	if (a_score == -1 && b_score == -1)
	{
		LLView::child_tab_order_const_iter_t a_found =
			mTabOrder.find((const LLUICtrl*)a);
		LLView::child_tab_order_const_iter_t b_found =
			mTabOrder.find((const LLUICtrl*)b);
		if (a_found != mTabOrder.end()) --a_score;
		if (b_found != mTabOrder.end()) --b_score;
		if (a_score == -2 && b_score == -2)
		{
			// Once we are in here, they are both in the tab order, and we can
			// compare based on that
			return compareTabOrders(a_found->second, b_found->second);
		}
	}
	return a_score == b_score ? a < b : a_score < b_score;
}

bool LLView::isInVisibleChain() const
{
	const LLView* cur_view = this;
	while (cur_view)
	{
		if (!cur_view->getVisible())
		{
			return false;
		}
		cur_view = cur_view->getParent();
	}
	return true;
}

bool LLView::isInEnabledChain() const
{
	const LLView* cur_view = this;
	while (cur_view)
	{
		if (!cur_view->getEnabled())
		{
			return false;
		}
		cur_view = cur_view->getParent();
	}
	return true;
}

bool LLView::focusNextRoot()
{
	LLView::child_list_t result = LLView::getFocusRootsQuery().run(this);
	return LLView::focusNext(result);
}

bool LLView::focusPrevRoot()
{
	LLView::child_list_t result = LLView::getFocusRootsQuery().run(this);
	return LLView::focusPrev(result);
}

//static
bool LLView::focusNext(LLView::child_list_t& result)
{
	LLView::child_list_iter_t result_begin = result.begin();
	LLView::child_list_iter_t result_end = result.end();
	LLView::child_list_iter_t focused = result_end;
	for (LLView::child_list_iter_t iter = result_begin; iter != result_end;
		 ++iter)
	{
		if (gFocusMgr.childHasKeyboardFocus(*iter))
		{
			focused = iter;
			break;
		}
	}
	LLView::child_list_iter_t next = focused;
	next = (next == result_end) ? result_begin : ++next;
	while (next != focused)
	{
		// wrap around to beginning if necessary
		if (next == result_end)
		{
			next = result_begin;
		}
		if ((*next)->isCtrl())
		{
			LLUICtrl* ctrl = (LLUICtrl*)*next;
			ctrl->setFocus(true);
			ctrl->onTabInto();
			gFocusMgr.triggerFocusFlash();
			return true;
		}
		++next;
	}
	return false;
}

//static
bool LLView::focusPrev(LLView::child_list_t& result)
{
	LLView::child_list_reverse_iter_t result_rbegin = result.rbegin();
	LLView::child_list_reverse_iter_t result_rend = result.rend();
	LLView::child_list_reverse_iter_t focused = result_rend;
	for (LLView::child_list_reverse_iter_t iter = result_rbegin;
		 iter != result_rend; ++iter)
	{
		if (gFocusMgr.childHasKeyboardFocus(*iter))
		{
			focused = iter;
			break;
		}
	}
	LLView::child_list_reverse_iter_t next = focused;
	next = next == result_rend ? result_rbegin : ++next;
	while (next != focused)
	{
		// wrap around to beginning if necessary
		if (next == result_rend)
		{
			next = result_rbegin;
		}

		LLView* childp = *next++;
		if (childp && childp->isCtrl())
		{
			LLUICtrl* ctrl = (LLUICtrl*)childp;
			if (!ctrl->hasFocus())
			{
				ctrl->setFocus(true);
				ctrl->onTabInto();
				gFocusMgr.triggerFocusFlash();
			}
			return true;
		}
	}
	return false;
}

// Delete all children. Override this function if you need to perform any extra
// clean up such as cached pointers to selected children, etc.
void LLView::deleteAllChildren()
{
	// Clear out the control ordering
	mCtrlOrder.clear();

	while (!mChildList.empty())
	{
		LLView* viewp = mChildList.front();
		viewp->mParentView = NULL;
		delete viewp;
		mChildList.pop_front();
	}
	mChildListSize = 0;
}

void LLView::setAllChildrenEnabled(bool b)
{
	for (child_list_iter_t child_it = mChildList.begin(),
						   end = mChildList.end();
		 child_it != end; ++child_it)
	{
		LLView* viewp = *child_it;
		if (viewp)
		{
			viewp->setEnabled(b);
		}
	}
}

//virtual
void LLView::setVisible(bool visible)
{
	if (mVisible != visible)
	{
		if (!visible && gFocusMgr.getTopCtrl() == this)
		{
			gFocusMgr.setTopCtrl(NULL);
		}

		mVisible = visible;

		// notify children of visibility change if root, or part of visible
		// hierarchy
		if (!getParent() || getParent()->isInVisibleChain())
		{
			// tell all children of this view that the visibility may have
			// changed
			onVisibilityChange(visible);
		}
		updateBoundingRect();
	}
}

//virtual
void LLView::onVisibilityChange(bool new_visibility)
{
	for (child_list_iter_t child_it = mChildList.begin(),
						   end = mChildList.end();
		 child_it != end; ++child_it)
	{
		LLView* viewp = *child_it;
		// only views that are themselves visible will have their overall
		// visibility affected by their ancestors
		if (viewp && viewp->getVisible())
		{
			viewp->onVisibilityChange(new_visibility);
		}
	}
}

//virtual
void LLView::translate(S32 x, S32 y)
{
	mRect.translate(x, y);
	updateBoundingRect();
}

//virtual
bool LLView::canSnapTo(LLView* other_view)
{
	return other_view != this && other_view->getVisible();
}

bool LLView::handleHover(S32 x, S32 y, MASK mask)
{
	bool handled = childrenHandleHover(x, y, mask) != NULL;
	if (!handled && blockMouseEvent(x, y))
	{
		gWindowp->setCursor(mHoverCursor);
		LL_DEBUGS("UserInput") << "hover handled by " << getName() << LL_ENDL;
		handled = true;
	}

	return handled;
}

std::string LLView::getShowNamesToolTip()
{
	LLView* view = getParent();
	std::string name;
	std::string tool_tip = mName;

	while (view)
	{
		name = view->getName();

		if (name == "root") break;

		if (view->getToolTip().find(".xml") != std::string::npos)
		{
			tool_tip = view->getToolTip() + "/" +  tool_tip;
			break;
		}
		else
		{
			tool_tip = view->getName() + "/" +  tool_tip;
		}

		view = view->getParent();
	}

	return "/" + tool_tip;
}

bool LLView::handleToolTip(S32 x, S32 y, std::string& msg,
						   LLRect* sticky_rect_screen)
{
	bool handled = false;

	for (child_list_iter_t it = mChildList.begin(), end = mChildList.end();
		 it != end; ++it)
	{
		LLView* viewp = *it;
		if (!viewp) continue;	// Paranoia

		S32 local_x = x - viewp->mRect.mLeft;
		S32 local_y = y - viewp->mRect.mBottom;
		// Allow tooltips for disabled views so we can explain to the user why
		// the view is disabled. JC
		if (viewp->getVisible() && //viewp->getEnabled() &&
			viewp->pointInView(local_x, local_y) &&
			viewp->handleToolTip(local_x, local_y, msg, sticky_rect_screen))
		{
			// Child provided a tooltip, just return
			if (!msg.empty()) return true;

			// Otherwise, one of our children ate the event so do not traverse
			// siblings however, our child did not actually provide a tooltip
            // so we might want to.
			handled = true;
			break;
		}
	}

	// Get our own tooltip
	std::string tool_tip = mToolTipMsgPtr ? mToolTipMsgPtr->getString() : "";
	if (LLUI::sShowXUINames && tool_tip.find(".xml", 0) == std::string::npos)
	{
		tool_tip = getShowNamesToolTip();
	}

	bool show_names_text_box = LLUI::sShowXUINames &&
							   dynamic_cast<LLTextBox*>(this) != NULL;

	// Do not allow any siblings to handle this event even if we do not have a
	// tooltip
	if (blockMouseEvent(x, y) || show_names_text_box)
	{
		if (!tool_tip.empty())
		{
			msg = tool_tip;

			// Convert rect local to screen coordinates
			localPointToScreen(0, 0,
							   &(sticky_rect_screen->mLeft),
							   &(sticky_rect_screen->mBottom));
			localPointToScreen(mRect.getWidth(), mRect.getHeight(),
							   &(sticky_rect_screen->mRight),
							   &(sticky_rect_screen->mTop));
		}
		handled = true;
	}

	return handled;
}

bool LLView::handleKey(KEY key, MASK mask, bool called_from_parent)
{
	bool handled = false;

	if (getVisible() && getEnabled())
	{
		if (called_from_parent)
		{
			// Downward traversal
			handled = childrenHandleKey(key, mask) != NULL;
		}

		if (!handled)
		{
			handled = handleKeyHere(key, mask);
			if (handled && sDebugKeys)
			{
				llinfos << "Key handled by " << getName() << llendl;
			}
		}
	}

	if (!handled && !called_from_parent && mParentView)
	{
		// Upward traversal
		handled = mParentView->handleKey(key, mask, false);
	}

	return handled;
}

bool LLView::handleKeyUp(KEY key, MASK mask, bool called_from_parent)
{
	bool handled = false;

	if (getVisible() && getEnabled())
	{
		if (called_from_parent)
		{
			// Downward traversal
			handled = childrenHandleKeyUp(key, mask) != NULL;
		}

		if (!handled)
		{
			handled = handleKeyUpHere(key, mask);
			if (handled && sDebugKeys)
			{
				llinfos << "Key handled by " << getName() << llendl;
			}
		}
	}

	if (!handled && !called_from_parent && mParentView)
	{
		// Upward traversal
		handled = mParentView->handleKeyUp(key, mask, false);
	}

	return handled;
}

bool LLView::handleUnicodeChar(llwchar uni_char, bool called_from_parent)
{
	bool handled = false;

	if (getVisible() && getEnabled())
	{
		if (called_from_parent)
		{
			// Downward traversal
			handled = childrenHandleUnicodeChar(uni_char) != NULL;
		}

		if (!handled)
		{
			handled = handleUnicodeCharHere(uni_char);
			if (handled && sDebugKeys)
			{
				llinfos << "Unicode key handled by " << getName() << llendl;
			}
		}
	}

	if (!handled && !called_from_parent && mParentView)
	{
		// Upward traversal
		handled = mParentView->handleUnicodeChar(uni_char, false);
	}

	return handled;
}

bool LLView::handleDragAndDrop(S32 x, S32 y, MASK mask, bool drop,
							   EDragAndDropType cargo_type, void* cargo_data,
							   EAcceptance* accept,
							   std::string& tooltip_msg)
{
	// CRO this is an experiment to allow drag and drop into object inventory
	// based on the DragAndDrop tool's permissions rather than the parent
	bool handled = childrenHandleDragAndDrop(x, y, mask, drop,
											cargo_type, cargo_data, accept,
											tooltip_msg) != NULL;
	if (!handled && blockMouseEvent(x, y))
	{
		*accept = ACCEPT_NO;
		handled = true;
		LL_DEBUGS("UserInput") << "dragAndDrop handled by: " << getName()
							   << " - drop = " << (drop ? "true" : "false")
							   << " - accepted = false" << LL_ENDL;
	}

	return handled;
}

LLView* LLView::childrenHandleDragAndDrop(S32 x, S32 y, MASK mask,
									   bool drop,
									   EDragAndDropType cargo_type,
									   void* cargo_data,
									   EAcceptance* accept,
									   std::string& tooltip_msg)
{
	LLView* handled_view = NULL;
	// CRO this is an experiment to allow drag and drop into object inventory
	// based on the DragAndDrop tool's permissions rather than the parent
	if (getVisible()) //&& getEnabled())
	{
		for (child_list_iter_t child_it = mChildList.begin(),
							   end = mChildList.end();
			 child_it != end; ++child_it)
		{
			LLView* viewp = *child_it;
			if (!viewp) continue;	// Paranoia

			S32 local_x = x - viewp->getRect().mLeft;
			S32 local_y = y - viewp->getRect().mBottom;
			if (viewp->getVisible() && viewp->getEnabled() &&
				viewp->pointInView(local_x, local_y) &&
				viewp->handleDragAndDrop(local_x, local_y, mask, drop,
										 cargo_type,
										 cargo_data,
										 accept,
										 tooltip_msg))
			{
				handled_view = viewp;
				break;
			}
		}
	}
	return handled_view;
}

//virtual
bool LLView::hasMouseCapture()
{
	return gFocusMgr.getMouseCapture() == this;
}

bool LLView::handleMouseUp(S32 x, S32 y, MASK mask)
{
	bool handled = childrenHandleMouseUp(x, y, mask) != NULL;
	if (!handled && blockMouseEvent(x, y))
	{
		handled = true;
	}
	return handled;
}

bool LLView::handleMouseDown(S32 x, S32 y, MASK mask)
{
	LLView* handled_view = childrenHandleMouseDown(x, y, mask);
	bool handled = handled_view != NULL;
	if (!handled && blockMouseEvent(x, y))
	{
		handled = true;
		handled_view = this;
	}

	// *HACK: if we are editing the UI select the leaf view that ate the click.
	// Note: checks are sorted by "most probable" first to speed up things as
	// much as possible, even though it is not a speed-critical part.
	// *TODO: optimize by replacing these costly dynamic casts with a virtual
	// LLView method indicating that this control can grab clicks (and that we
	// do care about it for sEditingUIView), and override appropriately in each
	// control class.
	if (handled_view && sEditingUI &&
		(dynamic_cast<LLButton*>(handled_view) ||
		 dynamic_cast<LLLineEditor*>(handled_view) ||
		 dynamic_cast<LLTextEditor*>(handled_view) ||
		 dynamic_cast<LLScrollListCtrl*>(handled_view) ||
		 dynamic_cast<LLSlider*>(handled_view) ||
		 dynamic_cast<LLTextBox*>(handled_view) ||
		 dynamic_cast<LLVirtualTrackball*>(handled_view) ||
		 dynamic_cast<LLXYVector*>(handled_view) ||
		 dynamic_cast<LLMultiSlider*>(handled_view)))
	{
		sEditingUIView = handled_view;
	}

	return handled;
}

bool LLView::handleDoubleClick(S32 x, S32 y, MASK mask)
{
	bool handled = childrenHandleDoubleClick(x, y, mask) != NULL;
	if (!handled && blockMouseEvent(x, y))
	{
		handleMouseDown(x, y, mask);
		handled = true;
	}
	return handled;
}

bool LLView::handleScrollWheel(S32 x, S32 y, S32 clicks)
{
	bool handled = false;
	if (getVisible() && getEnabled())
	{
		handled = childrenHandleScrollWheel(x, y, clicks) != NULL;
		if (!handled && blockMouseEvent(x, y))
		{
			handled = true;
		}
	}
	return handled;
}

bool LLView::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
	bool handled = childrenHandleRightMouseDown(x, y, mask) != NULL;
	if (!handled && blockMouseEvent(x, y))
	{
		handled = true;
	}
	return handled;
}

bool LLView::handleRightMouseUp(S32 x, S32 y, MASK mask)
{
	bool handled = childrenHandleRightMouseUp(x, y, mask) != NULL;
	if (!handled && blockMouseEvent(x, y))
	{
		handled = true;
	}
	return handled;
}

bool LLView::handleMiddleMouseDown(S32 x, S32 y, MASK mask)
{
	LLView* handled_view = childrenHandleMiddleMouseDown(x, y, mask);
	bool handled = handled_view != NULL;
	if (!handled && blockMouseEvent(x, y))
	{
		handled = true;
		handled_view = this;
	}

	return handled;
}

bool LLView::handleMiddleMouseUp(S32 x, S32 y, MASK mask)
{
	bool handled = childrenHandleMiddleMouseUp(x, y, mask) != NULL;
	if (!handled && blockMouseEvent(x, y))
	{
		handled = true;
	}
	return handled;
}

LLView* LLView::childrenHandleScrollWheel(S32 x, S32 y, S32 clicks)
{
	LLView* handled_view = NULL;
	if (getVisible() && getEnabled())
	{
		for (child_list_iter_t child_it = mChildList.begin(),
							   end = mChildList.end();
			 child_it != end; ++child_it)
		{
			LLView* viewp = *child_it;
			if (!viewp) continue;	// Paranoia

			S32 local_x = x - viewp->getRect().mLeft;
			S32 local_y = y - viewp->getRect().mBottom;
			if (viewp->getVisible() && viewp->getEnabled() &&
				viewp->pointInView(local_x, local_y) &&
				viewp->handleScrollWheel(local_x, local_y, clicks))
			{
				if (sDebugMouseHandling)
				{
					sMouseHandlerMessage = "->" + viewp->mName +
										   sMouseHandlerMessage;
				}

				handled_view = viewp;
				break;
			}
		}
	}
	return handled_view;
}

LLView* LLView::childrenHandleHover(S32 x, S32 y, MASK mask)
{
	LLView* handled_view = NULL;
	if (getVisible() && getEnabled())
	{
		for (child_list_iter_t child_it = mChildList.begin(),
							   end = mChildList.end();
			 child_it != end; ++child_it)
		{
			LLView* viewp = *child_it;
			if (!viewp) continue;	// Paranoia

			S32 local_x = x - viewp->getRect().mLeft;
			S32 local_y = y - viewp->getRect().mBottom;
			if (viewp->getVisible() && viewp->getEnabled() &&
				viewp->pointInView(local_x, local_y) &&
				viewp->handleHover(local_x, local_y, mask))
			{
				if (sDebugMouseHandling)
				{
					sMouseHandlerMessage = "->" + viewp->mName +
										   sMouseHandlerMessage;
				}

				handled_view = viewp;
				break;
			}
		}
	}
	return handled_view;
}

// Called during downward traversal
LLView* LLView::childrenHandleKey(KEY key, MASK mask)
{
	LLView* handled_view = NULL;

	if (getVisible() && getEnabled())
	{
		for (child_list_iter_t child_it = mChildList.begin(),
							   end = mChildList.end();
			 child_it != end; ++child_it)
		{
			LLView* viewp = *child_it;
			if (viewp && viewp->handleKey(key, mask, true))
			{
				if (sDebugKeys)
				{
					llinfos << "Key handled by " << viewp->getName() << llendl;
				}
				handled_view = viewp;
				break;
			}
		}
	}

	return handled_view;
}

// Called during downward traversal
LLView* LLView::childrenHandleKeyUp(KEY key, MASK mask)
{
	LLView* handled_view = NULL;

	if (getVisible() && getEnabled())
	{
		for (child_list_iter_t child_it = mChildList.begin(),
							   end = mChildList.end();
			 child_it != end; ++child_it)
		{
			LLView* viewp = *child_it;
			if (viewp && viewp->handleKeyUp(key, mask, true))
			{
				if (sDebugKeys)
				{
					llinfos << "Key Up handled by " << viewp->getName()
							<< llendl;
				}
				handled_view = viewp;
				break;
			}
		}
	}

	return handled_view;
}

// Called during downward traversal
LLView* LLView::childrenHandleUnicodeChar(llwchar uni_char)
{
	LLView* handled_view = NULL;

	if (getVisible() && getEnabled())
	{
		for (child_list_iter_t child_it = mChildList.begin(),
							   end = mChildList.end();
			 child_it != end; ++child_it)
		{
			LLView* viewp = *child_it;
			if (viewp && viewp->handleUnicodeChar(uni_char, true))
			{
				if (sDebugKeys)
				{
					llinfos << "Unicode character handled by "
							<< viewp->getName() << llendl;
				}
				handled_view = viewp;
				break;
			}
		}
	}

	return handled_view;
}

LLView* LLView::childrenHandleMouseDown(S32 x, S32 y, MASK mask)
{
	LLView* handled_view = NULL;

	for (child_list_iter_t child_it = mChildList.begin(),
						   end = mChildList.end();
		 child_it != end; ++child_it)
	{
		LLView* viewp = *child_it;
		if (!viewp) continue;	// Paranoia

		S32 local_x = x - viewp->getRect().mLeft;
		S32 local_y = y - viewp->getRect().mBottom;

		if (viewp->getVisible() && viewp->getEnabled() &&
			viewp->pointInView(local_x, local_y) &&
			viewp->handleMouseDown(local_x, local_y, mask))
		{
			if (sDebugMouseHandling)
			{
				sMouseHandlerMessage = "->" + viewp->mName +
									   sMouseHandlerMessage;
			}
			handled_view = viewp;
			break;
		}
	}
	return handled_view;
}

LLView* LLView::childrenHandleRightMouseDown(S32 x, S32 y, MASK mask)
{
	LLView* handled_view = NULL;

	if (getVisible() && getEnabled())
	{
		for (child_list_iter_t child_it = mChildList.begin(),
							   end = mChildList.end();
			 child_it != end; ++child_it)
		{
			LLView* viewp = *child_it;
			if (!viewp) continue;	// Paranoia

			S32 local_x = x - viewp->getRect().mLeft;
			S32 local_y = y - viewp->getRect().mBottom;
			if (viewp->getVisible() && viewp->getEnabled() &&
				viewp->pointInView(local_x, local_y) &&
				viewp->handleRightMouseDown(local_x, local_y, mask))
			{
				if (sDebugMouseHandling)
				{
					sMouseHandlerMessage = "->" + viewp->mName +
										   sMouseHandlerMessage;
				}
				handled_view = viewp;
				break;
			}
		}
	}
	return handled_view;
}

LLView* LLView::childrenHandleMiddleMouseDown(S32 x, S32 y, MASK mask)
{
	LLView* handled_view = NULL;

	if (getVisible() && getEnabled())
	{
		for (child_list_iter_t child_it = mChildList.begin(),
							   end = mChildList.end();
			 child_it != end; ++child_it)
		{
			LLView* viewp = *child_it;
			if (!viewp) continue;	// Paranoia

			S32 local_x = x - viewp->getRect().mLeft;
			S32 local_y = y - viewp->getRect().mBottom;
			if (viewp->getVisible() && viewp->getEnabled() &&
				viewp->pointInView(local_x, local_y) &&
				viewp->handleMiddleMouseDown(local_x, local_y, mask))
			{
				if (sDebugMouseHandling)
				{
					sMouseHandlerMessage = "->" + viewp->mName +
										   sMouseHandlerMessage;
				}
				handled_view = viewp;
				break;
			}
		}
	}
	return handled_view;
}

LLView* LLView::childrenHandleDoubleClick(S32 x, S32 y, MASK mask)
{
	LLView* handled_view = NULL;

	if (getVisible() && getEnabled())
	{
		for (child_list_iter_t child_it = mChildList.begin(),
							   end = mChildList.end();
			 child_it != end; ++child_it)
		{
			LLView* viewp = *child_it;
			if (!viewp) continue;	// Paranoia

			S32 local_x = x - viewp->getRect().mLeft;
			S32 local_y = y - viewp->getRect().mBottom;
			if (viewp->getVisible() && viewp->getEnabled() &&
				viewp->pointInView(local_x, local_y) &&
				viewp->handleDoubleClick(local_x, local_y, mask))
			{
				if (sDebugMouseHandling)
				{
					sMouseHandlerMessage = "->" + viewp->mName +
										   sMouseHandlerMessage;
				}
				handled_view = viewp;
				break;
			}
		}
	}
	return handled_view;
}

LLView* LLView::childrenHandleMouseUp(S32 x, S32 y, MASK mask)
{
	LLView* handled_view = NULL;
	if (getVisible() && getEnabled())
	{
		for (child_list_iter_t child_it = mChildList.begin(),
							   end = mChildList.end();
			 child_it != end; ++child_it)
		{
			LLView* viewp = *child_it;
			if (!viewp) continue;	// Paranoia

			S32 local_x = x - viewp->getRect().mLeft;
			S32 local_y = y - viewp->getRect().mBottom;
			if (viewp->getVisible() && viewp->getEnabled() &&
				viewp->pointInView(local_x, local_y) &&
				viewp->handleMouseUp(local_x, local_y, mask))
			{
				if (sDebugMouseHandling)
				{
					sMouseHandlerMessage = "->" + viewp->mName +
										   sMouseHandlerMessage;
				}
				handled_view = viewp;
				break;
			}
		}
	}
	return handled_view;
}

LLView* LLView::childrenHandleRightMouseUp(S32 x, S32 y, MASK mask)
{
	LLView* handled_view = NULL;
	if (getVisible() && getEnabled())
	{
		for (child_list_iter_t child_it = mChildList.begin(),
							   end = mChildList.end();
			 child_it != end; ++child_it)
		{
			LLView* viewp = *child_it;
			if (!viewp) continue;	// Paranoia

			S32 local_x = x - viewp->getRect().mLeft;
			S32 local_y = y - viewp->getRect().mBottom;
			if (viewp->getVisible() && viewp->getEnabled() &&
				viewp->pointInView(local_x, local_y) &&
				viewp->handleRightMouseUp(local_x, local_y, mask))
			{
				if (sDebugMouseHandling)
				{
					sMouseHandlerMessage = "->" + viewp->mName +
										   sMouseHandlerMessage;
				}
				handled_view = viewp;
				break;
			}
		}
	}
	return handled_view;
}

LLView* LLView::childrenHandleMiddleMouseUp(S32 x, S32 y, MASK mask)
{
	LLView* handled_view = NULL;
	if (getVisible() && getEnabled())
	{
		for (child_list_iter_t child_it = mChildList.begin(),
							   end = mChildList.end();
			 child_it != end; ++child_it)
		{
			LLView* viewp = *child_it;
			if (!viewp) continue;	// Paranoia

			S32 local_x = x - viewp->getRect().mLeft;
			S32 local_y = y - viewp->getRect().mBottom;
			if (viewp->getVisible() && viewp->getEnabled() &&
				viewp->pointInView(local_x, local_y) &&
				viewp->handleMiddleMouseUp(local_x, local_y, mask))
			{
				if (sDebugMouseHandling)
				{
					sMouseHandlerMessage = "->" + viewp->mName +
										   sMouseHandlerMessage;
				}
				handled_view = viewp;
				break;
			}
		}
	}
	return handled_view;
}

void LLView::draw()
{
	if (sDebugRects)
	{
		drawDebugRect();

		// Check for bogus rectangle
		if (getRect().mRight <= getRect().mLeft ||
			getRect().mTop <= getRect().mBottom)
		{
			llwarns << "Bogus rectangle for " << getName() << " with " << mRect
					<< llendl;
		}
	}

	// Draw focused control on top of everything else
	LLUICtrl* focus_view = gFocusMgr.getKeyboardFocusUICtrl();
	if (focus_view && focus_view->getParent() != this)
	{
		focus_view = NULL;
	}

	LLRect root_rect = getRootView()->getRect();
	LLRect screen_rect;
	++sDepth;
	for (child_list_reverse_iter_t child_iter = mChildList.rbegin();
		 child_iter != mChildList.rend(); ++child_iter)
	{
		LLView* viewp = *child_iter;
		if (viewp && viewp->getVisible() &&
			viewp != focus_view && viewp->getRect().isValid())
		{
			// Only draw views that are within the root view
			localRectToScreen(viewp->getRect(), &screen_rect);
			if (root_rect.overlaps(screen_rect))
			{
				gGL.matrixMode(LLRender::MM_MODELVIEW);
				LLUI::pushMatrix();
				{
					LLUI::translate((F32)viewp->getRect().mLeft,
									(F32)viewp->getRect().mBottom, 0.f);
					viewp->draw();
				}
				LLUI::popMatrix();
			}
		}

	}
	--sDepth;

	if (focus_view && focus_view->getVisible())
	{
		drawChild(focus_view);
	}

	// HACK
	if (sEditingUI && this == sEditingUIView)
	{
		drawDebugRect();
	}
}

// Draw a box for debugging.
void LLView::drawDebugRect()
{
	LLUI::pushMatrix();
	{
		// Drawing solids requires texturing be disabled
		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

		if (mUseBoundingRect)
		{
			LLUI::translate((F32)mBoundingRect.mLeft - (F32)mRect.mLeft,
							(F32)mBoundingRect.mBottom - (F32)mRect.mBottom,
							0.f);
		}

		LLRect debug_rect = mUseBoundingRect ? mBoundingRect : mRect;

		// draw red rectangle for the border
		LLColor4 border_color(0.f, 0.f, 0.f, 1.f);
		if (sEditingUI)
		{
			border_color.mV[0] = 1.f;
		}
		else
		{
			border_color.mV[sDepth%3] = 1.f;
		}

		gGL.color4fv(border_color.mV);

		gGL.begin(LLRender::LINES);
			gGL.vertex2i(0, debug_rect.getHeight() - 1);
			gGL.vertex2i(0, 0);

			gGL.vertex2i(0, 0);
			gGL.vertex2i(debug_rect.getWidth() - 1, 0);

			gGL.vertex2i(debug_rect.getWidth() - 1, 0);
			gGL.vertex2i(debug_rect.getWidth() - 1,
						 debug_rect.getHeight() - 1);

			gGL.vertex2i(debug_rect.getWidth() - 1,
						 debug_rect.getHeight() - 1);
			gGL.vertex2i(0, debug_rect.getHeight() - 1);
		gGL.end();

		// Draw the name if it is not a leaf node
		if (mChildListSize && !sEditingUI)
		{
			static LLFontGL* fontp = LLFontGL::getFontSansSerifSmall();
			S32 x, y;
			gGL.color4fv(border_color.mV);
			x = debug_rect.getWidth()/2;
			y = debug_rect.getHeight()/2;
			std::string debug_text = llformat("%s (%d x %d)",
											  getName().c_str(),
											  debug_rect.getWidth(),
											  debug_rect.getHeight());
			fontp->renderUTF8(debug_text, 0, (F32)x, (F32)y, border_color,
							  LLFontGL::HCENTER, LLFontGL::BASELINE,
							  LLFontGL::NORMAL, S32_MAX, S32_MAX, NULL, false);
		}
	}
	LLUI::popMatrix();
}

void LLView::drawChild(LLView* childp, S32 x_offset, S32 y_offset,
					   bool force_draw)
{
	if (childp && childp->getParent() == this)
	{
		++sDepth;

		if (force_draw ||
			(childp->getVisible() && childp->getRect().isValid()))
		{
			gGL.matrixMode(LLRender::MM_MODELVIEW);
			LLUI::pushMatrix();
			{
				LLUI::translate((F32)childp->getRect().mLeft + x_offset,
								(F32)childp->getRect().mBottom + y_offset,
								0.f);
				childp->draw();
			}
			LLUI::popMatrix();
		}

		--sDepth;
	}
}

void LLView::reshape(S32 width, S32 height, bool called_from_parent)
{
	// Compute how much things changed and apply reshape logic to children
	S32 delta_width = width - getRect().getWidth();
	S32 delta_height = height - getRect().getHeight();

	if (delta_width || delta_height || sForceReshape)
	{
		// Adjust our rectangle
		mRect.mRight = getRect().mLeft + width;
		mRect.mTop = getRect().mBottom + height;

		// Move child views according to reshape flags
		for (child_list_iter_t child_it = mChildList.begin(),
									 end = mChildList.end();
			 child_it != end; ++child_it)
		{
			LLView* viewp = *child_it;
			if (!viewp) continue;	// Paranoia

			LLRect child_rect(viewp->mRect);

			if (viewp->followsRight() && viewp->followsLeft())
			{
				child_rect.mRight += delta_width;
			}
			else if (viewp->followsRight())
			{
				child_rect.mLeft += delta_width;
				child_rect.mRight += delta_width;
			}
#if 0
			else if (viewp->followsLeft())
			{
				// Left is 0, do not need to adjust coords
			}
			else
			{
				// *FIXME: what to do when we do not follow anyone ?
				// For now, same as followsLeft
			}
#endif

			if (viewp->followsTop() && viewp->followsBottom())
			{
				child_rect.mTop += delta_height;
			}
			else if (viewp->followsTop())
			{
				child_rect.mTop += delta_height;
				child_rect.mBottom += delta_height;
			}
#if 0
			else if (viewp->followsBottom())
			{
				// Bottom is 0, so do not need to adjust coords
			}
			else
			{
				// *FIXME what to do when we do not follow ?
				// For now, same as bottom
			}
#endif

			S32 delta_x = child_rect.mLeft - viewp->getRect().mLeft;
			S32 delta_y = child_rect.mBottom - viewp->getRect().mBottom;
			viewp->translate(delta_x, delta_y);
			viewp->reshape(child_rect.getWidth(), child_rect.getHeight());
		}
	}

	if (!called_from_parent && mParentView)
	{
		mParentView->reshape(mParentView->getRect().getWidth(),
							 mParentView->getRect().getHeight(), false);
	}

	updateBoundingRect();
}

void LLView::updateBoundingRect()
{
	if (isDead()) return;

	if (mUseBoundingRect)
	{
		LLRect local_bounding_rect = LLRect::null;

		child_list_const_iter_t child_it;
		for (child_list_iter_t child_it = mChildList.begin(),
							   end = mChildList.end();
			 child_it != end; ++child_it)
		{
			LLView* childp = *child_it;
			// Ignore invisible and "top" children when calculating bounding
			// rect such as combobox popups
			if (!childp || !childp->getVisible() ||
				childp == gFocusMgr.getTopCtrl())
			{
				continue;
			}

			LLRect child_bounding_rect = childp->getBoundingRect();

			if (local_bounding_rect.isEmpty())
			{
				// Start out with bounding rect equal to first visible child's
				// bounding rect
				local_bounding_rect = child_bounding_rect;
			}
			else
			{
				// Accumulate non-null children rectangles
				if (!child_bounding_rect.isEmpty())
				{
					local_bounding_rect.unionWith(child_bounding_rect);
				}
			}
		}

		mBoundingRect = local_bounding_rect;
		// Translate into parent-relative coordinates
		mBoundingRect.translate(mRect.mLeft, mRect.mBottom);
	}
	else
	{
		mBoundingRect = mRect;
	}

	// Give parent view a chance to resize, in case we just moved, for example
	if (getParent() && getParent()->mUseBoundingRect)
	{
		getParent()->updateBoundingRect();
	}
}

LLRect LLView::getScreenRect() const
{
	// *FIX: check for one-off error
	LLRect screen_rect;
	localPointToScreen(0, 0, &screen_rect.mLeft, &screen_rect.mBottom);
	localPointToScreen(getRect().getWidth(), getRect().getHeight(),
					   &screen_rect.mRight, &screen_rect.mTop);
	return screen_rect;
}

LLRect LLView::getLocalBoundingRect() const
{
	LLRect local_bounding_rect = getBoundingRect();
	local_bounding_rect.translate(-mRect.mLeft, -mRect.mBottom);
	return local_bounding_rect;
}

LLRect LLView::getLocalRect() const
{
	return LLRect(0, getRect().getHeight(), getRect().getWidth(), 0);
}

LLRect LLView::getLocalSnapRect() const
{
	LLRect local_snap_rect = getSnapRect();
	local_snap_rect.translate(-getRect().mLeft, -getRect().mBottom);
	return local_snap_rect;
}

bool LLView::hasAncestor(const LLView* parentp) const
{
	if (!parentp)
	{
		return false;
	}

	LLView* viewp = getParent();
	while (viewp)
	{
		if (viewp == parentp)
		{
			return true;
		}
		viewp = viewp->getParent();
	}

	return false;
}

bool LLView::childHasKeyboardFocus(const char* childname) const
{
	LLView* childp = getChildView(childname, true, false);
	return childp && gFocusMgr.childHasKeyboardFocus(childp);
}

LLView* LLView::getChildView(const char* name, bool recurse,
							 bool create_if_missing) const
{
#if 0	// Should we forbid empty names ?
	if (name.empty()) return NULL;
#endif
	LL_DEBUGS("GetChildCalls") << "Requested child name: " << name << LL_ENDL;

	// Look for direct children *first*
	for (child_list_const_iter_t child_it = mChildList.begin(),
								  end = mChildList.end();
		 child_it != end; ++child_it)
	{
		LLView* childp = *child_it;
		if (childp && strcmp(childp->getName().c_str(), name) == 0)
		{
			return childp;
		}
	}
	if (recurse)
	{
		// Look inside each child as well.
		for (child_list_const_iter_t child_it = mChildList.begin(),
									 end = mChildList.end();
			 child_it != end; ++child_it)
		{
			LLView* childp = *child_it;
			if (!childp) continue;	// Paranoia

			LLView* viewp = childp->getChildView(name, recurse, false);
			if (viewp)
			{
				return viewp;
			}
		}
	}

	if (create_if_missing)
	{
		return createDummyWidget<LLView>(name);
	}
	return NULL;
}

bool LLView::parentPointInView(S32 x, S32 y, EHitTestType type) const
{
	return (mUseBoundingRect && type == HIT_TEST_USE_BOUNDING_RECT
				? mBoundingRect.pointInRect(x, y)
				: mRect.pointInRect(x, y));
}

bool LLView::pointInView(S32 x, S32 y, EHitTestType type) const
{
	return (mUseBoundingRect && type == HIT_TEST_USE_BOUNDING_RECT
				? mBoundingRect.pointInRect(x + mRect.mLeft, y + mRect.mBottom)
				: mRect.localPointInRect(x, y));
}

bool LLView::blockMouseEvent(S32 x, S32 y) const
{
	return mMouseOpaque && pointInView(x, y, HIT_TEST_IGNORE_BOUNDING_RECT);
}

//virtual
void LLView::screenPointToLocal(S32 screen_x, S32 screen_y, S32* local_x,
								S32* local_y) const
{
	*local_x = screen_x;
	*local_y = screen_y;

	const LLView* cur = this;
	LLRect cur_rect;
	do
	{
		cur_rect = cur->getRect();
		*local_x -= cur_rect.mLeft;
		*local_y -= cur_rect.mBottom;
		cur = cur->mParentView;
	}
	while (cur);
}

//virtual
void LLView::localPointToScreen(S32 local_x, S32 local_y, S32* screen_x,
								S32* screen_y) const
{
	*screen_x = local_x;
	*screen_y = local_y;

	const LLView* cur = this;
	LLRect cur_rect;
	do
	{
		cur_rect = cur->getRect();
		*screen_x += cur_rect.mLeft;
		*screen_y += cur_rect.mBottom;
		cur = cur->mParentView;
	}
	while (cur);
}

void LLView::screenRectToLocal(const LLRect& screen, LLRect* local) const
{
	*local = screen;
	local->translate(-getRect().mLeft, -getRect().mBottom);

	const LLView* cur = this;
	while (cur->mParentView)
	{
		cur = cur->mParentView;
		local->translate(-cur->getRect().mLeft, -cur->getRect().mBottom);
	}
}

void LLView::localRectToScreen(const LLRect& local, LLRect* screen) const
{
	*screen = local;
	screen->translate(getRect().mLeft, getRect().mBottom);

	const LLView* cur = this;
	while (cur->mParentView)
	{
		cur = cur->mParentView;
		screen->translate(cur->getRect().mLeft, cur->getRect().mBottom);
	}
}

LLView* LLView::getRootView()
{
	static LLView* sRootView = NULL;
	if (!sRootView)
	{
		sRootView = this;
		while (sRootView->mParentView)
		{
			sRootView = sRootView->mParentView;
		}
	}
	return sRootView;
}

LLView* LLView::findPrevSibling(LLView* child)
{
	child_list_t::iterator prev_it = std::find(mChildList.begin(),
											   mChildList.end(), child);
	if (prev_it != mChildList.end() && prev_it != mChildList.begin())
	{
		return *(--prev_it);
	}
	return NULL;
}

LLView* LLView::findNextSibling(LLView* child)
{
	child_list_t::iterator next_it = std::find(mChildList.begin(),
											   mChildList.end(), child);
	if (next_it != mChildList.end())
	{
		++next_it;
	}

	return next_it != mChildList.end() ? *next_it : NULL;
}

bool LLView::deleteViewByHandle(LLHandle<LLView> handle)
{
	LLView* viewp = handle.get();
	if (viewp)
	{
		delete viewp;
		return true;
	}
	return false;
}

// Moves the view so that it is entirely inside of constraint.
// If the view will not fit because it is too big, aligns with the top and
// left.
bool LLView::translateIntoRect(const LLRect& constraint,
							   bool allow_partial_outside)
{
	S32 delta_x = 0;
	S32 delta_y = 0;

	if (allow_partial_outside)
	{
		constexpr S32 KEEP_ONSCREEN_PIXELS = 16;

		if (getRect().mRight - KEEP_ONSCREEN_PIXELS < constraint.mLeft)
		{
			delta_x = constraint.mLeft - (getRect().mRight - KEEP_ONSCREEN_PIXELS);
		}
		else if (getRect().mLeft + KEEP_ONSCREEN_PIXELS > constraint.mRight)
		{
			delta_x = constraint.mRight - (getRect().mLeft + KEEP_ONSCREEN_PIXELS);
		}

		if (getRect().mTop > constraint.mTop)
		{
			delta_y = constraint.mTop - getRect().mTop;
		}
		else if (getRect().mTop - KEEP_ONSCREEN_PIXELS < constraint.mBottom)
		{
			delta_y = constraint.mBottom - (getRect().mTop - KEEP_ONSCREEN_PIXELS);
		}
	}
	else
	{
		if (getRect().mLeft < constraint.mLeft)
		{
			delta_x = constraint.mLeft - getRect().mLeft;
		}
		else if (getRect().mRight > constraint.mRight)
		{
			delta_x = constraint.mRight - getRect().mRight;
			// compensate for left edge possible going off screen
			delta_x += llmax(0, getRect().getWidth() - constraint.getWidth());
		}

		if (getRect().mTop > constraint.mTop)
		{
			delta_y = constraint.mTop - getRect().mTop;
		}
		else if (getRect().mBottom < constraint.mBottom)
		{
			delta_y = constraint.mBottom - getRect().mBottom;
			// compensate for top edge possible going off screen
			delta_y -= llmax(0, getRect().getHeight() - constraint.getHeight());
		}
	}

	if (delta_x != 0 || delta_y != 0)
	{
		translate(delta_x, delta_y);
		return true;
	}
	return false;
}

void LLView::centerWithin(const LLRect& bounds)
{
	S32 left   = bounds.mLeft + (bounds.getWidth() - getRect().getWidth()) / 2;
	S32 bottom = bounds.mBottom + (bounds.getHeight() - getRect().getHeight()) / 2;

	translate(left - getRect().mLeft, bottom - getRect().mBottom);
}

bool LLView::localPointToOtherView(S32 x, S32 y, S32* other_x, S32* other_y,
								   LLView* other_view) const
{
	const LLView* cur_view = this;
	const LLView* root_view = NULL;

	while (cur_view)
	{
		if (cur_view == other_view)
		{
			*other_x = x;
			*other_y = y;
			return true;
		}

		x += cur_view->getRect().mLeft;
		y += cur_view->getRect().mBottom;

		cur_view = cur_view->getParent();
		root_view = cur_view;
	}

	// assuming common root between two views, chase other_view's parents up to
	// root
	cur_view = other_view;
	while (cur_view)
	{
		x -= cur_view->getRect().mLeft;
		y -= cur_view->getRect().mBottom;

		cur_view = cur_view->getParent();

		if (cur_view == root_view)
		{
			*other_x = x;
			*other_y = y;
			return true;
		}
	}

	*other_x = x;
	*other_y = y;
	return false;
}

bool LLView::localRectToOtherView(const LLRect& local, LLRect* other,
								  LLView* other_view) const
{
	LLRect cur_rect = local;
	const LLView* cur_view = this;
	const LLView* root_view = NULL;

	while (cur_view)
	{
		if (cur_view == other_view)
		{
			*other = cur_rect;
			return true;
		}

		cur_rect.translate(cur_view->getRect().mLeft,
						   cur_view->getRect().mBottom);

		cur_view = cur_view->getParent();
		root_view = cur_view;
	}

	// assuming common root between two views, chase other_view's parents up to
	// root
	cur_view = other_view;
	while (cur_view)
	{
		cur_rect.translate(-cur_view->getRect().mLeft,
						   -cur_view->getRect().mBottom);

		cur_view = cur_view->getParent();

		if (cur_view == root_view)
		{
			*other = cur_rect;
			return true;
		}
	}

	*other = cur_rect;
	return false;
}

//virtual
LLXMLNodePtr LLView::getXML(bool save_children) const
{
	// If called from a derived class, the derived class will override the node
	// name
	LLXMLNodePtr node = new LLXMLNode("view", false);

	node->createChild("name", true)->setStringValue(getName());
	node->createChild("width", true)->setIntValue(getRect().getWidth());
	node->createChild("height", true)->setIntValue(getRect().getHeight());

	LLView* parent = getParent();
	S32 left = getRect().mLeft;
	S32 bottom = getRect().mBottom;
	if (parent) bottom -= parent->getRect().getHeight();

	node->createChild("left", true)->setIntValue(left);
	node->createChild("bottom", true)->setIntValue(bottom);

	U8 follows_flags = getFollows();
	if (follows_flags)
	{
		std::stringstream buffer;
		bool pipe = false;
		if (followsLeft())
		{
			buffer << "left";
			pipe = true;
		}
		if (followsTop())
		{
			if (pipe) buffer << "|";
			buffer << "top";
			pipe = true;
		}
		if (followsRight())
		{
			if (pipe) buffer << "|";
			buffer << "right";
			pipe = true;
		}
		if (followsBottom())
		{
			if (pipe) buffer << "|";
			buffer << "bottom";
		}
		node->createChild("follows", true)->setStringValue(buffer.str());
	}
	// Export all widgets as enabled and visible - code must disable.
	node->createChild("mouse_opaque", true)->setBoolValue(mMouseOpaque);
	if (mToolTipMsgPtr && !mToolTipMsgPtr->getString().empty())
	{
		node->createChild("tool_tip", true)->setStringValue(mToolTipMsgPtr->getString());
	}
	if (mSoundFlags != MOUSE_UP)
	{
		node->createChild("sound_flags", true)->setIntValue((S32)mSoundFlags);
	}

	node->createChild("enabled", true)->setBoolValue(getEnabled());

	if (!mControlName.empty())
	{
		node->createChild("control_name", true)->setStringValue(mControlName);
	}
	return node;
}

//static
LLView* LLView::fromXML(LLXMLNodePtr node, LLView* parent,
						LLUICtrlFactory* factory)
{
	LLView* viewp = new LLView();
	viewp->initFromXML(node, parent);
	return viewp;
}

//static
void LLView::addColorXML(LLXMLNodePtr node, const LLColor4& color,
						 const char* xml_name, const char* control_name)
{
	if (color != LLUI::sColorsGroup->getColor(ll_safe_string(control_name).c_str()))
	{
		node->createChild(xml_name, true)->setFloatValue(4, color.mV);
	}
}

//static
std::string LLView::escapeXML(const std::string& xml, std::string& indent)
{
	std::string ret = indent + "\"" + LLXMLNode::escapeXML(xml);

	// Replace every newline with a close quote, new line, indent, open quote
	size_t index = ret.size()-1;
	size_t fnd;

	while ((fnd = ret.rfind("\n", index)) != std::string::npos)
	{
		ret.replace(fnd, 1, "\"\n" + indent + "\"");
		index = fnd-1;
	}

	// Append close quote
	ret.append("\"");

	return ret;
}

//static
LLWString LLView::escapeXML(const LLWString& xml)
{
	LLWString out;
	for (LLWString::size_type i = 0; i < xml.size(); ++i)
	{
		llwchar c = xml[i];
		switch (c)
		{
			case '"':	out.append(utf8str_to_wstring("&quot;"));	break;
			case '\'':	out.append(utf8str_to_wstring("&apos;"));	break;
			case '&':	out.append(utf8str_to_wstring("&amp;"));	break;
			case '<':	out.append(utf8str_to_wstring("&lt;"));		break;
			case '>':	out.append(utf8str_to_wstring("&gt;"));		break;
			default:	out.push_back(c); break;
		}
	}
	return out;
}

//static
const LLCtrlQuery& LLView::getTabOrderQuery()
{
	static LLCtrlQuery query;
	if (query.getPreFilters().size() == 0)
	{
		query.addPreFilter(LLVisibleFilter::getInstance());
		query.addPreFilter(LLEnabledFilter::getInstance());
		query.addPreFilter(LLTabStopFilter::getInstance());
		query.addPostFilter(LLLeavesFilter::getInstance());
	}
	return query;
}

// This class is only used internally by getFocusRootsQuery below.
class LLFocusRootsFilter : public LLQueryFilter, public LLSingleton<LLFocusRootsFilter>
{
	friend class LLSingleton<LLFocusRootsFilter>;

	filter_result_t operator()(const LLView* const view,
							   const view_list_t& children) const override
	{
		return filter_result_t(view->isCtrl() && view->isFocusRoot(),
							   !view->isFocusRoot());
	}
};

//static
const LLCtrlQuery& LLView::getFocusRootsQuery()
{
	static LLCtrlQuery query;
	if (query.getPreFilters().size() == 0)
	{
		query.addPreFilter(LLVisibleFilter::getInstance());
		query.addPreFilter(LLEnabledFilter::getInstance());
		query.addPreFilter(LLFocusRootsFilter::getInstance());
		query.addPostFilter(LLRootsFilter::getInstance());
	}
	return query;
}

void LLView::userSetShape(const LLRect& new_rect)
{
	reshape(new_rect.getWidth(), new_rect.getHeight());
	translate(new_rect.mLeft - getRect().mLeft,
			  new_rect.mBottom - getRect().mBottom);
}

LLView* LLView::findSnapRect(LLRect& new_rect, const LLCoordGL& mouse_dir,
							 LLView::ESnapType snap_type, S32 threshold,
							 S32 padding)
{
	new_rect = mRect;
	LLView* snap_view = NULL;

	if (!mParentView)
	{
		return NULL;
	}

	S32 delta_x = 0;
	S32 delta_y = 0;
	if (mouse_dir.mX >= 0)
	{
		S32 new_right = mRect.mRight;
		LLView* view = findSnapEdge(new_right, mouse_dir, SNAP_RIGHT,
									snap_type, threshold, padding);
		delta_x = new_right - mRect.mRight;
		snap_view = view ? view : snap_view;
	}

	if (mouse_dir.mX <= 0)
	{
		S32 new_left = mRect.mLeft;
		LLView* view = findSnapEdge(new_left, mouse_dir, SNAP_LEFT,
									snap_type, threshold, padding);
		delta_x = new_left - mRect.mLeft;
		snap_view = view ? view : snap_view;
	}

	if (mouse_dir.mY >= 0)
	{
		S32 new_top = mRect.mTop;
		LLView* view = findSnapEdge(new_top, mouse_dir, SNAP_TOP,
									snap_type, threshold, padding);
		delta_y = new_top - mRect.mTop;
		snap_view = view ? view : snap_view;
	}

	if (mouse_dir.mY <= 0)
	{
		S32 new_bottom = mRect.mBottom;
		LLView* view = findSnapEdge(new_bottom, mouse_dir, SNAP_BOTTOM,
									snap_type, threshold, padding);
		delta_y = new_bottom - mRect.mBottom;
		snap_view = view ? view : snap_view;
	}

	new_rect.translate(delta_x, delta_y);
	return snap_view;
}

LLView*	LLView::findSnapEdge(S32& new_edge_val, const LLCoordGL& mouse_dir,
							 ESnapEdge snap_edge, ESnapType snap_type,
							 S32 threshold, S32 padding)
{
	LLRect snap_rect = getSnapRect();
	S32 snap_pos = 0;
	switch (snap_edge)
	{
		case SNAP_LEFT:
			snap_pos = snap_rect.mLeft;
			break;

		case SNAP_RIGHT:
			snap_pos = snap_rect.mRight;
			break;

		case SNAP_TOP:
			snap_pos = snap_rect.mTop;
			break;

		case SNAP_BOTTOM:
			snap_pos = snap_rect.mBottom;
		}

	if (!mParentView)
	{
		new_edge_val = snap_pos;
		return NULL;
	}

	LLView* snap_view = NULL;

	// If the view is near the edge of its parent, snap it to the edge.
	LLRect test_rect = snap_rect;
	test_rect.stretch(padding);

	S32 x_threshold = threshold;
	S32 y_threshold = threshold;

	LLRect parent_local_snap_rect = mParentView->getLocalSnapRect();

	if (snap_type == SNAP_PARENT || snap_type == SNAP_PARENT_AND_SIBLINGS)
	{
		switch (snap_edge)
		{
			case SNAP_RIGHT:
			{
				if ((parent_local_snap_rect.mRight - test_rect.mRight) *
					mouse_dir.mX >= 0)
				{
					S32 delta = abs(parent_local_snap_rect.mRight -
									test_rect.mRight);
					if (delta <= x_threshold)
					{
						snap_pos = parent_local_snap_rect.mRight - padding;
						snap_view = mParentView;
						x_threshold = delta;
					}
				}
				break;
			}

			case SNAP_LEFT:
			{
				if (test_rect.mLeft * mouse_dir.mX <= 0)
				{
					S32 delta = abs(test_rect.mLeft -
									parent_local_snap_rect.mLeft);
					if (delta <= x_threshold)
					{
						snap_pos = parent_local_snap_rect.mLeft + padding;
						snap_view = mParentView;
						x_threshold = delta;
					}
				}
				break;
			}

			case SNAP_BOTTOM:
			{
				if (test_rect.mBottom * mouse_dir.mY <= 0)
				{
					S32 delta = abs(test_rect.mBottom -
									parent_local_snap_rect.mBottom);
					if (delta <= y_threshold)
					{
						snap_pos = parent_local_snap_rect.mBottom + padding;
						snap_view = mParentView;
						y_threshold = delta;
					}
				}
				break;
			}

			case SNAP_TOP:
			{
				if ((parent_local_snap_rect.mTop - test_rect.mTop) *
					mouse_dir.mY >= 0)
				{
					S32 delta = abs(parent_local_snap_rect.mTop -
									test_rect.mTop);
					if (delta <= y_threshold)
					{
						snap_pos = parent_local_snap_rect.mTop - padding;
						snap_view = mParentView;
						y_threshold = delta;
					}
				}
				break;
			}

			default:
			{
				llerrs << "Invalid snap edge" << llendl;
			}
		}
	}

	if (snap_type == SNAP_SIBLINGS || snap_type == SNAP_PARENT_AND_SIBLINGS)
	{
		for (child_list_const_iter_t it = mParentView->getChildList()->begin(),
									 end = mParentView->getChildList()->end();
			 it != end; ++it)
		{
			LLView* siblingp = *it;
			if (!siblingp || !canSnapTo(siblingp)) continue;

			LLRect sibling_rect = siblingp->getSnapRect();

			switch (snap_edge)
			{
				case SNAP_RIGHT:
				{
					S32 delta = abs(test_rect.mRight - sibling_rect.mLeft);
					if (delta <= x_threshold &&
						(test_rect.mRight - sibling_rect.mLeft) *
						mouse_dir.mX <= 0)
					{
						snap_pos = sibling_rect.mLeft - padding;
						snap_view = siblingp;
						x_threshold = delta;
					}
					// If snapped with sibling along other axis, check for
					// shared edge
					else if (abs(sibling_rect.mTop - test_rect.mBottom +
								 padding) <= y_threshold ||
							 abs(sibling_rect.mBottom - test_rect.mTop -
								 padding) <= x_threshold)
					{
						delta = abs(test_rect.mRight - sibling_rect.mRight);
						if (delta <= x_threshold &&
							(test_rect.mRight - sibling_rect.mRight) *
							mouse_dir.mX <= 0)
						{
							snap_pos = sibling_rect.mRight;
							snap_view = siblingp;
							x_threshold = delta;
						}
					}
					break;
				}

				case SNAP_LEFT:
				{
					S32 delta = abs(test_rect.mLeft - sibling_rect.mRight);
					if (delta <= x_threshold &&
						(test_rect.mLeft - sibling_rect.mRight) *
						mouse_dir.mX <= 0)
					{
						snap_pos = sibling_rect.mRight + padding;
						snap_view = siblingp;
						x_threshold = delta;
					}
					// If snapped with sibling along other axis, check for
					// shared edge
					else if (abs(sibling_rect.mTop - test_rect.mBottom +
								 padding) <= y_threshold ||
							 abs(sibling_rect.mBottom - test_rect.mTop -
							 	 padding) <= y_threshold)
					{
						delta = abs(test_rect.mLeft - sibling_rect.mLeft);
						if (delta <= x_threshold &&
							(test_rect.mLeft - sibling_rect.mLeft) *
							mouse_dir.mX <= 0)
						{
							snap_pos = sibling_rect.mLeft;
							snap_view = siblingp;
							x_threshold = delta;
						}
					}
					break;
				}

				case SNAP_BOTTOM:
				{
					S32 delta = abs(test_rect.mBottom - sibling_rect.mTop);
					if (delta <= y_threshold &&
						(test_rect.mBottom - sibling_rect.mTop) *
						mouse_dir.mY <= 0)
					{
						snap_pos = sibling_rect.mTop + padding;
						snap_view = siblingp;
						y_threshold = delta;
					}
					// If snapped with sibling along other axis, check for
					// shared edge
					else if (abs(sibling_rect.mRight - test_rect.mLeft +
								 padding) <= x_threshold ||
							 abs(sibling_rect.mLeft - test_rect.mRight -
								 padding) <= x_threshold)
					{
						delta = abs(test_rect.mBottom - sibling_rect.mBottom);
						if (delta <= y_threshold &&
							(test_rect.mBottom - sibling_rect.mBottom) *
							mouse_dir.mY <= 0)
						{
							snap_pos = sibling_rect.mBottom;
							snap_view = siblingp;
							y_threshold = delta;
						}
					}
					break;
				}

				case SNAP_TOP:
				{
					S32 delta = abs(test_rect.mTop - sibling_rect.mBottom);
					if (delta <= y_threshold &&
						(test_rect.mTop - sibling_rect.mBottom) *
						mouse_dir.mY <= 0)
					{
						snap_pos = sibling_rect.mBottom - padding;
						snap_view = siblingp;
						y_threshold = delta;
					}
					// If snapped with sibling along other axis, check for
					// shared edge
					else if (abs(sibling_rect.mRight - test_rect.mLeft +
								 padding) <= x_threshold ||
							 abs(sibling_rect.mLeft - test_rect.mRight -
								 padding) <= x_threshold)
					{
						delta = abs(test_rect.mTop - sibling_rect.mTop);
						if (delta <= y_threshold &&
							(test_rect.mTop - sibling_rect.mTop) *
							mouse_dir.mY <= 0)
						{
							snap_pos = sibling_rect.mTop;
							snap_view = siblingp;
							y_threshold = delta;
						}
					}
					break;
				}

				default:
				{
					llerrs << "Invalid snap edge" << llendl;
				}
			}
		}
	}

	new_edge_val = snap_pos;
	return snap_view;
}

void LLView::registerEventListener(const std::string& name,
								   LLSimpleListener* function)
{
	mDispatchList.emplace(name, function);
	LL_DEBUGS("View") << getName() << " registered " << name << LL_ENDL;
}

void LLView::deregisterEventListener(const std::string& name)
{
	dispatch_list_t::iterator it = mDispatchList.find(name);
	if (it != mDispatchList.end())
	{
		mDispatchList.hmap_erase(it);
	}
}

std::string LLView::findEventListener(LLSimpleListener* listener) const
{
	for (dispatch_list_t::const_iterator it = mDispatchList.begin(),
										 end = mDispatchList.end();
		 it != end; ++it)
	{
		if (it->second == listener)
		{
			return it->first;
		}
	}
	if (mParentView)
	{
		return mParentView->findEventListener(listener);
	}
	return LLStringUtil::null;
}

LLSimpleListener* LLView::getListenerByName(const std::string& callback_name)
{
	LLSimpleListener* callback = NULL;
	dispatch_list_t::iterator it = mDispatchList.find(callback_name);
	if (it != mDispatchList.end())
	{
		callback = it->second;
	}
	else if (mParentView)
	{
		callback = mParentView->getListenerByName(callback_name);
	}
	return callback;
}

LLControlVariable* LLView::findControl(const std::string& name)
{
	control_map_t::iterator it = mControls.find(name);
	if (it != mControls.end())
	{
		return it->second;
	}
	if (mParentView)
	{
		return mParentView->findControl(name);
	}
	return LLUI::sConfigGroup->getControl(name.c_str());
}

//static
U32 LLView::createRect(LLXMLNodePtr node, LLRect& rect, LLView* parent_view,
					   const LLRect& required_rect)
{
	U32 follows = 0;
	S32 x = rect.mLeft;
	S32 y = rect.mBottom;
	S32 w = rect.getWidth();
	S32 h = rect.getHeight();

	U32 last_x = 0;
	U32 last_y = 0;
	if (parent_view)
	{
		last_y = parent_view->getRect().getHeight();
		child_list_t::const_iterator it = parent_view->getChildList()->begin();
		if (it != parent_view->getChildList()->end())
		{
			LLView* last_view = *it;
			if (last_view->getSaveToXML())
			{
				last_x = last_view->getRect().mLeft;
				last_y = last_view->getRect().mBottom;
			}
		}
	}

	std::string rect_control;
	node->getAttributeString("rect_control", rect_control);
	if (!rect_control.empty())
	{
		LLRect rect = LLUI::sConfigGroup->getRect(rect_control.c_str());
		x = rect.mLeft;
		y = rect.mBottom;
		w = rect.getWidth();
		h = rect.getHeight();
	}

	if (node->hasAttribute("left"))
	{
		node->getAttributeS32("left", x);
	}
	if (node->hasAttribute("bottom"))
	{
		node->getAttributeS32("bottom", y);
	}

	// Make your width the width of the containing view if you do not specify a
	// width.
	if (parent_view)
	{
		if (w == 0)
		{
			w = llmax(required_rect.getWidth(),
					  parent_view->getRect().getWidth() - FLOATER_H_MARGIN - x);
		}

		if (h == 0)
		{
			h = llmax(MIN_WIDGET_HEIGHT, required_rect.getHeight());
		}
	}

	if (node->hasAttribute("width"))
	{
		node->getAttributeS32("width", w);
	}
	if (node->hasAttribute("height"))
	{
		node->getAttributeS32("height", h);
	}

	if (parent_view)
	{
		if (node->hasAttribute("left_delta"))
		{
			S32 left_delta = 0;
			node->getAttributeS32("left_delta", left_delta);
			x = last_x + left_delta;
		}
		else if (node->hasAttribute("left") && node->hasAttribute("right"))
		{
			// Compute width based on left and right
			S32 right = 0;
			node->getAttributeS32("right", right);
			if (right < 0)
			{
				right = parent_view->getRect().getWidth() + right;
			}
			w = right - x;
		}
		else if (node->hasAttribute("left"))
		{
			if (x < 0)
			{
				x = parent_view->getRect().getWidth() + x;
				follows |= FOLLOWS_RIGHT;
			}
			else
			{
				follows |= FOLLOWS_LEFT;
			}
		}
		else if (node->hasAttribute("width") && node->hasAttribute("right"))
		{
			S32 right = 0;
			node->getAttributeS32("right", right);
			if (right < 0)
			{
				right = parent_view->getRect().getWidth() + right;
			}
			x = right - w;
		}
		else
		{
			// Left not specified, same as last
			x = last_x;
		}

		if (node->hasAttribute("bottom_delta"))
		{
			S32 bottom_delta = 0;
			node->getAttributeS32("bottom_delta", bottom_delta);
			y = last_y + bottom_delta;
		}
		else if (node->hasAttribute("top"))
		{
			// compute height based on top
			S32 top = 0;
			node->getAttributeS32("top", top);
			if (top < 0)
			{
				top = parent_view->getRect().getHeight() + top;
			}
			h = top - y;
		}
		else if (node->hasAttribute("bottom"))
		{
			if (y < 0)
			{
				y = parent_view->getRect().getHeight() + y;
				follows |= FOLLOWS_TOP;
			}
			else
			{
				follows |= FOLLOWS_BOTTOM;
			}
		}
		else
		{
			// If bottom not specified, generate automatically
			if (last_y == 0)
			{
				// Treat first child as "bottom"
				y = parent_view->getRect().getHeight() - (h + VPAD);
				follows |= FOLLOWS_TOP;
			}
			else
			{
				// Treat subsequent children as "bottom_delta"
				y = last_y - (h + VPAD);
			}
		}
	}
	else
	{
		x = llmax(x, 0);
		y = llmax(y, 0);
		follows = FOLLOWS_LEFT | FOLLOWS_TOP;
	}
	rect.setOriginAndSize(x, y, w, h);

	return follows;
}

void LLView::initFromXML(LLXMLNodePtr node, LLView* parent)
{
	// create rect first, as this will supply initial follows flags
	LLRect view_rect;
	U32 follows_flags = createRect(node, view_rect, parent, getRequiredRect());
	// call reshape in case there are any child elements that need to be layed
	// out
	reshape(view_rect.getWidth(), view_rect.getHeight());
	setRect(view_rect);
	setFollows(follows_flags);

	parseFollowsFlags(node);

	if (node->hasAttribute("control_name"))
	{
		std::string control_name;
		node->getAttributeString("control_name", control_name);
		setControlName(control_name.c_str(), NULL);
	}

	if (node->hasAttribute("tool_tip"))
	{
		std::string tool_tip_msg;
		node->getAttributeString("tool_tip", tool_tip_msg);
		setToolTip(tool_tip_msg);
	}

	if (node->hasAttribute("enabled"))
	{
		bool enabled;
		node->getAttributeBool("enabled", enabled);
		setEnabled(enabled);
	}

	if (node->hasAttribute("visible"))
	{
		bool visible;
		node->getAttributeBool("visible", visible);
		setVisible(visible);
	}

	if (node->hasAttribute("hover_cursor"))
	{
		std::string cursor_string;
		node->getAttributeString("hover_cursor", cursor_string);
		mHoverCursor = getCursorFromString(cursor_string);
	}

	node->getAttributeBool("use_bounding_rect", mUseBoundingRect);
	node->getAttributeBool("mouse_opaque", mMouseOpaque);

	node->getAttributeS32("default_tab_group", mDefaultTabGroup);

	reshape(view_rect.getWidth(), view_rect.getHeight());
}

void LLView::parseFollowsFlags(LLXMLNodePtr node)
{
	if (node->hasAttribute("follows"))
	{
		setFollowsNone();

		std::string follows;
		node->getAttributeString("follows", follows);

		typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
		boost::char_separator<char> sep("|");
		tokenizer tokens(follows, sep);
		tokenizer::iterator token_iter = tokens.begin();

		while (token_iter != tokens.end())
		{
			const std::string& token_str = *token_iter;
			if (token_str == "left")
			{
				setFollowsLeft();
			}
			else if (token_str == "right")
			{
				setFollowsRight();
			}
			else if (token_str == "top")
			{
				setFollowsTop();
			}
			else if (token_str == "bottom")
			{
				setFollowsBottom();
			}
			else if (token_str == "all")
			{
				setFollowsAll();
			}
			++token_iter;
		}
	}
}

//static
LLFontGL* LLView::selectFont(LLXMLNodePtr node)
{
	std::string font_name;
	if (node->hasAttribute("font"))
	{
		node->getAttributeString("font", font_name);
	}
	if (font_name.empty())
	{
		return NULL;
	}

	std::string font_size;
	if (node->hasAttribute("font_size"))
	{
		node->getAttributeString("font_size", font_size);
	}

	U8 style = 0;
	std::string font_style;
	if (node->hasAttribute("font_style"))	// Used in XUI XML files
	{
		node->getAttributeString("font_style", font_style);
		style = LLFontGL::getStyleFromString(font_style);
	}
	if (node->hasAttribute("font-style"))	// Used in scroll list items
	{
		node->getAttributeString("font-style", font_style);
		style = LLFontGL::getStyleFromString(font_style);
	}

	LLFontDescriptor desc(font_name, font_size, style);
	LLFontGL* gl_font = LLFontGL::getFont(desc);

	return gl_font;
}

//static
LLFontGL::HAlign LLView::selectFontHAlign(LLXMLNodePtr node)
{
	if (node->hasAttribute("halign"))
	{
		std::string horizontal_align_name;
		node->getAttributeString("halign", horizontal_align_name);
		return LLFontGL::hAlignFromName(horizontal_align_name);
	}
	return LLFontGL::LEFT;
}

//static
LLFontGL::VAlign LLView::selectFontVAlign(LLXMLNodePtr node)
{
	if (node->hasAttribute("valign"))
	{
		std::string vert_align_name;
		node->getAttributeString("valign", vert_align_name);
		return LLFontGL::vAlignFromName(vert_align_name);
	}
	return LLFontGL::BASELINE;
}

//static
LLFontGL::StyleFlags LLView::selectFontStyle(LLXMLNodePtr node)
{
	if (node->hasAttribute("style"))
	{
		std::string style_flags_name;
		node->getAttributeString("style", style_flags_name);
		if (style_flags_name == "normal")
		{
			return LLFontGL::NORMAL;
		}
		if (style_flags_name == "bold")
		{
			return LLFontGL::BOLD;
		}
		if (style_flags_name == "italic")
		{
			return LLFontGL::ITALIC;
		}
		if (style_flags_name == "underline")
		{
			return LLFontGL::UNDERLINE;
		}
	}
	return LLFontGL::NORMAL;
}

bool LLView::setControlValue(const LLSD& value)
{
	const std::string& ctrlname = getControlName();
	if (!ctrlname.empty())
	{
		LLUI::sConfigGroup->setUntypedValue(ctrlname.c_str(), value);
		return true;
	}
	return false;
}

//virtual
void LLView::setControlName(const char* control_name, LLView* context)
{
	if (!context)
	{
		context = this;
	}

	if (!mControlName.empty())
	{
		if (control_name && *control_name)
		{
			llwarns << "Overwriting control '" << mControlName << "' with '"
					<< control_name << llendl;
		}
		mControlConnection.disconnect(); // Disconnect current signal
		mControlName.clear();
	}

	// Register new listener
	if (control_name && *control_name)
	{
		std::string ctrl_name(control_name);
		LLControlVariable* control = context->findControl(ctrl_name);
		if (control)
		{
			mControlName = std::move(ctrl_name);
			mControlConnection =
				control->getSignal()->connect(boost::bind(&controlListener,
														  _2, getHandle(),
														  "value"));
			setValue(control->getValue());
		}
	}
}

//static
bool LLView::controlListener(const LLSD& newvalue, LLHandle<LLView> handle,
							 const std::string& type)
{
	LLView* view = handle.get();
	if (view)
	{
		if (type == "value")
		{
			view->setValue(newvalue);
			return true;
		}
		if (type == "enabled")
		{
			view->setEnabled(newvalue.asBoolean());
			return true;
		}
		if (type == "visible")
		{
			view->setVisible(newvalue.asBoolean());
			return true;
		}
	}
	return false;
}

void LLView::addBoolControl(const std::string& name, bool initial_value)
{
	mControls[name] = new LLControlVariable(name.c_str(), TYPE_BOOLEAN,
											initial_value, "UI");
}

LLControlVariable* LLView::getControl(const std::string& name)
{
	control_map_t::iterator it = mControls.find(name);
	return it != mControls.end() ? it->second : NULL;
}

LLView* LLView::createWidget(LLXMLNodePtr xml_node) const
{
	// forward requests to ui ctrl factory
	return LLUICtrlFactory::getInstance()->createCtrlWidget(NULL, xml_node);
}
