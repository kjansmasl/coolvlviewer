/**
 * @file llgesturemgr.cpp
 * @brief Manager for playing gestures on the viewer
 *
 * $LicenseInfo:firstyear=2004&license=viewergpl$
 *
 * Copyright (c) 2004-2009, Linden Research, Inc.
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

#include "llviewerprecompiledheaders.h"

#include "boost/tokenizer.hpp"

#include "llgesturemgr.h"

#include "llcallbacklist.h"
#include "lldatapacker.h"
#include "llfilesystem.h"
#include "llfloatergesture.h"
#include "llinventory.h"
#include "llmultigesture.h"
#include "llnotifications.h"

#include "llagent.h"
#include "llchatbar.h"
#include "llinventorymodel.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llviewermessage.h"	// send_sound_trigger()
#include "llviewerstats.h"
#include "llvoavatarself.h"

LLGestureManager gGestureManager;

// Longest time, in seconds, to wait for all animations to stop playing
constexpr F32 MAX_WAIT_ANIM_SECS = 30.f;

// Delay before notifying a failure to load a gesture
constexpr F32 MAX_NAME_WAIT_TIME = 5.0f;

///////////////////////////////////////////////////////////////////////////////
// LLGestureInventoryFetchObserver helper class
///////////////////////////////////////////////////////////////////////////////

class LLGestureInventoryFetchObserver final : public LLInventoryFetchObserver
{
public:
	LLGestureInventoryFetchObserver()
	{
	}

	void done() override
	{
		// We have downloaded all the items, so refresh the floater
		LLFloaterGesture::refreshAll();

		gInventory.removeObserver(this);
		delete this;
	}
};

///////////////////////////////////////////////////////////////////////////////
// LLDelayedGestureError class: helper for reporting delayed load failures
///////////////////////////////////////////////////////////////////////////////

class LLDelayedGestureError
{
public:
	/**
	 * @brief Generates a missing gesture error
	 * @param id UUID of missing gesture
	 * Delays message for up to 5 seconds if UUID cannot be immediately
	 * converted to a text description
	 */
	static void gestureMissing(const LLUUID& id);

	/**
	 * @brief Generates a gesture failed to load error
	 * @param id UUID of missing gesture
	 * Delays message for up to 5 seconds if UUID cannot be immediately
	 * converted to a text description
	 */
	static void gestureFailedToLoad(const LLUUID& id);

private:
	struct LLErrorEntry
	{
		LLErrorEntry(const std::string& notify, const LLUUID& item)
		:	mTimer(),
			mNotifyName(notify),
			mItemID(item)
		{
		}

		LLTimer mTimer;
		std::string mNotifyName;
		LLUUID mItemID;
	};

	static bool doDialog(const LLErrorEntry& ent, bool uuid_ok = false);
	static void enqueue(const LLErrorEntry& ent);
	static void onIdle(void* userdata);

private:
	typedef std::list<LLErrorEntry> error_queue_t;
	static error_queue_t sQueue;
};

LLDelayedGestureError::error_queue_t LLDelayedGestureError::sQueue;

//static
void LLDelayedGestureError::gestureMissing(const LLUUID& id)
{
	LLErrorEntry ent("GestureMissing", id);
	if (!doDialog(ent))
	{
		enqueue(ent);
	}
}

//static
void LLDelayedGestureError::gestureFailedToLoad(const LLUUID& id)
{
	LLErrorEntry ent("UnableToLoadGesture", id);
	if (!doDialog(ent))
	{
		enqueue(ent);
	}
}

//static
void LLDelayedGestureError::enqueue(const LLErrorEntry& ent)
{
	if (sQueue.empty())
	{
		gIdleCallbacks.addFunction(onIdle, NULL);
	}
	sQueue.emplace_back(ent);
}

//static
void LLDelayedGestureError::onIdle(void* userdata)
{
	if (!sQueue.empty())
	{
		LLErrorEntry ent = sQueue.front();
		sQueue.pop_front();

		if (!doDialog(ent, false))
		{
			enqueue(ent);
		}
	}
	else
	{
		// Nothing to do anymore
		gIdleCallbacks.deleteFunction(onIdle, NULL);
	}
}

//static
bool LLDelayedGestureError::doDialog(const LLErrorEntry& ent, bool uuid_ok)
{
	LLSD args;

	LLInventoryItem* item = gInventory.getItem(ent.mItemID);
	if (item)
	{
		args["NAME"] = item->getName();
	}
	else
	{
		if (uuid_ok || ent.mTimer.getElapsedTimeF32() > MAX_NAME_WAIT_TIME)
		{
			args["NAME"] = ent.mItemID.asString();
		}
		else
		{
			return false;
		}
	}

	gNotifications.add(ent.mNotifyName, args);

	return true;
}

