/**
 * @file llsys.h
 * @brief System information debugging classes.
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

#ifndef LL_SYS_H
#define LL_SYS_H

#include <string>

#include "llsingleton.h"

class LLProcessorInfo;
class LLSD;

class LLOSInfo final : public LLSingleton<LLOSInfo>
{
	friend class LLSingleton<LLOSInfo>;

protected:
	LOG_CLASS(LLOSInfo);

	LLOSInfo();
	~LLOSInfo() override;

public:
	LL_INLINE const std::string& getOSString() const
	{
		return mOSString;
	}

	LL_INLINE const std::string& getOSStringSimple() const
	{
		return mOSStringSimple;
	}

	LL_INLINE const std::string& getOSVersionString() const
	{
		return mOSVersionString;
	}

#if LL_LINUX
	// Returns the major version number for the Linux kernel. HB
	LL_INLINE U32 getKernelVersionMajor() const
	{
		return mVersionMajor;
	}

	// Returns the mminor version number for the Linux kernel. HB
	LL_INLINE U32 getKernelVersionMinor() const
	{
		return mVersionMinor;
	}
#elif LL_WINDOWS
	// When ran under Wine, this method returns the Wine version string. HB
	LL_INLINE const std::string& getWineVersionString() const
	{
		return mWineVersionString;
	}

	// Returns true when the viewer is running under Wine. HB
	LL_INLINE bool underWine() const
	{
		return mUnderWine;
	}

	// Windows 10 and later got an inaccurate Sleep() call by default, which
	// we change/configure for a 1ms resolution when we detect it. This method
	// returns true in such cases (so that the default inaccurate timer can be
	// restored on viewer exit). HB
	LL_INLINE bool inaccurateSleep() const
	{
		return mInaccurateSleep;
	}
#endif

	// Returns 1 on success, 0 or -1 on failure to get a node Id from the MAC
	// address of the network interface. On success, the six bytes of this
	// address are copied into the unsigned char buffer pointed to by node_id
	// (which must therefore point onto a valid and adequately sized buffer).
	// I moved this method here from LLUUID, since the latter should not be
	// OS-dependent and got nothing to do with networking... This method would
	// actually be better as a function in llnet.h/cpp, but then we would need
	// to make llcommon dependent on llmessage (because LLUUID and llrand need
	// it), so it finally landed here... HB
	static S32 getNodeID(unsigned char* node_id);

private:
	// OS-dependent code.
	static S32 getOSNodeID(unsigned char* node_id);

private:
	std::string	mOSString;
	std::string	mOSStringSimple;
	std::string	mOSVersionString;
#if LL_LINUX
	U32			mVersionMajor;
	U32			mVersionMinor;
#elif LL_WINDOWS
	std::string	mWineVersionString;
	bool		mUnderWine;
	bool		mInaccurateSleep;
#endif
};

class LLCPUInfo final : public LLSingleton<LLCPUInfo>
{
	friend class LLSingleton<LLCPUInfo>;

protected:
	LOG_CLASS(LLCPUInfo);

	LLCPUInfo();
	~LLCPUInfo() override;

public:
	// When passed update_freq is true, checks for the affected core frequency,
	// if possible, to try and give a better stat in the returned CPU string).
	// HB
	std::string getCPUString(bool update_freq = false);

	// Returns the CPU family (e.g."AMD K8" or "Intel Pentium Pro").
	LL_INLINE const std::string& getFamily() const	{ return mFamily; }

	LL_INLINE bool hasSSE2() const					{ return mHasSSE2; }
	LL_INLINE bool hasSSE3() const					{ return mHasSSE3; }
	LL_INLINE bool hasSSE3S() const					{ return mHasSSE3S; }
	LL_INLINE bool hasSSE41() const					{ return mHasSSE41; }
	LL_INLINE bool hasSSE42() const					{ return mHasSSE42; }
	LL_INLINE bool hasSSE4a() const					{ return mHasSSE4a; }
	LLSD getSSEVersions() const;

	LL_INLINE F64 getMHz() const					{ return mCPUMHz; }

	std::string getInfo() const;

	LL_INLINE U32 getPhysicalCores()				{ return mPhysicalCores; }
	LL_INLINE U32 getVirtualCores()					{ return mVirtualCores; }
	// Returns the maximum number of child threads the process needs to launch
	// to saturate all the available cores. This is in excess of cores that
	// should be reserved for the main program render loop and any OpenGL
	// threaded driver. A minimum concurrency of 1 thread is always returned.
	LL_INLINE U32 getMaxThreadConcurrency()			{ return mMaxChildThreads; }

	// Returns the CPU single-core performance relatively to a 9700K @ 5GHz
	// as a factor (1.f = same perfs, larger factor = better perfs).
	F32 benchmarkFactor();

	// The following methods are no-operations under macOS (which does not
	// provide a way to set threads affinity). HB

	// Emits its own set of llinfos and llwarns, so no need for a returned
	// success boolean.
	static void setMainThreadCPUAffinifty(U32 cpu_mask);
	// Returns 1 when successful, 0 when failed, -1 when waiting for main
	// thread affinity to be set (i.e. when it should be retried). When a
	// name is passed, the method warns whenever it could not set the thread
	// affinity.
	static S32 setThreadCPUAffinity(const char* name = NULL);

private:
	// Checks for the affected core frequency (when possible) and returns the
	// delta in MHz compared with the frequency cached in mCPUMHz (the latter
	// being updated accordingly by this method). NOTE: for now we reject
	// negative deltas (i.e. we only account for the max turbo frequency, which
	// is normally the one used during rendering). HB
	S32 affectedCoreFreqDelta();

private:
	LLProcessorInfo*	mImpl;
	std::string			mFamily;
	std::string			mCPUString;
	F64					mCPUMHz;
	U32					mPhysicalCores;
	U32					mVirtualCores;
	U32					mMaxChildThreads;
	bool				mHasSSE2;
	bool				mHasSSE3;
	bool				mHasSSE3S;
	bool				mHasSSE41;
	bool				mHasSSE42;
	bool				mHasSSE4a;

	static U32			sMainThreadAffinityMask;
	static bool			sMainThreadAffinitySet;
};

#endif // LL_LLSYS_H
