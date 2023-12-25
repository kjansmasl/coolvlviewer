/**
 * @file lltoolpie.cpp
 * @brief LLToolPie class implementation
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

#include "lltoolpie.h"

#include "lleditmenuhandler.h"
#include "llmediaentry.h"
#include "llmenugl.h"
#include "llparcel.h"
#include "lltrans.h"
#include "llwindow.h"				// For gDebugClicks

#include "llagent.h"
#include "llfirstuse.h"
#include "llfloateravatarinfo.h"
#include "llfloaterland.h"
#include "llfloatertools.h"
#include "llhoverview.h"
#include "llhudeffectspiral.h"
#include "llmutelist.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llselectmgr.h"
#include "lltoolfocus.h"
#include "lltoolgrab.h"
#include "lltoolmgr.h"
#include "lltoolselect.h"
#include "hbviewerautomation.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewermedia.h"
#include "llviewermediafocus.h"
#include "llviewermenu.h"
#include "llviewerobjectlist.h"
#include "llviewerparcelmedia.h"
#include "llviewerparcelmgr.h"
#include "llvoavatarself.h"
#include "llworld.h"
#include "llweb.h"

extern void handle_buy(void*);

static void handle_click_action_play();
static void handle_click_action_open_media(LLPointer<LLViewerObject> objectp);
static ECursorType cursor_from_parcel_media(U8 click_action);

LLToolPie gToolPie;

LLToolPie::LLToolPie()
:	LLTool("Pie"),
	mPieMouseButtonDown(false),
	mGrabMouseButtonDown(false),
	mClickAction(0)
{
}

bool LLToolPie::handleMouseDown(S32 x, S32 y, MASK mask)
{
	static LLCachedControl<bool> pick_transparent(gSavedSettings,
												  "AllowPickTransparent");
	gViewerWindowp->pickAsync(x, y, mask, leftMouseCallback,
							  // maybe pick transparent (normally no)
							  pick_transparent,
							  // not rigged, not particles, get surface info
							  false, false, true);
	mGrabMouseButtonDown = true;
	return true;
}

//static
void LLToolPie::leftMouseCallback(const LLPickInfo& pick_info)
{
	gToolPie.mPick = pick_info;
	gToolPie.handleLeftClickPick();
}

bool LLToolPie::handleLeftClickPick()
{
	S32 x = mPick.mMousePt.mX;
	S32 y = mPick.mMousePt.mY;
	MASK mask = mPick.mKeyMask;

	if (handleMediaClick(mPick))
	{
		return true;
	}

	if (mPick.mPickType == LLPickInfo::PICK_PARCEL_WALL)
	{
		LLParcel* parcel = gViewerParcelMgr.getCollisionParcel();
		if (parcel)
		{
			gViewerParcelMgr.selectCollisionParcel();
			if (parcel->getParcelFlag(PF_USE_PASS_LIST) &&
				!gViewerParcelMgr.isCollisionBanned())
			{
				// If selling passes, just buy one
				void* deselect_when_done = (void*)true;
				LLPanelLandGeneral::onClickBuyPass(deselect_when_done);
			}
			else
//MK
			if (!gRLenabled || !gRLInterface.mContainsShowloc)
//mk
			{
				// Not selling passes, get info
				LLFloaterLand::showInstance();
			}
		}

		gFocusMgr.setKeyboardFocus(NULL);
		return LLTool::handleMouseDown(x, y, mask);
	}

	if (mPick.mPickType != LLPickInfo::PICK_LAND)
	{
		gViewerParcelMgr.deselectLand();
	}

	// Did not click in any UI object, so must have clicked in the world
	LLViewerObject* parent = NULL;
	LLViewerObject* object = mPick.getObject();
	if (object)
	{
		parent = object->getRootEdit();
	}

	// If we have a special action, do it.
	if (useClickAction(mask, object, parent))
	{
//MK
		if (gRLenabled && !gRLInterface.canTouch(object, mPick.mIntersection))
		{
			return true;
		}
//mk

		mClickAction = 0;
		if (object && object->getClickAction())
		{
			mClickAction = object->getClickAction();
		}
		else if (parent && parent->getClickAction())
		{
			mClickAction = parent->getClickAction();
		}

		switch (mClickAction)
		{
			case CLICK_ACTION_SIT:
			{
				if (isAgentAvatarValid() && !gAgentAvatarp->mIsSitting &&
					gSavedSettings.getBool("LeftClickToSit"))
				{
					// Agent is not already sitting
					handle_sit_or_stand();
					// Put focus in world when sitting on an object
					gFocusMgr.setKeyboardFocus(NULL);
					return true;
				}
				break;	// else nothing (fall through to touch)
			}

			case CLICK_ACTION_PAY:
			{
				if (((object && object->flagTakesMoney()) ||
					 (parent && parent->flagTakesMoney())) &&
					gSavedSettings.getBool("LeftClickToPay"))
				{
					// Pay event goes to object actually clicked on
					mClickActionObject = object;
					mLeftClickSelection =
						LLToolSelect::handleObjectSelection(mPick, false,
															true);
					if (gSelectMgr.selectGetAllValid())
					{
						// Call this right away, since we have all the info we
						// need to continue the action
						selectionPropertiesReceived();
					}
					return true;
				}
				break;	// Else nothing (fall through to touch)
			}

			case CLICK_ACTION_BUY:
			{
				if (gSavedSettings.getBool("LeftClickToPay"))
				{
					mClickActionObject = parent;
					mLeftClickSelection =
						LLToolSelect::handleObjectSelection(mPick, false,
															true, true);
					if (gSelectMgr.selectGetAllValid())
					{
						// Call this right away, since we have all the info we
						// need to continue the action
						selectionPropertiesReceived();
					}
					return true;
				}
				break;	// Else nothing (fall through to touch)
			}

			case CLICK_ACTION_OPEN:
			{
				if (parent && parent->allowOpen() &&
					gSavedSettings.getBool("LeftClickToOpen"))
				{
					mClickActionObject = parent;
					mLeftClickSelection =
						LLToolSelect::handleObjectSelection(mPick, false,
															true, true);
					if (gSelectMgr.selectGetAllValid())
					{
						// Call this right away, since we have all the info we
						// need to continue the action
						selectionPropertiesReceived();
					}
					return true;
				}
				break;	// Else nothing (fall through to touch)
			}

			case CLICK_ACTION_PLAY:
			{
				if (gSavedSettings.getBool("LeftClickToPlay"))
				{
					handle_click_action_play();
					return true;
				}
			}

			case CLICK_ACTION_OPEN_MEDIA:
			{
				if (gSavedSettings.getBool("LeftClickToPlay"))
				{
					// mClickActionObject = object;
					handle_click_action_open_media(object);
					return true;
				}
				break;	// Else nothing (fall through to touch)
			}

			case CLICK_ACTION_ZOOM:
			{
				if (gSavedSettings.getBool("LeftClickToZoom"))
				{
					constexpr F32 PADDING_FACTOR = 2.f;
					LLViewerObject* object =
						gObjectList.findObject(mPick.mObjectID);
					if (object)
					{
						gAgent.setFocusOnAvatar(false);
						LLBBox bbox = object->getBoundingBoxAgent();
						F32 aspect = gViewerCamera.getAspect();
						F32 view = gViewerCamera.getView();
						F32 angle_of_view = llmax(0.1f,
												  aspect > 1.f ? view * aspect
															   : view);
						F32 distance = bbox.getExtentLocal().length() *
									   PADDING_FACTOR / atanf(angle_of_view);
						LLVector3 obj_to_cam = gViewerCamera.getOrigin() -
											   bbox.getCenterAgent();
						obj_to_cam.normalize();
						LLVector3d center_global =
							gAgent.getPosGlobalFromAgent(bbox.getCenterAgent());
						gAgent.setCameraPosAndFocusGlobal(center_global +
														  LLVector3d(obj_to_cam *
																	 distance),
														  center_global,
														  mPick.mObjectID);
					}
					return true;
				}
				break;	// Else nothing (fall through to touch)
			}

			case CLICK_ACTION_DISABLED:
				return true;

			case CLICK_ACTION_TOUCH:
			default:
				break;	// fall through to touch
		}
	}

	// Put focus back "in world"
	gFocusMgr.setKeyboardFocus(NULL);

	// Switch to grab tool if physical or triggerable
	bool touchable = (object && object->flagHandleTouch()) ||
					 (parent && parent->flagHandleTouch());
	if (object && !object->isAvatar() &&
		(touchable || object->flagUsePhysics() ||
		 (parent && !parent->isAvatar() && parent->flagUsePhysics())))
	{
		gGrabTransientTool = this;
		gToolMgr.getCurrentToolset()->selectTool(&gToolGrab);
		return gToolGrab.handleObjectHit(mPick);
	}

	if (!object)
	{
		LLHUDIcon* icon = mPick.mHUDIcon;
		LLViewerObject* src_obj = icon ? icon->getSourceObject() : NULL;
		if (src_obj)
		{
			const LLUUID& object_id = src_obj->getID();
			icon->fireClickedCallback(object_id);
		}
	}

	if (gSavedSettings.getBool("LeftClickSteersAvatar"))
	{
		// Mouse already released
		if (!mGrabMouseButtonDown)
		{
			return true;
		}

		while (object && object->isAttachment() && !object->flagHandleTouch())
		{
			// Do not pick avatar through hud attachment
			if (object->isHUDAttachment())
			{
				break;
			}
			object = (LLViewerObject*)object->getParent();
		}
		if (object && object == gAgentAvatarp)
		{
			// We left clicked on avatar, switch to focus mode
			gToolMgr.setTransientTool(&gToolFocus);
			gViewerWindowp->hideCursor();
			gToolFocus.setMouseCapture(true);
			gToolFocus.pickCallback(mPick);
			gAgent.setFocusOnAvatar();
			return true;
		}
	}

	// Could be first left-click on nothing
	LLFirstUse::useLeftClickNoHit();

	return LLTool::handleMouseDown(x, y, mask);
}

bool LLToolPie::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
//MK
	// HACK : if alt-right-clicking and not in mouselook, HUDs are passed
	// through and we risk right-clicking in-world => discard this click
	if (gRLenabled && (mask & MASK_ALT) &&
		gAgent.getCameraMode() != CAMERA_MODE_MOUSELOOK)
	{
		handleMouseDown(x, y, mask);
		return true;
	}
//mk

	static LLCachedControl<bool> pick_rigged_meshes(gSavedSettings,
													"AllowPickRiggedMeshes");
	static LLCachedControl<bool> pick_particles(gSavedSettings,
												"AllowPickParticles");

	mPieMouseButtonDown = true;
	// Note: we do not pick transparent so users cannot "pay" transparent
	// objects.
	gViewerWindowp->pickAsync(x, y, mask, rightMouseCallback,
							  // do not (always) pick transparent
							  false,
							  // maybe pick rigged meshes or particles
							  pick_rigged_meshes, pick_particles,
							  // get surface info
							  true);

	// Do not steal focus from UI
	return false;
}

bool LLToolPie::handleScrollWheel(S32 x, S32 y, S32 clicks)
{
	return LLViewerMediaFocus::getInstance()->handleScrollWheel(x, y, clicks);
}

//static
void LLToolPie::rightMouseCallback(const LLPickInfo& pick_info)
{
	gToolPie.mPick = pick_info;
	gToolPie.handleRightClickPick();
}

bool LLToolPie::handleRightClickPick()
{
	S32 x = mPick.mMousePt.mX;
	S32 y = mPick.mMousePt.mY;
	MASK mask = mPick.mKeyMask;

	LLViewerMediaFocus::getInstance()->clearFocus();

	if (mPick.mPickType != LLPickInfo::PICK_LAND)
	{
		gViewerParcelMgr.deselectLand();
	}

	// Put focus back "in world"
	gFocusMgr.setKeyboardFocus(NULL);

	// Cannot ignore children here.
	LLToolSelect::handleObjectSelection(mPick, false, true);

	if (!gMenuHolderp)
	{
		// Either at early initialization or late quitting stage
		return true;
	}

	// Did not click in any UI object, so must have clicked in-world
	LLViewerObject* object = mPick.getObject();
	if (object && object->isAttachment() && !object->isHUDAttachment() &&
		!object->permYouOwner())
	{
		// Find the avatar corresponding to any attachment object we do not own
		while (object->isAttachment())
		{
			object = (LLViewerObject*)object->getParent();
			if (!object) return false;	// Orphaned object ?
		}
	}

	if (mask == MASK_SHIFT && gLuaPiep && gLuaPiep->onPieMenu(mPick, object))
	{
		gLuaPiep->show(x, y, mPieMouseButtonDown);
		LLTool::handleRightMouseDown(x, y, mask);
		return true;
	}

	// Spawn the pie menu
	if ((!object || !object->isHUDAttachment()) && // HUDs got priority !
		gPieParticlep && mPick.mPickParticle &&
		mPick.mParticleOwnerID.notNull())
	{
		gPieParticlep->show(x, y, mPieMouseButtonDown);
		return true;
	}
	else if (mPick.mPickType == LLPickInfo::PICK_LAND)
	{
		LLParcelSelectionHandle selection =
			gViewerParcelMgr.selectParcelAt(mPick.mPosGlobal);
		gMenuHolderp->setParcelSelection(selection);
		gPieLandp->show(x, y, mPieMouseButtonDown);

		// VEFFECT: ShowPie
		LLHUDEffectSpiral::sphereAtPosition(mPick.mPosGlobal);
	}
	else if (mPick.mObjectID == gAgentID)
	{
		LLMenuItemGL* item =
			gPieSelfp->getChild<LLMenuItemGL>("Self Sit", true, false);
		if (item)
		{
			if (isAgentAvatarValid() && gAgentAvatarp->mIsSitting)
			{
				item->setValue(LLTrans::getString("stand_up"));
			}
			else
			{
				item->setValue(LLTrans::getString("sit_here"));
			}
		}

		gPieSelfp->show(x, y, mPieMouseButtonDown);
	}
	else if (object)
	{
		if (gRLenabled && !object->isAvatar() && LLFloaterTools::isVisible() &&
			!gRLInterface.canEdit(object))
		{
			gFloaterToolsp->close();
		}

		gMenuHolderp->setObjectSelection(gSelectMgr.getSelection());

		if (object->isAvatar())
		{
			// Object is an avatar, so check for mute by id.
			LLVOAvatar* avatar = (LLVOAvatar*)object;
			LLUUID id = avatar->getID();
			std::string name = avatar->getFullname();

			if (gMutesPieMenup)
			{
				bool fully_muted = LLMuteList::isMuted(id, name);
				LLMenuItemGL* item =
					gMutesPieMenup->getChild<LLMenuItemGL>("Avatar Mute",
														  true, false);
				if (item)
				{
					if (fully_muted)
					{
						item->setValue(LLTrans::getString("unmute_all"));
					}
					else
					{
						item->setValue(LLTrans::getString("mute_all"));
					}
				}

				item = gMutesPieMenup->getChild<LLMenuItemGL>("Avatar Mute chat",
															 true, false);
				if (item)
				{
					if (LLMuteList::isMuted(id, name, LLMute::flagTextChat))
					{
						item->setValue(LLTrans::getString("unmute_chat"));
					}
					else
					{
						item->setValue(LLTrans::getString("mute_chat"));
					}
				}

				item = gMutesPieMenup->getChild<LLMenuItemGL>("Avatar Mute voice",
															 true, false);
				if (item)
				{
					if (LLMuteList::isMuted(id, name, LLMute::flagVoiceChat))
					{
						item->setValue(LLTrans::getString("unmute_voice"));
					}
					else
					{
						item->setValue(LLTrans::getString("mute_voice"));
					}
				}

				item = gMutesPieMenup->getChild<LLMenuItemGL>("Avatar Mute sounds",
															 true, false);
				if (item)
				{
					if (LLMuteList::isMuted(id, name, LLMute::flagObjectSounds))
					{
						item->setValue(LLTrans::getString("unmute_sounds"));
					}
					else
					{
						item->setValue(LLTrans::getString("mute_sounds"));
					}
				}

				item = gMutesPieMenup->getChild<LLMenuItemGL>("Avatar Mute particles",
															 true, false);
				if (item)
				{
					if (LLMuteList::isMuted(id, name, LLMute::flagParticles))
					{
						item->setValue(LLTrans::getString("unmute_particles"));
					}
					else
					{
						item->setValue(LLTrans::getString("mute_particles"));
					}
				}

				LLVOAvatar::VisualMuteSettings val =
					avatar->getVisualMuteSettings();
				bool settings_available = LLVOAvatar::sUseImpostors;
//MK
				settings_available = settings_available &&
									 (!gRLenabled || !avatar->isRLVMuted());
//mk

				item = gMutesPieMenup->getChild<LLMenuItemGL>("Avatar Always Render",
															 true, false);
				if (item)
				{
					item->setEnabled(!fully_muted && settings_available &&
									 val != LLVOAvatar::AV_ALWAYS_RENDER);
				}

				item = gMutesPieMenup->getChild<LLMenuItemGL>("Avatar Normal Render",
															 true, false);
				if (item)
				{
					item->setEnabled(!fully_muted && settings_available &&
									 val != LLVOAvatar::AV_RENDER_NORMALLY);
				}

				item = gMutesPieMenup->getChild<LLMenuItemGL>("Avatar Never Render",
															 true, false);
				if (item)
				{
					item->setEnabled(!fully_muted && settings_available &&
									 val != LLVOAvatar::AV_DO_NOT_RENDER);
				}
			}

			gPieAvatarp->show(x, y, mPieMouseButtonDown);
		}
		else if (object->isAttachment())
		{
			LLMenuItemGL* item =
				gPieAttachmentp->getChild<LLMenuItemGL>("Self Sit Attachment",
														true, false);
			if (item)
			{
				if (isAgentAvatarValid() && gAgentAvatarp->mIsSitting)
				{
					item->setValue(LLTrans::getString("stand_up"));
				}
				else
				{
					item->setValue(LLTrans::getString("sit_here"));
				}
			}

			gPieAttachmentp->show(x, y, mPieMouseButtonDown);
		}
		else
		{
#if 0		// Sadly, the object name is unknown/empty when the pie menu is
			// built...
			std::string name;
			LLSelectNode* node = gSelectMgr.getSelection()->getFirstRootNode();
			if (node)
			{
				name = node->mName;
			}
#endif
			if (gPieObjectMutep)
			{
				LLMenuItemGL* item =
					gPieObjectMutep->getChild<LLMenuItemGL>("Mute object",
															true, false);
				if (item)
				{
					if (LLMuteList::isMuted(object->getID()))
					{
						item->setValue(LLTrans::getString("unmute"));
					}
					else
					{
						item->setValue(LLTrans::getString("mute"));
					}
				}
#if 0			// Sadly, the object name is unknown/empty when the pie menu is
				// built... So, we cannot determine wether this object is
				// already muted by name of not.
				item = gPieObjectMutep->getChild<LLMenuItemGL>("Mute by name",
															   true, false);
				if (item)
				{
					if (LLMuteList::isMuted(LLUUID::null, name))
					{
						item->setValue(LLTrans::getString("unmute_by_name"));
					}
					else
					{
						item->setValue(LLTrans::getString("mute_by_name"));
					}
				}
#endif
			}
#if 0		// Avatar puppets "jelly-dollifying" does not work anyway...
			LLVOAvatarPuppet* puppet = object->getPuppetAvatar();
			if (puppet && gPieObjectMutep)
			{
				LLVOAvatar::VisualMuteSettings val =
					puppet->getVisualMuteSettings();
				bool settings_available = LLVOAvatar::sUseImpostors;
//MK
				settings_available = settings_available &&
									 (!gRLenabled || !puppet->isRLVMuted());
//mk
				LLMenuItemGL* item =
					gPieObjectMutep->getChild<LLMenuItemGL>("Puppet Always Render",
															true, false);
				if (item)
				{
					item->setEnabled(settings_available &&
									 val != LLVOAvatar::AV_ALWAYS_RENDER);
				}

				item = gPieObjectMutep->getChild<LLMenuItemGL>("Puppet Normal Render",
															   true, false);
				if (item)
				{
					item->setEnabled(settings_available &&
									 val != LLVOAvatar::AV_RENDER_NORMALLY);
				}

				item = gPieObjectMutep->getChild<LLMenuItemGL>("Puppet Never Render",
															   true, false);
				if (item)
				{
					item->setEnabled(settings_available &&
									 val != LLVOAvatar::AV_DO_NOT_RENDER);
				}
			}
#endif
			gPieObjectp->show(x, y, mPieMouseButtonDown);

			// VEFFECT: ShowPie object. Do not show when you click on someone
			// else: it could freak them out.
			LLHUDEffectSpiral::sphereAtPosition(mPick.mPosGlobal);
		}
	}

	// Ignore return value
	LLTool::handleRightMouseDown(x, y, mask);

	// We handled the event.
	return true;
}

bool LLToolPie::useClickAction(MASK mask, LLViewerObject* object,
							   LLViewerObject* parent)
{
	if (mask != MASK_NONE || !object || object->isAttachment() ||
		!LLPrimitive::isPrimitive(object->getPCode()))
	{
		return false;
	}

	U8 object_action = object->getClickAction();
	U8 parent_action = parent ? parent->getClickAction() : 0;
	return (object_action && object_action != CLICK_ACTION_DISABLED) ||
		   (parent_action && parent_action != CLICK_ACTION_DISABLED);
}

U8 final_click_action(LLViewerObject* obj)
{
	if (!obj || obj->isAttachment())
	{
		return CLICK_ACTION_NONE;
	}

	U8 object_action = obj->getClickAction();
	if (object_action)
	{
		return object_action;
	}
	// Note: at this point object_action = 0 = CLICK_ACTION_TOUCH

	LLViewerObject* parent = obj->getRootEdit();
	if (!parent)
	{
		return CLICK_ACTION_TOUCH;
	}

	U8 parent_action = parent->getClickAction();
	// CLICK_ACTION_DISABLED ("None" in UI) is intended for child action to
	// override parents action when assigned to parent or to child.
	if (/*parent_action && */parent_action != CLICK_ACTION_DISABLED)
	{
		// Note: no need to test for parent_action != 0 because
		// CLICK_ACTION_TOUCH = 0, which would be returned below anyway.
		return parent_action;
	}

	return CLICK_ACTION_TOUCH;
}

