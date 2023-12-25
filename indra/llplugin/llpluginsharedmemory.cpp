/**
 * @file llpluginsharedmemory.cpp
 * LLPluginSharedMemory manages a shared memory segment for use by the
 * LLPlugin API.
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

#include "linden_common.h"

#if !LL_WINDOWS
# include <unistd.h>			// For getpid()
#endif

#include "llpluginsharedmemory.h"

// on Mac and Linux, we use the native shm_open/mmap interface by using
//	#define USE_SHM_OPEN_SHARED_MEMORY 1
// in the appropriate sections below.

// For Windows, use:
//	#define USE_WIN32_SHARED_MEMORY 1

// If we ever want to fall back to the apr implementation for a platform, use:
//	#define USE_APR_SHARED_MEMORY 1

#if LL_WINDOWS
//# define USE_APR_SHARED_MEMORY 1
#	define USE_WIN32_SHARED_MEMORY 1
#elif LL_DARWIN
#	define USE_SHM_OPEN_SHARED_MEMORY 1
#elif LL_LINUX
#	define USE_SHM_OPEN_SHARED_MEMORY 1
#endif

#define APR_SHARED_MEMORY_DIR_STRING "LLPlugin_"
#if LL_WINDOWS
#	define APR_SHARED_MEMORY_PREFIX_STRING "C:\\LLPlugin_"
	// Apparently using the "Global\\" prefix here only works from
	// administrative accounts under Vista. Other options I've seen referenced
	// are "Local\\" and "Session\\".
#	define WIN32_SHARED_MEMORY_PREFIX_STRING "Local\\LL_"
#else
	// mac and linux
#	define APR_SHARED_MEMORY_PREFIX_STRING "/tmp/LLPlugin_"
#	define SHM_OPEN_SHARED_MEMORY_PREFIX_STRING "/LL"
#endif

#if USE_APR_SHARED_MEMORY
#	include "llapr.h"
#	include "apr_shm.h"
#elif USE_SHM_OPEN_SHARED_MEMORY
#	include <sys/fcntl.h>
#	include <sys/mman.h>
#	include <errno.h>
#elif USE_WIN32_SHARED_MEMORY
#	include <windows.h>
#endif // USE_APR_SHARED_MEMORY

int LLPluginSharedMemory::sSegmentNumber = 0;

std::string LLPluginSharedMemory::createName()
{
	std::stringstream newname;

#if LL_WINDOWS
	newname << GetCurrentProcessId();
#else // LL_WINDOWS
	newname << getpid();
#endif // LL_WINDOWS

	newname << "_" << sSegmentNumber++;

	return newname.str();
}

class LLPluginSharedMemoryPlatformImpl
{
public:
	LLPluginSharedMemoryPlatformImpl();

#if USE_APR_SHARED_MEMORY
	apr_shm_t* mAprSharedMemory;
#elif USE_SHM_OPEN_SHARED_MEMORY
	int mSharedMemoryFD;
#elif USE_WIN32_SHARED_MEMORY
	HANDLE mMapFile;
#endif
};

// Constructor. Creates a shared memory segment.
LLPluginSharedMemory::LLPluginSharedMemory()
:	mImpl(new LLPluginSharedMemoryPlatformImpl),
	mSize(0),
	mMappedAddress(NULL),
	mNeedsDestroy(false)
{
}

// Destructor. Uses destroy() and detach() to ensure shared memory segment is
// cleaned up.
LLPluginSharedMemory::~LLPluginSharedMemory()
{
	if (mNeedsDestroy)
	{
		destroy();
	}
	else
	{
		detach();
	}

	unlink();

	delete mImpl;
}

#if USE_APR_SHARED_MEMORY
// MARK: apr implementation

LLPluginSharedMemoryPlatformImpl::LLPluginSharedMemoryPlatformImpl()
{
	mAprSharedMemory = NULL;
}

bool LLPluginSharedMemory::map()
{
	mMappedAddress = apr_shm_baseaddr_get(mImpl->mAprSharedMemory);
	return mMappedAddress != NULL;
}

bool LLPluginSharedMemory::unmap()
{
	// This is a no-op under apr.
	return true;
}

bool LLPluginSharedMemory::close()
{
	// This is a no-op under apr.
	return true;
}

bool LLPluginSharedMemory::unlink()
{
	// This is a no-op under apr.
	return true;
}

bool LLPluginSharedMemory::create(size_t size)
{
	mName = APR_SHARED_MEMORY_PREFIX_STRING;
	char* env = getenv("TMP");
#if LL_WINDOWS
	if (!env)
	{
		env = getenv("TEMP");
	}
	if (env)
	{
		mName.assign(env);
		size_t length = mName.length();
		if (length)
		{
			if (mName[length - 1] != '\\')
			{
				mName += '\\';
			}
			mName += APR_SHARED_MEMORY_DIR_STRING;
		}
		else
		{
			mName = APR_SHARED_MEMORY_PREFIX_STRING;
		}
	}
#else
	if (!env)
	{
		env = getenv("TMPDIR");
	}
	if (env)
	{
		mName.assign(env);
		size_t length = mName.length();
		if (length)
		{
			if (mName[length - 1] != '/')
			{
				mName += '/';
			}
			mName += APR_SHARED_MEMORY_DIR_STRING;
		}
		else
		{
			mName = APR_SHARED_MEMORY_PREFIX_STRING;
		}
	}
#endif

	mName += createName();
	mSize = size;

	apr_status_t status = apr_shm_create(&(mImpl->mAprSharedMemory), mSize,
										 mName.c_str(), gAPRPoolp);

	if (ll_apr_warn_status(status))
	{
		return false;
	}

	mNeedsDestroy = true;

	return map();
}

bool LLPluginSharedMemory::destroy()
{
	if (mImpl->mAprSharedMemory)
	{
		apr_status_t status = apr_shm_destroy(mImpl->mAprSharedMemory);
		if (ll_apr_warn_status(status))
		{
			// *TODO: Is this a fatal error ?  I think not...
		}
		mImpl->mAprSharedMemory = NULL;
	}

	return true;
}

bool LLPluginSharedMemory::attach(const std::string& name, size_t size)
{
	mName = name;
	mSize = size;

	apr_status_t status = apr_shm_attach(&(mImpl->mAprSharedMemory),
										 mName.c_str(), gAPRPoolp);

	if (ll_apr_warn_status(status))
	{
		return false;
	}

	return map();
}

bool LLPluginSharedMemory::detach()
{
	if (mImpl->mAprSharedMemory)
	{
		apr_status_t status = apr_shm_detach(mImpl->mAprSharedMemory);
		if (ll_apr_warn_status(status))
		{
			// *TODO: Is this a fatal error ?  I think not...
		}
		mImpl->mAprSharedMemory = NULL;
	}

	return true;
}

#elif USE_SHM_OPEN_SHARED_MEMORY
// MARK: shm_open/mmap implementation

LLPluginSharedMemoryPlatformImpl::LLPluginSharedMemoryPlatformImpl()
{
	mSharedMemoryFD = -1;
}

bool LLPluginSharedMemory::map()
{
	mMappedAddress = ::mmap(NULL, mSize, PROT_READ | PROT_WRITE, MAP_SHARED,
							mImpl->mSharedMemoryFD, 0);
	if (!mMappedAddress)
	{
		return false;
	}

	LL_DEBUGS("Plugin") << "memory mapped at " << mMappedAddress << LL_ENDL;

	return true;
}

bool LLPluginSharedMemory::unmap()
{
	if (mMappedAddress)
	{
		LL_DEBUGS("Plugin") << "calling munmap(" << mMappedAddress << ", "
							<< mSize << ")" << LL_ENDL;
		if (::munmap(mMappedAddress, mSize) == -1)
		{
			// *TODO: Is this a fatal error ?  I think not...
		}

		mMappedAddress = NULL;
	}

	return true;
}

bool LLPluginSharedMemory::close()
{
	if (mImpl->mSharedMemoryFD != -1)
	{
		LL_DEBUGS("Plugin") << "calling close(" << mImpl->mSharedMemoryFD
							<< ")" << LL_ENDL;
		if (::close(mImpl->mSharedMemoryFD) == -1)
		{
			// *TODO: Is this a fatal error ?  I think not...
		}

		mImpl->mSharedMemoryFD = -1;
	}
	return true;
}

bool LLPluginSharedMemory::unlink()
{
	if (!mName.empty())
	{
		if (::shm_unlink(mName.c_str()) == -1)
		{
			return false;
		}
	}

	return true;
}

bool LLPluginSharedMemory::create(size_t size)
{
	mName = SHM_OPEN_SHARED_MEMORY_PREFIX_STRING;
	mName += createName();
	mSize = size;

	// Preemptive unlink, just in case something didn't get cleaned up.
	unlink();

	mImpl->mSharedMemoryFD = ::shm_open(mName.c_str(),
										O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
	if (mImpl->mSharedMemoryFD == -1)
	{
		return false;
	}

	mNeedsDestroy = true;

	if (::ftruncate(mImpl->mSharedMemoryFD, mSize) == -1)
	{
		return false;
	}

	return map();
}

bool LLPluginSharedMemory::destroy()
{
	unmap();
	close();
	return true;
}

bool LLPluginSharedMemory::attach(const std::string& name, size_t size)
{
	mName = name;
	mSize = size;

	mImpl->mSharedMemoryFD = ::shm_open(mName.c_str(), O_RDWR,
										S_IRUSR | S_IWUSR);
	if (mImpl->mSharedMemoryFD == -1)
	{
		return false;
	}

	// unlink here so the segment will be cleaned up automatically after the
	// last close.
	unlink();

	return map();
}

bool LLPluginSharedMemory::detach()
{
	unmap();
	close();
	return true;
}

#elif USE_WIN32_SHARED_MEMORY
// MARK: Win32 CreateFileMapping-based implementation

// Reference: http://msdn.microsoft.com/en-us/library/aa366551(VS.85).aspx

LLPluginSharedMemoryPlatformImpl::LLPluginSharedMemoryPlatformImpl()
{
	mMapFile = NULL;
}

bool LLPluginSharedMemory::map()
{
	mMappedAddress = MapViewOfFile(mImpl->mMapFile,		// handle to map object
								   FILE_MAP_ALL_ACCESS,	// r/w permission
								   0, 0, mSize);
	if (mMappedAddress == NULL)
	{
		llwarns << "MapViewOfFile failed: " << GetLastError() << llendl;
		return false;
	}

	LL_DEBUGS("Plugin") << "memory mapped at " << mMappedAddress << LL_ENDL;

	return true;
}

bool LLPluginSharedMemory::unmap()
{
	if (mMappedAddress)
	{
		UnmapViewOfFile(mMappedAddress);
		mMappedAddress = NULL;
	}

	return true;
}

bool LLPluginSharedMemory::close()
{
	if (mImpl->mMapFile)
	{
		CloseHandle(mImpl->mMapFile);
		mImpl->mMapFile = NULL;
	}

	return true;
}

bool LLPluginSharedMemory::unlink()
{
	// This is a no-op on Windows.
	return true;
}

bool LLPluginSharedMemory::create(size_t size)
{
	mName = WIN32_SHARED_MEMORY_PREFIX_STRING;
	mName += createName();
	mSize = size;

	mImpl->mMapFile = CreateFileMappingA(INVALID_HANDLE_VALUE,	// use paging file
										 NULL,					// default security
										 PAGE_READWRITE,		// read/write access
										 0,						// max. object size
										 mSize,					// buffer size
										 mName.c_str());		// name of mapping object
	if (!mImpl->mMapFile)
	{
		llwarns << "CreateFileMapping failed: " << GetLastError() << llendl;
		return false;
	}

	mNeedsDestroy = true;

	return map();
}

bool LLPluginSharedMemory::destroy()
{
	unmap();
	close();
	return true;
}

bool LLPluginSharedMemory::attach(const std::string& name, size_t size)
{
	mName = name;
	mSize = size;

	mImpl->mMapFile = OpenFileMappingA(FILE_MAP_ALL_ACCESS,	// read/write access
									   FALSE,				// do not inherit the name
									   mName.c_str());		// name of mapping object
	if (!mImpl->mMapFile)
	{
		llwarns << "OpenFileMapping failed: " << GetLastError() << llendl;
		return false;
	}

	return map();
}

bool LLPluginSharedMemory::detach()
{
	unmap();
	close();
	return true;
}

#endif
