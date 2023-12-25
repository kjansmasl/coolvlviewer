/**
 * @file mkrlinterface.cpp
 * @author Marine Kelley (many parts rewritten/expanded by Henri Beauchamp)
 * @brief Implementation of the RLV features
 *
 * RLV Source Code
 * The source code in this file("Source Code") is provided by Marine Kelley
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Marine Kelley.  Terms of
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

#include "llviewerprecompiledheaders.h"

#include "mkrlinterface.h"

#include "llapp.h"
#include "llcachename.h"
#include "llregionhandle.h"
#include "llrenderutils.h"			// gSphere

#include "llagent.h"
#include "llagentwearables.h"
#include "llappearancemgr.h"
#include "llappviewer.h"			// For gFrameTimeSeconds
#include "lldrawpoolalpha.h"
#include "llenvironment.h"
#include "llenvsettings.h"
#include "llfloaterchat.h"
#include "hbfloaterrlv.h"
#include "llfloaterwindlight.h"
#include "llgesturemgr.h"
#include "llhudtext.h"
#include "llinventorybridge.h"
#include "llpipeline.h"
#include "llselectmgr.h"
#include "llsky.h"
#include "llstartup.h"
#include "lltracker.h"
#include "lltooldraganddrop.h"
#include "hbviewerautomation.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerinventory.h"
#include "llviewerfoldertype.h"
#include "llviewerjointattachment.h"
#include "llviewermenu.h"
#include "llviewermessage.h"		// send_agent_update()
#include "llviewerobjectlist.h"
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"
#include "llviewertexture.h"
#include "llviewerwearable.h"
#include "llvoavatarself.h"
#include "llworld.h"
#include "llworldmap.h"
#include "roles_constants.h"

RLInterface gRLInterface;

// Global and static variables initialization.
bool gRLenabled = true;
bool RLInterface::sRLNoSetEnv = false;
bool RLInterface::sUntruncatedEmotes = false;
bool RLInterface::sCanOoc = true;
std::string RLInterface::sRecvimMessage;
std::string RLInterface::sSendimMessage;
std::string RLInterface::sBlackList;
std::string RLInterface::sRolePlayBlackList;
std::string RLInterface::sVanillaBlackList;
RLInterface::rl_command_map_t RLInterface::sCommandsMap;

//----------------------------------------------------------------------------
// Helper functions
//----------------------------------------------------------------------------

static std::string dumpList2String(std::deque<std::string> list,
								   std::string sep, S32 size = -1)
{
	if (size < 0)
	{
		size = (S32)list.size();
	}
	std::string res;
	for (S32 i = 0; i < (S32)list.size() && i < size; ++i)
	{
		if (i != 0)
		{
			res += sep;
		}
		res += list[i];
	}
	return res;
}

static S32 match(std::deque<std::string> list, std::string str,
				 bool& exact_match)
{
	// does str contain list[0]/list[1]/.../list[n] ?
	// yes => return the size of the list
	// no  => try again after removing the last element
	// return 0 if never found
	// exact_match is an output, set to true when strict matching is found,
	// false otherwise.
	U32 size = list.size();
	std::string dump;
	exact_match = false;
	while (size > 0)
	{
		dump = dumpList2String(list, "/", (S32) size);
		if (str == dump)
		{
			exact_match = true;
			return (S32)size;
		}
		else if (str.find(dump) != std::string::npos)
		{
			return (S32)size;
		}
		--size;
	}
	return 0;
}

static std::deque<std::string> getSubList(std::deque<std::string> list,
										  S32 min, S32 max = -1)
{
	if (min < 0)
	{
		min = 0;
	}
	if (max < 0)
	{
		max = list.size() - 1;
	}
	std::deque<std::string> res;
	for (S32 i = min; i <= max; ++i)
	{
		res.emplace_back(list[i]);
	}
	return res;
}

static bool findMultiple(std::deque<std::string> list, std::string str)
{
	// returns true if all the tokens in list are contained into str
	U32 size = list.size();
	for (U32 i = 0; i < size; ++i)
	{
		if (str.find(list[i]) == std::string::npos) return false;
	}
	return true;
}

static void updateAllHudTexts()
{
	for (LLHUDText::htobj_list_it_t it = LLHUDText::sTextObjects.begin(),
									end = LLHUDText::sTextObjects.end();
		 it != end; ++it)
	{
		LLHUDText* hud_text = *it;
		if (hud_text && !hud_text->mLastMessageText.empty() &&
			hud_text->getDoFade())
		{
			// do not update the floating names of the avatars around
			LLViewerObject* obj = hud_text->getSourceObject();
			if (obj && !obj->isAvatar())
			{
				hud_text->setStringUTF8(hud_text->mLastMessageText);
			}
		}
	}
}

static void updateOneHudText(LLUUID id)
{
	LLViewerObject* obj = gObjectList.findObject(id);
	if (obj && obj->mText.notNull())
	{
		LLHUDText* hud_text = obj->mText.get();
		if (hud_text && !hud_text->mLastMessageText.empty() &&
			hud_text->getDoFade())
		{
			hud_text->setStringUTF8(hud_text->mLastMessageText);
		}
	}
}

//----------------------------------------------------------------------------
// RLInterface() class proper
//----------------------------------------------------------------------------

RLInterface::RLInterface()
:	mInventoryFetched(false),
	mAllowCancelTp(true),
	mReattaching(false),
	mReattachTimeout(false),
	mRestoringOutfit(false),
	mSnappingBackToLastStandingLocation(false),
	mSitGroundOnStandUp(false),
	mHasLockedHuds(false),
	mContainsDetach(false),
	mContainsShowinv(false),
	mContainsUnsit(false),
	mContainsStandtp(false),
	mContainsInteract(false),
	mContainsShowworldmap(false),
	mContainsShowminimap(false),
	mContainsShowloc(false),
	mContainsShownames(false),
	mContainsShownametags(false),
	mContainsShowNearby(false),
	mContainsSetenv(false),
	mContainsSetdebug(false),
	mContainsFly(false),
	mContainsEdit(false),
	mContainsRez(false),
	mContainsShowhovertextall(false),
	mContainsShowhovertexthud(false),
	mContainsShowhovertextworld(false),
	mContainsDefaultwear(false),
	mContainsPermissive(false),
	mContainsRun(false),
	mContainsAlwaysRun(false),
	mContainsViewscript(false),
	mContainsCamTextures(false),
	mContainsTp(false),
	mGotSit(false),
	mGotUnsit(false),
	mSkipAll(false),
	mHandleBackToLastStanding(false),
	mHandleNoStrip(false),
	mHandleNoRelay(false),
	mLastCmdBlacklisted(false),
	mLaunchTimestamp((U32)LLTimer::getEpochSeconds()),
	// Give the garbage collector a moment before even kicking in the first
	// time, in case we are logging in a very laggy place, taking time to rez
	mNextGarbageCollection(30.f),
	mVisionRestricted(false),
	mRenderLimitRenderedThisFrame(false),
	mCamDistNbGradients(10),
	mCamDistDrawFromJoint(NULL),
	mCamZoomMax(EXTREMUM),
	mCamZoomMin(-EXTREMUM),
	mCamDistMax(EXTREMUM),
	mCamDistMin(-EXTREMUM),
	mCamDistDrawMax(EXTREMUM),
	mCamDistDrawMin(EXTREMUM),
	mShowavsDistMax(EXTREMUM),
	mFartouchMax(EXTREMUM),
	mSittpMax(EXTREMUM),
	mTplocalMax(EXTREMUM)
{
	mAllowedGetDebug.emplace_back("AvatarSex");
	mAllowedGetDebug.emplace_back("RenderResolutionDivisor");
	mAllowedGetDebug.emplace_back("RestrainedLoveForbidGiveToRLV");
	mAllowedGetDebug.emplace_back("RestrainedLoveNoSetEnv");

	// 0 female, 1 male (unreliable: depends on shape)
	mAllowedSetDebug.emplace_back("AvatarSex");
	// To allow simulating blur; default is 1 for no blur
	mAllowedSetDebug.emplace_back("RenderResolutionDivisor");

	mJustDetached.mId.setNull();
	mJustDetached.mName.clear();
#if 0
	mJustReattached.mId.setNull();
	mJustReattached.mName.clear();
#endif
}

RLInterface::~RLInterface()
{
	mCamTexturesCustom = NULL;
}

// init() must be called at an early stage, to setup all RestrainedLove session
// variables. It is called from LLAppViewer::init() in llappviewer.cpp. This
// cannot be done in the constructor for RLInterface, because calling
// gSavedSettings.get*() at that stage would cause crashes under Windows
// (working fine under Linux): probably a race condition in constructors.

//static
void RLInterface::init()
{
	// Info commands (not "blacklistable").
	sCommandsMap.emplace("version", RL_INFO);
	sCommandsMap.emplace("versionnew", RL_INFO);
	sCommandsMap.emplace("versionnum", RL_INFO);
	sCommandsMap.emplace("versionnumbl", RL_INFO);
	sCommandsMap.emplace("getcommand", RL_INFO);
	sCommandsMap.emplace("getstatus", RL_INFO);
	sCommandsMap.emplace("getstatusall", RL_INFO);
	sCommandsMap.emplace("getsitid", RL_INFO);
	sCommandsMap.emplace("getoutfit", RL_INFO);
	sCommandsMap.emplace("getattach", RL_INFO);
	sCommandsMap.emplace("getinv", RL_INFO);
	sCommandsMap.emplace("getinvworn", RL_INFO);
	sCommandsMap.emplace("getpath", RL_INFO);
	sCommandsMap.emplace("getpathnew", RL_INFO);
	sCommandsMap.emplace("findfolder", RL_INFO);
	sCommandsMap.emplace("findfolders", RL_INFO);
	sCommandsMap.emplace("getgroup", RL_INFO);
	sCommandsMap.emplace("getdebug_", RL_INFO);
	sCommandsMap.emplace("getenv_", RL_INFO);
	sCommandsMap.emplace("getcam_", RL_INFO);

	// Miscellaneous non-info commands that are not "blacklistable".
	sCommandsMap.emplace("notify", RL_MISCELLANEOUS);
	sCommandsMap.emplace("clear", RL_MISCELLANEOUS);
	sCommandsMap.emplace("detachme%f", RL_MISCELLANEOUS);
	sCommandsMap.emplace("setrot%f", RL_MISCELLANEOUS);
	sCommandsMap.emplace("adjustheight%f", RL_MISCELLANEOUS);
	sCommandsMap.emplace("emote", RL_MISCELLANEOUS);
	sCommandsMap.emplace("relayed", RL_MISCELLANEOUS);

	// Normal commands, "blacklistable".

	// Movement restrictions
	sCommandsMap.emplace("fly", RL_MOVE);
	sCommandsMap.emplace("temprun", RL_MOVE);
	sCommandsMap.emplace("alwaysrun", RL_MOVE);
	sVanillaBlackList += "fly,temprun,alwaysrun,";

	// Chat sending restrictions
	sCommandsMap.emplace("sendchat", RL_SENDCHAT);
	sCommandsMap.emplace("chatshout", RL_SENDCHAT);
	sCommandsMap.emplace("chatnormal", RL_SENDCHAT);
	sCommandsMap.emplace("chatwhisper", RL_SENDCHAT);
	sCommandsMap.emplace("sendgesture", RL_SENDCHAT);
	sVanillaBlackList += "sendchat,chatshout,chatnormal,chatwhisper,sendgesture,";

	// Chat receiving restrictions
	sCommandsMap.emplace("recvchat", RL_RECEIVECHAT);
	sCommandsMap.emplace("recvchat_sec", RL_RECEIVECHAT);
	sCommandsMap.emplace("recvchatfrom", RL_RECEIVECHAT);
	sVanillaBlackList += "recvchat,recvchat_sec,recvchatfrom,";

	// Chat on private channels restrictions
	sCommandsMap.emplace("sendchannel", RL_CHANNEL);
	sCommandsMap.emplace("sendchannel_sec", RL_CHANNEL);
	sCommandsMap.emplace("sendchannel_except", RL_CHANNEL);
	sRolePlayBlackList += "sendchannel,sendchannel_sec,sendchannel_except,";
	sVanillaBlackList += "sendchannel,sendchannel_sec,sendchannel_except,";

	// Chat and emotes redirections
	sCommandsMap.emplace("redirchat", RL_REDIRECTION);
	sCommandsMap.emplace("rediremote", RL_REDIRECTION);

	// Emotes restrictions
	sCommandsMap.emplace("recvemote", RL_EMOTE);
	sCommandsMap.emplace("recvemote_sec", RL_EMOTE);
	sCommandsMap.emplace("recvemotefrom", RL_EMOTE);
	sRolePlayBlackList += "recvemote,recvemote_sec,recvemotefrom,";
	sVanillaBlackList += "recvemote,recvemote_sec,recvemotefrom,";

	// Instant messaging restrictions
	sCommandsMap.emplace("sendim", RL_INSTANTMESSAGE);
	sCommandsMap.emplace("sendim_sec", RL_INSTANTMESSAGE);
	sCommandsMap.emplace("sendimto", RL_INSTANTMESSAGE);
	sCommandsMap.emplace("startim", RL_INSTANTMESSAGE);
	sCommandsMap.emplace("startimto", RL_INSTANTMESSAGE);
	sCommandsMap.emplace("recvim", RL_INSTANTMESSAGE);
	sCommandsMap.emplace("recvim_sec", RL_INSTANTMESSAGE);
	sCommandsMap.emplace("recvimfrom", RL_INSTANTMESSAGE);
	sRolePlayBlackList += "sendim,sendim_sec,sendimto,startim,startimto,recvim,recvim_sec,recvimfrom,";
	sVanillaBlackList += "sendim,sendim_sec,sendimto,startim,startimto,recvim,recvim_sec,recvimfrom,";

	// Teleport restrictions
	sCommandsMap.emplace("tplm", RL_TELEPORT);
	sCommandsMap.emplace("tploc", RL_TELEPORT);
	sCommandsMap.emplace("tplocal", RL_TELEPORT);
	sCommandsMap.emplace("tplure", RL_TELEPORT);
	sCommandsMap.emplace("tplure_sec", RL_TELEPORT);
	sCommandsMap.emplace("sittp", RL_TELEPORT);
	sCommandsMap.emplace("standtp", RL_TELEPORT);
	sCommandsMap.emplace("tpto%f", RL_TELEPORT);
	sCommandsMap.emplace("accepttp", RL_TELEPORT);
	sCommandsMap.emplace("accepttprequest", RL_TELEPORT);
	sCommandsMap.emplace("tprequest", RL_TELEPORT);
	// Note: tpto is used by teleporters: allow
	sVanillaBlackList += "tplm,tploc,tplocal,tplure,tplure_sec,sittp,standtp,accepttp,accepttprequest,tprequest,";

	// Inventory access restrictions
	sCommandsMap.emplace("showinv", RL_INVENTORY);
	sCommandsMap.emplace("viewnote", RL_INVENTORY);
	sCommandsMap.emplace("viewscript", RL_INVENTORY);
	sCommandsMap.emplace("viewtexture", RL_INVENTORY);
	sCommandsMap.emplace("sharedwear", RL_INVENTORYLOCK);
	sCommandsMap.emplace("unsharedwear", RL_INVENTORYLOCK);
	sCommandsMap.emplace("unsharedunwear", RL_INVENTORYLOCK);
	sRolePlayBlackList += "showinv,viewnote,viewscript,viewtexture,sharedwear,unsharedwear,unsharedunwear,";
	sVanillaBlackList += "showinv,viewnote,viewscript,viewtexture,sharedwear,unsharedwear,unsharedunwear,";

	// Building restrictions
	sCommandsMap.emplace("edit", RL_BUILD);
	sCommandsMap.emplace("editattach", RL_BUILD);
	sCommandsMap.emplace("editobj", RL_BUILD);
	sCommandsMap.emplace("editworld", RL_BUILD);
	sCommandsMap.emplace("rez", RL_BUILD);
	sRolePlayBlackList += "edit,editattach,editobj,editworld,rez,";
	sVanillaBlackList += "edit,editattach,editobj,editworld,rez,";

	// Sitting restrictions
	sCommandsMap.emplace("unsit", RL_SIT);
	sCommandsMap.emplace("unsit%f", RL_SIT);
	sCommandsMap.emplace("sit", RL_SIT);
	sCommandsMap.emplace("sit%f", RL_SIT);
	sCommandsMap.emplace("sitground%f", RL_SIT);
	sVanillaBlackList += "unsit,unsit%f,sit,sit%f,sitground%f";

	// Locking commands
	sCommandsMap.emplace("detach", RL_LOCK);
	sCommandsMap.emplace("detachthis", RL_LOCK);
	sCommandsMap.emplace("detachallthis", RL_LOCK);
	sCommandsMap.emplace("detachthis_except", RL_LOCK);
	sCommandsMap.emplace("detachallthis_except", RL_LOCK);
	sCommandsMap.emplace("attachthis", RL_LOCK);
	sCommandsMap.emplace("attachallthis", RL_LOCK);
	sCommandsMap.emplace("attachthis_except", RL_LOCK);
	sCommandsMap.emplace("attachallthis_except", RL_LOCK);
	sCommandsMap.emplace("addattach", RL_LOCK);
	sCommandsMap.emplace("remattach", RL_LOCK);
	sCommandsMap.emplace("addoutfit", RL_LOCK);
	sCommandsMap.emplace("remoutfit", RL_LOCK);
	sCommandsMap.emplace("defaultwear", RL_LOCK);
	sVanillaBlackList += "detach,detachthis,detachallthis,detachthis_except,detachallthis_except,attachthis,attachallthis,attachthis_except,attachallthis_except,addattach,remattach,addoutfit,remoutfit,defaultwear,";

	// Detach/remove commands
	sCommandsMap.emplace("detach%f", RL_DETACH);
	sCommandsMap.emplace("detachall%f", RL_DETACH);
	sCommandsMap.emplace("detachthis%f", RL_DETACH);
	sCommandsMap.emplace("detachallthis%f", RL_DETACH);
	sCommandsMap.emplace("remattach%f", RL_DETACH);
	sCommandsMap.emplace("remoutfit%f", RL_DETACH);

	// Attach/wear commands
	sCommandsMap.emplace("attach%f", RL_ATTACH);
	sCommandsMap.emplace("attachover%f", RL_ATTACH);
	sCommandsMap.emplace("attachoverorreplace%f", RL_ATTACH);
	sCommandsMap.emplace("attachall%f", RL_ATTACH);
	sCommandsMap.emplace("attachallover%f", RL_ATTACH);
	sCommandsMap.emplace("attachalloverorreplace%f", RL_ATTACH);
	sCommandsMap.emplace("attachthis%f", RL_ATTACH);
	sCommandsMap.emplace("attachthisover%f", RL_ATTACH);
	sCommandsMap.emplace("attachthisover%f", RL_ATTACH);
	sCommandsMap.emplace("attachthisoverorreplace%f", RL_ATTACH);
	sCommandsMap.emplace("attachallthis%f", RL_ATTACH);
	sCommandsMap.emplace("attachallthisover%f", RL_ATTACH);
	sCommandsMap.emplace("attachallthisoverorreplace%f", RL_ATTACH);

	// Touch restrictions
	sCommandsMap.emplace("fartouch", RL_TOUCH);
	sCommandsMap.emplace("interact", RL_TOUCH);
	sCommandsMap.emplace("touchfar", RL_TOUCH);
	sCommandsMap.emplace("touchall", RL_TOUCH);
	sCommandsMap.emplace("touchworld", RL_TOUCH);
	sCommandsMap.emplace("touchthis", RL_TOUCH);
	sCommandsMap.emplace("touchme", RL_TOUCH);
	sCommandsMap.emplace("touchattach", RL_TOUCH);
	sCommandsMap.emplace("touchattachself", RL_TOUCH);
	sCommandsMap.emplace("touchhud", RL_TOUCH);
	sCommandsMap.emplace("touchattachother", RL_TOUCH);
	sVanillaBlackList += "fartouch,interact,touchfar,touchall,touchworld,touchthis,touchme,touchattach,touchattachself,touchhud,touchattachother,";

	// Location/mapping restrictions
	sCommandsMap.emplace("showworldmap", RL_LOCATION);
	sCommandsMap.emplace("showminimap", RL_LOCATION);
	sCommandsMap.emplace("showloc", RL_LOCATION);
	sRolePlayBlackList += "showworldmap,showminimap,showloc,";
	sVanillaBlackList += "showworldmap,showminimap,showloc,";

	// Name viewing restrictions
	sCommandsMap.emplace("shownames", RL_NAME);
	sCommandsMap.emplace("shownames_sec", RL_NAME);
	sCommandsMap.emplace("shownametags", RL_NAME);
	sCommandsMap.emplace("shownearby", RL_NAME);
	sCommandsMap.emplace("showhovertextall", RL_NAME);
	sCommandsMap.emplace("showhovertext", RL_NAME);
	sCommandsMap.emplace("showhovertexthud", RL_NAME);
	sCommandsMap.emplace("showhovertextworld", RL_NAME);
	sRolePlayBlackList += "shownames,shownametags,showhovertextall,showhovertext,showhovertexthud,showhovertextworld,";
	sVanillaBlackList += "shownames,shownametags,showhovertextall,showhovertext,showhovertexthud,showhovertextworld,";

	// Group restrictions
	sCommandsMap.emplace("setgroup", RL_GROUP);
	sCommandsMap.emplace("setgroup%f", RL_GROUP);
	sRolePlayBlackList += "setgroup,";
	sVanillaBlackList += "setgroup,";	// @setgroup=force May be used as a helper: allow

	// Sharing restrictions
	sCommandsMap.emplace("share", RL_SHARE);
	sCommandsMap.emplace("share_sec", RL_SHARE);
	sRolePlayBlackList += "share,share_sec,";
	sVanillaBlackList += "share,share_sec,";

	// Permissions/extra-restriction commands.
	sCommandsMap.emplace("permissive", RL_PERM);
	sCommandsMap.emplace("acceptpermission", RL_PERM);
	sVanillaBlackList += "permissive,acceptpermission,";

	// Camera restriction commands.
	sCommandsMap.emplace("camtextures", RL_CAMERA);
	sCommandsMap.emplace("camunlock", RL_CAMERA);
	sCommandsMap.emplace("camzoommax", RL_CAMERA);
	sCommandsMap.emplace("camzoommin", RL_CAMERA);
	sCommandsMap.emplace("camdistmax", RL_CAMERA);
	sCommandsMap.emplace("camdistmin", RL_CAMERA);
	sCommandsMap.emplace("camdrawmax", RL_CAMERA);
	sCommandsMap.emplace("camdrawmin", RL_CAMERA);
	sCommandsMap.emplace("camdrawalphamax", RL_CAMERA);
	sCommandsMap.emplace("camdrawalphamin", RL_CAMERA);
	sCommandsMap.emplace("camdrawcolor", RL_CAMERA);
	sCommandsMap.emplace("camavdist", RL_CAMERA);
	sCommandsMap.emplace("setcam_", RL_CAMERA);
	sCommandsMap.emplace("setcam_fov%f", RL_CAMERA);
	sRolePlayBlackList += "camtextures,camunlock,camzoommax,camzoommin,camdistmax,camdistmin,camdrawmax,camdrawmin,camdrawalphamax,camdrawalphamin,camdrawcolor,camavdist,setcam_,setcam_fov%f,";
	sVanillaBlackList += "camtextures,camunlock,camzoommax,camzoommin,camdistmax,camdistmin,camdrawmax,camdrawmin,camdrawalphamax,camdrawalphamin,camdrawcolor,camavdist,setcam_,setcam_fov%f,";

	// Debug settings commands.
	sCommandsMap.emplace("setdebug", RL_DEBUG);
	sCommandsMap.emplace("setdebug_%f", RL_DEBUG);
	sRolePlayBlackList += "setdebug";
	sVanillaBlackList += "setdebug,setdebug_%f,";

	sVanillaBlackList += "setenv";

	gRLInterface.mCamTexturesCustom = LLViewerFetchedTexture::sDefaultImagep;

	gRLenabled = gSavedSettings.getBool("RestrainedLove");
	if (gRLenabled)
	{
		sRLNoSetEnv = gSavedSettings.getBool("RestrainedLoveNoSetEnv");
		sUntruncatedEmotes =
			gSavedSettings.getBool("RestrainedLoveUntruncatedEmotes");
		sCanOoc = gSavedSettings.getBool("RestrainedLoveCanOoc");
		sBlackList = gSavedSettings.getString("RestrainedLoveBlacklist");

		if (!sRLNoSetEnv)
		{
			sCommandsMap.emplace("setenv", RL_ENVIRONMENT);
			sCommandsMap.emplace("setenv_%f", RL_ENVIRONMENT);
		}

		gRLInterface.updateCameraLimits();
		gRLInterface.updateLimits();

		llinfos << "RestrainedLove enabled and initialized." << llendl;
	}
}

//static
void RLInterface::usePerAccountSettings()
{
	if (gRLenabled)
	{
		sRecvimMessage =
			gSavedPerAccountSettings.getString("RestrainedLoveRecvimMessage");
		sSendimMessage =
			gSavedPerAccountSettings.getString("RestrainedLoveSendimMessage");
	}
}

// Call this function when adding/removing a restriction only, i.e. in this
// file. Test the cached variables in the code of the viewer itself.
void RLInterface::refreshCachedVariable(const std::string& var)
{
	if (!isAgentAvatarValid()) return;

	bool update_names_exceptions = false;

	bool contained = contains(var);
	if (var == "detach" || var.compare(0, 7, "detach:") == 0 ||
		var.compare(0, 9, "addattach") == 0 ||
		var.compare(0, 9, "remattach") == 0)
	{
		contained = contains("detach") || containsSubstr("detach:") ||
					containsSubstr("addattach") || containsSubstr("remattach");
		mContainsDetach = contained;
		mHasLockedHuds = hasLockedHuds();
		if (mHasLockedHuds)
		{
			// To force the viewer to render the HUDs again, just in case
			LLPipeline::sShowHUDAttachments = true;
		}
		if (gUseWireframe && (mHasLockedHuds || mCamDistDrawMax < EXTREMUM))
		{
			handle_toggle_wireframe(NULL);
		}
	}
	else if (var == "showinv")
	{
		mContainsShowinv = contained;
	}
	else if (var == "unsit")
	{
		mContainsUnsit = contained;
	}
	else if (var == "standtp")
	{
		mContainsStandtp = contained;
	}
	else if (var == "interact")
	{
		mContainsInteract = contained;
	}
	else if (var == "showworldmap")
	{
		mContainsShowworldmap = contained;
	}
	else if (var == "showminimap")
	{
		mContainsShowminimap = contained;
	}
	else if (var == "showloc")
	{
		mContainsShowloc = contained;
	}
	else if (var == "shownames" || var == "shownames_sec")
	{
		mContainsShownames = contained;
		update_names_exceptions = true;
	}
	else if (var == "shownametags")
	{
		mContainsShownametags = contained;
		update_names_exceptions = true;
	}
	else if (var == "shownearby")
	{
		mContainsShowNearby = contained;
	}
	else if (var == "setenv")
	{
		mContainsSetenv = contained;
	}
	else if (var == "setdebug")
	{
		mContainsSetdebug = contained;
	}
	else if (var == "fly")
	{
		mContainsFly = contained;
	}
	else if (var == "edit")
	{
		mContainsEdit =
#if 0
			containsSubstr("editobj") ||
#endif
			containsWithoutException("edit");
	}
	else if (var == "rez")
	{
		mContainsRez = contained;
	}
	else if (var == "showhovertextall")
	{
		mContainsShowhovertextall = contained;
	}
	else if (var == "showhovertexthud")
	{
		mContainsShowhovertexthud = contained;
	}
	else if (var == "showhovertextworld")
	{
		mContainsShowhovertextworld = contained;
	}
	else if (var == "defaultwear")
	{
		mContainsDefaultwear = contained;
	}
	else if (var == "permissive")
	{
		mContainsPermissive = contained;
	}
	else if (var == "temprun")
	{
		mContainsRun = contained;
	}
	else if (var == "alwaysrun")
	{
		mContainsAlwaysRun = contained;
	}
	else if (var == "viewscript")
	{
		mContainsViewscript = contained;
	}
	else if (var.compare(0, 11, "camtextures") == 0 ||
			 var.compare(0, 15, "setcam_textures") == 0)
	{
		mContainsCamTextures = containsSubstr("camtextures") ||
							   containsSubstr("setcam_textures");
		// Is there a uuid specified ?
		size_t i = var.find(":");
		if (i != std::string::npos)
		{
			std::string id_str = var.substr(i + 1);
			LLUUID tex_id;
			tex_id.set(id_str, false);
			if (tex_id.notNull())
			{
				mCamTexturesCustom =
					LLViewerTextureManager::getFetchedTexture(tex_id,
															  FTT_DEFAULT,
															  true,
															  LLGLTexture::BOOST_NONE,
															  LLViewerTexture::LOD_TEXTURE);
			}
			else
			{
				mCamTexturesCustom = LLViewerFetchedTexture::sDefaultImagep;
			}
		}
		// Silly hack, but we need to force all textures in world to be updated
		handle_objects_visibility(NULL);
	}
	else if (var == "camzoommax" || var == "camzoommin")
	{
		gViewerCamera.setDefaultFOV(gSavedSettings.getF32("CameraAngle"));
	}

	mContainsTp = contains("tplm") || contains("tploc") ||
				  contains("tplure") ||
				  (mContainsUnsit && gAgentAvatarp->mIsSitting);

	refreshTPflag(true);

	if (update_names_exceptions)
	{
		// Rebuild the list of exceptions for shownames and shownametags
		std::string command, behav, option, param;
		LLUUID avid;
		mExceptions.clear();
		for (rl_map_it_t it = mSpecialObjectBehaviours.begin(),
						 end = mSpecialObjectBehaviours.end();
			 it != end; ++it)
		{
			command = it->second;
			LLStringUtil::toLower(command);
			if (command.compare(0, 10, "shownames:") == 0 ||
				command.compare(0, 13, "shownames_sec:") == 0 ||
				command.compare(0, 13, "shownametags:") == 0)
			{
				if (parseCommand(command, behav, option, param))
				{
					avid.set(option, false);
					if (avid.notNull())
					{
						mExceptions.emplace(avid);
					}
				}
			}
		}
	}
}

void RLInterface::refreshTPflag(bool save)
{
	static bool last_value =
		gSavedPerAccountSettings.getBool("RestrainedLoveTPOK");
	bool new_value = !mContainsTp;
	if (new_value != last_value)
	{
		last_value = new_value;
		gSavedPerAccountSettings.setBool("RestrainedLoveTPOK", new_value);
		if (save)
		{
			gSavedPerAccountSettings.saveToFile(gSavedSettings.getString("PerAccountSettingsFile"));
		}
	}
}

void RLInterface::idleTasks()
{
	// If RLV share inventory has not been fetched yet, fetch it now
	fetchInventory();

	// Perform some maintenance only if no object is waiting to be reattached
	if (mAssetsToReattach.empty())
	{
		// Fire all the stored commands that we received while initializing
		fireCommands();

		// Fire the garbage collector for orphaned restrictions
		if (gFrameTimeSeconds > mNextGarbageCollection)
		{
			garbageCollector(false);
			mNextGarbageCollection = gFrameTimeSeconds + 30.f;
		}
	}

	// We must check whether there is an object waiting to be reattached after
	// having been kicked off while locked.
	if (!mAssetsToReattach.empty())
	{
		// Get the elapsed time since detached, and the delay before reattach.
		U32 elapsed = (U32)mReattachTimer.getElapsedTimeF32();
		static LLCachedControl<U32> reattach_delay(gSavedSettings,
												   "RestrainedLoveReattachDelay");
		// Timeout flag.
		bool timeout = mReattaching && elapsed > 4 * reattach_delay;
		if (timeout)
		{
			// If we timed out, reset the timer and tell the interface...
			mReattachTimer.reset();
			mReattachTimeout = true;
			llwarns << "Timeout reattaching an asset, retrying." << llendl;
		}
		if (!mReattaching || timeout)
		{
			// We are not reattaching an object (or we timed out), so let's see
			// if the delay before auto-reattach has elapsed.
			if (elapsed >= reattach_delay)
			{
				// Let's reattach the object to its default attach point.
				const RLAttachment& at = mAssetsToReattach.front();
				S32 tmp_attachpt_nb = 0;
				LLViewerJointAttachment* attachpt =
					findAttachmentPointFromName(at.mName, true);
				if (attachpt)
				{
					tmp_attachpt_nb = findAttachmentPointNumber(attachpt);
				}
				llinfos << "Reattaching asset " << at.mId << " to point '"
						<< at.mName << "' (number " << tmp_attachpt_nb << ")"
						<< llendl;
				mReattaching = true;
				attachObjectByUUID(at.mId, tmp_attachpt_nb);
			}
		}
	}
}

std::string RLInterface::getVersion()
{
	return RL_VIEWER_NAME " viewer v" RL_VERSION;
}

std::string RLInterface::getVersion2()
{
	return RL_VIEWER_NAME_NEW " viewer v" RL_VERSION;
}

std::string RLInterface::getVersionNum()
{
	std::string res = RL_VERSION_NUM;
	if (!sBlackList.empty())
	{
		res += "," + sBlackList;
	}
	return res;
}

bool RLInterface::isAllowed(LLUUID object_id, std::string action,
							bool log_it)
{
	if (log_it)
	{
		LL_DEBUGS("RestrainedLove") << object_id << "      " << action
									<< LL_ENDL;
	}
	rl_map_it_t it = mSpecialObjectBehaviours.find(object_id.asString());
	while (it != mSpecialObjectBehaviours.end() &&
		   it != mSpecialObjectBehaviours.upper_bound(object_id.asString()))
	{
#if 0
		if (log_it)
		{
			LL_DEBUGS("RestrainedLove") << "  checking " << it->second
										<< LL_ENDL;
		}
#endif
		if (it->second == action)
		{
			if (log_it)
			{
				LL_DEBUGS("RestrainedLove") << "  => forbidden. " << LL_ENDL;
			}
			return false;
		}
		++it;
	}
	if (log_it)
	{
		LL_DEBUGS("RestrainedLove") << "  => allowed. " << LL_ENDL;
	}
	return true;
}

bool RLInterface::contains(std::string action)
{
	LLStringUtil::toLower(action);
	for (rl_map_it_t it = mSpecialObjectBehaviours.begin(),
					 end = mSpecialObjectBehaviours.end();
		 it != end; ++it)
	{
		if (it->second == action)
		{
			return true;
		}
	}
	return false;
}

bool RLInterface::containsSubstr(std::string action)
{
	LLStringUtil::toLower(action);
	for (rl_map_it_t it = mSpecialObjectBehaviours.begin(),
					 end = mSpecialObjectBehaviours.end();
		 it != end; ++it)
	{
		if (it->second.find(action) != std::string::npos)
		{
			return true;
		}
	}
	return false;
}

bool RLInterface::containsWithoutException(std::string action,
										   const std::string& except)
{
	// 'action' is a restriction like @sendim, which can accept exceptions
	// (@sendim:except_uuid=add)
	// action_sec is the same action, with "_sec" appended (like @sendim_sec)

	LLStringUtil::toLower(action);
	std::string action_sec = action + "_sec";
	LLUUID id;

	// 1. If except is empty, behave like contains(), but looking for both
	// action and action_sec
	if (except.empty())
	{
		return contains(action) || contains(action_sec);
	}

	// 2. For each action_sec, if we do not find an exception tied to the same
	// object, return true. If @permissive is set, then even action needs the
	// exception to be tied to the same object, not just action_sec (@permissive
	// restrains the scope of all the exceptions to their own objects)
	for (rl_map_it_t it = mSpecialObjectBehaviours.begin(),
					 end = mSpecialObjectBehaviours.end();
		 it != end; ++it)
	{
		if (it->second == action_sec ||
			(it->second == action && mContainsPermissive))
		{
			id.set(it->first);
			// We use isAllowed because we need to check the object, but it
			// really means "does not contain"
			if (isAllowed(id, action + ":" + except, false) &&
				isAllowed(id, action_sec + ":" + except, false))
			{
				return true;
			}
		}
	}

	// 3. If we did not return yet, but the map contains action, just look for
	// except_uuid without regard to its object, if none is found return true
	if (contains(action))
	{
		if (!contains(action + ":" + except) &&
			!contains(action_sec + ":" + except))
		{
			return true;
		}
	}

	// 4. Finally return false if we did not find anything
	return false;
}

F32 RLInterface::getMax(std::string action, F32 dflt)
{
	LLStringUtil::toLower(action);
	// an action may be a comma separated list list of behaviours
	action = "," + action + ",";
	F32 res = -EXTREMUM;
	F32 tmp;
	std::string command, behav, option, param;
	bool found_one = false;
	for (rl_map_it_t it = mSpecialObjectBehaviours.begin(),
					 end = mSpecialObjectBehaviours.end();
		 it != end; ++it)
	{
		command = it->second;
		LLStringUtil::toLower(command);
		if (parseCommand(command + "=n", behav, option, param))
		{
			if (action.find("," + behav + ",") != std::string::npos)
			{
				if (option.empty())
				{
					tmp = 1.5f;
				}
				else
				{
					tmp = atof(option.c_str());
				}
				if (tmp > res)
				{
					res = tmp;
					found_one = true;
				}
			}
		}
	}

	return found_one ? res : dflt;
}

F32 RLInterface::getMin(std::string action, F32 dflt)
{
	LLStringUtil::toLower(action);
	// An action may be a comma separated list list of behaviours
	action = "," + action + ",";
	F32 res = EXTREMUM;
	F32 tmp;
	std::string command, behav, option, param;
	bool found_one = false;
	for (rl_map_it_t it = mSpecialObjectBehaviours.begin(),
					 end = mSpecialObjectBehaviours.end();
		 it != end; ++it)
	{
		command = it->second;
		LLStringUtil::toLower(command);
		if (parseCommand(command + "=n", behav, option, param))
		{
			if (action.find("," + behav + ",") != std::string::npos)
			{
				if (option.empty())
				{
					tmp = 1.5f;
				}
				else
				{
					tmp = atof(option.c_str());
				}
				if (tmp < res)
				{
					res = tmp;
					found_one = true;
				}
			}
		}
	}

	return found_one ? res : dflt;
}

LLColor3 RLInterface::getMixedColors(std::string action, LLColor3 dflt)
{
	bool found = false;
	LLColor3 res = LLColor3::white;

	LLStringUtil::toLower(action);
	// An action may be a comma separated list list of behaviours
	action = "," + action + ",";
	LLColor3 tmp;
	std::string command, behav, option, param;
	std::deque<std::string> tokens;
	for (rl_map_it_t it = mSpecialObjectBehaviours.begin(),
					 end = mSpecialObjectBehaviours.end();
		 it != end; ++it)
	{
		command = it->second;
		LLStringUtil::toLower(command);
		if (parseCommand(command + "=n", behav, option, param))
		{
			if (action.find("," + behav + ",") != std::string::npos)
			{
				tokens = parse(option, ";");
				tmp.mV[0] = atof(tokens[0].c_str());
				tmp.mV[1] = atof(tokens[1].c_str());
				tmp.mV[2] = atof(tokens[2].c_str());
				res *= tmp;
				found = true;
			}
		}
	}

	return found ? res : dflt;
}

bool RLInterface::isFolderLocked(LLInventoryCategory* cat)
{
	if (!cat) return false;

	const LLFolderType::EType folder_type = cat->getPreferredType();
	if (LLFolderType::lookupIsProtectedType(folder_type)) return false;

	bool shared = isUnderRlvShare(cat);
	if (!shared && contains("unsharedwear")) return true;
	if (shared && contains("sharedwear")) return true;

	if (isFolderLockedWithoutException(cat, "attach") != FolderNotLocked)
	{
		return true;
	}

	return isFolderLockedWithoutException(cat, "detach") != FolderNotLocked;
}

RLInterface::EFolderLock
RLInterface::isFolderLockedWithoutException(LLInventoryCategory* cat,
											std::string attach_or_detach)
{
	if (!cat) return FolderNotLocked;

	LL_DEBUGS("RestrainedLove") << "Category: " << cat->getName()
								<< " - attach_or_detach: " << attach_or_detach
								<< LL_ENDL;

	// For each object that is locking this folder, check whether it also
	// issues exceptions to this lock
	std::deque<std::string> commands_list;
	std::string command, behav, option, param;
	std::string this_command, this_behav, this_option, this_param;
	bool this_object_locks;
	EFolderLock current_lock = FolderNotLocked;

	for (rl_map_it_t it = mSpecialObjectBehaviours.begin(),
					 end = mSpecialObjectBehaviours.end();
		 it != end; ++it)
	{
		LLUUID id(it->first);
		command = it->second;
		LL_DEBUGS("RestrainedLove") << "command = " << command << LL_ENDL;

		// param will always be equal to "n" in this case since we added it to
		// command, but we do not care about this here

		// detach=n, recvchat=n, recvim=n, unsit=n, recvim:<uuid>=add,
		// clear=tplure...
		// Attention, an option must absolutely be specified here (there must
		// be a ':' character), or we would not be able to tell "detachthis"
		// from "detachthis:" and both have different meanings.
		if (command.find(':') != std::string::npos &&
			parseCommand(command + "=n", behav, option, param))
		{
			// find whether this object has issued a "{attach|detach}[all]this"
			// command on a folder that is either this one, or a parent
			this_object_locks = false;
			if (behav == attach_or_detach + "this")
			{
				if (getCategoryUnderRlvShare(option) == cat)
				{
					this_object_locks = true;
				}
			}
			else if (behav == attach_or_detach + "allthis")
			{
				if (isUnderFolder(getCategoryUnderRlvShare(option), cat))
				{
					this_object_locks = true;
				}
			}

			// This object has issued such a command, check whether it has
			// issued an exception to it as well
			if (this_object_locks)
			{
				commands_list = getListOfRestrictions(id);
				EFolderLock this_lock =
					isFolderLockedWithoutExceptionAux(cat, attach_or_detach,
													  commands_list);
				if (this_lock == FolderLockedNoException)
				{
					return FolderLockedNoException;
				}
				else
				{
					current_lock = this_lock;
				}
				LL_DEBUGS("RestrainedLove") << "this_lock=" << this_lock
											<< LL_ENDL;
			}
		}
	}

	// Finally, return unlocked since we did not find any lock on this folder
	return current_lock;
}

RLInterface::EFolderLock
RLInterface::isFolderLockedWithoutExceptionAux(LLInventoryCategory* cat,
											   std::string attach_or_detach,
											   std::deque<std::string> restrictions)
{
	// 'restrictions' contains the list of restrictions issued by one
	// particular object, at least one is supposed to be a
	// "{attach|detach}[all]this".
	// For each folder from cat up to the root folder, check :
	// - if we are on cat and we find "{attach|detach}this_except", there is an
	//   exception, keep looking up
	// - if we are on cat and we find "{attach|detach}this", there is no
	//   exception, return FolderLockedNoException
	// - if we are on a parent and we find "{attach|detach}allthis_except",
	//   there is an exception, keep looking up
	// - if we are on a parent and we find "{attach|detach}allthis", if we
	//   found an exception return FolderLockedWithException, else return
	//   FolderLockedNoException
	// - finally, if we are on the root, return FolderLocked_unlocked (whether
	//   there was an exception or not)

	if (!cat) return FolderNotLocked;

	LL_DEBUGS("RestrainedLove") << "isFolderLockedWithoutExceptionAux("
								<< cat->getName() << ", " << attach_or_detach
								<< ", ["
								<< dumpList2String(restrictions, ",")
								<< "])" << LL_ENDL;

	EFolderLock current_lock = FolderNotLocked;
	std::string command, behav, option, param;
	const LLUUID& root_id = gInventory.getRootFolderID();
	const LLUUID& cat_id = cat->getUUID();
	LLInventoryCategory* it = gInventory.getCategory(cat_id);
	LLInventoryCategory* cat_option = NULL;
	do
	{
		LL_DEBUGS("RestrainedLove") << "it=" << it->getName() << LL_ENDL;

		for (U32 i = 0; i < restrictions.size(); ++i)
		{
			command = restrictions[i];
			LL_DEBUGS("RestrainedLove") << "command2=" << command << LL_ENDL;
			// 'param' will always be equal to "n" in this case since we added
			// it to command, but we do not care about this here

			// detach=n, recvchat=n, recvim=n, unsit=n, recvim:<uuid>=add,
			// clear=tplure:
			if (parseCommand(command + "=n", behav, option, param))
			{
				cat_option = getCategoryUnderRlvShare(option);
				if (cat_option == it)
				{
					if (it == cat)
					{
						if (behav == attach_or_detach + "this_except" ||
							behav == attach_or_detach + "allthis_except")
						{
							current_lock = FolderLockedWithException;
						}
						else if (behav == attach_or_detach + "this" ||
								 behav == attach_or_detach + "allthis")
						{
							return FolderLockedNoException;
						}
					}
					else if (behav == attach_or_detach + "allthis_except")
					{
						current_lock = FolderLockedWithException;
					}
					else if (behav == attach_or_detach + "allthis")
					{
						if (current_lock == FolderLockedWithException)
						{
							return FolderLockedWithException;
						}
						else
						{
							return FolderLockedNoException;
						}
					}
				}
			}
		}

		const LLUUID& parent_id = it->getParentUUID();
		it = gInventory.getCategory(parent_id);
	}
	while (it && it->getUUID() != root_id);

	// This should never happen since list_of_commands is supposed to contain
	// at least one "{attach|detach}[all]this" restriction
	return FolderNotLocked;
}

bool RLInterface::isBlacklisted(const LLUUID& id, std::string command,
								const std::string& option, bool force)
{
	// Possibly allow all RestrainedLove commands for Lua scripts (automation
	// script, chat command line script, executed Lua file script, but not a
	// Lua command line relayed from an object, or via D-Bus under Linux), even
	// black-listed ones.
	static LLCachedControl<bool> lua_skip(gSavedSettings,
										  "RestrainedLoveLuaNoBlacklist");
	if (lua_skip && id == gAgentID)
	{
		return false;
	}

	if (sRLNoSetEnv && command.compare(0, 6, "setenv") == 0)
	{
		return true;
	}

	if (mHandleNoRelay && !option.empty() &&
		option.find(RL_NORELAY_FOLDER_TAG) != std::string::npos)
	{
		return true;
	}

	if (sBlackList.empty())
	{
		return false;
	}

	size_t i = command.find('_');
	if (i != std::string::npos && command.find("_sec") != i &&
		command.find("_except") != i)
	{
		command = command.substr(0, i + 1);
	}
	if (force)
	{
		command += "%f";
	}

	rl_command_map_it_t it = sCommandsMap.find(command);
	if (it == sCommandsMap.end())
	{
		return false;
	}

	S32 type = it->second;
	if (type == (S32)RL_INFO || type == (S32)RL_MISCELLANEOUS)
	{
		return false;
	}

	std::string blacklist = "," + sBlackList + ",";
	return blacklist.find("," + command + ",") != std::string::npos;
}

bool RLInterface::add(const LLUUID& obj_id, std::string action,
					  std::string option)
{
	LL_DEBUGS("RestrainedLove") << obj_id << ": " << action << " / " << option
								<< LL_ENDL;

	mLastCmdBlacklisted = false;

	std::string canon_action = action;
	if (!option.empty())
	{
		action += ":" + option;
	}

	if (!isAllowed(obj_id, action))
	{
		return false;
	}

	// Notify if needed
	notify(action, "=n");

	// Check the action against the blacklist
	if (isBlacklisted(obj_id, canon_action, option))
	{
		mLastCmdBlacklisted = true;
		llinfos << "Blacklisted RestrainedLove command: " << action
				<< "=n for object " << obj_id << llendl;
		return true;
	}

	// Actions to do BEFORE inserting the new behav
	if (action == "shownames" || action == "shownames_sec" ||
		action == "shownametags")
	{
		LLFloaterChat::getInstance()->childSetVisible("active_speakers_panel",
													  false);
	}
	else if (action == "fly")
	{
		gAgent.setFlying(false);
	}
	else if (action == "temprun")
	{
		if (gAgent.getRunning())
		{
			if (gAgent.getAlwaysRun())
			{
				gAgent.clearAlwaysRun();
			}
			gAgent.clearRunning();
			gAgent.sendWalkRun(false);
		}
	}
	else if (action == "alwaysrun")
	{
		if (gAgent.getAlwaysRun())
		{
			if (gAgent.getRunning())
			{
				gAgent.clearRunning();
			}
			gAgent.clearAlwaysRun();
			gAgent.sendWalkRun(false);
		}
	}
	else if (action == "edit")
	{
		gSavedSettings.setBool("BeaconAlwaysOn", false);
		LLDrawPoolAlpha::sShowDebugAlpha = false;
	}
	else if (action == "setenv")
	{
		gSavedSettings.setBool("UseLocalEnvironment", false);
		gSavedSettings.setBool("UseParcelEnvironment", false);
	}
	else if (action == "camunlock" || action == "setcam_unlock")
	{
		gAgent.resetView(true, true);
	}

	// Insert the new behav
	mSpecialObjectBehaviours.emplace(obj_id.asString(), action);
	refreshCachedVariable(action);

	// Actions to do AFTER inserting the new behav
	if (action == "showhovertextall" || action == "showloc" ||
		action == "shownames" || action == "showhovertexthud" ||
		action == "showhovertextworld")
	{
		updateAllHudTexts();
	}
	else if (canon_action == "showhovertext")
	{
		updateOneHudText(LLUUID(option));
	}
	else if (canon_action.compare(0, 3, "cam") == 0 ||
			 canon_action.compare(0, 7, "setcam_") == 0)
	{
		updateCameraLimits();
		// Force an update of the zoom if necessary
		if (canon_action == "camzoommax" || canon_action == "camzoommin" ||
		 	canon_action == "setcam_fovmin" || canon_action == "setcam_fovmax")
		{
			gViewerCamera.setDefaultFOV(gSavedSettings.getF32("CameraAngle"));
			// setView() may have clamped it:
			gSavedSettings.setF32("CameraAngle", gViewerCamera.getView());
		}
	}
	else if (canon_action == "fartouch" || canon_action == "touchfar" ||
			 canon_action == "sittp" || canon_action == "tplocal")
	{
		updateLimits();
	}

	// Update the stored last standing location, to allow grabbers to
	// transport a victim inside a cage while sitting, and restrict them
	// before standing up. If we did not do this, the avatar would snap
	// back to a safe location when being unsitted by the grabber, which
	// would be rather silly.
	if (action == "standtp")
	{
		storeLastStandingLoc(true);
	}

	return true;
}

bool RLInterface::remove(const LLUUID& obj_id, std::string action,
						 std::string option)
{
	LL_DEBUGS("RestrainedLove") << obj_id << ":" << action << " / " << option
								<< LL_ENDL;

	std::string canon_action = action;
	if (!option.empty())
	{
		action += ":" + option;
	}

	// Notify if needed
	notify(action, "=y");

	// Actions to do BEFORE removing the behav

	// Remove the behav
	rl_map_it_t it = mSpecialObjectBehaviours.find(obj_id.asString());
	while (it != mSpecialObjectBehaviours.end() &&
		   it != mSpecialObjectBehaviours.upper_bound(obj_id.asString()))
	{
#if 0
		LL_DEBUGS("RestrainedLove") << "  Checking " << it->second << LL_ENDL;
#endif
		if (it->second == action)
		{
			mSpecialObjectBehaviours.erase(it);
			LL_DEBUGS("RestrainedLove") << "  => removed." << LL_ENDL;

			refreshCachedVariable(action);

			// Actions to do AFTER removing the behav
			if (action == "shownames" || action == "showloc" ||
				action == "showhovertexthud" || action == "showhovertextall" ||
				action == "showhovertextworld")
			{
				updateAllHudTexts();
			}
			else if (canon_action == "showhovertext")
			{
				updateOneHudText(LLUUID(option));
			}
			else if (action == "standtp")
			{
				// If not sitting, then we can clear the last standing location
				if (isAgentAvatarValid() && !gAgentAvatarp->mIsSitting)
				{
					mLastStandingLocation.clear();
					gSavedPerAccountSettings.setVector3d("RestrainedLoveLastStandingLocation",
														 mLastStandingLocation);
				}
			}
			else if (canon_action.compare(0, 3, "cam") == 0 ||
					 canon_action.compare(0, 7, "setcam_") == 0)
			{
				updateCameraLimits();
			}
			else if (canon_action == "fartouch" ||
					 canon_action == "touchfar" ||
					 canon_action == "sittp" || canon_action == "tplocal")
			{
				updateLimits();
			}

			return true;
		}
		++it;
	}

	LL_DEBUGS("RestrainedLove") << "  => not in force." << LL_ENDL;

	return false;
}

bool RLInterface::clear(const LLUUID& obj_id, const std::string& command)
{
	LL_DEBUGS("RestrainedLove") << obj_id << ": " << command << LL_ENDL;

	// Notify if needed
	notify("clear" + (command.empty() ? "" : ":" + command));

	const std::string id_as_str = obj_id.asString();

	rl_map_it_t it = mSpecialObjectBehaviours.begin();
	while (it != mSpecialObjectBehaviours.end())
	{
		LL_DEBUGS("RestrainedLove") << "  removing " << it->second << LL_ENDL;

		if (it->first == id_as_str &&
			(command.empty() || it->second.find(command) != std::string::npos))
		{
			notify(it->second, "=y");
			LL_DEBUGS("RestrainedLove") << it->second << " => removed."
										<< LL_ENDL;

			std::string tmp = it->second;
			mSpecialObjectBehaviours.erase(it);
			refreshCachedVariable(tmp);
			it = mSpecialObjectBehaviours.begin();
		}
		else
		{
			++it;
		}
	}

	// If not still under @standtp restriction, or not sitting, then we can
	// clear the last standing location
	if (!mContainsStandtp ||
		(isAgentAvatarValid() && !gAgentAvatarp->mIsSitting))
	{
		mLastStandingLocation.clear();
		gSavedPerAccountSettings.setVector3d("RestrainedLoveLastStandingLocation",
											 mLastStandingLocation);
	}

	updateAllHudTexts();
	updateCameraLimits();
	updateLimits();

	return true;
}

void RLInterface::replace(const LLUUID& src_id, const LLUUID& by_id)
{
	LLUUID id;
	rl_map_it_t it = mSpecialObjectBehaviours.begin();
	while (it != mSpecialObjectBehaviours.end())
	{
		id.set(it->first);
		if (id == src_id)
		{
			// Found the UUID to replace => add a copy of the command with the
			// new UUID
			mSpecialObjectBehaviours.emplace(by_id.asString(), it->second);
		}
		++it;
	}
	// And then clear the old UUID
	clear(src_id, "");
	HBFloaterRLV::setDirty();
}

bool RLInterface::garbageCollector(bool all)
{
	bool res = false;
	LLUUID id;
	rl_map_it_t it = mSpecialObjectBehaviours.begin();
	while (it != mSpecialObjectBehaviours.end())
	{
		id.set(it->first);
#if LL_LINUX
		bool is_lua = id == gAgentID ||
					  (id == HBViewerAutomation::sLuaDBusFakeObjectId &&
					   id.notNull());
#else
		bool is_lua = id == gAgentID;
#endif
		if (!is_lua && (all || id.notNull()))
		{
			LLViewerObject* objp = gObjectList.findObject(id);
			if (!objp)
			{
				LL_DEBUGS("RestrainedLove") << it->first
											<< " not found => cleaning... "
											<< LL_ENDL;
				clear(id);
				res = true;
				it = mSpecialObjectBehaviours.begin();
				HBFloaterRLV::setDirty();
			}
			else
			{
				++it;
			}
		}
		else
		{
			LL_DEBUGS("RestrainedLove") << "Ignoring " << it->second
										<< LL_ENDL;
			++it;
		}
	}
	return res;
}

std::deque<std::string> RLInterface::parse(std::string str, std::string sep)
{
	size_t ind;
	size_t length = sep.length();
	std::string token;
	std::deque<std::string> res;

	do
	{
		ind = str.find(sep);
		if (ind != std::string::npos)
		{
			token = str.substr(0, ind);
			if (!token.empty())
			{
				res.emplace_back(token);
			}
			str = str.substr(ind + length);
		}
		else if (!str.empty())
		{
			res.emplace_back(str);
		}
	}
	while (ind != std::string::npos);

	return res;
}

void RLInterface::notify(const std::string& action, const std::string& suffix)
{
	size_t length = 7;	// size of "notify:"
	std::deque<std::string> tokens;
	std::string rule;
	LLUUID obj_id;
	for (rl_map_it_t it = mSpecialObjectBehaviours.begin(),
					 end = mSpecialObjectBehaviours.end();
		 it != end; ++it)
	{
		// We are looking for rules like "notify:2222;tp", if action contains
		// "tp" then notify the scripts on channel 2222
		rule = it->second;
		if (rule.compare(0, 7, "notify:") == 0)
		{
			// Found a possible notification to send
			rule = rule.substr(length);	// keep right part only(here "2222;tp")
			tokens = parse(rule, ";");
			S32 size = tokens.size();
			if (size == 1 ||
				(size > 1 && action.find(tokens[1]) != std::string::npos))
			{
				obj_id.set(it->first);
				// suffix can be "=n", "=y" or whatever else we want, "/" is
				// needed to avoid some clever griefing
				answerOnChat(obj_id, tokens[0], "/" + action + suffix);
			}
		}
	}
}

bool RLInterface::parseCommand(const std::string& command,
							   std::string& behaviour, std::string& option,
							   std::string& param)
{
	size_t i = command.find('=');
	if (i == std::string::npos)
	{
		behaviour = command;
		param.clear();
		option.clear();
		return false;
	}

	behaviour = command.substr(0, i);
	param = command.substr(i + 1);
	i = behaviour.find(':');
	if (i != std::string::npos)
	{
		option = behaviour.substr(i + 1);
		// Keep in this order(option first, then behav) or crash
		behaviour = behaviour.substr(0, i);
	}
	else
	{
		option.clear();		
	}

	return true;
}

bool RLInterface::handleCommand(const LLUUID& id, std::string command)
{
	mHandleNoRelay = mRelays.count(id) != 0;

	// Parse the command, which is of one of these forms:
	// behav=param
	// behav:option=param
	std::string behav, option, param;
	LLStringUtil::toLower(command);

	// detach=n, recvchat=n, recvim=n, unsit=n, recvim:<uuid>=add,
	// clear=tplure:
	if (parseCommand(command, behav, option, param))
	{
		LL_DEBUGS("RestrainedLove") << "[" << id << "]  [" << behav << "]  ["
									<< option << "] [" << param << "]"
									<< LL_ENDL;
		if (gAutomationp)
		{
			gAutomationp->onRLVHandleCommand(id, behav, option, param);
		}

		if (behav == "version")
		{
			return answerOnChat(id, param, getVersion());
		}
		else if (behav == "versionnew")
		{
			return answerOnChat(id, param, getVersion2());
		}
		else if (behav == "versionnum")
		{
			return answerOnChat(id, param, RL_VERSION_NUM);
		}
		else if (behav == "versionnumbl")
		{
			return answerOnChat(id, param, getVersionNum());
		}
		else if (behav == "getblacklist")
		{
			return answerOnChat(id, param,
								dumpList2String(getBlacklist(option), ","));
		}
		else if (behav == "getoutfit")
		{
			return answerOnChat(id, param, getOutfit(option));
		}
		else if (behav == "getattach")
		{
			return answerOnChat(id, param, getAttachments(option));
		}
		else if (behav == "getstatus")
		{
			return answerOnChat(id, param, getStatus(id, option));
		}
		else if (behav == "getstatusall")
		{
			return answerOnChat(id, param, getStatus(LLUUID::null, option));
		}
		else if (behav == "getcommand")
		{
			return answerOnChat(id, param, getCommand(option));
		}
		else if (behav == "getinv")
		{
			return answerOnChat(id, param, getInventoryList(option));
		}
		else if (behav == "getinvworn")
		{
			return answerOnChat(id, param, getInventoryList(option, true));
		}
		else if (behav == "getsitid")
		{
			return answerOnChat(id, param, mSitTargetId.asString());
		}
		else if (behav == "getpath")
		{
			// Option can be empty (=> find path to object) or the name of an
			// attach pt or the name of a clothing layer
			return answerOnChat(id, param,
								getFullPath(getItem(id), option, false));
		}
		else if (behav == "getpathnew")
		{
			// Option can be empty (=> find path to object) or the name of an
			// attach pt or the name of a clothing layer
			return answerOnChat(id, param, getFullPath(getItem(id), option));
		}
		else if (behav == "findfolder")
		{
			return answerOnChat(id, param,
								getFullPath(findCategoryUnderRlvShare(option)));
		}
		else if (behav == "findfolders")
		{
			std::string response;
			std::deque<std::string> options = parse(option, ";");
			S32 opt_count = options.size();
			if (opt_count)
			{
				const std::string& folder_to_find = options[0];
				const std::string& separator = opt_count > 1 ? options[1] : ",";
				std::deque<LLInventoryCategory*> cats =
					findCategoriesUnderRlvShare(folder_to_find);
				for (S32 i = 0, count = cats.size(); i < count; ++i)
				{
					if (i > 0)
					{
						response += separator;
					}
					response += getFullPath(cats[i]);
				}
			}
			return answerOnChat(id, param, response);
		}
		else if (behav.compare(0, 7, "getenv_") == 0)
		{
			return answerOnChat(id, param, getEnvironment(behav));
		}
		else if (behav.compare(0, 9, "getdebug_") == 0)
		{
			return answerOnChat(id, param, getDebugSetting(behav));
		}
		else if (behav == "getgroup")
		{
			LLUUID group_id = gAgent.getGroupID();
			std::string group_name = "none";
			if (group_id.notNull() && gCacheNamep)
			{
				gCacheNamep->getGroupName(group_id, group_name);
			}
			return answerOnChat(id, param, group_name);
		}
		else if (behav == "getcam_avdistmin")
		{
			std::stringstream str;
			str << std::fixed << mCamDistMin;
			return answerOnChat(id, param, str.str());
		}
		else if (behav == "getcam_avdistmax")
		{
			std::stringstream str;
			str << std::fixed << mCamDistMax;
			return answerOnChat(id, param, str.str());
		}
		else if (behav == "getcam_zoommin")
		{
			std::stringstream str;
			str << std::fixed << mCamZoomMin;
			return answerOnChat(id, param, str.str());
		}
		else if (behav == "getcam_zoommax")
		{
			std::stringstream str;
			str << std::fixed << mCamZoomMax;
			return answerOnChat(id, param, str.str());
		}
		else if (behav == "getcam_fovmin")
		{
			std::stringstream str;
			str << std::fixed << DEFAULT_FIELD_OF_VIEW / mCamZoomMax;
			return answerOnChat(id, param, str.str());
		}
		else if (behav == "getcam_fovmax")
		{
			std::stringstream str;
			str << std::fixed << DEFAULT_FIELD_OF_VIEW / mCamZoomMin;
			return answerOnChat(id, param, str.str());
		}
		else if (behav == "getcam_fov")
		{
			std::stringstream str;
			str << std::fixed << gViewerCamera.getView();
			return answerOnChat(id, param, str.str());
		}
		else if (behav == "getcam_textures")
		{
			LLUUID tex_id;
			if (mCamTexturesCustom)
			{
				tex_id = mCamTexturesCustom->getID();
			}
			return answerOnChat(id, param, tex_id.asString());
		}
		else if (param == "n" || param == "add")
		{
			if (behav == "unsit" && (mGotSit || mGotUnsit))
			{
				mSkipAll = true;
				LL_DEBUGS("RestrainedLove") << "Detected @unsit=n command right after @"
											<< (mGotSit ? "sit" : "unsit")
											<< "=force. Delaying." << LL_ENDL;
				return true;
			}
			add(id, behav, option);
		}
		else if (param == "y" || param == "rem")
		{
			remove(id, behav, option);
		}
		else if (behav == "clear")
		{
			clear(id, param);
		}
		else if (param == "force")
		{
			if ((mGotUnsit && (behav == "sit" || behav == "sitground")) ||
				(mGotSit && behav == "unsit"))
			{
				// When we just executed an (un)sit=force command in the queue,
				// skip any opposite (un)sit=force command and everything
				// following it, so to let some time for the viewer and server
				// to agree on the sitting status...
				mSkipAll = true;
				LL_DEBUGS("RestrainedLove") << "Detected @" << behav
											<< "=force command right after @"
											<< (mGotSit ? "sit*" : "unsit")
											<< "=force. Delaying." << LL_ENDL;
				return true;
			}
			return force(id, behav, option);
		}
		else
		{
			return false;
		}
	}
	else
	{
		LL_DEBUGS("RestrainedLove") << id << ": "
									<< (behav == " " ? "Cancelling @relayed"
													 : behav)
									<< LL_ENDL;
		if (behav == "clear")
		{
			clear(id);
		}
		else if (behav == "relayed")
		{
			mRelays.emplace(id);
		}
		else if (behav == " ") // A single space means "end relayed"
		{
			mRelays.erase(id);
		}
		else
		{
			return false;
		}
	}

	return true;
}

void RLInterface::fireCommands()
{
	// Do not execute queued commands if the avatar is not yet fully baked !
	if (!LLStartUp::isLoggedIn() || !isAgentAvatarValid() ||
		(!mAssetsToReattach.empty() && !mReattachTimeout) ||
		!gAppearanceMgr.isAvatarFullyBaked())
	{
		return;
	}

	// Check if the last @sit=force or @unsit=force has been executed
	bool is_sitting = gAgentAvatarp->mIsSitting;
	if (mGotSit && is_sitting)
	{
		mSkipAll = mGotSit = false;
	}
	if (mGotUnsit && !is_sitting)
	{
		mSkipAll = mGotUnsit = false;
	}
	if (mSkipAll && mSitUnsitDelayTimer.getElapsedTimeF32() > 1.f)
	{
		llwarns << "Timeout waiting for " << (mGotSit ? "sit" : "unsit")
				<< " event. Resuming command queue processing." << llendl;
		mSkipAll = mGotSit = mGotUnsit = false;
	}

	if (mQueuedCommands.empty())
	{
		return;
	}
	LL_DEBUGS("RestrainedLove") << "Number of currently queued commands: "
								<< mQueuedCommands.size() << LL_ENDL;

	while (!mQueuedCommands.empty() && !mSkipAll)
	{
		S32 result = HBFloaterRLV::EXECUTED;
		const RLCommand& cmd = mQueuedCommands[0];
		mLastCmdBlacklisted = false;
		if (handleCommand(cmd.mId, cmd.mCommand))
		{
			// "Success" executing this command (which could as well have been
			// black-listed and thus ignored).
			if (mLastCmdBlacklisted)
			{
				mLastCmdBlacklisted = false;
				result = HBFloaterRLV::BLACKLISTED;
			}
		}
		else
		{
			// Failure executing this command
			result = HBFloaterRLV::FAILED;
		}
		HBFloaterRLV::logCommand(cmd.mId, cmd.mName, cmd.mCommand, result);

		mQueuedCommands.pop_front();
	}
	LL_DEBUGS("RestrainedLove") << "Number of remaining queued commands: "
								<< mQueuedCommands.size() << LL_ENDL;
}

void RLInterface::queueCommand(const LLUUID& id, const std::string& name,
							   const std::string& command)
{
	// Never queue any of the @version* and @getcommand commands: answer them
	// immediately. These commands are likely to be sent as soon as a scripted
	// RLV attachment rezzes as a form of "ping" to discover whether the viewer
	// supports RestrainedLove or not, and with what features; since we delay
	// other commands processing after full rezzing and baking of the agent
	// (which may take an indeterminate amount of time, especially if the
	// inventory cache got emptied before login), we cannot risk having the
	// attachment timing out on us...
	if (command.compare(0, 7, "version") == 0 ||
		command.compare(0, 10, "getcommand") == 0)
	{
		if (handleCommand(id, command))
		{
			// Success executing this command. Note: "version" and "getcommand"
			// cannot be black-listed, so we do not check for it.
			HBFloaterRLV::logCommand(id, name, command);
		}
		else
		{
			// Failure executing this command
			HBFloaterRLV::logCommand(id, name, command, HBFloaterRLV::FAILED);
		}
	}
	else
	{
		// A single space means "end relayed": do not log it.
		if (command != " ")
		{
			HBFloaterRLV::logCommand(id, name, command, HBFloaterRLV::QUEUED);
		}
		mQueuedCommands.emplace_back(id, name.empty() ? id.asString() : name,
									 command);
	}
}

void RLInterface::queueCommands(const LLUUID& id, const std::string& name,
								const std::string& cmd_line)
{
	// Check whether the command is a single one or instead a coma-separated
	// list of commands, and act accordingly.
	if (cmd_line.find(',') != std::string::npos)
	{
		bool has_relayed = false;
		std::deque<std::string> list_of_commands = parse(cmd_line, ",");
		for (U32 i = 0; i < list_of_commands.size(); ++i)
		{
			const std::string& command = list_of_commands[i];
			if (command.length() > 1 && command[0] != ' ')
			{
				queueCommand(id, name, command);
			}
			if (command == "relayed")
			{
				has_relayed = true;
			}
		}
		if (has_relayed)
		{
			// A single space means "end relayed"
			queueCommand(id, name, " ");
		}
	}
	else if (cmd_line != "relayed")	// a single @relayed command is a NOP
	{
		queueCommand(id, name, cmd_line);
	}
}

void RLInterface::storeLastStandingLoc(bool force)
{
	if (force || (isAgentAvatarValid() && !gAgentAvatarp->mIsSitting))
	{
		// We are now standing, and we want to sit down => store our current
		// location so that we can snap back here when we stand up, if under
		// @standtp
		LLVector3d pos = LLVector3d(gAgent.getPositionGlobal());
		mLastStandingLocation = pos;
		gSavedPerAccountSettings.setVector3d("RestrainedLoveLastStandingLocation",
											 pos);
		mHandleBackToLastStanding = false;
	}
}

void RLInterface::validateLastStandingLoc()
{
	if (!gRLenabled || (!mContainsStandtp && !mHandleBackToLastStanding))
	{
		// Reset this position to zero if not restricted with @standtp
		gSavedPerAccountSettings.setVector3d("RestrainedLoveLastStandingLocation",
											 LLVector3d(0.0, 0.0, 0.0));
	}
}

void RLInterface::restoreLastStandingLoc()
{
	mLastStandingLocation =
		gSavedPerAccountSettings.getVector3d("RestrainedLoveLastStandingLocation");
	mHandleBackToLastStanding = !mLastStandingLocation.isExactlyZero();
}

static void force_sit(const LLUUID& object_id)
{
	LL_DEBUGS("RestrainedLove") << "Attempting to force-sit agent on object: "
								<< object_id << LL_ENDL;
	LLViewerObject* objectp = gObjectList.findObject(object_id);
	if (!objectp)
	{
		LL_DEBUGS("RestrainedLove") << "Object not found !" << LL_ENDL;
		return;
	}

	LLViewerRegion* regionp = objectp->getRegion();
	if (!regionp)
	{
		LL_DEBUGS("RestrainedLove") << "Region not found for object."
									<< LL_ENDL;
		return;
	}

	if (isAgentAvatarValid() && gAgentAvatarp->mIsSitting)
	{
		if (gRLInterface.mContainsUnsit)
		{
			// Do not allow a script to force the avatar to sit somewhere
			// if already forced to stay sitting here
			LL_DEBUGS("RestrainedLove") << "@unsit=n in force. Aborting."
										<< LL_ENDL;
			return;
		}

		LLViewerObject* parent = (LLViewerObject*)gAgentAvatarp->getParent();
		if (parent && parent->getID() == object_id)
		{
			// Already sitting there !
			LL_DEBUGS("RestrainedLove") << "Already sitting on that object."
										<< LL_ENDL;
			return;
		}
	}

	if (gRLInterface.mContainsInteract || gRLInterface.contains("sit"))
	{
		LL_DEBUGS("RestrainedLove") << "Not permitted to force-sit."
									<< LL_ENDL;
		return;
	}

	// Store our current standing location if adequate and possible
	gRLInterface.storeLastStandingLoc();

	LL_DEBUGS("RestrainedLove") << "Sending the sit request to the server."
								<< LL_ENDL;
	LL_DEBUGS("AgentSit") << "RestrainedLove sending agent sit on object request"
						  << LL_ENDL;
	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessageFast(_PREHASH_AgentRequestSit);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlockFast(_PREHASH_TargetObject);
	msg->addUUIDFast(_PREHASH_TargetID, objectp->mID);
#if 0	// Note: for seats without a sit target, transmitting the offset
		// results in a sit failure with "There is no suitable surface to sit
		// on" message, while transmitting a 0 offset seems to work, as long as
		// the seat is close to the avatar (8 meters away at most)...
	msg->addVector3Fast(_PREHASH_Offset,
						gAgent.calcFocusOffset(objectp,
											   gAgent.getPositionAgent(),
											   0, 0));
#else
	msg->addVector3Fast(_PREHASH_Offset, LLVector3::zero);
#endif
	regionp->sendReliableMessage();
}

void RLInterface::backToLastStandingLoc()
{
	if (!mLastStandingLocation.isExactlyZero() && !LLApp::isExiting())
	{
		// Verify that a TP on the agent parcel would not cause the said agent
		// to either fail to TP (blocked TPs) or be TPed to a landing point.
		LLParcel* parcel = gViewerParcelMgr.getAgentParcel();
		if (parcel)
		{
			S32 type = parcel->getLandingType();
			if (type == LLParcel::L_NONE ||
				(type == LLParcel::L_LANDING_POINT &&
				 !parcel->getUserLocation().isExactlyZero()))
			{
				static uuid_list_t warned_parcels;
				const LLUUID& parcel_id = parcel->getID();
				if (warned_parcels.count(parcel_id) == 0 &&
					LLViewerParcelMgr::isParcelModifiableByAgent(parcel,
																 GP_LAND_SET_LANDING_POINT))
				{
					warned_parcels.emplace(parcel_id);
					gNotifications.add("RLVStandtpFailsOnRoutedParcel");
				}
				else
				{
					llwarns << "Cannot enforce @standtp on a parcel with teleport routing..."
							<< llendl;
				}
				return;
			}
		}
		mSnappingBackToLastStandingLocation = true;
		gAgent.teleportViaLocationLookAt(mLastStandingLocation);
		mSnappingBackToLastStandingLocation = false;
		mHandleBackToLastStanding = false;
	}
}

static void force_tp_callback(U64 handle, const LLVector3& pos_region,
							  bool keep_lookat)
{
	if (handle)
	{
		LLVector3d pos_global = from_region_handle(handle) +
								LLVector3d(pos_region);
		if (keep_lookat)
		{
			gAgent.teleportViaLocationLookAt(pos_global);
		}
		else
		{
			gAgent.teleportViaLocation(pos_global);
		}
	}
}

// Note: "location" must be X/Y/Z where X, Y and Z are ABSOLUTE coordinates =>
// use a script in-world to translate from local to global.
bool RLInterface::forceTeleport(const std::string& location, bool keep_lookat)
{
	std::string region_name;
	S32 x = 128;
	S32 y = 128;
	S32 z = 0;
	std::deque<std::string> tokens = parse(location, "/");
	if (tokens.size() == 3)
	{
		x = atoi(tokens[0].c_str());
		y = atoi(tokens[1].c_str());
		z = atoi(tokens[2].c_str());
	}
	else if (tokens.size() == 4)
	{
		region_name = tokens[0];
		x = atoi(tokens[1].c_str());
		y = atoi(tokens[2].c_str());
		z = atoi(tokens[3].c_str());
	}
	else
	{
		return false;
	}

	LL_DEBUGS("RestrainedLove") << "Location = '" << location
								<< "' decoded as: " << x << "," << y << ","
								<< z << " - Region name: " << region_name
								<< LL_ENDL;

	// Will be checked once receiving the tp order from the sim, then set to
	// true again:
	mAllowCancelTp = false;

	if (region_name.empty())
	{
		LLVector3d pos_global((F32)x, (F32)y, (F32)z);
		if (keep_lookat)
		{
			gAgent.teleportViaLocationLookAt(pos_global);
		}
		else
		{
			gAgent.teleportViaLocation(pos_global);
		}
	}
	else
	{
		const LLVector3 pos_local((F32)x, (F32)y, (F32)z);
		LLWorldMap::url_callback_t cb = boost::bind(&force_tp_callback, _1,
													pos_local, keep_lookat);
		gWorldMap.sendNamedRegionRequest(region_name, cb, "", true);
	}

	return true;
}

bool RLInterface::force(const LLUUID& obj_id, std::string command,
						std::string option)
{
	LL_DEBUGS("RestrainedLove") << command << " / " << option << LL_ENDL;

	mLastCmdBlacklisted = false;

	// Check the command against the blacklist
	if (isBlacklisted(obj_id, command, option, true))
	{
		mLastCmdBlacklisted = true;
		if (!option.empty())
		{
			command += ":" + option;
		}
		llinfos << "Blacklisted RestrainedLove command: " << command
				<< "=force for object " << obj_id << llendl;
		return true;
	}

	// RLVa allows #RLV/ to be used at the start of the path in an option, so
	// support it too for compatibility.
	if (option.compare(0, RL_HRLVS_LENGTH, RL_RLV_REDIR_FOLDER_PREFIX) == 0)
	{
		// Remove #RLV/, keep the tilde.
		option.erase(0, RL_HRLVS_LENGTH);
	}

	bool res = true;
	mHandleNoStrip = true;

	if (command == "sit")
	{							// sit:UUID
		bool allowed_to_sittp = true;
		if (!isAllowed(obj_id, "sittp"))
		{
			allowed_to_sittp = false;
			remove(obj_id, "sittp", "");
		}
		LLUUID id(option);
		force_sit(id);
		mGotSit = true;
		mSitUnsitDelayTimer.reset();
		if (!allowed_to_sittp)
		{
			add(obj_id, "sittp", "");
		}
	}
	else if (command == "sitground")
	{
		if (isAgentAvatarValid() &&
			// Verify we are not already sat on ground...
			!(gAgentAvatarp->mIsSitting && mSitTargetId.isNull()))
		{
			mGotSit = true;
			if (gAgentAvatarp->mIsSitting)
			{
				mSitGroundOnStandUp = true;
				gAgent.setControlFlags(AGENT_CONTROL_STAND_UP);
			}
			else
			{
				gAgent.setFlying(false);
				gAgent.clearControlFlags(AGENT_CONTROL_STAND_UP);
				gAgent.setControlFlags(AGENT_CONTROL_SIT_ON_GROUND);
				storeLastStandingLoc(true);
			}
		}
	}
	else if (command == "unsit")
	{							// unsit
		LL_DEBUGS("RestrainedLove") << "trying to unsit" << LL_ENDL;
		if (isAgentAvatarValid() && gAgentAvatarp->mIsSitting)
		{
			LL_DEBUGS("RestrainedLove") << "Found sitting avatar object"
										<< LL_ENDL;
			if (mContainsUnsit)
			{
				LL_DEBUGS("RestrainedLove") << "prevented from unsitting" << LL_ENDL;
			}
			else
			{
				LL_DEBUGS("RestrainedLove") << "unsitting agent" << LL_ENDL;
				mGotUnsit = true;
				mSitUnsitDelayTimer.reset();
				LL_DEBUGS("AgentSit") << "Sending agent unsit request"
									  << LL_ENDL;
				gAgent.setControlFlags(AGENT_CONTROL_STAND_UP);
				send_agent_update(true, true);
				if (mContainsStandtp)
				{
					backToLastStandingLoc();
				}
			}
		}
	}
	else if (command == "remoutfit")
	{							// remoutfit or remoutfit:shoes
		if (option.empty())
		{
			gAgentWearables.removeWearable(LLWearableType::WT_GLOVES, true, 0);
			gAgentWearables.removeWearable(LLWearableType::WT_JACKET, true, 0);
			gAgentWearables.removeWearable(LLWearableType::WT_PANTS, true, 0);
			gAgentWearables.removeWearable(LLWearableType::WT_SHIRT, true, 0);
			gAgentWearables.removeWearable(LLWearableType::WT_SHOES, true, 0);
			gAgentWearables.removeWearable(LLWearableType::WT_SKIRT, true, 0);
			gAgentWearables.removeWearable(LLWearableType::WT_SOCKS, true, 0);
			gAgentWearables.removeWearable(LLWearableType::WT_UNDERPANTS, true,
										   0);
			gAgentWearables.removeWearable(LLWearableType::WT_UNDERSHIRT, true,
										   0);
			gAgentWearables.removeWearable(LLWearableType::WT_ALPHA, true, 0);
			gAgentWearables.removeWearable(LLWearableType::WT_TATTOO, true, 0);
			gAgentWearables.removeWearable(LLWearableType::WT_UNIVERSAL, true,
										   0);
			gAgentWearables.removeWearable(LLWearableType::WT_PHYSICS, true,
										   0);
		}
		else
		{
			LLWearableType::EType type = getOutfitLayerAsType(option);
			if (type != LLWearableType::WT_INVALID)
			{
				// clothes only, not skin, eyes, hair or shape
				if (LLWearableType::getAssetType(type) ==
					LLAssetType::AT_CLOTHING)
				{
					// Remove by layer
					gAgentWearables.removeWearable(type, true, 0);
				}
			}
			else
			{
				// Remove by category (in RLV share)
				forceDetachByName(option, false);
			}
		}
	}
	else if (command == "detach" || command == "remattach")
	{
		// detach:chest=force OR detach:restraints/cuffs=force (@remattach is a
		// synonym). If option is an UUID, detach the corresponding object.
		if (LLUUID::validate(option))
		{
			res = forceDetachByUuid(option);
		}
		else
		{
			LLViewerJointAttachment* attachpt =
				findAttachmentPointFromName(option, true); // Exact name
			if (attachpt || option.empty())
			{
				res = forceDetach(option);	// Remove by attach pt
			}
			else
			{
				res = forceDetachByName(option, false);
			}
		}
	}
	else if (command == "detachme")
	{
		// detachme=force to detach this object specifically
		res = forceDetachByUuid(obj_id.asString());	// Remove by uuid
	}
	else if (command == "detachthis")
	{
		// detachthis=force to detach the folder containing this object. If
		// option is an UUID, we do not detach the folder containing the
		// calling object, but the referenced object instead.
		std::string pathes_str;
		if (LLUUID::validate(option))
		{
			pathes_str = getFullPath(getItem(LLUUID(option)));
		}
		else
		{
			pathes_str = getFullPath(getItem(obj_id), option);
		}
		std::deque<std::string> pathes = parse(pathes_str, ",");
		for (U32 i = 0; i < pathes.size(); ++i)
		{
			res &= forceDetachByName(pathes[i], false);
		}
	}
	else if (command == "detachall")
	{
		// detachall:cuffs=force to detach a folder and its subfolders
		res = forceDetachByName(option, true);
	}
	else if (command == "detachallthis")
	{
		// detachallthis=force to detach the folder containing this object and
		// also its subfolders. If option is an UUID, we do not detach the
		// folder containing the calling object, but the referenced object
		// instead.
		std::string pathes_str;
		if (LLUUID::validate(option))
		{
			pathes_str = getFullPath(getItem(LLUUID(option)));
		}
		else
		{
			pathes_str = getFullPath(getItem(obj_id), option);
		}
		std::deque<std::string> pathes = parse(pathes_str, ",");
		for (U32 i = 0; i < pathes.size(); ++i)
		{
			res &= forceDetachByName(pathes[i], true);
		}
	}
	else if (command == "tpto")
	{
		bool keep_lookat = false;
		// tpto:[region/]X/Y/Z=force(X, Y, Z are local or global coordinates,
		// depending on the presence of the region name or not)
		size_t i = option.find(";");
		if (i != std::string::npos && i + 1 < option.length())
		{
			// Strip off the "lookat" vector: we do not support it.
			option = option.substr(0, i);
			// Instead, pass a flag telling there was a lookat vector, and use
			// that in the teleport function to keep facing in the same
			// direction after TP as before it.
			keep_lookat = true;
		}
		bool allowed_to_tploc = true;
		bool allowed_to_local = true;
		bool allowed_to_unsit = true;
		bool allowed_to_sittp = true;
		if (!isAllowed(obj_id, "tploc"))
		{
			allowed_to_tploc = false;
			remove(obj_id, "tploc", "");
		}
		if (!isAllowed(obj_id, "tplocal"))
		{
			allowed_to_local = false;
			remove(obj_id, "tplocal", "");
		}
		if (!isAllowed(obj_id, "unsit"))
		{
			allowed_to_unsit = false;
			remove(obj_id, "unsit", "");
		}
		if (!isAllowed(obj_id, "sittp"))
		{
			allowed_to_sittp = false;
			remove(obj_id, "sittp", "");
		}
		res = forceTeleport(option, keep_lookat);
		if (!allowed_to_tploc)
		{
			add(obj_id, "tploc", "");
		}
		if (!allowed_to_local)
		{
			add(obj_id, "tplocal", "");
		}
		if (!allowed_to_unsit)
		{
			add(obj_id, "unsit", "");
		}
		if (!allowed_to_sittp)
		{
			add(obj_id, "sittp", "");
		}
	}
	else if (command == "attach" || command == "addoutfit")
	{
		// attach:cuffs=force
		// Will have to be changed back to AttachReplace eventually, but not
		// before a clear and early communication
		 forceAttach(option, false, AttachOverOrReplace);
	}
	else if (command == "attachover" || command == "addoutfitover")
	{
		// attachover:cuffs=force
		forceAttach(option, false, AttachOver);
	}
	else if (command == "attachoverorreplace" ||
			 command == "addoutfitoverorreplace")
	{
		// attachoverorreplace:cuffs=force
		forceAttach(option, false, AttachOverOrReplace);
	}
	else if (command == "attachthis" || command == "addoutfitthis")
	{
		// attachthis=force to attach the folder containing this object
		std::string pathes_str = getFullPath(getItem(obj_id), option);
		if (!pathes_str.empty())
		{
			std::deque<std::string> pathes = parse(pathes_str, ",");
			for (U32 i = 0; i < pathes.size(); ++i)
			{
				// Will have to be changed back to AttachReplace eventually,
				// but not before a clear and early communication
				forceAttach(pathes[i], false, AttachOverOrReplace);
			}
		}
	}
	else if (command == "attachthisover" || command == "addoutfitthisover")
	{
		// attachthisover=force to attach the folder containing this object
		std::string pathes_str = getFullPath(getItem(obj_id), option);
		if (!pathes_str.empty())
		{
			std::deque<std::string> pathes = parse(pathes_str, ",");
			for (U32 i = 0; i < pathes.size(); ++i)
			{
				forceAttach(pathes[i], false, AttachOver);
			}
		}
	}
	else if (command == "attachthisoverorreplace" ||
			 command == "addoutfitthisoverorreplace")
	{
		// attachthisoverorreplace=force to attach the folder containing this
		// object
		std::string pathes_str = getFullPath(getItem(obj_id), option);
		if (!pathes_str.empty())
		{
			std::deque<std::string> pathes = parse(pathes_str, ",");
			for (U32 i = 0; i < pathes.size(); ++i)
			{
				forceAttach(pathes[i], false, AttachOverOrReplace);
			}
		}
	}
	else if (command == "attachall" || command == "addoutfitall")
	{
		// attachall:cuffs=force to attach a folder and its subfolders. Will
		// have to be changed back to AttachReplace eventually, but not before
		// a clear and early communication.
		forceAttach(option, true, AttachOverOrReplace);
	}
	else if (command == "attachallover" || command == "addoutfitallover")
	{
		// attachallover:cuffs=force to attach a folder and its subfolders
		forceAttach(option, true, AttachOver);
	}
	else if (command == "attachalloverorreplace" ||
			 command == "addoutfitalloverorreplace")
	{
		// attachalloverorreplace:cuffs=force to attach a folder and its
		// subfolders
		forceAttach(option, true, AttachOverOrReplace);
	}
	else if (command == "attachallthis" || command == "addoutfitallthis")
	{
		// attachallthis=force to attach the folder containing this object and
		// its subfolders
		std::string pathes_str = getFullPath(getItem(obj_id), option);
		if (!pathes_str.empty())
		{
			std::deque<std::string> pathes = parse(pathes_str, ",");
			for (U32 i = 0; i < pathes.size(); ++i)
			{
				// Will have to be changed back to AttachReplace eventually,
				// but not before a clear and early communication
				forceAttach(pathes[i], true, AttachOverOrReplace);
			}
		}
	}
	else if (command == "attachallthisover" ||
			 command == "addoutfitallthisover")
	{
		// attachallthisover=force to attach the folder containing this object
		// and its subfolders
		std::string pathes_str = getFullPath(getItem(obj_id), option);
		if (!pathes_str.empty())
		{
			std::deque<std::string> pathes = parse(pathes_str, ",");
			for (U32 i = 0; i < pathes.size(); ++i)
			{
				forceAttach(pathes[i], true, AttachOver);
			}
		}
	}
	else if (command == "attachallthisoverorreplace" ||
			 command == "addoutfitallthisoverorreplace")
	{
		// attachallthisoverorreplace=force to attach the folder containing
		// this object and its subfolders
		std::string pathes_str = getFullPath(getItem(obj_id), option);
		if (!pathes_str.empty())
		{
			std::deque<std::string> pathes = parse(pathes_str, ",");
			for (U32 i = 0; i < pathes.size(); ++i)
			{
				forceAttach(pathes[i], true, AttachOverOrReplace);
			}
		}
	}
	else if (command.compare(0, 7, "setenv_") == 0)
	{
		bool allowed = true;
		if (!isAllowed(obj_id, "setenv"))
		{
			allowed = false;
			remove(obj_id, "setenv", "");
		}
		if (!mContainsSetenv)
		{
			res = forceEnvironment(command, option);
		}
		if (!allowed)
		{
			add(obj_id, "setenv", "");
		}
	}
	else if (command.compare(0, 9, "setdebug_") == 0)
	{
		bool allowed = true;
		if (!isAllowed(obj_id, "setdebug"))
		{
			allowed = false;
			remove(obj_id, "setdebug", "");
		}
		if (!contains("setdebug"))
		{
			res = forceDebugSetting(command, option);
		}
		if (!allowed)
		{
			add(obj_id, "setdebug", "");
		}
	}
	else if (command == "setrot")
	{
		// setrot:angle_radians=force
		F32 val = atof(option.c_str());
		gAgent.startCameraAnimation();
		LLVector3 rot(0.f, 1.f, 0.f);
		rot = rot.rotVec(-val, LLVector3::z_axis);
		rot.normalize();
		gAgent.resetAxes(rot);
	}
	else if (command == "adjustheight")
	{
		// adjustheight:adjustment_centimeters=force or
		// adjustheight:ref_pelvis_to_foot;scalar[;delta]=force
		if (isAgentAvatarValid())
		{
			F32 val = (F32)atoi(option.c_str()) / 100.f;
			size_t i = option.find(';');
			if (i != std::string::npos && i + 1 < option.length())
			{
				F32 scalar = (F32)atof(option.substr(i + 1).c_str());
				if (scalar != 0.f)
				{
					LL_DEBUGS("RestrainedLove") << "Pelvis to foot = "
												<< gAgentAvatarp->getPelvisToFoot()
												<< "m" << LL_ENDL;
					val = (atof(option.c_str()) -
						   gAgentAvatarp->getPelvisToFoot()) * scalar;
					option = option.substr(i + 1);
					i = option.find(';');
					if (i != std::string::npos && i + 1 < option.length())
					{
						val += (F32)atof(option.substr(i + 1).c_str());
					}
				}
			}
			if (!LLVOAvatarSelf::canUseServerBaking() ||
				LLVOAvatarSelf::useAvatarHoverHeight())
			{
				gSavedSettings.setF32("AvatarOffsetZ", val);
			}
		}
	}
	else if (command == "setgroup")
	{
		std::string target_group_name = option;
		LLStringUtil::toLower(target_group_name);
		// Note: "none" is not localized here because a script should not have
		// to bother about viewer language
		if (target_group_name == "none")
		{
			gAgent.setGroup(LLUUID::null);
		}
		else
		{
			std::string name;
			for (S32 i = 0, count = gAgent.mGroups.size(); i < count; ++i)
			{
				const LLGroupData& gdatap = gAgent.mGroups[i];
				name = gdatap.mName;
				LLStringUtil::toLower(name);
				if (name == target_group_name)
				{
					gAgent.setGroup(gdatap.mID);
					break;
				}
			}
		}
	}
	else if (command == "setcam_fov")
	{
		F32 new_fov_rad = atof(option.c_str());
		gViewerCamera.setDefaultFOV(new_fov_rad);
		// setView() may have clamped it:
		gSavedSettings.setF32("CameraAngle", gViewerCamera.getView());
	}
	else
	{
		// Unknown command
		res = false;
	}

	mHandleNoStrip = false;
	return res;
}

void RLInterface::removeWearableItemFromAvatar(LLViewerInventoryItem* item_or_link)
{
	if (!item_or_link)
	{
		return;
	}

	LLViewerInventoryItem* item = item_or_link;
	if (LLViewerInventoryItem* linked_item = item_or_link->getLinkedItem())
	{
		item = linked_item;
	}

	if (item->getInventoryType() != LLInventoryType::IT_WEARABLE ||
		!canUnwear(item))
	{
		return;
	}

	LLViewerWearable* wearable;
	wearable = gAgentWearables.getWearableFromItemID(item->getUUID());
	if (!wearable)
	{
		return;
	}

	LLWearableType::EType type = wearable->getType();
	U32 index;
	if (gAgentWearables.getWearableIndex(wearable, index))
	{
		gAgentWearables.removeWearable(type, false, index);
	}
}

bool RLInterface::answerOnChat(const LLUUID& obj_id,
							   const std::string& channel, std::string msg)
{
	S32 chan = (S32)atoi(channel.c_str());
	if (chan == 0)
	{
		// Protection against abusive "@getstatus=0" commands, or against a
		// non-numerical channel
		return false;
	}
	if (msg.length() > (size_t)(chan > 0 ? 1023 : 254))
	{
		llwarns << "Too large an answer: maximum is "
				<< (chan > 0 ? "1023 characters"
							 : "254 characters for a negative channel")
				<< ". Truncated reply." << llendl;
		msg = msg.substr(0, (size_t)(chan > 0 ? 1022 : 254));
	}
	LLMessageSystem* msgsys = gMessageSystemp;
	if (chan > 0)
	{
		msgsys->newMessageFast(_PREHASH_ChatFromViewer);
		msgsys->nextBlockFast(_PREHASH_AgentData);
		msgsys->addUUIDFast(_PREHASH_AgentID, gAgentID);
		msgsys->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
		msgsys->nextBlockFast(_PREHASH_ChatData);
		msgsys->addStringFast(_PREHASH_Message, msg);
		msgsys->addU8Fast(_PREHASH_Type, CHAT_TYPE_SHOUT);
		msgsys->addS32(_PREHASH_Channel, chan);
	}
	else
	{
		msgsys->newMessage(_PREHASH_ScriptDialogReply);
		msgsys->nextBlock(_PREHASH_AgentData);
		msgsys->addUUID(_PREHASH_AgentID, gAgentID);
		msgsys->addUUID(_PREHASH_SessionID, gAgentSessionID);
		msgsys->nextBlock(_PREHASH_Data);
		msgsys->addUUID(_PREHASH_ObjectID, gAgentID);
		msgsys->addS32(_PREHASH_ChatChannel, chan);
		msgsys->addS32(_PREHASH_ButtonIndex, 1);
		msgsys->addString(_PREHASH_ButtonLabel, msg);
	}
	gAgent.sendReliableMessage();

	if (gAutomationp)
	{
		gAutomationp->onRLVAnswerOnChat(obj_id, chan, msg);
	}

	LL_DEBUGS("RestrainedLove") << "/" << chan << " " << msg << LL_ENDL;

	return true;
}

std::string RLInterface::crunchEmote(const std::string& msg, U32 truncate_to)
{
	if (msg.empty())
	{
		return LLStringUtil::null;
	}

	std::string crunched = msg;

	if (msg.compare(0, 4, "/me ") == 0 || msg.compare(0, 4, "/me'") == 0)
	{
		// Only allow emotes without "spoken" text.
		// Forbid text containing any symbol which could be used as quotes.
		if (msg.find('"') != std::string::npos ||
			msg.find("''") != std::string::npos ||
			msg.find('(') != std::string::npos ||
			msg.find(')') != std::string::npos ||
			msg.find(" -") != std::string::npos ||
			msg.find("- ") != std::string::npos ||
			msg.find('*') != std::string::npos ||
			msg.find('=') != std::string::npos ||
			msg.find('^') != std::string::npos ||
			msg.find('_') != std::string::npos ||
			msg.find('~') != std::string::npos)
		{
			crunched = "...";
		}
		else if (truncate_to > 0 && !sUntruncatedEmotes && !contains("emote"))
		{
			// Only allow short emotes.
			size_t i = msg.find('.');
			if (i != std::string::npos)
			{
				crunched = msg.substr(0, ++i);
			}
			if (crunched.length() > truncate_to)
			{
				crunched = crunched.substr(0, truncate_to);
			}
		}
	}
	else if (msg[0] == '/')
	{
		// Only allow short gesture names(to avoid cheats).
		if (msg.length() > 7)
		{						// allows things like "/ao off", "/hug X"
			crunched = "...";
		}
	}
	else if (!sCanOoc || msg.compare(0, 2, "((") != 0 ||
			 msg.find("))") != msg.length() - 2)
	{
		// Only allow OOC chat, starting with "((" and ending with "))".
		crunched = "...";
	}
	return crunched;
}

std::string RLInterface::getOutfitLayerAsString(LLWearableType::EType layer)
{
	switch (layer)
	{
		case LLWearableType::WT_SKIN:
			return WS_SKIN;
		case LLWearableType::WT_GLOVES:
			return WS_GLOVES;
		case LLWearableType::WT_JACKET:
			return WS_JACKET;
		case LLWearableType::WT_PANTS:
			return WS_PANTS;
		case LLWearableType::WT_SHIRT:
			return WS_SHIRT;
		case LLWearableType::WT_SHOES:
			return WS_SHOES;
		case LLWearableType::WT_SKIRT:
			return WS_SKIRT;
		case LLWearableType::WT_SOCKS:
			return WS_SOCKS;
		case LLWearableType::WT_UNDERPANTS:
			return WS_UNDERPANTS;
		case LLWearableType::WT_UNDERSHIRT:
			return WS_UNDERSHIRT;
		case LLWearableType::WT_ALPHA:
			return WS_ALPHA;
		case LLWearableType::WT_TATTOO:
			return WS_TATTOO;
		case LLWearableType::WT_UNIVERSAL:
			return WS_UNIVERSAL;
		case LLWearableType::WT_PHYSICS:
			return WS_PHYSICS;
		case LLWearableType::WT_EYES:
			return WS_EYES;
		case LLWearableType::WT_HAIR:
			return WS_HAIR;
		case LLWearableType::WT_SHAPE:
			return WS_SHAPE;
		default:
			return "";
	}
}

LLWearableType::EType RLInterface::getOutfitLayerAsType(const std::string& layer)
{
	if (layer == WS_SKIN)		return LLWearableType::WT_SKIN;
	if (layer == WS_GLOVES)		return LLWearableType::WT_GLOVES;
	if (layer == WS_JACKET)		return LLWearableType::WT_JACKET;
	if (layer == WS_PANTS)		return LLWearableType::WT_PANTS;
	if (layer == WS_SHIRT)		return LLWearableType::WT_SHIRT;
	if (layer == WS_SHOES)		return LLWearableType::WT_SHOES;
	if (layer == WS_SKIRT)		return LLWearableType::WT_SKIRT;
	if (layer == WS_SOCKS)		return LLWearableType::WT_SOCKS;
	if (layer == WS_UNDERPANTS)	return LLWearableType::WT_UNDERPANTS;
	if (layer == WS_UNDERSHIRT)	return LLWearableType::WT_UNDERSHIRT;
	if (layer == WS_ALPHA)		return LLWearableType::WT_ALPHA;
	if (layer == WS_TATTOO)		return LLWearableType::WT_TATTOO;
	if (layer == WS_UNIVERSAL)	return LLWearableType::WT_UNIVERSAL;
	if (layer == WS_PHYSICS)	return LLWearableType::WT_PHYSICS;
	if (layer == WS_EYES)		return LLWearableType::WT_EYES;
	if (layer == WS_HAIR)		return LLWearableType::WT_HAIR;
	if (layer == WS_SHAPE)		return LLWearableType::WT_SHAPE;
	return LLWearableType::WT_INVALID;
}

std::string RLInterface::getOutfit(const std::string& layer)
{
	if (layer == WS_SKIN)		return (gAgentWearables.getWearable(LLWearableType::WT_SKIN, 0) ? "1" : "0");
	if (layer == WS_GLOVES)		return (gAgentWearables.getWearable(LLWearableType::WT_GLOVES, 0) ? "1" : "0");
	if (layer == WS_JACKET)		return (gAgentWearables.getWearable(LLWearableType::WT_JACKET, 0) ? "1" : "0");
	if (layer == WS_PANTS)		return (gAgentWearables.getWearable(LLWearableType::WT_PANTS, 0) ? "1" : "0");
	if (layer == WS_SHIRT)		return (gAgentWearables.getWearable(LLWearableType::WT_SHIRT, 0) ? "1" : "0");
	if (layer == WS_SHOES)		return (gAgentWearables.getWearable(LLWearableType::WT_SHOES, 0) ? "1" : "0");
	if (layer == WS_SKIRT)		return (gAgentWearables.getWearable(LLWearableType::WT_SKIRT, 0) ? "1" : "0");
	if (layer == WS_SOCKS)		return (gAgentWearables.getWearable(LLWearableType::WT_SOCKS, 0) ? "1" : "0");
	if (layer == WS_UNDERPANTS)	return (gAgentWearables.getWearable(LLWearableType::WT_UNDERPANTS, 0) ? "1" : "0");
	if (layer == WS_UNDERSHIRT)	return (gAgentWearables.getWearable(LLWearableType::WT_UNDERSHIRT, 0) ? "1" : "0");
	if (layer == WS_ALPHA)		return (gAgentWearables.getWearable(LLWearableType::WT_ALPHA, 0) ? "1" : "0");
	if (layer == WS_TATTOO)		return (gAgentWearables.getWearable(LLWearableType::WT_TATTOO, 0) ? "1" : "0");
	if (layer == WS_UNIVERSAL)	return (gAgentWearables.getWearable(LLWearableType::WT_UNIVERSAL, 0) ? "1" : "0");
	if (layer == WS_PHYSICS)	return (gAgentWearables.getWearable(LLWearableType::WT_PHYSICS, 0) ? "1" : "0");
	if (layer == WS_EYES)		return (gAgentWearables.getWearable(LLWearableType::WT_EYES, 0) ? "1" : "0");
	if (layer == WS_HAIR)		return (gAgentWearables.getWearable(LLWearableType::WT_HAIR, 0) ? "1" : "0");
	if (layer == WS_SHAPE)		return (gAgentWearables.getWearable(LLWearableType::WT_SHAPE, 0) ? "1" : "0");
	return getOutfit(WS_GLOVES) + getOutfit(WS_JACKET) + getOutfit(WS_PANTS) +
		   getOutfit(WS_SHIRT) + getOutfit(WS_SHOES) + getOutfit(WS_SKIRT) +
		   getOutfit(WS_SOCKS) + getOutfit(WS_UNDERPANTS) + getOutfit(WS_UNDERSHIRT) +
		   getOutfit(WS_SKIN) + getOutfit(WS_EYES) + getOutfit(WS_HAIR) +
		   getOutfit(WS_SHAPE) + getOutfit(WS_ALPHA) + getOutfit(WS_TATTOO) +
		   getOutfit(WS_PHYSICS) + getOutfit(WS_UNIVERSAL);
}

std::string RLInterface::getAttachments(const std::string& attachpt)
{
	std::string res, name;
	if (!isAgentAvatarValid())
	{
		llwarns << "NULL avatar pointer. Aborting." << llendl;
		return res;
	}
	if (attachpt.empty())
	{
		res += "0";				// to match the LSL macros
	}

	for (LLVOAvatar::attachment_map_t::iterator
			iter = gAgentAvatarp->mAttachmentPoints.begin(),
			end = gAgentAvatarp->mAttachmentPoints.end();
		 iter != end; ++iter)
	{
		LLViewerJointAttachment* attachment = iter->second;
		name = attachment->getName();
		if (name == "Avatar Center")
		{
			name = "Root";
		}
		LLStringUtil::toLower(name);
		LL_DEBUGS("RestrainedLove") << "trying <" << name << ">" << LL_ENDL;
		if (attachpt.empty() || attachpt == name)
		{
			if (attachment->getNumObjects() > 0)
			{
				res += "1";		// attachment->getName();
			}
			else
			{
				res += "0";
			}
		}
	}
	return res;
}

std::string RLInterface::getStatus(const LLUUID& obj_id, std::string rule)
{
	std::string res, name;
	std::string separator = "/";
	// If rule contains a specification of the separator, extract it
	size_t ind = rule.find(";");
	if (ind != std::string::npos)
	{
		separator = rule.substr(ind + 1);
		rule = rule.substr(0, ind);
	}
	if (separator.empty())
	{
		// Prevent a hack to force the avatar to say something
		separator = "/";
	}

	rl_map_it_t it;
	if (obj_id.isNull())
	{
		it = mSpecialObjectBehaviours.begin();
	}
	else
	{
		it = mSpecialObjectBehaviours.find(obj_id.asString());
	}

	while (it != mSpecialObjectBehaviours.end() &&
		   (obj_id.isNull() ||
			it != mSpecialObjectBehaviours.upper_bound(obj_id.asString())))
	{
		if (rule.empty() || it->second.find(rule) != std::string::npos)
		{
			res += separator;
			res += it->second;
		}
		++it;
	}
	return res;
}

std::string RLInterface::getCommand(std::string match, bool blacklist)
{
	std::string res, command, name, temp;
	LLStringUtil::toLower(match);
	for (rl_command_map_it_t it = sCommandsMap.begin(),
							 end = sCommandsMap.end();
		 it != end; ++it)
	{
		command = it->first;
		size_t i = command.find("%f");
		bool force = i != std::string::npos;
		name = force ? command.substr(0, i) : command;
		temp = res + "/";
		if (match.empty() || command.find(match) != std::string::npos)
		{
			if (temp.find("/" + command + "/") == std::string::npos &&
				(blacklist || !isBlacklisted(LLUUID::null, name, "", force)))
			{
				res += "/" + command;
			}
		}
	}
	return res;
}

std::string RLInterface::getCommandsByType(S32 type, bool blacklist)
{
	std::string res, command, name, temp;
	for (rl_command_map_it_t it = sCommandsMap.begin(),
							 end = sCommandsMap.end();
		 it != end; ++it)
	{
		S32 cmdtype = (S32)it->second;
		if (cmdtype == type)
		{
			command = it->first;
			size_t i = command.find("%f");
			bool force = i != std::string::npos;
			name = force ? command.substr(0, i) : command;
			temp = res + "/";
			if (temp.find("/" + command + "/") == std::string::npos &&
				(blacklist || !isBlacklisted(LLUUID::null, name, "", force)))
			{
				res += "/" + command;
			}
		}
	}
	return res;
}

std::deque<std::string> RLInterface::getBlacklist(std::string filter)
{
	std::deque<std::string> list, res;
	list = parse(sBlackList, ",");
	res.clear();

	size_t size = list.size();
	for (size_t i = 0; i < size; ++i)
	{
		if (filter.empty() || list[i].find(filter) != std::string::npos)
		{
			res.emplace_back(list[i]);
		}
	}

	return res;
}

std::string RLInterface::getRlvRestrictions(const std::string& filter)
{
	LLUUID id;
	std::string res = "\n################ RLV RESTRICTIONS ################";
	std::string object_name, old_object_name;
	for (rl_map_it_t it = mSpecialObjectBehaviours.begin(),
					 end = mSpecialObjectBehaviours.end();
		 it != end; ++it)
	{
		id.set(it->first);
		LLInventoryItem* item = getItem(id);
		if (item)
		{
			object_name = item->getName();
		}
		if (filter.empty() || object_name.find(filter) != std::string::npos)
		{
			if (object_name.empty())
			{
				object_name = id.asString();
			}
			// print the name of the object
			if (object_name != old_object_name)
			{
				res += "\nObject: " + object_name;
			}
			res += "\n - " + it->second;
		}
		old_object_name = object_name;
		object_name.clear();
	}
	return res + "\n##################################################";
}

bool RLInterface::forceDetach(const std::string& attachpt)
{
	bool res = false;

	if (!isAgentAvatarValid()) return res;

	std::string name;
	for (LLVOAvatar::attachment_map_t::iterator
			iter = gAgentAvatarp->mAttachmentPoints.begin(),
			end = gAgentAvatarp->mAttachmentPoints.end();
		 iter != end; ++iter)
	{
		LLViewerJointAttachment* attachment = iter->second;
		if (!attachment) continue; // Paranoia
		name = attachment->getName();
		if (name == "Avatar Center")
		{
			name = "Root";
		}
		LLStringUtil::toLower(name);
		LL_DEBUGS("RestrainedLove") << "trying <" << name << ">" << LL_ENDL;

		if (attachpt.empty() || attachpt == name)
		{
			LL_DEBUGS("RestrainedLove") << "found => detaching" << LL_ENDL;
			detachAllObjectsFromAttachment(attachment);
			res = true;
		}
	}
	return res;
}

bool RLInterface::forceDetachByUuid(const std::string& object_id)
{
	bool res = false;

	if (!isAgentAvatarValid()) return res;

	LLViewerObject* object = gObjectList.findObject(LLUUID(object_id));
	if (object)
	{
		object = object->getRootEdit();
		for (LLVOAvatar::attachment_map_t::iterator
				iter = gAgentAvatarp->mAttachmentPoints.begin(),
				end = gAgentAvatarp->mAttachmentPoints.end();
			 iter != end; ++iter)
		{
			LLViewerJointAttachment* attachment = iter->second;
			if (attachment && attachment->isObjectAttached(object))
			{
				detachObject(object);
				res = true;
			}
		}
	}
	return res;
}

bool RLInterface::hasLockedHuds()
{
	if (!isAgentAvatarValid()) return false;

	for (S32 i = 0, count = gAgentAvatarp->mAttachedObjectsVector.size();
		 i < count; ++i)
	{
		LLViewerObject* objp = gAgentAvatarp->mAttachedObjectsVector[i].first;
		if (objp && objp->isHUDAttachment() && !canDetach(objp))
		{
			return true;
		}
	}

	return false;
}

std::deque<LLInventoryItem*> RLInterface::getListOfLockedItems(LLInventoryCategory* root)
{
	std::deque<LLInventoryItem*> res;
	std::deque<LLInventoryItem*> tmp;

	if (root && isAgentAvatarValid())
	{
		LLInventoryModel::cat_array_t* cats;
		LLInventoryModel::item_array_t* items;
		gInventory.getDirectDescendentsOf(root->getUUID(), cats, items);

		// Try to find locked items in the current category
		std::string attach_point_name;
		for (S32 i = 0, count = items->size(); i < count; ++i)
		{
			LLInventoryItem* item = (*items)[i];
			// If this is an object, add it if it is worn and locked, or worn
			// and its attach point is locked
			if (item && item->getType() == LLAssetType::AT_OBJECT)
			{
				LLViewerObject* attached_object;
				attached_object = gAgentAvatarp->getWornAttachment(item->getUUID());
				if (attached_object)
				{
					attach_point_name = gAgentAvatarp->getAttachedPointName(item->getLinkedUUID());
					if (!canDetach(attached_object))
					{
						LL_DEBUGS("RestrainedLove") << "Found a locked object: "
													<< item->getName()
													<< " on "
													<< attach_point_name
													<< LL_ENDL;
						res.push_back(item);
					}
				}
			}
			// If this is a piece of clothing, add it if the avatar cannot
			// unwear clothes, or if this layer itself cannot be unworn
			else if (item && item->getType() == LLAssetType::AT_CLOTHING)
			{
				if (contains("remoutfit") || containsSubstr("remoutfit:"))
				{
					LL_DEBUGS("RestrainedLove") << "Found a locked clothing: "
												<< item->getName() << LL_ENDL;
					res.push_back(item);
				}
			}
		}

		// We have all the locked objects contained directly in this folder,
		// now add all the ones contained in children folders recursively
		for (S32 i = 0, count = cats->size(); i < count; ++i)
		{
			LLInventoryCategory* cat = (*cats)[i];
			tmp = getListOfLockedItems(cat);
			for (S32 j = 0, count2 = tmp.size(); j < count2; ++j)
			{
				LLInventoryItem* item = tmp[j];
				if (item)
				{
					res.push_back(item);
				}
			}
		}

		LL_DEBUGS("RestrainedLove") << "Number of locked objects under "
									<< root->getName() << " =  " << res.size()
									<< LL_ENDL;
	}

	return res;
}

std::deque<std::string> RLInterface::getListOfRestrictions(const LLUUID& obj_id,
														   const std::string& rule)
{
	std::deque<std::string> res;
	std::string name;
	rl_map_it_t it;
	if (obj_id.isNull())
	{
		it = mSpecialObjectBehaviours.begin();
	}
	else
	{
		it = mSpecialObjectBehaviours.find(obj_id.asString());
	}
	while (it != mSpecialObjectBehaviours.end() &&
		   (obj_id.isNull() ||
			it != mSpecialObjectBehaviours.upper_bound(obj_id.asString())))
	{
		if (rule.empty() || it->second.find(rule) != std::string::npos)
		{
			res.emplace_back(it->second);
		}
		++it;
	}
	return res;
}

std::string RLInterface::getInventoryList(const std::string& path,
										  bool with_worn_info)
{
	std::string res;
	LLInventoryModel::cat_array_t* cats;
	LLInventoryModel::item_array_t* items;
	LLInventoryCategory* root = NULL;
	if (path.empty())
	{
		root = getRlvShare();
	}
	else
	{
		root = getCategoryUnderRlvShare(path);
	}

	if (root)
	{
		gInventory.getDirectDescendentsOf(root->getUUID(), cats, items);
		if (cats)
		{
			bool found_one = false;
			if (with_worn_info)
			{
				std::string worn_items = getWornItems(root);
				res += "|";
				found_one = true;
				if (worn_items == "n")
				{
					res += "10";
				}
				else if (worn_items == "N")
				{
					res += "30";
				}
				else
				{
					res += worn_items;
				}
			}
			for (S32 i = 0, count = cats->size(); i < count; ++i)
			{
				LLInventoryCategory* cat = (*cats)[i];
				const std::string& name = cat->getName();
				if (!name.empty() && name[0] != '.' &&
					(!mHandleNoRelay ||
					 name.find(RL_NORELAY_FOLDER_TAG) == std::string::npos))
				{				// hidden folders => invisible to the list
					if (found_one)
					{
						res += ",";
					}
					res += name.c_str();
					if (with_worn_info)
					{
						std::string worn_items = getWornItems(cat);
						res += "|";
						found_one = true;
						if (worn_items == "n")
						{
							res += "10";
						}
						else if (worn_items == "N")
						{
							res += "30";
						}
						else
						{
							res += worn_items;
						}
					}
					found_one = true;
				}
			}
		}
	}

	return res;
}

std::string RLInterface::getWornItems(LLInventoryCategory* cat)
{
	// Returns a string of 2 digits according to the proportion of worn items
	// in this folder and its children:
	// First digit is this folder, second digit is children folders
	// 0 : No item contained in the folder
	// 1 : Some items contained but none is worn
	// 2 : Some items contained and some of them are worn
	// 3 : Some items contained and all of them are worn
	S32 res = 0;
	S32 sub_res = 0;
	S32 prev_sub_res = 0;
	S32 nb_items = 0;
	S32 nb_worn = 0;
	bool no_mod = false;
	bool is_rlv_root = getRlvShare() == cat;

	// if cat exists, scan all the items inside it
	if (cat)
	{
		LLInventoryModel::cat_array_t* cats;
		LLInventoryModel::item_array_t* items;

		// Retrieve all the objects contained in this folder
		gInventory.getDirectDescendentsOf(cat->getUUID(), cats, items);
		if (!is_rlv_root && items)	// Do not scan the shared root
		{
			// Scan them one by one
			LLViewerInventoryItem* item;
			for (S32 i = 0, count = items->size(); i < count; ++i)
			{
				item = (LLViewerInventoryItem*)(*items)[i];
				if (item)
				{
					if (item->getType() == LLAssetType::AT_OBJECT ||
						item->getType() == LLAssetType::AT_CLOTHING ||
						item->getType() == LLAssetType::AT_BODYPART)
					{
						++nb_items;
					}
					if (gAgentWearables.isWearingItem(item->getUUID()) ||
						(isAgentAvatarValid() &&
						 gAgentAvatarp->isWearingAttachment(item->getUUID())))
					{
						++nb_worn;
					}

					// Special case: this item is no-mod, hence we need to check
					// its parent folder is correctly named, and that the item
					// is alone in its folder. If so, then the calling method
					// will have to deal with a special character instead of a
					// number
					if (count == 1 && item->getType() == LLAssetType::AT_OBJECT &&
						!item->getPermissions().allowModifyBy(gAgentID))
					{
						if (findAttachmentPointFromName(cat->getName()))
						{
							no_mod = true;
						}
					}
				}
			}
		}

		// Scan every subfolder of the folder we are scanning, recursively.
		// Note: in the case of no-mod items we should not have sub-folders,
		// so there is no need to check.
		if (cats && !no_mod)
		{
			for (S32 i = 0, count = cats->size(); i < count; ++i)
			{
				LLViewerInventoryCategory* childp =
					(LLViewerInventoryCategory*)(*cats)[i];
				if (childp)
				{
					std::string tmp = getWornItems(childp);
					// Translate the result for no-mod items into something the
					// upper levels can understand
					if (tmp == "N")
					{
						if (!is_rlv_root)
						{
							++nb_worn;
							++nb_items;
							sub_res = 3;
						}
					}
					else if (tmp == "n")
					{
						if (!is_rlv_root)
						{
							++nb_items;
							sub_res = 1;
						}
					}
					else if (!childp->getName().empty() &&
							 // We do not want to include invisible folders,
							 // except the ones containing a no-mod item
							 childp->getName()[0] != '.')
					{
						// This is an actual sub-folder with several items and
						// sub-folders inside, so retain its score to include
						// it into the current one. As it is a sub-folder, to
						// include it we need to reduce its score first
						// (consider "0" as "ignore")
						// "00" = 0, "01" = 1, "10" = 1, "30" = 3, "03" = 3,
						// "33" = 3, all the rest gives 2 (some worn, some not
						// worn)
						if (tmp == "00")
						{
							sub_res = 0;
						}
						else if (tmp == "11" || tmp == "01" || tmp == "10")
						{
							sub_res = 1;
						}
						else if (tmp == "33" || tmp == "03" || tmp == "30")
						{
							sub_res = 3;
						}
						else
						{
							sub_res = 2;
						}

						// Then we must combine with the previous sibling
						// sub-folders./ Same rule as above, set to 2 in all
						// cases except when prev_sub_res == sub_res or when
						// either == 0 (nothing present, ignore)
						if (prev_sub_res == 0 && sub_res == 0)
						{
							sub_res = 0;
						}
						else if (prev_sub_res == 0 && sub_res == 1)
						{
							sub_res = 1;
						}
						else if (prev_sub_res == 1 && sub_res == 0)
						{
							sub_res = 1;
						}
						else if (prev_sub_res == 1 && sub_res == 1)
						{
							sub_res = 1;
						}
						else if (prev_sub_res == 0 && sub_res == 3)
						{
							sub_res = 3;
						}
						else if (prev_sub_res == 3 && sub_res == 0)
						{
							sub_res = 3;
						}
						else if (prev_sub_res == 3 && sub_res == 3)
						{
							sub_res = 3;
						}
						else
						{
							sub_res = 2;
						}
						prev_sub_res = sub_res;
					}
				}
			}
		}
	}

	if (no_mod)
	{
		// The folder contains one no-mod object and is named from an
		// attachment point => return a special character that will be handled
		// by the calling method
		if (nb_worn > 0)
		{
			return "N";
		}
		else
		{
			return "n";
		}
	}
	else
	{
		if (is_rlv_root || nb_items == 0)
		{
			// Forcibly hide all items contained directly under #RLV
			res = 0;
		}
		else if (nb_worn >= nb_items)
		{
			res = 3;
		}
		else if (nb_worn > 0)
		{
			res = 2;
		}
		else
		{
			res = 1;
		}
	}

	std::stringstream str;
	str << res;
	str << sub_res;
	return str.str();
}

LLInventoryCategory* RLInterface::getRlvShare()
{
	LLInventoryModel::cat_array_t* cats;
	LLInventoryModel::item_array_t* items;
	gInventory.getDirectDescendentsOf(gInventory.getRootFolderID(), cats,
									  items);
	if (cats)
	{
		for (S32 i = 0, count = cats->size(); i < count; ++i)
		{
			LLInventoryCategory* cat = (*cats)[i];
			const std::string& name = cat->getName();
			if (name == RL_SHARED_FOLDER)
			{
//              LL_DEBUGS("RestrainedLove") << "found " << name << LL_ENDL;
				return cat;
			}
		}
	}
	return NULL;
}

bool RLInterface::isUnderRlvShare(LLInventoryItem* item)
{
	if (!item) return false;
	const LLUUID& cat_id = item->getParentUUID();
	return isUnderFolder(getRlvShare(), gInventory.getCategory(cat_id));
}

bool RLInterface::isUnderRlvShare(LLInventoryCategory* cat)
{
	return isUnderFolder(getRlvShare(), cat);
}

bool RLInterface::isUnderFolder(LLInventoryCategory* parentp,
								LLInventoryCategory* childp)
{
	if (!parentp || !childp)
	{
		return false;
	}
	if (childp == parentp)
	{
		return true;
	}

	const LLUUID& root_id = gInventory.getRootFolderID();

	const LLUUID& cat_id = childp->getParentUUID();
	LLInventoryCategory* res = gInventory.getCategory(cat_id);

	while (res && res->getUUID() != root_id)
	{
		if (res == parentp)
		{
			return true;
		}
		const LLUUID& parent_id = res->getParentUUID();
		res = gInventory.getCategory(parent_id);
	}
	return false;
}

LLInventoryCategory* RLInterface::getCategoryUnderRlvShare(std::string cat_name,
														   LLInventoryCategory* root)
{
	if (!root)
	{
		root = getRlvShare();
		if (!root)
		{
			LL_DEBUGS("RestrainedLove") << "No " << RL_SHARED_FOLDER
										<< " folder !" << LL_ENDL;
			return NULL;
		}
	}
	if (cat_name.empty())
	{
		return root;
	}

	LLStringUtil::toLower(cat_name);
	std::deque<std::string> tokens = parse(cat_name, "/");

	// Preliminary action: remove everything after pipes("|"), including pipes
	// themselves. This way we can feed the result of a @getinvworn command
	// directly into this method without having to clean up what is after the
	// pipes.
	for (S32 i = 0, count = tokens.size(); i < count; ++i)
	{
		std::string tok = tokens[i];
		size_t ind = tok.find('|');
		if (ind != std::string::npos)
		{
			tok = tok.substr(0, ind);
			tokens[i] = tok;
		}
	}

	LLInventoryModel::cat_array_t* cats;
	LLInventoryModel::item_array_t* items;
	gInventory.getDirectDescendentsOf(root->getUUID(), cats, items);
	if (!cats)
	{
		LL_DEBUGS("RestrainedLove") << "No sub-folder in "
									<< RL_SHARED_FOLDER << LL_ENDL;
		return NULL;
	}

	// We first need to scan the folder tree and retain the best match
	bool exact_match = false;
	S32 max_size_index = -1;
	S32 max_size = 0;
	for (S32 i = 0, count = cats->size(); i < count; ++i)
	{
		LLInventoryCategory* cat = (*cats)[i];
		std::string name = cat->getName();
		if (!name.empty() && name[0] != '.') // Ignore invisible cats
		{
			LLStringUtil::toLower(name);
			S32 size = match(tokens, name, exact_match);
			if (size > max_size || (exact_match && size == max_size))
			{
				max_size = size;
				max_size_index = i;
			}
		}
	}

	if (max_size <= 0)
	{
		LL_DEBUGS("RestrainedLove") << "No matching category name found for "
									<< cat_name << LL_ENDL;
		return NULL;
	}

	// Only now we can grab the best match and either continue deeper or return
	// it
	LLInventoryCategory* cat = (*cats)[max_size_index];
	if (max_size == (S32)tokens.size())
	{
		return cat;
	}

	// Recurse...
	std::string subcat = dumpList2String(getSubList(tokens, max_size), "/");
	return getCategoryUnderRlvShare(subcat, cat);
}

LLInventoryCategory* RLInterface::findCategoryUnderRlvShare(std::string cat_name,
															LLInventoryCategory* root)
{
	if (!root)
	{
		root = getRlvShare();
		if (!root)
		{
			LL_DEBUGS("RestrainedLove") << "No " << RL_SHARED_FOLDER
										<< " folder !" << LL_ENDL;
			return NULL;
		}
	}

	LLStringUtil::toLower(cat_name);
	std::deque<std::string> tokens = parse(cat_name, "&&");

	LLInventoryModel::cat_array_t* cats;
	LLInventoryModel::item_array_t* items;
	gInventory.getDirectDescendentsOf(root->getUUID(), cats, items);
	if (cats)
	{
		LLInventoryCategory* found;
		for (S32 i = 0, count = cats->size(); i < count; ++i)
		{
			LLInventoryCategory* cat = (*cats)[i];
			const std::string& name = cat->getName();
			// We cannot find invisible folders('.') and given folders('~')
			if (!name.empty() && name[0] != '.' && name[0] != '~')
			{
				// Search recursively deeper
				found = findCategoryUnderRlvShare(cat_name, cat);
				if (found)
				{
					return found;
				}
			}
		}
	}

	// Return this category if it matches
	std::string name = root->getName();
	LLStringUtil::toLower(name);
	// We cannot find invisible folders('.') and given folders('~')
	if (!name.empty() && name[0] != '.' && name[0] != '~' &&
		findMultiple(tokens, name))
	{
		return root;
	}

	return NULL;	// We did not find anything
}

std::deque<LLInventoryCategory*> RLInterface::findCategoriesUnderRlvShare(std::string cat_name,
																		  LLInventoryCategory* root)
{
	std::deque<LLInventoryCategory*> res;
	if (!root)
	{
		root = getRlvShare();
		if (!root)
		{
			LL_DEBUGS("RestrainedLove") << "No " << RL_SHARED_FOLDER
										<< " folder !" << LL_ENDL;
			return res;
		}
	}

	LLStringUtil::toLower(cat_name);
	std::deque<std::string> tokens = parse(cat_name, "&&");

	LLInventoryModel::cat_array_t* cats;
	LLInventoryModel::item_array_t* items;
	gInventory.getDirectDescendentsOf(root->getUUID(), cats, items);
	if (cats)
	{
		std::deque<LLInventoryCategory*> found;
		for (S32 i = 0, count = cats->size(); i < count; ++i)
		{
			LLInventoryCategory* cat = (*cats)[i];
			const std::string& name = cat->getName();
			// We cannot find invisible folders('.') and given folders('~')
			if (!name.empty() && name[0] != '.' && name[0] != '~')
			{
				// Search recursively deeper
				found = findCategoriesUnderRlvShare(cat_name, cat);
				for (S32 i = 0, count2 = found.size(); i < count2; ++i)
				{
					res.push_back(found[i]);
				}
			}
		}
	}

	// Return this category if it matches
	std::string name = root->getName();
	LLStringUtil::toLower(name);
	// We cannot find invisible folders('.') and given folders('~')
	if (!name.empty() && name[0] != '.' && name[0] != '~' &&
		findMultiple(tokens, name))
	{
		res.push_back(root);
	}

	return res;
}

bool RLInterface::shouldMoveToSharedSubFolder(LLViewerInventoryCategory* catp)
{
	// Note: we do not test for getRlvShare(), since it is time consuming; the
	// caller should test for it once and for all before doing repetitive calls
	// to this method.
	return catp->getName().compare(0, RL_HRLVST_LENGTH,
								   RL_RLV_REDIR_FOLDER_PREFIX) == 0;
}

void RLInterface::moveToSharedSubFolder(LLViewerInventoryCategory* catp)
{
	LLInventoryCategory* rlv_root_catp = getRlvShare();
	if (!rlv_root_catp)
	{
		return;
	}

	std::string folder_name = catp->getName();
	if (folder_name.compare(0, RL_HRLVST_LENGTH,
							RL_RLV_REDIR_FOLDER_PREFIX) != 0)
	{
		return;
	}
	// Remove #RLV/
	folder_name.erase(0, RL_HRLVS_LENGTH);
	// Sanitize the name
	LLInventoryObject::correctInventoryName(folder_name);

	// By default, we will put this folder under #RLV directly
	LLInventoryCategory* target_catp = rlv_root_catp;

	// We have received a "#RLV/~A/B/C" folder so we want to move it under our
	// #RLV/ root folder.
	// To avoid cluttering the #RLV folder with many sub-folders of the same
	// name, we try to unify the hierarchy like so:
	//  - The last folder in the string must be created even if it already
	//    exists so we do not pollute an existing folder with new items.
	//  - All its parents must be unified with existing folders if possible,
	//    created if not possible.
	std::deque<std::string> hierarchy = parse(folder_name, "/");
	S32 sub_folders = hierarchy.size();

	// For each parent folder in the name from left to right (if any, meaning
	// if there is at least one "/" in the name of the folder we have
	// received), unify or create that folder and make it the parent of the
	// folder on its right.
	for (S32 i = 0; i < sub_folders - 1; ++i)
	{
		const std::string& name = hierarchy[i];
		LLInventoryModel::cat_array_t* cats;
		LLInventoryModel::item_array_t* items;
		gInventory.getDirectDescendentsOf(target_catp->getUUID(), cats, items);
		// Try to find the first folder among the descendents which name
		// matches the one we are examining.
		LLInventoryCategory* found_catp = NULL;
		for (S32 j = 0, count = cats->size(); j < count; ++j)
		{
			LLInventoryCategory* old_catp = (*cats)[j];
			if (!LLStringUtil::compareInsensitive(old_catp->getName(), name))
			{
				// Found an existing folder with that name
				found_catp = old_catp;
				break;
			}
		}
		if (found_catp)
		{
			target_catp = found_catp;
		}
		else
		{
			LLUUID id = gInventory.createCategoryUDP(target_catp->getUUID(),
													 LLFolderType::FT_NONE,
													 name);
			gInventory.notifyObservers();
			if (id.notNull())
			{
				target_catp = gInventory.getCategory(id);
			}
		}
	}

	// Now, move the folder we have received (the one with all the items in
	// it) to our last created (deepest) folder.
	gInventory.changeCategoryParent(catp, target_catp->getUUID(), false);
	gInventory.notifyObservers();
	// And rename it using the last folder name in the path.
	rename_category(&gInventory, catp->getUUID(), hierarchy[sub_folders - 1]);
}

// This struct is meant to be used in RLInterface::findAttachmentPointFromName
// below
typedef struct
{
	LLViewerJointAttachment*	attachment;
	S32							length;
	S32							index;
} Candidate;

LLViewerJointAttachment* RLInterface::findAttachmentPointFromName(std::string obj_name,
																  bool exact_name)
{
	// For each possible attachment point, check whether its name appears in
	// the name of the item.
	// We are going to scan the whole list of attachments, but we would not
	// decide which one to take right away.
	// Instead, for each matching point, we will store in lists the following
	// results:
	// - length of its name
	// - right-most index where it is found in the name
	// - a pointer to that attachment point
	// When we have that list, choose the highest index, and in case of
	// ex-aequo choose the longest length.
	if (obj_name.length() < 3)
	{
		// No need to bother: the shorter attachment name is "Top" with 3
		// characters...
		return NULL;
	}
	if (!isAgentAvatarValid())
	{
		llwarns << "NULL avatar pointer. Aborting." << llendl;
		return NULL;
	}
	LL_DEBUGS("RestrainedLove") << "Searching attachment name with "
								<< (exact_name ? "exact match"
											   : "partial matches") << " in: "
								<< obj_name << LL_ENDL;
	LLStringUtil::toLower(obj_name);
	std::string attach_name;
	bool found_one = false;
	std::vector<Candidate> candidates;

	for (LLVOAvatar::attachment_map_t::iterator
			iter = gAgentAvatarp->mAttachmentPoints.begin(),
			end = gAgentAvatarp->mAttachmentPoints.end();
		 iter != end; ++iter)
	{
		LLViewerJointAttachment* attachment = iter->second;
		if (attachment)
		{
			attach_name = attachment->getName();
			if (attach_name == "Avatar Center")
			{
				attach_name = "Root";
			}
			LLStringUtil::toLower(attach_name);
#if 0
			LL_DEBUGS("RestrainedLove") << "Trying attachment: " << attach_name
										<< LL_ENDL;
#endif
			if (exact_name)
			{
				if (obj_name == attach_name)
				{
					return attachment;
				}
			}
			else
			{
				size_t ind = obj_name.rfind(attach_name);
				if (ind != std::string::npos &&
					obj_name.substr(0, ind).find('(') != std::string::npos &&
					obj_name.substr(ind).find(')') != std::string::npos)
				{
					Candidate new_candidate;
					new_candidate.index = ind;
					new_candidate.length = attach_name.length();
					new_candidate.attachment = attachment;
					candidates.emplace_back(new_candidate);
					found_one = true;
					LL_DEBUGS("RestrainedLove") << "New candidate: '"
												<< attach_name << "', index="
												<< new_candidate.index
												<< ", length="
												<< new_candidate.length
												<< LL_ENDL;
				}
			}
		}
	}
	if (!found_one)
	{
		LL_DEBUGS("RestrainedLove") << "No attachment found." << LL_ENDL;
		return NULL;
	}

	// Now that we have at least one candidate, we have to decide which one to
	// return
	LLViewerJointAttachment* res = NULL;
	Candidate candidate;
	S32 ind_res = -1;
	S32 max_index = -1;
	S32 max_length = -1;
	S32 count = candidates.size();
	// Find the highest index
	for (S32 i = 0; i < count; ++i)
	{
		candidate = candidates[i];
		if (candidate.index > max_index)
		{
			max_index = candidate.index;
		}
	}
	// Find the longest match among the ones found at that index
	for (S32 i = 0; i < count; ++i)
	{
		candidate = candidates[i];
		if (candidate.index == max_index)
		{
			if (candidate.length > max_length)
			{
				max_length = candidate.length;
				ind_res = i;
			}
		}
	}
	// Return this attachment point
	if (ind_res > -1)
	{
		candidate = candidates[ind_res];
		res = candidate.attachment;
		if (res)
		{
			LL_DEBUGS("RestrainedLove") << "Returning: '" << res->getName()
										<< "'" << LL_ENDL;
		}
	}
	return res;
}

LLViewerJointAttachment* RLInterface::findAttachmentPointFromParentName(LLInventoryItem* item)
{
	if (item)
	{
		// Look in parent folder(this could be a no-mod item), use its name to
		// find the target attach point
		const LLUUID& parent_id = item->getParentUUID();
		LLViewerInventoryCategory* cat = gInventory.getCategory(parent_id);
		if (cat)
		{
			return findAttachmentPointFromName(cat->getName());
		}
	}
	return NULL;
}

S32 RLInterface::findAttachmentPointNumber(LLViewerJointAttachment* attachment)
{
	if (isAgentAvatarValid())
	{
		for (LLVOAvatar::attachment_map_t::iterator
				iter = gAgentAvatarp->mAttachmentPoints.begin(),
				end = gAgentAvatarp->mAttachmentPoints.end();
			 iter != end; ++iter)
		{
			if (iter->second == attachment)
			{
				return iter->first;
			}
		}
	}
	return -1;
}

// When an inventory item in #RLV gets attached and does not contain any
// attachment info in its name, rename it for later (truncate the name first if
// needed). Mod-ok items are renamed, else their parent folder, when two level
// deep or more in the tree and named "New Folder", gets renamed, else a new
// folder bearing the joint name is created and the item moved inside it.
// This is called only by LLVOAvatarSelf::attachObject() and must be followed
// with a 'gInventory.notifyObservers()' call as soon as appropriate. HB
void RLInterface::addAttachmentPointName(LLViewerObject* vobj)
{
	static LLCachedControl<bool> rename(gSavedSettings,
										"RestrainedLoveAutomaticRenameItems");
	if (!isAgentAvatarValid() || !rename) return;

	LLViewerInventoryItem* item =
		gInventory.getItem(vobj->getAttachmentItemID());
	if (!item || !item->isFinished() || !isUnderRlvShare(item) ||
		findAttachmentPointFromName(item->getName()))
	{
		// Nothing to do.
		return;
	}

	const LLUUID& item_id = item->getUUID();
	std::string attach_name = gAgentAvatarp->getAttachedPointName(item_id);
	LLStringUtil::toLower(attach_name);

	if (item->getPermissions().allowModifyBy(gAgentID))
	{
		// Truncate the original inventory item name if too long
		size_t max_name_length = DB_INV_ITEM_NAME_STR_LEN - 3 -
								 attach_name.size();
		std::string item_name = item->getName();
		if (item_name.length() >= max_name_length)
		{
			item_name = item_name.substr(0, max_name_length);
		}

		// Add the name of the attach point at the end of the name of the item.
		// Note: this code uses AIS whenever enabled/possible. HB
		LLSD updates;
		updates["name"] = item_name + " (" + attach_name + ")";
		update_inventory_item(item_id, updates);
		return;
	}

	// This is a no-mod item, so we have to rename its parent category instead,
	// provided it is at least 2 levels deep in the #RLV tree, or to move it
	// inside a newly created sub-folder bearing the proper joint name.

	LLInventoryCategory* rlv_share = getRlvShare();

	const LLUUID& parent_id = item->getParentUUID();
	LLViewerInventoryCategory* parentp = gInventory.getCategory(parent_id);
	if (!parentp || (LLInventoryCategory*)parentp == rlv_share)
	{
		// No parent (!) or just under #RLV/: do not rename the #RLV/ folder !
		return;
	}

	// Check to see the folder is already bearing the right attachment name.
	if (findAttachmentPointFromName(parentp->getName()))
	{
		// Yes, so nothing to do...
		return;
	}

	std::string new_name = ".(" + attach_name + ")";

	// Do not rename the folder if it is only 1 level under #RLV/ (i.e. it is
	// an outfit sub-folder) and do not rename it either if the user renamed it
	// themselves, or if another call to this method already renamed it for
	// another no-mod attachment. I.e. only allow to rename a freshly created
	// "New Folder". HB
	LLInventoryCategory* gparentp =
		(LLInventoryCategory*)gInventory.getCategory(parentp->getParentUUID());
	const std::string& default_name =
		LLViewerFolderType::lookupNewCategoryName(LLFolderType::FT_NONE);
	if (gparentp != rlv_share && parentp->getName() == default_name)
	{
		// Rename the category as ".(attachment name)".
		// Note: this code uses AIS whenever enabled/possible. HB
		LLSD updates;
		updates["name"] = new_name;
		update_inventory_category(parent_id, updates, NULL);
	}
	// Else, create a new category with the appropriate name, and move the
	// no-mod item inside it. HB
	else
	{
		LLUUID cat_id = gInventory.createCategoryUDP(parent_id,
													 LLFolderType::FT_NONE,
													 new_name);
		move_inventory_item(item_id, cat_id, item->getName());
	}
}

// Handles the detach message to the sim here, after a check
void RLInterface::detachObject(LLViewerObject* object)
{
	if (object && (!gRLenabled || canDetach(object)))
	{
		LLMessageSystem* msg = gMessageSystemp;
		msg->newMessage("ObjectDetach");
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
		msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
		msg->nextBlockFast(_PREHASH_ObjectData);
		msg->addU32Fast(_PREHASH_ObjectLocalID, object->getLocalID());
		msg->sendReliable(gAgent.getRegionHost());
	}
}

void RLInterface::detachAllObjectsFromAttachment(LLViewerJointAttachment* attachment)
{
	if (!attachment) return;

	// We need to remove all the objects from attachment->mAttachedObjects, one
	// by one. To do this, and in order to avoid any race condition, we are
	// going to copy the list and iterate on the copy instead of the original
	// which changes everytime something is attached and detached, asynchronously.
	LLViewerJointAttachment::attachedobjs_vec_t attached_objects =
		attachment->mAttachedObjects;

	for (S32 i = 0, count = attached_objects.size(); i < count; ++i)
	{
		LLViewerObject* object = attached_objects[i];
		detachObject(object);
	}
}

bool RLInterface::canDetachAllObjectsFromAttachment(LLViewerJointAttachment* attachment)
{
	if (!attachment) return false;

	for (U32 i = 0; i < attachment->mAttachedObjects.size(); ++i)
	{
		LLViewerObject* object = attachment->mAttachedObjects[i];
		if (!canDetach(object))
		{
			return false;
		}
	}

	return true;
}

void RLInterface::fetchInventory(LLInventoryCategory* root)
{
	// do this only once on login

	if (mInventoryFetched) return;

	bool last_step = false;

	if (!root)
	{
		root = getRlvShare();
		last_step = true;
	}

	if (root)
	{
		LLViewerInventoryCategory* viewer_root = (LLViewerInventoryCategory*)root;
		viewer_root->fetch();

		LLInventoryModel::cat_array_t* cats;
		LLInventoryModel::item_array_t* items;

		// Retrieve all the shared folders
		gInventory.getDirectDescendentsOf(viewer_root->getUUID(), cats, items);
		if (cats)
		{
			for (S32 i = 0, count = cats->size(); i < count; ++i)
			{
				LLInventoryCategory* cat = (LLInventoryCategory*)(*cats)[i];
				fetchInventory(cat);
			}
		}
	}

	if (last_step)
	{
		mInventoryFetched = true;
	}
}

// Note: 'recursive' is true in the case of an attachall command
void RLInterface::forceAttach(const std::string& category, bool recursive,
							  EAttachMethod how)
{
	if (category.empty())
	{
		return;
	}

	// Find the category under RLV shared folder
	LLInventoryCategory* cat = getCategoryUnderRlvShare(category);
	if (!cat)
	{
		// No such category. Skip.
		return;
	}

	// We are replacing for now, but the name of the category could decide
	// otherwise
	bool replacing = how == AttachReplace || how == AttachOverOrReplace;
	// If the name of the category begins with a "+", then we force to stack
	// instead of replacing
	if (how == AttachOverOrReplace)
	{
		const std::string& name = cat->getName();
		if (!name.empty() && name[0] == '+')
		{
			replacing = false;
		}
	}

	// Retrieve all the objects contained in this folder
	LLInventoryModel::cat_array_t* cats;
	LLInventoryModel::item_array_t* items;
	gInventory.getDirectDescendentsOf(cat->getUUID(), cats, items);

	bool is_rlv_root = getRlvShare() == cat;
	if (!is_rlv_root && items)
	{
		// Wear them one by one
		for (S32 i = 0, count = items->size(); i < count; ++i)
		{
			LLViewerInventoryItem* item = (LLViewerInventoryItem*)(*items)[i];
			LL_DEBUGS("RestrainedLove") << "Trying to attach "
										<< item->getName() << LL_ENDL;

			// This is an object to attach somewhere
			if (item && item->getType() == LLAssetType::AT_OBJECT)
			{
				LLViewerJointAttachment* attachpt =
					findAttachmentPointFromName(item->getName());
				if (attachpt)
				{
					LL_DEBUGS("RestrainedLove") << "Attaching item to "
												<< attachpt->getName()
												<< LL_ENDL;
					if (replacing)
					{
						// We are replacing => mimick rezAttachment without
						// confirmation dialog
						S32 number = findAttachmentPointNumber(attachpt);
						if (canDetach(attachpt->getName()) && canAttach(item))
						{
							attachObjectByUUID(item->getLinkedUUID(), number,
											   true);
						}
					}
					else
					{
						// We are stacking => call rezAttachment directly
						gAppearanceMgr.rezAttachment(item, attachpt, false);
					}
				}
				else
				{
					// Attachment point is not in the name => stack
					gAppearanceMgr.rezAttachment(item, attachpt, false);
				}
			}
			// This is a piece of clothing
			else if (item->getType() == LLAssetType::AT_CLOTHING ||
					 item->getType() == LLAssetType::AT_BODYPART)
			{
				gAppearanceMgr.wearInventoryItemOnAvatar(item, replacing);
			}
			// This is a gesture: activate
			else if (item->getType() == LLAssetType::AT_GESTURE)
			{
				if (!gGestureManager.isGestureActive(item->getLinkedUUID()))
				{
					gGestureManager.activateGesture(item->getLinkedUUID());
				}
			}
			// This is an environment setting: activate
			else if (item->getType() == LLAssetType::AT_SETTINGS)
			{
				if (!mContainsSetenv && !sRLNoSetEnv)
				{
					gEnvironment.setEnvironment(LLEnvironment::ENV_LOCAL,
												item->getAssetUUID());
					gEnvironment.setSelectedEnvironment(LLEnvironment::ENV_LOCAL,
														LLEnvironment::TRANSITION_INSTANT);
				}
			}
		}
	}

	if (!cats)
	{
		// No sub-folder, we are done !
		return;
	}

	// Not taken into account, we (possibly) recurse instead:
	LLInventoryModel::cat_array_t* subcats;
	// Actual no-mod item(s):
	LLInventoryModel::item_array_t* subcatitems;

	// Scan every sub-folder of the folder we are attaching, in order to attach
	// no-mod items. For each subfolder, attach the first item it contains
	// according to its name
	for (S32 i = 0, count = cats->size(); i < count; ++i)
	{
		LLViewerInventoryCategory* childp =
			(LLViewerInventoryCategory*)(*cats)[i];
		LLViewerJointAttachment* attachpt =
			findAttachmentPointFromName(childp->getName());
		if (!is_rlv_root && attachpt)
		{
			// This subfolder is properly named => attach the first item it
			// contains.
			gInventory.getDirectDescendentsOf(childp->getUUID(), subcats,
											  subcatitems);

			if (subcatitems && subcatitems->size() == 1)
			{
				LLViewerInventoryItem* subcatitem =
					(LLViewerInventoryItem*)(*subcatitems)[0];
				if (subcatitem &&
					subcatitem->getType() == LLAssetType::AT_OBJECT &&
					!subcatitem->getPermissions().allowModifyBy(gAgentID) &&
					findAttachmentPointFromParentName(subcatitem))
				{
					// It is no-mod and its parent is named correctly: we use
					// the attach point from the name of the folder, not the
					// no-mod item
					if (replacing)
					{
						// Mimick rezAttachment without a confirmation dialog
						S32 number = findAttachmentPointNumber(attachpt);
						if (canDetach(attachpt->getName()) &&
							canAttach(subcatitem))
						{
							attachObjectByUUID(subcatitem->getLinkedUUID(),
											   number, true);
						}
					}
					else
					{
						// We are stacking => call rezAttachment directly
						gAppearanceMgr.rezAttachment(subcatitem, attachpt,
													 false);
					}
				}
			}
		}

		if (recursive)
		{
			const std::string& name = childp->getName();
			if (name.empty() || name[0] != '.')
			{
				// attachall and not invisible
				forceAttach(getFullPath(childp), recursive, how);
			}
		}
	}
}

bool RLInterface::forceDetachByName(const std::string& category,
									bool recursive)
{
	if (!isAgentAvatarValid())
	{
		return false;
	}

	if (category.empty())
	{
		return true;	// Nothing to do = success
	}

	// Find the category under RLV shared folder
	LLInventoryCategory* cat = getCategoryUnderRlvShare(category);
	if (!cat)
	{
		return true;	// Nothing to do = success
	}


	bool is_rlv_root = getRlvShare() == cat;

	if (mHandleNoStrip)
	{
		std::string name = cat->getName();
		LLStringUtil::toLower(name);
		if (name.find(RL_PROTECTED_FOLDER_TAG) != std::string::npos)
		{
			return false;	// Protected folder !
		}
	}

	// Retrieve all the objects contained in this folder
	LLInventoryModel::cat_array_t* cats;
	LLInventoryModel::item_array_t* items;
	gInventory.getDirectDescendentsOf(cat->getUUID(), cats, items);

	if (!is_rlv_root && items)
	{
		// Un-wear them one by one
		for (S32 i = 0, count = items->size(); i < count; ++i)
		{
			LLViewerInventoryItem* item = (LLViewerInventoryItem*)(*items)[i];
			LL_DEBUGS("RestrainedLove") << "Trying to detach "
										<< item->getName() << LL_ENDL;

			if (item->getType() == LLAssetType::AT_OBJECT)
			{
				// This is an attached object, find the attachpoint from which
				// to detach
				for (LLVOAvatar::attachment_map_t::iterator
						iter = gAgentAvatarp->mAttachmentPoints.begin(),
						end = gAgentAvatarp->mAttachmentPoints.end();
					 iter != end; ++iter)
				{
					LLViewerJointAttachment* attachment = iter->second;
					LLViewerObject* object =
						gAgentAvatarp->getWornAttachment(item->getUUID());
					if (object && attachment &&
						attachment->isObjectAttached(object))
					{
						detachObject(object);
						break;
					}
				}
			}
			else if (item->getType() == LLAssetType::AT_CLOTHING)
			{
				// This is a piece of clothing: remove
				if (canDetach(item))
				{
					removeWearableItemFromAvatar(item);
				}
			}
			else if (item->getType() == LLAssetType::AT_GESTURE)
			{
				// This is a gesture: deactivate
				if (gGestureManager.isGestureActive(item->getLinkedUUID()))
				{
					gGestureManager.deactivateGesture(item->getLinkedUUID());
				}
			}
#if 0		// Do nothing because we do not know what to replace it with... HB
			// This is an environment setting: deactivate
			else if (item->getType() == LLAssetType::AT_SETTINGS)
			{
				if (!mContainsSetenv && !sRLNoSetEnv)
				{
					gEnvironment.clearEnvironment(LLEnvironment::ENV_LOCAL);
					gEnvironment.setSelectedEnvironment(LLEnvironment::ENV_LOCAL,
														LLEnvironment::TRANSITION_INSTANT);
				}
			}
#endif
		}
	}

	if (!cats)
	{
		// No sub-folder, we are done !
		return true;
	}

	// Not taken into account, we (possibly) recurse instead:
	LLInventoryModel::cat_array_t* subcats;
	// Actual no-mod item(s):
	LLInventoryModel::item_array_t* subcatitems;

	// For each subfolder, detach the first item it contains (only for single
	// no-mod items contained in appropriately named folders)
	for (S32 i = 0, count = cats->size(); i < count; ++i)
	{
		LLViewerInventoryCategory* childp =
			(LLViewerInventoryCategory*)(*cats)[i];
		if (mHandleNoStrip)
		{
			std::string name = childp->getName();
			LLStringUtil::toLower(name);
			if (name.find(RL_PROTECTED_FOLDER_TAG) != std::string::npos)
			{
				continue;	// Protected folder...
			}
		}

		gInventory.getDirectDescendentsOf(childp->getUUID(), subcats,
										  subcatitems);
		if (!is_rlv_root && subcatitems && subcatitems->size() == 1)
		{
			// Only one item...
			LLViewerInventoryItem* subcatitem =
				(LLViewerInventoryItem*)(*subcatitems)[0];

			if (subcatitem &&
				subcatitem->getType() == LLAssetType::AT_OBJECT &&
				!subcatitem->getPermissions().allowModifyBy(gAgentID) &&
				findAttachmentPointFromParentName(subcatitem))
			{
				// ... and it is no-mod and its parent is named correctly
				// detach this object. Find the attachpoint from which to
				// detach.
				for (LLVOAvatar::attachment_map_t::iterator
						iter = gAgentAvatarp->mAttachmentPoints.begin(),
						end = gAgentAvatarp->mAttachmentPoints.end();
					 iter != end; ++iter)
				{
					LLViewerJointAttachment* attachment = iter->second;
					LLViewerObject* object =
						gAgentAvatarp->getWornAttachment(subcatitem->getUUID());
					if (object && attachment &&
						attachment->isObjectAttached(object))
					{
						detachObject(object);
						break;
					}
				}
			}
		}

		if (recursive)
		{
			const std::string& name = childp->getName();
			if (name.empty() || name[0] != '.')
			{
				// detachall and not invisible
				forceDetachByName(getFullPath(childp), recursive);
			}
		}
	}

	return true;
}

#if LL_CLANG
// Ignore the '°' encoding in separators below
# pragma clang diagnostic ignored "-Winvalid-source-encoding"
#endif

std::string RLInterface::stringReplace(std::string s, std::string what_str,
									   const std::string& by_str,
									   bool case_sensitive)
{
	if (what_str.empty() || what_str == " ")
	{
		return s;				// Avoid an infinite loop
	}

	size_t len_by_str = by_str.length();
	if (len_by_str == 0)
	{
		len_by_str = 1;			// Avoid an infinite loop
	}

	size_t len_what_str = what_str.length();

	size_t ind;
	while ((ind = s.find("%20")) != std::string::npos)	// Unescape
	{
		s = s.replace(ind, 3, " ");
	}

	std::string lower = s;
	if (!case_sensitive)
	{
		LLStringUtil::toLower(lower);
		LLStringUtil::toLower(what_str);
	}

	static std::string separators = " .,:;!?'\"_()[]{}*/+-=°~&|@#%$`<>\\\t\n";
	size_t len_s = s.length();
	size_t old_ind = 0;
	while ((ind = lower.find(what_str, old_ind)) != std::string::npos)
	{
		char prec = ' ';
		if (ind > 0)
		{
			prec = s[ind - 1];
		}
		char succ = ' ';
		if (ind < len_s - len_what_str - 1)
		{
			succ = s[ind + len_what_str];
		}
		if (separators.find(prec) != std::string::npos && 
			separators.find(succ) != std::string::npos)
		{
			s = s.replace(ind, len_what_str, by_str);
			lower = s;
			if (!case_sensitive)
			{
				LLStringUtil::toLower(lower);
			}
		}
		old_ind = ind + len_by_str;
	}

	return s;
}