///////////////////////////////////////////////////////////////////////////////
// LLGestureManager class proper
///////////////////////////////////////////////////////////////////////////////

LLGestureManager::LLGestureManager()
:	mValid(false),
	mLoadingCount(0)
{
}

// We own the data for gestures, so clean them up.
LLGestureManager::~LLGestureManager()
{
	for (item_map_t::iterator it = mActive.begin(); it != mActive.end(); ++it)
	{
		LLMultiGesture* gesture = it->second;
		if (gesture)
		{
			delete gesture;
		}
	}
}

void LLGestureManager::load(const LLSD& gestures)
{
	LL_DEBUGS("Gestures") << "Loading " << gestures.size() << " gestures."
						  << LL_ENDL;
	uuid_vec_t item_ids;
	for (LLSD::array_const_iterator it = gestures.beginArray(),
									end = gestures.endArray();
		 it != end; ++it)
	{
		const LLSD& entry = *it;

		LLUUID item_id;
		if (entry.has("item_id"))
		{
			item_id = entry["item_id"].asUUID();
		}
		if (item_id.isNull()) continue;

		LLUUID asset_id;
		if (entry.has("asset_id"))
		{
			asset_id = entry["asset_id"].asUUID();
		}
		if (asset_id.isNull()) continue;

		// NOTE: false, false -> do not inform server and do not deactivate
		// similar gesture.
		activateGestureWithAsset(item_id, asset_id, false, false);
		// We need to fetch the inventory items for these gestures so we have
		// the names to populate the UI.
		item_ids.emplace_back(item_id);
	}

	LLGestureInventoryFetchObserver* fetch =
		new LLGestureInventoryFetchObserver();
	fetch->fetchItems(item_ids);
	// Deletes itself when done
	gInventory.addObserver(fetch);
}

// Use this version when you have the item_id but not the asset_id, and you
// KNOW the inventory is loaded.
void LLGestureManager::activateGesture(const LLUUID& item_id)
{
	LLViewerInventoryItem* item = gInventory.getItem(item_id);
	if (!item)
	{
		llwarns << "No item found for gesture: " << item_id << llendl;
		return;
	}

	LL_DEBUGS("Gestures") << "Activating gesture: " << item_id << LL_ENDL;

	mLoadingCount = 1;
	mDeactivateSimilarNames.clear();

	// true, false -> Inform server, do not deactivate similar gesture
	LLUUID asset_id = item->getAssetUUID();
	activateGestureWithAsset(item_id, asset_id, true, false);
}

void LLGestureManager::activateGestures(LLViewerInventoryItem::item_array_t& items)
{
	LLMessageSystem* msg = gMessageSystemp;
	if (!msg) return;	// Paranoia

	// Load up the assets
	S32 count = 0;
	LLViewerInventoryItem::item_array_t::const_iterator it;
	for (it = items.begin(); it != items.end(); ++it)
	{
		LLViewerInventoryItem* item = *it;
		if (!isGestureActive(item->getUUID()))
		{
			// Make gesture active and persistent through login sessions
			// -spatters 07-12-06
			activateGesture(item->getUUID());
			++count;
		}
	}

	mLoadingCount = count;
	mDeactivateSimilarNames.clear();

	for (it = items.begin(); it != items.end(); ++it)
	{
		LLViewerInventoryItem* item = *it;
		if (!isGestureActive(item->getUUID()))
		{
			activateGestureWithAsset(item->getUUID(), item->getAssetUUID(),
									 // Do not inform server, we will do that
									 // in bulk, but do deactivate any similar
									 // gesture.
									 false, true);
		}
	}

	// Inform the database of this change
	bool start_message = true;
	for (it = items.begin(); it != items.end(); ++it)
	{
		LLViewerInventoryItem* item = *it;

		if (isGestureActive(item->getUUID()))
		{
			continue;
		}

		if (start_message)
		{
			msg->newMessage("ActivateGestures");
			msg->nextBlock("AgentData");
			msg->addUUID("AgentID", gAgentID);
			msg->addUUID("SessionID", gAgentSessionID);
			msg->addU32("Flags", 0x0);
			start_message = false;
		}

		msg->nextBlock("Data");
		msg->addUUID("ItemID", item->getUUID());
		msg->addUUID("AssetID", item->getAssetUUID());
		msg->addU32("GestureFlags", 0x0);

		if (msg->getCurrentSendTotal() > MTUBYTES)
		{
			gAgent.sendReliableMessage();
			start_message = true;
		}
	}
	if (!start_message)
	{
		gAgent.sendReliableMessage();
	}
}

struct LLLoadInfo
{
	LLUUID	mItemID;
	bool	mInformServer;
	bool	mDeactivateSimilar;
};

