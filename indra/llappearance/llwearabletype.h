/**
 * @file llwearabletype.h
 * @brief LLWearableType class header file
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 *
 * Copyright (c) 2010, Linden Research, Inc.
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

#ifndef LL_LLWEARABLETYPE_H
#define LL_LLWEARABLETYPE_H

#include "llassettype.h"
#include "llinventorytype.h"

// Purely static class
class LLWearableType
{
	LLWearableType() = delete;
	~LLWearableType() = delete;

public:
	enum EType
	{
		WT_SHAPE = 0,
		WT_SKIN,
		WT_HAIR,
		WT_EYES,
		WT_SHIRT,
		WT_PANTS,
		WT_SHOES,
		WT_SOCKS,
		WT_JACKET,
		WT_GLOVES,
		WT_UNDERSHIRT,
		WT_UNDERPANTS,
		WT_SKIRT,
		WT_ALPHA,
		WT_TATTOO,
		WT_PHYSICS,
		WT_UNIVERSAL,
		WT_COUNT,

		WT_INVALID = 255,
		WT_NONE = -1,
	};

	static void initClass(LLTranslationBridge::ptr_t trans);
	static void cleanupClass();

	static const std::string& getTypeName(EType type);
	static std::string getCapitalizedTypeName(EType type);
	static const std::string& getTypeDefaultNewName(EType type);
	static const std::string& getTypeLabel(EType type);
	static LLAssetType::EType getAssetType(EType type);
	static EType typeNameToType(const std::string& type_name);
	static LLInventoryType::EIconName getIconName(EType type);
	static bool getDisableCameraSwitch(EType type);
	static bool getAllowMultiwear(EType type);
	static EType inventoryFlagsToWearableType(U32 flags);
};

#endif  // LL_LLWEARABLETYPE_H
