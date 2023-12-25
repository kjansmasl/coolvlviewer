/**
 * @file lleventnotifier.h
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

#ifndef LL_LLEVENTNOTIFIER_H
#define LL_LLEVENTNOTIFIER_H

#include <map>

#include "llframetimer.h"
#include "llvector3d.h"

class LLMessageSystem;

class LLEventInfo
{
public:
	LLEventInfo()		{}

	void unpack(LLMessageSystem* msg);

	static void loadCategories(const LLSD& event_options);

public:
	std::string		mName;
	U32				mID;
	std::string		mDesc;
	std::string		mCategoryStr;
	U32				mDuration;
	std::string		mTimeStr;
	LLUUID			mRunByID;
	std::string		mSimName;
	LLVector3d		mPosGlobal;
	time_t			mUnixTime;
	U32				mCover;
	U32				mEventFlags;
	bool			mHasCover;
	bool			mSelected;

	typedef std::map<U32, std::string> map_t;
	static map_t	sCategories;
};

class LLEventNotification final
{
protected:
	LOG_CLASS(LLEventNotification);

public:
	LLEventNotification();

	// In the format it comes in from login
	bool load(const LLSD& event_options);

	// From existing event_info on the viewer.
	bool load(const LLEventInfo& event_info);

#if 0
	void setEventID(U32 event_id);
	void setEventName(std::string& event_name);
#endif

	LL_INLINE U32 getEventID() const						{ return mEventID; }
	LL_INLINE const std::string& getEventName() const		{ return mEventName; }
	LL_INLINE time_t getEventDate() const					{ return mEventDate; }
	LL_INLINE const std::string& getEventDateStr() const	{ return mEventDateStr; }
	LL_INLINE LLVector3d getEventPosGlobal() const			{ return mEventPosGlobal; }
	LL_INLINE bool handleResponse(const LLSD& notif, const LLSD& payload);

protected:
	U32			mEventID;			// EventID for this event
	std::string	mEventName;
	std::string mEventDateStr;
	time_t		mEventDate;
	LLVector3d	mEventPosGlobal;
};

class LLEventNotifier final
{
public:
	LLEventNotifier();
	~LLEventNotifier();

	void update();	// Notify the user of the event if it is coming up

	// In the format that it comes in from login
	void load(const LLSD& event_options);

	// Add a new notification for an event
	void add(LLEventInfo& event_info);
	void remove(U32 event_id);

	bool hasNotification(U32 event_id);

	typedef std::map<U32, LLEventNotification*> map_t;

protected:
	map_t			mEventNotifications;
	LLFrameTimer	mNotificationTimer;
};

extern LLEventNotifier gEventNotifier;

constexpr U32 EVENT_FLAG_NONE   = 0x0000;
constexpr U32 EVENT_FLAG_MATURE = 0x0001;
constexpr U32 EVENT_FLAG_ADULT  = 0x0002;

#endif //LL_LLEVENTNOTIFIER_H
