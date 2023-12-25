/**
 * @file llpathfindingcharacter.cpp
 * @brief Definition of a pathfinding character that contains various properties required for havok pathfinding.
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

#include "llpathfindingcharacter.h"

#include "llpathfindingobject.h"
#include "llsd.h"

#define CHARACTER_CPU_TIME_FIELD   "cpu_time"
#define CHARACTER_HORIZONTAL_FIELD "horizontal"
#define CHARACTER_LENGTH_FIELD     "length"
#define CHARACTER_RADIUS_FIELD     "radius"

LLPathfindingCharacter::LLPathfindingCharacter(const LLUUID& id,
											   const LLSD& char_data)
:	LLPathfindingObject(id, char_data),
	mCPUTime(0U),
	mIsHorizontal(false),
	mLength(0.f),
	mRadius(0.f)
{
	parseCharacterData(char_data);
}

LLPathfindingCharacter::LLPathfindingCharacter(const LLPathfindingCharacter& obj)
:	LLPathfindingObject(obj),
	mCPUTime(obj.mCPUTime),
	mIsHorizontal(obj.mIsHorizontal),
	mLength(obj.mLength),
	mRadius(obj.mRadius)
{
}

LLPathfindingCharacter& LLPathfindingCharacter::operator=(const LLPathfindingCharacter& obj)
{
	dynamic_cast<LLPathfindingObject&>(*this) = obj;

	mCPUTime = obj.mCPUTime;
	mIsHorizontal = obj.mIsHorizontal;
	mLength = obj.mLength;
	mRadius = obj.mRadius;

	return *this;
}

void LLPathfindingCharacter::parseCharacterData(const LLSD& char_data)
{
	if (char_data.has(CHARACTER_CPU_TIME_FIELD) &&
		char_data.get(CHARACTER_CPU_TIME_FIELD).isReal())
	{
		mCPUTime = char_data.get(CHARACTER_CPU_TIME_FIELD).asReal();
	}
	else
	{
		llwarns << "Malformed pathfinding character data: no CPU time"
				<< llendl;
	}

	if (char_data.has(CHARACTER_HORIZONTAL_FIELD) &&
		char_data.get(CHARACTER_HORIZONTAL_FIELD).isBoolean())
	{
		mIsHorizontal = char_data.get(CHARACTER_HORIZONTAL_FIELD).asBoolean();
	}
	else
	{
		llwarns << "Malformed pathfinding character data: no horizontal flag"
				<< llendl;
	}

	if (char_data.has(CHARACTER_LENGTH_FIELD) &&
		char_data.get(CHARACTER_LENGTH_FIELD).isReal())
	{
		mLength = char_data.get(CHARACTER_LENGTH_FIELD).asReal();
	}
	else
	{
		llwarns << "Malformed pathfinding character data: no length"
				<< llendl;
	}

	if (char_data.has(CHARACTER_RADIUS_FIELD) &&
		char_data.get(CHARACTER_RADIUS_FIELD).isReal())
	{
		mRadius = char_data.get(CHARACTER_RADIUS_FIELD).asReal();
	}
	else
	{
		llwarns << "Malformed pathfinding character data: no radius"
				<< llendl;
	}
}
