/**
 * @file llsdutil.h
 * @author Phoenix
 * @date 2006-05-24
 * @brief Utility classes, functions, etc, for using structured data.
 *
 * $LicenseInfo:firstyear=2006&license=viewergpl$
 *
 * Copyright (c) 2006-2009, Linden Research, Inc.
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

#ifndef LL_LLSDUTIL_H
#define LL_LLSDUTIL_H

#include <memory>			// For std::make_shared
#include <utility>			// For std::forward
#include <type_traits>		// For std::decay_t

#include "llsd.h"

// U32
LLSD ll_sd_from_U32(U32);
U32 ll_U32_from_sd(const LLSD& sd);

// U64
LLSD ll_sd_from_U64(U64);
U64 ll_U64_from_sd(const LLSD& sd);

// IP Address
LLSD ll_sd_from_ipaddr(U32);
U32 ll_ipaddr_from_sd(const LLSD& sd);

// Binary to string
LLSD ll_string_from_binary(const LLSD& sd);

//String to binary
LLSD ll_binary_from_string(const LLSD& sd);

// Serializes sd to static buffer and returns pointer, useful for gdb debugging.
char* ll_print_sd(const LLSD& sd);

// Serializes sd to static buffer and returns pointer, using "pretty printing"
// mode.
char* ll_pretty_print_sd_ptr(const LLSD* sd);
char* ll_pretty_print_sd(const LLSD& sd);

// Compares the structure of an LLSD to a template LLSD and stores the "valid"
// values in a 3rd LLSD. Default values are pulled from the template. Extra
// keys/values in the test are ignored in the resultant LLSD. Ordering of
// arrays matters. Returns false if the test is of same type but values differ
// in type. Otherwise, returns true.
bool compare_llsd_with_template(const LLSD& llsd_to_test,
								const LLSD& template_llsd,
								LLSD& resultant_llsd);

// filter_llsd_with_template() is a direct clone (copy-n-paste) of 
// compare_llsd_with_template with the following differences:
// (1) bool vs BOOL return types
// (2) A map with the key value "*" is a special value and maps any key in the
//     test llsd that doesn't have an explicitly matching key in the template.
// (3) The element of an array with exactly one element is taken as a template
//     for *all* the elements of the test array.  If the template array is of
//     different size, compare_llsd_with_template() semantics apply.
bool filter_llsd_with_template(const LLSD& llsd_to_test,
							   const LLSD& template_llsd,
							   LLSD& result_llsd);

// Recursively determine whether a given LLSD data block "matches" another
// LLSD prototype. The returned string is empty() on success, non-empty() on
// mismatch.
//
// This function tests structure (types) rather than data values. It is
// intended for when a consumer expects an LLSD block with a particular
// structure, and must succinctly detect whether the arriving block is
// well-formed. For instance, a test of the form:
// if (!(data.has("request") && data.has("target") && ...))
// could instead be expressed by initializing a prototype LLSD map with the
// required keys and writing:
// if (!llsd_matches(prototype, data).empty())
//
// A non-empty return value is an error-message fragment intended to indicate
// to (English-speaking) developers where in the prototype structure the
// mismatch occurred.
//
// * If a slot in the prototype isUndefined(), then anything is valid at that
//   place in the real object. (Passing prototype == LLSD() matches anything
//   at all.)
// * An array in the prototype must match a data array at least that large.
//   (Additional entries in the data array are ignored.) Every isDefined()
//   entry in the prototype array must match the corresponding entry in the
//   data array.
// * A map in the prototype must match a map in the data. Every key in the
//   prototype map must match a corresponding key in the data map. (Additional
//   keys in the data map are ignored.) Every isDefined() value in the
//   prototype map must match the corresponding key's value in the data map.
// * Scalar values in the prototype are tested for @em type rather than value.
//   For instance, a String in the prototype matches any String at all. In
//   effect, storing an Integer at a particular place in the prototype asserts
//   that the caller intends to apply asInteger() to the corresponding slot in
//   the data.
// * A String in the prototype matches String, Boolean, Integer, Real, UUID,
//   Date and URI, because asString() applied to any of these produces a
//   meaningful result.
// * Similarly, a Boolean, Integer or Real in the prototype can match any of
//   Boolean, Integer or Real in the data -- or even String.
// * UUID matches UUID or String.
// * Date matches Date or String.
// * URI matches URI or String.
// * Binary in the prototype matches only Binary in the data.
//
// *TODO: when a Boolean, Integer or Real in the prototype matches a String in
// the data, we should examine the String @em value to ensure it can be
// meaningfully converted to the requested type. The same goes for UUID, Date
// and URI.
std::string llsd_matches(const LLSD& prototype, const LLSD& data,
						 const std::string& pfx = "");

// Deep equality. If you want to compare LLSD::Real values for approximate
// equality rather than bitwise equality, pass @a bits as for
// is_approx_equal_fraction().
bool llsd_equals(const LLSD& lhs, const LLSD& rhs, S32 bits = -1);

// Simple function to copy data out of input & output iterators if there is no
// need for casting.
template<typename Input> LLSD llsd_copy_array(Input iter, Input end)
{
	LLSD dest;
	for ( ; iter != end; ++iter)
	{
		dest.append(*iter);
	}
	return dest;
}

namespace llsd
{
	LLSD& drill_ref(LLSD& blob, const LLSD& path);

	LL_INLINE LLSD drill(const LLSD& blob, const LLSD& path)
	{
		// Non-const drill_ref() does exactly what we want. Temporarily cast
		// away constness and use that.
		return drill_ref(const_cast<LLSD&>(blob), path);
	}
}

///////////////////////////////////////////////////////////////////////////////
// LLSDMap
///////////////////////////////////////////////////////////////////////////////
// Constructs an LLSD::Map inline, with implicit conversion to LLSD. Usage:
//
// void somefunc(const LLSD&);
// ...
// somefunc(LLSDMap("alpha", "abc")("number", 17)("pi", 3.14));
//
// For completeness, LLSDMap() with no args constructs an empty map, so
// LLSDMap()("alpha", "abc")("number", 17)("pi", 3.14)</tt> produces a map
// equivalent to the above. But for most purposes, LLSD() is already equivalent
// to an empty map, and if you explicitly want an empty isMap(), there is
// LLSD::emptyMap(). However, supporting a no-args LLSDMap() constructor
// follows the principle of least astonishment.
class LLSDMap
{
public:
	LLSDMap()
	:	mData_(LLSD::emptyMap())
	{
	}

	LLSDMap(const LLSD::String& key, const LLSD& value)
	:	mData_(LLSD::emptyMap())
	{
		mData_[key] = value;
	}

	LLSDMap& operator()(const LLSD::String& key, const LLSD& value)
	{
		mData_[key] = value;
		return *this;
	}

	LL_INLINE operator LLSD() const			{ return mData_; }
	LL_INLINE LLSD get() const				{ return mData_; }

private:
	LLSD mData_;
};

///////////////////////////////////////////////////////////////////////////////
// llsd::map(): constructs an LLSD::Map inline, using modern C++ variadic
// arguments.
///////////////////////////////////////////////////////////////////////////////

namespace llsd
{

// Recursion tail
LL_INLINE void map_(LLSD&) {}

// Recursion call
template<typename T0, typename... Ts>
void map_(LLSD& data, const LLSD::String& k0, T0&& v0, Ts&&... vs)
{
	data[k0] = v0;
	map_(data, std::forward<Ts>(vs)...);
}

// Public interface
template<typename... Ts>
LLSD map(Ts&&... vs)
{
	LLSD data;
	map_(data, std::forward<Ts>(vs)...);
	return data;
}

}	// namespace llsd

///////////////////////////////////////////////////////////////////////////////
//   LLSDParam
///////////////////////////////////////////////////////////////////////////////

class LLSDParamBase
{
public:
	virtual ~LLSDParamBase() = default;
};

// LLSDParam is a customization point for passing LLSD values to function
// parameters of more or less arbitrary type. LLSD provides a small set of
// native conversions; but if a generic algorithm explicitly constructs an
// LLSDParam object in the function's argument list, a consumer can provide
// LLSDParam specializations to support more different parameter types than
// LLSD's native conversions.
//
// Usage:
// void somefunc(const paramtype&);
// ...
// somefunc(..., LLSDParam<paramtype>(someLLSD), ...);
template<typename T>
class LLSDParam : public LLSDParamBase
{
public:
	/**
	 * Default implementation converts to T on construction, saves converted
	 * value for later retrieval
	 */
	LL_INLINE LLSDParam(const LLSD& value)
	:	mValue_(value)
	{
	}

	LL_INLINE operator T() const			{ return mValue_; }