// If inform_server is true, will send a message upstream to update the
// user_gesture_active table.
void LLGestureManager::activateGestureWithAsset(const LLUUID& item_id,
												const LLUUID& asset_id,
												bool inform_server,
												bool deactivate_similar)
{
	const LLUUID& base_item_id = gInventory.getLinkedItemID(item_id);

	if (!gAssetStoragep)
	{
		llwarns << "No valid asset storage !" << llendl;
		return;
	}
	// If gesture is already active, nothing to do.
	if (isGestureActive(base_item_id))
	{
		llwarns << "Tried to load gesture twice " << base_item_id << llendl;
		return;
	}

#if 0
	if (asset_id.isNull())
	{
		llwarns << "Gesture has no asset !" << llendl;
		return;
	}
#endif

	// For now, put NULL into the item map. We will build a gesture class
	// object when the asset data arrives.
	mActive[base_item_id] = NULL;

	// Copy the UUID
	if (asset_id.notNull())
	{
		LLLoadInfo* info = new LLLoadInfo;
		info->mItemID = base_item_id;
		info->mInformServer = inform_server;
		info->mDeactivateSimilar = deactivate_similar;
		gAssetStoragep->getAssetData(asset_id, LLAssetType::AT_GESTURE,
									 onLoadComplete, (void*)info,
									 true); // high priority
	}
	else
	{
		notifyObservers();
	}
}

void LLGestureManager::deactivateGesture(const LLUUID& item_id)
{
	const LLUUID& base_item_id = gInventory.getLinkedItemID(item_id);
	item_map_t::iterator it = mActive.find(base_item_id);
	if (it == mActive.end())
	{
		llwarns << "Gesture " << base_item_id << " was already inactive."
				<< llendl;
		return;
	}

	LL_DEBUGS("Gestures") << "Deactivating gesture: " << item_id << LL_ENDL;

	// mActive owns this gesture pointer, so clean up memory.
	LLMultiGesture* gesture = it->second;

	// Can be NULL gestures in the map
	if (gesture)
	{
		stopGesture(gesture);
		delete gesture;
	}

	mActive.erase(it);
	gInventory.addChangedMask(LLInventoryObserver::LABEL, base_item_id);

	// Inform the database of this change
	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessage("DeactivateGestures");
	msg->nextBlock("AgentData");
	msg->addUUID("AgentID", gAgentID);
	msg->addUUID("SessionID", gAgentSessionID);
	msg->addU32("Flags", 0x0);

	msg->nextBlock("Data");
	msg->addUUID("ItemID", base_item_id);
	msg->addU32("GestureFlags", 0x0);

	gAgent.sendReliableMessage();

	notifyObservers();
}

void LLGestureManager::deactivateSimilarGestures(LLMultiGesture* in,
												 const LLUUID& in_item_id)
{
	const LLUUID& base_in_item_id = gInventory.getLinkedItemID(in_item_id);
	uuid_vec_t gest_item_ids;

	// Deactivate all gestures that match
	item_map_t::iterator it;
	for (it = mActive.begin(); it != mActive.end(); )
	{
		const LLUUID& item_id = it->first;
		LLMultiGesture* gest = it->second;

		// Do not deactivate the gesture we are looking for duplicates of
		// (for replaceGesture)
		if (!gest || item_id == base_in_item_id)
		{
			// legal, can have null pointers in list
			++it;
		}
		else if ((!gest->mTrigger.empty() && gest->mTrigger == in->mTrigger) ||
				 (gest->mKey != KEY_NONE && gest->mKey == in->mKey &&
				  gest->mMask == in->mMask))
		{
			gest_item_ids.emplace_back(item_id);

			stopGesture(gest);
			delete gest;

			mActive.erase(it++);
			gInventory.addChangedMask(LLInventoryObserver::LABEL, item_id);
		}
		else
		{
			++it;
		}
	}

	if (!gest_item_ids.empty())
	{
		// Inform database of the change
		LLMessageSystem* msg = gMessageSystemp;
		bool start_message = true;
		for (uuid_vec_t::const_iterator it = gest_item_ids.begin(),
										end = gest_item_ids.end();
			 it != end; ++it)
		{
			if (start_message)
			{
				msg->newMessage("DeactivateGestures");
				msg->nextBlock("AgentData");
				msg->addUUID("AgentID", gAgentID);
				msg->addUUID("SessionID", gAgentSessionID);
				msg->addU32("Flags", 0x0);
				start_message = false;
			}

			msg->nextBlock("Data");
			msg->addUUID("ItemID", *it);
			msg->addU32("GestureFlags", 0x0);

			if (msg->getCurrentSendTotal() > MTUBYTES)
			{
				gAgent.sendReliableMessage();
				start_message = true;
			}
		}
		if (!start_message)
		{
			gAgent.sendReliableMessage();
		}

		// Add to the list of names for the user.
		for (uuid_vec_t::const_iterator it = gest_item_ids.begin(),
										end = gest_item_ids.end();
			 it != end; ++it)
		{
			LLViewerInventoryItem* item = gInventory.getItem(*it);
			if (item)
			{
				mDeactivateSimilarNames.append(item->getName());
				mDeactivateSimilarNames.append("\n");
			}
		}
	}

	notifyObservers();
}

