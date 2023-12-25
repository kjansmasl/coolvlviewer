/**
 * @file llfile.h
 * @author Michael Schlachter
 * @date 2006-03-23
 * @brief Declaration of cross-platform POSIX file buffer and c++
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

#ifndef LL_LLFILE_H
#define LL_LLFILE_H

typedef FILE LLFILE;

#include <algorithm>
#include <fstream>
#include <sys/stat.h>
#include <time.h>

// Safe char* -> std::string conversion. Also indirectly includes
// llpreprocessor.h (for LL_INLINE).
#include "llstring.h"

// This replaces advantageously gDirUtilp->getDirDelimiter() and saves us the
// costly use of a std::string (and quite a few std::string/const char* strings
// concatenations) for the directory delimiter, which is known at compile
// time... HB
#if LL_WINDOWS
# define LL_DIR_DELIM_STR "\\"
# define LL_DIR_DELIM_CHR '\\'
#else
# define LL_DIR_DELIM_STR "/"
# define LL_DIR_DELIM_CHR '/'
#endif

#if LL_WINDOWS
// Windows version of stat function and stat data structure are called _stat
typedef struct _stat llstat;
#elif LL_DARWIN
typedef struct stat llstat;
#else
typedef struct stat llstat;
# include <ext/stdio_filebuf.h>
# include <bits/postypes.h>
#endif

#ifndef S_ISREG
# define S_ISREG(x) (((x) & S_IFMT) == S_IFREG)
#endif

#ifndef S_ISDIR
# define S_ISDIR(x) (((x) & S_IFMT) == S_IFDIR)
#endif

// This class provides a cross platform interface to the filesystem. Attempts
// to mostly mirror the POSIX style IO functions.

class LLFile
{
protected:
	LOG_CLASS(LLFile);

public:
	// All the methods below take UTF-8 path/filenames.

	// These non-static methods have been implemented to get rid of LLAPRFile.
	// They implement the equivalent of what could be found in the latter, with
	// the open() method (implicit in constructor) and close() method (implicit
	// in destructor) removed. They also replace LLUniqueFile (which was only
	// used in llerror.cpp). HB

	// Empty constructor (used in llerror.cpp)
	LL_INLINE LLFile()
	:	mFile(NULL)
	{
	}

	// Normal constructor
	LLFile(const std::string& filename, const char* mode, S64* size = NULL);

	// Wrap constructor. E.g. result of LLFile::open() (used in llerror.cpp)
	LL_INLINE LLFile(LLFILE* f)
	:	mFile(f)
	{
	}

	// Move constructor
	LL_INLINE LLFile(LLFile&& other)
	{
		mFile = other.mFile;
		other.mFile = NULL;
	}

	// Forbid copy constructor usage
	LLFile(const LLFile&) = delete;

	~LLFile();

	// Simple assignment
	LLFile& operator=(LLFILE* f);
	// Move assignment
	LLFile& operator=(LLFile&& other);
	// Forbid copy assignment usage
	LLFile& operator=(const LLFile&) = delete;

	// Detect whether the wrapped LLFILE is open or not
	LL_INLINE explicit operator bool() const	{ return bool(mFile); }
	LL_INLINE bool operator!()					{ return !mFile; }

	// LLFile should be usable for any operation that accepts LLFILE* (or FILE*
	// for that matter).
	LL_INLINE operator LLFILE*() const			{ return mFile; }
	// Explicit method, handier/clearer when using a pointer on an LLFile.
	LL_INLINE LLFILE* getStream()				{ return mFile; }

	S64 read(U8* buffer, S64 bytes);
	S64 write(const U8* buffer, S64 bytes);
	// Returns true on success.
	bool flush();
	// Use true for 'delta' when seeking at offset from current file position.
	S64 seek(S64 position, bool delta = false);
	// Returns true on EOF marker set.
	bool eof();

	// Returns false if a non-blocking file lock could not be obtained. Note
	// that the 'exclusive' boolean is only actually used under Windows where
	// shared locks prevent any writing by the lock holder, while exclusive
	// locks allow it but prevent any other process to read the locked file !
	// On POSIX OSes, a write lock is always used, which still allows the lock
	// holder to write to the file and any other process to read it. HB
	bool lock(bool exclusive = false);

	// Windows is stupid: you cannot change the contents of a locked file, when
	// you own a shared lock on it, and you cannot read it from another process
	// if you take an exclusive lock on it. This is unlike POSIX systems where
	// a write-locked file can still be read by everyone... So under Windoze,
	// we need to be able to unlock our file to change its contents and re-lock
	// it afterwards. HB
	bool unlock();

	// These two methods (which used to be in the now removed LLAPRFile) return
	// the number of bytes read/written, or 0 if read/write failed. They are
	// limited to 2GB files. HB

	static S32 readEx(const std::string& filename, void* buf, S32 offset,
					  S32 nbytes);
	// Note: 'offset' < 0 means append.
	static S32 writeEx(const std::string& filename, void* buf, S32 offset,
					   S32 nbytes);

	// For all these methods, returned booleans are true on success to perform
	// the requested operation, false on failure. This is unlike LL's original
	// code that usually returns an error code which is 0 to denote a success.
	// HB

	static LLFILE* open(const std::string& filename, const char* accessmode);
	static LLFILE* open(const char* filename, const char* accessmode);

	static void close(LLFILE* file);

	// 'perms' is a permissions mask (in octal) like 0777 or 0700. In most
	// cases it will be overridden by the user's umask. It is ignored on
	// Windows.
	static bool mkdir(const std::string& filename, U16 perms = 0700);

	static bool rmdir(const std::string& filename);
	static bool remove(const std::string& filename);
	static bool rename(const std::string& filename, const std::string& newname,
					   bool ignore_cross_linking = false);
	static bool copy(const std::string from, const std::string to);

	// Note: this method returns the error code from the OS stat() call:
	// 0 denotes a success (i.e. file exists and its metadata could be read).
	static S32 stat(const std::string& filename, llstat* file_status);

	static bool exists(const std::string& filename);
	static bool isdir(const std::string& filename);
	static bool isfile(const std::string& filename);

	static size_t getFileSize(const std::string& filename);
	static time_t lastModidied(const std::string& filename);

	// Tries and creates a symbolic 'link' for regular file 'filename'. When
	// 'filename' does not correspond to an existing file, this method tries to
	// create an empty file for it. Returns true on success, or false when
	// 'filename' is not an existing regular file and could not be created as
	// an empty file, or'link' is empty, or the link creation failed (warnings
	// are also logged as appropriate). HB
	static bool createFileSymlink(const std::string& filename,
								  const std::string& link);

	static const char* tmpdir();

	// Used to be gzip_file() and gunzip_file() and are defined in llsys.h/cpp
	// in LL's sources, but best moved here, since they make use of LLFile
	// operations and have *strictly* nothing to do llsys stuff !... HB
	// These methods return true on success or false otherwise.
	static bool gzip(const std::string& srcfile, const std::string& dstfile);
	static bool gunzip(const std::string& srcfile, const std::string& dstfile);

private:
	LLFILE* mFile;

public:
 	// This is to work around a bug in Wine (*) which reports a wrong file
	// position after non flushed writes. (*) This is for people perverted
	// enough to run a Windows build under Wine under Linux instead of a Linux
	// native build: yes, I am perverted since I do it to test Windows builds
	// under Linux... :-P  HB
	static bool sFlushOnWrite;
};

#if !LL_WINDOWS

typedef std::ifstream llifstream;
typedef std::ofstream llofstream;

#else

// Controlling input for files.
//
// This class supports writing to named files, using the inherited methods from
// std::ifstream. The only added value is that our constructor Does The Right
// Thing when passed a non-ASCII pathname. Sadly, that is not true of
// Microsoft's std::ofstream.
class llifstream : public std::ifstream
{
  public:
	// Default constructor.
	// Initializes sb using its default constructor, and passes &sb to the base
	// class initializer. Does not open any files (you have not given it a
	// filename to open).
	llifstream();

	// Creates an input file stream, opening file 'filename' in specified
	// 'mode' (see std::ios_base). ios_base::in is automatically included in
	// 'mode'.
	explicit llifstream(const std::string& filename,
                        ios_base::openmode mode = ios_base::in);

	// Opens an external file named 'filename'  in specified 'mode'.
	// Calls llstdio_filebuf::open(s,mode|in). If that method
	// fails, @c failbit is set in the stream's error state.
	void open(const std::string& filename,
              ios_base::openmode mode = ios_base::in);
};

// Controlling output for files.
//
// This class supports writing to named files, using the inherited methods from
// std::ofstream. The only added value is that our constructor Does The Right
// Thing when passed a non-ASCII pathname. Sadly, that is not true of
// Microsoft's std::ofstream.
class llofstream : public std::ofstream
{
  public:
	// Default constructor.
	// Initializes sb using its default constructor, and passes &sb to the base
	// class initializer. Does not open any file (you have not given it a
	// filename to open).
	llofstream();

	// Creates an output file stream, opening file 'filename' in specified
	// 'mode' (see std::ios_base). ios_base::out is automatically included in
	// 'mode'.
	explicit llofstream(const std::string& filename,
                        ios_base::openmode mode = ios_base::out|ios_base::trunc);

	// Opens an external file named 'filename'  in specified 'mode'.
	// ios_base::out is automatically included in 'mode'.
	void open(const std::string& filename,
              ios_base::openmode mode = ios_base::out|ios_base::trunc);
};

#endif	// LL_WINDOWS

#endif // LL_LLFILE_H