ECursorType cursor_from_object(LLViewerObject* object)
{
	LLViewerObject* parent = NULL;
	if (object)
	{
		parent = object->getRootEdit();
	}
	U8 click_action = final_click_action(object);
	ECursorType cursor = UI_CURSOR_ARROW;
	switch (click_action)
	{
		case CLICK_ACTION_SIT:
			// Not already sitting ?
			if (isAgentAvatarValid() && !gAgentAvatarp->mIsSitting)
			{
				cursor = UI_CURSOR_TOOLSIT;
			}
			break;

		case CLICK_ACTION_BUY:
			cursor = UI_CURSOR_TOOLBUY;
			break;

		case CLICK_ACTION_OPEN:
			// Open always opens the parent.
			if (parent && parent->allowOpen())
			{
				cursor = UI_CURSOR_TOOLOPEN;
			}
			break;

		case CLICK_ACTION_PAY:
			if ((object && object->flagTakesMoney()) ||
				(parent && parent->flagTakesMoney()))
			{
				cursor = UI_CURSOR_TOOLPAY;
			}
			break;

		case CLICK_ACTION_ZOOM:
			cursor = UI_CURSOR_TOOLZOOMIN;
			break;

		case CLICK_ACTION_PLAY:
		case CLICK_ACTION_OPEN_MEDIA:
			cursor = cursor_from_parcel_media(click_action);
			break;

		default:
			break;
	}

	return cursor;
}

