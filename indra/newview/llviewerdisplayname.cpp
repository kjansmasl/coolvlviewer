/**
 * @file llviewerdisplayname.cpp
 * @brief Wrapper for display name functionality
 *
 * $LicenseInfo:firstyear=2010&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "llviewerdisplayname.h"

#include "llavatarnamecache.h"
#include "llcorehttputil.h"
#include "llhttpnode.h"
#include "llnotifications.h"

#include "llagent.h"
#include "llvoavatar.h"

namespace LLViewerDisplayName
{
	// Fired when viewer receives server response to display name change
	set_name_signal_t sSetDisplayNameSignal;

	void setCoro(const std::string& url, const LLSD& change_array);
}

void LLViewerDisplayName::setCoro(const std::string& url,
								  const LLSD& change_array)
{
	LLSD body;
	body["display_name"] = change_array;

	LLCore::HttpOptions::ptr_t options(new LLCore::HttpOptions);

	// People API can return localized error messages. Indicate our language
	// preference via header.
	LLCore::HttpHeaders::ptr_t headers(new LLCore::HttpHeaders);
	headers->append(HTTP_OUT_HEADER_ACCEPT_LANGUAGE, LLUI::getLanguage());

	LLCoreHttpUtil::HttpCoroutineAdapter adapter("setDisplayName");
	LLSD result = adapter.postAndSuspend(url, body, options, headers);

	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	// We only care about errors
	if (!status)
	{
		llwarns << "Error: " << status.toString() << llendl;
		sSetDisplayNameSignal(false, "", LLSD());
		sSetDisplayNameSignal.disconnect_all_slots();
	}
}

void LLViewerDisplayName::set(const std::string& display_name,
							  const set_name_slot_t& slot)
{
	const std::string& cap_url = gAgent.getRegionCapability("SetDisplayName");
	if (cap_url.empty())
	{
		// This server does not support display names, report error
		slot(false, "unsupported", LLSD());
		return;
	}

	// People API requires both the old and new value to change a variable.
	// Our display name will be in cache before the viewer's UI is available
	// to request a change, so we can use direct lookup without callback.
	LLAvatarName av_name;
	if (!LLAvatarNameCache::get(gAgentID, &av_name))
	{
		slot(false, "name unavailable", LLSD());
		return;
	}

	// People API expects array of [ "old value", "new value" ]
	LLSD change_array = LLSD::emptyArray();
	change_array.append(av_name.mDisplayName);
	change_array.append(display_name);

	llinfos << "Set name POST to " << cap_url << llendl;

	// Record our caller for when the server sends back a reply
	sSetDisplayNameSignal.connect(slot);

	// POST the requested change. The sim will not send a response back to
	// this request directly, rather it will send a separate message after it
	// communicates with the back-end.
	gCoros.launch("setDisplayNameCoro",
				  boost::bind(&LLViewerDisplayName::setCoro, cap_url,
							  change_array));
}

class LLSetDisplayNameReply final : public LLHTTPNode
{
protected:
	LOG_CLASS(LLSetDisplayNameReply);

public:
	void post(LLHTTPNode::ResponsePtr response, const LLSD& context,
			  const LLSD& input) const override
	{
		LLSD body = input["body"];

		S32 status = body["status"].asInteger();
		std::string reason = body["reason"].asString();
		LLSD content = body["content"];

		bool success = status == HTTP_OK;
		if (!success)
		{
			llwarns << "Status: " << status << " - Reason: " << reason
					<< llendl;
		}

		// If viewer's concept of display name is out-of-date, the set request
		// will fail with 409 Conflict. If that happens, fetch up-to-date name
		// information.
		if (status == HTTP_CONFLICT)
		{
			LLUUID agent_id = gAgentID;
			// Flush stale data
			LLAvatarNameCache::erase(agent_id);
			// Queue request for new data
			LLAvatarName ignored;
			LLAvatarNameCache::get(agent_id, &ignored);
			// Kill name tag, as it is wrong
			LLVOAvatar::invalidateNameTag(agent_id);
		}

		// inform caller of result
		LLViewerDisplayName::sSetDisplayNameSignal(success, reason, content);
		LLViewerDisplayName::sSetDisplayNameSignal.disconnect_all_slots();
	}
};

class LLDisplayNameUpdate final : public LLHTTPNode
{
protected:
	LOG_CLASS(LLDisplayNameUpdate);

public:
	void post(LLHTTPNode::ResponsePtr response, const LLSD& context,
			  const LLSD& input) const override
	{
		LLSD body = input["body"];
		LLUUID agent_id = body["agent_id"];
		std::string old_display_name = body["old_display_name"];
		// By convention this record is called "agent" in the People API
		LLSD name_data = body["agent"];

		// Inject the new name data into cache
		LLAvatarName av_name;
		av_name.fromLLSD(name_data);

		if (agent_id == gAgentID)
		{
			llinfos << "Next display name change allowed after: "
					<< LLDate(av_name.mNextUpdate) << llendl;
		}

		// Name expiration time may be provided in headers, or we may use a
		// default value
		// *TODO: get actual headers out of ResponsePtr
		//LLSD headers = response->mHeaders;
		LLSD headers;
		av_name.mExpires = LLAvatarNameCache::nameExpirationFromHeaders(headers);

		LLAvatarNameCache::insert(agent_id, av_name);

		// force name tag to update
		LLVOAvatar::invalidateNameTag(agent_id);

		LLSD args;
		args["OLD_NAME"] = old_display_name;
		args["LEGACY_NAME"] = av_name.getLegacyName();
		args["NEW_NAME"] = av_name.mDisplayName;
		gNotifications.add("DisplayNameUpdate", args);
	}
};

LLHTTPRegistration<LLSetDisplayNameReply>
    gHTTPRegistrationMessageSetDisplayNameReply("/message/SetDisplayNameReply");

LLHTTPRegistration<LLDisplayNameUpdate>
    gHTTPRegistrationMessageDisplayNameUpdate("/message/DisplayNameUpdate");
