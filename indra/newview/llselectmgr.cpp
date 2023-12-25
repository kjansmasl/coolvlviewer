/**
 * @file llselectmgr.cpp
 * @brief A manager for selected objects and faces.
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

#include "llselectmgr.h"

#include "llcachename.h"
#include "lldbstrings.h"
#include "lleconomy.h"
#include "llgl.h"
#include "llmediaentry.h"
#include "llmenugl.h"
#include "llparcel.h"
#include "llpermissions.h"
#include "llpermissionsflags.h"
#include "llrender.h"
#include "lltrans.h"
#include "llundo.h"
#include "llvolume.h"
#include "llmessage.h"
#include "object_flags.h"

#include "llagent.h"
#include "lldrawable.h"
#include "llfloaterinspect.h"
#include "llfloaterproperties.h"
#include "llfloaterreporter.h"
#include "llfloatertools.h"
#include "llgltfmateriallist.h"
#include "llgridmanager.h"					// For gIsInSecondLife
#include "llhudeffectspiral.h"
#include "llinventorymodel.h"
#include "llmaterialmgr.h"
#include "llmeshrepository.h"
#include "llmutelist.h"
#include "llpanelface.h"
#include "llpipeline.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llstatusbar.h"
#include "llsurface.h"
#include "lltool.h"
#include "lltooldraganddrop.h"
#include "lltoolmgr.h"
#include "lltoolpie.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewermedia.h"
#include "llviewermediafocus.h"
#include "llviewermenu.h"
#include "llviewerobjectlist.h"
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"
#include "llviewershadermgr.h"
#include "llviewerstats.h"
#include "llviewertexturelist.h"
#include "llviewerwindow.h"
#include "llvoavatarpuppet.h"
#include "llvoavatarself.h"
#include "llvograss.h"
#include "llvotree.h"
#include "llvovolume.h"

LLSelectMgr gSelectMgr;

// For linked sets
constexpr S32 MAX_CHILDREN_PER_TASK = 255;

constexpr F32 SILHOUETTE_UPDATE_THRESHOLD_SQUARED = 0.02f;
constexpr S32 MAX_SILS_PER_FRAME = 50;
constexpr S32 MAX_OBJECTS_PER_PACKET = 254;

bool LLSelectMgr::sRectSelectInclusive = true;
bool LLSelectMgr::sRenderLightRadius = false;
F32	LLSelectMgr::sHighlightThickness = 0.f;
F32	LLSelectMgr::sHighlightUScale = 0.f;
F32	LLSelectMgr::sHighlightVScale = 0.f;
F32	LLSelectMgr::sHighlightAlpha = 0.f;
F32	LLSelectMgr::sHighlightAlphaTest = 0.f;
F32	LLSelectMgr::sHighlightUAnim = 0.f;
F32	LLSelectMgr::sHighlightVAnim = 0.f;
LLColor4 LLSelectMgr::sSilhouetteParentColor;
LLColor4 LLSelectMgr::sSilhouetteChildColor;
LLColor4 LLSelectMgr::sHighlightInspectColor;
LLColor4 LLSelectMgr::sHighlightParentColor;
LLColor4 LLSelectMgr::sHighlightChildColor;
LLColor4 LLSelectMgr::sContextSilhouetteColor;
uuid_list_t LLSelectMgr::sObjectPropertiesFamilyRequests;

// Used to keep track of important derez info.
struct LLDeRezInfo
{
	EDeRezDestination mDestination;
	LLUUID mDestinationID;
	LLDeRezInfo(EDeRezDestination dest, const LLUUID& dest_id)
	:	mDestination(dest),
		mDestinationID(dest_id)
	{
	}
};

// Helper function
LLViewerObject* getSelectedParentObject(LLViewerObject* object)
{
	LLViewerObject* parent;
	while (object && (parent = (LLViewerObject*)object->getParent()))
	{
		if (parent->isSelected())
		{
			object = parent;
		}
		else
		{
			break;
		}
	}
	return object;
}

//-----------------------------------------------------------------------------
// LLSelectMgr class
//-----------------------------------------------------------------------------

LLSelectMgr::LLSelectMgr()
:	mHideSelectedObjects(false),
	mRenderSelectionsPolicy(2),
	mAllowSelectAvatar(false),
	mDebugSelectMgr(false),
	mEditLinkedParts(false),
	mSelectOwnedOnly(false),
	mSelectMovableOnly(false),
	mForceSelection(false),
	mShowSelection(false),
	mTEMode(false),
	mRenderSilhouettes(true),
	mGridMode(GRID_MODE_WORLD),
	mTextureChannel(LLRender::DIFFUSE_MAP)
{
	mSelectedObjects = new LLObjectSelection();
	mHoverObjects = new LLObjectSelection();
	mHighlightedObjects = new LLObjectSelection();
}

LLSelectMgr::~LLSelectMgr()
{
	clearSelections();
}

void LLSelectMgr::clearSelections()
{
	mHoverObjects->deleteAllNodes();
	mSelectedObjects->deleteAllNodes();
	mHighlightedObjects->deleteAllNodes();
	mRectSelectedObjects.clear();
	mGridObjects.deleteAllNodes();

	LLPipeline::setRenderHighlightTextureChannel(LLRender::DIFFUSE_MAP);
}

void LLSelectMgr::initClass()
{
	connectRefreshCachedSettingsSafe("HideSelectedObjects");
	connectRefreshCachedSettingsSafe("RenderHighlightSelectionsPolicy");
	connectRefreshCachedSettingsSafe("AllowSelectAvatar");
	connectRefreshCachedSettingsSafe("DebugSelectMgr");
	connectRefreshCachedSettingsSafe("EditLinkedParts");
	connectRefreshCachedSettingsSafe("SelectOwnedOnly");
	connectRefreshCachedSettingsSafe("SelectMovableOnly");

	connectRefreshCachedSettingsSafe("RectangleSelectInclusive");
	connectRefreshCachedSettingsSafe("RenderLightRadius");

	connectRefreshCachedSettingsSafe("SelectionHighlightThickness");
	connectRefreshCachedSettingsSafe("SelectionHighlightUScale");
	connectRefreshCachedSettingsSafe("SelectionHighlightVScale");
	connectRefreshCachedSettingsSafe("SelectionHighlightAlpha");
	connectRefreshCachedSettingsSafe("SelectionHighlightAlphaTest");
	connectRefreshCachedSettingsSafe("SelectionHighlightUAnim");
	connectRefreshCachedSettingsSafe("SelectionHighlightVAnim");

	connectRefreshCachedSettingsSafe("SilhouetteParentColor");
	connectRefreshCachedSettingsSafe("SilhouetteChildColor");
	connectRefreshCachedSettingsSafe("HighlightParentColor");
	connectRefreshCachedSettingsSafe("HighlightChildColor");
	connectRefreshCachedSettingsSafe("HighlightInspectColor");
	connectRefreshCachedSettingsSafe("ContextSilhouetteColor");

	setGridMode(GRID_MODE_WORLD);

	refreshCachedSettings();

	llinfos << "Selection manager initialized" << llendl;
}

//static
void LLSelectMgr::connectRefreshCachedSettingsSafe(const char* name)
{
	LLControlVariable* controlp = gSavedSettings.getControl(name);
	if (!controlp)
	{
		controlp = gColors.getControl(name);
	}
	if (!controlp)
	{
		llwarns << "Setting name not found: " << name << llendl;
		return;
	}
	controlp->getSignal()->connect(boost::bind(&LLSelectMgr::refreshCachedSettings));
}

//static
void LLSelectMgr::refreshCachedSettings()
{
	// Note: do not bother using LLCachedControls here: this method is rarely
	// called ("EditLinkedParts" is the most frequent trigger and still rare).

	gSelectMgr.mHideSelectedObjects =
		gSavedSettings.getBool("HideSelectedObjects");
	gSelectMgr.mRenderSelectionsPolicy =
		gSavedSettings.getU32("RenderHighlightSelectionsPolicy");
	gSelectMgr.mAllowSelectAvatar =
		gSavedSettings.getBool("AllowSelectAvatar");
	gSelectMgr.mDebugSelectMgr = gSavedSettings.getBool("DebugSelectMgr");
	gSelectMgr.mEditLinkedParts = gSavedSettings.getBool("EditLinkedParts");
	gSelectMgr.mSelectOwnedOnly = gSavedSettings.getBool("SelectOwnedOnly");
	gSelectMgr.mSelectMovableOnly =
		gSavedSettings.getBool("SelectMovableOnly");

	sRectSelectInclusive = gSavedSettings.getBool("RectangleSelectInclusive");
	sRenderLightRadius = gSavedSettings.getBool("RenderLightRadius");

	sHighlightThickness = gSavedSettings.getF32("SelectionHighlightThickness");
	sHighlightUScale = gSavedSettings.getF32("SelectionHighlightUScale");
	sHighlightVScale = gSavedSettings.getF32("SelectionHighlightVScale");
	sHighlightAlpha = gSavedSettings.getF32("SelectionHighlightAlpha");
	sHighlightAlphaTest = gSavedSettings.getF32("SelectionHighlightAlphaTest");
	sHighlightUAnim = gSavedSettings.getF32("SelectionHighlightUAnim");
	sHighlightVAnim = gSavedSettings.getF32("SelectionHighlightVAnim");

	sSilhouetteParentColor = gColors.getColor("SilhouetteParentColor");
	sSilhouetteChildColor = gColors.getColor("SilhouetteChildColor");
	sHighlightParentColor = gColors.getColor("HighlightParentColor");
	sHighlightChildColor = gColors.getColor("HighlightChildColor");
	sHighlightInspectColor = gColors.getColor("HighlightInspectColor");
	sContextSilhouetteColor = gColors.getColor("ContextSilhouetteColor") *
							  0.5f;
}

//static
bool LLSelectMgr::renderHiddenSelection()
{
	U32 policy = gSelectMgr.mRenderSelectionsPolicy;
	return policy > 2 || (policy == 2 && gToolMgr.inEdit());
}

void LLSelectMgr::update()
{
	mSelectedObjects->cleanupNodes();
}

void LLSelectMgr::updateEffects()
{
	// Keep reference grid objects active
	struct f final : public LLSelectedObjectFunctor
	{
		bool apply(LLViewerObject* object) override
		{
			LLDrawable* drawablep = object->mDrawable;
			if (drawablep)
			{
				gPipeline.markMoved(drawablep);
			}
			return true;
		}
	} func;
	mGridObjects.applyToObjects(&func);

	if (mEffectsTimer.getElapsedTimeF32() > 1.f)
	{
		mSelectedObjects->updateEffects();
		mEffectsTimer.reset();
	}
}

void LLSelectMgr::overrideObjectUpdates()
{
	// Override any position updates from simulator on objects being edited
	struct f final : public LLSelectedNodeFunctor
	{
		bool apply(LLSelectNode* selectNode) override
		{
			LLViewerObject* object = selectNode->getObject();
			if (object && object->permMove() && !object->isPermanentEnforced())
			{
				if (!selectNode->mLastPositionLocal.isExactlyZero())
				{
					object->setPositionLocal(selectNode->mLastPositionLocal);
				}
				if (selectNode->mLastRotation != LLQuaternion())
				{
					object->setRotation(selectNode->mLastRotation);
				}
				if (!selectNode->mLastScale.isExactlyZero())
				{
					object->setScale(selectNode->mLastScale);
				}
			}
			return true;
		}
	} func;
	getSelection()->applyToNodes(&func);
}

// Selects just the object, not any other group members.
LLObjectSelectionHandle LLSelectMgr::selectObjectOnly(LLViewerObject* object,
													  S32 face)
{
	llassert(object);

	// Remember primary object
	mSelectedObjects->mPrimaryObject = object;

	// Do not add an object that is already in the list
	if (object->isSelected())
	{
		// Make sure point at position is updated
		updatePointAt();
		grabMenuHandler();
		return NULL;
	}

	if (!canSelectObject(object))
	{
#if 0
		make_ui_sound("UISndInvalidOp");
#endif
		return NULL;
	}

	// Place it in the list and tag it.
	// This will refresh dialogs.
	addAsIndividual(object, face);

	// Stop the object from moving (this anticipates changes on the simulator
	// in LLTask::userSelect)
	// *FIX: should not zero out these either
	object->setVelocity(LLVector3::zero);
	object->setAcceleration(LLVector3::zero);
#if 0	// Do not do that: it breaks target-omega seats !
	object->setAngularVelocity(LLVector3::zero);
#endif
	object->resetRot();

	// Always send to simulator, so you get a copy of the permissions structure
	// back.
	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessageFast(_PREHASH_ObjectSelect);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlockFast(_PREHASH_ObjectData);
	msg->addU32Fast(_PREHASH_ObjectLocalID, object->getLocalID());
	LLViewerRegion* regionp = object->getRegion();
	msg->sendReliable(regionp->getHost());

	updatePointAt();
	updateSelectionCenter();
	saveSelectedObjectTransform(SELECT_ACTION_TYPE_PICK);

	// Have selection manager handle edit menu immediately after user selects
	// an object
	if (mSelectedObjects->getObjectCount())
	{
		grabMenuHandler();
	}

	return mSelectedObjects;
}

// Selects the object, parents and children.
LLObjectSelectionHandle LLSelectMgr::selectObjectAndFamily(LLViewerObject* obj,
														   bool add_to_end)
{
	llassert(obj);

	// Remember primary object
	mSelectedObjects->mPrimaryObject = obj;

	// This may be incorrect if things were not family selected before...
	// Do not add an object that is already in the list
	if (obj->isSelected())
	{
		// make sure pointat position is updated
		updatePointAt();
		grabMenuHandler();
		return NULL;
	}

	if (!canSelectObject(obj))
	{
		//make_ui_sound("UISndInvalidOp");
		return NULL;
	}

	// Since we are selecting a family, start at the root, but do not include
	// an avatar.
	LLViewerObject* root = obj;

	while (!root->isAvatar() && root->getParent())
	{
		LLViewerObject* parent = (LLViewerObject*)root->getParent();
		if (parent->isAvatar())
		{
			break;
		}
		root = parent;
	}

	// Collect all of the objects
	std::vector<LLViewerObject*> objects;

	root->addThisAndNonJointChildren(objects);
	addAsFamily(objects, add_to_end);

	updateSelectionCenter();
	saveSelectedObjectTransform(SELECT_ACTION_TYPE_PICK);
	updatePointAt();

	dialog_refresh_all();

	// Always send to simulator, so you get a copy of the permissions structure
	// back.
	sendSelect();

	// Stop the object from moving (this anticipates changes on the simulator
	// in LLTask::userSelect)
	root->setVelocity(LLVector3::zero);
	root->setAcceleration(LLVector3::zero);
#if 0	// Do not do that: it breaks target-omega seats !
	root->setAngularVelocity(LLVector3::zero);
#endif
	root->resetRot();

	// Leave component mode
	if (mEditLinkedParts)
	{
		gSavedSettings.setBool("EditLinkedParts", false);
		promoteSelectionToRoot();
	}

	// Have selection manager handle edit menu immediately after user selects
	// an object
	if (mSelectedObjects->getObjectCount())
	{
		grabMenuHandler();
	}

	return mSelectedObjects;
}

// Selects the object, parents and children.
LLObjectSelectionHandle LLSelectMgr::selectObjectAndFamily(const std::vector<LLViewerObject*>& object_list,
														   bool send_to_sim)
{
	// Collect all of the objects, children included
	std::vector<LLViewerObject*> objects;

	// Clear primary object (no primary object)
	mSelectedObjects->mPrimaryObject = NULL;

	if (object_list.size() < 1)
	{
		return NULL;
	}

	// NOTE: we add the objects in REVERSE ORDER to preserve the order in the
	// mSelectedObjects list
	for (std::vector<LLViewerObject*>::const_reverse_iterator
			riter = object_list.rbegin(), rend = object_list.rend();
		 riter != rend; ++riter)
	{
		LLViewerObject* object = *riter;

		llassert(object);

		if (!canSelectObject(object)) continue;

		object->addThisAndNonJointChildren(objects);
		addAsFamily(objects);

		// Stop the object from moving (this anticipates changes on the
		// simulator in LLTask::userSelect)
		object->setVelocity(LLVector3::zero);
		object->setAcceleration(LLVector3::zero);
#if 0	// Do not do that: it breaks target-omega seats !
		object->setAngularVelocity(LLVector3::zero);
#endif
		object->resetRot();
	}

	updateSelectionCenter();
	saveSelectedObjectTransform(SELECT_ACTION_TYPE_PICK);
	updatePointAt();
	dialog_refresh_all();

	// Almost always send to simulator, so you get a copy of the permissions
	// structure back.
	// JC: The one case where you do not want to do this is if you're selecting
	// all the objects on a sim.
	if (send_to_sim)
	{
		sendSelect();
	}

	// leave component mode
	if (mEditLinkedParts)
	{
		gSavedSettings.setBool("EditLinkedParts", false);
		promoteSelectionToRoot();
	}

	// have selection manager handle edit menu immediately after user selects
	// an object
	if (mSelectedObjects->getObjectCount())
	{
		grabMenuHandler();
	}

	return mSelectedObjects;
}

// Use for when the simulator kills an object. This version also handles
// informing the current tool of the object's deletion.
// Caller needs to call dialog_refresh_all if necessary.
bool LLSelectMgr::removeObjectFromSelections(const LLUUID& id)
{
	bool object_found = false;
	LLTool* tool = gToolMgr.getCurrentTool();

	// It is possible that the tool is editing an object that is not selected
	LLViewerObject* tool_editing_object = tool->getEditingObject();
	if (tool_editing_object && tool_editing_object->mID == id)
	{
		tool->stopEditing();
		object_found = true;
	}

	// Iterate through selected objects list and kill the object
	if (!object_found)
	{
		for (LLObjectSelection::iterator iter = getSelection()->begin();
			 iter != getSelection()->end(); )
		{
			LLSelectNode* node = *iter++;
			if (!node) continue;		// Paranoia

			LLViewerObject* object = node->getObject();
			if (!object) continue;		// Paranoia

			if (object->mID == id)
			{
				if (tool)
				{
					tool->stopEditing();
				}

				// Loose the selection, do not tell simulator, it knows
				deselectObjectAndFamily(object, false);
				object_found = true;
				// Must break here: may have removed multiple objects from list
				break;
			}
			else if (object->isAvatar() && object->getParent() &&
					 ((LLViewerObject*)object->getParent())->mID == id)
			{
				// It is possible the item being removed has an avatar sitting
				// on it, so remove the avatar that is sitting on the object.
				deselectObjectAndFamily(object, false);
				// Must break here: may have removed multiple objects from list
				break;
			}
		}
	}

	return object_found;
}

bool LLSelectMgr::linkObjects()
{
//MK
	if (gRLenabled && gRLInterface.mContainsUnsit &&
		gRLInterface.isSittingOnAnySelectedObject())
	{
		return true;
	}
//mk
	if (!selectGetAllRootsValid())
	{
		gNotifications.add("UnableToLinkWhileDownloading");
		return true;
	}

	S32 object_count = getSelection()->getObjectCount();
	S32 max_linked_prims = MAX_CHILDREN_PER_TASK + 1;
	if (!gIsInSecondLife)
	{
		static LLCachedControl<S32> os_max_prim_scale(gSavedSettings,
													  "OSMaxLinkedPrims");
		if (os_max_prim_scale > max_linked_prims)
		{
			max_linked_prims = os_max_prim_scale;
		}
		else if (os_max_prim_scale < 0)
		{
			max_linked_prims = object_count;	// no limit
		}
	}
	if (object_count > max_linked_prims)
	{
		LLSD args;
		args["COUNT"] = llformat("%d", object_count);
		args["MAX"] = llformat("%d", max_linked_prims);
		gNotifications.add("UnableToLinkObjects", args);
		return true;
	}

	if (getSelection()->getRootObjectCount() < 2)
	{
		gNotifications.add("CannotLinkIncompleteSet");
		return true;
	}

	if (!selectGetRootsModify())
	{
		gNotifications.add("CannotLinkModify");
		return true;
	}

	if (!selectGetRootsNonPermanentEnforced())
	{
		gNotifications.add("CannotLinkPermanent");
		return true;
	}

	LLUUID owner_id;
	std::string owner_name;
	if (!selectGetOwner(owner_id, owner_name))
	{
		// We do not actually care if you're the owner, but novices are the
		// most likely to be stumped by this one, so offer the easiest and
		// most likely solution.
		gNotifications.add("CannotLinkDifferentOwners");
		return true;
	}

	sendLink();

	return true;
}

bool LLSelectMgr::unlinkObjects()
{
//MK
	if (gRLenabled && gRLInterface.mContainsUnsit &&
		gRLInterface.isSittingOnAnySelectedObject())
	{
		return true;
	}
//mk
	sendDelink();
	return true;
}

// In order to link, all objects must have the same owner, and the agent must
// have the ability to modify all of the objects. However, we are not answering
// that question with this method. The question we are answering is: does the
// user have a reasonable expectation that a link operation should work ?  If
// so, return true, false otherwise. This allows the handle_link method to more
// finely check the selection and give an error message when the uer has a
// reasonable expectation for the link to work, but it will fail.
// For animated objects, there's additional check that if the selection
// includes at least one animated object, the total mesh triangle count cannot
// exceed the designated limit.
bool LLSelectMgr::enableLinkObjects()
{
	bool new_value = false;

	// Check if there are at least 2 objects selected, and that the user can
	// modify at least one of the selected objects.

	// In component mode, cannot link
	if (!mEditLinkedParts)
	{
		if (selectGetAllRootsValid() &&
			getSelection()->getRootObjectCount() >= 2)
		{
			struct f final : public LLSelectedObjectFunctor
			{
				bool apply(LLViewerObject* object) override
				{
					LLViewerObject* root_object = object->getRootEdit();
					return object->permModify() &&
						   !object->isPermanentEnforced() &&
						   (!root_object ||
							!root_object->isPermanentEnforced());
				}
			} func;
			constexpr bool firstonly = true;
			new_value = getSelection()->applyToRootObjects(&func, firstonly);
		}
//MK
		if (gRLenabled && gRLInterface.mContainsUnsit &&
			gRLInterface.isSittingOnAnySelectedObject())
		{
			new_value = false;
		}
//mk
	}
	if (!getSelection()->checkAnimatedObjectEstTris())
	{
		new_value = false;
	}

	return new_value;
}

bool LLSelectMgr::enableUnlinkObjects()
{
	LLViewerObject* first_editable_object =
		getSelection()->getFirstEditableObject();
	LLViewerObject* root_object =
		first_editable_object ? first_editable_object->getRootEdit() :  NULL;

	bool new_value = selectGetAllRootsValid() &&
					 first_editable_object &&
					 !first_editable_object->isAttachment() &&
					 !first_editable_object->isPermanentEnforced() &&
					 (!root_object || !root_object->isPermanentEnforced());
//MK
		if (gRLenabled && gRLInterface.mContainsUnsit &&
			gRLInterface.isSittingOnAnySelectedObject())
		{
			new_value = false;
		}
//mk
	return new_value;
}

void LLSelectMgr::deselectObjectAndFamily(LLViewerObject* object,
										  bool send_to_sim,
										  bool include_entire_object)
{
	// Bail if nothing selected or if object was not selected in the first
	// place
	if (!object || !object->isSelected()) return;

	// Collect all of the objects, and remove them
	std::vector<LLViewerObject*> objects;

	if (include_entire_object)
	{
		// Since we are selecting a family, start at the root, but do not
		// include an avatar.
		LLViewerObject* parent;
		while (!object->isAvatar() &&
			   (parent = (LLViewerObject*)object->getParent()) != NULL &&
			   !parent->isAvatar())
		{
			object = parent;
		}
	}
	else
	{
		object = (LLViewerObject*)object->getRoot();
	}

	object->addThisAndAllChildren(objects);
	remove(objects);

	if (!send_to_sim) return;

	//-----------------------------------------------------------
	// Inform simulator of deselection
	//-----------------------------------------------------------
	LLViewerRegion* regionp = object->getRegion();
	if (!regionp) return;

	bool start_new_message = true;
	S32 select_count = 0;

	LLMessageSystem* msg = gMessageSystemp;
	for (U32 i = 0, count = objects.size(); i < count; ++i)
	{
		if (start_new_message)
		{
			msg->newMessageFast(_PREHASH_ObjectDeselect);
			msg->nextBlockFast(_PREHASH_AgentData);
			msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
			msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
			select_count++;
			start_new_message = false;
		}

		msg->nextBlockFast(_PREHASH_ObjectData);
		msg->addU32Fast(_PREHASH_ObjectLocalID, (objects[i])->getLocalID());
		++select_count;

#if 0	// Do not do that: it breaks target-omega seats !
		// Zap the angular velocity, as the sim will set it to zero
		objects[i]->setAngularVelocity(0.f, 0.f, 0.f);
#endif
		objects[i]->setVelocity(0.f, 0.f, 0.f);

		if (msg->isSendFull(NULL) || select_count >= MAX_OBJECTS_PER_PACKET)
		{
			msg->sendReliable(regionp->getHost());
			select_count = 0;
			start_new_message = true;
		}
	}

	if (!start_new_message)
	{
		msg->sendReliable(regionp->getHost());
	}

	updatePointAt();
	updateSelectionCenter();
}

void LLSelectMgr::deselectObjectOnly(LLViewerObject* object, bool send_to_sim)
{
	// Bail if nothing selected or if object was not selected in the first
	// place
	if (!object || !object->isSelected()) return;

#if 0	// Do not do that: it breaks target-omega seats !
	// Zap the angular velocity, as the sim will set it to zero
	object->setAngularVelocity(0.f, 0.f, 0.f);
#endif
	object->setVelocity(0.f, 0.f, 0.f);

	if (send_to_sim)
	{
		LLViewerRegion* region = object->getRegion();
		LLMessageSystem* msg = gMessageSystemp;
		msg->newMessageFast(_PREHASH_ObjectDeselect);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
		msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
		msg->nextBlockFast(_PREHASH_ObjectData);
		msg->addU32Fast(_PREHASH_ObjectLocalID, object->getLocalID());
		msg->sendReliable(region->getHost());
	}

	// This will refresh dialogs.
	remove(object);

	updatePointAt();
	updateSelectionCenter();
}

void LLSelectMgr::addAsFamily(std::vector<LLViewerObject*>& objects,
							  bool add_to_end)
{
	for (std::vector<LLViewerObject*>::iterator iter = objects.begin(),
												end = objects.end();
		 iter != end; ++iter)
	{
		LLViewerObject* objectp = *iter;
		if (!objectp || objectp->isDead())
		{
			continue;
		}

		// Cannot select yourself
		if (objectp->mID == gAgentID && !mAllowSelectAvatar)
		{
			continue;
		}

		if (!objectp->isSelected())
		{
			LLSelectNode* nodep = new LLSelectNode(objectp, true);
			if (add_to_end)
			{
				mSelectedObjects->addNodeAtEnd(nodep);
			}
			else
			{
				mSelectedObjects->addNode(nodep);
			}
			objectp->setSelected(true);

			if (objectp->getNumTEs() > 0) // Make sure object got a face !
			{
				nodep->selectAllTEs(true);
				objectp->setAllTESelected(true);
			}
		}
		else
		{
			// We want this object to be selected for real so clear transient
			// flag
			LLSelectNode* select_node = mSelectedObjects->findNode(objectp);
			if (select_node)
			{
				select_node->setTransient(false);
			}
		}
	}
	saveSelectedObjectTransform(SELECT_ACTION_TYPE_PICK);
}

// Adds a single object, face, etc
void LLSelectMgr::addAsIndividual(LLViewerObject* objectp, S32 face,
								  bool undoable)
{
	// Check to see if object is already in list
	LLSelectNode* nodep = mSelectedObjects->findNode(objectp);

	// Reset in anticipation of being set to an appropriate value by panel
	// refresh, if they are up.
	setTextureChannel(LLRender::DIFFUSE_MAP);

	// If not in list, add it
	if (!nodep)
	{
		nodep = new LLSelectNode(objectp, true);
		mSelectedObjects->addNode(nodep);
		llassert_always(nodep->getObject());
	}
	else
	{
		// Make this a full-fledged selection
		nodep->setTransient(false);
		// Move it to the front of the list
		mSelectedObjects->moveNodeToFront(nodep);
	}

	// Make sure the object is tagged as selected
	objectp->setSelected(true);

	// And make sure we do not consider it as part of a family
	nodep->mIndividualSelection = true;

	// Handle face selection
	if (objectp->getNumTEs() > 0) // Make sure object got a face !
	{
		if (face == SELECT_ALL_TES)
		{
			nodep->selectAllTEs(true);
			objectp->setAllTESelected(true);
		}
		else if (face >= 0 && face < SELECT_MAX_TES)
		{
			nodep->selectTE(face, true);
			objectp->setTESelected(face, true);
		}
		else
		{
			llwarns << "Face " << face << " out of range !" << llendl;
			return;
		}
	}

	saveSelectedObjectTransform(SELECT_ACTION_TYPE_PICK);
	updateSelectionCenter();
	dialog_refresh_all();
}

LLObjectSelectionHandle LLSelectMgr::setHoverObject(LLViewerObject* objectp,
													S32 face)
{
	if (!objectp ||
		// Cannot select yourself
		(objectp->mID == gAgentID && !mAllowSelectAvatar) ||
		// Cannot select land
		(objectp->getPCode() == LLViewerObject::LL_VO_SURFACE_PATCH))
	{
		mHoverObjects->deleteAllNodes();
		return NULL;
	}

	mHoverObjects->mPrimaryObject = objectp;

	objectp = objectp->getRootEdit();

	// Is the requested object the same as the existing hover object root ?
	// NOTE: there is only ever one linked set in mHoverObjects
	if (mHoverObjects->getFirstRootObject() != objectp)
	{
		// Collect all of the objects
		std::vector<LLViewerObject*> objects;
		objectp = objectp->getRootEdit();
		objectp->addThisAndNonJointChildren(objects);

		mHoverObjects->deleteAllNodes();
		for (std::vector<LLViewerObject*>::iterator iter = objects.begin(),
													end = objects.end();
			 iter != end; ++iter)
		{
			LLViewerObject* cur_objectp = *iter;
			if (cur_objectp && !cur_objectp->isDead())
			{
				LLSelectNode* nodep = new LLSelectNode(cur_objectp, false);
				nodep->selectTE(face, true);
				mHoverObjects->addNodeAtEnd(nodep);
			}
		}

		requestObjectPropertiesFamily(objectp);
	}

	return mHoverObjects;
}

LLSelectNode* LLSelectMgr::getHoverNode()
{
	return mHoverObjects->getFirstRootNode();
}

LLSelectNode* LLSelectMgr::getPrimaryHoverNode()
{
	return mHoverObjects->mSelectNodeMap[mHoverObjects->mPrimaryObject];
}

void LLSelectMgr::highlightObjectOnly(LLViewerObject* objectp)
{
	if (!objectp)
	{
		return;
	}

	if (objectp->getPCode() != LL_PCODE_VOLUME &&
		objectp->getPCode() != LL_PCODE_LEGACY_TREE &&
		objectp->getPCode() != LL_PCODE_LEGACY_GRASS)
	{
		return;
	}

	if ((mSelectOwnedOnly && !objectp->permYouOwner()) ||
		(mSelectMovableOnly &&
		 (!objectp->permMove() || objectp->isPermanentEnforced())))
	{
		// Only select my own objects
		return;
	}

	mRectSelectedObjects.insert(objectp);
}

void LLSelectMgr::highlightObjectAndFamily(LLViewerObject* objectp)
{
	if (!objectp)
	{
		return;
	}

	LLViewerObject* root_obj = (LLViewerObject*)objectp->getRoot();

	highlightObjectOnly(root_obj);

	LLViewerObject::const_child_list_t& child_list = root_obj->getChildren();
	for (LLViewerObject::child_list_t::const_iterator
			iter = child_list.begin(), end = child_list.end();
		 iter != end; ++iter)
	{
		LLViewerObject* child = *iter;
		highlightObjectOnly(child);
	}
}

// Note that this ignores the "select owned only" flag. It is also more
// efficient than calling the single-object version over and over.
void LLSelectMgr::highlightObjectAndFamily(const std::vector<LLViewerObject*>& objects)
{
	for (std::vector<LLViewerObject*>::const_iterator
			iter1 = objects.begin(), end1 = objects.end();
		 iter1 != end1; ++iter1)
	{
		LLViewerObject* object = *iter1;

		if (!object)
		{
			continue;
		}

		LLPCode pcode = object->getPCode();
		if (pcode != LL_PCODE_VOLUME && pcode != LL_PCODE_LEGACY_TREE &&
			pcode != LL_PCODE_LEGACY_GRASS)
		{
			continue;
		}

		LLViewerObject* root = (LLViewerObject*)object->getRoot();
		mRectSelectedObjects.insert(root);

		LLViewerObject::const_child_list_t& child_list = root->getChildren();
		for (LLViewerObject::child_list_t::const_iterator
				iter2 = child_list.begin(), end2 = child_list.end();
			 iter2 != end2; ++iter2)
		{
			LLViewerObject* child = *iter2;
			mRectSelectedObjects.insert(child);
		}
	}
}

void LLSelectMgr::unhighlightObjectOnly(LLViewerObject* objectp)
{
	if (!objectp)
	{
		return;
	}

	LLPCode pcode = objectp->getPCode();
	if (pcode == LL_PCODE_VOLUME || pcode == LL_PCODE_LEGACY_TREE ||
		pcode == LL_PCODE_LEGACY_GRASS)
	{
		mRectSelectedObjects.erase(objectp);
	}
}

void LLSelectMgr::unhighlightObjectAndFamily(LLViewerObject* objectp)
{
	if (!objectp)
	{
		return;
	}

	LLViewerObject* root_obj = (LLViewerObject*)objectp->getRoot();

	unhighlightObjectOnly(root_obj);

	LLViewerObject::const_child_list_t& child_list = root_obj->getChildren();
	for (LLViewerObject::child_list_t::const_iterator
			iter = child_list.begin(), end = child_list.end();
		 iter != end; ++iter)
	{
		LLViewerObject* child = *iter;
		unhighlightObjectOnly(child);
	}
}

void LLSelectMgr::unhighlightAll()
{
	mRectSelectedObjects.clear();
	mHighlightedObjects->deleteAllNodes();
}

LLObjectSelectionHandle LLSelectMgr::selectHighlightedObjects()
{
	if (!mHighlightedObjects->getNumNodes())
	{
		return NULL;
	}

	// Clear primary object
	mSelectedObjects->mPrimaryObject = NULL;

	for (LLObjectSelection::iterator iter = getHighlightedObjects()->begin();
		 iter != getHighlightedObjects()->end(); )
	{
		LLSelectNode* nodep = *iter++;
		if (!nodep) continue;		// Paranoia

		LLViewerObject* objectp = nodep->getObject();
		if (!objectp || !canSelectObject(objectp))
		{
			continue;
		}

		// Already selected
		if (objectp->isSelected())
		{
			continue;
		}

		LLSelectNode* new_nodep = new LLSelectNode(*nodep);
		mSelectedObjects->addNode(new_nodep);

		// Flag this object as selected
		objectp->setSelected(true);
		objectp->setAllTESelected(true);

		mSelectedObjects->mSelectType = getSelectTypeForObject(objectp);

		// Request properties on root objects
		if (objectp->isRootEdit())
		{
			requestObjectPropertiesFamily(objectp);
		}
	}

	// Pack up messages to let sim know these objects are selected
	sendSelect();
	unhighlightAll();
	updateSelectionCenter();
	saveSelectedObjectTransform(SELECT_ACTION_TYPE_PICK);
	updatePointAt();

	if (mSelectedObjects->getObjectCount())
	{
		grabMenuHandler();
	}

	return mSelectedObjects;
}

void LLSelectMgr::deselectHighlightedObjects()
{
	bool select_linked_set = !mEditLinkedParts;
	for (std::set<LLPointer<LLViewerObject> >::iterator
			iter = mRectSelectedObjects.begin();
		 iter != mRectSelectedObjects.end(); ++iter)
	{
		LLViewerObject* objectp = *iter;
		if (!objectp) continue;		// Paranoia

		if (!select_linked_set)
		{
			deselectObjectOnly(objectp);
		}
		else
		{
			LLViewerObject* root_object = (LLViewerObject*)objectp->getRoot();
			if (root_object && root_object->isSelected())
			{
				deselectObjectAndFamily(root_object);
			}
		}
	}

	unhighlightAll();
}

void LLSelectMgr::addGridObject(LLViewerObject* objectp)
{
	LLSelectNode* nodep = new LLSelectNode(objectp, false);
	mGridObjects.addNodeAtEnd(nodep);

	LLViewerObject::const_child_list_t& child_list = objectp->getChildren();
	for (LLViewerObject::child_list_t::const_iterator
			iter = child_list.begin(), end = child_list.end();
		 iter != end; ++iter)
	{
		LLViewerObject* child = *iter;
		nodep = new LLSelectNode(child, false);
		mGridObjects.addNodeAtEnd(nodep);
	}
}

void LLSelectMgr::clearGridObjects()
{
	mGridObjects.deleteAllNodes();
}

void LLSelectMgr::setGridMode(EGridMode mode)
{
	mGridMode = mode;
	gSavedSettings.setS32("GridMode", mode);
	updateSelectionCenter();
}

void LLSelectMgr::getGrid(LLVector3& origin, LLQuaternion& rotation,
						  LLVector3& scale, bool for_snap_guides)
{
	mGridObjects.cleanupNodes();

	LLViewerObject* first_grid_object = mGridObjects.getFirstObject();

	if (mGridMode == GRID_MODE_LOCAL && mSelectedObjects->getObjectCount())
	{
#if 0
		LLViewerObject* root =
			getSelectedParentObject(mSelectedObjects->getFirstObject());
		mGridOrigin = mSavedSelectionBBox.getCenterAgent();
		mGridScale = mSavedSelectionBBox.getExtentLocal() * 0.5f;
		// DEV-12570 Just taking the saved selection box rotation prevents wild
		// rotations of linked sets while in local grid mode
		if (mSelectedObjects->getObjectCount() < 2 || !root ||
			root->mDrawable.isNull())
		{
			mGridRotation = mSavedSelectionBBox.getRotation();
		}
		else // set to the root object
		{
			mGridRotation = root->getRenderRotation();
		}
#else
		mGridOrigin = mSavedSelectionBBox.getCenterAgent();
		mGridScale = mSavedSelectionBBox.getExtentLocal() * 0.5f;
		mGridRotation = mSavedSelectionBBox.getRotation();
#endif
	}
	else if (mGridMode == GRID_MODE_REF_OBJECT && first_grid_object &&
			 first_grid_object->mDrawable.notNull())
	{
		LLSelectNode* node = mSelectedObjects->findNode(first_grid_object);
		if (node && !for_snap_guides)
		{
			mGridRotation = node->mSavedRotation;
		}
		else
		{
			mGridRotation = first_grid_object->getRenderRotation();
		}

		LLVector4a min_extents(F32_MAX);
		LLVector4a max_extents(-F32_MAX);
		bool grid_changed = false;
		for (LLObjectSelection::iterator iter = mGridObjects.begin(),
										 end = mGridObjects.end();
			 iter != end; ++iter)
		{
			LLViewerObject* object = (*iter)->getObject();
			if (!object) return;	// Paranoia

			LLDrawable* drawablep = object->mDrawable;
			if (drawablep)
			{
				const LLVector4a* ext = drawablep->getSpatialExtents();
				update_min_max(min_extents, max_extents, ext[0]);
				update_min_max(min_extents, max_extents, ext[1]);
				grid_changed = true;
			}
		}
		if (grid_changed)
		{
			LLVector4a center, size;
			center.setAdd(min_extents, max_extents);
			center.mul(0.5f);
			size.setSub(max_extents, min_extents);
			size.mul(0.5f);

			mGridOrigin.set(center.getF32ptr());
			LLDrawable* drawablep = first_grid_object->mDrawable;
			if (drawablep && drawablep->isActive())
			{
				mGridOrigin = mGridOrigin *
							  first_grid_object->getRenderMatrix();
			}
			mGridScale.set(size.getF32ptr());
		}
	}
	else // GRID_MODE_WORLD or just plain default
	{
		constexpr bool non_root_ok = true;
		LLViewerObject* first_obj =
			mSelectedObjects->getFirstRootObject(non_root_ok);

		mGridOrigin.clear();
		mGridRotation.loadIdentity();

		mSelectedObjects->mSelectType = getSelectTypeForObject(first_obj);

		static LLCachedControl<F32> grid_resolution(gSavedSettings,
													"GridResolution");

		switch (mSelectedObjects->mSelectType)
		{
			case SELECT_TYPE_ATTACHMENT:
			{
				if (first_obj && first_obj->getRootEdit() &&
					first_obj->getRootEdit()->mDrawable.notNull())
				{
					// This means this object *has* to be an attachment
					LLXform* attach_point_xform =
						first_obj->getRootEdit()->mDrawable->mXform.getParent();
					if (attach_point_xform)
					{
						mGridOrigin = attach_point_xform->getWorldPosition();
						mGridRotation = attach_point_xform->getWorldRotation();
					}
					F32 scale = grid_resolution;
					mGridScale.set(scale, scale, scale);
				}
				break;
			}

			case SELECT_TYPE_HUD:
			{
				F32 scale = llmin((F32)grid_resolution, 0.5f);
				mGridScale.set(scale, scale, scale);
				break;
			}

			case SELECT_TYPE_WORLD:
			{
				F32 scale = grid_resolution;
				mGridScale.set(scale, scale, scale);
			}
		}
	}
	llassert(mGridOrigin.isFinite());

	origin = mGridOrigin;
	rotation = mGridRotation;
	scale = mGridScale;
}

// Removes an array of objects
void LLSelectMgr::remove(std::vector<LLViewerObject*>& objects)
{
	for (std::vector<LLViewerObject*>::iterator iter = objects.begin(),
												end = objects.end();
		 iter != end; ++iter)
	{
		LLViewerObject* objectp = *iter;
		LLSelectNode* nodep = mSelectedObjects->findNode(objectp);
		if (nodep)
		{
			objectp->setSelected(false);
			mSelectedObjects->removeNode(nodep);
			nodep = NULL;
		}
	}
	updateSelectionCenter();
	dialog_refresh_all();
}

// Removes a single object
void LLSelectMgr::remove(LLViewerObject* objectp, S32 te, bool undoable)
{
	// Get object node (and verify it is in the selected list)
	LLSelectNode* nodep = mSelectedObjects->findNode(objectp);
	if (!nodep)
	{
		return;
	}

	// If face = all, remove object from list
	if (objectp->getNumTEs() <= 0 || te == SELECT_ALL_TES)
	{
		// Remove all faces (or the object does not have faces) so remove the
		// node.
		mSelectedObjects->removeNode(nodep);
		nodep = NULL;
		objectp->setSelected(false);
	}
	else if (0 <= te && te < SELECT_MAX_TES)
	{
		// Valid face, check to see if it was on
		if (nodep->isTESelected(te))
		{
			nodep->selectTE(te, false);
			objectp->setTESelected(te, false);
		}
		else
		{
			llwarns << "Tried to remove face " << te
					<< " that was not selected !" << llendl;
			llassert(false);
			return;
		}

		// Check to see if this operation turned off all faces
		bool found = false;
		for (S32 i = 0, count = nodep->getObject()->getNumTEs(); i < count;
			 i++)
		{
			found = found || nodep->isTESelected(i);
		}

		// ...all faces now turned off, so remove
		if (!found)
		{
			mSelectedObjects->removeNode(nodep);
			nodep = NULL;
			objectp->setSelected(false);
			// *FIXME: does not update simulator that object is no longer
			// selected
		}
	}
	else
	{
		// Face number out of range
		llwarns << "Face " << te << " out of range !" << llendl;
		llassert(false);
		return;
	}

	updateSelectionCenter();
	dialog_refresh_all();
}

void LLSelectMgr::removeAll()
{
	for (LLObjectSelection::iterator iter = mSelectedObjects->begin(),
									 end = mSelectedObjects->end();
		 iter != end; ++iter)
	{
		LLViewerObject* objectp = (*iter)->getObject();
		objectp->setSelected(false);
	}

	mSelectedObjects->deleteAllNodes();

	updateSelectionCenter();
	dialog_refresh_all();
}

void LLSelectMgr::promoteSelectionToRoot()
{
	std::vector<LLViewerObject*> selection_set;
	bool selection_changed = false;

	for (LLObjectSelection::iterator iter = getSelection()->begin();
		 iter != getSelection()->end(); )
	{
		LLSelectNode* nodep = *iter++;
		LLViewerObject* object = nodep->getObject();
		if (nodep->mIndividualSelection)
		{
			selection_changed = true;
		}

		LLViewerObject* parentp = object;
		while (parentp->getParent() && !parentp->isRootEdit())
		{
			parentp = (LLViewerObject*)parentp->getParent();
		}

		selection_set.push_back(parentp);
	}

	if (selection_changed)
	{
		deselectAll();

		for (S32 i = 0, count = selection_set.size(); i < count; ++i)
		{
			selectObjectAndFamily(selection_set[i], true);
		}
	}
}

void LLSelectMgr::demoteSelectionToIndividuals()
{
	std::vector<LLViewerObject*> objects;

	for (LLObjectSelection::root_iterator iter = getSelection()->root_begin(),
										  end = getSelection()->root_end();
		 iter != end; ++iter)
	{
		LLViewerObject* object = (*iter)->getObject();
		if (object)	// Paranoia
		{
			object->addThisAndNonJointChildren(objects);
		}
	}

	if (!objects.empty())
	{
		deselectAll();
		for (std::vector<LLViewerObject*>::iterator iter = objects.begin(),
													end = objects.end();
			 iter != end; ++iter)
		{
			LLViewerObject* objectp = *iter;
			if (objectp && !objectp->isDead())
			{
				selectObjectOnly(objectp);
			}
		}
	}
}

void LLSelectMgr::dump()
{
	llinfos << "Selection Manager: " << mSelectedObjects->getNumNodes()
			<< " items" << llendl;

	llinfos << "TE mode " << mTEMode << llendl;

	S32 count = 0;
	for (LLObjectSelection::iterator iter = getSelection()->begin(),
									 end = getSelection()->end();
		 iter != end; ++iter)
	{
		LLViewerObject* objectp = (*iter)->getObject();
		if (!objectp) continue;

		llinfos << "Object " << count++ << " type "
				<< LLPrimitive::pCodeToString(objectp->getPCode()) << llendl;
		llinfos << "  hasLSL " << objectp->flagScripted() << llendl;
		llinfos << "  hasTouch " << objectp->flagHandleTouch() << llendl;
		llinfos << "  hasMoney " << objectp->flagTakesMoney() << llendl;
		llinfos << "  getposition " << objectp->getPosition() << llendl;
		llinfos << "  getpositionAgent " << objectp->getPositionAgent()
				<< llendl;
		llinfos << "  getpositionRegion " << objectp->getPositionRegion()
				<< llendl;
		llinfos << "  getpositionGlobal " << objectp->getPositionGlobal()
				<< llendl;
		LLDrawable* drawablep = objectp->mDrawable;
		if (!drawablep) continue;

		llinfos << "  " << (drawablep->isVisible() ? "visible" : "invisible")
				<< llendl;
		llinfos << "  " << (drawablep->isState(LLDrawable::FORCE_INVISIBLE) ?
							"force_invisible" : "")
				<< llendl;
	}

	// Face iterator
	for (LLObjectSelection::iterator iter = getSelection()->begin(),
									 end = getSelection()->end();
		 iter != end; ++iter)
	{
		LLSelectNode* node = *iter;
		LLViewerObject* objectp = node->getObject();
		if (!objectp) continue;

		for (S32 te = 0, count2 = objectp->getNumTEs(); te < count2; ++te)
		{
			if (node->isTESelected(te))
			{
				llinfos << "Object " << objectp << " te " << te << llendl;
			}
		}
	}

	llinfos << mHighlightedObjects->getNumNodes()
			<< " objects currently highlighted. Center global: "
			<< mSelectionCenterGlobal << llendl;
}

void LLSelectMgr::cleanup()
{
	mSilhouetteImagep = NULL;
}

//---------------------------------------------------------------------------
// Manipulate properties of selected objects
//---------------------------------------------------------------------------

struct LLSelectMgrSendFunctor final : public LLSelectedObjectFunctor
{
	bool apply(LLViewerObject* objectp) override
	{
		if (objectp && objectp->permModify())
		{
			objectp->sendTEUpdate();
		}
		return true;
	}
};

struct DropTextureFn final : public LLSelectedTEFunctor
{
	LLViewerInventoryItem*	mItem;
	LLUUID					mTextureID;

	DropTextureFn(LLViewerInventoryItem* item, const LLUUID& id)
	:	mItem(item),
		mTextureID(id)
	{
	}

	bool apply(LLViewerObject* objectp, S32 te) override
	{
		if (!objectp || !objectp->permModify())
		{
			return false;
		}

		if (mItem)
		{
			if (objectp->isAttachment() &&
				!mItem->getPermissions().unrestricted())
			{
				// Attachments are in both in world and in inventory, which is
				// not a situation supported server side at the moment for
				// applying restricted assets.
				return false;
			}

			constexpr LLToolDragAndDrop::ESource source =
				LLToolDragAndDrop::SOURCE_AGENT;
			if (te == -1)	// All faces
			{
				LLToolDragAndDrop::dropTextureAllFaces(objectp, mItem, source);
				return true;
			}

			// One face
			LLToolDragAndDrop::dropTextureOneFace(objectp, te, mItem, source);
			return true;
		}

		// Not an inventory item, meaning a Texture picker default texture or a
		// or a local texture, so we do not need to worry about permissions for
		// them and can just apply the texture and be done with it.
		LLViewerFetchedTexture* texp =
			LLViewerTextureManager::getFetchedTexture(mTextureID,
													  FTT_DEFAULT, true,
													  LLGLTexture::BOOST_NONE,
													  LLViewerTexture::LOD_TEXTURE);
		if (texp)
		{
			objectp->setTEImage(te, texp);
		}
		return true;
	}
};

struct SendTEUpdatesFn final : public LLSelectedObjectFunctor
{
	SendTEUpdatesFn() = default;

	bool apply(LLViewerObject* objectp) override
	{
		if (!objectp || !objectp->permModify())
		{
			return false;
		}

		objectp->sendTEUpdate();
		// 1 particle effect per object
		LLHUDEffectSpiral::agentBeamToObject(objectp);
		return true;
	}
};

// *TODO: re-arch texture applying out of LLToolDragAndDrop
void LLSelectMgr::selectionSetTexture(const LLUUID& tex_id)
{
	// Check for no copy texture and/or multiple objects selection cases
	LLViewerInventoryItem* itemp = gInventory.getItem(tex_id);
	if (itemp && !itemp->getPermissions().allowCopyBy(gAgentID))
	{
		if (mSelectedObjects->getNumNodes() > 1)
		{
			llwarns << "Attempted to apply no-copy texture to multiple objects"
					<< llendl;
			return;
		}
		getSelection()->applyNoCopyTextureToTEs(itemp);
	}
	else
	{
		DropTextureFn setfunc(itemp, tex_id);
		getSelection()->applyToTEs(&setfunc);
	}

	// Nothing more to do when applying an inventory item (sendTEUpdate() and
	// particle effect already performed in LLToolDragAndDrop::dropTexture*()
	// methods)... HB
	if (!itemp)
	{
		SendTEUpdatesFn sendfunc;
		getSelection()->applyToObjects(&sendfunc);
	}
}

struct DropMaterialFn final : public LLSelectedTEFunctor
{
	LLViewerInventoryItem* mItem;
	LLUUID mMaterialID;

	DropMaterialFn(LLViewerInventoryItem* item, const LLUUID& id)
	:	mItem(item),
		mMaterialID(id)
	{
	}

	bool apply(LLViewerObject* objectp, S32 te) override
	{
		if (!objectp || !objectp->permModify())
		{
			return false;
		}

		LLUUID asset_id = mMaterialID;
		if (mItem)
		{
			if (objectp->isAttachment() &&
				!mItem->getPermissions().unrestricted())
			{
				// Attachments are in both in world and in inventory, which is
				// not a situation supported server side at the moment for
				// applying restricted assets.
				return false;
			}

			constexpr LLToolDragAndDrop::ESource source =
				LLToolDragAndDrop::SOURCE_AGENT;
			// On success, the material may be copied into the object's
			// inventory.
			if (!LLToolDragAndDrop::handleDropAssetProtections(objectp, mItem,
															   source))
			{
				return false;
			}
			asset_id = mItem->getAssetUUID();
			if (asset_id.isNull())
			{
				asset_id = BLANK_MATERIAL_ASSET_ID;
			}
		}

		// Blanks out most override data on the object and send to server.
		objectp->setRenderMaterialID(te, asset_id);
		return true;
	}
};

bool LLSelectMgr::selectionSetGLTFMaterial(const LLUUID& mat_id)
{
	LLViewerInventoryItem* itemp = NULL;
	if (mat_id.notNull())
	{
		itemp = gInventory.getItem(mat_id);
	}

	bool copy_ok = !itemp || itemp->getPermissions().allowCopyBy(gAgentID);
	if (!copy_ok && mSelectedObjects->getNumNodes() > 1)
	{
		llwarns << "Attempted to apply no-copy texture to multiple objects"
				<< llendl;
		return false;
	}

	bool success = true;
	if (!itemp ||
		(copy_ok &&
		 itemp->getPermissions().allowTransferBy(gAgentID) &&
		 itemp->getPermissions().allowModifyBy(gAgentID)))
	{
		DropMaterialFn setfunc(itemp, mat_id);
		getSelection()->applyToTEs(&setfunc);
	}
	else
	{
		success = getSelection()->applyRestrictedPbrMatToTEs(itemp);
	}

	struct func final : public LLSelectedObjectFunctor
	{
		LLViewerInventoryItem* mItem;

		func(LLViewerInventoryItem* itemp)
		:	mItem(itemp)
		{
		}

		bool apply(LLViewerObject* objectp) override
		{
			if (!objectp || !objectp->permModify())
			{
				return false;
			}

			if (mItem && objectp->isAttachment() &&
				!mItem->getPermissions().unrestricted())
			{
				// Attachments are in both in world and in inventory, which is
				// not a situation supported server side at the moment for
				// applying restricted assets.
				return false;
			}

			if (!mItem)
			{
				// 1 particle effect per object
				LLHUDEffectSpiral::agentBeamToObject(objectp);
			}

			dialog_refresh_all();
			objectp->sendTEUpdate();
			return true;
		}
	} sendfunc(itemp);
	success = success && getSelection()->applyToObjects(&sendfunc);

	LLGLTFMaterialList::flushUpdates();

	return success;
}

void LLSelectMgr::selectionSetColor(const LLColor4& color)
{
	struct f final : public LLSelectedTEFunctor
	{
		LLColor4 mColor;

		f(const LLColor4& c)
		:	mColor(c)
		{
		}

		bool apply(LLViewerObject* object, S32 te) override
		{
			if (object->permModify())
			{
				object->setTEColor(te, mColor);
			}
			return true;
		}
	} setfunc(color);
	getSelection()->applyToTEs(&setfunc);

	LLSelectMgrSendFunctor sendfunc;
	getSelection()->applyToObjects(&sendfunc);
}

void LLSelectMgr::selectionSetColorOnly(const LLColor4& color)
{
	struct f final : public LLSelectedTEFunctor
	{
		LLColor4 mColor;

		f(const LLColor4& c)
		:	mColor(c)
		{
		}

		bool apply(LLViewerObject* object, S32 te) override
		{
			if (object->permModify())
			{
				LLTextureEntry* tep = object->getTE(te);
				LLColor4 prev_color = tep ? tep->getColor() : LLColor4::white;
				mColor.mV[VALPHA] = prev_color.mV[VALPHA];
				// Update viewer side color in anticipation of update from
				// simulator
				object->setTEColor(te, mColor);
			}
			return true;
		}
	} setfunc(color);
	getSelection()->applyToTEs(&setfunc);

	LLSelectMgrSendFunctor sendfunc;
	getSelection()->applyToObjects(&sendfunc);
}

void LLSelectMgr::selectionSetAlphaOnly(F32 alpha)
{
	struct f final : public LLSelectedTEFunctor
	{
		F32 mAlpha;

		f(const F32& a)
		:	mAlpha(a)
		{
		}

		bool apply(LLViewerObject* object, S32 te) override
		{
			if (object->permModify())
			{
				LLTextureEntry* tep = object->getTE(te);
				LLColor4 prev_color = tep ? tep->getColor() : LLColor4::white;
				prev_color.mV[VALPHA] = mAlpha;
				// Update viewer side color in anticipation of update from
				// simulator
				object->setTEColor(te, prev_color);
			}
			return true;
		}
	} setfunc(alpha);
	getSelection()->applyToTEs(&setfunc);

	LLSelectMgrSendFunctor sendfunc;
	getSelection()->applyToObjects(&sendfunc);
}

void LLSelectMgr::selectionRevertColors()
{
	struct f final : public LLSelectedTEFunctor
	{
		LLObjectSelectionHandle mSelectedObjects;

		f(LLObjectSelectionHandle sel)
		:	mSelectedObjects(sel)
		{
		}

		bool apply(LLViewerObject* object, S32 te) override
		{
			if (object->permModify())
			{
				LLSelectNode* nodep = mSelectedObjects->findNode(object);
				if (nodep && te < (S32)nodep->mSavedColors.size())
				{
					LLColor4 color = nodep->mSavedColors[te];
					// Update viewer side color in anticipation of update from
					// simulator
					object->setTEColor(te, color);
				}
			}
			return true;
		}
	} setfunc(mSelectedObjects);
	getSelection()->applyToTEs(&setfunc);

	LLSelectMgrSendFunctor sendfunc;
	getSelection()->applyToObjects(&sendfunc);
}

bool LLSelectMgr::selectionRevertTextures()
{
	struct f final : public LLSelectedTEFunctor
	{
		LLObjectSelectionHandle mSelectedObjects;

		f(LLObjectSelectionHandle sel)
		:	mSelectedObjects(sel)
		{
		}

		bool apply(LLViewerObject* objectp, S32 te) override
		{
			if (!objectp || !objectp->permModify())
			{
				return true;	// Ignore no-mod primitive
			}
			LLSelectNode* nodep = mSelectedObjects->findNode(objectp);
			if (!nodep || te >= (S32)nodep->mSavedTextures.size())
			{
				return true;	// Ignore missing node or out of range te
			}
			const LLUUID& id = nodep->mSavedTextures[te];
			if (id.isNull())
			{
				// This was probably a no-copy texture, leave image as-is and
				// report a failure.
				return false;
			}

			LLViewerTexture* texp =
				LLViewerTextureManager::getFetchedTexture(id, FTT_DEFAULT,
														  true,
														  LLGLTexture::BOOST_NONE,
														  LLViewerTexture::LOD_TEXTURE);
			if (!texp)	// Paranoia ?
			{
				// Report a failure. HB
				return false;
			}
			// Update textures on viewer side
			objectp->setTEImage(te, texp);
			return true;
		}
	} setfunc(mSelectedObjects);
	bool revert_successful = getSelection()->applyToTEs(&setfunc);

	LLSelectMgrSendFunctor sendfunc;
	getSelection()->applyToObjects(&sendfunc);

	return revert_successful;
}

void LLSelectMgr::selectionRevertGLTFMaterials()
{
	struct f final : public LLSelectedTEFunctor
	{
		LLObjectSelectionHandle mSelectedObjects;

		f(LLObjectSelectionHandle sel)
		:	mSelectedObjects(sel)
		{
		}

		bool apply(LLViewerObject* objectp, S32 te) override
		{
			if (!objectp || !objectp->permModify())
			{
				return true;	// Ignore no-mod primitive
			}
			LLSelectNode* nodep = mSelectedObjects->findNode(objectp);
			if (!nodep || te >= (S32)nodep->mSavedGLTFMaterialIds.size())
			{
				return true;	// Ignore missing node or out of range te
			}

			const LLUUID& asset_id = nodep->mSavedGLTFMaterialIds[te];
			LLGLTFMaterial* matp =
				nodep->mSavedGLTFOverrideMaterials[te].get();
			// Update material locally
			objectp->setRenderMaterialID(te, asset_id, false);
			objectp->setTEGLTFMaterialOverride(te, matp);
			// Enqueue update to server
			if (asset_id.notNull())
			{
				// Restore overrides and base material
				LLGLTFMaterialList::queueApply(objectp, te, asset_id, matp);
			}
			else
			{
				// Blank override out
				LLGLTFMaterialList::queueApply(objectp, te, asset_id);
			}
			return true;
		}
	} setfunc(mSelectedObjects);
	getSelection()->applyToTEs(&setfunc);
}

void LLSelectMgr::selectionSetTexGen(U8 texgen)
{
	struct f final : public LLSelectedTEFunctor
	{
		U8 mTexgen;
		f(const U8& t) : mTexgen(t) {}
		bool apply(LLViewerObject* object, S32 te) override
		{
			if (object->permModify())
			{
				// Update viewer side tex gen in anticipation of update from
				// simulator
				object->setTETexGen(te, mTexgen);
			}
			return true;
		}
	} setfunc(texgen);
	getSelection()->applyToTEs(&setfunc);

	LLSelectMgrSendFunctor sendfunc;
	getSelection()->applyToObjects(&sendfunc);
}

void LLSelectMgr::selectionSetBumpmap(U8 bumpmap)
{
	struct f final : public LLSelectedTEFunctor
	{
		U8 mBump;
		f(const U8& b) : mBump(b) {}
		bool apply(LLViewerObject* object, S32 te) override
		{
			if (object->permModify())
			{
				// Update viewer side bump map in anticipation of update from
				// simulator
				object->setTEBumpmap(te, mBump);
			}
			return true;
		}
	} setfunc(bumpmap);
	getSelection()->applyToTEs(&setfunc);

	LLSelectMgrSendFunctor sendfunc;
	getSelection()->applyToObjects(&sendfunc);
}

void LLSelectMgr::selectionSetShiny(U8 shiny)
{
	struct f final : public LLSelectedTEFunctor
	{
		U8 mShiny;
		f(const U8& t) : mShiny(t) {}
		bool apply(LLViewerObject* object, S32 te) override
		{
			if (object->permModify())
			{
				// Update viewer side shiny in anticipation of update from
				// simulator
				object->setTEShiny(te, mShiny);
			}
			return true;
		}
	} setfunc(shiny);
	getSelection()->applyToTEs(&setfunc);

	LLSelectMgrSendFunctor sendfunc;
	getSelection()->applyToObjects(&sendfunc);
}

void LLSelectMgr::selectionSetFullbright(U8 fullbright)
{
	struct f final : public LLSelectedTEFunctor
	{
		U8 mFullbright;
		f(const U8& t) : mFullbright(t) {}
		bool apply(LLViewerObject* object, S32 te) override
		{
			if (object->permModify())
			{
				// Update viewer side full bright in anticipation of update
				// from simulator
				object->setTEFullbright(te, mFullbright);
			}
			return true;
		}
	} setfunc(fullbright);
	getSelection()->applyToTEs(&setfunc);

	struct g final : public LLSelectedObjectFunctor
	{
		U8 mFullbright;
		g(const U8& t) : mFullbright(t) {}
		bool apply(LLViewerObject* object) override
		{
			if (object->permModify())
			{
				object->sendTEUpdate();
				if (mFullbright)
				{
					U8 material = object->getMaterial();
					U8 mcode = material & LL_MCODE_MASK;
					if (mcode == LL_MCODE_LIGHT)
					{
						mcode = LL_MCODE_GLASS;
						material = (material & ~LL_MCODE_MASK) | mcode;
						object->setMaterial(material);
						object->sendMaterialUpdate();
					}
				}
			}
			return true;
		}
	} sendfunc(fullbright);
	getSelection()->applyToObjects(&sendfunc);
}

// This function expects media_data to be a map containing relevant media data
// name/value pairs (e.g. home_url, etc).
void LLSelectMgr::selectionSetMedia(U8 media_type, const LLSD& media_data)
{
	struct f final : public LLSelectedTEFunctor
	{
		U8 mMediaFlags;
		const LLSD& mMediaData;
		f(const U8& t, const LLSD& d)
		:	mMediaFlags(t),
			mMediaData(d)
		{
		}

		bool apply(LLViewerObject* object, S32 te) override
		{
			if (object && object->permModify())
			{
				// If we are adding media, then check the current state of the
				// media data on this face.
				//  - If it does not have media, AND we are NOT setting the
				//    HOME URL, then do NOT add media to this face.
				//  - If it does not have media, and we ARE setting the HOME
				//    URL, add media to this face.
				//  - If it does already have media, add/update media to/on
				//    this face
				// If we are removing media, just do it (ignore the passed-in
				// LLSD).
				if (mMediaFlags & LLTextureEntry::MF_HAS_MEDIA)
				{
					llassert(mMediaData.isMap());
					const LLTextureEntry* texture_entry = object->getTE(te);
					if (!mMediaData.isMap() ||
						(texture_entry && !texture_entry->hasMedia() &&
						 !mMediaData.has(LLMediaEntry::HOME_URL_KEY)))
					{
						// Skip adding/updating media
					}
					else
					{
						// Add/update media
						object->setTEMediaFlags(te, mMediaFlags);
						LLVOVolume* vo = object->asVolume();
						if (vo)
						{
							// 1st true = merge, 2nd true = ignore_agent
							vo->syncMediaData(te, mMediaData, true, true);
						}
						else
						{
							llwarns << "Trying to add/update media on NULL LLVOVolume (1) !"
									<< llendl;
						}
					}
				}
				else
				{
					// Delete media (or just set the flags)
					object->setTEMediaFlags(te, mMediaFlags);
				}
			}
			return true;
		}
	} setfunc(media_type, media_data);
	getSelection()->applyToTEs(&setfunc);

	struct f2 final : public LLSelectedObjectFunctor
	{
		bool apply(LLViewerObject* object) override
		{
			if (object && object->permModify())
			{
				object->sendTEUpdate();
				LLVOVolume* vo = object->asVolume();
				// It is okay to skip this object if hasMedia() is false...
				// the sendTEUpdate() above would remove all media data if it
				// were there.
				if (vo && vo->hasMedia())
				{
					// Send updated media data FOR THE ENTIRE OBJECT
					vo->sendMediaDataUpdate();
				}
				else
				{
					llwarns << "Trying to send media update for NULL LLVOVolume (2) !"
							<< llendl;
				}
			}
			return true;
		}
	} func2;
	mSelectedObjects->applyToObjects(&func2);
}

void LLSelectMgr::selectionSetGlow(F32 glow)
{
	struct f1 final : public LLSelectedTEFunctor
	{
		F32 mGlow;
		f1(F32 glow) : mGlow(glow) {}
		bool apply(LLViewerObject* object, S32 face) override
		{
			if (object->permModify())
			{
				// Update viewer side glow in anticipation of update from
				// simulator
				object->setTEGlow(face, mGlow);
			}
			return true;
		}
	} func1(glow);
	mSelectedObjects->applyToTEs(&func1);

	struct f2 final : public LLSelectedObjectFunctor
	{
		bool apply(LLViewerObject* object) override
		{
			if (object->permModify())
			{
				object->sendTEUpdate();
			}
			return true;
		}
	} func2;
	mSelectedObjects->applyToObjects(&func2);
}

void LLSelectMgr::selectionSetMaterialParams(LLSelectedTEMaterialFunctor* material_func,
											 S32 te)
{
	struct f1 final : public LLSelectedTEFunctor
	{
		LLMaterialPtr mMaterial;
		f1(LLSelectedTEMaterialFunctor* material_func, S32 te)
		:	_material_func(material_func),
			_specific_te(te)
		{
		}

		bool apply(LLViewerObject* object, S32 te) override
		{
			if (_specific_te != -1 && te != _specific_te)
			{
				return true;
			}
			if (object && object->permModify() && _material_func)
			{
				LLTextureEntry* tep = object->getTE(te);
				if (tep)
				{
					LLMaterialPtr current_material = tep->getMaterialParams();
					_material_func->apply(object, te, tep, current_material);
				}
			}
			return true;
		}

		LLSelectedTEMaterialFunctor*	_material_func;
		S32								_specific_te;
	} func1(material_func, te);
	mSelectedObjects->applyToTEs(&func1);

	struct f2 final : public LLSelectedObjectFunctor
	{
		bool apply(LLViewerObject* object) override
		{
			if (object->permModify())
			{
				object->sendTEUpdate();
			}
			return true;
		}
	} func2;
	mSelectedObjects->applyToObjects(&func2);
}

#if 1
void LLSelectMgr::selectionSetMaterials(LLMaterialPtr material)
{
	struct f1 final : public LLSelectedTEFunctor
	{
		LLMaterialPtr mMaterial;
		f1(LLMaterialPtr material) : mMaterial(material) {}
		bool apply(LLViewerObject* object, S32 face) override
		{
			if (object->permModify())
			{
				LL_DEBUGS("Materials") << "Putting material on object "
									   << object->getID() << ", face "
									   << face << ", material: "
									   << mMaterial->asLLSD() << LL_ENDL;
				LLMaterialMgr::getInstance()->put(object->getID(), face,
												  *mMaterial);
				object->setTEMaterialParams(face, mMaterial);
			}
			return true;
		}
	} func1(material);
	mSelectedObjects->applyToTEs(&func1);

	struct f2 final : public LLSelectedObjectFunctor
	{
		bool apply(LLViewerObject* object) override
		{
			if (object->permModify())
			{
				object->sendTEUpdate();
			}
			return true;
		}
	} func2;
	mSelectedObjects->applyToObjects(&func2);
}
#endif

void LLSelectMgr::selectionRemoveMaterial()
{
	struct f1 final : public LLSelectedTEFunctor
	{
		bool apply(LLViewerObject* object, S32 face) override
		{
			if (object->permModify())
			{
				LL_DEBUGS("Materials") << "Removing material from object "
									   << object->getID() << ", face " << face
									   << LL_ENDL;
				LLMaterialMgr::getInstance()->remove(object->getID(), face);
				object->setTEMaterialParams(face, NULL);
			}
			return true;
		}
	} func1;
	mSelectedObjects->applyToTEs(&func1);

	struct f2 final : public LLSelectedObjectFunctor
	{
		bool apply(LLViewerObject* object) override
		{
			if (object->permModify())
			{
				object->sendTEUpdate();
			}
			return true;
		}
	} func2;
	mSelectedObjects->applyToObjects(&func2);
}

LLPermissions* LLSelectMgr::findObjectPermissions(const LLViewerObject* objp)
{
	for (LLObjectSelection::valid_iterator
			iter = getSelection()->valid_begin(),
			end = getSelection()->valid_end();
		 iter != end; ++iter)
	{
		LLSelectNode* nodep = *iter;
		if (nodep && nodep->getObject() == objp)
		{
			return nodep->mPermissions;
		}
	}
	return NULL;
}

bool LLSelectMgr::selectionGetGlow(F32* glow)
{
	bool identical;
	F32 lglow = 0.f;
	struct f1 final : public LLSelectedTEGetFunctor<F32>
	{
		F32 get(LLViewerObject* object, S32 face) override
		{
			LLTextureEntry* tep = object->getTE(face);
			return tep ? tep->getGlow() : 0.f;
		}
	} func;
	identical = mSelectedObjects->getSelectedTEValue(&func, lglow);

	*glow = lglow;
	return identical;
}

void LLSelectMgr::selectionSetPhysicsType(U8 type)
{
	struct f final : public LLSelectedObjectFunctor
	{
		U8 mType;
		f(const U8& t) : mType(t) {}
		bool apply(LLViewerObject* object) override
		{
			if (object->permModify())
			{
				object->setPhysicsShapeType(mType);
				object->updateFlags(true);
			}
			return true;
		}
	} sendfunc(type);
	getSelection()->applyToObjects(&sendfunc);
}

void LLSelectMgr::selectionSetFriction(F32 friction)
{
	struct f final : public LLSelectedObjectFunctor
	{
		F32 mFriction;
		f(const F32& friction) : mFriction(friction) {}
		bool apply(LLViewerObject* object) override
		{
			if (object->permModify())
			{
				object->setPhysicsFriction(mFriction);
				object->updateFlags(true);
			}
			return true;
		}
	} sendfunc(friction);
	getSelection()->applyToObjects(&sendfunc);
}

void LLSelectMgr::selectionSetGravity(F32 gravity)
{
	struct f final : public LLSelectedObjectFunctor
	{
		F32 mGravity;
		f(const F32& gravity) : mGravity(gravity) {}
		bool apply(LLViewerObject* object) override
		{
			if (object->permModify())
			{
				object->setPhysicsGravity(mGravity);
				object->updateFlags(true);
			}
			return true;
		}
	} sendfunc(gravity);
	getSelection()->applyToObjects(&sendfunc);
}

void LLSelectMgr::selectionSetDensity(F32 density)
{
	struct f final : public LLSelectedObjectFunctor
	{
		F32 mDensity;
		f(const F32& density) : mDensity(density) {}
		bool apply(LLViewerObject* object) override
		{
			if (object->permModify())
			{
				object->setPhysicsDensity(mDensity);
				object->updateFlags(true);
			}
			return true;
		}
	} sendfunc(density);
	getSelection()->applyToObjects(&sendfunc);
}

void LLSelectMgr::selectionSetRestitution(F32 restitution)
{
	struct f final : public LLSelectedObjectFunctor
	{
		F32 mRestitution;
		f(const F32& restitution) : mRestitution(restitution) {}
		bool apply(LLViewerObject* object) override
		{
			if (object->permModify())
			{
				object->setPhysicsRestitution(mRestitution);
				object->updateFlags(true);
			}
			return true;
		}
	} sendfunc(restitution);
	getSelection()->applyToObjects(&sendfunc);
}

void LLSelectMgr::selectionSetMaterial(U8 material)
{
	struct f final : public LLSelectedObjectFunctor
	{
		U8 mMaterial;
		f(const U8& t) : mMaterial(t) {}
		bool apply(LLViewerObject* object) override
		{
			if (object->permModify())
			{
				U8 cur_material = object->getMaterial();
				U8 material = mMaterial | (cur_material & ~LL_MCODE_MASK);
				object->setMaterial(material);
				object->sendMaterialUpdate();
			}
			return true;
		}
	} sendfunc(material);
	getSelection()->applyToObjects(&sendfunc);
}

// true if all selected objects have this PCode
bool LLSelectMgr::selectionAllPCode(LLPCode code)
{
	struct f final : public LLSelectedObjectFunctor
	{
		LLPCode mCode;
		f(LLPCode t) : mCode(t) {}
		bool apply(LLViewerObject* object) override
		{
			if (object->getPCode() != mCode)
			{
				return false;
			}
			return true;
		}
	} func(code);
	bool res = getSelection()->applyToObjects(&func);
	return res;
}

bool LLSelectMgr::selectionGetIncludeInSearch(bool* include_in_search_out)
{
	LLViewerObject *object = mSelectedObjects->getFirstRootObject();
	if (!object) return false;

	bool include_in_search = object->getIncludeInSearch();

	bool identical = true;

	for (LLObjectSelection::root_iterator iter = getSelection()->root_begin(),
										  end = getSelection()->root_end();
		 iter != end; ++iter)
	{
		LLViewerObject* object = (*iter)->getObject();
		if (object && include_in_search != object->getIncludeInSearch())
		{
			identical = false;
			break;
		}
	}

	*include_in_search_out = include_in_search;
	return identical;
}

void LLSelectMgr::selectionSetIncludeInSearch(bool include_in_search)
{
	LLViewerObject* object = NULL;
	for (LLObjectSelection::root_iterator iter = getSelection()->root_begin(),
										  end = getSelection()->root_end();
		 iter != end; ++iter)
	{
		object = (*iter)->getObject();
		object->setIncludeInSearch(include_in_search);
	}
	sendListToRegions("ObjectIncludeInSearch", packAgentAndSessionID,
					  packObjectIncludeInSearch, &include_in_search,
					  SEND_ONLY_ROOTS);
}

bool LLSelectMgr::selectionGetClickAction(U8* out_action)
{
	LLViewerObject* object = mSelectedObjects->getFirstObject();
	if (!object)
	{
		return false;
	}

	U8 action = object->getClickAction();
	*out_action = action;

	struct f final : public LLSelectedObjectFunctor
	{
		U8 mAction;
		f(const U8& t) : mAction(t) {}
		bool apply(LLViewerObject* object) override
		{
			if (mAction != object->getClickAction())
			{
				return false;
			}
			return true;
		}
	} func(action);
	bool res = getSelection()->applyToObjects(&func);
	return res;
}

void LLSelectMgr::selectionSetClickAction(U8 action)
{
	struct f final : public LLSelectedObjectFunctor
	{
		U8 mAction;
		f(const U8& t) : mAction(t) {}
		bool apply(LLViewerObject* object) override
		{
			object->setClickAction(mAction);
			return true;
		}
	} func(action);
	getSelection()->applyToObjects(&func);

	sendListToRegions("ObjectClickAction",
					  packAgentAndSessionID,
					  packObjectClickAction,
					  &action,
					  SEND_INDIVIDUALS);
}

typedef std::pair<const std::string, const std::string> godlike_request_t;

void LLSelectMgr::sendGodlikeRequest(const std::string& request,
									 const std::string& param)
{
	// If the agent is neither godlike nor an estate owner, the server
	// will reject the request.
	std::string message_type;
	if (gAgent.isGodlike())
	{
		message_type = "GodlikeMessage";
	}
	else
	{
		message_type = "EstateOwnerMessage";
	}

	godlike_request_t data(request, param);
	if (!mSelectedObjects->getRootObjectCount())
	{
		LLMessageSystem* msg = gMessageSystemp;
		msg->newMessage(message_type.c_str());
		LLSelectMgr::packGodlikeHead(&data);
		gAgent.sendReliableMessage();
	}
	else
	{
		sendListToRegions(message_type, packGodlikeHead, packObjectIDAsParam,
						  &data, SEND_ONLY_ROOTS);
	}
}

void LLSelectMgr::packGodlikeHead(void* user_data)
{
	LLMessageSystem* msg = gMessageSystemp;
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->addUUID("TransactionID", LLUUID::null);
	godlike_request_t* data = (godlike_request_t*)user_data;
	msg->nextBlock("MethodData");
	msg->addString("Method", data->first);
	msg->addUUID("Invoice", LLUUID::null);

	// The parameters used to be restricted to either string or integer. This
	// mimics that behavior under the new 'string-only' parameter list by not
	// packing a string if there was not one specified. The object ids will be
	// packed in the packObjectIDAsParam() method.
	if (data->second.size() > 0)
	{
		msg->nextBlock("ParamList");
		msg->addString("Parameter", data->second);
	}
}

//static
void LLSelectMgr::packObjectIDAsParam(LLSelectNode* node, void*)
{
	std::string buf = llformat("%u", node->getObject()->getLocalID());
	LLMessageSystem* msg = gMessageSystemp;
	msg->nextBlock("ParamList");
	msg->addString("Parameter", buf);
}

void LLSelectMgr::selectionTexScaleAutofit(F32 repeats_per_meter)
{
	struct f final : public LLSelectedTEFunctor
	{
		F32 mRepeatsPerMeter;
		f(const F32& t) : mRepeatsPerMeter(t) {}
		bool apply(LLViewerObject* object, S32 te) override
		{

			if (object->permModify())
			{
				// Compute S,T to axis mapping
				U32 s_axis, t_axis;
				if (!LLPrimitive::getTESTAxes(te, &s_axis, &t_axis))
				{
					return true;
				}

				F32 new_s = object->getScale().mV[s_axis] * mRepeatsPerMeter;
				F32 new_t = object->getScale().mV[t_axis] * mRepeatsPerMeter;

				object->setTEScale(te, new_s, new_t);
			}
			return true;
		}
	} setfunc(repeats_per_meter);
	getSelection()->applyToTEs(&setfunc);

	LLSelectMgrSendFunctor sendfunc;
	getSelection()->applyToObjects(&sendfunc);
}

// Called at the end of a scale operation, this adjusts the textures to attempt
// to maintain a constant repeats per meter.
// BUG: Only works for flex boxes.
void LLSelectMgr::adjustTexturesByScale(bool send_to_sim, bool stretch)
{
	for (LLObjectSelection::iterator iter = getSelection()->begin(),
									 end = getSelection()->end();
		 iter != end; ++iter)
	{
		LLSelectNode* selectNode = *iter;
		LLViewerObject* object = selectNode->getObject();

		if (!object || !object->permModify() || object->getNumTEs() == 0)
		{
			continue;
		}

		bool send = false;

		for (U8 te_num = 0, count = object->getNumTEs(); te_num < count;
			 te_num++)
		{
			LLTextureEntry* tep = object->getTE(te_num);
			if (!tep) continue;

			bool planar = tep->getTexGen() == LLTextureEntry::TEX_GEN_PLANAR;
			if (planar != stretch)
			{
				continue;
			}
			// Figure out how S,T changed with scale operation
			U32 s_axis, t_axis;
			if (!LLPrimitive::getTESTAxes(te_num, &s_axis, &t_axis))
			{
				continue;
			}

			LLVector3 object_scale = object->getScale();
			LLVector3 scale_ratio = selectNode->mTextureScaleRatios[te_num];
			F32 scale_s, scale_t;
			if (planar)
			{
				scale_s = scale_ratio.mV[s_axis] / object_scale.mV[s_axis];
				scale_t = scale_ratio.mV[t_axis] / object_scale.mV[t_axis];
			}
			else
			{
				scale_s = scale_ratio.mV[s_axis] * object_scale.mV[s_axis];
				scale_t = scale_ratio.mV[t_axis] * object_scale.mV[t_axis];
			}

			// Apply new scale to face
			object->setTEScale(te_num, scale_s, scale_t);

			if (tep->getMaterialParams().notNull())
			{
				LLMaterialPtr orig = tep->getMaterialParams();
				LLMaterialPtr p =
					gFloaterToolsp->getPanelFace()->createDefaultMaterial(orig);
				p->setNormalRepeat(scale_s, scale_t);
				p->setSpecularRepeat(scale_s, scale_t);
				LLMaterialMgr::getInstance()->put(object->getID(), te_num, *p);
			}

			send = send_to_sim;
		}

		if (send)
		{
			object->sendTEUpdate();
		}
	}
}

bool LLSelectMgr::selectionIsAvatarAttachment()
{
	return mSelectedObjects->mSelectType == SELECT_TYPE_ATTACHMENT &&
		   mSelectedObjects->getObjectCount();
}

// Returns true if the viewer has information on all selected objects
bool LLSelectMgr::selectGetAllRootsValid()
{
	for (LLObjectSelection::root_iterator iter = getSelection()->root_begin(),
										  end = getSelection()->root_end();
		 iter != end; ++iter)
	{
		LLSelectNode* node = *iter;
		if (!node->mValid)
		{
			return false;
		}
	}
	return true;
}

// Returns true if the viewer has information on all selected objects
bool LLSelectMgr::selectGetAllValid()
{
	for (LLObjectSelection::iterator iter = getSelection()->begin(),
									 end = getSelection()->end();
		 iter != end; ++iter)
	{
		LLSelectNode* node = *iter;
		if (!node->mValid)
		{
			return false;
		}
	}
	return true;
}

// Returns true if current agent can modify all selected objects.
bool LLSelectMgr::selectGetModify()
{
	for (LLObjectSelection::iterator iter = getSelection()->begin(),
									 end = getSelection()->end();
		 iter != end; ++iter)
	{
		LLSelectNode* node = *iter;
		LLViewerObject* object = node->getObject();
		if (!object || !node->mValid || !object->permModify())
		{
			return false;
		}
	}
	return true;
}

// Returns true if current agent can modify all selected root objects.
bool LLSelectMgr::selectGetRootsModify()
{
	for (LLObjectSelection::root_iterator iter = getSelection()->root_begin(),
										  end = getSelection()->root_end();
		 iter != end; ++iter)
	{
		LLSelectNode* node = *iter;
		LLViewerObject* object = node->getObject();
		if (!node->mValid || !object->permModify())
		{
			return false;
		}
	}
	return true;
}

// Returns true if all objects are not permanent enforced
bool LLSelectMgr::selectGetNonPermanentEnforced()
{
	for (LLObjectSelection::iterator iter = getSelection()->begin(),
									 end = getSelection()->end();
		 iter != end; ++iter)
	{
		LLSelectNode* node = *iter;
		LLViewerObject* object = node->getObject();
		if (!node->mValid || !object || object->isPermanentEnforced())
		{
			return false;
		}
	}
	return true;
}

// Returns true if all root objects are not permanent enforced
bool LLSelectMgr::selectGetRootsNonPermanentEnforced()
{
	for (LLObjectSelection::root_iterator iter = getSelection()->root_begin(),
										  end = getSelection()->root_end();
		 iter != end; ++iter)
	{
		LLSelectNode* node = *iter;
		LLViewerObject* object = node->getObject();
		if (!node->mValid || !object || object->isPermanentEnforced())
		{
			return false;
		}
	}
	return true;
}

// Returns true if all objects are permanent
bool LLSelectMgr::selectGetPermanent()
{
	for (LLObjectSelection::iterator iter = getSelection()->begin(),
									 end = getSelection()->end();
		 iter != end; ++iter)
	{
		LLSelectNode* node = *iter;
		LLViewerObject* object = node->getObject();
		if (!node->mValid || !object || !object->flagObjectPermanent())
		{
			return false;
		}
	}
	return true;
}

// Returns true if all root objects are permanent
bool LLSelectMgr::selectGetRootsPermanent()
{
	for (LLObjectSelection::root_iterator iter = getSelection()->root_begin(),
										  end = getSelection()->root_end();
		 iter != end; ++iter)
	{
		LLSelectNode* node = *iter;
		LLViewerObject* object = node->getObject();
		if (!node->mValid || !object || !object->flagObjectPermanent())
		{
			return false;
		}
	}
	return true;
}

// Return true if all objects are characters
bool LLSelectMgr::selectGetCharacter()
{
	for (LLObjectSelection::iterator iter = getSelection()->begin(),
									 end = getSelection()->end();
		 iter != end; ++iter)
	{
		LLSelectNode* node = *iter;
		LLViewerObject* object = node->getObject();
		if (!node->mValid || !object || !object->flagCharacter())
		{
			return false;
		}
	}
	return true;
}

// Returns true if all root objects are characters
bool LLSelectMgr::selectGetRootsCharacter()
{
	for (LLObjectSelection::root_iterator iter = getSelection()->root_begin(),
										  end = getSelection()->root_end();
		 iter != end; ++iter)
	{
		LLSelectNode* node = *iter;
		LLViewerObject* object = node->getObject();
		if (!node->mValid || !object || !object->flagCharacter())
		{
			return false;
		}
	}
	return true;
}

// Returns true if all objects are not pathfinding
bool LLSelectMgr::selectGetNonPathfinding()
{
	for (LLObjectSelection::iterator iter = getSelection()->begin(),
									 end = getSelection()->end();
		 iter != end; ++iter)
	{
		LLSelectNode* node = *iter;
		LLViewerObject* object = node->getObject();
		if (!node->mValid || !object || object->flagObjectPermanent() ||
			object->flagCharacter())
		{
			return false;
		}
	}
	return true;
}

// Returns true if all root objects are not pathfinding
bool LLSelectMgr::selectGetRootsNonPathfinding()
{
	for (LLObjectSelection::root_iterator iter = getSelection()->root_begin(),
										  end = getSelection()->root_end();
		 iter != end; ++iter)
	{
		LLSelectNode* node = *iter;
		LLViewerObject* object = node->getObject();
		if (!node->mValid || !object || object->flagObjectPermanent() ||
			object->flagCharacter())
		{
			return false;
		}
	}
	return true;
}

// Return true if all objects are not permanent
bool LLSelectMgr::selectGetNonPermanent()
{
	for (LLObjectSelection::iterator iter = getSelection()->begin(),
									 end = getSelection()->end();
		 iter != end; ++iter)
	{
		LLSelectNode* node = *iter;
		LLViewerObject* object = node->getObject();
		if (!node->mValid || !object || object->flagObjectPermanent())
		{
			return false;
		}
	}
	return true;
}

// Returns true if all root objects are not permanent
bool LLSelectMgr::selectGetRootsNonPermanent()
{
	for (LLObjectSelection::root_iterator iter = getSelection()->root_begin(),
										  end = getSelection()->root_end();
		 iter != end; ++iter)
	{
		LLSelectNode* node = *iter;
		LLViewerObject* object = node->getObject();
		if (!node->mValid || !object || object->flagObjectPermanent())
		{
			return false;
		}
	}
	return true;
}

// Returns true if all objects are not character
bool LLSelectMgr::selectGetNonCharacter()
{
	for (LLObjectSelection::iterator iter = getSelection()->begin(),
									 end = getSelection()->end();
		 iter != end; ++iter)
	{
		LLSelectNode* node = *iter;
		LLViewerObject* object = node->getObject();
		if (!node->mValid || !object || object->flagCharacter())
		{
			return false;
		}
	}
	return true;
}

// Returns true if all root objects are not character
bool LLSelectMgr::selectGetRootsNonCharacter()
{
	for (LLObjectSelection::root_iterator iter = getSelection()->root_begin(),
										  end = getSelection()->root_end();
		 iter != end; ++iter)
	{
		LLSelectNode* node = *iter;
		LLViewerObject* object = node->getObject();
		if (!node->mValid || !object || object->flagCharacter())
		{
			return false;
		}
	}
	return true;
}

// Returns true if all objects are editable pathfinding linksets
bool LLSelectMgr::selectGetEditableLinksets()
{
	for (LLObjectSelection::iterator iter = getSelection()->begin(),
									 end = getSelection()->end();
		 iter != end; ++iter)
	{
		LLSelectNode* node = *iter;
		LLViewerObject* object = node->getObject();
		if (!node->mValid || !object ||
			object->flagUsePhysics() ||
			object->flagTemporaryOnRez() ||
			object->flagCharacter() ||
			object->flagVolumeDetect() ||
			object->flagAnimSource() ||
			object->getRegion() != gAgent.getRegion() ||
			(!gAgent.isGodlike() && !gAgent.canManageEstate() &&
			 !object->permYouOwner() && !object->permMove()))
		{
			return false;
		}
	}
	return true;
}

// Returns true if all objects are characters viewable within the pathfinding
// characters floater
bool LLSelectMgr::selectGetViewableCharacters()
{
	for (LLObjectSelection::iterator iter = getSelection()->begin(),
									 end = getSelection()->end();
		 iter != end; ++iter)
	{
		LLSelectNode* node = *iter;
		LLViewerObject* object = node->getObject();
		if (!node->mValid || !object || !object->flagCharacter() ||
			object->getRegion() != gAgent.getRegion())
		{
			return false;
		}
	}
	return true;
}

std::string LLSelectMgr::getPathFindingAttributeInfo(bool empty_for_none)
{
	std::string pf_attr_name;

	bool got_root_node = getSelection()->getFirstRootNode() != NULL;

	if (selectGetNonPathfinding() ||
		(got_root_node && selectGetRootsNonPathfinding()))
	{
		if (!empty_for_none)
		{
			pf_attr_name = "Pathfinding_Object_Attr_None";
		}
	}
	else if (selectGetPermanent() ||
			 (got_root_node && selectGetRootsPermanent()))
	{
		pf_attr_name = "Pathfinding_Object_Attr_Permanent";
	}
	else if (selectGetCharacter() ||
			 (got_root_node && selectGetRootsCharacter()))
	{
		pf_attr_name = "Pathfinding_Object_Attr_Character";
	}
	else
	{
		pf_attr_name = "Pathfinding_Object_Attr_MultiSelect";
	}

	return pf_attr_name.empty() ? "" : LLTrans::getString(pf_attr_name);
}

// Returns true if current agent can transfer all selected root objects.
bool LLSelectMgr::selectGetRootsTransfer()
{
	for (LLObjectSelection::root_iterator iter = getSelection()->root_begin(),
										  end = getSelection()->root_end();
		 iter != end; ++iter)
	{
		LLSelectNode* node = *iter;
		LLViewerObject* object = node->getObject();
		if (!node->mValid ||!object->permTransfer())
		{
			return false;
		}
	}
	return true;
}

// Returns true if current agent can copy all selected root objects.
bool LLSelectMgr::selectGetRootsCopy()
{
	for (LLObjectSelection::root_iterator iter = getSelection()->root_begin(),
										  end = getSelection()->root_end();
		 iter != end; ++iter)
	{
		LLSelectNode* node = *iter;
		LLViewerObject* object = node->getObject();
		if (!node->mValid || !object->permCopy())
		{
			return false;
		}
	}
	return true;
}

struct LLSelectGetFirstTest
{
	LLSelectGetFirstTest()
	:	mIdentical(true),
		mFirst(true)
	{
	}

	virtual ~LLSelectGetFirstTest() {}

	// Returns false to break out of the iteration.
	bool checkMatchingNode(LLSelectNode* node)
	{
		if (!node || !node->mValid)
		{
			return false;
		}

		if (mFirst)
		{
			mFirstValue = getValueFromNode(node);
			mFirst = false;
		}
		else
		{
			if (mFirstValue != getValueFromNode(node))
			{
				mIdentical = false;
				// Stop testing once we know not all selected are identical.
				return false;
			}
		}
		// Continue testing.
		return true;
	}

	bool mIdentical;
	LLUUID mFirstValue;

protected:
	virtual const LLUUID& getValueFromNode(LLSelectNode* node) = 0;

private:
	bool mFirst;
};

void LLSelectMgr::getFirst(LLSelectGetFirstTest* test)
{
	if (!test) return;

	if (mEditLinkedParts)
	{
		for (LLObjectSelection::valid_iterator
				iter = getSelection()->valid_begin(),
				end = getSelection()->valid_end();
			 iter != end; ++iter)
		{
			if (!test->checkMatchingNode(*iter))
			{
				break;
			}
		}
	}
	else
	{
		for (LLObjectSelection::root_object_iterator
				iter = getSelection()->root_object_begin(),
				end = getSelection()->root_object_end();
			 iter != end; ++iter)
		{
			if (!test->checkMatchingNode(*iter))
			{
				break;
			}
		}
	}
}

// Creator information only applies to root objects.
struct LLSelectGetFirstCreator final : public LLSelectGetFirstTest
{
protected:
	const LLUUID& getValueFromNode(LLSelectNode* node) override
	{
		return node->mPermissions->getCreator();
	}
};

bool LLSelectMgr::selectGetCreator(LLUUID& result_id, std::string& name)
{
	LLSelectGetFirstCreator test;
	getFirst(&test);

	if (test.mFirstValue.isNull())
	{
		name = LLTrans::getString("AvatarNameNobody");
		return false;
	}

	result_id = test.mFirstValue;

	if (!gCacheNamep)
	{
		name = "unknown";
	}
	else if (test.mIdentical)
	{
		gCacheNamep->getFullName(result_id, name);
	}
	else
	{
		name = LLTrans::getString("AvatarNameMultiple");
	}

	return test.mIdentical;
}

// Owner information only applies to roots.
struct LLSelectGetFirstOwner final : public LLSelectGetFirstTest
{
protected:
	const LLUUID& getValueFromNode(LLSelectNode* node) override
	{
		// Do not use 'getOwnership' since we return a reference, not a copy.
		// Will return LLUUID::null if unowned (which is not allowed and
		// should never happen).
		return node->mPermissions->isGroupOwned() ? node->mPermissions->getGroup()
												  : node->mPermissions->getOwner();
	}
};

bool LLSelectMgr::selectGetOwner(LLUUID& result_id, std::string& name)
{
	LLSelectGetFirstOwner test;
	getFirst(&test);

	if (test.mFirstValue.isNull())
	{
		return false;
	}

	result_id = test.mFirstValue;

	if (test.mIdentical)
	{
		bool group_owned = selectIsGroupOwned();
		if (!gCacheNamep)
		{
			name = "unknown";
		}
		else if (group_owned)
		{
			gCacheNamep->getGroupName(result_id, name);
		}
		else
		{
			gCacheNamep->getFullName(result_id, name);
		}
	}
	else
	{
		name = LLTrans::getString("AvatarNameMultiple");
	}

	return test.mIdentical;
}

// Owner information only applies to roots.
struct LLSelectGetFirstLastOwner final : public LLSelectGetFirstTest
{
protected:
	const LLUUID& getValueFromNode(LLSelectNode* node) override
	{
		return node->mPermissions->getLastOwner();
	}
};

bool LLSelectMgr::selectGetLastOwner(LLUUID& result_id, std::string& name)
{
	LLSelectGetFirstLastOwner test;
	getFirst(&test);

	if (test.mFirstValue.isNull())
	{
		return false;
	}

	result_id = test.mFirstValue;

	if (gCacheNamep && test.mIdentical)
	{
		gCacheNamep->getFullName(result_id, name);
	}
	else
	{
		name.clear();
	}

	return test.mIdentical;
}

// Group information only applies to roots.
struct LLSelectGetFirstGroup final : public LLSelectGetFirstTest
{
protected:
	const LLUUID& getValueFromNode(LLSelectNode* node) override
	{
		return node->mPermissions->getGroup();
	}
};

bool LLSelectMgr::selectGetGroup(LLUUID& result_id)
{
	LLSelectGetFirstGroup test;
	getFirst(&test);

	result_id = test.mFirstValue;
	return test.mIdentical;
}

// Only operates on root nodes. Returns true if all have valid data and they
// are all group owned.
struct LLSelectGetFirstGroupOwner final : public LLSelectGetFirstTest
{
protected:
	const LLUUID& getValueFromNode(LLSelectNode* node) override
	{
		if (node->mPermissions->isGroupOwned())
		{
			return node->mPermissions->getGroup();
		}
		return LLUUID::null;
	}
};

bool LLSelectMgr::selectIsGroupOwned()
{
	LLSelectGetFirstGroupOwner test;
	getFirst(&test);
	return test.mFirstValue.notNull();
}

// Only operates on root nodes. Returns true if all have valid data.
//  - mask_on has bits set to true where all permissions are true.
//  - mask_off has bits set to true where all permissions are false.
// If a bit is off both in mask_on and mask_off, the values differ within the
// selection.
bool LLSelectMgr::selectGetPerm(U8 which_perm, U32* mask_on, U32* mask_off)
{
	U32 mask;
	U32 mask_and = 0xffffffff;
	U32 mask_or = 0x00000000;
	bool all_valid = false;

	for (LLObjectSelection::root_iterator iter = getSelection()->root_begin(),
										  end = getSelection()->root_end();
		 iter != end; ++iter)
	{
		LLSelectNode* node = *iter;

		if (!node->mValid)
		{
			all_valid = false;
			break;
		}

		all_valid = true;

		switch (which_perm)
		{
			case PERM_BASE:
				mask = node->mPermissions->getMaskBase();
				break;

			case PERM_OWNER:
				mask = node->mPermissions->getMaskOwner();
				break;

			case PERM_GROUP:
				mask = node->mPermissions->getMaskGroup();
				break;

			case PERM_EVERYONE:
				mask = node->mPermissions->getMaskEveryone();
				break;

			case PERM_NEXT_OWNER:
				mask = node->mPermissions->getMaskNextOwner();
				break;

			default:
				mask = 0x0;
		}
		mask_and &= mask;
		mask_or |= mask;
	}

	if (all_valid)
	{
		// ...true through all ANDs means all true
		*mask_on = mask_and;

		// ...false through all ORs means all false
		*mask_off = ~mask_or;
	}
	else
	{
		*mask_on = *mask_off = 0;
	}

	return all_valid;
}

bool LLSelectMgr::selectGetPermissions(LLPermissions& result_perm)
{
	bool first = true;
	LLPermissions perm;
	for (LLObjectSelection::root_iterator
			iter = getSelection()->root_begin(),
			end = getSelection()->root_end();
		 iter != end; ++iter)
	{
		LLSelectNode* node = *iter;
		if (!node->mValid)
		{
			return false;
		}

		if (first)
		{
			perm = *(node->mPermissions);
			first = false;
		}
		else
		{
			perm.accumulate(*(node->mPermissions));
		}
	}

	result_perm = perm;

	return true;
}

void LLSelectMgr::selectDelete()
{
	bool can_delete = false;
	bool locked_but_deleteable_object = false;
	bool no_copy_but_deleteable_object = false;
	bool all_owned_by_you = true;

//MK
	// True when a sit restriction is in force and the agent is sitting
	bool is_rlv_restricted = gRLenabled &&
							 (gRLInterface.mSittpMax < EXTREMUM ||
							  (gRLInterface.mContainsUnsit &&
							   isAgentAvatarValid() &&
							   gAgentAvatarp->mIsSitting));
//mk
	for (LLObjectSelection::iterator iter = getSelection()->begin(),
									 end = getSelection()->end();
		 iter != end; ++iter)
	{
		LLViewerObject* obj = (*iter)->getObject();
		if (obj->isAttachment())
		{
			continue;
		}
//MK
		if (is_rlv_restricted && obj->isAgentSeat())
		{
			continue;
		}
//mk

		can_delete = true;

		// Check to see if you can delete objects which are locked.
		if (!obj->permMove())
		{
			locked_but_deleteable_object = true;
		}
		if (!obj->permCopy())
		{
			no_copy_but_deleteable_object = true;
		}
		if (!obj->permYouOwner())
		{
			all_owned_by_you = false;
		}
	}

	if (!can_delete)
	{
		make_ui_sound("UISndInvalidOp");
		return;
	}

	LLNotification::Params params("ConfirmObjectDeleteLock");
	params.functor(boost::bind(&LLSelectMgr::confirmDelete, _1, _2,
							   getSelection()));

	if (locked_but_deleteable_object || no_copy_but_deleteable_object ||
		!all_owned_by_you)
	{
		// Convert any transient pie-menu selections to full selection so this
		// operation has some context.
		// NOTE: if user cancels delete operation, this will potentially leave
		// objects selected outside of build mode but this is ok, if not ideal
		convertTransient();

		// This is messy, but needed to get all english our of the UI.
		if (locked_but_deleteable_object && !no_copy_but_deleteable_object &&
			all_owned_by_you)
		{
			//Locked only
			params.name("ConfirmObjectDeleteLock");
		}
		else if (!locked_but_deleteable_object &&
				 no_copy_but_deleteable_object && all_owned_by_you)
		{
			// No Copy only
			params.name("ConfirmObjectDeleteNoCopy");
		}
		else if (!locked_but_deleteable_object &&
				 !no_copy_but_deleteable_object && !all_owned_by_you)
		{
			// Not owned only
			params.name("ConfirmObjectDeleteNoOwn");
		}
		else if (locked_but_deleteable_object &&
				 no_copy_but_deleteable_object && all_owned_by_you)
		{
			// Locked and no copy
			params.name("ConfirmObjectDeleteLockNoCopy");
		}
		else if (locked_but_deleteable_object &&
				 !no_copy_but_deleteable_object && !all_owned_by_you)
		{
			// Locked and not owned
			params.name("ConfirmObjectDeleteLockNoOwn");
		}
		else if (!locked_but_deleteable_object &&
				 no_copy_but_deleteable_object && !all_owned_by_you)
		{
			// No copy and not owned
			params.name("ConfirmObjectDeleteNoCopyNoOwn");
		}
		else
		{
			// Locked, no copy and not owned
			params.name("ConfirmObjectDeleteLockNoCopyNoOwn");
		}

		gNotifications.add(params);
	}
	else
	{
		gNotifications.forceResponse(params, 0);
	}
}

//static
bool LLSelectMgr::confirmDelete(const LLSD& notification,
								const LLSD& response,
								LLObjectSelectionHandle handle)
{
	if (!handle->getObjectCount())
	{
		llwarns << "Nothing to delete !" << llendl;
	}
	else if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		make_ui_sound("UISndObjectDelete");

		// *TODO: Make sure you have delete permissions on all of them.
		const LLUUID& trash_id = gInventory.getTrashID();
		// Attempt to derez into the trash.
		LLDeRezInfo* info = new LLDeRezInfo(DRD_TRASH, trash_id);
		gSelectMgr.sendListToRegions("DeRezObject", packDeRezHeader,
									  packObjectLocalID, (void*)info,
									  SEND_ONLY_ROOTS);
		S32 objects_count = gSelectMgr.mSelectedObjects->getObjectCount();
		if (gSelectMgr.mSelectedObjects->mSelectType != SELECT_TYPE_HUD)
		{
			// VEFFECT: delete object, with one effect for all deletes
			const LLVector3d& pos = gSelectMgr.getSelectionCenterGlobal();
			F32 duration = 0.5f + (F32)objects_count / 64.f;
			LLHUDEffectSpiral::swirlAtPosition(pos, duration);
		}

		gAgent.setLookAt(LOOKAT_TARGET_CLEAR);

		// Keep track of how many objects have been deleted.
		F64 deleted =
			(F64)objects_count +
			gViewerStats.getStat(LLViewerStats::ST_OBJECT_DELETE_COUNT);
		gViewerStats.setStat(LLViewerStats::ST_OBJECT_DELETE_COUNT, deleted);
	}

	return false;
}

void LLSelectMgr::selectForceDelete()
{
	sendListToRegions("ObjectDelete", packDeleteHeader, packObjectLocalID,
					  (void*)true, SEND_ONLY_ROOTS);
}

bool LLSelectMgr::selectGetEditMoveLinksetPermissions(bool& move, bool& modify)
{
	move = modify = true;
	bool select_linked_set = !mEditLinkedParts;
//MK
	// True when a sit restriction is in force and the agent is sitting
	bool is_rlv_restricted = gRLenabled &&
							 (gRLInterface.mSittpMax < EXTREMUM ||
							  (gRLInterface.mContainsUnsit &&
							   isAgentAvatarValid() &&
							   gAgentAvatarp->mIsSitting));
//mk

	for (LLObjectSelection::root_iterator
			iter = getSelection()->root_begin(),
			end = getSelection()->root_end();
		 iter != end; ++iter)
	{
		LLSelectNode* node = *iter;
		if (!node->mValid)
		{
			move = modify = false;
			return false;
		}

		LLViewerObject* object = node->getObject();
		LLViewerObject* root_object = object ? object->getRootEdit() : NULL;
		move &= object && object->permMove() &&
				!object->isPermanentEnforced() &&
				!(root_object && root_object->isPermanentEnforced()) &&
				(select_linked_set || object->permModify());
//MK
		// Cannot edit objects that the agent is sitting on when sit-restricted
		if (is_rlv_restricted && object && object->isAgentSeat())
		{
			move = false;
		}
//mk
		modify &= object->permModify();
	}

	return true;
}

void LLSelectMgr::selectGetAggregateSaleInfo(U32& num_for_sale,
											 bool& is_for_sale_mixed,
											 bool& is_sale_price_mixed,
											 S32& total_sale_price,
											 S32& individual_sale_price)
{
	num_for_sale = 0;
	is_for_sale_mixed = false;
	is_sale_price_mixed = false;
	total_sale_price = 0;
	individual_sale_price = 0;

	// Empty set.
	if (getSelection()->root_begin() == getSelection()->root_end())
	{
		return;
	}

	LLSelectNode* node = *(getSelection()->root_begin());
	const bool first_node_for_sale = node->mSaleInfo.isForSale();
	const S32 first_node_sale_price = node->mSaleInfo.getSalePrice();

	for (LLObjectSelection::root_iterator
			iter = getSelection()->root_begin(),
			end = getSelection()->root_end();
		 iter != end; ++iter)
	{
		LLSelectNode* node = *iter;
		const bool node_for_sale = node->mSaleInfo.isForSale();
		const S32 node_sale_price = node->mSaleInfo.getSalePrice();

		// Set mixed if the fields do not match the first node's fields.
		if (node_for_sale != first_node_for_sale)
		{
			is_for_sale_mixed = true;
		}
		if (node_sale_price != first_node_sale_price)
		{
			is_sale_price_mixed = true;
		}

		if (node_for_sale)
		{
			total_sale_price += node_sale_price;
			++num_for_sale;
		}
	}

	individual_sale_price = first_node_sale_price;
	if (is_for_sale_mixed)
	{
		is_sale_price_mixed = true;
		individual_sale_price = 0;
	}
}

// returns true if all nodes are valid. method also stores an
// accumulated sale info.
bool LLSelectMgr::selectGetSaleInfo(LLSaleInfo& result_sale_info)
{
	bool first = true;
	LLSaleInfo sale_info;
	for (LLObjectSelection::root_iterator
			iter = getSelection()->root_begin(),
			end = getSelection()->root_end();
		 iter != end; ++iter)
	{
		LLSelectNode* node = *iter;
		if (!node->mValid)
		{
			return false;
		}

		if (first)
		{
			sale_info = node->mSaleInfo;
			first = false;
		}
		else
		{
			sale_info.accumulate(node->mSaleInfo);
		}
	}

	result_sale_info = sale_info;

	return true;
}

bool LLSelectMgr::selectGetAggregatePermissions(LLAggregatePermissions& result_perm)
{
	bool first = true;
	LLAggregatePermissions perm;
	for (LLObjectSelection::root_iterator
			iter = getSelection()->root_begin(),
			end = getSelection()->root_end();
		 iter != end; ++iter)
	{
		LLSelectNode* node = *iter;
		if (!node->mValid)
		{
			return false;
		}

		if (first)
		{
			perm = node->mAggregatePerm;
			first = false;
		}
		else
		{
			perm.aggregate(node->mAggregatePerm);
		}
	}

	result_perm = perm;

	return true;
}

bool LLSelectMgr::selectGetAggregateTexturePermissions(LLAggregatePermissions& result_perm)
{
	bool first = true;
	LLAggregatePermissions perm;
	for (LLObjectSelection::root_iterator
			iter = getSelection()->root_begin(),
			end = getSelection()->root_end();
		 iter != end; ++iter)
	{
		LLSelectNode* node = *iter;
		if (!node->mValid)
		{
			return false;
		}

		LLAggregatePermissions t_perm =
			node->getObject()->permYouOwner() ? node->mAggregateTexturePermOwner
											  : node->mAggregateTexturePerm;
		if (first)
		{
			perm = t_perm;
			first = false;
		}
		else
		{
			perm.aggregate(t_perm);
		}
	}

	result_perm = perm;

	return true;
}

//-----------------------------------------------------------------------------
// Duplicate objects
//-----------------------------------------------------------------------------

// JC - If this does not work right, duplicate the selection list before doing
// anything, do a deselect, then send the duplicate messages.
struct LLDuplicateData
{
	LLVector3	offset;
	U32			flags;
};

void LLSelectMgr::selectDuplicate(const LLVector3& offset, bool select_copy)
{
	if (mSelectedObjects->isAttachment())
	{
		// RN: do not duplicate attachments
		make_ui_sound("UISndInvalidOp");
		return;
	}
	LLDuplicateData	data;

	data.offset = offset;
	data.flags = (select_copy ? FLAGS_CREATE_SELECTED : 0x0);

	sendListToRegions("ObjectDuplicate", packDuplicateHeader, packDuplicate,
					   &data, SEND_ONLY_ROOTS);

	if (select_copy)
	{
		// The new copy will be coming in selected
		deselectAll();
	}
	else
	{
		for (LLObjectSelection::root_iterator iter = getSelection()->root_begin();
			 iter != getSelection()->root_end(); ++iter)
		{
			LLSelectNode* node = *iter;
			node->mDuplicated = true;
			node->mDuplicatePos = node->getObject()->getPositionGlobal();
			node->mDuplicateRot = node->getObject()->getRotation();
		}
	}
}

void LLSelectMgr::repeatDuplicate()
{
	if (mSelectedObjects->isAttachment())
	{
		// RN: do not duplicate attachments
		make_ui_sound("UISndInvalidOp");
		return;
	}

	std::vector<LLViewerObject*> non_duplicated_objects;

	for (LLObjectSelection::root_iterator
			iter = getSelection()->root_begin(),
			end = getSelection()->root_end();
		 iter != end; ++iter)
	{
		LLSelectNode* node = *iter;
		if (!node->mDuplicated)
		{
			non_duplicated_objects.push_back(node->getObject());
		}
	}

	// make sure only previously duplicated objects are selected
	for (std::vector<LLViewerObject*>::iterator
			iter = non_duplicated_objects.begin(),
			end = non_duplicated_objects.end();
		 iter != end; ++iter)
	{
		LLViewerObject* objectp = *iter;
		deselectObjectAndFamily(objectp);
	}

	// duplicate objects in place
	LLDuplicateData	data;

	data.offset = LLVector3::zero;
	data.flags = 0x0;

	sendListToRegions("ObjectDuplicate", packDuplicateHeader, packDuplicate,
					   &data, SEND_ONLY_ROOTS);

	// Move current selection based on delta from duplication position and
	// update duplication position
	for (LLObjectSelection::root_iterator
			iter = getSelection()->root_begin(),
			end = getSelection()->root_end();
		 iter != end; ++iter)
	{
		LLSelectNode* node = *iter;
		if (node->mDuplicated)
		{
			LLQuaternion cur_rot = node->getObject()->getRotation();
			LLQuaternion rot_delta = (~node->mDuplicateRot * cur_rot);
			LLQuaternion new_rot = cur_rot * rot_delta;
			LLVector3d cur_pos = node->getObject()->getPositionGlobal();
			LLVector3d new_pos = cur_pos + (cur_pos - node->mDuplicatePos) * rot_delta;

			node->mDuplicatePos = node->getObject()->getPositionGlobal();
			node->mDuplicateRot = node->getObject()->getRotation();
			node->getObject()->setPositionGlobal(new_pos);
			node->getObject()->setRotation(new_rot);
		}
	}

	sendMultipleUpdate(UPD_ROTATION | UPD_POSITION);
}

//static
void LLSelectMgr::packDuplicate(LLSelectNode* node, void* duplicate_data)
{
	LLMessageSystem* msg = gMessageSystemp;
	msg->nextBlockFast(_PREHASH_ObjectData);
	msg->addU32Fast(_PREHASH_ObjectLocalID, node->getObject()->getLocalID());
}

//-----------------------------------------------------------------------------
// Duplicate On Ray
//-----------------------------------------------------------------------------

// Duplicates the selected objects, but places the copy along a cast
// ray.
struct LLDuplicateOnRayData
{
	LLVector3	mRayStartRegion;
	LLVector3	mRayEndRegion;
	bool		mBypassRaycast;
	bool		mRayEndIsIntersection;
	LLUUID		mRayTargetID;
	bool		mCopyCenters;
	bool		mCopyRotates;
	U32			mFlags;
};

void LLSelectMgr::selectDuplicateOnRay(const LLVector3& ray_start_region,
									   const LLVector3& ray_end_region,
									   bool bypass_raycast,
									   bool ray_end_is_intersection,
									   const LLUUID& ray_target_id,
									   bool copy_centers,
									   bool copy_rotates,
									   bool select_copy)
{
	if (mSelectedObjects->isAttachment())
	{
		// Do not duplicate attachments
		make_ui_sound("UISndInvalidOp");
		return;
	}

	LLDuplicateOnRayData data;

	data.mRayStartRegion = ray_start_region;
	data.mRayEndRegion = ray_end_region;
	data.mBypassRaycast = bypass_raycast;
	data.mRayEndIsIntersection = ray_end_is_intersection;
	data.mRayTargetID = ray_target_id;
	data.mCopyCenters = copy_centers;
	data.mCopyRotates = copy_rotates;
	data.mFlags = select_copy ? FLAGS_CREATE_SELECTED : 0x0;

	sendListToRegions("ObjectDuplicateOnRay", packDuplicateOnRayHead,
					  packObjectLocalID, &data, SEND_ONLY_ROOTS);

	if (select_copy)
	{
		// the new copy will be coming in selected
		deselectAll();
	}
}

//static
void LLSelectMgr::packDuplicateOnRayHead(void* user_data)
{
	LLMessageSystem* msg = gMessageSystemp;
	LLDuplicateOnRayData* data = (LLDuplicateOnRayData*)user_data;

	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	LLUUID group_id = gAgent.getGroupID();
	if (gSavedSettings.getBool("RezWithLandGroup"))
	{
		LLParcel* parcel = gViewerParcelMgr.getAgentParcel();
		if (gAgent.isInGroup(parcel->getGroupID()))
		{
			group_id = parcel->getGroupID();
		}
		else if (gAgent.isInGroup(parcel->getOwnerID()))
		{
			group_id = parcel->getOwnerID();
		}
	}
	msg->addUUIDFast(_PREHASH_GroupID, group_id);
	msg->addVector3Fast(_PREHASH_RayStart, data->mRayStartRegion);
	msg->addVector3Fast(_PREHASH_RayEnd, data->mRayEndRegion);
	msg->addBoolFast(_PREHASH_BypassRaycast, data->mBypassRaycast);
	msg->addBoolFast(_PREHASH_RayEndIsIntersection, data->mRayEndIsIntersection);
	msg->addBoolFast(_PREHASH_CopyCenters, data->mCopyCenters);
	msg->addBoolFast(_PREHASH_CopyRotates, data->mCopyRotates);
	msg->addUUIDFast(_PREHASH_RayTargetID, data->mRayTargetID);
	msg->addU32Fast(_PREHASH_DuplicateFlags, data->mFlags);
}

//-----------------------------------------------------------------------------
// Object position, scale, rotation update, all-in-one
//-----------------------------------------------------------------------------

void LLSelectMgr::sendMultipleUpdate(U32 type)
{
	if (type == UPD_NONE) return;

	// Send individual updates when selecting textures or individual objects
	ESendType send_type = !mEditLinkedParts && !getTEMode() ? SEND_ONLY_ROOTS
															: SEND_ROOTS_FIRST;
	if (send_type == SEND_ONLY_ROOTS)
	{
		// Tell simulator to apply to whole linked sets
		type |= UPD_LINKED_SETS;
	}

	sendListToRegions("MultipleObjectUpdate", packAgentAndSessionID,
					  packMultipleUpdate, &type, send_type);
}

//static
void LLSelectMgr::packMultipleUpdate(LLSelectNode* node, void* user_data)
{
	LLViewerObject* object = node->getObject();
	U32* type32 = (U32*)user_data;
	U8 type = (U8)*type32;
	U8	data[256];

	LLMessageSystem* msg = gMessageSystemp;
	msg->nextBlockFast(_PREHASH_ObjectData);
	msg->addU32Fast(_PREHASH_ObjectLocalID,	object->getLocalID());
	msg->addU8Fast(_PREHASH_Type, type);

	S32 offset = 0;

	// JC: You MUST pack the data in this order.  The receiving
	// routine process_multiple_update_message on simulator will
	// extract them in this order.

	if (type & UPD_POSITION)
	{
		htonmemcpy(&data[offset], &(object->getPosition().mV), MVT_LLVector3, 12);
		offset += 12;
	}
	if (type & UPD_ROTATION)
	{
		LLQuaternion quat = object->getRotation();
		LLVector3 vec = quat.packToVector3();
		htonmemcpy(&data[offset], &(vec.mV), MVT_LLQuaternion, 12);
		offset += 12;
	}
	if (type & UPD_SCALE)
	{
		//llinfos << "Sending object scale " << object->getScale() << llendl;
		htonmemcpy(&data[offset], &(object->getScale().mV), MVT_LLVector3, 12);
		offset += 12;
	}
	msg->addBinaryDataFast(_PREHASH_Data, data, offset);
}

//-----------------------------------------------------------------------------
// Ownership
//-----------------------------------------------------------------------------
struct LLOwnerData
{
	LLUUID	owner_id;
	LLUUID	group_id;
	bool	do_override;
};

void LLSelectMgr::sendOwner(const LLUUID& owner_id,
							const LLUUID& group_id,
							bool do_override)
{
	LLOwnerData data;
	data.owner_id = owner_id;
	data.group_id = group_id;
	data.do_override = do_override;

	sendListToRegions("ObjectOwner", packOwnerHead, packObjectLocalID, &data,
					  SEND_ONLY_ROOTS);
}

//static
void LLSelectMgr::packOwnerHead(void* user_data)
{
	LLOwnerData* data = (LLOwnerData*)user_data;

	LLMessageSystem* msg = gMessageSystemp;
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlockFast(_PREHASH_HeaderData);
	msg->addBoolFast(_PREHASH_Override, data->do_override);
	msg->addUUIDFast(_PREHASH_OwnerID, data->owner_id);
	msg->addUUIDFast(_PREHASH_GroupID, data->group_id);
}

//-----------------------------------------------------------------------------
// Group
//-----------------------------------------------------------------------------

void LLSelectMgr::sendGroup(const LLUUID& group_id)
{
	LLUUID local_group_id(group_id);
	sendListToRegions("ObjectGroup", packAgentAndSessionAndGroupID,
					  packObjectLocalID, &local_group_id, SEND_ONLY_ROOTS);
}

//-----------------------------------------------------------------------------
// Buy
//-----------------------------------------------------------------------------

struct LLBuyData
{
	std::vector<LLViewerObject*> mObjectsSent;
	LLUUID mCategoryID;
	LLSaleInfo mSaleInfo;
};

// *NOTE: does not work for multiple objects buy, which UI does not currently
// support; sale info is used for verification only, if it does not match
// region info then the sale is cancelled. We need to get sale info -as
// displayed in the UI- for every item.
void LLSelectMgr::sendBuy(const LLUUID& buyer_id, const LLUUID& category_id,
						  const LLSaleInfo sale_info)
{
	LLBuyData buy;
	buy.mCategoryID = category_id;
	buy.mSaleInfo = sale_info;
	sendListToRegions("ObjectBuy", packAgentGroupAndCatID, packBuyObjectIDs,
					  &buy, SEND_ONLY_ROOTS);
}

//static
void LLSelectMgr::packBuyObjectIDs(LLSelectNode* node, void* data)
{
	LLBuyData* buy = (LLBuyData*)data;

	LLViewerObject* object = node->getObject();
	if (std::find(buy->mObjectsSent.begin(), buy->mObjectsSent.end(),
				  object) == buy->mObjectsSent.end())
	{
		buy->mObjectsSent.push_back(object);
		LLMessageSystem* msg = gMessageSystemp;
		msg->nextBlockFast(_PREHASH_ObjectData);
		msg->addU32Fast(_PREHASH_ObjectLocalID, object->getLocalID());
		msg->addU8Fast(_PREHASH_SaleType, buy->mSaleInfo.getSaleType());
		msg->addS32Fast(_PREHASH_SalePrice, buy->mSaleInfo.getSalePrice());
	}
}

//-----------------------------------------------------------------------------
// Permissions
//-----------------------------------------------------------------------------

struct LLPermData
{
	U8 mField;
	U32 mMask;
	bool mSet;
	bool mOverride;
};

// *TODO: Make this able to fail elegantly.
void LLSelectMgr::selectionSetObjectPermissions(U8 field, bool set, U32 mask,
									   			bool do_override)
{
	LLPermData data;
	data.mField = field;
	data.mSet = set;
	data.mMask = mask;
	data.mOverride = do_override;

	sendListToRegions("ObjectPermissions", packPermissionsHead,
					  packPermissions, &data, SEND_ONLY_ROOTS);
}

void LLSelectMgr::packPermissionsHead(void* user_data)
{
	LLPermData* data = (LLPermData*)user_data;
	LLMessageSystem* msg = gMessageSystemp;
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlockFast(_PREHASH_HeaderData);
	msg->addBoolFast(_PREHASH_Override, data->mOverride);
}

void LLSelectMgr::deselectAll()
{
	if (mSelectedObjects->getNumNodes())
	{
		deselectAllForStandingUp();
	}
}

// This method is similar deselectAll() except for the if statement which was
// removed; this is needed as a workaround for DEV-2854.
void LLSelectMgr::deselectAllForStandingUp()
{
	// Zap the angular velocity, as the sim will set it to zero
	for (LLObjectSelection::iterator iter = mSelectedObjects->begin(),
									 end = mSelectedObjects->end();
		 iter != end; ++iter)
	{
		LLViewerObject* objectp = (*iter)->getObject();
#if 0	// Do not do that: it breaks target-omega seats !
		objectp->setAngularVelocity(0.f, 0.f, 0.f);
#endif
		objectp->setVelocity(0.f, 0.f, 0.f);
	}

	sendListToRegions("ObjectDeselect", packAgentAndSessionID,
					  packObjectLocalID, NULL, SEND_INDIVIDUALS);

	removeAll();

	mLastSentSelectionCenterGlobal.clear();

	updatePointAt();
}

void LLSelectMgr::deselectUnused()
{
	// no more outstanding references to this selection
	if (mSelectedObjects->getNumRefs() == 1)
	{
		deselectAll();
	}
}

void LLSelectMgr::convertTransient()
{
	for (LLObjectSelection::iterator node_it = mSelectedObjects->begin(),
									 end = mSelectedObjects->end();
		 node_it != end; ++node_it)
	{
		LLSelectNode* nodep = *node_it;
		nodep->setTransient(false);
	}
}

void LLSelectMgr::deselectAllIfTooFar()
{
	if (mSelectedObjects->isEmpty() ||
		mSelectedObjects->mSelectType == SELECT_TYPE_HUD)
	{
		return;
	}

	// Do not deselect when we are navigating an object's pie menu or when we
	// are quitting the application (with gPieObjectp deleted)
	if (!gPieObjectp || gPieObjectp->getVisible())
	{
		return;
	}

//MK
	if (gRLenabled && gRLInterface.mContainsInteract)
	{
		deselectAll();
		return;
	}
//mk

	LLVector3d selectionCenter = getSelectionCenterGlobal();
	static LLCachedControl<bool> limit_select_distance(gSavedSettings,
													   "LimitSelectDistance");
	static LLCachedControl<F32> max_select_distance(gSavedSettings,
													"MaxSelectDistance");
	if (limit_select_distance &&
		(!mSelectedObjects->getPrimaryObject() ||
		 !mSelectedObjects->getPrimaryObject()->isAvatar()) &&
		!mSelectedObjects->isAttachment() &&
		!selectionCenter.isExactlyZero())
	{
		F32 deselect_dist_sq = max_select_distance * max_select_distance;

		LLVector3d select_delta = gAgent.getPositionGlobal() - selectionCenter;
		F32 select_dist_sq = (F32) select_delta.lengthSquared();

		if (select_dist_sq > deselect_dist_sq)
		{
			if (mDebugSelectMgr)
			{
				llinfos << "Selection manager: auto-deselecting, select_dist = "
						<< sqrtf(select_dist_sq)
						<< " - agent pos global = "
						<< gAgent.getPositionGlobal()
						<< " - selection pos global = "
						<< selectionCenter << llendl;
			}

			deselectAll();
		}
	}
}

void LLSelectMgr::selectionSetObjectName(const std::string& name)
{
	// we only work correctly if 1 object is selected.
	if (mSelectedObjects->getRootObjectCount() == 1)
	{
		sendListToRegions("ObjectName", packAgentAndSessionID, packObjectName,
						  (void*)(new std::string(name)), SEND_ONLY_ROOTS);
	}
	else if (mSelectedObjects->getObjectCount() == 1)
	{
		sendListToRegions("ObjectName", packAgentAndSessionID, packObjectName,
						  (void*)(new std::string(name)), SEND_INDIVIDUALS);
	}
}

void LLSelectMgr::selectionSetObjectDescription(const std::string& desc)
{
	// we only work correctly if 1 object is selected.
	if (mSelectedObjects->getRootObjectCount() == 1)
	{
		sendListToRegions("ObjectDescription", packAgentAndSessionID,
						  packObjectDescription,
						  (void*)(new std::string(desc)), SEND_ONLY_ROOTS);
	}
	else if (mSelectedObjects->getObjectCount() == 1)
	{
		sendListToRegions("ObjectDescription", packAgentAndSessionID,
						  packObjectDescription,
						  (void*)(new std::string(desc)), SEND_INDIVIDUALS);
	}
}

void LLSelectMgr::selectionSetObjectCategory(const LLCategory& category)
{
	// for now, we only want to be able to set one root category at
	// a time.
	if (mSelectedObjects->getRootObjectCount() != 1) return;
	sendListToRegions("ObjectCategory", packAgentAndSessionID,
					  packObjectCategory, (void*)&category, SEND_ONLY_ROOTS);
}

void LLSelectMgr::selectionSetObjectSaleInfo(const LLSaleInfo& sale_info)
{
	sendListToRegions("ObjectSaleInfo", packAgentAndSessionID,
					  packObjectSaleInfo, (void*)&sale_info, SEND_ONLY_ROOTS);
}

//----------------------------------------------------------------------
// Attachments
//----------------------------------------------------------------------

void LLSelectMgr::sendAttach(U8 attachment_point)
{
	LLViewerObject* attach_object = mSelectedObjects->getFirstRootObject();

	if (!attach_object || !isAgentAvatarValid() ||
		mSelectedObjects->mSelectType != SELECT_TYPE_WORLD)
	{
		return;
	}

	bool build_mode = gToolMgr.inEdit();
	// Special case: Attach to default location for this object.
	if (attachment_point == 0 ||
		get_ptr_in_map(gAgentAvatarp->mAttachmentPoints,
					   (S32)attachment_point))
	{
		if (attachment_point != 0)
		{
			// If we know the attachment point then we got here by clicking an
			// "Attach to..." context menu item, so we should add, not replace.
			attachment_point |= ATTACHMENT_ADD;
		}

		sendListToRegions("ObjectAttach", packAgentIDAndSessionAndAttachment,
						  packObjectIDAndRotation, &attachment_point,
						  SEND_ONLY_ROOTS);
		if (!build_mode)
		{
			// After "ObjectAttach" server will unsubscribe us from properties
			// updates so either deselect objects or resend selection after
			// attach packet reaches server In case of build_mode == true,
			// LLPanelInventory::refresh() will deal with selection.
			deselectAll();
		}
	}
}

void LLSelectMgr::sendDetach()
{
	if (!mSelectedObjects->getNumNodes() ||
		mSelectedObjects->mSelectType == SELECT_TYPE_WORLD)
	{
		return;
	}

	sendListToRegions("ObjectDetach", packAgentAndSessionID, packObjectLocalID,
					  NULL, SEND_ONLY_ROOTS);
}

void LLSelectMgr::sendDropAttachment()
{
	if (!mSelectedObjects->getNumNodes() ||
		mSelectedObjects->mSelectType == SELECT_TYPE_WORLD)
	{
		return;
	}

	sendListToRegions("ObjectDrop", packAgentAndSessionID, packObjectLocalID,
					  NULL, SEND_ONLY_ROOTS);
}

//----------------------------------------------------------------------
// Links
//----------------------------------------------------------------------

void LLSelectMgr::sendLink()
{
	if (!mSelectedObjects->getNumNodes())
	{
		return;
	}

	sendListToRegions("ObjectLink", packAgentAndSessionID, packObjectLocalID,
					  NULL, SEND_ONLY_ROOTS);
}

void LLSelectMgr::sendDelink()
{
	if (!mSelectedObjects->getNumNodes())
	{
		return;
	}

	struct f final : public LLSelectedObjectFunctor
	{
		f() {}

		bool apply(LLViewerObject* object) override
		{
			if (object->permModify())
			{
				if (object->getPhysicsShapeType() == LLViewerObject::PHYSICS_SHAPE_NONE)
				{
					object->setPhysicsShapeType(LLViewerObject::PHYSICS_SHAPE_CONVEX_HULL);
					object->updateFlags();
				}
			}
			return true;
		}
	} sendfunc;
	getSelection()->applyToObjects(&sendfunc);

	// Delink needs to send individuals so you can unlink a single object from
	// a linked set.
	sendListToRegions("ObjectDelink", packAgentAndSessionID, packObjectLocalID,
					  NULL, SEND_INDIVIDUALS);
}

#ifdef SEND_HINGES
//----------------------------------------------------------------------
// Hinges
//----------------------------------------------------------------------

void LLSelectMgr::sendHinge(U8 type)
{
	if (!mSelectedObjects->getNumNodes())
	{
		return;
	}

	sendListToRegions("ObjectHinge", packHingeHead, packObjectLocalID, &type,
					  SEND_ONLY_ROOTS);
}

void LLSelectMgr::sendDehinge()
{
	if (!mSelectedObjects->getNumNodes())
	{
		return;
	}

	sendListToRegions("ObjectDehinge", packAgentAndSessionID, packObjectLocalID,
					  NULL, SEND_ONLY_ROOTS);
}

//static
void LLSelectMgr::packHingeHead(void* user_data)
{
	U8* type = (U8*)user_data;

	LLMessageSystem* msg = gMessageSystemp;
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlockFast(_PREHASH_JointType);
	msg->addU8Fast(_PREHASH_Type, *type);
}
#endif

void LLSelectMgr::sendSelect()
{
	if (mSelectedObjects->getNumNodes())
	{
		sendListToRegions("ObjectSelect", packAgentAndSessionID,
						  packObjectLocalID, NULL, SEND_INDIVIDUALS);
	}
}

void LLSelectMgr::selectionDump()
{
	struct f final : public LLSelectedObjectFunctor
	{
		bool apply(LLViewerObject* object) override
		{
			object->dump();
			return true;
		}
	} func;
	getSelection()->applyToObjects(&func);
}

void LLSelectMgr::saveSelectedObjectColors()
{
	struct f final : public LLSelectedNodeFunctor
	{
		bool apply(LLSelectNode* node) override
		{
			node->saveColors();
			return true;
		}
	} func;
	getSelection()->applyToNodes(&func);
}

void LLSelectMgr::saveSelectedObjectTextures()
{
	// Invalidate current selection so we update saved textures
	struct f final : public LLSelectedNodeFunctor
	{
		bool apply(LLSelectNode* node) override
		{
			node->mValid = false;
			return true;
		}
	} func;
	getSelection()->applyToNodes(&func);

	// Request object properties message to get updated permissions data
	sendSelect();
}

// This routine should be called whenever a drag is initiated.
// Also need to know to which simulator to send update message
void LLSelectMgr::saveSelectedObjectTransform(EActionType action_type)
{
	if (mSelectedObjects->isEmpty())
	{
		// nothing selected, so nothing to save
		return;
	}

	struct f final : public LLSelectedNodeFunctor
	{
		EActionType mActionType;
		LLSelectMgr* mManager;
		f(EActionType a, LLSelectMgr* p) : mActionType(a), mManager(p) {}
		bool apply(LLSelectNode* selectNode) override
		{
			LLViewerObject*	object = selectNode->getObject();
			if (!object)
			{
				return true; // skip
			}
			selectNode->mSavedPositionLocal = object->getPosition();
			if (object->isAttachment())
			{
				if (object->isRootEdit())
				{
					LLXform* parent_xform =
						object->mDrawable->getXform()->getParent();
					if (parent_xform)
					{
						selectNode->mSavedPositionGlobal =
							gAgent.getPosGlobalFromAgent((object->getPosition() *
														  parent_xform->getWorldRotation()) +
														 parent_xform->getWorldPosition());
					}
					else
					{
						selectNode->mSavedPositionGlobal = object->getPositionGlobal();
					}
				}
				else
				{
					LLViewerObject* attachment_root =
						(LLViewerObject*)object->getParent();
					LLXform* parent_xform =
						attachment_root ? attachment_root->mDrawable->getXform()->getParent()
										: NULL;
					if (parent_xform)
					{
						LLVector3 root_pos = (attachment_root->getPosition() *
											  parent_xform->getWorldRotation()) +
											 parent_xform->getWorldPosition();
						LLQuaternion root_rot =
							attachment_root->getRotation() *
							parent_xform->getWorldRotation();
						selectNode->mSavedPositionGlobal =
							gAgent.getPosGlobalFromAgent((object->getPosition() *
														  root_rot) +
														 root_pos);
					}
					else
					{
						selectNode->mSavedPositionGlobal = object->getPositionGlobal();
					}
				}
				selectNode->mSavedRotation = object->getRenderRotation();
			}
			else
			{
				selectNode->mSavedPositionGlobal = object->getPositionGlobal();
				selectNode->mSavedRotation = object->getRotationRegion();
			}

			selectNode->mSavedScale = object->getScale();
			selectNode->saveTextureScaleRatios();
			return true;
		}
	} func(action_type, this);
	getSelection()->applyToNodes(&func);

	mSavedSelectionBBox = getBBoxOfSelection();
}

struct LLSelectMgrApplyFlags final : public LLSelectedObjectFunctor
{
	LLSelectMgrApplyFlags(U32 flags, bool state)
	:	mFlags(flags),
		mState(state)
	{}
	U32 mFlags;
	bool mState;
	bool apply(LLViewerObject* object) override
	{
		if (object->permModify() &&	// Preemptive permissions check
			object->isRoot())		// Do not send for child objects
		{
			object->setFlags(mFlags, mState);
		}
		return true;
	}
};

void LLSelectMgr::selectionUpdatePhysics(bool physics)
{
	LLSelectMgrApplyFlags func(FLAGS_USE_PHYSICS, physics);
	getSelection()->applyToObjects(&func);
}

void LLSelectMgr::selectionUpdateTemporary(bool is_temporary)
{
	LLSelectMgrApplyFlags func(FLAGS_TEMPORARY_ON_REZ, is_temporary);
	getSelection()->applyToObjects(&func);
}

void LLSelectMgr::selectionUpdatePhantom(bool is_phantom)
{
	LLSelectMgrApplyFlags func(FLAGS_PHANTOM, is_phantom);
	getSelection()->applyToObjects(&func);
}

//----------------------------------------------------------------------
// Helpful packing functions for sendObjectMessage()
//----------------------------------------------------------------------

//static
void LLSelectMgr::packAgentIDAndSessionAndAttachment(void* user_data)
{
	U8* attachment_point = (U8*)user_data;
	LLMessageSystem* msg = gMessageSystemp;
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->addU8Fast(_PREHASH_AttachmentPoint, *attachment_point);
}

//static
void LLSelectMgr::packAgentID(void* user_data)
{
	LLMessageSystem* msg = gMessageSystemp;
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
}

//static
void LLSelectMgr::packAgentAndSessionID(void* user_data)
{
	LLMessageSystem* msg = gMessageSystemp;
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
}

//static
void LLSelectMgr::packAgentAndGroupID(void* user_data)
{
	LLOwnerData *data = (LLOwnerData *)user_data;

	LLMessageSystem* msg = gMessageSystemp;
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, data->owner_id);
	msg->addUUIDFast(_PREHASH_GroupID, data->group_id);
}

//static
void LLSelectMgr::packAgentAndSessionAndGroupID(void* user_data)
{
	LLUUID* group_idp = (LLUUID*)user_data;

	LLMessageSystem* msg = gMessageSystemp;
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->addUUIDFast(_PREHASH_GroupID, *group_idp);
}

//static
void LLSelectMgr::packDuplicateHeader(void* data)
{
	LLUUID group_id(gAgent.getGroupID());
	if (gSavedSettings.getBool("RezWithLandGroup"))
	{
		LLParcel* parcel = gViewerParcelMgr.getAgentParcel();
		if (gAgent.isInGroup(parcel->getGroupID()))
		{
			group_id = parcel->getGroupID();
		}
		else if (gAgent.isInGroup(parcel->getOwnerID()))
		{
			group_id = parcel->getOwnerID();
		}
	}
	packAgentAndSessionAndGroupID(&group_id);

	LLDuplicateData* dup_data = (LLDuplicateData*)data;

	LLMessageSystem* msg = gMessageSystemp;
	msg->nextBlockFast(_PREHASH_SharedData);
	msg->addVector3Fast(_PREHASH_Offset, dup_data->offset);
	msg->addU32Fast(_PREHASH_DuplicateFlags, dup_data->flags);
}

//static
void LLSelectMgr::packDeleteHeader(void* userdata)
{
	bool force = (bool)(intptr_t)userdata;

	LLMessageSystem* msg = gMessageSystemp;
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->addBoolFast(_PREHASH_Force, force);
}

//static
void LLSelectMgr::packAgentGroupAndCatID(void* user_data)
{
	LLBuyData* buy = (LLBuyData*)user_data;

	LLMessageSystem* msg = gMessageSystemp;
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->addUUIDFast(_PREHASH_GroupID, gAgent.getGroupID());
	msg->addUUIDFast(_PREHASH_CategoryID, buy->mCategoryID);
}

//static
void LLSelectMgr::packDeRezHeader(void* user_data)
{
	LLDeRezInfo* info = (LLDeRezInfo*)user_data;

	LLMessageSystem* msg = gMessageSystemp;
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlockFast(_PREHASH_AgentBlock);
	msg->addUUIDFast(_PREHASH_GroupID, gAgent.getGroupID());
	msg->addU8Fast(_PREHASH_Destination, (U8)info->mDestination);
	msg->addUUIDFast(_PREHASH_DestinationID, info->mDestinationID);
	LLUUID tid;
	tid.generate();
	msg->addUUIDFast(_PREHASH_TransactionID, tid);
	constexpr U8 PACKET = 1;
	msg->addU8Fast(_PREHASH_PacketCount, PACKET);
	msg->addU8Fast(_PREHASH_PacketNumber, PACKET);
}

//static
void LLSelectMgr::packObjectID(LLSelectNode* node, void* user_data)
{
	LLMessageSystem* msg = gMessageSystemp;
	msg->nextBlockFast(_PREHASH_ObjectData);
	msg->addUUIDFast(_PREHASH_ObjectID, node->getObject()->mID);
}

void LLSelectMgr::packObjectIDAndRotation(LLSelectNode* node, void* user_data)
{
	LLMessageSystem* msg = gMessageSystemp;
	msg->nextBlockFast(_PREHASH_ObjectData);
	msg->addU32Fast(_PREHASH_ObjectLocalID, node->getObject()->getLocalID());
	msg->addQuatFast(_PREHASH_Rotation, node->getObject()->getRotation());
}

void LLSelectMgr::packObjectClickAction(LLSelectNode* node, void* user_data)
{
	LLMessageSystem* msg = gMessageSystemp;
	msg->nextBlockFast(_PREHASH_ObjectData);
	msg->addU32Fast(_PREHASH_ObjectLocalID, node->getObject()->getLocalID());
	msg->addU8("ClickAction", node->getObject()->getClickAction());
}

void LLSelectMgr::packObjectIncludeInSearch(LLSelectNode* node,
											void* user_data)
{
	LLMessageSystem* msg = gMessageSystemp;
	msg->nextBlockFast(_PREHASH_ObjectData);
	msg->addU32Fast(_PREHASH_ObjectLocalID, node->getObject()->getLocalID());
	msg->addBool("IncludeInSearch", node->getObject()->getIncludeInSearch());
}

//static
void LLSelectMgr::packObjectLocalID(LLSelectNode* node, void*)
{
	LLMessageSystem* msg = gMessageSystemp;
	msg->nextBlockFast(_PREHASH_ObjectData);
	msg->addU32Fast(_PREHASH_ObjectLocalID, node->getObject()->getLocalID());
}

//static
void LLSelectMgr::packObjectName(LLSelectNode* node, void* user_data)
{
	const std::string* name = (const std::string*)user_data;
	if (!name->empty())
	{
		LLMessageSystem* msg = gMessageSystemp;
		msg->nextBlockFast(_PREHASH_ObjectData);
		msg->addU32Fast(_PREHASH_LocalID, node->getObject()->getLocalID());
		msg->addStringFast(_PREHASH_Name, *name);
	}
	delete name;
}

//static
void LLSelectMgr::packObjectDescription(LLSelectNode* node, void* user_data)
{
	const std::string* desc = (const std::string*)user_data;
	if (desc && !desc->empty())
	{
		LLMessageSystem* msg = gMessageSystemp;
		msg->nextBlockFast(_PREHASH_ObjectData);
		msg->addU32Fast(_PREHASH_LocalID, node->getObject()->getLocalID());
		msg->addStringFast(_PREHASH_Description, *desc);
	}
}

//static
void LLSelectMgr::packObjectCategory(LLSelectNode* node, void* user_data)
{
	LLCategory* category = (LLCategory*)user_data;
	if (category)
	{
		LLMessageSystem* msg = gMessageSystemp;
		msg->nextBlockFast(_PREHASH_ObjectData);
		msg->addU32Fast(_PREHASH_LocalID, node->getObject()->getLocalID());
		category->packMessage(msg);
	}
}

//static
void LLSelectMgr::packObjectSaleInfo(LLSelectNode* node, void* user_data)
{
	LLSaleInfo* sale_info = (LLSaleInfo*)user_data;
	if (sale_info)
	{
		LLMessageSystem* msg = gMessageSystemp;
		msg->nextBlockFast(_PREHASH_ObjectData);
		msg->addU32Fast(_PREHASH_LocalID, node->getObject()->getLocalID());
		sale_info->packMessage(msg);
	}
}

//static
void LLSelectMgr::packPhysics(LLSelectNode* node, void* user_data)
{
}

//static
void LLSelectMgr::packShape(LLSelectNode* node, void* user_data)
{
}

// Because there is a bug in message_template.msg, which define the "Set" field
// in the ObjectData block of the ObjectPermissions message as an U8 instead of
// a bool (it does not change anything at the message level, but it causes the
// viewers to issue warnings when they set "Set" as a bool, which it should be.
#define MESSAGE_TEMPLATE_FIXED 0

//static
void LLSelectMgr::packPermissions(LLSelectNode* node, void* user_data)
{
	LLPermData* data = (LLPermData*)user_data;

	LLMessageSystem* msg = gMessageSystemp;
	msg->nextBlockFast(_PREHASH_ObjectData);
	msg->addU32Fast(_PREHASH_ObjectLocalID, node->getObject()->getLocalID());

	msg->addU8Fast(_PREHASH_Field, data->mField);
#if MESSAGE_TEMPLATE_FIXED
	msg->addBoolFast(_PREHASH_Set, data->mSet);
#else
	msg->addU8Fast(_PREHASH_Set, data->mSet);
#endif
	msg->addU32Fast(_PREHASH_Mask, data->mMask);
}

// Utility function to send some information to every region containing an
// object on the selection list. We want to do this to reduce the total number
// of packets sent by the viewer.
void LLSelectMgr::sendListToRegions(const std::string& message_name,
									void (*pack_header)(void* user_data),
									void (*pack_body)(LLSelectNode*, void*),
									void* user_data,
									ESendType send_type)
{
	// Clear update override data (allow next update through)
	struct f final : public LLSelectedNodeFunctor
	{
		bool apply(LLSelectNode* node) override
		{
			node->mLastPositionLocal.set(0, 0, 0);
			node->mLastRotation = LLQuaternion();
			node->mLastScale.set(0, 0, 0);
			return true;
		}
	} func;
	getSelection()->applyToNodes(&func);

	struct push_all final : public LLSelectedNodeFunctor
	{
		std::queue<LLSelectNode*>& nodes_to_send;

		push_all(std::queue<LLSelectNode*>& n)
		:	nodes_to_send(n)
		{
		}

		bool apply(LLSelectNode* node) override
		{
			if (node->getObject())
			{
				nodes_to_send.push(node);
			}
			return true;
		}
	};

	struct push_some final : public LLSelectedNodeFunctor
	{
		std::queue<LLSelectNode*>& nodes_to_send;
		bool mRoots;
		push_some(std::queue<LLSelectNode*>& n, bool roots)
		:	nodes_to_send(n),
			mRoots(roots)
		{
		}

		bool apply(LLSelectNode* node) override
		{
			LLViewerObject* object = node->getObject();
			if (object)
			{
				bool is_root = object->isRootEdit();
				if ((mRoots && is_root) || (!mRoots && !is_root))
				{
					nodes_to_send.push(node);
				}
			}
			return true;
		}
	};

	std::queue<LLSelectNode*> nodes_to_send;
	struct push_all pushall(nodes_to_send);
	struct push_some pushroots(nodes_to_send, true);
	struct push_some pushnonroots(nodes_to_send, false);

	switch (send_type)
	{
		case SEND_ONLY_ROOTS:
			if (message_name == "ObjectBuy")
			{
				getSelection()->applyToRootNodes(&pushroots);
			}
		  	else
			{
				getSelection()->applyToRootNodes(&pushall);
			}
			break;

		case SEND_INDIVIDUALS:
			getSelection()->applyToNodes(&pushall);
			break;

		case SEND_ROOTS_FIRST:
			// Roots first...
			getSelection()->applyToNodes(&pushroots);
			// ... then children.
			getSelection()->applyToNodes(&pushnonroots);
			break;

		case SEND_CHILDREN_FIRST:
			// Children first...
			getSelection()->applyToNodes(&pushnonroots);
			// ... then roots.
			getSelection()->applyToNodes(&pushroots);
			break;

		default:
			llwarns << "Bad send type " << send_type << llendl;
			llassert(false);
			return;
	}

	// Bail if nothing selected
	if (nodes_to_send.empty())
	{
		return;
	}

	LLSelectNode* node = nodes_to_send.front();
	nodes_to_send.pop();

	bool link_operation = message_name == "ObjectLink";
	LLSelectNode* linkset_root = NULL;
	S32 objects_in_this_packet = 0;
	// Cache last region information
	LLViewerRegion*	current_region = node->getObject()->getRegion();

	// Start duplicate message
	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessage(message_name.c_str());
	(*pack_header)(user_data);

	// For each object
	while (node)
	{
		// Remember the last region, look up the current one
		LLViewerRegion*	last_region = current_region;
		LLViewerObject* object = node->getObject();
		current_region = object->getRegion();
		// Ff to same simulator and message not too big
		if (current_region == last_region && !msg->isSendFull(NULL) &&
			objects_in_this_packet < MAX_OBJECTS_PER_PACKET)
		{
			if (link_operation && !linkset_root)
			{
				// Link sets over 254 will be split into multiple messages, but
				// we need to provide same root for all messages or we will get
				// separate linksets
				linkset_root = node;
			}

			// Add another instance of the body of the data
			(*pack_body)(node, user_data);
			++objects_in_this_packet;

			// and on to the next object
			if (nodes_to_send.empty())
			{
				node = NULL;
			}
			else
			{
				node = nodes_to_send.front();
				nodes_to_send.pop();
			}
		}
		else
		{
			// Otherwise send current message and start new one
			msg->sendReliable(last_region->getHost());
			objects_in_this_packet = 0;

			msg->newMessage(message_name.c_str());
			(*pack_header)(user_data);

			if (linkset_root)
			{
				if (current_region == last_region)
				{
					// add root instance into new message
					(*pack_body)(linkset_root, user_data);
					++objects_in_this_packet;
				}
				else
				{
					// Root should be in the same region as the children; reset
					// it.
					linkset_root = NULL;
				}
			}
			// Do not move to the next object, we still need to add the body
			// data.
		}
	}

	// Flush messages
	if (msg->getCurrentSendTotal() > 0)
	{
		msg->sendReliable(current_region->getHost());
	}
	else
	{
		msg->clearMessage();
	}
}

//
// Network communications
//

void LLSelectMgr::registerObjectPropertiesFamilyRequest(const LLUUID& object_id)
{
	sObjectPropertiesFamilyRequests.emplace(object_id);
}

void LLSelectMgr::requestObjectPropertiesFamily(LLViewerObject* object)
{
	// Remember that we asked the properties of this object.
	registerObjectPropertiesFamilyRequest(object->mID);

	if (mDebugSelectMgr)
	{
		llinfos << "Registered a request for object: " << object->mID
				<< llendl;
	}

	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessageFast(_PREHASH_RequestObjectPropertiesFamily);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlockFast(_PREHASH_ObjectData);
	msg->addU32Fast(_PREHASH_RequestFlags, 0x0);
	msg->addUUIDFast(_PREHASH_ObjectID, object->mID);

	LLViewerRegion* regionp = object->getRegion();
	msg->sendReliable(regionp->getHost());
}

//static
void LLSelectMgr::processObjectProperties(LLMessageSystem* msg, void**)
{
	S32 count = msg->getNumberOfBlocksFast(_PREHASH_ObjectData);
	for (S32 i = 0; i < count; ++i)
	{
		LLUUID id;
		msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_ObjectID, id, i);

		LLUUID creator_id, owner_id, group_id, extra_id;
		msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_CreatorID, creator_id,
						 i);
		msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_OwnerID, owner_id, i);
		msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_GroupID, group_id, i);

		U64 creation_date;
		msg->getU64Fast(_PREHASH_ObjectData, _PREHASH_CreationDate,
						creation_date, i);

		U32 base_mask, owner_mask, group_mask, everyone_mask, next_owner_mask;
		msg->getU32Fast(_PREHASH_ObjectData, _PREHASH_BaseMask, base_mask, i);
		msg->getU32Fast(_PREHASH_ObjectData, _PREHASH_OwnerMask, owner_mask,
						i);
		msg->getU32Fast(_PREHASH_ObjectData, _PREHASH_GroupMask, group_mask,
						i);
		msg->getU32Fast(_PREHASH_ObjectData, _PREHASH_EveryoneMask,
						everyone_mask, i);
		msg->getU32Fast(_PREHASH_ObjectData, _PREHASH_NextOwnerMask,
						next_owner_mask, i);

		LLSaleInfo sale_info;
		sale_info.unpackMultiMessage(msg, _PREHASH_ObjectData, i);

		LLAggregatePermissions ag_perms, ag_tex_perms, ag_tex_perms_owner;
		ag_perms.unpackMessage(msg, _PREHASH_ObjectData,
							   _PREHASH_AggregatePerms, i);
		ag_tex_perms.unpackMessage(msg, _PREHASH_ObjectData,
								   _PREHASH_AggregatePermTextures, i);
		ag_tex_perms_owner.unpackMessage(msg, _PREHASH_ObjectData,
										 _PREHASH_AggregatePermTexturesOwner,
										 i);

		LLCategory category;
		category.unpackMultiMessage(msg, _PREHASH_ObjectData, i);

		S16 inv_serial = 0;
		msg->getS16Fast(_PREHASH_ObjectData, _PREHASH_InventorySerial,
						inv_serial, i);

		LLUUID item_id, folder_id, from_task_id;
		msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_ItemID, item_id, i);
		msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_FolderID, folder_id, i);
		msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_FromTaskID,
						 from_task_id, i);

		LLUUID last_owner_id;
		msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_LastOwnerID,
						 last_owner_id, i);

		std::string name, desc, touch_name, sit_name;
		msg->getStringFast(_PREHASH_ObjectData, _PREHASH_Name, name, i);
		msg->getStringFast(_PREHASH_ObjectData, _PREHASH_Description, desc, i);
		msg->getStringFast(_PREHASH_ObjectData, _PREHASH_TouchName, touch_name,
						   i);
		msg->getStringFast(_PREHASH_ObjectData, _PREHASH_SitName, sit_name, i);

		// Unpack TE IDs
		uuid_vec_t texture_ids;
		S32 size = msg->getSizeFast(_PREHASH_ObjectData, i,
									_PREHASH_TextureID);
		if (size > 0)
		{
			S8 packed_buffer[SELECT_MAX_TES * UUID_BYTES];
			msg->getBinaryDataFast(_PREHASH_ObjectData, _PREHASH_TextureID,
								   packed_buffer, 0, i,
								   SELECT_MAX_TES * UUID_BYTES);

			for (S32 buf_offset = 0; buf_offset < size;
				 buf_offset += UUID_BYTES)
			{
				LLUUID tid;
				memcpy(tid.mData, packed_buffer + buf_offset, UUID_BYTES);
				texture_ids.emplace_back(tid);
			}
		}

		// Iterate through nodes at end, since it can be on both the regular
		// AND hover list
		struct f final : public LLSelectedNodeFunctor
		{
			LLUUID mID;

			f(const LLUUID& id)
			:	mID(id)
			{
			}

			bool apply(LLSelectNode* nodep) override
			{
				LLViewerObject* objectp = nodep->getObject();
				return objectp && objectp->mID == mID;
			}
		} func(id);

		LLSelectNode* nodep = gSelectMgr.getSelection()->getFirstNode(&func);
		if (!nodep) continue;

		// Save texture data as soon as we get texture perms first time
		bool save_textures = !nodep->mValid;
		LLViewerObject* objectp = nodep->getObject();
		if (objectp && objectp->getInventorySerial() != inv_serial)
		{
			objectp->dirtyInventory();
			// Even if this is not object's first udpate, inventory changed and
			// some of the applied textures might have been in inventory, so
			// update texture list.
			save_textures = true;
		}
		if (save_textures)
		{
			bool can_copy = false;
			bool can_transfer = false;

			LLAggregatePermissions::EValue value =
				LLAggregatePermissions::AP_NONE;
			if (objectp && objectp->permYouOwner())
			{
				value = ag_tex_perms_owner.getValue(PERM_COPY);
				if (value == LLAggregatePermissions::AP_EMPTY ||
					value == LLAggregatePermissions::AP_ALL)
				{
					can_copy = true;
				}
				value = ag_tex_perms_owner.getValue(PERM_TRANSFER);
				if (value == LLAggregatePermissions::AP_EMPTY ||
					value == LLAggregatePermissions::AP_ALL)
				{
					can_transfer = true;
				}
			}
			else
			{
				value = ag_tex_perms.getValue(PERM_COPY);
				if (value == LLAggregatePermissions::AP_EMPTY ||
					value == LLAggregatePermissions::AP_ALL)
				{
					can_copy = true;
				}
				value = ag_tex_perms.getValue(PERM_TRANSFER);
				if (value == LLAggregatePermissions::AP_EMPTY ||
					value == LLAggregatePermissions::AP_ALL)
				{
					can_transfer = true;
				}
			}

			if (can_copy && can_transfer)
			{
				// This should be the only place that saved textures is called
				nodep->saveTextures(texture_ids);
			}

			if (can_copy && can_transfer && objectp->getVolume())
			{
				uuid_vec_t mat_ids;
				gltf_mat_vec_t mats;
				LLVOVolume* vobjp = (LLVOVolume*)objectp;
				for (U32 te = 0, tes = vobjp->getNumTEs(); te < tes; ++te)
				{
					mat_ids.push_back(vobjp->getRenderMaterialID(te));
					// Make a copy to ensure we would not affect live material
					// with any potential changes nor live changes will be
					// reflected in a saved copy, like changes from local
					// material (reuses pointer) or from live editor (revert
					// mechanics might modify this).
					LLTextureEntry* tep = objectp->getTE(te);
					LLGLTFMaterial* matp = NULL;
					if (tep)
					{
						matp = tep->getGLTFMaterialOverride();
					}
					if (matp)
					{
						mats.emplace_back(new LLGLTFMaterial(*matp));
					}
					else
					{
						mats.emplace_back(nullptr);
					}
				}
				// processObjectProperties() does not include overrides so this
				// might need to be moved to
				// LLGLTFMaterialOverrideDispatchHandler
				nodep->saveGLTFMaterials(mat_ids, mats);
			}
		}

		nodep->mValid = true;
		nodep->mPermissions->init(creator_id, owner_id, last_owner_id,
								  group_id);
		nodep->mPermissions->initMasks(base_mask, owner_mask, everyone_mask,
									   group_mask, next_owner_mask);
		nodep->mCreationDate = creation_date;
		nodep->mItemID = item_id;
		nodep->mFolderID = folder_id;
		nodep->mFromTaskID = from_task_id;
		nodep->mName.assign(name);
		nodep->mDescription.assign(desc);
		nodep->mSaleInfo = sale_info;
		nodep->mAggregatePerm = ag_perms;
		nodep->mAggregateTexturePerm = ag_tex_perms;
		nodep->mAggregateTexturePermOwner = ag_tex_perms_owner;
		nodep->mCategory = category;
		nodep->mInventorySerial = inv_serial;
		nodep->mSitName.assign(sit_name);
		nodep->mTouchName.assign(touch_name);
	}

	dialog_refresh_all();

	// *HACK: for left-click buy object
	LLToolPie::selectionPropertiesReceived();
}

//static
void LLSelectMgr::processObjectPropertiesFamily(LLMessageSystem* msg, void**)
{
	LLUUID id;
	msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_ObjectID, id);

	LLUUID owner_id;
	msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_OwnerID, owner_id);

	// Keep track of the owner's Id for that object.
	LLViewerObject* objectp = gObjectList.findObject(id);
	if (objectp && objectp->mOwnerID.isNull())
	{
		objectp->mOwnerID = owner_id;
	}

	uuid_list_t::iterator it = sObjectPropertiesFamilyRequests.find(id);
	if (it == sObjectPropertiesFamilyRequests.end())
	{
		// This reply is not for us.
		return;
	}
	// We got the reply, so remove the object from the list of pending requests
	sObjectPropertiesFamilyRequests.erase(it);
	if (gSelectMgr.mDebugSelectMgr)
	{
		llinfos << "Got ObjectPropertiesFamily reply for object: " << id
				<< llendl;
	}

	U32 request_flags;
	msg->getU32Fast(_PREHASH_ObjectData, _PREHASH_RequestFlags, request_flags);

	LLUUID group_id;
	msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_GroupID, group_id);

	U32 base_mask, owner_mask, group_mask, everyone_mask, next_owner_mask;
	msg->getU32Fast(_PREHASH_ObjectData, _PREHASH_BaseMask, base_mask);
	msg->getU32Fast(_PREHASH_ObjectData, _PREHASH_OwnerMask, owner_mask);
	msg->getU32Fast(_PREHASH_ObjectData,_PREHASH_GroupMask, group_mask);
	msg->getU32Fast(_PREHASH_ObjectData, _PREHASH_EveryoneMask, everyone_mask);
	msg->getU32Fast(_PREHASH_ObjectData, _PREHASH_NextOwnerMask,
					next_owner_mask);

	LLSaleInfo sale_info;
	sale_info.unpackMessage(msg, _PREHASH_ObjectData);

	LLCategory category;
	category.unpackMessage(msg, _PREHASH_ObjectData);

	LLUUID last_owner_id;
	msg->getUUIDFast(_PREHASH_ObjectData, _PREHASH_LastOwnerID, last_owner_id);

	std::string name, desc;
	msg->getStringFast(_PREHASH_ObjectData, _PREHASH_Name, name);
	msg->getStringFast(_PREHASH_ObjectData, _PREHASH_Description, desc);

	// The reporter widget asks the server for info about picked objects
	if (request_flags & COMPLAINT_REPORT_REQUEST)
	{
		LLFloaterReporter* reporterp = LLFloaterReporter::findInstance();
		if (reporterp)
		{
			std::string fullname;
			if (gCacheNamep)
			{
				gCacheNamep->getFullName(owner_id, fullname);
			}
			reporterp->setPickedObjectProperties(name, fullname, owner_id);
		}
	}
	else if (request_flags & OBJECT_PAY_REQUEST)
	{
		// Check if the owner of the paid object is muted
		LLMuteList::autoRemove(owner_id, LLMuteList::AR_MONEY);
	}

	// Now look through all of the hovered nodes
	struct f final : public LLSelectedNodeFunctor
	{
		f(const LLUUID& id)
		:	mID(id)
		{
		}

		bool apply(LLSelectNode* node) override
		{
			return node->getObject() && node->getObject()->mID == mID;
		}

		LLUUID mID;
	} func(id);

	LLSelectNode* node = gSelectMgr.mHoverObjects->getFirstNode(&func);
	if (node)
	{
		node->mValid = true;
		node->mPermissions->init(LLUUID::null, owner_id,
								 last_owner_id, group_id);
		node->mPermissions->initMasks(base_mask, owner_mask, everyone_mask,
									  group_mask, next_owner_mask);
		node->mSaleInfo = sale_info;
		node->mCategory = category;
		node->mName.assign(name);
		node->mDescription.assign(desc);
	}

	dialog_refresh_all();
}

//static
void LLSelectMgr::processForceObjectSelect(LLMessageSystem* msg, void**)
{
	bool reset_list;
	msg->getBool("Header", "ResetList", reset_list);

	if (reset_list)
	{
		gSelectMgr.deselectAll();
	}

	std::vector<LLViewerObject*> objects;

	S32 local_id;
	LLUUID full_id;
	U32 ip = msg->getSenderIP();
	U32 port = msg->getSenderPort();
	for (S32 i = 0, count = msg->getNumberOfBlocks("Data"); i < count; ++i)
	{
		msg->getS32("Data", "LocalID", local_id, i);
		LLViewerObjectList::getUUIDFromLocal(full_id, local_id, ip, port);
		LLViewerObject* object = gObjectList.findObject(full_id);
		if (object)
		{
			objects.push_back(object);
		}
	}

	// Do not select, just highlight
	gSelectMgr.highlightObjectAndFamily(objects);
}

void LLSelectMgr::updateSilhouettes()
{
	if (!mSilhouetteImagep)
	{
		mSilhouetteImagep =
			LLViewerTextureManager::getFetchedTextureFromFile("silhouette.j2c");
	}

	mHighlightedObjects->cleanupNodes();

	LLVector3d camera_pos = gAgent.getCameraPositionGlobal();
	F32 camera_zoom = gAgent.getCurrentCameraBuildOffset();
	if ((camera_pos - mLastCameraPos).lengthSquared() >
			SILHOUETTE_UPDATE_THRESHOLD_SQUARED * camera_zoom * camera_zoom)
	{
		struct f final : public LLSelectedObjectFunctor
		{
			bool apply(LLViewerObject* object) override
			{
				object->setChanged(LLXform::SILHOUETTE);
				return true;
			}
		} func;
		getSelection()->applyToObjects(&func);

		mLastCameraPos = gAgent.getCameraPositionGlobal();
	}

	std::vector<LLViewerObject*> changed_objects;

	S32 num_sils_genned = 0;
	updateSelectionSilhouette(mSelectedObjects, num_sils_genned,
							  changed_objects);
	if (mRectSelectedObjects.size() > 0)
	{
		fast_hset<LLViewerObject*> roots;

		// Sync mHighlightedObjects with mRectSelectedObjects since the latter
		// is rebuilt every frame and former persists from frame to frame to
		// avoid regenerating object silhouettes mHighlightedObjects includes
		// all siblings of rect selected objects

		bool select_linked_set = !mEditLinkedParts;

		// generate list of roots from current object selection
		for (std::set<LLPointer<LLViewerObject> >::iterator
				iter = mRectSelectedObjects.begin(),
				end = mRectSelectedObjects.end();
			 iter != end; ++iter)
		{
			LLViewerObject* objectp = *iter;
			if (select_linked_set)
			{
				roots.insert((LLViewerObject*)objectp->getRoot());
			}
			else
			{
				roots.insert(objectp);
			}
		}

		// Remove highlight nodes not in roots list
		std::vector<LLSelectNode*> remove_these_nodes;
		std::vector<LLViewerObject*> remove_these_roots;

		for (LLObjectSelection::iterator iter = mHighlightedObjects->begin(),
										 end = mHighlightedObjects->end();
			 iter != end; ++iter)
		{
			LLSelectNode* node = *iter;
			LLViewerObject* objectp = node->getObject();
			if (!objectp) continue;
			if (objectp->isRoot() || !select_linked_set)
			{
				if (roots.count(objectp) == 0)
				{
					remove_these_nodes.push_back(node);
				}
				else
				{
					remove_these_roots.push_back(objectp);
				}
			}
			else if (!roots.count((LLViewerObject*)objectp->getRoot()))
			{
				remove_these_nodes.push_back(node);
			}
		}

		// Remove all highlight nodes no longer in rectangle selection
		for (std::vector<LLSelectNode*>::iterator
				iter = remove_these_nodes.begin(),
				end = remove_these_nodes.end();
			 iter != end; ++iter)
		{
			LLSelectNode* nodep = *iter;
			mHighlightedObjects->removeNode(nodep);
		}

		// Remove all root objects already being highlighted
		for (std::vector<LLViewerObject*>::iterator
				iter = remove_these_roots.begin(),
				end = remove_these_roots.end();
			 iter != end; ++iter)
		{
			LLViewerObject* objectp = *iter;
			roots.erase(objectp);
		}

		// Add all new objects in rectangle selection
		for (fast_hset<LLViewerObject*>::iterator iter = roots.begin(),
												  end = roots.end();
			 iter != end; ++iter)
		{
			LLViewerObject* objectp = *iter;
			if (!canSelectObject(objectp))
			{
				continue;
			}

			LLSelectNode* rect_select_root_node = new LLSelectNode(objectp,
																   true);
			rect_select_root_node->selectAllTEs(true);

			if (!select_linked_set)
			{
				rect_select_root_node->mIndividualSelection = true;
			}
			else
			{
				LLViewerObject::const_child_list_t& child_list =
					objectp->getChildren();
				for (LLViewerObject::child_list_t::const_iterator
						iter2 = child_list.begin(),
						end2 = child_list.end();
					 iter2 != end2; ++iter2)
				{
					LLViewerObject* child_objectp = *iter2;

					if (!canSelectObject(child_objectp))
					{
						continue;
					}

					LLSelectNode* rect_select_node =
						new LLSelectNode(child_objectp, true);
					rect_select_node->selectAllTEs(true);
					mHighlightedObjects->addNodeAtEnd(rect_select_node);
				}
			}

			// Add the root last, to preserve order for link operations.
			mHighlightedObjects->addNodeAtEnd(rect_select_root_node);
		}

		num_sils_genned	= 0;

		const LLVector3& camera_origin = gViewerCamera.getOrigin();
		// Render silhouettes for highlighted objects
		for (S32 pass = 0; pass < 2; ++pass)
		{
			for (LLObjectSelection::iterator iter = mHighlightedObjects->begin();
				 iter != mHighlightedObjects->end(); ++iter)
			{
				LLSelectNode* nodep = *iter;
				if (!nodep) continue;		// Paranoia

				LLViewerObject* objectp = nodep->getObject();
				if (!objectp || objectp->isDead())
				{
					continue;
				}

				// Do roots first, then children so that root flags are cleared
				// ASAP
				bool roots_only = pass == 0;
				bool is_root = objectp->isRootEdit();
				if (roots_only != is_root)
				{
					continue;
				}

				if (nodep->mSilhouetteGenerated &&
					!objectp->isChanged(LLXform::SILHOUETTE) &&
					(is_root ||
					 !objectp->getParent()->isChanged(LLXform::SILHOUETTE)))
				{
					continue;
				}

				if (num_sils_genned++ < MAX_SILS_PER_FRAME)
				{
					generateSilhouette(nodep, camera_origin);
					changed_objects.push_back(objectp);
					continue;
				}

				if (!objectp->isAttachment())
				{
					continue;
				}

				LLDrawable* drawablep = objectp->getRootEdit()->mDrawable;
				if (!drawablep)
				{
					continue;
				}

				// RN: hack for orthogonal projection of HUD attachments
				LLViewerJointAttachment* attachp =
					(LLViewerJointAttachment*)drawablep->getParent();
				if (attachp && attachp->getIsHUDAttachment())
				{
					generateSilhouette(nodep, LLVector3(-10000.f, 0.f, 0.f));
				}
			}
		}
	}
	else
	{
		mHighlightedObjects->deleteAllNodes();
	}

	for (std::vector<LLViewerObject*>::iterator iter = changed_objects.begin(),
												end = changed_objects.end();
		 iter != end; ++iter)
	{
		// Clear flags after traversing node list (as child objects need to
		// refer to parent flags, etc)
		LLViewerObject* objectp = *iter;
		objectp->clearChanged(LLXform::MOVED | LLXform::SILHOUETTE);
	}
}

void LLSelectMgr::updateSelectionSilhouette(LLObjectSelectionHandle object_handle,
											S32& num_sils_genned,
											std::vector<LLViewerObject*>& changed_objects)
{
	if (!object_handle->getNumNodes())
	{
		return;
	}

	const LLVector3& camera_origin = gViewerCamera.getOrigin();
	for (S32 pass = 0; pass < 2; ++pass)
	{
		for (LLObjectSelection::iterator iter = object_handle->begin(),
										 end = object_handle->end();
			iter != end; ++iter)
		{
			LLSelectNode* nodep = *iter;
			if (!nodep) continue;		// Paranoia

			LLViewerObject* objectp = nodep->getObject();
			if (!objectp || objectp->isDead()) continue;

			// Do roots first, then children so that root flags are cleared
			// ASAP
			bool roots_only = pass == 0;
			bool is_root = objectp->isRootEdit();
			if (roots_only != is_root || objectp->mDrawable.isNull())
			{
				continue;
			}

			if (!nodep->mSilhouetteGenerated ||
				objectp->isChanged(LLXform::SILHOUETTE) ||
				(!is_root &&
				 objectp->getParent()->isChanged(LLXform::SILHOUETTE)))
			{
				if (num_sils_genned++ < MAX_SILS_PER_FRAME)
				{
					generateSilhouette(nodep, camera_origin);
					changed_objects.push_back(objectp);
				}
				else if (objectp->isAttachment())
				{
					// RN: hack for orthogonal projection of HUD attachments
					LLViewerJointAttachment* attachment_pt =
						(LLViewerJointAttachment*)objectp->getRootEdit()->mDrawable->getParent();
					if (attachment_pt && attachment_pt->getIsHUDAttachment())
					{
						generateSilhouette(nodep,
										   LLVector3(-10000.f, 0.f, 0.f));
					}
				}
			}
		}
	}
}

// Helper function for LLSelectMgr::renderMeshSelection().
static void render_face(LLDrawable* drawablep, LLFace* facep)
{
	if (!drawablep || drawablep->isDead()) return;

	LLVOVolume* vovolp = drawablep->getVOVolume();
	if (!vovolp || !facep) return;

	LLVolume* volp;
	if (drawablep->isState(LLDrawable::RIGGED))
	{
		volp = vovolp->getRiggedVolume();
	}
	else
	{
		volp = vovolp->getVolume();
	}
	// While rezzing a mesh object, it may happen that the face TE offset is
	// larger than the current object number of faces, because the mesh is not
	// yet fully loaded. Check for this case and skip, to avoid a crash
	// whenever this object is also selected (e.g. when rezzing while in Build
	// mode). HB
	if (volp && facep->getTEOffset() < (S32)volp->getVolumeFaces().size())
	{
		const LLVolumeFace& vf = volp->getVolumeFace(facep->getTEOffset());
		LLVertexBuffer::drawElements(vf.mNumVertices, vf.mPositions, NULL,
									 vf.mNumIndices, vf.mIndices);
	}
}

// Only used for the PBR renderer (the EE renderer still uses the v1 legacy
// selection rendering routines). HB
void LLSelectMgr::renderMeshSelection(LLSelectNode* nodep,
									  LLViewerObject* objectp,
									  LLDrawable* drawablep,
									  LLVOVolume* volp,
									  const LLColor4& color, bool no_hidden)
{
	bool wireframe = renderHiddenSelection() && !no_hidden;

	LLGLSLShader* shaderp = LLGLSLShader::sCurBoundShaderPtr;

	gGL.flush();

	if (shaderp)
	{
		gDebugProgram.bind();
	}

	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.pushMatrix();

	bool is_hud_object = objectp->isHUDAttachment();
	if (!is_hud_object)
	{
		gGL.loadIdentity();
		gGL.multMatrix(gGLModelView);
	}
	if (drawablep->isActive())
	{
		gGL.loadMatrix(gGLModelView);
		gGL.multMatrix(objectp->getRenderMatrix().getF32ptr());
	}
	else if (!is_hud_object)
	{
		LLVector3 trans = objectp->getRegion()->getOriginAgent();
		gGL.translatef(trans.mV[0], trans.mV[1], trans.mV[2]);
	}

	LLVertexBuffer::unbind();
	gGL.pushMatrix();
	gGL.multMatrix(volp->getRelativeXform().getF32ptr());
	if (drawablep->isState(LLDrawable::RIGGED))
	{
		volp->updateRiggedVolume(true);
	}

	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	for (U32 te = 0,
			 count = llmin(objectp->getNumTEs(), objectp->getNumFaces());
		 te < count; ++te)
	{
		if (nodep->isTESelected(te))
		{
			LLFace* facep = objectp->mDrawable->getFace(te);
			if (!facep) continue;	// Paranoia.

			// Question: would niot it be also drawablep ?... HB
			LLDrawable* drawp = facep->getDrawable().get();
			if (!drawp) continue;	// Paranoia.

			// The following code is the simplified form (shader always
			// exists, no stencil since reserved for PBR rendering) of LL's
			// LLFace::renderOneWireframe(), that we do not have. HB

			if (wireframe)
			{
				gGL.blendFunc(LLRender::BF_SOURCE_COLOR, LLRender::BF_ONE);
				LLGLDepthTest gls_depth(GL_TRUE, GL_FALSE, GL_GEQUAL);
				gGL.diffuseColor4f(color.mV[VRED], color.mV[VGREEN],
								   color.mV[VBLUE], 0.4f);
				render_face(drawp, facep);
			}

			gGL.flush();
			gGL.setSceneBlendType(LLRender::BT_ALPHA);
			gGL.diffuseColor4f(color.mV[VRED] * 2.f, color.mV[VGREEN] * 2.f,
							   color.mV[VBLUE] * 2.f, color.mV[VALPHA]);
			{
				LLGLDisable depth(wireframe ? 0 : GL_BLEND);
				LLGLEnable offset(GL_POLYGON_OFFSET_LINE);
				glPolygonOffset(3.f, 3.f);
				gGL.lineWidth(5.f);
				glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
				render_face(drawp, facep);
			}
		}
	}

	gGL.popMatrix();
	gGL.popMatrix();

	gGL.lineWidth(1.f);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

	if (shaderp)
	{
		shaderp->bind();
	}
}

void LLSelectMgr::renderSilhouettes(bool for_hud)
{
	if (!mRenderSilhouettes || !mRenderSelectionsPolicy)
	{
		return;
	}

	LLTexUnit* unit0 = gGL.getTexUnit(0);
	unit0->bind(mSilhouetteImagep);

	LLGLSPipelineSelection gls_select;
	LLGLEnable blend(GL_BLEND);
	LLGLDepthTest gls_depth(GL_TRUE, GL_FALSE);

	if (for_hud && isAgentAvatarValid())
	{
		LLBBox hud_bbox = gAgentAvatarp->getHUDBBox();

		F32 cur_zoom = gAgent.mHUDCurZoom;

		// Set-up transform to encompass bounding box of HUD
		gGL.matrixMode(LLRender::MM_PROJECTION);
		gGL.pushMatrix();
		gGL.loadIdentity();
		F32 depth = llmax(1.f, hud_bbox.getExtentLocal().mV[VX] * 1.1f);
		gGL.ortho(-0.5f * gViewerCamera.getAspect(),
				  0.5f * gViewerCamera.getAspect(),
				  -0.5f, 0.5f, 0.f, depth);

		gGL.matrixMode(LLRender::MM_MODELVIEW);
		gGL.pushMatrix();
		gGL.pushUIMatrix();
		gGL.loadUIIdentity();
		gGL.loadIdentity();
		// Load Cory's favorite reference frame
		gGL.loadMatrix(OGL_TO_CFR_ROT4A);
		gGL.translatef(-hud_bbox.getCenterLocal().mV[VX] + depth * 0.5f,
					   0.f, 0.f);
		gGL.scalef(cur_zoom, cur_zoom, cur_zoom);
	}
	if (mSelectedObjects->getNumNodes())
	{
		LLColor4 color;
		LLUUID inspect_item_id = LLFloaterInspect::getSelectedUUID();
		LLUUID focus_item_id = LLViewerMediaFocus::getInstance()->getFocusedObjectID();
		for (LLObjectSelection::iterator iter = mSelectedObjects->begin(),
										 end = mSelectedObjects->end();
			 iter != end; ++iter)
		{
			LLSelectNode* nodep = *iter;
			if (!nodep) continue;	// Paranoia ?

			LLViewerObject* objectp = nodep->getObject();
			if (!objectp || objectp->isDead() ||
				objectp->isHUDAttachment() != for_hud)
			{
				continue;
			}
			LLDrawable* drawablep = objectp->mDrawable;
			if (!drawablep)
			{
				continue;
			}

			bool no_hidden = false;
			if (objectp->getID() == focus_item_id)
			{
				color = gFocusMgr.getFocusColor();
			}
			else if (objectp->getID() == inspect_item_id)
			{
				color = sHighlightInspectColor;
			}
			else if (nodep->isTransient())
			{
				color = sContextSilhouetteColor;
				no_hidden = true;
			}
			else if (objectp->isRootEdit())
			{
				color = sSilhouetteParentColor;
			}
			else
			{
				color = sSilhouetteChildColor;
			}

			LLVOVolume* volp = drawablep->getVOVolume();
			if (gUsePBRShaders && volp && volp->isMesh())
			{
				renderMeshSelection(nodep, objectp, drawablep, volp, color,
									no_hidden);
			}
			else
			{
				nodep->renderOneSilhouette(color, no_hidden);
			}
		}
	}

	if (mHighlightedObjects->getNumNodes())
	{
		// Render silhouettes for highlighted objects
		bool subtract_from_sel = gKeyboardp &&
								 gKeyboardp->currentMask(true) == MASK_CONTROL;
		LLColor4 color = LLColor4::red;	// Color for subtract_from_sel
		for (LLObjectSelection::iterator iter = mHighlightedObjects->begin(),
										 end = mHighlightedObjects->end();
			 iter != end; ++iter)
		{
			LLSelectNode* nodep = *iter;
			if (!nodep) continue;	// Paranoia ?

			LLViewerObject* objectp = nodep->getObject();
			if (!objectp || objectp->isDead() ||
				objectp->isHUDAttachment() != for_hud)
			{
				continue;
			}
			LLDrawable* drawablep = objectp->mDrawable;
			if (!drawablep)
			{
				continue;
			}

			if (!subtract_from_sel)
			{
				if (objectp->isSelected())
				{
					continue;
				}
				if (objectp->isRoot())
				{
					color = sHighlightParentColor;
				}
				else
				{
					color = sHighlightChildColor;
				}
			}

			LLVOVolume* volp = drawablep->getVOVolume();
			if (gUsePBRShaders && volp && volp->isMesh())
			{
				renderMeshSelection(nodep, objectp, drawablep, volp, color);
			}
			else
			{
				nodep->renderOneSilhouette(color);
			}
		}
	}

	if (for_hud && isAgentAvatarValid())
	{
		gGL.matrixMode(LLRender::MM_PROJECTION);
		gGL.popMatrix();

		gGL.matrixMode(LLRender::MM_MODELVIEW);
		gGL.popMatrix();
		gGL.popUIMatrix();
	}

	unit0->unbind(LLTexUnit::TT_TEXTURE);

	stop_glerror();
}

void LLSelectMgr::generateSilhouette(LLSelectNode* nodep,
									 const LLVector3& view_point)
{
	LLViewerObject* objectp = nodep->getObject();
	if (!objectp) return;	// Paranoia

	// Faster than a virtual generateSilhouette() method, beside view_point is
	// not needed for trees and grass.
	LLPCode pcode = objectp->getPCode();
	if (pcode == LL_PCODE_VOLUME)
	{
		((LLVOVolume*)objectp)->generateSilhouette(nodep, view_point);
	}
	else if (pcode == LL_PCODE_LEGACY_GRASS)
	{
		((LLVOGrass*)objectp)->generateSilhouette(nodep);
	}
	else if (pcode == LL_PCODE_LEGACY_TREE)
	{
		((LLVOTree*)objectp)->generateSilhouette(nodep);
	}
}

//
// Utility classes
//
LLSelectNode::LLSelectNode(LLViewerObject* object, bool glow)
:	mObject(object),
	mIndividualSelection(false),
	mTransient(false),
	mValid(false),
	mPermissions(new LLPermissions()),
	mInventorySerial(0),
	mSilhouetteGenerated(false),
	mDuplicated(false),
	mTESelectMask(0),
	mLastTESelected(0),
	mName(LLStringUtil::null),
	mDescription(LLStringUtil::null),
	mTouchName(LLStringUtil::null),
	mSitName(LLStringUtil::null),
	mCreationDate(0)
{
	selectAllTEs(false);
	saveColors();
}

LLSelectNode::LLSelectNode(const LLSelectNode& nodep)
{
	mTESelectMask = nodep.mTESelectMask;
	mLastTESelected = nodep.mLastTESelected;

	mIndividualSelection = nodep.mIndividualSelection;

	mValid = nodep.mValid;
	mTransient = nodep.mTransient;
	mPermissions = new LLPermissions(*nodep.mPermissions);
	mSaleInfo = nodep.mSaleInfo;;
	mAggregatePerm = nodep.mAggregatePerm;
	mAggregateTexturePerm = nodep.mAggregateTexturePerm;
	mAggregateTexturePermOwner = nodep.mAggregateTexturePermOwner;
	mName = nodep.mName;
	mDescription = nodep.mDescription;
	mCategory = nodep.mCategory;
	mInventorySerial = 0;
	mSavedPositionLocal = nodep.mSavedPositionLocal;
	mSavedPositionGlobal = nodep.mSavedPositionGlobal;
	mSavedScale = nodep.mSavedScale;
	mSavedRotation = nodep.mSavedRotation;
	mDuplicated = nodep.mDuplicated;
	mDuplicatePos = nodep.mDuplicatePos;
	mDuplicateRot = nodep.mDuplicateRot;
	mItemID = nodep.mItemID;
	mFolderID = nodep.mFolderID;
	mFromTaskID = nodep.mFromTaskID;
	mTouchName = nodep.mTouchName;
	mSitName = nodep.mSitName;
	mCreationDate = nodep.mCreationDate;

	mSilhouetteVertices = nodep.mSilhouetteVertices;
	mSilhouetteNormals = nodep.mSilhouetteNormals;
	mSilhouetteGenerated = nodep.mSilhouetteGenerated;
	mObject = nodep.mObject;

	mSavedColors.clear();
	S32 count = nodep.mSavedColors.size();
	mSavedColors.reserve(count);
	for (S32 i = 0; i < count; ++i)
	{
		mSavedColors.emplace_back(nodep.mSavedColors[i]);
	}

	saveTextures(nodep.mSavedTextures);
	saveGLTFMaterials(nodep.mSavedGLTFMaterialIds,
					  nodep.mSavedGLTFOverrideMaterials);
}

LLSelectNode::~LLSelectNode()
{
	delete mPermissions;
	mPermissions = NULL;
}

void LLSelectNode::selectAllTEs(bool b)
{
	mTESelectMask = b ? TE_SELECT_MASK_ALL : 0x0;
	mLastTESelected = 0;
}

void LLSelectNode::selectTE(S32 te_index, bool selected)
{
	if (te_index < 0 || te_index >= SELECT_MAX_TES)
	{
		return;
	}
	S32 mask = 0x1 << te_index;
	if (selected)
	{
		mTESelectMask |= mask;
	}
	else
	{
		mTESelectMask &= ~mask;
	}
	mLastTESelected = te_index;
}

bool LLSelectNode::isTESelected(S32 te_index)
{
	if (te_index < 0 || te_index >= mObject->getNumTEs())
	{
		return false;
	}
	return (mTESelectMask & (0x1 << te_index)) != 0;
}

S32 LLSelectNode::getLastSelectedTE()
{
	if (!isTESelected(mLastTESelected))
	{
		return -1;
	}
	return mLastTESelected;
}

LLViewerObject* LLSelectNode::getObject()
{
	if (!mObject)
	{
		return NULL;
	}
	else if (mObject->isDead())
	{
		mObject = NULL;
	}
	return mObject;
}

void LLSelectNode::saveColors()
{
	if (mObject.notNull())
	{
		mSavedColors.clear();
		for (S32 i = 0, count = mObject->getNumTEs(); i < count; ++i)
		{
			const LLTextureEntry* tep = mObject->getTE(i);
			if (tep)
			{
				mSavedColors.emplace_back(tep->getColor());
			}
		}
	}
}

void LLSelectNode::saveTextures(const uuid_vec_t& tex_ids)
{
	if (mObject.notNull())
	{
		mSavedTextures.clear();

		for (U32 i = 0, count = tex_ids.size(); i < count; ++i)
		{
			mSavedTextures.emplace_back(tex_ids[i]);
		}
	}
}

void LLSelectNode::saveGLTFMaterials(const uuid_vec_t& mat_ids,
									 const gltf_mat_vec_t& override_mats)
{
	if (mObject.isNull())
	{
		return;
	}

	mSavedGLTFMaterialIds.clear();
	mSavedGLTFOverrideMaterials.clear();

	for (U32 i = 0, count = mat_ids.size(); i < count; ++i)
	{
		mSavedGLTFMaterialIds.emplace_back(mat_ids[i]);
	}

	for (U32 i = 0, count = override_mats.size(); i < count; ++i)
	{
		mSavedGLTFOverrideMaterials.emplace_back(override_mats[i]);
	}
}

void LLSelectNode::saveTextureScaleRatios()
{
	mTextureScaleRatios.clear();

	if (mObject.isNull())
	{
		return;
	}

	LLVector3 scale = mObject->getScale();
	for (U8 i = 0, count = mObject->getNumTEs(); i < count; ++i)
	{
		const LLTextureEntry* tep = mObject->getTE(i);
		if (!tep) continue;

		F32 diffuse_s = 1.f;
		F32 diffuse_t = 1.f;
		LLVector3 v;

		U32 s_axis = VX;
		U32 t_axis = VY;
		LLPrimitive::getTESTAxes(i, &s_axis, &t_axis);

		tep->getScale(&diffuse_s, &diffuse_t);

		if (tep->getTexGen() == LLTextureEntry::TEX_GEN_PLANAR)
		{
			v.mV[s_axis] = diffuse_s * scale.mV[s_axis];
			v.mV[t_axis] = diffuse_t * scale.mV[t_axis];
			mTextureScaleRatios.emplace_back(v);
		}
		else
		{
			v.mV[s_axis] = diffuse_s / scale.mV[s_axis];
			v.mV[t_axis] = diffuse_t / scale.mV[t_axis];
			mTextureScaleRatios.emplace_back(v);
		}
	}
}

// This implementation should be similar to LLTask::allowOperationOnTask
bool LLSelectNode::allowOperationOnNode(PermissionBit op,
										U64 group_proxy_power) const
{
	// Extract ownership.
	bool object_is_group_owned = false;
	LLUUID object_owner_id;
	mPermissions->getOwnership(object_owner_id, object_is_group_owned);

	// Operations on invalid or public objects is not allowed.
	if (!mObject || mObject->isDead() || !mPermissions->isOwned())
	{
		return false;
	}

	// The transfer permissions can never be given through proxy.
	if (PERM_TRANSFER == op)
	{
		// The owner of an agent-owned object can transfer to themselves.
		if (!object_is_group_owned && gAgentID == object_owner_id)
		{
			return true;
		}
		else
		{
			// Otherwise check aggregate permissions.
			return mObject->permTransfer();
		}
	}

	if (PERM_MOVE == op || PERM_MODIFY == op)
	{
		// only owners can move or modify their attachments
		// no proxy allowed.
		if (mObject->isAttachment() && object_owner_id != gAgentID)
		{
			return false;
		}
	}

	// Calculate proxy_agent_id and group_id to use for permissions checks.
	// proxy_agent_id may be set to the object owner through group powers.
	// group_id can only be set to the object's group, if the agent is in that
	// group.
	LLUUID group_id;
	LLUUID proxy_agent_id = gAgentID;

	// Gods can always operate.
	if (gAgent.isGodlike())
	{
		return true;
	}

	// Check if the agent is in the same group as the object.
	LLUUID object_group_id = mPermissions->getGroup();
	if (object_group_id.notNull() &&
		gAgent.isInGroup(object_group_id))
	{
		// Assume the object's group during this operation.
		group_id = object_group_id;
	}

	// Only allow proxy powers for PERM_COPY if the actual agent can receive
	// the item (ie has PERM_TRANSFER permissions).
	// NOTE: op == PERM_TRANSFER has already been handled, but if that ever
	// changes we need to BLOCK proxy powers for PERM_TRANSFER. DK 03/28/06
	if (PERM_COPY != op || mPermissions->allowTransferTo(gAgentID))
	{
		// Check if the agent can assume ownership through group proxy or
		// agent-granted proxy.
		if ((object_is_group_owned &&
			 gAgent.hasPowerInGroup(object_owner_id, group_proxy_power)) ||
			 // Only allow proxy for move, modify, and copy.
			((PERM_MOVE == op || PERM_MODIFY == op || PERM_COPY == op) &&
			 (!object_is_group_owned && gAgent.isGrantedProxy(*mPermissions))))
		{
			// This agent is able to assume the ownership role for this
			// operation.
			proxy_agent_id = object_owner_id;
		}
	}

	// We now have max ownership information.
	if (PERM_OWNER == op)
	{
		// This this was just a check for ownership, we can now return the
		// answer.
		return proxy_agent_id == object_owner_id;
	}

	// Check permissions to see if the agent can operate
	return mPermissions->allowOperationBy(op, proxy_agent_id, group_id);
}

// Helper function for pushing relevant vertices from drawable to GL
void push_wireframe(LLDrawable* drawablep)
{
	LLVOVolume* vobj = drawablep->getVOVolume();
	if (vobj)
	{
		LLVertexBuffer::unbind();
		gGL.pushMatrix();
		gGL.multMatrix(vobj->getRelativeXform().getF32ptr());

		LLVolume* volume = NULL;

		if (drawablep->isState(LLDrawable::RIGGED))
		{
			vobj->updateRiggedVolume(true);
			volume = vobj->getRiggedVolume();
		}
		else
		{
			volume = vobj->getVolume();
		}

		if (volume)
		{
			for (S32 i = 0, count = volume->getNumVolumeFaces(); i < count;
				 ++i)
			{
				const LLVolumeFace& face = volume->getVolumeFace(i);
				LLVertexBuffer::drawElements(face.mNumVertices,
											 face.mPositions, NULL,
											 face.mNumIndices,
											 face.mIndices);
			}
		}

		gGL.popMatrix();
	}
}

void LLSelectNode::renderOneWireframe(const LLColor4& color)
{
	// MAINT-5018: needed because of crash on ATI 3800 (and similar cards)
	LLGLDisable multisample(!gUsePBRShaders && gGLManager.mIsAMD &&
							LLPipeline::RenderFSAASamples > 0 ? GL_MULTISAMPLE
															  : 0);

	LLViewerObject* objectp = getObject();
	if (!objectp)
	{
		return;
	}

	LLDrawable* drawablep = objectp->mDrawable;
	if (!drawablep)
	{
		return;
	}

	LLGLSLShader* shaderp = LLGLSLShader::sCurBoundShaderPtr;

	gDebugProgram.bind();

	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.pushMatrix();

	bool is_hud_object = objectp->isHUDAttachment();

	if (drawablep->isActive())
	{
		gGL.loadMatrix(gGLModelView);
		gGL.multMatrix(objectp->getRenderMatrix().getF32ptr());
	}
	else if (!is_hud_object)
	{
		gGL.loadIdentity();
		gGL.multMatrix(gGLModelView);
		LLVector3 trans = objectp->getRegion()->getOriginAgent();
		gGL.translatef(trans.mV[0], trans.mV[1], trans.mV[2]);
	}

	glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	if (LLSelectMgr::renderHiddenSelection())
	{
		gGL.blendFunc(LLRender::BF_SOURCE_COLOR, LLRender::BF_ONE);
		LLGLDepthTest gls_depth(GL_TRUE, GL_FALSE, GL_GEQUAL);
		gGL.diffuseColor4f(color.mV[VRED], color.mV[VGREEN], color.mV[VBLUE],
						   0.4f);
		push_wireframe(drawablep);
	}

	gGL.flush();
	gGL.setSceneBlendType(LLRender::BT_ALPHA);

	gGL.diffuseColor4f(color.mV[VRED] * 2, color.mV[VGREEN] * 2,
					   color.mV[VBLUE] * 2, LLSelectMgr::sHighlightAlpha * 2);

	LLGLEnable offset(GL_POLYGON_OFFSET_LINE);
	glPolygonOffset(3.f, 3.f);
	gGL.lineWidth(3.f);
	push_wireframe(drawablep);
	gGL.lineWidth(1.f);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	gGL.popMatrix();

	if (shaderp)
	{
		shaderp->bind();
	}
}

void LLSelectNode::renderOneSilhouette(const LLColor4& color, bool no_hidden)
{
	LLViewerObject* objectp = getObject();
	if (!objectp)
	{
		return;
	}

	LLDrawable* drawablep = objectp->mDrawable;
	if (!drawablep)
	{
		return;
	}

	LLVOVolume* vobj = drawablep->getVOVolume();
	if (vobj && vobj->isMesh())
	{
		renderOneWireframe(color);
		return;
	}

	if (!mSilhouetteGenerated)
	{
		return;
	}

	bool is_hud_object = objectp->isHUDAttachment();

	if (mSilhouetteVertices.size() == 0 ||
		mSilhouetteNormals.size() != mSilhouetteVertices.size())
	{
		return;
	}

	LLGLSLShader* shader = LLGLSLShader::sCurBoundShaderPtr;
	if (shader)
	{
		// Use UI program for selection highlights (texture color modulated by
		// vertex color).
		gUIProgram.bind();
	}

	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.pushMatrix();
	gGL.pushUIMatrix();
	gGL.loadUIIdentity();

	if (!is_hud_object)
	{
		gGL.loadIdentity();
		gGL.multMatrix(gGLModelView);
	}

	if (drawablep->isActive())
	{
		gGL.multMatrix(objectp->getRenderMatrix().getF32ptr());
	}

#if 0	// We used to only call this for volumes, but let's render silhouettes
		// for any node that has them.
	LLVolume* volume = objectp->getVolume();
	if (volume)
#endif
	{
		F32 silhouette_thickness;
		if (is_hud_object && isAgentAvatarValid())
		{
			silhouette_thickness = LLSelectMgr::sHighlightThickness /
								   gAgent.mHUDCurZoom;
		}
		else
		{
			LLVector3 view_vector = gViewerCamera.getOrigin() -
									objectp->getRenderPosition();
			silhouette_thickness = view_vector.length() *
								   LLSelectMgr::sHighlightThickness *
								   (gViewerCamera.getView() /
									gViewerCamera.getDefaultFOV());
		}
		F32 animationTime = (F32)LLFrameTimer::getElapsedSeconds();

		F32 u_coord = fmod(animationTime * LLSelectMgr::sHighlightUAnim, 1.f);
		F32 v_coord = 1.f - fmod(animationTime * LLSelectMgr::sHighlightVAnim, 1.f);
		F32 u_divisor = 1.f / ((F32)(mSilhouetteVertices.size() - 1));

		if (!no_hidden && LLSelectMgr::renderHiddenSelection())
		{
			gGL.flush();
			gGL.blendFunc(LLRender::BF_SOURCE_COLOR, LLRender::BF_ONE);

			LLGLDepthTest gls_depth(GL_TRUE, GL_FALSE, GL_GEQUAL);

			gGL.begin(LLRender::LINES);
			gGL.color4f(color.mV[VRED], color.mV[VGREEN], color.mV[VBLUE],
						0.4f);
			for (S32 i = 0, count = mSilhouetteVertices.size();
				 i < count; i += 2)
			{
				u_coord += u_divisor * LLSelectMgr::sHighlightUScale;
				gGL.texCoord2f(u_coord, v_coord);
				gGL.vertex3fv(mSilhouetteVertices[i].mV);
				u_coord += u_divisor * LLSelectMgr::sHighlightUScale;
				gGL.texCoord2f(u_coord, v_coord);
				gGL.vertex3fv(mSilhouetteVertices[i + 1].mV);
			}
			gGL.end();

			u_coord = fmod(animationTime * LLSelectMgr::sHighlightUAnim, 1.f);
		}

		gGL.flush();
		gGL.setSceneBlendType(LLRender::BT_ALPHA);

		gGL.begin(LLRender::TRIANGLES);
		LLVector3 v[4];
		LLVector2 tc[4];
		for (S32 i = 0, count = mSilhouetteVertices.size(); i < count; i += 2)
		{
			if (!mSilhouetteNormals[i].isFinite() ||
				!mSilhouetteNormals[i+1].isFinite())
			{
				// Skip skewed segments
				continue;
			}

			v[0] = mSilhouetteVertices[i] +
				   mSilhouetteNormals[i] * silhouette_thickness;
			tc[0].set(u_coord, v_coord + LLSelectMgr::sHighlightVScale);

			v[1] = mSilhouetteVertices[i];
			tc[1].set(u_coord, v_coord);

			u_coord += u_divisor * LLSelectMgr::sHighlightUScale;

			v[2] = mSilhouetteVertices[i + 1] +
				   mSilhouetteNormals[i + 1] * silhouette_thickness;
			tc[2].set(u_coord, v_coord + LLSelectMgr::sHighlightVScale);

			v[3] = mSilhouetteVertices[i + 1];
			tc[3].set(u_coord,v_coord);

			gGL.color4f(color.mV[VRED], color.mV[VGREEN],
						color.mV[VBLUE], 0.f); //LLSelectMgr::sHighlightAlpha);
			gGL.texCoord2fv(tc[0].mV);
			gGL.vertex3fv(v[0].mV);

			gGL.color4f(color.mV[VRED] * 2, color.mV[VGREEN] * 2,
						color.mV[VBLUE] * 2, LLSelectMgr::sHighlightAlpha * 2);
			gGL.texCoord2fv(tc[1].mV);
			gGL.vertex3fv(v[1].mV);

			gGL.color4f(color.mV[VRED], color.mV[VGREEN],
						color.mV[VBLUE], 0.f); //LLSelectMgr::sHighlightAlpha);
			gGL.texCoord2fv(tc[2].mV);
			gGL.vertex3fv(v[2].mV);

			gGL.vertex3fv(v[2].mV);

			gGL.color4f(color.mV[VRED] * 2, color.mV[VGREEN] * 2,
						color.mV[VBLUE] * 2, LLSelectMgr::sHighlightAlpha * 2);
			gGL.texCoord2fv(tc[1].mV);
			gGL.vertex3fv(v[1].mV);

			gGL.texCoord2fv(tc[3].mV);
			gGL.vertex3fv(v[3].mV);
		}
		gGL.end(true);
	}
	gGL.popMatrix();
	gGL.popUIMatrix();

	if (shader)
	{
		shader->bind();
	}
}

//
// Utility Functions
//

// Update everyone who cares about the selection list
void dialog_refresh_all()
{
	// This is the easiest place to fire the update signal, as it will make
	// cleaning up the functions below easier. Also, sometimes entities outside
	// the selection manager change properties of selected objects and call
	// into this function. Yuck.
	gSelectMgr.mUpdateSignal();

	if (gFloaterToolsp)
	{
		gFloaterToolsp->dirty();
	}

	if (gPieObjectp && gPieObjectp->getVisible())
	{
		gPieObjectp->arrange();
	}

	if (gPieAttachmentp && gPieAttachmentp->getVisible())
	{
		gPieAttachmentp->arrange();
	}

	LLFloaterProperties::dirtyAll();
	LLFloaterInspect::dirty();
}

void LLSelectMgr::updateSelectionCenter()
{
	// Movement threshold in meters for updating selection center (tractor
	// beam)
	constexpr F32 MOVE_SELECTION_THRESHOLD = 1.f;

	// Override any object updates received for selected objects
	overrideObjectUpdates();

	LLViewerObject* objp = mSelectedObjects->getFirstObject();
	if (!objp)
	{
		// Nothing selected, probably grabbing. Ignore by setting to avatar
		// origin.
		mSelectionCenterGlobal.clear();
		mShowSelection = false;
		mSelectionBBox = LLBBox();
		gAgent.resetHUDZoom();
	}
	else
	{
		mSelectedObjects->mSelectType = getSelectTypeForObject(objp);

		if (mSelectedObjects->mSelectType != SELECT_TYPE_HUD &&
			isAgentAvatarValid())
		{
			gAgent.resetHUDZoom();
		}

		mShowSelection = false;
		LLBBox bbox;

		// Have stuff selected
		LLVector3d select_center;
		// Keep a list of jointed objects for showing the joint HUDEffects

		// Initialize the bounding box to the root prim, so the BBox orientation
		// matches the root prim's (affecting the orientation of the
		// manipulators).
		bbox.addBBoxAgent((mSelectedObjects->getFirstRootObject(true))->getBoundingBoxAgent());

		for (LLObjectSelection::iterator iter = mSelectedObjects->begin(),
										 end = mSelectedObjects->end();
			 iter != end; ++iter)
		{
			LLSelectNode* nodep = *iter;
			objp = nodep->getObject();
			if (!objp) continue;

			LLViewerObject* rootp = objp->getRootEdit();
			//  Not an attachment
			if (mSelectedObjects->mSelectType == SELECT_TYPE_WORLD &&
				// Not the object you are sitting on
				!rootp->isChild(gAgentAvatarp) &&
				// Not another avatar
				!objp->isAvatar())
			{
				mShowSelection = true;
			}

			bbox.addBBoxAgent(objp->getBoundingBoxAgent());
		}

		LLVector3 bbox_center_agent = bbox.getCenterAgent();
		mSelectionCenterGlobal =
			gAgent.getPosGlobalFromAgent(bbox_center_agent);
		mSelectionBBox = bbox;
	}

	LLTool* toolp = gToolMgr.getCurrentTool();
	if (toolp && mShowSelection)
	{
		LLVector3d sel_center_global;
		if (toolp->isEditing())
		{
			sel_center_global = toolp->getEditingPointGlobal();
		}
		else
		{
			sel_center_global = mSelectionCenterGlobal;
		}

		// Send selection center if moved beyond threshold (used to animate
		// tractor beam)
		LLVector3d diff = sel_center_global - mLastSentSelectionCenterGlobal;
		if (diff.lengthSquared() >
				MOVE_SELECTION_THRESHOLD * MOVE_SELECTION_THRESHOLD)
		{
			// Transmit updated selection center
			mLastSentSelectionCenterGlobal = sel_center_global;
		}
	}

	// Give up edit menu if no objects selected
	if (!mSelectedObjects->getObjectCount())
	{
		releaseMenuHandler();
	}

	pauseAssociatedAvatars();
}

// If the selection includes an attachment or an animated object, the parent
// avatars should pause their animations until they are no longer selected.
void LLSelectMgr::pauseAssociatedAvatars()
{
	mPauseRequests.clear();

	bool agent_valid = isAgentAvatarValid();
	for (LLObjectSelection::iterator iter = mSelectedObjects->begin(),
									 end = mSelectedObjects->end();
		 iter != end; ++iter)
	{
		LLSelectNode* node = *iter;
		if (!node) continue;	// Pananoia

		LLViewerObject* object = node->getObject();
		if (!object) continue;

		mSelectedObjects->mSelectType = getSelectTypeForObject(object);

		bool animated_object = object->isAnimatedObject();
		if (mSelectedObjects->mSelectType == SELECT_TYPE_ATTACHMENT &&
			agent_valid && object->getAvatarAncestor())
		{
			if (animated_object)
			{
				// This is an animated object attachment, so pause the
				// puppet avatar as well.
				LLVOAvatarPuppet* puppet = object->getPuppetAvatar();
				if (puppet)
				{
					mPauseRequests.emplace_back(puppet->requestPause());
				}
			}
			mPauseRequests.emplace_back(gAgentAvatarp->requestPause());
		}
		else if (object && animated_object)
		{
			LLVOAvatarPuppet* puppet = object->getPuppetAvatar();
			if (puppet)
			{
				// This is a non-attached animated object, so pause the
				// puppet avatar only.
				mPauseRequests.emplace_back(puppet->requestPause());
			}
		}
	}
}

void LLSelectMgr::updatePointAt()
{
	if (mShowSelection)
	{
		if (mSelectedObjects->getObjectCount())
		{
			const LLPickInfo& pick = gViewerWindowp->getLastPick();
			LLViewerObject* click_object = pick.getObject();
			if (click_object && click_object->isSelected())
			{
				// Clicked on another object in our selection group, use that
				// as target
				LLVector3 select_offset(pick.mObjectOffset);
				select_offset.rotVec(~click_object->getRenderRotation());

				gAgent.setPointAt(POINTAT_TARGET_SELECT, click_object,
								  select_offset);
				gAgent.setLookAt(LOOKAT_TARGET_SELECT, click_object,
								 select_offset);
			}
			else
			{
				// Did not click on an object this time, revert to pointing at
				// center of first object
				gAgent.setPointAt(POINTAT_TARGET_SELECT,
								  mSelectedObjects->getFirstObject());
				gAgent.setLookAt(LOOKAT_TARGET_SELECT,
								 mSelectedObjects->getFirstObject());
			}
		}
		else
		{
			gAgent.setPointAt(POINTAT_TARGET_CLEAR);
			gAgent.setLookAt(LOOKAT_TARGET_CLEAR);
		}
	}
	else
	{
		gAgent.setPointAt(POINTAT_TARGET_CLEAR);
		gAgent.setLookAt(LOOKAT_TARGET_CLEAR);
	}
}

LLBBox LLSelectMgr::getBBoxOfSelection() const
{
	return mSelectionBBox;
}

bool LLSelectMgr::canUndo() const
{
	// *HACK: casting away constness - MG
	LLSelectMgr* self = const_cast<LLSelectMgr*>(this);
	// Can edit or move
	return self->mSelectedObjects->getFirstUndoEnabledObject() != NULL;
}

void LLSelectMgr::undo()
{
	bool select_linked_set = !mEditLinkedParts;
	LLUUID group_id(gAgent.getGroupID());
	sendListToRegions("Undo", packAgentAndSessionAndGroupID, packObjectID,
					  &group_id, select_linked_set ? SEND_ONLY_ROOTS
												   : SEND_CHILDREN_FIRST);
}

bool LLSelectMgr::canRedo() const
{
	// *HACK: casting away constness - MG
	LLSelectMgr* self = const_cast<LLSelectMgr*>(this);
	return self->mSelectedObjects->getFirstEditableObject() != NULL;
}

void LLSelectMgr::redo()
{
	bool select_linked_set = !mEditLinkedParts;
	LLUUID group_id(gAgent.getGroupID());
	sendListToRegions("Redo",
					  packAgentAndSessionAndGroupID,
					  packObjectID,
					  &group_id,
					  select_linked_set ? SEND_ONLY_ROOTS
										: SEND_CHILDREN_FIRST);
}

bool LLSelectMgr::canDoDelete() const
{
	// *HACK: casting away constness - MG
	LLSelectMgr* self = const_cast<LLSelectMgr*>(this);
	// Note: Can only delete root objects (see getFirstDeleteableObject() for
	// more info).
	return self->mSelectedObjects->getFirstDeleteableObject() != NULL;
}

void LLSelectMgr::doDelete()
{
	selectDelete();
}

bool LLSelectMgr::canDeselect() const
{
	return !mSelectedObjects->isEmpty();
}

void LLSelectMgr::deselect()
{
	deselectAll();
}

bool LLSelectMgr::canDuplicate() const
{
	// *HACK: casting away constness - MG
	LLSelectMgr* self = const_cast<LLSelectMgr*>(this);
	return self->mSelectedObjects->getFirstCopyableObject() != NULL;
}

void LLSelectMgr::duplicate()
{
	LLVector3 offset(0.5f, 0.5f, 0.f);
	selectDuplicate(offset, true);
}

ESelectType LLSelectMgr::getSelectTypeForObject(LLViewerObject* object)
{
	if (!object)
	{
		return SELECT_TYPE_WORLD;
	}
	if (object->isHUDAttachment())
	{
		return SELECT_TYPE_HUD;
	}
	else if (object->isAttachment())
	{
		return SELECT_TYPE_ATTACHMENT;
	}
	else
	{
		return SELECT_TYPE_WORLD;
	}
}

void LLSelectMgr::validateSelection()
{
	struct f final : public LLSelectedObjectFunctor
	{
		bool apply(LLViewerObject* object) override
		{
			if (!gSelectMgr.canSelectObject(object))
			{
				gSelectMgr.deselectObjectOnly(object);
			}
			return true;
		}
	} func;
	getSelection()->applyToObjects(&func);
}

bool LLSelectMgr::canSelectObject(LLViewerObject* object)
{
	// Never select dead objects
	if (!object || object->isDead())
	{
		return false;
	}

	if (mForceSelection)
	{
		return true;
	}

	// Cannot select orphans, avatars or land
	if (object->isOrphaned() || object->isAvatar() ||
		object->getPCode() == LLViewerObject::LL_VO_SURFACE_PATCH)
	{
		return false;
	}

	if ((mSelectOwnedOnly && !object->permYouOwner()) ||
		(mSelectMovableOnly &&
		 (!object->permMove() || object->isPermanentEnforced())))
	{
		// Only select my own objects
		return false;
	}

	ESelectType selection_type = getSelectTypeForObject(object);
	if (mSelectedObjects->getObjectCount() > 0 &&
		mSelectedObjects->mSelectType != selection_type)
	{
		return false;
	}

	return true;
}

bool LLSelectMgr::setForceSelection(bool force)
{
	std::swap(mForceSelection, force);
	return force;
}

bool LLSelectMgr::selectionMove(const LLVector3& displ, F32 roll, F32 pitch,
								F32 yaw, U32 update_type)
{
	if (update_type == UPD_NONE)
	{
		return false;
	}

	LLVector3 displ_global;
	bool update_success = true;
	bool update_position = update_type & UPD_POSITION;
	bool update_rotation = update_type & UPD_ROTATION;
	const bool noedit_linked_parts = !mEditLinkedParts;

	if (update_position)
	{
		// Calculate the distance of the object closest to the camera origin
		F32 min_dist_squared = F32_MAX; // Value will be overridden in the loop
		LLVector3 obj_pos;
		for (LLObjectSelection::root_iterator
				it = getSelection()->root_begin(),
				end = getSelection()->root_end();
			 it != end; ++it)
		{
			obj_pos = (*it)->getObject()->getPositionEdit();

			F32 obj_dist_squared = dist_vec_squared(obj_pos,
													gViewerCamera.getOrigin());
			if (obj_dist_squared < min_dist_squared)
			{
				min_dist_squared = obj_dist_squared;
			}
		}

		// Factor the distance inside the displacement vector. This will get us
		// equally visible movements for both close and far away selections.
		F32 min_dist = sqrtf(sqrtf(min_dist_squared)) * 0.5f;
		displ_global.set(displ.mV[0] * min_dist, displ.mV[1] * min_dist,
						 displ.mV[2] * min_dist);

		// Equates to: Displ_global = Displ * M_cam_axes_in_global_frame
		displ_global = gViewerCamera.rotateToAbsolute(displ_global);
	}

	LLQuaternion new_rot;
	if (update_rotation)
	{
		// Let's calculate the rotation around each camera axes
		LLQuaternion qx(roll, gViewerCamera.getAtAxis());
		LLQuaternion qy(pitch, gViewerCamera.getLeftAxis());
		LLQuaternion qz(yaw, gViewerCamera.getUpAxis());
		new_rot.set(qx * qy * qz);
	}

	S32 obj_count = getSelection()->getObjectCount();
	for (LLObjectSelection::root_iterator it = getSelection()->root_begin(),
										  end = getSelection()->root_end();
		 it != end; ++it)
	{
		LLViewerObject* obj = (*it)->getObject();
		if (!obj) continue;	// Paranoia

		bool enable_pos = false;
		bool enable_rot = false;
		bool perm_move = obj->permMove() && !obj->isPermanentEnforced();
		bool perm_mod = obj->permModify();

		LLVector3d sel_center(getSelectionCenterGlobal());

		if (update_rotation)
		{
			enable_rot = perm_move &&
						 ((perm_mod && !obj->isAttachment()) ||
						   noedit_linked_parts);

			if (enable_rot)
			{
				S32 children_count = obj->getChildren().size();
				if (obj_count > 1 && children_count > 0)
				{
					// For linked sets, rotate around the group center
					const LLVector3 t(obj->getPositionGlobal() - sel_center);

					// Ra = T x R x T^-1
					LLMatrix4 mt;
					mt.setTranslation(t);
					const LLMatrix4 mnew_rot(new_rot);
					LLMatrix4 mt_1;	mt_1.setTranslation(-t);
					mt *= mnew_rot;
					mt *= mt_1;

					// Rfin = Rcur * Ra
					obj->setRotation(obj->getRotationEdit() * mt.quaternion());
					displ_global += mt.getTranslation();
				}
				else
				{
					obj->setRotation(obj->getRotationEdit() * new_rot);
				}
			}
			else
			{
				update_success = false;
			}
		}

		if (update_position)
		{
			// Establish if object can be moved or not
			enable_pos = perm_move && !obj->isAttachment() &&
						 (perm_mod || noedit_linked_parts);

			if (enable_pos)
			{
				obj->setPositionLocal(obj->getPositionEdit() + displ_global);
			}
			else
			{
				update_success = false;
			}
		}

		if (enable_pos && enable_rot && obj->mDrawable.notNull())
		{
			gPipeline.markMoved(obj->mDrawable, true);
		}
	}

	if (update_position && update_success && obj_count > 1)
	{
		updateSelectionCenter();
	}

	return update_success;
}

void LLSelectMgr::sendSelectionMove()
{
	LLSelectNode* node = mSelectedObjects->getFirstRootNode();
	if (!node)
	{
		return;
	}

#if 0
	saveSelectedObjectTransform(SELECT_ACTION_TYPE_PICK);
#endif

	U32 update_type = UPD_POSITION | UPD_ROTATION;
	// Apply to linked objects if unable to select their individual parts
	if (!mEditLinkedParts && !getTEMode())
	{
		// Tell simulator to apply to whole linked sets
		update_type |= UPD_LINKED_SETS;
	}

	LLViewerRegion* curr_region = node->getObject()->getRegion();
	S32 objects_in_this_packet = 0;

	// Prepare first bulk message
	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessage("MultipleObjectUpdate");
	packAgentAndSessionID(&update_type);

	for (LLObjectSelection::root_iterator it = getSelection()->root_begin();
		 it != getSelection()->root_end(); ++it)
	{
		node = *it;
		if (!node) continue;	// Paranoia

		LLViewerObject* obj = node->getObject();
		if (!obj) continue;	// Paranoia

		// Note: following code adapted from sendListToRegions()
		LLViewerRegion* last_region = curr_region;
		curr_region = obj->getRegion();

		// If not simulator or message too big
		if (curr_region != last_region || msg->isSendFull(NULL) ||
			objects_in_this_packet >= MAX_OBJECTS_PER_PACKET)
		{
			// Send the current message to the sim and start new one
			msg->sendReliable(last_region->getHost());
			objects_in_this_packet = 0;
			msg->newMessage("MultipleObjectUpdate");
			packAgentAndSessionID(&update_type);
		}

		// Add another instance of the body of data
		packMultipleUpdate(node, &update_type);
		++objects_in_this_packet;
	}

	// Flush remaining messages
	if (msg->getCurrentSendTotal() > 0)
	{
		msg->sendReliable(curr_region->getHost());
	}
	else
	{
		msg->clearMessage();
	}

#if 0
	saveSelectedObjectTransform(SELECT_ACTION_TYPE_PICK);
#endif
}

/////////////////////////////////////////////////////////////////////////////
// LLObjectSelection class
/////////////////////////////////////////////////////////////////////////////

// Object selection iterator helpers
bool LLObjectSelection::is_root::operator()(LLSelectNode* node)
{
	LLViewerObject* object = node->getObject();
	return object && !node->mIndividualSelection && object->isRootEdit();
}

bool LLObjectSelection::is_valid_root::operator()(LLSelectNode* node)
{
	LLViewerObject* object = node->getObject();
	return object && node->mValid && !node->mIndividualSelection &&
		   object->isRootEdit();
}

bool LLObjectSelection::is_root_object::operator()(LLSelectNode* node)
{
	LLViewerObject* object = node->getObject();
	return object && object->isRootEdit();
}

LLObjectSelection::LLObjectSelection()
:	LLRefCount(),
	mSelectType(SELECT_TYPE_WORLD)
{
}

LLObjectSelection::~LLObjectSelection()
{
	deleteAllNodes();
}

void LLObjectSelection::cleanupNodes()
{
	for (list_t::iterator iter = mList.begin(), end = mList.end();
		 iter != end; )
	{
		list_t::iterator curiter = iter++;
		LLSelectNode* node = *curiter;
		if (!node || !node->getObject() || node->getObject()->isDead())
		{
			mList.erase(curiter);
			delete node;
		}
	}
}

S32 LLObjectSelection::getNumNodes()
{
	return mList.size();
}

void LLObjectSelection::addNode(LLSelectNode* nodep)
{
	llassert_always(nodep->getObject() && !nodep->getObject()->isDead());
	mList.push_front(nodep);
	mSelectNodeMap[nodep->getObject()] = nodep;
}

void LLObjectSelection::addNodeAtEnd(LLSelectNode* nodep)
{
	llassert_always(nodep->getObject() && !nodep->getObject()->isDead());
	mList.push_back(nodep);
	mSelectNodeMap[nodep->getObject()] = nodep;
}

void LLObjectSelection::moveNodeToFront(LLSelectNode* nodep)
{
	mList.remove(nodep);
	mList.push_front(nodep);
}

void LLObjectSelection::removeNode(LLSelectNode* nodep)
{
	mSelectNodeMap.erase(nodep->getObject());
	if (nodep->getObject() == mPrimaryObject)
	{
		mPrimaryObject = NULL;
	}
	nodep->setObject(NULL); // Will get erased in cleanupNodes()
	mList.remove(nodep);
}

void LLObjectSelection::deleteAllNodes()
{
	std::for_each(mList.begin(), mList.end(), DeletePointer());
	mList.clear();
	mSelectNodeMap.clear();
	mPrimaryObject = NULL;
}

LLSelectNode* LLObjectSelection::findNode(LLViewerObject* objectp)
{
	if (objectp)
	{
		std::map<LLPointer<LLViewerObject>, LLSelectNode*>::iterator it =
			mSelectNodeMap.find(objectp);
		if (it != mSelectNodeMap.end())
		{
			return it->second;
		}
	}
	return NULL;
}

F32 LLObjectSelection::getSelectedObjectCost()
{
	cleanupNodes();
	F32 cost = 0.f;

	for (list_t::iterator iter = mList.begin(), end = mList.end();
		 iter != end; ++iter)
	{
		LLSelectNode* node = *iter;
		if (!node) continue;	// Paranoia

		LLViewerObject* object = node->getObject();
		if (object)
		{
			cost += object->getObjectCost();
		}
	}

	return cost;
}

F32 LLObjectSelection::getSelectedLinksetCost()
{
	cleanupNodes();
	F32 cost = 0.f;
	fast_hset<LLViewerObject*> me_roots;

	for (list_t::iterator iter = mList.begin(), end = mList.end();
		 iter != end; ++iter)
	{
		LLSelectNode* node = *iter;
		if (!node) continue;	// Paranoia

		LLViewerObject* object = node->getObject();
		if (object && !object->isAttachment())
		{
			LLViewerObject* root = (LLViewerObject*)object->getRoot();
			if (root && !me_roots.count(root))
			{
				me_roots.insert(root);
				cost += root->getLinksetCost();
			}
		}
	}

	return cost;
}

F32 LLObjectSelection::getSelectedPhysicsCost()
{
	cleanupNodes();
	F32 cost = 0.f;

	for (list_t::iterator iter = mList.begin(), end = mList.end();
		 iter != end; ++iter)
	{
		LLSelectNode* node = *iter;
		if (!node) continue;	// Paranoia

		LLViewerObject* object = node->getObject();
		if (object)
		{
			cost += object->getPhysicsCost();
		}
	}

	return cost;
}

F32 LLObjectSelection::getSelectedLinksetPhysicsCost()
{
	cleanupNodes();
	F32 cost = 0.f;
	fast_hset<LLViewerObject*> me_roots;

	for (list_t::iterator iter = mList.begin(), end = mList.end();
		 iter != end; ++iter)
	{
		LLSelectNode* node = *iter;
		if (!node) continue;	// Paranoia

		LLViewerObject* object = node->getObject();
		if (object)
		{
			LLViewerObject* root = (LLViewerObject*)object->getRoot();
			if (root && !me_roots.count(root))
			{
				me_roots.insert(root);
				cost += root->getLinksetPhysicsCost();
			}
		}
	}

	return cost;
}

F32 LLObjectSelection::getSelectedObjectStreamingCost(S32* total_bytes,
													  S32* visible_bytes)
{
	F32 cost = 0.f;
	for (list_t::iterator iter = mList.begin(), end = mList.end();
		 iter != end; ++iter)
	{
		LLSelectNode* node = *iter;
		if (!node) continue;	// Paranoia

		LLViewerObject* object = node->getObject();
		if (object)
		{
			S32 bytes = 0;
			S32 visible = 0;
			cost += object->getStreamingCost(&bytes, &visible);

			if (total_bytes)
			{
				*total_bytes += bytes;
			}

			if (visible_bytes)
			{
				*visible_bytes += visible;
			}
		}
	}

	return cost;
}

U32 LLObjectSelection::getSelectedObjectTriangleCount(S32* vcount)
{
	U32 count = 0;
	for (list_t::iterator iter = mList.begin(), end = mList.end();
		 iter != end; ++iter)
	{
		LLSelectNode* node = *iter;
		if (!node) continue;	// Paranoia

		LLViewerObject* object = node->getObject();
		if (object)
		{
			S32 vt = 0;
			count += object->getTriangleCount(&vt);
			*vcount += vt;
		}
	}

	return count;
}

S32 LLObjectSelection::getTECount()
{
	S32 count = 0;
	for (LLObjectSelection::iterator iter = begin(); iter != end(); ++iter)
	{
		LLSelectNode* node = *iter;
		if (!node) continue;	// Paranoia

		LLViewerObject* object = node->getObject();
		if (object)
		{
			S32 num_tes = object->getNumTEs();
			for (S32 te = 0; te < num_tes; ++te)
			{
				if (node->isTESelected(te))
				{
					++count;
				}
			}
		}
	}
	return count;
}

S32 LLObjectSelection::getRootObjectCount()
{
	S32 count = 0;
	for (LLObjectSelection::root_iterator iter = root_begin();
		 iter != root_end(); ++iter)
	{
		++count;
	}
	return count;
}

bool LLObjectSelection::applyToObjects(LLSelectedObjectFunctor* func)
{
	bool result = true;
	for (iterator iter = begin(); iter != end(); )
	{
		iterator cutiter = iter++;
		LLSelectNode* nodep = *cutiter;
		if (!nodep) continue;	// Paranoia

		LLViewerObject* objectp = nodep->getObject();
		if (objectp)
		{
			// Always apply functor, even if result == false already
			bool r = func->apply(objectp);
			// Then, AND the results
			result &= r;
		}
	}
	return result;
}

void LLObjectSelection::applyNoCopyTextureToTEs(LLViewerInventoryItem* itemp)
{
	if (!itemp) return;

	LLViewerTexture* texp =
		LLViewerTextureManager::getFetchedTexture(itemp->getAssetUUID());
	if (!texp) return;

	constexpr LLToolDragAndDrop::ESource source =
		LLToolDragAndDrop::SOURCE_AGENT;
	for (iterator iter = begin(); iter != end(); )
	{
		iterator cutiter = iter++;
		LLSelectNode* nodep = *cutiter;
		if (!nodep) continue;	// Paranoia

		LLViewerObject* objectp = nodep->getObject();
		if (!objectp) continue;

		bool texture_copied = false;
		bool updated = false;
		U32 num_tes = llmin(objectp->getNumTEs(), objectp->getNumFaces());
		for (U32 te = 0; te < num_tes; ++te)
		{
			if (!nodep->isTESelected(te)) continue;
			if (!texture_copied)
			{
				texture_copied = true;
				if (LLToolDragAndDrop::handleDropAssetProtections(objectp,
																  itemp,
																  source))
				{
					gViewerStats.incStat(LLViewerStats::ST_EDIT_TEXTURE_COUNT);
				}
			}
			// Apply texture to the selected face
			objectp->setTEImage(te, texp);
			updated = true;
		}

		// *TODO: check whether this is necessary or not (i.e. not already
		// done).
		if (updated)
		{
			dialog_refresh_all();
			objectp->sendTEUpdate();
		}
	}
}

bool LLObjectSelection::applyRestrictedPbrMatToTEs(LLViewerInventoryItem* itemp)
{
	if (!itemp)
	{
		return false;
	}

	bool success = true;

	LLUUID asset_id = itemp->getAssetUUID();
	if (asset_id.isNull())
	{
		asset_id = BLANK_MATERIAL_ASSET_ID;
	}

	constexpr LLToolDragAndDrop::ESource source =
		LLToolDragAndDrop::SOURCE_AGENT;
	for (iterator iter = begin(); iter != end(); )
	{
		iterator cutiter = iter++;
		LLSelectNode* nodep = *cutiter;
		if (!nodep) continue;	// Paranoia

		LLViewerObject* objectp = nodep->getObject();
		if (!objectp) continue;

		bool material_copied = false;
		U32 num_tes = llmin(objectp->getNumTEs(), objectp->getNumFaces());
		for (U32 te = 0; te < num_tes; ++te)
		{
			if (!nodep->isTESelected(te)) continue;

			// No-copy, no-modify, no-transfer materials must be moved to the
			// object inventory only once without making any copies.
			if (!material_copied && asset_id.notNull())
			{
				material_copied =
					LLToolDragAndDrop::handleDropAssetProtections(objectp,
																  itemp,
																  source);
				if (!material_copied)
				{
					// Applying the material is not possible for this object
					// given the current inventory.
					success = false;
					break;
				}
			}
			// Apply material to the selected face; blanks out most override
			// data on the server.
			objectp->setRenderMaterialID(te, asset_id);
		}
	}

	LLGLTFMaterialList::flushUpdates();

	return success;
}

bool LLObjectSelection::checkAnimatedObjectEstTris()
{
	F32 est_tris = 0.f;
	F32 max_tris =  0.f;
	S32 anim_count = 0;
	for (root_iterator iter = root_begin(), end = root_end();
		 iter != end; ++iter)
	{
		LLSelectNode* node = *iter;
		if (!node) continue;	// Paranoia

		LLViewerObject* object = node->getObject();
		if (!object) continue;

		if (object->isAnimatedObject())
		{
			++anim_count;
		}
		est_tris += object->recursiveGetEstTrianglesMax();
		max_tris = llmax(max_tris, (F32)object->getAnimatedObjectMaxTris());
	}
	return anim_count == 0 || est_tris <= max_tris;
}

bool LLObjectSelection::applyToRootObjects(LLSelectedObjectFunctor* func,
										   bool firstonly)
{
	bool result = firstonly ? false : true;
	for (root_iterator iter = root_begin(); iter != root_end(); )
	{
		root_iterator nextiter = iter++;
		LLSelectNode* node = *nextiter;
		if (!node) continue;	// Paranoia

		LLViewerObject* object = node->getObject();
		if (object)
		{
			bool r = func->apply(object);

			if (firstonly && r)
			{
				return true;
			}

			result = result && r;
		}
	}
	return result;
}

bool LLObjectSelection::applyToTEs(LLSelectedTEFunctor* func, bool firstonly)
{
	bool result = !firstonly;
	for (iterator iter = begin(); iter != end(); )
	{
		iterator nextiter = iter++;
		LLSelectNode* node = *nextiter;
		if (!node) continue;	// Paranoia

		LLViewerObject* object = node->getObject();
		if (!object)
		{
			continue;
		}
		// Avatars have TEs but no faces
		S32 num_tes = llmin((S32)object->getNumTEs(),
							(S32)object->getNumFaces());
		for (S32 te = 0; te < num_tes; ++te)
		{
			if (node->isTESelected(te))
			{
				bool r = func->apply(object, te);

				if (firstonly && r)
				{
					return true;
				}

				result = result && r;
			}
		}
	}
	return result;
}

bool LLObjectSelection::applyToNodes(LLSelectedNodeFunctor* func,
									 bool firstonly)
{
	bool result = !firstonly;
	for (iterator iter = begin(); iter != end(); )
	{
		iterator nextiter = iter++;
		LLSelectNode* node = *nextiter;
		if (!node) continue;	// Paranoia

		bool r = func->apply(node);
		if (firstonly && r)
		{
			return true;
		}

		result = result && r;
	}
	return result;
}

bool LLObjectSelection::applyToRootNodes(LLSelectedNodeFunctor *func,
										 bool firstonly)
{
	bool result = !firstonly;
	for (root_iterator iter = root_begin(); iter != root_end(); )
	{
		root_iterator nextiter = iter++;
		LLSelectNode* node = *nextiter;
		if (!node) continue;	// Paranoia

		bool r = func->apply(node);
		if (firstonly && r)
		{
			return true;
		}
		else
		{
			result = result && r;
		}
	}
	return result;
}

bool LLObjectSelection::isMultipleTESelected()
{
	bool te_selected = false;
	// ...all faces
	for (LLObjectSelection::iterator iter = begin(); iter != end(); ++iter)
	{
		LLSelectNode* nodep = *iter;
		if (!nodep) continue;	// Paranoia

		for (S32 i = 0; i < SELECT_MAX_TES; ++i)
		{
			if (nodep->isTESelected(i))
			{
				if (te_selected)
				{
					return true;
				}
				te_selected = true;
			}
		}
	}
	return false;
}

bool LLObjectSelection::getSelectedTEValue(LLSelectedTEGetFunctor<F32>* func,
										   F32& res, F32 tolerance)
{
	bool have_first = false;
	bool have_selected = false;
	F32 selected_value = 0.f;

	// Now iterate through all TEs to test for sameness
	bool identical = true;
	for (iterator iter = begin(); iter != end(); ++iter)
	{
		LLSelectNode* node = *iter;
		LLViewerObject* object = node->getObject();
		S32 selected_te = -1;
		if (object == getPrimaryObject())
		{
			selected_te = node->getLastSelectedTE();
		}
		for (S32 te = 0, count = object->getNumTEs(); te < count; ++te)
		{
			if (!node->isTESelected(te))
			{
				continue;
			}
			F32 value = func->get(object, te);
			if (!have_first)
			{
				have_first = true;
				if (!have_selected)
				{
					selected_value = value;
				}
			}
			else
			{
				if (fabs(value - selected_value) > tolerance)
				{
					identical = false;
				}
				if (te == selected_te)
				{
					selected_value = value;
					have_selected = true;
				}
			}
		}
		if (!identical && have_selected)
		{
			break;
		}
	}
	if (have_first || have_selected)
	{
		res = selected_value;
	}
	return identical;
}

bool LLObjectSelection::contains(LLViewerObject* object)
{
	return findNode(object) != NULL;
}

bool LLObjectSelection::contains(LLViewerObject* object, S32 te)
{
	if (te == SELECT_ALL_TES)
	{
		// ...all faces
		for (LLObjectSelection::iterator iter = begin();
			 iter != end(); ++iter)
		{
			LLSelectNode* nodep = *iter;
			if (nodep && nodep->getObject() == object)
			{
				// Optimization
				if (nodep->getTESelectMask() == TE_SELECT_MASK_ALL)
				{
					return true;
				}

				bool all_selected = true;
				for (S32 i = 0, count = object->getNumTEs(); i < count; ++i)
				{
					all_selected = all_selected && nodep->isTESelected(i);
				}
				return all_selected;
			}
		}
		return false;
	}
	else
	{
		// ... one face
		for (LLObjectSelection::iterator iter = begin(); iter != end(); ++iter)
		{
			LLSelectNode* nodep = *iter;
			if (nodep &&
				nodep->getObject() == object && nodep->isTESelected(te))
			{
				return true;
			}
		}
		return false;
	}
}

// Returns true is any node is currenly worn as an attachment
bool LLObjectSelection::isAttachment()
{
	return mSelectType == SELECT_TYPE_ATTACHMENT ||
		   mSelectType == SELECT_TYPE_HUD;
}

LLSelectNode* LLObjectSelection::getFirstNode(LLSelectedNodeFunctor* func)
{
	for (iterator iter = begin(); iter != end(); ++iter)
	{
		LLSelectNode* node = *iter;
		if (!func || (node && func->apply(node)))
		{
			return node;
		}
	}
	return NULL;
}

LLSelectNode* LLObjectSelection::getFirstRootNode(LLSelectedNodeFunctor* func,
												  bool non_root_ok)
{
	for (root_iterator iter = root_begin(); iter != root_end(); ++iter)
	{
		LLSelectNode* node = *iter;
		if (!func || (node && func->apply(node)))
		{
			return node;
		}
	}
	if (non_root_ok)
	{
		// Get non root
		return getFirstNode(func);
	}
	return NULL;
}

LLViewerObject* LLObjectSelection::getFirstSelectedObject(LLSelectedNodeFunctor* func,
														  bool get_parent)
{
	LLSelectNode* res = getFirstNode(func);
	if (!res)
	{
		return NULL;
	}

	if (get_parent)
	{
		return getSelectedParentObject(res->getObject());
	}

	return res->getObject();
}

LLSelectNode* LLObjectSelection::getFirstMoveableNode(bool get_root_first)
{
	struct f final : public LLSelectedNodeFunctor
	{
		bool apply(LLSelectNode* node) override
		{
			LLViewerObject* obj = node->getObject();
			return obj && obj->permMove() && !obj->isPermanentEnforced();
		}
	} func;
	LLSelectNode* res = get_root_first ? getFirstRootNode(&func, true)
									   : getFirstNode(&func);
	return res;
}

LLViewerObject* LLObjectSelection::getFirstCopyableObject(bool get_parent)
{
	struct f final : public LLSelectedNodeFunctor
	{
		bool apply(LLSelectNode* node) override
		{
			LLViewerObject* obj = node->getObject();
			return obj && obj->permCopy() && !obj->isAttachment();
		}
	} func;
	return getFirstSelectedObject(&func, get_parent);
}

LLViewerObject* LLObjectSelection::getFirstDeleteableObject()
{
	// RN: do not currently support deletion of child objects, as that requires
	// separating them first then derezzing to trash

	struct f final : public LLSelectedNodeFunctor
	{
		bool apply(LLSelectNode* node) override
		{
			LLViewerObject* obj = node->getObject();
			// You can delete an object if it is not an attachment and you are
			// the owner or you have permission to modify it.
			if (obj && !obj->isAttachment() && !obj->isPermanentEnforced() &&
				(obj->permModify() || obj->permYouOwner() ||
				 !obj->permAnyOwner()))		// not public
			{
				return true;
			}
			return false;
		}
	} func;
	LLSelectNode* node = getFirstNode(&func);
	return node ? node->getObject() : NULL;
}

LLViewerObject* LLObjectSelection::getFirstEditableObject(bool get_parent)
{
	struct f final : public LLSelectedNodeFunctor
	{
		bool apply(LLSelectNode* node) override
		{
			LLViewerObject* obj = node->getObject();
			return obj && obj->permModify();
		}
	} func;
	return getFirstSelectedObject(&func, get_parent);
}

LLViewerObject* LLObjectSelection::getFirstMoveableObject(bool get_parent)
{
	struct f final : public LLSelectedNodeFunctor
	{
		bool apply(LLSelectNode* node) override
		{
			LLViewerObject* obj = node->getObject();
			return obj && obj->permMove() && !obj->isPermanentEnforced();
		}
	} func;
	return getFirstSelectedObject(&func, get_parent);
}

LLViewerObject* LLObjectSelection::getFirstUndoEnabledObject(bool get_parent)
{
	struct f final : public LLSelectedNodeFunctor
	{
		bool apply(LLSelectNode* node) override
		{
			LLViewerObject* obj = node->getObject();
			return obj &&
				   (obj->permModify() ||
					(obj->permMove() && !obj->isPermanentEnforced()));
		}
	} func;
	return getFirstSelectedObject(&func, get_parent);
}
