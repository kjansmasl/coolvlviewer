/**
 * @file llapr.h
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

#ifndef LL_LLAPR_H
#define LL_LLAPR_H

// Modifies APR declarations for static linking under Windows
#include "llpreprocessor.h"

#if LL_LINUX
# include <sys/param.h>		// Need PATH_MAX in APR headers...
#endif
#include "apr.h"
#include "apr_errno.h"
#include "apr_pools.h"
#include "apr_dso.h"

#include "llstring.h"

extern bool gAPRInitialized;

// Function which appropriately logs error or remains quiet on APR_SUCCESS.
// Returns true if status is an error condition.
bool ll_apr_warn_status(apr_status_t status);

// There is a whole other APR error-message function if you pass a DSO handle.
bool ll_apr_warn_status(apr_status_t status, apr_dso_handle_t* handle);

extern "C" apr_pool_t* gAPRPoolp; // Global APR memory pool

// Initializes the common APR constructs: APR itself, the global pool and a
// mutex.
void ll_init_apr();

// Cleans up those common APR constructs.
void ll_cleanup_apr();

// This class manages apr_pool_t and destroys the allocated APR pool in its
// destructor.

class LLAPRPool
{
public:
	LLAPRPool(apr_pool_t* parent = NULL, apr_size_t size = 0,
			  bool release_pool = true);
	virtual ~LLAPRPool();

	virtual apr_pool_t* getAPRPool();

	LL_INLINE apr_status_t getStatus()	{ return mStatus; }

protected:
	void releaseAPRPool();
	void createAPRPool();

protected:
	apr_pool_t*		mPool;		// Pointing to an apr_pool
	apr_pool_t*		mParent;	// Parent pool

	// Max size of mPool, mPool should return memory to system if allocated
	// memory beyond this limit. However it seems not to work.
	apr_size_t		mMaxSize;

	apr_status_t	mStatus;	// Status when creating the pool

	// If set, mPool is destroyed when LLAPRPool is deleted. Default value is
	// true.
	bool			mReleasePoolFlag;
};

#endif // LL_LLAPR_H
