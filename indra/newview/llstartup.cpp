/**
 * @file llstartup.cpp
 * @brief Startup routines. Purely static class.
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

#include "llviewerprecompiledheaders.h"

#include "llstartup.h"

#include "imageids.h"
#include "llapp.h"
#include "llaudioengine.h"
#if LL_FMOD
# include "llaudioengine_fmod.h"
#endif
#if LL_OPENAL
# include "llaudioengine_openal.h"
#endif
#include "llbase64.h"
#include "llcallbacklist.h"
#include "llcachename.h"
#include "llconsole.h"
#include "lldir.h"
#include "lleconomy.h"
#include "llerrorcontrol.h"
#include "llexperiencecache.h"
#include "llimagebmp.h"
#include "lllandmark.h"
#include "lllocaltextureobject.h"
#include "llmd5.h"
#include "llmemorystream.h"
#include "llmessageconfig.h"
#include "llnamebox.h"
#include "llnameeditor.h"
#include "llnamelistctrl.h"
#include "llnotifications.h"
#include "llprimitive.h"
#include "llproxy.h"
#include "llregionhandle.h"
#include "llsd.h"
#include "llsdserialize.h"
#include "llsdutil.h"
#include "llsdutil_math.h"
#include "llstreamingaudio.h"
#if 0
#include "llthreadpool.h"
#endif
#include "lltrans.h"
#include "llurlhistory.h"
#include "lluserauth.h"
#include "lluserrelations.h"
#include "llversionviewer.h"
#include "llvolumemessage.h"
#include "llxmlrpctransaction.h"
#include "llxorcipher.h"

#include "llagent.h"
#include "llagentpilot.h"
#include "llagentwearables.h"
#include "llappearancemgr.h"
#include "llappcorehttp.h"
#include "llappviewer.h"
#include "llavatarproperties.h"
#include "llavatartracker.h"
#include "llcommandhandler.h"
#include "lldebugview.h"
#include "lldrawable.h"
#include "llenvironment.h"
#include "lleventnotifier.h"
#include "llexperiencelog.h"
#include "llface.h"
#include "llfasttimerview.h"
#include "llfeaturemanager.h"
#include "llfirstuse.h"
#include "llfloateractivespeakers.h"
#include "llfloateravatarpicker.h"
#include "llfloaterbeacons.h"
#include "llfloatercamera.h"
#include "llfloaterchat.h"
#include "hbfloaterdebugtags.h"
#include "llfloaterinventory.h"
#include "llfloaterland.h"
#include "llfloaterminimap.h"
#include "llfloatermove.h"
#include "hbfloaterradar.h"
#include "hbfloatersearch.h"
#include "llfloaterstats.h"
#include "hbfloaterteleporthistory.h"
#include "llfloatertopobjects.h"
#include "llfloatertos.h"
#include "llfloaterworldmap.h"
#include "llgesturemgr.h"
#include "llgridmanager.h"
#include "llgroupmgr.h"
#include "llhudmanager.h"
#include "llinventorymodel.h"
#include "llinventorymodelfetch.h"
#include "llkeyboard.h"
#include "llpanellogin.h"
#include "llmarketplacefunctions.h"
#include "llmutelist.h"
#include "llpanelavatar.h"
#include "llpanelclassified.h"		// For LLClassifiedInfo
#include "llpaneldirbrowser.h"
#include "llpaneldirland.h"
#include "llpanelevent.h"
#include "llpanelgrouplandmoney.h"
#include "llpanelgroupnotices.h"
#include "llpipeline.h"
#include "llpreview.h"
#include "llpreviewscript.h"		// For LLPreviewScript::loadFunctions()
#include "llproductinforequest.h"
#include "llprogressview.h"			// gStartImageWidth and gStartImageHeight
//MK
#include "mkrlinterface.h"
//mk
#include "llselectmgr.h"
#include "llsky.h"
#include "llslurl.h"
#include "llstatusbar.h"			// For sendMoneyBalanceRequest()
#include "lltoolmgr.h"
#include "llurldispatcher.h"
#include "hbviewerautomation.h"
#include "llvieweraudio.h"
#include "llviewerassetstorage.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerdisplay.h"
#include "llviewergesture.h"
#include "llviewermedia.h"
#include "llviewermenu.h"
#include "llviewermessage.h"		// process_*(), invalid_message_callback()
#include "llviewerobjectlist.h"
#include "llviewerparcelmedia.h"
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"
#include "llviewershadermgr.h"
#include "llviewerstats.h"
#include "llviewertexturelist.h"
#include "llviewerthrottle.h"
#include "llviewerwindow.h"
#include "llvoavatarself.h"
#include "llvocache.h"
#include "llvoclouds.h"
#include "llvosky.h"				// For gSunTextureID and gMoonTextureID
#include "llvoiceclient.h"
#include "llweb.h"
#include "llworld.h"
#include "llworldmap.h"
#include "llxfermanager.h"

// Exported constants
const std::string SCREEN_HOME_FILENAME = "screen_home.bmp";
const std::string SCREEN_LAST_FILENAME = "screen_last.bmp";
const std::string SCREEN_LAST_BETA_FILENAME = "screen_last-beta.bmp";
constexpr S32 DEFAULT_MAX_AGENT_GROUPS = 25;
constexpr S32 OPENSIM_DEFAULT_MAX_AGENT_GROUPS = 100;

// Exported global
S32 gMaxAgentGroups = DEFAULT_MAX_AGENT_GROUPS;
LLPointer<LLViewerTexture> gStartTexture;
bool gAgentMovementCompleted = false;
std::string gLoginFirstName;
std::string gLoginLastName;

// Local globals
LLHost gAgentSimHost;
bool gGotUseCircuitCodeAck = false;
bool gUseCircuitCallbackCalled = false;

// Static variables
std::string LLStartUp::sInitialOutfit;
std::string LLStartUp::sInitialOutfitGender;	// "male" or "female"
EStartupState LLStartUp::sStartupState = STATE_FIRST;
LLSLURL LLStartUp::sLoginSLURL;
LLSLURL LLStartUp::sStartSLURL;

// Defined in llspatialpartition.cpp, used in lloctree.h
extern LLVector4a gOctreeMaxMag;

// Helper function
static std::string xml_escape_string(const std::string& in)
{
	std::ostringstream out;
	std::string::const_iterator it = in.begin();
	std::string::const_iterator end = in.end();
	for ( ; it != end; ++it)
	{
		switch (*it)
		{
			case '<':
				out << "&lt;";
				break;

			case '>':
				out << "&gt;";
				break;

			case '&':
				out << "&amp;";
				break;

			case '\'':
				out << "&apos;";
				break;

			case '"':
				out << "&quot;";
				break;

			case '\t':
			case '\n':
			case '\r':
				out << *it;
				break;

			default:
				if (*it >= 0 && *it < 20)
				{
					// Do not output control codes
					out << "?";
				}
				else
				{
					out << *it;
				}
		}
	}
	return out.str();
}

///////////////////////////////////////////////////////////////////////////////
// LLLoginHandler class
// Handles filling in the login panel information from a SLURL
///////////////////////////////////////////////////////////////////////////////

class LLLoginHandler final : public LLCommandHandler
{
 public:
	LLLoginHandler()
	:	LLCommandHandler("login", UNTRUSTED_ALLOW)
	{
	}

	bool handle(const LLSD&, const LLSD& query_map, LLMediaCtrl*) override;
};

// Must have instance to auto-register with LLCommandHandler
LLLoginHandler gLoginHandler;

//virtual
bool LLLoginHandler::handle(const LLSD&, const LLSD& query_map, LLMediaCtrl*)
{
	LL_DEBUGS("Login") << "Parsing: " << ll_pretty_print_sd(query_map)
					   << LL_ENDL;

	if (query_map.has("grid"))
	{
		LLGridManager* gm = LLGridManager::getInstance();
		gm->setGridChoice(query_map["grid"].asString());
	}

	std::string firstname = query_map["first_name"].asString();
	std::string lastname = query_map["last_name"].asString();
	std::string password = query_map["password"].asString();
	if (password.empty() && !firstname.empty() &&
		firstname == gLoginFirstName && lastname == gLoginLastName)
	{
		password = LLStartUp::getPasswordHashFromSettings();
	}

	std::string start_loc = query_map["location"].asString();
	if (start_loc == "specify")
	{
		LLStartUp::setStartSLURL(query_map["region"].asString());
	}
	else if (start_loc == "home")
	{
		gSavedSettings.setBool("LoginLastLocation", false);
		LLStartUp::setStartSLURL(LLSLURL(LLSLURL::SIM_LOCATION_HOME));
	}
	else if (start_loc == "last")
	{
		gSavedSettings.setBool("LoginLastLocation", true);
		LLStartUp::setStartSLURL(LLSLURL(LLSLURL::SIM_LOCATION_LAST));
	}

	if (LLStartUp::getStartupState() < STATE_LOGIN_CLEANUP)  // On splash page
	{
		if (!firstname.empty())
		{
			// Fill in the name, and maybe the password
			LL_DEBUGS("Login") << "Using login credentials: User: "
							   << firstname << " " << lastname
#if LL_DEBUG_LOGIN_PASSWORD
							   << " - Password hash: " << password
#endif
							   << LL_ENDL;
			LLPanelLogin::setFields(firstname, lastname, password);
		}
		LLPanelLogin::loadLoginPage();
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////
// LLStartUp class proper
///////////////////////////////////////////////////////////////////////////////

//static
void LLStartUp::applyUdpBlacklist(const std::string& csv)
{
	std::string::size_type start = 0;
	std::string::size_type comma = 0;
	do
	{
		comma = csv.find(",", start);
		if (comma == std::string::npos)
		{
			comma = csv.length();
		}
		std::string item(csv, start, comma-start);

		LL_DEBUGS("AppInit") << "udp_blacklist " << item << LL_ENDL;
		gMessageSystemp->banUdpMessage(item);

		start = comma + 1;
	}
	while (comma < csv.length());
}

//static
void LLStartUp::shutdownAudioEngine()
{
	if (gAudiop)
	{
		llinfos << "Deleting existing audio engine instance" << llendl;

		// Shut down the streaming audio sub-subsystem first, in case it relies
		// on not outliving the general audio subsystem.
		LLStreamingAudioInterface* sai = gAudiop->getStreamingAudioImpl();
		delete sai;
		gAudiop->setStreamingAudioImpl(NULL);

		// Shut down the audio subsystem
		gAudiop->shutdown();

		delete gAudiop;
		gAudiop = NULL;
	}
}

//static
void LLStartUp::startAudioEngine()
{
	shutdownAudioEngine();

	if (gSavedSettings.getBool("NoAudio")) return;

#if LL_FMOD
	if (!gAudiop && !gSavedSettings.getBool("AudioDisableFMOD"))
	{
# if LL_LINUX
		LLAudioEngine_FMOD::sNoALSA =
			gSavedSettings.getBool("FMODDisableALSA");
		LLAudioEngine_FMOD::sNoPulseAudio =
			gSavedSettings.getBool("FMODDisablePulseAudio");
# endif	// LL_LINUX
		bool use_profiler = gSavedSettings.getBool("FMODProfilerEnable");
		gAudiop = (LLAudioEngine*)new LLAudioEngine_FMOD(use_profiler);
	}
#endif	// LL_FMOD

#if LL_OPENAL
	if (!gAudiop && !gSavedSettings.getBool("AudioDisableOpenAL"))
	{
		gAudiop = (LLAudioEngine*)new LLAudioEngine_OpenAL();
	}
#endif

	if (gAudiop)
	{
#if LL_WINDOWS
		// FMOD Ex on Windows needs the window handle to stop playing audio
		// when window is minimized. JC
		void* window_handle = (HWND)gViewerWindowp->getPlatformWindow();
#else
		void* window_handle = NULL;
#endif
		bool init = gAudiop->init(window_handle);
		if (init)
		{
			LLViewerParcelMedia::registerStreamingAudioPlugin();
		}
		else
		{
			llwarns << "Unable to initialize audio engine" << llendl;
			delete gAudiop;
			gAudiop = NULL;
		}
	}

	if (gAudiop)
	{
		if (isLoggedIn())
		{
			setup_audio_listener();
		}
		llinfos << "Audio engine initialized." << llendl;
	}
	else
	{
		llwarns << "Failed to create an appropriate audio engine" << llendl;
	}
}

static void process_messages()
{
#if LL_USE_FIBER_AWARE_MUTEX
	LockMessageChecker lmc(gMessageSystemp);
	while (lmc.checkAllMessages(gFrameCount, gServicePumpIOp)) ;
	lmc.processAcks();
#else
	LLMessageSystem* msg = gMessageSystemp;
	while (msg->checkAllMessages(gFrameCount, gServicePumpIOp)) ;
	msg->processAcks();
#endif
}

static void call_force_quit()
{
	gAppViewerp->forceQuit();
}

// Returns false to skip other idle processing. Should only return true when
// all initializations are done.
//static
bool LLStartUp::idleStartup()
{
	const F32 precaching_delay = gSavedSettings.getF32("PrecachingDelay");
	constexpr F32 TIMEOUT_SECONDS = 5.f;
	constexpr S32 MAX_TIMEOUT_COUNT = 3;
	constexpr F32 STATE_AGENT_WAIT_TIMEOUT = 240.f;	// seconds
	constexpr U32 MAX_SEED_CAP_ATTEMPTS_BEFORE_LOGIN = 3;
	static LLTimer timeout;

	// Until this is encapsulated, this little hack for the auth/transform loop
	// will do.
	static F32 progress = 0.1f;

	static std::string auth_method, auth_desc, auth_message, password;
	static std::vector<const char*> requested_options;

	static U64 first_sim_handle = 0;
	static LLHost first_sim;
	static std::string first_sim_seed_cap;

	// Default for when no space server:
	static LLVector3 agent_start_position_region(10.f, 10.f, 10.f);
	static LLVector3 agent_start_look_at(1.0f, 0.f, 0.f);
	static std::string agent_start_location = "safe";

	// Last location by default
	static S32 agent_location_id = START_LOCATION_ID_LAST;

	static bool show_connect_box = true;

	static U32 first_region_size = (U32)REGION_WIDTH_METERS;

	static bool first_grid_login = false;

	// *HACK: these are things from the main loop that usually are not done
	// until initialization is complete, but need to be done here for things
	// to work.
	gIdleCallbacks.callFunctions();
	gViewerWindowp->handlePerFrameHover();
	LLMortician::updateClass();

	// Note: removing this line will cause incorrect button size in the login
	// screen. - Bao.
	gTextureList.updateImages(0.01f);

	LLGridManager* gm = LLGridManager::getInstance();

	if (getStartupState() == STATE_FIRST)
	{
		gViewerWindowp->showCursor();
		gWindowp->setCursor(UI_CURSOR_WAIT);

#if LL_LINUX
		// *HACK: to compute window borders offsets. HB
		gWindowp->calculateBordersOffsets();
#endif

		// Initialize stuff that does not need data from simulators

		S32 last_feature_version = gSavedSettings.getS32("LastFeatureVersion");
		if (gFeatureManager.isSafe())
		{
			gNotifications.add("DisplaySetToSafe");
		}
		else if (last_feature_version < gFeatureManager.getVersion() &&
				 last_feature_version != 0)
		{
			gNotifications.add("DisplaySetToRecommended");
		}
		else if (!gViewerWindowp->getInitAlert().empty())
		{
			gNotifications.add(gViewerWindowp->getInitAlert());
		}

		// Init the SOCKS 5 proxy if the user has configured one. We need to do
		// this early in case the user is using SOCKS for HTTP so we get the
		// login screen and HTTP tables via SOCKS.
		startLLProxy();

		gSavedSettings.setS32("LastFeatureVersion",
							  gFeatureManager.getVersion());

		std::string xml_file = LLUI::locateSkin("xui_version.xml");
		LLXMLNodePtr root;
		bool xml_ok = false;
		if (LLXMLNode::parseFile(xml_file, root, NULL))
		{
			if ((root->hasName("xui_version")))
			{
				std::string value = root->getValue();
				F32 version = 0.f;
				LLStringUtil::convertToF32(value, version);
				if (version >= 1.f)
				{
					xml_ok = true;
				}
			}
		}
		if (!xml_ok)
		{
			// If XML is bad, there is a large risk that notifications.xml
			// is ALSO bad. If that is so, then we will get a fatal error on
			// attempting to load it, which will display a non-translatable
			// error message that says so. Otherwise, we will display a
			// reasonable error message that IS translatable.
			gAppViewerp->earlyExit("BadInstallation");
		}

		// Statistics stuff

		// Load the throttle settings
		gViewerThrottle.load();

		// Initialize messaging system

		LL_DEBUGS("AppInit") << "Initializing messaging system..." << LL_ENDL;

		std::string message_template_path =
			gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS,
										  "message_template.msg");
		LLFILE* found_template = LLFile::open(message_template_path, "r");
		if (found_template)
		{
			LLFile::close(found_template);

			U32 port = gSavedSettings.getU32("UserConnectionPort");
			// if nothing specified on command line (-port)
			if (port == NET_USE_OS_ASSIGNED_PORT &&
				gSavedSettings.getBool("ConnectionPortEnabled"))
			{
				port = gSavedSettings.getU32("ConnectionPort");
			}

			// *TODO: parameterize
			constexpr F32 HEARTBEAT_INTERVAL = 5;
			constexpr F32 TIMEOUT = 100;
			const LLUseCircuitCodeResponder* responder = NULL;
			if (!start_messaging_system(message_template_path, port,
										LL_VERSION_MAJOR, LL_VERSION_MINOR,
										LL_VERSION_BRANCH, responder,
										HEARTBEAT_INTERVAL, TIMEOUT))
			{
				std::string diagnostic =
					llformat(" Error: %d", gMessageSystemp->getErrorCode());
				llwarns << diagnostic << llendl;
				gAppViewerp->earlyExit("LoginFailedNoNetwork",
									   LLSD().with("DIAGNOSTIC", diagnostic));
			}

			LLMessageConfig::initClass("viewer",
									   gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS,
																	  ""));
		}
		else
		{
			gAppViewerp->earlyExit("MessageTemplateNotFound",
								   LLSD().with("PATH", message_template_path));
		}

		LLMessageSystem* msg = gMessageSystemp;
		if (msg && msg->isOK())
		{
			// Initialize all of the callbacks in case of bad message
			// system data
			msg->setExceptionFunc(MX_UNREGISTERED_MESSAGE,
								  invalid_message_callback,
								  NULL);
			msg->setExceptionFunc(MX_PACKET_TOO_SHORT,
								  invalid_message_callback,
								  NULL);

#if 0		// Running off end of a packet is now valid in the case when a
			// reader has a newer message template than the sender
			msg->setExceptionFunc(MX_RAN_OFF_END_OF_PACKET,
								  invalid_message_callback, NULL);
#else
			msg->setExceptionFunc(MX_WROTE_PAST_BUFFER_SIZE,
								  invalid_message_callback, NULL);
#endif

			if (gSavedSettings.getBool("LogMessages"))
			{
				LL_DEBUGS("AppInit") << "Message logging activated !"
									 << LL_ENDL;
				msg->startLogging();
			}

			// Start the xfer system.
			gXferManagerp = new LLXferManager();
			F32 xfer_throttle_bps = gSavedSettings.getF32("XferThrottle");
			if (xfer_throttle_bps >= 1.f)
			{
				gXferManagerp->setUseAckThrottling(true);
				gXferManagerp->setAckThrottleBPS(xfer_throttle_bps);
			}
			else
			{
				// By default, choke the downloads a lot...
				constexpr S32 VIEWER_MAX_XFER = 3;
				gXferManagerp->setMaxIncomingXfers(VIEWER_MAX_XFER);
			}
			gAssetStoragep = new LLViewerAssetStorage(msg, gXferManagerp);

			F32 bw = gSavedSettings.getF32("InBandwidth");
			if (bw >= 1.f)
			{
				llinfos << "Setting packetring incoming bandwidth to " << bw
						<< " bps" << llendl;
				msg->mPacketRing.setUseInThrottle(true);
				msg->mPacketRing.setInBandwidth(bw);
			}
			bw = gSavedSettings.getF32("OutBandwidth");
			if (bw >= 1.f)
			{
				llinfos << "Setting packetring outgoing bandwidth to " << bw
						<< " bps" << llendl;
				msg->mPacketRing.setUseOutThrottle(true);
				msg->mPacketRing.setOutBandwidth(bw);
			}

			// Now that gMessageSystemp is up, we can initialize the mute list:
			LLMuteList::initClass();
		}

		llinfos << "Message system initialized." << llendl;

		// Init audio, which may be needed for prefs dialog or audio cues in
		// connection UI.
		startAudioEngine();

		// Initialize the world class before we need it
		gWorld.initClass();

		// Log on to system
		if (gSavedSettings.getLLSD("UserLoginInfo").size() == 3)
		{
			LLSD cmd_line_login = gSavedSettings.getLLSD("UserLoginInfo");
			gLoginFirstName = cmd_line_login[0].asString();
			gLoginLastName = cmd_line_login[1].asString();

			LLMD5 pass((unsigned char*)cmd_line_login[2].asString().c_str());
			char md5pass[MD5HEX_STR_BYTES + 1];
			pass.hex_digest(md5pass);
			password = md5pass;

			show_connect_box = false;
			gSavedSettings.setBool("AutoLogin", true);
			llinfos << "Login credentials obtained from command line"
					<< llendl;
		}
		else
		{
			gLoginFirstName = gSavedSettings.getString("FirstName");
			gLoginLastName = gSavedSettings.getString("LastName");
			gm->setGridChoice(gSavedSettings.getS32("ServerChoice"));
			password = getPasswordHashFromSettings();
			show_connect_box = !gSavedSettings.getBool("AutoLogin");
			llinfos << "Login credentials obtained from saved settings"
					<< llendl;
		}
		LL_DEBUGS("Login") << "Using login credentials: User: "
						   << gLoginFirstName << " " << gLoginLastName
#if LL_DEBUG_LOGIN_PASSWORD
						   << " - Password hash: " << password
#endif
						   << LL_ENDL;

		// Fall through immediately to the next state
		setStartupState(STATE_BROWSER_INIT);
	}

	if (getStartupState() == STATE_BROWSER_INIT)
	{
		std::string msg = LLTrans::getString("LoginInitializingBrowser");
		setStartupStatus(0.03f, msg, gAgent.mMOTD);
		display_startup();
		// Fall through immediately to the next state
		setStartupState(STATE_LOGIN_SHOW);
	}

	if (getStartupState() == STATE_LOGIN_SHOW)
	{
		gWindowp->setCursor(UI_CURSOR_ARROW);

		// Load URL History File for saved user. Needs to happen *before* login
		// panel is displayed.
		// Note: it only loads them if it can figure out the saved username.
		if (!gLoginFirstName.empty() && !gLoginLastName.empty())
		{
			gDirUtilp->setLindenUserDir(gm->getGridLabel(), gLoginFirstName,
										gLoginLastName);
			LLFile::mkdir(gDirUtilp->getLindenUserDir());
			LLURLHistory::loadFile("url_history.xml");
		}

		// Initialize all our tools. Must be done after saved settings loaded.
		gToolMgr.initTools();

		// Quickly get something onscreen to look at.
		gViewerWindowp->initWorldUI();

		if (show_connect_box)
		{
			// Make sure the progress dialog does not hide things
			gViewerWindowp->setShowProgress(false);

			// Show the login dialog.
			static bool first_attempt = true;
			bool have_loginuri = loginShow(first_attempt);
			if (first_attempt)
			{
				// Do not modify login credentials on subsequent attempts
				first_attempt = false;
				llinfos << "Setting default login credentials" << llendl;
				// Connect dialog is already shown, so fill in the names
				if (have_loginuri)
				{
			  		// We have either a login SLURL or a --loginuri on
					// command line that was not recognized as a known
					// login entry. Select it.
					LLPanelLogin::selectFirstElement();
				}
				else
				{
					LLPanelLogin::setFields(gLoginFirstName, gLoginLastName,
											password);
					LLPanelLogin::giveFocus();
				}
			}

			gSavedSettings.setBool("FirstRunThisInstall", false);

			if (gSavedSettings.getBool("FirstRunTPV"))
			{
				// Show the TPV agreement
				setStartupState(STATE_TPV_FIRST_USE);
			}
			else
			{
				// Wait for user input
				setStartupState(STATE_LOGIN_WAIT);
			}
		}
		else
		{
			// Skip directly to message template verification
			setStartupState(STATE_LOGIN_CLEANUP);
		}

		// If we got a secondlife:///app/login SLURL, dispatch it now
		if (sLoginSLURL.isValid())
		{
			LLMediaCtrl* web = NULL;
			LLURLDispatcher::dispatch(sLoginSLURL.getSLURLString(), "clicked",
									  web, false);
		}

		gViewerWindowp->setNormalControlsVisible(false);
		gLoginMenuBarViewp->setVisible(true);
		gLoginMenuBarViewp->setEnabled(true);

		// Push our window frontmost
		gWindowp->show();
		display_startup();

		// DEV-16927. The following code removes errant keystrokes that happen
		// while the window is being first made visible.
#ifdef _WIN32
		MSG msg;
		// All hWnds owned by this thread
		while (PeekMessage(&msg, NULL, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE));
#endif
		timeout.reset();
		return false;
	}

	if (getStartupState() == STATE_TPV_FIRST_USE)
	{
		setStartupState(STATE_UPDATE_CHECK);
		LLFloaterTOS::show(LLFloaterTOS::TOS_FIRST_TPV_USE);
		gFrameSleepTime = 10;	// Do not hog the CPU
		return false;
	}

	if (getStartupState() == STATE_LOGIN_WAIT)
	{
		// Do not do anything. Wait for the login view to call the
		// loginCallback(), which will push us to the next state.
		gFrameSleepTime = 10;	// Do not hog the CPU
		return false;
	}

	if (getStartupState() == STATE_LOGIN_CLEANUP)
	{
		// Post login screen, we should see if any settings have changed that
		// may require us to either start/stop or change the socks proxy.
		// As various communications past this point may require the proxy to
		// be up.
		gFrameSleepTime = 1;
		if (!startLLProxy())
		{
			// Proxy start up failed, we should now bail the state machine
			// startLLProxy() will have reported an error to the user already,
			// so we just go back to the login screen. The user could then
			// change the preferences to fix the issue.
			setStartupState(STATE_LOGIN_SHOW);
			return false;
		}

		if (show_connect_box)
		{
			// Load all the name information out of the login view
			LLPanelLogin::getFields(gLoginFirstName, gLoginLastName, password);

			// *HACK: try to make not jump on login
			if (gKeyboardp)
			{
				gKeyboardp->resetKeys();
			}
		}

		if (!gLoginFirstName.empty() && !gLoginLastName.empty())
		{
			llinfos << "Attempting login as: " << gLoginFirstName << " "
					<< gLoginLastName << llendl;
			gDebugInfo["LoginName"] = gLoginFirstName + " " + gLoginLastName;
		}

		// Create necessary directories. *FIXME: these mkdir should error check
		const std::string grid_label = gm->getGridLabel();
		gDirUtilp->setLindenUserDir(grid_label, gLoginFirstName,
									gLoginLastName);
		LLFile::mkdir(gDirUtilp->getLindenUserDir());

		// Set PerAccountSettingsFile to the default value.
		std::string fname = gAppViewerp->getSettingsFilename("Account",
															 "PerAccount");
		fname = gDirUtilp->getExpandedFilename(LL_PATH_PER_ACCOUNT, fname);
		gSavedSettings.setString("PerAccountSettingsFile", fname);

		// Overwrite default user settings with user settings
		gAppViewerp->loadSettingsFromDirectory("Account");

		// Need to set the LastLogoff time here if we don't have one.
		// LastLogoff is used for "Recent Items" calculation and startup time
		// is close enough if we don't have a real value.
		if (gSavedPerAccountSettings.getU32("LastLogoff") == 0)
		{
			first_grid_login = true;
			gSavedPerAccountSettings.setU32("LastLogoff", time_corrected());
		}

		// Recover RestrainedLove's per-account settings.
		RLInterface::usePerAccountSettings();

		// Default the path if one is not set.
		std::string im_logs_path =
			gSavedPerAccountSettings.getString("InstantMessageLogPath");
		if (im_logs_path.empty())
		{
			gDirUtilp->setChatLogsDir(gDirUtilp->getOSUserAppDir());
			gSavedPerAccountSettings.setString("InstantMessageLogPath",
											   gDirUtilp->getChatLogsDir());
		}
		else
		{
			gDirUtilp->setChatLogsDir(im_logs_path);
		}

		gDirUtilp->setPerAccountChatLogsDir(gm->getGridLabel(),
											gLoginFirstName, gLoginLastName);

		LLFile::mkdir(gDirUtilp->getChatLogsDir());
		LLFile::mkdir(gDirUtilp->getPerAccountChatLogsDir());

		// Good as place as any to create user windlight directories
		std::string wl_path =
			gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS,
										   "windlight", "");
		LLFile::mkdir(wl_path.c_str());

		std::string wl_skies_path =
			gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS,
										   "windlight", "skies", "");
		LLFile::mkdir(wl_skies_path.c_str());

		std::string wl_water_path =
			gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS,
										   "windlight", "water", "");
		LLFile::mkdir(wl_water_path.c_str());

		std::string wl_days_path =
			gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS,
										   "windlight", "days", "");
		LLFile::mkdir(wl_days_path.c_str());

		if (show_connect_box)
		{
			LLPanelLogin::hide();
		}

		// Load URL History File
		LLURLHistory::loadFile("url_history.xml");

		//-------------------------------------------------
		// Handle startup progress screen
		//-------------------------------------------------

		// On startup the user can request to go to their home, their last
		// location, or some URL "--url //sim/x/y[/z]". All accounts have both
		// a home and a last location, and we do not support more locations
		// than that. Choose the appropriate one. JC
		switch (getStartSLURL().getType())
		{
			case LLSLURL::LOCATION:
				agent_location_id = START_LOCATION_ID_URL;
				break;

			case LLSLURL::LAST_LOCATION:
				agent_location_id = START_LOCATION_ID_LAST;
				break;

			case LLSLURL::HOME_LOCATION:
				agent_location_id = START_LOCATION_ID_HOME;
				break;

			default:
				if (gSavedSettings.getBool("LoginLastLocation"))
				{
					agent_location_id = START_LOCATION_ID_LAST;
					setStartSLURL(LLSLURL(LLSLURL::SIM_LOCATION_LAST));
				}
				else
				{
					agent_location_id = START_LOCATION_ID_HOME;
					setStartSLURL(LLSLURL(LLSLURL::SIM_LOCATION_HOME));
				}
		}
//MK
		if (gRLenabled &&
			!gSavedPerAccountSettings.getBool("RestrainedLoveTPOK"))
		{
			gSavedSettings.setBool("LoginLastLocation", true);
			// Always last location (actually ignore list)
			agent_location_id = START_LOCATION_ID_LAST;
			setStartSLURL(LLSLURL(LLSLURL::SIM_LOCATION_LAST));
		}
//mk
		gWindowp->setCursor(UI_CURSOR_WAIT);

		initStartScreen(agent_location_id);

		// Display the startup progress bar.
		gViewerWindowp->setShowProgress(true);
		// *TODO: Translate
		const std::string label = "Quit";
		gViewerWindowp->setProgressCancelButtonVisible(true, label);

		// Skipping over STATE_UPDATE_CHECK because that just waits for input
		setStartupState(STATE_LOGIN_AUTH_INIT);

		return false;
	}

	if (getStartupState() == STATE_UPDATE_CHECK)
	{
		// Wait for user to give input via dialog box
		gFrameSleepTime = 10;	// Do not hog the CPU
		return false;
	}

	if (getStartupState() == STATE_LOGIN_AUTH_INIT)
	{
		gFrameSleepTime = 1;
		gDebugInfo["GridName"] = gm->getGridLabel();

		requested_options.clear();
		requested_options.emplace_back("inventory-root");
		requested_options.emplace_back("inventory-skeleton");
		requested_options.emplace_back("inventory-lib-root");
		requested_options.emplace_back("inventory-lib-owner");
		requested_options.emplace_back("inventory-skel-lib");
#if 0	// Never used but always kept commented out in LL's sources... HB
		requested_options.emplace_back("inventory-skel-targets");
		requested_options.emplace_back("inventory-targets");
		requested_options.emplace_back("inventory-meat");
		requested_options.emplace_back("inventory-meat-lib");
#endif
		requested_options.emplace_back("agent_appearance_service");
		requested_options.emplace_back("initial-outfit");
		requested_options.emplace_back("gestures");
		requested_options.emplace_back("event_categories");
		requested_options.emplace_back("event_notifications");
		requested_options.emplace_back("classified_categories");
		requested_options.emplace_back("adult_compliant");
		requested_options.emplace_back("buddy-list");
		requested_options.emplace_back("ui-config");
		requested_options.emplace_back("max_groups");			// OpenSim
		requested_options.emplace_back("max-agent-groups");		// SL
		requested_options.emplace_back("map-server-url");
		requested_options.emplace_back("search-server-url");	// OpenSim
		requested_options.emplace_back("login-flags");
		requested_options.emplace_back("global-textures");
		if (gSavedSettings.getBool("ConnectAsGod"))
		{
			requested_options.emplace_back("god-connect");
		}
		// Hopefully, LL will, some time, implement this suggestion of mine to
		// restore the compatibility with old SL clients missing LLSD array
		// support in XML RPC replies... HB
		requested_options.emplace_back("account_level_benefits");

		auth_method = "login_to_simulator";

		LLStringUtil::format_map_t args;
		args["[APP_NAME]"] = gSecondLife;
		auth_desc = LLTrans::getString("LoginInProgressWait", args);

		setStartupState(STATE_XMLRPC_LOGIN);
	}

	if (getStartupState() == STATE_XMLRPC_LOGIN)
	{
		gFrameSleepTime = 1;
		progress += 0.02f;
		display_startup();

		std::stringstream start;
//MK
		if (gRLenabled &&
			!gSavedPerAccountSettings.getBool("RestrainedLoveTPOK"))
		{
			setStartSLURL(LLSLURL(LLSLURL::SIM_LOCATION_LAST));
		}
//mk
		LLSLURL start_slurl = getStartSLURL();
		LLSLURL::SLURL_TYPE start_slurl_type = start_slurl.getType();
		switch (start_slurl_type)
		{
			case LLSLURL::LOCATION:
			{
				// A startup URL was specified
				std::stringstream unescaped_start;
				unescaped_start << "uri:"
								<< start_slurl.getRegion() << "&"
								<< start_slurl.getPosition().mV[VX] << "&"
								<< start_slurl.getPosition().mV[VY] << "&"
								<< start_slurl.getPosition().mV[VZ];
				start << xml_escape_string(unescaped_start.str());
				break;
			}

			case LLSLURL::HOME_LOCATION:
				start << "home";
				gSavedSettings.setBool("LoginLastLocation", false);
				break;

			case LLSLURL::LAST_LOCATION:
				start << "last";
				gSavedSettings.setBool("LoginLastLocation", true);
				break;

			default:
				break;
		}

		std::string grid_uri = gm->getGridURI();
		llinfos << "Authenticating with " << grid_uri << llendl;

		// Determine whether we are connecting to SL or not
		gm->setIsInSecondlife();

		// Set some URLs for LLXMLRPCTransaction, part of the llmessage library
		bool use_mfa;
		if (gIsInSecondLife)
		{
			// MFA support is required now in SL, when the user configured
			// their account to use it. HB
			use_mfa = true;
			LLXMLRPCTransaction::setSupportURL(SUPPORT_URL);
			LLXMLRPCTransaction::setWebsiteURL(SL_GRID_STATUS_URL);
		}
		else
		{
			use_mfa = gSavedSettings.getBool("UseMFAinOS");
			std::string url = gm->getSupportURL();
			if (url.empty())
			{
				url = gm->getWebsiteURL();
			}
			LLXMLRPCTransaction::setSupportURL(url);
			LLXMLRPCTransaction::setWebsiteURL(gm->getWebsiteURL());
		}

		std::string mfa_hash, mfa_token;
		if (use_mfa)
		{
			mfa_hash = getMFAHashFromSettings();
			mfa_token = LLPanelLogin::getToken();
#if LL_DEBUG_LOGIN_PASSWORD
			LL_DEBUGS("Login") << "MFA hash: " << mfa_hash << " - MFA token: "
							   << mfa_token << LL_ENDL;
#endif
		}
		else
		{
			// Clear any remembered MFA hash
			gSavedPerAccountSettings.setString("MFAHash", "");
		}
		gUserAuth.setMFA(use_mfa, mfa_hash, mfa_token);

		gUserAuth.authenticate(grid_uri, auth_method, gLoginFirstName,
							   gLoginLastName, password, start.str(),
							   true, gAcceptTOS, gAcceptCriticalMessage,
							   gLastExecEvent, requested_options);

		// Reset globals
		gAcceptTOS = false;
		gAcceptCriticalMessage = false;

		// Set the flag for marking wearable textures as no-delete in OpenSim.
		LLLocalTextureObject::sMarkNoDelete = !gIsInSecondLife;
		// Adjust the prim parameters limits according to the grid's.
		LLPrimitive::setLimits(gIsInSecondLife);
		// In OpenSim, ignore bad ratio in volume params
		LLVolumeMessage::sIgnoreBadRatio = !gIsInSecondLife;
		// In OpenSim, use larger max mag for octrees
		if (!gIsInSecondLife)
		{
			gOctreeMaxMag.splat(4096.f * 4096.f);
		}

		// Load script functions symbols common to all grids
		LLPreviewScript::loadFunctions("lsl_functions_sl.xml");
		if (!gIsInSecondLife)
		{
			// Load script functions symbols specific to OpenSim/Aurora
			LLPreviewScript::loadFunctions("lsl_functions_os.xml");
			// Add a 60s timeout for untrusted messages in OpenSim
			gMessageSystemp->setHttpOptionsWithTimeout(60);
		}

		// Adjust HTTP pipelining if needed
		gAppViewerp->getAppCoreHttp().refreshSettings();

		// Allow face wrinkles in OpenSim, i.e. where we can bake them...
		LLTexLayerSet::sAllowFaceWrinkles = !gIsInSecondLife;
		// Allow large texture bakes in OpenSim grids configured for it.
		if (gIsInSecondLife)
		{
			LLControlVariable* controlp =
				gSavedPerAccountSettings.getControl("OSUseLargeAvatarBakes");
			if (controlp)
			{
				controlp->setHiddenFromUser(true);
			}
		}
		else
		{
			LLTexLayerSetInfo::sUseLargeBakes =
				gSavedPerAccountSettings.getBool("OSUseLargeAvatarBakes");
		}

		// Initialize the object cache now that we know which grid we are
		// connected to
		U32 max_size = gSavedSettings.getU32("CacheNumberOfRegionsForObjects");
		LLVOCache::getInstance()->initCache(LL_PATH_CACHE, max_size);

		setStartupState(STATE_LOGIN_NO_DATA_YET);

		return false;
	}

	if (getStartupState() == STATE_LOGIN_NO_DATA_YET)
	{
		LL_DEBUGS_ONCE("AppInit") << "STATE_LOGIN_NO_DATA_YET" << LL_ENDL;
		// If we get here we have gotten past the potential stall in curl, so
		// take "may appear frozen" out of progress bar. JC
		auth_desc = LLTrans::getString("LoginInProgress");
		setStartupStatus(progress, auth_desc, auth_message);
		// Process messages to keep from dropping circuit.
		process_messages();
		LLUserAuth::UserAuthcode error = gUserAuth.authResponse();
		if (LLUserAuth::E_NO_RESPONSE_YET == error)
		{
			LL_DEBUGS_ONCE("AppInit") << "waiting..." << LL_ENDL;
			gFrameSleepTime = 10;	// Do not hog the CPU
			return false;
		}
		gFrameSleepTime = 1;
		setStartupState(STATE_LOGIN_DOWNLOADING);
		progress += 0.01f;
		setStartupStatus(progress, auth_desc, auth_message);
		return false;
	}

	if (getStartupState() == STATE_LOGIN_DOWNLOADING)
	{
		// Process messages to keep from dropping circuit.
		process_messages();
		LLUserAuth::UserAuthcode error = gUserAuth.authResponse();
		if (LLUserAuth::E_DOWNLOADING == error)
		{
			LL_DEBUGS("AppInit") << "Downloading..." << LL_ENDL;
			gFrameSleepTime = 10;	// Do not hog the CPU
			return false;
		}
		gFrameSleepTime = 1;
		setStartupState(STATE_LOGIN_PROCESS_RESPONSE);
		progress += 0.01f;
		setStartupStatus(progress,
						 LLTrans::getString("LoginProcessingResponse"),
						 auth_message);
		return false;
	}

	if (getStartupState() == STATE_LOGIN_PROCESS_RESPONSE)
	{
		gFrameSleepTime = 1;
		std::ostringstream emsg;

		bool notify_user = true;	// Notify when login error happens
		bool quit = false;
		bool successful_login = false;

		// Reset globals
		gAcceptTOS = false;
		gAcceptCriticalMessage = false;

		std::string login_response, reason_response, message_response;
		LLUserAuth::UserAuthcode error = gUserAuth.authResponse();

		switch (error)
		{
		case LLUserAuth::E_OK:
			login_response = gUserAuth.getResponseStr("login");
			if (login_response == "true")
			{
				// Yay, login !
				successful_login = true;
			}
			else if (login_response == "indeterminate")
			{
				llinfos << "Indeterminate login..." << llendl;
				gm->setGridURI(gUserAuth.getResponseStr("next_url"));

				auth_method = gUserAuth.getResponseStr("next_method");
				auth_message = gUserAuth.getResponseStr("message");
				if (auth_method.substr(0, 5) == "login")
				{
					auth_desc.assign(LLTrans::getString("LoginAuthenticating"));
				}
				else
				{
					auth_desc.assign(LLTrans::getString("LoginMaintenance"));
				}
				setStartupState(STATE_XMLRPC_LOGIN);

				return false;
			}
			else
			{
				emsg << "Login failed.\n";
				reason_response = gUserAuth.getResponseStr("reason");
				message_response = gUserAuth.getResponseStr("message");
				if (!message_response.empty())
				{
					// *TODO: fix translation for strings returned during
					// login. We need a generic table for translations.
					std::string big_reason =
						LLAgent::sTeleportErrorMessages[message_response];
					if (big_reason.empty())
					{
						emsg << message_response;
					}
					else
					{
						emsg << big_reason;
					}
				}
				if (reason_response == "tos")
				{
					if (show_connect_box)
					{
						LL_DEBUGS("AppInit") << "Need tos agreement"
											 << LL_ENDL;
						setStartupState(STATE_UPDATE_CHECK);
						LLFloaterTOS::show(LLFloaterTOS::TOS_TOS,
										   message_response);
						gFrameSleepTime = 10;	// Do not hog the CPU
						return false;
					}
					else
					{
						quit = true;
					}
				}
				else if (reason_response == "critical")
				{
					if (show_connect_box)
					{
						LL_DEBUGS("AppInit") << "Need critical message"
											 << LL_ENDL;
						setStartupState(STATE_UPDATE_CHECK);
						LLFloaterTOS::show(LLFloaterTOS::TOS_CRITICAL_MESSAGE,
										   message_response);
						return false;
					}
					else
					{
						quit = true;
					}
				}
				else if (reason_response == "key")
				{
					// Could not login because user/password is wrong.
					password.clear();	// Clear the password
				}
				else if (reason_response == "mfa_challenge")
				{
					// Login failed because the MFA hash is wrong or missing...
					// A new MFA challenge is being performed on a third party
					// device or web site (or hopefully soon, via email), and
					// the user will get a token that they will need to provide
					// us with: enable the MFA token entry on the login screen,
					// and inform the user that they must fill it up before
					// they can try and log in again. HB
					LLPanelLogin::showTokenInputLine(true);
					notify_user = false;	// We use our own notification...
					gNotifications.add("MFAChallengeRequired");
					// We also force-enable the MFA usage variable; if we got
					// this login failure response from the login server, then
					// MFA support is/became required on this grid ! HB
					if (!gIsInSecondLife)	// Always required in SL already
					{
						gSavedSettings.setBool("UseMFAinOS", true);
					}
				}
				else if (reason_response == "update")
				{
					auth_message = gUserAuth.getResponseStr("message");
					LLSD args;
					args["MESSAGE"] = "(" + auth_message + ")";
					gNotifications.add("NeedUpdate", args);
					setStartupState(STATE_UPDATE_CHECK);
					return false;
				}
			}
			break;
		case LLUserAuth::E_COULDNT_RESOLVE_HOST:
		case LLUserAuth::E_SSL_PEER_CERTIFICATE:
		case LLUserAuth::E_UNHANDLED_ERROR:
		case LLUserAuth::E_SSL_CACERT:
		case LLUserAuth::E_SSL_CONNECT_ERROR:
		default:
			emsg << "Unable to connect to the grid.\n";
			emsg << gUserAuth.errorMessage();
		}

		if (quit)
		{
			gUserAuth.reset();
			gAppViewerp->forceQuit();
			return false;
		}

		// XML-RPC successful login
		if (successful_login)
		{
			std::string text = gUserAuth.getResponseStr("udp_blacklist");
			if (!text.empty())
			{
				applyUdpBlacklist(text);
			}

			// Get "agent benefits" stuff, if present.
			const LLSD& benefits =
				gUserAuth.getResponse("account_level_benefits");
			if (benefits.isDefined())
			{
				std::string account_type = "Base";
				const LLSD& account = gUserAuth.getResponse("account_type");
				if (account.isDefined())
				{
					account_type = account.asString();
				}
				LLEconomy::getInstance()->setBenefits(benefits, account_type);
				update_upload_costs_in_menus();
			}

			// Unpack login data needed by the application
			text = gUserAuth.getResponseStr("agent_id");
			if (text.empty())
			{
				emsg << "Login failed.\nMissing agent Id !";
			}
			else
			{
				gAgentID.set(text);
				gDebugInfo["AgentID"] = text;
			}

			text = gUserAuth.getResponseStr("session_id");
			if (text.empty())
			{
				if (gAgentID.notNull())
				{
					emsg << "Login failed.\nMissing agent session Id !";
				}
			}
			else
			{
				gAgentSessionID.set(text);
				gDebugInfo["SessionID"] = text;
			}

			text = gUserAuth.getResponseStr("secure_session_id");
			if (text.empty())
			{
				llwarns << "Missing secure agent session Id. Asset uploads will fail !"
						<< llendl;
			}
			else
			{
				gAgent.mSecureSessionID.set(text);
			}

			text = gUserAuth.getResponseStr("first_name");
			if (!text.empty())
			{
				// Remove quotes from string. Login.cgi sends these to force
				// names that look like numbers into strings.
				gLoginFirstName = text;
				LLStringUtil::replaceChar(gLoginFirstName, '"', ' ');
				LLStringUtil::trim(gLoginFirstName);
			}
			text = gUserAuth.getResponseStr("last_name");
			if (!text.empty())
			{
				gLoginLastName = text;
			}

			// Touch the login data only if we logged in from the login screen
			// and if we are not a second Cool VL Viewer instance (else the
			// risk is that the saved login name is reused for the wrong grid
			// next time).
			if (show_connect_box &&
				!gAppViewerp->isSecondInstanceSiblingViewer())
			{
				// Load the current saved grids logins data
				std::string history_file =
					gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS,
												   "saved_grids_login.xml");
				LLSavedLogins history_data =
					LLSavedLogins::loadFile(history_file);
				// Delete any old matching entry in the grids logins
				EGridInfo grid_choice = gm->getGridChoice();
				history_data.deleteEntry(grid_choice, gLoginFirstName,
										 gLoginLastName, gm->getGridURI());
				if (gSavedSettings.getBool("RememberLogin"))
				{
					// Successful login means the credentails are valid, so
					// save them
					gSavedSettings.setString("FirstName", gLoginFirstName);
					gSavedSettings.setString("LastName", gLoginLastName);
					savePasswordHashToSettings(password);
					// Add our credentials to the saved grids logins
					LLSavedLoginEntry login_entry(grid_choice, gLoginFirstName,
												  gLoginLastName, password);
					if (grid_choice == GRID_INFO_OTHER)
					{
						std::string grid_uri = gm->getGridURI();
						if (!grid_uri.empty())
						{
							login_entry.setGridURI(LLURI(grid_uri));
						}
						std::string login_uri = gm->getLoginPageURI();
						if (!login_uri.empty())
						{
							login_entry.setLoginPageURI(LLURI(login_uri));
						}
						std::string helper_uri = gm->getHelperURI();
						if (!helper_uri.empty())
						{
							login_entry.setHelperURI(LLURI(helper_uri));
						}
					}
					history_data.addEntry(login_entry);
					// Reassert and save our grid choice
					gm->setGridChoice(gm->getGridChoice());
					text = gUserAuth.getResponseStr("mfa_hash");
					if (!text.empty())
					{
						saveMFAHashToSettings(text);
					}
					llinfos << "Saved this successful login info." << llendl;
				}
				else
				{
					// Clear any last login data.
					gSavedSettings.setString("FirstName", "");
					gSavedSettings.setString("LastName", "");
					gSavedSettings.setString("HashedPassword", "");
					gSavedPerAccountSettings.setString("MFAHash", "");
				}
				// Save back the login history data to disk
				LLSavedLogins::saveFile(history_data, history_file);
			}

			// This is their actual ability to access content
			text = gUserAuth.getResponseStr("agent_access_max");
			if (!text.empty())
			{
				// agent_access can be 'A', 'M', and 'PG'.
				gAgent.setMaturity(text[0]);
			}
			// This is the value of their preference setting for that content
			// which will always be <= agent_access_max
			text = gUserAuth.getResponseStr("agent_region_access");
			if (!text.empty())
			{
				U8 preferredMaturity = LLAgent::convertTextToMaturity(text[0]);
				gSavedSettings.setU32("PreferredMaturity", preferredMaturity);
			}

			text = gUserAuth.getResponseStr("start_location");
			if (!text.empty()) agent_start_location.assign(text);
			text = gUserAuth.getResponseStr("circuit_code");
			if (!text.empty())
			{
				gMessageSystemp->mOurCircuitCode = strtoul(text.c_str(), NULL,
														   10);
			}
			std::string sim_ip_str = gUserAuth.getResponseStr("sim_ip");
			std::string sim_port_str = gUserAuth.getResponseStr("sim_port");
			if (!sim_ip_str.empty() && !sim_port_str.empty())
			{
				U32 sim_port = strtoul(sim_port_str.c_str(), NULL, 10);
				first_sim.set(sim_ip_str, sim_port);
				if (first_sim.isOk())
				{
					gMessageSystemp->enableCircuit(first_sim, true);
				}
			}
			std::string region_x_str = gUserAuth.getResponseStr("region_x");
			std::string region_y_str = gUserAuth.getResponseStr("region_y");
			if (!region_x_str.empty() && !region_y_str.empty())
			{
				U32 region_x = strtoul(region_x_str.c_str(), NULL, 10);
				U32 region_y = strtoul(region_y_str.c_str(), NULL, 10);
				first_sim_handle = to_region_handle(region_x, region_y);
			}

			// Variable region size support
			region_x_str = gUserAuth.getResponseStr("region_size_x");
			if (!region_x_str.empty())
			{
				first_region_size = atoi(region_x_str.c_str());
				if (first_region_size == 0)
				{
					first_region_size = REGION_WIDTH_METERS;
				}
			}
			// Let's assume regions are square
			U32 region_y_size = first_region_size;
			region_y_str = gUserAuth.getResponseStr("region_size_y");
			if (!region_y_str.empty())
			{
				region_y_size = atoi(region_y_str.c_str());
				if (region_y_size == 0)
				{
					region_y_size = first_region_size;
				}
			}
			if (first_region_size != region_y_size)
			{
				llwarns << "RECTANGULAR REGIONS NOT SUPPORTED: expect a crash !"
						<< llendl;
				first_region_size = llmax(first_region_size, region_y_size);
			}
			gViewerParcelMgr.setRegionWidth(first_region_size);

			std::string look_at_str = gUserAuth.getResponseStr("look_at");
			if (!look_at_str.empty())
			{
				size_t len = look_at_str.size();
				LLMemoryStream mstr((U8*)look_at_str.c_str(), len);
				LLSD sd = LLSDSerialize::fromNotation(mstr, len);
				agent_start_look_at = ll_vector3_from_sd(sd);
			}

			text = gUserAuth.getResponseStr("seed_capability");
			if (!text.empty())
			{
				first_sim_seed_cap = text;
			}

			text = gUserAuth.getResponseStr("seconds_since_epoch");
			if (!text.empty())
			{
				U32 server_utc_time = strtoul(text.c_str(), NULL, 10);
				if (server_utc_time)
				{
					time_t now = time(NULL);
					gUTCOffset = server_utc_time - now;
					llinfos << "UTC offset with server: " << gUTCOffset << "s"
							<< llendl;
				}
			}

			std::string home_location = gUserAuth.getResponseStr("home");
			if (!home_location.empty())
			{
				size_t len = home_location.size();
				LLMemoryStream mstr((U8*)home_location.c_str(), len);
				LLSD sd = LLSDSerialize::fromNotation(mstr, len);
				S32 region_x = sd["region_handle"][0].asInteger();
				S32 region_y = sd["region_handle"][1].asInteger();
				U64 region_handle = to_region_handle(region_x, region_y);
				LLVector3 position = ll_vector3_from_sd(sd["position"]);
				gAgent.setHomePosRegion(region_handle, position);
			}

			gAgent.mMOTD.assign(gUserAuth.getResponseStr("message"));

			const LLSD& inventory_root =
				gUserAuth.getResponse1stMap("inventory-root");
			if (inventory_root.isDefined() && inventory_root.has("folder_id"))
			{
				LLUUID inv_root_folder_id =
					inventory_root["folder_id"].asUUID();
				gInventory.setRootFolderID(inv_root_folder_id);
			}

			const LLSD& login_flags =
				gUserAuth.getResponse1stMap("login-flags");
			if (login_flags.isDefined())
			{
				std::string flag;
				if (login_flags.has("ever_logged_in"))
				{
					flag = login_flags["ever_logged_in"].asString();
					gAgent.setFirstLogin(flag == "N");
				}
				if (login_flags.has("gendered"))
				{
					flag = login_flags["gendered"].asString();
					if (flag == "Y")
					{
						gAgent.setGenderChosen(true);
					}
				}
				if (login_flags.has("daylight_savings"))
				{
					flag = login_flags["daylight_savings"].asString();
					gPacificDaylightTime = flag == "Y";
				}
			}

			const LLSD& initial_outfit =
				gUserAuth.getResponse1stMap("initial-outfit");
			if (initial_outfit.isDefined())
			{
				if (initial_outfit.has("folder_name"))
				{
					// Initial outfit is a folder in your inventory, must be an
					// exact folder-name match.
					sInitialOutfit = initial_outfit["folder_name"].asString();
				}
				if (initial_outfit.has("gender"))
				{
					sInitialOutfitGender = initial_outfit["gender"].asString();
				}
			}

			const LLSD& global_textures =
				gUserAuth.getResponse1stMap("global-textures");
			if (global_textures.isDefined())
			{
				// Extract sun and moon texture IDs. These are used in the
				// LLVOSky constructor, but I cannot figure out how to pass
				// them in.  JC
				if (global_textures.has("sun_texture_id"))
				{
					gSunTextureID = global_textures["sun_texture_id"].asUUID();
					if (gSunTextureID != IMG_SUN)
					{
						llinfos << "Sun texture Id: " << gSunTextureID
								<< llendl;
					}
				}
				if (global_textures.has("moon_texture_id"))
				{
					gMoonTextureID =
						global_textures["moon_texture_id"].asUUID();
					if (gMoonTextureID != IMG_MOON)
					{
						llinfos << "Moon texture Id: " << gMoonTextureID
								<< llendl;
					}
				}
				if (global_textures.has("cloud_texture_id"))
				{
					gCloudTextureID =
						global_textures["cloud_texture_id"].asUUID();
					if (gCloudTextureID != IMG_CLOUD_POOF)
					{
						llinfos << "Clouds texture Id: " << gCloudTextureID
								<< llendl;
					}
				}
			}

			// Set the location of the Agent Appearance service, from which
			// we can request avatar baked textures if they are supported by
			// the current region
			std::string agent_appearance_url =
				gUserAuth.getResponseStr("agent_appearance_service");
			if (!agent_appearance_url.empty())
			{
				LLVOAvatar::sAgentAppearanceServiceURL = agent_appearance_url;
			}

			// Start the process of fetching the OpenID session cookie for this
			// user login
			std::string openid_url = gUserAuth.getResponseStr("openid_url");
			if (!openid_url.empty())
			{
				std::string openid_token =
					gUserAuth.getResponseStr("openid_token");
				LLViewerMedia::openIDSetup(openid_url, openid_token);
			}

			std::string token = gUserAuth.getResponseStr("currency");
			if (!token.empty())
			{
				if (token.length() > 3)
				{
					llwarns << "Grid currency symbol too long, truncating..."
							<< llendl;
					token = token.substr(0, 2) + "$";
				}
				llinfos << "Setting grid currency symbol to: " << token
						<< llendl;
				LLUIString::setGridCurrency(token);
			}
			else if (gIsInSecondLife)
			{
				llinfos << "Using L$ as the grid currency symbol." << llendl;
			}
			else
			{
				llinfos << "Using OS$ as the grid currency symbol." << llendl;
				LLUIString::setGridCurrency("OS$");
			}

			token = gUserAuth.getResponseStr("real_currency");
			if (!token.empty())
			{
				llinfos << "Setting real currency symbol to: " << token
						<< llendl;
				LLUIString::setRealCurrency(token);
			}
			else
			{
				llinfos << "Using US$ as the real currency symbol." << llendl;
			}

			// Translate UI strings that were already built and need their
			// currency symbols translated.
			LLUIString::translatePendingCurrency();

			gMaxAgentGroups =
				LLEconomy::getInstance()->getGroupMembershipLimit();
			if (gMaxAgentGroups > 0)
			{
				llinfos << "gMaxAgentGroups read from account benefits: "
						<< gMaxAgentGroups << llendl;
			}
			else
			{
				token = gUserAuth.getResponseStr("max_groups");
				if (!token.empty())
				{
					gMaxAgentGroups = atoi(token.c_str());
					if (gMaxAgentGroups > 0)
					{
						llinfos << "gMaxAgentGroups read from 'max_groups' in login.cgi: "
								<< gMaxAgentGroups << llendl;
					}
					else
					{
						llwarns << "Invalid 'max_groups' value in login.cgi: '"
								<< token << "'" << llendl;
					}
				}
			}
			if (gMaxAgentGroups <= 0)
			{
				token = gUserAuth.getResponseStr("max-agent-groups");
				if (!token.empty())
				{
					gMaxAgentGroups = atoi(token.c_str());
					if (gMaxAgentGroups > 0)
					{
						llinfos << "gMaxAgentGroups read from 'max-agent-groups' in login.cgi: "
								<< gMaxAgentGroups << llendl;
					}
					else
					{
						llwarns << "Invalid 'max-agent-groups' value in login.cgi: '"
								<< token << "'" << llendl;
					}
				}
			}
			if (gMaxAgentGroups <= 0)
			{
				gMaxAgentGroups = gIsInSecondLife ? DEFAULT_MAX_AGENT_GROUPS
												  : OPENSIM_DEFAULT_MAX_AGENT_GROUPS;
				llinfos << "gMaxAgentGroups set to default: "
						<< gMaxAgentGroups << llendl;
			}

			token = gUserAuth.getResponseStr("map-server-url");
			if (token.empty())
			{
				LLWorldMap::setMapServerURL(gSavedSettings.getString("MapServerURL"));
			}
			else
			{
				LLWorldMap::gotMapServerURL(true);
				LLWorldMap::setMapServerURL(token, true);
				llinfos << "Got map server URL: " << token << llendl;
			}

			token = gUserAuth.getResponseStr("search-server-url");
			if (!gIsInSecondLife && !token.empty())
			{
				HBFloaterSearch::setSearchURL(token, true);
				llinfos << "Got search query URL: " << token << llendl;
			}

			// JC: gesture loading done below, when we have an asset system
			// in place. Do not delete/clear user_credentials until then.

			if (gAgentID.notNull() && gAgentSessionID.notNull() &&
				gMessageSystemp->mOurCircuitCode && first_sim.isOk())
			{
				// Pass the user information to the voice chat server interface.
				gVoiceClient.userAuthorized(gLoginFirstName, gLoginLastName,
											gAgentID);
				setStartupState(STATE_WORLD_INIT);
				return false;
			}
		}

		// When auto-logged in, abort after a 5s display of the error message
		// in the progress bar
		if (gSavedSettings.getBool("AutoLogin"))
		{
			// *TODO: translate
			std::string errmsg =
				"Cannot connect. The viewer will auto-close in a few seconds...";
			gViewerWindowp->setProgressString(errmsg);
			doAfterInterval(call_force_quit, 5.f);
			// Jail ourselves in a no-op state until we quit...
			setStartupState(STATE_LOGIN_WAIT);
			return false;
		}

		if (notify_user)
		{
			LLSD args;
			args["ERROR_MESSAGE"] = emsg.str();
			gNotifications.add("ErrorMessage", args, LLSD(), loginAlertDone);
		}
		// Bounce back to the login screen.
		resetLogin();
		show_connect_box = true;
		return false;
	}

	//---------------------------------------------------------------------
	// World init
	//---------------------------------------------------------------------
	if (getStartupState() == STATE_WORLD_INIT)
	{
		gFrameSleepTime = 0;
		setStartupStatus(0.4f, LLTrans::getString("LoginInitializingWorld"),
						 gAgent.mMOTD);
		display_startup();
		// We should have an agent id by this point.
		llassert(gAgentID.notNull());

		// Finish agent initialization (requires gSavedSettings, inits camera)
		gAgent.init();
		set_underclothes_menu_options();

		// Since we connected, save off the settings so the user does not have
		// to type the name/password again if we crash.
		gAppViewerp->saveGlobalSettings();

		// Load the teleport history
		gFloaterTeleportHistoryp->loadEntries();

		// Load autopilot stuff
		gAgentPilot.load(gSavedSettings.getString("AutoPilotFile"));

		//
		// Initialize classes w/graphics stuff.
		//
		gTextureList.doPrefetchImages();

		// We used to call LLFace::initClass() here (now empty and removed)
		// We used to call LLDrawable::initClass() here (now empty and removed)

		LLAvatarAppearance::initClass("avatar_lad.xml", "avatar_skeleton.xml");

		LLViewerObject::initVOClasses();

		display_startup();

		// This is where we used to initialize gWorldp. Original comment said:
		// World initialization must be done after above window init

		// User might have overridden far clip
		gWorld.setLandFarClip(gAgent.mDrawDistance);

		// Before we create the first region, we need to set the agent's
		// mOriginGlobal. This is necessary because creating objects before
		// this is set will result in a bad mPositionAgent cache.
		gAgent.initOriginGlobal(from_region_handle(first_sim_handle));

		gWorld.addRegion(first_sim_handle, first_sim, first_region_size);

		LLViewerRegion* regionp = gWorld.getRegionFromHandle(first_sim_handle);
		llinfos << "Adding initial simulator "
				<< regionp->getOriginGlobal() << llendl;

		regionp->setSeedCapability(first_sim_seed_cap);
		LL_DEBUGS("AppInit") << "Waiting for seed grant ...." << LL_ENDL;

		// Set agent's initial region to be the one we just created.
		gAgent.setRegion(regionp);

		// Set agent's initial position, which will be read by LLVOAvatar when
		// the avatar object is created. I think this must be done after
		// setting the region. JC
		gAgent.setPositionAgent(agent_start_position_region);

		// Initialize experiences
		gAppViewerp->loadExperienceCache();
		LLExperienceCache* expcache = LLExperienceCache::getInstance();
		expcache->setCapabilityQuery(boost::bind(&LLAgent::getRegionCapability,
												 &gAgent, _1));
		LLExperienceLog::getInstance()->initialize();

		display_startup();
		setStartupState(STATE_MULTIMEDIA_INIT);
		return false;
	}

	//---------------------------------------------------------------------
	// Load multimedia engines; can be slow. Do it while we are waiting on
	// the network for our seed capability. JC
	//---------------------------------------------------------------------
	if (getStartupState() == STATE_MULTIMEDIA_INIT)
	{
		multimediaInit();
		setStartupState(STATE_SEED_GRANTED_WAIT);
		return false;
	}

	//---------------------------------------------------------------------
	// Wait for seed cap grant.
	//---------------------------------------------------------------------
	if (getStartupState() == STATE_SEED_GRANTED_WAIT)
	{
		U32 retries = 0;
		LLViewerRegion* regionp = gWorld.getRegionFromHandle(first_sim_handle);
		if (regionp &&
			(regionp->capabilitiesReceived() ||
			 // Try to connect despite capabilities' error state...
			 regionp->capabilitiesError() ||
			 // ... or exhausted retries count.
			 (retries = regionp->getNumSeedCapRetries()) >
				MAX_SEED_CAP_ATTEMPTS_BEFORE_LOGIN))
		{
			setStartupState(STATE_SEED_CAP_GRANTED);
			return false;
		}
		if (retries > 1)
		{
			setStartupStatus(0.44f,
							 LLTrans::getString("LoginRetrySeedRequest"),
							 gAgent.mMOTD);
		}
		else
		{
			setStartupStatus(0.43f, LLTrans::getString("LoginWaitingForSeed"),
							 gAgent.mMOTD);
		}
		gFrameSleepTime = 10;	// Do not hog the CPU
		return false;
	}

	//---------------------------------------------------------------------
	// Seed capability granted.
	// No newMessage calls should happen before this point.
	//---------------------------------------------------------------------
	if (getStartupState() == STATE_SEED_CAP_GRANTED)
	{
		gFrameSleepTime = 1;
		LLAppViewer::updateTextureFetch();

		if (gViewerWindowp)
		{
			// This is not the first logon attempt, so show the UI
			gViewerWindowp->setNormalControlsVisible(true);
		}
		gLoginMenuBarViewp->setVisible(false);
		gLoginMenuBarViewp->setEnabled(false);

		if (gAudiop)
		{
			gAudiop->setMuted(true); // Do not play the floaters opening sound
		}

		// Adjust the floaters position for first use
		gViewerWindowp->adjustRectanglesForFirstUse();

		// Move the progress view in front of the UI
		gViewerWindowp->moveProgressViewToFront();

		if (gDebugViewp && gDebugViewp->mDebugConsolep)
		{
			LLError::logToFixedBuffer(gDebugViewp->mDebugConsolep);
			// Set initial visibility of debug console
			gDebugViewp->mDebugConsolep->setVisible(gSavedSettings.getBool("ShowDebugConsole"));
		}

		// Load the chat history now, if configured by the user. HB
		if (gSavedPerAccountSettings.getBool("LogShowHistory"))
		{
			LLFloaterChat::getInstance(LLSD())->loadHistory();
		}

		//
		// Set message handlers
		//
		llinfos << "Initializing communications..." << llendl;

		// Register callbacks for messages... Do this after initial handshake
		// to make sure that we don't catch any unwanted
		LLMessageSystem* msg = gMessageSystemp;
		registerViewerCallbacks(msg);
		// Register null callbacks for audio until the audio system is
		// initialized
		msg->setHandlerFuncFast(_PREHASH_SoundTrigger, null_message_callback);
		msg->setHandlerFuncFast(_PREHASH_AttachedSound, null_message_callback);

		// Debugging info parameters

		// Spam if decoding all msgs takes more than 500ms
		msg->setMaxMessageTime(0.5f);

#if LL_DEBUG
		// Time the decode of each msg
		msg->setTimeDecodes(true);
		// Spam if a single msg takes over 50ms to decode
		msg->setTimeDecodesSpamThreshold(0.05f);
#endif

		gXferManagerp->registerCallbacks(msg);

		if (!gCacheNamep)
		{
			// Initialize the legacy name cache
			gCacheNamep = new LLCacheName(msg);
			gCacheNamep->addObserver(&callbackCacheName);

			// Load stored cache if possible
			gAppViewerp->loadNameCache();

			// Initialize the new avatar name cache
			LLAvatarNameCache::initClass();
		}

		// Reset statistics
		gViewerStats.resetStats();

		//
		// Set up all of our statistics UI stuff.
		//
		display_startup();

		//
		// Set up region and surface defaults
		//

		// Sets up the parameters for the first simulator

		LL_DEBUGS("AppInit") << "Initializing camera..." << LL_ENDL;
		gFrameTime = LLTimer::totalTime();
		F32 last_time = gFrameTimeSeconds;
		gFrameTimeSeconds = (S64)(gFrameTime - gStartTime) / SEC_TO_MICROSEC;

		gFrameIntervalSeconds = gFrameTimeSeconds - last_time;
		if (gFrameIntervalSeconds < 0.f)
		{
			gFrameIntervalSeconds = 0.f;
		}

		// Make sure agent knows correct aspect ratio. FOV limits depend upon
		// aspect ratio so this needs to happen before initializing the FOV
		// below.
		gViewerCamera.setViewHeightInPixels(gViewerWindowp->getWindowDisplayHeight());
		if (gWindowp->getFullscreen())
		{
			gViewerCamera.setAspect(gViewerWindowp->getDisplayAspectRatio());
		}
		else
		{
			gViewerCamera.setAspect((F32)gViewerWindowp->getWindowWidth() /
									 (F32)gViewerWindowp->getWindowHeight());
		}
		// Initialize FOV
		gViewerCamera.setDefaultFOV(gSavedSettings.getF32("CameraAngle"));

		// Move agent to starting location. The position handed to us by the
		// space server is in global coordinates, but the agent frame is in
		// region local coordinates. Therefore, we need to adjust the
		// coordinates handed to us to fit in the local region.
		gAgent.setPositionAgent(agent_start_position_region);
		gAgent.resetAxes(agent_start_look_at);
		gAgent.stopCameraAnimation();
		gAgent.resetCamera();

		// Initialize global class data needed for surfaces (i.e. textures)

		LL_DEBUGS("AppInit") << "Initializing sky..." << LL_ENDL;
		LL_GL_CHECK_STATES;
		gSky.init();
		LL_GL_CHECK_STATES;

		LL_DEBUGS("AppInit") << "Decoding images..." << LL_ENDL;
		// For all images pre-loaded into viewer cache, decode them.
		// Need to do this AFTER we init the sky
		std::string decoding = LLTrans::getString("LoginDecodingImages");
		constexpr S32 DECODE_TIME_SEC = 3;
		for (S32 i = 0; i < DECODE_TIME_SEC; ++i)
		{
			F32 frac = (F32)i / (F32)DECODE_TIME_SEC;
			setStartupStatus(0.45f + frac * 0.1f, decoding, gAgent.mMOTD);
			display_startup();
			if (!gTextureList.decodeAllImages(1.f))
			{
				setStartupStatus(0.55f, decoding, gAgent.mMOTD);
				break;
			}
		}
		setStartupState(STATE_WORLD_WAIT);

		// JC - Do this as late as possible to increase likelihood Purify will
		// run.
		if (!msg->mOurCircuitCode)
		{
			llwarns << "Attempting to connect to simulator with a zero circuit code !"
					<< llendl;
		}

		gUseCircuitCallbackCalled = false;

		msg->enableCircuit(first_sim, true);
		// Now, use the circuit info to tell simulator about us !
		llinfos << "Enabling simulator '" << first_sim << "' with code: "
				<< msg->mOurCircuitCode << llendl;
		msg->newMessageFast(_PREHASH_UseCircuitCode);
		msg->nextBlockFast(_PREHASH_CircuitCode);
		msg->addU32Fast(_PREHASH_Code, msg->mOurCircuitCode);
		msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
		msg->addUUIDFast(_PREHASH_ID, gAgentID);
		msg->sendReliable(first_sim, MAX_TIMEOUT_COUNT, false, TIMEOUT_SECONDS,
						  useCircuitCallback, NULL);

		timeout.reset();

		return false;
	}

	//---------------------------------------------------------------------
	// World wait.
	//---------------------------------------------------------------------
	if (getStartupState() == STATE_WORLD_WAIT)
	{
		LL_DEBUGS_ONCE("AppInit") << "Waiting for simulator ack...."
								  << LL_ENDL;
		setStartupStatus(0.59f,
						 LLTrans::getString("LoginWaitingForRegionHandshake"),
						 gAgent.mMOTD);

		// Process messages to keep from dropping circuit.
		process_messages();

		if (gGotUseCircuitCodeAck)
		{
			gFrameSleepTime = 1;
			setStartupState(STATE_AGENT_SEND);
		}
		else
		{
			gFrameSleepTime = 10;	// Do not hog the CPU
		}

		return false;
	}

	//---------------------------------------------------------------------
	// Agent send.
	//---------------------------------------------------------------------
	if (getStartupState() == STATE_AGENT_SEND)
	{
		gFrameSleepTime = 1;
		LL_DEBUGS_ONCE("AppInit") << "Connecting to region..." << LL_ENDL;
		setStartupStatus(0.6f, LLTrans::getString("LoginConnectingToRegion"),
						 gAgent.mMOTD);
		// Register with the message system so it knows we are expecting this
		// message
		LLMessageSystem* msg = gMessageSystemp;
		msg->setHandlerFuncFast(_PREHASH_AgentMovementComplete,
								process_agent_movement_complete);
		LLViewerRegion* regionp = gAgent.getRegion();
		if (regionp)
		{
			send_complete_agent_movement(regionp->getHost());
			gAssetStoragep->setUpstream(regionp->getHost());
			gCacheNamep->setUpstream(regionp->getHost());
			msg->newMessageFast(_PREHASH_EconomyDataRequest);
			gAgent.sendReliableMessage();
		}

		setStartupState(STATE_AGENT_WAIT);	// Go to STATE_AGENT_WAIT

		timeout.reset();
		return false;
	}

	//---------------------------------------------------------------------
	// Agent wait.
	//---------------------------------------------------------------------
	if (getStartupState() == STATE_AGENT_WAIT)
	{
		gFrameSleepTime = 1;
		{	// Scope guard for LockMessageChecker
			LLMessageSystem* msg = gMessageSystemp;
#if LL_USE_FIBER_AWARE_MUTEX
			LockMessageChecker lmc(msg);
			while (lmc.checkAllMessages(gFrameCount, gServicePumpIOp))
#else
			while (msg->checkAllMessages(gFrameCount, gServicePumpIOp))
#endif
			{
				if (gAgentMovementCompleted)
				{
					// Sometimes we have more than one message in the queue.
					// Break out of this loop and continue processing. If we do
					// not, then this could skip one or more login steps.
					break;
				}
				else
				{
					LL_DEBUGS("AppInit") << "Awaiting AvatarInitComplete, got "
										 << msg->getMessageName()
										 << LL_ENDL;
				}
			}
#if LL_USE_FIBER_AWARE_MUTEX
			lmc.processAcks();
#else
			msg->processAcks();
#endif
		}	// End of scope for LockMessageChecker

		if (gAgentMovementCompleted)
		{
			setStartupState(STATE_INVENTORY_SEND);
		}
		else if (timeout.getElapsedTimeF32() > STATE_AGENT_WAIT_TIMEOUT)
		{
			// Make sure user knows something bad happened.
			// When auto-logged in, abort after a 5s display of the error message
			// in the progress bar
			if (gSavedSettings.getBool("AutoLogin"))
			{
				// *TODO: translate
				std::string errmsg =
					"Cannot connect. The viewer will auto-close in a few seconds...";
				gViewerWindowp->setProgressString(errmsg);
				doAfterInterval(call_force_quit, 5.f);
				// Jail ourselves in a no-op state until we quit...
				setStartupState(STATE_LOGIN_WAIT);
				return false;
			}
			// Make sure user knows something bad happened.
			gNotifications.add("LoginPacketNeverReceived", LLSD(), LLSD(),
							   loginAlertStatus);
			llwarns << "Returning to login screen !" << llendl;
			resetLogin();
		}
		else
		{
			gFrameSleepTime = 10;	// Do not hog the CPU
		}

		return false;
	}

	//---------------------------------------------------------------------
	// Inventory send.
	//---------------------------------------------------------------------
	if (getStartupState() == STATE_INVENTORY_SEND)
	{
		gFrameSleepTime = 0;
		// Inform simulator of our language preference
		gAgent.updateLanguage();

		// Request mute list
		llinfos << "Requesting Mute list" << llendl;
		LLMuteList::requestFromServer();

		// Get L$ and ownership credit information
		llinfos << "Requesting Money balance" << llendl;
		LLStatusBar::sendMoneyBalanceRequest();

		if (gSavedPerAccountSettings.getBool("ClearInventoryCache"))
		{
			gSavedPerAccountSettings.setBool("ClearInventoryCache", false);
			std::string file = gInventory.getCacheFileName(gAgentID) + ".gz";
			if (LLFile::exists(file))
			{
				llinfos << "Per user request, removing inventory cache file: "
						<< file << llendl;
				LLFile::remove(file);
			}
		}

		const LLSD& inv_lib_root =
			gUserAuth.getResponse1stMap("inventory-lib-root");
		if (inv_lib_root.isDefined() && inv_lib_root.has("folder_id"))
		{
			LLUUID id = inv_lib_root["folder_id"].asUUID();
			gInventory.setLibraryRootFolderID(id);
		}
		else
		{
			llwarns << "Cannot find library root inventory folder Id !"
					<< llendl;
		}

		const LLSD& inv_lib_owner =
			gUserAuth.getResponse1stMap("inventory-lib-owner");
		if (inv_lib_owner.isDefined() && inv_lib_owner.has("agent_id"))
		{
			LLUUID id = inv_lib_owner["agent_id"].asUUID();
			gInventory.setLibraryOwnerID(id);
		}
		else
		{
			gInventory.setLibraryOwnerID(ALEXANDRIA_LINDEN_ID);
			llwarns << "Cannot find inventory library owner Id. Using Alexandra Linden's Id."
					<< llendl;
		}

		const LLSD& inv_skel_lib = gUserAuth.getResponse("inventory-skel-lib");
		if (inv_skel_lib.isDefined() &&
			!gInventory.loadSkeleton(inv_skel_lib,
									 gInventory.getLibraryOwnerID()))
		{
			llwarns << "Problem loading inventory-skel-lib" << llendl;
		}

		const LLSD& inv_skeleton = gUserAuth.getResponse("inventory-skeleton");
		if (inv_skeleton.isDefined() &&
			!gInventory.loadSkeleton(inv_skeleton, gAgentID))
		{
			llwarns << "Problem loading inventory-skeleton" << llendl;
		}

		const LLSD& buddy_list = gUserAuth.getResponse("buddy-list");
		if (buddy_list.isDefined())
		{
			LLAvatarTracker::buddy_map_t list;
			LLUUID agent_id;
			S32 has_rights, given_rights;
			for (LLSD::array_const_iterator it = buddy_list.beginArray(),
											end = buddy_list.endArray();
				 it != end; ++it)
			{
				const LLSD& entry = *it;

				if (!entry.has("buddy_id"))
				{
					continue;
				}
				agent_id = entry["buddy_id"].asUUID();

				has_rights = given_rights = 0;
				if (entry.has("buddy_rights_has"))
				{
					has_rights = entry["buddy_rights_has"].asInteger();
				}
				if (entry.has("buddy_rights_given"))
				{
					given_rights = entry["buddy_rights_given"].asInteger();
				}

				list[agent_id] = new LLRelationship(given_rights, has_rights,
													false);
			}
			gAvatarTracker.addBuddyList(list);
		}

		const LLSD& ui_config = gUserAuth.getResponse("ui-config");
		if (ui_config.isDefined())
		{
			for (LLSD::array_const_iterator it = ui_config.beginArray(),
											end = ui_config.endArray();
				 it != end; ++it)
			{
				const LLSD& entry = *it;
				if (entry.has("allow_first_life") &&
					entry["allow_first_life"].asString() == "Y")
				{
					LLPanelAvatar::sAllowFirstLife = true;
					break;
				}
			}
		}

		const LLSD& event_cats = gUserAuth.getResponse("event_categories");
		if (event_cats.isDefined())
		{
			LLEventInfo::loadCategories(event_cats);
		}

		const LLSD& event_notif = gUserAuth.getResponse("event_notifications");
		if (event_notif.isDefined())
		{
			gEventNotifier.load(event_notif);
		}

		const LLSD& classified_cats =
			gUserAuth.getResponse("classified_categories");
		if (classified_cats.isDefined())
		{
			LLClassifiedInfo::loadCategories(classified_cats);
		}

		gInventory.buildParentChildMap();

		// Set up callbacks
		llinfos << "Registering callbacks" << llendl;
		LLMessageSystem* msg = gMessageSystemp;
		llinfos << "Inventory" << llendl;
		LLInventoryModel::registerCallbacks(msg);
		llinfos << "AvatarTracker" << llendl;
		gAvatarTracker.registerCallbacks(msg);
		llinfos << "Landmark" << llendl;
		LLLandmark::registerCallbacks(msg);

		// Request all group information
		llinfos << "Requesting agent groups data" << llendl;
		gAgent.sendAgentDataUpdateRequest();

		// Create the inventory floater
		llinfos << "Creating inventory floater" << llendl;
		bool shown_at_exit = gSavedSettings.getBool("ShowInventory");
		LLFloaterInventory::showAgentInventory();
		// Hide the inventory floater if it was not shown at exit
		if (!shown_at_exit)
		{
			LLFloaterInventory::toggleVisibility(NULL);
		}

		// Change the window title to include the avatar name.
		gWindowTitle = gSecondLife + " - " + gLoginFirstName + " " +
					   gLoginLastName;
#if LL_DEBUG || LL_NO_FORCE_INLINE
		LLStringUtil::truncate(gWindowTitle, 247);
		gWindowp->setWindowTitle(gWindowTitle + " [DEVEL]");
#else
		LLStringUtil::truncate(gWindowTitle, 255);
		gWindowp->setWindowTitle(gWindowTitle);
#endif

		setStartupState(STATE_MISC);
		return false;
	}

	//---------------------------------------------------------------------
	// Misc
	//---------------------------------------------------------------------
	if (getStartupState() == STATE_MISC)
	{
		// Display the floaters that we left open on logout
		bool show_radar = gSavedSettings.getBool("ShowRadar");
		if (show_radar || gSavedSettings.getBool("RadarKeepOpen"))
		{
			// Start the radar updates and bookkeeping
			HBFloaterRadar::showInstance();
			if (!show_radar)
			{
				// Hide the radar if the floater was not visible on last logout
				HBFloaterRadar::hideInstance();
			}
		}
		if (gSavedSettings.getBool("ShowMiniMap"))
		{
			LLFloaterMiniMap::showInstance();
		}
		if (gSavedSettings.getBool("ShowCameraControls"))
		{
			LLFloaterCamera::showInstance();
		}
		if (gSavedSettings.getBool("ShowMovementControls"))
		{
			LLFloaterMove::showInstance();
		}
		if (gSavedSettings.getBool("ShowActiveSpeakers"))
		{
			LLFloaterActiveSpeakers::showInstance();
		}
		if (gSavedSettings.getBool("BeaconAlwaysOn"))
		{
			LLFloaterBeacons::showInstance();
		}
		if (gSavedSettings.getBool("ShowDebugStats"))
		{
			LLFloaterStats::showInstance();
		}

		// We are successfully logged in.
		LLPanelLogin::close();

		std::string nextLoginLocation =
			gSavedSettings.getString("NextLoginLocation");
		if (nextLoginLocation.length())
		{
			// Clear it
			gSavedSettings.setString("NextLoginLocation", "");

			// And make sure it is saved
			gAppViewerp->saveGlobalSettings();
		}

		// JC: Initializing audio requests many sounds for download.
		init_audio();
		// Re-register callbacks for audio, this time with active ones
		LLMessageSystem* msg = gMessageSystemp;
		msg->setHandlerFuncFast(_PREHASH_SoundTrigger, process_sound_trigger);
		msg->setHandlerFuncFast(_PREHASH_PreloadSound, process_preload_sound);
		msg->setHandlerFuncFast(_PREHASH_AttachedSound,
								process_attached_sound);
		msg->setHandlerFuncFast(_PREHASH_AttachedSoundGainChange,
								process_attached_sound_gain_change);

		// JC: Initialize "active" gestures. This may also trigger many gesture
		// downloads, if this is the user's first time on this machine or
		// -purge has been run.
		const LLSD& gesture_options = gUserAuth.getResponse("gestures");
		if (gesture_options.isDefined())
		{
			gGestureManager.load(gesture_options);
		}

		gDisplaySwapBuffers = true;

		LL_DEBUGS("AppInit") << "Initialization complete" << LL_ENDL;

		gRenderStartTime.reset();
		// Make sure we are not paused before calling reset()
		gForegroundTime.pause();
		gForegroundTime.unpause();
		gForegroundTime.reset();

		// Fetch inventory in the background
		LLInventoryModelFetch::getInstance()->start();

		// *HACK: inform simulator of window size. Do this here so it is less
		// likely to race with RegisterNewAgent. JC - 7/20/2002
		gViewerWindowp->sendShapeToSim();

		if (!gAgent.isFirstLogin())
		{
			LLSLURL start_slurl = getStartSLURL();
			LLSLURL::SLURL_TYPE start_slurl_type = start_slurl.getType();
			if (!(start_slurl_type == LLSLURL::LOCATION &&
				  agent_start_location == "url") &&
				!(start_slurl_type == LLSLURL::LAST_LOCATION &&
				  agent_start_location == "last") &&
				!(start_slurl_type == LLSLURL::HOME_LOCATION &&
				  agent_start_location == "home"))
			{
				// The reason we show the alert is because we want to reduce
				// confusion for when you log in and your provided location is
				// not your expected location. So, if this is your first login,
				// then you do not have an expectation, thus, do not show this
				// alert.
				LLSD args;
				switch (start_slurl.getType())
				{
					case LLSLURL::LOCATION:
						args["TYPE"] = "desired";
						args["HELP"] = "";
						break;

					case LLSLURL::HOME_LOCATION:
						args["TYPE"] = "home";
						args["HELP"] = "You may want to set a new home location.";
						break;

					default:
						args["TYPE"] = "last";
						args["HELP"] = "";
				}
				gNotifications.add("AvatarMoved", args);
				gAvatarMovedOnLogin = true;
			}
		}

		// DEV-17797. Get null folder. Any items found here moved to Lost and
		// Found
		LLInventoryModelFetch::getInstance()->findLostItems();

		setStartupState(STATE_PRECACHE);
		timeout.reset();
		return false;
	}

	if (getStartupState() == STATE_PRECACHE)
	{
		F32 timeout_frac = timeout.getElapsedTimeF32() / precaching_delay;

		// We now have an inventory skeleton, so if this is a user's first
		// login, we can start setting up their clothing and avatar appearance.
		if (gAgent.isFirstLogin() &&
			!sInitialOutfit.empty() &&			// registration set up an outfit
			!sInitialOutfitGender.empty() &&	// and a gender
			isAgentAvatarValid() &&				// can't wear clothes without object
			!gAgent.isGenderChosen())			// nothing already loading
		{
			// Start loading the wearables, textures, gestures
			loadInitialOutfit(sInitialOutfit, sInitialOutfitGender);
		}
		else if (gIsInSecondLife && isAgentAvatarValid() &&
				 !gAgent.isFirstLogin() &&
				 !gAgentWearables.initialWearablesUpdateReceived())
		{
			// The initial outfit UDP message is no more relevant/valid in SL, so
			// do not bother waiting for it if not yet received at this point.
			llinfos << "Flagging the deprecated initial outfit message as received"
					<< llendl;
			gAgentWearables.setInitialWearablesUpdateReceived();
		}

		// Wait precache-delay and for agent's avatar or a lot longer.
		if ((timeout_frac > 1.f && isAgentAvatarValid()) || timeout_frac > 3.f)
		{
			setStartupState(STATE_WEARABLES_WAIT);
		}
		else
		{
			LLAppViewer::updateTextureFetch();
			setStartupStatus(0.6f + 0.3f * timeout_frac,
							 LLTrans::getString("LoginPrecaching"),
							 gAgent.mMOTD);
			display_startup();
		}

		return true;
	}

	if (getStartupState() == STATE_WEARABLES_WAIT)
	{
		static LLFrameTimer wearables_timer;

		const F32 wearables_time = wearables_timer.getElapsedTimeF32();
		constexpr F32 MAX_WEARABLES_TIME = 10.f;

		// Fetch inventory in the background (again, just in case the first
		// fetch could not yet complete and maybe got stuck). HB
		LLInventoryModelFetch::getInstance()->start();

		if (!gAgent.isGenderChosen())
		{
			// No point in waiting for clothing, we do not even know what
			// gender we are. Pop a dialog to ask and proceed to draw the
			// world. Note: we might hit this case even if we have an initial
			// outfit, but if the load has not started already then something
			// is wrong so fall back to generic outfits. JC
			gNotifications.add("WelcomeChooseSex", LLSD(), LLSD(),
							   callbackChooseGender);
			setStartupState(STATE_CLEANUP);
			return true;
		}

		if (wearables_time > MAX_WEARABLES_TIME)
		{
			gNotifications.add("ClothingLoading");
			gViewerStats.incStat(LLViewerStats::ST_WEARABLES_TOO_LONG);
			setStartupState(STATE_CLEANUP);
			return true;
		}

		if (gAgent.isFirstLogin())
		{
			// Wait for avatar to be completely loaded
			if (isAgentAvatarValid() && gAgentAvatarp->isFullyLoaded())
			{
				LL_DEBUGS("AppInit") << "Avatar fully loaded" << LL_ENDL;
				setStartupState(STATE_CLEANUP);
				return true;
			}
		}
		else if (gAgentWearables.areWearablesLoaded())
		{
			// We have our clothing, proceed.
			LL_DEBUGS("AppInit") << "Wearables loaded" << LL_ENDL;
			setStartupState(STATE_CLEANUP);
			return true;
		}

		LLAppViewer::updateTextureFetch();
		setStartupStatus(0.9f + 0.1f * wearables_time / MAX_WEARABLES_TIME,
						 LLTrans::getString("LoginDownloadingClothing"),
						 gAgent.mMOTD);
		return true;
	}

	if (getStartupState() == STATE_CLEANUP)
	{
		setStartupStatus(1.f, "", "");

		LLViewerMedia::loadDomainFilterList();

		// Let the map know about the inventory and online friends.
		if (gFloaterWorldMapp)
		{
			gFloaterWorldMapp->observeInventory(&gInventory);
			gFloaterWorldMapp->observeFriends();
		}

		gViewerWindowp->showCursor();
		gWindowp->resetBusyCount();
		gWindowp->setCursor(UI_CURSOR_ARROW);
		LL_DEBUGS("AppInit") << "Done releasing bitmap" << LL_ENDL;
		gViewerWindowp->setShowProgress(false);
		gViewerWindowp->setProgressCancelButtonVisible(false);

		// We are not away from keyboard, even though login might have taken a
		// while. JC
		gAgent.clearAFK();

		// Have the agent start watching the friends list so we can update
		// proxies
		gAgent.observeFriends();
//MK
		if (gRLenabled)
		{
			// If we were restricted with @standtp before logging out, TP back
			// there
			gRLInterface.restoreLastStandingLoc();
			gRLInterface.backToLastStandingLoc();
		}
		else
//mk
		{
			// If we have got a startup URL, dispatch it now
			dispatchURL();
		}

		// Retrieve information about the land data (just accessing this the
		// first time will fetch it, then the data is cached for the viewer's
		// lifetime)
		LLProductInfoRequestManager::getInstance()->create();

		// If costs have not been received at this point, set the default ones
		// (issue seen in some OpenSim grids that do not charge for anything).
		if (LLEconomy::getInstance()->getPriceUpload() < 0)
		{
			llwarns << "Costs info not reveived. Setting default costs for: "
					<< (gIsInSecondLife ? "Second Life" : "OpenSim") << llendl;
			LLEconomy::getInstance()->setDefaultCosts(gIsInSecondLife);
			update_upload_costs_in_menus();
		}

		// Clean up LLUserAuth global instance.
		gUserAuth.reset();

		setStartupState(STATE_STARTED);

		if (gSavedSettings.getBool("SpeedRez"))
		{
			// Speed up rezzing if requested.
			F32 dist1 = gSavedSettings.getF32("RenderFarClip");
			F32 dist2 = gSavedSettings.getF32("SavedRenderFarClip");
			gSavedDrawDistance = (dist1 >= dist2 ? dist1 : dist2);
			gSavedSettings.setF32("SavedRenderFarClip", gSavedDrawDistance);
			gSavedSettings.setF32("RenderFarClip", 32.0f);
		}
		LLViewerTextureList::sLastTeleportTime = gFrameTimeSeconds;

		// Unmute audio if desired and setup volumes.
		// This is a not-uncommon crash site, so surround it with llinfos
		// output to aid diagnosis.
		llinfos << "Doing first audio_update_volume..." << llendl;
		audio_update_volume();
		llinfos << "Done first audio_update_volume." << llendl;

		// Reset keyboard focus to sane state of pointing at world
		gFocusMgr.setKeyboardFocus(NULL);

		gAppViewerp->handleLoginComplete();

		if (isAgentAvatarValid())
		{
			gAgentAvatarp->scheduleHoverUpdate();
		}

		// Set a fixed Sun position at login, if requested by the user. HB
		F32 login_sun_pos = gSavedSettings.getF32("SunPositionAtLogin");
		if (login_sun_pos >= 0.f && login_sun_pos <= 1.f)
		{
			gSavedSettings.setBool("UseParcelEnvironment", false);
			gEnvironment.setLocalEnvFromDefaultWindlightDay(login_sun_pos);
		}
		else if (!gAgent.hasExtendedEnvironment())
		{
			gSavedSettings.setBool("UseParcelEnvironment", false);
			// Load the default Windlight day settings, and use region time
			gEnvironment.setLocalEnvFromDefaultWindlightDay();
		}
		else
		{
			gSavedSettings.setBool("UseParcelEnvironment", true);
		}

		// Setup the Marketplace, if any.
		LLMarketplace::setup();

		// *HACK: fix bogus OpenSim inventory layouts (happens on first login
		// after account creation, but we cannot rely on gAgent.isFirstLogin()
		// which is always false in OpenSim). HB
		if (!gIsInSecondLife && first_grid_login)
		{
			// Also consolidate the COF, even when not using it.
			bool use_cof = gSavedSettings.getBool("OSUseCOF");
			gSavedSettings.setBool("OSUseCOF", true);
			LLInventoryModel::checkSystemFolders(NULL);
			gSavedSettings.setBool("OSUseCOF", use_cof);
		}

		// Signal our login to the automation script, if any. HB
		if (gAutomationp)
		{
			gAutomationp->onLogin();
		}

		// We can now disable the debug messages if no debug tag was added by
		// the user from the login screen (the user may also re-enable the
		// debug message from the Advanced menu). HB
		LLError::Log::sDebugMessages =
			HBFloaterDebugTags::hasActiveDebugTags();

		// *HACK: force a refresh of objects visibility a few seconds after
		// rezzing the world, to fix pseudo-invisible object cases. Note that a
		// delay is needed (we need more frames to be rendered) and calling
		// handle_objects_visibility() immediately would not have any effect.
		// HB
		schedule_objects_visibility_refresh(1);

		return true;
	}

	llwarns << "Unexpectedly reached end of method at state: "
			<< getStartupState() << llendl;

	return true;
}

void LLStartUp::refreshLoginPanel()
{
	LLPanelLogin::clearServers();
	loginShow(true);
	LLPanelLogin::selectFirstElement();
}

//static
bool LLStartUp::loginShow(bool update_servers)
{
	static bool have_loginuri = false;

	// This creates the LLPanelLogin instance, or shows it if existing already
	LLPanelLogin::show(loginCallback);

	if (!update_servers) return have_loginuri;

	LL_DEBUGS("AppInit") << "Setting Servers" << LL_ENDL;

	// Remember which servers are already listed.
	std::set<EGridInfo> listed;
	std::set<std::string> listed_name;	// Only the 'other' grids.

	LLGridManager* gm = LLGridManager::getInstance();

	LLSavedLogins saved_logins = LLPanelLogin::getLoginHistory();
	const LLSavedLogins::list_t& login_entries = saved_logins.getEntries();

	// Add the commandline -loginuri's to the list at the top.
	have_loginuri = false;
	const std::vector<std::string>& cmd_line_uris = gm->getCommandLineURIs();
	for (S32 i = 0, count = cmd_line_uris.size(); i < count; ++i)
	{
	  	LLURI cli_uri(cmd_line_uris[i]);
		std::string cli_grid_name = cli_uri.hostName();
		LLStringUtil::toLower(cli_grid_name);
		if (listed_name.insert(cli_grid_name).second)
		{
			// If the loginuri already exists in the saved logins then use just
			// its name, otherwise show the full uri.
			bool exists = false;
			for (LLSavedLogins::list_t::const_iterator
					it = login_entries.begin(), end = login_entries.end();
			 	 it != end; ++it)
			{
				if (it->getGridName() == cli_grid_name)
				{
					exists = true;
					break;
				}
			}
			LLPanelLogin::addServer(exists ? cli_grid_name : cmd_line_uris[i],
									GRID_INFO_OTHER);
			// Causes the first server to be added here to be selected:
			have_loginuri = true;
		}
	}
	// Only look at the name for 'other' grids.
	listed.insert(GRID_INFO_OTHER);

	// Add the saved logins, last used grids first.
	for (LLSavedLogins::list_const_rit_t rit = login_entries.rbegin(),
										 rend = login_entries.rend();
		 rit != rend; ++rit)
	{
		const LLSavedLoginEntry& entry = *rit;
		EGridInfo idx = entry.getGrid();
		std::string grid_name = entry.getGridName();
		// Only show non-duplicate entries: duplicate entries do occur for ALTs
		if (listed.insert(idx).second ||
			(idx == GRID_INFO_OTHER && listed_name.insert(grid_name).second))
		{
			LLPanelLogin::addServer(grid_name, idx);
		}
	}

	// Finally show the other grid servers.
	for (EGridInfo idx = 1; idx < GRID_INFO_OTHER; ++idx)
	{
		if (listed.find(idx) == listed.end())
		{
			LLPanelLogin::addServer(gm->getKnownGridLabel(idx), idx);
		}
	}

	// Remember that the user did not change anything yet.
	gm->setNameEdited(false);

	return have_loginuri;
}

// Callback for when login screen is closed.
//static
void LLStartUp::loginCallback(S32 option, void*)
{
	constexpr S32 CONNECT_OPTION = 0;
	constexpr S32 QUIT_OPTION = 1;

	if (option == CONNECT_OPTION)
	{
		setStartupState(STATE_LOGIN_CLEANUP);
		return;
	}
	else if (option == QUIT_OPTION)
	{
		// Next iteration through main loop should shut down the app cleanly.
		gAppViewerp->userQuit();

		if (gAppViewerp->quitRequested())
		{
			LLPanelLogin::close();
		}
		return;
	}
	else
	{
		llwarns << "Unknown login button clicked" << llendl;
		llassert(false);
	}
}

//static
std::string LLStartUp::getPasswordHashFromSettings()
{
	std::string hashed_password;
#if 0	// Problem: the user might not want to loose all passwords for all
		// avatars of all grids... If the password was saved for this avatar
		// on this grid, then the viewer was asked to remember it at last
		// login, and after this new login, the password will be remembered or
		// cleared based on the new RememberLogin value anyway. HB
	// Only load password if we also intend to save it (otherwise the user
	// wonders what we are doing behind their back). JC
	if (!gSavedSettings.getBool("RememberLogin"))
	{
		return hashed_password;
	}
#endif
	hashed_password = gSavedSettings.getString("HashedPassword");
	if (hashed_password.empty())
	{
		return hashed_password;
	}

	hashed_password = LLBase64::decode(hashed_password);
	if (hashed_password.size() != MD5HEX_STR_BYTES)
	{
		llwarns << "Bad base64 saved password hash: "
				<< gSavedSettings.getString("HashedPassword") << llendl;
		return "";
	}

	// Decipher with MAC address
	LLXORCipher cipher(gMACAddress, MAC_ADDRESS_BYTES);
	cipher.decrypt((U8*)hashed_password.data(), MD5HEX_STR_BYTES);

	// Check to see if the MAC address generated a bad hashed password. It
	// should be a hex-string or else the mac address has changed. This is a
	// security feature to make sure that if you get someone's settings file,
	// you cannot hack their account.
	if (!LLStringOps::isHexString(hashed_password))
	{
		llwarns << "Invalid hash: MAC address probably changed..." << llendl;
		return "";
	}

#if LL_DEBUG_LOGIN_PASSWORD
	LL_DEBUGS("Login") << "Returning password hash: " << hashed_password
					   << LL_ENDL;
#endif
	return hashed_password;
}

//static
void LLStartUp::savePasswordHashToSettings(std::string password)
{
	if (password.size() != MD5HEX_STR_BYTES)
	{
		llwarns << "Incorrect length for password hash: " << password
				<< llendl;
		return;
	}

#if LL_DEBUG_LOGIN_PASSWORD
	LL_DEBUGS("Login") << "Ciphering password hash: " << password
					   << LL_ENDL;
#endif

	U8 buffer[MD5HEX_STR_BYTES + 1];
	LLStringUtil::copy((char*)buffer, password.c_str(), MD5HEX_STR_BYTES + 1);

	LLXORCipher cipher(gMACAddress, MAC_ADDRESS_BYTES);
	cipher.encrypt(buffer, MD5HEX_STR_BYTES);

	password = LLBase64::encode((const char*)buffer, MD5HEX_STR_BYTES);

#if LL_DEBUG_LOGIN_PASSWORD
	LL_DEBUGS("Login") << "Base64-encoded cipher: " << password << LL_ENDL;
#endif

	gSavedSettings.setString("HashedPassword", password);
}

//static
std::string LLStartUp::getMFAHashFromSettings()
{
	std::string mfa_hash = gSavedPerAccountSettings.getString("MFAHash");
	if (mfa_hash.empty())
	{
		return mfa_hash;
	}

	mfa_hash = LLBase64::decode(mfa_hash);

	// Decipher with MAC address
	LLXORCipher cipher(gMACAddress, MAC_ADDRESS_BYTES);
	cipher.decrypt((U8*)mfa_hash.data(), mfa_hash.size());

#if LL_DEBUG_LOGIN_PASSWORD
	LL_DEBUGS("Login") << "Returning MFA hash: " << mfa_hash << LL_ENDL;
#endif
	return mfa_hash;
}

//static
void LLStartUp::saveMFAHashToSettings(std::string mfa_hash)
{
	if (mfa_hash.empty())
	{
		gSavedPerAccountSettings.setString("MFAHash", mfa_hash);
		return;
	}
	size_t len = mfa_hash.size();

#if LL_DEBUG_LOGIN_PASSWORD
	LL_DEBUGS("Login") << "Ciphering MFA hash: " << mfa_hash << LL_ENDL;
#endif

	U8* buffer = new U8[len + 2];
	LLStringUtil::copy((char*)buffer, mfa_hash.c_str(), mfa_hash.size() + 1);

	LLXORCipher cipher(gMACAddress, MAC_ADDRESS_BYTES);
	cipher.encrypt(buffer, mfa_hash.size());

	mfa_hash = LLBase64::encode((const char*)buffer, mfa_hash.size());

#if LL_DEBUG_LOGIN_PASSWORD
	LL_DEBUGS("Login") << "Base64-encoded cipher: " << mfa_hash << LL_ENDL;
#endif

	gSavedPerAccountSettings.setString("MFAHash", mfa_hash);

	delete[] buffer;
}

//static
void LLStartUp::setStartupStatus(F32 frac, const std::string& string,
								 const std::string& msg)
{
	gViewerWindowp->setProgressPercent(frac * 100.f);
	gViewerWindowp->setProgressString(string);
	gViewerWindowp->setProgressMessage(msg);
}

//static
bool LLStartUp::loginAlertStatus(const LLSD&, const LLSD&)
{
	// At this point, sadly, nothing would work, including a login retry, the
	// reason being that the viewer got half-logged in and its status is too
	// unclean to retry and login from scratch.
	llinfos << "Viewer only half-logged in; cannot retry from a clean state. Quitting."
			<< llendl;
	gAppViewerp->forceQuit();
	return true;
}

//static
void LLStartUp::useCircuitCallback(void**, S32 result)
{
	if (!gUseCircuitCallbackCalled && !LLApp::isExiting())
	{
		gUseCircuitCallbackCalled = true;
		if (result)
		{
			// Make sure user knows something bad happened. JC
			llwarns << "Backing up to login screen !" << llendl;
			gNotifications.add("LoginPacketNeverReceived", LLSD(), LLSD(),
							   loginAlertStatus);
			resetLogin();
		}
		else
		{
			gGotUseCircuitCodeAck = true;
		}
	}
}

//static
void LLStartUp::callbackCacheName(const LLUUID& id,
								  const std::string& fullname,
								  bool is_group)
{
	LL_DEBUGS("NameCache") << "Legacy cache name callback triggered, refreshing name controls"
						   << LL_ENDL;

	LLNameListCtrl::refreshAll(id, fullname, is_group);
	LLNameBox::refreshAll(id, fullname, is_group);
	LLNameEditor::refreshAll(id, fullname, is_group);

	// *TODO: Actually be intelligent about the refresh; for now, just brute
	// force refresh the dialogs.
	dialog_refresh_all();
}

//static
void LLStartUp::registerViewerCallbacks(LLMessageSystem* msg)
{
	if (!msg)	// Paranoia
	{
		llerrs << "No message system !" << llendl;
	}

	msg->setHandlerFuncFast(_PREHASH_LayerData, process_layer_data);

	msg->setHandlerFuncFast(_PREHASH_ImageData,
							LLViewerTextureList::receiveImageHeader);

	msg->setHandlerFuncFast(_PREHASH_ImagePacket,
							LLViewerTextureList::receiveImagePacket);

	msg->setHandlerFuncFast(_PREHASH_ObjectUpdate, process_object_update);

	msg->setHandlerFunc(_PREHASH_ObjectUpdateCompressed,
						process_compressed_object_update);

	msg->setHandlerFunc(_PREHASH_ObjectUpdateCached,
						process_cached_object_update);

	msg->setHandlerFuncFast(_PREHASH_ImprovedTerseObjectUpdate,
							process_terse_object_update_improved);

	msg->setHandlerFunc(_PREHASH_SimStats, process_sim_stats);

	msg->setHandlerFuncFast(_PREHASH_HealthMessage, process_health_message);

	msg->setHandlerFuncFast(_PREHASH_EconomyData, process_economy_data);

	msg->setHandlerFunc(_PREHASH_RegionInfo,
						LLViewerRegion::processRegionInfo);

	msg->setHandlerFuncFast(_PREHASH_ChatFromSimulator,
							process_chat_from_simulator);

	msg->setHandlerFuncFast(_PREHASH_KillObject, process_kill_object);

	msg->setHandlerFuncFast(_PREHASH_SimulatorViewerTimeMessage,
							process_time_synch);

	msg->setHandlerFuncFast(_PREHASH_EnableSimulator,
							LLWorld::processEnableSimulator);
	msg->setHandlerFuncFast(_PREHASH_DisableSimulator,
							LLWorld::processDisableSimulator);

	msg->setHandlerFuncFast(_PREHASH_KickUser, process_kick_user);

	msg->setHandlerFunc(_PREHASH_CrossedRegion, process_crossed_region);

	msg->setHandlerFuncFast(_PREHASH_TeleportFinish, process_teleport_finish);

	msg->setHandlerFuncFast(_PREHASH_AlertMessage, process_alert_message);

	msg->setHandlerFunc(_PREHASH_AgentAlertMessage,
						process_agent_alert_message);

	msg->setHandlerFuncFast(_PREHASH_MeanCollisionAlert,
							process_mean_collision_alert_message);

	msg->setHandlerFunc(_PREHASH_ViewerFrozenMessage, process_frozen_message);

	msg->setHandlerFuncFast(_PREHASH_NameValuePair, process_name_value);

	msg->setHandlerFuncFast(_PREHASH_RemoveNameValuePair,
							process_remove_name_value);

	msg->setHandlerFuncFast(_PREHASH_AvatarAnimation,
							process_avatar_animation);

	msg->setHandlerFuncFast(_PREHASH_ObjectAnimation,
							process_object_animation);

	msg->setHandlerFuncFast(_PREHASH_AvatarAppearance,
							process_avatar_appearance);

	msg->setHandlerFunc(_PREHASH_AgentCachedTextureResponse,
						LLAgent::processAgentCachedTextureResponse);

	msg->setHandlerFunc(_PREHASH_RebakeAvatarTextures,
						LLVOAvatarSelf::processRebakeAvatarTextures);

	msg->setHandlerFuncFast(_PREHASH_CameraConstraint,
							process_camera_constraint);

	msg->setHandlerFuncFast(_PREHASH_AvatarSitResponse,
							process_avatar_sit_response);

	msg->setHandlerFunc(_PREHASH_SetFollowCamProperties,
						process_set_follow_cam_properties);

	msg->setHandlerFunc(_PREHASH_ClearFollowCamProperties,
						process_clear_follow_cam_properties);

	msg->setHandlerFuncFast(_PREHASH_ImprovedInstantMessage,
							process_improved_im);

	msg->setHandlerFuncFast(_PREHASH_ScriptQuestion,
							process_script_question);

	msg->setHandlerFuncFast(_PREHASH_ObjectProperties,
							LLSelectMgr::processObjectProperties);

	msg->setHandlerFuncFast(_PREHASH_ObjectPropertiesFamily,
							process_object_properties_family);

	msg->setHandlerFunc(_PREHASH_ForceObjectSelect,
						LLSelectMgr::processForceObjectSelect);

	msg->setHandlerFuncFast(_PREHASH_MoneyBalanceReply,
							process_money_balance_reply);

	msg->setHandlerFuncFast(_PREHASH_CoarseLocationUpdate,
							LLWorld::processCoarseUpdate);

	msg->setHandlerFuncFast(_PREHASH_ReplyTaskInventory,
						 	LLViewerObject::processTaskInv);

	msg->setHandlerFuncFast(_PREHASH_DerezContainer,
							process_derez_container);

	msg->setHandlerFuncFast(_PREHASH_ScriptRunningReply,
							LLLiveLSLEditor::processScriptRunningReply);

	msg->setHandlerFuncFast(_PREHASH_DeRezAck, process_derez_ack);

	msg->setHandlerFunc(_PREHASH_LogoutReply, process_logout_reply);

	msg->setHandlerFuncFast(_PREHASH_AgentDataUpdate,
							LLAgent::processAgentDataUpdate);

	msg->setHandlerFuncFast(_PREHASH_AgentGroupDataUpdate,
							LLAgent::processAgentGroupDataUpdate);

	msg->setHandlerFunc(_PREHASH_AgentDropGroup, LLAgent::processAgentDropGroup);

	// Land ownership messages
	msg->setHandlerFuncFast(_PREHASH_ParcelOverlay,
							LLViewerParcelMgr::processParcelOverlay);

	msg->setHandlerFuncFast(_PREHASH_ParcelProperties,
							LLViewerParcelMgr::processParcelProperties);

	msg->setHandlerFunc(_PREHASH_ParcelAccessListReply,
						LLViewerParcelMgr::processParcelAccessListReply);

	msg->setHandlerFunc(_PREHASH_ParcelDwellReply,
						LLViewerParcelMgr::processParcelDwellReply);

	msg->setHandlerFunc(_PREHASH_AvatarPropertiesReply,
						LLAvatarProperties::processAvatarPropertiesReply);

	msg->setHandlerFunc(_PREHASH_AvatarInterestsReply,
						LLAvatarProperties::processAvatarInterestsReply);

	msg->setHandlerFunc(_PREHASH_AvatarGroupsReply,
						LLAvatarProperties::processAvatarGroupsReply);

	msg->setHandlerFunc(_PREHASH_AvatarNotesReply,
						LLAvatarProperties::processAvatarNotesReply);

	msg->setHandlerFunc(_PREHASH_AvatarPicksReply,
						LLAvatarProperties::processAvatarPicksReply);

	msg->setHandlerFunc(_PREHASH_AvatarClassifiedReply,
						LLAvatarProperties::processAvatarClassifiedReply);

	msg->setHandlerFuncFast(_PREHASH_CreateGroupReply,
							LLGroupMgr::processCreateGroupReply);

	msg->setHandlerFuncFast(_PREHASH_JoinGroupReply,
							LLGroupMgr::processJoinGroupReply);

	msg->setHandlerFuncFast(_PREHASH_EjectGroupMemberReply,
							LLGroupMgr::processEjectGroupMemberReply);

	msg->setHandlerFuncFast(_PREHASH_LeaveGroupReply,
							LLGroupMgr::processLeaveGroupReply);

	msg->setHandlerFuncFast(_PREHASH_GroupProfileReply,
							LLGroupMgr::processGroupPropertiesReply);

	msg->setHandlerFuncFast(_PREHASH_AgentWearablesUpdate,
							LLAgentWearables::processAgentInitialWearablesUpdate);

	msg->setHandlerFunc(_PREHASH_ScriptControlChange,
						LLAgent::processScriptControlChange);

	msg->setHandlerFuncFast(_PREHASH_ViewerEffect,
							LLHUDManager::processViewerEffect);

	msg->setHandlerFuncFast(_PREHASH_GrantGodlikePowers,
							process_grant_godlike_powers);

	msg->setHandlerFuncFast(_PREHASH_GroupAccountSummaryReply,
							LLPanelGroupLandMoney::processGroupAccountSummaryReply);

	msg->setHandlerFuncFast(_PREHASH_GroupAccountDetailsReply,
							LLPanelGroupLandMoney::processGroupAccountDetailsReply);

	msg->setHandlerFuncFast(_PREHASH_GroupAccountTransactionsReply,
							LLPanelGroupLandMoney::processGroupAccountTransactionsReply);

	msg->setHandlerFuncFast(_PREHASH_UserInfoReply, process_user_info_reply);

	msg->setHandlerFunc(_PREHASH_RegionHandshake,
						LLWorld::processRegionHandshake);

	msg->setHandlerFunc(_PREHASH_TeleportStart, process_teleport_start);
	msg->setHandlerFunc(_PREHASH_TeleportProgress, process_teleport_progress);
	msg->setHandlerFunc(_PREHASH_TeleportFailed, process_teleport_failed);
	msg->setHandlerFunc(_PREHASH_TeleportLocal, process_teleport_local);
	msg->setHandlerFunc(_PREHASH_ScriptTeleportRequest,
						process_script_teleport_request);

	msg->setHandlerFunc(_PREHASH_ImageNotInDatabase,
						LLViewerTextureList::processImageNotInDatabase);

	msg->setHandlerFuncFast(_PREHASH_GroupMembersReply,
							LLGroupMgr::processGroupMembersReply);

	msg->setHandlerFunc(_PREHASH_GroupRoleDataReply,
						LLGroupMgr::processGroupRoleDataReply);

	msg->setHandlerFunc(_PREHASH_GroupRoleMembersReply,
						LLGroupMgr::processGroupRoleMembersReply);

	msg->setHandlerFunc(_PREHASH_GroupTitlesReply,
						LLGroupMgr::processGroupTitlesReply);

	// Special handler as this message is sometimes used for group land.
	msg->setHandlerFunc(_PREHASH_PlacesReply, process_places_reply);

	msg->setHandlerFunc(_PREHASH_GroupNoticesListReply,
						LLPanelGroupNotices::processGroupNoticesListReply);

	msg->setHandlerFunc(_PREHASH_DirPlacesReply,
						LLPanelDirBrowser::processDirPlacesReply);

	msg->setHandlerFunc(_PREHASH_DirPeopleReply,
						LLPanelDirBrowser::processDirPeopleReply);

	msg->setHandlerFunc(_PREHASH_DirEventsReply,
						LLPanelDirBrowser::processDirEventsReply);

	msg->setHandlerFunc(_PREHASH_DirGroupsReply,
						LLPanelDirBrowser::processDirGroupsReply);

	msg->setHandlerFunc(_PREHASH_DirClassifiedReply,
						LLPanelDirBrowser::processDirClassifiedReply);

	msg->setHandlerFunc(_PREHASH_DirLandReply,
						LLPanelDirBrowser::processDirLandReply);

	msg->setHandlerFunc(_PREHASH_AvatarPickerReply,
						LLFloaterAvatarPicker::processAvatarPickerReply);

	msg->setHandlerFunc(_PREHASH_MapLayerReply,
						LLWorldMap::processMapLayerReply);
	msg->setHandlerFunc(_PREHASH_MapBlockReply,
						LLWorldMap::processMapBlockReply);
	msg->setHandlerFunc(_PREHASH_MapItemReply,
						LLWorldMap::processMapItemReply);

	msg->setHandlerFunc(_PREHASH_EventInfoReply,
						LLPanelEvent::processEventInfoReply);
	msg->setHandlerFunc(_PREHASH_PickInfoReply,
						LLAvatarProperties::processPickInfoReply);

	msg->setHandlerFunc(_PREHASH_ClassifiedInfoReply,
						LLAvatarProperties::processClassifiedInfoReply);

	msg->setHandlerFunc(_PREHASH_ParcelInfoReply,
						LLViewerParcelMgr::processParcelInfoReply);

	msg->setHandlerFunc(_PREHASH_ScriptDialog, process_script_dialog);
	msg->setHandlerFunc(_PREHASH_LoadURL, process_load_url);

	msg->setHandlerFunc(_PREHASH_EstateCovenantReply, process_covenant_reply);

	// Calling cards
	msg->setHandlerFunc(_PREHASH_OfferCallingCard, process_offer_callingcard);
	msg->setHandlerFunc(_PREHASH_AcceptCallingCard,
						process_accept_callingcard);
	msg->setHandlerFunc(_PREHASH_DeclineCallingCard,
						process_decline_callingcard);

	msg->setHandlerFunc(_PREHASH_ParcelObjectOwnersReply,
						LLPanelLandObjects::processParcelObjectOwnersReply);

	msg->setHandlerFunc(_PREHASH_InitiateDownload, process_initiate_download);
	msg->setHandlerFunc(_PREHASH_LandStatReply,
						LLFloaterTopObjects::handleLandReply);
	msg->setHandlerFunc(_PREHASH_GenericMessage, process_generic_message);
	msg->setHandlerFunc(_PREHASH_GenericStreamingMessage,
						process_generic_streaming_message);
	msg->setHandlerFunc(_PREHASH_LargeGenericMessage,
						process_large_generic_message);

	msg->setHandlerFuncFast(_PREHASH_FeatureDisabled,
							process_feature_disabled_message);
}

// *HACK: Must match names in Library or agent inventory
const std::string COMMON_GESTURES_FOLDER = "Common Gestures";
const std::string MALE_GESTURES_FOLDER = "Male Gestures";
const std::string FEMALE_GESTURES_FOLDER = "Female Gestures";
const std::string MALE_OUTFIT_FOLDER = "Male Shape & Outfit";
const std::string FEMALE_OUTFIT_FOLDER = "Female Shape & Outfit";

constexpr S32 OPT_MALE = 0;
constexpr S32 OPT_FEMALE = 1;

//static
bool LLStartUp::callbackChooseGender(const LLSD& notification,
									 const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) == OPT_MALE)
	{
		loadInitialOutfit(MALE_OUTFIT_FOLDER, "male");
	}
	else
	{
		loadInitialOutfit(FEMALE_OUTFIT_FOLDER, "female");
	}
	return false;
}

//static
void LLStartUp::loadInitialOutfit(const std::string& outfit_folder_name,
								  const std::string& gender_name)
{
	S32 gender = 0;
	std::string gestures;
	if (gender_name == "male")
	{
		gender = OPT_MALE;
		gestures = MALE_GESTURES_FOLDER;
	}
	else
	{
		gender = OPT_FEMALE;
		gestures = FEMALE_GESTURES_FOLDER;
	}

	// Try to find the outfit: if not there, create some default wearables.
	LLInventoryModel::cat_array_t cat_array;
	LLInventoryModel::item_array_t item_array;
	LLNameCategoryCollector has_name(outfit_folder_name);
	gInventory.collectDescendentsIf(LLUUID::null, cat_array, item_array,
									LLInventoryModel::EXCLUDE_TRASH,
									has_name);
	if (cat_array.empty())
	{
		gAgentWearables.createStandardWearables(gender);
	}
	else
	{
		gAppearanceMgr.wearOutfitByName(outfit_folder_name);
	}
	gAppearanceMgr.wearOutfitByName(gestures);
	gAppearanceMgr.wearOutfitByName(COMMON_GESTURES_FOLDER);

	// This is really misnamed -- it means we have started loading an
	// outfit/shape that will give the avatar a gender eventually. JC
	gAgent.setGenderChosen(true);
}

// Loads a bitmap to display during load
// location_id = 0 => last position
// location_id = 1 => home position
//static
void LLStartUp::initStartScreen(S32 location_id)
{
	if (gStartTexture.notNull())
	{
		gStartTexture = NULL;
		llinfos << "Re-initializing start screen" << llendl;
	}

	LL_DEBUGS("AppInit") << "Loading startup bitmap..." << LL_ENDL;

	std::string temp_str = gDirUtilp->getLindenUserDir() + LL_DIR_DELIM_STR;
	if (!gIsInProductionGrid)
	{
		temp_str += SCREEN_LAST_BETA_FILENAME;
	}
	else if ((S32)START_LOCATION_ID_LAST == location_id)
	{
		temp_str += SCREEN_LAST_FILENAME;
	}
	else
	{
		temp_str += SCREEN_HOME_FILENAME;
	}

	LLPointer<LLImageBMP> start_image_bmp = new LLImageBMP;

	if (!start_image_bmp->load(temp_str))
	{
		return;
	}
	llinfos << "Loaded bitmap: " << temp_str << llendl;

	gStartImageWidth = start_image_bmp->getWidth();
	gStartImageHeight = start_image_bmp->getHeight();

	LLPointer<LLImageRaw> raw = new LLImageRaw;
	if (!start_image_bmp->decode(raw))
	{
		llwarns << "Bitmap decode failed" << llendl;
		gStartTexture = NULL;
		return;
	}

	raw->expandToPowerOfTwo();
	gStartTexture = LLViewerTextureManager::getLocalTexture(raw.get(), false);
}

//static
std::string LLStartUp::startupStateToString(EStartupState state)
{
#define RTNENUM(E) case E: return #E
	switch (state)
	{
		RTNENUM(STATE_FIRST);
		RTNENUM(STATE_BROWSER_INIT);
		RTNENUM(STATE_LOGIN_SHOW);
		RTNENUM(STATE_TPV_FIRST_USE);
		RTNENUM(STATE_LOGIN_WAIT);
		RTNENUM(STATE_LOGIN_CLEANUP);
		RTNENUM(STATE_UPDATE_CHECK);
		RTNENUM(STATE_LOGIN_AUTH_INIT);
		RTNENUM(STATE_XMLRPC_LOGIN);
		RTNENUM(STATE_LOGIN_NO_DATA_YET);
		RTNENUM(STATE_LOGIN_DOWNLOADING);
		RTNENUM(STATE_LOGIN_PROCESS_RESPONSE);
		RTNENUM(STATE_WORLD_INIT);
		RTNENUM(STATE_MULTIMEDIA_INIT);
		RTNENUM(STATE_SEED_GRANTED_WAIT);
		RTNENUM(STATE_SEED_CAP_GRANTED);
		RTNENUM(STATE_WORLD_WAIT);
		RTNENUM(STATE_AGENT_SEND);
		RTNENUM(STATE_AGENT_WAIT);
		RTNENUM(STATE_INVENTORY_SEND);
		RTNENUM(STATE_MISC);
		RTNENUM(STATE_PRECACHE);
		RTNENUM(STATE_WEARABLES_WAIT);
		RTNENUM(STATE_CLEANUP);
		RTNENUM(STATE_STARTED);
	default:
		return llformat("(state #%d)", state);
	}
#undef RTNENUM
}

//static
void LLStartUp::setStartupState(EStartupState state)
{
	llinfos << "Startup state changing from "
			<< startupStateToString(sStartupState) << " to "
			<< startupStateToString(state) << llendl;
	sStartupState = state;
}

//static
void LLStartUp::resetLogin()
{
	// Save URL history file. This needs to be done on login failure because it
	// gets read on *every* login attempt
	LLURLHistory::saveFile("url_history.xml");

	LLStartUp::setStartupState(STATE_LOGIN_SHOW);

	if (gViewerWindowp)
	{
		// Hide menus and normal buttons
		gViewerWindowp->setNormalControlsVisible(false);
		gLoginMenuBarViewp->setVisible(true);
		gLoginMenuBarViewp->setEnabled(true);
	}

	// Hide any other stuff
	LLFloaterMiniMap::hideInstance();
}

// Initialize all plug-ins except the web browser (which was initialized early,
// before the login screen). JC
//static
void LLStartUp::multimediaInit()
{
	LL_DEBUGS("AppInit") << "Initializing Multimedia...." << LL_ENDL;
	setStartupStatus(0.42f,
					 LLTrans::getString("LoginInitializingMultimedia"),
					 gAgent.mMOTD);
	display_startup();

#if 0	// Done in LLAppViewer::init()
	LLViewerMedia::initClass();
#endif
	LLViewerParcelMedia::initClass();
}

//static
bool LLStartUp::dispatchURL()
{
	// OK, if we have gotten this far and have a startup URL
	if (sStartSLURL.isSpatial())
	{
		// If we started with a location, but we are already at that location,
		// do not pop dialogs open.
		LLVector3 pos = gAgent.getPositionAgent();
		LLVector3 slurlpos = sStartSLURL.getPosition();
		F32 dx = pos.mV[VX] - slurlpos.mV[VX];
		F32 dy = pos.mV[VY] - slurlpos.mV[VY];
		constexpr F32 SLOP = 2.f;	// meters

		std::string region_name;
		LLViewerRegion* regionp = gAgent.getRegion();
		if (regionp)
		{
			region_name = regionp->getName();
		}
		if (getStartSLURL().getRegion() != region_name ||
			dx * dx > SLOP * SLOP || dy * dy > SLOP * SLOP)
		{
			std::string url = getStartSLURL().getSLURLString();
			LLMediaCtrl* web = NULL;
			LLURLDispatcher::dispatch(url, "clicked", web, false);
		}

		return true;
	}

	return false;
}

//static
S32 LLStartUp::setStartSLURL(const LLSLURL& slurl)
{
	if (slurl.isSpatial())
	{
		std::string new_start = slurl.getSLURLString();
		LL_DEBUGS("Login") << "Startup SLURL: " << new_start << LL_ENDL;
		sStartSLURL = slurl;
		LLPanelLogin::refreshLocation(); // Updates grid if needed

		// Remember that this is where we wanted to log in... If the login
		// fails, the next attempt will default to the same place.
		gSavedSettings.setString("NextLoginLocation", new_start);
	}
	else if (slurl.getType() == LLSLURL::APP && slurl.getAppCmd() == "login")
	{
		LL_DEBUGS("Login") << "Loging SLURL: " << slurl.getSLURLString()
						   << LL_ENDL;
		sLoginSLURL = slurl;
	}

	return (S32)slurl.getType();
}

//static
bool LLStartUp::loginAlertDone(const LLSD&, const LLSD&)
{
	LLPanelLogin::giveFocus();
	return false;
}

/**
 * Read all proxy configuration settings and set up both the HTTP proxy and
 * SOCKS proxy as needed.
 *
 * Any errors that are encountered will result in showing the user a
 * notification.
 *
 * @return Returns true if setup was successful, false if an error was
 * encountered.
 */