void LLToolPie::resetSelection()
{
	mLeftClickSelection = NULL;
	mClickActionObject = NULL;
	mClickAction = 0;
}

//static
void LLToolPie::selectionPropertiesReceived()
{
	// Make sure all data has been received since this function will be called
	// repeatedly as the data comes in.
	if (!gSelectMgr.selectGetAllValid())
	{
		return;
	}

	LLObjectSelection* selection = gToolPie.getLeftClickSelection();
	if (selection)
	{
		LLViewerObject* selected_object = selection->getPrimaryObject();
		// since we don't currently have a way to lock a selection, it could
		// have changed after we initially clicked on the object
		if (selected_object == gToolPie.getClickActionObject())
		{
			U8 click_action = gToolPie.getClickAction();
			switch (click_action)
			{
			case CLICK_ACTION_BUY:
				// When we get object properties after left-clicking on an
				// object with left-click = buy, if it's the same object, do
				// the buy.
				handle_buy(NULL);
				break;

			case CLICK_ACTION_PAY:
				handle_give_money_dialog();
				break;

			case CLICK_ACTION_OPEN:
//MK
				if (gRLenabled &&
					!gRLInterface.canEdit(gSelectMgr.getSelection()->getPrimaryObject()))
				{
					return;
				}

				if (gRLenabled &&
					!gRLInterface.canTouchFar(selected_object,
											  gToolPie.getPick().mIntersection))
				{
					return;
				}
//mk
				handle_object_open();
				break;

			default:
				break;
			}
		}
	}
	gToolPie.resetSelection();
}

