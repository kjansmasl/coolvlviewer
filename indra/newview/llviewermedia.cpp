/**
 * @file llviewermedia.cpp
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
 * ("Other License"), formally executed by you and Linden Lab. Terms of
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

#include "llviewerprecompiledheaders.h"

#include <regex>

#include "boost/signals2.hpp"

#include "cef/dullahan.h"			// For HB_DULLAHAN_EXTENDED

#include "llviewermedia.h"

#include "llcallbacklist.h"
#include "lldir.h"
#include "lldiriterator.h"
#include "llfasttimer.h"
#include "hbfileselector.h"
#include "hbfloateruserauth.h"
#include "llimagegl.h"
#include "llkeyboard.h"
#include "llmediaentry.h"
#include "llmimetypes.h"
#include "llparcel.h"
#include "llpluginclassmedia.h"
#include "llsdserialize.h"
#include "llversionviewer.h"
#include "llview.h"

#include "llagent.h"
#include "llappviewer.h"
#include "llfloateravatarinfo.h"	// for getProfileURL()
#include "slfloatermediafilter.h"
#include "llhoverview.h"
#include "llmutelist.h"
#include "llstartup.h"				// for getStartupState()
#include "llurldispatcher.h"
#include "llviewercontrol.h"
#include "llviewermediafocus.h"
#include "llviewerparcelmedia.h"
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"
#include "llviewertexturelist.h"
#include "llviewerwindow.h"
#include "llvoavatarself.h"
#include "llvovolume.h"
#include "llweb.h"

// Move this to its own file.

LLViewerMediaEventEmitter::~LLViewerMediaEventEmitter()
{
	observerListType::iterator iter = mObservers.begin();

	while (iter != mObservers.end())
	{
		LLViewerMediaObserver* self = *iter++;
		remObserver(self);
	}
}

bool LLViewerMediaEventEmitter::addObserver(LLViewerMediaObserver* observer)
{
	if (!observer ||
		std::find(mObservers.begin(), mObservers.end(),
				  observer) != mObservers.end())
	{
		return false;
	}

	mObservers.push_back(observer);
	observer->mEmitters.push_back(this);

	return true;
}

bool LLViewerMediaEventEmitter::remObserver(LLViewerMediaObserver* observer)
{
	if (!observer)
	{
		return false;
	}

	mObservers.remove(observer);
	observer->mEmitters.remove(this);

	return true;
}

void LLViewerMediaEventEmitter::emitEvent(LLPluginClassMedia* media,
										  LLPluginClassMediaOwner::EMediaEvent event)
{
	// Broadcast the event to any observers.
	observerListType::iterator iter = mObservers.begin();
	while (iter != mObservers.end())
	{
		LLViewerMediaObserver* self = *iter++;
		self->handleMediaEvent(media, event);
	}
}

// Move this to its own file.
LLViewerMediaObserver::~LLViewerMediaObserver()
{
	std::list<LLViewerMediaEventEmitter*>::iterator iter = mEmitters.begin();

	while (iter != mEmitters.end())
	{
		LLViewerMediaEventEmitter* self = *iter++;
		self->remObserver(this);
	}
}

std::string LLViewerMedia::sOpenIDCookie;
static LLViewerMedia::impl_list sViewerMediaImplList;
typedef fast_hmap<LLUUID, LLViewerMediaImpl*> impl_id_map_t;
static impl_id_map_t sViewerMediaTextureIDMap;
static LLTimer sMediaCreateTimer;
constexpr F32 LLVIEWERMEDIA_CREATE_DELAY = 1.f;
static F32 sGlobalVolume = 1.f;
static bool sForceUpdate = false;
static LLUUID sOnlyAudibleTextureID;
static F64 sLowestLoadableImplInterest = 0.0;
bool LLViewerMedia::sAnyMediaShowing = false;
bool LLViewerMedia::sAnyMediaEnabled = false;
bool LLViewerMedia::sAnyMediaDisabled = false;
static boost::signals2::connection sTeleportFinishConnection;

// For the media filter implementation:
bool LLViewerMedia::sIsUserAction = false;
bool LLViewerMedia::sMediaFilterListLoaded = false;
LLSD LLViewerMedia::sMediaFilterList;
std::set<std::string> LLViewerMedia::sMediaQueries;
std::set<std::string> LLViewerMedia::sAllowedMedia;
std::set<std::string> LLViewerMedia::sDeniedMedia;
std::map<std::string, std::string> LLViewerMedia::sDNSlookups;

static void add_media_impl(LLViewerMediaImpl* media)
{
	sViewerMediaImplList.push_back(media);
}

static void remove_media_impl(LLViewerMediaImpl* media)
{
	for (LLViewerMedia::impl_list::iterator
			iter = sViewerMediaImplList.begin(),
			end = sViewerMediaImplList.end();
		 iter != end; ++iter)
	{
		if (media == *iter)
		{
			sViewerMediaImplList.erase(iter);
			return;
		}
	}
}

static bool is_media_impl_valid(LLViewerMediaImpl* media)
{
	if (!media) return false;

	LLViewerMedia::impl_list::iterator end = sViewerMediaImplList.end();
	return std::find(sViewerMediaImplList.begin(), end, media) != end;
}

class LLViewerMediaMuteListObserver final : public LLMuteListObserver
{
	void onChange() override
	{
		LLViewerMedia::muteListChanged();
	}
};

static LLViewerMediaMuteListObserver sViewerMediaMuteListObserver;
static bool sViewerMediaMuteListObserverInitialized = false;

///////////////////////////////////////////////////////////////////////////////
// LLViewerMedia
///////////////////////////////////////////////////////////////////////////////

//static
viewer_media_t LLViewerMedia::newMediaImpl(const LLUUID& texture_id,
										   S32 media_width,
										   S32 media_height,
										   bool media_auto_scale,
										   bool media_loop)
{
	LLViewerMediaImpl* self = getMediaImplFromTextureID(texture_id);
	if (!self || texture_id.isNull())
	{
		// Create the media impl
		self = new LLViewerMediaImpl(texture_id, media_width, media_height,
									 media_auto_scale, media_loop);
	}
	else
	{
		self->unload();
		self->setTextureID(texture_id);
		self->mMediaWidth = media_width;
		self->mMediaHeight = media_height;
		self->mMediaAutoScale = media_auto_scale;
		self->mMediaLoop = media_loop;
	}

	return self;
}

//static
viewer_media_t LLViewerMedia::updateMediaImpl(LLMediaEntry* media_entry,
											  const std::string& previous_url,
											  bool update_from_self)
{
	// Try to find media with the same media ID
	viewer_media_t self = getMediaImplFromTextureID(media_entry->getMediaID());

	LL_DEBUGS("Media") << "called, current URL is \""
					   << media_entry->getCurrentURL()
					   << "\", previous URL is \"" << previous_url
					   << "\", update_from_self is "
					   << (update_from_self ? "true" : "false") << LL_ENDL;

	bool was_loaded = false;
	bool needs_navigate = false;

	if (self)
	{
		was_loaded = self->hasMedia();

		self->setHomeURL(media_entry->getHomeURL());

		self->mMediaAutoScale = media_entry->getAutoScale();
		self->mMediaLoop = media_entry->getAutoLoop();
		self->mMediaWidth = media_entry->getWidthPixels();
		self->mMediaHeight = media_entry->getHeightPixels();
		self->mMediaAutoPlay = media_entry->getAutoPlay();
		self->mMediaEntryURL = media_entry->getCurrentURL();
		if (self->mMediaSource)
		{
			self->mMediaSource->setAutoScale(self->mMediaAutoScale);
			self->mMediaSource->setLoop(self->mMediaLoop);
			self->mMediaSource->setSize(media_entry->getWidthPixels(),
										media_entry->getHeightPixels());
		}

		bool url_changed = self->mMediaEntryURL != previous_url;
		if (self->mMediaEntryURL.empty())
		{
			if (url_changed)
			{
				// The current media URL is now empty. Unload the media source.
				self->unload();

				LL_DEBUGS("Media") << "Unloading media instance (new current URL is empty)."
								   << LL_ENDL;
			}
		}
		else
		{
			// The current media URL is not empty. If (the media was already
			// loaded OR the media was set to autoplay) AND this update did not
			// come from this agent, then do a navigate.
			bool auto_play = self->isAutoPlayable();
			if ((was_loaded || auto_play) && !update_from_self)
			{
				needs_navigate = url_changed;
			}

			LL_DEBUGS("Media") << "was_loaded is "
							   << (was_loaded ? "true" : "false")
							   << ", auto_play is "
							   << (auto_play ? "true" : "false")
							   << ", needs_navigate is "
							   << (needs_navigate ? "true" : "false")
							   << LL_ENDL;
		}
	}
	else
	{
		self = newMediaImpl(media_entry->getMediaID(),
							media_entry->getWidthPixels(),
							media_entry->getHeightPixels(),
							media_entry->getAutoScale(),
							media_entry->getAutoLoop());

		self->setHomeURL(media_entry->getHomeURL());
		self->mMediaAutoPlay = media_entry->getAutoPlay();
		self->mMediaEntryURL = media_entry->getCurrentURL();
		if (self->isAutoPlayable())
		{
			needs_navigate = true;
		}
	}

	if (self)
	{
		if (needs_navigate)
		{
			self->navigateTo(self->mMediaEntryURL, "", true, true);
			LL_DEBUGS("Media") << "Navigating to URL " << self->mMediaEntryURL
							   << LL_ENDL;
		}
		else if (!self->mMediaURL.empty() &&
				 self->mMediaURL != self->mMediaEntryURL)
		{
			// If we already have a non-empty media URL set and we are not
			// doing a navigate, update the media URL to match the media entry.
			self->mMediaURL = self->mMediaEntryURL;

			// If this causes a navigate at some point (such as after a
			// reload), it should be considered server-driven so it is not
			// broadcast.
			self->mNavigateServerRequest = true;

			LL_DEBUGS("Media") << "Updating URL in the media impl to "
							   << self->mMediaEntryURL << LL_ENDL;
		}
	}

	return self;
}

//static
LLViewerMediaImpl* LLViewerMedia::getMediaImplFromTextureID(const LLUUID& texture_id)
{
	LLViewerMediaImpl* result = NULL;

	// Look up the texture ID in the texture id->impl map.
	impl_id_map_t::iterator iter = sViewerMediaTextureIDMap.find(texture_id);
	if (iter != sViewerMediaTextureIDMap.end())
	{
		result = iter->second;
	}

	return result;
}

//static
LLViewerMediaImpl* LLViewerMedia::getMediaImplFromTextureEntry(const LLTextureEntry* tep)
{
	if (!tep) return NULL;

	LLUUID tid;
	LLMediaEntry* mep = tep->hasMedia() ? tep->getMediaData() : NULL;
	if (mep)
	{
		tid = mep->getMediaID();
	}
	else
	{
		// Parcel media do not have media data, but they nonetheless got a
		// media implement...
		tid = tep->getID();
	}
	return LLViewerMedia::getMediaImplFromTextureID(tid);
}

//static
std::string LLViewerMedia::getCurrentUserAgent()
{
	// Append our magic version number string to the browser user agent Id.
	// See the HTTP 1.0 and 1.1 specifications for allowed formats:
	// http://www.ietf.org/rfc/rfc1945.txt section 10.15
	// http://www.ietf.org/rfc/rfc2068.txt section 3.8
	// This was also helpful:
	// http://www.mozilla.org/build/revised-user-agent-strings.html
	std::ostringstream ua;
	ua << "SecondLife/"
	   << LL_VERSION_MAJOR << "." << LL_VERSION_MINOR << "."
	   << LL_VERSION_BRANCH << "." << LL_VERSION_RELEASE << " ("
	   << gSavedSettings.getString("VersionChannelName") << "; "
	   << gSavedSettings.getString("SkinCurrent") << " skin)";
	llinfos << "User agent: " << ua.str() << llendl;

	return ua.str();
}

//static
bool LLViewerMedia::textureHasMedia(const LLUUID& texture_id)
{
	for (impl_list::iterator iter = sViewerMediaImplList.begin(),
							 end = sViewerMediaImplList.end();
		 iter != end; ++iter)
	{
		LLViewerMediaImpl* pimpl = *iter;
		if (pimpl->getMediaTextureID() == texture_id)
		{
			return true;
		}
	}
	return false;
}

//static
void LLViewerMedia::setVolume(F32 volume)
{
	if (volume != sGlobalVolume || sForceUpdate)
	{
		sGlobalVolume = volume;

		for (impl_list::iterator iter = sViewerMediaImplList.begin(),
								 end = sViewerMediaImplList.end();
			 iter != end; ++iter)
		{
			LLViewerMediaImpl* pimpl = *iter;
			pimpl->updateVolume();
		}

		sForceUpdate = false;
	}
}

//static
F32 LLViewerMedia::getVolume()
{
	return sGlobalVolume;
}

//static
void LLViewerMedia::muteListChanged()
{
	// When the mute list changes, we need to check mute status on all impls.
	for (impl_list::iterator iter = sViewerMediaImplList.begin(),
							 end = sViewerMediaImplList.end();
		 iter != end; iter++)
	{
		LLViewerMediaImpl* pimpl = *iter;
		pimpl->mNeedsMuteCheck = true;
	}
}

//static
bool LLViewerMedia::isInterestingEnough(const LLVOVolume* object,
										F64 object_interest)
{
	bool result = false;

	if (!object)
	{
		result = false;
	}
	// Focused ?  Then it is interesting !
	else if (LLViewerMediaFocus::getInstance()->getFocusedObjectID() == object->getID())
	{
		result = true;
	}
	// Selected ?  Then it is interesting !
	// XXX Sadly, 'contains()' does not take a const :(
	else if (gSelectMgr.getSelection()->contains(const_cast<LLVOVolume*>(object)))
	{
		result = true;
	}
	else
	{
		LL_DEBUGS("Media") << "object interest = " << object_interest
						   << ", lowest loadable = "
						   << sLowestLoadableImplInterest << LL_ENDL;
		if (object_interest >= sLowestLoadableImplInterest)
		{
			result = true;
		}
	}

	return result;
}

LLViewerMedia::impl_list& LLViewerMedia::getPriorityList()
{
	return sViewerMediaImplList;
}

// This is the predicate function used to sort sViewerMediaImplList by
// priority.
bool LLViewerMedia::priorityComparator(const LLViewerMediaImpl* i1,
									   const LLViewerMediaImpl* i2)
{
	if (i1->isForcedUnloaded() && !i2->isForcedUnloaded())
	{
		// Muted or failed items always go to the end of the list, period.
		return false;
	}
	if (i2->isForcedUnloaded() && !i1->isForcedUnloaded())
	{
		// Muted or failed items always go to the end of the list, period.
		return true;
	}
	if (i1->hasFocus())
	{
		// The item with user focus always comes to the front of the list,
		// period.
		return true;
	}
	if (i2->hasFocus())
	{
		// The item with user focus always comes to the front of the list,
		// period.
		return false;
	}
	if (i1->isParcelMedia())
	{
		// The parcel media impl sorts above all other inworld media, unless
		// one has focus.
		return true;
	}
	if (i2->isParcelMedia())
	{
		// The parcel media impl sorts above all other inworld media, unless
		// one has focus.
		return false;
	}
	if (i1->getUsedInUI() && !i2->getUsedInUI())
	{
		// i1 is a UI element, i2 is not. This makes i1 "less than" i2, so it
		// sorts earlier in our list.
		return true;
	}
	if (i2->getUsedInUI() && !i1->getUsedInUI())
	{
		// i2 is a UI element, i1 is not. This makes i2 "less than" i1, so it
		// sorts earlier in our list.
		return false;
	}
	if (i1->getUsedOnHUD() && !i2->getUsedOnHUD())
	{
		// i1 is used on a HUD, i2 is not. This makes i1 "less than" i2, so it
		// sorts earlier in our list.
		return true;
	}
	if (i2->getUsedOnHUD() && !i1->getUsedOnHUD())
	{
		// i2 is used on a HUD, i1 is not. This makes i2 "less than" i1, so it
		// sorts earlier in our list.
		return false;
	}
	if (i1->isPlayable() && !i2->isPlayable())
	{
		// Playable items sort above ones that would not play even if they got
		// high enough priority
		return true;
	}
	if (!i1->isPlayable() && i2->isPlayable())
	{
		// Playable items sort above ones that would not play even if they got
		// high enough priority
		return false;
	}
	if (i1->getInterest() == i2->getInterest())
	{
		// Generally this will mean both objects have zero interest. In this
		// case, sort on distance.
		return i1->getProximityDistance() < i2->getProximityDistance();
	}
	// The object with the larger interest value should be earlier in the list,
	// so we reverse the sense of the comparison here.
	return i1->getInterest() > i2->getInterest();
}

static bool proximity_comparator(const LLViewerMediaImpl* i1,
								 const LLViewerMediaImpl* i2)
{
	if (i1->getProximityDistance() < i2->getProximityDistance())
	{
		return true;
	}
	if (i1->getProximityDistance() > i2->getProximityDistance())
	{
		return false;
	}
	// Both objects have the same distance. This most likely means they are two
	// faces of the same object. They may also be faces on different objects
	// with exactly the same distance (like HUD objects). We do not actually
	// care what the sort order is for this case, as long as it is stable and
	// does not change when you enable/disable media. Comparing the impl
	// pointers gives a completely arbitrary ordering, but it will be stable.
	return i1 < i2;
}

//static
void LLViewerMedia::updateMedia(void*)
{
	LL_FAST_TIMER(FTM_MEDIA_UPDATE);

	if (gDisconnected || LLApp::isExiting())
	{
		setAllMediaEnabled(false);
		return;
	}

	// Enable/disable the plugin read thread
	static LLCachedControl<bool> plugin_use_read_thread(gSavedSettings,
														"PluginUseReadThread");
	LLPluginProcessParent::setUseReadThread(plugin_use_read_thread);

	impl_list::iterator iter = sViewerMediaImplList.begin();
	impl_list::iterator end = sViewerMediaImplList.end();

	sAnyMediaShowing = sAnyMediaEnabled = sAnyMediaDisabled = false;

	{
		LL_FAST_TIMER(FTM_MEDIA_UPDATE_INTEREST);
		while (iter != end)
		{
			LLViewerMediaImpl* pimpl = *iter++;
			pimpl->update();
			pimpl->calculateInterest();
		}
	}

	{
		LL_FAST_TIMER(FTM_MEDIA_SORT);
		// Sort the static instance list using our interest criteria
		sViewerMediaImplList.sort(priorityComparator);
	}

	// Go through the list again and adjust according to priority.
	iter = sViewerMediaImplList.begin();
	end = sViewerMediaImplList.end();

	F64 total_cpu = 0.0;
	U32 impl_count_total = 0;
	U32 impl_count_interest_low = 0;
	U32 impl_count_interest_normal = 0;

	std::vector<LLViewerMediaImpl*> proximity_order;

	static LLCachedControl<bool> inworld_media_enabled(gSavedSettings,
													   "EnableStreamingMedia");
	static LLCachedControl<bool> inworld_audio_enabled(gSavedSettings,
													   "EnableStreamingMusic");
	static LLCachedControl<U32> max_instances(gSavedSettings,
											  "PluginInstancesTotal");
	static LLCachedControl<U32> max_normal(gSavedSettings,
										   "PluginInstancesNormal");
	static LLCachedControl<U32> max_low(gSavedSettings, "PluginInstancesLow");
	static LLCachedControl<F32> max_cpu(gSavedSettings,
										"PluginInstancesCPULimit");

	// Setting max_cpu to 0.0 disables CPU usage checking.
	bool check_cpu_usage = max_cpu != 0.f;

	LLViewerMediaImpl* lowest_interest_loadable = NULL;

	// Notes on tweakable params:
	// max_instances must be set high enough to allow the various instances
	// Used in the UI (for the help browser, search, etc) to be loaded. If
	// max_normal + max_low is less than max_instances, things will tend to get
	// unloaded instead of being set to slideshow.
	{
		LL_FAST_TIMER(FTM_MEDIA_MISC);
		LLPluginClassMedia::EPriority new_priority;
		for ( ; iter != end; ++iter)
		{
			LLViewerMediaImpl* pimpl = *iter;

			new_priority = LLPluginClassMedia::PRIORITY_NORMAL;

			if (pimpl->isForcedUnloaded() || impl_count_total >= max_instances)
			{
				// Never load muted or failed impls. Hard limit on the number
				// of instances that will be loaded at one time
				new_priority = LLPluginClassMedia::PRIORITY_UNLOADED;
			}
			else if (!pimpl->getVisible())
			{
				new_priority = LLPluginClassMedia::PRIORITY_HIDDEN;
			}
			else if (pimpl->hasFocus())
			{
				new_priority = LLPluginClassMedia::PRIORITY_HIGH;
				// Count this against the count of "normal" instances for
				// priority purposes
				++impl_count_interest_normal;
			}
			else if (pimpl->getUsedInUI() || pimpl->getUsedOnHUD() ||
					 pimpl->isParcelMedia())
			{
				new_priority = LLPluginClassMedia::PRIORITY_NORMAL;
				++impl_count_interest_normal;
			}
			else
			{
				// Look at interest and CPU usage for instances that are not in
				// any of the above states.
				// Heuristic: if the media texture's approximate screen area is
				// less than 1/4 of the native area of the texture, turn it
				// down to low instead of normal. This may downsample for
				// plugins that support it.
				bool media_is_small = false;
				F64 approx_interest = pimpl->getApproximateTextureInterest();
				if (approx_interest == 0.f)
				{
					// This media has no current size, which probably means it
					// is not loaded.
					media_is_small = true;
				}
				else if (pimpl->getInterest() < approx_interest / 4)
				{
					media_is_small = true;
				}

				if (pimpl->getInterest() == 0.f)
				{
					// This media is completely invisible, due to being outside
					// the view frustum or out of range.
					new_priority = LLPluginClassMedia::PRIORITY_HIDDEN;
				}
				else if (check_cpu_usage && total_cpu > max_cpu)
				{
					// Higher priority plugins have already used up the CPU
					// budget. Set remaining ones to slideshow priority.
					new_priority = LLPluginClassMedia::PRIORITY_SLIDESHOW;
				}
				else if (!media_is_small &&
						 impl_count_interest_normal < max_normal)
				{
					// Up to max_normal inworld get normal priority
					new_priority = LLPluginClassMedia::PRIORITY_NORMAL;
					++impl_count_interest_normal;
				}
				else if (impl_count_interest_low + impl_count_interest_normal <
							max_low + max_normal)
				{
					// The next max_low inworld get turned down
					new_priority = LLPluginClassMedia::PRIORITY_LOW;
					++impl_count_interest_low;

					// Set the low priority size for downsampling to
					// approximately the size the texture is displayed at.
					{
						F32 dimension = sqrtf(pimpl->getInterest());
						pimpl->setLowPrioritySizeLimit(ll_roundp(dimension));
					}
				}
				else
				{
					// Any additional impls (up to max_instances) get very
					// infrequent time
					new_priority = LLPluginClassMedia::PRIORITY_SLIDESHOW;
				}
			}

			if (!pimpl->getUsedInUI() &&
				new_priority != LLPluginClassMedia::PRIORITY_UNLOADED)
			{
				// This is a loadable inworld impl -- the last one in the list
				// in this class defines the lowest loadable interest.
				lowest_interest_loadable = pimpl;
				++impl_count_total;
			}

			// Overrides if the window is minimized or we lost focus (taking
			// care not to accidentally "raise" the priority either)
			if (!gViewerWindowp->getActive() && // viewer window minimized ?
				new_priority > LLPluginClassMedia::PRIORITY_HIDDEN)
			{
				new_priority = LLPluginClassMedia::PRIORITY_HIDDEN;
			}
			else if (!gFocusMgr.getAppHasFocus() && // viewer win lost focus ?
					 new_priority > LLPluginClassMedia::PRIORITY_LOW)
			{
				new_priority = LLPluginClassMedia::PRIORITY_LOW;
			}

			if (!inworld_media_enabled)
			{
				// If inworld media is locked out, force all inworld media to
				// stay unloaded.
				if (!pimpl->getUsedInUI())
				{
					new_priority = LLPluginClassMedia::PRIORITY_UNLOADED;
				}
			}
			// Update the audio stream here as well
			if (!inworld_audio_enabled)
			{
				if (LLViewerParcelMedia::hasParcelAudio() &&
					LLViewerParcelMedia::isParcelAudioPlaying())
				{
					LLViewerParcelMedia::stopStreamingMusic();
				}
			}
			pimpl->setPriority(new_priority);

			if (pimpl->getUsedInUI())
			{
				// Any impl used in the UI should not be in the proximity list.
				pimpl->mProximity = -1;
			}
			else
			{
				proximity_order.push_back(pimpl);
			}

			total_cpu += pimpl->getCPUUsage();

			if (!pimpl->getUsedInUI())
			{
				if (pimpl->hasMedia())
				{
					sAnyMediaShowing = true;
				}
				if (pimpl != LLViewerParcelMedia::getParcelMedia())
				{
					if (pimpl->isMediaDisabled())
					{
						sAnyMediaDisabled = true;
					}
					else
					{
						sAnyMediaEnabled = true;
					}
				}
			}
		}
	}

	// Re-calculate this every time.
	sLowestLoadableImplInterest	= 0.f;

	// Only do this calculation if we have hit the impl count limit; up until
	// that point we always need to load media data.
	if (lowest_interest_loadable && impl_count_total >= (U32)max_instances)
	{
		// Get the interest value of this impl's object for use by
		// isInterestingEnough
		LLVOVolume* object = lowest_interest_loadable->getSomeObject();
		if (object)
		{
			// NOTE: Do not use getMediaInterest() here. We want the pixel
			// area, not the total media interest, so that we match up with the
			// calculation done in LLMediaDataClient.
			sLowestLoadableImplInterest = object->getPixelArea();
		}
	}

	{
		LL_FAST_TIMER(FTM_MEDIA_SORT2);
		// Use a distance-based sort for proximity values.
		std::stable_sort(proximity_order.begin(), proximity_order.end(),
						 proximity_comparator);
	}

	// Transfer the proximity order to the proximity fields in the objects.
	for (S32 i = 0, count = proximity_order.size(); i < count; ++i)
	{
		proximity_order[i]->mProximity = i;
	}

	LL_DEBUGS("PluginPriority") << "Total reported CPU usage is " << total_cpu
								<< LL_ENDL;
}

//static
void LLViewerMedia::setAllMediaEnabled(bool enable, bool parcel_media)
{
	// Set "tentative" autoplay first. We need to do this here or else
	// re-enabling would not start up the media below.
	gSavedSettings.setBool("MediaTentativeAutoPlay", enable);

	for (impl_list::iterator iter = sViewerMediaImplList.begin(),
							 end = sViewerMediaImplList.end();
		 iter != end; ++iter)
	{
		LLViewerMediaImpl* pimpl = *iter;
		if (!pimpl->getUsedInUI() &&
			(parcel_media || pimpl != LLViewerParcelMedia::getParcelMedia()))
		{
			pimpl->setDisabled(!enable);
		}
	}

	if (!parcel_media)
	{
		return;
	}

	// Also do Parcel Media and Parcel Audio
	if (enable)
	{
		LLParcel* parcel = gViewerParcelMgr.getAgentParcel();
		if (LLViewerParcelMedia::hasParcelMedia() &&
			!LLViewerParcelMedia::isParcelMediaPlaying())
		{
			LLViewerParcelMedia::playMedia(parcel);
		}

		if (LLViewerParcelMedia::hasParcelAudio() &&
			!LLViewerParcelMedia::isParcelAudioPlaying())
		{
			LLViewerParcelMedia::playStreamingMusic(parcel);
		}
	}
	else
	{
		// This actually unloads the impl, as opposed to "stop"ping the media
		LLViewerParcelMedia::stop();
		LLViewerParcelMedia::stopStreamingMusic();
	}
}

//static
void LLViewerMedia::sharedMediaEnable(void*)
{
	setAllMediaEnabled(true, false);
}

//static
void LLViewerMedia::sharedMediaDisable(void*)
{
	setAllMediaEnabled(false, false);
}

//static
void LLViewerMedia::onAuthSubmit(const LLUUID media_id,
								 const std::string username,
								 const std::string password,
								 bool validated)
{
	LLViewerMediaImpl* self =
		LLViewerMedia::getMediaImplFromTextureID(media_id);
	if (self)
	{
		LLPluginClassMedia* media = self->getMediaPlugin();
		if (media)
		{
			if (validated)
			{
				media->sendAuthResponse(true, username, password);
			}
			else
			{
				media->sendAuthResponse(false, "", "");
			}
		}
	}
}

//static
void LLViewerMedia::clearAllCookies()
{
	// The streaming plugins do not use cookies, so they do not implement
	// clear_cookies() and the CEF plugin will only clear its cookies when
	// one such plugin is running while this method is called...
	// Clear all cookies for all plugins
	for (impl_list::iterator iter = sViewerMediaImplList.begin(),
							 end = sViewerMediaImplList.end();
		 iter != end; ++iter)
	{
		LLViewerMediaImpl* pimpl = *iter;
		if (pimpl && pimpl->mMediaSource)
		{
			pimpl->mMediaSource->clear_cookies();
		}
	}

	// Clear the built-in CEF browser cookies (indepedently of the actually
	// running plugins, i.e. this works too when no CEF browser plugin is
	// running); note that this supposes that the cookies directory name (set
	// in media_plugin_cef.cpp) is known: if it changes in the future, this
	// code will have to be modified.

	// Base directory for the logged-off cache:
	std::string cookies_dir = gDirUtilp->getOSUserAppDir();
	// If logged in, clear the corresponding per-user cache:
	std::string linden_user_dir = gDirUtilp->getLindenUserDir();
	if (!linden_user_dir.empty() && LLStartUp::isLoggedIn())
	{
		cookies_dir = linden_user_dir;
	}
	if (cookies_dir.empty())
	{
		llwarns << "Could not determine the cookies directory location. Aborting."
				<< llendl;
		return;
	}

	cookies_dir += LL_DIR_DELIM_STR "cef_cache";
	if (!LLFile::isdir(cookies_dir))
	{
		llinfos << "No CEF cache directory found. No cookies." << llendl;
	}
	else
	{
		LLDirIterator::deleteFilesInDir(cookies_dir, "Cookies*");
	}
}

// Clears the built-in CEF browser cache (there are no caches for streaming
// media plugins, currently). Note that this supposes that the cache directory
// name (set in media_plugin_cef.cpp) and the sub-directories structure (as
// determined by CEF itself) are known: if they change in the future, this code
// will have to be modified.
//static
void LLViewerMedia::clearAllCaches()
{
	// Base directory for the logged-off cache:
	std::string cache_dir = gDirUtilp->getOSUserAppDir();
	// If logged in, clear the corresponding per-user cache:
	std::string linden_user_dir = gDirUtilp->getLindenUserDir();
	if (!linden_user_dir.empty() && LLStartUp::isLoggedIn())
	{
		cache_dir = linden_user_dir;
	}
	if (cache_dir.empty())
	{
		llwarns << "Could not determine the cache directory location. Aborting."
				<< llendl;
		return;
	}

	cache_dir += LL_DIR_DELIM_STR "cef_cache";
	if (!LLFile::isdir(cache_dir))
	{
		llinfos << "No CEF cache directory found." << llendl;
		return;
	}

	// Delete all files in cache *but* the "Cookies*" ones
	LLDirIterator::deleteRecursivelyInDir(cache_dir, "Cookies*", true);
}

//static
void LLViewerMedia::setCookiesEnabled(bool enabled)
{
	// Set the "cookies enabled" flag for all loaded plugins
	for (impl_list::iterator iter = sViewerMediaImplList.begin(),
							 end = sViewerMediaImplList.end();
		 iter != end; ++iter)
	{
		LLViewerMediaImpl* pimpl = *iter;
		if (pimpl->mMediaSource)
		{
			pimpl->mMediaSource->cookies_enabled(enabled);
		}
	}
}

//static
void LLViewerMedia::setProxyConfig(bool enable, const std::string& host,
								   S32 port)
{
	// Set the proxy config for all loaded plugins
	for (impl_list::iterator iter = sViewerMediaImplList.begin(),
							 end = sViewerMediaImplList.end();
		 iter != end; ++iter)
	{
		LLViewerMediaImpl* pimpl = *iter;
		if (pimpl->mMediaSource)
		{
			pimpl->mMediaSource->proxy_setup(enable, host, port);
		}
	}
}

//static
void LLViewerMedia::setOpenIDCookie(const std::string& url)
{
	if (sOpenIDCookie.empty())
	{
		return;
	}

	// We want just the hostname to associate it with the cookie
	std::string cookie_host;
	size_t i = url.find('@');
	if (i != std::string::npos)
	{
		// Hostname starts after the @.
		cookie_host = url.substr(i + 1);
	}
	else
	{
		// No username/password
		i = url.find("://");
		if (i != std::string::npos)
		{
			cookie_host = url.substr(i + 3);
		}
	}
	if (!cookie_host.empty())
	{
		i = cookie_host.find(':');
		if (i == std::string::npos)
		{
			// No port number
			i = cookie_host.find('/');
		}
		if (i != std::string::npos)
		{
			cookie_host = cookie_host.substr(0, i);
		}
	}

	// Set the cookie for all open media controls (works only for the CEF
	// plugin).
	if (!url.empty() && !cookie_host.empty())
	{
		LLMediaCtrl::setOpenIdCookie(url, cookie_host, sOpenIDCookie);
	}
}

//static
void LLViewerMedia::openIDSetup(const std::string& url,
								const std::string& token)
{
	LL_DEBUGS("Media") << "url = \"" << url << "\", token = \""
					   << token << "\"" << LL_ENDL;
	if (!gSavedSettings.getBool("MediaGetOpenID"))
	{
		LL_DEBUGS("Media") << "NOT fetching OpenID, as per viewer settings"
						   << LL_ENDL;
		return;
	}
	gCoros.launch("LLViewerMedia::openIDSetupCoro",
				  boost::bind(&LLViewerMedia::openIDSetupCoro, url, token));
}

//static
void LLViewerMedia::openIDSetupCoro(std::string url, const std::string& token)
{
	LLCore::HttpOptions::ptr_t options(new LLCore::HttpOptions);
	options->setWantHeaders(true);

	LLCore::HttpHeaders::ptr_t headers(new LLCore::HttpHeaders);
	headers->append(HTTP_OUT_HEADER_ACCEPT, "*/*");
	headers->append(HTTP_OUT_HEADER_CONTENT_TYPE,
					"application/x-www-form-urlencoded");

	LLCore::BufferArray::ptr_t rawbody(new LLCore::BufferArray);
	LLCore::BufferArrayStream bas(rawbody.get());
	bas << std::noskipws << token;

	LLCoreHttpUtil::HttpCoroutineAdapter adapter("openIDSetupCoro");
	LLSD result = adapter.postRawAndSuspend(url, rawbody, options,
											headers);
	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	if (!status)
	{
		llwarns << "Error getting Open ID cookie: " << status.toString()
				<< llendl;
		return;
	}

	const LLSD& httpres =
		result[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS];
	const LLSD& header =
		httpres[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS_HEADERS];
	if (!header.has(HTTP_IN_HEADER_SET_COOKIE))
	{
		llwarns << "No cookie in response." << llendl;
		return;
	}

	// We do not care about the content of the response, only the Set-Cookie
	// header.
	const std::string& cookie = header[HTTP_IN_HEADER_SET_COOKIE].asStringRef();
	// *TODO: What about bad status codes ?  Does this destroy previous
	// cookies ?
	LL_DEBUGS("Media") << "Cookie = " << cookie << LL_ENDL;
	openIDCookieResponse(url, cookie);
}

