/**
* @file media_plugin_cef.cpp
* @brief CEF (Chromium Embedding Framework) plugin for LLMedia API plugin system
*
* @cond
* $LicenseInfo:firstyear=2008&license=viewerlgpl$
* Second Life Viewer Source Code
* Copyright (C) 2010, Linden Research, Inc.
*
* This library is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation;
* version 2.1 of the License only.
*
* This library is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with this library; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*
* Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
* $/LicenseInfo$
* @endcond
*/

#include "linden_common.h"


#if LL_WINDOWS
# include <process.h>				// For _getpid()
#else
# include <unistd.h>				// For getpid()
#endif

#include "dullahan.h"

#ifndef CEF_VERSION
# include "dullahan_version.h"
#endif

#include "indra_constants.h"		// For indra keyboard codes
#include "lldiriterator.h"			// LLDirIterator::deleteRecursivelyInDir()
#include "llglheaders.h"			// For the GL_* constants
#include "llplugininstance.h"
#include "llpluginmessage.h"
#include "llpluginmessageclasses.h"
#include "llsdutil.h"
#include "lltimer.h"				// For ms_sleep()
#include "media_plugin_base.h"
#include "volume_catcher.h"

// Defines used as shortcuts for placeholders
#define PH1 std::placeholders::_1
#define PH2 std::placeholders::_2
#define PH3 std::placeholders::_3
#define PH4 std::placeholders::_4
#define PH5 std::placeholders::_5

class MediaPluginCEF : public MediaPluginBase
{
public:
	MediaPluginCEF(LLPluginInstance::sendMessageFunction host_send_func,
				   void* host_user_data);
	~MediaPluginCEF() override;

	void receiveMessage(const char* msgstr) override;

private:
	bool init();

	void onPageChangedCallback(const unsigned char* pixels, int x, int y,
							   int width, int height);
	void onLoadError(int status, const std::string error_text);
	void onOpenPopupCallback(const std::string url, const std::string target);
#if HB_DULLAHAN_EXTENDED >= 2
	void onCustomSchemeURLCallback(const std::string url, bool user_gesture,
								   bool is_redirect);
#else
	void onCustomSchemeURLCallback(const std::string url);
#endif
	void onConsoleMessageCallback(std::string message, std::string source,
								  int line);
	void onStatusMessageCallback(const std::string value);
	void onTitleChangeCallback(std::string title);
	void onTooltipCallback(const std::string text);
	bool onJSDialogCallback(const std::string origin_url,
							const std::string message_text,
							const std::string default_prompt_text);
	bool onJSBeforeUnloadCallback();
	void onLoadStartCallback();
	void onLoadEndCallback(int httpStatusCode);
	void onAddressChangeCallback(const std::string url);
	bool onHTTPAuthCallback(const std::string host, const std::string realm,
							std::string& username, std::string& password);

	void onCursorChangedCallback(dullahan::ECursorType type);
	void onRequestExitCallback();

	void postDebugMessage(const std::string& msg);

	void authResponse(LLPluginMessage& message);

	const std::vector<std::string> onFileDialog(dullahan::EFileDialogType dialog_type,
												const std::string dialog_title,
												const std::string default_file,
												const std::string dialog_accept_filter,
												bool& use_default);

	void keyEvent(dullahan::EKeyEvent key_event,
				  LLSD native_key_data = LLSD::emptyMap());
	void unicodeInput(std::string event, LLSD native_key_data = LLSD::emptyMap());

	void checkEditState();
	void setVolume();

private:
	dullahan*					mCEFLib;

	VolumeCatcher				mVolumeCatcher;
	F32							mCurVolume;

	U32							mMinimumFontSize;
	U32							mDefaultFontSize;

	std::string 				mHostLanguage;
	std::string					mAuthUsername;
	std::string					mAuthPassword;
	std::string					mPreferredFont;
	std::string					mUserAgent;
	std::string					mPickedFile;
	std::string					mUserDataDir;
	std::string					mUserCacheDir;

	std::vector<std::string>	mPickedFiles;

	std::string					mProxyHost;
	U16							mProxyPort;
	bool						mProxyEnabled;

	// Plugins support has been entirely gutted out from CEF 100
#if CHROME_VERSION_MAJOR < 100
	bool						mPluginsEnabled;
#endif
	bool						mCookiesEnabled;
	bool						mJavascriptEnabled;
	bool						mAuthOK;
	bool						mRemoteFonts;
	bool						mCanCopy;
	bool						mCanCut;
	bool						mCanPaste;
	bool						mEnableMediaPluginDebugging;
	bool						mCleanupDone;
	bool						mWheelHackDone;
};

MediaPluginCEF::MediaPluginCEF(LLPluginInstance::sendMessageFunction host_send_func,
							   void* host_user_data)
:	MediaPluginBase(host_send_func, host_user_data),
	mMinimumFontSize(0),
	mDefaultFontSize(0),
	mHostLanguage("en"),
	mCurVolume(0.5f),		// Set default to a reasonnable level
	mProxyPort(0),
	mProxyEnabled(false),
	mWheelHackDone(false),
#if CHROME_VERSION_MAJOR < 100
	mPluginsEnabled(true),
