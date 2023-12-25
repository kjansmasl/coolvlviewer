/**
 * @file llfloatersellland.h
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

#ifndef LL_LLFLOATERSELLLAND_H
#define LL_LLFLOATERSELLLAND_H

#include "llfloater.h"
#include "llsafehandle.h"

#include "llviewerparcelmgr.h"

class LLViewerRegion;

class LLFloaterSellLand final : public LLFloater,
								public LLFloaterSingleton<LLFloaterSellLand>
{
	friend class LLUISingleton<LLFloaterSellLand,
							   VisibilityPolicy<LLFloater> >;

public:
	~LLFloaterSellLand() override;

	bool postBuild() override;

	static void sellLand(LLViewerRegion* region,
						 LLSafeHandle<LLParcelSelection> parcel);

private:
	// Open only via the sellLand() method defined above
	LLFloaterSellLand(const LLSD&);

	enum Badge { BADGE_OK, BADGE_NOTE, BADGE_WARN, BADGE_ERROR };

	void updateParcelInfo();
	void refreshUI();
	void setBadge(const char* id, Badge badge);

	bool setParcel(LLViewerRegion* region, LLParcelSelectionHandle parcel);

	static void onChangeValue(LLUICtrl*, void* userdata);
	static void doSelectAgent(void* userdata);
	static void doCancel(void* userdata);
	static void doSellLand(void* userdata);
	bool onConfirmSale(const LLSD& notification, const LLSD& response);
	static void doShowObjects(void* userdata);
	static bool callbackHighlightTransferable(const LLSD& notification,
											  const LLSD& response);

	static void callbackAvatarPick(const std::vector<std::string>& names,
								   const std::vector<LLUUID>& ids, void* data);

	class SelectionObserver final : public LLParcelSelectionObserver
	{
	public:
		void changed() override;
	};

private:
	LLViewerRegion*						mRegion;
	LLParcelSelectionHandle				mParcelSelection;
	LLUUID								mParcelSnapshot;
	LLUUID								mAuthorizedBuyer;
	S32									mParcelPrice;
	S32									mParcelActualArea;
	bool								mParcelIsForSale;
	bool								mSellToBuyer;
	bool								mChoseSellTo;
	bool								mParcelSoldWithObjects;

	static LLParcelSelectionObserver*	sParcelObserver;
};

#endif // LL_LLFLOATERSELLLAND_H
