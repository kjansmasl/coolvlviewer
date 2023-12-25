/**
 * @file llavatarjoint.cpp
 * @brief Implementation of LLAvatarJoint class
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

#include "llavatarjoint.h"

#include "llavatarappearance.h"
#include "llgl.h"
#include "llmath.h"
#include "llrender.h"

//static
bool LLAvatarJoint::sDisableLOD = false;

//-----------------------------------------------------------------------------
// LLAvatarJoint class
//-----------------------------------------------------------------------------

LLAvatarJoint::LLAvatarJoint()
:	LLJoint()
{
	init();
}

LLAvatarJoint::LLAvatarJoint(const std::string& name, LLJoint* parent)
:	LLJoint(name, parent)
{
	init();
}

void LLAvatarJoint::init()
{
	mVisible = true;
	mValid = mIsTransparent = false;
	mComponents = SC_JOINT | SC_BONE | SC_AXES;
	mMinPixelArea = DEFAULT_AVATAR_JOINT_LOD;
	mPickName = PN_DEFAULT;
	mMeshID = 0;
}

void LLAvatarJoint::setValid(bool valid, bool recursive)
{
	// Set visibility for this joint
	mValid = valid;

	// Set visibility for children
	if (recursive)
	{
		for (child_list_t::iterator iter = mChildren.begin(),
									end = mChildren.end();
			 iter != end; ++iter)
		{
			LLJoint* jointp = *iter;
			if (jointp)
			{
				LLAvatarJoint* avjointp = jointp->asAvatarJoint();
				if (avjointp)
				{
					avjointp->setValid(valid, true);
				}
			}
		}
	}
}

void LLAvatarJoint::setSkeletonComponents(U32 comp, bool recursive)
{
	mComponents = comp;
	if (recursive)
	{
		for (child_list_t::iterator iter = mChildren.begin(),
									end = mChildren.end();
			 iter != end; ++iter)
		{
			LLJoint* jointp = *iter;
			if (jointp)
			{
				LLAvatarJoint* avjointp = jointp->asAvatarJoint();
				if (avjointp)
				{
					avjointp->setSkeletonComponents(comp, recursive);
				}
			}
		}
	}
}

void LLAvatarJoint::setVisible(bool visible, bool recursive)
{
	mVisible = visible;

	if (recursive)
	{
		for (child_list_t::iterator iter = mChildren.begin(),
									end = mChildren.end();
			 iter != end; ++iter)
		{
			LLJoint* jointp = *iter;
			if (jointp)
			{
				LLAvatarJoint* avjointp = jointp->asAvatarJoint();
				if (avjointp)
				{
					avjointp->setVisible(visible, recursive);
				}
			}
		}
	}
}

void LLAvatarJoint::updateFaceSizes(U32& num_vertices, U32& num_indices,
									F32 pixel_area)
{
	for (child_list_t::iterator iter = mChildren.begin(),
								end = mChildren.end();
		 iter != end; ++iter)
	{
		LLJoint* jointp = *iter;
		if (jointp)
		{
			LLAvatarJoint* avjointp = jointp->asAvatarJoint();
			if (avjointp)
			{
				avjointp->updateFaceSizes(num_vertices, num_indices,
										  pixel_area);
			}
		}
	}
}

void LLAvatarJoint::updateFaceData(LLFace* face, F32 pixel_area,
								   bool damp_wind, bool terse_update)
{
	for (child_list_t::iterator iter = mChildren.begin(),
								end = mChildren.end();
		 iter != end; ++iter)
	{
		LLJoint* jointp = *iter;
		if (jointp)
		{
			LLAvatarJoint* avjointp = jointp->asAvatarJoint();
			if (avjointp)
			{
				avjointp->updateFaceData(face, pixel_area, damp_wind,
										 terse_update);
			}
		}
	}
}

void LLAvatarJoint::updateJointGeometry()
{
	for (child_list_t::iterator iter = mChildren.begin(),
								end = mChildren.end();
		 iter != end; ++iter)
	{
		LLJoint* jointp = *iter;
		if (jointp)
		{
			LLAvatarJoint* avjointp = jointp->asAvatarJoint();
			if (avjointp)
			{
				avjointp->updateJointGeometry();
			}
		}
	}
}

bool LLAvatarJoint::updateLOD(F32 pixel_area, bool activate)
{
	bool lod_changed = false;
	bool found_lod = false;

	for (child_list_t::iterator iter = mChildren.begin(),
								end = mChildren.end();
		 iter != end; ++iter)
	{
		LLJoint* jointp = *iter;
		if (!jointp) continue;

		LLAvatarJoint* avjointp = jointp->asAvatarJoint();
		if (!avjointp) continue;

		F32 joint_lod = avjointp->getLOD();

		if (found_lod || joint_lod == DEFAULT_AVATAR_JOINT_LOD)
		{
			// We have already found a joint to enable, so enable the rest as
			// alternatives
			lod_changed |= avjointp->updateLOD(pixel_area, true);
		}
		else if (pixel_area >= joint_lod || sDisableLOD)
		{
			lod_changed |= avjointp->updateLOD(pixel_area, true);
			found_lod = true;
		}
		else
		{
			lod_changed |= avjointp->updateLOD(pixel_area, false);
		}
	}
	return lod_changed;
}

void LLAvatarJoint::dump()
{
	for (child_list_t::iterator iter = mChildren.begin(),
								end = mChildren.end();
		 iter != end; ++iter)
	{
		LLJoint* jointp = *iter;
		if (jointp)
		{
			LLAvatarJoint* avjointp = jointp->asAvatarJoint();
			if (avjointp)
			{
				avjointp->dump();
			}
		}
	}
}

void LLAvatarJoint::setMeshesToChildren()
{
	removeAllChildren();
	for (avatar_joint_mesh_list_t::iterator iter = mMeshParts.begin(),
											end = mMeshParts.end();
		iter != end; ++iter)
	{
		addChild(*iter);
	}
}

//-----------------------------------------------------------------------------
// LLAvatarJointCollisionVolume class
//-----------------------------------------------------------------------------

LLAvatarJointCollisionVolume::LLAvatarJointCollisionVolume()
{
	mUpdateXform = false;
}

//virtual
U32 LLAvatarJointCollisionVolume::render(F32 pixelArea, bool first_pass,
										 bool is_dummy)
{
	llerrs << "Cannot call render() on LLAvatarJointCollisionVolume" << llendl;
	return 0;
}

LLVector3 LLAvatarJointCollisionVolume::getVolumePos(const LLVector3& offset)
{
	mUpdateXform = true;

	LLVector3 result = offset;
	result.scaleVec(getScale());
	result.rotVec(getWorldRotation());
	result += getWorldPosition();

	return result;
}

void LLAvatarJointCollisionVolume::renderCollision()
{
	updateWorldMatrix();

	gGL.pushMatrix();
	gGL.multMatrix(mXform.getWorldMatrix().getF32ptr());

	gGL.diffuseColor3f(0.f, 0.f, 1.f);

	gGL.begin(LLRender::LINES);

	static LLVector3 v[] = {
		LLVector3::x_axis,
		LLVector3::x_axis_neg,
		LLVector3::y_axis,
		LLVector3::y_axis_neg,
		LLVector3::z_axis_neg,
		LLVector3::z_axis,
	};

	// Sides
	gGL.vertex3fv(v[0].mV);
	gGL.vertex3fv(v[2].mV);

	gGL.vertex3fv(v[0].mV);
	gGL.vertex3fv(v[3].mV);

	gGL.vertex3fv(v[1].mV);
	gGL.vertex3fv(v[2].mV);

	gGL.vertex3fv(v[1].mV);
	gGL.vertex3fv(v[3].mV);

	// Top
	gGL.vertex3fv(v[0].mV);
	gGL.vertex3fv(v[4].mV);

	gGL.vertex3fv(v[1].mV);
	gGL.vertex3fv(v[4].mV);

	gGL.vertex3fv(v[2].mV);
	gGL.vertex3fv(v[4].mV);

	gGL.vertex3fv(v[3].mV);
	gGL.vertex3fv(v[4].mV);

	// Bottom
	gGL.vertex3fv(v[0].mV);
	gGL.vertex3fv(v[5].mV);

	gGL.vertex3fv(v[1].mV);
	gGL.vertex3fv(v[5].mV);

	gGL.vertex3fv(v[2].mV);
	gGL.vertex3fv(v[5].mV);

	gGL.vertex3fv(v[3].mV);
	gGL.vertex3fv(v[5].mV);

	gGL.end();

	gGL.popMatrix();
}
