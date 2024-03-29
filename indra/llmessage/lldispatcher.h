/**
 * @file lldispatcher.h
 * @brief LLDispatcher class header file.
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

#ifndef LL_LLDISPATCHER_H
#define LL_LLDISPATCHER_H

#include <map>
#include <vector>
#include <string>

#include "llpreprocessor.h"

class LLDispatcher;
class LLMessageSystem;
class LLUUID;

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLDispatchHandler
//
// Abstract base class for handling dispatches. Derive your own classes,
// construct them, and add them to the dispatcher you want to use.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

class LLDispatchHandler
{
public:
	typedef std::vector<std::string> sparam_t;

	LLDispatchHandler() = default;
	virtual ~LLDispatchHandler() = default;

	virtual bool operator()(const LLDispatcher* dispatcher,
							const std::string& key,
							const LLUUID& invoice,
							const sparam_t& string) = 0;
};

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLDispatcher
//
// Basic utility class that handles dispatching keyed operations to function
// objects implemented as LLDispatchHandler derivations.
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class LLDispatcher
{
protected:
	LOG_CLASS(LLDispatcher);

public:
	typedef std::vector<std::string> keys_t;
	typedef std::vector<std::string> sparam_t;

	LLDispatcher() = default;
	virtual ~LLDispatcher() = default;

	// Returns true if they keyed handler exists in this dispatcher.
	LL_INLINE bool isHandlerPresent(const std::string& name) const
	{
		return mHandlers.count(name) != 0;
	}

	// Call this method with the name of the request that has come in. If the
	// handler is present, it is called with the params and returns the return
	// value from.
	bool dispatch(const std::string& name, const LLUUID& invoice,
				  const sparam_t& strings) const;
				  //const iparam_t& itegers) const;

	// Add a handler. If one with the same key already exists, its pointer is
	// returned, otherwise returns NULL. This object does not do memory
	// management of the LLDispatchHandler, and relies on the caller to delete
	// the object if necessary.
	LLDispatchHandler* addHandler(const std::string& name,
								  LLDispatchHandler* func);

	// Helper method to unpack the dispatcher message bus format. Returns true
	// on success.
	static bool unpackMessage(LLMessageSystem* msg, std::string& method,
							  LLUUID& invoice, sparam_t& parameters);

	static bool unpackLargeMessage(LLMessageSystem* msg, std::string& method,
								   LLUUID& invoice, sparam_t& parameters);

protected:
	typedef std::map<std::string, LLDispatchHandler*> dispatch_map_t;
	dispatch_map_t mHandlers;
};

#endif // LL_LLDISPATCHER_H
