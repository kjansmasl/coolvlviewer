/**
 * @file llviewermedia.h
 * @brief Client interface to the media engine
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

#ifndef LLVIEWERMEDIA_H
#define LLVIEWERMEDIA_H

#include <list>

#include "llcolor4.h"
#include "llcorehttputil.h"
#include "llnotifications.h"
#include "llmutex.h"
#include "llpanel.h"
#include "llpluginclassmedia.h"
#include "llpluginclassmediaowner.h"

class LLUUID;
class LLMediaEntry;
class LLMimeDiscoveryResponder;
class LLParcel;
class LLTextureEntry;
class LLViewerMediaEventEmitter;
class LLViewerMediaImpl;
class LLViewerMediaTexture;
class LLViewerTexture;
class LLVOVolume;

typedef LLPointer<LLViewerMediaImpl> viewer_media_t;

///////////////////////////////////////////////////////////////////////////////
// Classes that inherit from LLViewerMediaObserver should add this to their
// class declaration:
//
//	// inherited from LLViewerMediaObserver
//	void handleMediaEvent(LLPluginClassMedia* self, EMediaEvent evt) override;
//
// and will probably need to add this to their cpp file:
//	#include "llpluginclassmedia.h"

class LLViewerMediaObserver : public LLPluginClassMediaOwner
{
	friend class LLViewerMediaEventEmitter;

public:
	~LLViewerMediaObserver() override;

private:
	// Emitters will manage this list in addObserver/remObserver.
	std::list<LLViewerMediaEventEmitter*> mEmitters;
};

///////////////////////////////////////////////////////////////////////////////

class LLViewerMediaEventEmitter
{
public:
	virtual ~LLViewerMediaEventEmitter();

	bool addObserver(LLViewerMediaObserver* subject);
	bool remObserver(LLViewerMediaObserver* subject);
	void emitEvent(LLPluginClassMedia* self,
				   LLPluginClassMediaOwner::EMediaEvent event);

private:
	typedef std::list<LLViewerMediaObserver*> observerListType;
	observerListType mObservers;
};

class LLViewerMedia
{
protected:
	LOG_CLASS(LLViewerMedia);

public:
	typedef std::list<LLViewerMediaImpl*> impl_list;

	// Special case early init for just web browser component so we can show
	// login screen. See .cpp file for details. JC
	static viewer_media_t newMediaImpl(const LLUUID& texture_id,
									   S32 media_width = 0,
									   S32 media_height = 0,
									   bool media_auto_scale = false,
									   bool media_loop = false);
	static viewer_media_t updateMediaImpl(LLMediaEntry* media_entry,
										  const std::string& previous_url,
										  bool update_from_self);

	static LLViewerMediaImpl* getMediaImplFromTextureID(const LLUUID& texture_id);
	static LLViewerMediaImpl* getMediaImplFromTextureEntry(const LLTextureEntry* tep);
	static std::string getCurrentUserAgent();
	static bool textureHasMedia(const LLUUID& texture_id);
	static void setVolume(F32 volume);

	// Is any media currently "showing" ?  Includes parcel media. Does not
	// include media in the UI.
	static LL_INLINE bool isAnyMediaShowing()			{ return sAnyMediaShowing; }
	static LL_INLINE bool isAnyMediaEnabled()			{ return sAnyMediaEnabled; }
	static LL_INLINE bool isAnyMediaDisabled()			{ return sAnyMediaDisabled; }

	// Set all media enabled or disabled, depending on val. Does not include
	// media in the UI.
	static void setAllMediaEnabled(bool enable, bool parcel_media = true);

	// For use in menu callbacks (like setAllMediaEnabled(true/false) but with
	// parcel_media = false):
	static void sharedMediaEnable(void* data = NULL);
	static void sharedMediaDisable(void* data = NULL);

	static void updateMedia(void* dummy_arg = NULL);

	static void initClass();
	static void cleanupClass();

	static F32 getVolume();
	static void muteListChanged();
	static bool isInterestingEnough(const LLVOVolume* object, F64 interest);

	// Returns the priority-sorted list of all media impls.
	static impl_list& getPriorityList();

	// This is the comparator used to sort the list.
	static bool priorityComparator(const LLViewerMediaImpl* i1,
								   const LLViewerMediaImpl* i2);

	// This is just a helper function for the convenience of others working
	// with media:
	static bool hasInWorldMedia();

	static void onAuthSubmit(const LLUUID media_id, const std::string username,
							 const std::string password, bool validated);

	// Clear all cookies for all plugins
	static void clearAllCookies();

	// Clear all plugins' caches
	static void clearAllCaches();

	// Set the "cookies enabled" flag for all loaded plugins
	static void setCookiesEnabled(bool enabled);

	// Set the proxy config for all loaded plugins
	static void setProxyConfig(bool enable, const std::string& host, S32 port);

	static void openIDSetup(const std::string& url, const std::string& token);
	static void openIDCookieResponse(const std::string& url,
									 const std::string& cookie);

	static void setOnlyAudibleMediaTextureID(const LLUUID& texture_id);

	// For the media filter implementation:

	// type: 0 = media, 1 = streaming music
	static void filterParcelMedia(LLParcel* parcel, U32 type);

	// Returns true if filtering is needed (permission dialog shown to user, or
	// media rejected).
	static bool filterMedia(LLViewerMediaImpl* impl);

	static bool allowedMedia(std::string media_url);
	static bool loadDomainFilterList();
	static void saveDomainFilterList();
	static void clearDomainFilterList();
	static std::string extractDomain(std::string url);
	static std::string getDomainIP(const std::string& domain,
								   bool force = false);

private:
	static void setOpenIDCookie(const std::string& url = LLStringUtil::null);
	static void openIDSetupCoro(std::string url, const std::string& token);

	static void onTeleportFinished();

public:
	// For the media filter implementation:
	static bool sIsUserAction;
	static bool sMediaFilterListLoaded;
	static LLSD sMediaFilterList;
	static std::set<std::string> sMediaQueries;
	static std::set<std::string> sAllowedMedia;
	static std::set<std::string> sDeniedMedia;

private:
	// For the media filter implementation:
	static std::map<std::string, std::string> sDNSlookups;

	static std::string sOpenIDCookie;

	static bool sAnyMediaShowing;
	static bool sAnyMediaEnabled;
	static bool sAnyMediaDisabled;
};

// Implementation functions not exported into header file
class LLViewerMediaImpl : public LLMouseHandler,
						  public LLRefCount,
						  public LLPluginClassMediaOwner,
						  public LLViewerMediaEventEmitter,
						  public LLEditMenuHandler
{
protected:
	LOG_CLASS(LLViewerMediaImpl);

public:
	friend class LLViewerMedia;

	LLViewerMediaImpl(const LLUUID& texture_id,
					  S32 media_width, S32 media_height,
					  bool media_auto_scale, bool media_loop);

	~LLViewerMediaImpl() override;

	// Override inherited version from LLViewerMediaEventEmitter
	virtual void emitEvent(LLPluginClassMedia* self,
						   LLViewerMediaObserver::EMediaEvent event);

	void createMediaSource();
	void destroyMediaSource();
	void setMediaType(const std::string& media_type);
	bool initializeMedia(const std::string& mime_type);
	bool initializePlugin(const std::string& media_type);
	void loadURI();
	LL_INLINE LLPluginClassMedia* getMediaPlugin()		{ return mMediaSource; }
	void setSize(int width, int height);

	void showNotification(LLNotificationPtr notify);
	void hideNotification();

	void play();
	void stop();
	void pause();
	void start();
	void seek(F32 time);
	void skipBack(F32 step_scale);
	void skipForward(F32 step_scale);

	void updateVolume();
	F32 getVolume();
	void setVolume(F32 volume);
	void setMute(bool mute = true);

	void focus(bool focus);
	// True if the impl has user focus.
	bool hasFocus() const;

	void mouseDown(S32 x, S32 y, MASK mask, S32 button = 0);
	void mouseUp(S32 x, S32 y, MASK mask, S32 button = 0);
	void mouseMove(S32 x, S32 y, MASK mask);
	void mouseDown(const LLVector2& texture_coords, MASK mask, S32 button = 0);
	void mouseUp(const LLVector2& texture_coords, MASK mask, S32 button = 0);
	void mouseMove(const LLVector2& texture_coords, MASK mask);
	void mouseDoubleClick(const LLVector2& texture_coords, MASK mask);
	void mouseDoubleClick(S32 x, S32 y, MASK mask, S32 button = 0);
	void scrollWheel(S32 x, S32 y, S32 scroll_x, S32 scroll_y, MASK mask);
	void mouseCapture();

	void unload();

	void navigateTo(const std::string& url, const std::string& mime_type = "",
					bool rediscover_type = false, bool server_request = false,
					bool filter_url = true);
	void navigateInternal();
	void navigateHome();
	void navigateStop();
	void navigateReload();
	void navigateBack();
	void navigateForward();

	bool handleKeyHere(KEY key, MASK mask);
	bool handleKeyUpHere(KEY key, MASK mask);
	bool handleUnicodeCharHere(llwchar uni_char);
	bool canNavigateForward();
	bool canNavigateBack();

	LL_INLINE std::string getMediaURL()					{ return mMediaURL; }
	LL_INLINE std::string getHomeURL()					{ return mHomeURL; }
	std::string getCurrentMediaURL();
	LL_INLINE std::string getMediaEntryURL()			{ return mMediaEntryURL; }
	void setHomeURL(const std::string& home_url,
					const std::string& mime_type = LLStringUtil::null);

	void setPageZoomFactor(F64 factor);

	LL_INLINE std::string getMimeType()					{ return mMimeType; }
	void scaleMouse(S32* mouse_x, S32* mouse_y);
	void scaleTextureCoords(const LLVector2& texture_coords, S32* x, S32* y);

	LL_INLINE const LLUUID& getMediaTextureID()			{ return mTextureId; }

	void update();
	LL_INLINE void suspendUpdates(bool suspend)			{ mSuspendUpdates = suspend; };

	LL_INLINE bool getVisible() const					{ return mVisible; }
	LL_INLINE bool isVisible() const					{ return mVisible; }
	void setVisible(bool visible);

	bool isMediaPlaying();
	bool isMediaPaused();
	bool hasMedia();
	bool isMediaTimeBased();
	LL_INLINE bool isMediaFailed() const				{ return mMediaSourceFailed; }
	LL_INLINE void setMediaFailed(bool val)				{ mMediaSourceFailed = val; }

	void setDisabled(bool disabled, bool forcePlayOnEnable = false);
	LL_INLINE bool isMediaDisabled() const				{ return mIsDisabled; }

	LL_INLINE void setInNearbyMediaList(bool b)			{ mInNearbyMediaList = b; }
	LL_INLINE bool getInNearbyMediaList()				{ return mInNearbyMediaList; }

	// returns true if this instance should not be loaded (disabled, muted
	// object, crashed, etc.)
	bool isForcedUnloaded() const;

	// returns true if this instance could be playable based on autoplay
	// setting, current load state, etc.
	bool isPlayable() const;

	LL_INLINE void setIsParcelMedia(bool b)				{ mIsParcelMedia = b; }
	LL_INLINE bool isParcelMedia() const				{ return mIsParcelMedia; }

	LL_INLINE ECursorType getLastSetCursor()			{ return mLastSetCursor; }

	void resetPreviousMediaState();

	LL_INLINE void setTarget(const std::string& target)	{ mTarget = target; }

	// utility function to create a ready-to-use media instance from a desired media type.
	static LLPluginClassMedia* newSourceFromMediaType(std::string media_type,
													  LLPluginClassMediaOwner* owner, /* may be NULL */
													  S32 default_width,
													  S32 default_height,
													  const std::string target = LLStringUtil::null);

	// Need these to handle mouseup...
	void onMouseCaptureLost() override;
	bool handleMouseUp(S32 x, S32 y, MASK mask) override;

	// Grr... the only thing I want as an LLMouseHandler are the
	// onMouseCaptureLost and handleMouseUp calls. Sadly, these are all pure
	// virtual, so I have to supply implementations here:

	LL_INLINE bool handleMouseDown(S32, S32, MASK) override
	{
		return false;
	}

	LL_INLINE bool handleHover(S32, S32, MASK) override	{ return false; }

	LL_INLINE bool handleScrollWheel(S32, S32, S32) override
	{
		return false;
	}

	LL_INLINE bool handleDoubleClick(S32, S32, MASK) override
	{
		return false;
	}

	LL_INLINE bool handleRightMouseDown(S32, S32, MASK) override
	{
		return false;
	}

	LL_INLINE bool handleRightMouseUp(S32, S32, MASK) override
	{
		return false;
	}

	LL_INLINE bool handleMiddleMouseDown(S32, S32, MASK) override
	{
		return false;
	}

	LL_INLINE bool handleMiddleMouseUp(S32, S32, MASK) override
	{
		return false;
	}

	LL_INLINE bool handleToolTip(S32, S32, std::string&, LLRect*) override
	{
		return false;
	}

	std::string getName() const override;

	LL_INLINE bool isView() const override				{ return false; }

	LL_INLINE void screenPointToLocal(S32, S32, S32*, S32*) const override
	{
	}

	LL_INLINE void localPointToScreen(S32, S32, S32*, S32*) const override
	{
	}

	LL_INLINE bool hasMouseCapture() override
	{
		return gFocusMgr.getMouseCapture() == this;
	}

	// Inherited from LLPluginClassMediaOwner
	void handleMediaEvent(LLPluginClassMedia* self,
						  LLPluginClassMediaOwner::EMediaEvent) override;

	// LLEditMenuHandler overrides
	void cut() override;
	bool canCut() const override;

	void copy() override;
	bool canCopy() const override;

	void paste() override;
	bool canPaste() const override;

	void addObject(LLVOVolume* obj);
	void removeObject(LLVOVolume* obj);
	const std::list<LLVOVolume*>* getObjectList() const;
	LLVOVolume* getSomeObject();
	LL_INLINE void setUpdated(bool updated)				{ mIsUpdated = updated; }
	LL_INLINE bool isUpdated()							{ return mIsUpdated; }

	// Updates the "interest" value in this object
	void calculateInterest();
	LL_INLINE F64 getInterest() const					{ return mInterest; }
	F64 getApproximateTextureInterest();
	LL_INLINE S32 getProximity() const					{ return mProximity; }
	LL_INLINE F64 getProximityDistance() const			{ return mProximityDistance; }

	// Mark this object as being used in a UI panel instead of on a prim
	// This will be used as part of the interest sorting algorithm.
	void setUsedInUI(bool used_in_ui);
	LL_INLINE bool getUsedInUI() const					{ return mUsedInUI; }

	LL_INLINE void setUsedOnHUD(bool used_on_hud)		{ mUsedOnHUD = used_on_hud; }
	LL_INLINE bool getUsedOnHUD() const					{ return mUsedOnHUD; }

	F64 getCPUUsage() const;

	void setPriority(LLPluginClassMedia::EPriority priority);
	LL_INLINE LLPluginClassMedia::EPriority getPriority()
	{
		return mPriority;
	}

	void setLowPrioritySizeLimit(int size);

	void setTextureID(LLUUID id = LLUUID::null);

	void setBackgroundColor(LLColor4 color);

	LL_INLINE bool isTrustedBrowser()					{ return mTrustedBrowser; }
	LL_INLINE void setTrustedBrowser(bool trusted)		{ mTrustedBrowser = trusted; }

	typedef enum
	{
		MEDIANAVSTATE_NONE,										// State is outside what we need to track for navigation.
		MEDIANAVSTATE_BEGUN,									// a MEDIA_EVENT_NAVIGATE_BEGIN has been received which was not server-directed
		MEDIANAVSTATE_FIRST_LOCATION_CHANGED,					// first LOCATION_CHANGED event after a non-server-directed BEGIN
		MEDIANAVSTATE_FIRST_LOCATION_CHANGED_SPURIOUS,			// Same as above, but the new URL is identical to the previously navigated URL.
		MEDIANAVSTATE_COMPLETE_BEFORE_LOCATION_CHANGED,			// we received a NAVIGATE_COMPLETE event before the first LOCATION_CHANGED
		MEDIANAVSTATE_COMPLETE_BEFORE_LOCATION_CHANGED_SPURIOUS,// Same as above, but the new URL is identical to the previously navigated URL.
		MEDIANAVSTATE_SERVER_SENT,								// server-directed nav has been requested, but MEDIA_EVENT_NAVIGATE_BEGIN hasn't been received yet
		MEDIANAVSTATE_SERVER_BEGUN,								// MEDIA_EVENT_NAVIGATE_BEGIN has been received which was server-directed
		MEDIANAVSTATE_SERVER_FIRST_LOCATION_CHANGED,			// first LOCATION_CHANGED event after a server-directed BEGIN
		MEDIANAVSTATE_SERVER_COMPLETE_BEFORE_LOCATION_CHANGED	// we received a NAVIGATE_COMPLETE event before the first LOCATION_CHANGED
	} EMediaNavState;

	// Returns the current nav state of the media. Note that this will be
	// updated BEFORE listeners and objects receive media messages
	LL_INLINE EMediaNavState getNavState()				{ return mMediaNavState; }
	void setNavState(EMediaNavState state);

	void setNavigateSuspended(bool suspend);
	LL_INLINE bool isNavigateSuspended()				{ return mNavigateSuspended; }

	void cancelMimeTypeProbe();

	// Is this media attached to an avatar *not* self
	bool isAttachedToAnotherAvatar() const;

	// Is this media in the agent's parcel ?
	bool isInAgentParcel() const;

	// get currently active notification associated with this media instance
	LLNotificationPtr getCurrentNotification() const;

