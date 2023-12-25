/**
 * @file   lleventdispatcher.cpp
 * @author Nat Goodspeed
 * @date   2009-06-18
 * @brief  Implementation for lleventdispatcher.
 * 
 * $LicenseInfo:firstyear=2009&license=viewergpl$
 * 
 * Copyright (c) 2010-2022, Linden Research, Inc.
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

#if LL_WINDOWS
// 'this' used in initializer list: yes, intentionally
# pragma warning (disable : 4355)
#endif

#include <sstream>

#include "linden_common.h"

#include "lleventdispatcher.h"

///////////////////////////////////////////////////////////////////////////////
//  DispatchError
///////////////////////////////////////////////////////////////////////////////
struct DispatchError : public std::runtime_error
{
	LL_INLINE DispatchError(const std::string& what)
	:	std::runtime_error(what)
	{
	}

	LL_INLINE DispatchError(const char* what)
	:	std::runtime_error(what)
	{
	}
};

///////////////////////////////////////////////////////////////////////////////
//  LLSDArgsSource
///////////////////////////////////////////////////////////////////////////////

// Stores an LLSD array, producing its elements one at a time. Throws if the
// consumer requests more elements than the array contains.

class LLSDArgsSource
{
protected:
	LOG_CLASS(LLSDArgsSource);

public:
	LLSDArgsSource(const std::string function, const LLSD& args);
	~LLSDArgsSource();

	LLSD next();
	void done() const;

private:
	std::string	mFunction;
	LLSD		mArgs;
	size_t		mIndex;
};

LLSDArgsSource::LLSDArgsSource(const std::string function, const LLSD& args)
:	mFunction(function),
	mArgs(args),
	mIndex(0)
{
	if (!(mArgs.isUndefined() || mArgs.isArray()))
	{
		std::ostringstream s;
		s << mFunction << " needs an args array instead of " << mArgs;
		llwarns << s.str() << llendl;
		throw DispatchError(s.str());
	}
}

LLSDArgsSource::~LLSDArgsSource()
{
	done();
}

LLSD LLSDArgsSource::next()
{
	if (mIndex >= mArgs.size())
	{
		std::ostringstream s;
		s << mFunction << " requires more arguments than the "
		  << mArgs.size() << " provided: " << mArgs;
		llwarns << s.str() << llendl;
		throw DispatchError(s.str());
	}
	return mArgs[mIndex++];
}

void LLSDArgsSource::done() const
{
	if (mIndex < mArgs.size())
	{
		std::ostringstream s;
		s << mFunction << " only consumed " << mIndex << " of the "
		  << mArgs.size()  << " arguments provided: " << mArgs;
		llwarns << s.str() << llendl;
		throw DispatchError(s.str());
	}
}

///////////////////////////////////////////////////////////////////////////////
//   LLSDArgsMapper
///////////////////////////////////////////////////////////////////////////////

// From a formal parameters description and a map of arguments, construct an
// arguments array.
//
// That is, given:
// - an LLSD array of length n containing parameter-name strings,
//   corresponding to the arguments of a function of interest
// - an LLSD collection specifying default parameter values, either:
//   - an LLSD array of length m <= n, matching the rightmost m params, or
//   - an LLSD map explicitly stating default name=value pairs
// - an LLSD map of parameter names and actual values for a particular
//   function call
// construct an LLSD array of actual argument values for this function call.
//
// The parameter-names array and the defaults collection describe the function
// being called. The map might vary with every call, providing argument values
// for the described parameters.
//
// The array of parameter names must match the number of parameters expected
// by the function of interest.
//
// If you pass a map of default parameter values, it provides default values
// as you might expect. It is an error to specify a default value for a name
// not listed in the parameters array.
//
// If you pass an array of default parameter values, it is mapped to the
// rightmost m of the n parameter names. It is an error if the default-values
// array is longer than the parameter-names array. Consider the following
// parameter names: ["a", "b", "c", "d"].
//
// - An empty array of default values (or an isUndefined() value) asserts that
//   every one of the above parameter names is required.
// - An array of four default values [1, 2, 3, 4] asserts that every one of
//   the above parameters is optional. If the current parameter map is empty,
//   they will be passed to the function as [1, 2, 3, 4].
// - An array of two default values [11, 12] asserts that parameters "a" and
//   "b" are required, while "c" and "d" are optional, having default values
//   "c"=11 and "d"=12.
//
// The arguments array is constructed as follows:
//
// - Arguments-map keys not found in the parameter-names array are ignored.
// - Entries from the map provide values for an improper subset of the
//   parameters named in the parameter-names array. This results in a
//   tentative values array with "holes." (size of map) + (number of holes) =
//   (size of names array)
// - Holes are filled with the default values.
// - Any remaining holes constitute an error.

class LLSDArgsMapper
{
protected:
	LOG_CLASS(LLSDArgsMapper);

public:
	// Accept description of function: function name, param names, param
	// default values
	LLSDArgsMapper(const std::string& function, const LLSD& names,
				   const LLSD& defaults);

	// Given arguments map, return LLSD::Array of parameter values, or throws.
	LLSD map(const LLSD& argsmap) const;

private:
	static std::string formatlist(const LLSD&);

private:
	// The function-name string is purely descriptive. We want error messages
	// to be able to indicate which function's LLSDArgsMapper has the problem.
	std::string		mFunction;
	// Store the names array pretty much as given.
	LLSD			mNames;
	// Though we are handed an array of name strings, it is more useful to us
	// to store it as a map from name string to position index. Of course that
	// is easy to generate from the incoming names array, but why do it more
	// than once ?
	typedef std::map<LLSD::String, size_t> idx_map_t;
	idx_map_t		mIndexes;
	// Generated array of default values, aligned with the array of param names.
	LLSD mDefaults;
	// Indicate whether we have a default value for each param.
	typedef std::vector<char> default_vec_t;
	default_vec_t	mHasDefault;
};

LLSDArgsMapper::LLSDArgsMapper(const std::string& function,
							   const LLSD& names, const LLSD& defaults)
:	mFunction(function),
	mNames(names),
	mHasDefault(names.size())
{
	if (!(mNames.isUndefined() || mNames.isArray()))
	{
		std::ostringstream s;
		s << function << " names must be an array, not " << names;
		llwarns << s.str() << llendl;
		throw DispatchError(s.str());
	}
	size_t nparams = mNames.size();
	// From mNames generate mIndexes.
	for (size_t ni = 0, nend = mNames.size(); ni < nend; ++ni)
	{
		mIndexes[mNames[ni]] = ni;
	}

	// Presize mDefaults() array so we don't have to resize it more than once.
	// All entries are initialized to LLSD(); but since mHasDefault is still
	// all 0, they are all "holes" for now.
	if (nparams)
	{
		mDefaults[nparams - 1] = LLSD();
	}

	if (defaults.isUndefined() || defaults.isArray())
	{
		size_t ndefaults = defaults.size();
		// defaults is a (possibly empty) array. Right-align it with names.
		if (ndefaults > nparams)
		{
			std::ostringstream s;
			s << function << " names array " << names
			  << " shorter than defaults array " << defaults;
			llwarns << s.str() << llendl;
			throw DispatchError(s.str());
		}

		// Offset by which we slide defaults array right to right-align with
		// mNames array
		size_t offset = nparams - ndefaults;
		// Fill rightmost mDefaults entries from defaults, and mark them as
		// filled
		for (size_t i = 0, iend = ndefaults; i < iend; ++i)
		{
			mDefaults[i + offset] = defaults[i];
			mHasDefault[i + offset] = 1;
		}
	}
	else if (defaults.isMap())
	{
		// 'defaults' is a map. Use it to populate the mDefaults array.
		LLSD bogus;
		for (LLSD::map_const_iterator it = defaults.beginMap(),
									  end = defaults.endMap();
			 it != end; ++it)
		{
			idx_map_t::const_iterator ixit = mIndexes.find(it->first);
			if (ixit == mIndexes.end())
			{
				bogus.append(it->first);
				continue;
			}

			size_t pos = ixit->second;
			// Store default value at that position in the mDefaults array.
			mDefaults[pos] = it->second;
			// Do not forget to record the fact that we have filled this
			// position.
			mHasDefault[pos] = 1;
		}
		if (bogus.size())
		{
			std::ostringstream s;
			s << function << " defaults specified for nonexistent params "
			  << formatlist(bogus);
			llwarns << s.str() << llendl;
			throw DispatchError(s.str());
		}
	}
	else
	{
		std::ostringstream s;
		s << function << " defaults must be a map or an array, not "
		  << defaults;
		llwarns << s.str() << llendl;
		throw DispatchError(s.str());
	}
}

LLSD LLSDArgsMapper::map(const LLSD& argsmap) const
{
	if (!(argsmap.isUndefined() || argsmap.isMap() || argsmap.isArray()))
	{
		std::ostringstream s;
		s << mFunction << " map() needs a map or array, not " << argsmap;
		llwarns << s.str() << llendl;
		throw DispatchError(s.str());
	}
	// Initialize the args array. Indexing a non-const LLSD array grows it
	// to appropriate size, but we don't want to resize this one on each
	// new operation. Just make it as big as we need before we start
	// stuffing values into it.
	LLSD args(LLSD::emptyArray());
	if (mDefaults.size() == 0)
	{
		// If this function requires no arguments, fast exit (do not try to
		// assign to args[-1]).
		return args;
	}
	args[mDefaults.size() - 1] = LLSD();

	// Get a vector of chars to indicate holes. It's tempting to just scan
	// for LLSD::isUndefined() values after filling the args array from
	// the map, but it's plausible for caller to explicitly pass
	// isUndefined() as the value of some parameter name. That's legal
	// since isUndefined() has well-defined conversions (default value)
	// for LLSD data types. So use a whole separate array for detecting
	// holes. (Avoid std::vector<bool> which is known to be odd -- can we
	// iterate?)
	default_vec_t filled(args.size());

	if (argsmap.isArray())
	{
		// Fill args from array. If there are too many args in passed array,
		// ignore the rest.
		size_t size = argsmap.size();
		if (size > args.size())
		{
			// We do not just use std::min() because we want to sneak in this
			// warning if caller passes too many args.
			llwarns << mFunction << " needs " << args.size()
					<< " params, ignoring last " << (size - args.size())
					<< " of passed " << size << ": " << argsmap << llendl;
			size = args.size();
		}
		for (size_t i = 0; i < size; ++i)
		{
			// Copy the actual argument from argsmap
			args[i] = argsmap[i];
			// Note that it has been filled
			filled[i] = 1;
		}
	}
	else
	{
		// argsmap is in fact a map. Walk the map.
		for (LLSD::map_const_iterator it = argsmap.beginMap(),
									  end = argsmap.endMap();
			 it != end; ++it)
		{
			// it->first is a parameter-name string, with it->second its
			// value. Look up the name's position index in mIndexes.
			idx_map_t::const_iterator ixit = mIndexes.find(it->first);
			if (ixit == mIndexes.end())
			{
				// Allow for a map containing more params than were passed in
				// our names array. Caller typically receives a map containing
				// the function name, cruft such as reqid, etc. Ignore keys
				// not defined in mIndexes.
				LL_DEBUGS("LLSDArgsMapper") << mFunction << " ignoring "
											<< it->first << "=" << it->second
											<< LL_ENDL;
				continue;
			}
			size_t pos = ixit->second;
			// Store the value at that position in the args array.
			args[pos] = it->second;
			// Do not forget to record the fact that we have filled this
			// position.
			filled[pos] = 1;
		}
	}

	// Fill any remaining holes from mDefaults.
	LLSD unfilled(LLSD::emptyArray());
	for (size_t i = 0, iend = args.size(); i < iend; ++i)
	{
		if (!filled[i])
		{
			// If there is no default value for this parameter, that is an
			// error.
			if (!mHasDefault[i])
			{
				unfilled.append(mNames[i]);
			}
			else
			{
				args[i] = mDefaults[i];
			}
		}
	}
	// If any required args (args without defaults) were left unfilled by
	// argsmap, that is a problem.
	if (unfilled.size())
	{
		std::ostringstream s;
		s << mFunction << " missing required arguments "
		  << formatlist(unfilled) << " from " << argsmap;
		llwarns << s.str() << llendl;
		throw DispatchError(s.str());
	}

	// Done
	return args;
}

std::string LLSDArgsMapper::formatlist(const LLSD& list)
{
	std::ostringstream out;
	const char* delim = "";
	for (LLSD::array_const_iterator it = list.beginArray(),
									end = list.endArray();
		 it != end; ++it)
	{
		out << delim << it->asString();
		delim = ", ";
	}
	return out.str();
}

///////////////////////////////////////////////////////////////////////////////
// LLEventDispatcher
///////////////////////////////////////////////////////////////////////////////

LLEventDispatcher::LLEventDispatcher(const std::string& desc,
									 const std::string& key)
:	mDesc(desc),
	mKey(key)
{
}

// DispatchEntry subclass used for callables accepting(const LLSD&)

struct LLEventDispatcher::LLSDDispatchEntry
:	public LLEventDispatcher::DispatchEntry
{
	LLSDDispatchEntry(const std::string& desc, const Callable& func,
					  const LLSD& required)
	:	DispatchEntry(desc),
		mFunc(func),
		mRequired(required)
	{
	}

	void call(const std::string& desc, const LLSD& event) const override
	{
		// Validate the syntax of the event itself.
		std::string mismatch(llsd_matches(mRequired, event));
		if (!mismatch.empty())
		{
			std::ostringstream s;
			s << desc << ": bad request: " << mismatch;
			llwarns << s.str() << llendl;
			throw DispatchError(s.str());
		}
		// Event syntax looks good, go for it!
		mFunc(event);
	}

	LL_INLINE LLSD addMetadata(LLSD meta) const override
	{
		meta["required"] = mRequired;
		return meta;
	}

	Callable mFunc;
	LLSD mRequired;
};

// DispatchEntry subclass for passing LLSD to functions accepting arbitrary
// argument types (convertible via LLSDParam)

struct LLEventDispatcher::ParamsDispatchEntry
:	public LLEventDispatcher::DispatchEntry
{
	LL_INLINE ParamsDispatchEntry(const std::string& desc,
								  const invoker_function& func)
	:	DispatchEntry(desc),
		mInvoker(func)
	{
	}

	LL_INLINE void call(const std::string& desc,
						const LLSD& event) const override
	{
		LLSDArgsSource src(desc, event);
		mInvoker(boost::bind(&LLSDArgsSource::next, boost::ref(src)));
	}

	invoker_function mInvoker;
};

// DispatchEntry subclass for dispatching LLSD::Array to functions accepting
// arbitrary argument types (convertible via LLSDParam)
struct LLEventDispatcher::ArrayParamsDispatchEntry
:	public LLEventDispatcher::ParamsDispatchEntry
{
	ArrayParamsDispatchEntry(const std::string& desc,
							 const invoker_function& func,
							 size_t arity)
	:	ParamsDispatchEntry(desc, func),
		mArity(arity)
	{
	}

	LLSD addMetadata(LLSD meta) const override
	{
		LLSD array(LLSD::emptyArray());
		// Resize to number of arguments required
		if (mArity)
		{
			array[mArity - 1] = LLSD();
		}
		llassert_always(array.size() == mArity);
		meta["required"] = array;
		return meta;
	}

	size_t mArity;
};

// DispatchEntry subclass for dispatching LLSD::Map to functions accepting
// arbitrary argument types (convertible via LLSDParam)

struct LLEventDispatcher::MapParamsDispatchEntry
:	public LLEventDispatcher::ParamsDispatchEntry
{
	MapParamsDispatchEntry(const std::string& name, const std::string& desc,
						   const invoker_function& func,
						   const LLSD& params, const LLSD& defaults)
	:	ParamsDispatchEntry(desc, func),
		mMapper(name, params, defaults),
		mRequired(LLSD::emptyMap())
	{
		// Build the set of all param keys, then delete the ones that are
		// optional. What's left are the ones that are required.
		for (LLSD::array_const_iterator it = params.beginArray(),
										end = params.endArray();
			 it != end; ++it)
		{
			mRequired[it->asString()] = LLSD();
		}

		if (defaults.isArray() || defaults.isUndefined())
		{
			// Right-align the params and defaults arrays.
			size_t offset = params.size() - defaults.size();
			// Now the name of every defaults[i] is at params[i + offset].
			for (size_t i = 0, count = defaults.size(); i < count; ++i)
			{
				// Erase this optional param from mRequired.
				mRequired.erase(params[i + offset].asString());
				// Instead, make an entry in mOptional with the default
				// param's name and value.
				mOptional[params[i + offset].asString()] = defaults[i];
			}
		}
		else if (defaults.isMap())
		{
			// If defaults is already a map, then it's already in the form we
			// intend to deliver in metadata
			mOptional = defaults;
			// Just delete from mRequired every key appearing in mOptional.
			for (LLSD::map_const_iterator it = mOptional.beginMap(),
										  end = mOptional.endMap();
				 it != end; ++it)
			{
				mRequired.erase(it->first);
			}
		}
	}

	LL_INLINE void call(const std::string& desc,
						const LLSD& event) const override
	{
		// Just convert from LLSD::Map to LLSD::Array using mMapper, then pass
		// to base-class call() method.
		ParamsDispatchEntry::call(desc, mMapper.map(event));
	}

	LL_INLINE LLSD addMetadata(LLSD meta) const override
	{
		meta["required"] = mRequired;
		meta["optional"] = mOptional;
		return meta;
	}

	LLSDArgsMapper	mMapper;
	LLSD			mRequired;
	LLSD			mOptional;
};

void LLEventDispatcher::addArrayParamsDispatchEntry(const std::string& name,
													const std::string& desc,
													const invoker_function& invoker,
													size_t arity)
{
	mDispatch.emplace(name,
					  DispatchMap::mapped_type(new ArrayParamsDispatchEntry(desc,
																			invoker,
																			arity)));
}

void LLEventDispatcher::addMapParamsDispatchEntry(const std::string& name,
												  const std::string& desc,
												  const invoker_function& invoker,
												  const LLSD& params,
												  const LLSD& defaults)
{
	mDispatch.emplace(name,
					  DispatchMap::mapped_type(new MapParamsDispatchEntry(name,
																		  desc,
																		  invoker,
																		  params,
																		  defaults)));
}

// Registers a callable by name
void LLEventDispatcher::add(const std::string& name, const std::string& desc,
							const Callable& callable, const LLSD& required)
{
	mDispatch.emplace(name,
					  DispatchMap::mapped_type(new LLSDDispatchEntry(desc,
																	 callable,
																	 required)));
}

void LLEventDispatcher::addFail(const std::string& name,
								const std::string& classname) const
{
	llerrs << "LLEventDispatcher(" << *this << ")::add(" << name
		   << "): " << classname << " is not a subclass of LLEventDispatcher"
		   << llendl;
}

// Unregisters a callable
bool LLEventDispatcher::remove(const std::string& name)
{
	DispatchMap::iterator found = mDispatch.find(name);
	if (found == mDispatch.end())
	{
		return false;
	}
	mDispatch.erase(found);
	return true;
}

// Calls a registered callable with an explicitly-specified name.
void LLEventDispatcher::operator()(const std::string& name, const LLSD& event) const
{
	std::string error = try_call_log("", name, event);
	if (!error.empty())
	{
		callFail(event, error);
	}
}

// Extracts the key value from the incoming event, and calls the callable whose
// name is specified by that map key.
void LLEventDispatcher::operator()(const LLSD& event) const
{
	std::string error = try_call_log(mKey, event[mKey], event);
	if (!error.empty())
	{
		callFail(event, error);
	}
}

void LLEventDispatcher::callFail(const LLSD& event, const std::string& msg) const
{
	static LLSD::String key("reply");
	if (event.has(key))
	{
		// If the passed event has a "reply" key, send a reply to that
		// LLEventPump.
		gEventPumps.sendReply(llsd::map("error", msg), event, key);
	}
}

bool LLEventDispatcher::try_call(const LLSD& event) const
{
	return try_call_log(mKey, event[mKey], event).empty();
}

bool LLEventDispatcher::try_call(const std::string& name, const LLSD& event) const
{
	return try_call_log(std::string(), name, event).empty();
}

std::string LLEventDispatcher::try_call_log(const std::string& key,
											const std::string& name,
											const LLSD& event) const
{
	std::string error = try_call(key, name, event);
	if (!error.empty())
	{
		llwarns << error << llendl;
	}
	return error;
}

// This internal method returns an empty string when the call succeeded, or a
// non-empty error message otherwise.
std::string LLEventDispatcher::try_call(const std::string& key,
										const std::string& name,
										const LLSD& event) const
{
	std::string report = "LLEventDispatcher(" + mDesc + ")";

	DispatchMap::const_iterator it = mDispatch.find(name);
	if (it == mDispatch.end())
	{
		if (key.empty())
		{
			return report + ": '" + name + "' not found";
		}
		return report + ": bad '" + key + "' value '" + name + "'";
	}

	try
	{
		// Found the name, so it is plausible to even attempt the call.
		it->second->call(report + " calling '" + name + "'", event);
	}
	catch (const DispatchError& err)
	{
		return err.what();
	}

	// Call successful.
	return "";
}

LLSD LLEventDispatcher::getMetadata(const std::string& name) const
{
	DispatchMap::const_iterator found = mDispatch.find(name);
	if (found == mDispatch.end())
	{
		return LLSD();
	}
	LLSD meta;
	meta["name"] = name;
	meta["desc"] = found->second->mDesc;
	return found->second->addMetadata(meta);
}

LLDispatchListener::LLDispatchListener(const std::string& pumpname,
									   const std::string& key)
:	LLEventDispatcher(pumpname, key),
	mPump(pumpname, true),		  // allow tweaking for uniqueness
	mBoundListener(mPump.listen("self",
								boost::bind(&LLDispatchListener::process, this,
											_1)))
{
}

bool LLDispatchListener::process(const LLSD& event)
{
	(*this)(event);
	return false;
}

LLEventDispatcher::DispatchEntry::DispatchEntry(const std::string& desc)
:	mDesc(desc)
{
}

///////////////////////////////////////////////////////////////////////////////
// LLEventAPI
///////////////////////////////////////////////////////////////////////////////

LLEventAPI::LLEventAPI(const std::string& name, const std::string& desc,
					   const std::string& field)
:	lbase(name, field),
	ibase(name),
	mDesc(desc)
{
}

LLEventAPI::Response::Response(const LLSD& seed, const LLSD& request,
							   const LLSD::String& reply_key)
:	mResp(seed),
	mReq(request),
	mKey(reply_key)
{
}

LLEventAPI::Response::~Response()
{
	// When you instantiate a stack Response object, if the original request
	// requested a reply, send it when we leave this block, no matter how.
	gEventPumps.sendReply(mResp, mReq, mKey);
}

void LLEventAPI::Response::warn(const std::string& warning)
{
	llwarns << warning << llendl;
	mResp["warnings"].append(warning);
}

void LLEventAPI::Response::error(const std::string& error)
{
	// Use llwarns rather than llerrs: we do not want the viewer to shut down
	// altogether.
    llwarns << error << llendl;
    mResp["error"] = error;
}
