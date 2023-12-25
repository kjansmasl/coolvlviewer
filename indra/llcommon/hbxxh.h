/**
 * @file hbxxh.h
 * @brief High performances vectorized hashing based on xxHash.
 *
 * $LicenseInfo:firstyear=2023&license=viewergpl$
 *
 * Copyright (c) 2023, Henri Beauchamp.
 *
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.	Terms of
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

#ifndef LL_HBXXH_H
#define LL_HBXXH_H

#include "lluuid.h"

// HBXXH* classes are to be used where speed matters and cryptographic quality
// is not required (no "one-way" guarantee, though they are likely not worst in
// this respect than MD5 which got busted and is now considered too weak). The
// xxHash code they are built upon is vectorized and about 50 times faster than
// MD5. A 64 bits hash class is also provided for when 128 bits of entropy are
// not needed. The hashes collision rate is similar to MD5's.
// See https://github.com/Cyan4973/xxHash#readme for details.

// 64 bits hashing class

class HBXXH64
{
	friend std::ostream& operator<<(std::ostream&, HBXXH64);

protected:
	LOG_CLASS(HBXXH64);

public:
	LL_INLINE HBXXH64()							{ init(); }

	// Constructors for special circumstances; they all digest the first passed
	// parameter. Set 'do_finalize' to false if you do not want to finalize the
	// context, which is useful/needed when you want to update() it afterwards.
	// Ideally, the compiler should be smart enough to get our clue and
	// optimize out the const bool test during inlining...

	LL_INLINE HBXXH64(const void* buffer, size_t len,
					  const bool do_finalize = true)
	{
		init();
		update(buffer, len);
		if (do_finalize)
		{
			finalize();
		}
	}

	LL_INLINE HBXXH64(const std::string& str, const bool do_finalize = true)
	{
		init();
		update(str);
		if (do_finalize)
		{
			finalize();
		}
	}

	LL_INLINE HBXXH64(std::istream& s, const bool do_finalize = true)
	{
		init();
		update(s);
		if (do_finalize)
		{
			finalize();
		}
	}

	LL_INLINE HBXXH64(FILE* file, const bool do_finalize = true)
	{
		init();
		update(file);
		if (do_finalize)
		{
			finalize();
		}
	}

	// Make this class no-copy (it would be possible, with custom copy
	// operators, but it is not trivially copyable, because of the mState
	// pointer): it does not really make sense to allow copying it anyway,
	// since all we care about is the resulting digest (so you should only
	// need and care about storing/copying the digest and not a class
	// instance).
	HBXXH64(const HBXXH64&) noexcept = delete;
	HBXXH64& operator=(const HBXXH64&) noexcept = delete;

	~HBXXH64();

	void update(const void* buffer, size_t len);
	void update(const std::string& str);
	void update(std::istream& s);
	void update(FILE* file);

	// Convenience template to hash other types.
	// IMPORTANT: do only use for types represented in memory as a *continuous*
	// block making up the value. E.g. LLUUIDs, U32, F64, etc... NOT to be used
	// for containers such as std::map, std::set, etc... For structures,
	// classes etc, be wary of padding bytes between values and any trailing
	// padding bytes (accounted for in sizeof(T)): these *must* have been
	// zeroed on construction, or the hash will be random) !
	template<typename T>
	LL_INLINE void update(const T& value)
	{
		update((const void*)value, sizeof(T));
	}

	// Note that unlike what happens with LLMD5, you do not need to finalize()
	// HBXXH64 before using digest(), and you may keep updating() it even after
	// you got a first digest() (the next digest would of course change after
	// any update). It is still useful to use finalize() when you do not want
	// to store a final digest() result in a separate U64; after this method
	// has been called, digest() simply returns mDigest value.
	void finalize();

	U64 digest() const;

	// Fast static methods. Use them when hashing just one contiguous block of
	// data.
	static U64 digest(const void* buffer, size_t len);
	static U64 digest(const char* str);	// str must be NUL-terminated
	static U64 digest(const std::string& str);

private:
	void init();

private:
	// We use a void pointer to avoid including xxhash.h here for XXH3_state_t
	// (which cannot either be trivially forward-declared, due to complex API
	// related pre-processor macros in xxhash.h).
	void*	mState;
	U64		mDigest;
};

LL_INLINE bool operator==(const HBXXH64& a, const HBXXH64& b)
{
	return a.digest() == b.digest();
}

LL_INLINE bool operator!=(const HBXXH64& a, const HBXXH64& b)
{
	return a.digest() != b.digest();
}

// 128 bits hashing class

class HBXXH128
{
	friend std::ostream& operator<<(std::ostream&, HBXXH128);

protected:
	LOG_CLASS(HBXXH128);

public:
	LL_INLINE HBXXH128()						{ init(); }

	// Constructors for special circumstances; they all digest the first passed
	// parameter. Set 'do_finalize' to false if you do not want to finalize the
	// context, which is useful/needed when you want to update() it afterwards.
	// Ideally, the compiler should be smart enough to get our clue and
	// optimize out the const bool test during inlining...

	LL_INLINE HBXXH128(const void* buffer, size_t len,
					   const bool do_finalize = true)
	{
		init();
		update(buffer, len);
		if (do_finalize)
		{
			finalize();
		}
	}

	LL_INLINE HBXXH128(const std::string& str, const bool do_finalize = true)
	{
		init();
		update(str);
		if (do_finalize)
		{
			finalize();
		}
	}

	LL_INLINE HBXXH128(std::istream& s, const bool do_finalize = true)
	{
		init();
		update(s);
		if (do_finalize)
		{
			finalize();
		}
	}

	LL_INLINE HBXXH128(FILE* file, const bool do_finalize = true)
	{
		init();
		update(file);
		if (do_finalize)
		{
			finalize();
		}
	}

	// Make this class no-copy (it would be possible, with custom copy
	// operators, but it is not trivially copyable, because of the mState
	// pointer): it does not really make sense to allow copying it anyway,
	// since all we care about is the resulting digest (so you should only
	// need and care about storing/copying the digest and not a class
	// instance).
	HBXXH128(const HBXXH128&) noexcept = delete;
	HBXXH128& operator=(const HBXXH128&) noexcept = delete;

	~HBXXH128();

	void update(const void* buffer, size_t len);
	void update(const std::string& str);
	void update(std::istream& s);
	void update(FILE* file);

	// Convenience template to hash other types.
	// IMPORTANT: do only use for types represented in memory as a *continuous*
	// block making up the value. E.g. LLUUIDs, U32, F64, etc... NOT to be used
	// for containers such as std::map, std::set, etc... For structures,
	// classes etc, be wary of padding bytes between values and any trailing
	// padding bytes (accounted for in sizeof(T)): these *must* have been
	// zeroed on construction, or the hash will be random) !
	template<typename T>
	LL_INLINE void update(const T& value)
	{
		update((const void*)value, sizeof(T));
	}

	// Note that unlike what happens with LLMD5, you do not need to finalize()
	// HBXXH128 before using digest(), and you may keep updating() it even
	// after you got a first digest() (the next digest would of course change
	// after any update). It is still useful to use finalize() when you do not
	// want to store a final digest() result in a separate LLUUID; after this
	// method has been called, digest() simply returns a reference on mDigest.
	void finalize();

	// We use an LLUUID for the digest, since this is a 128 bits wide native
	// type available in the viewer code, making it easy to manipulate. It also
	// allows to use HBXXH128 digests efficiently as keys for std, boost or
	// phmap containers, since I already provided a very efficient hash_value()
	// function override for LLUUID (a simple XOR of the two 64 bits words).
	const LLUUID& digest() const;

	// Here, we avoid an LLUUID copy whenever we already got one to store the
	// result *and* we did not yet call finalize().
	void digest(LLUUID& result) const;

	// Fast static methods. Use them when hashing just one contiguous block of
	// data.
	static LLUUID digest(const void* buffer, size_t len);
	static LLUUID digest(const char* str);	// str must be NUL-terminated
	static LLUUID digest(const std::string& str);
	// Same as above, but saves you from an LLUUID copy when you already got
	// one for storage use.
	static void digest(LLUUID& result, const void* buffer, size_t len);
	static void digest(LLUUID& result, const char* str); // str NUL-terminated
	static void digest(LLUUID& result, const std::string& str);

private:
	void init();

private:
	// We use a void pointer to avoid including xxhash.h here for XXH3_state_t
	// (which cannot either be trivially forward-declared, due to complex API
	// related pre-processor macros in xxhash.h).
	void*	mState;
	LLUUID	mDigest;
};

LL_INLINE bool operator==(const HBXXH128& a, const HBXXH128& b)
{
	return a.digest() == b.digest();
}

LL_INLINE bool operator!=(const HBXXH128& a, const HBXXH128& b)
{
	return a.digest() != b.digest();
}

// Utility function to reduce the size of a 64 bits digest to 32 bits while
// preserving as much entropy as possible. HB
LL_INLINE U32 digest64to32(U64 digest64)
{
	return U32(digest64) ^ U32(digest64 >> 32);
}

#endif // LL_HBXXH_H
