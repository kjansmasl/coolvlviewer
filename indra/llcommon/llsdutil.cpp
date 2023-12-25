/**
 * @file llsdutil.cpp
 * @author Phoenix
 * @date 2006-05-24
 * @brief Implementation of classes, functions, etc, for using structured data.
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

#include "linden_common.h"

#include "boost/functional/hash.hpp"
#include "boost/range.hpp"

#include "llsdutil.h"

#if LL_WINDOWS
#	define WIN32_LEAN_AND_MEAN
#	include <winsock2.h>	// For htonl()
#elif LL_LINUX
#	include <netinet/in.h>
#elif LL_DARWIN
#	include <arpa/inet.h>
#endif

#include "llcommonmath.h"
#include "llsdserialize.h"

// U32
LLSD ll_sd_from_U32(U32 val)
{
	LLSD::Binary v;
	v.resize(4);

	U32 net_order = htonl(val);
	memcpy(&(v[0]), &net_order, 4);

	return LLSD(v);
}

U32 ll_U32_from_sd(const LLSD& sd)
{
	const LLSD::Binary& v = sd.asBinary();
	if (v.size() < 4)
	{
		return 0;
	}
	U32 ret;
	memcpy(&ret, v.data(), 4);
	return ntohl(ret);
}

// U64
LLSD ll_sd_from_U64(U64 val)
{
	LLSD::Binary v;
	v.resize(8);

	U32 high = htonl((U32)(val >> 32));
	U32 low = htonl((U32)val);

	memcpy(&(v[0]), &high, 4);
	memcpy(&(v[4]), &low, 4);

	return LLSD(v);
}

U64 ll_U64_from_sd(const LLSD& sd)
{
	const LLSD::Binary& v = sd.asBinary();
	if (v.size() < 8)
	{
		return 0;
	}

	U32 high, low;
	memcpy(&high, &(v[0]), 4);
	memcpy(&low, &(v[4]), 4);
	high = ntohl(high);
	low = ntohl(low);

	return U64(high) << 32 | low;
}

// IP Address (stored in net order in a U32, so don't need swizzling)
LLSD ll_sd_from_ipaddr(U32 val)
{
	LLSD::Binary v;
	v.resize(4);

	memcpy(&(v[0]), &val, 4);

	return LLSD(v);
}

U32 ll_ipaddr_from_sd(const LLSD& sd)
{
	const LLSD::Binary& v = sd.asBinary();
	if (v.size() < 4)
	{
		return 0;
	}
	U32 ret;
	memcpy(&ret, &(v[0]), 4);
	return ret;
}

// Converts an LLSD binary to an LLSD string
LLSD ll_string_from_binary(const LLSD& sd)
{
	const LLSD::Binary& value = sd.asBinary();
	std::string str;
	str.resize(value.size());
	memcpy(&str[0], &value[0], value.size());
	return str;
}

// Converts an LLSD string to an LLSD binary
LLSD ll_binary_from_string(const LLSD& sd)
{
	LLSD::Binary binary_value;

	std::string string_value = sd.asString();
	for (std::string::iterator iter = string_value.begin();
		 iter != string_value.end(); ++iter)
	{
		binary_value.push_back(*iter);
	}

	binary_value.push_back('\0');

	return binary_value;
}

char* ll_print_sd(const LLSD& sd)
{
	constexpr size_t BUFFER_SIZE = 100 * 1024;
	static char buffer[BUFFER_SIZE + 1];
	std::ostringstream stream;
	//stream.rdbuf()->pubsetbuf(buffer, BUFFER_SIZE);
	stream << LLSDOStreamer<LLSDXMLFormatter>(sd);
	stream << std::ends;
	strncpy(buffer, stream.str().c_str(), BUFFER_SIZE);
	buffer[BUFFER_SIZE] = '\0';
	return buffer;
}

char* ll_pretty_print_sd_ptr(const LLSD* sd)
{
	return sd ? ll_pretty_print_sd(*sd) : NULL;
}

char* ll_pretty_print_sd(const LLSD& sd)
{
	constexpr size_t BUFFER_SIZE = 100 * 1024;
	static char buffer[BUFFER_SIZE + 1];
	std::ostringstream stream;
	//stream.rdbuf()->pubsetbuf(buffer, BUFFER_SIZE);
	stream << LLSDOStreamer<LLSDXMLFormatter>(sd, LLSDFormatter::OPTIONS_PRETTY);
	stream << std::ends;
	strncpy(buffer, stream.str().c_str(), BUFFER_SIZE);
	buffer[BUFFER_SIZE] = '\0';
	return buffer;
}

// Compares the structure of an LLSD to a template LLSD and stores the "valid"
// values in a 3rd LLSD. Default values are pulled from the template. Extra
// keys/values in the test are ignored in the resultant LLSD. Ordering of
// arrays matters.
// Returns false if the test is of same type but values differ in type.
// Otherwise, returns true.
bool compare_llsd_with_template(const LLSD& llsd_to_test,
								const LLSD& template_llsd,
								LLSD& result_llsd)
{
	if (llsd_to_test.isUndefined() && template_llsd.isDefined())
	{
		result_llsd = template_llsd;
		return true;
	}
	if (llsd_to_test.type() != template_llsd.type())
	{
		result_llsd = LLSD();
		return false;
	}

	if (llsd_to_test.isArray())
	{
		// They are both arrays. We loop over all the items in the template,
		// verifying that the to_test has a subset (in the same order) any
		// shortcoming in the testing_llsd are just taken to be the rest of the
		// template
		LLSD data;
		LLSD::array_const_iterator test_iter;
		LLSD::array_const_iterator template_iter;

		result_llsd = LLSD::emptyArray();
		test_iter = llsd_to_test.beginArray();

		for (template_iter = template_llsd.beginArray();
			 template_iter != template_llsd.endArray() &&
			 test_iter != llsd_to_test.endArray(); ++template_iter)
		{
			if (!compare_llsd_with_template(*test_iter, *template_iter, data))
			{
				result_llsd = LLSD();
				return false;
			}
			else
			{
				result_llsd.append(data);
			}

			++test_iter;
		}

		// So either the test or the template ended. We do another loop now to
		// the end of the template grabbing the default values
		for ( ; template_iter != template_llsd.endArray(); ++template_iter)
		{
			result_llsd.append(*template_iter);
		}
	}
	else if (llsd_to_test.isMap())
	{
		// Now we loop over the keys of the two maps. Any excess is taken from
		// the template. Excess is ignored in the test.
		LLSD value;
		LLSD::map_const_iterator template_iter;

		result_llsd = LLSD::emptyMap();
		for (template_iter = template_llsd.beginMap();
			 template_iter != template_llsd.endMap(); ++template_iter)
		{
			if (llsd_to_test.has(template_iter->first))
			{
				// The test LLSD has the same key
				if (!compare_llsd_with_template(llsd_to_test[template_iter->first],
												template_iter->second, value))
				{
					result_llsd = LLSD();
					return false;
				}
				else
				{
					result_llsd[template_iter->first] = value;
				}
			}
			else
			{
				// Test llsd does not have it... Take the template as default
				// value
				result_llsd[template_iter->first] = template_iter->second;
			}
		}
	}
	else
	{
		// Of same type... Take the test LLSD's value
		result_llsd = llsd_to_test;
	}

	return true;
}

// filter_llsd_with_template() is a direct clone (copy-n-paste) of
// compare_llsd_with_template with the following differences:
// (1) bool vs BOOL return types
// (2) A map with the key value "*" is a special value and maps any key in the
//     test llsd that doesn't have an explicitly matching key in the template.
// (3) The element of an array with exactly one element is taken as a template
//     for *all* the elements of the test array.  If the template array is of
//     different size, compare_llsd_with_template() semantics apply.
bool filter_llsd_with_template(const LLSD& llsd_to_test,
							   const LLSD& template_llsd, LLSD& result_llsd)
{
	if (llsd_to_test.isUndefined() && template_llsd.isDefined())
	{
		result_llsd = template_llsd;
		return true;
	}
	if (llsd_to_test.type() != template_llsd.type())
	{
		result_llsd = LLSD();
		return false;
	}

	if (llsd_to_test.isArray())
	{
		// They are both arrays; we loop over all the items in the template
		// verifying that the to_test has a subset (in the same order) any
		// shortcoming in the testing_llsd are just taken to be the rest of the
		// template
		LLSD data;
		result_llsd = LLSD::emptyArray();
		LLSD::array_const_iterator test_iter = llsd_to_test.beginArray();
		LLSD::array_const_iterator template_iter = template_llsd.beginArray();

		if (template_llsd.size() == 1)
		{
			// If the template has a single item, treat it as the template for
			// *all* items in the test LLSD.
			while (test_iter != llsd_to_test.endArray())
			{
				if (!filter_llsd_with_template(*test_iter, *template_iter,
											   data))
				{
					result_llsd = LLSD();
					return false;
				}
				else
				{
					result_llsd.append(data);
				}
				++test_iter;
			}
		}
		else
		{
			// Traditional compare_llsd_with_template matching
			while (template_iter != template_llsd.endArray() &&
				   test_iter != llsd_to_test.endArray())
			{
				if (!filter_llsd_with_template(*test_iter, *template_iter,
											   data))
				{
					result_llsd = LLSD();
					return false;
				}
				else
				{
					result_llsd.append(data);
				}
				++template_iter;
				++test_iter;
			}

			// So either the test or the template ended. We do another loop now
			// to the end of the template grabbing the default values.
			while (template_iter != template_llsd.endArray())
			{
				result_llsd.append(*template_iter++);
			}
		}
	}
	else if (llsd_to_test.isMap())
	{
		result_llsd = LLSD::emptyMap();

		// Now we loop over the keys of the two maps any excess is taken from
		// the template excess is ignored in the test

		// Special tag for wildcarded LLSD map key templates
		const LLSD::String wildcard_tag("*");

		bool template_has_wildcard = template_llsd.has(wildcard_tag);
		LLSD wildcard_value;
		LLSD value;

		const LLSD::map_const_iterator template_iter_end(template_llsd.endMap());
		for (LLSD::map_const_iterator template_iter(template_llsd.beginMap());
			 template_iter_end != template_iter; ++template_iter)
		{
			if (wildcard_tag == template_iter->first)
			{
				wildcard_value = template_iter->second;
			}
			else if (llsd_to_test.has(template_iter->first))
			{
				// The test LLSD has the same key
				if (!filter_llsd_with_template(llsd_to_test[template_iter->first],
											   template_iter->second,
											   value))
				{
					result_llsd = LLSD();
					return false;
				}
				else
				{
					result_llsd[template_iter->first] = value;
				}
			}
			else if (!template_has_wildcard)
			{
				// Test llsd does not have it... Take the template as default
				// value
				result_llsd[template_iter->first] = template_iter->second;
			}
		}
		if (template_has_wildcard)
		{
			LLSD sub_value;
			for (LLSD::map_const_iterator test_iter = llsd_to_test.beginMap();
				 test_iter != llsd_to_test.endMap(); ++test_iter)
			{
				if (result_llsd.has(test_iter->first))
				{
					// Final value has test key, assume more specific template
					// matched and we shouldn't modify it again.
					continue;
				}
				else if (!filter_llsd_with_template(test_iter->second,
													wildcard_value, sub_value))
				{
					// Test value does not match wildcarded template
					result_llsd = LLSD();
					return false;
				}
				else
				{
					// Test value matches template, add the actuals.
					result_llsd[test_iter->first] = sub_value;
				}
			}
		}
	}
	else
	{
		// Of same type... Take the test LLSD's value
		result_llsd = llsd_to_test;
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////
// Helpers for llsd_matches()
///////////////////////////////////////////////////////////////////////////////

// Raw data used for LLSD::Type lookup
struct Data
{
	LLSD::Type type;
	const char* name;
} typedata[] =
{
#define def(type) { LLSD::type, &#type[4] }
	def(TypeUndefined),
	def(TypeBoolean),
	def(TypeInteger),
	def(TypeReal),
	def(TypeString),
	def(TypeUUID),
	def(TypeDate),
	def(TypeURI),
	def(TypeBinary),
	def(TypeMap),
	def(TypeArray)
#undef  def
};

// LLSD::Type lookup class into which we load the above static data
class TypeLookup
{
	typedef std::map<LLSD::Type, std::string> MapType;

public:
	TypeLookup()
	{
		for (const Data *di(boost::begin(typedata)),
						*dend(boost::end(typedata)); di != dend; ++di)
		{
			mMap[di->type] = di->name;
		}
	}

	std::string lookup(LLSD::Type type) const
	{
		MapType::const_iterator found = mMap.find(type);
		if (found == mMap.end())
		{
			return llformat("<unknown LLSD type %d>", type);
		}
		return found->second;
	}

private:
	MapType mMap;
};

// Static instance of the lookup class
static const TypeLookup sTypes;

// Describes a mismatch; phrasing may want tweaking
static const char op[] = " required instead of ";

// llsd_matches() wants to identify specifically where in a complex prototype
// structure the mismatch occurred. This entails passing a prefix string,
// empty for the top-level call. If the prototype contains an array of maps,
// and the mismatch occurs in the second map in a key 'foo', we want to
// decorate the returned string with: "[1]['foo']: etc." On the other hand, we
// want to omit the entire prefix -- including colon -- if the mismatch is at
// top level. This helper accepts the (possibly empty) recursively-accumulated
// prefix string, returning either empty or the original string with colon
// appended.
static const char* colon(const std::string& pfx)
{
	thread_local std::string buffer;
	if (pfx.empty())
	{
		return pfx.c_str();
	}
	buffer = pfx + ": ";
	return buffer.c_str();
}

// Param type for match_types
typedef std::vector<LLSD::Type> TypeVector;

// The scalar cases in llsd_matches() use this helper. In most cases, we can
// accept not only the exact type specified in the prototype, but also other
// types convertible to the expected type. That implies looping over an array
// of such types. If the actual type doesn't match any of them, we want to
// provide a list of acceptable conversions as well as the exact type, e.g.:
// "Integer (or Boolean, Real, String) required instead of UUID". Both the
// implementation and the calling logic are simplified by separating out the
// expected type from the convertible types.
static std::string match_types(LLSD::Type expect, // prototype.type()
							   const TypeVector& accept, // types convertible to that type
							   LLSD::Type actual,		// type we're checking
							   const std::string& pfx)   // as for llsd_matches
{
	// Trivial case: if the actual type is exactly what we expect, we're good.
	if (actual == expect)
	{
		return "";
	}

	// For the rest of the logic, build up a suitable error string as we go so
	// we only have to make a single pass over the list of acceptable types.
	// If we detect success along the way, we'll simply discard the partial
	// error string.
	std::ostringstream out;
	out << colon(pfx) << sTypes.lookup(expect);

	// If there are any convertible types, append that list.
	if (!accept.empty())
	{
		out << " (";
		const char* sep = "or ";
		for (TypeVector::const_iterator ai(accept.begin()), aend(accept.end());
			 ai != aend; ++ai, sep = ", ")
		{
			// Do not forget to return success if we match any of those types
			if (actual == *ai)
			{
				return "";
			}
			out << sep << sTypes.lookup(*ai);
		}
		out << ')';
	}
	// If we got this far, it is because 'actual' was not one of the acceptable
	// types, so we must return an error. 'out' already contains colon(pfx)
	// and the formatted list of acceptable types, so just append the mismatch
	// phrase and the actual type.
	out << op << sTypes.lookup(actual);
	return out.str();
}

// See docstring in .h file
std::string llsd_matches(const LLSD& prototype, const LLSD& data,
						 const std::string& pfx)
{
	// An undefined prototype means that any data is valid. An undefined slot
	// in an array or map prototype means that any data may fill that slot.
	if (prototype.isUndefined())
	{
		return "";
	}

	// A prototype array must match a data array with at least as many entries.
	// Moreover, every prototype entry must match the corresponding data entry.
	if (prototype.isArray())
	{
		if (!data.isArray())
		{
			return llformat("%sArray%s%s", colon(pfx), op,
							sTypes.lookup(data.type()).c_str());
		}
		if (data.size() < prototype.size())
		{
			return llformat("%sArray size %d%sArray size %d", colon(pfx),
							prototype.size(), op, data.size());
		}
		for (size_t i = 0; i < prototype.size(); ++i)
		{
			std::string match(llsd_matches(prototype[i], data[i],
										   llformat("[%d]", i)));
			if (!match.empty())
			{
				return match;
			}
		}
		return "";
	}

	// A prototype map must match a data map. Every key in the prototype must
	// have a corresponding key in the data map; every value in the prototype
	// must match the corresponding key's value in the data.
	if (prototype.isMap())
	{
		if (!data.isMap())
		{
			return llformat("%sMap%s%s", colon(pfx), op,
							sTypes.lookup(data.type()).c_str());
		}
		// If there are a number of keys missing from the data, it would be
		// frustrating to a coder to discover them one at a time, with a big
		// build each time. Enumerate all missing keys.
		std::ostringstream out;
		out << colon(pfx);
		const char* init = "Map missing keys: ";
		const char* sep = init;
		for (LLSD::map_const_iterator mi = prototype.beginMap();
			 mi != prototype.endMap(); ++mi)
		{
			if (!data.has(mi->first))
			{
				out << sep << mi->first;
				sep = ", ";
			}
		}
		// So... are we missing any keys ?
		if (sep != init)
		{
			return out.str();
		}
		std::string key, match;
		// Good, the data block contains all the keys required by the
		// prototype. Now match the prototype entries.
		for (LLSD::map_const_iterator mi2 = prototype.beginMap();
			 mi2 != prototype.endMap(); ++mi2)
		{
			key = mi2->first;
			match = llsd_matches(mi2->second, data[key],
								 llformat("['%s']", key.c_str()));
			if (!match.empty())
			{
				return match;
			}
		}
		return "";
	}
	// A String prototype can match String, Boolean, Integer, Real, UUID, Date
	// and URI, because any of these can be converted to String.
	if (prototype.isString())
	{
		static LLSD::Type accept[] =
		{
			LLSD::TypeBoolean,
			LLSD::TypeInteger,
			LLSD::TypeReal,
			LLSD::TypeUUID,
			LLSD::TypeDate,
			LLSD::TypeURI
		};
		return match_types(prototype.type(),
						   TypeVector(boost::begin(accept),
									  boost::end(accept)),
						   data.type(),
						   pfx);
	}
	// Boolean, Integer, Real match each other or String. TBD: ensure that
	// a String value is numeric.
	if (prototype.isBoolean() || prototype.isInteger() || prototype.isReal())
	{
		static LLSD::Type all[] =
		{
			LLSD::TypeBoolean,
			LLSD::TypeInteger,
			LLSD::TypeReal,
			LLSD::TypeString
		};
		// Funny business: shuffle the set of acceptable types to include all
		// but the prototype's type. Get the acceptable types in a set.
		std::set<LLSD::Type> rest(boost::begin(all), boost::end(all));
		// Remove the prototype's type because we pass that separately.
		rest.erase(prototype.type());
		return match_types(prototype.type(),
						   TypeVector(rest.begin(), rest.end()),
						   data.type(), pfx);
	}
	// UUID, Date and URI match themselves or String.
	if (prototype.isUUID() || prototype.isDate() || prototype.isURI())
	{
		static LLSD::Type accept[] =
		{
			LLSD::TypeString
		};
		return match_types(prototype.type(),
						   TypeVector(boost::begin(accept),
									  boost::end(accept)),
						   data.type(), pfx);
	}
	// We do not yet know the conversion semantics associated with any new LLSD
	// data type that might be added, so until we have been extended to handle
	// them, assume it is strict: the new type matches only itself (this is
	// true of Binary, which is why we do not handle that case separately). Too
	// bad LLSD doesn't define isConvertible(Type to, Type from).
	return match_types(prototype.type(), TypeVector(), data.type(), pfx);
}

bool llsd_equals(const LLSD& lhs, const LLSD& rhs, S32 bits)
{
	// We are comparing strict equality of LLSD representation rather than
	// performing any conversions. So if the types aren't equal, the LLSD
	// values aren't equal.
	if (lhs.type() != rhs.type())
	{
		return false;
	}

	// Here we know both types are equal. Now compare values.
	switch (lhs.type())
	{
		case LLSD::TypeUndefined:
			// Both are TypeUndefined. There is nothing more to know.
			return true;

		case LLSD::TypeReal:
			// This is where the 'bits' argument comes in handy. If passed
			// explicitly, it means to use is_approx_equal_fraction() to compare.
			if (bits >= 0)
			{
				return is_approx_equal_fraction(lhs.asReal(), rhs.asReal(),
												bits);
			}
			// Otherwise we compare bit representations, and the usual caveats
			// about comparing floating-point numbers apply. Omitting 'bits'
			// when comparing Real values is only useful when we expect
			// identical bit representation for a given Real value, e.g. for
			// integer-valued Reals.
			return lhs.asReal() == rhs.asReal();

#define COMPARE_SCALAR(type)									\
		case LLSD::Type##type:									  \
			/* LLSD::URI has operator!=() but not operator==() */   \
			/* rely on the optimizer for all others */			  \
			return (!(lhs.as##type() != rhs.as##type()))

		COMPARE_SCALAR(Boolean);
		COMPARE_SCALAR(Integer);
		COMPARE_SCALAR(String);
		COMPARE_SCALAR(UUID);
		COMPARE_SCALAR(Date);
		COMPARE_SCALAR(URI);
		COMPARE_SCALAR(Binary);
#undef COMPARE_SCALAR

		case LLSD::TypeArray:
		{
			LLSD::array_const_iterator lai(lhs.beginArray()),
									   laend(lhs.endArray()),
									   rai(rhs.beginArray()),
									   raend(rhs.endArray());
			// Compare array elements, walking the two arrays in parallel.
			for ( ; lai != laend && rai != raend; ++lai, ++rai)
			{
				// If any one array element is unequal, the arrays are unequal.
				if (!llsd_equals(*lai, *rai, bits))
				{
					return false;
				}
			}
			// Here we have reached the end of one or the other array. They are
			// equal only if they are BOTH at end: that is, if they have equal
			// length too.
			return lai == laend && rai == raend;
		}

		case LLSD::TypeMap:
		{
			// Build a set of all rhs keys.
			std::set<LLSD::String> rhskeys;
			for (LLSD::map_const_iterator rmi(rhs.beginMap()),
										  rmend(rhs.endMap());
				 rmi != rmend; ++rmi)
			{
				rhskeys.insert(rmi->first);
			}
			// Now walk all the lhs keys.
			for (LLSD::map_const_iterator lmi(lhs.beginMap()),
										  lmend(lhs.endMap());
				 lmi != lmend; ++lmi)
			{
				// Try to erase this lhs key from the set of rhs keys. If rhs
				// has no such key, the maps are unequal. erase(key) returns
				// count of items erased.
				if (rhskeys.erase(lmi->first) != 1)
				{
					return false;
				}
				// Both maps have the current key. Compare values.
				if (!llsd_equals(lmi->second, rhs[lmi->first], bits))
				{
					return false;
				}
			}
			// We've now established that all the lhs keys have equal values in
			// both maps. The maps are equal unless rhs contains a superset of
			// those keys.
			return rhskeys.empty();
		}

		default:
			// We expect that every possible type() value is specifically
			// handled above. Failing to extend this switch to support a new
			// LLSD type is an error that must be brought to the coder's
			// attention.
			llerrs << "llsd_equals(" << lhs << ", " << rhs << ", " << bits
				   << "): unknown type " << lhs.type() << llendl;
	}

	return false;
}