std::string RLInterface::getDummyName(std::string name,
									  EChatAudible audible)
{
	std::string res;
	size_t len = name.length();
	if (len == 0) return res;

	// We use mLaunchTimestamp in order to modify the scrambling when the
	// session restarts (it stays consistent during the session though). But in
	// crashy situations, let's not make it change at EVERY session, more like
	// once a day or so. A day is 86400 seconds, the closest power of two is
	// 65536, that is a 16 bits shift. Very lame hash function I know... but it
	// should be linear enough (the old length method was way too gaussian with
	// a peak at 11 to 16 characters)
	unsigned char hash = name[0] + name[len - 1] + len +
						 (mLaunchTimestamp >> 16);

	unsigned char mod = hash % 28;
	switch (mod)
	{
		case 0:
			res = "A resident";
			break;
		case 1:
			res = "This resident";
			break;
		case 2:
			res = "That resident";
			break;
		case 3:
			res = "An individual";
			break;
		case 4:
			res = "This individual";
			break;
		case 5:
			res = "That individual";
			break;
		case 6:
			res = "A person";
			break;
		case 7:
			res = "This person";
			break;
		case 8:
			res = "That person";
			break;
		case 9:
			res = "A stranger";
			break;
		case 10:
			res = "This stranger";
			break;
		case 11:
			res = "That stranger";
			break;
		case 12:
			res = "A human being";
			break;
		case 13:
			res = "This human being";
			break;
		case 14:
			res = "That human being";
			break;
		case 15:
			res = "An agent";
			break;
		case 16:
			res = "This agent";
			break;
		case 17:
			res = "That agent";
			break;
		case 18:
			res = "A soul";
			break;
		case 19:
			res = "This soul";
			break;
		case 20:
			res = "That soul";
			break;
		case 21:
			res = "Somebody";
			break;
		case 22:
			res = "Anonymous one";
			break;
		case 23:
			res = "Someone";
			break;
		case 24:
			res = "Mysterious one";
			break;
		case 25:
			res = "An unknown being";
			break;
		case 26:
			res = "Unidentified one";
			break;
		default:
			res = "An unknown person";
	}
	if (audible == CHAT_AUDIBLE_BARELY)
	{
		res += " afar";
	}
	return res;
}

