/**
 * @file llfloaterpathfindingcharacters.cpp
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

#include "llviewerprecompiledheaders.h"

#include "llfloaterpathfindingcharacters.h"

#include "llcheckboxctrl.h"
#include "llscrolllistctrl.h"
#include "lluictrlfactory.h"

#include "llfloaterpathfindingobjects.h"
#include "llpathfindingcharacter.h"
#include "llpathfindingcharacterlist.h"
#include "llpathfindingmanager.h"
#if HAVE_PATHINGLIB
#include "llpathinglib.h"
#include "llpipeline.h"
#endif
//MK
#include "mkrlinterface.h"
//mk
#include "llviewerobjectlist.h"

void LLFloaterPathfindingCharacters::openCharactersWithSelectedObjects()
{
	LLFloaterPathfindingCharacters* self = findInstance();
	if (self)
	{
		self->open();
	}
	else
	{
		self = getInstance();	// Creates a new instance
		self->showFloaterWithSelectionObjects();
	}
}

LLFloaterPathfindingCharacters::LLFloaterPathfindingCharacters(const LLSD&)
{
	LLUICtrlFactory* factory = LLUICtrlFactory::getInstance();
	factory->buildFloater(this, "floater_pathfinding_characters.xml");
}

bool LLFloaterPathfindingCharacters::postBuild()
{
	mBeaconColor =
		LLUI::sColorsGroup->getColor("PathfindingCharacterBeaconColor");

#if HAVE_PATHINGLIB
	mShowCapsuleCheck = getChild<LLCheckBoxCtrl>("show_physics_capsule");
	mShowCapsuleCheck->setCommitCallback(onShowPhysicsCapsuleClicked);
	mShowCapsuleCheck->setCallbackUserData(this);
	mShowCapsuleCheck->setVisible(true);
	mShowCapsuleCheck->setEnabled(LLPathingLib::getInstance() != NULL);
#else
	childHide("show_physics_capsule");
#endif

	return LLFloaterPathfindingObjects::postBuild();
}

void LLFloaterPathfindingCharacters::onClose(bool app_quitting)
{
#if HAVE_PATHINGLIB
	// Hide any capsule that might be showing on floater close
	hideCapsule();
#endif
	LLFloaterPathfindingObjects::onClose(app_quitting);
}

void LLFloaterPathfindingCharacters::requestGetObjects()
{
	LL_DEBUGS("NavMesh") << "Requesting characters list" << LL_ENDL;
	LLPathfindingManager* pmgrp = LLPathfindingManager::getInstance();
	pmgrp->requestGetCharacters(getNewRequestId(),
								boost::bind(&LLFloaterPathfindingCharacters::newObjectList,
											_1, _2, _3));
}

LLSD LLFloaterPathfindingCharacters::buildCharacterScrollListItemData(const LLPathfindingCharacter* charp) const
{
	LLSD columns = LLSD::emptyArray();

	columns[0]["column"] = "name";
	columns[0]["value"] = charp->getName();
	columns[0]["font"] = "SANSSERIF";
	columns[0]["font-style"] = "NORMAL";

	columns[1]["column"] = "description";
	columns[1]["value"] = charp->getDescription();
	columns[1]["font"] = "SANSSERIF";
	columns[1]["font-style"] = "NORMAL";

	columns[2]["column"] = "owner";
	columns[2]["value"] = getOwnerName(charp);
	columns[2]["font"] = "SANSSERIF";
	columns[2]["font-style"] = "NORMAL";

	S32 cpu_time = ll_roundp(charp->getCPUTime());
	std::string cpu_timeString = llformat("%d", cpu_time);
	LLStringUtil::format_map_t string_args;
	string_args["[CPU_TIME]"] = cpu_timeString;

	columns[3]["column"] = "cpu_time";
	columns[3]["value"] = getString("character_cpu_time", string_args);
	columns[3]["font"] = "SANSSERIF";
	columns[3]["font-style"] = "NORMAL";

	std::string altitude = llformat("%1.0f m", charp->getLocation()[2]);
//MK
	if (gRLenabled && gRLInterface.mContainsShowloc)
	{
		altitude = "?";
	}
//mk

	columns[4]["column"] = "altitude";
	columns[4]["value"] = altitude;
	columns[4]["font"] = "SANSSERIF";
	columns[4]["font-style"] = "NORMAL";

	LLSD element;
	element["id"] = charp->getUUID();
	element["columns"] = columns;

	return element;
}

void LLFloaterPathfindingCharacters::addObjectsIntoScrollList(const LLPathfindingObjectList::ptr_t pobjects)
{
	if (!pobjects || pobjects->isEmpty())
	{
		llassert(false);
		return;
	}

	for (LLPathfindingObjectList::const_iterator iter = pobjects->begin(),
												 end = pobjects->end();
		 iter != end; ++iter)
	{
		const LLPathfindingObject::ptr_t objectp = iter->second;
		if (!objectp) continue;

		const LLPathfindingCharacter* charp = objectp.get()->asCharacter();
		if (!charp) continue;

		LLSD row = buildCharacterScrollListItemData(charp);
		mObjectsScrollList->addElement(row, ADD_BOTTOM);

		if (charp->hasOwner() && !charp->hasOwnerName())
		{
			rebuildScrollListAfterAvatarNameLoads(objectp);
		}
	}
}

//static
void LLFloaterPathfindingCharacters::handleObjectNameResponse(const LLPathfindingObject* pobj)
{
	LLFloaterPathfindingCharacters* self = findInstance();
	if (!self) return;

	uuid_list_t::iterator it = self->mLoadingNameObjects.find(pobj->getUUID());
	if (it != self->mLoadingNameObjects.end())
	{
		self->mLoadingNameObjects.erase(it);
		if (self->mLoadingNameObjects.empty())
		{
			self->rebuildObjectsScrollList();
		}
	}
}

void LLFloaterPathfindingCharacters::rebuildScrollListAfterAvatarNameLoads(const LLPathfindingObject::ptr_t pobj)
{
	mLoadingNameObjects.emplace(pobj->getUUID());
	pobj->registerOwnerNameListener(boost::bind(&LLFloaterPathfindingCharacters::handleObjectNameResponse,
												_1));
}

// NOTE: we need a static function to prevent crash in case the floater is
//       closed while the object list is being received... This static function
//       then calls the inherited parent class' function only when the floater
//       instance still exists.
//static
void LLFloaterPathfindingCharacters::newObjectList(LLPathfindingManager::request_id_t request_id,
												   LLPathfindingManager::ERequestStatus req_status,
												   LLPathfindingObjectList::ptr_t pobjects)
{
	LLFloaterPathfindingCharacters* self = findInstance();
	if (self)
	{
		self->handleNewObjectList(request_id, req_status, pobjects);
	}
}

void LLFloaterPathfindingCharacters::updateControlsOnScrollListChange()
{
	LLFloaterPathfindingObjects::updateControlsOnScrollListChange();
#if HAVE_PATHINGLIB
	updateStateOnDisplayControls();
	showSelectedCharacterCapsules();
#endif
}

std::string LLFloaterPathfindingCharacters::getOwnerName(const LLPathfindingObject* obj) const
{
	if (!obj->hasOwner())
	{
		static std::string unknown = getString("character_owner_unknown");
		return unknown;
	}

	if (!obj->hasOwnerName())
	{
		static std::string loading = getString("character_owner_loading");
		return loading;
	}

	std::string owner = obj->getOwnerName();
//MK
	if (gRLenabled &&
		 (gRLInterface.mContainsShownames ||
		  gRLInterface.mContainsShownametags) &&
		!obj->isGroupOwned())
	{
		owner = gRLInterface.getDummyName(owner);
	}
//mk

	if (obj->isGroupOwned())
	{
		static std::string group = " " + getString("character_owner_group");
		owner += group;
	}

	return owner;
}

LLPathfindingObjectList::ptr_t LLFloaterPathfindingCharacters::getEmptyObjectList() const
{
	LLPathfindingObjectList::ptr_t objlistp(new LLPathfindingCharacterList());
	return objlistp;
}

#if HAVE_PATHINGLIB
bool LLFloaterPathfindingCharacters::isPhysicsCapsuleEnabled(LLUUID& id,
															 LLVector3& pos,
															 LLQuaternion& rot) const
{
	id = mSelectedCharacterId;
	// Physics capsule is enable if the checkbox is enabled and if we can get
	// the required render parameters for any selected object
	return mShowCapsuleCheck->get() && getCapsuleRenderData(pos, rot);
}

//static
void LLFloaterPathfindingCharacters::onShowPhysicsCapsuleClicked(LLUICtrl* ctrl,
																 void* data)
{
	LLFloaterPathfindingCharacters* self = (LLFloaterPathfindingCharacters*)data;
	LLCheckBoxCtrl* check = (LLCheckBoxCtrl*)ctrl;
	if (!self || !check) return;

	bool enabled = check->get();
	if (LLPathingLib::getInstance() == NULL)
	{
		if (enabled)
		{
			check->set(false);
		}
	}
	else if (self->mSelectedCharacterId.notNull() && enabled)
	{
		showCapsule();
	}
	else
	{
		hideCapsule();
	}
}

void LLFloaterPathfindingCharacters::updateStateOnDisplayControls()
{
	bool enabled = getNumSelectedObjects() == 1 && LLPathingLib::getInstance();
	mShowCapsuleCheck->setEnabled(enabled);
	if (!enabled)
	{
		mShowCapsuleCheck->set(false);
	}
}

void LLFloaterPathfindingCharacters::showSelectedCharacterCapsules()
{
	// Hide any previous capsule
	hideCapsule();

	// Get the only selected object, or set the selected object to null if we
	// do not have exactly one object selected
	if (getNumSelectedObjects() == 1)
	{
		LLPathfindingObject::ptr_t objectp = getFirstSelectedObject();
		if (objectp)	// Paranoia
		{
			mSelectedCharacterId = objectp->getUUID();
		}
		else
		{
			mSelectedCharacterId.setNull();
		}
	}
	else
	{
		mSelectedCharacterId.setNull();
	}

	// Show any capsule if enabled
	showCapsule();
}

void LLFloaterPathfindingCharacters::showCapsule() const
{
	if (mSelectedCharacterId.isNull() || !mShowCapsuleCheck->get())
	{
		return;
	}

	LLPathfindingObject::ptr_t objectp = getFirstSelectedObject();
	if (!objectp)
	{
		return;
	}

	const LLPathfindingCharacter* charp = objectp.get()->asCharacter();
	if (!charp || charp->getUUID() != mSelectedCharacterId)
	{
		llassert(false);
		return;
	}

	gPipeline.hideObject(mSelectedCharacterId);

	LLPathingLib* pthlip = LLPathingLib::getInstance();
	if (pthlip)
	{	
		pthlip->createPhysicsCapsuleRep(charp->getLength(),
										charp->getRadius(),
										charp->isHorizontal(),
										charp->getUUID());
	}
}

void LLFloaterPathfindingCharacters::hideCapsule() const
{
	if (mSelectedCharacterId.notNull())
	{
		gPipeline.restoreHiddenObject(mSelectedCharacterId);
	}
	LLPathingLib* pthlip = LLPathingLib::getInstance();
	if (pthlip)
	{
		pthlip->cleanupPhysicsCapsuleRepResiduals();
	}
}

bool LLFloaterPathfindingCharacters::getCapsuleRenderData(LLVector3& pos,
														  LLQuaternion& rot) const
{
	// If we have a selected object, find the object on the viewer object list
	// and return its position. Else, return false indicating that we either do
	// not have a selected object or we cannot find the selected object on the
	// viewer object list
	if (mSelectedCharacterId.notNull())
	{
		LLViewerObject* vobj = gObjectList.findObject(mSelectedCharacterId);
		if (vobj)
		{
			rot = vobj->getRotation();
			pos = vobj->getRenderPosition();
			return true;
		}
	}
	return false;
}
#endif
