/**
 * @file media_plugin_gstreamer.cpp
 * @brief GStreamer-1.0 plugin for LLMedia API plugin system
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 *
 * Copyright (c) 2007-2009, Linden Research, Inc.
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

#include "llgl.h"

#include "llplugininstance.h"
#include "llpluginmessage.h"
#include "llpluginmessageclasses.h"
#include "media_plugin_base.h"
#include "volume_catcher.h"

#define G_DISABLE_CAST_CHECKS

#if LL_CLANG
// When using C++11, clang warns about the "register" type usage in gst/gst.h
// (actually stemming from glib-2.0/gobject/gtype.h), while this is a C
// header (and gcc is fine with it)... And with C++17, clang just errors out...
// So, let's nullify the "register" keyword before including the headers. HB
# define register
#endif

#include "gst/gst.h"
#include "gst/app/gstappsink.h"

#include "llmediaimplgstreamer.h"
#include "llmediaimplgstreamer_syms.h"

constexpr double MIN_LOOP_SEC = 1.0F;
constexpr U32 INTERNAL_TEXTURE_SIZE = 1024;

#if 0
struct LLStreamMetadata
{
	std::string mArtist;
	std::string mTitle;
};

// Extract stream metadata so we can report back into the client what is playing
static void extract_metadata(const GstTagList* list, const gchar* tag,
							 gpointer user_data)
{
	if (!user_data) return;

	LLStreamMetadata* outp = reinterpret_cast<LLStreamMetadata*>(user_data);
	std::string* outstrp = NULL;
	if (strcmp(tag, "title") == 0)
	{
		outstrp = &outp->mTitle;
	}
	else if (strcmp(tag, "artist") == 0)
	{
		outstrp = &outp->mArtist;
	}
	if (!outstrp) return;

	for (int i = 0, num = llgst_tag_list_get_tag_size(list, tag); i < num; ++i)
	{
		const GValue* val = llgst_tag_list_get_value_index(list, tag, i);
		if (G_VALUE_HOLDS_STRING(val))
		{
			outstrp->assign(g_value_get_string(val));
		}
	}
}
#endif

LL_INLINE static void llgst_caps_unref(GstCaps* caps)
{
	llgst_mini_object_unref(GST_MINI_OBJECT_CAST(caps));
}

LL_INLINE static void llgst_sample_unref(GstSample* sample)
{
	llgst_mini_object_unref(GST_MINI_OBJECT_CAST(sample));
}

class MediaPluginGStreamer : public MediaPluginBase
{
public:
	MediaPluginGStreamer(LLPluginInstance::sendMessageFunction host_send_func,
						   void* host_user_data);
	~MediaPluginGStreamer() override;

	void receiveMessage(const char* message_string) override;

	static bool startup();
	static bool closedown();

	gboolean processGSTEvents(GstBus* bus, GstMessage* message);

private:
	bool load();
	bool unload();

	std::string getVersion();
	bool navigateTo(const std::string url);
	bool seek(double time_sec);
	bool setVolume(float volume);

	bool pause();
	bool stop();
	bool play(double rate);
	bool getTimePos(double& sec_out);

	bool update(int milliseconds);
	void mouseDown(int x, int y);
	void mouseUp(int x, int y);
	void mouseMove(int x, int y);

private:
	// Very GStreamer-specific
	GMainLoop*		mPump;			// event pump for this media
	GstElement*		mPlaybin;
	GstAppSink*		mAppSink;

	VolumeCatcher	mVolumeCatcher;

	enum ECommand {
		COMMAND_NONE,
		COMMAND_STOP,
		COMMAND_PLAY,
		COMMAND_FAST_FORWARD,
		COMMAND_FAST_REWIND,
		COMMAND_PAUSE,
		COMMAND_SEEK,
	};
	ECommand		mCommand;

	guint			mBusWatchID;

	float			mVolume;

	int				mDepth;

	// padded texture size we need to write into
	int				mTextureWidth;
	int				mTextureHeight;

	double			mSeekDestination;
	bool			mSeekWanted;

	bool			mIsLooping;

	bool			mEnableMediaPluginDebugging;

	static bool		mDoneInit;
};

//static
bool MediaPluginGStreamer::mDoneInit = false;

MediaPluginGStreamer::MediaPluginGStreamer(LLPluginInstance::sendMessageFunction host_send_func,
										   void* host_user_data)
:	MediaPluginBase(host_send_func, host_user_data),
	mBusWatchID(0),
	mSeekWanted(false),
	mIsLooping(false),
	mSeekDestination(0.0),
	mPump(NULL),
	mPlaybin(NULL),
	mAppSink(NULL),
	mCommand(COMMAND_NONE),
	mEnableMediaPluginDebugging(false)
{
	// Ensure the volume is set at maximum in the system mixer: the plugin got
	// its own volume control.
	mVolumeCatcher.setVolume(1.f);
}

gboolean MediaPluginGStreamer::processGSTEvents(GstBus* bus,
												GstMessage* message)
{
	if (!message)
	{
		return TRUE; // shield against GStreamer bug
	}

	switch (GST_MESSAGE_TYPE(message))
	{
		case GST_MESSAGE_BUFFERING:
		{
			if (llgst_message_parse_buffering)
			{
				gint percent = 0;
				llgst_message_parse_buffering(message, &percent);
			}
			break;
		}

		case GST_MESSAGE_STATE_CHANGED:
		{
			GstState old_state;
			GstState new_state;
			GstState pending_state;
			llgst_message_parse_state_changed(message, &old_state, &new_state,
											  &pending_state);
			switch (new_state)
			{
				case GST_STATE_VOID_PENDING:
				case GST_STATE_NULL:
					break;

				case GST_STATE_READY:
					setStatus(STATUS_LOADED);
					break;

				case GST_STATE_PAUSED:
					setStatus(STATUS_PAUSED);
					break;

				case GST_STATE_PLAYING:
					setStatus(STATUS_PLAYING);
			}
			break;
		}
#if 0
		case GST_MESSAGE_TAG:
		{
			LLStreamMetadata mdata;
			GstTagList* tags = NULL;
			llgst_message_parse_tag(message, &tags);
			llgst_tag_list_foreach(tags, extract_metadata, &mdata);
			llgst_tag_list_unref(tags);
		
			LLPluginMessage message(LLPLUGIN_MESSAGE_CLASS_MEDIA, "name_text");
			message.setValue("name", mdata.mTitle);
			message.setValue("artist", mdata.mArtist);
			sendMessage(message);
			break;
		}
#endif
		case GST_MESSAGE_ERROR:
		{
			GError* err = NULL;
			gchar* debug = NULL;
			llgst_message_parse_error(message, &err, &debug);
			if (err)
			{
				llg_error_free(err);
			}
			llg_free(debug);

			mCommand = COMMAND_STOP;
			setStatus(STATUS_ERROR);
			break;
		}

		case GST_MESSAGE_INFO:
		{
			if (llgst_message_parse_info)
			{
				GError* err = NULL;
				gchar* debug = NULL;
				llgst_message_parse_info(message, &err, &debug);
				if (err)
				{
					llg_error_free(err);
				}
				llg_free(debug);
			}
			break;
		}

		case GST_MESSAGE_WARNING:
		{
			GError* err = NULL;
			gchar* debug = NULL;
			llgst_message_parse_info(message, &err, &debug);
			if (err)
			{
				llg_error_free(err);
			}
			llg_free(debug);

			break;
		}

		case GST_MESSAGE_EOS:	// end-of-stream
		{
			if (mIsLooping)
			{
				double eos_pos_sec = 0.0F;
				bool got_eos_position = getTimePos(eos_pos_sec);
				if (got_eos_position && eos_pos_sec < MIN_LOOP_SEC)
				{
					// If we know that the movie is really short, do not loop
					// it else it can easily become a time-hog because of
					// GStreamer spin-up overhead; inject a COMMAND_PAUSE
					mCommand = COMMAND_PAUSE;
				}
				else
				{
					stop();
					play(1.0);
				}
			}
			else // not a looping media
			{
				// Inject a COMMAND_STOP
				mCommand = COMMAND_STOP;
			}
			break;
		}

		default:				// Unhandled message
			break;
	}

	// We want to be notified again the next time there is a message on the
	// bus, so return true (false means we want to stop watching for messages
	// on the bus and our callback should not be called again)
	return TRUE;
}

extern "C" {
gboolean llmediaimplgstreamer_bus_callback(GstBus* bus, GstMessage* message,
										   gpointer data)
{
	MediaPluginGStreamer* impl = (MediaPluginGStreamer*)data;
	return impl && impl->processGSTEvents(bus, message);
}
} // extern "C"

bool MediaPluginGStreamer::navigateTo(const std::string url)
{
	if (!mDoneInit)
	{
		return false; // Error
	}

	setStatus(STATUS_LOADING);

	mSeekWanted = false;

	if (!mPump || !mPlaybin)
	{
		setStatus(STATUS_ERROR);
		return false; // Error
	}

	// Send a "navigate begin" event. This is really a browser message but the
	// QuickTime plugin does it and the media system relies on this message to
	// update internal state so we must send it too.
	// Note: see "navigate_complete" message below too.
	LLPluginMessage message_begin(LLPLUGIN_MESSAGE_CLASS_MEDIA_BROWSER,
								  "navigate_begin");
	message_begin.setValue("uri", url);
	message_begin.setValueBoolean("history_back_available", false);
	message_begin.setValueBoolean("history_forward_available", false);
	sendMessage(message_begin);

	llg_object_set(G_OBJECT(mPlaybin), "uri", url.c_str(), NULL);

	// navigateTo implicitly plays, too.
	play(1.0);

	// Send a "location_changed" message; this informs the media system that a
	// new URL is the 'current' one and is used extensively. Again, this is
	// really a browser message but we will use it here.
	LLPluginMessage message(LLPLUGIN_MESSAGE_CLASS_MEDIA_BROWSER,
							"location_changed");
	message.setValue("uri", url);
	sendMessage(message);

	// Send a "navigate complete" event.
	LLPluginMessage message_complete(LLPLUGIN_MESSAGE_CLASS_MEDIA_BROWSER,
									 "navigate_complete");
	message_complete.setValue("uri", url);
	message_complete.setValueS32("result_code", 200);
	message_complete.setValue("result_string", "OK");
	sendMessage(message_complete);

	return true;
}

class GstSampleUnref
{
public:
	GstSampleUnref(GstSample* aT)
	:	mT(aT)
	{
		llassert_always(mT);
	}

	~GstSampleUnref()
	{
		llgst_sample_unref(mT);
	}

private:
	GstSample* mT;
};

bool MediaPluginGStreamer::update(int milliseconds)
{
	if (!mDoneInit)
	{
		return false; // error
	}

	// Sanity check
	if (!mPump || !mPlaybin)
	{
		return false;
	}

	// See if there is an outstanding seek wanted
	if (mSeekWanted &&
		// Bleh, GST has to be happy that the movie is really truly playing or
		// it may quietly ignore the seek (with rtsp:// at least).
	    GST_STATE(mPlaybin) == GST_STATE_PLAYING)
	{
		seek(mSeekDestination);
		mSeekWanted = false;
	}

	// *TODO: time-limit - but there is not a lot we can do here, most time is
	// spent in gstreamer's own opaque worker-threads. Maybe we can do
	// something sneaky like only unlock the video object for 'milliseconds'
	// and otherwise hold the lock.
	while (llg_main_context_pending(llg_main_loop_get_context(mPump)))
	{
		llg_main_context_iteration(llg_main_loop_get_context(mPump), FALSE);
	}

	// Check for availability of a new frame

	if (!mAppSink)
	{
		return true;
	}

	// Do not try to pull a sample if not in playing state
	if (GST_STATE(mPlaybin) != GST_STATE_PLAYING)
	{
		return true;
	}

	GstSample* samplep = llgst_app_sink_pull_sample(mAppSink);
	if (!samplep)
	{
		return false; // Done playing
	}

	GstSampleUnref oSampleUnref(samplep);

	GstCaps* capsp = llgst_sample_get_caps(samplep);
	if (!capsp)
	{
		return false;
	}

	gint width, height;
	GstStructure* pStruct = llgst_caps_get_structure(capsp, 0);

	llgst_structure_get_int(pStruct, "width", &width);
	llgst_structure_get_int(pStruct, "height", &height);

	if (!mPixels)
	{
		return true;
	}

	GstBuffer* bufferp = llgst_sample_get_buffer(samplep);
	GstMapInfo map;
	llgst_buffer_map(bufferp, &map, GST_MAP_READ);

	// Our render buffer is always INTERNAL_TEXTURE_SIZExINTERNAL_TEXTURE_SIZE
	// Downsample if the viewer requested a smaller texture (zoomed out).
	// *TODO: investigate how to use a gstreamer scale filter rather than this
	// quick and dirty scale, which will not give any nice results when scaling
	// very low.
	U32 row_skip = INTERNAL_TEXTURE_SIZE / mTextureHeight;
	U32 col_skip = INTERNAL_TEXTURE_SIZE / mTextureWidth;

	U8* dest = mPixels + (mTextureHeight - 1) * mTextureWidth * mDepth;
	for (int row = 0; row < mTextureHeight; ++row)
	{
		const U8* texel_in = map.data + row * row_skip * width * 3;
		U8* texel_out = dest - row * mTextureWidth * mDepth;
		for (int col = 0; col < width && col < mTextureWidth; ++col)
		{
			texel_out[0] = texel_in[0];
			texel_out[1] = texel_in[1];
			texel_out[2] = texel_in[2];
			texel_out += mDepth;
			texel_in += col_skip * 3;
		}
	}

	llgst_buffer_unmap(bufferp, &map);
	setDirty(0, 0, mTextureWidth, mTextureHeight);

	return true;
}

void MediaPluginGStreamer::mouseDown(int x, int y)
{
	// Do nothing
}

void MediaPluginGStreamer::mouseUp(int x, int y)
{
	// Do nothing
}

void MediaPluginGStreamer::mouseMove(int x, int y)
{
	// Do nothing
}

bool MediaPluginGStreamer::pause()
{
	// *TODO: error-check this ?
	if (mDoneInit && mPlaybin)
	{
		llgst_element_set_state(mPlaybin, GST_STATE_PAUSED);
		return true;
	}
	return false;
}

bool MediaPluginGStreamer::stop()
{
	// *TODO: error-check this ?
	if (mDoneInit && mPlaybin)
	{
		llgst_element_set_state(mPlaybin, GST_STATE_READY);
		return true;
	}
	return false;
}

// NOTE: we do not actually support non-natural rate.
bool MediaPluginGStreamer::play(double rate)
{
	// *TODO: error-check this ?
	if (mDoneInit && mPlaybin)
	{
		llgst_element_set_state(mPlaybin, GST_STATE_PLAYING);
		return true;
	}
	return false;
}

bool MediaPluginGStreamer::setVolume(float volume)
{
	// We try to only update volume as conservatively as possible, as many
	// gst-plugins-base versions up to at least November 2008 have critical
	// race-conditions in setting volume - sigh
	if (mVolume == volume)
	{
		return true;	// Nothing to do, everything is fine
	}

	mVolume = volume;
	if (mDoneInit && mPlaybin)
	{
		llg_object_set(mPlaybin, "volume", mVolume, NULL);
		return true;
	}

	return false;
}

bool MediaPluginGStreamer::seek(double time_sec)
{
	bool success = false;
	if (mDoneInit && mPlaybin)
	{
		success = llgst_element_seek(mPlaybin, 1.0F, GST_FORMAT_TIME,
									 GstSeekFlags(GST_SEEK_FLAG_FLUSH |
												  GST_SEEK_FLAG_KEY_UNIT),
									 GST_SEEK_TYPE_SET,
									 gint64(time_sec * GST_SECOND),
									 GST_SEEK_TYPE_NONE, GST_CLOCK_TIME_NONE);
	}
	return success;
}

bool MediaPluginGStreamer::getTimePos(double& sec_out)
{
	bool got_position = false;

	if (mDoneInit && mPlaybin)
	{
		gint64 pos = 0;
		GstFormat timefmt = GST_FORMAT_TIME;
		got_position = llgst_element_query_position &&
					   llgst_element_query_position(mPlaybin, &timefmt, &pos);
		got_position &= timefmt == GST_FORMAT_TIME;
		// GStreamer may have other ideas, but we consider the current position
		// undefined if not PLAYING or PAUSED
		got_position &= GST_STATE(mPlaybin) == GST_STATE_PLAYING ||
						GST_STATE(mPlaybin) == GST_STATE_PAUSED;
		if (got_position && !GST_CLOCK_TIME_IS_VALID(pos))
		{
			if (GST_STATE(mPlaybin) == GST_STATE_PLAYING)
			{
				// If we are playing then we treat an invalid clock time as 0,
				// for complicated reasons (insert reason here)
				pos = 0;
			}
			else
			{
				got_position = false;
			}

		}
		// If all the preconditions succeeded... we can trust the result.
		if (got_position)
		{
			sec_out = double(pos) / double(GST_SECOND); // gst to sec
		}
	}

	return got_position;
}

bool MediaPluginGStreamer::load()
{
	if (!mDoneInit)
	{
		return false; // error
	}

	setStatus(STATUS_LOADING);

	mIsLooping = false;
	mVolume = 0.1234567f; // Minor hack to force an initial volume update

	// Create a pumpable main-loop for this media
	mPump = llg_main_loop_new(NULL, FALSE);
	if (!mPump)
	{
		setStatus(STATUS_ERROR);
		return false; // error
	}

	// Instantiate a playbin element to do the hard work
	mPlaybin = llgst_element_factory_make("playbin", "");
	if (!mPlaybin)
	{
		setStatus(STATUS_ERROR);
		return false; // error
	}

	// get playbin's bus
	GstBus* bus = llgst_pipeline_get_bus(GST_PIPELINE (mPlaybin));
	if (!bus)
	{
		setStatus(STATUS_ERROR);
		return false; // error
	}
	mBusWatchID = llgst_bus_add_watch(bus, llmediaimplgstreamer_bus_callback,
									  this);
	llgst_object_unref(bus);

	mAppSink = (GstAppSink*)llgst_element_factory_make("appsink", "");

	GstCaps* capsp = llgst_caps_new_simple("video/x-raw", "format",
										   G_TYPE_STRING, "RGB",
										   "width", G_TYPE_INT,
										   INTERNAL_TEXTURE_SIZE,
										   "height", G_TYPE_INT,
										   INTERNAL_TEXTURE_SIZE,
										   NULL);
	llgst_app_sink_set_caps(mAppSink, capsp);
	llgst_caps_unref(capsp);

	if (!mAppSink)
	{
		setStatus(STATUS_ERROR);
		return false;
	}

	llg_object_set(mPlaybin, "video-sink", mAppSink, NULL);

	return true;
}

bool MediaPluginGStreamer::unload()
{
	if (!mDoneInit)
	{
		return false; // error
	}

	// Stop getting callbacks for this bus
	llg_source_remove(mBusWatchID);
	mBusWatchID = 0;

	if (mPlaybin)
	{
		llgst_element_set_state(mPlaybin, GST_STATE_NULL);
		llgst_object_unref(GST_OBJECT (mPlaybin));
		mPlaybin = NULL;
	}

	if (mPump)
	{
		llg_main_loop_quit(mPump);
		mPump = NULL;
	}

	mAppSink = NULL;

	setStatus(STATUS_NONE);

	return true;
}

void LogFunction(GstDebugCategory* category, GstDebugLevel level,
				 const gchar* file, const gchar* function, gint line,
				 GObject* object, GstDebugMessage* message,
				 gpointer user_data)
{
	std::cerr << file << ": " << line << "(" << function << "): "
			  << llgst_debug_message_get(message) << std::endl;
}

//static
bool MediaPluginGStreamer::startup()
{
	// Only do global GStreamer initialization once.
	if (mDoneInit)
	{
		return true;
	}

	ll_init_apr();

	// Get symbols
	std::vector<std::string> dso_names;
#if LL_WINDOWS
	dso_names.push_back("libgstreamer-1.0-0.dll");
	dso_names.push_back("libgstapp-1.0-0.dll");
	dso_names.push_back("libglib-2.0-0.dll");
	dso_names.push_back("libgobject-2.0-0.dll");
#elif LL_DARWIN
	dso_names.push_back("libgstreamer-1.0.0.dylib");
	dso_names.push_back("libgstapp-1.0.0.dylib");
	dso_names.push_back("libglib-2.0.0.dylib");
	dso_names.push_back("libgobject-2.0.0.dylib");
#else // Linux or other ELFy unixoid
	dso_names.push_back("libgstreamer-1.0.so.0");
	dso_names.push_back("libgstapp-1.0.so.0");
	dso_names.push_back("libglib-2.0.so.0");
	dso_names.push_back("libgobject-2.0.so.0");
#endif
	if (!grab_gst_syms(dso_names))
	{
		return false;
	}

	if (llgst_segtrap_set_enabled)
	{
		llgst_segtrap_set_enabled(FALSE);
	}
#if LL_LINUX
	// Gstreamer tries a fork during init, waitpid-ing on it, which conflicts
	// with any installed SIGCHLD handler...
	struct sigaction tmpact, oldact;
	if (llgst_registry_fork_set_enabled)
	{
		// If we can disable SIGCHLD-using forking behaviour, do it.
		llgst_registry_fork_set_enabled(FALSE);
	}
	else
	{
		// Else temporarily install default SIGCHLD handler while GStreamer
		// initialises
		tmpact.sa_handler = SIG_DFL;
		sigemptyset(&tmpact.sa_mask);
		tmpact.sa_flags = SA_SIGINFO;
		sigaction(SIGCHLD, &tmpact, &oldact);
	}
#endif
	// Protect against GStreamer resetting the locale, yuck.
	static std::string saved_locale;
	saved_locale = setlocale(LC_ALL, NULL);

	llgst_debug_set_default_threshold(GST_LEVEL_WARNING);
	llgst_debug_add_log_function(LogFunction, NULL, NULL);
	llgst_debug_set_active(FALSE);

	// Finally, try to initialize GStreamer !
	GError* err = NULL;
	gboolean init_gst_success = llgst_init_check(NULL, NULL, &err);

	// Restore old locale
	setlocale(LC_ALL, saved_locale.c_str());

#if LL_LINUX
	// Restore old SIGCHLD handler
	if (!llgst_registry_fork_set_enabled)
	{
		sigaction(SIGCHLD, &oldact, NULL);
	}
#endif

	if (!init_gst_success)	// Failed
	{
		if (err)
		{
			llg_error_free(err);
		}
		return false;
	}

	mDoneInit = true;

	return true;
}

//static
bool MediaPluginGStreamer::closedown()
{
	if (!mDoneInit)
	{
		return false;	// Error
	}

	ungrab_gst_syms();

	mDoneInit = false;

	return true;
}

MediaPluginGStreamer::~MediaPluginGStreamer()
{
	closedown();
}

std::string MediaPluginGStreamer::getVersion()
{
	std::string plugin_version = "GStreamer media plugin, GStreamer version ";
	if (mDoneInit && llgst_version)
	{
		guint major, minor, micro, nano;
		llgst_version(&major, &minor, &micro, &nano);
		plugin_version += llformat("%u.%u.%u.%u (runtime), %u.%u.%u.%u (headers)",
								   (unsigned int)major, (unsigned int)minor,
								   (unsigned int)micro, (unsigned int)nano,
								   (unsigned int)GST_VERSION_MAJOR,
								   (unsigned int)GST_VERSION_MINOR,
								   (unsigned int)GST_VERSION_MICRO,
								   (unsigned int)GST_VERSION_NANO);
	}
	else
	{
		plugin_version += "(unknown)";
	}
	return plugin_version;
}

void MediaPluginGStreamer::receiveMessage(const char* message_string)
{
#if LL_DEBUG	// Very spammy...
	if (mEnableMediaPluginDebugging)
	{
		std::cerr << "MediaPluginGStreamer::receiveMessage: received message: \""
				  << message_string << "\"" << std::endl;
	}
#endif

	LLPluginMessage message_in;

	if (message_in.parse(message_string) >= 0)
	{
		std::string message_class = message_in.getClass();
		std::string message_name = message_in.getName();

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
				versions[LLPLUGIN_MESSAGE_CLASS_MEDIA_TIME] =
					LLPLUGIN_MESSAGE_CLASS_MEDIA_TIME_VERSION;
				message.setValueLLSD("versions", versions);

				load();

				message.setValue("plugin_version", getVersion());
				sendMessage(message);
			}
			else if (message_name == "idle")
			{
				// No response is necessary here.
				double time = message_in.getValueReal("time");

				// Convert time to milliseconds for update()
				update((int)(time * 1000.0f));
#if 0			// Not really used for now...
				mVolumeCatcher.pump();
#endif
			}
			else if (message_name == "cleanup")
			{
				unload();
				closedown();
				LLPluginMessage message("base", "goodbye");
				sendMessage(message);
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

				mSharedSegments.insert(SharedSegmentMap::value_type(name,
																	info));
			}
			else if (message_name == "shm_remove")
			{
				std::string name = message_in.getValue("name");

				SharedSegmentMap::iterator iter = mSharedSegments.find(name);
				if (iter != mSharedSegments.end())
				{
					if (mPixels == iter->second.mAddress)
					{
						// This is the currently active pixel buffer. Make sure
						// we stop drawing to it.
						mPixels = NULL;
						mTextureSegmentName.clear();
					}
					mSharedSegments.erase(iter);
				}

				// Send the response so it can be cleaned up.
				LLPluginMessage message("base", "shm_remove_response");
				message.setValue("name", name);
				sendMessage(message);
			}
		}
		else if (message_class == LLPLUGIN_MESSAGE_CLASS_MEDIA)
		{
			if (message_name == "init")
			{
				// Plugin gets to decide the texture parameters to use.
				LLPluginMessage message(LLPLUGIN_MESSAGE_CLASS_MEDIA,
										"texture_params");
				// Lame to have to decide this now, it depends on the movie.
				// Oh well.
				mDepth = 4;

				mTextureWidth = 1;
				mTextureHeight = 1;

				message.setValueU32("format", GL_RGBA);
				message.setValueU32("type", GL_UNSIGNED_INT_8_8_8_8_REV);

				message.setValueS32("depth", mDepth);
				message.setValueS32("default_width", INTERNAL_TEXTURE_SIZE);
				message.setValueS32("default_height", INTERNAL_TEXTURE_SIZE);
				message.setValueU32("internalformat", GL_RGBA8);
				message.setValueBoolean("coords_opengl", true);
				// We respond with grace and performance if asked to downscale
				message.setValueBoolean("allow_downsample", true);
				sendMessage(message);
			}
			else if (message_name == "size_change")
			{
				std::string name = message_in.getValue("name");
				S32 width = message_in.getValueS32("width");
				S32 height = message_in.getValueS32("height");
				S32 texture_width = message_in.getValueS32("texture_width");
				S32 texture_height = message_in.getValueS32("texture_height");

				LLPluginMessage message(LLPLUGIN_MESSAGE_CLASS_MEDIA,
										"size_change_response");
				message.setValue("name", name);
				message.setValueS32("width", width);
				message.setValueS32("height", height);
				message.setValueS32("texture_width", texture_width);
				message.setValueS32("texture_height", texture_height);
				sendMessage(message);

				if (!name.empty())
				{
					// Find the shared memory region with this name
					SharedSegmentMap::iterator it = mSharedSegments.find(name);
					if (it != mSharedSegments.end())
					{
						mTextureSegmentName = name;
						mTextureWidth = texture_width;
						mTextureHeight = texture_height;
						mPixels = (unsigned char*)it->second.mAddress;
						if (mPixels)
						{
							memset(mPixels, 0,
								   mTextureWidth * mTextureHeight * mDepth);
						}
					}

					LLPluginMessage message(LLPLUGIN_MESSAGE_CLASS_MEDIA,
											"size_change_request");
					message.setValue("name", mTextureSegmentName);
					message.setValueS32("width", INTERNAL_TEXTURE_SIZE);
					message.setValueS32("height", INTERNAL_TEXTURE_SIZE);
					sendMessage(message);
				}
			}
			else if (message_name == "load_uri")
			{
				std::string uri = message_in.getValue("uri");
				navigateTo(uri);
				sendStatus();
			}
			else if (message_name == "mouse_event")
			{
				std::string event = message_in.getValue("event");
				S32 x = message_in.getValueS32("x");
				S32 y = message_in.getValueS32("y");

				if (event == "down")
				{
					mouseDown(x, y);
				}
				else if (event == "up")
				{
					mouseUp(x, y);
				}
				else if (event == "move")
				{
					mouseMove(x, y);
				}
			}
			else if (message_name == "enable_media_plugin_debugging")
			{
				mEnableMediaPluginDebugging = message_in.getValueBoolean("enable");
				llgst_debug_set_active(mEnableMediaPluginDebugging);
			}
		}
		else if (message_class == LLPLUGIN_MESSAGE_CLASS_MEDIA_TIME)
		{
			if (message_name == "stop")
			{
				stop();
			}
			else if (message_name == "start")
			{
				double rate = 0.0;
				if (message_in.hasValue("rate"))
				{
					rate = message_in.getValueReal("rate");
				}
				// Note: we do not actually support rate.
				play(rate);
			}
			else if (message_name == "pause")
			{
				pause();
			}
			else if (message_name == "seek")
			{
				double time = message_in.getValueReal("time");
				// Defer the actual seek in case we have not really truly
				// started yet in which case there is nothing to seek upon.
				mSeekWanted = true;
				mSeekDestination = time;
			}
			else if (message_name == "set_loop")
			{
				bool loop = message_in.getValueBoolean("loop");
				mIsLooping = loop;
			}
			else if (message_name == "set_volume")
			{
				double volume = message_in.getValueReal("volume");
				setVolume(volume);
			}
		}
	}
}

int init_media_plugin(LLPluginInstance::sendMessageFunction host_send_func,
					  void* host_user_data,
					  LLPluginInstance::sendMessageFunction* plugin_send_func,
					  void** plugin_user_data)
{
	if (!MediaPluginGStreamer::startup())
	{
		return -1;	// Failed to init
	}

	MediaPluginGStreamer* self = new MediaPluginGStreamer(host_send_func,
															  host_user_data);
	*plugin_send_func = MediaPluginGStreamer::staticReceiveMessage;
	*plugin_user_data = (void*)self;

	return 0;	// Success
}
