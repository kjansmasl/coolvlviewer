/**
 * @file llfile.cpp
 * @author Michael Schlachter
 * @date 2006-03-23
 * @brief Implementation of cross-platform POSIX file buffer and c++
 * stream classes.
 *
 * $LicenseInfo:firstyear=2006&license=viewergpl$
 *
 * Copyright (c) 2006-2009, Linden Research, Inc.
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

#include "linden_common.h"			// Also includes llfile.h

#if LL_WINDOWS
# include <stdlib.h>				// Windows errno
# include <io.h>                 	// _get_osfhandle()
// There is no symlink() C function for Windows: use C++17 instead.
# include <filesystem>
#else
# include <errno.h>
# include <fcntl.h>
# include <unistd.h>
#endif
#include <stdio.h>

#include <utility>

#include "zlib.h"

#include "llstring.h"

using namespace std;

//static
bool LLFile::sFlushOnWrite = false;

// Many of the methods below use OS-level functions that mess with errno. Wrap
// variants of strerror() to report errors.

#if LL_WINDOWS
// On Windows, use strerror_s().
std::string strerr(int errn)
{
	char buffer[256];
	strerror_s(buffer, errn);	// infers sizeof(buffer) -- love it !
	return buffer;
}

typedef std::basic_ios<char, std::char_traits<char> > _Myios;

#else
// On POSIX we want to call strerror_r(), but alarmingly, there are two
// different variants. The one that returns int always populates the passed
// buffer (except in case of error), whereas the other one always returns a
// valid char* but might or might not populate the passed buffer. How do we
// know which one we are getting ?  Define adapters for each and let the
// compiler select the applicable adapter.

// strerror_r() returns char*
std::string message_from(int orig_errno, const char* buffer, size_t bufflen,
						 const char* strerror_ret)
{
	return strerror_ret;
}

// strerror_r() returns int
std::string message_from(int orig_errno, const char* buffer, size_t bufflen,
						 int strerror_ret)
{
	if (strerror_ret == 0)
	{
		return buffer;
	}
	// Here strerror_r() has set errno. Since strerror_r() has already failed,
	// seems like a poor bet to call it again to diagnose its own error...
	int stre_errno = errno;
	if (stre_errno == ERANGE)
	{
		return llformat("strerror_r() cannot explain errno %d (%d-byte buffer too small)",
						orig_errno, bufflen);
	}
	if (stre_errno == EINVAL)
	{
		return llformat("unknown errno %d", orig_errno);
	}
	// Here we do not even understand the errno from strerror_r() !
	return llformat("strerror_r() cannot explain errno %d (error %d)",
					orig_errno, stre_errno);
}

std::string strerr(int errn)
{
	char buffer[256];
	// Select message_from() function matching the strerror_r() we have on hand
	return message_from(errn, buffer, sizeof(buffer),
						strerror_r(errn, buffer, sizeof(buffer)));
}
#endif	// LL_WINDOWS

// On either system, shorthand call just infers global 'errno'.
std::string strerr()
{
	return strerr(errno);
}

LLFile::LLFile(const std::string& filename, const char* mode, S64* size)
{
	if (size)
	{
		llstat st;
		*size = LLFile::stat(filename, &st) == 0 &&
				S_ISREG(st.st_mode) ? (S64)st.st_size : 0;
	}
	mFile = LLFile::open(filename, mode);
}

LLFile::~LLFile()
{
	if (mFile)
	{
		fclose(mFile);
	}
}

LLFile& LLFile::operator=(LLFILE* f)
{
	if (mFile)
	{
		fclose(mFile);
	}
	mFile = f;
	return *this;
}

LLFile& LLFile::operator=(LLFile&& other)
{
	if (mFile)
	{
		fclose(mFile);
	}
	std::swap(mFile, other.mFile);
	return *this;
}

S64 LLFile::read(U8* buffer, S64 bytes)
{
	if (!mFile)
	{
		return 0;
	}
	return fread((void*)buffer, 1, bytes, mFile);
}

S64 LLFile::write(const U8* buffer, S64 bytes)
{
	if (!mFile)
	{
		return 0;
	}
	S64 written = fwrite((const void*)buffer, 1, bytes, mFile);
	if (sFlushOnWrite)
	{
		fflush(mFile);
	}
	return written;
}

bool LLFile::flush()
{
	return mFile && fflush(mFile) == 0;
}

S64 LLFile::seek(S64 position, bool delta)
{
	if (!mFile)
	{
		return -1;
	}
	if (position < 0)
	{
		fseek(mFile, 0, SEEK_END);
	}
	else if (delta)
	{
		fseek(mFile, position, SEEK_CUR);
	}
	else
	{
		fseek(mFile, position, SEEK_SET);
	}
	return ftell(mFile);
}

bool LLFile::eof()
{
	return !mFile || feof(mFile) != 0;
}

// Implementation borrowed/adapted from APR. HB
bool LLFile::lock(bool exclusive)
{
	if (!mFile)
	{
		return false;
	}
#if LL_WINDOWS
	// Windows does not work like POSIX OSes... An exclusive lock on a file
	// prevents any read access to it from another process and a shared lock
	// prevents writes by anyone, including the lock holder. HB
	DWORD flags = LOCKFILE_FAIL_IMMEDIATELY;
	if (exclusive)
	{
		flags |= LOCKFILE_EXCLUSIVE_LOCK;
	}
    constexpr DWORD len = 0xffffffff;
	OVERLAPPED offset;
	memset(&offset, 0, sizeof(offset));
	HANDLE h = (HANDLE)_get_osfhandle(_fileno(mFile));
	memset(&offset, 0, sizeof(offset));
	return LockFileEx(h, flags, 0, len, len, &offset);
#else
	// With POSIX OSes a write lock still allows the lock holder to write to
	// the file, while all others can still read from it. We therefore can use
	// the write lock and ignore entirely the 'exclusive' boolean. HB
	struct flock l = { 0 };
	l.l_whence = SEEK_SET;		// Lock from start of file
	l.l_start = 0;				// Begin lock at this offset
	l.l_len = 0;				// Lock to end of file
	l.l_type = F_WRLCK;			// Write lock ('exclusive' ignored).
	constexpr int fc = F_SETLK;	// Non-blocking lock.
	// Keep trying if fcntl() gets interrupted (by a signal)
	int rc;
	while ((rc = fcntl(fileno(mFile), fc, &l)) < 0 && errno == EINTR) ;
	return rc != -1;
#endif
}

bool LLFile::unlock()
{
	if (!mFile)
	{
		return false;
	}
#if LL_WINDOWS
    constexpr DWORD len = 0xffffffff;
	OVERLAPPED offset;
	memset(&offset, 0, sizeof(offset));
	HANDLE h = (HANDLE)_get_osfhandle(_fileno(mFile));
	memset(&offset, 0, sizeof(offset));
	return UnlockFileEx(h, 0, len, len, &offset);
#else
	struct flock l = { 0 };
	l.l_whence = SEEK_SET;		// Lock from start of file
	l.l_start = 0;				// Begin lock at this offset
	l.l_len = 0;				// Lock to end of file
	l.l_type = F_UNLCK;			// Unlock.
	constexpr int fc = F_SETLK;	// Non-blocking lock.
	// Keep trying if fcntl() gets interrupted (by a signal)
	int rc;
	while ((rc = fcntl(fileno(mFile), fc, &l)) < 0 && errno == EINTR) ;
	return rc != -1;
#endif
}

//static
bool LLFile::mkdir(const std::string& dirname, U16 perms)
{
#if LL_WINDOWS
	// Permissions are ignored on Windows
	int rc = _wmkdir(ll_convert_string_to_wide(dirname).c_str());
#else
	int rc = ::mkdir(dirname.c_str(), (mode_t)perms);
#endif
	if (rc < 0)
	{
		// Capture errno before we start emitting output
		int errn = errno;
		// We often use mkdir() to ensure the existence of a directory that
		// might already exist. Consider it a success when that directory
		// already exists.
		if (errn != EEXIST)
		{
			llwarns << "Failed on '" << dirname << "' (errno " << errn
					<< "): " << strerr(errn) << llendl;
			return false;
		}
	}
	return true;
}

//static
bool LLFile::rmdir(const std::string& dirname)
{
#if LL_WINDOWS
	// Permissions are ignored on Windows
	int rc = _wrmdir(ll_convert_string_to_wide(dirname).c_str());
#else
	int rc = ::rmdir(dirname.c_str());
#endif
	if (rc < 0)
	{
		// Capture errno before we start emitting output
		int errn = errno;
		llwarns << "Failed on '" << dirname << "' (errno " << errn
				<< "): " << strerr(errn) << llendl;
		return false;
	}
	return true;
}

//static
LLFILE* LLFile::open(const std::string& filename, const char* mode)
{
#if LL_WINDOWS
	return _wfopen(ll_convert_string_to_wide(filename).c_str(),
				   ll_convert_string_to_wide(mode).c_str());
#else
	return fopen(filename.c_str(), mode);
#endif
}

//static
LLFILE* LLFile::open(const char* filename, const char* mode)
{
#if LL_WINDOWS
	return _wfopen(ll_convert_string_to_wide(filename).c_str(),
				   ll_convert_string_to_wide(mode).c_str());
#else
	return fopen(filename, mode);
#endif
}

void LLFile::close(LLFILE* file)
{
	if (file)
	{
		// Note: we do not care about errors when closing
		fclose(file);
	}
}

bool LLFile::remove(const std::string& filename)
{
#if LL_WINDOWS
	int rc = _wremove(ll_convert_string_to_wide(filename).c_str());
#else
	int rc = ::remove(filename.c_str());
#endif
	if (rc)
	{
		// We do not care if the file to be removed does not exist.
		// Do not spam the log with such warnings either.
		if (errno != ENOENT)
		{
			// Capture errno before we start emitting output
			int errn = errno;
			llwarns << "Failed on '" << filename << "' (errno " << errn
					<< "): " << strerr(errn) << llendl;
			return false;
		}
	}
	return true;
}

bool LLFile::rename(const std::string& filename, const std::string& newname,
					bool ignore_cross_linking)
{
#if LL_WINDOWS
	int rc = _wrename(ll_convert_string_to_wide(filename).c_str(),
					  ll_convert_string_to_wide(newname).c_str());
	// Capture errno before we (possibly) start emitting output
	int errn = errno;
#else
	int rc = ::rename(filename.c_str(), newname.c_str());
	int errn = errno;	// Capture errno before it (possibly) changes
	if (rc && errn == EXDEV)
	{
		if (LLFile::copy(filename, newname))
		{
			if (!ignore_cross_linking)
			{
				llinfos << "Rename across mounts detected; moving "
						<< filename << " to " << newname << " instead."
						<< llendl;
			}
			unlink(filename.c_str());
			return true;
		}
	}
#endif
	if (rc)
	{
		llwarns << "Failed to rename \""<< filename << "\" to \"" << newname
				<< " \" (errno " << errn << "): " << strerr(errn) << llendl;
		return false;
	}
	return true;
}

bool LLFile::copy(const std::string from, const std::string to)
{
	bool copied = false;

	LLFILE* in = LLFile::open(from, "rb");
	if (in)
	{
		LLFILE* out = LLFile::open(to, "wb");
		if (out)
		{
			char buf[16384];
			size_t readbytes;
			bool write_ok = true;
			while (write_ok && (readbytes = fread(buf, 1, 16384, in)))
			{
				if (fwrite(buf, 1, readbytes, out) != readbytes)
				{
					llwarns << "Short write to: " << to << llendl;
					write_ok = false;
				}
			}
			if (write_ok)
			{
				copied = true;
			}
			fclose(out);
		}
		fclose(in);
	}

	return copied;
}

// Returns the OS stat error code (with errno reflecting the actual error, when
// it occurs): we do not warn on failures here, since this call is used in
// places where the appropriate action will be taken when a failure occurs
// (most of the time, we use stat to just check that a file exists). HB
S32 LLFile::stat(const std::string& filename, llstat* filestatus)
{
#if LL_WINDOWS
	return _wstat(ll_convert_string_to_wide(filename).c_str(), filestatus);
#else
	return ::stat(filename.c_str(), filestatus);
#endif
}

bool LLFile::exists(const std::string& filename)
{
	llstat st;
	return LLFile::stat(filename, &st) == 0;
}

bool LLFile::isdir(const std::string& filename)
{
	llstat st;
	return LLFile::stat(filename, &st) == 0 && S_ISDIR(st.st_mode);
}

bool LLFile::isfile(const std::string& filename)
{
	llstat st;
	return LLFile::stat(filename, &st) == 0 && S_ISREG(st.st_mode);
}

size_t LLFile::getFileSize(const std::string& filename)
{
	llstat st;
	return LLFile::stat(filename, &st) == 0 &&
		   S_ISREG(st.st_mode) ? st.st_size : 0;
}

time_t LLFile::lastModidied(const std::string& filename)
{
	llstat st;
	return LLFile::stat(filename, &st) == 0 ? st.st_mtime : 0;
}

const char* LLFile::tmpdir()
{
	static std::string utf8path;

	if (utf8path.empty())
	{
		char sep;
#if LL_WINDOWS
		sep = '\\';

		std::vector<wchar_t> utf16path(MAX_PATH + 1);
		GetTempPathW(utf16path.size(), &utf16path[0]);
		utf8path = ll_convert_wide_to_string(&utf16path[0]);
#else
		sep = '/';

		char* env = getenv("TMP");
		if (!env)
		{
			env = getenv("TMPDIR");
		}
		utf8path = env ? env : "/tmp/";
#endif
		if (utf8path[utf8path.size() - 1] != sep)
		{
			utf8path += sep;
		}
	}
	return utf8path.c_str();
}

//static
S32 LLFile::readEx(const std::string& filename, void* buf, S32 offset,
				   S32 nbytes)
{
	if (offset < 0)
	{
		llwarns << "Negative offset passed to read: " << filename << llendl;
		llassert(false);
		return 0;
	}

	LLFile infile(filename, "rb");
	if (!infile)
	{
		llwarns << "Failed to open for reading: " << filename << llendl;
		return 0;
	}

	if (offset > 0 && infile.seek(offset) != offset)
	{
		llwarns << "Failed to seek at offset " << offset << " in file: "
				<< filename << llendl;
		return 0;
	}

	S32 bytes_read = infile.read((U8*)buf, nbytes);
	if (bytes_read != nbytes)
	{
		llwarns << "Failed to read " << nbytes << " bytes from file: "
				<< filename << llendl;
		return 0;
	}
	llassert_always(bytes_read <= 0x7fffffff);
	return bytes_read;
}

//static
S32 LLFile::writeEx(const std::string& filename, void* buf, S32 offset,
					S32 nbytes)
{
	bool exists = LLFile::exists(filename);
	const char* flags = exists ? (offset < 0 ? "ab" : "r+b") : "wb";
	LLFile outfile(filename, flags);
	if (!outfile)
	{
		llwarns << "Failed to open for writing: " << filename << llendl;
		return 0;
	}

	if (offset > 0 && outfile.seek(offset) != offset)
	{
		llwarns << "Failed to seek at offset " << offset << " in file: "
				<< filename << llendl;
		return 0;
	}

	S32 bytes_written = outfile.write((U8*)buf, nbytes);
	if (bytes_written != nbytes)
	{
		llwarns << "Failed to write " << nbytes << " bytes to file: "
				<< filename << llendl;
		return 0;
	}
	llassert_always(bytes_written <= 0x7fffffff);
	return bytes_written;
}

//static
bool LLFile::createFileSymlink(const std::string& filename,
							   const std::string& link)
{
	if (filename.empty() || link.empty())
	{
		return false;
	}
	if (!LLFile::exists(filename))
	{
		// Create an empty file
		llofstream outfile(filename);
		if (!outfile.is_open())
		{
			llwarns << "Failed to create an empty file for non-existent "
					<< filename << " to link to: " << link << llendl;
			return false;
		}
	}
	if (!isfile(filename))
	{
		llwarns << "Target " << filename
				<< " is not a regular file. Cannot link it as: " << link
				<< llendl;
		return false;
	}
	// Note: we could use C++17 std::filesystem too for Linux and macOS, if
	// only gcc 8 or older and clang v9 or older did not need a special add-on
	// stdc++fs library... On its side, Windows does not have the symlink() C
	// function. An alternative would be using boost::filesystem for everyone,
	// but I want to keep llcommon indepedendant from the boost_filesystem
	// library (which is only used by llfilesystem). HB
#if LL_WINDOWS
	std::error_code ec;
	std::filesystem::create_symlink(filename, link, ec);
#else
	int ec = symlink(filename.c_str(), link.c_str());
#endif
	if (ec)
	{
		llwarns << "Failed to create symbolic link " << link << " for file "
				<< filename << llendl;
	}
	return true;
}

//static
bool LLFile::gzip(const std::string& srcfile, const std::string& dstfile)
{
	constexpr S32 COMPRESS_BUFFER_SIZE = 32768;
	U8 buffer[COMPRESS_BUFFER_SIZE];

	std::string tmpfile = dstfile + ".tmp";
#if LL_WINDOWS
	gzFile dst = gzopen_w(ll_convert_string_to_wide(tmpfile).c_str(), "wb");
#else
	gzFile dst = gzopen(tmpfile.c_str(), "wb");
#endif
	if (!dst)
	{
		return false;
	}

	LLFILE* src = LLFile::open(srcfile, "rb");
	if (!src)
	{
		gzclose(dst);
		return false;
	}

	do
	{
		size_t bytes = fread(buffer, sizeof(U8), COMPRESS_BUFFER_SIZE, src);
		gzwrite(dst, buffer, bytes);
	}
	while (!feof(src));

	LLFile::close(src);
	gzclose(dst);

	LLFile::remove(dstfile);

	return LLFile::rename(tmpfile, dstfile);
}

//static
bool LLFile::gunzip(const std::string& srcfile, const std::string& dstfile)
{
	constexpr S32 UNCOMPRESS_BUFFER_SIZE = 32768;
	U8 buffer[UNCOMPRESS_BUFFER_SIZE];

#if LL_WINDOWS
	gzFile src = gzopen_w(ll_convert_string_to_wide(srcfile).c_str(), "rb");
#else
	gzFile src = gzopen(srcfile.c_str(), "rb");
#endif
	if (!src)
	{
		return false;
	}

	std::string tmpfile = dstfile + ".tmp";
	LLFILE* dst = LLFile::open(tmpfile, "wb");
	if (!dst)
	{
		gzclose(src);
		return false;
	}

	do
	{
		size_t bytes = gzread(src, buffer, UNCOMPRESS_BUFFER_SIZE);
		size_t nwrit = fwrite(buffer, sizeof(U8), bytes, dst);
		if (nwrit < bytes)
		{
			llwarns << "Short write on " << tmpfile << ": Wrote " << nwrit
					<< " of " << bytes << " bytes." << llendl;
			gzclose(src);
			LLFile::close(dst);
			return false;
		}
	}
	while (!gzeof(src));

	gzclose(src);
	LLFile::close(dst);

	LLFile::remove(dstfile);

	return LLFile::rename(tmpfile, dstfile);
}

#if LL_WINDOWS

///////////////////////////////////////////////////////////////////////////////
// Modified file stream created to overcome the incorrect behaviour of POSIX
// fopen in windows
///////////////////////////////////////////////////////////////////////////////

// Input file stream

llifstream::llifstream()
{
}

//explicit
llifstream::llifstream(const std::string& filename, ios_base::openmode mode)
:	std::ifstream(ll_convert_string_to_wide(filename).c_str(),
				  mode | ios_base::in)
{
}

void llifstream::open(const std::string& filename, ios_base::openmode mode)
{
	std::ifstream::open(ll_convert_string_to_wide(filename).c_str(),
						mode | ios_base::in);
}

// Output file stream

llofstream::llofstream()
{
}

//explicit
llofstream::llofstream(const std::string& filename, ios_base::openmode mode)
:	std::ofstream(ll_convert_string_to_wide(filename), mode | ios_base::out)
{
}

void llofstream::open(const std::string& filename, ios_base::openmode mode)
{
	std::ofstream::open(ll_convert_string_to_wide(filename).c_str(),
						mode | ios_base::out);
}

#endif	// LL_WINDOWS