#endif
	mCookiesEnabled(true),
	mJavascriptEnabled(true),
	mAuthOK(false),
	mRemoteFonts(true),
	mCanCopy(false),
	mCanCut(false),
	mCanPaste(false),
	mEnableMediaPluginDebugging(true),
	mCleanupDone(false)
{
	mWidth = 0;
	mHeight = 0;
	mDepth = 4;
	mPixels = 0;

	mCEFLib = new dullahan();

	setVolume();
}

MediaPluginCEF::~MediaPluginCEF()
{
	if (mEnableMediaPluginDebugging)
	{
		std::cerr << "MediaPluginCEF::~MediaPluginCEF called" << std::endl;
	}
	if (!mCleanupDone)
	{
		std::cerr << "MediaPluginCEF::~MediaPluginCEF: calling requestExit()"
				  << std::endl;
		mCleanupDone = true;
		mCEFLib->requestExit();
		// Let some time for CEF to actually cleanup
		ms_sleep(1000);
		std::cerr << "MediaPluginCEF::~MediaPluginCEF: now shutting down"
				  << std::endl;
	}
	mCEFLib->shutdown();

	// With CEF 120+, delete the per-CEF instance cache sub-directory. HB
#if CHROME_VERSION_MAJOR >= 120
	LLDirIterator::deleteRecursivelyInDir(mUserCacheDir);
	LLFile::rmdir(mUserCacheDir);
#endif
}

void MediaPluginCEF::postDebugMessage(const std::string& msg)
{
	if (mEnableMediaPluginDebugging)
	{
		std::stringstream str;
		str << "@Media Msg> " << msg;

		LLPluginMessage debug_message(LLPLUGIN_MESSAGE_CLASS_MEDIA,
									  "debug_message");
		debug_message.setValue("message_text", str.str());
		debug_message.setValue("message_level", "info");
		sendMessage(debug_message);
	}
}

void MediaPluginCEF::onPageChangedCallback(const unsigned char* pixels, int x,
										   int y, int width, int height)
{
	if (mPixels && pixels)
	{
		if (mWidth == width && mHeight == height)
		{
			memcpy(mPixels, pixels, mWidth * mHeight * mDepth);
		}
		else
		{
			mCEFLib->setSize(mWidth, mHeight);
		}
		setDirty(0, 0, mWidth, mHeight);
	}
# if 1	// *HACK: to get the first scrollable page to draw
	if (!mWheelHackDone)
	{
		mWheelHackDone = true;
		mCEFLib->mouseWheel(0, 0, 0, -1);
		mCEFLib->mouseWheel(0, 0, 0, 1);
	}
# endif
}

void MediaPluginCEF::onLoadError(int status, const std::string error_text)
{
	std::stringstream msg;
	msg << "<b>Loading error !</b><p>Message: " << error_text;
	msg << "<br />Code: " << status << "</p>";
	mCEFLib->showBrowserMessage(msg.str());
}

void MediaPluginCEF::onConsoleMessageCallback(std::string message,
											  std::string source, int line)
{
	std::stringstream str;
	str << "Console message: " << message << " in file(" << source
		<< ") at line " << line;
	postDebugMessage(str.str());
}

void MediaPluginCEF::onStatusMessageCallback(const std::string value)
{
	LLPluginMessage message(LLPLUGIN_MESSAGE_CLASS_MEDIA_BROWSER,
							"status_text");
	message.setValue("status", value);
	sendMessage(message);
}

void MediaPluginCEF::onTitleChangeCallback(std::string title)
{
	LLPluginMessage message(LLPLUGIN_MESSAGE_CLASS_MEDIA, "name_text");
	message.setValue("name", title);
	message.setValue("artist", "");
	message.setValueBoolean("history_back_available", mCEFLib->canGoBack());
	message.setValueBoolean("history_forward_available",
							mCEFLib->canGoForward());
	sendMessage(message);
}

void MediaPluginCEF::onTooltipCallback(const std::string text)
{
	LLPluginMessage message(LLPLUGIN_MESSAGE_CLASS_MEDIA, "tooltip_text");
	message.setValue("tooltip", text);
	sendMessage(message);
}

bool MediaPluginCEF::onJSDialogCallback(const std::string origin_url,
										const std::string message_text,
										const std::string default_prompt_text)
{
	// Indicates we suppress the JavaScript alert UI entirely
	return true;
}

bool MediaPluginCEF::onJSBeforeUnloadCallback()
{
	// Indicates we suppress the JavaScript alert UI entirely
	return true;
}

void MediaPluginCEF::onLoadStartCallback()
{
	LLPluginMessage message(LLPLUGIN_MESSAGE_CLASS_MEDIA_BROWSER,
							"navigate_begin");
#if 0	// Not easily available here in CEF - needed ?
	message.setValue("uri", event.getEventUri());
#endif
	message.setValueBoolean("history_back_available", mCEFLib->canGoBack());
	message.setValueBoolean("history_forward_available",
							mCEFLib->canGoForward());
	sendMessage(message);
}

void MediaPluginCEF::onLoadEndCallback(int httpStatusCode)
{
	LLPluginMessage message(LLPLUGIN_MESSAGE_CLASS_MEDIA_BROWSER,
							"navigate_complete");
#if 0	// Not easily available here in CEF - needed ?
	message.setValue("uri", event.getEventUri());
#endif
	message.setValueS32("result_code", httpStatusCode);
	message.setValueBoolean("history_back_available", mCEFLib->canGoBack());
	message.setValueBoolean("history_forward_available",
							mCEFLib->canGoForward());
	sendMessage(message);
}

