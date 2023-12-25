/**
 * @file lltemplatemessagebuilder.h
 * @brief Declaration of LLTemplateMessageBuilder class.
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

#ifndef LL_LLTEMPLATEMESSAGEBUILDER_H
#define LL_LLTEMPLATEMESSAGEBUILDER_H

#include <map>

#include "llmessagebuilder.h"
#include "llpreprocessor.h"

class LLMsgData;
class LLMessageTemplate;
class LLMsgBlkData;
class LLMessageTemplate;

class LLTemplateMessageBuilder : public LLMessageBuilder
{
protected:
	LOG_CLASS(LLTemplateMessageBuilder);

public:
	typedef std::map<const char*, LLMessageTemplate*> template_name_map_t;

	LLTemplateMessageBuilder(const template_name_map_t&);
	virtual ~LLTemplateMessageBuilder();

	virtual void newMessage(const char* name);

	virtual void nextBlock(const char* blockname);
	// *TODO: Babbage: remove this horror...
	virtual bool removeLastBlock();

	/** All add* methods expect pointers to canonical varname strings. */
	virtual void addBinaryData(const char *varname, const void* data,
							   S32 size);
	virtual void addBool(const char* varname, bool b);
	virtual void addS8(const char* varname, S8 s);
	virtual void addU8(const char* varname, U8 u);
	virtual void addS16(const char* varname, S16 i);
	virtual void addU16(const char* varname, U16 i);
	virtual void addF32(const char* varname, F32 f);
	virtual void addS32(const char* varname, S32 s);
	virtual void addU32(const char* varname, U32 u);
	virtual void addU64(const char* varname, U64 lu);
	virtual void addF64(const char* varname, F64 d);
	virtual void addVector3(const char* varname, const LLVector3& vec);
	virtual void addVector4(const char* varname, const LLVector4& vec);
	virtual void addVector3d(const char* varname, const LLVector3d& vec);
	virtual void addQuat(const char* varname, const LLQuaternion& quat);
	virtual void addUUID(const char* varname, const LLUUID& uuid);
	virtual void addIPAddr(const char* varname, U32 ip);
	virtual void addIPPort(const char* varname, U16 port);
	virtual void addString(const char* varname, const char* s);
	virtual void addString(const char* varname, const std::string& s);

	virtual bool isMessageFull(const char* blockname) const;
	virtual void compressMessage(U8*& buf_ptr, U32& buffer_length);

	LL_INLINE virtual bool isBuilt() const					{ return mSBuilt; }
	LL_INLINE virtual bool isClear() const					{ return mSClear; }

	// Returns the built message size
	virtual U32 buildMessage(U8* buffer, U32 buffer_size, U8 offset_to_data);

	virtual void clearMessage();

	// *TODO: babbage: remove this horror.
	LL_INLINE virtual void setBuilt(bool b)					{ mSBuilt = b; }

	LL_INLINE virtual S32 getMessageSize()					{ return mCurrentSendTotal; }

	LL_INLINE virtual const char* getMessageName() const	{ return mCurrentSMessageName; }

	virtual void copyFromMessageData(const LLMsgData& data);
	virtual void copyFromLLSD(const LLSD&)					{}

	LL_INLINE LLMsgData* getCurrentMessage() const			{ return mCurrentSMessageData; }

private:
	void addData(const char* varname, const void* data, EMsgVariableType type,
				 S32 size);

	void addData(const char* varname, const void* data, EMsgVariableType type);

private:
	LLMsgData*					mCurrentSMessageData;
	const LLMessageTemplate*	mCurrentSMessageTemplate;
	LLMsgBlkData*				mCurrentSDataBlock;
	char*						mCurrentSMessageName;
	char*						mCurrentSBlockName;
	const template_name_map_t&	mMessageTemplates;
	S32							mCurrentSendTotal;
	bool						mSBuilt;
	bool						mSClear;
};

#endif // LL_LLTEMPLATEMESSAGEBUILDER_H
