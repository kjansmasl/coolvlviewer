/**
 * @file llsettingsbase.h
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

#ifndef LL_SETTINGSBASE_H
#define LL_SETTINGSBASE_H

#include <memory>
#include <vector>

#include "boost/function.hpp"
#include "boost/signals2.hpp"

#include "llcolor3.h"
#include "llcolor4.h"
#include "hbfastmap.h"
#include "hbfastset.h"
#include "llquaternion.h"
#include "llsettingstype.h"
#include "llsdutil.h"
#include "lltimer.h"
#include "lluuid.h"
#include "llvector4.h"

class LLShaderUniforms;

constexpr F32 INVALID_TRACKPOS = -1.f;

class LLSettingsBase : public std::enable_shared_from_this<LLSettingsBase>
{
	friend class LLEnvironment;
	friend class LLSettingsDay;
	friend std::ostream& operator<<(std::ostream& os, LLSettingsBase& set);

protected:
	LOG_CLASS(LLSettingsBase);

	LLSettingsBase();
	LLSettingsBase(const LLSD& setting);

public:
	virtual ~LLSettingsBase() = default;

	// Non-copyable
	LLSettingsBase(const LLSettingsBase&) = delete;
	LLSettingsBase& operator=(const LLSettingsBase&) = delete;

	static constexpr U32 FLAG_NOCOPY	= 1;
	static constexpr U32 FLAG_NOMOD		= 2;
	static constexpr U32 FLAG_NOTRANS 	= 4;
	static constexpr U32 FLAG_NOSAVE	= 8;

	static const std::string SETTING_ID;
	static const std::string SETTING_NAME;
	static const std::string SETTING_HASH;
	static const std::string SETTING_TYPE;
	static const std::string SETTING_ASSETID;
	static const std::string SETTING_FLAGS;

	class DefaultParam
	{
	public:
		DefaultParam(S32 key, const LLSD& value)
		:	mShaderKey(key),
			mDefaultValue(value)
		{
		}

		DefaultParam()
		:	mShaderKey(-1)
		{
		}

		LL_INLINE S32 getShaderKey() const				{ return mShaderKey; }
		LL_INLINE const LLSD getDefaultValue() const	{ return mDefaultValue; }

	private:
		S32		mShaderKey;
		LLSD	mDefaultValue;
	};
	// Contains settings' names (map key), related shader id-key and default
	// value for revert in case we need to reset shader (no need to search
	// each time)
	typedef flat_hmap<std::string, DefaultParam>  parammapping_t;

	virtual std::string getSettingsType() const = 0;
	virtual LLSettingsType::EType getSettingsTypeValue() const = 0;

	// Settings status

	LL_INLINE bool hasSetting(const std::string& param) const
	{
		return mSettings.has(param);
	}

	LL_INLINE virtual bool isDirty() const				{ return mDirty; }
	LL_INLINE virtual bool isVeryDirty() const			{ return mReplaced; }

	LL_INLINE void setDirtyFlag(bool dirty)
	{
		mDirty = dirty;
		clearAssetId();
	}

	// Hash will not include Name, ID or a previously stored Hash
	size_t getHash() const;

	LL_INLINE LLUUID getId() const
	{
		return getValue(SETTING_ID).asUUID();
	}

	LL_INLINE std::string getName() const
	{
		return getValue(SETTING_NAME).asString();
	}

	LL_INLINE void setName(std::string val)
	{
		setValue(SETTING_NAME, val);
	}

	LL_INLINE LLUUID getAssetId() const
	{
		if (mSettings.has(SETTING_ASSETID))
		{
			return mSettings[SETTING_ASSETID].asUUID();
		}
		return LLUUID::null;
	}

	LL_INLINE U32 getFlags() const
	{
		if (mSettings.has(SETTING_FLAGS))
		{
			return mSettings[SETTING_FLAGS].asInteger();
		}
		return 0;
	}

	LL_INLINE void setFlags(U32 value)
	{
		setLLSD(SETTING_FLAGS, LLSD::Integer(value));
	}

	LL_INLINE bool getFlag(U32 flag) const
	{
		return mSettings.has(SETTING_FLAGS) &&
			   ((U32)mSettings[SETTING_FLAGS].asInteger() & flag) == flag;
	}

	LL_INLINE void setFlag(U32 flag)
	{
		U32 flags = flag;
		if (mSettings.has(SETTING_FLAGS))
		{
			flags |= (U32)mSettings[SETTING_FLAGS].asInteger();
		}
		if (flags)
		{
			mSettings[SETTING_FLAGS] = LLSD::Integer(flags);
		}
		else
		{
			mSettings.erase(SETTING_FLAGS);
		}
	}

	LL_INLINE void clearFlag(U32 flag)
	{
		U32 flags = 0;
		if (mSettings.has(SETTING_FLAGS))
		{
			flags &= (U32)mSettings[SETTING_FLAGS].asInteger();
		}
		if (flags)
		{
			mSettings[SETTING_FLAGS] = LLSD::Integer(flags);
		}
		else
		{
			mSettings.erase(SETTING_FLAGS);
		}
	}

	LL_INLINE virtual void replaceSettings(const LLSD& settings)
	{
		mBlendedFactor = 0.0;
		setDirtyFlag(true);
		mReplaced = true;
		mSettings = settings;
	}

	LL_INLINE virtual LLSD getSettings() const			{ return mSettings; }

	LL_INLINE void setLLSD(const std::string& name, const LLSD& value)
	{
		mSettings[name] = value;
		mDirty = true;
		if (name != SETTING_ASSETID)
		{
			clearAssetId();
		}
	}

	LL_INLINE void setValue(const std::string& name, const LLSD& value)
	{
		setLLSD(name, value);
	}

	LL_INLINE LLSD getValue(const std::string& name,
							const LLSD& deflt = LLSD()) const
	{
		return mSettings.has(name) ? mSettings[name] : deflt;
	}

	LL_INLINE void setValue(const std::string& name, F32 v)
	{
		setLLSD(name, LLSD::Real(v));
	}

	LL_INLINE void setValue(const std::string& name, const LLVector2& value)
	{
		setValue(name, value.getValue());
	}

	LL_INLINE void setValue(const std::string& name, const LLVector3& value)
	{
		setValue(name, value.getValue());
	}

	LL_INLINE void setValue(const std::string& name, const LLVector4& value)
	{
		setValue(name, value.getValue());
	}

	LL_INLINE void setValue(const std::string& name, const LLQuaternion& value)
	{
		setValue(name, value.getValue());
	}

	LL_INLINE void setValue(const std::string& name, const LLColor3& value)
	{
		setValue(name, value.getValue());
	}

	LL_INLINE void setValue(const std::string& name, const LLColor4& value)
	{
		setValue(name, value.getValue());
	}

	LL_INLINE F64 getBlendFactor() const				{ return mBlendedFactor; }

	// Note this method is marked const but may modify the settings object
	// (note the internal cast). This is so that it may be called without
	// special consideration from getters.
	LL_INLINE void update() const
	{
		if (mDirty || mReplaced)
		{
			((LLSettingsBase*)this)->updateSettings();
		}
	}

	typedef std::shared_ptr<LLSettingsBase> ptr_t;

	virtual void blend(const ptr_t& end, F64 blendf) = 0;

	virtual bool validate();

	virtual ptr_t buildDerivedClone() const = 0;

	class Validator
	{
	public:
		static constexpr U32 VALIDATION_PARTIAL = 1;

		typedef boost::function<bool(LLSD&, U32)> verify_pr;

		Validator(const std::string& name, bool required, LLSD::Type type,
				  verify_pr verify = verify_pr(), const LLSD& defval = LLSD())
		:	mName(name),
			mRequired(required),
			mType(type),
			mVerify(verify),
			mDefault(defval)
		{
		}

		LL_INLINE std::string getName() const			{ return mName; }
		LL_INLINE bool isRequired() const				{ return mRequired; }
		LL_INLINE LLSD::Type getType() const			{ return mType; }

		bool verify(LLSD& data, U32 flags) const;

		// Some basic verifications
		static bool verifyColor(LLSD& value, U32 flags);
		static bool verifyVector(LLSD& value, U32 flags, size_t length);
		static bool verifyVectorMinMax(LLSD& value, U32 flags, LLSD minvals,
									   LLSD maxvals);
		static bool verifyVectorNormalized(LLSD& value, U32 flags,
										   size_t length);
		static bool verifyQuaternion(LLSD& value, U32 flags);
		static bool verifyQuaternionNormal(LLSD& value, U32 flags);
		static bool verifyFloatRange(LLSD& value, U32 flags, LLSD range);
		static bool verifyIntegerRange(LLSD& value, U32 flags, LLSD range);
		static bool verifyStringLength(LLSD& value, U32 flags, size_t length);

	private:
		LLSD::Type  mType;
		verify_pr   mVerify;
		std::string mName;
		LLSD		mDefault;
		bool		mRequired;
	};
	typedef std::vector<Validator> validation_list_t;

	static LLSD settingValidation(LLSD& settings,
								  const validation_list_t& validations,
								  bool partial = false);

	LL_INLINE void setAssetId(const LLUUID& value)
	{
	   // Note that this skips setLLSD
		mSettings[SETTING_ASSETID] = value;
	}

	LL_INLINE void clearAssetId()
	{
		if (mSettings.has(SETTING_ASSETID))
		{
			mSettings.erase(SETTING_ASSETID);
		}
	}

	// Calculate any custom settings that may need to be cached.
	LL_INLINE virtual void updateSettings()				{ mDirty = mReplaced = false; }

protected:
	typedef flat_hset<std::string> stringset_t;

	// Combining settings objects. Customize for specific setting types
	virtual void lerpSettings(const LLSettingsBase& other, F64 mix);

	// Combine settings maps where it can, based on mix rate.
	//  - 'settings' is initial value (mix==0)
	//  - 'other' is target value (mix==1)
	//  - 'defaults' is a list of default values for legacy fields and
	//    (re)setting shaders
	//  - 'mix' from 0 to 1, is the ratio or rate of transition from initial
	//    'settings' to 'other'
	// Return interpolated and combined LLSD map.
	LLSD interpolateSDMap(const LLSD& settings, const LLSD& other,
						  const parammapping_t& defaults, F64 mix) const;
	LLSD interpolateSDValue(const std::string& name, const LLSD& value,
							const LLSD& other, const parammapping_t& defaults,
							F64 mix, const stringset_t& slerps) const;

	// When lerping between settings, some may require special handling.
	// This method gets a list of these key to be skipped by the default
	// settings lerp (handling should be performed in the override of
	// lerpSettings).
	virtual const stringset_t& getSkipInterpolateKeys() const;

	// A list of settings that represent quaternions and should be slerped
	// rather than lerped.
	virtual const stringset_t& getSlerpKeys() const;

	virtual const parammapping_t& getParameterMap() const;

	virtual const validation_list_t& getValidationList() const = 0;

	// Apply any settings that need special handling.
	LL_INLINE virtual void applySpecial(LLShaderUniforms* uniforms)
	{
	}

	LL_INLINE void setBlendFactor(F64 factor)			{ mBlendedFactor = factor; }

	LL_INLINE void replaceWith(LLSettingsBase::ptr_t other)
	{
		replaceSettings(other->cloneSettings());
		setBlendFactor(other->getBlendFactor());
	}

	LLSD cloneSettings() const;

private:
	LLSD combineSDMaps(const LLSD& first, const LLSD& other) const;

private:
	F64			mBlendedFactor;

protected:
	LLAssetID   mAssetID;
	LLSD		mSettings;
	bool		mIsValid;

private:
	bool		mDirty;
	bool		mReplaced;
};

class LLSettingsBlender : public std::enable_shared_from_this<LLSettingsBlender>
{
protected:
	LOG_CLASS(LLSettingsBlender);

public:
	typedef std::shared_ptr<LLSettingsBlender> ptr_t;
	typedef boost::signals2::signal<void(const ptr_t)> finish_signal_t;
	typedef boost::signals2::connection connection_t;

	LLSettingsBlender(const LLSettingsBase::ptr_t& target,
					  const LLSettingsBase::ptr_t& initsetting,
					  const LLSettingsBase::ptr_t& endsetting)
	:	mOnFinished(),
		mTarget(target),
		mInitial(initsetting),
		mFinal(endsetting)
	{
		if (mInitial && mTarget)
		{
			mTarget->replaceSettings(mInitial->getSettings());
		}
		if (!mFinal)
		{
			mFinal = mInitial;
		}
	}

	virtual ~LLSettingsBlender() = default;

	virtual void reset(LLSettingsBase::ptr_t& initsetting,
					  const LLSettingsBase::ptr_t& endsetting, F32);

	LL_INLINE LLSettingsBase::ptr_t getTarget() const	{ return mTarget; }

	LL_INLINE LLSettingsBase::ptr_t getInitial() const	{ return mInitial; }

	LL_INLINE LLSettingsBase::ptr_t getFinal() const	{ return mFinal; }

	LL_INLINE connection_t setOnFinished(const finish_signal_t::slot_type& sig)
	{
		return mOnFinished.connect(sig);
	}

	virtual void update(F64 blendf);

	LL_INLINE virtual bool applyTimeDelta(F64 delta)	{ return false; }

	virtual F64 setBlendFactor(F64 position);

	virtual void switchTrack(S32 trackno, F32 position)	{}

protected:
	void triggerComplete();

protected:
	finish_signal_t			mOnFinished;

	LLSettingsBase::ptr_t   mTarget;
	LLSettingsBase::ptr_t   mInitial;
	LLSettingsBase::ptr_t   mFinal;
};

class LLSettingsBlenderTimeDelta : public LLSettingsBlender
{
protected:
	LOG_CLASS(LLSettingsBlenderTimeDelta);

public:
	static constexpr F64 MIN_BLEND_DELTA = 0.001;

	LLSettingsBlenderTimeDelta(const LLSettingsBase::ptr_t& target,
							   const LLSettingsBase::ptr_t& initsetting,
							   const LLSettingsBase::ptr_t& endsetting,
							   F64 blend_span)
	:	LLSettingsBlender(target, initsetting, endsetting),
		mBlendSpan(blend_span),
		mLastUpdate(0.0),
		mTimeSpent(0.0),
		mBlendFMinDelta(MIN_BLEND_DELTA),
		mLastBlendF(-1.0)
	{
		mTimeStart = LLTimer::getEpochSeconds();
		mLastUpdate = mTimeStart;
	}

	LL_INLINE void reset(LLSettingsBase::ptr_t& initsetting,
						 const LLSettingsBase::ptr_t& endsetting,
						 F32 blend_span) override
	{
		LLSettingsBlender::reset(initsetting, endsetting, blend_span);
		mBlendSpan = blend_span;
		mTimeStart = LLTimer::getEpochSeconds();
		mLastUpdate = mTimeStart;
		mTimeSpent = 0.0;
		mLastBlendF = -1.0;
	}

	bool applyTimeDelta(F64 timedelta) override;

	LL_INLINE void setTimeSpent(F64 time)				{ mTimeSpent = time; }

protected:
	F64 calculateBlend(F32 spanpos, F32 spanlen) const;

protected:
	F64		mLastUpdate;
	F64		mTimeSpent;
	F64		mTimeStart;
	F64		mBlendFMinDelta;
	F64		mLastBlendF;
	F32		mBlendSpan;
};

#endif	// LL_SETTINGSBASE_H