void MediaPluginCEF::onAddressChangeCallback(const std::string url)
{
	LLPluginMessage message(LLPLUGIN_MESSAGE_CLASS_MEDIA_BROWSER,
							"location_changed");
	message.setValue("uri", url);
	sendMessage(message);
}

void MediaPluginCEF::onOpenPopupCallback(const std::string url,
										 const std::string target)
{
	LLPluginMessage message(LLPLUGIN_MESSAGE_CLASS_MEDIA_BROWSER,
							"click_href");
	message.setValue("uri", url);
	message.setValue("target", target);
	message.setValue("uuid", "");	// not used right now
	sendMessage(message);
}

#if HB_DULLAHAN_EXTENDED >= 2
void MediaPluginCEF::onCustomSchemeURLCallback(const std::string url,
											   bool user_gesture,
											   bool is_redirect)
#else
void MediaPluginCEF::onCustomSchemeURLCallback(const std::string url)
#endif
{
	if (mEnableMediaPluginDebugging)
	{
		std::cerr << "MediaPluginCEF::onCustomSchemeURLCallback called with: url = "
				  << url << std::endl;
	}
	LLPluginMessage message(LLPLUGIN_MESSAGE_CLASS_MEDIA_BROWSER,
							"click_nofollow");
	message.setValue("uri", url);
#if HB_DULLAHAN_EXTENDED >= 2
	message.setValue("nav_type", user_gesture ? "clicked" : "navigated");
	message.setValueBoolean("is_redirect", is_redirect);
#else
	message.setValue("nav_type", "clicked");
#endif
	sendMessage(message);
}

bool MediaPluginCEF::onHTTPAuthCallback(const std::string host,
										const std::string realm,
										std::string& username,
										std::string& password)
{
	mAuthOK = false;

	LLPluginMessage message(LLPLUGIN_MESSAGE_CLASS_MEDIA, "auth_request");
	message.setValue("url", host);
	message.setValue("realm", realm);
	message.setValueBoolean("blocking_request", true);

	// The "blocking_request" key in the message means this sendMessage call
	// will block until a response is received.
	sendMessage(message);

	if (mAuthOK)
	{
		username = mAuthUsername;
		password = mAuthPassword;
	}

	return mAuthOK;
}

const std::vector<std::string> MediaPluginCEF::onFileDialog(dullahan::EFileDialogType dialog_type,
															const std::string dialog_title,
															const std::string default_file,
															const std::string dialog_accept_filter,
															bool& use_default)
{
	// Never use the default CEF file picker
	use_default = false;

	if (dialog_type == dullahan::FD_OPEN_FILE ||
		dialog_type == dullahan::FD_OPEN_MULTIPLE_FILES)
	{
		mPickedFiles.clear();

		LLPluginMessage message(LLPLUGIN_MESSAGE_CLASS_MEDIA, "pick_file");
		message.setValueBoolean("blocking_request", true);
		message.setValueBoolean("multiple_files",
								dialog_type == dullahan::FD_OPEN_MULTIPLE_FILES);

		// The "blocking_request" key in the message means this sendMessage
		// call will block until a response is received.
		sendMessage(message);

		return mPickedFiles;
	}
	else if (dialog_type == dullahan::FD_SAVE_FILE)
	{
		mAuthOK = false;

		LLPluginMessage message(LLPLUGIN_MESSAGE_CLASS_MEDIA, "file_download");
		message.setValue("filename", default_file);

		sendMessage(message);
	}

	return std::vector<std::string>();
}

void MediaPluginCEF::onCursorChangedCallback(dullahan::ECursorType type)
{
	std::string name;

	switch (type)
	{
		case dullahan::CT_IBEAM:
			name = "ibeam";
			break;

		case dullahan::CT_NORTHSOUTHRESIZE:
			name = "splitv";
			break;

		case dullahan::CT_EASTWESTRESIZE:
			name = "splith";
			break;

		case dullahan::CT_HAND:
			name = "hand";
			break;

		// For anything else, default to the arrow
		case dullahan::CT_POINTER:
		default:
			name = "arrow";
	}

	LLPluginMessage message(LLPLUGIN_MESSAGE_CLASS_MEDIA, "cursor_changed");
	message.setValue("name", name);
	sendMessage(message);
}

void MediaPluginCEF::onRequestExitCallback()
{
	if (mEnableMediaPluginDebugging)
	{
		std::cerr << "MediaPluginCEF::onRequestExitCallback called"
				  << std::endl;
	}
	mCleanupDone = true;

	LLPluginMessage message("base", "goodbye");
	sendMessage(message);

	mDeleteMe = true;
}

void MediaPluginCEF::authResponse(LLPluginMessage& message)
{
	mAuthOK = message.getValueBoolean("ok");
	if (mAuthOK)
	{
		mAuthUsername = message.getValue("username");
		mAuthPassword = message.getValue("password");
	}
}

