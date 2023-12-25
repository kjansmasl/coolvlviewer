/**
 * @file llpanelgroup.h
 *
 * $LicenseInfo:firstyear=2006&license=viewergpl$
 *
 * Copyright (c) 2006-2009, Linden Research, Inc.
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

#ifndef LL_LLPANELGROUP_H
#define LL_LLPANELGROUP_H

#include "llpanel.h"
#include "lltimer.h"

#include "llgroupmgr.h"

constexpr F32 UPDATE_MEMBERS_SECONDS_PER_FRAME = 0.005f; // 5ms

class LLOfferInfo;
class LLPanelGroupTab;
class LLTabContainer;

class LLPanelGroupTabObserver
{
public:
	LLPanelGroupTabObserver()					{}
	virtual ~LLPanelGroupTabObserver()			{}
	virtual void tabChanged() = 0;
};

class LLPanelGroup : public LLPanel,
					 public LLGroupMgrObserver,
					 public LLPanelGroupTabObserver
{
public:
	LLPanelGroup(const std::string& filename, const std::string& name,
				 const LLUUID& group_id,
				 const std::string& initial_tab_selected = std::string());
	virtual ~LLPanelGroup();

	virtual bool postBuild();

	static void onBtnOK(void*);
	static void onBtnCancel(void*);
	static void onBtnApply(void*);
	static void onBtnRefresh(void*);
	static void onClickTab(void*,bool);
	void handleClickTab();

	void setGroupID(const LLUUID& group_id);
	void selectTab(std::string tab_name);

	// Called when embedded in a floater during a close attempt.
	bool canClose();

	// Checks if the current tab needs to be applied, and tries to switch to
	// the requested tab.
	bool attemptTransition();

	// Switches to the requested tab (will close() if requested is NULL)
	void transitionToTab();

	void updateTabVisibility();

	// Used by attemptTransition to query the user's response to a tab that
	// needs to apply.
	bool handleNotifyCallback(const LLSD& notification, const LLSD& response);

	bool apply();
	void refreshData();
	void close();
	void draw();

	// Group manager observer trigger.
	virtual void changed(LLGroupChange gc);

	// PanelGroupTab observer trigger
	virtual void tabChanged();

	void setAllowEdit(bool v)					{ mAllowEdit = v; }

	void showNotice(const std::string& subject, const std::string& message,
					bool has_inventory, const std::string& inventory_name,
					LLOfferInfo* inventory_offer);
protected:
	LLPanelGroupTab*	mCurrentTab;
	LLPanelGroupTab*	mRequestedTab;
	LLTabContainer*		mTabContainer;
	LLButton*			mApplyBtn;
	LLButton*			mRefreshBtn;

	LLTimer				mRefreshTimer;

	bool				mIgnoreTransition;
	bool				mForceClose;
	bool				mAllowEdit;
	bool				mShowingNotifyDialog;

	std::string			mInitialTab;
	std::string			mFilename;
	std::string			mDefaultNeedsApplyMesg;
	std::string			mWantApplyMesg;
};

class LLPanelGroupTab : public LLPanel
{
public:
	LLPanelGroupTab(const std::string& name, const LLUUID& group_id)
	:	LLPanel(name),
		mGroupID(group_id),
		mAllowEdit(true),
		mHasModal(false)
	{
	}

	virtual ~LLPanelGroupTab();

	// Factory that returns a new LLPanelGroupFoo tab.
	static void* createTab(void* data);

	// Triggered when the tab becomes active.
	virtual void activate()						{}

	// Triggered when the tab becomes inactive.
	virtual void deactivate()					{}

	// Asks if something needs to be applied.
	// If returning true, this function should modify the message to the user.
	virtual bool needsApply(std::string& mesg)	{ return false; }

	// Asks if there is currently a modal dialog being shown.
	virtual bool hasModal()						{ return mHasModal; }

	// Request to apply current data.
	// If returning fail, this function should modify the message to the user.
	virtual bool apply(std::string& mesg)		{ return true; }

	// Request a cancel of changes
	virtual void cancel()						{}

	// Triggered when group information changes in the group manager.
	virtual void update(LLGroupChange gc)		{}

	// This is the text to be displayed when a help button is pressed.
	virtual std::string getHelpText() const		{ return mHelpText; }

	// Display anything returned by getHelpText
	static void onClickHelp(void* data);
	void handleClickHelp();

	// This just connects the help button callback.
	virtual bool postBuild();

	virtual bool isVisibleByAgent();

	void setAllowEdit(bool v)					{ mAllowEdit = v; }

	void addObserver(LLPanelGroupTabObserver* obs);
	void removeObserver(LLPanelGroupTabObserver* obs);
	void notifyObservers();

protected:
	LLUUID			mGroupID;
	LLTabContainer*	mTabContainer;
	std::string		mHelpText;

	bool			mAllowEdit;
	bool			mHasModal;

	typedef std::set<LLPanelGroupTabObserver*> observer_list_t;
	observer_list_t	mObservers;
};

#endif // LL_LLPANELGROUP_H
