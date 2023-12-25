/**
 * @file llviewerfoldertype.cpp
 * @brief Implementation of LLViewerFolderType functionality.
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

#include "llviewerfoldertype.h"

#include "lldictionary.h"

static const std::string empty_string;

struct ViewerFolderEntry : public LLDictionaryEntry
{
	// Constructor for non-ensembles
	ViewerFolderEntry(const std::string& category_name,
					  const std::string& icon_name,
					  const LLUIImagePtr icon, bool is_quiet,
					  bool hide_if_empty = false,
					  // No reverse lookup needed, so in most cases just
					  // leave this blank:
					  const std::string& dictionary_name = empty_string)
	:	LLDictionaryEntry(dictionary_name),
		mNewCategoryName(category_name),
		mIconName(icon_name),
		mIcon(icon),
		mIsQuiet(is_quiet),
		mHideIfEmpty(hide_if_empty)
	{
	}

	const std::string	mIconName;		// Name of the folder icon
	const LLUIImagePtr	mIcon;			// Pointer to the icon itself
	// Default name when creating a new folder of this type
	const std::string	mNewCategoryName;
	// When true, folder does not need an UI update when changed
	bool				mIsQuiet;
	bool				mHideIfEmpty;	// Folder not shown if empty when true
};

class LLViewerFolderDictionary final : public LLSingleton<LLViewerFolderDictionary>,
									   public LLDictionary<LLFolderType::EType,
														   ViewerFolderEntry>
{
	friend class LLSingleton<LLViewerFolderDictionary>;

public:
	LLViewerFolderDictionary();
};

LLViewerFolderDictionary::LLViewerFolderDictionary()
{
	static const LLUIImagePtr plain_icon = LLUI::getUIImage("inv_folder_plain_closed.tga");
	static const LLUIImagePtr texture_icon = LLUI::getUIImage("inv_folder_texture.tga");
	static const LLUIImagePtr sound_icon = LLUI::getUIImage("inv_folder_sound.tga");
	static const LLUIImagePtr callingcard_icon = LLUI::getUIImage("inv_folder_callingcard.tga");
	static const LLUIImagePtr landmark_icon = LLUI::getUIImage("inv_folder_landmark.tga");
	static const LLUIImagePtr clothing_icon = LLUI::getUIImage("inv_folder_clothing.tga");
	static const LLUIImagePtr object_icon = LLUI::getUIImage("inv_folder_object.tga");
	static const LLUIImagePtr notecard_icon = LLUI::getUIImage("inv_folder_notecard.tga");
	static const LLUIImagePtr script_icon = LLUI::getUIImage("inv_folder_script.tga");
	static const LLUIImagePtr bodypart_icon = LLUI::getUIImage("inv_folder_bodypart.tga");
	static const LLUIImagePtr trash_icon = LLUI::getUIImage("inv_folder_trash.tga");
	static const LLUIImagePtr snapshot_icon = LLUI::getUIImage("inv_folder_snapshot.tga");
	static const LLUIImagePtr lostandfound_icon = LLUI::getUIImage("inv_folder_lostandfound.tga");
	static const LLUIImagePtr animation_icon = LLUI::getUIImage("inv_folder_animation.tga");
	static const LLUIImagePtr gesture_icon = LLUI::getUIImage("inv_folder_gesture.tga");
	static const LLUIImagePtr settings_icon = LLUI::getUIImage("inv_folder_settings.tga");
	static const LLUIImagePtr materials_icon = LLUI::getUIImage("inv_folder_materials.tga");
	static const LLUIImagePtr inbox_icon = LLUI::getUIImage("inv_folder_inbox.tga");
	static const LLUIImagePtr outbox_icon = LLUI::getUIImage("inv_folder_outbox.tga");
	static const LLUIImagePtr stock_icon = LLUI::getUIImage("inv_folder_stock.tga");
	static const LLUIImagePtr version_icon = LLUI::getUIImage("inv_folder_version.tga");

	//       													    	  NEW CATEGORY NAME			FOLDER ICON NAME                FOLDER ICON POINTER QUIET   HIDE IF EMPTY
	//      												  		     |-------------------------|-------------------------------|-------------------|-------|-------------|
	addEntry(LLFolderType::FT_TEXTURE,				new ViewerFolderEntry("Textures",				"inv_folder_texture.tga",		texture_icon,		false,	true));
	addEntry(LLFolderType::FT_SOUND,				new ViewerFolderEntry("Sounds",					"inv_folder_sound.tga",			sound_icon,			false,	true));
	addEntry(LLFolderType::FT_CALLINGCARD,			new ViewerFolderEntry("Calling Cards",			"inv_folder_callingcard.tga",	callingcard_icon,	true,	true));
	addEntry(LLFolderType::FT_LANDMARK,				new ViewerFolderEntry("Landmarks",				"inv_folder_landmark.tga",		landmark_icon,		false,	true));
	addEntry(LLFolderType::FT_CLOTHING,				new ViewerFolderEntry("Clothing",				"inv_folder_clothing.tga",		clothing_icon,		false,	true));
	addEntry(LLFolderType::FT_OBJECT,				new ViewerFolderEntry("Objects",				"inv_folder_object.tga",		object_icon,		false,	true));
	addEntry(LLFolderType::FT_NOTECARD,				new ViewerFolderEntry("Notecards",				"inv_folder_notecard.tga",		notecard_icon,		false,	true));
	addEntry(LLFolderType::FT_ROOT_INVENTORY,		new ViewerFolderEntry("My Inventory",			"inv_folder_plain_closed.tga",	plain_icon,			false,	false));
	addEntry(LLFolderType::FT_ROOT_INVENTORY_OS,	new ViewerFolderEntry("My Inventory",			"inv_folder_plain_closed.tga",	plain_icon,			false,	false));
	addEntry(LLFolderType::FT_LSL_TEXT,				new ViewerFolderEntry("Scripts",				"inv_folder_script.tga",		script_icon,		false,	true));
	addEntry(LLFolderType::FT_BODYPART,				new ViewerFolderEntry("Body Parts",				"inv_folder_bodypart.tga",		bodypart_icon,		false,	true));
	addEntry(LLFolderType::FT_TRASH,				new ViewerFolderEntry("Trash",					"inv_folder_trash.tga",			trash_icon,			true,	false));
	addEntry(LLFolderType::FT_SNAPSHOT_CATEGORY,	new ViewerFolderEntry("Photo Album",			"inv_folder_snapshot.tga",		snapshot_icon,		false,	true));
	addEntry(LLFolderType::FT_LOST_AND_FOUND,		new ViewerFolderEntry("Lost And Found",			"inv_folder_lostandfound.tga",	lostandfound_icon,	true,	true));
	addEntry(LLFolderType::FT_ANIMATION,			new ViewerFolderEntry("Animations",				"inv_folder_animation.tga",		animation_icon,		false,	true));
	addEntry(LLFolderType::FT_GESTURE,				new ViewerFolderEntry("Gestures",				"inv_folder_gesture.tga",		gesture_icon,		false,	true));
	addEntry(LLFolderType::FT_MESH,					new ViewerFolderEntry("Meshes",					"inv_folder_plain_closed.tga",	plain_icon,			false,	true));
	addEntry(LLFolderType::FT_CURRENT_OUTFIT,		new ViewerFolderEntry("Current Outfit",			"inv_folder_plain_closed.tga",	plain_icon,			true,	true));
	addEntry(LLFolderType::FT_INBOX,				new ViewerFolderEntry("Received Items",			"inv_folder_inbox.tga",			inbox_icon,			false,	true));
	addEntry(LLFolderType::FT_SETTINGS,				new ViewerFolderEntry("Settings",				"inv_folder_settings.tga",		settings_icon,		false,	true));
	addEntry(LLFolderType::FT_MATERIAL,				new ViewerFolderEntry("Materials",				"inv_folder_materials.tga",		materials_icon,		false,	true));
	addEntry(LLFolderType::FT_SUITCASE,				new ViewerFolderEntry("My Suitcase",			"inv_folder_plain_closed.tga",	plain_icon,			false,	true));

	addEntry(LLFolderType::FT_MARKETPLACE_LISTINGS,	new ViewerFolderEntry("Marketplace Listings",	"inv_folder_outbox.tga",		outbox_icon,		false,	true));
	addEntry(LLFolderType::FT_MARKETPLACE_STOCK,	new ViewerFolderEntry("New Stock",				"inv_folder_stock.tga",			stock_icon,			false,	false));
	addEntry(LLFolderType::FT_MARKETPLACE_VERSION,	new ViewerFolderEntry("New Version",			"inv_folder_version.tga",		version_icon,		false,	false));

	addEntry(LLFolderType::FT_NONE,					new ViewerFolderEntry("New Folder",				"inv_folder_plain_closed.tga",	plain_icon,			false,	false,	"default"));

#if 0	// Viewer 2 folders: useless for v1 !
	addEntry(LLFolderType::FT_BASIC_ROOT,			new ViewerFolderEntry("Basic Root",				"inv_folder_plain_closed.tga",	plain_icon,			true,	false));
	addEntry(LLFolderType::FT_FAVORITE,				new ViewerFolderEntry("Favorites",				"inv_folder_plain_closed.tga",	plain_icon,			true,	false));
	addEntry(LLFolderType::FT_OUTFIT,				new ViewerFolderEntry("New Outfit",				"inv_folder_plain_closed.tga",	plain_icon,			true,	false));
	addEntry(LLFolderType::FT_MY_OUTFITS,			new ViewerFolderEntry("My Outfits",				"inv_folder_plain_closed.tga",	plain_icon,			true,	false));
#endif
}

//static
const std::string& LLViewerFolderType::lookupXUIName(LLFolderType::EType type)
{
	const ViewerFolderEntry* entry =
		LLViewerFolderDictionary::getInstance()->lookup(type);
	if (entry)
	{
		return entry->mName;
	}
	return badLookup();
}

//static
LLFolderType::EType LLViewerFolderType::lookupTypeFromXUIName(const std::string& name)
{
	return LLViewerFolderDictionary::getInstance()->lookup(name);
}

//static
const std::string& LLViewerFolderType::lookupIconName(LLFolderType::EType type)
{
	const ViewerFolderEntry* entry =
		LLViewerFolderDictionary::getInstance()->lookup(type);
	if (entry)
	{
		return entry->mIconName;
	}

	// Error condition. Return something so that we do not show a grey box in
	// inventory floater.
	const ViewerFolderEntry* default_entry =
		LLViewerFolderDictionary::getInstance()->lookup(LLFolderType::FT_NONE);
	if (!default_entry)
	{
		llerrs << "Missing FT_NONE entry in LLViewerFolderDictionary !"
			   << llendl;
	}
	return default_entry->mIconName;
}

//static
const LLUIImagePtr LLViewerFolderType::lookupIcon(LLFolderType::EType type)
{
	const ViewerFolderEntry* entry =
		LLViewerFolderDictionary::getInstance()->lookup(type);
	if (entry)
	{
		return entry->mIcon;
	}

	const ViewerFolderEntry* default_entry =
		LLViewerFolderDictionary::getInstance()->lookup(LLFolderType::FT_NONE);
	if (!default_entry)
	{
		llerrs << "Missing FT_NONE entry in LLViewerFolderDictionary !"
			   << llendl;
	}
	return default_entry->mIcon;
}

//static
bool LLViewerFolderType::lookupIsQuietType(LLFolderType::EType type)
{
	const ViewerFolderEntry* entry =
		LLViewerFolderDictionary::getInstance()->lookup(type);
	if (entry)
	{
		return entry->mIsQuiet;
	}
	return false;
}

//static
bool LLViewerFolderType::lookupIsHiddenIfEmpty(LLFolderType::EType type)
{
	const ViewerFolderEntry* entry =
		LLViewerFolderDictionary::getInstance()->lookup(type);
	if (entry)
	{
		return entry->mHideIfEmpty;
	}
	return false;
}

//static
const std::string& LLViewerFolderType::lookupNewCategoryName(LLFolderType::EType type)
{
	const ViewerFolderEntry* entry =
		LLViewerFolderDictionary::getInstance()->lookup(type);
	if (entry)
	{
		return entry->mNewCategoryName;
	}
	return badLookup();
}

#if 0	// Not used
//static
LLFolderType::EType LLViewerFolderType::lookupTypeFromNewCatName(const std::string& name)
{
	LLViewerFolderDictionary* dictp = LLViewerFolderDictionary::getInstance();
	for (LLViewerFolderDictionary::const_iterator iter = dictp->begin(),
												  end = dictp->end();
		 iter != end; ++iter)
	{
		const ViewerFolderEntry* entry = iter->second;
		if (entry->mNewCategoryName == name)
		{
			return iter->first;
		}
	}
	return FT_NONE;
}
#endif
