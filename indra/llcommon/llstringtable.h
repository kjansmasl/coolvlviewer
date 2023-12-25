/**
 * @file llstringtable.h
 * @brief The LLStringTable class provides a _fast_ method for finding
 * unique copies of strings.
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 *
 * Copyright (c) 2001-2009, Linden Research, Inc.
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

#ifndef LL_STRING_TABLE_H
#define LL_STRING_TABLE_H

#include <list>
#include <set>

#include "hbfastmap.h"
#include "hbxxh.h"

constexpr U32 MAX_STRINGS_LENGTH = 256;

///////////////////////////////////////////////////////////////////////////////
// LLStringTableEntry class
///////////////////////////////////////////////////////////////////////////////

class LLStringTableEntry
{
public:
	LLStringTableEntry(const char* str);
	~LLStringTableEntry();

	LL_INLINE void incCount()		{ ++mCount; }
	LL_INLINE bool decCount()		{ return --mCount != 0; }

public:
	char*	mString;
	S32		mCount;
};

///////////////////////////////////////////////////////////////////////////////
// LLStringTable class
///////////////////////////////////////////////////////////////////////////////

class LLStringTable
{
protected:
	LOG_CLASS(LLStringTable);

public:
	LLStringTable(S32 tablesize);
	~LLStringTable();

	LL_INLINE char* checkString(const char* str)
	{
		LLStringTableEntry* entry = checkStringEntry(str);
		return entry ? entry->mString : NULL;
	}

	LL_INLINE char* checkString(const std::string& str)
	{
		return checkString(str.c_str());
	}

	LLStringTableEntry* checkStringEntry(const char* str);

	LL_INLINE LLStringTableEntry* checkStringEntry(const std::string& str)
	{
		return checkStringEntry(str.c_str());
	}

	LLStringTableEntry* addStringEntry(const char* str);

	LL_INLINE LLStringTableEntry* addStringEntry(const std::string& str)
	{
		return addStringEntry(str.c_str());
	}

	LL_INLINE char* addString(const char* str)
	{
		LLStringTableEntry* entry = addStringEntry(str);
		return entry ? entry->mString : NULL;
	}

	LL_INLINE char* addString(const std::string& str)
	{
		// RN: safe to use temporary c_str since string is copied
		return addString(str.c_str());
	}

	void removeString(const char* str);

public:
	U32					mMaxEntries;
	U32					mUniqueEntries;

	typedef std::list<LLStringTableEntry*> string_list_t;
	typedef string_list_t* string_list_ptr_t;
	string_list_ptr_t*	mStringList;
};

extern LLStringTable gStringTable;

///////////////////////////////////////////////////////////////////////////////
// LLStdStringTable class designed to be used locally, e.g. as a member of an
// LLXmlTree. Strings can be inserted only, then quickly looked up
///////////////////////////////////////////////////////////////////////////////

typedef const std::string* LLStdStringHandle;

class LLStdStringTable
{
protected:
	LOG_CLASS(LLStdStringTable);

public:
	LLStdStringTable(S32 tablesize = 0);

	LL_INLINE ~LLStdStringTable()
	{
		cleanup();
		delete[] mStringList;
	}

	void cleanup();

	LL_INLINE LLStdStringHandle lookup(const std::string& s)
	{
		U32 hashval = makehash(s);
		return lookup(hashval, s);
	}

	LL_INLINE LLStdStringHandle checkString(const std::string& s)
	{
		U32 hashval = makehash(s);
		return lookup(hashval, s);
	}

	LL_INLINE LLStdStringHandle insert(const std::string& s)
	{
		U32 hashval = makehash(s);
		LLStdStringHandle result = lookup(hashval, s);
		if (!result)
		{
			result = new std::string(s);
			mStringList[hashval].insert(result);
		}
		return result;
	}

	LL_INLINE LLStdStringHandle addString(const std::string& s)
	{
		return insert(s);
	}

private:
	LL_INLINE U32 makehash(const std::string& s)
	{
		if (s.empty())
		{
			return 0;
		}
		return digest64to32(HBXXH64::digest(s)) & (mTableSize - 1);
	}

	LL_INLINE LLStdStringHandle lookup(U32 hashval, const std::string& s)
	{
		string_set_t& stringset = mStringList[hashval];
		LLStdStringHandle handle = &s;
		// Compares actual strings:
		string_set_t::iterator iter = stringset.find(handle);
		return iter != stringset.end() ? *iter : NULL;
	}

	// Used to compare the contents of two pointers (e.g. std::string*)
	template <typename T>
	struct compare_pointer_contents
	{
		typedef const T* Tptr;
		LL_INLINE bool operator()(const Tptr& a, const Tptr& b) const
		{
			return *a < *b;
		}
	};

private:
	typedef std::set<LLStdStringHandle,
					 compare_pointer_contents<std::string> > string_set_t;
	string_set_t*	mStringList; // [mTableSize]

	U32				mTableSize;
};

///////////////////////////////////////////////////////////////////////////////
// LLStaticHashedString class, mainly used by shaders for uniforms hashing
///////////////////////////////////////////////////////////////////////////////

class LLStaticHashedString
{
public:
	LLStaticHashedString(const std::string& s)
	:	mString(s)
	{
		mStringHash = makehash(s);
	}

	LL_INLINE const std::string& String() const		{ return mString; }
	LL_INLINE size_t Hash() const					{ return mStringHash; }

	LL_INLINE bool operator==(const LLStaticHashedString& b) const
	{
		return Hash() == b.Hash();
	}

protected:
	LL_INLINE size_t makehash(const std::string& s)
	{
		return s.empty() ? std::string::npos : (size_t)HBXXH64::digest(s);
	}

protected:
	std::string	mString;
	size_t		mStringHash;
};

struct LLStaticStringHasher
{
	enum { bucket_size = 8 };

	LL_INLINE size_t operator()(const LLStaticHashedString& key_value) const noexcept
	{
		return key_value.Hash();
	}

	LL_INLINE bool operator()(const LLStaticHashedString& left,
							  const LLStaticHashedString& right) const noexcept
	{
		return left.Hash() < right.Hash();
	}
};

template<typename MappedObject>
class LLStaticStringTable
:	public safe_hmap<LLStaticHashedString, MappedObject, LLStaticStringHasher>
{
};

#endif
