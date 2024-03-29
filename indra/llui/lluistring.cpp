/**
 * @file lluistring.cpp
 * @brief LLUIString implementation.
 *
 * $LicenseInfo:firstyear=2006&license=viewergpl$
 *
 * Copyright (c) 2006-2009, Linden Research, Inc.
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

#include "lluistring.h"

#include "llsd.h"

const LLStringUtil::format_map_t LLUIString::sNullArgs;

// Statics
bool LLUIString::sCurrencyKnown = false;
std::string LLUIString::sGridCurrency;
std::string LLUIString::sRealCurrency;
LLUIString::pending_currency_t LLUIString::sPendingCurrencyUIStrings;

LLUIString::LLUIString()
:	mOrig(),
	mArgs()
{
}

LLUIString::LLUIString(const std::string& instring,
					   const LLStringUtil::format_map_t& args)
:	mArgs(args)
{
	assign(instring);
}

LLUIString::LLUIString(const std::string& instring)
:	mArgs()
{
	assign(instring);
}

LLUIString::~LLUIString()
{
	if (!sPendingCurrencyUIStrings.empty())
	{
		// In case an UI string containing pending currency translations gets
		// destroyed before we perform the said translation after login...
		sPendingCurrencyUIStrings.erase(this);
	}
}

void LLUIString::assign(const std::string& s)
{
	mOrig = s;
	if (sCurrencyKnown)
	{
		// in-lined (instead of calling translateCurrency()) for speed
		if (!sGridCurrency.empty())
		{
			LLStringUtil::replaceString(mOrig, "L$", sGridCurrency);
		}
		if (!sRealCurrency.empty())
		{
			LLStringUtil::replaceString(mOrig, "US$", sRealCurrency);
		}
	}
	else if (mOrig.find("L$") != std::string::npos ||
			 mOrig.find("US$") != std::string::npos)
	{
		sPendingCurrencyUIStrings.insert(this);
	}
	format();
}

void LLUIString::setArgList(const LLStringUtil::format_map_t& args)
{
	mArgs = args;
	format();
}

void LLUIString::setArgs(const LLSD& sd)
{
	if (sd.isMap())
	{
		for (LLSD::map_const_iterator sd_it = sd.beginMap(), end = sd.endMap();
			 sd_it != end; ++sd_it)
		{
			setArg(sd_it->first, sd_it->second.asString());
		}
		format();
	}
}

void LLUIString::setArg(const std::string& key, const std::string& replacement)
{
	mArgs[key] = replacement;
	format();
}

void LLUIString::truncate(S32 maxchars)
{
	if (maxchars >= 0 && (size_t)maxchars < mWResult.size())
	{
		LLWStringUtil::truncate(mWResult, maxchars);
		mResult = wstring_to_utf8str(mWResult);
	}
}

void LLUIString::erase(S32 charidx, S32 len)
{
	if (charidx >= 0 && (size_t)charidx < mWResult.size())
	{
		mWResult.erase(charidx, len);
		mResult = wstring_to_utf8str(mWResult);
	}
}

void LLUIString::insert(S32 charidx, const LLWString& wchars)
{
	if (charidx >= 0 && (size_t)charidx <= mWResult.size())
	{
		mWResult.insert(charidx, wchars);
		mResult = wstring_to_utf8str(mWResult);
	}
}

void LLUIString::replace(S32 charidx, llwchar wc)
{
	if (charidx >= 0 && (size_t)charidx < mWResult.size())
	{
		mWResult[charidx] = wc;
		mResult = wstring_to_utf8str(mWResult);
	}
}

void LLUIString::clear()
{
	// Keep Args
	mOrig.clear();
	mResult.clear();
	mWResult.clear();
}

void LLUIString::format()
{
	if (mOrig.empty())
	{
		mResult.clear();
		mWResult.clear();
		return;
	}
	mResult = mOrig;
	if (!mArgs.empty())
	{
		LLStringUtil::format(mResult, mArgs);
	}
	mWResult = utf8str_to_wstring(mResult);
}

//static
void LLUIString::setGridCurrency(const std::string& str)
{
	if (str != "L$")
	{
		sGridCurrency = str;
	}
	else
	{
		sGridCurrency.clear();
	}
}

//static
void LLUIString::setRealCurrency(const std::string& str)
{
	if (str != "US$")
	{
		sRealCurrency = str;
	}
	else
	{
		sRealCurrency.clear();
	}
}

//static
void LLUIString::translateCurrency(std::string& text)
{
	if (!sGridCurrency.empty())
	{
		LLStringUtil::replaceString(text, "L$", sGridCurrency);
	}
	if (!sRealCurrency.empty())
	{
		LLStringUtil::replaceString(text, "US$", sRealCurrency);
	}
}

//static
void LLUIString::translatePendingCurrency()
{
	sCurrencyKnown = true;
	for (pending_currency_t::iterator it = sPendingCurrencyUIStrings.begin(),
									  end = sPendingCurrencyUIStrings.end();
		 it != end; ++it)
	{
		LLUIString* uistr = *it;
		translateCurrency(uistr->mOrig);
		uistr->format();
	}
	sPendingCurrencyUIStrings.clear();
}
