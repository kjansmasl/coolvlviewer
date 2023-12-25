 /**
 * @file llvoiceclient.cpp
 * @brief Implementation of LLVoiceClient class which is the interface to the
 *        voice client process.
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 *
 * Copyright (c) 2001-2009, Linden Research, Inc.
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

#if LL_LINUX
# include <stdlib.h>                               // for getenv()
#endif

#include "boost/tokenizer.hpp"
#include "expat.h"

#include "llvoiceclient.h"

#include "llapr.h"
#include "llbase64.h"
#include "llbufferstream.h"
#include "llcachename.h"
#include "llcallbacklist.h"
#include "llcorehttputil.h"
#include "lldir.h"
#include "llkeyboard.h"
#include "llmd5.h"
#include "llparcel.h"
#include "llprocesslauncher.h"
#include "llsdutil.h"

#include "llagent.h"
#include "llappviewer.h"			// For gDisconnected and gSecondLife
#include "llfloaterchat.h"			// For LLFloaterChat::addChat()
#include "llgridmanager.h"
#include "llimmgr.h"
#include "llmutelist.h"
#include "llstartup.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"
#include "llvoavatarself.h"
#include "llvoicechannel.h"

// Global
LLVoiceClient gVoiceClient;

//static
bool LLVoiceClient::sInitDone = false;

constexpr F32 SPEAKING_TIMEOUT = 1.f;

constexpr S32 VOICE_MAJOR_VERSION = 1;

// Do not retry connecting to the daemon more frequently than this:
constexpr F32 CONNECT_THROTTLE_SECONDS = 1.f;

// Do not send positional updates more frequently than this:
constexpr F32 UPDATE_THROTTLE_SECONDS = 0.1f;

constexpr F32 LOGIN_RETRY_SECONDS = 10.f;
constexpr S32 MAX_LOGIN_RETRIES = 12;

// Incoming volume has the range [0.0 ... 2.0], with 1.0 as the default.
// Map it as follows: 0.0 -> 40, 1.0 -> 44, 2.0 -> 75
static S32 scale_mic_volume(F32 volume)
{
	// Offset volume to the range [-1.0, 1.0], with 0 at the default.
	volume -= 1.f;

	S32 scaled_volume = 44;	// offset scaled_volume by its default level
	if (volume < 0.f)
	{
		scaled_volume += (S32)(volume * 4.f);	// (44 - 40)
	}
	else
	{
		scaled_volume += (S32)(volume * 31.f);	// (75 - 44)
	}

	return scaled_volume;
}

// Incoming volume has the range [0.0 ... 1.0], with 0.5 as the default.
// Map it as follows: 0.0 -> 0, 0.5 -> 62, 1.0 -> 75
static S32 scale_speaker_volume(F32 volume)
{
	// Offset volume to the range [-0.5, 0.5], with 0 at the default.
	volume -= 0.5f;

	S32 scaled_volume = 62;	// Offset scaled_volume by its default level
	if (volume < 0.f)
	{
		scaled_volume += (S32)(volume * 124.f);	// (62 - 0) * 2
	}
	else
	{
		scaled_volume += (S32)(volume * 26.f);	// (75 - 62) * 2
	}

	return scaled_volume;
}

static std::string random_handle()
{
	LLUUID id;
	id.generate();
	return LLBase64::encode((const char*)id.mData, UUID_BYTES);
}

///////////////////////////////////////////////////////////////////////////////
// LLViewerRequiredVoiceVersion class
//
// This class helps constructing new LLIOPipe specializations
///////////////////////////////////////////////////////////////////////////////

class LLVivoxProtocolParser : public LLIOPipe
{
protected:
	LOG_CLASS(LLVivoxProtocolParser);

public:
	LLVivoxProtocolParser();
	~LLVivoxProtocolParser() override;

protected:
	// LLIOPipe virtual implementations: process the data in buffer
	EStatus process_impl(const LLChannelDescriptors& channels,
						 buffer_ptr_t& buffer, bool& eos, LLSD& context,
						 LLPumpIO* pump) override;

	void reset();

	void processResponse(std::string tag);

	static void XMLCALL ExpatStartTag(void* data, const char* el,
									  const char** attr);
	static void XMLCALL ExpatEndTag(void* data, const char* el);
	static void XMLCALL ExpatCharHandler(void* data, const XML_Char* s,
										 int len);

	void StartTag(const char* tag, const char** attr);
	void EndTag(const char* tag);
	void CharData(const char* buffer, int length);

protected:
	std::string 	mInput;

	// Expat control members
	S32				mResponseDepth;
	S32				mIgnoreDepth;
	XML_Parser		mParser;
	bool			mIgnoringTags;
	bool			mIsEvent;

	// Members for processing responses. The values are transient and only
	// valid within a call to processResponse().
	bool			mSquelchDebugOutput;
	S32				mReturnCode;
	S32				mStatusCode;
	std::string		mStatusString;
	std::string		mRequestId;
	std::string		mActionString;
	std::string		mConnectorHandle;
	std::string		mVersionId;
	std::string		mAccountHandle;
	std::string		mSessionHandle;
	std::string		mSessionGrpHandle;
	std::string		mAlias;

	// Members for processing events. The values are transient and only valid
	// within a call to processResponse().
	S32				mState;
	S32				mVolume;
	S32				mParticipantType;
	S32				mNumberOfAliases;
	F32				mEnergy;
	std::string		mEventTypeString;
	std::string		mUriString;
	std::string		mDeviceString;
	std::string		mNameString;
	std::string		mDisplayNameString;
	std::string		mMessageHeader;
	std::string		mMessageBody;
	std::string		mNotificationType;
	bool			mIsModeratorMuted;
	bool			mIsSpeaking;
	bool			mIsChannel;
	bool			mIncoming;
	bool			mEnabled;

	// Members for processing text between tags
	bool			mAccumulateText;
	std::string		mTextBuffer;
};

LLVivoxProtocolParser::LLVivoxProtocolParser()
{
	mParser = NULL;
	mParser = XML_ParserCreate(NULL);
	reset();
}

void LLVivoxProtocolParser::reset()
{
	mResponseDepth = mIgnoreDepth = mParticipantType = mState = mVolume =
					 mNumberOfAliases = mStatusCode = 0;
	mIgnoringTags = mAccumulateText = mIsChannel = mIsEvent = mIsSpeaking =
					mIsModeratorMuted = mSquelchDebugOutput = false;
	mEnergy = 0.f;
	mReturnCode = -1;
	mAlias.clear();
	mTextBuffer.clear();
}

//virtual
LLVivoxProtocolParser::~LLVivoxProtocolParser()
{
	if (mParser)
	{
		XML_ParserFree(mParser);
	}
}

// virtual
LLIOPipe::EStatus LLVivoxProtocolParser::process_impl(const LLChannelDescriptors& channels,
													  buffer_ptr_t& buffer,
													  bool& eos,
													  LLSD& context,
													  LLPumpIO* pump)
{
	LLBufferStream istr(channels, buffer.get());
	std::ostringstream ostr;
	while (istr.good())
	{
		char buf[1024];
		istr.read(buf, sizeof(buf));
		mInput.append(buf, istr.gcount());
	}

	// Look for input delimiter(s) in the input buffer. If one is found, send
	// the message to the xml parser.
	size_t start = 0;
	size_t delim;
	while ((delim = mInput.find("\n\n\n", start)) != std::string::npos)
	{
		// Reset internal state of the LLVivoxProtocolParser (no effect on the
		// expat parser)
		reset();

		XML_ParserReset(mParser, NULL);
		XML_SetElementHandler(mParser, ExpatStartTag, ExpatEndTag);
		XML_SetCharacterDataHandler(mParser, ExpatCharHandler);
		XML_SetUserData(mParser, this);
		XML_Parse(mParser, mInput.data() + start, delim - start, false);

		// If this message is not set to be squelched, output the raw XML
		// received
		if (!mSquelchDebugOutput)
		{
			LL_DEBUGS("Voice") << "Parsing: "
							   << mInput.substr(start, delim - start)
							   << LL_ENDL;
		}

		start = delim + 3;
	}

	if (start)
	{
		mInput = mInput.substr(start);
	}

	LL_DEBUGS("VivoxProtocolParser") << "At end, mInput is: " << mInput
									 << LL_ENDL;

	if (!gVoiceClient.mConnected)
	{
		// If voice has been disabled, we just want to close the socket.
		// This does so.
		llinfos << "Returning STATUS_STOP" << llendl;
		return STATUS_STOP;
	}

	return STATUS_OK;
}

void XMLCALL LLVivoxProtocolParser::ExpatStartTag(void* data, const char* el,
												  const char** attr)
{
	if (data)
	{
		LLVivoxProtocolParser* object = (LLVivoxProtocolParser*)data;
		object->StartTag(el, attr);
	}
}

void XMLCALL LLVivoxProtocolParser::ExpatEndTag(void* data, const char* el)
{
	if (data)
	{
		((LLVivoxProtocolParser*)data)->EndTag(el);
	}
}

void XMLCALL LLVivoxProtocolParser::ExpatCharHandler(void* data,
													 const XML_Char* s,
													 int len)
{
	if (data)
	{
		((LLVivoxProtocolParser*)data)->CharData(s, len);
	}
}

void LLVivoxProtocolParser::StartTag(const char* tag, const char** attr)
{
	// Reset the text accumulator. We should not have strings that are
	// interrupted by new tags
	mTextBuffer.clear();
	// Only accumulate text if we're not ignoring tags.
	mAccumulateText = !mIgnoringTags;

	if (mResponseDepth == 0)
	{
		mIsEvent = !stricmp("Event", tag);

		if (!stricmp("Response", tag) || mIsEvent)
		{
			// Grab the attributes
			while (*attr)
			{
				const char* key = *attr++;
				const char* value = *attr++;

				if (!stricmp("requestId", key))
				{
					mRequestId = value;
				}
				else if (!stricmp("action", key))
				{
					mActionString = value;
				}
				else if (!stricmp("type", key))
				{
					mEventTypeString = value;
				}
			}
		}
		LL_DEBUGS("VivoxProtocolParser") << "Tag: " << tag << " ("
										 << mResponseDepth << ")"  << LL_ENDL;
	}
	else if (mIgnoringTags)
	{
		LL_DEBUGS("VivoxProtocolParser") << "Ignoring tag " << tag
										 << " (depth = " << mResponseDepth
										 << ")" << LL_ENDL;
	}
	else
	{
		LL_DEBUGS("VivoxProtocolParser") << "Tag: " << tag << " ("
										 << mResponseDepth << ")"  << LL_ENDL;

		// Ignore the InputXml stuff so we do not get confused
		if (!stricmp("InputXml", tag))
		{
			mIgnoringTags = true;
			mIgnoreDepth = mResponseDepth;
			mAccumulateText = false;

			LL_DEBUGS("VivoxProtocolParser") << "Starting ignore, mIgnoreDepth is "
											 << mIgnoreDepth << LL_ENDL;
		}
		else if (!stricmp("CaptureDevices", tag))
		{
			gVoiceClient.clearCaptureDevices();
		}
		else if (!stricmp("RenderDevices", tag))
		{
			gVoiceClient.clearRenderDevices();
		}
		else if (!stricmp("CaptureDevice", tag) ||
				 !stricmp("RenderDevice", tag))
		{
			mDeviceString.clear();
		}
	}

	++mResponseDepth;
}

void LLVivoxProtocolParser::EndTag(const char* tag)
{
	const std::string& string = mTextBuffer;

	--mResponseDepth;

	if (mIgnoringTags)
	{
		if (mIgnoreDepth == mResponseDepth)
		{
			LL_DEBUGS("VivoxProtocolParser") << "End of ignore" << LL_ENDL;
			mIgnoringTags = false;
		}
		else
		{
			LL_DEBUGS("VivoxProtocolParser") << "Ignoring tag " << tag
											 << " (depth = " << mResponseDepth
											 << ")" << LL_ENDL;
		}
	}

	if (!mIgnoringTags)
	{
		LL_DEBUGS("VivoxProtocolParser") << "Processing tag: " << tag
										 << " (depth = " << mResponseDepth
										 << ")" << LL_ENDL;

		// Closing a tag. Finalize the text we have accumulated and reset
		if (!stricmp("ReturnCode", tag))
		{
			mReturnCode = strtol(string.c_str(), NULL, 10);
		}
		else if (!stricmp("SessionHandle", tag))
		{
			mSessionHandle = string;
			LL_DEBUGS("Voice") << "Received session handle: " << mSessionHandle
							   << LL_ENDL;
		}
		else if (!stricmp("SessionGroupHandle", tag))
		{
			mSessionGrpHandle = string;
			LL_DEBUGS("Voice") << "Received session group handle: "
							   << mSessionGrpHandle << LL_ENDL;
		}
		else if (!stricmp("StatusCode", tag))
		{
			mStatusCode = strtol(string.c_str(), NULL, 10);
		}
		else if (!stricmp("StatusString", tag))
		{
			mStatusString = string;
		}
		else if (!stricmp("ParticipantURI", tag))
		{
			mUriString = string;
		}
		else if (!stricmp("Volume", tag))
		{
			mVolume = strtol(string.c_str(), NULL, 10);
		}
		else if (!stricmp("Energy", tag))
		{
			mEnergy = (F32)strtod(string.c_str(), NULL);
		}
		else if (!stricmp("IsModeratorMuted", tag))
		{
			mIsModeratorMuted = !stricmp(string.c_str(), "true");
		}
		else if (!stricmp("IsSpeaking", tag))
		{
			mIsSpeaking = !stricmp(string.c_str(), "true");
		}
		else if (!stricmp("Alias", tag))
		{
			mAlias = string;
		}
		else if (!stricmp("NumberOfAliases", tag))
		{
			mNumberOfAliases = strtol(string.c_str(), NULL, 10);
		}
		else if (!stricmp("ConnectorHandle", tag))
		{
			mConnectorHandle = string;
			LL_DEBUGS("Voice") << "Received connector handle: "
							   << mConnectorHandle << LL_ENDL;
		}
		else if (!stricmp("VersionID", tag))
		{
			mVersionId = string;
		}
		else if (!stricmp("AccountHandle", tag))
		{
			mAccountHandle = string;
		}
		else if (!stricmp("State", tag))
		{
			mState = strtol(string.c_str(), NULL, 10);
		}
		else if (!stricmp("URI", tag))
		{
			mUriString = string;
		}
		else if (!stricmp("IsChannel", tag))
		{
			mIsChannel = !stricmp(string.c_str(), "true");
		}
		else if (!stricmp("Incoming", tag))
		{
			mIncoming = !stricmp(string.c_str(), "true");
		}
		else if (!stricmp("Enabled", tag))
		{
			mEnabled = !stricmp(string.c_str(), "true");
		}
		else if (!stricmp("Name", tag))
		{
			mNameString = string;
		}
		else if (!stricmp("ChannelName", tag))
		{
			mNameString = string;
		}
		else if (!stricmp("DisplayName", tag))
		{
			mDisplayNameString = string;
		}
		else if (!stricmp("AccountName", tag))
		{
			mNameString = string;
		}
		else if (!stricmp("ParticipantType", tag))
		{
			mParticipantType = strtol(string.c_str(), NULL, 10);
		}
		else if (!stricmp("MicEnergy", tag))
		{
			mEnergy = (F32)strtod(string.c_str(), NULL);
		}
		else if (!stricmp("ChannelName", tag))
		{
			mNameString = string;
		}
		else if (!stricmp("ChannelURI", tag))
		{
			mUriString = string;
		}
		else if (!stricmp("BuddyURI", tag))
		{
			mUriString = string;
			llwarns << "Buddy feature no more supported." << llendl;
		}
		else if (!stricmp("Presence", tag))
		{
			mStatusString = string;
		}
		else if (!stricmp("Device", tag))
		{
			mDeviceString = string;
		}
		else if (!stricmp("CaptureDevice", tag))
		{
			gVoiceClient.addCaptureDevice(mDeviceString);
		}
		else if (!stricmp("RenderDevice", tag))
		{
			gVoiceClient.addRenderDevice(mDeviceString);
		}
		else if (!stricmp("MessageHeader", tag))
		{
			mMessageHeader = string;
		}
		else if (!stricmp("MessageBody", tag))
		{
			mMessageBody = string;
		}
		else if (!stricmp("NotificationType", tag))
		{
			mNotificationType = string;
		}
		else
		{
			LL_DEBUGS("VivoxProtocolParser") << "Unhandled tag; " << tag
											 << LL_ENDL;
		}

		mTextBuffer.clear();
		mAccumulateText = false;

		if (mResponseDepth == 0)
		{
			// We finished all of the XML, process the data
			processResponse(tag);
		}
	}
}

// This method is called for anything that is not a tag, which can be text you
// want that lies between tags, and a lot of stuff you do not want like file
// formatting (tabs, spaces, CR/LF, etc).
void LLVivoxProtocolParser::CharData(const char* buffer, int length)
{
	// Only copy text if we are in accumulate mode...
	if (mAccumulateText)
	{
		mTextBuffer.append(buffer, length);
	}
}

void LLVivoxProtocolParser::processResponse(std::string tag)
{
	LL_DEBUGS("VivoxProtocolParser") << "Response for tag: " << tag << LL_ENDL;

	// SLIM SDK: the SDK now returns a mStatusCode of "200" (OK) for success.
	// This is a change vs. previous SDKs.
	// According to Mike S., "The actual API convention is that responses with
	// return codes of 0 are successful, regardless of the status code
	// returned", so I believe this will give correct behavior.
	if (mReturnCode == 0)
	{
		mStatusCode = 0;
	}

	if (mIsEvent)
	{
		const char* event_type = mEventTypeString.c_str();
		if (!stricmp(event_type, "ParticipantUpdatedEvent"))
		{
			/*
			<Event type="ParticipantUpdatedEvent">
				<SessionGroupHandle>c1_m1000xFnPP04IpREWNkuw1cOXlhw==_sg0</SessionGroupHandle>
				<SessionHandle>c1_m1000xFnPP04IpREWNkuw1cOXlhw==0</SessionHandle>
				<ParticipantUri>sip:xFnPP04IpREWNkuw1cOXlhw==@bhr.vivox.com</ParticipantUri>
				<IsModeratorMuted>false</IsModeratorMuted>
				<IsSpeaking>true</IsSpeaking>
				<Volume>44</Volume>
				<Energy>0.0879437</Energy>
			</Event>
			*/

			// These happen so often that logging them is pretty useless.
			mSquelchDebugOutput = true;

			gVoiceClient.participantUpdatedEvent(mSessionHandle,
												 mSessionGrpHandle,
												 mUriString, mAlias,
												 mIsModeratorMuted,
												 mIsSpeaking, mVolume,
												 mEnergy);
		}
		else if (!stricmp(event_type, "AccountLoginStateChangeEvent"))
		{
			gVoiceClient.accountLoginStateChangeEvent(mAccountHandle,
													  mStatusCode,
													  mStatusString, mState);
		}
		else if (!stricmp(event_type, "SessionAddedEvent"))
		{
			/*
			<Event type="SessionAddedEvent">
				<SessionGroupHandle>c1_m1000xFnPP04IpREWNkuw1cOXlhw==_sg0</SessionGroupHandle>
				<SessionHandle>c1_m1000xFnPP04IpREWNkuw1cOXlhw==0</SessionHandle>
				<Uri>sip:confctl-1408789@bhr.vivox.com</Uri>
				<IsChannel>true</IsChannel>
				<Incoming>false</Incoming>
				<ChannelName />
			</Event>
			*/
			gVoiceClient.sessionAddedEvent(mUriString, mAlias, mSessionHandle,
										   mSessionGrpHandle, mIsChannel,
										   mIncoming, mNameString);
		}
		else if (!stricmp(event_type, "SessionRemovedEvent"))
		{
			gVoiceClient.sessionRemovedEvent(mSessionHandle,
											 mSessionGrpHandle);
		}
		else if (!stricmp(event_type, "MediaStreamUpdatedEvent"))
		{
			/*
			<Event type="MediaStreamUpdatedEvent">
				<SessionGroupHandle>c1_m1000xFnPP04IpREWNkuw1cOXlhw==_sg0</SessionGroupHandle>
				<SessionHandle>c1_m1000xFnPP04IpREWNkuw1cOXlhw==0</SessionHandle>
				<StatusCode>200</StatusCode>
				<StatusString>OK</StatusString>
				<State>2</State>
				<Incoming>false</Incoming>
			</Event>
			*/
			gVoiceClient.mediaStreamUpdatedEvent(mSessionHandle,
												 mSessionGrpHandle,
												 mStatusCode, mStatusString,
												 mState, mIncoming);
		}
		else if (!stricmp(event_type, "ParticipantAddedEvent"))
		{
			/*
			<Event type="ParticipantAddedEvent">
				<SessionGroupHandle>c1_m1000xFnPP04IpREWNkuw1cOXlhw==_sg4</SessionGroupHandle>
				<SessionHandle>c1_m1000xFnPP04IpREWNkuw1cOXlhw==4</SessionHandle>
				<ParticipantUri>sip:xI5auBZ60SJWIk606-1JGRQ==@bhr.vivox.com</ParticipantUri>
				<AccountName>xI5auBZ60SJWIk606-1JGRQ==</AccountName>
				<DisplayName />
				<ParticipantType>0</ParticipantType>
			</Event>
			*/
			gVoiceClient.participantAddedEvent(mSessionHandle,
											   mSessionGrpHandle,
											   mUriString, mAlias,
											   mNameString, mDisplayNameString,
											   mParticipantType);
		}
		else if (!stricmp(event_type, "ParticipantRemovedEvent"))
		{
			/*
			<Event type="ParticipantRemovedEvent">
				<SessionGroupHandle>c1_m1000xFnPP04IpREWNkuw1cOXlhw==_sg4</SessionGroupHandle>
				<SessionHandle>c1_m1000xFnPP04IpREWNkuw1cOXlhw==4</SessionHandle>
				<ParticipantUri>sip:xtx7YNV-3SGiG7rA1fo5Ndw==@bhr.vivox.com</ParticipantUri>
				<AccountName>xtx7YNV-3SGiG7rA1fo5Ndw==</AccountName>
			</Event>
			*/
			gVoiceClient.participantRemovedEvent(mSessionHandle,
												 mSessionGrpHandle,
												 mUriString, mAlias,
												 mNameString);
		}
		else if (!stricmp(event_type, "AuxAudioPropertiesEvent"))
		{
			gVoiceClient.auxAudioPropertiesEvent(mEnergy);
		}
		else if (!stricmp(event_type, "MessageEvent"))
		{
			gVoiceClient.messageEvent(mSessionHandle, mUriString, mAlias,
									  mMessageHeader, mMessageBody);
		}
		else if (!stricmp(event_type, "SessionNotificationEvent"))
		{
			gVoiceClient.sessionNotificationEvent(mSessionHandle, mUriString,
												  mNotificationType);
		}
		else if (!stricmp(event_type, "SessionUpdatedEvent"))
		{
			/*
			<Event type="SessionUpdatedEvent">
				<SessionGroupHandle>c1_m1000xFnPP04IpREWNkuw1cOXlhw==_sg0</SessionGroupHandle>
				<SessionHandle>c1_m1000xFnPP04IpREWNkuw1cOXlhw==0</SessionHandle>
				<Uri>sip:confctl-9@bhd.vivox.com</Uri>
				<IsMuted>0</IsMuted>
				<Volume>50</Volume>
				<TransmitEnabled>1</TransmitEnabled>
				<IsFocused>0</IsFocused>
				<SpeakerPosition><Position><X>0</X><Y>0</Y><Z>0</Z></Position></SpeakerPosition>
				<SessionFontID>0</SessionFontID>
			</Event>
			*/
			// We do not need to process this, but we also should not warn on
			// it, since that confuses people.
			LL_DEBUGS("VivoxProtocolParser") << "Ignored event: "
											 << mEventTypeString << LL_ENDL;
		}
		else if (!stricmp(event_type, "AudioDeviceHotSwapEvent"))
		{
			/*
			<Event type = "AudioDeviceHotSwapEvent">
				<EventType>RenderDeviceChanged</EventType>
				<RelevantDevice>
					<Device>Speakers(Turtle Beach P11 Headset)</Device>
					<DisplayName>Speakers(Turtle Beach P11 Headset)</DisplayName>
					<Type>SpecificDevice</Type>
				</RelevantDevice>
			</Event>
			*/
			// An audio device was removed or added, fetch and update the local
			// list of audio devices.
			gVoiceClient.getCaptureDevicesSendMessage();
			gVoiceClient.getRenderDevicesSendMessage();
		}
		// Warn for all other events but those (deprecated/unused):
		else if (stricmp(event_type, "BuddyAndGroupListChangedEvent") &&
				 stricmp(event_type, "SessionGroupUpdatedEvent") &&
				 stricmp(event_type, "SessionGroupRemovedEvent") &&
				 stricmp(event_type, "SessionGroupAddedEvent") &&
				 // This one relates to voice morphing (not implemented):
				 stricmp(event_type, "MediaCompletionEvent") &&
				 stricmp(event_type, "VoiceServiceConnectionStateChangedEvent"))
		{
			llwarns << "Unknown event type " << mEventTypeString << llendl;
		}
	}
	else
	{
		const char* action = mActionString.c_str();
		if (!stricmp(action, "Session.Set3DPosition.1"))
		{
			// We do not need to process these, but they are so spammy we do
			// not want to log them.
			mSquelchDebugOutput = true;
		}
		else if (!stricmp(action, "Connector.Create.1"))
		{
			gVoiceClient.connectorCreateResponse(mStatusCode, mStatusString,
												 mConnectorHandle, mVersionId);
		}
		else if (!stricmp(action, "Account.Login.1"))
		{
			gVoiceClient.loginResponse(mStatusCode, mStatusString,
									   mAccountHandle, mNumberOfAliases);
		}
		else if (!stricmp(action, "Session.Create.1"))
		{
			gVoiceClient.sessionCreateResponse(mRequestId, mStatusCode,
											   mStatusString, mSessionHandle);
		}
		else if (!stricmp(action, "SessionGroup.AddSession.1"))
		{
			gVoiceClient.sessionGroupAddSessionResponse(mRequestId,
														mStatusCode,
														mStatusString,
														mSessionHandle);
		}
		else if (!stricmp(action, "Session.Connect.1"))
		{
			gVoiceClient.sessionConnectResponse(mRequestId, mStatusCode,
												mStatusString);
		}
		else if (!stricmp(action, "Aux.SetVadProperties.1"))
		{
			if (mStatusCode && mStatusCode != 200)
			{
				llwarns << "Aux.SetVadProperties.1 request failed with code "
						<< mStatusCode << " and status string: "
						<< mStatusString << llendl;
			}
		}
		else if (!stricmp(action, "Account.Logout.1"))
		{
			gVoiceClient.logoutResponse(mStatusCode, mStatusString);
		}
		else if (!stricmp(action, "Connector.InitiateShutdown.1"))
		{
			gVoiceClient.connectorShutdownResponse(mStatusCode, mStatusString);
		}
		else
		{
			LL_DEBUGS("VivoxProtocolParser") << "Unhandled action: " << action
											 << LL_ENDL;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
// LLVivoxVoiceClientMuteListObserver class
///////////////////////////////////////////////////////////////////////////////

class LLVivoxVoiceClientMuteListObserver : public LLMuteListObserver
{
	LL_INLINE void onChange() override
	{
		gVoiceClient.muteListChanged();
	}
};

static LLVivoxVoiceClientMuteListObserver sMutelistListener;
static bool sMuteListListening = false;

///////////////////////////////////////////////////////////////////////////////
// LLVoiceClient class
///////////////////////////////////////////////////////////////////////////////

LLVoiceClient::LLVoiceClient()
:	mState(stateDisabled),
	mAccountLoggedIn(false),
	mConnectorEstablished(false),
	mSessionTerminateRequested(false),
	mRelogRequested(false),
	mConnected(false),
#if LL_LINUX
	mDeprecatedClient(false),
#endif
	mRetries(0),
	mPump(NULL),
	mTuningMode(false),
	mTuningEnergy(0.f),
	mTuningMicVolume(0),
	mTuningMicVolumeDirty(true),
	mTuningSpeakerVolume(0),
	mTuningSpeakerVolumeDirty(true),
	mTuningExitState(stateDisabled),
#if 0	// Not used
	mAreaVoiceDisabled(false),
#endif
	mProcess(NULL),
	mAudioSession(NULL),
	mNextAudioSession(NULL),
	mCurrentParcelLocalID(0),
	mNumberOfAliases(0),
	mCommandCookie(0),
	mLoginRetryCount(0),
	mLogLevel(0),
	mCaptureDeviceDirty(false),
	mRenderDeviceDirty(false),
	mSpatialCoordsDirty(false),
	mPTT(true),
	mPTTDirty(true),
	mUserPTTState(false),
	mUsePTT(true),
	mPTTIsToggle(false),
	mEarLocation(0),
	mSpeakerVolume(0),
	mSpeakerVolumeDirty(true),
	mSpeakerMuteDirty(true),
	mMicVolume(0),
	mMicVolumeDirty(true),
	mMuteMic(false),
	mVoiceEnabled(false),
	mLipSyncEnabled(false)
{
#if LL_DARWIN || LL_LINUX
	// HACK: THIS DOES NOT BELONG HERE
	// When the vivox daemon dies, the next write attempt on our socket
	// generates a SIGPIPE, which kills us.
	// This should cause us to ignore SIGPIPE and handle the error through
	// proper channels.
	// This should really be set up elsewhere. Where should it go ?
	signal(SIGPIPE, SIG_IGN);

	// Since we are now launching the gateway with fork/exec instead of
	// system(), we need to deal with zombie processes.
	// Ignoring SIGCHLD should prevent zombies from being created.
	// Alternately, we could use wait(), but I would rather not do that.
	signal(SIGCHLD, SIG_IGN);
#endif

	mAccountHandle = random_handle();
	mConnectorHandle = random_handle();
}

LLVoiceClient::~LLVoiceClient()
{
	sInitDone = false;
	killDaemon();
}

void LLVoiceClient::init(LLPumpIO* pump)
{
	if (sInitDone)
	{
		return;
	}
	sInitDone = true;

	llinfos << "Initializing voice client. Default account handle: "
			<< gVoiceClient.mAccountHandle
			<< " - Default connector handle: "
			<< gVoiceClient.mConnectorHandle << llendl;

	gVoiceClient.mPump = pump;
	gVoiceClient.updateSettings();
	gIdleCallbacks.addFunction(LLVoiceClient::idle, &gVoiceClient);

	LLControlVariable* controlp = gSavedSettings.getControl("VivoxVadAuto");
	controlp->getSignal()->connect(boost::bind(&LLVoiceClient::setupVADParams,
											   &gVoiceClient));
	controlp = gSavedSettings.getControl("VivoxVadHangover");
	controlp->getSignal()->connect(boost::bind(&LLVoiceClient::setupVADParams,
											   &gVoiceClient));
	controlp = gSavedSettings.getControl("VivoxVadNoiseFloor");
	controlp->getSignal()->connect(boost::bind(&LLVoiceClient::setupVADParams,
											   &gVoiceClient));
	controlp = gSavedSettings.getControl("VivoxVadSensitivity");
	controlp->getSignal()->connect(boost::bind(&LLVoiceClient::setupVADParams,
											   &gVoiceClient));
}

void LLVoiceClient::killDaemon()
{
	if (mProcess)
	{
		delete mProcess;
		mProcess = NULL;
	}
}

void LLVoiceClient::terminate()
{
	if (sInitDone)
	{
		llinfos << "Terminating voice client..." << llendl;
		if (gVoiceClient.mConnected)
		{
			gVoiceClient.logout();
			gVoiceClient.connectorShutdown();
			// Need to do this now: bad things happen if the destructor does it
			// later.
			gVoiceClient.closeSocket();
		}
		gVoiceClient.mPump = NULL;
	}
}

void LLVoiceClient::updateSettings()
{
	setVoiceEnabled(gSavedSettings.getBool("EnableVoiceChat"));
	setUsePTT(gSavedSettings.getBool("PTTCurrentlyEnabled"));
	std::string keyString = gSavedSettings.getString("PushToTalkButton");
	setPTTKey(keyString);
	setPTTIsToggle(gSavedSettings.getBool("PushToTalkToggle"));
	setEarLocation(gSavedSettings.getS32("VoiceEarLocation"));

	std::string inputDevice = gSavedSettings.getString("VoiceInputAudioDevice");
	setCaptureDevice(inputDevice);
	std::string outputDevice = gSavedSettings.getString("VoiceOutputAudioDevice");
	setRenderDevice(outputDevice);
	F32 mic_level = gSavedSettings.getF32("AudioLevelMic");
	setMicGain(mic_level);
	setLipSyncEnabled(gSavedSettings.getBool("LipSyncEnabled"));
}

bool LLVoiceClient::writeString(const std::string& str)
{
	bool result = false;
	if (mConnected && mSocket)
	{
		apr_status_t err;
		apr_size_t size = (apr_size_t)str.size();
		apr_size_t written = size;

		// Check return code: sockets will fail (broken, etc)
		err = apr_socket_send(mSocket->getSocket(), (const char*)str.data(),
							  &written);
		if (err == 0)
		{
			// Success.
			result = true;
		}
#if 0	// *TODO: handle partial writes (written is number of bytes written)
		// Need to set socket to non-blocking before this will work.
		else if (APR_STATUS_IS_EAGAIN(err))
		{
		}
#endif
		else
		{
			// Assume any socket error means something bad. For now, just close
			// the socket.
			char buf[MAX_STRING];
			llwarns << "APR error " << err << " ("
					<< apr_strerror(err, buf, MAX_STRING)
					<< ") sending data to vivox daemon." << llendl;
			daemonDied();
		}
	}

	return result;
}

void LLVoiceClient::connectorCreate()
{
	// Transition to stateConnectorStarted when the connector handle comes back.
	setState(stateConnectorStarting);

	std::string logpath = gDirUtilp->getExpandedFilename(LL_PATH_LOGS, "");

	std::ostringstream stream;
	stream << "<Request requestId=\"" << mCommandCookie++
		   << "\" action=\"Connector.Create.1\">"
		   << "<ClientName>V2 SDK</ClientName><AccountManagementServer>"
		   << mVoiceAccountServerURI
		   << "</AccountManagementServer><Mode>Normal</Mode>"
		   << "<ConnectorHandle>" << mConnectorHandle << "</ConnectorHandle>"
		   << "<Logging><Folder>" << logpath
		   << "</Folder><FileNamePrefix>Connector</FileNamePrefix>"
		   << "<FileNameSuffix>.log</FileNameSuffix><LogLevel>"
		   << mLogLevel << "</LogLevel></Logging><Application>" << gSecondLife
		   << "</Application><MaxCalls>12</MaxCalls></Request>\n\n\n";
	writeString(stream.str());
}

void LLVoiceClient::connectorShutdown()
{
	setState(stateConnectorStopping);

	if (!mConnectorEstablished)
	{
		return;
	}

	std::ostringstream stream;
	stream << "<Request requestId=\"" << mCommandCookie++
		   << "\" action=\"Connector.InitiateShutdown.1\"><ConnectorHandle>"
		   << mConnectorHandle << "</ConnectorHandle></Request>\n\n\n";
	mConnectorEstablished = false;
	writeString(stream.str());
}

void LLVoiceClient::userAuthorized(const std::string& first_name,
								   const std::string& last_name,
								   const LLUUID& agent_id)
{
	mAccountFirstName = first_name;
	mAccountLastName = last_name;
	mAccountDisplayName = first_name + " " + last_name;

	llinfos << "Name \"" << mAccountDisplayName << "\", Id " << agent_id
			<< llendl;

	mAccountName = nameFromID(agent_id);
}

void LLVoiceClient::requestVoiceAccountProvision(S32 retries)
{
	if (!mVoiceEnabled || !LLStartUp::isLoggedIn())
	{
		return;
	}

	std::string url =
			gAgent.getRegionCapability("ProvisionVoiceAccountRequest");
	if (url.empty())
	{
		LL_DEBUGS("Voice") << "Region does not have ProvisionVoiceAccountRequest capability !"
						   << LL_ENDL;
		return;
	}

	gCoros.launch("LLVivoxVoiceClient::voiceAccountProvisionCoro",
				  boost::bind(&LLVoiceClient::voiceAccountProvisionCoro, url,
							  retries));
	setState(stateConnectorStart);
}

//static
void LLVoiceClient::voiceAccountProvisionCoro(const std::string& url,
											  S32 retries)
{
	LLCore::HttpOptions::ptr_t options(new LLCore::HttpOptions);
	options->setRetries(retries);

	LLCoreHttpUtil::HttpCoroutineAdapter adapter("voiceAccountProvision");
	LLSD result = adapter.postAndSuspend(url, LLSD(), options);

	if (!sInitDone)
	{
		// Voice has since been shut down
		return;
	}

	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	if (!status)
	{
		llwarns << "Unable to provision voice account: " << status.toString()
				<< llendl;
		gVoiceClient.giveUp();
		return;
	}

	result.erase(LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS);
	LL_DEBUGS("Voice") << "ProvisionVoiceAccountRequest response: " << result
					   << LL_ENDL;

	std::string sip_uri_hostname;
	if (result.has("voice_sip_uri_hostname"))
	{
		sip_uri_hostname = result["voice_sip_uri_hostname"].asString();
	}
	// Old Vivox protocol key... Just in case (for OpenSim grids)...
	else  if (result.has("sip_uri_hostname"))
	{
		sip_uri_hostname = result["sip_uri_hostname"].asString();
	}

	// This key is actually misnamed; it is an entire URI, not just a hostname.
	std::string account_server_uri;
	if (result.has("voice_account_server_name"))
	{
		account_server_uri = result["voice_account_server_name"].asString();
	}

	gVoiceClient.login(result["username"].asString(),
					   result["password"].asString(),
					   sip_uri_hostname, account_server_uri);
}

void LLVoiceClient::login(const std::string& account_name,
						  const std::string& password,
						  const std::string& sip_uri_hostname,
						  const std::string& account_server_uri)
{
	mVoiceSIPURIHostName = sip_uri_hostname;
	mVoiceAccountServerURI = account_server_uri;

	if (mAccountLoggedIn)
	{
		// Already logged in.
		llwarns << "Called while already logged in." << llendl;

		// Do not process another login.
		return;
	}
	else if (account_name != mAccountName)
	{
		// *TODO: error ?
		llwarns << "Wrong account name " << account_name << " instead of "
				<< mAccountName << llendl;
	}
	else
	{
		mAccountPassword = password;
	}

	std::string sip_override = gSavedSettings.getString("VivoxSIPURIHostName");
	if (!sip_override.empty())
	{
		mVoiceSIPURIHostName = sip_override;
	}

	if (mVoiceSIPURIHostName.empty())
	{
		// We have an empty account server name so we fall back to hardcoded
		// defaults
		if (gIsInSecondLifeBetaGrid)
		{
			// Use the development account server
			mVoiceSIPURIHostName = "bhd.vivox.com";
		}
		else
		{
			// Use the release account server
			mVoiceSIPURIHostName = "bhr.vivox.com";
		}
	}

	std::string server_override =
		gSavedSettings.getString("VivoxVoiceAccountServerURI");
	if (!server_override.empty())
	{
		mVoiceAccountServerURI = server_override;
		llinfos << "Overriding account server based on VivoxVoiceAccountServerURI setting: "
				<< mVoiceAccountServerURI << llendl;
	}

	if (mVoiceAccountServerURI.empty())
	{
		// If the account server URI is not specified, construct it from the
		// SIP URI hostname
		mVoiceAccountServerURI = "https://www." + mVoiceSIPURIHostName +
								 "/api2/";
		llinfos << "Inferring account server based on SIP URI Host name: "
				<< mVoiceAccountServerURI << llendl;
	}
}

void LLVoiceClient::idle(void* user_data)
{
	LLVoiceClient* self = (LLVoiceClient*)user_data;
	if (self && sInitDone)
	{
		self->stateMachine();
	}
}

std::string LLVoiceClient::state2string(state new_state)
{
	std::string result = "UNKNOWN";

		// Prevent copy-paste errors when updating this list...
#define CASE(x)  case x:  result = #x;  break

	switch (new_state)
	{
		CASE(stateDisableCleanup);
		CASE(stateDisabled);
		CASE(stateStart);
		CASE(stateDaemonLaunched);
		CASE(stateConnecting);
		CASE(stateConnected);
		CASE(stateIdle);
		CASE(stateMicTuningStart);
		CASE(stateMicTuningRunning);
		CASE(stateMicTuningStop);
		CASE(stateConnectorStart);
		CASE(stateConnectorStarting);
		CASE(stateConnectorStarted);
		CASE(stateLoginRetry);
		CASE(stateLoginRetryWait);
		CASE(stateNeedsLogin);
		CASE(stateLoggingIn);
		CASE(stateLoggedIn);
		CASE(stateNoChannel);
		CASE(stateJoiningSession);
		CASE(stateSessionJoined);
		CASE(stateRunning);
		CASE(stateLeavingSession);
		CASE(stateSessionTerminated);
		CASE(stateLoggingOut);
		CASE(stateLoggedOut);
		CASE(stateConnectorStopping);
		CASE(stateConnectorStopped);
		CASE(stateConnectorFailed);
		CASE(stateConnectorFailedWaiting);
		CASE(stateLoginFailed);
		CASE(stateLoginFailedWaiting);
		CASE(stateJoinSessionFailed);
		CASE(stateJoinSessionFailedWaiting);
		CASE(stateJail);
	}

#undef CASE

	return result;
}

std::string LLVoiceClientStatusObserver::status2string(LLVoiceClientStatusObserver::EStatusType inStatus)
{
	std::string result = "UNKNOWN";

	// Prevent copy-paste errors when updating this list...
#define CASE(x)  case x:  result = #x;  break

	switch (inStatus)
	{
		CASE(STATUS_LOGIN_RETRY);
		CASE(STATUS_LOGGED_IN);
		CASE(STATUS_JOINING);
		CASE(STATUS_JOINED);
		CASE(STATUS_LEFT_CHANNEL);
		CASE(STATUS_VOICE_DISABLED);
		CASE(STATUS_VOICE_ENABLED);
		CASE(BEGIN_ERROR_STATUS);
		CASE(ERROR_CHANNEL_FULL);
		CASE(ERROR_CHANNEL_LOCKED);
		CASE(ERROR_NOT_AVAILABLE);
		CASE(ERROR_UNKNOWN);

		default:
			break;
	}

#undef CASE

	return result;
}

void LLVoiceClient::setState(state new_state)
{
	LL_DEBUGS("Voice") << "Entering state " << state2string(new_state)
					   << LL_ENDL;
	mState = new_state;
}

void LLVoiceClient::stateMachine()
{
	static bool first_run = true;

	if (gDisconnected)
	{
		// The viewer has been disconnected from the sim. Disable voice.
		setVoiceEnabled(false);
	}

	if (gDisconnected || !LLStartUp::isLoggedIn())
	{
		return;
	}

	if (mVoiceEnabled)
	{
		updatePosition();
	}
	// NOTE: tuning mode is special: it needs to launch SLVoice even if voice
	// is disabled.
	else if (!mTuningMode)
	{
		if (mState != stateDisabled && mState != stateDisableCleanup)
		{
			// User turned off voice support. Send the cleanup messages, close
			// the socket, and reset.
			if (!mConnected)
			{
				// If voice was turned off after the daemon was launched but
				// before we could connect to it, we may need to issue a kill.
				llinfos << "Disabling voice before connection to daemon, terminating."
						<< llendl;
				killDaemon();
			}

			logout();
			connectorShutdown();

			setState(stateDisableCleanup);
		}
	}

	// Check for parcel boundary crossing
	if (mVoiceEnabled)
	{
		LLViewerRegion* region = gAgent.getRegion();
		LLParcel* parcel = gViewerParcelMgr.getAgentParcel();

		if (region && parcel)
		{
			S32 parcel_local_id = parcel->getLocalID();
			std::string region_name = region->getName();
			const std::string& cap =
				region->getCapability("ParcelVoiceInfoRequest");

			// The region name starts out empty and gets filled in later. Also,
			// the cap gets filled in a short time after the region cross, but
			// a little too late for our purposes. If either is empty, wait for
			// the next time around.
			if (!region_name.empty())
			{
				if (!cap.empty())
				{
					if (parcel_local_id != mCurrentParcelLocalID ||
						region_name != mCurrentRegionName)
					{
						// We have changed parcels. Initiate a parcel channel
						// lookup.
						mCurrentParcelLocalID = parcel_local_id;
						mCurrentRegionName = region_name;

						parcelChanged();
					}
				}
				else
				{
					LL_DEBUGS("Voice") << "Region does not have ParcelVoiceInfoRequest capability. This is normal for a short time after teleporting, but bad if it persists for very long."
									   << LL_ENDL;
				}
			}
		}
	}

	switch (mState)
	{
		case stateDisableCleanup:
			// Clean up and reset everything.
			closeSocket();
			deleteAllSessions();

			mConnectorEstablished = false;
			mAccountLoggedIn = false;
			mAccountPassword.clear();
			mVoiceAccountServerURI.clear();

			setState(stateDisabled);
			break;

		case stateDisabled:
			if (mTuningMode || (mVoiceEnabled && !mAccountName.empty()))
			{
				setState(stateStart);
			}
			break;

		case stateStart:
			if (!LLStartUp::isLoggedIn())
			{
				break;
			}
			if (gSavedSettings.getBool("CmdLineDisableVoice"))
			{
				// Voice is locked out, we must not launch the vivox daemon.
				setState(stateJail);
			}
			else if (!mProcess || !mProcess->isRunning())
			{
				killDaemon();

				// Refresh the log level
				mLogLevel =
					llmin((U32)gSavedSettings.getU32("VivoxDebugLevel"), 10U);

				// Launch the voice daemon

				std::string exe_path = gDirUtilp->getExecutableDir();
				std::string full_path;
#if LL_DARWIN
				full_path = exe_path + "/../Resources/SLVoice";
#elif LL_WINDOWS
				full_path = exe_path + "\\SLVoice.exe";
#elif LL_LINUX
				mDeprecatedClient = false;
				// Linux SLVoice is alas totally deprecated, so we better use
				// Wine to run the Windows binary if we want working voice in
				// SL (and most probably as well in OpenSim grids using Vivox).
				// We get the Windows binary path from the LL_WINE_SLVOICE
				// environment variable...
				// See install-wine-SLVoice.sh and viewer.conf in linux_tools/
				char* envvar = getenv("LL_WINE_SLVOICE");
				if (envvar)
				{
					full_path.assign(envvar);
					if (!full_path.empty())
					{
						size_t i = full_path.rfind('/');
						if (i == std::string::npos || i == 0)
						{
							llwarns << "Invalid LL_WINE_SLVOICE environment variable setting: '"
									<< full_path
									<< "' does not point to a program. Falling back to Linux SLVoice."
									<< llendl;
							full_path.clear();
						}
						else
						{
							exe_path = full_path.substr(0, i - 1);
						}
					}
				}
				if (full_path.empty())
				{
					if (gIsInSecondLife)
					{
						llwarns << "Using the deprecated Linux SLVoice binary. Expect voice to be flaky..."
								<< llendl;
					}
					full_path = exe_path + "/SLVoice";
					mDeprecatedClient = true;
				}
#endif
				// See if the vivox executable exists
				if (LLFile::isfile(full_path))
				{
					std::string host =
						gSavedSettings.getString("VivoxVoiceHost");
					// Port base, clamped to non-priviledged ports and so that
					// with the added cyclic offset, we are till below the
					// highest possible port number.
					U32 port =
						llclamp((U32)gSavedSettings.getU32("VivoxVoicePort"),
								1024U, 65435U);
					// Vivox executable exists. Build the command line and
					// launch the daemon.
					mProcess = new LLProcessLauncher();
					mProcess->setExecutable(full_path);
					mProcess->setWorkingDirectory(exe_path);
					// Add an auto-incremented offset at each new connection
					// so that the old connection port would not be reused
					// (when reconnecting at a short interval of time, such as
					// during retries) before it got time to close...
					// The offset is also randomized on first connection, so
					// to lower the risk of a port collision with other running
					// viewer sessions.
					static U32 portoffset = 49 + ll_rand(49); // 0 to 98
					port += portoffset++;
					portoffset %= 100; // Cycle the offset over 100 ports
					mProcess->addArgument("-i");
					mProcess->addArgument(llformat("%s:%d", host.c_str(),
												   port));
					S32 log_level = mLogLevel;
#if LL_LINUX
					if (mDeprecatedClient)
					{
						log_level = log_level == 0 ? -1 : 10;
					}
#endif
					mProcess->addArgument("-ll");
					mProcess->addArgument(llformat("%d", log_level));
#if LL_LINUX
					if (!mDeprecatedClient)
#endif
					{
						std::string log_dir =
							gDirUtilp->getExpandedFilename(LL_PATH_LOGS, "");
						mProcess->addArgument("-lf");
						mProcess->addArgument(log_dir);
						mProcess->addArgument("-lp");
						mProcess->addArgument("SLVoice");
						mProcess->addArgument("-ls");
						mProcess->addArgument(".log");
						S32 timeout =
							gSavedSettings.getU32("VivoxShutdownTimeout");
						mProcess->addArgument("-st");
						mProcess->addArgument(llformat("%d", timeout));
					}
					if (mProcess->launch() != 0)
					{
						llwarns << "Failure to launch SLVoice. Giving up."
								<< llendl;
						killDaemon();
						setState(stateJail);
						break;
					}
					mDaemonHost = LLHost(host, port);
				}
				else
				{
					llwarns << full_path << " not found. Giving up." << llendl;
					setState(stateJail);
					break;
				}

				mUpdateTimer.start();
				mUpdateTimer.setTimerExpirySec(CONNECT_THROTTLE_SECONDS);

				setState(stateDaemonLaunched);

				// Dirty the states we will need to sync with the daemon when
				// it comes up.
				mPTTDirty = true;
				mMicVolumeDirty = true;
				mSpeakerVolumeDirty = true;
				mSpeakerMuteDirty = true;
				// These only need to be set if they are not default (i.e.
				// empty strings).
				mCaptureDeviceDirty = !mCaptureDevice.empty();
				mRenderDeviceDirty = !mRenderDevice.empty();
				// Delay the first socket connection attempt to let the process
				// deamon start and listen for the socket. HB
				mUpdateTimer.start();
				mUpdateTimer.setTimerExpirySec(CONNECT_THROTTLE_SECONDS);
			}
			break;

		case stateDaemonLaunched:
			if (mUpdateTimer.hasExpired())
			{
				LL_DEBUGS("Voice") << "Connecting to vivox daemon" << LL_ENDL;
				if (!mSocket)
				{
					LL_DEBUGS("Voice") << "Creating socket to vivox daemon"
									   << LL_ENDL;
					mSocket = LLSocket::create(gAPRPoolp, LLSocket::STREAM_TCP);
				}

				mConnected = mSocket->blockingConnect(mDaemonHost);
				if (mConnected)
				{
					LL_DEBUGS("Voice") << "Connected to socket" << LL_ENDL;
					setState(stateConnecting);
					break;
				}

				LL_DEBUGS("Voice") << "Failure to connect to socket" << LL_ENDL;
				// If the connection failed, the socket may have been put into
				// a bad state; delete it.
				closeSocket();

				// Give up after 12 failed attempts in total. HB
				if (mRetries >= 12)
				{
					llwarns << "Too many retries. Giving up." << llendl;
					setState(stateJail);
					break;
				}

				// Every 3 failed connection retries, try and restart the
				// daemon itself... HB
				if (++mRetries % 3 == 0)
				{
					killDaemon();
					setState(stateStart);
				}

				mUpdateTimer.setTimerExpirySec(CONNECT_THROTTLE_SECONDS);
			}
			break;

		case stateConnecting:
			// Cannot do this until we have the pump available.
			if (mPump)
			{
				// Attach the pumps and pipes
				LLPumpIO::chain_t read_chain;
				read_chain.push_back(LLIOPipe::ptr_t(new LLIOSocketReader(mSocket)));
				read_chain.push_back(LLIOPipe::ptr_t(new LLVivoxProtocolParser()));

				mPump->addChain(read_chain, 0.f);	// 0.f =  never expire

				setState(stateConnected);
			}
			break;

		case stateConnected:
			// Initial devices query
			getCaptureDevicesSendMessage();
			getRenderDevicesSendMessage();
			setupVADParams();

			mLoginRetryCount = 0;

			setState(stateIdle);
			break;

		case stateIdle:
			// This is the idle state where we are connected to the daemon but
			// have not set up a connector yet.
			if (mTuningMode)
			{
				mTuningExitState = stateIdle;
				setState(stateMicTuningStart);
			}
			else if (!mVoiceEnabled)
			{
				// We never started up the connector. This will shut down the
				// daemon.
				setState(stateConnectorStopped);
			}
			else if (!mAccountName.empty() && mAccountPassword.empty())
			{
				requestVoiceAccountProvision();
			}
			break;

		case stateMicTuningStart:
			if (mUpdateTimer.hasExpired())
			{
				if (mCaptureDeviceDirty || mRenderDeviceDirty)
				{
					// These cannot be changed while in tuning mode. Set them
					// before starting.
					std::ostringstream stream;

					buildSetCaptureDevice(stream);
					buildSetRenderDevice(stream);

					if (!stream.str().empty())
					{
						writeString(stream.str());
					}

					// This will come around again in the same state and start
					// the capture, after the timer expires.
					mUpdateTimer.start();
					mUpdateTimer.setTimerExpirySec(UPDATE_THROTTLE_SECONDS);
				}
				else
				{
					// Duration parameter is currently unused, per Mike S.
					tuningCaptureStartSendMessage(10000);

					setState(stateMicTuningRunning);
				}
			}
			break;

		case stateMicTuningRunning:
			if (!mTuningMode || mCaptureDeviceDirty || mRenderDeviceDirty)
			{
				// All of these conditions make us leave tuning mode.
				setState(stateMicTuningStop);
			}
			else
			{
				// Process mic/speaker volume changes
				if (mTuningMicVolumeDirty || mTuningSpeakerVolumeDirty)
				{
					std::ostringstream stream;

					if (mTuningMicVolumeDirty)
					{
						llinfos << "Setting tuning mic level to "
								<< mTuningMicVolume << llendl;
						stream << "<Request requestId=\"" << mCommandCookie++
							   << "\" action=\"Aux.SetMicLevel.1\"><Level>"
							   << mTuningMicVolume
							   << "</Level></Request>\n\n\n";
					}

					if (mTuningSpeakerVolumeDirty)
					{
						stream << "<Request requestId=\"" << mCommandCookie++
							   << "\" action=\"Aux.SetSpeakerLevel.1\">"
							   << "<Level>" << mTuningSpeakerVolume
							   << "</Level></Request>\n\n\n";
					}

					mTuningMicVolumeDirty = false;
					mTuningSpeakerVolumeDirty = false;

					if (!stream.str().empty())
					{
						writeString(stream.str());
					}
				}
			}
			break;

		case stateMicTuningStop:
		{
			// Transition out of mic tuning
			tuningCaptureStopSendMessage();

			setState(mTuningExitState);

			// If we exited just to change devices, this will keep us from
			// re-entering too fast.
			mUpdateTimer.start();
			mUpdateTimer.setTimerExpirySec(UPDATE_THROTTLE_SECONDS);
			break;
		}

		case stateConnectorStart:
			if (!mVoiceEnabled)
			{
				// We were never logged in. This will shut down the connector.
				setState(stateLoggedOut);
			}
			else if (!mVoiceAccountServerURI.empty())
			{
				connectorCreate();
			}
			break;

		case stateConnectorStarting:
			// Waiting for connector handle connectorCreateResponse() will
			// transition from here to stateConnectorStarted.
			break;

		case stateConnectorStarted:		// Connector handle received
			if (!mVoiceEnabled)
			{
				// We were never logged in. This will shut down the connector.
				setState(stateLoggedOut);
			}
			else
			{
				// The connector is started. Send a login message.
				setState(stateNeedsLogin);
			}
			break;

		case stateLoginRetry:
			if (mLoginRetryCount == 0)
			{
				// First retry: display a message to the user
				notifyStatusObservers(LLVoiceClientStatusObserver::STATUS_LOGIN_RETRY);
			}

			++mLoginRetryCount;

			if (mLoginRetryCount > MAX_LOGIN_RETRIES)
			{
				llwarns << "Too many login retries, giving up." << llendl;
				setState(stateLoginFailed);
			}
			else
			{
				llinfos << "Will retry login in " << LOGIN_RETRY_SECONDS
						<< " seconds." << llendl;
				mUpdateTimer.start();
				mUpdateTimer.setTimerExpirySec(LOGIN_RETRY_SECONDS);
				setState(stateLoginRetryWait);
			}
			break;

		case stateLoginRetryWait:
			if (mUpdateTimer.hasExpired())
			{
				setState(stateNeedsLogin);
			}
			break;

		case stateNeedsLogin:
			if (!mAccountPassword.empty())
			{
				setState(stateLoggingIn);
				loginSendMessage();
			}
			break;

		case stateLoggingIn:			// Waiting for account handle
			// loginResponse() will transition from here to stateLoggedIn.
			break;

		case stateLoggedIn:				// Account handle received
		{
			notifyStatusObservers(LLVoiceClientStatusObserver::STATUS_LOGGED_IN);

			// Set up the mute list observer if it has not been set up already.
			if (!sMuteListListening)
			{
				LLMuteList::addObserver(&sMutelistListener);
				sMuteListListening = true;
			}

			// Set the initial state of mic mute, local speaker volume, etc.
			std::ostringstream stream;
			buildLocalAudioUpdates(stream);
			if (!stream.str().empty())
			{
				writeString(stream.str());
			}

			setState(stateNoChannel);

			// Initial kick-off of channel lookup logic
			parcelChanged();
			break;
		}

		case stateNoChannel:
			if (mSessionTerminateRequested || !mVoiceEnabled)
			{
				// *TODO: is this the right way out of this state ?
				setState(stateSessionTerminated);
			}
			else if (mTuningMode)
			{
				mTuningExitState = stateNoChannel;
				setState(stateMicTuningStart);
			}
			else if (sessionNeedsRelog(mNextAudioSession))
			{
				requestRelog();
				setState(stateSessionTerminated);
			}
			else if (mNextAudioSession)
			{
				sessionState* oldSession = mAudioSession;

				mAudioSession = mNextAudioSession;
				if (!mAudioSession->mReconnect)
				{
					mNextAudioSession = NULL;
				}

				// The old session may now need to be deleted.
				reapSession(oldSession);

				if (!mAudioSession->mHandle.empty())
				{
					// Connect to a session by session handle

					sessionMediaConnectSendMessage(mAudioSession);
				}
				else
				{
					// Connect to a session by URI
					sessionCreateSendMessage(mAudioSession, true, false);
				}

				notifyStatusObservers(LLVoiceClientStatusObserver::STATUS_JOINING);
				setState(stateJoiningSession);
			}
			else if (!mSpatialSessionURI.empty())
			{
				// If we're not headed elsewhere and have a spatial URI,
				// return to spatial.
				switchChannel(mSpatialSessionURI, true, false, false,
							  mSpatialSessionCredentials);
			}
			break;

		case stateJoiningSession:		// Waiting for session handle
			// joinedAudioSession() will transition from here to
			// stateSessionJoined.
			if (!mVoiceEnabled)
			{
				// User bailed out during connect -- jump straight to teardown.
				setState(stateSessionTerminated);
			}
			else if (mSessionTerminateRequested)
			{
				if (mAudioSession && !mAudioSession->mHandle.empty())
				{
					// Only allow direct exits from this state in P2P calls
					// (for cancelling an invite).
					// Terminating a half-connected session on other types of
					// calls seems to break something in the vivox gateway.
					if (mAudioSession->mIsP2P)
					{
						sessionMediaDisconnectSendMessage(mAudioSession);
						setState(stateSessionTerminated);
					}
				}
			}
			break;

		case stateSessionJoined:		// Session handle received
			// It appears that we need to wait for BOTH the
			// SessionGroup.AddSession response and the
			// SessionStateChangeEvent with state 4 before continuing from
			// this state. They can happen in either order, and if we do not
			// wait for both, things can get stuck.
			// For now, the SessionGroup.AddSession response handler sets
			// mSessionHandle and the SessionStateChangeEvent handler
			// transitions to stateSessionJoined.
			// This is a cheap way to make sure both have happened before
			// proceeding.
			if (mAudioSession && mAudioSession->mVoiceEnabled)
			{
				// Dirty state that may need to be sync'ed with the daemon.
				mPTTDirty = true;
				mSpeakerVolumeDirty = true;
				mSpatialCoordsDirty = true;

				setState(stateRunning);

				// Start the throttle timer
				mUpdateTimer.start();
				mUpdateTimer.setTimerExpirySec(UPDATE_THROTTLE_SECONDS);

				// Events that need to happen when a session is joined could go
				// here. Maybe send initial spatial data ?
				notifyStatusObservers(LLVoiceClientStatusObserver::STATUS_JOINED);

			}
			else if (!mVoiceEnabled)
			{
				// User bailed out during connect; jump straight to teardown.
				setState(stateSessionTerminated);
			}
			else if (mSessionTerminateRequested)
			{
				// Only allow direct exits from this state in P2P calls (for
				// cancelling an invite).
				// Terminating a half-connected session on other types of calls
				// seems to break something in the vivox gateway.
				if (mAudioSession && mAudioSession->mIsP2P)
				{
					sessionMediaDisconnectSendMessage(mAudioSession);
					setState(stateSessionTerminated);
				}
			}
			break;

		case stateRunning:				// Steady state
			// Disabling voice or disconnect requested.
			if (!mVoiceEnabled || mSessionTerminateRequested)
			{
				leaveAudioSession();
			}
			else
			{
				// Figure out whether the PTT state needs to change
				bool new_ptt;
				if (mUsePTT)
				{
					// If configured to use PTT, track the user state.
					new_ptt = mUserPTTState;
				}
				else
				{
					// If not configured to use PTT, it should always be true
					// (otherwise the user will be unable to speak).
					new_ptt = true;
				}
				if (mMuteMic)
				{
					// This always overrides any other PTT setting.
					new_ptt = false;
				}
				// Dirty if state changed.
				if (new_ptt != mPTT)
				{
					mPTT = new_ptt;
					mPTTDirty = true;
				}

				if (!inSpatialChannel())
				{
					// When in a non-spatial channel, never send positional
					// updates.
					mSpatialCoordsDirty = false;
				}
				else
				{
					// Do the calculation that enforces the listener<->speaker
					// tether (and also updates the real camera position)
					enforceTether();
				}

				// Send an update if the ptt state has changed (which should
				// not be able to happen that often; the user can only click so
				// fast) or every 10hz, whichever is sooner.
				if ((mAudioSession && mAudioSession->mVolumeDirty) ||
					mPTTDirty || mSpeakerVolumeDirty ||
					mUpdateTimer.hasExpired())
				{
					mUpdateTimer.setTimerExpirySec(UPDATE_THROTTLE_SECONDS);
					sendPositionalUpdate();
				}
				// Dity hack to get voice to initialize properly after login
				if (first_run)
				{
					first_run = false;
					LLVoiceChannel::suspend();
					LLVoiceChannel::resume();
				}
			}
			break;

		case stateLeavingSession:	// Waiting for terminate session response
			// The handler for the Session.Terminate response will transition
			// from here to stateSessionTerminated.
			break;

		case stateSessionTerminated:
			// Must do this first, since it uses mAudioSession.
			notifyStatusObservers(LLVoiceClientStatusObserver::STATUS_LEFT_CHANNEL);

			if (mAudioSession)
			{
				sessionState* oldSession = mAudioSession;

				mAudioSession = NULL;

				// The old session may now need to be deleted.
				reapSession(oldSession);
			}
			else
			{
				llwarns << "stateSessionTerminated with NULL mAudioSession"
						<< llendl;
			}

			// Always reset the terminate request flag when we get here.
			mSessionTerminateRequested = false;

			if (mVoiceEnabled && !mRelogRequested)
			{
				// Just leaving a channel, go back to stateNoChannel (the
				// "logged in but have no channel" state).
				setState(stateNoChannel);
			}
			else
			{
				// Shutting down voice, continue with disconnecting.
				logout();

				// The state machine will take it from here
				mRelogRequested = false;
			}
			break;

		case stateLoggingOut:			// Waiting for logout response
			// The handler for the AccountLoginStateChangeEvent will transition
			// from here to stateLoggedOut.
			break;

		case stateLoggedOut:			// Logout response received
			// Once we are logged out, all these things are invalid.
			mAccountLoggedIn = false;
			deleteAllSessions();

			if (mVoiceEnabled && !mRelogRequested)
			{
				// User was logged out, but wants to be logged in. Send a new
				// login request.
				setState(stateNeedsLogin);
			}
			else
			{
				// Shut down the connector
				connectorShutdown();
			}
			break;

		case stateConnectorStopping:	// Waiting for connector stop
			// The handler for the Connector.InitiateShutdown response will
			// transition from here to stateConnectorStopped.
			break;

		case stateConnectorStopped:		// Connector stop received
			setState(stateDisableCleanup);
			break;

		case stateConnectorFailed:
			setState(stateConnectorFailedWaiting);
			break;

		case stateConnectorFailedWaiting:
			if (!mVoiceEnabled)
			{
				setState(stateDisableCleanup);
			}
			break;

		case stateLoginFailed:
			setState(stateLoginFailedWaiting);
			break;

		case stateLoginFailedWaiting:
			if (!mVoiceEnabled)
			{
				setState(stateDisableCleanup);
			}
			break;

		case stateJoinSessionFailed:
			// Transition to error state. Send out any notifications here.
			if (mAudioSession)
			{
				llwarns << "stateJoinSessionFailed: ("
						<< mAudioSession->mErrorStatusCode << "): "
						<< mAudioSession->mErrorStatusString << llendl;
			}
			else
			{
				llwarns << "stateJoinSessionFailed with no current session"
						<< llendl;
			}
			notifyStatusObservers(LLVoiceClientStatusObserver::ERROR_UNKNOWN);
			setState(stateJoinSessionFailedWaiting);
			break;

		case stateJoinSessionFailedWaiting:
			// Joining a channel failed, either due to a failed channel name ->
			// sip url lookup or an error from the join message.
			// Region crossings may leave this state and try the join again.
			if (mSessionTerminateRequested)
			{
				setState(stateSessionTerminated);
			}
			break;

		case stateJail:
			// We have given up. Do nothing.
			break;
	}
}

void LLVoiceClient::closeSocket()
{
	mSocket.reset();
	mConnected = mConnectorEstablished = mAccountLoggedIn = false;
}

void LLVoiceClient::loginSendMessage()
{
	std::ostringstream stream;
	stream << "<Request requestId=\"" << mCommandCookie++
		   << "\" action=\"Account.Login.1\"><ConnectorHandle>"
		   << mConnectorHandle << "</ConnectorHandle><AccountName>"
		   << mAccountName << "</AccountName><AccountPassword>"
		   << mAccountPassword << "</AccountPassword>"
		   << "<AccountHandle>" << mAccountHandle << "</AccountHandle>"
		   << "<AudioSessionAnswerMode>VerifyAnswer</AudioSessionAnswerMode>"
		   << "<EnableBuddiesAndPresence>false</EnableBuddiesAndPresence>"
		   << "<BuddyManagementMode>Application</BuddyManagementMode>"
		   << "<ParticipantPropertyFrequency>5</ParticipantPropertyFrequency>"
		   << "</Request>\n\n\n";
	writeString(stream.str());
}

void LLVoiceClient::logout()
{
	// Ensure that we will re-request provisioning before logging in again
	mAccountPassword.clear();
	mVoiceAccountServerURI.clear();

	setState(stateLoggingOut);
	logoutSendMessage();
}

void LLVoiceClient::logoutSendMessage()
{
	if (mAccountLoggedIn)
	{
		std::ostringstream stream;
		stream << "<Request requestId=\"" << mCommandCookie++
			   << "\" action=\"Account.Logout.1\"><AccountHandle>"
			   << mAccountHandle << "</AccountHandle></Request>\n\n\n";

		mAccountLoggedIn = false;

		writeString(stream.str());
	}
}

void LLVoiceClient::sessionCreateSendMessage(sessionState* session,
											 bool start_audio,
											 bool start_text)
{
	LL_DEBUGS("Voice") << "Requesting create: " << session->mSIPURI << LL_ENDL;

	session->mCreateInProgress = true;
	if (start_audio)
	{
		session->mMediaConnectInProgress = true;
	}

	std::ostringstream stream;
	stream << "<Request requestId=\"" << session->mSIPURI
		   << "\" action=\"Session.Create.1\"><AccountHandle>"
		   << mAccountHandle << "</AccountHandle><URI>" << session->mSIPURI
		   << "</URI>";

	static const std::string allowed_chars =
				"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
				"0123456789"
				"-._~";

	if (!session->mHash.empty())
	{
		stream << "<Password>" << LLURI::escape(session->mHash, allowed_chars)
			   << "</Password>"
			   << "<PasswordHashAlgorithm>SHA1UserName</PasswordHashAlgorithm>";
	}

	stream << "<ConnectAudio>" << (start_audio ? "true" : "false")
		   << "</ConnectAudio><ConnectText>" << (start_text ? "true" : "false")
		   << "</ConnectText><Name>" << mChannelName
		   << "</Name><VoiceFontID>0</VoiceFontID></Request>\n\n\n";
	writeString(stream.str());
}

void LLVoiceClient::sessionGroupAddSessionSendMessage(sessionState* session,
													  bool start_audio,
													  bool start_text)
{
	LL_DEBUGS("Voice") << "Requesting create: " << session->mSIPURI << LL_ENDL;

	session->mCreateInProgress = true;
	if (start_audio)
	{
		session->mMediaConnectInProgress = true;
	}

	std::string password;
	if (!session->mHash.empty())
	{
		static const std::string allowed_chars =
					"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"
					"0123456789"
					"-._~";
		password = LLURI::escape(session->mHash, allowed_chars);
	}

	std::ostringstream stream;
	stream << "<Request requestId=\"" << session->mSIPURI
		   << "\" action=\"SessionGroup.AddSession.1\"><SessionGroupHandle>"
		   << session->mGroupHandle << "</SessionGroupHandle><URI>"
		   << session->mSIPURI << "</URI><Name>" << mChannelName
		   << "</Name><ConnectAudio>" << (start_audio ? "true" : "false")
		   << "</ConnectAudio><ConnectText>" << (start_text ? "true" : "false")
		   << "<VoiceFontID>0</VoiceFontID></ConnectText><Password>"
		   << password << "</Password>"
		   << "<PasswordHashAlgorithm>SHA1UserName</PasswordHashAlgorithm>"
		   << "</Request>\n\n\n";
	writeString(stream.str());
}

void LLVoiceClient::sessionMediaConnectSendMessage(sessionState* session)
{
	LL_DEBUGS("Voice") << "Connecting audio to session handle: "
					   << session->mHandle << LL_ENDL;

	session->mMediaConnectInProgress = true;

	std::ostringstream stream;
	stream << "<Request requestId=\"" << session->mHandle
		   << "\" action=\"Session.MediaConnect.1\"><SessionGroupHandle>"
		   << session->mGroupHandle << "</SessionGroupHandle><SessionHandle>"
		   << session->mHandle << "</SessionHandle><VoiceFontID>0</VoiceFontID>"
		   <<"<Media>Audio</Media></Request>\n\n\n";
	writeString(stream.str());
}

void LLVoiceClient::sessionTextConnectSendMessage(sessionState* session)
{
	LL_DEBUGS("Voice") << "Connecting text to session handle: "
					   << session->mHandle << LL_ENDL;

	std::ostringstream stream;
	stream << "<Request requestId=\"" << session->mHandle
		   << "\" action=\"Session.TextConnect.1\"><SessionGroupHandle>"
		   << session->mGroupHandle << "</SessionGroupHandle><SessionHandle>"
		   << session->mHandle << "</SessionHandle></Request>\n\n\n";
	writeString(stream.str());
}

void LLVoiceClient::sessionTerminate()
{
	mSessionTerminateRequested = true;
}

void LLVoiceClient::requestRelog()
{
	mSessionTerminateRequested = true;
	mRelogRequested = true;
}

void LLVoiceClient::leaveAudioSession()
{
	if (mAudioSession)
	{
		LL_DEBUGS("Voice") << "Leaving session: " << mAudioSession->mSIPURI
						   << LL_ENDL;

		switch (mState)
		{
			case stateNoChannel:
				// In this case, we want to pretend the join failed so our
				// state machine does not get stuck.
				// Skip the join failed transition state so we do not send out
				// error notifications.
				setState(stateJoinSessionFailedWaiting);
				break;

			case stateJoiningSession:
			case stateSessionJoined:
			case stateRunning:
				if (!mAudioSession->mHandle.empty())
				{
					sessionMediaDisconnectSendMessage(mAudioSession);
					setState(stateLeavingSession);
				}
				else
				{
					llwarns << "Called without session handle" << llendl;
					setState(stateSessionTerminated);
				}
				break;

			case stateJoinSessionFailed:
			case stateJoinSessionFailedWaiting:
				setState(stateSessionTerminated);
				break;

			default:
				llwarns << "Called from unknown state" << llendl;
				break;
		}
	}
	else
	{
		llwarns << "Called with no active session" << llendl;
		setState(stateSessionTerminated);
	}
}

#if 0	// Not used
void LLVoiceClient::sessionTerminateSendMessage(sessionState* session)
{
	LL_DEBUGS("Voice") << "Sending Session.Terminate with handle "
					   << session->mHandle << LL_ENDL;

	std::ostringstream stream;
	stream << "<Request requestId=\"" << mCommandCookie++
		   << "\" action=\"Session.Terminate.1\"><SessionHandle>"
		   << session->mHandle << "</SessionHandle></Request>\n\n\n";
	writeString(stream.str());
}
#endif

void LLVoiceClient::sessionGroupTerminateSendMessage(sessionState* session)
{
	LL_DEBUGS("Voice") << "Sending SessionGroup.Terminate with handle "
					   << session->mGroupHandle << LL_ENDL;

	std::ostringstream stream;
	stream << "<Request requestId=\"" << mCommandCookie++
		   << "\" action=\"SessionGroup.Terminate.1\"><SessionGroupHandle>"
		   << session->mGroupHandle << "</SessionGroupHandle></Request>\n\n\n";
	writeString(stream.str());
}

void LLVoiceClient::sessionMediaDisconnectSendMessage(sessionState* session)
{
#if 0
	LL_DEBUGS("Voice") << "Sending Session.MediaDisconnect with handle "
					   << session->mHandle << LL_ENDL;

	std::ostringstream stream;
	stream << "<Request requestId=\"" << mCommandCookie++
		   << "\" action=\"Session.MediaDisconnect.1\"><SessionGroupHandle>"
		   << session->mGroupHandle << "</SessionGroupHandle><SessionHandle>"
		   << session->mHandle << "</SessionHandle><Media>Audio</Media>"
		   << "</Request>\n\n\n";
	writeString(stream.str());
#else
	sessionGroupTerminateSendMessage(session);
#endif
}

void LLVoiceClient::getCaptureDevicesSendMessage()
{
	std::ostringstream stream;
	stream << "<Request requestId=\"" << mCommandCookie++
		   << "\" action=\"Aux.GetCaptureDevices.1\"></Request>\n\n\n";
	writeString(stream.str());
}

void LLVoiceClient::getRenderDevicesSendMessage()
{
	std::ostringstream stream;
	stream << "<Request requestId=\"" << mCommandCookie++
		   << "\" action=\"Aux.GetRenderDevices.1\"></Request>\n\n\n";
	writeString(stream.str());
}

void LLVoiceClient::setCaptureDevice(const std::string& name)
{
	if (name == "Default")
	{
		if (!mCaptureDevice.empty())
		{
			mCaptureDevice.clear();
			mCaptureDeviceDirty = true;
		}
	}
	else
	{
		if (mCaptureDevice != name)
		{
			mCaptureDevice = name;
			mCaptureDeviceDirty = true;
		}
	}
}

void LLVoiceClient::setRenderDevice(const std::string& name)
{
	if (name == "Default")
	{
		if (!mRenderDevice.empty())
		{
			mRenderDevice.clear();
			mRenderDeviceDirty = true;
		}
	}
	else
	{
		if (mRenderDevice != name)
		{
			mRenderDevice = name;
			mRenderDeviceDirty = true;
		}
	}
}

void LLVoiceClient::tuningStart()
{
	mTuningMode = true;
	if (mState >= stateNoChannel)
	{
		sessionTerminate();
	}
}

bool LLVoiceClient::inTuningMode()
{
	return mState == stateMicTuningRunning;
}

void LLVoiceClient::tuningRenderStartSendMessage(const std::string& name,
												 bool loop)
{
	mTuningAudioFile = name;
	std::ostringstream stream;
	stream	<< "<Request requestId=\"" << mCommandCookie++
			<< "\" action=\"Aux.RenderAudioStart.1\"><SoundFilePath>"
		    << mTuningAudioFile << "</SoundFilePath><Loop>"
			<< (loop ? "1" : "0") << "</Loop></Request>\n\n\n";
	writeString(stream.str());
}

void LLVoiceClient::tuningRenderStopSendMessage()
{
	std::ostringstream stream;
	stream << "<Request requestId=\"" << mCommandCookie++
		   << "\" action=\"Aux.RenderAudioStop.1\"><SoundFilePath>"
		   << mTuningAudioFile << "</SoundFilePath></Request>\n\n\n";
	writeString(stream.str());
}

void LLVoiceClient::tuningCaptureStartSendMessage(S32 duration)
{
	LL_DEBUGS("Voice") << "Sending CaptureAudioStart" << LL_ENDL;

	std::ostringstream stream;
	stream << "<Request requestId=\"" << mCommandCookie++
		   << "\" action=\"Aux.CaptureAudioStart.1\"><Duration>"
		   << duration << "</Duration></Request>\n\n\n";
	writeString(stream.str());
}

void LLVoiceClient::tuningCaptureStopSendMessage()
{
	LL_DEBUGS("Voice") << "Sending CaptureAudioStop" << LL_ENDL;

	std::ostringstream stream;
	stream << "<Request requestId=\"" << mCommandCookie++
		   << "\" action=\"Aux.CaptureAudioStop.1\"></Request>\n\n\n";
	writeString(stream.str());

	mTuningEnergy = 0.f;
}

void LLVoiceClient::tuningSetMicVolume(F32 volume)
{
	S32 scaled_volume = scale_mic_volume(volume);
	if (scaled_volume != mTuningMicVolume)
	{
		mTuningMicVolume = scaled_volume;
		mTuningMicVolumeDirty = true;
	}
}

#if 0	// Not used
void LLVoiceClient::tuningSetSpeakerVolume(F32 volume)
{
	S32 scaled_volume = scale_speaker_volume(volume);
	if (scaled_volume != mTuningSpeakerVolume)
	{
		mTuningSpeakerVolume = scaled_volume;
		mTuningSpeakerVolumeDirty = true;
	}
}
#endif

bool LLVoiceClient::deviceSettingsAvailable()
{
	return mConnected && !mRenderDevices.empty();
}

void LLVoiceClient::refreshDeviceLists(bool clearCurrentList)
{
	if (clearCurrentList)
	{
		clearCaptureDevices();
		clearRenderDevices();
	}
	getCaptureDevicesSendMessage();
	getRenderDevicesSendMessage();
}

void LLVoiceClient::daemonDied()
{
	// The daemon died, so the connection is gone.  Reset everything and start
	// over.
	llwarns << "Connection to Vivox daemon lost. Resetting state." << llendl;

	// Try to relaunch the daemon
	setState(stateDisableCleanup);
}

void LLVoiceClient::giveUp()
{
	// Avoid infinite loop while giving up...
	static bool giving_up = false;
	if (!giving_up)
	{
		giving_up = true;
		// All has failed. Clean up and stop trying.
		closeSocket();
		deleteAllSessions();
		setState(stateJail);
		llwarns << "Unrecoverable error: voice permanently disabled."
				<< llendl;
	}
}

static void oldSDKTransform(LLVector3& left, LLVector3& up, LLVector3& at,
							LLVector3d& pos, LLVector3& vel)
{
	// the new at, up, left vectors and the new position and velocity
	F32 nat[3], nup[3], nl[3];
	F64 npos[3];

	// The original XML command was sent like this:
	/*
			<< "<Position>"
				<< "<X>" << pos[VX] << "</X>"
				<< "<Y>" << pos[VZ] << "</Y>"
				<< "<Z>" << pos[VY] << "</Z>"
			<< "</Position>"
			<< "<Velocity>"
				<< "<X>" << mAvatarVelocity[VX] << "</X>"
				<< "<Y>" << mAvatarVelocity[VZ] << "</Y>"
				<< "<Z>" << mAvatarVelocity[VY] << "</Z>"
			<< "</Velocity>"
			<< "<AtOrientation>"
				<< "<X>" << l.mV[VX] << "</X>"
				<< "<Y>" << u.mV[VX] << "</Y>"
				<< "<Z>" << a.mV[VX] << "</Z>"
			<< "</AtOrientation>"
			<< "<UpOrientation>"
				<< "<X>" << l.mV[VZ] << "</X>"
				<< "<Y>" << u.mV[VY] << "</Y>"
				<< "<Z>" << a.mV[VZ] << "</Z>"
			<< "</UpOrientation>"
			<< "<LeftOrientation>"
				<< "<X>" << l.mV [VY] << "</X>"
				<< "<Y>" << u.mV [VZ] << "</Y>"
				<< "<Z>" << a.mV [VY] << "</Z>"
			<< "</LeftOrientation>";
	*/

#if 1
	// This was the original transform done when building the XML command
	nat[0] = left.mV[VX];
	nat[1] = up.mV[VX];
	nat[2] = at.mV[VX];

	nup[0] = left.mV[VZ];
	nup[1] = up.mV[VY];
	nup[2] = at.mV[VZ];

	nl[0] = left.mV[VY];
	nl[1] = up.mV[VZ];
	nl[2] = at.mV[VY];

	npos[0] = pos.mdV[VX];
	npos[1] = pos.mdV[VZ];
	npos[2] = pos.mdV[VY];

	for (S32 i = 0; i < 3; ++i)
	{
		at.mV[i] = nat[i];
		up.mV[i] = nup[i];
		left.mV[i] = nl[i];
		pos.mdV[i] = npos[i];
	}

	// This was the original transform done in the SDK
	nat[0] = at.mV[2];
	nat[1] = 0; // y component of at vector is always 0, this was up[2]
	nat[2] = -1 * left.mV[2];

	// We override whatever the application gives us
	nup[0] = 0; // x component of up vector is always 0
	nup[1] = 1; // y component of up vector is always 1
	nup[2] = 0; // z component of up vector is always 0

	nl[0] = at.mV[0];
	nl[1] = 0;  // y component of left vector is always zero, this was up[0]
	nl[2] = -1 * left.mV[0];

	npos[2] = pos.mdV[2] * -1.0;
	npos[1] = pos.mdV[1];
	npos[0] = pos.mdV[0];

	for (S32 i = 0; i < 3; ++i)
	{
		at.mV[i] = nat[i];
		up.mV[i] = nup[i];
		left.mV[i] = nl[i];
		pos.mdV[i] = npos[i];
	}
#else
	// This is the compose of the two transforms (at least, that's what I'm trying for)
	nat[0] = at.mV[VX];
	nat[1] = 0; // y component of at vector is always 0, this was up[2]
	nat[2] = -1 * up.mV[VZ];

	// We override whatever the application gives us
	nup[0] = 0; // x component of up vector is always 0
	nup[1] = 1; // y component of up vector is always 1
	nup[2] = 0; // z component of up vector is always 0

	nl[0] = left.mV[VX];
	nl[1] = 0;  // y component of left vector is always zero, this was up[0]
	nl[2] = -1 * left.mV[VY];

	npos[0] = pos.mdV[VX];
	npos[1] = pos.mdV[VZ];
	npos[2] = pos.mdV[VY] * -1.0;

	for (S32 i = 0; i < 3; ++i)
	{
		at.mV[i] = nat[i];
		up.mV[i] = nup[i];
		left.mV[i] = nl[i];
		pos.mdV[i] = npos[i];
	}
#endif
}

void LLVoiceClient::sendPositionalUpdate()
{
	std::ostringstream stream;

	if (mSpatialCoordsDirty)
	{
		LLVector3 l, u, a, vel;
		LLVector3d pos;

		mSpatialCoordsDirty = false;

		// Always send both speaker and listener positions together.
		stream << "<Request requestId=\"" << mCommandCookie++
			   << "\" action=\"Session.Set3DPosition.1\">"
			   << "<SessionHandle>" << getAudioSessionHandle()
			   << "</SessionHandle>";

		stream << "<SpeakerPosition>";

		l = mAvatarRot.getLeftRow();
		u = mAvatarRot.getUpRow();
		a = mAvatarRot.getFwdRow();
		pos = mAvatarPosition;
		vel = mAvatarVelocity;

		// SLIM SDK: the old SDK was doing a transform on the passed
		// coordinates that the new one does not do anymore. The old transform
		// is replicated by this function.
		oldSDKTransform(l, u, a, pos, vel);

		stream << "<Position><X>" << pos.mdV[VX] << "</X><Y>" << pos.mdV[VY]
			   << "</Y><Z>" << pos.mdV[VZ] << "</Z></Position><Velocity><X>"
			   << vel.mV[VX] << "</X><Y>" << vel.mV[VY] << "</Y><Z>"
			   << vel.mV[VZ] << "</Z></Velocity><AtOrientation><X>" << a.mV[VX]
			   << "</X><Y>" << a.mV[VY] << "</Y><Z>" << a.mV[VZ]
			   << "</Z></AtOrientation><UpOrientation><X>" << u.mV[VX]
			   << "</X><Y>" << u.mV[VY] << "</Y><Z>" << u.mV[VZ]
			   << "</Z></UpOrientation><LeftOrientation><X>" << l.mV[VX]
			   << "</X><Y>" << l.mV[VY] << "</Y><Z>" << l.mV[VZ]
			   << "</Z></LeftOrientation>";

		stream << "</SpeakerPosition><ListenerPosition>";

		LLVector3d ear_pos;
		LLVector3 ear_vel;
		LLMatrix3 ear_rot;
		switch (mEarLocation)
		{
			case earLocAvatar:
				ear_pos = mAvatarPosition;
				ear_vel = mAvatarVelocity;
				ear_rot = mAvatarRot;
				break;

			case earLocMixed:
				ear_pos = mAvatarPosition;
				ear_vel = mAvatarVelocity;
				ear_rot = mCameraRot;
				break;

			case earLocCamera:
			default:
				ear_pos = mCameraPosition;
				ear_vel = mCameraVelocity;
				ear_rot = mCameraRot;
		}

		l = ear_rot.getLeftRow();
		u = ear_rot.getUpRow();
		a = ear_rot.getFwdRow();
		pos = ear_pos;
		vel = ear_vel;

		oldSDKTransform(l, u, a, pos, vel);

		stream << "<Position><X>" << pos.mdV[VX] << "</X><Y>" << pos.mdV[VY]
			   << "</Y><Z>" << pos.mdV[VZ] << "</Z></Position><Velocity><X>"
			   << vel.mV[VX] << "</X><Y>" << vel.mV[VY] << "</Y><Z>"
			   << vel.mV[VZ] << "</Z></Velocity><AtOrientation><X>" << a.mV[VX]
			   << "</X><Y>" << a.mV[VY] << "</Y><Z>" << a.mV[VZ]
			   << "</Z></AtOrientation><UpOrientation><X>" << u.mV[VX]
			   << "</X><Y>" << u.mV[VY] << "</Y><Z>" << u.mV[VZ]
			   << "</Z></UpOrientation><LeftOrientation><X>" << l.mV[VX]
			   << "</X><Y>" << l.mV[VY] << "</Y><Z>" << l.mV[VZ]
			   << "</Z></LeftOrientation>";

		stream << "</ListenerPosition></Request>\n\n\n";
	}

	if (mAudioSession && mAudioSession->mVolumeDirty)
	{
		particip_map_t::iterator iter =
			mAudioSession->mParticipantsByURI.begin();

		mAudioSession->mVolumeDirty = false;

		for ( ; iter != mAudioSession->mParticipantsByURI.end(); ++iter)
		{
			participantState* p = iter->second;

			if (p->mVolumeDirty)
			{
				// Cannot set volume/mute for yourself
				if (!p->mIsSelf)
				{
					S32 volume = 56; // nominal default value
					bool mute = p->mOnMuteList;

					if (p->mUserVolume != -1)
					{
						// Scale from user volume in the range 0-400 (with 100
						// as "normal") to vivox volume in the range 0-100
						// (with 56 as "normal")
						if (p->mUserVolume < 100)
						{
							volume = (p->mUserVolume * 56) / 100;
						}
						else
						{
							volume = 44 * (p->mUserVolume - 100) / 300 + 56;
						}
					}
					else if (p->mVolume != -1)
					{
						// Use the previously reported internal volume (comes
						// in with a ParticipantUpdatedEvent)
						volume = p->mVolume;
					}

					if (mute)
					{
						// SetParticipantMuteForMe does not work in P2P
						// sessions. If we want the user to be muted, set their
						// volume to 0 as well. This is not perfect, but it
						// will at least reduce their volume to a minimum.
						volume = 0;
					}

					if (volume <= 0)
					{
						mute = true;
					}

					LL_DEBUGS("Voice") << "Setting volume/mute for avatar "
									   << p->mAvatarID << " to " << volume
									   << (mute ? "/true" : "/false")
									   << LL_ENDL;

					// SLIM SDK: Send both volume and mute commands.

					// Send a "volume for me" command for the user.
					stream << "<Request requestId=\"" << mCommandCookie++
						   << "\" action=\"Session.SetParticipantVolumeForMe.1\">"
						   << "<SessionHandle>" << getAudioSessionHandle()
						   << "</SessionHandle><ParticipantURI>"
						   << p->mURI << "</ParticipantURI><Volume>"
						   << volume << "</Volume></Request>\n\n\n";

					// Send a "mute for me" command for the user
					stream << "<Request requestId=\"" << mCommandCookie++
						   << "\" action=\"Session.SetParticipantMuteForMe.1\">"
						   << "<SessionHandle>" << getAudioSessionHandle()
						   << "</SessionHandle><ParticipantURI>"
						   << p->mURI << "</ParticipantURI><Mute>"
						   << (mute ? "1" : "0") << "</Mute></Request>\n\n\n";
				}

				p->mVolumeDirty = false;
			}
		}
	}

	buildLocalAudioUpdates(stream);

	if (!stream.str().empty())
	{
		writeString(stream.str());
	}
}

void LLVoiceClient::buildSetCaptureDevice(std::ostringstream& stream)
{
	if (mCaptureDeviceDirty)
	{
		LL_DEBUGS("Voice") << "Setting input device = \"" << mCaptureDevice
						   << "\"" << LL_ENDL;

		stream << "<Request requestId=\"" << mCommandCookie++
			   << "\" action=\"Aux.SetCaptureDevice.1\">"
			   << "<CaptureDeviceSpecifier>" << mCaptureDevice
			   << "</CaptureDeviceSpecifier></Request>\n\n\n";

		mCaptureDeviceDirty = false;
	}
}

void LLVoiceClient::buildSetRenderDevice(std::ostringstream& stream)
{
	if (mRenderDeviceDirty)
	{
		LL_DEBUGS("Voice") << "Setting output device = \"" << mRenderDevice
						   << "\"" << LL_ENDL;

		stream << "<Request requestId=\"" << mCommandCookie++
			   << "\" action=\"Aux.SetRenderDevice.1\"><RenderDeviceSpecifier>"
			   << mRenderDevice << "</RenderDeviceSpecifier></Request>\n\n\n";
		mRenderDeviceDirty = false;
	}
}

void LLVoiceClient::buildLocalAudioUpdates(std::ostringstream& stream)
{
	buildSetCaptureDevice(stream);

	buildSetRenderDevice(stream);

	if (mPTTDirty)
	{
		mPTTDirty = false;

		// Send a local mute command.
		// NOTE: the state of "PTT" is the inverse of "local mute" (i.e. when
		// PTT is true, we send a mute command with "false", and vice versa).

		LL_DEBUGS("Voice") << "Sending MuteLocalMic command with parameter "
						   << (mPTT ? "false" : "true") << LL_ENDL;

		stream << "<Request requestId=\"" << mCommandCookie++
			   << "\" action=\"Connector.MuteLocalMic.1\"><ConnectorHandle>"
			   << mConnectorHandle << "</ConnectorHandle>" << "<Value>"
			   << (mPTT ? "false" : "true") << "</Value></Request>\n\n\n";
	}

	if (mSpeakerMuteDirty)
	{
		const char* muteval = mSpeakerVolume == 0 ? "true" : "false";

		mSpeakerMuteDirty = false;

		llinfos << "Setting speaker mute to " << muteval << llendl;

		stream << "<Request requestId=\"" << mCommandCookie++
			   << "\" action=\"Connector.MuteLocalSpeaker.1\">"
			   << "<ConnectorHandle>" << mConnectorHandle
			   << "</ConnectorHandle><Value>" << muteval
			   << "</Value></Request>\n\n\n";
	}

	if (mSpeakerVolumeDirty)
	{
		mSpeakerVolumeDirty = false;

		llinfos << "Setting speaker volume to " << mSpeakerVolume << llendl;

		stream << "<Request requestId=\"" << mCommandCookie++
			   << "\" action=\"Connector.SetLocalSpeakerVolume.1\">"
			   << "<ConnectorHandle>" << mConnectorHandle
			   << "</ConnectorHandle><Value>" << mSpeakerVolume
			   << "</Value></Request>\n\n\n";
	}

	if (mMicVolumeDirty)
	{
		mMicVolumeDirty = false;

		llinfos << "Setting mic volume to " << mMicVolume << llendl;

		stream << "<Request requestId=\"" << mCommandCookie++
			   << "\" action=\"Connector.SetLocalMicVolume.1\">"
			   << "<ConnectorHandle>" << mConnectorHandle
			   << "</ConnectorHandle><Value>" << mMicVolume
			   << "</Value></Request>\n\n\n";
	}
}

/////////////////////////////
// Response/Event handlers

void LLVoiceClient::connectorCreateResponse(S32 status_code,
											std::string& status_str,
											std::string& connector_handle,
											std::string& version_id)
{
	if (status_code)
	{
		llwarns << "Connector.Create response failure: " << status_str
				<< llendl;
		setState(stateConnectorFailed);
	}
	else
	{
		// Connector created, move forward.
		llinfos << "Connector.Create succeeded, Vivox SDK version is "
				<< version_id << " - Connector handle: " << connector_handle
				<< llendl;
		mConnectorEstablished = true;
		mConnectorHandle = connector_handle;
		if (mState == stateConnectorStarting)
		{
			setState(stateConnectorStarted);
		}
	}
}

void LLVoiceClient::loginResponse(S32 status_code,
								  std::string& status_str,
								  std::string& account_handle,
								  S32 aliases_number)
{
	LL_DEBUGS("Voice") << "Account.Login response (" << status_code << "): "
					   << status_str << " - Handle: " << account_handle
					   << LL_ENDL;

	// Status code of 20200 means "bad password".  We may want to special-case
	// that at some point.

	if (status_code == HTTP_UNAUTHORIZED)
	{
		// Login failure which is probably caused by the delay after a user's
		// password being updated.
		llinfos << "Account.Login response failure (" << status_code << "): "
				<< status_str << llendl;
		setState(stateLoginRetry);
	}
	else if (status_code)
	{
		llwarns << "Account.Login response failure (" << status_code << "): "
				<< status_str << llendl;
		setState(stateLoginFailed);
	}
	else
	{
		// Login succeeded, move forward.
		mAccountLoggedIn = true;
		mAccountHandle = account_handle;
		mNumberOfAliases = aliases_number;
		llinfos << "Account.Login succeeded. Account handle: "
				<< account_handle << llendl;
#if 0	// This needs to wait until the AccountLoginStateChangeEvent is received.
		if (mState == stateLoggingIn)
		{
			setState(stateLoggedIn);
		}
#endif
	}
}

void LLVoiceClient::sessionCreateResponse(std::string& request_id,
										  S32 status_code,
										  std::string& status_str,
										  std::string& session_handle)
{
	llinfos << "Got Session.Create response for request Id: " << request_id
			<< " - Session handle " << session_handle << llendl;

	sessionState* session = findSessionBeingCreatedByURI(request_id);

	if (session)
	{
		LL_DEBUGS("Voice") << "Found session, marking as creation in progress."
						   << LL_ENDL;
		session->mCreateInProgress = false;
	}

	if (status_code)
	{
		llwarns << "Failure (" << status_code << "): " << status_str << llendl;
		if (session)
		{
			session->mErrorStatusCode = status_code;
			session->mErrorStatusString = status_str;
			if (session == mAudioSession)
			{
				setState(stateJoinSessionFailed);
			}
			else
			{
				reapSession(session);
			}
		}
	}
	else
	{
		llinfos << "Session successfully created." << llendl;
		if (session)
		{
			setSessionHandle(session, session_handle);
		}
	}
}

void LLVoiceClient::sessionGroupAddSessionResponse(std::string& request_id,
												   S32 status_code,
												   std::string& status_str,
												   std::string& session_handle)
{
	sessionState* session = findSessionBeingCreatedByURI(request_id);
	if (session)
	{
		session->mCreateInProgress = false;
	}

	if (status_code)
	{
		llwarns << "SessionGroup.AddSession response failure (" << status_code
				<< "): " << status_str << " - Session handle "
				<< session_handle << llendl;
		if (session)
		{
			session->mErrorStatusCode = status_code;
			session->mErrorStatusString = status_str;
			if (session == mAudioSession)
			{
				setState(stateJoinSessionFailed);
			}
			else
			{
				reapSession(session);
			}
		}
	}
	else
	{
		LL_DEBUGS("Voice") << "SessionGroup.AddSession response received (success), session handle: "
						   << session_handle << LL_ENDL;
		if (session)
		{
			setSessionHandle(session, session_handle);
		}
	}
}

void LLVoiceClient::sessionConnectResponse(std::string& request_id,
										   S32 status_code,
										   std::string& status_str)
{
	sessionState* session = findSession(request_id);
	if (status_code)
	{
		llwarns << "Session.Connect response failure (" << status_code << "): "
				<< status_str << llendl;
		if (session)
		{
			session->mMediaConnectInProgress = false;
			session->mErrorStatusCode = status_code;
			session->mErrorStatusString = status_str;
			if (session == mAudioSession)
			{
				setState(stateJoinSessionFailed);
			}
		}
	}
	else
	{
		LL_DEBUGS("Voice") << "Session.Connect response received (success)"
						   << LL_ENDL;
	}
}

void LLVoiceClient::logoutResponse(S32 status_code, std::string& status_str)
{
	if (status_code)
	{
		llwarns << "Account.Logout response failure: " << status_str
				<< llendl;
		// Should this ever fail ?  Do we care if it does ?
	}
}

void LLVoiceClient::connectorShutdownResponse(S32 status_code,
											  std::string& status_str)
{
	if (status_code)
	{
		llwarns << "Connector.InitiateShutdown response failure: "
				<< status_str << llendl;
		// Should this ever fail ?  Do we care if it does ?
	}

	mConnected = false;

	if (mState == stateConnectorStopping)
	{
		setState(stateConnectorStopped);
	}
}

void LLVoiceClient::sessionAddedEvent(std::string& uri_str,
									  std::string& alias,
									  std::string& session_handle,
                                      std::string& session_grp_handle,
									  bool is_channel, bool incoming,
									  std::string& name_str)
{
	sessionState* session = NULL;

	llinfos << "Session: " << uri_str << " - Alias: " << alias << " - Name: "
			<< name_str << " - Session handle: " << session_handle
			<< " - Group handle: " << session_grp_handle << llendl;

	session = addSession(uri_str, session_handle);
	if (session)
	{
		session->mGroupHandle = session_grp_handle;
		session->mIsChannel = is_channel;
		session->mIncoming = incoming;
		session->mAlias = alias;

		// Generate a caller UUID: we do not need to do this for channels
		if (!session->mIsChannel)
		{
			if (IDFromName(session->mSIPURI, session->mCallerID))
			{
				// Normal URI(base64-encoded UUID)
			}
			else if (!session->mAlias.empty() &&
					 IDFromName(session->mAlias, session->mCallerID))
			{
				// Wrong URI, but an alias is available. Stash the incoming URI
				// as an alternate
				session->mAlternateSIPURI = session->mSIPURI;

				// And generate a proper URI from the ID.
				setSessionURI(session, sipURIFromID(session->mCallerID));
			}
			else
			{
				llinfos << "Could not generate caller id from uri, using hash of URI "
						<< session->mSIPURI << llendl;
				session->mCallerID.generate(session->mSIPURI);
				session->mSynthesizedCallerID = true;

				// Cannot look up the name in this case: we have to extract it
				// from the URI.
				std::string name_portion = nameFromsipURI(session->mSIPURI);
				if (name_portion.empty())
				{
					// Did not seem to be a SIP URI, just use the whole
					// provided name.
					name_portion = name_str;
				}

				// Some incoming names may be separated with an underscore
				// instead of a space. Fix this.
				LLStringUtil::replaceChar(name_portion, '_', ' ');

				// Act like we just finished resolving the name (this stores it
				// in all the right places)
				avatarNameResolved(session->mCallerID, name_portion);
			}

			llinfos << "Caller Id: " << session->mCallerID << llendl;

			if (!session->mSynthesizedCallerID)
			{
				// If we got here, we do not have a proper name. Initiate a
				// lookup.
				lookupName(session->mCallerID);
			}
		}
	}
}

void LLVoiceClient::joinedAudioSession(sessionState* session)
{
	if (mAudioSession != session)
	{
		sessionState* oldSession = mAudioSession;

		mAudioSession = session;

		// The old session may now need to be deleted.
		reapSession(oldSession);
	}

	// This is the session we're joining.
	if (mState == stateJoiningSession)
	{
		setState(stateSessionJoined);

		// SLIM SDK: we do not always receive a participant state change for
		// ourselves when joining a channel now.
		// Add the current user as a participant here.
		participantState* participant =
			session->addParticipant(sipURIFromName(mAccountName));
		if (participant)
		{
			participant->mIsSelf = true;
			lookupName(participant->mAvatarID);

			llinfos << "Added self as participant \""
					<< participant->mAccountName << "\" ("
					<< participant->mAvatarID << ")" << llendl;
		}

		if (!session->mIsChannel)
		{
			// This is a P2P session. Make sure the other end is added as a
			// participant.
			participantState* participant =
				session->addParticipant(session->mSIPURI);
			if (participant)
			{
				if (participant->mAvatarIDValid)
				{
					lookupName(participant->mAvatarID);
				}
				else if (!session->mName.empty())
				{
					participant->mLegacyName = session->mName;
					avatarNameResolved(participant->mAvatarID, session->mName);
				}

				// *TODO: do we need to set up mAvatarID/mAvatarIDValid here ?
				llinfos << "Added caller as participant \""
						<< participant->mAccountName << "\" ("
						<< participant->mAvatarID << ")" << llendl;
			}
		}
	}
}

void LLVoiceClient::sessionRemovedEvent(std::string& session_handle,
										std::string& session_grp_handle)
{
	sessionState* session = findSession(session_handle);
	if (!session)
	{
		llwarns << "Unknown session " << session_handle << " removed"
				<< llendl;
		return;
	}

	leftAudioSession(session);

	// This message invalidates the session's handle. Set it to empty.
	setSessionHandle(session);

	// This also means that the session's session group is now empty.
	// Terminate the session group so it does not leak.
	sessionGroupTerminateSendMessage(session);

	// Conditionally delete the session
	reapSession(session);

	llinfos << "Removed session. Session handle: " << session_handle
			<< " - Group handle: " << session_grp_handle << llendl;
}

void LLVoiceClient::reapSession(sessionState* session)
{
	if (!session)
	{
		return;
	}

	if (!session->mHandle.empty())
	{
		LL_DEBUGS("Voice") << "NOT deleting session " << session->mSIPURI
						   << " (non-null session handle)" << LL_ENDL;
	}
	else if (session->mCreateInProgress)
	{
		LL_DEBUGS("Voice") << "NOT deleting session " << session->mSIPURI
						   << " (create in progress)" << LL_ENDL;
	}
	else if (session->mMediaConnectInProgress)
	{
		LL_DEBUGS("Voice") << "NOT deleting session " << session->mSIPURI
						   << " (connect in progress)" << LL_ENDL;
	}
	else if (session == mAudioSession)
	{
		LL_DEBUGS("Voice") << "NOT deleting session " << session->mSIPURI
						   << " (it is the current session)" << LL_ENDL;
	}
	else if (session == mNextAudioSession)
	{
		LL_DEBUGS("Voice") << "NOT deleting session " << session->mSIPURI
						   << " (it is the next session)" << LL_ENDL;
	}
	else
	{
		// *TODO: should we check for queued text messages here ?
		// We do not have a reason to keep tracking this session, so just
		// delete it.
		LL_DEBUGS("Voice") << "Deleting session " << session->mSIPURI
						   << LL_ENDL;
		deleteSession(session);
		session = NULL;
	}
}

// Returns true if the session seems to indicate we've moved to a region on a
// different voice server
bool LLVoiceClient::sessionNeedsRelog(sessionState* session)
{
	// Only make this check for spatial channels (so it would not happen for
	// group or P2P calls)
	if (session && session->mIsSpatial)
	{
		size_t i = session->mSIPURI.find("@");
		if (i != std::string::npos)
		{
			std::string urihost = session->mSIPURI.substr(i + 1);
			if (stricmp(urihost.c_str(), mVoiceSIPURIHostName.c_str()))
			{
				// The hostname in this URI is different from what we expect.
				// This probably means we need to relog.

				// We could make a ProvisionVoiceAccountRequest and compare the
				// result with the current values of mVoiceSIPURIHostName and
				// mVoiceAccountServerURI to be really sure, but this is a
				// pretty good indicator.
				return true;
			}
		}
	}

	return false;
}

void LLVoiceClient::leftAudioSession(sessionState* session)
{
	if (mAudioSession == session)
	{
		switch (mState)
		{
			case stateJoiningSession:
			case stateSessionJoined:
			case stateRunning:
			case stateLeavingSession:
			case stateJoinSessionFailed:
			case stateJoinSessionFailedWaiting:
				// Normal transition
				LL_DEBUGS("Voice") << "Left session " << session->mHandle
								   << " in state " << state2string(mState)
								   << LL_ENDL;
				setState(stateSessionTerminated);
				break;

			case stateSessionTerminated:
				// This will happen sometimes -- there are cases where we send
				// the terminate and then go straight to this state.
				llwarns << "Left session " << session->mHandle << " in state "
						<< state2string(mState) << llendl;
				break;

			default:
				llwarns << "Unexpected SessionStateChangeEvent (left session) in state "
						<< state2string(mState) << llendl;
				setState(stateSessionTerminated);
		}
	}
}

void LLVoiceClient::accountLoginStateChangeEvent(std::string& account_handle,
												 S32 status_code,
												 std::string& status_str,
												 S32 state)
{
	LL_DEBUGS("Voice") << "State is " << state  << " - Handle: "
					   << account_handle << LL_ENDL;
	/*
		According to Mike S., status codes for this event are:
		login_state_logged_out=0,
        login_state_logged_in = 1,
        login_state_logging_in = 2,
        login_state_logging_out = 3,
        login_state_resetting = 4,
        login_state_error=100
	*/

	switch (state)
	{
		case 0:
			// The user has been logged out.
			setState(stateLoggedOut);
			break;

		case 1:
			if (mState == stateLoggingIn)
			{
				setState(stateLoggedIn);
			}
			break;

		case 3:
			// The user is in the process of logging out.
			setState(stateLoggingOut);
			break;

		default:
			// Used to be a commented out warning
			LL_DEBUGS("Voice") << "Unknown state: " << state << LL_ENDL;
	}
}

void LLVoiceClient::mediaStreamUpdatedEvent(std::string& session_handle,
											std::string& session_grp_handle,
											S32 status_code,
											std::string& status_str,
											S32 state, bool incoming)
{
	sessionState* session = findSession(session_handle);

	LL_DEBUGS("Voice") << "session " << session_handle << ", status code "
					   << status_code << ", string \"" << status_str << "\""
					   << LL_ENDL;

	if (session)	// If we know about this session
	{
		switch (status_code)
		{
			case 0:
			case HTTP_OK:
				// Generic success: do not change the saved error code (it may
				// have been set elsewhere).
				break;

			default:
				// Save the status code for later
				session->mErrorStatusCode = status_code;
		}

		switch (state)
		{
			case streamStateIdle:
			case streamStateDisconnecting:
				// Standard "left audio session"
				session->mVoiceEnabled = false;
				session->mMediaConnectInProgress = false;
				leftAudioSession(session);
				break;

			case streamStateConnecting:	// Nothing to do
				break;

			case streamStateConnected:
				session->mVoiceEnabled = true;
				session->mMediaConnectInProgress = false;
				joinedAudioSession(session);
				break;

			case streamStateRinging:
				if (incoming)
				{
					// Send the voice chat invite to the GUI layer
					// *TODO: should we correlate with the mute list here ?
					session->mIMSessionID =
						LLIMMgr::computeSessionID(IM_SESSION_P2P_INVITE,
												  session->mCallerID);
					session->mVoiceInvitePending = true;
					if (session->mName.empty())
					{
						lookupName(session->mCallerID);
					}
					else
					{
						// Act like we just finished resolving the name
						avatarNameResolved(session->mCallerID, session->mName);
					}
				}
				break;

			default:
				llwarns << "Unknown state " << state << llendl;
		}
	}
	else
	{
		llwarns << "Session " << session_handle << " not found !" << llendl;
	}
}

void LLVoiceClient::participantAddedEvent(std::string& session_handle,
										  std::string& session_grp_handle,
										  std::string& uri_str,
										  std::string& alias,
										  std::string& name_str,
										  std::string& display_name_str,
										  S32 participant_type)
{
	sessionState* session = findSession(session_handle);
	if (session)
	{
		participantState* participant = session->addParticipant(uri_str);
		if (participant)
		{
			participant->mAccountName = name_str;

			LL_DEBUGS("Voice") << "Added participant \""
							   << participant->mAccountName << "\" ("
							   << participant->mAvatarID << ")" << LL_ENDL;

			if (participant->mAvatarIDValid)
			{
				// Initiate a lookup
				lookupName(participant->mAvatarID);
			}
			else
			{
				// If we do not have a valid avatar UUID, we need to fill in
				// the display name to make the active speakers floater work.
				std::string name_portion = nameFromsipURI(uri_str);
				if (name_portion.empty())
				{
					// Problem with the SIP URI, fall back to the display name
					name_portion = display_name_str;
				}
				if (name_portion.empty())
				{
					// Problems with both of the above, fall back to the
					// account name
					name_portion = name_str;
				}

				// Set the display name (which is a hint to the active speakers
				// window not to do its own lookup)
				participant->mLegacyName = name_portion;
				avatarNameResolved(participant->mAvatarID, name_portion);
			}
		}
	}
}

void LLVoiceClient::participantRemovedEvent(std::string& session_handle,
											std::string& session_grp_handle,
											std::string& uri_str,
											std::string& alias,
											std::string& name_str)
{
	sessionState* session = findSession(session_handle);
	if (session)
	{
		participantState* participant = session->findParticipant(uri_str);
		if (participant)
		{
			session->removeParticipant(participant);
		}
		else
		{
			LL_DEBUGS("Voice") << "Unknown participant " << uri_str << LL_ENDL;
		}
	}
	else
	{
		LL_DEBUGS("Voice") << "Unknown session " << session_handle << LL_ENDL;
	}
}

void LLVoiceClient::participantUpdatedEvent(std::string& session_handle,
											std::string& session_grp_handle,
											std::string& uri_str,
											std::string& alias,
											bool muted_by_moderator,
											bool speaking, S32 volume,
											F32 energy)
{
	sessionState* session = findSession(session_handle);
	if (session)
	{
		participantState* participant = session->findParticipant(uri_str);

		if (participant)
		{
			participant->mIsSpeaking = speaking;
			participant->mIsModeratorMuted = muted_by_moderator;

			// SLIM SDK: convert range: ensure that energy is set to zero if
			// is_speaking is false
			if (speaking)
			{
				participant->mSpeakingTimeout.reset();
				participant->mPower = energy;
			}
			else
			{
				participant->mPower = 0.f;
			}
			participant->mVolume = volume;
		}
		else
		{
			llwarns << "Unknown participant: " << uri_str << llendl;
		}
	}
	else
	{
		llinfos << "Unknown session " << session_handle << llendl;
	}
}

void LLVoiceClient::messageEvent(std::string& session_handle,
								 std::string& uri_str, std::string& alias,
								 std::string& msg_header,
								 std::string& msg_body)
{
	LL_DEBUGS("Voice") << "Message event, session " << session_handle
					   << " from " << uri_str << LL_ENDL;

	size_t start, end;

	if (msg_header.find(HTTP_CONTENT_TEXT_HTML) != std::string::npos)
	{
		static const std::string STARTMARKER = "<body";
		static const std::string STARTMARKER2 = ">";
		static const std::string ENDMARKER = "</body>";
		static const std::string STARTSPAN = "<span";
		static const std::string ENDSPAN = "</span>";

		// Default to displaying the raw string, so the message gets through.
		std::string raw_msg = msg_body;

		// Find the actual message text within the XML fragment
		start = msg_body.find(STARTMARKER);
		start = msg_body.find(STARTMARKER2, start);
		end = msg_body.find(ENDMARKER);

		if (start != std::string::npos)
		{
			start += STARTMARKER2.size();
			if (end != std::string::npos)
			{
				end -= start;
			}
			raw_msg.assign(msg_body, start, end);
		}
		else
		{
			// Did not find a <body>, try looking for a <span> instead.
			start = msg_body.find(STARTSPAN);
			start = msg_body.find(STARTMARKER2, start);
			end = msg_body.find(ENDSPAN);

			if (start != std::string::npos)
			{
				start += STARTMARKER2.size();
				if (end != std::string::npos)
				{
					end -= start;
				}
				raw_msg.assign(msg_body, start, end);
			}
		}

		// Strip formatting tags
		while ((start = raw_msg.find('<')) != std::string::npos)
		{
			if ((end = raw_msg.find('>', start + 1)) != std::string::npos)
			{
				// Strip out the tag
				raw_msg.erase(start, (end + 1) - start);
			}
			else
			{
				// Avoid an infinite loop
				break;
			}
		}

		// Decode ampersand-escaped chars
		// The text may contain text encoded with &lt;, &gt;, and &amp;
		size_t mark = 0;
		while ((mark = raw_msg.find("&lt;", mark)) != std::string::npos)
		{
			raw_msg.replace(mark++, 4, "<");
		}

		mark = 0;
		while ((mark = raw_msg.find("&gt;", mark)) != std::string::npos)
		{
			raw_msg.replace(mark++, 4, ">");
		}

		mark = 0;
		while ((mark = raw_msg.find("&amp;", mark)) != std::string::npos)
		{
			raw_msg.replace(mark++, 5, "&");
		}

		// Strip leading/trailing whitespace (since we always seem to get a
		// couple newlines)
		LLStringUtil::trim(raw_msg);

		sessionState* session = findSession(session_handle);
		if (session)
		{
			bool is_busy = gAgent.getBusy();
			bool is_muted = LLMuteList::isMuted(session->mCallerID,
												session->mName,
												LLMute::flagTextChat,
												LLMute::AGENT);
			bool is_linden = LLMuteList::isLinden(session->mName);
			bool quiet_chat = false;
			LLChat chat;

			chat.mMuted = is_muted && !is_linden;

			if (!chat.mMuted)
			{
				chat.mFromID = session->mCallerID;
				chat.mFromName = session->mName;
				chat.mSourceType = CHAT_SOURCE_AGENT;

				if (is_busy && !is_linden)
				{
					quiet_chat = true;
					// *TODO: Return busy mode response here ?  Or maybe when
					// session is started instead ?
				}

				LL_DEBUGS("Voice") << "Adding message, name " << session->mName
								   << ", session " << session->mIMSessionID
								   << ", target " << session->mCallerID
								   << LL_ENDL;

				std::string full_msg = ": " + raw_msg;

				if (gIMMgrp)
				{
					gIMMgrp->addMessage(session->mIMSessionID,
										session->mCallerID,
										session->mName.c_str(),
										full_msg.c_str(),
										LLStringUtil::null, IM_NOTHING_SPECIAL,
										0, LLUUID::null, LLVector3::zero,
										// Prepend name and make it a link to
										// the user's profile
										true);
				}

				chat.mText = "IM: " + session->mName + full_msg;
				// If the chat should come in quietly (i.e. we are in busy
				// mode), pretend it is from a local agent.
				LLFloaterChat::addChat(chat, true, quiet_chat);
			}
		}
	}
}

void LLVoiceClient::sessionNotificationEvent(std::string& session_handle,
											 std::string& uri_str,
											 std::string& notif_type)
{
	sessionState* session = findSession(session_handle);
	if (!session)
	{
		LL_DEBUGS("Voice") << "Unknown session handle " << session_handle
						   << LL_ENDL;
		return;
	}

	participantState* participant = session->findParticipant(uri_str);
	if (participant)
	{
		if (!stricmp(notif_type.c_str(), "Typing"))
		{
			LL_DEBUGS("Voice") << "Participant " << uri_str << " in session "
							   << session->mSIPURI << " starts typing."
							   << LL_ENDL;
		}
		else if (!stricmp(notif_type.c_str(), "NotTyping"))
		{
			LL_DEBUGS("Voice") << "Participant " << uri_str << " in session "
							   << session->mSIPURI << " stops typing."
							   << LL_ENDL;
		}
		else
		{
			LL_DEBUGS("Voice") << "Unknown notification type "
							   << notif_type << "for participant "
							   << uri_str << " in session "
							   << session->mSIPURI << LL_ENDL;
		}
	}
	else
	{
		LL_DEBUGS("Voice") << "Unknown participant " << uri_str
						   << " in session " << session->mSIPURI
						   << LL_ENDL;
	}
}

// The user's mute list has been updated. This method goes through the current
// participants list and syncs it with the mute list.
void LLVoiceClient::muteListChanged()
{
	if (!mAudioSession)
	{
		return;
	}

	for (particip_map_t::iterator
			it = mAudioSession->mParticipantsByURI.begin(),
			end = mAudioSession->mParticipantsByURI.end();
		 it != end; ++it)
	{
		participantState* p = it->second;
		if (p && p->updateMuteState())
		{
			mAudioSession->mVolumeDirty = true;
		}
	}
}

/////////////////////////////
// Managing list of participants

LLVoiceClient::participantState::participantState(const std::string& uri)
:	mURI(uri),
	mPTT(false),
	mIsSpeaking(false),
	mIsModeratorMuted(false),
	mLastSpokeTimestamp(0.f),
	mPower(0.f),
	mVolume(-1),
	mOnMuteList(false),
	mUserVolume(-1),
	mVolumeDirty(false),
	mAvatarIDValid(false),
	mIsSelf(false)
{
}

LLVoiceClient::participantState* LLVoiceClient::sessionState::addParticipant(const std::string& uri)
{
	participantState* result = NULL;
	bool useAlternateURI = false;

	// Note: this is mostly the body of
	// LLVoiceClient::sessionState::findParticipant(), but since we need to
	// know if it matched the alternate SIP URI (so we can add it properly), we
	// need to reproduce it here.
	{
		particip_map_t::iterator iter = mParticipantsByURI.find(&uri);

		if (iter == mParticipantsByURI.end())
		{
			if (!mAlternateSIPURI.empty() && uri == mAlternateSIPURI)
			{
				// This is a P2P session (probably with the SLIM client) with
				// an alternate URI for the other participant.
				// Use mSIPURI instead, since it will be properly encoded.
				iter = mParticipantsByURI.find(&(mSIPURI));
				useAlternateURI = true;
			}
		}

		if (iter != mParticipantsByURI.end())
		{
			result = iter->second;
		}
	}

	if (!result)
	{
		// Participant is not already in one list or the other.
		result = new participantState(useAlternateURI?mSIPURI:uri);
		mParticipantsByURI.emplace(&(result->mURI), result);
		// Try to do a reverse transform on the URI to get the GUID back.
		LLUUID id;
		if (IDFromName(result->mURI, id))
		{
			result->mAvatarIDValid = true;
			result->mAvatarID = id;

			if (result->updateMuteState())
			{
				mVolumeDirty = true;
			}
		}
		else
		{
			// Create a UUID by hashing the URI, but do NOT set mAvatarIDValid.
			// This tells both code in LLVoiceClient and code in
			// llfloateractivespeakers.cpp that the ID will not be in the name
			// cache.
			result->mAvatarID.generate(uri);
		}

		mParticipantsByUUID.emplace(&(result->mAvatarID), result);
		LL_DEBUGS("Voice") << "Participant \"" << result->mURI << "\" added."
						   << LL_ENDL;
	}

	return result;
}

bool LLVoiceClient::participantState::updateMuteState()
{
	bool result = false;
	if (mAvatarIDValid)
	{
		bool muted = LLMuteList::isMuted(mAvatarID, LLMute::flagVoiceChat);
		if (mOnMuteList != muted)
		{
			mOnMuteList = muted;
			mVolumeDirty = true;
			result = true;
		}
	}
	return result;
}

void LLVoiceClient::sessionState::removeParticipant(LLVoiceClient::participantState* participant)
{
	if (participant)
	{
		particip_map_t::iterator iter =
			mParticipantsByURI.find(&(participant->mURI));
		particip_id_map_t::iterator iter2 =
			mParticipantsByUUID.find(&(participant->mAvatarID));

		LL_DEBUGS("Voice") << "Participant \"" << participant->mURI
						   <<  "\" (" << participant->mAvatarID
						   << ") removed." << LL_ENDL;

		if (iter == mParticipantsByURI.end())
		{
			llwarns << "Internal error: participant " << participant->mURI
					<< " not in URI map" << llendl;
			gVoiceClient.giveUp();
		}
		else if (iter2 == mParticipantsByUUID.end())
		{
			llwarns << "Internal error: participant ID "
					<< participant->mAvatarID << " not in UUID map" << llendl;
			gVoiceClient.giveUp();
		}
		else if (iter->second != iter2->second)
		{
			llwarns << "Internal error: participant mismatch !" << llendl;
			gVoiceClient.giveUp();
		}
		else
		{
			mParticipantsByURI.erase(iter);
			mParticipantsByUUID.erase(iter2);

			delete participant;
		}
	}
}

void LLVoiceClient::sessionState::removeAllParticipants()
{
	while (!mParticipantsByURI.empty())
	{
		removeParticipant(mParticipantsByURI.begin()->second);
	}

	if (!mParticipantsByUUID.empty())
	{
		llwarns << "Internal error: empty URI map, non-empty UUID map"
				<< llendl;
		gVoiceClient.giveUp();
	}
}

LLVoiceClient::particip_map_t* LLVoiceClient::getParticipantList()
{
	particip_map_t* result = NULL;
	if (mAudioSession)
	{
		result = &(mAudioSession->mParticipantsByURI);
	}
	return result;
}

LLVoiceClient::participantState* LLVoiceClient::sessionState::findParticipant(const std::string& uri)
{
	participantState* result = NULL;

	particip_map_t::iterator iter = mParticipantsByURI.find(&uri);

	if (iter == mParticipantsByURI.end())
	{
		if (!mAlternateSIPURI.empty() && uri == mAlternateSIPURI)
		{
			// This is a P2P session (probably with the SLIM client) with an
			// alternate URI for the other participant.
			// Look up the other URI
			iter = mParticipantsByURI.find(&(mSIPURI));
		}
	}

	if (iter != mParticipantsByURI.end())
	{
		result = iter->second;
	}

	return result;
}

LLVoiceClient::participantState* LLVoiceClient::sessionState::findParticipantByID(const LLUUID& id)
{
	participantState* result = NULL;
	particip_id_map_t::iterator iter = mParticipantsByUUID.find(&id);

	if (iter != mParticipantsByUUID.end())
	{
		result = iter->second;
	}

	return result;
}

LLVoiceClient::participantState* LLVoiceClient::findParticipantByID(const LLUUID& id)
{
	participantState* result = NULL;

	if (mAudioSession)
	{
		result = mAudioSession->findParticipantByID(id);
	}

	return result;
}

void LLVoiceClient::parcelChanged()
{
	if (mState < stateNoChannel)
	{
		// The transition to stateNoChannel needs to kick this off again.
		llinfos << "Not logged in yet, deferring..." << llendl;
		return;
	}

	// If the user is logged in, start a channel lookup.
	LL_DEBUGS("Voice") << "Sending ParcelVoiceInfoRequest ("
					   << mCurrentRegionName << ", "
					   << mCurrentParcelLocalID << ")" << LL_ENDL;

	const std::string& url = gAgent.getRegionCapability("ParcelVoiceInfoRequest");
	if (url.empty())
	{
		LL_DEBUGS("Voice") << "No ParcelVoiceInfoRequest capability for region "
						   << mCurrentRegionName << LL_ENDL;
		return;
	}

	gCoros.launch("LLVivoxVoiceClient::parcelVoiceInfoRequestCoro",
				  boost::bind(&LLVoiceClient::parcelVoiceInfoRequestCoro,
							  url));
}

//static
void LLVoiceClient::parcelVoiceInfoRequestCoro(const std::string& url)
{
	LLCoreHttpUtil::HttpCoroutineAdapter adapter("parcelVoiceInfoRequest");
	LLSD result = adapter.postAndSuspend(url, LLSD());

	if (!sInitDone) return;	// Voice has since been shut down

	LL_DEBUGS("Voice") << "Received voice info reply..." << LL_ENDL;
	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	if (!status)
	{
		llwarns << "No voice on parcel: " << status.toString() << llendl;
		gVoiceClient.sessionTerminate();
		return;
	}

	std::string uri;
	std::string credentials;

	result.erase(LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS);
	if (result.has("voice_credentials"))
	{
		const LLSD& voice_credentials = result["voice_credentials"];
		if (voice_credentials.has("channel_uri"))
		{
			uri = voice_credentials["channel_uri"].asString();
		}
		else
		{
			LL_DEBUGS("Voice") << "No voice channel URI" << LL_ENDL;
		}
		if (voice_credentials.has("channel_credentials"))
		{
			credentials = voice_credentials["channel_credentials"].asString();
		}
		else
		{
			LL_DEBUGS("Voice") << "No voice channel credentials" << LL_ENDL;
		}
	}
	else
	{
		LL_DEBUGS("Voice") << "No voice credentials" << LL_ENDL;
	}

	gVoiceClient.setSpatialChannel(uri, credentials);
}

void LLVoiceClient::switchChannel(std::string uri, bool spatial,
								  bool no_reconnect, bool is_p2p,
								  std::string hash)
{
	bool needs_switch = false;

	LL_DEBUGS("Voice") << "Called in state " << state2string(mState)
					   << " with uri \"" << uri << "\", spatial is "
					   << (spatial ? "true" : "false")
					   << LL_ENDL;

	switch (mState)
	{
		case stateJoinSessionFailed:
		case stateJoinSessionFailedWaiting:
		case stateNoChannel:
		{
			// Always switch to the new URI from these states.
			needs_switch = true;
			break;
		}

		default:
		{
			if (mSessionTerminateRequested)
			{
				// If a terminate has been requested, we need to compare
				// against where the URI we're already headed to.
				if (mNextAudioSession)
				{
					if (mNextAudioSession->mSIPURI != uri)
					{
						needs_switch = true;
					}
				}
				else
				{
					// mNextAudioSession is null -- this probably means we are
					// on our way back to spatial.
					if (!uri.empty())
					{
						// We do want to process a switch in this case.
						needs_switch = true;
					}
				}
			}
			// Otherwise, compare against the URI we are in now.
			else if (mAudioSession)
			{
				if (mAudioSession->mSIPURI != uri)
				{
					needs_switch = true;
				}
			}
			else if (!uri.empty())
			{
				// mAudioSession is null; it is not clear what case would cause
				// this. Log it as a warning and see if it ever crops up.
				llwarns << "No current audio session." << llendl;
			}
			break;
		}
	}

	if (!needs_switch)
	{
		return;
	}

	if (uri.empty())
	{
		// Leave any channel we may be in
		LL_DEBUGS("Voice") << "Leaving channel" << LL_ENDL;

		sessionState* oldSession = mNextAudioSession;
		mNextAudioSession = NULL;

		// The old session may now need to be deleted.
		reapSession(oldSession);

		// Make sure voice is turned off
		mUserPTTState = false;

		notifyStatusObservers(LLVoiceClientStatusObserver::STATUS_VOICE_DISABLED);
	}
	else
	{
		LL_DEBUGS("Voice") << "Switching to channel " << uri << LL_ENDL;
		mNextAudioSession = addSession(uri);
		mNextAudioSession->mHash = hash;
		mNextAudioSession->mIsSpatial = spatial;
		mNextAudioSession->mReconnect = !no_reconnect;
		mNextAudioSession->mIsP2P = is_p2p;
	}

	if (mState > stateNoChannel)
	{
		// State machine will come around and rejoin if URI/handle is not empty
		sessionTerminate();
	}
}

void LLVoiceClient::joinSession(sessionState* session)
{
	mNextAudioSession = session;

	if (mState > stateNoChannel)
	{
		// State machine will come around and rejoin if URI/handle is not empty
		sessionTerminate();
	}
}

void LLVoiceClient::setNonSpatialChannel(const std::string& uri,
										 const std::string& credentials)
{
	switchChannel(uri, false, false, false, credentials);
}

void LLVoiceClient::setSpatialChannel(const std::string& uri,
									  const std::string& credentials)
{
	mSpatialSessionURI = uri;
	mSpatialSessionCredentials = credentials;
#if 0	// Not used
	mAreaVoiceDisabled = mSpatialSessionURI.empty();
#endif

	LL_DEBUGS("Voice") << "Got spatial channel uri: \"" << uri << "\""
					   << LL_ENDL;

	if ((mAudioSession && !mAudioSession->mIsSpatial) ||
		(mNextAudioSession && !mNextAudioSession->mIsSpatial))
	{
		// User is in a non-spatial chat or joining a non-spatial chat. Do not
		// switch channels.
		llinfos << "In non-spatial chat, not switching channels" << llendl;
	}
	else
	{
		switchChannel(mSpatialSessionURI, true, false, false,
					  mSpatialSessionCredentials);
	}
}

void LLVoiceClient::callUser(const LLUUID& uuid)
{
	std::string userURI = sipURIFromID(uuid);

	switchChannel(userURI, false, true, true);
}

#if 0	// Vivox text IMs are not in use.
LLVoiceClient::sessionState* LLVoiceClient::startUserIMSession(const LLUUID& uuid)
{
	// Figure out if a session with the user already exists
	sessionState* session = findSession(uuid);
	if (!session)
	{
		// No session with user, need to start one.
		std::string uri = sipURIFromID(uuid);
		session = addSession(uri);
		session->mIsSpatial = false;
		session->mReconnect = false;
		session->mIsP2P = true;
		session->mCallerID = uuid;
	}

	if (session)
	{
		if (session->mHandle.empty())
		{
			// Session is not active: start it up.
			sessionCreateSendMessage(session, false, true);
		}
		else
		{
			// Session is already active: start up text.
			sessionTextConnectSendMessage(session);
		}
	}

	return session;
}

void LLVoiceClient::endUserIMSession(const LLUUID& uuid)
{
	// Figure out if a session with the user exists
	sessionState* session = findSession(uuid);
	if (session)
	{
		// Found the session
		if (!session->mHandle.empty())
		{
			sessionTextDisconnectSendMessage(session);
		}
	}
	else
	{
		LL_DEBUGS("Voice") << "Session not found for participant ID " << uuid
						   << LL_ENDL;
	}
}

void LLVoiceClient::sessionTextDisconnectSendMessage(sessionState* session)
{
	LL_DEBUGS("Voice") << "Sending Session.TextDisconnect with handle "
					   << session->mHandle << LL_ENDL;

	std::ostringstream stream;
	stream << "<Request requestId=\"" << mCommandCookie++
		   << "\" action=\"Session.TextDisconnect.1\"><SessionGroupHandle>"
		   << session->mGroupHandle << "</SessionGroupHandle><SessionHandle>"
		   << session->mHandle << "</SessionHandle></Request>\n\n\n";
	writeString(stream.str());
}
#endif

// This is only ever used to answer incoming P2P call invites.
bool LLVoiceClient::answerInvite(std::string& session_handle)
{
	sessionState* session = findSession(session_handle);
	if (session)
	{
		session->mIsSpatial = false;
		session->mReconnect = false;
		session->mIsP2P = true;

		joinSession(session);
		return true;
	}

	return false;
}

// Returns true if the indicated participant in the current audio session is
// really an SL avatar. Currently this will be false only for PSTN callers
// into group chats, and PSTN P2P calls.
bool LLVoiceClient::isParticipantAvatar(const LLUUID& id)
{
	sessionState* session = findSession(id);
	if (session)
	{
		// This is a P2P session with the indicated caller, or the session with
		// the specified UUID.
		if (session->mSynthesizedCallerID)
		{
			return false;
		}
	}
	// Did not find a matching session: check the current audio session for a
	// matching participant.
	else if (mAudioSession)
	{
		participantState* participant = findParticipantByID(id);
		if (participant)
		{
			return participant->isAvatar();
		}
	}

	return true;
}

// Returns true if calling back the session URI after the session has closed is
// possible.
// Currently this will be false only for PSTN P2P calls.
bool LLVoiceClient::isSessionCallBackPossible(const LLUUID& session_id)
{
	sessionState* session = findSession(session_id);
	return !session || session->isCallBackPossible();
}

// Returns true if the session can accept text IM's.
// Currently this will be false only for PSTN P2P calls.
bool LLVoiceClient::isSessionTextIMPossible(const LLUUID& session_id)
{
	sessionState* session = findSession(session_id);
	return !session || session->isTextIMPossible();
}

void LLVoiceClient::declineInvite(std::string& session_handle)
{
	sessionState* session = findSession(session_handle);
	if (session)
	{
		sessionMediaDisconnectSendMessage(session);
	}
}

void LLVoiceClient::leaveNonSpatialChannel()
{
	LL_DEBUGS("Voice") << "Called in state " << state2string(mState)
					   << LL_ENDL;

	// Make sure we do not rejoin the current session.
	sessionState* old_next_session = mNextAudioSession;
	mNextAudioSession = NULL;

	// Most likely this will still be the current session at this point, but
	// check it anyway.
	reapSession(old_next_session);

	verifySessionState();

	sessionTerminate();
}

std::string LLVoiceClient::getCurrentChannel()
{
	if (!mSessionTerminateRequested && mState == stateRunning)
	{
		return getAudioSessionURI();
	}
	return "";
}

bool LLVoiceClient::inProximalChannel()
{
	return !mSessionTerminateRequested && mState == stateRunning &&
		   inSpatialChannel();
}

std::string LLVoiceClient::sipURIFromID(const LLUUID& id)
{
	return "sip:" + nameFromID(id) + "@" + mVoiceSIPURIHostName;
}

std::string LLVoiceClient::sipURIFromAvatar(LLVOAvatar* avatar)
{
	if (!avatar)
	{
		return "";
	}
	return "sip:" + nameFromID(avatar->getID()) + "@" + mVoiceSIPURIHostName;
}

std::string LLVoiceClient::nameFromAvatar(LLVOAvatar* avatar)
{
	return avatar ? nameFromID(avatar->getID()) : "";
}

// If you need to transform a GUID to this form on the Mac OS X command line,
// this will do so:
// echo -n x && (echo e669132a-6c43-4ee1-a78d-6c82fff59f32 | xxd -r -p | openssl base64 | tr '/+' '_-')
//
// The reverse transform can be done with:
// echo 'x5mkTKmxDTuGnjWyC__WfMg==' | cut -b 2- - | tr '_-' '/+' | openssl base64 -d | xxd -p
std::string LLVoiceClient::nameFromID(const LLUUID& uuid)
{
	if (uuid.isNull())
	{
		return "";
	}

	// Prepending this apparently prevents conflicts with reserved names inside
	// the vivox and diamondware code.
	std::string result = "x";

	// Base64 encode and replace the pieces of base64 that are less compatible
	// with e-mail local-parts.
	// See RFC-4648 "Base 64 Encoding with URL and Filename Safe Alphabet"
	result += LLBase64::encode((const char*)uuid.mData, UUID_BYTES);
	LLStringUtil::replaceChar(result, '+', '-');
	LLStringUtil::replaceChar(result, '/', '_');

	return result;
}

bool LLVoiceClient::IDFromName(const std::string in_name, LLUUID& uuid)
{
	bool result = false;

	// SLIM SDK: The "name" may actually be a SIP URI such as:
	// "sip:xFnPP04IpREWNkuw1cOXlhw==@bhr.vivox.com"
	// If it is, convert to a bare name before doing the transform.
	std::string name = nameFromsipURI(in_name);

	// Does not look like a SIP URI, assume it is an actual name.
	if (name.empty())
	{
		name = in_name;
	}

	// This will only work if the name is of the proper form.
	// As an example, the account name for Monroe Linden
	// (UUID 1673cfd3-8229-4445-8d92-ec3570e5e587) is:
	// "xFnPP04IpREWNkuw1cOXlhw=="

	if (name.size() == 25 && name[0] == 'x' && name[23] == '=' &&
		name[24] == '=')
	{
		// The name appears to have the right form.

		// Reverse the transforms done by nameFromID
		std::string temp = name;
		LLStringUtil::replaceChar(temp, '-', '+');
		LLStringUtil::replaceChar(temp, '_', '/');

		std::string buffer = LLBase64::decode(temp.c_str() + 1);
		if (buffer.size() == (size_t)UUID_BYTES)
		{
			// The decode succeeded. Stuff the bits into the UUID
			memcpy(uuid.mData, buffer.c_str(), UUID_BYTES);
			result = true;
			LL_DEBUGS("Voice") << "Decoded UUID: " << uuid << LL_ENDL;
		}
		else
		{
			llwarns << "Invalid UUID encoding" << llendl;
		}
	}

	if (!result)
	{
		// VIVOX: not a standard account name, just copy the URI name
		// mURIString field and hope for the best. bpj
		uuid.setNull();  // VIVOX, set the uuid field to nulls
	}

	return result;
}

std::string LLVoiceClient::displayNameFromAvatar(LLVOAvatar* avatar)
{
	return avatar ? avatar->getFullname() : "";
}

std::string LLVoiceClient::sipURIFromName(std::string& name)
{
	return "sip:" + name + "@" + mVoiceSIPURIHostName;
}

std::string LLVoiceClient::nameFromsipURI(const std::string& uri)
{
	std::string result;

	size_t sip_offset = uri.find("sip:");
	size_t at_offset = uri.find("@");
	if (sip_offset != std::string::npos && at_offset != std::string::npos)
	{
		result = uri.substr(sip_offset + 4, at_offset - sip_offset - 4);
	}

	return result;
}

bool LLVoiceClient::inSpatialChannel()
{
	return mAudioSession && mAudioSession->mIsSpatial;
}

std::string LLVoiceClient::getAudioSessionURI()
{
	return mAudioSession ? mAudioSession->mSIPURI : "";
}

std::string LLVoiceClient::getAudioSessionHandle()
{
	return mAudioSession ? mAudioSession->mHandle : "";
}

// Because of the recurring voice cutout issues (SL-15072) we are going to try
// to disable the automatic VAD (Voice Activity Detection) and set the
// associated parameters directly. We will expose them via Debug Settings and
// that should let us iterate on a collection of values that work for us.
//
// From the VIVOX docs:
//
// VadAuto: flag to enable (1) or disable (0) automatic VAD.
//
// VadHangover: the time (in milliseconds) that it takes for the VAD to switch
//              back to silence from speech mode after the last speech frame
//              has been detected.
//
// VadNoiseFloor: dimensionless value between 0 and 20000 (default 576) that
//                controls the maximum level at which the noise floor may be
//                set at by the VAD's noise tracking. Too low of a value will
//                make noise tracking ineffective (a value of 0 disables noise
//                tracking and the VAD then  relies purely on the sensitivity
//                property). Too high of a value will make long speech
//                classifiable as noise.
//
// VadSensitivity: dimensionless value between 0 and  100, indicating the
//                 'sensitivity of the VAD'. Increasing this value corresponds
//                 to decreasing the sensitivity of the VAD (i.e. 0 is most
//                 sensitive, while 100 is least sensitive).

void LLVoiceClient::setupVADParams()
{
#if LL_LINUX
	if (mDeprecatedClient)
	{
		return;
	}
#endif
	U32 vad_auto = gSavedSettings.getBool("VivoxVadAuto") ? 1 : 0;
	U32 vad_hangover = gSavedSettings.getU32("VivoxVadHangover");
	U32 vad_noise_floor = gSavedSettings.getU32("VivoxVadNoiseFloor");
	if (vad_noise_floor > 20000)
	{
		vad_noise_floor = 20000;
	}
	U32 vad_sensitivity = gSavedSettings.getU32("VivoxVadSensitivity");
	if (vad_sensitivity > 100)
	{
		vad_sensitivity = 100;
	}
	if (vad_auto)
	{
		llinfos << "Enabling the automatic VAD." << llendl;
	}
	else
	{
		llinfos << "Disabling the automatic VAD. Setting fixed values: VadHangover = "
				<< vad_hangover << " - VadSensitivity = " << vad_sensitivity
				<< " - VadNoiseFloor = " << vad_noise_floor << llendl;
	}
	std::ostringstream stream;
	stream << "<Request requestId=\"" << mCommandCookie++
		   << "\" action=\"Aux.SetVadProperties.1\">"
		   << "<VadAuto>" << vad_auto << "</VadAuto>"
		   << "<VadHangover>" << vad_hangover << "</VadHangover>"
		   << "<VadSensitivity>" << vad_sensitivity << "</VadSensitivity>"
		   << "<VadNoiseFloor>" << vad_noise_floor << "</VadNoiseFloor>"
		   << "</Request>\n\n\n";
	writeString(stream.str());
}

/////////////////////////////
// Sending updates of current state

void LLVoiceClient::enforceTether()
{
	LLVector3d tethered	= mCameraRequestedPosition;

	// Constrain 'tethered' to within 50m of mAvatarPosition.
	F32 max_dist = 50.f;
	LLVector3d camera_offset = mCameraRequestedPosition - mAvatarPosition;
	F32 camera_distance = (F32)camera_offset.length();
	if (camera_distance > max_dist)
	{
		tethered = mAvatarPosition +
				   (max_dist / camera_distance) * camera_offset;
	}

	if (dist_vec(mCameraPosition, tethered) > 0.1)
	{
		mCameraPosition = tethered;
		mSpatialCoordsDirty = true;
	}
}

void LLVoiceClient::updatePosition()
{
	if (!sInitDone)
	{
		return;
	}

	LLViewerRegion* region = gAgent.getRegion();
	if (!region || !isAgentAvatarValid())
	{
		return;
	}

	// *TODO: If camera and avatar velocity are actually used by the voice
	// system, we could compute them here... They are currently always set
	// to zero.

	// Send the current camera position to the voice code
	LLMatrix3 rot;
	rot.setRows(gViewerCamera.getAtAxis(), gViewerCamera.getLeftAxis(),
				gViewerCamera.getUpAxis());

	LLVector3d pos = region->getPosGlobalFromRegion(gViewerCamera.getOrigin());

	setCameraPosition(pos, LLVector3::zero, rot);

	// Send the current avatar position to the voice code
	rot = gAgentAvatarp->getRootJoint()->getWorldRotation().getMatrix3();

	pos = gAgentAvatarp->getPositionGlobal();
#if 0	// *TODO: Can we get the head offset from outside the LLVOAvatar ?
	pos += LLVector3d(mHeadOffset);
#endif
	pos += LLVector3d(0.f, 0.f, 1.f);

	setAvatarPosition(pos, LLVector3::zero, rot);
}

void LLVoiceClient::setCameraPosition(const LLVector3d& position,
									  const LLVector3& velocity,
									  const LLMatrix3& rot)
{
	mCameraRequestedPosition = position;

	if (mCameraVelocity != velocity)
	{
		mCameraVelocity = velocity;
		mSpatialCoordsDirty = true;
	}

	if (mCameraRot != rot)
	{
		mCameraRot = rot;
		mSpatialCoordsDirty = true;
	}
}

void LLVoiceClient::setAvatarPosition(const LLVector3d& position,
									  const LLVector3& velocity,
									  const LLMatrix3& rot)
{
	if (dist_vec(mAvatarPosition, position) > 0.1)
	{
		mAvatarPosition = position;
		mSpatialCoordsDirty = true;
	}

	if (mAvatarVelocity != velocity)
	{
		mAvatarVelocity = velocity;
		mSpatialCoordsDirty = true;
	}

	if (mAvatarRot != rot)
	{
		mAvatarRot = rot;
		mSpatialCoordsDirty = true;
	}
}

bool LLVoiceClient::channelFromRegion(LLViewerRegion* region,
									  std::string& name)
{
	if (region)
	{
		name = region->getName();
	}
	return !name.empty();
}

void LLVoiceClient::leaveChannel()
{
	if (mState == stateRunning)
	{
		LL_DEBUGS("Voice") << "Leaving channel for teleport/logout" << LL_ENDL;
		mChannelName.clear();
		sessionTerminate();
	}
}

void LLVoiceClient::setVoiceEnabled(bool enabled)
{
	if (enabled != mVoiceEnabled)
	{
		mVoiceEnabled = enabled;
		LLVoiceClientStatusObserver::EStatusType status;
		if (enabled)
		{
			LLVoiceChannel::getCurrentVoiceChannel()->activate();
			status = LLVoiceClientStatusObserver::STATUS_VOICE_ENABLED;
		}
		else
		{
			// Turning voice off looses your current channel: this makes sure
			// the UI is not out of sync when you re-enable it.
			LLVoiceChannel::getCurrentVoiceChannel()->deactivate();
			status = LLVoiceClientStatusObserver::STATUS_VOICE_DISABLED;
			mRetries = 0;
		}
		notifyStatusObservers(status);
	}
}

bool LLVoiceClient::voiceEnabled()
{
	static LLCachedControl<bool> enable_voice(gSavedSettings,
											  "EnableVoiceChat");
	static LLCachedControl<bool> disable_voice(gSavedSettings,
											   "CmdLineDisableVoice");
	return enable_voice && !disable_voice;
}

bool LLVoiceClient::lipSyncEnabled()
{
	return mVoiceEnabled && mState != stateDisabled && mLipSyncEnabled;
}

void LLVoiceClient::setUsePTT(bool usePTT)
{
	if (usePTT && !mUsePTT)
	{
		// When the user turns on PTT, reset the current state.
		mUserPTTState = false;
	}
	mUsePTT = usePTT;
}

void LLVoiceClient::setPTTIsToggle(bool PTTIsToggle)
{
	if (!PTTIsToggle && mPTTIsToggle)
	{
		// When the user turns off toggle, reset the current state.
		mUserPTTState = false;
	}
	mPTTIsToggle = PTTIsToggle;
}

void LLVoiceClient::setPTTKey(std::string& key)
{
	if (key == "MiddleMouse")
	{
		mPTTIsMiddleMouse = true;
	}
	else
	{
		mPTTIsMiddleMouse = false;
		if (!LLKeyboard::keyFromString(key.c_str(), &mPTTKey))
		{
			// If the call failed, do not match any key.
			key = KEY_NONE;
		}
	}
}

void LLVoiceClient::setEarLocation(S32 loc)
{
	if (mEarLocation != loc)
	{
		LL_DEBUGS("Voice") << "Setting mEarLocation to " << loc << LL_ENDL;

		mEarLocation = loc;
		mSpatialCoordsDirty = true;
	}
}

void LLVoiceClient::setVoiceVolume(F32 volume)
{
	S32 scaled_volume = scale_speaker_volume(volume);

	if (scaled_volume != mSpeakerVolume)
	{
		if (scaled_volume == 0 || mSpeakerVolume == 0)
		{
			mSpeakerMuteDirty = true;
		}

		mSpeakerVolume = scaled_volume;
		mSpeakerVolumeDirty = true;
	}
}

void LLVoiceClient::setMicGain(F32 volume)
{
	S32 scaled_volume = scale_mic_volume(volume);
	if (scaled_volume != mMicVolume)
	{
		mMicVolume = scaled_volume;
		mMicVolumeDirty = true;
	}
}

void LLVoiceClient::keyDown(KEY key, MASK mask)
{
	if (!gKeyboardp || gKeyboardp->getKeyRepeated(key))
	{
		return;	// ignore auto-repeat keys
	}

	if (!mPTTIsMiddleMouse)
	{
		if (mPTTIsToggle)
		{
			if (key == mPTTKey)
			{
				toggleUserPTTState();
			}
		}
		else if (mPTTKey != KEY_NONE)
		{
			setUserPTTState(gKeyboardp->getKeyDown(mPTTKey));
		}
	}
}
void LLVoiceClient::keyUp(KEY key, MASK mask)
{
	if (!mPTTIsMiddleMouse)
	{
		if (!mPTTIsToggle && (mPTTKey != KEY_NONE) && gKeyboardp)
		{
			setUserPTTState(gKeyboardp->getKeyDown(mPTTKey));
		}
	}
}
void LLVoiceClient::middleMouseState(bool down)
{
	if (mPTTIsMiddleMouse)
	{
		if (mPTTIsToggle)
		{
			if (down)
			{
				toggleUserPTTState();
			}
		}
		else
		{
			setUserPTTState(down);
		}
	}
}

/////////////////////////////
// Accessors for data related to nearby speakers

// Not sure what the semantics of this should be. For now, if we have any data
// about the user that came through the chat channel, assume they have voice.
bool LLVoiceClient::getVoiceEnabled(const LLUUID& id)
{
	return findParticipantByID(id) != NULL;
}

bool LLVoiceClient::getIsSpeaking(const LLUUID& id)
{
	participantState* participant = findParticipantByID(id);
	if (participant)
	{
		if (participant->mSpeakingTimeout.getElapsedTimeF32() > SPEAKING_TIMEOUT)
		{
			participant->mIsSpeaking = false;
		}
		return participant->mIsSpeaking;
	}

	return false;
}

bool LLVoiceClient::getIsModeratorMuted(const LLUUID& id)
{
	participantState* participant = findParticipantByID(id);
	return participant && participant->mIsModeratorMuted;
}

F32 LLVoiceClient::getCurrentPower(const LLUUID& id)
{
	participantState* participant = findParticipantByID(id);
	return participant ? participant->mPower : 0.f;
}

bool LLVoiceClient::getOnMuteList(const LLUUID& id)
{
	participantState* participant = findParticipantByID(id);
	return participant && participant->mOnMuteList;
}

// External accessiors. Maps 0.0 to 1.0 to internal values 0-400 with .5 == 100
// internal = 400 * external^2
F32 LLVoiceClient::getUserVolume(const LLUUID& id)
{
	F32 result = 0.f;

	participantState* participant = findParticipantByID(id);
	if (participant)
	{
		S32 ires = 100; // Nominal default volume

		if (participant->mIsSelf)
		{
			// Always make it look like the user's own volume is set at the
			// default.
		}
		else if (participant->mUserVolume != -1)
		{
			// Use the internal volume
			ires = participant->mUserVolume;
		}
		else if (participant->mVolume != -1)
		{
			// Map backwards from vivox volume
			if (participant->mVolume < 56)
			{
				ires = (participant->mVolume * 100) / 56;
			}
			else
			{
				ires = 300 * (participant->mVolume - 56) / 44 + 100;
			}
		}
		result = sqrtf((F32)ires / 400.f);
	}

	return result;
}

void LLVoiceClient::setUserVolume(const LLUUID& id, F32 volume)
{
	if (mAudioSession)
	{
		participantState* participant = findParticipantByID(id);
		if (participant)
		{
			// Volume can amplify by as much as 4x !
			S32 ivol = (S32)(400.f * volume * volume);
			participant->mUserVolume = llclamp(ivol, 0, 400);
			participant->mVolumeDirty = true;
			mAudioSession->mVolumeDirty = true;
		}
	}
}

#if 0	// Not used
std::string LLVoiceClient::getGroupID(const LLUUID& id)
{
	participantState* participant = findParticipantByID(id);
	return participant ? participant->mGroupID : "";
}
#endif

LLVoiceClient::sessionState::sessionState()
:	mCreateInProgress(false),
	mMediaConnectInProgress(false),
	mVoiceInvitePending(false),
	mSynthesizedCallerID(false),
	mIsChannel(false),
	mIsSpatial(false),
	mIsP2P(false),
	mIncoming(false),
	mVoiceEnabled(false),
	mReconnect(false),
	mVolumeDirty(false)
{
}

LLVoiceClient::sessionState::~sessionState()
{
	removeAllParticipants();
}

bool LLVoiceClient::sessionState::isCallBackPossible()
{
	// This may change to be explicitly specified by Vivox in the future...
	// Currently, only PSTN P2P calls cannot be returned.
	// Conveniently, this is also the only case where we synthesize a caller
	// UUID.
	return !mSynthesizedCallerID;
}

bool LLVoiceClient::sessionState::isTextIMPossible()
{
	// This may change to be explicitly specified by vivox in the future...
	return !mSynthesizedCallerID;
}

LLVoiceClient::sessionState* LLVoiceClient::findSession(const std::string& handle)
{
	session_map_t::iterator iter = mSessionsByHandle.find(&handle);
	return iter != mSessionsByHandle.end() ? iter->second : NULL;
}

LLVoiceClient::sessionState* LLVoiceClient::findSessionBeingCreatedByURI(const std::string& uri)
{
	for (session_set_it_t iter = mSessions.begin(); iter != mSessions.end();
		 ++iter)
	{
		sessionState* session = *iter;
		if (session->mCreateInProgress && session->mSIPURI == uri)
		{
			return session;
		}
	}

	return NULL;
}

LLVoiceClient::sessionState* LLVoiceClient::findSession(const LLUUID& participant_id)
{
	for (session_set_it_t iter = mSessions.begin(); iter != mSessions.end();
		 ++iter)
	{
		sessionState* session = *iter;
		if (session->mCallerID == participant_id ||
			session->mIMSessionID == participant_id)
		{
			return session;
		}
	}

	return NULL;
}

LLVoiceClient::sessionState* LLVoiceClient::addSession(const std::string& uri,
													   const std::string& handle)
{
	sessionState* result = NULL;

	if (handle.empty())
	{
		// No handle supplied: check whether there is already a session with
		// this URI
		for (session_set_it_t iter = mSessions.begin();
			 iter != mSessions.end(); ++iter)
		{
			sessionState* s = *iter;
			if (s->mSIPURI == uri || s->mAlternateSIPURI == uri)
			{
				// *TODO: it is possible that this case we should raise an
				// Internal error.
				result = s;
				break;
			}
		}
	}
	else
	{
		// Check for an existing session with this handle
		session_map_t::iterator iter = mSessionsByHandle.find(&handle);
		if (iter != mSessionsByHandle.end())
		{
			result = iter->second;
		}
	}

	if (!result)
	{
		// No existing session found.

		LL_DEBUGS("Voice") << "Adding new session: handle " << handle
						   << " URI " << uri << LL_ENDL;
		result = new sessionState();
		result->mSIPURI = uri;
		result->mHandle = handle;

		mSessions.insert(result);

		if (!result->mHandle.empty())
		{
			mSessionsByHandle.emplace(&(result->mHandle), result);
		}
	}
	else
	{
		// Found an existing session

		if (uri != result->mSIPURI)
		{
			// TODO: Should this be an Internal error?
			LL_DEBUGS("Voice") << "Changing uri from " << result->mSIPURI
							   << " to " << uri << LL_ENDL;
			setSessionURI(result, uri);
		}

		if (handle != result->mHandle)
		{
			if (handle.empty())
			{
				// There is at least one race condition where where addSession
				// was clearing an existing session handle, which caused things
				// to break.
				LL_DEBUGS("Voice") << "NOT clearing handle " << result->mHandle
								   << LL_ENDL;
			}
			else
			{
				// TODO: Should this be an Internal error ?
				LL_DEBUGS("Voice") << "Changing handle from " << result->mHandle
								   << " to " << handle << LL_ENDL;
				setSessionHandle(result, handle);
			}
		}

		LL_DEBUGS("Voice") << "Returning existing session: handle " << handle
						   << " URI " << uri << LL_ENDL;
	}

	verifySessionState();

	return result;
}

void LLVoiceClient::setSessionHandle(sessionState* session,
									 const std::string& handle)
{
	// Have to remove the session from the handle-indexed map before changing
	// the handle, or things will break badly.

	if (!session->mHandle.empty())
	{
		// Remove session from the map if it should have been there.
		session_map_t::iterator iter =
			mSessionsByHandle.find(&(session->mHandle));
		if (iter != mSessionsByHandle.end())
		{
			if (iter->second != session)
			{
				llwarns << "Internal error: session mismatch !" << llendl;
				giveUp();
				return;
			}

			mSessionsByHandle.erase(iter);
		}
		else
		{
			llwarns << "Internal error: session handle not found in map !"
					<< llendl;
			giveUp();
			return;
		}
	}

	session->mHandle = handle;

	if (!handle.empty())
	{
		mSessionsByHandle.emplace(&(session->mHandle), session);
	}

	verifySessionState();
}

void LLVoiceClient::setSessionURI(sessionState* session,
								  const std::string& uri)
{
	// There used to be a map of session URIs to sessions, which made this
	// complex....
	session->mSIPURI = uri;

	verifySessionState();
}

void LLVoiceClient::deleteSession(sessionState* session)
{
	// Remove the session from the handle map
	if (!session->mHandle.empty())
	{
		session_map_t::iterator iter =
			mSessionsByHandle.find(&(session->mHandle));
		if (iter != mSessionsByHandle.end())
		{
			if (iter->second != session)
			{
				llwarns << "Internal error: session mismatch !" << llendl;
				giveUp();
				return;
			}
			mSessionsByHandle.erase(iter);
		}
	}

	// Remove the session from the URI map
	mSessions.erase(session);

	// At this point, the session should be unhooked from all lists and all
	// states should be consistent.
	verifySessionState();

	// If this is the current audio session, clean up the pointer which will
	// soon be dangling.
	if (mAudioSession == session)
	{
		mAudioSession = NULL;
	}

	// Ditto for the next audio session
	if (mNextAudioSession == session)
	{
		mNextAudioSession = NULL;
	}

	// Delete the session
	delete session;
}

void LLVoiceClient::deleteAllSessions()
{
	while (!mSessions.empty())
	{
		deleteSession(*(mSessions.begin()));
	}

	if (!mSessionsByHandle.empty())
	{
		llwarns << "Internal error: empty session map, non-empty handle map"
				<< llendl;
		giveUp();
	}
}

void LLVoiceClient::verifySessionState()
{
	// This is mostly intended for debugging problems with session state
	// management.
	LL_DEBUGS("Voice") << "Total session count: " << mSessions.size()
					   << " , session handle map size: "
					   << mSessionsByHandle.size() << LL_ENDL;

	session_map_t::iterator map_end = mSessionsByHandle.end();
	session_set_it_t end = mSessions.end();
	for (session_set_it_t iter = mSessions.begin(); iter != end; ++iter)
	{
		sessionState* session = *iter;

		LL_DEBUGS("Voice") << "Session " << session << ": handle "
						   << session->mHandle << ", URI " << session->mSIPURI
						   << LL_ENDL;

		if (!session->mHandle.empty())
		{
			// Every session with a non-empty handle needs to be in the handle
			// map
			session_map_t::iterator i2 =
				mSessionsByHandle.find(&(session->mHandle));
			if (i2 == map_end)
			{
				llwarns << "Internal error (handle " << session->mHandle
						<< " not found in session map)" << llendl;
				giveUp();
				return;
			}
			else if (i2->second != session)
			{
				llwarns << "Internal error (handle " << session->mHandle
						<< " in session map points to another session)"
						<< llendl;
				giveUp();
				return;
			}
		}
	}

	// Check that every entry in the handle map points to a valid session in
	// the session set
	for (session_map_t::iterator iter = mSessionsByHandle.begin();
		 iter != map_end; ++iter)
	{
		sessionState* session = iter->second;
		session_set_it_t i2 = mSessions.find(session);
		if (i2 == mSessions.end())
		{
			llwarns << "Internal error (session for handle "
					<< session->mHandle << " not found in session map)"
					<< llendl;
			giveUp();
			return;
		}
		else if (session->mHandle != (*i2)->mHandle)
		{
			llwarns << "Internal error (session for handle "
					<< session->mHandle
					<< " points to session with different handle "
					<< (*i2)->mHandle << ")" << llendl;
			giveUp();
			return;
		}
	}
}

void LLVoiceClient::addObserver(LLVoiceClientStatusObserver* observer)
{
	mStatusObservers.insert(observer);
}

void LLVoiceClient::removeObserver(LLVoiceClientStatusObserver* observer)
{
	mStatusObservers.erase(observer);
}

void LLVoiceClient::notifyStatusObservers(LLVoiceClientStatusObserver::EStatusType status)
{
	if (mAudioSession)
	{
		if (status == LLVoiceClientStatusObserver::ERROR_UNKNOWN)
		{
			switch (mAudioSession->mErrorStatusCode)
			{
				case 20713:
					status = LLVoiceClientStatusObserver::ERROR_CHANNEL_FULL;
				 	break;

				case 20714:
					status = LLVoiceClientStatusObserver::ERROR_CHANNEL_LOCKED;
					 break;

				case 20715:
					// Invalid channel, we may be using a set of poorly cached
					// info
					status = LLVoiceClientStatusObserver::ERROR_NOT_AVAILABLE;
					break;

				case 1009:
					// Invalid username and password
					status = LLVoiceClientStatusObserver::ERROR_NOT_AVAILABLE;
			}

			// Reset the error code to make sure it would not be reused later
			// by accident.
			mAudioSession->mErrorStatusCode = 0;
		}
		else if (status == LLVoiceClientStatusObserver::STATUS_LEFT_CHANNEL)
		{
			switch (mAudioSession->mErrorStatusCode)
			{
				case HTTP_NOT_FOUND:		// 404
				//  *TODO: Should this be 503 ?

				case 480:					// TEMPORARILY_UNAVAILABLE
				case HTTP_REQUEST_TIME_OUT:	// 408
					// Call failed because other user was not available treat
					// this as an error case
					status = LLVoiceClientStatusObserver::ERROR_NOT_AVAILABLE;

					// Reset the error code to make sure it would not be reused
					// later by accident.
					mAudioSession->mErrorStatusCode = 0;
			}
		}
	}

	LL_DEBUGS("Voice") << LLVoiceClientStatusObserver::status2string(status)
					   << ", session URI " << getAudioSessionURI()
					   << ", proximal is "
					   << (inSpatialChannel() ? "true" : "false") << LL_ENDL;

	for (status_observer_set_t::iterator it = mStatusObservers.begin();
		it != mStatusObservers.end(); )
	{
		LLVoiceClientStatusObserver* observer = *it;
		observer->onChange(status, getAudioSessionURI(), inSpatialChannel());
		// In case onError() deleted an entry.
		it = mStatusObservers.upper_bound(observer);
	}
}

void LLVoiceClient::lookupName(const LLUUID& id)
{
	if (gCacheNamep)
	{
		gCacheNamep->get(id, false, onAvatarNameLookup);
	}
}

//static
void LLVoiceClient::onAvatarNameLookup(const LLUUID& id,
									   const std::string& fullname,
									   bool is_group)
{
	if (sInitDone)
	{
		gVoiceClient.avatarNameResolved(id, fullname);
	}
}

void LLVoiceClient::avatarNameResolved(const LLUUID& id,
									   const std::string& name)
{
	// Iterate over all sessions.
	for (session_set_it_t iter = mSessions.begin(); iter != mSessions.end();
		 ++iter)
	{
		sessionState* session = *iter;

		// Check for this user as a participant in this session
		participantState* participant = session->findParticipantByID(id);
		if (participant)
		{
			// Found: fill in the name
			participant->mAccountName = name;
		}

		// Check whether this is a P2P session whose caller name just resolved
		if (session->mCallerID == id)
		{
			// This session's "caller ID" just resolved.  Fill in the name.
			session->mName = name;
			if (session->mVoiceInvitePending)
			{
				session->mVoiceInvitePending = false;

				gIMMgrp->inviteToSession(session->mIMSessionID,
										session->mName,
										session->mCallerID,
										session->mName,
										IM_SESSION_P2P_INVITE,
										LLIMMgr::INVITATION_TYPE_VOICE,
										session->mHandle,
										session->mSIPURI);
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
// LLViewerParcelVoiceInfo class
///////////////////////////////////////////////////////////////////////////////

class LLViewerParcelVoiceInfo final : public LLHTTPNode
{
	void post(LLHTTPNode::ResponsePtr response, const LLSD& context,
			  const LLSD& input) const override
	{
		// The parcel you are in has changed something about its voice
		// information. This is a misnomer, as it can also be when you are not
		// in a parcel at all. Should really be something like
		// LLViewerVoiceInfoChanged...
		if (input.has("body"))
		{
			LLSD body = input["body"];

			// body has "region_name" (str), "parcel_local_id"(int),
			// "voice_credentials" (map).

			// body["voice_credentials"] has "channel_uri" (str),
			// body["voice_credentials"] has "channel_credentials" (str)

			// if we really wanted to be extra careful, we'd check the supplied
			// local parcel id to make sure it's for the same parcel we believe
			// we're in
			if (body.has("voice_credentials"))
			{
				LLSD voice_credentials = body["voice_credentials"];
				std::string uri;
				std::string credentials;

				if (voice_credentials.has("channel_uri"))
				{
					uri = voice_credentials["channel_uri"].asString();
				}
				if (voice_credentials.has("channel_credentials"))
				{
					credentials = voice_credentials["channel_credentials"].asString();
				}

				gVoiceClient.setSpatialChannel(uri, credentials);
			}
		}
	}
};

LLHTTPRegistration<LLViewerParcelVoiceInfo>
    gHTTPRegistrationMessageParcelVoiceInfo("/message/ParcelVoiceInfo");

///////////////////////////////////////////////////////////////////////////////
// LLViewerRequiredVoiceVersion class
///////////////////////////////////////////////////////////////////////////////

class LLViewerRequiredVoiceVersion final : public LLHTTPNode
{
	void post(LLHTTPNode::ResponsePtr response, const LLSD& context,
			  const LLSD& input) const override
	{
		// You received this messsage (most likely on region cross or teleport)
		if (input.has("body") && input["body"].has("major_version"))
		{
			S32 major = input["body"]["major_version"].asInteger();
			if (LLVoiceClient::sInitDone && major > VOICE_MAJOR_VERSION)
			{
				gNotifications.add("VoiceVersionMismatch");
				// Toggles the listener
				gSavedSettings.setBool("EnableVoiceChat", false);
			}
		}
	}
};

LLHTTPRegistration<LLViewerRequiredVoiceVersion>
    gHTTPRegistrationMessageRequiredVoiceVersion("/message/RequiredVoiceVersion");
