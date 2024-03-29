/**
 * @file llvoicechannel.h
 * @brief Voice channel related classes
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

#ifndef LL_VOICECHANNEL_H
#define LL_VOICECHANNEL_H

#include "hbfastmap.h"
#include "llpanel.h"

#include "llvoiceclient.h"

class LLVoiceChannel : public LLVoiceClientStatusObserver
{
protected:
	LOG_CLASS(LLVoiceChannel);

public:
	typedef enum e_voice_channel_state
	{
		STATE_NO_CHANNEL_INFO,
		STATE_ERROR,
		STATE_HUNG_UP,
		STATE_READY,
		STATE_CALL_STARTED,
		STATE_RINGING,
		STATE_CONNECTED
	} EState;

	LLVoiceChannel(const LLUUID& session_id, const std::string& session_name);
	virtual ~LLVoiceChannel();

	void onChange(EStatusType status,
				  const std::string& channel_uri, bool proximal) override;

	virtual void handleStatusChange(EStatusType status);
	virtual void handleError(EStatusType status);
	virtual void deactivate();
	virtual void activate();
	virtual void setChannelInfo(const std::string& uri,
								const std::string& credentials);
	virtual void getChannelInfo();
	virtual bool isActive();
	virtual bool callStarted();

	const LLUUID getSessionID()						{ return mSessionID; }
	EState getState()								{ return mState; }

	void updateSessionID(const LLUUID& new_session_id);
	const LLSD& getNotifyArgs()						{ return mNotifyArgs; }

	static LLVoiceChannel* getChannelByID(const LLUUID& session_id);
	static LLVoiceChannel* getChannelByURI(std::string uri);
	static LLVoiceChannel* getCurrentVoiceChannel()	{ return sCurrentVoiceChannel; }
	static void initClass();

	static void suspend();
	static void resume();

protected:
	virtual void setState(EState state);
	void setURI(std::string uri);

protected:
	LLHandle<LLPanel>				mLoginNotificationHandle;
	std::string						mURI;
	std::string						mCredentials;
	std::string						mSessionName;
	LLUUID							mSessionID;
	EState							mState;
	LLSD							mNotifyArgs;
	bool							mIgnoreNextSessionLeave;

	typedef fast_hmap<LLUUID, LLVoiceChannel*> voice_channel_map_t;
	static voice_channel_map_t		sVoiceChannelMap;

	typedef std::map<std::string, LLVoiceChannel*> voice_channel_map_uri_t;
	static voice_channel_map_uri_t	sVoiceChannelURIMap;

	static LLVoiceChannel*			sCurrentVoiceChannel;
	static LLVoiceChannel*			sSuspendedVoiceChannel;
	static bool						sSuspended;
};

class LLVoiceChannelGroup : public LLVoiceChannel
{
protected:
	LOG_CLASS(LLVoiceChannelGroup);

public:
	LLVoiceChannelGroup(const LLUUID& session_id,
						const std::string& session_name);

	void handleStatusChange(EStatusType status) override;
	void handleError(EStatusType status) override;
	void activate() override;
	void deactivate() override;
	void setChannelInfo(const std::string& uri,
						const std::string& credentials) override;
	void getChannelInfo() override;

protected:
	void setState(EState state) override;

private:
	static void voiceCallCapCoro(const std::string& url, LLUUID session_id);

private:
	U32		mRetries;
	bool	mIsRetrying;
};

class LLVoiceChannelProximal final : public LLVoiceChannel,
									 public LLSingleton<LLVoiceChannelProximal>
{
	friend class LLSingleton<LLVoiceChannelProximal>;

protected:
	LOG_CLASS(LLVoiceChannelProximal);

public:
	LLVoiceChannelProximal();

	void onChange(EStatusType status, const std::string& channel_uri,
				  bool proximal) override;
	void handleStatusChange(EStatusType status) override;
	void handleError(EStatusType status) override;
	bool isActive() override;
	void activate() override;
	void deactivate() override;
};

class LLVoiceChannelP2P final : public LLVoiceChannelGroup
{
protected:
	LOG_CLASS(LLVoiceChannelP2P);

public:
	LLVoiceChannelP2P(const LLUUID& session_id,
					  const std::string& session_name,
					  const LLUUID& other_user_id);

	void handleStatusChange(EStatusType status) override;
	void handleError(EStatusType status) override;
    void activate() override;
	void getChannelInfo() override;

	void setSessionHandle(const std::string& handle,
						  const std::string& in_uri);

protected:
	void setState(EState state) override;

private:
	std::string	mSessionHandle;
	LLUUID		mOtherUserID;
	bool		mReceivedCall;
};

#endif	// LL_VOICECHANNEL_H
