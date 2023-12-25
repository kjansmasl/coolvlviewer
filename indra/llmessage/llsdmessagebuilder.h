/**
 * @file llsdmessagebuilder.h
 * @brief Declaration of LLSDMessageBuilder class.
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

#ifndef LL_LLSDMESSAGEBUILDER_H
#define LL_LLSDMESSAGEBUILDER_H

#include <map>

#include "llmessagebuilder.h"
#include "llsd.h"

class LLMessageTemplate;
class LLMsgData;

class LLSDMessageBuilder final : public LLMessageBuilder
{
protected:
	LOG_CLASS(LLSDMessageBuilder);

public:
	LLSDMessageBuilder();

	void newMessage(const char* name) override;

	void nextBlock(const char* blockname) override;

	// *TODO: Babbage: remove this horror...
	LL_INLINE bool removeLastBlock() override				{ return false; }

	/** All add* methods expect pointers to canonical varname strings. */
	void addBinaryData(const char* varname, const void* data,
					   S32 size) override;
	void addBool(const char* varname, bool b) override;
	void addS8(const char* varname, S8 s) override;
	void addU8(const char* varname, U8 u) override;
	void addS16(const char* varname, S16 i) override;
	void addU16(const char* varname, U16 i) override;
	void addF32(const char* varname, F32 f) override;
	void addS32(const char* varname, S32 s) override;
	void addU32(const char* varname, U32 u) override;
	void addU64(const char* varname, U64 lu) override;
	void addF64(const char* varname, F64 d) override;
	void addVector3(const char* varname, const LLVector3& vec) override;
	void addVector4(const char* varname, const LLVector4& vec) override;
	void addVector3d(const char* varname, const LLVector3d& vec) override;
	void addQuat(const char* varname, const LLQuaternion& quat) override;
	void addUUID(const char* varname, const LLUUID& uuid) override;
	void addIPAddr(const char* varname, U32 ip) override;
	void addIPPort(const char* varname, U16 port) override;
	void addString(const char* varname, const char* s) override;
	void addString(const char* varname, const std::string& s) override;

	LL_INLINE bool isMessageFull(const char*) const override
	{
		return false;
	}

	LL_INLINE void compressMessage(U8*&, U32&) override		{}

	LL_INLINE bool isBuilt() const override					{ return mSBuilt; }
	LL_INLINE bool isClear() const override					{ return mSClear; }

	// Null implementation which returns 0
	LL_INLINE U32 buildMessage(U8*, U32, U8) override		{ return 0; }

	void clearMessage() override;

	// *TODO: babbage: remove this horror.
	LL_INLINE void setBuilt(bool b) override				{ mSBuilt = b; }

	// Babbage: size is unknown as message stored as LLSD. Return non-zero if
	// pending data, as send can be skipped for 0 size. Return 1 to encourage
	// senders checking size against splitting message.
	LL_INLINE S32 getMessageSize() override					{ return mCurrentMessage.size() ? 1 : 0; }

	LL_INLINE const char* getMessageName() const override	{ return mCurrentMessageName.c_str(); }

	void copyFromMessageData(const LLMsgData& data) override;

	void copyFromLLSD(const LLSD& msg) override;

	LL_INLINE const LLSD& getMessage() const				{  return mCurrentMessage; }

private:

	/* mCurrentMessage is of the following format:
		mCurrentMessage = { 'block_name1' : [ { 'block1_field1' : 'b1f1_data',
												'block1_field2' : 'b1f2_data',
												...
												'block1_fieldn' : 'b1fn_data'},
											{ 'block2_field1' : 'b2f1_data',
												'block2_field2' : 'b2f2_data',
												...
												'block2_fieldn' : 'b2fn_data'},
											...
											{ 'blockm_field1' : 'bmf1_data',
												'blockm_field2' : 'bmf2_data',
												...
												'blockm_fieldn' : 'bmfn_data'} ],
							'block_name2' : ...,
							...
							'block_namem' }
	*/
	LLSD		mCurrentMessage;
	LLSD*		mCurrentBlock;
	std::string	mCurrentMessageName;
	std::string	mCurrentBlockName;
	bool		mSBuilt;
	bool		mSClear;
};

#endif // LL_LLSDMESSAGEBUILDER_H
