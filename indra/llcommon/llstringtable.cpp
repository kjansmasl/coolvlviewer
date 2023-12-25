/**
 * @file llstringtable.cpp
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

#include "linden_common.h"

#include "llstringtable.h"

LLStringTable gStringTable(32768);

// Helper function
static U32 make_hash(const char* str, U32 max_entries)
{
	U32 retval = 0;
	if (*str)
	{
		retval = digest64to32(HBXXH64::digest(str));
	}
	// max_entries is guaranteed to be a power of 2
	return retval & (max_entries - 1);
}

///////////////////////////////////////////////////////////////////////////////
// LLStringTableEntry class
///////////////////////////////////////////////////////////////////////////////

LLStringTableEntry::LLStringTableEntry(const char* str)
:	mString(NULL),
	mCount(1)
{
	// Copy string
	U32 length = (U32)strlen(str) + 1;
	length = llmin(length, MAX_STRINGS_LENGTH);
	mString = new char[length];
	strncpy(mString, str, length);
	mString[length - 1] = 0;
}

LLStringTableEntry::~LLStringTableEntry()
{
	if (mString)
	{
		delete[] mString;
		mString = NULL;
	}
	mCount = 0;
}

///////////////////////////////////////////////////////////////////////////////
// LLStringTable class
///////////////////////////////////////////////////////////////////////////////

LLStringTable::LLStringTable(S32 tablesize)
:	mUniqueEntries(0)
{
	if (tablesize <= 0)
	{
		tablesize = 4096; // Some arbitrary default
	}
	// Make sure tablesize is power of 2
	for (S32 i = 31; i > 0; --i)
	{
		if (tablesize & (1 << i))
		{
			if (tablesize >= 3 << (i - 1))
			{
				tablesize = 1 << (i + 1);
			}
			else
			{
				tablesize = 1 << i;
			}
			break;
		}
	}
	mMaxEntries = tablesize;

	// Allocate strings
	mStringList = new string_list_ptr_t[mMaxEntries];
	// Clear strings
	for (U32 i = 0; i < mMaxEntries; ++i)
	{
		mStringList[i] = NULL;
	}
}

LLStringTable::~LLStringTable()
{
	if (mStringList)
	{
		for (U32 i = 0; i < mMaxEntries; ++i)
		{
			if (mStringList[i])
			{
				for (string_list_t::iterator iter = mStringList[i]->begin(),
											 end = mStringList[i]->end();
					 iter != end; ++iter)
				{
					LLStringTableEntry* entry = *iter;
					if (entry)
					{
						delete entry;
					}
				}
				delete mStringList[i];
			}
		}
		delete[] mStringList;
		mStringList = NULL;
	}
}

LLStringTableEntry* LLStringTable::checkStringEntry(const char* str)
{
	if (str)
	{
		U32 hash_value = make_hash(str, mMaxEntries);
		string_list_t* strlist = mStringList[hash_value];
		if (strlist)
		{
			for (string_list_t::iterator iter = strlist->begin(),
										 end = strlist->end();
										 iter != end; ++iter)
			{
				LLStringTableEntry* entry = *iter;
				char* ret_val = entry->mString;
				if (!strncmp(ret_val, str, MAX_STRINGS_LENGTH))
				{
					return entry;
				}
			}
		}
	}
	return NULL;
}

LLStringTableEntry* LLStringTable::addStringEntry(const char* str)
{
	if (!str)
	{
		return NULL;
	}

	U32 hash_value = make_hash(str, mMaxEntries);
	string_list_t* strlist = mStringList[hash_value];
	if (strlist)
	{
		for (string_list_t::iterator iter = strlist->begin(),
									 end = strlist->end();
			 iter != end; ++iter)
		{
			LLStringTableEntry* entry = *iter;
			char* entry_str = entry->mString;
			if (!strncmp(entry_str, str, MAX_STRINGS_LENGTH))
			{
				entry->incCount();
				return entry;
			}
		}
	}
	else
	{
		strlist = new string_list_t;
		mStringList[hash_value] = strlist;
	}

	// Not found, so add !
	if (++mUniqueEntries > mMaxEntries)
	{
		llerrs << "String table too small to store a new entry: "
			   << mMaxEntries << " stored." << llendl;
	}

	LLStringTableEntry* newentry = new LLStringTableEntry(str);
	strlist->push_front(newentry);
	LL_DEBUGS("StringTable") << mUniqueEntries << "/" << mMaxEntries
							 << " unique entries." << LL_ENDL;
	return newentry;
}

void LLStringTable::removeString(const char* str)
{
	if (!str)
	{
		return;
	}

	U32 hash_value = make_hash(str, mMaxEntries);
	string_list_t* strlist = mStringList[hash_value];
	if (!strlist)
	{
		return;
	}

	for (string_list_t::iterator iter = strlist->begin(), end = strlist->end();
		 iter != end; ++iter)
	{
		LLStringTableEntry* entry = *iter;
		char* entry_str = entry->mString;
		if (!strncmp(entry_str, str, MAX_STRINGS_LENGTH))
		{
			if (!entry->decCount())
			{
				if (!mUniqueEntries)
				{
					llerrs << "Trying to remove too many strings !" << llendl;
				}
				--mUniqueEntries;
				strlist->remove(entry);
				delete entry;
			}
			return;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
// LLStdStringTable class
///////////////////////////////////////////////////////////////////////////////

LLStdStringTable::LLStdStringTable(S32 tablesize)
{
	if (tablesize <= 0)
	{
		tablesize = 256; // Default
	}
	else
	{
		// Make sure tablesize is power of 2
		S32 initial_size = tablesize;
		tablesize = 2;
		while (tablesize < initial_size)
		{
			tablesize *= 2;
		}
	}
	mTableSize = (U32)tablesize;
	mStringList = new string_set_t[tablesize];
}

void LLStdStringTable::cleanup()
{
	// Remove strings
	for (U32 i = 0; i < mTableSize; ++i)
	{
		string_set_t& stringset = mStringList[i];
		for (string_set_t::iterator iter = stringset.begin(),
									end = stringset.end();
			 iter != end; ++iter)
		{
			delete *iter;
		}
		stringset.clear();
	}
}
