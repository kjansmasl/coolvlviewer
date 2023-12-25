/**
 * @file lltabcontainer.h
 * @brief LLTabContainer class
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

#ifndef LL_TABCONTAINER_H
#define LL_TABCONTAINER_H

#include "llframetimer.h"
#include "llpanel.h"
#include "lltextbox.h"

constexpr S32 TABCNTR_CLOSE_BTN_SIZE = 16;
constexpr S32 TABCNTR_HEADER_HEIGHT = LLPANEL_BORDER_WIDTH +
									  TABCNTR_CLOSE_BTN_SIZE;

class LLTabContainer : public LLPanel
{
protected:
	LOG_CLASS(LLTabContainer);

public:
	enum TabPosition
	{
		TOP,
		BOTTOM,
		LEFT
	};

	typedef enum e_insertion_point
	{
		START,
		END,
		LEFT_OF_CURRENT,
		RIGHT_OF_CURRENT
	} eInsertionPoint;

	LLTabContainer(const std::string& name, const LLRect& rect,
				   TabPosition pos, bool bordered, bool is_vertical);

	~LLTabContainer() override;

	// From LLView

	void setValue(const LLSD& value) override;

	void reshape(S32 width, S32 height, bool call_from_parent = true) override;

	void draw() override;

	bool handleMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleHover(S32 x, S32 y, MASK mask) override;
	bool handleMouseUp(S32 x, S32 y, MASK mask) override;
	bool handleToolTip(S32 x, S32 y, std::string& msg, LLRect* rect) override;
	bool handleKeyHere(KEY key, MASK mask) override;
	bool handleDragAndDrop(S32 x, S32 y, MASK mask, bool drop,
						   EDragAndDropType type, void* cargo_data,
						   EAcceptance* accept, std::string& tooltip) override;

	LLXMLNodePtr getXML(bool save_children = true) const override;
	LLView* getChildView(const char* name, bool recurse = true,
						 bool create_if_missing = true) const override;

	void addTabPanel(LLPanel* child, const std::string& label,
					 bool select = false,
					 void (*on_tab_clicked)(void*, bool) = NULL,
					 void* userdata = NULL,
					 S32 indent = 0, bool placeholder = false,
					 eInsertionPoint insertion_point = END);

	void addPlaceholder(LLPanel* child, const std::string& label);
	void removeTabPanel(LLPanel* child);

	void lockTabs(S32 num_tabs = 0);
	void unlockTabs();
	LL_INLINE S32 getNumLockedTabs()				{ return mLockedTabCount; }

	void enableTabButton(S32 which, bool enable);
	void deleteAllTabs();
	LLPanel* getCurrentPanel();
	S32 getCurrentPanelIndex();
	S32 getTabCount();
	LLPanel* getPanelByIndex(S32 index);
	S32 getIndexForPanel(LLPanel* panel);
	S32 getPanelIndexByTitle(const std::string& title);
	LLPanel* getPanelByName(const std::string& name);
	void  setCurrentTabName(const std::string& name);

	LL_INLINE S32 getTotalTabWidth() const			{ return mTotalTabWidth; }

	void selectFirstTab();
	void selectLastTab();
	void selectNextTab();
	void selectPrevTab();
	bool selectTabPanel(LLPanel* child);
	bool selectTab(S32 which);
	bool selectTabByName(const std::string& title);
	bool setTab(S32 which);

	// Sets a tooltip on the tab button: when 'tooltip' is an empty string,
	// the tooltip is reverted to the default one.
	void setTabButtonTooltip(LLPanel* child, const std::string& tooltip);

	bool getTabPanelFlashing(LLPanel* child);
	void setTabPanelFlashing(LLPanel* child, bool state);

	void setTabImage(LLPanel* child, std::string img_name,
					 const LLColor4& color = LLColor4::white);

	void setTitle(const std::string& title);
	const std::string getPanelTitle(S32 index);

	void setTopBorderHeight(S32 height);
	S32 getTopBorderHeight() const;

	void setTabChangeCallback(LLPanel* tab,
							  void (*on_tab_clicked)(void*, bool));
	void setTabPrecommitChangeCallback(LLPanel* tab,
									   void (*on_precommit)(void*, bool));
	void setTabUserData(LLPanel* tab, void* userdata);

	void setRightTabBtnOffset(S32 offset);
	void setPanelTitle(S32 index, const std::string& title);

	LL_INLINE TabPosition getTabPosition() const	{ return mTabPosition; }
	LL_INLINE void setMinTabWidth(S32 width)		{ mMinTabWidth = width; }
	LL_INLINE void setMaxTabWidth(S32 width)		{ mMaxTabWidth = width; }
	LL_INLINE S32 getMinTabWidth() const			{ return mMinTabWidth; }
	LL_INLINE S32 getMaxTabWidth() const			{ return mMaxTabWidth; }

	LL_INLINE void startDragAndDropDelayTimer()		{ mDragAndDropDelayTimer.start(); }

	static void	onCloseBtn(void* userdata);
	static void	onTabBtn(void* userdata);
	static void	onNextBtn(void* userdata);
	static void	onNextBtnHeld(void* userdata);
	static void	onPrevBtn(void* userdata);
	static void	onPrevBtnHeld(void* userdata);
	static void onJumpFirstBtn(void* userdata);
	static void onJumpLastBtn(void* userdata);

	static LLView* fromXML(LLXMLNodePtr node, LLView* parent,
						   LLUICtrlFactory* factory);

private:
	// Structure used to map tab buttons to and from tab panels
	struct LLTabTuple
	{
		LLTabTuple(LLTabContainer* c, LLPanel* p, LLButton* b,
				   void (*cb)(void*, bool), void* userdata,
				   LLTextBox* placeholder = NULL,
				   void (*pcb)(void*, bool) = NULL)
		:	mTabContainer(c),
			mTabPanel(p),
			mButton(b),
			mOnChangeCallback(cb),
			mPrecommitChangeCallback(pcb),
			mUserData(userdata),
			mPlaceholderText(placeholder),
			mPadding(0)
		{
		}

		LLTabContainer*  mTabContainer;
		LLPanel*		 mTabPanel;
		LLButton*		 mButton;
		void			 (*mOnChangeCallback)(void*, bool);
		// Precommit callback gets called before tab is changed and can prevent
		// it from being changed. onChangeCallback is called immediately after
		// tab is actually changed - Nyx
		void			 (*mPrecommitChangeCallback)(void*, bool);
		void*			 mUserData;
		LLTextBox*		 mPlaceholderText;
		S32				 mPadding;
	};

	void initButtons();

	LL_INLINE LLTabTuple* getTab(S32 index) 		{ return mTabList[index]; }
	LLTabTuple* getTabByPanel(LLPanel* child);
	void insertTuple(LLTabTuple * tuple, eInsertionPoint insertion_point);

	LL_INLINE S32 getScrollPos() const				{ return mScrollPos; }
	LL_INLINE void setScrollPos(S32 pos)			{ mScrollPos = pos; }
	LL_INLINE S32 getMaxScrollPos() const			{ return mMaxScrollPos; }
	LL_INLINE void setMaxScrollPos(S32 pos)			{ mMaxScrollPos = pos; }
	LL_INLINE S32 getScrollPosPixels() const		{ return mScrollPosPixels; }
	LL_INLINE void setScrollPosPixels(S32 pixels)	{ mScrollPosPixels = pixels; }

	LL_INLINE void setTabsHidden(bool hidden)		{ mTabsHidden = hidden; }
	LL_INLINE bool getTabsHidden() const			{ return mTabsHidden; }

	LL_INLINE void setCurrentPanelIndex(S32 index)	{ mCurrentTabIdx = index; }

	// No wrap
	LL_INLINE void scrollPrev()						{ mScrollPos = llmax(0, mScrollPos - 1); }
	LL_INLINE void scrollNext()						{ mScrollPos = llmin(mScrollPos + 1, mMaxScrollPos); }

	void updateMaxScrollPos();
	void commitHoveredButton(S32 x, S32 y);

private:
	LLTextBox*		mTitleBox;
	LLButton*		mPrevArrowBtn;
	LLButton*		mNextArrowBtn;
	// Horizontal specific
	LLButton*		mJumpPrevArrowBtn;
	LLButton*		mJumpNextArrowBtn;

	S32				mCurrentTabIdx;
	S32				mNextTabIdx;
	S32				mScrollPos;
	S32				mScrollPosPixels;
	S32				mMaxScrollPos;

	void			(*mCloseCallback)(void*);
	void*			mCallbackUserdata;

	TabPosition 	mTabPosition;
	S32				mTopBorderHeight;
	S32				mLockedTabCount;
	S32				mMinTabWidth;

	// Extra room to the right of the tab buttons.
	S32				mRightTabBtnOffset;

	S32				mMaxTabWidth;
	S32				mTotalTabWidth;

	LLFrameTimer	mScrollTimer;
	LLFrameTimer	mDragAndDropDelayTimer;

	bool			mTabsHidden;
	bool			mIsVertical;

	bool			mScrolled;

	typedef std::vector<LLTabTuple*> tuple_list_t;
	tuple_list_t	mTabList;
};

#endif  // LL_TABCONTAINER_H
