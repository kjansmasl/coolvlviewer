/**
 * @file   llmaterialid.h
 * @brief  Header file for llmaterialid
 * @author Stinson@lindenlab.com
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

#ifndef LL_LLMATERIALID_H
#define LL_LLMATERIALID_H

#include <string>

#include "llsd.h"
#include "lluuid.h"

class LLMaterialID final
{
public:
	LLMaterialID();
	LLMaterialID(const LLSD& matidp);
	LLMaterialID(const LLSD::Binary& matidp);
	LLMaterialID(const void* memoryp);
	LLMaterialID(const LLMaterialID& other_mat_id);
	LLMaterialID(const LLUUID& uuid);

	// Allow the use of the C++11 default move constructor
	LLMaterialID(LLMaterialID&& other) noexcept = default;

	LL_INLINE bool operator==(const LLMaterialID& other_mat_id) const
	{
		return compareToOtherMaterialID(other_mat_id) == 0;
	}

	LL_INLINE bool operator!=(const LLMaterialID& other_mat_id) const
	{
		return compareToOtherMaterialID(other_mat_id) != 0;
	}

	LL_INLINE bool operator<(const LLMaterialID& other_mat_id) const
	{
		return compareToOtherMaterialID(other_mat_id) < 0;
	}

	LL_INLINE bool operator<=(const LLMaterialID& other_mat_id) const
	{
		return compareToOtherMaterialID(other_mat_id) <= 0;
	}

	LL_INLINE bool operator>(const LLMaterialID& other_mat_id) const
	{
		return compareToOtherMaterialID(other_mat_id) > 0;
	}

	LL_INLINE bool operator>=(const LLMaterialID& other_mat_id) const
	{
		return compareToOtherMaterialID(other_mat_id) >= 0;
	}

	LL_INLINE bool isNull() const
	{
		return compareToOtherMaterialID(LLMaterialID::null) == 0;
	}

	LL_INLINE bool notNull() const
	{
		return compareToOtherMaterialID(LLMaterialID::null) != 0;
	}

	LLMaterialID& operator=(const LLMaterialID& other_mat_id)
	{
		copyFromOtherMaterialID(other_mat_id);
		return *this;
	}

	LL_INLINE const U8* get() const					{ return mID; }

	void set(const void* memoryp);
	void clear();

	LLUUID asUUID() const;
	LLSD asLLSD() const;
	std::string asString() const;

	friend std::ostream& operator<<(std::ostream& s,
									const LLMaterialID& material_id);

	// Returns a 64 bits digest of the material Id, by XORing its two 64 bits
	// long words. HB
	LL_INLINE U64 getDigest64() const
	{
		U64* tmp = (U64*)mID;
		return tmp[0] ^ tmp[1];
	}

public:
	static const LLMaterialID null;

private:
	void parseFromBinary(const LLSD::Binary& matidp);
	void copyFromOtherMaterialID(const LLMaterialID& other_mat_id);
	S32 compareToOtherMaterialID(const LLMaterialID& other_mat_id) const;

public:
	U8 mID[UUID_BYTES];
};

// std::hash implementation for LLMaterialID
namespace std
{
	template<> struct hash<LLMaterialID>
	{
		LL_INLINE size_t operator()(const LLMaterialID& id) const noexcept
		{
			return id.getDigest64();
		}
	};
}

// For use with boost::unordered_map and boost::unordered_set
LL_INLINE size_t hash_value(const LLMaterialID& id) noexcept
{
	return id.getDigest64();
}

#endif // LL_LLMATERIALID_H
