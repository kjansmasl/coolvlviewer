/**
 * @file llgridmanager.h
 * @brief Grids management.
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

#ifndef LL_LLVIEWERNETWORK_H
#define LL_LLVIEWERNETWORK_H

#include <time.h>		// For time_t

#include "llstring.h"

class LLHost;

#define EGridInfo S32

/**
 * @brief A singleton class to manage the grids available to the viewer.
 *
 * This class maintains several properties for each known grid, and provides
 * interfaces for obtaining each of these properties given a specified
 * grid. Grids are specified by an identifier (the "grid host", normally
 * (part of) the actual domain name for the grid), which must be unique among
 * all known grids:
 **/
class LLGridManager final : public LLSingleton<LLGridManager>
{
	friend class LLSingleton<LLGridManager>;

protected:
	LOG_CLASS(LLGridManager);

public:
	LLGridManager();

	void setGridChoice(EGridInfo grid);
	void setGridChoice(const std::string& grid_name);
	LL_INLINE void setGridURI(const std::string& uri)	{ mGridURI = uri; }

	// Get the enumeration of the grid choice. Should only return values > 0
	// and <= GRID_INFO_OTHER
	LL_INLINE EGridInfo getGridChoice() const			{ return mGridChoice; }

	// Returns the readable name for the grid choice. If the grid is 'other',
	// returns something the string used to specifiy the grid.
	std::string getGridLabel();

	std::string getKnownGridLabel(EGridInfo grid_index) const;
	const std::string getStaticGridURI(const EGridInfo grid) const;
	const std::string getStaticGridHelperURI(const EGridInfo grid) const;

	const std::string& getGridURI()	 const				{ return mGridURI; }
#if 0	// Not used any more since SL is now on varying IP, AWS servers...
	const std::string getGridIP() const;
#endif

	const std::vector<std::string>& getCommandLineURIs();

	const std::string getHelperURI() const;
	void setHelperURI(const std::string& uri);

	const std::string getLoginPageURI() const;
	void setLoginPageURI(const std::string& uri);

	LL_INLINE const std::string& getWebsiteURL() const	{ return mWebsiteURL; }
	LL_INLINE const std::string& getSupportURL() const	{ return mSupportURL; }
	LL_INLINE const std::string& getAccountURL() const	{ return mAccountURL; }
	LL_INLINE const std::string& getPasswordURL() const	{ return mPasswordURL; }

	// Returns the shorter grid host matching "grid". When "grid" is omitted or
	// empty, returns the current grid's host.
	std::string getGridHost(std::string grid = LLStringUtil::null);

	// Returns an Id for the grid, based on its domain name (stripping leading
	// "prefix." and trailing ".suffix" parts). When "grid" is omitted or
	// empty, returns the current grid's Id.
	std::string getGridId(const std::string& grid = LLStringUtil::null);

	std::string getSLURLBase(const std::string& grid = LLStringUtil::null);
	std::string getAppSLURLBase(const std::string& grid = LLStringUtil::null);

	LL_INLINE void setNameEdited(bool value)			{ mNameEdited = value; }
	LL_INLINE bool nameEdited() const					{ return mNameEdited; }

	void setIsInSecondlife();

	void setMenuColor() const;

	void loadGridsList();

	const EGridInfo gridIndexInList(LLSD& grids, std::string name,
									std::string label = "");

	void loadGridsLLSD(LLSD& grids, const std::string& filename,
					   bool can_edit = false);

	LL_INLINE const LLSD& getGridsList() const			{ return mGridList; }

	static std::string getDomain(const std::string& url);

	// Returns a time stamp in the time zone of the grid: PDT or PST for SL,
	// and since we do not know what else to use, UTC for OpenSim grids. The
	// time zone is automatically appended to the returned string (i.e. it
	// does not need to be part of the 'fmt' format string) when 'append_tz' is
	// true. HB
	static std::string getTimeStamp(time_t time_utc, const std::string& fmt,
									bool append_tz = true);

private:
	void parseCommandLineURIs();

private:
	LLSD						mGridList;
	EGridInfo					mGridChoice;
	std::string					mGridName;
	std::string					mGridHost;
	std::string					mGridURI;
	std::string					mHelperURI;
	std::string					mLoginPageURI;
	std::string					mWebsiteURL;
	std::string					mSupportURL;
	std::string					mAccountURL;
	std::string					mPasswordURL;
	std::vector<std::string>	mCommandLineURIs;

	// Set if the user edits/sets the First or Last name field:
	bool						mNameEdited;
	bool						mVerbose;
};

constexpr EGridInfo DEFAULT_GRID_CHOICE = 1;
constexpr EGridInfo GRID_INFO_NONE = 0;
extern EGridInfo GRID_INFO_OTHER;

extern bool gIsInSecondLife;
extern bool gIsInSecondLifeProductionGrid;
extern bool gIsInSecondLifeBetaGrid;
extern bool gIsInProductionGrid;

// Is the Pacific time zone (aka server time zone) currently in daylight
// savings time ?
extern bool gPacificDaylightTime;

// SecondLife URLs

// Account registration web page
extern const std::string CREATE_ACCOUNT_URL;

extern const std::string AUCTION_URL;

extern const std::string EVENTS_URL;

// Support URL
extern const std::string SUPPORT_URL;

// Forgotten Password URL
extern const std::string FORGOTTEN_PASSWORD_URL;

// Currency page
extern const std::string BUY_CURRENCY_URL;

// LSL script wiki
extern const std::string LSL_DOC_URL;

// Release Notes Redirect URL for Server and Viewer
extern const std::string RELEASE_NOTES_BASE_URL;

// Agni login URI
extern const std::string AGNI_LOGIN_URI;

// Aditi login URI
extern const std::string ADITI_LOGIN_URI;

// Agni helper URI
extern const std::string AGNI_HELPER_URI;

// Aditi helper URI
extern const std::string ADITI_HELPER_URI;

// SL login page URL (legacy)
extern const std::string SL_LOGIN_PAGE_URL;

// Agni Mesh upload validation URL
extern const std::string AGNI_VALIDATE_MESH_UPLOAD_PAGE_URL;

// Aditi Mesh upload validation URL
extern const std::string ADITI_VALIDATE_MESH_UPLOAD_PAGE_URL;

// SL grid status BLOG URL
extern const std::string SL_GRID_STATUS_URL;

#endif
