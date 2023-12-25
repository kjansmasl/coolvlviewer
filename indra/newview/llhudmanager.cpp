/**
 * @file llhudmanager.cpp
 * @brief LLHUDManager class implementation
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

#include "llhudmanager.h"

#include "llfasttimer.h"
#include "llmessage.h"
#include "object_flags.h"

#include "llagent.h"
#include "llhudeffect.h"
#include "llpipeline.h"
#include "llviewercontrol.h"
#include "llviewerobjectlist.h"

//static
LLHUDManager::effects_list_t LLHUDManager::sHUDEffects;

//static
void LLHUDManager::updateEffects()
{
	LL_FAST_TIMER(FTM_HUD_EFFECTS);
	for (S32 i = 0, count = sHUDEffects.size(); i < count; ++i)
	{
		LLHUDEffect* hep = sHUDEffects[i];
		if (!hep->isDead())
		{
			hep->update();
		}
	}
}

//static
void LLHUDManager::sendEffects()
{
	for (S32 i = 0, count = sHUDEffects.size(); i < count; ++i)
	{
		LLHUDEffect* hep = sHUDEffects[i];
		if (hep->isDead())
		{
			// It does happen (e.g. on TP or logout). Harmless: just ignore. HB
			continue;
		}
		if (hep->mType < LLHUDObject::LL_HUD_EFFECT_BEAM)
		{
			llwarns << "Trying to send effect of unknown type: " << hep->mType
					<< llendl;
			llassert(false);
			continue;
		}
		if (hep->getNeedsSendToSim() && hep->getOriginatedHere())
		{
			LLMessageSystem* msg = gMessageSystemp;
			msg->newMessageFast(_PREHASH_ViewerEffect);
			msg->nextBlockFast(_PREHASH_AgentData);
			msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
			msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
			msg->nextBlockFast(_PREHASH_Effect);
			hep->packData(msg);
			hep->setNeedsSendToSim(false);
			gAgent.sendMessage();
		}
	}
}

//static
void LLHUDManager::cleanupClass()
{
	sHUDEffects.clear();
}

//static
void LLHUDManager::cleanupEffects()
{
	effects_list_t::iterator iter = sHUDEffects.begin();
	while (iter != sHUDEffects.end())
	{
		LLHUDEffect* effect = (*iter).get();
		if (!effect || effect->isDead())
		{
			if (iter + 1 != sHUDEffects.end())
			{
				*iter = sHUDEffects.back();
			}
			sHUDEffects.pop_back();
		}
		else
		{
			++iter;
		}
	}
}

//static
LLHUDEffect* LLHUDManager::createEffect(U8 type, bool send_to_sim,
										bool originated_here)
{
	// SJB: DO NOT USE addHUDObject !  Not all LLHUDObjects are LLHUDEffects !
	LLHUDEffect* effectp = LLHUDObject::addHUDEffect(type);
	if (effectp)
	{
		LLUUID tmp;
		tmp.generate();
		effectp->setID(tmp);
		effectp->setNeedsSendToSim(send_to_sim);
		effectp->setOriginatedHere(originated_here);

		sHUDEffects.push_back(effectp);
	}
	return effectp;
}

//static
void LLHUDManager::processViewerEffect(LLMessageSystem* mesgsys, void**)
{
	LLUUID effect_id;
	U8 effect_type = 0;
	S32 number_blocks = mesgsys->getNumberOfBlocksFast(_PREHASH_Effect);
	for (S32 k = 0; k < number_blocks; ++k)
	{
		LLHUDEffect* effectp = NULL;
		LLHUDEffect::getIDType(mesgsys, k, effect_id, effect_type);
		effects_list_t::iterator iter = sHUDEffects.begin();
		while (iter != sHUDEffects.end())
		{
			LLHUDEffect* cur_effectp = (*iter).get();
			if (!cur_effectp || cur_effectp->isDead())
			{
				LL_DEBUGS("HudManager") << (cur_effectp ? "Dead" : "NULL")
										<< " effect in manager list; removed."
										<< LL_ENDL;
				if (iter + 1 != sHUDEffects.end())
				{
					*iter = sHUDEffects.back();
				}
				sHUDEffects.pop_back();
				continue;
			}
			if (cur_effectp->getID() == effect_id)
			{
				if (cur_effectp->getType() != effect_type)
				{
					llwarns << "Viewer effect " << effect_id
							<< " update does not match effect type (effect type: "
							<< cur_effectp->getType() << " - update type: "
							<< effect_type << ")" << llendl;
				}
				effectp = cur_effectp;
				break;
			}
			++iter;
		}

		if (effect_type)
		{
			if (!effectp)
			{
				effectp = LLHUDManager::createEffect(effect_type, false,
													 false);
			}
			if (effectp)
			{
				effectp->unpackData(mesgsys, k);
			}
		}
		else
		{
			llwarns << "Received viewer effect " << effect_id
					<< " without type; skipped." << llendl;
		}
	}
}
