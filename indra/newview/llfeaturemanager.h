/**
 * @file llfeaturemanager.h
 * @brief The feature manager is responsible for determining what features are turned on/off in the app.
 *
 * $LicenseInfo:firstyear=2003&license=viewergpl$
 *
 * Copyright (c) 2003-2009, Linden Research, Inc.
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

#ifndef LL_LLFEATUREMANAGER_H
#define LL_LLFEATUREMANAGER_H

// Note: we do not use boost's unordered_maps/sets, because boost's default
// hashing function for strings is slow and the one we use as an overload is
// not well suited for keys that are so very similar ("Render*"), such as for
// feature names, causing most strings to go into the same unordered map
// bucket. The std::map/set classes will be faster for this particular use...
#include <map>
#include <set>

#include "stdtypes.h"

#include "llstring.h"

typedef enum EGPUClass
{
	GPU_CLASS_UNKNOWN = -1,
	GPU_CLASS_0 = 0,
	GPU_CLASS_1,
	GPU_CLASS_2,
	GPU_CLASS_3,
	GPU_CLASS_4,
	GPU_CLASS_5
} EGPUClass;

class LLFeatureInfo
{
public:
	LLFeatureInfo()
	:	mValid(false),
		mAvailable(false),
		mRecommendedLevel(-1.f)
	{
	}

	LLFeatureInfo(const std::string& name, bool available, F32 level);

	LL_INLINE bool isValid() const				{ return mValid; }

public:
	F32			mRecommendedLevel;
	bool		mValid;
	bool		mAvailable;
	std::string	mName;
};

class LLFeatureList
{
protected:
	LOG_CLASS(LLFeatureList);

public:
	typedef std::map<std::string, LLFeatureInfo> feature_map_t;

	LLFeatureList(const std::string& name);
	virtual ~LLFeatureList() = default;

	bool isFeatureAvailable(const std::string& name);

	void setRecommendedLevel(const std::string& name, F32 level);

	void maskList(LLFeatureList& mask);

	void addFeature(const std::string& name, bool available, F32 level);

	LL_INLINE feature_map_t& getFeatures()		{ return mFeatures; }

	void dump();

protected:
	std::string		mName;
	feature_map_t	mFeatures;
};

class LLFeatureManager : public LLFeatureList
{
protected:
	LOG_CLASS(LLFeatureManager);

public:
	LL_INLINE LLFeatureManager()
	:	LLFeatureList("default"),
		mTableVersion(0),
		mSafe(false),
		mGPUClass(GPU_CLASS_UNKNOWN),
		mGPUSupported(false),
		mGPUMemoryBandwidth(0)
	{
	}

	LL_INLINE ~LLFeatureManager()				{ cleanupFeatureTables(); }

	// initialize this by loading feature table and gpu table
	void init();

	// Mask the current feature list with the named list
	void maskCurrentList(const std::string& name);

	bool loadFeatureTables();

	LL_INLINE EGPUClass getGPUClass() const		{ return mGPUClass; }

	LL_INLINE const std::string& getGPUString() const
	{
		return mGPUString;
	}

	LL_INLINE bool isGPUSupported() const		{ return mGPUSupported; }
	LL_INLINE F32 getGPUMemoryBandwidth() const	{ return mGPUMemoryBandwidth; }

	void cleanupFeatureTables();

	LL_INLINE S32 getVersion() const			{ return mTableVersion; }
	LL_INLINE void setSafe(bool safe)			{ mSafe = safe; }
	LL_INLINE bool isSafe() const				{ return mSafe; }

	LLFeatureList* findMask(const std::string& name);
	bool maskFeatures(const std::string& name);

	// Set the graphics to low, medium, high, or ultra. skip_features forces
	// skipping of mostly hardware settings that we don't want to change when
	// we change graphics settings.
	void setGraphicsLevel(S32 level, bool skip_features);

	void applyBaseMasks();
	void applyRecommendedSettings();

	// Apply the basic masks. Also, skip one saved in the skip list if true
	void applyFeatures(bool skip_features);

protected:
	void loadGPUClass(bool benchmark_gpu);
	void initBaseMask();
	static F32 benchmarkGPU();

protected:
	S32										mTableVersion;
	std::map<std::string, LLFeatureList*>	mMaskList;
	std::set<std::string>					mSkippedFeatures;
	F32										mGPUMemoryBandwidth;
	EGPUClass								mGPUClass;
	bool									mGPUSupported;
	// To reinitialize everything to the "safe" mask:
	bool									mSafe;
	std::string								mGPUString;
};

extern LLFeatureManager gFeatureManager;

#endif // LL_LLFEATUREMANAGER_H
