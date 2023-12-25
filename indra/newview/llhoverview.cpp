/**
 * @file llhoverview.cpp
 * @brief LLHoverView class implementation
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

#include "llhoverview.h"

#include "llcachename.h"
#include "llfontgl.h"
#include "llgl.h"
#include "llparcel.h"
#include "llpermissions.h"
#include "llrender.h"
#include "lltrans.h"

#include "llagent.h"
#include "lldrawable.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llselectmgr.h"
#include "lltoolmgr.h"
#include "lltoolpie.h"
#include "lltoolselectland.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerobjectlist.h"
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"
#include "llviewertexturelist.h"

//
// Constants
//
const char* DEFAULT_DESC = "(No Description)";
constexpr F32 DELAY_BEFORE_SHOW_TIP = 0.35f;
constexpr F32 MAX_HOVER_DISPLAY_SECS = 5.f;

// Globals

// Instance created in LLViewerWindow::initWorldUI()
LLHoverView* gHoverViewp = NULL;

// Statics
bool LLHoverView::sShowHoverTips = true;

// Member functions

LLHoverView::LLHoverView(const LLRect& rect)
:	LLView("hover view", rect, false),
	mLastObjectWithFullText(NULL),
	mLastParcelWithFullText(NULL),
	mDoneHoverPick(false),
	mStartHoverPickTimer(false),
	mHoverActive(false),
	mUseHover(false),
	mTyping(false)
{
	mHoverOffset.clear();

	mRetrievingData = LLTrans::getString("RetrievingData");
	mTooltipPerson = LLTrans::getString("TooltipPerson");
	mTooltipNoName = LLTrans::getString("TooltipNoName");
	mTooltipOwner = LLTrans::getString("TooltipOwner");
	mTooltipPublic = LLTrans::getString("TooltipPublic");
	mTooltipIsGroup = LLTrans::getString("TooltipIsGroup");
	mTooltipFlagScript = LLTrans::getString("TooltipFlagScript");
	mTooltipFlagCharacter = LLTrans::getString("TooltipFlagCharacter");
	mTooltipFlagPhysics = LLTrans::getString("TooltipFlagPhysics");
	mTooltipFlagPermanent = LLTrans::getString("TooltipFlagPermanent");
	mTooltipFlagTouch = LLTrans::getString("TooltipFlagTouch");
	mTooltipFlagMoney = LLTrans::getString("TooltipFlagL$");
	mTooltipFlagDropInventory = LLTrans::getString("TooltipFlagDropInventory");
	mTooltipFlagPhantom = LLTrans::getString("TooltipFlagPhantom");
	mTooltipFlagTemporary = LLTrans::getString("TooltipFlagTemporary");
	mTooltipFlagRightClickMenu = LLTrans::getString("TooltipFlagRightClickMenu");
	mTooltipFreeToCopy = LLTrans::getString("TooltipFreeToCopy");
	mTooltipForSaleMsg = LLTrans::getString("TooltipForSaleMsg") + mRetrievingData;
	mTooltipLand = LLTrans::getString("TooltipLand");
	mTooltipFlagGroupBuild = LLTrans::getString("TooltipFlagGroupBuild");
	mTooltipFlagNoBuild = LLTrans::getString("TooltipFlagNoBuild");
	mTooltipFlagNoEdit = LLTrans::getString("TooltipFlagNoEdit");
	mTooltipFlagNotSafe = LLTrans::getString("TooltipFlagNotSafe");
	mTooltipFlagNoFly = LLTrans::getString("TooltipFlagNoFly");
	mTooltipFlagGroupScripts = LLTrans::getString("TooltipFlagGroupScripts");
	mTooltipFlagNoScripts = LLTrans::getString("TooltipFlagNoScripts");

	mShadowImage = LLUI::getUIImage("rounded_square_soft.tga");
	if (mShadowImage.isNull())
	{
		llerrs << "Missing shadow image !" << llendl;
	}
	mFont = LLFontGL::getFontSansSerifSmall();

	llinfos << "Hover-view initialized." << llendl;
}

LLHoverView::~LLHoverView()
{
	gHoverViewp = NULL;
	llinfos << "Hover-view destroyed." << llendl;
}

void LLHoverView::updateHover(LLTool* current_tool)
{
	bool picking_tool = current_tool == &gToolPie ||
						current_tool == &gToolSelectLand;
	mUseHover = picking_tool && !gAgent.cameraMouselook() && !mTyping;
	if (!mUseHover)
	{
		return;
	}

	if (LLViewerWindow::getMouseVelocityStat().getPrev(0) < 0.01f &&
		LLViewerCamera::getAngularVelocityStat().getPrev(0) < 0.01f &&
		LLViewerCamera::getVelocityStat().getPrev(0) < 0.01f)
	{
		if (!mStartHoverPickTimer)
		{
			mStartHoverTimer.reset();
			mStartHoverPickTimer = true;
			// Clear the existing text so that we do not briefly show the wrong
			// data.
			mText.clear();
		}

		if (mDoneHoverPick)
		{
			// Just update the hover data
			updateText();
		}
		else if (mStartHoverTimer.getElapsedTimeF32() > DELAY_BEFORE_SHOW_TIP)
		{
			gViewerWindowp->pickAsync(gViewerWindowp->getCurrentMouseX(),
									  gViewerWindowp->getCurrentMouseY(),
									  0, pickCallback);
		}
	}
	else
	{
		cancelHover();
	}
}

void LLHoverView::pickCallback(const LLPickInfo& pick_info)
{
	if (!gHoverViewp) return;

	gHoverViewp->mLastPickInfo = pick_info;
	LLViewerObject* hit_obj = pick_info.getObject();

	if (hit_obj)
	{
		gHoverViewp->setHoverActive(true);
		gSelectMgr.setHoverObject(hit_obj, pick_info.mObjectFace);
		gHoverViewp->mLastHoverObject = hit_obj;
		gHoverViewp->mHoverOffset = pick_info.mObjectOffset;
	}
	else
	{
		gHoverViewp->mLastHoverObject = NULL;
	}

	// We did not hit an object, but we did hit land.
	if (!hit_obj && pick_info.mPosGlobal != LLVector3d::zero)
	{
		gHoverViewp->setHoverActive(true);
		gHoverViewp->mHoverLandGlobal = pick_info.mPosGlobal;
		gViewerParcelMgr.setHoverParcel(gHoverViewp->mHoverLandGlobal);
	}
	else
	{
		gHoverViewp->mHoverLandGlobal.clear();
	}

	gHoverViewp->mDoneHoverPick = true;
}

void LLHoverView::cancelHover()
{
	mStartHoverTimer.reset();
	mDoneHoverPick = false;
	mStartHoverPickTimer = false;

	gSelectMgr.setHoverObject(NULL);
#if 0	// Cannot do this, some code relies on hover object still being set
		// after the hover is cancelled !  Dammit. JC
	mLastHoverObject = NULL;
#endif

	setHoverActive(false);
}

void LLHoverView::resetLastHoverObject()
{
	mLastHoverObject = NULL;
	mLastObjectWithFullText = NULL;
	mLastParcelWithFullText = NULL;
}

void LLHoverView::updateText()
{
	LLViewerObject* hit_object = getLastHoverObject();
	if (hit_object && hit_object == mLastObjectWithFullText)
	{
		// mText is already up to date
		return;
	}

	mLastObjectWithFullText = NULL;
	std::string line;
	bool text_complete = true;
	if (hit_object)
	{
		mLastParcelWithFullText = NULL;
		mText.clear();
		if (hit_object->isHUDAttachment())
		{
			// No hover tips for HUD elements, since they can obscure what the
			// HUD is displaying
			mLastObjectWithFullText = hit_object;
			return;
		}

		if (hit_object->isAttachment())
		{
			// Get root of attachment then parent, which is avatar
			LLViewerObject* root_edit = hit_object->getRootEdit();
			if (!root_edit)
			{
				// Strange parenting issue, don't show any text
				return;
			}
			hit_object = (LLViewerObject*)root_edit->getParent();
			if (!hit_object)
			{
				// Another strange parenting issue, bail out
				return;
			}
		}

		line.clear();
		if (hit_object->isAvatar())
		{
			LLNameValue* title = hit_object->getNVPair("Title");
			LLNameValue* firstname = hit_object->getNVPair("FirstName");
			LLNameValue* lastname =  hit_object->getNVPair("LastName");
			if (firstname && lastname)
			{
				std::string complete_name = firstname->getString();
				std::string last = lastname->getString();
				if (!LLAvatarName::sOmitResidentAsLastName ||
					last != "Resident")
				{
					complete_name += " " + last;
				}
				else
				{
					text_complete = false;
				}

				if (LLAvatarNameCache::useDisplayNames())
				{
					LLAvatarName avatar_name;
					if (LLAvatarNameCache::get(hit_object->getID(),
											   &avatar_name))
					{
						if (LLAvatarNameCache::useDisplayNames() == 2)
						{
							complete_name = avatar_name.mDisplayName;
						}
						else
						{
							complete_name = avatar_name.getNames();
						}
					}
					else
					{
						text_complete = false;
					}
				}

				if (title)
				{
					line.append(title->getString());
					line.append(1, ' ');
				}
				line += complete_name;
			}
			else
			{
				line.append(mTooltipPerson);
				text_complete = false;
			}
//MK
			if (gRLenabled &&
				(gRLInterface.mContainsShownames ||
				 gRLInterface.mContainsShownametags))
			{
				line.clear();
				line.append(firstname->getString());
				line.append(1, ' ');
				line.append(lastname->getString());
				line = gRLInterface.getDummyName(line);
			}
//mk
			mText.emplace_back(line);
		}
		else
		{
			// We have hit a regular object (not an avatar or attachment)

			// Default prefs will suppress display unless the object is
			// interactive
			static LLCachedControl<bool> show_all_tip(gSavedSettings,
													  "ShowAllObjectHoverTip");
			bool suppress_tip = !show_all_tip;

			LLSelectNode* nodep = gSelectMgr.getHoverNode();
			if (nodep)
			{
				if (nodep->mName.empty())
				{
					line = mTooltipNoName;
					text_complete = false;
				}
				else
				{
					line = nodep->mName;
				}
				mText.emplace_back(line);

				if (!nodep->mDescription.empty() &&
					nodep->mDescription != DEFAULT_DESC)
				{
					mText.emplace_back(nodep->mDescription);
				}

				// Line: "Owner: James Linden"
				line = mTooltipOwner;

				if (nodep->mValid)
				{
					LLUUID owner;
					std::string name;
					if (!nodep->mPermissions->isGroupOwned())
					{
						owner = nodep->mPermissions->getOwner();
						if (LLUUID::null == owner)
						{
							line.append(" " + mTooltipPublic);
						}
						else if (gCacheNamep &&
								 gCacheNamep->getFullName(owner, name))
						{
//MK
							if (gRLenabled &&
								(gRLInterface.mContainsShownames ||
								 gRLInterface.mContainsShownametags))
							{
								name = gRLInterface.getDummyName(name);
							}
//mk
							line.append(" " + name);
						}
						else
						{
							line.append(" " + mRetrievingData);
							text_complete = false;
						}
					}
					else
					{
						std::string name;
						owner = nodep->mPermissions->getGroup();
						if (gCacheNamep &&
							gCacheNamep->getGroupName(owner, name))
						{
							line.append(" " + name + " " + mTooltipIsGroup);
						}
						else
						{
							line.append(" " + mRetrievingData);
							text_complete = false;
						}
					}
				}
				else
				{
					line.append(" " + mRetrievingData);
					text_complete = false;
				}
				mText.emplace_back(line);

				// Build a line describing any special properties of this object.
				LLViewerObject* object = hit_object;
				LLViewerObject* parent = object ? (LLViewerObject*)object->getParent()
												: NULL;
				if (object)
				{
					bool permanent = object->flagObjectPermanent() ||
									 (parent && parent->flagObjectPermanent());
					bool character = object->flagCharacter() ||
									 (parent && parent->flagCharacter());
					bool handle_touch = object->flagHandleTouch() ||
										(parent && parent->flagHandleTouch());
					bool takes_money = object->flagTakesMoney() ||
									   (parent && parent->flagTakesMoney());
					if (permanent || character || handle_touch ||
						takes_money || object->flagUsePhysics() ||
						object->flagScripted() || object->flagPhantom() ||
						object->flagAllowInventoryAdd() ||
						object->flagTemporaryOnRez())
					{
						line.clear();
						if (object->flagScripted())
						{
							line.append(mTooltipFlagScript);
						}

						if (character)
						{
							if (!line.empty()) line.append(" ");
							line.append(mTooltipFlagCharacter);
							suppress_tip = false;		//  Show tip
						}

						if (object->flagUsePhysics())
						{
							if (!line.empty()) line.append(" ");
							line.append(mTooltipFlagPhysics);
						}

						if (permanent)
						{
							if (!line.empty()) line.append(" ");
							line.append(mTooltipFlagPermanent);
						}

						if (handle_touch)
						{
							if (!line.empty()) line.append(" ");
							line.append(mTooltipFlagTouch);
							suppress_tip = false;		//  Show tip
						}

						if (takes_money)
						{
							if (!line.empty()) line.append(" ");
							line.append(mTooltipFlagMoney);
							suppress_tip = false;		//  Show tip
						}

						if (object->flagAllowInventoryAdd())
						{
							if (!line.empty()) line.append(" ");
							line.append(mTooltipFlagDropInventory);
							suppress_tip = false;		//  Show tip
						}

						if (object->flagPhantom())
						{
							if (!line.empty()) line.append(" ");
							line.append(mTooltipFlagPhantom);
						}

						if (object->flagTemporaryOnRez())
						{
							if (!line.empty()) line.append(" ");
							line.append(mTooltipFlagTemporary);
						}

						if (!line.empty())
						{
							mText.emplace_back(line);
						}

						if (object->flagUsePhysics() || handle_touch)
						{
							line = mTooltipFlagRightClickMenu;
							mText.emplace_back(line);
						}
					}
				}
				else
				{
					text_complete = false;
				}

				// Free to copy / For Sale: L$
				line.clear();
				if (nodep->mValid)
				{
					bool for_copy = (nodep->mPermissions->getMaskEveryone() & PERM_COPY) &&
									 object->permCopy();
					bool for_sale = nodep->mSaleInfo.isForSale() &&
									(nodep->mPermissions->getMaskOwner() & PERM_TRANSFER) &&
									 ((nodep->mPermissions->getMaskOwner() & PERM_COPY) ||
									   nodep->mSaleInfo.getSaleType() != LLSaleInfo::FS_COPY);
					if (for_copy)
					{
						line.append(mTooltipFreeToCopy);
						suppress_tip = false;		//  Show tip
					}
					else if (for_sale)
					{
						LLStringUtil::format_map_t args;
						args["[AMOUNT]"] = llformat("%d",
													nodep->mSaleInfo.getSalePrice());
						line.append(LLTrans::getString("TooltipForSaleL$", args));
						suppress_tip = false;		//  Show tip
					}
#if 0				// Nothing if not for sale
					else
					{
						line.append("Not for sale");
					}
#endif
				}
				else
				{
					line.append(mTooltipForSaleMsg);
					text_complete = false;
				}
				if (!line.empty())
				{
					mText.emplace_back(line);
				}
			}

			//  If the hover tip shouldn't be shown, delete all the object text
			if (suppress_tip)
			{
				mText.clear();
			}
		}

		if (text_complete)
		{
			mLastObjectWithFullText = hit_object;
		}
	}
	else if (!mHoverLandGlobal.isExactlyZero())
	{
		// Did not hit an object, but since we have a land point we must be
		// hovering over land.

		//  Do not show hover for land unless prefs are set to allow it.
		static LLCachedControl<bool> show_land_hover_tip(gSavedSettings,
														 "ShowLandHoverTip");
		if (!show_land_hover_tip)
		{
			mText.clear();
			return;
		}

		LLParcel* hover_parcel = gViewerParcelMgr.getHoverParcel();
		if (hover_parcel && hover_parcel == mLastParcelWithFullText)
		{
			// mText is already up to date
			return;
		}

		mLastParcelWithFullText = NULL;
		mText.clear();

		LLUUID owner;
		if (hover_parcel)
		{
			owner = hover_parcel->getOwnerID();
		}

		// Line: "Land"
		line = mTooltipLand;
		if (hover_parcel)
		{
			line.append(" " + hover_parcel->getName());
			mText.emplace_back(line);
		}

		// Line: "Owner: James Linden"
		line = mTooltipOwner + " ";

		if (hover_parcel)
		{
			std::string name;
			if (LLUUID::null == owner)
			{
				line.append(mTooltipPublic);
			}
			else if (hover_parcel->getIsGroupOwned())
			{
				if (gCacheNamep && gCacheNamep->getGroupName(owner, name))
				{
					line.append(name + mTooltipIsGroup);
				}
				else
				{
					line.append(mRetrievingData);
					text_complete = false;
				}
			}
			else if (gCacheNamep && gCacheNamep->getFullName(owner, name))
			{
				line.append(name);
			}
			else
			{
				line.append(mRetrievingData);
				text_complete = false;
			}
		}
		else
		{
			line.append(mRetrievingData);
			text_complete = false;
		}
		mText.emplace_back(line);

		// Line: "no fly, not safe, no build"

		// Do not display properties for your land. This is just confusing,
		// because you can do anything on your own land.
		if (hover_parcel && owner != gAgentID)
		{
			S32 words = 0;

			line.clear();
			// JC - Keep this in the same order as the checkboxes
			// on the land info panel
			if (!hover_parcel->getAllowModify())
			{
				if (hover_parcel->getAllowGroupModify())
				{
					line.append(mTooltipFlagGroupBuild);
				}
				else
				{
					line.append(mTooltipFlagNoBuild);
				}
				++words;
			}

			if (!hover_parcel->getAllowTerraform())
			{
				if (words) line.append(", ");
				line.append(mTooltipFlagNoEdit);
				++words;
			}

			if (hover_parcel->getAllowDamage())
			{
				if (words) line.append(", ");
				line.append(mTooltipFlagNotSafe);
				++words;
			}

			// Maybe we should reflect the estate block fly bit here as well ?
			// DK 12/1/04
			if (!hover_parcel->getAllowFly())
			{
				if (words) line.append(", ");
				line.append(mTooltipFlagNoFly);
				++words;
			}

			if (!hover_parcel->getAllowOtherScripts())
			{
				if (words) line.append(", ");
				if (hover_parcel->getAllowGroupScripts())
				{
					line.append(mTooltipFlagGroupScripts);
				}
				else
				{
					line.append(mTooltipFlagNoScripts);
				}

				++words;
			}

			if (words)
			{
				mText.emplace_back(line);
			}
		}

#if 0
		// Line: "Size: 1x4"
		// Only show for non-public land
		if (hover_parcel && LLUUID::null != owner)
		{
			line = llformat("Size: %dx%d", width, height);
			mText.emplace_back(line);
		}
#endif

		if (hover_parcel && hover_parcel->getParcelFlag(PF_FOR_SALE))
		{
			LLStringUtil::format_map_t args;
			args["[AMOUNT]"] = llformat("%d", hover_parcel->getSalePrice());
			line = LLTrans::getString("TooltipForSaleL$", args);
			mText.emplace_back(line);
		}

		if (text_complete)
		{
			mLastParcelWithFullText = hover_parcel;
		} 
	}
}

void LLHoverView::draw()
{
	// To toggle off hover tips, you have to just suppress the draw. The
	// picking is still needed to do cursor changes over physical and scripted
	// objects. JC
	if (!isHovering() ||
		!sShowHoverTips ||
		mHoverTimer.getElapsedTimeF32() > MAX_HOVER_DISPLAY_SECS)
	{
		return;
	}

	F32 alpha = 1.f;
	if (mHoverActive)
	{
		if (isHoveringObject())
		{
			// Look at object
			LLViewerObject* hover_object = getLastHoverObject();
			if (hover_object->isAvatar())
			{
				gAgent.setLookAt(LOOKAT_TARGET_HOVER, getLastHoverObject(),
								 LLVector3::zero);
			}
			else
			{
				gAgent.setLookAt(LOOKAT_TARGET_HOVER, getLastHoverObject(),
								 mHoverOffset);
			}
		}
	}
	else
	{
		constexpr F32 MAX_ALPHA = 0.9f;
		alpha = llmax(0.f, MAX_ALPHA - mHoverTimer.getElapsedTimeF32() * 2.f);
	}

	// Bail out if no text to display
	if (mText.empty())
	{
		return;
	}

	// Don't draw if no alpha
	if (alpha <= 0.f)
	{
		return;
	}

	// Render text.

	static LLCachedControl<LLColor4U> tool_tip_text_color(gColors,
														  "ToolTipTextColor");
	LLColor4 text_color = LLColor4(tool_tip_text_color);
	static LLCachedControl<LLColor4U> tool_tip_bg_color(gColors,
														"ToolTipBgColor");
	LLColor4 bg_color = LLColor4(tool_tip_bg_color);
	LLColor4 shadow_color = LLUI::sColorDropShadow;

#if 0	// Could decrease the alpha here. JC
	static LLCachedControl<LLColor4U> border_color(gColors,
												   "ToolTipBorderColor");
	text_color.mV[VALPHA] = alpha;
	border_color.mV[VALPHA] = alpha;
	bg_color.mV[VALPHA] = alpha;
#endif

	S32 max_width = 0;
	S32 num_lines = mText.size();
	text_list_t::iterator text_end = mText.end();
	for (text_list_t::iterator iter = mText.begin(); iter != text_end; ++iter)
	{
		max_width = llmax(max_width, (S32)mFont->getWidth(*iter));
	}

	S32 left	= mHoverPos.mX + 10;
	S32 top		= mHoverPos.mY - 16;
	S32 right	= mHoverPos.mX + max_width + 30;
	S32 bottom	= mHoverPos.mY - 24 - llfloor(num_lines * mFont->getLineHeight());

	// Push down if there's a one-click icon
	if (mHoverActive && isHoveringObject() &&
		mLastHoverObject->getClickAction() != CLICK_ACTION_NONE)
	{
		constexpr S32 CLICK_OFFSET = 10;
		top -= CLICK_OFFSET;
		bottom -= CLICK_OFFSET;
	}

	// Make sure the rect is completely visible
	LLRect old_rect = getRect();
	setRect(LLRect(left, top, right, bottom));
	translateIntoRect(gViewerWindowp->getVirtualWindowRect(), false);
	left = getRect().mLeft;
	top = getRect().mTop;
	right = getRect().mRight;
	bottom = getRect().mBottom;
	setRect(old_rect);

	LLGLSUIDefault gls_ui;

	S32 shadow_offset = LLUI::sDropShadowTooltip;
	shadow_color.mV[VALPHA] = 0.7f * alpha;
	mShadowImage->draw(LLRect(left + shadow_offset, top - shadow_offset,
							  right + shadow_offset, bottom - shadow_offset),
					   shadow_color);

	bg_color.mV[VALPHA] = alpha;
	LLUIImage::sRoundedSquare->draw(LLRect(left, top, right, bottom),
									bg_color);

	S32 cur_offset = top - 4;
	for (text_list_t::iterator iter = mText.begin(); iter != text_end; ++iter)
	{
		mFont->renderUTF8(*iter, 0, left + 10, cur_offset,
						  text_color, LLFontGL::LEFT, LLFontGL::TOP);
		cur_offset -= llfloor(mFont->getLineHeight());
	}
}

void LLHoverView::setHoverActive(bool active)
{
	if (active != mHoverActive)
	{
		mHoverTimer.reset();
	}

	mHoverActive = active;
	if (active)
	{
		mHoverPos = gViewerWindowp->getCurrentMouse();
	}
	else
	{
		mLastObjectWithFullText = NULL;
		mLastParcelWithFullText = NULL;
	}
}

LLViewerObject* LLHoverView::getLastHoverObject() const
{
	if (mLastHoverObject.notNull() && !mLastHoverObject->isDead())
	{
		return mLastHoverObject;
	}
	else
	{
		return NULL;
	}
}