//static
void LLViewerMedia::openIDCookieResponse(const std::string& url,
										 const std::string& cookie)
{
	LL_DEBUGS("Media") << "Cookie received: \"" << cookie << "\"" << LL_ENDL;
	sOpenIDCookie += cookie;
	setOpenIDCookie(url);
}

bool LLViewerMedia::hasInWorldMedia()
{
	// This should be quick, because there should be very few non in-world
	// media impls
	for (impl_list::iterator iter = sViewerMediaImplList.begin(),
							 end = sViewerMediaImplList.end();
		 iter != end; ++iter)
	{
		LLViewerMediaImpl* pimpl = *iter;
		if (pimpl && !pimpl->getUsedInUI() && !pimpl->isParcelMedia())
		{
			// Found an in-world media impl
			return true;
		}
	}
	return false;
}

//static
void LLViewerMedia::initClass()
{
	gIdleCallbacks.addFunction(LLViewerMedia::updateMedia, NULL);
	sTeleportFinishConnection =
		gViewerParcelMgr.setTPFinishedCallback(boost::bind(&LLViewerMedia::onTeleportFinished));
}

//static
void LLViewerMedia::cleanupClass()
{
	gIdleCallbacks.deleteFunction(LLViewerMedia::updateMedia, NULL);
	sTeleportFinishConnection.disconnect();
}