void MediaPluginCEF::receiveMessage(const char* msgstr)
{
	if (mCleanupDone)
	{
		if (mEnableMediaPluginDebugging)
		{
			std::cerr << "MediaPluginCEF::receiveMessage: received message: \""
					  << msgstr << "\" after cleanup !" << std::endl;
		}
		return;
	}

	LLPluginMessage message_in;
	if (message_in.parse(msgstr) < 0)
	{
		return;
	}

	std::string message_class = message_in.getClass();
	std::string message_name = message_in.getName();

	if (mEnableMediaPluginDebugging)
	{
		// Do not spam cerr with a gazillon of idle messages...
		if (message_name != "idle" &&
			// Neither with mouse move messages !
			(message_name != "mouse_event" ||
			 std::string(msgstr).find("<string>move</string>") ==
				std::string::npos))
		{
			std::cerr << "MediaPluginCEF::receiveMessage: received message: \""
					  << msgstr << "\"" << std::endl;
		}
	}

	if (message_class == LLPLUGIN_MESSAGE_CLASS_BASE)
	{
		if (message_name == "init")
		{
			LLPluginMessage message("base", "init_response");
			LLSD versions = LLSD::emptyMap();
			versions[LLPLUGIN_MESSAGE_CLASS_BASE] =
				LLPLUGIN_MESSAGE_CLASS_BASE_VERSION;
			versions[LLPLUGIN_MESSAGE_CLASS_MEDIA] =
				LLPLUGIN_MESSAGE_CLASS_MEDIA_VERSION;
			versions[LLPLUGIN_MESSAGE_CLASS_MEDIA_BROWSER] =
				LLPLUGIN_MESSAGE_CLASS_MEDIA_BROWSER_VERSION;
			message.setValueLLSD("versions", versions);

			std::string cef_version = CEF_VERSION;
			// Shorten the version string by removing the last commit hash. HB
			size_t i = cef_version.find('+');
			size_t j = cef_version.rfind('-');
			if (j > i && i != std::string::npos)
			{
				cef_version = cef_version.substr(0, i) + "/Chromium " +
							  cef_version.substr(j + 1);
			}
			message.setValue("plugin_version",
							 llformat("Dullahan %d.%d.%d/CEF %s",
									  DULLAHAN_VERSION_MAJOR,
									  DULLAHAN_VERSION_MINOR,
									  DULLAHAN_VERSION_POINT,
									  cef_version.c_str()));
			sendMessage(message);
		}
		else if (message_name == "idle")
		{
			mCEFLib->update();
			mVolumeCatcher.pump();
			checkEditState();
		}
		else if (message_name == "cleanup")
		{
			mCEFLib->requestExit();
		}
		else if (message_name == "force_exit")
		{
			mDeleteMe = true;
		}
		else if (message_name == "shm_added")
		{
			SharedSegmentInfo info;
			info.mAddress = message_in.getValuePointer("address");
			info.mSize = (size_t)message_in.getValueS32("size");
			std::string name = message_in.getValue("name");

			mSharedSegments.emplace(name, info);
		}
		else if (message_name == "shm_remove")
		{
			std::string name = message_in.getValue("name");

			SharedSegmentMap::iterator iter = mSharedSegments.find(name);
			if (iter != mSharedSegments.end())
			{
				if (mPixels == iter->second.mAddress)
				{
					mPixels = NULL;
					mTextureSegmentName.clear();
				}
				mSharedSegments.erase(iter);
			}
			else if (mEnableMediaPluginDebugging)
			{
				std::cerr << "MediaPluginCEF::receiveMessage: unknown shared memory region !"
						  << std::endl;
			}

			LLPluginMessage message("base", "shm_remove_response");
			message.setValue("name", name);
			sendMessage(message);
		}
		else if (mEnableMediaPluginDebugging)
		{
			std::cerr << "MediaPluginCEF::receiveMessage: unknown base message: "
					  << message_name << std::endl;
		}
	}
	else if (message_class == LLPLUGIN_MESSAGE_CLASS_MEDIA)
	{
		if (message_name == "init")
		{
			mCEFLib->setOnPageChangedCallback(std::bind(&MediaPluginCEF::onPageChangedCallback,
														this, PH1, PH2, PH3, PH4, PH5));
			mCEFLib->setOnOpenPopupCallback(std::bind(&MediaPluginCEF::onOpenPopupCallback,
													  this, PH1, PH2));
			mCEFLib->setOnFileDialogCallback(std::bind(&MediaPluginCEF::onFileDialog,
													   this, PH1, PH2, PH3, PH4, PH5));
			mCEFLib->setOnLoadErrorCallback(std::bind(&MediaPluginCEF::onLoadError,
													  this, PH1, PH2));
#if HB_DULLAHAN_EXTENDED >= 2
			mCEFLib->setOnCustomSchemeURLCallback(std::bind(&MediaPluginCEF::onCustomSchemeURLCallback,
															this, PH1, PH2,
															PH3));
#else
			mCEFLib->setOnCustomSchemeURLCallback(std::bind(&MediaPluginCEF::onCustomSchemeURLCallback,
															this, PH1));
#endif
			mCEFLib->setOnConsoleMessageCallback(std::bind(&MediaPluginCEF::onConsoleMessageCallback,
														   this, PH1, PH2, PH3));
			mCEFLib->setOnStatusMessageCallback(std::bind(&MediaPluginCEF::onStatusMessageCallback,
														  this, PH1));
			mCEFLib->setOnTitleChangeCallback(std::bind(&MediaPluginCEF::onTitleChangeCallback,
														this, PH1));
			mCEFLib->setOnTooltipCallback(std::bind(&MediaPluginCEF::onTooltipCallback,
													this, PH1));
			mCEFLib->setOnLoadStartCallback(std::bind(&MediaPluginCEF::onLoadStartCallback,
													  this));
			mCEFLib->setOnLoadEndCallback(std::bind(&MediaPluginCEF::onLoadEndCallback,
													this, PH1));
			mCEFLib->setOnAddressChangeCallback(std::bind(&MediaPluginCEF::onAddressChangeCallback,
														  this, PH1));
			mCEFLib->setOnHTTPAuthCallback(std::bind(&MediaPluginCEF::onHTTPAuthCallback,
													 this, PH1, PH2, PH3, PH4));
			mCEFLib->setOnCursorChangedCallback(std::bind(&MediaPluginCEF::onCursorChangedCallback,
														  this, PH1));
			mCEFLib->setOnRequestExitCallback(std::bind(&MediaPluginCEF::onRequestExitCallback,
														this));
			mCEFLib->setOnJSDialogCallback(std::bind(&MediaPluginCEF::onJSDialogCallback,
													 this, PH1, PH2, PH3));
			mCEFLib->setOnJSBeforeUnloadCallback(std::bind(&MediaPluginCEF::onJSBeforeUnloadCallback,
														   this));
			dullahan::dullahan_settings settings;
			settings.initial_width = 1024;
			settings.initial_height = 1024;
			settings.user_agent_substring =
				mCEFLib->makeCompatibleUserAgentString(mUserAgent);
			settings.cookies_enabled = mCookiesEnabled;
			settings.cache_enabled = true;
			settings.accept_language_list = mHostLanguage;
			settings.javascript_enabled = mJavascriptEnabled;
#if CHROME_VERSION_MAJOR < 100
			settings.plugins_enabled = mPluginsEnabled;
#endif
			if (mProxyEnabled && !mProxyHost.empty())
			{
				std::ostringstream proxy_url;
				proxy_url << mProxyHost << ":" << mProxyPort;
				settings.proxy_host_port = proxy_url.str();
			}
			// MAINT-6060 - WebRTC media removed until we can add granularity 
			// or query UI
			settings.media_stream_enabled = false;
			settings.background_color = 0xffffffff;
			settings.disable_gpu = false;
			settings.flip_mouse_y = false;
			settings.flip_pixels_y = true;
			settings.frame_rate = 60;
			settings.force_wave_audio = false;
			settings.autoplay_without_gesture = true;
			settings.java_enabled = false;
			settings.webgl_enabled = true;
			// Disable remote debugging:
			settings.remote_debugging_port = -1;
			std::vector<std::string> custom_schemes;
			custom_schemes.emplace_back("secondlife");
			custom_schemes.emplace_back("hop");
			custom_schemes.emplace_back("x-grid-info");
			custom_schemes.emplace_back("x-grid-location-info");
			mCEFLib->setCustomSchemes(custom_schemes);
#if HB_DULLAHAN_EXTENDED
			// Not implemented in LL's pre-compiled Dullahan
			settings.minimum_font_size = mMinimumFontSize;
			settings.default_font_size = mDefaultFontSize;
			settings.remote_fonts = mRemoteFonts;
			settings.preferred_font = mPreferredFont;
			settings.user_data_dir = mUserCacheDir;
			settings.debug = mEnableMediaPluginDebugging;
#else	// HB_DULLAHAN_EXTENDED
			settings.cache_path = mUserCacheDir;
			settings.root_cache_path = mUserCacheDir;
			settings.context_cache_path = "";	// Disabled
# if LL_WINDOWS
			std::vector<wchar_t> buffer(MAX_PATH + 1);
			GetCurrentDirectoryW(MAX_PATH, &buffer[0]);
			settings.host_process_path = ll_convert_wide_to_string(&buffer[0]);
# endif
			settings.log_file = mUserCacheDir + "cef_log.txt";
			settings.log_verbose = mEnableMediaPluginDebugging;
#endif	// HB_DULLAHAN_EXTENDED
#if LL_DARWIN
			settings.disable_network_service = true;
			settings.use_mock_keychain = true;
#endif

			bool result = mCEFLib->init(settings);
#if 0		// *TODO - return something to indicate failure
			if (!result)
			{
				MessageBoxA(0, "FAIL INIT", 0, 0);
			}
#endif
			if (!result && mEnableMediaPluginDebugging)
			{
				std::cerr << "MediaPluginCEF::receiveMessage: mCEFLib->init() failed"
						  << std::endl;
			}

			// Plugin gets to decide the texture parameters to use.
			mDepth = 4;
			LLPluginMessage message(LLPLUGIN_MESSAGE_CLASS_MEDIA,
									"texture_params");
			message.setValueS32("default_width", 1024);
			message.setValueS32("default_height", 1024);
			message.setValueS32("depth", mDepth);
			message.setValueU32("internalformat", GL_RGB);
			message.setValueU32("format", GL_BGRA);
			message.setValueU32("type", GL_UNSIGNED_BYTE);
			message.setValueBoolean("coords_opengl", true);
			sendMessage(message);
		}
		else if (message_name == "set_user_data_path")
		{
			// Note: path always got a trailing platform-specific directory
			// delimiter
			mUserDataDir = message_in.getValue("path") + "cef_cache";
			// Starting with CEF 120, we *must* use a different cache directory
			// for each new CEF instance. Let's make it so we can still share
			// cookies, by linking them on CEF instance creation; it is a hack,
			// but it is the best we can do... HB
#if CHROME_VERSION_MAJOR >= 120
			LLFile::mkdir(mUserDataDir);
			mUserCacheDir = mUserDataDir + LL_DIR_DELIM_STR;
# if LL_WINDOWS
			mUserCacheDir += std::to_string(_getpid());
# else
			mUserCacheDir += std::to_string(getpid());
# endif
			LLFile::mkdir(mUserCacheDir);
# define COOKIES LL_DIR_DELIM_STR "Cookies"
# define JOURNAL LL_DIR_DELIM_STR "Cookies-journal"
			bool success = LLFile::createFileSymlink(mUserDataDir + COOKIES,
													 mUserCacheDir + COOKIES);
			if (success)
			{
				success = LLFile::createFileSymlink(mUserDataDir + JOURNAL,
													mUserCacheDir + JOURNAL);
			}
			if (!success && mEnableMediaPluginDebugging)
			{
				std::cerr << "Failed to link cookies database" << std::endl;
			}
#else
			mUserCacheDir = mUserDataDir;
#endif
			if (mEnableMediaPluginDebugging)
			{
				std::cerr << "Using cache directory: " << mUserCacheDir
						  << std::endl;
			}
		}
		else if (message_name == "size_change")
		{
			std::string name = message_in.getValue("name");
			S32 width = message_in.getValueS32("width");
			S32 height = message_in.getValueS32("height");
			S32 texture_width = message_in.getValueS32("texture_width");
			S32 texture_height = message_in.getValueS32("texture_height");

			if (!name.empty())
			{
				// Find the shared memory region with this name
				SharedSegmentMap::iterator iter = mSharedSegments.find(name);
				if (iter != mSharedSegments.end())
				{
					mPixels = (unsigned char*)iter->second.mAddress;
					mWidth = width;
					mHeight = height;

					mTextureWidth = texture_width;
					mTextureHeight = texture_height;
					mCEFLib->setSize(mWidth, mHeight);
				}
			}

			mCEFLib->setSize(mWidth, mHeight);

			LLPluginMessage message(LLPLUGIN_MESSAGE_CLASS_MEDIA,
									"size_change_response");
			message.setValue("name", name);
			message.setValueS32("width", width);
			message.setValueS32("height", height);
			message.setValueS32("texture_width", texture_width);
			message.setValueS32("texture_height", texture_height);
			sendMessage(message);
		}
		else if (message_name == "set_language_code")
		{
			mHostLanguage = message_in.getValue("language");
		}
		else if (message_name == "load_uri")
		{
			std::string uri = message_in.getValue("uri");
			mCEFLib->navigate(uri);
		}
		else if (message_name == "set_cookie")
		{
			std::string uri = message_in.getValue("uri");
			std::string name = message_in.getValue("name");
			std::string value = message_in.getValue("value");
			std::string domain = message_in.getValue("domain");
			std::string path = message_in.getValue("path");
			bool httponly = message_in.getValueBoolean("httponly");
			bool secure = message_in.getValueBoolean("secure");
			mCEFLib->setCookie(uri, name, value, domain, path, httponly,
							   secure);
		}
		else if (message_name == "mouse_event")
		{
			std::string event = message_in.getValue("event");

			S32 x = message_in.getValueS32("x");
			S32 y = message_in.getValueS32("y");

			dullahan::EMouseButton btn = dullahan::MB_MOUSE_BUTTON_LEFT;
			S32 button = message_in.getValueS32("button");
#if 1		// Do not transmit middle or right clicks
			if (button == 1 || button == 2) return;
#else
			if (button == 1) btn = dullahan::MB_MOUSE_BUTTON_RIGHT;
			if (button == 2) btn = BROWSER_MB_MIDDLE;
#endif
#if 0		// Not used for now
			std::string modifiers = message_in.getValue("modifiers");
#endif
			if (event == "down")
			{
				mCEFLib->mouseButton(btn, dullahan::ME_MOUSE_DOWN, x, y);
				mCEFLib->setFocus();
				std::stringstream str;
				str << "Mouse down at = " << x << ", " << y;
				postDebugMessage(str.str());
			}
			else if (event == "up")
			{
				mCEFLib->mouseButton(btn, dullahan::ME_MOUSE_UP, x, y);
				std::stringstream str;
				str << "Mouse up at = " << x << ", " << y;
				postDebugMessage(str.str());
			}
			else if (event == "double_click")
			{
				mCEFLib->mouseButton(btn, dullahan::ME_MOUSE_DOUBLE_CLICK,
									 x, y);
			}
			else
			{
				mCEFLib->mouseMove(x, y);
			}
		}
		else if (message_name == "scroll_event")
		{
			S32 x = message_in.getValueS32("x");
			S32 y = message_in.getValueS32("y");
			S32 delta_x = 40 * message_in.getValueS32("clicks_x");
			S32 delta_y = -40 * message_in.getValueS32("clicks_y");
			mCEFLib->mouseWheel(x, y, delta_x, delta_y);
		}
		else if (message_name == "text_event")
		{
			LLSD native_key_data = message_in.getValueLLSD("native_key_data");
			std::string event = message_in.getValue("event");
			unicodeInput(event, native_key_data);
		}
		else if (message_name == "key_event")
		{
			LLSD native_key_data = message_in.getValueLLSD("native_key_data");
			std::string event = message_in.getValue("event");
			// Treat unknown events as key-up for safety.
			dullahan::EKeyEvent key_event = dullahan::KE_KEY_UP;
			if (event == "down")
			{
				key_event = dullahan::KE_KEY_DOWN;
			}
			else if (event == "repeat")
			{
				key_event = dullahan::KE_KEY_REPEAT;
			}
			keyEvent(key_event, native_key_data);
		}
		else if (message_name == "enable_media_plugin_debugging")
		{
			mEnableMediaPluginDebugging = message_in.getValueBoolean("enable");
		}
		else if (message_name == "pick_file_response")
		{
			mPickedFile = message_in.getValue("file");
			LLSD file_list = message_in.getValueLLSD("file_list");
			for (LLSD::array_const_iterator iter = file_list.beginArray(),
											end = file_list.endArray();
				 iter != end; ++iter)
			{
				mPickedFiles.emplace_back((*iter).asString());
			}
			if (mPickedFiles.empty() && !mPickedFile.empty())
			{
				mPickedFiles.emplace_back(mPickedFile);
			}
		}
		else if (message_name == "auth_response")
		{
			authResponse(message_in);
		}
		else if (message_name == "edit_copy")
		{
			mCEFLib->editCopy();
		}
		else if (message_name == "edit_cut")
		{
			mCEFLib->editCut();
		}
		else if (message_name == "edit_paste")
		{
			mCEFLib->editPaste();
		}
	}
	else if (message_class == LLPLUGIN_MESSAGE_CLASS_MEDIA_BROWSER)
	{
		if (message_name == "set_page_zoom_factor")
		{
			F32 factor = (F32)message_in.getValueReal("factor");
			mCEFLib->setPageZoom(factor);
		}
		else if (message_name == "proxy_setup")
		{
			mProxyEnabled = message_in.getValueBoolean("enable");
			mProxyHost = message_in.getValue("host");
			mProxyPort = (U16)message_in.getValueS32("port");
		}
		else if (message_name == "cookies_enabled")
		{
			mCookiesEnabled = message_in.getValueBoolean("enable");
		}
		else if (message_name == "show_web_inspector")
		{
			mCEFLib->showDevTools();
		}
#if CHROME_VERSION_MAJOR < 100
		else if (message_name == "plugins_enabled")
		{
			mPluginsEnabled = message_in.getValueBoolean("enable");
		}
#endif
		else if (message_name == "javascript_enabled")
		{
			mJavascriptEnabled = message_in.getValueBoolean("enable");
		}
		else if (message_name == "minimum_font_size")
		{
			mMinimumFontSize = message_in.getValueU32("size");
		}
		else if (message_name == "default_font_size")
		{
			mDefaultFontSize = message_in.getValueU32("size");
		}
		else if (message_name == "remote_fonts")
		{
			mRemoteFonts = message_in.getValueBoolean("enable");
		}
		else if (message_name == "preferred_font")
		{
			mPreferredFont = message_in.getValue("font_family");
		}
		else if (message_name == "browse_stop")
		{
			mCEFLib->stop();
		}
		else if (message_name == "browse_reload")
		{
			bool ignore_cache = true;
			mCEFLib->reload(ignore_cache);
		}
		else if (message_name == "browse_forward")
		{
			mCEFLib->goForward();
		}
		else if (message_name == "browse_back")
		{
			mCEFLib->goBack();
		}
		else if (message_name == "clear_cookies")
		{
			mCEFLib->deleteAllCookies();
		}
		else if (message_name == "set_user_agent")
		{
			mUserAgent = message_in.getValue("user_agent");
		}
	}
	else if (message_class == LLPLUGIN_MESSAGE_CLASS_MEDIA_TIME)
	{
		if (message_name == "set_volume")
		{
			mCurVolume = (F32)message_in.getValueReal("volume");
			setVolume();
		}
	}
	else if (mEnableMediaPluginDebugging)
	{
		std::cerr << "MediaPluginCEF::receiveMessage: unknown message class: "
				  << message_class << std::endl;
	}
}

