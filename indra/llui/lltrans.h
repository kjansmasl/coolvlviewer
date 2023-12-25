/**
 * @file lltrans.h
 * @brief LLTrans definition
 *
 * $LicenseInfo:firstyear=2000&license=viewergpl$
 *
 * Copyright (c) 2000-2009, Linden Research, Inc.
 * Copyright (c) 2023, Henri Beauchamp.
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

#ifndef LL_TRANS_H
#define LL_TRANS_H

#include "hbfastmap.h"
#include "llstring.h"

// String template loaded from strings.xml
class LLTransTemplate
{
public:
	LLTransTemplate(const std::string& name = LLStringUtil::null,
					const std::string& text = LLStringUtil::null)
	:	mName(name),
		mText(text)
	{
	}

	std::string mName;
	std::string mText;
};

// Purely static class for localized strings with a general usage and which do
// not belong to any specific floater or panel XUI definition. E.g. "Owner:",
// "Retrieving...".
// Note: LLUITrans has been merged with LLTrans since they performed the same
// operations with just different string files loaded; LLTrans::getUIString()
// should now be used instead of the now removed LLUITrans::getString(). HB

class LLTrans
{
protected:
	LOG_CLASS(LLTrans);

public:
	LLTrans() = delete;
	~LLTrans() = delete;

	// Called only once, at early viewer initialization stage (from
	// LLAppViewer::initWindow()). HB
	static void init();

	// Returns the translated string for 'xml_desc' string name, 'args' is a
	// list of substrings to replace in the string.
	static std::string getString(const std::string& xml_desc,
								 const LLStringUtil::format_map_t& args);
	static std::string getString(const std::string& xml_desc,
								 const LLSD& args);
	static bool hasString(const std::string& xml_desc);

	// Returns a translated string for 'xml_desc' string name
	LL_INLINE static std::string getString(const std::string& xml_desc)
	{
		LLStringUtil::format_map_t empty;
		return getString(xml_desc, empty);
	}

	// Same as above, but returns a wide characters string.
	LL_INLINE static LLWString getWString(const std::string& xml_desc)
	{
		return utf8str_to_wstring(getString(xml_desc));
	}

	// Returns a translated string for a llui-specific 'xml_desc' string name.
	static const std::string& getUIString(const std::string& xml_desc);

private:
	typedef fast_hmap<std::string, LLTransTemplate> map_t;

	// Parses the 'xml_filename' file that holds the strings. Stops with a
	// llerrs if something went wrong. Used only by init(). HB
	static void parseStrings(const std::string& xml_filename,
							 map_t& templates);

	// Used to find a string in one of the two template maps. Returns true when
	// found, with the iterator pointing on the template, or false when not
	// found (iterator equal to end() for the corresponding templates map).
	// Searches the llui-specicfic strings when ui_string is true. HB
	static bool findString(const std::string& xml_desc,
						   map_t::const_iterator& it, bool ui_string = false);

private:
	static map_t sStringTemplates;
	static map_t sUIStringTemplates;
};

class LLAnimStateLabels
{
public:
	LL_INLINE static std::string getStateLabel(const char* anim_name)
	{
		return LLTrans::getString(std::string("anim_") +
								  std::string(anim_name));
	}
};

#endif
