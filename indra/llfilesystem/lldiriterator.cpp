/**
 * @file lldiriterator.cpp
 * @brief Implementation of directory iterator class
 *
 * $LicenseInfo:firstyear=2010&license=viewergpl$
 *
 * Copyright (c) 2010, Linden Research, Inc. (c) 2021 Henri Beauchamp.
 *
 * Modifications by Henri Beauchamp:
 *  - Allow a simple iterator without matching pattern.
 *  - Allow iterating on entries that do *not* match the given pattern.
 *  - Allow to return sundry information for each found entry.
 *  - Added LLDirIterator::deleteFilesInDir().
 *  - Added LLDirIterator::deleteRecursivelyInDir().
 *  - Proper catching of throw()s and boost::filesystem errors.
 *  - Got rid of boost::regex in favour of std::regex since we now use C++11.
 *  - Added support for iterating on logical drives (when passed an empty
 *    path), under Windows.
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

#include <regex>

#include "boost/filesystem.hpp"

#include "lldiriterator.h"

#include "llstring.h"

using namespace boost::filesystem;

///////////////////////////////////////////////////////////////////////////////
// LLDirIterator::Impl class
// Hides all the gory details and saves from including verbose header files by
// code using LLDirIterator...
///////////////////////////////////////////////////////////////////////////////

class LLDirIterator::Impl
{
protected:
	LOG_CLASS(LLDirIterator::Impl);

public:
	LL_INLINE Impl(const directory_iterator& dir_iter, U32 requested_info)
	:	mIter(dir_iter),
		mRequestedInfo(requested_info),
		mGotFilter(false),
		mIsFile(false),
		mIsDirectory(false),
		mIsLink(false),
		mIsHidden(false),
#if LL_WINDOWS
		mIsDriveIterator(false),
		mNextDrive(0),
#endif
		mSize(0),
		mTimeStamp(0)
	{
	}

	LL_INLINE void setFilter(const std::regex& regexp)
	{
		mGotFilter = true;
		mFilterExp.assign(regexp);
	}

#if LL_WINDOWS
	LL_INLINE void setDriveIterator()
	{
		mIsDriveIterator = true;
	}
#endif

	bool next(std::string& name, bool not_matching);

	LL_INLINE bool isFile() const
	{
		checkRequestedInfo(DI_ISFILE);
		return mIsFile;
	}

	LL_INLINE bool isDirectory() const
	{
		checkRequestedInfo(DI_ISDIR);
		return mIsDirectory;
	}

	LL_INLINE bool isLink() const
	{
		checkRequestedInfo(DI_ISLINK);
		return mIsLink;
	}

	LL_INLINE bool isHidden() const
	{
		checkRequestedInfo(DI_ISHIDDEN);
		return mIsHidden;
	}

	LL_INLINE size_t getSize() const
	{
		checkRequestedInfo(DI_SIZE);
		return mSize;
	}

	LL_INLINE time_t getTimeStamp() const
	{
		checkRequestedInfo(DI_TIMESTAMP);
		return mTimeStamp;
	}

	static std::string globPatternToRegex(const char* glob);

private:
	LL_INLINE bool hasRequestedInfo(U32 info) const
	{
		return (mRequestedInfo & info) != 0;
	}

	// Do not let the compiler inline the llerrs (would waste CPU cache)
	LL_NO_INLINE void checkRequestedInfo(U32 info) const;

	void populateEntryInfo();

private:
	directory_iterator	mIter;
	std::regex			mFilterExp;
	size_t				mSize;
	time_t				mTimeStamp;
	U32					mRequestedInfo;
#if LL_WINDOWS
	U8					mNextDrive;
	bool				mIsDriveIterator;
#endif
	bool				mGotFilter;
	bool				mIsFile;
	bool				mIsDirectory;
	bool				mIsLink;
	bool				mIsHidden;
};

//static
std::string LLDirIterator::Impl::globPatternToRegex(const char* glob)
{
	size_t glob_len = strlen(glob);
	std::string expr;
	expr.reserve(glob_len * 2);
	S32 braces = 0;
	bool escaped = false;
	bool square_brace_open = false;

	for (size_t i = 0; i < glob_len; ++i)
	{
		const char& c = glob[i];
		switch (c)
		{
			case '*':
				if (i == 0)
				{
					expr = "[^.].*";
				}
				else
				{
					expr += escaped ? "*" : ".*";
				}
				break;

			case '?':
				expr += escaped ? '?' : '.';
				break;

			case '{':
				++braces;
				expr += '(';
				break;

			case '}':
				if (--braces < 0)
				{
					llerrs << "Closing brace without an equivalent opening brace in: "
						   << glob << llendl;
				}
				expr += ')';
				break;

			case ',':
				expr += braces ? '|' : c;
				break;

			case '!':
				expr += square_brace_open ? '^' : c;
				break;

			case '.':
			case '^':
			case '(':
			case ')':
			case '+':
			case '|':
			case '$':
				expr += '\\';
				// And fall-through...

			default:
				expr += c;
		}

		escaped = c == '\\';
		square_brace_open = c == '[';
	}

	if (braces)
	{
		llerrs << "Unterminated brace expression in: " << glob << llendl;
	}

	return expr;
}

void LLDirIterator::Impl::checkRequestedInfo(U32 info) const
{
	if (!(mRequestedInfo & info))
	{
		llerrs << "Bad info request: " << info << llendl;
	}
}

void LLDirIterator::Impl::populateEntryInfo()
{
	// Do this first, since it does not risk throw()ing at our face...
	if (hasRequestedInfo(DI_ISHIDDEN))
	{
#if LL_WINDOWS
		DWORD attr = GetFileAttributesA(mIter->path().string().c_str());
		mIsHidden = (attr & FILE_ATTRIBUTE_HIDDEN) != 0;
#else
		// Note: gcc v12.1 detects "dangling pointer to an unnamed temporary
		// may be used" when trying to use a const std::string& name here... HB
		std::string name = mIter->path().filename().string();
		mIsHidden = !name.empty() && name[0] == '.';
#endif
		if (mRequestedInfo == DI_ISHIDDEN)
		{
			return;	// No need to enter the try {} catch {} ...
		}
	}
	try
	{
		bool want_size = hasRequestedInfo(DI_SIZE);
		if (want_size || hasRequestedInfo(DI_ISFILE))
		{
			mIsFile = is_regular_file(mIter->path());
		}
		if (hasRequestedInfo(DI_ISDIR))
		{
			mIsDirectory = is_directory(mIter->path());
		}
		if (hasRequestedInfo(DI_ISLINK))
		{
			mIsLink = is_symlink(mIter->path());
		}
		if (want_size)
		{
			mSize = mIsFile ? file_size(mIter->path()) : 0;
		}
		if (hasRequestedInfo(DI_TIMESTAMP))
		{
			mTimeStamp = last_write_time(mIter->path());
		}
	}
	catch (const filesystem_error& e)
	{
		llwarns << e.what() << llendl;
	}
}

bool LLDirIterator::Impl::next(std::string& name, bool not_matching)
{
#if LL_WINDOWS
	if (mIsDriveIterator)
	{
		// Note: we ignore name matching since we iterate on drives and not
		// on files.
		name.clear();
		mIsDirectory = false;

		if (mNextDrive >= 26)
		{
			return false;
		}

		DWORD drives_map = GetLogicalDrives();
		for (U8 i = mNextDrive; i < 26; ++i)
		{
			if (drives_map & (1L << i))
			{
				char volume = 'A' + (char)i;
				name = volume;
				name += ':';
				mIsDirectory = true;
				mNextDrive = i + 1;
				break;
			}
		}

		return mIsDirectory;
	}
#endif

	bool found = false;

	directory_iterator end; // Default construction yields past-the-end

	if (mGotFilter)	// Iterator with glob pattern matching
	{
		try
		{
			boost::system::error_code ec;
			std::smatch match;
			while (mIter != end)
			{
				name = mIter->path().filename().string();
				if (name == "." || name == "..")
				{
					continue;
				}
				found = std::regex_match(name, match, mFilterExp);
				if (not_matching)
				{
					// We want entries not matching the pattern (unless they
					// are symbolic links).
					found = !found || is_symlink(mIter->path(), ec);
				}
				if (found)
				{
					break;
				}
				++mIter;
			}
		}
		catch (const std::regex_error& e)
		{
			llwarns << e.what() << llendl;
		}
	}
	// Simple iterator without matching glob pattern
	else
	{
		while (mIter != end)
		{
			name = mIter->path().filename().string();
			if (name != "." && name != "..")
			{
				found = true;
				break;
			}
			++mIter;
		}
	}

	if (found)
	{
		if (mRequestedInfo)
		{
			populateEntryInfo();
		}
	}
	else
	{
		name.clear();
		if (mRequestedInfo)
		{
			mIsFile = mIsDirectory = mIsLink = mIsHidden = false;
			mTimeStamp = 0;
		}
	}

	// Now that we got all the needed info, we can increment the iterator, if
	// not already at end...
	if (mIter != end)
	{
		++mIter;
	}

	return found;
}

///////////////////////////////////////////////////////////////////////////////
// LLDirIterator class proper
///////////////////////////////////////////////////////////////////////////////

// Helper function
static void append_separator_if_needed(std::string& dirname)
{
#if LL_WINDOWS
	if (dirname.back() != '\\')
	{
		dirname += '\\';
	}
#else
	if (dirname.back() != '/')
	{
		dirname += '/';
	}
#endif
}

// NOTE: I chose to initialize the iterator and regexp inside LLDirIterator()
// instead of LLDirIterator::Impl::Impl(), because I want the warnings to be
// identified as coming from the former in the log file; only llerrs (which
// shall never be seen happening by the end users) might come from the
// implementation sub-class. The mImpl pointer is also used as a "flag" to
// denote a successful initialization (stays NULL when init fails). HB
LLDirIterator::LLDirIterator(const std::string& dirname, const char* mask,
							 U32 requested_info)
:	mImpl(NULL)
{
	if (dirname.empty())
	{
#if LL_WINDOWS
		// When iterating on an empty path under Windows, we actually want the
		// list of the existing logical drives... In this case we ignore the
		// matching pattern, if any.
		directory_iterator iter;
		mImpl = new Impl(iter, requested_info);
		mImpl->setDriveIterator();
		return;
#else
		llwarns << "Invalid (empty) path." << llendl;
		return;
#endif
	}

#if LL_WINDOWS
	path dir_path(ll_convert_string_to_wide(dirname));
#else
	path dir_path(dirname);
#endif

	// Check if dir_path is a directory.
	boost::system::error_code ec;
	if (!is_directory(dir_path, ec) || ec.failed())
	{
		if (ec.failed())
		{
			llwarns << "Invalid path: " << dirname << " - Error: "
					<< ec.message() << llendl;
		}
		else
		{
			llwarns << "Invalid path: " << dirname << llendl;
		}
		return;
	}

	mDirPath = dir_path.string();
	append_separator_if_needed(mDirPath);

	// Initialize the directory iterator for the given path.
	directory_iterator iter(dir_path, ec);
	if (ec.failed())
	{
		llwarns << "Directory: " << mDirPath
				<< " - Error while creating iterator: " << ec.message()
				<< llendl;
		return;
	}

	std::regex regexp;
	bool has_glob = mask && *mask;
	if (has_glob)
	{
		// Convert the glob mask into a regular expression string
		std::string expr = Impl::globPatternToRegex(mask);
		// Initialize regexp with the expression converted from the glob mask
		try
		{
			regexp.assign(expr);
		}
		catch (const std::regex_error& e)
		{
			llwarns << "\"" << expr << "\" is not a valid regular expression: "
					<< e.what() << " - Search match global pattern was: "
					<< mask << llendl;
			return;
		}
	}

	mImpl = new Impl(iter, requested_info);
	if (has_glob)
	{
		mImpl->setFilter(regexp);
	}
}

LLDirIterator::~LLDirIterator()
{
	delete mImpl;
}

bool LLDirIterator::next(std::string& name, bool not_matching)
{
	return mImpl && mImpl->next(name, not_matching);
}

bool LLDirIterator::isFile() const
{
	return mImpl && mImpl->isFile();
}

bool LLDirIterator::isDirectory() const
{
	return mImpl && mImpl->isDirectory();
}

bool LLDirIterator::isLink() const
{
	return mImpl && mImpl->isLink();
}

bool LLDirIterator::isHidden() const
{
	return mImpl && mImpl->isHidden();
}

size_t LLDirIterator::getSize() const
{
	return mImpl ? mImpl->getSize() : 0;
}

time_t LLDirIterator::getTimeStamp() const
{
	return mImpl ? mImpl->getTimeStamp() : 0;
}

//static
U32 LLDirIterator::deleteFilesInDir(const std::string& dirname,
									const char* mask, bool not_matching)
{
	if (not_matching && !(mask && *mask))
	{
		// Nothing to do !
		return 0;
	}

	LLDirIterator iter(dirname, mask, DI_ISDIR);
	if (!iter.isValid())
	{
		return 0;
	}

	U32 count = 0;

	boost::system::error_code ec;
	std::string filename;
	while (iter.next(filename, not_matching))
	{
		if (!iter.isDirectory())	// We remove files and links alike
		{
			++count;
			remove(iter.getPath() + filename, ec);
			if (ec.failed())
			{
				--count;
				llwarns << "Failure to remove \"" << filename << "\". Reason: "
						<< ec.message() << llendl;
			}
		}
	}

	return count;
}

//static
U32 LLDirIterator::deleteRecursivelyInDir(const std::string& dirname,
										  const char* mask, bool not_matching)
{
	if (not_matching && !(mask && *mask))
	{
		// Nothing to do !
		return 0;
	}

	LLDirIterator iter(dirname, mask, DI_ISDIR);
	if (!iter.isValid())
	{
		return 0;
	}

	U32 count = 0;

	boost::system::error_code ec;
	std::string name, subdir;
	while (iter.next(name, not_matching))
	{
		if (iter.isDirectory())
		{
			subdir = dirname;
			append_separator_if_needed(subdir);
			subdir += name;
			count += deleteRecursivelyInDir(subdir, mask, not_matching);
			// That subdirectory is now empty and can be removed (excepted,
			// maybe, when not_matching is true and a corresponding file was
			// found in it, but then the directory removal will simply fail).
			LLFile::rmdir(subdir);
		}
		else	// We remove files and links alike
		{
			++count;
			remove(iter.getPath() + name, ec);
			if (ec.failed())
			{
				--count;
				llwarns << "Failure to remove \"" << name << "\". Reason: "
						<< ec.message() << llendl;
			}
		}
	}

	return count;
}
