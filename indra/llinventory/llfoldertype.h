/**
 * @file llfoldertype.h
 * @brief Declaration of LLFolderType.
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

#ifndef LL_LLFOLDERTYPE_H
#define LL_LLFOLDERTYPE_H

#include <string>

#include "llassettype.h"

// This class handles folder types (similar to assettype, except for folders)
// and operations on those.
class LLFolderType
{
protected:
	LOG_CLASS(LLFolderType);

public:
	// ! BACKWARDS COMPATIBILITY ! Folder type enums must match asset type enums.
	enum EType
	{
		FT_TEXTURE = 0,

		FT_SOUND = 1,

		FT_CALLINGCARD = 2,

		FT_LANDMARK = 3,

		FT_CLOTHING = 5,

		FT_OBJECT = 6,

		FT_NOTECARD = 7,

		FT_ROOT_INVENTORY = 8,

		// Bogus OpenSim root folder type... See:
		// http://opensimulator.org/pipermail/opensim-dev/2015-August/025876.html
		FT_ROOT_INVENTORY_OS = 9,

		FT_LSL_TEXT = 10,

		FT_BODYPART = 13,

		FT_TRASH = 14,

		FT_SNAPSHOT_CATEGORY = 15,

		FT_LOST_AND_FOUND = 16,

		FT_ANIMATION = 20,

		FT_GESTURE = 21,

#if 0	// Viewer 2 folder: useless for v1 ! HB
		FT_FAVORITE = 23,
#endif

		FT_CURRENT_OUTFIT = 46,

		// Viewer 2 folders: useless for v1; this value is yet needed for
		// the new version of AISAPI inventory fetches. HB
		FT_OUTFIT = 47,

		// In the Cool VL Viewer, we actually use this value as a way to
		// differenciate between the folder used for creating clothing items
		// and the one (configurable) to store outfits. It is used exclusively
		// via LLInventoryModel::findChoosenCategoryUUIDForType() for 
		// LLAgentWearables::makeNewOutfit(). We do not use this value anywhere
		// else, even though some users may have a v2+-like inventory with a
		// "My Outfits" folder, which is just considered a normal (and
		// deletable) folder by the Cool VL Viewer. HB
		FT_MY_OUTFITS = 48,

		FT_MESH = 49,

		FT_INBOX = 50,

#if 0
		// Deprecated in SL, never implemented in OpenSim.
		FT_OUTBOX = 51,

		// Viewer 2 folder: useless for v1 ! HB
		FT_BASIC_ROOT = 52,
#endif

		FT_MARKETPLACE_LISTINGS = 53,
		FT_MARKETPLACE_STOCK = 54,
	    // Note: We actually *never* create folders with that type. This is
		// used for icon override only.
		FT_MARKETPLACE_VERSION = 55,

		FT_SETTINGS = 56,

		FT_MATERIAL = 57,

		// OpenSim only. See:
		// http://opensimulator.org/pipermail/opensim-dev/2015-August/025876.html
		FT_SUITCASE = 100,

		FT_NONE = -1
	};

	static EType lookup(const std::string& type_name);
	static const std::string& lookup(EType folder_type);

	static bool lookupIsProtectedType(EType folder_type);

	static LLAssetType::EType folderTypeToAssetType(LLFolderType::EType ftype);
	static LLFolderType::EType assetTypeToFolderType(LLAssetType::EType atype);

	static const std::string& badLookup(); // error string when a lookup fails

	static void setCanDeleteCOF(bool allow)	{ sCanDeleteCOF = allow; }
	static bool getCanDeleteCOF()			{ return sCanDeleteCOF; }

protected:
	// Needed (not deleted), even if never used, because LLViewerFolderType is
	// derived from us...
	LL_INLINE LLFolderType()				{}
	LL_INLINE virtual ~LLFolderType()		{}

private:
	static bool sCanDeleteCOF;
};

#endif // LL_LLFOLDERTYPE_H