bool LLGestureManager::isGestureActive(const LLUUID& item_id)
{
	const LLUUID& base_item_id = gInventory.getLinkedItemID(item_id);
	item_map_t::iterator it = mActive.find(base_item_id);
	return it != mActive.end();
}

bool LLGestureManager::isGesturePlaying(const LLUUID& item_id)
{
	const LLUUID& base_item_id = gInventory.getLinkedItemID(item_id);
	item_map_t::iterator it = mActive.find(base_item_id);
	if (it == mActive.end()) return false;

	LLMultiGesture* gesture = it->second;
	return gesture && gesture->mPlaying;
}

void LLGestureManager::replaceGesture(const LLUUID& item_id,
									  LLMultiGesture* new_gesture,
									  const LLUUID& asset_id)
{
	const LLUUID& base_item_id = gInventory.getLinkedItemID(item_id);
	item_map_t::iterator it = mActive.find(base_item_id);
	if (it == mActive.end())
	{
		llwarns << "Gesture " << base_item_id
				<< " is inactive: cannot replace !" << llendl;
		return;
	}

	LLMultiGesture* old_gesture = it->second;
	stopGesture(old_gesture);

	mActive.erase(base_item_id);

	mActive[base_item_id] = new_gesture;

	// replaceGesture(const LLUUID& item_id, const LLUUID& new_asset_id) below
	// replaces Ids without replacing the gesture...
	if (old_gesture != new_gesture)
	{
		delete old_gesture;
	}

	if (asset_id.notNull())
	{
		mLoadingCount = 1;
		mDeactivateSimilarNames.clear();

		LLLoadInfo* info = new LLLoadInfo;
		info->mItemID = base_item_id;
		info->mInformServer = true;
		info->mDeactivateSimilar = false;
		gAssetStoragep->getAssetData(asset_id, LLAssetType::AT_GESTURE,
									 onLoadComplete, (void*)info,
									 true); // high priority
	}

	notifyObservers();
}

void LLGestureManager::replaceGesture(const LLUUID& item_id,
									  const LLUUID& new_asset_id)
{
	const LLUUID& base_item_id = gInventory.getLinkedItemID(item_id);
	item_map_t::iterator it = gGestureManager.mActive.find(base_item_id);
	if (it == mActive.end())
	{
		llwarns << "Gesture " << base_item_id
				<< " is inactive: cannot replace !" << llendl;
		return;
	}

	// mActive owns this gesture pointer, so clean up memory.
	LLMultiGesture* gesture = it->second;
	gGestureManager.replaceGesture(base_item_id, gesture, new_asset_id);
}

void LLGestureManager::playGesture(LLMultiGesture* gesture)
{
	if (!gesture) return;

//MK
	if (gRLenabled && gRLInterface.contains("sendgesture")) return;
//mk

	// Reset gesture to first step
	gesture->mCurrentStep = 0;

	// Add to list of playing
	gesture->mPlaying = true;
	mPlaying.push_back(gesture);

	// And get it going
	stepGesture(gesture);

	notifyObservers();
}

// Convenience function that looks up the item_id for you.
void LLGestureManager::playGesture(const LLUUID& item_id)
{
	const LLUUID& base_item_id = gInventory.getLinkedItemID(item_id);
	item_map_t::iterator it = mActive.find(base_item_id);
	if (it != mActive.end())
	{
		LLMultiGesture* gesture = it->second;
		if (gesture)
		{
			playGesture(gesture);
		}
	}
}

