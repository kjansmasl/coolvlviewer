/**
 * @file lldir.h
 * @brief Definition of directory utilities class
 *
 * $LicenseInfo:firstyear=2000&license=viewergpl$
 *
 * Copyright (c) 2000-2009, Linden Research, Inc.
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

#ifndef LL_LLDIR_H
#define LL_LLDIR_H

#include "llstring.h"

// These numbers *may* get serialized, so we need to be explicit
typedef enum ELLPath
{
	LL_PATH_NONE = 0,
	LL_PATH_USER_SETTINGS = 1,
	LL_PATH_APP_SETTINGS = 2,
	LL_PATH_PER_ACCOUNT = 3,
	LL_PATH_CACHE = 4,
	LL_PATH_CHARACTER = 5,
#if 0	// Not used
	LL_PATH_MOTIONS = 6,
#endif
	LL_PATH_HELP = 7,
	LL_PATH_LOGS = 8,
	LL_PATH_TEMP = 9,
	LL_PATH_SKINS = 10,
	LL_PATH_TOP_SKIN = 11,
	LL_PATH_CHAT_LOGS = 12,
	LL_PATH_PER_ACCOUNT_CHAT_LOGS = 13,
#if 0	// Deprecated
	LL_PATH_MOZILLA_PROFILE = 14,
	LL_PATH_HTML = 15,
#endif
	LL_PATH_EXECUTABLE = 16,
	LL_PATH_LAST
} ELLPath;

class LLDir
{
protected:
	LOG_CLASS(LLDir);

public:
	LLDir();
	virtual ~LLDir();

	virtual void initAppDirs(const std::string& app_name) = 0;

	virtual std::string getCurPath() = 0;

	std::string findFile(const std::string& filename,
						 const std::string& path1 = LLStringUtil::null,
						 const std::string& path2 = LLStringUtil::null,
						 const std::string& path3 = LLStringUtil::null,
						 const std::string& path4 = LLStringUtil::null) const;

	// Full path and name for the plugin wrapper executable (SLPlugin)
	virtual std::string getLLPluginLauncher() = 0;

	// Full path and name to the plugin DSO for this base_name (i.e.
	// 'FOO' -> '/bar/baz/libFOO.so')
	virtual std::string getLLPluginFilename(std::string base_name) = 0;

	// Full pathname of the executable
	LL_INLINE const std::string& getExecutablePathAndName() const
	{
		return mExecutablePathAndName;
	}

	// Directory where the executable is located
	LL_INLINE const std::string& getExecutableDir() const
	{
		return mExecutableDir;
	}

	// Filename of the executable
	LL_INLINE const std::string& getExecutableFilename() const
	{
		return mExecutableFilename;
	}

	// Directory containing plugins and plugin shell
	LL_INLINE const std::string& getLLPluginDir() const
	{
		return mLLPluginDir;
	}

	// Application name (SecondLife)
	LL_INLINE const std::string& getAppName() const
	{
		return mAppName;
	}

	// Current working directory
	LL_INLINE const std::string& getWorkingDir() const
	{
		return mWorkingDir;
	}

	// Location of read-only data files
	LL_INLINE const std::string& getAppRODataDir() const
	{
		return mAppRODataDir;
	}

	// Location of the OS-specific user directory
	LL_INLINE const std::string& getOSUserDir() const
	{
		return mOSUserDir;
	}

	// Location of the OS-specific user application directory
	LL_INLINE const std::string& getOSUserAppDir() const
	{
		return mOSUserAppDir;
	}

	// Location of the Linden user directory
	LL_INLINE const std::string& getLindenUserDir() const
	{
		return mLindenUserDir;
	}

	// Location of the chat logs directory
	LL_INLINE const std::string& getChatLogsDir() const
	{
		return mChatLogsDir;
	}

	// Location of the per-account chat logs directory.
	LL_INLINE const std::string& getPerAccountChatLogsDir() const
	{
		return mPerAccountChatLogsDir;
	}

	// Common temporary directory
	LL_INLINE const std::string& getTempDir() const
	{
		return mTempDir;
	}

	// Location of OS-specific cache folder (may be an empty string)
	LL_INLINE const std::string& getOSCacheDir() const
	{
		return mOSCacheDir;
	}

	// File containing TLS certificate authorities
	LL_INLINE const std::string& getCRTFile() const
	{
		return mCRTFile;
	}

	// Current (active) skin directory.
	LL_INLINE const std::string& getSkinDir() const
	{
		return mSkinDir;
	}

	// User-specified skin directory with user overrides. E.g.
	// ~/.secondlife/skins/silver
	LL_INLINE const std::string& getUserSkinDir() const
	{
		return mUserSkinDir;
	}

	// User-specified skin directory with user overrides. E.g.
	// ~/.secondlife/skins/default
	LL_INLINE const std::string& getUserDefaultSkinDir() const
	{
		return mUserDefaultSkinDir;
	}

	// Directory for the default skin. e.g.
	// /usr/local/CoolVLViewer/skins/default
	LL_INLINE const std::string& getDefaultSkinDir() const
	{
		return mDefaultSkinDir;
	}

	// Location of the cache.
	std::string getCacheDir(bool get_default = false) const;

	// Directory containing all the installed skins (not user overrides). E.g.
	// /usr/local/CoolVLViewer/skins
	std::string getSkinBaseDir() const;

	// Expanded filename
	std::string getExpandedFilename(ELLPath location,
									const std::string& filename) const;
	std::string getExpandedFilename(ELLPath location,
									const std::string& subdir,
									const std::string& filename) const;
	std::string getExpandedFilename(ELLPath location,
									const std::string& subdir1,
									const std::string& subdir2,
									const std::string& filename) const;

	// Base and directory name extraction
	std::string getBaseFileName(const std::string& filepath,
								bool strip_exten = false) const;
	std::string getDirName(const std::string& filepath) const;

	// Excludes '.', e.g getExtension("foo.wav") == "wav"
	std::string getExtension(const std::string& filepath) const;

	bool isRelativePath(const std::string& path) const;

	// These methods search the various skin paths for the specified file in
	// the following order: getUserSkinDir(), getSkinDir(), getDefaultSkinDir()
	std::string findSkinnedFilename(const std::string& filename) const;
	std::string findSkinnedFilename(const std::string& subdir,
									const std::string& filename) const;
	std::string findSkinnedFilename(const std::string& subdir1,
									const std::string& subdir2,
									const std::string& filename) const;

	// This is for use with files stored in LL_PATH_PER_ACCOUNT or
	// LL_PATH_USER_SETTINGS subdirectories (in this order of priority), or
	// even in mOSUserDir if the desired_subdir string starts with '~'. It is
	// currently only used for the preprocessor #include mechanism. It ensures
	// the file name and sub-directory names are scrubbed and that the latter
	// do not allow navigating upwards in the file system.
	// In case of failure to find an adequate file, it returns an empty string.
	// Any path separators in 'fallback_subdir' and 'filename' should always be
	// '/', even for Windows, and in the latter case, they get properly
	// replaced with '\' in the returned full path string. HB
	std::string getUserFilename(std::string desired_subdir,
								std::string fallback_subdir,
								std::string filename);

	// Returns a random filename in common temporary directory, without a
	// ".tmp" extension when passed false.
	std::string getTempFilename(bool with_extension = true) const;

	// For producing safe download directory or file names from potentially
	// unsafe ones

	static std::string getForbiddenDirChars();
	static std::string getForbiddenFileChars();
	static std::string getScrubbedDirName(const std::string& dirname);
	static std::string getScrubbedFileName(const std::string& filename);

	// Sets the chat logs dir to this user's dir
	void setChatLogsDir(const std::string& path);
	// Sets the per-account chat log directory.
	void setPerAccountChatLogsDir(const std::string& grid,
								  const std::string& first,
								  const std::string& last);

	// Sets the linden user dir to this user's dir
	void setLindenUserDir(const std::string& grid,
						  const std::string& first,
						  const std::string& last);

	void setSkinFolder(const std::string& skin_folder);
	bool setCacheDir(const std::string& path);

	void dumpCurrentDirectories();

	// Utility routine
	std::string buildSLOSCacheDir() const;

protected:
	std::string mAppName;				// Application name ("SecondLife")
	std::string mExecutablePathAndName;	// full path + Filename of .exe
	std::string mExecutableFilename;	// Filename of .exe
	std::string mExecutableDir;			// Location of executable
	std::string mWorkingDir;			// Current working directory
	std::string mAppRODataDir;			// Location for static app data
	std::string mOSUserDir;				// OS Specific user directory
	std::string mOSUserAppDir;			// OS Specific user app directory
	std::string mLindenUserDir;			// Location for user-specific data
	std::string mPerAccountChatLogsDir;	// Location for chat logs.
	std::string mChatLogsDir;			// Location for chat logs.
	std::string mCRTFile;				// Location of the TLS CRT file.
	std::string mTempDir;				// Temporary files directory.
	std::string mCacheDir;				// Cache directory as set by user
	std::string mDefaultCacheDir;		// default cache diretory
	std::string mOSCacheDir;			// Operating system cache dir
	std::string mSkinDir;				// Location for current skin info.
	std::string mDefaultSkinDir;		// Location for default skin info.
	std::string mUserSkinDir;			// Location for user-modified skin info
	std::string mUserDefaultSkinDir;	// User-modified default skin info
	std::string mLLPluginDir;			// Location for plugins

private:
	bool		mUsingDefaultSkin;		// true when using skin "default"
};

extern LLDir* gDirUtilp;

#endif // LL_LLDIR_H
