/**
 * @file lldir_mac.cpp
 * @brief Implementation of directory utilities for Mac OS X
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

#if LL_DARWIN

#include "linden_common.h"

#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <unistd.h>
#include <uuid/uuid.h>

#include "boost/filesystem.hpp"

#include "lldir_mac.h"

#include "lldir_utils_objc.h"

// Helper functions

static bool macos_create_dir(const std::string& parent,
							 const std::string& child,
							 std::string* fullname = NULL)
{
	boost::filesystem::path p(parent);
	p /= child;

	if (fullname)
	{
		*fullname = std::string(p.string());
	}

	if (!boost::filesystem::create_directory(p))
	{
		return boost::filesystem::is_directory(p);
	}
    return true;
}

static std::string get_user_home()
{
	const uid_t uid = getuid();
	struct passwd* pw;
	char* result_cstr = (char*)"/Users";

	pw = getpwuid(uid);
	if (pw && pw->pw_dir)
	{
		result_cstr = (char*)pw->pw_dir;
	}
	else
	{
		llinfos << "Could not detect home directory from passwd; trying $HOME"
				<< llendl;
		const char* const home_env = getenv("HOME");
		if (home_env)
		{
			result_cstr = (char*)home_env;
		}
		else
		{
			llwarns << "Could not detect home directory !  Falling back to: /Users "
					<< llendl;
		}
	}

	return std::string(result_cstr);
}

// ----------------------------------------------------------------------------
// LLDir_Mac class
// ----------------------------------------------------------------------------

LLDir_Mac::LLDir_Mac()
{
	const std::string second_life_string = "SecondLife";
	const std::string tpv_string = "CoolVLViewer";
	std::string* executablepathstr = getSystemExecutableFolder();

	if (executablepathstr)
	{
		// mExecutablePathAndName
		mExecutablePathAndName = *executablepathstr;

		boost::filesystem::path executablepath(*executablepathstr);

		// mExecutableFilename && mExecutableDir
		mExecutableFilename = executablepath.filename().string();
		mExecutableDir = executablepath.parent_path().string();

		// mAppRODataDir
		std::string* resourcepath = getSystemResourceFolder();
		mAppRODataDir = *resourcepath;

		// mOSUserDir
		mOSUserDir = get_user_home();

		std::string* appdir = getSystemApplicationSupportFolder();
		std::string rootdir;

		// Create root directory
		if (macos_create_dir(*appdir, second_life_string, &rootdir))
		{
			// mOSUserAppDir
			mOSUserAppDir = rootdir;

			// Create our sub-dirs
			macos_create_dir(rootdir, "data");
			macos_create_dir(rootdir, "logs");
			macos_create_dir(rootdir, "user_settings");
			macos_create_dir(rootdir, "browser_profile");
		}
		else
		{
			// mOSUserAppDir
			mOSUserAppDir = mOSUserDir;
		}

		// mOSCacheDir
		std::string* cachedir = getSystemCacheFolder();
		if (cachedir)
		{
			mOSCacheDir = *cachedir;
			macos_create_dir(mOSCacheDir, tpv_string);
		}


		// mTempDir
		std::string* tmpdir = getSystemTempFolder();
		if (tmpdir)
		{
			macos_create_dir(*tmpdir, tpv_string, &mTempDir);
			if (tmpdir)
			{
				delete tmpdir;
			}
		}

		mWorkingDir = getCurPath();

		mLLPluginDir = mAppRODataDir + "/llplugin";
	}

	dumpCurrentDirectories();
}

void LLDir_Mac::initAppDirs(const std::string& app_name)
{
	mAppName = app_name;
	mCRTFile = getExpandedFilename(LL_PATH_APP_SETTINGS, "ca-bundle.crt");

	dumpCurrentDirectories();
}

std::string LLDir_Mac::getCurPath()
{
	return boost::filesystem::path(boost::filesystem::current_path()).string();
}

//virtual
std::string LLDir_Mac::getLLPluginLauncher()
{
	return gDirUtilp->getAppRODataDir() +
		   "/SLPlugin.app/Contents/MacOS/SLPlugin";
}

//virtual
std::string LLDir_Mac::getLLPluginFilename(std::string base_name)
{
	return gDirUtilp->getLLPluginDir() + "/" + base_name + ".dylib";
}

#endif // LL_DARWIN
