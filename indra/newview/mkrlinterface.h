/**
 * @file mkrlinterface.h
 * @author Marine Kelley
 * @brief The header for all RLV features
 *
 * RLV Source Code
 * The source code in this file("Source Code") is provided by Marine Kelley
 * to you under the terms of the GNU General Public License, version 2.0
 *("GPL"), unless you have obtained a separate licensing agreement
 *("Other License"), formally executed by you and Marine Kelley.  Terms of
 * the GPL can be found in doc/GPL-license.txt in the distribution of the
 * original source of the Second Life Viewer, or online at
 * http://secondlifegrid.net/programs/open_source/licensing/gplv2
 *
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 *
 * ALL SOURCE CODE FROM MARINE KELLEY IS PROVIDED "AS IS." MARINE KELLEY
 * MAKES NO WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING
 * ITS ACCURACY, COMPLETENESS OR PERFORMANCE.
 */

#ifndef LL_MKRLINTERFACE_H
#define LL_MKRLINTERFACE_H

#include <deque>
#include <map>

#include "llframetimer.h"
#include "llstring.h"
#include "lluuid.h"
#include "llwearabletype.h"

#include "llchat.h"

#define RL_VIEWER_NAME "RestrainedLife"
#define RL_VIEWER_NAME_NEW "RestrainedLove"
#define RL_VERSION_NUM "2092928"
#define RL_VERSION "2.09.29.28"

#define RL_PREFIX '@'
#define RL_SHARED_FOLDER "#RLV"
#define RL_RLV_REDIR_FOLDER_PREFIX "#RLV/~"
// Length of the "#RLV/~" string constant in characters.
#define RL_HRLVST_LENGTH 6
// Length of the "#RLV/" string constant in characters.
#define RL_HRLVS_LENGTH 5
#define RL_PROTECTED_FOLDER_TAG "nostrip"
#define RL_NORELAY_FOLDER_TAG "norelay"

// Define to 1 if you wish to allow the user to attach/detach recently
// received items/folders. This breaks restrictions imposed by items given by
// force-transformation scripts, and this "feature" was more or less
// implemented in Marine's RLV to work around failures to attach objects at
// outfit restoration on login, something that does not affect us since the
// Cool VL Viewer automatically queues RLV commands and delays their execution
// until the avatar is fully rezzed.
#define RL_ALLOW_ATTACH_DETACH_RECENTLY_RECEIVED_ITEMS 0

// Wearable types as strings
#define WS_ALL "all"
#define WS_EYES "eyes"
#define WS_SKIN "skin"
#define WS_SHAPE "shape"
#define WS_HAIR "hair"
#define WS_GLOVES "gloves"
#define WS_JACKET "jacket"
#define WS_PANTS "pants"
#define WS_SHIRT "shirt"
#define WS_SHOES "shoes"
#define WS_SKIRT "skirt"
#define WS_SOCKS "socks"
#define WS_UNDERPANTS "underpants"
#define WS_UNDERSHIRT "undershirt"
#define WS_ALPHA "alpha"
#define WS_TATTOO "tattoo"
#define WS_UNIVERSAL "universal"
#define WS_PHYSICS "physics"

#define EXTREMUM 1000000.f

class LLColor3;
class LLInventoryCategory;
class LLInventoryItem;
class LLJoint;
class LLVector3;
class LLViewerFetchedTexture;
class LLViewerInventoryItem;
class LLViewerInventoryCategory;
class LLViewerJointAttachment;
class LLViewerObject;
class LLVOAvatar;

extern bool gRLenabled;

typedef std::multimap<std::string, std::string> rl_map_t;
typedef std::multimap<std::string, std::string>::iterator rl_map_it_t;

class RLInterface
{
protected:
	LOG_CLASS(RLInterface);

public:
	RLInterface();
	~RLInterface();

	// Methods called from llappviewer.cpp:
	static void init();
	void idleTasks();
	void refreshTPflag(bool save);	// Called when the viewer is closing.

	// Method called from llstartup.cpp
	static void usePerAccountSettings();

	// If all is false, do not clear rules attached to NULL_KEY as they are
	// issued from external objects (only cleared when changing parcel)
	// Method used internally but also called by hbfloaterrlv.cpp
	bool garbageCollector(bool all = true);

