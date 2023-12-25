/**
* @file lleditmenuhandler.h
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

#ifndef LL_LLEDITMENUHANDLER_H
#define LL_LLEDITMENUHANDLER_H

#include "llview.h"

class LLMenuGL;

struct HBContextMenuData
{
	// Operation types, for use in mOperation
	enum { SET = 0, CUT = 1, COPY = 2, PASTE = 3 };

	std::string	mMenuType;
	U32			mHandlerID;
	S32			mOperation;
};

constexpr U32 NO_CONTEXT_MENU	= 0x00;
constexpr U32 HAS_CONTEXT_MENU	= 0x01;
constexpr U32 HAS_UNDO_REDO		= 0x02;
constexpr U32 HAS_CUSTOM		= 0x04;

// Custom menu entries global callback: the HBContextMenuData structure
// instance pointed to by datap is created by LLEditMenuHandler and must be
// deleted by the callback. HB
typedef void (*context_menu_cb_t)(HBContextMenuData* datap);

// Interface used by menu system for plug-in hotkey/menu handling
class LLEditMenuHandler
{
public:
	LLEditMenuHandler(U32 context_menu_flags = NO_CONTEXT_MENU);
	virtual ~LLEditMenuHandler();

	LL_INLINE U32 getID() const						{ return mID; }

	// Used by the text and line editors.

	LL_INLINE virtual void cut()					{}
	LL_INLINE virtual bool canCut() const			{ return false; }

	LL_INLINE virtual void copy()					{}
	LL_INLINE virtual bool canCopy() const			{ return false; }

	LL_INLINE virtual void paste()					{}
	LL_INLINE virtual bool canPaste() const			{ return false; }

	// "doDelete" since "delete" is a C++ keyword...
	LL_INLINE virtual void doDelete()				{}
	LL_INLINE virtual bool canDoDelete() const		{ return false; }

	LL_INLINE virtual void selectAll()				{}
	LL_INLINE virtual bool canSelectAll() const		{ return false; }

	LL_INLINE virtual void deselect()				{}
	LL_INLINE virtual bool canDeselect() const		{ return false; }

	// Used by the text editor and the selection manager.

	LL_INLINE virtual void undo()					{}
	LL_INLINE virtual bool canUndo() const			{ return false; }

	LL_INLINE virtual void redo()					{}
	LL_INLINE virtual bool canRedo() const			{ return false; }

	// Used only by the selection manager.

	LL_INLINE virtual void duplicate()				{}
	LL_INLINE virtual bool canDuplicate() const		{ return false; }

	// Used to set the 'type' of the editor handler, which is an arbitrary
	// string used to determine how to deal with the menu data in the global
	// custom callback (e.g. for a "script" editor, or a "note card" editor).
	// Whenever the custom callback is already set, it gets triggered by this
	// method with a SET operation type. HB
	void setCustomMenuType(const char* type);

	// Used to set the labels for the context menu custom entries, passing
	// an empty string (or omitting it) causes the corresponding entry to be
	// hidden. HB
	LL_INLINE void setCustomMenu(const std::string& cut = LLStringUtil::null,
								 const std::string& copy = LLStringUtil::null,
								 const std::string& paste = LLStringUtil::null)
	{
		mCustomCutLabel = cut;
		mCustomCopyLabel = copy;
		mCustomPasteLabel = paste;
		updateCustomEntries();
	}

	// Same as above, but using a menu handler Id: typically used via a Lua
	// callback, in reply to a SET operation. Returns true when successful
	// (i.e. when menu_handler_id is valid). HB
	static bool setCustomMenu(U32 menu_handler_id,
							  const std::string& cut = LLStringUtil::null,
							  const std::string& copy = LLStringUtil::null,
							  const std::string& paste = LLStringUtil::null);

	// Used to set the the global custom callback for all context menus. This
	// is currently only used by the Lua automation script, since this is why
	// I implemented the custom menu entries in the first place. HB
	LL_INLINE static void setCustomCallback(context_menu_cb_t callback)
	{
		sContextMenuCallback = callback;
	}

	// Called, maybe asynchronously, as a result of a PASTE action sent to
	// sContextMenuCallback, to actually paste the text into the UI element
	// linked to this menu handler. Returns true when menu_handler_id was valid
	// and the text could be pasted, false otherwise. HB
	static bool pasteTo(U32 menu_handler_id);

protected:
	// Grabs (sets to 'this') unconditionally the global menu handler pointer.
	void grabMenuHandler();
	// Releases (sets to NULL) the global menu handler pointer if it is
	// currently held by this instance (set to 'this').
	void releaseMenuHandler();

	// When it does not exist (mPopupMenuHandle.get() == NULL), creates a
	// context menu and returns its pointer. When the menu already exists,
	// it returns the pointer for the current menu. When it is passed false
	// for with_spell_separator, it does not add a menu item separator at the
	// end of the menu (used to separate editing actions items from spell
	// checking menu entries and suggestions). HB
	LLMenuGL* createContextMenu(bool with_spell_separator = true);

	// Returns the pointer associated to mPopupMenuHandle, which may be NULL
	// when the menu has not yet been created or got deleted for this menu
	// handler. This also updates the custom menu entries labels and visibility
	// as needed. HB
	LLMenuGL* getContextMenu();

private:
	void updateCustomEntries();

	// Context menu actions
	static void contextSelectall(void* data);
	static void contextCut(void* data);
	static void contextCutCustom(void* data);
	static void contextCopy(void* data);
	static void contextCopyCustom(void* data);
	static void contextPaste(void* data);
	static void contextPasteCustom(void* data);
	static void contextDelete(void* data);
	static void contextUndo(void* data);
	static void contextRedo(void* data);
	static bool contextEnableSelectall(void* data);
	static bool contextEnableCut(void* data);
	static bool contextEnableCopy(void* data);
	static bool contextEnablePaste(void* data);
	static bool contextEnableDelete(void* data);
	static bool contextEnableUndo(void* data);
	static bool contextEnableRedo(void* data);

private:
	LLHandle<LLView>			mPopupMenuHandle;

	U32							mID;
	const U32					mContextMenuFlags;

	std::string					mCustomMenuType;
	std::string					mCustomCutLabel;
	std::string					mCustomCopyLabel;
	std::string					mCustomPasteLabel;

	static context_menu_cb_t	sContextMenuCallback;
	typedef flat_hmap<U32, LLEditMenuHandler*> map_t;
	static map_t				sMenuHandlersMap;
	static U32					sNextID;
};

extern LLEditMenuHandler* gEditMenuHandlerp;

#endif	// LL_LLEDITMENUHANDLER_H
