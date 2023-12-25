/**
 * @file   llevents.cpp
 * @author Nat Goodspeed
 * @date   2008-09-12
 * @brief  Implementation for llevents.
 *
 * $LicenseInfo:firstyear=2008&license=viewergpl$
 *
 * Copyright (c) 2010, Linden Research, Inc.
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

#if LL_WINDOWS
#pragma warning (disable : 4675) // "resolved by ADL" -- just as I want!
#endif

#include <algorithm>
#include <cctype>
#include <sstream>
#include <typeinfo>

#include "boost/fiber/exceptions.hpp"
#include "boost/lexical_cast.hpp"
#include "boost/ref.hpp"
#include "boost/range/iterator_range.hpp"

#include "llevents.h"

#include "llsdutil.h"

/*****************************************************************************
*   LLEventPumps
*****************************************************************************/

// Global
LLEventPumps gEventPumps;

//static
bool LLEventPumps::sInstanceDestroyed = false;

const std::string LLEventPump::ANONYMOUS = std::string();

LLEventPump& LLEventPumps::obtain(const std::string& name)
{
	PumpMap::iterator it = mPumpMap.find(name);
	if (it != mPumpMap.end())
	{
		// Here we already have an LLEventPump instance with the requested
		// name.
		return *it->second;
	}

	// Here we must instantiate an LLEventPump subclass.
	LLEventPump* new_instance = new LLEventStream(name);

	// LLEventPump's constructor implicitly registers each new instance in
	// mPumpMap. But remember that we instantiated it (in mOurPumps) so we'll
	// delete it later.
	mOurPumps.insert(new_instance);

	return *new_instance;
}

LLEventPump& LLEventPumps::make(const std::string& name, bool tweak,
								const std::string& type)
{
	if (!type.empty() && type != "LLEventStream")
	{
		throw BadType(type);
	}

	// Here we must instantiate an LLEventPump subclass.
	LLEventPump* new_instance = new LLEventStream(name, tweak);

	// LLEventPump's constructor implicitly registers each new instance in
	// mPumpMap. But remember that we instantiated it (in mOurPumps) so we'll
	// delete it later.
	mOurPumps.insert(new_instance);

	return *new_instance;
}

bool LLEventPumps::post(const std::string& name, const LLSD& message)
{
	PumpMap::iterator it = mPumpMap.find(name);
	if (it == mPumpMap.end())
	{
		return false;
	}
	return it->second->post(message);
}

void LLEventPumps::flush()
{
	// Flush every known LLEventPump instance. Leave it up to each instance to
	// decide what to do with the flush() call.
	for (PumpMap::iterator it = mPumpMap.begin(), end = mPumpMap.end();
		 it != end; ++it)
	{
		it->second->flush();
	}
}

void LLEventPumps::clear()
{
	// Clear every known LLEventPump instance. Leave it up to each instance to
	// decide what to do with the clear() call.
	for (PumpMap::iterator it = mPumpMap.begin(), end = mPumpMap.end();
		 it != end; ++it)
	{
		it->second->clear();
	}
}

void LLEventPumps::reset()
{
	// Reset every known LLEventPump instance. Leave it up to each instance to
	// decide what to do with the reset() call.
	for (PumpMap::iterator it = mPumpMap.begin(), end = mPumpMap.end();
		 it != end; ++it)
	{
		it->second->reset();
	}
}

