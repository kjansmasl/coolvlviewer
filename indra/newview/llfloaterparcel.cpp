/**
 * @file llfloaterparcel.cpp
 * @brief LLFloaterParcel class implementation
 * Parcel information as shown in a floating window from secondlife:// command
 * handler.
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 *
 * Copyright (c) 2007-2009, Linden Research, Inc.
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

#include "llfloaterparcel.h"

#include "lluictrlfactory.h"

#include "llcommandhandler.h"
#include "llpanelplace.h"

LLFloaterParcelInfo::instances_map_t LLFloaterParcelInfo::sInstances;

class LLParcelHandler final : public LLCommandHandler
{
public:
	LLParcelHandler()
	:	LLCommandHandler("parcel", UNTRUSTED_THROTTLE)
	{
	}

	bool handle(const LLSD& params, const LLSD&, LLMediaCtrl*) override
	{
		if (params.size() < 2)
		{
			return false;
		}
		LLUUID parcel_id;
		if (!parcel_id.set(params[0], false))
		{
			return false;
		}
		if (params[1].asString() == "about")
		{
			LLFloaterParcelInfo::show(parcel_id);
			return true;
		}
		return false;
	}
};
LLParcelHandler gParcelHandler;

void* LLFloaterParcelInfo::createPanelPlace(void* data)
{
	LLFloaterParcelInfo* self = (LLFloaterParcelInfo*)data;
	self->mPanelParcelp = new LLPanelPlace(); // allow edit self
	LLUICtrlFactory::getInstance()->buildPanel(self->mPanelParcelp,
											   "panel_place.xml");
	return self->mPanelParcelp;
}

LLFloaterParcelInfo::LLFloaterParcelInfo(const std::string& name,
										 const LLUUID& parcel_id)
:	LLFloater(name),
	mParcelID(parcel_id)
{
	mFactoryMap["place_details_panel"] = LLCallbackMap(LLFloaterParcelInfo::createPanelPlace,
													   this);
	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_preview_url.xml",
												 &getFactoryMap());
	sInstances[parcel_id] = this;
}

//virtual
LLFloaterParcelInfo::~LLFloaterParcelInfo()
{
	// Child views automatically deleted
	sInstances.erase(mParcelID);
}

void LLFloaterParcelInfo::displayParcelInfo(const LLUUID& parcel_id)
{
	mPanelParcelp->setParcelID(parcel_id);
}

//static
LLFloaterParcelInfo* LLFloaterParcelInfo::show(const LLUUID& parcel_id)
{
	if (parcel_id.isNull())
	{
		return NULL;
	}

	LLFloaterParcelInfo* floater;
	instances_map_t::iterator it = sInstances.find(parcel_id);
	if (it != sInstances.end())
	{
		// ...bring that window to front
		floater = it->second;
		floater->open();
		floater->setFrontmost(true);
	}
	else
	{
		floater = new LLFloaterParcelInfo("parcelinfo", parcel_id);
		floater->center();
		floater->open();
		floater->displayParcelInfo(parcel_id);
		floater->setFrontmost(true);
	}

	return floater;
}
