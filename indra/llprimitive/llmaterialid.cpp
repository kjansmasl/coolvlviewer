/**
 * @file llmaterialid.cpp
 * @brief Implementation of llmaterialid
 * @author Stinson@lindenlab.com
 *
 * $LicenseInfo:firstyear=2012&license=viewergpl$
 *
 * Copyright (c) 2012, Linden Research, Inc.
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

#include "llmaterialid.h"

const LLMaterialID LLMaterialID::null;

LLMaterialID::LLMaterialID()
{
	clear();
}

LLMaterialID::LLMaterialID(const LLSD& matidp)
{
	if (matidp.isBinary())
	{
		parseFromBinary(matidp.asBinary());
	}
	else if (matidp.isUUID())
	{
		set(matidp.asUUID().mData);
	}
	else
	{
		llwarns << "Non-binary and non-UUID material LLSD: "
				<< matidp << llendl;
		llassert(false);
		clear();
	}
}

LLMaterialID::LLMaterialID(const LLSD::Binary& matidp)
{
	parseFromBinary(matidp);
}

LLMaterialID::LLMaterialID(const void* memoryp)
{
	set(memoryp);
}

LLMaterialID::LLMaterialID(const LLMaterialID& other_mat_id)
{
	copyFromOtherMaterialID(other_mat_id);
}

LLMaterialID::LLMaterialID(const LLUUID& uuid)
{
	set(uuid.mData);
}

void LLMaterialID::set(const void* memoryp)
{
	// Assumes that the required size of memory is available
	if (memoryp)
	{
		memcpy(mID, memoryp, UUID_BYTES * sizeof(U8));
	}
	else
	{
		llwarns << "NULL memory pointer passed !" << llendl;
		llassert(false);
		clear();
	}
}

void LLMaterialID::clear()
{
	memset(mID, 0, UUID_BYTES * sizeof(U8));
}

LLUUID LLMaterialID::asUUID() const
{
	LLUUID ret;
	memcpy(ret.mData, mID, UUID_BYTES * sizeof(U8));
	return ret;
}

LLSD LLMaterialID::asLLSD() const
{
	LLSD::Binary mat_id_binary;
	mat_id_binary.resize(UUID_BYTES * sizeof(U8));
	memcpy(mat_id_binary.data(), mID, UUID_BYTES * sizeof(U8));
	return LLSD(mat_id_binary);
}

std::string LLMaterialID::asString() const
{
	std::string mat_id_str;
	for (size_t i = 0; i < UUID_BYTES / sizeof(U32); ++i)
	{
		if (i != 0)
		{
			mat_id_str += "-";
		}
		const U32* value =
			reinterpret_cast<const U32*>(&get()[i * sizeof(U32)]);
		mat_id_str += llformat("%08x", *value);
	}
	return mat_id_str;
}

std::ostream& operator<<(std::ostream& s, const LLMaterialID &material_id)
{
	s << material_id.asString();
	return s;
}

void LLMaterialID::parseFromBinary(const LLSD::Binary& matidp)
{
	llassert(matidp.size() == (UUID_BYTES * sizeof(U8)));
	memcpy(mID, &matidp[0], UUID_BYTES * sizeof(U8));
}

void LLMaterialID::copyFromOtherMaterialID(const LLMaterialID& other_mat_id)
{
	memcpy(mID, other_mat_id.get(), UUID_BYTES * sizeof(U8));
}

S32 LLMaterialID::compareToOtherMaterialID(const LLMaterialID& other_mat_id) const
{
	S32 retval = 0;

	for (size_t i = 0; retval == 0 && i < UUID_BYTES / sizeof(U32); ++i)
	{
		const U32* this_val =
			reinterpret_cast<const U32*>(&get()[i * sizeof(U32)]);
		const U32* other_val =
			reinterpret_cast<const U32*>(&other_mat_id.get()[i * sizeof(U32)]);
		retval = *this_val < *other_val ? -1
										: (*this_val > *other_val ? 1 : 0);
	}

	return retval;
}
