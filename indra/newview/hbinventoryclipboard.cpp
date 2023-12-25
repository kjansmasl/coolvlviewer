/**
 * @file hbinventoryclipboard.cpp
 * @brief HBInventoryClipboard class implementation
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

#include "llviewerprecompiledheaders.h"

#include "hbinventoryclipboard.h"

#include "llinventorymodel.h"
#include "llwindow.h"						// For gWindowp

uuid_vec_t HBInventoryClipboard::sObjects;
uuid_vec_t HBInventoryClipboard::sCutObjects;
HBInventoryClipboard::assets_map_t HBInventoryClipboard::sAssets;

///////////////////////////////////////////////////////////////////////////////
// Inventory objects management

//static
void HBInventoryClipboard::store(const uuid_vec_t& inv_objects)
{
	reset();
	for (S32 i = 0, count = inv_objects.size(); i < count; ++i)
	{
		sObjects.emplace_back(inv_objects[i]);
	}
}

//static
void HBInventoryClipboard::retrieve(uuid_vec_t& inv_objects)
{
	inv_objects.clear();
	LLUUID object_id;
	for (S32 i = 0, count = sObjects.size(); i < count; ++i)
	{
		object_id = sObjects[i];
		// Only add objects that have not since been purged...
		if (gInventory.getItem(object_id) ||
			gInventory.getCategory(object_id))
		{
			inv_objects.emplace_back(object_id);
		}
	}
}

//static
void HBInventoryClipboard::retrieveCuts(uuid_vec_t& inv_objects)
{
	inv_objects.clear();
	LLUUID object_id;
	for (S32 i = 0, count = sCutObjects.size(); i < count; ++i)
	{
		object_id = sCutObjects[i];
		// Only add objects that have not since been purged...
		if (gInventory.getItem(object_id) ||
			gInventory.getCategory(object_id))
		{
			inv_objects.emplace_back(object_id);
		}
	}
}

//static
bool HBInventoryClipboard::isCopied(const LLUUID& object_id)
{
	uuid_vec_t::iterator end = sObjects.end();
	return std::find(sObjects.begin(), end, object_id) != end;
}

//static
bool HBInventoryClipboard::isCut(const LLUUID& object_id)
{
	uuid_vec_t::iterator end = sCutObjects.end();
	return std::find(sCutObjects.begin(), end, object_id) != end;
}

///////////////////////////////////////////////////////////////////////////////
// Inventory assets management

// Helper function.
static void copy_to_text_clipboard(const LLUUID& asset_id)
{
	if (!gWindowp) return;	// Paranoia
	gWindowp->copyTextToClipboard(utf8str_to_wstring(asset_id.asString()));
}

//static
void HBInventoryClipboard::addAsset(const LLUUID& asset_id,
									LLInventoryType::EType type,
									bool copy_id_to_text_clipboard)
{
	// Snapshot and textures are share same type of asset...
	if (type == LLInventoryType::IT_SNAPSHOT)
	{
		type = LLInventoryType::IT_TEXTURE;
	}
	if (asset_id.notNull())
	{
		sAssets.emplace(asset_id, type);
		if (copy_id_to_text_clipboard)
		{
			copy_to_text_clipboard(asset_id);
		}
	}
}

//static
void HBInventoryClipboard::storeAsset(const LLUUID& asset_id,
									  LLInventoryType::EType type,
									  bool copy_id_to_text_clipboard)
{
	resetAssets();
	// Snapshot and textures are share same type of asset...
	if (type == LLInventoryType::IT_SNAPSHOT)
	{
		type = LLInventoryType::IT_TEXTURE;
	}
	if (asset_id.notNull())
	{
		sAssets.emplace(asset_id, type);
		if (copy_id_to_text_clipboard)
		{
			copy_to_text_clipboard(asset_id);
		}
	}
}

//static
void HBInventoryClipboard::storeAsset(const LLInventoryItem* itemp,
									  bool copy_id_to_text_clipboard)
{
	if (itemp)
	{
		resetAssets();
		LLInventoryType::EType type = itemp->getInventoryType();
		// Snapshot and textures are share same type of asset...
		if (type == LLInventoryType::IT_SNAPSHOT)
		{
			type = LLInventoryType::IT_TEXTURE;
		}
		const LLUUID& asset_id = itemp->getAssetUUID();
		if (asset_id.notNull())
		{
			sAssets.emplace(asset_id, type);
			if (copy_id_to_text_clipboard)
			{
				copy_to_text_clipboard(asset_id);
			}
		}
	}
}

//static
void HBInventoryClipboard::retrieveAssets(uuid_vec_t& inv_assets,
										  LLInventoryType::EType type)
{
	// Snapshot and textures are share same type of asset...
	if (type == LLInventoryType::IT_SNAPSHOT)
	{
		type = LLInventoryType::IT_TEXTURE;
	}
	inv_assets.clear();
	for (assets_map_t::const_iterator it = sAssets.begin(),
									  end = sAssets.end();
		 it != end; ++it)
	{
		if (it->second == type)
		{
			inv_assets.emplace_back(it->first);
		}
	}
}

//static
bool HBInventoryClipboard::hasAssets(LLInventoryType::EType type)
{
	// Snapshot and textures are share same type of asset...
	if (type == LLInventoryType::IT_SNAPSHOT)
	{
		type = LLInventoryType::IT_TEXTURE;
	}
	for (assets_map_t::const_iterator it = sAssets.begin(),
									  end = sAssets.end();
		 it != end; ++it)
	{
		if (it->second == type)
		{
			return true;
		}
	}
	return false;
}