LLSD llsd_clone(LLSD value, LLSD filter)
{
	bool has_filter = filter.isMap();
	LLSD clone;
	switch (value.type())
	{
		case LLSD::TypeMap:
		{
			clone = LLSD::emptyMap();
			for (LLSD::map_const_iterator it = value.beginMap(),
										  end = value.endMap();
				 it != end; ++it)
			{
				if (has_filter)
				{
					if (filter.has(it->first))
					{
						if (!filter[it->first].asBoolean())
						{
							continue;
						}
					}
					else if (filter.has("*"))
					{
						if (!filter["*"].asBoolean())
						{
							continue;
						}
					}
					else
					{
						continue;
					}
				}
				clone[it->first] = llsd_clone(it->second, filter);
			}
			break;
		}

		case LLSD::TypeArray:
		{
			clone = LLSD::emptyArray();
			for (LLSD::array_const_iterator it = value.beginArray(),
											end = value.endArray();
				 it != end; ++it)
			{
				clone.append(llsd_clone(*it, filter));
			}
			break;
		}

		case LLSD::TypeBinary:
		{
			clone = LLSD::Binary(value.asBinary().begin(),
								 value.asBinary().end());
			break;
		}

		default:
		{
			clone = value;
		}
	}

	return clone;
}

LLSD llsd_shallow(LLSD value, LLSD filter)
{
	bool has_filter = filter.isMap();
	LLSD shallow;

	if (value.isMap())
	{
		shallow = LLSD::emptyMap();
		for (LLSD::map_const_iterator it = value.beginMap(),
									  end = value.endMap();
			 it != end; ++it)
		{
			if (has_filter)
			{
				if (filter.has(it->first))
				{
					if (!filter[it->first].asBoolean())
					{
						continue;
					}
				}
				else if (filter.has("*"))
				{
					if (!filter["*"].asBoolean())
					{
						continue;
					}
				}
				else
				{
					continue;
				}
			}
			shallow[it->first] = it->second;
		}
	}
	else if (value.isArray())
	{
		shallow = LLSD::emptyArray();
		for (LLSD::array_const_iterator it = value.beginArray(),
										end = value.endArray();
			 it != end; ++it)
		{
			shallow.append(*it);
		}
	}
	else
	{
		return value;
	}

	return shallow;
}

