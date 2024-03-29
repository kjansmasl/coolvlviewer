/**
 * @file llpluginsharedmemory.h
 * @brief LLPluginSharedMemory manages a shared memory segment for use by the
 *        LLPlugin API.
 *
 * $LicenseInfo:firstyear=2008&license=viewergpl$
 *
 * Copyright (c) 2008-2009, Linden Research, Inc.
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

#ifndef LL_LLPLUGINSHAREDMEMORY_H
#define LL_LLPLUGINSHAREDMEMORY_H

#include "llpreprocessor.h"

class LLPluginSharedMemoryPlatformImpl;

class LLPluginSharedMemory
{
protected:
	LOG_CLASS(LLPluginSharedMemory);

public:
	LLPluginSharedMemory();
	~LLPluginSharedMemory();

	// Parent will use create/destroy, child will use attach/detach.
	// Message transactions will ensure child attaches after parent creates
	// and detaches before parent destroys.

	/**
	 * Creates a shared memory segment, with a name which is guaranteed to be
	 * unique on the host at the current time. Used by parent.
	 * Message transactions will (*TODO: DOC - should ? must ?) ensure child
	 * attaches after parent creates and detaches before parent destroys.
	 *
	 * @param[in] size Shared memory size in TODO:DOC units = bytes ?
	 *
	 * @return False for failure, true for success.
	 */
	bool create(size_t size);

	/**
	 * Destroys a shared memory segment. Used by parent.
	 * Message transactions will (? TODO:DOC - should? must?) ensure child
	 * attaches after parent creates and detaches before parent destroys.
	 *
	 * @return True. *TODO: DOC - always returns true. Is this the intended
	 *         behavior ?
	 */
	bool destroy();

   /**
	 * Creates and attaches a name to a shared memory segment. *TODO: DOC
	 * what is the difference between attach() and create() ?
	 *
	 * @param[in] name Name to attach to memory segment
	 * @param[in] size Size of memory segment *TODO: DOC in bytes ?
	 *
	 * @return False on failure, true otherwise.
	 */
	bool attach(const std::string& name, size_t size);

	/**
	 * Detaches shared memory segment.
	 *
	 * @return False on failure, true otherwise.
	 */
	bool detach();

	/**
	 * Checks if shared memory is mapped to a non-null address.
	 *
	 * @return True if memory address is non-null, false otherwise.
	 */
	LL_INLINE bool isMapped() const				{ return mMappedAddress != NULL; }

	/**
	 * Get pointer to shared memory.
	 *
	 * @return Pointer to shared memory.
	 */
	LL_INLINE void* getMappedAddress() const	{ return mMappedAddress; }

	/**
	 * Get size of shared memory.
	 *
	 * @return Size of shared memory in bytes. TODO:DOC are bytes the correct
	*         unit ?
	 */
	LL_INLINE size_t getSize() const			{ return mSize; }

	/**
	 * Get name of shared memory.
	 *
	 * @return Name of shared memory.
	 */
	LL_INLINE std::string getName() const		{ return mName; }

private:
	bool map();
	bool unmap();
	bool close();
	bool unlink();

	static std::string createName();

private:
	LLPluginSharedMemoryPlatformImpl*	mImpl;
	void*								mMappedAddress;
	std::string							mName;
	size_t								mSize;
	bool								mNeedsDestroy;

	static int							sSegmentNumber;
};

#endif // LL_LLPLUGINSHAREDMEMORY_H
