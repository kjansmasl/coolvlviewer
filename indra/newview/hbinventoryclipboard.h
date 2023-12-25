/**
 * @file hbinventoryclipboard.h
 * @brief HBInventoryClipboard class declaration
 * This is a full rewrite/expansion of LL's original LLInventoryClipboard class
 *
 * $LicenseInfo:firstyear=2012&license=viewergpl$
 *
 * Copyright (c) 2012-2023, Henri Beauchamp.
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

#ifndef LL_HBINVENTORYCLIPBOARD_H
#define LL_HBINVENTORYCLIPBOARD_H

#include "hbfastmap.h"
#include "llinventorytype.h"
#include "lluuid.h"

class LLInventoryItem;

// Purely static class
class HBInventoryClipboard
{
public:
	HBInventoryClipboard() = delete;
	~HBInventoryClipboard() = delete;

	///////////////////////////////////////////////////////////////////////////
	// Inventory objects management

	// Empties out the objects clipboard
	LL_INLINE static void reset()
	{
		sObjects.clear();
		sCutObjects.clear();
	}

	// Adds to the current list.
	LL_INLINE static void add(const LLUUID& object_id)	
	{
		sObjects.emplace_back(object_id);
	}

	// Add for Cut operation
	LL_INLINE static void addCut(const LLUUID& object_id)
	{
		sCutObjects.emplace_back(object_id);
	}

	// Stores a single inventory object
	LL_INLINE static void store(const LLUUID& object_id)
	{
		reset();
		add(object_id);
	}

	// Stores an array of objects
	static void store(const uuid_vec_t& inventory_objects);

	// Gets the objects in the clipboard by copying them into the vector.
	static void retrieve(uuid_vec_t& inventory_objects);

	// Gets the objects in the clipboard by copying them into the vector.
	static void retrieveCuts(uuid_vec_t& inventory_objects);

	// These methods return true when object_id is in the corresponding
	// clipboard
	static bool isCopied(const LLUUID& object_id);
	static bool isCut(const LLUUID& object_id);

	// The following three methods return true if the clipboard contains
	// something that can be pasted.
	LL_INLINE static bool hasCopiedContents()	{ return !sObjects.empty(); }

	LL_INLINE static bool hasCutContents()
	{
		return !sCutObjects.empty();
	}

	LL_INLINE static bool hasContents()
	{
		return !sObjects.empty() || !sCutObjects.empty();
	}

	///////////////////////////////////////////////////////////////////////////
	// Inventory assets management

	// Empties out the assets clipboard
	LL_INLINE static void resetAssets()			{ sAssets.clear(); }

	// Adds to the current list of assets. Also copies the asset Id to the text
	// clipboard unless false is passed for 'copy_id_to_text_clipboard'.
	// Note: if the asset Id is null, it is not stored/copied.
	static void addAsset(const LLUUID& asset_id, LLInventoryType::EType type,
						 bool copy_id_to_text_clipboard = true);

	// Stores a single asset Id. Also copies the asset Id to the text clipboard
	// unless false is passed for 'copy_id_to_text_clipboard'.
	// Note: if the asset Id is null, it is not stored/copied.
	static void storeAsset(const LLUUID& asset_id, LLInventoryType::EType type,
						   bool copy_id_to_text_clipboard = true);

	// Stores the asset Id associated with the passed inventory item. Also
	// copies the asset Id to the text clipboard unless false is passed for
	// 'copy_id_to_text_clipboard'.
	// Note: if the asset Id is null, it is not stored/copied.
	static void storeAsset(const LLInventoryItem* itemp,
						   bool copy_id_to_text_clipboard = true);

	// Gets the assets of the specified inventory type stored in the clipboard
	// by copying their UUID into the vector.
	static void retrieveAssets(uuid_vec_t& inventory_assets,
							   LLInventoryType::EType type);

	// Returns true if assets of the specified inventory type are stored.
	static bool hasAssets(LLInventoryType::EType type);

private:
	static uuid_vec_t	sObjects;
	static uuid_vec_t	sCutObjects;

	typedef fast_hmap<LLUUID, LLInventoryType::EType> assets_map_t;
	static assets_map_t	sAssets;
};

#endif // LL_HBINVENTORYCLIPBOARD_H
