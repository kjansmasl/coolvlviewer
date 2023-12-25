/** 
 * @file llpanelland.cpp
 * @brief Land information in the tool floater, NOT the "About Land" floater
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

#include "llpanelland.h"

#include "llbutton.h"
#include "llparcel.h"
#include "lltextbox.h"
#include "lluictrlfactory.h"
#include "roles_constants.h"

#include "llagent.h"
#include "llfloaterland.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llviewercontrol.h"
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"

LLPanelLandSelectObserver* LLPanelLandInfo::sObserver = NULL;
LLPanelLandInfo* LLPanelLandInfo::sInstance = NULL;

class LLPanelLandSelectObserver final : public LLParcelSelectionObserver
{
public:
	LLPanelLandSelectObserver()				{}
	~LLPanelLandSelectObserver() override	{}

	void changed() override
	{
		LLPanelLandInfo::refreshAll();
	}
};

LLPanelLandInfo::LLPanelLandInfo(const std::string& name)
:	LLPanel(name)
{
	if (!sInstance)
	{
		sInstance = this;
	}
	if (!sObserver)
	{
		sObserver = new LLPanelLandSelectObserver();
		gViewerParcelMgr.addSelectionObserver(sObserver);
	}
}

//virtual
LLPanelLandInfo::~LLPanelLandInfo()
{
	gViewerParcelMgr.removeSelectionObserver(sObserver);
	delete sObserver;
	sObserver = NULL;
	sInstance = NULL;
}

//virtual
bool LLPanelLandInfo::postBuild()
{
	mBtnBuyLand = getChild<LLButton>("button buy land");
	mBtnBuyLand->setClickedCallback(onClickClaim, this);

	mBtnAbandonLand = getChild<LLButton>("button abandon land");
	mBtnAbandonLand->setClickedCallback(onClickRelease, this);

	mBtnDivideLand = getChild<LLButton>("button subdivide land");
	mBtnDivideLand->setClickedCallback(onClickDivide, this);

	mBtnJoinLand = getChild<LLButton>("button join land");
	mBtnJoinLand->setClickedCallback(onClickJoin, this);

	mBtnAboutLand = getChild<LLButton>("button about land");
	mBtnAboutLand->setClickedCallback(onClickAbout, this);

	childSetAction("button show owners help", onShowOwnersHelp, this);

	mTextLabelPrice = getChild<LLTextBox>("label_area_price");
	mTextPrice = getChild<LLTextBox>("label_area");

	return true;
}

//virtual
void LLPanelLandInfo::refresh()
{
	LLParcel* parcel = gViewerParcelMgr.getParcelSelection()->getParcel();
	LLViewerRegion* regionp = gViewerParcelMgr.getSelectionRegion();

	if (!parcel || !regionp)
	{
		// nothing selected, disable panel
		mTextLabelPrice->setVisible(false);
		mTextPrice->setVisible(false);

		mBtnBuyLand->setEnabled(false);
		mBtnAbandonLand->setEnabled(false);
		mBtnDivideLand->setEnabled(false);
		mBtnJoinLand->setEnabled(false);
		mBtnAboutLand->setEnabled(false);
	}
	else
	{
		// something selected, hooray!
		const LLUUID& owner_id = parcel->getOwnerID();
		const LLUUID& auth_buyer_id = parcel->getAuthorizedBuyerID();

		bool is_public = parcel->isPublic();
		bool is_for_sale = parcel->getForSale() &&
						   (parcel->getSalePrice() > 0 ||
							auth_buyer_id.notNull());
		bool can_buy = is_for_sale && owner_id != gAgentID &&
					   (auth_buyer_id == gAgentID || auth_buyer_id.isNull());
			
		if (is_public)
		{
			mBtnBuyLand->setEnabled(true);
		}
		else
		{
			mBtnBuyLand->setEnabled(can_buy);
		}

		bool owner_release =
			LLViewerParcelMgr::isParcelOwnedByAgent(parcel, GP_LAND_RELEASE);
		bool owner_divide =
			LLViewerParcelMgr::isParcelOwnedByAgent(parcel,
													GP_LAND_DIVIDE_JOIN);

		bool manager_releaseable = gAgent.canManageEstate() &&
								   parcel->getOwnerID() == regionp->getOwner();
		
		bool manager_divideable =
			gAgent.canManageEstate() &&
			(parcel->getOwnerID() == regionp->getOwner() || owner_divide);

		mBtnAbandonLand->setEnabled(owner_release || manager_releaseable ||
									gAgent.isGodlike());

		// Only mainland sims are subdividable by owner
		if (regionp->getRegionFlag(REGION_FLAGS_ALLOW_PARCEL_CHANGES))
		{
			mBtnDivideLand->setEnabled(owner_divide || manager_divideable ||
									   gAgent.isGodlike());
		}
		else
		{
			mBtnDivideLand->setEnabled(manager_divideable ||
									   gAgent.isGodlike());
		}
		
		// To join land, must have something selected, not just a single unit
		// of land, you must own part of it and it must not be a whole parcel.
		if (gViewerParcelMgr.getSelectedArea() > PARCEL_UNIT_AREA &&
#if 0
			gViewerParcelMgr.getSelfCount() > 1 &&
#endif
			!gViewerParcelMgr.getParcelSelection()->getWholeParcelSelected())
		{
			mBtnJoinLand->setEnabled(true);
		}
		else
		{
			LL_DEBUGS("Land") << "Invalid selection for joining land"
							  << LL_ENDL;
			mBtnJoinLand->setEnabled(false);
		}

		mBtnAboutLand->setEnabled(true);

		// show pricing information
		S32 area;
		S32 claim_price;
		S32 rent_price;
		bool for_sale;
		F32 dwell;
		gViewerParcelMgr.getDisplayInfo(&area, &claim_price, &rent_price,
										&for_sale, &dwell);
		if (is_public ||
			(is_for_sale &&
			 gViewerParcelMgr.getParcelSelection()->getWholeParcelSelected()))
		{
			mTextLabelPrice->setTextArg("[PRICE]",
										llformat("%d", claim_price));
			mTextLabelPrice->setTextArg("[AREA]", llformat("%d", area));
			mTextLabelPrice->setVisible(true);
			mTextPrice->setVisible(false);
		}
		else
		{
			mTextLabelPrice->setVisible(false);
			mTextPrice->setTextArg("[AREA]", llformat("%d", area));
			mTextPrice->setVisible(true);
		}
	}
}

// static
void LLPanelLandInfo::refreshAll()
{
	if (sInstance)
	{
		sInstance->refresh();
	}
}

//static
void LLPanelLandInfo::onClickClaim(void*)
{
//MK
	if (gRLenabled && gRLInterface.mContainsShowloc)
	{
		return;
	}
//mk
	gViewerParcelMgr.startBuyLand();
}

//static
void LLPanelLandInfo::onClickRelease(void*)
{
	gViewerParcelMgr.startReleaseLand();
}

// static
void LLPanelLandInfo::onClickDivide(void*)
{
	gViewerParcelMgr.startDivideLand();
}

// static
void LLPanelLandInfo::onClickJoin(void*)
{
	gViewerParcelMgr.startJoinLand();
}

//static
void LLPanelLandInfo::onClickAbout(void*)
{
	// Promote the rectangle selection to a parcel selection
	if (!gViewerParcelMgr.getParcelSelection()->getWholeParcelSelected())
	{
		gViewerParcelMgr.selectParcelInRectangle();
	}

//MK
	if (gRLenabled && gRLInterface.mContainsShowloc)
	{
		return;
	}
//mk
	LLFloaterLand::showInstance();
}

void LLPanelLandInfo::onShowOwnersHelp(void*)
{
	gNotifications.add("ShowOwnersHelp");
}