size_t hash_value(const LLSD& s) noexcept
{
	size_t seed = 0;

	LLSD::Type stype = s.type();
	boost::hash_combine(seed, (S32)stype);

	switch (stype)
	{
		case LLSD::TypeBoolean:
			boost::hash_combine(seed, s.asBoolean());
			break;

		case LLSD::TypeInteger:
			boost::hash_combine(seed, s.asInteger());
			break;

		case LLSD::TypeReal:
			boost::hash_combine(seed, s.asReal());
			break;

		case LLSD::TypeURI:
		case LLSD::TypeString:
			boost::hash_combine(seed, s.asString());
			break;

		case LLSD::TypeUUID:
			boost::hash_combine(seed, s.asUUID());
			break;

		case LLSD::TypeDate:
			boost::hash_combine(seed, s.asDate().secondsSinceEpoch());
			break;

		case LLSD::TypeBinary:
		{
			const LLSD::Binary& b(s.asBinary());
			boost::hash_range(seed, b.begin(), b.end());
			break;
		}

		case LLSD::TypeMap:
			for (LLSD::map_const_iterator it = s.beginMap(),
										  end = s.endMap();
				 it != end; ++it)
			{
				boost::hash_combine(seed, it->first);
				boost::hash_combine(seed, it->second);
			}
			break;

		case LLSD::TypeArray:
			for (LLSD::array_const_iterator it = s.beginArray(),
											end = s.endArray();
				 it != end; ++it)
			{
				boost::hash_combine(seed, *it);
			}
			break;

		case LLSD::TypeUndefined:
		default:
			break;
	}

	return seed;
}

