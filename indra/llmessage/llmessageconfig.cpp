/**
 * @file llmessageconfig.cpp
 * @brief Live file handling for messaging
 *
 * $LicenseInfo:firstyear=2000&license=viewergpl$
 *
 * Copyright (c) 2000-2009, Linden Research, Inc.
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

#include "llmessageconfig.h"

#include "lllivefile.h"
#include "llsdutil.h"
#include "llsdserialize.h"
#include "llmessage.h"

static const char MESSAGE_CONFIG_FILENAME[] = "message.xml";
constexpr F32 MESSAGE_CONFIG_REFRESH_RATE = 5.f; // seconds
constexpr S32 DEFAULT_MAX_QUEUED_EVENTS = 100;

//---------------------------------------------------------------
// LLMessageConfigFile class
//---------------------------------------------------------------

class LLMessageConfigFile final : public LLLiveFile
{
protected:
	LOG_CLASS(LLMessageConfigFile);

private:
	// Use createInstance() only
	LLMessageConfigFile(const std::string& filename)
	:	LLLiveFile(filename, MESSAGE_CONFIG_REFRESH_RATE),
		mMaxQueuedEvents(0)
	{
		sInstance = this;
	}

public:
	~LLMessageConfigFile() override
	{
		sInstance = NULL;
	}

	// Used to instantiate the singleton class
	static void createInstance(const std::string& server_name,
							   const std::string& config_dir);

	// Returns the singleton configuration file after a call to
	// LLLiveFile::checkAndReload()
	static LLMessageConfigFile* getInstance();

	bool loadFile() override;

	void loadServerDefaults(const LLSD& data);
	void loadMaxQueuedEvents(const LLSD& data);
	void loadMessages(const LLSD& data);
	void loadCapBans(const LLSD& blacklist);
	void loadMessageBans(const LLSD& blacklist);
	bool isCapBanned(const std::string& cap_name) const;

public:
	S32							mMaxQueuedEvents;
	LLSD						mMessages;
	LLSD						mCapBans;
	std::string					mServerDefault;

private:
	static LLMessageConfigFile* sInstance;
	static std::string			sServerName;
};

LLMessageConfigFile* LLMessageConfigFile::sInstance = NULL;
std::string LLMessageConfigFile::sServerName;

//static
void LLMessageConfigFile::createInstance(const std::string& server_name,
										 const std::string& config_dir)
{
	if (sInstance)
	{
		llerrs << "Instance already exists !" << llendl;
	}
	LL_DEBUGS("AppInit") << "Config file: " << config_dir << "/"
						 << MESSAGE_CONFIG_FILENAME << LL_ENDL;
	sServerName = server_name;
	sInstance = new LLMessageConfigFile(config_dir + "/" +
										MESSAGE_CONFIG_FILENAME);
}

LLMessageConfigFile* LLMessageConfigFile::getInstance()
{
	if (!sInstance)
	{
		llerrs << "Call done before class initialization !" << llendl;
	}
	sInstance->checkAndReload();
	return sInstance;
}

//virtual
bool LLMessageConfigFile::loadFile()
{
	LLSD data;
    {
        llifstream file(filename().c_str());
        if (file.is_open())
        {
			LL_DEBUGS("AppInit") << "Loading message.xml file at "
								 << filename() << LL_ENDL;
            LLSDSerialize::fromXML(data, file);
        }

        if (data.isUndefined())
        {
            llwarns << "File missing, ill-formed, or simply undefined; not changing the file."
					<< llendl;
            return false;
        }
    }
	loadServerDefaults(data);
	loadMaxQueuedEvents(data);
	loadMessages(data);
	loadCapBans(data);
	loadMessageBans(data);
	return true;
}

void LLMessageConfigFile::loadServerDefaults(const LLSD& data)
{
	mServerDefault = data["serverDefaults"][sServerName].asString();
}

void LLMessageConfigFile::loadMaxQueuedEvents(const LLSD& data)
{
	 if (data.has("maxQueuedEvents"))
	 {
		  mMaxQueuedEvents = data["maxQueuedEvents"].asInteger();
	 }
	 else
	 {
		  mMaxQueuedEvents = DEFAULT_MAX_QUEUED_EVENTS;
	 }
}

void LLMessageConfigFile::loadMessages(const LLSD& data)
{
	mMessages = data["messages"];
	LL_DEBUGS("AppInit") << "Loading...\n";
	std::ostringstream out;
	LLSDXMLFormatter* formatter = new LLSDXMLFormatter;
	formatter->format(mMessages, out);
	LL_CONT << out.str() << "\nLoaded: " << mMessages.size() << " messages."
			<< LL_ENDL;
}

void LLMessageConfigFile::loadCapBans(const LLSD& data)
{
    LLSD bans = data["capBans"];
    if (!bans.isMap())
    {
        llwarns << "Missing capBans section" << llendl;
        return;
    }

	mCapBans = bans;

    LL_DEBUGS("AppInit") << bans.size() << " ban tests" << LL_ENDL;
}

void LLMessageConfigFile::loadMessageBans(const LLSD& data)
{
    LLSD bans = data["messageBans"];
    if (!bans.isMap())
    {
        llwarns << "Missing messageBans section" << llendl;
        return;
    }

	gMessageSystemp->setMessageBans(bans["trusted"], bans["untrusted"]);
}

bool LLMessageConfigFile::isCapBanned(const std::string& cap_name) const
{
	LL_DEBUGS("AppInit") << "mCapBans is " << LLSDNotationStreamer(mCapBans)
						 << LL_ENDL;
    return mCapBans[cap_name];
}

//---------------------------------------------------------------
// LLMessageConfig class
//---------------------------------------------------------------

//static
void LLMessageConfig::initClass(const std::string& server_name,
								const std::string& config_dir)
{
	LLMessageConfigFile::createInstance(server_name, config_dir);
}

//static
void LLMessageConfig::useConfig(const LLSD& config)
{
	LLMessageConfigFile* file = LLMessageConfigFile::getInstance();
	file->loadServerDefaults(config);
	file->loadMaxQueuedEvents(config);
	file->loadMessages(config);
	file->loadCapBans(config);
	file->loadMessageBans(config);
}

//static
LLMessageConfig::Flavor LLMessageConfig::getServerDefaultFlavor()
{
	LLMessageConfigFile* file = LLMessageConfigFile::getInstance();
	if (file->mServerDefault == "llsd")
	{
		return LLSD_FLAVOR;
	}
	if (file->mServerDefault == "template")
	{
		return TEMPLATE_FLAVOR;
	}
	return NO_FLAVOR;
}

//static
S32 LLMessageConfig::getMaxQueuedEvents()
{
	return LLMessageConfigFile::getInstance()->mMaxQueuedEvents;
}

//static
LLMessageConfig::Flavor LLMessageConfig::getMessageFlavor(const std::string& msg_name)
{
	LLSD config = LLMessageConfigFile::getInstance()->mMessages[msg_name];
	if (config["flavor"].asString() == "llsd")
	{
		return LLSD_FLAVOR;
	}
	if (config["flavor"].asString() == "template")
	{
		return TEMPLATE_FLAVOR;
	}
	return NO_FLAVOR;
}

//static
LLMessageConfig::SenderTrust LLMessageConfig::getSenderTrustedness(const std::string& msg_name)
{
	LLSD config = LLMessageConfigFile::getInstance()->mMessages[msg_name];
	if (config.has("trusted-sender"))
	{
		return config["trusted-sender"].asBoolean() ? TRUSTED : UNTRUSTED;
	}
	return NOT_SET;
}

//static
bool LLMessageConfig::isValidMessage(const std::string& msg_name)
{
	return LLMessageConfigFile::getInstance()->mMessages.has(msg_name);
}

//static
bool LLMessageConfig::onlySendLatest(const std::string& msg_name)
{
	LLSD config = LLMessageConfigFile::getInstance()->mMessages[msg_name];
	return config["only-send-latest"].asBoolean();
}

bool LLMessageConfig::isCapBanned(const std::string& cap_name)
{
	return LLMessageConfigFile::getInstance()->isCapBanned(cap_name);
}
