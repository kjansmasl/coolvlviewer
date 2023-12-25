/**
 * @file lluuid.cpp
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

#include "linden_common.h"

#include "llmd5.h"
#include "llmutex.h"
#include "llrand.h"
#include "llstring.h"
#include "llsys.h"				// For LLOSInfo::getNodeID()
#include "lltimer.h"
#include "hbxxh.h"

const LLUUID LLUUID::null;
const LLTransactionID LLTransactionID::tnull;

static LLMutex sTimeMutex;

#if LL_UUID_ALIGMENT_STATS
static LLMutex sAlignmentMutex;

//static
U64 LLUUID::sAlignmentCounts[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

//static
void LLUUID::incAlignment(intptr_t address)
{
	sAlignmentMutex.lock();
	++sAlignmentCounts[address & 7UL];
	sAlignmentMutex.unlock();
}
#endif

LLUUID::LLUUID(const char* in_string) noexcept
{
#if LL_UUID_ALIGMENT_STATS
	incAlignment((intptr_t)mData);
#endif
	if (!in_string || !*in_string)
	{
		setNull();
		return;
	}

	set(in_string);
}

LLUUID::LLUUID(const char* in_string, bool emit) noexcept
{
#if LL_UUID_ALIGMENT_STATS
	incAlignment((intptr_t)mData);
#endif
	if (!in_string || !*in_string)
	{
		setNull();
		return;
	}

	set(in_string, emit);
}

LLUUID::LLUUID(const std::string& in_string) noexcept
{
#if LL_UUID_ALIGMENT_STATS
	incAlignment((intptr_t)mData);
#endif
	if (in_string.empty())
	{
		setNull();
		return;
	}

	set(in_string);
}

LLUUID::LLUUID(const std::string& in_string, bool emit) noexcept
{
#if LL_UUID_ALIGMENT_STATS
	incAlignment((intptr_t)mData);
#endif
	if (in_string.empty())
	{
		setNull();
		return;
	}

	set(in_string, emit);
}

static const char* sDigits = "0123456789abcdef";

// I am using a lookup table to avoid branches during hexadecimal to digit
// conversion, and unrolled manually the loop, with properly interleaved '-'
// additions. HB
// *TODO: consider SSE/AVX to convert (long ?) integers to string.
std::string LLUUID::asString() const
{
	std::string out;
	out.reserve(UUID_BYTES + 4);

	U8 byte = mData[0];
	out += sDigits[(byte >> 4) & 0x0F];
	out += sDigits[byte & 0x0F];

	byte = mData[1];
	out += sDigits[(byte >> 4) & 0x0F];
	out += sDigits[byte & 0x0F];

	byte = mData[2];
	out += sDigits[(byte >> 4) & 0x0F];
	out += sDigits[byte & 0x0F];

	byte = mData[3];
	out += sDigits[(byte >> 4) & 0x0F];
	out += sDigits[byte & 0x0F];

	out += '-';

	byte = mData[4];
	out += sDigits[(byte >> 4) & 0x0F];
	out += sDigits[byte & 0x0F];

	byte = mData[5];
	out += sDigits[(byte >> 4) & 0x0F];
	out += sDigits[byte & 0x0F];

	out += '-';

	byte = mData[6];
	out += sDigits[(byte >> 4) & 0x0F];
	out += sDigits[byte & 0x0F];

	byte = mData[7];
	out += sDigits[(byte >> 4) & 0x0F];
	out += sDigits[byte & 0x0F];

	out += '-';

	byte = mData[8];
	out += sDigits[(byte >> 4) & 0x0F];
	out += sDigits[byte & 0x0F];

	byte = mData[9];
	out += sDigits[(byte >> 4) & 0x0F];
	out += sDigits[byte & 0x0F];

	out += '-';

	byte = mData[10];
	out += sDigits[(byte >> 4) & 0x0F];
	out += sDigits[byte & 0x0F];

	byte = mData[11];
	out += sDigits[(byte >> 4) & 0x0F];
	out += sDigits[byte & 0x0F];

	byte = mData[12];
	out += sDigits[(byte >> 4) & 0x0F];
	out += sDigits[byte & 0x0F];

	byte = mData[13];
	out += sDigits[(byte >> 4) & 0x0F];
	out += sDigits[byte & 0x0F];

	byte = mData[14];
	out += sDigits[(byte >> 4) & 0x0F];
	out += sDigits[byte & 0x0F];

	byte = mData[15];
	out += sDigits[(byte >> 4) & 0x0F];
	out += sDigits[byte & 0x0F];

	return out;
}

// Private method to perform the string conversion on the stack (when 'out' is
// allocated on the latter), which avoids memory allocations with some C++
// libraries (where std::string is allocated on the heap when over 12 bytes
// long or so). 'out' must point to a buffer with at least 37 bytes free. HB
void LLUUID::toCString(char* out) const
{
	U8 byte = mData[0];
	*out++ = sDigits[(byte >> 4) & 0x0F];
	*out++ = sDigits[byte & 0x0F];

	byte = mData[1];
	*out++ = sDigits[(byte >> 4) & 0x0F];
	*out++ = sDigits[byte & 0x0F];

	byte = mData[2];
	*out++ = sDigits[(byte >> 4) & 0x0F];
	*out++ = sDigits[byte & 0x0F];

	byte = mData[3];
	*out++ = sDigits[(byte >> 4) & 0x0F];
	*out++ = sDigits[byte & 0x0F];

	*out++ = '-';

	byte = mData[4];
	*out++ = sDigits[(byte >> 4) & 0x0F];
	*out++ = sDigits[byte & 0x0F];

	byte = mData[5];
	*out++ = sDigits[(byte >> 4) & 0x0F];
	*out++ = sDigits[byte & 0x0F];

	*out++ = '-';

	byte = mData[6];
	*out++ = sDigits[(byte >> 4) & 0x0F];
	*out++ = sDigits[byte & 0x0F];

	byte = mData[7];
	*out++ = sDigits[(byte >> 4) & 0x0F];
	*out++ = sDigits[byte & 0x0F];

	*out++ = '-';

	byte = mData[8];
	*out++ = sDigits[(byte >> 4) & 0x0F];
	*out++ = sDigits[byte & 0x0F];

	byte = mData[9];
	*out++ = sDigits[(byte >> 4) & 0x0F];
	*out++ = sDigits[byte & 0x0F];

	*out++ = '-';

	byte = mData[10];
	*out++ = sDigits[(byte >> 4) & 0x0F];
	*out++ = sDigits[byte & 0x0F];

	byte = mData[11];
	*out++ = sDigits[(byte >> 4) & 0x0F];
	*out++ = sDigits[byte & 0x0F];

	byte = mData[12];
	*out++ = sDigits[(byte >> 4) & 0x0F];
	*out++ = sDigits[byte & 0x0F];

	byte = mData[13];
	*out++ = sDigits[(byte >> 4) & 0x0F];
	*out++ = sDigits[byte & 0x0F];

	byte = mData[14];
	*out++ = sDigits[(byte >> 4) & 0x0F];
	*out++ = sDigits[byte & 0x0F];

	byte = mData[15];
	*out++ = sDigits[(byte >> 4) & 0x0F];
	*out++ = sDigits[byte & 0x0F];

	*out = '\0';
}

bool LLUUID::set(const char* in_string, bool emit)
{
	return set(ll_safe_string(in_string), emit);
}

bool LLUUID::set(const std::string& in_string, bool emit)
{
	bool broken_format = false;

	// Empty strings should make null UUID
	if (in_string.empty())
	{
		setNull();
		return true;
	}

	if (in_string.length() != UUID_STR_LENGTH - 1)
	{
		// First implementation did not have the right UUID format. Should not
		// see any of these any more.
		if (in_string.length() == UUID_STR_LENGTH - 2)
		{
			if (emit)
			{
				llwarns << "Warning !  Using broken UUID string format"
						<< llendl;
			}
			broken_format = true;
		}
		else
		{
			if (emit)
			{
				llwarns << "Bad UUID string: " << in_string << llendl;
			}
			setNull();
			return false;
		}
	}

	constexpr char hexa = 10 - 'a';
	constexpr char HEXA = 10 - 'A';
	U8 cur_pos = 0;
	U8* ptr = mData;
	for (S32 i = 0; i < UUID_BYTES; ++i)
	{
		if (i == 4 || i == 6 || i == 8 || i == 10)
		{
			++cur_pos;
			if (broken_format && i == 10)
			{
				// Missing '-' in the broken format
				--cur_pos;
			}
		}

		*ptr = 0;
		char c = in_string[cur_pos];
		if (c >= '0' && c <= '9')
		{
			*ptr += (U8)(c - '0');
		}
		else if (c >= 'a' && c <= 'f')
		{
			*ptr += (U8)(c + hexa);
		}
		else if (c >= 'A' && c <= 'F')
		{
			*ptr += (U8)(c + HEXA);
		}
		else
		{
			if (emit)
			{
				llwarns << "Invalid UUID string character" << llendl;
			}
			setNull();
			return false;
		}
		*ptr <<= 4;

		c = in_string[++cur_pos];
		if (c >= '0' && c <= '9')
		{
			*ptr += (U8)(c - '0');
		}
		else if (c >= 'a' && c <= 'f')
		{
			*ptr += (U8)(c + hexa);
		}
		else if (c >= 'A' && c <= 'F')
		{
			*ptr += (U8)(c + HEXA);
		}
		else
		{
			if (emit)
			{
				llwarns << "Invalid UUID string character" << llendl;
			}
			setNull();
			return false;
		}
		++cur_pos;
		++ptr;
	}

	return true;
}

// Helper function
bool is_hex_digit(char c)
{
	return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
		   (c >= 'A' && c <= 'F');
}

//static
bool LLUUID::validate(const std::string& in_string)
{
	bool broken_format = false;
	if (in_string.length() != UUID_STR_LENGTH - 1)
	{
		// First implementation did not have the right UUID format.
		if (in_string.length() == UUID_STR_LENGTH - 2)
		{
			broken_format = true;
		}
		else
		{
			return false;
		}
	}

	U8 cur_pos = 0;
	for (U32 i = 0; i < 16; ++i)
	{
		if (i == 4 || i == 6 || i == 8 || i == 10)
		{
			++cur_pos;
			if (broken_format && i == 10)
			{
				// Missing '-' in the broken format
				--cur_pos;
			}
		}

		if (!is_hex_digit(in_string[cur_pos++]))
		{
			return false;
		}

		if (!is_hex_digit(in_string[cur_pos++]))
		{
			return false;
		}
	}

	return true;
}

const LLUUID& LLUUID::operator^=(const LLUUID& rhs)
{
	U32* me = (U32*)&(mData[0]);
	const U32* other = (U32*)&(rhs.mData[0]);
	for (S32 i = 0; i < 4; ++i)
	{
		me[i] = me[i] ^ other[i];
	}
	return *this;
}

LLUUID LLUUID::operator^(const LLUUID& rhs) const
{
	LLUUID id(*this);
	id ^= rhs;
	return id;
}

// WARNING: this algorithm SHALL NOT be changed. It is also used by the server
// and plays a role in some assets validation (e.g. clothigng items). Changing
// it would cause invalid assets.
void LLUUID::combine(const LLUUID& other, LLUUID& result) const
{
	LLMD5 md5_uuid;
	md5_uuid.update((unsigned char*)mData, 16);
	md5_uuid.update((unsigned char*)other.mData, 16);
	md5_uuid.finalize();
	md5_uuid.raw_digest(result.mData);
}

LLUUID LLUUID::combine(const LLUUID& other) const
{
	LLUUID combination;
	combine(other, combination);
	return combination;
}

std::ostream& operator<<(std::ostream& s, const LLUUID& uuid)
{
	char uuid_str[UUID_STR_LENGTH];
	uuid.toCString(uuid_str);
	s << uuid_str;
	return s;
}

std::istream& operator>>(std::istream& s, LLUUID& uuid)
{
	U32 i;
	char uuid_str[UUID_STR_LENGTH];
	for (i = 0; i < UUID_STR_LENGTH - 1; ++i)
	{
		s >> uuid_str[i];
	}
	uuid_str[i] = '\0';
	uuid.set(std::string(uuid_str));
	return s;
}

S32 LLUUID::cmpTime(uuid_time_t* t1, uuid_time_t* t2)
{
	// Compare two time values.

	if (t1->high < t2->high) return -1;
	if (t1->high > t2->high) return 1;
	if (t1->low < t2->low)  return -1;
	if (t1->low > t2->low)  return 1;
	return 0;
}

void LLUUID::getSystemTime(uuid_time_t* timestamp)
{
	// Get system time with 100ns precision. Time is since Oct 15, 1582.
#if LL_WINDOWS
	ULARGE_INTEGER time;
	GetSystemTimeAsFileTime((FILETIME*)&time);
	// NT keeps time in FILETIME format which is 100ns ticks since
	// Jan 1, 1601. UUIDs use time in 100ns ticks since Oct 15, 1582.
	// The difference is 17 Days in Oct + 30 (Nov) + 31 (Dec)
	// + 18 years and 5 leap days.
	time.QuadPart += (unsigned __int64)(1000 * 1000 * 10) *				// seconds
					 (unsigned __int64)(60 * 60 * 24) *					// days
					 (unsigned __int64)(17 + 30 + 31 + 365 * 18 + 5);	// # of days

	timestamp->high = time.HighPart;
	timestamp->low  = time.LowPart;
#else
	struct timeval tp;
	gettimeofday(&tp, 0);

	// Offset between UUID formatted times and Unix formatted times.
	// UUID UTC base time is October 15, 1582.
	// Unix base time is January 1, 1970.
	U64 uuid_time = ((U64)tp.tv_sec * 10000000) + (tp.tv_usec * 10) +
									U64L(0x01B21DD213814000);
	timestamp->high = (U32) (uuid_time >> 32);
	timestamp->low  = (U32) (uuid_time & 0xFFFFFFFF);
#endif
}

void LLUUID::getCurrentTime(uuid_time_t* timestamp)
{
	// Get current time as 60 bit 100ns ticks since whenever. Compensate for
	// the fact that real clock resolution is less than 100ns.

	constexpr U32 uuids_per_tick = 1024;

	static uuid_time_t time_last;
	static U32 uuids_this_tick;

	static bool init = false;
	if (!init)
	{
		getSystemTime(&time_last);
		uuids_this_tick = uuids_per_tick;
		init = true;
	}

	uuid_time_t time_now = { 0, 0 };

	while (true)
	{
		getSystemTime(&time_now);

		// If clock reading changed since last UUID generated
		if (cmpTime(&time_last, &time_now))
		{
			// Reset count of uuid's generated with this clock reading
			uuids_this_tick = 0;
			break;
		}
		if (uuids_this_tick < uuids_per_tick)
		{
			++uuids_this_tick;
			break;
		}
		// Going too fast for our clock; spin
	}

	time_last = time_now;

	if (uuids_this_tick != 0)
	{
		if (time_now.low & 0x80000000)
		{
			time_now.low += uuids_this_tick;
			if (!(time_now.low & 0x80000000))
			{
				++time_now.high;
			}
		}
		else
		{
			time_now.low += uuids_this_tick;
		}
	}

	timestamp->high = time_now.high;
	timestamp->low  = time_now.low;
}

void LLUUID::generate()
{
	static unsigned char node_id[6];
	static uuid_time_t time_last = { 0, 0 };
	static U16 clock_seq = 0;

	static bool init_done = false;
	if (!init_done)
	{
		init_done = true;
		if (LLOSInfo::getNodeID(node_id) <= 0)
		{
			for (U32 i = 0; i < 6; ++i)
			{
				node_id[i] = ll_rand() & 0xFF;
			}
			// Set multicast bit, to prevent conflicts with IEEE 802 addresses
			// obtained from network cards
			node_id[0] |= 0x80;
		}

		getCurrentTime(&time_last);
		clock_seq = (U16)ll_rand(65536);
	}

	// Get current time
	uuid_time_t timestamp;
	getCurrentTime(&timestamp);
	U16 our_clock_seq = clock_seq;

	// If clock has not changed or went backward, change clockseq
	if (cmpTime(&timestamp, &time_last) != 1)
	{
		sTimeMutex.lock();
		clock_seq = (clock_seq + 1) & 0x3FFF;
		if (clock_seq == 0)
		{
			++clock_seq;
		}
		// Ensure we are using a different clock_seq value from previous time
		our_clock_seq = clock_seq;
		sTimeMutex.unlock();
	}

	time_last = timestamp;

	memcpy(mData + 10, node_id, 6);
	U32 tmp;
	tmp = timestamp.low;
	mData[3] = (unsigned char)tmp;
	tmp >>= 8;
	mData[2] = (unsigned char)tmp;
	tmp >>= 8;
	mData[1] = (unsigned char)tmp;
	tmp >>= 8;
	mData[0] = (unsigned char)tmp;

	tmp = (U16) timestamp.high;
	mData[5] = (unsigned char)tmp;
	tmp >>= 8;
	mData[4] = (unsigned char)tmp;

	tmp = (timestamp.high >> 16) | 0x1000;
	mData[7] = (unsigned char)tmp;
	tmp >>= 8;
	mData[6] = (unsigned char)tmp;

	tmp = our_clock_seq;
	mData[9] = (unsigned char)tmp;
	tmp >>= 8;
	mData[8] = (unsigned char)tmp;

	HBXXH128::digest(*this, (const void*)mData, 16);
}

void LLUUID::generate(const std::string& hash_string)
{
	HBXXH128::digest(*this, hash_string);
}

bool LLUUID::parseUUID(const std::string& buf, LLUUID* value)
{
	if (buf.empty() || value == NULL)
	{
		return false;
	}

	std::string temp(buf);
	LLStringUtil::trim(temp);
	if (LLUUID::validate(temp))
	{
		value->set(temp);
		return true;
	}
	return false;
}

//static
LLUUID LLUUID::generateNewID(std::string hash_string)
{
	LLUUID new_id;
	if (hash_string.empty())
	{
		new_id.generate();
	}
	else
	{
		new_id.generate(hash_string);
	}
	return new_id;
}

LLAssetID LLTransactionID::makeAssetID(const LLUUID& session) const
{
	LLAssetID result;
	if (isNull())
	{
		result.setNull();
	}
	else
	{
		combine(session, result);
	}
	return result;
}
