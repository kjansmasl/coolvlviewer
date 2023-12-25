/**
 * @file llviewerfoldertype.h
 * @brief Declaration of LLViewerFolderType.
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

#ifndef LL_LLVIEWERFOLDERTYPE_H
#define LL_LLVIEWERFOLDERTYPE_H

#include <string>

#include "llfoldertype.h"

#include "llui.h"	// For LLUIImagePtr

// This class is similar to LLFolderType, but contains (static) methods only
// used by the viewer.

class LLViewerFolderType final : public LLFolderType
{
public:
	// Name used by the UI
	static const std::string& lookupXUIName(EType type);
	static LLFolderType::EType lookupTypeFromXUIName(const std::string& name);

	// Folder icon name:
	static const std::string& lookupIconName(EType type);
	// Folder icon:
	static const LLUIImagePtr lookupIcon(EType type);
	// Folder does not require UI update when changes have occured:
	static bool lookupIsQuietType(EType type);
	// Folder is not displayed if empty:
	static bool lookupIsHiddenIfEmpty(EType type);
	// Default name when creating new category:
	static const std::string& lookupNewCategoryName(EType type);
#if 0	// Not used
	// Default type when creating new category
	static LLFolderType::EType lookupTypeFromNewCatName(const std::string& n);
#endif

protected:
	LL_INLINE LLViewerFolderType()	{}
	~LLViewerFolderType() override = default;
};

#endif // LL_LLVIEWERFOLDERTYPE_H
