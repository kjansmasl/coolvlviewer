/**
 * @file llslurl.h
 * @brief Handles "SLURL fragments" like Ahern/123/45 for
 * startup processing, login screen, prefs, etc.
 *
 * $LicenseInfo:firstyear=2010&license=viewergpl$
 *
 * Copyright (c) 2010, Linden Research, Inc.
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

#ifndef LLSLURL_H
#define LLSLURL_H

#include "llerror.h"
#include "llstring.h"

class LLAvatarName;
class LLUUID;
class LLVector3;
class LLVector3d;

class LLSLURL
{
protected:
	LOG_CLASS(LLSLURL);

public:
	// If you modify this enumeration, update typeName as well
	enum SLURL_TYPE {
		INVALID,
		LOCATION,
		HOME_LOCATION,
		LAST_LOCATION,
		APP,
		HELP,
		NUM_SLURL_TYPES // must be last
	};

	LL_INLINE LLSLURL()
	:	mType(INVALID)
	{
	}

	LLSLURL(const std::string& slurl);
	LLSLURL(const std::string& grid, const std::string& region);
	LLSLURL(const std::string& region, const LLVector3& position);
	LLSLURL(const std::string& grid, const std::string& region,
			const LLVector3& position);
	LLSLURL(const std::string& grid, const std::string& region,
			const LLVector3d& global_position);
	LLSLURL(const std::string& region, const LLVector3d& global_position);
	LLSLURL(const std::string& command, const LLUUID& id,
			const std::string& verb);

	LL_INLINE SLURL_TYPE getType() const		{ return mType; }

	std::string getSLURLString() const;
	std::string getLocationString() const;

	LL_INLINE std::string getGrid() const		{ return mGrid; }
	LL_INLINE std::string getRegion() const		{ return mRegion; }
	LL_INLINE LLVector3 getPosition() const		{ return mPosition; }
	LL_INLINE std::string getAppCmd() const		{ return mAppCmd; }
	LL_INLINE std::string getAppQuery() const	{ return mAppQuery; }
	LL_INLINE LLSD getAppQueryMap() const		{ return mAppQueryMap; }
	LL_INLINE LLSD getAppPath() const			{ return mAppPath; }

	LL_INLINE bool isValid() const				{ return mType != INVALID; }
	LL_INLINE bool isSpatial() const			{ return mType != INVALID && mType <= LAST_LOCATION; }

	bool operator==(const LLSLURL& rhs);
	LL_INLINE bool operator!=(const LLSLURL& rhs)
	{
		return !(*this == rhs);
	}

    std::string asString() const;

	static uuid_list_t findSLURLs(const std::string& txt);
	static void resolveSLURLs();

private:
	// Get a human-readable version of the type for logging
	static std::string getTypeString(SLURL_TYPE type);

	static void avatarNameCallback(const LLUUID& id,
								   const LLAvatarName& avatar_name);
	static void cacheNameCallback(const LLUUID& id, const std::string& name,
								  bool is_group);
	static void experienceNameCallback(const LLSD& experience_details);

public:
	static const char*	SLURL_SECONDLIFE_SCHEME;
	static const char*	SLURL_HOP_SCHEME;
	static const char*	SLURL_X_GRID_LOCATION_INFO_SCHEME;
	static const char*	SLURL_X_GRID_INFO_SCHEME;
	static const char*	SLURL_HTTPS_SCHEME;
	static const char*	SLURL_HTTP_SCHEME;
	static const char*	SLURL_SECONDLIFE_PATH;
	static const char*	SLURL_COM;
	static const char*	WWW_SLURL_COM;
	static const char*	MAPS_SECONDLIFE_COM;
	static const char*	SIM_LOCATION_HOME;
	static const char*	SIM_LOCATION_LAST;
	static const char*	SLURL_APP_PATH;
	static const char*	SLURL_REGION_PATH;

private:
	SLURL_TYPE			mType;

	// Used for Apps and Help
	std::string			mAppCmd;
	LLSD				mAppPath;
	LLSD				mAppQueryMap;
	std::string			mAppQuery;

	std::string			mGrid;		// Reference to grid manager grid
	std::string			mRegion;
	LLVector3			mPosition;

	static uuid_list_t	sAvatarUUIDs;
	static uuid_list_t	sGroupUUIDs;
	static uuid_list_t	sExperienceUUIDs;
	static uuid_list_t	sObjectsUUIDs;

	typedef std::map<std::string, LLUUID> slurls_map_t;
	static slurls_map_t	sPendingSLURLs;

	static const std::string typeName[NUM_SLURL_TYPES];
};

#endif // LLSLURL_H
