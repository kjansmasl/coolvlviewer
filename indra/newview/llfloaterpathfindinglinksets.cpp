/**
 * @file llfloaterpathfindinglinksets.cpp
 * @brief Pathfinding linksets floater, allowing manipulation of the linksets
 * on the current region.
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

#include "llfloaterpathfindinglinksets.h"

#include "llbutton.h"
#include "llcombobox.h"
#include "llnotifications.h"
#include "lllineeditor.h"
#include "llscrolllistctrl.h"
#include "lltextbox.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "llfloaterpathfindingobjects.h"
#include "llpathfindinglinkset.h"
#include "llpathfindinglinksetlist.h"
#include "llpathfindingmanager.h"
//MK
#include "mkrlinterface.h"
//mk

#define XUI_LINKSET_USE_NONE             0
#define XUI_LINKSET_USE_WALKABLE         1
#define XUI_LINKSET_USE_STATIC_OBSTACLE  2
#define XUI_LINKSET_USE_DYNAMIC_OBSTACLE 3
#define XUI_LINKSET_USE_MATERIAL_VOLUME  4
#define XUI_LINKSET_USE_EXCLUSION_VOLUME 5
#define XUI_LINKSET_USE_DYNAMIC_PHANTOM  6

//static
void LLFloaterPathfindingLinksets::openLinksetsWithSelectedObjects()
{
	LLFloaterPathfindingLinksets* self = findInstance();
	if (self)
	{
		self->open();
	}
	else
	{
		self = getInstance();	// creates a new instance
		self->clearFilters();
		self->showFloaterWithSelectionObjects();
	}
}

LLFloaterPathfindingLinksets::LLFloaterPathfindingLinksets(const LLSD&)
:	mPreviousValueA(MAX_WALKABILITY_VALUE),
	mPreviousValueB(MAX_WALKABILITY_VALUE),
	mPreviousValueC(MAX_WALKABILITY_VALUE),
	mPreviousValueD(MAX_WALKABILITY_VALUE),
	mHasKnownScripedStatus(false),
	mScriptedColumnWidth(60)
{
	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_pathfinding_linksets.xml");
}

bool LLFloaterPathfindingLinksets::postBuild()
{
	mBeaconColor =
		LLUI::sColorsGroup->getColor("PathfindingLinksetBeaconColor");

	mFilterByName = getChild<LLLineEditor>("filter_by_name");
	mFilterByName->setCommitCallback(onApplyAllFilters);
	mFilterByName->setCallbackUserData(this);
	mFilterByName->setSelectAllonFocusReceived(true);
	mFilterByName->setCommitOnFocusLost(true);

	mFilterByDescription = getChild<LLLineEditor>("filter_by_description");
	mFilterByDescription->setCommitCallback(onApplyAllFilters);
	mFilterByDescription->setCallbackUserData(this);
	mFilterByDescription->setSelectAllonFocusReceived(true);
	mFilterByDescription->setCommitOnFocusLost(true);

	mFilterByLinksetUse = getChild<LLComboBox>("filter_by_linkset_use");
	mFilterByLinksetUse->setCommitCallback(onApplyAllFilters);
	mFilterByDescription->setCallbackUserData(this);

	childSetAction("apply_filters", onApplyAllFiltersClicked, this);
	childSetAction("clear_filters", onClearFiltersClicked, this);

	mEditLinksetUse = getChild<LLComboBox>("edit_linkset_use");
	mEditLinksetUse->clearRows();

	mUseUnset = mEditLinksetUse->add(getString("linkset_choose_use"),
									 XUI_LINKSET_USE_NONE);

	mUseWalkable =
		mEditLinksetUse->add(getLinksetUseString(LLPathfindingLinkset::kWalkable),
							  XUI_LINKSET_USE_WALKABLE);

	mUseStaticObstacle =
		mEditLinksetUse->add(getLinksetUseString(LLPathfindingLinkset::kStaticObstacle),
							 XUI_LINKSET_USE_STATIC_OBSTACLE);

	mUseDynamicObstacle =
		mEditLinksetUse->add(getLinksetUseString(LLPathfindingLinkset::kDynamicObstacle),
							 XUI_LINKSET_USE_DYNAMIC_OBSTACLE);

	mUseMaterialVolume =
		mEditLinksetUse->add(getLinksetUseString(LLPathfindingLinkset::kMaterialVolume),
							 XUI_LINKSET_USE_MATERIAL_VOLUME);

	mUseExclusionVolume =
		mEditLinksetUse->add(getLinksetUseString(LLPathfindingLinkset::kExclusionVolume),
												 XUI_LINKSET_USE_EXCLUSION_VOLUME);

	mUseDynamicPhantom =
		mEditLinksetUse->add(getLinksetUseString(LLPathfindingLinkset::kDynamicPhantom),
												 XUI_LINKSET_USE_DYNAMIC_PHANTOM);

	mEditLinksetUse->selectFirstItem();

	mLabelCoefficients = getChild<LLTextBox>("walkability_coefficients_label");

	mLabelEditA = getChild<LLTextBox>("edit_a_label");

	mLabelSuggestedUseA = getChild<LLTextBox>("suggested_use_a_label");

	mEditA = getChild<LLLineEditor>("edit_a_value");
	mEditA->setPrevalidate(LLLineEditor::prevalidateNonNegativeS32);
	mEditA->setCommitCallback(onWalkabilityCoefficientEntered);
	mEditA->setCallbackUserData(this);

	mLabelEditB = getChild<LLTextBox>("edit_b_label");

	mLabelSuggestedUseB = getChild<LLTextBox>("suggested_use_b_label");

	mEditB = getChild<LLLineEditor>("edit_b_value");
	mEditB->setPrevalidate(LLLineEditor::prevalidateNonNegativeS32);
	mEditB->setCommitCallback(onWalkabilityCoefficientEntered);
	mEditB->setCallbackUserData(this);

	mLabelEditC = getChild<LLTextBox>("edit_c_label");

	mLabelSuggestedUseC = getChild<LLTextBox>("suggested_use_c_label");

	mEditC = getChild<LLLineEditor>("edit_c_value");
	mEditC->setPrevalidate(LLLineEditor::prevalidateNonNegativeS32);
	mEditC->setCommitCallback(onWalkabilityCoefficientEntered);
	mEditC->setCallbackUserData(this);

	mLabelEditD = getChild<LLTextBox>("edit_d_label");

	mLabelSuggestedUseD = getChild<LLTextBox>("suggested_use_d_label");

	mEditD = getChild<LLLineEditor>("edit_d_value");
	mEditD->setPrevalidate(LLLineEditor::prevalidateNonNegativeS32);
	mEditD->setCommitCallback(onWalkabilityCoefficientEntered);
	mEditD->setCallbackUserData(this);

	mApplyEditsButton = getChild<LLButton>("apply_edit_values");
	mApplyEditsButton->setClickedCallback(onApplyChangesClicked, this);

	return LLFloaterPathfindingObjects::postBuild();
}

void LLFloaterPathfindingLinksets::requestGetObjects()
{
	LL_DEBUGS("NavMesh") << "Requesting linksets list" << LL_ENDL;
	LLPathfindingManager* mngrp = LLPathfindingManager::getInstance();
	mngrp->requestGetLinksets(getNewRequestId(),
							  boost::bind(&LLFloaterPathfindingLinksets::newObjectList,
										  _1, _2, _3));
}

LLSD LLFloaterPathfindingLinksets::buildLinksetScrollListItemData(const LLPathfindingLinkset* linksetp,
																  const LLVector3& av_pos)
{
	llassert(linksetp != NULL);
	LLSD columns = LLSD::emptyArray();

	if (linksetp->isTerrain())
	{
		columns[0]["column"] = "name";
		columns[0]["value"] = getString("linkset_terrain_name");

		columns[1]["column"] = "description";
		columns[1]["value"] = getString("linkset_terrain_description");

		columns[2]["column"] = "owner";
		columns[2]["value"] = getString("linkset_terrain_owner");

		columns[3]["column"] = "scripted";
		columns[3]["value"] = getString("linkset_terrain_scripted");

		columns[4]["column"] = "land_impact";
		columns[4]["value"] = getString("linkset_terrain_land_impact");

		columns[5]["column"] = "dist_from_you";
		columns[5]["value"] = getString("linkset_terrain_dist_from_you");
	}
	else
	{
		columns[0]["column"] = "name";
		columns[0]["value"] = linksetp->getName();

		columns[1]["column"] = "description";
		columns[1]["value"] = linksetp->getDescription();

		columns[2]["column"] = "owner";
		columns[2]["value"] = getOwnerName(linksetp);

		std::string scripted;
		if (!linksetp->hasIsScripted())
		{
			scripted = getString("linkset_is_unknown_scripted");
		}
		else if (linksetp->isScripted())
		{
			scripted = getString("linkset_is_scripted");
			mHasKnownScripedStatus = true;
		}
		else
		{
			scripted = getString("linkset_is_not_scripted");
			mHasKnownScripedStatus = true;
		}
		columns[3]["column"] = "scripted";
		columns[3]["value"] = scripted;

		columns[4]["column"] = "land_impact";
		columns[4]["value"] = llformat("%1d", linksetp->getLandImpact());

		columns[5]["column"] = "dist_from_you";
		columns[5]["value"] = llformat("%1.0f m", dist_vec(av_pos, linksetp->getLocation()));
	}
	columns[0]["font"] = "SANSSERIF";
	columns[1]["font"] = "SANSSERIF";
	columns[2]["font"] = "SANSSERIF";
	columns[3]["font"] = "SANSSERIF";
	columns[4]["font"] = "SANSSERIF";
	columns[5]["font"] = "SANSSERIF";

	columns[6]["column"] = "linkset_use";
	std::string use_str = getLinksetUseString(linksetp->getLinksetUse());
	if (linksetp->isTerrain())
	{
		use_str += (" " + getString("linkset_is_terrain"));
	}
	else if (!linksetp->isModifiable() && linksetp->canBeVolume())
	{
		use_str += (" " + getString("linkset_is_restricted_state"));
	}
	else if (linksetp->isModifiable() && !linksetp->canBeVolume())
	{
		use_str += (" " + getString("linkset_is_non_volume_state"));
	}
	else if (!linksetp->isModifiable() && !linksetp->canBeVolume())
	{
		use_str += (" " + getString("linkset_is_restricted_non_volume_state"));
	}
	columns[6]["value"] = use_str;
	columns[6]["font"] = "SANSSERIF";

	columns[7]["column"] = "a_percent";
	columns[7]["value"] = llformat("%3d %%",
								   linksetp->getWalkabilityCoefficientA());
	columns[7]["font"] = "SANSSERIF";

	columns[8]["column"] = "b_percent";
	columns[8]["value"] = llformat("%3d %%",
								   linksetp->getWalkabilityCoefficientB());
	columns[8]["font"] = "SANSSERIF";

	columns[9]["column"] = "c_percent";
	columns[9]["value"] = llformat("%3d %%",
								   linksetp->getWalkabilityCoefficientC());
	columns[9]["font"] = "SANSSERIF";

	columns[10]["column"] = "d_percent";
	columns[10]["value"] = llformat("%3d %%",
								    linksetp->getWalkabilityCoefficientD());
	columns[10]["font"] = "SANSSERIF";

	LLSD row;
	row["id"] = linksetp->getUUID();
	row["columns"] = columns;

	return row;
}

void LLFloaterPathfindingLinksets::addObjectsIntoScrollList(const LLPathfindingObjectList::ptr_t pobjects)
{
	if (!pobjects || pobjects->isEmpty())
	{
		llassert(false);
		return;
	}

	LLScrollListColumn* scripted = mObjectsScrollList->getColumn(3);
	if (scripted && scripted->getWidth() > 0)
	{
		// Store the current width (the first time the list is built and,
		// subsequently, in case the column was resized by the user).
		mScriptedColumnWidth = scripted->getWidth();
	}

	const LLVector3& av_pos = gAgent.getPositionAgent();

	std::string terrain;
	std::string name_filter = mFilterByName->getText();
	bool filter_by_name = !name_filter.empty();
	if (filter_by_name)
	{
		LLStringUtil::toUpper(name_filter);
		terrain = getString("linkset_terrain_name");
	}

	std::string desc_filter = mFilterByDescription->getText();
	bool filter_by_desc = !desc_filter.empty();
	if (filter_by_desc)
	{
		LLStringUtil::toUpper(desc_filter);
	}

	LLPathfindingLinkset::ELinksetUse use_filter = getFilterLinksetUse();
	bool filter_by_use = use_filter != LLPathfindingLinkset::kUnknown;

	std::string tmp;
	for (LLPathfindingObjectList::const_iterator iter = pobjects->begin(),
												 end = pobjects->end();
		 iter != end; ++iter)
	{
		const LLPathfindingObject::ptr_t objp = iter->second;
		if (!objp) continue;

		const LLPathfindingLinkset* linksetp = objp.get()->asLinkset();
		if (!linksetp) continue;

		if (filter_by_use && linksetp->getLinksetUse() != use_filter)
		{
			continue;
		}

		if (filter_by_name)
		{
			tmp = linksetp->isTerrain() ? terrain : linksetp->getName();
			LLStringUtil::toUpper(tmp);
			if (tmp.find(name_filter) == std::string::npos)
			{
				continue;
			}
		}

		if (filter_by_desc)
		{
			tmp = linksetp->getDescription();
			LLStringUtil::toUpper(tmp);
			if (tmp.find(desc_filter) == std::string::npos)
			{
				continue;
			}
		}

		LLSD row = buildLinksetScrollListItemData(linksetp, av_pos);
		mObjectsScrollList->addElement(row, ADD_BOTTOM);

		if (linksetp->hasOwner() && !linksetp->hasOwnerName())
		{
			rebuildScrollListAfterAvatarNameLoads(objp);
		}
	}

	if (scripted)
	{
		// Set the column width to zero if no script info is available,
		// or to its last non-zero width if that info exists.
		scripted->setWidth(mHasKnownScripedStatus ? mScriptedColumnWidth : 0);
	}
}

//static
void LLFloaterPathfindingLinksets::handleObjectNameResponse(const LLPathfindingObject* pobj)
{
	LLFloaterPathfindingLinksets* self = findInstance();
	if (!self) return;

	uuid_list_t::iterator it = self->mLoadingNameObjects.find(pobj->getUUID());
	if (it != self->mLoadingNameObjects.end())
	{
		self->mLoadingNameObjects.hset_erase(it);
		if (self->mLoadingNameObjects.empty())
		{
			self->rebuildObjectsScrollList();
		}
	}
}

void LLFloaterPathfindingLinksets::rebuildScrollListAfterAvatarNameLoads(const LLPathfindingObject::ptr_t pobj)
{
	mLoadingNameObjects.emplace(pobj->getUUID());
	pobj->registerOwnerNameListener(boost::bind(&LLFloaterPathfindingLinksets::handleObjectNameResponse,
												_1));
}

// NOTE: we need a static function to prevent crash in case the floater is
//       closed while the object list is being received... This static function
//       then calls the inherited parent class' function only when the floater
//       instance still exists. HB
//static
void LLFloaterPathfindingLinksets::newObjectList(LLPathfindingManager::request_id_t request_id,
												 LLPathfindingManager::ERequestStatus req_status,
												 LLPathfindingObjectList::ptr_t pobjects)
{
	LLFloaterPathfindingLinksets* self = findInstance();
	if (self)
	{
		self->handleNewObjectList(request_id, req_status, pobjects);
	}
}

void LLFloaterPathfindingLinksets::updateControlsOnScrollListChange()
{
	LLFloaterPathfindingObjects::updateControlsOnScrollListChange();
	updateEditFieldValues();
	updateStateOnEditFields();
	updateStateOnEditLinksetUse();
}

std::string LLFloaterPathfindingLinksets::getOwnerName(const LLPathfindingObject* obj) const
{
	if (!obj->hasOwner())
	{
		static std::string unknown = getString("linkset_owner_unknown");
		return unknown;
	}

	if (!obj->hasOwnerName())
	{
		static std::string loading = getString("linkset_owner_loading");
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
		static std::string group = " " + getString("linkset_owner_group");
		owner += group;
	}

	return owner;
}

LLPathfindingObjectList::ptr_t LLFloaterPathfindingLinksets::getEmptyObjectList() const
{
	LLPathfindingObjectList::ptr_t list(new LLPathfindingLinksetList());
	return list;
}

void LLFloaterPathfindingLinksets::requestSetLinksets(LLPathfindingObjectList::ptr_t linkset_list,
													  LLPathfindingLinkset::ELinksetUse use,
													  S32 a, S32 b, S32 c,
													  S32 d)
{
	LLPathfindingManager* mngp = LLPathfindingManager::getInstance();
	mngp->requestSetLinksets(getNewRequestId(), linkset_list, use, a, b, c, d,
							 boost::bind(&LLFloaterPathfindingLinksets::handleUpdateObjectList,
										 this, _1, _2, _3));
}

//static
void LLFloaterPathfindingLinksets::onApplyAllFilters(LLUICtrl* ctrl, void* data)
{
	LLFloaterPathfindingLinksets* self = (LLFloaterPathfindingLinksets*)data;
	if (self)
	{
		self->rebuildObjectsScrollList();
	}
}

//static
void LLFloaterPathfindingLinksets::onApplyAllFiltersClicked(void* data)
{
	LLFloaterPathfindingLinksets* self = (LLFloaterPathfindingLinksets*)data;
	if (self)
	{
		self->rebuildObjectsScrollList();
	}
}

//static
void LLFloaterPathfindingLinksets::onClearFiltersClicked(void* data)
{
	LLFloaterPathfindingLinksets* self = (LLFloaterPathfindingLinksets*)data;
	if (self)
	{
		self->clearFilters();
		self->rebuildObjectsScrollList();
	}
}

//static
void LLFloaterPathfindingLinksets::onWalkabilityCoefficientEntered(LLUICtrl* ctrl,
																   void* data)
{
	LLFloaterPathfindingLinksets* self = (LLFloaterPathfindingLinksets*)data;
	LLLineEditor* line_editor = (LLLineEditor*)ctrl;
	if (!self || !line_editor) return;

	const std::string& value_str = line_editor->getText();
	bool set_value = false;
	S32 value;
	if (value_str.empty())
	{
		if (line_editor == self->mEditA)
		{
			value = self->mPreviousValueA;
		}
		else if (line_editor == self->mEditB)
		{
			value = self->mPreviousValueB;
		}
		else if (line_editor == self->mEditC)
		{
			value = self->mPreviousValueC;
		}
		else if (line_editor == self->mEditD)
		{
			value = self->mPreviousValueD;
		}
		else
		{
			llwarns << "Invalid call: no corresponding line editor." << llendl;
			value = MAX_WALKABILITY_VALUE;
		}
		set_value = true;
	}
	else if (LLStringUtil::convertToS32(value_str, value))
	{
		if (value < MIN_WALKABILITY_VALUE || value > MAX_WALKABILITY_VALUE)
		{
			value = llclamp(value, MIN_WALKABILITY_VALUE,
							MAX_WALKABILITY_VALUE);
			set_value = true;
		}
	}
	else
	{
		value = MAX_WALKABILITY_VALUE;
		set_value = true;
	}

	if (set_value)
	{
		line_editor->setValue(LLSD(value));
	}
}

//static
void LLFloaterPathfindingLinksets::onApplyChangesClicked(void* data)
{
	LLFloaterPathfindingLinksets* self = (LLFloaterPathfindingLinksets*)data;
	if (self)
	{
		self->applyEdit();
	}
}

void LLFloaterPathfindingLinksets::clearFilters()
{
	mFilterByName->clear();
	mFilterByDescription->clear();
	setFilterLinksetUse(LLPathfindingLinkset::kUnknown);
}

void LLFloaterPathfindingLinksets::updateEditFieldValues()
{
	if (getNumSelectedObjects() <= 0)
	{
		mEditLinksetUse->selectFirstItem();
		mEditA->clear();
		mEditB->clear();
		mEditC->clear();
		mEditD->clear();
		return;
	}

	LLPathfindingObject::ptr_t objectp = getFirstSelectedObject();
	if (!objectp) return;

	const LLPathfindingLinkset* linksetp = objectp.get()->asLinkset();
	if (!linksetp) return;

	setEditLinksetUse(linksetp->getLinksetUse());
	mPreviousValueA = linksetp->getWalkabilityCoefficientA();
	mPreviousValueB = linksetp->getWalkabilityCoefficientB();
	mPreviousValueC = linksetp->getWalkabilityCoefficientC();
	mPreviousValueD = linksetp->getWalkabilityCoefficientD();
	mEditA->setValue(LLSD(mPreviousValueA));
	mEditB->setValue(LLSD(mPreviousValueB));
	mEditC->setValue(LLSD(mPreviousValueC));
	mEditD->setValue(LLSD(mPreviousValueD));
}

bool LLFloaterPathfindingLinksets::showUnmodifiablePhantomWarning(LLPathfindingLinkset::ELinksetUse use) const
{
	if (use == LLPathfindingLinkset::kUnknown)
	{
		return false;
	}

	LLPathfindingObjectList::ptr_t objects = getSelectedObjects();
	if (!objects || objects->isEmpty())
	{
		return false;
	}

	const LLPathfindingLinksetList* listp = objects.get()->asLinksetList();

	return listp && listp->showUnmodifiablePhantomWarning(use);
}

bool LLFloaterPathfindingLinksets::showPhantomToggleWarning(LLPathfindingLinkset::ELinksetUse use) const
{
	if (use == LLPathfindingLinkset::kUnknown)
	{
		return false;
	}

	LLPathfindingObjectList::ptr_t objects = getSelectedObjects();
	if (!objects || objects->isEmpty())
	{
		return false;
	}

	const LLPathfindingLinksetList* listp = objects.get()->asLinksetList();

	return listp && listp->showPhantomToggleWarning(use);
}

bool LLFloaterPathfindingLinksets::showCannotBeVolumeWarning(LLPathfindingLinkset::ELinksetUse use) const
{
	if (use == LLPathfindingLinkset::kUnknown)
	{
		return false;
	}

	LLPathfindingObjectList::ptr_t objects = getSelectedObjects();
	if (!objects || objects->isEmpty())
	{
		return false;
	}

	const LLPathfindingLinksetList* listp = objects.get()->asLinksetList();

	return listp && listp->showCannotBeVolumeWarning(use);
}

void LLFloaterPathfindingLinksets::updateStateOnEditFields()
{
	bool enabled = getNumSelectedObjects() > 0;

	mEditLinksetUse->setEnabled(enabled);

	mLabelCoefficients->setEnabled(enabled);
	mLabelEditA->setEnabled(enabled);
	mLabelEditB->setEnabled(enabled);
	mLabelEditC->setEnabled(enabled);
	mLabelEditD->setEnabled(enabled);
	mLabelSuggestedUseA->setEnabled(enabled);
	mLabelSuggestedUseB->setEnabled(enabled);
	mLabelSuggestedUseC->setEnabled(enabled);
	mLabelSuggestedUseD->setEnabled(enabled);
	mEditA->setEnabled(enabled);
	mEditB->setEnabled(enabled);
	mEditC->setEnabled(enabled);
	mEditD->setEnabled(enabled);

	mApplyEditsButton->setEnabled(enabled &&
								  getMessagingState() == kMessagingComplete);
}

void LLFloaterPathfindingLinksets::updateStateOnEditLinksetUse()
{
	bool walkable = false;
	bool static_obstacle = false;
	bool dynamic_obstacle = false;
	bool material_volume = false;
	bool exclusion_volume = false;
	bool dynamic_phantom = false;

	LLPathfindingObjectList::ptr_t objects = getSelectedObjects();
	if (objects && !objects->isEmpty())
	{
		const LLPathfindingLinksetList* listp = objects.get()->asLinksetList();
		if (listp)
		{
			listp->determinePossibleStates(walkable, static_obstacle,
										   dynamic_obstacle, material_volume,
										   exclusion_volume, dynamic_phantom);
		}
	}

	mUseWalkable->setEnabled(walkable);
	mUseStaticObstacle->setEnabled(static_obstacle);
	mUseDynamicObstacle->setEnabled(dynamic_obstacle);
	mUseMaterialVolume->setEnabled(material_volume);
	mUseExclusionVolume->setEnabled(exclusion_volume);
	mUseDynamicPhantom->setEnabled(dynamic_phantom);
}

void LLFloaterPathfindingLinksets::applyEdit()
{
	LLPathfindingLinkset::ELinksetUse use = getEditLinksetUse();
	bool warn_phantom = showPhantomToggleWarning(use);
	bool warn_nomod = showUnmodifiablePhantomWarning(use);
	bool warn_no_vol = showCannotBeVolumeWarning(use);

	if (!warn_phantom && !warn_nomod && !warn_no_vol)
	{
		doApplyEdit();
		return;
	}

	LLPathfindingLinkset::ELinksetUse restricted_use =
		LLPathfindingLinkset::getLinksetUseWithToggledPhantom(use);
	LLSD substitutions;
	substitutions["REQUESTED_TYPE"] = getLinksetUseString(use);
	substitutions["RESTRICTED_TYPE"] = getLinksetUseString(restricted_use);

	// Build one of the following notifications names
	//   - PathfindingLinksets_WarnOnPhantom
	//   - PathfindingLinksets_WarnOnPhantom_MismatchOnRestricted
	//   - PathfindingLinksets_WarnOnPhantom_MismatchOnVolume
	//   - PathfindingLinksets_WarnOnPhantom_MismatchOnRestricted_MismatchOnVolume
	//   - PathfindingLinksets_MismatchOnRestricted
	//   - PathfindingLinksets_MismatchOnVolume
	//   - PathfindingLinksets_MismatchOnRestricted_MismatchOnVolume
	std::string notification = "PathfindingLinksets";
	if (warn_phantom)
	{
		notification += "_WarnOnPhantom";
	}
	if (warn_nomod)
	{
		notification += "_MismatchOnRestricted";
	}
	if (warn_no_vol)
	{
		notification += "_MismatchOnVolume";
	}
	gNotifications.add(notification, substitutions, LLSD(),
					   boost::bind(&LLFloaterPathfindingLinksets::handleApplyEdit,
								   this, _1, _2));
}

void LLFloaterPathfindingLinksets::handleApplyEdit(const LLSD& notification,
												   const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		doApplyEdit();
	}
}

void LLFloaterPathfindingLinksets::doApplyEdit()
{
	LLPathfindingObjectList::ptr_t objects = getSelectedObjects();
	if (objects && !objects->isEmpty())
	{
		S32 a = atoi(mEditA->getText().c_str());
		S32 b = atoi(mEditB->getText().c_str());
		S32 c = atoi(mEditC->getText().c_str());
		S32 d = atoi(mEditD->getText().c_str());
		requestSetLinksets(objects, getEditLinksetUse(), a, b, c, d);
	}
}

std::string LLFloaterPathfindingLinksets::getLinksetUseString(LLPathfindingLinkset::ELinksetUse use) const
{
	switch (use)
	{
		case LLPathfindingLinkset::kWalkable:
		{
			static std::string walkable = getString("linkset_use_walkable");
			return walkable;
		}

		case LLPathfindingLinkset::kStaticObstacle:
		{
			static std::string sobstacle =
				getString("linkset_use_static_obstacle");
			return sobstacle;
		}

		case LLPathfindingLinkset::kMaterialVolume:
		{
			static std::string material =
				getString("linkset_use_material_volume");
			return material;
		}

		case LLPathfindingLinkset::kExclusionVolume:
		{
			static std::string exclusion =
				getString("linkset_use_exclusion_volume");
			return exclusion;
		}

		case LLPathfindingLinkset::kDynamicPhantom:
		{
			static std::string phantom =
				getString("linkset_use_dynamic_phantom");
			return phantom;
		}

		default:
		{
			llassert(use == LLPathfindingLinkset::kDynamicObstacle);
			static std::string dobstacle =
				getString("linkset_use_dynamic_obstacle");
			return dobstacle;
		}
	}
}

LLPathfindingLinkset::ELinksetUse LLFloaterPathfindingLinksets::getFilterLinksetUse() const
{
	return convertToLinksetUse(mFilterByLinksetUse->getValue());
}

void LLFloaterPathfindingLinksets::setFilterLinksetUse(LLPathfindingLinkset::ELinksetUse use)
{
	mFilterByLinksetUse->setValue(convertToXuiValue(use));
}

LLPathfindingLinkset::ELinksetUse LLFloaterPathfindingLinksets::getEditLinksetUse() const
{
	return convertToLinksetUse(mEditLinksetUse->getValue());
}

void LLFloaterPathfindingLinksets::setEditLinksetUse(LLPathfindingLinkset::ELinksetUse use)
{
	mEditLinksetUse->setValue(convertToXuiValue(use));
}

LLPathfindingLinkset::ELinksetUse LLFloaterPathfindingLinksets::convertToLinksetUse(LLSD value) const
{
	switch (value.asInteger())
	{
		case XUI_LINKSET_USE_NONE:
			return LLPathfindingLinkset::kUnknown;

		case XUI_LINKSET_USE_WALKABLE:
			return LLPathfindingLinkset::kWalkable;

		case XUI_LINKSET_USE_STATIC_OBSTACLE:
			return LLPathfindingLinkset::kStaticObstacle;

		case XUI_LINKSET_USE_DYNAMIC_OBSTACLE:
			return LLPathfindingLinkset::kDynamicObstacle;

		case XUI_LINKSET_USE_MATERIAL_VOLUME:
			return LLPathfindingLinkset::kMaterialVolume;

		case XUI_LINKSET_USE_EXCLUSION_VOLUME:
			return LLPathfindingLinkset::kExclusionVolume;

		case XUI_LINKSET_USE_DYNAMIC_PHANTOM:
			return LLPathfindingLinkset::kDynamicPhantom;

		default:
			llassert(false);
			return LLPathfindingLinkset::kUnknown;
	}
}

LLSD LLFloaterPathfindingLinksets::convertToXuiValue(LLPathfindingLinkset::ELinksetUse use) const
{
	switch (use)
	{
		case LLPathfindingLinkset::kUnknown:
			return LLSD(XUI_LINKSET_USE_NONE);

		case LLPathfindingLinkset::kWalkable:
			return LLSD(XUI_LINKSET_USE_WALKABLE);

		case LLPathfindingLinkset::kStaticObstacle:
			return LLSD(XUI_LINKSET_USE_STATIC_OBSTACLE);

		case LLPathfindingLinkset::kDynamicObstacle:
			return LLSD(XUI_LINKSET_USE_DYNAMIC_OBSTACLE);

		case LLPathfindingLinkset::kMaterialVolume:
			return LLSD(XUI_LINKSET_USE_MATERIAL_VOLUME);

		case LLPathfindingLinkset::kExclusionVolume:
			return LLSD(XUI_LINKSET_USE_EXCLUSION_VOLUME);

		case LLPathfindingLinkset::kDynamicPhantom:
			return LLSD(XUI_LINKSET_USE_DYNAMIC_PHANTOM);

		default:
			llassert(false);
			return LLSD(XUI_LINKSET_USE_NONE);
	}
}
