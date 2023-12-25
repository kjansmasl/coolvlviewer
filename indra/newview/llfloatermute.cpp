/**
 * @file llfloatermute.cpp
 * @brief Container for mute list
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

#include "llfloatermute.h"

#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llscrolllistctrl.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "llfloateravatarinfo.h"
#include "llfloateravatarpicker.h"
#include "llfloatergroupinfo.h"
#include "llfloaterinspect.h"
#include "llmutelist.h"
#include "llviewerobjectlist.h"

//-----------------------------------------------------------------------------
// LLFloaterMuteObjectUI() - For handling mute object by name.
//-----------------------------------------------------------------------------

class LLFloaterMuteObjectUI final
:	public LLFloater,
	public LLFloaterSingleton<LLFloaterMuteObjectUI>

{
	friend class LLUISingleton<LLFloaterMuteObjectUI,
							   VisibilityPolicy<LLFloater> >;

public:
	~LLFloaterMuteObjectUI() override;

	typedef void (*callback_t)(const std::string&, void*);

	static LLFloaterMuteObjectUI* show(callback_t callback, void* userdata);

	bool postBuild() override;
	bool handleKeyHere(KEY key, MASK mask) override;

private:
	// Open via show() only
	LLFloaterMuteObjectUI(const LLSD&);

	// UI Callbacks
	static void onBtnOk(void* data);
	static void onBtnCancel(void* data);

	void (*mCallback)(const std::string& objectName, void* userdata);
	void* mCallbackUserData;
};

//static
LLFloaterMuteObjectUI* LLFloaterMuteObjectUI::show(callback_t callback,
												   void* userdata)
{
	// This will create a new instance if needed
	LLFloaterMuteObjectUI* self = getInstance();

	self->mCallback = callback;
	self->mCallbackUserData = userdata;
	self->open();

	return self;
}

LLFloaterMuteObjectUI::LLFloaterMuteObjectUI(const LLSD&)
:	mCallback(NULL),
	mCallbackUserData(NULL)
{
	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_mute_object.xml");
}

//virtual
LLFloaterMuteObjectUI::~LLFloaterMuteObjectUI()
{
	gFocusMgr.releaseFocusIfNeeded(this);
}

//virtual
bool LLFloaterMuteObjectUI::postBuild()
{
	childSetAction("OK", onBtnOk, this);
	childSetAction("Cancel", onBtnCancel, this);

	center();

	return true;
}

//virtual
bool LLFloaterMuteObjectUI::handleKeyHere(KEY key, MASK mask)
{
	if (key == KEY_RETURN && mask == MASK_NONE)
	{
		onBtnOk(this);
		return true;
	}
	else if (key == KEY_ESCAPE && mask == MASK_NONE)
	{
		onBtnCancel(this);
		return true;
	}

	return LLFloater::handleKeyHere(key, mask);
}

//static
void LLFloaterMuteObjectUI::onBtnOk(void* userdata)
{
	LLFloaterMuteObjectUI* self = (LLFloaterMuteObjectUI*)userdata;
	if (!self) return;

	if (self->mCallback)
	{
		const std::string& text = self->childGetValue("object_name").asString();
		self->mCallback(text, self->mCallbackUserData);
	}
	self->close();
}

//static
void LLFloaterMuteObjectUI::onBtnCancel(void* userdata)
{
	LLFloaterMuteObjectUI* self = (LLFloaterMuteObjectUI*)userdata;
	if (self)
	{
		self->close();
	}
}

//-----------------------------------------------------------------------------
// LLFloaterMute()
//-----------------------------------------------------------------------------

LLFloaterMute::LLFloaterMute(const LLSD&)
{
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_mute.xml");
}

//virtual
LLFloaterMute::~LLFloaterMute()
{
	LLMuteList::removeObserver(this);
}

//virtual
bool LLFloaterMute::postBuild()
{
	mMuteList = getChild<LLScrollListCtrl>("mutes");
	mMuteList->setCommitCallback(onSelectName);
	mMuteList->setDoubleClickCallback(onDoubleClickName);
	mMuteList->setCallbackUserData(this);
	mMuteList->setCommitOnSelectionChange(true);

	childSetAction("mute_resident", onClickPick, this);
	childSetAction("mute_by_name", onClickMuteByName, this);

	mUnmute = getChild<LLButton>("unmute");
	mUnmute->setClickedCallback(onClickRemove, this);

	mUpdateMutes = getChild<LLButton>("update_mutes");
	mUpdateMutes->setClickedCallback(onClickUpdateMutes, this);

	mMuteAll = getChild<LLCheckBoxCtrl>("mute_all");
	mMuteAll->setCommitCallback(onMuteAllToggled);
	mMuteAll->setCallbackUserData(this);

	mMuteChat = getChild<LLCheckBoxCtrl>("mute_chat");
	mMuteChat->setCommitCallback(onMuteTypeToggled);
	mMuteChat->setCallbackUserData(this);

	mMuteVoice = getChild<LLCheckBoxCtrl>("mute_voice");
	mMuteVoice->setCommitCallback(onMuteTypeToggled);
	mMuteVoice->setCallbackUserData(this);

	mMuteSound = getChild<LLCheckBoxCtrl>("mute_sounds");
	mMuteSound->setCommitCallback(onMuteTypeToggled);
	mMuteSound->setCallbackUserData(this);

	mMuteParticles = getChild<LLCheckBoxCtrl>("mute_particles");
	mMuteParticles->setCommitCallback(onMuteTypeToggled);
	mMuteParticles->setCallbackUserData(this);

	LLMuteList::addObserver(this);

	refreshMuteList();

	return true;
}

// LLMuteListObserver callback interface implementation.
//virtual
void LLFloaterMute::onChange()
{
	refreshMuteList();
}

void LLFloaterMute::refreshMuteList()
{
	// Remember any selected entry and the scroll position in the list
	S32 scrollpos = mMuteList->getScrollPos();
	LLScrollListItem* item = mMuteList->getFirstSelected();
	LLUUID selected_id;
	if (item)
	{
		selected_id = mMuteList->getValue().asUUID();
	}

	mMuteList->deleteAllItems();

	bool has_selected = false;
	std::string text;
	std::vector<LLMute> mutes = LLMuteList::getMutes();
	for (std::vector<LLMute>::iterator it = mutes.begin();
		 it != mutes.end(); ++it)
	{
		text = it->getNameAndType();
		const LLUUID& id = it->mID;
		if (id == selected_id)
		{
			has_selected = true;
		}

		LLScrollListItem* item = mMuteList->addStringUUIDItem(text, id);
		if (item && gObjectList.findObject(id))
		{
			LLScrollListText* text_cell =
				dynamic_cast<LLScrollListText*>(item->getColumn(0));
			if (text_cell)
			{
				text_cell->setFontStyle(LLFontGL::BOLD);
			}
		}
	}

	// Restore any selected item and scroll position in list
	mMuteList->setScrollPos(scrollpos);
	if (has_selected && selected_id.notNull())
	{
		mMuteList->selectByID(selected_id);
		mMuteList->scrollToShowSelected();
	}

	updateButtons();
}

void LLFloaterMute::updateButtons()
{
	bool selected = mMuteList->getFirstSelected() != NULL;
	bool enabled = selected;
	bool mute_all = false;
	U32 flags = 0;

	if (selected)
	{
		const LLUUID& id = mMuteList->getStringUUIDSelectedItem();
		std::vector<LLMute> mutes = LLMuteList::getMutes();
		std::vector<LLMute>::iterator it;
		for (it = mutes.begin(); it != mutes.end(); ++it)
		{
			if (it->mID == id)
			{
				break;
			}
		}
		if (it == mutes.end())
		{
			// Shoud never happen...
			enabled = false;
		}
		else
		{
			enabled = it->mType == LLMute::AGENT || it->mType == LLMute::GROUP;
			mute_all = it->mFlags == 0;
			if (!mute_all && enabled)
			{
				flags = ~(it->mFlags);
			}
		}
	}

	mUpdateMutes->setEnabled(false);	// Mutes are up to date
	mUnmute->setEnabled(selected);

	mMuteAll->setEnabled(enabled && !mute_all);
	mMuteChat->setEnabled(enabled);
	mMuteVoice->setEnabled(enabled);
	mMuteSound->setEnabled(enabled);
	mMuteParticles->setEnabled(enabled);

	mMuteAll->setValue(mute_all);
	mMuteChat->setValue((flags & LLMute::flagTextChat) != 0);
	mMuteVoice->setValue((flags & LLMute::flagVoiceChat) != 0);
	mMuteSound->setValue((flags & LLMute::flagObjectSounds) != 0);
	mMuteParticles->setValue((flags & LLMute::flagParticles) != 0);
}

//static
void LLFloaterMute::selectMute(const LLUUID& mute_id)
{
	// This will create a new instance if needed:
	LLFloaterMute* self = getInstance();
	if (self)	// Paranoia
	{
		self->mMuteList->selectByID(mute_id);
		self->mMuteList->scrollToShowSelected();
		self->updateButtons();
		self->open();
	}
}

//static
void LLFloaterMute::selectMute(const std::string& name)
{
	// This will create a new instance if needed:
	LLFloaterMute* self = getInstance();
	if (!self) return;	// Paranoia

	std::vector<LLScrollListItem*> data = self->mMuteList->getAllData();
	std::string label;
	LLUUID id;
	for (S32 i = 0, count = data.size(); i < count; ++i)
	{
		LLScrollListItem* item = data[i];
		id = item->getUUID();
		LLMute mute(id);
		label = item->getColumn(0)->getValue().asString();
		mute.setFromDisplayName(label); // trims off the suffix from mute.mName
		if (mute.mName == name)
		{
			self->mMuteList->selectItem(item);
			self->mMuteList->scrollToShowSelected();
			break;
		}
	}
	self->updateButtons();
	self->open();
}

//static
void LLFloaterMute::onDoubleClickName(void* data)
{
	LLFloaterMute* self = (LLFloaterMute*)data;
	if (self)
	{
		const LLUUID& id = self->mMuteList->getStringUUIDSelectedItem();
		std::vector<LLMute> mutes = LLMuteList::getMutes();
		std::vector<LLMute>::iterator it;
		for (it = mutes.begin(); it != mutes.end(); ++it)
		{
			if (it->mID == id)
			{
				break;
			}
		}
		if (it != mutes.end())
		{
			if (it->mType == LLMute::AGENT)
			{
				LLFloaterAvatarInfo::show(id);
			}
			else if (it->mType == LLMute::GROUP)
			{
				LLFloaterGroupInfo::showFromUUID(id);
			}
			else if (it->mType == LLMute::OBJECT)
			{
				LLViewerObject* objectp = gObjectList.findObject(id);
				if (objectp)
				{
					LLFloaterInspect::show(objectp);
				}
			}
		}
	}
}

//static
void LLFloaterMute::onSelectName(LLUICtrl* caller, void* data)
{
	LLFloaterMute* self = (LLFloaterMute*)data;
	if (self)
	{
		self->updateButtons();
	}
}

//static
void LLFloaterMute::onClickRemove(void* data)
{
	LLFloaterMute* self = (LLFloaterMute*)data;
	if (!self) return;

	std::string name = self->mMuteList->getSelectedItemLabel();
	LLUUID id = self->mMuteList->getStringUUIDSelectedItem();
	LLMute mute(id);
	mute.setFromDisplayName(name); // Now mute.mName has the suffix trimmed off

	S32 last_selected = self->mMuteList->getFirstSelectedIndex();
	if (LLMuteList::remove(mute))
	{
		// Above removals may rebuild this dialog.

		if (last_selected == self->mMuteList->getItemCount())
		{
			// We were on the last item, so select the last item again
			self->mMuteList->selectNthItem(last_selected - 1);
		}
		else
		{
			// Else select the item after the last item previously selected
			self->mMuteList->selectNthItem(last_selected);
		}
	}
	self->updateButtons();
	self->mMuteList->deselectAllItems(true);
}

//static
void LLFloaterMute::onClickPick(void* data)
{
	LLFloaterMute* self = (LLFloaterMute*)data;
	if (self)
	{
		LLFloaterAvatarPicker* picker =
			// Not allowing multiple selection, with close on select
			LLFloaterAvatarPicker::show(onPickUser, data, false, true);
		self->addDependentFloater(picker);
	}
}

//static
void LLFloaterMute::onPickUser(const std::vector<std::string>& names,
							   const std::vector<LLUUID>& ids, void* data)
{
	LLFloaterMute* self = (LLFloaterMute*)data;
	if (self && !names.empty() && !ids.empty())
	{
		LLMute mute(ids[0], names[0], LLMute::AGENT);
		LLMuteList::add(mute);
		self->updateButtons();
	}
}

//static
void LLFloaterMute::onClickMuteByName(void* data)
{
	LLFloaterMute* self = (LLFloaterMute*)data;
	if (self)
	{
		LLFloaterMuteObjectUI* picker;
		picker = LLFloaterMuteObjectUI::show(callbackMuteByName, data);
		if (picker)
		{
			self->addDependentFloater(picker);
		}
	}
}

//static
void LLFloaterMute::callbackMuteByName(const std::string& text, void*)
{
	if (!text.empty())
	{
		LLMute mute(LLUUID::null, text, LLMute::BY_NAME);
		LLMuteList::add(mute);
	}
}

//static
void LLFloaterMute::onMuteAllToggled(LLUICtrl*, void* data)
{
	LLFloaterMute* self = (LLFloaterMute*)data;
	if (self)
	{
		self->mMuteChat->setValue(false);
		self->mMuteVoice->setValue(false);
		self->mMuteSound->setValue(false);
		self->mMuteParticles->setValue(false);
		self->mUpdateMutes->setEnabled(true);
	}
}

//static
void LLFloaterMute::onMuteTypeToggled(LLUICtrl* ctrl, void* data)
{
	LLFloaterMute* self = (LLFloaterMute*)data;
	LLCheckBoxCtrl* check = (LLCheckBoxCtrl*)ctrl;
	if (!self || !check) return;

	if (check->get())
	{
		self->mMuteAll->setValue(false);
		self->mMuteAll->setEnabled(true);
	}
	else
	{
		bool enabled = !(self->mMuteChat->get() || self->mMuteVoice->get() ||
						 self->mMuteSound->get() ||
						 self->mMuteParticles->get());
		self->mMuteAll->setValue(enabled);
		self->mMuteAll->setEnabled(!enabled);
	}
	self->mUpdateMutes->setEnabled(true);
}

//static
void LLFloaterMute::onClickUpdateMutes(void* data)
{
	LLFloaterMute* self = (LLFloaterMute*)data;
	if (!self) return;

	std::string name = self->mMuteList->getSelectedItemLabel();
	LLUUID id = self->mMuteList->getStringUUIDSelectedItem();
	LLMute mute(id);
	mute.setFromDisplayName(name); // now mute.mName has the suffix trimmed off

	U32 flags = 0;
	if (!self->mMuteAll->get())
	{
		if (self->mMuteChat->get())
		{
			flags |= LLMute::flagTextChat;
		}
		if (self->mMuteVoice->get())
		{
			flags |= LLMute::flagVoiceChat;
		}
		if (self->mMuteSound->get())
		{
			flags |= LLMute::flagObjectSounds;
		}
		if (self->mMuteParticles->get())
		{
			flags |= LLMute::flagParticles;
		}
	}

	// Refresh the mute entry by removing the mute then re-adding it.
	S32 last_selected = self->mMuteList->getFirstSelectedIndex();
	LLMuteList::remove(mute);
	LLMuteList::add(mute, flags);
	self->mMuteList->selectNthItem(last_selected);

	self->updateButtons();
}