bool LLToolPie::handleHover(S32 x, S32 y, MASK mask)
{
	LLPickInfo hover_pick = gViewerWindowp->getHoverPick();
	LLViewerObject* object = hover_pick.getObject();
	LLViewerObject* parent = object ? object->getRootEdit() : NULL;

	if (handleMediaHover(hover_pick))
	{
		// Cursor set by media object
		// *TODO: implement glow-like highlighting ?
	}
	else if (object)
	{
		if (useClickAction(mask, object, parent))
		{
			ECursorType cursor = cursor_from_object(object);
			gWindowp->setCursor(cursor);
		}
		else if ((!object->isAvatar() && object->flagUsePhysics()) ||
				 (parent && !parent->isAvatar() && parent->flagUsePhysics()))
		{
			gWindowp->setCursor(UI_CURSOR_TOOLGRAB);
		}
		else if ((object->getClickAction() != CLICK_ACTION_DISABLED ||
				  !object->isAttachment()) &&
				 (object->flagHandleTouch() ||
				  (parent && parent->flagHandleTouch())))
		{
			gWindowp->setCursor(UI_CURSOR_HAND);
		}

		else
		{
			gWindowp->setCursor(UI_CURSOR_ARROW);
		}
	}
	else
	{
		gWindowp->setCursor(UI_CURSOR_ARROW);
		LLViewerMediaFocus::getInstance()->clearHover();
	}

	return true;
}