// Hides every occurrence of the name of anybody around (found in cache, so not
// completely accurate neither completely immediate).
std::string RLInterface::getCensoredMessage(std::string str)
{
	uuid_vec_t avatar_ids;
	gWorld.getAvatars(avatar_ids);

	LLUUID avatar_id;
	std::string name, dummy_name;
	LLAvatarName avatar_name;
	for (U32 i = 0, count = avatar_ids.size(); i < count; ++i)
	{
		avatar_id = avatar_ids[i];

		// If listed in exceptions, skip this avatar
		if (mExceptions.count(avatar_id)) continue;

		if (gCacheNamep && gCacheNamep->getFullName(avatar_id, name))
		{
			dummy_name = getDummyName(name);
			str = stringReplace(str, name, dummy_name);	// Legacy name
			size_t j = name.find(" Resident");
			if (j > 0)
			{
				name = name.substr(0, j);
				// legacy name, without " Resident"
				str = stringReplace(str, name, dummy_name);
			}
		}
		if (LLAvatarNameCache::get(avatar_id, &avatar_name))
		{
			if (!avatar_name.mIsDisplayNameDefault)
			{
				name = avatar_name.mDisplayName;
				dummy_name = getDummyName(name);
				str = stringReplace(str, name, dummy_name);	// Display name
			}
		}
	}

	return str;
}

