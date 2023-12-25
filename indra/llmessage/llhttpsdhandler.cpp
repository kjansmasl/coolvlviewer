/**
 * @file llhttpsdhandler.cpp
 * @brief Public-facing declarations for the HttpHandler class
 *
 * $LicenseInfo:firstyear=2012&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2012, Linden Research, Inc.
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

#include "linden_common.h"

#include "llhttpsdhandler.h"

#include "llcorebufferstream.h"
#include "llcorehttpheaders.h"
#include "llcorehttpresponse.h"
#include "llcorehttputil.h"
#include "llhttpconstants.h"
#include "llsd.h"
#include "llsdserialize.h"

LLHttpSDHandler::LLHttpSDHandler()
{
}

void LLHttpSDHandler::onCompleted(LLCore::HttpHandle handle,
								  LLCore::HttpResponse* response)
{
	LLCore::HttpStatus status = response->getStatus();
	if (!status)
	{
		this->onFailure(response, status);
	}
	else
	{
		LLSD resplsd;
		bool parsed = response->getBodySize() != 0 &&
					  LLCoreHttpUtil::responseToLLSD(response, false, resplsd);
		if (!parsed)
		{
			// Only emit a warning if we failed to parse when
			// 'content-type' == 'application/llsd+xml'
			LLCore::HttpHeaders::ptr_t headers(response->getHeaders());
			const std::string* content_type = NULL;
			if (headers)
			{
				content_type = headers->find(HTTP_IN_HEADER_CONTENT_TYPE);
			}

			if (content_type && HTTP_CONTENT_LLSD_XML == *content_type)
			{
				std::string thebody = LLCoreHttpUtil::responseToString(response);
				llwarns << "Failed to deserialize: "
						<< response->getRequestURL() << " - Status: "
						<< response->getStatus().toString() << " - Body: "
						<< thebody << llendl;
			}
		}

		this->onSuccess(response, resplsd);
	}
}