bool LLToolPie::handleMouseUp(S32 x, S32 y, MASK mask)
{
	LLViewerObject* obj = mPick.getObject();
	U8 click_action = final_click_action(obj);
	if (click_action == CLICK_ACTION_BUY || click_action == CLICK_ACTION_PAY ||
		click_action == CLICK_ACTION_OPEN)
	{
		// Because these actions open UI dialogs, we won't change the cursor
		// again until the next hover and GL pick over the world. Keep the
		// cursor an arrow, assuming that after the user moves off the UI, they
		// won't be on the same object anymore.
		gWindowp->setCursor(UI_CURSOR_ARROW);
		// Make sure the hover-picked object is ignored.
		gHoverViewp->resetLastHoverObject();
	}

	mGrabMouseButtonDown = false;
	gToolMgr.clearTransientTool();

	// Maybe look at object/person clicked on
	gAgent.setLookAt(LOOKAT_TARGET_CONVERSATION, obj);

	return LLTool::handleMouseUp(x, y, mask);
}

bool LLToolPie::handleRightMouseUp(S32 x, S32 y, MASK mask)
{
	mPieMouseButtonDown = false;
	gToolMgr.clearTransientTool();
	return LLTool::handleRightMouseUp(x, y, mask);
}

bool LLToolPie::handleDoubleClick(S32 x, S32 y, MASK mask)
{
	if (gDebugClicks)
	{
		llinfos << "LLToolPie handleDoubleClick (becoming mouseDown)"
				<< llendl;
	}

	if (handleMediaDblClick(mPick))
	{
		return true;
	}

	if (mPick.mPosGlobal.isExactlyZero())
	{
		return false;
	}

	LLViewerObject* objp = mPick.getObject();
	LLViewerObject* parentp = objp ? objp->getRootEdit() : NULL;
	bool is_in_world = mPick.mObjectID.notNull() && objp &&
					   !objp->isHUDAttachment();
	bool is_land = mPick.mPickType == LLPickInfo::PICK_LAND;
	bool has_touch_handler = false;
	bool has_click_action = false;
	if (!is_land && is_in_world &&	// Note: if is_in_world then objp != NULL
		!gSavedSettings.getBool("DoubleClickScriptedObject"))
	{
		has_touch_handler = objp->flagHandleTouch() ||
							(parentp && parentp->flagHandleTouch());
		has_click_action = final_click_action(objp);
		if (!has_touch_handler || !has_click_action)
		{
			// Is media playing on this face ?
			const LLTextureEntry* tep = objp->getTE(mPick.mObjectFace);
			viewer_media_t media_impl;
			media_impl = LLViewerMedia::getMediaImplFromTextureEntry(tep);
			if (media_impl.notNull() && media_impl->hasMedia())
			{
				has_touch_handler = has_click_action = true;
			}
		}
	}

	if (is_land || (is_in_world && !has_touch_handler && !has_click_action))
	{
		U32 action = gSavedSettings.getU32("DoubleClickAction");
		if (action == 1)
		{
			handle_go_to();
			return true;
		}
		else if (action == 2 && isAgentAvatarValid()
//MK
				 && !(gRLenabled && gRLInterface.contains ("tploc")))
//mk
		{
			LLVector3d pos = mPick.mPosGlobal;
			pos.mdV[VZ] += gAgentAvatarp->getPelvisToFoot();
			gAgent.teleportViaLocationLookAt(pos);
			return true;
		}
	}

	return false;
}

