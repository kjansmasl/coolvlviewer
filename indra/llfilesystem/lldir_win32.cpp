/**
 * @file lldir_win32.cpp
 * @brief Implementation of directory utilities for windows
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 *
 * Copyright (c) 2002-2009, Linden Research, Inc.
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

#include "linden_common.h"

#include <shlobj.h>
#include <direct.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "lldir_win32.h"

LLDir_Win32::LLDir_Win32()
{
	WCHAR w_str[MAX_PATH];

	// Application Data is where user settings go
	SHGetFolderPath(NULL, CSIDL_APPDATA | CSIDL_FLAG_CREATE, NULL,
					SHGFP_TYPE_DEFAULT, w_str);
	mOSBaseAppDir = ll_convert_wide_to_string(w_str);

	// This is the user's directory
	SHGetFolderPathW(NULL, CSIDL_PROFILE, NULL, 0, w_str);
	mOSUserDir = ll_convert_wide_to_string(w_str);

	// We want cache files to go on the local disk, even if the user is on a
	// network with a "roaming profile": E.g. C:\Users\James\AppData\Local
	SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE, NULL,
					SHGFP_TYPE_DEFAULT, w_str);
	mOSCacheDir = ll_convert_wide_to_string(w_str);

	if (GetTempPath(MAX_PATH, w_str))
	{
		if (wcslen(w_str))
		{
			w_str[wcslen(w_str) - 1] = '\0';  // Remove trailing slash
		}
		mTempDir = ll_convert_wide_to_string(w_str);
	}
	else
	{
		mTempDir = mOSBaseAppDir;
	}

#if 1
	// Do not use the real app path for now, as we will have to add parsing to
	// detect if we are in a developer tree, which has a different structure
	// from the installed product.

	S32 size = GetModuleFileName(NULL, w_str, MAX_PATH);
	if (size)
	{
		w_str[size] = '\0';
		mExecutablePathAndName = ll_convert_wide_to_string(w_str);
		size_t path_end = mExecutablePathAndName.find_last_of('\\');
		if (path_end != std::string::npos)
		{
			mExecutableDir = mExecutablePathAndName.substr(0, path_end);
			mExecutableFilename = mExecutablePathAndName.substr(path_end + 1);
		}
		else
		{
			mExecutableFilename = mExecutablePathAndName;
		}
		GetCurrentDirectory(MAX_PATH, w_str);
		mWorkingDir = ll_convert_wide_to_string(w_str);
	}
	else
	{
		fprintf(stderr,
				"Could not get APP path, assuming current directory !");
		GetCurrentDirectory(MAX_PATH, w_str);
		mExecutableDir = ll_convert_wide_to_string(w_str);
		// Assume it is the current directory
	}
#else
	GetCurrentDirectory(MAX_PATH, w_str);
	mExecutableDir = utf16str_to_utf8str(llutf16string(w_str));
#endif

	mAppRODataDir = mExecutableDir;

	// Build the default cache directory
	mDefaultCacheDir = buildSLOSCacheDir();

	// Make sure it exists
	if (!LLFile::mkdir(mDefaultCacheDir))
	{
		llwarns << "Could not create LL_PATH_CACHE dir " << mDefaultCacheDir
				<< llendl;
	}

	mLLPluginDir = mExecutableDir + "\\llplugin";

	dumpCurrentDirectories();
}

void LLDir_Win32::initAppDirs(const std::string& app_name)
{
	mAppName = app_name;
	mOSUserAppDir = mOSBaseAppDir;
	mOSUserAppDir += "\\";
	mOSUserAppDir += app_name;

	if (!LLFile::mkdir(mOSUserAppDir))
	{
		llwarns << "Could not create app user dir " << mOSUserAppDir
				<< " - Default to base dir " << mOSBaseAppDir << llendl;
		mOSUserAppDir = mOSBaseAppDir;
	}

	if (!LLFile::mkdir(getExpandedFilename(LL_PATH_LOGS, "")))
	{
		llwarns << "Could not create LL_PATH_LOGS dir "
				<< getExpandedFilename(LL_PATH_LOGS, "") << llendl;
	}

	if (!LLFile::mkdir(getExpandedFilename(LL_PATH_USER_SETTINGS, "")))
	{
		llwarns << "Could not create LL_PATH_USER_SETTINGS dir "
				<< getExpandedFilename(LL_PATH_USER_SETTINGS, "") << llendl;
	}

	if (!LLFile::mkdir(getExpandedFilename(LL_PATH_CACHE,"")))
	{
		llwarns << "Could not create LL_PATH_CACHE dir "
				<< getExpandedFilename(LL_PATH_CACHE, "") << llendl;
	}

	mCRTFile = getExpandedFilename(LL_PATH_APP_SETTINGS, "ca-bundle.crt");

	dumpCurrentDirectories();
}

std::string LLDir_Win32::getCurPath()
{
	WCHAR w_str[MAX_PATH];
	GetCurrentDirectory(MAX_PATH, w_str);

	return ll_convert_wide_to_string(w_str);
}

//virtual
std::string LLDir_Win32::getLLPluginLauncher()
{
	return gDirUtilp->getExecutableDir() + "\\SLPlugin.exe";
}

//virtual
std::string LLDir_Win32::getLLPluginFilename(std::string base_name)
{
	return gDirUtilp->getLLPluginDir() + "\\" + base_name + ".dll";
}

#endif	// LL_WINDOWS
