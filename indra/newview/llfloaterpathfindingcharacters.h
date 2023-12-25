/** 
 * @file llfloaterpathfindingcharacters.h
 * @brief Pathfinding characters floater, allowing for identification of
 * pathfinding characters and their cpu usage.
 *
 * $LicenseInfo:firstyear=2012&license=viewergpl$
 *
 * Copyright (c) 2012, Linden Research, Inc.
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

#ifndef LL_LLFLOATERPATHFINDINGCHARACTERS_H
#define LL_LLFLOATERPATHFINDINGCHARACTERS_H

#include "llfloaterpathfindingobjects.h"

class LLCheckBoxCtrl;
class LLPathfindingCharacter;
class LLQuaternion;
class LLVector3;

class LLFloaterPathfindingCharacters final
:	public LLFloaterPathfindingObjects,
	public LLFloaterSingleton<LLFloaterPathfindingCharacters>
{
	friend class LLUISingleton<LLFloaterPathfindingCharacters,
							   VisibilityPolicy<LLFloater> >;

protected:
	LOG_CLASS(LLFloaterPathfindingCharacters);

public:
	void onClose(bool app_quitting) override;

	static void openCharactersWithSelectedObjects();

protected:
	// Open only via openCharactersWithSelectedObjects()
	LLFloaterPathfindingCharacters(const LLSD&);

	bool postBuild() override;

	void requestGetObjects() override;

	void addObjectsIntoScrollList(const LLPathfindingObjectList::ptr_t) override;

	LL_INLINE void resetLoadingNameObjectsList() override
	{
		mLoadingNameObjects.clear();
	}

	void updateControlsOnScrollListChange() override;

	LL_INLINE S32 getNameColumnIndex() const override
	{
		return 0;
	}

	LL_INLINE S32 getOwnerNameColumnIndex() const override
	{
		return 2;
	}

	std::string getOwnerName(const LLPathfindingObject* obj) const override;

	LL_INLINE const LLColor4& getBeaconColor() const override
	{
		return mBeaconColor;
	}

	LLPathfindingObjectList::ptr_t getEmptyObjectList() const override;

#if HAVE_PATHINGLIB
	bool isPhysicsCapsuleEnabled(LLUUID& id, LLVector3& pos,
								 LLQuaternion& rot) const;
#endif

	static void newObjectList(LLPathfindingManager::request_id_t request_id,
							  LLPathfindingManager::ERequestStatus req_status,
							  LLPathfindingObjectList::ptr_t pobjects);
private:
	LLSD buildCharacterScrollListItemData(const LLPathfindingCharacter* charp) const;

	void rebuildScrollListAfterAvatarNameLoads(const LLPathfindingObject::ptr_t pobj);

#if HAVE_PATHINGLIB
	void updateStateOnDisplayControls();
	void showSelectedCharacterCapsules();

	void showCapsule() const;
	void hideCapsule() const;
	bool getCapsuleRenderData(LLVector3& pPosition, LLQuaternion& rot) const;

	static void onShowPhysicsCapsuleClicked(LLUICtrl* ctrl, void* data);
#endif

	static void handleObjectNameResponse(const LLPathfindingObject* pobj);

private:
	LLUUID				mSelectedCharacterId;
	LLColor4			mBeaconColor;
	uuid_list_t			mLoadingNameObjects;
#if HAVE_PATHINGLIB
	LLCheckBoxCtrl*		mShowCapsuleCheck;
#endif
};

#endif // LL_LLFLOATERPATHFINDINGCHARACTERS_H
