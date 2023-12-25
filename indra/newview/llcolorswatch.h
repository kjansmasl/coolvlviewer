/**
 * @file llcolorswatch.h
 * @brief LLColorSwatch class definition
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

#ifndef LL_LLCOLORSWATCH_H
#define LL_LLCOLORSWATCH_H

#include "llfloater.h"

#include "llviewertexture.h"

//
// Classes
//
class LLColor4;
class LLTextBox;
class LLFloaterColorPicker;

class LLColorSwatchCtrl final : public LLUICtrl
{
public:
	typedef enum e_color_pick_op
	{
		COLOR_CHANGE,
		COLOR_SELECT,
		COLOR_CANCEL
	} EColorPickOp;

	LLColorSwatchCtrl(const std::string& name, const LLRect& rect,
					  const LLColor4& color,
					  void (*on_commit_callback)(LLUICtrl*, void*),
					  void* callback_userdata);
	LLColorSwatchCtrl(const std::string& name, const LLRect& rect,
					  const std::string& label, const LLColor4& color,
					  void (*on_commit_callback)(LLUICtrl*, void*),
					  void* callback_userdata);

	~LLColorSwatchCtrl() override;

	void setValue(const LLSD& value) override;

	LL_INLINE LLSD getValue() const override			{ return mColor.getValue(); }
	LL_INLINE const LLColor4& get()						{ return mColor; }

	void set(const LLColor4& color, bool update_picker = false,
			 bool from_event = false);

	void setEnabled(bool enabled) override;

	void draw() override;

	void setOriginal(const LLColor4& color);
	void setValid(bool valid);
	void setLabel(const std::string& label);
	void setCanApplyImmediately(bool apply)				{ mCanApplyImmediately = apply; }
	void setOnCancelCallback(LLUICtrlCallback cb)		{ mOnCancelCallback = cb; }
	void setOnSelectCallback(LLUICtrlCallback cb)		{ mOnSelectCallback = cb; }
	void setFallbackImageName(const std::string& image_name);

	void showPicker(bool take_focus);

	bool handleMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleMouseUp(S32 x, S32 y, MASK mask) override;
	bool handleDoubleClick(S32 x,S32 y,MASK mask) override;
	bool handleHover(S32 x, S32 y, MASK mask) override;
	bool handleUnicodeCharHere(llwchar uni_char) override;

	LLXMLNodePtr getXML(bool save_children = true) const override;

	static LLView* fromXML(LLXMLNodePtr node, LLView* parent,
						   LLUICtrlFactory* factory);

	static void onColorChanged(void* data, EColorPickOp op = COLOR_CHANGE);

protected:
	LLPointer<LLViewerFetchedTexture>	mFallbackImage;
	LLPointer<LLUIImage>				mAlphaGradientImage;
	LLHandle<LLFloater>					mPickerHandle;
	LLUICtrlCallback					mOnCancelCallback;
	LLUICtrlCallback					mOnSelectCallback;
	LLColor4							mColor;
	LLColor4							mBorderColor;
	LLTextBox*							mCaption;
	LLViewBorder*						mBorder;
	bool								mValid;
	bool								mCanApplyImmediately;
};

#endif  // LL_LLBUTTON_H
