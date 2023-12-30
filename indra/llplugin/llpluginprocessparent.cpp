/**
 * @file llpluginprocessparent.cpp
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

#include "linden_common.h"

#include "llpluginprocessparent.h"

#include "llapp.h"
#include "llapr.h"
#include "llpluginmessagepipe.h"
#include "llpluginmessageclasses.h"
#include "llsdserialize.h"

std::string LLPluginProcessParent::sMediaBrowserVersion;
apr_pollset_t* LLPluginProcessParent::sPollSet = NULL;
bool LLPluginProcessParent::sPollsetNeedsRebuild = false;
bool LLPluginProcessParent::sUseReadThread = false;
LLThread* LLPluginProcessParent::sReadThread = NULL;
LLMutex LLPluginProcessParent::sInstancesMutex;
LLPluginProcessParent::instances_map_t LLPluginProcessParent::sInstances;

class LLPluginProcessParentPollThread final : public LLThread
{
public:
	LLPluginProcessParentPollThread()
	:	LLThread("LLPluginProcessParentPollThread")
	{
	}

protected:
	// Inherited from LLThread
	void run() override
	{
		while (!isQuitting() && LLPluginProcessParent::getUseReadThread())
		{
			bool active = LLPluginProcessParent::poll(0.1f);
			checkPause();
			ms_sleep(active ? 1 : 10); // Do not eat-up a full CPU core !!!
		}

		// Final poll to clean up the pollset, etc.
		LLPluginProcessParent::poll(0.f);
	}

	// Inherited from LLThread
	LL_INLINE bool runCondition() override
	{
		return LLPluginProcessParent::canPollThreadRun();
	}
};

class LLPluginProcessCreationThread final : public LLThread
{
public:
	LLPluginProcessCreationThread(LLPluginProcessParent* parent)
	:	LLThread("LLPluginProcessCreationThread"),
		mParent(parent)
	{
	}

protected:
	// Inherited from LLThread
	LL_INLINE void run() override
	{
		mParent->createPluginProcess();
	}

private:
	LLPluginProcessParent*	mParent;
};

//static
LLPluginProcessParent::ptr_t LLPluginProcessParent::create(LLPluginProcessParentOwner* owner)
{
	ptr_t self(new LLPluginProcessParent(owner));
	// Do not add to the global list until fully constructed.
	sInstancesMutex.lock();
	sInstances.emplace(self.get(), self);
	sInstancesMutex.unlock();
	return self;
}

//static
void LLPluginProcessParent::shutdown()
{
	sInstancesMutex.lock();
	if (!sInstances.empty())
	{
		for (instances_map_t::iterator it = sInstances.begin();
			 it != sInstances.end(); ++it)
		{
			if (it->second->mState < STATE_GOODBYE)
			{
				it->second->requestShutdown();
			}
		}
		sInstances.clear();
	}
	sInstancesMutex.unlock();
}

LLPluginProcessParent::LLPluginProcessParent(LLPluginProcessParentOwner* owner)
:	mProcessCreationThread(NULL),
	mProcessStarted(false)
{
	mOwner = owner;
	mBoundPort = 0;
	mState = STATE_UNINITIALIZED;
	mSleepTime = mCPUUsage = 0.0;
	mDisableTimeout = mPolledInput = mBlocked = mDebug = false;
	mPollFD.client_data = NULL;

	mPluginLaunchTimeout = 60.f;
	mPluginLockupTimeout = 15.f;

	// Do not start the timer here: start it when we actually launch the plugin
	// process.
	mHeartbeat.stop();
}

LLPluginProcessParent::~LLPluginProcessParent()
{
	LL_DEBUGS("Plugin") << "Destructor called" << LL_ENDL;

	if (mProcessCreationThread)
	{
		if (!mProcessCreationThread->isStopped())
		{
			llwarns << "Shutting down active process creation thread" << llendl;
			mProcessCreationThread->shutdown();
			ms_sleep(20);
		}
		delete mProcessCreationThread;
		mProcessCreationThread = NULL;
	}

	// Destroy any remaining shared memory regions
	shared_mem_regions_t::iterator iter;
	while ((iter = mSharedMemoryRegions.begin()) != mSharedMemoryRegions.end())
	{
		// Destroy the shared memory region
		iter->second->destroy();
		delete iter->second;
		iter->second = NULL;

		// And remove it from our map
		mSharedMemoryRegions.erase(iter);
	}

	mProcess.kill();
	if (!LLApp::isQuitting())
	{
		// If we are quitting, the sockets will already have been destroyed.
		killSockets();
	}

	if (mPolling.connected())
	{
		mPolling.disconnect();
	}
}

void LLPluginProcessParent::requestShutdown()
{
	setState(STATE_GOODBYE);
	mOwner = NULL;

	if (LLApp::isError())
	{
		if (mPolling.connected())
		{
			mPolling.disconnect();
		}
		// If we are crashing, run the idle once more since there will be no
		// polling.
		idle();
		removeFromProcessing();
		return;
	}

	// *HACK: after this method has been called, our previous owner will no
	// longer call our idle() method. Tie into the main event loop here to do
	// that until we are good and finished.

	static S32 count = 0;
	std::string name = llformat("LLPluginProcessParentListener%d", ++count);
	LL_DEBUGS("Plugin") << "Listening on 'mainloop' for: " << name << LL_ENDL;

	LLEventPump& pump = gEventPumps.obtain("mainloop");
	mPolling = pump.listen(name,
						   boost::bind(&LLPluginProcessParent::pollTick,
									   this));
}

bool LLPluginProcessParent::pollTick()
{
	if (mState != STATE_DONE)
	{
		idle();
		return false;
	}

	// This grabs a copy of the smart pointer to ourselves to ensure that we do
	// not get destroyed until after this method returns.
	ptr_t self;
	sInstancesMutex.lock();
	instances_map_t::iterator it = sInstances.find(this);
	if (it != sInstances.end())
	{
		self = it->second;
	}
	sInstancesMutex.unlock();

	removeFromProcessing();
	return true;
}

// Removes our instance from the global list before beginning destruction.
void LLPluginProcessParent::removeFromProcessing()
{
	// Make sure to get the global mutex _first_ here, to avoid a possible
	// deadlock against LLPluginProcessParent::poll()
	sInstancesMutex.lock();
	{
		mIncomingQueueMutex.lock();
		sInstances.erase(this);
		mIncomingQueueMutex.unlock();
	}
	sInstancesMutex.unlock();
}

bool LLPluginProcessParent::wantsPolling() const
{
	return mState != STATE_DONE && mPollFD.client_data;
}

void LLPluginProcessParent::killSockets()
{
	mIncomingQueueMutex.lock();
	killMessagePipe();
	mIncomingQueueMutex.unlock();

	mListenSocket.reset();
	mSocket.reset();
}

void LLPluginProcessParent::errorState()
{
	if (mState < STATE_RUNNING)
	{
		setState(STATE_LAUNCH_FAILURE);
	}
	else
	{
		setState(STATE_ERROR);
	}
}

void LLPluginProcessParent::init(const std::string& launcher_filename,
								 const std::string& plugin_dir,
								 const std::string& plugin_filename,
								 bool debug)
{
	mProcess.setExecutable(launcher_filename);
	mProcess.setWorkingDirectory(plugin_dir);
	mPluginFile = plugin_filename;
	mPluginDir = plugin_dir;
	mCPUUsage = 0.f;
	mDebug = debug;
	setState(STATE_INITIALIZED);
}

bool LLPluginProcessParent::accept()
{
	if (!mListenSocket)
	{
		return false;
	}

	apr_socket_t* new_socket = NULL;
	apr_status_t status = apr_socket_accept(&new_socket,
											mListenSocket->getSocket(),
											gAPRPoolp);
	if (status == APR_SUCCESS)
	{
		LL_DEBUGS("Plugin") << "APR SUCCESS" << LL_ENDL;
		// Success. Create a message pipe on the new socket

		// We MUST create a new pool for the LLSocket, since it will take
		// ownership of it and delete it in its destructor!
		apr_pool_t* new_pool = NULL;
		status = apr_pool_create(&new_pool, gAPRPoolp);

		mSocket = LLSocket::create(new_socket, new_pool);
		new LLPluginMessagePipe(this, mSocket);

		return true;
	}
	if (APR_STATUS_IS_EAGAIN(status))
	{
		LL_DEBUGS("Plugin") << "APR EAGAIN" << LL_ENDL;
		// No incoming connections.  This is not an error.
		status = APR_SUCCESS;
	}
	else
	{
		LL_DEBUGS("Plugin") << "APR Error:" << llendl;
		ll_apr_warn_status(status);

		// Some other error.
		errorState();
	}
	return false;
}

bool LLPluginProcessParent::createPluginProcess()
{
	if (!mProcessStarted)
	{
		// Only argument to the launcher is the port number we are listening
		// on
		std::stringstream stream;
		stream << mBoundPort;
		mProcess.addArgument(stream.str());

		mProcessStarted = mProcess.launch() == 0;
	}
	return mProcessStarted;
}

void LLPluginProcessParent::clearProcessCreationThread()
{
	if (mProcessCreationThread)
	{
		if (mProcessCreationThread->isStopped())
		{
			delete mProcessCreationThread;
			mProcessCreationThread = NULL;
		}
		else
		{
			mProcessCreationThread->shutdown();
		}
	}
}

void LLPluginProcessParent::idle()
{
	bool idle_again;
	do
	{
		// Process queued messages
		// Inside main thread, it is preferable not to block it on mutex.
		bool locked = mIncomingQueueMutex.trylock();
		while (locked && !mIncomingQueue.empty())
		{
			LLPluginMessage message = mIncomingQueue.front();
			mIncomingQueue.pop();
			mIncomingQueueMutex.unlock();

			receiveMessage(message);

			locked = mIncomingQueueMutex.trylock();
		}
		if (locked)
		{
			mIncomingQueueMutex.unlock();
		}

		// Give time to network processing
		if (mMessagePipe)
		{
			// Drain any queued outgoing messages
			mMessagePipe->pumpOutput();

			// Only do input processing here if this instance is not in a
			// pollset. Also, if we are shutting down the plugin (STATE_GOODBYE
			// or later) or the viewer, we cannot handle the pumping.
			if (!mPolledInput && mState < STATE_GOODBYE && !LLApp::isExiting())
			{
				mMessagePipe->pumpInput();
			}
		}

		if (mState <= STATE_RUNNING)
		{
			if (APR_STATUS_IS_EOF(mSocketError))
			{
				// Plugin socket was closed. This covers both normal plugin
				// termination and plugin crashes.
				errorState();
			}
			else if (mSocketError != APR_SUCCESS)
			{
				// The socket is in an error state -- the plugin is gone.
				llwarns << "Socket hit an error state (" << mSocketError << ")"
						<< llendl;
				errorState();
			}
		}

		// If a state needs to go directly to another state (as a performance
		// enhancement), it can set idle_again to true after calling
		// setState().
		// USE THIS CAREFULLY, since it can starve other code. Specifically
		// make sure there is no way to get into a closed cycle and never
		// return. When in doubt, do not do it.
		idle_again = false;
		switch (mState)
		{
			case STATE_UNINITIALIZED:
				break;

			case STATE_INITIALIZED:
			{
				apr_status_t status = APR_SUCCESS;
				apr_sockaddr_t* addr = NULL;
				mListenSocket = LLSocket::create(gAPRPoolp,
												 LLSocket::STREAM_TCP);
				mBoundPort = 0;
				if (!mListenSocket)
				{
					killSockets();
					errorState();
					break;
				}

				// This code is based on parts of LLSocket::create() in
				// lliosocket.cpp.

				status = apr_sockaddr_info_get(&addr, "127.0.0.1", APR_INET,
											   // port 0 = ephemeral
											   // ("find me a port")
											   0, 0, gAPRPoolp);
				if (ll_apr_warn_status(status))
				{
					killSockets();
					errorState();
					break;
				}

				// This allows us to reuse the address on quick down/up. This
				// is unlikely to create problems.
				ll_apr_warn_status(apr_socket_opt_set(mListenSocket->getSocket(),
													  APR_SO_REUSEADDR, 1));

				status = apr_socket_bind(mListenSocket->getSocket(), addr);
				if (ll_apr_warn_status(status))
				{
					killSockets();
					errorState();
					break;
				}

				// Get the actual port the socket was bound to
				{
					apr_sockaddr_t* bound_addr = NULL;
					if (ll_apr_warn_status(apr_socket_addr_get(&bound_addr,
															   APR_LOCAL,
															   mListenSocket->getSocket())))
					{
						killSockets();
						errorState();
						break;
					}
					mBoundPort = bound_addr->port;

					if (mBoundPort == 0)
					{
						llwarns << "Bound port number unknown, bailing out."
								<< llendl;

						killSockets();
						errorState();
						break;
					}
				}

				LL_DEBUGS("Plugin") << "Bound tcp socket to port: "
									<< addr->port << LL_ENDL;

				// Make the listen socket non-blocking
				status = apr_socket_opt_set(mListenSocket->getSocket(),
											APR_SO_NONBLOCK, 1);
				if (ll_apr_warn_status(status))
				{
					killSockets();
					errorState();
					break;
				}

				apr_socket_timeout_set(mListenSocket->getSocket(), 0);
				if (ll_apr_warn_status(status))
				{
					killSockets();
					errorState();
					break;
				}

				// If it is a stream based socket, we need to tell the OS to
				// keep a queue of incoming connections for ACCEPT.
				// FIXME: 10 is a magic number for queue size...
				status = apr_socket_listen(mListenSocket->getSocket(), 10);
				if (ll_apr_warn_status(status))
				{
					killSockets();
					errorState();
					break;
				}

				// If we got here, we are listening.
				setState(STATE_LISTENING);
				break;
			}

			case STATE_LISTENING:
			{
				// Launch the plugin process.
				if (mDebug && !mProcessCreationThread)
				{
					if (!createPluginProcess())
					{
						errorState();
					}
				}
				else if (!mProcessCreationThread)
				{
					mProcessCreationThread =
						new LLPluginProcessCreationThread(this);
					mProcessCreationThread->start();
				}
				else if (!mProcessStarted &&
						 mProcessCreationThread->isStopped())
				{
					delete mProcessCreationThread;
					mProcessCreationThread = NULL;
					errorState();
				}

				if (mProcessStarted)
				{
#if LL_DARWIN
					if (mDebug)
					{
						// If we are set to debug, start up a gdb instance in a
						// new terminal window and have it attach to the plugin
						// process and continue.

						// The command we are constructing would look like this
						// on the command line:
						// osascript -e 'tell application "Terminal"' -e 'set win to do script "gdb -pid 12345"' -e 'do script "continue" in win' -e 'end tell'

						std::stringstream cmd;

						mDebugger.setExecutable("/usr/bin/osascript");
						mDebugger.addArgument("-e");
						mDebugger.addArgument("tell application \"Terminal\"");
						mDebugger.addArgument("-e");
						cmd << "set win to do script \"gdb -pid "
							<< mProcess.getProcessID() << "\"";
						mDebugger.addArgument(cmd.str());
						mDebugger.addArgument("-e");
						mDebugger.addArgument("do script \"continue\" in win");
						mDebugger.addArgument("-e");
						mDebugger.addArgument("end tell");
						mDebugger.launch();
					}
#endif
					// This will allow us to time out if the process never
					// starts.
					mHeartbeat.start();
					mHeartbeat.setTimerExpirySec(mPluginLaunchTimeout);
					setState(STATE_LAUNCHED);
				}
				break;
			}

			case STATE_LAUNCHED:
			{
				// Waiting for the plugin to connect
				if (pluginLockedUpOrQuit())
				{
					errorState();
				}
				// Check for the incoming connection.
				else if (accept())
				{
					// Stop listening on the server port
					mListenSocket.reset();
					setState(STATE_CONNECTED);
				}
				break;
			}

			case STATE_CONNECTED:
			{
				// Waiting for hello message from the plugin
				if (pluginLockedUpOrQuit())
				{
					errorState();
				}
				break;
			}

			case STATE_HELLO:
			{
				LL_DEBUGS("Plugin") << "Received hello message" << LL_ENDL;

				// Send the message to load the plugin
				{
					LLPluginMessage message(LLPLUGIN_MESSAGE_CLASS_INTERNAL,
											"load_plugin");
					message.setValue("file", mPluginFile);
					message.setValue("dir", mPluginDir);
					sendMessage(message);
				}

				setState(STATE_LOADING);
				break;
			}

			case STATE_LOADING:
			{
				// The load_plugin_response message will kick us from here into
				// STATE_RUNNING
				if (pluginLockedUpOrQuit())
				{
					errorState();
				}
				break;
			}

			case STATE_RUNNING:
			{
				if (pluginLockedUpOrQuit())
				{
					errorState();
				}
				break;
			}

			case STATE_GOODBYE:
			{
				{
					LLPluginMessage message(LLPLUGIN_MESSAGE_CLASS_INTERNAL,
											"shutdown_plugin");
					sendMessage(message);
				}
				setState(STATE_EXITING);
				break;
			}

			case STATE_EXITING:
			{
				if (!mProcess.isRunning())
				{
					setState(STATE_CLEANUP);
				}
				else if (pluginLockedUp())
				{
					llwarns << "Timeout in exiting state, bailing out"
							<< llendl;
					errorState();
				}
				break;
			}

			case STATE_LAUNCH_FAILURE:
			{
				if (mOwner)
				{
					mOwner->pluginLaunchFailed();
				}
				setState(STATE_CLEANUP);
				break;
			}

			case STATE_ERROR:
			{
				if (mOwner)
				{
					mOwner->pluginDied();
				}
				setState(STATE_CLEANUP);
				break;
			}

			case STATE_CLEANUP:
			{
				mProcess.kill();
				killSockets();
				setState(STATE_DONE);
				dirtyPollSet();
				clearProcessCreationThread();
				break;
			}

			case STATE_DONE:
				// Just sit here.
				break;
		}

	}
	while (idle_again);
}

void LLPluginProcessParent::setSleepTime(F64 sleep_time, bool force_send)
{
	if (force_send || sleep_time != mSleepTime)
	{
		// Cache the time locally
		mSleepTime = sleep_time;

		if (canSendMessage())
		{
			// And send to the plugin.
			LLPluginMessage message(LLPLUGIN_MESSAGE_CLASS_INTERNAL,
									"sleep_time");
			message.setValueReal("time", mSleepTime);
			sendMessage(message);
		}
#if 0
		else
		{
			// Too early to send -- the load_plugin_response message will
			// trigger us to send mSleepTime later.
		}
#endif
	}
}

void LLPluginProcessParent::sendMessage(const LLPluginMessage& message)
{
	if (message.hasValue("blocking_response"))
	{
		mBlocked = false;

		// reset the heartbeat timer, since there will have been no heartbeats
		// while the plugin was blocked.
		mHeartbeat.setTimerExpirySec(mPluginLockupTimeout);
	}

	std::string buffer = message.generate();
	LL_DEBUGS("Plugin") << "Sending: " << buffer << LL_ENDL;
	writeMessageRaw(buffer);

	// Try to send message immediately.
	if (mMessagePipe)
	{
		mMessagePipe->pumpOutput();
	}
}

//virtual
void LLPluginProcessParent::setMessagePipe(LLPluginMessagePipe* message_pipe)
{
	bool update_pollset = false;

	if (mMessagePipe)
	{
		// Unsetting an existing message pipe -- remove from the pollset
		mPollFD.client_data = NULL;

		// Poll set needs an update
		update_pollset = true;
	}
	if (message_pipe != NULL)
	{
		// Set up the apr_pollfd_t
		mPollFD.p = gAPRPoolp;
		mPollFD.desc_type = APR_POLL_SOCKET;
		mPollFD.reqevents = APR_POLLIN|APR_POLLERR|APR_POLLHUP;
		mPollFD.rtnevents = 0;
		mPollFD.desc.s = mSocket->getSocket();
		mPollFD.client_data = (void*)this;

		// Poll set needs an update
		update_pollset = true;
	}

	mMessagePipe = message_pipe;

	if (update_pollset)
	{
		dirtyPollSet();
	}
}

//static
void LLPluginProcessParent::dirtyPollSet()
{
	sPollsetNeedsRebuild = true;

	if (sReadThread)
	{
		LL_DEBUGS("PluginPoll") << "Unpausing read thread " << LL_ENDL;
		sReadThread->unpause();
	}
}

void LLPluginProcessParent::updatePollset()
{
	sInstancesMutex.lock();
	if (sInstances.empty())
	{
		// No instance, so there is no work to do.
		sInstancesMutex.unlock();
		return;
	}

	if (sPollSet)
	{
		LL_DEBUGS("PluginPoll") << "Destroying pollset " << sPollSet
								<< LL_ENDL;
		// Delete the existing pollset.
		apr_pollset_destroy(sPollSet);
		sPollSet = NULL;
	}

	// Count the number of instances that want to be in the pollset
	S32 count = 0;
	for (instances_map_t::iterator iter = sInstances.begin(),
								   end = sInstances.end();
		 iter != end; ++iter)
	{
		iter->second->mPolledInput = false;
		if (iter->second->wantsPolling())
		{
			// This instance has a socket that needs to be polled.
			++count;
		}
	}

	if (sUseReadThread && sReadThread && !sReadThread->isQuitting())
	{
		if (!sPollSet && count > 0)
		{
#ifdef APR_POLLSET_NOCOPY
			// The pollset does not exist yet. Create it now.
			apr_status_t status = apr_pollset_create(&sPollSet, count,
													 gAPRPoolp,
													 APR_POLLSET_NOCOPY);
			if (status != APR_SUCCESS)
			{
#endif // APR_POLLSET_NOCOPY
				llwarns << "Could not create pollset. Falling back to non-pollset mode."
						<< llendl;
				sPollSet = NULL;
#ifdef APR_POLLSET_NOCOPY
			}
			else
			{
				LL_DEBUGS("PluginPoll") << "Created pollset " << sPollSet
										<< LL_ENDL;

				// Pollset was created, add all instances to it.
				for (instances_map_t::iterator iter = sInstances.begin(),
											   end = sInstances.end();
					 iter != end; ++iter)
				{
					if (!iter->second->wantsPolling())
					{
						continue;
					}
					status = apr_pollset_add(sPollSet,
											 &(iter->second->mPollFD));
					if (status == APR_SUCCESS)
					{
						iter->second->mPolledInput = true;
					}
					else
					{
						llwarns << "apr_pollset_add failed with status "
								<< status << llendl;
					}
				}
			}
#endif // APR_POLLSET_NOCOPY
		}
	}

	sInstancesMutex.unlock();
}

void LLPluginProcessParent::setUseReadThread(bool use_read_thread)
{
	if (sUseReadThread != use_read_thread)
	{
		sUseReadThread = use_read_thread;
		if (sUseReadThread)
		{
			if (!sReadThread)
			{
				// Start up the read thread
				llinfos << "Creating read thread" << llendl;

				// Make sure the pollset gets rebuilt.
				sPollsetNeedsRebuild = true;

				sReadThread = new LLPluginProcessParentPollThread;
				sReadThread->start();
			}
		}
		else if (sReadThread)
		{
			// Shut down the read thread
			llinfos << "Destroying read thread" << llendl;
			delete sReadThread;
			sReadThread = NULL;
		}
	}
}

//static
bool LLPluginProcessParent::poll(F64 timeout)
{
	sInstancesMutex.lock();
	if (sInstances.empty())
	{
		// No instance, so there is no work to do.
		sInstancesMutex.unlock();
		return false;
	}
	sInstancesMutex.unlock();

	bool active = false;

	if (sPollsetNeedsRebuild || !sUseReadThread)
	{
		sPollsetNeedsRebuild = false;
		updatePollset();
	}

	if (sPollSet)
	{
		apr_int32_t count;
		const apr_pollfd_t* descriptors;
		apr_status_t status =
			apr_pollset_poll(sPollSet,
							 (apr_interval_time_t)(timeout * 1000000),
							 &count, &descriptors);
		if (status == APR_SUCCESS)
		{
			// One or more of the descriptors signalled. Call them.
			for (S32 i = 0; i < count; ++i)
			{
				ptr_t self;
				sInstancesMutex.lock();
				instances_map_t::iterator it =
					sInstances.find((void*)descriptors[i].client_data);
				if (it != sInstances.end())
				{
					self = it->second;
				}
				sInstancesMutex.unlock();

				if (self)
				{
					self->mIncomingQueueMutex.lock();
					self->servicePoll();
					self->mIncomingQueueMutex.unlock();
				}
			}
			active = true;	// Plugin is active
		}
		else if (APR_STATUS_IS_TIMEUP(status))
		{
			// Timed out with no incoming data. Just return.
		}
		else if (APR_STATUS_IS_EBADF(status))
		{
			// This happens when one of the file descriptors in the pollset is
			// destroyed, which happens whenever a plugin's socket is closed.
			// The pollset has been or will be recreated, so just return.
			LL_DEBUGS("PluginPoll") << "apr_pollset_poll returned EBADF"
									<< LL_ENDL;
		}
		else if (status != APR_SUCCESS)
		{
			llwarns << "apr_pollset_poll failed with status " << status
					<< llendl;
		}
	}

	// Remove instances in the done state from the sInstances map.
	sInstancesMutex.lock();
	instances_map_t::iterator it = sInstances.begin();
	while (it != sInstances.end())
	{
		if (it->second->isDone())
		{
			 it = sInstances.erase(it);
		}
		else
		{
			++it;
		}
	}
	sInstancesMutex.unlock();

	return active;
}

void LLPluginProcessParent::servicePoll()
{
	bool result = true;

	// Poll signalled on this object's socket. Try to process incoming
	// messages.
	if (mMessagePipe)
	{
		result = mMessagePipe->pumpInput(0.f);
	}

	if (!result)
	{
		// If we got a read error on input, remove this pipe from the pollset
		apr_pollset_remove(sPollSet, &mPollFD);

		// and tell the code not to re-add it
		mPollFD.client_data = NULL;
	}
}

void LLPluginProcessParent::receiveMessageRaw(const std::string& message)
{
	LL_DEBUGS("Plugin") << "Received: " << message << LL_ENDL;

	LLPluginMessage parsed;
	if (parsed.parse(message) != LLSDParser::PARSE_FAILURE)
	{
		if (parsed.hasValue("blocking_request"))
		{
			mBlocked = true;
		}

		if (mPolledInput)
		{
			// This is being called on the polling thread -- only do minimal
			// processing/queueing.
			receiveMessageEarly(parsed);
		}
		else
		{
			// This is not being called on the polling thread -- do full
			// message processing at this time.
			receiveMessage(parsed);
		}
	}
}

void LLPluginProcessParent::receiveMessageEarly(const LLPluginMessage& message)
{
	// NOTE: this function will be called from the polling thread. It will be
	// called with mIncomingQueueMutex _already locked_.

	bool handled = false;

	std::string message_class = message.getClass();
	// No internal messages need to be handled early.
	if (message_class != LLPLUGIN_MESSAGE_CLASS_INTERNAL)
	{
		// Call out to the owner and see if they to reply
		// *TODO: Should this only happen when blocked ?
		if (mOwner)
		{
			handled = mOwner->receivePluginMessageEarly(message);
		}
	}

	if (!handled)
	{
		// Any message that was not handled early needs to be queued.
		mIncomingQueue.emplace(message);
	}
}

void LLPluginProcessParent::receiveMessage(const LLPluginMessage& message)
{
	std::string message_class = message.getClass();
	if (message_class == LLPLUGIN_MESSAGE_CLASS_INTERNAL)
	{
		// Internal messages should be handled here
		std::string message_name = message.getName();
		if (message_name == "hello")
		{
			if (mState == STATE_CONNECTED)
			{
				// Plugin host has launched. Tell it which plugin to load.
				setState(STATE_HELLO);
			}
			else
			{
				llwarns << "Received hello message in wrong state: bailing out."
						<< llendl;
				errorState();
			}
		}
		else if (message_name == "load_plugin_response")
		{
			if (mState == STATE_LOADING)
			{
				// Plugin has been loaded.

				mPluginVersionString = message.getValue("plugin_version");
				llinfos << "plugin version string: " << mPluginVersionString
						<< llendl;

				// Check which message classes/versions the plugin supports.
				// *TODO: check against current versions
				// *TODO: kill plugin on major mismatches?
				std::string msg_class;
				mMessageClassVersions = message.getValueLLSD("versions");
				for (LLSD::map_iterator
						iter = mMessageClassVersions.beginMap(),
						end = mMessageClassVersions.endMap();
					 iter != end; ++iter)
				{
					msg_class = iter->first;
					llinfos << "Message class: " << msg_class
							<< " -> version: " << iter->second.asString()
							<< llendl;
					if (msg_class == "media_browser")
					{
						// Remember the media browser version, for reporting
						// it in the About floater.
						sMediaBrowserVersion = mPluginVersionString;
					}
				}

				// Send initial sleep time
				llassert_always(mSleepTime != 0.f);
				setSleepTime(mSleepTime, true);

				setState(STATE_RUNNING);
			}
			else
			{
				llwarns << "Received load_plugin_response message in wrong state: bailing out"
						<< llendl;
				errorState();
			}
		}
		else if (message_name == "heartbeat")
		{
			// This resets our timer.
			mHeartbeat.setTimerExpirySec(mPluginLockupTimeout);

			mCPUUsage = message.getValueReal("cpu_usage");

			LL_DEBUGS("Plugin") << "CPU usage reported as " << mCPUUsage
								<< LL_ENDL;
		}
		else if (message_name == "shm_add_response")
		{
			// Nothing to do here.
		}
		else if (message_name == "shm_remove_response")
		{
			std::string name = message.getValue("name");
			shared_mem_regions_t::iterator iter =
				mSharedMemoryRegions.find(name);
			if (iter != mSharedMemoryRegions.end())
			{
				// Destroy the shared memory region
				iter->second->destroy();
				delete iter->second;
				iter->second = NULL;

				// And remove it from our map
				mSharedMemoryRegions.erase(iter);
			}
		}
		else
		{
			llwarns << "Unknown internal message from child: " << message_name
					<< llendl;
		}
	}
	else if (mOwner)
	{
		mOwner->receivePluginMessage(message);
	}
}

std::string LLPluginProcessParent::addSharedMemory(size_t size)
{
	std::string name;

	LLPluginSharedMemory* region = new LLPluginSharedMemory;

	// This is a new region
	if (region->create(size))
	{
		name = region->getName();

		mSharedMemoryRegions.emplace(name, region);

		LLPluginMessage message(LLPLUGIN_MESSAGE_CLASS_INTERNAL, "shm_add");
		message.setValue("name", name);
		message.setValueS32("size", (S32)size);
		sendMessage(message);
	}
	else
	{
		llwarns << "Could not create a shared memory segment !" << llendl;
		delete region;
	}

	return name;
}

void LLPluginProcessParent::removeSharedMemory(const std::string& name)
{
	shared_mem_regions_t::iterator iter = mSharedMemoryRegions.find(name);
	if (iter != mSharedMemoryRegions.end())
	{
		// This segment exists.  Send the message to the child to unmap it. The
		// response will cause the parent to unmap our end.
		LLPluginMessage message(LLPLUGIN_MESSAGE_CLASS_INTERNAL, "shm_remove");
		message.setValue("name", name);
		sendMessage(message);
	}
	else
	{
		llwarns << "Request to remove an unknown shared memory segment."
				<< llendl;
	}
}

size_t LLPluginProcessParent::getSharedMemorySize(const std::string& name)
{
	shared_mem_regions_t::iterator iter = mSharedMemoryRegions.find(name);
	return iter == mSharedMemoryRegions.end() ? 0 : iter->second->getSize();
}

void* LLPluginProcessParent::getSharedMemoryAddress(const std::string& name)
{
	void* result = NULL;
	shared_mem_regions_t::iterator iter = mSharedMemoryRegions.find(name);
	if (iter != mSharedMemoryRegions.end())
	{
		result = iter->second->getMappedAddress();
	}
	return result;
}

std::string LLPluginProcessParent::getMessageClassVersion(const std::string& mclass)
{
	std::string result;
	if (mMessageClassVersions.has(mclass))
	{
		result = mMessageClassVersions[mclass].asString();
	}
	return result;
}

void LLPluginProcessParent::setState(EState state)
{
	LL_DEBUGS("Plugin") << "Setting state to " << state << LL_ENDL;
	mState = state;
}

bool LLPluginProcessParent::pluginLockedUpOrQuit()
{
	if (!mProcess.isRunning())
	{
		llwarns << "Child exited" << llendl;
		return true;
	}

	if (pluginLockedUp())
	{
		llwarns << "Timeout" << llendl;
		return true;
	}

	return false;
}

bool LLPluginProcessParent::pluginLockedUp()
{
	if (mDisableTimeout || mDebug || mBlocked)
	{
		// Never time out a plugin process in these cases.
		return false;
	}

	// If the timer is running and has expired, the plugin has locked up.
	return mHeartbeat.getStarted() && mHeartbeat.hasExpired();
}
