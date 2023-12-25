/**
 * @file llsettingsdaycycle.cpp
 * @brief The day cycles settings asset support class.
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

#include "boost/pointer_cast.hpp"

#include "llsettingsdaycycle.h"

#include "llframetimer.h"
#include "llsettingssky.h"
#include "llsettingswater.h"

// Helper functions

template<typename T>
LL_INLINE T get_wrapping_distance(T begin, T end)
{
	if (begin < end)
	{
		return end - begin;
	}
	else if (begin > end)
	{
		return T(1) - begin + end;
	}
	return 0;
}

static LLSettingsDay::cycle_track_it_t get_wrapping_atafter(LLSettingsDay::cycle_track_t& coll,
															F32 key)
{
	if (coll.empty())
	{
		return coll.end();
	}

	LLSettingsDay::cycle_track_it_t it = coll.upper_bound(key);
	if (it == coll.end())
	{
		// Wrap around
		it = coll.begin();
	}

	return it;
}

static LLSettingsDay::cycle_track_it_t get_wrapping_atbefore(LLSettingsDay::cycle_track_t& coll,
															 F32 key)
{
	if (coll.empty())
	{
		return coll.end();
	}

	LLSettingsDay::cycle_track_it_t it = coll.lower_bound(key);
	if (it == coll.end())
	{
		// All keyframes are lower, take the last one.
		--it; // we know the range is not empty
	}
	else if (it->first > key)
	{
		// The keyframe we are interested in is smaller than the found.
		if (it == coll.begin())
		{
			it = coll.end();
		}
		--it;
	}

	return it;
}

// Constants

const std::string LLSettingsDay::SETTING_KEYID		= "key_id";
const std::string LLSettingsDay::SETTING_KEYNAME	= "key_name";
const std::string LLSettingsDay::SETTING_KEYKFRAME	= "key_keyframe";
const std::string LLSettingsDay::SETTING_KEYHASH	= "key_hash";
const std::string LLSettingsDay::SETTING_TRACKS		= "tracks";
const std::string LLSettingsDay::SETTING_FRAMES		= "frames";

const LLUUID LLSettingsDay::DEFAULT_ASSET_ID("5646d39e-d3d7-6aff-ed71-30fc87d64a91");

// Minimum value to prevent multislider in edit floaters from eating up frames
// that 'encroach' on one another's space
constexpr F32 DEFAULT_MULTISLIDER_INCREMENT = 0.005f;

LLSettingsDay::LLSettingsDay(const LLSD& data)
:	LLSettingsBase(data),
	mInitialized(false)
{
	mDayTracks.resize(TRACK_MAX);
}

LLSettingsDay::LLSettingsDay()
:	LLSettingsBase(),
	mInitialized(false)
{
	mDayTracks.resize(TRACK_MAX);
}

LLSD LLSettingsDay::getSettings() const
{
	LLSD settings(LLSD::emptyMap());

	if (mSettings.has(SETTING_NAME))
	{
		settings[SETTING_NAME] = mSettings[SETTING_NAME];
	}

	if (mSettings.has(SETTING_ID))
	{
		settings[SETTING_ID] = mSettings[SETTING_ID];
	}

	if (mSettings.has(SETTING_ASSETID))
	{
		settings[SETTING_ASSETID] = mSettings[SETTING_ASSETID];
	}

	settings[SETTING_TYPE] = getSettingsType();

	typedef std::map<std::string, LLSettingsBase::ptr_t> str_ptr_map_t;
	str_ptr_map_t in_use;

	LLSD tracks(LLSD::emptyArray());

	for (cycle_list_t::const_iterator it1 = mDayTracks.begin(),
									  end1 = mDayTracks.end();
		 it1 != end1; ++it1)
	{
		LLSD trackout(LLSD::emptyArray());
		for (cycle_track_t::const_iterator it2 = it1->begin(),
										   end2 = it1->end();
			 it2 != end2; ++it2)
		{
			F32 frame = it2->first;
			LLSettingsBase::ptr_t data = it2->second;
			size_t datahash = data->getHash();

			std::stringstream keyname;
			keyname << datahash;

			trackout.append(LLSD(LLSDMap(SETTING_KEYKFRAME, LLSD::Real(frame))
										(SETTING_KEYNAME, keyname.str())));
			in_use[keyname.str()] = data;
		}
		tracks.append(trackout);
	}
	settings[SETTING_TRACKS] = tracks;

	LLSD frames(LLSD::emptyMap());
	for (str_ptr_map_t::iterator it = in_use.begin(), end = in_use.end();
		 it != end; ++it)
	{
		LLSD framesettings =
			llsd_clone(it->second->getSettings(),
					   LLSDMap("*", true)(SETTING_NAME, false)
							  (SETTING_ID, false)(SETTING_HASH, false));

		frames[it->first] = framesettings;
	}
	settings[SETTING_FRAMES] = frames;

	return settings;
}

bool LLSettingsDay::initialize(bool validate_frames)
{
	LLSD tracks = mSettings[SETTING_TRACKS];
	LLSD frames = mSettings[SETTING_FRAMES];

	// Save for later...
	LLUUID assetid;
	if (mSettings.has(SETTING_ASSETID))
	{
		assetid = mSettings[SETTING_ASSETID].asUUID();
	}

	std::map<std::string, LLSettingsBase::ptr_t> used;

	for (LLSD::map_const_iterator it = frames.beginMap(), end = frames.endMap();
		 it != end; ++it)
	{
		const std::string& name = it->first;
		LLSD data = it->second;
		LLSettingsBase::ptr_t keyframe;
		if (data[SETTING_TYPE].asString() == "sky")
		{
			LL_DEBUGS("EnvSettings") << "Building sky frame: " << name
									 << LL_ENDL;
				keyframe = buildSky(data);
		}
		else if (data[SETTING_TYPE].asString() == "water")
		{
			LL_DEBUGS("EnvSettings") << "Building water frame: " << name
									 << LL_ENDL;
			keyframe = buildWater(data);
		}
		else
		{
			llwarns << "Unknown child setting type '"
					<< data[SETTING_TYPE].asString() << "' named '" << name
					<< "'" << llendl;
		}
		if (!keyframe)
		{
			llwarns << "Invalid frame data for child: " << name << llendl;
			continue;
		}

		used[name] = keyframe;
	}

	// We consider frame DEFAULT_FRAME_SLOP_FACTOR away as still encroaching,
	// so add minimum increment
	constexpr F32 MOVE_FACTOR = DEFAULT_FRAME_SLOP_FACTOR +
								DEFAULT_MULTISLIDER_INCREMENT;
	bool haswater = false;
	bool hassky = false;
	for (S32 i = 0, count = llmin((S32)tracks.size(), (S32)TRACK_MAX);
		 i < count; ++i)
	{
		mDayTracks[i].clear();
		LLSD curtrack = tracks[i];
		for (LLSD::array_const_iterator it = curtrack.beginArray(),
										end = curtrack.endArray();
			 it != end; ++it)
		{
			F32 keyframe = (*it)[SETTING_KEYKFRAME].asReal();
			keyframe = llclamp(keyframe, 0.f, 1.f);

			LLSettingsBase::ptr_t setting;
			if (it->has(SETTING_KEYNAME))
			{
				std::string key_name = (*it)[SETTING_KEYNAME];
				if (i == TRACK_WATER)
				{
					setting = used[key_name];
					if (setting && setting->getSettingsType() != "water")
					{
						llwarns << "Water track referencing "
								<< setting->getSettingsType() << " frame at "
								<< keyframe << "." << llendl;
						setting.reset();
					}
				}
				else
				{
					setting = used[key_name];
					if (setting && setting->getSettingsType() != "sky")
					{
						llwarns << "Sky track #" << i << " referencing "
								<< setting->getSettingsType() << " frame at "
								<< keyframe << "." << llendl;
						setting.reset();
					}
				}
			}
			if (!setting)
			{
				continue;
			}

			if (i == TRACK_WATER)
			{
				haswater = true;
			}
			else
			{
				hassky = true;
			}

			if (validate_frames && mDayTracks[i].size() > 0)
			{
				// Check if we hit close to anything in the list
				LLSettingsDay::cycle_track_t::value_type frame =
					getSettingsNearKeyframe(keyframe, i, DEFAULT_FRAME_SLOP_FACTOR);
				if (frame.second)
				{
					// Figure out direction of search
					F32 found = frame.first;
					F32 new_frame = keyframe;
					F32 total_frame_shift = 0;
					if ((new_frame < found &&
						 found - new_frame <= DEFAULT_FRAME_SLOP_FACTOR) ||
						(new_frame > found &&
						 new_frame - found > DEFAULT_FRAME_SLOP_FACTOR))
					{
						// Move backward
						cycle_track_rit_t iter = mDayTracks[i].rbegin();
						while (iter->first != found)
						{
							++iter;
						}
						new_frame = found; // For total_frame_shift
						while (total_frame_shift < 1.f)
						{
							// Calculate shifted position from current found point
							total_frame_shift +=
								MOVE_FACTOR + new_frame -
								(found <= new_frame ? found : found - 1.f);
							new_frame = found - MOVE_FACTOR;
							if (new_frame < 0.f)
							{
								new_frame += 1.f;
							}

							// We know that current point is too close, go for
							// next one
							++iter;
							if (iter == mDayTracks[i].rend())
							{
								iter = mDayTracks[i].rbegin();
							}

							if ((iter->first <= new_frame + DEFAULT_MULTISLIDER_INCREMENT &&
								 new_frame - DEFAULT_FRAME_SLOP_FACTOR <= iter->first) ||
								(iter->first > new_frame &&
								 new_frame - DEFAULT_FRAME_SLOP_FACTOR <= iter->first - 1.f))
							{
								// We are encroaching at new point as well
								found = iter->first;
							}
							else
							{
								// We have:
								// new_frame - DEFAULT_FRAME_SLOP_FACTOR >
								// 	iter->first
								// We found a clear spot.
								break;
							}
						}
					}
					else
					{
						// Move forward
						cycle_track_it_t iter = mDayTracks[i].find(found);
						new_frame = found; // For total_frame_shift
						while (total_frame_shift < 1.f)
						{
							// Calculate shifted position from previous found
							// point
							total_frame_shift +=
								MOVE_FACTOR - new_frame +
								(found >= new_frame ? found : found + 1.f);
							new_frame = found + MOVE_FACTOR;
							if (new_frame > 1.f)
							{
								new_frame -= 1.f;
							}

							// We know that current point is too close, go for
							// next one
							++iter;
							if (iter == mDayTracks[i].end())
							{
								iter = mDayTracks[i].begin();
							}

							if ((iter->first >= new_frame - DEFAULT_MULTISLIDER_INCREMENT &&
								 new_frame + DEFAULT_FRAME_SLOP_FACTOR >= iter->first) ||
								(iter->first < new_frame &&
								 new_frame + DEFAULT_FRAME_SLOP_FACTOR >= iter->first + 1.f))
							{
								// We are encroaching at new point as well
								found = iter->first;
							}
							else
							{
								// We have:
								// new_frame + DEFAULT_FRAME_SLOP_FACTOR <
								//	iter->first
								// We found clear spot.
								break;
							}
						}
					}

					if (total_frame_shift >= 1.f)
					{
						llwarns << "Could not fix frame position, adding as is to position: "
								<< keyframe << llendl;
					}
					else
					{
						// Mark as new position
						keyframe = new_frame;
					}
				}
			}
			mDayTracks[i][keyframe] = setting;
		}
	}

	if (!haswater)
	{
		llwarns << "Must have one water frame !" << llendl;
		return false;
	}

	if (!hassky)
	{
		llwarns << "Must have at least one sky frame !" << llendl;
		return false;
	}

	// These are no longer needed and just take up space now.
	mSettings.erase(SETTING_TRACKS);
	mSettings.erase(SETTING_FRAMES);

	if (!assetid.isNull())
	{
		mSettings[SETTING_ASSETID] = assetid;
	}

	mInitialized = true;
	return true;
}

LLSD LLSettingsDay::defaults()
{
	static LLSD dfltsetting;
	if (dfltsetting.size() == 0)
	{
		dfltsetting[SETTING_NAME] = "_default_";
		dfltsetting[SETTING_TYPE] = "daycycle";

		LLSD frames(LLSD::emptyMap());
		LLSD water_track;
		LLSD sky_track;

		constexpr U32 FRAME_COUNT = 8;
		constexpr F32 FRAME_STEP = 1.f / (F32)FRAME_COUNT;
		F32 time = 0.f;
		for (U32 i = 0; i < FRAME_COUNT; ++i)
		{
			std::string name = "_default_";
			name += 'a' + i;

			std::string water_frame_name = "water:" + name;
			std::string sky_frame_name = "sky:" + name;

			water_track[SETTING_KEYKFRAME] = time;
			water_track[SETTING_KEYNAME] = water_frame_name;

			sky_track[SETTING_KEYKFRAME] = time;
			sky_track[SETTING_KEYNAME] = sky_frame_name;

			frames[water_frame_name] = LLSettingsWater::defaults(time);
			frames[sky_frame_name] = LLSettingsSky::defaults(time);

			time += FRAME_STEP;
		}

		LLSD tracks;
		tracks.append(llsd::array(water_track));
		tracks.append(llsd::array(sky_track));

		dfltsetting[SETTING_TRACKS] = tracks;
		dfltsetting[SETTING_FRAMES] = frames;
	}

	return dfltsetting;
}

void LLSettingsDay::blend(const LLSettingsBase::ptr_t&, F64)
{
	llerrs << "Day cycles are not blendable !" << llendl;
}

bool LLSettingsDay::validateDayCycleTrack(LLSD& value, U32)
{
	// Trim extra tracks.
	while (value.size() > LLSettingsDay::TRACK_MAX)
	{
		value.erase(value.size() - 1);
	}

	S32 framecount = 0;
	for (LLSD::array_iterator track = value.beginArray(),
							  end = value.endArray();
		 track != end; ++track)
	{
		size_t index = 0;
		while (index < track->size())
		{
			LLSD& elem = (*track)[index];

			++framecount;
			if (index >= LLSettingsDay::FRAME_MAX)
			{
				track->erase(index);
				continue;
			}

			if (!elem.has(LLSettingsDay::SETTING_KEYKFRAME))
			{
				track->erase(index);
				continue;
			}

			if (!elem[LLSettingsDay::SETTING_KEYKFRAME].isReal())
			{
				track->erase(index);
				continue;
			}

			if (!elem.has(LLSettingsDay::SETTING_KEYNAME) &&
				!elem.has(LLSettingsDay::SETTING_KEYID))
			{
				track->erase(index);
				continue;
			}

			F32 frame = elem[LLSettingsDay::SETTING_KEYKFRAME].asReal();
			if (frame < 0.f || frame > 1.f)
			{
				frame = llclamp(frame, 0.f, 1.f);
				elem[LLSettingsDay::SETTING_KEYKFRAME] = frame;
			}
			++index;
		}
	}

	S32 water_tracks = value[0].size();
	if (water_tracks < 1)
	{
		llwarns << "Missing water track" << llendl;
		return false;
	}
	if (framecount - water_tracks < 1)
	{
		llwarns << "Missing sky tracks" << llendl;
		return false;
	}

	return true;
}

bool LLSettingsDay::validateDayCycleFrames(LLSD& value, U32 flags)
{
	bool has_sky = false;
	bool has_water = false;

	for (LLSD::map_iterator it = value.beginMap(), end = value.endMap();
		 it != end; ++it)
	{
		LLSD frame = it->second;

		std::string ftype = frame[LLSettingsBase::SETTING_TYPE];
		if (ftype == "sky")
		{
			const LLSettingsSky::validation_list_t& valid_sky =
				LLSettingsSky::validationList();
			LLSD res_sky = LLSettingsBase::settingValidation(frame, valid_sky,
															 flags);
			if (res_sky["success"].asInteger() == 0)
			{
				llwarns << "Sky setting named '" << it->first
						<< "' validation failed: " << res_sky << " - Sky: "
						<< frame << llendl;
				continue;
			}
			has_sky |= true;
		}
		else if (ftype == "water")
		{
			const LLSettingsWater::validation_list_t& valid_h2o =
				LLSettingsWater::validationList();
			LLSD res_h2o = LLSettingsBase::settingValidation(frame, valid_h2o,
															 flags);
			if (res_h2o["success"].asInteger() == 0)
			{
				llwarns << "Water setting named '" << it->first
						<< "' validation failed: " << res_h2o << " - Water: "
						<< frame << llendl;
				continue;
			}
			has_water |= true;
		}
		else
		{
			llwarns << "Unknown settings block of type '" << ftype
					<< "' named '" << it->first << "'" << llendl;
			return false;
		}
	}

	if (!(flags & LLSettingsBase::Validator::VALIDATION_PARTIAL))
	{
		if (!has_sky)
		{
			llwarns << "No skies defined." << llendl;
			return false;
		}
		if (!has_water)
		{
			llwarns << "No waters defined." << llendl;
			return false;
		}
	}

	return true;
}

//virtual
const LLSettingsDay::validation_list_t& LLSettingsDay::getValidationList() const
{
	return validationList();
}

//static
const LLSettingsDay::validation_list_t& LLSettingsDay::validationList()
{
	static validation_list_t validation;
	if (validation.empty())
	{
		validation.emplace_back(SETTING_TRACKS, true, LLSD::TypeArray,
								&validateDayCycleTrack);
		validation.emplace_back(SETTING_FRAMES, true, LLSD::TypeMap,
								&validateDayCycleFrames);
	}
	return validation;
}

LLSettingsDay::cycle_track_t& LLSettingsDay::getCycleTrack(S32 track)
{
	static cycle_track_t empty;
	return (size_t)track < mDayTracks.size() ? mDayTracks[track] : empty;
}

const LLSettingsDay::cycle_track_t& LLSettingsDay::getCycleTrackConst(S32 track) const
{
	static const cycle_track_t empty;
	return (size_t)track < mDayTracks.size() ? mDayTracks[track] : empty;
}

bool LLSettingsDay::clearCycleTrack(S32 track)
{
	if (track < 0 || track >= TRACK_MAX)
	{
		llwarns << "Attempt to clear track (#" << track << ") out of range"
				<< llendl;
		return false;
	}
	mDayTracks[track].clear();
	clearAssetId();
	setDirtyFlag(true);
	return true;
}

bool LLSettingsDay::replaceCycleTrack(S32 track, const cycle_track_t& source)
{
	if (source.empty())
	{
		llwarns << "Attempt to copy an empty track." << llendl;
		return false;
	}

	{
		LLSettingsBase::ptr_t first(source.begin()->second);
		std::string setting_type = first->getSettingsType();

		if ((track && setting_type == "water") ||
			(!track && setting_type == "sky"))
		{
			llwarns << "Attempt to copy track missmatch" << llendl;
			return false;
		}
	}

	if (!clearCycleTrack(track))
	{
		return false;
	}

	mDayTracks[track] = source;
	return true;
}

bool LLSettingsDay::isTrackEmpty(S32 track) const
{
	if (track < 0 || track >= TRACK_MAX)
	{
		llwarns << "Attempt to test track (#" << track << ") out of range"
				<< llendl;
		return true;
	}
	return mDayTracks[track].empty();
}

void LLSettingsDay::startDayCycle()
{
	if (!mInitialized)
	{
		llwarns << "Attempt to start day cycle on uninitialized object."
				<< llendl;
		return;
	}
}

LLSettingsDay::keyframe_list_t LLSettingsDay::getTrackKeyframes(S32 trackno)
{
	if (trackno < 0 || trackno >= TRACK_MAX)
	{
		llwarns << "Attempt get track (#" << trackno << ") out of range"
				<< llendl;
		return keyframe_list_t();
	}

	keyframe_list_t keyframes;
	cycle_track_t& track = mDayTracks[trackno];

	keyframes.reserve(track.size());

	for (cycle_track_it_t it = track.begin(), end = track.end();
		 it != end; ++it)
	{
		keyframes.push_back(it->first);
	}

	return keyframes;
}

bool LLSettingsDay::moveTrackKeyframe(S32 trackno, F32 old_frame,
									  F32 new_frame)
{
	if (trackno < 0 || trackno >= TRACK_MAX)
	{
		llwarns << "Attempt get track (#" << trackno << ") out of range"
				<< llendl;
		return false;
	}

	if (fabsf(old_frame - new_frame) < F_APPROXIMATELY_ZERO)
	{
		return false;
	}

	cycle_track_t& track = mDayTracks[trackno];
	cycle_track_it_t iter = track.find(old_frame);
	if (iter != track.end())
	{
		LLSettingsBase::ptr_t base = iter->second;
		track.erase(iter);
		track[llclamp(new_frame, 0.f, 1.f)] = base;
		track[new_frame] = base;
		return true;
	}

	return false;

}

bool LLSettingsDay::removeTrackKeyframe(S32 trackno, F32 frame)
{
	if (trackno < 0 || trackno >= TRACK_MAX)
	{
		llwarns << "Attempt get track (#" << trackno << ") out of range"
				<< llendl;
		return false;
	}

	cycle_track_t& track = mDayTracks[trackno];
	cycle_track_it_t iter = track.find(frame);
	if (iter != track.end())
	{
		LLSettingsBase::ptr_t base = iter->second;
		track.erase(iter);
		return true;
	}

	return false;
}

void LLSettingsDay::setWaterAtKeyframe(const settings_water_ptr_t& water,
									   F32 keyframe)
{
	setSettingsAtKeyframe(water, keyframe, TRACK_WATER);
}

LLSettingsWater::ptr_t LLSettingsDay::getWaterAtKeyframe(F32 keyframe) const
{
	LLSettingsBase* p = getSettingsAtKeyframe(keyframe, TRACK_WATER).get();
	return LLSettingsWater::ptr_t((LLSettingsWater*)p);
}

void LLSettingsDay::setSkyAtKeyframe(const LLSettingsSky::ptr_t &sky, F32 keyframe, S32 track)
{
	if (track < 1 || track >= TRACK_MAX)
	{
		llwarns << "Attempt to set sky track (#" << track << ") out of range"
				<< llendl;
		return;
	}

	setSettingsAtKeyframe(sky, keyframe, track);
}

LLSettingsSky::ptr_t LLSettingsDay::getSkyAtKeyframe(F32 keyframe, S32 track) const
{
	if ((track < 1) || (track >= TRACK_MAX))
	{
		llwarns << "Attempt to set sky track (#" << track << ") out of range"
				<< llendl;
		return LLSettingsSky::ptr_t();
	}

	return boost::dynamic_pointer_cast<LLSettingsSky>(getSettingsAtKeyframe(keyframe,
																			track));
}

void LLSettingsDay::setSettingsAtKeyframe(const LLSettingsBase::ptr_t& settings,
										  F32 keyframe, S32 track)
{
	if (track < 0 || track >= TRACK_MAX)
	{
		llwarns << "Attempt to set track (#" << track << ") out of range"
				<< llendl;
		return;
	}

	std::string type = settings->getSettingsType();
	if (track == TRACK_WATER && type != "water")
	{
		llwarns << "Attempt to add frame of type '" << type
				<< "' to water track" << llendl;
		llassert(type == "water");
		return;
	}
	else if (track != TRACK_WATER && type != "sky")
	{
		llwarns << "Attempt to add frame of type '" << type
				<< "' to sky track" << llendl;
		llassert(type == "sky");
		return;
	}

	mDayTracks[track][llclamp(keyframe, 0.f, 1.f)] = settings;
	setDirtyFlag(true);
}

LLSettingsBase::ptr_t LLSettingsDay::getSettingsAtKeyframe(F32 keyframe,
														   S32 track) const
{
	if (track < 0 || track >= TRACK_MAX)
	{
		llwarns << "Attempt to set track (#" << track << ") out of range"
				<< llendl;
		return LLSettingsBase::ptr_t();
	}

	// *TODO: better way to identify keyframes ?
	cycle_track_t::const_iterator iter = mDayTracks[track].find(keyframe);
	if (iter != mDayTracks[track].end())
	{
		return iter->second;
	}

	return LLSettingsBase::ptr_t();
}

LLSettingsDay::cycle_track_t::value_type LLSettingsDay::getSettingsNearKeyframe(F32 keyframe,
																				S32 track,
																				F32 fudge) const
{
	if (track < 0 || track >= TRACK_MAX)
	{
		llwarns << "Attempt to get track (#" << track << ") out of range"
				<< llendl;
		return cycle_track_t::value_type(INVALID_TRACKPOS,
										 LLSettingsBase::ptr_t());
	}

	if (mDayTracks[track].empty())
	{
		llinfos << "Empty track" << llendl;
		return cycle_track_t::value_type(INVALID_TRACKPOS,
										 LLSettingsBase::ptr_t());
	}

	F32 startframe = keyframe - fudge;
	if (startframe < 0.f)
	{
		startframe += 1.f;
	}

	LLSettingsDay::cycle_track_t collection =
		const_cast<cycle_track_t&>(mDayTracks[track]);
	cycle_track_it_t it = get_wrapping_atafter(collection, startframe);

	F32 dist = get_wrapping_distance(startframe, it->first);

	cycle_track_it_t next_it = std::next(it);
	if (dist <= DEFAULT_MULTISLIDER_INCREMENT && next_it != collection.end())
	{
		return *next_it;
	}
	else if (dist <= fudge * 2.f)
	{
		return *it;
	}

	return cycle_track_t::value_type(INVALID_TRACKPOS,
									 LLSettingsBase::ptr_t());
}

F32 LLSettingsDay::getUpperBoundFrame(S32 track, F32 keyframe)
{
	return get_wrapping_atafter(mDayTracks[track], keyframe)->first;
}

F32 LLSettingsDay::getLowerBoundFrame(S32 track, F32 keyframe)
{
	return get_wrapping_atbefore(mDayTracks[track], keyframe)->first;
}

LLSettingsDay::track_bound_t LLSettingsDay::getBoundingEntries(cycle_track_t& track,
															   F32 keyframe)
{
	return track_bound_t(get_wrapping_atbefore(track, keyframe),
						 get_wrapping_atafter(track, keyframe));
}

const LLUUID& LLSettingsDay::getDefaultAssetId()
{
	return DEFAULT_ASSET_ID;
}