private:
	T mValue_;
};

// LLSDParam<LLSD> is for when you do not already have the target parameter
// type in hand. Instantiate LLSDParam<LLSD>(your LLSD object), and the
// templated conversion operator will try to select a more specific LLSDParam
// specialization.

template<>
class LLSDParam<LLSD> : public LLSDParamBase
{
public:
	LL_INLINE LLSDParam(const LLSD& value)
	:	mValue_(value)
	{
	}

	// If we are literally being asked for an LLSD parameter, avoid infinite
	// recursion.
	LL_INLINE operator LLSD() const			{ return mValue_; }

	// Otherwise, instantiate a more specific LLSDParam<T> to convert; this
	// preserves the existing customization mechanism.
	template<typename T>
	operator T() const
	{
		// Capture 'ptr' with the specific subclass type because mConverters_
		// only stores LLSDParamBase pointers.
		auto ptr{ std::make_shared<LLSDParam<std::decay_t<T> > >(mValue_) };
		// Keep the new converter alive until we ourselves are destroyed.
		mConverters_.emplace_back(ptr);
		return *ptr;
	}

private:
	// LLSDParam<LLSD>::operator T() works by instantiating an LLSDParam<T> on
	// demand. Returning that engages LLSDParam<T>::operator T(), producing the
	// desired result. But LLSDParam<const char*> owns a std::string whose
	// c_str() is returned by its operator const char*(). If we return a temp
	// LLSDParam<const char*>, the compiler can destroy it right away, as soon
	// as we have called operator const char*(). That is a problem since it
	// invalidates the const char* we have just passed to the subject function.
	// This LLSDParam<LLSD> is presumably guaranteed to survive until the
	// subject function has returned, so we must ensure that any constructed
	// LLSDParam<T> lives just as long as this LLSDParam<LLSD> does. Putting
	// each LLSDParam<T> on the heap and capturing a smart pointer in a vector
	// works. We would have liked to use std::unique_ptr, but vector entries
	// must be copyable.
	typedef std::vector<std::shared_ptr<LLSDParamBase> > converters_vec_t;
	mutable converters_vec_t	mConverters_;
	LLSD						mValue_;
};

