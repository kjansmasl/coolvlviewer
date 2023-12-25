/**
 * @file llfloaterproperties.h
 * @brief A floater which shows an inventory item's properties.
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

#ifndef LL_LLFLOATERPROPERTIES_H
#define LL_LLFLOATERPROPERTIES_H

#include "llfloater.h"
#include "llinventorymodel.h"

class LLFloaterProperties final : public LLFloater, public LLInventoryObserver
{
protected:
	LOG_CLASS(LLFloaterProperties);

public:
	// LLFloater override (public since used by
	// LLPanelInventory::inventoryChanged()).
	void refresh() override;

	// Note: when 'ownerp' is passed (and not NULL), and the properties floater
	// does not yet exist for this item, the credated floater is made dependent
	// to the parent floater of this owning view. HB
	static void show(const LLUUID& item_id,
					 const LLUUID& object_id = LLUUID::null,
					 LLView* ownerp = NULL);
	static void closeByID(const LLUUID& item_id, const LLUUID& object_id);

	static LLFloaterProperties* find(const LLUUID& item_id,
									 const LLUUID& object_id);

	static void dirtyAll();

protected:
	// Use show() and closeByID()
	LLFloaterProperties(const LLUUID& item_id, const LLUUID& object_id,
						LLView* ownerp);
	~LLFloaterProperties() override;

	// LLFloater overrides
	bool postBuild() override;
	void draw() override;

	// LLInventoryObserver override
	void changed(U32 mask) override;

	LLInventoryItem* findItem() const;
	void refreshFromItem(LLInventoryItem* itemp);
	void updateSaleInfo();

	// UI callbacks
	static void onClickCreator(void* datap);
	static void onClickOwner(void* datap);
	static void onClickLastOwner(void* datap);
	static void onClickThumbnail(void* datap);
	static void onCommitName(LLUICtrl*, void* datap);
	static void onCommitDescription(LLUICtrl*, void* datap);
	static void onCommitPermissions(LLUICtrl*, void* datap);
	static void onCommitSale(LLUICtrl*, void* datap);

protected:
	// The item Id of the inventory item in question.
	LLUUID					mItemID;

	// mObjectID will have a value if it is associated with a rezzed in-world
	// object, and will be LLUUID::null if it is in the agent inventory.
	LLUUID					mObjectID;

	bool					mDirty;

	typedef fast_hmap<LLUUID, LLFloaterProperties*> instance_map_t;
	static instance_map_t	sInstances;
};

class LLMultiProperties : public LLMultiFloater
{
public:
	LLMultiProperties(const LLRect& rect);
};

#endif // LL_LLFLOATERPROPERTIES_H
