/**
 * @file llfloaterinspect.h
 * @brief Declaration of floaters for object and avatar inspection tool
 *
 * $LicenseInfo:firstyear=2006&license=viewergpl$
 *
 * Copyright (c) 2006-2009, Linden Research, Inc, 2009-2023 Henri Beauchamp.
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

#ifndef LL_LLFLOATERINSPECT_H
#define LL_LLFLOATERINSPECT_H

#include "llfloater.h"
#include "llsafehandle.h"

#include "llvoinventorylistener.h"

class LLButton;
class LLIconCtrl;
class LLObjectSelection;
class LLScrollListCtrl;

class LLFloaterInspect final : public LLFloater,
							   public LLFloaterSingleton<LLFloaterInspect>,
							   public LLVOInventoryListener
{
	friend class LLUISingleton<LLFloaterInspect, VisibilityPolicy<LLFloater> >;

public:
	~LLFloaterInspect() override;

	bool postBuild() override;
	void draw() override;
	void refresh() override;
	void onFocusReceived() override;

	// LLVOInventoryListener interface
	void inventoryChanged(LLViewerObject* obj,
						  LLInventoryObject::object_list_t* inv,
						  S32, void*) override;

	// Can be passed a LLViewerObject* as "data"
	static void show(void* data = NULL);

	static void dirty();
	static LLUUID getSelectedUUID();

private:
	// Show only via show()
	LLFloaterInspect(const LLSD&);

	void requestInventory(LLViewerObject* vobj);

	static void onClickCreatorProfile(void* data);
	static void onClickOwnerProfile(void* data);
	static void onClickWeights(void* data);
	static void onClickRefresh(void* data);
	static void onClickClose(void* data);
	static void onSelectObject(LLUICtrl*, void* data);

private:
	LLScrollListCtrl*				mObjectList;
	LLButton*						mButtonOwner;
	LLButton*						mButtonCreator;
	LLButton*						mButtonWeights;
	LLIconCtrl*						mIconNavMeshInfo;

	LLSafeHandle<LLObjectSelection>	mObjectSelection;

	 // Map holding LLUUID, <scripts, total>
	typedef fast_hmap<LLUUID, std::pair<S32, S32> > invcounts_map_t;
	invcounts_map_t					mInventoryNums;

	std::string						mNavMeshToolTip;

	bool							mDirty;
};

class HBFloaterInspectAvatar final
:	public LLFloater,
	public LLFloaterSingleton<HBFloaterInspectAvatar>,
	public LLVOInventoryListener
{
	friend class LLUISingleton<HBFloaterInspectAvatar,
							   VisibilityPolicy<LLFloater> >;

public:
	~HBFloaterInspectAvatar() override = default;

	bool postBuild() override;
	void draw() override;
	void refresh() override;

	// LLVOInventoryListener interface
	void inventoryChanged(LLViewerObject* obj,
						  LLInventoryObject::object_list_t* inv,
						  S32, void*) override;

	static void show(const LLUUID& av_id);

private:
	// Show only via show()
	HBFloaterInspectAvatar(const LLSD&);

	void requestInventory(LLViewerObject* vobj);

	static void onClickProfile(void* data);
	static void onClickRefresh(void* data);
	static void onClickClose(void* data);
	static void onDoubleClickObject(void* data);

private:
	LLUUID							mAvatarID;

	LLScrollListCtrl*				mObjectList;

	std::string						mTitle;

	 // Map holding attachments UUIDs and script counts
	typedef fast_hmap<LLUUID, S32> scriptcounts_map_t;
	scriptcounts_map_t				mScriptCounts;

	bool							mDirty;
};

#endif //LL_LLFLOATERINSPECT_H
