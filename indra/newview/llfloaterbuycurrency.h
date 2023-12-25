/**
 * @file llfloaterbuycurrency.h
 * @brief LLFloaterBuyCurrency class definition
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

#ifndef LL_LLFLOATERBUYCURRENCY_H
#define LL_LLFLOATERBUYCURRENCY_H

#include "llfloater.h"

#include "llcurrencyuimanager.h"

class LLFloaterBuyCurrency final
:	public LLFloater,
	public LLFloaterSingleton<LLFloaterBuyCurrency>
{
	friend class LLUISingleton<LLFloaterBuyCurrency,
							   VisibilityPolicy<LLFloater> >;

public:
	bool postBuild() override;

	void draw() override;
	bool canClose() override;

	static void buyCurrency();

	// 'name' should be a noun phrase of the object or service being bought:
	//	"That object costs"
	//	"Trying to give"
	//	"Uploading costs"
	// A space and the price will be appended.
	static void buyCurrency(const std::string& name, S32 price);

private:
	// Open only via the buyCurrency() methods defined above
	LLFloaterBuyCurrency(const LLSD&);

	void updateUI();

	void noTarget();
	void target(const std::string& name, S32 price);

	static void onClickBuy(void* data);
	static void onClickCancel(void* data);
	static void onClickErrorWeb(void* data);

private:
	LLCurrencyUIManager	mManager;
	std::string			mErrorURI;
	std::string			mTargetName;
	S32					mTargetPrice;
	bool				mHasTarget;
};

#endif