//static
void LLViewerMedia::onTeleportFinished()
{
	// On teleport, clear this setting (i.e. set it to true)
	gSavedSettings.setBool("MediaTentativeAutoPlay", true);
}

//static
void LLViewerMedia::setOnlyAudibleMediaTextureID(const LLUUID& texture_id)
{
	sOnlyAudibleTextureID = texture_id;
	sForceUpdate = true;
}

///////////////////////////////////////////////////////////////////////////////
// Media filter implementation:
///////////////////////////////////////////////////////////////////////////////

//static
bool LLViewerMedia::allowedMedia(std::string media_url)
{
	LLStringUtil::trim(media_url);
	std::string domain = extractDomain(media_url);
	std::string ip = getDomainIP(domain); // maybe == domain
	if (sAllowedMedia.count(domain) || sAllowedMedia.count(ip))
	{
		return true;
	}
	std::string server;
	for (S32 i = 0, count = sMediaFilterList.size(); i < count; ++i)
	{
		server = sMediaFilterList[i]["domain"].asString();
		if (server == domain || server == ip)
		{
			return (sMediaFilterList[i]["action"].asString() == "allow");
		}
	}
	return false;
}

void callback_parcel_media_alert(const LLSD& notification,
								 const LLSD& response,
								 LLParcel* parcel,
								 U32 type,
								 std::string domain)
{
	S32 option = LLNotification::getSelectedOption(notification, response);

	std::string ip = LLViewerMedia::getDomainIP(domain);

	LLSD args;
	if (ip != domain && domain.find('/') == std::string::npos)
	{
		args["DOMAIN"] = domain + " (" + ip + ")";
	}
	else
	{
		args["DOMAIN"] = domain;
	}

	if (option == 0 || option == 3) // Allow or Whitelist
	{
		LLViewerMedia::sAllowedMedia.emplace(domain);
		if (option == 3) // Whitelist
		{
			LLSD newmedia;
			newmedia["domain"] = domain;
			newmedia["action"] = "allow";
			LLViewerMedia::sMediaFilterList.append(newmedia);
			if (ip != domain && domain.find('/') == std::string::npos)
			{
				newmedia["domain"] = ip;
				LLViewerMedia::sMediaFilterList.append(newmedia);
			}
			LLViewerMedia::saveDomainFilterList();
			args["LISTED"] = "whitelisted";
			gNotifications.add("MediaListed", args);
		}
		if (parcel == gViewerParcelMgr.getAgentParcel())
		{
			if (type == 0)
			{
				LLViewerParcelMedia::playMedia(parcel, false);
			}
			else
			{
				LLViewerParcelMedia::playStreamingMusic(parcel, false);
			}
		}
	}
	else if (option == 1 || option == 2) // Deny or Blacklist
	{
		LLViewerMedia::sDeniedMedia.emplace(domain);
		if (ip != domain && domain.find('/') == std::string::npos)
		{
			LLViewerMedia::sDeniedMedia.emplace(ip);
		}
		if (type == 1 && parcel == gViewerParcelMgr.getAgentParcel())
		{
			LLViewerParcelMedia::stopStreamingMusic();
		}
		if (option == 1) // Deny
		{
			gNotifications.add("MediaBlocked", args);
		}
		else // Blacklist
		{
			LLSD newmedia;
			newmedia["domain"] = domain;
			newmedia["action"] = "deny";
			LLViewerMedia::sMediaFilterList.append(newmedia);
			if (ip != domain && domain.find('/') == std::string::npos)
			{
				newmedia["domain"] = ip;
				LLViewerMedia::sMediaFilterList.append(newmedia);
			}
			LLViewerMedia::saveDomainFilterList();
			args["LISTED"] = "blacklisted";
			gNotifications.add("MediaListed", args);
		}
	}

	LLViewerMedia::sMediaQueries.erase(domain);
	SLFloaterMediaFilter::setDirty();
}

//static
void LLViewerMedia::filterParcelMedia(LLParcel* parcel, U32 type)
{
	if (parcel != gViewerParcelMgr.getAgentParcel())
	{
		// The parcel just changed (may occur right out after a TP)
		sIsUserAction = false;
		return;
	}

	std::string media_url;
	if (type == 0)
	{
		media_url = parcel->getMediaURL();
	}
	else
	{
		media_url = parcel->getMusicURL();
	}
	LLStringUtil::trim(media_url);

	std::string domain = extractDomain(media_url);

	if (sMediaQueries.count(domain) > 0)
	{
		sIsUserAction = false;
		return;
	}

	std::string ip = getDomainIP(domain);

	if (sIsUserAction)
	{
		// This was a user manual request to play this media, so give it
		// another chance...
		sIsUserAction = false;
		bool dirty = false;
		if (sDeniedMedia.count(domain))
		{
			sDeniedMedia.erase(domain);
			dirty = true;
		}
		if (sDeniedMedia.count(ip))
		{
			sDeniedMedia.erase(ip);
			dirty = true;
		}
		if (dirty)
		{
			SLFloaterMediaFilter::setDirty();
		}
	}

	std::string media_action;
	if (media_url.empty())
	{
		media_action = "allow";
	}
	else if (!sMediaFilterListLoaded || sDeniedMedia.count(domain) ||
			 sDeniedMedia.count(ip))
	{
		media_action = "ignore";
	}
	else if (sAllowedMedia.count(domain) || sAllowedMedia.count(ip))
	{
		media_action = "allow";
	}
	else
	{
		std::string server;
		for (S32 i = 0, count = sMediaFilterList.size(); i < count; ++i)
		{
			server = sMediaFilterList[i]["domain"].asString();
			if (server == domain || server == ip)
			{
				media_action = sMediaFilterList[i]["action"].asString();
				break;
			}
		}
	}

	if (media_action == "allow")
	{
		if (type == 0)
		{
			LLViewerParcelMedia::playMedia(parcel, false);
		}
		else
		{
			LLViewerParcelMedia::playStreamingMusic(parcel, false);
		}
		return;
	}
	if (media_action == "ignore")
	{
		if (type == 1)
		{
			LLViewerParcelMedia::stopStreamingMusic();
		}
		return;
	}

	LLSD args;
	if (ip != domain && domain.find('/') == std::string::npos)
	{
		args["DOMAIN"] = domain + " (" + ip + ")";
	}
	else
	{
		args["DOMAIN"] = domain;
	}

	if (media_action == "deny")
	{
		gNotifications.add("MediaBlocked", args);
		if (type == 1)
		{
			LLViewerParcelMedia::stopStreamingMusic();
		}
		// So to avoid other "blocked" messages later in the session
		// for this url should it be requested again by a script.
		// We do not add the IP, on purpose (want to show different
		// blocks for different domains pointing to the same IP).
		sDeniedMedia.emplace(domain);
	}
	else
	{
		sMediaQueries.emplace(domain);
		args["URL"] = media_url;
		if (type == 0)
		{
			args["TYPE"] = "media";
		}
		else
		{
			args["TYPE"] = "audio";
		}
		gNotifications.add("ParcelMediaAlert", args, LLSD(),
						   boost::bind(callback_parcel_media_alert, _1, _2,
									   parcel, type, domain));
	}
}

void callback_media_alert(const LLSD& notification, const LLSD& response,
						  LLViewerMediaImpl* impl, std::string domain)
{
	S32 option = LLNotification::getSelectedOption(notification, response);

	std::string ip = LLViewerMedia::getDomainIP(domain);

	LLSD args;
	if (ip != domain && domain.find('/') == std::string::npos)
	{
		args["DOMAIN"] = domain + " (" + ip + ")";
	}
	else
	{
		args["DOMAIN"] = domain;
	}

	if (option == 0 || option == 3) // Allow or Whitelist
	{
		LLViewerMedia::sAllowedMedia.emplace(domain);
		if (option == 3) // Whitelist
		{
			LLSD newmedia;
			newmedia["domain"] = domain;
			newmedia["action"] = "allow";
			LLViewerMedia::sMediaFilterList.append(newmedia);
			if (ip != domain && domain.find('/') == std::string::npos)
			{
				newmedia["domain"] = ip;
				LLViewerMedia::sMediaFilterList.append(newmedia);
			}
			LLViewerMedia::saveDomainFilterList();
			args["LISTED"] = "whitelisted";
			gNotifications.add("MediaListed", args);
		}
		if (is_media_impl_valid(impl))
		{
			impl->navigateInternal();
		}
	}
	else if (option == 1 || option == 2) // Deny or Blacklist
	{
		LLViewerMedia::sDeniedMedia.emplace(domain);
		if (ip != domain && domain.find('/') == std::string::npos)
		{
			LLViewerMedia::sDeniedMedia.emplace(ip);
		}
		if (option == 1) // Deny
		{
			gNotifications.add("MediaBlocked", args);
		}
		else // Blacklist
		{
			LLSD newmedia;
			newmedia["domain"] = domain;
			newmedia["action"] = "deny";
			LLViewerMedia::sMediaFilterList.append(newmedia);
			if (ip != domain && domain.find('/') == std::string::npos)
			{
				newmedia["domain"] = ip;
				LLViewerMedia::sMediaFilterList.append(newmedia);
			}
			LLViewerMedia::saveDomainFilterList();
			args["LISTED"] = "blacklisted";
			gNotifications.add("MediaListed", args);
		}
		if (is_media_impl_valid(impl))
		{
			impl->setDisabled(true);
		}
	}

	LLViewerMedia::sMediaQueries.erase(domain);
	SLFloaterMediaFilter::setDirty();
}