	// Called from llviewermessage.cpp:
	void queueCommands(const LLUUID& id, const std::string& name,
					   const std::string& cmd_line);

	std::string getVersion();		// returns "RestrainedLife Viewer .../..."
	std::string getVersion2();		// returns "RestrainedLove Viewer .../..."

	// These methods return true if the action is part of the restrictions
	bool contains(std::string action);
	bool containsSubstr(std::string action);
	// Returns true if the action or action+"_sec" is part of the restrictions
	// and either there is no global exception, or there is no local exception
	// when the match is action+"_sec".
	bool containsWithoutException(std::string action,
								  const std::string& except = LLStringUtil::null);

	// Returns true if cat or one of its parents is locked, or not shared while
	// @unshared is active
	bool isFolderLocked(LLInventoryCategory* cat);

	void replace(const LLUUID& src_id, const LLUUID& by_id);

	// Scans the list of restrictions and when finding "notify" tells the
	// restriction on the specified channel
	void notify(const std::string& action,
				const std::string& suffix = LLStringUtil::null);

	std::string crunchEmote(const std::string& msg, U32 truncate_to = 0);

	std::string getOutfitLayerAsString(LLWearableType::EType layer);

	std::string getCommandsByType(S32 type, bool blacklist = false);

	std::string getRlvRestrictions(const std::string& filter = LLStringUtil::null);

	std::string getWornItems(LLInventoryCategory* cat);
	// Returns the pointer to the #RLV folder or NULL if does not exist
	LLInventoryCategory* getRlvShare();
	bool isUnderRlvShare(LLInventoryItem* item);
	bool isUnderRlvShare(LLInventoryCategory* cat);

	bool shouldMoveToSharedSubFolder(LLViewerInventoryCategory* catp);
	void moveToSharedSubFolder(LLViewerInventoryCategory* catp);

#if RL_ALLOW_ATTACH_DETACH_RECENTLY_RECEIVED_ITEMS
	bool isInventoryFolderNew(LLInventoryCategory* folder);
	bool isInventoryItemNew(LLInventoryItem* item);
#endif

	LLViewerJointAttachment* findAttachmentPointFromName(std::string obj_name,
														 bool exact_name = false);
	LLViewerJointAttachment* findAttachmentPointFromParentName(LLInventoryItem* item);
	S32 findAttachmentPointNumber(LLViewerJointAttachment* attachment);
	void addAttachmentPointName(LLViewerObject* vobj);

	bool canDetachAllObjectsFromAttachment(LLViewerJointAttachment* attachment);
	void fetchInventory(LLInventoryCategory* root = NULL);

	LL_INLINE bool getAllowCancelTp()				{ return mAllowCancelTp; }
	LL_INLINE void setAllowCancelTp(bool newval)	{ mAllowCancelTp = newval; }

	void storeLastStandingLoc(bool force = false);
	void validateLastStandingLoc();
	void restoreLastStandingLoc();
	void backToLastStandingLoc();

	// Returns "someone", "unknown" etc according to the length of the name
	// (when shownames is on)
	std::string getDummyName(std::string name,
							 EChatAudible audible = CHAT_AUDIBLE_FULLY);
	// Replaces names by dummy names
	std::string getCensoredMessage(std::string str);
	// Censors region and parcel names
	std::string getCensoredLocation(std::string str);

	LL_INLINE void setSitTargetId(const LLUUID& newval)
	{
		mSitTargetId = newval;
	}

	LL_INLINE void setLastLoadedPreset(const std::string& newval)
	{
		mLastLoadedPreset = newval;
	}

	LLInventoryItem* getItem(const LLUUID& worn_object_id);

	void attachObjectByUUID(const LLUUID& asset_id, S32 attach_pt_num = 0,
							bool kick = false);

	bool canDetachAllSelectedObjects();
	bool isSittingOnAnySelectedObject();

	bool isRestoringOutfit();

