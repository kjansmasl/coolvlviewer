/**
 * @file llinventorybridge.h
 * @brief Implementation of the Inventory-Folder-View-Bridge classes.
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

#ifndef LL_LLINVENTORYBRIDGE_H
#define LL_LLINVENTORYBRIDGE_H

#include <vector>

#include "llavatartracker.h"
#include "llfolderview.h"
#include "llinventorymodel.h"
#include "llviewerwearable.h"

class LLInventoryModel;
class LLLandmark;

// The "Restore to Last Position" feature is no more supported in SL and was
// never implemented in OpenSim... In SL, it also relies on having rez rights
// at position 0,0 in the sim, which is rarely the case in mainland regions...
#define LL_RESTORE_TO_WORLD 0

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Class LLInvFVBridge (& its derived classes)
//
// Short for Inventory-Folder-View-Bridge. This is an
// implementation class to be able to view inventory items.
//
// You'll want to call LLInvItemFVELister::createBridge() to actually create
// an instance of this class. This helps encapsulate the
// funcationality a bit. (except for folders, you can create those
// manually...)
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
class LLInvFVBridge : public LLFolderViewEventListener
{
protected:
	LOG_CLASS(LLInvFVBridge);

public:
	// This method is a convenience function which creates the correct
	// type of bridge based on some basic information
	static LLInvFVBridge* createBridge(LLAssetType::EType asset_type,
									   LLAssetType::EType actual_asset_type,
									   LLInventoryType::EType inv_type,
									   LLInventoryPanel* inventory,
									   const LLUUID& uuid,
									   U32 flags = 0x00);
	~LLInvFVBridge() override = default;

	LL_INLINE const LLUUID& getUUID() const override		{ return mUUID; }

	LL_INLINE virtual const LLUUID& getThumbnailUUID() const
	{
		return LLUUID::null;
	}

	LL_INLINE virtual const std::string& getPrefix()		{ return LLStringUtil::null; }
	LL_INLINE virtual void restoreItem()					{}
#if LL_RESTORE_TO_WORLD
	LL_INLINE virtual void restoreToWorld()					{}
#endif

	// LLFolderViewEventListener methods

	const std::string& getName() const override;

	LL_INLINE const std::string& getDisplayName() const override
	{
		return getName();
	}

	LL_INLINE PermissionMask getPermissionMask() const override
	{
		return PERM_ALL;	// Folders have full perms
	}

	LL_INLINE LLFolderType::EType getPreferredType() const override
	{
		return LLFolderType::FT_NONE;
	}

	// NOTE: folders do not have a creation date
	LL_INLINE time_t getCreationDate() const override		{ return 0; }

	LL_INLINE LLFontGL::StyleFlags getLabelStyle() const override
	{
		return LLFontGL::NORMAL;
	}

	LL_INLINE std::string getLabelSuffix() const override	{ return LLStringUtil::null; }
	LL_INLINE void openItem() override						{}
	LL_INLINE void previewItem() override					{ openItem(); }
	void showProperties() override;
	LL_INLINE virtual bool isMultiPreviewAllowed() const	{ return true; }
	LL_INLINE bool isItemRenameable() const override		{ return true; }
	bool isItemRemovable() override;
	bool isItemMovable() override;
	void removeBatch(std::vector<LLFolderViewEventListener*>& batch) override;

	LL_INLINE void move(LLFolderViewEventListener*) override
	{
	}

	LL_INLINE bool isItemCopyable() const override			{ return false; }
	LL_INLINE bool copyToClipboard() const override			{ return false; }
	bool cutToClipboard() const override;
	bool isClipboardPasteable() const override;
	virtual bool isClipboardPasteableAsLink() const;
	LL_INLINE void pasteFromClipboard() override			{}
	LL_INLINE void pasteLinkFromClipboard() override		{}
	void getClipboardEntries(bool show_asset_id,
							 std::vector<std::string>& items,
							 std::vector<std::string>& disabled_items,
							 U32 flags);
	void buildContextMenu(LLMenuGL& menu, U32 flags) override;

	bool startDrag(EDragAndDropType* type, LLUUID* id) const override;

	LL_INLINE bool dragOrDrop(MASK, bool, EDragAndDropType, void*,
							  std::string&) override
	{
		return false;
	}

	LL_INLINE LLInventoryType::EType getInventoryType() const override
	{
		return mInvType;
	}

	LL_INLINE S32 getSubType() const override
	{
		return mSubType;
	}

	// LLInvFVBridge functionality
	LL_INLINE virtual void clearDisplayName()				{}

//MK
////protected:
//mk
	LLInvFVBridge(LLInventoryPanel* inv_panelp, const LLUUID& uuid)
	:	mInventoryPanel(inv_panelp),
		mUUID(uuid),
		mInvType(LLInventoryType::IT_NONE)
	{
	}

	LLInventoryObject* getInventoryObject() const;

	bool isInTrash() const;
	bool isInLostAndFound() const;
	bool isInCOF() const;
	bool isInMarketplace() const;

	// Is this obj or its baseobj in the trash ?
	bool isLinkedObjectInTrash() const;

	// Is this a linked obj whose baseobj is not in inventory ?
	bool isLinkedObjectMissing() const;

	// Returns true if the item is in agent inventory. If false, it must be
	// lost or in the inventory library.
	bool isAgentInventory() const;

	LL_INLINE virtual bool isItemPermissive() const			{ return false; }
	static void changeItemParent(LLInventoryModel* model,
								 LLViewerInventoryItem* item,
								 const LLUUID& new_parent,
								 bool restamp);
	static void changeCategoryParent(LLInventoryModel* model,
									 LLViewerInventoryCategory* item,
									 const LLUUID& new_parent,
									 bool restamp);
	void removeBatchNoCheck(std::vector<LLFolderViewEventListener*>& batch);

	void purgeItem(LLInventoryModel* model, const LLUUID& uuid);

protected:
	LLInventoryPanel*		mInventoryPanel;
	LLUUID					mUUID;	// item id
	LLInventoryType::EType	mInvType;
	S32						mSubType;
};

class LLItemBridge : public LLInvFVBridge
{
protected:
	LOG_CLASS(LLItemBridge);

public:
	LLItemBridge(LLInventoryPanel* inventory, const LLUUID& uuid)
	:	LLInvFVBridge(inventory, uuid)
	{
	}

	void performAction(LLFolderView* folderp, LLInventoryModel* modelp,
					   const std::string& action) override;

	void selectItem() override;
	void restoreItem() override;
#if LL_RESTORE_TO_WORLD
	void restoreToWorld() override;
#endif
	virtual void gotoItem(LLFolderView* folderp);

	LLUIImagePtr getIcon() const override;
	const std::string& getDisplayName() const override;
	std::string getLabelSuffix() const override;
	PermissionMask getPermissionMask() const override;
	time_t getCreationDate() const override;
	bool isItemRenameable() const override;
	bool renameItem(const std::string& new_name) override;
	bool removeItem() override;
	bool isItemCopyable() const override;
	bool copyToClipboard() const override;
	LL_INLINE bool isUpToDate() const override			{ return true; }
	LL_INLINE bool hasChildren() const override			{ return false; }

	// Override for LLInvFVBridge
	LL_INLINE void clearDisplayName() override			{ mDisplayName.clear(); }

	LLViewerInventoryItem* getItem() const;

	const LLUUID& getThumbnailUUID() const override;

protected:
	bool isItemPermissive() const override;
	static void buildDisplayName(LLInventoryItem* item, std::string& name);
	mutable std::string mDisplayName;
};

class LLFolderBridge : public LLInvFVBridge
{
	friend class LLInvFVBridge;

protected:
	LOG_CLASS(LLFolderBridge);

public:
	~LLFolderBridge() override;

	bool dragItemIntoFolder(LLInventoryItem* inv_item,
							bool drop, std::string& tooltip_msg);
	bool dragCategoryIntoFolder(LLInventoryCategory* inv_category,
								bool drop, std::string& tooltip_msg);
	void performAction(LLFolderView* folder, LLInventoryModel* model,
					   const std::string& action) override;
	void openItem() override;
	bool isItemRenameable() const override;
	LL_INLINE void selectItem() override					{}
	void restoreItem() override;

	LLFolderType::EType getPreferredType() const override;
	LLUIImagePtr getIcon() const override;
	std::string getLabelSuffix() const override;
	LLFontGL::StyleFlags getLabelStyle() const override;
	const LLUUID& getThumbnailUUID() const override;

	bool renameItem(const std::string& new_name) override;
	bool removeItem() override;
	void pasteFromClipboard() override;
	void pasteLinkFromClipboard() override;
	bool isClipboardPasteableAsLink() const override;
	void buildContextMenu(LLMenuGL& menu, U32 flags) override;
	bool hasChildren() const override;
	bool dragOrDrop(MASK mask, bool drop, EDragAndDropType cargo_type,
					void* cargo_data, std::string& tooltip_msg) override;

	bool isItemRemovable() override;
	bool isItemMovable() override;
	bool isUpToDate() const override;
	bool isItemCopyable() const override;
	bool copyToClipboard() const override;

	LLViewerInventoryCategory* getCategory() const;

protected:
	LLFolderBridge(LLInventoryPanel* inventory, const LLUUID& uuid)
	:	LLInvFVBridge(inventory, uuid),
		mCallingCards(false),
		mWearables(false)
	{
	}

	bool checkFolderForContentsOfType(LLInventoryModel* model,
									  LLInventoryCollectFunctor& typeToCheck);

	void modifyOutfit(bool append, bool replace);

public:
	static LLFolderBridge* sSelf;
	static void staticFolderOptionsMenu();
	void folderOptionsMenu(U32 flags = FIRST_SELECTED_ITEM);

private:
	bool						mCallingCards;
	bool						mWearables;
	LLMenuGL*					mMenu;
	std::vector<std::string>	mItems;
	std::vector<std::string>	mDisabledItems;
};

// DEPRECATED
class LLScriptBridge final : public LLItemBridge
{
	friend class LLInvFVBridge;

protected:
	LOG_CLASS(LLScriptBridge);

public:
	LLUIImagePtr getIcon() const;

protected:
	LLScriptBridge(LLInventoryPanel* inventory, const LLUUID& uuid)
	:	LLItemBridge(inventory, uuid)
	{
	}
};

class LLTextureBridge final : public LLItemBridge
{
	friend class LLInvFVBridge;

protected:
	LOG_CLASS(LLTextureBridge);

public:
	const std::string& getPrefix() override;

	LLUIImagePtr getIcon() const override;
	void openItem() override;

protected:
	LL_INLINE LLTextureBridge(LLInventoryPanel* inventory, const LLUUID& uuid,
							  LLInventoryType::EType type)
	:	LLItemBridge(inventory, uuid), mInvType(type)
	{
	}

protected:
	LLInventoryType::EType mInvType;
};

class LLSoundBridge final : public LLItemBridge
{
	friend class LLInvFVBridge;

protected:
	LOG_CLASS(LLSoundBridge);

public:
	const std::string& getPrefix() override;

	LLUIImagePtr getIcon() const override;
	void openItem() override;
	void previewItem() override;
	void performAction(LLFolderView* folder, LLInventoryModel* model,
					   const std::string& action) override;
	void buildContextMenu(LLMenuGL& menu, U32 flags) override;

protected:
	LLSoundBridge(LLInventoryPanel* inventory, const LLUUID& uuid)
	:	LLItemBridge(inventory, uuid)
	{
	}
};

class LLLandmarkBridge final : public LLItemBridge
{
	friend class LLInvFVBridge;

protected:
	LOG_CLASS(LLLandmarkBridge);

public:
	const std::string& getPrefix() override;

	void performAction(LLFolderView* folder, LLInventoryModel* model,
					   const std::string& action) override;
	void buildContextMenu(LLMenuGL& menu, U32 flags) override;
	LLUIImagePtr getIcon() const override;
	void openItem() override;
	void showOnMap(LLLandmark* landmark);

protected:
	LLLandmarkBridge(LLInventoryPanel* inventory,
					 const LLUUID& uuid, U32 flags = 0x00)
	:	LLItemBridge(inventory, uuid)
	{
		mVisited = (flags & LLInventoryItem::II_FLAGS_LANDMARK_VISITED) != 0;
	}

protected:
	bool mVisited;
};

class LLCallingCardBridge;

class LLCallingCardObserver final : public LLFriendObserver
{
public:
	LLCallingCardObserver(LLCallingCardBridge* bridge)
	:	mBridgep(bridge)
	{
	}

	~LLCallingCardObserver() override
	{
		mBridgep = NULL;
	}

	void changed(U32 mask) override;

protected:
	LLCallingCardBridge* mBridgep;
};

class LLCallingCardBridge final : public LLItemBridge
{
	friend class LLInvFVBridge;

protected:
	LOG_CLASS(LLCallingCardBridge);

public:
	const std::string& getPrefix() override;

	std::string getLabelSuffix() const override;
	//const std::string& getDisplayName() const override;
	LLUIImagePtr getIcon() const override;
	void performAction(LLFolderView* folder, LLInventoryModel* model,
					   const std::string& action) override;
	void openItem() override;
	void buildContextMenu(LLMenuGL& menu, U32 flags) override;
	//void renameItem(const std::string& new_name) override;
	//bool removeItem() override;
	bool dragOrDrop(MASK mask, bool drop, EDragAndDropType cargo_type,
					void* cargo_data, std::string& tooltip_msg) override;
	void refreshFolderViewItem();

protected:
	LLCallingCardBridge(LLInventoryPanel* inventory, const LLUUID& uuid);
	~LLCallingCardBridge() override;

protected:
	LLCallingCardObserver* mObserver;
};

class LLNotecardBridge final : public LLItemBridge
{
	friend class LLInvFVBridge;

protected:
	LOG_CLASS(LLNotecardBridge);

public:
	const std::string& getPrefix() override;

	LLUIImagePtr getIcon() const override;
	void openItem() override;

protected:
	LLNotecardBridge(LLInventoryPanel* inventory, const LLUUID& uuid)
	:	LLItemBridge(inventory, uuid)
	{
	}
};

class LLGestureBridge final : public LLItemBridge
{
	friend class LLInvFVBridge;

protected:
	LOG_CLASS(LLGestureBridge);

public:
	const std::string& getPrefix() override;

	LLUIImagePtr getIcon() const override;

	// Only suffix for gesture items, not task items, because only
	// gestures in your inventory can be active.
	LLFontGL::StyleFlags getLabelStyle() const override;
	std::string getLabelSuffix() const override;

	void performAction(LLFolderView* folder, LLInventoryModel* model,
					   const std::string& action) override;
	void openItem() override;
	bool removeItem() override;

	void buildContextMenu(LLMenuGL& menu, U32 flags) override;

protected:
	LLGestureBridge(LLInventoryPanel* inventory, const LLUUID& uuid)
	:	LLItemBridge(inventory, uuid)
	{
	}
};

class LLAnimationBridge final : public LLItemBridge
{
	friend class LLInvFVBridge;

protected:
	LOG_CLASS(LLAnimationBridge);

public:
	const std::string& getPrefix() override;

	void performAction(LLFolderView* folder, LLInventoryModel* model,
					   const std::string& action) override;
	void buildContextMenu(LLMenuGL& menu, U32 flags) override;

	LLUIImagePtr getIcon() const override;
	void openItem() override;

protected:
	LLAnimationBridge(LLInventoryPanel* inventory, const LLUUID& uuid)
	:	LLItemBridge(inventory, uuid)
	{
	}
};

class LLObjectBridge final : public LLItemBridge
{
	friend class LLInvFVBridge;

protected:
	LOG_CLASS(LLObjectBridge);

public:
	const std::string& getPrefix() override;

	LLUIImagePtr getIcon() const override;
	void performAction(LLFolderView* folder, LLInventoryModel* model,
					   const std::string& action) override;
	void openItem() override;
	LLFontGL::StyleFlags getLabelStyle() const override;
	std::string getLabelSuffix() const override;
	void buildContextMenu(LLMenuGL& menu, U32 flags) override;
	bool isItemRemovable() override;
	bool renameItem(const std::string& new_name) override;

protected:
	LLObjectBridge(LLInventoryPanel* inventory, const LLUUID& uuid,
				   LLInventoryType::EType type, U32 flags)
	:	LLItemBridge(inventory, uuid), mInvType(type)
	{
		mAttachPt = (flags & 0xff); // low bye of inventory flags

		mIsMultiObject = (flags & LLInventoryItem::II_FLAGS_OBJECT_HAS_MULTIPLE_ITEMS) != 0;
	}

protected:
	U32						mAttachPt;
	LLInventoryType::EType	mInvType;
	bool					mIsMultiObject;
};

class LLLSLTextBridge final : public LLItemBridge
{
	friend class LLInvFVBridge;

protected:
	LOG_CLASS(LLLSLTextBridge);

public:
	const std::string& getPrefix() override;

	LLUIImagePtr getIcon() const override;
	void openItem() override;

protected:
	LLLSLTextBridge(LLInventoryPanel* inventory, const LLUUID& uuid)
	:	LLItemBridge(inventory, uuid)
	{
	}
};

class LLWearableBridge final : public LLItemBridge
{
	friend class LLInvFVBridge;

protected:
	LOG_CLASS(LLWearableBridge);

public:
	LLUIImagePtr getIcon() const override;
	void performAction(LLFolderView* folder, LLInventoryModel* model,
					   const std::string& action) override;
	void openItem() override;
	void buildContextMenu(LLMenuGL& menu, U32 flags) override;
	LLFontGL::StyleFlags getLabelStyle() const override;
	std::string getLabelSuffix() const override;
	bool isItemRemovable() override;
	bool renameItem(const std::string& new_name) override;

	// Access to wearOnAvatar() from menu:
	static void onWearOnAvatar(void* userdata);
	static bool canWearOnAvatar(void* userdata);
	static void onWearOnAvatarArrived(LLViewerWearable* wearable,
									  void* userdata);
	void wearOnAvatar(bool replace = true);

	// Access to editOnAvatar() from menu:
	static bool canEditOnAvatar(void* userdata);
	static void onEditOnAvatar(void* userdata);
	void editOnAvatar();

	static bool canRemoveFromAvatar(void* userdata);
	static void onRemoveFromAvatar(void* userdata);
	static void onRemoveFromAvatarArrived(LLViewerWearable* wearable,
										  void* userdata);

protected:
	LLWearableBridge(LLInventoryPanel* inventory,
					 const LLUUID& uuid,
					 LLAssetType::EType asset_type,
					 LLInventoryType::EType inv_type,
					 LLWearableType::EType  wearable_type)
	:	LLItemBridge(inventory, uuid),
		mAssetType(asset_type),
		mInvType(inv_type),
		mWearableType(wearable_type)
	{
	}

protected:
	LLAssetType::EType		mAssetType;
	LLInventoryType::EType	mInvType;
	LLWearableType::EType	mWearableType;
};

class LLLinkItemBridge final : public LLItemBridge
{
	friend class LLInvFVBridge;

protected:
	LOG_CLASS(LLLinkItemBridge);

public:
	const std::string& getPrefix() override;

	LLUIImagePtr getIcon() const override;
	void buildContextMenu(LLMenuGL& menu, U32 flags) override;

protected:
	LLLinkItemBridge(LLInventoryPanel* inventory, const LLUUID& uuid)
	:	LLItemBridge(inventory, uuid)
	{
	}
};

class LLLinkFolderBridge final : public LLItemBridge
{
	friend class LLInvFVBridge;

protected:
	LOG_CLASS(LLLinkFolderBridge);

public:
	const std::string& getPrefix() override;
	LLUIImagePtr getIcon() const override;
	void buildContextMenu(LLMenuGL& menu, U32 flags) override;
	void performAction(LLFolderView* folder, LLInventoryModel* model,
						   const std::string& action) override;
	void gotoItem(LLFolderView* folder) override;

protected:
	LLLinkFolderBridge(LLInventoryPanel* inventory, const LLUUID& uuid)
	:	LLItemBridge(inventory, uuid)
	{
	}

	const LLUUID& getFolderID() const;
};

#if LL_MESH_ASSET_SUPPORT
class LLMeshBridge final : public LLItemBridge
{
	friend class LLInvFVBridge;

protected:
	LOG_CLASS(LLMeshBridge);

public:
	const std::string& getPrefix() override;
	LLUIImagePtr getIcon() const override;

	void openItem() override;
	void previewItem() override;
	void buildContextMenu(LLMenuGL& menu, U32 flags) override;
	void performAction(LLFolderView* folder, LLInventoryModel* model,
					   const std::string& action) override;

protected:
	LLMeshBridge(LLInventoryPanel* inventory, const LLUUID& uuid)
	:	LLItemBridge(inventory, uuid)
	{
	}
};
#endif

class LLSettingsBridge final : public LLItemBridge
{
	friend class LLInvFVBridge;

protected:
	LOG_CLASS(LLSettingsBridge);

public:
	const std::string& getPrefix() override;
	LLUIImagePtr getIcon() const override;
	LLFontGL::StyleFlags getLabelStyle() const override;

	bool isMultiPreviewAllowed() const override			{ return false; }
	void openItem() override;
	void previewItem() override;
	void buildContextMenu(LLMenuGL& menu, U32 flags) override;
	void performAction(LLFolderView* folder, LLInventoryModel* model,
					   const std::string& action) override;

protected:
	LLSettingsBridge(LLInventoryPanel* inventory, const LLUUID& uuid, U32 type)
	:	LLItemBridge(inventory, uuid),
		mSettingsType(type)
	{
	}

protected:
	U32 mSettingsType;
};

class LLMaterialBridge final : public LLItemBridge
{
	friend class LLInvFVBridge;

protected:
	LOG_CLASS(LLMaterialBridge);

public:
	const std::string& getPrefix() override;
	LLUIImagePtr getIcon() const override;

	bool isMultiPreviewAllowed() const override			{ return false; }
	void openItem() override;
	void previewItem() override;
	void buildContextMenu(LLMenuGL& menu, U32 flags) override;
	void performAction(LLFolderView* folder, LLInventoryModel* model,
					   const std::string& action) override;

protected:
	LLMaterialBridge(LLInventoryPanel* inventory, const LLUUID& uuid)
	:	LLItemBridge(inventory, uuid)
	{
	}
};

class LLFindWearables final : public LLInventoryCollectFunctor
{
public:
	LLFindWearables() = default;
	bool operator()(LLInventoryCategory* cat,
					LLInventoryItem* item) override;
};

// Moves items from an in-world object's "Contents" folder to a specified
// folder in agent inventory.
// Also used in llfloateropenobject.cpp.
bool move_inv_category_world_to_agent(const LLUUID& object_id,
									  const LLUUID& category_id,
									  bool drop,
									  void (*callback)(S32, void*) = NULL,
									  void* user_data = NULL);

// Sets menu entries state according to on entries_to_show and disabled_entries
// Also used in llpanelinventory.cpp.
void set_menu_entries_state(LLMenuGL& menu,
							const std::vector<std::string>& entries_to_show,
							const std::vector<std::string>& disabled_entries);

#endif // LL_LLINVENTORYBRIDGE_H