std::string RLInterface::getCensoredLocation(std::string str)
{
	if (gAgent.getRegion())
	{
		// Hide every occurrence of the Parcel name
		str = stringReplace(str, mParcelName, "(Parcel hidden)");
		// Hide every occurrence of the Region name
		str = stringReplace(str, gAgent.getRegion()->getName(),
							"(Region hidden)");
	}
	return str;
}

bool RLInterface::forceEnvironment(std::string command, std::string option)
{
	// Compatibility with RLVa
	LLStringUtil::replaceChar(option, '/', ';');

	// Reset this since we are going to change any loaded preset...
	mLastLoadedPreset.clear();

	// 'command' is "setenv_<something>"
	F32 val = (F32)atof(option.c_str());

	constexpr size_t length = 7;	// Size of "setenv_"
	command = command.substr(length);

	LLSettingsSky::ptr_t skyp;
	if (gEnvironment.hasEnvironment(LLEnvironment::ENV_LOCAL))
	{
		if (gEnvironment.getEnvironmentDay(LLEnvironment::ENV_LOCAL))
		{
			// We have a full day cycle in the local environment: freeze
			// the sky.
			skyp = gEnvironment.getEnvironmentFixedSky(LLEnvironment::ENV_LOCAL)->buildClone();
			gEnvironment.setEnvironment(LLEnvironment::ENV_LOCAL, skyp, 0);
		}
		else
		{
			// Otherwise we can just use the local sky.
			skyp = gEnvironment.getEnvironmentFixedSky(LLEnvironment::ENV_LOCAL);
		}
	}
	else
	{
		// Use a copy of the parcel environment sky instead.
		skyp = gEnvironment.getEnvironmentFixedSky(LLEnvironment::ENV_PARCEL,
												   true)->buildClone();
		gEnvironment.setEnvironment(LLEnvironment::ENV_LOCAL, skyp, 0);
	}
	gEnvironment.setSelectedEnvironment(LLEnvironment::ENV_LOCAL,
										LLEnvironment::TRANSITION_INSTANT);

	if (command == "daytime")
	{
		if (val > 1.f)
		{
			val = 1.f;
		}
		if (val >= 0.f)
		{
			gEnvironment.setFixedTimeOfDay(val);
		}
		else
		{
			gSavedSettings.setBool("UseParcelEnvironment", true);
		}
	}
	else if (command == "reset")	// Synonym for "daytime:-1"
	{
		gEnvironment.clearEnvironment(LLEnvironment::ENV_LOCAL);
		gEnvironment.setSelectedEnvironment(LLEnvironment::ENV_LOCAL,
											LLEnvironment::TRANSITION_INSTANT);
	}

	else if (command == "bluehorizonr")
	{
		LLColor3 bluehorizon = skyp->getBlueHorizon();
		bluehorizon.mV[0] = val * 2.f;
		skyp->setBlueHorizon(bluehorizon);
		skyp->update();
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
	}
	else if (command == "bluehorizong")
	{
		LLColor3 bluehorizon = skyp->getBlueHorizon();
		bluehorizon.mV[1] = val * 2.f;
		skyp->setBlueHorizon(bluehorizon);
		skyp->update();
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
	}
	else if (command == "bluehorizonb")
	{
		LLColor3 bluehorizon = skyp->getBlueHorizon();
		bluehorizon.mV[2] = val * 2.f;
		skyp->setBlueHorizon(bluehorizon);
		skyp->update();
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
	}
	else if (command == "bluehorizoni")
	{
		LLColor3 bluehorizon = skyp->getBlueHorizon();
		F32 old_intensity = llmax(bluehorizon.mV[0], bluehorizon.mV[1],
								  bluehorizon.mV[2]);
		if (val == 0.f || old_intensity == 0.f)
		{
			bluehorizon.mV[0] = bluehorizon.mV[1] =
								bluehorizon.mV[2] = val * 2.f;
		}
		else
		{
			F32 factor = val * 2.f / old_intensity;
			bluehorizon.mV[0] *= factor;
			bluehorizon.mV[1] *= factor;
			bluehorizon.mV[2] *= factor;
		}
		skyp->setBlueHorizon(bluehorizon);
		skyp->update();
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
	}
	else if (command == "bluehorizon")
	{
		std::deque<std::string> tokens = parse(option, ";");
		F32 r = atof(tokens[0].c_str()) * 2.f;
		F32 g = atof(tokens[1].c_str()) * 2.f;
		F32 b = atof(tokens[2].c_str()) * 2.f;
		LLColor3 bluehorizon = skyp->getBlueHorizon();
		bluehorizon.mV[0] = r;
		bluehorizon.mV[1] = g;
		bluehorizon.mV[2] = b;
		skyp->setBlueHorizon(bluehorizon);
		skyp->update();
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
	}

	else if (command == "bluedensityr")
	{
		LLColor3 bluedensity = skyp->getBlueDensity();
		bluedensity.mV[0] = val * 2.f;
		skyp->setBlueDensity(bluedensity);
		skyp->update();
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
	}
	else if (command == "bluedensityg")
	{
		LLColor3 bluedensity = skyp->getBlueDensity();
		bluedensity.mV[1] = val * 2.f;
		skyp->setBlueDensity(bluedensity);
		skyp->update();
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
	}
	else if (command == "bluedensityb")
	{
		LLColor3 bluedensity = skyp->getBlueDensity();
		bluedensity.mV[2] = val * 2.f;
		skyp->setBlueDensity(bluedensity);
		skyp->update();
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
	}
	else if (command == "bluedensityi")
	{
		LLColor3 bluedensity = skyp->getBlueDensity();
		F32 old_intensity = llmax(bluedensity.mV[0], bluedensity.mV[1],
								  bluedensity.mV[2]);
		if (val == 0.f || old_intensity == 0.f)
		{
			bluedensity.mV[0] = bluedensity.mV[1] =
								bluedensity.mV[2] = val * 2.f;
		}
		else
		{
			F32 factor = val * 2.f / old_intensity;
			bluedensity.mV[0] *= factor;
			bluedensity.mV[1] *= factor;
			bluedensity.mV[2] *= factor;
		}
		skyp->setBlueDensity(bluedensity);
		skyp->update();
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
	}
	else if (command == "bluedensity")
	{
		std::deque<std::string> tokens = parse(option, ";");
		F32 r = atof(tokens[0].c_str()) * 2.f;
		F32 g = atof(tokens[1].c_str()) * 2.f;
		F32 b = atof(tokens[2].c_str()) * 2.f;
		LLColor3 bluedensity = skyp->getBlueDensity();
		bluedensity.mV[0] = r;
		bluedensity.mV[1] = g;
		bluedensity.mV[2] = b;
		skyp->setBlueDensity(bluedensity);
		skyp->update();
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
	}

	else if (command == "hazehorizon")
	{
		skyp->setHazeHorizon(val);
		skyp->update();
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
	}
	else if (command == "hazedensity")
	{
		skyp->setHazeDensity(val * 4.f);
		skyp->update();
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
	}

	else if (command == "densitymultiplier")
	{
		skyp->setDensityMultiplier(val * 0.001f);
		skyp->update();
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
	}

	else if (command == "distancemultiplier")
	{
		skyp->setDistanceMultiplier(val);
		skyp->update();
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
	}

	else if (command == "maxaltitude")
	{
		skyp->setMaxY(val);
		skyp->update();
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
	}

	else if (command == "sunmooncolorr" || command == "sunlightcolorr")
	{
		LLColor3 suncolour= skyp->getSunlightColor();
		suncolour.mV[0] = val * 3.f;
		skyp->setSunlightColor(suncolour);
		skyp->update();
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
	}
	else if (command == "sunmooncolorg" || command == "sunlightcolorg")
	{
		LLColor3 suncolour= skyp->getSunlightColor();
		suncolour.mV[1] = val * 3.f;
		skyp->setSunlightColor(suncolour);
		skyp->update();
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
	}
	else if (command == "sunmooncolorb" || command == "sunlightcolorb")
	{
		LLColor3 suncolour= skyp->getSunlightColor();
		suncolour.mV[2] = val * 3.f;
		skyp->setSunlightColor(suncolour);
		skyp->update();
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
	}
	else if (command == "sunmooncolori" || command == "sunlightcolori")
	{
		LLColor3 suncolour= skyp->getSunlightColor();
		F32 old_intensity = llmax(suncolour.mV[0], suncolour.mV[1],
								  suncolour.mV[2]);
		if (val == 0.f || old_intensity == 0.f)
		{
			suncolour.mV[0] = suncolour.mV[1] = suncolour.mV[2] = val * 3.f;
		}
		else
		{
			F32 factor = val * 3.f / old_intensity;
			suncolour.mV[0] *= factor;
			suncolour.mV[1] *= factor;
			suncolour.mV[2] *= factor;
		}
		skyp->setSunlightColor(suncolour);
		skyp->update();
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
	}
	else if (command == "sunmooncolor" || command == "sunlightcolor")
	{
		std::deque<std::string> tokens = parse(option, ";");
		F32 r = atof(tokens[0].c_str()) * 3.f;
		F32 g = atof(tokens[1].c_str()) * 3.f;
		F32 b = atof(tokens[2].c_str()) * 3.f;
		LLColor3 suncolour = skyp->getSunlightColor();
		suncolour.mV[0] = r;
		suncolour.mV[1] = g;
		suncolour.mV[2] = b;
		skyp->setSunlightColor(suncolour);
		skyp->update();
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
	}

	else if (command == "ambientr")
	{
		LLColor3 ambientcolor = skyp->getAmbientColor();
		ambientcolor.mV[0] = val * 3.f;
		skyp->setAmbientColor(ambientcolor);
		skyp->update();
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
	}
	else if (command == "ambientg")
	{
		LLColor3 ambientcolor = skyp->getAmbientColor();
		ambientcolor.mV[1] = val * 3.f;
		skyp->setAmbientColor(ambientcolor);
		skyp->update();
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
	}
	else if (command == "ambientb")
	{
		LLColor3 ambientcolor = skyp->getAmbientColor();
		ambientcolor.mV[2] = val * 3.f;
		skyp->setAmbientColor(ambientcolor);
		skyp->update();
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
	}
	else if (command == "ambienti")
	{
		LLColor3 ambientcolor = skyp->getAmbientColor();
		F32 old_intensity = llmax(ambientcolor.mV[0], ambientcolor.mV[1],
								  ambientcolor.mV[2]);
		if (val == 0.f || old_intensity == 0.f)
		{
			ambientcolor.mV[0] = ambientcolor.mV[1] = ambientcolor.mV[2] =
								 val * 3.f;
		}
		else
		{
			F32 factor = val * 3.f / old_intensity;
			ambientcolor.mV[0] *= factor;
			ambientcolor.mV[1] *= factor;
			ambientcolor.mV[2] *= factor;
		}
		skyp->setAmbientColor(ambientcolor);
		skyp->update();
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
	}
	else if (command == "ambient")
	{
		std::deque<std::string> tokens = parse(option, ";");
		F32 r = atof(tokens[0].c_str()) * 3.f;
		F32 g = atof(tokens[1].c_str()) * 3.f;
		F32 b = atof(tokens[2].c_str()) * 3.f;
		LLColor3 ambientcolor = skyp->getAmbientColor();
		ambientcolor.mV[0] = r;
		ambientcolor.mV[1] = g;
		ambientcolor.mV[2] = b;
		skyp->setAmbientColor(ambientcolor);
		skyp->update();
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
	}

	else if (command == "sunglowfocus")
	{
		LLColor3 glow = skyp->getGlow();
		glow.mV[2] = val * -5.f;
		skyp->setGlow(glow);
		skyp->update();
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
	}
	else if (command == "sunglowsize")
	{
		LLColor3 glow = skyp->getGlow();
		glow.mV[0] = val * 20.f;
		skyp->setGlow(glow);
		skyp->update();
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
	}

	else if (command == "scenegamma")
	{
		skyp->setGamma(val);
		skyp->update();
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
	}

	else if (command == "sunazim" || command == "sunazimuth")
	{
		val -= F_TWO_PI * floorf((val + F_PI) / F_TWO_PI);
		LLQuaternion orig_quat = skyp->getSunRotation();
		F32 roll, pitch, yaw;
		orig_quat.getEulerAngles(&roll, &pitch, &yaw);
		LLQuaternion rotation_world;
		rotation_world.setEulerAngles(0.f, 0.f, val - yaw);
		rotation_world.normalize();
		LLQuaternion new_quat = orig_quat * rotation_world;
		skyp->setSunRotation(new_quat);
		skyp->update();
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);		
	}
	else if (command == "sunelev" || command == "sunelevation")
	{
		val = -llclamp(val, -F_PI_BY_TWO, F_PI_BY_TWO);
		LLQuaternion orig_quat = skyp->getSunRotation();
		F32 roll, pitch, yaw;
		orig_quat.getEulerAngles(&roll, &pitch, &yaw);
		LLQuaternion pitch_quat;
		pitch_quat.setAngleAxis(val, 0.f, 1.f, 0.f);
		LLQuaternion yaw_quat;
		yaw_quat.setAngleAxis(yaw, 0.f, 0.f, 1.f);
		LLQuaternion new_quat = pitch_quat * yaw_quat;
		skyp->setSunRotation(new_quat);
		skyp->update();
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);		
	}

	else if (command == "moonazim" || command == "moonazimuth")
	{
		val -= F_TWO_PI * floorf((val + F_PI) / F_TWO_PI);
		LLQuaternion orig_quat = skyp->getMoonRotation();
		F32 roll, pitch, yaw;
		orig_quat.getEulerAngles(&roll, &pitch, &yaw);
		LLQuaternion rotation_world;
		rotation_world.setEulerAngles(0.f, 0.f, val - yaw);
		rotation_world.normalize();
		LLQuaternion new_quat = orig_quat * rotation_world;
		skyp->setMoonRotation(new_quat);
		skyp->update();
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);		
	}
	else if (command == "moonelev" || command == "moonelevation")
	{
		val = -llclamp(val, -F_PI_BY_TWO, F_PI_BY_TWO);
		LLQuaternion orig_quat = skyp->getMoonRotation();
		F32 roll, pitch, yaw;
		orig_quat.getEulerAngles(&roll, &pitch, &yaw);
		LLQuaternion pitch_quat;
		pitch_quat.setAngleAxis(val, 0.f, 1.f, 0.f);
		LLQuaternion yaw_quat;
		yaw_quat.setAngleAxis(yaw, 0.f, 0.f, 1.f);
		LLQuaternion new_quat = pitch_quat * yaw_quat;
		skyp->setMoonRotation(new_quat);
		skyp->update();
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);		
	}