std::string LLEventPumps::registerNew(const LLEventPump& pump,
									  const std::string& name, bool tweak)
{
	std::pair<PumpMap::iterator, bool> inserted =
		mPumpMap.emplace(name, const_cast<LLEventPump*>(&pump));
	// If the insert worked, then the name is unique; return that.
	if (inserted.second)
	{
		return name;
	}

	// Here the new entry was NOT inserted, and therefore name is not unique.
	// Unless we are permitted to tweak it, that's Bad.
	if (!tweak)
	{
		throw LLEventPump::DupPumpName("Duplicate LLEventPump name '" + name +
									   "'");
	}

	// The passed name is not unique, but we are permitted to tweak it. Find
	// the first decimal-integer suffix not already taken. The insert() attempt
	// above will have set inserted.first to the iterator of the existing
	// entry by that name. Starting there, walk forward until we reach an
	// entry that does not start with 'name'. For each entry consisting of name
	// + integer suffix, capture the integer suffix in a set. Use a set
	// because we are going to encounter string suffixes in the order: name1,
	// name10, name11, name2, ... Walking those possibilities in that order
	// is not convenient to detect the first available "hole."
	std::set<int> suffixes;
	PumpMap::iterator pmi(inserted.first), pmend(mPumpMap.end());
	// We already know inserted.first references the existing entry with
	// 'name' as the key; skip that one and start with the next.
	while (++pmi != pmend)
	{
		if (pmi->first.substr(0, name.length()) != name)
		{
			// Found the first entry beyond the entries starting with 'name':
			// stop looping.
			break;
		}

		// Here we are looking at an entry that starts with 'name'. Is the rest
		// of it an integer?
		// Dubious (?) assumption: in the local character set, decimal digits
		// are in increasing order such that '9' is the last of them. This
		// test deals with 'name' values such as 'a', where there might be a
		// very large number of entries starting with 'a' whose suffixes
		// are not integers. A secondary assumption is that digit characters
		// precede most common name characters (true in ASCII, false in
		// EBCDIC). The test below is correct either way, but it is worth more
		// if the assumption holds.
		if (pmi->first[name.length()] > '9')
		{
			break;
		}

		// It should be cheaper to detect that we're not looking at a digit
		// character -- and therefore the suffix can't possibly be an integer
		// -- than to attempt the lexical_cast and catch the exception.
		if (!std::isdigit(pmi->first[name.length()]))
		{
			continue;
		}

		// Okay, the first character of the suffix is a digit, it's worth at
		// least attempting to convert to int.
		try
		{
			suffixes.insert(boost::lexical_cast<int>(pmi->first.substr(name.length())));
		}
		catch (const boost::bad_lexical_cast&)
		{
			// If the rest of pmi->first is not an int, just ignore it.
		}
	}
	// Here we have accumulated in 'suffixes' all existing int suffixes of the
	// entries starting with 'name'. Find the first unused one.
	int suffix = 1;
	for ( ; suffixes.find(suffix) != suffixes.end(); ++suffix) ;

	// Here 'suffix' is not in 'suffixes'. Construct a new name based on that
	// suffix, insert it and return it.
	std::ostringstream out;
	out << name << suffix;

	return registerNew(pump, out.str(), tweak);
}

void LLEventPumps::unregister(const LLEventPump& pump)
{
	// Remove this instance from mPumpMap
	PumpMap::iterator it = mPumpMap.find(pump.getName());
	if (it != mPumpMap.end())
	{
		mPumpMap.erase(it);
	}
	// If this instance is one we created, also remove it from mOurPumps so we
	// would not try again to delete it later !
	PumpSet::iterator it2 = mOurPumps.find(const_cast<LLEventPump*>(&pump));
	if (it2 != mOurPumps.end())
	{
		mOurPumps.erase(it2);
	}
}

LLEventPumps::~LLEventPumps()
{
	// On destruction, delete every LLEventPump we instantiated (via obtain()).
	// CAREFUL: deleting an LLEventPump calls its destructor, which calls
	// unregister(), which removes that LLEventPump instance from mOurPumps.
	// So an iterator loop over mOurPumps to delete contained LLEventPump
	// instances is dangerous !  Instead, delete them one at a time until
	// mOurPumps is empty.
	while (!mOurPumps.empty())
	{
		delete *mOurPumps.begin();
	}
	// Reset every remaining registered LLEventPump subclass instance: those
	// we did not instantiate using either make() or obtain().
	reset();
	sInstanceDestroyed = true;	// Flag as gone forever. HB
}

bool LLEventPumps::sendReply(const LLSD& reply, const LLSD& request,
							 const std::string& reply_key)
{
	// If the original request has no value for reply_key, it is pointless to
	// construct or send a reply event: on which LLEventPump should we send
	// it ?  Allow that to be optional: if the caller wants to require
	// reply_key, it can so specify when registering the operation method.
	if (!request.has(reply_key))
	{
		return false;
	}

    // Here the request definitely contains reply_key; reasonable to proceed.
	// Copy 'reply' to modify it.
	LLSD newreply(reply);
	// Get the ["reqid"] element from request
	LLReqID req_id(request);
	// And copy it to 'newreply'.
	req_id.stamp(newreply);
	// Send reply on LLEventPump named in request[reply_key]. Do not forget to
	// send the modified 'newreply' instead of the original 'reply'.
	return obtain(request[reply_key]).post(newreply);
}

