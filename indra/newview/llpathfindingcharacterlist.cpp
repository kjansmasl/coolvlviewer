/**
 * @file llpathfindingcharacterlist.cpp
 * @brief Implementation of llpathfindingcharacterlist
 *
 * $LicenseInfo:firstyear=2012&license=viewergpl$
 *
 * Copyright (c) 2012, Linden Research, Inc.
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

#include "llpathfindingcharacterlist.h"

#include "llpathfindingcharacter.h"
#include "llpathfindingobjectlist.h"
#include "llsd.h"

LLPathfindingCharacterList::LLPathfindingCharacterList()
:	LLPathfindingObjectList()
{
}

LLPathfindingCharacterList::LLPathfindingCharacterList(const LLSD& char_data)
:	LLPathfindingObjectList()
{
	parseCharacterListData(char_data);
}

void LLPathfindingCharacterList::parseCharacterListData(const LLSD& char_data)
{
	LLPathfindingObject::map_t& obj_map = getObjectMap();

	std::string id_str;
	LLUUID id;
	for (LLSD::map_const_iterator iter = char_data.beginMap(),
								  end = char_data.endMap();
		 iter != end; ++iter)
	{
		id_str = iter->first;
		const LLSD& data = iter->second;
		if (!data.size())
		{
			llwarns << "Empty data for path finding character Id: " << id_str
					<< llendl;
			continue;
		}
		if (LLUUID::validate(id_str))
		{
			LLUUID id = LLUUID(id_str);
			obj_map.emplace(id,
							std::make_shared<LLPathfindingCharacter>(id,
																	 data));
		}
		else
		{
			llwarns << "Invalid path finding character Id: " << id_str
					<< llendl;
		}
	}
}