private:
	bool preMediaTexUpdate(LLViewerMediaTexture*& media_tex, U8*& data,
						   S32& data_width, S32& data_height, S32& x_pos,
						   S32& y_pos, S32& width, S32& height);
	void doMediaTexUpdate(LLViewerMediaTexture* media_tex, U8* data,
						  S32 data_width, S32 data_height, S32 x_pos,
						  S32 y_pos, S32 width, S32 height, bool sync);
	LLViewerMediaTexture* updateMediaImage();
	bool isAutoPlayable() const;
	bool shouldShowBasedOnClass() const;
	void mimeDiscoveryCoro(std::string url);

public:
	LLPluginClassMedia*				mMediaSource;
	LLUUID							mTextureId;
	LLNotificationPtr				mNotification;
	LLColor4						mBackgroundColor;
	ECursorType						mLastSetCursor;
	std::string						mMediaURL;
	std::string						mHomeURL;
	// Forced mime type for home url:
	std::string						mHomeMimeType;
	std::string						mMimeType;
	// The most current media url from the plugin (via the "location changed"
	// or "navigate complete" events):
	std::string						mCurrentMediaURL;
	// The MIME type that caused the currently loaded plugin to be loaded:
	std::string						mCurrentMimeType;
	std::string						mTarget;
	std::string						mMediaEntryURL;
	LLPluginClassMedia::EPriority	mPriority;

	// Save the last mouse coord we get, so when we lose capture we can
	// simulate a mouseup at that point:
	S32								mLastMouseX;
	S32								mLastMouseY;

	S32								mMediaWidth;
	S32								mMediaHeight;
	S32								mTextureUsedWidth;
	S32								mTextureUsedHeight;

	F64								mZoomFactor;
	F64								mInterest;
	F32								mRequestedVolume;
	F32								mPreviousVolume;
	F64								mPreviousMediaTime;
	F64								mProximityDistance;
	F64								mProximityCamera;
	S32								mProximity;
	S32								mPreviousMediaState;
	EMediaNavState					mMediaNavState;

	bool							mMovieImageHasMips;
	bool							mFilterURL;
	bool							mMediaAutoScale;
	bool							mMediaLoop;
	bool							mNeedsNewTexture;
	bool							mSuspendUpdates;
	bool							mTextureUpdatePending;
	bool							mVisible;
	bool							mHasFocus;
	bool							mMediaSourceFailed;
	bool							mTrustedBrowser;
	bool							mUsedOnHUD;
	bool							mUsedInUI;
	bool							mNavigateRediscoverType;
	bool							mNavigateServerRequest;
	bool							mIsMuted;
	bool							mNeedsMuteCheck;
	bool							mIsDisabled;
	bool							mIsParcelMedia;
	bool							mMediaAutoPlay;

	// Used by LLPanelNearbyMedia::refreshList() for performance reasons:
	bool							mInNearbyMediaList;

	bool							mNavigateSuspended;
	bool							mNavigateSuspendedDeferred;

private:
	bool							mIsUpdated;

	std::list<LLVOVolume*>			mObjectList;

	typedef LLCoreHttpUtil::HttpCoroutineAdapter::wptr_t mime_probe_ptr_t;
	mime_probe_ptr_t				mMimeProbe;

	LLMutex							mLock;
};

#endif	// LLVIEWERMEDIA_H