//static
bool LLViewerMedia::filterMedia(LLViewerMediaImpl* impl)
{
	if (!is_media_impl_valid(impl))
	{
		return true;
	}

	std::string media_url = impl->getMediaURL();
	LLStringUtil::trim(media_url);
	if (media_url.find("://") == std::string::npos)
	{
		// That's a filename...
		return false;
	}

	LLURI uri(media_url);
	std::string scheme = uri.scheme();
	if (scheme == "data" || scheme == "file" || scheme == "about")
	{
		return false;
	}

	std::string domain = extractDomain(media_url);

	if (sMediaQueries.count(domain) > 0 || !sMediaFilterListLoaded)
	{
		// Pending actions in progress, deny for now.
		return true;
	}
	std::string ip = getDomainIP(domain);

	std::string media_action;
	if (media_url.empty())
	{
		media_action = "allow";
	}
	else if (sDeniedMedia.count(domain) || sDeniedMedia.count(ip))
	{
		media_action = "ignore";
	}
	else if (sAllowedMedia.count(domain) || sAllowedMedia.count(ip))
	{
		media_action = "allow";
	}
	else
	{
		std::string server;
		for (S32 i = 0, count = sMediaFilterList.size(); i < count; ++i)
		{
			server = sMediaFilterList[i]["domain"].asString();
			if (server == domain || server == ip)
			{
				media_action = sMediaFilterList[i]["action"].asString();
				break;
			}
		}
	}

	if (media_action == "allow")
	{
		return false;
	}
	if (media_action == "ignore")
	{
		impl->setDisabled(true);
		return true;
	}

	LLSD args;
	if (ip != domain && domain.find('/') == std::string::npos)
	{
		args["DOMAIN"] = domain + " (" + ip + ")";
	}
	else
	{
		args["DOMAIN"] = domain;
	}

	if (media_action == "deny")
	{
		gNotifications.add("MediaBlocked", args);
		// So to avoid other "blocked" messages later in the session
		// for this url should it be requested again by a script.
		// We do not add the IP, on purpose (want to show different
		// blocks for different domains pointing to the same IP).
		sDeniedMedia.emplace(domain);
		impl->setDisabled(true);
		return true;
	}
	else
	{
		sMediaQueries.emplace(domain);
		args["URL"] = media_url;
		gNotifications.add("MediaAlert", args, LLSD(),
						   boost::bind(callback_media_alert, _1, _2, impl,
									   domain));
		return true;
	}
}

//static
void LLViewerMedia::saveDomainFilterList()
{
	std::string medialist_filename = gDirUtilp->getExpandedFilename(LL_PATH_PER_ACCOUNT,
																	"media_filter.xml");

	llofstream medialistFile(medialist_filename.c_str());
	if (medialistFile.is_open())
	{
		LLSDSerialize::toPrettyXML(sMediaFilterList, medialistFile);
		medialistFile.close();
	}
	else
	{
		llwarns << "Could not open file '" << medialist_filename
				<< "' for writing." << llendl;
	}
}

//static
bool LLViewerMedia::loadDomainFilterList()
{
	sMediaFilterListLoaded = true;

	std::string medialist_filename = gDirUtilp->getExpandedFilename(LL_PATH_PER_ACCOUNT,
																	"media_filter.xml");

	if (!LLFile::isfile(medialist_filename))
	{
		LLSD emptyllsd;
		llofstream medialistFile(medialist_filename.c_str());
		if (medialistFile.is_open())
		{
			LLSDSerialize::toPrettyXML(emptyllsd, medialistFile);
			medialistFile.close();
		}
		else
		{
			llwarns << "Could not open file '" << medialist_filename
					<< "' for writing." << llendl;
		}
	}

	if (LLFile::isfile(medialist_filename))
	{
		llifstream medialistFile(medialist_filename.c_str());
		if (medialistFile.is_open())
		{
			LLSDSerialize::fromXML(sMediaFilterList, medialistFile);
			medialistFile.close();
		}
		SLFloaterMediaFilter::setDirty();
		return true;
	}
	else
	{
		return false;
	}
}

//static
void LLViewerMedia::clearDomainFilterList()
{
	sMediaFilterList.clear();
	sAllowedMedia.clear();
	sDeniedMedia.clear();
	saveDomainFilterList();
	gNotifications.add("MediaFiltersCleared");
	SLFloaterMediaFilter::setDirty();
}

//static
std::string LLViewerMedia::extractDomain(std::string url)
{
	static std::string last_region = "@";

	if (url.empty())
	{
		return url;
	}

	LLStringUtil::toLower(url);

	size_t pos = url.find("//");

	if (pos != std::string::npos)
	{
		size_t count = url.size() - pos + 2;
		url = url.substr(pos + 2, count);
	}

	// Check that there is at least one slash in the URL and add a trailing
	// one if not (for media/audio URLs such as http://mydomain.net)
	if (url.find('/') == std::string::npos)
	{
		url += '/';
	}

	// If there's a user:password@ part, remove it
	pos = url.find('@');
	if (pos != std::string::npos && pos < url.find('/'))
	{
		// if '@' is not before the first '/', then it's not a user:password
		size_t count = url.size() - pos + 1;
		url = url.substr(pos + 1, count);
	}

	
	const LLHost& host = gAgent.getRegionHost();
	if (host.isOk() &&
		(url.find(host.getHostName()) == 0 || url.find(last_region) == 0))
	{
		// This must be a scripted object rezzed in the region:
		// extend the concept of "domain" to encompass the scripted object
		// server Id and avoid blocking all other objects at once in this
		// region...

		// Get rid of any port number
		pos = url.find('/');	// We earlier made sure that there is one
		url = host.getHostName() + url.substr(pos);

		pos = url.find('?');
		if (pos != std::string::npos)
		{
			// Get rid of any parameter
			url = url.substr(0, pos);
		}

		pos = url.rfind('/');
		if (pos != std::string::npos)
		{
			// Get rid of the filename, if any, keeping only the server + path
			url = url.substr(0, pos);
		}
	}
	else
	{
		pos = url.find(':');
		if (pos != std::string::npos && pos < url.find('/'))
		{
			// Keep anything before the port number and strip the rest off
			url = url.substr(0, pos);
		}
		else
		{
			pos = url.find('/');	// We earlier made sure that there's one
			url = url.substr(0, pos);
		}
	}

	// Remember this region, so to cope with requests occuring just after a
	// TP out of it.
	if (host.isOk())
	{
		last_region = host.getHostName();
	}

	return url;
}

//static
std::string LLViewerMedia::getDomainIP(const std::string& domain, bool force)
{
	std::string ip = domain;	// Default for no lookups or IP domains

	static const std::regex ipv4("\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}");
	// Check to see if the domain is already an IP
	try
	{
		if (std::regex_match(domain, ipv4))
		{
			return ip;
		}
	}
	catch (std::regex_error& e)
	{
		llwarns << "Regex error: " << e.what() << llendl;
	}

	// The domain is a name, not an IP. Make a DNS lookup.
	std::map<std::string, std::string>::iterator it = sDNSlookups.find(domain);
	if (it != sDNSlookups.end())
	{
		ip = it->second;
	}
	else if (force || gSavedSettings.getBool("MediaLookupIP"))
	{
		// Lookup the domain to get its IP.
		// This incurs a short pause (one second or so) on succesful lookups
		// and a long pause (several seconds) on failing lookups (bad domain).
		LLHost host;
		host.setHostByName(domain);
		ip = host.getIPString();

		// Cache this (domain, ip) pair for later lookups
		sDNSlookups.emplace(domain, ip);
	}

	return ip;
}

///////////////////////////////////////////////////////////////////////////////
// LLViewerMediaImpl
///////////////////////////////////////////////////////////////////////////////

LLViewerMediaImpl::LLViewerMediaImpl(const LLUUID& texture_id,
									 S32 media_width,
									 S32 media_height,
									 bool media_auto_scale,
									 bool media_loop)
:	mMediaSource(NULL),
	mMovieImageHasMips(false),
	mMediaWidth(media_width),
	mMediaHeight(media_height),
	mMediaAutoScale(media_auto_scale),
	mMediaLoop(media_loop),
	mNeedsNewTexture(true),
	mTextureUsedWidth(0),
	mTextureUsedHeight(0),
	mSuspendUpdates(false),
	mTextureUpdatePending(false),
	mVisible(true),
	mLastSetCursor(UI_CURSOR_ARROW),
	mMediaNavState(MEDIANAVSTATE_NONE),
	mInterest(0.f),
	mUsedInUI(false),
	mUsedOnHUD(false),
	mHasFocus(false),
	mPriority(LLPluginClassMedia::PRIORITY_UNLOADED),
	mNavigateRediscoverType(false),
	mNavigateServerRequest(false),
	mMediaSourceFailed(false),
	mRequestedVolume(1.f),
	mPreviousVolume(1.f),
	mIsMuted(false),
	mNeedsMuteCheck(false),
	mPreviousMediaState(MEDIA_NONE),
	mPreviousMediaTime(0.f),
	mIsDisabled(false),
	mIsParcelMedia(false),
	mProximity(-1),
	mProximityDistance(0.f),
	mMediaAutoPlay(false),
	mInNearbyMediaList(false),
	mBackgroundColor(LLColor4::black), // Do not set to white or may get "white flash" bug.
	mNavigateSuspended(false),
	mNavigateSuspendedDeferred(false),
	mIsUpdated(false),
	mTrustedBrowser(false),
	mZoomFactor(1.0),
	mFilterURL(true),
	mMimeProbe()
{
	// Set up the mute list observer if it has not been set up already.
	if (!sViewerMediaMuteListObserverInitialized)
	{
		LLMuteList::addObserver(&sViewerMediaMuteListObserver);
		sViewerMediaMuteListObserverInitialized = true;
	}

	add_media_impl(this);
	setTextureID(texture_id);

	// Connect this impl to the media texture, creating it if it does not
	// exist. This is necessary because we need to be able to use
	// getMaxVirtualSize() even if the media plugin is not loaded.
	LLViewerMediaTexture* media_tex =
		LLViewerTextureManager::getMediaTexture(mTextureId);
	if (media_tex)
	{
		media_tex->setMediaImpl();
	}
}

LLViewerMediaImpl::~LLViewerMediaImpl()
{
	destroyMediaSource();

	LLViewerMediaTexture::removeMediaImplFromTexture(mTextureId);
	setTextureID();

	remove_media_impl(this);
}

void LLViewerMediaImpl::emitEvent(LLPluginClassMedia* plugin,
								  LLViewerMediaObserver::EMediaEvent event)
{
	// Broadcast to observers using the superclass version
	LLViewerMediaEventEmitter::emitEvent(plugin, event);

	// If this media is on one or more LLVOVolume objects, tell them about the
	// event as well.
	for (std::list<LLVOVolume*>::iterator iter = mObjectList.begin(),
										  end = mObjectList.end();
		 iter != end; )
	{
		LLVOVolume* self = *iter++;
		self->mediaEvent(this, plugin, event);
	}
}

bool LLViewerMediaImpl::initializeMedia(const std::string& mime_type)
{
	bool mimeTypeChanged = (mMimeType != mime_type);
	bool pluginChanged = (LLMIMETypes::implType(mCurrentMimeType) !=
						  LLMIMETypes::implType(mime_type));

	if (!mMediaSource || pluginChanged)
	{
		if (!initializePlugin(mime_type))
		{
			llwarns << "plugin intialization failed for mime type: "
					<< mime_type << llendl;
			return false;
		}
	}
	else if (mimeTypeChanged)
	{
		// The same plugin should be able to handle the new media,
		// just update the stored mime type.
		mMimeType = mime_type;
	}

	return mMediaSource != NULL;
}

void LLViewerMediaImpl::createMediaSource()
{
	if (mPriority == LLPluginClassMedia::PRIORITY_UNLOADED)
	{
		// This media should not be created yet.
		return;
	}
	if (!mMediaURL.empty())
	{
		navigateInternal();
	}
	else if (!mMimeType.empty())
	{
		if (!initializeMedia(mMimeType))
		{
			llwarns << "Failed to initialize media for mime type " << mMimeType
					<< llendl;
		}
	}
}

void LLViewerMediaImpl::destroyMediaSource()
{
	mNeedsNewTexture = true;

	if (mTextureId.notNull())
	{
		// Tell the viewer media texture it's no longer active
		LLViewerMediaTexture* tex = LLViewerTextureManager::findMediaTexture(mTextureId);
		if (tex)
		{
			tex->setPlaying(false);
		}
	}

	cancelMimeTypeProbe();

	mLock.lock();		// Delay tear-down while bg thread is updating
	if (mMediaSource)
	{
		mMediaSource->setDeleteOK(true);
		delete mMediaSource;
		mMediaSource = NULL;
	}
	mLock.unlock();
}

void LLViewerMediaImpl::setMediaType(const std::string& media_type)
{
	mMimeType = media_type;
}

//static
LLPluginClassMedia* LLViewerMediaImpl::newSourceFromMediaType(std::string media_type,
															  // NOTE: owner may be NULL
															  LLPluginClassMediaOwner* owner,
															  S32 default_width,
															  S32 default_height,
															  const std::string target)
{
	std::string plugin_basename = LLMIMETypes::implType(media_type);
	if (plugin_basename.empty())
	{
		llwarns << "Could not find plugin for media type " << media_type
				<< llendl;
	}
	else
	{
		std::string launcher_name = gDirUtilp->getLLPluginLauncher();
		if (plugin_basename == "media_plugin_gstreamer10" ||
			plugin_basename == "media_plugin_libvlc" ||
			plugin_basename == "streaming_plugin")
		{
			plugin_basename = "media_plugin_gstreamer";
		}

		std::string plugin_name =
			gDirUtilp->getLLPluginFilename(plugin_basename);

		std::string user_data_path = gDirUtilp->getOSUserAppDir();
		// Fix for EXT-5960 - make browser profile specific to user (cache,
		// cookies etc). If the linden username returned is blank, that can
		// only mean we are at the login page displaying login Web page or Web
		// browser test via Develop menu. In this case we just use whatever
		// gDirUtilp->getOSUserAppDir() gives us (this is what we always used
		// before this change)
		std::string linden_user_dir = gDirUtilp->getLindenUserDir();
		if (!linden_user_dir.empty() && LLStartUp::isLoggedIn())
		{
			// gDirUtilp->getLindenUserDir() is whole path, not just Linden
			// name
			user_data_path = linden_user_dir;
		}
		user_data_path += LL_DIR_DELIM_STR;

		// See if the plugin executable exists
		if (!LLFile::isfile(launcher_name))
		{
			llwarns_once << "Could not find launcher at " << launcher_name
						 << llendl;
		}
		else if (!LLFile::isfile(plugin_name))
		{
			llwarns_once << "Could not find plugin at " << plugin_name
						 << llendl;
		}
		else
		{
			LLPluginClassMedia* media_source = new LLPluginClassMedia(owner);
			media_source->setSize(default_width, default_height);
			media_source->setUserDataPath(user_data_path);
			media_source->setLanguageCode(LLUI::getLanguage());
			if (plugin_basename == "media_plugin_cef")
			{
				media_source->cookies_enabled(gSavedSettings.getBool("CookiesEnabled"));
				media_source->setJavascriptEnabled(gSavedSettings.getBool("BrowserJavascriptEnabled"));
#if CHROME_VERSION_MAJOR < 100
				media_source->setPluginsEnabled(gSavedSettings.getBool("BrowserPluginsEnabled"));
#endif
				media_source->setBrowserUserAgent(LLViewerMedia::getCurrentUserAgent());
#ifdef HB_DULLAHAN_EXTENDED
				media_source->setPreferredFont(gSavedSettings.getString("CEFPreferredFont"));
				media_source->setMinimumFontSize(gSavedSettings.getU32("CEFMinimumFontSize"));
				media_source->setDefaultFontSize(gSavedSettings.getU32("CEFDefaultFontSize"));
				media_source->setRemoteFontsEnabled(gSavedSettings.getBool("CEFRemoteFonts"));
#endif
			}
			media_source->enableMediaPluginDebugging(gSavedSettings.getBool("MediaPluginDebugging"));
			media_source->setTarget(target);

			const std::string plugin_dir = gDirUtilp->getLLPluginDir();
			if (media_source->init(launcher_name, plugin_dir, plugin_name, false))
			{
				return media_source;
			}
			else
			{
				llwarns << "Failed to initialize plugin. Destroying media."
						<< llendl;
				delete media_source;
			}
		}
	}

	static std::set<std::string> warned_missing_types;
	if (!warned_missing_types.count(media_type)) // Warn only once per session
	{
		warned_missing_types.emplace(media_type);
		llwarns << "Plugin intialization failed for mime type: " << media_type
				<< llendl;
		LLSD args;
		args["MIME_TYPE"] = media_type;
		gNotifications.add("NoPlugin", args);
	}

	return NULL;
}

