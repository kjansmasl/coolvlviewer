/**
 * @file llfilesystem.cpp
 * @brief Implementation of the local file system.
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

#include "linden_common.h"

#include "llfilesystem.h"

#include "lldiskcache.h"

LLFileSystem::LLFileSystem(const LLUUID& id, S32 mode, const char* extra_info)
:	mFileID(id),
	mMode(mode),
	mPosition(0),
	mBytesRead(0),
	mTotalBytesWritten(0),
	mFilename(LLDiskCache::getFilePath(id, extra_info)),
	mValid(LLDiskCache::isValid())
{
	if (extra_info && *extra_info)
	{
		mExtraInfo.assign(extra_info);
	}

	mExists = mValid && LLFile::exists(mFilename);
	if (mExists)
	{
		// Update the last access time for the file since this is the way the
		// cache works; it relies on a valid "last accessed time" for each file
		// so that it knows how to remove the oldest, unused files.
		// Since LLFileSystem instances are short-lived, we update the file
		// access time on construction (which also allows to update that time
		// at once during cache purging, preventing an old file that would be
		// reused to get purged in between LLFileSystem instance construction
		// and its actual usage). HB
		LLDiskCache::updateFileAccessTime(mFilename);
	}

	// In append mode, we always write to the end of file, so make sure to
	// initialize the current position there... HB
	if (mExists && mMode == APPEND)
	{
		mPosition = LLFile::getFileSize(mFilename);
	}
}

LLFileSystem::~LLFileSystem()
{
	if (mTotalBytesWritten)
	{
		// Inform the disk cache about how much bytes we added or removed. HB
		LLDiskCache::addBytesWritten(mTotalBytesWritten);
	}
}

bool LLFileSystem::read(U8* buffer, S32 bytes)
{
	if (!mValid || bytes < 0 || !buffer)
	{
		return false;
	}
	if (!bytes)
	{
		mExists = LLFile::isfile(mFilename);
		return mExists;
	}

	LLFILE* file = LLFile::open(mFilename, "rb");
	mExists = file != NULL;
	if (!mExists)
	{
		return false;
	}

	if (mPosition > 0)
	{
		fseek(file, mPosition, SEEK_SET);
	}
	mBytesRead = fread(buffer, 1, bytes, file);
	LLFile::close(file);

	if (mBytesRead > 0)
	{
		mPosition += mBytesRead;
		// Short reads are also considered a success (needed due to how
		// buffered reads are implemented in the viewer code such as in,
		// for example, LLAssetStorage::legacyGetDataCallback())... HB
		return true;
	}

	return false;
}

bool LLFileSystem::write(const U8* buffer, S32 bytes)
{
	if (!mValid)
	{
		return false;
	}
	if (mMode == APPEND)
	{
		// Write to file, appending to it if it already exists.
		LLFILE* file = LLFile::open(mFilename, "a+b");
		if (file)
		{
			fwrite((void*)buffer, 1, bytes, file);
			if (LLFile::sFlushOnWrite)
			{
				fflush(file);
			}
			mPosition = ftell(file);
			LLFile::close(file);
			mTotalBytesWritten += bytes;
			mExists = true;
			return true;
		}
	}
	else if (mMode == OVERWRITE)
	{
		// Discard any existing contents and write.
		mTotalBytesWritten -= LLFile::getFileSize(mFilename);
		LLFILE* file = LLFile::open(mFilename, "wb");
		if (file)
		{
			fwrite((void*)buffer, 1, bytes, file);
			if (LLFile::sFlushOnWrite)
			{
				fflush(file);
			}
			mPosition = ftell(file);
			LLFile::close(file);
			mTotalBytesWritten += bytes;
			mExists = true;
			return true;
		}
	}
	else if (mMode == WRITE)
	{
		// Write at current position, without truncating
		S32 size = LLFile::getFileSize(mFilename);	// Remember current size
		mExists = size > 0;
		const char* mode = mExists ? "r+b" : "wb";
		LLFILE* file = LLFile::open(mFilename, mode);
		if (file)
		{
			if (mExists && mPosition > 0)
			{
				fseek(file, mPosition, SEEK_SET);
			}
			fwrite((void*)buffer, 1, bytes, file);
			if (LLFile::sFlushOnWrite)
			{
				fflush(file);
			}
			mPosition = ftell(file);
			LLFile::close(file);
			if (mPosition > size)
			{
				mTotalBytesWritten += mPosition - size;
			}
			mExists = true;
			return true;
		}
	}
	else
	{
		llerrs << "Cannot write in READ mode." << llendl;
	}

	mExists = false;
	return false;
}

bool LLFileSystem::seek(S32 offset, S32 origin)
{
	if (!mValid)
	{
		return false;
	}
	if (mMode == OVERWRITE || mMode == APPEND)
	{
		llerrs << "Cannot seek in file before writing into it in mode "
			   << (mMode == APPEND ? "APPEND" : "OVERWRITE") << llendl;
	}
	if (origin < 0)
	{
		origin = mPosition;
	}
	S32 new_pos = origin + offset;
	S32 size = LLFile::getFileSize(mFilename);
	if (new_pos > size)
	{
		if (mMode == READ)
		{
			llwarns << "Attempt to seek past end of file: " << mFilename
					<< llendl;
			mPosition = size;
			return false;
		}
		else	// Append zeros to the file up to the new position. HB
		{
			mPosition = size;
			LLFILE* file = LLFile::open(mFilename, "a+b");
			if (!file)
			{
				llwarns << "Attempt to seek past end of file \"" << mFilename
						<< "\", and could not open it to pad it with zeros."
						<< llendl;
				return false;
			}

			mExists = true;
			size_t bytes = new_pos - size;
			char* buffer = new (std::nothrow) char[bytes];
			if (buffer)
			{
				LL_DEBUGS("FileSystem") << "Appending " << bytes
										<< " padding bytes to: " << mFilename
										<< LL_ENDL;
				memset((void*)buffer, 0, bytes);
				fwrite((void*)buffer, 1, bytes, file);
				if (LLFile::sFlushOnWrite)
				{
					fflush(file);
				}
				mPosition = ftell(file);
				mTotalBytesWritten += mPosition - size;
				delete[] buffer;
			}
			LLFile::close(file);
			if (mPosition == new_pos)
			{
				return true;
			}
			llwarns << "Could not append enough padding bytes to seek to position: "
					<< size << " in \"" << mFilename << "\" (position "
					<< mPosition << " reached)." << llendl;
			return false;
		}
	}
	if (new_pos < 0)
	{
		llwarns << "Attempt to seek past beginning of file: " << mFilename
				<< llendl;
		mPosition = 0;
		return false;
	}
	mPosition = new_pos;
	return true;
}

S32 LLFileSystem::getSize() const
{
	return mValid ? (S32)LLFile::getFileSize(mFilename) : 0;
}

bool LLFileSystem::remove()
{
	if (!mValid)
	{
		return false;
	}
	mExists = false;
	llstat st;
	if (LLFile::stat(mFilename, &st))
	{
		// No such file, we are done.
		return true;
	}
	mTotalBytesWritten -= st.st_size;
	return LLFile::remove(mFilename);
}

bool LLFileSystem::rename(const LLUUID& new_id)
{
	mFileID = new_id;
	if (!mValid)
	{
		return false;
	}
	std::string newfname = LLDiskCache::getFilePath(new_id,
													mExtraInfo.c_str());
	// First remove the new file when it exists
	llstat st;
	if (LLFile::stat(newfname, &st) == 0)
	{
		mTotalBytesWritten -= st.st_size;
		LLFile::remove(newfname);
	}
	// Note: this call may fail and will appropriately warn in the log...
	mExists = LLFile::rename(mFilename, newfname);
	mFilename = newfname;
	return mExists;
}

//static
bool LLFileSystem::getExists(const LLUUID& id, const char* extra_info)
{
	if (!LLDiskCache::isValid())
	{
		return false;
	}
	return LLFile::isfile(LLDiskCache::getFilePath(id, extra_info));
}

//static
S32 LLFileSystem::getFileSize(const LLUUID& id, const char* extra_info)
{
	if (!LLDiskCache::isValid())
	{
		return 0;
	}
	return LLFile::getFileSize(LLDiskCache::getFilePath(id, extra_info));
}

//static
bool LLFileSystem::removeFile(const LLUUID& id, const char* extra_info)
{
	if (!LLDiskCache::isValid())
	{
		return false;
	}
	std::string filename = LLDiskCache::getFilePath(id, extra_info);
	llstat st;
	if (LLFile::stat(filename, &st))
	{
		// No such file, we are done.
		return true;
	}
	if (st.st_size)
	{
		LLDiskCache::addBytesWritten(-st.st_size);
	}
	return LLFile::remove(filename);
}

//static
bool LLFileSystem::renameFile(const LLUUID& old_id, const LLUUID& new_id,
							  const char* extra_info)
{
	if (!LLDiskCache::isValid())
	{
		return false;
	}

	std::string old_filename = LLDiskCache::getFilePath(old_id, extra_info);
	std::string new_filename = LLDiskCache::getFilePath(new_id, extra_info);
	// First remove the new file when it exists
	llstat st;
	if (LLFile::stat(old_filename, &st) == 0)
	{
		if (st.st_size)
		{
			LLDiskCache::addBytesWritten(-st.st_size);
		}
		LLFile::remove(new_filename);
	}

	// Note: this call may fail and will appropriately warn in the log...
	return LLFile::rename(old_filename, new_filename);
}
