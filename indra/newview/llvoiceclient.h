/**
*  @file llvoiceclient.h
 * @brief Declaration of LLVoiceClient class which is the interface to the
 * voice client process.
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

#ifndef LL_VOICE_CLIENT_H
#define LL_VOICE_CLIENT_H

#include <queue>
#include <vector>

#include "llframetimer.h"
#include "lliosocket.h"
#include "llpumpio.h"
#include "llvector3.h"

#include "llviewerregion.h"

class LLProcessLauncher;
class LLVOAvatar;
class LLVivoxProtocolParser;

constexpr F32 OVERDRIVEN_POWER_LEVEL = 0.7f;

class LLVoiceClientStatusObserver
{
public:
	typedef enum e_voice_status_type
	{
		// NOTE: when updating this enum, please also update the switch in
		//  LLVoiceClientStatusObserver::status2string().
		STATUS_LOGIN_RETRY,
		STATUS_LOGGED_IN,
		STATUS_JOINING,
		STATUS_JOINED,
		STATUS_LEFT_CHANNEL,
		STATUS_VOICE_DISABLED,
		STATUS_VOICE_ENABLED,
		BEGIN_ERROR_STATUS,
		ERROR_CHANNEL_FULL,
		ERROR_CHANNEL_LOCKED,
		ERROR_NOT_AVAILABLE,
		ERROR_UNKNOWN
	} EStatusType;

	virtual ~LLVoiceClientStatusObserver() = default;
	virtual void onChange(EStatusType status, const std::string& channelURI,
						  bool proximal) = 0;

	static std::string status2string(EStatusType inStatus);
};

class LLVoiceClient
{
	friend class LLVivoxProtocolParser;

protected:
	LOG_CLASS(LLVoiceClient);

public:
	LLVoiceClient();
	~LLVoiceClient();

public:
	// Call after loading settings and whenever they change
	void updateSettings();

	// Methods used in llfloatervoicedevicesettings.cpp

	void setCaptureDevice(const std::string& name);

	typedef std::vector<std::string> device_list_t;
	LL_INLINE device_list_t* getCaptureDevices()		{ return &mCaptureDevices; }

	void setRenderDevice(const std::string& name);

	LL_INLINE device_list_t* getRenderDevices()			{ return &mRenderDevices; }

	void tuningStart();
	LL_INLINE void tuningStop()							{ mTuningMode = false; }
	bool inTuningMode();

	void tuningSetMicVolume(F32 volume);
#if 0	// Not used
	void tuningSetSpeakerVolume(F32 volume);
#endif
	LL_INLINE F32 tuningGetEnergy()						{ return mTuningEnergy; }

	// This returns true when it's safe to bring up the "device settings"
	// dialog in the prefs. I.e. when the daemon is running and connected, and
	// the device lists are populated.
	bool deviceSettingsAvailable();

	// Requery the vivox daemon for the current list of input/output devices.
	// If you pass true for clearCurrentList, deviceSettingsAvailable() will be
	// false until the query has completed (use this if you want to know when
	// it is done). If you pass false, you will have no way to know when the
	// query finishes, but the device lists will not appear empty in the
	// interim.
	void refreshDeviceLists(bool clearCurrentList = true);

	// Used in mute list observer
	void muteListChanged();

	/////////////////////////////
	// Sending updates of current state

	// Called on logout or teleport begin from llappviewer.cpp and llagent.cpp
	void leaveChannel();

	// Use this to mute the local mic (for when the client is minimized, etc),
	// ignoring user PTT state. Used from llvieweraudio.cpp.
	LL_INLINE void setMuteMic(bool muted)				{ mMuteMic = muted; }
	// Used from llvieweraudio.cpp
	void setVoiceVolume(F32 volume);
	void setMicGain(F32 volume);

	// Used from llfloateractivespeakers.cpp and llfloaterim.cpp

	F32 getUserVolume(const LLUUID& id);
	// Sets volume for specified agent, from 0-1 (where .5 is nominal)
	void setUserVolume(const LLUUID& id, F32 volume);

	// Used from llvoiceremotectrl.cpp
	LL_INLINE void setUserPTTState(bool ptt)			{ mUserPTTState = ptt; }
	LL_INLINE bool getUserPTTState()					{ return mUserPTTState; }
	LL_INLINE void toggleUserPTTState()					{ mUserPTTState = !mUserPTTState; }

	// Used from llvoavatar.cpp
	bool lipSyncEnabled();

	void setUsePTT(bool usePTT);
	void setPTTIsToggle(bool PTTIsToggle);
	void setPTTKey(std::string& key);
	void setEarLocation(S32 loc);
	LL_INLINE void setLipSyncEnabled(bool enabled)		{ mLipSyncEnabled = enabled; }

	// PTT key triggering. Used from llviewerwindow.cpp
	void keyDown(KEY key, MASK mask);
	void keyUp(KEY key, MASK mask);
	void middleMouseState(bool down);

	/////////////////////////////
	// Accessors for data related to nearby speakers

	// Used from llfloateractivespeakers.cpp and llvoavatar.cpp

	bool getIsSpeaking(const LLUUID& id);
	// true if we have received data for this avatar.
	bool getVoiceEnabled(const LLUUID& id);

	// "power" is related to "amplitude" in a defined way. I'm just not sure
	// what the formula is...
	F32 getCurrentPower(const LLUUID& id);

	// Used from llfloateractivespeakers.cpp
	bool getIsModeratorMuted(const LLUUID& id);
	bool getOnMuteList(const LLUUID& id);

#if 0	// Not used
	// Group ID if the user is in group chat (empty string if not applicable)
	std::string getGroupID(const LLUUID& id);

	// Returns true if the area the avatar is in is speech-disabled. Use this
	// to determine whether to show a "no speech" icon in the status bar.
	LL_INLINE bool getAreaVoiceDisabled()				{ return mAreaVoiceDisabled; }
#endif

	// This is used by the string-keyed maps below, to avoid storing the string
	// twice. The 'const std::string*' in the key points to a string actually
	// stored in the object referenced by the map. The add and delete
	// operations for each map allocate and delete in the right order to avoid
	// dangling references. The default compare operation would just compare
	// pointers, which is incorrect, so they must use this comparator instead.
	struct stringMapComparator
	{
		LL_INLINE bool operator()(const std::string* a,
								  const std::string* b) const
		{
			return a->compare(*b) < 0;
		}
	};

	struct uuidMapComparator
	{
		LL_INLINE bool operator()(const LLUUID* a, const LLUUID* b) const
		{
			return *a < *b;
		}
	};

	class participantState
	{
	public:
		participantState(const std::string& uri);

		LL_INLINE bool isAvatar()						{ return mAvatarIDValid; }

		bool updateMuteState();

	public:
		S32 mVolume;
		S32 mUserVolume;
		F32 mPower;
		F32	mLastSpokeTimestamp;
		LLUUID mAvatarID;
		std::string mURI;
		std::string mAccountName;
		std::string mLegacyName;
		std::string mDisplayName;
		std::string mGroupID;
		LLFrameTimer mSpeakingTimeout;
		bool mIsSelf;
		bool mAvatarIDValid;
		bool mPTT;
		bool mIsSpeaking;
		bool mIsModeratorMuted;

		// true if this avatar is on the user's mute list (and should be muted)
		bool mOnMuteList;

		// true if this participant needs a volume command sent (either
		// mOnMuteList or mUserVolume has changed):
		bool mVolumeDirty;
	};
	typedef std::map<const std::string*, participantState*,
					 stringMapComparator> particip_map_t;
	// Used in llfloateractivespeakers.cpp
	particip_map_t* getParticipantList();

	// Called from llstartup.cpp
	void userAuthorized(const std::string& first_name,
						const std::string& last_name, const LLUUID& agent_id);

	// Used by llvoicechannel.cpp

	void addObserver(LLVoiceClientStatusObserver* observer);
	void removeObserver(LLVoiceClientStatusObserver* observer);

	void setNonSpatialChannel(const std::string& uri,
							  const std::string& credentials);
	void leaveNonSpatialChannel();
	// Returns the URI of the current channel, or an empty string if not
	// currently in a channel.
	// NOTE that it will return an empty string if it's in the process of
	// joining a channel.
	std::string getCurrentChannel();

	// Starts a voice session with the specified user
	void callUser(const LLUUID& uuid);

	bool answerInvite(std::string& session_handle);

	std::string sipURIFromID(const LLUUID& id);

	// Used by LLViewerParcelVoiceInfo
	void setSpatialChannel(const std::string& uri,
						   const std::string& credentials);

	// Called from llfloaterim.cpp

	// Returns true if the indicated participant is really an SL avatar.
	// This should be used to control the state of the "profile" button.
	// Currently this will be false only for PSTN callers into group chats and
	// PSTN p2p calls.
	bool isParticipantAvatar(const LLUUID& id);

	// Returns true if calling back the session URI after the session has
	// closed is possible. Currently this will be false only for PSTN P2P
	// calls. NOTE: this will return true if the session can't be found.
	bool isSessionCallBackPossible(const LLUUID& session_id);

	// Returns true if the session can accepte text IM's. Currently this will
	// be false only for PSTN P2P calls. NOTE: this will return true if the
	// session can't be found.
	bool isSessionTextIMPossible(const LLUUID& session_id);

	// Called from llimmgr.cpp
	void declineInvite(std::string& session_handle);

	// Called from llfloateractivespeakers.cpp, llvoavatar.cpp and
	// llvoicechannel.cpp
	// Returns true iff the user is currently in a proximal (local spatial)
	// channel. Note that gestures should only fire if this returns true.
	bool inProximalChannel();

	// Called once from llappviewer.cpp at application startup (creates
	// the connector)
	static void init(LLPumpIO* pump);
	// Called from llappviewer.cpp to clean up during shutdown
	static void terminate();

	// Called from various places in the viewer
	static bool voiceEnabled();

private:
	enum streamState
	{
		streamStateUnknown = 0,
		streamStateIdle,
		streamStateConnected,
		streamStateRinging,
		// Same as Vivox session_media_connecting enum
		streamStateConnecting = 6,
		// Same as Vivox session_media_disconnecting enum
		streamStateDisconnecting = 7,
	};

	typedef std::map<const LLUUID*, participantState*,
					 uuidMapComparator> particip_id_map_t;
	typedef particip_id_map_t::value_type particip_id_map_val_t;

	class sessionState
	{
	public:
		sessionState();
		~sessionState();

		participantState* addParticipant(const std::string& uri);
		// Note: after removeParticipant returns, the participant* that was
		// passed to it will have been deleted.
		// Take care not to use the pointer again after that.
		void removeParticipant(participantState* participant);
		void removeAllParticipants();

		participantState* findParticipant(const std::string& uri);
		participantState* findParticipantByID(const LLUUID& id);

		bool isCallBackPossible();
		bool isTextIMPossible();

	public:
		S32					mErrorStatusCode;

		LLUUID				mIMSessionID;
		LLUUID				mCallerID;

		std::string			mHandle;
		std::string			mGroupHandle;
		std::string			mSIPURI;
		std::string			mAlias;
		std::string			mName;
		std::string			mAlternateSIPURI;
		std::string			mHash;			// Channel password
		std::string			mErrorStatusString;

		particip_map_t		mParticipantsByURI;
		particip_id_map_t	mParticipantsByUUID;

		// True if a Session.Create has been sent for this session and no
		// response has been received yet:
		bool				mCreateInProgress;
		// True if a Session.MediaConnect has been sent for this session and no
		// response has been received yet:
		bool				mMediaConnectInProgress;
		// True if a voice invite is pending for this session (usually waiting
		// on a name lookup):
		bool				mVoiceInvitePending;
		// True if the caller ID is a hash of the SIP URI (this means we should
		// not do a name lookup):
		bool				mSynthesizedCallerID;
		// True for both group and spatial channels (false for p2p, PSTN):
		bool				mIsChannel;
		bool				mIsSpatial;	// true for spatial channels
		bool				mIsP2P;
		bool				mIncoming;
		bool				mVoiceEnabled;
		// Whether we should try to reconnect to this session if it's dropped:
		bool				mReconnect;
		// Set to true when the mute state of someone in the participant list
		// changes. The code will have to walk the list to find the changed
		// participant(s):
		bool				mVolumeDirty;
	};

	participantState* findParticipantByID(const LLUUID& id);

	sessionState* findSession(const std::string& handle);
	sessionState* findSessionBeingCreatedByURI(const std::string& uri);
	sessionState* findSession(const LLUUID& participant_id);

	sessionState* addSession(const std::string& uri,
							 const std::string& handle = LLStringUtil::null);

	void setSessionHandle(sessionState* session,
						  const std::string& handle = LLStringUtil::null);

	void setSessionURI(sessionState* session, const std::string& uri);
	void deleteSession(sessionState* session);
	void deleteAllSessions();

	void verifySessionState();

	// This is called in several places where the session _may_ need to be
	// deleted. It contains logic for whether to delete the session or keep it
	// around.
	void reapSession(sessionState* session);

	// Returns true if the session seems to indicate we've moved to a region on
	// a different voice server
	bool sessionNeedsRelog(sessionState* session);

	// Pokes the state machine to shut down the connector and restart it.
	void requestRelog();

	void setVoiceEnabled(bool enabled);

	LL_INLINE void clearCaptureDevices()				{ mCaptureDevices.clear(); }

	LL_INLINE void addCaptureDevice(const std::string& name)
	{
		mCaptureDevices.push_back(name);
	}

	LL_INLINE void clearRenderDevices()					{ mRenderDevices.clear(); }

	LL_INLINE void addRenderDevice(const std::string& name)
	{
		mRenderDevices.push_back(name);
	}

	void tuningRenderStartSendMessage(const std::string& name, bool loop);
	void tuningRenderStopSendMessage();

	void tuningCaptureStartSendMessage(S32 duration);
	void tuningCaptureStopSendMessage();

	// Call this if the connection to the daemon terminates unexpectedly. It
	// will attempt to reset everything and relaunch.
	void daemonDied();

	// Call this if we're just giving up on voice (can't provision an account,
	// etc). It will clean up and go away.
	void giveUp();

	/////////////////////////////
	// Session control messages
	void connectorCreate();
	void connectorShutdown();

	void closeSocket();

	void requestVoiceAccountProvision(S32 retries = 3);

	void login(const std::string& account_name, const std::string& password,
			   const std::string& sip_uri_hostname,
			   const std::string& account_server_uri);
	void loginSendMessage();
	void logout();
	void logoutSendMessage();

	// Pokes the state machine to leave the audio session next time around.
	void sessionTerminate();

	// Does the actual work to get out of the audio session
	void leaveAudioSession();

	void lookupName(const LLUUID& id);
	static void onAvatarNameLookup(const LLUUID& id,
								   const std::string& fullname,
								   bool is_group);
	void avatarNameResolved(const LLUUID& id, const std::string& name);

	/////////////////////////////
	// Response/Event handlers
	void connectorCreateResponse(S32 status_code, std::string& status_str,
								 std::string& connector_handle,
								 std::string& version_id);
	void loginResponse(S32 status_code, std::string& status_str,
					   std::string& account_handle, S32 aliases_number);
	void sessionCreateResponse(std::string& request_id, S32 status_code,
							   std::string& status_str,
							   std::string& session_handle);
	void sessionGroupAddSessionResponse(std::string& request_id, S32 status_code,
										std::string& status_str,
										std::string& session_handle);
	void sessionConnectResponse(std::string& request_id, S32 status_code,
								std::string& status_str);
	void logoutResponse(S32 status_code, std::string& status_str);
	void connectorShutdownResponse(S32 status_code, std::string& status_str);

	void accountLoginStateChangeEvent(std::string& account_handle,
									  S32 status_code,
									  std::string& status_str, S32 state);
	void mediaStreamUpdatedEvent(std::string& session_handle,
								 std::string& session_grp_handle,
								 S32 status_code, std::string& status_str,
								 S32 state, bool incoming);
	void sessionAddedEvent(std::string& uri_str, std::string& alias,
						   std::string& session_handle,
						   std::string& session_grp_handle,
						   bool is_channel, bool incoming,
						   std::string& name_str);
	void sessionRemovedEvent(std::string& session_handle,
							 std::string& session_grp_handle);
	void participantAddedEvent(std::string& session_handle,
							   std::string& session_grp_handle,
							   std::string& uri_str, std::string& alias,
							   std::string& name_str,
							   std::string& display_name_str,
							   S32 participantType);
	void participantRemovedEvent(std::string& session_handle,
								 std::string& session_grp_handle,
								 std::string& uri_str, std::string& alias,
								 std::string& name_str);
	void participantUpdatedEvent(std::string& session_handle,
								 std::string& session_grp_handle,
								 std::string& uri_str, std::string& alias,
								 bool muted_by_moderator, bool speaking,
								 S32 volume, F32 energy);

	void messageEvent(std::string& session_handle, std::string& uri_str,
					  std::string& alias, std::string& msg_header,
					  std::string& msg_body);
	void sessionNotificationEvent(std::string& session_handle,
								  std::string& uri_str,
								  std::string& notif_type);

	LL_INLINE void auxAudioPropertiesEvent(F32 energy)	{ mTuningEnergy = energy; }

	void joinedAudioSession(sessionState* session);
	void leftAudioSession(sessionState* session);

	void sessionCreateSendMessage(sessionState* session,
								  bool start_audio = true,
								  bool start_text = false);
	void sessionGroupAddSessionSendMessage(sessionState* session,
										   bool start_audio = true,
										   bool start_text = false);

	// Just joins the audio session:
	void sessionMediaConnectSendMessage(sessionState* session);

	// Just joins the text session:
	void sessionTextConnectSendMessage(sessionState* session);

#if 0	// Not used
	void sessionTerminateSendMessage(sessionState* session);
#endif
	void sessionGroupTerminateSendMessage(sessionState* session);

	void sessionMediaDisconnectSendMessage(sessionState* session);

	bool writeString(const std::string& str);

	void getCaptureDevicesSendMessage();
	void getRenderDevicesSendMessage();

	/////////////////////////////
	// Sending updates of current state

	void setCameraPosition(const LLVector3d& position,
						   const LLVector3& velocity,
						   const LLMatrix3& rot);
	void setAvatarPosition(const LLVector3d& position,
						   const LLVector3& velocity,
						   const LLMatrix3& rot);
	void updatePosition();

	// Internal state for a simple state machine. This is used to deal with the
	// asynchronous nature of some of the messages. Note: if you change this
	// list, please make corresponding changes to LLVoiceClient::state2string()
	enum state
	{
		stateDisableCleanup,
		stateDisabled,				// Voice is turned off.
		stateStart,					// Class is initialized, socket is created
		stateDaemonLaunched,		// Daemon has been launched
		stateConnecting,			// connect() call has been issued
		stateConnected,				// connection to the daemon has been made, send some initial setup commands.
		stateIdle,					// socket is connected, ready for messaging
		stateMicTuningStart,
		stateMicTuningRunning,
		stateMicTuningStop,
		stateConnectorStart,		// connector needs to be started
		stateConnectorStarting,		// waiting for connector handle
		stateConnectorStarted,		// connector handle received
		stateLoginRetry,			// need to retry login (failed due to changing password)
		stateLoginRetryWait,		// waiting for retry timer
		stateNeedsLogin,			// send login request
		stateLoggingIn,				// waiting for account handle
		stateLoggedIn,				// account handle received
		stateNoChannel,				//
		stateJoiningSession,		// waiting for session handle
		stateSessionJoined,			// session handle received
		stateRunning,				// in session, steady state
		stateLeavingSession,		// waiting for terminate session response
		stateSessionTerminated,		// waiting for terminate session response

		stateLoggingOut,			// waiting for logout response
		stateLoggedOut,				// logout response received
		stateConnectorStopping,		// waiting for connector stop
		stateConnectorStopped,		// connector stop received

		// We go to this state if the login fails because the account needs to
		// be provisioned.

		// Error states.  No way to recover from these yet.
		stateConnectorFailed,
		stateConnectorFailedWaiting,
		stateLoginFailed,
		stateLoginFailedWaiting,
		stateJoinSessionFailed,
		stateJoinSessionFailedWaiting,

		// Go here when all else has failed. Nothing will be retried, we are done.
		stateJail
	};

	void setState(state new_state);

	void stateMachine();

	// This should be called when the code detects we have changed parcels.
	// It initiates the call to the server that gets the parcel channel.
	void parcelChanged();

	void switchChannel(std::string uri = std::string(),
					   bool spatial = true, bool no_reconnect = false,
					   bool is_p2p = false, std::string hash = "");
	void joinSession(sessionState* session);

	std::string sipURIFromAvatar(LLVOAvatar* avatar);
	std::string sipURIFromName(std::string& name);

	bool inSpatialChannel();
	std::string getAudioSessionURI();
	std::string getAudioSessionHandle();

	void sendPositionalUpdate();

	void buildSetCaptureDevice(std::ostringstream& stream);
	void buildSetRenderDevice(std::ostringstream& stream);
	void buildLocalAudioUpdates(std::ostringstream& stream);

#if 0	// Vivox text IMs are not in use.
	// Start a text IM session with the specified user. This will be
	// asynchronous, the session may be established at a future time.
	sessionState* startUserIMSession(const LLUUID& uuid);
	// Closes any existing text IM session with the specified user
	void endUserIMSession(const LLUUID& uuid);
	void sessionTextDisconnectSendMessage(sessionState* session);
#endif

	void setupVADParams();

	void enforceTether();

	void notifyParticipantObservers();
	void notifyStatusObservers(LLVoiceClientStatusObserver::EStatusType status);

	bool channelFromRegion(LLViewerRegion* region, std::string& name);

	// This tries and kills the SLVoice daemon.
	void killDaemon();

	static void idle(void* user_data);

	static std::string nameFromAvatar(LLVOAvatar* avatar);
	static std::string nameFromID(const LLUUID& id);
	static bool IDFromName(const std::string name, LLUUID& uuid);
	static std::string displayNameFromAvatar(LLVOAvatar* avatar);

	// Returns the name portion of the SIP URI if the string looks vaguely
	// like a SIP URI, or an empty string if not.
	static std::string nameFromsipURI(const std::string& uri);

	static std::string state2string(state new_state);

	static void voiceAccountProvisionCoro(const std::string& url, S32 retries);
	static void parcelVoiceInfoRequestCoro(const std::string& url);

public:
	static bool				sInitDone;

private:
	LLPumpIO*				mPump;
	LLProcessLauncher*		mProcess;

	state					mState;
	// Session state for the current audio session
	sessionState*			mAudioSession;
	// Session state for the audio session we are trying to join
	sessionState*			mNextAudioSession;

	U32						mRetries;

	U32						mLogLevel;

	S32						mLoginRetryCount;

	S32 					mNumberOfAliases;
	U32						mCommandCookie;

	S32						mSpeakerVolume;

	enum
	{
		earLocCamera = 0,		// Ear at camera
		earLocAvatar,			// Ear at avatar
		earLocMixed				// Ear at avatar location/camera direction
	};
	S32						mEarLocation;

	S32						mMicVolume;

	KEY						mPTTKey;

	// State to return to when we leave tuning mode
	state					mTuningExitState;
	F32						mTuningEnergy;
	S32						mTuningMicVolume;
	S32						mTuningSpeakerVolume;

	LLHost					mDaemonHost;
	LLSocket::ptr_t			mSocket;

	std::string				mAccountName;
	std::string				mAccountPassword;
	std::string				mAccountDisplayName;
	std::string				mAccountFirstName;
	std::string				mAccountLastName;

	std::string				mTuningAudioFile;

	std::string				mSpatialSessionURI;
	std::string				mSpatialSessionCredentials;

	// Name of the channel to be looked up
	std::string				mChannelName;

	// These two are used to detect parcel boundary crossings:
	S32						mCurrentParcelLocalID;
	std::string				mCurrentRegionName;

	// Returned by "Create Connector" message
	std::string				mConnectorHandle;
	// Returned by login message
	std::string				mAccountHandle;

	std::string				mVoiceAccountServerURI;
	std::string				mVoiceSIPURIHostName;

	std::string				mCaptureDevice;
	std::string				mRenderDevice;

	// Active sessions, indexed by session handle. Sessions which are being
	// initiated may not be in this map.
	typedef std::map<const std::string*, sessionState*,
					 stringMapComparator> session_map_t;
	session_map_t			mSessionsByHandle;

	device_list_t			mCaptureDevices;
	device_list_t			mRenderDevices;

	std::string				mWriteString;

	LLVector3d				mCameraPosition;
	LLVector3d				mCameraRequestedPosition;
	LLVector3				mCameraVelocity;
	LLMatrix3				mCameraRot;

	LLVector3d				mAvatarPosition;
	LLVector3				mAvatarVelocity;
	LLMatrix3				mAvatarRot;

	LLTimer					mUpdateTimer;

	// All sessions, not indexed. This is the canonical session list.
	typedef std::set<sessionState*> session_set_t;
	typedef session_set_t::iterator session_set_it_t;
	session_set_t			mSessions;

	typedef std::set<LLVoiceClientStatusObserver*> status_observer_set_t;
	status_observer_set_t	mStatusObservers;

	bool					mVoiceEnabled;
	bool					mAccountLoggedIn;
	bool					mConnectorEstablished;
#if LL_LINUX
	// When true, denotes the use of the deprecated native Linux client
	bool					mDeprecatedClient;
#endif

	bool					mConnected;
	bool					mSessionTerminateRequested;
	bool					mRelogRequested;

	bool					mCaptureDeviceDirty;
	bool					mRenderDeviceDirty;

	bool					mTuningMode;
	bool					mTuningMicVolumeDirty;
	bool					mTuningSpeakerVolumeDirty;

	bool					mSpatialCoordsDirty;
	bool					mSpeakerVolumeDirty;
	bool					mSpeakerMuteDirty;
	bool					mMicVolumeDirty;

	bool					mUsePTT;
	bool					mPTTIsMiddleMouse;
	bool					mPTTIsToggle;
	bool					mUserPTTState;
	bool					mPTTDirty;
	bool					mPTT;
	bool					mMuteMic;

	bool					mLipSyncEnabled;
};

extern LLVoiceClient gVoiceClient;

#endif //LL_VOICE_CLIENT_H