bool LLViewerMediaImpl::initializePlugin(const std::string& media_type)
{
	if (mMediaSource)
	{
		// Save the previous media source's last set size before destroying it.
		mMediaWidth = mMediaSource->getSetWidth();
		mMediaHeight = mMediaSource->getSetHeight();
	}

	// Always delete the old media impl first.
	destroyMediaSource();

	// and unconditionally set the mime type
	mMimeType = media_type;

	if (mPriority == LLPluginClassMedia::PRIORITY_UNLOADED)
	{
		// This impl should not be loaded at this time.
		LL_DEBUGS("PluginPriority") << this
									<< "Not loading (PRIORITY_UNLOADED)"
									<< LL_ENDL;

		return false;
	}

	// If we got here, we want to ignore previous init failures.
	mMediaSourceFailed = false;

	// Save the MIME type that really caused the plugin to load
	mCurrentMimeType = mMimeType;

	LLPluginClassMedia* media_source = newSourceFromMediaType(media_type,
															  this,
															  mMediaWidth,
															  mMediaHeight,
															  mTarget);
	if (media_source)
	{
		media_source->injectOpenIdCookie();
		media_source->setDisableTimeout(gSavedSettings.getBool("DebugPluginDisableTimeout"));
		media_source->setLoop(mMediaLoop);
		media_source->setAutoScale(mMediaAutoScale);
		media_source->focus(mHasFocus);
		media_source->setBackgroundColor(mBackgroundColor);

		media_source->proxy_setup(gSavedSettings.getBool("BrowserProxyEnabled"),
								  gSavedSettings.getString("BrowserProxyAddress"),
								  gSavedSettings.getS32("BrowserProxyPort"));

		if (gSavedSettings.getBool("BrowserIgnoreSSLCertErrors"))
		{
			media_source->ignore_ssl_cert_errors(true);
		}
		// The correct way to deal with certificates it to load ours from
		// ca-bundle.crt and append them to the ones the browser plugin loads
		// from your system location.
		media_source->addCertificateFilePath(gDirUtilp->getCRTFile());

		mMediaSource = media_source;
		mMediaSource->setDeleteOK(false);
		updateVolume();

		return true;
	}

	// Make sure the timer does not try re-initing this plugin repeatedly until
	// something else changes.
	mMediaSourceFailed = true;

	return false;
}

void LLViewerMediaImpl::loadURI()
{
	if (mMediaSource && !mMediaURL.empty())
	{
		// Trim whitespace from front and back of URL - fixes EXT-5363
		LLStringUtil::trim(mMediaURL);
		if (mMediaURL.empty()) return;

		std::string uri = LLURI::escapePathAndData(mMediaURL);

		// Do not log the query parts
		std::string sanitized_uri;
		LLURI u(uri);
		if (u.query().empty())
		{
			sanitized_uri = uri;
		}
		else
		{
			sanitized_uri = u.scheme() + "://" + u.authority() + u.path();
		}
		llinfos << "Asking media source to load URI: " << sanitized_uri
				<< llendl;

		mMediaSource->loadURI(uri);

		// A non-zero mPreviousMediaTime means that either this media was
		// previously unloaded by the priority code while playing/paused, or a
		// seek happened before the media loaded. In either case, seek to the
		// saved time.
		if (mPreviousMediaTime != 0.f)
		{
			seek(mPreviousMediaTime);
		}

		if (mPreviousMediaState == MEDIA_PLAYING)
		{
			// This media was playing before this instance was unloaded.
			start();
		}
		else if (mPreviousMediaState == MEDIA_PAUSED)
		{
			// This media was paused before this instance was unloaded.
			pause();
		}
		else
		{
			// No relevant previous media play state; if we are loading the
			// URL, we want to start playing.
			start();
		}
	}
}

void LLViewerMediaImpl::setSize(int width, int height)
{
	mMediaWidth = width;
	mMediaHeight = height;
	if (mMediaSource)
	{
		mMediaSource->setSize(width, height);
	}
}

void LLViewerMediaImpl::play()
{
	// If the media source is not there, try to initialize it and load an URL.
	if (mMediaSource == NULL)
	{
	 	if (!initializePlugin(mMimeType))
		{
			// Plugin failed initialization... should assert or something
			return;
		}
		// Only do this if the media source was just loaded.
		loadURI();
	}

	// always start the media
	start();
}

void LLViewerMediaImpl::stop()
{
	if (mMediaSource)
	{
		mMediaSource->stop();
		//destroyMediaSource();
	}
}

void LLViewerMediaImpl::pause()
{
	if (mMediaSource)
	{
		mMediaSource->pause();
	}
	else
	{
		mPreviousMediaState = MEDIA_PAUSED;
	}
}

void LLViewerMediaImpl::start()
{
	if (mMediaSource)
	{
		mMediaSource->start();
	}
	else
	{
		mPreviousMediaState = MEDIA_PLAYING;
	}
}

void LLViewerMediaImpl::seek(F32 time)
{
	if (mMediaSource)
	{
		mMediaSource->seek(time);
	}
	else
	{
		// Save the seek time to be set when the media is loaded.
		mPreviousMediaTime = time;
	}
}

void LLViewerMediaImpl::skipBack(F32 step_scale)
{
	if (mMediaSource && mMediaSource->pluginSupportsMediaTime())
	{
		F64 back_step = mMediaSource->getCurrentTime() -
						step_scale * mMediaSource->getDuration();
		if (back_step < 0.0)
		{
			back_step = 0.0;
		}
		mMediaSource->seek(back_step);
	}
}

void LLViewerMediaImpl::skipForward(F32 step_scale)
{
	if (mMediaSource && mMediaSource->pluginSupportsMediaTime())
	{
		F64 forward_step = mMediaSource->getCurrentTime() +
						   step_scale * mMediaSource->getDuration();
		if (forward_step > mMediaSource->getDuration())
		{
			forward_step = mMediaSource->getDuration();
		}
		mMediaSource->seek(forward_step);
	}
}

void LLViewerMediaImpl::setVolume(F32 volume)
{
	mRequestedVolume = volume;
	updateVolume();
}

void LLViewerMediaImpl::setMute(bool mute)
{
	if (mute)
	{
		mPreviousVolume = mRequestedVolume;
		setVolume(0.f);
	}
	else
	{
		setVolume(mPreviousVolume);
	}
}

void LLViewerMediaImpl::updateVolume()
{
	static LLCachedControl<F32> media_roll_off_min(gSavedSettings,
												   "MediaRollOffMin");
	static LLCachedControl<F32> media_roll_off_max(gSavedSettings,
												   "MediaRollOffMax");
	static LLCachedControl<F32> media_roll_off_rate(gSavedSettings,
													"MediaRollOffRate");
	if (mMediaSource)
	{
		// always scale the volume by the global media volume
		F32 volume = mRequestedVolume * LLViewerMedia::getVolume();

		if (mProximityCamera > 0.0)
		{
			if (mProximityCamera > (F64)media_roll_off_max)
			{
				volume = 0.f;
			}
			else if (mProximityCamera > (F64)media_roll_off_min)
			{
				// attenuated_volume = 1 / (roll_off_rate * (d - min))^2
				// the +1 is there so that for distance 0 the volume stays the
				// same
				F64 adjusted_distance = mProximityCamera - media_roll_off_min;
				F64 attenuation = 1.0 + media_roll_off_rate * adjusted_distance;
				attenuation = 1.0 / (attenuation * attenuation);
				// the attenuation multiplier should never be more than one
				// since that would increase volume
				volume = volume * llmin(1.0, attenuation);
			}
		}

		if (sOnlyAudibleTextureID.isNull() ||
			sOnlyAudibleTextureID == mTextureId)
		{
			mMediaSource->setVolume(volume);
		}
		else
		{
			mMediaSource->setVolume(0.f);
		}
	}
}

F32 LLViewerMediaImpl::getVolume()
{
	return mRequestedVolume;
}

void LLViewerMediaImpl::focus(bool focus)
{
	mHasFocus = focus;

	if (mMediaSource)
	{
		// call focus just for the hell of it, even though this apopears to be
		// a nop
		mMediaSource->focus(focus);

#if 0	// Do not do this anymore: it actually clicks through now.
		if (focus)
		{
			// spoof a mouse click to *actually* pass focus
			mMediaSource->mouseEvent(LLPluginClassMedia::MOUSE_EVENT_DOWN,
									 1, 1, 0);
			mMediaSource->mouseEvent(LLPluginClassMedia::MOUSE_EVENT_UP,
									 1, 1, 0);
		}
#endif
	}
}

bool LLViewerMediaImpl::hasFocus() const
{
	// FIXME: This might be able to be a bit smarter by hooking into
	// LLViewerMediaFocus, etc.
	return mHasFocus;
}

void LLViewerMediaImpl::setHomeURL(const std::string& home_url,
								   const std::string& mime_type)
{
	mHomeURL = home_url;
	mHomeMimeType = mime_type;
}

std::string LLViewerMediaImpl::getCurrentMediaURL()
{
	if (!mCurrentMediaURL.empty())
	{
		return mCurrentMediaURL;
	}

	return mMediaURL;
}

void LLViewerMediaImpl::setPageZoomFactor(F64 factor)
{
	if (mMediaSource && factor != mZoomFactor)
	{
		mZoomFactor = factor;
		mMediaSource->set_page_zoom_factor(factor);
	}
}

void LLViewerMediaImpl::mouseDown(S32 x, S32 y, MASK mask, S32 button)
{
	scaleMouse(&x, &y);
	mLastMouseX = x;
	mLastMouseY = y;
	if (mMediaSource)
	{
		if (gDebugClicks)
		{
			llinfos << "Sending event Mouse Down to media" << llendl;
		}
		mMediaSource->mouseEvent(LLPluginClassMedia::MOUSE_EVENT_DOWN, button,
								 x, y, mask);
	}
}

void LLViewerMediaImpl::mouseUp(S32 x, S32 y, MASK mask, S32 button)
{
	scaleMouse(&x, &y);
	mLastMouseX = x;
	mLastMouseY = y;
	if (mMediaSource)
	{
		if (gDebugClicks)
		{
			llinfos << "Sending event Mouse Up to media" << llendl;
		}
		mMediaSource->mouseEvent(LLPluginClassMedia::MOUSE_EVENT_UP, button,
								 x, y, mask);
	}
}

void LLViewerMediaImpl::mouseMove(S32 x, S32 y, MASK mask)
{
	scaleMouse(&x, &y);
	mLastMouseX = x;
	mLastMouseY = y;
	if (mMediaSource)
	{
		mMediaSource->mouseEvent(LLPluginClassMedia::MOUSE_EVENT_MOVE, 0,
								 x, y, mask);
	}
}

//static
void LLViewerMediaImpl::scaleTextureCoords(const LLVector2& texture_coords,
										   S32* x, S32* y)
{
	F32 texture_x = texture_coords.mV[VX];
	F32 texture_y = texture_coords.mV[VY];

	// Deal with repeating textures by wrapping the coordinates into the range
	// [0.0, 1.0)
	texture_x = fmodf(texture_x, 1.f);
	if (texture_x < 0.f)
	{
		texture_x = 1.f + texture_x;
	}

	texture_y = fmodf(texture_y, 1.f);
	if (texture_y < 0.f)
	{
		texture_y = 1.f + texture_y;
	}

	// Scale x and y to texel units.
	*x = ll_round(texture_x * mMediaSource->getTextureWidth());
	*y = ll_round((1.f - texture_y) * mMediaSource->getTextureHeight());

	// Adjust for the difference between the actual texture height and the
	// amount of the texture in use.
	*y -= mMediaSource->getTextureHeight() - mMediaSource->getHeight();
}

void LLViewerMediaImpl::mouseDown(const LLVector2& texture_coords, MASK mask,
								  S32 button)
{
	if (mMediaSource)
	{
		S32 x, y;
		scaleTextureCoords(texture_coords, &x, &y);
		mouseDown(x, y, mask, button);
	}
}

void LLViewerMediaImpl::mouseUp(const LLVector2& texture_coords, MASK mask,
								S32 button)
{
	if (mMediaSource)
	{
		S32 x, y;
		scaleTextureCoords(texture_coords, &x, &y);
		mouseUp(x, y, mask, button);
	}
}

void LLViewerMediaImpl::mouseMove(const LLVector2& texture_coords, MASK mask)
{
	if (mMediaSource)
	{
		S32 x, y;
		scaleTextureCoords(texture_coords, &x, &y);
		mouseMove(x, y, mask);
	}
}

void LLViewerMediaImpl::mouseDoubleClick(const LLVector2& texture_coords,
										 MASK mask)
{
	if (mMediaSource)
	{
		S32 x, y;
		scaleTextureCoords(texture_coords, &x, &y);
		mouseDoubleClick(x, y, mask);
	}
}

void LLViewerMediaImpl::mouseDoubleClick(S32 x, S32 y, MASK mask, S32 button)
{
	scaleMouse(&x, &y);
	mLastMouseX = x;
	mLastMouseY = y;
	if (mMediaSource)
	{
		if (gDebugClicks)
		{
			llinfos << "Sending event Mouse Double-click to media" << llendl;
		}
		mMediaSource->mouseEvent(LLPluginClassMedia::MOUSE_EVENT_DOUBLE_CLICK,
								 button, x, y, mask);
	}
}

void LLViewerMediaImpl::scrollWheel(S32 x, S32 y, S32 scroll_x, S32 scroll_y,
									MASK mask)
{
	scaleMouse(&x, &y);
	mLastMouseX = x;
	mLastMouseY = y;
	if (mMediaSource)
	{
		mMediaSource->scrollEvent(x, y, scroll_x, scroll_y, mask);
	}
}

void LLViewerMediaImpl::onMouseCaptureLost()
{
	if (mMediaSource)
	{
		if (gDebugClicks)
		{
			llinfos << "Sending event Mouse Up to media" << llendl;
		}
		mMediaSource->mouseEvent(LLPluginClassMedia::MOUSE_EVENT_UP, 0,
								 mLastMouseX, mLastMouseY, 0);
	}
}

bool LLViewerMediaImpl::handleMouseUp(S32 x, S32 y, MASK mask)
{
	// NOTE: this is called when the mouse is released when we have capture.
	// Due to the way mouse coordinates are mapped to the object, we cannot
	// use the x and y coordinates that come in with the event.

	if (hasMouseCapture())
	{
		if (gDebugClicks)
		{
			llinfos << "Media lost mouse capture" << llendl;
		}
		// Release the mouse -- this will also send a mouseup to the media
		gFocusMgr.setMouseCapture(NULL);
	}

	return true;
}

