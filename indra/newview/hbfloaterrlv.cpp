/**
 * @file hbfloaterrlv.cpp
 * @brief The HBFloaterRLV and HBFloaterBlacklistRLV classes definitions
 *
 * $LicenseInfo:firstyear=2011&license=viewergpl$
 *
 * Copyright (c) 2011-2020, Henri Beauchamp
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

#include "hbfloaterrlv.h"

#include "llbutton.h"
#include "llcachename.h"
#include "llcheckboxctrl.h"
#include "hbfastmap.h"
#include "llinventory.h"
#include "llnotifications.h"
#include "llscrolllistctrl.h"
#include "lltabcontainer.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "mkrlinterface.h"
#include "hbviewerautomation.h"
#include "llviewercontrol.h"
#include "llviewerobjectlist.h"
#include "llviewerwindow.h"			// For gViewerWindowp
#include "llvoavatar.h"

// This is used as a cache for the names of objects emitting RestrainedLove
// commands. The name is captured from the log or from the inventory for
// attachments. We keep this cache for the duration of the viewer session since
// it will stay quite small.
typedef fast_hmap<LLUUID, std::string> cached_names_map_t;
static cached_names_map_t sCachedNamesMap;

///////////////////////////////////////////////////////////////////////////////
// Helper functions
///////////////////////////////////////////////////////////////////////////////

static std::string get_object_name(const LLUUID& id, bool& is_lua, bool& is_gone,
								   bool& is_root)
{
	is_lua = is_gone = is_root = false;

#if LL_LINUX
	// If it bears a the fake UUID used for Lua D-Bus, then the restrictions
	// were set via Lua D-Bus scripting. Note: we check for id.notNull() since
	// it may happen that no D-Bus command was sent just yet, in which case
	// sLuaDBusFakeObjectId would still be a null UUID.
	if (id.notNull() && id == HBViewerAutomation::sLuaDBusFakeObjectId)
	{
		is_lua = true;
		return "Lua D-Bus";
	}
#endif

	if (id == gAgentID)
	{
		// If it bears our avatar UUID, then the restrictions were set via Lua
		// scripting.
		is_lua = true;
		return "Lua script";
	}

	LLViewerObject* objectp = gObjectList.findObject(id);
	if (!objectp)
	{
		// Object is gone (detached/derezzed) !
		is_gone = true;

		// Let's see if we have its name cached...
		cached_names_map_t::iterator it = sCachedNamesMap.find(id);
		if (it != sCachedNamesMap.end())
		{
			return it->second;
		}

		return id.asString();
	}

	if (objectp == objectp->getRootEdit())
	{
		// Root primitive
		is_root = true;
	}

	std::string name;

	LLInventoryItem* itemp = gRLInterface.getItem(id);
	if (itemp)
	{
		name = itemp->getName();
	}

	// If the name is empty, either this is an attachement that renamed itself
	// with an empty name, or this is not an attachament but an in-world object
	// (owned by us)... which should have used a relay...
	if (name.empty())
	{
		// Let's see if we have its name cached...
		cached_names_map_t::iterator it = sCachedNamesMap.find(id);
		if (it != sCachedNamesMap.end())
		{
			name = it->second;
		}
		else
		{
			name = id.asString();
		}
	}
	else
	{
		// Cache the name for this object, in case it gets derezzed (or renamed
		// to an empty name) later...
		sCachedNamesMap.emplace(id, name);
	}

	return name;
}

// This helper function is responsible for decorating object names with the
// proper font face and color.
static std::string get_name_decoration(bool is_lua, bool is_gone,
									   bool is_root, LLColor4& color)
{
	if (is_lua)
	{
		color = LLColor4::green3.getValue();
		return "NORMAL";
	}
	if (is_gone)
	{
		color = LLColor4::red2.getValue();
	}
	if (is_root)
	{
		return "BOLD";
	}

	return "NORMAL";
}

// Helper to setup the LLSD for an object status list element.
static void set_status(LLSD& element, const LLUUID& id,
					   const std::string& text)
{
	LLSD& columns = element["columns"];

	columns[0]["column"] = "object_name";
	columns[0]["font"] = "SANSSERIF_SMALL";
	bool is_lua, is_gone, is_root;
	columns[0]["value"] = get_object_name(id, is_lua, is_gone, is_root);
	LLColor4 color;	// Set to black by default on construction
	columns[0]["font-style"] = get_name_decoration(is_lua, is_gone, is_root,
												   color);
	if (color != LLColor4::black)	// If black, use the default skin color
	{
		columns[0]["color"] = color.getValue();
	}

	columns[1]["column"] = "commands";
	columns[1]["font"] = "SANSSERIF_SMALL";
	columns[1]["value"] = text;
}

// Helper to resolve potential UUIDs into attachment/avatar/group names in RLV
// exceptions target.
static void resolve_name(std::string& text)
{
	if (text.size() != 36)
	{
		// Not an Id, do not bother
		return;
	}

	LLUUID id(text, false);
	if (id.isNull())
	{
		// Not a valid Id either.
		return;
	}

	// Perhaps an attachment...
	LLInventoryItem* itemp = gRLInterface.getItem(id);
	if (itemp)
	{
		text = itemp->getName();
		return;
	}

	// Note: we do not bother with gCacheNamep callbacks and asynchronous UUID
	// to name (time consuming) replacements in the scroll lists, because the
	// queries for missing names will be sent to the server anyway, and the
	// result will have arrived next time we rebuild the list (on next RLV
	// command processing or by using the "Refresh" button)...

	// Perhaps a group... Note: it is important NOT to query for a group name
	// when the Id is not one of a group, because then the following avatar
	// name request with the same Id would not get properly queued !  That is
	// why we restrict the search to groups to which the agent does belong
	// (they would not be able to use other groups anyway).
	std::string name;
	if (gAgent.isInGroup(id))
	{
		if (gCacheNamep && gCacheNamep->getGroupName(id, name))
		{
			text = name;
		}
		return;
	}

	if (gRLenabled  && gRLInterface.mContainsShownames)
	{
		return;
	}

	// Perhaps an avatar currently around...
	LLVOAvatar* avatarp = gObjectList.findAvatar(id);
	if (avatarp)
	{
		text = avatarp->getFullname(true);
		return;
	}

	// Perhaps an offline or far avatar...
	if (gCacheNamep && gCacheNamep->getFullName(id, name))
	{
		text = name;
	}
}

// Helper function to maintain and update a set of 'restrictions' and a map of
// 'exceptions'. NOTE: 'cmd' (which is the input parameter) may also get
// modified by this function.
typedef std::set<std::string> cmd_list_t;
typedef std::map<std::string, std::string> except_map_t;
static void parse_command(std::string& cmd, cmd_list_t& restrictions,
						  except_map_t& exceptions)
{
	// Note: this list can be found by grep'ing the sources for
	// containsWithoutException
	static const cmd_list_t exception_types =
	{
		"edit",
		"recvchat",
		"recvemote",
		"recvim",
		"sendchannel",
		"sendim",
		"share",
		"shownames",
		"startim",
		"touchhud",
		"touchworld",
		"tplure",
		"tprequest"
	};

	if (cmd.compare(0, 6, "notify") == 0)
	{
		return;	// Ignore notification commands
	}

	// Special exception/relaxation, applying to the restricted agent.
	if (cmd == "emote")
	{
		if (!restrictions.count(cmd))
		{
			restrictions.emplace(cmd);
			exceptions[cmd] = HBFloaterRLV::sUnrestrictedEmotes;
		}
		return;
	}

	// Check to see if the command is another type of exception...
	size_t i = cmd.find(':');
	if (i != std::string::npos && i < cmd.size() - 1)
	{
		std::string restriction = cmd.substr(0, i);
		// Account *_sec variants exceptions in the same category as their
		// non-*_sec variant; this is correct (even if not docummented)
		// and corresponds exactly with what containsWithoutException() is
		// doing with exceptions...
		size_t j = restriction.rfind("_sec");
		if (j != std::string::npos && j == restriction.size() - 4)
		{
			restriction.erase(j);
		}
		cmd_list_t::const_iterator it = exception_types.find(restriction);
		if (it != exception_types.end())
		{
			// We have an exception !
			std::string exception = cmd.substr(i + 1);
			resolve_name(exception);	// Turn UUID into name if applicable
			except_map_t::iterator iter = exceptions.find(restriction);
			if (iter == exceptions.end())
			{
				// New exception for this type
				exceptions[restriction] = exception;
			}
			else
			{
				// Add exception to existing exception(s) for this type, if not
				// already there.
				exception = "," + exception;
				std::string existing = "," + iter->second + ",";
				if (existing.find(exception + ",") == std::string::npos)
				{
					iter->second += exception;
				}
			}
			return;
		}
	}

	// Account *_sec variants restrictions in the same category as their
	// non-*_sec variant. This is a simplification of how RestrainedLove
	// deals with *_sec restrictions (since those only accept exceptions set
	// from the same object). *TODO: fix that, perhaps ?...
	i = cmd.rfind("_sec");
	if (i != std::string::npos && i == cmd.size() - 4)
	{
		cmd.erase(i);
	}
	// It is not a notification or an exception, so it must be a restriction
	if (!restrictions.count(cmd))
	{
		restrictions.emplace(cmd);
	}
}

// Helper to setup the LLSD for a log list element.
static void set_log_line(LLSD& element,
						 const HBFloaterRLV::LoggedCommand& log_line)
{
	LLSD& columns = element["columns"];

	columns[0]["column"] = "time_stamp";
	columns[0]["font"] = "SANSSERIF_SMALL";
	columns[0]["value"] = log_line.mTimeStamp;

	columns[1]["column"] = "object_name";
	columns[1]["font"] = "SANSSERIF_SMALL";
	columns[1]["value"] = log_line.mName;
	LLColor4 color;	// Set to black by default on construction
	columns[1]["font-style"] = get_name_decoration(log_line.mIsLua,
												   log_line.mIsGone,
												   log_line.mIsRoot, color);
	if (color != LLColor4::black)	// If black, use the default skin color
	{
		columns[1]["color"] = color.getValue();
	}

	columns[2]["column"] = "status";
	columns[2]["font"] = "SANSSERIF_SMALL";
	switch (log_line.mStatus)
	{
		case HBFloaterRLV::QUEUED:
			columns[2]["value"] = HBFloaterRLV::sQueued;
			columns[2]["color"] = LLColor4::blue.getValue();
			break;

		case HBFloaterRLV::FAILED:
			columns[2]["value"] = HBFloaterRLV::sFailed;
			columns[2]["color"] = LLColor4::red2.getValue();
			break;

		case HBFloaterRLV::BLACKLISTED:
			columns[2]["value"] = HBFloaterRLV::sBlacklisted;
			columns[2]["font-style"] = "BOLD";
			break;

		case HBFloaterRLV::IMPLICIT:
			columns[2]["value"] = HBFloaterRLV::sImplicit;
			columns[2]["color"] = LLColor4::green3.getValue();
			break;

		default:
			columns[2]["value"] = HBFloaterRLV::sExecuted;
			columns[2]["color"] = LLColor4::green3.getValue();
	}

	columns[3]["column"] = "command";
	columns[3]["font"] = "SANSSERIF_SMALL";
	columns[3]["value"] = log_line.mCommand;
}

///////////////////////////////////////////////////////////////////////////////
// HBFloaterRLV class
///////////////////////////////////////////////////////////////////////////////

//static
std::vector<HBFloaterRLV::LoggedCommand> HBFloaterRLV::sLoggedCommands;
std::string HBFloaterRLV::sQueued;
std::string HBFloaterRLV::sFailed;
std::string HBFloaterRLV::sExecuted;
std::string HBFloaterRLV::sBlacklisted;
std::string HBFloaterRLV::sImplicit;
std::string HBFloaterRLV::sUnrestrictedEmotes;

// Sub-class used to register commands in the log
HBFloaterRLV::LoggedCommand::LoggedCommand(const LLUUID& id,
										   const std::string& name,
										   const std::string& cmd, S32 status)
:	mName(name),
	mCommand(cmd),
	mStatus(status)
{
	// Note: we register the data at the moment the command is logged because
	// the object could disappear or be renamed later on. We do not store the
	// Id either (excepted as a name for missing objects or objects with empty
	// names), since in the case of an attachment, it could get modified via
	// auto-reattaching when kicked off; see the RLInterface::replace() method
	// which is used by the LLViewerJointAttachment::addObject() method.
	std::string cur_name = get_object_name(id, mIsLua, mIsGone, mIsRoot);
	// Give priority to the name transmitted via the llOwnerSay() chat message,
	// but if empty, use the name we found with get_object_name() which is, at
	// worst, the object UUID...
	if (mName.empty())
	{
		mName = cur_name;
	}
	else if (!mIsLua)
	{
		// Cache the object name when we have one.
		sCachedNamesMap.emplace(id, mName);
	}

	time_t utc_time = time_corrected();
	struct tm* timep = utc_time_to_tm(utc_time);
	// Make it easy to sort: use the Year-Month-Day HH:MM:SS ISO convention
	timeStructToFormattedString(timep, "%Y-%m-%d %H:%M:%S", mTimeStamp);
	// Special, internal command meaning: end of @relayed commands block.
	if (cmd == " ")
	{
		mCommand = "end-relayed";
		if (status == EXECUTED)
		{
			mStatus = IMPLICIT;
		}
	}
}

HBFloaterRLV::HBFloaterRLV(const LLSD&)
:	mIsDirty(false),
	mFirstOpen(true),
	mLastCommandsLogSize(0)
{
	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_rlv_status.xml");
	if (sQueued.empty())
	{
		sQueued = getString("queued");
		sFailed = getString("failed");
		sExecuted = getString("executed");
		sBlacklisted = getString("blacklisted");
		sImplicit = getString("implicit");
		sUnrestrictedEmotes = getString("unrestricted_emote");
	}
}

//virtual
bool HBFloaterRLV::postBuild()
{
	mTabContainer = getChild<LLTabContainer>("tabs");
	LLPanel* tab = mTabContainer->getChild<LLPanel>("status");
	mTabContainer->setTabChangeCallback(tab, onTabChanged);
	mTabContainer->setTabUserData(tab, this);
	tab = mTabContainer->getChild<LLPanel>("restrictions");
	mTabContainer->setTabChangeCallback(tab, onTabChanged);
	mTabContainer->setTabUserData(tab, this);
	tab = mTabContainer->getChild<LLPanel>("commands_log");
	mTabContainer->setTabChangeCallback(tab, onTabChanged);
	mTabContainer->setTabUserData(tab, this);

	mStatusByObject = getChild<LLScrollListCtrl>("status_list");
	mStatusByObject->setDoubleClickCallback(onDoubleClick);
	mStatusByObject->setCallbackUserData(this);

	mRestrictions = getChild<LLScrollListCtrl>("restrictions_list");

	mCommandsLog = getChild<LLScrollListCtrl>("commands_list");

	childSetAction("help", onButtonHelp, this);

	mRefreshButton = getChild<LLButton>("refresh_btn");
	mRefreshButton->setClickedCallback(onButtonRefresh, this);

	mClearButton = getChild<LLButton>("clear_btn");
	mClearButton->setClickedCallback(onButtonClear, this);

	childSetAction("close_btn", onButtonClose, this);

	setButtonsStatus();

	mIsDirty = true;

	return true;
}

//virtual
void HBFloaterRLV::onOpen()
{
	if (mFirstOpen)
	{
		mFirstOpen = false;
		mTabContainer->selectTab(gSavedSettings.getS32("LastRLVFloaterTab"));
	}
}

//virtual
void HBFloaterRLV::draw()
{
	if (gRLenabled && gRLInterface.mContainsViewscript)
	{
		close();
		return;
	}

	if (mIsDirty)
	{
		S32 scrollpos1 = mStatusByObject->getScrollPos();
		S32 scrollpos2 = mRestrictions->getScrollPos();

		// It is faster to rebuild fully these lists than trying to figure out
		// what changed in them...
		mStatusByObject->deleteAllItems();
		mRestrictions->deleteAllItems();

		if (!gRLInterface.mSpecialObjectBehaviours.empty())
		{
			cmd_list_t restrictions;
			except_map_t exceptions;
			LLUUID id, old_id;
			std::string commands, cmd;
			bool first = true;
			rl_map_it_t it = gRLInterface.mSpecialObjectBehaviours.begin();
			while (it != gRLInterface.mSpecialObjectBehaviours.end())
			{
				// We concatenate all commands pertaining to the same object
				old_id = id;
				id = LLUUID(it->first);
				if (!first && id != old_id)
				{
					LLSD element;
					set_status(element, old_id, commands);
					mStatusByObject->addElement(element);
					commands = "";
				}
				if (!commands.empty())
				{
					commands += ",";
				}
				cmd = it++->second;
				commands += cmd;
				first = false;

				// Now, parse the command and classify it as restriction or
				// exception, as appropriate.
				parse_command(cmd, restrictions, exceptions);
			}
			// Add the last object in the status list
			LLSD element;
			set_status(element, id, commands);
			mStatusByObject->addElement(element);

			// Finally, build the restrictions/exceptions list from our map
			except_map_t::const_iterator iter;
			except_map_t::const_iterator except_end = exceptions.end();
			for (cmd_list_t::const_iterator it = restrictions.begin(),
											end = restrictions.end();
				 it != end; ++it)
			{
				LLSD element;
				LLSD& columns = element["columns"];

				columns[0]["column"] = "restriction";
				columns[0]["font"] = "SANSSERIF_SMALL";
				columns[0]["value"] = *it;

				columns[1]["column"] = "exception";
				columns[1]["font"] = "SANSSERIF_SMALL";
				iter = exceptions.find(*it);
				columns[1]["value"] = iter != except_end ? iter->second : "";

				mRestrictions->addElement(element);
			}
		}

		mStatusByObject->setScrollPos(scrollpos1);
		mRestrictions->setScrollPos(scrollpos2);

		// If a log line is selected in the list, remember the scroll position
		// to restore it later.
		S32 scrollpos = -1;
		if (mCommandsLog->getFirstSelected())
		{
			scrollpos = mCommandsLog->getScrollPos();
		}

		// Here we append new log lines to the existing list, for speed

		U32 count = sLoggedCommands.size();
		if (!mLastCommandsLogSize || count < mLastCommandsLogSize)
		{
			mCommandsLog->deleteAllItems();
			mLastCommandsLogSize = 0;
		}
		if (count > mLastCommandsLogSize)
		{
			if (mCommandsLog->hasSortOrder())
			{
				mCommandsLog->clearSortOrder();
			}
			for (U32 i = mLastCommandsLogSize; i < count; ++i)
			{
				LLSD element;
				set_log_line(element, sLoggedCommands[i]);
				mCommandsLog->addElement(element);
			}
			mLastCommandsLogSize = sLoggedCommands.size();
			// Automatically clamped to last line
			mCommandsLog->setScrollPos(S32_MAX);
		}

		// Restore the scroll position, if a log line was selected, so that
		// the user can choose whether or not to let the list scroll on new
		// events.
		if (scrollpos >= 0)
		{
			mCommandsLog->setScrollPos(scrollpos);
		}
		mIsDirty = false;
	}

	LLFloater::draw();
}

void HBFloaterRLV::setButtonsStatus()
{
	bool can_refresh = mTabContainer->getCurrentPanelIndex() < 2;
	mRefreshButton->setVisible(can_refresh);
	mClearButton->setVisible(!can_refresh);
}

//static
void HBFloaterRLV::setDirty()
{
 	HBFloaterRLV* self = findInstance();
	if (self)
	{
		self->mIsDirty = true;
	}
}

//static
void HBFloaterRLV::logCommand(const LLUUID& obj_id, const std::string& obj_name,
							  const std::string& command, S32 status)
{
	sLoggedCommands.emplace_back(obj_id, obj_name, command, status);

	// Note: the constructor of LoggedCommand may change the status and name of
	// the logged command (this is currently the case for the "end-relay"
	// implicit command) as well as the name of the object name. So we must
	// recover the actually stored status and names.
	const LoggedCommand& logged_cmd = sLoggedCommands.back();
	const std::string& name = logged_cmd.mName;
	const std::string& cmd = logged_cmd.mCommand;
	status = logged_cmd.mStatus;
	switch (status)
	{
		case QUEUED:
			LL_DEBUGS("RestrainedLove") << "Queued command for '"
										<< name << "' (" << obj_id << "): "
										<< cmd << LL_ENDL;
			break;

		case FAILED:
			llwarns << "Failed command for '" << name << "' (" << obj_id
					<< "): " << cmd << llendl;
			break;

		case EXECUTED:
			LL_DEBUGS("RestrainedLove") << "Success executing command for '"
										<< name << "' (" << obj_id << "): "
										<< cmd << LL_ENDL;
			break;

		case IMPLICIT:
			LL_DEBUGS("RestrainedLove") << "Executed implicit command for '"
										<< name << "' (" << obj_id << "): "
										<< cmd << LL_ENDL;
			break;

		case BLACKLISTED:
			LL_DEBUGS("RestrainedLove") << "Blacklisted command for '"
										<< name << "' (" << obj_id << "): "
										<< cmd << LL_ENDL;
			break;

		default:
			llwarns << "Unknown status code " << status << " for command '"
					<< cmd << "' executed on behalf of: '" << name << "' ("
					<< obj_id << ")" << llendl;
			llassert(false);
	}

	setDirty();
}

//static
void HBFloaterRLV::onTabChanged(void* data, bool)
{
	HBFloaterRLV* self = (HBFloaterRLV*)data;
	if (self && self->mTabContainer)	// Paranoia
	{
		gSavedSettings.setS32("LastRLVFloaterTab",
							  self->mTabContainer->getCurrentPanelIndex());
		self->setButtonsStatus();
	}
}

//static
void HBFloaterRLV::onButtonHelp(void*)
{
	gNotifications.add("RLVFLoaterHelp");
}

//static
void HBFloaterRLV::onButtonRefresh(void* data)
{
	HBFloaterRLV* self = (HBFloaterRLV*)data;
	if (self)
	{
		gRLInterface.garbageCollector(false);
		self->setDirty();
	}
}

//static
void HBFloaterRLV::onButtonClear(void* data)
{
	HBFloaterRLV* self = (HBFloaterRLV*)data;
	if (self)
	{
		sLoggedCommands.clear();
		self->mLastCommandsLogSize = 0;
		self->mIsDirty = true;
	}
}

//static
void HBFloaterRLV::onButtonClose(void* data)
{
	HBFloaterRLV* self = (HBFloaterRLV*)data;
	if (self)
	{
		self->close();
	}
}

//static
void HBFloaterRLV::onDoubleClick(void* data)
{
	HBFloaterRLV* self = (HBFloaterRLV*)data;
 	if (!self) return;

	LLScrollListItem* item = self->mStatusByObject->getFirstSelected();
	if (!item) return;

	// Get the commands in force
	const std::string& commands = item->getColumn(1)->getValue().asString();

	// Copy it to clipboard
	gWindowp->copyTextToClipboard(utf8str_to_wstring(commands));

	// Notify

	LL_DEBUGS("RestrainedLove") << "RestrainedLove commands in force for object '";
	const std::string& name = item->getColumn(0)->getValue().asString();
	LL_CONT << name << "': " << commands << LL_ENDL;

	gNotifications.add("RLVCommandsCopiedtoClipboard");
}

///////////////////////////////////////////////////////////////////////////////
// HBFloaterBlacklistRLV class
///////////////////////////////////////////////////////////////////////////////

// Helper functions

std::string getCommands(S32 type, bool csv = false)
{
	std::string commands = gRLInterface.getCommandsByType(type, true);
	if (!commands.empty())
	{
		commands = commands.substr(1);
		if (csv)
		{
			commands = "," + commands;
			LLStringUtil::replaceString(commands, "/", ",");
		}
		else
		{
			commands = "@" + commands + ",";
			LLStringUtil::replaceString(commands, "/", ", @");
			LLStringUtil::replaceString(commands, "%f", "=force");
			LLStringUtil::replaceString(commands, "_=", "_*=");
			LLStringUtil::replaceString(commands, "_,", "_*,");
			commands = commands.substr(0, commands.length() - 1);
		}
	}
	return commands;
}

bool isGroupInBlacklist(S32 type)
{
	std::string blacklist = gSavedSettings.getString("RestrainedLoveBlacklist");
	blacklist = "," + blacklist + ",";
	for (RLInterface::rl_command_map_it_t
			it = RLInterface::sCommandsMap.begin(),
			end = RLInterface::sCommandsMap.end();
		 it != end; ++it)
	{
		if ((S32)it->second == type &&
			blacklist.find("," + it->first + ",") == std::string::npos)
		{
			return false;
		}
	}
	return true;
}

// Member functions

HBFloaterBlacklistRLV::HBFloaterBlacklistRLV(const LLSD&)
{
    LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_rlv_blacklist.xml");
}

//virtual
bool HBFloaterBlacklistRLV::postBuild()
{
	childSetAction("apply", onButtonApply, this);
	childSetAction("cancel", onButtonCancel, this);

	// Tool tips creation:
	std::string prefix = getString("tool_tip_prefix") + " ";

	std::string tooltip = getCommands(RLInterface::RL_INSTANTMESSAGE);
	LLCheckBoxCtrl* check = getChild<LLCheckBoxCtrl>("instantmessage");
	check->setToolTip(prefix + tooltip);
	check->set(isGroupInBlacklist(RLInterface::RL_INSTANTMESSAGE));

	tooltip = getCommands(RLInterface::RL_CHANNEL);
	check = getChild<LLCheckBoxCtrl>("channel");
	check->setToolTip(prefix + tooltip);
	check->set(isGroupInBlacklist(RLInterface::RL_CHANNEL));

	tooltip = getCommands(RLInterface::RL_SENDCHAT);
	check = getChild<LLCheckBoxCtrl>("sendchat");
	check->setToolTip(prefix + tooltip);
	check->set(isGroupInBlacklist(RLInterface::RL_SENDCHAT));

	tooltip = getCommands(RLInterface::RL_RECEIVECHAT);
	check = getChild<LLCheckBoxCtrl>("receivechat");
	check->setToolTip(prefix + tooltip);
	check->set(isGroupInBlacklist(RLInterface::RL_RECEIVECHAT));

	tooltip = getCommands(RLInterface::RL_EMOTE);
	check = getChild<LLCheckBoxCtrl>("emote");
	check->setToolTip(prefix + tooltip);
	check->set(isGroupInBlacklist(RLInterface::RL_EMOTE));

	tooltip = getCommands(RLInterface::RL_REDIRECTION);
	check = getChild<LLCheckBoxCtrl>("redirection");
	check->setToolTip(prefix + tooltip);
	check->set(isGroupInBlacklist(RLInterface::RL_REDIRECTION));

	tooltip = getCommands(RLInterface::RL_MOVE);
	check = getChild<LLCheckBoxCtrl>("move");
	check->setToolTip(prefix + tooltip);
	check->set(isGroupInBlacklist(RLInterface::RL_MOVE));

	tooltip = getCommands(RLInterface::RL_SIT);
	check = getChild<LLCheckBoxCtrl>("sit");
	check->setToolTip(prefix + tooltip);
	check->set(isGroupInBlacklist(RLInterface::RL_SIT));

	tooltip = getCommands(RLInterface::RL_TELEPORT);
	check = getChild<LLCheckBoxCtrl>("teleport");
	check->setToolTip(prefix + tooltip);
	check->set(isGroupInBlacklist(RLInterface::RL_TELEPORT));

	tooltip = getCommands(RLInterface::RL_TOUCH);
	check = getChild<LLCheckBoxCtrl>("touch");
	check->setToolTip(prefix + tooltip);
	check->set(isGroupInBlacklist(RLInterface::RL_TOUCH));

	tooltip = getCommands(RLInterface::RL_INVENTORY);
	check = getChild<LLCheckBoxCtrl>("inventory");
	check->setToolTip(prefix + tooltip);
	check->set(isGroupInBlacklist(RLInterface::RL_INVENTORY));

	tooltip = getCommands(RLInterface::RL_INVENTORYLOCK);
	check = getChild<LLCheckBoxCtrl>("inventorylock");
	check->setToolTip(prefix + tooltip);
	check->set(isGroupInBlacklist(RLInterface::RL_INVENTORYLOCK));

	tooltip = getCommands(RLInterface::RL_LOCK);
	check = getChild<LLCheckBoxCtrl>("lock");
	check->setToolTip(prefix + tooltip);
	check->set(isGroupInBlacklist(RLInterface::RL_LOCK));

	tooltip = getCommands(RLInterface::RL_BUILD);
	check = getChild<LLCheckBoxCtrl>("build");
	check->setToolTip(prefix + tooltip);
	check->set(isGroupInBlacklist(RLInterface::RL_BUILD));

	tooltip = getCommands(RLInterface::RL_ATTACH);
	check = getChild<LLCheckBoxCtrl>("attach");
	check->setToolTip(prefix + tooltip);
	check->set(isGroupInBlacklist(RLInterface::RL_ATTACH));

	tooltip = getCommands(RLInterface::RL_DETACH);
	check = getChild<LLCheckBoxCtrl>("detach");
	check->setToolTip(prefix + tooltip);
	check->set(isGroupInBlacklist(RLInterface::RL_DETACH));

	tooltip = getCommands(RLInterface::RL_NAME);
	check = getChild<LLCheckBoxCtrl>("name");
	check->setToolTip(prefix + tooltip);
	check->set(isGroupInBlacklist(RLInterface::RL_NAME));

	tooltip = getCommands(RLInterface::RL_LOCATION);
	check = getChild<LLCheckBoxCtrl>("location");
	check->setToolTip(prefix + tooltip);
	check->set(isGroupInBlacklist(RLInterface::RL_LOCATION));

	tooltip = getCommands(RLInterface::RL_CAMERA);
	check = getChild<LLCheckBoxCtrl>("camera");
	check->setToolTip(prefix + tooltip);
	check->set(isGroupInBlacklist(RLInterface::RL_CAMERA));

	tooltip = getCommands(RLInterface::RL_GROUP);
	check = getChild<LLCheckBoxCtrl>("group");
	check->setToolTip(prefix + tooltip);
	check->set(isGroupInBlacklist(RLInterface::RL_GROUP));

	tooltip = getCommands(RLInterface::RL_DEBUG);
	check = getChild<LLCheckBoxCtrl>("debug");
	check->setToolTip(prefix + tooltip);
	check->set(isGroupInBlacklist(RLInterface::RL_DEBUG));

	tooltip = getCommands(RLInterface::RL_SHARE);
	check = getChild<LLCheckBoxCtrl>("share");
	check->setToolTip(prefix + tooltip);
	check->set(isGroupInBlacklist(RLInterface::RL_SHARE));

	center();

	return true;
}

//static
void HBFloaterBlacklistRLV::onButtonCancel(void* data)
{
	HBFloaterBlacklistRLV* self = (HBFloaterBlacklistRLV*)data;
	if (self)
	{
		self->close();
	}
}

//static
void HBFloaterBlacklistRLV::onButtonApply(void* data)
{
	HBFloaterBlacklistRLV* self = (HBFloaterBlacklistRLV*)data;
	if (self)
	{
		std::string blacklist;
		LLCheckBoxCtrl* check = self->getChild<LLCheckBoxCtrl>("instantmessage");
		if (check->get())
		{
			blacklist += getCommands(RLInterface::RL_INSTANTMESSAGE, true);
		}

		check = self->getChild<LLCheckBoxCtrl>("channel");
		if (check->get())
		{
			blacklist += getCommands(RLInterface::RL_CHANNEL, true);
		}

		check = self->getChild<LLCheckBoxCtrl>("sendchat");
		if (check->get())
		{
			blacklist += getCommands(RLInterface::RL_SENDCHAT, true);
		}

		check = self->getChild<LLCheckBoxCtrl>("receivechat");
		if (check->get())
		{
			blacklist += getCommands(RLInterface::RL_RECEIVECHAT, true);
		}

		check = self->getChild<LLCheckBoxCtrl>("emote");
		if (check->get())
		{
			blacklist += getCommands(RLInterface::RL_EMOTE, true);
		}

		check = self->getChild<LLCheckBoxCtrl>("redirection");
		if (check->get())
		{
			blacklist += getCommands(RLInterface::RL_REDIRECTION, true);
		}

		check = self->getChild<LLCheckBoxCtrl>("move");
		if (check->get())
		{
			blacklist += getCommands(RLInterface::RL_MOVE, true);
		}

		check = self->getChild<LLCheckBoxCtrl>("sit");
		if (check->get())
		{
			blacklist += getCommands(RLInterface::RL_SIT, true);
		}

		check = self->getChild<LLCheckBoxCtrl>("teleport");
		if (check->get())
		{
			blacklist += getCommands(RLInterface::RL_TELEPORT, true);
		}

		check = self->getChild<LLCheckBoxCtrl>("touch");
		if (check->get())
		{
			blacklist += getCommands(RLInterface::RL_TOUCH, true);
		}

		check = self->getChild<LLCheckBoxCtrl>("inventory");
		if (check->get())
		{
			blacklist += getCommands(RLInterface::RL_INVENTORY, true);
		}

		check = self->getChild<LLCheckBoxCtrl>("inventorylock");
		if (check->get())
		{
			blacklist += getCommands(RLInterface::RL_INVENTORYLOCK, true);
		}

		check = self->getChild<LLCheckBoxCtrl>("lock");
		if (check->get())
		{
			blacklist += getCommands(RLInterface::RL_LOCK, true);
		}

		check = self->getChild<LLCheckBoxCtrl>("build");
		if (check->get())
		{
			blacklist += getCommands(RLInterface::RL_BUILD, true);
		}

		check = self->getChild<LLCheckBoxCtrl>("attach");
		if (check->get())
		{
			blacklist += getCommands(RLInterface::RL_ATTACH, true);
		}

		check = self->getChild<LLCheckBoxCtrl>("detach");
		if (check->get())
		{
			blacklist += getCommands(RLInterface::RL_DETACH, true);
		}

		check = self->getChild<LLCheckBoxCtrl>("name");
		if (check->get())
		{
			blacklist += getCommands(RLInterface::RL_NAME, true);
		}

		check = self->getChild<LLCheckBoxCtrl>("location");
		if (check->get())
		{
			blacklist += getCommands(RLInterface::RL_LOCATION, true);
		}

		check = self->getChild<LLCheckBoxCtrl>("camera");
		if (check->get())
		{
			blacklist += getCommands(RLInterface::RL_CAMERA, true);
		}

		check = self->getChild<LLCheckBoxCtrl>("group");
		if (check->get())
		{
			blacklist += getCommands(RLInterface::RL_GROUP, true);
		}

		check = self->getChild<LLCheckBoxCtrl>("debug");
		if (check->get())
		{
			blacklist += getCommands(RLInterface::RL_DEBUG, true);
		}

		check = self->getChild<LLCheckBoxCtrl>("share");
		if (check->get())
		{
			blacklist += getCommands(RLInterface::RL_SHARE, true);
		}

		if (!blacklist.empty())
		{
			blacklist = blacklist.substr(1);
		}
		gSavedSettings.setString("RestrainedLoveBlacklist",	blacklist);

		self->close();
	}
}
