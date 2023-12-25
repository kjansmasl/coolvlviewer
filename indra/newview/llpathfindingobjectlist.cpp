/**
 * @file llpathfindingobjectlist.cpp
 * @brief Implementation of llpathfindingobjectlist
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

#include "llviewerprecompiledheaders.h"

#include "llpathfindingobjectlist.h"

#include "llpathfindingobject.h"

LLPathfindingObjectList::LLPathfindingObjectList()
{
}

LLPathfindingObjectList::~LLPathfindingObjectList()
{
	clear();
}

void LLPathfindingObjectList::clear()
{
	for (LLPathfindingObject::map_t::iterator it = mObjectMap.begin(),
											  end = mObjectMap.end();
			it != end; ++it)
	{
		it->second.reset();
	}
	mObjectMap.clear();
}

LLPathfindingObject::ptr_t LLPathfindingObjectList::find(const LLUUID& obj_id) const
{
	LLPathfindingObject::ptr_t objectp;

	LLPathfindingObject::map_t::const_iterator it = mObjectMap.find(obj_id);
	if (it != mObjectMap.end())
	{
		objectp = it->second;
	}

	return objectp;
}

void LLPathfindingObjectList::update(LLPathfindingObject::ptr_t objectp)
{
	if (!objectp) return;

	const LLUUID& object_id = objectp->getUUID();

	LLPathfindingObject::map_t::iterator it = mObjectMap.find(object_id);
	if (it == mObjectMap.end())
	{
		mObjectMap.emplace(object_id, objectp);
	}
	else
	{
		it->second = objectp;
	}
}

void LLPathfindingObjectList::update(ptr_t object_listp)
{
	if (object_listp && !object_listp->isEmpty())
	{
		for (const_iterator it = object_listp->begin();
			it != object_listp->end(); ++it)
		{
			LLPathfindingObject::ptr_t objectp = it->second;
			update(objectp);
		}
	}
}