#if 0	// *TODO: implement the EE approximation
	else if (command == "sunmoonposition")
	{
	}

	else if (command == "eastangle")
	{
	}
#endif

	else if (command == "starbrightness")
	{
		skyp->setStarBrightness(val);
		skyp->update();
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
	}

	else if (command == "cloudcolorr")
	{
		LLColor3 cloudcolor = skyp->getCloudColor();
		cloudcolor.mV[0] = val;
		skyp->setCloudColor(cloudcolor);
		skyp->update();
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
	}
	else if (command == "cloudcolorg")
	{
		LLColor3 cloudcolor = skyp->getCloudColor();
		cloudcolor.mV[1] = val;
		skyp->setCloudColor(cloudcolor);
		skyp->update();
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
	}
	else if (command == "cloudcolorb")
	{
		LLColor3 cloudcolor = skyp->getCloudColor();
		cloudcolor.mV[2] = val;
		skyp->setCloudColor(cloudcolor);
		skyp->update();
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
	}
	else if (command == "cloudcolori")
	{
		LLColor3 cloudcolor = skyp->getCloudColor();
		F32 old_intensity = llmax(cloudcolor.mV[0], cloudcolor.mV[1],
								  cloudcolor.mV[2]);
		if (val == 0.f || old_intensity == 0.f)
		{
			cloudcolor.mV[0] = cloudcolor.mV[1] = cloudcolor.mV[2] = val;
		}
		else
		{
			F32 factor = val / old_intensity;
			cloudcolor.mV[0] *= factor;
			cloudcolor.mV[1] *= factor;
			cloudcolor.mV[2] *= factor;
		}
		skyp->setCloudColor(cloudcolor);
		skyp->update();
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
	}
	else if (command == "cloudcolor")
	{
		std::deque<std::string> tokens = parse(option, ";");
		F32 r = atof(tokens[0].c_str());
		F32 g = atof(tokens[1].c_str());
		F32 b = atof(tokens[2].c_str());
		LLColor3 cloudcolor = skyp->getCloudColor();
		cloudcolor.mV[0] = r;
		cloudcolor.mV[1] = g;
		cloudcolor.mV[2] = b;
		skyp->setCloudColor(cloudcolor);
		skyp->update();
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
	}

	else if (command == "cloudx" || command == "clouddensityx")
	{
		LLColor3 clouddetail = skyp->getCloudPosDensity1();
		clouddetail.mV[0] = val;
		skyp->setCloudPosDensity1(clouddetail);
		skyp->update();
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
	}
	else if (command == "cloudy" || command == "clouddensityy")
	{
		LLColor3 clouddetail = skyp->getCloudPosDensity1();
		clouddetail.mV[1] = val;
		skyp->setCloudPosDensity1(clouddetail);
		skyp->update();
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
	}
	else if (command == "cloudd" || command == "clouddensityd")
	{
		LLColor3 clouddetail = skyp->getCloudPosDensity1();
		clouddetail.mV[2] = val;
		skyp->setCloudPosDensity1(clouddetail);
		skyp->update();
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
	}
	else if (command == "cloud" || command == "clouddensity")
	{
		std::deque<std::string> tokens = parse(option, ";");
		F32 r = atof(tokens[0].c_str());
		F32 g = atof(tokens[1].c_str());
		F32 b = atof(tokens[2].c_str());
		LLColor3 clouddetail = skyp->getCloudPosDensity1();
		clouddetail.mV[0] = r;
		clouddetail.mV[1] = g;
		clouddetail.mV[2] = b;
		skyp->setCloudPosDensity1(clouddetail);
		skyp->update();
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
	}

	else if (command == "clouddetailx")
	{
		LLColor3 clouddetail = skyp->getCloudPosDensity2();
		clouddetail.mV[0] = val;
		skyp->setCloudPosDensity2(clouddetail);
		skyp->update();
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
	}
	else if (command == "clouddetaily")
	{
		LLColor3 clouddetail = skyp->getCloudPosDensity2();
		clouddetail.mV[1] = val;
		skyp->setCloudPosDensity2(clouddetail);
		skyp->update();
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
	}
	else if (command == "clouddetaild")
	{
		LLColor3 clouddetail = skyp->getCloudPosDensity2();
		clouddetail.mV[2] = val;
		skyp->setCloudPosDensity2(clouddetail);
		skyp->update();
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
	}
	else if (command == "clouddetail")
	{
		std::deque<std::string> tokens = parse(option, ";");
		F32 r = atof(tokens[0].c_str());
		F32 g = atof(tokens[1].c_str());
		F32 b = atof(tokens[2].c_str());
		LLColor3 clouddetail = skyp->getCloudPosDensity2();
		clouddetail.mV[0] = r;
		clouddetail.mV[1] = g;
		clouddetail.mV[2] = b;
		skyp->setCloudPosDensity2(clouddetail);
		skyp->update();
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
	}

	else if (command == "cloudcoverage")
	{
		skyp->setCloudShadow(val);
		skyp->update();
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
	}

	else if (command == "cloudscale")
	{
		skyp->setCloudScale(val);
		skyp->update();
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
	}

	else if (command == "cloudvariance")
	{
		skyp->setCloudVariance(val);
		skyp->update();
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
	}

	else if (command == "cloudscrollx")
	{
		skyp->setCloudScrollRateX(val + 10.f);
		skyp->update();
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
	}
	else if (command == "cloudscrolly")
	{
		skyp->setCloudScrollRateY(val + 10.f);
		skyp->update();
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
	}
	else if (command == "cloudscroll")
	{
		std::deque<std::string> tokens = parse(option, ";");
		F32 x = atof(tokens[0].c_str()) + 10.f;
		F32 y = atof(tokens[1].c_str()) + 10.f;
		skyp->setCloudScrollRateX(x + 10.f);
		skyp->setCloudScrollRateY(y + 10.f);
		skyp->update();
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
	}

	else if (command == "moisturelevel")
	{
		skyp->setSkyMoistureLevel(val);
		skyp->update();
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
	}
	else if (command == "dropletradius")
	{
		skyp->setSkyDropletRadius(val);
		skyp->update();
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
	}
	else if (command == "icelevel")
	{
		skyp->setSkyDropletRadius(val);
		skyp->update();
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
	}

	else if (command == "sunscale")
	{
		skyp->setSunScale(val);
		skyp->update();
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
	}
	else if (command == "moonscale")
	{
		skyp->setMoonScale(val);
		skyp->update();
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
	}
	else if (command == "moonbrightness")
	{
		skyp->setMoonBrightness(val);
		skyp->update();
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
	}

	else if (command == "sunimage" || command == "suntexture")
	{
		LLUUID id;
		id.set(option, false);
		skyp->setSunTextureId(id);
		skyp->update();
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
	}
	else if (command == "moonimage" || command == "moontexture")
	{
		LLUUID id;
		id.set(option, false);
		skyp->setMoonTextureId(id);
		skyp->update();
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
	}
	else if (command == "cloudimage" || command == "cloudtexture")
	{
		LLUUID id;
		id.set(option, false);
		skyp->setCloudNoiseTextureId(id);
		skyp->update();
		gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);
	}

	else if (command == "preset" || command == "asset")
	{
		// This is an extension to RLV's API, allowing to choose what type of
		// setting to search for: e.g. @setenv_preset:sky|blizzard=force will
		// search for "blizzard" in sky settings only. An especially useful
		// case is when a setting name is shared by all types, like "Default",
		// e.g. @setenv_preset:day|default=force will load the default day
		// setting.
		// The pipe ('|') was chosen as a separator, because it is an illegal
		// character for inventory assets names and file names.
		bool skies = true;
		bool days = true;
		bool waters = true;
		size_t i = option.find('|');
		if (i != std::string::npos && i < option.size() - 1)
		{
			std::string category = option.substr(0, i);
			option.erase(0, i);
			skies = category == "sky";
			days = category == "day";
			waters = category == "water";
		}
		// Apply any preset matching the name in 'option' (ignoring case), be
		// it an inventory setting, a Windlight setting, sky, day or water
		// setting (in this order of preferences). When successfully loaded,
		// the preset will be converted to EE settings and Windlight overriding
		// is enabled if it was not in force already.
		if ((skies && LLEnvSettingsSky::applyPresetByName(option, true)) ||
			(days && LLEnvSettingsDay::applyPresetByName(option, true)) ||
			(waters && LLEnvSettingsWater::applyPresetByName(option, true)))
		{
			mLastLoadedPreset = option;
		}
	}

	return true;
}

