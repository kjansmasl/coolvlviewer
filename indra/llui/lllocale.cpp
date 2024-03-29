/**
 * @file lllocale.cpp
 * @brief Localized resource manager
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 *
 * Copyright (c) 2001-2009, Linden Research, Inc.
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

#include <locale.h>

#include "lllocale.h"

#include "llstring.h"

#if LL_WINDOWS
const std::string LLLocale::USER_LOCALE("English_United States.1252");
const std::string LLLocale::SYSTEM_LOCALE("English_United States.1252");
#elif LL_DARWIN
const std::string LLLocale::USER_LOCALE("en_US.iso8859-1");
const std::string LLLocale::SYSTEM_LOCALE("en_US.iso8859-1");
#else // LL_LINUX likes this
const std::string LLLocale::USER_LOCALE("en_US.utf8");
const std::string LLLocale::SYSTEM_LOCALE("C");
#endif

//static
std::string LLLocale::sPrevFailedLocaleString;

LLLocale::LLLocale(const std::string& locale_string)
{
	mPrevLocaleString = setlocale(LC_ALL, NULL);
	char* new_locale_string = setlocale(LC_ALL, locale_string.c_str());
	if (new_locale_string == NULL && sPrevFailedLocaleString != locale_string)
	{
		llwarns << "Failed to set locale " << locale_string << llendl;
		setlocale(LC_ALL, SYSTEM_LOCALE.c_str());
		sPrevFailedLocaleString = locale_string;
	}
	else
	{
		LL_DEBUGS("Locale") << "Set locale to " << new_locale_string << llendl;
	}
}

LLLocale::~LLLocale()
{
	setlocale(LC_ALL, mPrevLocaleString.c_str());
}

//static
char LLLocale::getDecimalPoint()
{
	char decimal = localeconv()->decimal_point[0];

#if LL_DARWIN
	// On the Mac, locale support is broken before 10.4, which causes things to
	// go all pear-shaped.
	if (decimal == 0)
	{
		decimal = '.';
	}
#endif

	return decimal;
}

//static
char LLLocale::getThousandsSeparator()
{
	char separator = localeconv()->thousands_sep[0];

#if LL_DARWIN
	// On the Mac, locale support is broken before 10.4, which causes things to
	// go all pear-shaped.
	if (separator == 0)
	{
		separator = ',';
	}
#endif

	return separator;
}

//static
char LLLocale::getMonetaryDecimalPoint()
{
	char decimal = localeconv()->mon_decimal_point[0];

#if LL_DARWIN
	// On the Mac, locale support is broken before 10.4, which causes things to
	// go all pear-shaped.
	if (decimal == 0)
	{
		decimal = '.';
	}
#endif

	return decimal;
}

//static
char LLLocale::getMonetaryThousandsSeparator()
{
	char separator = localeconv()->mon_thousands_sep[0];

#if LL_DARWIN
	// On the Mac, locale support is broken before 10.4, which causes things to
	// go all pear-shaped.
	if (separator == 0)
	{
		separator = ',';
	}
#endif

	return separator;
}

// Sets output to a string of integers with monetary separators inserted
// according to the locale.
//static
std::string LLLocale::getMonetaryString(S32 input)
{
	std::string output;

	LLLocale locale(LLLocale::USER_LOCALE);
	struct lconv* conv = localeconv();

#if LL_DARWIN
	// On the Mac, locale support is broken before 10.4, which causes things to
	// go all pear-shaped.
	// Fake up a conv structure with some reasonable values for the fields this
	// function uses.
	struct lconv fakeconv;
	char fake_neg[2] = "-";
	char fake_mon_group[4] = "\x03\x03\x00";	// commas every 3 digits
	if (conv->negative_sign[0] == 0)			// Real locales all seem to have something here...
	{
		fakeconv = *conv;						// start with what's there.
		fakeconv.negative_sign = fake_neg;
		fakeconv.mon_grouping = fake_mon_group;
		fakeconv.n_sign_posn = 1; 				// negative sign before the string
		conv = &fakeconv;
	}
#endif

	char* negative_sign = conv->negative_sign;
	char separator = getMonetaryThousandsSeparator();
	char* grouping = conv->mon_grouping;

	// Note on mon_grouping:
	// Specifies a string that defines the size of each group of digits in
	// formatted monetary quantities. The operand for the mon_grouping keyword
	// consists of a sequence of semicolon-separated integers. Each integer
	// specifies the number of digits in a group. The initial integer defines
	// the size of the group immediately to the left of the decimal delimiter.
	// The following integers define succeeding groups to the left of the
	// previous group. If the last integer is not -1, the size of the previous
	// group (if any) is repeatedly used for the remainder of the digits. If
	// the last integer is -1, no further grouping is performed.

	// Note: we assume here that the currency symbol goes on the left.
	bool negative = (input < 0);
	bool negative_before = negative && (conv->n_sign_posn != 2);
	bool negative_after = negative && (conv->n_sign_posn == 2);

	std::string digits = llformat("%u", abs(input));
	if (!grouping || !grouping[0])
	{
		if (negative_before)
		{
			output.append(negative_sign);
		}
		output.append(digits);
		if (negative_after)
		{
			output.append(negative_sign);
		}
		return output;
	}

	S32 groupings[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	S32 cur_group;
	for (cur_group = 0; grouping[cur_group]; ++cur_group)
	{
		if (grouping[cur_group] != ';')
		{
			groupings[cur_group] = grouping[cur_group];
		}
		cur_group++;

		if (groupings[cur_group] < 0)
		{
			break;
		}
	}
	S32 group_count = cur_group;

	char reversed_output[20] = "";
	char forward_output[20] = "";
	S32 output_pos = 0;

	cur_group = 0;
	S32 pos = digits.size() - 1;
	S32 count_within_group = 0;
	while (pos >= 0 && (groupings[cur_group] >= 0))
	{
		count_within_group++;
		if (count_within_group > groupings[cur_group])
		{
			count_within_group = 1;
			reversed_output[output_pos++] = separator;

			if (cur_group + 1 >= group_count)
			{
				break;
			}
			else
			if (groupings[cur_group + 1] > 0)
			{
				cur_group++;
			}
		}
		reversed_output[output_pos++] = digits[pos--];
	}

	while (pos >= 0)
	{
		reversed_output[output_pos++] = digits[pos--];
	}

	reversed_output[output_pos] = '\0';
	forward_output[output_pos] = '\0';

	for (S32 i = 0; i < output_pos; ++i)
	{
		forward_output[output_pos - 1 - i] = reversed_output[i];
	}

	if (negative_before)
	{
		output.append(negative_sign);
	}
	output.append(forward_output);
	if (negative_after)
	{
		output.append(negative_sign);
	}
	return output;
}

//static
void LLLocale::getIntegerString(std::string& output, S32 input)
{
	output.clear();
	std::string fraction_string;
	while (input > 0)
	{
		S32 fraction = input % 1000;

		if (!output.empty())
		{
			if (fraction == input)
			{
				fraction_string = llformat("%d%c", fraction,
										   getThousandsSeparator());
			}
			else
			{
				fraction_string = llformat("%3.3d%c", fraction,
										   getThousandsSeparator());
			}
			output = fraction_string + output;
		}
		else
		{
			if (fraction == input)
			{
				fraction_string = llformat("%d", fraction);
			}
			else
			{
				fraction_string = llformat("%3.3d", fraction);
			}
			output = fraction_string;
		}
		input /= 1000;
	}
}