// Iterates through space delimited tokens in string, triggering any gestures
// found. Generates a revised string that has the found tokens replaced by
// their replacement strings and when a replacement is found (as a minor side
// effect) has multiple spaces in a row replaced by single spaces.
bool LLGestureManager::triggerAndReviseString(const std::string& utf8str,
											  std::string* revised_string)
{
	std::string tokenized = utf8str;

	bool found_gestures = false;
	bool first_token = true;

	typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
	boost::char_separator<char> sep(" ");
	tokenizer tokens(tokenized, sep);
	tokenizer::iterator token_iter;

	for (token_iter = tokens.begin(); token_iter != tokens.end(); ++token_iter)
	{
		const char* cur_token = token_iter->c_str();
		LLMultiGesture* gesture = NULL;

		// Only pay attention to the first gesture in the string.
		if (!found_gestures)
		{
			// Collect gestures that match
			std::vector <LLMultiGesture*> matching;
			for (item_map_t::iterator it = mActive.begin(),
									  end = mActive.end();
				 it != end; ++it)
			{
				gesture = it->second;
				// Gesture asset data might not have arrived yet
				if (!gesture) continue;

				if (LLStringUtil::compareInsensitive(gesture->mTrigger,
													 cur_token) == 0)
				{
					matching.push_back(gesture);
				}
			}

			gesture = NULL;
			if (matching.size())
			{
				found_gestures = true;

				// Choose one at random
				S32 random = ll_rand(matching.size());

				gesture = matching[random];

				playGesture(gesture);

				if (!gesture->mReplaceText.empty())
				{
					if (!first_token && revised_string)
					{
						revised_string->append(" ");
					}

					// Do not muck with the user's capitalization if we do not
					// have to.
					if (LLStringUtil::compareInsensitive(cur_token,
														 gesture->mReplaceText) == 0)
					{
						if (revised_string)
						{
							revised_string->append(cur_token);
						}
					}
					else if (revised_string)
					{
						revised_string->append(gesture->mReplaceText);
					}
				}
			}
		}

		if (!gesture)
		{
			// This token does not match a gesture. Pass it through to the
			// output.
			if (!first_token && revised_string)
			{
				revised_string->append(" ");
			}
			if (revised_string)
			{
				revised_string->append(cur_token);
			}
		}

		first_token = false;
	}

	// If no gesture is involved, restore the original text to preserve spacing
	if (revised_string && !found_gestures)
	{
		revised_string->assign(utf8str);
	}

	return found_gestures;
}

bool LLGestureManager::triggerGesture(KEY key, MASK mask)
{
	std::vector <LLMultiGesture*> matching;

	// Collect matching gestures
	for (item_map_t::iterator it = mActive.begin(); it != mActive.end(); ++it)
	{
		LLMultiGesture* gesture = it->second;
		if (gesture && gesture->mKey == key && gesture->mMask == mask)
		{
			matching.push_back(gesture);
		}
	}

	// Choose one and play it
	if (matching.size() > 0)
	{
		U32 random = ll_rand(matching.size());
		LLMultiGesture* gesture = matching[random];
		playGesture(gesture);
		return true;
	}

	return false;
}

S32 LLGestureManager::getPlayingCount() const
{
	return mPlaying.size();
}

struct IsGesturePlaying
{
	bool operator()(const LLMultiGesture* gesture) const
	{
		return gesture->mPlaying;
	}
};

void LLGestureManager::update()
{
	for (S32 i = 0, count = mPlaying.size(); i < count; ++i)
	{
		stepGesture(mPlaying[i]);
	}

	// Clear out gestures that are done, by moving all the ones that are still
	// playing to the front.
	std::vector<LLMultiGesture*>::iterator new_end;
	new_end = std::partition(mPlaying.begin(), mPlaying.end(),
							 IsGesturePlaying());

	// Something finished playing
	if (new_end != mPlaying.end())
	{
		// Delete the completed gestures that want deletion
		for (std::vector<LLMultiGesture*>::iterator it = new_end;
			 it != mPlaying.end(); ++it)
		{
			LLMultiGesture* gesture = *it;
			if (gesture && gesture->mDoneCallback)
			{
				gesture->mDoneCallback(gesture, gesture->mCallbackData);
			}
		}

		// And take done gestures out of the playing list
		mPlaying.erase(new_end, mPlaying.end());

		notifyObservers();
	}
}

