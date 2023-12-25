/** 
 * @file lldictionary.h
 * @brief Lldictionary class header file
 *
 * $LicenseInfo:firstyear=2010&license=viewergpl$
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

#ifndef LL_LLDICTIONARY_H
#define LL_LLDICTIONARY_H

#include <map>
#include <string>

#include "llpreprocessor.h"

struct LLDictionaryEntry
{
	LLDictionaryEntry(const std::string& name);
	virtual ~LLDictionaryEntry() = default;

	const std::string mName;
	std::string mNameCapitalized;
};

// This is to avoid inlining llerrs...
LL_NO_INLINE void errorDictionaryEntryAlreadyAdded();

template <class Index, class Entry>
class LLDictionary : public std::map<Index, Entry*>
{
public:
	typedef std::map<Index, Entry*> map_t;
	typedef typename map_t::iterator iterator_t;
	typedef typename map_t::const_iterator const_iterator_t;
	
	LL_INLINE LLDictionary()					{}

	virtual ~LLDictionary()
	{
		for (iterator_t iter = map_t::begin(); iter != map_t::end(); ++iter)
		{
			delete iter->second;
		}
	}

	LL_INLINE const Entry* lookup(Index index) const
	{
		const_iterator_t iter = map_t::find(index);
		return iter != map_t::end() ? iter->second : NULL;
	}

	const Index lookup(const std::string& name) const 
	{
		for (const_iterator_t iter = map_t::begin(), end = map_t::end();
			 iter != end; ++iter)
		{
			const Entry* entry = iter->second;
			if (entry && entry->mName == name)
			{
				return iter->first;
			}
		}
		return notFound();
	}

protected:
	// g++ v10.n (with n <= 2, at least) chokes on the LL_INLINE (which
	// translates into a force inline attribute for release builds), when
	// asked to compile with LTO. This is quite obviously a gcc bug... HB
#if defined(GCC_VERSION) && GCC_VERSION >= 100000
	virtual Index notFound() const				{ return Index(-1); }
#else
	LL_INLINE virtual Index notFound() const	{ return Index(-1); }
#endif

	LL_INLINE void addEntry(Index index, Entry* entry)
	{
		if (lookup(index))
		{
			errorDictionaryEntryAlreadyAdded();
		}
		(*this)[index] = entry;
	}
};

#endif // LL_LLDICTIONARY_H
