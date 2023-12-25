/**
 * @file lltemplatemessagebuilder.cpp
 * @brief LLTemplateMessageBuilder class implementation.
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

#include "linden_common.h"

#include "lltemplatemessagebuilder.h"

#include "llmessagetemplate.h"
#include "llmath.h"
#include "llquaternion.h"
#include "llvector3d.h"
#include "llvector3.h"
#include "llvector4.h"

LLTemplateMessageBuilder::LLTemplateMessageBuilder(const template_name_map_t& name_template_map)
:	mCurrentSMessageData(NULL),
	mCurrentSMessageTemplate(NULL),
	mCurrentSDataBlock(NULL),
	mCurrentSMessageName(NULL),
	mCurrentSBlockName(NULL),
	mSBuilt(false),
	mSClear(true),
	mCurrentSendTotal(0),
	mMessageTemplates(name_template_map)
{
}

//virtual
LLTemplateMessageBuilder::~LLTemplateMessageBuilder()
{
	delete mCurrentSMessageData;
	mCurrentSMessageData = NULL;
}

//virtual
void LLTemplateMessageBuilder::newMessage(const char* name)
{
	mSBuilt = false;
	mSClear = false;

	mCurrentSendTotal = 0;

	if (mCurrentSMessageData)
	{
		delete mCurrentSMessageData;
		mCurrentSMessageData = NULL;
	}

	template_name_map_t::const_iterator it = mMessageTemplates.find(name);
	if (it == mMessageTemplates.end())
	{
		llerrs << "Message " << name << " not registered" << llendl;
		return;
	}

	mCurrentSMessageTemplate = it->second;
	mCurrentSMessageData = new LLMsgData(name);
	mCurrentSMessageName = (char*)name;
	mCurrentSDataBlock = NULL;
	mCurrentSBlockName = NULL;

	// Add at one of each block
	const LLMessageTemplate* msg_template =
		mMessageTemplates.find(name)->second;

	if (msg_template->getDeprecation() != MD_NOTDEPRECATED)
	{
		llwarns << "Sending deprecated message " << name << llendl;
	}

	for (LLMessageTemplate::message_block_map_t::const_iterator
			iter = msg_template->mMemberBlocks.begin(),
			end = msg_template->mMemberBlocks.end();
		 iter != end; ++iter)
	{
		LLMessageBlock* ci = *iter;
		mCurrentSMessageData->addBlock(new LLMsgBlkData(ci->mName, 0));
	}
}

//virtual
void LLTemplateMessageBuilder::clearMessage()
{
	mSBuilt = false;
	mSClear = true;

	mCurrentSendTotal = 0;

	mCurrentSMessageTemplate = NULL;

	delete mCurrentSMessageData;
	mCurrentSMessageData = NULL;

	mCurrentSMessageName = NULL;
	mCurrentSDataBlock = NULL;
	mCurrentSBlockName = NULL;
}

//virtual
void LLTemplateMessageBuilder::nextBlock(const char* blockname)
{
	char* bnamep = (char*)blockname;

	if (!mCurrentSMessageTemplate)
	{
		llerrs << "newMessage not called prior to setBlock" << llendl;
		return;
	}

	// now, does this block exist ?
	const LLMessageBlock* template_data =
		mCurrentSMessageTemplate->getBlock(bnamep);
	if (!template_data)
	{
		llerrs << bnamep << " is not a block in "
			   << mCurrentSMessageTemplate->mName << llendl;
		return;
	}

	// OK, have we already set this block ?
	LLMsgBlkData* block_data = mCurrentSMessageData->mMemberBlocks[bnamep];
	if (block_data->mBlockNumber == 0)
	{
		// nope ! set this as the current block
		block_data->mBlockNumber = 1;
		mCurrentSDataBlock = block_data;
		mCurrentSBlockName = bnamep;

		// add placeholders for each of the variables
		for (LLMessageBlock::message_variable_map_t::const_iterator
				iter = template_data->mMemberVariables.begin();
			 iter != template_data->mMemberVariables.end(); ++iter)
		{
			LLMessageVariable& ci = **iter;
			mCurrentSDataBlock->addVariable(ci.getName(), ci.getType());
		}
		return;
	}
	else
	{
		// Already have this block; are we supposed to have a new one ?

		// if the block is type MBT_SINGLE this is bad !
		if (template_data->mType == MBT_SINGLE)
		{
			llerrs << "Call done multiple times for " << bnamep
				   << " which is type MBT_SINGLE" << llendl;
			return;
		}

		// If the block is type MBT_MULTIPLE then we need a known number,
		// make sure that we're not exceeding it
		if (template_data->mType == MBT_MULTIPLE &&
			mCurrentSDataBlock->mBlockNumber == template_data->mNumber)
		{
			llerrs << "Called " << mCurrentSDataBlock->mBlockNumber
				   << " times for " << bnamep << ", exceeding "
				   << template_data->mNumber
				   << " specified in type MBT_MULTIPLE." << llendl;
			return;
		}

		// OK, we can make a new one; modify the name to avoid name collision
		// by adding number to end.
		S32 count = block_data->mBlockNumber;

		// incrememt base name's count
		block_data->mBlockNumber++;

		if (block_data->mBlockNumber > MAX_BLOCKS)
		{
			llerrs << "Trying to pack too many blocks into MBT_VARIABLE type "
				   << "(limited to " << MAX_BLOCKS << ")" << llendl;
		}

		// Create new name. NB: if things are working correctly, then
		// mCurrentMessageData->mMemberBlocks[blockname]->mBlockNumber ==
		// mCurrentDataBlock->mBlockNumber + 1

		char* nbnamep = bnamep + count;

		mCurrentSDataBlock = new LLMsgBlkData(bnamep, count);
		mCurrentSDataBlock->mName = nbnamep;
		mCurrentSMessageData->mMemberBlocks[nbnamep] = mCurrentSDataBlock;

		// Add placeholders for each of the variables
		for (LLMessageBlock::message_variable_map_t::const_iterator
				 iter = template_data->mMemberVariables.begin(),
				 end = template_data->mMemberVariables.end();
			 iter != end; ++iter)
		{
			LLMessageVariable& ci = **iter;
			mCurrentSDataBlock->addVariable(ci.getName(), ci.getType());
		}
	}
}

// *TODO: Remove this horror...
bool LLTemplateMessageBuilder::removeLastBlock()
{
	if (!mCurrentSBlockName || !mCurrentSMessageData || !mCurrentSMessageTemplate ||
		mCurrentSMessageData->mMemberBlocks[mCurrentSBlockName]->mBlockNumber < 1)
	{
		return false;
	}

	// At least one block for the current block name.

	// Store the current block name for future reference.
	char* block_name = mCurrentSBlockName;

	// Decrement the sent total by the size of the data in the message block
	// that we are currently building.

	const LLMessageBlock* template_data =
		mCurrentSMessageTemplate->getBlock(mCurrentSBlockName);

	for (LLMessageBlock::message_variable_map_t::const_iterator
			iter = template_data->mMemberVariables.begin(),
				end = template_data->mMemberVariables.end();
		 iter != end; ++iter)
	{
		LLMessageVariable& ci = **iter;
		mCurrentSendTotal -= ci.getSize();
	}

	// Now we want to find the block that we're blowing away.

	// Get the number of blocks.
	LLMsgBlkData* block_data = mCurrentSMessageData->mMemberBlocks[block_name];
	S32 num_blocks = block_data->mBlockNumber;

	// Use the same (suspect?) algorithm that's used to generate the names in
	// the nextBlock method to find it.
	char* block_getting_whacked = block_name + num_blocks - 1;

	LLMsgBlkData* whacked_data =
		mCurrentSMessageData->mMemberBlocks[block_getting_whacked];
	delete whacked_data;

	mCurrentSMessageData->mMemberBlocks.erase(block_getting_whacked);

	if (num_blocks <= 1)
	{
		// We just blew away the last one, so return false
		llwarns << "not blowing away the only block of message "
				<< mCurrentSMessageName << ". Block: " << block_name
				<< ". Number: " << num_blocks << llendl;
		return false;
	}

	// Decrement the counter.
	--block_data->mBlockNumber;
	return true;
}

// Add data to variable in current block
void LLTemplateMessageBuilder::addData(const char* varname, const void* data,
									   EMsgVariableType type, S32 size)
{
	char* vnamep = (char*)varname;

	// Do we have a current message ?
	if (!mCurrentSMessageTemplate)
	{
		llerrs << "newMessage not called prior to addData" << llendl;
		return;
	}

	// Do we have a current block ?
	if (!mCurrentSDataBlock)
	{
		llerrs << "setBlock not called prior to addData" << llendl;
		return;
	}

	// Add the data if it exists
	const LLMessageVariable* var_data =
		mCurrentSMessageTemplate->getBlock(mCurrentSBlockName)->getVariable(vnamep);
	if (!var_data || !var_data->getName())
	{
		llerrs << vnamep << " not a variable in block " << mCurrentSBlockName
			   << " of " << mCurrentSMessageTemplate->mName << llendl;
		return;
	}

	// Are we the correct size ?
	if (var_data->getType() == MVT_VARIABLE)
	{
		// Variable 1 can only store 255 bytes, make sure our data is smaller
		if (var_data->getSize() == 1 && size > 255)
		{
			llwarns_once << "Field " << varname
						 << " is a Variable 1 (255 bytes max) but program attempted to stuff "
						 << size << " bytes. Truncating data." << llendl;
			size = 255;
			char* truncate = (char*)data;
			 // Array size is 255 but the last element index is 254
			truncate[254] = 0;
		}

		// No correct size for MVT_VARIABLE, instead we need to tell how many
		// bytes the size will be encoded as
		mCurrentSDataBlock->addData(vnamep, data, size, type,
									var_data->getSize());
		mCurrentSendTotal += size;
	}
	else
	{
		if (size != var_data->getSize())
		{
			llerrs << varname << " is type MVT_FIXED but request size "
				   << size << " doesn't match template size "
				   << var_data->getSize() << llendl;
			return;
		}

		// Alright, smash it in
		mCurrentSDataBlock->addData(vnamep, data, size, type);
		mCurrentSendTotal += size;
	}
}

// add data to variable in current block - fails if variable isn't MVT_FIXED
void LLTemplateMessageBuilder::addData(const char* varname, const void* data,
									   EMsgVariableType type)
{
	char* vnamep = (char*)varname;

	// Do we have a current message ?
	if (!mCurrentSMessageTemplate)
	{
		llerrs << "newMessage not called prior to addData" << llendl;
		return;
	}

	// Do we have a current block ?
	if (!mCurrentSDataBlock)
	{
		llerrs << "setBlock not called prior to addData" << llendl;
		return;
	}

	// Add the data if it exists
	const LLMessageVariable* var_data =
		mCurrentSMessageTemplate->getBlock(mCurrentSBlockName)->getVariable(vnamep);
	if (!var_data->getName())
	{
		llerrs << vnamep << " not a variable in block " << mCurrentSBlockName
			   << " of " << mCurrentSMessageTemplate->mName << llendl;
		return;
	}

	// Are we MVT_VARIABLE ?
	if (var_data->getType() == MVT_VARIABLE)
	{
		// nope
		llerrs << vnamep
			   << " is type MVT_VARIABLE. Call using addData(name, data, size)"
			   << llendl;
		return;
	}
	else
	{
		mCurrentSDataBlock->addData(vnamep, data, var_data->getSize(), type);
		mCurrentSendTotal += var_data->getSize();
	}
}

void LLTemplateMessageBuilder::addBinaryData(const char* varname,
											 const void* data, S32 size)
{
	addData(varname, data, MVT_FIXED, size);
}

void LLTemplateMessageBuilder::addS8(const char* varname, S8 s)
{
	addData(varname, &s, MVT_S8, sizeof(s));
}

void LLTemplateMessageBuilder::addU8(const char* varname, U8 u)
{
	addData(varname, &u, MVT_U8, sizeof(u));
}

void LLTemplateMessageBuilder::addS16(const char* varname, S16 i)
{
	addData(varname, &i, MVT_S16, sizeof(i));
}

void LLTemplateMessageBuilder::addU16(const char* varname, U16 i)
{
	addData(varname, &i, MVT_U16, sizeof(i));
}

void LLTemplateMessageBuilder::addF32(const char* varname, F32 f)
{
	addData(varname, &f, MVT_F32, sizeof(f));
}

void LLTemplateMessageBuilder::addS32(const char* varname, S32 s)
{
	addData(varname, &s, MVT_S32, sizeof(s));
}

void LLTemplateMessageBuilder::addU32(const char* varname, U32 u)
{
	addData(varname, &u, MVT_U32, sizeof(u));
}

void LLTemplateMessageBuilder::addU64(const char* varname, U64 lu)
{
	addData(varname, &lu, MVT_U64, sizeof(lu));
}

void LLTemplateMessageBuilder::addF64(const char* varname, F64 d)
{
	addData(varname, &d, MVT_F64, sizeof(d));
}

void LLTemplateMessageBuilder::addIPAddr(const char* varname, U32 u)
{
	addData(varname, &u, MVT_IP_ADDR, sizeof(u));
}

void LLTemplateMessageBuilder::addIPPort(const char* varname, U16 u)
{
	u = htons(u);
	addData(varname, &u, MVT_IP_PORT, sizeof(u));
}

void LLTemplateMessageBuilder::addBool(const char* varname, bool b)
{
	U8 temp = b;
	addData(varname, &temp, MVT_BOOL, sizeof(temp));
}

void LLTemplateMessageBuilder::addString(const char* varname, const char* s)
{
	if (s)
	{
		addData(varname, (void*)s, MVT_VARIABLE, (S32)strlen(s) + 1);
	}
	else
	{
		addData(varname, NULL, MVT_VARIABLE, 0);
	}
}

void LLTemplateMessageBuilder::addString(const char* varname,
										 const std::string& s)
{
	if (s.size())
	{
		addData(varname, (void*)s.c_str(), MVT_VARIABLE, (S32)s.size() + 1);
	}
	else
	{
		addData(varname, NULL, MVT_VARIABLE, 0);
	}
}

void LLTemplateMessageBuilder::addVector3(const char* varname,
										  const LLVector3& vec)
{
	addData(varname, vec.mV, MVT_LLVector3, sizeof(vec.mV));
}

void LLTemplateMessageBuilder::addVector4(const char* varname,
										  const LLVector4& vec)
{
	addData(varname, vec.mV, MVT_LLVector4, sizeof(vec.mV));
}

void LLTemplateMessageBuilder::addVector3d(const char* varname,
										   const LLVector3d& vec)
{
	addData(varname, vec.mdV, MVT_LLVector3d, sizeof(vec.mdV));
}

void LLTemplateMessageBuilder::addQuat(const char* varname,
									   const LLQuaternion& quat)
{
	addData(varname, quat.packToVector3().mV, MVT_LLQuaternion,
			sizeof(LLVector3));
}

void LLTemplateMessageBuilder::addUUID(const char* varname, const LLUUID& uuid)
{
	addData(varname, uuid.mData, MVT_LLUUID, sizeof(uuid.mData));
}

static S32 zero_code(U8** data, U32* data_size)
{
	// Encoded send buffer needs to be slightly larger since the zero coding
	// can potentially increase the size of the send data.
	static U8 encodedSendBuffer[2 * MAX_BUFFER_SIZE];

	S32 count = *data_size;

	S32 net_gain = 0;
	U8 num_zeroes = 0;

	U8* inptr = (U8*)*data;
	U8* outptr = (U8*)encodedSendBuffer;

	// Skip the packet id field
	for (U32 ii = 0; ii < LL_PACKET_ID_SIZE ; ++ii)
	{
		--count;
		*outptr++ = *inptr++;
	}

	// Build encoded packet, keeping track of net size gain

	// Sequential zero bytes are encoded as 0 [U8 count]  with 0 0 [count]
	// representing wrap (>256 zeroes)

	while (count--)
	{
		if (!(*inptr))   // in a zero count
		{
			if (num_zeroes)
			{
				if (++num_zeroes > 254)
				{
					*outptr++ = num_zeroes;
					num_zeroes = 0;
				}
				--net_gain;   // subseqent zeroes save one
			}
			else
			{
				*outptr++ = 0;
				++net_gain;  // starting a zero count adds one
				num_zeroes = 1;
			}
			++inptr;
		}
		else
		{
			if (num_zeroes)
			{
				*outptr++ = num_zeroes;
				num_zeroes = 0;
			}
			*outptr++ = *inptr++;
		}
	}

	if (num_zeroes)
	{
		*outptr++ = num_zeroes;
	}

	if (net_gain < 0)
	{
#if 0	// *TODO: babbage: reinstate stat collecting...
		++mCompressedPacketsOut;
		mUncompressedBytesOut += *data_size;
#endif
		*data = encodedSendBuffer;
		*data_size += net_gain;
		// Set the head bit to indicate zero coding
		encodedSendBuffer[0] |= LL_ZERO_CODE_FLAG;

#if 0	// *TODO: babbage: reinstate stat collecting...
		mCompressedBytesOut += *data_size;
#endif

	}
#if 0	// *TODO: babbage: reinstate stat collecting...
	mTotalBytesOut += *data_size;
#endif

	return net_gain;
}

void LLTemplateMessageBuilder::compressMessage(U8*& buf_ptr, U32& buffer_size)
{
	if (mCurrentSMessageTemplate->getEncoding() == ME_ZEROCODED)
	{
		zero_code(&buf_ptr, &buffer_size);
	}
}

bool LLTemplateMessageBuilder::isMessageFull(const char* blockname) const
{
	if (mCurrentSendTotal > MTUBYTES)
	{
		return true;
	}
	if (!blockname)
	{
		return false;
	}

	char* bnamep = (char*)blockname;
	const LLMessageBlock* template_data =
		mCurrentSMessageTemplate->getBlock(bnamep);

	S32 max;
	switch (template_data->mType)
	{
		case MBT_SINGLE:
			max = 1;
			break;

		case MBT_MULTIPLE:
			max = template_data->mNumber;
			break;

		case MBT_VARIABLE:
		default:
			max = MAX_BLOCKS;
	}

	return mCurrentSMessageData->mMemberBlocks[bnamep]->mBlockNumber >= max;
}

static S32 buildBlock(U8* buffer, S32 buffer_size,
					  const LLMessageBlock* template_data,
					  LLMsgData* message_data)
{
	S32 result = 0;
	LLMsgData::msg_blk_data_map_t::const_iterator block_iter;
	block_iter = message_data->mMemberBlocks.find(template_data->mName);
	const LLMsgBlkData* mbci = block_iter->second;

	// OK, if this is the first block of a repeating pack, set block_count and,
	// if it is type MBT_VARIABLE encode a byte for how many there are.
	S32 block_count = mbci->mBlockNumber;
	if (template_data->mType == MBT_VARIABLE)
	{
		// Remember that mBlockNumber is a S32
		U8 temp_block_number = (U8)mbci->mBlockNumber;
		if ((S32)(result + sizeof(U8)) < MAX_BUFFER_SIZE)
		{
			memcpy(&buffer[result], &temp_block_number, sizeof(U8));
			result += sizeof(U8);
		}
		else
		{
			// Just reporting error is likely not enough. Need to check how to
			// abort or error out gracefully from this function.
			llerrs << "buildBlock failed. Message excedding sendBuffersize."
				   << llendl;
		}
	}
	else if (template_data->mType == MBT_MULTIPLE)
	{
		if (block_count != template_data->mNumber)
		{
			// Nope !  Need to fill it in all the way !
			llerrs << "Block " << mbci->mName
				<< " is type MBT_MULTIPLE but only has data for "
				<< block_count << " out of its "
				<< template_data->mNumber << " blocks" << llendl;
		}
	}

	while (block_count > 0)
	{
		// Now loop through the variables
		for (LLMsgBlkData::msg_var_data_map_t::const_iterator
				iter = mbci->mMemberVarData.begin();
			 iter != mbci->mMemberVarData.end(); iter++)
		{
			const LLMsgVarData& mvci = *iter;
			if (mvci.getSize() == -1)
			{
				// Oops, this variable was never set !
				llerrs << "The variable " << mvci.getName() << " in block "
					   << mbci->mName << " of message " << template_data->mName
					   << " was not set prior to buildMessage call" << llendl;
			}
			else
			{
				S32 data_size = mvci.getDataSize();
				if (data_size > 0)
				{
					// The type is MVT_VARIABLE, which means that we need to
					// encode a size argument. Otherwise, there is no need.
					S32 size = mvci.getSize();
					U8 sizeb;
					U16 sizeh;
					switch (data_size)
					{
						case 1:
							sizeb = size;
							htonmemcpy(&buffer[result], &sizeb, MVT_U8, 1);
							break;

						case 2:
							sizeh = size;
							htonmemcpy(&buffer[result], &sizeh, MVT_U16, 2);
							break;

						case 4:
							htonmemcpy(&buffer[result], &size, MVT_S32, 4);
							break;

						default:
							llerrs << "Attempting to build variable field with unknown size of "
								   << size << llendl;
							break;
					}
					result += mvci.getDataSize();
				}

				// If there is any data to pack, pack it
				if (mvci.getData() != NULL && mvci.getSize())
				{
					if (result + mvci.getSize() < buffer_size)
					{
					    memcpy(&buffer[result], mvci.getData(),
							   mvci.getSize());
					    result += mvci.getSize();
					}
					else
					{
					    // Just reporting error is likely not enough. Need to
						// check how to abort or error out gracefully from this
						// function. XXXTBD
						llerrs << "Failed attempted to pack "
							   << (result + mvci.getSize())
							   << " bytes into a buffer with size "
							   << buffer_size << "." << llendl;
					}
				}
			}
		}

		--block_count;
		if (block_iter != message_data->mMemberBlocks.end())
		{
			++block_iter;
			if (block_iter != message_data->mMemberBlocks.end())
			{
				mbci = block_iter->second;
			}
		}
	}

	return result;
}

// Make sure that all the desired data is in place and then copy the data into
// MAX_BUFFER_SIZEd buffer
U32 LLTemplateMessageBuilder::buildMessage(U8* buffer, U32 buffer_size,
										   U8 offset_to_data)
{
	// Basic algorithm is to loop through the various pieces, building size and
	// offset info if we encounter a -1 for mSize at any point that variable
	// wasn't given data.

	// Do we have a current message ?
	if (!mCurrentSMessageTemplate)
	{
		llerrs << "newMessage not called prior to buildMessage" << llendl;
		return 0;
	}

	// Leave room for flags, packet sequence #, and data offset
	// information.
	buffer[PHL_OFFSET] = offset_to_data;
	U32 result = LL_PACKET_ID_SIZE;

	// Encode message number and adjust total_offset
	if (mCurrentSMessageTemplate->mFrequency == MFT_HIGH)
	{
#if 0	// old, endian-dependant way
		memcpy(&buffer[result], &mCurrentMessageTemplate->mMessageNumber,
			   sizeof(U8));
#else 	// new, independant way
		buffer[result] = (U8)mCurrentSMessageTemplate->mMessageNumber;
#endif
		result += sizeof(U8);
	}
	else if (mCurrentSMessageTemplate->mFrequency == MFT_MEDIUM)
	{
		U8 temp = 255;
		memcpy(&buffer[result], &temp, sizeof(U8));
		result += sizeof(U8);

		// Mask off unsightly bits
		temp = mCurrentSMessageTemplate->mMessageNumber & 255;
		memcpy(&buffer[result], &temp, sizeof(U8));
		result += sizeof(U8);
	}
	else if (mCurrentSMessageTemplate->mFrequency == MFT_LOW)
	{
		U8 temp = 255;
		U16  message_num;
		memcpy(&buffer[result], &temp, sizeof(U8));
		result += sizeof(U8);
		memcpy(&buffer[result], &temp, sizeof(U8));
		result += sizeof(U8);

		// Mask off unsightly bits
		message_num = mCurrentSMessageTemplate->mMessageNumber & 0xFFFF;

	    // Convert to network byte order
		message_num = htons(message_num);
		memcpy(&buffer[result], &message_num, sizeof(U16));
		result += sizeof(U16);
	}
	else
	{
		llerrs << "unexpected message frequency in buildMessage" << llendl;
		return 0;
	}

	// Fast forward through the offset and build the message
	result += offset_to_data;
	for (LLMessageTemplate::message_block_map_t::const_iterator
			iter = mCurrentSMessageTemplate->mMemberBlocks.begin(),
			end = mCurrentSMessageTemplate->mMemberBlocks.end();
		 iter != end;
		++iter)
	{
		result += buildBlock(buffer + result, buffer_size - result, *iter,
							 mCurrentSMessageData);
	}
	mSBuilt = true;

	return result;
}

void LLTemplateMessageBuilder::copyFromMessageData(const LLMsgData& data)
{
	// Counting variables used to encode multiple block info
	S32 block_count = 0;
    char* block_name = NULL;

	// Loop through msg blocks to loop through variables, totalling up size
	// data and filling the new (send) message
	for (LLMsgData::msg_blk_data_map_t::const_iterator
			iter = data.mMemberBlocks.begin(),
			end = data.mMemberBlocks.end();
		 iter != end; ++iter)
	{
		const LLMsgBlkData* mbci = iter->second;
		if (!mbci) continue;

		// Do we need to encode a block code ?
		if (block_count == 0)
		{
			block_count = mbci->mBlockNumber;
			block_name = (char*)mbci->mName;
		}

		// Counting down mutliple blocks
		--block_count;

		nextBlock(block_name);

		// Now loop through the variables
		for (LLMsgBlkData::msg_var_data_map_t::const_iterator
				dit = mbci->mMemberVarData.begin(),
				dend = mbci->mMemberVarData.end();
			 dit != dend; ++dit)
		{
			const LLMsgVarData& mvci = *dit;
			addData(mvci.getName(), mvci.getData(), mvci.getType(),
					mvci.getSize());
		}
	}
}
