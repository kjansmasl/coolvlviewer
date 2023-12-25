/**
 * @file llsdjson.cpp
 * @brief LLSD flexible data system
 *
 * $LicenseInfo:firstyear=2015&license=viewergpl$
 *
 * Copyright (c) 2015, Linden Research, Inc.
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

#include "llsdjson.h"

LLSD LlsdFromJson(const Json::Value& val)
{
	LLSD result;

	switch (val.type())
	{
		case Json::intValue:
			result = LLSD(static_cast<LLSD::Integer>(val.asInt()));
			break;

		case Json::uintValue:
			result = LLSD(static_cast<LLSD::Integer>(val.asUInt()));
			break;

		case Json::realValue:
			result = LLSD(static_cast<LLSD::Real>(val.asDouble()));
			break;

		case Json::stringValue:
			result = LLSD(static_cast<LLSD::String>(val.asString()));
			break;

		case Json::booleanValue:
			result = LLSD(static_cast<LLSD::Boolean>(val.asBool()));
			break;

		case Json::arrayValue:
			result = LLSD::emptyArray();
			for (Json::ValueConstIterator it = val.begin(), end = val.end();
				 it != end; ++it)
			{
				result.append(LlsdFromJson(*it));
			}
			break;

		case Json::objectValue:
			result = LLSD::emptyMap();
			for (Json::ValueConstIterator it = val.begin(), end = val.end();
				 it != end; ++it)
			{
				result[it.name()] = LlsdFromJson(*it);
			}
			break;

		case Json::nullValue:
		default:
			break;
	}

	return result;
}

Json::Value LlsdToJson(const LLSD& val)
{
	Json::Value result;

	switch (val.type())
	{
		case LLSD::TypeUndefined:
			result = Json::Value::null;
			break;

		case LLSD::TypeBoolean:
			result = Json::Value(static_cast<bool>(val.asBoolean()));
			break;

		case LLSD::TypeInteger:
			result = Json::Value(static_cast<int>(val.asInteger()));
			break;

		case LLSD::TypeReal:
			result = Json::Value(static_cast<double>(val.asReal()));
			break;

		case LLSD::TypeURI:
		case LLSD::TypeDate:
		case LLSD::TypeUUID:
		case LLSD::TypeString:
			result = Json::Value(val.asString());
			break;

		case LLSD::TypeMap:
			result = Json::Value(Json::objectValue);
			for (LLSD::map_const_iterator it = val.beginMap(),
										  end = val.endMap();
				 it != end; ++it)
			{
				result[it->first] = LlsdToJson(it->second);
			}
			break;

		case LLSD::TypeArray:
			result = Json::Value(Json::arrayValue);
			for (LLSD::array_const_iterator it = val.beginArray(),
											end = val.endArray();
				 it != end; ++it)
			{
				result.append(LlsdToJson(*it));
			}
			break;

		case LLSD::TypeBinary:
		default:
			llerrs << "Unsupported conversion to JSON from LLSD type: "
				   << val.type() << llendl;
	}

	return result;
}
