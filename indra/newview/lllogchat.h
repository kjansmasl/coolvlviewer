/**
 * @file lllogchat.h
 * @brief LLLogChat class definition
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

#ifndef LL_LLLOGCHAT_H
#define LL_LLLOGCHAT_H

#include <time.h>		// For time_t

#include "lluuid.h"
#include "llsd.h"

// Purely static class
class LLLogChat
{
protected:
	LOG_CLASS(LLLogChat);

	LLLogChat() = delete;
	~LLLogChat() = delete;

public:
	// Status values for callback function
	enum EResponseType
	{
		LOG_FILENAME,
		LOG_SERVER_FETCH,
		LOG_SERVER,
		LOG_LINE,
		LOG_END,
	};

	// Returns a time stamp with the SL (or UTC for OpenSim) time zone, which
	// format (date and time format, with or without date, with or without the
	// seconds) follows the user preferences. When 'no_date' is true, then the
	// date is always omitted, regardless of the said preferences.
	// When 'ts' is omitted or 0, the current time is used, else it it supposed
	// to correspond to the grid time. HB
	static std::string timestamp(bool no_date = false, time_t ts = 0);

	static std::string makeLogFileName(std::string filename);

	static void saveHistory(const std::string& filename,
							const std::string& line);
	static void loadHistory(const std::string& filename,
		                    void (*callback)(S32, const LLSD&, void*),
							void* userdata,
							const LLUUID& session_id = LLUUID::null);
private:
	static void fetchHistoryCoro(const std::string& url, LLUUID session_id,
								 void (*callback)(S32, const LLSD&, void*),
								 time_t last_modified);
};

#endif