void MediaPluginCEF::keyEvent(dullahan::EKeyEvent key_event,
							  LLSD native_key_data)
{
#if LL_DARWIN
	U32 event_modifiers = native_key_data["event_modifiers"].asInteger();
	U32 event_keycode = native_key_data["event_keycode"].asInteger();
	U32 event_chars = native_key_data["event_chars"].asInteger();
	U32 event_umodchars = native_key_data["event_umodchars"].asInteger();
	bool event_isrepeat = native_key_data["event_isrepeat"].asBoolean();
	// Adding new code below in unicodeInput means we do not send ASCII chars
	// here too or we get double key presses on a Mac.
	bool tab_key_up = event_umodchars == 9 &&
					  key_event == dullahan::EKeyEvent::KE_KEY_UP;
	if (!tab_key_up &&
		 (event_umodchars == 27 || (unsigned char)event_chars < 0x10 ||
		 (unsigned char)event_chars >= 0x7f))
	{
		mCEFLib->nativeKeyboardEventOSX(key_event, event_modifiers, 
										event_keycode, event_chars, 
										event_umodchars, event_isrepeat);
	}
#elif LL_WINDOWS
	U32 msg = ll_U32_from_sd(native_key_data["msg"]);
	U32 wparam = ll_U32_from_sd(native_key_data["w_param"]);
	U64 lparam = ll_U32_from_sd(native_key_data["l_param"]);
	mCEFLib->nativeKeyboardEventWin(msg, wparam, lparam);
#elif LL_LINUX
	U32 native_virtual_key = native_key_data["virtual_key"].asInteger();
	if (native_virtual_key == (U32)'\n')
	{
		native_virtual_key = (U32)'\r';
	}
	U32 native_modifiers = native_key_data["sdl_modifiers"].asInteger();
	if (mEnableMediaPluginDebugging)
	{
		std::cerr << "MediaPluginCEF::keyEvent: key_event = " << key_event
				  << " - native_virtual_key =  " << native_virtual_key
				  << " - native_modifiers =  " << native_modifiers
				  << std::endl;
	}

	mCEFLib->nativeKeyboardEventLin2(key_event, native_virtual_key,
									 native_modifiers, false);

	if (key_event == dullahan::KE_KEY_UP && native_virtual_key == (U32)'\r')
	{
		// *HACK: to have CEF honor enter (e.g. to accept form input), in
		// excess of sending KE_KEY_UP/DOWN we must send a KE_KEY_CHAR event.
		mCEFLib->nativeKeyboardEventLin2(dullahan::KE_KEY_CHAR,
										 native_virtual_key, native_modifiers,
										 false);
	}
#endif
}

