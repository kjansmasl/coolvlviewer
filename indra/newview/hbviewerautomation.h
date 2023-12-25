/**
 * @file hbviewerautomation.h
 * @brief HBViewerAutomation class definition
 *
 * $LicenseInfo:firstyear=2016&license=viewergpl$
 *
 * Copyright (c) 2016-2023, Henri Beauchamp.
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

#ifndef LL_HBVIEWERAUTOMATION_H
#define LL_HBVIEWERAUTOMATION_H

#include <deque>
#include <set>

#include "boost/signals2.hpp"

#include "llmenugl.h"
#include "llmutex.h"
#include "llpanel.h"
#include "lltimer.h"
#include "llvector3d.h"

// Work in progress for future features not finalized/fully specified. HB
#define HB_LUA_FLOATER_FUNCTIONS 0

class HBAutomationThread;
class LLColor4;
class LLControlVariable;
class LLFriendObserver;
class LLMessageSystem;
class LLPickInfo;
class LLSimInfo;
class LLViewerObject;
struct lua_Debug;
struct lua_State;

class HBViewerAutomation
{
	friend class HBLuaDialog;
	friend class HBLuaFloater;
	friend class HBLuaPieMenu;
	friend class HBIgnoreCallback;

protected:
	LOG_CLASS(HBViewerAutomation);

public:
	// Methods used for the resident gAutomationp interpreter in which the
	// automation script gets loaded
	static void start(std::string file_name = LLStringUtil::null);
	static void cleanup();

	// Method used to spawn transient interpreters for commands sent via the
	// chat input line, the llOwnersay() and llInstantMessage() LSL functions,
	// and the D-Bus commands. When use_print_buffer is true (used for D-Bus
	// commands), all print() and reportError() outputs are redirected to the
	// mPrintBuffer string and the contents of that string is returned by this
	// function (and otherwise an empty string).
	// When the command comes from an object script, that object's 'id' and
	// 'name' are passed (by checkLuaCommand()).
	// Note that viewer events are not transmitted to this type of interpreter.
	static std::string eval(const std::string& chunk,
							bool use_print_buffer = false,
							const LLUUID& id = LLUUID::null,
							const std::string& name = LLStringUtil::null);

	// Method used to interpret possible Lua commands in script messages.
	// Returns true if the message was indeed a Lua command.
	static bool checkLuaCommand(const std::string& message,
								const LLUUID& from_object_id,
								const std::string& from_object_name);

	// Method used to execute a Lua script file
	static void execute(const std::string& file_name);

	// Events transmitted by the viewer to the Lua automation script
	void onLogin();
	void onRegionChange();
	void onParcelChange();
	void onPositionChange(const LLVector3& pos_local,
						  const LLVector3d& pos_global);
	void onAveragedFPS(F32 fps, bool limited, F32 frame_render_time);
	void onAgentOccupationChange(S32 type);
	void onAgentPush(const LLUUID& id, S32 type, F32 mag);
	bool onSendChat(std::string& text);
	void onReceivedChat(U8 chat_type, const LLUUID& from_id,
						const std::string& name, const std::string& text);
	bool onChatTextColoring(const LLUUID& from_id, const std::string& name,
							const std::string& text, LLColor4& color);
	void onInstantMsg(const LLUUID& session_id, const LLUUID& origin_id,
					  const std::string& name, const std::string& text);
	void onScriptDialog(const LLUUID& notif_id, const std::string& message,
						const std::vector<std::string>& buttons);
	void onNotification(const std::string& dialog_name, const LLUUID& notif_id,
						const std::string& message);
	void onFriendStatusChange(const LLUUID& id, U32 mask, bool is_online);
	void onAvatarRezzing(const LLUUID& id);
	void onAgentBaked();
	void onRadar(const LLUUID& id, const std::string& name, S32 range,
				 bool marked);
	void onRadarSelection(const uuid_vec_t& ids);
	void onRadarMark(const LLUUID& id, const std::string& name, bool marked);
	void onRadarTrack(const LLUUID& id, const std::string& name, bool tracked);
	void onSideBarVisibilityChange(bool visible);
	void onAutoPilotFinished(const std::string& type, bool reached,
							 bool user_cancel);
	void onTPStateChange(S32 state, const std::string& reason);
	void onFailedTPSimChange(S32 agents_count);
	void onWindlightChange(const std::string& sky_settings_name,
						   const std::string& water_settings_name,
						   const std::string& day_settings_name);
	void onCameraModeChange(S32 mode);
	void onJoystickButtons(S32 old_state, S32 new_state);
	void onRLVHandleCommand(const LLUUID& object_id, const std::string& behav,
							const std::string& option,
							const std::string& param);
	void onRLVAnswerOnChat(const LLUUID& object_id, S32 channel,
						   const std::string& text);

	// Called by llviermessage.cpp when receiving object properties messages
	static void processObjectPropertiesFamily(LLMessageSystem* msg);

	// Called from llagent.cpp to keep track of the agent positions history
	static void addToAgentPosHistory(const LLVector3d& global_pos);

protected:
	HBViewerAutomation(bool use_print_buffer = false);
	virtual ~HBViewerAutomation();

	LL_INLINE virtual bool isThreaded() const		{ return false; }
	LL_INLINE virtual U32 getLuaThreadID() const	{ return 0; }

	static HBViewerAutomation* findInstance(lua_State* state);

	void resetCallbackFlags();

	bool load(const std::string& file_name);
	bool loadString(const std::string& chunk);

	void reportError();
	// Callback method for the Lua warn() function
	static void reportWarning(void* data, const char* msg, int to_continue);

	bool registerCFunctions();	// Returns true on success
	S32 getGlobal(const std::string& global);

	void resetTimer();

	void pushGridSimAndPos();
	void pushParcelInfo();

	// Method used to pre-process Lua source files
	std::string preprocess(const std::string& file_name);

	// Callback for use with HBPreprocessor to load #include Lua files
	static S32 loadInclude(std::string& include_name, const std::string& path,
						   std::string& buffer, void*);

	// Callback for use with HBPreprocessor to report warning and error
	// messages
	static void preprocessorMessageCB(const std::string& message,
									  bool is_warning, void*);

	static bool callAutomationFunc(HBAutomationThread* threadp);

	// Idle callbacks.
	static void onIdleThread(void* userdata);
	static void onIdleSimChange(void* userdata);

	// Helper method to request object details. 'reason' must be 0 for muting,
	// 1 for un-muting, anything else for Lua GetObjectInfo().
	static bool requestObjectPropertiesFamily(const LLUUID& object_id,
											  U32 reason);

	// Helper method to find an item or category UUID from its full path name
	// in the inventory. The path separator is the pipe symbol ('|') and was
	// choosen because it is not a valid/accepted character for inventory
	// objects names).
	static const LLUUID& getInventoryObjectId(const std::string& name,
											  bool& is_category);

	// Watchdog timeout hook
	static void watchdog(lua_State* state, lua_Debug*);

	// Overridden print() Lua function
	static int print(lua_State* state);

	// New viewer-related Lua functions:
	static int hasThread(lua_State* state);
	static int startThread(lua_State* state);
	static int stopThread(lua_State* state);
	static int sendSignal(lua_State* state);
	static int getSourceFileName(lua_State* state);
	static int getWatchdogState(lua_State* state);
	static int isUUID(lua_State* state);
	static int isAvatar(lua_State* state);
	static int isObject(lua_State* state);
	static int isAgentFriend(lua_State* state);
	static int isAgentGroup(lua_State* state);
	static int getAvatarName(lua_State* state);
	static int getGroupName(lua_State* state);
	static int isAdmin(lua_State* state);
	static int getRadarData(lua_State* state);
	static int setRadarTracking(lua_State* state);
	static int setRadarToolTip(lua_State* state);
	static int setRadarMarkChar(lua_State* state);
	static int setRadarMarkColor(lua_State* state);
	static int setRadarNameColor(lua_State* state);
	static int setAvatarMinimapColor(lua_State* state);
	static int setAvatarNameTagColor(lua_State* state);
	static int getAgentPosHistory(lua_State* state);
	static int getAgentInfo(lua_State* state);
	static int setAgentOccupation(lua_State* state);
	static int getAgentGroupData(lua_State* state);
	static int setAgentGroup(lua_State* state);
	static int agentGroupInvite(lua_State* state);
	static int agentSit(lua_State* state);
	static int agentStand(lua_State* state);
	static int setAgentTyping(lua_State* state);
	static int sendChat(lua_State* state);
	static int getIMSession(lua_State* state);
	static int closeIMSession(lua_State* state);
	static int sendIM(lua_State* state);
	static int scriptDialogResponse(lua_State* state);
	static int cancelNotification(lua_State* state);
	static int getObjectInfo(lua_State* state);
	static int browseToURL(lua_State* state);
	static int dispatchSLURL(lua_State* state);
	static int executeRLV(lua_State* state);
	static int openNotification(lua_State* state);
	static int openFloater(lua_State* state);
	static int closeFloater(lua_State* state);
#if HB_LUA_FLOATER_FUNCTIONS
	static int getFloaterInstances(lua_State* state);
	static int getFloaterButtons(lua_State* state);
	static int getFloaterCheckBoxes(lua_State* state);
	static int showFloater(lua_State* state);
#endif
	static int makeDialog(lua_State* state);
	static int openLuaFloater(lua_State* state);
	static int showLuaFloater(lua_State* state);
	static int setLuaFloaterCommand(lua_State* state);
	static int getLuaFloaterValue(lua_State* state);
	static int getLuaFloaterValues(lua_State* state);
	static int setLuaFloaterValue(lua_State* state);
	static int setLuaFloaterEnabled(lua_State* state);
	static int setLuaFloaterVisible(lua_State* state);
	static int closeLuaFloater(lua_State* state);
	static int overlayBarLuaButton(lua_State* state);
	static int statusBarLuaIcon(lua_State* state);
	static int sideBarButton(lua_State* state);
	static int sideBarButtonToggle(lua_State* state);
	static int sideBarHide(lua_State* state);
	static int sideBarHideOnRightClick(lua_State* state);
	static int sideBarButtonHide(lua_State* state);
	static int sideBarButtonDisable(lua_State* state);
	static int luaPieMenuSlice(lua_State* state);
	static int luaContextMenu(lua_State* state);
	static int pasteToContextHandler(lua_State* state);
	static int automationMessage(lua_State* state);
	static int automationRequest(lua_State* state);
	static int playUISound(lua_State* state);
	static int renderDebugInfo(lua_State* state);
	static int getDebugSetting(lua_State* state);
	static int setDebugSetting(lua_State* state);
	static int getFrameTimeSeconds(lua_State* state);
	static int getTimeStamp(lua_State* state);
	static int getClipBoardString(lua_State* state);
	static int setClipBoardString(lua_State* state);
	static int findInventoryObject(lua_State* state);
	static int giveInventory(lua_State* state);
	static int makeInventoryLink(lua_State* state);
	static int deleteInventoryLink(lua_State* state);
	static int newInventoryFolder(lua_State* state);
	static int listInventoryFolder(lua_State* state);
	static int moveToInventoryFolder(lua_State* state);
	static int pickInventoryItem(lua_State* state);
	static int pickAvatar(lua_State* state);
	static int getAgentAttachments(lua_State* state);
	static int getAgentWearables(lua_State* state);
	static int agentAutoPilotToPos(lua_State* state);
	static int agentAutoPilotFollow(lua_State* state);
	static int agentAutoPilotStop(lua_State* state);
	static int agentAutoPilotLoad(lua_State* state);
	static int agentAutoPilotSave(lua_State* state);
	static int agentAutoPilotRemove(lua_State* state);
	static int agentAutoPilotRecord(lua_State* state);
	static int agentAutoPilotReplay(lua_State* state);
	static int agentPuppetryStart(lua_State* state);
	static int agentPuppetryStop(lua_State* state);
	static int agentRotate(lua_State* state);
	static int getAgentRotation(lua_State* state);
	static int teleportAgentHome(lua_State* state);
	static int teleportAgentToPos(lua_State* state);
	static int getGridSimAndPos(lua_State* state);
	static int getParcelInfo(lua_State* state);
	static int getCameraMode(lua_State* state);
	static int setCameraMode(lua_State* state);
	static int setCameraFocus(lua_State* state);
	static int addMute(lua_State* state);
	static int removeMute(lua_State* state);
	static int isMuted(lua_State* state);
	static int blockSound(lua_State* state);
	static int isBlockedSound(lua_State* state);
	static int getBlockedSounds(lua_State* state);
	static int derenderObject(lua_State* state);
	static int getDerenderedObjects(lua_State* state);
	static int getAgentPushes(lua_State* state);
	static int applyDaySettings(lua_State* state);
	static int applySkySettings(lua_State* state);
	static int applyWaterSettings(lua_State* state);
	static int setDayTime(lua_State* state);
	static int getEESettingsList(lua_State* state);
	static int getWLSettingsList(lua_State* state);
	static int getEnvironmentStatus(lua_State* state);
	static int getGlobalData(lua_State* state);
	static int setGlobalData(lua_State* state);
	static int getPerAccountData(lua_State* state);
	static int setPerAccountData(lua_State* state);
	static int callbackAfter(lua_State* state);
	static int forceQuit(lua_State* state);
	static int minimizeWindow(lua_State* state);

	// This is the callback used by callbackAfter() via doAfterInterval()
	static void doAfterIntervalCallback(lua_State* state, int ref);

	// This is the callback used by onAgentBaked() via doAfterInterval()
	static void doCallOnAgentBaked(lua_State* state);

	// This is the callback for the inventory item picker.
	static void onPickInventoryItem(const std::vector<std::string>& names,
									const uuid_vec_t& ids, void* userdata,
									bool on_close);

	// This is the callback for the avatar picker.
	static void onPickAvatar(const std::vector<std::string>& names,
							 const uuid_vec_t& ids, void* userdata);

	// Helper methods used to de/serialize simple Lua tables from/into strings
	static bool serializeTable(lua_State* state, S32 stack_level = 1,
							   std::string* output = NULL);
	static bool deserializeTable(lua_State* state, std::string data);

	void onObjectInfoReply(const LLUUID& object_id, const std::string& name,
						   const std::string& desc, const LLUUID& owner_id,
						   const LLUUID& group_id);

	// Events transmitted to the Lua automation script by Lua UI elements
	void onLuaDialogClose(const std::string& title, S32 button,
						  const std::string& text);
	void onLuaFloaterAction(const std::string& floater_name,
							const std::string& ctrl_name,
							const std::string& value);
	void onLuaFloaterOpen(const std::string& floater_name,
						  const std::string& parameter);
	void onLuaFloaterClose(const std::string& floater_name,
						   const std::string& parameter);
	void onLuaPieMenu(U32 slice, S32 type, const LLPickInfo& pick);

	bool onContextMenu(U32 handler_id, S32 operation, const std::string& type);
	// This is the method passed as a callback to LLEditMenuHandler for custom
	// context menu entries.
	static void contextMenuCallback(HBContextMenuData* datap);

protected:
	lua_State*					mLuaState;

	// mFromObjectId is gAgentID unless the Lua interpreter is one set up for
	// a scripted object command, or (under Linux) for a D-Bus Lua command (in
	// which case mFromObjectId is set to sLuaDBusFakeObjectId).
	LLUUID						mFromObjectId;
	std::string					mFromObjectName;

	std::string					mSourceFileName;

	LLTimer						mWatchdogTimer;
	F32							mWatchdogTimeout;

	boost::signals2::connection	mRegionChangedConnection;
	boost::signals2::connection	mParcelChangedConnection;
	boost::signals2::connection	mPositionChangedConnection;

	// These are used only in the automation script, by GetObjectInfo()
	uuid_list_t					mObjectInfoRequests;

	// Internal print buffer for D-Bus or threaded Lua instances
	std::string					mPrintBuffer;

	// Used to deal Lua warnings
	std::string					mWarningPrefix;
	std::string					mPendingWarningText;
	bool						mPausedWarnings;
	bool						mForceWarningsToChat;
	// 'true' when using the print buffer (D-Bus or threaded Lua instances)
	bool						mUsePrintBuffer;

	// Flags used to speed-up callbacks when they are not used in the
	// automation script, which is the only instance using them...
	bool						mHasCallbacks;
	bool						mHasOnSignal;
	bool						mHasOnLogin;
	bool						mHasOnRegionChange;
	bool						mHasOnParcelChange;
	bool						mHasOnPositionChange;
	bool						mHasOnAveragedFPS;
	bool						mHasOnAgentOccupationChange;
	bool						mHasOnAgentPush;
	bool						mHasOnSendChat;
	bool						mHasOnReceivedChat;
	bool						mHasOnChatTextColoring;
	bool						mHasOnInstantMsg;
	bool						mHasOnScriptDialog;
	bool						mHasOnNotification;
	bool						mHasOnFriendStatusChange;
	bool						mHasOnAvatarRezzing;
	bool						mHasOnAgentBaked;
	bool						mHasOnRadar;
	bool						mHasOnRadarSelection;
	bool						mHasOnRadarMark;
	bool						mHasOnRadarTrack;
	bool						mHasOnLuaDialogClose;
	bool						mHasOnLuaFloaterAction;
	bool						mHasOnLuaFloaterOpen;
	bool						mHasOnLuaFloaterClose;
	bool						mHasOnSideBarVisibilityChange;
	bool						mHasOnAutomationMessage;
	bool						mHasOnAutomationRequest;
	bool						mHasOnAutoPilotFinished;
	bool						mHasOnTPStateChange;
	bool						mHasOnFailedTPSimChange;
	bool						mHasOnWindlightChange;
	bool						mHasOnCameraModeChange;
	bool						mHasOnJoystickButtons;
	bool						mHasOnLuaPieMenu;
	bool						mHasOnContextMenu;
	bool						mHasOnRLVHandleCommand;
	bool						mHasOnRLVAnswerOnChat;
	bool						mHasOnObjectInfoReply;
	bool						mHasOnPickInventoryItem;
	bool						mHasOnPickAvatar;

	typedef fast_hmap<lua_State*, HBViewerAutomation*> instances_map_t;
	static instances_map_t		sInstances;

	// We must protect the instance pointers and pending signals with a mutex !
	static LLMutex				sThreadsMutex;

	typedef fast_hmap<U32, HBAutomationThread*> threads_list_t;
	static threads_list_t		sThreadsInstances;
	static threads_list_t		sDeadThreadsInstances;

	struct HBThreadSignals
	{
		std::vector<std::string>	mSignals;
		U32							mThreadID;
	};
	typedef fast_hmap<HBAutomationThread*, HBThreadSignals*> signals_map_t;
	static signals_map_t		sThreadsSignals;

	// Array of flags/counters used to avoid infinite recursion when dealing
	// with some Lua callbacks. They are used only by the automation script and
	// must be set (and automatically reset) via the HBIgnoreCallback helper
	// class.
	enum {
		E_ONSENDCHAT,
		E_ONINSTANTMSG,
		E_ONRADARTRACK,
		E_ONAGENTOCCUPATIONCHANGE,
		E_ONCAMERAMODECHANGE,
		E_ONWINDLIGHTCHANGE,
		E_IGN_CB_COUNT
	};
	static S32					sIgnoredCallbacks[E_IGN_CB_COUNT];

	// Used for the automation script, to observe friends status changes
	static LLFriendObserver*	sFriendsObserver;

	// These are used internally; for any script, in order to fulfill the
	// objects (un)muting needs.
	static uuid_list_t			sMuteObjectRequests;
	static uuid_list_t			sUnmuteObjectRequests;

	static std::string			sLastAutomationScriptFile;

	typedef std::deque<LLVector3d> pos_history_t;
	static pos_history_t		sPositionsHistory;

#if LL_LINUX
public:
	static LLUUID				sLuaDBusFakeObjectId;
#endif
};

class HBLuaSideBar final : public LLPanel
{
protected:
	LOG_CLASS(HBLuaSideBar);

public:
	HBLuaSideBar();
	~HBLuaSideBar() override;

	void draw() override;
	void setVisible(bool visible) override;
	void reshape(S32 width, S32 height, bool from_parent = true) override;
	bool handleRightMouseDown(S32 x, S32 y, MASK mask) override;

	void setHidden(bool hidden);

	U32 setButton(U32 number, std::string icon, std::string command,
				  const std::string& tooltip);
	S32 buttonToggle(U32 number, S32 toggle);
	void buttonSetControl(U32 number, LLControlVariable* control);
	void setButtonEnabled(U32 number, bool enabled);
	void setButtonVisible(U32 number, bool visible);

	void removeAllButtons();

	LL_INLINE void hideOnRightClick(bool b)		{ mHideOnRightClick = b; }

private:
	void setShape();

	static bool handleSideChanged(const LLSD&);

	static void onButtonClicked(void* user_data);

private:
	std::vector<std::string>	mCommands;
	std::set<U32>				mActiveButtons;
	U32							mNumberOfButtons;
	bool						mLeftSide;
	bool						mHidden;
	bool						mHideOnRightClick;
};

class HBLuaPieMenu final : public LLPieMenu
{
protected:
	LOG_CLASS(HBLuaPieMenu);

public:
	HBLuaPieMenu();
	~HBLuaPieMenu() override;

	bool onPieMenu(const LLPickInfo& pick, LLViewerObject* object);
	void onPieSliceClick(U32 slice, const LLPickInfo& pick);

	void setSlice(S32 type, U32 slice, const std::string& label,
				  const std::string& command);

	void removeAllSlices();

private:
	// We have two methods so to avoid re-running here the same (costly) code
	// as the one executed in LLToolPie::handleRightClickPick() for finding the
	// avatar associated with an attachment...

	// This is the simpler method where "object" must never be another avatar's
	// attachment. It is called from onPieMenu() (istelf called from
	// LLToolPie::handleRightClickPick()) and from the method below.
	S32 getPickedType(const LLPickInfo& pick, LLViewerObject* object);

	// This is the full method (which calls the above one when needed) and it
	// uses a caching scheme.
	S32 getPickedType(const LLPickInfo& pick);

private:
	std::vector<std::string>	mCommands;
	std::vector<std::string>	mLabels;
	LLUUID						mLastPickId;
	S32							mLastPickType;
};

extern HBViewerAutomation*	gAutomationp;
extern HBLuaSideBar*		gLuaSideBarp;
extern HBLuaPieMenu*		gLuaPiep;

#endif	// LL_HBVIEWERAUTOMATION_H
