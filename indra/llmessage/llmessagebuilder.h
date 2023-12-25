/**
 * @file llmessagebuilder.h
 * @brief Declaration of LLMessageBuilder class.
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

#ifndef LL_LLMESSAGEBUILDER_H
#define LL_LLMESSAGEBUILDER_H

#include <string>
#include "stdtypes.h"

class LLMsgData;
class LLQuaternion;
class LLSD;
class LLUUID;
class LLVector3;
class LLVector3d;
class LLVector4;

class LLMessageBuilder
{
protected:
	LOG_CLASS(LLMessageBuilder);

public:
	virtual ~LLMessageBuilder() = default;
	virtual void newMessage(const char* name) = 0;

	virtual void nextBlock(const char* blockname) = 0;
	// *TODO: Babbage: remove this horror...
	virtual bool removeLastBlock() = 0;

	/** All add* methods expect pointers to canonical strings. */
	virtual void addBinaryData(const char* varname, const void* data,
							   S32 size) = 0;
	virtual void addBool(const char* varname, bool b) = 0;
	virtual void addS8(const char* varname, S8 s) = 0;
	virtual void addU8(const char* varname, U8 u) = 0;
	virtual void addS16(const char* varname, S16 i) = 0;
	virtual void addU16(const char* varname, U16 i) = 0;
	virtual void addF32(const char* varname, F32 f) = 0;
	virtual void addS32(const char* varname, S32 s) = 0;
	virtual void addU32(const char* varname, U32 u) = 0;
	virtual void addU64(const char* varname, U64 lu) = 0;
	virtual void addF64(const char* varname, F64 d) = 0;
	virtual void addVector3(const char* varname, const LLVector3& vec) = 0;
	virtual void addVector4(const char* varname, const LLVector4& vec) = 0;
	virtual void addVector3d(const char* varname, const LLVector3d& vec) = 0;
	virtual void addQuat(const char* varname, const LLQuaternion& quat) = 0;
	virtual void addUUID(const char* varname, const LLUUID& uuid) = 0;
	virtual void addIPAddr(const char* varname, U32 ip) = 0;
	virtual void addIPPort(const char* varname, U16 port) = 0;
	virtual void addString(const char* varname, const char* s) = 0;
	virtual void addString(const char* varname, const std::string& s) = 0;

	virtual bool isMessageFull(const char* blockname) const = 0;
	virtual void compressMessage(U8*& buf_ptr, U32& buffer_length) = 0;
	virtual S32 getMessageSize() = 0;

	virtual bool isBuilt() const = 0;
	virtual bool isClear() const = 0;

	// Returns the built message size
	virtual U32 buildMessage(U8* buffer, U32 buffer_size,
							 U8 offset_to_data) = 0;

	virtual void clearMessage() = 0;

	// *TODO: Babbage: remove this horror
	virtual void setBuilt(bool b) = 0;

	virtual const char* getMessageName() const = 0;

	virtual void copyFromMessageData(const LLMsgData& data) = 0;
	virtual void copyFromLLSD(const LLSD& data) = 0;
};

typedef enum e_message_variable_type
{
	MVT_NULL,
	MVT_FIXED,
	MVT_VARIABLE,
	MVT_U8,
	MVT_U16,
	MVT_U32,
	MVT_U64,
	MVT_S8,
	MVT_S16,
	MVT_S32,
	MVT_S64,
	MVT_F32,
	MVT_F64,
	MVT_LLVector3,
	MVT_LLVector3d,
	MVT_LLVector4,
	MVT_LLQuaternion,
	MVT_LLUUID,	
	MVT_BOOL,
	MVT_IP_ADDR,
	MVT_IP_PORT,
	MVT_U16Vec3,
	MVT_U16Quat,
	MVT_S16Array,
	MVT_EOL
} EMsgVariableType;

#endif //  LL_LLMESSAGEBUILDER_H