// Run all steps until you're either done or hit a wait.
void LLGestureManager::stepGesture(LLMultiGesture* gesture)
{
	if (!gesture || !isAgentAvatarValid())
	{
		return;
	}

	LLVOAvatar::anim_it_t play_it;
	// Of the ones that started playing, have any stopped ?
	for (uuid_list_t::iterator it = gesture->mPlayingAnimIDs.begin();
		 it != gesture->mPlayingAnimIDs.end(); )
	{
		// look in signaled animations (simulator's view of what is currently
		// playing.
		play_it = gAgentAvatarp->mSignaledAnimations.find(*it);
		if (play_it != gAgentAvatarp->mSignaledAnimations.end())
		{
			++it;
		}
		else
		{
			// not found, so not currently playing or scheduled to play: delete
			// from the triggered set
			gesture->mPlayingAnimIDs.erase(it++);
		}
	}

	// Of all the animations that we asked the sim to start for us, pick up the
	// ones that have actually started.
	for (uuid_list_t::iterator it = gesture->mRequestedAnimIDs.begin();
		 it != gesture->mRequestedAnimIDs.end(); )
	{
		play_it = gAgentAvatarp->mSignaledAnimations.find(*it);
		if (play_it != gAgentAvatarp->mSignaledAnimations.end())
		{
			// Hooray, this animation has started playing !  Copy into playing.
			gesture->mPlayingAnimIDs.emplace(*it);
			gesture->mRequestedAnimIDs.erase(it++);
		}
		else
		{
			// Nope, not playing yet
			++it;
		}
	}

	// Run the current steps
	bool waiting = false;
	while (!waiting && gesture->mPlaying)
	{
		// Get the current step, if there is one. Otherwise enter the waiting
		// at end state.
		LLGestureStep* step = NULL;
		if (gesture->mCurrentStep < (S32)gesture->mSteps.size())
		{
			step = gesture->mSteps[gesture->mCurrentStep];
			llassert(step != NULL);
		}
		else
		{
			// Step stays null, we are off the end
			gesture->mWaitingAtEnd = true;
		}

		// If we are waiting at the end, wait for all gestures to stop playing.
		// *TODO: wait for all sounds to complete as well.
		if (gesture->mWaitingAtEnd)
		{
			// Neither do we have any pending requests, nor are they still
			// playing.
			if (gesture->mRequestedAnimIDs.empty() &&
				gesture->mPlayingAnimIDs.empty())
			{
				// All animations are done playing
				gesture->mWaitingAtEnd = false;
				gesture->mPlaying = false;
			}
			else
			{
				waiting = true;
			}
			continue;
		}

		// If we are waiting on our animations to stop, poll for completion.
		if (gesture->mWaitingAnimations)
		{
			// Neither do we have any pending requests, nor are they still
			// playing.
			if (gesture->mRequestedAnimIDs.empty() &&
				gesture->mPlayingAnimIDs.empty())
			{
				// All animations are done playing
				gesture->mWaitingAnimations = false;
				++gesture->mCurrentStep;
			}
			else if (gesture->mWaitTimer.getElapsedTimeF32() > MAX_WAIT_ANIM_SECS)
			{
				// We have waited too long for an animation
				llinfos << "Waited too long for animations to stop, continuing gesture."
						<< llendl;
				gesture->mWaitingAnimations = false;
				++gesture->mCurrentStep;
			}
			else
			{
				waiting = true;
			}
			continue;
		}

		// If we are waiting a fixed amount of time, check for timer expiration
		if (gesture->mWaitingTimer)
		{
			// We are waiting for a certain amount of time to pass
			LLGestureStepWait* wait_step = (LLGestureStepWait*)step;

			F32 elapsed = gesture->mWaitTimer.getElapsedTimeF32();
			if (elapsed > wait_step->mWaitSeconds)
			{
				// Wait is done, continue execution
				gesture->mWaitingTimer = false;
				++gesture->mCurrentStep;
			}
			else
			{
				// We are waiting, so execution is done for now
				waiting = true;
			}
			continue;
		}

		// Not waiting, do normal execution
		runStep(gesture, step);
	}
}

void LLGestureManager::runStep(LLMultiGesture* gesture, LLGestureStep* step)
{
	switch (step->getType())
	{
		case STEP_ANIMATION:
		{
			LLGestureStepAnimation* anim_step = (LLGestureStepAnimation*)step;
			if (anim_step->mAnimAssetID.isNull())
			{
				++gesture->mCurrentStep;
			}

			if (anim_step->mFlags & ANIM_FLAG_STOP)
			{
				gAgent.sendAnimationRequest(anim_step->mAnimAssetID,
											ANIM_REQUEST_STOP);
				// Remove it from our request set in case we just requested it
				uuid_list_t::iterator set_it =
					gesture->mRequestedAnimIDs.find(anim_step->mAnimAssetID);
				if (set_it != gesture->mRequestedAnimIDs.end())
				{
					gesture->mRequestedAnimIDs.erase(set_it);
				}
			}
			else
			{
				gAgent.sendAnimationRequest(anim_step->mAnimAssetID,
											ANIM_REQUEST_START);
				// Indicate that we've requested this animation to play as
				// part of this gesture (but it would not start playing for at
				// least one round-trip to simulator).
				gesture->mRequestedAnimIDs.emplace(anim_step->mAnimAssetID);
			}
			++gesture->mCurrentStep;
			break;
		}

		case STEP_SOUND:
		{
			LLGestureStepSound* sound_step = (LLGestureStepSound*)step;
			const LLUUID& sound_id = sound_step->mSoundAssetID;
			send_sound_trigger(sound_id, 1.f); // 100% relative volume
			++gesture->mCurrentStep;
			break;
		}

		case STEP_CHAT:
		{
			LLGestureStepChat* chat_step = (LLGestureStepChat*)step;
			std::string chat_text = chat_step->mChatText;
			// Do not animate the nodding, as this might not blend with other
			// playing animations.
			constexpr bool animate = false;
//MK
			if (gRLenabled && gRLInterface.contains("sendchat") &&
				chat_text.find("/me ") != 0 && chat_text.find("/me'") != 0)
			{
				chat_text = gRLInterface.crunchEmote(chat_text, 20);
			}
//mk
			if (gChatBarp)
			{
				gChatBarp->sendChatFromViewer(chat_text, CHAT_TYPE_NORMAL,
											  animate);
			}
			++gesture->mCurrentStep;
			break;
		}

		case STEP_WAIT:
		{
			LLGestureStepWait* wait_step = (LLGestureStepWait*)step;
			if (wait_step->mFlags & WAIT_FLAG_TIME)
			{
				gesture->mWaitingTimer = true;
				gesture->mWaitTimer.reset();
			}
			else if (wait_step->mFlags & WAIT_FLAG_ALL_ANIM)
			{
				gesture->mWaitingAnimations = true;
				// Use the wait timer as a deadlock breaker for animation
				// waits.
				gesture->mWaitTimer.reset();
			}
			else
			{
				++gesture->mCurrentStep;
			}
			// Do not increment instruction pointer until wait is complete.
			break;
		}

		default:
			break;
	}
}

