/**
 * @file llsdmessagereader.h
 * @brief LLSDMessageReader class Declaration
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 *
 * Copyright (c) 2007-2009, Linden Research, Inc.
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

#ifndef LL_LLSDMESSAGEREADER_H
#define LL_LLSDMESSAGEREADER_H

#include "llmessagereader.h"
#include "llsd.h"

#include <map>

class LLMessageTemplate;
class LLMsgData;

class LLSDMessageReader final : public LLMessageReader
{
public:
	LLSDMessageReader();

	/** All get* methods expect pointers to canonical strings. */
	void getBinaryData(const char* block, const char* var, void* datap,
					   S32 size, S32 blocknum = 0,
					   S32 max_size = S32_MAX) override;
	void getBool(const char* block, const char* var, bool& data,
				 S32 blocknum = 0) override;
	void getS8(const char* block, const char* var, S8& data,
			   S32 blocknum = 0) override;
	void getU8(const char* block, const char* var, U8& data,
			   S32 blocknum = 0) override;
	void getS16(const char* block, const char* var, S16& data,
				S32 blocknum = 0) override;
	void getU16(const char* block, const char* var, U16& data,
				S32 blocknum = 0) override;
	void getS32(const char* block, const char* var, S32& data,
				S32 blocknum = 0) override;
	void getF32(const char* block, const char* var, F32& data,
				S32 blocknum = 0) override;
	void getU32(const char* block, const char* var, U32& data,
				S32 blocknum = 0) override;
	void getU64(const char* block, const char* var, U64& data,
				S32 blocknum = 0) override;
	void getF64(const char* block, const char* var, F64& data,
				S32 blocknum = 0) override;
	void getVector3(const char* block, const char* var, LLVector3& vec,
					S32 blocknum = 0) override;
	void getVector4(const char* block, const char* var, LLVector4& vec,
					S32 blocknum = 0) override;
	void getVector3d(const char* block, const char* var, LLVector3d& vec,
					 S32 blocknum = 0) override;
	void getQuat(const char* block, const char* var, LLQuaternion& q,
				 S32 blocknum = 0) override;
	void getUUID(const char* block, const char* var, LLUUID& uuid,
				 S32 blocknum = 0) override;
	void getIPAddr(const char* block, const char* var, U32& ip,
				   S32 blocknum = 0) override;
	void getIPPort(const char* block, const char* var, U16& port,
				   S32 blocknum = 0) override;
	void getString(const char* block, const char* var, S32 buffer_size,
				   char* buffer, S32 blocknum = 0) override;
	void getString(const char* block, const char* var, std::string& outstr,
				   S32 blocknum = 0) override;

	S32	getNumberOfBlocks(const char* blockname) override;
	S32	getSize(const char* blockname, const char* varname) override;
	S32	getSize(const char* blockname, S32 blocknum,
				const char* varname) override;

	void clearMessage() override;

	const char* getMessageName() const override;
	S32 getMessageSize() const override;

	void copyToBuilder(LLMessageBuilder&) const override;

	/** Expects a pointer to a canonical name string */
	void setMessage(const char* name, const LLSD& msg);

private:
	const char* mMessageName; // Canonical (prehashed) string.
	LLSD mMessage;
};

#endif // LL_LLSDMESSAGEREADER_H
