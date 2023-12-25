/**
 * @file llapr.cpp
 * @author Phoenix
 * @date 2004-11-28
 * @brief Helper functions for using the apache portable runtime library.
 *
 * $LicenseInfo:firstyear=2004&license=viewergpl$
 *
 * Copyright (c) 2004-2009, Linden Research, Inc.
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

#include <stdlib.h>			// For atexit()

#include "llapr.h"

// Global APR memory pool:
apr_pool_t* gAPRPoolp = NULL;

bool gAPRInitialized = false;

void ll_init_apr()
{
	if (gAPRInitialized)
	{
		return;
	}
	gAPRInitialized = true;

	// Initialize APR and create the global pool
	apr_initialize();

	// Register apr_terminate() so that it is called on exit. DO NOT use
	// apr_terminate() directly in ll_cleanup_apr(): there is no guarantee
	// whatsoever that globally or statically declared class members will
	// not keep using APR after ll_cleanup_apr() is called and so terminating
	// APR must be done at the program late exit step. HB
	atexit(apr_terminate);

	if (!gAPRPoolp)
	{
		apr_pool_create(&gAPRPoolp, NULL);
	}
}

void ll_cleanup_apr()
{
	if (gAPRPoolp)
	{
		llinfos << "Cleaning up APR" << llendl;
		apr_pool_destroy(gAPRPoolp);
		gAPRPoolp = NULL;
	}
}

///////////////////////////////////////////////////////////////////////////////
// LLAPRPool class
///////////////////////////////////////////////////////////////////////////////

LLAPRPool::LLAPRPool(apr_pool_t* parent, apr_size_t size, bool release_pool)
:	mParent(parent),
	mReleasePoolFlag(release_pool),
	mMaxSize(size),
	mPool(NULL)
{
	createAPRPool();
}

LLAPRPool::~LLAPRPool()
{
	releaseAPRPool();
}

void LLAPRPool::createAPRPool()
{
	if (mPool)
	{
		return;
	}

	mStatus = apr_pool_create(&mPool, mParent);
	ll_apr_warn_status(mStatus);

	// mMaxSize is the number of blocks (which is usually 4K), NOT bytes.
	if (mMaxSize > 0)
	{
		apr_allocator_t* allocator = apr_pool_allocator_get(mPool);
		if (allocator)
		{
			apr_allocator_max_free_set(allocator, mMaxSize);
		}
	}
}

void LLAPRPool::releaseAPRPool()
{
	if (!mPool)
	{
		return;
	}

	if (!mParent || mReleasePoolFlag)
	{
		apr_pool_destroy(mPool);
		mPool = NULL;
	}
}

//virtual
apr_pool_t* LLAPRPool::getAPRPool()
{
	return mPool;
}

///////////////////////////////////////////////////////////////////////////////
// APR helpers
///////////////////////////////////////////////////////////////////////////////

bool ll_apr_warn_status(apr_status_t status)
{
	if (status == APR_SUCCESS)
	{
		return false;
	}

	// Do not warn about end of file, which is a "normal" occurrence of
	// some reads (reads till EOF). HB
	if (status == APR_EOF)
	{
		return true;
	}

	char buf[MAX_STRING];
	apr_strerror(status, buf, sizeof(buf));
	llwarns << "APR: " << buf << llendl;
	return true;
}

bool ll_apr_warn_status(apr_status_t status, apr_dso_handle_t* handle)
{
    bool result = ll_apr_warn_status(status);
    // Despite observed truncation of actual Mac dylib load errors, increasing
    // this buffer to more than MAX_STRING does not help: it appears that APR
    // stores the output in a fixed 255-character internal buffer. (*sigh*)
    char buf[MAX_STRING];
    apr_dso_error(handle, buf, sizeof(buf));
    llwarns << "APR: " << buf << llendl;
    return result;
}