//static
void LLGestureManager::onLoadComplete(const LLUUID& asset_uuid,
									  LLAssetType::EType type, void* user_data,
									  S32 status, LLExtStat)
{
	if (!user_data) return;

	LLLoadInfo* info = (LLLoadInfo*)user_data;
	LLUUID item_id = info->mItemID;
	bool inform_server = info->mInformServer;
	bool deactivate_similar = info->mDeactivateSimilar;
	delete info;

	--gGestureManager.mLoadingCount;

	if (status == 0)
	{
		LLFileSystem file(asset_uuid);
		S32 size = file.getSize();
		char* buffer = (char*)malloc(size + 1);
		if (!buffer)
		{
			llwarns << "Memory allocation failed !" << llendl;
			return;
		}
		file.read((U8*)buffer, size);
		// ensure there's a trailing NULL so strlen will work.
		buffer[size] = '\0';

		LLMultiGesture* gesture = new LLMultiGesture();

		LLDataPackerAsciiBuffer dp(buffer, size + 1);
		if (gesture->deserialize(dp))
		{
			if (deactivate_similar)
			{
				gGestureManager.deactivateSimilarGestures(gesture, item_id);

				// Display deactivation message if this was the last of the
				// bunch.
				if (gGestureManager.mLoadingCount == 0 &&
					gGestureManager.mDeactivateSimilarNames.length() > 0)
				{
					// We are done with this set of deactivations
					LLSD args;
					args["NAMES"] = gGestureManager.mDeactivateSimilarNames;
					gNotifications.add("DeactivatedGesturesTrigger", args);
				}
			}

			// Gesture may be present already...
			item_map_t::iterator it = gGestureManager.mActive.find(item_id);
			if (it != gGestureManager.mActive.end())
			{
				// In case someone manages to activate, deactivate and then
				// activate gesture again before the asset finishes loading...
				// LLLoadInfo will have a different pointer, asset storage will
				// see it as a different request, resulting in two callbacks.
				LLMultiGesture* old_gesture = it->second;
				if (old_gesture && old_gesture != gesture)
				{
					// deactivateSimilarGestures() did not turn this one off
					// because of matching item_id.
					gGestureManager.stopGesture(old_gesture);
					delete old_gesture;
				}
			}

			// Everything has been successful. Add to the active list.
			gGestureManager.mActive[item_id] = gesture;
			gInventory.addChangedMask(LLInventoryObserver::LABEL, item_id);
			if (inform_server)
			{
				// Inform the database of this change
				LLMessageSystem* msg = gMessageSystemp;
				msg->newMessage("ActivateGestures");
				msg->nextBlock("AgentData");
				msg->addUUID("AgentID", gAgentID);
				msg->addUUID("SessionID", gAgentSessionID);
				msg->addU32("Flags", 0x0);

				msg->nextBlock("Data");
				msg->addUUID("ItemID", item_id);
				msg->addUUID("AssetID", asset_uuid);
				msg->addU32("GestureFlags", 0x0);

				gAgent.sendReliableMessage();
			}

			gGestureManager.notifyObservers();
		}
		else
		{
			llwarns << "Unable to load gesture" << llendl;
			gGestureManager.mActive.erase(item_id);
			delete gesture;
		}

		free((void*)buffer);
	}
	else
	{
		gViewerStats.incStat(LLViewerStats::ST_DOWNLOAD_FAILED);
		notifyLoadFailed(item_id, status);
		llwarns << "Problem loading gesture: " << status << llendl;

		gGestureManager.mActive.erase(item_id);
	}
}

//static
void LLGestureManager::notifyLoadFailed(const LLUUID& item_id, S32 status)
{
	if (status == LL_ERR_FILE_EMPTY ||
		status == LL_ERR_ASSET_REQUEST_NOT_IN_DATABASE)
	{
		LLDelayedGestureError::gestureMissing(item_id);
	}
	else
	{
		LLDelayedGestureError::gestureFailedToLoad(item_id);
	}
}

