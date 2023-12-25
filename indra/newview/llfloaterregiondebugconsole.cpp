/**
 * @file llfloaterregiondebugconsole.h
 * @author Brad Kittenbrink <brad@lindenlab.com>
 * @brief Quick and dirty console for region debug settings
 *
 * $LicenseInfo:firstyear=2010&license=viewergpl$
 *
 * Copyright (c) 2010-2010, Linden Research, Inc.
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

#include "llfloaterregiondebugconsole.h"

#include "llcorehttputil.h"
#include "llhttpnode.h"
#include "lllineeditor.h"
#include "lltexteditor.h"
#include "lluictrlfactory.h"

#include "llagent.h"

// Two versions of the sim console API are supported.
//
// SimConsole capability (deprecated):
// This is the initial implementation that is supported by some versions of the
// simulator. It is simple and straight forward, just POST a command and the
// body of the response has the result. This API is deprecated because it
// doesn't allow the sim to use any asynchronous API.
//
// SimConsoleAsync capability:
// This capability replaces the original SimConsole capability. It is similar
// in that the command is POSTed to the SimConsoleAsync cap, but the response
// comes in through the event poll, which gives the simulator more flexibility
// and allows it to perform complex operations without blocking any frames.
//
// We will assume the SimConsoleAsync capability is available, and fall back to
// the SimConsole cap if it is not. The simulator will only support one or the
// other.

const std::string PROMPT = "\n\n> ";
const std::string UNABLE_TO_SEND_COMMAND = "\nERROR: The last command was not received by the server.";
const std::string CONSOLE_UNAVAILABLE = "\nERROR: No console available for this region/simulator.";
const std::string CONSOLE_NOT_SUPPORTED = "\nThis region does not support the simulator console.";

// This handles responses for console commands sent via the asynchronous API.
class ConsoleResponseNode final : public LLHTTPNode
{
protected:
	LOG_CLASS(ConsoleResponseNode);

public:
	void post(LLHTTPNode::ResponsePtr reponse, const LLSD& context,
			  const LLSD& input) const override
	{
		llinfos << "Received response from the debug console: " << input
				<< llendl;
		LLFloaterRegionDebugConsole::onReplyReceived(input["body"].asString());
	}
};

LLFloaterRegionDebugConsole::LLFloaterRegionDebugConsole(const LLSD&)
{
	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_region_debug_console.xml");
}

bool LLFloaterRegionDebugConsole::postBuild()
{
	LLLineEditor* input = getChild<LLLineEditor>("region_debug_console_input");
	input->setEnableLineHistory(true);
	input->setCommitCallback(onInput);
	input->setCallbackUserData(this);
	input->setFocus(true);
	input->setCommitOnFocusLost(false);

	mOutput = getChild<LLTextEditor>("region_debug_console_output");

	mUseNewCap = !gAgent.getRegionCapability("SimConsoleAsync").empty();
	if (!mUseNewCap && gAgent.getRegionCapability("SimConsole").empty())
	{
		mOutput->appendText(CONSOLE_NOT_SUPPORTED + PROMPT,	false, false);
	}
	else
	{
		mOutput->appendText("Type \"help\" for the list of commands.\n\n> ",
							false, false);
	}

	return true;
}

//static
void LLFloaterRegionDebugConsole::onInput(LLUICtrl* ctrl, void* userdata)
{
	LLFloaterRegionDebugConsole* self = (LLFloaterRegionDebugConsole*)userdata;
	LLLineEditor* input = (LLLineEditor*)ctrl;
	if (!self || !input) return;

	std::string text = input->getText() + "\n";

	LLSD body = LLSD(input->getText());
	const std::string& url =
		self->mUseNewCap ? gAgent.getRegionCapability("SimConsoleAsync")
						 : gAgent.getRegionCapability("SimConsole");
	if (url.empty())
	{
		text += CONSOLE_UNAVAILABLE + PROMPT;
	}
	else if (self->mUseNewCap)
	{
		// Using SimConsoleAsync
		LLCoreHttpUtil::HttpCoroutineAdapter::callbackHttpPost(url, body,
															   NULL,
															   onConsoleError);
	}
	else
	{
		// Using SimConsole (deprecated)
		LLCoreHttpUtil::HttpCoroutineAdapter::callbackHttpPost(url, body,
															   onConsoleSuccess,
															   onConsoleError);
	}

	self->mOutput->appendText(text, false, false);
	input->clear();
}

//static
void LLFloaterRegionDebugConsole::onConsoleSuccess(const LLSD& result)
{
	LLSD content = result;
	if (result.isMap() &&
		result.has(LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS_CONTENT))
	{
		content = result[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS_CONTENT];
	}
	onReplyReceived(content.asString());
}

//static
void LLFloaterRegionDebugConsole::onConsoleError(const LLSD& result)
{
	llwarns << result << llendl;
	onReplyReceived(UNABLE_TO_SEND_COMMAND);
}

//static
void LLFloaterRegionDebugConsole::onReplyReceived(const std::string& output)
{
	LLFloaterRegionDebugConsole* self = findInstance();
	if (self)
	{
		self->mOutput->appendText(output + PROMPT, false, false);
	}
}

LLHTTPRegistration<ConsoleResponseNode>
	gHTTPRegistrationMessageDebugConsoleResponse("/message/SimConsoleResponse");