void LLToolPie::handleDeselect()
{
	if (hasMouseCapture())
	{
		setMouseCapture(false);  // Calls onMouseCaptureLost() indirectly
	}
	// Remove temporary selection for pie menu
	gSelectMgr.validateSelection();
}

LLTool* LLToolPie::getOverrideTool(MASK mask)
{
	if (mask == MASK_CONTROL || mask == (MASK_CONTROL | MASK_SHIFT))
	{
		return &gToolGrab;
	}
	return LLTool::getOverrideTool(mask);
}

void LLToolPie::stopEditing()
{
	if (hasMouseCapture())
	{
		setMouseCapture(false);  // Calls onMouseCaptureLost() indirectly
	}
}

static void handle_click_action_play()
{
	LLViewerMediaImpl::EMediaStatus status = LLViewerParcelMedia::getStatus();
	switch (status)
	{
		case LLViewerMediaImpl::MEDIA_PLAYING:
			LLViewerParcelMedia::pause();
			break;

		case LLViewerMediaImpl::MEDIA_PAUSED:
			LLViewerParcelMedia::start();
			break;

		default:
			LLViewerParcelMedia::play();
	}
}

bool LLToolPie::handleMediaClick(const LLPickInfo& pick)
{
	// *FIXME: how do we handle object in different parcel than us ?
	LLParcel* parcel = gViewerParcelMgr.getAgentParcel();
	LLPointer<LLViewerObject> objectp = pick.getObject();

	LLViewerMediaFocus* mfocus = LLViewerMediaFocus::getInstance();

	if (!parcel || objectp.isNull() || pick.mObjectFace < 0 ||
		pick.mObjectFace >= objectp->getNumTEs())
	{
		mfocus->clearFocus();
		return false;
	}

	// Does this face have media ?
	const LLTextureEntry* tep = objectp->getTE(pick.mObjectFace);
	viewer_media_t media_impl;
	media_impl = LLViewerMedia::getMediaImplFromTextureEntry(tep);
	if (media_impl.isNull() || !media_impl->hasMedia())
	{
		mfocus->clearFocus();
		return false;
	}

	if (!mfocus->isFocusedOnFace(pick.getObject(), pick.mObjectFace))
	{
		LL_DEBUGS("Media") << (media_impl.isNull() ? "Media impl is NULL"
												   : "New focus detected")
						   << ", focusing on media face." << LL_ENDL;
		mfocus->setFocusFace(true, pick.getObject(), pick.mObjectFace,
							 media_impl, pick.mNormal);
	}
	else if (gKeyboardp)
	{
		// Make sure keyboard focus is set to the media focus object.
		gFocusMgr.setKeyboardFocus(mfocus);
		gEditMenuHandlerp = mfocus->getFocusedMediaImpl();

		media_impl->mouseDown(pick.mUVCoords, gKeyboardp->currentMask(true));
		// The mouse-up will happen when capture is lost
		media_impl->mouseCapture();
		LL_DEBUGS("Media") << "Mouse down event passed to media" << LL_ENDL;
	}

	return true;
}

