/**
 * @file llexperiencelog.h
 * @brief llexperiencelog and related class definitions
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

#ifndef LL_LLEXPERIENCELOG_H
#define LL_LLEXPERIENCELOG_H

#include "boost/signals2.hpp"

#include "llerror.h"
#include "llsingleton.h"

extern const std::string PUMP_EXPERIENCE;

class LLExperienceLog : public LLSingleton<LLExperienceLog>
{
	friend class LLExperienceLogDispatchHandler;
	friend class LLSingleton<LLExperienceLog>;

protected:
	LOG_CLASS(LLExperienceLog);

	LLExperienceLog();

	void loadEvents();
	void saveEvents();
	void eraseExpired();

public:
	virtual ~LLExperienceLog();

	typedef boost::signals2::signal<void(LLSD&)> callback_signal_t;
	typedef callback_signal_t::slot_type callback_slot_t;
	typedef boost::signals2::connection callback_connection_t;
	callback_connection_t addUpdateSignal(const callback_slot_t& cb);

	void initialize();

	LL_INLINE U32 getMaxDays() const			{ return mMaxDays; }
	LL_INLINE void setMaxDays(U32 val)			{ mMaxDays = val; }

	LL_INLINE bool getNotifyNewEvent() const	{ return mNotifyNewEvent; }
	void setNotifyNewEvent(bool val);

	LL_INLINE U32 getPageSize() const			{ return mPageSize; }
	LL_INLINE void setPageSize(U32 val)			{ mPageSize = val; }

	LL_INLINE const LLSD& getEvents()const		{ return mEvents; }

	LL_INLINE void clear()						{ mEvents.clear(); }

	static void notify(LLSD& message);
	static std::string getFilename();
	static std::string getPermissionString(const LLSD& message,
										   const std::string& base);

	LL_INLINE void setEventsToSave(LLSD event)	{ mEventsToSave = event; }
	bool isNotExpired(std::string& date);

	void handleExperienceMessage(LLSD& message);

protected:
	callback_signal_t		mSignals;
	callback_connection_t	mNotifyConnection;
	U32						mMaxDays;
	U32						mPageSize;
	LLSD					mEvents;
	LLSD					mEventsToSave;
	bool					mNotifyNewEvent;
};

#endif // LL_LLEXPERIENCELOG_H
