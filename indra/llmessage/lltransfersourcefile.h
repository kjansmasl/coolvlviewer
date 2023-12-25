/**
 * @file lltransfersourcefile.h
 * @brief Transfer system for sending a file.
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

#ifndef LL_LLTRANSFERSOURCEFILE_H
#define LL_LLTRANSFERSOURCEFILE_H

#include "lltransfermanager.h"

class LLTransferSourceParamsFile : public LLTransferSourceParams
{
protected:
	LOG_CLASS(LLTransferSourceParamsFile);

public:
	LLTransferSourceParamsFile();
	virtual ~LLTransferSourceParamsFile()				{}
	void packParams(LLDataPacker& dp) const override;
	bool unpackParams(LLDataPacker& dp) override;

	void setFilename(const std::string& filename)		{ mFilename = filename; }
	std::string getFilename() const						{ return mFilename; }

	void setDeleteOnCompletion(bool enabled)			{ mDeleteOnCompletion = enabled; }
	bool getDeleteOnCompletion()						{ return mDeleteOnCompletion; }

protected:
	std::string	mFilename;

	// ONLY DELETE THINGS OFF THE SIM IF THE FILENAME BEGINS IN 'TEMP'
	bool		mDeleteOnCompletion;
};

class LLTransferSourceFile : public LLTransferSource
{
protected:
	LOG_CLASS(LLTransferSourceFile);

public:
	LLTransferSourceFile(const LLUUID& transfer_id, F32 priority);
	~LLTransferSourceFile() override;

protected:
	void initTransfer() override;
	F32 updatePriority() override					{ return 0; }
	LLTSCode dataCallback(S32 packet_id, S32 max_bytes, U8** datap,
						  S32& returned_bytes, bool& delete_returned) override;
	void completionCallback(LLTSCode status) override;

	void packParams(LLDataPacker& dp) const override;
	bool unpackParams(LLDataPacker& dp) override;

protected:
	LLTransferSourceParamsFile	mParams;
	LLFILE*						mFP;
};

#endif // LL_LLTRANSFERSOURCEFILE_H
