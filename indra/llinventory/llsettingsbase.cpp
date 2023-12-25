/**
 * @file llsettingsbase.cpp
 * @brief A base class for asset based settings groups.
 *
 * $LicenseInfo:firstyear=2018&license=viewergpl$
 *
 * Copyright (c) 2001-2019, Linden Research, Inc.
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

#include <ostream>
#include <sstream>

#include "llsettingsbase.h"

#include "llsdserialize.h"

#define VALIDATION_DEBUG 0

constexpr F32 BREAK_POINT = 0.5f;

const std::string LLSettingsBase::SETTING_ID		= "id";
const std::string LLSettingsBase::SETTING_NAME		= "name";
const std::string LLSettingsBase::SETTING_HASH		= "hash";
const std::string LLSettingsBase::SETTING_TYPE		= "type";
const std::string LLSettingsBase::SETTING_ASSETID	= "asset_id";
const std::string LLSettingsBase::SETTING_FLAGS		= "flags";

std::ostream& operator<<(std::ostream& os, LLSettingsBase& settings)
{
	LLSDSerialize::serialize(settings.getSettings(), os,
							 LLSDSerialize::LLSD_NOTATION);
	return os;
}

LLSettingsBase::LLSettingsBase()
:	mSettings(LLSD::emptyMap()),
	mBlendedFactor(0.0),
	mDirty(true)
{
}

LLSettingsBase::LLSettingsBase(const LLSD& setting)
:	mSettings(setting),
	mBlendedFactor(0.0),
	mDirty(true)
{
}

void LLSettingsBase::lerpSettings(const LLSettingsBase& other, F64 mix)
{
	mSettings = interpolateSDMap(mSettings, other.mSettings,
								 other.getParameterMap(), mix);
	setDirtyFlag(true);
}

LLSD LLSettingsBase::combineSDMaps(const LLSD& settings,
								   const LLSD& other) const
{
	LLSD new_settings;

	for (LLSD::map_const_iterator it = settings.beginMap(),
								  end = settings.endMap();
		 it != end; ++it)
	{
		const std::string& key_name = it->first;
		const LLSD& value = it->second;

		LLSD::Type setting_type = value.type();
		switch (setting_type)
		{
			case LLSD::TypeMap:
				new_settings[key_name] = combineSDMaps(value, LLSD());
				break;

			case LLSD::TypeArray:
				new_settings[key_name] = LLSD::emptyArray();
				for (LLSD::array_const_iterator ita = value.beginArray(),
												enda = value.endArray();
					 ita != enda; ++ita)
				{
					new_settings[key_name].append(*ita);
				}
				break;

			default:
				new_settings[key_name] = value;
		}
	}

	if (!other.isUndefined())
	{
		for (LLSD::map_const_iterator it = other.beginMap(),
									  end = other.endMap();
			 it !=end; ++it)
		{
			const std::string& key_name = it->first;
			const LLSD& value = it->second;

			LLSD::Type setting_type = value.type();
			switch (setting_type)
			{
				case LLSD::TypeMap:
					new_settings[key_name] = combineSDMaps(value, LLSD());
					break;

				case LLSD::TypeArray:
					new_settings[key_name] = LLSD::emptyArray();
					for (LLSD::array_const_iterator ita = value.beginArray(),
													enda = value.endArray();
						 ita != enda; ++ita)
					{
						new_settings[key_name].append(*ita);
					}
					break;

				default:
					new_settings[key_name] = value;
			}
		}
	}

	return new_settings;
}

LLSD LLSettingsBase::interpolateSDMap(const LLSD& settings, const LLSD& other,
									  const parammapping_t& defaults,
									  F64 mix) const
{
	llassert(mix >= 0.f && mix <= 1.f);

	LLSD new_settings;

	const stringset_t& skip = getSkipInterpolateKeys();
	const stringset_t& slerps = getSlerpKeys();

	for (LLSD::map_const_iterator it = settings.beginMap(),
								  end = settings.endMap();
		 it != end; ++it)
	{
		const std::string& key_name = it->first;
		if (skip.find(key_name) != skip.end())
		{
			continue;
		}

		const LLSD& value = it->second;
		LLSD other_value;
		if (other.has(key_name))
		{
			other_value = other[key_name];
		}
		else
		{
			parammapping_t::const_iterator def_it = defaults.find(key_name);
			if (def_it != defaults.end())
			{
				other_value = def_it->second.getDefaultValue();
			}
			else if (value.type() == LLSD::TypeMap)
			{
				// Interpolate in case there are defaults inside (part of legacy)
				other_value = LLSDMap();
			}
			else
			{
				// The other or defaults does not contain this setting, keep
				// the original value. *TODO: blend this out instead ?
				new_settings[key_name] = value;
				continue;
			}
		}
		new_settings[key_name] = interpolateSDValue(key_name, value,
													other_value, defaults, mix,
													slerps);
	}

	// Special handling cases

	if (settings.has(SETTING_FLAGS))
	{
		U32 flags = (U32)settings[SETTING_FLAGS].asInteger();
		if (other.has(SETTING_FLAGS))
		{
			flags |= (U32)other[SETTING_FLAGS].asInteger();
		}
		new_settings[SETTING_FLAGS] = LLSD::Integer(flags);
	}

	// Now add anything that is in other but not in the settings
	for (LLSD::map_const_iterator it = other.beginMap(), end = other.endMap();
		 it != end; ++it)
	{
		const std::string& key_name = it->first;
		if (skip.find(key_name) != skip.end() || settings.has(key_name))
		{
			continue;
		}

		parammapping_t::const_iterator def_it = defaults.find(key_name);
		if (def_it != defaults.end())
		{
			// Blend against default value
			new_settings[key_name] =
				interpolateSDValue(key_name, def_it->second.getDefaultValue(),
								   it->second, defaults, mix, slerps);
		}
		else if (it->second.type() == LLSD::TypeMap)
		{
			// Interpolate in case there are defaults inside (part of legacy)
			new_settings[key_name] = interpolateSDValue(key_name, LLSDMap(),
														it->second, defaults,
														mix, slerps);
		}
		// Else do nothing when no known defaults. *TODO: blend out instead ?
	}

	// Note: writes variables from skip list: bug ?
	for (LLSD::map_const_iterator it = other.beginMap(), end = other.endMap();
		 it != end; ++it)
	{
		// *TODO: blend this in instead ?
		if (skip.find(it->first) == skip.end())
		{
			continue;
		}

		if (settings.has(it->first))
		{
			new_settings[it->first] = it->second;
		}
	}

	return new_settings;
}

LLSD LLSettingsBase::interpolateSDValue(const std::string& key_name,
										const LLSD& value,
										const LLSD& other_value,
										const parammapping_t& defaults,
										F64 mix,
										const stringset_t& slerps) const
{
	LLSD new_value;
	LLSD::Type setting_type = value.type();

	if (other_value.type() != setting_type)
	{
		// The data type mismatched between this and other. Hard switch when we
		// pass the break point, but issue a warning.
		llwarns << "Setting lerp between mismatched types for '" << key_name
				<< "'." << llendl;
		new_value = mix > BREAK_POINT ? other_value : value;
	}

	switch (setting_type)
	{
		case LLSD::TypeInteger:
			// lerp between the two values rounding the result to the nearest
			// integer
			new_value = LLSD::Integer(ll_round(lerp(value.asReal(),
											   other_value.asReal(), mix)));
			break;

		case LLSD::TypeReal:
			// lerp between the two values.
			new_value = LLSD::Real(lerp(value.asReal(), other_value.asReal(),
										mix));
			break;

		case LLSD::TypeMap:
			// Deep copy.
			new_value = interpolateSDMap(value, other_value, defaults, mix);
			break;

		case LLSD::TypeArray:
		{
			LLSD new_array(LLSD::emptyArray());

			if (slerps.find(key_name) != slerps.end())
			{
				LLQuaternion a(value);
				LLQuaternion b(other_value);
				LLQuaternion q = slerp(mix, a, b);
				new_array = q.getValue();
			}
			else
			{
				// *TODO: We could expand this to inspect the type and do a
				// deep lerp based on type. For now assume a heterogeneous
				// array of reals.
				for (U32 i = 0, len = llmax(value.size(), other_value.size());
					 i < len; ++i)
				{
					new_array[i] = lerp(value[i].asReal(),
										other_value[i].asReal(), mix);
				}
			}

			new_value = new_array;
			break;
		}

		case LLSD::TypeUUID:
			new_value = value.asUUID();
			break;

		default:
			// Atomic or unknown data types. Lerping between them does not make
			// sense so switch at the break.
			new_value = mix > BREAK_POINT ? other_value : value;
	}

	return new_value;
}

//virtual
const LLSettingsBase::stringset_t& LLSettingsBase::getSkipInterpolateKeys() const
{
	static stringset_t skip_set;
	if (skip_set.empty())
	{
		skip_set.emplace(SETTING_FLAGS);
		skip_set.emplace(SETTING_HASH);
	}
	return skip_set;
}

//virtual
const LLSettingsBase::stringset_t& LLSettingsBase::getSlerpKeys() const
{
	static stringset_t slerp_keys;
	return slerp_keys;
}

//virtual
const LLSettingsBase::parammapping_t& LLSettingsBase::getParameterMap() const
{
	static parammapping_t param_mapping;
	return param_mapping;
}

LLSD LLSettingsBase::cloneSettings() const
{
	U32 flags = getFlags();
	LLSD settings(combineSDMaps(getSettings(), LLSD()));
	if (flags)
	{
		settings[SETTING_FLAGS] = LLSD::Integer(flags);
	}
	return settings;
}

size_t LLSettingsBase::getHash() const
{
	// Get a shallow copy of the LLSD filtering out values to not include in
	// the hash
	LLSD hash_settings =
		llsd_shallow(getSettings(),
					 LLSDMap(SETTING_NAME, false)(SETTING_ID, false)(SETTING_HASH, false)("*", true));
	return hash_value(hash_settings);
}

bool LLSettingsBase::validate()
{
	const validation_list_t& validations = getValidationList();

	if (!mSettings.has(SETTING_TYPE))
	{
		mSettings[SETTING_TYPE] = getSettingsType();
	}

	LLSD result = LLSettingsBase::settingValidation(mSettings, validations);

	if (result["errors"].size() > 0)
	{
		llwarns << "Validation errors: " << result["errors"] << llendl;
	}
	if (result["warnings"].size() > 0)
	{
		LL_DEBUGS("EnvSettings") << "Validation warnings: "
								 << result["warnings"] << LL_ENDL;
	}

	return result["success"].asBoolean();
}

LLSD LLSettingsBase::settingValidation(LLSD& settings,
									   const validation_list_t& validations,
									   bool partial)
{
	static Validator validateName(SETTING_NAME, false, LLSD::TypeString,
								  boost::bind(&Validator::verifyStringLength,
											  _1, _2, 63));
	static Validator validateId(SETTING_ID, false, LLSD::TypeUUID);
	static Validator validateHash(SETTING_HASH, false, LLSD::TypeInteger);
	static Validator validateType(SETTING_TYPE, false, LLSD::TypeString);
	static Validator validateAssetId(SETTING_ASSETID, false, LLSD::TypeUUID);
	static Validator validateFlags(SETTING_FLAGS, false, LLSD::TypeInteger);
	stringset_t validated, strip;
	bool is_valid = true;
	LLSD errors(LLSD::emptyArray());
	LLSD warnings(LLSD::emptyArray());

	U32 flags = 0;
	if (partial)
	{
		flags = Validator::VALIDATION_PARTIAL;
	}

	// Fields common to all settings.
	if (!validateName.verify(settings, flags))
	{
		errors.append(LLSD::String("Unable to validate 'name'."));
		is_valid = false;
	}
	validated.emplace(validateName.getName());

	if (!validateId.verify(settings, flags))
	{
		errors.append(LLSD::String("Unable to validate 'id'."));
		is_valid = false;
	}
	validated.emplace(validateId.getName());

	if (!validateHash.verify(settings, flags))
	{
		errors.append(LLSD::String("Unable to validate 'hash'."));
		is_valid = false;
	}
	validated.emplace(validateHash.getName());

	if (!validateAssetId.verify(settings, flags))
	{
		errors.append(LLSD::String("Invalid asset Id"));
		is_valid = false;
	}
	validated.emplace(validateAssetId.getName());

	if (!validateType.verify(settings, flags))
	{
		errors.append(LLSD::String("Unable to validate 'type'."));
		is_valid = false;
	}
	validated.emplace(validateType.getName());

	if (!validateFlags.verify(settings, flags))
	{
		errors.append(LLSD::String("Unable to validate 'flags'."));
		is_valid = false;
	}
	validated.emplace(validateFlags.getName());

	// Fields for specific settings.
	for (validation_list_t::const_iterator it = validations.begin(),
										   end = validations.end();
		 it != end; ++it)
	{
#if VALIDATION_DEBUG
		LLSD oldvalue;
		if (settings.has(it->getName()))
		{
			oldvalue = llsd_clone(mSettings[it->getName()]);
		}
#endif
		if (!it->verify(settings, flags))
		{
			std::stringstream errtext;
			errtext << "Settings LLSD fails validation and could not be corrected for '"
					<< it->getName() << "'!\n";
			errors.append(errtext.str());
			is_valid = false;
		}
		validated.emplace(it->getName());
#if VALIDATION_DEBUG
		if (!oldvalue.isUndefined() && !compare_llsd(settings[it->getName()],
													 oldvalue))
		{
			llwarns << "Setting '" << it->getName() << "' was changed: "
					<< oldvalue << " -> " << settings[it->getName()] << llendl;
		}
#endif
	}

	// Strip extra entries
	for (LLSD::map_const_iterator it = settings.beginMap(),
								  end = settings.endMap();
		 it != end; ++it)
	{
		if (validated.find(it->first) == validated.end())
		{
			std::stringstream warntext;

			warntext << "Stripping setting '" << it->first << "'";
			warnings.append( warntext.str() );
			strip.emplace(it->first);
		}
	}

	for (stringset_t::iterator it = strip.begin(), end = strip.end();
		 it != end; ++it)
	{
		settings.erase(*it);
	}

	return LLSDMap("success",
				   LLSD::Boolean(is_valid))("errors", errors)("warnings", warnings);
}

bool LLSettingsBase::Validator::verify(LLSD& data, U32 flags) const
{
	if (!data.has(mName) || (data.has(mName) && data[mName].isUndefined()))
	{
		if (flags & VALIDATION_PARTIAL)
		{
			// We are doing a partial validation; do no attempt to set a
			// default if missing (or fail even if required).
			return true;
		}

		if (!mDefault.isUndefined())
		{
			data[mName] = mDefault;
			return true;
		}

		if (mRequired)
		{
			llwarns << "Missing required setting '" << mName
					<< "' with no default." << llendl;
		}

		return !mRequired;
	}

	if (data[mName].type() != mType)
	{
		llwarns << "Setting '" << mName << "' is incorrect type." << llendl;
		return false;
	}

	if (!mVerify.empty() && !mVerify(data[mName], flags))
	{
		llwarns << "Setting '" << mName << "' fails validation." << llendl;
		return false;
	}

	return true;
}

bool LLSettingsBase::Validator::verifyColor(LLSD& value, U32)
{
	return value.size() == 3 || value.size() == 4;
}

bool LLSettingsBase::Validator::verifyVector(LLSD& value, U32, size_t length)
{
	return value.size() == length;
}

bool LLSettingsBase::Validator::verifyVectorNormalized(LLSD& value, U32,
													   size_t length)
{
	if (value.size() != length)
	{
		return false;
	}

	LLSD newvector;

	switch (length)
	{
		case 2:
		{
			LLVector2 vect(value);
			if (is_approx_equal(vect.normalize(), 1.f))
			{
				return true;
			}
			newvector = vect.getValue();
			break;
		}

		case 3:
		{
			LLVector3 vect(value);
			if (is_approx_equal(vect.normalize(), 1.f))
			{
				return true;
			}
			newvector = vect.getValue();
			break;
		}

		case 4:
		{
			LLVector4 vect(value);
			if (is_approx_equal(vect.normalize(), 1.f))
			{
				return true;
			}
			newvector = vect.getValue();
			break;
		}

		default:
			return false;
	}

	for (size_t index = 0; index < length; ++index)
	{
		value[index] = newvector[index];
	}

	return true;
}

bool LLSettingsBase::Validator::verifyVectorMinMax(LLSD& value, U32,
												   LLSD minvals, LLSD maxvals)
{
	for (U32 index = 0, count = value.size(); index < count; ++index)
	{
		if (minvals[index].asString() != "*" &&
			minvals[index].asReal() > value[index].asReal())
		{
			value[index] = minvals[index].asReal();
		}
		if (maxvals[index].asString() != "*" &&
			maxvals[index].asReal() < value[index].asReal())
		{
			value[index] = maxvals[index].asReal();
		}
	}

	return true;
}

bool LLSettingsBase::Validator::verifyQuaternion(LLSD& value, U32)
{
	return value.size() == 4;
}

bool LLSettingsBase::Validator::verifyQuaternionNormal(LLSD& value, U32)
{
	if (value.size() != 4)
	{
		return false;
	}

	LLQuaternion quat(value);
	if (is_approx_equal(quat.normalize(), 1.f))
	{
		return true;
	}

	LLSD newquat = quat.getValue();
	for (S32 index = 0; index < 4; ++index)
	{
		value[index] = newquat[index];
	}

	return true;
}

bool LLSettingsBase::Validator::verifyFloatRange(LLSD& value, U32, LLSD range)
{
	F64 real = value.asReal();
	F64 clampedval = llclamp(LLSD::Real(real), range[0].asReal(),
							 range[1].asReal());

	if (!is_approx_equal(clampedval, real))
	{
		value = LLSD::Real(clampedval);
	}

	return true;
}

bool LLSettingsBase::Validator::verifyIntegerRange(LLSD& value, U32,
												   LLSD range)
{
	S32 ival = value.asInteger();
	S32 clampedval = llclamp(LLSD::Integer(ival), range[0].asInteger(),
							 range[1].asInteger());

	if (clampedval != ival)
	{
		value = LLSD::Integer(clampedval);
	}

	return true;
}

bool LLSettingsBase::Validator::verifyStringLength(LLSD& value, U32,
												  size_t length)
{
	std::string sval = value.asString();
	if (!sval.empty())
	{
		sval = sval.substr(0, length);
		value = LLSD::String(sval);
	}
	return true;
}

void LLSettingsBlender::update(F64 blendf)
{
	F64 res = setBlendFactor(blendf);
	if (res >= 0.0 && res < 1.0)
	{
		mTarget->update();
	}
	else
	{
		llassert(false);
	}
}

F64 LLSettingsBlender::setBlendFactor(F64 blendf_in)
{
	F32 blendf = blendf_in;
	if (blendf >= 1.f)
	{
		triggerComplete();
	}
	blendf = llclamp(blendf, 0.f, 1.f);

	if (mTarget)
	{
		mTarget->replaceSettings(mInitial->getSettings());
		mTarget->blend(mFinal, blendf);
	}
	else
	{
		llwarns << "No target for settings blender." << llendl;
	}

	return blendf;
}

void LLSettingsBlender::triggerComplete()
{
	if (mTarget)
	{
		mTarget->replaceSettings(mFinal->getSettings());
	}

	// Prevents this from deleting too soon
	LLSettingsBlender::ptr_t hold = shared_from_this();

	mTarget->update();
	mOnFinished(shared_from_this());
}

void LLSettingsBlender::reset(LLSettingsBase::ptr_t& initsetting,
							  const LLSettingsBase::ptr_t& endsetting, F32)
{
	// Note: the 'span' reset parameter is unused by the base class.
	if (!mInitial)
	{
		llwarns << "Reseting blender with empty initial setting. Expect badness in the future."
				<< llendl;
	}

	mInitial = initsetting;
	mFinal = endsetting;

	if (!mFinal)
	{
		mFinal = mInitial;
	}

	if (mTarget)
	{
		mTarget->replaceSettings(mInitial->getSettings());
	}
}

F64 LLSettingsBlenderTimeDelta::calculateBlend(F32 spanpos, F32 spanlen) const
{
	return fmod((F64)spanpos, (F64)spanlen) / (F64)spanlen;
}

bool LLSettingsBlenderTimeDelta::applyTimeDelta(F64 timedelta)
{
	mTimeSpent += timedelta;

	if (mTimeSpent > mBlendSpan)
	{
		triggerComplete();
		return false;
	}

	F64 blendf = calculateBlend(mTimeSpent, mBlendSpan);

	if (fabs(mLastBlendF - blendf) < mBlendFMinDelta)
	{
		return false;
	}

	mLastBlendF = blendf;
	update(blendf);
	return true;
}