// It turns out that several target types could accept an LLSD param using any
// of a few different conversions, e.g. LLUUID's constructor can accept LLUUID
// or std::string. Therefore, the compiler can't decide which LLSD conversion
// operator to choose, even though to us it seems obvious. But this is okay, we
// can specialize LLSDParam for such target types, explicitly specifying the
// desired conversion; this is part of what LLSDParam is all about. We have to
// do that enough to make it worthwhile generalizing. Using a macro because we
// need to specify one of the asReal, etc., explicit conversion methods as well
// as a type.
#define LLSDParam_for(T, AS)				\
template<>									\
class LLSDParam<T> : public LLSDParamBase	\
{											\
public:										\
	LLSDParam(const LLSD& value)			\
	:	mValue_((T)value.AS())				\
	{										\
	}										\
											\
	operator T() const { return mValue_; }	\
											\
private:									\
	T mValue_;								\
}

LLSDParam_for(F32, asReal);
LLSDParam_for(LLUUID, asUUID);
LLSDParam_for(LLDate, asDate);
LLSDParam_for(LLURI, asURI);
LLSDParam_for(LLSD::Binary, asBinary);

// LLSDParam<const char*> is an example of the kind of conversion you can
// support with LLSDParam beyond native LLSD conversions. Normally you cannot
// pass an LLSD object to a function accepting const char*, but you can safely
// pass an LLSDParam<const char*>(yourLLSD).
template<>
class LLSDParam<const char*> : public LLSDParamBase
{
public:
	LLSDParam(const LLSD& value)
	:	mValue_(value),
		mUndefined_(value.isUndefined())
	{
	}