///////////////////////////////////////////////////////////////////////////////
// llsd::drill_ref()
///////////////////////////////////////////////////////////////////////////////

namespace llsd
{

LLSD& drill_ref(LLSD& blob, const LLSD& raw_path)
{
	// Treat raw_path uniformly as an array. If it is not already an array,
	// store it as the only entry in one.
	LLSD path;
	if (raw_path.isArray())
	{
		path = raw_path;
	}
	else
	{
		path.append(raw_path);
	}

	// Need to indicate a current destination, but that current destination
	// needs to change as we step through the path array. Where normally we
	// would use an LLSD& to capture a subscripted LLSD lvalue, this time we
	// must instead use a pointer, since it must be reassigned.
	LLSD* located = &blob;

	// Now loop through that array
	for (size_t i = 0; i < path.size(); ++i)
	{
		const LLSD& key = path[i];
		if (key.isString())
		{
			// *located is an LLSD map
			located = &((*located)[key.asString()]);
		}
		else if (key.isInteger())
		{
			// *located is an LLSD array
			located = &((*located)[key.asInteger()]);
		}
		else
		{
			// What do we do with Real or Array or Map or ... ?
			// As it is a coder error, not a user error, so rub the coder's
			// face in it so it gets fixed.
			llerrs << "drill_ref(" << blob << ", " << raw_path << "): path["
				   << i << "] bad type " << sTypes.lookup(key.type())
				   << llendl;
		}
	}

	// Dereference the pointer to return a reference to the element we found
	return *located;
}

}	// namespace llsd
