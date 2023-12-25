/**
 * @file lltoolplacer.cpp
 * @brief Tool for placing new objects into the world
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

#include "lltoolplacer.h"

#include "llaudioengine.h"
#include "llbutton.h"
#include "llparcel.h"
#include "llprimitive.h"
#include "roles_constants.h"

#include "llagent.h"
#include "llfirstuse.h"
#include "llfloatertools.h"
#include "llhudeffectspiral.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llselectmgr.h"
#include "llstatusbar.h"
#include "lltoolcomp.h"
#include "lltoolmgr.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerobjectlist.h"
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"
#include "llviewerstats.h"
#include "llviewerwindow.h"
#include "llvoavatarself.h"
#include "llvograss.h"
#include "llvolumemessage.h"
#include "llvotree.h"
#include "llworld.h"

const LLVector3 DEFAULT_OBJECT_SCALE(0.5f, 0.5f, 0.5f);

//static
LLPCode	LLToolPlacer::sObjectType = LL_PCODE_CUBE;

LLToolPlacer::LLToolPlacer()
:	LLTool("Create")
{
}

bool LLToolPlacer::raycastForNewObjPos(S32 x, S32 y, LLViewerObject** hit_obj,
									   S32* hit_face, bool* b_hit_land,
									   LLVector3* ray_start_region,
									   LLVector3* ray_end_region,
									   LLViewerRegion** region)
{
	F32 max_dist_from_camera = gSavedSettings.getF32("MaxSelectDistance") - 1.f;

	// Viewer-side pick to find the right sim to create the object on. First
	// find the surface the object will be created on.
	LLPickInfo pick = gViewerWindowp->pickImmediate(x, y);

	// Note: use the frontmost non-flora version because (a) plants usually
	// have lots of alpha and (b) pants' Havok
	// representations (if any) are NOT the same as their viewer representation.
	if (pick.mPickType == LLPickInfo::PICK_FLORA)
	{
		*hit_obj = NULL;
		*hit_face = -1;
	}
	else
	{
		*hit_obj = pick.getObject();
		*hit_face = pick.mObjectFace;
	}
	*b_hit_land = !(*hit_obj) && !pick.mPosGlobal.isExactlyZero();
	LLVector3d land_pos_global = pick.mPosGlobal;

	// Make sure there's a surface to place the new object on.
	bool bypass_sim_raycast = false;
	LLVector3d surf_pos_global;
	if (*b_hit_land)
	{
		surf_pos_global = land_pos_global;
		bypass_sim_raycast = true;
	}
	else if (*hit_obj)
	{
		surf_pos_global = (*hit_obj)->getPositionGlobal();
	}
	else
	{
		return false;
	}

	// Make sure the surface isn't too far away.
	LLVector3d ray_start_global = gAgent.getCameraPositionGlobal();
	F32 dist_to_surface_sq = (F32)((surf_pos_global - ray_start_global).lengthSquared());
	if (dist_to_surface_sq > (max_dist_from_camera * max_dist_from_camera))
	{
		return false;
	}

	// Find the sim where the surface lives.
	LLViewerRegion* regionp = gWorld.getRegionFromPosGlobal(surf_pos_global);
	if (!regionp)
	{
		llwarns << "Trying to add object outside of all known regions !"
				<< llendl;
		return false;
	}

	// Find the simulator-side ray that will be used to place the object
	// accurately
	LLVector3d mouse_direction;
	mouse_direction.set(gViewerWindowp->mouseDirectionGlobal(x, y));

	*region = regionp;
	*ray_start_region =	regionp->getPosRegionFromGlobal(ray_start_global);
	// Include an epsilon to avoid rounding issues.
	F32 near_clip = gViewerCamera.getNear() + 0.01f;
	*ray_start_region += gViewerCamera.getAtAxis() * near_clip;

	if (bypass_sim_raycast)
	{
		// Hack to work around Havok's inability to ray cast onto height fields
		// ray end is the viewer's intersection point
		*ray_end_region = regionp->getPosRegionFromGlobal(surf_pos_global);
	}
	else
	{
		// Add an epsilon to the sim version of the ray to avoid rounding
		// problems.
		LLVector3d ray_end_global = ray_start_global +
								    (1.f + max_dist_from_camera) * mouse_direction;
		*ray_end_region = regionp->getPosRegionFromGlobal(ray_end_global);
	}

	return true;
}

S32 LLToolPlacer::getTreeGrassSpecies(std::map<std::string, S32> &table,
									  const char *control, S32 max)
{
	const std::string& species = gSavedSettings.getString(control);
	std::map<std::string, S32>::iterator it;
	it = table.find(species);
	if (it != table.end())
	{
		return it->second;
	}

	// If saved species not found, default to "Random"
	return rand() % max;
}

bool LLToolPlacer::addObject(LLPCode pcode, S32 x, S32 y, U8 use_physics)
{
	LLVector3 ray_start_region;
	LLVector3 ray_end_region;
	LLViewerRegion* regionp = NULL;
	bool b_hit_land = false;
	S32 hit_face = -1;
	LLViewerObject* hit_obj = NULL;
	U8 state = 0;
	bool success = raycastForNewObjPos(x, y, &hit_obj, &hit_face, &b_hit_land,
									   &ray_start_region, &ray_end_region,
									   &regionp);
	if (!success)
	{
		return false;
	}

	if (hit_obj && (hit_obj->isAvatar() || hit_obj->isAttachment()))
	{
		// Cannot create objects on avatars or attachments
		return false;
	}

	if (!regionp)
	{
		llwarns << "Region was NULL, aborting." << llendl;
		return false;
	}

	if (regionp->getRegionFlag(REGION_FLAGS_SANDBOX))
	{
		LLFirstUse::useSandbox();
	}

	// Set params for new object based on its PCode.
	LLQuaternion	rotation;
	LLVector3		scale = DEFAULT_OBJECT_SCALE;
	U8				material = LL_MCODE_WOOD;
	bool			create_selected = false;
	LLVolumeParams	volume_params;

	switch (pcode)
	{
	case LL_PCODE_LEGACY_GRASS:
		//  Randomize size of grass patch
		scale.set(10.f + ll_frand(20.f),
				  10.f + ll_frand(20.f),
				  1.f + ll_frand(2.f));
		state = getTreeGrassSpecies(LLVOGrass::sSpeciesNames,
									"LastGrass",
									LLVOGrass::sMaxGrassSpecies);
		break;

	case LL_PCODE_LEGACY_TREE:
		state = getTreeGrassSpecies(LLVOTree::sSpeciesNames,
									"LastTree",
									LLVOTree::sMaxTreeSpecies);
		break;

	case LL_PCODE_SPHERE:
	case LL_PCODE_CONE:
	case LL_PCODE_CUBE:
	case LL_PCODE_CYLINDER:
	case LL_PCODE_TORUS:
	case LLViewerObject::LL_VO_SQUARE_TORUS:
	case LLViewerObject::LL_VO_TRIANGLE_TORUS:
	default:
		create_selected = true;
		break;
	}

	// Play creation sound
	if (gAudiop)
	{
		gAudiop->triggerSound(LLUUID(gSavedSettings.getString("UISndObjectCreate")),
							  gAgentID, 1.f, LLAudioEngine::AUDIO_TYPE_UI);
	}

	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessageFast(_PREHASH_ObjectAdd);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	LLUUID group_id = gAgent.getGroupID();
	LLParcel* parcel = gViewerParcelMgr.getAgentParcel();
	if (gSavedSettings.getBool("RezWithLandGroup"))
	{
		if (gAgent.isInGroup(parcel->getGroupID()))
		{
			group_id = parcel->getGroupID();
		}
		else if (gAgent.isInGroup(parcel->getOwnerID()))
		{
			group_id = parcel->getOwnerID();
		}
	}
	else if (gAgent.hasPowerInGroup(parcel->getGroupID(),
									GP_LAND_ALLOW_CREATE) &&
			 !parcel->getIsGroupOwned())
	{
		group_id = parcel->getGroupID();
	}
	msg->addUUIDFast(_PREHASH_GroupID, group_id);
	msg->nextBlockFast(_PREHASH_ObjectData);
	msg->addU8Fast(_PREHASH_Material, material);

	U32 flags = 0;		// not selected
	if (use_physics)
	{
		flags |= FLAGS_USE_PHYSICS;
	}
	if (create_selected)
	{
		flags |= FLAGS_CREATE_SELECTED;
	}
	msg->addU32Fast(_PREHASH_AddFlags, flags);

	LLPCode volume_pcode;	// ...PCODE_VOLUME, or the original on error
	switch (pcode)
	{
	case LL_PCODE_SPHERE:
		rotation.setAngleAxis(90.f * DEG_TO_RAD, LLVector3::y_axis);

		volume_params.setType(LL_PCODE_PROFILE_CIRCLE_HALF, LL_PCODE_PATH_CIRCLE);
		volume_params.setBeginAndEndS(0.f, 1.f);
		volume_params.setBeginAndEndT(0.f, 1.f);
		volume_params.setRatio(1.f, 1.f);
		volume_params.setShear(0.f, 0.f);
		LLVolumeMessage::packVolumeParams(&volume_params, msg);
		volume_pcode = LL_PCODE_VOLUME;
		break;

	case LL_PCODE_TORUS:
		rotation.setAngleAxis(90.f * DEG_TO_RAD, LLVector3::y_axis);

		volume_params.setType(LL_PCODE_PROFILE_CIRCLE, LL_PCODE_PATH_CIRCLE);
		volume_params.setBeginAndEndS(0.f, 1.f);
		volume_params.setBeginAndEndT(0.f, 1.f);
		volume_params.setRatio(1.f, 0.25f);	// "top size"
		volume_params.setShear(0.f, 0.f);
		LLVolumeMessage::packVolumeParams(&volume_params, msg);
		volume_pcode = LL_PCODE_VOLUME;
		break;

	case LLViewerObject::LL_VO_SQUARE_TORUS:
		rotation.setAngleAxis(90.f * DEG_TO_RAD, LLVector3::y_axis);

		volume_params.setType(LL_PCODE_PROFILE_SQUARE, LL_PCODE_PATH_CIRCLE);
		volume_params.setBeginAndEndS(0.f, 1.f);
		volume_params.setBeginAndEndT(0.f, 1.f);
		volume_params.setRatio(1.f, 0.25f);	// "top size"
		volume_params.setShear(0.f, 0.f);
		LLVolumeMessage::packVolumeParams(&volume_params, msg);
		volume_pcode = LL_PCODE_VOLUME;
		break;

	case LLViewerObject::LL_VO_TRIANGLE_TORUS:
		rotation.setAngleAxis(90.f * DEG_TO_RAD, LLVector3::y_axis);

		volume_params.setType(LL_PCODE_PROFILE_EQUALTRI, LL_PCODE_PATH_CIRCLE);
		volume_params.setBeginAndEndS(0.f, 1.f);
		volume_params.setBeginAndEndT(0.f, 1.f);
		volume_params.setRatio(1.f, 0.25f);	// "top size"
		volume_params.setShear(0.f, 0.f);
		LLVolumeMessage::packVolumeParams(&volume_params, msg);
		volume_pcode = LL_PCODE_VOLUME;
		break;

	case LL_PCODE_SPHERE_HEMI:
		volume_params.setType(LL_PCODE_PROFILE_CIRCLE_HALF, LL_PCODE_PATH_CIRCLE);
		//volume_params.setBeginAndEndS(0.5f, 1.f);
		volume_params.setBeginAndEndT(0.f, 0.5f);
		volume_params.setRatio(1.f, 1.f);
		volume_params.setShear(0.f, 0.f);
		LLVolumeMessage::packVolumeParams(&volume_params, msg);
		volume_pcode = LL_PCODE_VOLUME;
		break;

	case LL_PCODE_CUBE:
		volume_params.setType(LL_PCODE_PROFILE_SQUARE, LL_PCODE_PATH_LINE);
		volume_params.setBeginAndEndS(0.f, 1.f);
		volume_params.setBeginAndEndT(0.f, 1.f);
		volume_params.setRatio(1.f, 1.f);
		volume_params.setShear(0.f, 0.f);
		LLVolumeMessage::packVolumeParams(&volume_params, msg);
		volume_pcode = LL_PCODE_VOLUME;
		break;

	case LL_PCODE_PRISM:
		volume_params.setType(LL_PCODE_PROFILE_SQUARE, LL_PCODE_PATH_LINE);
		volume_params.setBeginAndEndS(0.f, 1.f);
		volume_params.setBeginAndEndT(0.f, 1.f);
		volume_params.setRatio(0.f, 1.f);
		volume_params.setShear(-0.5f, 0.f);
		LLVolumeMessage::packVolumeParams(&volume_params, msg);
		volume_pcode = LL_PCODE_VOLUME;
		break;

	case LL_PCODE_PYRAMID:
		volume_params.setType(LL_PCODE_PROFILE_SQUARE, LL_PCODE_PATH_LINE);
		volume_params.setBeginAndEndS(0.f, 1.f);
		volume_params.setBeginAndEndT(0.f, 1.f);
		volume_params.setRatio(0.f, 0.f);
		volume_params.setShear(0.f, 0.f);
		LLVolumeMessage::packVolumeParams(&volume_params, msg);
		volume_pcode = LL_PCODE_VOLUME;
		break;

	case LL_PCODE_TETRAHEDRON:
		volume_params.setType(LL_PCODE_PROFILE_EQUALTRI, LL_PCODE_PATH_LINE);
		volume_params.setBeginAndEndS(0.f, 1.f);
		volume_params.setBeginAndEndT(0.f, 1.f);
		volume_params.setRatio(0.f, 0.f);
		volume_params.setShear(0.f, 0.f);
		LLVolumeMessage::packVolumeParams(&volume_params, msg);
		volume_pcode = LL_PCODE_VOLUME;
		break;

	case LL_PCODE_CYLINDER:
		volume_params.setType(LL_PCODE_PROFILE_CIRCLE, LL_PCODE_PATH_LINE);
		volume_params.setBeginAndEndS(0.f, 1.f);
		volume_params.setBeginAndEndT(0.f, 1.f);
		volume_params.setRatio(1.f, 1.f);
		volume_params.setShear(0.f, 0.f);
		LLVolumeMessage::packVolumeParams(&volume_params, msg);
		volume_pcode = LL_PCODE_VOLUME;
		break;

	case LL_PCODE_CYLINDER_HEMI:
		volume_params.setType(LL_PCODE_PROFILE_CIRCLE, LL_PCODE_PATH_LINE);
		volume_params.setBeginAndEndS(0.25f, 0.75f);
		volume_params.setBeginAndEndT(0.f, 1.f);
		volume_params.setRatio(1.f, 1.f);
		volume_params.setShear(0.f, 0.f);
		LLVolumeMessage::packVolumeParams(&volume_params, msg);
		volume_pcode = LL_PCODE_VOLUME;
		break;

	case LL_PCODE_CONE:
		volume_params.setType(LL_PCODE_PROFILE_CIRCLE, LL_PCODE_PATH_LINE);
		volume_params.setBeginAndEndS(0.f, 1.f);
		volume_params.setBeginAndEndT(0.f, 1.f);
		volume_params.setRatio(0.f, 0.f);
		volume_params.setShear(0.f, 0.f);
		LLVolumeMessage::packVolumeParams(&volume_params, msg);
		volume_pcode = LL_PCODE_VOLUME;
		break;

	case LL_PCODE_CONE_HEMI:
		volume_params.setType(LL_PCODE_PROFILE_CIRCLE, LL_PCODE_PATH_LINE);
		volume_params.setBeginAndEndS(0.25f, 0.75f);
		volume_params.setBeginAndEndT(0.f, 1.f);
		volume_params.setRatio(0.f, 0.f);
		volume_params.setShear(0.f, 0.f);
		LLVolumeMessage::packVolumeParams(&volume_params, msg);
		volume_pcode = LL_PCODE_VOLUME;
		break;

	default:
		LLVolumeMessage::packVolumeParams(0, msg);
		volume_pcode = pcode;
		break;
	}
	msg->addU8Fast(_PREHASH_PCode, volume_pcode);

	msg->addVector3Fast(_PREHASH_Scale, scale);
	msg->addQuatFast(_PREHASH_Rotation, rotation);
	msg->addVector3Fast(_PREHASH_RayStart, ray_start_region);
	msg->addVector3Fast(_PREHASH_RayEnd, ray_end_region);
	msg->addU8Fast(_PREHASH_BypassRaycast, (U8)b_hit_land);
	msg->addU8Fast(_PREHASH_RayEndIsIntersection, (U8)false);
	msg->addU8Fast(_PREHASH_State, state);

	// Limit raycast to a single object.
	// Speeds up server raycast + avoid problems with server ray hitting objects
	// that were clipped by the near plane or culled on the viewer.
	LLUUID ray_target_id;
	if (hit_obj)
	{
		ray_target_id = hit_obj->getID();
	}
	else
	{
		ray_target_id.setNull();
	}
	msg->addUUIDFast(_PREHASH_RayTargetID, ray_target_id);

	// Pack in name value pairs
	msg->sendReliable(regionp->getHost());

	// Spawns a message, so must be after above send
	if (create_selected)
	{
		gSelectMgr.deselectAll();
		gWindowp->incBusyCount();
	}

	// VEFFECT: AddObject
	LLHUDEffectSpiral::agentBeamToPosition(regionp->getPosGlobalFromRegion(ray_end_region));

	gViewerStats.incStat(LLViewerStats::ST_CREATE_COUNT);

	return true;
}

// Used by the placer tool to add copies of the current selection.
bool LLToolPlacer::addDuplicate(S32 x, S32 y)
{
	LLVector3 ray_start_region;
	LLVector3 ray_end_region;
	LLViewerRegion* regionp = NULL;
	bool b_hit_land = false;
	S32 hit_face = -1;
	LLViewerObject* hit_obj = NULL;
	bool success = raycastForNewObjPos(x, y, &hit_obj, &hit_face, &b_hit_land,
									   &ray_start_region, &ray_end_region,
									   &regionp);
	if (!success)
	{
		make_ui_sound("UISndInvalidOp");
		return false;
	}
	if (hit_obj && (hit_obj->isAvatar() || hit_obj->isAttachment()))
	{
		// Can't create objects on avatars or attachments
		make_ui_sound("UISndInvalidOp");
		return false;
	}

	// Limit raycast to a single object.
	// Speeds up server raycast + avoid problems with server ray hitting objects
	// that were clipped by the near plane or culled on the viewer.
	LLUUID ray_target_id;
	if (hit_obj)
	{
		ray_target_id = hit_obj->getID();
	}
	else
	{
		ray_target_id.setNull();
	}

	gSelectMgr.selectDuplicateOnRay(ray_start_region, ray_end_region,
									b_hit_land,	// Suppress raycast
									false,		// Intersection
									ray_target_id,
									gSavedSettings.getBool("CreateToolCopyCenters"),
									gSavedSettings.getBool("CreateToolCopyRotates"),
									false);		// Select copy

	if (regionp && regionp->getRegionFlag(REGION_FLAGS_SANDBOX))
	{
		LLFirstUse::useSandbox();
	}

	return true;
}

bool LLToolPlacer::placeObject(S32 x, S32 y, MASK mask)
{
	bool added = true;

//MK
	if (gRLenabled &&
		(gRLInterface.mContainsEdit || gRLInterface.mContainsRez ||
		 gRLInterface.mContainsInteract))
	{
		return true;
	}
//mk

	if (gSavedSettings.getBool("CreateToolCopySelection"))
	{
		added = addDuplicate(x, y);
	}
	else
	{
		added = addObject(sObjectType, x, y, false);
	}

	// ...and go back to the default tool
	if (added && !gSavedSettings.getBool("CreateToolKeepSelected"))
	{
		gToolMgr.getCurrentToolset()->selectTool(&gToolCompTranslate);
	}

	return added;
}

bool LLToolPlacer::handleHover(S32 x, S32 y, MASK mask)
{
	LL_DEBUGS("UserInput") << "hover handled by LLToolPlacer" << LL_ENDL;
	gWindowp->setCursor(UI_CURSOR_TOOLCREATE);
	return true;
}

void LLToolPlacer::handleSelect()
{
	if (gFloaterToolsp)
	{
		gFloaterToolsp->setStatusText("place");
	}
}

void LLToolPlacer::handleDeselect()
{
}

//////////////////////////////////////////////////////
// LLToolPlacerPanel

// static
LLPCode LLToolPlacerPanel::sCube		= LL_PCODE_CUBE;
LLPCode LLToolPlacerPanel::sPrism		= LL_PCODE_PRISM;
LLPCode LLToolPlacerPanel::sPyramid		= LL_PCODE_PYRAMID;
LLPCode LLToolPlacerPanel::sTetrahedron	= LL_PCODE_TETRAHEDRON;
LLPCode LLToolPlacerPanel::sCylinder	= LL_PCODE_CYLINDER;
LLPCode LLToolPlacerPanel::sCylinderHemi= LL_PCODE_CYLINDER_HEMI;
LLPCode LLToolPlacerPanel::sCone		= LL_PCODE_CONE;
LLPCode LLToolPlacerPanel::sConeHemi	= LL_PCODE_CONE_HEMI;
LLPCode LLToolPlacerPanel::sTorus		= LL_PCODE_TORUS;
LLPCode LLToolPlacerPanel::sSquareTorus = LLViewerObject::LL_VO_SQUARE_TORUS;
LLPCode LLToolPlacerPanel::sTriangleTorus =
	LLViewerObject::LL_VO_TRIANGLE_TORUS;
LLPCode LLToolPlacerPanel::sSphere		= LL_PCODE_SPHERE;
LLPCode LLToolPlacerPanel::sSphereHemi	= LL_PCODE_SPHERE_HEMI;
LLPCode LLToolPlacerPanel::sTree		= LL_PCODE_LEGACY_TREE;
LLPCode LLToolPlacerPanel::sGrass		= LL_PCODE_LEGACY_GRASS;

S32 LLToolPlacerPanel::sButtonsAdded = 0;
LLButton* LLToolPlacerPanel::sButtons[TOOL_PLACER_NUM_BUTTONS];

LLToolPlacerPanel::LLToolPlacerPanel(const std::string& name,
									 const LLRect& rect)
:	LLPanel(name, rect)
{
}

void LLToolPlacerPanel::addButton(const std::string& up_state,
								  const std::string& down_state,
								  LLPCode* pcode)
{
	constexpr S32 TOOL_SIZE = 32;
	constexpr S32 HORIZ_SPACING = TOOL_SIZE + 5;
	constexpr S32 VERT_SPACING = TOOL_SIZE + 5;
	constexpr S32 VPAD = 10;
	constexpr S32 HPAD = 7;

	S32 row = sButtonsAdded / 4;
	S32 column = sButtonsAdded % 4;

	LLRect help_rect = gSavedSettings.getRect("ToolHelpRect");

	// Build the rectangle, recalling the origin is at lower left and we want
	// the icons to build down from the top.
	LLRect rect;
	rect.setLeftTopAndSize(HPAD + column * HORIZ_SPACING,
						   help_rect.mBottom - VPAD - row * VERT_SPACING,
						   TOOL_SIZE, TOOL_SIZE);

	LLButton* btn = new LLButton("ToolPlacerOptBtn", rect,
								 up_state, down_state, NULL,
								 &LLToolPlacerPanel::setObjectType, pcode,
								 LLFontGL::getFontSansSerif());
	btn->setFollowsBottom();
	btn->setFollowsLeft();
	addChild(btn);

	sButtons[sButtonsAdded++] = btn;
}

//static
void LLToolPlacerPanel::setObjectType(void* data)
{
	LLPCode pcode = *(LLPCode*) data;
	LLToolPlacer::setObjectType(pcode);
}
