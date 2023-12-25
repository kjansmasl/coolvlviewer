/**
 * @file llcolor4.cpp
 * @brief LLColor4 class implementation.
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

#include "boost/tokenizer.hpp"

#include "llcolor4.h"

#include "llcolor4u.h"
#include "llcolor3.h"
#include "llvector4.h"
#include "llmath.h"

LLColor4 LLColor4::red(1.f, 0.f, 0.f, 1.f);
LLColor4 LLColor4::green(0.f, 1.f, 0.f, 1.f);
LLColor4 LLColor4::blue(0.f, 0.f, 1.f, 1.f);
LLColor4 LLColor4::black(0.f, 0.f, 0.f, 1.f);
LLColor4 LLColor4::yellow(1.f, 1.f, 0.f, 1.f);
LLColor4 LLColor4::magenta(1.f, 0.f, 1.f, 1.f);
LLColor4 LLColor4::cyan(0.f, 1.f, 1.f, 1.f);
LLColor4 LLColor4::white(1.f, 1.f, 1.f, 1.f);
LLColor4 LLColor4::smoke(0.5f, 0.5f, 0.5f, 0.5f);
LLColor4 LLColor4::grey(0.5f, 0.5f, 0.5f, 1.f);
LLColor4 LLColor4::orange(1.f, 0.5, 0.f, 1.f);
LLColor4 LLColor4::purple(0.6f, 0.2f, 0.8f, 1.f);
LLColor4 LLColor4::pink(1.f, 0.5f, 0.8f, 1.f);
LLColor4 LLColor4::transparent(0.f, 0.f, 0.f, 0.f);

LLColor4 LLColor4::grey1(0.8f, 0.8f, 0.8f, 1.f);
LLColor4 LLColor4::grey2(0.6f, 0.6f, 0.6f, 1.f);
LLColor4 LLColor4::grey3(0.4f, 0.4f, 0.4f, 1.f);
LLColor4 LLColor4::grey4(0.3f, 0.3f, 0.3f, 1.f);
LLColor4 LLColor4::grey5(0.125f, 0.125f, 0.125f, 1.f);

LLColor4 LLColor4::red0(0.5f, 0.f, 0.f, 1.f);
LLColor4 LLColor4::red1(1.f, 0.f, 0.f, 1.f);
LLColor4 LLColor4::red2(0.6f, 0.f, 0.f, 1.f);
LLColor4 LLColor4::red3(1.f, 0.2f, 0.2f, 1.f);
LLColor4 LLColor4::red4(0.5f, 0.1f, 0.1f, 1.f);
LLColor4 LLColor4::red5(0.8f, 0.1f, 0.f, 1.f);

LLColor4 LLColor4::green0(0.f, 0.5f, 0.f, 1.f);
LLColor4 LLColor4::green1(0.f, 1.f, 0.f, 1.f);
LLColor4 LLColor4::green2(0.f, 0.6f, 0.f, 1.f);
LLColor4 LLColor4::green3(0.f, 0.4f, 0.f, 1.f);
LLColor4 LLColor4::green4(0.f, 1.f, 0.4f, 1.f);
LLColor4 LLColor4::green5(0.2f, 0.6f, 0.4f, 1.f);
LLColor4 LLColor4::green6(0.4f, 0.6f, 0.2f, 1.f);
LLColor4 LLColor4::green7(0.6f, 1.f, 0.4f, 1.f);
LLColor4 LLColor4::green8(0.4f, 1.f, 0.6f, 1.f);
LLColor4 LLColor4::green9(0.6f, 1.f, 0.6f, 1.f);

LLColor4 LLColor4::blue0(0.f, 0.f, 0.5f, 1.f);
LLColor4 LLColor4::blue1(0.f, 0.f, 1.f, 1.f);
LLColor4 LLColor4::blue2(0.f, 0.4f, 1.f, 1.f);
LLColor4 LLColor4::blue3(0.2f, 0.2f, 0.8f, 1.f);
LLColor4 LLColor4::blue4(0.f, 0.f, 0.6f, 1.f);
LLColor4 LLColor4::blue5(0.4f, 0.2f, 1.f, 1.f);
LLColor4 LLColor4::blue6(0.4f, 0.5f, 1.f, 1.f);
LLColor4 LLColor4::blue7(0.f, 0.f, 0.5f, 1.f);

LLColor4 LLColor4::yellow1(1.f, 1.f, 0.f, 1.f);
LLColor4 LLColor4::yellow2(0.6f, 0.6f, 0.f, 1.f);
LLColor4 LLColor4::yellow3(0.8f, 1.f, 0.2f, 1.f);
LLColor4 LLColor4::yellow4(1.f, 1.f, 0.4f, 1.f);
LLColor4 LLColor4::yellow5(0.6f, 0.4f, 0.2f, 1.f);
LLColor4 LLColor4::yellow6(1.f, 0.8f, 0.4f, 1.f);
LLColor4 LLColor4::yellow7(0.8f, 0.8f, 0.f, 1.f);
LLColor4 LLColor4::yellow8(0.8f, 0.8f, 0.2f, 1.f);
LLColor4 LLColor4::yellow9(0.8f, 0.8f, 0.4f, 1.f);

LLColor4 LLColor4::orange1(1.f, 0.8f, 0.f, 1.f);
LLColor4 LLColor4::orange2(1.f, 0.6f, 0.f, 1.f);
LLColor4 LLColor4::orange3(1.f, 0.4f, 0.2f, 1.f);
LLColor4 LLColor4::orange4(0.8f, 0.4f, 0.f, 1.f);
LLColor4 LLColor4::orange5(0.9f, 0.5f, 0.f, 1.f);
LLColor4 LLColor4::orange6(1.f, 0.8f, 0.2f, 1.f);

LLColor4 LLColor4::magenta1(1.f, 0.f, 1.f, 1.f);
LLColor4 LLColor4::magenta2(0.6f, 0.2f, 0.4f, 1.f);
LLColor4 LLColor4::magenta3(1.f, 0.4f, 0.6f, 1.f);
LLColor4 LLColor4::magenta4(1.f, 0.2f, 0.8f, 1.f);

LLColor4 LLColor4::purple1(0.6f, 0.2f, 0.8f, 1.f);
LLColor4 LLColor4::purple2(0.8f, 0.2f, 1.f, 1.f);
LLColor4 LLColor4::purple3(0.6f, 0.f, 1.f, 1.f);
LLColor4 LLColor4::purple4(0.4f, 0.f, 0.8f, 1.f);
LLColor4 LLColor4::purple5(0.6f, 0.f, 0.8f, 1.f);
LLColor4 LLColor4::purple6(0.8f, 0.f, 0.6f, 1.f);

LLColor4 LLColor4::pink1(1.f, 0.5f, 0.8f, 1.f);
LLColor4 LLColor4::pink2(1.f, 0.8f, 0.9f, 1.f);

LLColor4 LLColor4::cyan1(0.f, 1.f, 1.f, 1.f);
LLColor4 LLColor4::cyan2(0.4f, 0.8f, 0.8f, 1.f);
LLColor4 LLColor4::cyan3(0.f, 1.f, 0.6f, 1.f);
LLColor4 LLColor4::cyan4(0.6f, 1.f, 1.f, 1.f);
LLColor4 LLColor4::cyan5(0.2f, 0.6f, 1.f, 1.f);
LLColor4 LLColor4::cyan6(0.2f, 0.6f, 0.6f, 1.f);

// Conversion
LLColor4::operator LLColor4U() const
{
	return LLColor4U((U8)llmin((S32)(llmax(0.f, mV[VRED] * 255.f) + 0.5f),
							   255),
					 (U8)llmin((S32)(llmax(0.f, mV[VGREEN] * 255.f) + 0.5f),
							   255),
					 (U8)llmin((S32)(llmax(0.f, mV[VBLUE] * 255.f) + 0.5f),
							   255),
					 (U8)llmin((S32)(llmax(0.f, mV[VALPHA] * 255.f) + 0.5f),
							   255));
}

LLColor4::LLColor4(const LLColor3& vec, F32 a) noexcept
{
	mV[VX] = vec.mV[VX];
	mV[VY] = vec.mV[VY];
	mV[VZ] = vec.mV[VZ];
	mV[VW] = a;
}

LLColor4::LLColor4(const LLColor4U& color4u) noexcept
{
	constexpr F32 SCALE = 1.f / 255.f;
	mV[VX] = color4u.mV[VX] * SCALE;
	mV[VY] = color4u.mV[VY] * SCALE;
	mV[VZ] = color4u.mV[VZ] * SCALE;
	mV[VW] = color4u.mV[VW] * SCALE;
}

LLColor4::LLColor4(const LLVector4& vector4) noexcept
{
	mV[VX] = vector4.mV[VX];
	mV[VY] = vector4.mV[VY];
	mV[VZ] = vector4.mV[VZ];
	mV[VW] = vector4.mV[VW];
}

const LLColor4&	LLColor4::set(const LLColor4U& color4u)
{
	constexpr F32 SCALE = 1.f / 255.f;
	mV[VX] = color4u.mV[VX] * SCALE;
	mV[VY] = color4u.mV[VY] * SCALE;
	mV[VZ] = color4u.mV[VZ] * SCALE;
	mV[VW] = color4u.mV[VW] * SCALE;
	return *this;
}

const LLColor4&	LLColor4::set(const LLColor3& vec)
{
	mV[VX] = vec.mV[VX];
	mV[VY] = vec.mV[VY];
	mV[VZ] = vec.mV[VZ];

#if 0	/// do not change alpha !
	mV[VW] = 1.f;
#endif

	return *this;
}

const LLColor4&	LLColor4::set(const LLColor3& vec, F32 a)
{
	mV[VX] = vec.mV[VX];
	mV[VY] = vec.mV[VY];
	mV[VZ] = vec.mV[VZ];
	mV[VW] = a;
	return *this;
}

void LLColor4::setValue(const LLSD& sd)
{
#if 0
	// Clamping on setValue from LLSD is inconsistent with other set behavior
	F32 val;
	bool out_of_range = false;
	val = sd[0].asReal();
	mV[0] = llclamp(val, 0.f, 1.f);
	out_of_range = mV[0] != val;

	val = sd[1].asReal();
	mV[1] = llclamp(val, 0.f, 1.f);
	out_of_range |= mV[1] != val;

	val = sd[2].asReal();
	mV[2] = llclamp(val, 0.f, 1.f);
	out_of_range |= mV[2] != val;

	val = sd[3].asReal();
	mV[3] = llclamp(val, 0.f, 1.f);
	out_of_range |= mV[3] != val;

	if (out_of_range)
	{
		llwarns << "LLSD color value out of range !" << llendl;
	}
#else
	mV[0] = (F32) sd[0].asReal();
	mV[1] = (F32) sd[1].asReal();
	mV[2] = (F32) sd[2].asReal();
	mV[3] = (F32) sd[3].asReal();
#endif
}

const LLColor4& LLColor4::operator=(const LLColor3& a)
{
	mV[VX] = a.mV[VX];
	mV[VY] = a.mV[VY];
	mV[VZ] = a.mV[VZ];
	// Converting from an rgb sets a=1 (opaque)
	mV[VW] = 1.f;
	return *this;
}

std::ostream& operator<<(std::ostream& s, const LLColor4& a)
{
	s << "{ " << a.mV[VX] << ", " << a.mV[VY] << ", " << a.mV[VZ] << ", "
	  << a.mV[VW] << " }";
	return s;
}

bool operator==(const LLColor4& a, const LLColor3& b)
{
	return a.mV[VX] == b.mV[VX] && a.mV[VY] == b.mV[VY] &&
		   a.mV[VZ] == b.mV[VZ];
}

bool operator!=(const LLColor4& a, const LLColor3& b)
{
	return a.mV[VX] != b.mV[VX] || a.mV[VY] != b.mV[VY] ||
		   a.mV[VZ] != b.mV[VZ];
}

LLColor3 vec4to3(const LLColor4& vec)
{
	LLColor3 temp(vec.mV[VX], vec.mV[VY], vec.mV[VZ]);
	return temp;
}

LLColor4 vec3to4(const LLColor3& vec)
{
	LLColor3 temp(vec.mV[VX], vec.mV[VY], vec.mV[VZ]);
	return temp;
}

LL_INLINE static F32 hue_to_component(F32 val1, F32 val2, F32 hue)
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
		constexpr F32 TWOTHIRD = 2.f / 3.f;
		return val1 + (val2 - val1) * (TWOTHIRD - hue) * 6.f;
	}

	return val1;
}

void LLColor4::setHSL(F32 h, F32 s, F32 l)
{
	if (s < 0.00001f)
	{
		mV[VRED] = mV[VGREEN] = mV[VBLUE] = l;
	}
	else
	{
		F32 v2 = l < 0.5f ? l * (1.f + s) : (l + s) - (s * l);
		F32 v1 = 2.f * l - v2;
		constexpr F32 ONETHIRD = 1.f / 3.f;
		mV[VRED] = hue_to_component(v1, v2, h + ONETHIRD);
		mV[VGREEN] = hue_to_component(v1, v2, h);
		mV[VBLUE] = hue_to_component(v1, v2, h - ONETHIRD);
	}
}

void LLColor4::calcHSL(F32* hue, F32* saturation, F32* luminance) const
{
	F32 var_r = mV[VRED];
	F32 var_g = mV[VGREEN];
	F32 var_b = mV[VBLUE];

	F32 var_min = (var_r < (var_g < var_b ? var_g : var_b) ? var_r :
					(var_g < var_b ? var_g : var_b));
	F32 var_max = (var_r > (var_g > var_b ? var_g : var_b) ? var_r :
					(var_g > var_b ? var_g : var_b));

	F32 del_max = var_max - var_min;

	F32 val_l = (var_max + var_min) * 0.5f;
	F32 val_h = 0.f;
	F32 val_s = 0.f;

	if (del_max != 0.f)
	{
		if (val_l < 0.5f)
		{
			val_s = del_max / (var_max + var_min);
		}
		else
		{
			val_s = del_max / (2.f - var_max - var_min);
		}

		F32 del_r = ((var_max - var_r) / 6.f + del_max * 0.5f) / del_max;
		F32 del_g = ((var_max - var_g) / 6.f + del_max * 0.5f) / del_max;
		F32 del_b = ((var_max - var_b) / 6.f + del_max * 0.5f) / del_max;

		if (var_r >= var_max)
		{
			val_h = del_b - del_g;
		}
		else if (var_g >= var_max)
		{
			constexpr F32 ONETHIRD = 1.f / 3.f;
			val_h = ONETHIRD + del_r - del_b;
		}
		else if (var_b >= var_max)
		{
			constexpr F32 TWOTHIRD = 2.f / 3.f;
			val_h = TWOTHIRD + del_g - del_r;
		}

		if (val_h < 0.f)
		{
			val_h += 1.f;
		}
		else if (val_h > 1.f)
		{
			val_h -= 1.f;
		}
	}

	if (hue)
	{
		*hue = val_h;
	}
	if (saturation)
	{
		*saturation = val_s;
	}
	if (luminance)
	{
		*luminance = val_l;
	}
}

//static
bool LLColor4::parseColor(const std::string& buf, LLColor4* color)
{
	if (buf.empty() || color == NULL)
	{
		return false;
	}

	typedef boost::tokenizer<boost::char_separator<char> > boost_tokenizer;
	boost_tokenizer tokens(buf, boost::char_separator<char>(", "));
	boost_tokenizer::iterator token_iter = tokens.begin();
	if (token_iter == tokens.end())
	{
		return false;
	}

	// Grab the first token into a string, since we do not know whether this is
	// a float or a color name.
	std::string color_name(*token_iter++);

	if (token_iter != tokens.end())
	{
		// There are more tokens to read; this must be a vector.
		LLColor4 v;
		LLStringUtil::convertToF32(color_name, v.mV[VX]);
		LLStringUtil::convertToF32(*token_iter, v.mV[VY]);
		v.mV[VZ] = 0.f;
		v.mV[VW] = 1.f;

		++token_iter;
		if (token_iter == tokens.end())
		{
			// This is a malformed vector.
			llwarns << "Malformed color: " << buf << llendl;
		}
		else
		{
			// There is a z-component.
			LLStringUtil::convertToF32(*token_iter, v.mV[VZ]);

			++token_iter;
			if (token_iter != tokens.end())
			{
				// There is an alpha component.
				LLStringUtil::convertToF32(*token_iter, v.mV[VW]);
			}
		}

		//  Make sure all values are between 0 and 1.
		if (v.mV[VX] > 1.f || v.mV[VY] > 1.f || v.mV[VZ] > 1.f || v.mV[VW] > 1.f)
		{
			constexpr F32 SCALE = 1.f / 255.f;
			v *= SCALE;			// Does not affect alpha
			v.mV[VW] *= SCALE;	// Scale alpha as well !
		}
		color->set(v);
	}
	else // Single value. Read as a named color.
	{
		// We have a color name
		if ("red" == color_name)
		{
			color->set(LLColor4::red);
		}
		else if ("red0" == color_name)
		{
			color->set(LLColor4::red0);
		}
		else if ("red1" == color_name)
		{
			color->set(LLColor4::red1);
		}
		else if ("red2" == color_name)
		{
			color->set(LLColor4::red2);
		}
		else if ("red3" == color_name)
		{
			color->set(LLColor4::red3);
		}
		else if ("red4" == color_name)
		{
			color->set(LLColor4::red4);
		}
		else if ("red5" == color_name)
		{
			color->set(LLColor4::red5);
		}
		else if ("green" == color_name)
		{
			color->set(LLColor4::green);
		}
		else if ("green0" == color_name)
		{
			color->set(LLColor4::green0);
		}
		else if ("green1" == color_name)
		{
			color->set(LLColor4::green1);
		}
		else if ("green2" == color_name)
		{
			color->set(LLColor4::green2);
		}
		else if ("green3" == color_name)
		{
			color->set(LLColor4::green3);
		}
		else if ("green4" == color_name)
		{
			color->set(LLColor4::green4);
		}
		else if ("green5" == color_name)
		{
			color->set(LLColor4::green5);
		}
		else if ("green6" == color_name)
		{
			color->set(LLColor4::green6);
		}
		else if ("green7" == color_name)
		{
			color->set(LLColor4::green7);
		}
		else if ("green8" == color_name)
		{
			color->set(LLColor4::green8);
		}
		else if ("green9" == color_name)
		{
			color->set(LLColor4::green9);
		}
		else if ("blue" == color_name)
		{
			color->set(LLColor4::blue);
		}
		else if ("blue0" == color_name)
		{
			color->set(LLColor4::blue0);
		}
		else if ("blue1" == color_name)
		{
			color->set(LLColor4::blue1);
		}
		else if ("blue2" == color_name)
		{
			color->set(LLColor4::blue2);
		}
		else if ("blue3" == color_name)
		{
			color->set(LLColor4::blue3);
		}
		else if ("blue4" == color_name)
		{
			color->set(LLColor4::blue4);
		}
		else if ("blue5" == color_name)
		{
			color->set(LLColor4::blue5);
		}
		else if ("blue6" == color_name)
		{
			color->set(LLColor4::blue6);
		}
		else if ("blue7" == color_name)
		{
			color->set(LLColor4::blue7);
		}
		else if ("black" == color_name)
		{
			color->set(LLColor4::black);
		}
		else if ("white" == color_name)
		{
			color->set(LLColor4::white);
		}
		else if ("yellow" == color_name)
		{
			color->set(LLColor4::yellow);
		}
		else if ("yellow1" == color_name)
		{
			color->set(LLColor4::yellow1);
		}
		else if ("yellow2" == color_name)
		{
			color->set(LLColor4::yellow2);
		}
		else if ("yellow3" == color_name)
		{
			color->set(LLColor4::yellow3);
		}
		else if ("yellow4" == color_name)
		{
			color->set(LLColor4::yellow4);
		}
		else if ("yellow5" == color_name)
		{
			color->set(LLColor4::yellow5);
		}
		else if ("yellow6" == color_name)
		{
			color->set(LLColor4::yellow6);
		}
		else if ("yellow7" == color_name)
		{
			color->set(LLColor4::yellow7);
		}
		else if ("yellow8" == color_name)
		{
			color->set(LLColor4::yellow8);
		}
		else if ("yellow9" == color_name)
		{
			color->set(LLColor4::yellow9);
		}
		else if ("magenta" == color_name)
		{
			color->set(LLColor4::magenta);
		}
		else if ("magenta1" == color_name)
		{
			color->set(LLColor4::magenta1);
		}
		else if ("magenta2" == color_name)
		{
			color->set(LLColor4::magenta2);
		}
		else if ("magenta3" == color_name)
		{
			color->set(LLColor4::magenta3);
		}
		else if ("magenta4" == color_name)
		{
			color->set(LLColor4::magenta4);
		}
		else if ("purple" == color_name)
		{
			color->set(LLColor4::purple);
		}
		else if ("purple1" == color_name)
		{
			color->set(LLColor4::purple1);
		}
		else if ("purple2" == color_name)
		{
			color->set(LLColor4::purple2);
		}
		else if ("purple3" == color_name)
		{
			color->set(LLColor4::purple3);
		}
		else if ("purple4" == color_name)
		{
			color->set(LLColor4::purple4);
		}
		else if ("purple5" == color_name)
		{
			color->set(LLColor4::purple5);
		}
		else if ("purple6" == color_name)
		{
			color->set(LLColor4::purple6);
		}
		else if ("pink" == color_name)
		{
			color->set(LLColor4::pink);
		}
		else if ("pink1" == color_name)
		{
			color->set(LLColor4::pink1);
		}
		else if ("pink2" == color_name)
		{
			color->set(LLColor4::pink2);
		}
		else if ("cyan" == color_name)
		{
			color->set(LLColor4::cyan);
		}
		else if ("cyan1" == color_name)
		{
			color->set(LLColor4::cyan1);
		}
		else if ("cyan2" == color_name)
		{
			color->set(LLColor4::cyan2);
		}
		else if ("cyan3" == color_name)
		{
			color->set(LLColor4::cyan3);
		}
		else if ("cyan4" == color_name)
		{
			color->set(LLColor4::cyan4);
		}
		else if ("cyan5" == color_name)
		{
			color->set(LLColor4::cyan5);
		}
		else if ("cyan6" == color_name)
		{
			color->set(LLColor4::cyan6);
		}
		else if ("smoke" == color_name)
		{
			color->set(LLColor4::smoke);
		}
		else if ("grey" == color_name)
		{
			color->set(LLColor4::grey);
		}
		else if ("grey1" == color_name)
		{
			color->set(LLColor4::grey1);
		}
		else if ("grey2" == color_name)
		{
			color->set(LLColor4::grey2);
		}
		else if ("grey3" == color_name)
		{
			color->set(LLColor4::grey3);
		}
		else if ("grey4" == color_name)
		{
			color->set(LLColor4::grey4);
		}
		else if ("grey5" == color_name)
		{
			color->set(LLColor4::grey5);
		}
		else if ("orange" == color_name)
		{
			color->set(LLColor4::orange);
		}
		else if ("orange1" == color_name)
		{
			color->set(LLColor4::orange1);
		}
		else if ("orange2" == color_name)
		{
			color->set(LLColor4::orange2);
		}
		else if ("orange3" == color_name)
		{
			color->set(LLColor4::orange3);
		}
		else if ("orange4" == color_name)
		{
			color->set(LLColor4::orange4);
		}
		else if ("orange5" == color_name)
		{
			color->set(LLColor4::orange5);
		}
		else if ("orange6" == color_name)
		{
			color->set(LLColor4::orange6);
		}
		else if ("clear" == color_name)
		{
			color->set(0.f, 0.f, 0.f, 0.f);
		}
		else
		{
			llwarns << "Invalid color: " << color_name << llendl;
			return false;
		}
	}

	return true;
}

//static
bool LLColor4::parseColor4(const std::string& buf, LLColor4* value)
{
	if (buf.empty() || !value)
	{
		return false;
	}

	LLColor4 v;
	S32 count = sscanf(buf.c_str(), "%f, %f, %f, %f",
					   v.mV, v.mV + 1, v.mV + 2, v.mV + 3);
	if (count == 1)
	{
		// Try this format
		count = sscanf(buf.c_str(), "%f %f %f %f",
					   v.mV, v.mV + 1, v.mV + 2, v.mV + 3);
	}
	if (count == 4)
	{
		value->set(v);
		return true;
	}

	return false;
}
