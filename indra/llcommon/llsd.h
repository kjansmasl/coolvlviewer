/**
 * @file llsd.h
 * @brief LLSD flexible data system.
 *
 * $LicenseInfo:firstyear=2005&license=viewergpl$
 *
 * Copyright (c) 2005-2009, Linden Research, Inc.
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

#ifndef LL_LLSD_H
#define LL_LLSD_H

#include <map>
#include <ostream>
#include <type_traits>

#include "lldate.h"
#include "lluri.h"
#include "lluuid.h"

// LLSD provides a flexible data system similar to the data facilities of
// dynamic languages like Perl and Python.  It is created to support exchange
// of structured data between loosly coupled systems.  (Here, "loosly coupled"
// means not compiled together into the same module.)
//
// Data in such exchanges must be highly tollerant of changes on either side
// such as:
//	- recompilation
//	- implementation in a different langauge
//	- addition of extra parameters
//	- execution of older versions (with fewer parameters)
//
// To this aim, the C++ API of LLSD strives to be very easy to use, and to
// default to "the right thing" whereever possible. It is extremely tollerant
// of errors and unexpected situations.
//
// The fundimental class is LLSD. LLSD is a value holding object. It holds one
// value that is either undefined, one of the scalar types, or a map or an
// array. LLSD objects have value semantics (copying them copies the value,
// though it can be considered efficient, due to shareing.), and mutable.
//
// Undefined is the singular value given to LLSD objects that are not
// initialized with any data.  It is also used as the return value for
// operations that return an LLSD,
//
// The sclar data types are:
//	- Boolean	- true or false
//	- Integer	- a 32 bit signed integer
//	- Real		- a 64 IEEE 754 floating point value
//	- UUID		- a 128 unique value
//	- String	- a sequence of zero or more Unicode chracters
//	- Date		- an absolute point in time, UTC, with resolution to the second
//	- URI		- a String that is a URI
//	- Binary	- a sequence of zero or more octets (unsigned bytes)
//
// A map is a dictionary mapping String keys to LLSD values. The keys are
// unique within a map, and have only one value (though that value could be an
// LLSD array).
//
// An array is a sequence of zero or more LLSD values.

class LLSD final	// This class may NOT be subclassed
{
public:
	LLSD();		// Initially undefined
	~LLSD();

	// Copyable and assignable (*TODO: C++11 movable)

	LLSD(const LLSD& other);
	void assign(const LLSD& other);

	LL_INLINE LLSD& operator=(const LLSD& other)
	{
		assign(other);
		return *this;
	}

	void clear();	// Resets to Undefined

	// The scalar types, and how they map onto C++
	typedef bool Boolean;
	typedef S32 Integer;
	typedef F64 Real;
	typedef std::string String;
	typedef LLUUID UUID;
	typedef LLDate Date;
	typedef LLURI URI;
	typedef std::vector<U8> Binary;

	// Scalar constructors
	LLSD(Boolean);
	LLSD(Integer);
	LLSD(Real);
	LLSD(const String&);
	LLSD(const UUID&);
	LLSD(const Date&);
	LLSD(const URI&);
	LLSD(const Binary&);

	// Support construction from size_t et al.
	template <typename VALUE,
			  typename std::enable_if<std::is_integral<VALUE>::value &&
									  !std::is_same<VALUE, Boolean>::value,
									  bool>::type = true>
	LL_INLINE LLSD(VALUE v) : LLSD((Integer)v)	{}

	// Scalar assignments
	void assign(Boolean);
	void assign(Integer);
	void assign(Real);
	void assign(const String&);
	void assign(const UUID&);
	void assign(const Date&);
	void assign(const URI&);
	void assign(const Binary&);

	// Support assignment from size_t et al.
	template <typename VALUE,
			  typename std::enable_if<std::is_integral<VALUE>::value &&
									  !std::is_same<VALUE, Boolean>::value,
									  bool>::type = true>
	LL_INLINE void assign(VALUE v)				{ assign((Integer)v); }

	// Support assignment from F32 et al.
	template <typename VALUE,
			  typename std::enable_if<std::is_floating_point<VALUE>::value,
									  bool>::type = true>
	LL_INLINE void assign(VALUE v)				{ assign((Real)v); }

	template <typename VALUE>
	LL_INLINE LLSD& operator=(VALUE v)			{ assign(v); return *this; }

	// Scalar accessors: fetch a scalar value, converting if needed and
	// possible
	// Conversion among the basic types, Boolean, Integer, Real and String, is
	// fully defined.  Each type can be converted to another with a reasonable
	// interpretation.  These conversions can be used as a convenience even
	// when you know the data is in one format, but you want it in another. Of
	// course, many of these conversions lose information.
	//
	// Note: These conversions are not the same as Perl's. In particular, when
	// converting a String to a Boolean, only the empty string converts to
	// false. Converting the String "0" to Boolean results in true.
	//
	// Conversion to and from UUID, Date, and URI is only defined to and from
	// String. Conversion is defined to be information preserving for valid
	// values of those types. These conversions can be used when one needs to
	// convert data to or from another system that cannot handle these types
	// natively, but can handle strings.
	//
	// Conversion to and from Binary is not defined.

	// Conversion of the Undefined value to any scalar type results in a
	// reasonable null or zero value for the type.

	Boolean asBoolean() const;
	Integer asInteger() const;
	Real asReal() const;
	String asString() const;
	UUID asUUID() const;
	Date asDate() const;
	URI asURI() const;
	const Binary& asBinary() const;

	// Applied to any non-string type will return a ref to an empty string.
	const String& asStringRef() const;

	LL_INLINE operator Boolean() const			{ return asBoolean(); }
	LL_INLINE operator Integer() const			{ return asInteger(); }
	LL_INLINE operator Real() const				{ return asReal(); }
	LL_INLINE operator String() const			{ return asString(); }
	LL_INLINE operator UUID() const				{ return asUUID(); }
	LL_INLINE operator Date() const				{ return asDate(); }
	LL_INLINE operator URI() const				{ return asURI(); }
	LL_INLINE operator Binary() const			{ return asBinary(); }

	// This is needed because most platforms do not automatically convert the
	// boolean negation as a bool in an if statement.
	LL_INLINE bool operator!() const			{ return !asBoolean(); }

	// Character pointer helpers. These are helper methods to make working with
	// char* the same as easy as working with strings.
	LLSD(const char*);
	void assign(const char*);
	LL_INLINE LLSD& operator=(const char* v)	{ assign(v); return *this; }

	// Map values

	static LLSD emptyMap();

	bool has(const String&) const;
	LLSD get(const String&) const;

	bool has(const char*) const;
	LLSD get(const char*) const;

	// Returns an LLSD array with keys as strings
	LLSD getKeys() const;

	void insert(const String&, const LLSD&);
	void erase(const String&);
	LLSD& with(const String&, const LLSD&);

	LLSD& operator[](const String&);
	LL_INLINE LLSD& operator[](const char* c)	{ return (*this)[String(c)]; }

	const LLSD& operator[](const String&) const;

	LL_INLINE const LLSD& operator[](const char* c) const
	{
		return (*this)[String(c)];
	}

	// Array values

	static LLSD emptyArray();

	LLSD get(size_t) const;
	void set(size_t, const LLSD&);
	void insert(size_t, const LLSD&);
	LLSD& append(const LLSD&);
	void erase(size_t);
	LLSD& with(Integer, const LLSD&);

	// Accept size_t so we can index relative to size()
	const LLSD& operator[](size_t) const;
	LLSD& operator[](size_t);

	// Template overloads to support int literals, U32 et al.
	template <typename IDX,
			  typename std::enable_if<std::is_convertible<IDX, size_t>::value,
									  bool>::type = true>
	LL_INLINE const LLSD& operator[](IDX i) const
	{
		return (*this)[size_t(i)];
	}

	template <typename IDX,
			  typename std::enable_if<std::is_convertible<IDX, size_t>::value,
									  bool>::type = true>
	LL_INLINE LLSD& operator[](IDX i)			{ return (*this)[size_t(i)]; }

	// Iterators

	size_t size() const;

	typedef std::map<String, LLSD>::iterator map_iterator;
	map_iterator beginMap();
	map_iterator endMap();

	typedef std::map<String, LLSD>::const_iterator map_const_iterator;
	map_const_iterator beginMap() const;
	map_const_iterator endMap() const;

	typedef std::vector<LLSD>::iterator array_iterator;
	array_iterator beginArray();
	array_iterator endArray();

	typedef std::vector<LLSD>::const_iterator array_const_iterator;
	array_const_iterator beginArray() const;
	array_const_iterator endArray() const;

	typedef std::vector<LLSD>::reverse_iterator reverse_array_iterator;
	reverse_array_iterator rbeginArray();
	reverse_array_iterator rendArray();

	map_const_iterator find(const String& k) const;
	map_const_iterator find(const char* k) const;

	// Type testing

	enum Type {
		TypeUndefined,
		TypeBoolean,
		TypeInteger,
		TypeReal,
		TypeString,
		TypeUUID,
		TypeDate,
		TypeURI,
		TypeBinary,
		TypeMap,
		TypeArray
	};

	Type type() const;

	LL_INLINE bool isUndefined() const			{ return type() == TypeUndefined; }
	LL_INLINE bool isDefined() const			{ return type() != TypeUndefined; }
	LL_INLINE bool isBoolean() const			{ return type() == TypeBoolean; }
	LL_INLINE bool isInteger() const			{ return type() == TypeInteger; }
	LL_INLINE bool isReal() const				{ return type() == TypeReal; }
	LL_INLINE bool isString() const				{ return type() == TypeString; }
	LL_INLINE bool isUUID() const				{ return type() == TypeUUID; }
	LL_INLINE bool isDate() const				{ return type() == TypeDate; }
	LL_INLINE bool isURI() const				{ return type() == TypeURI; }
	LL_INLINE bool isBinary() const				{ return type() == TypeBinary; }
	LL_INLINE bool isMap() const				{ return type() == TypeMap; }
	LL_INLINE bool isArray() const				{ return type() == TypeArray; }

	// Automatic cast protection. Without them, C++ can perform some
	// conversions that are clearly not what the programmer intended.
	// If you get a compiler error about them, you have made mistake in your
	// code. DO NOT IMPLEMENT THESE FUNCTIONS as a fix.
	// All of thse problems stem from trying to support char* in LLSD or in
	// std::string. There are too many automatic casts that will lead to using
	// an arbitrary pointer or scalar type to std::string.
	LLSD(const void*) = delete;				// Constructs from aribrary pointers
	void assign(const void*) = delete;		// Assigns from arbitrary pointers
	LLSD& operator=(const void*) = delete;	// Assigns from arbitrary pointers
	bool has(Integer) const = delete;		// has() only works for maps

public:
	class Impl;

private:
	Impl* impl;
};

// Declared here for convenience, but implemented in llsdserialize.cpp
std::ostream& operator<<(std::ostream& s, const LLSD& llsd);

/* QUESTIONS & TO DO
	- Would Binary be more convenient as unsigned char* buffer semantics ?
	- Should Binary be convertable to/from String, and if so how ?
		- as UTF8 encoded strings (making not like UUID<->String)
		- as Base64 or Base96 encoded (making like UUID<->String)
	- Conversions to std::string and LLUUID do not result in easy assignment
	  to std::string, std::string or LLUUID due to non-unique conversion paths
*/

#endif // LL_LLSD_H
