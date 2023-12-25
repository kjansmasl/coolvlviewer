/**
 * @file llfloaterbuycurrency.cpp
 * @brief LLFloaterBuyCurrency class implementation
 *
 * $LicenseInfo:firstyear=2005&license=viewergpl$
 *
 * Copyright (c) 2005-2009, Linden Research, Inc.
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

#include "llfloaterbuycurrency.h"

#include "lltextbox.h"
#include "lluictrlfactory.h"

#include "llappviewer.h"
#include "llcommandhandler.h"
#include "llstatusbar.h"
#include "llweb.h"

#if LL_WINDOWS
// passing 'this' during construction generates a warning. The callee only uses
// the pointer to hold a reference to 'this' which is already valid, so this
// call does the correct thing. Disable the warning so that we can compile
// without generating a warning.
#pragma warning(disable : 4355)
#endif

///////////////////////////////////////////////////////////////////////////////
// Command handler for SLURLs in the form of:
// secondlife:///app/buycurrencyhtml/{ACTION}/{NEXT_ACTION}/{RETURN_CODE}
// Note: we do not have the HTML floater in v1 viewers (and I do not see any
// point in implementing it), so we always use the XUI-based buy currency
// floater instead. HB

class LLBuyCurrencyHandler final : public LLCommandHandler
{
protected:
	LOG_CLASS(LLBuyCurrencyHandler);

public:
	LLBuyCurrencyHandler()
	:	LLCommandHandler("buycurrencyhtml", UNTRUSTED_THROTTLE)
	{
	}

	bool handle(const LLSD& params, const LLSD&, LLMediaCtrl*) override
	{
		S32 count = params.size();

		std::string action;
		if (count)
		{
			action = params[0].asString();
		}

		// Note: we do not care about "NEXT_ACTION", because it may only be
		// "open_legacy", and we always open the legacy floater anyway... HB

		S32 result_code = 0;
		if (count >= 3)
		{
			result_code = params[2].asInteger();
			if (result_code)
			{
				llwarns <<" Received non-zero result code: " << result_code
						<< llendl;
			}
		}

		// If the requested action is anything else than "close" (which would
		// apply only to the HTML floater version of the buy currency, in LL's
		// viewer), then open the legacy floater. HB
		if (action != "close")
		{
			LLFloaterBuyCurrency::buyCurrency();
		}
		return true;
	}
};

LLBuyCurrencyHandler gBuyCurrencyHandler;

///////////////////////////////////////////////////////////////////////////////

constexpr S32 STANDARD_BUY_AMOUNT = 2000;
constexpr S32 MINIMUM_BALANCE_AMOUNT = 0;

LLFloaterBuyCurrency::LLFloaterBuyCurrency(const LLSD&)
:	mManager(*this)
{
	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_buy_currency.xml",
												 NULL, false);
}

//virtual
bool LLFloaterBuyCurrency::postBuild()
{
	mManager.prepare();

	childSetAction("buy_btn", onClickBuy, this);
	childSetAction("cancel_btn", onClickCancel, this);
	childSetAction("error_web", onClickErrorWeb, this);

	updateUI();

	center();

	return true;
}

//virtual
bool LLFloaterBuyCurrency::canClose()
{
	return mManager.canCancel();
}

//virtual
void LLFloaterBuyCurrency::draw()
{
	if (mManager.process())
	{
		if (mManager.bought())
		{
			close();
			return;
		}

		updateUI();
	}

	LLFloater::draw();
}

void LLFloaterBuyCurrency::noTarget()
{
	mHasTarget = false;
	mManager.setAmount(STANDARD_BUY_AMOUNT);
}

void LLFloaterBuyCurrency::target(const std::string& name, S32 price)
{
	mHasTarget = true;
	mTargetName = name;
	mTargetPrice = price;

	S32 balance = gStatusBarp->getBalance();
	S32 need = price - balance;
	if (need < 0)
	{
		need = 0;
	}

	mManager.setAmount(need + MINIMUM_BALANCE_AMOUNT);
}

void LLFloaterBuyCurrency::updateUI()
{
	bool has_error = mManager.hasError();
	mManager.updateUI(!has_error && !mManager.buying());

	// Section zero: title area
	{
		childSetVisible("info_buying", false);
		childSetVisible("info_cannot_buy", false);
		childSetVisible("info_need_more", false);
		if (has_error)
		{
			childSetVisible("info_cannot_buy", true);
		}
		else if (mHasTarget)
		{
			childSetVisible("info_need_more", true);
		}
		else
		{
			childSetVisible("info_buying", true);
		}
	}

	// Error section
	if (has_error)
	{
		childSetBadge("step_error", BADGE_ERROR);

		LLTextBox* message = getChild<LLTextBox>("error_message");
		if (message)
		{
			message->setVisible(true);
			message->setWrappedText(mManager.errorMessage());
		}
		mErrorURI = mManager.errorURI();
		bool has_error_uri = !mErrorURI.empty();
		childSetVisible("error_web", has_error_uri);
		if (has_error_uri)
		{
			childHide("getting_data");
		}

		mManager.clearError();
	}
	else
	{
		childHide("step_error");
		childHide("error_message");
		childHide("error_web");
	}

	// Currency
	childSetVisible("contacting", false);
	childSetVisible("buy_action", false);
	childSetVisible("buy_action_unknown", false);

	if (!has_error)
	{
		childSetBadge("step_1", BADGE_NOTE);

		if (mManager.buying())
		{
			childSetVisible("contacting", true);
		}
		else
		{
			if (mHasTarget)
			{
				childSetVisible("buy_action", true);
				childSetTextArg("buy_action", "[NAME]", mTargetName);
				childSetTextArg("buy_action", "[PRICE]",
								llformat("%d",mTargetPrice));
			}
			else
			{
				childSetVisible("buy_action_unknown", true);
			}
		}

		S32 balance = gStatusBarp->getBalance();
		childShow("balance_label");
		childShow("balance_amount");
		childSetTextArg("balance_amount", "[AMT]", llformat("%d", balance));

		S32 buying = mManager.getAmount();
		childShow("buying_label");
		childShow("buying_amount");
		childSetTextArg("buying_amount", "[AMT]", llformat("%d", buying));

		S32 total = balance + buying;
		childShow("total_label");
		childShow("total_amount");
		childSetTextArg("total_amount", "[AMT]", llformat("%d", total));

		childSetVisible("purchase_warning_repurchase", false);
		childSetVisible("purchase_warning_notenough", false);
		if (mHasTarget)
		{
			if (total >= mTargetPrice)
			{
				childSetVisible("purchase_warning_repurchase", true);
			}
			else
			{
				childSetVisible("purchase_warning_notenough", true);
			}
		}
	}
	else
	{
		childHide("step_1");
		childHide("balance_label");
		childHide("balance_amount");
		childHide("buying_label");
		childHide("buying_amount");
		childHide("total_label");
		childHide("total_amount");
		childHide("purchase_warning_repurchase");
		childHide("purchase_warning_notenough");
	}

	childSetEnabled("buy_btn", mManager.canBuy());

	if (!mManager.canBuy() && !childIsVisible("error_web"))
	{
		childShow("getting_data");
	}
}

//static
void LLFloaterBuyCurrency::onClickBuy(void* data)
{
	LLFloaterBuyCurrency* self = (LLFloaterBuyCurrency*)data;
	if (self)
	{
		self->mManager.buy(self->getString("buy_currency"));
		self->updateUI();
		// JC: updateUI() does not get called again until progress is made
		// with transaction processing, so the "Purchase" button would be
		// left enabled for some time.  Pre-emptively disable.
		self->childSetEnabled("buy_btn", false);
	}
}

//static
void LLFloaterBuyCurrency::onClickCancel(void* data)
{
	LLFloaterBuyCurrency* self = (LLFloaterBuyCurrency*)data;
	if (self)
	{
		self->close();
	}
}

//static
void LLFloaterBuyCurrency::onClickErrorWeb(void* data)
{
	LLFloaterBuyCurrency* self = (LLFloaterBuyCurrency*)data;
	if (self)
	{
		LLWeb::loadURL(self->mErrorURI);
		self->close();
	}
}

//static
void LLFloaterBuyCurrency::buyCurrency()
{
	LLFloaterBuyCurrency* self = getInstance();
	if (self)
	{
		self->noTarget();
		self->updateUI();
		self->open();
	}
}

//static
void LLFloaterBuyCurrency::buyCurrency(const std::string& name, S32 price)
{
	LLFloaterBuyCurrency* self = getInstance();
	if (self)
	{
		self->target(name, price);
		self->updateUI();
		self->open();
	}
}
