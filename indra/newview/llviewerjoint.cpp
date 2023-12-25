/**
 * @file llviewerjoint.cpp
 * @brief Implementation of LLViewerJoint class
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

#include "llviewerjoint.h"

#include "llgl.h"
#include "llrender.h"

#include "llpipeline.h"
#include "llvoavatar.h"

constexpr S32 MIN_PIXEL_AREA_3PASS_HAIR = 64 * 64;

LLViewerJoint::LLViewerJoint()
:	LLAvatarJoint()
{
}

LLViewerJoint::LLViewerJoint(const std::string& name, LLJoint* parentp)
:	LLAvatarJoint(name, parentp)
{
}

U32 LLViewerJoint::render(F32 pixel_area, bool first_pass, bool is_dummy)
{
	U32 triangle_count = 0;

	// Ignore invisible objects
	if (mValid)
	{
		// If object is transparent, defer it, otherwise give the joint
		// subclass a chance to draw itself
		if (is_dummy)
		{
			triangle_count += drawShape(pixel_area, first_pass, is_dummy);
		}
		else if (LLPipeline::sShadowRender)
		{
			triangle_count += drawShape(pixel_area, first_pass, is_dummy);
		}
		else if (isTransparent() && !LLPipeline::sReflectionRender)
		{
			// Hair and Skirt
			if (pixel_area > MIN_PIXEL_AREA_3PASS_HAIR)
			{
				// Render all three passes
				LLGLDisable cull(GL_CULL_FACE);
				// First pass renders without writing to the z buffer
				{
					LLGLDepthTest gls_depth(GL_TRUE, GL_FALSE);
					triangle_count += drawShape(pixel_area, first_pass,
												is_dummy);
				}
				// Second pass writes to z buffer only
				gGL.setColorMask(false, false);
				{
					triangle_count += drawShape(pixel_area, false, is_dummy);
				}
				// Third past respects z buffer and writes color
				gGL.setColorMask(true, false);
				{
					LLGLDepthTest gls_depth(GL_TRUE, GL_FALSE);
					triangle_count += drawShape(pixel_area, false, is_dummy);
				}
			}
			else
			{
				// Render Inside (no Z buffer write)
				glCullFace(GL_FRONT);
				{
					LLGLDepthTest gls_depth(GL_TRUE, GL_FALSE);
					triangle_count += drawShape(pixel_area, first_pass,
												is_dummy);
				}
				// Render Outside (write to the Z buffer)
				glCullFace(GL_BACK);
				{
					triangle_count += drawShape(pixel_area, false, is_dummy);
				}
			}
		}
		else
		{
			// Set up render state
			triangle_count += drawShape(pixel_area, first_pass);
		}
	}

	// Render children
	for (S32 i = 0, count = mChildren.size(); i < count; ++i)
	{
		LLJoint* jointp = mChildren[i];
		if (!jointp) continue;	// Paranoia

		LLAvatarJoint* avjointp = jointp->asAvatarJoint();
		if (!avjointp) continue;

		F32 joint_lod = avjointp->getLOD();
		if (pixel_area >= joint_lod || sDisableLOD)
		{
			triangle_count += avjointp->render(pixel_area, true, is_dummy);
			if (joint_lod != DEFAULT_AVATAR_JOINT_LOD)
			{
				break;
			}
		}
	}

	stop_glerror();

	return triangle_count;
}