/*****************************************************************************
*   LLEventPump
*****************************************************************************/
#if LL_WINDOWS
# pragma warning (push)
// 'this' is intentionally used in initializer list:
# pragma warning (disable : 4355)
#endif

LLEventPump::LLEventPump(const std::string& name, bool tweak)
	// Register every new instance with LLEventPumps
:	mName(gEventPumps.registerNew(*this, name, tweak)),
	mSignal(std::make_shared<LLStandardSignal>()),
	mEnabled(true)
{
}

#if LL_WINDOWS
# pragma warning (pop)
#endif

LLEventPump::~LLEventPump()
{
	// Unregister this doomed instance from LLEventPumps, but only when the
	// latter is still around !
	if (!LLEventPumps::destroyed())
	{
		gEventPumps.unregister(*this);
	}
}

// Static data member
const LLEventPump::NameList LLEventPump::empty;

//static
std::string LLEventPump::inventName(const std::string& pfx)
{
	static S32 suffix = 0;
	return llformat("%s%d", pfx.c_str(), suffix++);
}

void LLEventPump::clear()
{
	mConnectionListMutex.lock();
	// Destroy the original LLStandardSignal instance, replacing it with a
	// whole new one.
	mSignal = std::make_shared<LLStandardSignal>();
	mConnections.clear();
	mConnectionListMutex.unlock();
}

void LLEventPump::reset()
{
	mConnectionListMutex.lock();
	mSignal.reset();
	mConnections.clear();
#if 0
	mDeps.clear();
#endif
	mConnectionListMutex.unlock();
}

