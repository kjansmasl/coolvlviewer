/**
 * @file llsavedlogins.cpp
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

#include "llviewerprecompiledheaders.h"

#include "llsavedlogins.h"

#include "llmd5.h"				// For MD5HEX_STR_BYTES
#include "llsdserialize.h"
#include "llxorcipher.h"

#include "llappviewer.h"		// For gMACAddress

//-----------------------------------------------------------------------------
// LLSavedLoginEntry class
//-----------------------------------------------------------------------------

LLSavedLoginEntry::LLSavedLoginEntry(const LLSD& entry_data)
{
	if (entry_data.isUndefined() || !entry_data.isMap())
	{
		throw std::invalid_argument("Cannot create a null login entry.");
	}

	if (entry_data.has("grid"))
	{
		if (entry_data.get("grid").isString())
		{
			LL_DEBUGS("Login") << "Grid: " << entry_data["grid"] << LL_ENDL;
		}
		else	
		{
			throw std::invalid_argument("Grid name is not string.");
		}
	}
	else if (entry_data.has("griduri"))
	{
		if (entry_data.get("griduri").isURI())
		{
			LL_DEBUGS("Login") << "Grid URI: " << entry_data["griduri"]
							   << LL_ENDL;
		}
		else
		{
			throw std::invalid_argument("Grid URI is not a valid URI.");
		}
	}
	else
	{
		throw std::invalid_argument("Missing grid name or URI.");
	}

	if (entry_data.has("firstname"))
	{
		if (entry_data.get("firstname").isString())
		{
			LL_DEBUGS("Login") << "First name: " << entry_data["firstname"]
							   << LL_ENDL;
		}
		else
		{
			throw std::invalid_argument("firstname key is not a string.");
		}
	}
	else
	{
		throw std::invalid_argument("Missing firstname key.");
	}

	if (entry_data.has("lastname"))
	{
		if (entry_data.get("lastname").isString())
		{
			LL_DEBUGS("Login") << "Last name: " << entry_data["lastname"]
							   << LL_ENDL;
		}
		else
		{
			throw std::invalid_argument("lastname key is not a string.");
		}
	}
	else
	{
		throw std::invalid_argument("Missing lastname key.");
	}

	if (entry_data.has("password"))
	{
		if (entry_data.get("password").isUndefined())
		{
			LL_DEBUGS("Login") << "Blank password" << LL_ENDL;
		}
		else if (entry_data.get("password").isBinary())
		{
			LL_DEBUGS("Login") << "Encrypted password" << LL_ENDL;
		}
		else
		{
			throw std::invalid_argument("Password key is neither blank nor binary.");
		}
	}
	else
	{
		throw std::invalid_argument("Missing password key.");
	}

	mEntry = entry_data;
}

LLSavedLoginEntry::LLSavedLoginEntry(const EGridInfo grid,
									 const std::string& firstname,
									 const std::string& lastname,
									 const std::string& password)
{
	mEntry.clear();
	LLGridManager* gm = LLGridManager::getInstance();
	std::string gridname = gm->getKnownGridLabel(grid);
	if (gridname == "None")
	{
		mEntry.insert("grid", LLSD("Other"));
		gridname = gm->getStaticGridURI(grid);
		LLStringUtil::toLower(gridname);
		mEntry.insert("griduri", LLSD(LLURI(gridname)));
	}
	else
	{
		mEntry.insert("grid", LLSD(gridname));
	}
	mEntry.insert("firstname", LLSD(firstname));
	mEntry.insert("lastname", LLSD(lastname));
	LL_DEBUGS("Login") << "Login credentials for grid: " << gridname
					   << " - User: " << firstname << " " << lastname
#if LL_DEBUG_LOGIN_PASSWORD
					   << " - Password hash: " << password
#endif
					   << LL_ENDL;
	setPassword(password);
}

EGridInfo LLSavedLoginEntry::getGrid() const
{
	if (mEntry.has("grid"))
	{
		std::string gridname = mEntry.get("grid").asString();
		if (gridname == "Other")
		{
			return GRID_INFO_OTHER;
		}
		if (gridname != "None")
		{
			LLGridManager* gm = LLGridManager::getInstance();
			for (S32 i = 1; i < GRID_INFO_OTHER; ++i)
			{
				if (gm->getKnownGridLabel((EGridInfo)i) == gridname)
				{
					return (EGridInfo)i;
				}
			}
		}
	}
	return GRID_INFO_NONE;
}

std::string LLSavedLoginEntry::getGridLabel() const
{
	return mEntry.has("grid") ? mEntry.get("grid").asString() : "None";
}

std::string LLSavedLoginEntry::getGridName() const
{
	std::string gridname;
	if (mEntry.has("griduri") && mEntry.get("griduri").isURI())
	{
		gridname = mEntry.get("griduri").asURI().hostName();
		LLStringUtil::toLower(gridname);
	}
	else if (mEntry.has("grid"))
	{
		gridname = mEntry.get("grid").asString();
	}
	return gridname;
}

LLSD LLSavedLoginEntry::asLLSD() const
{
	return mEntry;
}

const std::string LLSavedLoginEntry::getDisplayString() const
{
	std::ostringstream etitle;
	etitle << getFirstName() << " " << getLastName() << " (" <<	getGridName()
		   << ")";
	return etitle.str();
}

const std::string LLSavedLoginEntry::getPassword() const
{
	if (mEntry.has("password"))
	{
		std::string hash = decryptPassword(mEntry.get("password"));
#if LL_DEBUG_LOGIN_PASSWORD
		LL_DEBUGS("Login") << "Password hash: " << hash << LL_ENDL;
#endif
		return decryptPassword(mEntry.get("password"));
	}
	else
	{
		LL_DEBUGS("Login") << "No password." << LL_ENDL;
		return  LLStringUtil::null;
	}
}

void LLSavedLoginEntry::setPassword(const std::string& value)
{
#if LL_DEBUG_LOGIN_PASSWORD
	LL_DEBUGS("Login") << "Password hash: " << value << LL_ENDL;
#else
	LL_DEBUGS("Login") << "Setting " << (value.empty() ? "empty" : "encrypted")
					   << " password." << LL_ENDL;
#endif
	mEntry.insert("password", encryptPassword(value));
}

const std::string LLSavedLoginEntry::decryptPassword(const LLSD& pwdata)
{
	std::string pw;

	if (pwdata.isBinary() &&
		pwdata.asBinary().size() == MD5HEX_STR_BYTES + 1)
	{
		LLSD::Binary buffer = pwdata.asBinary();

		LLXORCipher cipher(gMACAddress, MAC_ADDRESS_BYTES);
		cipher.decrypt(buffer.data(), MD5HEX_STR_BYTES);

		buffer[MD5HEX_STR_BYTES] = '\0';
		pw.assign((const char*)buffer.data());
		if (!LLStringOps::isHexString(pw))
		{
			pw.clear();	// Invalid data
		}
	}

	return pw;
}

const LLSD LLSavedLoginEntry::encryptPassword(const std::string& password)
{
	LLSD pwdata;

	if (password.size() == MD5HEX_STR_BYTES &&
		LLStringOps::isHexString(password))
	{
		LLSD::Binary buffer(MD5HEX_STR_BYTES + 1);
		LLStringUtil::copy((char*)buffer.data(), password.c_str(),
						   MD5HEX_STR_BYTES + 1);
		buffer[MD5HEX_STR_BYTES] = '\0';
		LLXORCipher cipher(gMACAddress, MAC_ADDRESS_BYTES);
		cipher.encrypt(buffer.data(), MD5HEX_STR_BYTES);
		pwdata.assign(buffer);
	}

	return pwdata;
}

//-----------------------------------------------------------------------------
// LLSavedLogins class
//-----------------------------------------------------------------------------

LLSavedLogins::LLSavedLogins()
{
}

LLSavedLogins::LLSavedLogins(const LLSD& history_data)
{
	if (!history_data.isArray())
	{
		throw std::invalid_argument("Invalid history data.");
	}
	for (LLSD::array_const_iterator it = history_data.beginArray();
		 it != history_data.endArray(); ++it)
	{
	  	// Put the last used grids first.
		if (!it->isUndefined())
		{
			mEntries.push_front(LLSavedLoginEntry(*it));
		}
	}
}

LLSD LLSavedLogins::asLLSD() const
{
	LLSD output;
	for (list_t::const_iterator it = mEntries.begin(), end = mEntries.end();
		 it != end; ++it)
	{
		output.insert(0, it->asLLSD());
	}
	return output;
}

void LLSavedLogins::addEntry(const LLSavedLoginEntry& entry)
{
	mEntries.emplace_back(entry);
}

void LLSavedLogins::deleteEntry(EGridInfo grid,
								const std::string& firstname,
								const std::string& lastname,
								const std::string& griduri)
{
	LLGridManager* gm = LLGridManager::getInstance();
	for (list_t::iterator it = mEntries.begin(); it != mEntries.end(); )
	{
		if (it->getFirstName() == firstname && it->getLastName() == lastname &&
			it->getGridName() == gm->getKnownGridLabel(grid) &&
			(grid != GRID_INFO_OTHER ||
			 it->getGridURI().asString() == griduri))
		{
			LL_DEBUGS("Login") << "Erasing entry for grid: "
							   << it->getGridName() << " - User: "
							   << firstname << " " << lastname << LL_ENDL;
			it = mEntries.erase(it);
		}
		else
		{
			++it;
		}
	}
}

LLSavedLogins LLSavedLogins::loadFile(const std::string& filepath)
{
	LLSavedLogins hist;
	LLSD data;

	llifstream file(filepath.c_str());
	if (file.is_open())
	{
		llinfos << "Loading login history file at " << filepath << llendl;
		LLSDSerialize::fromXML(data, file);
	}

	if (data.isUndefined())
	{
		llinfos << "Login History File \"" << filepath
				<< "\" is missing, ill-formed, or simply undefined; not loading the file."
				<< llendl;
	}
	else
	{
		try
		{
			hist = LLSavedLogins(data);
		}
		catch (std::invalid_argument& error)
		{
			llwarns << "Login History File \"" << filepath
					<< "\" is ill-formed (" << error.what()
					<< "); not loading the file." << llendl;
		}
	}

	return hist;
}

bool LLSavedLogins::saveFile(const LLSavedLogins& history,
							 const std::string& filepath)
{
	llofstream out(filepath.c_str());
	if (!out.is_open())
	{
		llwarns << "Unable to open \"" << filepath << "\" for output."
				<< llendl;
		return false;
	}

	LLSDSerialize::toPrettyXML(history.asLLSD(), out);
	out.close();

	return true;
}
