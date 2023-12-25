/**
 * @file lltooldraganddrop.cpp
 * @brief LLToolDragAndDrop class implementation
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

#include "lltooldraganddrop.h"

#include "indra_constants.h"				// For BLANK_MATERIAL_ASSET_ID
#include "llinstantmessage.h"
#include "llnotifications.h"
#include "llparcel.h"
#include "lltrans.h"
#include "llvolume.h"
#include "llmessage.h"
#include "object_flags.h"

#include "llagent.h"
#include "llagentwearables.h"
#include "llappearancemgr.h"
#include "llfirstuse.h"
#include "llfloatertools.h"
#include "llgesturemgr.h"
#include "llhudeffectspiral.h"
#include "llimmgr.h"
#include "llinventorybridge.h"
#include "llmaterialmgr.h"
#include "llmutelist.h"
#include "llpanelface.h"
#include "llpreviewnotecard.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llselectmgr.h"
#include "lltoolmgr.h"
#include "llviewercontrol.h"
#include "llviewerinventory.h"
#include "llviewerobjectlist.h"
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"
#include "llviewertexturelist.h"
#include "llviewerstats.h"
#include "llviewerwindow.h"
#include "llvoavatarself.h"
#include "llworld.h"

// MAX ITEMS is based on (sizeof(uuid) + 2) * count must be < MTUBYTES or
// 18 * count < 1200 => count < 1200 / 18 => 66. I've cut it down a bit from
// there to give some pad.
constexpr S32 MAX_ITEMS = 42;

// LLVM/clang does not like our sugar...
#if LL_CLANG
# pragma clang diagnostic ignored "-Wunused-value"
#endif
// Syntactic sugar
#define callMemberFunction(object,ptrToMember) ((object).*(ptrToMember))

LLToolDragAndDrop gToolDragAndDrop;

class LLNoPreferredType final : public LLInventoryCollectFunctor
{
public:
	LLNoPreferredType() =  default;

	bool operator()(LLInventoryCategory* catp, LLInventoryItem*) override
	{
		return catp && catp->getPreferredType() == LLFolderType::FT_NONE;
	}
};

class LLNoPreferredTypeOrItem final : public LLInventoryCollectFunctor
{
public:
	LLNoPreferredTypeOrItem() = default;

	bool operator()(LLInventoryCategory* catp, LLInventoryItem* itemp) override
	{
		return itemp ||
			   (catp && catp->getPreferredType() == LLFolderType::FT_NONE);
	}
};

class LLDroppableItem final : public LLInventoryCollectFunctor
{
public:
	LLDroppableItem(bool is_transfer)
	:	mCountLosing(0),
		mIsTransfer(is_transfer)
	{
	}

	bool operator()(LLInventoryCategory*, LLInventoryItem* itemp) override
	{
		if (!itemp || !itemTransferCommonlyAllowed(itemp))
		{
			return false;
		}
		if (mIsTransfer &&
			!itemp->getPermissions().allowTransferBy(gAgentID))
		{
			return false;
		}
		if (!itemp->getPermissions().allowCopyBy(gAgentID))
		{
			++mCountLosing;
		}
		return true;
	}

	LL_INLINE S32 countNoCopy() const			{ return mCountLosing; }

protected:
	S32		mCountLosing;
	bool	mIsTransfer;
};

class LLUncopyableItems final : public LLInventoryCollectFunctor
{
public:
	LLUncopyableItems() = default;

	bool operator()(LLInventoryCategory*, LLInventoryItem* itemp) override
	{
		return itemp && itemTransferCommonlyAllowed(itemp) &&
			   !itemp->getPermissions().allowCopyBy(gAgentID);
	}
};

class LLDropCopyableItems final : public LLInventoryCollectFunctor
{
public:
	LLDropCopyableItems() = default;

	bool operator()(LLInventoryCategory*, LLInventoryItem* itemp) override
	{
		return itemp && itemTransferCommonlyAllowed(itemp) &&
			   itemp->getPermissions().allowCopyBy(gAgentID);
	}
};

class LLGiveable final : public LLInventoryCollectFunctor
{
public:
	LLGiveable()
	:	mCountLosing(0)
	{
	}

	bool operator()(LLInventoryCategory* catp, LLInventoryItem* itemp) override
	{
		// All categories can be given.
		if (catp)
		{
			return true;
		}
		if (!itemp || !itemTransferCommonlyAllowed(itemp))
		{
			return false;
		}
		if (!itemp->getPermissions().allowTransferBy(gAgentID))
		{
			return false;
		}
		if (!itemp->getPermissions().allowCopyBy(gAgentID))
		{
			++mCountLosing;
		}
		return true;
	}

	LL_INLINE S32 countNoCopy() const			{ return mCountLosing; }

protected:
	S32 mCountLosing;
};

// Starts a fetch on folders and items. This is really not used as an observer
// in the traditional sense; we are just using it to request a fetch and we do
// not care about when/if the response arrives.
class LLCategoryFireAndForget final : public LLInventoryFetchComboObserver
{
protected:
	LOG_CLASS(LLCategoryFireAndForget);

public:
	LLCategoryFireAndForget() = default;

	void done() override
	{
		// No-op: it is fire and forget, right ?
		LL_DEBUGS("DragAndDrop") << "Done." << LL_ENDL;
	}
};

class LLCategoryDropObserver final : public LLInventoryFetchObserver
{
public:
	LLCategoryDropObserver(const LLUUID& obj_id,
						   LLToolDragAndDrop::ESource src)
	:	mObjectID(obj_id),
		mSource(src)
	{
	}

	void done() override
	{
		gInventory.removeObserver(this);
		LLViewerObject* dst_objp = gObjectList.findObject(mObjectID);
		if (dst_objp)
		{
			// *FIX: coalesce these...
  			for (U32 i = 0, count =  mComplete.size(); i < count; ++i)
	  		{
 				LLInventoryItem* itemp = gInventory.getItem(mComplete[i]);
 				if (itemp)
 				{
 					LLToolDragAndDrop::dropInventory(dst_objp, itemp, mSource);
 				}
  			}
		}
		delete this;
	}

protected:
	LLUUID						mObjectID;
	LLToolDragAndDrop::ESource	mSource;
};

LLToolDragAndDrop::DragAndDropEntry::DragAndDropEntry(dragOrDrop3dImpl f_none,
													  dragOrDrop3dImpl f_self,
													  dragOrDrop3dImpl f_avatar,
													  dragOrDrop3dImpl f_object,
													  dragOrDrop3dImpl f_land) :
	LLDictionaryEntry("")
{
	mFunctions[DT_NONE] = f_none;
	mFunctions[DT_SELF] = f_self;
	mFunctions[DT_AVATAR] = f_avatar;
	mFunctions[DT_OBJECT] = f_object;
	mFunctions[DT_LAND] = f_land;
}

LLToolDragAndDrop::dragOrDrop3dImpl
LLToolDragAndDrop::LLDragAndDropDictionary::get(EDragAndDropType dad_type,
												EDropTarget drop_target)
{
	const DragAndDropEntry* entry = lookup(dad_type);
	if (entry)
	{
		return entry->mFunctions[(U8)drop_target];
	}
	return &LLToolDragAndDrop::dad3dNULL;
}

LLToolDragAndDrop::LLDragAndDropDictionary::LLDragAndDropDictionary()
{
 	//       										 DT_NONE                         DT_SELF                                        DT_AVATAR                   					DT_OBJECT                       					DT_LAND
	//      										|-------------------------------|----------------------------------------------|-----------------------------------------------|---------------------------------------------------|--------------------------------|
	addEntry(DAD_NONE, 			new DragAndDropEntry(&LLToolDragAndDrop::dad3dNULL,	&LLToolDragAndDrop::dad3dNULL,					&LLToolDragAndDrop::dad3dNULL,					&LLToolDragAndDrop::dad3dNULL,						&LLToolDragAndDrop::dad3dNULL));
	addEntry(DAD_TEXTURE, 		new DragAndDropEntry(&LLToolDragAndDrop::dad3dNULL,	&LLToolDragAndDrop::dad3dNULL,					&LLToolDragAndDrop::dad3dGiveInventory,			&LLToolDragAndDrop::dad3dTextureObject,				&LLToolDragAndDrop::dad3dNULL));
	addEntry(DAD_MATERIAL, 		new DragAndDropEntry(&LLToolDragAndDrop::dad3dNULL,	&LLToolDragAndDrop::dad3dNULL,					&LLToolDragAndDrop::dad3dGiveInventory,			&LLToolDragAndDrop::dad3dMaterialObject,			&LLToolDragAndDrop::dad3dNULL));
	addEntry(DAD_SOUND, 		new DragAndDropEntry(&LLToolDragAndDrop::dad3dNULL,	&LLToolDragAndDrop::dad3dNULL,					&LLToolDragAndDrop::dad3dGiveInventory,			&LLToolDragAndDrop::dad3dUpdateInventory,			&LLToolDragAndDrop::dad3dNULL));
	addEntry(DAD_CALLINGCARD, 	new DragAndDropEntry(&LLToolDragAndDrop::dad3dNULL,	&LLToolDragAndDrop::dad3dNULL,					&LLToolDragAndDrop::dad3dGiveInventory, 		&LLToolDragAndDrop::dad3dUpdateInventory, 			&LLToolDragAndDrop::dad3dNULL));
	addEntry(DAD_LANDMARK, 		new DragAndDropEntry(&LLToolDragAndDrop::dad3dNULL, &LLToolDragAndDrop::dad3dNULL, 					&LLToolDragAndDrop::dad3dGiveInventory, 		&LLToolDragAndDrop::dad3dUpdateInventory, 			&LLToolDragAndDrop::dad3dNULL));
	addEntry(DAD_SCRIPT, 		new DragAndDropEntry(&LLToolDragAndDrop::dad3dNULL, &LLToolDragAndDrop::dad3dNULL, 					&LLToolDragAndDrop::dad3dGiveInventory, 		&LLToolDragAndDrop::dad3dRezScript, 				&LLToolDragAndDrop::dad3dNULL));
	addEntry(DAD_CLOTHING, 		new DragAndDropEntry(&LLToolDragAndDrop::dad3dNULL, &LLToolDragAndDrop::dad3dWearItem, 				&LLToolDragAndDrop::dad3dGiveInventory, 		&LLToolDragAndDrop::dad3dUpdateInventory, 			&LLToolDragAndDrop::dad3dNULL));
	addEntry(DAD_OBJECT, 		new DragAndDropEntry(&LLToolDragAndDrop::dad3dNULL, &LLToolDragAndDrop::dad3dRezAttachmentFromInv,	&LLToolDragAndDrop::dad3dGiveInventoryObject,	&LLToolDragAndDrop::dad3dRezObjectOnObject, 		&LLToolDragAndDrop::dad3dRezObjectOnLand));
	addEntry(DAD_NOTECARD, 		new DragAndDropEntry(&LLToolDragAndDrop::dad3dNULL, &LLToolDragAndDrop::dad3dNULL, 					&LLToolDragAndDrop::dad3dGiveInventory, 		&LLToolDragAndDrop::dad3dUpdateInventory, 			&LLToolDragAndDrop::dad3dNULL));
	addEntry(DAD_CATEGORY, 		new DragAndDropEntry(&LLToolDragAndDrop::dad3dNULL, &LLToolDragAndDrop::dad3dWearCategory,			&LLToolDragAndDrop::dad3dGiveInventoryCategory,	&LLToolDragAndDrop::dad3dRezCategoryOnObject,		&LLToolDragAndDrop::dad3dNULL));
	addEntry(DAD_ROOT_CATEGORY, new DragAndDropEntry(&LLToolDragAndDrop::dad3dNULL,	&LLToolDragAndDrop::dad3dNULL,					&LLToolDragAndDrop::dad3dNULL,					&LLToolDragAndDrop::dad3dNULL,						&LLToolDragAndDrop::dad3dNULL));
	addEntry(DAD_BODYPART, 		new DragAndDropEntry(&LLToolDragAndDrop::dad3dNULL,	&LLToolDragAndDrop::dad3dWearItem,				&LLToolDragAndDrop::dad3dGiveInventory,			&LLToolDragAndDrop::dad3dUpdateInventory,			&LLToolDragAndDrop::dad3dNULL));
	addEntry(DAD_ANIMATION, 	new DragAndDropEntry(&LLToolDragAndDrop::dad3dNULL,	&LLToolDragAndDrop::dad3dNULL,					&LLToolDragAndDrop::dad3dGiveInventory,			&LLToolDragAndDrop::dad3dUpdateInventory,			&LLToolDragAndDrop::dad3dNULL));
	addEntry(DAD_GESTURE, 		new DragAndDropEntry(&LLToolDragAndDrop::dad3dNULL,	&LLToolDragAndDrop::dad3dActivateGesture,		&LLToolDragAndDrop::dad3dGiveInventory,			&LLToolDragAndDrop::dad3dUpdateInventory,			&LLToolDragAndDrop::dad3dNULL));
	addEntry(DAD_LINK, 			new DragAndDropEntry(&LLToolDragAndDrop::dad3dNULL,	&LLToolDragAndDrop::dad3dNULL,					&LLToolDragAndDrop::dad3dNULL,					&LLToolDragAndDrop::dad3dNULL,						&LLToolDragAndDrop::dad3dNULL));
#if LL_MESH_ASSET_SUPPORT
	addEntry(DAD_MESH, 			new DragAndDropEntry(&LLToolDragAndDrop::dad3dNULL,	&LLToolDragAndDrop::dad3dNULL,					&LLToolDragAndDrop::dad3dGiveInventory,			&LLToolDragAndDrop::dad3dMeshObject,				&LLToolDragAndDrop::dad3dNULL));
#endif
	addEntry(DAD_SETTINGS, 		new DragAndDropEntry(&LLToolDragAndDrop::dad3dNULL,	&LLToolDragAndDrop::dad3dNULL,					&LLToolDragAndDrop::dad3dGiveInventory,			&LLToolDragAndDrop::dad3dUpdateInventory,			&LLToolDragAndDrop::dad3dNULL));
	// *TODO: animation on self could play it ?  edit it ?
	// *TODO: gesture on self could play it ?  edit it ?
};

LLToolDragAndDrop::LLToolDragAndDrop()
:	LLTool("draganddrop", NULL),
	mDragStartX(0),
	mDragStartY(0),
	mSource(SOURCE_AGENT),
	mCursor(UI_CURSOR_NO),
	mLastAccept(ACCEPT_NO),
	mDrop(false),
	mCurItemIndex(0)
{
}

bool LLToolDragAndDrop::isOverThreshold(S32 x, S32 y)
{
	constexpr S32 MIN_MANHATTAN_DIST = 3;
	S32 manhattan_dist = abs(x - mDragStartX) + abs(y - mDragStartY);
	return manhattan_dist >= MIN_MANHATTAN_DIST;
}

void LLToolDragAndDrop::beginDrag(EDragAndDropType type,
								  const LLUUID& cargo_id, ESource source,
								  const LLUUID& source_id,
								  const LLUUID& object_id)
{
	if (type == DAD_NONE)
	{
		llwarns << "Attempted to start drag without a cargo type" << llendl;
		return;
	}
	LL_DEBUGS("DragAndDrop") << "Type: " << type << " - Source: " << source
							 << LL_ENDL;

	mCargoTypes.clear();
	mCargoTypes.push_back(type);
	mCargoIDs.clear();
	mCargoIDs.emplace_back(cargo_id);
	mSource = source;
	mSourceID = source_id;
	mObjectID = object_id;

	setMouseCapture(true);
	gToolMgr.setTransientTool(this);
	mCursor = UI_CURSOR_NO;
	if (mCargoTypes[0] == DAD_CATEGORY &&
		(mSource == SOURCE_AGENT || mSource == SOURCE_LIBRARY))
	{
		LLInventoryCategory* cat = gInventory.getCategory(cargo_id);
		// Go ahead and fire & forget the descendents if we are not dragging a
		// protected folder.
		if (cat)
		{
			LLViewerInventoryCategory::cat_array_t cats;
			LLViewerInventoryItem::item_array_t items;
			LLNoPreferredTypeOrItem is_not_preferred;
			uuid_vec_t folder_ids, item_ids;
			if (is_not_preferred(cat, NULL))
			{
				folder_ids.emplace_back(cargo_id);
			}
			gInventory.collectDescendentsIf(cargo_id, cats, items,
											LLInventoryModel::EXCLUDE_TRASH,
											is_not_preferred);
			for (S32 i = 0, count = cats.size(); i < count; ++i)
			{
				folder_ids.emplace_back(cats[i]->getUUID());
			}
			for (S32 i = 0, count = items.size(); i < count; ++i)
			{
				item_ids.emplace_back(items[i]->getUUID());
			}
			if (!folder_ids.empty() || !item_ids.empty())
			{
				LLCategoryFireAndForget fetcher;
				fetcher.fetch(folder_ids, item_ids);
			}
		}
	}
}

void LLToolDragAndDrop::beginMultiDrag(const std::vector<EDragAndDropType> types,
									   const std::vector<LLUUID>& cargo_ids,
									   ESource source, const LLUUID& source_id)
{
	for (std::vector<EDragAndDropType>::const_iterator it = types.begin(),
													   end = types.end();
		 it != end; ++it)
	{
		if (*it == DAD_NONE)
		{
			llwarns << "Attempted to start drag without a cargo type"
					<< llendl;
			return;
		}
	}

	LL_DEBUGS("DragAndDrop") << "Source: " << source << LL_ENDL;

	mCargoTypes = types;
	mCargoIDs = cargo_ids;
	mSource = source;
	mSourceID = source_id;

	setMouseCapture(true);
	gToolMgr.setTransientTool(this);
	mCursor = UI_CURSOR_NO;
	if (mSource == SOURCE_AGENT || mSource == SOURCE_LIBRARY)
	{
		// Find categories (i.e. inventory folders) in the cargo.
		std::set<LLUUID> cat_ids;
		for (S32 i = 0, count = llmin(cargo_ids.size(), types.size());
			 i < count; ++i)
		{
			LLInventoryCategory* catp = gInventory.getCategory(cargo_ids[i]);
			if (catp)
			{
				LLViewerInventoryCategory::cat_array_t cats;
				LLViewerInventoryItem::item_array_t items;
				LLNoPreferredType is_not_preferred;
				const LLUUID& cat_id = catp->getUUID();
				if (is_not_preferred(catp, NULL))
				{
					cat_ids.emplace(cat_id);
				}
				gInventory.collectDescendentsIf(cat_id, cats, items,
												LLInventoryModel::EXCLUDE_TRASH,
												is_not_preferred);
				for (S32 i = 0, cat_count = cats.size(); i < cat_count; ++i)
				{
					cat_ids.emplace(cat_id);
				}
			}
		}
		if (!cat_ids.empty())
		{
			uuid_vec_t folder_ids, item_ids;
			std::back_insert_iterator<uuid_vec_t> copier(folder_ids);
			std::copy(cat_ids.begin(), cat_ids.end(), copier);
			LLCategoryFireAndForget fetcher;
			fetcher.fetch(folder_ids, item_ids);
		}
	}
}

void LLToolDragAndDrop::endDrag()
{
	gSelectMgr.unhighlightAll();
	setMouseCapture(false);
}

// Called whenever the drag ends or if mouse capture is simply lost
//virtual
void LLToolDragAndDrop::onMouseCaptureLost()
{
	gToolMgr.clearTransientTool();
	mCargoTypes.clear();
	mCargoIDs.clear();
	mSource = SOURCE_AGENT;
	mSourceID.setNull();
	mObjectID.setNull();
}

//virtual
bool LLToolDragAndDrop::handleMouseUp(S32 x, S32 y, MASK mask)
{
	if (hasMouseCapture())
	{
		EAcceptance acceptance = ACCEPT_NO;
		dragOrDrop(x, y, mask, true, &acceptance);
		endDrag();
	}
	return true;
}

ECursorType LLToolDragAndDrop::acceptanceToCursor(EAcceptance acceptance)
{
	switch (acceptance)
	{
		case ACCEPT_YES_MULTI:
			mCursor = mCargoIDs.size() > 1 ? UI_CURSOR_ARROWDRAGMULTI
										   : UI_CURSOR_ARROWDRAG;
			break;

		case ACCEPT_YES_SINGLE:
			if (mCargoIDs.size() > 1)
			{
				mToolTipMsg = LLTrans::getString("TooltipMustSingleDrop");
				mCursor = UI_CURSOR_NO;
			}
			else
			{
				mCursor = UI_CURSOR_ARROWDRAG;
			}
			break;

		case ACCEPT_NO_LOCKED:
			mCursor = UI_CURSOR_NOLOCKED;
			break;

		case ACCEPT_NO:
			mCursor = UI_CURSOR_NO;
			break;

		case ACCEPT_YES_COPY_MULTI:
			mCursor = mCargoIDs.size() > 1 ? UI_CURSOR_ARROWCOPYMULTI
										   : UI_CURSOR_ARROWCOPY;
			break;

		case ACCEPT_YES_COPY_SINGLE:
			if (mCargoIDs.size() > 1)
			{
				mToolTipMsg = LLTrans::getString("TooltipMustSingleDrop");
				mCursor = UI_CURSOR_NO;
			}
			else
			{
				mCursor = UI_CURSOR_ARROWCOPY;
			}
			break;

		case ACCEPT_POSTPONED:
			break;

		default:
			llassert(false);
	}

	return mCursor;
}

//virtual
bool LLToolDragAndDrop::handleHover(S32 x, S32 y, MASK mask)
{
	EAcceptance acceptance = ACCEPT_NO;
	dragOrDrop(x, y, mask, false, &acceptance);

	ECursorType cursor = acceptanceToCursor(acceptance);
	gWindowp->setCursor(cursor);

	LL_DEBUGS("UserInput") << "hover handled by LLToolDragAndDrop" << LL_ENDL;
	return true;
}

//virtual
bool LLToolDragAndDrop::handleKey(KEY key, MASK mask)
{
	if (key == KEY_ESCAPE && mask == MASK_NONE)
	{
		// Cancel drag and drop operation
		endDrag();
		return true;
	}
	return false;
}

//virtual
bool LLToolDragAndDrop::handleToolTip(S32 x, S32 y, std::string& msg,
									  LLRect* sticky_rect_screen)
{
	if (!mToolTipMsg.empty())
	{
		msg = mToolTipMsg;
		return true;
	}
	return false;
}

//virtual
void LLToolDragAndDrop::handleDeselect()
{
	mToolTipMsg.clear();
}

void LLToolDragAndDrop::dragOrDrop(S32 x, S32 y, MASK mask, bool drop,
								   EAcceptance* acceptance)
{
	*acceptance = ACCEPT_YES_MULTI;

	bool handled = false;

	LLView* top_view = gFocusMgr.getTopCtrl();
	LLViewerInventoryItem* item;
	LLViewerInventoryCategory* cat;

	mToolTipMsg.clear();

	if (top_view)
	{
		handled = true;

		for (mCurItemIndex = 0; mCurItemIndex < (S32)mCargoIDs.size();
			 ++mCurItemIndex)
		{
			LLInventoryObject* cargo = locateInventory(item, cat);

			if (cargo)
			{
				S32 local_x, local_y;
				top_view->screenPointToLocal(x, y, &local_x, &local_y);
				EAcceptance item_acceptance = ACCEPT_NO;
				handled &=
					top_view->handleDragAndDrop(local_x, local_y,
												mask, false,
												mCargoTypes[mCurItemIndex],
												(void*)cargo, &item_acceptance,
												mToolTipMsg);
				if (handled)
				{
					// Use sort order to determine priority of acceptance
					*acceptance = (EAcceptance)llmin((U32)item_acceptance,
													 (U32)*acceptance);
				}
			}
			else
			{
				return;
			}
		}

		// All objects passed, go ahead and perform drop if necessary
		if (handled && drop && (U32)*acceptance >= ACCEPT_YES_COPY_SINGLE)
		{
			if ((U32)*acceptance < ACCEPT_YES_COPY_MULTI &&
				mCargoIDs.size() > 1)
			{
				// Tried to give multi-cargo to a single-acceptor: refuse and
				// return.
				*acceptance = ACCEPT_NO;
				return;
			}

			for (mCurItemIndex = 0; mCurItemIndex < (S32)mCargoIDs.size();
				 ++mCurItemIndex)
			{
				LLInventoryObject* cargo = locateInventory(item, cat);

				if (cargo)
				{
					S32 local_x, local_y;

					EAcceptance item_acceptance;
					top_view->screenPointToLocal(x, y, &local_x, &local_y);
					handled &=
						top_view->handleDragAndDrop(local_x, local_y,
													mask, true,
													mCargoTypes[mCurItemIndex],
													(void*)cargo,
													&item_acceptance,
													mToolTipMsg);
				}
			}
		}
		if (handled)
		{
			mLastAccept = (EAcceptance)*acceptance;
		}
	}

	if (!handled)
	{
		handled = true;

		LLView* root_view = gViewerWindowp->getRootView();

		for (mCurItemIndex = 0; mCurItemIndex < (S32)mCargoIDs.size();
			 ++mCurItemIndex)
		{
			LLInventoryObject* cargo = locateInventory(item, cat);

			if (!cargo)
			{
				handled = false;
				break;
			}

			EAcceptance item_acceptance = ACCEPT_NO;
			handled &=
			  root_view->handleDragAndDrop(x, y, mask, false,
										   mCargoTypes[mCurItemIndex],
										   (void*)cargo, &item_acceptance,
										   mToolTipMsg);
			if (handled)
			{
				// Use sort order to determine priority of acceptance
				*acceptance = (EAcceptance)llmin((U32)item_acceptance,
												 (U32)*acceptance);
			}
		}
		// All objects passed, go ahead and perform drop if necessary
		if (handled && drop && (U32)*acceptance > ACCEPT_NO_LOCKED)
		{
			if ((U32)*acceptance < ACCEPT_YES_COPY_MULTI &&
			    mCargoIDs.size() > 1)
			{
				// Tried to give multi-cargo to a single-acceptor: refuse and
				// return.
				*acceptance = ACCEPT_NO;
				return;
			}

			for (mCurItemIndex = 0; mCurItemIndex < (S32)mCargoIDs.size();
				 ++mCurItemIndex)
			{
				LLInventoryObject* cargo = locateInventory(item, cat);
				if (cargo)
				{
					EAcceptance item_acceptance;
					handled &=
						root_view->handleDragAndDrop(x, y, mask, true,
													 mCargoTypes[mCurItemIndex],
													 (void*)cargo,
													  &item_acceptance,
													  mToolTipMsg);
				}
			}
		}

		if (handled)
		{
			mLastAccept = (EAcceptance)*acceptance;
		}
	}

	if (!handled)
	{
		dragOrDrop3D(x, y, mask, drop, acceptance);
	}
}

void LLToolDragAndDrop::dragOrDrop3D(S32 x, S32 y, MASK mask, bool drop,
									 EAcceptance* acceptance)
{
	mDrop = drop;
	if (mDrop)
	{
		// Note: do not allow drag and drop onto transparent objects (this
		// defaults to false already in pickImmediate()).
		pickCallback(gViewerWindowp->pickImmediate(x, y));
	}
	else
	{
		// Note: do not allow drag and drop onto transparent objects (this
		// defaults to false already in pickAsync()).
		gViewerWindowp->pickAsync(x, y, mask, pickCallback);
	}

	*acceptance = mLastAccept;
}

//static
void LLToolDragAndDrop::pickCallback(const LLPickInfo& pick_info)
{
	LLToolDragAndDrop* self = &gToolDragAndDrop;
	EDropTarget target = DT_NONE;
	S32	hit_face = -1;

	LLViewerObject* hit_objp = pick_info.getObject();
	gSelectMgr.unhighlightAll();
	bool highlight_object = false;
	// Treat attachments as part of the avatar they are attached to.
	if (hit_objp)
	{
		// Do not allow drag and drop on grass, trees, etc.
		if (pick_info.mPickType == LLPickInfo::PICK_FLORA)
		{
			self->mCursor = UI_CURSOR_NO;
			gWindowp->setCursor(self->mCursor);
			return;
		}

		if (hit_objp->isAttachment() && !hit_objp->isHUDAttachment())
		{
			LLVOAvatar* avp = LLVOAvatar::findAvatarFromAttachment(hit_objp);
			if (!avp)
			{
				self->mLastAccept = ACCEPT_NO;
				self->mCursor = UI_CURSOR_NO;
				gWindowp->setCursor(self->mCursor);
				return;
			}
			hit_objp = avp;
		}

		if (hit_objp->isAvatar())
		{
			if (((LLVOAvatar*)hit_objp)->isSelf())
			{
				target = DT_SELF;
				hit_face = -1;
			}
			else
			{
				target = DT_AVATAR;
				hit_face = -1;
			}
		}
		else
		{
			target = DT_OBJECT;
			hit_face = pick_info.mObjectFace;
			highlight_object = true;
		}
	}
	else if (pick_info.mPickType == LLPickInfo::PICK_LAND)
	{
		target = DT_LAND;
		hit_face = -1;
	}

	self->mLastAccept = ACCEPT_YES_MULTI;

	for (self->mCurItemIndex = 0;
		 self->mCurItemIndex < (S32)self->mCargoIDs.size();
		 self->mCurItemIndex++)
	{
		const S32 item_index = self->mCurItemIndex;
		const EDragAndDropType dad_type = self->mCargoTypes[item_index];
		// Call the right implementation function
		self->mLastAccept = (EAcceptance)llmin(
			(U32)self->mLastAccept,
			(U32)callMemberFunction(*self,
									LLDragAndDropDictionary::getInstance()->get(dad_type,
																				target))
										(hit_objp, hit_face,
										 pick_info.mKeyMask, false));
	}

	if (self->mDrop && (U32)self->mLastAccept >= ACCEPT_YES_COPY_SINGLE)
	{
		// If target allows multi-drop or there is only one item being dropped,
		// go ahead
		if (self->mLastAccept >= ACCEPT_YES_COPY_MULTI ||
		    self->mCargoIDs.size() == 1)
		{
			// Target accepts multi, or cargo is a single-drop
			for (self->mCurItemIndex = 0;
			     self->mCurItemIndex < (S32)self->mCargoIDs.size();
			     ++self->mCurItemIndex)
			{
				const S32 item_index = self->mCurItemIndex;
				const EDragAndDropType dad_type = self->mCargoTypes[item_index];
				// Call the right implementation function
				(U32)callMemberFunction(*self,
										LLDragAndDropDictionary::getInstance()->get(dad_type,
																					target))
											(hit_objp, hit_face,
											 pick_info.mKeyMask, true);
			}
		}
		else
		{
			// Target does not accept multi, but cargo is multi
			self->mLastAccept = ACCEPT_NO;
		}
	}

	if (highlight_object && self->mLastAccept > ACCEPT_NO_LOCKED)
	{
		// If any item being dragged will be applied to the object under our
		// cursor highlight that object
		for (S32 i = 0, count = self->mCargoIDs.size(); i < count; ++i)
		{
			if (self->mCargoTypes[i] != DAD_OBJECT ||
				(pick_info.mKeyMask & MASK_CONTROL))
			{
				gSelectMgr.highlightObjectAndFamily(hit_objp);
				break;
			}
		}
	}
	ECursorType cursor = self->acceptanceToCursor(self->mLastAccept);
	gWindowp->setCursor(cursor);

	self->mLastHitPos = pick_info.mPosGlobal;
	self->mLastCameraPos = gAgent.getCameraPositionGlobal();
}

//static
bool LLToolDragAndDrop::handleDropAssetProtections(LLViewerObject* hit_objp,
												   LLInventoryItem* itemp,
												   ESource source,
												   const LLUUID& src_id)
{
	if (!itemp) return false;

	// Always succeed if asset is from the library or already in the contents
	// of the object
	if (source == SOURCE_LIBRARY)
	{
		// Dropping an asset from the library always just works.
		return true;
	}

	// In case the inventory has not been updated (e.g. due to some recent
	// operation causing a dirty inventory) and we can do an update, stall the
	// user while fetching the inventory.
	// Fetch if inventory is both dirty and listener is present (otherwise we
	// will not receive update).
	if (hit_objp->isInventoryDirty() && hit_objp->hasInventoryListeners())
	{
		hit_objp->requestInventory();
		LLSD args;
		args["ERROR_MESSAGE"] = "Unable to add asset.\nPlease wait a few seconds and try again.";
		gNotifications.add("ErrorMessage", args);
		return false;
	}
	// Make sure to verify both asset Id and asset type since a null UUID is a
	// shared default for different asset types.
	if (hit_objp->getInventoryItemByAsset(itemp->getAssetUUID(),
										  itemp->getType()))
	{
		// If the asset is already in the object's inventory then it can always
		// be added to a side. This saves some work if the task's inventory is
		// already loaded and ensures that the texture item is only added once.
		return true;
	}

	LLPointer<LLViewerInventoryItem> new_itemp =
		new LLViewerInventoryItem(itemp);
	const LLPermissions& perms = itemp->getPermissions();
	if (!perms.allowCopyBy(gAgentID))
	{
		// Check that we can add the asset as inventory to the object
		if (willObjectAcceptInventory(hit_objp,
									  itemp) < ACCEPT_YES_COPY_SINGLE)
		{
			return false;
		}
		// Make sure the object has the asset in its inventory.
		if (source == SOURCE_AGENT)
		{
			// Remove the asset from local inventory. The server will actually
			// remove the item from agent inventory.
			gInventory.deleteObject(itemp->getUUID());
			gInventory.notifyObservers();
		}
		else if (source == SOURCE_WORLD)
		{
			LLViewerObject* src_objp = gObjectList.findObject(src_id);
			// *FIX: if the objects are in different regions, and the source
			// region has crashed, you can bypass these permissions.
			if (!src_objp)
			{
				llwarns << "Unable to find source object." << llendl;
				return false;
			}
			src_objp->removeInventory(itemp->getUUID());
		}

		// Add the asset's corresponding item to the target object's inventory.
		hit_objp->updateInventory(new_itemp, true);

		// Force the object to update and refetch its inventory so it has this
		// asset.
		hit_objp->dirtyInventory();
		hit_objp->requestInventory();

		// *TODO: check to see if adding the item was successful; if not, then
		// we should return false here.
	}
	else if (!perms.allowTransferBy(gAgentID))
	{
		// Check that we can add the asset as inventory to the object
		if (willObjectAcceptInventory(hit_objp,
									  itemp) < ACCEPT_YES_COPY_SINGLE)
		{
			return false;
		}

		// Add the asset item to the target object's inventory.
		hit_objp->updateInventory(new_itemp, true);

		// Force the object to update and refetch its inventory so it has this
		// asset.
		hit_objp->dirtyInventory();
		hit_objp->requestInventory();

		// *TODO: check to see if adding the item was successful; if not, then
		// we should return false here. This will requre a separate listener
		// since without listener, we have no way to receive update.
	}
	else if (new_itemp->getType() == LLAssetType::AT_MATERIAL &&
			 !itemp->getPermissions().allowModifyBy(gAgentID))
	{
		// Check that we can add the material as inventory to the object
		if (willObjectAcceptInventory(hit_objp,
									  itemp) < ACCEPT_YES_COPY_SINGLE)
		{
			return false;
		}
		// *FIXME: may want to make sure agent can paint hit_objp.

		// Add the material item to the target object's inventory.
		 hit_objp->updateInventory(new_itemp, true);

		// Force the object to update and refetch its inventory so it has this
		// asset.
		hit_objp->dirtyInventory();
		hit_objp->requestInventory();

		// *TODO: check to see if adding the item was successful; if not, then
		// we should return false here. This will requre a separate listener
		// since without listener, we have no way to receive update.
	}

	return true;
}

//static
void LLToolDragAndDrop::dropTextureOneFace(LLViewerObject* hit_objp,
										   S32 hit_face,
										   LLInventoryItem* itemp,
										   ESource source,
										   const LLUUID& src_id)
{
	if (!hit_objp)
	{
		llwarns << "No hit object." << llendl;
		return;
	}

	if (hit_face == -1)
	{
		return;
	}

	if (!itemp)
	{
		llwarns << "No texture item." << llendl;
		return;
	}

	if (hit_objp->getRenderMaterialID(hit_face).notNull())
	{
		return;
	}

	const LLUUID& asset_id = itemp->getAssetUUID();
	if (!handleDropAssetProtections(hit_objp, itemp, source, src_id))
	{
		return;
	}

	// Update viewer side image in anticipation of update from simulator
	LLViewerTexture* image = LLViewerTextureManager::getFetchedTexture(asset_id);
	if (!image)
	{
		llwarns << "Image " << asset_id << " not found" << llendl;
		return;
	}

	gViewerStats.incStat(LLViewerStats::ST_EDIT_TEXTURE_COUNT);

	LLPanelFace* panelp =
		LLFloaterTools::isVisible() ? gFloaterToolsp->getPanelFace() : NULL;

	LLTextureEntry* tep = hit_objp ? hit_objp->getTE(hit_face) : NULL;

	LLRender::eTexIndex channel = LLPanelFace::getTextureChannelToEdit();

	if (tep && panelp &&
		(channel == LLRender::NORMAL_MAP || channel == LLRender::SPECULAR_MAP))
	{
		LLMaterialPtr old_matp = tep->getMaterialParams();
		LLMaterialPtr new_matp = panelp->createDefaultMaterial(old_matp);
		if (channel == LLRender::NORMAL_MAP)
		{
			new_matp->setNormalID(asset_id);
			tep->setMaterialParams(new_matp);
			hit_objp->setTENormalMap(hit_face, asset_id);
		}
		else
		{
			new_matp->setSpecularID(asset_id);
			tep->setMaterialParams(new_matp);
			hit_objp->setTESpecularMap(hit_face, asset_id);
		}
		LLMaterialMgr::getInstance()->put(hit_objp->getID(), hit_face,
										  *new_matp);
	}
	else
	{
		hit_objp->setTEImage(hit_face, image);
	}

	dialog_refresh_all();

	// Send the update to the simulator
	hit_objp->sendTEUpdate();
}

//static
void LLToolDragAndDrop::dropTextureAllFaces(LLViewerObject* hit_objp,
											LLInventoryItem* itemp,
											ESource source,
											const LLUUID& src_id)
{
	if (!hit_objp)
	{
		llwarns << "No hit object." << llendl;
		return;
	}

	if (!itemp)
	{
		llwarns << "No texture item." << llendl;
		return;
	}

	U8 num_tes = hit_objp->getNumTEs();

	for (U8 te = 0; te < num_tes; ++te)
	{
		if (hit_objp->getRenderMaterialID(te).notNull())
		{
			return;	// Got a PBR face: do not ruin it.
		}
	}

	const LLUUID& asset_id = itemp->getAssetUUID();
	bool success = handleDropAssetProtections(hit_objp, itemp, source, src_id);
	if (!success)
	{
		return;
	}

	LLViewerTexture* texp =
		LLViewerTextureManager::getFetchedTexture(asset_id);
	if (!texp)
	{
		llwarns << "Texture " << asset_id << " not found" << llendl;
		return;
	}

	bool updated = false;
	for (U8 i = 0; i < num_tes; ++i)
	{
		if (hit_objp->getRenderMaterialID(i).isNull())
		{
			// Update viewer side texture in anticipation of update from
			// simulator
			hit_objp->setTEImage(i, texp);
			updated = true;
		}
	}
	if (updated)
	{
		// Send the update to the simulator
		hit_objp->sendTEUpdate();
		dialog_refresh_all();
		gViewerStats.incStat(LLViewerStats::ST_EDIT_TEXTURE_COUNT);
	}
}

void LLToolDragAndDrop::dropMaterialOneFace(LLViewerObject* hit_objp,
											S32 hit_face,
											LLInventoryItem* itemp,
											ESource source,
											const LLUUID& src_id)
{
	if (!hit_objp)
	{
		llwarns << "No hit object." << llendl;
		return;
	}

	if (hit_face == -1)
	{
		return;
	}

	if (!itemp || itemp->getInventoryType() != LLInventoryType::IT_MATERIAL)
	{
		llwarns << "No material item." << llendl;
		return;
	}

	// SL-20013 must save asset_id before handleDropAssetProtections since
	// our itemp instance may be deleted if it is moved into task inventory.
	LLUUID asset_id = itemp->getAssetUUID();
	if (!handleDropAssetProtections(hit_objp, itemp, source, src_id))
	{
		return;
	}

	if (asset_id.isNull())
	{
		// Use blank material
		asset_id = BLANK_MATERIAL_ASSET_ID;
	}

	hit_objp->setRenderMaterialID(hit_face, asset_id);
	dialog_refresh_all();

	// Send the update to the simulator
	hit_objp->sendTEUpdate();
}

void LLToolDragAndDrop::dropMaterialAllFaces(LLViewerObject* hit_objp,
											 LLInventoryItem* itemp,
											 ESource source,
											 const LLUUID& src_id)
{
	if (!itemp || itemp->getInventoryType() != LLInventoryType::IT_MATERIAL)
	{
		llwarns << "No material item." << llendl;
		return;
	}

	// SL-20013 must save asset_id before handleDropAssetProtections since
	// our itemp instance may be deleted if it is moved into task inventory.
	LLUUID asset_id = itemp->getAssetUUID();
	if (!handleDropAssetProtections(hit_objp, itemp, source, src_id))
	{
		return;
	}

	if (asset_id.isNull())
	{
		// Use blank material
		asset_id = BLANK_MATERIAL_ASSET_ID;
	}

	hit_objp->setRenderMaterialIDs(asset_id);
	dialog_refresh_all();

	// Send the update to the simulator
	hit_objp->sendTEUpdate();
}

#if LL_MESH_ASSET_SUPPORT
//static
void LLToolDragAndDrop::dropMesh(LLViewerObject* hit_objp,
								 LLInventoryItem* itemp,
								 ESource source,
								 const LLUUID& src_id)
{
	if (!hit_objp)
	{
		llwarns << "No hit object." << llendl;
		return;
	}

	if (!itemp)
	{
		llwarns << "No inventory item." << llendl;
		return;
	}

	if (!handleDropAssetProtections(hit_objp, itemp, source, src_id)
	{
		return;
	}

	const LLUUID& asset_id = itemp->getAssetUUID();
	LLSculptParams sculpt_params;
	sculpt_params.setSculptTexture(asset_id, LL_SCULPT_TYPE_MESH);
	hit_objp->setParameterEntry(LLNetworkData::PARAMS_SCULPT, sculpt_params,
								true);

	dialog_refresh_all();
}
#endif

//static
void LLToolDragAndDrop::dropScript(LLViewerObject* hit_objp,
								   LLInventoryItem* itemp, bool active,
								   ESource source, const LLUUID& src_id)
{
	// *HACK: In order to resolve SL-22177, we need to block drags
	// from notecards and objects onto other objects.
	ESource source2 = gToolDragAndDrop.mSource;
	if (source2 == SOURCE_WORLD || source2 == SOURCE_NOTECARD)
	{
		llwarns << "Illegal call, from world or notecard." << llendl;
		return;
	}
	if (!hit_objp || !itemp)
	{
		return;
	}
//MK
	if (gRLenabled)
	{
		// Cannot edit objects that we are sitting on, when sit-restricted
		if ((gRLInterface.mSittpMax < EXTREMUM ||
			 gRLInterface.mContainsUnsit) &&
			hit_objp->isAgentSeat())
		{
			LL_DEBUGS("DragAndDrop") << "Cannot drop script in RLV locked seat"
									 << LL_ENDL;
			return;
		}

		if (!gRLInterface.canDetach(hit_objp))
		{
			LL_DEBUGS("DragAndDrop") << "Cannot drop script in RLV locked attachment"
									 << LL_ENDL;
			return;
		}
	}
//mk
	LLPointer<LLViewerInventoryItem> new_scriptp =
		new LLViewerInventoryItem(itemp);
	if (!itemp->getPermissions().allowCopyBy(gAgentID))
	{
		if (source == SOURCE_AGENT)
		{
			// Remove the script from local inventory. The server will actually
			// remove the item from agent inventory.
			gInventory.deleteObject(itemp->getUUID());
			gInventory.notifyObservers();
		}
		else if (source == SOURCE_WORLD)
		{
			// *FIX: if the objects are in different regions, and the source
			// region has crashed, you can bypass these permissions.
			LLViewerObject* src_objp = gObjectList.findObject(src_id);
			if (!src_objp)
			{
				llwarns << "Unable to find source object." << llendl;
				return;
			}
			src_objp->removeInventory(itemp->getUUID());
		}
	}
	hit_objp->saveScript(new_scriptp, active, true);
	if (gFloaterToolsp)
	{
		gFloaterToolsp->dirty();
	}

	// VEFFECT: SetScript
	LLHUDEffectSpiral::agentBeamToObject(hit_objp);
}

void LLToolDragAndDrop::dropObject(LLViewerObject* hit_objp,
								   bool bypass_sim_raycast,
								   bool from_task_inventory,
								   bool remove_from_inventory)
{
	LLViewerRegion* regionp = gWorld.getRegionFromPosGlobal(mLastHitPos);
	if (!regionp)
	{
		llwarns << "Could not find region to rez object" << llendl;
		return;
	}

//MK
	if (gRLenabled &&
		(gRLInterface.mContainsRez || gRLInterface.mContainsInteract))
	{
		return;
	}
//mk

	LLViewerInventoryItem* itemp;
	LLViewerInventoryCategory* catp;
	locateInventory(itemp, catp);
	if (!itemp || !itemp->isFinished())
	{
		LL_DEBUGS("DragAndDrop") << "Inventory item is not finished. Aborted."
								 << LL_ENDL;
		return;
	}

	const LLUUID& item_id = itemp->getUUID();

	LL_DEBUGS("DragAndDrop") << "Rezzing object" << LL_ENDL;
	make_ui_sound("UISndObjectRezIn");

	if (regionp && regionp->getRegionFlag(REGION_FLAGS_SANDBOX))
	{
		LLFirstUse::useSandbox();
	}

	// Limit raycast to a single object. Speeds up server raycast and avoids
	// problems with server ray hitting objects that were clipped by the near
	// plane or culled on the viewer.
	LLUUID ray_target_id;
	if (hit_objp)
	{
		ray_target_id = hit_objp->getID();
	}

	// Check if it cannot be copied, and mark as remove in that case: this will
	// remove the object from inventory after rezzing. Only bother with this
	// check if we would not normally remove from inventory.
	if (!remove_from_inventory &&
		!itemp->getPermissions().allowCopyBy(gAgentID))
	{
		remove_from_inventory = true;
	}

	// Check if it is in the trash.
	bool is_in_trash = gInventory.isInTrash(item_id);

	const LLUUID& source_id = from_task_inventory ? mSourceID : LLUUID::null;

	// Select the object only if we're editing.
	bool rez_selected = gToolMgr.inEdit();

	LLVector3 ray_start = regionp->getPosRegionFromGlobal(mLastCameraPos);
	LLVector3 ray_end   = regionp->getPosRegionFromGlobal(mLastHitPos);
	// Currently the ray's end point is an approximation, and is sometimes too
	// short (causing failure),  so we double the ray's length:
	if (!bypass_sim_raycast)
	{
		LLVector3 ray_direction = ray_start - ray_end;
		ray_end = ray_end - ray_direction;
	}

	// Message packing code should be it's own uninterrupted block
	LLMessageSystem* msg = gMessageSystemp;
	if (mSource == SOURCE_NOTECARD)
	{
		msg->newMessageFast(_PREHASH_RezObjectFromNotecard);
	}
	else
	{
		msg->newMessageFast(_PREHASH_RezObject);
	}
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	LLUUID group_id = gAgent.getGroupID();
	if (gSavedSettings.getBool("RezWithLandGroup"))
	{
		LLParcel* parcelp = gViewerParcelMgr.getAgentParcel();
		if (gAgent.isInGroup(parcelp->getGroupID()))
		{
			group_id = parcelp->getGroupID();
		}
		else if (gAgent.isInGroup(parcelp->getOwnerID()))
		{
			group_id = parcelp->getOwnerID();
		}
	}
	msg->addUUIDFast(_PREHASH_GroupID, group_id);

	msg->nextBlock("RezData");
	// If it is being rezzed from task inventory, we need to enable saving it
	// back into the task inventory.
	// *FIXME: We can probably compress this to a single byte, since I think
	// folderid == mSourceID. This will be a later optimization.
	msg->addUUIDFast(_PREHASH_FromTaskID, source_id);
	msg->addU8Fast(_PREHASH_BypassRaycast, (U8)bypass_sim_raycast);
	msg->addVector3Fast(_PREHASH_RayStart, ray_start);
	msg->addVector3Fast(_PREHASH_RayEnd, ray_end);
	msg->addUUIDFast(_PREHASH_RayTargetID, ray_target_id);
	msg->addBoolFast(_PREHASH_RayEndIsIntersection, false);
	msg->addBoolFast(_PREHASH_RezSelected, rez_selected);
	msg->addBoolFast(_PREHASH_RemoveItem, remove_from_inventory);

	// Deal with permissions slam logic
	pack_permissions_slam(msg, itemp->getFlags(), itemp->getPermissions());

	LLUUID folder_id = itemp->getParentUUID();
	if (mSource == SOURCE_LIBRARY || is_in_trash)
	{
		// Since it is coming from the library or trash, we want to not 'take'
		// it back to the same place.
		itemp->setParent(LLUUID::null);
		// *TODO this code is not working: the parent (FolderID) is still set
		// when the object is "taken". So code on the "take" side is checking
		// for trash and library as well (llviewermenu.cpp)
		LL_DEBUGS("DragAndDrop") << "Rezzed object parent set to a null UUID"
								 << LL_ENDL;
	}
	if (mSource == SOURCE_NOTECARD)
	{
		msg->nextBlockFast(_PREHASH_NotecardData);
		msg->addUUIDFast(_PREHASH_NotecardItemID, mSourceID);
		msg->addUUIDFast(_PREHASH_ObjectID, mObjectID);
		msg->nextBlockFast(_PREHASH_InventoryData);
		msg->addUUIDFast(_PREHASH_ItemID, item_id);
		LL_DEBUGS("DragAndDrop") << "Rezzed object parent set to a notecard"
								 << LL_ENDL;
	}
	else
	{
		msg->nextBlockFast(_PREHASH_InventoryData);
		itemp->packMessage(msg);
		LL_DEBUGS("DragAndDrop") << "Rezzed object parent set to category: "
								 << folder_id.asString() << LL_ENDL;
	}
	msg->sendReliable(regionp->getHost());
	// Back out the change; no actual internal changes take place.
	itemp->setParent(folder_id);

	// If we are going to select it, get ready for the incoming selected object
	if (rez_selected)
	{
		gSelectMgr.deselectAll();
		gWindowp->incBusyCount();
	}

	if (remove_from_inventory)
	{
		// Delete it from inventory immediately so that users cannot easily
		// bypass copy protection in laggy situations. If the rez fails, we
		// will put it back on the server.
		gInventory.deleteObject(item_id);
		gInventory.notifyObservers();
	}

	// VEFFECT: DropObject
	LLHUDEffectSpiral::agentBeamToPosition(mLastHitPos);

	gViewerStats.incStat(LLViewerStats::ST_REZ_COUNT);
}

//static
void LLToolDragAndDrop::dropInventory(LLViewerObject* hit_objp,
									  LLInventoryItem* itemp,
									  ESource source,
									  const LLUUID& src_id)
{
	if (!hit_objp)
	{
		llwarns << "No hit object." << llendl;
		return;
	}

	if (!itemp)
	{
		llwarns << "No inventory item." << llendl;
		return;
	}

	// *HACK: in order to resolve SL-22177, we need to block drags from
	// notecards and objects onto other objects.
	ESource source2 = gToolDragAndDrop.mSource;
	if (source2 == SOURCE_WORLD || source2 == SOURCE_NOTECARD)
	{
		llwarns << "Illegal call done from world or notecard." << llendl;
		return;
	}

	LLPointer<LLViewerInventoryItem> new_itemp =
		new LLViewerInventoryItem(itemp);
	time_t creation_date = time_corrected();
	new_itemp->setCreationDate(creation_date);

	if (!itemp->getPermissions().allowCopyBy(gAgentID))
	{
		if (source == SOURCE_AGENT)
		{
			// Remove the inventory item from local inventory. The server will
			// actually remove the item from agent inventory.
			gInventory.deleteObject(itemp->getUUID());
			gInventory.notifyObservers();
		}
		else if (source == SOURCE_WORLD)
		{
			// *FIX: if the objects are in different regions, and the source
			// region has crashed, you can bypass these permissions.
			LLViewerObject* src_objp = gObjectList.findObject(src_id);
			if (src_objp)
			{
				src_objp->removeInventory(itemp->getUUID());
			}
			else
			{
				llwarns << "Unable to find source object." << llendl;
				return;
			}
		}
	}

	hit_objp->updateInventory(new_itemp, true);
	if (LLFloaterTools::isVisible())
	{
		// *FIX: only show this if panel not expanded ?
		gFloaterToolsp->showPanel(LLFloaterTools::PANEL_CONTENTS);
	}

	// VEFFECT: AddToInventory
	LLHUDEffectSpiral::agentBeamToObject(hit_objp);

	if (gFloaterToolsp)
	{
		gFloaterToolsp->dirty();
	}
}

struct LLGiveInventoryInfo
{
	LLUUID mToAgentID;
	LLUUID mInventoryObjectID;
	LLUUID mIMSessionID;
	LLGiveInventoryInfo(const LLUUID& to_agent, const LLUUID& obj_id,
						const LLUUID& im_session_id = LLUUID::null)
	:	mToAgentID(to_agent),
		mInventoryObjectID(obj_id),
		mIMSessionID(im_session_id)
	{
	}
};

//static
void LLToolDragAndDrop::giveInventory(const LLUUID& to_agent,
									  LLInventoryItem* itemp,
									  const LLUUID& im_session_id)
{
	if (!itemp || !isInventoryGiveAcceptable(itemp))
	{
		return;
	}
//MK
	if (gRLenabled &&
		gRLInterface.containsWithoutException("share", to_agent.asString()))
	{
		gNotifications.add("CannotGiveItem");
		return;
	}
//mk

	const LLUUID& item_id = itemp->getUUID();

	llinfos << "Giving inventory item " << item_id << " to agent "
			<< to_agent << llendl;
	if (itemp->getPermissions().allowCopyBy(gAgentID))
	{
		// Just give it away.
		commitGiveInventoryItem(to_agent, itemp, im_session_id);
	}
	else
	{
		// Ask if the agent is sure.
		LLSD payload;
		payload["agent_id"] = to_agent;
		payload["item_id"] = item_id;
		gNotifications.add("CannotCopyWarning", LLSD(), payload,
						   &handleCopyProtectedItem);
	}
}

//static
bool LLToolDragAndDrop::handleCopyProtectedItem(const LLSD& notification,
												const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response))
	{
		// No, cancel, whatever, who cares, not yes.
		gNotifications.add("TransactionCancelled");
		return false;
	}

	LLInventoryItem* itemp =
		gInventory.getItem(notification["payload"]["item_id"].asUUID());
	if (!itemp)
	{
		gNotifications.add("CannotGiveItem");
		return false;
	}

	LLUUID agent_id = notification["payload"]["agent_id"].asUUID();
	commitGiveInventoryItem(agent_id, itemp);
	// Delete it from viewer inventory for now; it will be deleted on the
	// server quickly enough.
	gInventory.deleteObject(notification["payload"]["item_id"].asUUID());
	gInventory.notifyObservers();
	return false;
}

//static
void LLToolDragAndDrop::commitGiveInventoryItem(const LLUUID& to_agent,
												LLInventoryItem* itemp,
												const LLUUID& im_session_id)
{
	if (!itemp) return;

	std::string name;
	gAgent.buildFullname(name);

	LLUUID transaction_id;
	transaction_id.generate();

	constexpr S32 BUCKET_SIZE = sizeof(U8) + UUID_BYTES;
	U8 bucket[BUCKET_SIZE];
	bucket[0] = (U8)itemp->getType();
	memcpy(&bucket[1], &(itemp->getUUID().mData), UUID_BYTES);
	pack_instant_message(gAgentID, false, gAgentSessionID, to_agent, name,
						 itemp->getName(), IM_ONLINE, IM_INVENTORY_OFFERED,
						 transaction_id, 0, LLUUID::null,
						 gAgent.getPositionAgent(), NO_TIMESTAMP, bucket,
						 BUCKET_SIZE);
	gAgent.sendReliableMessage();

	// VEFFECT: giveInventory
	LLHUDEffectSpiral::agentBeamToObject(gObjectList.findObject(to_agent));
	if (gFloaterToolsp)
	{
		gFloaterToolsp->dirty();
	}

	LLMuteList::autoRemove(to_agent, LLMuteList::AR_INVENTORY);

	// If this item was given by drag-and-drop into an IM panel, log this
	// action in the IM panel chat.
	if (im_session_id.notNull() && gIMMgrp)
	{
		LLSD args;
		gIMMgrp->addSystemMessage(im_session_id, "inventory_item_offered",
								  args);
	}
}

//static
void LLToolDragAndDrop::giveInventoryCategory(const LLUUID& to_agent,
											  LLInventoryCategory* catp,
											  const LLUUID& im_session_id)
{
	if (!catp || !isAgentAvatarValid())
	{
		return;
	}
//MK
	if (gRLenabled &&
		gRLInterface.containsWithoutException("share", to_agent.asString()))
	{
		gNotifications.add("CannotGiveItem");
		return;
	}
//mk

	const LLUUID& cat_id = catp->getUUID();

	llinfos << "Giving inventory folder " << cat_id << " to agent "
			<< to_agent << llendl;

	// Test out how many items are being given.
	LLViewerInventoryCategory::cat_array_t cats;
	LLViewerInventoryItem::item_array_t items;
	LLGiveable giveable;
	gInventory.collectDescendentsIf(cat_id, cats, items,
									LLInventoryModel::EXCLUDE_TRASH, giveable);
	S32 count = cats.size();
	bool complete = true;
	for (S32 i = 0; i < count; ++i)
	{
		if (!gInventory.isCategoryComplete(cats[i]->getUUID()))
		{
			complete = false;
			break;
		}
	}
	if (!complete)
	{
		gNotifications.add("IncompleteInventory");
		return;
	}

 	count = items.size() + cats.size();
 	if (count > MAX_ITEMS)
  	{
		gNotifications.add("TooManyItems");
  		return;
  	}
 	if (count == 0)
  	{
		gNotifications.add("NoItems");
  		return;
  	}

	if (giveable.countNoCopy() == 0)
	{
		commitGiveInventoryCategory(to_agent, catp, im_session_id);
	}
	else
	{
		LLSD args;
		args["COUNT"] = llformat("%d",giveable.countNoCopy());
		LLSD payload;
		payload["agent_id"] = to_agent;
		payload["folder_id"] = cat_id;
		gNotifications.add("CannotCopyCountItems", args, payload,
						   &LLToolDragAndDrop::handleCopyProtectedCategory);
	}
}

//static
bool LLToolDragAndDrop::handleCopyProtectedCategory(const LLSD& notification,
													const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		LLInventoryCategory* cat =
			gInventory.getCategory(notification["payload"]["folder_id"].asUUID());
		if (cat)
		{
			LLUUID agent_id = notification["payload"]["agent_id"].asUUID();
			commitGiveInventoryCategory(agent_id, cat);
			LLViewerInventoryCategory::cat_array_t cats;
			LLViewerInventoryItem::item_array_t items;
			LLUncopyableItems remove;
			gInventory.collectDescendentsIf(cat->getUUID(), cats, items,
											LLInventoryModel::EXCLUDE_TRASH,
											remove);
			for (S32 i = 0, count = items.size(); i < count; ++i)
			{
				gInventory.deleteObject(items[i]->getUUID());
			}
			gInventory.notifyObservers();
		}
		else
		{
			gNotifications.add("CannotGiveCategory");
		}
	}
	else	// No, cancel, whatever, who cares, not yes.
	{
		gNotifications.add("TransactionCancelled");
	}
	return false;
}

//static
void LLToolDragAndDrop::commitGiveInventoryCategory(const LLUUID& to_agent,
													LLInventoryCategory* cat,
													const LLUUID& im_session_id)

{
	if (!cat) return;

	// Test out how many items are being given.
	LLViewerInventoryCategory::cat_array_t cats;
	LLViewerInventoryItem::item_array_t items;
	LLGiveable giveable;
	gInventory.collectDescendentsIf(cat->getUUID(), cats, items,
									LLInventoryModel::EXCLUDE_TRASH,
									giveable);

	// MAX ITEMS is based on (sizeof(uuid)+2) * count must be < MTUBYTES or
	// 18 * count < 1200 => count < 1200/18 => 66. I have cut it down a bit
	// from there to give some pad.
 	S32 count = items.size() + cats.size();
 	if (count > MAX_ITEMS)
  	{
		gNotifications.add("TooManyItems");
  		return;
  	}
 	if (count == 0)
  	{
		gNotifications.add("NoItems");
  		return;
  	}

	llinfos << "Giving inventory folder " << cat->getUUID() << " now."
			<< llendl;

	std::string name;
	gAgent.buildFullname(name);

	LLUUID transaction_id;
	transaction_id.generate();

	S32 bucket_size = (sizeof(U8) + UUID_BYTES) * (count + 1);
	U8* bucket = new U8[bucket_size];
	U8* pos = bucket;
	U8 type = (U8)cat->getType();
	memcpy(pos, &type, sizeof(U8));
	pos += sizeof(U8);
	memcpy(pos, &(cat->getUUID()), UUID_BYTES);
	pos += UUID_BYTES;

	for (S32 i = 0, count = cats.size(); i < count; ++i)
	{
		memcpy(pos, &type, sizeof(U8));
		pos += sizeof(U8);
		memcpy(pos, &(cats[i]->getUUID()), UUID_BYTES);
		pos += UUID_BYTES;
	}

	for (S32 i = 0, count = items.size(); i < count; ++i)
	{
		type = (U8)items[i]->getType();
		memcpy(pos, &type, sizeof(U8));
		pos += sizeof(U8);
		memcpy(pos, &(items[i]->getUUID()), UUID_BYTES);
		pos += UUID_BYTES;
	}

	pack_instant_message(gAgentID, false, gAgentSessionID, to_agent, name,
						 cat->getName(), IM_ONLINE, IM_INVENTORY_OFFERED,
						 transaction_id, 0, LLUUID::null,
						 gAgent.getPositionAgent(), NO_TIMESTAMP, bucket,
						 bucket_size);
	gAgent.sendReliableMessage();
	delete[] bucket;

	// VEFFECT: giveInventoryCategory
	LLHUDEffectSpiral::agentBeamToObject(gObjectList.findObject(to_agent));

	if (gFloaterToolsp)
	{
		gFloaterToolsp->dirty();
	}

	LLMuteList::autoRemove(to_agent, LLMuteList::AR_INVENTORY);

	// If this item was given by drag-and-drop into an IM panel, log this
	// action in the IM panel chat.
	if (im_session_id.notNull() && gIMMgrp)
	{
		LLSD args;
		gIMMgrp->addSystemMessage(im_session_id, "inventory_item_offered",
								  args);
	}
}

//static
bool LLToolDragAndDrop::isInventoryGiveAcceptable(LLInventoryItem* itemp)
{
	if (!isAgentAvatarValid() || !itemp ||
		!itemp->getPermissions().allowTransferBy(gAgentID))
	{
		LL_DEBUGS("DragAndDrop") << "Cannot give away this inventory item"
								 << LL_ENDL;
		return false;
	}

	bool acceptable = true;
	switch (itemp->getType())
	{
		case LLAssetType::AT_OBJECT:
			if (gAgentAvatarp->isWearingAttachment(itemp->getUUID()))
			{
				acceptable = false;
				LL_DEBUGS("DragAndDrop") << "Cannot give away an attached inventory item"
										 << LL_ENDL;
			}
			break;

		case LLAssetType::AT_BODYPART:
		case LLAssetType::AT_CLOTHING:
			if (!itemp->getPermissions().allowCopyBy(gAgentID) &&
				gAgentWearables.isWearingItem(itemp->getUUID()))
			{
				acceptable = false;
				LL_DEBUGS("DragAndDrop") << "Cannot give away a worn inventory item"
										 << LL_ENDL;
			}
			break;

		default:
			break;
	}
	return acceptable;
}

//static
bool LLToolDragAndDrop::isInventoryGroupGiveAcceptable(LLInventoryItem* itemp)
{
	if (!itemp || !isAgentAvatarValid()) return false;

	const LLPermissions& perms = itemp->getPermissions();
	if (!perms.allowTransferBy(gAgentID) || !perms.allowCopyBy(gAgentID))
	{
		LL_DEBUGS("DragAndDrop") << "Cannot give away this inventory item: insufficient permissions."
								 << LL_ENDL;
		return false;
	}

	if (itemp->getType() == LLAssetType::AT_OBJECT &&
		gAgentAvatarp->isWearingAttachment(itemp->getUUID()))
	{
		LL_DEBUGS("DragAndDrop") << "Cannot give away an attached inventory item"
								 << LL_ENDL;
		return false;
	}

	return true;
}

// Accessor that looks at permissions, copyability, and names of inventory
// items to determine if a drop would be ok.
EAcceptance LLToolDragAndDrop::willObjectAcceptInventory(LLViewerObject* objp,
														 LLInventoryItem* itemp,
														 EDragAndDropType type)
{
	// Check the basics
	if (!itemp || !objp) return ACCEPT_NO;

//MK
	if (gRLenabled)
	{
		// Cannot edit objects that someone is sitting on, when prevented from
		// sit-tping
		if ((gRLInterface.mSittpMax < EXTREMUM ||
			 gRLInterface.mContainsUnsit) &&
			objp->isAgentSeat())
		{
			LL_DEBUGS("DragAndDrop") << "Object is a seat and sit is RLV locked; drop refused."
									 << LL_ENDL;
			return ACCEPT_NO_LOCKED;
		}

		if (!gRLInterface.canDetach(objp))
		{
			LL_DEBUGS("DragAndDrop") << "Attachment is RLV locked; drop refused."
									 << LL_ENDL;
			return ACCEPT_NO_LOCKED;
		}

		// If the origin folder is locked, do not allow to drop an item from
		// it into the inventory of an object because then the user could
		// get back the item from that object and place it into a non-locked
		// inventory folder to wear it, bypassing the lock.
		const LLUUID& parent_id = itemp->getParentUUID();
		if (gRLInterface.isFolderLocked(gInventory.getCategory(parent_id)))
		{
			LL_DEBUGS("DragAndDrop") << "Inventory folder is RLV locked; drop refused."
									 << LL_ENDL;
			return ACCEPT_NO_LOCKED;
		}
	}
//mk

	// *HACK: down-cast
	LLViewerInventoryItem* vitemp = (LLViewerInventoryItem*)itemp;
	if (type != DAD_CATEGORY && !vitemp->isFinished())
	{
		// Note: for DAD_CATEGORY we assume that folder version check passed
		// and folder is complete, meaning that items inside are up to date.
		// isFinished() is false at the moment shows that item was loaded from
		// cache. Library or agent inventory only.
		LL_DEBUGS("DragAndDrop") << "Inventory item not yet fully loaded, refusing drop for now."
								 << LL_ENDL;
		return ACCEPT_NO;
	}

	if (vitemp->getIsLinkType())
	{
		// Never give away links
		LL_DEBUGS("DragAndDrop") << "Cannot give away an inventory link"
								 << LL_ENDL;
		return ACCEPT_NO;
	}

	// Deny attempts to drop from an object onto itself. This is to help and
	// make sure that drops that are from an object to an object do not have to
	// worry about order of evaluation. Think of this like check for self in
	// assignment.
	if (objp->getID() == itemp->getParentUUID())
	{
		LL_DEBUGS("DragAndDrop") << "Cannot drop object onto itself"
								 << LL_ENDL;
		return ACCEPT_NO;
	}

	bool worn = false;
	switch (itemp->getType())
	{
		case LLAssetType::AT_OBJECT:
			if (isAgentAvatarValid() &&
				gAgentAvatarp->isWearingAttachment(itemp->getUUID()))
			{
				worn = true;
			}
			break;

		case LLAssetType::AT_BODYPART:
		case LLAssetType::AT_CLOTHING:
			if (gAgentWearables.isWearingItem(itemp->getUUID()))
			{
				worn = true;
			}
			break;

		case LLAssetType::AT_CALLINGCARD:
			// Calling cards in object are disabled for now because of
			// incomplete LSL support. See STORM-1117.
			LL_DEBUGS("DragAndDrop") << "Calling cards cannot be dropped in objects"
									 << LL_ENDL;
			return ACCEPT_NO;

		default:
			break;
	}

	const LLPermissions& perm = itemp->getPermissions();

	// If attached and not full perm, cannot accept.
	if (objp->isAttachment() &&	!perm.unrestricted())
	{
		return ACCEPT_NO_LOCKED;
	}

	bool modify = objp->permModify() || objp->flagAllowInventoryAdd();

	bool transfer = false;
	if ((objp->permYouOwner() && perm.getOwner() == gAgentID) ||
		perm.allowTransferBy(gAgentID))
	{
		transfer = true;
	}

	bool volume = objp->getPCode() == LL_PCODE_VOLUME;

	if (modify && transfer && volume && !worn)
	{
		return ACCEPT_YES_MULTI;
	}
	else if (!modify)
	{
		LL_DEBUGS("DragAndDrop") << "Object is no mod and does not allow inventory add"
								 << LL_ENDL;
		return ACCEPT_NO_LOCKED;
	}

	LL_DEBUGS("DragAndDrop") << " - worn: " << (worn ? "true": "false")
							 << " - mod/add permission: "
							 << (modify ? "true": "false")
							 << " - transfer permission: "
							 << (transfer ? "true": "false")
							 << " - Drop action refused." << LL_ENDL;
	return ACCEPT_NO;
}

// Method used as drag-and-drop handler for simple agent give inventory
// requests
//static
bool LLToolDragAndDrop::handleGiveDragAndDrop(const LLUUID& dest_agent,
											  const LLUUID& session_id,
											  bool drop,
											  EDragAndDropType cargo_type,
											  void* cargo_data,
											  EAcceptance* accept)
{
//MK
	if (gRLenabled &&
		gRLInterface.containsWithoutException("share", dest_agent.asString()))
	{
		*accept = ACCEPT_NO;
		return true;
	}
//mk
	if ((cargo_type == DAD_SETTINGS && !gAgent.hasInventorySettings()) ||
		(cargo_type == DAD_MATERIAL && !gAgent.hasInventoryMaterial()))
	{
		return false;
	}

	// Check the type
	switch (cargo_type)
	{
		case DAD_TEXTURE:
		case DAD_SOUND:
		case DAD_LANDMARK:
		case DAD_SCRIPT:
		case DAD_OBJECT:
		case DAD_NOTECARD:
		case DAD_CLOTHING:
		case DAD_BODYPART:
		case DAD_ANIMATION:
		case DAD_GESTURE:
		case DAD_CALLINGCARD:
#if LL_MESH_ASSET_SUPPORT
		case DAD_MESH:
#endif
		case DAD_SETTINGS:
		case DAD_MATERIAL:
		{
			LLInventoryObject* inv_objp = (LLInventoryObject*)cargo_data;
			LLInventoryItem* inv_item = inv_objp->asInventoryItem();
			if (inv_item && gInventory.getItem(inv_objp->getUUID()) &&
				isInventoryGiveAcceptable(inv_item))
			{
				// *TODO: get multiple object transfers working
				*accept = ACCEPT_YES_COPY_SINGLE;
				if (drop)
				{
					giveInventory(dest_agent, inv_item, session_id);
				}
			}
			else
			{
				// It is not in the user's inventory (it is probably contained
				// in an object), so disallow dragging it here. You cannot give
				// something you do not yet have.
				*accept = ACCEPT_NO;
				LL_DEBUGS("DragAndDrop") << "Item is not in user inventory. Refusing."
										 << LL_ENDL;
			}
			break;
		}

		case DAD_CATEGORY:
		{
			LLViewerInventoryCategory* inv_cat =
				(LLViewerInventoryCategory*)cargo_data;
			if (gInventory.getCategory(inv_cat->getUUID()))
			{
				// *TODO: get multiple object transfers working
				*accept = ACCEPT_YES_COPY_SINGLE;
				if (drop)
				{
					giveInventoryCategory(dest_agent, inv_cat, session_id);
				}
			}
			else
			{
				// It is not in the user's inventory (it is probably contained
				// in an object), so disallow dragging it here. You cannot give
				// something you do not yet have.
				*accept = ACCEPT_NO;
				LL_DEBUGS("DragAndDrop") << "Folder is not in user inventory. Refusing."
										 << LL_ENDL;
			}
			break;
		}

		default:
			*accept = ACCEPT_NO;
			LL_DEBUGS("DragAndDrop") << "Cannot give this type of inventory item."
									 << LL_ENDL;
	}

	return true;
}

//
// Methods called in the drag & drop array
//

EAcceptance LLToolDragAndDrop::dad3dNULL(LLViewerObject*, S32, MASK, bool)
{
	LL_DEBUGS("DragAndDrop") << "No operation" << LL_ENDL;
	return ACCEPT_NO;
}

EAcceptance LLToolDragAndDrop::dad3dRezAttachmentFromInv(LLViewerObject* objp,
														 S32 face, MASK mask,
														 bool drop)
{
//MK
	if (gRLenabled && gRLInterface.mContainsDetach)
	{
		LL_DEBUGS("DragAndDrop") << "Attachment is RLV locked. Refusing."
								 << LL_ENDL;
		return ACCEPT_NO;
	}
//mk
	// Must be in the user's inventory
	if (mSource != SOURCE_AGENT && mSource != SOURCE_LIBRARY)
	{
		LL_DEBUGS("DragAndDrop") << "Not in user inventory. Refusing."
								 << LL_ENDL;
		return ACCEPT_NO;
	}

	LLViewerInventoryItem* itemp;
	LLViewerInventoryCategory* catp;
	locateInventory(itemp, catp);
	if (!itemp || !itemp->isFinished())
	{
		LL_DEBUGS("DragAndDrop") << "Unfinished inventory item. Refusing for now."
								 << LL_ENDL;
		return ACCEPT_NO;
	}

	const LLUUID& item_id = itemp->getUUID();

	// Must not be in the trash
	if (gInventory.isInTrash(item_id))
	{
		LL_DEBUGS("DragAndDrop") << "Inventory item is in Trash. Refusing."
								 << LL_ENDL;
		return ACCEPT_NO;
	}

	// Must not be already wearing it
	if (!isAgentAvatarValid() || gAgentAvatarp->isWearingAttachment(item_id))
	{
		LL_DEBUGS("DragAndDrop") << "Cannot give a worn inventory item."
								 << LL_ENDL;
		return ACCEPT_NO;
	}

	if (drop)
	{
		if (mSource == SOURCE_LIBRARY)
		{
			LLPointer<LLInventoryCallback> cb =
				new LLRezAttachmentCallback(NULL);
			copy_inventory_item(itemp->getPermissions().getOwner(),
								item_id, LLUUID::null, std::string(), cb);
		}
		else
		{
			gAppearanceMgr.rezAttachment(itemp, 0);
		}
	}

	return ACCEPT_YES_SINGLE;
}

EAcceptance LLToolDragAndDrop::dad3dRezObjectOnLand(LLViewerObject* objp,
													S32 face, MASK mask,
													bool drop)
{
//MK
	if (gRLenabled &&
		(gRLInterface.mContainsRez || gRLInterface.mContainsInteract))
	{
		LL_DEBUGS("DragAndDrop") << "Rezzing is forbidden by RLV. Refusing."
								 << LL_ENDL;
		return ACCEPT_NO_LOCKED;
	}
//mk

	if (mSource == SOURCE_WORLD)
	{
		return dad3dRezFromObjectOnLand(objp, face, mask, drop);
	}

	LLViewerInventoryItem* itemp;
	LLViewerInventoryCategory* catp;
	locateInventory(itemp, catp);
	if (!itemp || !itemp->isFinished()) return ACCEPT_NO;

	const LLUUID& item_id = itemp->getUUID();

	if (!isAgentAvatarValid() || gAgentAvatarp->isWearingAttachment(item_id))
	{
		LL_DEBUGS("DragAndDrop") << "Cannot drop attached inventory item."
								 << LL_ENDL;
		return ACCEPT_NO;
	}

#if 1	// For now, always make copy
	EAcceptance accept = ACCEPT_YES_COPY_SINGLE;
	bool remove_inventory = false;
#else
	EAcceptance accept;
	bool remove_inventory;
	// Get initial settings based on shift key
	if (mask & MASK_SHIFT)
	{
		accept = ACCEPT_YES_SINGLE;
		remove_inventory = true;
	}
	else
	{
		accept = ACCEPT_YES_COPY_SINGLE;
		remove_inventory = false;
	}
#endif

	// Check if the item can be copied. If not, send that to the sim which will
	// remove the inventory item.
	if (!itemp->getPermissions().allowCopyBy(gAgentID))
	{
		accept = ACCEPT_YES_SINGLE;
		remove_inventory = true;
	}

	// Check if it is in the trash.
	if (gInventory.isInTrash(item_id))
	{
		accept = ACCEPT_YES_SINGLE;
		remove_inventory = true;
	}

	if (drop)
	{
		dropObject(objp, true, false, remove_inventory);
	}

	return accept;
}

EAcceptance LLToolDragAndDrop::dad3dRezObjectOnObject(LLViewerObject* objp,
													  S32 face, MASK mask,
													  bool drop)
{
//MK
	if (gRLenabled &&
		(gRLInterface.mContainsRez || gRLInterface.mContainsInteract))
	{
		LL_DEBUGS("DragAndDrop") << "Rezzing is forbidden by RLV. Refusing."
								 << LL_ENDL;
		return ACCEPT_NO_LOCKED;
	}
//mk

	// handle objects coming from object inventory
	if (mSource == SOURCE_WORLD)
	{
		return dad3dRezFromObjectOnObject(objp, face, mask, drop);
	}

	LLViewerInventoryItem* itemp;
	LLViewerInventoryCategory* catp;
	locateInventory(itemp, catp);
	if (!itemp || !itemp->isFinished())
	{
		LL_DEBUGS("DragAndDrop") << "Unfinished inventory item. Refusing for now."
								 << LL_ENDL;
		return ACCEPT_NO;
	}

	const LLUUID& item_id = itemp->getUUID();

	if (!isAgentAvatarValid() || gAgentAvatarp->isWearingAttachment(item_id))
	{
		LL_DEBUGS("DragAndDrop") << "Cannot drop attached inventory item."
								 << LL_ENDL;
		return ACCEPT_NO;
	}

	if ((mask & MASK_CONTROL))
	{
		// *HACK: In order to resolve SL-22177, we need to block drags from
		// notecards and objects onto other objects.
		if (mSource == SOURCE_NOTECARD)
		{
			LL_DEBUGS("DragAndDrop") << "Cannot drop notecards into objects."
									 << LL_ENDL;
			return ACCEPT_NO;
		}

		EAcceptance rv = willObjectAcceptInventory(objp, itemp);
		if (drop && (ACCEPT_YES_SINGLE <= rv))
		{
			dropInventory(objp, itemp, mSource, mSourceID);
		}
		return rv;
	}

#if 1	// For now, always make copy
	EAcceptance accept = ACCEPT_YES_COPY_SINGLE;
	bool remove_inventory = false;
#else
	EAcceptance accept;
	bool remove_inventory;
	// Get initial settings based on shift key
	if (mask & MASK_SHIFT)
	{
		accept = ACCEPT_YES_SINGLE;
		remove_inventory = true;
	}
	else
	{
		accept = ACCEPT_YES_COPY_SINGLE;
		remove_inventory = false;
	}
#endif

	// Check if the item can be copied. If not, send that to the sim which will
	// remove the inventory item.
	if (!itemp->getPermissions().allowCopyBy(gAgentID))
	{
		accept = ACCEPT_YES_SINGLE;
		remove_inventory = true;
	}

	// Check if it is in the trash.
	if (gInventory.isInTrash(item_id))
	{
		accept = ACCEPT_YES_SINGLE;
	}

	if (drop)
	{
		dropObject(objp, false, false, remove_inventory);
	}

	return accept;
}

EAcceptance LLToolDragAndDrop::dad3dRezCategoryOnObject(LLViewerObject* obj,
														S32 face, MASK mask,
														bool drop)
{
	if (mask & MASK_CONTROL)
	{
		return dad3dUpdateInventoryCategory(obj, face, mask, drop);
	}
	else
	{
		return ACCEPT_NO;
	}
}

EAcceptance LLToolDragAndDrop::dad3dRezScript(LLViewerObject* objp, S32 face,
											  MASK mask, bool drop)
{
	// *HACK: In order to resolve SL-22177, we need to block drags from
	// notecards and objects onto other objects.
	if (mSource == SOURCE_WORLD || mSource == SOURCE_NOTECARD)
	{
		LL_DEBUGS("DragAndDrop") << "Cannot drop script from this source."
								 << LL_ENDL;
		return ACCEPT_NO;
	}

	LLViewerInventoryItem* itemp;
	LLViewerInventoryCategory* catp;
	locateInventory(itemp, catp);
	if (!itemp || !itemp->isFinished())
	{
		LL_DEBUGS("DragAndDrop") << "Unfinished inventory item. Refusing for now."
								 << LL_ENDL;
		return ACCEPT_NO;
	}

	EAcceptance rv = willObjectAcceptInventory(objp, itemp);
	if (drop && rv >= ACCEPT_YES_SINGLE)
	{
		LLViewerObject* root_objectp = objp;
		if (objp && objp->getParent())
		{
			LLViewerObject* parent_objp = (LLViewerObject*)objp->getParent();
			if (!parent_objp->isAvatar())
			{
				root_objectp = parent_objp;
			}
		}

		// Rez in the script active by default, rez in inactive if the
		// control key is being held down.
		bool active = (mask & MASK_CONTROL) == 0;
		dropScript(root_objectp, itemp, active, mSource, mSourceID);
	}
	return rv;
}

EAcceptance LLToolDragAndDrop::dad3dApplyToObject(LLViewerObject* objp,
												  S32 face, MASK mask,
												  bool drop,
												  EDragAndDropType cargo_type)
{
	// *HACK: In order to resolve SL-22177, we need to block drags from
	// notecards and objects onto other objects.
	if (mSource == SOURCE_WORLD || mSource == SOURCE_NOTECARD)
	{
		LL_DEBUGS("DragAndDrop") << "Cannot drop script from this source."
								 << LL_ENDL;
		return ACCEPT_NO;
	}

	LLViewerInventoryItem* itemp;
	LLViewerInventoryCategory* catp;
	locateInventory(itemp, catp);
	if (!itemp || !itemp->isFinished())
	{
		LL_DEBUGS("DragAndDrop") << "Unfinished inventory item. Refusing for now."
								 << LL_ENDL;
		return ACCEPT_NO;
	}

	EAcceptance rv = willObjectAcceptInventory(objp, itemp);
	if ((mask & MASK_CONTROL))
	{
		if (drop && rv >= ACCEPT_YES_SINGLE)
		{
			dropInventory(objp, itemp, mSource, mSourceID);
		}
		return rv;
	}

	if (!objp->permModify())
	{
		LL_DEBUGS("DragAndDrop") << "Object is no-modify, cannot apply item to it."
								 << LL_ENDL;
		return ACCEPT_NO_LOCKED;
	}

	if (!itemp->getPermissions().allowCopyBy(gAgentID))
	{
		LL_DEBUGS("DragAndDrop") << "Inventory item is not copyable, cannot apply to object."
								 << LL_ENDL;
		return ACCEPT_NO;
	}

	U8 num_tes = objp->getNumTEs();

	if (cargo_type == DAD_TEXTURE)
	{
		if ((mask & MASK_SHIFT))
		{
			for (U8 te = 0; te < num_tes; ++te)
			{
				if (objp->getRenderMaterialID(te).notNull())
				{
					return ACCEPT_NO;	// Got a PBR face: do not ruin it.
				}
			}
		}
		else if (objp->getRenderMaterialID(face).notNull())
		{
			return ACCEPT_NO;	// This is a PBR face: do not ruin it.
		}
	}

	if (drop && rv >= ACCEPT_YES_SINGLE)
	{
		if (cargo_type == DAD_TEXTURE)
		{
			// Get ready to save textures of any selected node.
			LLSelectNode* nodep = NULL;
			if (objp->isSelected())
			{
				 nodep = gSelectMgr.getSelection()->findNode(objp);
			}

			if ((mask & MASK_SHIFT))
			{
				dropTextureAllFaces(objp, itemp, mSource, mSourceID);

				// If the user dropped a texture onto a face, it implies
				// applying texture now without cancel, save to selection.
				if (nodep)
				{
					uuid_vec_t tids;
					for (U8 te = 0; te < num_tes; ++te)
					{
						LLViewerTexture* texp = objp->getTEImage(te);
						tids.emplace_back(texp ? texp->getID() : LLUUID::null);
					}
					nodep->saveTextures(tids);
				}
			}
			else
			{
				dropTextureOneFace(objp, face, itemp, mSource, mSourceID);

				// If the user dropped a texture onto a face, it implies
				// applying texture now without cancel, save to selection.
				if (nodep && LLFloaterTools::isVisible() &&
					// If the diffuse channel is being selected only
					gSelectMgr.getTextureChannel() == 0 &&
					(S32)nodep->mSavedGLTFMaterialIds.size() > face)
				{
					LLViewerTexture* texp = objp->getTEImage(face);
					nodep->mSavedTextures[face] = texp ? texp->getID()
													   : LLUUID::null;
				}
			}
		}
		else if (cargo_type == DAD_MATERIAL)
		{
			// Get ready to save textures of any selected node.
			LLSelectNode* nodep = NULL;
			if (objp->isSelected())
			{
				 nodep = gSelectMgr.getSelection()->findNode(objp);
			}

			if ((mask & MASK_SHIFT))
			{
				dropMaterialAllFaces(objp, itemp, mSource, mSourceID);

				// If the user dropped a texture onto a face, it implies
				// applying texture now without cancel, save to selection.
				if (nodep)
				{
					uuid_vec_t matids;
					gltf_mat_vec_t mats;
					for (U8 te = 0; te < num_tes; ++te)
					{
						matids.emplace_back(objp->getRenderMaterialID(te));
						mats.emplace_back(nullptr);
					}
					nodep->saveGLTFMaterials(matids, mats);
				}
			}
			else
			{
				dropMaterialOneFace(objp, face, itemp, mSource, mSourceID);

				// If the user dropped a texture onto a face, it implies
				// applying texture now without cancel, save to selection.
				if (nodep && LLFloaterTools::isVisible() &&
					(S32)nodep->mSavedGLTFMaterialIds.size() > face)
				{
					nodep->mSavedGLTFMaterialIds[face] =
						objp->getRenderMaterialID(face);
					nodep->mSavedGLTFOverrideMaterials[face] = nullptr;
				}
			}
		}
#if LL_MESH_ASSET_SUPPORT
		else if (cargo_type == DAD_MESH)
		{
			dropMesh(objp, itemp, mSource, mSourceID);
		}
#endif
		else
		{
			llwarns << "Unsupported asset type" << llendl;
		}

		// VEFFECT: SetTexture
		LLHUDEffectSpiral::agentBeamToObject(objp);
	}

	// Enable multi-drop, although last texture will win
	return ACCEPT_YES_MULTI;
}

EAcceptance LLToolDragAndDrop::dad3dTextureObject(LLViewerObject* objp,
												  S32 face, MASK mask,
												  bool drop)
{
	return dad3dApplyToObject(objp, face, mask, drop, DAD_TEXTURE);
}

EAcceptance LLToolDragAndDrop::dad3dMaterialObject(LLViewerObject* objp,
												   S32 face, MASK mask,
												   bool drop)
{
	return dad3dApplyToObject(objp, face, mask, drop, DAD_MATERIAL);
}

#if LL_MESH_ASSET_SUPPORT
EAcceptance LLToolDragAndDrop::dad3dMeshObject(LLViewerObject* objp, S32 face,
											   MASK mask, bool drop)
{
	return dad3dApplyToObject(objp, face, mask, drop, DAD_MESH);
}
#endif

EAcceptance LLToolDragAndDrop::dad3dWearItem(LLViewerObject* objp, S32 face,
											 MASK mask, bool drop)
{
	LLViewerInventoryItem* itemp;
	LLViewerInventoryCategory* catp;
	locateInventory(itemp, catp);
	if (!itemp || !itemp->isFinished())
	{
		LL_DEBUGS("DragAndDrop") << "Unfinished inventory item. Refusing for now."
								 << LL_ENDL;
		return ACCEPT_NO;
	}

	if (mSource == SOURCE_AGENT || mSource == SOURCE_LIBRARY)
	{
		// Is it in the agent inventory ?
		if (gInventory.isInTrash(itemp->getUUID()))
		{
			LL_DEBUGS("DragAndDrop") << "Inventory item is not in agent inventory. Refused."
									 << LL_ENDL;
			return ACCEPT_NO;
		}

		if (drop)
		{
			// Do not wear anything until initial wearables are loaded, can
			// destroy clothing items.
			if (!gAgentWearables.areWearablesLoaded())
			{
				gNotifications.add("CanNotChangeAppearanceUntilLoaded");
				LL_DEBUGS("DragAndDrop") << "Agent not fully rezzed. Refused for now."
										 << LL_ENDL;
				return ACCEPT_NO;
			}

			gAppearanceMgr.wearItemOnAvatar(itemp->getUUID(), false);
		}
		return ACCEPT_YES_MULTI;
	}

	// *TODO: copy/move item to avatar's inventory and then wear it.
	LL_DEBUGS("DragAndDrop") << "Invalid source. Refused." << LL_ENDL;
	return ACCEPT_NO;
}

EAcceptance LLToolDragAndDrop::dad3dActivateGesture(LLViewerObject* objp,
													S32 face, MASK mask,
													bool drop)
{
	LLViewerInventoryItem* itemp;
	LLViewerInventoryCategory* catp;
	locateInventory(itemp, catp);
	if (!itemp || !itemp->isFinished())
	{
		LL_DEBUGS("DragAndDrop") << "Unfinished inventory item. Refusing for now."
								 << LL_ENDL;
		return ACCEPT_NO;
	}

	const LLUUID& item_id = itemp->getUUID();

	if (mSource == SOURCE_AGENT || mSource == SOURCE_LIBRARY)
	{
		// It is in the agent inventory
		if (gInventory.isInTrash(item_id))
		{
			LL_DEBUGS("DragAndDrop") << "Inventory item is not in agent inventory. Refused."
									 << LL_ENDL;
			return ACCEPT_NO;
		}

		if (drop)
		{
			if (mSource == SOURCE_LIBRARY)
			{
				// Create item based on that one, and put it on if that was a
				// success.
				LLPointer<LLInventoryCallback> cb =
					new ActivateGestureCallback();
				copy_inventory_item(itemp->getPermissions().getOwner(),
									item_id, LLUUID::null, std::string(), cb);
			}
			else
			{
				gGestureManager.activateGesture(item_id);
				gInventory.updateItem(itemp);
				gInventory.notifyObservers();
			}
		}
		return ACCEPT_YES_MULTI;
	}

	LL_DEBUGS("DragAndDrop") << "Invalid source. Refused." << LL_ENDL;
	return ACCEPT_NO;
}

EAcceptance LLToolDragAndDrop::dad3dWearCategory(LLViewerObject* objp,
												 S32 face, MASK mask,
												 bool drop)
{
	LLViewerInventoryItem* itemp;
	LLViewerInventoryCategory* catp;
	locateInventory(itemp, catp);
	if (!catp)
	{
		return ACCEPT_NO;
	}

//MK
	if (gRLenabled && (gRLInterface.mContainsDetach ||
		gRLInterface.contains("addoutfit") ||
		gRLInterface.contains("remoutfit")))
	{
		LL_DEBUGS("DragAndDrop") << "Outfit is RLV locked. Refused" << LL_ENDL;
		return ACCEPT_NO;
	}
//mk
	if (drop)
	{
		// Do not wear anything until initial wearables are loaded; can destroy
		// clothing items.
		if (!gAgentWearables.areWearablesLoaded())
		{
			gNotifications.add("CanNotChangeAppearanceUntilLoaded");
			return ACCEPT_NO;
		}
	}

	if (mSource == SOURCE_AGENT)
	{
		if (gInventory.isInTrash(catp->getUUID()))
		{
			LL_DEBUGS("DragAndDrop") << "Item is in Trash. Refused" << LL_ENDL;
			return ACCEPT_NO;
		}

		if (drop)
		{
		    bool append = (mask & MASK_SHIFT);
			gAppearanceMgr.wearInventoryCategory(catp, false, append);
		}
		return ACCEPT_YES_MULTI;
	}

	if (mSource == SOURCE_LIBRARY)
	{
		if (drop)
		{
			gAppearanceMgr.wearInventoryCategory(catp, true, false);
		}
		return ACCEPT_YES_MULTI;
	}

	// *TODO: copy/move category to avatar's inventory and then wear it.
	LL_DEBUGS("DragAndDrop") << "Invalid source. Refused." << LL_ENDL;
	return ACCEPT_NO;
}

EAcceptance LLToolDragAndDrop::dad3dUpdateInventory(LLViewerObject* objp,
													S32 face, MASK mask,
													bool drop)
{
	// *HACK: In order to resolve SL-22177, we need to block drags from
	// notecards and objects onto other objects.
	if (mSource == SOURCE_WORLD || mSource == SOURCE_NOTECARD)
	{
		LL_DEBUGS("DragAndDrop") << "Invalid source. Refused." << LL_ENDL;
		return ACCEPT_NO;
	}

	LLViewerInventoryItem* itemp;
	LLViewerInventoryCategory* catp;
	locateInventory(itemp, catp);
	if (!itemp || !itemp->isFinished())
	{
		LL_DEBUGS("DragAndDrop") << "Unfinished inventory item. Refusing for now."
								 << LL_ENDL;
		return ACCEPT_NO;
	}

	LLViewerObject* root_objectp = objp;
	if (objp && objp->getParent())
	{
		LLViewerObject* parent_objp = (LLViewerObject*)objp->getParent();
		if (!parent_objp->isAvatar())
		{
			root_objectp = parent_objp;
		}
	}

	EAcceptance rv = willObjectAcceptInventory(root_objectp, itemp);
	if (root_objectp && drop && (ACCEPT_YES_COPY_SINGLE <= rv))
	{
		dropInventory(root_objectp, itemp, mSource, mSourceID);
	}
	return rv;
}

bool LLToolDragAndDrop::dadUpdateInventory(LLViewerObject* objp, bool drop)
{
	EAcceptance rv = dad3dUpdateInventory(objp, -1, MASK_NONE, drop);
	return rv >= ACCEPT_YES_COPY_SINGLE;
}

EAcceptance LLToolDragAndDrop::dad3dUpdateInventoryCategory(LLViewerObject* objp,
															S32 face,
															MASK mask,
															bool drop)
{
	if (!objp)
	{
		llwarns << "NULL object pointer; aborting func with ACCEPT_NO"
				<< llendl;
		return ACCEPT_NO;
	}

	if (mSource != SOURCE_AGENT && mSource != SOURCE_LIBRARY)
	{
		LL_DEBUGS("DragAndDrop") << "Invalid source. Refused." << LL_ENDL;
		return ACCEPT_NO;
	}

	if (objp->isAttachment())
	{
		LL_DEBUGS("DragAndDrop") << "Cannot apply to attachments." << LL_ENDL;
		return ACCEPT_NO_LOCKED;
	}

	LLViewerInventoryItem* itemp;
	LLViewerInventoryCategory* catp;
	locateInventory(itemp, catp);
	if (!catp)
	{
		LL_DEBUGS("DragAndDrop") << "Category not found. Refused." << LL_ENDL;
		return ACCEPT_NO;
	}

	// Find all the items in the category
	LLDroppableItem droppable(!objp->permYouOwner());
	LLInventoryModel::cat_array_t cats;
	LLInventoryModel::item_array_t items;
	gInventory.collectDescendentsIf(catp->getUUID(), cats, items,
									LLInventoryModel::EXCLUDE_TRASH,
									droppable);
	cats.push_back(catp);
 	if (droppable.countNoCopy() > 0)
 	{
 		llwarns << "*** Need to confirm this step" << llendl;
 	}
	LLViewerObject* root_objectp = objp;
	if (objp->getParent())
	{
		LLViewerObject* parent_objp = (LLViewerObject*)objp->getParent();
		if (!parent_objp->isAvatar())
		{
			root_objectp = parent_objp;
		}
	}

	EAcceptance rv = ACCEPT_NO;

	// Check for accept
	for (S32 i = 0, count = cats.size(); i < count; ++i)
	{
		rv = gInventory.isCategoryComplete(cats[i]->getUUID()) ? ACCEPT_YES_MULTI
															   : ACCEPT_NO;
		if (rv < ACCEPT_YES_SINGLE)
		{
			LL_DEBUGS("DragAndDrop") << "Category " << cats[i]->getUUID()
									 << "is not complete." << LL_ENDL;
			break;
		}
	}
	if (ACCEPT_YES_COPY_SINGLE <= rv)
	{
		for (S32 i = 0, count = items.size(); i < count; ++i)
		{
			LLViewerInventoryItem* itemp = items[i];
#if 0
			// Pass the base objects, not the links.
			if (itemp && itemp->getIsLinkType())
			{
				itemp = itemp->getLinkedItem();
				items[i] = itemp;
			}
#endif
			rv = willObjectAcceptInventory(root_objectp, itemp, DAD_CATEGORY);
			if (rv < ACCEPT_YES_COPY_SINGLE)
			{
				LL_DEBUGS("DragAndDrop") << "Object will not accept "
										 << itemp->getUUID() << LL_ENDL;
				break;
			}
		}
	}

	// If every item is accepted, send it on.
	if (drop && ACCEPT_YES_COPY_SINGLE <= rv)
	{
		uuid_vec_t ids;
		for (S32 i = 0, count = items.size(); i < count; ++i)
		{
			ids.emplace_back(items[i]->getUUID());
		}
		LLCategoryDropObserver* dropperp =
			new LLCategoryDropObserver(objp->getID(), mSource);
		dropperp->fetchItems(ids);
		if (dropperp->isFinished())
		{
			dropperp->done();
		}
		else
		{
			gInventory.addObserver(dropperp);
		}
	}
	return rv;
}

bool LLToolDragAndDrop::dadUpdateInventoryCategory(LLViewerObject* objp,
												   bool drop)
{
	EAcceptance rv = dad3dUpdateInventoryCategory(objp, -1, MASK_NONE, drop);
	return rv >= ACCEPT_YES_COPY_SINGLE;
}

EAcceptance LLToolDragAndDrop::dad3dGiveInventoryObject(LLViewerObject* objp,
														S32 face, MASK mask,
														bool drop)
{
	// Item has to be in agent inventory.
	if (mSource != SOURCE_AGENT) return ACCEPT_NO;
//MK
	if (gRLenabled &&
		(gRLInterface.mContainsShownames ||
		 gRLInterface.mContainsShownametags))
	{
		// To avoid having "so-and-so accepted/declined your inventory offer."
		// messages
		LL_DEBUGS("DragAndDrop") << "Refused under RLV show names restrictions."
								 << LL_ENDL;
		return ACCEPT_NO;
	}
//mk
	// Find the item now.
	LLViewerInventoryItem* itemp;
	LLViewerInventoryCategory* catp;
	locateInventory(itemp, catp);
	if (!itemp || !itemp->isFinished())
	{
		LL_DEBUGS("DragAndDrop") << "Unfinished inventory item. Refusing for now."
								 << LL_ENDL;
		return ACCEPT_NO;
	}

	if (!itemp->getPermissions().allowTransferBy(gAgentID))
	{
		// Cannot give away no-transfer objects
		LL_DEBUGS("DragAndDrop") << "No transfer inventory item. Refused."
								 << LL_ENDL;
		return ACCEPT_NO;
	}

	if (isAgentAvatarValid() &&
		gAgentAvatarp->isWearingAttachment(itemp->getUUID()))
	{
		// You cannot give objects that are attached to you
		LL_DEBUGS("DragAndDrop") << "Attached inventory item. Refused."
								 << LL_ENDL;
		return ACCEPT_NO;
	}

	if (objp && isAgentAvatarValid())
	{
		if (drop)
		{
			giveInventory(objp->getID(), itemp);
		}
		// *TODO: deal with all the issues surrounding multi-object inventory
		// transfers.
		return ACCEPT_YES_SINGLE;
	}

	LL_DEBUGS("DragAndDrop") << "Refused action." << LL_ENDL;
	return ACCEPT_NO;
}

EAcceptance LLToolDragAndDrop::dad3dGiveInventory(LLViewerObject* objp,
												  S32 face, MASK mask,
												  bool drop)
{
	// Item has to be in agent inventory.
	if (mSource != SOURCE_AGENT) return ACCEPT_NO;
//MK
	if (gRLenabled &&
		(gRLInterface.mContainsShownames ||
		 gRLInterface.mContainsShownametags))
	{
		// To avoid having "so-and-so accepted/declined your inventory offer."
		// messages
		LL_DEBUGS("DragAndDrop") << "Unfinished inventory item. Refusing for now."
								 << LL_ENDL;
		return ACCEPT_NO;
	}
//mk
	LLViewerInventoryItem* itemp;
	LLViewerInventoryCategory* catp;
	locateInventory(itemp, catp);
	if (!itemp || !itemp->isFinished())
	{
		LL_DEBUGS("DragAndDrop") << "Unfinished inventory item. Refusing for now."
								 << LL_ENDL;
		return ACCEPT_NO;
	}

	if (!isInventoryGiveAcceptable(itemp))
	{
		return ACCEPT_NO;
	}

	if (drop && objp)
	{
		giveInventory(objp->getID(), itemp);
	}

	// *TODO: deal with all the issues surrounding multi-object inventory
	// transfers.
	return ACCEPT_YES_SINGLE;
}

EAcceptance LLToolDragAndDrop::dad3dGiveInventoryCategory(LLViewerObject* objp,
														  S32 face, MASK mask,
														  bool drop)
{
//MK
	if (gRLenabled &&
		(gRLInterface.mContainsShownames ||
		 gRLInterface.mContainsShownametags))
	{
		// To avoid having "so-and-so accepted/declined your inventory offer."
		// messages
		LL_DEBUGS("DragAndDrop") << "Unfinished inventory item. Refusing for now."
								 << LL_ENDL;
		return ACCEPT_NO;
	}
//mk
	if (drop && objp)
	{
		LLViewerInventoryItem* itemp;
		LLViewerInventoryCategory* catp;
		locateInventory(itemp, catp);
		if (!catp)
		{
			LL_DEBUGS("DragAndDrop") << "Category not found. Refused."
									 << LL_ENDL;
			return ACCEPT_NO;
		}
		giveInventoryCategory(objp->getID(), catp);
	}

	// *TODO: deal with all the issues surrounding multi-object inventory
	// transfers.
	return ACCEPT_YES_SINGLE;
}

EAcceptance LLToolDragAndDrop::dad3dRezFromObjectOnLand(LLViewerObject* objp,
														S32 face, MASK mask,
														bool drop)
{
	LLViewerInventoryItem* itemp;
	LLViewerInventoryCategory* catp;
	locateInventory(itemp, catp);
	if (!itemp || !itemp->isFinished())
	{
		LL_DEBUGS("DragAndDrop") << "Unfinished inventory item. Refusing for now."
								 << LL_ENDL;
		return ACCEPT_NO;
	}

	if (!gAgent.allowOperation(PERM_COPY, itemp->getPermissions()) ||
		!itemp->getPermissions().allowTransferTo(LLUUID::null))
	{
		LL_DEBUGS("DragAndDrop") << "Insufficient permisions for inventory item."
								 << LL_ENDL;
		return ACCEPT_NO_LOCKED;
	}

	if (drop)
	{
		dropObject(objp, true, true, false);
	}

	return ACCEPT_YES_SINGLE;
}

EAcceptance LLToolDragAndDrop::dad3dRezFromObjectOnObject(LLViewerObject* objp,
														  S32 face, MASK mask,
														  bool drop)
{
	LLViewerInventoryItem* itemp;
	LLViewerInventoryCategory* catp;
	locateInventory(itemp, catp);
	if (!itemp || !itemp->isFinished())
	{
		LL_DEBUGS("DragAndDrop") << "Unfinished inventory item. Refusing for now."
								 << LL_ENDL;
		return ACCEPT_NO;
	}

	if ((mask & MASK_CONTROL))
	{
#if 1	// *HACK: in order to resolve SL-22177, we need to block drags from
		// notecards and objects onto other objects.
		LL_DEBUGS("DragAndDrop") << "Cannot drop from this source" << LL_ENDL;
		return ACCEPT_NO;
#else
		EAcceptance rv = willObjectAcceptInventory(objp, itemp);
		if (drop && (ACCEPT_YES_SINGLE <= rv))
		{
			dropInventory(objp, itemp, mSource, mSourceID);
		}
		return rv;
#endif
	}

	if (!itemp->getPermissions().allowCopyBy(gAgentID, gAgent.getGroupID()) ||
		!itemp->getPermissions().allowTransferTo(LLUUID::null))
	{
		LL_DEBUGS("DragAndDrop") << "Insufficient permisions for inventory item."
								 << LL_ENDL;
		return ACCEPT_NO_LOCKED;
	}

	if (drop)
	{
		dropObject(objp, false, true, false);
	}

	return ACCEPT_YES_SINGLE;
}

EAcceptance LLToolDragAndDrop::dad3dCategoryOnLand(LLViewerObject*, S32, MASK,
												   bool)
{
	return ACCEPT_NO;
}

EAcceptance LLToolDragAndDrop::dad3dAssetOnLand(LLViewerObject*, S32, MASK,
												bool)
{
	return ACCEPT_NO;
}

LLInventoryObject* LLToolDragAndDrop::locateInventory(LLViewerInventoryItem*& itemp,
													  LLViewerInventoryCategory*& catp)
{
	itemp = NULL;
	catp = NULL;
	if (mCargoIDs.empty())
	{
		return NULL;
	}

	const LLUUID& cargo_id = mCargoIDs[mCurItemIndex];

	if (mSource == SOURCE_AGENT || mSource == SOURCE_LIBRARY)
	{
		// The object should be in user inventory.
		itemp = (LLViewerInventoryItem*)gInventory.getItem(cargo_id);
		if (itemp) return itemp;

		catp = (LLViewerInventoryCategory*)gInventory.getCategory(cargo_id);
		return catp;
	}

	if (mSource == SOURCE_NOTECARD)
	{
		LLPreviewNotecard* previewp =
			(LLPreviewNotecard*)LLPreview::find(mSourceID);
		if (previewp)
		{
			itemp = (LLViewerInventoryItem*)previewp->getDragItem();
		}
		return itemp;
	}

	if (mSource != SOURCE_WORLD)
	{
		return NULL;
	}

	// This object is in some task inventory somewhere.
	LLViewerObject* objp = gObjectList.findObject(mSourceID);
	if (!objp)
	{
		return NULL;
	}

	EDragAndDropType type = mCargoTypes[mCurItemIndex];
	if (type == DAD_CATEGORY || type == DAD_ROOT_CATEGORY)
	{
		catp = (LLViewerInventoryCategory*)objp->getInventoryObject(cargo_id);
		return catp;
	}

	itemp = (LLViewerInventoryItem*)objp->getInventoryObject(cargo_id);
	return itemp;
}

// Utility function
void pack_permissions_slam(LLMessageSystem* msg, U32 flags,
						   const LLPermissions& perms)
{
	// CRUFT: The server no longer pays attention to this data
	U32 group_mask = perms.getMaskGroup();
	U32 everyone_mask = perms.getMaskEveryone();
	U32 next_owner_mask = perms.getMaskNextOwner();

	msg->addU32Fast(_PREHASH_ItemFlags, flags);
	msg->addU32Fast(_PREHASH_GroupMask, group_mask);
	msg->addU32Fast(_PREHASH_EveryoneMask, everyone_mask);
	msg->addU32Fast(_PREHASH_NextOwnerMask, next_owner_mask);
}