std::string RLInterface::getEnvironment(std::string command)
{
	F32 res = 0.f;
	constexpr size_t length = 7;	// Size of "getenv_"
	command = command.substr(length);

	const LLSettingsSky::ptr_t& skyp = gEnvironment.getCurrentSky();

	if (command == "daytime")
	{
		static LLCachedControl<bool> estate(gSavedSettings, "UseWLEstateTime");
		if (gSavedSettings.getBool("UseParcelEnvironment"))
		{
			res = -1.f;
		}
		else if (skyp->getIsSunUp())
		{
			res = 1.f;
		}
	}

	else if (command == "bluehorizonr")
	{
		res = skyp->getBlueHorizon().mV[0] * 0.5f;
	}
	else if (command == "bluehorizong")
	{
		res = skyp->getBlueHorizon().mV[1] * 0.5f;
	}
	else if (command == "bluehorizonb")
	{
		res = skyp->getBlueHorizon().mV[2] * 0.5f;
	}
	else if (command == "bluehorizoni")
	{
		LLColor3 bluehorizon = skyp->getBlueHorizon();
		res = llmax(bluehorizon.mV[0], bluehorizon.mV[1], bluehorizon.mV[2]) *
			  0.5f;
	}
	else if (command == "bluehorizon")
	{
		std::stringstream str;
		str << skyp->getBlueHorizon().mV[0] * 0.5f << ";";
		str << skyp->getBlueHorizon().mV[1] * 0.5f << ";";
		str << skyp->getBlueHorizon().mV[2] * 0.5f;
		return str.str();
	}

	else if (command == "bluedensityr")
	{
		res = skyp->getBlueDensity().mV[0] * 0.5f;
	}
	else if (command == "bluedensityg")
	{
		res = skyp->getBlueDensity().mV[1] * 0.5f;
	}
	else if (command == "bluedensityb")
	{
		res = skyp->getBlueDensity().mV[2] * 0.5f;
	}
	else if (command == "bluedensityi")
	{
		LLColor3 bluedensity = skyp->getBlueDensity();
		res = llmax(bluedensity.mV[0], bluedensity.mV[1], bluedensity.mV[2]) *
			  0.5f;
	}
	else if (command == "bluedensity")
	{
		std::stringstream str;
		str << skyp->getBlueDensity().mV[0] * 0.5f << ";";
		str << skyp->getBlueDensity().mV[1] * 0.5f << ";";
		str << skyp->getBlueDensity().mV[2] * 0.5f;
		return str.str();
	}

	else if (command == "hazehorizon")
	{
		res = skyp->getHazeHorizon();
	}
	else if (command == "hazedensity")
	{
		res = skyp->getHazeDensity() * 0.25f;
	}

	else if (command == "densitymultiplier")
	{
		res = skyp->getDensityMultiplier() * 1000.f;
	}
	else if (command == "distancemultiplier")
	{
		res = skyp->getDistanceMultiplier();
	}
	else if (command == "maxaltitude")
	{
		res = skyp->getMaxY();
	}

	else if (command == "sunmooncolorr")
	{
		res = skyp->getSunlightColor().mV[0] / 3.f;
	}
	else if (command == "sunmooncolorg")
	{
		res = skyp->getSunlightColor().mV[1] / 3.f;
	}
	else if (command == "sunmooncolorb")
	{
		res = skyp->getSunlightColor().mV[2] / 3.f;
	}
	else if (command == "sunmooncolori")
	{
		LLColor3 sunlightcolor = skyp->getSunlightColor();
		res = llmax(sunlightcolor.mV[0],sunlightcolor.mV[1],
					sunlightcolor.mV[2]) / 3.f;
	}
	else if (command == "sunmooncolor")
	{
		std::stringstream str;
		str << skyp->getSunlightColor().mV[0] / 3.f << ";";
		str << skyp->getSunlightColor().mV[1] / 3.f << ";";
		str << skyp->getSunlightColor().mV[2] / 3.f;
		return str.str();
	}

	else if (command == "ambientr")
	{
		res = skyp->getAmbientColor().mV[0] / 3.f;
	}
	else if (command == "ambientg")
	{
		res = skyp->getAmbientColor().mV[1] / 3.f;
	}
	else if (command == "ambientb")
	{
		res = skyp->getAmbientColor().mV[2] / 3.f;
	}
	else if (command == "ambienti")
	{
		LLColor3 ambientcolor = skyp->getAmbientColor();
		res = llmax(ambientcolor.mV[0], ambientcolor.mV[1],
					ambientcolor.mV[2]) / 3.f;
	}
	else if (command == "ambient")
	{
		std::stringstream str;
		str << skyp->getAmbientColor().mV[0] / 3.f << ";";
		str << skyp->getAmbientColor().mV[1] / 3.f << ";";
		str << skyp->getAmbientColor().mV[2] / 3.f;
		return str.str();
	}

	else if (command == "sunglowfocus")
	{
		res = -skyp->getGlow().mV[2] / 5.f;
	}
	else if (command == "sunglowsize")
	{
		res = 2.f - skyp->getGlow().mV[0] / 20.f;
	}
	else if (command == "scenegamma")
	{
		res = skyp->getGamma();
	}

	else if (command == "sunazim" || command == "sunazimuth")
	{
		LLQuaternion orig_quat = skyp->getSunRotation();
		F32 roll, pitch;
		orig_quat.getEulerAngles(&roll, &pitch, &res);
	}
	else if (command == "sunelev" || command == "sunelevation")
	{
		LLQuaternion orig_quat = skyp->getSunRotation();
		F32 roll, pitch, yaw;
		orig_quat.getEulerAngles(&roll, &pitch, &yaw);
		LLQuaternion rotation_world;
		rotation_world.setEulerAngles(0.f, 0.f, -yaw);
		rotation_world.normalize();
		LLQuaternion new_quat = orig_quat * rotation_world;
		new_quat.getEulerAngles(&roll, &pitch, &yaw);
		if (roll <= -F_PI_BY_TWO || roll >= F_PI_BY_TWO)
		{
			pitch = -pitch;
		}
		res = -pitch;
	}

	else if (command == "moonazim" || command == "moonazimuth")
	{
		LLQuaternion orig_quat = skyp->getMoonRotation();
		F32 roll, pitch;
		orig_quat.getEulerAngles(&roll, &pitch, &res);
	}
	else if (command == "moonelev" || command == "moonelevation")
	{
		LLQuaternion orig_quat = skyp->getMoonRotation();
		F32 roll, pitch, yaw;
		orig_quat.getEulerAngles(&roll, &pitch, &yaw);
		LLQuaternion rotation_world;
		rotation_world.setEulerAngles(0.f, 0.f, -yaw);
		rotation_world.normalize();
		LLQuaternion new_quat = orig_quat * rotation_world;
		new_quat.getEulerAngles(&roll, &pitch, &yaw);
		if (roll <= -F_PI_BY_TWO || roll >= F_PI_BY_TWO)
		{
			pitch = -pitch;
		}
		res = -pitch;
	}

#if 0	// *TODO: implement the EE approximations
	else if (command == "sunmoonposition")
	{
	}

	else if (command == "eastangle")
	{
	}
#endif

	else if (command == "starbrightness")
	{
		res = skyp->getStarBrightness();
	}

	else if (command == "cloudcolorr")
	{
		res = skyp->getCloudColor().mV[0];
	}
	else if (command == "cloudcolorg")
	{
		res = skyp->getCloudColor().mV[1];
	}
	else if (command == "cloudcolorb")
	{
		res = skyp->getCloudColor().mV[2];
	}
	else if (command == "cloudcolori")
	{
		LLColor3 cloudcolor = skyp->getCloudColor();
		res = llmax(cloudcolor.mV[0],cloudcolor.mV[1], cloudcolor.mV[2]);
	}
	else if (command == "cloudcolor")
	{
		std::stringstream str;
		str << skyp->getCloudColor().mV[0] << ";";
		str << skyp->getCloudColor().mV[1] << ";";
		str << skyp->getCloudColor().mV[2];
		return str.str();
	}

	else if (command == "cloudx")
	{
		res = skyp->getCloudPosDensity1().mV[0];
	}
	else if (command == "cloudy")
	{
		res = skyp->getCloudPosDensity1().mV[1];
	}
	else if (command == "cloudd")
	{
		res = skyp->getCloudPosDensity1().mV[2];
	}
	else if (command == "cloud")
	{
		std::stringstream str;
		str << skyp->getCloudPosDensity1().mV[0] << ";";
		str << skyp->getCloudPosDensity1().mV[1] << ";";
		str << skyp->getCloudPosDensity1().mV[2];
		return str.str();
	}

	else if (command == "clouddetailx")
	{
		res = skyp->getCloudPosDensity2().mV[0];
	}
	else if (command == "clouddetaily")
	{
		res = skyp->getCloudPosDensity2().mV[1];
	}
	else if (command == "clouddetaild")
	{
		res = skyp->getCloudPosDensity2().mV[2];
	}
	else if (command == "clouddetail")
	{
		std::stringstream str;
		str << skyp->getCloudPosDensity2().mV[0] << ";";
		str << skyp->getCloudPosDensity2().mV[1] << ";";
		str << skyp->getCloudPosDensity2().mV[2];
		return str.str();
	}

	else if (command == "cloudcoverage")
	{
		res = skyp->getCloudShadow();
	}
	else if (command == "cloudscale")
	{
		res = skyp->getCloudScale();
	}

	else if (command == "cloudvariance")
	{
		res = skyp->getCloudVariance();
	}

	else if (command == "cloudscrollx")
	{
		res = skyp->getCloudScrollRate().mV[0] - 10.f;
	}
	else if (command == "cloudscrolly")
	{
		res = skyp->getCloudScrollRate().mV[1] - 10.f;
	}
	else if (command == "cloudscroll")
	{
		std::stringstream str;
		str << skyp->getCloudScrollRate().mV[0] - 10.f << ";";
		str << skyp->getCloudScrollRate().mV[1] - 10.f;
		return str.str();
	}

	else if (command == "moisturelevel")
	{
		res = skyp->getSkyMoistureLevel();
	}
	else if (command == "dropletradius")
	{
		res = skyp->getSkyDropletRadius();
	}
	else if (command == "icelevel")
	{
		res = skyp->getSkyIceLevel();
	}

	else if (command == "sunscale")
	{
		res = skyp->getSunScale();
	}
	else if (command == "moonscale")
	{
		res = skyp->getMoonScale();
	}
	else if (command == "moonbrightness")
	{
		res = skyp->getMoonBrightness();
	}

	else if (command == "sunimage" || command == "suntexture")
	{
		return skyp->getSunTextureId().asString();
	}
	else if (command == "moonimage" || command == "moontexture")
	{
		return skyp->getMoonTextureId().asString();
	}
	else if (command == "cloudimage" || command == "cloudtexture")
	{
		return skyp->getCloudNoiseTextureId().asString();
	}

	else if (command == "preset" || command == "asset")
	{
		return mLastLoadedPreset;
	}

	std::stringstream str;
	str << res;
	return str.str();
}

