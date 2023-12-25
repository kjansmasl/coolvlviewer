/** 
 * @file llstartup.h
 * @brief Startup routines and logic declaration - Purely static class.
 *
 * $LicenseInfo:firstyear=2004&license=viewergpl$
 * 
 * Copyright (c) 2004-2009, Linden Research, Inc.
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

#ifndef LL_LLSTARTUP_H
#define LL_LLSTARTUP_H

#include "llerror.h"

class LLMessageSystem;
class LLSLURL;
class LLViewerTexture;

// Constants, variables and enumerations
extern const std::string SCREEN_HOME_FILENAME;
extern const std::string SCREEN_LAST_FILENAME;
extern const std::string SCREEN_LAST_BETA_FILENAME;

// Start location constants
enum EStartLocation
{
	START_LOCATION_ID_LAST,
	START_LOCATION_ID_HOME,
	START_LOCATION_ID_DIRECT,
	START_LOCATION_ID_PARCEL,
	START_LOCATION_ID_TELEHUB,
	START_LOCATION_ID_URL,
	START_LOCATION_ID_COUNT
};

typedef enum {
	STATE_FIRST,					// Initial startup
	STATE_BROWSER_INIT,             // Initialize web browser for login screen
	STATE_LOGIN_SHOW,				// Show login screen
	STATE_TPV_FIRST_USE,			// Show TPV agreement
	STATE_LOGIN_WAIT,				// Wait for user input at login screen
	STATE_LOGIN_CLEANUP,			// Get rid of login screen and start login
	STATE_UPDATE_CHECK,				// Wait for user at a dialog box (updates, term-of-service, etc)
	STATE_LOGIN_AUTH_INIT,			// Start login to SL servers
	STATE_XMLRPC_LOGIN,      		// XMLRPC login
	STATE_LOGIN_NO_DATA_YET,		// Waiting for authentication replies to start
	STATE_LOGIN_DOWNLOADING,		// Waiting for authentication replies to download
	STATE_LOGIN_PROCESS_RESPONSE,	// Check authentication reply
	STATE_WORLD_INIT,				// Start building the world
	STATE_MULTIMEDIA_INIT,			// Init the rest of multimedia library
	STATE_SEED_GRANTED_WAIT,		// Wait for seed cap grant
	STATE_SEED_CAP_GRANTED,			// Have seed cap grant 
	STATE_WORLD_WAIT,				// Waiting for simulator
	STATE_AGENT_SEND,				// Connect to a region
	STATE_AGENT_WAIT,				// Wait for region
	STATE_INVENTORY_SEND,			// Do inventory transfer
	STATE_MISC,						// Do more things (set bandwidth, start audio, save location, etc)
	STATE_PRECACHE,					// Wait a bit for textures to download
	STATE_WEARABLES_WAIT,			// Wait for clothing to download
	STATE_CLEANUP,					// Final cleanup
	STATE_STARTED					// Up and running in-world
} EStartupState;

class LLStartUp
{
	friend class LLLoginHandler;

	LLStartUp() = delete;
	~LLStartUp() = delete;

protected:
	LOG_CLASS(LLStartUp);

	// Get, base64-decode and decipher the user's password or MFA hash from
	// settings. Return the corresponding plain hash.
	static std::string getPasswordHashFromSettings();
	static std::string getMFAHashFromSettings();

	// Cipher, base64-encode and save in settings the user's password or MFA
	// hash for next login.
	static void savePasswordHashToSettings(std::string password);
	static void saveMFAHashToSettings(std::string mfa_hash);

public:
	static bool idleStartup();

	// Always use this to set sStartupState so changes are logged
	static void setStartupState(EStartupState state);

	LL_INLINE static bool isLoggedIn()
	{
		return sStartupState == STATE_STARTED;
	}

	LL_INLINE static EStartupState getStartupState()
	{
		return sStartupState;
	}

	LL_INLINE static std::string getStartupStateString()
	{
		return startupStateToString(sStartupState);
	}

	// Initializes LLViewerMedia multimedia engine.
	static void multimediaInit();

	// outfit_folder_name can be a folder anywhere in your inventory, 
	// but the name must be a case-sensitive exact match.
	// gender_name is either "male" or "female"
	static void loadInitialOutfit(const std::string& outfit_folder_name,
								  const std::string& gender_name);

	static S32 setStartSLURL(const LLSLURL& slurl); 
	LL_INLINE static LLSLURL& getStartSLURL()		{ return sStartSLURL; }

	// If we have a SLURL or sim string ("Ahern/123/45") that started the
	// viewer, dispatches it.
	static bool dispatchURL();

	// Initializes the SOCKS 5 proxy, if any
	static bool startLLProxy();

	static void refreshLoginPanel();

	static bool loginAlertDone(const LLSD&, const LLSD&);

	static void startAudioEngine();
	static void shutdownAudioEngine();

private:
	static void resetLogin();
	static bool loginShow(bool update_servers);
	static void initStartScreen(S32 location_id);
	static void setStartupStatus(F32 frac, const std::string& string,
								 const std::string& msg);
	static std::string startupStateToString(EStartupState state);
	static void updateTextureFetch();
	static void applyUdpBlacklist(const std::string& csv);
	static void loginCallback(S32 option, void*);
	static void useCircuitCallback(void**, S32 result);
	static bool loginAlertStatus(const LLSD&, const LLSD&);
	static bool callbackChooseGender(const LLSD& notification,
									 const LLSD& response);
	static void callbackCacheName(const LLUUID& id,
								  const std::string& fullname,
								  bool is_group);
	static void registerViewerCallbacks(LLMessageSystem* msg);

private:
	// Do not set directly, use LLStartup::setStartupState
	static EStartupState sStartupState;

	static std::string sInitialOutfit;
	static std::string sInitialOutfitGender;	// "male" or "female"

	static LLSLURL sLoginSLURL;
	static LLSLURL sStartSLURL;
};

// Exported globals
extern LLPointer<LLViewerTexture>	gStartTexture;
extern S32							gMaxAgentGroups;
extern bool							gAgentMovementCompleted;
extern std::string					gLoginFirstName;
extern std::string					gLoginLastName;

#endif // LL_LLSTARTUP_H
