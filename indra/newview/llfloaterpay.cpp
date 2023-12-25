/**
 * @file llfloaterpay.cpp
 * @author Aaron Brashears, Kelly Washington, James Cook
 * @brief Implementation of the LLFloaterPay class.
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

#include "llfloaterpay.h"

#include "llbutton.h"
#include "llcachename.h"
#include "lllineeditor.h"
#include "lllocale.h"
#include "llnotifications.h"
#include "lltextbox.h"
#include "lltransactiontypes.h"
#include "lluictrlfactory.h"
#include "llmessage.h"

#include "llagent.h"
#include "llfloaterreporter.h"		// For OBJECT_PAY_REQUEST
#include "llmutelist.h"
#include "llselectmgr.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLGiveMoneyInfo
//
// A small class used to track callback information
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

struct LLGiveMoneyInfo
{
	LLFloaterPay* mFloater;
	S32 mAmount;

	LLGiveMoneyInfo(LLFloaterPay* floater, S32 amount)
	:	mFloater(floater),
		mAmount(amount)
	{
	}
};

///----------------------------------------------------------------------------
/// Class LLFloaterPay
///----------------------------------------------------------------------------

std::set<LLFloaterPay*> LLFloaterPay::sInstances;
S32 LLFloaterPay::sLastAmount = 0;
constexpr S32 FASTPAY_BUTTON_WIDTH = 80;

//static
void LLFloaterPay::payViaObject(money_callback callback,
								const LLUUID& object_id)
{
	LLViewerObject* object = gObjectList.findObject(object_id);
	if (!object || !object->getRegion()) return;

	LLFloaterPay* floater = new LLFloaterPay("Give L$", callback, object_id,
											 true);
	if (!floater) return;

	LLSelectNode* node = NULL;
	if (floater->mObjectSelection.notNull())
	{
		node = floater->mObjectSelection->getFirstRootNode();
	}
	if (!node)
	{
		gNotifications.add("PayObjectFailed");
		floater->close();
		return;
	}

	LLHost target_region = object->getRegion()->getHost();

	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessageFast(_PREHASH_RequestPayPrice);
	msg->nextBlockFast(_PREHASH_ObjectData);
	msg->addUUIDFast(_PREHASH_ObjectID, object_id);
	msg->sendReliable(target_region);
	msg->setHandlerFuncFast(_PREHASH_PayPriceReply, processPayPriceReply,
							(void**)floater);

	LLUUID owner_id;
	bool is_group = false;
	node->mPermissions->getOwnership(owner_id, is_group);

	floater->childSetText("object_name_text", node->mName);

	floater->finishPayUI(owner_id, is_group);
}

//static
void LLFloaterPay::payDirectly(money_callback callback,
							   const LLUUID& target_id, bool is_group)
{
	LLFloaterPay* floater = new LLFloaterPay("Give L$", callback, target_id,
											 false);
	if (!floater) return;

	floater->childSetVisible("amount", true);
	floater->childSetVisible("pay btn", true);
	floater->childSetVisible("amount text", true);

	for (S32 i = 0; i < MAX_PAY_BUTTONS; ++i)
	{
		floater->mQuickPayButton[i]->setVisible(true);
	}

	floater->finishPayUI(target_id, is_group);
}

LLFloaterPay::LLFloaterPay(const std::string& name, money_callback callback,
						   const LLUUID& uuid, bool target_is_object)
:	LLFloater(name, "FloaterPayRectB", LLStringUtil::null, RESIZE_NO,
			  DEFAULT_MIN_WIDTH, DEFAULT_MIN_HEIGHT, DRAG_ON_TOP, MINIMIZE_NO,
			  CLOSE_YES),
	mCallbackData(),
	mCallback(callback),
	mObjectNameText(NULL),
	mPayMessageText(NULL),
	mTargetUUID(uuid),
	mTargetIsObject(target_is_object),
	mTargetIsGroup(false)
{
	if (target_is_object)
	{
		LLUICtrlFactory::getInstance()->buildFloater(this,
													 "floater_pay_object.xml");
		mObjectSelection = gSelectMgr.getEditSelection();
	}
	else
	{
		LLUICtrlFactory::getInstance()->buildFloater(this, "floater_pay.xml");
	}

	sInstances.insert(this);

	S32 i = 0;

	LLGiveMoneyInfo* info = new LLGiveMoneyInfo(this, PAY_BUTTON_DEFAULT_0);
	mCallbackData.push_back(info);

	childSetAction("fastpay 1", onGive, info);
	childSetVisible("fastpay 1", false);

	mQuickPayButton[i] = getChild<LLButton>("fastpay 1");
	mQuickPayInfo[i++] = info;

	info = new LLGiveMoneyInfo(this, PAY_BUTTON_DEFAULT_1);
	mCallbackData.push_back(info);

	childSetAction("fastpay 5", onGive, info);
	childSetVisible("fastpay 5", false);

	mQuickPayButton[i] = getChild<LLButton>("fastpay 5");
	mQuickPayInfo[i++] = info;

	info = new LLGiveMoneyInfo(this, PAY_BUTTON_DEFAULT_2);
	mCallbackData.push_back(info);

	childSetAction("fastpay 10", onGive, info);
	childSetVisible("fastpay 10", false);

	mQuickPayButton[i] = getChild<LLButton>("fastpay 10");
	mQuickPayInfo[i++] = info;

	info = new LLGiveMoneyInfo(this, PAY_BUTTON_DEFAULT_3);
	mCallbackData.push_back(info);

	childSetAction("fastpay 20", onGive, info);
	childSetVisible("fastpay 20", false);

	mQuickPayButton[i] = getChild<LLButton>("fastpay 20");
	mQuickPayInfo[i++] = info;

    childSetVisible("amount text", false);

	std::string last_amount;
	if (sLastAmount > 0)
	{
		last_amount = llformat("%d", sLastAmount);
	}

	mPayMessageText = getChild<LLLineEditor>("payment_message", true, false);
	if (mPayMessageText)
	{
		mPayMessageText->setPrevalidate(LLLineEditor::prevalidateASCII);
	}

    childSetVisible("amount", false);

	childSetKeystrokeCallback("amount", onKeystroke, this);
	childSetText("amount", last_amount);
	childSetPrevalidate("amount", LLLineEditor::prevalidateNonNegativeS32);

	info = new LLGiveMoneyInfo(this, 0);
	mCallbackData.push_back(info);

	childSetAction("pay btn", onGive, info);
	setDefaultBtn("pay btn");
	childSetVisible("pay btn", false);
	childSetEnabled("pay btn", sLastAmount > 0);

	childSetAction("cancel btn", onCancel, this);

	center();
	open();
}

LLFloaterPay::~LLFloaterPay()
{
	sInstances.erase(this);
	gMessageSystemp->setHandlerFunc("PayPriceReply", NULL, NULL);
	std::for_each(mCallbackData.begin(), mCallbackData.end(), DeletePointer());
	mCallbackData.clear();
}

void LLFloaterPay::finishPayUI(const LLUUID& target_id, bool is_group)
{
	if (!gCacheNamep) return;	// Paranoia

	gCacheNamep->get(target_id, is_group,
					 boost::bind(&LLFloaterPay::onCacheOwnerName,
								 _1, _2, _3, this));

	// Make sure the amount field has focus

	childSetFocus("amount", true);

	LLLineEditor* amount = getChild<LLLineEditor>("amount");
	amount->selectAll();
	mTargetIsGroup = is_group;
}

bool LLFloaterPay::give(S32 amount)
{
	if (mCallback)
	{
		// if the amount is 0, that means that we should use the text field.
		if (amount == 0)
		{
			amount = atoi(childGetText("amount").c_str());
		}
		sLastAmount = amount;

		// Try to pay an object.
		if (mTargetIsObject)
		{
			LLViewerObject* dest_object = gObjectList.findObject(mTargetUUID);
			LLViewerRegion* region;
			if (dest_object && (region = dest_object->getRegion()))
			{
				std::string object_name;
				if (mObjectSelection.notNull())
				{
					// Find the name of the root object
					LLSelectNode* node = mObjectSelection->getFirstRootNode();
					if (node)
					{
						object_name = node->mName;
					}
					else
					{
						return false;
					}
				}

				S32 tx_type = TRANS_PAY_OBJECT;
				if (dest_object->isAvatar())
				{
					tx_type = TRANS_GIFT;
				}

				mCallback(mTargetUUID, region, amount, false, tx_type,
						  object_name);
				mObjectSelection = NULL;

				// request the object owner in order to check if the owner
				// needs to be unmuted
				LLSelectMgr::registerObjectPropertiesFamilyRequest(mTargetUUID);
				LLMessageSystem* msg = gMessageSystemp;
				msg->newMessageFast(_PREHASH_RequestObjectPropertiesFamily);
				msg->nextBlockFast(_PREHASH_AgentData);
				msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
				msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
				msg->nextBlockFast(_PREHASH_ObjectData);
				msg->addU32Fast(_PREHASH_RequestFlags, OBJECT_PAY_REQUEST);
				msg->addUUIDFast(_PREHASH_ObjectID, mTargetUUID);
				msg->sendReliable(region->getHost());
			}
			else
			{
				return false;
			}
		}
		else
		{
			// just transfer the L$
			std::string message;
			if (mPayMessageText)
			{
				message = mPayMessageText->getValue().asString();
			}
			mCallback(mTargetUUID, gAgent.getRegion(), amount, mTargetIsGroup,
					  TRANS_GIFT, message);

			// check if the payee needs to be unmuted
			LLMuteList::autoRemove(mTargetUUID, LLMuteList::AR_MONEY);
		}
	}

	return true;
}

//static
void LLFloaterPay::processPayPriceReply(LLMessageSystem* msg, void** userdata)
{
	LLFloaterPay* self = (LLFloaterPay*)userdata;
	if (self && sInstances.count(self))
	{
		S32 price;
		LLUUID target;

		msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_ObjectID, target);
		if (target != self->mTargetUUID)
		{
			// This is a message for a different object's pay info
			return;
		}

		msg->getS32Fast(_PREHASH_ObjectData, _PREHASH_DefaultPayPrice, price);

		if (price == PAY_PRICE_HIDE)
		{
			self->childSetVisible("amount", false);
			self->childSetVisible("pay btn", false);
			self->childSetVisible("amount text", false);
		}
		else if (price == PAY_PRICE_DEFAULT)
		{
			self->childSetVisible("amount", true);
			self->childSetVisible("pay btn", true);
			self->childSetVisible("amount text", true);
		}
		else
		{
			// PAY_PRICE_HIDE and PAY_PRICE_DEFAULT are negative values. So we
			// take the absolute value here after we have checked for those
			// cases.

			self->childSetVisible("amount", true);
			self->childSetVisible("pay btn", true);
			self->childSetEnabled("pay btn", true);
			self->childSetVisible("amount text", true);

			self->childSetText("amount", llformat("%d", abs(price)));
		}

		S32 num_blocks = msg->getNumberOfBlocksFast(_PREHASH_ButtonData);
		S32 i = 0;
		if (num_blocks > MAX_PAY_BUTTONS) num_blocks = MAX_PAY_BUTTONS;

		S32 max_pay_amount = 0;
		S32 padding_required = 0;

		for (i = 0; i < num_blocks; ++i)
		{
			S32 pay_button;
			msg->getS32Fast(_PREHASH_ButtonData, _PREHASH_PayButton,
							pay_button, i);
			if (pay_button > 0)
			{
				std::string button_str = "L$";
				button_str += LLLocale::getMonetaryString(pay_button);

				self->mQuickPayButton[i]->setLabelSelected(button_str);
				self->mQuickPayButton[i]->setLabelUnselected(button_str);
				self->mQuickPayButton[i]->setVisible(true);
				self->mQuickPayInfo[i]->mAmount = pay_button;

				if (pay_button > max_pay_amount)
				{
					max_pay_amount = pay_button;
				}
			}
			else
			{
				self->mQuickPayButton[i]->setVisible(false);
			}
		}

		// build a string containing the maximum value and calc nerw button
		// width from it.
		std::string balance_str = "L$";
		balance_str += LLLocale::getMonetaryString(max_pay_amount);
		static const LLFontGL* font = LLFontGL::getFontSansSerif();
		S32 new_button_width = font->getWidth(std::string(balance_str));
		new_button_width += 24;	// padding

		// dialog is sized for 2 digit pay amounts - larger pay values need to
		// be scaled
		constexpr S32 threshold = 100000;
		if (max_pay_amount >= threshold)
		{
			S32 num_digits_threshold = (S32)log10f((F32)threshold) + 1;
			S32 num_digits_max = (S32)log10f((F32)max_pay_amount) + 1;

			// Calculate the extra width required by 2 buttons with max amount
			// and some commas
			padding_required = font->getWidth(std::string("0")) *
							   (num_digits_max - num_digits_threshold +
								num_digits_max / 3);
		}

		// Change in button width
		S32 button_delta = new_button_width - FASTPAY_BUTTON_WIDTH;
		if (button_delta < 0)
		{
			button_delta = 0;
		}

		// now we know the maximum amount, we can resize all the buttons to be
		for (i = 0; i < num_blocks; ++i)
		{
			LLRect r;
			r = self->mQuickPayButton[i]->getRect();

			// RHS button colum needs to move further because LHS changed too
			if (i % 2)
			{
				r.setCenterAndSize(r.getCenterX() + (button_delta * 3) / 2 ,
								   r.getCenterY(), r.getWidth() + button_delta,
								   r.getHeight());
			}
			else
			{
				r.setCenterAndSize(r.getCenterX() + button_delta / 2,
								   r.getCenterY(), r.getWidth() + button_delta,
								   r.getHeight());
			}
			self->mQuickPayButton[i]->setRect(r);
		}

		for (i = num_blocks; i < MAX_PAY_BUTTONS; ++i)
		{
			self->mQuickPayButton[i]->setVisible(false);
		}

		self->reshape(self->getRect().getWidth() + padding_required,
					  self->getRect().getHeight(), false);
	}
	msg->setHandlerFunc("PayPriceReply", NULL, NULL);
}

//static
void LLFloaterPay::onCacheOwnerName(const LLUUID& owner_id,
									const std::string& full_name,
									bool is_group,
									LLFloaterPay* self)
{
	if 	(!self || !sInstances.count(self))
	{
		return;
	}

	if (self->mTargetIsObject)
	{
		if (is_group)
		{
			self->childSetVisible("payee_group", true);
			self->childSetVisible("payee_resident", false);
		}
		else
		{
			self->childSetVisible("payee_group", false);
			self->childSetVisible("payee_resident", true);
		}
	}

	self->childSetTextArg("payeename", "[NAME]", full_name);
}

//static
void LLFloaterPay::onCancel(void* data)
{
	LLFloaterPay* self = (LLFloaterPay*)data;
	if (self)
	{
		self->close();
	}
}

//static
void LLFloaterPay::onKeystroke(LLLineEditor*, void* data)
{
	LLFloaterPay* self = (LLFloaterPay*)data;
	if (self)
	{
		// enable the Pay button when amount is non-empty and positive, disable
		// otherwise
		std::string amtstr = self->childGetText("amount");
		self->childSetEnabled("pay btn",
							  !amtstr.empty() && atoi(amtstr.c_str()) > 0);
	}
}

//static
void LLFloaterPay::onGive(void* data)
{
	LLGiveMoneyInfo* info = (LLGiveMoneyInfo*)data;
	if (info && info->mFloater)
	{
		if (!info->mFloater->give(info->mAmount))
		{
			gNotifications.add("PayObjectFailed");
		}
		info->mFloater->close();
	}
}
