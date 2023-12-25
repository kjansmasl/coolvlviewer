/**
 * @file llcolor3.cpp
 * @brief LLColor3 class implementation.
 *
 * $LicenseInfo:firstyear=2000&license=viewergpl$
 *
 * Copyright (c) 2000-2009, Linden Research, Inc.
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

#include "llcolor3.h"

#include "llcolor4.h"
#include "llvector4.h"

#if LL_WINDOWS
# pragma warning(disable : 4996) // strncpy the sux0r
#endif

LLColor3 LLColor3::white(1.f, 1.f, 1.f);
LLColor3 LLColor3::black(0.f, 0.f, 0.f);
LLColor3 LLColor3::grey(0.5f, 0.5f, 0.5f);

LLColor3::LLColor3(const LLColor4& a) noexcept
{
	mV[0] = a.mV[0];
	mV[1] = a.mV[1];
	mV[2] = a.mV[2];
}

LLColor3::LLColor3(const LLVector4& a) noexcept
{
	mV[0] = a.mV[0];
	mV[1] = a.mV[1];
	mV[2] = a.mV[2];
}

const LLColor3& LLColor3::operator=(const LLColor4& a) noexcept
{
	mV[0] = a.mV[0];
	mV[1] = a.mV[1];
	mV[2] = a.mV[2];
	return *this;
}

LLColor3::LLColor3(const char* color_string) noexcept
{
	if (strlen(color_string) <  6)
	{
		mV[0] = mV[1] = mV[2] = 0.f;
		return;
	}
	constexpr F32 ONE255TH = 1.f / 255.f;
	char tempstr[7];
	strncpy(tempstr, color_string, 6);
	tempstr[6] = '\0';
	mV[VZ] = (F32)strtol(&tempstr[4], NULL, 16) * ONE255TH;
	tempstr[4] = '\0';
	mV[VY] = (F32)strtol(&tempstr[2], NULL, 16) * ONE255TH;
	tempstr[2] = '\0';
	mV[VX] = (F32)strtol(&tempstr[0], NULL, 16) * ONE255TH;
}

std::ostream& operator<<(std::ostream& s, const LLColor3& a)
{
	s << "{ " << a.mV[VX] << ", " << a.mV[VY] << ", " << a.mV[VZ] << " }";
	return s;
}

static F32 hueToRgb(F32 val1, F32 val2, F32 hue)
{
	if (hue < 0.f)
	{
		hue += 1.f;
	}
	else if (hue > 1.f)
	{
		hue -= 1.f;
	}
	if (6.f * hue < 1.f)
	{
		return val1 + (val2 - val1) * 6.f * hue;
	}
	if (2.f * hue < 1.f)
	{
		return val2;
	}
	if (3.f * hue < 2.f)
	{
		constexpr F32 TWO3RD = 2.f / 3.f;
		return val1 + (val2 - val1) * (TWO3RD - hue) * 6.f;
	}
	return val1;
}

void LLColor3::setHSL(F32 hue, F32 sat, F32 lum)
{
	if (sat < 0.00001f)
	{
		mV[VRED] = mV[VGREEN] = mV[VBLUE] = lum;
	}
	else
	{
		F32 interval2 = lum < 0.5f ? lum * (1.f + sat) : lum + sat - sat * lum;
		F32 interval1 = 2.f * lum - interval2;
		constexpr F32 ONE3RD = 1.f / 3.f;
		mV[VRED] = hueToRgb(interval1, interval2, hue + ONE3RD);
		mV[VGREEN] = hueToRgb(interval1, interval2, hue);
		mV[VBLUE] = hueToRgb(interval1, interval2, hue - ONE3RD);
	}
}

void LLColor3::calcHSL(F32* hue, F32* saturation, F32* luminance) const
{
	const F32& r = mV[VRED];
	const F32& g = mV[VGREEN];
	const F32& b = mV[VBLUE];

	F32 var_min = (r < (g < b ? g : b) ? r : (g < b ? g : b));
	F32 var_max = (r > (g > b ? g : b) ? r : (g > b ? g : b));

	F32 l = (var_max + var_min) * 0.5f;
	if (luminance)
	{
		*luminance = l;
	}

	F32 delta = var_max - var_min;
	if (saturation)
	{
		F32 s = 0.f;
		if (delta != 0.f)
		{
			if (l < 0.5f)
			{
			    s = delta / (var_max + var_min);
			}
			else
			{
		    	s = delta / (2.f - var_max - var_min);
			}
		}
		*saturation = s;
	}

	if (hue)
	{
		F32 h = 0.f;
		if (delta != 0.f)
		{
			constexpr F32 ONE6TH = 1.f / 6.f;
			F32 half_delta = delta * 0.5f;
			F32 del_r = ((var_max - r) * ONE6TH + half_delta) / delta;
			F32 del_g = ((var_max - g) * ONE6TH + half_delta) / delta;
			F32 del_b = ((var_max - b) * ONE6TH + half_delta) / delta;

			if (r >= var_max)
			{
				h = del_b - del_g;
			}
			else if (g >= var_max)
			{
				constexpr F32 ONE3RD = 1.f / 3.f;
				h = ONE3RD + del_r - del_b;
			}
			else if (b >= var_max)
			{
				constexpr F32 TWO3RD = 2.f / 3.f;
				h = TWO3RD + del_g - del_r;
			}

			if (h < 0.f)
			{
				h += 1.f;
			}
			else if (h > 1.f)
			{
				h -= 1.f;
			}
		}
		*hue = h;
	}
}