bool LLToolPie::handleMediaDblClick(const LLPickInfo& pick)
{
	// *FIXME: how do we handle object in different parcel than us ?
	LLParcel* parcel = gViewerParcelMgr.getAgentParcel();
	if (!parcel) return false;

	LLViewerMediaFocus* mfocus = LLViewerMediaFocus::getInstance();

	LLPointer<LLViewerObject> objectp = mPick.getObject();
	if (objectp.isNull() || pick.mObjectFace < 0 ||
		pick.mObjectFace >= objectp->getNumTEs())
	{
		mfocus->clearFocus();
		return false;
	}

	const LLTextureEntry* tep = objectp->getTE(pick.mObjectFace);
	viewer_media_t media_impl;
	media_impl = LLViewerMedia::getMediaImplFromTextureEntry(tep);
	if (media_impl.isNull() || !media_impl->hasMedia())
	{
		mfocus->clearFocus();
		return false;
	}

	if (!mfocus->isFocusedOnFace(pick.getObject(), pick.mObjectFace))
	{
		mfocus->setFocusFace(true, pick.getObject(), pick.mObjectFace,
							 media_impl, pick.mNormal);
	}
	else if (gKeyboardp)
	{
		// Make sure keyboard focus is set to the media focus object.
		gFocusMgr.setKeyboardFocus(mfocus);
		gEditMenuHandlerp = mfocus->getFocusedMediaImpl();

		media_impl->mouseDoubleClick(pick.mUVCoords,
									 gKeyboardp->currentMask(true));
		// The mouse-up will happen when capture is lost
		media_impl->mouseCapture();
		LL_DEBUGS("Media") << "Mouse double-click event passed to media"
						   << LL_ENDL;
	}

	return true;
}