	bool canAttachCategory(LLInventoryCategory* folder,
						   bool with_exceptions = true);
	bool canDetachCategory(LLInventoryCategory* folder,
						   bool with_exceptions = true);
	bool canUnwear(LLViewerInventoryItem* item);
	bool canUnwear(LLWearableType::EType type);
	bool canWear(LLViewerInventoryItem* item);
	bool canWear(LLWearableType::EType type);
	bool canDetach(LLViewerInventoryItem* item);
	bool canDetach(LLViewerObject* attached_object);
	bool canDetach(std::string attachpt);
	bool canAttach(LLViewerObject* object_to_attach, std::string attachpt);
	bool canAttach(LLViewerInventoryItem* item);
	bool canStartIM(const LLUUID& to_id);
	bool canSendIM(const LLUUID& to_id);
	bool canReceiveIM(const LLUUID& from_id);
	bool canSendGroupIM(std::string group_name);
	bool canReceiveGroupIM(std::string group_name);
	bool canEdit(LLViewerObject* object);
	// Sets pick_intersection to force the check on this position
	bool canTouch(LLViewerObject* object,
				  LLVector3 pick_intersection = LLVector3::zero);
	// Sets pick_intersection to force the check on this position
	bool canTouchFar(LLViewerObject* object,
					 LLVector3 pick_intersection = LLVector3::zero);

	void updateLimits();

	bool checkCameraLimits(bool and_correct = false);
	bool updateCameraLimits();
	void drawRenderLimit(bool force_opaque);

	LLJoint* getCamDistDrawFromJoint();

	LL_INLINE void setCamDistDrawFromJoint(LLJoint* joint)
	{
		mCamDistDrawFromJoint = joint;
	}

	// Returns 1 is the avatar can be fully visible, 0 if it should be fully
	// invisible or -1 is it should be rendered as a jelly doll, based on RLV
	// current restriction and its distance from the agent avatar. HB
	S32 avatarVisibility(LLVOAvatar* avatarp);

private:
	std::string getVersionNum();	// returns "RL_VERSION_NUM[,blacklist]"

	std::deque<std::string> parse(std::string str, std::string sep);
	bool parseCommand(const std::string& command, std::string& behaviour,
					  std::string& option, std::string& param);

	// Executes queued commands, in chronological order
	void fireCommands();
	// Executes a single command
	bool handleCommand(const LLUUID& id, std::string command);
	// Queues a single command (or executes it for version* commands)
	void queueCommand(const LLUUID& id, const std::string& name,
					  const std::string& command);

	bool force(const LLUUID& obj_id, std::string command, std::string option);
	bool add(const LLUUID& obj_id, std::string action, std::string option);
	bool remove(const LLUUID& obj_id, std::string action, std::string option);
	bool clear(const LLUUID& obj_id,
			   const std::string& command = LLStringUtil::null);

	bool isBlacklisted(const LLUUID& id, std::string command,
					   const std::string& option, bool force = false);

	bool answerOnChat(const LLUUID& obj_id, const std::string& channel,
					  std::string msg);

	void refreshCachedVariable(const std::string& var);

	bool isAllowed(LLUUID object_id, std::string action, bool log_it = true);

	// Note: this method is costly, so it is only used internally to refresh
	// the public mHasLockedHuds boolean when appropriate. Use the latter for
	// testing for locked HUDs.
	bool hasLockedHuds();

	// Returns the max value of all the @action:...=n restrictions
	F32 getMax(std::string action, F32 dflt = EXTREMUM);
	// Returns the min value of all the @action:...=n restrictions
	F32 getMin(std::string action, F32 dflt = -EXTREMUM);
	// Returns the product of all the colors specified by actions "action"
	LLColor3 getMixedColors(std::string action,
							LLColor3 dflt = LLColor3::black);

	void removeWearableItemFromAvatar(LLViewerInventoryItem* item);

	// Type of the lock of a folder
	typedef enum EFolderLock {
		FolderNotLocked = 0,
		FolderLockedWithException,
		FolderLockedNoException,
		FolderLockCount
	} EFolderLock;

	// Helper method used by isFolderLocked(). attach_or_detach must be either
	// "attach" or "detach"
	EFolderLock isFolderLockedWithoutException(LLInventoryCategory* cat,
											  std::string attach_or_detach);
	// Helper method used by isFolderLockedWithoutException()
	EFolderLock isFolderLockedWithoutExceptionAux(LLInventoryCategory* cat,
												  std::string attach_or_detach,
												  std::deque<std::string> list);

	LLWearableType::EType getOutfitLayerAsType(const std::string& layer);
	std::string getOutfit(const std::string& layer);
	std::string getAttachments(const std::string& attachpt);

