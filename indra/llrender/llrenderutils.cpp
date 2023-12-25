/**
 * @file llrenderutils.cpp
 * @brief Utility 2D and 3D GL rendering functions implementation
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

#include "linden_common.h"

#include "llrenderutils.h"

#include "llcolor4.h"
#include "llgl.h"
#include "llgltexture.h"
#include "llmatrix3.h"
#include "llrender.h"
#include "llvector2.h"
#include "llvector3.h"

// Puts GL into 2D drawing mode by turning off lighting, setting to an
// orthographic projection, etc.
void gl_state_for_2d(S32 width, S32 height)
{
	F32 window_width = (F32)width;		//gViewerWindowp->getWindowWidth();
	F32 window_height = (F32)height;	//gViewerWindowp->getWindowHeight();

	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.loadIdentity();
	gGL.ortho(0.0f, llmax(window_width, 1.f), 0.0f, llmax(window_height, 1.f),
			  -1.0f, 1.0f);
	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.loadIdentity();
}

void gl_draw_x(const LLRect& rect, const LLColor4& color)
{
	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

	gGL.color4fv(color.mV);

	gGL.begin(LLRender::LINES);
		gGL.vertex2i(rect.mLeft, rect.mTop);
		gGL.vertex2i(rect.mRight, rect.mBottom);
		gGL.vertex2i(rect.mLeft, rect.mBottom);
		gGL.vertex2i(rect.mRight, rect.mTop);
	gGL.end();
}

void gl_rect_2d(S32 left, S32 top, S32 right, S32 bottom, bool filled)
{
	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

	// Counterclockwise triangles face the camera
	if (filled)
	{
		gGL.begin(LLRender::TRIANGLES);
			gGL.vertex2i(left, top);
			gGL.vertex2i(left, bottom);
			gGL.vertex2i(right, top);
			gGL.vertex2i(right, top);
			gGL.vertex2i(left, bottom);
			gGL.vertex2i(right, bottom);
		gGL.end();
	}
	else
	{
		gGL.begin(LLRender::LINE_STRIP);
			gGL.vertex2i(left, top);
			gGL.vertex2i(left, bottom);
			gGL.vertex2i(right, bottom);
			gGL.vertex2i(right, top);
			gGL.vertex2i(left, top);
		gGL.end();
	}
}

// Given a rectangle on the screen, draws a drop shadow _outside_ the right and
// bottom edges of it.  Along the right it has width "lines" and along the
// bottom it has height "lines".
void gl_drop_shadow(S32 left, S32 top, S32 right, S32 bottom,
					const LLColor4& start_color, S32 lines)
{
	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

	LLColor4 end_color = start_color;
	end_color.mV[VALPHA] = 0.f;

	gGL.begin(LLRender::TRIANGLES);

	// Right edge, CCW faces screen
	gGL.color4fv(start_color.mV);
	// HACK: Overlap with the rectangle by a single pixel.
	// (--right; ++bottom; ++lines;)
	gGL.vertex2i(--right, top - ++lines);
	gGL.vertex2i(right, ++bottom);
	gGL.color4fv(end_color.mV);
	gGL.vertex2i(right + lines, bottom);
	gGL.vertex2i(right + lines, bottom);
	gGL.vertex2i(right + lines, top - lines);
	gGL.color4fv(start_color.mV);
	gGL.vertex2i(right, top - lines);

	// Bottom edge, CCW faces screen
	gGL.vertex2i(left + lines, bottom);
	gGL.color4fv(end_color.mV);
	gGL.vertex2i(left + lines, bottom - lines);
	gGL.vertex2i(right, bottom - lines);
	gGL.vertex2i(right, bottom - lines);
	gGL.color4fv(start_color.mV);
	gGL.vertex2i(right, bottom);
	gGL.vertex2i(left + lines, bottom);

	// bottom left Corner
	gGL.color4fv(start_color.mV);
	gGL.vertex2i(left + lines, bottom);
	gGL.color4fv(end_color.mV);
	gGL.vertex2i(left, bottom);
	gGL.vertex2i(left + lines, bottom - lines);
	gGL.vertex2i(left + lines, bottom - lines);
	gGL.vertex2i(left, bottom);
	// make the bottom left corner not sharp
	gGL.vertex2i(left + 1, bottom - lines + 1);

	// bottom right corner
	gGL.color4fv(start_color.mV);
	gGL.vertex2i(right, bottom);
	gGL.color4fv(end_color.mV);
	gGL.vertex2i(right, bottom - lines);
	gGL.vertex2i(right + lines, bottom);
	gGL.vertex2i(right + lines, bottom);
	gGL.vertex2i(right, bottom - lines);
	// make the rightmost corner not sharp
	gGL.vertex2i(right + lines - 1, bottom - lines + 1);

	// top right corner
	gGL.color4fv(start_color.mV);
	gGL.vertex2i(right, top - lines);
	gGL.color4fv(end_color.mV);
	gGL.vertex2i(right + lines, top - lines);
	gGL.vertex2i(right, top);
	gGL.vertex2i(right, top);
	gGL.vertex2i(right + lines, top - lines);
	// make the corner not sharp
	gGL.vertex2i(right + lines - 1, top - 1);

	gGL.end();
}

void gl_line_2d(S32 x1, S32 y1, S32 x2, S32 y2)
{
	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

	gGL.begin(LLRender::LINES);
		gGL.vertex2i(x1, y1);
		gGL.vertex2i(x2, y2);
	gGL.end();
}

void gl_line_2d(S32 x1, S32 y1, S32 x2, S32 y2, const LLColor4& color)
{
	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

	gGL.color4fv(color.mV);

	gGL.begin(LLRender::LINES);
		gGL.vertex2i(x1, y1);
		gGL.vertex2i(x2, y2);
	gGL.end();
}

void gl_triangle_2d(S32 x1, S32 y1, S32 x2, S32 y2, S32 x3, S32 y3,
					const LLColor4& color, bool filled)
{
	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

	gGL.color4fv(color.mV);

	gGL.begin(filled ? LLRender::TRIANGLES : LLRender::LINE_LOOP);
	gGL.vertex2i(x1, y1);
	gGL.vertex2i(x2, y2);
	gGL.vertex2i(x3, y3);
	gGL.end();
}

void gl_corners_2d(S32 left, S32 top, S32 right, S32 bottom, S32 length,
				   F32 max_frac)
{
	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

	length = llmin((S32)(max_frac * (right - left)), length);
	length = llmin((S32)(max_frac * (top - bottom)), length);
	gGL.begin(LLRender::LINES);
	gGL.vertex2i(left, top);
	gGL.vertex2i(left + length, top);

	gGL.vertex2i(left, top);
	gGL.vertex2i(left, top - length);

	gGL.vertex2i(left, bottom);
	gGL.vertex2i(left + length, bottom);

	gGL.vertex2i(left, bottom);
	gGL.vertex2i(left, bottom + length);

	gGL.vertex2i(right, top);
	gGL.vertex2i(right - length, top);

	gGL.vertex2i(right, top);
	gGL.vertex2i(right, top - length);

	gGL.vertex2i(right, bottom);
	gGL.vertex2i(right - length, bottom);

	gGL.vertex2i(right, bottom);
	gGL.vertex2i(right, bottom + length);
	gGL.end();
}

void gl_draw_image(S32 x, S32 y, LLGLTexture* texp, const LLColor4& color,
				   const LLRectf& uv_rect)
{
	if (!texp)
	{
		llwarns << "NULL image pointer, aborting function" << llendl;
		return;
	}
	gl_draw_scaled_rotated_image(x, y, texp->getWidth(0), texp->getHeight(0),
								 0.f, texp, color, uv_rect);
}

void gl_draw_scaled_image(S32 x, S32 y, S32 width, S32 height,
						  LLGLTexture* texp, const LLColor4& color,
						  const LLRectf& uv_rect)
{
	if (!texp)
	{
		llwarns << "NULL image pointer, aborting function" << llendl;
		return;
	}
	gl_draw_scaled_rotated_image(x, y, width, height, 0.f, texp, color,
								 uv_rect);
}

void gl_draw_scaled_image_with_border(S32 x, S32 y, S32 border_width,
									  S32 border_height, S32 width, S32 height,
									  LLGLTexture* texp,
									  const LLColor4& color, bool solid_color,
									  const LLRectf& uv_rect)
{
	if (!texp)
	{
		llwarns << "NULL image pointer, aborting function" << llendl;
		return;
	}

	// scale screen size of borders down
	F32 border_width_fraction = (F32)border_width / (F32)texp->getWidth(0);
	F32 border_height_fraction = (F32)border_height / (F32)texp->getHeight(0);

	LLRectf scale_rect(border_width_fraction, 1.f - border_height_fraction,
					   1.f - border_width_fraction, border_height_fraction);
	gl_draw_scaled_image_with_border(x, y, width, height, texp, color,
									 solid_color, uv_rect, scale_rect);
}

void gl_draw_scaled_image_with_border(S32 x, S32 y, S32 width, S32 height,
									  LLGLTexture* texp,
									  const LLColor4& color, bool solid_color,
									  const LLRectf& uv_rect,
									  const LLRectf& scale_rect)
{
	if (!texp)
	{
		llwarns << "NULL image pointer, aborting function" << llendl;
		return;
	}

	LLTexUnit* unit0 = gGL.getTexUnit(0);

	if (solid_color)
	{
		gSolidColorProgram.bind();
	}

	// Add in offset of current image to current UI translation
	LLVector3 ui_scale = gGL.getUIScale();
	LLVector3 ui_translation =
		(gGL.getUITranslation() + LLVector3(x, y, 0.f)).scaledVec(ui_scale);

	F32 uv_width = uv_rect.getWidth();
	F32 uv_height = uv_rect.getHeight();

	// Shrink scaling region to be proportional to clipped image region
	LLRectf uv_center_rect(uv_rect.mLeft + scale_rect.mLeft * uv_width,
						   uv_rect.mBottom + scale_rect.mTop * uv_height,
						   uv_rect.mLeft + scale_rect.mRight * uv_width,
						   uv_rect.mBottom + scale_rect.mBottom * uv_height);

	F32 image_width = texp->getWidth(0);
	F32 image_height = texp->getHeight(0);

	S32 image_natural_width = ll_roundp(image_width * uv_width);
	S32 image_natural_height = ll_roundp(image_height * uv_height);

	LLRectf draw_center_rect(uv_center_rect.mLeft * image_width,
							 uv_center_rect.mTop * image_height,
							 uv_center_rect.mRight * image_width,
							 uv_center_rect.mBottom * image_height);

	// Scale fixed region of image to drawn region
	draw_center_rect.mRight += width - image_natural_width;
	draw_center_rect.mTop += height - image_natural_height;

	S32 border_shrink_width = llmax(0.f, draw_center_rect.mLeft - draw_center_rect.mRight);
	S32 border_shrink_height = llmax(0.f, draw_center_rect.mBottom - draw_center_rect.mTop);

	F32 shrink_width_ratio =
		scale_rect.getWidth() == 1.f ? 0.f
									 : border_shrink_width /
									   ((F32)image_natural_width *
										(1.f - scale_rect.getWidth()));
	F32 shrink_height_ratio =
		scale_rect.getHeight() == 1.f ? 0.f
									  : border_shrink_height /
										((F32)image_natural_height *
										 (1.f - scale_rect.getHeight()));

	F32 border_shrink_scale =
		1.f - llmax(shrink_width_ratio, shrink_height_ratio);
	draw_center_rect.mLeft *= border_shrink_scale;
	draw_center_rect.mTop = ll_round(lerp((F32)height,
										  (F32)draw_center_rect.mTop,
										  border_shrink_scale));
	draw_center_rect.mRight = ll_round(lerp((F32)width,
											(F32)draw_center_rect.mRight,
											border_shrink_scale));
	draw_center_rect.mBottom *= border_shrink_scale;

	draw_center_rect.mLeft = ll_round(ui_translation.mV[VX] +
									  (F32)draw_center_rect.mLeft *
									   ui_scale.mV[VX]);
	draw_center_rect.mTop = ll_round(ui_translation.mV[VY] +
									 (F32)draw_center_rect.mTop *
									 ui_scale.mV[VY]);
	draw_center_rect.mRight = ll_round(ui_translation.mV[VX] +
									   (F32)draw_center_rect.mRight *
									   ui_scale.mV[VX]);
	draw_center_rect.mBottom = ll_round(ui_translation.mV[VY] +
										(F32)draw_center_rect.mBottom *
										ui_scale.mV[VY]);

	LLRectf draw_outer_rect(ui_translation.mV[VX],
							ui_translation.mV[VY] + height * ui_scale.mV[VY],
							ui_translation.mV[VX] + width * ui_scale.mV[VX],
							ui_translation.mV[VY]);
	LLGLSUIDefault gls_ui;
	unit0->bind(texp);
	gGL.color4fv(color.mV);

	constexpr S32 NUM_VERTICES = 9 * 6; // 9 quads turned into triangles
	LLVector2 uv[NUM_VERTICES];
	LLVector3 pos[NUM_VERTICES];

	gGL.begin(LLRender::TRIANGLES);
	{
		S32 index = 0;

		// draw bottom left
		uv[index] = LLVector2(uv_rect.mLeft, uv_rect.mBottom);
		pos[index++].set(draw_outer_rect.mLeft, draw_outer_rect.mBottom, 0.f);

		uv[index] = LLVector2(uv_center_rect.mLeft, uv_rect.mBottom);
		pos[index++].set(draw_center_rect.mLeft, draw_outer_rect.mBottom, 0.f);

		uv[index] = LLVector2(uv_center_rect.mLeft, uv_center_rect.mBottom);
		pos[index++].set(draw_center_rect.mLeft, draw_center_rect.mBottom, 0.f);

		uv[index] = uv[index - 3];
		pos[index] = pos[index - 3];
		++index;

		uv[index] = uv[index - 2];
		pos[index] = pos[index - 2];
		++index;

		uv[index] = LLVector2(uv_rect.mLeft, uv_center_rect.mBottom);
		pos[index++].set(draw_outer_rect.mLeft, draw_center_rect.mBottom, 0.f);

		// draw bottom middle
		uv[index] = LLVector2(uv_center_rect.mLeft, uv_rect.mBottom);
		pos[index++].set(draw_center_rect.mLeft, draw_outer_rect.mBottom, 0.f);

		uv[index] = LLVector2(uv_center_rect.mRight, uv_rect.mBottom);
		pos[index++].set(draw_center_rect.mRight, draw_outer_rect.mBottom, 0.f);

		uv[index] = LLVector2(uv_center_rect.mRight, uv_center_rect.mBottom);
		pos[index++].set(draw_center_rect.mRight, draw_center_rect.mBottom, 0.f);

		uv[index] = uv[index - 3];
		pos[index] = pos[index - 3];
		++index;

		uv[index] = uv[index - 2];
		pos[index] = pos[index - 2];
		++index;

		uv[index] = LLVector2(uv_center_rect.mLeft, uv_center_rect.mBottom);
		pos[index++].set(draw_center_rect.mLeft, draw_center_rect.mBottom, 0.f);

		// draw bottom right
		uv[index] = LLVector2(uv_center_rect.mRight, uv_rect.mBottom);
		pos[index++].set(draw_center_rect.mRight, draw_outer_rect.mBottom, 0.f);

		uv[index] = LLVector2(uv_rect.mRight, uv_rect.mBottom);
		pos[index++].set(draw_outer_rect.mRight, draw_outer_rect.mBottom, 0.f);

		uv[index] = LLVector2(uv_rect.mRight, uv_center_rect.mBottom);
		pos[index++].set(draw_outer_rect.mRight, draw_center_rect.mBottom, 0.f);

		uv[index] = uv[index - 3];
		pos[index] = pos[index - 3];
		++index;

		uv[index] = uv[index - 2];
		pos[index] = pos[index - 2];
		++index;

		uv[index] = LLVector2(uv_center_rect.mRight, uv_center_rect.mBottom);
		pos[index++].set(draw_center_rect.mRight, draw_center_rect.mBottom, 0.f);

		// draw left 
		uv[index] = LLVector2(uv_rect.mLeft, uv_center_rect.mBottom);
		pos[index++].set(draw_outer_rect.mLeft, draw_center_rect.mBottom, 0.f);

		uv[index] = LLVector2(uv_center_rect.mLeft, uv_center_rect.mBottom);
		pos[index++].set(draw_center_rect.mLeft, draw_center_rect.mBottom, 0.f);

		uv[index] = LLVector2(uv_center_rect.mLeft, uv_center_rect.mTop);
		pos[index++].set(draw_center_rect.mLeft, draw_center_rect.mTop, 0.f);

		uv[index] = uv[index - 3];
		pos[index] = pos[index - 3];
		++index;

		uv[index] = uv[index - 2];
		pos[index] = pos[index - 2];
		++index;

		uv[index] = LLVector2(uv_rect.mLeft, uv_center_rect.mTop);
		pos[index++].set(draw_outer_rect.mLeft, draw_center_rect.mTop, 0.f);

		// draw middle
		uv[index] = LLVector2(uv_center_rect.mLeft, uv_center_rect.mBottom);
		pos[index++].set(draw_center_rect.mLeft, draw_center_rect.mBottom, 0.f);

		uv[index] = LLVector2(uv_center_rect.mRight, uv_center_rect.mBottom);
		pos[index++].set(draw_center_rect.mRight, draw_center_rect.mBottom, 0.f);

		uv[index] = LLVector2(uv_center_rect.mRight, uv_center_rect.mTop);
		pos[index++].set(draw_center_rect.mRight, draw_center_rect.mTop, 0.f);

		uv[index] = uv[index - 3];
		pos[index] = pos[index - 3];
		++index;

		uv[index] = uv[index - 2];
		pos[index] = pos[index - 2];
		++index;

		uv[index] = LLVector2(uv_center_rect.mLeft, uv_center_rect.mTop);
		pos[index++].set(draw_center_rect.mLeft, draw_center_rect.mTop, 0.f);

		// draw right 
		uv[index] = LLVector2(uv_center_rect.mRight, uv_center_rect.mBottom);
		pos[index++].set(draw_center_rect.mRight, draw_center_rect.mBottom, 0.f);

		uv[index] = LLVector2(uv_rect.mRight, uv_center_rect.mBottom);
		pos[index++].set(draw_outer_rect.mRight, draw_center_rect.mBottom, 0.f);

		uv[index] = LLVector2(uv_rect.mRight, uv_center_rect.mTop);
		pos[index++].set(draw_outer_rect.mRight, draw_center_rect.mTop, 0.f);

		uv[index] = uv[index - 3];
		pos[index] = pos[index - 3];
		++index;

		uv[index] = uv[index - 2];
		pos[index] = pos[index - 2];
		++index;

		uv[index] = LLVector2(uv_center_rect.mRight, uv_center_rect.mTop);
		pos[index++].set(draw_center_rect.mRight, draw_center_rect.mTop, 0.f);

		// draw top left
		uv[index] = LLVector2(uv_rect.mLeft, uv_center_rect.mTop);
		pos[index++].set(draw_outer_rect.mLeft, draw_center_rect.mTop, 0.f);

		uv[index] = LLVector2(uv_center_rect.mLeft, uv_center_rect.mTop);
		pos[index++].set(draw_center_rect.mLeft, draw_center_rect.mTop, 0.f);

		uv[index] = LLVector2(uv_center_rect.mLeft, uv_rect.mTop);
		pos[index++].set(draw_center_rect.mLeft, draw_outer_rect.mTop, 0.f);

		uv[index] = uv[index - 3];
		pos[index] = pos[index - 3];
		++index;

		uv[index] = uv[index - 2];
		pos[index] = pos[index - 2];
		++index;

		uv[index] = LLVector2(uv_rect.mLeft, uv_rect.mTop);
		pos[index++].set(draw_outer_rect.mLeft, draw_outer_rect.mTop, 0.f);

		// draw top middle
		uv[index] = LLVector2(uv_center_rect.mLeft, uv_center_rect.mTop);
		pos[index++].set(draw_center_rect.mLeft, draw_center_rect.mTop, 0.f);

		uv[index] = LLVector2(uv_center_rect.mRight, uv_center_rect.mTop);
		pos[index++].set(draw_center_rect.mRight, draw_center_rect.mTop, 0.f);

		uv[index] = LLVector2(uv_center_rect.mRight, uv_rect.mTop);
		pos[index++].set(draw_center_rect.mRight, draw_outer_rect.mTop, 0.f);

		uv[index] = uv[index - 3];
		pos[index] = pos[index - 3];
		++index;

		uv[index] = uv[index - 2];
		pos[index] = pos[index - 2];
		++index;

		uv[index] = LLVector2(uv_center_rect.mLeft, uv_rect.mTop);
		pos[index++].set(draw_center_rect.mLeft, draw_outer_rect.mTop, 0.f);

		// draw top right
		uv[index] = LLVector2(uv_center_rect.mRight, uv_center_rect.mTop);
		pos[index++].set(draw_center_rect.mRight, draw_center_rect.mTop, 0.f);

		uv[index] = LLVector2(uv_rect.mRight, uv_center_rect.mTop);
		pos[index++].set(draw_outer_rect.mRight, draw_center_rect.mTop, 0.f);

		uv[index] = LLVector2(uv_rect.mRight, uv_rect.mTop);
		pos[index++].set(draw_outer_rect.mRight, draw_outer_rect.mTop, 0.f);

		uv[index] = uv[index - 3];
		pos[index] = pos[index - 3];
		++index;

		uv[index] = uv[index - 2];
		pos[index] = pos[index - 2];
		++index;

		uv[index] = LLVector2(uv_center_rect.mRight, uv_rect.mTop);
		pos[index].set(draw_center_rect.mRight, draw_outer_rect.mTop, 0.f);

		gGL.vertexBatchPreTransformed(pos, uv, NUM_VERTICES);
	}
	gGL.end();

	if (solid_color)
	{
		gUIProgram.bind();
	}
}

void gl_draw_rotated_image(S32 x, S32 y, F32 degrees, LLGLTexture* texp,
						   const LLColor4& color, const LLRectf& uv_rect)
{
	gl_draw_scaled_rotated_image(x, y, texp->getWidth(0), texp->getHeight(0),
								 degrees, texp, color, uv_rect);
}

void gl_draw_scaled_rotated_image(S32 x, S32 y, S32 width, S32 height,
								  F32 degrees, LLGLTexture* texp,
								  const LLColor4& color,
								  const LLRectf& uv_rect)
{
	if (!texp)
	{
		llwarns << "NULL image pointer, aborting function" << llendl;
		return;
	}

	LLGLSUIDefault gls_ui;

	LLTexUnit* unit0 = gGL.getTexUnit(0);
	unit0->bind(texp);

	gGL.color4fv(color.mV);

	if (degrees == 0.f)
	{
		constexpr S32 NUM_VERTICES = 6;
		LLVector2 uv[NUM_VERTICES];
		LLVector3 pos[NUM_VERTICES];

		gGL.begin(LLRender::TRIANGLES);
		{
			LLVector3 ui_scale = gGL.getUIScale();
			LLVector3 ui_translation = gGL.getUITranslation();
			ui_translation.mV[VX] += x;
			ui_translation.mV[VY] += y;
			ui_translation.scaleVec(ui_scale);
			S32 index = 0;
			S32 scaled_width = ll_roundp(width * ui_scale.mV[VX]);
			S32 scaled_height = ll_roundp(height * ui_scale.mV[VY]);

			uv[index] = LLVector2(uv_rect.mRight, uv_rect.mTop);
			pos[index++] = LLVector3(ui_translation.mV[VX] + scaled_width,
									 ui_translation.mV[VY] + scaled_height,
									 0.f);

			uv[index] = LLVector2(uv_rect.mLeft, uv_rect.mTop);
			pos[index++] = LLVector3(ui_translation.mV[VX],
									 ui_translation.mV[VY] + scaled_height,
									 0.f);

			uv[index] = LLVector2(uv_rect.mLeft, uv_rect.mBottom);
			pos[index++] = LLVector3(ui_translation.mV[VX],
									 ui_translation.mV[VY], 0.f);

			uv[index] = uv[index - 3];
			pos[index] = pos[index - 3];
			++index;

			uv[index] = uv[index - 2];
			pos[index] = pos[index - 2];
			++index;

			uv[index] = LLVector2(uv_rect.mRight, uv_rect.mBottom);
			pos[index++] = LLVector3(ui_translation.mV[VX] + scaled_width,
									 ui_translation.mV[VY], 0.f);

			gGL.vertexBatchPreTransformed(pos, uv, NUM_VERTICES);
		}
		gGL.end();
	}
	else
	{
		F32 offset_x = F32(width / 2);
		F32 offset_y = F32(height / 2);

		gGL.pushUIMatrix();
		gGL.translateUI((F32)x, (F32)y, 0.f);
		gGL.translateUI(offset_x, offset_y, 0.f);
	
		LLMatrix3 quat(0.f, 0.f, degrees * DEG_TO_RAD);
		
		unit0->bind(texp);

		gGL.color4fv(color.mV);
		
		gGL.begin(LLRender::TRIANGLES);
		{
			LLVector3 v1 = LLVector3(-offset_x, offset_y, 0.f) * quat;
			gGL.texCoord2f(uv_rect.mLeft, uv_rect.mTop);
			gGL.vertex2f(v1.mV[0], v1.mV[1]);

			v1 = LLVector3(-offset_x, -offset_y, 0.f) * quat;
			gGL.texCoord2f(uv_rect.mLeft, uv_rect.mBottom);
			gGL.vertex2f(v1.mV[0], v1.mV[1]);

			LLVector3 v2 = LLVector3(offset_x, offset_y, 0.f) * quat;
			gGL.texCoord2f(uv_rect.mRight, uv_rect.mTop);
			gGL.vertex2f(v2.mV[0], v2.mV[1]);

			gGL.texCoord2f(uv_rect.mRight, uv_rect.mTop);
			gGL.vertex2f(v2.mV[0], v2.mV[1]);

			gGL.texCoord2f(uv_rect.mLeft, uv_rect.mBottom);
			gGL.vertex2f(v1.mV[0], v1.mV[1]);

			v1 = LLVector3(offset_x, -offset_y, 0.f) * quat;
			gGL.texCoord2f(uv_rect.mRight, uv_rect.mBottom);
			gGL.vertex2f(v1.mV[0], v1.mV[1]);
		}
		gGL.end();
		gGL.popUIMatrix();
	}
}

void gl_arc_2d(F32 center_x, F32 center_y, F32 radius, S32 steps, bool filled,
			   F32 start_angle, F32 end_angle)
{
	if (end_angle < start_angle)
	{
		end_angle += F_TWO_PI;
	}

	gGL.pushUIMatrix();
	gGL.translateUI(center_x, center_y, 0.f);
	{
		// Inexact, but reasonably fast.
		F32 delta = (end_angle - start_angle) / steps;
		F32 sin_delta = sinf(delta);
		F32 cos_delta = cosf(delta);
		F32 x = cosf(start_angle) * radius;
		F32 y = sinf(start_angle) * radius;

		if (filled)
		{
			gGL.begin(LLRender::TRIANGLE_FAN);
			gGL.vertex2f(0.f, 0.f);
			// make sure circle is complete
			++steps;
		}
		else
		{
			gGL.begin(LLRender::LINE_STRIP);
		}

		while (steps--)
		{
			// Successive rotations
			gGL.vertex2f(x, y);
			F32 x_new = x * cos_delta - y * sin_delta;
			y = x * sin_delta +  y * cos_delta;
			x = x_new;
		}
		gGL.end();
	}
	gGL.popUIMatrix();
}

void gl_circle_2d(F32 center_x, F32 center_y, F32 radius, S32 steps,
				  bool filled)
{
	gGL.pushUIMatrix();
	{
		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
		gGL.translateUI(center_x, center_y, 0.f);

		// Inexact, but reasonably fast.
		F32 delta = F_TWO_PI / steps;
		F32 sin_delta = sinf(delta);
		F32 cos_delta = cosf(delta);
		F32 x = radius;
		F32 y = 0.f;

		if (filled)
		{
			gGL.begin(LLRender::TRIANGLE_FAN);
			gGL.vertex2f(0.f, 0.f);
			// make sure circle is complete
			++steps;
		}
		else
		{
			gGL.begin(LLRender::LINE_LOOP);
		}

		while (steps--)
		{
			// Successive rotations
			gGL.vertex2f(x, y);
			F32 x_new = x * cos_delta - y * sin_delta;
			y = x * sin_delta +  y * cos_delta;
			x = x_new;
		}
		gGL.end();
	}
	gGL.popUIMatrix();
}

// Renders a ring with sides (tube shape)
void gl_deep_circle(F32 radius, F32 depth, S32 steps)
{
	F32 x = radius;
	F32 y = 0.f;
	F32 angle_delta = F_TWO_PI / (F32)steps;
	gGL.begin(LLRender::TRIANGLE_STRIP);
	{
		S32 step = steps + 1; // An extra step to close the circle.
		while (step--)
		{
			gGL.vertex3f(x, y, depth);
			gGL.vertex3f(x, y, 0.f);

			F32 x_new = x * cosf(angle_delta) - y * sinf(angle_delta);
			y = x * sinf(angle_delta) +  y * cosf(angle_delta);
			x = x_new;
		}
	}
	gGL.end();
}

void gl_ring(F32 radius, F32 width, const LLColor4& center_color,
			 const LLColor4& side_color, S32 steps, bool render_center)
{
	gGL.pushUIMatrix();
	gGL.translateUI(0.f, 0.f, -width / 2);
	{
		if (render_center)
		{
			gGL.color4fv(center_color.mV);
			gl_deep_circle(radius, width, steps);
		}
		else
		{
			gGL.color4fv(side_color.mV);
			gl_washer_2d(radius, radius - width, steps, side_color, side_color);
			gGL.translatef(0.f, 0.f, width);
			gl_washer_2d(radius - width, radius, steps, side_color, side_color);
		}
	}
	gGL.popUIMatrix();
}

// Draws the area between two concentric circles, like
// a doughnut or washer.
void gl_washer_2d(F32 outer_radius, F32 inner_radius, S32 steps,
				  const LLColor4& inner_color, const LLColor4& outer_color)
{
	const F32 delta = F_TWO_PI / steps;
	const F32 sin_delta = sinf(delta);
	const F32 cos_delta = cosf(delta);

	F32 x1 = outer_radius;
	F32 y1 = 0.f;
	F32 x2 = inner_radius;
	F32 y2 = 0.f;

	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

	gGL.begin(LLRender::TRIANGLE_STRIP);
	{
		++steps; // An extra step to close the circle.
		while (steps--)
		{
			gGL.color4fv(outer_color.mV);
			gGL.vertex2f(x1, y1);
			gGL.color4fv(inner_color.mV);
			gGL.vertex2f(x2, y2);

			F32 x1_new = x1 * cos_delta - y1 * sin_delta;
			y1 = x1 * sin_delta +  y1 * cos_delta;
			x1 = x1_new;

			F32 x2_new = x2 * cos_delta - y2 * sin_delta;
			y2 = x2 * sin_delta +  y2 * cos_delta;
			x2 = x2_new;
		}
	}
	gGL.end();
}

// Draws the area between two concentric circles, like a doughnut or washer.
void gl_washer_segment_2d(F32 outer_radius, F32 inner_radius,
						  F32 start_radians, F32 end_radians,
						  S32 steps, const LLColor4& inner_color,
						  const LLColor4& outer_color)
{
	const F32 delta = (end_radians - start_radians) / steps;
	const F32 sin_delta = sinf(delta);
	const F32 cos_delta = cosf(delta);

	F32 x1 = outer_radius * cosf(start_radians);
	F32 y1 = outer_radius * sinf(start_radians);
	F32 x2 = inner_radius * cosf(start_radians);
	F32 y2 = inner_radius * sinf(start_radians);

	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
	gGL.begin(LLRender::TRIANGLE_STRIP);
	{
		++steps; // An extra step to close the circle.
		while (steps--)
		{
			gGL.color4fv(outer_color.mV);
			gGL.vertex2f(x1, y1);
			gGL.color4fv(inner_color.mV);
			gGL.vertex2f(x2, y2);

			F32 x1_new = x1 * cos_delta - y1 * sin_delta;
			y1 = x1 * sin_delta +  y1 * cos_delta;
			x1 = x1_new;

			F32 x2_new = x2 * cos_delta - y2 * sin_delta;
			y2 = x2 * sin_delta +  y2 * cos_delta;
			x2 = x2_new;
		}
	}
	gGL.end();
}

// Draws spokes around a circle.
void gl_washer_spokes_2d(F32 outer_radius, F32 inner_radius, S32 count,
						 const LLColor4& inner_color,
						 const LLColor4& outer_color)
{
	const F32 delta = F_TWO_PI / count;
	const F32 half_delta = delta * 0.5f;
	const F32 sin_delta = sinf(delta);
	const F32 cos_delta = cosf(delta);

	F32 x1 = outer_radius * cosf(half_delta);
	F32 y1 = outer_radius * sinf(half_delta);
	F32 x2 = inner_radius * cosf(half_delta);
	F32 y2 = inner_radius * sinf(half_delta);

	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

	gGL.begin(LLRender::LINES);
	{
		while (count--)
		{
			gGL.color4fv(outer_color.mV);
			gGL.vertex2f(x1, y1);
			gGL.color4fv(inner_color.mV);
			gGL.vertex2f(x2, y2);

			F32 x1_new = x1 * cos_delta - y1 * sin_delta;
			y1 = x1 * sin_delta +  y1 * cos_delta;
			x1 = x1_new;

			F32 x2_new = x2 * cos_delta - y2 * sin_delta;
			y2 = x2 * sin_delta +  y2 * cos_delta;
			x2 = x2_new;
		}
	}
	gGL.end();
}

void gl_rect_2d_simple_tex(S32 width, S32 height)
{
	gGL.begin(LLRender::TRIANGLES);
	{
		gGL.texCoord2f(1.f, 1.f);
		gGL.vertex2i(width, height);

		gGL.texCoord2f(0.f, 1.f);
		gGL.vertex2i(0, height);

		gGL.texCoord2f(0.f, 0.f);
		gGL.vertex2i(0, 0);

		gGL.texCoord2f(1.f, 1.f);
		gGL.vertex2i(width, height);

		gGL.texCoord2f(0.f, 0.f);
		gGL.vertex2i(0, 0);

		gGL.texCoord2f(1.f, 0.f);
		gGL.vertex2i(width, 0);
	}
	gGL.end();
}

void gl_rect_2d_simple(S32 width, S32 height)
{
	// Important: we MUST draw the triangles counterclockwise so that they
	// "face" the camera (else, the rectangle drawn with gl_rect_2d_simple()
	// won't occlude the UI+world view, such as in the model preview floater,
	// for example).
	gGL.begin(LLRender::TRIANGLES);
	{
		gGL.vertex2i(width, height);
		gGL.vertex2i(0, height);
		gGL.vertex2i(0, 0);
		gGL.vertex2i(width, height);
		gGL.vertex2i(0, 0);
		gGL.vertex2i(width, 0);
	}
	gGL.end();
}

void gl_segmented_rect_2d_tex(S32 left, S32 top, S32 right, S32 bottom,
							  S32 texture_width, S32 texture_height,
							  S32 border_size, U32 edges)
{
	S32 width = abs(right - left);
	S32 height = abs(top - bottom);

	gGL.pushUIMatrix();
	gGL.translateUI((F32)left, (F32)bottom, 0.f);

	LLVector2 border_uv_scale((F32)border_size / (F32)texture_width,
							  (F32)border_size / (F32)texture_height);
	if (border_uv_scale.mV[VX] > 0.5f)
	{
		border_uv_scale *= 0.5f / border_uv_scale.mV[VX];
	}
	if (border_uv_scale.mV[VY] > 0.5f)
	{
		border_uv_scale *= 0.5f / border_uv_scale.mV[VY];
	}

	F32 border_scale = llmin((F32)border_size, (F32)width * 0.5f,
							 (F32)height * 0.5f);
	LLVector2 border_width_left, border_width_right,
			  border_height_bottom, border_height_top;
	if ((edges & (~(U32)ROUNDED_RECT_RIGHT)) != 0)
	{
		border_width_left = LLVector2(border_scale, 0.f);
	}
	if ((edges & (~(U32)ROUNDED_RECT_LEFT)) != 0)
	{
		border_width_right = LLVector2(border_scale, 0.f);
	}
	if ((edges & (~(U32)ROUNDED_RECT_TOP)) != 0)
	{
		border_height_bottom = LLVector2(0.f, border_scale);
	}
	if ((edges & (~(U32)ROUNDED_RECT_BOTTOM)) != 0)
	{
		border_height_top = LLVector2(0.f, border_scale);
	}

	LLVector2 width_vec((F32)width, 0.f);
	LLVector2 height_vec(0.f, (F32)height);

	gGL.begin(LLRender::TRIANGLES);
	{
		// draw bottom left
		gGL.texCoord2f(0.f, 0.f);
		gGL.vertex2f(0.f, 0.f);

		F32 border_uv_scale_x = border_uv_scale.mV[VX];
		F32 border_uv_scale_y = border_uv_scale.mV[VY];

		gGL.texCoord2f(border_uv_scale_x, 0.f);
		gGL.vertex2fv(border_width_left.mV);

		LLVector2 bwl_bhb = border_width_left + border_height_bottom;
		gGL.texCoord2f(border_uv_scale_x, border_uv_scale_y);
		gGL.vertex2fv(bwl_bhb.mV);

		gGL.texCoord2f(0.f, 0.f);
		gGL.vertex2f(0.f, 0.f);

		gGL.texCoord2f(border_uv_scale_x, border_uv_scale_y);
		gGL.vertex2fv(bwl_bhb.mV);

		gGL.texCoord2f(0.f, border_uv_scale_y);
		gGL.vertex2fv(border_height_bottom.mV);

		// draw bottom middle
		gGL.texCoord2f(border_uv_scale_x, 0.f);
		gGL.vertex2fv(border_width_left.mV);

		LLVector2 wv_bwr = width_vec - border_width_right;
		gGL.texCoord2f(1.f - border_uv_scale_x, 0.f);
		gGL.vertex2fv(wv_bwr.mV);

		LLVector2 wv_bwr_bhb = width_vec - border_width_right +
							   border_height_bottom;
		gGL.texCoord2f(1.f - border_uv_scale_x, border_uv_scale_y);
		gGL.vertex2fv(wv_bwr_bhb.mV);

		gGL.texCoord2f(border_uv_scale_x, 0.f);
		gGL.vertex2fv(border_width_left.mV);

		gGL.texCoord2f(1.f - border_uv_scale_x, border_uv_scale_y);
		gGL.vertex2fv(wv_bwr_bhb.mV);

		gGL.texCoord2f(border_uv_scale_x, border_uv_scale_y);
		gGL.vertex2fv(bwl_bhb.mV);

		// draw bottom right
		gGL.texCoord2f(1.f - border_uv_scale_x, 0.f);
		gGL.vertex2fv(wv_bwr.mV);

		gGL.texCoord2f(1.f, 0.f);
		gGL.vertex2fv(width_vec.mV);

		LLVector2 wv_bhb = width_vec + border_height_bottom;
		gGL.texCoord2f(1.f, border_uv_scale_y);
		gGL.vertex2fv(wv_bhb.mV);

		gGL.texCoord2f(1.f - border_uv_scale_x, 0.f);
		gGL.vertex2fv(wv_bwr.mV);

		gGL.texCoord2f(1.f, border_uv_scale_y);
		gGL.vertex2fv(wv_bhb.mV);

		gGL.texCoord2f(1.f - border_uv_scale_x, border_uv_scale_y);
		gGL.vertex2fv(wv_bwr_bhb.mV);

		// draw left
		gGL.texCoord2f(0.f, border_uv_scale_y);
		gGL.vertex2fv(border_height_bottom.mV);

		gGL.texCoord2f(border_uv_scale_x, border_uv_scale_y);
		gGL.vertex2fv(bwl_bhb.mV);

		LLVector2 bwl_hv_bht = border_width_left + height_vec -
							   border_height_top;
		gGL.texCoord2f(border_uv_scale_x, 1.f - border_uv_scale_y);
		gGL.vertex2fv(bwl_hv_bht.mV);

		gGL.texCoord2f(0.f, border_uv_scale_y);
		gGL.vertex2fv(border_height_bottom.mV);

		gGL.texCoord2f(border_uv_scale_x, 1.f - border_uv_scale_y);
		gGL.vertex2fv(bwl_hv_bht.mV);

		LLVector2 hv_bht = height_vec - border_height_top;
		gGL.texCoord2f(0.f, 1.f - border_uv_scale_y);
		gGL.vertex2fv(hv_bht.mV);

		// draw middle
		gGL.texCoord2f(border_uv_scale_x, border_uv_scale_y);
		gGL.vertex2fv(bwl_bhb.mV);

		gGL.texCoord2f(1.f - border_uv_scale_x, border_uv_scale_y);
		gGL.vertex2fv(wv_bwr_bhb.mV);

		LLVector2 wv_bwr_hv_bht = width_vec - border_width_right + height_vec -
								  border_height_top;
		gGL.texCoord2f(1.f - border_uv_scale_x, 1.f - border_uv_scale_y);
		gGL.vertex2fv(wv_bwr_hv_bht.mV);

		gGL.texCoord2f(border_uv_scale_x, border_uv_scale_y);
		gGL.vertex2fv(bwl_bhb.mV);

		gGL.texCoord2f(1.f - border_uv_scale_x, 1.f - border_uv_scale_y);
		gGL.vertex2fv(wv_bwr_hv_bht.mV);

		gGL.texCoord2f(border_uv_scale_x, 1.f - border_uv_scale_y);
		gGL.vertex2fv(bwl_hv_bht.mV);

		// draw right
		gGL.texCoord2f(1.f - border_uv_scale_x, border_uv_scale_y);
		gGL.vertex2fv(wv_bwr_bhb.mV);

		gGL.texCoord2f(1.f, border_uv_scale_y);
		gGL.vertex2fv(wv_bhb.mV);

		LLVector2 wv_hv_bht = width_vec + height_vec - border_height_top;
		gGL.texCoord2f(1.f, 1.f - border_uv_scale_y);
		gGL.vertex2fv(wv_hv_bht.mV);

		gGL.texCoord2f(1.f - border_uv_scale_x, border_uv_scale_y);
		gGL.vertex2fv(wv_bwr_bhb.mV);

		gGL.texCoord2f(1.f, 1.f - border_uv_scale_y);
		gGL.vertex2fv(wv_hv_bht.mV);

		gGL.texCoord2f(1.f - border_uv_scale_x, 1.f - border_uv_scale_y);
		gGL.vertex2fv(wv_bwr_hv_bht.mV);

		// draw top left
		gGL.texCoord2f(0.f, 1.f - border_uv_scale_y);
		gGL.vertex2fv(hv_bht.mV);

		gGL.texCoord2f(border_uv_scale_x, 1.f - border_uv_scale_y);
		gGL.vertex2fv(bwl_hv_bht.mV);

		LLVector2 bwl_hv = border_width_left + height_vec;
		gGL.texCoord2f(border_uv_scale_x, 1.f);
		gGL.vertex2fv(bwl_hv.mV);

		gGL.texCoord2f(0.f, 1.f - border_uv_scale_y);
		gGL.vertex2fv(hv_bht.mV);

		gGL.texCoord2f(border_uv_scale_x, 1.f);
		gGL.vertex2fv(bwl_hv.mV);

		gGL.texCoord2f(0.f, 1.f);
		gGL.vertex2fv(height_vec.mV);

		// draw top middle
		gGL.texCoord2f(border_uv_scale_x, 1.f - border_uv_scale_y);
		gGL.vertex2fv(bwl_hv_bht.mV);

		gGL.texCoord2f(1.f - border_uv_scale_x, 1.f - border_uv_scale_y);
		gGL.vertex2fv(wv_bwr_hv_bht.mV);

		LLVector2 wv_bwr_hv = width_vec - border_width_right + height_vec;
		gGL.texCoord2f(1.f - border_uv_scale_x, 1.f);
		gGL.vertex2fv(wv_bwr_hv.mV);

		gGL.texCoord2f(border_uv_scale_x, 1.f - border_uv_scale_y);
		gGL.vertex2fv(bwl_hv_bht.mV);

		gGL.texCoord2f(1.f - border_uv_scale_x, 1.f);
		gGL.vertex2fv(wv_bwr_hv.mV);

		gGL.texCoord2f(border_uv_scale_x, 1.f);
		gGL.vertex2fv(bwl_hv.mV);

		// draw top right
		gGL.texCoord2f(1.f - border_uv_scale_x, 1.f - border_uv_scale_y);
		gGL.vertex2fv(wv_bwr_hv_bht.mV);

		gGL.texCoord2f(1.f, 1.f - border_uv_scale_y);
		gGL.vertex2fv(wv_hv_bht.mV);

		LLVector2 wv_hv = width_vec + height_vec;
		gGL.texCoord2f(1.f, 1.f);
		gGL.vertex2fv(wv_hv.mV);

		gGL.texCoord2f(1.f - border_uv_scale_x, 1.f - border_uv_scale_y);
		gGL.vertex2fv(wv_bwr_hv_bht.mV);

		gGL.texCoord2f(1.f, 1.f);
		gGL.vertex2fv(wv_hv.mV);

		gGL.texCoord2f(1.f - border_uv_scale_x, 1.f);
		gGL.vertex2fv(wv_bwr_hv.mV);
	}
	gGL.end();

	gGL.popUIMatrix();
}

void gl_segmented_rect_2d_fragment_tex(S32 left, S32 top, S32 right,
									   S32 bottom, S32 texture_width,
									   S32 texture_height, S32 border_size,
									   F32 start_fragment, F32 end_fragment,
									   U32 edges)
{
	S32 width = abs(right - left);
	S32 height = abs(top - bottom);

	gGL.pushUIMatrix();
	gGL.translateUI((F32)left, (F32)bottom, 0.f);

	LLVector2 border_uv_scale((F32)border_size / (F32)texture_width,
							  (F32)border_size / (F32)texture_height);
	if (border_uv_scale.mV[VX] > 0.5f)
	{
		border_uv_scale *= 0.5f / border_uv_scale.mV[VX];
	}
	if (border_uv_scale.mV[VY] > 0.5f)
	{
		border_uv_scale *= 0.5f / border_uv_scale.mV[VY];
	}

	F32 border_scale = llmin((F32)border_size, (F32)width * 0.5f,
							 (F32)height * 0.5f);
	LLVector2 border_width_left, border_width_right,
			  border_height_bottom, border_height_top;
	if ((edges & (~(U32)ROUNDED_RECT_RIGHT)) != 0)
	{
		border_width_left = LLVector2(border_scale, 0.f);
	}
	if ((edges & (~(U32)ROUNDED_RECT_LEFT)) != 0)
	{
		border_width_right = LLVector2(border_scale, 0.f);
	}
	if ((edges & (~(U32)ROUNDED_RECT_TOP)) != 0)
	{
		border_height_bottom = LLVector2(0.f, border_scale);
	}
	if ((edges & (~(U32)ROUNDED_RECT_BOTTOM)) != 0)
	{
		border_height_top = LLVector2(0.f, border_scale);
	}

	LLVector2 width_vec((F32)width, 0.f);
	LLVector2 height_vec(0.f, (F32)height);

	F32 middle_start = border_scale / (F32)width;
	F32 middle_end = 1.f - middle_start;

	F32 u_min = 0.f;
	F32 u_max = 0.f;
	LLVector2 x_min;
	LLVector2 x_max;

	gGL.begin(LLRender::TRIANGLES);
	{
		if (start_fragment < middle_start)
		{
			F32 start_factor = start_fragment / middle_start;
			F32 end_factor = llmin(end_fragment / middle_start, 1.f);
			u_min = start_factor * border_uv_scale.mV[VX];
			u_max = end_factor * border_uv_scale.mV[VX];
			x_min = start_factor * border_width_left;
			x_max = end_factor * border_width_left;

			// draw bottom left
			gGL.texCoord2f(u_min, 0.f);
			gGL.vertex2fv(x_min.mV);

			gGL.texCoord2f(border_uv_scale.mV[VX], 0.f);
			gGL.vertex2fv(x_max.mV);

			gGL.texCoord2f(u_max, border_uv_scale.mV[VY]);
			gGL.vertex2fv((x_max + border_height_bottom).mV);

			gGL.texCoord2f(u_min, 0.f);
			gGL.vertex2fv(x_min.mV);

			gGL.texCoord2f(u_max, border_uv_scale.mV[VY]);
			gGL.vertex2fv((x_max + border_height_bottom).mV);

			gGL.texCoord2f(u_min, border_uv_scale.mV[VY]);
			gGL.vertex2fv((x_min + border_height_bottom).mV);

			// draw left
			gGL.texCoord2f(u_min, border_uv_scale.mV[VY]);
			gGL.vertex2fv((x_min + border_height_bottom).mV);

			gGL.texCoord2f(u_max, border_uv_scale.mV[VY]);
			gGL.vertex2fv((x_max + border_height_bottom).mV);

			gGL.texCoord2f(u_max, 1.f - border_uv_scale.mV[VY]);
			gGL.vertex2fv((x_max + height_vec - border_height_top).mV);

			gGL.texCoord2f(u_min, border_uv_scale.mV[VY]);
			gGL.vertex2fv((x_min + border_height_bottom).mV);

			gGL.texCoord2f(u_max, 1.f - border_uv_scale.mV[VY]);
			gGL.vertex2fv((x_max + height_vec - border_height_top).mV);

			gGL.texCoord2f(u_min, 1.f - border_uv_scale.mV[VY]);
			gGL.vertex2fv((x_min + height_vec - border_height_top).mV);

			// draw top left
			gGL.texCoord2f(u_min, 1.f - border_uv_scale.mV[VY]);
			gGL.vertex2fv((x_min + height_vec - border_height_top).mV);

			gGL.texCoord2f(u_max, 1.f - border_uv_scale.mV[VY]);
			gGL.vertex2fv((x_max + height_vec - border_height_top).mV);

			gGL.texCoord2f(u_max, 1.f);
			gGL.vertex2fv((x_max + height_vec).mV);

			gGL.texCoord2f(u_min, 1.f - border_uv_scale.mV[VY]);
			gGL.vertex2fv((x_min + height_vec - border_height_top).mV);

			gGL.texCoord2f(u_max, 1.f);
			gGL.vertex2fv((x_max + height_vec).mV);

			gGL.texCoord2f(u_min, 1.f);
			gGL.vertex2fv((x_min + height_vec).mV);
		}

		if (end_fragment > middle_start || start_fragment < middle_end)
		{
			x_min = border_width_left + width_vec *
					(llclamp(start_fragment, middle_start, middle_end) -
					 middle_start);
			x_max = border_width_left + width_vec * 
					(llclamp(end_fragment, middle_start, middle_end) -
					 middle_start);

			// draw bottom middle
			gGL.texCoord2f(border_uv_scale.mV[VX], 0.f);
			gGL.vertex2fv(x_min.mV);

			gGL.texCoord2f(1.f - border_uv_scale.mV[VX], 0.f);
			gGL.vertex2fv((x_max).mV);

			gGL.texCoord2f(1.f - border_uv_scale.mV[VX],
						   border_uv_scale.mV[VY]);
			gGL.vertex2fv((x_max + border_height_bottom).mV);

			gGL.texCoord2f(border_uv_scale.mV[VX], 0.f);
			gGL.vertex2fv(x_min.mV);

			gGL.texCoord2f(1.f - border_uv_scale.mV[VX],
						   border_uv_scale.mV[VY]);
			gGL.vertex2fv((x_max + border_height_bottom).mV);

			gGL.texCoord2f(border_uv_scale.mV[VX], border_uv_scale.mV[VY]);
			gGL.vertex2fv((x_min + border_height_bottom).mV);

			// draw middle
			gGL.texCoord2f(border_uv_scale.mV[VX], border_uv_scale.mV[VY]);
			gGL.vertex2fv((x_min + border_height_bottom).mV);

			gGL.texCoord2f(1.f - border_uv_scale.mV[VX],
						   border_uv_scale.mV[VY]);
			gGL.vertex2fv((x_max + border_height_bottom).mV);

			gGL.texCoord2f(1.f - border_uv_scale.mV[VX],
						   1.f - border_uv_scale.mV[VY]);
			gGL.vertex2fv((x_max + height_vec - border_height_top).mV);

			gGL.texCoord2f(border_uv_scale.mV[VX], border_uv_scale.mV[VY]);
			gGL.vertex2fv((x_min + border_height_bottom).mV);

			gGL.texCoord2f(1.f - border_uv_scale.mV[VX],
						   1.f - border_uv_scale.mV[VY]);
			gGL.vertex2fv((x_max + height_vec - border_height_top).mV);

			gGL.texCoord2f(border_uv_scale.mV[VX],
						   1.f - border_uv_scale.mV[VY]);
			gGL.vertex2fv((x_min + height_vec - border_height_top).mV);

			// draw top middle
			gGL.texCoord2f(border_uv_scale.mV[VX],
						   1.f - border_uv_scale.mV[VY]);
			gGL.vertex2fv((x_min + height_vec - border_height_top).mV);

			gGL.texCoord2f(1.f - border_uv_scale.mV[VX],
						   1.f - border_uv_scale.mV[VY]);
			gGL.vertex2fv((x_max + height_vec - border_height_top).mV);

			gGL.texCoord2f(1.f - border_uv_scale.mV[VX], 1.f);
			gGL.vertex2fv((x_max + height_vec).mV);

			gGL.texCoord2f(border_uv_scale.mV[VX],
						   1.f - border_uv_scale.mV[VY]);
			gGL.vertex2fv((x_min + height_vec - border_height_top).mV);

			gGL.texCoord2f(1.f - border_uv_scale.mV[VX], 1.f);
			gGL.vertex2fv((x_max + height_vec).mV);

			gGL.texCoord2f(border_uv_scale.mV[VX], 1.f);
			gGL.vertex2fv((x_min + height_vec).mV);
		}

		if (end_fragment > middle_end)
		{
			F32 start_factor =
				1.f - llmax(0.f, (start_fragment - middle_end) / middle_start);
			F32 end_factor = 1.f - (end_fragment - middle_end) / middle_start;
			u_min = start_factor * border_uv_scale.mV[VX];
			u_max = end_factor * border_uv_scale.mV[VX];
			x_min = width_vec - start_factor * border_width_right;
			x_max = width_vec - end_factor * border_width_right;

			// draw bottom right
			gGL.texCoord2f(u_min, 0.f);
			gGL.vertex2fv((x_min).mV);

			gGL.texCoord2f(u_max, 0.f);
			gGL.vertex2fv(x_max.mV);

			gGL.texCoord2f(u_max, border_uv_scale.mV[VY]);
			gGL.vertex2fv((x_max + border_height_bottom).mV);

			gGL.texCoord2f(u_min, 0.f);
			gGL.vertex2fv((x_min).mV);

			gGL.texCoord2f(u_max, border_uv_scale.mV[VY]);
			gGL.vertex2fv((x_max + border_height_bottom).mV);

			gGL.texCoord2f(u_min, border_uv_scale.mV[VY]);
			gGL.vertex2fv((x_min + border_height_bottom).mV);

			// draw right
			gGL.texCoord2f(u_min, border_uv_scale.mV[VY]);
			gGL.vertex2fv((x_min + border_height_bottom).mV);

			gGL.texCoord2f(u_max, border_uv_scale.mV[VY]);
			gGL.vertex2fv((x_max + border_height_bottom).mV);

			gGL.texCoord2f(u_max, 1.f - border_uv_scale.mV[VY]);
			gGL.vertex2fv((x_max + height_vec - border_height_top).mV);

			gGL.texCoord2f(u_min, border_uv_scale.mV[VY]);
			gGL.vertex2fv((x_min + border_height_bottom).mV);

			gGL.texCoord2f(u_max, 1.f - border_uv_scale.mV[VY]);
			gGL.vertex2fv((x_max + height_vec - border_height_top).mV);

			gGL.texCoord2f(u_min, 1.f - border_uv_scale.mV[VY]);
			gGL.vertex2fv((x_min + height_vec - border_height_top).mV);

			// draw top right
			gGL.texCoord2f(u_min, 1.f - border_uv_scale.mV[VY]);
			gGL.vertex2fv((x_min + height_vec - border_height_top).mV);

			gGL.texCoord2f(u_max, 1.f - border_uv_scale.mV[VY]);
			gGL.vertex2fv((x_max + height_vec - border_height_top).mV);

			gGL.texCoord2f(u_max, 1.f);
			gGL.vertex2fv((x_max + height_vec).mV);

			gGL.texCoord2f(u_min, 1.f - border_uv_scale.mV[VY]);
			gGL.vertex2fv((x_min + height_vec - border_height_top).mV);

			gGL.texCoord2f(u_max, 1.f);
			gGL.vertex2fv((x_max + height_vec).mV);

			gGL.texCoord2f(u_min, 1.f);
			gGL.vertex2fv((x_min + height_vec).mV);
		}
	}
	gGL.end();

	gGL.popUIMatrix();
}

void gl_segmented_rect_3d_tex(const LLVector2& border_scale,
							  const LLVector3& border_width,
							  const LLVector3& border_height,
							  const LLVector3& width_vec,
							  const LLVector3& height_vec, U32 edges)
{
	LLVector3 left_border_width, right_border_width,
			  top_border_height, bottom_border_height;
	if ((edges & (~(U32)ROUNDED_RECT_RIGHT)) != 0)
	{
		left_border_width = border_width;
	}
	if ((edges & (~(U32)ROUNDED_RECT_LEFT)) != 0)
	{
		right_border_width = border_width;
	}
	if ((edges & (~(U32)ROUNDED_RECT_BOTTOM)) != 0)
	{
		top_border_height = border_height;
	}
	if ((edges & (~(U32)ROUNDED_RECT_TOP)) != 0)
	{
		bottom_border_height = border_height;
	}

	gGL.begin(LLRender::TRIANGLES);
	{
		F32 border_scale_x = border_scale.mV[VX];
		F32 border_scale_y = border_scale.mV[VY];

		// draw bottom left
		gGL.texCoord2f(0.f, 0.f);
		gGL.vertex3f(0.f, 0.f, 0.f);

		gGL.texCoord2f(border_scale_x, 0.f);
		gGL.vertex3fv(left_border_width.mV);

		LLVector3 lbw_bbh = left_border_width + bottom_border_height;
		gGL.texCoord2f(border_scale_x, border_scale_y);
		gGL.vertex3fv(lbw_bbh.mV);

		gGL.texCoord2f(0.f, 0.f);
		gGL.vertex3f(0.f, 0.f, 0.f);

		gGL.texCoord2f(border_scale_x, border_scale_y);
		gGL.vertex3fv(lbw_bbh.mV);

		gGL.texCoord2f(0.f, border_scale_y);
		gGL.vertex3fv(bottom_border_height.mV);

		// draw bottom middle
		gGL.texCoord2f(border_scale_x, 0.f);
		gGL.vertex3fv(left_border_width.mV);

		LLVector3 wv_rbw = width_vec - right_border_width;
		gGL.texCoord2f(1.f - border_scale_x, 0.f);
		gGL.vertex3fv(wv_rbw.mV);

		LLVector3 wv_rbw_bbh = width_vec - right_border_width +
							   bottom_border_height;
		gGL.texCoord2f(1.f - border_scale_x, border_scale_y);
		gGL.vertex3fv(wv_rbw_bbh.mV);

		gGL.texCoord2f(border_scale_x, 0.f);
		gGL.vertex3fv(left_border_width.mV);

		gGL.texCoord2f(1.f - border_scale_x, border_scale_y);
		gGL.vertex3fv(wv_rbw_bbh.mV);

		gGL.texCoord2f(border_scale_x, border_scale_y);
		gGL.vertex3fv(lbw_bbh.mV);

		// draw bottom right
		gGL.texCoord2f(1.f - border_scale_x, 0.f);
		gGL.vertex3fv(wv_rbw.mV);

		gGL.texCoord2f(1.f, 0.f);
		gGL.vertex3fv(width_vec.mV);

		LLVector3 wv_bbh = width_vec + bottom_border_height;
		gGL.texCoord2f(1.f, border_scale_y);
		gGL.vertex3fv(wv_bbh.mV);

		gGL.texCoord2f(1.f - border_scale_x, 0.f);
		gGL.vertex3fv(wv_rbw.mV);

		gGL.texCoord2f(1.f, border_scale_y);
		gGL.vertex3fv(wv_bbh.mV);

		gGL.texCoord2f(1.f - border_scale_x, border_scale_y);
		gGL.vertex3fv(wv_rbw_bbh.mV);

		// draw left
		gGL.texCoord2f(0.f, border_scale_y);
		gGL.vertex3fv(bottom_border_height.mV);

		gGL.texCoord2f(border_scale_x, border_scale_y);
		gGL.vertex3fv(lbw_bbh.mV);

		LLVector3 lbw_hv_tbh = left_border_width + height_vec -
							   top_border_height;
		gGL.texCoord2f(border_scale_x, 1.f - border_scale_y);
		gGL.vertex3fv(lbw_hv_tbh.mV);

		gGL.texCoord2f(0.f, border_scale_y);
		gGL.vertex3fv(bottom_border_height.mV);

		gGL.texCoord2f(border_scale_x, 1.f - border_scale_y);
		gGL.vertex3fv(lbw_hv_tbh.mV);

		LLVector3 hv_tbh = height_vec - top_border_height;
		gGL.texCoord2f(0.f, 1.f - border_scale_y);
		gGL.vertex3fv(hv_tbh.mV);

		// draw middle
		gGL.texCoord2f(border_scale_x, border_scale_y);
		gGL.vertex3fv(lbw_bbh.mV);

		gGL.texCoord2f(1.f - border_scale_x, border_scale_y);
		gGL.vertex3fv(wv_rbw_bbh.mV);

		LLVector3 wv_rbw_hv_tbh = width_vec - right_border_width + height_vec -
								  top_border_height;
		gGL.texCoord2f(1.f - border_scale_x, 1.f - border_scale_y);
		gGL.vertex3fv(wv_rbw_hv_tbh.mV);

		gGL.texCoord2f(border_scale_x, border_scale_y);
		gGL.vertex3fv(lbw_bbh.mV);

		gGL.texCoord2f(1.f - border_scale_x, 1.f - border_scale_y);
		gGL.vertex3fv(wv_rbw_hv_tbh.mV);

		gGL.texCoord2f(border_scale_x, 1.f - border_scale_y);
		gGL.vertex3fv(lbw_hv_tbh.mV);

		// draw right
		gGL.texCoord2f(1.f - border_scale_x, border_scale_y);
		gGL.vertex3fv(wv_rbw_bbh.mV);

		gGL.texCoord2f(1.f, border_scale_y);
		gGL.vertex3fv(wv_bbh.mV);

		LLVector3 wv_hv_tbh = width_vec + height_vec - top_border_height;
		gGL.texCoord2f(1.f, 1.f - border_scale_y);
		gGL.vertex3fv(wv_hv_tbh.mV);

		gGL.texCoord2f(1.f - border_scale_x, border_scale_y);
		gGL.vertex3fv(wv_rbw_bbh.mV);

		gGL.texCoord2f(1.f, 1.f - border_scale_y);
		gGL.vertex3fv(wv_hv_tbh.mV);

		gGL.texCoord2f(1.f - border_scale_x, 1.f - border_scale_y);
		gGL.vertex3fv(wv_rbw_hv_tbh.mV);

		// draw top left
		gGL.texCoord2f(0.f, 1.f - border_scale_y);
		gGL.vertex3fv(hv_tbh.mV);

		gGL.texCoord2f(border_scale_x, 1.f - border_scale_y);
		gGL.vertex3fv(lbw_hv_tbh.mV);

		LLVector3 lbw_hv = left_border_width + height_vec;
		gGL.texCoord2f(border_scale_x, 1.f);
		gGL.vertex3fv(lbw_hv.mV);

		gGL.texCoord2f(0.f, 1.f - border_scale_y);
		gGL.vertex3fv(hv_tbh.mV);

		gGL.texCoord2f(border_scale_x, 1.f);
		gGL.vertex3fv(lbw_hv.mV);

		gGL.texCoord2f(0.f, 1.f);
		gGL.vertex3fv((height_vec).mV);

		// draw top middle
		gGL.texCoord2f(border_scale_x, 1.f - border_scale_y);
		gGL.vertex3fv(lbw_hv_tbh.mV);

		gGL.texCoord2f(1.f - border_scale_x, 1.f - border_scale_y);
		gGL.vertex3fv(wv_rbw_hv_tbh.mV);

		LLVector3 wv_rbw_hv = width_vec - right_border_width + height_vec;
		gGL.texCoord2f(1.f - border_scale_x, 1.f);
		gGL.vertex3fv(wv_rbw_hv.mV);

		gGL.texCoord2f(border_scale_x, 1.f - border_scale_y);
		gGL.vertex3fv(lbw_hv_tbh.mV);

		gGL.texCoord2f(1.f - border_scale_x, 1.f);
		gGL.vertex3fv(wv_rbw_hv.mV);

		gGL.texCoord2f(border_scale_x, 1.f);
		gGL.vertex3fv(lbw_hv.mV);

		// draw top right
		gGL.texCoord2f(1.f - border_scale_x, 1.f - border_scale_y);
		gGL.vertex3fv(wv_rbw_hv_tbh.mV);

		gGL.texCoord2f(1.f, 1.f - border_scale_y);
		gGL.vertex3fv(wv_hv_tbh.mV);

		LLVector3 wv_hv = width_vec + height_vec;
		gGL.texCoord2f(1.f, 1.f);
		gGL.vertex3fv(wv_hv.mV);

		gGL.texCoord2f(1.f - border_scale_x, 1.f - border_scale_y);
		gGL.vertex3fv(wv_rbw_hv_tbh.mV);

		gGL.texCoord2f(1.f, 1.f);
		gGL.vertex3fv(wv_hv.mV);

		gGL.texCoord2f(1.f - border_scale_x, 1.f);
		gGL.vertex3fv(wv_rbw_hv.mV);
	}
	gGL.end();
}

void gl_draw_3d_cross_lines(const LLVector3& center, F32 dx, F32 dy, F32 dz)
{
	const F32& x = center.mV[VX];
	const F32& y = center.mV[VY];
	const F32& z = center.mV[VZ];
	gGL.vertex3f(x - dx, y, z);
	gGL.vertex3f(x + dx, y, z);
	gGL.vertex3f(x, y - dy, z);
	gGL.vertex3f(x, y + dy, z);
	gGL.vertex3f(x, y, z - dz);
	gGL.vertex3f(x, y, z + dz);
}

void gl_draw_3d_line_cube(F32 width, const LLVector3& center)
{
	width *= 0.5f;
	const F32& x = center.mV[VX];
	const F32 x1 = x + width;
	const F32 x2 = x - width;
	const F32& y = center.mV[VY];
	const F32 y1 = y + width;
	const F32 y2 = y - width;
	const F32& z = center.mV[VZ];
	const F32 z1 = z + width;
	const F32 z2 = z - width;

	gGL.vertex3f(x1, y1, z1);
	gGL.vertex3f(x2, y1, z1);
	gGL.vertex3f(x2, y1, z1);
	gGL.vertex3f(x2, y2, z1);
	gGL.vertex3f(x2, y2, z1);
	gGL.vertex3f(x1, y2, z1);
	gGL.vertex3f(x1, y2, z1);
	gGL.vertex3f(x1, y1, z1);

	gGL.vertex3f(x1, y1, z2);
	gGL.vertex3f(x2, y1, z2);
	gGL.vertex3f(x2, y1, z2);
	gGL.vertex3f(x2, y2, z2);
	gGL.vertex3f(x2, y2, z2);
	gGL.vertex3f(x1, y2, z2);
	gGL.vertex3f(x1, y2, z2);
	gGL.vertex3f(x1, y1, z2);

	gGL.vertex3f(x1, y1, z1);
	gGL.vertex3f(x1, y1, z2);
	gGL.vertex3f(x2, y1, z1);
	gGL.vertex3f(x2, y1, z2);
	gGL.vertex3f(x2, y2, z1);
	gGL.vertex3f(x2, y2, z2);
	gGL.vertex3f(x1, y2, z1);
	gGL.vertex3f(x1, y2, z2);
}

///////////////////////////////////////////////////////////////////////////////
// LLBox / gBox. This used to be in llbox.cpp
///////////////////////////////////////////////////////////////////////////////

LLBox gBox;

// These routines support multiple textures on a box
void LLBox::prerender()
{
	F32 size = 1.f;

	mTriangleCount = 6 * 2;

	mVertex[0][0] = mVertex[1][0] = mVertex[2][0] = mVertex[3][0] = -size / 2;
	mVertex[4][0] = mVertex[5][0] = mVertex[6][0] = mVertex[7][0] =  size / 2;
	mVertex[0][1] = mVertex[1][1] = mVertex[4][1] = mVertex[5][1] = -size / 2;
	mVertex[2][1] = mVertex[3][1] = mVertex[6][1] = mVertex[7][1] =  size / 2;
	mVertex[0][2] = mVertex[3][2] = mVertex[4][2] = mVertex[7][2] = -size / 2;
	mVertex[1][2] = mVertex[2][2] = mVertex[5][2] = mVertex[6][2] =  size / 2;
}

// These routines support multiple textures on a box
void LLBox::cleanupGL()
{
	// No GL state, a noop.
}

void LLBox::renderface(S32 which_face)
{
#if 0
	static F32 normals[6][3] =
	{
		{ -1.f,  0.f,  0.f },
		{  0.f,  1.f,  0.f },
		{  1.f,  0.f,  0.f },
		{  0.f, -1.f,  0.f },
		{  0.f,  0.f,  1.f },
		{  0.f,  0.f, -1.f }
	};
#endif
	static S32 faces[6][4] =
	{
		{ 0, 1, 2, 3 },
		{ 3, 2, 6, 7 },
		{ 7, 6, 5, 4 },
		{ 4, 5, 1, 0 },
		{ 5, 6, 2, 1 },
		{ 7, 4, 0, 3 }
	};

	gGL.begin(LLRender::TRIANGLES);
	{
#if 0
		gGL.normal3fv(&normals[which_face][0]);
#endif
		gGL.texCoord2f(1.f, 0.f);
		gGL.vertex3fv(&mVertex[faces[which_face][0]][0]);
		gGL.texCoord2f(1.f, 1.f);
		gGL.vertex3fv(&mVertex[faces[which_face][1]][0]);
		gGL.texCoord2f(0.f, 1.f);
		gGL.vertex3fv(&mVertex[faces[which_face][2]][0]);
		gGL.texCoord2f(1.f, 0.f);
		gGL.vertex3fv(&mVertex[faces[which_face][0]][0]);
		gGL.texCoord2f(0.f, 1.f);
		gGL.vertex3fv(&mVertex[faces[which_face][2]][0]);
		gGL.texCoord2f(0.f, 0.f);
		gGL.vertex3fv(&mVertex[faces[which_face][3]][0]);
	}
	gGL.end();
}

void LLBox::render()
{
    // This is a flattend representation of the box as render here
    //                                       .
    //              (-++)        (+++)      /|\t
    //                +------------+         | (texture coordinates)
    //                |2          1|         |
    //                |     4      |        (*) --->s
    //                |    TOP     |
    //                |            |
    // (-++)     (--+)|3          0|(+-+)     (+++)        (-++)
    //   +------------+------------+------------+------------+
    //   |2          1|2          1|2          1|2          1|
    //   |     0      |     1      |     2      |     3      |
    //   |   BACK     |   RIGHT    |   FRONT    |   LEFT     |
    //   |            |            |            |            |
    //   |3          0|3          0|3          0|3          0|
    //   +------------+------------+------------+------------+
    // (-+-)     (---)|2          1|(+--)     (++-)        (-+-)
    //                |     5      |
    //                |   BOTTOM   |
    //                |            |
    //                |3          0|
    //                +------------+
    //              (-+-)        (++-)

	renderface(5);
	renderface(4);
	renderface(3);
	renderface(2);
	renderface(1);
	renderface(0);
}

///////////////////////////////////////////////////////////////////////////////
// LLCone / gCone. This used to be in llcylinder.cpp
///////////////////////////////////////////////////////////////////////////////

LLCone gCone;

void LLCone::render(S32 sides)
{
	gGL.begin(LLRender::TRIANGLE_FAN);
	gGL.vertex3f(0, 0, 0);

	for (S32 i = 0; i < sides; ++i)
	{
		F32 a = (F32)i / sides * F_PI * 2.f;
		F32 x = cosf(a) * 0.5f;
		F32 y = sinf(a) * 0.5f;
		gGL.vertex3f(x, y, -.5f);
	}
	gGL.vertex3f(cosf(0.f) * 0.5f, sinf(0.f) * 0.5f, -0.5f);

	gGL.end();

	gGL.begin(LLRender::TRIANGLE_FAN);
	gGL.vertex3f(0.f, 0.f, 0.5f);
	for (S32 i = 0; i < sides; ++i)
	{
		F32 a = (F32)i / sides * F_PI * 2.f;
		F32 x = cosf(a) * 0.5f;
		F32 y = sinf(a) * 0.5f;
		gGL.vertex3f(x, y, -0.5f);
	}
	gGL.vertex3f(cosf(0.f) * 0.5f, sinf(0.f) * 0.5f, -0.5f);

	gGL.end();
}

///////////////////////////////////////////////////////////////////////////////
// LLRenderSphere / gSphere. This used to be in llrendersphere.cpp
///////////////////////////////////////////////////////////////////////////////

LLRenderSphere gSphere;

void LLRenderSphere::render()
{
	renderGGL();
	gGL.flush();
}

LL_INLINE LLVector3 polar_to_cart(F32 latitude, F32 longitude)
{
	return LLVector3(sinf(F_TWO_PI * latitude) * cosf(F_TWO_PI * longitude),
					 sinf(F_TWO_PI * latitude) * sinf(F_TWO_PI * longitude),
					 cosf(F_TWO_PI * latitude));
}

void LLRenderSphere::renderGGL()
{
	constexpr S32 LATITUDE_SLICES = 20;
	constexpr S32 LONGITUDE_SLICES = 30;

	if (mSpherePoints.empty())
	{
		mSpherePoints.resize(LATITUDE_SLICES + 1);
		for (S32 lat_i = 0; lat_i < LATITUDE_SLICES + 1; lat_i++)
		{
			mSpherePoints[lat_i].resize(LONGITUDE_SLICES + 1);
			for (S32 lon_i = 0; lon_i < LONGITUDE_SLICES + 1; lon_i++)
			{
				F32 lat = (F32)lat_i / LATITUDE_SLICES;
				F32 lon = (F32)lon_i / LONGITUDE_SLICES;

				mSpherePoints[lat_i][lon_i] = polar_to_cart(lat, lon);
			}
		}
	}

	gGL.begin(LLRender::TRIANGLES);
	for (S32 lat_i = 0; lat_i < LATITUDE_SLICES; lat_i++)
	{
		for (S32 lon_i = 0; lon_i < LONGITUDE_SLICES; lon_i++)
		{
			gGL.vertex3fv(mSpherePoints[lat_i][lon_i].mV);
			gGL.vertex3fv(mSpherePoints[lat_i][lon_i+1].mV);
			gGL.vertex3fv(mSpherePoints[lat_i+1][lon_i].mV);

			gGL.vertex3fv(mSpherePoints[lat_i+1][lon_i].mV);
			gGL.vertex3fv(mSpherePoints[lat_i][lon_i+1].mV);
			gGL.vertex3fv(mSpherePoints[lat_i+1][lon_i+1].mV);
		}
	}
	gGL.end();
}