bool LLToolPie::handleMediaHover(const LLPickInfo& pick)
{
	// *FIXME: how do we handle object in different parcel than us ?
	LLParcel* parcel = gViewerParcelMgr.getAgentParcel();
	if (!parcel) return false;

	LLViewerMediaFocus* mfocus = LLViewerMediaFocus::getInstance();

	LLPointer<LLViewerObject> objectp = pick.getObject();

	// Early out cases. Must clear mouse over media focus flag did not hit an
	// object or did not hit a valid face
	if (objectp.isNull() || pick.mObjectFace < 0 ||
		pick.mObjectFace >= objectp->getNumTEs())
	{
		mfocus->clearHover();
		return false;
	}

	const LLTextureEntry* tep = objectp->getTE(pick.mObjectFace);
	viewer_media_t media_impl;
	media_impl = LLViewerMedia::getMediaImplFromTextureEntry(tep);
	if (media_impl.notNull() && gKeyboardp)
	{
		// Update media hover object
		if (!mfocus->isHoveringOverFace(objectp, pick.mObjectFace))
		{
			mfocus->setHoverFace(objectp, pick.mObjectFace, media_impl,
								 pick.mNormal);
			gSelectMgr.setHoverObject(objectp, pick.mObjectFace);
			mfocus->setPickInfo(pick);
		}

		// If this is the focused media face, send mouse move events.
		if (mfocus->isFocusedOnFace(objectp, pick.mObjectFace))
		{
			media_impl->mouseMove(pick.mUVCoords,
								  gKeyboardp->currentMask(true));
			gViewerWindowp->setCursor(media_impl->getLastSetCursor());
		}
		else
		{
			// This is not the focused face -- set the default cursor.
			gViewerWindowp->setCursor(UI_CURSOR_ARROW);
		}

		return true;
	}

	// In all other cases, clear media hover.
	mfocus->clearHover();

	return false;
}

static void handle_click_action_open_media(LLPointer<LLViewerObject> objectp)
{
	// *FIXME: how do we handle object in different parcel than us ?
	LLParcel* parcel = gViewerParcelMgr.getAgentParcel();
	if (!parcel) return;

	// Did we hit an object ?
	if (objectp.isNull()) return;

	// Did we hit a valid face on the object ?
	S32 face = gToolPie.getPick().mObjectFace;
	if (face < 0 || face >= objectp->getNumTEs()) return;

	// Is media playing on this face ?
	LLTextureEntry* tep = objectp->getTE(face);
	if (tep && LLViewerMedia::getMediaImplFromTextureID(tep->getID()))
	{
		handle_click_action_play();
		return;
	}

	std::string media_url = parcel->getMediaURL();
	std::string media_type = parcel->getMediaType();
	LLStringUtil::trim(media_url);

	// Get the scheme, see if that is handled as well.
	LLURI uri(media_url);
	std::string media_scheme = uri.scheme() != "" ? uri.scheme() : "http";

	LLWeb::loadURL(media_url);
}

static ECursorType cursor_from_parcel_media(U8 click_action)
{
	// *FIXME: how do we handle object in different parcel than us ?
	LLParcel* parcel = gViewerParcelMgr.getAgentParcel();
	if (!parcel)
	{
		return UI_CURSOR_ARROW;
	}

	LLViewerMediaImpl::EMediaStatus status = LLViewerParcelMedia::getStatus();
	if (status == LLViewerMediaImpl::MEDIA_PLAYING)
	{
		return click_action == CLICK_ACTION_PLAY ? UI_CURSOR_TOOLPAUSE
												 : UI_CURSOR_TOOLMEDIAOPEN;
	}
	else
	{
		return UI_CURSOR_TOOLPLAY;
	}
}
