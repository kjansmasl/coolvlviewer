/**
 * @file lluictrl.cpp
 * @author James Cook, Richard Nelson, Tom Yedwab
 * @brief Abstract base class for UI controls
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

//#include "llviewerprecompiledheaders.h"
#include "linden_common.h"

#include "lluictrl.h"

#include "llpanel.h"

static const std::string LL_UI_CRTL_TAG = "ui_ctrl";
static LLRegisterWidget<LLUICtrl> r28(LL_UI_CRTL_TAG);

// NOTE: the LLFocusableElement implementation has been moved to
// llfocusmgr.cpp, to mirror the header where the class is defined.

LLUICtrl::LLUICtrl()
:	mCommitCallback(NULL),
	mLostTopCallback(NULL),
	mValidateCallback(NULL),
	mCallbackUserData(NULL),
	mTentative(false),
	mTabStop(true),
	mIsChrome(false)
{
}

LLUICtrl::LLUICtrl(const std::string& name, const LLRect& rect,
				   bool mouse_opaque,
				   void (*on_commit_callback)(LLUICtrl*, void*),
				   void* callback_userdata, U32 reshape)
:	LLView(name, rect, mouse_opaque, reshape),
	mCommitCallback(on_commit_callback),
	mLostTopCallback(NULL),
	mValidateCallback(NULL),
	mCallbackUserData(callback_userdata),
	mTentative(false),
	mTabStop(true),
	mIsChrome(false)
{
}

//virtual
LLUICtrl::~LLUICtrl()
{
	gFocusMgr.releaseFocusIfNeeded(this); // calls onCommit()

	if (gFocusMgr.getTopCtrl() == this)
	{
		llwarns << "UI Control holding top ctrl deleted: " << getName()
				<< ".  Top view removed." << llendl;
		gFocusMgr.removeTopCtrlWithoutCallback(this);
	}
}

//virtual
void LLUICtrl::onCommit()
{
	if (mCommitCallback)
	{
		(*mCommitCallback)(this, mCallbackUserData);
	}
}

//virtual
bool LLUICtrl::hasFocus() const
{
	return gFocusMgr.childHasKeyboardFocus(this);
}

//virtual
void LLUICtrl::setFocus(bool b)
{
	// Focus NEVER goes to UI ctrls that are disabled !
	if (!getEnabled())
	{
		return;
	}
	if (b)
	{
		if (!hasFocus())
		{
			gFocusMgr.setKeyboardFocus(this);
		}
	}
	else if (gFocusMgr.childHasKeyboardFocus(this))
	{
		gFocusMgr.setKeyboardFocus(NULL);
	}
}

//virtual
void LLUICtrl::onFocusReceived()
{
	// Trigger callbacks
	LLFocusableElement::onFocusReceived();

	// Find first view in hierarchy above new focus that is a LLUICtrl

	LLUICtrl* last_focusp = gFocusMgr.getLastKeyboardFocusUICtrl();

	LLView* viewp = getParent();
	while (viewp && !viewp->isCtrl())
	{
		viewp = viewp->getParent();
	}

	// And if it has newly gained focus, call onFocusReceived()
	LLUICtrl* ctrlp = (LLUICtrl*)viewp;
	if (ctrlp && (!last_focusp || !last_focusp->hasAncestor(ctrlp)))
	{
		ctrlp->onFocusReceived();
	}
}

//virtual
void LLUICtrl::onFocusLost()
{
	// Trigger callbacks
	LLFocusableElement::onFocusLost();

	// Find first view in hierarchy above old focus that is a LLUICtrl
	LLView* viewp = getParent();
	while (viewp && !viewp->isCtrl())
	{
		viewp = viewp->getParent();
	}

	// And if it has just lost focus, call onFocusReceived()
	LLUICtrl* ctrlp = (LLUICtrl*)viewp;
	// hasFocus() includes any descendants
	if (ctrlp && !ctrlp->hasFocus())
	{
		ctrlp->onFocusLost();
	}
}

//virtual
void LLUICtrl::onLostTop()
{
	if (mLostTopCallback)
	{
		mLostTopCallback(this, mCallbackUserData);
	}
}

bool LLUICtrl::getIsChrome() const
{
	if (mIsChrome) return true;

	LLView* parent_ctrlp = getParent();
	while (parent_ctrlp)
	{
		if (parent_ctrlp->getIsChrome())
		{
			return true;
		}
		parent_ctrlp = parent_ctrlp->getParent();
	}

	return false;
}

// This comparator uses the crazy disambiguating logic of LLCompareByTabOrder,
// but to switch up the order so that children that have the default tab group
// come first and those that are prior to the default tab group come last.
class CompareByDefaultTabGroup : public LLCompareByTabOrder
{
public:
	LL_INLINE CompareByDefaultTabGroup(LLView::child_tab_order_t order,
									   S32 default_tab_group)
	:	LLCompareByTabOrder(order),
		mDefaultTabGroup(default_tab_group)
	{
	}

private:
	LL_INLINE bool compareTabOrders(const LLView::tab_order_t& a,
									const LLView::tab_order_t& b) const override
	{
		S32 ag = a.first; // tab group for a
		S32 bg = b.first; // tab group for b
		// These two ifs have the effect of moving elements prior to the
		// default tab group to the end of the list (still sorted relative to
		// each other, though)
		if (ag < mDefaultTabGroup && bg >= mDefaultTabGroup)
		{
			return false;
		}
		if (bg < mDefaultTabGroup && ag >= mDefaultTabGroup)
		{
			return true;
		}
		// Sort correctly if they're both on the same side of the default tab
		// group
		return a < b;
	}

private:
	S32 mDefaultTabGroup;
};

// Sorter for plugging into the query.
class LLUICtrl::DefaultTabGroupFirstSorter
:	public LLQuerySorter, public LLSingleton<DefaultTabGroupFirstSorter>
{
	friend class LLSingleton<DefaultTabGroupFirstSorter>;

public:
	LL_INLINE void operator()(LLView* parent,
							  view_list_t& children) const override
	{
		children.sort(CompareByDefaultTabGroup(parent->getCtrlOrder(),
											   parent->getDefaultTabGroup()));
	}
};

bool LLUICtrl::focusFirstItem(bool prefer_text_fields, bool focus_flash)
{
	// Try to select default tab group child
	LLCtrlQuery query = getTabOrderQuery();
	// Sort things such that the default tab group is at the front
	query.setSorter(DefaultTabGroupFirstSorter::getInstance());
	child_list_t result = query(this);
	if (result.size() > 0)
	{
		LLUICtrl* ctrlp = (LLUICtrl*)result.front();
		if (!ctrlp->hasFocus())
		{
			ctrlp->setFocus(true);
			ctrlp->onTabInto();
			if (focus_flash)
			{
				gFocusMgr.triggerFocusFlash();
			}
		}
		return true;
	}
	// Search for text field first
	if (prefer_text_fields)
	{
		LLCtrlQuery query = getTabOrderQuery();
		query.addPreFilter(LLUICtrl::LLTextInputFilter::getInstance());
		child_list_t result = query(this);
		if (result.size() > 0)
		{
			LLUICtrl* ctrlp = (LLUICtrl*)result.front();
			if (!ctrlp->hasFocus())
			{
				ctrlp->setFocus(true);
				ctrlp->onTabInto();
				gFocusMgr.triggerFocusFlash();
			}
			return true;
		}
	}
	// No text field found, or we do not care about text fields
	result = getTabOrderQuery().run(this);
	if (result.size() > 0)
	{
		LLUICtrl* ctrlp = (LLUICtrl*)result.front();
		if (!ctrlp->hasFocus())
		{
			ctrlp->setFocus(true);
			ctrlp->onTabInto();
			gFocusMgr.triggerFocusFlash();
		}
		return true;
	}
	return false;
}

bool LLUICtrl::focusLastItem(bool prefer_text_fields)
{
	// Search for text field first
	if (prefer_text_fields)
	{
		LLCtrlQuery query = getTabOrderQuery();
		query.addPreFilter(LLUICtrl::LLTextInputFilter::getInstance());
		child_list_t result = query(this);
		if (result.size() > 0)
		{
			LLUICtrl* ctrlp = (LLUICtrl*)result.back();
			if (!ctrlp->hasFocus())
			{
				ctrlp->setFocus(true);
				ctrlp->onTabInto();
				gFocusMgr.triggerFocusFlash();
			}
			return true;
		}
	}
	// No text field found, or we do not care about text fields
	child_list_t result = getTabOrderQuery().run(this);
	if (result.size() > 0)
	{
		LLUICtrl* ctrlp = (LLUICtrl*)result.back();
		if (!ctrlp->hasFocus())
		{
			ctrlp->setFocus(true);
			ctrlp->onTabInto();
			gFocusMgr.triggerFocusFlash();
		}
		return true;
	}
	return false;
}

bool LLUICtrl::focusNextItem(bool text_fields_only)
{
	// This assumes that this method is called on the focus root.
	LLCtrlQuery query = getTabOrderQuery();
	if (text_fields_only || LLUI::sTabToTextFieldsOnly)
	{
		query.addPreFilter(LLUICtrl::LLTextInputFilter::getInstance());
	}
	child_list_t result = query(this);
	return focusNext(result);
}

bool LLUICtrl::focusPrevItem(bool text_fields_only)
{
	// This assumes that this method is called on the focus root.
	LLCtrlQuery query = getTabOrderQuery();
	if (text_fields_only || LLUI::sTabToTextFieldsOnly)
	{
		query.addPreFilter(LLUICtrl::LLTextInputFilter::getInstance());
	}
	child_list_t result = query(this);
	return focusPrev(result);
}

LLUICtrl* LLUICtrl::findRootMostFocusRoot()
{
	LLUICtrl* focus_rootp = NULL;
	LLUICtrl* next_viewp = this;
	while (next_viewp)
	{
		if (next_viewp->isFocusRoot())
		{
			focus_rootp = next_viewp;
		}
		next_viewp = next_viewp->getParentUICtrl();
	}
	return focus_rootp;
}

void LLUICtrl::initFromXML(LLXMLNodePtr nodep, LLView* parentp)
{
	bool has_tab_stop = mTabStop;
	nodep->getAttributeBool("tab_stop", has_tab_stop);
	setTabStop(has_tab_stop);

	LLView::initFromXML(nodep, parentp);
}

LLXMLNodePtr LLUICtrl::getXML(bool save_children) const
{
	LLXMLNodePtr nodep = LLView::getXML(save_children);
	nodep->createChild("tab_stop", true)->setBoolValue(mTabStop);
	return nodep;
}

//static
LLView* LLUICtrl::fromXML(LLXMLNodePtr node, LLView* parent,
						  class LLUICtrlFactory* factory)
{
	LLUICtrl* ctrlp = new LLUICtrl();
	ctrlp->initFromXML(node, parent);
	return ctrlp;
}

// Skip over any parents that are not LLUICtrl's. Used in focus logic since
// only LLUICtrl elements can have focus.
LLUICtrl* LLUICtrl::getParentUICtrl() const
{
	LLView* parentp = getParent();
	while (parentp)
	{
		if (parentp->isCtrl())
		{
			return (LLUICtrl*)parentp;
		}
		parentp = parentp->getParent();
	}
	return NULL;
}