// MK: As some debug settings are critical to the user's experience and others
// are just useless/not used, we are following a whitelist approach: only allow
// certain debug settings to be changed and not all.
bool RLInterface::forceDebugSetting(std::string command, std::string option)
{
	// Command is "setdebug_<something>"
	constexpr size_t length = 9;		// Size of "setdebug_"
	command = command.substr(length);	// Remove "setdebug_"
	LLStringUtil::toLower(command);

	// Find the index of the command in the list of allowed commands, ignoring
	// the case
	S32 ind = -1;
	std::string tmp;
	for (S32 i = 0, count = mAllowedSetDebug.size(); i < count; ++i)
	{
		tmp = mAllowedSetDebug[i];
		LLStringUtil::toLower(tmp);
		if (tmp == command)
		{
			ind = i;
			break;
		}
	}

	if (ind == -1)
	{
		return false;
	}

	tmp = mAllowedSetDebug[ind];
	LLControlVariable* control = gSavedSettings.getControl(tmp.c_str());
	if (!control)
	{
		llwarns << tmp
				<< " is listed among the modifiable settings, but is was not found in the viewer settings !"
				<< llendl;
		return false;
	}
	// Ensure the changed variable will not be saved on log off
	control->setPersist(false);

	switch (control->type())
	{
		case TYPE_U32:
			gSavedSettings.setU32(tmp.c_str(), atoi(option.c_str()));
			break;

		case TYPE_S32:
			gSavedSettings.setS32(tmp.c_str(), atoi(option.c_str()));
			break;

		case TYPE_F32:
			gSavedSettings.setF32(tmp.c_str(), atoi(option.c_str()));
			break;

		case TYPE_BOOLEAN:
			gSavedSettings.setBool(tmp.c_str(), atoi(option.c_str()));
			break;

		case TYPE_STRING:
			gSavedSettings.setString(tmp.c_str(), option);
			break;

		default:
			llwarns << tmp << " type is currently unsuported. Not set."
					<< llendl;
			return false;
	}

	return true;
}

std::string RLInterface::getDebugSetting(std::string command)
{
	// Command is "getdebug_<something>"
	constexpr size_t length = 9;		// Size of "getdebug_"
	command = command.substr(length);	// Remove "getdebug_"
	LLStringUtil::toLower(command);

	// Find the index of the command in the list of allowed commands, ignoring
	// the case
	S32 ind = -1;
	std::string tmp;
	for (S32 i = 0, count = mAllowedGetDebug.size(); i < count; ++i)
	{
		tmp = mAllowedGetDebug[i];
		LLStringUtil::toLower(tmp);
		if (tmp == command)
		{
			ind = i;
			break;
		}
	}

	if (ind == -1)
	{
		return "";
	}

	tmp = mAllowedGetDebug[ind];
	LLControlVariable* control = gSavedSettings.getControl(tmp.c_str());
	if (!control)
	{
		llwarns << tmp
				<< " is listed among the modifiable settings, but is was not found in the viewer settings !"
				<< llendl;
		return "";
	}

	std::stringstream res;
	switch (control->type())
	{
		case TYPE_U32:
			res << gSavedSettings.getU32(tmp.c_str());
			break;

		case TYPE_S32:
			res << gSavedSettings.getS32(tmp.c_str());
			break;

		case TYPE_F32:
			res << gSavedSettings.getF32(tmp.c_str());
			break;

		case TYPE_BOOLEAN:
			res << gSavedSettings.getBool(tmp.c_str());
			break;

		case TYPE_STRING:
			res << gSavedSettings.getString(tmp.c_str());
			break;

		case TYPE_RECT:
			res << gSavedSettings.getRect(tmp.c_str());
			break;

		case TYPE_COL3:
			res << gSavedSettings.getColor3(tmp.c_str());
			break;

		case TYPE_COL4:
			res << gSavedSettings.getColor4(tmp.c_str());
			break;

		case TYPE_COL4U:
			res << gSavedSettings.getColor4U(tmp.c_str());
			break;

		case TYPE_VEC3:
			res << gSavedSettings.getVector3(tmp.c_str());
			break;

		case TYPE_VEC3D:
			res << gSavedSettings.getVector3d(tmp.c_str());
			break;

		default:
			llwarns << tmp << " type is currently unsuported." << llendl;
	}

	return res.str();
}

std::string RLInterface::getFullPath(LLInventoryCategory* cat)
{
	if (!cat) return "";

	LLInventoryCategory* rlv = getRlvShare();
	if (!rlv) return "";

	LLInventoryCategory* res = cat;
	std::deque<std::string> tokens;

	while (res && res != rlv)
	{
		tokens.push_front(res->getName());
		const LLUUID& parent_id = res->getParentUUID();
		res = gInventory.getCategory(parent_id);
	}

	return dumpList2String(tokens, "/");
}

std::string RLInterface::getFullPath(LLInventoryItem* item,
									 const std::string& option, bool full_list)
{
	LL_DEBUGS("RestrainedLove") << "Item: "
								<< (item ? item->getName() : "NULL")
								<< " - Option: " << option << " - full_list = "
								<< full_list << LL_ENDL;

	// Returns the path from the shared root to this object, or to the object
	// worn at the attach point or clothing layer pointed by option if any
	if (!option.empty())
	{
		// An option is specified; we do not want to check the item that issued
		// the command, but something else that is currently worn (object or
		// clothing)
		item = NULL;
		if (LLUUID::validate(option))
		{
			// if option is an UUID, get the path of the viewer object which
			// bears this UUID
			LLUUID id;
			id.set(option, false);
			if (id.notNull())
			{
				// We want the viewer object from the UUID, not the inventory
				// object
				item = getItem(id);
				if (item && isUnderRlvShare(item))
				{
					// We have found the inventory item: add its path to the
					// list.
					// It looks like a recursive call but the recursion level
					// is only 2 for we would not execute this instruction
					// again in the called method since 'option' will be empty.
					std::deque<std::string> res;
					res.emplace_back(getFullPath(item, ""));
					return dumpList2String(res, ",");
				}
			}
			// UUID invalid, item not found, or not shared...
			return "";
		}

		LLWearableType::EType wearable_type;
		wearable_type = getOutfitLayerAsType(option);
		if (wearable_type != LLWearableType::WT_INVALID)
		{
			// this is a clothing layer; replace item with the piece clothing
			std::deque<std::string> res;
			for (U32 i = 0; i < LLAgentWearables::MAX_CLOTHING_LAYERS; ++i)
			{
				const LLUUID& id =
					gAgentWearables.getWearableItemID(wearable_type, i);
				if (id.notNull())
				{
					item = gInventory.getItem(id);
					// Security: we would return the path even if the item was
					// not shared otherwise
					if (item && isUnderRlvShare(item))
					{
						// We have found the inventory item => add its path to
						// the list.
						// It looks like a recursive call but the recursion
						// level is only 2 for we would not execute this
						// instruction again in the called method since
						// 'option' will be empty.
						res.emplace_back(getFullPath(item, ""));
						LL_DEBUGS("RestrainedLove") << "res = "
													<< dumpList2String(res, ", ")
													<< LL_ENDL;
						if (!full_list)
						{
							// old behaviour: we only return the first folder,
							// not a full list
							break;
						}
					}
				}
			}
			return dumpList2String(res, ",");
		}

		// This is not a clothing layer => it has to be an attachment point
		LLViewerJointAttachment* attach_point =
			findAttachmentPointFromName(option, true);
		if (attach_point)
		{
			std::deque<std::string> res;
			for (U32 i = 0; i < attach_point->mAttachedObjects.size(); ++i)
			{
				LLViewerObject* attached_object =
					attach_point->mAttachedObjects[i];
				if (attached_object)
				{
					item = getItemAux(attached_object, getRlvShare());
					if (item && !isUnderRlvShare(item))
					{
						// Otherwise, we would return the path even if the
						// item is not shared...
						item = NULL;
					}
					else
					{
						// We have found the inventory item => add its path
						// to the list.
						// It looks like a recursive call but the recursion
						// level is only 2 for we would not execute this
						// instruction again in the called method since
						// 'option' will be empty.
						res.emplace_back(getFullPath(item, ""));
						LL_DEBUGS("RestrainedLove") << "res="
													<< dumpList2String(res, ", ")
													<< LL_ENDL;
						// Old behaviour: we only return the first folder,
						// not a full list
						if (!full_list) break;
					}
				}
			}
			return dumpList2String(res, ",");
		}
	}

	if (!item || !isUnderRlvShare(item))
	{
		// Otherwise, we would return the path even if the item is not shared
		return "";
	}

	LLUUID parent_id = item->getParentUUID();
	LLInventoryCategory* parent_cat = gInventory.getCategory(parent_id);

	if (item->getType() == LLAssetType::AT_OBJECT &&
		!item->getPermissions().allowModifyBy(gAgentID))
	{
		if (findAttachmentPointFromName(parent_cat->getName()))
		{
			// This item is no-mod and its parent folder contains the name of
			// an attach point => probably we want the full path only to the
			// containing folder of that folder
			parent_id = parent_cat->getParentUUID();
			parent_cat = gInventory.getCategory(parent_id);
			return getFullPath(parent_cat);
		}
	}

	return getFullPath(parent_cat);
}

// Auxiliary function for getItem()
LLInventoryItem* RLInterface::getItemAux(LLViewerObject* attached_object,
										 LLInventoryCategory* root)
{
	if (attached_object && root && isAgentAvatarValid())
	{
		LLInventoryModel::cat_array_t* cats;
		LLInventoryModel::item_array_t* items;
		gInventory.getDirectDescendentsOf(root->getUUID(), cats, items);

		// Try to find the item in the current category
		for (S32 i = 0, count = items->size(); i < count; ++i)
		{
			LLInventoryItem* item = (*items)[i];
			if (item &&
				(item->getType() == LLAssetType::AT_OBJECT ||
				 item->getType() == LLAssetType::AT_CLOTHING) &&
				gAgentAvatarp->getWornAttachment(item->getUUID()) == attached_object)
			{
				// Found the item in the current category
				return item;
			}
		}

		// We did not find it here => browse the children categories
		for (S32 i = 0, count = cats->size(); i < count; ++i)
		{
			LLInventoryCategory* cat = (*cats)[i];
			LLInventoryItem* item = getItemAux(attached_object, cat);
			if (item)
			{
				return item;
			}
		}
	}

	return NULL;
}

// Returns the inventory item corresponding to the viewer object which UUID is
// "worn_object_id", if any
LLInventoryItem* RLInterface::getItem(const LLUUID& worn_object_id)
{
	LLViewerObject* object = gObjectList.findObject(worn_object_id);
	if (object)
	{
		object = object->getRootEdit();
		if (object->isAttachment())
		{
			return gInventory.getItem(object->getAttachmentItemID());
		}
	}

	// This object is not worn => it has nothing to do with any inventory item
	return NULL;
}

// Beware: this method does NOT check that the target attach point is already
// used by a locked item.
void RLInterface::attachObjectByUUID(const LLUUID& asset_id, S32 attach_pt_num,
									 bool kick)
{
	if (!isAgentAvatarValid()) return;
	LLSD payload;
	payload["item_id"] = asset_id;
	if (!kick && gAgentAvatarp->canAttachMoreObjects())
	{
		payload["attachment_point"] = attach_pt_num | ATTACHMENT_ADD;
	}
	else
	{
		payload["attachment_point"] = attach_pt_num;
	}
	gNotifications.forceResponse(LLNotification::Params("ReplaceAttachment").payload(payload),
								 0 /*YES*/);
}

bool RLInterface::canDetachAllSelectedObjects()
{
	for (LLObjectSelection::iterator iter = gSelectMgr.getSelection()->begin(),
									  end = gSelectMgr.getSelection()->end();
		 iter != end; ++iter)
	{
		LLViewerObject* object = (*iter)->getObject();
		if (object && !canDetach(object))
		{
			return false;
		}
	}
	return true;
}

bool RLInterface::isSittingOnAnySelectedObject()
{
	if (!isAgentAvatarValid() || !gAgentAvatarp->mIsSitting)
	{
		return false;
	}

	for (LLObjectSelection::iterator iter = gSelectMgr.getSelection()->begin(),
									  end = gSelectMgr.getSelection()->end();
		 iter != end; ++iter)
	{
		LLViewerObject* object = (*iter)->getObject();
		if (object && object->isAgentSeat())
		{
			return true;
		}
	}
	return false;
}

// Returns false if :
// - at least one object issued a @attachthis:folder restriction
// - at least one item in this folder is to be worn on an
//   @attachthis:attachpt restriction
// - at least one piece of clothing in this folder is to be worn on an
//   @attachthis:layer restriction
// - any parent folder returns false with @attachallthis
bool RLInterface::canAttachCategory(LLInventoryCategory* folder,
									bool with_exceptions)
{
	if (!folder || !isAgentAvatarValid()) return true;
#if RL_ALLOW_ATTACH_DETACH_RECENTLY_RECEIVED_ITEMS
	if (isInventoryFolderNew(folder)) return true;
#endif
	bool shared = isUnderRlvShare(folder);
	if (!shared || !getRlvShare())
	{
		return !contains("unsharedwear");
	}
	else if (contains("sharedwear"))
	{
		return false;
	}

	return canAttachCategoryAux(folder, false, false, with_exceptions);
}

bool RLInterface::canAttachCategoryAux(LLInventoryCategory* folder,
									   bool in_parent, bool in_no_mod,
									   bool with_exceptions)
{
	if (!isAgentAvatarValid()) return true;

	EFolderLock folder_lock = FolderNotLocked;
	if (folder)
	{
		// Check @attachthis:folder in all restrictions
		std::string restriction = "attachthis";
		if (in_parent)
		{
			restriction = "attachallthis";
		}

		folder_lock = isFolderLockedWithoutException(folder, "attach");
		if (folder_lock == FolderLockedNoException)
		{
			return false;
		}

		if (!with_exceptions && folder_lock == FolderLockedWithException)
		{
			return false;
		}

#if 0
		LLInventoryCategory* restricted_cat;
		std::string path_to_check;
		while (it != mSpecialObjectBehaviours.end())
		{
			if (it->second.find(restriction + ":") == 0)
			{
				// Remove ":" as well:
				path_to_check = it->second.substr(restriction.length() + 1);
				restricted_cat = getCategoryUnderRlvShare(path_to_check);
				if (restricted_cat == folder)
				{
					return false;
				}
			}
			++it;
		}
#endif

		LLInventoryModel::cat_array_t* cats;
		LLInventoryModel::item_array_t* items;
		gInventory.getDirectDescendentsOf(folder->getUUID(), cats, items);

		// Try to find the item in the current category
		for (S32 i = 0, count = items->size(); i < count; ++i)
		{
			LLInventoryItem* item = (*items)[i];
			if (item)
			{
				if (item->getType() == LLAssetType::AT_OBJECT)
				{
					LLViewerJointAttachment* attachpt = NULL;
					if (in_no_mod)
					{
						if (count > 1 ||
							item->getPermissions().allowModifyBy(gAgentID))
						{
							return true;
						}
						LLInventoryCategory* parent =
							gInventory.getCategory(folder->getParentUUID());
						attachpt = findAttachmentPointFromName(parent->getName());
					}
					else
					{
						attachpt = findAttachmentPointFromName(item->getName());
					}
					if (attachpt &&
						contains(restriction + ":" + attachpt->getName()))
					{
						return false;
					}
				}
				else if (item->getType() == LLAssetType::AT_CLOTHING ||
						 item->getType() == LLAssetType::AT_BODYPART)
				{
					LLViewerWearable* wearable =
						gAgentWearables.getWearableFromItemID(item->getLinkedUUID());
					if (wearable &&
						contains(restriction + ":" +
								 getOutfitLayerAsString(wearable->getType())))
					{
						return false;
					}
				}
			}
		}

		// Now check all no-mod items => look at the sub-categories and return
		// false if any of them returns false on a call to
		// canAttachCategoryAux()
		for (S32 i = 0, count = cats->size(); i < count; ++i)
		{
			LLInventoryCategory* cat = (*cats)[i];
			if (cat)
			{
				const std::string& name = cat->getName();
				if (!name.empty() && name[0] == '.' &&
					findAttachmentPointFromName(name))
				{
					if (!canAttachCategoryAux(cat, false, true,
											  with_exceptions))
					{
						return false;
					}
				}
			}
		}
	}

	if (folder == getRlvShare()) return true;

	if (!in_no_mod && folder_lock == FolderNotLocked)
	{
		// Check for @attachallthis in the parent
		return canAttachCategoryAux(gInventory.getCategory(folder->getParentUUID()),
									true, false, with_exceptions);
	}

	return true;
}

