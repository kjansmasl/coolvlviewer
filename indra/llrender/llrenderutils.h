/**
 * @file llrenderutils.h
 * @brief Utility 2D and 3D GL rendering functions declarations.
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

#ifndef LL_RENDERUTILS_H
#define LL_RENDERUTILS_H

#include <vector>

#include "llglslshader.h"
#include "llpreprocessor.h"
#include "llrect.h"
#include "llvector3.h"

class LLColor4;
class LLGLTexture;
class LLVector2;

// UI colors, defined in indra/llui/llui.cpp
extern const LLColor4 UI_VERTEX_COLOR;

///////////////////////////////////////////////////////////////////////////////
// 2D rendering functions

void gl_state_for_2d(S32 width, S32 height);

void gl_line_2d(S32 x1, S32 y1, S32 x2, S32 y2);
void gl_line_2d(S32 x1, S32 y1, S32 x2, S32 y2, const LLColor4& color);

void gl_triangle_2d(S32 x1, S32 y1, S32 x2, S32 y2, S32 x3, S32 y3,
					const LLColor4& color, bool filled);

void gl_draw_x(const LLRect& rect, const LLColor4& color);

void gl_rect_2d_simple(S32 width, S32 height);

void gl_rect_2d(S32 left, S32 top, S32 right, S32 bottom, bool filled = true);

LL_INLINE void gl_rect_2d(const LLRect& rect, bool filled = true)
{
	gl_rect_2d(rect.mLeft, rect.mTop, rect.mRight, rect.mBottom, filled);
}

LL_INLINE void gl_rect_2d(S32 left, S32 top, S32 right, S32 bottom,
						  const LLColor4& color, bool filled = true)
{
	gGL.color4fv(color.mV);
	gl_rect_2d(left, top, right, bottom, filled);
}

LL_INLINE void gl_rect_2d(const LLRect& rect, const LLColor4& color,
						  bool filled = true)
{
	gGL.color4fv(color.mV);
	gl_rect_2d(rect.mLeft, rect.mTop, rect.mRight, rect.mBottom, filled);
}

void gl_drop_shadow(S32 left, S32 top, S32 right, S32 bottom,
					const LLColor4& start_color, S32 lines);

void gl_circle_2d(F32 x, F32 y, F32 radius, S32 steps, bool filled);

void gl_arc_2d(F32 center_x, F32 center_y, F32 radius, S32 steps, bool filled,
			   F32 start_angle, F32 end_angle);

void gl_deep_circle(F32 radius, F32 depth);

void gl_ring(F32 radius, F32 width, const LLColor4& center_color,
			 const LLColor4& side_color, S32 steps, bool render_center);

void gl_corners_2d(S32 left, S32 top, S32 right, S32 bottom, S32 length,
				   F32 max_frac);

void gl_washer_2d(F32 outer_radius, F32 inner_radius, S32 steps,
				  const LLColor4& inner_color, const LLColor4& outer_color);

void gl_washer_segment_2d(F32 outer_radius, F32 inner_radius,
						  F32 start_radians, F32 end_radians, S32 steps,
						  const LLColor4& inner_color,
						  const LLColor4& outer_color);

void gl_washer_spokes_2d(F32 outer_radius, F32 inner_radius, S32 count,
						 const LLColor4& inner_color,
						 const LLColor4& outer_color);

void gl_draw_image(S32 x, S32 y, LLGLTexture* texp,
				   const LLColor4& color = UI_VERTEX_COLOR,
				   const LLRectf& uv_rect = LLRectf(0.f, 1.f, 1.f, 0.f));

void gl_draw_scaled_image(S32 x, S32 y, S32 width, S32 height,
						  LLGLTexture* texp,
						  const LLColor4& color = UI_VERTEX_COLOR,
						  const LLRectf& uv_rect = LLRectf(0.f, 1.f, 1.f, 0.f));

void gl_draw_rotated_image(S32 x, S32 y, F32 degrees, LLGLTexture* texp,
						   const LLColor4& color = UI_VERTEX_COLOR,
						   const LLRectf& uv_rect = LLRectf(0.f, 1.f, 1.f, 0.f));

void gl_draw_scaled_rotated_image(S32 x, S32 y, S32 width, S32 height,
								  F32 degrees, LLGLTexture* texp,
								  const LLColor4& color = UI_VERTEX_COLOR,
								  const LLRectf& uv_rect = LLRectf(0.f, 1.f, 1.f, 0.f));

void gl_draw_scaled_image_with_border(S32 x, S32 y, S32 border_width,
									  S32 border_height, S32 width, S32 height,
									  LLGLTexture* texp, const LLColor4& color,
									  bool solid_color = false,
									  const LLRectf& uv_rect = LLRectf(0.f, 1.f, 1.f, 0.f));

void gl_draw_scaled_image_with_border(S32 x, S32 y, S32 width, S32 height,
									  LLGLTexture* texp, const LLColor4& color,
									  bool solid_color = false,
									  const LLRectf& uv_rect = LLRectf(0.f, 1.f, 1.f, 0.f),
									  const LLRectf& scale_rect = LLRectf(0.f, 1.f, 1.f, 0.f));

void gl_rect_2d_simple_tex(S32 width, S32 height);

// segmented rectangles

/*
   TL |______TOP_________| TR
     /|                  |\
   _/_|__________________|_\_
   L| |    MIDDLE        | |R
   _|_|__________________|_|_
    \ |    BOTTOM        | /
   BL\|__________________|/ BR
      |                  |
*/

