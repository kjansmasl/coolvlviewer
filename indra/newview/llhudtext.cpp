/**
 * @file llhudtext.cpp
 * @brief LLHUDText class implementation
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

#include "boost/tokenizer.hpp"

#include "llhudtext.h"

#include "llglheaders.h"
#include "llmenugl.h"
#include "llimagegl.h"
#include "llrender.h"

#include "llagent.h"
#include "llchatbar.h"
#include "lldrawable.h"
#include "llpipeline.h"
#include "llstatusbar.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerdisplay.h"		// For hud_render_text()
#include "llviewerobject.h"
#include "llviewertexturelist.h"
#include "llviewerwindow.h"
//MK
#include "mkrlinterface.h"
#include "llviewerregion.h"
#include "llvoavatarself.h"
//mk
#include "llvovolume.h"

constexpr F32 SPRING_STRENGTH = 0.7f;
constexpr F32 HORIZONTAL_PADDING = 15.f;
constexpr F32 VERTICAL_PADDING = 12.f;
constexpr F32 BUFFER_SIZE = 2.f;
constexpr F32 HUD_TEXT_MAX_WIDTH = 190.f;
constexpr F32 HUD_TEXT_MAX_WIDTH_NO_BUBBLE = 1000.f;
constexpr S32 NUM_OVERLAP_ITERATIONS = 10;
constexpr F32 POSITION_DAMPING_TC = 0.2f;
constexpr F32 MAX_STABLE_CAMERA_VELOCITY = 0.1f;
constexpr F32 LOD_0_SCREEN_COVERAGE = 0.15f;
constexpr F32 LOD_1_SCREEN_COVERAGE = 0.3f;
constexpr F32 LOD_2_SCREEN_COVERAGE = 0.4f;

LLHUDText::htobj_list_t LLHUDText::sTextObjects;
LLHUDText::visible_list_t LLHUDText::sVisibleTextObjects;
LLHUDText::visible_list_t LLHUDText::sVisibleHUDTextObjects;
bool LLHUDText::sDisplayText = true;

struct hto_further_away
{
	LL_INLINE bool operator()(const LLPointer<LLHUDText>& lhs,
							  const LLPointer<LLHUDText>& rhs) const
	{
		return lhs->getDistance() > rhs->getDistance();
	}
};

LLHUDText::LLHUDText(U8 type)
:	LLHUDObject(type),
	mFontp(LLFontGL::getFontSansSerifSmall()),
	mBoldFontp(LLFontGL::getFontSansSerifBold()),
	mColor(LLColor4(1.f, 1.f, 1.f, 1.f)),
	mTextAlignment(ALIGN_TEXT_CENTER),
	mVertAlignment(ALIGN_VERT_CENTER),
	mLOD(0),
	mWidth(0.f),
	mHeight(0.f),
	mMass(1.f),
	mMaxLines(10),
	mOffsetY(0),
	mLastDistance(0.f),
	mFadeDistance(8.f),
	mFadeRange(4.f),
	mRadius(0.1f),
	mUseBubble(false),
	mUsePixelSize(true),
	mVisibleOffScreen(false),
	mOffScreen(false),
	mHidden(false),
	mDoFade(true),
	mZCompare(true),
	mDropShadow(true)
{
	sTextObjects.emplace(this);
}

bool LLHUDText::lineSegmentIntersect(const LLVector4a& start,
									 const LLVector4a& end,
									 LLVector4a& intersection,
									 bool debug_render)
{
	if (!mVisible || mHidden)
	{
		return false;
	}

	// Do not pick text that is not bound to a viewerobject or is not in a
	// bubble
	if (!mSourceObject || mSourceObject->mDrawable.isNull() || !mUseBubble)
	{
		return false;
	}

	F32 color_alpha = mColor.mV[3];
	if (mDoFade && mLastDistance > mFadeDistance)
	{
		// Could make color_alpha negative, but we do not care since we just
		// compare its max value below to decide whether to abort or not. HB
		color_alpha *= 1.f - (mLastDistance - mFadeDistance) / mFadeRange;
	}
	if (color_alpha < 0.01f)
	{
		return false;	// Nothing visible any more to intersect with.
	}

	mOffsetY = lltrunc(mHeight *
					   (mVertAlignment == ALIGN_VERT_CENTER ? 0.5f : 1.f));

	// Scale screen size of borders down. RN: for now, text on hud objects is
	// never occluded.

	LLVector3 x_pixel_vec;
	LLVector3 y_pixel_vec;

	if (mOnHUDAttachment)
	{
		x_pixel_vec =
			LLVector3::y_axis / (F32)gViewerWindowp->getWindowWidth();
		y_pixel_vec =
			LLVector3::z_axis / (F32)gViewerWindowp->getWindowHeight();
	}
	else
	{
		gViewerCamera.getPixelVectors(mPositionAgent, y_pixel_vec,
									   x_pixel_vec);
	}

	LLVector3 width_vec = mWidth * x_pixel_vec;
	LLVector3 height_vec = mHeight * y_pixel_vec;

	LLCoordGL screen_pos;
	gViewerCamera.projectPosAgentToScreen(mPositionAgent, screen_pos, false);

	LLVector2 screen_offset;
	screen_offset = updateScreenPos(mPositionOffset);

	LLVector3 render_position = mPositionAgent +
								x_pixel_vec * screen_offset.mV[VX] +
								y_pixel_vec * screen_offset.mV[VY];

	if (mUseBubble)
	{
		LLVector3 bg_pos = render_position + (F32)mOffsetY * y_pixel_vec -
						   width_vec * 0.5f - height_vec;
		//LLUI::translate(bg_pos.mV[VX], bg_pos.mV[VY], bg_pos.mV[VZ]);

		LLVector3 v[] = {
			bg_pos,
			bg_pos + width_vec,
			bg_pos + width_vec + height_vec,
			bg_pos + height_vec,
		};

		if (debug_render)
		{
			gGL.begin(LLRender::LINE_STRIP);
			gGL.vertex3fv(v[0].mV);
			gGL.vertex3fv(v[1].mV);
			gGL.vertex3fv(v[2].mV);
			gGL.vertex3fv(v[3].mV);
			gGL.vertex3fv(v[0].mV);
			gGL.vertex3fv(v[2].mV);
			gGL.end();
		}

		LLVector4a dir;
		dir.setSub(end,start);
		F32 a, b, t;

		LLVector4a v0, v1, v2, v3;
		v0.load3(v[0].mV);
		v1.load3(v[1].mV);
		v2.load3(v[2].mV);
		v3.load3(v[3].mV);

		if (LLTriangleRayIntersect(v0, v1, v2, start, dir, a, b, t) ||
			LLTriangleRayIntersect(v2, v3, v0, start, dir, a, b, t))
		{
			if (t <= 1.f)
			{
				dir.mul(t);
				intersection.setAdd(start, dir);
				return true;
			}
		}
	}

	return false;
}

void LLHUDText::render()
{
	if (!mOnHUDAttachment && sDisplayText)
	{
		LLGLDepthTest gls_depth(GL_TRUE, GL_FALSE);
		renderText();
	}
}

void LLHUDText::renderText()
{
	if (!mVisible || mHidden)
	{
		return;
	}

	LLTexUnit* unit0 = gGL.getTexUnit(0);
	unit0->enable(LLTexUnit::TT_TEXTURE);

	LLGLState gls_blend(GL_BLEND, GL_TRUE);

	LLColor4 shadow_color(0.f, 0.f, 0.f, 1.f);
	F32 alpha_factor = 1.f;
	LLColor4 text_color = mColor;
//MK
	F32 fade_distance = mFadeDistance;
	F32 fade_range = mFadeRange;
	if (gRLenabled && gRLInterface.mCamDistDrawMin < fade_distance)
	{
		fade_distance = gRLInterface.mCamDistDrawMin;
		fade_range = 1.f;
	}
//mk
	if (mDoFade && mLastDistance > fade_distance)
	{
		alpha_factor = llmax(0.f,
							 1.f - (mLastDistance - fade_distance) / fade_range);
		text_color.mV[3] = text_color.mV[3] * alpha_factor;
	}
	if (text_color.mV[3] < 0.01f)
	{
		return;
	}
	shadow_color.mV[3] = text_color.mV[3];

	if (gUsePBRShaders && mOnHUDAttachment)
	{
		text_color = linearColor4(text_color);
		shadow_color = linearColor4(text_color);
	}

	mOffsetY = lltrunc(mHeight * (mVertAlignment == ALIGN_VERT_CENTER ? 0.5f
																	  : 1.f));

	// *TODO: make this a per-text setting
	static LLCachedControl<LLColor4> background_chat_color(gSavedSettings,
														   "BackgroundChatColor");
	LLColor4 bg_color = background_chat_color;
	static LLCachedControl<F32> chat_bubble_opacity(gSavedSettings,
													"ChatBubbleOpacity");
	bg_color.setAlpha(chat_bubble_opacity * alpha_factor);

	constexpr S32 border_height = 16;
	constexpr S32 border_width = 16;

	F32 border_scale = 1.f;
	if (border_height * 2 > mHeight)
	{
		border_scale = (F32)mHeight / ((F32)border_height * 2.f);
	}
	if (border_width * 2 > mWidth)
	{
		border_scale = llmin(border_scale,
							 (F32)mWidth / ((F32)border_width * 2.f));
	}

	// Scale screen size of borders down
	// RN: for now, text on hud objects is never occluded

	LLVector3 x_pixel_vec;
	LLVector3 y_pixel_vec;
	if (mOnHUDAttachment)
	{
		x_pixel_vec =
			LLVector3::y_axis / (F32)gViewerWindowp->getWindowWidth();
		y_pixel_vec =
			LLVector3::z_axis / (F32)gViewerWindowp->getWindowHeight();
	}
	else
	{
		gViewerCamera.getPixelVectors(mPositionAgent, y_pixel_vec,
									   x_pixel_vec);
	}

	static const F32 tex_width = LLUIImage::sRoundedSquareWidth;
	static const F32 tex_height = LLUIImage::sRoundedSquareHeight;
	LLVector2 border_scale_vec((F32)border_width / tex_width,
							   (F32)border_height / tex_height);
	LLVector3 width_vec = mWidth * x_pixel_vec;
	LLVector3 height_vec = mHeight * y_pixel_vec;
	LLVector3 scaled_border_width = (F32)llfloor(border_scale *
												 (F32)border_width) *
									x_pixel_vec;
	LLVector3 scaled_border_height = (F32)llfloor(border_scale *
												  (F32)border_height) *
									 y_pixel_vec;

	mRadius = (width_vec + height_vec).length() * 0.5f;

	LLCoordGL screen_pos;
	gViewerCamera.projectPosAgentToScreen(mPositionAgent, screen_pos, false);

	LLVector2 screen_offset;
	if (!mUseBubble)
	{
		screen_offset = mPositionOffset;
	}
	else
	{
		screen_offset = updateScreenPos(mPositionOffset);
	}

	LLVector3 render_position = mPositionAgent +
								(x_pixel_vec * screen_offset.mV[VX]) +
								(y_pixel_vec * screen_offset.mV[VY]);

	if (mUseBubble)
	{
		LLGLDepthTest gls_depth(GL_TRUE, GL_FALSE);
		LLUI::pushMatrix();
		{
			LLVector3 bg_pos = render_position + (F32)mOffsetY * y_pixel_vec -
							   width_vec * 0.5f - height_vec;
			LLUI::translate(bg_pos.mV[VX], bg_pos.mV[VY], bg_pos.mV[VZ]);

			unit0->bind(LLUIImage::sRoundedSquare->getImage());

			gGL.color4fv(bg_color.mV);
			gl_segmented_rect_3d_tex(border_scale_vec, scaled_border_width,
									 scaled_border_height, width_vec,
									 height_vec);

			if (mLabelSegments.size())
			{
				LLUI::pushMatrix();
				{
					gGL.color4f(text_color.mV[VX], text_color.mV[VY],
								text_color.mV[VZ],
								chat_bubble_opacity * alpha_factor);
					LLVector3 label_height =
						(mFontp->getLineHeight() * mLabelSegments.size() +
						 VERTICAL_PADDING / 3.f) * y_pixel_vec;
					LLVector3 label_offset = height_vec - label_height;
					LLUI::translate(label_offset.mV[VX], label_offset.mV[VY],
									label_offset.mV[VZ]);
					gl_segmented_rect_3d_tex_top(border_scale_vec,
												 scaled_border_width,
												 scaled_border_height,
												 width_vec, label_height);
				}
				LLUI::popMatrix();
			}

			bool outside_width = fabsf(mPositionOffset.mV[VX]) > mWidth * 0.5f;
			bool outside_height = fabsf(mPositionOffset.mV[VY] +
										 (mVertAlignment == ALIGN_VERT_TOP ?
											mHeight * 0.5f : 0.f)) >
								  mHeight * (mVertAlignment == ALIGN_VERT_TOP ?
												mHeight * 0.75f : 0.5f);

			// Draw line segments pointing to parent object
			if (!mOffScreen && (outside_width || outside_height))
			{
				LLUI::pushMatrix();
				{
					gGL.color4fv(bg_color.mV);
					LLVector3 target_pos = -1.f * (mPositionOffset.mV[VX] *
												   x_pixel_vec +
												   mPositionOffset.mV[VY] *
												   y_pixel_vec);
					target_pos += width_vec * 0.5f;
					if (mVertAlignment == ALIGN_VERT_CENTER)
					{
						target_pos += height_vec * 0.5f;
					}
					target_pos -= 3.f * x_pixel_vec;
					target_pos -= 6.f * y_pixel_vec;
					LLUI::translate(target_pos.mV[VX], target_pos.mV[VY],
									target_pos.mV[VZ]);
					gl_segmented_rect_3d_tex(border_scale_vec,
											 3.f * x_pixel_vec,
											 3.f * y_pixel_vec,
											 6.f * x_pixel_vec,
											 6.f * y_pixel_vec);
				}
				LLUI::popMatrix();

				unit0->unbind(LLTexUnit::TT_TEXTURE);
				LLGLDepthTest gls_depth(mZCompare ? GL_TRUE : GL_FALSE,
										GL_FALSE);

				LLVector3 box_center_offset = width_vec * 0.5f +
											  height_vec * 0.5f;
				LLUI::translate(box_center_offset.mV[VX],
								box_center_offset.mV[VY],
								box_center_offset.mV[VZ]);
				gGL.color4fv(bg_color.mV);
				LLUI::setLineWidth(2.f);
				gGL.begin(LLRender::LINES);
				{
					if (outside_width)
					{
						LLVector3 vert;
						// Draw line in x then y
						if (mPositionOffset.mV[VX] < 0.f)
						{
							// Start at right edge
							vert = width_vec * 0.5f;
							gGL.vertex3fv(vert.mV);
						}
						else
						{
							// Start at left edge
							vert = width_vec * -0.5f;
							gGL.vertex3fv(vert.mV);
						}
						vert = -mPositionOffset.mV[VX] * x_pixel_vec;
						gGL.vertex3fv(vert.mV);
						gGL.vertex3fv(vert.mV);
						vert -= mPositionOffset.mV[VY] * y_pixel_vec;
						if (mVertAlignment == ALIGN_VERT_TOP)
						{
							vert -= height_vec * 0.5f;
						}
						gGL.vertex3fv(vert.mV);
					}
					else
					{
						LLVector3 vert;
						// Draw line in y then x
						if (mPositionOffset.mV[VY] < 0.f)
						{
							// Start at top edge
							vert = height_vec * 0.5f -
								   mPositionOffset.mV[VX] * x_pixel_vec;
							gGL.vertex3fv(vert.mV);
						}
						else
						{
							// Start at bottom edge
							vert = height_vec * -0.5f -
								   mPositionOffset.mV[VX] * x_pixel_vec;
							gGL.vertex3fv(vert.mV);
						}
						vert = -mPositionOffset.mV[VY] * y_pixel_vec -
							   mPositionOffset.mV[VX] * x_pixel_vec;
						vert -= mVertAlignment == ALIGN_VERT_TOP ? height_vec * 0.5f
																 : LLVector3::zero;
						gGL.vertex3fv(vert.mV);
					}
				}
				gGL.end();
				LLUI::setLineWidth(1.0);

			}
		}
		LLUI::popMatrix();
	}

	F32 x_offset = 0.f;
	bool center = mTextAlignment == ALIGN_TEXT_CENTER;
	if (!center)
	{
		// ALIGN_LEFT
		x_offset = -0.5f * mWidth + (HORIZONTAL_PADDING * 0.5f);
	}
	F32 y_offset = (F32)mOffsetY;

	// Render label
	{
		LLColor4 label_color;
		for (S32 i = 0, count = mLabelSegments.size(); i < count; ++i)
		{
			LLHUDTextSegment& segment = mLabelSegments[i];
			const LLFontGL* fontp =
				segment.mStyle == LLFontGL::BOLD ? mBoldFontp : mFontp;
			y_offset -= fontp->getLineHeight();

			if (center)
			{
				x_offset = -0.5f * segment.getWidth(fontp);
			}

			label_color.mV[VALPHA] = alpha_factor;
			if (gUsePBRShaders && mOnHUDAttachment)
			{
				label_color = linearColor4(label_color);
			}
			hud_render_text(segment.getText(), render_position, *fontp,
							segment.mStyle, x_offset, y_offset,
							label_color, mOnHUDAttachment);
		}
	}

	// Render text
	{
		S32 start_segment;
		S32 max_lines = getMaxLines();	// -1 means unlimited lines.
		if (max_lines < 0)
		{
			start_segment = 0;
		}
		else
		{
			start_segment = llmax(0, (S32)mTextSegments.size() - max_lines);
		}
		for (S32 i = start_segment, count = mTextSegments.size(); i < count;
			 ++i)
		{
			LLHUDTextSegment& segment = mTextSegments[i];
			U8 style = segment.mStyle;
			const LLFontGL* fontp = style == LLFontGL::BOLD ? mBoldFontp
															: mFontp;
			y_offset -= fontp->getLineHeight();

			if (mDropShadow)
			{
				style |= LLFontGL::DROP_SHADOW;
			}

			if (center)
			{
				x_offset = -0.5f * segment.getWidth(fontp);
			}

			text_color = segment.mColor;
			text_color.mV[VALPHA] *= alpha_factor;
			if (gUsePBRShaders && mOnHUDAttachment)
			{
				text_color = linearColor4(text_color);
			}

			hud_render_text(segment.getText(), render_position, *fontp,
							style, x_offset, y_offset, text_color,
							mOnHUDAttachment);
		}
	}

	// Reset the default color to white. The renderer expects this to be the
	// default.
	gGL.color4f(1.f, 1.f, 1.f, 1.f);
}

void LLHUDText::setStringUTF8(const std::string& wtext)
{
	setString(utf8str_to_wstring(wtext));
}

void LLHUDText::setString(const LLWString& wtext)
{
	mTextSegments.clear();
//MK
	if (gRLenabled)
	{
		LLWString local_wtext = wtext;
		if (gRLInterface.mContainsShowhovertextall)
		{
			local_wtext = utf8str_to_wstring("", 0);
		}
		else if ((mOnHUDAttachment &&
				  gRLInterface.mContainsShowhovertexthud) ||
				 (!mOnHUDAttachment &&
				  gRLInterface.mContainsShowhovertextworld) ||
				 (mSourceObject &&
				  gRLInterface.contains("showhovertext:" +
										mSourceObject->getID().asString())))
		{
			local_wtext = utf8str_to_wstring("", 0);
		}
		else
		{
			if (gRLInterface.mContainsShowloc)
			{
				std::string str = wstring_to_utf8str(local_wtext,
													 local_wtext.length());
				str = gRLInterface.getCensoredLocation(str);
				local_wtext = utf8str_to_wstring(str, str.length());
			}
			if (gRLInterface.mContainsShownames ||
				gRLInterface.mContainsShownametags)
			{
				std::string str = wstring_to_utf8str(local_wtext,
													 local_wtext.length());
				str = gRLInterface.getCensoredMessage(str);
				local_wtext = utf8str_to_wstring(str, str.length());
			}
		}
		addLine(local_wtext, mColor);
	}
	else
//mk
		addLine(wtext, mColor);
}

void LLHUDText::clearString()
{
	mTextSegments.clear();
}

void LLHUDText::addLine(const std::string& str, const LLColor4& color,
						const LLFontGL::StyleFlags style)
{
	addLine(utf8str_to_wstring(str), color, style);
}

void LLHUDText::addLine(const LLWString& wstr, const LLColor4& color,
						const LLFontGL::StyleFlags style)
{
	if (!wstr.empty())
	{
		LLWString wline(wstr);
		typedef boost::tokenizer<boost::char_separator<llwchar>,
								 LLWString::const_iterator,
								 LLWString> tokenizer;
		LLWString seps(utf8str_to_wstring("\r\n"));
		boost::char_separator<llwchar> sep(seps.c_str());

		tokenizer tokens(wline, sep);
		tokenizer::iterator iter = tokens.begin();
		tokenizer::iterator tokens_end = tokens.end();
		while (iter != tokens_end)
		{
			U32 line_length = 0;
			do
			{
				S32 segment_length =
					mFontp->maxDrawableChars(iter->substr(line_length).c_str(),
											 mUseBubble ? HUD_TEXT_MAX_WIDTH
														: HUD_TEXT_MAX_WIDTH_NO_BUBBLE,
											 wline.length(), true);
				mTextSegments.emplace_back(iter->substr(line_length,
														segment_length),
										   style, color);
				line_length += segment_length;
			}
			while (line_length != iter->size());
			++iter;
		}
	}
}

void LLHUDText::setLabel(const std::string& label)
{
	setLabel(utf8str_to_wstring(label));
}

void LLHUDText::setLabel(const LLWString& wlabel)
{
	mLabelSegments.clear();

	if (!wlabel.empty())
	{
		LLWString wstr(wlabel);
		LLWString seps(utf8str_to_wstring("\r\n"));
		LLWString empty;

		typedef boost::tokenizer<boost::char_separator<llwchar>,
								 LLWString::const_iterator,
								 LLWString> tokenizer;
		boost::char_separator<llwchar> sep(seps.c_str(), empty.c_str(),
										   boost::keep_empty_tokens);

		tokenizer tokens(wstr, sep);
		tokenizer::iterator iter = tokens.begin();
		tokenizer::iterator tokens_end = tokens.end();
		while (iter != tokens_end)
		{
			U32 line_length = 0;
			do
			{
				S32 segment_length =
					mFontp->maxDrawableChars(iter->substr(line_length).c_str(),
											 mUseBubble ? HUD_TEXT_MAX_WIDTH
														: HUD_TEXT_MAX_WIDTH_NO_BUBBLE,
											 wstr.length(), true);
				mLabelSegments.emplace_back(iter->substr(line_length,
														 segment_length),
											LLFontGL::NORMAL, mColor);
				line_length += segment_length;
			}
			while (line_length != iter->size());
			++iter;
		}
	}
}

void LLHUDText::setColor(const LLColor4& color)
{
	mColor = color;
	for (S32 i = 0, count = mTextSegments.size(); i < count; ++i)
	{
		mTextSegments[i].mColor = color;
	}
}

void LLHUDText::updateVisibility()
{
	if (mSourceObject)
	{
		mSourceObject->updateText();
	}

	mPositionAgent = gAgent.getPosAgentFromGlobal(mPositionGlobal);

	if (!mSourceObject)
	{
		mVisible = true;
		if (mOnHUDAttachment)
		{
			sVisibleHUDTextObjects.emplace_back(this);
		}
		else
		{
			sVisibleTextObjects.emplace_back(this);
		}
		return;
	}

	// Not visible if parent object is dead
	if (mSourceObject->isDead())
	{
		mVisible = false;
		return;
	}

	// For now, all text on HUD objects is visible
	if (mOnHUDAttachment)
	{
		mVisible = true;
		mLastDistance = mPositionAgent.mV[VX];
		sVisibleHUDTextObjects.emplace_back(this);
		return;
	}

	// Push text towards camera by radius of object, but not past camera
	LLVector3 vec_from_camera = mPositionAgent - gViewerCamera.getOrigin();
	LLVector3 dir_from_camera = vec_from_camera;
	dir_from_camera.normalize();

	if (dir_from_camera * gViewerCamera.getAtAxis() <= 0.f)
	{
		// Text is behind camera, do not render
		mVisible = false;
		return;
	}

	if (vec_from_camera * gViewerCamera.getAtAxis() <=
			gViewerCamera.getNear() + 0.1f + mSourceObject->getVObjRadius())
	{
		mPositionAgent = gViewerCamera.getOrigin() + vec_from_camera *
						 ((gViewerCamera.getNear() + 0.1f) /
						 (vec_from_camera * gViewerCamera.getAtAxis()));
	}
	else
	{
		mPositionAgent -= dir_from_camera * mSourceObject->getVObjRadius();
	}

//MK
	if (gRLenabled && gRLInterface.mCamDistDrawMin < EXTREMUM)
	{
		mLastDistance =
			(mPositionAgent -
			 (isAgentAvatarValid() ? gAgentAvatarp->mHeadp->getWorldPosition()
								   : gAgent.getPositionAgent())).length();
	}
	else
//mk
	{
		mLastDistance = (mPositionAgent - gViewerCamera.getOrigin()).length();
	}

	if (mLOD >= 3 || !mTextSegments.size() ||
		(mDoFade && mLastDistance > mFadeDistance + mFadeRange))
	{
		mVisible = false;
		return;
	}

	LLVector3 x_pixel_vec;
	LLVector3 y_pixel_vec;
	gViewerCamera.getPixelVectors(mPositionAgent, y_pixel_vec, x_pixel_vec);

	LLVector3 render_position = mPositionAgent +
								x_pixel_vec * mPositionOffset.mV[VX] +
								y_pixel_vec * mPositionOffset.mV[VY];

	mOffScreen = false;
	if (!gViewerCamera.sphereInFrustum(render_position, mRadius))
	{
		if (!mVisibleOffScreen)
		{
			mVisible = false;
			return;
		}
		mOffScreen = true;
	}

	mVisible = true;
	sVisibleTextObjects.emplace_back(this);
}

LLVector2 LLHUDText::updateScreenPos(LLVector2& offset)
{
	LLCoordGL screen_pos;
	LLVector2 screen_pos_vec;
	LLVector3 x_pixel_vec;
	LLVector3 y_pixel_vec;
	gViewerCamera.getPixelVectors(mPositionAgent, y_pixel_vec, x_pixel_vec);
	LLVector3 world_pos = mPositionAgent + offset.mV[VX] * x_pixel_vec +
						  offset.mV[VY] * y_pixel_vec;
	if (!gViewerCamera.projectPosAgentToScreen(world_pos, screen_pos, false) &&
		mVisibleOffScreen)
	{
		// Bubble off-screen, so find a spot for it along screen edge
		LLVector2 window_center(gViewerWindowp->getWindowDisplayWidth() * 0.5f,
								gViewerWindowp->getWindowDisplayHeight() * 0.5f);
		LLVector2 delta_from_center(screen_pos.mX - window_center.mV[VX],
									screen_pos.mY - window_center.mV[VY]);
		delta_from_center.normalize();

		F32 camera_aspect = gViewerCamera.getAspect();
		F32 delta_aspect = fabsf(delta_from_center.mV[VX] /
								 delta_from_center.mV[VY]);
		if (camera_aspect / llmax(delta_aspect, 0.001f) > 1.f)
		{
			// Camera has wider aspect ratio than offset vector, so clamp to
			// height
			delta_from_center *= fabsf(window_center.mV[VY] /
									   delta_from_center.mV[VY]);
		}
		else
		{
			// Camera has narrower aspect ratio than offset vector, so clamp to
			// width
			delta_from_center *= fabsf(window_center.mV[VX] /
									   delta_from_center.mV[VX]);
		}

		screen_pos_vec = window_center + delta_from_center;
	}
	else
	{
		screen_pos_vec.set((F32)screen_pos.mX, (F32)screen_pos.mY);
	}
	S32 bottom = gStatusBarHeight;
	if (gChatBarp && gChatBarp->getVisible())
	{
		bottom += CHAT_BAR_HEIGHT;
	}

	LLVector2 screen_center;
	screen_center.mV[VX] =
		llclamp((F32)screen_pos_vec.mV[VX], mWidth * 0.5f,
				(F32)gViewerWindowp->getWindowDisplayWidth() - mWidth * 0.5f);

	if (mVertAlignment == ALIGN_VERT_TOP)
	{
		screen_center.mV[VY] =
			llclamp((F32)screen_pos_vec.mV[VY], (F32)bottom,
					(F32)gViewerWindowp->getWindowDisplayHeight() - mHeight -
					(F32)gMenuBarHeight);
		mSoftScreenRect.setLeftTopAndSize(screen_center.mV[VX] -
										  (mWidth + BUFFER_SIZE) * 0.5f,
										  screen_center.mV[VY] + mHeight +
										  BUFFER_SIZE, mWidth + BUFFER_SIZE,
										  mHeight + BUFFER_SIZE);
	}
	else
	{
		screen_center.mV[VY] =
			llclamp((F32)screen_pos_vec.mV[VY],
					(F32)bottom + mHeight * 0.5f,
					(F32)gViewerWindowp->getWindowDisplayHeight() -
					mHeight * 0.5f - (F32)gMenuBarHeight);
		mSoftScreenRect.setCenterAndSize(screen_center.mV[VX],
										 screen_center.mV[VY],
										 mWidth + BUFFER_SIZE,
										 mHeight + BUFFER_SIZE);
	}

	return offset + screen_center -
		   LLVector2((F32)screen_pos.mX, (F32)screen_pos.mY);
}

void LLHUDText::updateSize()
{
	F32 width = 0.f;

	S32 max_lines = getMaxLines();
	S32 lines = max_lines < 0 ? (S32)mTextSegments.size()
							  : llmin((S32)mTextSegments.size(), max_lines);

	F32 height = (F32)mFontp->getLineHeight() * (lines + mLabelSegments.size());

	S32 start_segment = 0;
	if (max_lines > 0)
	{
		start_segment = llmax(0, (S32)mTextSegments.size() - max_lines);
	}

	segments_vec_t::iterator iter = mTextSegments.begin() + start_segment;
	segments_vec_t::iterator end = mTextSegments.end();
	while (iter != end)
	{
		width = llmax(width,
					  llmin(iter++->getWidth(mFontp), HUD_TEXT_MAX_WIDTH));
	}

	iter = mLabelSegments.begin();
	end = mLabelSegments.end();
	while (iter != end)
	{
		width = llmax(width,
					  llmin(iter++->getWidth(mFontp), HUD_TEXT_MAX_WIDTH));
	}

	if (width == 0.f)
	{
		return;
	}

	width += HORIZONTAL_PADDING;
	height += VERTICAL_PADDING;

	mWidth = llmax(width, lerp(mWidth, (F32)width, 1.f));
	mHeight = llmax(height, lerp(mHeight, (F32)height, 1.f));
}

void LLHUDText::updateAll()
{
	sVisibleTextObjects.clear();
	sVisibleHUDTextObjects.clear();
	if (sTextObjects.empty())
	{
		return;	// Nothing to do !
	}

	// Iterate over all text objects, calculate their restoration forces, and
	// add them to the visible set if they are on screen and close enough.
	for (htobj_list_it_t it = sTextObjects.begin(), end = sTextObjects.end();
		 it != end; ++it)
	{
		LLHUDText* textp = it->get();
		textp->mTargetPositionOffset.clear();
		textp->updateSize();
		textp->updateVisibility();
	}

	const S32 count = sVisibleTextObjects.size();

	if (count == 0)
	{
		if (!sVisibleHUDTextObjects.empty())
		{
			std::sort(sVisibleHUDTextObjects.begin(),
					  sVisibleHUDTextObjects.end(), hto_further_away());
		}
		// Nothing else to do...
		return;
	}

	// Sort back to front for rendering purposes.
	// Note: I tried to get rid of these costly std::sort calls by using a
	// std::set sorted with hto_further_away() (instead of a std::vector) for
	// both sVisibleTextObjects and sVisibleTextObjects, but it looks like it
	// does not suffice to get the objects properly and naturally sorted on
	// distance in the sets, causing bad HUD hover-text rendering issues... HB
	std::sort(sVisibleTextObjects.begin(), sVisibleTextObjects.end(),
			  hto_further_away());
	std::sort(sVisibleHUDTextObjects.begin(), sVisibleHUDTextObjects.end(),
			  hto_further_away());

	// Iterate from front to back, and set LOD based on current screen coverage
	F32 screen_area = (F32)(gViewerWindowp->getWindowWidth() *
							gViewerWindowp->getWindowHeight());
	F32 current_screen_area = 0.f;
	for (S32 i = count - 1; i >= 0; --i)
	{
		LLHUDText* textp = sVisibleTextObjects[i].get();
		if (textp->mUseBubble)
		{
			if (current_screen_area / screen_area > LOD_2_SCREEN_COVERAGE)
			{
				textp->setLOD(3);
			}
			else if (current_screen_area / screen_area > LOD_1_SCREEN_COVERAGE)
			{
				textp->setLOD(2);
			}
			else if (current_screen_area / screen_area > LOD_0_SCREEN_COVERAGE)
			{
				textp->setLOD(1);
			}
			else
			{
				textp->setLOD(0);
			}
			textp->updateSize();
			// Find on-screen position and initialize collision rectangle
			textp->mTargetPositionOffset =
				textp->updateScreenPos(LLVector2::zero);
			current_screen_area += (F32)(textp->mSoftScreenRect.getWidth() *
										 textp->mSoftScreenRect.getHeight());
		}
	}

	if (LLViewerCamera::getVelocityStat().getCurrent() >
			MAX_STABLE_CAMERA_VELOCITY)
	{
		return;
	}

	for (S32 i = 0; i < NUM_OVERLAP_ITERATIONS; ++i)
	{
		for (S32 src_idx = 0; src_idx < count; ++src_idx)
		{
			LLHUDText* srcp = sVisibleTextObjects[src_idx].get();
			if (!srcp->mUseBubble)
			{
				continue;
			}

			for (S32 dst_idx = src_idx + 1; dst_idx < count; ++dst_idx)
			{
				LLHUDText* dstp = sVisibleTextObjects[dst_idx].get();
				if (!dstp->mUseBubble)
				{
					continue;
				}

				if (!srcp->mSoftScreenRect.overlaps(dstp->mSoftScreenRect))
				{
					continue;
				}

				LLRectf intersect_rect = srcp->mSoftScreenRect;
				intersect_rect.intersectWith(dstp->mSoftScreenRect);
				intersect_rect.stretch(-BUFFER_SIZE * 0.5f);

				F32 src_center_x = srcp->mSoftScreenRect.getCenterX();
				F32 src_center_y = srcp->mSoftScreenRect.getCenterY();
				F32 dst_center_x = dstp->mSoftScreenRect.getCenterX();
				F32 dst_center_y = dstp->mSoftScreenRect.getCenterY();
				F32 intersect_center_x = intersect_rect.getCenterX();
				F32 intersect_center_y = intersect_rect.getCenterY();
				LLVector2 force =
					lerp(LLVector2(dst_center_x - intersect_center_x,
								   dst_center_y - intersect_center_y),
						 LLVector2(intersect_center_x - src_center_x,
								   intersect_center_y - src_center_y), 0.5f);
				force.set(dst_center_x - src_center_x,
						  dst_center_y - src_center_y);
				force.normalize();

				LLVector2 src_force = -1.f * force;
				LLVector2 dst_force = force;

				LLVector2 force_strength;
				F32 src_mult = dstp->mMass / (dstp->mMass + srcp->mMass);
				F32 dst_mult = 1.f - src_mult;
				F32 src_aspect_ratio = srcp->mSoftScreenRect.getWidth() /
									   srcp->mSoftScreenRect.getHeight();
				F32 dst_aspect_ratio = dstp->mSoftScreenRect.getWidth() /
									   dstp->mSoftScreenRect.getHeight();
				src_force.mV[VY] *= src_aspect_ratio;
				src_force.normalize();
				dst_force.mV[VY] *= dst_aspect_ratio;
				dst_force.normalize();

				src_force.mV[VX] *=
					llmin(intersect_rect.getWidth() * src_mult,
						  intersect_rect.getHeight() * SPRING_STRENGTH);
				src_force.mV[VY] *=
					llmin(intersect_rect.getHeight() * src_mult,
						  intersect_rect.getWidth() * SPRING_STRENGTH);
				dst_force.mV[VX] *=
					llmin(intersect_rect.getWidth() * dst_mult,
						  intersect_rect.getHeight() * SPRING_STRENGTH);
				dst_force.mV[VY] *=
					llmin(intersect_rect.getHeight() * dst_mult,
						  intersect_rect.getWidth() * SPRING_STRENGTH);

				srcp->mTargetPositionOffset += src_force;
				dstp->mTargetPositionOffset += dst_force;
				srcp->mTargetPositionOffset =
					srcp->updateScreenPos(srcp->mTargetPositionOffset);
				dstp->mTargetPositionOffset =
					dstp->updateScreenPos(dstp->mTargetPositionOffset);
			}
		}
	}

	for (S32 i = 0; i < count; ++i)
	{
		LLHUDText* textp = sVisibleTextObjects[i].get();
		if (textp->mUseBubble)
		{
			textp->mPositionOffset =
				lerp(textp->mPositionOffset, textp->mTargetPositionOffset,
					 LLCriticalDamp::getInterpolant(POSITION_DAMPING_TC));
		}
	}
}

S32 LLHUDText::getMaxLines()
{
	switch (mLOD)
	{
		case 0:
			return mMaxLines;
		case 1:
			return mMaxLines > 0 ? mMaxLines / 2 : 5;
		case 2:
			return mMaxLines > 0 ? mMaxLines / 3 : 2;
		default:
			// Label only
			return 0;
	}
}

void LLHUDText::markDead()
{
	// Hold a pointer until LLHUDObject::markDead() is done with us.
	LLPointer<LLHUDText> self = LLPointer<LLHUDText>(this);
	sTextObjects.erase(self);
	LLHUDObject::markDead();
}

void LLHUDText::renderAllHUD()
{
	{
		LLGLDepthTest depth(GL_FALSE, GL_FALSE);

		for (S32 i = 0, count = sVisibleHUDTextObjects.size(); i < count; ++i)
		{
			LLHUDText* textp = sVisibleHUDTextObjects[i].get();
			textp->renderText();
		}
	}

	LLVertexBuffer::unbind();
	LL_GL_CHECK_STATES;
}

void LLHUDText::shiftAll(const LLVector3& offset)
{
	htobj_list_it_t text_it;
	htobj_list_it_t end = sTextObjects.end();
	for (text_it = sTextObjects.begin(); text_it != end; ++text_it)
	{
		LLHUDText* textp = text_it->get();
		textp->shift(offset);
	}
}

//static
void LLHUDText::addPickable(std::set<LLViewerObject*>& pick_list)
{
	// This might put an object on the pick list a second time, overriding its
	// mGLName, which is ok.
	// *TODO: we should probably cull against pick frustum.
	for (S32 i = 0, count = sVisibleTextObjects.size(); i < count; ++i)
	{
		LLHUDText* textp = sVisibleTextObjects[i].get();
		if (textp->mUseBubble)
		{
			pick_list.insert(textp->mSourceObject);
		}
	}
}

// Called when UI scale changes, to flush font width caches
//static
void LLHUDText::reshape()
{
	for (htobj_list_it_t t_it = sTextObjects.begin(),
						 t_end = sTextObjects.end(); t_it != t_end; ++t_it)
	{
		LLHUDText* textp = t_it->get();
		segments_vec_t& text_segs = textp->mTextSegments;
		for (U32 i = 0, count = text_segs.size(); i < count; ++i)
		{
			text_segs[i].clearFontWidthCache();
		}
		segments_vec_t& label_segs = textp->mLabelSegments;
		for (U32 i = 0, count = label_segs.size(); i < count; ++i)
		{
			label_segs[i].clearFontWidthCache();
		}
	}
}
