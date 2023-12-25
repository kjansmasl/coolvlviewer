/**
 * @file lldir_linux.cpp
 * @brief Implementation of directory utilities for linux
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

#include "linden_common.h"

#include <dirent.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>

#include "lldir_linux.h"

static std::string getCurrentUserHome(char* fallback)
{
	const uid_t uid = getuid();
	struct passwd* pw;
	char* result_cstr = fallback;

	pw = getpwuid(uid);
	if (pw && pw->pw_dir)
	{
		result_cstr = (char*)pw->pw_dir;
	}
	else
	{
		llinfos << "Could not detect home directory from passwd - trying $HOME"
				<< llendl;
		const char* const home_env = getenv("HOME");
		if (home_env)
		{
			result_cstr = (char*)home_env;
		}
		else
		{
			llwarns << "Could not detect home directory !  Falling back to: "
					<< fallback << llendl;
		}
	}

	return std::string(result_cstr);
}

LLDir_Linux::LLDir_Linux()
{
	mTempDir.clear();
	char* env = getenv("TMP");
	if (!env)
	{
		env = getenv("TMPDIR");
	}
	if (env)
	{
		mTempDir.assign(env);
		size_t length = mTempDir.length();
		if (length && mTempDir[length - 1] == '/')
		{
			mTempDir = mTempDir.substr(0, length - 1);
		}
	}
	if (mTempDir.empty())
	{
		mTempDir = "/tmp";
	}

	char tmp_str[LL_MAX_PATH];
	if (getcwd(tmp_str, LL_MAX_PATH) == NULL)
	{
		strncpy(tmp_str, mTempDir.c_str(), LL_MAX_PATH - 1);
		tmp_str[LL_MAX_PATH - 1] = '\0';
		llwarns << "Could not get current directory; changing to " << tmp_str
				<< llendl;
		if (chdir(tmp_str) == -1)
		{
			llerrs << "Could not change directory to " << tmp_str << llendl;
		}
	}

	mExecutableFilename.clear();
	mExecutablePathAndName.clear();
	mExecutableDir = tmp_str;
	mWorkingDir = tmp_str;
	mAppRODataDir = tmp_str;
	mOSUserDir = getCurrentUserHome(tmp_str);
	mOSUserAppDir.clear();

	char path[32];

	// *NOTE: /proc/%d/exe would not work on FreeBSD, but this is the
	// Linux implementation, and it works this way.
	snprintf(path, sizeof(path), "/proc/%d/exe", (int)getpid());
	int rc = readlink(path, tmp_str, sizeof(tmp_str) - 1);
	if (rc != -1 && rc <= (int)sizeof(tmp_str) - 1)
	{
		tmp_str[rc] = '\0';	// readlink() does not 0-terminate the buffer
		mExecutablePathAndName = tmp_str;
		char* path_end;
		if ((path_end = strrchr(tmp_str,'/')))
		{
			*path_end = '\0';
			mExecutableDir = tmp_str;
			mWorkingDir = tmp_str;
			mExecutableFilename = path_end + 1;
		}
		else
		{
			mExecutableFilename = tmp_str;
		}
	}

	mLLPluginDir = mExecutableDir + "/llplugin";

	dumpCurrentDirectories();
}

void LLDir_Linux::initAppDirs(const std::string& app_name)
{
	mAppName = app_name;

	std::string upper_app_name(app_name);
	LLStringUtil::toUpper(upper_app_name);

	char* app_home_env = getenv((upper_app_name + "_USER_DIR").c_str());
	if (app_home_env)
	{
		// User has specified own userappdir i.e. $SECONDLIFE_USER_DIR
		mOSUserAppDir = app_home_env;
	}
	else
	{
		// Traditionally on unixoids, MyApp gets ~/.myapp dir for data
		std::string lower_app_name(app_name);
		LLStringUtil::toLower(lower_app_name);
		mOSUserAppDir = mOSUserDir + "/." + lower_app_name;
	}

	// Create any directories we expect to write to.

	if (!LLFile::mkdir(mOSUserAppDir))
	{
		llwarns << "Could not create app user dir: " << mOSUserAppDir
				<< " - Default to base dir: " << mOSUserDir << llendl;
		mOSUserAppDir = mOSUserDir;
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

	if (!LLFile::mkdir(getExpandedFilename(LL_PATH_CACHE, "")))
	{
		llwarns << "Could not create LL_PATH_CACHE dir "
				<< getExpandedFilename(LL_PATH_CACHE, "") << llendl;
	}

	mCRTFile = getExpandedFilename(LL_PATH_APP_SETTINGS, "ca-bundle.crt");

	dumpCurrentDirectories();
}

std::string LLDir_Linux::getCurPath()
{
	char tmp_str[LL_MAX_PATH];
	if (getcwd(tmp_str, LL_MAX_PATH) == NULL)
	{
		llwarns << "Could not get current directory" << llendl;
		tmp_str[0] = '\0';
	}
	return tmp_str;
}

//virtual
std::string LLDir_Linux::getLLPluginLauncher()
{
	return gDirUtilp->getExecutableDir() + "/llplugin/SLPlugin";
}

//virtual
std::string LLDir_Linux::getLLPluginFilename(std::string base_name)
{
	return gDirUtilp->getLLPluginDir() + "/" + base_name + ".so";
}
