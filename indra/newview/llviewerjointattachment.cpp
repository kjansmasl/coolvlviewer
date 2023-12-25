/**
 * @file llviewerjointattachment.cpp
 * @brief Implementation of LLViewerJointAttachment class
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

#include "llviewerprecompiledheaders.h"

#include "llviewerjointattachment.h"

#include "llgl.h"
#include "llrender.h"
#include "llvolume.h"

#include "llagent.h"				// For MAX_ATTACHMENT_DIST + RLV
#include "lldrawable.h"
#include "llface.h"
#include "llhudtext.h"
#include "llinventorymodel.h"
#include "llpipeline.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llspatialpartition.h"
#include "llviewercontrol.h"
#include "llviewerobjectlist.h"
#include "llvoavatarself.h"

LLViewerJointAttachment::LLViewerJointAttachment()
:	mGroup(0),
	mVisibleInFirst(false),
	mIsHUDAttachment(false),
	mPieSlice(-1)
{
	mValid = false;
	mUpdateXform = false;
	mAttachedObjects.clear();
}

U32 LLViewerJointAttachment::drawShape(F32 pixelArea, bool first_pass,
									   bool is_dummy)
{
	if (LLVOAvatar::sShowAttachmentPoints)
	{
		LLGLDisable cull_face(GL_CULL_FACE);

		gGL.color4f(1.f, 1.f, 1.f, 1.f);
		gGL.begin(LLRender::TRIANGLES);
		{
			gGL.vertex3f(-0.1f, 0.1f, 0.f);
			gGL.vertex3f(-0.1f, -0.1f, 0.f);
			gGL.vertex3f(0.1f, -0.1f, 0.f);
			gGL.vertex3f(-0.1f, 0.1f, 0.f);
			gGL.vertex3f(0.1f, -0.1f, 0.f);
			gGL.vertex3f(0.1f, 0.1f, 0.f);
		}
		gGL.end();
	}
	return 0;
}

void LLViewerJointAttachment::setupDrawable(LLViewerObject* object)
{
	if (!object->mDrawable)
	{
		return;
	}
	if (object->mDrawable->isActive())
	{
		object->mDrawable->makeStatic(false);
	}

	object->mDrawable->mXform.setParent(getXform());
	object->mDrawable->makeActive();
	LLVector3 current_pos = object->getRenderPosition();
	LLQuaternion current_rot = object->getRenderRotation();
	LLQuaternion attachment_pt_inv_rot = ~(getWorldRotation());

	current_pos -= getWorldPosition();
	current_pos.rotVec(attachment_pt_inv_rot);

	current_rot = current_rot * attachment_pt_inv_rot;

	object->mDrawable->mXform.setPosition(current_pos);
	object->mDrawable->mXform.setRotation(current_rot);
	gPipeline.markMoved(object->mDrawable);
	// face may need to change draw pool to/from POOL_HUD
	gPipeline.markTextured(object->mDrawable);

	if (mIsHUDAttachment)
	{
		for (S32 i = 0, count = object->mDrawable->getNumFaces();
			 i < count; ++i)
		{
			LLFace* facep = object->mDrawable->getFace(i);
			if (facep)
			{
				facep->setState(LLFace::HUD_RENDER);
			}
		}
		((LLViewerOctreeEntryData*)object->mDrawable)->setVisible();
	}

	LLViewerObject::const_child_list_t& child_list = object->getChildren();
	for (LLViewerObject::child_list_t::const_iterator
			iter = child_list.begin(), end = child_list.end();
		 iter != end; ++iter)
	{
		LLViewerObject* childp = *iter;
		if (childp && childp->mDrawable.notNull())
		{
			// Face may need to change draw pool to/from POOL_HUD
			gPipeline.markTextured(childp->mDrawable);
			gPipeline.markMoved(childp->mDrawable);

			if (mIsHUDAttachment)
			{
				((LLViewerOctreeEntryData*)childp->mDrawable)->setVisible();

				for (S32 i = 0, count = childp->mDrawable->getNumFaces();
					 i < count; ++i)
				{
					LLFace* facep = childp->mDrawable->getFace(i);
					if (facep)
					{
						facep->setState(LLFace::HUD_RENDER);
					}
				}
			}
		}
	}
}

bool LLViewerJointAttachment::addObject(LLViewerObject* object, bool is_self)
{
	if (!object || (is_self && !isAgentAvatarValid()))
	{
		return false;
	}

	object->extractAttachmentItemID();

	if (isObjectAttached(object))
	{
		llinfos << "Same object re-attached: " << object->getID() << llendl;
#if 0	// Do not do this: OpenSim grids (wrongly) send such duplicate attach
		// events after a TP into a new sim, and they (also wrongly) do not
		// send an addChild event after the attach event: with this code path
		// (i.e. removal of the object from the attachment list and waiting for
		// addChild to re-add it), we end up with phantom attachments still
		// rendered but not listed in mAttachedObjects !

		removeObject(object, is_self);
		// Pass through anyway to let setupDrawable() re-connect object to the
		// joint correctly

#else	// Instead, we make sure the drawable is properly connected and we
		// ignore the duplicate reattach event.
		setupDrawable(object); // re-connect object to the joint correctly
		return false;
#endif
	}

	// Two instances of the same inventory item attached: request detach and
	// kill the object in the meantime.
	if (is_self && getAttachedObject(object->getAttachmentItemID()))
	{
		llinfos << "Same inventory object re-attached, detaching spurious instance: "
				<< object->getAttachmentItemID() << llendl;
		object->markDead();

		// If this happens to be attached to self, then detach.
		LLVOAvatarSelf::detachAttachmentIntoInventory(object->getAttachmentItemID());
		return false;
	}

	mAttachedObjects.push_back(object);
	setupDrawable(object);

	if (is_self)
	{
		if (mIsHUDAttachment)
		{
			if (object->mText.notNull())
			{
				object->mText->setOnHUDAttachment(true);
			}
			LLViewerObject::const_child_list_t& child_list = object->getChildren();
			for (LLViewerObject::child_list_t::const_iterator
					iter = child_list.begin(), end = child_list.end();
				 iter != end; ++iter)
			{
				LLViewerObject* childp = *iter;
				if (childp && childp->mText.notNull())
				{
					childp->mText->setOnHUDAttachment(true);
				}
			}
		}
	}

	calcLOD();
	mUpdateXform = true;

//MK
	if (is_self && gRLenabled)
	{
		LLUUID item_id = object->getAttachmentItemID();
		std::string name = getName();
		LLStringUtil::toLower(name);
		// If this attachment point is locked then force detach, unless the
		// attached object was supposed to be reattached automatically
		if (!gRLInterface.canAttach(object, name) &&
			!gRLInterface.isRestoringOutfit())
		{
			bool just_reattaching = false;
			for (rl_reattach_it_t it = gRLInterface.mAssetsToReattach.begin();
				 it != gRLInterface.mAssetsToReattach.end(); ++it)
			{
				if (it->mId == item_id)
				{
					just_reattaching = true;
					break;
				}
			}
			if (!just_reattaching)
			{
				llinfos << "Illegally attached to a locked point: " << item_id
						<< ", detaching." << llendl;

				LLMessageSystem* msg = gMessageSystemp;
				msg->newMessage("ObjectDetach");
				msg->nextBlockFast(_PREHASH_AgentData);
				msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
				msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
				msg->nextBlockFast(_PREHASH_ObjectData);
				msg->addU32Fast(_PREHASH_ObjectLocalID, object->getLocalID());
				msg->sendReliable(gAgent.getRegionHost());

				gRLInterface.mJustDetached.mId = item_id;
				gRLInterface.mJustDetached.mName = getName();

				// Now notify that this object has been attached and will be
				// detached right away
				gRLInterface.notify("attached illegally " + getName());
			}
			else
			{
				// Notify that this object has just been reattached
				gRLInterface.notify("reattached legally " + getName());
			}
		}
		else
		{
			// Notify that this object has been attached
			gRLInterface.notify("attached legally " + getName());
		}

		// If the UUID of the attached item is contained in the list of the
		// objects waiting to reattach, signal it and remove it from the list.
		for (rl_reattach_it_t it = gRLInterface.mAssetsToReattach.begin();
			 it != gRLInterface.mAssetsToReattach.end(); ++it)
		{
			if (it->mId == item_id)
			{
				llinfos << "Reattached asset " << item_id << " automatically"
						<< llendl;
				gRLInterface.mReattaching = false;
				gRLInterface.mReattachTimeout = false;
				gRLInterface.mAssetsToReattach.erase(it);
#if 0
				gRLInterface.mJustReattached.mId = item_id;
				gRLInterface.mJustReattached.mName = getName();
#endif
				// Replace the previously stored asset id with the new viewer
				// id in the list of restrictions
				gRLInterface.replace(item_id, object->getRootEdit()->getID());
				break;
			}
		}
	}
//mk

	return true;
}

void LLViewerJointAttachment::removeObject(LLViewerObject* object,
										   bool is_self)
{
	attachedobjs_vec_t::iterator iter;
	attachedobjs_vec_t::iterator iend =  mAttachedObjects.end();
	for (iter = mAttachedObjects.begin(); iter != iend; ++iter)
	{
		LLViewerObject* attached_object = iter->get();
		if (attached_object == object)
		{
			break;
		}
	}
	if (iter == iend)
	{
		llwarns << "Could not find object to detach" << llendl;
		return;
	}
//MK
	if (is_self && gRLenabled && isAgentAvatarValid())
	{
		// We first need to check whether the object is locked, as some
		// techniques (like llAttachToAvatar) can kick even a locked attachment
		// off. If so, retain its UUID for later
		// Note: we need to delay the reattach a little, or we risk losing the
		// item in the inventory.
		LLInventoryItem* inv_item;
		inv_item = gRLInterface.getItem(object->getRootEdit()->getID());
		LLUUID inv_item_id;
		if (inv_item)
		{
			inv_item_id = inv_item->getUUID();
		}

		std::string target_attachpt;
		target_attachpt = gAgentAvatarp->getAttachedPointName(inv_item_id);
		inv_item_id = object->getAttachmentItemID();
 		if (!gRLInterface.canDetach(object) &&
#if 0
			// We did not just reattach something to this attach pt automatically
			gRLInterface.mJustReattached.mName != target_attachpt &&
#endif
			// We did not just detach something from this attach pt automatically
			gRLInterface.mJustDetached.mName != target_attachpt)
		{
			llinfos << "Detached a locked object : " << inv_item_id << " from "
					<< target_attachpt << llendl;

			// Now notify that this object has been detached and will be
			// reattached right away
			gRLInterface.notify("detached illegally " + getName());

			bool found = false;
			bool found_for_this_point = false;
			for (rl_reattach_it_t it = gRLInterface.mAssetsToReattach.begin();
				 it != gRLInterface.mAssetsToReattach.end(); ++it)
			{
				if (it->mId == inv_item_id)
				{
					found = true;
				}
				if (it->mName == target_attachpt)
				{
					found_for_this_point = true;
				}
			}

			if (!found && !found_for_this_point)
			{
				gRLInterface.mReattachTimer.reset();
				gRLInterface.mAssetsToReattach.emplace_back(inv_item_id,
															target_attachpt);
				// Little hack: store this item's asset id into the list of
				// restrictions so they are automatically reapplied when it is
				// reattached
				gRLInterface.replace(object->getRootEdit()->getID(),
									 inv_item_id);
			}
		}
		else if (inv_item)
		{
			// Notify that this object has been detached
			gRLInterface.notify("detached legally " + getName());
		}
		gRLInterface.mJustDetached.mId.setNull();
		gRLInterface.mJustDetached.mName.clear();
#if 0
		gRLInterface.mJustReattached.mId.setNull();
		gRLInterface.mJustReattached.mName.clear();
#endif
	}
//mk
	// Force object visibile
	setAttachmentVisibility(true);

	mAttachedObjects.erase(iter);
	if (object->mDrawable.notNull())
	{
		// If object is active, make it static
		if (object->mDrawable->isActive())
		{
			object->mDrawable->makeStatic(false);
		}

		LLVector3 cur_position = object->getRenderPosition();
		LLQuaternion cur_rotation = object->getRenderRotation();

		object->mDrawable->mXform.setPosition(cur_position);
		object->mDrawable->mXform.setRotation(cur_rotation);
		gPipeline.markMoved(object->mDrawable, true);
		// Face may need to change draw pool to/from POOL_HUD
		gPipeline.markTextured(object->mDrawable);

		if (is_self && mIsHUDAttachment)
		{
			for (S32 i = 0, count = object->mDrawable->getNumFaces();
				 i < count; ++i)
			{
				LLFace* facep = object->mDrawable->getFace(i);
				if (facep)
				{
					facep->clearState(LLFace::HUD_RENDER);
				}
			}
		}
	}

	LLViewerObject::const_child_list_t& child_list = object->getChildren();
	for (LLViewerObject::child_list_t::const_iterator
			iter = child_list.begin(), end = child_list.end();
		 iter != end; ++iter)
	{
		LLViewerObject* childp = *iter;
		if (childp && childp->mDrawable.notNull())
		{
			// Face may need to change draw pool to/from POOL_HUD:
			gPipeline.markTextured(childp->mDrawable);

			if (is_self && mIsHUDAttachment)
			{
				for (S32 i = 0, count = childp->mDrawable->getNumFaces();
					 i < count; ++i)
				{
					LLFace* facep = childp->mDrawable->getFace(i);
					if (facep)
					{
						facep->clearState(LLFace::HUD_RENDER);
					}
				}
			}
		}
	}

	if (is_self)
	{
		if (mIsHUDAttachment)
		{
			if (object->mText.notNull())
			{
				object->mText->setOnHUDAttachment(false);
			}
			LLViewerObject::const_child_list_t& child_list = object->getChildren();
			for (LLViewerObject::child_list_t::const_iterator
					iter = child_list.begin(), end = child_list.end();
				 iter != end; ++iter)
			{
				LLViewerObject* childp = *iter;
				if (childp && childp->mText.notNull())
				{
					childp->mText->setOnHUDAttachment(false);
				}
			}
		}
	}

	if (mAttachedObjects.size() == 0)
	{
		mUpdateXform = false;
	}
	object->setAttachmentItemID(LLUUID::null);
}

void LLViewerJointAttachment::setAttachmentVisibility(bool visible)
{
	for (attachedobjs_vec_t::const_iterator
			iter = mAttachedObjects.begin(), end = mAttachedObjects.end();
		 iter != end; ++iter)
	{
		LLViewerObject* attached_obj = iter->get();
		if (!attached_obj || attached_obj->mDrawable.isNull() ||
			!(attached_obj->mDrawable->getSpatialBridge()))
			continue;

		if (visible)
		{
			// *HACK: to make attachments not visible by disabling their type
			// mask !  This will break if you can ever attach non-volumes !
			// djs 02/14/03
			attached_obj->mDrawable->getSpatialBridge()->mDrawableType =
				attached_obj->isHUDAttachment() ? LLPipeline::RENDER_TYPE_HUD
												: LLPipeline::RENDER_TYPE_VOLUME;
		}
		else
		{
			attached_obj->mDrawable->getSpatialBridge()->mDrawableType = 0;
		}
	}
}

S32 LLViewerJointAttachment::getNumAnimatedObjects() const
{
	S32 count = 0;
	for (attachedobjs_vec_t::const_iterator
			iter = mAttachedObjects.begin(), end = mAttachedObjects.end();
		 iter != end; ++iter)
	{
		const LLViewerObject* attached_obj = iter->get();
		if (attached_obj && attached_obj->isAnimatedObject())
		{
			++count;
		}
	}
	return count;
}

void LLViewerJointAttachment::setOriginalPosition(LLVector3& position)
{
	mOriginalPos = position;
	setPosition(position);
}

void LLViewerJointAttachment::clampObjectPosition()
{
	for (attachedobjs_vec_t::const_iterator iter = mAttachedObjects.begin(),
											end = mAttachedObjects.end();
		 iter != end; ++iter)
	{
		if (LLViewerObject* attached_object = iter->get())
		{
			// *NOTE: object can drift when hitting maximum radius
			LLVector3 attachmentPos = attached_object->getPosition();
			F32 dist = attachmentPos.normalize();
			dist = llmin(dist, MAX_ATTACHMENT_DIST);
			attachmentPos *= dist;
			attached_object->setPosition(attachmentPos);
		}
	}
}

void LLViewerJointAttachment::calcLOD()
{
	F32 maxarea = 0;
	for (attachedobjs_vec_t::const_iterator iter = mAttachedObjects.begin(),
											end = mAttachedObjects.end();
		 iter != end; ++iter)
	{
		if (LLViewerObject* attached_object = iter->get())
		{
			maxarea = llmax(maxarea,
							attached_object->getMaxScale() * attached_object->getMidScale());
			LLViewerObject::const_child_list_t& child_list = attached_object->getChildren();
			for (LLViewerObject::child_list_t::const_iterator iter = child_list.begin();
				 iter != child_list.end(); ++iter)
			{
				LLViewerObject* childp = *iter;
				F32 area = childp->getMaxScale() * childp->getMidScale();
				maxarea = llmax(maxarea, area);
			}
		}
	}
	maxarea = llclamp(maxarea, .01f * .01f, 1.f);
	F32 avatar_area = (4.f * 4.f); // pixels for an avatar sized attachment
	F32 min_pixel_area = avatar_area / maxarea;
	setLOD(min_pixel_area);
}

bool LLViewerJointAttachment::updateLOD(F32 pixel_area, bool activate)
{
	if (mValid)
	{
		return false;
	}

	setValid(true, true);
	return true;
}

bool LLViewerJointAttachment::isObjectAttached(const LLViewerObject* viewer_object) const
{
	for (attachedobjs_vec_t::const_iterator iter = mAttachedObjects.begin(),
											end = mAttachedObjects.end();
		 iter != end; ++iter)
	{
		const LLViewerObject* attached_object = iter->get();
		if (attached_object == viewer_object)
		{
			return true;
		}
	}
	return false;
}

const LLViewerObject* LLViewerJointAttachment::getAttachedObject(const LLUUID& object_id) const
{
	for (attachedobjs_vec_t::const_iterator iter = mAttachedObjects.begin(),
											end = mAttachedObjects.end();
		 iter != end; ++iter)
	{
		const LLViewerObject* attached_object = iter->get();
		if (attached_object->getAttachmentItemID() == object_id)
		{
			return attached_object;
		}
	}
	return NULL;
}

LLViewerObject* LLViewerJointAttachment::getAttachedObject(const LLUUID& object_id)
{
	for (attachedobjs_vec_t::const_iterator iter = mAttachedObjects.begin(),
											end = mAttachedObjects.end();
		 iter != end; ++iter)
	{
		LLViewerObject* attached_object = iter->get();
		if (attached_object->getAttachmentItemID() == object_id)
		{
			return attached_object;
		}
	}
	return NULL;
}
