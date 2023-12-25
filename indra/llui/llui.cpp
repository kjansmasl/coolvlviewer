/**
 * @file llui.cpp
 * @brief General static UI services implementation
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

// Utilities functions the user interface needs

#include "linden_common.h"

#include <stack>

#include "llui.h"

#include "llcontrol.h"
#include "llconsole.h"
#include "lldir.h"
#include "lllineeditor.h"
#include "llwindow.h"
#include "llvector2.h"
#include "llcolor4.h"

//
// Globals
//
const LLColor4 UI_VERTEX_COLOR(1.f, 1.f, 1.f, 1.f);

// Used to hide the flashing text cursor when window doesn't have focus.
bool gShowTextEditCursor = true;

LLControlGroup* LLUI::sConfigGroup = NULL;
LLControlGroup* LLUI::sIgnoresGroup = NULL;
LLControlGroup* LLUI::sColorsGroup = NULL;
LLImageProviderInterface* LLUI::sImageProvider = NULL;
LLUIAudioCallback LLUI::sAudioCallback = NULL;
LLVector2 LLUI::sGLScaleFactor(1.f, 1.f);
LLHtmlHelp* LLUI::sHtmlHelp = NULL;
bool LLUI::sShowXUINames = false;
std::stack<LLRect> LLScreenClipRect::sClipRectStack;

LLUIImagePtr LLUIImage::sRoundedSquare;
S32 LLUIImage::sRoundedSquareWidth;
S32 LLUIImage::sRoundedSquareHeight;

S32 LLUI::sButtonFlashCount = 10;
F32 LLUI::sButtonFlashRate = 2.0f;
F32 LLUI::sColumnHeaderDropDownDelay = 0.3f;
bool LLUI::sConsoleBoxPerMessage = false;
bool LLUI::sDisableMessagesSpacing = true;
S32 LLUI::sDropShadowButton = 2;
S32 LLUI::sDropShadowFloater = 5;
S32 LLUI::sDropShadowTooltip = 4;
F32 LLUI::sMenuAccessKeyTime = 0.25f;
F32 LLUI::sPieMenuLineWidth = 2.5f;
S32 LLUI::sSnapMargin = 10;
F32 LLUI::sTypeAheadTimeout = 1.5f;
bool LLUI::sTabToTextFieldsOnly = false;
bool LLUI::sUseAltKeyForMenus = false;

LLColor4 LLUI::sAlertBoxColor;
LLColor4 LLUI::sAlertCautionBoxColor;
LLColor4 LLUI::sAlertCautionTextColor;
LLColor4 LLUI::sAlertTextColor;
LLColor4 LLUI::sButtonFlashBgColor;
LLColor4 LLUI::sButtonImageColor;
LLColor4 LLUI::sButtonLabelColor;
LLColor4 LLUI::sButtonLabelDisabledColor;
LLColor4 LLUI::sButtonLabelSelectedColor;
LLColor4 LLUI::sButtonLabelSelectedDisabledColor;
LLColor4 LLUI::sColorDropShadow;
LLColor4 LLUI::sDefaultBackgroundColor;
LLColor4 LLUI::sDefaultHighlightDark;
LLColor4 LLUI::sDefaultHighlightLight;
LLColor4 LLUI::sDefaultShadowDark;
LLColor4 LLUI::sDefaultShadowLight;
LLColor4 LLUI::sFloaterButtonImageColor;
LLColor4 LLUI::sFloaterFocusBorderColor;
LLColor4 LLUI::sFloaterUnfocusBorderColor;
LLColor4 LLUI::sFocusBackgroundColor;
LLColor4 LLUI::sHTMLLinkColor;
LLColor4 LLUI::sLabelDisabledColor;
LLColor4 LLUI::sLabelSelectedColor;
LLColor4 LLUI::sLabelTextColor;
LLColor4 LLUI::sLoginProgressBarBgColor;
LLColor4 LLUI::sMenuDefaultBgColor;
LLColor4 LLUI::sMultiSliderThumbCenterColor;
LLColor4 LLUI::sMultiSliderThumbCenterSelectedColor;
LLColor4 LLUI::sMultiSliderTrackColor;
LLColor4 LLUI::sMultiSliderTriangleColor;
LLColor4 LLUI::sPieMenuBgColor;
LLColor4 LLUI::sPieMenuLineColor;
LLColor4 LLUI::sPieMenuSelectedColor;
LLColor4 LLUI::sScrollbarThumbColor;
LLColor4 LLUI::sScrollbarTrackColor;
LLColor4 LLUI::sScrollBgReadOnlyColor;
LLColor4 LLUI::sScrollBGStripeColor;
LLColor4 LLUI::sScrollBgWriteableColor;
LLColor4 LLUI::sScrollDisabledColor;
LLColor4 LLUI::sScrollHighlightedColor;
LLColor4 LLUI::sScrollSelectedBGColor;
LLColor4 LLUI::sScrollSelectedFGColor;
LLColor4 LLUI::sScrollUnselectedColor;
LLColor4 LLUI::sSliderThumbCenterColor;
LLColor4 LLUI::sSliderThumbOutlineColor;
LLColor4 LLUI::sSliderTrackColor;
LLColor4 LLUI::sTextBgFocusColor;
LLColor4 LLUI::sTextBgReadOnlyColor;
LLColor4 LLUI::sTextBgWriteableColor;
LLColor4 LLUI::sTextCursorColor;
LLColor4 LLUI::sTextDefaultColor;
LLColor4 LLUI::sTextEmbeddedItemColor;
LLColor4 LLUI::sTextEmbeddedItemReadOnlyColor;
LLColor4 LLUI::sTextFgColor;
LLColor4 LLUI::sTextFgReadOnlyColor;
LLColor4 LLUI::sTextFgTentativeColor;
LLColor4 LLUI::sTitleBarFocusColor;
LLColor4 LLUI::sTrackColor;
LLColor4 LLUI::sDisabledTrackColor;

//
// Functions
//
void make_ui_sound(const char* namep, bool force)
{
	std::string name = ll_safe_string(namep);
	if (!LLUI::sConfigGroup->controlExists(name.c_str()))
	{
		llwarns << "tried to make UI sound for unknown sound name: " << name
				<< llendl;
		return;
	}
	std::string flagname = name + "Enable";
	if (force || !LLUI::sConfigGroup->controlExists(flagname.c_str()) ||
		LLUI::sConfigGroup->getBool(flagname.c_str()))
	{
		LLUUID uuid(LLUI::sConfigGroup->getString(name.c_str()));
		if (uuid.isNull())
		{
			if (LLUI::sConfigGroup->getString(name.c_str()) == LLUUID::null.asString())
			{
				LL_DEBUGS("UISounds") << "UI sound name: " << name
									  << " triggered but silent (null uuid)"
									  << LL_ENDL;
			}
			else
			{
				llwarns << "UI sound named: " << name
						<< " does not translate into a valid uuid" << llendl;
			}

		}
		else if (LLUI::sAudioCallback != NULL)
		{
			LL_DEBUGS("UISounds") << "UI sound name: " << name << LL_ENDL;
			LLUI::sAudioCallback(uuid);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
// UI-specific rendering functions that cannot go into llrenderutils.cpp
// since they refer to llui stuff

void gl_rect_2d_offset_local(S32 left, S32 top, S32 right, S32 bottom,
							 S32 pixel_offset, bool filled)
{
	gGL.pushUIMatrix();
	left += LLFontGL::sCurOrigin.mX;
	right += LLFontGL::sCurOrigin.mX;
	bottom += LLFontGL::sCurOrigin.mY;
	top += LLFontGL::sCurOrigin.mY;

	gGL.loadUIIdentity();
	gl_rect_2d(llfloor((F32)left * LLUI::sGLScaleFactor.mV[VX]) - pixel_offset,
			   llfloor((F32)top * LLUI::sGLScaleFactor.mV[VY]) + pixel_offset,
			   llfloor((F32)right * LLUI::sGLScaleFactor.mV[VX]) + pixel_offset,
			   llfloor((F32)bottom * LLUI::sGLScaleFactor.mV[VY]) - pixel_offset,
			   filled);
	gGL.popUIMatrix();
}

void gl_line_3d(const LLVector3& start, const LLVector3& end,
				const LLColor4& color, F32 phase)
{
	phase = fmod(phase, 1.f);

	gGL.color4f(color.mV[VRED], color.mV[VGREEN], color.mV[VBLUE],
				color.mV[VALPHA]);

	gGL.flush();
	gGL.lineWidth(2.5f);

	gGL.begin(LLRender::LINES);
	{
		gGL.vertex3fv(start.mV);
		gGL.vertex3fv(end.mV);
	}
	gGL.end();

	LLUI::setLineWidth(1.f);
}

// Draw gray and white checkerboard with black border
void gl_rect_2d_checkerboard(const LLRect& rect)
{
	LLTexUnit* unit0 = gGL.getTexUnit(0);

	// Polygon stipple is deprecated, use the checker.png texture
	static LLUIImagePtr img = LLUI::getUIImage("checker.png");
	unit0->bind(img->getImage());
	unit0->setTextureAddressMode(LLTexUnit::TAM_WRAP);
	unit0->setTextureFilteringOption(LLTexUnit::TFO_POINT);

	F32 width = rect.getWidth();
	F32 height = rect.getHeight();
	F32 scaler = width <= 32.f || height <= 32.f ? 1.f / 16.f : 1.f / 32.f;
	LLRectf uv_rect(0.f, 0.f, scaler * width, scaler * height);
	gl_draw_scaled_image(rect.mLeft, rect.mBottom, rect.getWidth(),
						 rect.getHeight(), img->getImage(), LLColor4::white,
						 uv_rect);
}

///////////////////////////////////////////////////////////////////////////////

//static
bool handleShowXUINamesChanged(const LLSD& newvalue)
{
	LLUI::sShowXUINames = newvalue.asBoolean();
	return true;
}

//static
void LLUI::initClass(LLControlGroup* config,
					 LLControlGroup* ignores,
					 LLControlGroup* colors,
					 LLImageProviderInterface* image_provider,
					 LLUIAudioCallback audio_callback,
					 const LLVector2* scale_factor,
					 const std::string& language)
{
	sConfigGroup = config;
	sIgnoresGroup = ignores;
	sColorsGroup = colors;

	if (sConfigGroup == NULL || sIgnoresGroup == NULL || sColorsGroup == NULL)
	{
		llerrs << "Failure to initialize configuration groups" << llendl;
	}

	sImageProvider = image_provider;
	sAudioCallback = audio_callback;
	sGLScaleFactor = scale_factor ? *scale_factor : LLVector2(1.f, 1.f);

	sShowXUINames = sConfigGroup->getBool("ShowXUINames");
	LLControlVariable* controlp = sConfigGroup->getControl("ShowXUINames");
	if (controlp)
	{
		controlp->getSignal()->connect(boost::bind(&handleShowXUINamesChanged,
												   _2));
	}

	connectRefreshSettingsSafe("ButtonFlashCount");
	connectRefreshSettingsSafe("ButtonFlashRate");
	connectRefreshSettingsSafe("ColumnHeaderDropDownDelay");
	connectRefreshSettingsSafe("ConsoleBoxPerMessage");
	connectRefreshSettingsSafe("DisableMessagesSpacing");
	connectRefreshSettingsSafe("DropShadowButton");
	connectRefreshSettingsSafe("DropShadowFloater");
	connectRefreshSettingsSafe("DropShadowTooltip");
	connectRefreshSettingsSafe("HTMLLinkColor");
	connectRefreshSettingsSafe("MenuAccessKeyTime");
	connectRefreshSettingsSafe("PieMenuLineWidth");
	connectRefreshSettingsSafe("SnapMargin");
	connectRefreshSettingsSafe("TabToTextFieldsOnly");
	connectRefreshSettingsSafe("TypeAheadTimeout");
	connectRefreshSettingsSafe("UseAltKeyForMenus");

	connectRefreshSettingsSafe("ColorDropShadow");
	connectRefreshSettingsSafe("AlertBoxColor");
	connectRefreshSettingsSafe("AlertCautionBoxColor");
	connectRefreshSettingsSafe("AlertCautionTextColor");
	connectRefreshSettingsSafe("AlertTextColor");
	connectRefreshSettingsSafe("ButtonFlashBgColor");
	connectRefreshSettingsSafe("ButtonImageColor");
	connectRefreshSettingsSafe("ButtonLabelColor");
	connectRefreshSettingsSafe("ButtonLabelDisabledColor");
	connectRefreshSettingsSafe("ButtonLabelSelectedColor");
	connectRefreshSettingsSafe("ButtonLabelSelectedDisabledColor");
	connectRefreshSettingsSafe("ColorDropShadow");
	connectRefreshSettingsSafe("DefaultBackgroundColor");
	connectRefreshSettingsSafe("DefaultHighlightDark");
	connectRefreshSettingsSafe("DefaultHighlightLight");
	connectRefreshSettingsSafe("DefaultShadowDark");
	connectRefreshSettingsSafe("DefaultShadowLight");
	connectRefreshSettingsSafe("FloaterButtonImageColor");
	connectRefreshSettingsSafe("FloaterFocusBorderColor");
	connectRefreshSettingsSafe("FloaterUnfocusBorderColor");
	connectRefreshSettingsSafe("FocusBackgroundColor");
	connectRefreshSettingsSafe("LabelDisabledColor");
	connectRefreshSettingsSafe("LabelSelectedColor");
	connectRefreshSettingsSafe("LabelTextColor");
	connectRefreshSettingsSafe("LoginProgressBarBgColor");
	connectRefreshSettingsSafe("MenuDefaultBgColor");
	connectRefreshSettingsSafe("MultiSliderThumbCenterColor");
	connectRefreshSettingsSafe("MultiSliderThumbCenterSelectedColor");
	connectRefreshSettingsSafe("MultiSliderTrackColor");
	connectRefreshSettingsSafe("MultiSliderTriangleColor");
	connectRefreshSettingsSafe("PieMenuBgColor");
	connectRefreshSettingsSafe("PieMenuLineColor");
	connectRefreshSettingsSafe("PieMenuSelectedColor");
	connectRefreshSettingsSafe("ScrollbarThumbColor");
	connectRefreshSettingsSafe("ScrollbarTrackColor");
	connectRefreshSettingsSafe("ScrollBgReadOnlyColor");
	connectRefreshSettingsSafe("ScrollBGStripeColor");
	connectRefreshSettingsSafe("ScrollBgWriteableColor");
	connectRefreshSettingsSafe("ScrollDisabledColor");
	connectRefreshSettingsSafe("ScrollHighlightedColor");
	connectRefreshSettingsSafe("ScrollSelectedBGColor");
	connectRefreshSettingsSafe("ScrollSelectedFGColor");
	connectRefreshSettingsSafe("ScrollUnselectedColor");
	connectRefreshSettingsSafe("SliderThumbCenterColor");
	connectRefreshSettingsSafe("SliderThumbOutlineColor");
	connectRefreshSettingsSafe("SliderTrackColor");
	connectRefreshSettingsSafe("TextBgFocusColor");
	connectRefreshSettingsSafe("TextBgReadOnlyColor");
	connectRefreshSettingsSafe("TextBgWriteableColor");
	connectRefreshSettingsSafe("TextCursorColor");
	connectRefreshSettingsSafe("TextDefaultColor");
	connectRefreshSettingsSafe("TextEmbeddedItemColor");
	connectRefreshSettingsSafe("TextEmbeddedItemReadOnlyColor");
	connectRefreshSettingsSafe("TextFgColor");
	connectRefreshSettingsSafe("TextFgReadOnlyColor");
	connectRefreshSettingsSafe("TextFgTentativeColor");
	connectRefreshSettingsSafe("TitleBarFocusColor");
	connectRefreshSettingsSafe("TrackColor");
	connectRefreshSettingsSafe("DisabledTrackColor");
	connectRefreshSettingsSafe("ConsoleBackground");
	connectRefreshSettingsSafe("ConsoleBackgroundOpacity");

	refreshSettings();
}

//static
void LLUI::connectRefreshSettingsSafe(const char* name)
{
	LLControlVariable* controlp = sConfigGroup->getControl(name);
	if (!controlp)
	{
		controlp = sColorsGroup->getControl(name);
	}
	if (!controlp)
	{
		llwarns << "Setting name not found: " << name << llendl;
		return;
	}
	controlp->getSignal()->connect(boost::bind(&LLUI::refreshSettings));
}

//static
void LLUI::refreshSettings()
{
	sButtonFlashCount = sConfigGroup->getS32("ButtonFlashCount");
	sButtonFlashRate = sConfigGroup->getF32("ButtonFlashRate");
	sColumnHeaderDropDownDelay =
		sConfigGroup->getF32("ColumnHeaderDropDownDelay");
	sConsoleBoxPerMessage = sConfigGroup->getBool("ConsoleBoxPerMessage");
	sDisableMessagesSpacing = sConfigGroup->getBool("DisableMessagesSpacing");
	sDropShadowButton = sConfigGroup->getS32("DropShadowButton");
	sDropShadowFloater = sConfigGroup->getS32("DropShadowFloater");
	sDropShadowTooltip = sConfigGroup->getS32("DropShadowTooltip");
	sHTMLLinkColor = LLUI::sConfigGroup->getColor4("HTMLLinkColor");
	sMenuAccessKeyTime = sConfigGroup->getF32("MenuAccessKeyTime");
	sPieMenuLineWidth = sConfigGroup->getF32("PieMenuLineWidth");
	sSnapMargin = sConfigGroup->getS32("SnapMargin");
	sTabToTextFieldsOnly = sConfigGroup->getBool("TabToTextFieldsOnly");
	sTypeAheadTimeout = sConfigGroup->getF32("TypeAheadTimeout");
	sUseAltKeyForMenus = sConfigGroup->getBool("UseAltKeyForMenus");

	LLFontGL::sShadowColor = sColorsGroup->getColor("ColorDropShadow");
	// We do the conversion here, once and for all, for speed
	LLFontGL::sShadowColorU = LLColor4U(LLFontGL::sShadowColor);

	sAlertBoxColor = sColorsGroup->getColor("AlertBoxColor");
	sAlertCautionBoxColor = sColorsGroup->getColor("AlertCautionBoxColor");
	sAlertCautionTextColor = sColorsGroup->getColor("AlertCautionTextColor");
	sAlertTextColor = sColorsGroup->getColor("AlertTextColor");
	sButtonFlashBgColor = sColorsGroup->getColor("ButtonFlashBgColor");
	sButtonImageColor = sColorsGroup->getColor("ButtonImageColor");
	sButtonLabelColor = sColorsGroup->getColor("ButtonLabelColor");
	sButtonLabelDisabledColor =
		sColorsGroup->getColor("ButtonLabelDisabledColor");
	sButtonLabelSelectedColor =
		sColorsGroup->getColor("ButtonLabelSelectedColor");
	sButtonLabelSelectedDisabledColor =
		sColorsGroup->getColor("ButtonLabelSelectedDisabledColor");
	sColorDropShadow = sColorsGroup->getColor("ColorDropShadow");
	sDefaultBackgroundColor = sColorsGroup->getColor("DefaultBackgroundColor");
	sDefaultHighlightDark = sColorsGroup->getColor("DefaultHighlightDark");
	sDefaultHighlightLight = sColorsGroup->getColor("DefaultHighlightLight");
	sDefaultShadowDark = sColorsGroup->getColor("DefaultShadowDark");
	sDefaultShadowLight = sColorsGroup->getColor("DefaultShadowLight");
	sFloaterButtonImageColor =
		sColorsGroup->getColor("FloaterButtonImageColor");
	sFloaterFocusBorderColor =
		sColorsGroup->getColor("FloaterFocusBorderColor");
	sFloaterUnfocusBorderColor =
		sColorsGroup->getColor("FloaterUnfocusBorderColor");
	sFocusBackgroundColor = sColorsGroup->getColor("FocusBackgroundColor");
	sLabelDisabledColor = sColorsGroup->getColor("LabelDisabledColor");
	sLabelSelectedColor = sColorsGroup->getColor("LabelSelectedColor");
	sLabelTextColor = sColorsGroup->getColor("LabelTextColor");
	sLoginProgressBarBgColor =
		sColorsGroup->getColor("LoginProgressBarBgColor");
	sMenuDefaultBgColor = sColorsGroup->getColor("MenuDefaultBgColor");
	sMultiSliderThumbCenterColor =
		sColorsGroup->getColor("MultiSliderThumbCenterColor");
	sMultiSliderThumbCenterSelectedColor =
		sColorsGroup->getColor("MultiSliderThumbCenterSelectedColor");
	sMultiSliderTrackColor = sColorsGroup->getColor("MultiSliderTrackColor");
	sMultiSliderTriangleColor =
		sColorsGroup->getColor("MultiSliderTriangleColor");
	sPieMenuBgColor = sColorsGroup->getColor("PieMenuBgColor");
	sPieMenuLineColor = sColorsGroup->getColor("PieMenuLineColor");
	sPieMenuSelectedColor = sColorsGroup->getColor("PieMenuSelectedColor");
	sScrollbarThumbColor = sColorsGroup->getColor("ScrollbarThumbColor");
	sScrollbarTrackColor = sColorsGroup->getColor("ScrollbarTrackColor");
	sScrollBgReadOnlyColor = sColorsGroup->getColor("ScrollBgReadOnlyColor");
	sScrollBGStripeColor = sColorsGroup->getColor("ScrollBGStripeColor");
	sScrollBgWriteableColor = sColorsGroup->getColor("ScrollBgWriteableColor");
	sScrollDisabledColor = sColorsGroup->getColor("ScrollDisabledColor");
	sScrollHighlightedColor = sColorsGroup->getColor("ScrollHighlightedColor");
	sScrollSelectedBGColor = sColorsGroup->getColor("ScrollSelectedBGColor");
	sScrollSelectedFGColor = sColorsGroup->getColor("ScrollSelectedFGColor");
	sScrollUnselectedColor = sColorsGroup->getColor("ScrollUnselectedColor");
	sSliderThumbCenterColor = sColorsGroup->getColor("SliderThumbCenterColor");
	sSliderThumbOutlineColor =
		sColorsGroup->getColor("SliderThumbOutlineColor");
	sSliderTrackColor = sColorsGroup->getColor("SliderTrackColor");
	sTextBgFocusColor = sColorsGroup->getColor("TextBgFocusColor");
	sTextBgReadOnlyColor = sColorsGroup->getColor("TextBgReadOnlyColor");
	sTextBgWriteableColor = sColorsGroup->getColor("TextBgWriteableColor");
	sTextCursorColor = sColorsGroup->getColor("TextCursorColor");
	sTextDefaultColor = sColorsGroup->getColor("TextDefaultColor");
	sTextEmbeddedItemColor = sColorsGroup->getColor("TextEmbeddedItemColor");
	sTextEmbeddedItemReadOnlyColor =
		sColorsGroup->getColor("TextEmbeddedItemReadOnlyColor");
	sTextFgColor = sColorsGroup->getColor("TextFgColor");
	sTextFgReadOnlyColor = sColorsGroup->getColor("TextFgReadOnlyColor");
	sTextFgTentativeColor = sColorsGroup->getColor("TextFgTentativeColor");
	sTitleBarFocusColor = sColorsGroup->getColor("TitleBarFocusColor");
	sTrackColor = sColorsGroup->getColor("TrackColor");
	sDisabledTrackColor = sColorsGroup->getColor("DisabledTrackColor");
	LLConsole::setBackground(sColorsGroup->getColor("ConsoleBackground"),
							 llclamp(sConfigGroup->getF32("ConsoleBackgroundOpacity"),
									 0.f, 1.f));
}

//static
void LLUI::cleanupClass()
{
	sImageProvider->cleanUp();
	LLLineEditor::cleanupLineEditor();
}

//static
void LLUI::translate(F32 x, F32 y, F32 z)
{
	gGL.translateUI(x, y, z);
	LLFontGL::sCurOrigin.mX += (S32) x;
	LLFontGL::sCurOrigin.mY += (S32) y;
	LLFontGL::sCurDepth += z;
}

//static
void LLUI::pushMatrix()
{
	gGL.pushUIMatrix();
	LLFontGL::sOriginStack.emplace_back(LLFontGL::sCurOrigin,
										LLFontGL::sCurDepth);
}

//static
void LLUI::popMatrix()
{
	gGL.popUIMatrix();
	LLFontGL::sCurOrigin = LLFontGL::sOriginStack.back().first;
	LLFontGL::sCurDepth = LLFontGL::sOriginStack.back().second;
	LLFontGL::sOriginStack.pop_back();
}

//static
void LLUI::loadIdentity()
{
	gGL.loadUIIdentity();
	LLFontGL::sCurOrigin.mX = 0;
	LLFontGL::sCurOrigin.mY = 0;
	LLFontGL::sCurDepth = 0.f;
}

//static
void LLUI::setLineWidth(F32 width)
{
	gGL.flush();
	gGL.lineWidth(width *
				  lerp(sGLScaleFactor.mV[VX], sGLScaleFactor.mV[VY], 0.5f));
}

//static
void LLUI::setCursorPositionScreen(S32 x, S32 y)
{
	if (!gWindowp) return;

#if LL_DARWIN
	F32 sys_size_factor = gWindowp->getSystemUISize();
	S32 screen_x = ll_round((F32)x * sGLScaleFactor.mV[VX] / sys_size_factor);
	S32 screen_y = ll_round((F32)y * sGLScaleFactor.mV[VY] / sys_size_factor);
#else
	S32 screen_x = ll_round((F32)x * sGLScaleFactor.mV[VX]);
	S32 screen_y = ll_round((F32)y * sGLScaleFactor.mV[VY]);
#endif

	LLCoordWindow window_point;
	gWindowp->convertCoords(LLCoordGL(screen_x, screen_y), &window_point);

	gWindowp->setCursorPosition(window_point);
}

//static
void LLUI::setCursorPositionLocal(const LLView* viewp, S32 x, S32 y)
{
	S32 screen_x, screen_y;
	viewp->localPointToScreen(x, y, &screen_x, &screen_y);

	setCursorPositionScreen(screen_x, screen_y);
}

//static
void LLUI::getCursorPositionLocal(const LLView* viewp, S32* x, S32* y)
{
	if (!gWindowp) return;

	LLCoordWindow cursor_pos_window;
	gWindowp->getCursorPosition(&cursor_pos_window);
	LLCoordGL cursor_pos_gl;
	gWindowp->convertCoords(cursor_pos_window, &cursor_pos_gl);
	cursor_pos_gl.mX = ll_round((F32)cursor_pos_gl.mX / LLUI::sGLScaleFactor.mV[VX]);
	cursor_pos_gl.mY = ll_round((F32)cursor_pos_gl.mY / LLUI::sGLScaleFactor.mV[VY]);
	viewp->screenPointToLocal(cursor_pos_gl.mX, cursor_pos_gl.mY, x, y);
}

//static
std::string LLUI::getLanguage()
{
	std::string language = "en-us";
	if (sConfigGroup)
	{
		language = sConfigGroup->getString("Language");
		if (language.empty() || language == "default")
		{
			language = sConfigGroup->getString("SystemLanguage");
		}
		if (language.empty() || language == "default")
		{
			language = "en-us";
		}
	}
	return language;
}

//static
std::string LLUI::locateSkin(const std::string& filename)
{
	std::string found_file = filename;
	if (!LLFile::exists(found_file))
	{
		// Should be CUSTOM_SKINS ?
		found_file = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS,
													filename);
	}
	if (sConfigGroup && sConfigGroup->controlExists("Language"))
	{
		if (!LLFile::exists(found_file))
		{
			std::string localization = getLanguage();
			std::string local_skin = "xui" LL_DIR_DELIM_STR;
			local_skin += localization;
			local_skin += LL_DIR_DELIM_STR;
			local_skin +=  filename;
			found_file = gDirUtilp->findSkinnedFilename(local_skin);
		}
	}
	if (!LLFile::exists(found_file))
	{
		std::string local_skin = "xui" LL_DIR_DELIM_STR "en-us" LL_DIR_DELIM_STR;
		local_skin += filename;
		found_file = gDirUtilp->findSkinnedFilename(local_skin);
	}
	if (!LLFile::exists(found_file))
	{
		found_file = gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS,
													filename);
	}
	return found_file;
}

//static
LLVector2 LLUI::getWindowSize()
{
	LLCoordWindow window_rect;
	gWindowp->getSize(&window_rect);

	return LLVector2(window_rect.mX / sGLScaleFactor.mV[VX],
					 window_rect.mY / sGLScaleFactor.mV[VY]);
}

//static
void LLUI::screenPointToGL(S32 screen_x, S32 screen_y, S32* gl_x, S32* gl_y)
{
	*gl_x = ll_round((F32)screen_x * sGLScaleFactor.mV[VX]);
	*gl_y = ll_round((F32)screen_y * sGLScaleFactor.mV[VY]);
}

//static
void LLUI::glPointToScreen(S32 gl_x, S32 gl_y, S32* screen_x, S32* screen_y)
{
	*screen_x = ll_round((F32)gl_x / sGLScaleFactor.mV[VX]);
	*screen_y = ll_round((F32)gl_y / sGLScaleFactor.mV[VY]);
}

//static
void LLUI::screenRectToGL(const LLRect& screen, LLRect* gl)
{
	screenPointToGL(screen.mLeft, screen.mTop, &gl->mLeft, &gl->mTop);
	screenPointToGL(screen.mRight, screen.mBottom, &gl->mRight, &gl->mBottom);
}

//static
void LLUI::glRectToScreen(const LLRect& gl, LLRect *screen)
{
	glPointToScreen(gl.mLeft, gl.mTop, &screen->mLeft, &screen->mTop);
	glPointToScreen(gl.mRight, gl.mBottom, &screen->mRight, &screen->mBottom);
}

//static
LLUIImagePtr LLUI::getUIImageByID(const LLUUID& image_id)
{
	if (!sImageProvider)
	{
		return NULL;
	}
	return sImageProvider->getUIImageByID(image_id);
}

//static
LLUIImagePtr LLUI::getUIImage(const std::string& name)
{
	if (!sImageProvider || name.empty())
	{
		return NULL;
	}
	return sImageProvider->getUIImage(name);
}

//static
void LLUI::setHtmlHelp(LLHtmlHelp* html_help)
{
	LLUI::sHtmlHelp = html_help;
}

LLScreenClipRect::LLScreenClipRect(const LLRect& rect, bool enabled)
:	mScissorState(GL_SCISSOR_TEST),
	mEnabled(enabled)
{
	if (mEnabled)
	{
		pushClipRect(rect);
	}
	mScissorState.setEnabled(!sClipRectStack.empty());
	updateScissorRegion();
}

LLScreenClipRect::~LLScreenClipRect()
{
	if (mEnabled)
	{
		popClipRect();
	}
	updateScissorRegion();
}

//static
void LLScreenClipRect::pushClipRect(const LLRect& rect)
{
	LLRect combined_clip_rect = rect;
	if (!sClipRectStack.empty())
	{
		LLRect top = sClipRectStack.top();
		combined_clip_rect.intersectWith(top);
	}
	sClipRectStack.push(combined_clip_rect);
}

//static
void LLScreenClipRect::popClipRect()
{
	sClipRectStack.pop();
}

//static
void LLScreenClipRect::updateScissorRegion()
{
	if (sClipRectStack.empty()) return;

	LLRect rect = sClipRectStack.top();
	S32 x, y , w, h;
	x = llfloor(rect.mLeft * LLUI::sGLScaleFactor.mV[VX]);
	y = llfloor(rect.mBottom * LLUI::sGLScaleFactor.mV[VY]);
	w = llmax(0, llceil(rect.getWidth() * LLUI::sGLScaleFactor.mV[VX])) + 1;
	h = llmax(0, llceil(rect.getHeight() * LLUI::sGLScaleFactor.mV[VY])) + 1;
	glScissor(x, y, w, h);
	stop_glerror();
}

LLLocalClipRect::LLLocalClipRect(const LLRect &rect, bool enabled)
:	LLScreenClipRect(LLRect(rect.mLeft + LLFontGL::sCurOrigin.mX,
							rect.mTop + LLFontGL::sCurOrigin.mY,
							rect.mRight + LLFontGL::sCurOrigin.mX,
							rect.mBottom + LLFontGL::sCurOrigin.mY),
					 enabled)
{
}

//
// LLUIImage
//

//static
void LLUIImage::initClass()
{
	sRoundedSquare = LLUI::getUIImage("rounded_square.tga");
	if (sRoundedSquare.isNull())
	{
		llerrs << "Failure to find rounded_square.tga" << llendl;
	}
	sRoundedSquareWidth = sRoundedSquare->getTextureWidth();
	sRoundedSquareHeight = sRoundedSquare->getTextureHeight();
}

//static
void LLUIImage::cleanupClass()
{
	sRoundedSquare = NULL;
}

LLUIImage::LLUIImage(const std::string& name, LLPointer<LLGLTexture> image)
:	mName(name),
	mImage(image),
	mScaleRegion(0.f, 1.f, 1.f, 0.f),
	mClipRegion(0.f, 1.f, 1.f, 0.f),
	mUniformScaling(true),
	mNoClip(true)
{
}

void LLUIImage::setClipRegion(const LLRectf& region)
{
	mClipRegion = region;
	mNoClip = mClipRegion.mLeft == 0.f && mClipRegion.mRight == 1.f &&
			  mClipRegion.mBottom == 0.f && mClipRegion.mTop == 1.f;
}

void LLUIImage::setScaleRegion(const LLRectf& region)
{
	mScaleRegion = region;
	mUniformScaling = mScaleRegion.mLeft == 0.f && mScaleRegion.mRight == 1.f &&
					  mScaleRegion.mBottom == 0.f && mScaleRegion.mTop == 1.f;
}

// *TODO: move drawing implementation inside class
void LLUIImage::draw(S32 x, S32 y, const LLColor4& color) const
{
#if 0
	gl_draw_scaled_image(x, y, getWidth(), getHeight(), mImage, color,
						 mClipRegion);
#endif
	gl_draw_image(x, y, mImage, color, mClipRegion);
}

void LLUIImage::draw(S32 x, S32 y, S32 width, S32 height,
					 const LLColor4& color) const
{
	if (mUniformScaling)
	{
		gl_draw_scaled_image(x, y, width, height, mImage, color, mClipRegion);
	}
	else
	{
		gl_draw_scaled_image_with_border(x, y, width, height, mImage, color,
										 false, mClipRegion, mScaleRegion);
	}
}

void LLUIImage::drawSolid(S32 x, S32 y, S32 width, S32 height,
						  const LLColor4& color) const
{
	gl_draw_scaled_image_with_border(x, y, width, height, mImage, color, true,
									 mClipRegion, mScaleRegion);
}

void LLUIImage::drawBorder(S32 x, S32 y, S32 width, S32 height,
						   const LLColor4& color, S32 border_width) const
{
	LLRect border_rect;
	border_rect.setOriginAndSize(x, y, width, height);
	border_rect.stretch(border_width, border_width);
	drawSolid(border_rect, color);
}

S32 LLUIImage::getWidth() const
{
	// return clipped dimensions of actual image area
	return ll_roundp((F32)mImage->getWidth(0) * mClipRegion.getWidth());
}

S32 LLUIImage::getHeight() const
{
	// return clipped dimensions of actual image area
	return ll_roundp((F32)mImage->getHeight(0) * mClipRegion.getHeight());
}

S32 LLUIImage::getTextureWidth() const
{
	return mImage->getWidth(0);
}

S32 LLUIImage::getTextureHeight() const
{
	return mImage->getHeight(0);
}
