/**
 * @file llvirtualtrackball.h
 * @brief LLVirtualTrackball class declaration
 *
 * $LicenseInfo:firstyear=2018&license=viewergpl$
 *
 * Copyright (c) 2018, Linden Research, Inc.
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

#ifndef LL_LLVIRTUALTRACKBALL_H
#define LL_LLVIRTUALTRACKBALL_H

#include "lluictrl.h"

class LLButton;
class LLPanel;
class LLTextBox;
class LLUICtrlFactory;
class LLUIImage;
class LLViewBorder;

class LLVirtualTrackball : public LLUICtrl
{
protected:
	LOG_CLASS(LLVirtualTrackball);

public:
	LLVirtualTrackball(const std::string& name, const LLRect& rect,
					   void (*commit_cb)(LLUICtrl*, void*), void* userdata);

	void draw() override;

	void setValue(F32 x, F32 y, F32 z, F32 w);
	void setValue(const LLSD& value) override;
	LLSD getValue() const override;

	bool handleKeyHere(KEY key, MASK mask) override;
	bool handleHover(S32 x, S32 y, MASK mask) override;
	bool handleMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleMouseUp(S32 x, S32 y, MASK mask) override;
	
	LLXMLNodePtr getXML(bool save_children = true) const override;
	static LLView* fromXML(LLXMLNodePtr node, LLView* parent,
						   LLUICtrlFactory* factory);

	LL_INLINE void setRotation(const LLQuaternion& val)	{ mValue = val; }
	LL_INLINE LLQuaternion getRotation() const			{ return mValue; }

	void getAzimuthAndElevationDeg(F32& azim, F32& elev);

private:
	enum EThumbMode
	{
		SUN,
		MOON
	};

	enum EDragMode
	{
		DRAG_SET,
		DRAG_SCROLL
	};

	void setValueAndCommit(const LLQuaternion& value);
	void drawThumb(S32 x, S32 y, EThumbMode mode, bool upper_hemi = true);
	bool pointInTouchCircle(S32 x, S32 y) const;

	static void onRotateTopClick(void* userdata);
	static void onRotateBottomClick(void* userdata);
	static void onRotateLeftClick(void* userdata);
	static void onRotateRightClick(void* userdata);

	static void onRotateTopClickNoSound(void* userdata);
	static void onRotateBottomClickNoSound(void* userdata);
	static void onRotateLeftClickNoSound(void* userdata);
	static void onRotateRightClickNoSound(void* userdata);

	static void onEditChange(LLUICtrl*, void* userdata);

private:
	LLPanel*		mTouchArea;

	LLViewBorder*	mBorder;

	LLButton*		mBtnRotateTop;
	LLButton*		mBtnRotateBottom;
	LLButton*		mBtnRotateLeft;
	LLButton*		mBtnRotateRight;

	LLTextBox*		mLabelN;
	LLTextBox*		mLabelS;
	LLTextBox*		mLabelW;
	LLTextBox*		mLabelE;

	LLUIImage*		mImgMoonBack;
	LLUIImage*		mImgMoonFront;
	LLUIImage*		mImgSunBack;
	LLUIImage*		mImgSunFront;
	LLUIImage*		mImgBtnRotTop;
	LLUIImage*		mImgBtnRotLeft;
	LLUIImage*		mImgBtnRotRight;
	LLUIImage*		mImgBtnRotBottom;
	LLUIImage*		mImgSphere;

	LLQuaternion	mValue;

	S32				mPrevX;
	S32				mPrevY;

	F32				mIncrementMouse;
	F32				mIncrementBtn;

	EThumbMode		mThumbMode;
	EDragMode		mDragMode;
};

#endif  // LL_LLVIRTUALTRACKBALL_H
