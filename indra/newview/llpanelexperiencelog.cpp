/**
 * @file llpanelexperiencelog.cpp
 * @brief LLPanelExperienceLog class
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

#include "llpanelexperiencelog.h"

#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "llexperiencecache.h"
#include "llscrolllistctrl.h"
#include "llspinctrl.h"
#include "lluictrlfactory.h"

#include "llexperiencelog.h"
#include "llfloaterexperienceprofile.h"
#include "llfloaterreporter.h"

LLPanelExperienceLog::LLPanelExperienceLog()
:	mPageSize(25),
	mCurrentPage(0)
{
	LLUICtrlFactory::getInstance()->buildPanel(this,
											   "panel_experience_log.xml");
}

bool LLPanelExperienceLog::postBuild()
{
	mEventList = getChild<LLScrollListCtrl>("experience_log_list");
	mEventList->setCommitCallback(onSelectionChanged);
	mEventList->setDoubleClickCallback(onProfileExperience);
	mEventList->setCallbackUserData(this);

	mClearBtn = getChild<LLButton>("btn_clear");
	mClearBtn->setClickedCallback(onClear, this);

	mProfileBtn = getChild<LLButton>("btn_profile_xp");
	mProfileBtn->setClickedCallback(onProfileExperience, this);

	mReportBtn = getChild<LLButton>("btn_report_xp");
	mReportBtn->setClickedCallback(onReportExperience, this);

	mNotifyBtn = getChild<LLButton>("btn_notify");
	mNotifyBtn->setClickedCallback(onNotify, this);

	mNextBtn = getChild<LLButton>("btn_next");
	mNextBtn->setClickedCallback(onNext, this);

	mPrevBtn = getChild<LLButton>("btn_prev");
	mPrevBtn->setClickedCallback(onPrev, this);

	LLExperienceLog* log = LLExperienceLog::getInstance();

	mNotifyAllCheck = getChild<LLCheckBoxCtrl>("notify_all");
	mNotifyAllCheck->set(log->getNotifyNewEvent());
	mNotifyAllCheck->setCommitCallback(onNotifyChanged);
	mNotifyAllCheck->setCallbackUserData(this);

	mLogSizeSpin = getChild<LLSpinCtrl>("logsizespinner");
	mLogSizeSpin->set(log->getMaxDays());
	mLogSizeSpin->setCommitCallback(onLogSizeChanged);
	mLogSizeSpin->setCallbackUserData(this);

	mPageSize = log->getPageSize();
	mNewEvent = log->addUpdateSignal(boost::bind(&LLPanelExperienceLog::refresh,
												 this));
	refresh();

	return true;
}

LLPanelExperienceLog* LLPanelExperienceLog::create()
{
	return new LLPanelExperienceLog();
}

void LLPanelExperienceLog::refresh()
{
	S32 selected = mEventList->getFirstSelectedIndex();
	mEventList->deleteAllItems();

	LLExperienceLog* log = LLExperienceLog::getInstance();

	const LLSD events = log->getEvents();
	if (events.size() == 0)
	{
		mEventList->addCommentText(getString("no_events"));
		return;
	}

	setAllChildrenEnabled(false);

	LLSD item;
	bool waiting = false;
	LLUUID waiting_id;

	S32 to_skip = mPageSize * mCurrentPage;
	S32 items = 0;
	bool more_items = false;
	LLSD events_to_save = events;

	LLExperienceCache* expcache = LLExperienceCache::getInstance();

	if (events.size() && events.isMap())
	{
		LLSD::map_const_iterator day = events.endMap();
		do
		{
			--day;
			const LLSD& day_array = day->second;

			std::string date = day->first;
			if (!log->isNotExpired(date))
			{
				events_to_save.erase(day->first);
				continue;
			}

			S32 size = day_array.size();
			if (to_skip > size)
			{
				to_skip -= size;
				continue;
			}

			if (items >= mPageSize && size > 0)
			{
				more_items = true;
				break;
			}

			for (S32 i = day_array.size() - to_skip - 1; i >= 0; --i)
			{
				if (items >= mPageSize)
				{
					more_items = true;
					break;
				}

				LLSD event = day_array[i];
				LLUUID id = event[LLExperienceCache::EXPERIENCE_ID].asUUID();
				const LLSD& experience = expcache->get(id);
				if (experience.isUndefined())
				{
					waiting = true;
					waiting_id = id;
				}

				if (!waiting)
				{
					std::string name = experience[LLExperienceCache::NAME].asString();
					if (!name.empty())
					{
						event["ExpName"] = name;
					}
					item["id"] = event;

					LLSD& columns = item["columns"];
					columns[0]["column"] = "time";
					columns[0]["value"] = day->first +
										  event["Time"].asString();
					columns[1]["column"] = "event";
					columns[1]["value"] = LLExperienceLog::getPermissionString(event,
																			   "ExperiencePermissionShort");
					columns[2]["column"] = "experience_name";
					columns[2]["value"] = name;
					columns[3]["column"] = "object_name";
					columns[3]["value"] = event["ObjectName"].asString();
					mEventList->addElement(item);
				}
				++items;
			}
		}
		while (day != events.beginMap());
	}

	log->setEventsToSave(events_to_save);

	if (waiting)
	{
		mEventList->deleteAllItems();
		mEventList->addCommentText(getString("no_events"));
		expcache->get(waiting_id,
					  boost::bind(&LLPanelExperienceLog::refresh, this));
	}
	else
	{
		setAllChildrenEnabled(true);

		mEventList->setEnabled(true);
		mNextBtn->setEnabled(more_items);
		mPrevBtn->setEnabled(mCurrentPage > 0);
		mClearBtn->setEnabled(mEventList->getItemCount() > 0);
		if (selected < 0)
		{
			selected = 0;
		}
		mEventList->selectNthItem(selected);
		onSelectionChanged(mEventList, this);
	}
}

LLSD LLPanelExperienceLog::getSelectedEvent()
{
	LLScrollListItem* item = mEventList->getFirstSelected();
	if (item)
	{
		return item->getValue();
	}
	return LLSD();
}

//static
void LLPanelExperienceLog::onProfileExperience(void* data)
{
	LLPanelExperienceLog* self = (LLPanelExperienceLog*)data;
	if (self)
	{
		LLSD event = self->getSelectedEvent();
		if (event.isDefined())
		{
			const LLUUID& id = event[LLExperienceCache::EXPERIENCE_ID].asUUID();
			LLFloaterExperienceProfile::show(id);
		}
	}
}

//static
void LLPanelExperienceLog::onClear(void* data)
{
	LLPanelExperienceLog* self = (LLPanelExperienceLog*)data;
	if (self)
	{
		LLExperienceLog::getInstance()->clear();
		self->refresh();
	}
}

//static
void LLPanelExperienceLog::onReportExperience(void* data)
{
	LLPanelExperienceLog* self = (LLPanelExperienceLog*)data;
	if (self)
	{
		LLSD event = self->getSelectedEvent();
		if (event.isDefined())
		{
			const LLUUID& id = event[LLExperienceCache::EXPERIENCE_ID].asUUID();
			LLFloaterReporter::showFromExperience(id);
		}
	}
}

//static
void LLPanelExperienceLog::onNotify(void* data)
{
	LLPanelExperienceLog* self = (LLPanelExperienceLog*)data;
	if (self)
	{
		LLSD event = self->getSelectedEvent();
		if (event.isDefined())
		{
			LLExperienceLog::getInstance()->notify(event);
		}
	}
}

//static
void LLPanelExperienceLog::onNext(void* data)
{
	LLPanelExperienceLog* self = (LLPanelExperienceLog*)data;
	if (self)
	{
		self->mCurrentPage++;
		self->refresh();
	}
}

//static
void LLPanelExperienceLog::onPrev(void* data)
{
	LLPanelExperienceLog* self = (LLPanelExperienceLog*)data;
	if (self)
	{
		self->mCurrentPage--;
		self->refresh();
	}
}

//static
void LLPanelExperienceLog::onNotifyChanged(LLUICtrl* ctrl, void* data)
{
	LLPanelExperienceLog* self = (LLPanelExperienceLog*)data;
	if (self)
	{
		bool notify = self->mNotifyAllCheck->get();
		LLExperienceLog::getInstance()->setNotifyNewEvent(notify);
	}
}

//static
void LLPanelExperienceLog::onLogSizeChanged(LLUICtrl* ctrl, void* data)
{
	LLPanelExperienceLog* self = (LLPanelExperienceLog*)data;
	if (self)
	{
		U32 value = (U32)self->mLogSizeSpin->get();
		LLExperienceLog::getInstance()->setMaxDays(value);
		self->refresh();
	}
}

//static
void LLPanelExperienceLog::onSelectionChanged(LLUICtrl* ctrl, void* data)
{
	LLPanelExperienceLog* self = (LLPanelExperienceLog*)data;
	if (self)
	{
		bool enabled = self->mEventList->getNumSelected() == 1;
		self->mReportBtn->setEnabled(enabled);
		self->mProfileBtn->setEnabled(enabled);
		self->mNotifyBtn->setEnabled(enabled);
	}
}