	// If obj_id is null, returns everything
	std::string getStatus(const LLUUID& obj_id, std::string rule);

	std::string getCommand(std::string match, bool blacklist = false);

	std::deque<std::string> getBlacklist(std::string filter = "");

	bool forceDetach(const std::string& attachpt);
	bool forceDetachByUuid(const std::string& object_id);

	std::deque<LLInventoryItem*> getListOfLockedItems(LLInventoryCategory* root);
	std::deque<std::string> getListOfRestrictions(const LLUUID& obj_id,
												  const std::string& rule = LLStringUtil::null);
	std::string getInventoryList(const std::string& path,
								 bool with_worn_info = false);

	// Returns true if cat_child is a child of cat_parent
	bool isUnderFolder(LLInventoryCategory* cat_parent,
					   LLInventoryCategory* cat_child);

	LLInventoryCategory* getCategoryUnderRlvShare(std::string cat_name,
												  LLInventoryCategory* root = NULL);
	LLInventoryCategory* findCategoryUnderRlvShare(std::string cat_name,
												   LLInventoryCategory* root = NULL);
	std::deque<LLInventoryCategory*> findCategoriesUnderRlvShare(std::string cat_name,
																 LLInventoryCategory* root = NULL);

	void detachObject(LLViewerObject* object);
	void detachAllObjectsFromAttachment(LLViewerJointAttachment* attachment);

	// How to call @attach:outfit=force(useful for multi-attachments and
	// multi-wearables
	typedef enum EAttachMethod {
		AttachReplace = 0,	// Always replace other attachments (default)
		AttachOver,			// Attach over, not replacing other attachments
		// Attach over if the name of the outfit begins with a special sign,
		// otherwise replace
		AttachOverOrReplace,
		AttachMethodsCount
	} EAttachMethod;

	void forceAttach(const std::string& category, bool recursive,
					 EAttachMethod how);
	bool forceDetachByName(const std::string& category, bool recursive);

	// If keep_lookat is true, the TP uses LLAgent::teleportViaLocationLookAt()
	// instead of LLAgent::teleportViaLocation(), so to keep facing in the same
	// direction on arrival than on departure.
	bool forceTeleport(const std::string& location, bool keep_lookat = false);

	std::string stringReplace(std::string s, std::string what_str,
							  const std::string& by_str,
							  bool case_sensitive = false);

	// 'command' is "setenv_<something>", option is a list of floats (separated
	// by "/")
	bool forceEnvironment(std::string command, std::string option);
	// command is "getenv_<something>"
	std::string getEnvironment(std::string command);

	// 'command' is "setdebug_<something>", option is a list of values
	// (separated by "/")
	bool forceDebugSetting(std::string command, std::string option);
	// 'command' is "getdebug_<something>"
	std::string getDebugSetting(std::string command);

	std::string getFullPath(LLInventoryCategory* cat);
	std::string getFullPath(LLInventoryItem* item,
							const std::string& option = LLStringUtil::null,
							bool full_list = true);

	// Various helper methods

	LLInventoryItem* getItemAux(LLViewerObject* attached_object,
								LLInventoryCategory* root);

	bool canAttachCategoryAux(LLInventoryCategory* folder, bool in_parent,
							  bool in_no_mod, bool with_exceptions = true);
	bool canDetachCategoryAux(LLInventoryCategory* folder, bool in_parent,
							  bool in_no_mod, bool with_exceptions = true);

	void drawSphere(const LLVector3& center, F32 scale, const LLColor3& color,
					F32 alpha);

public:
	enum RLBehaviourType {
		RL_INFO,				// Information commands, not-blacklistable.
		RL_MISCELLANEOUS,		// Miscellaneous not-blacklistable commands.
		RL_INSTANTMESSAGE,		// Instant Messaging commands.
		RL_SENDCHAT,			// Chat sending commands.
		RL_RECEIVECHAT,			// Chat receiving commands.
		RL_CHANNEL,				// Chat on private channels commands.
		RL_EMOTE,				// Emote/pose commands.
		RL_REDIRECTION,			// Emote/pose redirection commands.
		RL_MOVE,				// Movement commands.
		RL_SIT,					// Sitting/unsitting commands.
		RL_TELEPORT,			// Teleportation commands.
		RL_TOUCH,				// Touch commands.
		RL_LOCK,				// Locking/unlocking commands.
		RL_ATTACH,				// Attach/wear commands.
		RL_DETACH,				// Detach/remove commands.
		RL_INVENTORY,			// Inventory commands.
		RL_INVENTORYLOCK,		// Inventory locking commands.
		RL_BUILD,				// Rezing/editing commands.
		RL_LOCATION,			// Location commands.
		RL_NAME,				// Name commands.
		RL_GROUP,				// Group commands.
		RL_SHARE,				// Sharing commands.
		RL_PERM,				// Permissions/extra-restriction commands.
		RL_CAMERA,				// Camera restriction commands.
		RL_DEBUG,				// Debug settings commands.
		RL_ENVIRONMENT,			// Environment/rendering commands.
	};