	// The const char* we retrieve is for storage owned by our _value member.
	// That's how we guarantee that the const char* is valid for the lifetime
	// of this LLSDParam object. Constructing your LLSDParam in the argument
	// list should ensure that the LLSDParam object will persist for the
	// duration of the function call.
	operator const char*() const
	{
		if (mUndefined_)
		{
			// By default, an isUndefined() LLSD object's asString() method
			// will produce an empty string. But for a function accepting
			// const char*, it is often important to be able to pass NULL, and
			// isUndefined() seems like the best way. If you want to pass an
			// empty string, you can still pass LLSD(""). Without this special
			// case, though, no LLSD value could pass NULL.
			return NULL;
		}
		return mValue_.c_str();
	}

private:
	// The difference here is that we store a std::string rather than a const
	// char*. It is important that the LLSDParam object own the std::string.
	std::string	mValue_;
	// We do not bother storing the incoming LLSD object, but we do have to
	// distinguish whether _value is an empty string because the LLSD object
	// contains an empty string or because it's isUndefined().
	bool		mUndefined_;
};

///////////////////////////////////////////////////////////////////////////////
// llsd::array(): constructs an LLSD::Array inline, using modern C++ variadic
// arguments.
///////////////////////////////////////////////////////////////////////////////

namespace llsd
{

// Recursion tail
LL_INLINE void array_(LLSD&) {}

// Recursion call
template<typename T0, typename... Ts>
void array_(LLSD& data, T0&& v0, Ts&&... vs)
{
	data.append(std::forward<T0>(v0));
	array_(data, std::forward<Ts>(vs)...);
}

// Public interface
template<typename... Ts>
LLSD array(Ts&&... vs)
{
	LLSD data;
	array_(data, std::forward<Ts>(vs)...);
	return data;
}

}	// namespace llsd

// Creates a deep clone of an LLSD object. Maps, Arrays and binary objects are
// duplicated, atomic primitives (Boolean, Integer, Real, etc) simply use a
// shared reference.
// Optionally a filter may be specified to control what is duplicated. The map
// takes the form "keyname/boolean".
// If the value is true the value will be duplicated otherwise it will be
// skipped when encountered in a map. A key name of "*" can be specified as a
// wild card and will specify the default behavior.  If no wild card is given
// and the clone encounters a name not in the filter, that value will be
// skipped.
LLSD llsd_clone(LLSD value, LLSD filter = LLSD());

// Creates a shallow copy of a map or array. If passed any other type of LLSD
// object it simply returns that value.  See llsd_clone for a description of
// the filter parameter.
LLSD llsd_shallow(LLSD value, LLSD filter = LLSD());

// Specialization for generating a hash value from an LLSD block.
size_t hash_value(const LLSD& s) noexcept;

// std::hash implementation for LLSD
namespace std
{
	template<> struct hash<LLSD>
	{
		LL_INLINE size_t operator()(const LLSD& s) const noexcept
		{
			return hash_value(s);
		}
	};
}

#endif // LL_LLSDUTIL_H
