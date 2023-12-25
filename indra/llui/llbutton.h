/**
 * @file llbutton.h
 * @brief Header for buttons
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

#ifndef LL_LLBUTTON_H
#define LL_LLBUTTON_H

#include "llcontrol.h"
#include "llframetimer.h"
#include "llimage.h"
#include "lluictrl.h"
#include "lluistring.h"
#include "lluuid.h"
#include "llcolor4.h"

//
// Constants
//

// PLEASE please use these "constants" when building your own buttons. They are
// loaded from settings.xml at run time.
extern S32 gButtonHPad;
extern S32 gButtonVPad;
extern S32 gBtnHeightSmall;
extern S32 gBtnHeight;

//
// Helpful functions
//
S32 round_up(S32 grid, S32 value);

class LLUICtrlFactory;

//
// Classes
//

class LLButton : public LLUICtrl
{
protected:
	LOG_CLASS(LLButton);

public:
	// Simple button with text label
	LLButton(const std::string& name, const LLRect& rect,
			 const char* control_name = NULL,
			 void (*click_callback)(void*) = NULL,
			 void* callback_data = NULL);

	LLButton(const std::string& name, const LLRect& rect,
			 const std::string& unselected_image,
			 const std::string& selected_image, const char* control_name,
			 void (*click_callback)(void*), void* callback_data = NULL,
			 const LLFontGL* mGLFont = NULL,
			 const std::string& unselected_label = LLStringUtil::null,
			 const std::string& selected_label = LLStringUtil::null);

	~LLButton() override;
	void init(void (*click_callback)(void*), void* callback_data,
			  const LLFontGL* font, const char* control_name);

	void addImageAttributeToXML(LLXMLNodePtr node,
								const std::string& image_name,
								const LLUUID& imageID,
								const std::string& xml_tag_name) const;
	LLXMLNodePtr getXML(bool save_children = true) const override;
	static LLView* fromXML(LLXMLNodePtr node, LLView* parent,
						   LLUICtrlFactory* factory);

	bool handleUnicodeCharHere(llwchar uni_char) override;
	bool handleKeyHere(KEY key, MASK mask) override;
	bool handleMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleMouseUp(S32 x, S32 y, MASK mask) override;
	bool handleHover(S32 x, S32 y, MASK mask) override;
	void draw() override;

	void onMouseCaptureLost() override;

	void onCommit() override;

	LL_INLINE void setUnselectedLabelColor(const LLColor4& c)		{ mUnselectedLabelColor = c; }
	LL_INLINE void setSelectedLabelColor(const LLColor4& c)			{ mSelectedLabelColor = c; }
	// Mouse down and up within button:
	void setClickedCallback(void (*cb)(void*), void* data = NULL);

	// Mouse hovering within button
	LL_INLINE void setMouseHoverCallback(void (*cb)(void*))			{ mMouseHoverCallback = cb; }
	// Mouse down within button
	LL_INLINE void setMouseDownCallback(void (*cb)(void*))			{ mMouseDownCallback = cb; }
	// Mouse up, EVEN IF NOT IN BUTTON
	LL_INLINE void setMouseUpCallback(void (*cb)(void*))			{ mMouseUpCallback = cb; }
	// Mouse button held down and in button
	LL_INLINE void setHeldDownCallback(void (*cb)(void*))			{ mHeldDownCallback = cb; }

	LL_INLINE void setHeldDownDelay(F32 seconds, S32 frames = 0)
	{
		mHeldDownDelay = seconds;
		mHeldDownFrameDelay = frames;
	}

	LL_INLINE F32 getHeldDownTime() const							{ return mMouseDownTimer.getElapsedTimeF32(); }

	LL_INLINE bool getIsToggle() const								{ return mIsToggle; }
	LL_INLINE void setIsToggle(bool is_toggle)						{ mIsToggle = is_toggle; }
	bool toggleState();
	LL_INLINE bool getToggleState() const							{ return mToggleState; }
	void setToggleState(bool b);

	void setHighlight(bool b)										{ mNeedsHighlight = b; }

	void setFlashing(bool b);
	LL_INLINE bool getFlashing() const								{ return mFlashing; }

	LL_INLINE void setHAlign(LLFontGL::HAlign align)				{ mHAlign = align; }
	LL_INLINE LLFontGL::HAlign getHAlign() const					{ return mHAlign; }
	LL_INLINE void setLeftHPad(S32 pad)								{ mLeftHPad = pad; }
	LL_INLINE void setRightHPad(S32 pad)							{ mRightHPad = pad; }

	LL_INLINE const std::string getLabelUnselected() const			{ return wstring_to_utf8str(mUnselectedLabel); }
	LL_INLINE const std::string getLabelSelected() const			{ return wstring_to_utf8str(mSelectedLabel); }

	LL_INLINE const std::string getCurrentLabel() const
	{
		return mToggleState ? getLabelSelected() : getLabelUnselected();
	}

	void setImageColor(const std::string& color_control);
	void setImageColor(const LLColor4& c);
	void setColor(const LLColor4& c) override;
	void setAlpha(F32 alpha) override;

	void setImages(const std::string& image_name,
				   const std::string& selected_name);
	// Sets both selected and unselected images to image_name
	void setImages(const std::string& image_name);

	void setDisabledImages(const std::string& image_name,
						   const std::string& selected_name);
	void setDisabledImages(const std::string& image_name,
						   const std::string& selected_name,
						   const LLColor4& c);

	void setHoverImages(const std::string& image_name,
						const std::string& selected_name);

	LL_INLINE void setDisabledImageColor(const LLColor4& c)			{ mDisabledImageColor = c; }

	LL_INLINE void setDisabledSelectedLabelColor(const LLColor4& c)	{ mDisabledSelectedLabelColor = c; }

	void setImageOverlay(const std::string& image_name,
						 LLFontGL::HAlign alignment = LLFontGL::HCENTER,
						 const LLColor4& color = LLColor4::white);
	void setImageOverlay(LLUIImagePtr image,
						 LLFontGL::HAlign alignment = LLFontGL::HCENTER,
						 const LLColor4& color = LLColor4::white);
	LL_INLINE LLUIImagePtr getImageOverlay()						{ return mImageOverlay; }


	LL_INLINE void setValue(const LLSD& value) override				{ mToggleState = value.asBoolean(); }
	LL_INLINE LLSD getValue() const override						{ return LLSD(mToggleState); }

	LL_INLINE bool setLabelArg(const std::string& key,
							   const std::string& text) override
	{
		mUnselectedLabel.setArg(key, text);
		mSelectedLabel.setArg(key, text);
		return true;
	}

	LL_INLINE void setLabelUnselected(const std::string& label)		{ mUnselectedLabel = label; }
	LL_INLINE void setLabelSelected(const std::string& label)		{ mSelectedLabel = label; }
	LL_INLINE void setLabel(const std::string& label)				{ mUnselectedLabel = mSelectedLabel = label; }


	LL_INLINE void setDisabledLabel(const std::string& label)		{ mDisabledLabel = label; }

	LL_INLINE void setDisabledSelectedLabel(const std::string& label)
	{
		mDisabledSelectedLabel = label;
	}

	LL_INLINE void setDisabledLabelColor(const LLColor4& c)			{ mDisabledLabelColor = c; }

	LL_INLINE void setFont(const LLFontGL* font)					{ mGLFont = font ? font : LLFontGL::getFontSansSerif(); }

	LL_INLINE void setScaleImage(bool scale)						{ mScaleImage = scale; }
	LL_INLINE bool getScaleImage() const							{ return mScaleImage; }

	LL_INLINE void setDropShadowedText(bool b)						{ mDropShadowedText = b; }

	LL_INLINE void setBorderEnabled(bool b)							{ mBorderEnabled = b; }

	// to be called by gIdleCallbacks
	static void onHeldDown(void* userdata);

	LL_INLINE void setHoverGlowStrength(F32 strength)				{ mHoverGlowStrength = strength; }

	void setImageUnselected(const std::string& image_name);
	LL_INLINE const std::string& getImageUnselectedName() const		{ return mImageUnselectedName; }
	void setImageSelected(const std::string& image_name);
	LL_INLINE const std::string& getImageSelectedName() const		{ return mImageSelectedName; }
	void setImageHoverSelected(const std::string& image_name);
	void setImageHoverUnselected(const std::string& image_name);
	void setImageDisabled(const std::string& image_name);
	void setImageDisabledSelected(const std::string& image_name);

	void setImageUnselected(LLUIImagePtr image);
	void setImageSelected(LLUIImagePtr image);
	void setImageHoverSelected(LLUIImagePtr image);
	void setImageHoverUnselected(LLUIImagePtr image);
	void setImageDisabled(LLUIImagePtr image);
	void setImageDisabledSelected(LLUIImagePtr image);

	LL_INLINE void setCommitOnReturn(bool commit)					{ mCommitOnReturn = commit; }
	LL_INLINE bool getCommitOnReturn() const						{ return mCommitOnReturn; }

	void setHelpURLCallback(const std::string& help_url);
	LL_INLINE const std::string& getHelpURL() const					{ return mHelpURL; }

protected:
	virtual void drawBorder(const LLColor4& color, S32 size);

	void setImageUnselectedID(const LLUUID& image_id);
	LL_INLINE const LLUUID& getImageUnselectedID() const			{ return mImageUnselectedID; }
	void setImageSelectedID(const LLUUID& image_id);
	LL_INLINE const LLUUID& getImageSelectedID() const				{ return mImageSelectedID; }
	void setImageHoverSelectedID(const LLUUID& image_id);
	void setImageHoverUnselectedID(const LLUUID& image_id);
	void setImageDisabledID(const LLUUID& image_id);
	void setImageDisabledSelectedID(const LLUUID& image_id);
	LL_INLINE const LLUIImagePtr& getImageUnselected() const		{ return mImageUnselected; }
	LL_INLINE const LLUIImagePtr& getImageSelected() const			{ return mImageSelected; }

protected:
	LLFrameTimer		mMouseDownTimer;

private:
	void				(*mClickedCallback)(void*);
	void				(*mMouseHoverCallback)(void*);
	void				(*mMouseDownCallback)(void*);
	void				(*mMouseUpCallback)(void*);
	void				(*mHeldDownCallback)(void*);

	const LLFontGL*		mGLFont;

	S32					mMouseDownFrame;
	// seconds, after which held-down callbacks get called:
	F32					mHeldDownDelay;
	// frames, after which held-down callbacks get called:
	S32					mHeldDownFrameDelay;

	LLUIImagePtr		mImageOverlay;
	LLFontGL::HAlign	mImageOverlayAlignment;
	LLColor4			mImageOverlayColor;

	LLUIImagePtr		mImageUnselected;
	LLUIString			mUnselectedLabel;
	LLColor4			mUnselectedLabelColor;

	LLUIImagePtr		mImageSelected;
	LLUIString			mSelectedLabel;
	LLColor4			mSelectedLabelColor;

	LLUIImagePtr		mImageHoverSelected;

	LLUIImagePtr		mImageHoverUnselected;

	LLUIImagePtr		mImageDisabled;
	LLUIString			mDisabledLabel;
	LLColor4			mDisabledLabelColor;

	LLUIImagePtr		mImageDisabledSelected;
	LLUIString			mDisabledSelectedLabel;
	LLColor4			mDisabledSelectedLabelColor;

	LLUUID				mImageUnselectedID;
	LLUUID				mImageSelectedID;
	LLUUID				mImageHoverSelectedID;
	LLUUID				mImageHoverUnselectedID;
	LLUUID				mImageDisabledID;
	LLUUID				mImageDisabledSelectedID;

	LLColor4			mFlashBgColor;

	LLColor4			mImageColor;
	LLColor4			mDisabledImageColor;

	bool				mIsToggle;
	bool				mToggleState;
	bool				mScaleImage;

	bool				mDropShadowedText;

	bool				mBorderEnabled;

	bool				mNeedsHighlight;
	bool				mCommitOnReturn;

	bool				mFlashing;
	LLFrameTimer		mFlashingTimer;

	LLFontGL::HAlign	mHAlign;
	S32					mLeftHPad;
	S32					mRightHPad;

	F32					mHoverGlowStrength;
	F32					mCurGlowStrength;

	std::string			mImageUnselectedName;
	std::string			mImageSelectedName;
	std::string			mImageHoverSelectedName;
	std::string			mImageHoverUnselectedName;
	std::string			mImageDisabledName;
	std::string			mImageDisabledSelectedName;

	std::string			mHelpURL;

	LLUIImagePtr		mImagep;
};

#endif  // LL_LLBUTTON_H