std::string LLViewerMediaImpl::getName() const
{
	if (mMediaSource)
	{
		std::string name = mMediaSource->getMediaName();
		const std::string& artist = mMediaSource->getArtist();
		if (!artist.empty())
		{
			if (!name.empty())
			{
				name += " - ";
			}
			name += "Artist: " + artist;
		}
		return name;
	}

	return LLStringUtil::null;
}

void LLViewerMediaImpl::navigateBack()
{
	if (mMediaSource)
	{
		mMediaSource->browse_back();
	}
}

void LLViewerMediaImpl::navigateForward()
{
	if (mMediaSource)
	{
		mMediaSource->browse_forward();
	}
}

void LLViewerMediaImpl::navigateReload()
{
	navigateTo(getCurrentMediaURL(), "", true);
}

void LLViewerMediaImpl::navigateHome()
{
	bool rediscover_mimetype = mHomeMimeType.empty();
	navigateTo(mHomeURL, mHomeMimeType, rediscover_mimetype);
}

void LLViewerMediaImpl::unload()
{
	// Unload the media impl and clear its state.
	destroyMediaSource();
	resetPreviousMediaState();
	mMediaURL.clear();
	mMimeType.clear();
	mCurrentMediaURL.clear();
	mCurrentMimeType.clear();
}

void LLViewerMediaImpl::navigateTo(const std::string& url,
								   const std::string& mime_type,
								   bool rediscover_type,
								   bool server_request,
								   bool filter_url)
{
	cancelMimeTypeProbe();

	if (mMediaURL != url)
	{
		// Do not carry media play state across distinct URLs.
		resetPreviousMediaState();
	}

	// Always set the current URL and MIME type.
	mMediaURL = url;
	mFilterURL = filter_url;
	mMimeType = mime_type;

	// Clear the current media URL, since it will no longer be correct.
	mCurrentMediaURL.clear();

	// if mime type discovery was requested, we'll need to do it when the media
	// loads
	mNavigateRediscoverType = rediscover_type;

	// and if this was a server request, the navigate on load will also need to
	// be one.
	mNavigateServerRequest = server_request;

	// An explicit navigate resets the "failed" flag.
	mMediaSourceFailed = false;

	if (mPriority == LLPluginClassMedia::PRIORITY_UNLOADED)
	{
		// Helpful to have media urls in log file. Should not be spammy.
		// Do not log the query parts
		LLURI u(url);
		std::string sanitized_url;
		if (u.query().empty())
		{
			sanitized_url = url;
		}
		else
		{
			sanitized_url = u.scheme() + "://" + u.authority() + u.path();
		}
		llinfos << "NOT LOADING media id = " << mTextureId << " - url = "
				<< sanitized_url << " - mime_type = " << mime_type << llendl;

		// This impl should not be loaded at this time.
		LL_DEBUGS("PluginPriority") << std::hex << (intptr_t)this << std::dec
									<< "Not loading (PRIORITY_UNLOADED)"
									<< LL_ENDL;
	}
	else
	{
		navigateInternal();
	}
}

void LLViewerMediaImpl::navigateInternal()
{
	// Helpful to have media urls in log file. Should not be spammy.
	// Do not log the query parts
	LLURI u(mMediaURL);
	std::string sanitized_url;
	if (u.query().empty())
	{
		sanitized_url = mMediaURL;
	}
	else
	{
		sanitized_url = u.scheme() + "://" + u.authority() + u.path();
	}
	llinfos << "media id = " << mTextureId << " - url = " << sanitized_url
			<< " - mime_type = " << mMimeType << llendl;

	if (mNavigateSuspended)
	{
		llwarns << "Deferring navigate." << llendl;
		mNavigateSuspendedDeferred = true;
		return;
	}

	if (!mMimeProbe.expired())
	{
		llwarns << "MIME type probe already in progress -- bailing out."
				<< llendl;
		return;
	}

	if (mFilterURL && gSavedSettings.getBool("MediaEnableFilter") &&
		// Do not filter login screens:
		LLStartUp::isLoggedIn() && LLViewerMedia::filterMedia(this))
	{
		// Filter triggered: abort for now, navigateInternal() will potentially
		// be called again (on callback, if a permission dialog was popped up).
		return;
	}

	if (mNavigateServerRequest)
	{
		setNavState(MEDIANAVSTATE_SERVER_SENT);
	}
	else
	{
		setNavState(MEDIANAVSTATE_NONE);
	}

	// If the caller has specified a non-empty MIME type, look that up in our
	// MIME types list. If we have a plugin for that MIME type, use that
	// instead of attempting auto-discovery. This helps in supporting legacy
	// media content where the server the media resides on returns a bogus MIME
	// type but the parcel owner has correctly set the MIME type in the parcel
	// media settings.

	if (!mMimeType.empty() && mMimeType != LLMIMETypes::getDefaultMimeType())
	{
		std::string plugin_basename = LLMIMETypes::implType(mMimeType);
		if (!plugin_basename.empty())
		{
			// We have a plugin for this mime type
			mNavigateRediscoverType = false;
		}
	}

	if (mNavigateRediscoverType)
	{
		LLURI uri(mMediaURL);
		std::string scheme = uri.scheme();
		if (scheme.empty() || "http" == scheme || "https" == scheme)
		{
			gCoros.launch("LLViewerMediaImpl::mimeDiscoveryCoro",
						  boost::bind(&LLViewerMediaImpl::mimeDiscoveryCoro,
						  this, mMediaURL));
		}
		else if ("data" == scheme || "file" == scheme || "about" == scheme)
		{
			// FIXME: figure out how to really discover the type for these
			// schemes.
			// We use "data" internally for a text/html url for loading the
			// login screen
			if (initializeMedia(HTTP_CONTENT_TEXT_HTML))
			{
				loadURI();
			}
		}
		else
		{
			// This catches 'rtsp://' urls
			if (initializeMedia(scheme))
			{
				loadURI();
			}
		}
	}
	else if (initializeMedia(mMimeType))
	{
		loadURI();
	}
	else
	{
		llwarns << "Could not navigate to '" << mMediaURL
				<< "' as there is no media type for: " << mMimeType << llendl;
	}
}

void LLViewerMediaImpl::mimeDiscoveryCoro(std::string url)
{
	// Increment our refcount so that we do not go away while the coroutine is
	// active.
	ref();

	LLCoreHttpUtil::HttpCoroutineAdapter::ptr_t
		adapter(new LLCoreHttpUtil::HttpCoroutineAdapter("mimeDiscoveryCoro"));
	mMimeProbe = adapter;

	LLCore::HttpOptions::ptr_t options(new LLCore::HttpOptions);
	// We only need the MIME type (e.g. text/html), and following redirects can
	// takes several seconds with some sites (e.g. the SL Marketplace).
	options->setFollowRedirects(false);
	options->setHeadersOnly(true);

	LLCore::HttpHeaders::ptr_t headers(new LLCore::HttpHeaders);
	headers->append(HTTP_OUT_HEADER_ACCEPT, "*/*");
	headers->append(HTTP_OUT_HEADER_COOKIE, "");

	LLSD result = adapter->getRawAndSuspend(url, options, headers);

	mMimeProbe.reset();

	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	if (!status)
	{
		llwarns << "Error retrieving media headers: " << status.toString()
				<< llendl;
	}

	// If there is only a single ref count outstanding it will be the one we
	// took out above andwe can skip the rest of this routine.
	if (getNumRefs() > 1)
	{
		const LLSD& httpres =
			result[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS];
		const LLSD& header =
			httpres[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS_HEADERS];

		std::string mime_type = header[HTTP_IN_HEADER_CONTENT_TYPE].asString();
		size_t i = mime_type.find_first_of(";");
		mime_type = mime_type.substr(0, i);
		// We now no longer check the error code returned from the probe. If we
		// have a mime type, use it, if not default to the web plugin and let
		// it handle error reporting.
		if (mime_type.empty())
		{
			LL_DEBUGS("Media") << "Mime type empty or missing from header"
							   << LL_ENDL;
			// Some sites do not return any content-type header at all. Treat
			// an empty mime type as text/html.
			mime_type = HTTP_CONTENT_TEXT_HTML;
		}

		LL_DEBUGS("Media") << "Status: " << status.getType()
						   << " - Mime type: " << mime_type << LL_ENDL;

		// Note: the call to initializeMedia may disconnect the responder,
		// which would clear mMediaImpl.
		if (!mime_type.empty())
		{
			if (initializeMedia(mime_type))
			{
				loadURI();
			}
		}
	}
	else
	{
		LL_DEBUGS("Media") << "LLViewerMediaImpl to be released." << LL_ENDL;
	}

	unref();
}

void LLViewerMediaImpl::navigateStop()
{
	if (mMediaSource)
	{
		mMediaSource->browse_stop();
	}
}

bool LLViewerMediaImpl::handleKeyHere(KEY key, MASK mask)
{
	bool result = false;

	if (mMediaSource)
	{
		// FIXME: THIS IS SO WRONG.
		// Menu keys should be handled by the menu system and not passed to UI
		// elements, but this is how LLTextEditor and LLLineEditor do it...
		if ((MASK_CONTROL & mask) && (key == 'C' || key == 'V' || key == 'X'))
		{
			result = true;
		}

		if (!result)
		{
			if (LLView::sDebugKeys)
			{
				llinfos << "Key handling passed to the media plugin" << llendl;
			}
			LLSD native_key_data = gWindowp->getNativeKeyData();
			result = mMediaSource->keyEvent(LLPluginClassMedia::KEY_EVENT_DOWN,
											key, mask, native_key_data);
		}
	}

	return result;
}

bool LLViewerMediaImpl::handleKeyUpHere(KEY key, MASK mask)
{
	bool result = false;

	if (mMediaSource)
	{
		// FIXME: THIS IS SO WRONG.
		// Menu keys should be handled by the menu system and not passed to UI
		// elements, but this is how LLTextEditor and LLLineEditor do it...
		if (MASK_CONTROL & mask)
		{
			if (key == 'C')
			{
				mMediaSource->copy();
				result = true;
			}
			else if (key == 'V')
			{
				mMediaSource->paste();
				result = true;
			}
			else if (key == 'X')
			{
				mMediaSource->cut();
				result = true;
			}
		}

		if (!result)
		{
			LLSD native_key_data = gWindowp->getNativeKeyData();
			result = mMediaSource->keyEvent(LLPluginClassMedia::KEY_EVENT_UP,
											key, mask, native_key_data);
		}
	}

	return result;
}

bool LLViewerMediaImpl::handleUnicodeCharHere(llwchar uni_char)
{
	if (mMediaSource && gKeyboardp)
	{
		// Only accept 'printable' characters, sigh...
		if (uni_char >= 32 &&	// discard 'control' characters
			uni_char != 127)	// SDL thinks this is 'delete'
		{
			if (LLView::sDebugKeys)
			{
				llinfos << "Key handling passed to the media plugin" << llendl;
			}
			LLSD native_key_data = gWindowp->getNativeKeyData();
			mMediaSource->textInput(wstring_to_utf8str(LLWString(1, uni_char)),
									gKeyboardp->currentMask(false),
									native_key_data);
		}
	}

	return false;
}

bool LLViewerMediaImpl::canNavigateForward()
{
	return mMediaSource && mMediaSource->getHistoryForwardAvailable();
}

bool LLViewerMediaImpl::canNavigateBack()
{
	return mMediaSource && mMediaSource->getHistoryBackAvailable();
}

void LLViewerMediaImpl::update()
{
	LL_FAST_TIMER(FTM_MEDIA_DO_UPDATE);

	if (!mMediaSource &&
		// do not load sources doing a MIME type probe.
		mMimeProbe.expired() &&
		// or sources media source should not be loaded.
		mPriority != LLPluginClassMedia::PRIORITY_UNLOADED &&
		// do not load new instances that are at PRIORITY_SLIDESHOW or below,
		// they are just kept around to preserve state.
		mPriority > LLPluginClassMedia::PRIORITY_SLIDESHOW)
	{
		// This media may need to be loaded.
		if (sMediaCreateTimer.hasExpired())
		{
			LL_DEBUGS("PluginPriority") << this
										<< ": creating media based on timer expiration"
										<< LL_ENDL;
			createMediaSource();
			sMediaCreateTimer.setTimerExpirySec(LLVIEWERMEDIA_CREATE_DELAY);
		}
		else
		{
			LL_DEBUGS("PluginPriority") << this
										<< ": NOT creating media (waiting on timer)"
										<< LL_ENDL;
		}
	}
	else
	{
		updateVolume();

	}

	if (!mMediaSource)
	{
		return;
	}

	// Make sure a navigate does not happen during the idle: it can cause
	// mMediaSource to get destroyed, which can cause a crash.
	setNavigateSuspended(true);

	mMediaSource->idle();

	setNavigateSuspended(false);

	if (!mMediaSource)
	{
		return;
	}

	if (mMediaSource->isPluginExited())
	{
		resetPreviousMediaState();
		destroyMediaSource();
		return;
	}

	if (!mMediaSource->textureValid() || mSuspendUpdates || !mVisible)
	{
		return;
	}

	LLViewerMediaTexture* media_tex;
	U8* data;
	S32 data_width, data_height, x_pos, y_pos, width, height;
	if (!preMediaTexUpdate(media_tex, data, data_width, data_height, x_pos,
						   y_pos, width, height))
	{
		return;
	}

	static LLCachedControl<bool> use_worker(gSavedSettings,
											"GLWorkerUseForMedia");
	bool can_queue = use_worker && LLImageGLThread::sEnabled && gMainloopWorkp;
	if (can_queue)
	{
		mTextureUpdatePending = true;
		// Protect textures from deletion while active on bg queue
		ref();
		media_tex->ref();
		// Push update to the worker thread
		if (gMainloopWorkp->postTo(gImageQueuep,
								   [=]()	// Work done on worker thread
								   {
										doMediaTexUpdate(media_tex, data,
														 data_width,
														 data_height,
														 x_pos, y_pos,
														 width, height, true);
								   },
								   [=]()	// Callback to main thread
								   {
										mTextureUpdatePending = false;
										media_tex->unref();
										unref();
								   }))
		{
			return;	// Success
		}
		// Failed (gImageQueuep closed): fallback to update on main thread
		mTextureUpdatePending = false;
		media_tex->unref();
		unref();
	}

	{
		LL_FAST_TIMER(FTM_MEDIA_SET_SUBIMAGE);
		// Update on the main thread
		doMediaTexUpdate(media_tex, data, data_width, data_height, x_pos,
						 y_pos, width, height, false);
	}
}

bool LLViewerMediaImpl::preMediaTexUpdate(LLViewerMediaTexture*& media_tex,
										  U8*& data, S32& data_width,
										  S32& data_height, S32& x_pos,
										  S32& y_pos, S32& width, S32& height)
{
	LL_TRACY_TIMER(TRC_MEDIA_PRE_UPDATE);

	if (mTextureUpdatePending)
	{
		return false;
	}

	bool success = false;
	media_tex = updateMediaImage();
	if (media_tex && mMediaSource)
	{
		LLRect dirty_rect;
		S32 media_width = mMediaSource->getTextureWidth();
		S32 media_height = mMediaSource->getTextureHeight();

		// Since we are updating this texture, we know it is playing. Tell the
		// texture to do its replacement magic so it gets rendered.
		media_tex->setPlaying(true);

		if (mMediaSource->getDirty(&dirty_rect))
		{
			// Constrain the dirty rect to be inside the texture
			x_pos = llmax(dirty_rect.mLeft, 0);
			y_pos = llmax(dirty_rect.mBottom, 0);
			width = llmin(dirty_rect.mRight, media_width) - x_pos;
			height = llmin(dirty_rect.mTop, media_height) - y_pos;

			if (width > 0 && height > 0)
			{
				LL_FAST_TIMER(FTM_MEDIA_GET_DATA);
				data = mMediaSource->getBitsData();
				data_width = mMediaSource->getWidth();
				data_height = mMediaSource->getHeight();
				// This will be true when data is ready to be copied to GL
				success = data != NULL;
			}
		}
		mMediaSource->resetDirty();
	}
	return success;
}

