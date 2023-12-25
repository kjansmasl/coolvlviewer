/**
 * @file llpluginmessage.cpp
 * @brief LLPluginMessage encapsulates the serialization/deserialization of messages passed to and from plugins.
 *
 * $LicenseInfo:firstyear=2008&license=viewergpl$
 *
 * Copyright (c) 2008-2009, Linden Research, Inc.
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

#include "llpluginmessage.h"

#include "llsdserialize.h"
#include "llstring.h"

LLPluginMessage::LLPluginMessage(const LLPluginMessage& p)
{
	mMessage = p.mMessage;
}

LLPluginMessage::LLPluginMessage(const std::string& message_class,
								 const std::string& message_name)
{
	setMessage(message_class, message_name);
}

// Resets internal state.
void LLPluginMessage::clear()
{
	mMessage = LLSD::emptyMap();
	mMessage["params"] = LLSD::emptyMap();
}

// Sets the message class and name. Also has the side-effect of clearing any
// key-value pairs in the message.
void LLPluginMessage::setMessage(const std::string& message_class,
								 const std::string& message_name)
{
	clear();
	mMessage["class"] = message_class;
	mMessage["name"] = message_name;
}

// Sets a key/value pair in the message, where the value is a string.
void LLPluginMessage::setValue(const std::string& key,
							   const std::string& value)
{
	mMessage["params"][key] = value;
}

// Sets a key/value pair in the message, where the value is LLSD.
void LLPluginMessage::setValueLLSD(const std::string& key, const LLSD& value)
{
	mMessage["params"][key] = value;
}

// Sets a key/value pair in the message, where the value is signed 32 bits.
void LLPluginMessage::setValueS32(const std::string& key, S32 value)
{
	mMessage["params"][key] = value;
}

// Sets a key/value pair in the message, where the value is unsigned 32 bits.
// The value is stored as a string beginning with "0x".
void LLPluginMessage::setValueU32(const std::string& key, U32 value)
{
	std::stringstream temp;
	temp << "0x" << std::hex << value;
	setValue(key, temp.str());
}

// Sets a key/value pair in the message, where the value is a bool.
void LLPluginMessage::setValueBoolean(const std::string& key, bool value)
{
	mMessage["params"][key] = value;
}

// Sets a key/value pair in the message, where the value is a double.
void LLPluginMessage::setValueReal(const std::string& key, F64 value)
{
	mMessage["params"][key] = value;
}

// Sets a key/value pair in the message, where the value is a pointer. The
void LLPluginMessage::setValuePointer(const std::string& key, void* value)
{
	std::stringstream temp;
	// iostreams should output pointer values in hex with an initial 0x by
	// default.
	temp << value;
	setValue(key, temp.str());
}

// Gets the message class.
std::string LLPluginMessage::getClass() const
{
	return mMessage["class"];
}

// Gets the message name.
std::string LLPluginMessage::getName() const
{
	return mMessage["name"];
}

// Returns true if the specified key exists in this message (useful for
// optional parameters).
bool LLPluginMessage::hasValue(const std::string& key) const
{
	bool result = false;

	if (mMessage["params"].has(key))
	{
		result = true;
	}

	return result;
}

// Gets the value of a key as a string. If the key does not exist, an empty
// string will be returned.
std::string LLPluginMessage::getValue(const std::string& key) const
{
	std::string result;

	if (mMessage["params"].has(key))
	{
		result = mMessage["params"][key].asString();
	}

	return result;
}

// Gets the value of a key as LLSD. If the key does not exist, a null LLSD
// will be returned.
LLSD LLPluginMessage::getValueLLSD(const std::string& key) const
{
	LLSD result;

	if (mMessage["params"].has(key))
	{
		result = mMessage["params"][key];
	}

	return result;
}

// Gets the value of a key as signed 32 bits int. If the key does not exist, 0
// will be returned.
S32 LLPluginMessage::getValueS32(const std::string& key) const
{
	S32 result = 0;

	if (mMessage["params"].has(key))
	{
		result = mMessage["params"][key].asInteger();
	}

	return result;
}

// Gets the value of a key as unsigned 32 bits int. If the key does not exist,
// 0 will be returned.
U32 LLPluginMessage::getValueU32(const std::string& key) const
{
	U32 result = 0;

	if (mMessage["params"].has(key))
	{
		std::string value = mMessage["params"][key].asString();

		result = (U32)strtoul(value.c_str(), NULL, 16);
	}

	return result;
}

// Gets the value of a key as a bool. If the key does not exist, false will be
// returned.
bool LLPluginMessage::getValueBoolean(const std::string& key) const
{
	bool result = false;

	if (mMessage["params"].has(key))
	{
		result = mMessage["params"][key].asBoolean();
	}

	return result;
}

// Gets the value of a key as a double. If the key does not exist, 0 will be
// returned.
F64 LLPluginMessage::getValueReal(const std::string& key) const
{
	F64 result = 0.0f;

	if (mMessage["params"].has(key))
	{
		result = mMessage["params"][key].asReal();
	}

	return result;
}

// Gets the value of a key as a pointer. If the key does not exist, NULL will
// be returned.
void* LLPluginMessage::getValuePointer(const std::string& key) const
{
	void* result = NULL;

	if (mMessage["params"].has(key))
	{
		std::string value = mMessage["params"][key].asString();

		result = (void*)llstrtou64(value.c_str(), NULL, 16);
	}

	return result;
}

// Flattens the message into a string.
std::string LLPluginMessage::generate() const
{
	std::ostringstream result;

#if 0	// Pretty XML may be slightly easier to deal with while debugging...
	LLSDSerialize::toXML(mMessage, result);
#endif
	LLSDSerialize::toPrettyXML(mMessage, result);

	return result.str();
}

// Parses an incoming message into component parts. Clears all existing state
// before starting the parse. Returns -1 on failure, otherwise returns the
// number of key/value pairs in the incoming message.
int LLPluginMessage::parse(const std::string& message)
{
	// clear any previous state
	clear();

	std::istringstream input(message);

	S32 parse_result = LLSDSerialize::fromXML(mMessage, input);

	return (int)parse_result;
}

// Adds a message listener. TODO:DOC need more info on what uses this. When
// are multiple listeners needed ?
void LLPluginMessageDispatcher::addPluginMessageListener(LLPluginMessageListener* listener)
{
	mListeners.insert(listener);
}

// Removes a message listener.
void LLPluginMessageDispatcher::removePluginMessageListener(LLPluginMessageListener* listener)
{
	mListeners.erase(listener);
}

// Distribute a message to all message listeners.
void LLPluginMessageDispatcher::dispatchPluginMessage(const LLPluginMessage& message)
{
	for (listener_set_t::iterator it = mListeners.begin();
		 it != mListeners.end(); )
	{
		LLPluginMessageListener* listener = *it;
		listener->receivePluginMessage(message);
		// In case something deleted an entry.
		it = mListeners.upper_bound(listener);
	}
}
