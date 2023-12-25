/**
 * @file llvoinventorylistener.h
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

// Description of LLVOInventoryListener class, which is an interface
// for windows that are interested in updates to a ViewerObject's inventory.

#ifndef LL_LLVOINVENTORYLISTENER_H
#define LL_LLVOINVENTORYLISTENER_H

#include "llinventory.h"

class LLViewerObject;

class LLVOInventoryListener
{
public:
	virtual void inventoryChanged(LLViewerObject* object,
								 LLInventoryObject::object_list_t* inventory,
								 S32 serial_num,
								 void* user_data) = 0;

	// Remove the listener from the object and clear this listener
	// When object == NULL, mListenerVObject is used.
	void removeVOInventoryListener(LLViewerObject* object = NULL);

	// Remove all the listeners from the object and clear them
	void removeVOInventoryListeners();

	// Just clear this listener, don't worry about the object.
	// This assumes mListenerVObjects are clearing their own lists.
	// Used only in LLViewerObject::LLInventoryCallbackInfo's destructor.
	void clearVOInventoryListener(LLViewerObject* object);

	// Did we already register a listener with that object ?
	bool hasRegisteredListener(LLViewerObject* object);

	// This does the cleaning-up by removing object from all existing
	// listeners. This is called by LLViewerObject::markDead()
	static void removeObjectFromListeners(LLViewerObject* object);

protected:
	LLVOInventoryListener();
	virtual ~LLVOInventoryListener();

	void registerVOInventoryListener(LLViewerObject* object, void* user_data);

	// When object == NULL, mListenerVObject is used.
	void requestVOInventory(LLViewerObject* object = NULL);

private:
	// LLViewerObject is normally wrapped by an LLPointer, but not in this
	// case, because the listeners are cleaned up from an object as soon as it
	// is marked dead.

	// This holds the last added object (for compatibility with the old
	// one-object request per listener interface)
	LLViewerObject*			mListenerVObject;

	// This holds the data for multiple objects.
	typedef safe_hset<LLViewerObject*> objects_list_t;
	objects_list_t			mListenerVObjects;

	typedef safe_hset<LLVOInventoryListener*> listeners_list_t;
	static listeners_list_t	sListeners;
};

#endif
