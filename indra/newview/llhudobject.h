/**
 * @file llhudobject.h
 * @brief LLHUDObject class definition
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

#ifndef LL_LLHUDOBJECT_H
#define LL_LLHUDOBJECT_H

/**
 * Base class and manager for in-world 2.5D non-interactive objects
 */

#include <list>

#include "llpointer.h"
#include "llrefcount.h"
#include "llvector3.h"
#include "llvector3d.h"
#include "llcolor4.h"

class LLHUDEffect;
class LLViewerObject;

class LLHUDObject : public LLRefCount
{
protected:
	LOG_CLASS(LLHUDObject);

public:
	virtual void markDead();
	LL_INLINE virtual bool isDead() const				{ return mDead; }
	LL_INLINE virtual F32 getDistance() const			{ return 0.f; }
	virtual void setSourceObject(LLViewerObject* objectp);
	virtual void setTargetObject(LLViewerObject* objectp);

	LL_INLINE virtual LLViewerObject* getSourceObject()
	{
		return mSourceObject;
	}

	LL_INLINE virtual LLViewerObject* getTargetObject()	{ return mTargetObject; }

	void setPositionGlobal(const LLVector3d& position_global);
	void setPositionAgent(const LLVector3& position_agent);

	LL_INLINE bool isVisible() const					{ return mVisible; }

	LL_INLINE U8 getType() const						{ return mType; }

	LL_INLINE LLVector3d getPositionGlobal() const		{ return mPositionGlobal; }

	static LLHUDObject* addHUDObject(U8 type);
	static LLHUDEffect* addHUDEffect(U8 type);
	static void updateAll();
	static void renderAll();
	static void removeExpired();

	static void cleanupHUDObjects();

	enum
	{
		LL_HUD_TEXT,
		LL_HUD_ICON,
		LL_HUD_CONNECTOR,
		LL_HUD_FLEXIBLE_OBJECT,
		LL_HUD_ANIMAL_CONTROLS,
		LL_HUD_LOCAL_ANIMATION_OBJECT,
		LL_HUD_CLOTH,
		LL_HUD_EFFECT_BEAM,
		LL_HUD_EFFECT_GLOW,
		LL_HUD_EFFECT_POINT,
		LL_HUD_EFFECT_TRAIL,
		LL_HUD_EFFECT_SPHERE,
		LL_HUD_EFFECT_SPIRAL,
		LL_HUD_EFFECT_EDIT,
		LL_HUD_EFFECT_LOOKAT,
		LL_HUD_EFFECT_POINTAT,
		LL_HUD_EFFECT_VOICE_VISUALIZER
	};

protected:
	static void sortObjects();

	LLHUDObject(U8 type);
	// Do not declare = default here: because of includes circular dependencies
	// this would cause compilation failures. HB
	// *TODO: solve the circular dependency issue.
	~LLHUDObject() override;

	virtual void render() = 0;

	static void renderObjects();

protected:
	LLVector3d					mPositionGlobal;
	LLPointer<LLViewerObject>	mSourceObject;
	LLPointer<LLViewerObject>	mTargetObject;
	U8							mType;
	bool						mDead;
	bool						mVisible;
	bool						mOnHUDAttachment;

private:
	typedef std::list<LLPointer<LLHUDObject> > hud_object_list_t;
	static hud_object_list_t	sHUDObjects;
};

#endif // LL_LLHUDOBJECT_H