//static
bool LLStartUp::startLLProxy()
{
	bool proxy_ok = true;
	std::string proxy_type = gSavedSettings.getString("HttpProxyType");

	// Set up SOCKS proxy, if needed
	if (gSavedSettings.getBool("Socks5ProxyEnabled"))
	{
		// Determine and update LLProxy with the saved authentication system
		std::string auth_type = gSavedSettings.getString("Socks5AuthType");
		if (auth_type.compare("UserPass") == 0)
		{
			std::string socks_user = gSavedSettings.getString("Socks5Username");
			std::string socks_password = gSavedSettings.getString("Socks5Password");
			bool ok = LLProxy::getInstance()->setAuthPassword(socks_user,
															  socks_password);
			if (!ok)
			{
				gNotifications.add("SOCKS_BAD_CREDS");
				proxy_ok = false;
			}
		}
		else if (auth_type.compare("None") == 0)
		{
			LLProxy::getInstance()->setAuthNone();
		}
		else
		{
			// Unknown or missing setting.
			llwarns << "Invalid SOCKS 5 authentication type." << llendl;
			gSavedSettings.setString("Socks5AuthType", "None");
			LLProxy::getInstance()->setAuthNone();
		}

		if (proxy_ok)
		{
			// Start the proxy and check for errors. If status != SOCKS_OK,
			// stopSOCKSProxy() will already have been called when
			// startSOCKSProxy() returns.
			LLHost socks_host;
			socks_host.setHostByName(gSavedSettings.getString("Socks5ProxyHost"));
			socks_host.setPort(gSavedSettings.getU32("Socks5ProxyPort"));
			int status = LLProxy::getInstance()->startSOCKSProxy(socks_host);
			if (status != SOCKS_OK)
			{
				LLSD args;
				args["HOST"] = gSavedSettings.getString("Socks5ProxyHost");
				args["PORT"] = (S32)gSavedSettings.getU32("Socks5ProxyPort");

				std::string error_string;

				switch (status)
				{
					case SOCKS_CONNECT_ERROR:
						// TCP Fail
						error_string = "SOCKS_CONNECT_ERROR";
						break;

					case SOCKS_NOT_PERMITTED:
						// SOCKS 5 server rule set refused connection
						error_string = "SOCKS_NOT_PERMITTED";
						break;

					case SOCKS_NOT_ACCEPTABLE:
						// Selected authentication is not acceptable to server
						error_string = "SOCKS_NOT_ACCEPTABLE";
						break;

					case SOCKS_AUTH_FAIL:
						// Authentication failed
						error_string = "SOCKS_AUTH_FAIL";
						break;

					case SOCKS_UDP_FWD_NOT_GRANTED:
						// UDP forward request failed
						error_string = "SOCKS_UDP_FWD_NOT_GRANTED";
						break;

					case SOCKS_HOST_CONNECT_FAILED:
						// Failed to open a TCP channel to the socks server
						error_string = "SOCKS_HOST_CONNECT_FAILED";
						break;

					case SOCKS_INVALID_HOST:
						// Improperly formatted host address or port
						error_string = "SOCKS_INVALID_HOST";
						break;

					default:
						// Something strange happened
						error_string = "SOCKS_UNKNOWN_STATUS";
						llwarns << "Unknown return from LLProxy::startProxy(): "
								<< status << llendl;
						break;
				}

				gNotifications.add(error_string, args);
				proxy_ok = false;
			}
		}
	}
	else
	{
		// ensure no UDP proxy is running and it's all cleaned up
		LLProxy::getInstance()->stopSOCKSProxy();
	}

	if (proxy_ok)
	{
		// Determine the HTTP proxy type (if any)
		if (proxy_type.compare("Web") == 0 &&
			gSavedSettings.getBool("BrowserProxyEnabled"))
		{
			LLHost http_host;
			http_host.setHostByName(gSavedSettings.getString("BrowserProxyAddress"));
			http_host.setPort(gSavedSettings.getS32("BrowserProxyPort"));
			if (!LLProxy::getInstance()->enableHTTPProxy(http_host,
														 LLPROXY_HTTP))
			{
				LLSD args;
				args["HOST"] = http_host.getIPString();
				args["PORT"] = (S32)http_host.getPort();
				gNotifications.add("PROXY_INVALID_HTTP_HOST", args);
				proxy_ok = false;
			}
		}
		else if (proxy_type.compare("Socks") == 0 &&
				 gSavedSettings.getBool("Socks5ProxyEnabled"))
		{
			LLHost socks_host;
			socks_host.setHostByName(gSavedSettings.getString("Socks5ProxyHost"));
			socks_host.setPort(gSavedSettings.getU32("Socks5ProxyPort"));
			if (!LLProxy::getInstance()->enableHTTPProxy(socks_host,
														 LLPROXY_SOCKS))
			{
				LLSD args;
				args["HOST"] = socks_host.getIPString();
				args["PORT"] = (S32)socks_host.getPort();
				gNotifications.add("PROXY_INVALID_SOCKS_HOST", args);
				proxy_ok = false;
			}
		}
		else if (proxy_type.compare("None") == 0)
		{
			LLProxy::getInstance()->disableHTTPProxy();
		}
		else
		{
			llwarns << "Invalid other HTTP proxy configuration."<< llendl;

			// Set the missing or wrong configuration back to something valid.
			gSavedSettings.setString("HttpProxyType", "None");
			LLProxy::getInstance()->disableHTTPProxy();

			// Leave proxy_ok alone, since this isn't necessarily fatal.
		}
	}

	return proxy_ok;
}
