/**
 * @file llsavedlogins.h
 * @brief Manages a list of previous successful logins
 *
 * $LicenseInfo:firstyear=2009&license=viewergpl$
 *
 * Copyright (c) 2009, Linden Research, Inc.
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

#ifndef LLLOGINHISTORY_H
#define LLLOGINHISTORY_H

#include <list>

#include "llsd.h"

#include "llgridmanager.h"

// Enable only for debugging, never for release builds !
#define LL_DEBUG_LOGIN_PASSWORD 0

// Holds data for a single login attempt.
class LLSavedLoginEntry
{
protected:
	LOG_CLASS(LLSavedLoginEntry);

public:
	// Constructs a history entry from an appropriate LLSD containing
	// serialized LLSavedLoginEntry. Throws invalid_argument if the LLSD is
	// null or does not contain the required fields.
	LLSavedLoginEntry(const LLSD& entry_data);

	// Constructs a history entry from individual fields. With 'gridinfo' the
	// grid as EGridInfo enumeration, 'firstname' and 'lastname' the resident's
	// legacy full name name and 'password' the munged password of
	// MD5HEX_STR_BYTES.
	LLSavedLoginEntry(const EGridInfo gridinfo, const std::string& firstname,
					  const std::string& lastname, const std::string& password);

	// Returns the readable name for the grid. May be "Other" or "None" too.
	std::string getGridLabel() const;

	// Returns the internal name of the grid Id associated with this entry.
	std::string getGridName() const;

	// Gets the grid Id associated with this entry as an EGridInfo enumeration
	// index corresponding to grid.
	EGridInfo getGrid() const;

	// Sets the grid associated with the entry.
	LL_INLINE void setGrid(EGridInfo grid)
	{
		mEntry.insert("grid",
					  LLGridManager::getInstance()->getKnownGridLabel(grid));
	}

	// Gets the grid URI associated with the entry, if any.
	LL_INLINE const LLURI getGridURI() const
	{
		return (mEntry.has("griduri") ? mEntry.get("griduri").asURI()
									  : LLURI());
	}

	// Sets the grid URI associated with the entry.
	LL_INLINE void setGridURI(const LLURI& uri)
	{
		mEntry.insert("griduri", uri);
	}

	// Gets the login page URI associated with the entry, if any.
	LL_INLINE const LLURI getLoginPageURI() const
	{
		return mEntry.has("loginpageuri") ? mEntry.get("loginpageuri").asURI()
										  : LLURI();
	}

	// Sets the login page URI associated with the entry.
	LL_INLINE void setLoginPageURI(const LLURI& uri)
	{
		mEntry.insert("loginpageuri", uri);
	}

	// Gets the helper URI associated with the entry, if any.
	LL_INLINE const LLURI getHelperURI() const
	{
		return mEntry.has("helperuri") ? mEntry.get("helperuri").asURI()
									   : LLURI();
	}

	// Sets the helper URI associated with the entry.
	LL_INLINE void setHelperURI(const LLURI& uri)
	{
		mEntry.insert("helperuri", uri);
	}

	// Returns the first name associated with this login entry.
	LL_INLINE const std::string getFirstName() const
	{
		return (mEntry.has("firstname") ? mEntry.get("firstname").asString()
										: std::string());
	}

	// Sets the first name associated with this login entry.
	LL_INLINE void setFirstName(std::string& value)
	{
		mEntry.insert("firstname", LLSD(value));
	}

	// Returns the last name associated with this login entry.
	LL_INLINE const std::string getLastName() const
	{
		return (mEntry.has("lastname") ? mEntry.get("lastname").asString()
									   : std::string());
	}

	// Sets the last name associated with this login entry.
	LL_INLINE void setLastName(std::string& value)
	{
		mEntry.insert("lastname", LLSD(value));
	}

	// Returns the password associated with this entry. The password is stored
	// encrypted, but will be returned as a plain-text, pre-munged string of
	// MD5HEX_STR_BYTES.
	const std::string getPassword() const;

	// Sets the password associated with this entry. The password is stored
	// with system-specific encryption internally. It must be supplied to this
	// method as a munged string of MD5HEX_STR_BYTES.
	void setPassword(const std::string& value);

	// Returns the login entry as an LLSD for serialization.
	LLSD asLLSD() const;

	// Provides a string containing the username and grid for display.
	const std::string getDisplayString() const;

	static const std::string decryptPassword(const LLSD& pwdata);
	static const LLSD encryptPassword(const std::string& password);

private:
	LLSD mEntry;
};

// Holds a user's login history.
class LLSavedLogins
{
protected:
	LOG_CLASS(LLSavedLogins);

public:
	LLSavedLogins();

	// Constructs a login history from an LLSD array of history entries.
	LLSavedLogins(const LLSD& history_data);

	// Adds a new login history entry.
	void addEntry(const LLSavedLoginEntry& entry);

	// Deletes a login history entry by looking up its name and grid.
	void deleteEntry(EGridInfo grid, const std::string& firstname,
					 const std::string& lastname, const std::string& griduri);

	typedef std::list<LLSavedLoginEntry> list_t;
	typedef list_t::const_reverse_iterator list_const_rit_t;

	// Accesses internal list of login entries from the history.
	LL_INLINE const list_t& getEntries() const		{ return mEntries; }

	// Returns the login history as an LLSD for serialization.
	LLSD asLLSD() const;

	// Returns the count of login entries in the history.
	LL_INLINE const size_t size() const				{ return mEntries.size(); }

	// Loads a login history object from disk.
	static LLSavedLogins loadFile(const std::string& filepath);

	// Saves a login history object to absolute path on disk as XML. Returns
	// true if history was successfully saved, false if it was not.
	static bool saveFile(const LLSavedLogins& history,
						 const std::string& filepath);

private:
	list_t mEntries;
};

#endif // LLLOGINHISTORY_H
