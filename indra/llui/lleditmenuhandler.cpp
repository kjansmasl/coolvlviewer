/**
* @file lleditmenuhandler.cpp
* @authors Aaron Yonas, James Cook
*
* $LicenseInfo:firstyear=2006&license=viewergpl$
*
* Copyright (c) 2006-2009, Linden Research, Inc.
* Copyright (c) 2009-2023, Henri Beauchamp.
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

#include "linden_common.h"

#include "lleditmenuhandler.h"

#include "llmenugl.h"

// Global variable
LLEditMenuHandler* gEditMenuHandlerp = NULL;

// Static member variable
U32 LLEditMenuHandler::sNextID = 0;
LLEditMenuHandler::map_t LLEditMenuHandler::sMenuHandlersMap;
context_menu_cb_t LLEditMenuHandler::sContextMenuCallback = NULL;

LLEditMenuHandler::LLEditMenuHandler(U32 context_menu_flags)
:	mID(sNextID++),
	mContextMenuFlags(context_menu_flags)
{
	// Avoids a crash on startup seen with some compilers (e.g. clang with LTO)
	// due to the fact that LLSelectMgr is an LLEditMenuHandler and gets
	// initialized as a global gSelectMgr instance, before sMenuHandlersMap is
	// itself initialized on program startup (this is a compiler or linker bug:
	// either of them did not properly determine the order in which the modules
	// static and global variables/instances must be created on startup)...
	// For this kind of LLEditMenuHandler, context_menu_flags==0 since these
	// instances are not line or text editors, and they do not care about,
	// neither make any use of sMenuHandlersMap anyway. HB
	if (context_menu_flags)
	{
		sMenuHandlersMap.emplace(mID, this);
	}
}

//virtual
LLEditMenuHandler::~LLEditMenuHandler()
{
	releaseMenuHandler();
	if (mContextMenuFlags)
	{
		sMenuHandlersMap.erase(mID);
	}
	LLView::deleteViewByHandle(mPopupMenuHandle);
}

void LLEditMenuHandler::grabMenuHandler()
{
	gEditMenuHandlerp = this;
}

void LLEditMenuHandler::releaseMenuHandler()
{
	if (gEditMenuHandlerp == this)
	{
		gEditMenuHandlerp = NULL;
	}
}

void LLEditMenuHandler::setCustomMenuType(const char* type)
{
	if (mContextMenuFlags | HAS_CUSTOM)
	{
		mCustomMenuType = type;
		if (sContextMenuCallback)
		{
			HBContextMenuData* menudatap = new HBContextMenuData;
			menudatap->mHandlerID = mID;
			menudatap->mMenuType = mCustomMenuType;
			menudatap->mOperation = HBContextMenuData::SET;
			sContextMenuCallback(menudatap);
		}
	}
}

//static
bool LLEditMenuHandler::setCustomMenu(U32 menu_handler_id,
									  const std::string& cut_label,
							  		  const std::string& copy_label,
									  const std::string& paste_label)
{
	map_t::const_iterator it = sMenuHandlersMap.find(menu_handler_id);
	if (it == sMenuHandlersMap.end() ||
		!(it->second->mContextMenuFlags | HAS_CUSTOM))
	{
		return false;
	}
	it->second->setCustomMenu(cut_label, copy_label, paste_label);
	return true;
}

//static
void LLEditMenuHandler::contextSelectall(void* data)
{
	LLEditMenuHandler* self = (LLEditMenuHandler*)data;
	if (self)
	{
		self->selectAll();
	}
}

//static
bool LLEditMenuHandler::contextEnableSelectall(void* data)
{
	LLEditMenuHandler* self = (LLEditMenuHandler*)data;
	return self && self->canSelectAll();
}

//static
void LLEditMenuHandler::contextCut(void* data)
{
	LLEditMenuHandler* self = (LLEditMenuHandler*)data;
	if (self)
	{
		self->cut();
	}
}

//static
void LLEditMenuHandler::contextCutCustom(void* data)
{
	LLEditMenuHandler* self = (LLEditMenuHandler*)data;
	if (!self || !sContextMenuCallback)
	{
		return;
	}
	self->cut();
	HBContextMenuData* menudatap = new HBContextMenuData;
	menudatap->mHandlerID = self->mID;
	menudatap->mMenuType = self->mCustomMenuType;
	menudatap->mOperation = HBContextMenuData::CUT;
	sContextMenuCallback(menudatap);
}

//static
bool LLEditMenuHandler::contextEnableCut(void* data)
{
	LLEditMenuHandler* self = (LLEditMenuHandler*)data;
	return self && self->canCut();
}

//static
void LLEditMenuHandler::contextCopy(void* data)
{
	LLEditMenuHandler* self = (LLEditMenuHandler*)data;
	if (self)
	{
		self->copy();
	}
}

void LLEditMenuHandler::contextCopyCustom(void* data)
{
	LLEditMenuHandler* self = (LLEditMenuHandler*)data;
	if (!self || !sContextMenuCallback)
	{
		return;
	}
	self->copy();
	HBContextMenuData* menudatap = new HBContextMenuData;
	menudatap->mHandlerID = self->mID;
	menudatap->mMenuType = self->mCustomMenuType;
	menudatap->mOperation = HBContextMenuData::COPY;
	sContextMenuCallback(menudatap);
}

//static
bool LLEditMenuHandler::contextEnableCopy(void* data)
{
	LLEditMenuHandler* self = (LLEditMenuHandler*)data;
	return self && self->canCopy();
}

//static
void LLEditMenuHandler::contextPaste(void* data)
{
	LLEditMenuHandler* self = (LLEditMenuHandler*)data;
	if (self)
	{
		self->paste();
	}
}

//static
void LLEditMenuHandler::contextPasteCustom(void* data)
{
	LLEditMenuHandler* self = (LLEditMenuHandler*)data;
	if (!self || !sContextMenuCallback)
	{
		return;
	}
	HBContextMenuData* menudatap = new HBContextMenuData;
	menudatap->mHandlerID = self->mID;
	menudatap->mMenuType = self->mCustomMenuType;
	menudatap->mOperation = HBContextMenuData::PASTE;
	sContextMenuCallback(menudatap);
}

//static
bool LLEditMenuHandler::pasteTo(U32 id)
{
	map_t::const_iterator it = sMenuHandlersMap.find(id);
	if (it != sMenuHandlersMap.end() && it->second->canPaste())
	{
		it->second->paste();
		return true;
	}
	return false;
}

//static
bool LLEditMenuHandler::contextEnablePaste(void* data)
{
	LLEditMenuHandler* self = (LLEditMenuHandler*)data;
	return self && self->canPaste();
}

//static
void LLEditMenuHandler::contextDelete(void* data)
{
	LLEditMenuHandler* self = (LLEditMenuHandler*)data;
	if (self)
	{
		self->doDelete();
	}
}

//static
bool LLEditMenuHandler::contextEnableDelete(void* data)
{
	LLEditMenuHandler* self = (LLEditMenuHandler*)data;
	return self && self->canDoDelete();
}

//static
void LLEditMenuHandler::contextUndo(void* data)
{
	LLEditMenuHandler* self = (LLEditMenuHandler*)data;
	if (self)
	{
		self->undo();
	}
}

//static
bool LLEditMenuHandler::contextEnableUndo(void* data)
{
	LLEditMenuHandler* self = (LLEditMenuHandler*)data;
	return self && self->canUndo();
}

//static
void LLEditMenuHandler::contextRedo(void* data)
{
	LLEditMenuHandler* self = (LLEditMenuHandler*)data;
	if (self)
	{
		self->redo();
	}
}

//static
bool LLEditMenuHandler::contextEnableRedo(void* data)
{
	LLEditMenuHandler* self = (LLEditMenuHandler*)data;
	return self && self->canRedo();
}

LLMenuGL* LLEditMenuHandler::createContextMenu(bool with_spell_separator)
{
	if (!mContextMenuFlags)
	{
		return NULL;
	}
	LLMenuGL* menup = (LLMenuGL*)mPopupMenuHandle.get();
	if (menup)
	{
		return menup;
	}
	menup = new LLMenuGL("editor_context_menu");
	mPopupMenuHandle = menup->getHandle();
	// *TODO: translate
	menup->append(new LLMenuItemCallGL("Select all", contextSelectall,
									   contextEnableSelectall, this));
	menup->appendSeparator("sep1");
	menup->append(new LLMenuItemCallGL("Cut", contextCut,
									   contextEnableCut, this));
	menup->append(new LLMenuItemCallGL("Copy", contextCopy,
									   contextEnableCopy, this));
	menup->append(new LLMenuItemCallGL("Paste", contextPaste,
									   contextEnablePaste, this));
	menup->append(new LLMenuItemCallGL("Delete", contextDelete,
									   contextEnableDelete, this));
	if (mContextMenuFlags | HAS_UNDO_REDO)
	{
		menup->append(new LLMenuItemCallGL("Undo", contextUndo,
										   contextEnableUndo, this));
		menup->append(new LLMenuItemCallGL("Redo", contextRedo,
										   contextEnableRedo, this));
	}
	if (mContextMenuFlags | HAS_CUSTOM)
	{
		menup->appendSeparator("custom_sep");
		menup->append(new LLMenuItemCallGL("Custom cut", contextCutCustom,
										   contextEnableCut, this));
		menup->append(new LLMenuItemCallGL("Custom copy", contextCopyCustom,
										   contextEnableCopy, this));
		menup->append(new LLMenuItemCallGL("Custom paste", contextPasteCustom,
										   contextEnablePaste, this));
		updateCustomEntries();
	}
	if (with_spell_separator)
	{
		menup->appendSeparator("spell_sep");
		menup->setItemVisible("spell_sep", false);
	}
	menup->setCanTearOff(false);
	menup->setVisible(false);
	return menup;
}

LLMenuGL* LLEditMenuHandler::getContextMenu()
{
	LLMenuGL* menup = (LLMenuGL*)mPopupMenuHandle.get();
	if (menup && (mContextMenuFlags | HAS_CUSTOM))
	{
		updateCustomEntries();
	}
	return menup;
}

void LLEditMenuHandler::updateCustomEntries()
{
	LLMenuGL* menup = (LLMenuGL*)mPopupMenuHandle.get();
	if (!menup || !(mContextMenuFlags | HAS_CUSTOM))
	{
		return;
	}
	bool has_custom_callback = sContextMenuCallback != NULL;
	bool sep_visible = false;

	LLMenuItemGL* itemp = menup->getItem("Custom cut");
	if (itemp)
	{
		if (!has_custom_callback || mCustomCutLabel.empty())
		{
			itemp->setVisible(false);
		}
		else
		{
			itemp->setLabel(mCustomCutLabel);
			itemp->setVisible(true);
			sep_visible = true;
		}
	}
	itemp = menup->getItem("Custom copy");
	if (itemp)
	{
		if (!has_custom_callback || mCustomCopyLabel.empty())
		{
			itemp->setVisible(false);
		}
		else
		{
			itemp->setLabel(mCustomCopyLabel);
			itemp->setVisible(true);
			sep_visible = true;
		}
	}
	itemp = menup->getItem("Custom paste");
	if (itemp)
	{
		if (!has_custom_callback || mCustomPasteLabel.empty())
		{
			itemp->setVisible(false);
		}
		else
		{
			itemp->setLabel(mCustomPasteLabel);
			itemp->setVisible(true);
			sep_visible = true;
		}
	}

	menup->setItemVisible("custom_sep", sep_visible);
}
