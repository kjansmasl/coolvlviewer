/**
 * @file llxfer_file.cpp
 * @brief implementation of LLXfer_File class for a single xfer (file)
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 *
 * Copyright (c) 2001-2009, Linden Research, Inc.
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

#include "llxfer_file.h"

#include "lldir.h"
#include "llmath.h"
#include "llstring.h"

// local function to copy a file
LLXfer_File::LLXfer_File (S32 chunk_size)
:	LLXfer(chunk_size)
{
	init(LLStringUtil::null, false, chunk_size);
}

LLXfer_File::LLXfer_File (const std::string& local_filename,
						  bool delete_local_on_completion, S32 chunk_size)
:	LLXfer(chunk_size)
{
	init(local_filename, delete_local_on_completion, chunk_size);
}

LLXfer_File::~LLXfer_File ()
{
	cleanup();
}

void LLXfer_File::init(const std::string& local_filename,
					   bool delete_local_on_completion, S32 chunk_size)
{

	mFp = NULL;
	mLocalFilename.clear();
	mRemoteFilename.clear();
	mRemotePath = LL_PATH_NONE;
	mTempFilename.clear();
	mDeleteLocalOnCompletion = false;
	mDeleteRemoteOnCompletion = false;

	if (!local_filename.empty())
	{
		mLocalFilename =  local_filename.substr(0, LL_MAX_PATH - 1);

		// You can only automatically delete .tmp file as a safeguard against
		// nasty messages.
		std::string exten;
		exten = mLocalFilename.substr(mLocalFilename.length() - 4, 4);
		mDeleteLocalOnCompletion = delete_local_on_completion &&
								   exten == ".tmp";
	}
}

void LLXfer_File::cleanup ()
{
	if (mFp)
	{
		LLFile::close(mFp);
		mFp = NULL;
	}

	LLFile::remove(mTempFilename);

	if (mDeleteLocalOnCompletion)
	{
		LL_DEBUGS("FileTransfer") << "Removing file: " << mLocalFilename
								  << LL_ENDL;
		LLFile::remove(mLocalFilename);
	}
	else
	{
		LL_DEBUGS("FileTransfer") << "Keeping local file: " << mLocalFilename
								  << LL_ENDL;
	}

	LLXfer::cleanup();
}

S32 LLXfer_File::initializeRequest(U64 xfer_id,
								   const std::string& local_filename,
								   const std::string& remote_filename,
								   ELLPath remote_path,
								   const LLHost& remote_host,
								   bool delete_remote_on_completion,
								   void (*callback)(void**, S32, LLExtStat),
								   void** user_data)
{
 	S32 retval = 0;  // presume success

	mID = xfer_id;
	mLocalFilename = local_filename;
	mRemoteFilename = remote_filename;
	mRemotePath = remote_path;
	mRemoteHost = remote_host;
	mDeleteRemoteOnCompletion = delete_remote_on_completion;

	mTempFilename = gDirUtilp->getTempFilename();

	mCallback = callback;
	mCallbackDataHandle = user_data;
	mCallbackResult = LL_ERR_NOERR;

	llinfos << "Requesting xfer from " << remote_host << " for file: "
			<< mLocalFilename << llendl;

	if (mBuffer)
	{
		delete(mBuffer);
		mBuffer = NULL;
	}

	mBuffer = new char[LL_MAX_XFER_FILE_BUFFER];
	mBufferLength = 0;

	mPacketNum = 0;

 	mStatus = e_LL_XFER_PENDING;
	return retval;
}

S32 LLXfer_File::startDownload()
{
 	S32 retval = 0;  // presume success
	mFp = LLFile::open(mTempFilename, "w+b");
	if (mFp)
	{
		LLFile::close(mFp);
		mFp = NULL;

		LLMessageSystem* msg = gMessageSystemp;
		msg->newMessageFast(_PREHASH_RequestXfer);
		msg->nextBlockFast(_PREHASH_XferID);
		msg->addU64Fast(_PREHASH_ID, mID);
		msg->addStringFast(_PREHASH_Filename, mRemoteFilename);
		msg->addU8("FilePath", (U8) mRemotePath);
		msg->addBool("DeleteOnCompletion", mDeleteRemoteOnCompletion);
		msg->addBool("UseBigPackets", mChunkSize == LL_XFER_LARGE_PAYLOAD);
		msg->addUUIDFast(_PREHASH_VFileID, LLUUID::null);
		msg->addS16Fast(_PREHASH_VFileType, -1);

		msg->sendReliable(mRemoteHost);
		mStatus = e_LL_XFER_IN_PROGRESS;
	}
	else
	{
		llwarns << "Could not create file '" << mTempFilename
				<< "' to be received !" << llendl;
		retval = -1;
	}

	return retval;
}

S32 LLXfer_File::startSend (U64 xfer_id, const LLHost& remote_host)
{
	S32 retval = LL_ERR_NOERR;  // Presume success

    mRemoteHost = remote_host;
	mID = xfer_id;
   	mPacketNum = -1;

	delete [] mBuffer;
	mBuffer = new char[LL_MAX_XFER_FILE_BUFFER];

	mBufferLength = 0;
	mBufferStartOffset = 0;

	// We leave the file open, assuming we will start reading and sending soon
	mFp = LLFile::open(mLocalFilename, "rb");
	if (mFp)
	{
		fseek(mFp, 0, SEEK_END);

		S32 file_size = ftell(mFp);
		if (file_size <= 0)
		{
			return LL_ERR_FILE_EMPTY;
		}
		setXferSize(file_size);

		fseek(mFp, 0, SEEK_SET);
	}
	else
	{
		llwarns << mLocalFilename << " not found." << llendl;
		return LL_ERR_FILE_NOT_FOUND;
	}

	mStatus = e_LL_XFER_PENDING;

	return retval;
}

void LLXfer_File::closeFileHandle()
{
	if (mFp)
	{
		LLFile::close(mFp);
		mFp = NULL;
	}
}

S32 LLXfer_File::reopenFileHandle()
{
	if (!mFp)
	{
		mFp = LLFile::open(mLocalFilename, "rb");
		if (!mFp)
		{
			llwarns << mLocalFilename << " not found for reopening." << llendl;
			return LL_ERR_FILE_NOT_FOUND;
		}
	}

	return LL_ERR_NOERR;
}

S32 LLXfer_File::suck(S32 start_position)
{
	if (mFp)
	{
		// Grab a buffer from the right place in the file
		fseek (mFp, start_position, SEEK_SET);

		mBufferLength = (U32)fread(mBuffer, 1, LL_MAX_XFER_FILE_BUFFER, mFp);
		mBufferStartOffset = start_position;

		mBufferContainsEOF = feof(mFp) != 0;

		return 0;
	}

	return -1;
}

S32 LLXfer_File::flush()
{
	if (mBufferLength)
	{
		if (mFp)
		{
			llerrs << "Overwriting open file pointer !" << llendl;
		}

		mFp = LLFile::open(mTempFilename, "a+b");
		if (mFp)
		{
			S32 bytes_written = fwrite(mBuffer, 1, mBufferLength, mFp);
			if (bytes_written != (S32)mBufferLength)
			{
				llwarns << "Bad write size: requested " << mBufferLength
						<< " bytes but wrote " << bytes_written << " bytes."
						<< llendl;
			}

			LLFile::close(mFp);
			mFp = NULL;

			mBufferLength = 0;
		}
		else
		{
			llwarns << "Unable to open " << mTempFilename << " for writing !"
					<< llendl;
			return LL_ERR_CANNOT_OPEN_FILE;
		}
	}
	return LL_ERR_NOERR;
}

S32 LLXfer_File::processEOF()
{
	S32 retval = 0;
	mStatus = e_LL_XFER_COMPLETE;

	S32 flushval = flush();

	// If we have no other errors, our error becomes the error generated by
	// flush.
	if (!mCallbackResult)
	{
		mCallbackResult = flushval;
	}

	LLFile::remove(mLocalFilename);

	if (!mCallbackResult)
	{
		// Note: this will properly emit a warning in case of failure, and
		// after attempting a file move for renames across mounts under Linux
		// or macOS.
		LLFile::rename(mTempFilename, mLocalFilename, true);
	}

	if (mFp)
	{
		LLFile::close(mFp);
		mFp = NULL;
	}

	retval = LLXfer::processEOF();

	return retval;
}
