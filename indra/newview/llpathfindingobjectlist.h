/**
 * @file llpathfindingobjectlist.h
 * @brief Header file for llpathfindingobjectlist
 *
 * $LicenseInfo:firstyear=2012&license=viewergpl$
 *
 * Copyright (c) 2012, Linden Research, Inc.
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

#ifndef LL_LLPATHFINDINGOBJECTLIST_H
#define LL_LLPATHFINDINGOBJECTLIST_H

#include <map>
#include <memory>
#include <string>

#include "llpathfindingobject.h"

class LLPathfindingCharacterList;
class LLPathfindingLinksetList;

class LLPathfindingObjectList
{
public:
	typedef std::shared_ptr<LLPathfindingObjectList> ptr_t;

	LLPathfindingObjectList();
	virtual ~LLPathfindingObjectList();

	LL_INLINE virtual LLPathfindingCharacterList* asCharacterList()
	{
		return NULL;
	}

	LL_INLINE virtual const LLPathfindingCharacterList* asCharacterList() const
	{
		return NULL;
	}

	LL_INLINE virtual LLPathfindingLinksetList* asLinksetList()
	{
		return NULL;
	}

	LL_INLINE virtual const LLPathfindingLinksetList* asLinksetList() const
	{
		return NULL;
	}

	void clear();

	LLPathfindingObject::ptr_t find(const LLUUID& obj_id) const;

	LL_INLINE bool isEmpty() const				{ return mObjectMap.empty(); }

	typedef LLPathfindingObject::map_t::const_iterator const_iterator;

	LL_INLINE const_iterator begin() const		{ return mObjectMap.begin(); }

	LL_INLINE const_iterator end() const		{ return mObjectMap.end(); }

	void update(LLPathfindingObject::ptr_t objectp);
	void update(ptr_t object_listp);

protected:
	LL_INLINE LLPathfindingObject::map_t& getObjectMap()
	{
		return mObjectMap;
	}

private:
	LLPathfindingObject::map_t mObjectMap;
};

#endif // LL_LLPATHFINDINGOBJECTLIST_H
