/**
 * @file llmediaimplgstreamer_syms.cpp
 * @brief dynamic GStreamer symbol-grabbing code
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 *
 * Copyright (c) 2007-2009, Linden Research, Inc.
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

#if LL_WINDOWS
# undef _WIN32_WINNT
# define _WIN32_WINNT 0x0502
#endif

#include "linden_common.h"

#include <iostream>
#include <stdlib.h>

#if LL_DARWIN
// For stat()
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#if LL_CLANG
// When using C++11, clang warns about the "register" type usage in gst/gst.h
// (actually stemming from glib-2.0/gobject/gtype.h), while this is a C
// header (and gcc is fine with it)... And with C++17, clang just errors out...
// So, let's nullify the "register" keyword before including the headers. HB
# define register
#endif

#include "gst/gst.h"
#include "gst/app/gstappsink.h"

#include "apr_pools.h"
#include "apr_dso.h"

#ifdef LL_WINDOWS
std::string getGStreamerDir()
{
	std::string ret;
	HKEY hKey;

#ifdef _WIN64
	if (::RegOpenKeyExA(HKEY_LOCAL_MACHINE, "Software\\Wow6432Node\\GStreamer1.0\\x86_64", 0,
						KEY_QUERY_VALUE , &hKey) == ERROR_SUCCESS)
#else
	if (::RegOpenKeyExA(HKEY_LOCAL_MACHINE, "Software\\GStreamer1.0\\x86", 0,
						KEY_QUERY_VALUE , &hKey) == ERROR_SUCCESS)
#endif
	{
		DWORD dwLen(0);
		::RegQueryValueExA(hKey, "InstallDir", NULL, NULL, NULL, &dwLen);
		if (dwLen > 0)
		{
			std::vector<char> vctBuffer;
			vctBuffer.resize(dwLen);
			::RegQueryValueExA(hKey, "InstallDir", NULL, NULL,
							   reinterpret_cast<LPBYTE>(&vctBuffer[0]),
							   &dwLen);
			ret = &vctBuffer[0];

			if (ret[dwLen - 1] != '\\')
			{
				ret += "\\";
			}
#ifdef _WIN64
			ret += "1.0\\x86_64\\bin\\";
#else
			ret += "1.0\\x86\\bin\\";
#endif

			SetDllDirectoryA(ret.c_str());
		}
		::RegCloseKey(hKey);
	}

	if (ret.empty())
	{
#ifdef _WIN64
		char* path = getenv("GSTREAMER_SDK_ROOT_X86_64");
#else
		char* path = getenv("GSTREAMER_SDK_ROOT_X86");
#endif
		if (path)
		{
			ret.assign(path);
			if (ret.back() != '\\')
			{
				ret += '\\';
			}
			ret += "bin\\";
			SetDllDirectoryA(ret.c_str());
		}
	}

	return ret;
}
#elif LL_DARWIN
#define GST_PATH "/Library/Frameworks/GStreamer.framework/Versions/1.0/lib/"

std::string getGStreamerDir()
{
	struct stat info;

	std::string path = GST_PATH;
	if (stat(path.c_str(), &info) == 0)
	{
		return path;
	}

	path = "~" + path;
	if (stat(path.c_str(), &info) == 0)
	{
		return path;
	}

	std::cerr << "Cannot find the GStreamer framework" << std::endl;
	return "";
}
#else // Linux or other ELFy unixoid
std::string getGStreamerDir()
{
	return "";
}
#endif

#define LL_GST_SYM(REQ, GSTSYM, RTN, ...) RTN (*ll##GSTSYM)(__VA_ARGS__) = NULL;
#include "llmediaimplgstreamer_syms_raw.inc"
#undef LL_GST_SYM

struct Symloader
{
	bool mRequired;
	char const* mName;
	apr_dso_handle_sym_t* mPPFunc;
};

#define LL_GST_SYM(REQ, GSTSYM, RTN, ...) { REQ, #GSTSYM , (apr_dso_handle_sym_t*)&ll##GSTSYM},
Symloader sSyms[] = {
#include "llmediaimplgstreamer_syms_raw.inc"
{ false, 0, 0 } };
#undef LL_GST_SYM

// a couple of stubs for disgusting reasons
GstDebugCategory* ll_gst_debug_category_new(gchar* name, guint color,
											gchar* description)
{
	static GstDebugCategory dummy;
	return &dummy;
}

void ll_gst_debug_register_funcptr(GstDebugFuncPtr func, gchar* ptrname)
{
}

static bool sSymsGrabbed = false;
static apr_pool_t* sSymGSTDSOMemoryPool = NULL;

std::vector<apr_dso_handle_t*> sLoadedLibraries;

bool grab_gst_syms(const std::vector<std::string>& dso_names)
{
	if (sSymsGrabbed)
	{
		return true;
	}

	// attempt to load the shared libraries
	apr_pool_create(&sSymGSTDSOMemoryPool, NULL);

	std::string dso_name;
	for (std::vector<std::string>::const_iterator itr = dso_names.begin(),
												  end = dso_names.end();
		 itr != end; ++itr)
	{
		apr_dso_handle_t* dsop = NULL;
		dso_name = getGStreamerDir() + *itr;
		if (APR_SUCCESS == apr_dso_load(&dsop, dso_name.c_str(),
										sSymGSTDSOMemoryPool))
		{
			sLoadedLibraries.push_back(dsop);
		}

		for (int i = 0; sSyms[i].mName; ++i)
		{
			if (!*sSyms[i].mPPFunc)
			{
				apr_dso_sym(sSyms[i].mPPFunc, dsop, sSyms[i].mName);
			}
		}
	}

	std::stringstream strm;
	bool sym_error = false;
	for (int i = 0; sSyms[i].mName; ++i)
	{
		if (sSyms[i].mRequired && !*sSyms[i].mPPFunc)
		{
			sym_error = true;
			strm << sSyms[i].mName << " " << std::endl;
		}
	}

	if (sym_error)
	{
		std::cerr << "Failed to load the following symbols: " << strm.str()
				  << std::endl;
	}

	sSymsGrabbed = !sym_error;

	return sSymsGrabbed;
}

// Should be safe to call regardless of whether we have actually grabbed syms.
void ungrab_gst_syms()
{
	for (std::vector<apr_dso_handle_t*>::iterator
			itr = sLoadedLibraries.begin(), end = sLoadedLibraries.end();
		 itr != end; ++itr)
	{
		apr_dso_unload(*itr);
	}
	sLoadedLibraries.clear();

	if (sSymGSTDSOMemoryPool)
	{
		apr_pool_destroy(sSymGSTDSOMemoryPool);
		sSymGSTDSOMemoryPool = NULL;
	}

	for (int i = 0; sSyms[i].mName; ++i)
	{
		*sSyms[i].mPPFunc = NULL;
	}

	sSymsGrabbed = false;
}