void LLViewerMediaImpl::doMediaTexUpdate(LLViewerMediaTexture* media_tex,
										 U8* data, S32 data_width,
										 S32 data_height, S32 x_pos, S32 y_pos,
										 S32 width, S32 height, bool sync)
{
	LL_TRACY_TIMER(TRC_MEDIA_TEX_UPDATE);

	// Prevents media source tear-down during update
	mLock.lock();

	static LLCachedControl<bool> recreate(gSavedSettings,
										  "RecreateMediaGLTexOnUpdate");
	bool do_recreate = recreate;
	U32 tex_name = media_tex->getTexName();
	if (!tex_name)
	{
		do_recreate = true;
	}

	// Wrap 'data' in an LLImageRaw but do NOT make a copy.
	LLPointer<LLImageRaw> raw = new LLImageRaw(data, media_tex->getWidth(),
											   media_tex->getHeight(),
											   media_tex->getComponents(),
											   true);
	// Recreating the GL texture at each media update is wasteful but might be
	// needed when GL calls are blocking in some poor OpenGL implementations.
	if (do_recreate)
	{
		media_tex->createGLTexture(0, raw, 0, true, true, &tex_name);
	}

	// Copy just the subimage covered by the image raw to GL
	media_tex->setSubImage(data, data_width, data_height, x_pos, y_pos, width,
						   height, tex_name);
	if (sync)
	{
		media_tex->getGLImage()->syncToMainThread(tex_name);
	}
	else
	{
		media_tex->getGLImage()->syncTexName(tex_name);
	}

	// Release the data pointer before freeing raw so LLImageRaw destructor
	// does not free memory at data pointer.
	raw->releaseData();

	mLock.unlock();
}

LLViewerMediaTexture* LLViewerMediaImpl::updateMediaImage()
{
	if (mTextureId.isNull())
	{
		// The code that created this instance will read from the plugin's bits
		return NULL;
	}

	if (!mMediaSource)
	{
		// Not ready for updating
		return NULL;
	}

	LLViewerMediaTexture* media_tex =
		LLViewerTextureManager::getMediaTexture(mTextureId);
	if (!media_tex)
	{
		llwarns << "Could not find media texture " << mTextureId << llendl;
		return NULL;
	}

	if (mNeedsNewTexture ||
		media_tex->getWidth() != mMediaSource->getTextureWidth() ||
		media_tex->getHeight() != mMediaSource->getTextureHeight() ||
		mTextureUsedWidth != mMediaSource->getWidth() ||
		mTextureUsedHeight != mMediaSource->getHeight())
	{
		llinfos << "Initializing media placeholder with  movie image id: "
				<< mTextureId << llendl;

		U16 texture_width = mMediaSource->getTextureWidth();
		U16 texture_height = mMediaSource->getTextureHeight();
		S8 texture_depth = mMediaSource->getTextureDepth();

		// MEDIAOPT: check to see if size actually changed before doing work
		media_tex->destroyGLTexture();

		// MEDIAOPT: seems insane that we actually have to make an imageraw
		// then immediately discard it
		LLPointer<LLImageRaw> raw = new LLImageRaw(texture_width,
												   texture_height,
												   texture_depth);
		raw->clear(U8(mBackgroundColor.mV[VX] * 255.f),
				   U8(mBackgroundColor.mV[VY] * 255.f),
				   U8(mBackgroundColor.mV[VZ] * 255.f), 255);

		// Ask media source for correct GL image format constants
		media_tex->setExplicitFormat(mMediaSource->getTextureFormatInternal(),
									 mMediaSource->getTextureFormatPrimary(),
									 mMediaSource->getTextureFormatType(),
									 mMediaSource->getTextureFormatSwapBytes());

		media_tex->createGLTexture(0, raw);	// 0 discard

		mNeedsNewTexture = false;

		// If the amount of the texture being drawn by the media goes down in
		// either width or height, recreate the texture to avoid leaving parts
		// of the old image behind.
		mTextureUsedWidth = mMediaSource->getWidth();
		mTextureUsedHeight = mMediaSource->getHeight();
	}

	return media_tex;
}

void LLViewerMediaImpl::setVisible(bool visible)
{
	mVisible = visible;
	if (visible)
	{
		if (mMediaSource && mMediaSource->isPluginExited())
		{
			destroyMediaSource();
		}

		if (!mMediaSource)
		{
			createMediaSource();
		}
	}
}

void LLViewerMediaImpl::mouseCapture()
{
	if (gDebugClicks)
	{
		llinfos << "Media gained mouse capture" << llendl;
	}
	gFocusMgr.setMouseCapture(this);
}

void LLViewerMediaImpl::scaleMouse(S32 *mouse_x, S32 *mouse_y)
{
#if 0
	S32 media_width, media_height;
	S32 texture_width, texture_height;
	getMediaSize(&media_width, &media_height);
	getTextureSize(&texture_width, &texture_height);
	S32 y_delta = texture_height - media_height;

	*mouse_y -= y_delta;
#endif
}

bool LLViewerMediaImpl::isMediaTimeBased()
{
	return mMediaSource && mMediaSource->pluginSupportsMediaTime();
}

bool LLViewerMediaImpl::isMediaPlaying()
{
	bool result = false;

	if (mMediaSource)
	{
		EMediaStatus status = mMediaSource->getStatus();
		if (status == MEDIA_PLAYING || status == MEDIA_LOADING)
		{
			result = true;
		}
	}

	return result;
}

bool LLViewerMediaImpl::isMediaPaused()
{
	return mMediaSource && mMediaSource->getStatus() == MEDIA_PAUSED;
}

bool LLViewerMediaImpl::hasMedia()
{
	return mMediaSource != NULL;
}

void LLViewerMediaImpl::resetPreviousMediaState()
{
	mPreviousMediaState = MEDIA_NONE;
	mPreviousMediaTime = 0.f;
}

void LLViewerMediaImpl::setDisabled(bool disabled, bool forcePlayOnEnable)
{
	if (mIsDisabled != disabled)
	{
		// Only do this on actual state transitions.
		mIsDisabled = disabled;

		if (mIsDisabled)
		{
			// We just disabled this media.  Clear all state.
			unload();
		}
		else
		{
			// We just (re)enabled this media. Do a navigate if auto-play is in
			// order.
			if (isAutoPlayable() || forcePlayOnEnable)
			{
				navigateTo(mMediaEntryURL, "", true, true);
			}
		}
	}
}

bool LLViewerMediaImpl::isForcedUnloaded() const
{
	if (mIsMuted || mMediaSourceFailed || mIsDisabled ||
		// If this media's class is not supposed to be shown, unload
		!shouldShowBasedOnClass())
	{
		return true;
	}

	return false;
}

bool LLViewerMediaImpl::isPlayable() const
{
	if (isForcedUnloaded())
	{
		// All of the forced-unloaded criteria also imply not playable.
		return false;
	}

	if (((LLViewerMediaImpl*)this)->hasMedia())
	{
		// Anything that is already playing is, by definition, playable.
		return true;
	}

	if (!mMediaURL.empty())
	{
		// If something has navigated the instance, it's ready to be played.
		return true;
	}

	return false;
}

void select_file_callback(HBFileSelector::ELoadFilter type,
						  std::string& filename, void* user_data)
{
	LLPluginClassMedia* plugin = (LLPluginClassMedia*)user_data;
	if (plugin)	// *TODO: Add a check about the plugin's existence...
	{
		plugin->sendPickFileResponse(filename);
	}
}

void select_files_callback(HBFileSelector::ELoadFilter type,
						   std::deque<std::string>& files, void* user_data)
{
	LLPluginClassMedia* plugin = (LLPluginClassMedia*)user_data;
	if (plugin)	// *TODO: Add a check about the plugin's existence...
	{
		std::vector<std::string> file_list;
		while (!files.empty())
		{
			file_list.push_back(files.front());
			files.pop_front();
		}
		plugin->sendPickFileResponse(file_list);
	}
}

void LLViewerMediaImpl::handleMediaEvent(LLPluginClassMedia* plugin,
										 LLPluginClassMediaOwner::EMediaEvent event)
{
	if (!plugin) return;

	bool pass_through = true;
	switch (event)
	{
		case MEDIA_EVENT_CLICK_LINK_NOFOLLOW:
		{
			std::string url = plugin->getClickURL();
			LL_DEBUGS("Media") << "MEDIA_EVENT_CLICK_LINK_NOFOLLOW, uri: "
							   << url << LL_ENDL;
			std::string nav_type = plugin->getClickNavType();
			LLURLDispatcher::dispatch(url, nav_type, NULL, mTrustedBrowser);
			break;
		}

		case MEDIA_EVENT_CLICK_LINK_HREF:
		{
			// retrieve the event parameters
			std::string url = plugin->getClickURL();
			std::string target =
				plugin->isOverrideClickTarget() ? plugin->getOverrideClickTarget()
												: plugin->getClickTarget();
#if 0		// Not used
			std::string uuid = plugin->getClickUUID();
#endif
			// loadURL now handles distinguishing between _blank, _external,
			// and other named targets.
			LL_DEBUGS("Media") << "MEDIA_EVENT_CLICK_LINK_HREF, target: "
							   << target << " - uri: " << url << LL_ENDL;
			LLWeb::loadURL(url, target);
			break;
		}

		case MEDIA_EVENT_PLUGIN_FAILED_LAUNCH:
		{
			// The plugin failed to load properly. Make sure the timer does not
			// retry. *TODO: maybe mark this plugin as not loadable somehow ?
			mMediaSourceFailed = true;

			// Reset the last known state of the media to defaults.
			resetPreviousMediaState();

			// *TODO: may want a different message for this case ?
			LLSD args;
			args["PLUGIN"] = LLMIMETypes::implType(mCurrentMimeType);
			gNotifications.add("MediaPluginFailed", args);
			break;
		}

		case MEDIA_EVENT_PLUGIN_FAILED:
		{
			// The plugin crashed.
			mMediaSourceFailed = true;

			// Reset the last known state of the media to defaults.
			resetPreviousMediaState();

#if 0		// SJB: This is getting called every frame if the plugin fails to
			// load, continuously respawining the alert !
			LLSD args;
			args["PLUGIN"] = LLMIMETypes::implType(mMimeType);
			gNotifications.add("MediaPluginFailed", args);
#endif
			break;
		}

		case MEDIA_EVENT_CURSOR_CHANGED:
		{
			std::string cursor = plugin->getCursorName();
			LL_DEBUGS("Media") << "MEDIA_EVENT_CURSOR_CHANGED, new cursor is: "
							   << cursor << LL_ENDL;

			if (cursor == "ibeam")
			{
				mLastSetCursor = UI_CURSOR_IBEAM;
			}
			else if (cursor == "splith")
			{
				mLastSetCursor = UI_CURSOR_SIZEWE;
			}
			else if (cursor == "splitv")
			{
				mLastSetCursor = UI_CURSOR_SIZENS;
			}
			else if (cursor == "hand")
			{
				mLastSetCursor = UI_CURSOR_HAND;
			}
			else
			{
				// For anything else, default to the arrow
				mLastSetCursor = UI_CURSOR_ARROW;
			}
			break;
		}

		case LLViewerMediaObserver::MEDIA_EVENT_FILE_DOWNLOAD:
		{
			// *TODO: allow downloading by sending the file URL to the system
			// browser
			LL_DEBUGS("Media") << "MEDIA_EVENT_FILE_DOWNLOAD, filename is: "
							   << plugin->getFileDownloadFilename() << LL_ENDL;
			gNotifications.add("MediaFileDownloadUnsupported");
			pass_through = false; // Do not chain this event !
			break;
		}

		case LLViewerMediaObserver::MEDIA_EVENT_NAVIGATE_BEGIN:
		{
			LL_DEBUGS("Media") << "MEDIA_EVENT_NAVIGATE_BEGIN, uri is: "
							   << plugin->getNavigateURI() << LL_ENDL;

			if (getNavState() == MEDIANAVSTATE_SERVER_SENT)
			{
				setNavState(MEDIANAVSTATE_SERVER_BEGUN);
			}
			else
			{
				setNavState(MEDIANAVSTATE_BEGUN);
			}
			break;
		}

		case LLViewerMediaObserver::MEDIA_EVENT_NAVIGATE_COMPLETE:
		{
			LL_DEBUGS("Media") << "MEDIA_EVENT_NAVIGATE_COMPLETE, uri is: "
							   << plugin->getNavigateURI() << LL_ENDL;

			std::string url = plugin->getNavigateURI();
			if (getNavState() == MEDIANAVSTATE_BEGUN)
			{
				if (mCurrentMediaURL == url)
				{
					// This is a navigate that takes us to the same url as the
					// previous navigate.
					setNavState(MEDIANAVSTATE_COMPLETE_BEFORE_LOCATION_CHANGED_SPURIOUS);
				}
				else
				{
					mCurrentMediaURL = url;
					setNavState(MEDIANAVSTATE_COMPLETE_BEFORE_LOCATION_CHANGED);
				}
			}
			else if (getNavState() == MEDIANAVSTATE_SERVER_BEGUN)
			{
				mCurrentMediaURL = url;
				setNavState(MEDIANAVSTATE_SERVER_COMPLETE_BEFORE_LOCATION_CHANGED);
			}
			// all other cases need to leave the state alone.
			break;
		}

		case LLViewerMediaObserver::MEDIA_EVENT_LOCATION_CHANGED:
		{
			LL_DEBUGS("Media") << "MEDIA_EVENT_LOCATION_CHANGED, uri is: "
							   << plugin->getLocation() << LL_ENDL;

			std::string url = plugin->getLocation();

			if (getNavState() == MEDIANAVSTATE_BEGUN)
			{
				if (mCurrentMediaURL == url)
				{
					// This is a navigate that takes us to the same url as the
					// previous navigate.
					setNavState(MEDIANAVSTATE_FIRST_LOCATION_CHANGED_SPURIOUS);
				}
				else
				{
					mCurrentMediaURL = url;
					setNavState(MEDIANAVSTATE_FIRST_LOCATION_CHANGED);
				}
			}
			else if (getNavState() == MEDIANAVSTATE_SERVER_BEGUN)
			{
				mCurrentMediaURL = url;
				setNavState(MEDIANAVSTATE_SERVER_FIRST_LOCATION_CHANGED);
			}
			else
			{
				bool internal_nav = false;
				if (url != mCurrentMediaURL)
				{
					// Check if it is internal navigation. Note: not sure if we
					// should detect internal navigations as 'address change',
					// but they are not redirects and do not cause
					// NAVIGATE_BEGIN (also see SL-1005).
					size_t pos = url.find("#");
					if (pos != std::string::npos)
					{
						// Assume that new link always have '#', so this is
						// either transfer from 'link#1' to 'link#2' or from
						// link to 'link#2'; filter out cases like
						// 'redirect?link'
						std::string base_url = url.substr(0, pos);
						if (mCurrentMediaURL.find(base_url) == 0)
						{
							// Base link did not change
							internal_nav = true;
						}
					}
				}
				if (internal_nav)
				{
					// Internal navigation by '#'
					mCurrentMediaURL = url;
					setNavState(MEDIANAVSTATE_FIRST_LOCATION_CHANGED);
				}
				else
				{
					// Do not track redirects.
					setNavState(MEDIANAVSTATE_NONE);
				}
			}
			break;
		}

		case LLViewerMediaObserver::MEDIA_EVENT_PICK_FILE_REQUEST:
		{
			// Display a file(s) selector
			if (plugin->getIsMultipleFilePick())
			{
				HBFileSelector::loadFiles(HBFileSelector::FFLOAD_ALL,
										  select_files_callback, plugin);
			}
			else
			{
				HBFileSelector::loadFile(HBFileSelector::FFLOAD_ALL,
										 select_file_callback, plugin);
			}
			break;
		}

		case LLViewerMediaObserver::MEDIA_EVENT_AUTH_REQUEST:
		{
			std::string host = plugin->getAuthURL();
			size_t i = host.find("://");
			if (i != std::string::npos)
			{
				host = host.substr(i + 3);
			}
			i = host.find("/");
			if (i != std::string::npos)
			{
				host = host.substr(0, i);
			}
			const std::string& realm = plugin->getAuthRealm();
			llinfos << "Spawning authentication request dialog for host: "
					<< host << " - Realm: " << realm << " - Media Id: "
					<< mTextureId << llendl;
			HBFloaterUserAuth::request(host, realm, mTextureId,
									   LLViewerMedia::onAuthSubmit);
			pass_through = false; // Do not chain this event !
			break;
		}

		case LLViewerMediaObserver::MEDIA_EVENT_CLOSE_REQUEST:
		{
			std::string uuid = plugin->getClickUUID();

			llinfos << "MEDIA_EVENT_CLOSE_REQUEST for uuid " << uuid << llendl;

			if (!uuid.empty())
			{
				// This close request is directed at another instance
				pass_through = false;
				// *TODO: LLFloaterMediaBrowser::closeRequest(uuid);
			}
			break;
		}

		case LLViewerMediaObserver::MEDIA_EVENT_GEOMETRY_CHANGE:
		{
			std::string uuid = plugin->getClickUUID();

			llinfos << "MEDIA_EVENT_GEOMETRY_CHANGE for uuid " << uuid << llendl;

			if (uuid.empty())
			{
				// This geometry change request is directed at this instance,
				// let it fall through.
			}
			else
			{
				// This request is directed at another instance
				pass_through = false;
#if 0			// *TODO:
				LLFloaterMediaBrowser::geometryChanged(uuid,
													   plugin->getGeometryX(),
													   plugin->getGeometryY(),
													   plugin->getGeometryWidth(),
													   plugin->getGeometryHeight());
#endif
			}
			break;
		}

		default:
		{
		}
	}

	if (pass_through)
	{
		// Just chain the event to observers.
		emitEvent(plugin, event);
	}
}

