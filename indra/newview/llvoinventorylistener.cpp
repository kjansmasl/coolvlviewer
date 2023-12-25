/**
 * @file llvoinventorylistener.cpp
 * @brief Interface for classes that wish to receive updates about viewer object inventory
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

#include "llviewerprecompiledheaders.h"

#include "llvoinventorylistener.h"

#include "llviewerobject.h"

//static
LLVOInventoryListener::listeners_list_t LLVOInventoryListener::sListeners;

LLVOInventoryListener::LLVOInventoryListener()
{
	sListeners.insert(this);
}

LLVOInventoryListener::~LLVOInventoryListener()
{
	removeVOInventoryListeners();
	sListeners.erase(this);
}

void LLVOInventoryListener::removeVOInventoryListener(LLViewerObject* object)
{
	if (!object)
	{
		object = mListenerVObject;
	}

	if (object && mListenerVObjects.count(object))
	{
		object->removeInventoryListener(this);
		mListenerVObjects.erase(object);
		if (mListenerVObject == object)
		{
			mListenerVObject = NULL;
		}
	}
}

void LLVOInventoryListener::removeVOInventoryListeners()
{
	while (!mListenerVObjects.empty())
	{
		objects_list_t::iterator it = mListenerVObjects.begin();
		removeVOInventoryListener(*it);
	}
}

void LLVOInventoryListener::registerVOInventoryListener(LLViewerObject* object,
														void* user_data)
{
	if (object && !object->isDead())
	{
		removeVOInventoryListener(object);
		mListenerVObject = object;
		mListenerVObjects.insert(object);
		object->registerInventoryListener(this, user_data);
	}
}

void LLVOInventoryListener::requestVOInventory(LLViewerObject* object)
{
	if (!object)
	{
		object = mListenerVObject;
	}

	if (object && !object->isDead())
	{
		object->requestInventory();
	}
}

void LLVOInventoryListener::clearVOInventoryListener(LLViewerObject* object)
{
	mListenerVObjects.erase(object);
	if (mListenerVObject == object)
	{
		mListenerVObject = NULL;
	}
}

bool LLVOInventoryListener::hasRegisteredListener(LLViewerObject* object)
{
	return object && mListenerVObjects.count(object) != 0;
}

//static
void LLVOInventoryListener::removeObjectFromListeners(LLViewerObject* object)
{
	if (!object) return;

	for (listeners_list_t::iterator it = sListeners.begin(),
									end = sListeners.end();
		 it != end; ++it)
	{
		LLVOInventoryListener* listener = *it;
		if (listener)	// Paranoia
		{
			listener->removeVOInventoryListener(object);
		}
	}
}
