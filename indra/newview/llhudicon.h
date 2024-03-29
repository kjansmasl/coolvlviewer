/**
 * @file llhudicon.h
 * @brief LLHUDIcon class definition
 *
 * $LicenseInfo:firstyear=2006&license=viewergpl$
 *
 * Copyright (c) 2006-2009, Linden Research, Inc.
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

// Renders a 2D icon billboard floating at the location specified.

#ifndef LL_LLHUDICON_H
#define LL_LLHUDICON_H

#include <vector>

#include "llframetimer.h"
#include "lluuid.h"

#include "llhudobject.h"

class LLViewerTexture;

class LLHUDIcon final : public LLHUDObject
{
friend class LLHUDObject;

public:
	void render() override;
	void markDead() override;
	F32 getDistance() const override					{ return mDistance; }

	void setImage(LLViewerTexture* imagep);
	LL_INLINE void setScale(F32 fraction_of_fov)		{ mScale = fraction_of_fov; }

	void restartLifeTimer()								{ mLifeTimer.reset(); }

	static LLHUDIcon* lineSegmentIntersectAll(const LLVector4a& start,
											  const LLVector4a& end,
											  LLVector4a* intersection);

	static void cleanupDeadIcons();
	LL_INLINE static void updateAll()					{ cleanupDeadIcons(); }


	LL_INLINE static S32 getNumInstances()				{ return sIconInstances.size(); }

	bool getHidden() const								{ return mHidden; }
	void setHidden(bool hide)							{ mHidden = hide; }

	bool lineSegmentIntersect(const LLVector4a& start, const LLVector4a& end,
							  LLVector4a* intersection);

	void setClickedCallback(void (*cb)(const LLUUID&))	{ mClickedCallback = cb; }
	void fireClickedCallback(const LLUUID& id);

public:
	static F32 MAX_VISIBLE_TIME;

protected:
	LLHUDIcon(U8 type);
	~LLHUDIcon() override;

private:
	LLPointer<LLViewerTexture> mImagep;
	LLFrameTimer	mAnimTimer;
	LLFrameTimer	mLifeTimer;
	F32				mDistance;
	F32				mScale;
	bool			mHidden;
	bool			mIsScriptBugIcon;

	void			(*mClickedCallback)(const LLUUID& id);

	typedef std::vector<LLPointer<LLHUDIcon> > icon_instance_t;
	static icon_instance_t sIconInstances;
};

#endif // LL_LLHUDICON_H