//virtual
void LLViewerMediaImpl::cut()
{
	if (mMediaSource)
	{
		mMediaSource->cut();
	}
}

//virtual
bool LLViewerMediaImpl::canCut() const
{
	return mMediaSource && mMediaSource->canCut();
}

//virtual
void LLViewerMediaImpl::copy()
{
	if (mMediaSource)
	{
		mMediaSource->copy();
	}
}

//virtual
bool LLViewerMediaImpl::canCopy() const
{
	return mMediaSource && mMediaSource->canCopy();
}

//virtual
void LLViewerMediaImpl::paste()
{
	if (mMediaSource)
	{
		mMediaSource->paste();
	}
}

//virtual
bool LLViewerMediaImpl::canPaste() const
{
	return mMediaSource && mMediaSource->canPaste();
}

void LLViewerMediaImpl::setBackgroundColor(LLColor4 color)
{
	mBackgroundColor = color;

	if (mMediaSource)
	{
		mMediaSource->setBackgroundColor(mBackgroundColor);
	}
}

void LLViewerMediaImpl::setNavState(EMediaNavState state)
{
	mMediaNavState = state;

	LL_DEBUGS("Media") << "Setting nav state to: ";
	std::string state_str;
	switch (state)
	{
		case MEDIANAVSTATE_NONE:
			state_str = "MEDIANAVSTATE_NONE";
			break;

		case MEDIANAVSTATE_BEGUN:
			state_str = "MEDIANAVSTATE_BEGUN";
			break;

		case MEDIANAVSTATE_FIRST_LOCATION_CHANGED:
			state_str = "MEDIANAVSTATE_FIRST_LOCATION_CHANGED";
			break;

		case MEDIANAVSTATE_FIRST_LOCATION_CHANGED_SPURIOUS:
			state_str = "MEDIANAVSTATE_FIRST_LOCATION_CHANGED_SPURIOUS";
			break;

		case MEDIANAVSTATE_COMPLETE_BEFORE_LOCATION_CHANGED:
			state_str = "MEDIANAVSTATE_COMPLETE_BEFORE_LOCATION_CHANGED";
			break;

		case MEDIANAVSTATE_COMPLETE_BEFORE_LOCATION_CHANGED_SPURIOUS:
			state_str = "MEDIANAVSTATE_COMPLETE_BEFORE_LOCATION_CHANGED_SPURIOUS";
			break;

		case MEDIANAVSTATE_SERVER_SENT:
			state_str = "MEDIANAVSTATE_SERVER_SENT";
			break;

		case MEDIANAVSTATE_SERVER_BEGUN:
			state_str = "MEDIANAVSTATE_SERVER_BEGUN";
			break;

		case MEDIANAVSTATE_SERVER_FIRST_LOCATION_CHANGED:
			state_str = "MEDIANAVSTATE_SERVER_FIRST_LOCATION_CHANGED";
			break;

		case MEDIANAVSTATE_SERVER_COMPLETE_BEFORE_LOCATION_CHANGED:
			state_str = "MEDIANAVSTATE_SERVER_COMPLETE_BEFORE_LOCATION_CHANGED";
	}
	LL_CONT << state_str << LL_ENDL;
}

void LLViewerMediaImpl::calculateInterest()
{
	LL_FAST_TIMER(FTM_MEDIA_CALCULATE_INTEREST);
	LLViewerMediaTexture* texture =
		LLViewerTextureManager::findMediaTexture(mTextureId);

	if (texture)
	{
		mInterest = texture->getMaxVirtualSize();
	}
	else
	{
		// This will be a relatively common case now, since it will always be
		// true for unloaded media.
		mInterest = 0.f;
	}

	// Calculate distance from the avatar, for use in the proximity
	// calculation.
	mProximityDistance = 0.f;
	mProximityCamera = 0.f;
	if (!mObjectList.empty())
	{
		// Just use the first object in the list. We could go through the list
		// and find the closest object, but this should work well enough.
		std::list< LLVOVolume* >::iterator iter = mObjectList.begin();
		LLVOVolume* objp = *iter;
		llassert_always(objp != NULL);

		// The distance calculation is invalid for HUD attachments -- leave
		// both mProximityDistance and mProximityCamera at 0 for them.
		if (!objp->isHUDAttachment())
		{
			LLVector3d obj_global = objp->getPositionGlobal();
			LLVector3d agent_global = gAgent.getPositionGlobal();
			LLVector3d global_delta = agent_global - obj_global;
			// use distance-squared because it's cheaper and sorts the same:
			mProximityDistance = global_delta.lengthSquared();

			LLVector3d camera_delta = gAgent.getCameraPositionGlobal() -
									  obj_global;
			mProximityCamera = camera_delta.length();
		}
	}

	if (mNeedsMuteCheck)
	{
		// Check all objects this instance is associated with, and those
		// objects' owners, against the mute list
		mIsMuted = false;
		for (std::list<LLVOVolume*>::iterator iter = mObjectList.begin(),
											  end = mObjectList.end();
			 iter != end; ++iter)
		{
			LLVOVolume* obj = *iter;
			if (!obj) continue;

			if (LLMuteList::isMuted(obj->getID()))
			{
				mIsMuted = true;
			}
			else
			{
				// We may not have full permissions data for all objects.
				// Attempt to mute objects when we can tell their owners are
				// muted.
				LLPermissions* obj_perm =
					gSelectMgr.findObjectPermissions(obj);
				if (obj_perm && LLMuteList::isMuted(obj_perm->getOwner()))
				{
					mIsMuted = true;
				}
			}
		}

		mNeedsMuteCheck = false;
	}
}

F64 LLViewerMediaImpl::getApproximateTextureInterest()
{
	F64 result = 0.0;

	if (mMediaSource)
	{
		result = mMediaSource->getFullWidth();
		result *= mMediaSource->getFullHeight();
	}
	else
	{
		// No media source is loaded -- all we have to go on is the texture
		// size that has been set on the impl, if any.
		result = mMediaWidth;
		result *= mMediaHeight;
	}

	return result;
}

void LLViewerMediaImpl::setUsedInUI(bool used_in_ui)
{
	mUsedInUI = used_in_ui;

	// *HACK: Force elements used in UI to load right away. This fixes some
	// issues where UI code that uses the browser instance does not expect it
	// to be unloaded.
	if (mUsedInUI && mPriority == LLPluginClassMedia::PRIORITY_UNLOADED)
	{
		if (getVisible())
		{
			setPriority(LLPluginClassMedia::PRIORITY_NORMAL);
		}
		else
		{
			setPriority(LLPluginClassMedia::PRIORITY_HIDDEN);
		}

		createMediaSource();
	}
}

F64 LLViewerMediaImpl::getCPUUsage() const
{
	return mMediaSource ? mMediaSource->getCPUUsage() : 0.0;
}

void LLViewerMediaImpl::setPriority(LLPluginClassMedia::EPriority priority)
{
	if (mPriority != priority)
	{
		LL_DEBUGS("PluginPriority") << "changing priority of media id "
									<< mTextureId << " from "
									<< LLPluginClassMedia::priorityToString(mPriority)
									<< " to "
									<< LLPluginClassMedia::priorityToString(priority)
									<< LL_ENDL;
		mPriority = priority;
	}

	if (priority == LLPluginClassMedia::PRIORITY_UNLOADED)
	{
		if (mMediaSource)
		{
			// Need to unload the media source

			// First, save off previous media state
			mPreviousMediaState = mMediaSource->getStatus();
			mPreviousMediaTime = mMediaSource->getCurrentTime();

			destroyMediaSource();
		}
	}

	if (mMediaSource)
	{
		mMediaSource->setPriority(mPriority);
	}

	// NOTE: loading (or reloading) media sources whose priority has risen
	// above PRIORITY_UNLOADED is done in update().
}

void LLViewerMediaImpl::setLowPrioritySizeLimit(int size)
{
	if (mMediaSource)
	{
		mMediaSource->setLowPrioritySizeLimit(size);
	}
}

void LLViewerMediaImpl::setNavigateSuspended(bool suspend)
{
	if (mNavigateSuspended != suspend)
	{
		mNavigateSuspended = suspend;
		if (!suspend)
		{
			// We're coming out of suspend. If someone tried to do a navigate
			// while suspended, do one now instead.
			if (mNavigateSuspendedDeferred)
			{
				mNavigateSuspendedDeferred = false;
				navigateInternal();
			}
		}
	}
}

void LLViewerMediaImpl::cancelMimeTypeProbe()
{
	LLCoreHttpUtil::HttpCoroutineAdapter::ptr_t adapter = mMimeProbe.lock();
	if (adapter)
	{
		adapter->cancelSuspendedOperation();
	}
}

void LLViewerMediaImpl::addObject(LLVOVolume* obj)
{
	for (std::list<LLVOVolume*>::iterator iter = mObjectList.begin(),
										  end = mObjectList.end();
		 iter != end; ++iter)
	{
		if (*iter == obj)
		{
			return; // already in the list.
		}
	}

	mObjectList.push_back(obj);
	if (obj->isHUDAttachment())
	{
		mUsedOnHUD = true;
	}
	mNeedsMuteCheck = true;
}

void LLViewerMediaImpl::removeObject(LLVOVolume* obj)
{
	mObjectList.remove(obj);
	mNeedsMuteCheck = true;
	if (mUsedOnHUD && !mObjectList.empty())
	{
		// Check to see if any remaining object using this impl is a HUD
		mUsedOnHUD = false;
		for (std::list<LLVOVolume*>::iterator iter = mObjectList.begin(),
											  end = mObjectList.end();
			 iter != end; ++iter)
		{
			LLVOVolume* obj = *iter;
			if (obj && obj->isHUDAttachment())
			{
				mUsedOnHUD = true;
				return;	// No need to continue
			}
		}
	}
}

const std::list<LLVOVolume*>* LLViewerMediaImpl::getObjectList() const
{
	return &mObjectList;
}

LLVOVolume* LLViewerMediaImpl::getSomeObject()
{
	LLVOVolume* result = NULL;

	std::list<LLVOVolume*>::iterator iter = mObjectList.begin();
	if (iter != mObjectList.end())
	{
		result = *iter;
	}

	return result;
}

void LLViewerMediaImpl::setTextureID(LLUUID id)
{
	if (id != mTextureId)
	{
		if (mTextureId.notNull())
		{
			// Remove this item's entry from the map
			sViewerMediaTextureIDMap.erase(mTextureId);
		}

		if (id.notNull())
		{
			sViewerMediaTextureIDMap[id] = this;
		}

		mTextureId = id;
	}
}

bool LLViewerMediaImpl::isAutoPlayable() const
{
	static LLCachedControl<bool> parcel_media_auto_play(gSavedSettings,
														"ParcelMediaAutoPlayEnable");
	static LLCachedControl<bool> tentative_auto_play(gSavedSettings,
													 "MediaTentativeAutoPlay");
	return mMediaAutoPlay && tentative_auto_play &&
		   (parcel_media_auto_play || !isParcelMedia());
}

bool LLViewerMediaImpl::shouldShowBasedOnClass() const
{
	static LLCachedControl<bool> show_media_on_others(gSavedSettings,
													  "MediaShowOnOthers");
	static LLCachedControl<bool> show_media_within_parcel(gSavedSettings,
														  "MediaShowWithinParcel");
	static LLCachedControl<bool> show_media_outside_parcel(gSavedSettings,
														   "MediaShowOutsideParcel");

	// If this is parcel media, or in the UI, or on a HUD, return true always
	if (getUsedInUI() || getUsedOnHUD() || isParcelMedia()) return true;

#if 0	// This is incorrect, and causes EXT-6750 (disabled attachment media
		// still plays)
	// If it has focus, we should show it
	if (hasFocus()) return true;
#endif

	if (isAttachedToAnotherAvatar())
	{
		return show_media_on_others;
	}
	if (isInAgentParcel())
	{
		return show_media_within_parcel;
	}
	else
	{
		return show_media_outside_parcel;
	}
}

bool LLViewerMediaImpl::isAttachedToAnotherAvatar() const
{
	for (std::list<LLVOVolume*>::const_iterator iter = mObjectList.begin(),
												end = mObjectList.end();
		 iter != end; ++iter)
	{
		LLVOVolume* obj = *iter;
		if (obj)
		{
			LLVOAvatar* avatar = obj->getAvatarAncestor();
			if (avatar && !avatar->isSelf())
			{
				return true;
			}
		}
	}
	return false;
}

bool LLViewerMediaImpl::isInAgentParcel() const
{
	for (std::list<LLVOVolume*>::const_iterator iter = mObjectList.begin(),
												end = mObjectList.end();
		 iter != end; ++iter)
	{
		LLVOVolume* obj = *iter;
		if (obj && gViewerParcelMgr.inAgentParcel(obj->getPositionGlobal()))
		{
			return true;
		}
	}
	return false;
}
