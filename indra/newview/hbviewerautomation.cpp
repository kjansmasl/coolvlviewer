/**
 * @file hbviewerautomation.cpp
 * @brief HBViewerAutomation class implementation
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

#include "llviewerprecompiledheaders.h"

#include <deque>
#include <map>

#include "boost/algorithm/string.hpp"

#include "lua.hpp"

#include "hbviewerautomation.h"

#include "imageids.h"
#include "llatomic.h"
#include "llaudioengine.h"
#include "llbase64.h"
#include "llbutton.h"
#include "llcachename.h"
#include "llcallbacklist.h"
#include "llcheckboxctrl.h"
#include "llclipboard.h"
#include "llcombobox.h"
#include "lldir.h"
#include "lleconomy.h"
#include "hbfastmap.h"
#include "llfasttimer.h"
#include "lllineeditor.h"
#include "llnamelistctrl.h"
#include "llnotifications.h"
#include "llradiogroup.h"
#include "llscrolllistctrl.h"
#include "llsdserialize.h"
#include "llsliderctrl.h"
#include "llspinctrl.h"
#include "lltexteditor.h"
#include "llthread.h"
#include "lltrans.h"
#include "lluictrlfactory.h"
#include "llmessage.h"

#include "llagent.h"
#include "llagentpilot.h"
#include "llagentwearables.h"
#include "llappearancemgr.h"
#include "llappviewer.h"
#include "llavatartracker.h"
#include "llchatbar.h"
#include "llenvironment.h"
#include "llenvsettings.h"
#include "llfloateractivespeakers.h"
#include "hbfloaterareasearch.h"
#include "llfloateravatarinfo.h"
#include "llfloateravatarpicker.h"
#include "llfloateravatartextures.h"
#include "llfloaterbeacons.h"
#include "hbfloaterbump.h"
#include "llfloatercamera.h"
#include "llfloaterchat.h"
#include "llfloaterchatterbox.h"
#include "llfloaterdebugsettings.h"
#include "hbfloaterdebugtags.h"
#include "llfloaterexperiences.h"
#include "llfloaterfriends.h"
#include "llfloatergesture.h"
#include "llfloatergroupinfo.h"
#include "llfloatergroups.h"
#include "llfloaterim.h"
#include "llfloaterinspect.h"
#include "llfloaterinventory.h"
#include "hbfloaterinvitemspicker.h"
#include "llfloaterland.h"
#include "llfloaterlandholdings.h"
#include "slfloatermediafilter.h"
#include "llfloaterminimap.h"
#include "llfloatermove.h"
#include "llfloatermute.h"
#include "llfloaternearbymedia.h"
#include "llfloaternotificationsconsole.h"
#include "llfloaterpathfindingcharacters.h"
#include "llfloaterpathfindinglinksets.h"
#include "llfloaterpreference.h"
#include "hbfloaterradar.h"
#include "llfloaterregioninfo.h"
#include "hbfloatersearch.h"
#include "llfloatersnapshot.h"
#include "hbfloatersoundslist.h"
#include "llfloaterstats.h"
#include "hbfloaterteleporthistory.h"
#include "llfloatertools.h"
#include "llfloaterworldmap.h"
#include "llfolderview.h"
#include "llgridmanager.h"
#include "llgroupmgr.h"
#include "llimmgr.h"
#include "llinventorymodelfetch.h"
#include "llmutelist.h"
#include "llnotify.h"
#include "lloverlaybar.h"
#include "llpipeline.h"
#include "hbpreprocessor.h"
#include "llpuppetmodule.h"
#include "llpuppetmotion.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llselectmgr.h"
#include "llstartup.h"
#include "llstatusbar.h"
#include "lltooldraganddrop.h"
#include "llurldispatcher.h"
#include "llvieweraudio.h"				// For get_valid_sounds()
#include "llviewercontrol.h"
#include "llviewerinventory.h"
#include "llviewermenu.h"
#include "llviewerobjectlist.h"
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"
#include "llviewerwindow.h"
#include "llvoavatarself.h"
#include "llweb.h"
#include "llwlskyparammgr.h"
#include "llwlwaterparammgr.h"
#include "llworldmap.h"

using namespace LLAvatarAppearanceDefines;

HBViewerAutomation* gAutomationp = NULL;

// Note: keep in sync with LLSettingsType::EType
static const std::string sEnvSettingsTypes[] = { "sky", "water", "day" };

enum Picked_types
{
	PICKED_LAND = 0,
	PICKED_PARTICLE,
	PICKED_OBJECT,
	PICKED_ATTACHMENT,
	PICKED_AVATAR,
	PICKED_SELF,
	PICKED_INVALID
};

enum Notification_types
{
	NOTIFYTIP,
	NOTIFICATION,
	ALERT,
};

///////////////////////////////////////////////////////////////////////////////
// HBLuaDialog class (generic usage floater for Lua scripts)
///////////////////////////////////////////////////////////////////////////////

class HBLuaDialog final : public LLFloater
{
protected:
	LOG_CLASS(HBLuaDialog);

public:
	static HBLuaDialog* create(const std::string& title,
							   const std::string& text,
							   const std::string& suggestion,
							   const std::string& btn1,
							   const std::string& btn2,
							   const std::string& btn3,
							   const std::string& command1,
							   const std::string& command2,
							   const std::string& command3);

private:
	// Use the create() method to create the floater.
	HBLuaDialog(const LLSD& parameters);
	~HBLuaDialog() override;

	bool postBuild() override;

	// Returns true when the dialog shall be closed
	bool evalLuaCommand(const std::string& command);

	static void onButton(LLUICtrl* ctrl, void* userdata);

private:
	LLLineEditor*	mInputLine;
	S32				mPressedButton;
	LLSD			mParameters;
};

//static
HBLuaDialog* HBLuaDialog::create(const std::string& title,
								 const std::string& text,
								 const std::string& suggestion,
								 const std::string& btn1,
								 const std::string& btn2,
								 const std::string& btn3,
								 const std::string& command1,
								 const std::string& command2,
								 const std::string& command3)
{
	LLSD parameters = LLSD::emptyMap();
	parameters["title"] = title;
	parameters["suggestion"] = suggestion;
	if (!text.empty())
	{
		parameters["text"] = text;
	}
	if (!btn1.empty())
	{
		parameters["btn1"] = btn1;
		parameters["command1"] = command1;
	}
	if (!btn2.empty())
	{
		parameters["btn2"] = btn2;
		parameters["command2"] = command2;
	}
	if (!btn3.empty())
	{
		parameters["btn3"] = btn3;
		parameters["command3"] = command3;
	}

	LL_DEBUGS("Lua") << "Creating new Lua dialog with parameters:\n";
	std::stringstream str;
	LLSDSerialize::toPrettyXML(parameters, str);
	LL_CONT << "\n" << str.str() << LL_ENDL;

	return new HBLuaDialog(parameters);
}

HBLuaDialog::HBLuaDialog(const LLSD& parameters)
:	mParameters(parameters),
	mPressedButton(0)
{
	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_lua_dialog.xml");
}

//virtual
HBLuaDialog::~HBLuaDialog()
{
	if (gAutomationp)
	{
		gAutomationp->onLuaDialogClose(mParameters["title"].asString(),
									   mPressedButton, mInputLine->getText());
	}
}

//virtual
bool HBLuaDialog::postBuild()
{
	setTitle(mParameters["title"].asString());

	LLButton* button = getChild<LLButton>("btn1");
	if (mParameters.has("btn1"))
	{
		button->setLabel(mParameters["btn1"].asString());
		button->setCommitCallback(onButton);
		button->setCallbackUserData(this);
	}
	else
	{
		button->setEnabled(false);
		button->setVisible(false);
	}

	button = getChild<LLButton>("btn2");
	if (mParameters.has("btn2"))
	{
		button->setLabel(mParameters["btn2"].asString());
		button->setCommitCallback(onButton);
		button->setCallbackUserData(this);
	}
	else
	{
		button->setEnabled(false);
		button->setVisible(false);
	}

	button = getChild<LLButton>("btn3");
	if (mParameters.has("btn3"))
	{
		button->setLabel(mParameters["btn3"].asString());
		button->setCommitCallback(onButton);
		button->setCallbackUserData(this);
	}
	else
	{
		button->setEnabled(false);
		button->setVisible(false);
	}

	LLTextEditor* textedit = getChild<LLTextEditor>("text");
	textedit->setBorderVisible(false);
	if (mParameters.has("text"))
	{
		std::string text = mParameters["text"].asString();
		textedit->setParseHTML(true);
		textedit->appendColoredText(text, false, false,
									gColors.getColor("TextFgReadOnlyColor"));
	}

	mInputLine = getChild<LLLineEditor>("input");
	std::string suggestion = mParameters["suggestion"].asString();
	if (suggestion == " ")
	{
		mInputLine->setEnabled(false);
		mInputLine->setVisible(false);
	}
	else if (suggestion == "*")
	{
		mInputLine->setDrawAsterixes(true);
	}
	else if (!suggestion.empty())
	{
		mInputLine->setText(suggestion);
	}

	return true;
}

bool HBLuaDialog::evalLuaCommand(const std::string& command)
{
	bool close = false;

	// Setup dialog-specific Lua global variables
	std::string functions = "V_DIALOG_CLOSE=false;V_DIALOG_INPUT=\"";
	std::string text = mInputLine->getText();
	LLStringUtil::replaceString(text, "\"", "\\\"");
	functions += text + "\";";

	// Setup dialog-specific Lua functions using the global variables
	functions += "function DialogClose();V_DIALOG_CLOSE=true;end;";
	functions += "function GetDialogInput();return V_DIALOG_INPUT;end;";
	functions += "function SetDialogInput(text);V_DIALOG_INPUT=text;end;";

	HBViewerAutomation lua;
	lua_State* state = lua.mLuaState;
	if (state && lua.loadString(functions + command))
	{
		// Retreive and interpret the global variables values
		lua_getglobal(state, "V_DIALOG_INPUT");
		text = lua_tostring(state, -1);
		if (mInputLine->getText() != text)
		{
			mInputLine->setText(text);
		}
		lua_getglobal(state, "V_DIALOG_CLOSE");
		close = lua_toboolean(state, -1);
	}

	return close;
}

//static
void HBLuaDialog::onButton(LLUICtrl* ctrl, void* userdata)
{
	HBLuaDialog* self = (HBLuaDialog*)userdata;
	if (!self || !ctrl) return;

	std::string command;

	std::string name = ctrl->getName();
	if (name == "btn1")
	{
		self->mPressedButton = 1;
		command = self->mParameters["command1"].asString();
	}
	else if (name == "btn2")
	{
		self->mPressedButton = 2;
		command = self->mParameters["command2"].asString();
	}
	else if (name == "btn3")
	{
		self->mPressedButton = 3;
		command = self->mParameters["command3"].asString();
	}

	if (!command.empty() && self->evalLuaCommand(command))
	{
		self->close();
	}
	else
	{
		self->mPressedButton = 0;
	}
}

///////////////////////////////////////////////////////////////////////////////
// HBLuaFloater class (custom floaters support for the Lua scripts)
///////////////////////////////////////////////////////////////////////////////

class HBLuaFloater final : public LLFloater
{
protected:
	LOG_CLASS(HBLuaFloater);

public:
	static HBLuaFloater* create(const std::string& name,
								const std::string& parameter,
								const std::string& position, bool open);
	static bool setVisible(const std::string& name, bool show);
	static void destroy(const std::string& name, bool excute_callback);

	static bool setControlCallback(const std::string& floater_name,
								   const std::string& ctrl_name,
								   const std::string& lua_command);

	static bool getControlValue(const std::string& floater_name,
								const std::string& ctrl_name,
								std::string& value);
	static bool getControlValues(const std::string& floater_name,
								 const std::string& ctrl_name,
								 std::vector<std::string>& values);
	static bool setControlValue(const std::string& floater_name,
								const std::string& ctrl_name,
								const std::string& value);

	static bool setControlEnabled(const std::string& floater_name,
								  const std::string& ctrl_name, bool enable);
	static bool setControlVisible(const std::string& floater_name,
								  const std::string& ctrl_name, bool visible);

private:
	// Use the create() method to create the floater.
	HBLuaFloater(const std::string& name, const std::string& parameter);
	~HBLuaFloater() override;

	bool postBuild() override;
	void onOpen() override;
	void onClose(bool app_quitting) override;

	bool evalLuaCommand(const std::string& command, std::string value,
						bool with_close = false);

	static std::string getCtrlValue(LLUICtrl* ctrl);
	static void getCtrlValues(LLUICtrl* ctrl,
							  std::vector<std::string>& values);
	static bool setCtrlValue(LLUICtrl* ctrl, const std::string& value);
	static void onCommitCallback(LLUICtrl* ctrl, void* userdata);
	static void onInventorySelect(LLFolderView* ctrl,
								  bool user_action, void* userdata);

private:
	std::string				mName;
	std::string				mParameter;

	typedef fast_hmap<LLUICtrl*, std::string> commands_map_t;
	commands_map_t			mCommands;

	bool					mInitOK;

	typedef std::map<std::string, HBLuaFloater*> instances_map_t;
	static instances_map_t	sInstances;
};

HBLuaFloater::instances_map_t HBLuaFloater::sInstances;

//static
HBLuaFloater* HBLuaFloater::create(const std::string& name,
								   const std::string& parameter,
								   const std::string& position,
								   bool open)
{
	// Refuse to open two floaters with "dialog" as the name since the
	// corresponding XML file name is already used by our HBLuaDialog class.
	if (name == "dialog")
	{
		llwarns << "The 'dialog' Lua floater name is reserved. Aborted."
				<< llendl;
		return NULL;
	}

	// Sanitize the name to remove forbidden file name characters
	std::string fname = LLDir::getScrubbedFileName(name);

	// Refuse to open two floaters with the same name
	if (sInstances.count(fname))
	{
		llwarns << "Floater '" << fname
				<< "'is already opened, not opening a second instance."
				<< llendl;
		return NULL;
	}

	HBLuaFloater* self = new HBLuaFloater(fname, parameter);

	fname = "floater_lua_" + fname + ".xml";
	if (!LLUICtrlFactory::getInstance()->buildFloater(self, fname, NULL, open))
	{
		self->mInitOK = false;
		delete self;
		return NULL;
	}

	if (position.empty() || position == "center")
	{
		self->center();
		return self;
	}

	const LLRect& view = gFloaterViewp->getRect();
	LLRect r = self->getRect();
	if (position == "top" || position == "top-center")
	{
		r.setLeftTopAndSize((view.getWidth() - r.getWidth()) / 2,
							view.getHeight(), r.getWidth(), r.getHeight());
	}
	else if (position == "bottom" || position == "bottom-center")
	{
		r.setOriginAndSize((view.getWidth() - r.getWidth()) / 2, view.mBottom,
						   r.getWidth(), r.getHeight());
	}
	else if (position == "left" || position == "left-center")
	{
		r.setLeftTopAndSize(0,
							view.getHeight() -
							(view.getHeight() - r.getHeight()) / 2,
							r.getWidth(), r.getHeight());
	}
	else if (position == "right" || position == "right-center")
	{
		r.setLeftTopAndSize(view.getWidth() - r.getWidth(),
							view.getHeight() -
							(view.getHeight() - r.getHeight()) / 2,
							r.getWidth(), r.getHeight());
	}
	else if (position == "top-left")
	{
		r.setLeftTopAndSize(0, view.getHeight(), r.getWidth(), r.getHeight());
	}
	else if (position == "top-right")
	{
		r.setLeftTopAndSize(view.getWidth() - r.getWidth(), view.getHeight(),
							r.getWidth(), r.getHeight());
	}
	else if (position == "bottom-left")
	{
		r.setOriginAndSize(0, view.mBottom, r.getWidth(), r.getHeight());
	}
	else if (position == "bottom-right")
	{
		r.setOriginAndSize(view.getWidth() - r.getWidth(), view.mBottom,
						   r.getWidth(), r.getHeight());
	}
	else
	{
		llwarns << "Unrecognized position parameter '" << position
				<< "' for floater: " << name << llendl;
	}
	self->translateIntoRect(r, false);
	return self;
}

//static
bool HBLuaFloater::setVisible(const std::string& name, bool show)
{
	instances_map_t::iterator it = sInstances.find(name);
	if (it == sInstances.end())
	{
		return false;
	}

	LLFloater* self = (LLFloater*)it->second;
	bool visible = self->getVisible();
	if (show && !visible)
	{
		self->open();
	}
	else if (!show && visible)
	{
		self->setVisible(false);
	}

	return true;
}

//static
void HBLuaFloater::destroy(const std::string& name, bool excute_callback)
{
	instances_map_t::iterator it = sInstances.find(name);
	if (it != sInstances.end())
	{
		HBLuaFloater* self = it->second;
		if (!excute_callback)
		{
			// Do not call the OnLuaFloaterClose() callback:
			self->mInitOK = false;
		}
		self->close();
	}
}

//static
bool HBLuaFloater::setControlCallback(const std::string& floater_name,
									  const std::string& ctrl_name,
									  const std::string& lua_command)
{
	instances_map_t::iterator it = sInstances.find(floater_name);
	if (it == sInstances.end())
	{
		return false;
	}

	HBLuaFloater* self = it->second;

	LLUICtrl* ctrl = self->getChild<LLUICtrl>(ctrl_name.c_str(), true, false);
	if (!ctrl)
	{
		return false;
	}

	self->mCommands[ctrl] = lua_command;

	// For inventory panels, we use a special commit on selection callback
	LLInventoryPanel* inv = dynamic_cast<LLInventoryPanel*>(ctrl);
	if (inv)
	{
		inv->setSelectCallback(onInventorySelect, self);
	}
	else
	{
		// For all other control types, use the LLUICtrl commit callback
		ctrl->setCommitCallback(onCommitCallback);
		ctrl->setCallbackUserData(self);
	}

	// For line and text editors controls, we commit on lost focus

	LLLineEditor* lineedit = dynamic_cast<LLLineEditor*>(ctrl);
	if (lineedit)
	{
		lineedit->setCommitOnFocusLost(true);
		return true;
	}

	LLTextEditor* textedit = dynamic_cast<LLTextEditor*>(ctrl);
	if (textedit)
	{
		textedit->setCommitOnFocusLost(true);
		return true;
	}

	// For scroll list controls (and derived classes such as name list), we
	// commit on selection change
	LLScrollListCtrl* list = dynamic_cast<LLScrollListCtrl*>(ctrl);
	if (list)
	{
		list->setCommitOnSelectionChange(true);
	}

	return true;
}

//static
bool HBLuaFloater::getControlValue(const std::string& floater_name,
								   const std::string& ctrl_name,
								   std::string& value)
{
	instances_map_t::iterator it = sInstances.find(floater_name);
	if (it == sInstances.end())
	{
		return false;
	}

	HBLuaFloater* self = it->second;

	LLUICtrl* ctrl = self->getChild<LLUICtrl>(ctrl_name.c_str(), true, false);
	if (!ctrl)
	{
		return false;
	}

	value.assign(getCtrlValue(ctrl));

	return true;
}

//static
bool HBLuaFloater::getControlValues(const std::string& floater_name,
									const std::string& ctrl_name,
									std::vector<std::string>& values)
{
	instances_map_t::iterator it = sInstances.find(floater_name);
	if (it == sInstances.end())
	{
		return false;
	}

	HBLuaFloater* self = it->second;

	LLUICtrl* ctrl = self->getChild<LLUICtrl>(ctrl_name.c_str(), true, false);
	if (!ctrl)
	{
		return false;
	}

	getCtrlValues(ctrl, values);

	return true;
}

//static
bool HBLuaFloater::setControlValue(const std::string& floater_name,
								   const std::string& ctrl_name,
								   const std::string& value)
{
	instances_map_t::iterator it = sInstances.find(floater_name);
	if (it == sInstances.end())
	{
		return false;
	}

	HBLuaFloater* self = it->second;

	LLUICtrl* ctrl = self->getChild<LLUICtrl>(ctrl_name.c_str(), true, false);
	if (!ctrl)
	{
		return false;
	}

	return setCtrlValue(ctrl, value);
}

//static
bool HBLuaFloater::setControlEnabled(const std::string& floater_name,
									 const std::string& ctrl_name, bool enable)
{
	instances_map_t::iterator it = sInstances.find(floater_name);
	if (it == sInstances.end())
	{
		return false;
	}

	HBLuaFloater* self = it->second;

	LLUICtrl* ctrl = self->getChild<LLUICtrl>(ctrl_name.c_str(), true, false);
	if (!ctrl)
	{
		return false;
	}

	ctrl->setEnabled(enable);

	return true;
}

//static
bool HBLuaFloater::setControlVisible(const std::string& floater_name,
									 const std::string& ctrl_name,
									 bool visible)
{
	instances_map_t::iterator it = sInstances.find(floater_name);
	if (it == sInstances.end())
	{
		return false;
	}

	HBLuaFloater* self = it->second;

	LLUICtrl* ctrl = self->getChild<LLUICtrl>(ctrl_name.c_str(), true, false);
	if (!ctrl)
	{
		return false;
	}

	ctrl->setVisible(visible);

	return true;
}

HBLuaFloater::HBLuaFloater(const std::string& name,
						   const std::string& parameter)
:	mName(name),
	mParameter(parameter),
	mInitOK(false)
{
	sInstances[mName] = this;
}

//virtual
HBLuaFloater::~HBLuaFloater()
{
	sInstances.erase(mName);
}

//virtual
bool HBLuaFloater::postBuild()
{
	std::string name = getTitle();
	LLStringUtil::trimHead(name);
	LLStringUtil::toLower(name);
	if (name.compare(0, 3, "lua") != 0)
	{
		setTitle("Lua: " + getTitle());
	}

	U32 i = 0;
	LLButton* button;
	while ((button = getChild<LLButton>(llformat("button%d", ++i).c_str(),
										true, false)))
	{
		button->setCommitCallback(onCommitCallback);
		button->setCallbackUserData(this);
	}

	i = 0;
	LLCheckBoxCtrl* check;
	while ((check = getChild<LLCheckBoxCtrl>(llformat("check%d", ++i).c_str(),
											 true, false)))
	{
		check->setCommitCallback(onCommitCallback);
		check->setCallbackUserData(this);
	}

	i = 0;
	LLRadioGroup* radio;
	while ((radio = getChild<LLRadioGroup>(llformat("radio%d", ++i).c_str(),
										   true, false)))
	{
		radio->setCommitCallback(onCommitCallback);
		radio->setCallbackUserData(this);
	}

	i = 0;
	LLComboBox* combo;
	while ((combo = getChild<LLComboBox>(llformat("combo%d", ++i).c_str(),
										 true, false)))
	{
		combo->setCommitCallback(onCommitCallback);
		combo->setCallbackUserData(this);
	}

	i = 0;
	LLFlyoutButton* flyout;
	while ((flyout = getChild<LLFlyoutButton>(llformat("flyout%d", ++i).c_str(),
											  true, false)))
	{
		flyout->setCommitCallback(onCommitCallback);
		flyout->setCallbackUserData(this);
	}

	i = 0;
	LLSliderCtrl* slider;
	while ((slider = getChild<LLSliderCtrl>(llformat("slider%d", ++i).c_str(),
											true, false)))
	{
		slider->setCommitCallback(onCommitCallback);
		slider->setCallbackUserData(this);
	}

	i = 0;
	LLSpinCtrl* spin;
	while ((spin = getChild<LLSpinCtrl>(llformat("spin%d", ++i).c_str(),
										true, false)))
	{
		spin->setCommitCallback(onCommitCallback);
		spin->setCallbackUserData(this);
	}

	i = 0;
	LLLineEditor* lineedit;
	while ((lineedit = getChild<LLLineEditor>(llformat("lineedit%d", ++i).c_str(),
											  true, false)))
	{
		lineedit->setCommitCallback(onCommitCallback);
		lineedit->setCallbackUserData(this);
		lineedit->setCommitOnFocusLost(true);
		name = mName + " " + lineedit->getName();
		lineedit->setCustomMenuType(name.c_str());
	}

	i = 0;
	LLTextEditor* textedit;
	while ((textedit = getChild<LLTextEditor>(llformat("textedit%d", ++i).c_str(),
											  true, false)))
	{
		textedit->setCommitCallback(onCommitCallback);
		textedit->setCallbackUserData(this);
		textedit->setCommitOnFocusLost(true);
		name = mName + " " + textedit->getName();
		textedit->setCustomMenuType(name.c_str());
	}

	i = 0;
	LLScrollListCtrl* list;
	while ((list = getChild<LLScrollListCtrl>(llformat("list%d", ++i).c_str(),
											  true, false)))
	{
		list->setCommitCallback(onCommitCallback);
		list->setCallbackUserData(this);
		list->setCommitOnSelectionChange(true);
	}

	i = 0;
	LLScrollListCtrl* namelist;
	while ((namelist = getChild<LLNameListCtrl>(llformat("namelist%d", ++i).c_str(),
												true, false)))
	{
		namelist->setCommitCallback(onCommitCallback);
		namelist->setCallbackUserData(this);
		namelist->setCommitOnSelectionChange(true);
	}

	i = 0;
	LLInventoryPanel* inv;
	while ((inv = getChild<LLInventoryPanel>(llformat("inventory%d", ++i).c_str(),
											 true, false)))
	{
		inv->setSelectCallback(onInventorySelect, this);
	}

	mInitOK = true;

	return true;
}

//virtual
void HBLuaFloater::onOpen()
{
	if (mInitOK && gAutomationp)
	{
		gAutomationp->onLuaFloaterOpen(mName, mParameter);
	}
}

//virtual
void HBLuaFloater::onClose(bool app_quitting)
{
	if (mInitOK && gAutomationp)
	{
		gAutomationp->onLuaFloaterClose(mName, mParameter);
	}
	LLFloater::onClose(app_quitting);	// Calls LLFloater::destroy()
}

bool HBLuaFloater::evalLuaCommand(const std::string& command,
								  std::string value, bool with_close)
{
	bool close = false;

	// Setup floater-specific Lua global variables and functions
	std::string functions = "V_UICTRL_VALUE=\"";
	LLStringUtil::replaceString(value, "\"", "\\\"");
	functions += value + "\";";
	functions += "V_FLOATER_NAME=\"" + mName + "\";";
	value = mParameter;
	LLStringUtil::replaceString(value, "\"", "\\\"");
	functions += "V_FLOATER_PARAM=\"" + value + "\";";
	functions += "function GetValue();return V_UICTRL_VALUE;end;";
	functions += "function GetFloaterName();return V_FLOATER_NAME;end;";
	functions += "function GetFloaterParam();return V_FLOATER_PARAM;end;";
	if (with_close)
	{
		functions += "V_FLOATER_CLOSE=false;";
		functions += "function FloaterClose();V_FLOATER_CLOSE=true;end;";
	}

	HBViewerAutomation lua;
	bool success = lua.loadString(functions + command);
	if (success && with_close)
	{
		lua_State* state = lua.mLuaState;
		if (state)
		{
			// Retreive and interpret the global variable value
			lua_getglobal(state, "V_FLOATER_CLOSE");
			close = lua_toboolean(state, -1);
		}
	}

	return close;
}

//static
std::string HBLuaFloater::getCtrlValue(LLUICtrl* ctrl)
{
	LLInventoryPanel* panel = dynamic_cast<LLInventoryPanel*>(ctrl);
	if (panel)
	{
		ctrl = panel->getRootFolder();
		if (!ctrl) return "";
	}
	LLFolderView* inv = dynamic_cast<LLFolderView*>(ctrl);
	if (inv)
	{
		std::string result;
		const LLFolderView::selected_items_t& items = inv->getSelectedItems();
		LLFolderView::selected_items_t::const_iterator it = items.begin();
		if (it != items.end())
		{
			const LLFolderViewEventListener* listener = (*it)->getListener();
			if (listener)
			{
				result = listener->getUUID().asString();
			}
		}
		return result;
	}

	LLCheckBoxCtrl* check = dynamic_cast<LLCheckBoxCtrl*>(ctrl);
	if (check)
	{
		return check->get() ? "true": "false";
	}

	return ctrl->getValue().asString();
}

//static
void HBLuaFloater::getCtrlValues(LLUICtrl* ctrl,
								 std::vector<std::string>& values)
{
	LLInventoryPanel* panel = dynamic_cast<LLInventoryPanel*>(ctrl);
	if (panel)
	{
		ctrl = panel->getRootFolder();
		if (!ctrl) return;
	}
	LLFolderView* inv = dynamic_cast<LLFolderView*>(ctrl);
	if (inv)
	{
		const LLFolderView::selected_items_t& items = inv->getSelectedItems();
		for (LLFolderView::selected_items_t::const_iterator it = items.begin(),
															end = items.end();
			 it != end; ++it)
		{
			const LLFolderViewEventListener* listener = (*it)->getListener();
			if (listener)
			{
				values.emplace_back(listener->getUUID().asString());
			}
		}
		return;
	}

	// Note: name list controls share this code, LLNameListCtrl being a derived
	// class of LLScrollListCtrl.
	LLScrollListCtrl* list = dynamic_cast<LLScrollListCtrl*>(ctrl);
	if (list)
	{
		std::vector<LLScrollListItem*> items = list->getAllSelected();
		for (S32 i = 0, count = items.size(); i < count; ++i)
		{
			values.emplace_back(items[i]->getValue().asString());
		}
		return;
	}

	LLCheckBoxCtrl* check = dynamic_cast<LLCheckBoxCtrl*>(ctrl);
	if (check)
	{
		values.emplace_back(check->get() ? "true": "false");
		return;
	}

	// Valid for other UI element types.
	values.emplace_back(ctrl->getValue().asString());
}

//static
bool HBLuaFloater::setCtrlValue(LLUICtrl* ctrl, const std::string& value)
{
	// For line and text editors controls, we set their text

	LLLineEditor* lineedit = dynamic_cast<LLLineEditor*>(ctrl);
	if (lineedit)
	{
		lineedit->setText(value);
		return true;
	}

	LLTextEditor* textedit = dynamic_cast<LLTextEditor*>(ctrl);
	if (textedit)
	{
		textedit->setText(value);
		return true;
	}

	// For inventory panels, we set the filter to open a corresponding folder
	// and its descendents only.
	LLInventoryPanel* panel = dynamic_cast<LLInventoryPanel*>(ctrl);
	if (panel)
	{
		LLFolderView* inv = panel->getRootFolder();
		if (!inv) return false;

		bool is_category = false;
		const LLUUID& cat_id =
			HBViewerAutomation::getInventoryObjectId(value, is_category);
		if (!is_category) return false;

		LLInventoryFilter* filter = panel->getFilter();
		if (!filter) return false;

		if (filter->isActive())
		{
			// If our filter is active we may be the first thing requiring a
			// fetch in this folder, so we better start it here.
			LLInventoryModelFetch::getInstance()->start(cat_id);
		}

		// Do not open recursively all sub-folders in the target folder.
		inv->setCanAutoSelect(false);
		// But open all folders on the path from root to the target folder.
		LLFolderViewFolder* folderp =
			dynamic_cast<LLFolderViewFolder*>(inv->getItemByID(cat_id));
		LLFolderViewFolder* rootp = dynamic_cast<LLFolderViewFolder*>(inv);
		if (rootp)	// Paranoia
		{
			while (folderp && folderp != rootp)
			{
				inv->setSelection(folderp, false, false);
				folderp->setOpen(true);
				folderp = folderp->getParentFolder();
			}
		}

		panel->setLastOpenLocked(true);
		panel->setFilterLastOpen(true);
		panel->setFilterShowLinks(true);

		filter->markDefault();
		filter->setLastOpenID(cat_id);
		filter->setModified(LLInventoryFilter::FILTER_RESTART);

		return true;
	}

	// For name lists, we support only simple ones (with just one column), and
	// set the new value by UUID, with <GROUP> as a group tag marker.
	LLNameListCtrl* namelist = dynamic_cast<LLNameListCtrl*>(ctrl);
	if (namelist)
	{
		if (value.empty())
		{
			namelist->clearRows();
			return true;
		}

		bool is_group = false;
		std::string uuid_str = value;
		if (uuid_str.compare(0, 7, "<GROUP>") == 0)
		{
			uuid_str = uuid_str.substr(7);
			is_group = true;
		}
		if (!LLUUID::validate(uuid_str))
		{
			return false;
		}
		if (is_group)
		{
			namelist->addGroupNameItem(LLUUID(uuid_str));
		}
		else
		{
			namelist->addNameItem(LLUUID(uuid_str));
		}
		return true;
	}

	// For scroll lists, the case is more complex... We split the string using
	// the pipe character as a separator, to get the various columns from the
	// 'value' string, and set them as a new line for the list.
	LLScrollListCtrl* list = dynamic_cast<LLScrollListCtrl*>(ctrl);
	if (list)
	{
		if (value.empty())
		{
			list->clearRows();
			return true;
		}

		LLSD element;
		std::vector<std::string> cols;
		boost::split(cols, value, boost::is_any_of("|"));
		std::string col, style;
		for (S32 i = 0, count = cols.size(); i < count; ++i)
		{
			element["columns"][i]["column"] = llformat("col%d", i);
			col = cols[i];

			// Check for font style "<BOLD>" and/or "<ITALIC>" markers
			style.clear();
			if (col.compare(0, 6, "<BOLD>") == 0)
			{
				col = col.substr(6);
				style = "BOLD";
			}
			if (col.compare(0, 8, "<ITALIC>") == 0)
			{
				col = col.substr(8);
				if (!style.empty())
				{
					style += '|';
				}
				style += "ITALIC";
			}
			if (!style.empty())
			{
				element["columns"][i]["font-style"] = style;
			}

			// Check for color <name> or <r,g,b> marker
			if (col.compare(0, 1, "<") == 0)
			{
				size_t j = col.find('>');
				if (j != std::string::npos)
				{
					LLColor4 color;
					if (LLColor4::parseColor(col.substr(1, j - 1), &color))
					{
						col = col.substr(j + 1);
						element["columns"][i]["color"] = color.getValue();
					}
				}
			}

			element["columns"][i]["value"] = col;
		}
		element["id"] = list->getItemCount();
		list->addElement(element, ADD_BOTTOM);
		return true;
	}

	// The other control types get set with a LLSD-converted value (which may
	// not have any effect with some controls, but we report a success anyway).
	ctrl->setValue(LLSD(value));

	return true;
}

//static
void HBLuaFloater::onCommitCallback(LLUICtrl* ctrl, void* userdata)
{
	HBLuaFloater* self = (HBLuaFloater*)userdata;
	if (!self || !ctrl || !self->mInitOK) return;

	bool close = false;

	std::string value = getCtrlValue(ctrl);
	commands_map_t::iterator it = self->mCommands.find(ctrl);
	if (it != self->mCommands.end())
	{
		bool with_close = dynamic_cast<LLButton*>(ctrl) != NULL;
		close = self->evalLuaCommand(it->second, value, with_close);
	}
	if (gAutomationp)
	{
		// We want the name of the parent inventory panel, not the name
		// of the folder view (selected) item...
		std::string ctrl_name;
		LLFolderView* folderp = dynamic_cast<LLFolderView*>(ctrl);
		if (folderp)
		{
			LLPanel* panelp = folderp->getParentPanel();
			if (panelp)
			{
				ctrl_name = panelp->getName();
			}
		}
		else
		{
			ctrl_name = ctrl->getName();
		}
		gAutomationp->onLuaFloaterAction(self->mName, ctrl_name, value);
	}

	if (close)
	{
		self->close();
	}
}

//static
void HBLuaFloater::onInventorySelect(LLFolderView* ctrl,
									 bool, void* userdata)
{
	onCommitCallback(ctrl, userdata);
}

///////////////////////////////////////////////////////////////////////////////
// HBAutomationThread class
///////////////////////////////////////////////////////////////////////////////

// For now, a maximum of eight concurrent threads are permitted.
constexpr S32 MAX_LUA_THREADS = 8;

class HBAutomationThread final : public HBViewerAutomation, public LLThread
{
protected:
	LOG_CLASS(HBAutomationThread);

public:
	LL_INLINE HBAutomationThread()
	:	HBViewerAutomation(true),
		LLThread("Lua thread"),
		mLuaThreadID(++sThreadID),
		mHasSignal(false),
		mRunning(true)
	{
		// Update the LLThread name with our Lua thread Id
		mName = llformat("Lua thread %d", mLuaThreadID);
	}

	// HBViewerAutomation override
	LL_INLINE bool isThreaded() const override		{ return true; }
	LL_INLINE U32 getLuaThreadID() const override	{ return mLuaThreadID; }

	// LLThread overrides
	void run() override;
	LL_INLINE bool runCondition() override			{ return mRunning; }

	LL_INLINE bool isRunning()						{ return mRunning; }

	LL_INLINE void setRunning()
	{
		mRunning = true;
		wake();
	}

	LL_INLINE void setSignal()
	{
		mHasSignal = true;
		wake();
	}

	LL_INLINE void threadStart()
	{
		LLThread::start();
	}

	LL_INLINE void threadStop()
	{
		mWatchdogTimer.start();
		mWatchdogTimer.setTimerExpirySec(0.01f);
		mRunning = true;
		setQuitting();
	}

	LL_INLINE const std::string& getName() const	{ return mName; }

	LL_INLINE bool hasFuncCall() const
	{
		return !mMainFuncCall.empty();
	}

	LL_INLINE const std::string& getFuncCall() const
	{
		return mMainFuncCall;
	}

	LL_INLINE void setFuncCallError(const std::string& err)
	{
		mFuncCallError = err;
	}

	LL_INLINE void appendSignal(const std::string& sig_str)
	{
		mSignals.emplace_back(sig_str);
	}

	int callMainFunction(const std::string& func_name);

	static int getThreadID(lua_State* state);
	static int sleep(lua_State* state);

private:
	// This method is to be called to process pending signals. Returns true
	// (and an unchanged Lua stack) on success or false on failure (with the
	// error message on the Lua stack).
	bool processSignals();

private:
	// Set to the name of the function to call by the thread (*before* setting
	// mRunning to false) to signal it is waiting for that call to a function
	// of the automation script. Reset by the thread (after mRunning is reset
	// to true by the automation script) after the call is completed with the
	// result pushed on the thread stack.
	std::string			mMainFuncCall;
	std::string			mFuncCallError;

	// Filled up by the automation idle loop (while mRunning is false), and
	// consummed in run() (while mRunning is true).
	typedef std::vector<std::string> signals_vec_t;
	signals_vec_t		mSignals;

	U32					mLuaThreadID;

	// NOTE: we cannot use the mPaused variable of LLThread, because is is not
	// protected by a mutex and is not atomic either, while we need to pause
	// (mRunning = false) from inside the thread and un-pause (mRunning = true)
	// from the main loop...
	LLAtomicBool		mRunning;

	// Set by the automation script idle loop when the thread is caught running
	// (mRunning = true) while we have signals for it. Reset in run() by the
	// thread, after it acknowledged it and put itself in pause mode
	// (mRunning = false).
	LLAtomicBool		mHasSignal;

	static U32			sThreadID;
};

//static
U32 HBAutomationThread::sThreadID = 0;

//virtual
void HBAutomationThread::run()
{
	bool loop;
	do
	{
		// At each loop, sleep 1ms and yield to the OS for threads rescheduling
		ms_sleep(1);

		// This will block until runCondition() returns true or the thread
		// leaves the RUNNING state.
		checkPause();
		if (isQuitting() || !mLuaState)
		{
			break;
		}

		if (mHasSignal)
		{
			// Pause and wait for the automation idle loop to send us the
			// pending signal(s)
			mRunning = false;
			mHasSignal = false;  // Acknowledged !
			continue;
		}

		if (!mPrintBuffer.empty())
		{
			// Pause and wait for the automation idle loop to print our stuff
			mRunning = false;
			continue;
		}

		// Process our signals, now...
		if (!processSignals())
		{
			reportError();
			break;
		}

		{
			LL_TRACY_TIMER(TRC_LUA_THREAD_LOOP);

			// Run our main Lua function at each loop
			lua_getglobal(mLuaState, "ThreadRun");
			resetTimer();
			if (lua_pcall(mLuaState, 0, LUA_MULTRET, 0) != LUA_OK)
			{
				reportError();
				break;
			}
			if (lua_gettop(mLuaState) != 1 ||
				lua_type(mLuaState, 1) != LUA_TBOOLEAN)
			{
				lua_pushliteral(mLuaState,
								"ThreadRun() did not return an unique boolean");
				reportError();
				break;
			}
			loop = lua_toboolean(mLuaState, 1);
			lua_pop(mLuaState, 1);
		}
	}
	while (loop);

	llinfos << "Exiting " << mName << llendl;
}

// This method is used to call viewer-specific Lua functions that are not
// thread-safe and must therefore be executed from the main thread (in an idle
// loop callback) on our thread's behalf.
int HBAutomationThread::callMainFunction(const std::string& func_name)
{
	LL_TRACY_TIMER(TRC_LUA_THREAD_CALL_MAIN_FN);

	// Clear any previous error message (since mFuncCallError is also used as a
	// flag do denote an error during the function call by the automation
	// script).
	mFuncCallError.clear();

	// Signal to the automation idle callback that we have work for it by
	// filling up mMainFuncCall with the name of the function to call.
	mMainFuncCall = func_name;

	// Set the thread as "not running" (Lua processing paused), which will
	// allow the automation script idle callback to process our request.
	mRunning = false;

	// Sleep until allowed to resume running by the idle callback, or quitting
	while (!mRunning && !isQuitting())
	{
		// Since the idle callback is called once per frame, there is no use in
		// sleeping less than 5ms (= 1/2 frame at 100fps) at each loop...
		ms_sleep(5);
	}

	// Reset this now that we are done
	mMainFuncCall.clear();

	if (isQuitting())
	{
		std::string err = getName() + " aborted.";
		luaL_error(mLuaState, err.c_str());
	}

	if (!mFuncCallError.empty())
	{
		luaL_error(mLuaState, mFuncCallError.c_str());
	}

	// Return whatever the idle callback left onto our stack.
	return lua_gettop(mLuaState);
}

bool HBAutomationThread::processSignals()
{
	LL_TRACY_TIMER(TRC_LUA_THREAD_PROCESS_SIG);

	if (mSignals.empty())
	{
		// Nothing to do, report a success.
		return true;
	}
	if (!mHasOnSignal)
	{
		// No OnSignal() callback, so no need to bother and report a success.
		mSignals.clear();
		return true;
	}

	// Copy mSignals on stack (and clear it), because OnSignal() could call
	// custom Lua functions that would make us enter the mRunning = false
	// state, which could, in turn, allow the modification of mSignals by the
	// automation script idle loop...
	signals_vec_t signals_copy;
	signals_copy.swap(mSignals);	// This also empties mSignals
	for (U32 s = 0, count = signals_copy.size(); s < count; ++s)
	{
		std::string& signal_str = signals_copy[s];
		LL_DEBUGS("Lua") << "Processing signal: " << signal_str << LL_ENDL;
		// A signal string is always in the following form:
		// from_lua_thread_id;time_stamp_seconds|serialized_Lua_table
		size_t i = signal_str.find("|");
		std::string temp = signal_str.substr(0, i);
		signal_str = "_V_SIGNAL_TABLE=" + signal_str.substr(i + 1);

		lua_getglobal(mLuaState, "OnSignal");

		i = temp.find(";");
		lua_pushnumber(mLuaState, atoi(temp.substr(0, i).c_str()));
		lua_pushnumber(mLuaState, atof(temp.substr(i + 1).c_str()));

		if (luaL_dostring(mLuaState, signal_str.c_str()) == LUA_OK)
		{
			lua_getglobal(mLuaState, "_V_SIGNAL_TABLE");
			lua_pushnil(mLuaState);
			lua_setglobal(mLuaState, "_V_SIGNAL_TABLE");
		}
		else
		{
			lua_pushliteral(mLuaState, "Could not evaluate the signal table");
			return false;
		}

		resetTimer();
		if (lua_pcall(mLuaState, 3, 0, 0) != LUA_OK)
		{
			return false;
		}

		// Check that we did not get killed during the signal processing...
		if (isQuitting())
		{
			lua_pushliteral(mLuaState, "Thread aborted");
			return false;
		}
	}

	if (lua_gettop(mLuaState))
	{
		lua_pushliteral(mLuaState,
						"OnSignal() returned something when it should not !");
		return false;
	}

	return true;
}

//static
int HBAutomationThread::getThreadID(lua_State* state)
{
	HBAutomationThread* self = (HBAutomationThread*)findInstance(state);
	if (!self)
	{
		return 0;
	}

	S32 n = lua_gettop(state);
	if (n)
	{
		luaL_error(state, "%d arguments passed; expected 0.", n);
	}

	lua_pushinteger(state, self->mLuaThreadID);
	return 1;
}

//static
int HBAutomationThread::sleep(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_THREAD_SLEEP);

	HBAutomationThread* self = (HBAutomationThread*)findInstance(state);
	if (!self)
	{
		return 0;
	}

	S32 n = lua_gettop(state);
	if (n != 1)
	{
		luaL_error(state, "%d arguments passed; expected 1.", n);
	}

	S32 sleep_time = lua_tointeger(state, 1);
	lua_pop(state, 1);
	if (sleep_time < 0)
	{
		luaL_error(state, "Invalid (negative) sleep time");
	}

	// Always set ourselves as "not running" to let the automation idle loop
	// send us any pending signal and/or print whatever is in our print buffer.
	self->mRunning = false;
	if (self->mHasSignal)
	{
		self->mHasSignal = false;  // Acknowledged !
	}

	// Now wait for at least our sleep time and until permitted to run again.
	do
	{
		// Sleep by 10ms maximum slices (so that we check often enough for any
		// thread abortion), until we exhaust our sleep time
		if (sleep_time > 0)
		{
			if (sleep_time > 10)
			{
				sleep_time -= 10;
				ms_sleep(10);
			}
			else
			{
				ms_sleep(sleep_time);
				sleep_time = 0;
			}
		}
		else if (!self->mRunning)
		{
			// Sleep some more if not yet allowed to run by the automation
			// idle loop...
			ms_sleep(10);
		}

		// Check for any thread abortion request
		if (self->isQuitting())
		{
			std::string err = self->getName() + " aborted.";
			luaL_error(state, err.c_str());
		}
		// Extend our grace period since we just checked for exit conditions
		self->resetTimer();
	}
	while (sleep_time > 0 || !self->mRunning);

	// Process pending signals, if any.
	if (!self->processSignals())
	{
		// Let's use luaL_error() so to abort the Lua script.
		std::string message(lua_tostring(state, -1));
		luaL_error(state, message.c_str());
	}

	return 0;
}

///////////////////////////////////////////////////////////////////////////////
// HBFriendsStatusObserver helper class
// Used for the automation script to observe friends-related events and call
// the OnFriendsStatus() Lua callback in consequence.
///////////////////////////////////////////////////////////////////////////////
class HBFriendsStatusObserver final : public LLFriendObserver
{
protected:
	LOG_CLASS(HBFriendsStatusObserver);

public:
	HBFriendsStatusObserver()
	:	mMask(0)
	{
		gAvatarTracker.addObserver(this);
	}

	~HBFriendsStatusObserver() override
	{
		gAvatarTracker.removeObserver(this);
	}

	LL_INLINE void changed(U32 mask) override
	{
		mMask = mask;
	}

	void changedBuddies(const uuid_list_t& buddies) override
	{
		if (!gAutomationp)
		{
			return;
		}
		for (uuid_list_t::const_iterator it = buddies.begin(),
										 end = buddies.end();
			 it != end; ++it)
		{
			const LLUUID& id = *it;
			bool online = gAvatarTracker.isBuddyOnline(id);
			gAutomationp->onFriendStatusChange(id, mMask, online);
		}
	}

private:
	U32 mMask;
};

///////////////////////////////////////////////////////////////////////////////
// HBGroupTitlesObserver helper class
// Used by setAgentGroup() to set asynchronously the group title after
// receiving the appropriate data.
///////////////////////////////////////////////////////////////////////////////
class HBGroupTitlesObserver final : public LLGroupMgrObserver
{
protected:
	LOG_CLASS(HBGroupTitlesObserver);

public:
	~HBGroupTitlesObserver() override
	{
		gGroupMgr.removeObserver(this);
		sObservers.erase(mGroupId);
	}

	void changed(LLGroupChange gc) override
	{
		bool success = false;

		LLGroupMgrGroupData* gdatap = gGroupMgr.getGroupData(mGroupId);
		if (gdatap)	// Still a member of this group ?
		{
			if (gc != GC_TITLES)
			{
				return;	// Not interested in this type of changes...
			}

			for (U32 i = 0, count = gdatap->mTitles.size(); i < count; ++i)
			{
				const LLGroupTitle& title = gdatap->mTitles[i];
				if (title.mTitle == mTitleName)
				{
					if (gAgent.setGroup(mGroupId))
					{
						LL_DEBUGS("Lua") << "Setting requested agent group and role ("
										 << mTitleName << ")" << LL_ENDL;
						gGroupMgr.sendGroupTitleUpdate(mGroupId,
													   title.mRoleID);
						success = true;
					}
					break;
				}
			}
		}

		if (!success)
		{
			LL_DEBUGS("Lua") << "Failed to set agent group and role ("
							 << mTitleName << ")" << LL_ENDL;
		}

		// Commit suicide once we are no more needed.
		delete this;
	}

	static HBGroupTitlesObserver* addObserver(const LLUUID& group_id,
											  const std::string& title)
	{
		group_obs_map_t::iterator it = sObservers.find(group_id);

		if (it != sObservers.end())
		{
			// If we already got an observer, just update the desired group
			// title for it...
			it->second->mTitleName = title;
			return it->second;
		}

		// Create a new observer
		return new HBGroupTitlesObserver(group_id, title);
	}

	static void deleteObservers()
	{
		if (!sObservers.empty())
		{
			for (group_obs_map_t::iterator it = sObservers.begin(),
										   end = sObservers.end();
				 it != end; ++it)
			{
				delete it->second;
			}
			sObservers.clear();
		}
	}

private:
	// Use addObserver() to construct instances of this class
	HBGroupTitlesObserver(const LLUUID& group_id, const std::string& title)
	:	LLGroupMgrObserver(group_id),
		mGroupId(group_id),
		mTitleName(title)
	{
		sObservers.emplace(group_id, this);
		gGroupMgr.addObserver(this);
	}

private:
	LLUUID					mGroupId;
	std::string				mTitleName;

	typedef fast_hmap<LLUUID, HBGroupTitlesObserver*> group_obs_map_t;
	static group_obs_map_t	sObservers;
};

HBGroupTitlesObserver::group_obs_map_t HBGroupTitlesObserver::sObservers;

///////////////////////////////////////////////////////////////////////////////
// HBIgnoreCallback helper class to prevent infinite loops in Lua callbacks
///////////////////////////////////////////////////////////////////////////////

class HBIgnoreCallback
{
protected:
	LOG_CLASS(HBIgnoreCallback);

public:
	HBIgnoreCallback(S32 callback_code)
	:	mCallbackCode(callback_code)
	{
		++HBViewerAutomation::sIgnoredCallbacks[callback_code];
	}

	~HBIgnoreCallback()
	{
		if (--HBViewerAutomation::sIgnoredCallbacks[mCallbackCode] < 0)
		{
			llwarns << "Invocations count mismatch for callback: "
					<< mCallbackCode << llendl;
			llassert(false);
			HBViewerAutomation::sIgnoredCallbacks[mCallbackCode] = 0;
		}
	}

private:
	S32 mCallbackCode;
};

///////////////////////////////////////////////////////////////////////////////
// HBViewerAutomation class
///////////////////////////////////////////////////////////////////////////////

//static
HBViewerAutomation::instances_map_t HBViewerAutomation::sInstances;
LLMutex HBViewerAutomation::sThreadsMutex;
HBViewerAutomation::threads_list_t HBViewerAutomation::sThreadsInstances;
HBViewerAutomation::threads_list_t HBViewerAutomation::sDeadThreadsInstances;
HBViewerAutomation::signals_map_t HBViewerAutomation::sThreadsSignals;
std::string HBViewerAutomation::sLastAutomationScriptFile;
uuid_list_t HBViewerAutomation::sMuteObjectRequests;
uuid_list_t HBViewerAutomation::sUnmuteObjectRequests;
S32 HBViewerAutomation::sIgnoredCallbacks[HBViewerAutomation::E_IGN_CB_COUNT];
LLFriendObserver* HBViewerAutomation::sFriendsObserver = NULL;
HBViewerAutomation::pos_history_t HBViewerAutomation::sPositionsHistory;
#if LL_LINUX
LLUUID HBViewerAutomation::sLuaDBusFakeObjectId;
#endif

//static
void HBViewerAutomation::start(std::string file_name)
{
	if (file_name.empty())
	{
		file_name = sLastAutomationScriptFile;
	}
	if (file_name.empty())
	{
		llwarns << "No file name given for automation script. Aborted."
				<< llendl;
		return;
	}
	sLastAutomationScriptFile = file_name;

	if (gAutomationp)
	{
		cleanup();
		llinfos << "Restarting Lua automation..." << llendl;
	}
	else
	{
		llinfos << "Initializing Lua automation..." << llendl;
	}

	gAutomationp = new HBViewerAutomation();
	if (gAutomationp->load(file_name))
	{
		if (gAutomationp->mHasCallbacks)
		{
			for (S32 i = 0; i < E_IGN_CB_COUNT; ++i)
			{
				sIgnoredCallbacks[i] = 0;
			}
			llinfos << "Initialisation successful." << llendl;
			if (LLStartUp::isLoggedIn())
			{
				gAutomationp->onLogin();
			}
			return;
		}
		else
		{
			llinfos << "Lua script executed successfully, no callback found. Closing."
					<< llendl;
		}
	}
	else
	{
		llwarns << "Initialisation failed !" << llendl;
	}
	LLEditMenuHandler::setCustomCallback(NULL);
	delete gAutomationp;
	gAutomationp = NULL;
}

//static
void HBViewerAutomation::cleanup()
{
	if (gAutomationp)
	{
		llinfos << "Stopping Lua automation." << llendl;
		LLEditMenuHandler::setCustomCallback(NULL);
		delete gAutomationp;
		gAutomationp = NULL;
	}

	if (!sDeadThreadsInstances.empty())
	{
		llinfos << "Trying to clean-up dead thread instances..." << llendl;
		for (threads_list_t::iterator it = sDeadThreadsInstances.begin(),
									  end = sDeadThreadsInstances.end();
			 it != end; )
		{
			threads_list_t::iterator curit = it++;
			HBAutomationThread* threadp = curit->second;
			bool stopped = threadp->isStopped();
			for (U32 i = 0; !stopped && i < 10; ++i)
			{
				// It was already set quitting, but this will also wake it up
				threadp->threadStop();
				// Give it some more time...
				ms_sleep(10);
				stopped = threadp->isStopped();
			}
			if (stopped)
			{
				LL_DEBUGS("Lua") << "Deleting stopped thread: "
								 << threadp->getName() << LL_ENDL;
				delete threadp;
				sDeadThreadsInstances.erase(curit);
			}
			else
			{
				llwarns << "Timed out waiting for '" << threadp->getName()
						<< "' to stop" << llendl;
			}
		}
		if (sDeadThreadsInstances.empty())
		{
			llinfos << "All dead threads successfully removed." << llendl;
		}
	}
}

//static
std::string HBViewerAutomation::eval(const std::string& chunk,
									 bool use_print_buffer,
									 const LLUUID& id, const std::string& name)
{
	LL_TRACY_TIMER(TRC_LUA_EVAL);

	if (chunk.empty())
	{
		return "";
	}
	LL_DEBUGS("Lua") << "Executing Lua command line: " << chunk << LL_ENDL;
	HBViewerAutomation self(use_print_buffer);
	if (id.notNull())
	{
		LL_DEBUGS("Lua") << "Originator object: " << name << " (" << id
						 << ")" << LL_ENDL;
		self.mFromObjectId = id;
		self.mFromObjectName = name;
	}
	self.loadString(chunk);
	print(self.mLuaState);
	return self.mPrintBuffer;
}

//static
bool HBViewerAutomation::checkLuaCommand(const std::string& message,
										 const LLUUID& from_object_id,
										 const std::string& from_object_name)
{
	static LLCachedControl<bool> scripts_cmd(gSavedSettings,
											 "LuaAcceptScriptCommands");
	if (!scripts_cmd)
	{
		return false;
	}

	static LLCachedControl<std::string> prefix(gSavedSettings,
											   "LuaScriptCommandPrefix");
	size_t len = std::string(prefix).size();
	if (!len)
	{
		return false;
	}
	if (message.compare(0, len, prefix) != 0)
	{
		return false;
	}

	eval(message.substr(len), false, from_object_id, from_object_name);
	return true;
}

//static
void HBViewerAutomation::execute(const std::string& file_name)
{
	HBViewerAutomation self;

	// Allow a relaxed watchdog timeout for one-shot scripts loaded from files.
	static LLCachedControl<F32> timeout(gSavedSettings,
										"LuaTimeoutForScriptFile");
	self.mWatchdogTimeout = llclamp((F32)timeout, 0.01f, 30.f);

	if (self.load(file_name))
	{
		llinfos << "Lua script '" << file_name << "' executed successfully."
				<< llendl;
	}
	else
	{
		llwarns << "Lua script '" << file_name << "' failed !" << llendl;
	}
}

//static
HBViewerAutomation* HBViewerAutomation::findInstance(lua_State* state)
{
	if (state)
	{
		instances_map_t::iterator it = sInstances.find(state);
		if (it != sInstances.end())
		{
			return it->second;
		}
	}
	return NULL;
}

HBViewerAutomation::HBViewerAutomation(bool use_print_buffer)
:	mUsePrintBuffer(use_print_buffer),
	mPausedWarnings(false),
	mForceWarningsToChat(false),
	mFromObjectName("Lua script"),
	mFromObjectId(gAgentID)
{
	if (isThreaded())
	{
		mUsePrintBuffer = true;
		mWatchdogTimeout = 0.5f;
	}
	else
	{
		static LLCachedControl<F32> lua_timeout(gSavedSettings, "LuaTimeout");
		mWatchdogTimeout = llclamp((F32)lua_timeout, 0.01f, 2.f);
	}
	resetCallbackFlags();

	mLuaState = luaL_newstate();
	if (mLuaState)
	{
		luaL_requiref(mLuaState, "_G", luaopen_base, 1);
		luaL_requiref(mLuaState, LUA_TABLIBNAME, luaopen_table, 1);
		luaL_requiref(mLuaState, LUA_STRLIBNAME, luaopen_string, 1);
		luaL_requiref(mLuaState, LUA_MATHLIBNAME, luaopen_math, 1);
		luaL_requiref(mLuaState, LUA_UTF8LIBNAME, luaopen_utf8, 1);
		lua_settop(mLuaState, 0);
		sInstances[mLuaState] = this;
		LL_DEBUGS("Lua") << "Created new Lua state: " << std::hex << mLuaState
				<< std::dec << LL_ENDL;
	}
	else
	{
		llwarns << "Failure to allocate a new Lua state !" << llendl;
		llassert(false);
	}
}

HBViewerAutomation::~HBViewerAutomation()
{
	if (mHasOnFailedTPSimChange)
	{
		gIdleCallbacks.deleteFunction(onIdleSimChange, this);
	}
	if (mHasOnRegionChange)
	{
		mRegionChangedConnection.disconnect();
	}
	if (mHasOnParcelChange)
	{
		mParcelChangedConnection.disconnect();
	}
	if (mHasOnPositionChange)
	{
		mPositionChangedConnection.disconnect();
	}
	if (mLuaState)
	{
		sInstances.erase(mLuaState);
		lua_settop(mLuaState, 0);
		lua_close(mLuaState);
		mLuaState = NULL;
	}

	if (this != gAutomationp)
	{
		// If we are not the automation script instance, then we are done !
		return;
	}

	if (sFriendsObserver)
	{
		delete sFriendsObserver;
		sFriendsObserver = NULL;
	}

	HBGroupTitlesObserver::deleteObservers();

	// Do not close the opened UI elements if the automation script does not
	// have any callback (it was likely just used to setup those UI elements).
	if (mHasCallbacks)
	{
		if (gLuaSideBarp)
		{
			gLuaSideBarp->removeAllButtons();
		}
		if (gLuaPiep)
		{
			gLuaPiep->removeAllSlices();
		}
		if (gOverlayBarp)
		{
			gOverlayBarp->setLuaFunctionButton("", "", "");
		}
		if (gStatusBarp)
		{
			gStatusBarp->setLuaFunctionButton("", "");
		}
	}

	sThreadsMutex.lock();
	if (!sThreadsInstances.empty())
	{
		gIdleCallbacks.deleteFunction(onIdleThread, this);
		sThreadsSignals.clear();	// Abort any pending signal
		for (threads_list_t::iterator it = sThreadsInstances.begin(),
									  end = sThreadsInstances.end();
			 it != end; ++it)
		{
			HBAutomationThread* threadp = it->second;
			if (!threadp->isStopped())
			{
				threadp->threadStop();
			}
		}
		mWatchdogTimer.start();
		mWatchdogTimer.setTimerExpirySec(0.1f);
		while (!sThreadsInstances.empty() && !mWatchdogTimer.hasExpired())
		{
			for (threads_list_t::iterator it = sThreadsInstances.begin(),
										  end = sThreadsInstances.end();
				it != end; )
			{
				threads_list_t::iterator curit = it++;
				HBAutomationThread* threadp = curit->second;
				if (threadp->isStopped())
				{
					LL_DEBUGS("Lua") << "Deleting stopped thread: "
									 << threadp->getName() << LL_ENDL;
					delete threadp;
					sThreadsInstances.erase(curit);
				}
			}
			ms_sleep(1);
		}
		if (!sThreadsInstances.empty())
		{
			llwarns << "Could not stop all running threads before timeout..."
					<< llendl;
			sDeadThreadsInstances.clear();
			sDeadThreadsInstances.swap(sThreadsInstances);
		}
	}
	sThreadsMutex.unlock();
}


void HBViewerAutomation::resetCallbackFlags()
{
	mHasCallbacks = mHasOnSignal = mHasOnLogin = mHasOnRegionChange =
					mHasOnParcelChange = mHasOnPositionChange =
					mHasOnAveragedFPS = mHasOnAgentOccupationChange =
					mHasOnAgentPush = mHasOnSendChat = mHasOnReceivedChat =
					mHasOnChatTextColoring = mHasOnInstantMsg =
					mHasOnScriptDialog = mHasOnNotification =
					mHasOnFriendStatusChange = mHasOnAvatarRezzing =
					mHasOnAgentBaked = mHasOnRadar = mHasOnRadarSelection =
					mHasOnRadarMark = mHasOnRadarTrack = mHasOnLuaDialogClose =
					mHasOnSideBarVisibilityChange = mHasOnLuaFloaterAction =
					mHasOnLuaFloaterOpen = mHasOnLuaFloaterClose =
					mHasOnAutomationMessage = mHasOnAutomationRequest =
					mHasOnAutoPilotFinished = mHasOnTPStateChange =
					mHasOnFailedTPSimChange = mHasOnWindlightChange =
					mHasOnCameraModeChange = mHasOnJoystickButtons =
					mHasOnLuaPieMenu = mHasOnContextMenu =
				    mHasOnRLVHandleCommand = mHasOnRLVAnswerOnChat =
					mHasOnObjectInfoReply = mHasOnPickInventoryItem =
					mHasOnPickAvatar = false;
}

void HBViewerAutomation::reportError()
{
	if (!mLuaState)	// Paranoia
	{
		return;
	}

	std::string message(lua_tostring(mLuaState, -1));
	lua_settop(mLuaState, 0);	// Sanitize stack by emptying it
	llwarns << "Lua error: " << message << llendl;

	// NOTE: we need verify we have logged in before printing in chat, since
	// we otherwise could crash due to LLFloaterChat not yet being constructed.
	if (mUsePrintBuffer || !LLStartUp::isLoggedIn())
	{
		// Overwrite any existing contents with the error message
		mPrintBuffer = message;
		return;
	}

	LLChat chat;
	chat.mFromName = "Lua";
	chat.mText = "Lua: " + message;
	chat.mSourceType = CHAT_SOURCE_SYSTEM;
	LLFloaterChat::addChat(chat, false, false);
}

//static
void HBViewerAutomation::reportWarning(void* data, const char* msg,
									   int to_continue)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	lua_State* state = (lua_State*)data;
	HBViewerAutomation* self = findInstance(state);
	if (!self || !msg || !*msg)
	{
		return;
	}

	std::string message(msg);

	// Check for Lua warning system control messages: only "@on" and "@off" are
	// standard messages.
	if (message[0] == '@')
	{
		if (message == "@on")
		{
			self->mPausedWarnings = false;
		}
		else if (message == "@off")
		{
			self->mPausedWarnings = true;
		}
		else if (message == "@prefix")
		{
			self->mWarningPrefix.clear();
		}
		else if (message.compare(0, 8, "@prefix:") == 0)
		{
			if (message.size() > 8)
			{
				self->mWarningPrefix = message.substr(8);
				LLStringUtil::trim(self->mWarningPrefix);
			}
			else
			{
				self->mWarningPrefix.clear();
			}
		}
		else if (message == "@tochat")
		{
			self->mForceWarningsToChat = !self->isThreaded();
		}
		else if (message.compare(0, 8, "@tochat:") == 0)
		{
			self->mForceWarningsToChat = !self->isThreaded() &&
										 message != "@tochat:0" &&
										 message != "@tochat:false" &&
										 message != "@tochat:off";
		}
		if (to_continue || self->mPausedWarnings ||
			self->mPendingWarningText.empty())
		{
			return;	// Nothing to print right now.
		}
		message.clear();	// Do not print the control message itself !
	}

	if (to_continue || self->mPausedWarnings)
	{
		self->mPendingWarningText += message;
		return;
	}

	if (!self->mPendingWarningText.empty())
	{
		message = self->mPendingWarningText + message;
		self->mPendingWarningText.clear();
	}
	LL_DEBUGS("Lua") << "Lua warning: " << message << LL_ENDL;

	if (self->mWarningPrefix.empty())
	{
		message = "WARNING: " + message;
	}
	else
	{
		message = self->mWarningPrefix + ": " + message;
	}

	if ((self->mUsePrintBuffer && !self->mForceWarningsToChat) ||
		!LLStartUp::isLoggedIn())
	{
		if (!self->mPrintBuffer.empty())
		{
#if LL_WINDOWS
			self->mPrintBuffer += "\r\n";
#else
			self->mPrintBuffer += '\n';
#endif
		}
		self->mPrintBuffer += message;
		return;
	}

	LLChat chat;
	chat.mFromName = "Lua";
	chat.mText = "Lua: " + message;
	chat.mSourceType = CHAT_SOURCE_SYSTEM;
	LLFloaterChat::addChat(chat, false, false);
}

bool HBViewerAutomation::registerCFunctions()
{
	static const struct luaL_Reg printlib[] = {
  		{ "print", HBViewerAutomation::print },
		{ NULL, NULL }
	};

	if (!mLuaState)	// Paranoia
	{
		return false;
	}

	// Register a warning callback
	lua_setwarnf(mLuaState, reportWarning, mLuaState);

	// This registers our custom print(), overriding Lua's, and disables
	// load(), loadfile() and dofile().
	lua_getglobal(mLuaState, "_G");
	luaL_setfuncs(mLuaState, printlib, 0);
	lua_pushnil(mLuaState);
	lua_setfield(mLuaState, -2, "load");
	lua_pushnil(mLuaState);
	lua_setfield(mLuaState, -2, "loadfile");
	lua_pushnil(mLuaState);
	lua_setfield(mLuaState, -2, "dofile");
	lua_setglobal(mLuaState, "_G");

	// Set some useful global variables so that Lua scripts know what viewer
	// they are running within.
	lua_pushstring(mLuaState, gSecondLife.c_str());
	lua_setglobal(mLuaState, "VIEWER_NAME");
	lua_pushstring(mLuaState, gViewerVersionString.c_str());
	lua_setglobal(mLuaState, "VIEWER_VERSION");
	lua_pushinteger(mLuaState, gViewerVersionNumber);
	lua_setglobal(mLuaState, "VIEWER_VERNUM");

	// We setup Lua so that it calls our watchdog every 500 operations (which
	// should be small enough a number, even on slow computers).
	lua_sethook(mLuaState, watchdog, LUA_MASKCOUNT, 500);

	// Register our custom Lua functions
	lua_register(mLuaState, "GetSourceFileName", getSourceFileName);
	lua_register(mLuaState, "GetWatchdogState", getWatchdogState);
	lua_register(mLuaState, "IsUUID", isUUID);
	lua_register(mLuaState, "IsAvatar", isAvatar);
	lua_register(mLuaState, "IsObject", isObject);
	lua_register(mLuaState, "IsAgentFriend", isAgentFriend);
	lua_register(mLuaState, "IsAgentGroup", isAgentGroup);
	lua_register(mLuaState, "GetAvatarName", getAvatarName);
	lua_register(mLuaState, "GetGroupName", getGroupName);
	lua_register(mLuaState, "IsAdmin", isAdmin);
	lua_register(mLuaState, "GetRadarData", getRadarData);
	lua_register(mLuaState, "SetRadarTracking", setRadarTracking);
	lua_register(mLuaState, "SetRadarToolTip", setRadarToolTip);
	lua_register(mLuaState, "SetRadarMarkChar", setRadarMarkChar);
	lua_register(mLuaState, "SetRadarMarkColor", setRadarMarkColor);
	lua_register(mLuaState, "SetRadarNameColor", setRadarNameColor);
	lua_register(mLuaState, "SetAvatarMinimapColor", setAvatarMinimapColor);
	lua_register(mLuaState, "SetAvatarNameTagColor", setAvatarNameTagColor);
	lua_register(mLuaState, "GetAgentPosHistory", getAgentPosHistory);
	lua_register(mLuaState, "GetAgentInfo", getAgentInfo);
	lua_register(mLuaState, "SetAgentOccupation", setAgentOccupation);
	lua_register(mLuaState, "GetAgentGroupData", getAgentGroupData);
	lua_register(mLuaState, "SetAgentGroup", setAgentGroup);
	lua_register(mLuaState, "AgentGroupInvite", agentGroupInvite);
	lua_register(mLuaState, "AgentSit", agentSit);
	lua_register(mLuaState, "AgentStand", agentStand);
	lua_register(mLuaState, "SetAgentTyping", setAgentTyping);
	lua_register(mLuaState, "SendChat", sendChat);
	lua_register(mLuaState, "GetIMSession", getIMSession);
	lua_register(mLuaState, "SendIM", sendIM);
	lua_register(mLuaState, "ScriptDialogResponse", scriptDialogResponse);
	lua_register(mLuaState, "NotificationResponse", scriptDialogResponse);
	lua_register(mLuaState, "CancelNotification", cancelNotification);
	lua_register(mLuaState, "BrowseToURL", browseToURL);
	lua_register(mLuaState, "DispatchSLURL", dispatchSLURL);
	lua_register(mLuaState, "ExecuteRLV", executeRLV);
	lua_register(mLuaState, "OpenNotification", openNotification);
	lua_register(mLuaState, "OpenFloater", openFloater);
	lua_register(mLuaState, "CloseFloater", closeFloater);
	lua_register(mLuaState, "MakeDialog", makeDialog);
	lua_register(mLuaState, "OpenLuaFloater", openLuaFloater);
	lua_register(mLuaState, "ShowLuaFloater", showLuaFloater);
	lua_register(mLuaState, "SetLuaFloaterCommand", setLuaFloaterCommand);
	lua_register(mLuaState, "GetLuaFloaterValue", getLuaFloaterValue);
	lua_register(mLuaState, "GetLuaFloaterValues", getLuaFloaterValues);
	lua_register(mLuaState, "SetLuaFloaterValue", setLuaFloaterValue);
	lua_register(mLuaState, "SetLuaFloaterEnabled", setLuaFloaterEnabled);
	lua_register(mLuaState, "SetLuaFloaterVisible", setLuaFloaterVisible);
	lua_register(mLuaState, "CloseLuaFloater", closeLuaFloater);
	lua_register(mLuaState, "OverlayBarLuaButton", overlayBarLuaButton);
	lua_register(mLuaState, "StatusBarLuaIcon", statusBarLuaIcon);
	lua_register(mLuaState, "SideBarButton", sideBarButton);
	lua_register(mLuaState, "SideBarButtonToggle", sideBarButtonToggle);
	lua_register(mLuaState, "SideBarHide", sideBarHide);
	lua_register(mLuaState, "SideBarHideOnRightClick",
				 sideBarHideOnRightClick);
	lua_register(mLuaState, "SideBarButtonHide", sideBarButtonHide);
	lua_register(mLuaState, "SideBarButtonDisable", sideBarButtonDisable);
	lua_register(mLuaState, "LuaPieMenuSlice", luaPieMenuSlice);
	lua_register(mLuaState, "LuaContextMenu", luaContextMenu);
	lua_register(mLuaState, "PasteToContextHandler", pasteToContextHandler);
	lua_register(mLuaState, "PlayUISound", playUISound);
	lua_register(mLuaState, "RenderDebugInfo", renderDebugInfo);
	lua_register(mLuaState, "GetDebugSetting", getDebugSetting);
	lua_register(mLuaState, "SetDebugSetting", setDebugSetting);
	lua_register(mLuaState, "GetFrameTimeSeconds", getFrameTimeSeconds);
	lua_register(mLuaState, "GetTimeStamp", getTimeStamp);
	lua_register(mLuaState, "GetClipBoardString", getClipBoardString);
	lua_register(mLuaState, "SetClipBoardString", setClipBoardString);
	lua_register(mLuaState, "FindInventoryObject", findInventoryObject);
	lua_register(mLuaState, "GiveInventory", giveInventory);
	lua_register(mLuaState, "MakeInventoryLink", makeInventoryLink);
	lua_register(mLuaState, "DeleteInventoryLink", deleteInventoryLink);
	lua_register(mLuaState, "NewInventoryFolder", newInventoryFolder);
	lua_register(mLuaState, "ListInventoryFolder", listInventoryFolder);
	lua_register(mLuaState, "GetAgentAttachments", getAgentAttachments);
	lua_register(mLuaState, "GetAgentWearables", getAgentWearables);
	lua_register(mLuaState, "AgentAutoPilotToPos", agentAutoPilotToPos);
	lua_register(mLuaState, "AgentAutoPilotFollow", agentAutoPilotFollow);
	lua_register(mLuaState, "AgentAutoPilotStop", agentAutoPilotStop);
	lua_register(mLuaState, "AgentAutoPilotLoad", agentAutoPilotLoad);
	lua_register(mLuaState, "AgentAutoPilotSave", agentAutoPilotSave);
	lua_register(mLuaState, "AgentAutoPilotRemove", agentAutoPilotRemove);
	lua_register(mLuaState, "AgentAutoPilotRecord", agentAutoPilotRecord);
	lua_register(mLuaState, "AgentAutoPilotReplay", agentAutoPilotReplay);
	lua_register(mLuaState, "AgentRotate", agentRotate);
	lua_register(mLuaState, "GetAgentRotation", getAgentRotation);
	lua_register(mLuaState, "TeleportAgentHome", teleportAgentHome);
	lua_register(mLuaState, "TeleportAgentToPos", teleportAgentToPos);
	lua_register(mLuaState, "GetGridSimAndPos", getGridSimAndPos);
	lua_register(mLuaState, "GetParcelInfo", getParcelInfo);
	lua_register(mLuaState, "GetCameraMode", getCameraMode);
	lua_register(mLuaState, "SetCameraMode", setCameraMode);
	lua_register(mLuaState, "SetCameraFocus", setCameraFocus);
	lua_register(mLuaState, "AddMute", addMute);
	lua_register(mLuaState, "RemoveMute", removeMute);
	lua_register(mLuaState, "IsMuted", isMuted);
	lua_register(mLuaState, "BlockSound", blockSound);
	lua_register(mLuaState, "IsBlockedSound", isBlockedSound);
	lua_register(mLuaState, "GetBlockedSounds", getBlockedSounds);
	lua_register(mLuaState, "DerenderObject", derenderObject);
	lua_register(mLuaState, "GetDerenderedObjects", getDerenderedObjects);
	lua_register(mLuaState, "GetAgentPushes", getAgentPushes);
	lua_register(mLuaState, "ApplyDaySettings", applyDaySettings);
	lua_register(mLuaState, "ApplySkySettings", applySkySettings);
	lua_register(mLuaState, "ApplyWaterSettings", applyWaterSettings);
	lua_register(mLuaState, "SetDayTime", setDayTime);
	lua_register(mLuaState, "GetEESettingsList", getEESettingsList);
	lua_register(mLuaState, "GetWLSettingsList", getWLSettingsList);
	lua_register(mLuaState, "GetEnvironmentStatus", getEnvironmentStatus);
	if (isThreaded())
	{
		lua_register(mLuaState, "GetThreadID",
					 HBAutomationThread::getThreadID);
		lua_register(mLuaState, "Sleep", HBAutomationThread::sleep);
		lua_register(mLuaState, "HasThread", hasThread);
		lua_register(mLuaState, "SendSignal", sendSignal);
		// Also allow CloseIMSession() and GetObjectInfo() in threads
		lua_register(mLuaState, "CloseIMSession", closeIMSession);
		lua_register(mLuaState, "GetObjectInfo", getObjectInfo);
	}
	else if (this == gAutomationp || mFromObjectId == gAgentID)
	{
		lua_register(mLuaState, "AgentPuppetryStart", agentPuppetryStart);
		lua_register(mLuaState, "AgentPuppetryStop", agentPuppetryStop);
		lua_register(mLuaState, "CloseIMSession", closeIMSession);
		lua_register(mLuaState, "GetObjectInfo", getObjectInfo);
		lua_register(mLuaState, "GetGlobalData", getGlobalData);
		lua_register(mLuaState, "SetGlobalData", setGlobalData);
		lua_register(mLuaState, "GetPerAccountData", getPerAccountData);
		lua_register(mLuaState, "SetPerAccountData", setPerAccountData);
		lua_register(mLuaState, "PickAvatar", pickAvatar);
		lua_register(mLuaState, "MoveToInventoryFolder",
					 moveToInventoryFolder);
		lua_register(mLuaState, "PickInventoryItem", pickInventoryItem);
#if HB_LUA_FLOATER_FUNCTIONS
		lua_register(mLuaState, "GetFloaterInstances", getFloaterInstances);
		lua_register(mLuaState, "GetFloaterButtons", getFloaterButtons);
		lua_register(mLuaState, "GetFloaterCheckBoxes", getFloaterCheckBoxes);
		lua_register(mLuaState, "ShowFloater", showFloater);
#endif
		if (this == gAutomationp)
		{
			lua_register(mLuaState, "CallbackAfter", callbackAfter);
			lua_register(mLuaState, "HasThread", hasThread);
			lua_register(mLuaState, "StartThread", startThread);
			lua_register(mLuaState, "StopThread", stopThread);
			lua_register(mLuaState, "SendSignal", sendSignal);
			lua_register(mLuaState, "ForceQuit", forceQuit);
			lua_register(mLuaState, "MinimizeWindow", minimizeWindow);
		}
		else
		{
			lua_register(mLuaState, "AutomationMessage", automationMessage);
			lua_register(mLuaState, "AutomationRequest", automationRequest);
		}
	}
	else
	{
		lua_register(mLuaState, "AutomationMessage", automationMessage);
		lua_register(mLuaState, "AutomationRequest", automationRequest);
	}

	return true;
}

//static
void HBViewerAutomation::preprocessorMessageCB(const std::string& message,
											   bool is_warning, void*)
{
	LLChat chat;
	chat.mFromName = "Lua";
	chat.mText = "Lua preprocessor ";
	chat.mText += is_warning ? "warning: " : "error: ";
	chat.mText += message;
	chat.mSourceType = CHAT_SOURCE_SYSTEM;
	// NOTE: we need verify we have logged in before printing in chat, since
	// we otherwise could crash due to LLFloaterChat not yet being constructed.
	if (LLStartUp::isLoggedIn())
	{
		LLFloaterChat::addChat(chat, false, false);
	}
	else	// Just warn/report error in the log
	{
		llwarns << chat.mText << llendl;
	}
}

//static
S32 HBViewerAutomation::loadInclude(std::string& include_name,
									const std::string& default_path,
									std::string& buffer, void*)
{
	if (include_name.empty() || !gDirUtilp)
	{
		return HBPreprocessor::FAILURE;
	}

	std::string file;
	if (default_path.compare(0, 2, "~/") == 0)
	{
		// Search in user "home" directory, without fallback sub-directory
		file = gDirUtilp->getUserFilename(default_path, "", include_name);
	}
	else
	{
		file = gDirUtilp->getUserFilename(default_path, "include",
										  include_name);
	}
	if (file.empty())
	{
		return HBPreprocessor::FAILURE;
	}

	llifstream include_file(file.c_str());
	if (!include_file.is_open())
	{
		llwarns << "Failure to open file: " << file << llendl;
		return HBPreprocessor::FAILURE;
	}

	// Return the full path of the include file we opened successfully
	include_name = file;

	while (!include_file.eof())
	{
		getline(include_file, file);
		buffer += file + "\n";
	}
	include_file.close();

	return HBPreprocessor::SUCCESS;
}

std::string HBViewerAutomation::preprocess(const std::string& file_name)
{
	llifstream source_file(file_name.c_str());
	if (!source_file.is_open())
	{
		return "";
	}

	bool first_line = true;
	std::string sources, line;
	while (!source_file.eof())
	{
		getline(source_file, line);
		if (first_line && line.compare(0, 4, "\x1bLua") == 0)
		{
			// This is a Lua compiled file: cannot pre-process it !
			source_file.close();
			return "";
		}
		first_line = false;
		sources += line + "\n";
	}

	source_file.close();

	if (!HBPreprocessor::needsPreprocessing(sources))
	{
		// No known preprocesor directive in the file, so nothing to do here !
		return "";
	}

	HBPreprocessor pp(file_name, loadInclude);
	pp.setMessageCallback(preprocessorMessageCB);
	pp.addForbiddenToken("_G");	// This shall not be overridden !
	if (pp.preprocess(sources) != HBPreprocessor::SUCCESS)
	{
		// In case of error return an empty sources string.
		return "";
	}

	return pp.getResult();
}

bool HBViewerAutomation::load(const std::string& file_name)
{
	LL_TRACY_TIMER(TRC_LUA_LOAD);

	resetCallbackFlags();

	if (!mLuaState)
	{
		llwarns << "No Lua state defined. Aborted." << llendl;
		llassert(false);
		return false;
	}

	mSourceFileName = file_name;

	llinfos << "Loading Lua script file: " << file_name << llendl;
	S32 ret = luaL_loadfile(mLuaState, file_name.c_str());
	if (ret == LUA_ERRSYNTAX)
	{
		std::string err(lua_tostring(mLuaState, -1));
		llinfos << "Loading failure, attempting to pre-process the file..."
				<< llendl;
		std::string preprocessed = preprocess(file_name);
		if (preprocessed.empty())
		{
			// Any pre-processing error already got reported via the
			// preprocessorMessageCB() callback. Report the initial Lua error
			// (which is still on stack), in case the file did not need
			// pre-processing anyway and the error was a Lua one.
			reportError();
			return false;
		}
		lua_settop(mLuaState, 0);	// Sanitize stack by emptying it
		llinfos << "Loading pre-processed Lua script..." << llendl;
		if (luaL_loadstring(mLuaState, preprocessed.c_str()) != LUA_OK)
		{
			// Report the two errors we encountered: before and after
			// pre-processing
			err = "Before preprocesing: " + err + "\nAfter preprocessing: ";
			err += lua_tostring(mLuaState, -1);
			lua_pushstring(mLuaState, err.c_str());
			reportError();
			return false;
		}
	}
	else if (ret != LUA_OK)
	{
		reportError();
		return false;
	}

	if (!registerCFunctions())
	{
		return false;
	}

	resetTimer();
	if (lua_pcall(mLuaState, 0, LUA_MULTRET, 0) != LUA_OK)
	{
		reportError();
		return false;
	}

	if (isThreaded())
	{
		if (getGlobal("ThreadRun") != LUA_TFUNCTION)
		{
			lua_pushliteral(mLuaState,
							"Missing ThreadRun() function in thread code");
			reportError();
			return false;
		}
		lua_settop(mLuaState, 0);

		if (gAutomationp)
		{
			// Register the idle callback for our thead
			LL_DEBUGS("Lua") << "Registering thread idle callback." << LL_ENDL;
			gIdleCallbacks.addFunction(onIdleThread, gAutomationp);
		}

		mHasOnSignal = lua_getglobal(mLuaState, "OnSignal");
		lua_pop(mLuaState, 1);
		if (mHasOnSignal)
		{
			llinfos << "OnSignal Lua callback found" << llendl;
		}

		// No other callback for threads...
		return true;
	}

	if (this != gAutomationp)
	{
		return true;
	}

	mHasOnSignal = getGlobal("OnSignal") == LUA_TFUNCTION;
	lua_pop(mLuaState, 1);
	if (mHasOnSignal)
	{
		mHasCallbacks = true;
		llinfos << "OnSignal Lua callback found" << llendl;
	}

	mHasOnLogin = getGlobal("OnLogin") == LUA_TFUNCTION;
	lua_pop(mLuaState, 1);
	if (mHasOnLogin)
	{
		mHasCallbacks = true;
		llinfos << "OnLogin Lua callback found" << llendl;
	}

	mHasOnAveragedFPS = getGlobal("OnAveragedFPS") == LUA_TFUNCTION;
	lua_pop(mLuaState, 1);
	if (mHasOnAveragedFPS)
	{
		mHasCallbacks = true;
		llinfos << "OnAveragedFPS Lua callback found" << llendl;
	}

	mHasOnAgentOccupationChange =
		getGlobal("OnAgentOccupationChange") == LUA_TFUNCTION;
	lua_pop(mLuaState, 1);
	if (mHasOnAgentOccupationChange)
	{
		mHasCallbacks = true;
		llinfos << "OnAgentOccupationChange Lua callback found" << llendl;
	}

	mHasOnAgentPush = getGlobal("OnAgentPush") == LUA_TFUNCTION;
	lua_pop(mLuaState, 1);
	if (mHasOnAgentPush)
	{
		mHasCallbacks = true;
		llinfos << "OnAgentPush Lua callback found" << llendl;
	}

	mHasOnSendChat = getGlobal("OnSendChat") == LUA_TFUNCTION;
	lua_pop(mLuaState, 1);
	if (mHasOnSendChat)
	{
		mHasCallbacks = true;
		llinfos << "OnSendChat Lua callback found" << llendl;
	}

	mHasOnReceivedChat = getGlobal("OnReceivedChat") == LUA_TFUNCTION;
	lua_pop(mLuaState, 1);
	if (mHasOnReceivedChat)
	{
		mHasCallbacks = true;
		llinfos << "OnReceivedChat Lua callback found" << llendl;
	}

	mHasOnChatTextColoring = getGlobal("OnChatTextColoring") == LUA_TFUNCTION;
	lua_pop(mLuaState, 1);
	if (mHasOnChatTextColoring)
	{
		mHasCallbacks = true;
		llinfos << "OnChatTextColoring Lua callback found" << llendl;
	}

	mHasOnInstantMsg = getGlobal("OnInstantMsg") == LUA_TFUNCTION;
	lua_pop(mLuaState, 1);
	if (mHasOnInstantMsg)
	{
		mHasCallbacks = true;
		llinfos << "OnInstantMsg Lua callback found" << llendl;
	}

	mHasOnScriptDialog = getGlobal("OnScriptDialog") == LUA_TFUNCTION;
	lua_pop(mLuaState, 1);
	if (mHasOnScriptDialog)
	{
		mHasCallbacks = true;
		llinfos << "OnScriptDialog Lua callback found" << llendl;
	}

	mHasOnNotification = getGlobal("OnNotification") == LUA_TFUNCTION;
	lua_pop(mLuaState, 1);
	if (mHasOnNotification)
	{
		mHasCallbacks = true;
		llinfos << "OnNotification Lua callback found" << llendl;
	}

	mHasOnFriendStatusChange =
		getGlobal("OnFriendStatusChange") == LUA_TFUNCTION;
	lua_pop(mLuaState, 1);
	if (mHasOnFriendStatusChange)
	{
		mHasCallbacks = true;
		llinfos << "OnFriendStatusChange Lua callback found" << llendl;
		if (!sFriendsObserver)
		{
			sFriendsObserver = new HBFriendsStatusObserver;
		}
	}

	mHasOnAvatarRezzing = getGlobal("OnAvatarRezzing") == LUA_TFUNCTION;
	lua_pop(mLuaState, 1);
	if (mHasOnAvatarRezzing)
	{
		mHasCallbacks = true;
		llinfos << "OnAvatarRezzing Lua callback found" << llendl;
	}

	mHasOnAgentBaked = getGlobal("OnAgentBaked") == LUA_TFUNCTION;
	lua_pop(mLuaState, 1);
	if (mHasOnAgentBaked)
	{
		mHasCallbacks = true;
		llinfos << "OnAgentBaked Lua callback found" << llendl;
	}

	mHasOnRadar = getGlobal("OnRadar") == LUA_TFUNCTION;
	lua_pop(mLuaState, 1);
	if (mHasOnRadar)
	{
		mHasCallbacks = true;
		llinfos << "OnRadar Lua callback found" << llendl;
	}

	mHasOnRadarSelection = getGlobal("OnRadarSelection") == LUA_TFUNCTION;
	lua_pop(mLuaState, 1);
	if (mHasOnRadarSelection)
	{
		mHasCallbacks = true;
		llinfos << "OnRadarSelection Lua callback found" << llendl;
	}

	mHasOnRadarMark = getGlobal("OnRadarMark") == LUA_TFUNCTION;
	lua_pop(mLuaState, 1);
	if (mHasOnRadarMark)
	{
		mHasCallbacks = true;
		llinfos << "OnRadarMark Lua callback found" << llendl;
	}

	mHasOnRadarTrack = getGlobal("OnRadarTrack") == LUA_TFUNCTION;
	lua_pop(mLuaState, 1);
	if (mHasOnRadarTrack)
	{
		mHasCallbacks = true;
		llinfos << "OnRadarTrack Lua callback found" << llendl;
	}

	mHasOnLuaDialogClose = getGlobal("OnLuaDialogClose") == LUA_TFUNCTION;
	lua_pop(mLuaState, 1);
	if (mHasOnLuaDialogClose)
	{
		mHasCallbacks = true;
		llinfos << "OnLuaDialogClose Lua callback found" << llendl;
	}

	mHasOnLuaFloaterAction = getGlobal("OnLuaFloaterAction") == LUA_TFUNCTION;
	lua_pop(mLuaState, 1);
	if (mHasOnLuaFloaterAction)
	{
		mHasCallbacks = true;
		llinfos << "OnLuaFloaterAction Lua callback found" << llendl;
	}

	mHasOnLuaFloaterOpen = getGlobal("OnLuaFloaterOpen") == LUA_TFUNCTION;
	lua_pop(mLuaState, 1);
	if (mHasOnLuaFloaterOpen)
	{
		mHasCallbacks = true;
		llinfos << "OnLuaFloaterOpen Lua callback found" << llendl;
	}

	mHasOnLuaFloaterClose = getGlobal("OnLuaFloaterClose") == LUA_TFUNCTION;
	lua_pop(mLuaState, 1);
	if (mHasOnLuaFloaterClose)
	{
		mHasCallbacks = true;
		llinfos << "OnLuaFloaterClose Lua callback found" << llendl;
	}

	mHasOnSideBarVisibilityChange =
		getGlobal("OnSideBarVisibilityChange") == LUA_TFUNCTION;
	lua_pop(mLuaState, 1);
	if (mHasOnSideBarVisibilityChange)
	{
		mHasCallbacks = true;
		llinfos << "OnSideBarVisibilityChange Lua callback found" << llendl;
	}

	mHasOnAutomationMessage =
		getGlobal("OnAutomationMessage") == LUA_TFUNCTION;
	lua_pop(mLuaState, 1);
	if (mHasOnAutomationMessage)
	{
		mHasCallbacks = true;
		llinfos << "OnAutomationMessage Lua callback found" << llendl;
	}

	mHasOnAutomationRequest =
		getGlobal("OnAutomationRequest") == LUA_TFUNCTION;
	lua_pop(mLuaState, 1);
	if (mHasOnAutomationRequest)
	{
		mHasCallbacks = true;
		llinfos << "OnAutomationRequest Lua callback found" << llendl;
	}

	mHasOnAutoPilotFinished =
		getGlobal("OnAutoPilotFinished") == LUA_TFUNCTION;
	lua_pop(mLuaState, 1);
	if (mHasOnAutoPilotFinished)
	{
		mHasCallbacks = true;
		llinfos << "OnAutoPilotFinished Lua callback found" << llendl;
	}

	mHasOnTPStateChange = getGlobal("OnTPStateChange") == LUA_TFUNCTION;
	lua_pop(mLuaState, 1);
	if (mHasOnTPStateChange)
	{
		mHasCallbacks = true;
		llinfos << "OnTPStateChange Lua callback found" << llendl;
	}

	mHasOnFailedTPSimChange =
		getGlobal("OnFailedTPSimChange") == LUA_TFUNCTION;
	lua_pop(mLuaState, 1);
	if (mHasOnFailedTPSimChange)
	{
		mHasCallbacks = true;
		gIdleCallbacks.addFunction(onIdleSimChange, this);
		llinfos << "OnFailedTPSimChange Lua callback found" << llendl;
	}

	mHasOnRegionChange = getGlobal("OnRegionChange") == LUA_TFUNCTION;
	lua_pop(mLuaState, 1);
	if (mHasOnRegionChange)
	{
		mHasCallbacks = true;
		mRegionChangedConnection =
			gAgent.addRegionChangedCB(boost::bind(&HBViewerAutomation::onRegionChange,
												  this));
		llinfos << "OnRegionChange Lua callback found" << llendl;
	}

	mHasOnParcelChange = getGlobal("OnParcelChange") == LUA_TFUNCTION;
	lua_pop(mLuaState, 1);
	if (mHasOnParcelChange)
	{
		mHasCallbacks = true;
		mParcelChangedConnection =
			gViewerParcelMgr.addAgentParcelChangedCB(boost::bind(&HBViewerAutomation::onParcelChange,
																 this));
		llinfos << "OnParcelChange Lua callback found" << llendl;
	}

	mHasOnPositionChange = getGlobal("OnPositionChange") == LUA_TFUNCTION;
	lua_pop(mLuaState, 1);
	if (mHasOnPositionChange)
	{
		mHasCallbacks = true;
		mPositionChangedConnection =
			gAgent.setPosChangeCallback(boost::bind(&HBViewerAutomation::onPositionChange,
													this, _1, _2));
		llinfos << "OnPositionChange Lua callback found" << llendl;
	}

	mHasOnWindlightChange = getGlobal("OnWindlightChange") == LUA_TFUNCTION;
	lua_pop(mLuaState, 1);
	if (mHasOnWindlightChange)
	{
		mHasCallbacks = true;
		llinfos << "OnWindlightChange Lua callback found" << llendl;
	}

	mHasOnCameraModeChange = getGlobal("OnCameraModeChange") == LUA_TFUNCTION;
	lua_pop(mLuaState, 1);
	if (mHasOnCameraModeChange)
	{
		mHasCallbacks = true;
		llinfos << "OnCameraModeChange Lua callback found" << llendl;
	}

	mHasOnJoystickButtons = getGlobal("OnJoystickButtons") == LUA_TFUNCTION;
	lua_pop(mLuaState, 1);
	if (mHasOnJoystickButtons)
	{
		mHasCallbacks = true;
		llinfos << "OnJoystickButtons Lua callback found" << llendl;
	}

	mHasOnLuaPieMenu = getGlobal("OnLuaPieMenu") == LUA_TFUNCTION;
	lua_pop(mLuaState, 1);
	if (mHasOnLuaPieMenu)
	{
		mHasCallbacks = true;
		llinfos << "OnLuaPieMenu Lua callback found" << llendl;
	}

	mHasOnContextMenu = getGlobal("OnContextMenu") == LUA_TFUNCTION;
	lua_pop(mLuaState, 1);
	if (mHasOnContextMenu)
	{
		mHasCallbacks = true;
		llinfos << "OnContextMenu Lua callback found" << llendl;
		LLEditMenuHandler::setCustomCallback(contextMenuCallback);
	}
	else
	{
		LLEditMenuHandler::setCustomCallback(NULL);
	}

	mHasOnRLVHandleCommand = getGlobal("OnRLVHandleCommand") == LUA_TFUNCTION;
	lua_pop(mLuaState, 1);
	if (mHasOnRLVHandleCommand)
	{
		mHasCallbacks = true;
		llinfos << "OnRLVHandleCommand Lua callback found" << llendl;
	}

	mHasOnRLVAnswerOnChat = getGlobal("OnRLVAnswerOnChat") == LUA_TFUNCTION;
	lua_pop(mLuaState, 1);
	if (mHasOnRLVAnswerOnChat)
	{
		mHasCallbacks = true;
		llinfos << "OnRLVAnswerOnChat Lua callback found" << llendl;
	}

	mHasOnObjectInfoReply = getGlobal("OnObjectInfoReply") == LUA_TFUNCTION;
	lua_pop(mLuaState, 1);
	if (mHasOnObjectInfoReply)
	{
		mHasCallbacks = true;
		llinfos << "OnObjectInfoReply Lua callback found" << llendl;
	}

	mHasOnPickInventoryItem = getGlobal("OnPickInventoryItem") == LUA_TFUNCTION;
	lua_pop(mLuaState, 1);
	if (mHasOnPickInventoryItem)
	{
		mHasCallbacks = true;
		llinfos << "OnPickInventoryItem Lua callback found" << llendl;
	}

	mHasOnPickAvatar = getGlobal("OnPickAvatar") == LUA_TFUNCTION;
	lua_pop(mLuaState, 1);
	if (mHasOnPickAvatar)
	{
		mHasCallbacks = true;
		llinfos << "OnPickAvatar Lua callback found" << llendl;
	}

	return true;
}

bool HBViewerAutomation::loadString(const std::string& chunk)
{
	LL_TRACY_TIMER(TRC_LUA_LOAD_STRING);

	resetCallbackFlags();

	if (!mLuaState)
	{
		llwarns << "No Lua state defined. Aborted." << llendl;
		llassert(false);
		return false;
	}

	if (luaL_loadstring(mLuaState, chunk.c_str()) != LUA_OK)
	{
		reportError();
		return false;
	}

	if (!registerCFunctions())
	{
		return false;
	}

	resetTimer();
	if (lua_pcall(mLuaState, 0, LUA_MULTRET, 0) != LUA_OK)
	{
		reportError();
		return false;
	}

	return true;
}

S32 HBViewerAutomation::getGlobal(const std::string& global)
{
	if (!mLuaState)
	{
		llwarns << "No valid Lua state loaded. Aborted." << llendl;
		llassert(false);
		return LUA_TNONE;
	}

	if (global.empty())
	{
		return LUA_TNONE;
	}

	return lua_getglobal(mLuaState, global.c_str());
}

//static
int HBViewerAutomation::hasThread(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self)
	{
		return 0;
	}

	S32 n = lua_gettop(state);
	if (n != 1)
	{
		luaL_error(state, "%d arguments passed; expected 1.", n);
	}

	S32 thread_id = lua_tointeger(state, 1);
	lua_pop(state, 1);
	if (thread_id < 0)
	{
		luaL_error(state, "Not a valid thread Id: ", thread_id);
	}

	bool has_thread = false;

	if (thread_id)	// 0 = automation script, which is not a thread...
	{
		sThreadsMutex.lock();
		threads_list_t::iterator it = sThreadsInstances.find(thread_id);
		if (it != sThreadsInstances.end())
		{
			has_thread = !it->second->isStopped();
		}
		sThreadsMutex.unlock();
	}

	lua_pushboolean(state, has_thread);

	return 1;
}

//static
int HBViewerAutomation::startThread(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self || self != gAutomationp || !gDirUtilp)
	{
		return 0;
	}

	S32 n = lua_gettop(state);
	if (n != 1 && n != 2)
	{
		luaL_error(state, "%d arguments passed; expected 1 or 2.", n);
	}

	// The first argument is the file name for the thread code.
	std::string fname(luaL_checkstring(state, 1));
	if (fname.empty())
	{
		luaL_error(state, "Empty thread code file name");
	}
	lua_remove(state, 1);

	// When it exists, the second argument must be a "simple" table that we
	// will use for "argv".
	std::string argv;
	if (n > 1)
	{
		if (!serializeTable(state, 1, &argv))
		{
			luaL_error(state, "Unsupported thread argument format");
		}
		argv = "argv=" + argv;
	}

	std::string fpath;
	if (fname.compare(0, 2, "~/") == 0)
	{
		// Search in user "home" directory, without fallback sub-directory
		fpath = gDirUtilp->getUserFilename("~/", "",
										   gDirUtilp->getBaseFileName(fname));
	}
	else
	{
		// Search in the user_settings application directory, with an "include"
		// fallback sub-directory.
		fpath = gDirUtilp->getUserFilename(gDirUtilp->getOSUserAppDir(),
										   "include", fname);
	}
	if (fpath.empty())
	{
		luaL_error(state, "Cannot find file: %s", fname.c_str());
	}

	sThreadsMutex.lock();
	if (sThreadsInstances.size() >= MAX_LUA_THREADS)
	{
		sThreadsMutex.unlock();
		LL_DEBUGS("Lua") << "Too many running threads to start a new one."
						 << LL_ENDL;
		lua_pushboolean(state, false);
		return 1;
	}
	sThreadsMutex.unlock();

	HBAutomationThread* threadp = new HBAutomationThread;
	bool success = threadp->load(fpath);
	if (success && threadp->mLuaState)
	{
		if (!argv.empty())
		{
			if (luaL_dostring(threadp->mLuaState, argv.c_str()))
			{
				success = false;
				llwarns << "Failed to set the thread argv table for thread: "
						<< threadp->getName() << llendl;
			}
		}
	}
	else
	{
		success = false;	// Paranoia, in case threadp->mLuaState == NULL...
		llwarns << "Failed to load the Lua code for thread: "
				<< threadp->getName() << llendl;
	}
	if (success)
	{
		U32 thread_id = threadp->getLuaThreadID();
		sThreadsMutex.lock();
		sThreadsInstances[thread_id] = threadp;
		sThreadsMutex.unlock();
		threadp->threadStart();
		lua_pushnumber(state, thread_id);
	}
	else
	{
		delete threadp;
		lua_pushnil(state);
	}

	return 1;
}

//static
int HBViewerAutomation::stopThread(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self || self != gAutomationp)
	{
		return 0;
	}

	S32 n = lua_gettop(state);
	if (n != 1)
	{
		luaL_error(state, "%d arguments passed; expected 1.", n);
	}

	S32 thread_id = lua_tointeger(state, 1);
	lua_pop(state, 1);
	if (thread_id <= 0)
	{
		luaL_error(state, "Not a valid thread Id: ", thread_id);
	}

	sThreadsMutex.lock();
	threads_list_t::iterator it = sThreadsInstances.find(thread_id);
	if (it == sThreadsInstances.end())
	{
		sThreadsMutex.unlock();
		lua_pushboolean(state, false);
		return 1;
	}
	HBAutomationThread* threadp = it->second;
	LL_DEBUGS("Lua") << "Stopping the running thread: " << threadp->getName()
					 << LL_ENDL;
	threadp->threadStop();
	sThreadsMutex.unlock();

	lua_pushboolean(state, true);
	return 1;
}

//static
int HBViewerAutomation::sendSignal(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self)
	{
		return 0;
	}

	S32 n = lua_gettop(state);
	if (n != 2)
	{
		luaL_error(state, "%d arguments passed; expected 2.", n);
	}

	S32 thread_id = lua_tointeger(state, 1);
	if (thread_id < 0)
	{
		luaL_error(state, "Not a valid thread Id: ", thread_id);
	}
	if ((U32)thread_id == self->getLuaThreadID())
	{
		luaL_error(state, "Cannot send a signal to self !");
	}
	lua_remove(state, 1);

	if (lua_type(state, 1) != LUA_TTABLE)
	{
		luaL_error(state,
				   "Invalid type pased as second argument: table expected");
	}

	// Particular case for sending a signal from a thread to the automation
	// script itself.
	if (thread_id == 0 && self->isThreaded())
	{
		if (!gAutomationp || !gAutomationp->mHasOnSignal)
		{
			lua_pop(state, 1);
			lua_pushboolean(state, false);
			return 1;
		}
		// Push our thread Id on the stack...
		lua_pushnumber(state, self->getLuaThreadID());
		// ... and move it above the table in the stack.
		lua_insert(state, 1);
		// Push the time stamp on stack...
		lua_pushnumber(state, gFrameTimeSeconds);
		// ... and move it above the table in the stack.
		lua_insert(state, 2);
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		// Send the signal to the automation script via callAutomationFunc() by
		// calling its OnSignal() callback instead of reentering this method.
		threadp->callMainFunction("OnSignal");
		lua_pushboolean(state, true);
		return 1;
	}

	std::string signal_str;
	if (!serializeTable(state, 1, &signal_str))
	{
		luaL_error(state, "Unsupported thread signal format");
	}
	signal_str = llformat("%d;%f|", self->getLuaThreadID(),
						  gFrameTimeSeconds) + signal_str;
	LL_DEBUGS("Lua") << "Serialized signal string: " << signal_str << LL_ENDL;

	sThreadsMutex.lock();

	threads_list_t::iterator tit = sThreadsInstances.find(thread_id);
	if (tit == sThreadsInstances.end())
	{
		sThreadsMutex.unlock();
		lua_pushboolean(state, false);
		return 1;
	}
	HBAutomationThread* threadp = tit->second;
	if (!threadp->mHasOnSignal)
	{
		sThreadsMutex.unlock();
		lua_pushboolean(state, false);
		return 1;
	}

	HBThreadSignals* signals;
	signals_map_t::iterator sit = sThreadsSignals.find(threadp);
	if (sit == sThreadsSignals.end())
	{
		LL_DEBUGS("Lua") << "Creating new signal queue for thread: "
						 << thread_id << LL_ENDL;
		signals = new HBThreadSignals;
		signals->mThreadID = thread_id;
		sThreadsSignals[threadp] = signals;
	}
	else
	{
		LL_DEBUGS("Lua") << "Existing signal queue found for thread: "
						 << thread_id << LL_ENDL;
		signals = sit->second;
		if (signals->mThreadID != (U32)thread_id)
		{
			llwarns << "Dead thread signals found, removing them." << llendl;
			signals->mThreadID = thread_id;
			signals->mSignals.clear();
		}
	}
	signals->mSignals.emplace_back(signal_str);

	sThreadsMutex.unlock();

	lua_pushboolean(state, true);
	return 1;
}

//static
bool HBViewerAutomation::callAutomationFunc(HBAutomationThread* threadp)
{
	lua_State* astate = gAutomationp->mLuaState;
	lua_State* tstate = threadp->mLuaState;

	// Get the function name and the corresponding global in the automation
	// script.
	const std::string& function = threadp->getFuncCall();
	lua_getglobal(astate, function.c_str());
	if (lua_type(astate, -1) != LUA_TFUNCTION)
	{
		lua_settop(astate, 0);	// Clear the automation script stack
		threadp->setFuncCallError("No function named '" + function +
								  "' in automation script");
		return false;
	}

	// Process the paramaters present on the thread state stack, copying them
	// onto the automation script state stack...
	S32 n = lua_gettop(tstate);
	for (S32 i = 1; i <= n; ++i)
	{
		switch (lua_type(tstate, i))
		{
			case LUA_TBOOLEAN:
				lua_pushboolean(astate, lua_toboolean(tstate, i));
				break;

			case LUA_TNUMBER:
				lua_pushnumber(astate, lua_tonumber(tstate, i));
				break;

			case LUA_TSTRING:
				lua_pushstring(astate, lua_tostring(tstate, i));
				break;

			case LUA_TNIL:
				lua_pushnil(astate);
				break;

			case LUA_TTABLE:
			{
				std::string table;
				if (serializeTable(tstate, i, &table))
				{
					table = "_V_TABLE_PARAM=" + table;
					if (luaL_dostring(astate, table.c_str()) == LUA_OK)
					{
						lua_getglobal(astate, "_V_TABLE_PARAM");
						lua_pushnil(astate);
						lua_setglobal(astate, "_V_TABLE_PARAM");
						break;
					}
				}
				lua_settop(tstate, 0);	// Clear the thread script stack
				lua_settop(astate, 0);	// Clear the automation script stack
				threadp->setFuncCallError("Failed to copy a table parameter");
				return false;
			}

			default:
			{
				lua_settop(tstate, 0);	// Clear the thread script stack
				lua_settop(astate, 0);	// Clear the automation script stack
				std::string err = "Unsupported parameter type: ";
				err += lua_typename(tstate, i);
				threadp->setFuncCallError(err);
				return false;
			}
		}
	}
	lua_settop(tstate, 0);	// Clear the thread script stack

	gAutomationp->resetTimer();
	if (lua_pcall(astate, n, LUA_MULTRET, 0) != LUA_OK)
	{
		threadp->setFuncCallError(lua_tostring(astate, -1));
		lua_settop(astate, 0);	// Clear the automation script stack
		return false;
	}

	n = lua_gettop(astate);	// Number or returned results
	if (!n)
	{
		return true;	// We are done !
	}

	for (S32 i = 1; i <= n; ++i)
	{
		switch (lua_type(astate, i))
		{
			case LUA_TBOOLEAN:
				lua_pushboolean(tstate, lua_toboolean(astate, i));
				break;

			case LUA_TNUMBER:
				lua_pushnumber(tstate, lua_tonumber(astate, i));
				break;

			case LUA_TSTRING:
				lua_pushstring(tstate, lua_tostring(astate, i));
				break;

			case LUA_TNIL:
				lua_pushnil(tstate);
				break;

			case LUA_TTABLE:
			{
				std::string table;
				if (serializeTable(astate, i, &table))
				{
					table = "_V_RET_TABLE=" + table;
					if (luaL_dostring(tstate, table.c_str()) == LUA_OK)
					{
						lua_getglobal(tstate, "_V_RET_TABLE");
						lua_pushnil(tstate);
						lua_setglobal(tstate, "_V_RET_TABLE");
						break;
					}
				}
				lua_settop(tstate, 0);	// Clear the thread script stack
				lua_settop(astate, 0);	// Clear the automation script stack
				threadp->setFuncCallError("Failed to copy a returned table");
				return false;
			}

			default:
			{
				lua_settop(tstate, 0);	// Clear the thread script stack
				lua_settop(astate, 0);	// Clear the automation script stack
				std::string err = "Unsupported return type: ";
				err += lua_typename(astate, i);
				threadp->setFuncCallError(err);
				return false;
			}
		}
	}
	lua_settop(astate, 0);	// Clear the automation script stack

	return true;
}

//static
void HBViewerAutomation::onIdleThread(void* userdata)
{
	LL_FAST_TIMER(FTM_IDLE_LUA_THREAD);

	HBViewerAutomation* self = (HBViewerAutomation*)userdata;
	if (!self || self != gAutomationp)	// Paranoia
	{
		return;
	}

	// Note: no need to lock sThreadsMutex at this point, since only the
	// automation thread can change sThreadsInstances, either in startThread()
	// or here.
	if (sThreadsInstances.empty())
	{
		LL_DEBUGS("Lua") << "No thread left, unregistering idle callback."
						 << LL_ENDL;
		gIdleCallbacks.deleteFunction(onIdleThread, self);
		sThreadsSignals.clear();	// Clear any signal leftover
		return;
	}

	// This will be used to store pointers to threads waiting for a custom Lua
	// function call.
	std::vector<HBAutomationThread*> waiting_threads;

	for (threads_list_t::iterator it = sThreadsInstances.begin(),
								  end = sThreadsInstances.end();
		 it != end; )
	{
		threads_list_t::iterator curit = it++;
		HBAutomationThread* threadp = curit->second;

		// Only intervene after the thread sets itself to "not running" (i.e.
		// got locked on its run condition, or is executing a sleeping loop) or
		// exited... When it is, it is also safe to use/change its member
		// variables and Lua state.
		// Note: isRunning() usually returns true after the thread is actually
		// stopped (i.e. running loop exited after receiving a threadStop()
		// request)...
		if (threadp->isRunning() && !threadp->isStopped())
		{
			// Check for any pending signals to send to this thread.
			sThreadsMutex.lock();
			if (sThreadsSignals.count(threadp))
			{
				// Let it know that it has got signals and should pause so that
				// we can send them to it !
				threadp->setSignal();
			}
			sThreadsMutex.unlock();
			continue;
		}

		// If the thread print buffer contains something, print it now.
		if (!threadp->mPrintBuffer.empty() && LLStartUp::isLoggedIn())
		{
			LLChat chat;
			chat.mFromName = threadp->getName();
			chat.mText = chat.mFromName + ": " + threadp->mPrintBuffer;
			chat.mSourceType = CHAT_SOURCE_SYSTEM;
			LLFloaterChat::addChat(chat, false, false);
			threadp->mPrintBuffer.clear();
		}

		// If the thread is stopped, remove it.
		if (threadp->isStopped())
		{
			LL_DEBUGS("Lua") << "Thread '" << threadp->getName()
							 << "' stopped, deleting it." << LL_ENDL;
			// Protect sThreadsSignals and sThreadsInstances, in case some
			// other running thread would try and access them (via hasThread()
			// or sendSignal()) while we are deleting this thread.
			sThreadsMutex.lock();
			sThreadsInstances.erase(curit);
			signals_map_t::iterator sit = sThreadsSignals.find(threadp);
			if (sit != sThreadsSignals.end())
			{
				delete sit->second;
				sThreadsSignals.erase(sit);
			}
			sThreadsMutex.unlock();
			delete threadp;
			continue;
		}

		// Check for any pending signals to send to this thread.
		sThreadsMutex.lock();
		signals_map_t::iterator sit = sThreadsSignals.find(threadp);
		if (sit != sThreadsSignals.end())
		{
			HBThreadSignals* signals = sit->second;
			// Current thread Id and Id stored in signals table should match !
			if (signals->mThreadID == threadp->getLuaThreadID())
			{
				// Copy the signal strings in the proper (chronological) order
				// into the thread's own signals vector.
				std::vector<std::string>& sigs_vec = signals->mSignals;
				for (U32 i = 0, count = sigs_vec.size(); i < count; ++i)
				{
					const std::string& sig_str = sigs_vec[i];
					LL_DEBUGS("Lua") << "Copying signal string: " << sig_str
									 << LL_ENDL;
					threadp->appendSignal(sig_str);
				}
			}
			// Stale signals from a dead (crashed ?) thread which old address
			// got reaffected to a new thread (unlikely but possible)...
			else
			{
				llwarns << "Non-matching thread Id " << signals->mThreadID
						<< " found for signals queue associated with thread "
						<< threadp->getLuaThreadID()
						<< ": deleting stale queue." << llendl;
			}
			delete signals;
			sThreadsSignals.erase(sit);
		}
		sThreadsMutex.unlock();

		// If the thread is waiting for an automation script function call, we
		// must perform it on its behalf... But later (see below).
		if (threadp->hasFuncCall())
		{
			waiting_threads.push_back(threadp);
		}
		else
		{
			// Let the thread run again
			threadp->setRunning();
		}
	}

	// Now that we cleaned-up sThreadsInstances and sThreadsSignals, we can
	// proceed with performing our custom Lua function calls on behalf of the
	// waiting threads (since these calls could result in changes to either of
	// sThreadsInstances or sThreadsSignals via callbacks they would trigger in
	// the automation script)...
	for (U32 i = 0, count = waiting_threads.size(); i < count; ++i)
	{
		HBAutomationThread* threadp = waiting_threads[i];
		callAutomationFunc(threadp);
		// We can let this thread run again now
		threadp->setRunning();
	}
}

void HBViewerAutomation::pushGridSimAndPos()
{
	LLViewerRegion* regionp = gAgent.getRegion();
	if (regionp)
	{
		lua_newtable(mLuaState);

		lua_pushliteral(mLuaState, "grid");
		lua_pushstring(mLuaState,
					   LLGridManager::getInstance()->getGridLabel().c_str());
		lua_rawset(mLuaState, -3);

		lua_pushliteral(mLuaState, "region");
		lua_pushstring(mLuaState, regionp->getName().c_str());
		lua_rawset(mLuaState, -3);

		lua_pushliteral(mLuaState, "version");
		lua_pushstring(mLuaState, gLastVersionChannel.c_str());
		lua_rawset(mLuaState, -3);

		lua_pushliteral(mLuaState, "width");
		lua_pushnumber(mLuaState, regionp->getWidth());
		lua_rawset(mLuaState, -3);

		lua_pushliteral(mLuaState, "water_height");
		lua_pushnumber(mLuaState, regionp->getWaterHeight());
		lua_rawset(mLuaState, -3);

		lua_pushliteral(mLuaState, "flags");
		lua_pushinteger(mLuaState, regionp->getRegionFlags());
		lua_rawset(mLuaState, -3);

		std::vector<S32> neighbors;
		regionp->getNeighboringRegionsStatus(neighbors);
		lua_pushliteral(mLuaState, "neighbors");
		lua_pushinteger(mLuaState, neighbors.size());
		lua_rawset(mLuaState, -3);

		const LLVector3d& pos_global = gAgent.getPositionGlobal();
		lua_pushliteral(mLuaState, "global_x");
		lua_pushnumber(mLuaState, pos_global.mdV[VX]);
		lua_rawset(mLuaState, -3);

		lua_pushliteral(mLuaState, "global_y");
		lua_pushnumber(mLuaState, pos_global.mdV[VY]);
		lua_rawset(mLuaState, -3);

		const LLVector3& pos_local = gAgent.getPositionAgent();
		lua_pushliteral(mLuaState, "local_x");
		lua_pushnumber(mLuaState, pos_local.mV[VX]);
		lua_rawset(mLuaState, -3);

		lua_pushliteral(mLuaState, "local_y");
		lua_pushnumber(mLuaState, pos_local.mV[VY]);
		lua_rawset(mLuaState, -3);

		lua_pushliteral(mLuaState, "altitude");
		lua_pushnumber(mLuaState, pos_local.mV[VZ]);
		lua_rawset(mLuaState, -3);

		lua_pushliteral(mLuaState, "navmesh");
		const char* navmesh;
		if (!regionp->hasDynamicPathfinding())
		{
			navmesh = "none";
		}
		else if (gOverlayBarp && gOverlayBarp->isNavmeshDirty())
		{
			navmesh = "dirty";
		}
		else if (gOverlayBarp && gOverlayBarp->isNavmeshRebaking())
		{
			navmesh = "rebaking";
		}
		else if (regionp->dynamicPathfindingEnabled())
		{
			navmesh = "enabled";
		}
		else
		{
			navmesh = "disabled";
		}
		lua_pushstring(mLuaState, navmesh);
		lua_rawset(mLuaState, -3);
	}
	else
	{
		lua_pushnil(mLuaState);
	}
}

void HBViewerAutomation::onLogin()
{
	LL_TRACY_TIMER(TRC_LUA_CALLBACK);

	if (this != gAutomationp)	// Paranoia
	{
		return;
	}

	// Ensure mFromObjectId is properly initialized for gAutomationp which is
	// created on viewer launch while gAgentID was still a null UUID...
	mFromObjectId = gAgentID;

	// Print anything that got printed from the automation script before login.
	if (!mPrintBuffer.empty())
	{
		LLChat chat;
		chat.mFromName = "Lua";
		chat.mSourceType = CHAT_SOURCE_SYSTEM;
		chat.mText = "Lua: " + mPrintBuffer;
		mPrintBuffer.clear();
		LLFloaterChat::addChat(chat, false, false);
	}

	if (!mHasOnLogin || !mLuaState) return;

	LL_DEBUGS("Lua") << "Invoking OnLogin Lua callback." << LL_ENDL;

	lua_getglobal(mLuaState, "OnLogin");
	pushGridSimAndPos();
	lua_pushboolean(mLuaState, gAvatarMovedOnLogin);
	lua_pushboolean(mLuaState, gSavedSettings.getBool("AutoLogin"));
	resetTimer();
	if (lua_pcall(mLuaState, 3, 0, 0) != LUA_OK)
	{
		reportError();
	}
}

void HBViewerAutomation::onRegionChange()
{
	LL_TRACY_TIMER(TRC_LUA_CALLBACK);

	if (!mHasOnRegionChange || !mLuaState)
	{
		return;
	}

	LL_DEBUGS("Lua") << "Invoking OnRegionChange Lua callback." << LL_ENDL;

	lua_getglobal(mLuaState, "OnRegionChange");
	pushGridSimAndPos();
	resetTimer();
	if (lua_pcall(mLuaState, 1, 0, 0) != LUA_OK)
	{
		reportError();
	}
}

void HBViewerAutomation::pushParcelInfo()
{
	LLViewerRegion* region = gAgent.getRegion();
	LLParcel* parcel = gViewerParcelMgr.getAgentParcel();
	if (region && parcel)
	{
		lua_newtable(mLuaState);

		lua_pushliteral(mLuaState, "name");
		lua_pushstring(mLuaState, parcel->getName().c_str());
		lua_rawset(mLuaState, -3);

		lua_pushliteral(mLuaState, "description");
		lua_pushstring(mLuaState, parcel->getDesc().c_str());
		lua_rawset(mLuaState, -3);

		lua_pushliteral(mLuaState, "flags");
		lua_pushinteger(mLuaState, parcel->getParcelFlags());
		lua_rawset(mLuaState, -3);

		lua_pushliteral(mLuaState, "build");
		lua_pushboolean(mLuaState, gViewerParcelMgr.allowAgentBuild());
		lua_rawset(mLuaState, -3);

		lua_pushliteral(mLuaState, "damage");
		lua_pushboolean(mLuaState,
						gViewerParcelMgr.allowAgentDamage(region, parcel));
		lua_rawset(mLuaState, -3);

		lua_pushliteral(mLuaState, "fly");
		lua_pushboolean(mLuaState,
						gViewerParcelMgr.allowAgentFly(region, parcel));
		lua_rawset(mLuaState, -3);

		lua_pushliteral(mLuaState, "push");
		lua_pushboolean(mLuaState,
						gViewerParcelMgr.allowAgentPush(region, parcel));
		lua_rawset(mLuaState, -3);

		lua_pushliteral(mLuaState, "scripts");
		lua_pushboolean(mLuaState,
						gViewerParcelMgr.allowAgentScripts(region, parcel));
		lua_rawset(mLuaState, -3);

		lua_pushliteral(mLuaState, "see");
		lua_pushboolean(mLuaState,
						!parcel->getHaveNewParcelLimitData() ||
						parcel->getSeeAVs());
		lua_rawset(mLuaState, -3);

		lua_pushliteral(mLuaState, "voice");
		lua_pushboolean(mLuaState,
						gIsInSecondLife ? gViewerParcelMgr.allowAgentVoice()
										: parcel->getParcelFlagAllowVoice());
		lua_rawset(mLuaState, -3);
	}
	else
	{
		lua_pushnil(mLuaState);
	}
}

void HBViewerAutomation::onParcelChange()
{
	LL_TRACY_TIMER(TRC_LUA_CALLBACK);

	if (!mHasOnParcelChange || !mLuaState)
	{
		return;
	}

	LL_DEBUGS("Lua") << "Invoking OnParcelChange Lua callback." << LL_ENDL;

	lua_getglobal(mLuaState, "OnParcelChange");
	pushParcelInfo();
	resetTimer();
	if (lua_pcall(mLuaState, 1, 0, 0) != LUA_OK)
	{
		reportError();
	}
}

void HBViewerAutomation::onPositionChange(const LLVector3& pos_local,
										  const LLVector3d& pos_global)
{
	LL_TRACY_TIMER(TRC_LUA_CALLBACK);

	if (!mHasOnPositionChange || !mLuaState)
	{
		return;
	}

	LL_DEBUGS("Lua") << "Invoking OnPositionChange Lua callback." << LL_ENDL;

	lua_getglobal(mLuaState, "OnPositionChange");

	lua_newtable(mLuaState);

	lua_pushliteral(mLuaState, "global_x");
	lua_pushnumber(mLuaState, pos_global.mdV[VX]);
	lua_rawset(mLuaState, -3);

	lua_pushliteral(mLuaState, "global_y");
	lua_pushnumber(mLuaState, pos_global.mdV[VY]);
	lua_rawset(mLuaState, -3);

	lua_pushliteral(mLuaState, "local_x");
	lua_pushnumber(mLuaState, pos_local.mV[VX]);
	lua_rawset(mLuaState, -3);

	lua_pushliteral(mLuaState, "local_y");
	lua_pushnumber(mLuaState, pos_local.mV[VY]);
	lua_rawset(mLuaState, -3);

	lua_pushliteral(mLuaState, "altitude");
	lua_pushnumber(mLuaState, pos_local.mV[VZ]);
	lua_rawset(mLuaState, -3);

	resetTimer();
	if (lua_pcall(mLuaState, 1, 0, 0) != LUA_OK)
	{
		reportError();
	}
}

void HBViewerAutomation::onAveragedFPS(F32 fps, bool limited, F32 frame_time)
{
	LL_TRACY_TIMER(TRC_LUA_CALLBACK);

	if (!mHasOnAveragedFPS || !mLuaState)
	{
		return;
	}

	// Average the frame rates before actually invoking the Lua callback. Note:
	// onAveragedFPS() is called every 200ms or so by the status bar refresh()
	// method, sometimes at a shorter interval whenever the status bar needs an
	// immediate refresh.
	static F32 next_report = 0.f;
	static U32 cumulated_count = 0;
	static F32 cumulative_fps = 0.f;
	static bool has_been_limited = false;
	cumulative_fps += fps;
	++cumulated_count;
	has_been_limited |= limited;
	if (gFrameTimeSeconds < next_report || cumulated_count < 5)
	{
		return;
	}
	fps = cumulative_fps / F32(cumulated_count);
	limited = has_been_limited;
	cumulative_fps = 0.f;
	cumulated_count = 0;
	has_been_limited = false;
	static LLCachedControl<F32> cb_interval(gSavedSettings,
											"LuaOnAveragedFPSInterval");
	next_report = gFrameTimeSeconds + llmax(1.f, F32(cb_interval));

	LL_DEBUGS("Lua") << "Invoking OnAveragedFPS Lua callback. fps="
					 << fps << " - limited=" << (limited ? "true" : "false")
					 << " - frame_render_time= " << frame_time << LL_ENDL;

	lua_getglobal(mLuaState, "OnAveragedFPS");
	lua_pushnumber(mLuaState, fps);
	lua_pushboolean(mLuaState, limited);
	lua_pushnumber(mLuaState, frame_time);
	resetTimer();
	if (lua_pcall(mLuaState, 3, 0, 0) != LUA_OK)
	{
		reportError();
	}
}

void HBViewerAutomation::onAgentOccupationChange(S32 type)
{
	LL_TRACY_TIMER(TRC_LUA_CALLBACK);

	if (!mHasOnAgentOccupationChange || !mLuaState ||
		sIgnoredCallbacks[E_ONAGENTOCCUPATIONCHANGE])
	{
		return;
	}

	LL_DEBUGS("Lua") << "Invoking OnAgentOccupationChange Lua callback. type="
					 << type << LL_ENDL;

	lua_getglobal(mLuaState, "OnAgentOccupationChange");
	lua_pushinteger(mLuaState, type);
	resetTimer();
	if (lua_pcall(mLuaState, 1, 0, 0) != LUA_OK)
	{
		reportError();
	}
}

void HBViewerAutomation::onAgentPush(const LLUUID& id, S32 type, F32 mag)
{
	LL_TRACY_TIMER(TRC_LUA_CALLBACK);

	if (!mHasOnAgentPush || !mLuaState) return;

	LL_DEBUGS("Lua") << "Invoking OnAgentPush Lua callback. id=" << id
					 << " - type=" << type << " - mag=" << mag << LL_ENDL;

	lua_getglobal(mLuaState, "OnAgentPush");
	lua_pushstring(mLuaState, id.asString().c_str());
	lua_pushinteger(mLuaState, type);
	lua_pushnumber(mLuaState, mag);
	resetTimer();
	if (lua_pcall(mLuaState, 3, 0, 0) != LUA_OK)
	{
		reportError();
	}
}

bool HBViewerAutomation::onSendChat(std::string& text)
{
	LL_TRACY_TIMER(TRC_LUA_CALLBACK);

	if (!mHasOnSendChat || !mLuaState || sIgnoredCallbacks[E_ONSENDCHAT])
	{
		return false;
	}

	LL_DEBUGS("Lua") << "Invoking onSendChat Lua callback." << LL_ENDL;

	HBIgnoreCallback lock_on_chat(E_ONSENDCHAT);

	lua_getglobal(mLuaState, "OnSendChat");
	lua_pushstring(mLuaState, text.c_str());
	resetTimer();
	if (lua_pcall(mLuaState, 1, 1, 0) != LUA_OK)
	{
		reportError();
		return false;
	}

	if (lua_gettop(mLuaState) == 0 ||
		lua_type(mLuaState, -1) != LUA_TSTRING)
	{
		lua_pushliteral(mLuaState,
						"OnSendChat() Lua callback did not return a string");
		reportError();
		return false;
	}

	std::string new_text(lua_tolstring(mLuaState, -1, NULL));
	lua_pop(mLuaState, 1);
	if (new_text != text)
	{
		text = new_text;
		return true;
	}

	return false;
}

void HBViewerAutomation::onReceivedChat(U8 chat_type, const LLUUID& from_id,
										const std::string& name,
										const std::string& text)
{
	LL_TRACY_TIMER(TRC_LUA_CALLBACK);

	if (!mHasOnReceivedChat || !mLuaState) return;

	LL_DEBUGS("Lua") << "Invoking OnReceivedChat Lua callback. chat_type="
					 << chat_type << " - from_id=" << from_id << " - name="
					 << name << LL_ENDL;

	lua_getglobal(mLuaState, "OnReceivedChat");
	lua_pushinteger(mLuaState, chat_type);
	lua_pushstring(mLuaState, from_id.asString().c_str());
	lua_pushboolean(mLuaState, gObjectList.findAvatar(from_id) != NULL);
	lua_pushstring(mLuaState, name.c_str());
	lua_pushstring(mLuaState, text.c_str());
	resetTimer();
	if (lua_pcall(mLuaState, 5, 0, 0) != LUA_OK)
	{
		reportError();
	}
}

bool HBViewerAutomation::onChatTextColoring(const LLUUID& from_id,
											const std::string& name,
											const std::string& text,
											LLColor4& color)
{
	LL_TRACY_TIMER(TRC_LUA_CALLBACK);

	if (!mHasOnChatTextColoring || !mLuaState) return false;

	LL_DEBUGS("Lua") << "Invoking OnChatTextColoring Lua callback. name="
					 << name << LL_ENDL;

	lua_getglobal(mLuaState, "OnChatTextColoring");
	lua_pushstring(mLuaState, from_id.asString().c_str());
	lua_pushstring(mLuaState, name.c_str());
	lua_pushstring(mLuaState, text.c_str());
	resetTimer();
	if (lua_pcall(mLuaState, 3, 1, 0) != LUA_OK)
	{
		reportError();
	}

	if (lua_gettop(mLuaState) == 0 ||
		lua_type(mLuaState, -1) != LUA_TSTRING)
	{
		lua_pushliteral(mLuaState,
						"OnChatTextColoring() Lua callback did not return a string");
		reportError();
		return false;
	}

	std::string color_str(lua_tolstring(mLuaState, -1, NULL));
	lua_pop(mLuaState, 1);
	if (color_str.empty())
	{
		return false;
	}

	if (!LLColor4::parseColor(color_str, &color))
	{
		lua_pushliteral(mLuaState,
						"OnChatTextColoring() Lua returned an invalid color");
		reportError();
		return false;
	}

	return true;
}

void HBViewerAutomation::onInstantMsg(const LLUUID& session_id,
									  const LLUUID& origin_id,
									  const std::string& name,
									  const std::string& text)
{
	LL_TRACY_TIMER(TRC_LUA_CALLBACK);

	if (!mHasOnInstantMsg || !mLuaState || sIgnoredCallbacks[E_ONINSTANTMSG])
	{
		return;
	}

	// See LLIMMgr::computeSessionID() for the session Id computation rules
	S32 type;
	LLUUID other_participant_id = session_id;
	if (session_id == gAgentID || (session_id ^ origin_id) == gAgentID)
	{
		LL_DEBUGS("Lua") << "Peer to peer session detected." << LL_ENDL;
		other_participant_id = origin_id;
		type = 0;
	}
	else if (gAgent.isInGroup(session_id, true))
	{
		LL_DEBUGS("Lua") << "Group session detected." << LL_ENDL;
		type = 1;
	}
	else
	{
		LL_DEBUGS("Lua") << "Conference session assumed." << LL_ENDL;
		type = 2;
	}

	LL_DEBUGS("Lua") << "Invoking OnInstantMsg Lua callback. session_id="
					 << session_id << " - other_participant_id="
					 << other_participant_id << " - type=" << type
					 << " - name=" << name << LL_ENDL;
	HBIgnoreCallback lock_on_im(E_ONINSTANTMSG);
	lua_getglobal(mLuaState, "OnInstantMsg");
	lua_pushstring(mLuaState, session_id.asString().c_str());
	lua_pushstring(mLuaState, other_participant_id.asString().c_str());
	lua_pushinteger(mLuaState, type);
	lua_pushstring(mLuaState, name.c_str());
	lua_pushstring(mLuaState, text.c_str());
	resetTimer();
	if (lua_pcall(mLuaState, 5, 0, 0) != LUA_OK)
	{
		reportError();
	}
}

void HBViewerAutomation::onScriptDialog(const LLUUID& notif_id,
										const std::string& message,
										const std::vector<std::string>& buttons)
{
	LL_TRACY_TIMER(TRC_LUA_CALLBACK);

	if (!mHasOnScriptDialog || !mLuaState) return;

	LL_DEBUGS("Lua") << "Invoking OnScriptDialog Lua callback. notif_id="
					 << notif_id << LL_ENDL;

	lua_getglobal(mLuaState, "OnScriptDialog");

	lua_pushstring(mLuaState, notif_id.asString().c_str());
	lua_pushstring(mLuaState, message.c_str());

	lua_newtable(mLuaState);
	for (S32 i = 0, count = buttons.size(); i < count; ++i)
	{
		lua_pushstring(mLuaState, llformat("button%d", i + 1).c_str());
		lua_pushstring(mLuaState, buttons[i].c_str());
		lua_rawset(mLuaState, -3);
	}

	resetTimer();
	if (lua_pcall(mLuaState, 3, 0, 0) != LUA_OK)
	{
		reportError();
	}
}

void HBViewerAutomation::onNotification(const std::string& dialog_name,
										const LLUUID& notif_id,
										const std::string& message)
{
	LL_TRACY_TIMER(TRC_LUA_CALLBACK);

	if (!mHasOnNotification || !mLuaState ||
		dialog_name == "LuaNotifyTip" ||
#if 0	// For now, onNotification() is not called for alert boxes
		dialog_name == "LuaAlert" ||
#endif
		dialog_name == "LuaNotification")
	{
		return;
	}

	LL_DEBUGS("Lua") << "Invoking OnNotification Lua callback. dialog_name="
					 << dialog_name << " - notif_id=" << notif_id << LL_ENDL;

	lua_getglobal(mLuaState, "OnNotification");

	lua_pushstring(mLuaState, dialog_name.c_str());
	lua_pushstring(mLuaState, notif_id.asString().c_str());
	lua_pushstring(mLuaState, message.c_str());

	resetTimer();
	if (lua_pcall(mLuaState, 3, 0, 0) != LUA_OK)
	{
		reportError();
	}
}

void HBViewerAutomation::onFriendStatusChange(const LLUUID& id, U32 mask,
											  bool is_online)
{
	LL_TRACY_TIMER(TRC_LUA_CALLBACK);

	if (!mHasOnFriendStatusChange || !mLuaState) return;

	LL_DEBUGS("Lua") << "Invoking OnFriendStatusChange Lua callback. id="
					 << id << " - mask=" << mask << " - is_online="
					 << (is_online ? " true" : "false") << LL_ENDL;

	lua_getglobal(mLuaState, "OnFriendStatusChange");
	lua_pushstring(mLuaState, id.asString().c_str());
	lua_pushinteger(mLuaState, mask);
	lua_pushboolean(mLuaState, is_online);
	resetTimer();
	if (lua_pcall(mLuaState, 3, 0, 0) != LUA_OK)
	{
		reportError();
	}
}

void HBViewerAutomation::onAvatarRezzing(const LLUUID& id)
{
	LL_TRACY_TIMER(TRC_LUA_CALLBACK);

	if (!mHasOnAvatarRezzing || !mLuaState) return;

	LL_DEBUGS("Lua") << "Invoking OnAvatarRezzing Lua callback. id="
					 << id << LL_ENDL;

	lua_getglobal(mLuaState, "OnAvatarRezzing");
	lua_pushstring(mLuaState, id.asString().c_str());
	resetTimer();
	if (lua_pcall(mLuaState, 1, 0, 0) != LUA_OK)
	{
		reportError();
	}
}

void HBViewerAutomation::onAgentBaked()
{
	LL_TRACY_TIMER(TRC_LUA_CALLBACK);

	if (!mHasOnAgentBaked || !mLuaState || !isAgentAvatarValid()) return;

	if (!gAgent.isGodlikeWithoutAdminMenuFakery() &&
		!enable_avatar_textures(NULL))
	{
		return;
	}

	LL_DEBUGS("Lua") << "Queuing OnAgentBaked Lua callback." << LL_ENDL;

	// We use a callback with a 2 seconds delay, because we may otherwise
	// encounter race conditions between baking, messaging (in OpenSIM, with
	// legacy UDP messages), and the actual availability of the baked textures.
	doAfterInterval(boost::bind(&doCallOnAgentBaked, mLuaState), 2.f);
}

//static
void HBViewerAutomation::doCallOnAgentBaked(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_CALLBACK);

	HBViewerAutomation* self = findInstance(state);
	if (!self || !self->mHasOnAgentBaked || !isAgentAvatarValid()) return;

	// Double check...
	if (!gAgent.isGodlikeWithoutAdminMenuFakery() &&
		 !enable_avatar_textures(NULL))
	{
		return;
	}

	LL_DEBUGS("Lua") << "Invoking OnAgentBaked Lua callback." << LL_ENDL;

	lua_getglobal(state, "OnAgentBaked");

	lua_newtable(state);
	std::string te_name;
	uuid_vec_t ids;
	for (S32 i = 0, count = gAgentAvatarp->getNumTEs(); i < count; ++i)
	{
		LLFloaterAvatarTextures::getTextureIds(gAgentAvatarp, ETextureIndex(i),
											   te_name, ids);
		const LLUUID& id = ids[0];
		if (id != IMG_DEFAULT_AVATAR &&
			te_name.rfind("-baked") != std::string::npos)
		{
			lua_pushstring(state, te_name.c_str());
			lua_pushstring(state, id.asString().c_str());
			lua_rawset(state, -3);
		}
	}

	self->resetTimer();
	if (lua_pcall(state, 1, 0, 0) != LUA_OK)
	{
		self->reportError();
	}
}

void HBViewerAutomation::onRadar(const LLUUID& id, const std::string& name,
								 S32 range, bool marked)
{
	LL_TRACY_TIMER(TRC_LUA_CALLBACK);

	if (!mHasOnRadar || !mLuaState) return;

	LL_DEBUGS("Lua") << "Invoking OnRadar Lua callback. id=" << id
					 << " - name=" << name << " - range=" << range
					  << " - marked=" << marked << LL_ENDL;

	lua_getglobal(mLuaState, "OnRadar");
	lua_pushstring(mLuaState, id.asString().c_str());
	lua_pushstring(mLuaState, name.c_str());
	lua_pushinteger(mLuaState, range);
	lua_pushboolean(mLuaState, marked);
	resetTimer();
	if (lua_pcall(mLuaState, 4, 0, 0) != LUA_OK)
	{
		reportError();
	}
}

void HBViewerAutomation::onRadarSelection(const uuid_vec_t& ids)
{
	LL_TRACY_TIMER(TRC_LUA_CALLBACK);

	if (!mHasOnRadarSelection || !mLuaState || ids.empty()) return;

	S32 count = ids.size();
	LL_DEBUGS("Lua") << "Invoking OnRadarSelection Lua callback with " << count
					 << " selected radar entries." << LL_ENDL;

	lua_getglobal(mLuaState, "OnRadarSelection");

	lua_newtable(mLuaState);
	for (S32 i = 0; i < count; ++i)
	{
		lua_pushstring(mLuaState, ids[i].asString().c_str());
		lua_rawseti(mLuaState, -2, i + 1);
	}

	resetTimer();
	if (lua_pcall(mLuaState, 1, 0, 0) != LUA_OK)
	{
		reportError();
	}
}

void HBViewerAutomation::onRadarMark(const LLUUID& id, const std::string& name,
									 bool marked)
{
	LL_TRACY_TIMER(TRC_LUA_CALLBACK);

	if (!mHasOnRadarMark || !mLuaState) return;

	LL_DEBUGS("Lua") << "Invoking OnRadarMark Lua callback. avid=" << id
					 << " - name=" << name << " - marked="
					 << (marked ? "true" : "false") << LL_ENDL;

	lua_getglobal(mLuaState, "OnRadarMark");
	lua_pushstring(mLuaState, id.asString().c_str());
	lua_pushstring(mLuaState, name.c_str());
	lua_pushboolean(mLuaState, marked);
	resetTimer();
	if (lua_pcall(mLuaState, 3, 0, 0) != LUA_OK)
	{
		reportError();
	}
}

void HBViewerAutomation::onRadarTrack(const LLUUID& id,
									  const std::string& name, bool tracked)
{
	LL_TRACY_TIMER(TRC_LUA_CALLBACK);

	if (!mHasOnRadarTrack || !mLuaState || sIgnoredCallbacks[E_ONRADARTRACK])
	{
		return;
	}

	LL_DEBUGS("Lua") << "Invoking OnRadarTrack Lua callback. avid=" << id
					 << " - name=" << name << " - tracking="
					 << (tracked ? "true" : "false") << LL_ENDL;

	lua_getglobal(mLuaState, "OnRadarTrack");
	lua_pushstring(mLuaState, id.asString().c_str());
	lua_pushstring(mLuaState, name.c_str());
	lua_pushboolean(mLuaState, tracked);
	resetTimer();
	if (lua_pcall(mLuaState, 3, 0, 0) != LUA_OK)
	{
		reportError();
	}
}

void HBViewerAutomation::onLuaDialogClose(const std::string& title, S32 button,
										  const std::string& text)
{
	LL_TRACY_TIMER(TRC_LUA_CALLBACK);

	if (!mHasOnLuaDialogClose || !mLuaState) return;

	LL_DEBUGS("Lua") << "Invoking OnLuaDialogClose Lua callback. button="
					 << button << " - text=" << text << LL_ENDL;

	lua_getglobal(mLuaState, "OnLuaDialogClose");
	lua_pushstring(mLuaState, title.c_str());
	lua_pushinteger(mLuaState, button);
	lua_pushstring(mLuaState, text.c_str());
	resetTimer();
	if (lua_pcall(mLuaState, 3, 0, 0) != LUA_OK)
	{
		reportError();
	}
}

void HBViewerAutomation::onLuaFloaterAction(const std::string& floater_name,
											const std::string& ctrl_name,
											const std::string& value)
{
	LL_TRACY_TIMER(TRC_LUA_CALLBACK);

	if (!mHasOnLuaFloaterAction || !mLuaState) return;

	LL_DEBUGS("Lua") << "Invoking OnLuaFloaterAction Lua callback. Floater: "
					 << floater_name << " - Control: " << ctrl_name
					 << " - Value: " << value << LL_ENDL;

	lua_getglobal(mLuaState, "OnLuaFloaterAction");
	lua_pushstring(mLuaState, floater_name.c_str());
	lua_pushstring(mLuaState, ctrl_name.c_str());
	lua_pushstring(mLuaState, value.c_str());
	resetTimer();
	if (lua_pcall(mLuaState, 3, 0, 0) != LUA_OK)
	{
		reportError();
	}
}

void HBViewerAutomation::onLuaFloaterOpen(const std::string& floater_name,
										  const std::string& parameter)
{
	LL_TRACY_TIMER(TRC_LUA_CALLBACK);

	if (!mHasOnLuaFloaterOpen || !mLuaState) return;

	LL_DEBUGS("Lua") << "Invoking OnLuaFloaterOpen Lua callback. Floater: "
					 << floater_name << LL_ENDL;

	lua_getglobal(mLuaState, "OnLuaFloaterOpen");
	lua_pushstring(mLuaState, floater_name.c_str());
	lua_pushstring(mLuaState, parameter.c_str());
	resetTimer();
	if (lua_pcall(mLuaState, 2, 0, 0) != LUA_OK)
	{
		reportError();
	}
}

void HBViewerAutomation::onLuaFloaterClose(const std::string& floater_name,
										   const std::string& parameter)
{
	LL_TRACY_TIMER(TRC_LUA_CALLBACK);

	if (!mHasOnLuaFloaterClose || !mLuaState) return;

	LL_DEBUGS("Lua") << "Invoking OnLuaFloaterClose Lua callback. Floater: "
					 << floater_name << LL_ENDL;

	lua_getglobal(mLuaState, "OnLuaFloaterClose");
	lua_pushstring(mLuaState, floater_name.c_str());
	lua_pushstring(mLuaState, parameter.c_str());
	resetTimer();
	if (lua_pcall(mLuaState, 2, 0, 0) != LUA_OK)
	{
		reportError();
	}
}

void HBViewerAutomation::onSideBarVisibilityChange(bool visible)
{
	LL_TRACY_TIMER(TRC_LUA_CALLBACK);

	if (!mHasOnSideBarVisibilityChange || !mLuaState) return;

	LL_DEBUGS("Lua") << "Invoking OnSideBarVisibilityChange Lua callback. visible="
					 << (visible ? "true" : "false") << LL_ENDL;

	lua_getglobal(mLuaState, "OnSideBarVisibilityChange");
	lua_pushboolean(mLuaState, visible);
	resetTimer();
	if (lua_pcall(mLuaState, 1, 0, 0) != LUA_OK)
	{
		reportError();
	}
}

void HBViewerAutomation::onTPStateChange(S32 state, const std::string& reason)
{
	LL_TRACY_TIMER(TRC_LUA_CALLBACK);

	if (!mHasOnTPStateChange || !mLuaState)
	{
		return;
	}

	LL_DEBUGS("Lua") << "Invoking OnTPStateChange Lua callback. state="
					 << state << " - Reason: " << reason << LL_ENDL;

	lua_getglobal(mLuaState, "OnTPStateChange");
	lua_pushinteger(mLuaState, state);
	lua_pushstring(mLuaState, reason.c_str());
	resetTimer();
	if (lua_pcall(mLuaState, 2, 0, 0) != LUA_OK)
	{
		reportError();
	}
}

void HBViewerAutomation::onFailedTPSimChange(S32 agents_count)
{
	LL_TRACY_TIMER(TRC_LUA_CALLBACK);

	if (!mHasOnFailedTPSimChange || !mLuaState ||
		// Is a teleport in progress ?
		gAgent.teleportInProgress() ||
		// Are there valid global TP coordinates available ?
		gAgent.getTeleportedPosGlobal().isExactlyZero())
	{
		return;
	}

	LL_DEBUGS("Lua") << "Invoking OnFailedTPSimChange Lua callback. agents_count="
					 << agents_count << LL_ENDL;

	lua_getglobal(mLuaState, "OnFailedTPSimChange");
	lua_pushinteger(mLuaState, agents_count);
	lua_pushinteger(mLuaState, gAgent.getTeleportedPosGlobal().mdV[VX]);
	lua_pushinteger(mLuaState, gAgent.getTeleportedPosGlobal().mdV[VY]);
	lua_pushinteger(mLuaState, gAgent.getTeleportedPosGlobal().mdV[VZ]);
	resetTimer();
	if (lua_pcall(mLuaState, 4, 0, 0) != LUA_OK)
	{
		reportError();
	}
}

void HBViewerAutomation::onWindlightChange(const std::string& sky_settings,
										   const std::string& water_settings,
										   const std::string& day_settings)
{
	LL_TRACY_TIMER(TRC_LUA_CALLBACK);

	if (!mHasOnWindlightChange || !mLuaState ||
		sIgnoredCallbacks[E_ONWINDLIGHTCHANGE])
	{
		return;
	}

	LL_DEBUGS("Lua") << "Invoking OnWindlightChange Lua callback. sky_settings_name="
					 << sky_settings << " - water_settings_name="
					 << water_settings << " - day_settings_name="
					 << day_settings << LL_ENDL;

	lua_getglobal(mLuaState, "OnWindlightChange");
	lua_pushstring(mLuaState, sky_settings.c_str());
	lua_pushstring(mLuaState, water_settings.c_str());
	lua_pushstring(mLuaState, day_settings.c_str());
	resetTimer();
	if (lua_pcall(mLuaState, 3, 0, 0) != LUA_OK)
	{
		reportError();
	}
}

void HBViewerAutomation::onCameraModeChange(S32 mode)
{
	LL_TRACY_TIMER(TRC_LUA_CALLBACK);

	if (!mHasOnCameraModeChange || !mLuaState ||
		sIgnoredCallbacks[E_ONCAMERAMODECHANGE])
	{
		return;
	}

	LL_DEBUGS("Lua") << "Invoking OnCameraModeChange Lua callback. mode="
					 << mode << LL_ENDL;

	lua_getglobal(mLuaState, "OnCameraModeChange");
	lua_pushinteger(mLuaState, mode);
	resetTimer();
	if (lua_pcall(mLuaState, 1, 0, 0) != LUA_OK)
	{
		reportError();
	}
}

void HBViewerAutomation::onJoystickButtons(S32 old_state, S32 new_state)
{
	LL_TRACY_TIMER(TRC_LUA_CALLBACK);

	if (!mHasOnJoystickButtons || !mLuaState)
	{
		return;
	}
	LL_DEBUGS("Lua") << "Invoking OnJoystickButtons Lua callback. old_state="
					 << old_state << " - new_state=" << new_state << LL_ENDL;
	lua_getglobal(mLuaState, "OnJoystickButtons");
	lua_pushinteger(mLuaState, old_state);
	lua_pushinteger(mLuaState, new_state);
	resetTimer();
	if (lua_pcall(mLuaState, 2, 0, 0) != LUA_OK)
	{
		reportError();
	}
}

void HBViewerAutomation::onLuaPieMenu(U32 slice, S32 type, const LLPickInfo& pick)
{
	LL_TRACY_TIMER(TRC_LUA_CALLBACK);

	if (!mHasOnLuaPieMenu || !mLuaState)
	{
		return;
	}

	LL_DEBUGS("Lua") << "Invoking OnLuaPieMenu Lua callback." << LL_ENDL;

	lua_getglobal(mLuaState, "OnLuaPieMenu");

	lua_newtable(mLuaState);

	lua_pushliteral(mLuaState, "type");
	lua_pushinteger(mLuaState, type);
	lua_rawset(mLuaState, -3);

	lua_pushliteral(mLuaState, "slice");
	lua_pushinteger(mLuaState, slice);
	lua_rawset(mLuaState, -3);

	const LLVector3d& pos_global = pick.mPosGlobal;
	lua_pushliteral(mLuaState, "global_x");
	lua_pushnumber(mLuaState, pos_global.mdV[VX]);
	lua_rawset(mLuaState, -3);

	lua_pushliteral(mLuaState, "global_y");
	lua_pushnumber(mLuaState, pos_global.mdV[VY]);
	lua_rawset(mLuaState, -3);

	lua_pushliteral(mLuaState, "altitude");
	lua_pushnumber(mLuaState, pos_global.mdV[VZ]);
	lua_rawset(mLuaState, -3);

	const LLUUID& object_id = pick.mObjectID;
	lua_pushliteral(mLuaState, "object_id");
	lua_pushstring(mLuaState, object_id.asString().c_str());
	lua_rawset(mLuaState, -3);

	if (object_id.notNull())
	{
		lua_pushliteral(mLuaState, "object_face");
		lua_pushinteger(mLuaState, pick.mObjectFace);
		lua_rawset(mLuaState, -3);
	}

	if (type == PICKED_PARTICLE)
	{
		lua_pushliteral(mLuaState, "particle_owner_id");
		lua_pushstring(mLuaState, pick.mParticleOwnerID.asString().c_str());
		lua_rawset(mLuaState, -3);

		lua_pushliteral(mLuaState, "particle_source_id");
		lua_pushstring(mLuaState, pick.mParticleSourceID.asString().c_str());
		lua_rawset(mLuaState, -3);
	}

	resetTimer();
	if (lua_pcall(mLuaState, 1, 0, 0) != LUA_OK)
	{
		reportError();
	}
}

//static
void HBViewerAutomation::contextMenuCallback(HBContextMenuData* datap)
{
	if (gAutomationp && datap)
	{
		bool ret = gAutomationp->onContextMenu(datap->mHandlerID,
											   datap->mOperation,
											   datap->mMenuType);
		if (ret)
		{
			// When the OnContextMenu Lua callback returns true, perform the
			// default operation, where appropriate.
			switch (datap->mOperation)
			{
				case HBContextMenuData::SET:
					ret = LLEditMenuHandler::setCustomMenu(datap->mHandlerID,
														   "Cut to Lua",
														   "Copy to Lua",
														   "Paste from Lua");
					LL_DEBUGS("Lua") << "Default Lua context entries creation "
									 << (ret ? "succeeded" : "failed")
									 << " for handler_id=" << datap->mHandlerID
									 << LL_ENDL;
					
					break;

				case HBContextMenuData::PASTE:
					ret = LLEditMenuHandler::pasteTo(datap->mHandlerID);
					LL_DEBUGS("Lua") << "Pasting "
									 << (ret ? "succeeded" : "failed")
									 << " to handler_id=" << datap->mHandlerID
									 << LL_ENDL;
					break;

				default:
					LL_DEBUGS("Lua") << "handler_id=" << " - operation="
									 << datap->mOperation << LL_ENDL;
			}
		}
		else
		{
			LL_DEBUGS("Lua") << "No default action taken for handler_id="
							 << datap->mHandlerID << " - operation="
							 << datap->mOperation << LL_ENDL;
		}
	}
	delete datap;
}

bool HBViewerAutomation::onContextMenu(U32 handler_id, S32 operation,
									   const std::string& type)
{
	LL_TRACY_TIMER(TRC_LUA_CALLBACK);

	if (!mHasOnContextMenu || !mLuaState)
	{
		return false;
	}
	LL_DEBUGS("Lua") << "Invoking OnContextMenu Lua callback. handler_id="
					 << handler_id << " - operation=" << operation
					 << " - type=" << type << LL_ENDL;
	lua_getglobal(mLuaState, "OnContextMenu");
	lua_pushstring(mLuaState, type.c_str());
	lua_pushinteger(mLuaState, handler_id);
	lua_pushinteger(mLuaState, operation);
	lua_pushstring(mLuaState,
				   wstring_to_utf8str(gClipboard.getClipBoardString()).c_str());
	resetTimer();
	if (lua_pcall(mLuaState, 4, 1, 0) != LUA_OK)
	{
		reportError();
		return false;
	}
	if (lua_gettop(mLuaState) == 0 ||
		lua_type(mLuaState, -1) != LUA_TBOOLEAN)
	{
		lua_pushliteral(mLuaState,
						"OnContextMenu() Lua callback did not return a boolean");
		reportError();
		return false;
	}

	bool result = lua_toboolean(mLuaState, -1);
	lua_pop(mLuaState, 1);
	return result;
}

void HBViewerAutomation::onRLVHandleCommand(const LLUUID& object_id,
											const std::string& behav,
											const std::string& option,
											const std::string& param)
{
	LL_TRACY_TIMER(TRC_LUA_CALLBACK);

	if (!mHasOnRLVHandleCommand || !mLuaState) return;

	LL_DEBUGS("Lua") << "Invoking OnRLVHandleCommand Lua callback. Object Id: "
					 << object_id << " - behav=" << behav << " - option="
					 << option << " - param=" << param << LL_ENDL;

	lua_getglobal(mLuaState, "OnRLVHandleCommand");
	lua_pushstring(mLuaState, object_id.asString().c_str());
	lua_pushstring(mLuaState, behav.c_str());
	lua_pushstring(mLuaState, option.c_str());
	lua_pushstring(mLuaState, param.c_str());
	resetTimer();
	if (lua_pcall(mLuaState, 4, 0, 0) != LUA_OK)
	{
		reportError();
	}
}

void HBViewerAutomation::onRLVAnswerOnChat(const LLUUID& obj_id, S32 channel,
										   const std::string& text)
{
	LL_TRACY_TIMER(TRC_LUA_CALLBACK);

	if (!mHasOnRLVAnswerOnChat || !mLuaState) return;

	LL_DEBUGS("Lua") << "Invoking OnRLVAnswerOnChat Lua callback for object Id: "
					 << obj_id << " - channel: " << channel << LL_ENDL;

	lua_getglobal(mLuaState, "OnRLVAnswerOnChat");
	lua_pushstring(mLuaState, obj_id.asString().c_str());
	lua_pushinteger(mLuaState, channel);
	lua_pushstring(mLuaState, text.c_str());
	resetTimer();
	if (lua_pcall(mLuaState, 3, 0, 0) != LUA_OK)
	{
		reportError();
	}
}

void HBViewerAutomation::onObjectInfoReply(const LLUUID& object_id,
										   const std::string& name,
										   const std::string& desc,
										   const LLUUID& owner_id,
										   const LLUUID& group_id)
{
	LL_TRACY_TIMER(TRC_LUA_CALLBACK);

	if (!mHasOnObjectInfoReply || !mLuaState) return;

	LL_DEBUGS("Lua") << "Invoking OnObjectInfoReply Lua callback. Object: "
					 << name << " (" << object_id << ")" << LL_ENDL;

	lua_getglobal(mLuaState, "OnObjectInfoReply");
	lua_pushstring(mLuaState, object_id.asString().c_str());
	lua_pushstring(mLuaState, name.c_str());
	lua_pushstring(mLuaState, desc.c_str());
	lua_pushstring(mLuaState, owner_id.asString().c_str());
	lua_pushstring(mLuaState, group_id.asString().c_str());
	resetTimer();
	if (lua_pcall(mLuaState, 5, 0, 0) != LUA_OK)
	{
		reportError();
	}
}

void HBViewerAutomation::resetTimer()
{
	mWatchdogTimer.start();
	mWatchdogTimer.setTimerExpirySec(mWatchdogTimeout);
}

//static
void HBViewerAutomation::watchdog(lua_State* state, lua_Debug*)
{
	HBViewerAutomation* self = findInstance(state);
	if (self)
	{
		if (self->mWatchdogTimer.hasExpired())
		{
			lua_pushliteral(state, "Lua watchdog timeout reached !");
			lua_error(state);
		}
	}
	else
	{
		llwarns << "Lua instance gone !" << llendl;
	}
}

//static
bool HBViewerAutomation::requestObjectPropertiesFamily(const LLUUID& object_id,
													   U32 reason)
{
	LLMessageSystem* msg = gMessageSystemp;
	if (!msg || object_id.isNull())
	{
		return false;
	}
	// We need for the object to be around...
	LLViewerObject* objectp = gObjectList.findObject(object_id);
	if (!objectp)
	{
		return false;
	}
	// We need for the object to have a region (which should always be the
	// case)...
	LLViewerRegion* regionp = objectp->getRegion();
	if (!regionp)
	{
		return false;
	}

	bool in_mute = sMuteObjectRequests.count(object_id) != 0;
	bool in_unmute = sUnmuteObjectRequests.count(object_id) != 0;
	bool in_object_info = false;
	if (gAutomationp)
	{
		in_object_info =
			gAutomationp->mObjectInfoRequests.count(object_id) != 0;
	}

	switch (reason)
	{
		case 0:		// For mute
			if (in_mute)
			{
				return true;	// No need to re-request
			}
			sMuteObjectRequests.emplace(object_id);
			if (in_unmute || in_object_info)
			{
				return true;	// No need to re-request
			}
			break;

		case 1:		// For un-mute
			if (in_unmute)
			{
				return true;	// No need to re-request
			}
			sUnmuteObjectRequests.emplace(object_id);
			if (in_mute || in_object_info)
			{
				return true;	// No need to re-request
			}
			break;

		default:	// For object info request
			if (!gAutomationp)
			{
				return false;	// Not requesting if no automation script
			}
			if (in_object_info)
			{
				return true;	// No need to re-request
			}
			gAutomationp->mObjectInfoRequests.emplace(object_id);
			if (in_mute || in_unmute)
			{
				return true;	// No need to re-request
			}
	}

	msg->newMessageFast(_PREHASH_RequestObjectPropertiesFamily);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlockFast(_PREHASH_ObjectData);
	msg->addU32Fast(_PREHASH_RequestFlags, 0);
	msg->addUUIDFast(_PREHASH_ObjectID, object_id);
	msg->sendReliable(regionp->getHost());
	LL_DEBUGS("Lua") << "Sent data request for object " << object_id
					 << LL_ENDL;

	return true;
}

//static
void HBViewerAutomation::processObjectPropertiesFamily(LLMessageSystem* msg)
{
	LL_TRACY_TIMER(TRC_LUA_PROCESS_OBJ_PROP);

	if (!msg) return;

	LLUUID object_id;
	msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_ObjectID, object_id);

	uuid_list_t::iterator it = sMuteObjectRequests.find(object_id);
	bool for_mute = it != sMuteObjectRequests.end();
	if (for_mute)
	{
		sMuteObjectRequests.erase(it);
	}

	it = sUnmuteObjectRequests.find(object_id);
	bool for_unmute = it != sUnmuteObjectRequests.end();
	if (for_unmute)
	{
		sUnmuteObjectRequests.erase(it);
	}

	bool for_object_info = false;
	if (gAutomationp)
	{
		it = gAutomationp->mObjectInfoRequests.find(object_id);
		if (it != gAutomationp->mObjectInfoRequests.end())
		{
			for_object_info = true;
			gAutomationp->mObjectInfoRequests.erase(it);
		}
	}

	if (!for_mute && !for_unmute && !for_object_info)
	{
		// Object data not requested by us.
		return;
	}

	LLUUID owner_id, group_id;
	msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_OwnerID, owner_id);
	msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_GroupID, group_id);
	std::string name, desc;
	msg->getStringFast(_PREHASH_ObjectData, _PREHASH_Name, name);
	msg->getStringFast(_PREHASH_ObjectData, _PREHASH_Description, desc);

	// Process (un)mute first, in case we requested both one and object info
	if (for_mute || for_unmute)
	{
		LLMute mute(object_id, name, LLMute::OBJECT);
		if (for_mute)
		{
			LLMuteList::add(mute);
		}
		else
		{
			LLMuteList::remove(mute);
		}
	}

	if (for_object_info)
	{
		gAutomationp->onObjectInfoReply(object_id, name, desc, owner_id,
										group_id);
	}
}

//static
int HBViewerAutomation::print(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	S32 n = lua_gettop(state);
	if (!n) return 0;

	std::string value;
	for (S32 i = 1; i <= n; ++i)
	{
		int type = lua_type(state, i);
		switch (type)
		{
			case LUA_TNIL:
				value = "nil";
				break;

			case LUA_TBOOLEAN:
				value = lua_toboolean(state, i) ? "true" : "false";
				break;

			case LUA_TNUMBER:
				value = llformat(LUA_NUMBER_FMT, lua_tonumber(state, i));
				break;

			case LUA_TSTRING:
				value = lua_tostring(state, i);
				break;

			default:
				value = lua_typename(state, i);
		}
		// NOTE: we need to delay chat printing until after login, since we
		// otherwise could crash due to LLFloaterChat not yet being
		// constructed.
		if (self->mUsePrintBuffer || !LLStartUp::isLoggedIn())
		{
			if (!self->mPrintBuffer.empty())
			{
#if LL_WINDOWS
				self->mPrintBuffer += "\r\n";
#else
				self->mPrintBuffer += '\n';
#endif
			}
			self->mPrintBuffer += value;
		}
		else
		{
			LLChat chat;
			chat.mFromName = "Lua";
			chat.mSourceType = CHAT_SOURCE_SYSTEM;
			chat.mText = "Lua: " + value;
			LLFloaterChat::addChat(chat, false, false);
		}
	}

	lua_pop(state, n);

	return 0;
}

//static
int HBViewerAutomation::isUUID(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	if (!state) return 0;	// Paranoia

	S32 n = lua_gettop(state);
	if (n != 1)
	{
		luaL_error(state, "%d arguments passed; expected 1.", n);
	}

	bool valid = false;

	if (lua_type(state, 1) == LUA_TSTRING)
	{
		std::string param(luaL_checkstring(state, 1));
		valid = LLUUID::validate(param);
	}
	lua_pop(state, 1);

	lua_pushboolean(state, valid);

	return 1;
}

//static
int HBViewerAutomation::isAvatar(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("IsAvatar");
	}

	S32 n = lua_gettop(state);
	if (n != 1)
	{
		luaL_error(state, "%d arguments passed; expected 1.", n);
	}

	LLUUID id(luaL_checkstring(state, 1), false);
	lua_pop(state, 1);

	bool is_avatar = false;
	if (id.notNull())
	{
		is_avatar = gObjectList.findAvatar(id) != NULL;
	}
	lua_pushboolean(state, is_avatar);

	return 1;
}

//static
int HBViewerAutomation::isObject(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("IsObject");
	}

	S32 n = lua_gettop(state);
	if (n != 1)
	{
		luaL_error(state, "%d arguments passed; expected 1.", n);
	}

	LLUUID id(luaL_checkstring(state, 1), false);
	lua_pop(state, 1);

	bool is_object = false;
	if (id.notNull())
	{
		LLViewerObject* objectp = gObjectList.findObject(id);
		is_object = objectp && !objectp->isAvatar();
	}

	lua_pushboolean(state, is_object);

	return 1;
}

//static
int HBViewerAutomation::isAgentFriend(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("IsAgentFriend");
	}

	S32 n = lua_gettop(state);
	if (n != 1)
	{
		luaL_error(state, "%d arguments passed; expected 1.", n);
	}

	std::string param(luaL_checkstring(state, 1));
	lua_pop(state, 1);

	LLUUID id;
	if (LLUUID::validate(param))
	{
		id.set(param);
	}

	bool is_friend = false;
	bool is_online = false;

	if (id.notNull())
	{
		is_friend = LLAvatarTracker::isAgentFriend(id);
		is_online = is_friend && gAvatarTracker.isBuddyOnline(id);
	}
	else if (!param.empty())
	{
		// 'param' should contain the legacy name of the putative friend, with
		// the "Display Name [Legacy Name]" format accepted as well.
		size_t i = param.rfind(']');
		if (i == param.size() - 1)
		{
			size_t j = param.rfind('[');
			if (j != std::string::npos)
			{
				// This is indeed the "Display Name [Legacy Name]" format
				param = param.substr(j + 1, i - j - 1);
			}
		}
		// Eliminate the " Resident" last name if any.
		i = param.find(" Resident");
		if (i != std::string::npos)
		{
			param = param.substr(0, i);
		}

		// Collect all our friends in a map
		LLCollectAllBuddies friends;
		gAvatarTracker.applyFunctor(friends);
		// Try and find a matching friend name (case-sensitive)
		std::string name;
		typedef LLCollectAllBuddies::buddy_map_t::const_iterator buddies_it;
		for (buddies_it it = friends.mOnline.begin(),
						end = friends.mOnline.end();
			 it != end; ++it)
		{
			name = it->first;
			i = name.find(" Resident");
			if (i != std::string::npos)
			{
				name = name.substr(0, i);
			}
			if (name == param)
			{
				is_friend = is_online = true;
				break;
			}
		}
		if (!is_friend)
		{
			for (buddies_it it = friends.mOffline.begin(),
							end = friends.mOffline.end();
				 it != end; ++it)
			{
				name = it->first;
				i = name.find(" Resident");
				if (i != std::string::npos)
				{
					name = name.substr(0, i);
				}
				if (name == param)
				{
					is_friend = true;
					break;
				}
			}
		}
	}

	lua_pushboolean(state, is_friend);
	lua_pushboolean(state, is_online);

	return 2;
}

//static
int HBViewerAutomation::isAgentGroup(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("IsAgentGroup");
	}

	S32 n = lua_gettop(state);
	if (n != 1)
	{
		luaL_error(state, "%d arguments passed; expected 1.", n);
	}

	LLUUID id(luaL_checkstring(state, 1), false);
	lua_pop(state, 1);

	bool is_in_group = false;
	if (id.notNull())
	{
		is_in_group = gAgent.isInGroup(id, true);
	}

	lua_pushboolean(state, is_in_group);
	lua_pushboolean(state, is_in_group && gAgent.getGroupID() == id);

	return 2;
}

//static
int HBViewerAutomation::getAvatarName(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("GetAvatarName");
	}

	S32 n = lua_gettop(state);
	if (n != 1 && n != 2)
	{
		luaL_error(state, "%d arguments passed; expected 1 or 2.", n);
	}

	LLUUID id(luaL_checkstring(state, 1), false);

	S32 type = 0;
	if (n > 1)
	{
		type = luaL_checknumber(state, 2);
	}

	lua_pop(state, n);

	std::string name;
	if (id.notNull() && gCacheNamep && !gCacheNamep->getFullName(id, name))
	{
		name.clear();	// Prevents "loading..."
	}
	if (type != 0 && !name.empty())
	{
		LLAvatarName avatar_name;
		if (LLAvatarNameCache::get(id, &avatar_name))
		{
			if (type == 1)
			{
				name = avatar_name.mDisplayName;
			}
			else
			{
				name = avatar_name.getNames();
			}
		}
	}

	lua_pushstring(state, name.c_str());

	return 1;
}

//static
int HBViewerAutomation::getGroupName(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("GetGroupName");
	}

	S32 n = lua_gettop(state);
	if (n != 1)
	{
		luaL_error(state, "%d arguments passed; expected 1.", n);
	}

	LLUUID id(luaL_checkstring(state, 1), false);
	lua_pop(state, 1);

	std::string name;
	if (id.notNull() && gCacheNamep && !gCacheNamep->getGroupName(id, name))
	{
		name.clear();	// Prevents "loading..."
	}

	lua_pushstring(state, name.c_str());

	return 1;
}

//static
int HBViewerAutomation::isAdmin(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("IsAdmin");
	}

	S32 n = lua_gettop(state);
	if (n != 1)
	{
		luaL_error(state, "%d arguments passed; expected 1.", n);
	}

	std::string param(luaL_checkstring(state, 1));
	lua_pop(state, 1);

	std::string name;
	if (LLUUID::validate(param))
	{
		LLUUID av_id(param);
		if (av_id.notNull() && gCacheNamep &&
			gCacheNamep->getName(av_id, name, param))
		{
			name += " " + param;
		}
		else
		{
			name.clear();
		}
	}
	else
	{
		name = param;
	}

	if (name.empty())
	{
		lua_pushnil(state);
	}
	else
	{
		lua_pushboolean(state, LLMuteList::isLinden(name));
	}

	return 1;
}

//static
int HBViewerAutomation::getRadarData(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("GetRadarData");
	}

	S32 n = lua_gettop(state);
	if (n != 1)
	{
		luaL_error(state, "%d arguments passed; expected 1.", n);
	}

	LLUUID id(luaL_checkstring(state, 1), false);
	lua_pop(state, 1);

	HBRadarListEntry* entry = NULL;
	if (id.notNull())
	{
		HBFloaterRadar* avlist = HBFloaterRadar::findInstance();
		if (avlist)
		{
			entry = avlist->getAvatarEntry(id);
		}
	}
	if (!entry || entry->isDead())
	{
		lua_pushnil(state);
		return 1;
	}

	lua_newtable(state);

	lua_pushliteral(state, "id");
	lua_pushstring(state, id.asString().c_str());
	lua_rawset(state, -3);

	lua_pushliteral(state, "name");
	lua_pushstring(state, entry->getName().c_str());
	lua_rawset(state, -3);

	lua_pushliteral(state, "display_name");
	lua_pushstring(state, entry->getDisplayName().c_str());
	lua_rawset(state, -3);

	lua_pushliteral(state, "name_color");
	const LLColor4& name_color = entry->getColor();
	lua_pushstring(state, llformat("%f, %f, %f",
								   name_color.mV[0],
								   name_color.mV[1],
								   name_color.mV[2]).c_str());
	lua_rawset(state, -3);

	lua_pushliteral(state, "tooltip");
	lua_pushstring(state, entry->getToolTip().c_str());
	lua_rawset(state, -3);

	const LLVector3d& global_pos = entry->getPosition();
	lua_pushliteral(state, "global_x");
	lua_pushnumber(state, global_pos.mdV[VX]);
	lua_rawset(state, -3);

	lua_pushliteral(state, "global_y");
	lua_pushnumber(state, global_pos.mdV[VY]);
	lua_rawset(state, -3);

	lua_pushliteral(state, "altitude");
	lua_pushnumber(state, global_pos.mdV[VZ]);
	lua_rawset(state, -3);

	lua_pushliteral(state, "friend");
	lua_pushboolean(state, entry->isFriend());
	lua_rawset(state, -3);

	lua_pushliteral(state, "muted");
	lua_pushboolean(state, entry->isMuted());
	lua_rawset(state, -3);

	lua_pushliteral(state, "derendered");
	lua_pushboolean(state, entry->isDerendered());
	lua_rawset(state, -3);

	lua_pushliteral(state, "marked");
	lua_pushboolean(state, entry->isMarked());
	lua_rawset(state, -3);

	lua_pushliteral(state, "mark_char");
	lua_pushstring(state, entry->getMarkChar().c_str());
	lua_rawset(state, -3);

	lua_pushliteral(state, "mark_color");
	const LLColor4& mark_color = entry->getMarkColor();
	lua_pushstring(state, llformat("%f, %f, %f",
								   mark_color.mV[0],
								   mark_color.mV[1],
								   mark_color.mV[2]).c_str());
	lua_rawset(state, -3);

	lua_pushliteral(state, "focused");
	lua_pushboolean(state, entry->isFocused());
	lua_rawset(state, -3);

	lua_pushliteral(state, "drawn");
	lua_pushboolean(state, entry->isDrawn());
	lua_rawset(state, -3);

	lua_pushliteral(state, "in_sim");
	lua_pushboolean(state, entry->isInSim());
	lua_rawset(state, -3);

	lua_pushliteral(state, "entry_age");
	lua_pushnumber(state, entry->getEntryAgeSeconds());
	lua_rawset(state, -3);

	return 1;
}

//static
int HBViewerAutomation::setRadarTracking(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("SetRadarTracking");
	}

	S32 n = lua_gettop(state);
	if (n != 1 && n != 2)
	{
		luaL_error(state, "%d arguments passed; expected 1 or 2.", n);
	}

	LLUUID id(luaL_checkstring(state, 1), false);

	bool force = false;
	if (n > 1)
	{
		force = lua_toboolean(state, 2);
	}

	lua_pop(state, n);

	HBIgnoreCallback lock_on_radar_track(E_ONRADARTRACK);

	bool success = false;

	HBFloaterRadar* avlist = HBFloaterRadar::findInstance();
	if (id.isNull())
	{
		if (avlist)
		{
			avlist->stopTracker();
		}
		success = true;
	}
	else if (avlist)
	{
		success = avlist->startTracker(id);
	}
	else if (force)
	{
		if (!gSavedSettings.getBool("RadarKeepOpen"))
		{
			llinfos << "Enabling Radar background tracking" << llendl;
			gSavedSettings.setBool("RadarKeepOpen", true);
		}
		avlist = HBFloaterRadar::getInstance();
		if (avlist)
		{
			success = avlist->startTracker(id);
			HBFloaterRadar::hideInstance();
		}
	}

	lua_pushboolean(state, success);

	return 1;
}

//static
int HBViewerAutomation::setRadarToolTip(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("SetRadarToolTip");
	}

	S32 n = lua_gettop(state);
	if (n != 1 && n != 2)
	{
		luaL_error(state, "%d arguments passed; expected 1 or 2.", n);
	}
	LLUUID id(luaL_checkstring(state, 1), false);
	std::string tooltip;
	if (n > 1)
	{
		tooltip = luaL_checkstring(state, 2);
	}
	lua_pop(state, n);

	HBRadarListEntry* entry = NULL;
	if (id.notNull())
	{
		HBFloaterRadar* avlist = HBFloaterRadar::findInstance();
		if (avlist)
		{
			entry = avlist->getAvatarEntry(id);
		}
	}

	bool success = entry && !entry->isDead();
	if (success)
	{
		entry->setToolTip(tooltip);
	}
	lua_pushboolean(state, success);

	return 1;
}

//static
int HBViewerAutomation::setRadarMarkChar(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("SetRadarMarkChar");
	}

	S32 n = lua_gettop(state);
	if (n != 1 && n != 2)
	{
		luaL_error(state, "%d arguments passed; expected 1 or 2.", n);
	}
	LLUUID id(luaL_checkstring(state, 1), false);
	std::string chr;
	if (n > 1)
	{
		chr = luaL_checkstring(state, 2);
	}
	lua_pop(state, n);

	HBRadarListEntry* entry = NULL;
	if (id.notNull())
	{
		HBFloaterRadar* avlist = HBFloaterRadar::findInstance();
		if (avlist)
		{
			entry = avlist->getAvatarEntry(id);
		}
	}

	bool success = entry && !entry->isDead();
	if (success)
	{
		entry->setMarkChar(chr);
	}
	lua_pushboolean(state, success);

	return 1;
}

//static
int HBViewerAutomation::setRadarMarkColor(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("SetRadarMarkColor");
	}

	S32 n = lua_gettop(state);
	if (n != 1 && n != 2)
	{
		luaL_error(state, "%d arguments passed; expected 1 or 2.", n);
	}

	LLUUID id(luaL_checkstring(state, 1), false);
	std::string color_str;
	if (n > 1)
	{
		color_str = luaL_checkstring(state, 2);
	}
	lua_pop(state, n);

	LLColor4 color;
	if (color_str.empty())
	{
		color = gColors.getColor("RadarMarkColor");
	}
	else if (!LLColor4::parseColor(color_str, &color))
	{
		luaL_error(state, "invalid color: %s", color_str.c_str());
	}
	else
	{
		color.mV[3] = 1.f;	// Make sure we use an opaque color...
	}

	HBRadarListEntry* entry = NULL;
	if (id.notNull())
	{
		HBFloaterRadar* avlist = HBFloaterRadar::findInstance();
		if (avlist)
		{
			entry = avlist->getAvatarEntry(id);
		}
	}

	bool success = entry && !entry->isDead();
	if (success)
	{
		entry->setMarkColor(color);
	}
	lua_pushboolean(state, success);

	return 1;
}

//static
int HBViewerAutomation::setRadarNameColor(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("SetRadarNameColor");
	}

	S32 n = lua_gettop(state);
	if (n != 1 && n != 2)
	{
		luaL_error(state, "%d arguments passed; expected 1 or 2.", n);
	}

	LLUUID id(luaL_checkstring(state, 1), false);

	std::string color_str;
	if (n > 1)
	{
		color_str = luaL_checkstring(state, 2);
	}

	lua_pop(state, n);

	bool success = false;

	LLColor4 color;
	if (color_str.empty())
	{
		color = LLColor4::black;
	}
	else if (!LLColor4::parseColor(color_str, &color))
	{
		luaL_error(state, "invalid color: %s", color_str.c_str());
	}
	else
	{
		color.mV[3] = 1.f;	// Make sure we use an opaque color...
	}

	if (id != gAgentID && id.notNull())
	{
		LLVOAvatar* avatarp = gObjectList.findAvatar(id);
		success = avatarp != NULL;
		if (success)
		{
			avatarp->setRadarColor(color);
			success = HBFloaterRadar::setAvatarNameColor(id, color);
		}
	}

	lua_pushboolean(state, success);

	return 1;
}

//static
int HBViewerAutomation::setAvatarMinimapColor(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("SetAvatarMinimapColor");
	}

	S32 n = lua_gettop(state);
	if (n != 1 && n != 2)
	{
		luaL_error(state, "%d arguments passed; expected 1 or 2.", n);
	}

	LLUUID id(luaL_checkstring(state, 1), false);

	std::string color_str;
	if (n > 1)
	{
		color_str = luaL_checkstring(state, 2);
	}

	lua_pop(state, n);

	bool success = true;

	LLColor4 color;
	if (color_str.empty())
	{
		static LLCachedControl<LLColor4U> map_avatar(gColors, "MapAvatar");
		static LLCachedControl<LLColor4U> map_friend(gColors, "MapFriend");
		bool is_friend = LLAvatarTracker::isAgentFriend(id);
		color = LLColor4(is_friend ? map_friend : map_avatar);
	}
	else if (!LLColor4::parseColor(color_str, &color))
	{
		luaL_error(state, "invalid color: %s", color_str.c_str());
	}

	if (id == gAgentID)
	{
		success = false;
	}
	else
	{
		LLVOAvatar* avatarp = gObjectList.findAvatar(id);
		success = avatarp != NULL;
		if (success)
		{
			avatarp->setMinimapColor(color);
		}
	}

	lua_pushboolean(state, success);

	return 1;
}

//static
int HBViewerAutomation::setAvatarNameTagColor(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("SetAvatarNameTagColor");
	}

	S32 n = lua_gettop(state);
	if (n != 1 && n != 2)
	{
		luaL_error(state, "%d arguments passed; expected 1 or 2.", n);
	}

	LLUUID id(luaL_checkstring(state, 1), false);

	std::string color_str;
	if (n > 1)
	{
		color_str = luaL_checkstring(state, 2);
	}

	lua_pop(state, n);

	bool success = true;

	LLColor4 color;
	if (color_str.empty())
	{
		static LLCachedControl<LLColor4U> tag_color(gColors,
													"AvatarNameColor");
		color = LLColor4(tag_color);
	}
	else if (!LLColor4::parseColor(color_str, &color))
	{
		luaL_error(state, "invalid color: %s", color_str.c_str());
	}

	if (id == gAgentID)
	{
		success = isAgentAvatarValid();
		if (success)
		{
			gAgentAvatarp->setNameTagColor(color);
		}
	}
	else
	{
		LLVOAvatar* avatarp = gObjectList.findAvatar(id);
		success = avatarp != NULL;
		if (success)
		{
			avatarp->setNameTagColor(color);
		}
	}

	lua_pushboolean(state, success);

	return 1;
}

//static
void HBViewerAutomation::addToAgentPosHistory(const LLVector3d& global_pos)
{
	static LLCachedControl<U32> max_history(gSavedSettings,
											"LuaMaxAgentPosHistorySize");
	while (sPositionsHistory.size() >= (size_t)max_history)
	{
		sPositionsHistory.pop_front();
	}
	if (max_history > 0)
	{
		sPositionsHistory.emplace_back(global_pos);
	}
}

//static
int HBViewerAutomation::getAgentPosHistory(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("GetAgentPosHistory");
	}

	S32 n = lua_gettop(state);
	if (n)
	{
		luaL_error(state, "%d arguments passed; expected 0.", n);
	}

	if (sPositionsHistory.empty())
	{
		lua_pushnil(state);
		return 1;
	}

	lua_newtable(state);
	S32 i = 1;
	std::string vecstr;
	// Place the positions in reverse order in the Lua table (i.e. last known
	// position will be first in the table).
	for (pos_history_t::reverse_iterator it = sPositionsHistory.rbegin(),
										 end = sPositionsHistory.rend();
		 it != end; ++it)
	{
		vecstr = llformat("%lf %lf %lf", it->mdV[0], it->mdV[1], it->mdV[2]);
		lua_pushnumber(state, i++);
		lua_pushstring(state, vecstr.c_str());
		lua_rawset(state, -3);
	}
	return 1;
}

//static
int HBViewerAutomation::getAgentInfo(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("GetAgentInfo");
	}

	S32 n = lua_gettop(state);
	if (n)
	{
		luaL_error(state, "%d arguments passed; expected 0.", n);
	}

	lua_newtable(state);

	std::string temp;
	gAgent.getName(temp);
	lua_pushliteral(state, "name");
	lua_pushstring(state, temp.c_str());
	lua_rawset(state, -3);

	if (LLStartUp::isLoggedIn() && isAgentAvatarValid())
	{
		lua_pushliteral(state, "id");
		lua_pushstring(state, gAgentID.asString().c_str());
		lua_rawset(state, -3);

		LLAvatarName avatar_name;
		if (LLAvatarNameCache::get(gAgentID, &avatar_name))
		{
			lua_pushliteral(state, "display_name");
			lua_pushstring(state, avatar_name.mDisplayName.c_str());
			lua_rawset(state, -3);
		}

		temp = "unknown";
		if (gAgent.isTeen())
		{
			temp = "teen";
		}
		else if (gAgent.isAdult())
		{
			temp = "adult";
		}
		else if (gAgent.isMature())
		{
			temp = "mature";
		}
		lua_pushliteral(state, "maturity");
		lua_pushstring(state, temp.c_str());
		lua_rawset(state, -3);

		lua_pushliteral(state, "active_group_id");
		lua_pushstring(state, gAgent.getGroupID().asString().c_str());
		lua_rawset(state, -3);

		lua_pushliteral(state, "camera_mode");
		lua_pushinteger(state, gAgent.getCameraMode());
		lua_rawset(state, -3);

		lua_pushliteral(state, "control_flags");
		lua_pushinteger(state, gAgent.getControlFlags());
		lua_rawset(state, -3);

		lua_pushliteral(state, "occupation");
		S32 occupation = 0;
		if (gAgent.getAFK())
		{
			occupation = 1;
		}
		else if (gAgent.getBusy())
		{
			occupation = 2;
		}
		else if (gAgent.getAutoReply())
		{
			occupation = 3;
		}
		lua_pushinteger(state, occupation);
		lua_rawset(state, -3);

		lua_pushliteral(state, "flying");
		lua_pushboolean(state, gAgent.getFlying());
		lua_rawset(state, -3);

		lua_pushliteral(state, "sitting");
		lua_pushboolean(state, gAgentAvatarp->mIsSitting);
		lua_rawset(state, -3);

		lua_pushliteral(state, "sitting_on_ground");
		lua_pushboolean(state, gAgent.sittingOnGround());
		lua_rawset(state, -3);

		lua_pushliteral(state, "baked");
		lua_pushboolean(state, gAppearanceMgr.isAvatarFullyBaked());
		lua_rawset(state, -3);

		lua_pushliteral(state, "can_rebake_region");
		lua_pushboolean(state,
						gOverlayBarp && gOverlayBarp->canRebakeRegion());
		lua_rawset(state, -3);
//MK
		lua_pushliteral(state, "rlv");
		lua_pushboolean(state, gRLenabled);
		lua_rawset(state, -3);

		if (gRLenabled)
		{
			std::string restrictions = ",";
			rl_map_it_t it = gRLInterface.mSpecialObjectBehaviours.begin();
			while (it != gRLInterface.mSpecialObjectBehaviours.end())
			{
				temp = it++->second + ",";
				if (restrictions.find("," + temp) == std::string::npos)
				{
					restrictions += temp;
				}
			}
			if (restrictions != ",")
			{
				restrictions = restrictions.substr(1, restrictions.size() - 2);
			}
			else
			{
				restrictions.clear();
			}
			lua_pushliteral(state, "restrictions");
			lua_pushstring(state, restrictions.c_str());
			lua_rawset(state, -3);
		}
//mk
		LLEconomy* economyp = LLEconomy::getInstance();

		lua_pushliteral(state, "max_upload_cost");
		lua_pushinteger(state, economyp->getPriceUpload());
		lua_rawset(state, -3);

		lua_pushliteral(state, "animation_upload_cost");
		lua_pushinteger(state, economyp->getAnimationUploadCost());
		lua_rawset(state, -3);

		lua_pushliteral(state, "sound_upload_cost");
		lua_pushinteger(state, economyp->getSoundUploadCost());
		lua_rawset(state, -3);

		lua_pushliteral(state, "texture_upload_cost");
		lua_pushinteger(state, economyp->getTextureUploadCost());
		lua_rawset(state, -3);

		lua_pushliteral(state, "create_group_cost");
		lua_pushinteger(state, economyp->getCreateGroupCost());
		lua_rawset(state, -3);

		lua_pushliteral(state, "picks_limit");
		lua_pushinteger(state, economyp->getPicksLimit());
		lua_rawset(state, -3);

		lua_pushliteral(state, "group_membership_limit");
		lua_pushinteger(state, gMaxAgentGroups);
		lua_rawset(state, -3);

		lua_pushliteral(state, "attachment_limit");
		lua_pushinteger(state, gMaxSelfAttachments);
		lua_rawset(state, -3);

		lua_pushliteral(state, "animated_object_limit");
		lua_pushinteger(state,
						gAgentAvatarp->getMaxAnimatedObjectAttachments());
		lua_rawset(state, -3);
	}

	return 1;
}

//static
int HBViewerAutomation::setAgentOccupation(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("SetAgentOccupation");
	}

	S32 n = lua_gettop(state);
	if (n != 1)
	{
		luaL_error(state, "%d arguments passed; expected 1.", n);
	}

	S32 type = luaL_checknumber(state, 1);
	lua_pop(state, 1);

	HBIgnoreCallback lock_on_occupation(E_ONAGENTOCCUPATIONCHANGE);

	bool success = true;
	switch (type)
	{
		case 0:
			gAgent.clearAutoReply();
			gAgent.clearBusy();
			gAgent.clearAFK();
			break;

		case 1:
			gAgent.setAFK();
			break;

		case 2:
			gAgent.setBusy();
			break;

		case 3:
			gAgent.setAutoReply();
			break;

		default:
			success = false;
	}

	lua_pushboolean(state, success);

	return 1;
}

//static
int HBViewerAutomation::getAgentGroupData(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("GetAgentGroupData");
	}

	S32 n = lua_gettop(state);
	if (n > 1)
	{
		luaL_error(state, "%d arguments passed; expected 0 or 1.", n);
	}

	const LLUUID& current_group_id = gAgent.getGroupID();
	std::string group_name;
	if (n > 0)
	{
		group_name.assign(luaL_checkstring(state, 1));
		lua_pop(state, 1);
	}
	else
	{
		LLGroupData grp_data;
		if (gAgent.getGroupData(current_group_id, grp_data))
		{
			group_name = grp_data.mName;
		}
	}
	if (group_name.empty())
	{
		group_name = "none";
	}

	LL_DEBUGS("Lua") << "Searching group data for group: " << group_name
					 << LL_ENDL;

	lua_newtable(state);

	// The first time this method is called, we scan the whole agent groups
	// list, even after we found the right group, so to ensure that data for
	// all groups will have been loaded via gGroupMgr.fetchGroupMissingData().
	static bool scanned_once = false;

	U64 powers = 0;
	bool active_group = current_group_id.isNull() && group_name == "none";
	bool success = false;
	for (S32 i = 0, count = gAgent.mGroups.size(); i < count; ++i)
	{
		if (success && scanned_once)
		{
			// Stop scanning if we found the right group and already did a full
			// scan beforehand.
			break;
		}

		LLGroupData* gdatap = &gAgent.mGroups[i];
		if (!gdatap) continue;	// Paranoia

		const LLUUID& group_id = gdatap->mID;

		LLGroupMgrGroupData* mgrdatap = gGroupMgr.getGroupData(group_id);
		if (!mgrdatap)
		{
			gGroupMgr.fetchGroupMissingData(group_id);
			LL_DEBUGS("Lua") << "Group data not yet received for group Id: "
							 << group_id << LL_ENDL;
			continue;
		}

		if (!success && group_id.asString() == group_name)
		{
			group_name = gdatap->mName;
			// Make sure we get all the data for this group
			gGroupMgr.fetchGroupMissingData(group_id);
		}

		if (!success && gdatap->mName == group_name)
		{
			LL_DEBUGS("Lua") << "Found matching group name: " << group_name
							 << " - Group Id: " << group_id << LL_ENDL;
			lua_pushliteral(state, "group_id");
			lua_pushstring(state, group_id.asString().c_str());
			lua_rawset(state, -3);

			lua_pushliteral(state, "insignia_id");
			lua_pushstring(state, gdatap->mInsigniaID.asString().c_str());
			lua_rawset(state, -3);

			lua_pushliteral(state, "contribution");
			lua_pushinteger(state, gdatap->mContribution);
			lua_rawset(state, -3);

			lua_pushliteral(state, "in_profile");
			lua_pushboolean(state, gdatap->mListInProfile);
			lua_rawset(state, -3);

			lua_pushliteral(state, "accept_notices");
			lua_pushboolean(state, gdatap->mAcceptNotices);
			lua_rawset(state, -3);

			lua_pushliteral(state, "chat_muted");
			lua_pushboolean(state,
							LLMuteList::isMuted(group_id,
												LLMute::flagTextChat));
			lua_rawset(state, -3);

			lua_pushliteral(state, "founder_id");
			lua_pushstring(state, mgrdatap->mFounderID.asString().c_str());
			lua_rawset(state, -3);

			lua_pushliteral(state, "charter");
			lua_pushstring(state, mgrdatap->mCharter.c_str());
			lua_rawset(state, -3);

			lua_pushliteral(state, "fee");
			lua_pushinteger(state, mgrdatap->mMembershipFee);
			lua_rawset(state, -3);

			lua_pushliteral(state, "member_count");
			lua_pushinteger(state, mgrdatap->mMemberCount);
			lua_rawset(state, -3);

			lua_pushliteral(state, "open_enrollment");
			lua_pushboolean(state, mgrdatap->mOpenEnrollment);
			lua_rawset(state, -3);

			lua_pushliteral(state, "mature");
			lua_pushboolean(state, mgrdatap->mMaturePublish);
			lua_rawset(state, -3);

			lua_pushliteral(state, "members_list_ok");
			lua_pushboolean(state, mgrdatap->isMemberDataComplete());
			lua_rawset(state, -3);

			lua_pushliteral(state, "roles_list_ok");
			lua_pushboolean(state, mgrdatap->isRoleDataComplete() &&
								   mgrdatap->isRoleMemberDataComplete());
			lua_rawset(state, -3);

			lua_pushliteral(state, "properties_ok");
			lua_pushboolean(state, mgrdatap->isGroupPropertiesDataComplete());
			lua_rawset(state, -3);

			lua_pushliteral(state, "group_titles_ok");
			lua_pushboolean(state, mgrdatap->hasGroupTitles());
			lua_rawset(state, -3);

			powers = gdatap->mPowers;

			if (group_id == current_group_id)
			{
				LL_DEBUGS("Lua") << "Group is active" << LL_ENDL;
				active_group = true;
			}

			for (U32 j = 0, count2 = mgrdatap->mTitles.size(); j < count2; ++j)
			{
				const LLGroupTitle& title = mgrdatap->mTitles[j];
				const LLUUID& title_id = title.mRoleID;
				const std::string& title_name = title.mTitle;
				lua_pushstring(state, title_id.asString().c_str());
				lua_pushstring(state, title_name.c_str());
				lua_rawset(state, -3);
				LL_DEBUGS("Lua") << "Found group title: " << title_name
							 << " - Group title id: " << title_id << LL_ENDL;
				if (active_group &&	title.mSelected)
				{
					LL_DEBUGS("Lua") << "Group title is selected" << LL_ENDL;
					lua_pushliteral(state, "current_title_id");
					lua_pushstring(state, title_id.asString().c_str());
					lua_rawset(state, -3);
					lua_pushliteral(state, "current_title_name");
					lua_pushstring(state, title_name.c_str());
					lua_rawset(state, -3);
				}
			}

			success = true;
		}
	}

	scanned_once = true;

	lua_pushliteral(state, "name");
	lua_pushstring(state, group_name.c_str());
	lua_rawset(state, -3);

	if (!success)
	{
		LL_DEBUGS("Lua") << "Group not found" << LL_ENDL;
		lua_pushliteral(state, "group_id");
		lua_pushstring(state, LLUUID::null.asString().c_str());
		lua_rawset(state, -3);
	}

	lua_pushliteral(state, "powers");
	lua_pushinteger(state, powers);
	lua_rawset(state, -3);

	lua_pushliteral(state, "active");
	lua_pushboolean(state, active_group);
	lua_rawset(state, -3);

	return 1;
}

//static
int HBViewerAutomation::setAgentGroup(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("SetAgentGroup");
	}

	S32 n = lua_gettop(state);
	if (n > 2)
	{
		luaL_error(state, "%d arguments passed; expected 0 to 2.", n);
	}

	bool success = true;

	std::string param;
	LLUUID group_id;
	if (n > 0)
	{
		param = luaL_checkstring(state, 1);
		if (param != "none" && param != LLUUID::null.asString())
		{
			LL_DEBUGS("Lua") << "Searching a match in agent's groups for: "
							 << param << LL_ENDL;

			for (S32 i = 0, count = gAgent.mGroups.size(); i < count; ++i)
			{
				LLGroupData* gdatap = &gAgent.mGroups[i];
				if (!gdatap) continue;	// Paranoia

				const LLUUID& id = gdatap->mID;
				const std::string& name = gdatap->mName;
				if (name == param || id.asString() == param)
				{
					group_id = id;
					LL_DEBUGS("Lua") << "Found group Id: " << group_id
									 << LL_ENDL;
					break;
				}
			}
			success = group_id.notNull();
		}
	}

	LLUUID role_id;
	if (success && n > 1 && group_id.notNull())
	{
		param = luaL_checkstring(state, 2);

		LLGroupMgrGroupData* mgrdatap = gGroupMgr.getGroupData(group_id);
		if (mgrdatap)
		{
			LL_DEBUGS("Lua") << "Searching a match in roles for: " << param
							 << LL_ENDL;

			for (U32 i = 0, count = mgrdatap->mTitles.size(); i < count; ++i)
			{
				const LLGroupTitle& title = mgrdatap->mTitles[i];
				const LLUUID& title_id = title.mRoleID;
				const std::string& title_name = title.mTitle;
				if (title_name == param || title_id.asString() == param)
				{
					role_id = title_id;
					success = true;
					LL_DEBUGS("Lua") << "Found role Id: " << role_id
									 << LL_ENDL;
					break;
				}
			}
		}

		success = role_id.notNull();
		if (!success)
		{
			// Still try and set the group for now, at least...
			success = gAgent.setGroup(group_id);
			if (success)
			{
				LL_DEBUGS("Lua") << "Role/title not found; sending data requests for group Id "
								 << group_id
								 << ", with asynchronous title setting to: "
								 << param << LL_ENDL;
				gGroupMgr.fetchGroupMissingData(group_id);
				HBGroupTitlesObserver::addObserver(group_id, param);

				lua_pop(state, n);
				// Return a special value which is not 'true' since we could
				// not set the title for now, but not 'false' either, since it
				// may finally get set, asynchronously... The 'nil' value is
				// also compatible with the older versions of SetAgentGroup()
				// which used to give up and return 'false' in this case.
				lua_pushnil(state);
				return 1;
			}
		}
	}

	if (n)
	{
		lua_pop(state, n);
	}

	// Set the group if needed.
	if (success)
	{
		success = gAgent.setGroup(group_id);
		LL_DEBUGS("Lua") << "Setting agent group "
						 << (success ? "succeeded" : " failed") << LL_ENDL;
	}

	if (success && group_id.notNull() && role_id.notNull())
	{
		// Set the title for this group
		LL_DEBUGS("Lua") << "Setting agent group title" << LL_ENDL;
		gGroupMgr.sendGroupTitleUpdate(group_id, role_id);
	}

	lua_pushboolean(state, success);

	return 1;
}

//static
int HBViewerAutomation::agentGroupInvite(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("AgentGroupInvite");
	}

	S32 n = lua_gettop(state);
	if (n != 2 && n != 3)
	{
		luaL_error(state, "%d arguments passed; expected 2 or 3.", n);
	}

	U32 invited = 0;
	uuid_vec_t avids;
	if (lua_type(state, 1) == LUA_TTABLE)
	{
		for (U32 i = 1, count = lua_rawlen(state, 1); i <= count; ++i)
		{
			if (lua_rawgeti(state, 1, i) == LUA_TSTRING)
			{
				LLUUID id(luaL_checkstring(state, -1), false);
				if (id.notNull())
				{
					avids.emplace_back(id);
					++invited;
				}
			}
			lua_pop(state, 1);
		}
	}
	else
	{
		LLUUID id(luaL_checkstring(state, 1), false);
		if (id.notNull())
		{
			avids.emplace_back(id);
			invited = 1;
		}
	}

	LLUUID group_id(luaL_checkstring(state, 2), false);
	if (group_id.isNull())
	{
		llwarns << "Invalid (null) group Id passed." << llendl;
		invited = 0;
	}

	LLUUID role_id;
	if (n > 2)
	{
		role_id.set(luaL_checkstring(state, 2), false);
	}

	lua_pop(state, n);

	if (invited)
	{
		if (!gGroupMgr.agentCanAddToRole(group_id, role_id))
		{
			LL_DEBUGS("Lua") << "Cannot invite to group Id " << group_id
							 << " with role Id " << role_id << LL_ENDL;
			invited = 0;
		}
		else if (invited > MAX_GROUP_INVITES)
		{
			llwarns << "Too many simultaneous group invitations requested ("
					<< invited << ") to group Id: " << group_id
					<< ". Only the first " << MAX_GROUP_INVITES
					<< " invitations will be sent." << llendl;
			invited = MAX_GROUP_INVITES;
		}
	}

	if (invited)
	{
		LLGroupMgrGroupData* gdatap = gGroupMgr.getGroupData(group_id);
		LLGroupMgrGroupData::member_list_t::iterator mit;
		LLGroupMgrGroupData::member_list_t::iterator mend =
			gdatap->mMembers.end();
		LLGroupMgr::role_member_pairs_t invites;
		for (U32 i = 0; i < invited; ++i)
		{
			const LLUUID& id = avids[i];
	 		mit = gdatap->mMembers.find(id);
			// Do not re-invite a member in the role they already got...
			if (mit == mend || !mit->second->isInRole(role_id))
			{
				invites[id] = role_id;
			}
		}
		gGroupMgr.sendGroupMemberInvites(group_id, invites);
	}

	lua_pushinteger(state, invited);

	return 1;
}

//static
int HBViewerAutomation::agentSit(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("AgentSit");
	}

	S32 n = lua_gettop(state);
	if (n > 1)
	{
		luaL_error(state, "%d arguments passed; expected 0 or 1.", n);
	}

	if (n)
	{
		LLUUID object_id(luaL_checkstring(state, 1), false);
		if (object_id.isNull())
		{
			luaL_error(state, "Invalid object UUID passed as argument");
		}
		lua_pop(state, 1);

		LLViewerObject* object = gObjectList.findObject(object_id);
		lua_pushboolean(state, object && sit_on_object(object));
	}
	else
	{
		lua_pushboolean(state, sit_on_ground());
	}

	return 1;
}

//static
int HBViewerAutomation::agentStand(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("AgentStand");
	}

	S32 n = lua_gettop(state);
	if (n)
	{
		luaL_error(state, "%d arguments passed; expected 0.", n);
	}

	lua_pushboolean(state, stand_up());

	return 1;
}

//static
int HBViewerAutomation::setAgentTyping(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("SetAgentTyping");
	}

	S32 n = lua_gettop(state);
	if (n > 1)
	{
		luaL_error(state, "%d arguments passed; expected 0 or 1.", n);
	}

	bool start = true;
	if (n == 1)
	{
		start = lua_toboolean(state, 1);
		lua_pop(state, 1);
	}

	LL_DEBUGS("Lua") << "start=" << (start ? "true" : "false") << LL_ENDL;

	if (start)
	{
		gAgent.startTyping();
	}
	else
	{
		gAgent.stopTyping();
	}

	return 0;
}

//static
int HBViewerAutomation::sendChat(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("SendChat");
	}

	S32 n = lua_gettop(state);
	if (n != 1 && n != 2)
	{
		luaL_error(state, "%d arguments passed; expected 1 or 2.", n);
	}

	std::string message(luaL_checkstring(state, 1));

	std::string type;
	if (n > 1)
	{
		type.assign(luaL_checkstring(state, 2));
	}

	lua_pop(state, n);

	LL_DEBUGS("Lua") << "type=" << type << LL_ENDL;

	EChatType chat_type = CHAT_TYPE_NORMAL;
	if (type.find("whisper") != std::string::npos)
	{
		chat_type = CHAT_TYPE_WHISPER;
	}
	else if (type.find("shout") != std::string::npos)
	{
		chat_type = CHAT_TYPE_SHOUT;
	}

	bool animate = type.find("animate") != std::string::npos;

	if (gChatBarp)
	{
		HBIgnoreCallback lock_on_chat(E_ONSENDCHAT);
		gChatBarp->sendChatFromViewer(message, chat_type, animate, false);
	}

	return 0;
}

//static
int HBViewerAutomation::getIMSession(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self || !gIMMgrp) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("GetIMSession");
	}

	S32 n = lua_gettop(state);
	if (n != 1)
	{
		luaL_error(state, "%d arguments passed; expected 1.", n);
	}
	LLUUID target_id(luaL_checkstring(state, 1), false);
	lua_pop(state, 1);

	EInstantMessage dialog =
		gAgent.isInGroup(target_id, true) ? IM_SESSION_GROUP_START
										  : IM_NOTHING_SPECIAL;

	std::string name;
	if (gCacheNamep)
	{
		if (dialog == IM_SESSION_GROUP_START)
		{
			gCacheNamep->getGroupName(target_id, name);
		}
		else
		{
			gCacheNamep->getFullName(target_id, name);
		}
	}
	if (name.empty())
	{
		name = target_id.asString();
	}

	LLUUID session_id = gIMMgrp->addSession(name, dialog, target_id);

	LL_DEBUGS("Lua") << "target_id=" << target_id << " - target type: "
					 << (dialog == IM_SESSION_GROUP_START ? "group" : "agent")
					 << " - session_id=" << session_id << LL_ENDL;

	lua_pushstring(state, session_id.asString().c_str());

	return 1;
}

//static
int HBViewerAutomation::closeIMSession(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("CloseIMSession");
	}

	S32 n = lua_gettop(state);
	if (n != 1 && n != 2)
	{
		luaL_error(state, "%d arguments passed; expected 1 or 2.", n);
	}

	LLUUID session_id(luaL_checkstring(state, 1), false);
	U32 duration = 0;
	if (n > 1)
	{
		duration = llmax((S32)lua_tointeger(state, 2), 0);
	}
	lua_pop(state, n);

	if (session_id.notNull())
	{
		LLFloaterIMSession* im_floater =
			LLFloaterIMSession::findInstance(session_id);
		if (im_floater)
		{
			if (duration)
			{
				bool success = im_floater->setSnoozeDuration(duration);
				if (success)
				{
					LL_DEBUGS("Lua") << "Snoozing group IM session for: "
									 << duration << " minutes." << LL_ENDL;
				}
				else
				{
					llwarns << "Cannot snooze IM session: " << session_id
							<< ". Only group IM sessions may be snoozed. Leaving session instead."
							<< llendl;
				}
			}
			else
			{
				LL_DEBUGS("Lua") << "Closing IM session: " << session_id
								 << LL_ENDL;
			}
			im_floater->close();
		}
	}

	return 0;
}

//static
int HBViewerAutomation::sendIM(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("SendIM");
	}

	S32 n = lua_gettop(state);
	if (n != 2)
	{
		luaL_error(state, "%d arguments passed; expected 2.", n);
	}

	LLUUID session_id(luaL_checkstring(state, 1), false);
	std::string message(luaL_checkstring(state, 2));
	lua_pop(state, 2);

	if (session_id.notNull())
	{
		LLFloaterIMSession* im_floater =
			LLFloaterIMSession::findInstance(session_id);
		if (im_floater)
		{
			LL_DEBUGS("Lua") << "other_participant_id=" << session_id
							 << LL_ENDL;
			HBIgnoreCallback lock_on_im(E_ONINSTANTMSG);
			im_floater->sendText(utf8str_to_wstring(message));
		}
	}

	return 0;
}

//static
int HBViewerAutomation::scriptDialogResponse(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("ScriptDialogResponse");
	}

	S32 n = lua_gettop(state);
	if (n != 2)
	{
		luaL_error(state, "%d arguments passed; expected 2.", n);
	}

	LLUUID notif_id(luaL_checkstring(state, 1), false);
	std::string button(luaL_checkstring(state, 2));
	lua_pop(state, 2);

	if (notif_id.notNull() && !button.empty())
	{
		LLNotifyBox* boxp = LLNotifyBox::getNamedInstance(notif_id).get();
		if (boxp && !boxp->isDead())
		{
			const LLNotifyBox::cb_data_vec_t& data = boxp->getCallbackData();
			for (S32 i = 0, count = data.size(); i < count; ++i)
			{
				if (data[i]->mButtonName == button)
				{
					LLSD response =
						boxp->getNotification()->getResponseTemplate();
					if (!boxp->isDefaultBtnAdded())
					{
						response[button] = true;
					}
					boxp->getNotification()->respond(response);
					lua_pushboolean(state, true);
					return 1;
				}
			}
		}
	}

	lua_pushboolean(state, false);

	return 1;
}

//static
int HBViewerAutomation::cancelNotification(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("CancelNotification");
	}

	S32 n = lua_gettop(state);
	if (n != 1)
	{
		luaL_error(state, "%d arguments passed; expected 1.", n);
	}

	LLUUID notif_id(luaL_checkstring(state, 1), false);
	lua_pop(state, 1);

	if (notif_id.notNull())
	{
		LLNotificationPtr n = gNotifications.find(notif_id);
		if (n)
		{
			gNotifications.cancel(n);
			lua_pushboolean(state, true);
			return 1;
		}
	}

	lua_pushboolean(state, false);

	return 1;
}

//static
int HBViewerAutomation::getObjectInfo(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("GetObjectInfo");
	}

	S32 n = lua_gettop(state);
	if (n != 1)
	{
		luaL_error(state, "%d arguments passed; expected 1.", n);
	}

	LLUUID object_id(luaL_checkstring(state, 1), false);
	lua_pop(state, 1);

	lua_pushboolean(state, requestObjectPropertiesFamily(object_id, 2));

	return 1;
}

//static
int HBViewerAutomation::browseToURL(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("BrowseToURL");
	}

	S32 n = lua_gettop(state);
	if (n != 1 && n != 2)
	{
		luaL_error(state, "%d arguments passed; expected 1 or 2.", n);
	}

	std::string url(luaL_checkstring(state, 1));

	S32 browser = 0;
	if (n > 1)
	{
		browser = luaL_checknumber(state, 2);
	}

	lua_pop(state, n);

	LL_DEBUGS("Lua") << "Browsing with "
					 << (browser ? (browser == 1 ? "built-in" : "external")
								 : "preferred")
					 << " browser to URL: " << url << LL_ENDL;
	switch (browser)
	{
		case 1:
			LLWeb::loadURLInternal(url);
			break;

		case 2:
			LLWeb::loadURLExternal(url);
			break;

		default:
			LLWeb::loadURL(url);
	}

	return 0;
}

//static
int HBViewerAutomation::dispatchSLURL(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("DispatchSLURL");
	}

	S32 n = lua_gettop(state);
	if (n != 1 && n != 2)
	{
		luaL_error(state, "%d arguments passed; expected 1 or 2.", n);
	}

	std::string slurl(luaL_checkstring(state, 1));

	bool trusted = false;
	if (n > 1)
	{
		trusted = lua_toboolean(state, 2);
	}

	lua_pop(state, n);

	LL_DEBUGS("Lua") << "Dispatching ("
					 << (trusted ? "trusted" : "untrusted") << "): " << slurl
					 << LL_ENDL;

	LLURLDispatcher::dispatch(slurl, "clicked", NULL, trusted);

	return 0;
}

//static
int HBViewerAutomation::executeRLV(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("ExecuteRLV");
	}

	S32 n = lua_gettop(state);
	if (n != 1)
	{
		luaL_error(state, "%d arguments passed; expected 1.", n);
	}

	std::string rlvcmd(luaL_checkstring(state, 1));
	lua_pop(state, 1);

	if (gRLenabled && rlvcmd.length())
	{
		LLStringUtil::toLower(rlvcmd);

		LL_DEBUGS("Lua") << "Executing RLV command: \"" << rlvcmd
						 << "\" on behalf of: " << self->mFromObjectName
						 << LL_ENDL;

		gRLInterface.queueCommands(self->mFromObjectId, self->mFromObjectName,
								   rlvcmd);
	}

	return 0;
}

//static
int HBViewerAutomation::openNotification(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("OpenNotification");
	}

	S32 n = lua_gettop(state);
	if (n != 2)
	{
		luaL_error(state, "%d arguments passed; expected 2.", n);
	}

	S32 type = luaL_checknumber(state, 1);
	std::string message(luaL_checkstring(state, 2));
	lua_pop(state, 2);

	std::string name;
	switch (type)
	{
		case ALERT:
			name = "LuaAlert";
			break;

		case NOTIFICATION:
			name = "LuaNotification";
			break;

		case NOTIFYTIP:
			name = "LuaNotifyTip";
			break;

		default:
			luaL_error(state, "Unknown notification type !");
	}

	LL_DEBUGS("Lua") << "Notification type: " << name << LL_ENDL;

	LLSD args;
	args["MESSAGE"] = message;
	gNotifications.add(name, args);

	return 0;
}

//static
int HBViewerAutomation::openFloater(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("OpenFloater");
	}

	S32 n = lua_gettop(state);
	if (n != 1 && n != 2)
	{
		luaL_error(state, "%d arguments passed; expected 1 or 2.", n);
	}

	std::string name(luaL_checkstring(state, 1));

	std::string param;
	LLUUID target_id;
	if (n == 2)
	{
		param.assign(luaL_checkstring(state, 2));
		if (LLUUID::validate(param))
		{
			target_id.set(param);
		}
	}

	lua_pop(state, n);

	LL_DEBUGS("Lua") << "Floater: " << name << " - parameter: " << param
					 << LL_ENDL;

	if (name == "active speakers")
	{
		LLFloaterActiveSpeakers::showInstance();
	}
	else if (name == "area search")
	{
		HBFloaterAreaSearch::showInstance();
	}
	else if (name == "beacons")
	{
		LLFloaterBeacons::showInstance();
	}
	else if (name == "avatar info")
	{
		if (self && gObjectList.findAvatar(target_id))
		{
			LLFloaterAvatarInfo::show(target_id);
		}
	}
	else if (name == "camera controls")
	{
		LLFloaterCamera::showInstance();
	}
	else if (name == "chat")
	{
		LLFloaterChat::showInstance();
	}
	else if (name == "debug settings")
	{
		LLFloaterDebugSettings::showInstance();
	}
	else if (name == "debug tags")
	{
		HBFloaterDebugTags::showInstance();
	}
	else if (name == "experiences")
	{
		LLFloaterExperiences::showInstance();
	}
	else if (name == "friends")
	{
		LLFloaterFriends::showInstance();
	}
	else if (name == "gestures")
	{
		LLFloaterGesture::showInstance();
	}
	else if (name == "group info")
	{
		if (self && gAgent.isInGroup(target_id))
		{
			LLFloaterGroupInfo::showFromUUID(target_id);
		}
	}
	else if (name == "groups")
	{
		LLFloaterGroups::showInstance();
	}
	else if (name == "inspect")
	{
		LLViewerObject* object = gObjectList.findObject(target_id);
		if (object)
		{
			if (object->isAvatar())
			{
				HBFloaterInspectAvatar::show(target_id);
			}
			else
			{
				LLFloaterInspect::show(object);
			}
		}
	}
	else if (name == "instant messages")
	{
		LLFloaterChatterBox::showInstance();
	}
	else if (name == "inventory")
	{
		if (!gSavedSettings.getBool("ShowInventory"))
		{
			LLFloaterInventory::toggleVisibility();
		}
	}
	else if (name == "land")
	{
		if (!gRLenabled || !gRLInterface.mContainsShowloc)
		{
			if (gViewerParcelMgr.selectionEmpty())
			{
				gViewerParcelMgr.selectParcelAt(gAgent.getPositionGlobal());
			}
			LLFloaterLand::showInstance();
		}
	}
	else if (name == "land holdings")
	{
		LLFloaterLandHoldings::showInstance();
	}
	else if (name == "map")
	{
		if (!gSavedSettings.getBool("ShowWorldMap"))
		{
			LLFloaterWorldMap::toggle(NULL);
		}
	}
	else if (name == "media filter")
	{
		SLFloaterMediaFilter::showInstance();
	}
	else if (name == "mini map")
	{
		LLFloaterMiniMap::showInstance();
	}
	else if (name == "movement controls")
	{
		LLFloaterMove::showInstance();
	}
	else if (name == "mute list")
	{
		if (target_id.notNull())
		{
			LLFloaterMute::selectMute(target_id);
		}
		else
		{
			LLFloaterMute::selectMute(param);
		}
	}
	else if (name == "nearby media")
	{
		LLFloaterNearByMedia::showInstance();
	}
	else if (name == "notifications")
	{
		LLFloaterNotificationConsole::showInstance();
	}
	else if (name == "characters")
	{
		LLFloaterPathfindingCharacters::openCharactersWithSelectedObjects();
	}
	else if (name == "linksets")
	{
		LLFloaterPathfindingLinksets::openLinksetsWithSelectedObjects();
	}
	else if (name == "preferences")
	{
		S32 tab = param.empty() ? -1 : atoi(param.c_str());
		if (tab >= 0 && tab < LLFloaterPreference::NUMBER_OF_TABS)
		{
			LLFloaterPreference::openInTab(tab);
		}
		else
		{
			LLFloaterPreference::showInstance();
		}
	}
	else if (name == "pushes")
	{
		HBFloaterBump::showInstance();
	}
	else if (name == "radar")
	{
		HBFloaterRadar::showInstance();
	}
	else if (name == "region")
	{
		if (!gRLenabled || !gRLInterface.mContainsShowloc)
		{
			LLFloaterRegionInfo::showInstance();
		}
	}
	else if (name == "search")
	{
		if (!gSavedSettings.getBool("ShowSearch"))
		{
			HBFloaterSearch::toggle();
		}
	}
	else if (name == "snapshot")
	{
		LLFloaterSnapshot::show(NULL);
	}
	else if (name == "sounds list")
	{
		HBFloaterSoundsList::showInstance();
	}
	else if (name == "stats")
	{
		LLFloaterStats::showInstance();
	}
	else if (name == "teleport history")
	{
		if (gFloaterTeleportHistoryp &&
			!gFloaterTeleportHistoryp->getVisible())
		{
			gFloaterTeleportHistoryp->toggle();
		}
	}

	return 0;
}

//static
int HBViewerAutomation::closeFloater(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("CloseFloater");
	}

	S32 n = lua_gettop(state);
	if (n != 1)
	{
		luaL_error(state, "%d arguments passed; expected 1.", n);
	}

	std::string name(luaL_checkstring(state, 1));
	lua_pop(state, 1);

	LL_DEBUGS("Lua") << "Floater: " << name << LL_ENDL;

	if (name == "active speakers")
	{
		LLFloaterActiveSpeakers::hideInstance();
	}
	else if (name == "area search")
	{
		HBFloaterAreaSearch::hideInstance();
	}
	else if (name == "beacons")
	{
		LLFloaterBeacons::hideInstance();
	}
	else if (name == "camera controls")
	{
		LLFloaterCamera::hideInstance();
	}
	else if (name == "chat")
	{
		LLFloaterChat::hideInstance();
	}
	else if (name == "debug settings")
	{
		LLFloaterDebugSettings::hideInstance();
	}
	else if (name == "debug tags")
	{
		HBFloaterDebugTags::hideInstance();
	}
	else if (name == "experiences")
	{
		LLFloaterExperiences::hideInstance();
	}
	else if (name == "friends")
	{
		LLFloaterFriends::hideInstance();
	}
	else if (name == "gestures")
	{
		LLFloaterGesture::hideInstance();
	}
	else if (name == "groups")
	{
		LLFloaterGroups::hideInstance();
	}
	else if (name == "inspect object")
	{
		LLFloaterInspect::hideInstance();
	}
	else if (name == "inspect avatar")
	{
		HBFloaterInspectAvatar::hideInstance();
	}
	else if (name == "instant messages")
	{
		LLFloaterChatterBox::hideInstance();
	}
	else if (name == "inventory")
	{
		if (gSavedSettings.getBool("ShowInventory"))
		{
			LLFloaterInventory::toggleVisibility();
		}
	}
	else if (name == "land")
	{
		LLFloaterLand::hideInstance();
	}
	else if (name == "land holdings")
	{
		LLFloaterLandHoldings::hideInstance();
	}
	else if (name == "map")
	{
		if (gSavedSettings.getBool("ShowWorldMap"))
		{
			LLFloaterWorldMap::toggle(NULL);
		}
	}
	else if (name == "media filter")
	{
		SLFloaterMediaFilter::hideInstance();
	}
	else if (name == "mini map")
	{
		LLFloaterMiniMap::hideInstance();
	}
	else if (name == "movement controls")
	{
		LLFloaterMove::hideInstance();
	}
	else if (name == "mute list")
	{
		LLFloaterMute::hideInstance();
	}
	else if (name == "nearby media")
	{
		LLFloaterNearByMedia::hideInstance();
	}
	else if (name == "notifications")
	{
		LLFloaterNotificationConsole::hideInstance();
	}
	else if (name == "characters")
	{
		LLFloaterPathfindingCharacters::hideInstance();
	}
	else if (name == "linksets")
	{
		LLFloaterPathfindingLinksets::hideInstance();
	}
	else if (name == "preferences")
	{
		LLFloaterPreference::hideInstance();
	}
	else if (name == "pushes")
	{
		HBFloaterBump::hideInstance();
	}
	else if (name == "radar")
	{
		HBFloaterRadar::hideInstance();
	}
	else if (name == "region")
	{
		LLFloaterRegionInfo::hideInstance();
	}
	else if (name == "search")
	{
		if (gSavedSettings.getBool("ShowSearch"))
		{
			HBFloaterSearch::toggle();
		}
	}
	else if (name == "snapshot")
	{
		LLFloaterSnapshot::hide(NULL);
	}
	else if (name == "sounds list")
	{
		HBFloaterSoundsList::hideInstance();
	}
	else if (name == "stats")
	{
		LLFloaterStats::hideInstance();
	}
	else if (name == "teleport history")
	{
		if (gFloaterTeleportHistoryp &&
			gFloaterTeleportHistoryp->getVisible())
		{
			gFloaterTeleportHistoryp->toggle();
		}
	}

	return 0;
}

#if HB_LUA_FLOATER_FUNCTIONS
//static
int HBViewerAutomation::getFloaterInstances(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	if (!state) return 0;	// Paranoia

	S32 n = lua_gettop(state);
	if (n > 1)
	{
		luaL_error(state, "%d arguments passed; expected 0 or 1.", n);
	}
	std::string match;
	if (n)
	{
		match = luaL_checkstring(state, 1);
		lua_pop(state, 1);
	}

	lua_newtable(state);
	std::string name, title;
	using iter_t = LLView::child_list_const_iter_t;
	for (iter_t it = gFloaterViewp->getChildList()->begin(),
				end = gFloaterViewp->getChildList()->end();
		 it != end; ++it)
	{
		LLFloater* floaterp = (*it)->asFloater();
		if (floaterp)
		{
			name = floaterp->getName();
			if (!match.empty() && name != match)
			{
				continue;
			}
			if (!floaterp->isTitlePristine())
			{
				title = floaterp->getTitle();
				if (!title.empty() && stricmp(name.c_str(), title.c_str()))
				{
					name += "=" + title;
				}
			}
			lua_pushstring(state, name.c_str());
			lua_rawseti(state, -2, floaterp->getId());
		}
	}
	return 1;
}

static LLFloater* get_floater_by_id(S32 id)
{
	if (id > 0)
	{
		using iter_t = LLView::child_list_const_iter_t;
		for (iter_t it = gFloaterViewp->getChildList()->begin(),
					end = gFloaterViewp->getChildList()->end();
			 it != end; ++it)
		{
			LLFloater* floaterp = (*it)->asFloater();
			if (floaterp && floaterp->getId() == (U32)id)
			{
				return floaterp;
			}
		}
	}
	return NULL;
}

//static
int HBViewerAutomation::showFloater(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	if (!state) return 0;	// Paranoia

	S32 n = lua_gettop(state);
	if (n != 1)
	{
		luaL_error(state, "%d arguments passed; expected 1.", n);
	}
	S32 id = luaL_checknumber(state, 1);
	lua_pop(state, 1);

	LLFloater* floaterp = get_floater_by_id(id);
	if (floaterp)
	{
		floaterp->open();
	}
	lua_pushboolean(state, floaterp != NULL);
	return 1;
}

//static
int HBViewerAutomation::getFloaterButtons(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	if (!state) return 0;	// Paranoia

	S32 n = lua_gettop(state);
	if (n != 1)
	{
		luaL_error(state, "%d arguments passed; expected 1.", n);
	}
	S32 id = luaL_checknumber(state, 1);
	lua_pop(state, 1);

	LLFloater* floaterp = get_floater_by_id(id);
	if (!floaterp || !floaterp->getVisible())
	{
		lua_pushnil(state);
		return 1;
	}

	lua_newtable(state);
	std::string name;
	using iter_t = LLView::child_list_const_iter_t;
	for (iter_t it = floaterp->getChildList()->begin(),
				end = floaterp->getChildList()->end();
		 it != end; ++it)
	{
		LLButton* buttonp = dynamic_cast<LLButton*>(*it);
		if (buttonp && buttonp->getVisible())
		{
			name = buttonp->getName();
			lua_pushstring(state, name.c_str());
			lua_pushboolean(state, buttonp->getEnabled());
			lua_rawset(state, -3);
		}
	}
	return 1;
}

//static
int HBViewerAutomation::getFloaterCheckBoxes(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	if (!state) return 0;	// Paranoia

	S32 n = lua_gettop(state);
	if (n != 1)
	{
		luaL_error(state, "%d arguments passed; expected 1.", n);
	}
	S32 id = luaL_checknumber(state, 1);
	lua_pop(state, 1);

	LLFloater* floaterp = get_floater_by_id(id);
	if (!floaterp || !floaterp->getVisible())
	{
		lua_pushnil(state);
		return 1;
	}

	lua_newtable(state);
	std::string name;
	using iter_t = LLView::child_list_const_iter_t;
	for (iter_t it = floaterp->getChildList()->begin(),
				end = floaterp->getChildList()->end();
		 it != end; ++it)
	{
		LLCheckBoxCtrl* checkp = dynamic_cast<LLCheckBoxCtrl*>(*it);
		if (checkp && checkp->getVisible() && checkp->getEnabled())
		{
			name = checkp->getName();
			lua_pushstring(state, name.c_str());
			lua_pushboolean(state, checkp->get());
			lua_rawset(state, -3);
		}
	}
	return 1;
}
#endif

//static
int HBViewerAutomation::makeDialog(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("MakeDialog");
	}

	S32 n = lua_gettop(state);
	if (n != 9)
	{
		luaL_error(state, "%d arguments passed; expected 9.", n);
	}

	std::string params[9];
	for (S32 i = 0; i < 9; ++i)
	{
		params[i] = luaL_checkstring(state, i + 1);
	}

	lua_pop(state, 9);

	HBLuaDialog::create(params[0], params[1], params[2], params[3], params[4],
						params[5], params[6], params[7], params[8]);

	return 0;
}

//static
int HBViewerAutomation::openLuaFloater(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("OpenLuaFloater");
	}

	S32 n = lua_gettop(state);
	if (n < 1 || n > 4)
	{
		luaL_error(state, "%d arguments passed; expected 1 to 4.", n);
	}

	std::string name(luaL_checkstring(state, 1));
	std::string param, pos;
	if (n >= 2)
	{
		param.assign(luaL_checkstring(state, 2));
	}
	if (n >= 3)
	{
		pos.assign(luaL_checkstring(state, 3));
	}
	bool open = true;
	if (n == 4)
	{
		open = lua_toboolean(state, 4);
	}
	lua_pop(state, n);

	lua_pushboolean(state,
					HBLuaFloater::create(name, param, pos, open) != NULL);

	return 1;
}
//static
int HBViewerAutomation::showLuaFloater(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("ShowLuaFloater");
	}

	S32 n = lua_gettop(state);
	if (n != 1 && n != 2)
	{
		luaL_error(state, "%d arguments passed; expected 1 or 2.", n);
	}

	std::string name(luaL_checkstring(state, 1));
	bool show = true;
	if (n == 2)
	{
		show = lua_toboolean(state, 2);
	}
	lua_pop(state, n);

	lua_pushboolean(state, HBLuaFloater::setVisible(name, show));

	return 1;
}

//static
int HBViewerAutomation::closeLuaFloater(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("CloseLuaFloater");
	}

	S32 n = lua_gettop(state);
	if (n != 1)
	{
		luaL_error(state, "%d arguments passed; expected 1.", n);
	}

	std::string name(luaL_checkstring(state, 1));
	lua_pop(state, 1);

	// This will call the OnLuaFloaterClose() callback if
	// CloseLuaFloater() was not invoked from the automation script.
	HBLuaFloater::destroy(name, self != gAutomationp);

	return 0;
}

//static
int HBViewerAutomation::setLuaFloaterCommand(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("SetLuaFloaterCommand");
	}

	S32 n = lua_gettop(state);
	if (n != 3)
	{
		luaL_error(state, "%d arguments passed; expected 3.", n);
	}

	std::string floater_name(luaL_checkstring(state, 1));
	std::string ctrl_name(luaL_checkstring(state, 2));
	std::string command(luaL_checkstring(state, 3));
	lua_pop(state, 3);

	lua_pushboolean(state,
					HBLuaFloater::setControlCallback(floater_name,
													 ctrl_name, command));
	return 1;
}

//static
int HBViewerAutomation::getLuaFloaterValue(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("GetLuaFloaterValue");
	}

	S32 n = lua_gettop(state);
	if (n != 2)
	{
		luaL_error(state, "%d arguments passed; expected 2.", n);
	}

	std::string floater_name(luaL_checkstring(state, 1));
	std::string ctrl_name(luaL_checkstring(state, 2));
	lua_pop(state, 2);

	std::string value;
	if (HBLuaFloater::getControlValue(floater_name, ctrl_name, value))
	{
		lua_pushstring(state, value.c_str());
	}
	else
	{
		lua_pushnil(state);
	}

	return 1;
}

//static
int HBViewerAutomation::getLuaFloaterValues(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("GetLuaFloaterValues");
	}

	S32 n = lua_gettop(state);
	if (n != 2)
	{
		luaL_error(state, "%d arguments passed; expected 2.", n);
	}

	std::string floater_name(luaL_checkstring(state, 1));
	std::string ctrl_name(luaL_checkstring(state, 2));
	lua_pop(state, 2);

	std::vector<std::string> values;
	if (HBLuaFloater::getControlValues(floater_name, ctrl_name, values))
	{
		lua_newtable(state);
		for (S32 i = 0, count = values.size(); i < count; ++i)
		{
			lua_pushstring(state, values[i].c_str());
			lua_rawseti(state, -2, i + 1);
		}
	}
	else
	{
		lua_pushnil(state);
	}

	return 1;
}

//static
int HBViewerAutomation::setLuaFloaterValue(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("SetLuaFloaterValue");
	}

	S32 n = lua_gettop(state);
	if (n != 3)
	{
		luaL_error(state, "%d arguments passed; expected 3.", n);
	}

	std::string floater_name(luaL_checkstring(state, 1));
	std::string ctrl_name(luaL_checkstring(state, 2));
	std::string value(luaL_checkstring(state, 3));
	lua_pop(state, n);

	lua_pushboolean(state,
					HBLuaFloater::setControlValue(floater_name, ctrl_name,
												  value));

	return 1;
}

//static
int HBViewerAutomation::setLuaFloaterEnabled(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("SetLuaFloaterEnabled");
	}

	S32 n = lua_gettop(state);
	if (n != 2 && n != 3)
	{
		luaL_error(state, "%d arguments passed; expected 2 or 3.", n);
	}

	std::string floater_name(luaL_checkstring(state, 1));
	std::string ctrl_name(luaL_checkstring(state, 2));
	bool enable = true;
	if (n == 3)
	{
		enable = lua_toboolean(state, 3);
	}
	lua_pop(state, n);

	lua_pushboolean(state,
					HBLuaFloater::setControlEnabled(floater_name, ctrl_name,
													enable));

	return 1;
}

//static
int HBViewerAutomation::setLuaFloaterVisible(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("SetLuaFloaterVisible");
	}

	S32 n = lua_gettop(state);
	if (n != 2 && n != 3)
	{
		luaL_error(state, "%d arguments passed; expected 2 or 3.", n);
	}

	std::string floater_name(luaL_checkstring(state, 1));
	std::string ctrl_name(luaL_checkstring(state, 2));
	bool visible = true;
	if (n == 3)
	{
		visible = lua_toboolean(state, 3);
	}
	lua_pop(state, n);

	lua_pushboolean(state,
					HBLuaFloater::setControlVisible(floater_name, ctrl_name,
													visible));

	return 1;
}

//static
int HBViewerAutomation::overlayBarLuaButton(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("OverlayBarLuaButton");
	}

	S32 n = lua_gettop(state);
	if (n != 2 && n != 3)
	{
		luaL_error(state, "%d arguments passed; expected 2 or 3.", n);
	}

	std::string label(luaL_checkstring(state, 1));
	std::string command(luaL_checkstring(state, 2));
	std::string tooltip;
	if (n == 3)
	{
		tooltip.assign(luaL_checkstring(state, 3));
	}
	lua_pop(state, n);

	if (gOverlayBarp)
	{
		gOverlayBarp->setLuaFunctionButton(label, command, tooltip);
	}

	return 0;
}

//static
int HBViewerAutomation::statusBarLuaIcon(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("StatusBarLuaIcon");
	}

	S32 n = lua_gettop(state);
	if (n != 1 && n != 2)
	{
		luaL_error(state, "%d arguments passed; expected 1 or 2.", n);
	}

	std::string command(luaL_checkstring(state, 1));
	std::string tooltip;
	if (n == 2)
	{
		tooltip.assign(luaL_checkstring(state, 2));
	}
	lua_pop(state, n);

	if (gStatusBarp)
	{
		gStatusBarp->setLuaFunctionButton(command, tooltip);
	}

	return 0;
}

//static
int HBViewerAutomation::sideBarButton(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("SideBarButton");
	}

	S32 n = lua_gettop(state);
	if (n == 0 || n > 4)
	{
		luaL_error(state, "%d arguments passed; expected 1 to 4.", n);
	}

	U32 number = luaL_checknumber(state, 1);
	std::string icon, tooltip, command;
	if (n > 1)
	{
		icon.assign(luaL_checkstring(state, 2));
		if (!icon.empty() && n > 2)
		{
			command.assign(luaL_checkstring(state, 3));
			if (n > 3)
			{
				tooltip.assign(luaL_checkstring(state, 4));
			}
		}
	}
	lua_pop(state, n);

	U32 result = 0;
	if (gLuaSideBarp)
	{
		result = gLuaSideBarp->setButton(number, icon, command, tooltip);
	}
	lua_pushinteger(state, result);

	return 1;
}

//static
int HBViewerAutomation::sideBarButtonToggle(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("SideBarButtonToggle");
	}

	S32 n = lua_gettop(state);
	if (n == 0 || n > 2)
	{
		luaL_error(state, "%d arguments passed; expected 1 or 2.", n);
	}

	U32 number = luaL_checknumber(state, 1);

	S32 toggle = -1;
	if (n == 2)
	{
		int type = lua_type(state, 2);
		if (type == LUA_TNIL || type == LUA_TSTRING)
		{
			LLControlVariable* control = NULL;
			std::string control_name;
			if (type == LUA_TSTRING)
			{
				control_name.assign(luaL_checkstring(state, 2));
			}
			if (!control_name.empty())
			{
				control = gSavedSettings.getControl(control_name.c_str());
				if (!control)
				{
					control = gSavedPerAccountSettings.getControl(control_name.c_str());
				}
				if (!control)
				{
					luaL_error(state, "No setting named: %s",
							   control_name.c_str());
				}
				if (control->type() != TYPE_BOOLEAN)
				{
					luaL_error(state, "Setting '%s' is not of boolean type",
							   control_name.c_str());
				}
			}
			if (gLuaSideBarp)
			{
				gLuaSideBarp->buttonSetControl(number, control);
			}
		}
		else
		{
			toggle = lua_toboolean(state, 2) ? 1 : 0;
		}
	}
	lua_pop(state, n);

	if (gLuaSideBarp)
	{
		toggle = gLuaSideBarp->buttonToggle(number, toggle);
	}

	if (toggle == -1)
	{
		lua_pushnil(state);
	}
	else
	{
		lua_pushboolean(state, toggle);
	}

	return 1;
}

//static
int HBViewerAutomation::sideBarHide(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("SideBarHide");
	}

	S32 n = lua_gettop(state);
	if (n > 1)
	{
		luaL_error(state, "%d arguments passed; expected 0 or 1.", n);
	}

	bool hide = true;
	if (n == 1)
	{
		hide = lua_toboolean(state, 1);
	}
	lua_pop(state, 1);

	if (gLuaSideBarp)
	{
		gLuaSideBarp->setHidden(hide);
	}

	return 0;
}

//static
int HBViewerAutomation::sideBarHideOnRightClick(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("SideBarHideOnRightClick");
	}

	S32 n = lua_gettop(state);
	if (n > 1)
	{
		luaL_error(state, "%d arguments passed; expected 0 or 1.", n);
	}

	bool hide = true;
	if (n == 1)
	{
		hide = lua_toboolean(state, 1);
	}
	lua_pop(state, 1);

	if (gLuaSideBarp)
	{
		gLuaSideBarp->hideOnRightClick(hide);
	}

	return 0;
}

//static
int HBViewerAutomation::sideBarButtonHide(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("SideBarButtonHide");
	}

	S32 n = lua_gettop(state);
	if (n != 1 && n != 2)
	{
		luaL_error(state, "%d arguments passed; expected 1 or 2.", n);
	}

	U32 number = luaL_checknumber(state, 1);
	bool hide = true;
	if (n > 1)
	{
		hide = lua_toboolean(state, 2);
	}
	lua_pop(state, n);

	if (gLuaSideBarp)
	{
		gLuaSideBarp->setButtonVisible(number, !hide);
	}

	return 0;
}

//static
int HBViewerAutomation::sideBarButtonDisable(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("SideBarButtonDisable");
	}

	S32 n = lua_gettop(state);
	if (n != 1 && n != 2)
	{
		luaL_error(state, "%d arguments passed; expected 1 or 2.", n);
	}

	U32 number = luaL_checknumber(state, 1);
	bool disable = true;
	if (n > 1)
	{
		disable = lua_toboolean(state, 2);
	}
	lua_pop(state, n);

	if (gLuaSideBarp)
	{
		gLuaSideBarp->setButtonEnabled(number, !disable);
	}

	return 0;
}

//static
int HBViewerAutomation::luaPieMenuSlice(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("LuaPieMenuSlice");
	}

	S32 n = lua_gettop(state);
	if (n == 0 || n > 4)
	{
		luaL_error(state, "%d arguments passed; expected 1 to 4.", n);
	}

	S32 type = luaL_checknumber(state, 1);
	U32 slice = 0;
	std::string label, command;
	if (n > 1)
	{
		slice = luaL_checknumber(state, 2);
		if (slice && n > 2)
		{
			label.assign(luaL_checkstring(state, 3));
			if (n > 3)
			{
				command.assign(luaL_checkstring(state, 4));
			}
		}
	}
	lua_pop(state, n);

	if (gLuaPiep)
	{
		gLuaPiep->setSlice(type, slice, label, command);
	}

	return 0;
}

//static
int HBViewerAutomation::luaContextMenu(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("LuaContextMenu");
	}

	S32 n = lua_gettop(state);
	if (n == 0 || n > 4)
	{
		luaL_error(state, "%d arguments passed; expected 1 to 4.", n);
	}

	S32 id = luaL_checknumber(state, 1);
	std::string cut_label, copy_label, paste_label;
	if (n > 1)
	{
		cut_label = luaL_checkstring(state, 2);
		if (n > 2)
		{
			copy_label.assign(luaL_checkstring(state, 3));
			if (n > 3)
			{
				paste_label.assign(luaL_checkstring(state, 4));
			}
		}
	}
	lua_pop(state, n);

	lua_pushboolean(state,
					LLEditMenuHandler::setCustomMenu(id, cut_label, copy_label,
													 paste_label));
	return 1;
}

//static
int HBViewerAutomation::pasteToContextHandler(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("PasteToContextHandler");
	}

	S32 n = lua_gettop(state);
	if (n != 1 && n != 2)
	{
		luaL_error(state, "%d arguments passed; expected 1 or 2.", n);
	}

	S32 id = luaL_checknumber(state, 1);
	if (n > 1)
	{
		std::string text(luaL_checkstring(state, 2));
		gClipboard.copyFromSubstring(utf8str_to_wstring(text), 0,
									 text.length());
	}
	lua_pop(state, n);

	lua_pushboolean(state, LLEditMenuHandler::pasteTo(id));
	return 1;
}

//static
int HBViewerAutomation::automationMessage(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	S32 n = lua_gettop(state);
	if (n != 1)
	{
		luaL_error(state, "%d arguments passed; expected 1.", n);
	}
	std::string text(luaL_checkstring(state, 1));
	lua_pop(state, 1);

	if (gAutomationp && gAutomationp->mHasOnAutomationMessage)
	{
		LL_DEBUGS("Lua") << "Invoking OnAutomationMessage Lua callback. text="
						 << text << LL_ENDL;

		lua_getglobal(gAutomationp->mLuaState, "OnAutomationMessage");
		lua_pushstring(gAutomationp->mLuaState, text.c_str());
		gAutomationp->resetTimer();
		if (lua_pcall(gAutomationp->mLuaState, 1, 0, 0) != LUA_OK)
		{
			gAutomationp->reportError();
		}
	}

	return 0;
}

//static
int HBViewerAutomation::automationRequest(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	S32 n = lua_gettop(state);
	if (n != 1)
	{
		luaL_error(state, "%d arguments passed; expected 1.", n);
	}
	std::string request(luaL_checkstring(state, 1));
	lua_pop(state, 1);

	if (!gAutomationp || !gAutomationp->mHasOnAutomationRequest)
	{
		LL_DEBUGS("Lua") << "No OnAutomationRequest Lua callback. request="
						 << request << ". Returning an empty result string."
						 << LL_ENDL;
		lua_pushliteral(state, "");
		return 1;
	}

	LL_DEBUGS("Lua") << "Invoking OnAutomationRequest Lua callback. request="
					 << request << LL_ENDL;

	lua_getglobal(gAutomationp->mLuaState, "OnAutomationRequest");
	lua_pushstring(gAutomationp->mLuaState, request.c_str());
	gAutomationp->resetTimer();
	if (lua_pcall(gAutomationp->mLuaState, 1, 1, 0) != LUA_OK)
	{
		gAutomationp->reportError();
		return 0;
	}

	if (lua_gettop(gAutomationp->mLuaState) == 0 ||
		lua_type(gAutomationp->mLuaState, -1) != LUA_TSTRING)
	{
		lua_pushliteral(gAutomationp->mLuaState,
						"OnAutomationRequest() Lua callback did not return a string");
		gAutomationp->reportError();
		return 0;
	}

	// Recover the result from the automation script stack...
	std::string result(lua_tolstring(gAutomationp->mLuaState, -1, NULL));
	lua_pop(gAutomationp->mLuaState, 1);

	// ... and push it on our stack.
	lua_pushstring(state, result.c_str());

	return 1;
}

//static
int HBViewerAutomation::playUISound(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	static const std::string valid_sounds = get_valid_sounds();

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("PlayUISound");
	}

	S32 n = lua_gettop(state);
	if (n != 1 && n != 2)
	{
		luaL_error(state, "%d arguments passed; expected 1 or 2.", n);
	}

	std::string name(luaL_checkstring(state, 1));
	name = "UISnd" + name;
	if (valid_sounds.find(";" + name + ";") == std::string::npos)
	{
		llwarns << "No such UI sound name: " << name << llendl;
		lua_pop(state, n);
		return 0;
	}

	bool force = false;
	if (n > 1)
	{
		force = lua_toboolean(state, 2);
	}

	lua_pop(state, n);

	make_ui_sound(name.c_str(), force);

	return 0;
}

//static
int HBViewerAutomation::renderDebugInfo(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("RenderDebugInfo");
	}

	S32 n = lua_gettop(state);
	if (n != 1)
	{
		luaL_error(state, "%d arguments passed; expected 1.", n);
	}
	S32 feature = luaL_checknumber(state, 1);
	lua_pop(state, 1);

	if (feature < 0 || feature > 32)
	{
		luaL_error(state,
				   "Invalid render debug feature index (valid range is 0 to 32");
	}

	if (feature)
	{
		gPipeline.setRenderDebugMask(1U << (feature - 1));
	}
	else
	{
		gPipeline.setRenderDebugMask(0);
	}

	return 0;
}

//static
int HBViewerAutomation::getDebugSetting(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("GetDebugSetting");
	}

	S32 n = lua_gettop(state);
	if (n != 1)
	{
		luaL_error(state, "%d arguments passed; expected 1.", n);
	}

	std::string name(luaL_checkstring(state, 1));
	if (name.empty())
	{
		luaL_error(state, "Empty setting name");
	}
	lua_pop(state, 1);

	// Note: as of Cool VL Viewer v1.28.2.72 & v1.30.0.0, commands sent via
	// scripted objects (or via D-Bus, under Linux) are now forbidden access
	// to debug settings, but the settings white lists have been removed (i.e.
	// non-external scripts are granted full access to any valid debug
	// setting).
	if (self->mFromObjectId != gAgentID)
	{
		lua_pushnil(state);
		return 1;
	}

	LLControlVariable* control = gSavedSettings.getControl(name.c_str());
	if (!control)
	{
		control = gSavedPerAccountSettings.getControl(name.c_str());
	}
	if (!control)
	{
		control = gColors.getControl(name.c_str());
	}
	if (!control)
	{
		luaL_error(state, "No setting named: %s", name.c_str());
	}

	eControlType type = control->type();
	switch (type)
	{
		case TYPE_U32:
		case TYPE_S32:
			lua_pushinteger(state, control->getValue().asInteger());
			break;

		case TYPE_F32:
			lua_pushnumber(state, control->getValue().asReal());
			break;

		case TYPE_BOOLEAN:
			lua_pushboolean(state, control->getValue().asBoolean());
			break;

		case TYPE_STRING:
			lua_pushstring(state, control->getValue().asString().c_str());
			break;

		case TYPE_VEC3:
		{
			LLVector3 vec;
			vec.setValue(control->getValue());
			lua_pushnumber(state, vec.mV[0]);
			lua_pushnumber(state, vec.mV[1]);
			lua_pushnumber(state, vec.mV[2]);
			n = 3;
			break;
		}

		case TYPE_RECT:
		{
			LLRect r;
			r.setValue(control->getValue());
			lua_pushinteger(state, r.mLeft);
			lua_pushinteger(state, r.mTop);
			lua_pushinteger(state, r.mRight);
			lua_pushinteger(state, r.mBottom);
			n = 4;
			break;
		}

		case TYPE_COL4:
		{
			LLColor4 color;
			color.setValue(control->getValue());
			lua_pushnumber(state, color.mV[0]);
			lua_pushnumber(state, color.mV[1]);
			lua_pushnumber(state, color.mV[2]);
			lua_pushnumber(state, color.mV[3]);
			n = 4;
			break;
		}

		case TYPE_COL3:
		{
			LLColor3 color;
			color.setValue(control->getValue());
			lua_pushnumber(state, color.mV[0]);
			lua_pushnumber(state, color.mV[1]);
			lua_pushnumber(state, color.mV[2]);
			n = 3;
			break;
		}

		case TYPE_COL4U:
		{
			LLColor4U color;
			color.setValue(control->getValue());
			lua_pushinteger(state, color.mV[0]);
			lua_pushinteger(state, color.mV[1]);
			lua_pushinteger(state, color.mV[2]);
			lua_pushinteger(state, color.mV[3]);
			n = 4;
			break;
		}

		default:
			// Other setting types (TYPE_LLSD which is only used in a couple
			// hidden settings, and TYPE_VEC3D which is not used at all) are
			// unsupported for now.
			lua_pushnil(state);
	}

	return n;
}

//static
int HBViewerAutomation::setDebugSetting(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("SetDebugSetting");
	}

	S32 n = lua_gettop(state);
	if (!n)
	{
		luaL_error(state, "Missing arguments.", n);
	}

	std::string name(luaL_checkstring(state, 1));
	if (name.empty())
	{
		luaL_error(state, "Empty setting name");
	}

	// Note: as of Cool VL Viewer v1.28.2.72 & v1.30.0.0, commands sent via
	// scripted objects (or via D-Bus, under Linux) are now forbidden access
	// to debug settings, but the settings white lists have been removed (i.e.
	// non-external scripts are granted full access to any valid debug
	// setting).
	if (self->mFromObjectId != gAgentID)
	{
		lua_pop(state, n);
		lua_pushnil(state);
		return 1;
	}

	LLControlVariable* control = gSavedSettings.getControl(name.c_str());
	if (!control)
	{
		control = gSavedPerAccountSettings.getControl(name.c_str());
	}
	if (!control)
	{
		control = gColors.getControl(name.c_str());
	}
	if (!control)
	{
		luaL_error(state, "No setting named: %s", name.c_str());
	}
	if (control->isHiddenFromUser())
	{
		luaL_error(state,
				   "Cannot set '%s' which is reserved for internal viewer code use only",
				   name.c_str());
	}

	bool success;
	eControlType type = control->type();

	if (n == 1)
	{
		success = type != TYPE_LLSD && type != TYPE_VEC3D;
		if (success)
		{
			control->resetToDefault();
		}
	}
	else
	{
		LLSD value;
		switch (type)
		{
			case TYPE_U32:
			case TYPE_S32:
				success = n == 2;
				if (success)
				{
					value = LLSD::Integer(luaL_checknumber(state, 2));
				}
				break;

			case TYPE_F32:
				success = n == 2;
				if (success)
				{
					value = LLSD::Real(luaL_checknumber(state, 2));
				}
				break;

			case TYPE_BOOLEAN:
				success = n == 2;
				if (success)
				{
					value = LLSD::Boolean(lua_toboolean(state, 2));
				}
				break;

			case TYPE_STRING:
				success = n == 2;
				if (success)
				{
					value = LLSD::String(luaL_checkstring(state, 2));
				}
				break;

			case TYPE_VEC3:
				success = n == 4;
				if (success)
				{
					value = LLVector3(luaL_checknumber(state, 2),
									  luaL_checknumber(state, 3),
									  luaL_checknumber(state, 4)).getValue();
				}
				break;

			case TYPE_RECT:
				success = n == 5;
				if (success)
				{
					value = LLRect(S32(luaL_checknumber(state, 2)),
								   S32(luaL_checknumber(state, 3)),
								   S32(luaL_checknumber(state, 4)),
								   S32(luaL_checknumber(state, 5))).getValue();
				}
				break;

			case TYPE_COL4:
				success = n == 4 || n == 5;
				if (success)
				{
					F32 r = luaL_checknumber(state, 2);
					F32 g = luaL_checknumber(state, 3);
					F32 b = luaL_checknumber(state, 4);
					F32 a = 1.f;
					if (n == 5)
					{
						a = luaL_checknumber(state, 5);
					}
					if (r >= 0.f && g >= 0.f && b >= 0.f && a >= 0.f &&
						r <= 1.f && g <= 1.f && b <= 1.f && a <= 1.f)
					{
						value = LLColor4(r, g, b, a).getValue();
					}
					else
					{
						success = false;
					}
				}
				break;

			case TYPE_COL3:
				success = n == 4;
				if (success)
				{
					F32 r = luaL_checknumber(state, 2);
					F32 g = luaL_checknumber(state, 3);
					F32 b = luaL_checknumber(state, 4);
					if (r >= 0.f && g >= 0.f && b >= 0.f &&
						r <= 1.f && g <= 1.f && b <= 1.f)
					{
						value = LLColor3(r, g, b).getValue();
					}
					else
					{
						success = false;
					}
				}
				break;

			case TYPE_COL4U:
				success = n == 4 || n == 5;
				if (success)
				{
					F32 r = luaL_checknumber(state, 2);
					F32 g = luaL_checknumber(state, 3);
					F32 b = luaL_checknumber(state, 4);
					F32 a = 255.f;
					if (n == 5)
					{
						a = luaL_checknumber(state, 5);
					}
					if (r >= 0.f && g >= 0.f && b >= 0.f && a >= 0.f &&
						r <= 255.f && g <= 255.f && b <= 255.f && a <= 255.f)
					{
						value = LLColor4U((U8)r, (U8)g, (U8)b,
										  (U8)a).getValue();
					}
				}
				break;

			default:
				// Other setting types (TYPE_LLSD which is only used in a
				// couple hidden settings, and TYPE_VEC3D which is not used at
				// all) are unsupported for now.
				success = false;
		}
		if (success)
		{
			control->setValue(value);
		}
	}

	lua_pop(state, n);
	lua_pushboolean(state, success);

	return 1;
}

//static
bool HBViewerAutomation::serializeTable(lua_State* state, S32 stack_level,
										std::string* output)
{
	if (!state || lua_type(state, stack_level) != LUA_TTABLE)
	{
		return false;
	}

	std::string data, value;
	lua_pushnil(state);
	while (lua_next(state, stack_level))
	{
		if (data.empty())
		{
			data = "{[";
		}
		else
		{
			data += ";[";
		}

		int key_type = lua_type(state, -2);
		switch (key_type)
		{
			case LUA_TNUMBER:
				value = llformat(LUA_NUMBER_FMT, lua_tonumber(state, -2));
				break;

			case LUA_TSTRING:
				value = lua_tostring(state, -2);
				LLStringUtil::replaceString(value, "\"", "\\\"");
				value = "\"" + value + "\"";
				break;

			case LUA_TBOOLEAN:
			case LUA_TNIL:
			default:
				lua_pop(state, 2);
				return false;
		}
		data += value + "]=";

		int value_type = lua_type(state, -1);
		switch (value_type)
		{
			case LUA_TNIL:
				value = "nil";
				break;

			case LUA_TBOOLEAN:
				value = lua_toboolean(state, -1) ? "true" : "false";
				break;

			case LUA_TNUMBER:
				value = llformat(LUA_NUMBER_FMT, lua_tonumber(state, -1));
				break;

			case LUA_TSTRING:
				value = lua_tostring(state, -1);
				LLStringUtil::replaceString(value, "\"", "\\\"");
				value = "\"" + value + "\"";
				break;

			default:
				lua_pop(state, 2);
				return false;
		}
		data += value;
		lua_pop(state, 1);
	}
	lua_pop(state, 1);

	data += "}";
	LL_DEBUGS("Lua") << "Resulting Lua code (table): " << data << LL_ENDL;

	if (output)
	{
		*output = data;
	}
	else
	{
		data = "base64:" + LLBase64::encode(data);
		lua_pushstring(state, data.c_str());
	}

	return true;
}

//static
bool HBViewerAutomation::deserializeTable(lua_State* state, std::string data)
{
	if (!state || data.compare(0, 7, "base64:") != 0)
	{
		return false;
	}

	data = LLBase64::decode(data.substr(7).c_str());
	LL_DEBUGS("Lua") << "Decoded Base64 data: " << data << LL_ENDL;
	data = "_V_SETTINGS=" + data;
	if (luaL_dostring(state, data.c_str()))
	{
		return false;
	}
	lua_getglobal(state, "_V_SETTINGS");
	lua_pushnil(state);
	lua_setglobal(state, "_V_SETTINGS");

	return true;
}

//static
int HBViewerAutomation::getGlobalData(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	if (!state) return 0;	// Paranoia

	S32 n = lua_gettop(state);
	if (n)
	{
		luaL_error(state, "%d arguments passed; expected 0.", n);
	}

	std::string data = gSavedSettings.getString("LuaSessionData");
	if (!deserializeTable(state, data))
	{
		lua_pushstring(state, data.c_str());
	}

	return 1;
}

//static
int HBViewerAutomation::setGlobalData(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	if (!state) return 0;	// Paranoia

	S32 n = lua_gettop(state);
	if (n != 1)
	{
		luaL_error(state, "%d arguments passed; expected 1.", n);
	}

	if (lua_type(state, 1) == LUA_TTABLE && !serializeTable(state))
	{
		luaL_error(state, "Unsupported table format");
	}

	std::string data(luaL_checkstring(state, 1));
	lua_pop(state, 1);

	gSavedSettings.setString("LuaSessionData", data);

	return 0;
}

//static
int HBViewerAutomation::getPerAccountData(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	if (!state) return 0;	// Paranoia

	S32 n = lua_gettop(state);
	if (n)
	{
		luaL_error(state, "%d arguments passed; expected 0.", n);
	}

	std::string data = gSavedPerAccountSettings.getString("LuaUserData");
	if (!deserializeTable(state, data))
	{
		lua_pushstring(state, data.c_str());
	}

	return 1;
}

//static
int HBViewerAutomation::setPerAccountData(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	if (!state) return 0;	// Paranoia

	S32 n = lua_gettop(state);
	if (n != 1)
	{
		luaL_error(state, "%d arguments passed; expected 1.", n);
	}

	if (lua_type(state, 1) == LUA_TTABLE && !serializeTable(state))
	{
		luaL_error(state, "Unsupported table format");
	}

	std::string data(luaL_checkstring(state, 1));
	lua_pop(state, 1);

	gSavedPerAccountSettings.setString("LuaUserData", data);

	return 0;
}

//static
int HBViewerAutomation::getSourceFileName(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	S32 n = lua_gettop(state);
	if (n)
	{
		luaL_error(state, "%d arguments passed; expected 0.", n);
	}

	if (self->mSourceFileName.empty())
	{
		lua_pushnil(state);
		return 1;
	}

	lua_pushstring(state, self->mSourceFileName.c_str());
	return 1;
}

//static
int HBViewerAutomation::getWatchdogState(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	S32 n = lua_gettop(state);
	if (n)
	{
		luaL_error(state, "%d arguments passed; expected 0.", n);
	}

	lua_pushnumber(state, self->mWatchdogTimer.getRemainingTimeF64());
	lua_pushnumber(state, self->mWatchdogTimer.getElapsedTimeF64());
	return 2;
}

//static
int HBViewerAutomation::getFrameTimeSeconds(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	if (!state) return 0;	// Paranoia

	S32 n = lua_gettop(state);
	if (n)
	{
		luaL_error(state, "%d arguments passed; expected 0.", n);
	}

	lua_pushnumber(state, gFrameTimeSeconds);

	return 1;
}

//static
int HBViewerAutomation::getTimeStamp(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("GetTimeStamp");
	}

	S32 n = lua_gettop(state);
	if (n > 2)
	{
		luaL_error(state, "%d arguments passed; expected 0 to 2.", n);
	}

	S32 time_zone = 0;	// Default to UTC
	if (n)
	{
		time_zone = luaL_checknumber(state, 1);
	}

	std::string time_format;
	if (n > 1)
	{
		time_format.assign(luaL_checkstring(state, 2));
	}
	else
	{
		time_format = gSavedSettings.getString("ShortDateFormat") + " ";
		time_format += gSavedSettings.getString("ShortTimeFormat");
	}

	if (n)
	{
		lua_pop(state, n);
	}

	// Correct the UTC time, adding the time zone offset
	time_t tz_time = time_corrected() + time_zone * 3600;
	struct tm* internal_time = utc_time_to_tm(tz_time);

	std::string timestamp;
	timeStructToFormattedString(internal_time, time_format, timestamp);
	timestamp += " UTC";
	if (time_zone != 0)
	{
		timestamp += llformat("%+d", time_zone);
	}

	lua_pushstring(state, timestamp.c_str());

	return 1;
}

//static
int HBViewerAutomation::getClipBoardString(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("GetClipBoardString");
	}

	S32 n = lua_gettop(state);
	if (n > 1)
	{
		luaL_error(state, "%d arguments passed; expected 0 or 1.", n);
	}

	S32 clipboard = 0;	// Default to viewer clipboard
	if (n)
	{
		clipboard = luaL_checknumber(state, 1);
		lua_pop(state, 1);
	}

	LLWString wtext;
	switch (clipboard)
	{
		case 0:
			wtext = gClipboard.getClipBoardString();
			break;

		case 1:
			if (gWindowp)
			{
				gWindowp->pasteTextFromClipboard(wtext);
			}
			break;

		case 2:
			if (gWindowp)
			{
				gWindowp->pasteTextFromPrimary(wtext);
			}
			break;

		default:
			luaL_error(state,
					   "Invalid clipboard type %d (valid types are 0 to 2).",
					   n);
	}
	lua_pushstring(state, wstring_to_utf8str(wtext).c_str());

	return 1;
}

//static
int HBViewerAutomation::setClipBoardString(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("SetClipBoardString");
	}

	S32 n = lua_gettop(state);
	if (n > 1)
	{
		luaL_error(state, "%d arguments passed; expected 0 or 1.", n);
	}
	std::string text;
	if (n)
	{
		text = luaL_checkstring(state, 1);
		lua_pop(state, 1);
	}

	gClipboard.copyFromSubstring(utf8str_to_wstring(text), 0, text.length());

	return 0;
}

//static
const LLUUID& HBViewerAutomation::getInventoryObjectId(const std::string& name,
													   bool& is_category)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	if (name.empty() || name == "|")
	{
		is_category = true;
		return gInventory.getRootFolderID();
	}

	LLViewerInventoryItem* item;
	LLViewerInventoryCategory* cat;

	// First check if the passed string is a valid object inventory Id
	if (LLUUID::validate(name))
	{
		LLUUID inv_obj_id(name);
		cat = gInventory.getCategory(inv_obj_id);
		if (cat)
		{
			LL_DEBUGS("Lua") << "Found an inventory category for Id: "
							 << inv_obj_id << " - Name: " << cat->getName()
							 << LL_ENDL;
			is_category = true;
			return cat->getUUID();
		}
		item = gInventory.getItem(inv_obj_id);
		if (item)
		{
			LL_DEBUGS("Lua") << "Found an inventory item for Id: "
							 << inv_obj_id << " - Name: " << item->getName()
							 << LL_ENDL;
			is_category = false;
			return item->getUUID();
		}
	}

	// Not an UUID, so split the string into path elements
	std::string item_name = name;
	std::deque<std::string> path;
	size_t i;
	std::string temp;
	while ((i = item_name.find('|')) != std::string::npos)
	{
		temp = item_name.substr(0, i);
		item_name = item_name.substr(i + 1);
		// temp is empty when 2+ successive '|' exist in path, or when one is
		// leading the full path. In both cases, skip the empty element.
		if (!temp.empty())
		{
			LL_DEBUGS("Lua") << "Adding name to path: " << temp << LL_ENDL;
			path.emplace_back(temp);
		}
	}
	// item_name is empty when a '|' is trailing in path (in which case the
	// empty string shall not be added to the path elements queue).
	if (!item_name.empty())
	{
		LL_DEBUGS("Lua") << "Adding name to path: " << item_name << LL_ENDL;
		path.emplace_back(item_name);
	}

	// Search for a matching inventory object
	const LLUUID* cat_id = &gInventory.getRootFolderID();
	LLInventoryModel::cat_array_t* cats;
	LLInventoryModel::item_array_t* items;
	bool last_name = false;
	while (!last_name)
	{
		item_name = path.front();
		path.pop_front();
		last_name = path.empty();

		gInventory.getDirectDescendentsOf(*cat_id, cats, items);

		item = NULL;
		cat = NULL;
		if (last_name)
		{
			for (S32 i = 0, count = items->size(); i < count; ++i)
			{
				item = (*items)[i];
				if (!item) continue;	// Paranoia

				if (item->getName() == item_name)
				{
					LL_DEBUGS("Lua") << "Found matching item name: "
									 << item_name << " - Returning item Id: "
									 << item->getUUID() << LL_ENDL;
					is_category = false;
					return item->getUUID();
				}
			}
		}
		for (S32 i = 0, count = cats->size(); i < count; ++i)
		{
			cat = (*cats)[i];
			if (!cat) continue;	// Paranoia

			if (cat->getName() == item_name)
			{
				LL_DEBUGS("Lua") << "Found matching category name: "
								 << item_name << LL_ENDL;
				if (last_name)
				{
					LL_DEBUGS("Lua") << "Returning category Id: "
									 << cat->getUUID() << LL_ENDL;
					is_category = true;
					return cat->getUUID();
				}
				break;
			}

			cat = NULL;
		}
		if (!cat)
		{
			break;
		}
		cat_id = &cat->getUUID();
	}

	is_category = false;
	return LLUUID::null;
}

//static
int HBViewerAutomation::findInventoryObject(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("FindInventoryObject");
	}

	S32 n = lua_gettop(state);
	if (n != 1)
	{
		luaL_error(state, "%d arguments passed; expected 1.", n);
	}
	std::string obj_name(luaL_checkstring(state, 1));
	if (obj_name.empty())
	{
		luaL_error(state, "Empty inventory object path name");
	}
	lua_pop(state, 1);

	bool is_category = false;
	const LLUUID& obj_id = getInventoryObjectId(obj_name, is_category);

	bool export_support = gAgent.regionHasExportPermSupport();
	bool copy_ok = false;
	bool mod_ok = false;
	bool xfer_ok = false;
	bool export_ok = false;
	S32 type = LLAssetType::AT_NONE;
	if (is_category)
	{
		LLViewerInventoryCategory* cat = gInventory.getCategory(obj_id);
		if (cat)
		{
			obj_name = cat->getName();
		}
		type = LLAssetType::AT_CATEGORY;
	}
	else if (obj_id.notNull())
	{
		LLViewerInventoryItem* itemp = gInventory.getItem(obj_id);
		if (itemp)
		{
			type = itemp->getType();
			obj_name = itemp->getName();
			const LLPermissions& perms = itemp->getPermissions();
			copy_ok = perms.allowCopyBy(gAgentID);
			mod_ok = perms.allowModifyBy(gAgentID);
			xfer_ok = perms.allowTransferBy(gAgentID);
			export_ok = export_support &&
						perms.allowExportBy(gAgentID, ep_export_bit);
		}
	}

	lua_newtable(state);
	lua_pushliteral(state, "id");
	lua_pushstring(state, obj_id.asString().c_str());
	lua_rawset(state, -3);
	lua_pushliteral(state, "name");
	lua_pushstring(state, obj_name.c_str());
	lua_rawset(state, -3);
	lua_pushliteral(state, "type");
	lua_pushinteger(state, type);
	lua_rawset(state, -3);
	lua_pushliteral(state, "copy_ok");
	lua_pushboolean(state, copy_ok);
	lua_rawset(state, -3);
	lua_pushliteral(state, "mod_ok");
	lua_pushboolean(state, mod_ok);
	lua_rawset(state, -3);
	lua_pushliteral(state, "xfer_ok");
	lua_pushboolean(state, xfer_ok);
	lua_rawset(state, -3);
	if (!is_category && export_support)
	{
		lua_pushliteral(state, "export_ok");
		lua_pushboolean(state, export_ok);
		lua_rawset(state, -3);
	}

	return 1;
}

//static
int HBViewerAutomation::giveInventory(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("GiveInventory");
	}

	S32 n = lua_gettop(state);
	if (n != 2)
	{
		luaL_error(state, "%d arguments passed; expected 2.", n);
	}
	LLUUID avatar_id(luaL_checkstring(state, 1), false);
	std::string item_name(luaL_checkstring(state, 2));
	lua_pop(state, 2);

	bool success = false;

	if (avatar_id.notNull())
	{
		bool is_category = false;
		const LLUUID& inv_obj_id = getInventoryObjectId(item_name,
													    is_category);
		if (inv_obj_id.notNull())
		{
			if (is_category)
			{
				LLViewerInventoryCategory* cat =
					gInventory.getCategory(inv_obj_id);
				if (cat)
				{
					LL_DEBUGS("Lua") << "avatar_id=" << avatar_id
									 << " - cat_id=" << cat->getUUID()
									 << LL_ENDL;
					LLToolDragAndDrop::giveInventoryCategory(avatar_id, cat);
					success = true;
				}
			}
			else
			{
				LLViewerInventoryItem* item = gInventory.getItem(inv_obj_id);
				if (item)
				{
					LL_DEBUGS("Lua") << "avatar_id=" << avatar_id
									 << " - item_id=" << item->getUUID()
									 << LL_ENDL;
					LLToolDragAndDrop::giveInventory(avatar_id, item);
					success = true;
				}
			}
		}
	}

	lua_pushboolean(state, success);

	return 1;
}

//static
int HBViewerAutomation::makeInventoryLink(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("MakeInventoryLink");
	}

	S32 n = lua_gettop(state);
	if (n != 2)
	{
		luaL_error(state, "%d arguments passed; expected 2.", n);
	}
	std::string item_path(luaL_checkstring(state, 1));
	if (item_path.empty())
	{
		luaL_error(state, "Empty item name");
	}
	std::string link_cat_path(luaL_checkstring(state, 2));
	lua_pop(state, 2);

	bool success = false;
	bool is_category = false;
	const LLUUID& item_id = getInventoryObjectId(item_path, is_category);
	if (!is_category && item_id.notNull())
	{
		LLUUID cat_id;
		if (link_cat_path.empty())
		{
			cat_id = gInventory.getRootFolderID();
		}
		else
		{
			cat_id = getInventoryObjectId(link_cat_path, is_category);
			if (!is_category || gInventory.isInTrash(cat_id) ||
				gInventory.isInMarketPlace(cat_id))
			{
				cat_id.setNull();
			}
		}
		if (cat_id.notNull())
		{
			link_inventory_object(cat_id, item_id);
			success = true;
		}
	}

	lua_pushboolean(state, success);

	return 1;
}

//static
int HBViewerAutomation::deleteInventoryLink(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("DeleteInventoryLink");
	}

	S32 n = lua_gettop(state);
	if (n != 1)
	{
		luaL_error(state, "%d arguments passed; expected 1.", n);
	}
	std::string link_path(luaL_checkstring(state, 1));
	if (link_path.empty())
	{
		luaL_error(state, "Empty link name");
	}
	lua_pop(state, 1);

	bool success = false;
	bool is_category = false;
	const LLUUID& item_id = getInventoryObjectId(link_path, is_category);
	if (!is_category && item_id.notNull())
	{
		LLViewerInventoryItem* item = gInventory.getItem(item_id);
		if (item && item->getIsLinkType() && !gInventory.isInTrash(item_id) &&
			!gInventory.isInMarketPlace(item_id))
		{
			remove_inventory_item(item_id);
			success = true;
		}
	}

	lua_pushboolean(state, success);

	return 1;
}

//static
int HBViewerAutomation::newInventoryFolder(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("NewInventoryFolder");
	}

	S32 n = lua_gettop(state);
	if (n != 2)
	{
		luaL_error(state, "%d arguments passed; expected 2.", n);
	}
	std::string folder_path(luaL_checkstring(state, 1));
	std::string folder_name(luaL_checkstring(state, 2));
	lua_pop(state, 2);

	LLUUID cat_id;
	if (folder_path.empty())
	{
		cat_id = gInventory.getRootFolderID();
	}
	else
	{
		bool is_category = false;
		cat_id = getInventoryObjectId(folder_path, is_category);
		if (!is_category ||
			// Forbid to make a folder in trash or market place.
			gInventory.isInTrash(cat_id) || gInventory.isInMarketPlace(cat_id))
		{
			cat_id.setNull();
		}
	}

	// Verify that the folder name is valid. Skip folder creation if not.
	std::string tmp = folder_name;
	LLStringFn::replace_nonprintable_and_pipe_in_ascii(tmp, LL_UNKNOWN_CHAR);
	if (tmp != folder_name)
	{
		cat_id.setNull();
	}

	if (cat_id.notNull())
	{
		cat_id = gInventory.createCategoryUDP(cat_id, LLFolderType::FT_NONE,
											  folder_name);
	}

	lua_pushstring(state, cat_id.asString().c_str());

	return 1;
}

//static
int HBViewerAutomation::listInventoryFolder(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("ListInventoryFolder");
	}

	S32 n = lua_gettop(state);
	if (n != 1)
	{
		luaL_error(state, "%d arguments passed; expected 1.", n);
	}
	std::string folder_path(luaL_checkstring(state, 1));
	lua_pop(state, 1);

	LLUUID cat_id;
	if (folder_path.empty())
	{
		cat_id = gInventory.getRootFolderID();
	}
	else
	{
		bool is_category = false;
		cat_id = getInventoryObjectId(folder_path, is_category);
		if (!is_category)
		{
			cat_id.setNull();
		}
	}
	if (cat_id.isNull())
	{
		lua_pushnil(state);
		return 1;
	}

	LLInventoryModel::cat_array_t* cats;
	LLInventoryModel::item_array_t* items;
	gInventory.getDirectDescendentsOf(cat_id, cats, items);

	lua_newtable(state);

	for (S32 i = 0, count = cats->size(); i < count; ++i)
	{
		LLViewerInventoryCategory* cat = (*cats)[i];
		if (cat)	// Paranoia
		{
			folder_path = cat->getName() + "|";
			lua_pushstring(state, cat->getUUID().asString().c_str());
			lua_pushstring(state, folder_path.c_str());
			lua_rawset(state, -3);
		}
	}
	for (S32 i = 0, count = items->size(); i < count; ++i)
	{
		LLViewerInventoryItem* item = (*items)[i];
		if (item)	// Paranoia
		{
			lua_pushstring(state, item->getUUID().asString().c_str());
			lua_pushstring(state, item->getName().c_str());
			lua_rawset(state, -3);
		}
	}

	return 1;
}

//static
int HBViewerAutomation::moveToInventoryFolder(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("MoveToInventoryFolder");
	}

	S32 n = lua_gettop(state);
	if (n != 2)
	{
		luaL_error(state, "%d arguments passed; expected 2.", n);
	}
	std::string folder_path(luaL_checkstring(state, 1));
	LLUUID cat_id;
	if (folder_path.empty())
	{
		cat_id = gInventory.getRootFolderID();
	}
	else
	{
		bool is_category = false;
		cat_id = getInventoryObjectId(folder_path, is_category);
		if (!is_category)
		{
			cat_id.setNull();
		}
	}
	if (cat_id.isNull())
	{
		llwarns << "Could not find destination folder: " << folder_path
				<< llendl;
	}
	// Forbid to move to trash, COF or market place folders.
	else if (gInventory.isInTrash(cat_id) || gInventory.isInCOF(cat_id) ||
			 gInventory.isInMarketPlace(cat_id))
	{
		llwarns << "Invalid destination folder." << llendl;
		cat_id.setNull();
	}
	if (cat_id.isNull())
	{
		lua_pop(state, n);
		lua_pushboolean(state, false);
		return 1;
	}
	LL_DEBUGS("Lua") << "Destination folder found. Id = " << cat_id << LL_ENDL;

	bool success = true;

	uuid_vec_t inv_objects;
	std::string invobj_path;
	LLUUID obj_id;
	bool is_category = false;

	int type = lua_type(state, 2);
	if (type == LUA_TTABLE)
	{
		// We accept either a list of string values (i.e. with numbers as keys)
		// representing inventory items paths or UUIDs, or a table with UUIDs
		// as keys and paths as values (as returned by ListInventoryFolder()).
		lua_pushnil(state);
		while (lua_next(state, 2))
		{
			int key_type = lua_type(state, -2);
			if (key_type == LUA_TNUMBER)
			{
				// It could be an element of a list of strings.
				if (lua_type(state, -1) != LUA_TSTRING)
				{
					llwarns << "Table element key is a number but value is not a string."
							<< llendl;
					success = false;
					break;
				}
				// Use the string value to find the inventory object
				invobj_path = lua_tostring(state, -1);
			}
			else if (key_type == LUA_TSTRING)
			{
				// It is a pair of key,value, and we expect the key to be the
				// UUID or the full path name for an inventory object.
				invobj_path = lua_tostring(state, -2);				
			}
			else
			{
				llwarns << "Table element key is not a number or string."
						<< llendl;
				success = false;
				break;
			}
			if (invobj_path.empty())
			{
				llwarns << "Inventory object path/UUID empty." << llendl;
				success = false;
				break;
			}
			obj_id = getInventoryObjectId(invobj_path, is_category);
			if (obj_id.isNull())
			{
				llwarns << "Could not find inventory object: " << invobj_path
						<< llendl;
				success = false;
				break;
			}
			LL_DEBUGS("Lua") << "Inventory object found. Id = " << obj_id
							 << LL_ENDL;
			inv_objects.emplace_back(obj_id);
			lua_pop(state, 1);
		}
	}
	else
	{
		// We accept a single inventory object path or UUID too, passed as a
		// string.
		invobj_path = luaL_checkstring(state, 2);
		if (!invobj_path.empty())
		{
			obj_id = getInventoryObjectId(invobj_path, is_category);
		}
		success = obj_id.notNull();
		if (success)
		{
			inv_objects.emplace_back(obj_id);
			LL_DEBUGS("Lua") << "Inventory object found. Id = " << obj_id
							 << LL_ENDL;
		}
		else
		{
			llwarns << "Could not find inventory object: " << invobj_path
					<< llendl;
		}
	}
	lua_pop(state, lua_gettop(state));

	if (success)
	{
		success = !inv_objects.empty() &&
				  reparent_to_folder(cat_id, inv_objects);
	}

	lua_pushboolean(state, success);
	return 1;
}

//static
void HBViewerAutomation::onPickInventoryItem(const std::vector<std::string>& names,
											 const uuid_vec_t& ids,
											 void* userdata, bool on_close)
{
	LL_TRACY_TIMER(TRC_LUA_CALLBACK);

	lua_State* state = (lua_State*)userdata;
	HBViewerAutomation* self = findInstance(state);
	if (!self || !self->mHasOnPickInventoryItem) return;

	S32 count = ids.size();
	LL_DEBUGS("Lua") << "Invoking OnPickInventoryItem Lua callback with "
					 << count << " selected inventory item"
					 << (count > 1 ? "s." : ".") << LL_ENDL;

	lua_getglobal(state, "OnPickInventoryItem");
	if (count)
	{
		lua_newtable(state);
		for (S32 i = 0; i < count; ++i)
		{
			lua_pushstring(state, ids[i].asString().c_str());
			lua_pushstring(state, names[i].c_str());
			lua_rawset(state, -3);
		}
	}
	else
	{
		lua_pushnil(state);
	}
	lua_pushboolean(state, on_close);

	self->resetTimer();
	if (lua_pcall(state, 2, 0, 0) != LUA_OK)
	{
		self->reportError();
	}
}

//static
int HBViewerAutomation::pickInventoryItem(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	if (!state) return 0;	// Paranoia

	S32 n = lua_gettop(state);
	if (n < 1 || n > 7)
	{
		luaL_error(state, "%d arguments passed; expected 2 to 7.", n);
	}
	S32 type = lua_tointeger(state, 1);
	S32 subtype = -1;
	if (n > 1)
	{
		subtype = lua_tointeger(state, 2);
	}
	bool allow_multiple = false;
	if (n > 2)
	{
		allow_multiple = lua_toboolean(state, 3);
	}
	bool exclude_library = true;
	if (n > 3)
	{
		exclude_library = lua_toboolean(state, 4);
	}
	bool can_apply_immediately = false;
	bool apply_immediately = false;
	if (n > 4)
	{
		can_apply_immediately = true;
		apply_immediately = lua_toboolean(state, 5);
	}
	PermissionMask mask = PERM_NONE;	// No restriction on permissions
	if (n > 5)
	{
		mask = lua_tointeger(state, 6);
	}
	bool callback_on_close = false;
	if (n > 6)
	{
		callback_on_close = lua_toboolean(state, 7);
	}
	lua_pop(state, n);

	// NOTE: the inventory item picker will auto-close on selection or cancel
	// action. We therefore do not need to track its pointer...
	HBFloaterInvItemsPicker* pickerp =
		new HBFloaterInvItemsPicker(NULL, &onPickInventoryItem, state);
	pickerp->setAssetType((LLAssetType::EType)type, subtype);
	pickerp->setAllowMultiple(allow_multiple);
	pickerp->setExcludeLibrary(exclude_library);
	pickerp->setFilterPermMask(mask);
	if (can_apply_immediately)
	{
		pickerp->allowApplyImmediately();
		pickerp->setApplyImmediately(apply_immediately);
	}
	if (callback_on_close)
	{
		pickerp->callBackOnClose();
	}

	return 0;
}

//static
void HBViewerAutomation::onPickAvatar(const std::vector<std::string>& names,
									  const std::vector<LLUUID>& ids,
									  void* userdata)
{
	LL_TRACY_TIMER(TRC_LUA_CALLBACK);

	lua_State* state = (lua_State*)userdata;
	HBViewerAutomation* self = findInstance(state);
	if (!self || !self->mHasOnPickAvatar) return;

	S32 count = ids.size();
	LL_DEBUGS("Lua") << "Invoking OnPickAvatar Lua callback with "
					 << count << " picked avatars" << (count > 1 ? "s." : ".")
					 << LL_ENDL;

	lua_getglobal(state, "OnPickAvatar");
	if (count)
	{
		lua_newtable(state);
		for (S32 i = 0; i < count; ++i)
		{
			lua_pushstring(state, ids[i].asString().c_str());
			lua_pushstring(state, names[i].c_str());
			lua_rawset(state, -3);
		}
	}
	else
	{
		lua_pushnil(state);
	}
	self->resetTimer();
	if (lua_pcall(state, 1, 0, 0) != LUA_OK)
	{
		self->reportError();
	}
}

//static
int HBViewerAutomation::pickAvatar(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	if (!state) return 0;	// Paranoia

	S32 n = lua_gettop(state);
	if (n > 2)
	{
		luaL_error(state, "%d arguments passed; expected 0 to 2.", n);
	}
	bool allow_multiple = false;
	if (n > 0)
	{
		allow_multiple = lua_toboolean(state, 1);
	}
	std::string search_name;
	if (n > 1)
	{
		search_name.assign(luaL_checkstring(state, 2));
	}
	lua_pop(state, n);

	// NOTE: the avatar picker will auto-close on selection or cancel action.
	// We therefore do not need to track its pointer...
	LLFloaterAvatarPicker::show(&onPickAvatar, state, allow_multiple, true,
								search_name);

	return 0;
}

//static
int HBViewerAutomation::getAgentAttachments(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self || !isAgentAvatarValid()) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("GetAgentAttachments");
	}

	S32 n = lua_gettop(state);
	if (n > 1)
	{
		luaL_error(state, "%d arguments passed; expected 0 or 1.", n);
	}
	std::string search_string;
	if (n)
	{
		search_string.assign(luaL_checkstring(state, 1));
		lua_pop(state, 1);
		LLStringUtil::toLower(search_string);
	}
	bool has_search_string = !search_string.empty();

	lua_newtable(state);

	std::string inv_item_uuid, item_name, lc_name, joint_name;
	for (S32 i = 0, count = gAgentAvatarp->mAttachedObjectsVector.size();
		 i < count; ++i)
	{
		LLViewerJointAttachment* vatt =
			gAgentAvatarp->mAttachedObjectsVector[i].second;
		if (!vatt) continue;	// Paranoia

		joint_name = LLTrans::getString(vatt->getName());
		LLStringUtil::toLower(joint_name);

		LLViewerObject* vobj = gAgentAvatarp->mAttachedObjectsVector[i].first;
		if (!vobj) continue;	// Paranoia

		const LLUUID& item_id = vobj->getAttachmentItemID();
		if (item_id.isNull()) continue;

		LLViewerInventoryItem* inv_item = gInventory.getItem(item_id);
		if (inv_item)
		{
			inv_item_uuid = inv_item->getLinkedUUID().asString();
			item_name = inv_item->getName();
		}
		else if (vobj->isTempAttachment())
		{
			inv_item_uuid = item_id.asString();
			item_name = "temp_attachment:" + inv_item_uuid;
		}
		else
		{
			llwarns << "Could not find any valid object for attachment Id: "
					<< item_id << llendl;
			continue;
		}

		if (has_search_string)
		{
			lc_name = item_name;
			LLStringUtil::toLower(lc_name);
		}

		if (!has_search_string || joint_name == search_string ||
			inv_item_uuid == search_string ||
			lc_name.find(search_string) != std::string::npos)
		{
			item_name += "|" + joint_name;
			lua_pushstring(state, inv_item_uuid.c_str());
			lua_pushstring(state, item_name.c_str());
			lua_rawset(state, -3);
		}
	}

	return 1;
}

//static
int HBViewerAutomation::getAgentWearables(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self || !isAgentAvatarValid()) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("GetAgentWearables");
	}

	S32 n = lua_gettop(state);
	if (n > 1)
	{
		luaL_error(state, "%d arguments passed; expected 0 or 1.", n);
	}
	std::string search_string;
	if (n)
	{
		search_string.assign(luaL_checkstring(state, 1));
		lua_pop(state, 1);
		LLStringUtil::toLower(search_string);
	}
	bool has_search_string = !search_string.empty();

	lua_newtable(state);

	std::string inv_item_uuid, item_name, lc_name, type_name;
	for (U32 i = 0; i < (U32)LLWearableType::WT_COUNT; ++i)
	{
		LLWearableType::EType type = (LLWearableType::EType)i;

		type_name = LLTrans::getString(LLWearableType::getTypeLabel(type));
		LLStringUtil::toLower(type_name);

		for (U32 j = 0, count = gAgentWearables.getWearableCount(type);
			 j < count; ++j)
		{
			LLViewerWearable* wearable =
				gAgentWearables.getViewerWearable(type, j);
			if (!wearable) continue;

			LLViewerInventoryItem* inv_item =
				gInventory.getItem(wearable->getItemID());
			if (!inv_item) continue;

			inv_item_uuid = inv_item->getLinkedUUID().asString();
			item_name = inv_item->getName();
			if (has_search_string)
			{
				lc_name = item_name;
				LLStringUtil::toLower(lc_name);
			}

			if (!has_search_string || type_name == search_string ||
				inv_item_uuid == search_string ||
				lc_name.find(search_string) != std::string::npos)
			{
				item_name += "|" + type_name;
				lua_pushstring(state, inv_item_uuid.c_str());
				lua_pushstring(state, item_name.c_str());
				lua_rawset(state, -3);
			}
		}
	}

	return 1;
}

//static
int HBViewerAutomation::getGridSimAndPos(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("GetGridSimAndPos");
	}

	S32 n = lua_gettop(state);
	if (n != 0)
	{
		luaL_error(state, "%d arguments passed; expected 0.", n);
	}

	self->pushGridSimAndPos();

	return 1;
}

//static
int HBViewerAutomation::getParcelInfo(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("GetParcelInfo");
	}

	S32 n = lua_gettop(state);
	if (n != 0)
	{
		luaL_error(state, "%d arguments passed; expected 0.", n);
	}

	self->pushParcelInfo();

	return 1;
}

//static
int HBViewerAutomation::getCameraMode(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("GetCameraMode");
	}

	S32 n = lua_gettop(state);
	if (n != 0)
	{
		luaL_error(state, "%d arguments passed; expected 0.", n);
	}

	lua_pushnumber(state, gAgent.getCameraMode());

	return 1;
}

//static
int HBViewerAutomation::setCameraMode(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("SetCameraMode");
	}

	S32 n = lua_gettop(state);
	if (n != 1 && n != 2)
	{
		luaL_error(state, "%d arguments passed; expected 1 or 2.", n);
	}

	S32 mode = lua_tointeger(state, 1);
	bool animate = true;
	if (n > 1)
	{
		animate = lua_toboolean(state, 2);
	}
	lua_pop(state, n);

	HBIgnoreCallback lock_on_camera_change(E_ONCAMERAMODECHANGE);

	bool success = false;
	if (mode == -2)
	{
		success = handle_reset_view();
	}
	else if (mode == -1)
	{
		success = gAgent.changeCameraToDefault(animate);
	}
	else if (mode == (S32)CAMERA_MODE_THIRD_PERSON)
	{
		success = gAgent.changeCameraToThirdPerson(animate);
	}
	else if (mode == (S32)CAMERA_MODE_MOUSELOOK)
	{
		success = gAgent.changeCameraToMouselook(animate);
	}
	lua_pushboolean(state, success);

	return 1;
}

//static
int HBViewerAutomation::setCameraFocus(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("SetCameraFocus");
	}

	S32 n = lua_gettop(state);
	if (n > 1)
	{
		luaL_error(state, "%d arguments passed; expected 0 or 1.", n);
	}

	std::string id_str;
	if (n)
	{
		id_str = luaL_checkstring(state, 1);
		if (!id_str.empty() && !LLUUID::validate(id_str))
		{
			luaL_error(state, "Invalid UUID: %s", id_str.c_str());
		}
		lua_pop(state, 1);
	}

	HBIgnoreCallback lock_on_camera_change(E_ONCAMERAMODECHANGE);

	if (id_str.empty())
	{
		gAgent.setFocusOnAvatar(true);
	}
	else
	{
		gAgent.lookAtObject(LLUUID(id_str), CAMERA_POSITION_OBJECT);
	}

	return 0;
}

static void on_name_cache_mute(const LLUUID& id, const std::string& name,
							   bool is_group, S32 flags, bool mute_it)
{
	LLMute mute(id, name, is_group ? LLMute::GROUP : LLMute::AGENT);
	if (mute_it)
	{
		LLMuteList::add(mute, flags);
	}
	else
	{
		LLMuteList::remove(mute, flags);
	}
}

//static
int HBViewerAutomation::addMute(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("AddMute");
	}

	S32 n = lua_gettop(state);
	if (n < 1 || n > 3)
	{
		luaL_error(state, "%d arguments passed; expected 1 to 3.", n);
	}

	std::string name = luaL_checkstring(state, 1);
	LLUUID id;
	if (LLUUID::validate(name))
	{
		id.set(name);
		name.clear();
	}

	S32 type = LLMute::BY_NAME;
	if (n > 1)
	{
		type = luaL_checknumber(state, 2);
		if (type < 0 || type >= (S32)LLMute::COUNT)
		{
			luaL_error(state, "Invalid mute type passed: %d", type);
		}
	}
	if (type == (S32)LLMute::BY_NAME && id.notNull())
	{
		luaL_error(state, "Cannot mute by name with an UUID");
	}

	S32 flags = 0;
	if (n > 2)
	{
		flags = luaL_checknumber(state, 3);
		if (flags < 0)
		{
			luaL_error(state, "Invalid mute flag(s) passed: %d", flags);
		}
	}

	lua_pop(state, n);

	bool success = false;
	switch (type)
	{
		case (S32)LLMute::AGENT:
		case (S32)LLMute::GROUP:
		{
			if (gCacheNamep)
			{
				gCacheNamep->get(id, type == (S32)LLMute::GROUP,
								 boost::bind(&on_name_cache_mute, _1, _2, _3,
											 flags, true));
				success = true;
			}
			break;
		}

		case (S32)LLMute::OBJECT:
		{
			success = requestObjectPropertiesFamily(id, 0);
			break;
		}

		case (S32)LLMute::BY_NAME:
		{
			LLMute mute(LLUUID::null, name, LLMute::BY_NAME);
			success = LLMuteList::add(mute, flags);
			break;
		}

		default:	// Never happens, unless the LLMute::EType enum got changed
			llerrs << "Invalid mute type: " << type << llendl;
	}
	lua_pushboolean(state, success);

	return 1;
}

//static
int HBViewerAutomation::removeMute(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("RemoveMute");
	}

	S32 n = lua_gettop(state);
	if (n < 1 || n > 3)
	{
		luaL_error(state, "%d arguments passed; expected 1 to 3.", n);
	}

	std::string name = luaL_checkstring(state, 1);
	LLUUID id;
	if (LLUUID::validate(name))
	{
		id.set(name);
		name.clear();
	}

	S32 type = LLMute::BY_NAME;
	if (n > 1)
	{
		type = luaL_checknumber(state, 2);
		if (type < 0 || type >= (S32)LLMute::COUNT)
		{
			luaL_error(state, "Invalid mute type passed: %d", type);
		}
	}
	if (type == (S32)LLMute::BY_NAME && id.notNull())
	{
		luaL_error(state, "Cannot unmute by name with an UUID");
	}

	S32 flags = 0;
	if (n > 2)
	{
		flags = luaL_checknumber(state, 3);
		if (flags < 0)
		{
			luaL_error(state, "Negative mute flag passed: %d", flags);
		}
	}

	lua_pop(state, n);

	bool success = false;
	switch (type)
	{
		case (S32)LLMute::AGENT:
		case (S32)LLMute::GROUP:
		{
			if (gCacheNamep)
			{
				gCacheNamep->get(id, type == (S32)LLMute::GROUP,
								 boost::bind(&on_name_cache_mute, _1, _2, _3,
											 flags, false));
				success = true;
			}
			break;
		}

		case (S32)LLMute::OBJECT:
		{
			success = requestObjectPropertiesFamily(id, 1);
			break;
		}

		case (S32)LLMute::BY_NAME:
		{
			LLMute mute(LLUUID::null, name, LLMute::BY_NAME);
			success = LLMuteList::remove(mute);
			break;
		}

		default:	// Never happens, unless the LLMute::EType enum got changed
			llerrs << "Invalid mute type: " << type << llendl;
	}
	lua_pushboolean(state, success);

	return 1;
}

//static
int HBViewerAutomation::isMuted(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("IsMuted");
	}

	S32 n = lua_gettop(state);
	if (n < 1 || n > 3)
	{
		luaL_error(state, "%d arguments passed; expected 1 to 3.", n);
	}

	std::string name = luaL_checkstring(state, 1);
	LLUUID object_id;
	if (LLUUID::validate(name))
	{
		object_id.set(name);
		name.clear();
	}

	S32 type = LLMute::COUNT;
	if (n > 1)
	{
		type = luaL_checknumber(state, 2);
		if (type < 0 || type > (S32)LLMute::COUNT)
		{
			luaL_error(state, "Invalid mute type passed: %d", type);
		}
	}

	S32 flags = 0;
	if (n > 2)
	{
		flags = luaL_checknumber(state, 3);
		if (flags < 0)
		{
			luaL_error(state, "Negative mute flag passed: %d", flags);
		}
	}

	lua_pop(state, n);

	lua_pushboolean(state, LLMuteList::isMuted(object_id, name, flags,
											   (LLMute::EType)type));

	return 1;
}

//static
int HBViewerAutomation::blockSound(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("BlockSound");
	}

	S32 n = lua_gettop(state);
	if (n != 1 && n != 2)
	{
		luaL_error(state, "%d arguments passed; expected 1 or 2.", n);
	}

	std::string id_str = luaL_checkstring(state, 1);
	if (!LLUUID::validate(id_str))
	{
		luaL_error(state, "Invalid UUID: %s", id_str.c_str());
	}

	bool block = true;
	if (n > 1)
	{
		block = lua_toboolean(state, 2);
	}

	lua_pop(state, n);

	LLAudioData::blockSound(LLUUID(id_str), block);

	// Inform the sounds list floater (if opened) that blocked sounds changed.
	HBFloaterSoundsList::setDirty();

	return 0;
}

//static
int HBViewerAutomation::isBlockedSound(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("IsBlockedSound");
	}

	S32 n = lua_gettop(state);
	if (n != 1)
	{
		luaL_error(state, "%d arguments passed; expected 1.", n);
	}

	std::string id_str = luaL_checkstring(state, 1);
	if (!LLUUID::validate(id_str))
	{
		luaL_error(state, "Invalid UUID: %s", id_str.c_str());
	}
	lua_pop(state, 1);

	lua_pushboolean(state,  LLAudioData::isBlockedSound(LLUUID(id_str)));

	return 1;
}

//static
int HBViewerAutomation::getBlockedSounds(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("GetBlockedSounds");
	}

	S32 n = lua_gettop(state);
	if (n)
	{
		luaL_error(state, "%d arguments passed; expected 0.", n);
	}

	const uuid_list_t& sounds = LLAudioData::getBlockedSounds();
	int i = 0;
	lua_newtable(state);
	for (uuid_list_t::const_iterator it = sounds.begin(), end = sounds.end();
 		 it != end; ++it)
	{
		lua_pushstring(state, it->asString().c_str());
		lua_rawseti(state, -2, ++i);
	}

	return 1;
}

//static
int HBViewerAutomation::derenderObject(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("DerenderObject");
	}

	S32 n = lua_gettop(state);
	if (n > 2)
	{
		luaL_error(state, "%d arguments passed; expected 0 to 2.", n);
	}

	LLUUID object_id;
	if (n > 0)
	{
		object_id.set(luaL_checkstring(state, 1), false);
	}
	bool derender = true;
	if (n > 1)
	{
		derender = lua_toboolean(state, 2);
	}
	lua_pop(state, n);

	bool success = true;
	if (n == 0)
	{
		gObjectList.sBlackListedObjects.clear();
		HBFloaterRadar::setRenderStatusDirty();
	}
	else if (derender)
	{
		// Note: HBFloaterRadar::setRenderStatusDirty() will be called if
		// needed by derender_object().
		success = derender_object(object_id);
	}
	else if (gObjectList.sBlackListedObjects.count(object_id))
	{
		gObjectList.sBlackListedObjects.erase(object_id);
		// Call unconditionnaly (even for non-avatar objects): it really does
		// not matter, and searching for the object in the avatars list to
		// check whether it is an avatar or not would take more time.
		HBFloaterRadar::setRenderStatusDirty(object_id);
	}
	else
	{
		success = false;
	}

	lua_pushboolean(state, success);

	return 1;
}

//static
int HBViewerAutomation::getDerenderedObjects(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("GetDerenderedObjects");
	}

	S32 n = lua_gettop(state);
	if (n)
	{
		luaL_error(state, "%d arguments passed; expected 0.", n);
	}

	int i = 0;
	lua_newtable(state);
	for (uuid_list_t::iterator it = gObjectList.sBlackListedObjects.begin(),
							   end = gObjectList.sBlackListedObjects.end();
		 it != end ; ++it)
	{
		lua_pushstring(state, (*it).asString().c_str());
		lua_rawseti(state, -2, ++i);
	}

	return 1;
}

//static
int HBViewerAutomation::getAgentPushes(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("GetAgentPushes");
	}

	S32 n = lua_gettop(state);
	if (n != 1)
	{
		luaL_error(state, "%d arguments passed; expected 1.", n);
	}

	LLUUID perpetrator_id(luaL_checkstring(state, 1), false);
	lua_pop(state, 1);

	std::string desc = HBFloaterBump::getMeanCollisionsStats(perpetrator_id);

	lua_pushstring(state, desc.c_str());

	return 1;
}

//static
int HBViewerAutomation::applyDaySettings(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("ApplyDaySettings");
	}

	S32 n = lua_gettop(state);
	if (n != 1)
	{
		luaL_error(state, "%d arguments passed; expected 1.", n);
	}

	std::string preset(luaL_checkstring(state, 1));
	lua_pop(state, 1);

	bool success = LLStartUp::isLoggedIn() &&
//MK
				   (!gRLenabled || !gRLInterface.mContainsSetenv);
//mk
	if (!success)
	{
		lua_pushboolean(state, success);
		return 1;
	}

	HBIgnoreCallback lock_on_wl_change(E_ONWINDLIGHTCHANGE);

	// Check special "settings" that trigger specific environments
	if (preset == "animate")
	{
		LLEnvironment::setRegion();
	}
	else if (preset == "region")
	{
		success = false;
	}
	else if (preset == "sunrise")
	{
		LLEnvironment::setSunrise();
	}
	else if (preset == "midday" || preset == "noon")
	{
		LLEnvironment::setMidday();
	}
	else if (preset == "sunset")
	{
		LLEnvironment::setSunset();
	}
	else if (preset == "midnight")
	{
		LLEnvironment::setMidnight();
	}
	else if (preset == "parcel")
	{
		gSavedSettings.setBool("UseParcelEnvironment", true);
	}
	else if (preset == "local")
	{
		gSavedSettings.setBool("UseLocalEnvironment", true);
	}
	else if (preset == "windlight")
	{
		gSavedSettings.setBool("UseParcelEnvironment", false);
		gSavedSettings.setBool("UseLocalEnvironment", false);
	}
	else
	{
		success = false;
	}

	// Then try actual settings (inventory assets or Windlight)
	if (!success)
	{
		success = LLEnvSettingsDay::applyPresetByName(preset);
	}

	lua_pushboolean(state, success);

	return 1;
}

//static
int HBViewerAutomation::applySkySettings(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("ApplySkySettings");
	}

	S32 n = lua_gettop(state);
	if (n != 1)
	{
		luaL_error(state, "%d arguments passed; expected 1.", n);
	}

	std::string preset(luaL_checkstring(state, 1));
	lua_pop(state, 1);

	bool success = LLStartUp::isLoggedIn() &&
//MK
				   (!gRLenabled || !gRLInterface.mContainsSetenv);
//mk
	if (success)
	{
		HBIgnoreCallback lock_on_wl_change(E_ONWINDLIGHTCHANGE);
		success = LLEnvSettingsSky::applyPresetByName(preset);
	}

	lua_pushboolean(state, success);

	return 1;
}

//static
int HBViewerAutomation::applyWaterSettings(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("ApplyWaterSettings");
	}

	S32 n = lua_gettop(state);
	if (n != 1)
	{
		luaL_error(state, "%d arguments passed; expected 1.", n);
	}

	std::string preset(luaL_checkstring(state, 1));
	lua_pop(state, 1);

	bool success = LLStartUp::isLoggedIn() &&
//MK
				   (!gRLenabled || !gRLInterface.mContainsSetenv);
//mk

	if (success)
	{
		HBIgnoreCallback lock_on_wl_change(E_ONWINDLIGHTCHANGE);
		success = LLEnvSettingsWater::applyPresetByName(preset);
	}

	lua_pushboolean(state, success);

	return 1;
}

//static
int HBViewerAutomation::setDayTime(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("SetDayTime");
	}

	S32 n = lua_gettop(state);
	if (n != 1)
	{
		luaL_error(state, "%d arguments passed; expected 1.", n);
	}

	F32 time = luaL_checknumber(state, 1);
	lua_pop(state, 1);

	if (!LLStartUp::isLoggedIn() || time > 1.f ||
//MK
		(gRLenabled && gRLInterface.mContainsSetenv))
//mk
	{
		return 0;
	}

	HBIgnoreCallback lock_on_wl_change(E_ONWINDLIGHTCHANGE);

	if (time < 0.f)
	{
		// Revert to parcel environment...
		gSavedSettings.setBool("UseParcelEnvironment", true);
		return 0;
	}

	// Extended environment time of day, using a fixed sky setting...
	if (gEnvironment.hasEnvironment(LLEnvironment::ENV_LOCAL))
	{
		if (gEnvironment.getEnvironmentDay(LLEnvironment::ENV_LOCAL))
		{
			// We have a full day cycle in the local environment: freeze the
			// sky.
			LLSettingsSky::ptr_t skyp =
				gEnvironment.getEnvironmentFixedSky(LLEnvironment::ENV_LOCAL)->buildClone();
			gEnvironment.setEnvironment(LLEnvironment::ENV_LOCAL, skyp, 0);
		}
	}
	else
	{
		// Use a copy of the parcel environment sky instead.
		LLSettingsSky::ptr_t skyp =
			gEnvironment.getEnvironmentFixedSky(LLEnvironment::ENV_PARCEL,
												true)->buildClone();
		gEnvironment.setEnvironment(LLEnvironment::ENV_LOCAL, skyp, 0);
	}
	gEnvironment.setSelectedEnvironment(LLEnvironment::ENV_LOCAL,
										LLEnvironment::TRANSITION_INSTANT);

	// Set the time now...
	gEnvironment.setFixedTimeOfDay(time);

	return 0;
}

//static
int HBViewerAutomation::getEESettingsList(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("GetEESettingsList");
	}

	S32 n = lua_gettop(state);
	if (n != 0 && n != 1)
	{
		luaL_error(state, "%d arguments passed; expected 0 or 1.", n);
	}
	S32 wanted_type = -1;
	if (n)
	{
		wanted_type = luaL_checknumber(state, 1);
		lua_pop(state, n);
		if (wanted_type < 0 || wanted_type > 2)
		{
			wanted_type = -1;
		}
	}

	if (!gAgent.hasInventorySettings())
	{
		lua_pushnil(state);
		return 1;
	}

	const LLUUID& folder_id =
		gInventory.findCategoryUUIDForType(LLFolderType::FT_SETTINGS, false);
	if (folder_id.isNull())
	{
		lua_pushnil(state);
		return 1;
	}

	typedef std::map<std::string, std::string> smap_t;
	smap_t settings;

	LLEnvSettingsCollector collector;
	LLInventoryModel::cat_array_t cats;
	LLInventoryModel::item_array_t items;
	gInventory.collectDescendentsIf(folder_id, cats, items, false, collector);
	std::string name, type_str;
	for (LLInventoryModel::item_array_t::iterator iter = items.begin(),
												  end = items.end();
		 iter != end; ++iter)
	{
		LLViewerInventoryItem* itemp = *iter;
		S32 type = (S32)itemp->getSettingsType();
		if (type < 0 || type > 2 || (wanted_type != -1 && type != wanted_type))
		{
			continue;
		}
		name = itemp->getName();
		smap_t::iterator sit = settings.find(name);
		if (sit == settings.end())
		{
			settings[name] = sEnvSettingsTypes[type];
		}
		else if (sit->second.find(sEnvSettingsTypes[type]) == std::string::npos)
		{
			sit->second += "," + sEnvSettingsTypes[type];
		}
	}

	if (settings.empty())
	{
		lua_pushnil(state);
		return 1;
	}

	lua_newtable(state);
	for (smap_t::iterator it = settings.begin(), end = settings.end();
		 it != end; ++it)
	{
		lua_pushstring(state, it->first.c_str());
		lua_pushstring(state, it->second.c_str());
		lua_rawset(state, -3);
	}
	return 1;
}

//static
int HBViewerAutomation::getWLSettingsList(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("GetWLSettingsList");
	}

	S32 n = lua_gettop(state);
	if (n != 0 && n != 1)
	{
		luaL_error(state, "%d arguments passed; expected 0 or 1.", n);
	}
	S32 wanted_type = -1;
	if (n)
	{
		wanted_type = luaL_checknumber(state, 1);
		lua_pop(state, n);
		if (wanted_type < 0 || wanted_type > 2)
		{
			wanted_type = -1;
		}
	}

	if (!gAgent.hasInventorySettings())
	{
		lua_pushnil(state);
		return 1;
	}

	typedef std::map<std::string, std::string> smap_t;
	smap_t settings;

	std::vector<std::string> presets;

	if (wanted_type == 0 || wanted_type == -1)
	{
		presets = LLWLSkyParamMgr::getLoadedPresetsList();
		for (S32 i = 0, count = presets.size(); i < count; ++i)
		{
			const std::string& name = presets[i];
			settings[name] = "sky";
		}
	}

	if (wanted_type == 1 || wanted_type == -1)
	{
		presets = LLWLWaterParamMgr::getLoadedPresetsList();
		for (S32 i = 0, count = presets.size(); i < count; ++i)
		{
			const std::string& name = presets[i];
			smap_t::iterator sit = settings.find(name);
			if (sit == settings.end())
			{
				settings[name] = "water";
			}
			else if (sit->second.find("water") == std::string::npos)
			{
				sit->second += ",water";
			}
		}
	}

	if (wanted_type == 2 || wanted_type == -1)
	{
		presets = LLWLDayCycle::getLoadedPresetsList();
		for (S32 i = 0, count = presets.size(); i < count; ++i)
		{
			const std::string& name = presets[i];
			smap_t::iterator sit = settings.find(name);
			if (sit == settings.end())
			{
				settings[name] = "day";
			}
			else if (sit->second.find("day") == std::string::npos)
			{
				sit->second += ",day";
			}
		}
	}

	if (settings.empty())
	{
		lua_pushnil(state);
		return 1;
	}

	lua_newtable(state);
	for (smap_t::iterator it = settings.begin(), end = settings.end();
		 it != end; ++it)
	{
		lua_pushstring(state, it->first.c_str());
		lua_pushstring(state, it->second.c_str());
		lua_rawset(state, -3);
	}
	return 1;
}

//static
int HBViewerAutomation::getEnvironmentStatus(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("GetEnvironmentStatus");
	}

	S32 n = lua_gettop(state);
	if (n)
	{
		luaL_error(state, "%d arguments passed; expected 0.", n);
	}

	lua_newtable(state);

#if 1	// *TODO: remove ?
	lua_pushstring(state, "enhanced rendering");
	lua_pushboolean(state, true);
	lua_rawset(state, -3);

	lua_pushstring(state, "windlight override");
	lua_pushboolean(state, true);
	lua_rawset(state, -3);
#endif

	static LLCachedControl<bool> local(gSavedSettings, "UseLocalEnvironment");
	lua_pushstring(state, "local environment");
	lua_pushboolean(state, (bool)local);
	lua_rawset(state, -3);

	static LLCachedControl<bool> parcel(gSavedSettings,
										"UseParcelEnvironment");
	lua_pushstring(state, "parcel environment");
	lua_pushboolean(state, (bool)parcel);
	lua_rawset(state, -3);

	static LLCachedControl<bool> estate(gSavedSettings, "UseWLEstateTime");
	bool region_time = estate;
	if (region_time)
	{
		region_time = gWLSkyParamMgr.mAnimator.mIsRunning;
	}
	lua_pushstring(state, "region time");
	lua_pushboolean(state, region_time);
	lua_rawset(state, -3);

	lua_pushstring(state, "rlv locked");
	lua_pushboolean(state, gRLenabled && gRLInterface.mContainsSetenv);
	lua_rawset(state, -3);

	return 1;
}

void HBViewerAutomation::onAutoPilotFinished(const std::string& type,
											 bool reached, bool user_cancel)
{
	LL_TRACY_TIMER(TRC_LUA_CALLBACK);

	if (!mHasOnAutoPilotFinished || !mLuaState)
	{
		return;
	}

	LL_DEBUGS("Lua") << "Invoking OnAutoPilotFinished Lua callback. type="
					 << type << " - reached=" << reached << " - user_cancel="
					 << user_cancel << LL_ENDL;

	lua_getglobal(mLuaState, "OnAutoPilotFinished");
	lua_pushstring(mLuaState, type.c_str());
	lua_pushboolean(mLuaState, reached);
	lua_pushboolean(mLuaState, user_cancel);
	resetTimer();
	if (lua_pcall(mLuaState, 3, 0, 0) != LUA_OK)
	{
		reportError();
	}
}

//static
int HBViewerAutomation::agentAutoPilotToPos(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self || !isAgentAvatarValid()) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("AgentAutoPilotToPos");
	}

	S32 n = lua_gettop(state);
	if (n < 2 || n > 5)
	{
		luaL_error(state, "%d arguments passed; expected 2 to 5.", n);
	}

	F64 pos_x = luaL_checknumber(state, 1);
	F64 pos_y = luaL_checknumber(state, 2);
	F64 pos_z = -1.0;
	if (n >= 3)
	{
		pos_z = luaL_checknumber(state, 3);
	}
	if (pos_z < 0.0)
	{
		pos_z = gAgentAvatarp->getPositionGlobal().mdV[VZ];
	}
	bool allow_flying = n >= 4 && lua_toboolean(state, 4);

	F32 stop_distance = 1.f;
	if (n >= 5)
	{
		stop_distance = luaL_checknumber(state, 5);
	}

	lua_pop(state, n);

	static S32 counter = 0;
	std::string type = llformat("Lua auto-pilot %d", ++counter);

	gAgentPilot.startAutoPilotGlobal(LLVector3d(pos_x, pos_y, pos_z),
									 type, NULL, NULL, NULL, stop_distance,
									 0.03f, allow_flying);

	lua_pushstring(state, type.c_str());

	return 1;
}

//static
int HBViewerAutomation::agentAutoPilotFollow(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("AgentAutoPilotFollow");
	}

	S32 n = lua_gettop(state);
	if (n < 1 || n > 3)
	{
		luaL_error(state, "%d arguments passed; expected 1 to 3.", n);
	}

	LLUUID id(luaL_checkstring(state, 1), false);
	bool allow_flying = n >= 2 && lua_toboolean(state, 2);
	F32 stop_distance = 1.f;
	if (n >= 3)
	{
		stop_distance = luaL_checknumber(state, 3);
	}
	lua_pop(state, n);

	bool success = gAgentPilot.startFollowPilot(id, allow_flying,
												stop_distance);
	lua_pushboolean(state, success);

	return 1;
}

//static
int HBViewerAutomation::agentAutoPilotStop(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("AgentAutoPilotStop");
	}

	S32 n = lua_gettop(state);
	if (n)
	{
		luaL_error(state, "%d arguments passed; expected 0.", n);
	}

	gAgentPilot.stopAutoPilot();

	return 0;
}

//static
int HBViewerAutomation::agentAutoPilotLoad(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("AgentAutoPilotLoad");
	}

	S32 n = lua_gettop(state);
	if (n > 1)
	{
		luaL_error(state, "%d arguments passed; expected 0 or 1.", n);
	}
	std::string filename;
	if (n)
	{
		filename = luaL_checkstring(state, 1);
		lua_pop(state, 1);
	}
	else
	{
		filename = gSavedSettings.getString("AutoPilotFile");
	}

	lua_pushboolean(state, gAgentPilot.load(filename));

	return 1;
}

//static
int HBViewerAutomation::agentAutoPilotSave(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("AgentAutoPilotSave");
	}

	S32 n = lua_gettop(state);
	if (n > 1)
	{
		luaL_error(state, "%d arguments passed; expected 0 or 1.", n);
	}
	std::string filename;
	if (n)
	{
		filename = luaL_checkstring(state, 1);
		lua_pop(state, 1);
	}
	else
	{
		filename = gSavedSettings.getString("AutoPilotFile");
	}

	lua_pushboolean(state, gAgentPilot.save(filename));

	return 1;
}

//static
int HBViewerAutomation::agentAutoPilotRemove(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("AgentAutoPilotRemove");
	}

	S32 n = lua_gettop(state);
	if (n > 1)
	{
		luaL_error(state, "%d arguments passed; expected 0 or 1.", n);
	}
	std::string filename;
	if (n)
	{
		filename = luaL_checkstring(state, 1);
		lua_pop(state, 1);
	}
	else
	{
		filename = gSavedSettings.getString("AutoPilotFile");
	}

	LLAgentPilot::remove(filename);

	return 0;
}

//static
int HBViewerAutomation::agentAutoPilotRecord(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("AgentAutoPilotRecord");
	}

	S32 n = lua_gettop(state);
	if (n != 1)
	{
		luaL_error(state, "%d arguments passed; expected 1.", n);
	}
	bool start = lua_toboolean(state, 1);
	lua_pop(state, 1);

	bool success = start ? gAgentPilot.startRecord()
						 : gAgentPilot.stopRecord();
	lua_pushboolean(state, success);

	return 1;
}

//static
int HBViewerAutomation::agentAutoPilotReplay(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("AgentAutoPilotReplay");
	}

	S32 n = lua_gettop(state);
	if (n < 1 || n > 3)
	{
		luaL_error(state, "%d arguments passed; expected 1 to 3.", n);
	}

	bool start = lua_toboolean(state, 1);
	if (!start && n > 1)
	{
		luaL_error(state,
				   "%d arguments passed; expected only 1 for a stop action.",
				   n);
	}

	S32 runs = -1;
	if (n >= 2)
	{
		runs = lua_tointeger(state, 2);
	}

	bool allow_flying = false;
	if (n >= 3)
	{
		allow_flying = lua_toboolean(state, 3);
	}

	lua_pop(state, n);

	bool success = start ? gAgentPilot.startPlayback(runs, allow_flying)
						 : gAgentPilot.stopPlayback();
	lua_pushboolean(state, success);

	return 1;
}

//static
int HBViewerAutomation::agentPuppetryStart(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	S32 n = lua_gettop(state);
	if (n > 2)
	{
		luaL_error(state, "%d arguments passed; expected 0 to 2.", n);
	}

	bool is_plugin_filename = false;
	if (n > 1)
	{
		is_plugin_filename = lua_toboolean(state, 2);
	}

	bool is_saved_cmd;
	std::string command;
	if (n)
	{
		is_saved_cmd = false;
		command = luaL_checkstring(state, 1);
		lua_pop(state, n);
	}
	else
	{
		is_saved_cmd = true;
		command = gSavedSettings.getString("PuppetryLastCommand");
	}

	bool success = false;

	if (!command.empty() && LLPuppetMotion::enabled())
	{
		LLPuppetModule* modulep = LLPuppetModule::getInstance();
		// Only try and launch when no module is already running
		if (!modulep->havePuppetModule())
		{
			if (is_plugin_filename)
			{
				success = LLFile::exists(command);
				if (success)
				{
					success = modulep->launchLeapPlugin(command);
				}
			}
			else
			{
				success = modulep->launchLeapCommand(command);
				if (!success && is_saved_cmd)
				{
					// Clear the command, since it is obviously invalid... HB
					gSavedSettings.setString("PuppetryLastCommand", "");
				}
			}
		}
	}

	lua_pushboolean(state, success);

	return 1;
}

//static
int HBViewerAutomation::agentPuppetryStop(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	S32 n = lua_gettop(state);
	if (n)
	{
		luaL_error(state, "%d arguments passed; expected 0.", n);
	}

	if (LLPuppetMotion::enabled())
	{
		LLPuppetModule* modulep = LLPuppetModule::getInstance();
		if (modulep->havePuppetModule())
		{
			modulep->setSending(false);
			modulep->setEcho(false);
			modulep->clearLeapModule();
		}
	}

	return 0;
}

//static
int HBViewerAutomation::agentRotate(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("AgentRotate");
	}

	S32 n = lua_gettop(state);
	if (n != 1)
	{
		luaL_error(state, "%d arguments passed; expected 1.", n);
	}
	F32 angle = luaL_checknumber(state, 1);
	if (angle > 360.f)
	{
		angle = fmodf(angle, 360.f);
	}
	else if (angle < 0.f)
	{
		angle = 360.f - fmodf(-angle, 360.f);
	}
	lua_pop(state, 1);

	gAgent.startCameraAnimation();
	LLVector3 rot(0.f, 1.f, 0.f);
	rot = rot.rotVec(-angle * DEG_TO_RAD, LLVector3::z_axis);
	rot.normalize();
	gAgent.resetAxes(rot);

	return 0;
}

//static
int HBViewerAutomation::getAgentRotation(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	if (!state) return 0;	// Paranoia

	S32 n = lua_gettop(state);
	if (n != 0)
	{
		luaL_error(state, "%d arguments passed; expected 0.", n);
	}

	const LLVector3& at_axis = gAgent.getAtAxis();
	F32 rotation = atan2f(at_axis.mV[VX], at_axis.mV[VY]) * RAD_TO_DEG;
	if (rotation < 0.f)
	{
		rotation += 360.f;
	}
	lua_pushnumber(state, rotation);

	return 1;
}

//static
int HBViewerAutomation::teleportAgentHome(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("TeleportAgentHome");
	}

	S32 n = lua_gettop(state);
	if (n)
	{
		luaL_error(state, "%d arguments passed; expected 0.", n);
	}

	gAgent.teleportHome();

	return 0;
}

//static
int HBViewerAutomation::teleportAgentToPos(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return 0;

	if (self->isThreaded())
	{
		HBAutomationThread* threadp = (HBAutomationThread*)self;
		return threadp->callMainFunction("TeleportAgentToPos");
	}

	F64 pos_x, pos_y, pos_z;
	bool preserve_look_at;
	S32 n = lua_gettop(state);
	if ((n == 1 || n == 2) && lua_type(state, 1) == LUA_TSTRING)
	{
		std::string pos_str(luaL_checkstring(state, 1));
		LLVector3d global_pos;
		if (!LLVector3d::parseVector3d(pos_str, &global_pos))
		{
			luaL_error(state, "Invalid position string: %s", pos_str.c_str());
		}
		pos_x = global_pos.mdV[VX];
		pos_y = global_pos.mdV[VY];
		pos_z = global_pos.mdV[VZ];
		preserve_look_at = n == 2 && lua_toboolean(state, 2);
	}
	else if (n >= 2 && n <= 4)
	{
		pos_x = luaL_checknumber(state, 1);
		pos_y = luaL_checknumber(state, 2);
		pos_z = n >= 3 ? luaL_checknumber(state, 3) : 0.0;
		preserve_look_at = n == 4 && lua_toboolean(state, 4);
	}
	else
	{
		luaL_error(state, "%d arguments passed; expected 2 to 4.", n);
		return 0;	// To avoid "<var> may be used uninitialized" gcc warnings
	}
	lua_pop(state, n);

	if (preserve_look_at)
	{
		gAgent.teleportViaLocationLookAt(LLVector3d(pos_x, pos_y, pos_z));
	}
	else
	{
		gAgent.teleportViaLocation(LLVector3d(pos_x, pos_y, pos_z));
	}

	return 0;
}

//static
void HBViewerAutomation::onIdleSimChange(void* userdata)
{
	LL_TRACY_TIMER(TRC_LUA_CALLBACK);

	HBViewerAutomation* self = (HBViewerAutomation*)userdata;
	if (!self || self != gAutomationp || !self->mHasOnFailedTPSimChange ||
		// Is a teleport in progress ?
		gAgent.teleportInProgress())
	{
		return;
	}

	// Are there a failed teleported sim handle and valid TP coordinates ?
	U64 handle = gAgent.getTeleportedSimHandle();
	if (!handle || gAgent.getTeleportedPosGlobal().isExactlyZero())
	{
		return;
	}

	LLSimInfo* siminfo = gWorldMap.simInfoFromHandle(handle);
	if (!siminfo)
	{
		return;
	}

	bool sim_is_down = siminfo->mAccess == SIM_ACCESS_DOWN;
	F64 update_interval = sim_is_down ? 15.0 : 4.0;
	F64 current_time = LLTimer::getElapsedSeconds();
	F64 delta = current_time - siminfo->mAgentsUpdateTime;
	if (delta > update_interval)
	{
		// Time to update our sim info
		siminfo->mAgentsUpdateTime = current_time;
		if (sim_is_down)
		{
			gWorldMap.sendHandleRegionRequest(handle);
		}
		else
		{
			gWorldMap.sendItemRequest(MAP_ITEM_AGENT_LOCATIONS, handle);
		}
	}
	else if (!sim_is_down)
	{
		// Count the number of agents in sim, if that data is available
		LLWorldMap::agent_list_map_t::iterator counts_iter =
			gWorldMap.mAgentLocationsMap.find(handle);
		if (counts_iter != gWorldMap.mAgentLocationsMap.end())
		{
			S32 sim_agent_count = 0;
			LLWorldMap::item_info_list_t& agentcounts = counts_iter->second;
			for (LLWorldMap::item_info_list_t::iterator
					iter = agentcounts.begin(), end = agentcounts.end();
				 iter != end; ++iter)
			{
				sim_agent_count += iter->mExtra;
			}
			// If the number of agents in the sim changed then fire the
			// OnFailedTPSimChange() Lua callback.
			if (sim_agent_count != siminfo->mAgentsCount)
			{
				siminfo->mAgentsCount = sim_agent_count;
				self->onFailedTPSimChange(sim_agent_count);
			}
		}
	}
}

//static
int HBViewerAutomation::callbackAfter(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	if (!state) return 0;	// Paranoia

	S32 n = lua_gettop(state);
	if (n < 2)
	{
		luaL_error(state, "%d arguments passed; expected at least 2.", n);
	}

	F32 delay = llclamp((F64)luaL_checknumber(state, 1), 1.0, (F64)F32_MAX);

	if (lua_type(state, 2) != LUA_TFUNCTION)
	{
		luaL_error(state, "The second argument must be a function");
	}

	// Store the function and the parameters into a table
	lua_newtable(state);
	for (S32 i = 2; i <= n; ++i)
	{
		lua_pushvalue(state, i);
		lua_rawseti(state, -2, i - 1);
	}
	// Store the number of elements in the table
	lua_pushliteral(state, "n");
	lua_pushinteger(state, n - 1);
	lua_rawset(state, -3);

	// Store the table into registry and get the corresponding unique reference
	int ref = luaL_ref(state, LUA_REGISTRYINDEX);

	lua_settop(state, 0);

	LL_DEBUGS("Lua") << "Queuing Lua callback with reference: " << ref
					 << " - Number of function arguments: " << n - 2
					 << LL_ENDL;

	doAfterInterval(boost::bind(&doAfterIntervalCallback, state, ref), delay);
	return 0;
}

//static
void HBViewerAutomation::doAfterIntervalCallback(lua_State* state, int ref)
{
	LL_TRACY_TIMER(TRC_LUA_CALLBACK);

	HBViewerAutomation* self = findInstance(state);
	if (!self) return;

	LL_DEBUGS("Lua") << "Invoking Lua callback associated with reference: "
					 << ref << LL_ENDL;

	// Get our table back from registry
	int type = lua_rawgeti(state, LUA_REGISTRYINDEX, ref);
	if (type != LUA_TTABLE)
	{
		llwarns << "Bad type (" << lua_typename(state, -1)
				<< ") for object referenced at: " << ref << ". Aborting."
				<< llendl;
		lua_settop(state, 0);
		return;
	}

	// Get the number of elements in the table
	lua_pushliteral(state, "n");
	type = lua_rawget(state, -2);
	if (type != LUA_TNUMBER)
	{
		llwarns << "Bad callback table format ('n' is missing or bears an invalid type). Aborting."
				<< llendl;
		lua_settop(state, 0);
		return;
	}
	S32 n = lua_tointeger(state, -1);
	lua_pop(state, 1);

	// Copy each table element back onto the stack
	LL_DEBUGS("Lua") << "Retrieving the function and " << n - 1
					 << " argument(s)" << LL_ENDL;
	for (S32 i = 1; i <= n; ++i)
	{
		lua_rawgeti(state, 1, i);
		if (i == 1 && lua_type(state, -1) != LUA_TFUNCTION)
		{
			llwarns << "Invalid callback table (no function). Aborting."
					<< llendl;
			return;
		}
	}

	// Remove the table
	lua_remove(state, -n - 1);

	// Dereference the callback data from LUA_REGISTRYINDEX
	luaL_unref(state, LUA_REGISTRYINDEX, ref);

	LL_DEBUGS("Lua") << "Calling the Lua function with " << n - 1
					 << " argument(s)" << LL_ENDL;
	self->resetTimer();
	if (lua_pcall(state, n - 1, 0, 0) != LUA_OK)
	{
		self->reportError();
	}
}

static void lua_force_quit()
{
	gAppViewerp->forceQuit();
}

//static
int HBViewerAutomation::forceQuit(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	HBViewerAutomation* self = findInstance(state);
	if (!self || self != gAutomationp) return 0;

	S32 n = lua_gettop(state);
	if (n > 1)
	{
		luaL_error(state, "%d arguments passed; expected 0 or 1.", n);
	}

	S32 exit_code = 0;
	if (n)
	{
		exit_code = luaL_checknumber(state, 1);
		lua_pop(state, 1);
	}
	if (exit_code &&
		(exit_code < LLAppViewer::VIEWER_EXIT_CODES || exit_code > 125))
	{
		luaL_error(state,
				   "Invalid exit code (must be 0 or in the range [%d-125]).",
				   LLAppViewer::VIEWER_EXIT_CODES);
	}
	gExitCode = exit_code;

	LLSD args;
	args["CODE"] = exit_code;
	gNotifications.add("LuaForceQuit", args);
	doAfterInterval(lua_force_quit, 5.f);

	return 0;
}

//static
int HBViewerAutomation::minimizeWindow(lua_State* state)
{
	LL_TRACY_TIMER(TRC_LUA_FUNCTION);

	if (!state || !gWindowp) return 0;	// Paranoia

	S32 n = lua_gettop(state);
	if (n)
	{
		luaL_error(state, "%d arguments passed; expected 0.", n);
	}

	gWindowp->minimize();

	return 0;
}

///////////////////////////////////////////////////////////////////////////////
// HBLuaSideBar class
///////////////////////////////////////////////////////////////////////////////

// This is the maximum number of buttons in the Lua side bar. The buttons must
// be named "btnN" with N=1 to 20 and appear in a sequence without hole in the
// numbering.
constexpr U32 MAX_NUMBER_OF_BUTTONS = 20;

HBLuaSideBar* gLuaSideBarp = NULL;

HBLuaSideBar::HBLuaSideBar()
:	LLPanel("lua side bar", LLRect(), BORDER_NO),
	mNumberOfButtons(0),
	mLeftSide(false),
	mHidden(false),
	mHideOnRightClick(false)
{
	llassert_always(gLuaSideBarp == NULL);	// Only one instance allowed

	LLUICtrlFactory::getInstance()->buildPanel(this,
											   "panel_lua_sidebar.xml");

	LLControlVariable* ctrl = gSavedSettings.getControl("LuaSideBarOnLeft");
	if (ctrl)
	{
		ctrl->getSignal()->connect(boost::bind(&HBLuaSideBar::handleSideChanged,
											   _2));
		mLeftSide = ctrl->getValue().asBoolean();
	}
	if (!mLeftSide)
	{
		setFollows(FOLLOWS_TOP | FOLLOWS_RIGHT);
	}

	setMouseOpaque(false);
	setIsChrome(true);
	setFocusRoot(true);
	setShape();

	std::string name;
	mCommands.resize(MAX_NUMBER_OF_BUTTONS);
	for (U32 i = 1; i <= MAX_NUMBER_OF_BUTTONS; ++i)
	{
		name = llformat("btn%d", i);
		LLButton* button = getChild<LLButton>(name.c_str(), true, false);
		if (!button) break;

		++mNumberOfButtons;
		mCommands.emplace_back("");
		button->setClickedCallback(onButtonClicked, (void*)(intptr_t)i);
		button->setVisible(false);
		button->setImageDisabled("square_button_disabled.tga");
		button->setImageUnselected("square_button_enabled.tga");
		button->setImageSelected("square_button_selected.tga");
	}
	LL_DEBUGS("Lua") << "Found " << mNumberOfButtons << " in the side bar"
					 << LL_ENDL;

	gLuaSideBarp = this;
}

//virtual
HBLuaSideBar::~HBLuaSideBar()
{
	gLuaSideBarp = NULL;
}

//virtual
void HBLuaSideBar::draw()
{
	if (!mActiveButtons.empty() && LLStartUp::isLoggedIn())
	{
		LLPanel::draw();
	}
}

//virtual
void HBLuaSideBar::reshape(S32 width, S32 height, bool called_from_parent)
{
	LLView::reshape(width, height, called_from_parent);
	setShape();
}

//virtual
void HBLuaSideBar::setVisible(bool visible)
{
	LLPanel::setVisible(visible && !mHidden);
}

//virtual
bool HBLuaSideBar::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
	if (mHideOnRightClick)
	{
		setHidden(true);
		return true;
	}

	return LLPanel::handleRightMouseDown(x, y, mask);
}

void HBLuaSideBar::setShape()
{
	if (gViewerWindowp)
	{
		LLRect rect = getRect();
		S32 height = rect.getHeight();
		S32 width = rect.getWidth();
		rect.mBottom = CHAT_BAR_HEIGHT +
					   (gViewerWindowp->getWindowHeight() - height) / 2;
		rect.mTop = rect.mBottom + height;
		if (mLeftSide)
		{
			rect.mLeft = 1;
			rect.mRight = rect.mLeft + width;
		}
		else
		{
			rect.mRight = gViewerWindowp->getWindowWidth() - 1;
			rect.mLeft = rect.mRight - width;
		}
		setRect(rect);
		updateBoundingRect();
	}
}

void HBLuaSideBar::setHidden(bool hidden)
{
	mHidden = hidden;
	LLPanel::setVisible(!hidden && !gAgent.cameraMouselook());
	if (gAutomationp)
	{
		gAutomationp->onSideBarVisibilityChange(!hidden);
	}
}

U32 HBLuaSideBar::setButton(U32 number, std::string icon, std::string command,
							const std::string& tooltip)
{
	if (number > mNumberOfButtons)
	{
		llwarns << "Invalid button number: " << number
				<< ". Valid range is 1 to " << mNumberOfButtons
				<< ", inclusive (and 0 for auto slot affectation)." << llendl;
		return 0;
	}
	if (!number)
	{
		// Find the first empty button slot, if any.
		for (U32 i = 1; i <= mNumberOfButtons; ++i)
		{
			if (!mActiveButtons.count(i))
			{
				number = i;
				break;
			}
		}
		if (!number)
		{
			llwarns << "No free button slot left: all "
				<< mNumberOfButtons << " are in use in the side bar."
				<< llendl;
			return 0;
		}
	}

	std::string name = llformat("btn%d", number);
	LLButton* button = getChild<LLButton>(name.c_str(), true, false);
	if (!button) return 0;

	if (command.empty() && !mCommands[number - 1].empty())
	{
		command = mCommands[number - 1];
	}
	else
	{
		// Reset any existing debug setting control and toggle state
		button->setControlName("", NULL);
		button->setToggleState(false);
		button->setIsToggle(false);
	}

	bool visible = !icon.empty() && !command.empty();
	if (visible)
	{
		// If first character is an UTF-8 one, or there are only 1 or 2 ASCII
		// characters, interpret the icon name as a text label.
		if ((U8)icon[0] > 127 || icon.length() < 3)
		{
			button->setLabel(icon);
			button->setImageOverlay(LLUIImagePtr(NULL));
		}
		else
		{
			LLFontGL::HAlign alignment = LLFontGL::HCENTER;
			size_t i = icon.find('|');
			if (i != std::string::npos && i < icon.length() - 1)
			{
				std::string align_str = icon.substr(0, i);
				icon = icon.substr(i + 1);
				if (align_str == "left")
				{
					alignment = LLFontGL::LEFT;
				}
				else if (align_str == "right")
				{
					alignment = LLFontGL::RIGHT;
				}
			}
			LLUIImagePtr image = LLUI::getUIImage(icon);
			if (image.notNull())
			{
				button->setLabel(LLStringUtil::null);
				button->setImageOverlay(image, alignment);
			}
		}
		mActiveButtons.insert(number);
		mCommands[number - 1] = command;
		button->setToolTip(tooltip);
	}
	else
	{
		mActiveButtons.erase(number);
		mCommands[number - 1].clear();
		button->setToolTip(LLStringUtil::null);
	}
	button->setVisible(visible);
	button->setEnabled(visible);
	LL_DEBUGS("Lua") << (visible ? "Set" : "Reset") << " button " << number
					 << LL_ENDL;
	return number;
}

S32 HBLuaSideBar::buttonToggle(U32 number, S32 toggle)
{
	if (number == 0 || number > mNumberOfButtons)
	{
		llwarns << "Invalid button number: " << number
				<< ". Valid range is 1 to " << mNumberOfButtons
				<< ", inclusive." << llendl;
		return -1;
	}

	std::string name = llformat("btn%d", number);
	LLButton* button = getChild<LLButton>(name.c_str(), true, false);
	if (!button || mCommands[number - 1].empty())
	{
		return -1;
	}

	S32 result = toggle;
	switch (toggle)
	{
		case 0:
		case 1:
			button->setIsToggle(true);
			button->setToggleState(toggle == 1);
			break;

		default:
			result = button->getIsToggle() ? button->getToggleState() : -1;
	}

	return result;
}

void HBLuaSideBar::buttonSetControl(U32 number, LLControlVariable* control)
{
	if (number == 0 || number > mNumberOfButtons)
	{
		llwarns << "Invalid button number: " << number
				<< ". Valid range is 1 to " << mNumberOfButtons
				<< ", inclusive." << llendl;
		return;
	}

	std::string name = llformat("btn%d", number);
	LLButton* button = getChild<LLButton>(name.c_str(), true, false);
	if (button && !mCommands[number - 1].empty())
	{
		// Avoid changing the control debug setting value
		if (control)
		{
			button->setIsToggle(true);
			button->setToggleState(control->getValue().asBoolean());
			button->setControlName(control->getName().c_str(), NULL);
		}
		else
		{
			button->setControlName(NULL, NULL);
			button->setIsToggle(false);
			button->setToggleState(false);
		}
	}
}

void HBLuaSideBar::setButtonEnabled(U32 number, bool enabled)
{
	if (number == 0 || number > mNumberOfButtons)
	{
		llwarns << "Invalid button number: " << number
				<< ". Valid range is 1 to " << mNumberOfButtons
				<< ", inclusive." << llendl;
		return;
	}

	std::string name = llformat("btn%d", number);
	LLButton* button = getChild<LLButton>(name.c_str(), true, false);
	if (button && !mCommands[number - 1].empty())
	{
		button->setEnabled(enabled);
	}
}

void HBLuaSideBar::setButtonVisible(U32 number, bool visible)
{
	if (number == 0 || number > mNumberOfButtons)
	{
		llwarns << "Invalid button number: " << number
				<< ". Valid range is 1 to " << mNumberOfButtons
				<< ", inclusive." << llendl;
		return;
	}

	std::string name = llformat("btn%d", number);
	LLButton* button = getChild<LLButton>(name.c_str(), true, false);
	if (button && !mCommands[number - 1].empty())
	{
		button->setVisible(visible);
	}
}

void HBLuaSideBar::removeAllButtons()
{
	std::string name;
	for (U32 i = 1; i <= mNumberOfButtons; ++i)
	{
		name = llformat("btn%d", i);
		LLButton* button = getChild<LLButton>(name.c_str(), true, false);
		if (button)
		{
			mCommands[i - 1].clear();
			button->setEnabled(false);
			button->setVisible(false);
			button->setControlName("", NULL);
			button->setToggleState(false);
			button->setIsToggle(false);
		}
	}
	mActiveButtons.clear();
}

//static
bool HBLuaSideBar::handleSideChanged(const LLSD& newvalue)
{
	if (gLuaSideBarp)
	{
		gLuaSideBarp->mLeftSide = newvalue.asBoolean();
		if (gLuaSideBarp->mLeftSide)
		{
			gLuaSideBarp->setFollows(FOLLOWS_TOP | FOLLOWS_LEFT);
		}
		else
		{
			gLuaSideBarp->setFollows(FOLLOWS_TOP | FOLLOWS_RIGHT);
		}
		gLuaSideBarp->setShape();
	}

	return true;
}

//static
void HBLuaSideBar::onButtonClicked(void* user_data)
{
	U32 button = (U32)(intptr_t)user_data;
	if (gLuaSideBarp && button > 0 && button <= gLuaSideBarp->mNumberOfButtons)
	{
		const std::string& command = gLuaSideBarp->mCommands[button - 1];
		if (!command.empty() && command != "nop")
		{
			LL_DEBUGS("Lua") << "Executing command associated with button "
							 << button << LL_ENDL;
			HBViewerAutomation::eval(command);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
// HBLuaPieMenu class
///////////////////////////////////////////////////////////////////////////////

HBLuaPieMenu* gLuaPiep = NULL;

HBLuaPieMenu::HBLuaPieMenu()
:	LLPieMenu("Lua pie menu"),
	mLastPickType(0)
{
	llassert_always(gLuaPiep == NULL);	// Only one instance allowed
	if (!gMenuHolderp)
	{
		llwarns << "Menu holder is NULL !  Aborted." << llendl;
		return;
	}

	const std::string filename = "menu_pie_lua.xml";
	LLXMLNodePtr root;
	if (!LLUICtrlFactory::getLayeredXMLNode(filename, root))
	{
		return;
	}
	if (!root->hasName(LL_PIE_MENU_TAG))
	{
		llwarns << "Root node should be named " << LL_PIE_MENU_TAG << " in: "
				<< filename  << ". Aborted." << llendl;
		return;
	}

	gMenuHolderp->addChild(this);
	initXML(root, gMenuHolderp, LLUICtrlFactory::getInstance());

	if (LLUI::sShowXUINames)
	{
		setToolTip(filename);
	}

	mLabels.reserve(48);
	mCommands.reserve(48);
	for (S32 i = 0; i < 48; ++i)
	{
		mLabels.emplace_back("");
		mCommands.emplace_back("");
	}

	gLuaPiep = this;
}

//virtual
HBLuaPieMenu::~HBLuaPieMenu()
{
	gLuaPiep = NULL;
}

void HBLuaPieMenu::removeAllSlices()
{
	for (S32 i = 0; i < 48; ++i)
	{
		mLabels[i].clear();
		mCommands[i].clear();
	}
}

// Here, we duplicate the same logic for pie menu types selection as found in
// LLToolPie::handleRightClickPick()
S32 HBLuaPieMenu::getPickedType(const LLPickInfo& pick, LLViewerObject* object)
{
	S32 type = PICKED_INVALID;
	if ((!object || !object->isHUDAttachment()) &&
		pick.mPickParticle && pick.mParticleOwnerID.notNull())
	{
		type = PICKED_PARTICLE;
	}
	else if (pick.mPickType == LLPickInfo::PICK_LAND)
	{
		type = PICKED_LAND;
	}
	else if (pick.mObjectID == gAgentID)
	{
		type = PICKED_SELF;
	}
	else if (object)
	{
		if (object->isAvatar())
		{
			type = PICKED_AVATAR;
		}
		else if (object->isAttachment())
		{
			type = PICKED_ATTACHMENT;
		}
		else
		{
			type = PICKED_OBJECT;
		}
	}
	return type;
}

S32 HBLuaPieMenu::getPickedType(const LLPickInfo& pick)
{
	const LLUUID& object_id = pick.mObjectID;
	if (!mLastPickType || mLastPickId.isNull() || mLastPickId != object_id)
	{
		mLastPickId = object_id;
		LLViewerObject* object = gObjectList.findObject(object_id);
		if (object && object->isAttachment() && !object->isHUDAttachment() &&
			!object->permYouOwner())
		{
			// Find the avatar corresponding to any attachment object we do not
			// own
			while (object->isAttachment())
			{
				object = (LLViewerObject*)object->getParent();
				if (!object) return PICKED_INVALID;	// Orphaned object ?
			}
		}
		mLastPickType = getPickedType(pick, object);
	}

	return mLastPickType;
}

bool HBLuaPieMenu::onPieMenu(const LLPickInfo& pick, LLViewerObject* object)
{
	mLastPickId = pick.mObjectID;

	mLastPickType = getPickedType(pick, object);
	if (mLastPickType == PICKED_INVALID)
	{
		return false;
	}

	LL_DEBUGS("Lua") << "Considering Lua pie menu type " << mLastPickType
					 << " for object " << mLastPickId << LL_ENDL;

	if (object && mLastPickType >= PICKED_OBJECT)
	{
//MK
		if (gRLenabled && !object->isAvatar() && LLFloaterTools::isVisible() &&
			!gRLInterface.canEdit(object))
		{
			gFloaterToolsp->close();
		}
//mk
		gMenuHolderp->setObjectSelection(gSelectMgr.getSelection());
	}

	bool got_slice = false;

	if (mLastPickType)
	{
		// Setup the pie slices, if any, according to the pick type
		std::string name;
		for (S32 i = 0; i < 8; ++i)
		{
			S32 j = 8 * mLastPickType + i;
			const std::string& label = mLabels[j];

			bool enabled = !label.empty() && !mCommands[j].empty();
			if (enabled)
			{
				got_slice = true;
			}

			name = llformat("slice%d", i + 1);
			LLMenuItemGL* item = getChild<LLMenuItemGL>(name.c_str(), true,
														false);
			if (item)
			{
				item->setValue(label);
				item->setEnabled(enabled);
			}
			else
			{
				llwarns_once << "Malformed menu_pie_lua.xml file" << llendl;
			}
		}
	}

	return got_slice;
}

void HBLuaPieMenu::onPieSliceClick(U32 slice, const LLPickInfo& pick)
{
	if (slice < 1 || slice > 8) return;

	S32 type = getPickedType(pick);
	if (type == PICKED_INVALID) return;

	S32 i = 8 * type + slice - 1;
	const std::string& command = mCommands[i];
	if (!command.empty() && command != "nop")
	{
		LL_DEBUGS("Lua") << "Executing command associated with pie slice "
						 << slice << " for pick type " << type << LL_ENDL;
		// Setup a pie menu specific Lua global variable
		std::string functions = "V_PIE_OBJ_ID=\"" + mLastPickId.asString() +
								"\";";
		// Setup a pie menu specific Lua function using the global variable
		functions += "function GetPickedObjectID();return V_PIE_OBJ_ID;end;";
		HBViewerAutomation::eval(functions + command);
	}

	if (gAutomationp)
	{
		gAutomationp->onLuaPieMenu(slice, mLastPickType, pick);
	}
}

void HBLuaPieMenu::setSlice(S32 type, U32 slice, const std::string& label,
							const std::string& command)
{
	if (type < 0 || type >= PICKED_INVALID)
	{
		llwarns << "Invalid type value: " << type << ". Valid range is "
				<< PICKED_LAND << " to " << PICKED_INVALID - 1
				<< ", inclusive." << llendl;
		return;
	}

	if (!slice)
	{
		LL_DEBUGS("Lua") << "Resetting pie type " << type << LL_ENDL;
		for (S32 i = 8 * type; i < 8 * type + 8; ++i)
		{
			mLabels[i].clear();
			mCommands[i].clear();
		}
		return;
	}

	if (slice > 8)
	{
		llwarns << "Invalid slice number: " << slice
				<< ". Valid range is 0 to 8, inclusive." << llendl;
		return;
	}

	S32 i = 8 * type + slice - 1;
	if (label.empty())
	{
		mLabels[i].clear();
		mCommands[i].clear();
		LL_DEBUGS("Lua") << "Reset slice " << slice << " for pie type " << type
						 << LL_ENDL;
	}
	else
	{
		mLabels[i] = label;
		if (!command.empty())
		{
			mCommands[i] = command;
		}
		LL_DEBUGS("Lua") << "Set slice " << slice << " for pie type " << type
						 << LL_ENDL;
	}
}
