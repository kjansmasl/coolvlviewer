/**
 * @file llfloaterevent.cpp
 * @brief Event information as shown in a floating window from
 * secondlife:// command handler.
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

#include "llfloaterevent.h"

#include "lluictrlfactory.h"

#include "llcommandhandler.h"
#include "llpanelevent.h"

////////////////////////////////////////////////////////////////////////////
// LLFloaterEventInfo

//-----------------------------------------------------------------------------
// Globals
//-----------------------------------------------------------------------------

LLFloaterEventInfo::instances_map_t LLFloaterEventInfo::sInstances;

class LLEventHandler final : public LLCommandHandler
{
public:
	LLEventHandler()
	:	LLCommandHandler("event", UNTRUSTED_THROTTLE)
	{
	}

	bool handle(const LLSD& tokens, const LLSD&, LLMediaCtrl*) override
	{
		if (tokens.size() < 2)
		{
			return false;
		}
		U32 event_id = tokens[0].asInteger();
		std::string info_type = tokens[1].asString();
		if (info_type == "about" || info_type == "details")
		{
			LLFloaterEventInfo::show(event_id);
			return true;
		}
		return false;
	}
};
LLEventHandler gEventHandler;

LLFloaterEventInfo::LLFloaterEventInfo(const std::string& name, U32 event_id)
:	LLFloater(name),
	mEventID(event_id)
{

	mFactoryMap["event_details_panel"] = LLCallbackMap(LLFloaterEventInfo::createEventDetail,
													  this);
	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_preview_event.xml",
												 &getFactoryMap());
	sInstances[event_id] = this;
}

LLFloaterEventInfo::~LLFloaterEventInfo()
{
	// child views automatically deleted
	sInstances.erase(mEventID);
}

void LLFloaterEventInfo::displayEventInfo(U32 event_id)
{
	mPanelEventp->setEventID(event_id);
	this->setFrontmost(true);
}

// static
void* LLFloaterEventInfo::createEventDetail(void* userdata)
{
	LLFloaterEventInfo* self = (LLFloaterEventInfo*)userdata;
	if (!self) return NULL;

	self->mPanelEventp = new LLPanelEvent();

	LLUICtrlFactory::getInstance()->buildPanel(self->mPanelEventp,
											   "panel_event.xml");

	return self->mPanelEventp;
}

// static
LLFloaterEventInfo* LLFloaterEventInfo::show(U32 event_id)
{
	LLFloaterEventInfo* floater;
	instances_map_t::iterator it = sInstances.find(event_id);
	if (it != sInstances.end())
	{
		// ...bring that window to front
		floater = it->second;
		floater->open();
		floater->setFrontmost(true);
	}
	else
	{
		floater = new LLFloaterEventInfo("eventinfo", event_id);
		floater->center();
		floater->open();
		floater->displayEventInfo(event_id);
		floater->setFrontmost(true);
	}

	return floater;
}