typedef enum e_rounded_edge
{
	ROUNDED_RECT_LEFT	= 0x1,
	ROUNDED_RECT_TOP	= 0x2,
	ROUNDED_RECT_RIGHT	= 0x4,
	ROUNDED_RECT_BOTTOM	= 0x8,
	ROUNDED_RECT_ALL	= 0xf
} ERoundedEdge;

void gl_segmented_rect_2d_tex(S32 left, S32 top, S32 right, S32 bottom,
							  S32 texture_width, S32 texture_height,
							  S32 border_size, U32 edges = ROUNDED_RECT_ALL);

void gl_segmented_rect_2d_fragment_tex(S32 left, S32 top, S32 right, S32 bot,
									   S32 texture_width, S32 texture_height,
									   S32 border_size, F32 start_fragment,
									   F32 end_fragment,
									   U32 edges = ROUNDED_RECT_ALL);

///////////////////////////////////////////////////////////////////////////////
// 3D rendering functions

void gl_segmented_rect_3d_tex(const LLVector2& border_scale,
							  const LLVector3& border_width,
							  const LLVector3& border_height,
							  const LLVector3& width_vec,
							  const LLVector3& height_vec,
							  U32 edges = ROUNDED_RECT_ALL);

LL_INLINE void gl_segmented_rect_3d_tex_top(const LLVector2& border_scale,
											const LLVector3& border_width,
											const LLVector3& border_height,
											const LLVector3& width_vec,
											const LLVector3& height_vec)
{
	gl_segmented_rect_3d_tex(border_scale, border_width, border_height,
							 width_vec, height_vec, ROUNDED_RECT_TOP);
}

// Must be preceeded with gGL.begin(LLRender::LINES)
void gl_draw_3d_cross_lines(const LLVector3& center, F32 dx, F32 dy, F32 dz);
void gl_draw_3d_line_cube(F32 width, const LLVector3& center);

// This used to be in llbox.h
class LLBox
{
public:
	void prerender();
	void cleanupGL();

	void renderface(S32 which_face);
	void render();

	LL_INLINE U32 getTriangleCount()		{ return mTriangleCount; }

protected:
	F32		mVertex[8][3];
	U32		mTriangleCount;
};

extern LLBox gBox;

// This used to be in llcylinder.h
class LLCone
{
public:
	void render(S32 sides = 12);
};

extern LLCone gCone;

// This used to be in llrendersphere.h
class LLRenderSphere
{
public:
	void render();						// Render at highest LOD
	void renderGGL();                   // Render using LLRender

private:
	std::vector<std::vector<LLVector3> > mSpherePoints;
};

extern LLRenderSphere gSphere;

#endif	// LL_RENDERUTILS_H
