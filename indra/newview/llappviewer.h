/**
 * @file llappviewer.h
 * @brief The LLAppViewer class declaration
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 *
 * Copyright (c) 2007-2009, Linden Research, Inc.
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

#ifndef LL_LLAPPVIEWER_H
#define LL_LLAPPVIEWER_H

#include "llapp.h"

#include "llappcorehttp.h"

#if LL_LINUX
// Under Linux, calling viewer code from within a DBus callback dead-locked the
// next gtk_main() call when the viewer was made gdk threads aware (i.e. with
// gdk_threads_init() used, which allowed a non-blocking GTK file selector).
// Note that since we do not use GDK/GTK any more now in the Cool VL Viewer,
// this code seems to work again. Yet, it is super-dirty to call code that may
// reenter glib (in the render loop, via LLWindowSDL) in a glib callback, so I
// decided to keep it disabled. HB
# define LL_CALL_SLURL_DISPATCHER_IN_CALLBACK 0
#endif

class LLCommandLineParser;
class LLEventPump;
class LLFile;
class LLPumpIO;
class LLThreadPool;
class LLWorkQueue;

class LLAppViewer : public LLApp
{
protected:
	LOG_CLASS(LLAppViewer);

public:
	LLAppViewer();
	virtual ~LLAppViewer();

	//
	// Main application logic
	//
	virtual InitState init();	// Override to do application initialization
	virtual bool cleanup();		// Override to do application cleanup

	// Override for the application main loop. Needs to at least gracefully
	// notice the QUITTING state and exit.
	virtual bool mainLoop();

	enum EExitCode
	{
		EXIT_OK = 0,
		EXIT_INIT_FAILED,
		EXIT_CODE_BASH_RESERVED,	// Reserved for bash: do not use !
		EXIT_LOGIN_FAILED,
		EXIT_FORCE_LOGGED_OUT,
		VIEWER_EXIT_CODES			// Start of user-defined codes (e.g. Lua)
	};

	// Application control
	void forceQuit();	// Puts the viewer into 'shutting down without error' mode.
	void requestQuit();	// Request a quit. A kinder, gentler quit.
	void userQuit();	// The users asks to quit. Confirm, then requestQuit()

	// Displays an error dialog and forcibly quit.
	void earlyExit(const std::string& name,
				   const LLSD& substitutions = LLSD());

	void forceExit();	// exit(-1) immediately (after minimal cleanup).
	void abortQuit();	// Called to abort a quit request.

	LL_INLINE bool quitRequested()						{ return mQuitRequested; }
	LL_INLINE bool logoutRequestSent()					{ return mLogoutRequestSent; }

	LL_INLINE bool isSecondInstance()					{ return mSecondInstance; }
	LL_INLINE bool isSecondInstanceSiblingViewer()		{ return mIsSiblingViewer; }

	void idleAFKCheck(bool force_afk = false);

	void writeDebugInfo(bool log_interesting_info = true);

	// Report true if under the control of a debugger. A null-op default.
	LL_INLINE virtual bool beingDebugged()				{ return false; }

	// Plateform-specific Vulkan driver presence and API version detections.
	virtual bool probeVulkan(std::string& version) = 0;

	// Require platform-specific override to reset error handling mechanism.
	// Returns false if the error trap needed restoration.
	virtual bool restoreErrorTrap() = 0;

	// Any low-level crash-prep that has to happen in the context of the
	// crashing thread before the crash report is delivered:
	virtual void handleSyncCrashTrace() = 0;
	// Hey !  The viewer crashed. Do this, soon:
	static void handleViewerCrash();
	// Hey !  The viewer crashed. Do this right NOW in the context of the
	// crashing thread:
	static void handleSyncViewerCrash();

	LL_INLINE const std::string& getSerialNumber()		{ return mSerialNumber; }

	LL_INLINE bool getPurgeCache() const				{ return mPurgeCache; }

	// Forces disconnection, with a message to the user.
	void forceDisconnect(const std::string& msg);

	// Causes a crash state due to bad network packet.
	void badNetworkHandler();

	LL_INLINE bool hasSavedFinalSnapshot()				{ return mSavedFinalSnapshot; }
	void saveFinalSnapshot();

	void loadNameCache();
	void saveNameCache();

	void loadExperienceCache();
	void saveExperienceCache();

	void stampMarkerFile(LLFile* marker_file);
	bool isOurMarkerFile(std::string& filename);
	void checkSiblingMarkerFile(std::string& filename);
	void removeMarkerFile(bool leave_logout_marker = false);

	// Load settings from the location specified by loction_key.
	// Key availale and rules for loading, are specified in
	// 'app_settings/settings_files.xml'
	bool loadSettingsFromDirectory(const std::string& location_key,
								   bool set_defaults = false);

	std::string getSettingsFilename(const std::string& location_key,
									const std::string& file);

	// Saves the global settings, on the condition that we are the first
	// Cool VL Viewer running instance.
	void saveGlobalSettings();

	// Handle the 'login completed' event.
	void handleLoginComplete();

	// llcorehttp init/shutdown/config information.
	LL_INLINE LLAppCoreHttp& getAppCoreHttp()			{ return mAppCoreHttp; }

	// Called from llviewercontrol.cpp when UserLogFile is in use
	LL_INLINE void clearLogFilename()					{ mLogFileName.clear(); }

	static U32 getSettingU32(const std::string& name);
	static void setSettingU32(const std::string& name, U32 value);

	static void pauseTextureFetch();
	static void updateTextureFetch();

#if LL_WINDOWS
	bool isRunningUnderWine() const					{ return mUnderWine; }
#endif

protected:
	virtual bool initWindow();	// Initialize the viewer's window.
	virtual void initLogging();	// Initialize log files, logging system.

	static void errorCallback(const std::string& error_string);

	// Rename the log as appropriate on exit and depending whether this
	// viewer is/was the first running instance or not.
	// This must be called both on start-up and on exit (on start-up, if we
	// are a new instance).
	void renameLog(bool on_exit);

	// Initialize OS level debugging console:
	LL_INLINE virtual void initConsole()				{}

	// A false result indicates the app should quit:
	LL_INLINE virtual bool initHardwareTest()			{ return true; }

	virtual bool initAppMessagesHandler();
	virtual bool sendURLToOtherInstance(const std::string& url);
#if LL_LINUX && !LL_CALL_SLURL_DISPATCHER_IN_CALLBACK
	virtual const std::string& getReceivedSLURL() = 0;
	virtual void clearReceivedSLURL() = 0;
#endif

	// Allows platforms to specify the command line args.
	LL_INLINE virtual bool initParseCommandLine(LLCommandLineParser& clp)
	{
		return true;
	}

	// Platforms specific classes generate this.
	virtual std::string generateSerialNumber() = 0;

private:
	// Initializes viewer threads.
	void initThreads();
	// Initializes settings from the command line/config file:
	InitState initConfiguration();

	void initGridChoice();

	bool initCache(); // Initialize local client cache.
	void purgeCache(); // Clear the local cache.

	void checkMemory();

	// We have switched locations of both Mac and Windows cache, make sure
	// files migrate and old cache is cleared out.
	void migrateCacheDirectory();

	// Sets some config data to current or default values during cleanup:
	void cleanupSavedSettings();
	// Deletes cached files the match the given wildcard:
	void removeCacheFiles(const char* filemask);

	// Writes system info to "debug_info.log":
	void writeSystemInfo();

	// This shall be called only once, at early initialization time.
	bool anotherInstanceRunning();
	// Called only when we are the only running instance.
	void initMarkerFile();

	void frame(LLEventPump& mainloop);

	void idle(bool run_rlv_maintenance = true);
	void idleShutdown();
	void idleNameCache();
	void idleNetwork();

	void sendLogoutRequest();
	void disconnectViewer();

public:
	// *NOTE: There are currently 3 settings files: "Global" and "PerAccount"
	// The list is found in app_settings/settings_files.xml but since they are
	// used explicitly in code, the follow consts should also do the trick.
	static const std::string	sGlobalSettingsName;
	static const std::string	sPerAccountSettingsName;

private:
	LLThreadPool*				mGeneralThreadPool;

	// For tracking viewer<->region circuit death
	LLUUID						mAgentRegionLastID;

	LLSD						mSettingsLocationList;

	std::string					mSerialNumber;

	// llcorehttp library init/shutdown helper
	LLAppCoreHttp				mAppCoreHttp;

	F32							mMainloopTimeoutDefault;

	U32							mLastAgentControlFlags;
	F32							mLastAgentForceUpdate;

	std::string					mLogFileName;		// Our log file name.

	std::string					mMarkerFileName;
	// A file created to indicate the app is running.
	LLFile*						mMarkerFile;

	// A file created to indicate the app is running.
	std::string					mLogoutMarkerFileName;
	bool						mOwnsLogoutMarkerFile;

	bool						mPurgeCache;
	bool						mPurgeOnExit;

	bool						mSavedFinalSnapshot;
	// Only save per account settings if login succeeded
	bool						mSavePerAccountSettings;
	// User wants to quit, may have modified documents open.
	bool						mQuitRequested;
	// Disconnect message sent to simulator, no longer safe to send messages to
	// the sim.
	bool 						mLogoutRequestSent;
	// For tracking viewer<->region circuit death
	bool						mAgentRegionLastAlive;

	// Is this another instance of a SL viewer ?
	bool						mSecondInstance;
	// Is this a second instance of our viewer ?
	bool						mIsOurViewer;
	// Is that 2nd instance another version of the same branch of our viewer ?
	bool						mSameBranchViewer;
	// Is that 2nd instance an entirely different version of our viewer ?
	bool						mIsSiblingViewer;
#if LL_WINDOWS
	// Flag set when running a Windows build under Wine. HB
	bool						mUnderWine;
#endif
};

constexpr F32 AGENT_UPDATES_PER_SECOND = 10.f;
constexpr F32 AGENT_FORCE_UPDATES_PER_SECOND = 1.f;

// The LLAppViewer singleton is created in main()/WinMain(). So do not use it
// in pre-entry (static initialization) code.
extern LLAppViewer*			gAppViewerp;

// Globals with external linkage. From viewer.h
//
// "// llstartup" indicates that llstartup is the only client for this global.

extern LLSD					gDebugInfo;

typedef enum
{
	LAST_EXEC_NORMAL = 0,
	LAST_EXEC_FROZE,
	LAST_EXEC_LLERROR_CRASH,
	LAST_EXEC_OTHER_CRASH,
	LAST_EXEC_LOGOUT_FROZE,
	LAST_EXEC_LOGOUT_CRASH
} eLastExecEvent;

extern eLastExecEvent		gLastExecEvent;

extern U32					gFrameCount;
extern U32					gForegroundFrameCount;

extern LLPumpIO*			gServicePumpIOp;
extern LLWorkQueue*			gMainloopWorkp;

extern S32					gExitCode;

// The timestamp of the most-recently-processed frame
extern U64					gFrameTime;
// Loses msec precision after ~4.5 hours...
extern F32					gFrameTimeSeconds;
// Elapsed time between current and previous gFrameTimeSeconds
extern F32					gFrameIntervalSeconds;
// Frames per second, smoothed, weighted toward last frame
extern F32					gFPSClamped;
extern F32					gFrameDT;
extern U64					gStartTime;
extern U32					gFrameSleepTime;

extern LLTimer				gRenderStartTime;
extern LLFrameTimer			gForegroundTime;

extern F32					gLogoutMaxTime;
extern LLTimer				gLogoutTimer;

extern F32					gSimLastTime;
extern F32					gSimFrames;

extern bool					gAvatarMovedOnLogin;
extern bool					gLogoutInProgress;
extern bool					gDisconnected;

extern LLFrameTimer			gRestoreGLTimer;
extern bool					gRestoreGL;
extern bool					gUseWireframe;

extern bool					gEnableFastTimers;

// Memory checks
extern LLFrameTimer			gMemoryCheckTimer;
extern U64					gMemoryAllocated;

extern bool					gBalanceObjectCache;

extern std::string			gLastVersionChannel;

extern LLVector3			gWindVec;
extern LLVector3			gRelativeWindVec;
extern U32					gPacketsIn;

extern std::string			gSecondLife;
extern std::string			gViewerVersionString;
extern U64					gViewerVersionNumber;
// Set after settings are loaded
extern std::string			gCurrentVersion;

extern std::string			gWindowTitle;

// Plugin presence flag
extern bool					gHasGstreamer;

extern bool					gAllowTapTapHoldRun;
extern bool					gShowObjectUpdates;

extern bool					gAcceptTOS;
extern bool					gAcceptCriticalMessage;

constexpr size_t MAC_ADDRESS_BYTES = 6;
extern unsigned char		gMACAddress[MAC_ADDRESS_BYTES];

#endif // LL_LLAPPVIEWER_H
