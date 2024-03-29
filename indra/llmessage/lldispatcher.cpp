/**
 * @file lldispatcher.cpp
 * @brief Implementation of the dispatcher object.
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

#include "linden_common.h"

#include <algorithm>

#include "lldispatcher.h"

#include "llmessage.h"

bool LLDispatcher::dispatch(const std::string& name, const LLUUID& invoice,
							const sparam_t& strings) const
{
	dispatch_map_t::const_iterator it = mHandlers.find(name);
	if (it != mHandlers.end())
	{
		LLDispatchHandler* func = it->second;
		return (*func)(this, name, invoice, strings);
	}
	llwarns_once << "Unable to find handler for generic message: " << name
				 << llendl;
	return false;
}

LLDispatchHandler* LLDispatcher::addHandler(const std::string& name,
											LLDispatchHandler* func)
{
	dispatch_map_t::iterator it = mHandlers.find(name);
	LLDispatchHandler* old_handler = NULL;
	if (it != mHandlers.end())
	{
		old_handler = it->second;
		mHandlers.erase(it);
	}
	if (func)
	{
		// Only non-null handlers so that we don't have to worry about it later
		mHandlers.emplace(name, func);
	}
	return old_handler;
}

//static
bool LLDispatcher::unpackMessage(LLMessageSystem* msg, std::string& method,
								 LLUUID& invoice, sparam_t& parameters)
{
	char buf[MAX_STRING];
	msg->getStringFast(_PREHASH_MethodData, _PREHASH_Method, method);
	msg->getUUIDFast(_PREHASH_MethodData, _PREHASH_Invoice, invoice);
	S32 size;
	S32 count = msg->getNumberOfBlocksFast(_PREHASH_ParamList);
	for (S32 i = 0; i < count; ++i)
	{
		// We treat the SParam as binary data (since it might be an LLUUID in
		// compressed form which may have embedded \0's,)
		size = msg->getSizeFast(_PREHASH_ParamList, i, _PREHASH_Parameter);
		if (size >= 0)
		{
			msg->getBinaryDataFast(_PREHASH_ParamList, _PREHASH_Parameter,
								   buf, size, i, MAX_STRING-1);

			// If the last byte of the data is 0x0, this is either a normally
			// packed string, or a binary packed UUID (which for these messages
			// are packed with a 17th byte 0x0).  Unpack into a std::string
			// without the trailing \0, so "abc\0" becomes
			// std::string("abc", 3) which matches const char* "abc".
			if (size > 0 && buf[size - 1] == 0x0)
			{
				// Special char*/size constructor because UUIDs may have
				// embedded 0x0 bytes.
				std::string binary_data(buf, size - 1);
				parameters.push_back(binary_data);
			}
			else
			{
				// This is either a NULL string, or a string that was packed
				// incorrectly as binary data, without the usual trailing '\0'.
				std::string string_data(buf, size);
				parameters.push_back(string_data);
			}
		}
	}
	return true;
}

//static
bool LLDispatcher::unpackLargeMessage(LLMessageSystem* msg,
									  std::string& method, LLUUID& invoice,
									  sparam_t& parameters)
{
	msg->getStringFast(_PREHASH_MethodData, _PREHASH_Method, method);
	msg->getUUIDFast(_PREHASH_MethodData, _PREHASH_Invoice, invoice);
	S32 count = msg->getNumberOfBlocksFast(_PREHASH_ParamList);
	for (S32 i = 0; i < count; ++i)
	{
		// This method treats all Parameter List params as strings and unpacks
		// them regardless of length. If there is binary data it is the callers
		// responsibility to decode it.
		std::string param;
		msg->getStringFast(_PREHASH_ParamList, _PREHASH_Parameter, param, i);
		parameters.push_back(param);
	}
	return true;
}
