/**
 * @file lltoolbar.cpp
 * @author James Cook, Richard Nelson
 * @brief Large friendly buttons at bottom of screen.
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

#include "llviewerprecompiledheaders.h"

#include "lltoolbar.h"

#include "imageids.h"
#include "llbutton.h"
#include "llparcel.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "llfirstuse.h"
#include "llfloaterchatterbox.h"
#include "llfloaterfriends.h"
#include "llfloatergroups.h"
#include "llfloaterinventory.h"
#include "llfloaterminimap.h"
#include "hbfloatersearch.h"
#include "hbfloaterradar.h"
#include "llfloatersnapshot.h"
#include "llfloaterworldmap.h"
//MK
#include "mkrlinterface.h"
//mk
#include "lltooldraganddrop.h"
#include "lltoolgrab.h"
#include "lltoolmgr.h"
#include "llviewermenu.h"
#include "llviewercontrol.h"
#include "llviewerparcelmgr.h"
#include "llviewerwindow.h"
#include "llvoavatarself.h"

#if LL_DARWIN

#include "llresizehandle.h"

// This class draws like an LLResizeHandle but has no interactivity. It's just
// there to provide a cue to the user that the lower right corner of the window
// functions as a resize handle.
class LLFakeResizeHandle final : public LLResizeHandle
{
public:
	LLFakeResizeHandle(const std::string& name, const LLRect& rect,
					   S32 min_width, S32 min_height,
					   ECorner corner = RIGHT_BOTTOM)
	:	LLResizeHandle(name, rect, min_width, min_height, corner)
	{
	}

	bool handleHover(S32 x, S32 y, MASK mask) override		{ return false; };
	bool handleMouseDown(S32 x, S32 y, MASK mask) override	{ return false; };
	bool handleMouseUp(S32 x, S32 y, MASK mask) override	{ return false; };
};

#endif // LL_DARWIN

//
// Globals
//

// Instance created in LLViewerWindow::initWorldUI()
LLToolBar* gToolBarp = NULL;

//
// Statics
//
F32	LLToolBar::sInventoryAutoOpenTime = 1.f;

//
// Functions
//

LLToolBar::LLToolBar(const LLRect& rect)
:	LLPanel("tool bar", rect, BORDER_NO)
#if LL_DARWIN
	, mResizeHandle(NULL)
#endif // LL_DARWIN
{
	llassert_always(gToolBarp == NULL);	// Only one instance allowed

	setIsChrome(true);
	setFollows(FOLLOWS_LEFT | FOLLOWS_RIGHT | FOLLOWS_BOTTOM);
	LLUICtrlFactory::getInstance()->buildPanel(this, "panel_toolbar.xml");
	setFocusRoot(true);
	gToolBarp = this;
}

bool LLToolBar::postBuild()
{
	mChatButton = getChild<LLButton>("chat_btn");
	mChatButton->setClickedCallback(onClickChat, this);
	mChatButton->setControlName("ChatVisible", NULL);

	mIMButton = getChild<LLButton>("communicate_btn");
	mIMButton->setClickedCallback(onClickIM, this);
	mIMButton->setControlName("ShowCommunicate", NULL);

	mFriendsButton = getChild<LLButton>("friends_btn");
	mFriendsButton->setClickedCallback(onClickFriends, this);
	mFriendsButton->setControlName("ShowFriends", NULL);

	mGroupsButton = getChild<LLButton>("groups_btn");
	mGroupsButton->setClickedCallback(onClickGroups, this);
	mGroupsButton->setControlName("ShowGroups", NULL);

	mFlyButton = getChild<LLButton>("fly_btn");
	mFlyButton->setClickedCallback(onClickFly, this);
	mFlyButton->setControlName("FlyBtnState", NULL);

	mSnapshotButton = getChild<LLButton>("snapshot_btn");
	mSnapshotButton->setClickedCallback(onClickSnapshot, this);
	mSnapshotButton->setControlName("", NULL);

	mSearchButton = getChild<LLButton>("directory_btn");
	mSearchButton->setClickedCallback(onClickSearch, this);
	mSearchButton->setControlName("ShowSearch", NULL);

	mBuildButton = getChild<LLButton>("build_btn");
	mBuildButton->setClickedCallback(onClickBuild, this);
	mBuildButton->setControlName("BuildBtnState", NULL);

	mRadarButton = getChild<LLButton>("radar_btn");
	mRadarButton->setClickedCallback(onClickRadar, this);
	mRadarButton->setControlName("ShowRadar", NULL);

	mMiniMapButton = getChild<LLButton>("minimap_btn");
	mMiniMapButton->setClickedCallback(onClickMiniMap, this);
	mMiniMapButton->setControlName("ShowMiniMap", NULL);

	mMapButton = getChild<LLButton>("map_btn");
	mMapButton->setClickedCallback(onClickMap, this);
	mMapButton->setControlName("ShowWorldMap", NULL);

	mInventoryButton = getChild<LLButton>("inventory_btn");
	mInventoryButton->setClickedCallback(onClickInventory, this);
	mInventoryButton->setControlName("ShowInventory", NULL);

	for (child_list_const_iter_t child_iter = getChildList()->begin();
		 child_iter != getChildList()->end(); ++child_iter)
	{
		LLView* view = *child_iter;
		LLButton* buttonp = dynamic_cast<LLButton*>(view);
		if (buttonp)
		{
			buttonp->setSoundFlags(LLView::SILENT);
		}
	}

#if LL_DARWIN
	if (mResizeHandle == NULL)
	{
		LLRect rect(0, 0, RESIZE_HANDLE_WIDTH, RESIZE_HANDLE_HEIGHT);
		mResizeHandle = new LLFakeResizeHandle(std::string(""), rect,
											   RESIZE_HANDLE_WIDTH,
											   RESIZE_HANDLE_HEIGHT);
		this->addChildAtEnd(mResizeHandle);
	}
#endif // LL_DARWIN

	layoutButtons();

	return true;
}

LLToolBar::~LLToolBar()
{
	// LLView destructor cleans up children
	gToolBarp = NULL;
}

bool LLToolBar::handleDragAndDrop(S32 x, S32 y, MASK mask, bool drop,
								  EDragAndDropType cargo_type,
								  void* cargo_data,
								  EAcceptance* accept,
								  std::string& tooltip_msg)
{
	LLFloaterInventory* floaterp = LLFloaterInventory::getActiveFloater();

	if (floaterp && floaterp->getVisible())
	{
		mInventoryAutoOpen = false;
	}
	else if (mInventoryButton->getRect().pointInRect(x, y))
	{
		if (mInventoryAutoOpen)
		{
			if (!(floaterp && floaterp->getVisible()) &&
				mInventoryAutoOpenTimer.getElapsedTimeF32() > sInventoryAutoOpenTime)
			{
				LLFloaterInventory::showAgentInventory();
			}
		}
		else
		{
			mInventoryAutoOpen = true;
			mInventoryAutoOpenTimer.reset();
		}
	}

	return LLPanel::handleDragAndDrop(x, y, mask, drop, cargo_type, cargo_data,
									  accept, tooltip_msg);
}

// static
void LLToolBar::toggle()
{
	if (gToolBarp)
	{
		bool show = gSavedSettings.getBool("ShowToolBar");
		gSavedSettings.setBool("ShowToolBar", !show);
		gToolBarp->setVisible(!show);
	}
}

// static
bool LLToolBar::isVisible()
{
	return gToolBarp && gToolBarp->getVisible();
}

void LLToolBar::layoutButtons()
{
	// Always spans whole window. JC
	constexpr S32 FUDGE_WIDTH_OF_SCREEN = 4;
	constexpr S32 PAD = 2;
	S32 width = gViewerWindowp->getWindowWidth() + FUDGE_WIDTH_OF_SCREEN;
	S32 count = getChildCount();
	if (!count) return;

	bool show = gSavedSettings.getBool("ShowChatButton");
	mChatButton->setVisible(show);
	if (!show) --count;

	show = gSavedSettings.getBool("ShowIMButton");
	mIMButton->setVisible(show);
	if (!show) --count;

	show = gSavedSettings.getBool("ShowFriendsButton");
	mFriendsButton->setVisible(show);
	if (!show) --count;

	show = gSavedSettings.getBool("ShowGroupsButton");
	mGroupsButton->setVisible(show);
	if (!show) --count;

	show = gSavedSettings.getBool("ShowFlyButton");
	mFlyButton->setVisible(show);
	if (!show) --count;

	show = gSavedSettings.getBool("ShowSnapshotButton");
	mSnapshotButton->setVisible(show);
	if (!show) --count;

	show = gSavedSettings.getBool("ShowSearchButton");
	mSearchButton->setVisible(show);
	if (!show) --count;

	show = gSavedSettings.getBool("ShowBuildButton");
	mBuildButton->setVisible(show);
	if (!show) --count;

	show = gSavedSettings.getBool("ShowRadarButton");
	mRadarButton->setVisible(show);
	if (!show) --count;

	show = gSavedSettings.getBool("ShowMiniMapButton");
	mMiniMapButton->setVisible(show);
	if (!show) --count;

	show = gSavedSettings.getBool("ShowMapButton");
	mMapButton->setVisible(show);
	if (!show) --count;

	show = gSavedSettings.getBool("ShowInventoryButton");
	mInventoryButton->setVisible(show);
	if (!show) --count;

	if (count < 1)
	{
		// No button in the toolbar !  Hide it.
		gSavedSettings.setBool("ShowToolBar", false);
		return;
	}

#if LL_DARWIN
	// this function may be called before postBuild(), in which case
	// mResizeHandle won't have been set up yet.
	if (mResizeHandle)
	{
		// a resize handle has been added as a child, increasing the count by
		// one.
		--count;

		if (!gWindowp->getFullscreen())
		{
			// Only when running in windowed mode on the Mac, leave room for a
			// resize widget on the right edge of the bar.
			width -= RESIZE_HANDLE_WIDTH;

			LLRect r;
			r.mLeft = width - PAD;
			r.mBottom = 0;
			r.mRight = r.mLeft + RESIZE_HANDLE_WIDTH;
			r.mTop = r.mBottom + RESIZE_HANDLE_HEIGHT;
			mResizeHandle->setRect(r);
			mResizeHandle->setVisible(true);
		}
		else
		{
			mResizeHandle->setVisible(false);
		}
	}
#endif // LL_DARWIN

	// We actually want to extend "PAD" pixels off the right edge of the
	// screen, such that the rightmost button is aligned.
	F32 segment_width = (F32)(width + PAD) / (F32)count;
	S32 btn_width = lltrunc(segment_width - PAD);

	// Evenly space all views
	S32 height = -1;
	S32 i = count - 1;
	for (child_list_const_iter_t child_iter = getChildList()->begin();
		 child_iter != getChildList()->end(); ++child_iter)
	{
		LLView* btn_view = *child_iter;
		LLButton* buttonp = dynamic_cast<LLButton*>(btn_view);
		if (buttonp && btn_view->getVisible())
		{
			if (height < 0)
			{
				height = btn_view->getRect().getHeight();
			}
			S32 x = ll_roundp(i * segment_width);
			S32 y = 0;
			LLRect r;
			r.setOriginAndSize(x, y, btn_width, height);
			btn_view->setRect(r);
			--i;
		}
	}
}

// virtual
void LLToolBar::reshape(S32 width, S32 height, bool called_from_parent)
{
	LLPanel::reshape(width, height, called_from_parent);

	layoutButtons();
}

// Per-frame updates of visibility
void LLToolBar::refresh()
{
	// Ensure we use setVisible() on startup:
	static S32 previous_visibility = -1;
	static LLCachedControl<bool> show_toolbar(gSavedSettings, "ShowToolBar");
	bool visible = show_toolbar && !gAgent.cameraMouselook();

	if (previous_visibility != (S32)visible)
	{
		setVisible(visible);
		if (visible)
		{
			// In case there would be no button to show, it would re-hide the
			// toolbar (on next frame)
			layoutButtons();
		}
		previous_visibility = (S32)visible;
	}
	if (!visible) return;

	bool sitting = false;
	if (isAgentAvatarValid())
	{
		sitting = gAgentAvatarp->mIsSitting;
	}
	mFlyButton->setEnabled(!sitting &&
						   (gAgent.canFly() || gAgent.getFlying()));

//MK
	if (gRLenabled)
	{
		mRadarButton->setEnabled(!gRLInterface.mContainsShownames &&
								 !gRLInterface.mContainsShownametags);
		mMiniMapButton->setEnabled(!gRLInterface.mContainsShowminimap);
		mMapButton->setEnabled(!gRLInterface.mContainsShowworldmap &&
							   !gRLInterface.mContainsShowloc);
		mInventoryButton->setEnabled(!gRLInterface.mContainsShowinv);
	}
//mk

	mBuildButton->setEnabled(gViewerParcelMgr.allowAgentBuild());
}

// static
void LLToolBar::onClickChat(void* user_data)
{
	handle_chat(NULL);
}

// static
void LLToolBar::onClickIM(void* user_data)
{
	LLFloaterChatterBox::toggleInstance(LLSD());
}

// static
void LLToolBar::onClickFly(void*)
{
	gAgent.toggleFlying();
}

// static
void LLToolBar::onClickSnapshot(void*)
{
	LLFloaterSnapshot::show(NULL);
}

// static
void LLToolBar::onClickSearch(void*)
{
	HBFloaterSearch::toggle();
}

// static
void LLToolBar::onClickBuild(void*)
{
	gToolMgr.toggleBuildMode();
}

// static
void LLToolBar::onClickMiniMap(void*)
{
	LLFloaterMiniMap::toggleInstance();
}

// static
void LLToolBar::onClickRadar(void*)
{
	HBFloaterRadar::toggleInstance();
}

// static
void LLToolBar::onClickMap(void*)
{
	LLFloaterWorldMap::toggle(NULL);
}

// static
void LLToolBar::onClickFriends(void*)
{
	LLFloaterFriends::toggleInstance();
}

// static
void LLToolBar::onClickGroups(void*)
{
	LLFloaterGroups::toggleInstance();
}

// static
void LLToolBar::onClickInventory(void*)
{
	handle_inventory(NULL);
}
