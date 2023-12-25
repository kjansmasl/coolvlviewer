/**
 * @file llexperiencelog.cpp
 * @brief llexperiencelog implementation
 *
 * $LicenseInfo:firstyear=2014&license=viewergpl$
 *
 * Copyright (c) 2014, Linden Research, Inc.
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

#include "llexperiencelog.h"

#include "lldate.h"
#include "lldir.h"
#include "lldispatcher.h"
#include "llexperiencecache.h"
#include "llnotifications.h"
#include "llsdserialize.h"
#include "lltrans.h"

#include "llslurl.h"
#include "llviewermessage.h"	// For gGenericDispatcher

// Global
const std::string PUMP_EXPERIENCE = "experience_permission";

class LLExperienceLogDispatchHandler final : public LLDispatchHandler
{
protected:
	LOG_CLASS(LLExperienceLogDispatchHandler);

public:
	bool operator()(const LLDispatcher* dispatcher, const std::string& key,
					const LLUUID& invoice, const sparam_t& strings) override
	{
		LLSD message;

		sparam_t::const_iterator it = strings.begin();
		if (it != strings.end())
		{
			const std::string& llsd_raw = *it++;
			std::istringstream llsd_data(llsd_raw);
			if (!LLSDSerialize::deserialize(message, llsd_data,
											llsd_raw.length()))
			{
				llwarns << "Attempted to read parameter data into LLSD but failed: "
						<< llsd_raw << llendl;
			}
		}
		message["public_id"] = invoice;

		// Object Name
		if (it != strings.end())
		{
			message["ObjectName"] = *it++;
		}

		// Parcel Name
		if (it != strings.end())
		{
			message["ParcelName"] = *it++;
		}
		message["Count"] = 1;

		LLExperienceLog::getInstance()->handleExperienceMessage(message);
		return true;
	}
};

static LLExperienceLogDispatchHandler experience_log_dispatch_handler;

void LLExperienceLog::handleExperienceMessage(LLSD& message)
{
	time_t now;
	time(&now);
	char daybuf[16];
	char time_of_day[16];
	strftime(daybuf, 16, "%Y-%m-%d", localtime(&now));
	strftime(time_of_day, 16, " %H:%M:%S", localtime(&now));
	message["Time"] = time_of_day;

	std::string day = daybuf;

	if (!mEvents.has(day))
	{
		mEvents[day] = LLSD::emptyArray();
	}
	LLSD& dayEvents = mEvents[day];
	if (dayEvents.size() > 0)
	{
		LLSD& last = *(dayEvents.rbeginArray());
		if (last["public_id"].asUUID() == message["public_id"].asUUID() &&
			last["ObjectName"].asString() == message["ObjectName"].asString() &&
			last["OwnerID"].asUUID() == message["OwnerID"].asUUID() &&
			last["ParcelName"].asString() == message["ParcelName"].asString() &&
			last["Permission"].asInteger() == message["Permission"].asInteger())
		{
			last["Count"] = last["Count"].asInteger() + 1;
			last["Time"] = time_of_day;
			mSignals(last);
			return;
		}
	}
	message["Time"] = time_of_day;
	mEvents[day].append(message);
	mEventsToSave[day].append(message);
	mSignals(message);
}

LLExperienceLog::LLExperienceLog()
:	mMaxDays(7),
	mPageSize(25),
	mNotifyNewEvent(false)
{
}

void LLExperienceLog::initialize()
{
	loadEvents();
	if (!gGenericDispatcher.isHandlerPresent("ExperienceEvent"))
	{
		gGenericDispatcher.addHandler("ExperienceEvent",
									  &experience_log_dispatch_handler);
	}
}

std::string LLExperienceLog::getFilename()
{
	return gDirUtilp->getExpandedFilename(LL_PATH_PER_ACCOUNT,
										  "experience_events.xml");
}

std::string LLExperienceLog::getPermissionString(const LLSD& message,
												 const std::string& base)
{
	std::string name;
	if (message.has("Permission"))
	{
		name = base + llformat("%d", message["Permission"].asInteger());
		if (!LLTrans::hasString(name))
		{
			name.clear();
		}
	}
	else
	{
		std::stringstream str;
		LLSDSerialize::toPrettyXML(message, str);
		llwarns << "Missing \"Permission\" field in LLSD for message type: "
				<< base << " - LLSD = " << str.str() << llendl;
	}

	if (name.empty())
	{
		name = base + "Unknown";
	}

	return LLTrans::getString(name, message);
}

void LLExperienceLog::notify(LLSD& message)
{
	LL_DEBUGS("ExperienceLog") << "Notifying about event:";
	std::stringstream str;
	LLSDSerialize::toPrettyXML(message, str);
	LL_CONT << "\n" << str.str() << LL_ENDL;

	LLUUID id;
	std::string experience;
	if (message.has("public_id"))
	{
		id = message["public_id"].asUUID();
	}
	if (id.isNull())
	{
		llwarns << "Absent or invalid public experience Id !" << llendl;
		if (message.has("ExpName"))
		{
			experience = "'" + message["ExpName"].asString() + "'";
		}
		else
		{
			experience = "<Unknown>";
		}
	}
	else
	{
		experience = LLSLURL("experience", id, "profile").getSLURLString();
	}

	LLSD args;
	args["EXPERIENCE"] = experience;
	args["EVENTTYPE"] = getPermissionString(message, "ExperiencePermission");

	if (message.has("ObjectName"))
	{
		args["OBJECTNAME"] = message["ObjectName"].asString();
	}
	else
	{
		args["OBJECTNAME"] = LLStringUtil::null;
	}

	bool from_attachment = message.has("IsAttachment") &&
						   message["IsAttachment"].asBoolean();
	if (!from_attachment)
	{
		id.setNull();
		if (message.has("OwnerID"))
		{
			id = message["OwnerID"].asUUID();
		}

		std::string owner;
		if (id.notNull())
		{
			owner = LLSLURL("agent", id, "about").getSLURLString();
		}
		else
		{
			llwarns << "Absent or invalid experience owner Id !" << llendl;
		}
		args["OWNER"] = owner;

		if (message.has("ParcelName"))
		{
			args["PARCELNAME"] = message["ParcelName"].asString();
		}
		else
		{
			args["PARCELNAME"] = LLStringUtil::null;
		}
	}

	LL_DEBUGS("ExperienceLog") << "... translated into notification arguments:";
	std::stringstream str;
	LLSDSerialize::toPrettyXML(args, str);
	LL_CONT << "\n" << str.str() << LL_ENDL;

	if (from_attachment)
	{
		gNotifications.add("ExperienceEventAttachment", args);
	}
	else
	{
		gNotifications.add("ExperienceEvent", args);
	}
}

void LLExperienceLog::saveEvents()
{
	std::string filename = getFilename();
	LLSD settings = LLSD::emptyMap().with("Events", mEventsToSave);

	settings["MaxDays"] = (LLSD::Integer)mMaxDays;
	settings["Notify"] = mNotifyNewEvent;
	settings["PageSize"] = (LLSD::Integer)mPageSize;

	llofstream stream(filename.c_str());
	if (stream.is_open())
	{
		LLSDSerialize::toPrettyXML(settings, stream);
		stream.close();
	}
}

void LLExperienceLog::loadEvents()
{
	LLSD settings = LLSD::emptyMap();

	std::string filename = getFilename();
	llifstream stream(filename.c_str());
	if (stream.is_open())
	{
		LLSDSerialize::fromXMLDocument(settings, stream);
		stream.close();
	}

	if (settings.has("MaxDays"))
	{
		setMaxDays((U32)settings["MaxDays"].asInteger());
	}
	if (settings.has("Notify"))
	{
		setNotifyNewEvent(settings["Notify"].asBoolean());
	}
	if (settings.has("PageSize"))
	{
		setPageSize((U32)settings["PageSize"].asInteger());
	}
	mEvents.clear();
	if (mMaxDays > 0 && settings.has("Events"))
	{
		mEvents = settings["Events"];
		mEventsToSave = mEvents;
	}
}

LLExperienceLog::~LLExperienceLog()
{
	saveEvents();
}

void LLExperienceLog::eraseExpired()
{
	while ((U32)mEvents.size() > mMaxDays && mMaxDays > 0)
	{
		mEvents.erase(mEvents.beginMap()->first);
	}
}

bool LLExperienceLog::isNotExpired(std::string& date)
{
	LLDate event_date;
	S32 month, day, year;
	S32 matched = sscanf(date.c_str(), "%d-%d-%d", &year, &month, &day);
	if (matched != 3) return false;

	event_date.fromYMDHMS(year, month, day);

	S32 curr_year, curr_month, curr_day;
	LLDate curr_date = LLDate::now();
	curr_date.split(&curr_year, &curr_month, &curr_day);

	// Sets hour, min, and sec to 0
	curr_date.fromYMDHMS(curr_year, curr_month, curr_day);

	LLDate boundary_date = LLDate(curr_date.secondsSinceEpoch() -
								  86400 * getMaxDays());

	return event_date >= boundary_date;
}

LLExperienceLog::callback_connection_t LLExperienceLog::addUpdateSignal(const callback_slot_t& cb)
{
	return mSignals.connect(cb);
}

void LLExperienceLog::setNotifyNewEvent(bool val)
{
	mNotifyNewEvent = val;
	if (!val && mNotifyConnection.connected())
	{
		mNotifyConnection.disconnect();
	}
	else if (val && !mNotifyConnection.connected())
	{
		mNotifyConnection = addUpdateSignal(boost::function<void(LLSD&)>(LLExperienceLog::notify));
	}
}
