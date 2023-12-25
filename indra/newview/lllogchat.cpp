/**
 * @file lllogchat.cpp
 * @brief LLLogChat class implementation
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 *
 * Copyright (c) 2002-2009, Linden Research, Inc.
 * Copyright (c) 2011-2023, Henri Beauchamp.
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

#include "lllogchat.h"

#include "llcorehttputil.h"
#include "lldir.h"
#include "llsdutil.h"

#include "llagent.h"
#include "llfloaterim.h"
#include "llgridmanager.h"
#include "llmutelist.h"
#include "llviewercontrol.h"

//static
std::string LLLogChat::timestamp(bool no_date, time_t ts)
{
	std::string format;
	static LLCachedControl<bool> withdate(gSavedSettings, "LogTimestampDate");
	if (!no_date && withdate)
	{
		static LLCachedControl<std::string> date_fmt(gSavedSettings,
													 "ShortDateFormat");
		format = date_fmt;
		format += " ";
	}
	static LLCachedControl<bool> with_seconds(gSavedSettings,
											  "LogTimestampSeconds");
	if (with_seconds)
	{
		static LLCachedControl<std::string> long_fmt(gSavedSettings,
													 "LongTimeFormat");
		format += long_fmt;
	}
	else
	{
		static LLCachedControl<std::string> short_fmt(gSavedSettings,
													  "ShortTimeFormat");
		format += short_fmt;
	}
	if (!ts)
	{
		ts = time_corrected();
	}
	return "[" + LLGridManager::getTimeStamp(ts, format) + "] ";
}

//static
std::string LLLogChat::makeLogFileName(std::string filename)
{
	if (filename.empty())
	{
		filename = "chat";
	}
	else if (gIsInSecondLife &&
			 gSavedPerAccountSettings.getBool("LogFileNameWithoutResident"))
	{
		LLStringUtil::replaceString(filename, " Resident", "");
	}

	if (gSavedPerAccountSettings.getBool("LogFileNamewithDate"))
	{
		time_t now;
		time(&now);
		char dbuffer[20];
		if (filename == "chat")
		{
			strftime(dbuffer, 20, "-%Y-%m-%d", localtime(&now));
		}
		else
		{
			strftime(dbuffer, 20, "-%Y-%m", localtime(&now));
		}
		filename += dbuffer;
	}
	filename = LLDir::getScrubbedFileName(filename);
	filename = gDirUtilp->getExpandedFilename(LL_PATH_PER_ACCOUNT_CHAT_LOGS,
											  filename);
	filename += ".txt";
	return filename;
}

//static
void LLLogChat::saveHistory(const std::string& filename,
							const std::string& line)
{
	std::string log_filename = makeLogFileName(filename);
	LLFILE* fp = LLFile::open(log_filename, "a");
	if (!fp)
	{
		llwarns << "Could not write into chat/IM history log file: "
				<< log_filename << llendl;
		return;
	}
	fprintf(fp, "%s\n", line.c_str());
	LLFile::close (fp);
}

//static
void LLLogChat::loadHistory(const std::string& filename,
							void (*callback)(S32, const LLSD&, void*),
							void* userdata, const LLUUID& session_id)
{
	std::string log_filename = makeLogFileName(filename);
	// Inform the floater about the log file name to use.
	callback(LOG_FILENAME, llsd::map("filename", log_filename), userdata);

	// For server messages timestamp comparisons; returns 0 for non-existent
	// file. HB
	time_t last_modified = LLFile::lastModidied(log_filename);

	LLFILE* fp = last_modified ? LLFile::open(log_filename, "r") : NULL;
	if (fp)
	{
		U32 bsize = gSavedPerAccountSettings.getU32("LogShowHistoryMaxSize");
		// The minimum must be larger than the largest line (1024 characters
		// for the largest text line + timestamp size + resident name size). HB
		bsize = llmax(bsize, 2U) * 1024U;
		char* buffer = (char*)malloc((size_t)bsize * sizeof(char));
		if (!buffer)
		{
			llwarns << "Failure to allocate buffer !" << llendl;
			LLFile::close(fp);
			callback(LOG_END, LLSD(), userdata);
			// Better aborting altogether if memory is *that* low; i.e. do not
			// even bother to attempt and get the server log at this point. HB
			return;
		}

		char* bptr;
		S32 len;
		bool firstline = true;
		if (fseek(fp, 1L - (long)bsize, SEEK_END))
		{
			// File is smaller than recall size. Get it all.
			firstline = false;	// No risk of truncation.
			if (fseek(fp, 0, SEEK_SET))
			{
				llwarns << "Failure to seek file: " << log_filename << llendl;
				LLFile::close(fp);
				free(buffer);
				callback(LOG_END, LLSD(), userdata);
				// Better aborting now if the file system is corrupted !  HB
				return;
			}
		}

		while (fgets(buffer, bsize, fp))
		{
			len = strlen(buffer) - 1;
			for (bptr = buffer + len;
				 (*bptr == '\n' || *bptr == '\r') && bptr > buffer; --bptr)
			{
				*bptr = '\0';
			}

			if (firstline)	// Skip the truncated first line.
			{
				firstline = false;
				continue;
			}

			callback(LOG_LINE, llsd::map("line", std::string(buffer)),
					 userdata);
		}

		LLFile::close(fp);
		free(buffer);
	}

	if (session_id.isNull() ||
		!gSavedPerAccountSettings.getBool("FetchGroupChatHistory"))
	{
		// Not a group chat, or the user does not want us to fetch history from
		// the server. We are done. HB
		callback(LOG_END, LLSD(), userdata);
		return;
	}
	const std::string& url = gAgent.getRegionCapability("ChatSessionRequest");
	if (url.empty())
	{
		// No such capability. We are done.
		callback(LOG_END, LLSD(), userdata);
		return;
	}
	// This callback will cause all incoming messages to get queued until the
	// server log has been retreived and printed. HB
	callback(LOG_SERVER_FETCH, LLSD(), userdata);
	// Fetch the server log asynchronously.
	gCoros.launch("fetchHistoryCoro",
				  boost::bind(&LLLogChat::fetchHistoryCoro, url, session_id,
							  callback, last_modified));
}

//static
void LLLogChat::fetchHistoryCoro(const std::string& url, LLUUID session_id,
								 void (*callback)(S32, const LLSD&, void*),
								 time_t last_modified)
{
	LLSD query;
	query["method"] = "fetch history";
	query["session-id"] = session_id;

	LLCoreHttpUtil::HttpCoroutineAdapter adapter("FetchHistory");
	LLSD result = adapter.postAndSuspend(url, query);

	LLFloaterIMSession* floaterp =
		LLFloaterIMSession::findInstance(session_id);
	if (!floaterp)
	{
		llinfos << "Received a reply for closed session Id: " << session_id
				<< ". Ignored." << llendl;
		return;
	}
	// Note: in the (unlikely) event we would change the callback userdata NOT
	// to point on the corresponding IM floater, this would have to be changed
	// here too (i.e. userdata would have to be passed to this method) ! HB
	void* userdata = floaterp;

	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	if (!status)
	{
		llwarns << "Failed to retrieve the server log for IM session Id: "
				<< session_id << llendl;
		callback(LOG_END, LLSD(), userdata);
		return;
	}

	const LLSD& history =
		result[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS_CONTENT];
	if (!history.isArray() || !history.size())
	{
		// No log available, or bad format (not an array).
		callback(LOG_END, LLSD(), userdata);
		return;
	}

	// Take the host computer/server clocks difference into acount and add 1s
	// to avoid rounding issues. HB
	last_modified += gUTCOffset + 1;

	static LLCachedControl<bool> stamp(gSavedPerAccountSettings,
									   "IMLogTimestamp");
	std::string log_line, tmp;
	LLUUID from_id;
	for (LLSD::array_const_iterator it = history.beginArray(),
									end = history.endArray();
		 it != end; ++it)
	{
		const LLSD& data = *it;
		if (!data.isMap() || !data.has("message") || !data.has("time"))
		{
			LL_DEBUGS("ServerIMLog") << "Skipping message due incomplete info: "
									 << ll_pretty_print_sd(data) << LL_ENDL;
			continue;
		}

		time_t msg_stamp = data["time"].asInteger();
		if (msg_stamp <= last_modified)
		{
			LL_DEBUGS("ServerIMLog") << "Skipping message due to time stamp: "
									 << msg_stamp << " - Last modified: "
									 << last_modified << " - Skipped message: "
									 << data["message"].asString() << LL_ENDL;
			continue;
		}

		if (data.has("from_id"))
		{
			from_id.set(data["from_id"], false);
			if (LLMuteList::isMuted(from_id, LLMute::flagTextChat))
			{
				// Do not list muted avatars' prose.
				continue;
			}
		}
		else
		{
			from_id.setNull();
			LL_DEBUGS("ServerIMLog") << "Message without a source Id: "
									 << data["message"].asString() << LL_ENDL;
		}
		LLSD cbdata;
		cbdata["from_id"] = from_id.asString();

		// Get the text, and check for an emote.
		log_line = data["message"].asString();
		bool emote = log_line.compare(0, 4, "/me ") == 0;
		if (emote)
		{
			log_line.erase(0, 3);
		}
		// This will be used to compare with recently received messages:
		// since we cannot trust the time stamps or names formats, we only
		// retain the text (without "/me" for emotes, since this would not
		// appear in logs). HB
		cbdata["message"] = log_line;

		if (data.has("from"))
		{
			if (emote)
			{
				log_line = data["from"].asString() + log_line;
			}
			else
			{
				log_line = data["from"].asString() + ": " + log_line;
			}
		}
		if (stamp)
		{
			log_line = timestamp(false, msg_stamp) + log_line;
		}
		// This is the actual, full logged line to display in the floater. HB
		cbdata["line"] = log_line;

		callback(LOG_SERVER, cbdata, userdata);
	}

	callback(LOG_END, LLSD(), userdata);
}