// Returns false if:
// - at least one object contained in this folder issued a @detachthis
//   restriction
// - at least one object issued a @detachthis:folder restriction
// - at least one worn attachment in this folder is worn on a
//   @detachthis:attachpt restriction
// - at least one worn piece of clothing in this folder is worn on a
//   @detachthis:layer restriction
// - any parent folder returns false with @detachallthis
bool RLInterface::canDetachCategory(LLInventoryCategory* folder,
									bool with_exceptions)
{
	if (!folder || !isAgentAvatarValid()) return true;

	if (mHandleNoStrip)
	{
		std::string name = folder->getName();
		LLStringUtil::toLower(name);
		if (name.find(RL_PROTECTED_FOLDER_TAG) != std::string::npos)
		{
			return false;
		}
	}
#if RL_ALLOW_ATTACH_DETACH_RECENTLY_RECEIVED_ITEMS
	if (isInventoryFolderNew(folder)) return true;
#endif
	bool shared = isUnderRlvShare(folder);
	if (!shared || !getRlvShare())
	{
		return !contains("unsharedunwear");
	}
	else if (contains("sharedunwear"))
	{
		return false;
	}

	return canDetachCategoryAux(folder, false, false, with_exceptions);
}

bool RLInterface::canDetachCategoryAux(LLInventoryCategory* folder,
									   bool in_parent, bool in_no_mod,
									   bool with_exceptions)
{
	if (!isAgentAvatarValid()) return true;

	EFolderLock folder_lock = FolderNotLocked;
	if (folder)
	{
		// check @detachthis:folder in all restrictions
		std::string path_to_check;
		std::string restriction = "detachthis";
		if (in_parent)
		{
			restriction = "detachallthis";
		}

		folder_lock = isFolderLockedWithoutException(folder, "detach");
		if (folder_lock == FolderLockedNoException)
		{
			return false;
		}

		if (!with_exceptions && folder_lock == FolderLockedWithException)
		{
			return false;
		}

#if 0
		LLInventoryCategory* restricted_cat;
		while (it != mSpecialObjectBehaviours.end())
		{
			if (it->second.find(restriction + ":") == 0)
			{
				// remove ":" as well:
				path_to_check = it->second.substr(restriction.length()+1);
				restricted_cat = getCategoryUnderRlvShare(path_to_check);
				if (restricted_cat == folder) return false;
			}
			++it;
		}
#endif

		LLInventoryModel::cat_array_t* cats;
		LLInventoryModel::item_array_t* items;
		gInventory.getDirectDescendentsOf(folder->getUUID(), cats, items);

		// Try to find the item in the current category
		for (S32 i = 0, count = items->size(); i < count; ++i)
		{
			LLInventoryItem* item = (*items)[i];
			if (item)
			{
				if (item->getType() == LLAssetType::AT_OBJECT)
				{
					if (in_no_mod)
					{
						if (count > 1 ||
							item->getPermissions().allowModifyBy(gAgentID))
						{
							return true;
						}
					}
					LLViewerObject* attached_object;
					attached_object = gAgentAvatarp->getWornAttachment(item->getLinkedUUID());
					if (attached_object)
					{
						if (!isAllowed(attached_object->getRootEdit()->getID(),
									   restriction))
						{
							return false;
						}
						if (!in_parent &&
							!isAllowed(attached_object->getRootEdit()->getID(),
									   "detachallthis"))
						{
							// special case for objects contained into this
							// folder and that issued a @detachallthis command
							// without any parameter without issuing a
							// @detachthis command along with it
							return false;
						}
						if (contains(restriction + ":" +
									 gAgentAvatarp->getAttachedPointName(item->getLinkedUUID())))
						{
							return false;
						}
					}
				}
				else if (item->getType() == LLAssetType::AT_CLOTHING ||
						 item->getType() == LLAssetType::AT_BODYPART)
				{
					LLViewerWearable* wearable;
					wearable = gAgentWearables.getWearableFromItemID(item->getLinkedUUID());
					if (wearable &&
						contains(restriction + ":" +
								 getOutfitLayerAsString(wearable->getType())))
					{
						return false;
					}
				}
			}
		}

		// Now check all no-mod items => look at the sub-categories and return
		// false if any of them returns false on a call to
		// canDetachCategoryAux()
		for (S32 i = 0, count = cats->size(); i < count; ++i)
		{
			LLInventoryCategory* cat = (*cats)[i];
			if (cat)
			{
				const std::string& name = cat->getName();
				if (!name.empty() && name[0] == '.' &&
					findAttachmentPointFromName(name))
				{
					if (!canDetachCategoryAux(cat, false, true)) return false;
				}
			}
		}
	}

	if (folder == getRlvShare()) return true;

	if (!in_no_mod && folder_lock == FolderNotLocked)
	{
		// check for @detachallthis in the parent
		return canDetachCategoryAux(gInventory.getCategory(folder->getParentUUID()),
									true, false, with_exceptions);
	}

	return true;
}

bool RLInterface::isRestoringOutfit()
{
	return !gRLenabled || mRestoringOutfit || !isAgentAvatarValid() ||
		   gAgentAvatarp->getIsCloud();
}

bool RLInterface::canUnwear(LLViewerInventoryItem* item)
{
	if (item && !isRestoringOutfit())
	{
		if (item->getType() == LLAssetType::AT_OBJECT)
		{
			return canDetach(item);
		}
		if (item->getType() == LLAssetType::AT_CLOTHING ||
				 item->getType() == LLAssetType::AT_BODYPART)
		{
			if (!canUnwear(item->getWearableType()))
			{
				return false;
			}

			LLInventoryCategory* parent;
			parent = gInventory.getCategory(item->getParentUUID());
			if (!canDetachCategory(parent))
			{
				return false;
			}
		}
	}
	return true;
}

bool RLInterface::canUnwear(LLWearableType::EType type)
{
	if (!isRestoringOutfit())
	{
		if (contains("remoutfit"))
		{
			return false;
		}
		if (contains("remoutfit:" + getOutfitLayerAsString(type)))
		{
			return false;
		}
	}

	return true;
}

bool RLInterface::canWear(LLViewerInventoryItem* item)
{
	if (item && !isRestoringOutfit())
	{
#if RL_ALLOW_ATTACH_DETACH_RECENTLY_RECEIVED_ITEMS
		if (isInventoryItemNew(item)) return true;
#endif
		LLInventoryCategory* parent =
			gInventory.getCategory(item->getParentUUID());
		if (item->getType() == LLAssetType::AT_OBJECT)
		{
			LLViewerJointAttachment* attachpt;
			attachpt = findAttachmentPointFromName(item->getName());
			if (attachpt && !canAttach(NULL, attachpt->getName()))
			{
				return false;
			}
			return canAttachCategory(parent);
		}
		if (item->getType() == LLAssetType::AT_CLOTHING ||
			item->getType() == LLAssetType::AT_BODYPART)
		{
			if (!canWear(item->getWearableType()) ||
				!canAttachCategory(parent))
			{
				return false;
			}
		}
	}

	return true;
}

bool RLInterface::canWear(LLWearableType::EType type)
{
	if (!isRestoringOutfit())
	{
		if (contains("addoutfit"))
		{
			return false;
		}
		if (contains("addoutfit:" + getOutfitLayerAsString(type)))
		{
			return false;
		}
	}

	return true;
}

bool RLInterface::canDetach(LLViewerInventoryItem* item)
{
	if (!item || isRestoringOutfit()) return true;

	if (mHandleNoStrip)
	{
		std::string name = item->getName();
		LLStringUtil::toLower(name);
		if (name.find(RL_PROTECTED_FOLDER_TAG) != std::string::npos)
		{
			return false;
		}
	}

	if (item->getType() == LLAssetType::AT_OBJECT)
	{
		// We will check canDetachCategory() inside this function
		return canDetach(gAgentAvatarp->getWornAttachment(item->getLinkedUUID()));
	}
	else if (item->getType() == LLAssetType::AT_CLOTHING)
	{
		LLInventoryCategory* parentp =
			gInventory.getCategory(item->getParentUUID());
		if (parentp && !canDetachCategory(parentp))
		{
			return false;
		}
		const LLViewerWearable* wearable =
			gAgentWearables.getWearableFromItemID(item->getUUID());
		if (wearable)
		{
			return canUnwear(wearable->getType());
		}
	}

	return true;
}

bool RLInterface::canDetach(LLViewerObject* attached_object)
{
	if (!attached_object || isRestoringOutfit()) return true;

	LLViewerObject* root = attached_object->getRootEdit();
	if (!root) return true;

	// Check all the current restrictions, if "detach" is issued from a child
	// prim of the root prim of attached_object, then the whole object is
	// undetachable
	for (rl_map_it_t it = mSpecialObjectBehaviours.begin(),
					 end = mSpecialObjectBehaviours.end();
		 it != end; ++it)
	{
		if (it->second == "detach")
		{
			LLViewerObject* this_prim =
				gObjectList.findObject(LLUUID(it->first));
			if (this_prim && this_prim->getRootEdit() == root)
			{
				return false;
			}
		}
	}

	const LLUUID& obj_id = attached_object->getID();
	if (!isAllowed(obj_id, "detach", false) ||
		!isAllowed(obj_id, "detachthis", false) ||
		!isAllowed(obj_id, "detachallthis", false))
	{
		return false;
	}

	LLInventoryItem* item = getItem(root->getID());
	if (item)
	{
		if (mHandleNoStrip)
		{
			std::string name = item->getName();
			LLStringUtil::toLower(name);
			if (name.find(RL_PROTECTED_FOLDER_TAG) != std::string::npos)
			{
				return false;
			}
		}
#if RL_ALLOW_ATTACH_DETACH_RECENTLY_RECEIVED_ITEMS
		if (isInventoryItemNew(item)) return true;
#endif
		LLInventoryCategory* parentp =
			gInventory.getCategory(item->getParentUUID());
		if (parentp && !canDetachCategory(parentp)) return false;

		std::string attachpt;
		attachpt = gAgentAvatarp->getAttachedPointName(item->getLinkedUUID());
		if (contains("detach:" + attachpt)) return false;
		if (contains("remattach")) return false;
		if (contains("remattach:" + attachpt)) return false;
	}
	return true;
}

bool RLInterface::canDetach(std::string attachpt)
{
	if (isRestoringOutfit()) return true;

	LLStringUtil::toLower(attachpt);
	if (contains("detach:" + attachpt)) return false;
	if (contains("remattach")) return false;
	if (contains("remattach:" + attachpt)) return false;
	LLViewerJointAttachment* attachment;
	attachment = findAttachmentPointFromName(attachpt, true);
	return canDetachAllObjectsFromAttachment(attachment);
}

// Beware: this function does not check if we are replacing and there is a
// locked object already present on the attachment point
bool RLInterface::canAttach(LLViewerObject* object_to_attach,
							std::string attachpt)
{
	if (isRestoringOutfit())
	{
		return true;
	}

	LLStringUtil::toLower(attachpt);
	if (contains("addattach") || contains("addattach:" + attachpt))
	{
		return false;
	}
	if (object_to_attach)
	{
		LLInventoryItem* item =
			getItem(object_to_attach->getRootEdit()->getID());
		if (item)
		{
#if RL_ALLOW_ATTACH_DETACH_RECENTLY_RECEIVED_ITEMS
			if (isInventoryItemNew(item)) return true;
#endif
			LLInventoryCategory* parentp =
				gInventory.getCategory(item->getParentUUID());
			if (parentp && !canAttachCategory(parentp))
			{
				return false;
			}
		}
	}

	return true;
}

bool RLInterface::canAttach(LLViewerInventoryItem* item)
{
	if (!item || isRestoringOutfit()) return true;

#if RL_ALLOW_ATTACH_DETACH_RECENTLY_RECEIVED_ITEMS
	if (isInventoryItemNew(item)) return true;
#endif
	if (contains("addattach")) return false;

	LLViewerJointAttachment* attachpt;
	attachpt = findAttachmentPointFromName(item->getName());
	if (attachpt && contains("addattach:" + attachpt->getName())) return false;

	LLInventoryCategory* parentp =
		gInventory.getCategory(item->getParentUUID());
	return !parentp || canAttachCategory(parentp);
}

bool RLInterface::canStartIM(const LLUUID& to_id)
{
	std::string id_str = to_id.asString();
	return !contains("startimto:" + id_str) &&
		   !containsWithoutException("startim", id_str);
}

bool RLInterface::canSendIM(const LLUUID& to_id)
{
	std::string id_str = to_id.asString();
	return !contains("sendimto:" + id_str) &&
		   !containsWithoutException("sendim", id_str);
}

bool RLInterface::canReceiveIM(const LLUUID& from_id)
{
	std::string id_str = from_id.asString();
	return !contains("recvimfrom:" + id_str) &&
		   !containsWithoutException("recvim", id_str);
}

bool RLInterface::canSendGroupIM(std::string group_name)
{
	// Remove any separators from the group name
	LLStringUtil::replaceString(group_name, ",", "");
	LLStringUtil::replaceString(group_name, ";", "");
	return !((contains("sendimto:allgroups") &&
		      contains("sendimto:" + group_name)) ||
			 containsWithoutException("sendim", "allgroups") ||
		     containsWithoutException("sendim", group_name));
}

bool RLInterface::canReceiveGroupIM(std::string group_name)
{
	// Remove any separators from the group name
	LLStringUtil::replaceString(group_name, ",", "");
	LLStringUtil::replaceString(group_name, ";", "");
	return !((contains("recvimfrom:allgroups") &&
		      contains("recvimfrom:" + group_name)) ||
			 containsWithoutException("recvim", "allgroups") ||
		     containsWithoutException("recvim", group_name));
}

bool RLInterface::canEdit(LLViewerObject* object)
{
	if (!object) return false;

	LLViewerObject* root = object->getRootEdit();
	if (!root) return false;

	if (!mContainsEdit)
	{
		return true;
	}

	if (containsWithoutException("edit", root->getID().asString()))
	{
		return false;
	}

	bool is_attachment = object->isAttachment();
	if (is_attachment && contains("editworld"))
	{
		return false;
	}
	if (!is_attachment && contains("editattach"))
	{
		return false;
	}

	if (contains("editobj:" + root->getID().asString()))
	{
		return false;
	}

	return !mContainsInteract || object->isHUDAttachment();
}

bool RLInterface::canTouch(LLViewerObject* object,
						   LLVector3 pick_intersection)
{
	if (!object)
	{
		return true;
	}

	LLViewerObject* root = object->getRootEdit();
	if (!root)
	{
		return true;
	}

	// To check the presence of "touchme" on this object, which means that we
	// can touch it
	if (!isAllowed(root->getID(), "touchme"))
	{
		return true;
	}

	bool is_hud = root->isHUDAttachment();
	if (!is_hud && contains("touchall"))
	{
		return false;
	}

#if 0	// Not implemented
	if (!is_hud && contains("touchallnonhud"))
	{
		return false;
	}
#endif

	if (is_hud &&
		containsWithoutException("touchhud",
								 object->getRootEdit()->getID().asString()))
	{
		return false;
	}

	if (contains("touchthis:" + root->getID().asString()))
	{
		return false;
	}

	if (!canTouchFar(object, pick_intersection))
	{
		return false;
	}

	if (root->isAttachment())
	{
		if (!is_hud)
		{
			if (contains("touchattach")) return false;

			LLInventoryItem* inv_item = getItem(root->getID());
			if (inv_item)
			{
				// This attachment is in my inv => it belongs to me
				if (contains("touchattachself"))
				{
					return false;
				}
			}
			else
			{
				// This attachment is not in my inv => it does not belong to me
				if (contains("touchattachother"))
				{
					return false;
				}
				LLVOAvatar* av = root->getAvatar();
				if (!av ||
					contains("touchattachother:" + av->getID().asString()))
				{
					return false;
				}
			}
		}
	}
	else if (containsWithoutException("touchworld", root->getID().asString()))
	{
		return false;
	}

	return true;
}

bool RLInterface::canTouchFar(LLViewerObject* object,
							  LLVector3 pick_intersection)
{
	if (!object || object->isHUDAttachment()) return true;

	if (mContainsInteract) return false;

	LLVector3 pos = object->getPositionRegion();
	if (pick_intersection != LLVector3::zero)
	{
		pos = pick_intersection;
	}
	pos -= gAgent.getPositionAgent();

	F32 dist = pos.length();
#if 0 // Lift this restriction for now, as there may be cases where we want the
	  // avatar to touch something that is beyond their vision range.
	return dist <= mFartouchMax && dist <= mCamDistDrawMax;
#else
	return dist <= mFartouchMax;
#endif
}

#if RL_ALLOW_ATTACH_DETACH_RECENTLY_RECEIVED_ITEMS
bool RLInterface::isInventoryFolderNew(LLInventoryCategory* folder)
{
	return folder && mReceivedInventoryFolders.count(folder->getName()) != 0;
}

bool RLInterface::isInventoryItemNew(LLInventoryItem* item)
{
	if (!item) return false;

	const LLUUID& parent_id = item->getParentUUID();
	LLInventoryCategory* parent = gInventory.getCategory(parent_id);
	return parent && mReceivedInventoryFolders.count(parent->getName()) != 0;
}
#endif

// Updates the min and max values not related to camera and vision restrictions
void RLInterface::updateLimits()
{
	mFartouchMax = llmin(getMin("fartouch", EXTREMUM),
						 getMin("touchfar", EXTREMUM));
	mSittpMax = getMin("sittp", EXTREMUM);
	mTplocalMax = getMin("tplocal", EXTREMUM);
}

// Checks that we are within the imposed limits, forces the camera back into
// the limits and returns false when not, returns true when the camera is ok.
bool RLInterface::checkCameraLimits(bool and_correct)
{
	if (!gAgent.mInitialized)
	{
		return true;
	}

	if (mCamDistMax <= 0.f && !gAgent.cameraMouselook())
	{
		if (and_correct)
		{
			gAgent.changeCameraToMouselook();
		}
		return false;
	}
	else if (mCamDistMin > 0.f && gAgent.cameraMouselook())
	{
		if (and_correct)
		{
			gAgent.changeCameraToDefault();
		}
		return false;
	}

	return true;
}

bool RLInterface::updateCameraLimits()
{
	// Update the min and max
	mShowavsDistMax = getMin("camavdist", EXTREMUM);
	if (mShowavsDistMax < EXTREMUM)
	{
		LLVOAvatar::sUseImpostors = true;
		LLVOAvatar::updateSettings();
	}
	else if (LLStartUp::isLoggedIn())
	{
		LLVOAvatar::updateSettings();
	}

	F32 old_dist_min = mCamDistDrawMin;
	F32 old_dist_max = mCamDistDrawMax;
	F32 old_alpha_min = mCamDistDrawAlphaMin;
	F32 old_alpha_max = mCamDistDrawAlphaMax;

	mCamZoomMax = getMin("camzoommax", EXTREMUM);
	if (mCamZoomMax == 0.f) mCamZoomMax = EXTREMUM;
	mCamZoomMin = getMax("camzoommin", -EXTREMUM);
	if (mCamZoomMin == 0.f) mCamZoomMin = -EXTREMUM;

	// setcam_fovmin and setcam_fovmax set the FOV, i.e. 60°/multiplier;
	// in other words, they are equivalent to camzoommin and camzoommax.
	F32 fovmin = getMax("setcam_fovmin", 0.001f);
	if (fovmin != 0.f && fovmin != 0.001f)
	{
		F32 zoommax_from_fovmin = DEFAULT_FIELD_OF_VIEW / fovmin;
		if (zoommax_from_fovmin < mCamZoomMax)
		{
			mCamZoomMax = zoommax_from_fovmin;
		}
	}
	F32 fovmax = getMin("setcam_fovmax", EXTREMUM);
	if (fovmax != 0.f && fovmax != EXTREMUM)
	{
		F32 zoommin_from_fovmax = DEFAULT_FIELD_OF_VIEW / fovmax;
		if (zoommin_from_fovmax > mCamZoomMin)
		{
			mCamZoomMin = zoommin_from_fovmax;
		}
	}

	mCamDistMax = getMin("camdistmax,setcam_avdistmax", EXTREMUM);
	mCamDistMin = getMax("camdistmin,setcam_avdistmin", -EXTREMUM);

	mCamDistDrawMax = getMin("camdrawmax", EXTREMUM);
	mCamDistDrawMin = getMin("camdrawmin", EXTREMUM);

	mCamDistDrawAlphaMin = getMax("camdrawalphamin", 0.f);
	mCamDistDrawAlphaMax = getMax("camdrawalphamax", 1.f);

	mCamDistDrawColor = getMixedColors("camdrawcolor", LLColor3::black);

	if (mCamDistDrawMin <= 0.4f)
	{
		// So we are sure to render the spheres even when restricted to
		// mouselook
		mCamDistDrawMin = 0.4f;
	}

	if (mCamDistDrawMax < mCamDistDrawMin)
	{
		// Sort the two limits in order
		if (mCamDistDrawMin < EXTREMUM)
		{
			mCamDistDrawMax = mCamDistDrawMin;
		}
		else
		{
			mCamDistDrawMin = mCamDistDrawMax;
		}
	}

	if (mCamDistMax >= mCamDistDrawMin && mCamDistDrawMin < EXTREMUM)
	{
		// Make sure we cannot move the camera outside the minimum render limit
		mCamDistMax = mCamDistDrawMin * 0.75f;
	}
	if (mCamDistMax >= mCamDistDrawMax && mCamDistDrawMax < EXTREMUM)
	{
		// Make sure we cannot move the camera outside the maximum render limit
		mCamDistMax = mCamDistDrawMax * 0.75f;
	}

	if (mCamDistDrawAlphaMax < mCamDistDrawAlphaMin)
	{
		// Make sure the "fog" goes in the right direction
		mCamDistDrawAlphaMax = mCamDistDrawAlphaMin;
	}

	if (mCamZoomMin > mCamZoomMax)
	{
		mCamZoomMin = mCamZoomMax;
	}

	if (mCamDistMin > mCamDistMax)
	{
		mCamDistMin = mCamDistMax;
	}

	if (old_dist_min != mCamDistDrawMin || old_dist_max != mCamDistDrawMax ||
		old_alpha_min != mCamDistDrawAlphaMin ||
		old_alpha_max != mCamDistDrawAlphaMax)
	{
		// Force all the rendering types back to true (and we would not be able
		// to switch them off while the vision is restricted)
		if (mCamDistDrawMin < EXTREMUM || mCamDistDrawMax < EXTREMUM)
		{
			gSavedSettings.setBool("BeaconAlwaysOn", false);
			gPipeline.setAllRenderTypes();
		}

		// Silly hack, but we need to force all textures in world to be updated
		// (code copied from camtextures above)
		for (S32 i = 0, count = gObjectList.getNumObjects(); i < count; ++i)
		{
			LLViewerObject* object = gObjectList.getObject(i);
			if (object)
			{
				object->setSelected(false);
			}
		}
	}

	// Limit the number of gradients to 10 per meter, with 2 as the minimum
	// and 40 as the maximum.
	mCamDistNbGradients =
		llclamp((U32)((mCamDistDrawMax - mCamDistDrawMin) * 10.f), 2U, 40U);

	mVisionRestricted = mCamDistDrawMin < EXTREMUM ||
						mCamDistDrawMax < EXTREMUM;

	// And check the camera is still within the limits
	return checkCameraLimits(true);
}

#define UPPER_ALPHA_LIMIT 0.999999f
// This function returns the effective alpha to set to each step when going
// from 0.0 to "desired_alpha", so that everything seen through the last layer
// will be obscured as if it were behind only one layer of desired_alpha,
// regardless of nb_layers If we have N layers and want a transparency T
// (T = 1-A), we want to find X so that X**N = T (because combined
// transparencies multiply), in other words, X = T**(1/N). The problem with
// this formula is that with a target transparency of 0 (alpha = 1), we would
// not get any gradient at all so we need to limit the alpha to a maximum that
// is lower than 1.
F32 calculateDesiredAlphaPerStep(F32 desired_alpha, S32 nb_layers)
{
	if (desired_alpha > UPPER_ALPHA_LIMIT)
	{
		desired_alpha = UPPER_ALPHA_LIMIT;
	}
	F64 desired_trans = (F64)(1.f - desired_alpha);
	F64 trans_at_this_step = pow(desired_trans, 1.0 / (F64)nb_layers);
	return (F32)(1.0 - trans_at_this_step);
}

// This method draws several big black spheres around the avatar, with various
// alphas. Alpha goes from mCamDistDrawAlphaMin to mCamDistDrawAlphaMax.
// Things to remember :
// - There are two render limits in RLV: min and max (min is a sphere with a
//   variable alpha and max is an opaque sphere).
// - Render limit min <= render limit max.
// - If a render limit is <= 1.0, make it 1.0 because we will be forced into
//   mouselook anyway, so it would be better to render the sphere
// - If a render limit is unspecified (i.e. equal to EXTREMUM), do not render
//   it.
// - If both render limits are specified and different, render both and several
//   in-between at regular intervals, with a linear interpolation for alpha
//   between mCamDistDrawAlphaMin and mCamDistDrawAlphaMax for each sphere.
// - There are not too many spheres to render, because stacking alphas make the
//   video card complain.
void RLInterface::drawRenderLimit(bool force_opaque)
{
	if (!mVisionRestricted)
	{
		return;
	}

	gGL.setColorMask(true, false);

	gUIProgram.bind();

	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gPipeline.disableLights();

	// Calculate the center of the spheres
	LLJoint* ref_joint = getCamDistDrawFromJoint();
	LLVector3 center = ref_joint ? ref_joint->getWorldPosition()
								 : gAgent.getPositionAgent();

	static LLCachedControl<U32> policy(gSavedSettings,
									   "RenderHighlightSelectionsPolicy");
	// If the inner sphere is opaque, just render it and no other
	// Also make the inner sphere opaque if we are highlighting invisible
	// surfaces or if anything is highlighted by a selection (edit, select or
	// drag and drop).
	if (force_opaque || mCamDistDrawAlphaMin >= UPPER_ALPHA_LIMIT ||
		LLDrawPoolAlpha::sShowDebugAlpha ||
		(policy > 0 && (!gSelectMgr.getSelection()->isEmpty() ||
						gToolDragAndDrop.getCargoCount())))
	{
		drawSphere(center, mCamDistDrawMin, mCamDistDrawColor, 1.f);
	}
	else
	{
		// If the outer sphere is opaque, render it now before switching to
		// blend mode
		bool outer_opaque = mCamDistDrawAlphaMax >= UPPER_ALPHA_LIMIT;
		if (outer_opaque)
		{
			drawSphere(center, mCamDistDrawMax, mCamDistDrawColor, 1.f);
		}
		// Switch to blend mode now
		LLGLEnable gls_blend(GL_BLEND);
		LLGLEnable gls_cull(GL_CULL_FACE);
		LLGLDepthTest gls_depth(GL_TRUE, GL_FALSE);
		gGL.setColorMask(true, false);

		F32 alpha_step = calculateDesiredAlphaPerStep(mCamDistDrawAlphaMax,
													  mCamDistNbGradients);
		// If the outer sphere is not opaque, render it now since we have not
		// before switching to blend mode.
		if (!outer_opaque)
		{
			drawSphere(center, mCamDistDrawMax, mCamDistDrawColor, alpha_step);
		}

		F32 lerp_factor = 1.f / (F32)mCamDistNbGradients;
		for (S32 i = mCamDistNbGradients - 1; i > 0; --i)
		{
			drawSphere(center,
					  lerp(mCamDistDrawMin, mCamDistDrawMax,
						   (F32)i * lerp_factor),
					   mCamDistDrawColor, alpha_step);
		}
	}

	gGL.flush();
	gGL.setColorMask(true, false);

	gUIProgram.unbind();

	mRenderLimitRenderedThisFrame = true;
}

void RLInterface::drawSphere(const LLVector3& center, F32 scale,
							 const LLColor3& color, F32 alpha)
{
	if (alpha < 0.001f)
	{
		return;	// Sphere is almost invisible, so...
	}

	gGL.pushMatrix();

	gGL.translatef(center[0], center[1], center[2]);
	gGL.scalef(scale, scale, scale);

	LLColor4 color_alpha(color, alpha);
	gGL.color4fv(color_alpha.mV);

	// Render inside only (the camera is not supposed to go outside anyway)
	glCullFace(GL_FRONT);
	gSphere.render();
	glCullFace(GL_BACK);

	gGL.popMatrix();
}

LLJoint* RLInterface::getCamDistDrawFromJoint()
{
	if (!isAgentAvatarValid())
	{
		return NULL;
	}

	if (!mCamDistDrawFromJoint ||
		gAgent.getCameraMode() == CAMERA_MODE_MOUSELOOK)
	{
		return gAgentAvatarp->mHeadp;
	}

	return mCamDistDrawFromJoint;
}

S32 RLInterface::avatarVisibility(LLVOAvatar* avatarp)
{
	// Fastest tests first.
	if (!avatarp)
	{
		return 0;
	}
	if ((mShowavsDistMax == EXTREMUM && mCamDistDrawMax == EXTREMUM) ||
		avatarp->isSelf())
	{
		return 1;
	}

	// Get the distance from our agent avatar
	LLVector3d dist_vec = gAgent.getPositionGlobal() -
						  gAgent.getPosGlobalFromAgent(avatarp->getCharacterPosition()) -
		gAgent.getPositionGlobal();
	F32 squared_dist = dist_vec.lengthSquared();

	// For camavdist, we always jelly-dollify avatars beyond its distance.
	if (mShowavsDistMax < EXTREMUM &&
		squared_dist > mShowavsDistMax * mShowavsDistMax)
	{
		return -1;
	}

	// For camdrawmax, when the avatar is beyond this distance and the outer
	// sphere is opaque, we do not bother rendering it at all. When the outer
	// sphere is not opaque but ALM is off, we jelly-dollify any avatar beyond
	// this distance since legacy avatars are unaffected by the spheres. HB
	if (mCamDistDrawMax < EXTREMUM &&
		squared_dist > mCamDistDrawMax * mCamDistDrawMax)
	{
		if (mCamDistDrawAlphaMax >= 0.999f)
		{
			return 0;
		}
		return LLPipeline::sRenderDeferred ? 1 : -1;
	}

	return 1;
}
