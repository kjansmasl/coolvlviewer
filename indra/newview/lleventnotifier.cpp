/**
 * @file lleventnotifier.cpp
 * @brief Viewer code for managing event notifications
 *
 * $LicenseInfo:firstyear=2004&license=viewergpl$
 *
 * Copyright (c) 2004-2009, Linden Research, Inc.
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

#include "lleventnotifier.h"

#include "llmessage.h"
#include "lltrans.h"

#include "llagent.h"
#include "hbfloatersearch.h"
#include "llfloaterworldmap.h"
#include "llgridmanager.h"
#include "llviewercontrol.h"

// Helper function
static std::string get_timestamp(time_t utc_time)
{
	static LLCachedControl<std::string> date_fmt(gSavedSettings,
												 "TimestampFormat");
	return LLGridManager::getTimeStamp(utc_time, date_fmt);
}

///////////////////////////////////////////////////////////////////////////////
// LLEventInfo class
///////////////////////////////////////////////////////////////////////////////

LLEventInfo::map_t LLEventInfo::sCategories;

void LLEventInfo::unpack(LLMessageSystem* msg)
{
	U32 event_id;
	msg->getU32("EventData", "EventID", event_id);
	mID = event_id;

	msg->getString("EventData", "Name", mName);

	msg->getString("EventData", "Category", mCategoryStr);

#if 0
	msg->getString("EventData", "Date", mTimeStr);
#endif

	U32 duration;
	msg->getU32("EventData","Duration",duration);
	mDuration = duration;

	U32 date;
	msg->getU32("EventData", "DateUTC", date);
	mUnixTime = date;
	mTimeStr = get_timestamp(mUnixTime);

	msg->getString("EventData", "Desc", mDesc);

	std::string buffer;
	msg->getString("EventData", "Creator", buffer);
	mRunByID = LLUUID(buffer);

	U32 foo;
	msg->getU32("EventData", "Cover", foo);

	mHasCover = foo != 0;
	if (mHasCover)
	{
		U32 cover;
		msg->getU32("EventData", "Amount", cover);
		mCover = cover;
	}

	msg->getString("EventData", "SimName", mSimName);

	msg->getVector3d("EventData", "GlobalPos", mPosGlobal);

	// Mature content
	U32 event_flags;
	msg->getU32("EventData", "EventFlags", event_flags);
	mEventFlags = event_flags;
}

//static
void LLEventInfo::loadCategories(const LLSD& event_options)
{
	for (LLSD::array_const_iterator it = event_options.beginArray(),
									end = event_options.endArray(); 
		 it != end; ++it)
	{
		const LLSD& entry = *it;
		if (entry.has("category_name") && entry.has("category_id"))
		{
			U32 id = entry["category_id"].asInteger();
			sCategories[id] = entry["category_name"].asString();
		}
	}

}

///////////////////////////////////////////////////////////////////////////////
// LLEventNotification class
///////////////////////////////////////////////////////////////////////////////

LLEventNotification::LLEventNotification()
:	mEventID(0)
{
}

bool LLEventNotification::handleResponse(const LLSD& notification,
										 const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	switch (option)
	{
		case 0:
			gAgent.teleportViaLocation(getEventPosGlobal());
			if (gFloaterWorldMapp)
			{
				gFloaterWorldMapp->trackLocation(getEventPosGlobal());
			}
			break;

		case 1:
			gDisplayEventHack = true;
			HBFloaterSearch::showEvents(getEventID());
			break;

		default:
			break;
	}

	// We could clean up the notification on the server now if we really wanted
	// to.
	return false;
}

bool LLEventNotification::load(const LLSD& response)
{
	bool event_ok = true;

	if (response.has("event_id"))
	{
		mEventID = response["event_id"].asInteger();
	}
	else
	{
		event_ok = false;
	}

	if (response.has("event_name"))
	{
		mEventName = response["event_name"].asString();
		llinfos << "Event: " << mEventName << llendl;
	}
	else
	{
		event_ok = false;
	}

	mEventDateStr.clear();
	if (response.has("event_date"))
	{
		const LLSD& llsddate = response["event_date"];
		LLDate date;
		bool is_iso8601_date = false;
		if (llsddate.isDate())
		{
			date = llsddate.asDate();
			is_iso8601_date = true;
		}
		else if (date.fromString(llsddate.asString()))
		{
			is_iso8601_date = true;
		}
		if (is_iso8601_date)
		{
			mEventDateStr = "[" + LLTrans::getString("LTimeYear") + "]-[" +
							LLTrans::getString("LTimeMthNum") + "]-[" +
							LLTrans::getString("LTimeDay") + "] [" +
							LLTrans::getString("LTimeHour") + "]:[" +
							LLTrans::getString("LTimeMin") + "]:[" +
							LLTrans::getString("LTimeSec") + "]";
			LLSD substitution;
			substitution["datetime"] = date;
			LLStringUtil::format(mEventDateStr, substitution);
		}
		else
		{
			mEventDateStr = llsddate.asString();
		}
		llinfos << "EventDate: " << mEventDateStr << llendl;
	}
	if (response.has("event_date_ut"))
	{
		std::string date = response["event_date_ut"].asString();
		llinfos << "EventDate: " << date << llendl;
		mEventDate = strtoul(date.c_str(), NULL, 10);

		if (mEventDateStr.empty())
		{
			mEventDateStr = get_timestamp(mEventDate);
		}
	}
	else
	{
		event_ok = false;
	}

	S32 grid_x = 0;
	if (response.has("grid_x"))
	{
		grid_x = response["grid_x"].asInteger();
		llinfos << "GridX: " << grid_x << llendl;
	}
	else
	{
		event_ok = false;
	}

	S32 grid_y = 0;
	if (response.has("grid_y"))
	{
		grid_y = response["grid_y"].asInteger();
		llinfos << "GridY: " << grid_y << llendl;
	}
	else
	{
		event_ok = false;
	}

	S32 x_region = 0;
	if (response.has("x_region"))
	{
		x_region = response["x_region"].asInteger();
		llinfos << "RegionX: " << x_region << llendl;
	}
	else
	{
		event_ok = false;
	}

	S32 y_region = 0;
	if (response.has("y_region"))
	{
		y_region = response["y_region"].asInteger();
		llinfos << "RegionY: " << y_region << llendl;
	}
	else
	{
		event_ok = false;
	}

	mEventPosGlobal.mdV[VX] = grid_x * 256 + x_region;
	mEventPosGlobal.mdV[VY] = grid_y * 256 + y_region;
	mEventPosGlobal.mdV[VZ] = 0.f;

	return event_ok;
}

bool LLEventNotification::load(const LLEventInfo& event_info)
{
	mEventID = event_info.mID;
	mEventName = event_info.mName;
	mEventDateStr = event_info.mTimeStr;
	mEventDate = event_info.mUnixTime;
	mEventPosGlobal = event_info.mPosGlobal;
	return true;
}

///////////////////////////////////////////////////////////////////////////////
// LLEventNotifier class
///////////////////////////////////////////////////////////////////////////////

LLEventNotifier gEventNotifier;

LLEventNotifier::LLEventNotifier()
{
}

LLEventNotifier::~LLEventNotifier()
{
	map_t::iterator iter;

	for (map_t::iterator iter = mEventNotifications.begin(),
						  end =  mEventNotifications.end();
		 iter != end; ++iter)
	{
		delete iter->second;
	}
}

void LLEventNotifier::update()
{
	if (mNotificationTimer.getElapsedTimeF32() > 30.f)
	{
		// Check our notifications again and send out updates if they happen.
		time_t alert_time = time_corrected() + 5 * 60;
		map_t::iterator iter;
		for (iter = mEventNotifications.begin();
			 iter != mEventNotifications.end();)
		{
			LLEventNotification* np = iter->second;
			if (np && np->getEventDate() < alert_time)
			{
				LLSD args;
				args["NAME"] = np->getEventName();
				args["DATE"] = np->getEventDateStr();
				gNotifications.add("EventNotification", args, LLSD(),
								   boost::bind(&LLEventNotification::handleResponse,
											   np, _1, _2));
				mEventNotifications.erase(iter++);
			}
			else
			{
				++iter;
			}
		}
		mNotificationTimer.reset();
	}
}

void LLEventNotifier::load(const LLSD& event_options)
{
	for (LLSD::array_const_iterator it = event_options.beginArray(),
									end = event_options.endArray();
		 it != end; ++it)
	{
		LLEventNotification* new_enp = new LLEventNotification();
		if (new_enp->load(*it))
		{
			mEventNotifications[new_enp->getEventID()] = new_enp;
		}
		else
		{
			delete new_enp;
		}
	}
}

bool LLEventNotifier::hasNotification(U32 event_id)
{
	return mEventNotifications.count(event_id) != 0;
}

void LLEventNotifier::add(LLEventInfo& event_info)
{
	// We need to tell the simulator that we want to pay attention to
	// this event, as well as add it to our list.

	if (mEventNotifications.count(event_info.mID) != 0)
	{
		// We already have a notification for this event, don't bother.
		return;
	}

	// Push up a message to tell the server we have this notification.
	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessage("EventNotificationAddRequest");
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID );
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlock("EventData");
	msg->addU32("EventID", event_info.mID);
	gAgent.sendReliableMessage();

	LLEventNotification* enp = new LLEventNotification;
	enp->load(event_info);
	mEventNotifications[event_info.mID] = enp;
}

void LLEventNotifier::remove(U32 event_id)
{
	map_t::iterator iter;
	iter = mEventNotifications.find(event_id);
	if (iter == mEventNotifications.end())
	{
		// We do not have a notification for this event, do not bother.
		return;
	}

	// Push up a message to tell the server to remove this notification.
	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessage("EventNotificationRemoveRequest");
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID );
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlock("EventData");
	msg->addU32("EventID", event_id);
	gAgent.sendReliableMessage();

	delete iter->second;
	mEventNotifications.erase(iter);
}
