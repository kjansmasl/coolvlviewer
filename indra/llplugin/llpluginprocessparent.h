/**
 * @file llpluginprocessparent.h
 * @brief LLPluginProcessParent handles the parent side of the external-process plugin API.
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

#ifndef LL_LLPLUGINPROCESSPARENT_H
#define LL_LLPLUGINPROCESSPARENT_H

#include <memory>
#include <queue>

#include "llapr.h"
#include "llevents.h"
#include "llpreprocessor.h"
#include "llprocesslauncher.h"
#include "llpluginmessage.h"
#include "llpluginmessagepipe.h"
#include "llpluginsharedmemory.h"

#include "lliosocket.h"
#include "llthread.h"
#include "lltimer.h"

class LLPluginProcessParentOwner
:	public std::enable_shared_from_this<LLPluginProcessParentOwner>
{
public:
	virtual ~LLPluginProcessParentOwner()		{}

	virtual void receivePluginMessage(const LLPluginMessage& message) = 0;

	LL_INLINE virtual bool receivePluginMessageEarly(const LLPluginMessage& msg)
	{
		return false;
	}

	// This will only be called when the plugin has died unexpectedly
	virtual void pluginLaunchFailed()			{}
	virtual void pluginDied()					{}
};

class LLPluginProcessParent final : public LLPluginMessagePipeOwner
{
protected:
	LOG_CLASS(LLPluginProcessParent);

	LLPluginProcessParent(LLPluginProcessParentOwner* owner);

public:
	~LLPluginProcessParent() override;

	typedef std::shared_ptr<LLPluginProcessParent> ptr_t;

	void init(const std::string& launcher_filename,
			  const std::string& plugin_dir,
			  const std::string& plugin_filename, bool debug);

	// Launches the process. Returns true is successful.
	bool createPluginProcess();

	void idle();

	// Returns true if the plugin is on its way to steady state
	LL_INLINE bool isLoading()					{ return mState <= STATE_LOADING; }

	// Returns true if the plugin is in the steady state (processing messages)
	LL_INLINE bool isRunning()					{ return mState == STATE_RUNNING; }


	// Returns true if the process has exited or we've had a fatal error
	LL_INLINE bool isDone()						{ return mState == STATE_DONE; }

	// Returns true if the process is currently waiting on a blocking request
	LL_INLINE bool isBlocked()					{ return mBlocked; }

	void killSockets();

	// Goes to the proper error state
	void errorState();

	void setSleepTime(F64 sleep_time, bool force_send = false);
	LL_INLINE F64 getSleepTime() const			{ return mSleepTime; }

	void sendMessage(const LLPluginMessage& message);

	void receiveMessage(const LLPluginMessage& message);

	// Inherited from LLPluginMessagePipeOwner
	void setMessagePipe(LLPluginMessagePipe* message_pipe) override;
	void receiveMessageRaw(const std::string& message) override;
	void receiveMessageEarly(const LLPluginMessage& message);

	// This adds a memory segment shared with the client, generating a name for
	// the segment. The name generated is guaranteed to be unique on the host.
	// The caller must call removeSharedMemory first (and wait until
	// getSharedMemorySize returns 0 for the indicated name) before re-adding a
	// segment with the same name.
	std::string addSharedMemory(size_t size);

	// Negotiates for the removal of a shared memory segment. It is the
	// caller's responsibility to ensure that nothing touches the memory after
	// this has been called, since the segment will be unmapped shortly
	// thereafter.
	void removeSharedMemory(const std::string& name);
	size_t getSharedMemorySize(const std::string& name);
	void* getSharedMemoryAddress(const std::string& name);

	// Returns the version string the plugin indicated for the message class,
	// or an empty string if that class wasn't in the list.
	std::string getMessageClassVersion(const std::string& message_class);

	LL_INLINE std::string getPluginVersion()	{ return mPluginVersionString; }

	LL_INLINE bool getDisableTimeout()			{ return mDisableTimeout; }
	LL_INLINE void setDisableTimeout(bool b)	{ mDisableTimeout = b; }

	LL_INLINE void setLaunchTimeout(F32 t)		{ mPluginLaunchTimeout = t; }
	LL_INLINE void setLockupTimeout(F32 t)		{ mPluginLockupTimeout = t; }

	LL_INLINE F64 getCPUUsage()					{ return mCPUUsage; }

	void requestShutdown();

	static ptr_t create(LLPluginProcessParentOwner* owner);
	static void shutdown();

	static bool poll(F64 timeout);
	LL_INLINE static bool canPollThreadRun()
	{
		return sPollSet || sPollsetNeedsRebuild || sUseReadThread;
	}

	static void setUseReadThread(bool use_read_thread);
	LL_INLINE static bool getUseReadThread()	{ return sUseReadThread; }

	LL_INLINE static const std::string& getMediaBrowserVersion()
	{
		return sMediaBrowserVersion;
	}

private:
	enum EState
	{
		STATE_UNINITIALIZED,
		STATE_INITIALIZED,		// init() has been called
		STATE_LISTENING,		// listening for incoming connection
		STATE_LAUNCHED,			// process has been launched
		STATE_CONNECTED,		// process has connected
		STATE_HELLO,			// first message from the plugin process has been received
		STATE_LOADING,			// process has been asked to load the plugin
		STATE_RUNNING,
		STATE_GOODBYE,
		STATE_LAUNCH_FAILURE,	// Failure before plugin loaded
		STATE_ERROR,			// generic bailout state
		STATE_CLEANUP,			// clean everything up
		STATE_EXITING,			// Tried to kill process, waiting for it to exit
		STATE_DONE

	};
	void setState(EState state);

	bool wantsPolling() const;
	void removeFromProcessing();

	bool pluginLockedUp();
	bool pluginLockedUpOrQuit();

	bool accept();

	void clearProcessCreationThread();

	void servicePoll();
	bool pollTick();

	static void dirtyPollSet();
	static void updatePollset();

private:
	LLThread*					mProcessCreationThread;
	LLSocket::ptr_t				mListenSocket;
	LLSocket::ptr_t				mSocket;
	U32							mBoundPort;

	LLProcessLauncher			mProcess;
	LLProcessLauncher			mDebugger;

	LLPluginProcessParentOwner*	mOwner;

	LLTimer						mHeartbeat;
	F64							mSleepTime;
	F64							mCPUUsage;

	EState						mState;

	// Somewhat longer timeout for initial launch:
	F32							mPluginLaunchTimeout;
	// If we do not receive a heartbeat in this many seconds, we declare the
	// plugin locked up:
	F32							mPluginLockupTimeout;

	apr_pollfd_t				mPollFD;

	LLMutex						mIncomingQueueMutex;
	std::queue<LLPluginMessage>	mIncomingQueue;

	typedef std::map<std::string, LLPluginSharedMemory*> shared_mem_regions_t;
	shared_mem_regions_t		mSharedMemoryRegions;

	LLTempBoundListener			mPolling;

	LLSD						mMessageClassVersions;

	std::string					mPluginVersionString;
	std::string					mPluginFile;
	std::string					mPluginDir;

	bool						mProcessStarted;
	bool						mDisableTimeout;
	bool						mBlocked;
	bool						mPolledInput;
	bool						mDebug;

	static std::string			sMediaBrowserVersion;
	static apr_pollset_t*		sPollSet;
	static LLMutex				sInstancesMutex;
	typedef std::map<void*, ptr_t> instances_map_t;
	static instances_map_t		sInstances;
	static LLThread*			sReadThread;
	static bool					sUseReadThread;
	static bool					sPollsetNeedsRebuild;
};

#endif // LL_LLPLUGINPROCESSPARENT_H
