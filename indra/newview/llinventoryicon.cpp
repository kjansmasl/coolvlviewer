/**
 * @file llinventoryicon.cpp
 * @brief Implementation of the inventory icon.
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

#include "llviewerprecompiledheaders.h"

#include "llinventoryicon.h"

#include "lldictionary.h"
#include "llinventory.h"
#include "llui.h"
#include "llsettingstype.h"
#include "llwearabletype.h"

struct IconEntry : public LLDictionaryEntry
{
	IconEntry(const std::string& item_name, const LLUIImagePtr icon)
	:	LLDictionaryEntry(item_name),
		mIcon(icon)
	{
	}

	const LLUIImagePtr mIcon;
};

class LLIconDictionary : public LLSingleton<LLIconDictionary>,
						 public LLDictionary<LLInventoryType::EIconName, IconEntry>
{
	friend class LLSingleton<LLIconDictionary>;

public:
	LLIconDictionary();
};

LLIconDictionary::LLIconDictionary()
{
	static const LLUIImagePtr texture_icon = LLUI::getUIImage("inv_item_texture.tga");
	static const LLUIImagePtr sound_icon = LLUI::getUIImage("inv_item_sound.tga");
	static const LLUIImagePtr card_online_icon = LLUI::getUIImage("inv_item_callingcard_online.tga");
	static const LLUIImagePtr card_offline_icon = LLUI::getUIImage("inv_item_callingcard_offline.tga");
	static const LLUIImagePtr landmark_icon = LLUI::getUIImage("inv_item_landmark.tga");
	static const LLUIImagePtr landmark_visited_icon = LLUI::getUIImage("inv_item_landmark_visited.tga");
	static const LLUIImagePtr script_icon = LLUI::getUIImage("inv_item_script.tga");
	static const LLUIImagePtr clothing_icon = LLUI::getUIImage("inv_item_clothing.tga");
	static const LLUIImagePtr object_icon = LLUI::getUIImage("inv_item_object.tga");
	static const LLUIImagePtr object_multi_icon = LLUI::getUIImage("inv_item_object_multi.tga");
	static const LLUIImagePtr notecard_icon = LLUI::getUIImage("inv_item_notecard.tga");
	static const LLUIImagePtr snapshot_icon = LLUI::getUIImage("inv_item_snapshot.tga");

	static const LLUIImagePtr shape_icon = LLUI::getUIImage("inv_item_shape.tga");
	static const LLUIImagePtr skin_icon = LLUI::getUIImage("inv_item_skin.tga");
	static const LLUIImagePtr hair_icon = LLUI::getUIImage("inv_item_hair.tga");
	static const LLUIImagePtr eyes_icon = LLUI::getUIImage("inv_item_eyes.tga");

	static const LLUIImagePtr shirt_icon = LLUI::getUIImage("inv_item_shirt.tga");
	static const LLUIImagePtr pants_icon = LLUI::getUIImage("inv_item_pants.tga");
	static const LLUIImagePtr shoes_icon = LLUI::getUIImage("inv_item_shoes.tga");
	static const LLUIImagePtr socks_icon = LLUI::getUIImage("inv_item_socks.tga");
	static const LLUIImagePtr jacket_icon = LLUI::getUIImage("inv_item_jacket.tga");
	static const LLUIImagePtr gloves_icon = LLUI::getUIImage("inv_item_gloves.tga");
	static const LLUIImagePtr undershirt_icon = LLUI::getUIImage("inv_item_undershirt.tga");
	static const LLUIImagePtr underpants_icon = LLUI::getUIImage("inv_item_underpants.tga");
	static const LLUIImagePtr skirt_icon = LLUI::getUIImage("inv_item_skirt.tga");
	static const LLUIImagePtr alpha_icon = LLUI::getUIImage("inv_item_alpha.tga");
	static const LLUIImagePtr tattoo_icon = LLUI::getUIImage("inv_item_tattoo.tga");
	static const LLUIImagePtr universal_icon = LLUI::getUIImage("inv_item_universal.tga");
	static const LLUIImagePtr physics_icon = LLUI::getUIImage("inv_item_physics.tga");

	static const LLUIImagePtr animation_icon = LLUI::getUIImage("inv_item_animation.tga");
	static const LLUIImagePtr gesture_icon = LLUI::getUIImage("inv_item_gesture.tga");

	static const LLUIImagePtr link_item_icon = LLUI::getUIImage("inv_link_item.tga");
	static const LLUIImagePtr link_folder_icon = LLUI::getUIImage("inv_link_folder.tga");

#if LL_MESH_ASSET_SUPPORT
	static const LLUIImagePtr mesh_icon = LLUI::getUIImage("inv_item_mesh.tga");
#endif

	static const LLUIImagePtr settings_icon = LLUI::getUIImage("inv_item_settings.tga");
	static const LLUIImagePtr sky_icon = LLUI::getUIImage("inv_item_sky.tga");
	static const LLUIImagePtr water_icon = LLUI::getUIImage("inv_item_water.tga");
	static const LLUIImagePtr daycycle_icon = LLUI::getUIImage("inv_item_daycycle.tga");

	static const LLUIImagePtr material_icon = LLUI::getUIImage("inv_item_material.tga");

	static const LLUIImagePtr invalid_icon = LLUI::getUIImage("inv_item_invalid.tga");

	addEntry(LLInventoryType::ICONNAME_TEXTURE,
			 new IconEntry("inv_item_texture.tga", texture_icon));
	addEntry(LLInventoryType::ICONNAME_SOUND,
			 new IconEntry("inv_item_sound.tga", sound_icon));
	addEntry(LLInventoryType::ICONNAME_CALLINGCARD_ONLINE,
			 new IconEntry("inv_item_callingcard_online.tga", card_online_icon));
	addEntry(LLInventoryType::ICONNAME_CALLINGCARD_OFFLINE,
			 new IconEntry("inv_item_callingcard_offline.tga", card_offline_icon));
	addEntry(LLInventoryType::ICONNAME_LANDMARK,
			 new IconEntry("inv_item_landmark.tga", landmark_icon));
	addEntry(LLInventoryType::ICONNAME_LANDMARK_VISITED,
			 new IconEntry("inv_item_landmark_visited.tga", landmark_visited_icon));
	addEntry(LLInventoryType::ICONNAME_SCRIPT,
			 new IconEntry("inv_item_script.tga", script_icon));
	addEntry(LLInventoryType::ICONNAME_CLOTHING,
			 new IconEntry("inv_item_clothing.tga", clothing_icon));
	addEntry(LLInventoryType::ICONNAME_OBJECT,
			 new IconEntry("inv_item_object.tga", object_icon));
	addEntry(LLInventoryType::ICONNAME_OBJECT_MULTI,
			 new IconEntry("inv_item_object_multi.tga", object_multi_icon));
	addEntry(LLInventoryType::ICONNAME_NOTECARD,
			 new IconEntry("inv_item_notecard.tga", notecard_icon));
	addEntry(LLInventoryType::ICONNAME_BODYPART,
			 new IconEntry("inv_item_skin.tga", skin_icon));
	addEntry(LLInventoryType::ICONNAME_SNAPSHOT,
			 new IconEntry("inv_item_snapshot.tga", snapshot_icon));

	addEntry(LLInventoryType::ICONNAME_BODYPART_SHAPE,
			 new IconEntry("inv_item_shape.tga", shape_icon));
	addEntry(LLInventoryType::ICONNAME_BODYPART_SKIN,
 			 new IconEntry("inv_item_skin.tga", skin_icon));
	addEntry(LLInventoryType::ICONNAME_BODYPART_HAIR,
			 new IconEntry("inv_item_hair.tga", hair_icon));
	addEntry(LLInventoryType::ICONNAME_BODYPART_EYES,
			 new IconEntry("inv_item_eyes.tga", eyes_icon));

	addEntry(LLInventoryType::ICONNAME_CLOTHING_SHIRT,
			 new IconEntry("inv_item_shirt.tga", shirt_icon));
	addEntry(LLInventoryType::ICONNAME_CLOTHING_PANTS,
			 new IconEntry("inv_item_pants.tga", pants_icon));
	addEntry(LLInventoryType::ICONNAME_CLOTHING_SHOES,
			 new IconEntry("inv_item_shoes.tga", shoes_icon));
	addEntry(LLInventoryType::ICONNAME_CLOTHING_SOCKS,
			 new IconEntry("inv_item_socks.tga", socks_icon));
	addEntry(LLInventoryType::ICONNAME_CLOTHING_JACKET,
			 new IconEntry("inv_item_jacket.tga", jacket_icon));
	addEntry(LLInventoryType::ICONNAME_CLOTHING_GLOVES,
			 new IconEntry("inv_item_gloves.tga", gloves_icon));
	addEntry(LLInventoryType::ICONNAME_CLOTHING_UNDERSHIRT,
		 	 new IconEntry("inv_item_undershirt.tga", undershirt_icon));
	addEntry(LLInventoryType::ICONNAME_CLOTHING_UNDERPANTS,
		 	 new IconEntry("inv_item_underpants.tga", underpants_icon));
	addEntry(LLInventoryType::ICONNAME_CLOTHING_SKIRT,
			 new IconEntry("inv_item_skirt.tga", skirt_icon));
	addEntry(LLInventoryType::ICONNAME_CLOTHING_ALPHA,
			 new IconEntry("inv_item_alpha.tga", alpha_icon));
	addEntry(LLInventoryType::ICONNAME_CLOTHING_TATTOO,
			 new IconEntry("inv_item_tattoo.tga", tattoo_icon));
	addEntry(LLInventoryType::ICONNAME_CLOTHING_UNIVERSAL,
			 new IconEntry("inv_item_universal.tga", universal_icon));
	addEntry(LLInventoryType::ICONNAME_CLOTHING_PHYSICS,
			 new IconEntry("inv_item_physics.tga", physics_icon));

	addEntry(LLInventoryType::ICONNAME_ANIMATION,
			 new IconEntry("inv_item_animation.tga", animation_icon));
	addEntry(LLInventoryType::ICONNAME_GESTURE,
			 new IconEntry("inv_item_gesture.tga", gesture_icon));

	addEntry(LLInventoryType::ICONNAME_LINKITEM,
			 new IconEntry("inv_link_item.tga", link_item_icon));
	addEntry(LLInventoryType::ICONNAME_LINKFOLDER,
			 new IconEntry("inv_link_folder.tga", link_folder_icon));

#if LL_MESH_ASSET_SUPPORT
	addEntry(LLInventoryType::ICONNAME_MESH,
			 new IconEntry("inv_item_mesh.tga", mesh_icon));
#endif

	addEntry(LLInventoryType::ICONNAME_SETTINGS,
			 new IconEntry("inv_item_settings.tga", settings_icon));
	addEntry(LLInventoryType::ICONNAME_SETTINGS_SKY,
			 new IconEntry("inv_item_sky.tga", sky_icon));
	addEntry(LLInventoryType::ICONNAME_SETTINGS_WATER,
			 new IconEntry("inv_item_water.tga", water_icon));
	addEntry(LLInventoryType::ICONNAME_SETTINGS_DAY,
			 new IconEntry("inv_item_daycycle.tga", daycycle_icon));

	addEntry(LLInventoryType::ICONNAME_MATERIAL,
			 new IconEntry("inv_item_material.tga", material_icon));

	addEntry(LLInventoryType::ICONNAME_INVALID,
			 new IconEntry("inv_item_invalid.tga", invalid_icon));

	addEntry(LLInventoryType::ICONNAME_NONE,
			 new IconEntry("NONE", NULL));
}

const LLInventoryType::EIconName LLInventoryIcon::getIconIdx(LLAssetType::EType asset_type,
															 LLInventoryType::EType inventory_type,
															 U32 misc_flag,
															 bool item_is_multi)
{
	if (item_is_multi)
	{
		return LLInventoryType::ICONNAME_OBJECT_MULTI;
	}

	switch (asset_type)
	{
		case LLAssetType::AT_TEXTURE:
			return inventory_type == LLInventoryType::IT_SNAPSHOT ?
						LLInventoryType::ICONNAME_SNAPSHOT :
						LLInventoryType::ICONNAME_TEXTURE;

		case LLAssetType::AT_SOUND:
			return LLInventoryType::ICONNAME_SOUND;

		case LLAssetType::AT_CALLINGCARD:
			return misc_flag ? LLInventoryType::ICONNAME_CALLINGCARD_ONLINE
							: LLInventoryType::ICONNAME_CALLINGCARD_OFFLINE;

		case LLAssetType::AT_LANDMARK:
			return misc_flag ? LLInventoryType::ICONNAME_LANDMARK_VISITED
							 : LLInventoryType::ICONNAME_LANDMARK;

		case LLAssetType::AT_SCRIPT:
		case LLAssetType::AT_LSL_TEXT:
		case LLAssetType::AT_LSL_BYTECODE:
			return LLInventoryType::ICONNAME_SCRIPT;

		case LLAssetType::AT_CLOTHING:
		case LLAssetType::AT_BODYPART:
			return assignWearableIcon(misc_flag);

		case LLAssetType::AT_NOTECARD:
			return LLInventoryType::ICONNAME_NOTECARD;

		case LLAssetType::AT_ANIMATION:
			return LLInventoryType::ICONNAME_ANIMATION;

		case LLAssetType::AT_GESTURE:
			return LLInventoryType::ICONNAME_GESTURE;

		case LLAssetType::AT_LINK:
			return LLInventoryType::ICONNAME_LINKITEM;

		case LLAssetType::AT_LINK_FOLDER:
			return LLInventoryType::ICONNAME_LINKFOLDER;

		case LLAssetType::AT_OBJECT:
			return LLInventoryType::ICONNAME_OBJECT;

#if LL_MESH_ASSET_SUPPORT
		case LLAssetType::AT_MESH:
			return LLInventoryType::ICONNAME_MESH;
#endif

		case LLAssetType::AT_SETTINGS:
			return assignSettingsIcon(misc_flag);

		case LLAssetType::AT_MATERIAL:
			return LLInventoryType::ICONNAME_MATERIAL;

		default:
			break;
	}

	return LLInventoryType::ICONNAME_INVALID;
}

const LLUIImagePtr LLInventoryIcon::getIcon(LLAssetType::EType asset_type,
											LLInventoryType::EType inventory_type,
											U32 misc_flag,
											bool item_is_multi)
{
	const LLInventoryType::EIconName idx = getIconIdx(asset_type,
													  inventory_type, misc_flag,
													  item_is_multi);
	const IconEntry* entry = LLIconDictionary::getInstance()->lookup(idx);
	return entry->mIcon;
}

const LLUIImagePtr LLInventoryIcon::getIcon(LLInventoryType::EIconName idx)
{
	const IconEntry* entry = LLIconDictionary::getInstance()->lookup(idx);
	return entry->mIcon;
}

const std::string& LLInventoryIcon::getIconName(LLAssetType::EType asset_type,
												LLInventoryType::EType inventory_type,
												U32 misc_flag,
												bool item_is_multi)
{
	const LLInventoryType::EIconName idx = getIconIdx(asset_type,
													  inventory_type, misc_flag,
							 						  item_is_multi);
	return getIconName(idx);
}

const std::string& LLInventoryIcon::getIconName(LLInventoryType::EIconName idx)
{
	const IconEntry* entry = LLIconDictionary::getInstance()->lookup(idx);
	return entry->mName;
}

LLInventoryType::EIconName LLInventoryIcon::assignWearableIcon(U32 misc_flag)
{
	const LLWearableType::EType wearable_type = LLWearableType::inventoryFlagsToWearableType(misc_flag);
	return LLWearableType::getIconName(wearable_type);
}

LLInventoryType::EIconName LLInventoryIcon::assignSettingsIcon(U32 misc_flag)
{
	const LLSettingsType::EType settings_type = LLSettingsType::fromInventoryFlags(misc_flag);
	return LLSettingsType::getIconName(settings_type);
}
