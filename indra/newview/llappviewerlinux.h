/**
 * @file llappviewerlinux.h
 * @brief The LLAppViewerLinux class declaration
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 *
 * Copyright (c) 2007-2009, Linden Research, Inc.
 * Copyright (c) 2013-2022, Henri Beauchamp.
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

#ifndef LL_LLAPPVIEWERLINUX_H
#define LL_LLAPPVIEWERLINUX_H

#include "llappviewer.h"

class LLCommandLineParser;

class LLAppViewerLinux final : public LLAppViewer
{
protected:
	LOG_CLASS(LLAppViewerLinux);

public:
	LLAppViewerLinux() = default;

	std::string generateSerialNumber() override;
	bool setupSLURLHandler();

#if !LL_CALL_SLURL_DISPATCHER_IN_CALLBACK
	// Used by the DBus callback, thus static and public.
	LL_INLINE static void setReceivedSLURL(std::string slurl)
	{
		sReceivedSLURL = slurl;
	}
#endif

	static void pumpGlib();

	bool probeVulkan(std::string& version) override;

protected:
	bool beingDebugged() override;

	// Not needed under Linux
	LL_INLINE bool restoreErrorTrap() override		{ return true; }

	void handleSyncCrashTrace() override;

	void initLogging() override;
	bool initParseCommandLine(LLCommandLineParser& clp) override;

	bool initAppMessagesHandler() override;
	bool sendURLToOtherInstance(const std::string& url) override;

#if !LL_CALL_SLURL_DISPATCHER_IN_CALLBACK
	// Yes, these are non-static methods dealing with static data... But
	// LLAppViewerLinux is actually a singleton, so...

	LL_INLINE const std::string& getReceivedSLURL() override
	{
		return sReceivedSLURL;
	}

	LL_INLINE void clearReceivedSLURL() override	{ sReceivedSLURL.clear(); }

private:
	static std::string sReceivedSLURL;
#endif
};

#endif // LL_LLAPPVIEWERLINUX_H
