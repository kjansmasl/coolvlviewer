/**
 * @file llinventoryicon.h
 * @brief Class definition of the inventory icon.
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

#ifndef LL_LLINVENTORYICON_H
#define LL_LLINVENTORYICON_H

#include "llassettype.h"
#include "llinventorytype.h"
#include "llui.h"

// Purely static class
class LLInventoryIcon
{
public:
	LLInventoryIcon() = delete;
	~LLInventoryIcon() = delete;

	// Note: in the methods below, 'misc_flag' have different meanings
	// depending on item type.

	static const std::string& getIconName(LLAssetType::EType asset_type,
										  LLInventoryType::EType inv_type =
											LLInventoryType::IT_NONE,
										  U32 misc_flag = 0,
										  bool item_is_multi = false);
	static const std::string& getIconName(LLInventoryType::EIconName idx);

	static const LLUIImagePtr getIcon(LLAssetType::EType asset_type,
									  LLInventoryType::EType inv_type =
										LLInventoryType::IT_NONE,
									  U32 misc_flag = 0,
									  bool item_is_multi = false);
	static const LLUIImagePtr getIcon(LLInventoryType::EIconName idx);

protected:
	static const LLInventoryType::EIconName getIconIdx(LLAssetType::EType asset_type,
													   LLInventoryType::EType inv_type,
													   U32 misc_flag,
													   bool item_is_multi);

	static LLInventoryType::EIconName assignWearableIcon(U32 misc_flag);
	static LLInventoryType::EIconName assignSettingsIcon(U32 misc_flag);
};

#endif // LL_LLINVENTORYICON_H
