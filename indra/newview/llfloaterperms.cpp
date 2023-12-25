/**
 * @file llfloaterperms.cpp
 * @brief Asset creation permission preferences.
 * @author Coco
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 *
 * Copyright (c) 2001-2009, Linden Research, Inc.
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

#include "llfloaterperms.h"

#include "llpermissions.h"
#include "lluictrlfactory.h"

#include "llviewercontrol.h"

LLFloaterPerms::LLFloaterPerms(const LLSD& seed)
{
	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_perm_prefs.xml");
}

//virtual
bool LLFloaterPerms::postBuild()
{
	childSetEnabled("next_owner_transfer",
					gSavedSettings.getBool("NextOwnerCopy"));
	childSetAction("ok", onClickOK, this);
	childSetAction("cancel", onClickCancel, this);
	childSetCommitCallback("next_owner_copy", &onCommitCopy, this);

	refresh();

	return true;
}

//virtual
void LLFloaterPerms::refresh()
{
	mShareWithGroup		= gSavedSettings.getBool("ShareWithGroup");
	mEveryoneCopy		= gSavedSettings.getBool("EveryoneCopy");
	mNextOwnerCopy		= gSavedSettings.getBool("NextOwnerCopy");
	mNextOwnerModify	= gSavedSettings.getBool("NextOwnerModify");
	mNextOwnerTransfer	= gSavedSettings.getBool("NextOwnerTransfer");
	mFullPermNotecards	= gSavedSettings.getBool("FullPermNotecards");
	mFullPermSnapshots	= gSavedSettings.getBool("FullPermSnapshots");
	mNoModScripts		= gSavedSettings.getBool("NoModScripts");
}

void LLFloaterPerms::cancel()
{
	gSavedSettings.setBool("ShareWithGroup",	mShareWithGroup);
	gSavedSettings.setBool("EveryoneCopy",		mEveryoneCopy);
	gSavedSettings.setBool("NextOwnerCopy",		mNextOwnerCopy);
	gSavedSettings.setBool("NextOwnerModify",	mNextOwnerModify);
	gSavedSettings.setBool("NextOwnerTransfer",	mNextOwnerTransfer);
	gSavedSettings.setBool("FullPermNotecards",	mFullPermNotecards);
	gSavedSettings.setBool("FullPermSnapshots",	mFullPermSnapshots);
	gSavedSettings.setBool("NoModScripts",		mNoModScripts);
}

void LLFloaterPerms::onClose(bool app_quitting)
{
	// Cancel any unsaved changes before closing.
	// Note: when closed due to the OK button this amounts to a no-op.
	cancel();
	LLFloater::onClose(app_quitting);
}

//static
void LLFloaterPerms::onClickOK(void* data)
{
	LLFloaterPerms* self = (LLFloaterPerms*)data;
	if (self)
	{
		// Store changed flags so that cancel() in onClose() doesn't revert
		// them...
		self->refresh();
		self->close();
	}
}

//static
void LLFloaterPerms::onClickCancel(void* data)
{
	LLFloaterPerms* self = (LLFloaterPerms*)data;
	if (self)
	{
		self->cancel();
		self->close();
	}
}

//static
void LLFloaterPerms::onCommitCopy(LLUICtrl* ctrl, void* data)
{
	LLFloaterPerms* self = (LLFloaterPerms*)data;
	if (!self) return;
	// Implements fair use
	bool copyable = gSavedSettings.getBool("NextOwnerCopy");
	if (!copyable)
	{
		gSavedSettings.setBool("NextOwnerTransfer", true);
	}
	self->childSetEnabled("next_owner_transfer", copyable);
}

//static
U32 LLFloaterPerms::getGroupPerms(const std::string& prefix)
{
	std::string cname = prefix + "ShareWithGroup";
	return gSavedSettings.getBool(cname.c_str()) ? PERM_COPY
												 : PERM_NONE;
}

//static
U32 LLFloaterPerms::getEveryonePerms(const std::string& prefix)
{
	std::string cname = prefix + "EveryoneCopy";
	return gSavedSettings.getBool(cname.c_str()) ? PERM_COPY
												 : PERM_NONE;
}

//static
U32 LLFloaterPerms::getNextOwnerPerms(const std::string& prefix)
{
	U32 flags = PERM_MOVE;
	std::string cname = prefix + "NextOwnerCopy";
	if (gSavedSettings.getBool(cname.c_str()))
	{
		flags |= PERM_COPY;
	}
	cname = prefix + "NextOwnerModify";
	if (gSavedSettings.getBool(cname.c_str()))
	{
		flags |= PERM_MODIFY;
	}
	cname = prefix + "NextOwnerTransfer";
	if (gSavedSettings.getBool(cname.c_str()))
	{
		flags |= PERM_TRANSFER;
	}
	return flags;
}
