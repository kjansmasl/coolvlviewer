/**
 * @file qltoolalign.cpp
 * @brief A tool to align objects
 *
 * $LicenseInfo:firstyear=2010&license=viewergpl$
 *
 * Copyright (c) 2010, Qarl Linden
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

#include "qltoolalign.h"

#include "llbbox.h"
#include "llrenderutils.h"				// For gBox, gCone

#include "llagent.h"
#include "llfloatertools.h"
#include "llselectmgr.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerobject.h"
#include "llviewerwindow.h"

constexpr F32 MANIPULATOR_SIZE = 5.0;
constexpr F32 MANIPULATOR_SELECT_SIZE = 20.0;

QLToolAlign gToolAlign;

QLToolAlign::QLToolAlign()
:	LLTool("Align")
{
}

bool QLToolAlign::handleMouseDown(S32 x, S32 y, MASK mask)
{
	if (mHighlightedAxis != -1)
	{
		align();
	}
	else if (gViewerWindowp)
	{
		gViewerWindowp->pickAsync(x, y, mask, pickCallback);
	}
	return true;
}

bool QLToolAlign::handleMouseUp(S32 x, S32 y, MASK mask)
{
	// first, perform normal processing in case this was a quick-click
	handleHover(x, y, mask);
	gSelectMgr.updateSelectionCenter();
	bool handled = false;
	if (hasMouseCapture())
	{
		handled = true;
		setMouseCapture(false);
	}
	return handled;
}

void QLToolAlign::pickCallback(const LLPickInfo& pick_info)
{
	LLViewerObject* object = pick_info.getObject();

	if (object)
	{
		if (object->isAvatar())
		{
			return;
		}

		if (pick_info.mKeyMask & MASK_SHIFT)
		{
			// If object not selected, select it
			if (!object->isSelected())
			{
				gSelectMgr.selectObjectAndFamily(object);
			}
			else
			{
				gSelectMgr.deselectObjectAndFamily(object);
			}
		}
		else
		{
			gSelectMgr.deselectAll();
			gSelectMgr.selectObjectAndFamily(object);
		}

	}
	else
	{
		if (pick_info.mKeyMask != MASK_SHIFT)
		{
			gSelectMgr.deselectAll();
		}
	}

	gSelectMgr.promoteSelectionToRoot();
}

void QLToolAlign::handleSelect()
{
	LL_DEBUGS("ToolAlign") << "Tool Align in select." << LL_ENDL;
	// No parts, please
	gSelectMgr.promoteSelectionToRoot();
	gSelectMgr.updateSelectionCenter();
	if (gFloaterToolsp)
	{
		gFloaterToolsp->setStatusText("align");
	}
}

bool QLToolAlign::findSelectedManipulator(S32 x, S32 y)
{
	if (!gViewerWindowp) return false;

	mHighlightedAxis = -1;
	mHighlightedDirection = 0;

	LLMatrix4 transform;
	if (gSelectMgr.getSelection()->getSelectType() == SELECT_TYPE_HUD)
	{
		LLVector4 translation(mBBox.getCenterAgent());
		transform.initRotTrans(mBBox.getRotation(), translation);
		LLMatrix4 cfr(OGL_TO_CFR_ROTATION);
		transform *= cfr;
		LLMatrix4 window_scale;
		F32 zoom_level = 2.f * gAgent.mHUDCurZoom;
		window_scale.initAll(LLVector3(zoom_level / gViewerCamera.getAspect(),
									   zoom_level, 0.f),
							 LLQuaternion::DEFAULT,
							 LLVector3::zero);
		transform *= window_scale;
	}
	else
	{
		transform.initAll(LLVector3(1.f, 1.f, 1.f), mBBox.getRotation(), mBBox.getCenterAgent());

		LLMatrix4 projection_matrix = gViewerCamera.getProjection();
		LLMatrix4 model_matrix = gViewerCamera.getModelview();

		transform *= model_matrix;
		transform *= projection_matrix;
	}

	F32 half_width = (F32)gViewerWindowp->getWindowWidth() * 0.5f;
	F32 half_height = (F32)gViewerWindowp->getWindowHeight() * 0.5f;
	LLVector2 manip2d;
	LLVector2 mousePos((F32)x - half_width, (F32)y - half_height);
	LLVector2 delta;

	LLVector3 bbox_scale = mBBox.getMaxLocal() - mBBox.getMinLocal();

	for (S32 axis = VX; axis <= (S32)VZ; ++axis)
	{
		for (F32 direction = -1.0; direction <= 1.0; direction += 2.0)
		{
			LLVector3 axis_vector = LLVector3(0, 0, 0);
			axis_vector.mV[axis] = direction * bbox_scale.mV[axis] * 0.5f;

			LLVector4 manipulator_center = 	LLVector4(axis_vector);

			LLVector4 screen_center = manipulator_center * transform;
			screen_center /= screen_center.mV[VW];

			manip2d.set(screen_center.mV[VX] * half_width,
						screen_center.mV[VY] * half_height);

			delta = manip2d - mousePos;

			if (delta.lengthSquared() < MANIPULATOR_SELECT_SIZE * MANIPULATOR_SELECT_SIZE)
			{
				mHighlightedAxis = axis;
				mHighlightedDirection = direction;
				return true;
			}

		}
	}

	return false;
}

bool QLToolAlign::handleHover(S32 x, S32 y, MASK mask)
{
	if (mask & MASK_SHIFT)
	{
		mForce = false;
	}
	else
	{
		mForce = true;
	}

	if (gViewerWindowp)
	{
		gViewerWindowp->setCursor(UI_CURSOR_ARROW);
	}

	return findSelectedManipulator(x, y);
}

void setup_transforms_bbox(LLBBox bbox)
{
	// translate to center
	LLVector3 center = bbox.getCenterAgent();
	gGL.translatef(center.mV[VX], center.mV[VY], center.mV[VZ]);

	// rotate
	LLQuaternion rotation = bbox.getRotation();
	F32 angle_radians, x, y, z;
	rotation.getAngleAxis(&angle_radians, &x, &y, &z);
	// gGL has no rotate method (despite having translate and scale).
	// So we hack.
	gGL.flush();
	gGL.rotatef(angle_radians * RAD_TO_DEG, x, y, z);

	// scale
	LLVector3 scale = bbox.getMaxLocal() - bbox.getMinLocal();
	gGL.scalef(scale.mV[VX], scale.mV[VY], scale.mV[VZ]);
}

void render_bbox(LLBBox bbox)
{
	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.pushMatrix();

	setup_transforms_bbox(bbox);

	gGL.flush();
	gBox.render();

	gGL.popMatrix();
}

void render_cone_bbox(LLBBox bbox)
{
	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.pushMatrix();

	setup_transforms_bbox(bbox);

	gGL.flush();
	gCone.render();

	gGL.popMatrix();
}

// the selection bbox isn't axis aligned, so we must construct one
// should this be cached in the selection manager?  yes.
LLBBox get_selection_axis_aligned_bbox()
{
	LLBBox selection_bbox = gSelectMgr.getBBoxOfSelection();
	LLVector3 position = selection_bbox.getPositionAgent();

	LLBBox axis_aligned_bbox(position, LLQuaternion(), LLVector3::zero,
							 LLVector3::zero);
	axis_aligned_bbox.addPointLocal(LLVector3::zero);

	// cycle over the nodes in selection
	for (LLObjectSelection::iterator it = gSelectMgr.getSelection()->begin(),
									 end = gSelectMgr.getSelection()->end();
		 it != end; ++it)
	{
		LLSelectNode* select_node = *it;
		if (select_node)
		{
			LLViewerObject* object = select_node->getObject();
			if (object)
			{
				axis_aligned_bbox.addBBoxAgent(object->getBoundingBoxAgent());
			}
		}
	}

	return axis_aligned_bbox;
}

void QLToolAlign::computeManipulatorSize()
{
	if (gSelectMgr.getSelection()->getSelectType() == SELECT_TYPE_HUD)
	{
		mManipulatorSize = MANIPULATOR_SIZE / (gViewerCamera.getViewHeightInPixels() *
											   gAgent.mHUDCurZoom);
	}
	else
	{
		F32 distance = dist_vec(gAgent.getCameraPositionAgent(), mBBox.getCenterAgent());

		if (distance > 0.001f)
		{
			// range != zero
			F32 fraction_of_fov = MANIPULATOR_SIZE / gViewerCamera.getViewHeightInPixels();
			F32 apparent_angle = fraction_of_fov * gViewerCamera.getView();  // radians
			mManipulatorSize = MANIPULATOR_SIZE * distance * tanf(apparent_angle);
		}
		else
		{
			// range == zero
			mManipulatorSize = MANIPULATOR_SIZE;
		}
	}
}

LLColor4 manipulator_color[3] = {	LLColor4(0.7f, 0.0f, 0.0f, 0.5f),
									LLColor4(0.0f, 0.7f, 0.0f, 0.5f),
									LLColor4(0.0f, 0.0f, 0.7f, 0.5f) };

void QLToolAlign::renderManipulators()
{
	computeManipulatorSize();
	LLVector3 bbox_center = mBBox.getCenterAgent();
	LLVector3 bbox_scale = mBBox.getMaxLocal() - mBBox.getMinLocal();

	const S32 arrows = mForce ? 2 : 1;
	for (S32 axis = VX; axis <= (S32)VZ; ++axis)
	{
		for (F32 direction = -1.f; direction <= 1.f; direction += 2.f)
		{
			F32 size = mManipulatorSize;
			LLColor4 color = manipulator_color[axis];

			if (axis == mHighlightedAxis && direction == mHighlightedDirection)
			{
				size *= 2.f;
				color *= 1.5f;
			}

			const F32 size_third = size / 3.f;
			const LLVector3 vec1 = LLVector3(-1.f, -1.f, -0.75f) * size * 0.5f;
			const LLVector3 vec2 = LLVector3(1.f, 1.f, 0.75f) * size * 0.5f;
			LLVector3 axis_vector, manipulator_center;
			LLQuaternion manipulator_rotation;
			for (S32 i = 0; i < arrows; ++i)
			{
				axis_vector.clear();
				axis_vector.mV[axis] = direction *
									   (bbox_scale.mV[axis] * 0.5f +
										i * size_third);

				manipulator_center = bbox_center + axis_vector;

				manipulator_rotation.shortestArc(LLVector3::z_axis,
												 -1.f * axis_vector);

				LLBBox manipulator_bbox(manipulator_center,
										manipulator_rotation,
										LLVector3::zero, LLVector3::zero);
				manipulator_bbox.addPointLocal(vec1);
				manipulator_bbox.addPointLocal(vec2);

				gGL.color4fv(color.mV);
				// Sadly, gCone does not use gGL like gBox does so we also set
				// the raw GL color. Hopefully this would not screw-up later
				// rendering.
				glColor4fv(color.mV);

				render_cone_bbox(manipulator_bbox);
			}
		}
	}
}

void QLToolAlign::render()
{
	mBBox = get_selection_axis_aligned_bbox();

	// Draw bounding box
	LLGLSUIDefault gls_ui;
	LLGLEnable gl_blend(GL_BLEND);
	LLGLDepthTest gls_depth(GL_FALSE);
	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

	// Render box
	static const LLColor4 default_normal_color(0.7f, 0.7f, 0.7f, 0.1f);
	gGL.color4fv(default_normal_color.mV);

	LLObjectSelectionHandle selection = gSelectMgr.getEditSelection();
	bool can_move = (selection->getObjectCount() != 0);
	if (can_move)
	{
		struct f final : public LLSelectedObjectFunctor
		{
			bool apply(LLViewerObject* objectp) override
			{
				static LLCachedControl<bool> linked_parts(gSavedSettings,
														  "EditLinkedParts");
				return objectp->permMove() &&
					   (objectp->permModify() || !linked_parts);
			}
		} func;
		can_move = selection->applyToObjects(&func);
	}
	if (can_move)
	{
		render_bbox(mBBox);
		renderManipulators();
	}
}

// Only works for our specialized (AABB, position centered) bboxes
bool bbox_overlap(LLBBox bbox1, LLBBox bbox2)
{
	constexpr F32 FUDGE = 0.001f;  // Because of stupid SL precision/rounding

	LLVector3 delta = bbox1.getCenterAgent() - bbox2.getCenterAgent();

	LLVector3 half_extent = (bbox1.getExtentLocal() +
							 bbox2.getExtentLocal()) * 0.5f;

	return (fabs(delta.mV[VX]) < half_extent.mV[VX] - FUDGE &&
			fabs(delta.mV[VY]) < half_extent.mV[VY] - FUDGE &&
			fabs(delta.mV[VZ]) < half_extent.mV[VZ] - FUDGE);
}

// Used to sort bboxes before packing

typedef std::map<LLPointer<LLViewerObject>, LLBBox> bbox_map_t;

class BBoxCompare
{
public:
	BBoxCompare(S32 axis, F32 direction, bbox_map_t& bboxes)
	:	mAxis(axis),
		mDirection(direction),
		mBBoxes(bboxes)
	{
	}

	bool operator() (LLViewerObject* object1, LLViewerObject* object2)
	{
		LLVector3 corner1 = mBBoxes[object1].getCenterAgent() -
							mDirection *
							mBBoxes[object1].getExtentLocal() * 0.5f;

		LLVector3 corner2 = mBBoxes[object2].getCenterAgent() -
							mDirection *
							mBBoxes[object2].getExtentLocal() * 0.5f;

		return mDirection * corner1.mV[mAxis] < mDirection * corner2.mV[mAxis];
	}

public:
	bbox_map_t&	mBBoxes;
	S32			mAxis;
	F32			mDirection;
};

void QLToolAlign::align()
{
	// No linkset parts, please
	gSelectMgr.promoteSelectionToRoot();

	std::vector<LLPointer<LLViewerObject> > objects;
	bbox_map_t original_bboxes;

    // Cycle over the nodes in selection and collect them into an array
	for (LLObjectSelection::root_iterator
			it = gSelectMgr.getSelection()->root_begin(),
			end = gSelectMgr.getSelection()->root_end();
		 it != end; ++it)
	{
		LLSelectNode* select_node = *it;
		if (!select_node)	// Paranoia
		{
			continue;
		}

		LLViewerObject* object = select_node->getObject();
		if (!object)		// Paranoia bis
		{
			continue;
		}

		LLVector3 position = object->getPositionAgent();

		LLBBox bbox(position, LLQuaternion(), LLVector3::zero,
					LLVector3::zero);
		bbox.addPointLocal(LLVector3::zero);

		// Add the parent's bbox
		bbox.addBBoxAgent(object->getBoundingBoxAgent());
		typedef LLViewerObject::const_child_list_t child_list_t;
		const child_list_t& children = object->getChildren();

		for (child_list_t::const_iterator i = children.begin(),
										  e = children.end();
			 i != e; ++i)
		{
			// Add the child's bbox
			LLViewerObject* child = *i;
			bbox.addBBoxAgent(child->getBoundingBoxAgent());
		}

		objects.push_back(object);
		original_bboxes[object] = bbox;
	}

	S32 axis = mHighlightedAxis;
	F32 direction = mHighlightedDirection;

	// Sort them into positional order for proper packing
	BBoxCompare compare(axis, direction, original_bboxes);
	sort(objects.begin(), objects.end(), compare);

	// Storage for their new position after alignment; start with original
	// position first
	bbox_map_t new_bboxes = original_bboxes;

	// Find new positions
	for (S32 i = 0, count = objects.size(); i < count; ++i)
	{
		LLVector3 target_corner = mBBox.getCenterAgent() -
								  direction * mBBox.getExtentLocal() * 0.5f;

		LLViewerObject* object = objects[i];

		const LLBBox& this_bbox = original_bboxes[object];
		LLVector3 this_corner = this_bbox.getCenterAgent() -
								direction * this_bbox.getExtentLocal() * 0.5f;

		// For packing, we cycle over several possible positions, taking the
		// smallest that does not overlap
		// 999999 guaranteed not to be the smallest
		F32 smallest = direction * 9999999;
		for (S32 j = 0; j <= i; ++j)
		{
			// Now far must it move?
			LLVector3 delta = target_corner - this_corner;

			// New position moves only on one axis, please
			LLVector3 delta_one_axis = LLVector3(0, 0, 0);
			delta_one_axis.mV[axis] = delta.mV[axis];

			LLVector3 new_position =
				this_bbox.getCenterAgent() + delta_one_axis;

			// Construct the new bbox
			LLBBox new_bbox(new_position, LLQuaternion(), LLVector3::zero,
							LLVector3::zero);
			new_bbox.addPointLocal(this_bbox.getExtentLocal() * 0.5f);
			new_bbox.addPointLocal(this_bbox.getExtentLocal() * -0.5f);

			// Check to see if it overlaps the previously placed objects
			bool overlap = false;

			LL_DEBUGS("ToolAlign") << "i=" << i << " j=" << j << LL_ENDL;

			if (!mForce) // Well, do not check if in force mode
			{
				for (S32 k = 0; k < i; ++k)
				{
					LLViewerObject* other_object = objects[k];
					LLBBox other_bbox = new_bboxes[other_object];

					bool overlaps_this = bbox_overlap(other_bbox, new_bbox);
					if (overlaps_this)
					{
						LL_DEBUGS("ToolAlign") << "Overlap: "
											   << new_bbox.getCenterAgent()
											   << " / "
											   << other_bbox.getCenterAgent()
											   << " - Extent: "
											   << new_bbox.getExtentLocal()
											   << " / "
											   << other_bbox.getExtentLocal()
											   << LL_ENDL;
					}
					overlap |= overlaps_this;
				}
			}

			if (!overlap)
			{
				F32 this_value = (new_bbox.getCenterAgent() -
								  direction *
								  new_bbox.getExtentLocal() * 0.5f).mV[axis];

				if (direction * this_value < direction * smallest)
				{
					smallest = this_value;
					// Store it
					new_bboxes[object] = new_bbox;
				}
			}

			// Update target for next time through the loop
			if (j < (S32)objects.size())
			{
				LLBBox next_bbox = new_bboxes[objects[j]];
				target_corner = next_bbox.getCenterAgent() +
								direction * next_bbox.getExtentLocal() * 0.5f;
			}
		}
	}

	// Now move them
	LLVector3 delta;
	for (S32 i = 0, count = objects.size(); i < count; ++i)
	{
		LLViewerObject* object = objects[i];
		const LLBBox& original_bbox = original_bboxes[object];
		const LLBBox& new_bbox = new_bboxes[object];
		delta = new_bbox.getCenterAgent() - original_bbox.getCenterAgent();
		object->setPositionLocal(object->getPositionAgent() + delta);
	}

	gSelectMgr.sendMultipleUpdate(UPD_POSITION);
}
