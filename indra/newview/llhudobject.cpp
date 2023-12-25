/**
 * @file llhudobject.cpp
 * @brief LLHUDObject class implementation
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

#include "llhudobject.h"

#include "llfasttimer.h"
#include "llvertexbuffer.h"

#include "llagent.h"
#include "llhudeffectlookat.h"
#include "llhudeffectspiral.h"
#include "llhudicon.h"
#include "llhudtext.h"
#include "llviewerobject.h"
#include "llviewershadermgr.h"
#include "llvoicevisualizer.h"

// statics
std::list<LLPointer<LLHUDObject> > LLHUDObject::sHUDObjects;

struct hud_object_further_away
{
	bool operator()(const LLPointer<LLHUDObject>& lhs,
					const LLPointer<LLHUDObject>& rhs) const;
};

LL_INLINE bool hud_object_further_away::operator()(const LLPointer<LLHUDObject>& lhs,
												   const LLPointer<LLHUDObject>& rhs) const
{
	return lhs->getDistance() > rhs->getDistance();
}

LLHUDObject::LLHUDObject(U8 type)
:	mPositionGlobal(),
	mSourceObject(NULL),
	mTargetObject(NULL),
	mVisible(true),
	mType(type),
	mDead(false),
	mOnHUDAttachment(false)
{
}

//virtual
LLHUDObject::~LLHUDObject()
{
	// NOTE: it is important that this empty destructor is declared here
	// instead of = default in the header file; because of circular
	// dependencies in includes between llviewerobject.h and llhud*.h, the
	// compiler would fail when attempting to complete this class too early. HB
	// *TODO: solve the circular dependency issue.
}

void LLHUDObject::markDead()
{
	mVisible = false;
	mDead = true;
	mSourceObject = NULL;
	mTargetObject = NULL;
}

void LLHUDObject::setSourceObject(LLViewerObject* objectp)
{
	if (objectp != mSourceObject)
	{
		mSourceObject = objectp;
	}	
}

void LLHUDObject::setTargetObject(LLViewerObject* objectp)
{
	if (objectp != mTargetObject)
	{
		mTargetObject = objectp;
	}
}

void LLHUDObject::setPositionGlobal(const LLVector3d& position_global)
{
	mPositionGlobal = position_global;
}

void LLHUDObject::setPositionAgent(const LLVector3& position_agent)
{
	mPositionGlobal = gAgent.getPosGlobalFromAgent(position_agent);
}

//static
void LLHUDObject::cleanupHUDObjects()
{
	LLHUDIcon::cleanupDeadIcons();

	hud_object_list_t::iterator object_it;
	for (hud_object_list_t::iterator object_it = sHUDObjects.begin(),
									 end = sHUDObjects.end();
		 object_it != end; ++object_it)
	{
		LLHUDObject* hud_objp = *object_it;
		S32 numrefs = hud_objp->getNumRefs();
		S32 type = (S32)hud_objp->getType();
		// *HACK: voice visualizers may have 2 references on logout,
		// depending whether the corresponding hud effect gets destroyed before
		// or after the hud objects are cleaned up... which in turns depends
		// on the compiler being used. *TODO: explicitely destroy the HUD
		// effects before calling LLHUDObject::cleanupHUDObjects().
		if (numrefs > (type != LL_HUD_EFFECT_VOICE_VISUALIZER ? 1 : 2))
		{
			llinfos << "HUD Object " << std::hex << hud_objp << std::dec
					<< " type " << type << " still had " << numrefs
					<< " active references" << llendl;
		}
		hud_objp->markDead();
	}

	sHUDObjects.clear();
}

//static
LLHUDObject* LLHUDObject::addHUDObject(U8 type)
{
	LLHUDObject* hud_objectp = NULL;

	switch (type)
	{
		case LL_HUD_TEXT:
			hud_objectp = new LLHUDText(type);
			break;

		case LL_HUD_ICON:
			hud_objectp = new LLHUDIcon(type);
			break;

		default:
			llwarns << "Unknown type of hud object:" << (U32)type << llendl;
	}
	if (hud_objectp)
	{
		sHUDObjects.push_back(hud_objectp);
	}
	return hud_objectp;
}

LLHUDEffect* LLHUDObject::addHUDEffect(U8 type)
{
	LLHUDEffect* hud_objectp = NULL;

	switch (type)
	{
		case LL_HUD_EFFECT_BEAM:
		{
			LLHUDEffectSpiral* spiralp = new LLHUDEffectSpiral(type);
			hud_objectp = spiralp;
			spiralp->setDuration(0.7f);
			spiralp->setVMag(0.f);
			spiralp->setVOffset(0.f);
			spiralp->setInitialRadius(0.1f);
			spiralp->setFinalRadius(0.2f);
			spiralp->setSpinRate(10.f);
			spiralp->setFlickerRate(0.f);
			spiralp->setScaleBase(0.05f);
			spiralp->setScaleVar(0.02f);
			spiralp->setColor(LLColor4U(255, 255, 255, 255));
			break;
		}

		case LL_HUD_EFFECT_POINT:
		{
			LLHUDEffectSpiral* spiralp = new LLHUDEffectSpiral(type);
			hud_objectp = spiralp;
			spiralp->setDuration(0.5f);
			spiralp->setVMag(1.f);
			spiralp->setVOffset(0.f);
			spiralp->setInitialRadius(0.5f);
			spiralp->setFinalRadius(1.f);
			spiralp->setSpinRate(10.f);
			spiralp->setFlickerRate(0.f);
			spiralp->setScaleBase(0.1f);
			spiralp->setScaleVar(0.1f);
			spiralp->setColor(LLColor4U(255, 255, 255, 255));
			break;
		}

		case LL_HUD_EFFECT_SPHERE:
		{
			LLHUDEffectSpiral* spiralp = new LLHUDEffectSpiral(type);
			hud_objectp = spiralp;
			spiralp->setDuration(0.5f);
			spiralp->setVMag(1.f);
			spiralp->setVOffset(0.f);
			spiralp->setInitialRadius(0.5f);
			spiralp->setFinalRadius(0.5f);
			spiralp->setSpinRate(20.f);
			spiralp->setFlickerRate(0.f);
			spiralp->setScaleBase(0.1f);
			spiralp->setScaleVar(0.1f);
			spiralp->setColor(LLColor4U(255, 255, 255, 255));
			break;
		}

		case LL_HUD_EFFECT_SPIRAL:
		{
			LLHUDEffectSpiral* spiralp = new LLHUDEffectSpiral(type);
			hud_objectp = spiralp;
			spiralp->setDuration(2.f);
			spiralp->setVMag(-2.f);
			spiralp->setVOffset(0.5f);
			spiralp->setInitialRadius(1.f);
			spiralp->setFinalRadius(0.5f);
			spiralp->setSpinRate(10.f);
			spiralp->setFlickerRate(20.f);
			spiralp->setScaleBase(0.02f);
			spiralp->setScaleVar(0.02f);
			spiralp->setColor(LLColor4U(255, 255, 255, 255));
			break;
		}

		case LL_HUD_EFFECT_EDIT:
		{
			LLHUDEffectSpiral* spiralp = new LLHUDEffectSpiral(type);
			hud_objectp = spiralp;
			spiralp->setDuration(2.f);
			spiralp->setVMag(2.f);
			spiralp->setVOffset(-1.f);
			spiralp->setInitialRadius(1.5f);
			spiralp->setFinalRadius(1.f);
			spiralp->setSpinRate(4.f);
			spiralp->setFlickerRate(200.f);
			spiralp->setScaleBase(0.1f);
			spiralp->setScaleVar(0.1f);
			spiralp->setColor(LLColor4U(255, 255, 255, 255));
			break;
		}

		case LL_HUD_EFFECT_LOOKAT:
			hud_objectp = new LLHUDEffectLookAt(type);
			break;

		case LL_HUD_EFFECT_VOICE_VISUALIZER:
			hud_objectp = new LLVoiceVisualizer(type);
			break;

		case LL_HUD_EFFECT_POINTAT:
			hud_objectp = new LLHUDEffectPointAt(type);
			break;

		default:
			llwarns << "Unknown type of hud effect:" << (U32)type << llendl;
	}

	if (hud_objectp)
	{
		sHUDObjects.push_back(hud_objectp);
	}

	return hud_objectp;
}

//static
void LLHUDObject::updateAll()
{
	LL_FAST_TIMER(FTM_HUD_OBJECTS);
	LLHUDText::updateAll();
	LLHUDIcon::updateAll();
	sortObjects();
}

//static
void LLHUDObject::renderObjects()
{
	for (hud_object_list_t::iterator object_it = sHUDObjects.begin(),
									 end = sHUDObjects.end();
		 object_it != end; )
	{
		hud_object_list_t::iterator cur_it = object_it++;
		LLHUDObject* hud_objp = *cur_it;
		if (hud_objp->getNumRefs() == 1)
		{
			sHUDObjects.erase(cur_it);
		}
		else if (hud_objp->isVisible())
		{
			hud_objp->render();
		}
	}
}

//static
void LLHUDObject::renderAll()
{
	if (gUsePBRShaders)
	{
		LLGLSUIDefault gls_ui;
		gUIProgram.bind();
		gGL.color4f(1.f, 1.f, 1.f, 1.f);
		LLGLDepthTest depth(GL_FALSE, GL_FALSE);
		renderObjects();
		LLVertexBuffer::unbind();
		gUIProgram.unbind();
	}
	else
	{
		renderObjects();
		LLVertexBuffer::unbind();
	}
}

//static
void LLHUDObject::removeExpired()
{
	for (hud_object_list_t::iterator object_it = sHUDObjects.begin(),
									 end = sHUDObjects.end();
		 object_it != end; )
	{
		hud_object_list_t::iterator cur_it = object_it++;
		LLHUDObject* hud_objp = *cur_it;
		if (hud_objp->getNumRefs() == 1)
		{
			sHUDObjects.erase(cur_it);
		}
	}
}

//static
void LLHUDObject::sortObjects()
{
	sHUDObjects.sort(hud_object_further_away());
}