	typedef std::pair<std::string, S32> rl_command_entry_t;
	typedef std::map<std::string, S32> rl_command_map_t;
	typedef std::map<std::string, S32>::iterator rl_command_map_it_t;

	static rl_command_map_t		sCommandsMap;

	// User-blacklisted RestrainedLove commands.
	static std::string			sBlackList;
	// Standard blacklist for role-players
	static std::string			sRolePlayBlackList;
	// Standard blacklist for non-BDSM folks
	static std::string			sVanillaBlackList;

	// Message to replace an incoming IM, when under recvim
	static std::string			sRecvimMessage;
	// Message to replace an outgoing IM, when under sendim
	static std::string			sSendimMessage;

	F32							mTplocalMax;
	F32							mSittpMax;
	F32							mFartouchMax;

	F32							mCamZoomMax;
	F32							mCamZoomMin;
	F32							mCamDistMax;
	F32							mCamDistMin;
	F32							mCamDistDrawMax;
	F32							mCamDistDrawMin;
	F32							mCamDistDrawAlphaMin;
	F32							mCamDistDrawAlphaMax;
	F32							mShowavsDistMax;

	// Must be a LLPointer, else the texture may get removed from memory if not
	// used elsewhere, and cause a crash when used by the RLV code...
	LLPointer<LLViewerFetchedTexture> mCamTexturesCustom;

	// For convenience (gAgent does not retain the name of the current parcel):
	std::string					mParcelName;

	// Allowed debug settings(initialized in the ctor)
	std::vector<std::string>	mAllowedGetDebug;
	std::vector<std::string>	mAllowedSetDebug;

	// Public, because also used by other classes in llchatbar.cpp,
	// hbfloaterrlv.cpp and hbviewerautomation.cpp
	rl_map_t					mSpecialObjectBehaviours;

#if RL_ALLOW_ATTACH_DETACH_RECENTLY_RECEIVED_ITEMS
	// List of items received during the session
	typedef std::set<std::string> received_list_t;
	received_list_t				mReceivedInventoryFolders;
#endif

	// When a locked attachment is kicked off by another one with
	// llAttachToAvatar() in a script, retain its UUID here, to reattach it
	// later.
	struct RLAttachment
	{
		RLAttachment()
		{
		}

		RLAttachment(const LLUUID& id, const std::string& name)
		:	mId(id),
			mName(name)
		{
		}

		LLUUID		mId;
		std::string	mName;
	};
	typedef std::deque<RLAttachment> reattach_queue_t;

	reattach_queue_t			mAssetsToReattach;

	// Reset each time a locked attachment is kicked by a "Wear", and on
	// auto-reattachment timeout.
	LLFrameTimer				mReattachTimer;

	// We need this to inhibit the removeObject event that occurs right after
	// addObject in the case of a replacement
	RLAttachment				mJustDetached;
#if 0	// Not needed, actually...
	// We need this to inhibit the removeObject event that occurs right after
	// addObject in the case of a replacement
	RLAttachment				mJustReattached;
#endif

	// true when llappviewer.cpp asked for a reattachment. false when
	// llviewerjointattachment.cpp detected a reattachment.
	bool						mReattaching;
	// true when llappviewer.cpp detects a reattachment timeout, false when
	// llviewerjointattachment.cpp detected a reattachment.
	bool						mReattachTimeout;
	// Set this to true when restoring an outfit after logging in, to override
	// attach/detach restictions
	bool						mRestoringOutfit;