void MediaPluginCEF::unicodeInput(std::string event, LLSD native_key_data)
{
#if LL_DARWIN
	// I did not think this code was needed for MacOS but without it, the IME
	// input in japanese (and likely others too) does not work correctly.
	// see maint-7654
	U32 event_modifiers = native_key_data["event_modifiers"].asInteger();
	U32 event_keycode = native_key_data["event_keycode"].asInteger();
	U32 event_chars = native_key_data["event_chars"].asInteger();
	U32 event_umodchars = native_key_data["event_umodchars"].asInteger();
	bool is_repeat = native_key_data["event_isrepeat"].asBoolean();
    dullahan::EKeyEvent key_event = event == "down" ? dullahan::KE_KEY_DOWN
													: dullahan::KE_KEY_UP;
	mCEFLib->nativeKeyboardEventOSX(key_event, event_modifiers, 
									event_keycode, event_chars, 
									event_umodchars, is_repeat);
#elif LL_WINDOWS
	event = ""; // not needed here but prevents unused var warning as error
	U32 msg = ll_U32_from_sd(native_key_data["msg"]);
	U32 wparam = ll_U32_from_sd(native_key_data["w_param"]);
	U64 lparam = ll_U32_from_sd(native_key_data["l_param"]);
	mCEFLib->nativeKeyboardEventWin(msg, wparam, lparam);
#elif LL_LINUX && HB_DULLAHAN_EXTENDED
	U32 native_virtual_key = native_key_data["virtual_key"].asInteger();
	if (native_virtual_key == (U32)'\n')
	{
		native_virtual_key = (U32)'\r';
	}
	U32 native_modifiers = native_key_data["sdl_modifiers"].asInteger();
	if (mEnableMediaPluginDebugging)
	{
		std::cerr << "MediaPluginCEF::keyEvent: native_scan_code =  "
				  << native_virtual_key << " - native_modifiers =  "
				  << native_modifiers << std::endl;
	}
	mCEFLib->nativeKeyboardEventLin2(dullahan::KE_KEY_CHAR, native_virtual_key,
									 native_modifiers, false);
#endif
}