void LLGestureManager::stopGesture(LLMultiGesture* gesture)
{
	if (!gesture) return;

	// Stop any animations that this gesture is currently playing
	for (uuid_list_t::iterator it = gesture->mRequestedAnimIDs.begin(),
							   end = gesture->mRequestedAnimIDs.end();
		 it != end; ++it)
	{
		const LLUUID& anim_id = *it;
		gAgent.sendAnimationRequest(anim_id, ANIM_REQUEST_STOP);
	}
	for (uuid_list_t::iterator it = gesture->mPlayingAnimIDs.begin(),
							   end = gesture->mPlayingAnimIDs.end();
		 it != end; ++it)
	{
		const LLUUID& anim_id = *it;
		gAgent.sendAnimationRequest(anim_id, ANIM_REQUEST_STOP);
	}

	std::vector<LLMultiGesture*>::iterator it;
	it = std::find(mPlaying.begin(), mPlaying.end(), gesture);
	while (it != mPlaying.end())
	{
		mPlaying.erase(it);
		it = std::find(mPlaying.begin(), mPlaying.end(), gesture);
	}

	gesture->reset();

	if (gesture->mDoneCallback)
	{
		gesture->mDoneCallback(gesture, gesture->mCallbackData);
	}

	notifyObservers();
}

void LLGestureManager::stopGesture(const LLUUID& item_id)
{
	const LLUUID& base_item_id = gInventory.getLinkedItemID(item_id);
	item_map_t::iterator it = mActive.find(base_item_id);
	if (it != mActive.end())
	{
		LLMultiGesture* gesture = it->second;
		if (gesture)
		{
			stopGesture(gesture);
		}
	}
}

void LLGestureManager::addObserver(LLGestureManagerObserver* observer)
{
	mObservers.push_back(observer);
}

void LLGestureManager::removeObserver(LLGestureManagerObserver* observer)
{
	std::vector<LLGestureManagerObserver*>::iterator it;
	it = std::find(mObservers.begin(), mObservers.end(), observer);
	if (it != mObservers.end())
	{
		mObservers.erase(it);
	}
}

// Call this method when it is time to update everyone on a new state. Copy the
// list because an observer could respond by removing itself from the list.
void LLGestureManager::notifyObservers()
{
	std::vector<LLGestureManagerObserver*> observers = mObservers;

	std::vector<LLGestureManagerObserver*>::iterator it;
	for (it = observers.begin(); it != observers.end(); ++it)
	{
		LLGestureManagerObserver* observer = *it;
		if (observer)
		{
			observer->changed();
		}
	}
}

bool LLGestureManager::matchPrefix(const std::string& in_str,
								   std::string* out_str)
{
	// Return whole trigger, if received text equals to it
	for (item_map_t::iterator it = mActive.begin(), end = mActive.end();
		 it != end; ++it)
	{
		LLMultiGesture* gesture = it->second;
		if (gesture)
		{
			const std::string& trigger = gesture->getTrigger();
			if (!LLStringUtil::compareInsensitive(in_str, trigger))
			{
				*out_str = trigger;
				return true;
			}
		}
	}

	// Return common chars, if more than one trigger matches the prefix
	std::string rest_of_match, buf;
	size_t in_len = in_str.length();
	for (item_map_t::iterator it = mActive.begin(), end = mActive.end();
		 it != end; ++it)
	{
		LLMultiGesture* gesture = it->second;
		if (gesture)
		{
			const std::string& trigger = gesture->getTrigger();

			if (in_len > trigger.length())
			{
				// Too short, bail out
				continue;
			}

			std::string trigger_trunc = trigger;
			LLStringUtil::truncate(trigger_trunc, in_len);
			if (!LLStringUtil::compareInsensitive(in_str, trigger_trunc))
			{
				if (rest_of_match.empty())
				{
					rest_of_match = trigger.substr(in_str.size());
				}
				std::string cur_rest_of_match = trigger.substr(in_str.size());
				buf.clear();
				S32 i = 0;
				while (i < (S32)rest_of_match.length() &&
					   i < (S32)cur_rest_of_match.length())
				{
					if (rest_of_match[i] == cur_rest_of_match[i])
				    {
						buf.push_back(rest_of_match[i]);
				    }
				    else
				    {
				    	if (i == 0)
				    	{
				    		rest_of_match.clear();
				    	}
				    	break;
				    }
					++i;
				}
				if (rest_of_match.empty())
				{
					return false;
				}
				if (!buf.empty())
				{
					rest_of_match = buf;
				}
			}
		}
	}

	if (!rest_of_match.empty())
	{
		*out_str = in_str + rest_of_match;
		return true;
	}

	return false;
}

void LLGestureManager::getItemIDs(uuid_vec_t* ids)
{
	for (item_map_t::const_iterator it = mActive.begin(), end = mActive.end();
		 it != end; ++it)
	{
		ids->emplace_back(it->first);
	}
}
