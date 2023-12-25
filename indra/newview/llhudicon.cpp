/**
 * @file llhudicon.cpp
 * @brief LLHUDIcon class implementation
 *
 * $LicenseInfo:firstyear=2006&license=viewergpl$
 *
 * Copyright (c) 2006-2009, Linden Research, Inc.
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

#include "llhudicon.h"

#include "llgl.h"
#include "llrender.h"

#include "lldrawable.h"
#include "llviewercamera.h"
#include "llviewerobject.h"
#include "llviewertexture.h"
#include "llviewerwindow.h"

constexpr F32 ANIM_TIME = 0.4f;
constexpr F32 DIST_START_FADE = 15.f;
constexpr F32 DIST_END_FADE = 30.f;
constexpr F32 FADE_OUT_TIME = 1.f;

// Static members
F32 LLHUDIcon::MAX_VISIBLE_TIME = 15.f;
LLHUDIcon::icon_instance_t LLHUDIcon::sIconInstances;

// helper function
static F32 calc_bouncy_animation(F32 x)
{
	return cosf(x * F_PI * 2.5f - F_PI_BY_TWO) * (0.1f * x - 0.4f) + x * 1.3f;
}

LLHUDIcon::LLHUDIcon(U8 type)
:	LLHUDObject(type),
	mImagep(NULL),
	mScale(0.1f),
	mHidden(false),
	mClickedCallback(NULL)
{
	sIconInstances.emplace_back(this);
}

//virtual
LLHUDIcon::~LLHUDIcon()
{
	mImagep = NULL;
}

//virtual
void LLHUDIcon::render()
{
	LLGLSUIDefault texture_state;
	LLGLDepthTest gls_depth(GL_TRUE);

	if (mHidden)
	{
		return;
	}

	if (mSourceObject.isNull() || mImagep.isNull() ||
		mSourceObject->mDrawable.isNull())
	{
		markDead();
		return;
	}

	LLVector3 obj_position = mSourceObject->getRenderPosition();

	// Put the icon above and in front of the object.
	// RN: do not use drawable radius, it is fricking HUGE
	LLVector3 icon_relative_pos = gViewerCamera.getUpAxis() *
								  ~mSourceObject->getRenderRotation();
	icon_relative_pos.abs();

	F32 distance_scale =
		llmin(mSourceObject->getScale().mV[VX] / icon_relative_pos.mV[VX],
			  mSourceObject->getScale().mV[VY] / icon_relative_pos.mV[VY],
			  mSourceObject->getScale().mV[VZ] / icon_relative_pos.mV[VZ]);
	F32 up_distance = 0.5f * distance_scale;
	LLVector3 icon_position =
		obj_position + 1.2f * up_distance * gViewerCamera.getUpAxis();

	LLVector3 icon_to_cam = gViewerCamera.getOrigin() - icon_position;
	icon_to_cam.normalize();

	icon_position +=
		icon_to_cam * mSourceObject->mDrawable->getRadius() * 1.1f;

	mDistance = dist_vec(icon_position, gViewerCamera.getOrigin());

	F32 alpha_factor = clamp_rescale(mDistance, DIST_START_FADE, DIST_END_FADE,
									 1.f, 0.f);

	LLVector3 x_pixel_vec;
	LLVector3 y_pixel_vec;

	gViewerCamera.getPixelVectors(icon_position, y_pixel_vec, x_pixel_vec);

	F32 scale_factor = 1.f;
	if (mAnimTimer.getElapsedTimeF32() < ANIM_TIME)
	{
		scale_factor =
			llmax(0.f,
				  calc_bouncy_animation(mAnimTimer.getElapsedTimeF32() /
										ANIM_TIME));
	}

	F32 time_elapsed = mLifeTimer.getElapsedTimeF32();
	if (time_elapsed > MAX_VISIBLE_TIME)
	{
		markDead();
		return;
	}

	if (time_elapsed > MAX_VISIBLE_TIME - FADE_OUT_TIME)
	{
		alpha_factor *= clamp_rescale(time_elapsed,
									  MAX_VISIBLE_TIME - FADE_OUT_TIME,
									  MAX_VISIBLE_TIME, 1.f, 0.f);
	}

	F32 image_aspect =
		(F32)mImagep->getFullWidth() / (F32)mImagep->getFullHeight();
	LLVector3 x_scale = image_aspect * (F32)gViewerWindowp->getWindowHeight() *
						mScale * scale_factor * x_pixel_vec;
	LLVector3 y_scale = (F32)gViewerWindowp->getWindowHeight() * mScale *
						scale_factor * y_pixel_vec;

	LLVector3 lower_left = icon_position - x_scale * 0.5f;
	LLVector3 lower_right = icon_position + x_scale * 0.5f;
	LLVector3 upper_left = icon_position - x_scale * 0.5f + y_scale;
	LLVector3 upper_right = icon_position + x_scale * 0.5f + y_scale;

	LLColor4 icon_color = LLColor4::white;
	icon_color.mV[VALPHA] = alpha_factor;
	gGL.color4fv(icon_color.mV);
	gGL.getTexUnit(0)->bind(mImagep);

	gGL.begin(LLRender::TRIANGLES);
	{
		gGL.texCoord2f(0.f, 1.f);
		gGL.vertex3fv(upper_left.mV);
		gGL.texCoord2f(0.f, 0.f);
		gGL.vertex3fv(lower_left.mV);
		gGL.texCoord2f(1.f, 0.f);
		gGL.vertex3fv(lower_right.mV);
		gGL.texCoord2f(0.f, 1.f);
		gGL.vertex3fv(upper_left.mV);
		gGL.texCoord2f(1.f, 0.f);
		gGL.vertex3fv(lower_right.mV);
		gGL.texCoord2f(1.f, 1.f);
		gGL.vertex3fv(upper_right.mV);
	}
	gGL.end();
}

void LLHUDIcon::setImage(LLViewerTexture* imagep)
{
	mImagep = imagep;
	mImagep->setAddressMode(LLTexUnit::TAM_CLAMP);
}

//virtual
void LLHUDIcon::markDead()
{
	if (mSourceObject)
	{
		mSourceObject->clearIcon();
	}
	LLHUDObject::markDead();
}

bool LLHUDIcon::lineSegmentIntersect(const LLVector4a& start,
									 const LLVector4a& end,
									 LLVector4a* intersection)
{
	if (mHidden)
	{
		return false;
	}

	if (mSourceObject.isNull() || mImagep.isNull())
	{
		markDead();
		return false;
	}

	LLVector3 obj_position = mSourceObject->getRenderPosition();

	// put icon above object, and in front
	// RN: don't use drawable radius, it's fricking HUGE
	LLVector3 icon_relative_pos = (gViewerCamera.getUpAxis() *
								   ~mSourceObject->getRenderRotation());
	icon_relative_pos.abs();

	F32 distance_scale =
		llmin(mSourceObject->getScale().mV[VX] / icon_relative_pos.mV[VX],
			  mSourceObject->getScale().mV[VY] / icon_relative_pos.mV[VY],
			  mSourceObject->getScale().mV[VZ] / icon_relative_pos.mV[VZ]);
	F32 up_distance = 0.5f * distance_scale;
	LLVector3 icon_position =
		obj_position + 1.2f * up_distance * gViewerCamera.getUpAxis();

	LLVector3 icon_to_cam = gViewerCamera.getOrigin() - icon_position;
	icon_to_cam.normalize();

	icon_position +=
		icon_to_cam * mSourceObject->mDrawable->getRadius() * 1.1f;

	mDistance = dist_vec(icon_position, gViewerCamera.getOrigin());

	LLVector3 x_pixel_vec;
	LLVector3 y_pixel_vec;

	gViewerCamera.getPixelVectors(icon_position, y_pixel_vec, x_pixel_vec);

	F32 scale_factor = 1.f;
	if (mAnimTimer.getElapsedTimeF32() < ANIM_TIME)
	{
		scale_factor =
			llmax(0.f,
				  calc_bouncy_animation(mAnimTimer.getElapsedTimeF32() /
										ANIM_TIME));
	}

	F32 time_elapsed = mLifeTimer.getElapsedTimeF32();
	if (time_elapsed > MAX_VISIBLE_TIME)
	{
		markDead();
		return false;
	}

	F32 image_aspect =
		(F32)mImagep->getFullWidth() / (F32)mImagep->getFullHeight();
	LLVector3 x_scale = image_aspect * (F32)gViewerWindowp->getWindowHeight() *
						mScale * scale_factor * x_pixel_vec;
	LLVector3 y_scale = (F32)gViewerWindowp->getWindowHeight() * mScale *
						scale_factor * y_pixel_vec;

	LLVector4a x_scalea;
	LLVector4a icon_positiona;
	LLVector4a y_scalea;

	x_scalea.load3(x_scale.mV);
	x_scalea.mul(0.5f);
	y_scalea.load3(y_scale.mV);

	icon_positiona.load3(icon_position.mV);

	LLVector4a lower_left;
	lower_left.setSub(icon_positiona, x_scalea);
	LLVector4a lower_right;
	lower_right.setAdd(icon_positiona, x_scalea);
	LLVector4a upper_left;
	upper_left.setAdd(lower_left, y_scalea);
	LLVector4a upper_right;
	upper_right.setAdd(lower_right, y_scalea);

	LLVector4a dir;
	dir.setSub(end, start);

	F32 a,b,t;

	if (LLTriangleRayIntersect(upper_right, upper_left,
							   lower_right, start, dir, a, b, t) ||
		LLTriangleRayIntersect(upper_left, lower_left,
							   lower_right, start, dir, a, b, t))
	{
		if (intersection)
		{
			dir.mul(t);
			intersection->setAdd(start, dir);
		}
		return true;
	}

	return false;
}

void LLHUDIcon::fireClickedCallback(const LLUUID& id)
{
	if (mClickedCallback)
	{
		mClickedCallback(id);
	}
}

//static
LLHUDIcon* LLHUDIcon::lineSegmentIntersectAll(const LLVector4a& start,
											  const LLVector4a& end,
											  LLVector4a* intersection)
{
	icon_instance_t::iterator icon_it;
	icon_instance_t::iterator icons_end = sIconInstances.end();

	LLVector4a local_end = end;
	LLVector4a position;

	LLHUDIcon* ret = NULL;
	for (icon_it = sIconInstances.begin(); icon_it != icons_end; ++icon_it)
	{
		LLHUDIcon* icon = *icon_it;
		if (icon->lineSegmentIntersect(start, local_end, &position))
		{
			ret = icon;
			if (intersection)
			{
				*intersection = position;
			}
			local_end = position;
		}
	}

	return ret;
}

//static
void LLHUDIcon::cleanupDeadIcons()
{
	icon_instance_t::iterator icon_it;
	icon_instance_t::iterator end = sIconInstances.end();

	icon_instance_t icons_to_erase;
	for (icon_it = sIconInstances.begin(); icon_it != end; ++icon_it)
	{
		if ((*icon_it)->mDead)
		{
			icons_to_erase.push_back(*icon_it);
		}
	}

	end = icons_to_erase.end();
	for (icon_it = icons_to_erase.begin(); icon_it != end; ++icon_it)
	{
		icon_instance_t::iterator found_it = std::find(sIconInstances.begin(),
													   sIconInstances.end(),
													   *icon_it);
		if (found_it != sIconInstances.end())
		{
			sIconInstances.erase(found_it);
		}
	}
}
