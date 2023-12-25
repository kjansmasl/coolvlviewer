/**
 * @file llfloaterdisplayname.cpp
 * @author Leyla Farazha
 * @brief Implementation of the LLFloaterDisplayName class.
 *
 * $LicenseInfo:firstyear=2002&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#include "llviewerprecompiledheaders.h"

#include "llfloaterdisplayname.h"

#include "llavatarnamecache.h"
#include "llnotifications.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "llviewercontrol.h"
#include "llviewerdisplayname.h"
#include "llviewermessage.h"		// For formatted_time()

LLFloaterDisplayName::LLFloaterDisplayName(const LLSD&)
{
	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_display_name.xml");
}

//virtual
bool LLFloaterDisplayName::postBuild()
{
	childSetAction("reset_btn", onReset, this);
	childSetAction("cancel_btn", onCancel, this);
	childSetAction("save_btn", onSave, this);

	center();

	return true;
}

//virtual
void LLFloaterDisplayName::onOpen()
{
	getChild<LLUICtrl>("display_name_editor")->clear();
	getChild<LLUICtrl>("display_name_confirm")->clear();

	LLAvatarName av_name;
	LLAvatarNameCache::get(gAgentID, &av_name);

	F64 now_secs = LLTimer::getEpochSeconds();
	if (now_secs < av_name.mNextUpdate)
	{
		// ... cannot update until some time in the future
		F64 next_update_local_secs = av_name.mNextUpdate;
		std::string next_update_string =
			formatted_time((time_t)next_update_local_secs);
		getChild<LLUICtrl>("lockout_text")->setTextArg("[TIME]",
													   next_update_string);
		getChild<LLUICtrl>("lockout_text")->setVisible(true);
		getChild<LLUICtrl>("now_ok_text")->setVisible(false);
		getChild<LLUICtrl>("save_btn")->setEnabled(false);
		getChild<LLUICtrl>("display_name_editor")->setEnabled(false);
		getChild<LLUICtrl>("display_name_confirm")->setEnabled(false);
		getChild<LLUICtrl>("cancel_btn")->setFocus(true);

	}
	else
	{
		getChild<LLUICtrl>("lockout_text")->setVisible(false);
		getChild<LLUICtrl>("now_ok_text")->setVisible(true);
		getChild<LLUICtrl>("save_btn")->setEnabled(true);
		getChild<LLUICtrl>("display_name_editor")->setEnabled(true);
		getChild<LLUICtrl>("display_name_confirm")->setEnabled(true);

	}
}

//static
void LLFloaterDisplayName::onCacheSetName(bool success,
										  const std::string& reason,
										  const LLSD& content)
{
	if (success)
	{
		// Inform the user that the change took place, but will take a while
		// to percolate.
		LLSD args;
		args["DISPLAY_NAME"] = content["display_name"];
		gNotifications.add("SetDisplayNameSuccess", args);
		return;
	}

	// Request failed, notify the user
	std::string error_tag = content["error_tag"].asString();
	llwarns << "Set name failure error_tag: " << error_tag << llendl;
	// We might have a localized string for this message; error_args will
	// usually be empty from the server.
	if (!error_tag.empty() && gNotifications.templateExists(error_tag))
	{
		gNotifications.add(error_tag);
		return;
	}

	// The server error might have a localized message for us
	std::string lang_code = LLUI::getLanguage();
	LLSD error_desc = content["error_description"];
	if (error_desc.has(lang_code))
	{
		LLSD args;
		args["MESSAGE"] = error_desc[lang_code].asString();
		gNotifications.add("GenericAlert", args);
		return;
	}

	// No specific error, throw a generic one
	gNotifications.add("SetDisplayNameFailedGeneric");
}

//static
void LLFloaterDisplayName::onCancel(void* data)
{
	LLFloaterDisplayName* self = (LLFloaterDisplayName*)data;
	if (self)
	{
		self->close();
	}
}

//static
void LLFloaterDisplayName::onReset(void* data)
{
	LLFloaterDisplayName* self = (LLFloaterDisplayName*)data;
	if (!self) return;

	if (LLAvatarNameCache::useDisplayNames())
	{
		LLViewerDisplayName::set("",
								 boost::bind(&LLFloaterDisplayName::onCacheSetName,
											 _1, _2, _3));
	}
	else
	{
		gNotifications.add("SetDisplayNameFailedGeneric");
	}

	self->close();
}

//static
void LLFloaterDisplayName::onSave(void* data)
{
	LLFloaterDisplayName* self = (LLFloaterDisplayName*)data;
	if (!self) return;

	std::string display_name_utf8;
	display_name_utf8 = self->getChild<LLUICtrl>("display_name_editor")->getValue().asString();
	std::string display_name_confirm;
	display_name_confirm = self->getChild<LLUICtrl>("display_name_confirm")->getValue().asString();

	if (display_name_utf8.compare(display_name_confirm))
	{
		gNotifications.add("SetDisplayNameMismatch");
		return;
	}

	constexpr U32 DISPLAY_NAME_MAX_LENGTH = 31; // Characters, not bytes
	LLWString display_name_wstr = utf8str_to_wstring(display_name_utf8);
	if (display_name_wstr.size() > DISPLAY_NAME_MAX_LENGTH)
	{
		LLSD args;
		args["LENGTH"] = llformat("%d", DISPLAY_NAME_MAX_LENGTH);
		gNotifications.add("SetDisplayNameFailedLength", args);
		return;
	}

	if (LLAvatarNameCache::useDisplayNames())
	{
		LLViewerDisplayName::set(display_name_utf8,
								 boost::bind(&LLFloaterDisplayName::onCacheSetName,
											 _1, _2, _3));
	}
	else
	{
		gNotifications.add("SetDisplayNameFailedGeneric");
	}

	self->close();
}
