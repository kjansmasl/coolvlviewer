/** 
 * @file llviewermenu.h
 * @brief Builds menus out of objects
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 * 
 * Copyright (c) 2002-2009, Linden Research, Inc.
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

#ifndef LL_LLVIEWERMENU_H
#define LL_LLVIEWERMENU_H

#include <string>

#include "llassetstorage.h"
#include "llfoldertype.h"
#include "llmenugl.h"
#include "llsafehandle.h"
#include "llvector3.h"

class LLUICtrl;
class LLView;
class LLParcelSelection;
class LLObjectSelection;
class LLTransactionID;
class LLViewerObject;
class LLVOAvatar;

// Called in llviewerwindow.cpp
void pre_init_menus();
void init_menus();
void cleanup_menus();

// Called in llviewercontrol.cpp
void show_debug_menus();
void clear_assets_cache(void*);

// Called from several places. Returns true when successfully executed or
// false when delayed (when in customize avatar mode and when wearables are
// dirty) or refused (camera constrained by RestrainedLove).
bool handle_reset_view();

// Called in llfloaterpathfindingobjects.
void handle_copy(void*);
void handle_take();
void handle_take_copy();
void handle_object_delete();
void handle_object_return();
bool visible_take_object();
bool enable_object_take_copy();
bool enable_object_return();
bool enable_object_delete();

// Called in lltoolpie.cpp
void handle_buy(void*);
bool handle_sit_or_stand();
bool handle_give_money_dialog();
bool handle_object_open();
bool handle_go_to();

// Called from llstartup.cpp
void set_underclothes_menu_options();

// Called from llstartup.cpp and llviewermessage.cpp
void update_upload_costs_in_menus();

// Called from llappearancemgr.cpp
void confirm_replace_attachment(S32 option, void* user_data);

// Called from llvoavatarself.cpp
bool object_attached(void* user_data);
bool object_selected_and_point_valid(void*);
void handle_detach(void*);
void handle_detach_from_avatar(void* user_data);
void detach_label(std::string& label, void* user_data);
// Called from llinventorybridge.cpp and llvoavatarself.cpp
void attach_label(std::string& label, void* user_data);

// Called from llpuppetmotion.cpp
void handle_reset_avatars_animations(void*);

// Called from llstatusbar.cpp and llfloateravatartextures.cpp
void handle_rebake_textures(void*);

// Formerly declared in the now removed llmenucommands.h
// Called from lltoolbar.cpp and hbviewerautomation.cpp
void handle_chat(void*);
void handle_inventory(void*);

#if 0
// Export to XML or Collada
void handle_export_selected(void*);
#endif

//MK
// Called from mkrlinterface.cpp
void handle_toggle_wireframe(void*);
void handle_toggle_flycam();
//mk

// Called from llfloatertools.cpp
void select_face_or_linked_prim(const std::string& action);

// Called from llinventorybridge.cpp
bool handle_object_edit();
bool handle_object_inspect();

// Called from llfloateravatartextures.cpp
// When refresh_all = false, only refresh the backed textures.
void handle_refresh_avatar(LLVOAvatar* avatar, bool refresh_all);
bool enable_avatar_textures(void*);

// Called from hbviewerautomation.cpp
bool sit_on_ground();
bool sit_on_object(LLViewerObject* object,
				   const LLVector3& offset = LLVector3::zero);
bool stand_up();
bool derender_object(const LLUUID& object_id);

// *HACK: called from llagent.cpp and llstartup.cpp, each time we need to
// ensure that all objects will properly rez after the agent moved into a new
// region or TPed beyond draw distance in the same region.
// 'type' is 1 for refresh on login, 2 for refresh on sim border crossing, and
// 4 for refresh after a TP. The type is compared against the value of the
// "ObjectsVisibilityAutoRefreshMask" debug setting to determine whether an
// actual refresh will happen or not.
// *TODO: find the race condition causing the failed rezzing; probably in the
// code for objects caching or culling... HB
void schedule_objects_visibility_refresh(U32 type);
// Called (without delay) from in mkrlinterface.cpp
void handle_objects_visibility(void*);

// Pass in an empty string and this function will build a string that
// describes buyer permissions.
class LLSaleInfo;
class LLPermissions;

class LLViewerMenuHolderGL : public LLMenuHolderGL
{
public:
	LLViewerMenuHolderGL();

	virtual bool hideMenus();
	
	void setParcelSelection(LLSafeHandle<LLParcelSelection> selection);
	void setObjectSelection(LLSafeHandle<LLObjectSelection> selection);

	virtual const LLRect getMenuRect() const;

protected:
	LLSafeHandle<LLParcelSelection> mParcelSelection;
	LLSafeHandle<LLObjectSelection> mObjectSelection;
};

void handle_compress_image(void*);	// Used in the debug menu (see llviewermenu.cpp)

///////////////////////////////////////////////////////////////////////////////
// Global variables
///////////////////////////////////////////////////////////////////////////////

extern LLMenuBarGL*				gMenuBarViewp;
extern LLViewerMenuHolderGL*	gMenuHolderp;
extern LLMenuBarGL*				gLoginMenuBarViewp;

// Pie menus
extern LLPieMenu* gPieSelfp;
extern LLPieMenu* gPieAvatarp;
extern LLPieMenu* gPieObjectp;
extern LLPieMenu* gPieAttachmentp;
extern LLPieMenu* gPieLandp;
extern LLPieMenu* gPieParticlep;

// Sub-menu of gPieAvatarp
extern LLPieMenu* gMutesPieMenup;
// Sub-menu of gPieObjectp
extern LLPieMenu* gPieObjectMutep;

// Needed to build menus when attachment site list available
extern LLMenuGL*  gAttachSubMenup;
extern LLMenuGL*  gDetachSubMenup;
extern LLPieMenu* gAttachScreenPieMenup;
extern LLPieMenu* gDetachScreenPieMenup;
extern LLPieMenu* gAttachPieMenup;
extern LLPieMenu* gDetachPieMenup;

#endif
