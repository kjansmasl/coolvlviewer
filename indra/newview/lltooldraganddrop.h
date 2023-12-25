/**
 * @file lltooldraganddrop.h
 * @brief LLToolDragAndDrop class header file
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

#ifndef LL_TOOLDRAGANDDROP_H
#define LL_TOOLDRAGANDDROP_H

#include "llassetstorage.h"
#include "lldictionary.h"
#include "llpermissions.h"
#include "lltool.h"
#include "lluuid.h"
#include "llview.h"

#include "llviewerinventory.h"
#include "llwindow.h"

class LLToolDragAndDrop;
class LLViewerRegion;
class LLVOAvatar;
class LLPickInfo;

class LLToolDragAndDrop final : public LLTool
{
protected:
	LOG_CLASS(LLToolDragAndDrop);

public:
	LLToolDragAndDrop();

	// Overridden from LLTool
	bool handleMouseUp(S32 x, S32 y, MASK mask) override;
	bool handleHover(S32 x, S32 y, MASK mask) override;

	bool handleKey(KEY key, MASK mask) override;
	bool handleToolTip(S32 x, S32 y, std::string& msg, LLRect* rect) override;

	void onMouseCaptureLost() override;
	void handleDeselect() override;

	LL_INLINE void setDragStart(S32 x, S32 y)	// In screen space
	{
		mDragStartX = x;
		mDragStartY = y;
	}

	bool isOverThreshold(S32 x, S32 y);			// In screen space

	enum ESource
	{
		SOURCE_AGENT,
		SOURCE_WORLD,
		SOURCE_NOTECARD,
		SOURCE_LIBRARY
	};

	void beginDrag(EDragAndDropType type, const LLUUID& cargo_id,
				   ESource src, const LLUUID& src_id = LLUUID::null,
				   const LLUUID& object_id = LLUUID::null);
	void beginMultiDrag(const std::vector<EDragAndDropType> types,
						const std::vector<LLUUID>& cargo_ids,
						ESource src, const LLUUID& src_id = LLUUID::null);
	void endDrag();

	LL_INLINE ESource getSource() const			{ return mSource; }
	LL_INLINE const LLUUID& getSourceID() const	{ return mSourceID; }
	LL_INLINE const LLUUID& getObjectID() const	{ return mObjectID; }
	LL_INLINE EAcceptance getLastAccept()		{ return mLastAccept; }

	LL_INLINE U32 getCargoCount() const			{ return mCargoIDs.size(); }
	LL_INLINE S32 getCargoIndex() const			{ return mCurItemIndex; }

protected:
	enum EDropTarget
	{
		DT_NONE = 0,
		DT_SELF = 1,
		DT_AVATAR = 2,
		DT_OBJECT = 3,
		DT_LAND = 4,
		DT_COUNT = 5
	};

	// dragOrDrop3dImpl points to a member of LLToolDragAndDrop that
	// takes parameters (LLViewerObject* objp, S32 face, MASK, bool
	// drop) and returns a bool if drop is ok
	typedef EAcceptance (LLToolDragAndDrop::*dragOrDrop3dImpl)(LLViewerObject*,
															   S32, MASK,
															   bool);

	void dragOrDrop(S32 x, S32 y, MASK mask, bool drop,
					EAcceptance* acceptance);
	void dragOrDrop3D(S32 x, S32 y, MASK mask, bool drop,
					  EAcceptance* acceptance);
	static void pickCallback(const LLPickInfo& pick_info);

protected:
	// 3d drop functions. these call down into the static functions named
	// drop<ThingToDrop> if drop is true and permissions allow that behavior.
	EAcceptance dad3dNULL(LLViewerObject*, S32, MASK, bool);
	EAcceptance dad3dRezObjectOnLand(LLViewerObject* objp, S32 face,
									 MASK mask, bool drop);
	EAcceptance dad3dRezObjectOnObject(LLViewerObject* objp, S32 face,
									   MASK mask, bool drop);
	EAcceptance dad3dRezCategoryOnObject(LLViewerObject* objp, S32 face,
										 MASK mask, bool drop);
	EAcceptance dad3dRezScript(LLViewerObject* objp, S32 face,
							   MASK mask, bool drop);
	EAcceptance dad3dTextureObject(LLViewerObject* objp, S32 face,
								   MASK mask, bool drop);
	EAcceptance dad3dMaterialObject(LLViewerObject* objp, S32 face,
									MASK mask, bool drop);
#if LL_MESH_ASSET_SUPPORT
	EAcceptance dad3dMeshObject(LLViewerObject* objp, S32 face,
								MASK mask, bool drop);
#endif
	EAcceptance dad3dWearItem(LLViewerObject* obj, S32 face,
							  MASK mask, bool drop);
	EAcceptance dad3dWearCategory(LLViewerObject* objp, S32 face,
								  MASK mask, bool drop);
	EAcceptance dad3dUpdateInventory(LLViewerObject* objp, S32 face,
									 MASK mask, bool drop);
	EAcceptance dad3dUpdateInventoryCategory(LLViewerObject* objp, S32 face,
											 MASK mask, bool drop);
	EAcceptance dad3dGiveInventoryObject(LLViewerObject* objp, S32 face,
								   MASK mask, bool drop);
	EAcceptance dad3dGiveInventory(LLViewerObject* objp, S32 face,
								   MASK mask, bool drop);
	EAcceptance dad3dGiveInventoryCategory(LLViewerObject* objp, S32 face,
										   MASK mask, bool drop);
	EAcceptance dad3dRezFromObjectOnLand(LLViewerObject* objp, S32 face,
										 MASK mask, bool drop);
	EAcceptance dad3dRezFromObjectOnObject(LLViewerObject* objp, S32 face,
										   MASK mask, bool drop);
	EAcceptance dad3dRezAttachmentFromInv(LLViewerObject* objp, S32 face,
										  MASK mask, bool drop);
	EAcceptance dad3dCategoryOnLand(LLViewerObject* objp, S32 face,
									MASK mask, bool drop);
	EAcceptance dad3dAssetOnLand(LLViewerObject* objp, S32 face,
								 MASK mask, bool drop);
	EAcceptance dad3dActivateGesture(LLViewerObject* objp, S32 face,
									 MASK mask, bool drop);

	// Helper called by methods above to handle "application" of an item to an
	// object (texture applied to face, mesh applied to shape, etc.)
	EAcceptance dad3dApplyToObject(LLViewerObject* objp, S32 face, MASK mask,
								   bool drop, EDragAndDropType cargo_type);

	// Sets the LLToolDragAndDrop's cursor based on the given acceptance
	ECursorType acceptanceToCursor(EAcceptance acceptance);

	// This method converts mCargoID to an inventory item or folder. If no item
	// or category is found, both pointers will be returned NULL.
	LLInventoryObject* locateInventory(LLViewerInventoryItem*& itemp,
									   LLViewerInventoryCategory*& catp);

	void dropObject(LLViewerObject* raycast_target, bool bypass_sim_raycast,
					bool from_task_inventory, bool remove_from_inventory);

	// Accessor that looks at permissions, copyability, and names of inventory
	// items to determine if a drop would be ok.
	static EAcceptance willObjectAcceptInventory(LLViewerObject* objp,
												 LLInventoryItem* itemp,
												 EDragAndDropType type =
													DAD_NONE);

	// Gives inventory item functionality
	static bool handleCopyProtectedItem(const LLSD& notification,
										const LLSD& response);
	static void commitGiveInventoryItem(const LLUUID& to_agent,
										LLInventoryItem* itemp,
										const LLUUID& im_session_id =
											LLUUID::null);

	// Gives inventory category functionality
	static bool handleCopyProtectedCategory(const LLSD& notification,
											const LLSD& response);
	static void commitGiveInventoryCategory(const LLUUID& to_agent,
											LLInventoryCategory* catp,
											const LLUUID& im_session_id =
												LLUUID::null);

public:
	// Deals with permissions of object, etc. returns true if drop can
	// proceed, otherwise false.
	static bool handleDropAssetProtections(LLViewerObject* objp,
										   LLInventoryItem* itemp,
										   ESource source,
										   const LLUUID& src_id =
												LLUUID::null);

	// Helper method
	LL_INLINE static bool isInventoryDropAcceptable(LLViewerObject* objp,
													LLInventoryItem* itemp)
	{
		return willObjectAcceptInventory(objp, itemp) >=
				ACCEPT_YES_COPY_SINGLE;
	}

	// This simple helper function assumes you are attempting to
	// transfer item. returns true if you can give, otherwise false.
	static bool isInventoryGiveAcceptable(LLInventoryItem* itemp);
	static bool isInventoryGroupGiveAcceptable(LLInventoryItem* itemp);

	bool dadUpdateInventory(LLViewerObject* objp, bool drop);
	bool dadUpdateInventoryCategory(LLViewerObject* objp, bool drop);

	// Methods that act on the simulator state.
	static void dropTextureOneFace(LLViewerObject* hit_objp, S32 hit_face,
								   LLInventoryItem* itemp, ESource source,
								   const LLUUID& src_id = LLUUID::null);
	static void dropTextureAllFaces(LLViewerObject* hit_objp,
									LLInventoryItem* itemp, ESource source,
									const LLUUID& src_id = LLUUID::null);
	static void dropMaterialOneFace(LLViewerObject* hit_objp, S32 hit_face,
									LLInventoryItem* itemp, ESource source,
									const LLUUID& src_id = LLUUID::null);
	static void dropMaterialAllFaces(LLViewerObject* hit_objp,
									 LLInventoryItem* itemp, ESource source,
									 const LLUUID& src_id = LLUUID::null);
	static void dropScript(LLViewerObject* hit_objp, LLInventoryItem* itemp,
						   bool active, ESource source, const LLUUID& src_id);
#if LL_MESH_ASSET_SUPPORT
	static void dropMesh(LLViewerObject* hit_objp, LLInventoryItem* itemp,
						 ESource source, const LLUUID& src_id);
#endif
	static void dropInventory(LLViewerObject* hit_objp, LLInventoryItem* itemp,
							  ESource source,
							  const LLUUID& src_id = LLUUID::null);

	static void giveInventory(const LLUUID& to_agent, LLInventoryItem* itemp,
							  const LLUUID& session_id = LLUUID::null);
	static void giveInventoryCategory(const LLUUID& to_agent,
									  LLInventoryCategory* catp,
									  const LLUUID& session_id = LLUUID::null);

	static bool handleGiveDragAndDrop(const LLUUID& agent, const LLUUID& session,
									  bool drop, EDragAndDropType cargo_type,
									  void* cargo_data, EAcceptance* acceptp);

	// Classes used for determining 3d drag and drop types.
private:
	struct DragAndDropEntry : public LLDictionaryEntry
	{
		DragAndDropEntry(dragOrDrop3dImpl f_none,
						 dragOrDrop3dImpl f_self,
						 dragOrDrop3dImpl f_avatar,
						 dragOrDrop3dImpl f_object,
						 dragOrDrop3dImpl f_land);
		dragOrDrop3dImpl mFunctions[DT_COUNT];
	};
	class LLDragAndDropDictionary : public LLSingleton<LLDragAndDropDictionary>,
									public LLDictionary<EDragAndDropType, DragAndDropEntry>
	{
	public:
		LLDragAndDropDictionary();
		dragOrDrop3dImpl get(EDragAndDropType dad_type,
							 EDropTarget drop_target);
	};

protected:
	LLUUID							mSourceID;
	LLUUID							mObjectID;

	std::vector<LLUUID>				mCargoIDs;
	std::vector<EDragAndDropType>	mCargoTypes;

	LLVector3d						mLastCameraPos;
	LLVector3d						mLastHitPos;

	std::string						mToolTipMsg;

	S32								mCurItemIndex;

	S32								mDragStartX;
	S32								mDragStartY;

	ESource							mSource;
	ECursorType						mCursor;
	EAcceptance						mLastAccept;

	bool							mDrop;
};

// Utility function
void pack_permissions_slam(LLMessageSystem* msg, U32 flags,
						   const LLPermissions& perms);

extern LLToolDragAndDrop gToolDragAndDrop;

#endif  // LL_TOOLDRAGANDDROP_H
