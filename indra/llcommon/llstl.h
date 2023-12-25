/**
 * @file llstl.h
 * @brief helper object & functions for use with the stl.
 *
 * $LicenseInfo:firstyear=2003&license=viewergpl$
 *
 * Copyright (c) 2003-2009, Linden Research, Inc.
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

#ifndef LL_LLSTL_H
#define LL_LLSTL_H

#include <algorithm>
#include <list>
#include <map>
#include <set>
#include <vector>

#include "stdtypes.h"

#include "llpreprocessor.h"

// DeletePointer is a simple helper for deleting all pointers in a container.
// The general form is:
//
//  std::for_each(cont.begin(), cont.end(), DeletePointer());
//  somemap.clear();
//
// Do not forget to clear() !

struct DeletePointer
{
	template<typename T> void operator()(T* ptr) const
	{
		delete ptr;
	}
};

// DeletePairedPointer is a simple helper for deleting all pointers in a map.
// The general form is:
//
//  std::for_each(somemap.begin(), somemap.end(), DeletePairedPointer());
//  somemap.clear();		// Do not leave dangling pointers around
// WARNING: this does NOT work with Tessil fast hash maps and sets, that
// return a const pair as iter->second !
struct DeletePairedPointer
{
	template<typename T> void operator()(T& ptr) const
	{
		delete ptr.second;
		ptr.second = NULL;
	}
};

template<typename T, typename ALLOC>
void delete_and_clear(std::list<T*, ALLOC>& list)
{
	std::for_each(list.begin(), list.end(), DeletePointer());
	list.clear();
}

template<typename T, typename ALLOC>
void delete_and_clear(std::vector<T*, ALLOC>& vector)
{
	std::for_each(vector.begin(), vector.end(), DeletePointer());
	vector.clear();
}

template<typename T, typename COMPARE, typename ALLOC>
void delete_and_clear(std::set<T*, COMPARE, ALLOC>& set)
{
	std::for_each(set.begin(), set.end(), DeletePointer());
	set.clear();
}

template<typename K, typename V, typename COMPARE, typename ALLOC>
void delete_and_clear(std::map<K, V*, COMPARE, ALLOC>& map)
{
	std::for_each(map.begin(), map.end(), DeletePairedPointer());
	map.clear();
}

template<typename T>
void delete_and_clear(T*& ptr)
{
	if (ptr)
	{
		delete ptr;
		ptr = NULL;
	}
}

// Similar to get_ptr_in_map, but for any type with a valid T(0) constructor.
// WARNING: Make sure default_value (generally 0) is not a valid map entry !
template <typename T>
LL_INLINE typename T::mapped_type get_if_there(const T& inmap,
											   typename T::key_type const& key,
											   typename T::mapped_type default_value)
{
	// Typedef here avoids warnings because of new C++ naming rules.
	typedef typename T::const_iterator map_it_t;
	map_it_t iter = inmap.find(key);
	if (iter == inmap.end())
	{
		return default_value;
	}
	return iter->second;
};

// Simple function to help with finding pointers in maps.
// For example:
// 	typedef  map_t;
//  std::map<int, const char*> foo;
//	foo[18] = "there";
//	foo[2] = "hello";
// 	const char* bar = get_ptr_in_map(foo, 2); // bar -> "hello"
//  const char* baz = get_ptr_in_map(foo, 3); // baz == NULL
template <typename T>
LL_INLINE typename T::mapped_type get_ptr_in_map(const T& inmap,
												 typename T::key_type const& key)
{
	// Typedef here avoids warnings because of new C++ naming rules.
	typedef typename T::const_iterator map_it_t;
	map_it_t iter = inmap.find(key);
	if (iter == inmap.end())
	{
		return NULL;
	}
	return iter->second;
};

// Example:
//  for (std::vector<T>::iterator iter = mList.begin(); iter != mList.end(); )
//  {
//    if ((*iter)->isMarkedForRemoval())
//      iter = vector_replace_with_last(mList, iter);
//    else
//      ++iter;
//  }
template <typename T>
LL_INLINE typename std::vector<T>::iterator vector_replace_with_last(std::vector<T>& invec,
																	 typename std::vector<T>::iterator iter)
{
	typename std::vector<T>::iterator last = invec.end();
	if (iter == last)
	{
		return iter;
	}

	if (iter == --last)
	{
		invec.pop_back();
		return invec.end();
	}

	*iter = *last;
	invec.pop_back();
	return iter;
};

// Example: vector_replace_with_last(mList, x);
template <typename T>
LL_INLINE bool vector_replace_with_last(std::vector<T>& invec, const T& val)
{
	typename std::vector<T>::iterator last = invec.end();
	typename std::vector<T>::iterator it = std::find(invec.begin(), last, val);
	if (it == last)
	{
		return false;
	}
	if (it != --last)
	{
		*it = *last;
	}
	invec.pop_back();
	return true;
}

#endif // LL_LLSTL_H
