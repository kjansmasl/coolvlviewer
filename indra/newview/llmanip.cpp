/**
 * @file llmanip.cpp
 * @brief LLManip class implementation
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

#include "llmanip.h"

#include "llgl.h"
#include "lllocale.h"
#include "llprimitive.h"
#include "llrender.h"
#include "llview.h"

#include "llagent.h"
#include "lldrawable.h"
#include "llfontgl.h"
#include "llpipeline.h"
#include "llselectmgr.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerdisplay.h"		// For hud_render_text()
#include "llviewerjoint.h"
#include "llviewerobject.h"
#include "llviewerregion.h"
#include "llviewertexturelist.h"
#include "llviewerwindow.h"
#include "llvoavatar.h"
#include "llworld.h"

// Local constants...
constexpr F32 VERTICAL_OFFSET = 50.f;

F32 LLManip::sHelpTextVisibleTime = 2.f;
F32 LLManip::sHelpTextFadeTime = 2.f;
S32 LLManip::sNumTimesHelpTextShown = 0;
S32 LLManip::sMaxTimesShowHelpText = 5;
F32 LLManip::sGridMaxSubdivisionLevel = 32.f;
F32 LLManip::sGridMinSubdivisionLevel = 1.f;
LLVector2 LLManip::sTickLabelSpacing(60.f, 25.f);

//static
void LLManip::rebuild(LLViewerObject* vobj)
{
	LLDrawable* drawablep = vobj->mDrawable;
	if (drawablep && drawablep->getVOVolume())
	{
		gPipeline.markRebuild(drawablep, LLDrawable::REBUILD_VOLUME);
		drawablep->setState(LLDrawable::MOVE_UNDAMPED); // force to UNDAMPED
		drawablep->updateMove();
		LLSpatialGroup* group = drawablep->getSpatialGroup();
		if (group)
		{
			group->dirtyGeom();
			gPipeline.markRebuild(group);
		}

		LLViewerObject::const_child_list_t& child_list = vobj->getChildren();
		for (LLViewerObject::child_list_t::const_iterator
				iter = child_list.begin(), endIter = child_list.end();
			 iter != endIter; ++iter)
		{
			LLViewerObject* child = *iter;
			if (child)	// Paranoia
			{
				rebuild(child);
			}
		}
	}
}

LLManip::LLManip(const std::string& name, LLToolComposite* composite)
:	LLTool(name, composite),
	mInSnapRegime(false),
	mHighlightedPart(LL_NO_PART),
	mManipPart(LL_NO_PART)
{
}

void LLManip::getManipNormal(LLViewerObject* object, EManipPart manip,
							 LLVector3& normal)
{
	LLVector3 grid_origin;
	LLVector3 grid_scale;
	LLQuaternion grid_rotation;

	gSelectMgr.getGrid(grid_origin, grid_rotation, grid_scale);

	if (manip >= LL_X_ARROW && manip <= LL_Z_ARROW)
	{
		LLVector3 arrow_axis;
		getManipAxis(object, manip, arrow_axis);

		LLVector3 cross = arrow_axis % gViewerCamera.getAtAxis();
		normal = cross % arrow_axis;
		normal.normalize();
	}
	else if (manip >= LL_YZ_PLANE && manip <= LL_XY_PLANE)
	{
		switch (manip)
		{
			case LL_YZ_PLANE:
				normal = LLVector3::x_axis;
				break;
			case LL_XZ_PLANE:
				normal = LLVector3::y_axis;
				break;
			case LL_XY_PLANE:
				normal = LLVector3::z_axis;
				break;
			default:
				break;
		}
		normal.rotVec(grid_rotation);
	}
	else
	{
		normal.clear();
	}
}


bool LLManip::getManipAxis(LLViewerObject* object, EManipPart manip,
						   LLVector3& axis)
{
	LLVector3 grid_origin;
	LLVector3 grid_scale;
	LLQuaternion grid_rotation;

	gSelectMgr.getGrid(grid_origin, grid_rotation, grid_scale);

	if (manip == LL_X_ARROW)
	{
		axis = LLVector3::x_axis;
	}
	else if (manip == LL_Y_ARROW)
	{
		axis = LLVector3::y_axis;
	}
	else if (manip == LL_Z_ARROW)
	{
		axis = LLVector3::z_axis;
	}
	else
	{
		return false;
	}

	axis.rotVec(grid_rotation);
	return true;
}

F32 LLManip::getSubdivisionLevel(const LLVector3& reference_point,
								 const LLVector3& translate_axis,
								 F32 grid_scale, S32 min_pixel_spacing)
{
	// Update current snap subdivision level
	LLVector3 cam_to_reference;
	if (mObjectSelection->getSelectType() == SELECT_TYPE_HUD)
	{
		cam_to_reference = LLVector3(1.f / gAgent.mHUDCurZoom, 0.f, 0.f);
	}
	else
	{
		cam_to_reference = reference_point - gViewerCamera.getOrigin();
	}
	F32 current_range = cam_to_reference.normalize();

	F32 projected_translation_axis_length = (translate_axis % cam_to_reference).length();
	F32 subdivisions = llmax(projected_translation_axis_length * grid_scale /
							 (current_range / gViewerCamera.getPixelMeterRatio() *
							 min_pixel_spacing), 0.f);
	subdivisions = llclamp(powf(2.f, llfloor(logf(subdivisions) / F_LN2)),
						   1.f / 32.f, 32.f);

	return subdivisions;
}

void LLManip::handleSelect()
{
	mObjectSelection = gSelectMgr.getEditSelection();
}

void LLManip::handleDeselect()
{
	mHighlightedPart = LL_NO_PART;
	mManipPart = LL_NO_PART;
	mObjectSelection = NULL;
}

LLObjectSelectionHandle LLManip::getSelection()
{
	return mObjectSelection;
}

bool LLManip::handleHover(S32 x, S32 y, MASK mask)
{
	// We only handle the event if mousedown started with us
	if (hasMouseCapture())
	{
		if (mObjectSelection->isEmpty())
		{
			// Somehow the object got deselected while we were dragging it.
			// Release the mouse
			setMouseCapture(false);
		}

		LL_DEBUGS("UserInput") << "hover handled by LLManip (active)"
							   << LL_ENDL;
	}
	else
	{
		LL_DEBUGS("UserInput") << "hover handled by LLManip (inactive)"
							   << LL_ENDL;
	}
	gViewerWindowp->setCursor(UI_CURSOR_ARROW);
	return true;
}


bool LLManip::handleMouseUp(S32 x, S32 y, MASK mask)
{
	bool handled = false;
	if (hasMouseCapture())
	{
		handled = true;
		setMouseCapture(false);
	}
	return handled;
}

void LLManip::updateGridSettings()
{
	sGridMaxSubdivisionLevel = gSavedSettings.getBool("GridSubUnit") ? (F32)gSavedSettings.getS32("GridSubdivision")
																	 : 1.f;
}

bool LLManip::getMousePointOnPlaneAgent(LLVector3& point, S32 x, S32 y,
										LLVector3 origin, LLVector3 normal)
{
	LLVector3d origin_double = gAgent.getPosGlobalFromAgent(origin);
	LLVector3d global_point;
	bool result = getMousePointOnPlaneGlobal(global_point, x, y, origin_double,
											 normal);
	point = gAgent.getPosAgentFromGlobal(global_point);
	return result;
}

bool LLManip::getMousePointOnPlaneGlobal(LLVector3d& point,
										 S32 x, S32 y,
										 LLVector3d origin,
										 LLVector3 normal) const
{
	if (mObjectSelection->getSelectType() == SELECT_TYPE_HUD)
	{
		bool result = false;
		F32 mouse_x = ((F32)x / gViewerWindowp->getWindowWidth() - 0.5f) *
					  gViewerCamera.getAspect() /
					  gAgent.mHUDCurZoom;
		F32 mouse_y = ((F32)y / gViewerWindowp->getWindowHeight() - 0.5f) /
					  gAgent.mHUDCurZoom;

		LLVector3 origin_agent = gAgent.getPosAgentFromGlobal(origin);
		LLVector3 mouse_pos = LLVector3(0.f, -mouse_x, mouse_y);
		if (fabsf(normal.mV[VX]) < 0.001f)
		{
			// use largish value that should be outside HUD manipulation range
			mouse_pos.mV[VX] = 10.f;
		}
		else
		{
			mouse_pos.mV[VX] = (normal * (origin_agent - mouse_pos))
								/ (normal.mV[VX]);
			result = true;
		}

		point = gAgent.getPosGlobalFromAgent(mouse_pos);
		return result;
	}
	else
	{
		return gViewerWindowp->mousePointOnPlaneGlobal(point, x, y, origin,
													   normal);
	}
}

// Given the line defined by mouse cursor (a1 + a_param*(a2-a1)) and the line
// defined by b1 + b_param * (b2 - b1), returns a_param and b_param for the
// points where lines are closest to each other.
// Returns false if the two lines are parallel.
bool LLManip::nearestPointOnLineFromMouse(S32 x, S32 y, const LLVector3& b1,
										  const LLVector3& b2, F32& a_param,
										  F32& b_param)
{
	LLVector3 a1;
	LLVector3 a2;

	if (mObjectSelection->getSelectType() == SELECT_TYPE_HUD)
	{
		F32 mouse_x = (((F32)x / gViewerWindowp->getWindowWidth()) - 0.5f) *
					  gViewerCamera.getAspect() /
					  gAgent.mHUDCurZoom;
		F32 mouse_y = (((F32)y / gViewerWindowp->getWindowHeight()) - 0.5f) /
					  gAgent.mHUDCurZoom;
		a1 = LLVector3(llmin(b1.mV[VX] - 0.1f, b2.mV[VX] - 0.1f, 0.f),
					   -mouse_x, mouse_y);
		a2 = a1 + LLVector3(1.f, 0.f, 0.f);
	}
	else
	{
		a1 = gAgent.getCameraPositionAgent();
		a2 = gAgent.getCameraPositionAgent() + LLVector3(gViewerWindowp->mouseDirectionGlobal(x, y));
	}

	bool parallel = true;
	LLVector3 a = a2 - a1;
	LLVector3 b = b2 - b1;

	LLVector3 normal;
	F32 dist, denom;
	normal = (b % a) % b;	// normal to plane (P) through b and (shortest line between a and b)
	normal.normalize();
	dist = b1 * normal;		// distance from origin to P

	denom = normal * a;
	if (denom < -F_APPROXIMATELY_ZERO || F_APPROXIMATELY_ZERO < denom)
	{
		a_param = (dist - normal * a1) / denom;
		parallel = false;
	}

	normal = (a % b) % a;	// normal to plane (P) through a and (shortest line between a and b)
	normal.normalize();
	dist = a1 * normal;		// distance from origin to P
	denom = normal * b;
	if (denom < -F_APPROXIMATELY_ZERO || F_APPROXIMATELY_ZERO < denom)
	{
		b_param = (dist - normal * b1) / denom;
		parallel = false;
	}

	return parallel;
}

LLVector3 LLManip::getSavedPivotPoint() const
{
	return gSelectMgr.getSavedBBoxOfSelection().getCenterAgent();
}

LLVector3 LLManip::getPivotPoint()
{
	static LLCachedControl<bool> at_root(gSavedSettings,
										 "BuildUseRootForPivot");
	LLViewerObject*	object = mObjectSelection->getFirstRootObject(true);
	if (object && (at_root || mObjectSelection->getObjectCount() == 1))
	{
		return object->getPivotPositionAgent();
	}
	return gSelectMgr.getBBoxOfSelection().getCenterAgent();
}

void LLManip::renderGuidelines(bool draw_x, bool draw_y, bool draw_z)
{
	LLVector3 grid_origin;
	LLQuaternion grid_rot;
	LLVector3 grid_scale;
	gSelectMgr.getGrid(grid_origin, grid_rot, grid_scale);

	constexpr bool children_ok = true;
	LLViewerObject* object = mObjectSelection->getFirstRootObject(children_ok);
	if (!object)
	{
		return;
	}

	//LLVector3 center_agent = gSelectMgr.getBBoxOfSelection().getCenterAgent();
	LLVector3 center_agent = getPivotPoint();

	gGL.pushMatrix();
	{
		gGL.translatef(center_agent.mV[VX], center_agent.mV[VY],
					   center_agent.mV[VZ]);

		F32 angle_radians, x, y, z;

		grid_rot.getAngleAxis(&angle_radians, &x, &y, &z);
		gGL.rotatef(angle_radians * RAD_TO_DEG, x, y, z);

		LLViewerRegion* region = gAgent.getRegion();
		F32 region_size = region ? region->getWidth() : REGION_WIDTH_METERS;

		constexpr F32 LINE_ALPHA = 0.33f;

		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
		LLUI::setLineWidth(1.5f);

		if (draw_x)
		{
			gGL.color4f(1.f, 0.f, 0.f, LINE_ALPHA);
			gGL.begin(LLRender::LINES);
			gGL.vertex3f(-region_size, 0.f, 0.f);
			gGL.vertex3f( region_size, 0.f, 0.f);
			gGL.end();
		}

		if (draw_y)
		{
			gGL.color4f(0.f, 1.f, 0.f, LINE_ALPHA);
			gGL.begin(LLRender::LINES);
			gGL.vertex3f(0.f, -region_size, 0.f);
			gGL.vertex3f(0.f,  region_size, 0.f);
			gGL.end();
		}

		if (draw_z)
		{
			gGL.color4f(0.f, 0.f, 1.f, LINE_ALPHA);
			gGL.begin(LLRender::LINES);
			gGL.vertex3f(0.f, 0.f, -region_size);
			gGL.vertex3f(0.f, 0.f,  region_size);
			gGL.end();
		}
		LLUI::setLineWidth(1.0f);
	}
	gGL.popMatrix();
}

void LLManip::renderXYZ(const LLVector3& vec)
{
	static const LLFontGL* font = LLFontGL::getFontSansSerif();

	static const LLColor4 color_x(1.f, 0.5f, 0.5f, 1.f);
	static const LLColor4 color_y(0.5f, 1.f, 0.5f, 1.f);
	static const LLColor4 color_z(0.5f, 0.5f, 1.f, 1.f);
	static const LLColor4 color_bg(0.f, 0.f, 0.f, 0.7f);

	constexpr F32 PAD = 10.f;
	F32 window_center_x = (F32)(gViewerWindowp->getWindowWidth() / 2);
	F32 window_center_y = (F32)(gViewerWindowp->getWindowHeight() / 2);
	F32 vertical_offset = window_center_y - VERTICAL_OFFSET;
	F32 center_y = window_center_y + vertical_offset;

	LLWString str_x = utf8str_to_wstring(llformat("X: %.3f", vec.mV[VX]));
	LLWString str_y = utf8str_to_wstring(llformat("Y: %.3f", vec.mV[VY]));
	LLWString str_z = utf8str_to_wstring(llformat("Z: %.3f", vec.mV[VZ]));

	gGL.pushMatrix();
	{
		gViewerWindowp->setup2DRender();

		gGL.color4f(0.f, 0.f, 0.f, 0.7f);
		constexpr F32 y_factor = PAD * 2.f + 10.f;
		LLUIImage::sRoundedSquare->draw((window_center_x - 115.f) *
										LLFontGL::sScaleX,
										(center_y - PAD) * LLFontGL::sScaleY,
										235.f * LLFontGL::sScaleX,
										y_factor * LLFontGL::sScaleY,
										color_bg);

		LLLocale locale(LLLocale::USER_LOCALE);
		LLGLDepthTest gls_depth(GL_FALSE);

		// Render drop shadowed text (manually because of bigger 'distance')
		F32 right_x;
		font->render(str_x, 0, window_center_x - 101.f, center_y - 2.f,
					 LLColor4::black, LLFontGL::LEFT, LLFontGL::BASELINE,
					 LLFontGL::NORMAL, S32_MAX, 1000, &right_x);

		font->render(str_y, 0, window_center_x - 26.f, center_y - 2.f,
					 LLColor4::black, LLFontGL::LEFT, LLFontGL::BASELINE,
					 LLFontGL::NORMAL, S32_MAX, 1000, &right_x);

		font->render(str_z, 0, window_center_x + 49.f, center_y - 2.f,
					 LLColor4::black, LLFontGL::LEFT, LLFontGL::BASELINE,
					 LLFontGL::NORMAL, S32_MAX, 1000, &right_x);

		// Render text on top
		font->render(str_x, 0, window_center_x - 102.f, center_y, color_x,
					 LLFontGL::LEFT, LLFontGL::BASELINE, LLFontGL::NORMAL,
					 S32_MAX, 1000, &right_x);

		font->render(str_y, 0, window_center_x - 27.f, center_y, color_y,
					 LLFontGL::LEFT, LLFontGL::BASELINE, LLFontGL::NORMAL,
					 S32_MAX, 1000, &right_x);

		font->render(str_z, 0, window_center_x + 48.f, center_y, color_z,
					 LLFontGL::LEFT, LLFontGL::BASELINE, LLFontGL::NORMAL,
					 S32_MAX, 1000, &right_x);
	}

	gGL.popMatrix();

	gViewerWindowp->setup3DRender();
}

void LLManip::renderTickText(const LLVector3& pos, const std::string& text)
{
	static const LLFontGL* big_fontp = LLFontGL::getFontSansSerif();

	bool is_hud = mObjectSelection->getSelectType() == SELECT_TYPE_HUD;
	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.pushMatrix();
	LLVector3 render_pos = pos;
	if (is_hud)
	{
		F32 zoom_amt = gAgent.mHUDCurZoom;
		F32 inv_zoom_amt = 1.f / zoom_amt;
		// scale text back up to counter-act zoom level
		render_pos = pos * zoom_amt;
		gGL.scalef(inv_zoom_amt, inv_zoom_amt, inv_zoom_amt);
	}

	// Render shadow first
	const LLColor4& color = LLColor4::white;
	LLColor4 shadow_color = LLColor4::black;
	shadow_color.mV[VALPHA] = color.mV[VALPHA] * 0.5f;
	gViewerWindowp->setupViewport(1, -1);
	LLWString wstr(utf8str_to_wstring(text));
	F32 text_width = -0.5f * big_fontp->getWidthF32(wstr.c_str());
	hud_render_text(wstr, render_pos, *big_fontp, LLFontGL::NORMAL, text_width,
					3.f, shadow_color, is_hud);
	gViewerWindowp->setupViewport();
	hud_render_text(wstr, render_pos, *big_fontp, LLFontGL::NORMAL, text_width,
					3.f, color, is_hud);

	gGL.popMatrix();
}

void LLManip::renderTickValue(const LLVector3& pos, F32 value,
							  const std::string& suffix,
							  const LLColor4& color)
{
	LLLocale locale(LLLocale::USER_LOCALE);

	static const LLFontGL* big_fontp = LLFontGL::getFontSansSerif();
	static const LLFontGL* small_fontp = LLFontGL::getFontSansSerifSmall();

	std::string val_string;
	std::string fraction_string;
	F32 val_to_print = ll_round(value, 0.001f);
	S32 fractional_portion = ll_round(fmodf(fabsf(val_to_print), 1.f) * 100.f);
	if (val_to_print < 0.f)
	{
		if (fractional_portion == 0)
		{
			val_string = llformat("-%d%s", lltrunc(fabsf(val_to_print)),
								  suffix.c_str());
		}
		else
		{
			val_string = llformat("-%d", lltrunc(fabsf(val_to_print)));
		}
	}
	else if (fractional_portion == 0)
	{
		val_string = llformat("%d%s", lltrunc(fabsf(val_to_print)),
							  suffix.c_str());
	}
	else
	{
		val_string = llformat("%d", lltrunc(val_to_print));
	}
	F32 val_str_width = big_fontp->getWidthF32(val_string);

	bool is_hud = mObjectSelection->getSelectType() == SELECT_TYPE_HUD;
	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.pushMatrix();
	LLVector3 render_pos = pos;
	if (is_hud)
	{
		F32 zoom_amt = gAgent.mHUDCurZoom;
		F32 inv_zoom_amt = 1.f / zoom_amt;
		// Scale text back up to counter-act zoom level
		render_pos = pos * zoom_amt;
		gGL.scalef(inv_zoom_amt, inv_zoom_amt, inv_zoom_amt);
	}

	LLColor4 shadow_color = LLColor4::black;
	shadow_color.mV[VALPHA] = color.mV[VALPHA] * 0.5f;

	LLWString wstr(utf8str_to_wstring(val_string));
	if (fractional_portion != 0)
	{
		fraction_string = llformat("%c%02d%s", LLLocale::getDecimalPoint(),
								   fractional_portion, suffix.c_str());
		LLWString wstr2(utf8str_to_wstring(fraction_string));
		gViewerWindowp->setupViewport(1, -1);
		hud_render_text(wstr, render_pos, *big_fontp, LLFontGL::NORMAL,
						-val_str_width, 3.f, shadow_color, is_hud);
		hud_render_text(wstr2, render_pos, *small_fontp, LLFontGL::NORMAL,
						1.f, 3.f, shadow_color, is_hud);

		gViewerWindowp->setupViewport();
		hud_render_text(wstr, render_pos, *big_fontp, LLFontGL::NORMAL,
						-val_str_width, 3.f, color, is_hud);
		hud_render_text(wstr2, render_pos, *small_fontp, LLFontGL::NORMAL,
						1.f, 3.f, color, is_hud);
	}
	else
	{
		gViewerWindowp->setupViewport(1, -1);
		hud_render_text(wstr, render_pos, *big_fontp, LLFontGL::NORMAL,
						-0.5f * val_str_width, 3.f, shadow_color, is_hud);
		gViewerWindowp->setupViewport();
		hud_render_text(wstr, render_pos, *big_fontp, LLFontGL::NORMAL,
						-0.5f * val_str_width, 3.f, color, is_hud);
	}
	gGL.popMatrix();
}

LLColor4 LLManip::setupSnapGuideRenderPass(S32 pass)
{
	static LLCachedControl<LLColor4U> grid_color_fg(gColors, "GridlineColor");
	static LLCachedControl<LLColor4U> grid_color_bg(gColors,
													"GridlineBGColor");
	static LLCachedControl<LLColor4U> grid_color_shadow(gColors,
														"GridlineShadowColor");
	static LLCachedControl<F32> line_alpha(gSavedSettings, "GridOpacity");

	LLColor4 line_color;

	switch (pass)
	{
		case 0:	// Shadow
			gViewerWindowp->setupViewport(1, -1);
			line_color = LLColor4(grid_color_shadow);
			line_color.mV[VALPHA] *= (F32)line_alpha;
			LLUI::setLineWidth(2.f);
			break;

		case 1:	// Hidden lines
			gViewerWindowp->setupViewport();
			line_color = LLColor4(grid_color_bg);
			line_color.mV[VALPHA] *= (F32)line_alpha;
			LLUI::setLineWidth(1.f);
			break;

		case 2:	// Visible lines
			line_color = LLColor4(grid_color_fg);
			line_color.mV[VALPHA] *= (F32)line_alpha;
	}

	return line_color;
}
