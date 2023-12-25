/**
 * @file lltrans.cpp
 * @brief LLTrans implementation
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

#include "linden_common.h"

#include "lltrans.h"

#include "llnotifications.h"
#include "lluictrlfactory.h"
#include "llxmlnode.h"

LLTrans::map_t LLTrans::sStringTemplates;
LLTrans::map_t LLTrans::sUIStringTemplates;

//static
void LLTrans::parseStrings(const std::string& xml_filename,
						   map_t& templates)
{
	LLXMLNodePtr root;
	bool success = LLUICtrlFactory::getLayeredXMLNode(xml_filename, root);
	if (!success || root.isNull() || !root->hasName("strings"))
	{
		llerrs << "Problem reading strings: " << xml_filename << llendl;
	}

	for (LLXMLNode* string = root->getFirstChild(); string != NULL;
		 string = string->getNextSibling())
	{
		if (!string->hasName("string"))
		{
			continue;
		}

		std::string string_name;

		if (!string->getAttributeString("name", string_name))
		{
			llwarns << "Unable to parse string with no name" << llendl;
			continue;
		}

		LLTransTemplate xml_template(string_name, string->getTextContents());
		templates[xml_template.mName] = xml_template;
	}
}

//static
void LLTrans::init()
{
	parseStrings("ui_strings.xml", sUIStringTemplates);
	parseStrings("strings.xml", sStringTemplates);
}

//static
bool LLTrans::hasString(const std::string& xml_desc)
{
	return sStringTemplates.find(xml_desc) != sStringTemplates.end();
}

//static
bool LLTrans::findString(const std::string& xml_desc, map_t::const_iterator& it,
						 bool ui_string)
{
	const map_t& templates = ui_string ? sUIStringTemplates : sStringTemplates;
	it = templates.find(xml_desc);
	if (it == templates.end())
	{
		LLSD args;
		args["STRING_NAME"] = xml_desc;
		llwarns_once << "Missing String in "
					 << (ui_string ? "ui_strings" : "strings") << ".xml: "
					 << xml_desc << llendl;
		gNotifications.add("MissingString", args);
		return false;
	}
	return true;
}

//static
std::string LLTrans::getString(const std::string& xml_desc,
							   const LLStringUtil::format_map_t& args)
{
	map_t::const_iterator iter;
	if (!findString(xml_desc, iter))
	{
		return xml_desc;
	}
	std::string text = iter->second.mText;
	LLStringUtil::format(text, args);
	LL_DEBUGS("GetStringTrans") << "Translating '" << xml_desc << "': " << text
								<< LL_ENDL;
	return text;
}

//static
std::string LLTrans::getString(const std::string& xml_desc,
							   const LLSD& msg_args)
{
	map_t::const_iterator iter;
	if (!findString(xml_desc, iter))
	{
		return xml_desc;
	}
	std::string text = iter->second.mText;
	LLStringUtil::format(text, msg_args);
	LL_DEBUGS("GetStringTrans") << "Translating '" << xml_desc << "': " << text
								<< LL_ENDL;
	return text;
}

//static
const std::string& LLTrans::getUIString(const std::string& xml_desc)
{
	map_t::const_iterator iter;
	return findString(xml_desc, iter, true) ? iter->second.mText : xml_desc;
}
