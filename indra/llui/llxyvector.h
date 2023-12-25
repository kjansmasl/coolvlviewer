/**
 * @file llxyvector.h
 * @brief LLXYVector class declaration
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

#ifndef LL_LLXYVECTOR_H
#define LL_LLXYVECTOR_H

#include "llcolor4.h"
#include "lluictrl.h"

class LLLineEditor;
class LLPanel;
class LLTextBox;
class LLUICtrlFactory;
class LLViewBorder;

class LLXYVector : public LLUICtrl
{
protected:
	LOG_CLASS(LLXYVector);

public:
	LLXYVector(const std::string& name, const LLRect& rect,
			   void (*commit_cb)(LLUICtrl*, void*), void* userdata);

	// New method
	bool postBuild();

	void draw() override;

	void setValue(F32 x, F32 y);
	void setValue(const LLSD& value) override;
	LLSD getValue() const override;

	bool handleMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleMouseUp(S32 x, S32 y, MASK mask) override;
	bool handleHover(S32 x, S32 y, MASK mask) override;
	
	LLXMLNodePtr getXML(bool save_children = true) const override;
	static LLView* fromXML(LLXMLNodePtr node, LLView* parent,
						   LLUICtrlFactory* factory);

	LL_INLINE void setArrowColor(const LLColor4& c)	{ mArrowColor = c; }
	LL_INLINE void setAreaColor(const LLColor4& c)	{ mAreaColor = c; }
	LL_INLINE void setGridColor(const LLColor4& c)	{ mGridColor = c; }
	LL_INLINE void setGhostColor(const LLColor4& c)	{ mGhostColor = c; }

private:
	void update();
	void setValueAndCommit(F32 x, F32 y);
	static void onEditChange(LLUICtrl*, void* userdata);

private:
	LLPanel*		mTouchArea;
	LLViewBorder*	mBorder;
	LLTextBox*		mXLabel;
	LLTextBox*		mYLabel;
	LLLineEditor*	mXEntry;
	LLLineEditor*	mYEntry;

	U32				mGhostX;
	U32				mGhostY;

	F32				mValueX;
	F32				mValueY;
	F32				mMinValueX;
	F32				mMaxValueX;
	F32				mLogScaleX;
	F32				mIncrementX;
	F32				mMinValueY;
	F32				mMaxValueY;
	F32				mIncrementY;
	F32				mLogScaleY;

	LLColor4		mArrowColor;
	LLColor4		mAreaColor;
	LLColor4		mGridColor;
	LLColor4		mGhostColor;

	bool			mLogarithmic;
};

#endif  // LL_LLXYVECTOR_H
