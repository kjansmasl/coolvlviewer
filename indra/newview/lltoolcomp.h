/**
 * @file lltoolcomp.h
 * @brief Composite tools
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

#ifndef LL_TOOLCOMP_H
#define LL_TOOLCOMP_H

#include "lltool.h"

class LLManip;
class LLPickInfo;
class LLTextBox;
class LLToolGun;
class LLToolGrabBase;
class LLToolPlacer;
class LLToolSelect;
class LLToolSelectRect;
class LLView;

class LLToolComposite : public LLTool
{
protected:
	LOG_CLASS(LLToolComposite);

public:
	LLToolComposite(const std::string& name);
	~LLToolComposite() override = default;

	// Sets the current tool:
	bool handleMouseDown(S32 x, S32 y, MASK mask) override = 0;
	// Returns to the default tool:
	bool handleMouseUp(S32 x, S32 y, MASK mask) override;
	bool handleDoubleClick(S32 x, S32 y, MASK mask) override = 0;

	// Map virtual functions to the currently active internal tool:
	LL_INLINE bool handleHover(S32 x, S32 y, MASK mask) override
	{
		return mCur->handleHover(x, y, mask);
	}

	LL_INLINE bool handleScrollWheel(S32 x, S32 y, S32 clicks) override
	{
		return mCur->handleScrollWheel(x, y, clicks);
	}

	LL_INLINE bool handleRightMouseDown(S32 x, S32 y, MASK mask) override
	{
		return mCur->handleRightMouseDown(x, y, mask);
	}

	LL_INLINE LLViewerObject* getEditingObject() override	{ return mCur->getEditingObject(); }
	LL_INLINE LLVector3d getEditingPointGlobal() override	{ return mCur->getEditingPointGlobal(); }
	LL_INLINE bool isEditing() override						{ return mCur->isEditing(); }
	LL_INLINE void stopEditing() override					{ mCur->stopEditing(); mCur = mDefault; }

	LL_INLINE bool clipMouseWhenDown() override				{ return mCur->clipMouseWhenDown(); }

	void handleSelect() override;
	LL_INLINE void handleDeselect() override				{ mCur->handleDeselect(); mCur = mDefault; mSelected = false; }

	LL_INLINE void render() override						{ mCur->render(); }
	LL_INLINE void draw() override							{ mCur->draw(); }

	LL_INLINE bool handleKey(KEY key, MASK mask) override	{ return mCur->handleKey(key, mask); }

	void onMouseCaptureLost() override;

	LL_INLINE void screenPointToLocal(S32 scr_x, S32 scr_y, S32* loc_x,
									  S32* loc_y) const override
	{
		mCur->screenPointToLocal(scr_x, scr_y, loc_x, loc_y);
	}

	LL_INLINE void localPointToScreen(S32 loc_x, S32 loc_y, S32* scr_x,
									  S32* scr_y) const override
	{
		mCur->localPointToScreen(loc_x, loc_y, scr_x, scr_y);
	}

	bool isSelecting();

protected:
	void setCurrentTool(LLTool* new_tool);
	LL_INLINE LLTool* getCurrentTool()						{ return mCur; }

	// In hover handler, call this to auto-switch tools:
	void setToolFromMask(MASK mask, LLTool* normal);

protected:
	// The tool to which we are delegating:
	LLTool*						mCur;
	LLTool*						mDefault;
	LLManip*					mManip;
	LLToolSelectRect*			mSelectRect;
	bool						mSelected;
	bool						mMouseDown;

public:
	static const std::string	sNameComp;
};

class LLToolCompInspect final : public LLToolComposite
{
protected:
	LOG_CLASS(LLToolCompInspect);

public:
	LLToolCompInspect();
	~LLToolCompInspect() override;

	// Overridden from LLToolComposite
	bool handleMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleDoubleClick(S32 x, S32 y, MASK mask) override;

	static void pickCallback(const LLPickInfo& pick_info);
};

class LLToolCompTranslate final : public LLToolComposite
{
protected:
	LOG_CLASS(LLToolCompTranslate);

public:
	LLToolCompTranslate();
	~LLToolCompTranslate() override;

	// This is an object edit tool
	LL_INLINE bool isObjectEditTool() const override		{ return true; }

	// Overridden from LLToolComposite
	bool handleMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleDoubleClick(S32 x, S32 y, MASK mask) override;
	bool handleHover(S32 x, S32 y, MASK mask) override;
	// Returns to the default tool:
	bool handleMouseUp(S32 x, S32 y, MASK mask) override;

	void render() override;

	LLTool* getOverrideTool(MASK mask) override;

	static void pickCallback(const LLPickInfo& pick_info);
};

class LLToolCompScale final : public LLToolComposite
{
protected:
	LOG_CLASS(LLToolCompScale);

public:
	LLToolCompScale();
	~LLToolCompScale() override;

	// This is an object edit tool
	LL_INLINE bool isObjectEditTool() const override		{ return true; }

	// Overridden from LLToolComposite
	bool handleMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleDoubleClick(S32 x, S32 y, MASK mask) override;
	bool handleHover(S32 x, S32 y, MASK mask) override;
	// Returns to the default tool:
	bool handleMouseUp(S32 x, S32 y, MASK mask) override;

	void render() override;

	LLTool* getOverrideTool(MASK mask) override;

	static void pickCallback(const LLPickInfo& pick_info);
};

class LLToolCompRotate final : public LLToolComposite
{
protected:
	LOG_CLASS(LLToolCompRotate);

public:
	LLToolCompRotate();
	~LLToolCompRotate() override;

	// This is an object edit tool
	LL_INLINE bool isObjectEditTool() const override		{ return true; }

	// Overridden from LLToolComposite
	bool handleMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleDoubleClick(S32 x, S32 y, MASK mask) override;
	bool handleHover(S32 x, S32 y, MASK mask) override;
	bool handleMouseUp(S32 x, S32 y, MASK mask) override;
	void render() override;

	LLTool* getOverrideTool(MASK mask) override;

	static void pickCallback(const LLPickInfo& pick_info);
};

class LLToolCompCreate final : public LLToolComposite
{
protected:
	LOG_CLASS(LLToolCompCreate);

public:
	LLToolCompCreate();
	~LLToolCompCreate() override;

	// Overridden from LLToolComposite
	bool handleMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleDoubleClick(S32 x, S32 y, MASK mask) override;
	bool handleMouseUp(S32 x, S32 y, MASK mask) override;

	static void pickCallback(const LLPickInfo& pick_info);

protected:
	LLToolPlacer*	mPlacer;
	bool			mObjectPlacedOnMouseDown;
};

class LLToolCompGun final : public LLToolComposite
{
protected:
	LOG_CLASS(LLToolCompGun);

public:
	LLToolCompGun();
	~LLToolCompGun() override;

	// Overridden from LLToolComposite
	bool handleHover(S32 x, S32 y, MASK mask) override;
	bool handleMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleDoubleClick(S32 x, S32 y, MASK mask) override;
	bool handleRightMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleMouseUp(S32 x, S32 y, MASK mask) override;
	bool handleScrollWheel(S32 x, S32 y, S32 clicks) override;
	void onMouseCaptureLost() override;
	void handleSelect() override;
	void handleDeselect() override;

	LL_INLINE LLTool* getOverrideTool(MASK mask) override	{ return NULL; }

protected:
	LLToolGun*		mGun;
	LLToolGrabBase*	mGrab;
};

extern LLToolCompInspect gToolCompInspect;
extern LLToolCompTranslate gToolCompTranslate;
extern LLToolCompScale gToolCompScale;
extern LLToolCompRotate gToolCompRotate;
extern LLToolCompCreate gToolCompCreate;
extern LLToolCompGun gToolCompGun;

#endif  // LL_TOOLCOMP_H