LLBoundListener LLEventPump::listen_impl(const std::string& name,
										 const LLEventListener& listener,
										 const NameList& after,
										 const NameList& before)
{
	if (!mSignal)
	{
		llwarns << "Cannot connect listener to: "
				<< (name.empty() ? std::string("unnamed") : name) << " event."
				<< llendl;
		return LLBoundListener();
	}

	LLMutexLock lock(&mConnectionListMutex);

	float node_position = 1.f;

	// If the supplied name is empty we are not interested in the ordering
	// mechanism and can bypass attempting to find the optimal location to
	// insert the new listener. We will just tack it on to the end.
	bool named = !name.empty();
	if (named)
	{
		// Check for duplicate name before connecting listener to mSignal
		connect_map_t::const_iterator it = mConnections.find(name);

		// In some cases the user might disconnect a connection explicitly or
		// might use LLEventTrackable to disconnect implicitly. Either way, we
		// can end up retaining in mConnections a zombie connection object
		// that has already been disconnected. Such a connection object cannot
		// be reconnected nor, in the case of LLEventTrackable, would we want
		// to try, since disconnection happens with the destruction of the
		// listener object. That means it is safe to overwrite a disconnected
		// connection object with the new one we are attempting. The case we
		// want to prevent is only when the existing connection object is still
		// connected.
		if (it != mConnections.end() && it->second.connected())
		{
			throw DupListenerName("Attempt to register duplicate listener name '" +
								  name + "' on " + typeid(*this).name() +
								  " '" + getName() + "'");
		}

		// Okay, name is unique, try to reconcile its dependencies. Specify a
		// new "node" value that we never use for an mSignal placement; we will
		// fix it later.
		depend_map_t::node_type& new_node = mDeps.add(name, -1.f, after,
													  before);

		// What if this listener has been added, removed and re-added ?  In
		// that case new_node already has a non-negative value because we never
		// remove a listener from mDeps. But keep processing uniformly anyway
		// in case the listener was added back with different dependencies.
		// Then mDeps.sort() would put it in a different position, and the old
		// new_node placement value would be wrong, so we would have to
		// reassign it anyway. Trust that re-adding a listener with the same
		// dependencies is the trivial case for mDeps.sort(): it can just
		// replay its cache.
		depend_map_t::sorted_range sorted_range;
		try
		{
			// Can we pick an order that works including this new entry ?
			sorted_range = mDeps.sort();
		}
		catch (const depend_map_t::Cycle& e)
		{
			// No: the new node's after/before dependencies have made mDeps
			// unsortable. If we leave the new node in mDeps, it will continue
			// to screw up all future attempts to sort()! Pull it out.
			mDeps.remove(name);
			throw Cycle("New listener '" + name + "' on " +
						typeid(*this).name() + " '" + getName() +
						"' would cause cycle: " + e.what());
		}

		// Walk the list to verify that we have not changed the order.
		float previous = 0.f, myprev = 0.f;
		// need this visible after loop:
		depend_map_t::sorted_iterator mydmi = sorted_range.end();
		for (depend_map_t::sorted_iterator dmi = sorted_range.begin();
			 dmi != sorted_range.end(); ++dmi)
		{
			// Since we have added the new entry with an invalid placement,
			// recognize it and skip it.
			if (dmi->first == name)
			{
				// Remember the iterator belonging to our new node, and which
				// placement value was 'previous' at that point.
				mydmi = dmi;
				myprev = previous;
				continue;
			}

			// If the new node has rearranged the existing nodes, we will find
			// that their placement values are no longer in increasing order.
			if (dmi->second < previous)
			{
				// This is another scenario in which we'd better back out the
				// newly-added node from mDeps, but do not do it yet, we want
				// to traverse the existing mDeps to report on it !
				// Describe the change to the order of our listeners. Copy
				// everything but the newest listener to a vector we can sort
				// to obtain the old order.
				typedef std::vector<std::pair<float, std::string> > SortNameList;
				SortNameList sortnames;
				for (depend_map_t::sorted_iterator cdmi = sorted_range.begin(),
												   cdmend = sorted_range.end();
					 cdmi != cdmend; ++cdmi)
				{
					if (cdmi->first != name)
					{
						sortnames.push_back(SortNameList::value_type(cdmi->second,
																	 cdmi->first));
					}
				}
				std::sort(sortnames.begin(), sortnames.end());
				std::ostringstream out;
				out << "New listener '" << name << "' on "
					<< typeid(*this).name() << " '" << getName()
					<< "' would move previous listener '" << dmi->first
					<< "'\nwas: ";
				SortNameList::const_iterator sni(sortnames.begin()),
											 snend(sortnames.end());
				if (sni != snend)
				{
					out << sni->second;
					while (++sni != snend)
					{
						out << ", " << sni->second;
					}
				}
				out << "\nnow: ";
				depend_map_t::sorted_iterator ddmi(sorted_range.begin());
				depend_map_t::sorted_iterator ddmend(sorted_range.end());
				if (ddmi != ddmend)
				{
					out << ddmi->first;
					while (++ddmi != ddmend)
					{
						out << ", " << ddmi->first;
					}
				}
				// NOW remove the offending listener node.
				mDeps.remove(name);
				// Having constructed a description of the order change, inform
				// caller.
				throw OrderChange(out.str());
			}
			// This node becomes the previous one.
			previous = dmi->second;
		}
		// We just got done with a successful mDeps.add(name, ...) call. We had
		// better have found 'name' somewhere in that sorted list !
		assert(mydmi != sorted_range.end());
		// Four cases:
		// 0. name is the only entry: placement 1.0
		// 1. name is the first of several entries: placement (next placement)/2
		// 2. name is between two other entries: placement (myprev + (next placement))/2
		// 3. name is the last entry: placement ceil(myprev) + 1.0
		// Since we have cleverly arranged for myprev to be 0.0 if name is the
		// first entry, this folds down to two cases. Case 1 is subsumed by
		// case 2, and case 0 is subsumed by case 3. So we need only handle
		// cases 2 and 3, which means we need only detect whether name is the
		// last entry. Increment mydmi to see if there's anything beyond.
		if (++mydmi != sorted_range.end())
		{
			// The new node is not last. Place it between the previous node and
			// the successor.
			new_node = (myprev + mydmi->second) * 0.5f;
		}
		else
		{
			// The new node is last. Bump myprev up to the next integer, add
			// 1.0 and use that.
			new_node = std::ceil(myprev) + 1.f;
		}
		node_position = new_node;
	}

	// Now that new_node has a value that places it appropriately in mSignal,
	// connect it.
	LLBoundListener bound = mSignal->connect(node_position, listener);

	if (named)
	{
		// Note that we are not tracking anonymous listeners here either.
		// This means that it is the caller's responsibility to either assign
		// to a TempBoundListerer (scoped_connection) or manually disconnect
		// when done.
		mConnections[name] = bound;
	}

	return bound;
}

