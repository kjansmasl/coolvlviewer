/**
 * @file   llevents.h
 * @author Kent Quirk, Nat Goodspeed
 * @date   2008-09-11
 * @brief  This is an implementation of the event system described at
 *		 https://wiki.lindenlab.com/wiki/Viewer:Messaging/Event_System,
 *		 originally introduced in llnotifications.h. It has nothing
 *		 whatsoever to do with the older system in llevent.h.
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

#ifndef LL_LLEVENTS_H
#define LL_LLEVENTS_H

#include "llpreprocessor.h"

#include <deque>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "boost/optional/optional.hpp"
#include "boost/signals2.hpp"

#include "lldependencies.h"
#include "llmutex.h"
#include "llsd.h"
#include "llstl.h"

// Signal and handler declarations.
// Using a single handler signature means that we can have a common handler
// type, rather than needing a distinct one for each different handler.

// A boost::signals Combiner that stops the first time a handler returns true
// We need this because we want to have our handlers return bool, so that
// we have the option to cause a handler to stop further processing. The
// default handler fails when the signal returns a value but has no slots.
struct LLStopWhenHandled
{
	typedef bool result_type;

	template<typename InputIterator>
	result_type operator()(InputIterator first, InputIterator last) const
	{
		for (InputIterator si = first; si != last; ++si)
		{
			try
			{
				if (*si)
				{
					return true;
				}
			}
			catch (...)
			{
			}
		}
		return false;
	}
};

// We want to have a standard signature for all signals; this way, we can
// easily document a protocol for communicating across DLLs and into scripting
// languages someday.
//
// We want to return a bool to indicate whether the signal has been handled and
// should NOT be passed on to other listeners. Returns true to stop further
// handling of the signal, and false to continue.
//
// We take an LLSD because this way the contents of the signal are independent
// of the API used to communicate it. It is const ref because then there's low
// cost to pass it; if you only need to inspect it, it's very cheap.
//
// NOTE: the float template parameter indicates that we will internally use
// float to indicate relative listener order on a given LLStandardSignal. Do
// not worry, the float values are strictly internal !  They are not part of
// the interface, for the excellent reason that requiring the caller to specify
// a numeric key to establish order means that the caller must know the
// universe of possible values. We use LLDependencies for that instead.
typedef boost::signals2::signal<bool(const LLSD&), LLStopWhenHandled,
								float> LLStandardSignal;

// Methods that forward listeners (e.g. constructed with boost::bind()) should
// accept (const LLEventListener&)
typedef LLStandardSignal::slot_type LLEventListener;

// Result of registering a listener, supports connected(), disconnect() and
// blocked()
typedef boost::signals2::connection LLBoundListener;

// Storing an LLBoundListener in LLTempBoundListener will disconnect the
// referenced listener when the LLTempBoundListener instance is destroyed.
typedef boost::signals2::scoped_connection LLTempBoundListener;

// A common idiom for event-based code is to accept either a callable, directly
// called on completion, or the string name of an LLEventPump on which to post
// the completion event. Specifying a parameter as const LLListenerOrPumpName&
// allows either.
//
// Calling a validly-constructed LLListenerOrPumpName, passing the LLSD 'event'
// object, either calls the callable or posts the event to the named
// LLEventPump.
//
// A default-constructed LLListenerOrPumpName is 'empty'. (This is useful as
// the default value of an optional method parameter.) Calling it throws
// LLListenerOrPumpName::Empty. Test for this condition beforehand using either
// if (param) or if (! param).
class LLListenerOrPumpName
{
public:
	// Passing string name of LLEventPump
	LLListenerOrPumpName(const std::string& pumpname);

	// Passing string literal (overload so compiler isn't forced to infer
	// double conversion)
	LLListenerOrPumpName(const char* pumpname);

	// Passing listener: the "anything else" catch-all case. The type of an
	// object constructed by boost::bind() is not intended to be written out.
	// Normally we would just accept 'const LLEventListener&', but that would
	// require double implicit conversion: boost::bind() object to
	// LLEventListener, LLEventListener to LLListenerOrPumpName. So use a
	// template to forward anything.
	template<typename T>
	LLListenerOrPumpName(const T& listener)
	:	mListener(listener)
	{
	}

	// For omitted method parameter: uninitialized mListener
	LLListenerOrPumpName() = default;

	// Tests for validity
	LL_INLINE operator bool() const				{ return bool(mListener); }
	LL_INLINE bool operator!() const			{ return !mListener; }

	// Explicit accessor
	LL_INLINE const LLEventListener& getListener() const
	{
		return *mListener;
	}

	// Implicit conversion to LLEventListener
	LL_INLINE operator LLEventListener() const	{ return *mListener; }

	// Allows calling directly
	bool operator()(const LLSD& event) const;

	// Exception if you try to call when empty
	struct Empty : public std::runtime_error
	{
		Empty(const std::string& what)
		:	std::runtime_error(std::string("LLListenerOrPumpName::Empty: ") +
							   what)
		{
		}
	};

private:
	boost::optional<LLEventListener> mListener;
};

///////////////////////////////////////////////////////////////////////////////
// LLEventPumps
///////////////////////////////////////////////////////////////////////////////

class LLEventPump;

class LLEventPumps final
{
	friend class LLEventPump;

protected:
	LOG_CLASS(LLEventPumps);

public:
	LLEventPumps() = default;
	~LLEventPumps();

	// Finds or creates an LLEventPump instance with a specific name. We return
	// a reference so there is no question about ownership. obtain() finds
	// an instance without conferring ownership.
	LLEventPump& obtain(const std::string& name);

	// Creates an LLEventPump instance with suggested name. We return
	// a reference so there is no question about ownership. When type is
	// invalid, BadType is thrown.
	// Note: type only accepts an empty string or "LLEventStream", i.e. there
	// is not support for the unimplemented "LLEventMailDrop" type. HB
	LLEventPump& make(const std::string& name, bool tweak = false,
					  const std::string& type = std::string());

	struct BadType : public std::runtime_error
	{
		BadType(const std::string& what)
		:	std::runtime_error(std::string("BadType: ") + what)
		{
		}
	};

	// Finds the named LLEventPump instance. If it exists post the message to
	// it. If the pump does not exist, do nothing. Returns the result of the
	// LLEventPump::post. If no pump exists returns false.
	// This is syntactically similar to: gEventPumps.post(name, message)
	// however if the pump does not already exist it will not be created.
	bool post(const std::string& name, const LLSD& message);

	// Flushes all known LLEventPump instances
	void flush();

	// Disconnects listeners from all known LLEventPump instances
	void clear();

	// Resets all known LLEventPump instances (workaround for DEV-35406 crash
	// on shutdown).
	void reset();

	LL_INLINE static bool destroyed()		{ return sInstanceDestroyed; }

	// Conventionally sends a reply to a request event.
	//
	// 'reply' is the LLSD reply event to send
	// 'request' is the corresponding LLSD request event
	// 'reply_key' is the key in the @a request event, conventionally ["reply"],
	// whose value is the name of the LLEventPump on which to send the reply.
	//
	// Before sending the reply event, sendReply() copies the ["reqid"] item
	// from the request to the reply.
	bool sendReply(const LLSD& reply, const LLSD& request,
				   const std::string& reply_key = "reply");

private:
	// Registers a new LLEventPump instance (internal)
	std::string registerNew(const LLEventPump&, const std::string& name,
							bool tweak);
	// Unregisters a doomed LLEventPump instance (internal)
	void unregister(const LLEventPump&);

private:
	// Map of all known LLEventPump instances, whether or not we instantiated
	// them. We store a plain old LLEventPump* because this map does not claim
	// ownership of the instances. Though the common usage pattern is to
	// request an instance using obtain(), it is fair to instantiate an
	// LLEventPump subclass statically, as a class member, on the stack or on
	// the heap. In such cases, the instantiating party is responsible for its
	// lifespan.
	typedef std::map<std::string, LLEventPump*> PumpMap;
	PumpMap		mPumpMap;

	// Set of all LLEventPumps we instantiated. Membership in this set means
	// we claim ownership, and will delete them when this LLEventPumps is
	// destroyed.
	typedef std::set<LLEventPump*> PumpSet;
	PumpSet		mOurPumps;

	// Flag to indicate that the gEventPumps instance got destroyed already.
	static bool	sInstanceDestroyed;
};

extern LLEventPumps gEventPumps;

///////////////////////////////////////////////////////////////////////////////
//  LLEventTrackable
///////////////////////////////////////////////////////////////////////////////

// LLEventTrackable wraps boost::signals2::trackable, which resembles
// boost::trackable. Derive your listener class from LLEventTrackable instead,
// and use something like:
//   LLEventPump::listen(boost::bind(&YourTrackableSubclass::method, instance,
//									 _1)).
// This will implicitly disconnect when the object referenced by instance is
// destroyed.
//
// NOTE: LLEventTrackable does not address a couple of cases:
//	- Object destroyed during call
//	- You enter a slot call in thread A.
//	- Thread B destroys the object, which of course disconnects it from any
//	  future slot calls.
//	- Thread A's call uses 'this', which now refers to a defunct object.
//	  Undefined behavior results.
//	- Call during destruction
//		- MySubclass is derived from LLEventTrackable.
//		- MySubclass registers one of its own methods using
//		  LLEventPump::listen().
//		- The MySubclass object begins destruction. ~MySubclass() runs,
//		  destroying state specific to the subclass (for instance, a Foo* data
//		  member is deleted but not zeroed).
//		- The listening method will not be disconnected until
//		  ~LLEventTrackable() runs.
//		- Before we get there, another thread posts data to the LLEventPump
//		  instance, calling the MySubclass method.
//		- The method in question relies on valid MySubclass state (for
//		  instance, it attempts to dereference the Foo* pointer that was
//		  deleted but not zeroed).
//		- Undefined behavior results.
typedef boost::signals2::trackable LLEventTrackable;

///////////////////////////////////////////////////////////////////////////////
// LLEventPump
///////////////////////////////////////////////////////////////////////////////

// LLEventPump is the base class interface through which we access the concrete
// subclass LLEventStream.
//
// NOTE: LLEventPump derives from LLEventTrackable so that when you "chain"
// LLEventPump instances together, they will automatically disconnect on
// destruction. Please see LLEventTrackable documentation for situations in
// which this may be perilous across threads.
class LLEventPump : public LLEventTrackable
{
	friend class LLEventPumps;

protected:
	LOG_CLASS(LLEventPump);

public:
	static const std::string ANONYMOUS; // constant for anonymous listeners.

	// Exception thrown by LLEventPump(). You are trying to instantiate an
	// LLEventPump (subclass) using the same name as some other instance, and
	// you did not pass tweak=true to permit it to generate a unique variant.
	struct DupPumpName : public std::runtime_error
	{
		DupPumpName(const std::string& what)
		:	std::runtime_error(std::string("DupPumpName: ") + what)
		{
		}
	};

	// Instantiates an LLEventPump (subclass) with the string name by which it
	// can be found using LLEventPumps::obtain().
	//
	// If you pass (or default) tweak to false, then a duplicate name will
	// throw DupPumpName. This would not happen if LLEventPumps::obtain()
	// instantiates the LLEventPump, because obtain() uses find-or-create
	// logic. It can only happen if you instantiate an LLEventPump in your own
	// code, and a collision with the name of some other LLEventPump is likely
	// to cause much more subtle problems !
	//
	// When you hand-instantiate an LLEventPump, consider passing tweak as
	// true. This directs LLEventPump() to append a suffix to the passed name
	// to make it unique. You can retrieve the adjusted name by calling
	// getName() on your new instance.
	LLEventPump(const std::string& name, bool tweak = false);
	virtual ~LLEventPump();

	// Group exceptions thrown by listen(). We use exceptions because these
	// particular errors are likely to be coding errors, found and fixed by
	// the developer even before preliminary checkin.
	struct ListenError : public std::runtime_error
	{
		ListenError(const std::string& what)
		:	std::runtime_error(what)
		{
		}
	};

	// Exception thrown by listen(). You are attempting to register a listener
	// on this LLEventPump using the same listener name as an already
	// registered listener.
	struct DupListenerName : public ListenError
	{
		DupListenerName(const std::string& what)
		:	ListenError(std::string("DupListenerName: ") + what)
		{
		}
	};

	// Exception thrown by listen(). The order dependencies specified for your
	// listener are incompatible with existing listeners.
	//
	// Consider listener "a" which specifies before "b" and "b" which specifies
	// before "c". You are now attempting to register "c" before "a". There is
	// no order that can satisfy all constraints.
	struct Cycle : public ListenError
	{
		Cycle(const std::string& what)
		:	ListenError(std::string("Cycle: ") + what)
		{
		}
	};

	// Exception thrown by listen(). This one means that your new listener
	// would force a change to the order of previously-registered listeners,
	// and we do not have a good way to implement that.
	//
	// Consider listeners "some", "other" and "third". "some" and "other" are
	// registered earlier without specifying relative order, so "other" happens
	// to be first. Now you attempt to register "third" after "some" and before
	// "other". Whoops, that would require swapping "some" and "other", which
	// we cannot do. Instead we throw this exception.
	//
	// It may not be possible to change the registration order so we already
	// know "third"s order requirement by the time we register the second of
	// "some" and "other". A solution would be to specify that "some" must come
	// before "other", or equivalently that "other" must come after "some".
	struct OrderChange : public ListenError
	{
		OrderChange(const std::string& what)
		:	ListenError(std::string("OrderChange: ") + what)
		{
		}
	};

	// Used by listen()
	typedef std::vector<std::string> NameList;
	// Convenience placeholder for when you explicitly want to pass an empty
	// NameList
	const static NameList empty;

	// Get this LLEventPump's name
	LL_INLINE std::string getName() const		{ return mName; }

	// Register a new listener with a unique name. Specify an optional list of
	// other listener names after which this one must be called, likewise an
	// optional list of other listener names before which this one must be
	// called. The other listeners mentioned need not yet be registered
	// themselves. listen() can throw any ListenError; see ListenError sub-
	// classes.
	//
	// The listener name must be unique among active listeners for this
	// LLEventPump, else you get DupListenerName. If you do not care to invent
	// a name yourself, use inventName(). I was tempted to recognize e.g. ""
	// and internally generate a distinct name for that case. But that would
	// handle badly the scenario in which you want to add, remove, re-add,
	// etc. the same listener: each new listen() call would necessarily
	// perform a new dependency sort. Assuming you specify the same after/
	// before lists each time, using inventName() when you first instantiate
	// your listener, then passing the same name on each listen() call, allows
	// us to optimize away the second and subsequent dependency sorts.
	//
	// If name is set to LLEventPump::ANONYMOUS listen will bypass the entire
	// dependency and ordering calculation. In this case, it is critical that
	// the result be assigned to a LLTempBoundListener or the listener is
	// manually disconnected when no longer needed since there will be no way
	// to later find and disconnect this listener manually.
	LLBoundListener listen(const std::string& name,
						   const LLEventListener& listener,
						   const NameList& after = NameList(),
						   const NameList& before = NameList())
	{
		return listen_impl(name, listener, after, before);
	}

	// Get the LLBoundListener associated with the passed name (dummy
	// LLBoundListener if not found)
	virtual LLBoundListener getListener(const std::string& name) const;

	// Instantiate one of these to block an existing connection:
	// { // in some local scope
	//	 LLEventPump::Blocker block(someLLBoundListener);
	//	 // code that needs the connection blocked
	// } // unblock the connection again
	typedef boost::signals2::shared_connection_block Blocker;

	// Unregisters a listener by name. Prefer this to
	// getListener(name).disconnect() because stopListening() also forgets
	// this name.
	virtual void stopListening(const std::string& name);

	void removeFromDeps(const std::string& name);

	// Post an event to all listeners. The bool return is only meaningful
	// if the underlying leaf class is LLEventStream: beware of relying on
	// it too much !  Truthfully, we return bool mostly to permit chaining
	// one LLEventPump as a listener on another.
	virtual bool post(const LLSD&) = 0;

	// Enable/disable: while disabled, silently ignore all post() calls
	LL_INLINE virtual void enable(bool enabled = true)
	{
		mEnabled = enabled;
	}

	// Query
	LL_INLINE virtual bool enabled() const		{ return mEnabled; }

	// Generate a distinct name for a listener -- see listen()
	static std::string inventName(const std::string& pfx = "listener");

protected:
	virtual LLBoundListener listen_impl(const std::string& name,
										const LLEventListener&,
										const NameList& after,
										const NameList& before);
private:
	// Flushes queued events
	LL_INLINE virtual void flush()				{}
	virtual void clear();
	virtual void reset();

private:
	mutable LLMutex						mConnectionListMutex;
	std::string							mName;

protected:
	// Implements the dispatching
	std::shared_ptr<LLStandardSignal>	mSignal;

	// Map of named listeners. This tracks the listeners that actually exist
	// at this moment. When we stopListening(), we discard the entry from
	// this map.
	typedef std::map<std::string, boost::signals2::connection> connect_map_t;
	connect_map_t						mConnections;

	// Dependencies between listeners. For each listener, track the float
	// used to establish its place in mSignal's order. This caches all the
	// listeners that have ever registered; stopListening() does not discard
	// the entry from this map. This is to avoid a new dependency sort if the
	// same listener with the same dependencies keeps hopping on and off this
	// LLEventPump.
	typedef LLDependencies<std::string, float> depend_map_t;
	depend_map_t						mDeps;

	// Valve open ?
	bool								mEnabled;
};

///////////////////////////////////////////////////////////////////////////////
// LLEventStream
///////////////////////////////////////////////////////////////////////////////

// LLEventStream is a thin wrapper around LLStandardSignal. Posting an event
// immediately calls all registered listeners.
class LLEventStream : public LLEventPump
{
protected:
	LOG_CLASS(LLEventStream);

public:
	LLEventStream(const std::string& name, bool tweak = false)
	:	LLEventPump(name, tweak)
	{
	}

	// Posts an event to all listeners
	virtual bool post(const LLSD& event);
};

///////////////////////////////////////////////////////////////////////////////
// LLReqID
///////////////////////////////////////////////////////////////////////////////

// This class helps the implementer of a given event API to honor the ["reqid"]
// convention. By this convention, each event API stamps into its response LLSD
// a ["reqid"] key whose value echoes the ["reqid"] value, if any, from the
// corresponding request.
//
// This supports an (atypical, but occasionally necessary) use case in which
// two or more asynchronous requests are multiplexed onto the same ["reply"]
// LLEventPump. Since the response events could arrive in arbitrary order, the
// caller must be able to demux them. It does so by matching the ["reqid"]
// value in each response with the ["reqid"] value in the corresponding
// request.
//
// It is the caller's responsibility to ensure distinct ["reqid"] values for
// that case. Though LLSD::UUID is guaranteed to work, it might be overkill:
// the "namespace" of unique ["reqid"] values is simply the set of requests
// specifying the same ["reply"] LLEventPump name.
//
// Making a given event API echo the request's ["reqid"] into the response is
// nearly trivial. This helper is mostly for mnemonic purposes, to serve as a
// place to put these comments. We hope that each time a coder implements a
// new event API based on some existing one, they will say, "Huh, what is an
// LLReqID ?" and look up this material.
//
// The hardest part about the convention is deciding where to store the
// ["reqid"] value. Ironically, LLReqID cannot help with that: you must store
// an LLReqID instance in whatever storage will persist until the reply is
// sent. For example, if the request ultimately ends up using a Responder
// subclass, storing an LLReqID instance in the Responder works.
//
// NOTE: the implementer of an event API must honor the ["reqid"] convention.
// However, the caller of an event API need only use it if s/he is sharing the
// same ["reply"] LLEventPump for two or more asynchronous event API requests.
//
// In most cases, it is far easier for the caller to instantiate a local
// LLEventStream and pass its name to the event API in question. Then it is
// perfectly reasonable not to set a ["reqid"] key in the request, ignoring
// the isUndefined() ["reqid"] value in the response.
class LLReqID
{
protected:
	LOG_CLASS(LLReqID);

public:
	// If you have the request in hand at the time you instantiate the LLReqID,
	// pass that request to extract its ["reqid"].
	LLReqID(const LLSD& request)
	:	mReqid(request["reqid"])
	{
	}

	// If you do not yet have the request, use setFrom() later.
	LLReqID() = default;

	// Extracts and stores the ["reqid"] value from an incoming request.
	LL_INLINE void setFrom(const LLSD& req)		{ mReqid = req["reqid"]; }

	// Sets ["reqid"] key into a pending response LLSD object.
	void stamp(LLSD& response) const;

	// Makes a whole new response LLSD object with our ["reqid"].
	LL_INLINE LLSD makeResponse() const
	{
		LLSD response;
		stamp(response);
		return response;
	}

	// Not really sure of a use case for this accessor...
	LL_INLINE LLSD getReqID() const				{ return mReqid; }

private:
	LLSD mReqid;
};

#endif // LL_LLEVENTS_H
