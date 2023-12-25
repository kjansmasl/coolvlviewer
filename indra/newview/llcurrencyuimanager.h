/**
 * @file llcurrencyuimanager.h
 * @brief LLCurrencyUIManager class definition
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

#ifndef LL_LLCURRENCYUIMANAGER_H
#define LL_LLCURRENCYUIMANAGER_H

class LLPanel;

// Manages the currency purchase portion of any dialog takes control of, and
// assumes repsonsibility for several fields:
// - 'currency_action': the text "Buy L$" before the entry field.
// - 'currency_amt': the line editor for the entry amount.
// - 'currency_est': the estimated cost from the web site.
class LLCurrencyUIManager
{
public:
	LLCurrencyUIManager(LLPanel& parent);
	virtual ~LLCurrencyUIManager();

	// The amount in L$ to purchase; setting it overwrites the user's entry
	// if no_estimate is true, then no web request is made.
	void setAmount(S32 amount, bool no_estimate = false);
	S32 getAmount();

	// Sets the gray message to show when zero
	void setZeroMessage(const std::string& message);

	// The amount in US$ * 100 (in otherwords, in cents).
	void setEstimate(S32 amount);
	// Use set when you get this information from elsewhere.
	S32 getEstimate();

	// Call once after dialog is built, from postBuild()
	void prepare();

	// Updates all UI elements, if show is false, they are all set not visible.
	// Normally, this is done automatically, but you can force it. The show/
	// hidden state is remembered.
	void updateUI(bool show = true);

	// Call periodically, for example, from draw(). Returns true if the UI
	// needs to be updated
	bool process();

	// Call to initiate the purchase
	void buy(const std::string& buy_msg);

	bool inProcess();	// Is a transaction in process ?
	bool canCancel();	// Can we cancel it (by destructing this object) ?
	bool canBuy();		// Can the user choose to buy now ?
	bool buying();		// Are we in the process of buying ?
	bool bought();		// Did the buy() transaction complete successfully ?

	void clearError();
	bool hasError();
	std::string errorMessage();
	// Error information for the user, the URI may be blank the technical
	// error details will have already been logged
	std::string errorURI();

private:
	class Impl;
	Impl& impl;
};

#endif	// LL_LLCURRENCYUIMANAGER_H
