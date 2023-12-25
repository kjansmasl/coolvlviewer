/**
 * @file media_plugin_base.h
 * @brief Media plugin base class for LLMedia API plugin system
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

#include "llplugininstance.h"
#include "llpluginmessage.h"
#include "llpluginmessageclasses.h"

class MediaPluginBase
{
public:
	// @param[in] host_send_func Function for sending messages from plugin to
	//			  plugin loader shell
	// @param[in] host_user_data Message data for messages from plugin to
	//			  plugin loader shell
	MediaPluginBase(LLPluginInstance::sendMessageFunction host_send_func,
					void* host_user_data);

	virtual ~MediaPluginBase()					{}

	// Handle received message from plugin loader shell.
	virtual void receiveMessage(const char *message_string) = 0;

	/**
	 * Receives message from plugin loader shell.
	 *
	 * @param[in] message_string Message string
	 * @param[in] user_data Message data
	 */
	static void staticReceiveMessage(const char* message_string,
									 void** user_data);

protected:
   // Plugin status.
	typedef enum
	{
		STATUS_NONE,
		STATUS_LOADING,
		STATUS_LOADED,
		STATUS_ERROR,
		STATUS_PLAYING,
		STATUS_PAUSED,
		STATUS_DONE
	} EStatus;

	// Plugin shared memory.
	class SharedSegmentInfo
	{
	public:
		void*	mAddress;
		size_t	mSize;
	};

	/**
	 * Sends message to plugin loader shell.
	 *
	 * @param[in] message Message data being sent to plugin loader shell
	 */
	void sendMessage(const LLPluginMessage& message);

	/**
	 * Sends "media_status" message to plugin loader shell ("loading",
	 * "playing", "paused", etc.)
	 */
	void sendStatus();

	/**
	 * Converts current media status enum value into string (STATUS_LOADING
	 * into "loading", etc.)
	 *
	 * @return Media status string ("loading", "playing", "paused", etc)
	 */
	std::string statusString();

	/**
	 * Sets media status.
	 *
	 * @param[in] status Media status (STATUS_LOADING, STATUS_PLAYING,
	 *			  STATUS_PAUSED, etc)
	 */
	void setStatus(EStatus status);

	/**
	 * Notifies plugin loader shell that part of display area needs to be redrawn.
	 *
	 * @param[in] left Left X coordinate of area to redraw
	 * @param[in] top Top Y coordinate of area to redraw
	 * @param[in] right Right X-coordinate of area to redraw
	 * @param[in] bottom Bottom Y-coordinate of area to redraw
	 * Note: the (0,0) coordinates correspond to the top left corner.
	 */
	virtual void setDirty(int left, int top, int right, int bottom);

protected:
	// Map of shared memory names to shared memory.
	typedef std::map<std::string, SharedSegmentInfo>	SharedSegmentMap;

	// Function to send message from plugin to plugin loader shell.
	LLPluginInstance::sendMessageFunction				mHostSendFunction;

	// Message data being sent to plugin loader shell by mHostSendFunction.
	void*												mHostUserData;

	// Pixel array to display. *TODO documentation: are pixels always 24 bits
	// RGB format, aligned on 32 bits boundary ?  Also, calling this a pixel
	// array may be misleading since 1 pixel > 1 char.
	unsigned char*										mPixels;

	// *TODO documentation: what is this for ?  Does a texture have its own
	// piece of shared memory ?  Updated on size_change_request, cleared on
	// shm_remove.
	std::string											mTextureSegmentName;

	// Map of shared memory segments.
	SharedSegmentMap									mSharedSegments;

	// Width of plugin display in pixels.
	int													mWidth;
	// Height of plugin display in pixels.
	int													mHeight;
	// Width of plugin texture.
	int													mTextureWidth;
	// Height of plugin texture.
	int													mTextureHeight;
	// Pixel depth (pixel size in bytes).
	int													mDepth;

	// Current status of plugin.
	EStatus												mStatus;

	// Flag to delete plugin instance (self).
	bool												mDeleteMe;
};

/** The plugin <b>must</b> define this function to create its instance.
 * It should look something like this:
 * @code
 * {
 *	MediaPluginFoo *self = new MediaPluginFoo(host_send_func, host_user_data);
 *	*plugin_send_func = MediaPluginFoo::staticReceiveMessage;
 *	*plugin_user_data = (void*)self;
 *
 *	return 0;
 * }
 * @endcode
 */
int init_media_plugin(LLPluginInstance::sendMessageFunction host_send_func,
					  void* host_user_data,
					  LLPluginInstance::sendMessageFunction* plugin_send_func,
					  void** plugin_user_data);
