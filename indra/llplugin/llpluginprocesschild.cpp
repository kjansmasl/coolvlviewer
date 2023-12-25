/**
 * @file llpluginprocesschild.cpp
 * @brief LLPluginProcessChild handles the child side of the external-process plugin API.
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

#include "llpluginprocesschild.h"

#include "llplugininstance.h"
#include "llpluginmessagepipe.h"
#include "llpluginmessageclasses.h"

constexpr F32 HEARTBEAT_SECONDS = 1.f;
// Each call to idle will give the plugin this much time.
constexpr F32 PLUGIN_IDLE_SECONDS = 0.01f;
// Do not set it to be bigger than mPluginLockupTimeout or parent will kill
// LLPluginProcessChild
constexpr F32 GOODBYE_SECONDS = 5.f;

LLPluginProcessChild::LLPluginProcessChild()
:	mInstance(NULL),
	mState(STATE_UNINITIALIZED),
	mSleepTime(PLUGIN_IDLE_SECONDS),	// default: send idle messages at 100Hz
	mCPUElapsed(0.f),
	mBlockingRequest(false),
	mBlockingResponseReceived(false)
{
	mSocket = LLSocket::create(gAPRPoolp, LLSocket::STREAM_TCP);
}

LLPluginProcessChild::~LLPluginProcessChild()
{
	if (mInstance)
	{
		sendMessageToPlugin(LLPluginMessage("base", "cleanup"));

		// IMPORTANT: under some (unknown) circumstances the apr_dso_unload()
		// triggered when mInstance is deleted appears to fail and lock up
		// which means that a given instance of the slplugin process never
		// exits.
		// This is bad, especially when users try to update their version of SL
		// - it fails because the slplugin process as well as a bunch of plugin
		// specific files are locked and cannot be overwritten.
#if 0
		delete mInstance;
		mInstance = NULL;
#else
		exit(0);
#endif
	}
}

void LLPluginProcessChild::killSockets()
{
	killMessagePipe();
	mSocket.reset();
}

void LLPluginProcessChild::init(U32 launcher_port)
{
	mLauncherHost = LLHost("127.0.0.1", launcher_port);
	setState(STATE_INITIALIZED);
}

void LLPluginProcessChild::idle()
{
	bool idle_again;
	do
	{
		// Once we have hit the shutdown request state checking for errors
		// might put us in a spurious error state... Do not do that.
		if (mState < STATE_SHUTDOWNREQ)
		{
			if (APR_STATUS_IS_EOF(mSocketError))
			{
				// Plugin socket was closed. This covers both normal plugin
				// termination and host crashes.
				setState(STATE_ERROR);
			}
			else if (mSocketError != APR_SUCCESS)
			{
				llinfos << "message pipe is in error state (" << mSocketError
							<< "), moving to STATE_ERROR"<< llendl;
				setState(STATE_ERROR);
			}

			if (mState > STATE_INITIALIZED && !mMessagePipe)
			{
				// The pipe has been closed, we are done. *TODO: This could be
				// slightly more subtle, but not sure it needs to be.
				llinfos << "Message pipe went away, moving to STATE_ERROR"
						<< llendl;
				setState(STATE_ERROR);
			}
		}

		// If a state needs to go directly to another state (as a performance
		// enhancement), it can set idle_again to true after calling
		// setState(). USE THIS CAREFULLY, since it can starve other code.
		// Specifically make sure there is no way to get into a closed cycle
		// and never return. When in doubt, do not do it.
		idle_again = false;

		if (mInstance)
		{
			// Provide some time to the plugin
			mInstance->idle();
		}

		switch (mState)
		{
			case STATE_UNINITIALIZED:
				break;

			case STATE_INITIALIZED:
				if (mSocket->blockingConnect(mLauncherHost))
				{
					// This automatically sets mMessagePipe
					new LLPluginMessagePipe(this, mSocket);

					setState(STATE_CONNECTED);
				}
				else
				{
					// Connect failed
					setState(STATE_ERROR);
				}
				break;

			case STATE_CONNECTED:
				sendMessageToParent(LLPluginMessage(LLPLUGIN_MESSAGE_CLASS_INTERNAL,
													"hello"));
				setState(STATE_PLUGIN_LOADING);
				break;

			case STATE_PLUGIN_LOADING:
				if (!mPluginFile.empty())
				{
					mInstance = new LLPluginInstance(this);
					if (mInstance->load(mPluginDir, mPluginFile) == 0)
					{
						mHeartbeat.start();
						mHeartbeat.setTimerExpirySec(HEARTBEAT_SECONDS);
						mCPUElapsed = 0.f;
						setState(STATE_PLUGIN_LOADED);
					}
					else
					{
						setState(STATE_ERROR);
					}
				}
				break;

			case STATE_PLUGIN_LOADED:
			{
				setState(STATE_PLUGIN_INITIALIZING);
				LLPluginMessage message("base", "init");
				sendMessageToPlugin(message);
				break;
			}

			case STATE_PLUGIN_INITIALIZING:
				// Waiting for init_response...
				break;

			case STATE_RUNNING:
				if (mInstance)
				{
					// Provide some time to the plugin
					LLPluginMessage message("base", "idle");
					message.setValueReal("time", PLUGIN_IDLE_SECONDS);
					sendMessageToPlugin(message);

					mInstance->idle();

					if (mHeartbeat.hasExpired())
					{
						// This just proves that we are not stuck down inside
						// the plugin code.
						LLPluginMessage heartbeat(LLPLUGIN_MESSAGE_CLASS_INTERNAL,
												  "heartbeat");

						// Calculate the approximage CPU usage fraction
						// (floating point value between 0 and 1) used by the
						// plugin this heartbeat cycle. Note that this will not
						// take into account any threads or additional
						// processes the plugin spawns, but it's a first
						// approximation. If we could write OS-specific
						// functions to query the actual CPU usage of this
						// process, that would be a better approximation.
						heartbeat.setValueReal("cpu_usage",
											   mCPUElapsed /
											   mHeartbeat.getElapsedTimeF64());

						sendMessageToParent(heartbeat);

						mHeartbeat.reset();
						mHeartbeat.setTimerExpirySec(HEARTBEAT_SECONDS);
						mCPUElapsed = 0.f;
					}
				}
				// receivePluginMessage will transition to STATE_UNLOADING
				break;

			case STATE_SHUTDOWNREQ:
				// Set next state first thing in case "cleanup" message advances
				// state.
				setState(STATE_UNLOADING);
				mWaitGoodbye.setTimerExpirySec(GOODBYE_SECONDS);
				if (mInstance)
				{
					sendMessageToPlugin(LLPluginMessage("base", "cleanup"));
				}
				break;

			case STATE_UNLOADING:
				// Waiting for goodbye from plugin.
				if (mWaitGoodbye.hasExpired())
				{
					llwarns << "Wait for goodbye expired. Advancing to UNLOADED"
							<< llendl;			
					if (mInstance)
					{
						// Something went wrong, at least make sure plugin will
						// terminate
						sendMessageToPlugin(LLPluginMessage("base",
															"force_exit"));
					}
					setState(STATE_UNLOADED);
				}
				if (mInstance)
				{
					// Provide some time to the plugin. E.g. CEF on "cleanup"
					// sets shutdown request, but it still needs idle loop to
					// actually shutdown.
					LLPluginMessage message("base", "idle");
					message.setValueReal("time", PLUGIN_IDLE_SECONDS);
					sendMessageToPlugin(message);
					mInstance->idle();
				}
				break;

			case STATE_UNLOADED:
				killSockets();
				if (mInstance)
				{
					delete mInstance;
					mInstance = NULL;
				}
				setState(STATE_DONE);
				break;

			case STATE_ERROR:
				// Close the socket to the launcher
				killSockets();
				// TODO: Where do we go from here?  Just exit()?
				setState(STATE_DONE);
				break;

			case STATE_DONE:
				// Just sit here.
				break;
		}
	}
	while (idle_again);
}

void LLPluginProcessChild::sleep(F64 seconds)
{
	deliverQueuedMessages();
	if (mMessagePipe)
	{
		mMessagePipe->pump(seconds);
	}
	else
	{
		ms_sleep((int)(seconds * 1000.f));
	}
}

void LLPluginProcessChild::pump()
{
	deliverQueuedMessages();
	if (mMessagePipe)
	{
		mMessagePipe->pump(0.f);
	}
#if 0	// Should we warn here ?
	else
	{
	}
#endif
}

void LLPluginProcessChild::sendMessageToPlugin(const LLPluginMessage& message)
{
	if (mInstance)
	{
		std::string buffer = message.generate();

		LL_DEBUGS("Plugin") << "Sending to plugin: " << buffer << LL_ENDL;
		LLTimer elapsed;

		mInstance->sendMessage(buffer);

		mCPUElapsed += elapsed.getElapsedTimeF64();
	}
	else
	{
		llwarns_sparse << "Instance is NULL !" << llendl;
	}
}

void LLPluginProcessChild::sendMessageToParent(const LLPluginMessage& message)
{
	std::string buffer = message.generate();

	LL_DEBUGS("Plugin") << "Sending to parent: " << buffer << LL_ENDL;

	writeMessageRaw(buffer);
}

void LLPluginProcessChild::receiveMessageRaw(const std::string& message)
{
	// Incoming message from the TCP Socket

	LL_DEBUGS("Plugin") << "Received from parent: " << message << LL_ENDL;

	// Decode this message
	LLPluginMessage parsed;
	parsed.parse(message);

	if (mBlockingRequest)
	{
		// We are blocking the plugin waiting for a response.
		if (parsed.hasValue("blocking_response"))
		{
			// This is the message we've been waiting for: fall through and
			// send it immediately.
			mBlockingResponseReceived = true;
		}
		else
		{
			// Still waiting.  Queue this message and don't process it yet.
			mMessageQueue.emplace(message);
			return;
		}
	}

	bool pass_message = true;

	// FIXME: how should we handle queueing here ?

	{
		std::string message_class = parsed.getClass();
		if (message_class == LLPLUGIN_MESSAGE_CLASS_INTERNAL)
		{
			pass_message = false;

			std::string message_name = parsed.getName();
			if (message_name == "load_plugin")
			{
				mPluginFile = parsed.getValue("file");
				mPluginDir = parsed.getValue("dir");
			}
			else if (message_name == "shutdown_plugin")
			{
				setState(STATE_SHUTDOWNREQ);
			}
			else if (message_name == "shm_add")
			{
				std::string name = parsed.getValue("name");
				size_t size = (size_t)parsed.getValueS32("size");

				shared_mem_regions_t::iterator iter =
					mSharedMemoryRegions.find(name);
				if (iter != mSharedMemoryRegions.end())
				{
					// Need to remove the old region first
					llwarns << "Adding a duplicate shared memory segment !"
							<< llendl;
				}
				else
				{
					// This is a new region
					LLPluginSharedMemory* region = new LLPluginSharedMemory;
					if (region->attach(name, size))
					{
						mSharedMemoryRegions.emplace(name, region);
						std::stringstream addr;
						addr << region->getMappedAddress();

						// Send the add notification to the plugin...
						LLPluginMessage message("base", "shm_added");
						message.setValue("name", name);
						message.setValueS32("size", (S32)size);
						message.setValuePointer("address",
												region->getMappedAddress());
						sendMessageToPlugin(message);

						// And send the response to the parent.
						message.setMessage(LLPLUGIN_MESSAGE_CLASS_INTERNAL,
										   "shm_add_response");
						message.setValue("name", name);
						sendMessageToParent(message);
					}
					else
					{
						llwarns << "Could not create a shared memory segment !"
								<< llendl;
						delete region;
					}
				}
			}
			else if (message_name == "shm_remove")
			{
				std::string name = parsed.getValue("name");
				shared_mem_regions_t::iterator iter =
					mSharedMemoryRegions.find(name);
				if (iter != mSharedMemoryRegions.end())
				{
					// Forward the remove request to the plugin; its response
					// will trigger us to detach the segment.
					LLPluginMessage message("base", "shm_remove");
					message.setValue("name", name);
					sendMessageToPlugin(message);
				}
				else
				{
					llwarns << "shm_remove for unknown memory segment !"
							<< llendl;
				}
			}
			else if (message_name == "sleep_time")
			{
				mSleepTime = llmax(parsed.getValueReal("time"),
								   (F64)0.01f); // clamp to maximum of 100Hz
			}
#if LL_DEBUG
			else if (message_name == "crash")
			{
				// Crash the plugin
				llerrs << "Plugin crash requested." << llendl;
			}
			else if (message_name == "hang")
			{
				// Hang the plugin
				llwarns << "Plugin hang requested." << llendl;
				while (true) ;
			}
#endif
			else
			{
				llwarns << "Unknown internal message from parent: "
						<< message_name << llendl;
			}
		}
	}

	if (pass_message && mInstance)
	{
		LLTimer elapsed;
		mInstance->sendMessage(message);
		mCPUElapsed += elapsed.getElapsedTimeF64();
	}
}

//virtual
void LLPluginProcessChild::receivePluginMessage(const std::string& message)
{
	LL_DEBUGS("Plugin") << "Received from plugin: " << message << LL_ENDL;

	if (mBlockingRequest)
	{
		llwarns << "Cannot send a message while already waiting on a blocking request; aborting"
				<< llendl;
		return;
	}

	// Incoming message from the plugin instance
	bool pass_message = true;

	// *FIXME: how should we handle queueing here?

	// Intercept certain base messages (responses to ones sent by this class)
	{
		// Decode this message
		LLPluginMessage parsed;
		parsed.parse(message);

		if (parsed.hasValue("blocking_request"))
		{
			mBlockingRequest = true;
		}

		std::string message_class = parsed.getClass();
		if (message_class == "base")
		{
			std::string message_name = parsed.getName();
			if (message_name == "init_response")
			{
				// The plugin has finished initializing.
				setState(STATE_RUNNING);

				// Do not pass this message up to the parent
				pass_message = false;

				LLPluginMessage new_message(LLPLUGIN_MESSAGE_CLASS_INTERNAL,
											"load_plugin_response");
				LLSD versions = parsed.getValueLLSD("versions");
				new_message.setValueLLSD("versions", versions);

				if (parsed.hasValue("plugin_version"))
				{
					std::string version = parsed.getValue("plugin_version");
					new_message.setValueLLSD("plugin_version", version);
				}

				// Let the parent know it is loaded and initialized.
				sendMessageToParent(new_message);
			}
			else if (message_name == "goodbye")
			{
				setState(STATE_UNLOADED);
			}
			else if (message_name == "shm_remove_response")
			{
				// Do not pass this message up to the parent
				pass_message = false;

				std::string name = parsed.getValue("name");
				shared_mem_regions_t::iterator iter =
					mSharedMemoryRegions.find(name);
				if (iter != mSharedMemoryRegions.end())
				{
					// Detach the shared memory region
					iter->second->detach();

					// Remove it from our map
					mSharedMemoryRegions.erase(iter);

					// Finally, send the response to the parent.
					LLPluginMessage message(LLPLUGIN_MESSAGE_CLASS_INTERNAL,
											"shm_remove_response");
					message.setValue("name", name);
					sendMessageToParent(message);
				}
				else
				{
					llwarns << "shm_remove_response for unknown memory segment!"
							<< llendl;
				}
			}
		}
	}

	if (pass_message)
	{
		LL_DEBUGS("Plugin") << "Passing through to parent: " << message
							<< LL_ENDL;
		writeMessageRaw(message);
	}

	while (mBlockingRequest)
	{
		// The plugin wants to block and wait for a response to this message.
		sleep(mSleepTime);	// Pumps the message pipe and processes messages

		if (mBlockingResponseReceived || mSocketError != APR_SUCCESS ||
			!mMessagePipe)
		{
			// Response has been received, or we've hit an error state. Stop
			// waiting.
			mBlockingRequest = false;
			mBlockingResponseReceived = false;
		}
	}
}

void LLPluginProcessChild::setState(EState state)
{
	LL_DEBUGS("Plugin") << "Setting state to " << state << LL_ENDL;
	mState = state;
}

void LLPluginProcessChild::deliverQueuedMessages()
{
	if (!mBlockingRequest)
	{
		while (!mMessageQueue.empty())
		{
			receiveMessageRaw(mMessageQueue.front());
			mMessageQueue.pop();
		}
	}
}