	// true when already rendered the vision spheres during the current frame
	bool						mRenderLimitRenderedThisFrame;

	// true when we are teleporting back to the last standing location, in
	// order to bypass the usual checks
	bool						mSnappingBackToLastStandingLocation;

	// true while waiting to stand up from a seat before executing @sitground
	bool						mSitGroundOnStandUp;

	// Some cache variables to accelerate common checks
	bool						mHasLockedHuds;
	bool						mContainsDetach;
	bool						mContainsShowinv;
	bool						mContainsUnsit;
	bool						mContainsStandtp;
	bool						mContainsInteract;
	bool						mContainsShowworldmap;
	bool						mContainsShowminimap;
	bool						mContainsShowloc;
	bool						mContainsShownames;
	bool						mContainsShownametags;
	bool						mContainsShowNearby;
	bool						mContainsViewscript;
	bool						mContainsSetenv;
	bool						mContainsSetdebug;
	bool						mContainsFly;
	bool						mContainsEdit;
	bool						mContainsRez;
	bool						mContainsShowhovertextall;
	bool						mContainsShowhovertexthud;
	bool						mContainsShowhovertextworld;
	bool						mContainsDefaultwear;
	bool						mContainsPermissive;
	bool						mContainsRun;
	bool						mContainsAlwaysRun;
	bool						mContainsTp;
	bool						mContainsCamTextures;
	bool						mVisionRestricted;

private:
	// true after executing @sit=force
	bool						mGotSit;
	// true after executing @unsit=force
	bool						mGotUnsit;
	// Used to force-queue commands after and including @sit=force in a
	// command line that comes while @unsit=force was just executed and also
	// part of that same command line.
	bool						mSkipAll;
	// true while logging in and attempting to snap back to last standing
	// location before last logout
	bool						mHandleBackToLastStanding;
	// true while processing RestrainedLove commands, to prevent stripping
	// items which name contains "nostrip"
	bool						mHandleNoStrip;
	// true when a command just got black-listed in by add() or force()
	bool						mLastCmdBlacklisted;
	// true while processing RestrainedLove commands following a @relayed
	// command, to prevent passing folder names which contain "norelay"
	bool						mHandleNoRelay;
	// false at first, used to fetch RL Share inventory once upon login
	bool						mInventoryFetched;
	// true unless forced to TP with @tpto (=> receive TP order from server,
	// act like it is a lure from a Linden => don't show the cancel button)
	bool						mAllowCancelTp;

	F32 						mNextGarbageCollection;

	// Time stamp of the beginning of this session
	U32							mLaunchTimestamp;

	// Number of spheres to draw when restricting the camera view
	U32							mCamDistNbGradients;

	// mHeadp by default, but can be set it to another joint so the user can
	// "see" the world with vision spheres centered around that joint instead.
	LLJoint*					mCamDistDrawFromJoint;

	LLColor3					mCamDistDrawColor;

	LLFrameTimer				mSitUnsitDelayTimer;

	LLUUID						mSitTargetId;
	// This is the global position we had when we sat down on something, and we
	// will be teleported back there when we stand up if we are prevented from
	// "sit-tp by rezzing stuff"
	LLVector3d					mLastStandingLocation;

	// contains the name of the latest loaded Windlight preset
	std::string					mLastLoadedPreset;

	struct RLCommand
	{
		RLCommand(const LLUUID& id, const std::string& name,
				  const std::string& command)
		:	mId(id),
			mName(name),
			mCommand(command)
		{
		}

		LLUUID		mId;
		std::string	mName;
		std::string	mCommand;
	};
	std::deque<RLCommand>		mQueuedCommands;

	// List of avatar UUIDs for which the name sensoring is not applied
	uuid_list_t					mExceptions;

	// List of relay object UUIDs
	uuid_list_t					mRelays;

	// When true, the user can bypass a sendchat restriction by surrounding
	// with (( and ))
	static bool					sCanOoc;

	// When true, the user's emotes are never truncated.
	static bool					sUntruncatedEmotes;

	// When true, the @setnev command is disabled
	static bool					sRLNoSetEnv;
};

typedef RLInterface::reattach_queue_t::iterator rl_reattach_it_t;

extern RLInterface gRLInterface;

#endif	// LL_MKRLINTERFACE_H