LLBoundListener LLEventPump::getListener(const std::string& name) const
{
	LLMutexLock lock(&mConnectionListMutex);

	connect_map_t::const_iterator it = mConnections.find(name);
	if (it != mConnections.end())
	{
		return it->second;
	}
	// Not found, return dummy LLBoundListener
	return LLBoundListener();
}

void LLEventPump::stopListening(const std::string& name)
{
	mConnectionListMutex.lock();

	connect_map_t::iterator it = mConnections.find(name);
	if (it != mConnections.end())
	{
		it->second.disconnect();
		mConnections.erase(it);
	}
	// We intentionally do NOT remove this name from mDeps. It may happen that
	// the same listener with the same name and dependencies will jump on and
	// off this LLEventPump repeatedly. Keeping a cache of dependencies will
	// avoid a new dependency sort in such cases.

	mConnectionListMutex.unlock();
}

void LLEventPump::removeFromDeps(const std::string& name)
{
	mDeps.remove(name);
	mDeps.clearCache();
}

/*****************************************************************************
*   LLEventStream
*****************************************************************************/
bool LLEventStream::post(const LLSD& event)
{
	if (!mEnabled || !mSignal)
	{
		return false;
	}
	// NOTE NOTE NOTE: Any new access to member data beyond this point should
	// cause us to move our LLStandardSignal object to a pimpl class along
	// with said member data. Then the local shared_ptr will preserve both.

	// DEV-43463: capture a local copy of mSignal. We have turned up a cross-
	// coroutine scenario (described in the JIRA) in which this post() call
	// could end up destroying 'this', the LLEventPump subclass instance
	// containing mSignal, during the call through *mSignal. So capture a
	// *stack* instance of the shared_ptr, ensuring that our heap
	// LLStandardSignal object will live at least until post() returns, even
	// if 'this' gets destroyed during the call.
	std::shared_ptr<LLStandardSignal> signal(mSignal);

	// Let caller know if any one listener handled the event. This is mostly
	// useful when using LLEventStream as a listener for an upstream
	// LLEventPump.
	bool res = false;
	if (signal)
	{
		try
		{
			res = (*signal)(event);
		}
		catch (boost::fibers::fiber_error& e)
		{
			// Ignore these
			llwarns << "Event stream: " << getName() << " - Caught exception: "
					<< e.what() << llendl;
		}
	}
	return res;
}

/*****************************************************************************
*   LLListenerOrPumpName
*****************************************************************************/
LLListenerOrPumpName::LLListenerOrPumpName(const std::string& pumpname)
	// Look up the specified pumpname, and bind its post() method as our
	// listener
:	mListener(boost::bind(&LLEventPump::post,
						  boost::ref(gEventPumps.obtain(pumpname)), _1))
{
}

LLListenerOrPumpName::LLListenerOrPumpName(const char* pumpname)
	// Look up the specified pumpname, and bind its post() method as our
	// listener
:	mListener(boost::bind(&LLEventPump::post,
						  boost::ref(gEventPumps.obtain(pumpname)), _1))
{
}

bool LLListenerOrPumpName::operator()(const LLSD& event) const
{
	if (!mListener)
	{
		throw Empty("attempting to call uninitialized");
	}
	return (*mListener)(event);
}

void LLReqID::stamp(LLSD& response) const
{
	if (!(response.isUndefined() || response.isMap()))
	{
		// If 'response' was previously completely empty, it is OK to turn it
		// into a map. If it was already a map, then it should be OK to add a
		// key. But if it was anything else (e.g. a scalar), assigning a
		// ["reqid"] key will DISCARD the previous value, replacing it with a
		// map. That would be bad.
		llinfos << "stamp(" << mReqid
				<< ") leaving non-map response unmodified: " << response
				<< llendl;
		return;
	}
	LLSD old_reqid(response["reqid"]);
	if (!(old_reqid.isUndefined() || llsd_equals(old_reqid, mReqid)))
	{
		llinfos << "stamp(" << mReqid
				<< ") preserving existing [\"reqid\"] value " << old_reqid
				<< " in response: " << response << llendl;
		return;
	}
	response["reqid"] = mReqid;
}
