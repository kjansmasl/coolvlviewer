/**
 * @file llpluginclassmedia.h
 * @brief LLPluginClassMedia handles interaction with a plugin which knows about the "media" message class.
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

#ifndef LL_LLPLUGINCLASSMEDIA_H
#define LL_LLPLUGINCLASSMEDIA_H

#include <memory>
#include <queue>

#include "llpluginclassmediaowner.h"
#include "llpluginprocessparent.h"
#include "llpreprocessor.h"
#include "llrect.h"
#include "llcolor4.h"

class LLPluginClassMedia final : public LLPluginProcessParentOwner
{
protected:
	LOG_CLASS(LLPluginClassMedia);

public:
	LLPluginClassMedia(LLPluginClassMediaOwner* owner);
	~LLPluginClassMedia() override;

	// Local initialization, called by the media manager when creating a source
	bool init(const std::string& launcher_filename,
			  const std::string& plugin_dir,
			  const std::string& plugin_filename, bool debug);

	// Un-does everything init() did; called by the media manager when
	// destroying a source
	void reset();

	void idle();

	// All of these may return 0 or an actual valid value. Callers need to
	// check the return for 0, and not use the values in that case.
	LL_INLINE int getWidth() const						{ return mMediaWidth > 0 ? mMediaWidth : 0; }
	LL_INLINE int getHeight() const						{ return mMediaHeight > 0 ? mMediaHeight : 0; }
	LL_INLINE int getNaturalWidth() const				{ return mNaturalMediaWidth; }
	LL_INLINE int getNaturalHeight() const				{ return mNaturalMediaHeight; }
	LL_INLINE int getSetWidth() const					{ return mSetMediaWidth; }
	LL_INLINE int getSetHeight() const					{ return mSetMediaHeight; }
	LL_INLINE int getBitsWidth() const					{ return mTextureWidth > 0 ? mTextureWidth : 0; }
	LL_INLINE int getBitsHeight() const					{ return mTextureHeight > 0 ? mTextureHeight : 0; }
	LL_INLINE int getFullWidth() const					{ return mFullMediaWidth; }
	LL_INLINE int getFullHeight() const					{ return mFullMediaHeight; }
	LL_INLINE F64 getZoomFactor() const					{ return mZoomFactor; }
	int getTextureWidth() const;
	int getTextureHeight() const;

	// This may return NULL. Callers need to check for and handle this case.
	unsigned char* getBitsData();

	// Get the format details of the texture data. These may return 0 if they
	// have not been set up yet. The caller needs to detect this case.
	LL_INLINE int getTextureDepth() const				{ return mRequestedTextureDepth; }
	LL_INLINE int getTextureFormatInternal() const		{ return mRequestedTextureInternalFormat; }
	LL_INLINE int getTextureFormatPrimary() const		{ return mRequestedTextureFormat; }
	LL_INLINE int getTextureFormatType() const			{ return mRequestedTextureType; }
	LL_INLINE bool getTextureFormatSwapBytes() const	{ return mRequestedTextureSwapBytes; }
	LL_INLINE bool getTextureCoordsOpenGL() const		{ return mRequestedTextureCoordsOpenGL; }

	void setSize(int width, int height);
	void setAutoScale(bool auto_scale);

	LL_INLINE void setZoomFactor(F64 f)					{ mZoomFactor = f; }

	LL_INLINE void setBackgroundColor(const LLColor4& c)
	{
		mBackgroundColor = c;
	}

	LL_INLINE void setOwner(LLPluginClassMediaOwner* o)	{ mOwner = o; }

	// Returns true if all of the texture parameters (depth, format, size, and
	// texture size) are set up and consistent. This will initially be false,
	// and will also be false for some time after setSize while the resize is
	// processed. Note that if this returns true, it is safe to use all the
	// get() functions above without checking for invalid return values until
	// you call idle() again.
	bool textureValid();

	bool getDirty(LLRect* dirty_rect = NULL);
	void resetDirty();

	typedef enum
	{
		MOUSE_EVENT_DOWN,
		MOUSE_EVENT_UP,
		MOUSE_EVENT_MOVE,
		MOUSE_EVENT_DOUBLE_CLICK
	} EMouseEventType;

	void mouseEvent(EMouseEventType type, int button, int x, int y,
					MASK modifiers);

	typedef enum
	{
		KEY_EVENT_DOWN,
		KEY_EVENT_UP,
		KEY_EVENT_REPEAT
	} EKeyEventType;

	bool keyEvent(EKeyEventType type, int key_code, MASK modifiers,
				  LLSD native_key_data);

	void scrollEvent(int x, int y, int clicks_x, int clicks_y, MASK modifiers);

	// Enable/disable media plugin debugging messages and info spam
	void enableMediaPluginDebugging(bool enable);

	// Javascript <-> viewer events
	void jsEnableObject(bool enable);
	void jsAgentLocationEvent(double x, double y, double z);
	void jsAgentGlobalLocationEvent(double x, double y, double z);
	void jsAgentOrientationEvent(double angle);
	void jsAgentLanguageEvent(const std::string& language);
	void jsAgentRegionEvent(const std::string& region_name);
	void jsAgentMaturityEvent(const std::string& maturity);

	// Text may be unicode (utf8 encoded)
	bool textInput(const std::string& text, MASK modifiers,
				   LLSD native_key_data);

	void setCookie(const std::string& uri, const std::string& name,
				   const std::string& value, const std::string& domain,
				   const std::string& path, bool httponly, bool secure);

	void loadURI(const std::string& uri);

	// "Loading" means uninitialized or any state prior to fully running
	// (processing commands)
	LL_INLINE bool isPluginLoading()					{ return mPlugin && mPlugin->isLoading(); }

	// "Running" means the steady state -- i.e. processing messages
	LL_INLINE bool isPluginRunning()					{ return mPlugin && mPlugin->isRunning(); }

	// "Exited" means any regular or error state after "Running" (plugin may
	// have crashed or exited normally)
	LL_INLINE bool isPluginExited()						{ return mPlugin && mPlugin->isDone(); }

	LL_INLINE std::string getPluginVersion()			{ return mPlugin ? mPlugin->getPluginVersion() : std::string(""); }

	LL_INLINE bool getDisableTimeout()					{ return mPlugin && mPlugin->getDisableTimeout(); }

	LL_INLINE void setDisableTimeout(bool disable)
	{
		if (mPlugin)
		{
			mPlugin->setDisableTimeout(disable);
		}
	}

	// Inherited from LLPluginProcessParentOwner
	void receivePluginMessage(const LLPluginMessage& message) override;
	void pluginLaunchFailed() override;
	void pluginDied() override;

	typedef enum
	{
		PRIORITY_UNLOADED,	// media plugin isn't even loaded.
		PRIORITY_STOPPED,	// media is not playing, shouldn't need to update at all.
		PRIORITY_HIDDEN,	// media is not being displayed or is out of view, don't need to do graphic updates, but may still update audio, playhead, etc.
		PRIORITY_SLIDESHOW,	// media is in the far distance, updates very infrequently
		PRIORITY_LOW,		// media is in the distance, may be rendered at reduced size
		PRIORITY_NORMAL,	// normal (default) priority
		PRIORITY_HIGH		// media has user focus and/or is taking up most of the screen
	} EPriority;

	static const char* priorityToString(EPriority priority);
	void setPriority(EPriority priority);
	void setLowPrioritySizeLimit(int size);

	F64 getCPUUsage();

	void sendPickFileResponse(const std::string& file);
	void sendPickFileResponse(const std::vector<std::string> files);
	// These are valid during MEDIA_EVENT_PICK_FILE_REQUEST
	LL_INLINE bool getIsMultipleFilePick() const		{ return mIsMultipleFilePick; }

	void sendAuthResponse(bool ok, const std::string& username,
						  const std::string& password);

	// Valid after a MEDIA_EVENT_CURSOR_CHANGED event
	LL_INLINE std::string getCursorName() const			{ return mCursorName; }

	LL_INLINE LLPluginClassMediaOwner::EMediaStatus getStatus() const
	{
		return mStatus;
	}

	void cut();
	LL_INLINE bool canCut() const						{ return mCanCut; }

	void copy();
	LL_INLINE bool canCopy() const						{ return mCanCopy; }

	void paste();
	LL_INLINE bool canPaste() const						{ return mCanPaste; }

	// These can be called before init(), and they will be queued and sent
	// before the media init message.
	void setUserDataPath(const std::string& user_data_path);
	void setLanguageCode(const std::string& language_code);
	void setPreferredFont(const std::string& family);
	void setMinimumFontSize(U32 size);
	void setDefaultFontSize(U32 size);
	void setRemoteFontsEnabled(bool enabled);
	void setPluginsEnabled(bool enabled);
	void setJavascriptEnabled(bool enabled);

	LL_INLINE void setTarget(const std::string& tgt)	{ mTarget = tgt; }

	// mClickTarget is received from message and governs how link will be
	// opened; use this to enforce your own way of opening links inside
	// plugins.
	LL_INLINE void setOverrideClickTarget(const std::string& target)
	{
		mClickEnforceTarget = true;
		mOverrideClickTarget = target;
	}

	LL_INLINE std::string getOverrideClickTarget() const
	{
		return mOverrideClickTarget;
	}

	LL_INLINE void resetOverrideClickTarget()			{ mClickEnforceTarget = false; }
	LL_INLINE bool isOverrideClickTarget() const		{ return mClickEnforceTarget; }

	///////////////////////////////////
	// Media browser class functions
	bool pluginSupportsMediaBrowser();

	void focus(bool focused);
	void set_page_zoom_factor(double factor);
	void clear_cache();
	void clear_cookies();
	void set_cookies(const std::string& cookies);
	void cookies_enabled(bool enable);
	void proxy_setup(bool enable, const std::string& host = LLStringUtil::null,
					 int port = 0);
	void browse_stop();
	void browse_reload(bool ignore_cache = false);
	void browse_forward();
	void browse_back();
	void setBrowserUserAgent(const std::string& user_agent);
	void showWebInspector(bool show);
	void proxyWindowOpened(const std::string& target, const std::string& uuid);
	void proxyWindowClosed(const std::string& uuid);
	void ignore_ssl_cert_errors(bool ignore);
	void addCertificateFilePath(const std::string& path);

	// This is valid after MEDIA_EVENT_NAVIGATE_BEGIN or
	// MEDIA_EVENT_NAVIGATE_COMPLETE
	LL_INLINE std::string	getNavigateURI() const		{ return mNavigateURI; }

	// These are valid after MEDIA_EVENT_NAVIGATE_COMPLETE
	LL_INLINE S32 getNavigateResultCode() const			{ return mNavigateResultCode; }

	LL_INLINE std::string getNavigateResultString() const
	{
		return mNavigateResultString;
	}

	LL_INLINE bool getHistoryBackAvailable() const		{ return mHistoryBackAvailable; }
	LL_INLINE bool getHistoryForwardAvailable() const	{ return mHistoryForwardAvailable; }

	// This is valid after MEDIA_EVENT_PROGRESS_UPDATED
	LL_INLINE int getProgressPercent() const			{ return mProgressPercent; }

	// This is valid after MEDIA_EVENT_STATUS_TEXT_CHANGED
	LL_INLINE std::string getStatusText() const			{ return mStatusText; }

	// This is valid after MEDIA_EVENT_LOCATION_CHANGED
	LL_INLINE std::string getLocation() const			{ return mLocation; }

	// This is valid after MEDIA_EVENT_CLICK_LINK_HREF or
	// MEDIA_EVENT_CLICK_LINK_NOFOLLOW
	LL_INLINE std::string getClickURL() const			{ return mClickURL; }

	// This is valid after MEDIA_EVENT_CLICK_LINK_NOFOLLOW
	LL_INLINE std::string getClickNavType() const		{ return mClickNavType; }

	// This is valid after MEDIA_EVENT_CLICK_LINK_HREF
	LL_INLINE std::string getClickTarget() const		{ return mClickTarget; }

	// This is valid during MEDIA_EVENT_CLICK_LINK_HREF and
	// MEDIA_EVENT_GEOMETRY_CHANGE
	LL_INLINE std::string getClickUUID() const			{ return mClickUUID; }

	// These are valid during MEDIA_EVENT_DEBUG_MESSAGE
	LL_INLINE std::string getDebugMessageText() const	{ return mDebugMessageText; }
	LL_INLINE std::string getDebugMessageLevel() const	{ return mDebugMessageLevel; }

	// This is valid after MEDIA_EVENT_NAVIGATE_ERROR_PAGE
	LL_INLINE S32 getStatusCode() const					{ return mStatusCode; }

	// These are valid during MEDIA_EVENT_GEOMETRY_CHANGE
	LL_INLINE S32 getGeometryX() const					{ return mGeometryX; }
	LL_INLINE S32 getGeometryY() const					{ return mGeometryY; }
	LL_INLINE S32 getGeometryWidth() const				{ return mGeometryWidth; }
	LL_INLINE S32 getGeometryHeight() const				{ return mGeometryHeight; }

	// These are valid during MEDIA_EVENT_AUTH_REQUEST
	LL_INLINE std::string getAuthURL() const			{ return mAuthURL; }
	LL_INLINE std::string getAuthRealm() const			{ return mAuthRealm; }

	// These are valid during MEDIA_EVENT_LINK_HOVERED
	LL_INLINE std::string getHoverText() const			{ return mHoverText; }
	LL_INLINE std::string getHoverLink() const			{ return mHoverLink; }

	LL_INLINE const std::string& getPluginFileName() const
	{
		return mPluginFileName;
	}

	// These are valid during MEDIA_EVENT_FILE_DOWNLOAD
	LL_INLINE const std::string& getFileDownloadFilename() const
	{
		return mFileDownloadFilename;
	}

	// Media name (or title) and artist
	LL_INLINE const std::string& getMediaName() const	{ return mMediaName; }
	LL_INLINE const std::string& getArtist() const		{ return mArtist; }

	///////////////////////////////////
	// Media time class functions
	bool pluginSupportsMediaTime();
	void stop();
	void start(float rate = 0.f);
	void pause();
	void seek(float time);
	void setLoop(bool loop);

	void setVolume(float volume);
	LL_INLINE float getVolume()							{ return mRequestedVolume; }


	LL_INLINE F64 getCurrentTime() const				{ return mCurrentTime; }
	LL_INLINE F64 getDuration() const					{ return mDuration; }
	LL_INLINE F64 getCurrentPlayRate()					{ return mCurrentRate; }
	LL_INLINE F64 getLoadedDuration() const				{ return mLoadedDuration; }

	// Initialize the URL history of the plugin by sending
	// "init_history" message
	void initializeUrlHistory(const LLSD& url_history);

	// For debug use only
	LL_INLINE void setDeleteOK(bool flag)				{ mDeleteOK = flag; }
	// Crash the plugin. If you use this outside of a testbed, you will be
	// punished.
	void crashPlugin();
	// Hang the plugin. If you use this outside of a testbed, you will be
	// punished.
	void hangPlugin();

	LL_INLINE std::shared_ptr<LLPluginClassMedia> getSharedPtr()
	{
		return std::dynamic_pointer_cast<LLPluginClassMedia>(shared_from_this());
	}

	void injectOpenIdCookie();

	LL_INLINE static void setOpenIdCookie(const std::string& url,
										  const std::string& host,
										  const std::string& path,
										  const std::string& name,
										  const std::string& value)
	{
		sOpenIdCookieURL = url;
		sOpenIdCookieHost = host;
		sOpenIdCookiePath = path;
		sOpenIdCookieName = name;
		sOpenIdCookieValue = value;
	}

protected:
	// Notify this object's owner that an event has occurred.
	void mediaEvent(LLPluginClassMediaOwner::EMediaEvent event);

	// Send message internally, either queueing or sending directly.
	void sendMessage(const LLPluginMessage& message);

	void setSizeInternal();

	std::string translateModifiers(MASK modifiers);

protected:
	LLPluginClassMediaOwner*				mOwner;
	LLPluginProcessParent::ptr_t			mPlugin;

	LLPluginClassMediaOwner::EMediaStatus	mStatus;

	int										mProgressPercent;

	S32 									mRequestedTextureDepth;
	U32										mRequestedTextureInternalFormat;
	U32										mRequestedTextureFormat;
	U32										mRequestedTextureType;

	std::string								mTextureSharedMemoryName;
	size_t									mTextureSharedMemorySize;

	// default media size for the plugin, from the texture_params message.
	int										mDefaultMediaWidth;
	int										mDefaultMediaHeight;

	// Size that has been requested by the plugin itself
	int										mNaturalMediaWidth;
	int										mNaturalMediaHeight;

	// Size that has been requested with setSize()
	int										mSetMediaWidth;
	int										mSetMediaHeight;

	// Full calculated media size (before auto-scale and downsample
	// calculations)
	int										mFullMediaWidth;
	int										mFullMediaHeight;

	// Actual media size being set (after auto-scale)
	int										mRequestedMediaWidth;
	int										mRequestedMediaHeight;

	// Texture size calculated from actual media size
	int										mRequestedTextureWidth;
	int										mRequestedTextureHeight;

	// Size that the plugin has acknowledged
	int										mTextureWidth;
	int										mTextureHeight;
	int										mMediaWidth;
	int										mMediaHeight;

	float									mRequestedVolume;

	// Priority of this media stream
	EPriority								mPriority;
	int										mLowPrioritySizeLimit;

	int										mPadding;

	LLRect									mDirtyRect;

	int										mLastMouseX;
	int										mLastMouseY;

	F64										mZoomFactor;

	F64										mSleepTime;

	LLColor4								mBackgroundColor;

	std::string								mCursorName;

	std::string								mPluginFileName;

	std::string								mMediaName;
	std::string								mArtist;

	std::string								mTarget;

	/////////////////////////////////////////
	// media_time class
	F64										mCurrentTime;
	F64										mDuration;
	F64										mCurrentRate;
	F64										mLoadedDuration;
	/////////////////////////////////////////

	// Used to queue messages while the plugin initializes.
	std::queue<LLPluginMessage>				mSendQueue;

	/////////////////////////////////////////
	// media_browser class
	S32										mNavigateResultCode;
	S32										mGeometryX;
	S32										mGeometryY;
	S32										mGeometryWidth;
	S32										mGeometryHeight;
	S32										mStatusCode;

	std::string								mNavigateURI;
	std::string								mNavigateResultString;
	std::string								mStatusText;
	std::string								mLocation;
	std::string								mClickURL;
	std::string								mClickNavType;
	std::string								mClickTarget;
	std::string								mOverrideClickTarget;
	std::string								mClickUUID;
	std::string								mDebugMessageText;
	std::string								mDebugMessageLevel;
	std::string								mAuthURL;
	std::string								mAuthRealm;
	std::string								mHoverText;
	std::string								mHoverLink;
	std::string								mFileDownloadFilename;

	bool									mIsMultipleFilePick;
	bool									mClickEnforceTarget;
	/////////////////////////////////////////

	bool									mCanCut;
	bool									mCanCopy;
	bool									mCanPaste;

	bool									mAllowDownsample;

	// The mRequestedTexture* fields are only valid when this is true
	bool									mTextureParamsReceived;

	// True to scale requested media up to the full size of the texture (i.e.
	// next power of two)
	bool									mAutoScaleMedia;

	bool									mRequestedTextureSwapBytes;
	bool									mRequestedTextureCoordsOpenGL;

	bool									mHistoryBackAvailable;
	bool									mHistoryForwardAvailable;

private:
	// For debug use only
	bool									mDeleteOK;

	static std::string						sOpenIdCookieURL;
	static std::string						sOpenIdCookieHost;
	static std::string						sOpenIdCookiePath;
	static std::string						sOpenIdCookieName;
	static std::string						sOpenIdCookieValue;
};

#endif // LL_LLPLUGINCLASSMEDIA_H
