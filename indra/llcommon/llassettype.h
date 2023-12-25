/**
 * @file llassettype.h
 * @brief Declaration of LLAssetType.
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 *
 * Copyright (c) 2001-2009, Linden Research, Inc.
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

#ifndef LL_LLASSETTYPE_H
#define LL_LLASSETTYPE_H

#include "llpreprocessor.h"

#include <string>

// Define to non-zero to support mesh *inventory* assets: this is useless,
// since no such asset was ever created in SL (only in Aditi, in very early
// days of the mesh viewer development, before it was finally decided that
// meshes would be linked to object inventory items instead). HB
#define LL_MESH_ASSET_SUPPORT 0

// This enum is also used by the UI code and in the viewer (newview) code
enum EDragAndDropType
{
	DAD_NONE = 0,
	DAD_TEXTURE,
	DAD_SOUND,
	DAD_CALLINGCARD,
	DAD_LANDMARK,
	DAD_SCRIPT,
	DAD_CLOTHING,
	DAD_OBJECT,
	DAD_NOTECARD,
	DAD_CATEGORY,
	DAD_ROOT_CATEGORY,
	DAD_BODYPART,
	DAD_ANIMATION,
	DAD_GESTURE,
	DAD_LINK,
#if LL_MESH_ASSET_SUPPORT
	DAD_MESH,
#endif
	DAD_SETTINGS,
	DAD_MATERIAL,
	DAD_COUNT		   // Number of types in this enum
};

// Purely static class
class LLAssetType
{
	LLAssetType() = delete;
	~LLAssetType() = delete;

public:
	enum EType
	{
		// Used for painting the faces of geometry. Stored in typical j2c stream
		// format.
		AT_TEXTURE = 0,

		// Used to fill the aural spectrum.
		AT_SOUND = 1,

	    // References instant message access to the user on the card.
		AT_CALLINGCARD = 2,

		// References to places in the world with location and a screen shot or
		// image saved.
		AT_LANDMARK = 3,

		// Old scripts that can be attached to an object (deprecated).
		AT_SCRIPT = 4,

		// A collection of textures and parameters that can be worn by an
		// avatar and represent a piece of clothing.
		AT_CLOTHING = 5,

		// Any combination of textures, sounds, and scripts that are associated
		// with a fixed piece of primitives-based geometry.
		AT_OBJECT = 6,

		// Just text.
		AT_NOTECARD = 7,

		// Folder holding a collection of inventory items. It is treated as an
		// item in the inventory and therefore needs a type.
		AT_CATEGORY = 8,

		// The LSL is the scripting language. We have split it into a text and
		// (deprecated) bytecode representation.
		AT_LSL_TEXT = 10,
		AT_LSL_BYTECODE = 11,

		// Uncompressed TGA texture.
		AT_TEXTURE_TGA = 12,

		// A collection of textures and mandatory parameters that can be worn
		// by an avatar and define its body.
		AT_BODYPART = 13,

		// Uncompressed sound.
		AT_SOUND_WAV = 17,

		// Uncompressed non-square image, not appropriate for use as a texture.
		AT_IMAGE_TGA = 18,

		// Compressed non-square image, not appropriate for use as a texture.
		AT_IMAGE_JPEG = 19,

		// Animation.
		AT_ANIMATION = 20,

		// Gesture, sequence of animations, sounds, chat, pauses.
		AT_GESTURE = 21,

		// Simstate file.
		AT_SIMSTATE = 22,

		// Inventory symbolic link.
		AT_LINK = 24,

		// Inventory folder link.
		AT_LINK_FOLDER = 25,

		// Marketplace folder. Same as an AT_CATEGORY but different display
		// methods.
		AT_MARKETPLACE_FOLDER = 26,

	    // Mesh data in our proprietary SLM format: only for possible use in
		// assets cache (currently assets types not used by it) via the mesh
		// repository. Not represented by an inventory type.
		AT_MESH = 49,

		AT_RESERVED_1 = 50,
		AT_RESERVED_2 = 51,
		AT_RESERVED_3 = 52,
		AT_RESERVED_4 = 53,
		AT_RESERVED_5 = 54,
		AT_RESERVED_6 = 55,

		// Collection of settings.
		AT_SETTINGS = 56,

		// Render material.
		AT_MATERIAL = 57,

		AT_COUNT,

			// +*********************************************************+
			// |  TO ADD AN ELEMENT TO THIS ENUM:                        |
			// +*********************************************************+
			// | 1. INSERT BEFORE AT_COUNT                               |
			// | 2. INCREMENT AT_COUNT BY 1                              |
			// | 3. ADD TO llassettype.cpp                               |
			// | 4. ADD TO llinventorytype.h and llinventorytype.cpp     |
			// | 5. ADD ICON TO llinventoryicon.h and llinventoryicon.cpp|
			// +*********************************************************+

		AT_NONE = -1
	};

	///////////////////////////////////////////////////////////////////////////
	// Machine translation between type and strings

	// Safe conversion to std::string, *TODO: deprecate
	static EType lookup(const char* name);

	static EType lookup(const std::string& type_name);
	static const char* lookup(EType asset_type);
	///////////////////////////////////////////////////////////////////////////

	///////////////////////////////////////////////////////////////////////////
	// Translations from a type to a human readable form.

	// Safe conversion to std::string, *TODO: deprecate
	static EType lookupHumanReadable(const char* desc_name);

	static EType lookupHumanReadable(const std::string& readable_name);
	static const char* lookupHumanReadable(EType asset_type);
	///////////////////////////////////////////////////////////////////////////

	static EType getType(const std::string& desc_name);
	static const std::string& getDesc(EType asset_type);

	static bool lookupCanLink(EType asset_type);
	static bool lookupIsLinkType(EType asset_type);

	// Whether the asset allows direct download or not:
	static bool lookupIsAssetFetchByIDAllowed(EType asset_type);

	// Whether asset data can be known by the viewer or not:
	static bool lookupIsAssetIDKnowable(EType asset_type);

	// Error string when a lookup fails
	static const std::string& badLookup();

	// The following two static methods used to be defined in the (now removed)
	// indra/newview/llviewerassettype.h file. But there was strictly no reason
	// for not including them here instead, and getting rid of a superfluous
	// dictionary (LLViewerAssetDictionary) in the process...

	// Generates a good default description. You may want to add a verb or
	// agent name after this depending on your application.
	static void generateDescriptionFor(EType asset_type, std::string& desc);
	static EDragAndDropType lookupDragAndDropType(EType asset_type);
};

#endif // LL_LLASSETTYPE_H