void MediaPluginCEF::checkEditState()
{
	bool can_copy = mCEFLib->editCanCopy();
	bool can_cut = mCEFLib->editCanCut();
	bool can_paste = mCEFLib->editCanPaste();

	if (can_copy != mCanCopy || can_cut != mCanCut || can_paste != mCanPaste)
	{
		LLPluginMessage message(LLPLUGIN_MESSAGE_CLASS_MEDIA, "edit_state");

		if (can_copy != mCanCopy)
		{
			mCanCopy = can_copy;
			message.setValueBoolean("copy", can_copy);
		}
		if (can_cut != mCanCut)
		{
			mCanCut = can_cut;
			message.setValueBoolean("cut", can_cut);
		}
		if (can_paste != mCanPaste)
		{
			mCanPaste = can_paste;
			message.setValueBoolean("paste", can_paste);
		}
	}
}

void MediaPluginCEF::setVolume()
{
	mVolumeCatcher.setVolume(mCurVolume);
}

bool MediaPluginCEF::init()
{
	LLPluginMessage message(LLPLUGIN_MESSAGE_CLASS_MEDIA, "name_text");
	message.setValue("name", "CEF Plugin");
	sendMessage(message);

	return true;
}

int init_media_plugin(LLPluginInstance::sendMessageFunction host_send_func,
					  void* host_user_data,
					  LLPluginInstance::sendMessageFunction* plugin_send_func,
					  void** plugin_user_data)
{
	MediaPluginCEF* self = new MediaPluginCEF(host_send_func, host_user_data);
	*plugin_send_func = MediaPluginCEF::staticReceiveMessage;
	*plugin_user_data = (void*)self;

	return 0;
}
