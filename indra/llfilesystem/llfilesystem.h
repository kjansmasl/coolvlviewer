/**
 * @file llfilesystem.h
 * @brief Definition of the local file system implementation.
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 *
 * Copyright (c) 2020, Linden Research, Inc. (c) 2021 Henri Beauchamp.
 *
 * Modifications by Henri Beauchamp:
 *  - Pointless per-asset-type file naming removed.
 *  - Cached filename for faster operations.
 *  - Use of faster LLFile operations where possible.
 *  - Fixed various bugs in write operations. Removed the pointless READ_WRITE
 *    mode, added the OVERWRITE one, and changed seek() to auto-padding files
 *    with zeros in WRITE mode when seeking past the end of an existing file.
 *  - Real time tracking of bytes added to/removed from cache.
 *  - Proper cache validity verification.
 *  - Immediate date-stamping on creation of LLFileSystem instances, to prevent
 *    potential race conditions with the threaded cache purging mechanism.
 *  - Multiple threads and multiple viewer instances deconfliction.
 *  - Added LLFile::sFlushOnWrite to work around a bug in Wine (*) which
 *    reports a wrong file position after non flushed writes. (*) This is for
 *    people perverted enough to run a Windows build under Wine under Linux
 *    instead of a Linux native build: yes, I'm perverted since I do it to test
 *    Windows builds under Linux... :-P
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

#ifndef LL_FILESYSTEM_H
#define LL_FILESYSTEM_H

#include "lluuid.h"

// NOTE: this class supports only 2GB or smaller files (way more than what we
// do need).

class LLFileSystem
{
protected:
	LOG_CLASS(LLFileSystem);

public:
	enum
	{
		READ		= 0x00000001,
		WRITE		= 0x00000002,
		OVERWRITE	= 0x00000004,
		APPEND		= 0x00000008
	};

	LLFileSystem(const LLUUID& id, S32 mode = READ,
				 const char* extra_info = NULL); // extra_info not used for now
	~LLFileSystem();

	bool read(U8* buffer, S32 bytes);
	bool write(const U8* buffer, S32 bytes);
	// IMPORTANT: seek() is reserved for READ and WRITE modes (OVERWRITE always
	// writes from start of file, and APPEND from its end). A llerrs will occur
	// if you try to seek() in OVERWRITE or APPEND mode !
	bool seek(S32 offset, S32 origin = -1);

	LL_INLINE const std::string& getName() const	{ return mFilename; }
	LL_INLINE S32 tell() const						{ return mPosition; }
	LL_INLINE bool eof() const						{ return mPosition >= getSize(); }
	LL_INLINE S32 getLastBytesRead() const			{ return mBytesRead; }
	S32 getSize() const;

	// WARNING: mExists is cached and this method can therefore return a wrong
	// value if you touch the file with static methods, or with another program
	// (viewer instance) in-between calls to the constructor, read(), write(),
	// seek(), remove() or rename()...
	LL_INLINE bool exists() const					{ return mExists; }

	bool remove();
	bool rename(const LLUUID& new_id);

	static bool getExists(const LLUUID& id, const char* extra_info = NULL);
	static bool removeFile(const LLUUID& id, const char* extra_info = NULL);
	static bool renameFile(const LLUUID& old_id, const LLUUID& new_id,
						   const char* extra_info = NULL);
	static S32 getFileSize(const LLUUID& id, const char* extra_info = NULL);

protected:
	LLUUID		mFileID;
	std::string	mFilename;
	std::string	mExtraInfo;
	S32			mMode;
	S32			mPosition;
	S32			mBytesRead;
	S32			mTotalBytesWritten;
	bool		mExists;			// true when the file exists
	bool		mValid;				// true when the disk cache is valid
};

#endif	// LL_FILESYSTEM_H
