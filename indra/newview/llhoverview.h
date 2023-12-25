/**
 * @file llhoverview.h
 * @brief LLHoverView class definition
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

#ifndef LL_LLHOVERVIEW_H
#define LL_LLHOVERVIEW_H

#include <list>
#include <string>

#include "llframetimer.h"
#include "llview.h"

#include "llviewerobject.h"
#include "llviewerwindow.h"		// For LLPickInfo

class LLFontGL;
class LLParcel;
class LLTool;

class LLHoverView final : public LLView
{
public:
	LLHoverView(const LLRect& rect);
	~LLHoverView() override;

	void draw() override;

	void updateHover(LLTool* current_tool);
	void cancelHover();

	// The last hovered object is retained even after the hover is cancelled,
	// so allow it to be specifically reset. JC
	void resetLastHoverObject();

	void setHoverActive(bool active);

	// We do not do hover picks while the user is typing. In fact, we stop
	// until the mouse is moved.
	LL_INLINE void setTyping(bool b)			{ mTyping = b; }

	LL_INLINE bool isHoveringObject() const		{ return mLastHoverObject.notNull() && !mLastHoverObject->isDead(); }
	LL_INLINE bool isHoveringLand() const		{ return !mHoverLandGlobal.isExactlyZero(); }
	LL_INLINE bool isHovering() const			{ return isHoveringLand() || isHoveringObject(); }

	LLViewerObject* getLastHoverObject() const;
	LL_INLINE LLPickInfo getPickInfo()			{ return mLastPickInfo; }

	static void pickCallback(const LLPickInfo& info);

protected:
	void	updateText();

protected:
	// If not null and not dead, we're over an object.
	LLPointer<LLViewerObject>	mLastHoverObject;
	LLViewerObject*				mLastObjectWithFullText;
	LLParcel*					mLastParcelWithFullText;

	LLPickInfo					mLastPickInfo;

	LLCoordGL					mHoverPos;

	// If not LLVector3d::ZERO, we are over land.
	LLVector3d					mHoverLandGlobal;
	LLVector3					mHoverOffset;

	LLUIImagePtr				mShadowImage;

	const LLFontGL*				mFont;

	// How long has the hover popup been visible ?
	LLFrameTimer				mHoverTimer;
	LLFrameTimer				mStartHoverTimer;

	bool						mStartHoverPickTimer;
	bool						mDoneHoverPick;
	bool						mHoverActive;
	bool						mUseHover;
	bool						mTyping;

	std::string					mRetrievingData;
	std::string					mTooltipPerson;
	std::string					mTooltipNoName;
	std::string					mTooltipOwner;
	std::string					mTooltipPublic;
	std::string					mTooltipIsGroup;
	std::string					mTooltipFlagScript;
	std::string					mTooltipFlagCharacter;
	std::string					mTooltipFlagPhysics;
	std::string					mTooltipFlagPermanent;
	std::string					mTooltipFlagTouch;
	std::string					mTooltipFlagMoney;
	std::string					mTooltipFlagDropInventory;
	std::string					mTooltipFlagPhantom;
	std::string					mTooltipFlagTemporary;
	std::string					mTooltipFlagRightClickMenu;
	std::string					mTooltipFreeToCopy;
	std::string					mTooltipForSaleMsg;
	std::string					mTooltipLand;
	std::string					mTooltipFlagGroupBuild;
	std::string					mTooltipFlagNoBuild;
	std::string					mTooltipFlagNoEdit;
	std::string					mTooltipFlagNotSafe;
	std::string					mTooltipFlagNoFly;
	std::string					mTooltipFlagGroupScripts;
	std::string					mTooltipFlagNoScripts;

	typedef std::list<std::string> text_list_t;
	text_list_t					mText;

public:
	// Show in-world hover tips. Allow to turn off for movie making, game
	// playing. Public so menu can directly toggle.
	static bool					sShowHoverTips;
};

extern LLHoverView* gHoverViewp;

#endif
