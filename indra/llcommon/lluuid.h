/**
 * @file lluuid.h
 *
 * $LicenseInfo:firstyear=2000&license=viewergpl$
 *
 * Copyright (c) 2000-2009, Linden Research, Inc.
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

#ifndef LL_LLUUID_H
#define LL_LLUUID_H

#include <iostream>
#include <vector>

#include "llerror.h"
#include "hbfastset.h"
#include "stdtypes.h"

// Defined to 1 to optimize with 64 bits access: 64 bits aligned UUIDs are in
// no way guaranteed, but thanks to my careful placing (i.e. where they will be
// aligned on a 8 bytes boundary) of the LLUUIDs in almost all class members
// blocks, and the fact that 64 bits viewer builds use aligned memory
// allocations, properly aligned LLUUIDs do represent the overwhelming majority
// (around 90-95%) of the UUIDs we have to deal with, meaning that working with
// 64 bits words instead of 32 bits ones *should* be beneficial. *TODO: write a
// benchmark to verify, since depending on the CPU architecture (number of
// integer units, cache lines size and performances when crossing their
// boundaries), our mileage may vary... HB
#define LLUUID_OPTIMIZE_64_BITS 1

// Define to 1 for stats about the aligments of all the LLUUIDs created during
// the session (reported at the end of the log file on viewer exit): use only
// in devel builds (since the counting slows down LLUUID constructions) to
// verify the above affirmation about the 64 bits aligned UUIDs percentage, or
// to further optimize member variables placement in classes. HB
#define LL_UUID_ALIGMENT_STATS 0

constexpr S32 UUID_BYTES = 16;
constexpr S32 UUID_WORDS = 4;
// Actually wrong, should be 36 and use size below:
constexpr S32 UUID_STR_LENGTH = 37;
constexpr S32 UUID_STR_SIZE = 37;
constexpr S32 UUID_BASE85_LENGTH = 21; // including the trailing NULL.

struct uuid_time_t
{
	U32 high;
	U32 low;
};

class LLUUID
{
protected:
	LOG_CLASS(LLUUID);

public:
	LL_INLINE LLUUID() noexcept
	{
#if LL_UUID_ALIGMENT_STATS
		incAlignment((intptr_t)mData);
#endif
#if LLUUID_OPTIMIZE_64_BITS
		U64* tmp = (U64*)mData;
		*tmp = tmp[1] = 0;
#else
		U32* tmp = (U32*)mData;
		tmp[0] = tmp[1] = tmp[2] = tmp[3] = 0;
#endif
	}

#if LL_UUID_ALIGMENT_STATS
	LL_INLINE LLUUID(const LLUUID& rhs) noexcept
	{
		incAlignment((intptr_t)mData);
# if LLUUID_OPTIMIZE_64_BITS
		U64* tmp = (U64*)mData;
		U64* rhstmp = (U64*)rhs.mData;
		*tmp = *rhstmp;
		tmp[1] = rhstmp[1];
# else
		U32* tmp = (U32*)mData;
		U32* rhstmp = (U32*)rhs.mData;
		tmp[0] = rhstmp[0];
		tmp[1] = rhstmp[1];
		tmp[2] = rhstmp[2];
		tmp[3] = rhstmp[3];
# endif
	}
#else
	// Trivially copyable.
	LLUUID(const LLUUID& rhs) noexcept = default;
#endif

	// Make sure the std (and boost) containers will use the default move
	// constructor whenever possible, by explicitely marking it noexcept. HB
	LLUUID(LLUUID&& ptr) noexcept = default;

	// Conversions from strings:
	explicit LLUUID(const char* in_string) noexcept;
	explicit LLUUID(const char* in_string, bool emit) noexcept;
	explicit LLUUID(const std::string& in_string) noexcept;
	explicit LLUUID(const std::string& in_string, bool emit) noexcept;

	~LLUUID() = default;

	// Trivially copyable.
	LLUUID& operator=(const LLUUID& rhs) noexcept = default;
	LLUUID& operator=(LLUUID&& rhs) noexcept = default;

	// Generate a new UUID:
	void generate();
	// Generate a new UUID based on hash of input stream:
	void generate(const std::string& stream);

	// Static version of above for use in initializer expressions such as
	// constructor params, etc:
	static LLUUID generateNewID(std::string stream = "");

	// Convert from string, if emit is false, do not emit warnings:
	bool set(const char* in_string, bool emit = true);
	// Convert from string, if emit is false, do not emit warnings:
	bool set(const std::string& in_string, bool emit = true);

	// Faster than setting to LLUUID::null.
	LL_INLINE void setNull()
	{
#if LLUUID_OPTIMIZE_64_BITS
		U64* tmp = (U64*)mData;
		*tmp = tmp[1] = 0;
#else
		U32* tmp = (U32*)mData;
		tmp[0] = tmp[1] = tmp[2] = tmp[3] = 0;
#endif
	}

    S32 cmpTime(uuid_time_t* t1, uuid_time_t* t2);
	static void getSystemTime(uuid_time_t* timestamp);
	void getCurrentTime(uuid_time_t* timestamp);

	// Faster than comparing to LLUUID::null.
	LL_INLINE bool isNull() const
	{
#if LLUUID_OPTIMIZE_64_BITS
		U64* tmp = (U64*)mData;
		return !(*tmp | tmp[1]);
#else
		U32* tmp = (U32*)mData;
		return !(tmp[0] | tmp[1] | tmp[2] | tmp[3]);
#endif
	}

	// Faster than comparing to LLUUID::null.
	LL_INLINE bool notNull() const
	{
#if LLUUID_OPTIMIZE_64_BITS
		U64* tmp = (U64*)mData;
		return (*tmp | tmp[1]) > 0;
#else
		U32* tmp = (U32*)mData;
		return (tmp[0] | tmp[1] | tmp[2] | tmp[3]) > 0;
#endif
	}

	LL_INLINE bool operator==(const LLUUID& rhs) const
	{
#if LLUUID_OPTIMIZE_64_BITS
		U64* tmp = (U64*)mData;
		U64* rhstmp = (U64*)rhs.mData;
		// Note: binary & to avoid branching
		return (*tmp == *rhstmp) & (tmp[1] == rhstmp[1]);
#else
		U32* tmp = (U32*)mData;
		U32* rhstmp = (U32*)rhs.mData;
		// Note: binary & to avoid branching
		return (tmp[0] == rhstmp[0]) & (tmp[1] == rhstmp[1]) &
			   (tmp[2] == rhstmp[2]) & (tmp[3] == rhstmp[3]);
#endif
	}

	LL_INLINE bool operator!=(const LLUUID& rhs) const
	{
#if LLUUID_OPTIMIZE_64_BITS
		U64* tmp = (U64*)mData;
		U64* rhstmp = (U64*)rhs.mData;
		// Note: binary | to avoid branching
		return (*tmp != *rhstmp) | (tmp[1] != rhstmp[1]);
#else
		U32* tmp = (U32*)mData;
		U32* rhstmp = (U32*)rhs.mData;
		// Note: binary | to avoid branching
		return (tmp[0] != rhstmp[0]) | (tmp[1] != rhstmp[1]) |
			   (tmp[2] != rhstmp[2]) | (tmp[3] != rhstmp[3]);
#endif
	}

	// DO NOT "optimize" these two with U32/U64s or you will scoogie the sort
	// order and this will make me very sad. IW

	LL_INLINE bool operator<(const LLUUID& rhs) const
	{
		for (U32 i = 0; i < UUID_BYTES - 1; ++i)
		{
			if (mData[i] != rhs.mData[i])
			{
				return mData[i] < rhs.mData[i];
			}
		}
		return mData[UUID_BYTES - 1] < rhs.mData[UUID_BYTES - 1];
	}

	LL_INLINE bool operator>(const LLUUID& rhs) const
	{
		for (U32 i = 0; i < UUID_BYTES - 1; ++i)
		{
			if (mData[i] != rhs.mData[i])
			{
				return mData[i] > rhs.mData[i];
			}
		}
		return mData[UUID_BYTES - 1] > rhs.mData[UUID_BYTES - 1];
	}

	// Allowing a bool operator would be dangerous as it would allow UUIDs to
	// be cast automatically to integers, among other things. Use isNull() or
	// notNull() instead.
	explicit operator bool() const = delete;

	// XOR functions. Useful since any two random UUIDs xored together will
	// yield a determinate third random unique id that can be used as a key in
	// a single uuid that represents 2.
	const LLUUID& operator^=(const LLUUID& rhs);
	LLUUID operator^(const LLUUID& rhs) const;

	// Similar to functions above, but not invertible. Yields a third random
	// UUID that can be reproduced from the two inputs but which, given the
	// result and one of the inputs cannot be used to deduce the other input.
	LLUUID combine(const LLUUID& other) const;
	void combine(const LLUUID& other, LLUUID& result) const;

	friend std::ostream& operator<<(std::ostream& s, const LLUUID& uuid);

	friend std::istream& operator>>(std::istream& s, LLUUID& uuid);

	std::string asString() const;

	LL_INLINE void toString(std::string& out) const
	{
		// Convert on stack to avoid allocating a temporary std::string
		char uuid_str[UUID_STR_LENGTH];
		toCString(uuid_str);
		out.assign(uuid_str);
	}

	// IMPORTANT: 'out' must point to a buffer of at least 37 bytes.
	void toCString(char* out) const;

	LL_INLINE U32 getCRC32() const
	{
		U32* tmp = (U32*)mData;
		return tmp[0] + tmp[1] + tmp[2] + tmp[3];
	}

	// Returns a 64 bits digest of the UUID, by XORing its two 64 bits long
	// words. HB
	LL_INLINE U64 getDigest64() const
	{
		U64* tmp = (U64*)mData;
		return tmp[0] ^ tmp[1];
	}

	// Validate that the UUID string is legal:
	static bool validate(const std::string& in_string);

	static bool parseUUID(const std::string& buf, LLUUID* value);

#if LL_UUID_ALIGMENT_STATS
private:
	static void incAlignment(intptr_t address);
#endif

public:
	U8					mData[UUID_BYTES];

	static const LLUUID	null;
#if LL_UUID_ALIGMENT_STATS
	static U64			sAlignmentCounts[8];
#endif
};

typedef std::vector<LLUUID> uuid_vec_t;
// NOTE: fast_hset *might* work, but let's not assume anything about how
// iterators will be used (especially after an erase()) on this generic
// container type, which is so widely used in the viewer code... HB
typedef safe_hset<LLUUID> uuid_list_t;

// Sub-class for keeping transaction IDs and asset IDs straight.

typedef LLUUID LLAssetID;

class LLTransactionID : public LLUUID
{
public:
	LL_INLINE LLTransactionID()
	:	LLUUID()
	{
	}

	LLAssetID makeAssetID(const LLUUID& session) const;

public:
	static const LLTransactionID tnull;
};

// std::hash implementation for LLUUID
namespace std
{
	template<> struct hash<LLUUID>
	{
		LL_INLINE size_t operator()(const LLUUID& id) const noexcept
		{
			return id.getDigest64();
		}
	};
}

// For use with boost::unordered_map and boost::unordered_set
LL_INLINE size_t hash_value(const LLUUID& id) noexcept
{
	return id.getDigest64();
}

#endif
