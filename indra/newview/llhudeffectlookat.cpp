/**
 * @file llhudeffectlookat.cpp
 * @brief LLHUDEffectLookAt class implementation
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

#include "llhudeffectlookat.h"

#include "llanimationstates.h"
#include "llcachename.h"
#include "lldir.h"
#include "llnotifications.h"
#include "llrender.h"
#include "llxmltree.h"
#include "llmessage.h"

#include "llagent.h"
#include "lldrawable.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llviewercontrol.h"
#include "llviewerdisplay.h"		// For hud_render_text()
#include "llviewerobjectlist.h"
#include "llviewerwindow.h"
#include "llvoavatarself.h"

bool LLHUDEffectLookAt::sDebugLookAt = false;
U32 LLHUDEffectLookAt::sMaxLookAtBroadcastDist = 0;

// packet layout
constexpr S32 SOURCE_AVATAR = 0;
constexpr S32 TARGET_OBJECT = 16;
constexpr S32 TARGET_POS = 32;
constexpr S32 LOOKAT_TYPE = 56;
constexpr S32 PKT_SIZE = 57;

constexpr F32 MAX_SENDS_PER_SEC = 4.f;				// Throttle
constexpr F32 MIN_DELTAPOS_FOR_UPDATE = 0.05f;
constexpr F32 MIN_TARGET_OFFSET_SQUARED = 0.0001f;
// Cannot use actual F32_MAX, because we add this to the current frametime
constexpr F32 MAX_TIMEOUT = F32_MAX * 0.5f;

/**
 * Simple data class holding values for a particular type of attention.
 */
class LLAttention
{
public:
	LLAttention()
	:	mTimeout(0.f),
		mPriority(0.f)
	{
	}

	LLAttention(F32 timeout, F32 priority, const std::string& name,
				LLColor3 color)
	:	mTimeout(timeout),
		mPriority(priority),
		mName(name),
		mColor(color)
	{
	}

	F32			mTimeout;
	F32			mPriority;
	LLColor3	mColor;
	std::string	mName;
};

/**
 * Simple data class holding a list of attentions, one for every type.
 */
class LLAttentionSet
{
public:
	LLAttentionSet(const LLAttention attentions[])
	{
		for (S32 i = 0; i < LOOKAT_NUM_TARGETS; ++i)
		{
			mAttentions[i] = attentions[i];
		}
	}

	LL_INLINE LLAttention& operator[](int idx)	{ return mAttentions[idx]; }

public:
	LLAttention mAttentions[LOOKAT_NUM_TARGETS];
};

// Default attribute set data.
// Used to initialize the global attribute set objects, one of which will be
// refered to by the hud object at any given time.
// Note that the values below are only the default values and that any or all of them
// can be overwritten with customizing data from the XML file. The actual values below
// are those that will give exactly the same look-at behavior as before the ability
// to customize was added. - MG
static const
	LLAttention
		// Default set of masculine attentions
		BOY_ATTS[] = {
			// LOOKAT_TARGET_NONE
			LLAttention(MAX_TIMEOUT, 0, "None",
						LLColor3(0.3f, 0.3f, 0.3f)),
			// LOOKAT_TARGET_IDLE
			LLAttention(3.f, 1, "Idle",
						LLColor3(0.5f, 0.5f, 0.5f)),
			// LOOKAT_TARGET_AUTO_LISTEN
			LLAttention(4.f, 3, "AutoListen",
						LLColor3(0.5f, 0.5f, 0.5f)),
			// LOOKAT_TARGET_FREELOOK
			LLAttention(2.f, 2, "FreeLook",
						LLColor3(0.5f, 0.5f, 0.9f)),
			// LOOKAT_TARGET_RESPOND
			LLAttention(4.f, 3, "Respond",
						LLColor3(0.f, 0.f, 0.f)),
			// LOOKAT_TARGET_HOVER
			LLAttention(1.f, 4, "Hover",
						LLColor3(0.5f, 0.9f, 0.5f)),
			// LOOKAT_TARGET_CONVERSATION
			LLAttention(MAX_TIMEOUT, 0, "Conversation",
						LLColor3(0.1f, 0.1f, 0.5f)),
			// LOOKAT_TARGET_SELECT
			LLAttention(MAX_TIMEOUT, 6, "Select",
						LLColor3(0.9f, 0.5f, 0.5f)),
			// LOOKAT_TARGET_FOCUS
			LLAttention(MAX_TIMEOUT, 6, "Focus",
						LLColor3(0.9f, 0.5f, 0.9f)),
			// LOOKAT_TARGET_MOUSELOOK
			LLAttention(MAX_TIMEOUT, 7, "Mouselook",
						LLColor3(0.9f, 0.9f, 0.5f)),
			// LOOKAT_TARGET_CLEAR
			LLAttention(0.f, 8, "Clear",
						LLColor3(1.f, 1.f, 1.f)),
		},
		// Default set of feminine attentions
		GIRL_ATTS[] = {
			// LOOKAT_TARGET_NONE
			LLAttention(MAX_TIMEOUT, 0, "None",
						LLColor3(0.3f, 0.3f, 0.3f)),
			// LOOKAT_TARGET_IDLE
			LLAttention(3.f, 1, "Idle",
						LLColor3(0.5f, 0.5f, 0.5f)),
			// LOOKAT_TARGET_AUTO_LISTEN
			LLAttention(4.f, 3, "AutoListen",
						LLColor3(0.5f, 0.5f, 0.5f)),
			// LOOKAT_TARGET_FREELOOK
			LLAttention(2.f, 2, "FreeLook",
						LLColor3(0.5f, 0.5f, 0.9f)),
			// LOOKAT_TARGET_RESPOND
			LLAttention(4.f, 3, "Respond",
						LLColor3(0.f, 0.f, 0.f)),
			// LOOKAT_TARGET_HOVER
			LLAttention(1.f, 4, "Hover",
						LLColor3(0.5f, 0.9f, 0.5f)),
			// LOOKAT_TARGET_CONVERSATION
			LLAttention(MAX_TIMEOUT, 0, "Conversation",
						LLColor3(0.1f, 0.1f, 0.5f)),
			// LOOKAT_TARGET_SELECT
			LLAttention(MAX_TIMEOUT, 6, "Select",
						LLColor3(0.9f, 0.5f, 0.5f)),
			// LOOKAT_TARGET_FOCUS
			LLAttention(MAX_TIMEOUT, 6, "Focus",
						LLColor3(0.9f, 0.5f, 0.9f)),
			// LOOKAT_TARGET_MOUSELOOK
			LLAttention(MAX_TIMEOUT, 7, "Mouselook",
						LLColor3(0.9f, 0.9f, 0.5f)),
			// LOOKAT_TARGET_CLEAR
			LLAttention(0.f, 8, "Clear",
						LLColor3(1.f, 1.f, 1.f)),
		};

static LLAttentionSet sBoyAttentions(BOY_ATTS);
static LLAttentionSet sGirlAttentions(GIRL_ATTS);

static bool loadGender(LLXmlTreeNode* gender)
{
	if (!gender)
	{
		return false;
	}

	std::string str;
	gender->getAttributeString("name", str);
	LLAttentionSet& attentions =
		str.compare("Masculine") == 0 ? sBoyAttentions : sGirlAttentions;

	for (LLXmlTreeNode* attention_node = gender->getChildByName("param");
		 attention_node; attention_node = gender->getNextNamedChild())
	{
		S32 index;
		attention_node->getAttributeString("attention", str);
		if (str == "idle")
		{
			index = LOOKAT_TARGET_IDLE;
		}
		else if (str == "auto_listen")
		{
			index = LOOKAT_TARGET_AUTO_LISTEN;
		}
		else if (str == "freelook")
		{
			index = LOOKAT_TARGET_FREELOOK;
		}
		else if (str == "respond")
		{
			index = LOOKAT_TARGET_RESPOND;
		}
		else if (str == "hover")
		{
			index = LOOKAT_TARGET_HOVER;
		}
		else if (str == "conversation")
		{
			index = LOOKAT_TARGET_CONVERSATION;
		}
		else if (str == "select")
		{
			index = LOOKAT_TARGET_SELECT;
		}
		else if (str == "focus")
		{
			index = LOOKAT_TARGET_FOCUS;
		}
		else if (str == "mouselook")
		{
			index = LOOKAT_TARGET_MOUSELOOK;
		}
		else
		{
			return false;
		}
		LLAttention* attention = &attentions[index];

		F32 priority, timeout;
		attention_node->getAttributeF32("priority", priority);
		attention_node->getAttributeF32("timeout", timeout);
		if (timeout < 0)
		{
			timeout = MAX_TIMEOUT;
		}
		attention->mPriority = priority;
		attention->mTimeout = timeout;
	}

	return true;
}

static bool loadAttentions()
{
	static bool first_time = true;
	if (!first_time)
	{
		// Maybe not ideal but otherwise it can continue to fail forever.
		return true;
	}
	first_time = false;

	std::string filename;
	filename = gDirUtilp->getExpandedFilename(LL_PATH_CHARACTER,
											  "attentions.xml");
	LLXmlTree xml_tree;
	bool success = xml_tree.parseFile(filename, false);
	if (!success)
	{
		return false;
	}
	LLXmlTreeNode* root = xml_tree.getRoot();
	if (!root)
	{
		return false;
	}

	//-------------------------------------------------------------------------
	// <linden_attentions version="1.0"> (root)
	//-------------------------------------------------------------------------
	if (!root->hasName("linden_attentions"))
	{
		llwarns << "Invalid linden_attentions file header: " << filename
				<< llendl;
		return false;
	}

	static LLStdStringHandle version_string =
		LLXmlTree::addAttributeString("version");
	std::string version;
	if (!root->getFastAttributeString(version_string, version) ||
		version != "1.0")
	{
		llwarns << "Invalid linden_attentions file version: " << version
				<< llendl;
		return false;
	}

	//-------------------------------------------------------------------------
	// <gender>
	//-------------------------------------------------------------------------
	for (LLXmlTreeNode* child = root->getChildByName("gender"); child;
		 child = root->getNextNamedChild())
	{
		if (!loadGender(child))
		{
			return false;
		}
	}

	return true;
}

LLHUDEffectLookAt::LLHUDEffectLookAt(U8 type)
:	LLHUDEffect(type),
	mKillTime(0.f),
	mLastSendTime(0.f),
	mNotifyTime(0.f),
	mNotified(false)
{
	clearLookAtTarget();
	// Parse the default sets
	loadAttentions();
	// Initialize current attention set. Switches when avatar sex changes.
	mAttentions = &sGirlAttentions;
}

//static
void LLHUDEffectLookAt::updateSettings()
{
	if (gSavedSettings.getBool("PrivateLookAt"))
	{
		sMaxLookAtBroadcastDist = gSavedSettings.getU32("PrivateLookAtLimit");
	}
	else
	{
		sMaxLookAtBroadcastDist = U32_MAX;
	}
}

void LLHUDEffectLookAt::packData(LLMessageSystem* mesgsys)
{
	// Pack the default data
	LLHUDEffect::packData(mesgsys);

	// Pack the type-specific data. Uses a fun packed binary format. Whee !
	U8 packed_data[PKT_SIZE];
	memset(packed_data, 0, PKT_SIZE);

	if (mSourceObject)
	{
		htonmemcpy(&(packed_data[SOURCE_AVATAR]), mSourceObject->mID.mData,
				   MVT_LLUUID, 16);
	}
	else
	{
		htonmemcpy(&(packed_data[SOURCE_AVATAR]), LLUUID::null.mData,
				   MVT_LLUUID, 16);
	}

	// Pack both target object and position; position interpreted as offset if
	// target object is non-null.
	if (mTargetObject)
	{
		htonmemcpy(&(packed_data[TARGET_OBJECT]), mTargetObject->mID.mData,
				   MVT_LLUUID, 16);
	}
	else
	{
		htonmemcpy(&(packed_data[TARGET_OBJECT]), LLUUID::null.mData,
				   MVT_LLUUID, 16);
	}

	htonmemcpy(&(packed_data[TARGET_POS]), mTargetOffsetGlobal.mdV,
			   MVT_LLVector3d, 24);

	U8 lookAtTypePacked = (U8)mTargetType;

	htonmemcpy(&(packed_data[LOOKAT_TYPE]), &lookAtTypePacked, MVT_U8, 1);

	mesgsys->addBinaryDataFast(_PREHASH_TypeData, packed_data, PKT_SIZE);

	mLastSendTime = mTimer.getElapsedTimeF32();
}

void LLHUDEffectLookAt::unpackData(LLMessageSystem* mesgsys, S32 blocknum)
{
	LLVector3d new_target;
	U8 packed_data[PKT_SIZE];

	LLUUID data_id;
	mesgsys->getUUIDFast(_PREHASH_Effect, _PREHASH_ID, data_id, blocknum);

	if (gAgent.mLookAt.notNull() && data_id == gAgent.mLookAt->getID())
	{
		return;
	}

	LLHUDEffect::unpackData(mesgsys, blocknum);
	LLUUID source_id;
	LLUUID target_id;
	S32 size = mesgsys->getSizeFast(_PREHASH_Effect, blocknum,
									_PREHASH_TypeData);
	if (size != PKT_SIZE)
	{
		llwarns << "LookAt effect with bad size " << size << llendl;
		return;
	}
	mesgsys->getBinaryDataFast(_PREHASH_Effect, _PREHASH_TypeData, packed_data,
							   PKT_SIZE, blocknum);

	htonmemcpy(source_id.mData, &(packed_data[SOURCE_AVATAR]), MVT_LLUUID, 16);

	LLVOAvatar* avatarp = gObjectList.findAvatar(source_id);
	if (!avatarp)	// It does happen
	{
		return;
	}
	setSourceObject(avatarp);

	htonmemcpy(target_id.mData, &(packed_data[TARGET_OBJECT]), MVT_LLUUID, 16);

	htonmemcpy(new_target.mdV, &(packed_data[TARGET_POS]), MVT_LLVector3d, 24);

	LLViewerObject* objp = gObjectList.findObject(target_id);
	if (objp)
	{
		setTargetObjectAndOffset(objp, new_target);
	}
	else if (target_id.isNull())
	{
		setTargetPosGlobal(new_target);
	}

	U8 type_unpacked = 0;
	htonmemcpy(&type_unpacked, &(packed_data[LOOKAT_TYPE]), MVT_U8, 1);
	mTargetType = (ELookAtType)type_unpacked;
	if (mTargetType == LOOKAT_TARGET_NONE)
	{
		clearLookAtTarget();
	}
}

void LLHUDEffectLookAt::setTargetObjectAndOffset(LLViewerObject* objp,
												 const LLVector3d& offset)
{
	mTargetObject = objp;
	mTargetOffsetGlobal = offset;
	mNotifyTime = 0.f;
}

void LLHUDEffectLookAt::setTargetPosGlobal(const LLVector3d& target_pos_global)
{
	mTargetObject = NULL;
	mTargetOffsetGlobal = target_pos_global;
	mNotifyTime = 0.f;
}

// Called by agent logic to set look at behavior locally, and propagate to sim
bool LLHUDEffectLookAt::setLookAt(ELookAtType target_type,
								  LLViewerObject* object,
								  LLVector3 position)
{
	if (!mSourceObject)
	{
		return false;
	}

	if (target_type >= LOOKAT_NUM_TARGETS)
	{
		llwarns << "Bad target_type " << (S32)target_type << " - ignoring."
				<< llendl;
		return false;
	}

	// Must be same or higher priority than existing effect
	if ((*mAttentions)[target_type].mPriority <
			(*mAttentions)[mTargetType].mPriority)
	{
		return false;
	}

	// If type of lookat behavior or target object has changed...
	if (target_type != mTargetType || object != mTargetObject ||
		// or if lookat position has moved a certain amount and we do not have
		// just sent an update
		(dist_vec(position, mLastSentOffsetGlobal) > MIN_DELTAPOS_FOR_UPDATE &&
		 mTimer.getElapsedTimeF32() - mLastSendTime > 1.f / MAX_SENDS_PER_SEC))
	{
		mLastSentOffsetGlobal = position;
		F32 timeout = (*mAttentions)[target_type].mTimeout;
		setDuration(timeout);
		setNeedsSendToSim(true);
	}

	if (target_type == LOOKAT_TARGET_CLEAR)
	{
		clearLookAtTarget();
	}
	else
	{
		mTargetType = target_type;
		mTargetObject = object;
		if (object)
		{
			mTargetOffsetGlobal.set(position);
		}
		else
		{
			mTargetOffsetGlobal = gAgent.getPosGlobalFromAgent(position);
		}
		mKillTime = mTimer.getElapsedTimeF32() + mDuration;

		// This is *required* to update the sim *at once* (even though update()
		// is called at each frame), else your avatar's eyes might end up
		// looking behind its head in everyone else's viewer... HB
		update();
	}
	return true;
}

void LLHUDEffectLookAt::clearLookAtTarget()
{
	mTargetObject = NULL;
	mTargetOffsetGlobal.clear();
	mTargetType = LOOKAT_TARGET_NONE;
	mNotifyTime = 0.f;
	if (mSourceObject.notNull())
	{
		LLVOAvatar* avatarp = (LLVOAvatar*)mSourceObject.get();
		avatarp->stopMotion(ANIM_AGENT_HEAD_ROT);
	}
}

void LLHUDEffectLookAt::markDead()
{
	if (mSourceObject.notNull())
	{
		LLVOAvatar* avatarp = (LLVOAvatar*)mSourceObject.get();
		avatarp->removeAnimationData("LookAtPoint");
	}

	mSourceObject = NULL;
	clearLookAtTarget();
	LLHUDEffect::markDead();
}

void LLHUDEffectLookAt::setSourceObject(LLViewerObject* objectp)
{
	// Restrict source objects to avatars
	if (objectp && objectp->isAvatar())
	{
		LLHUDEffect::setSourceObject(objectp);
	}
}

void LLHUDEffectLookAt::render()
{
	static const LLFontGL* font = LLFontGL::getFontSansSerif();

	if (!isAgentAvatarValid()) return;

	if (sDebugLookAt && mSourceObject.notNull())
	{
//MK
		if (gRLenabled && gRLInterface.mVisionRestricted)
		{
			return;
		}
//mk
		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

		LLVOAvatar* avatarp = (LLVOAvatar*)mSourceObject.get();
		LLVector3 lookerpos = avatarp->mHeadp->getWorldPosition();
		LLVector3 target = mTargetPos + lookerpos;
		gGL.matrixMode(LLRender::MM_MODELVIEW);
		gGL.pushMatrix();
		gGL.translatef(target.mV[VX], target.mV[VY], target.mV[VZ]);
		gGL.scalef(0.3f, 0.3f, 0.3f);
		gGL.begin(LLRender::LINES);
		{
			LLColor3 color = (*mAttentions)[mTargetType].mColor;
			gGL.color3f(color.mV[VRED], color.mV[VGREEN], color.mV[VBLUE]);
			gGL.vertex3f(-1.f, 0.f, 0.f);
			gGL.vertex3f(1.f, 0.f, 0.f);

			gGL.vertex3f(0.f, -1.f, 0.f);
			gGL.vertex3f(0.f, 1.f, 0.f);

			gGL.vertex3f(0.f, 0.f, -1.f);
			gGL.vertex3f(0.f, 0.f, 1.f);
		}
		gGL.end();
		gGL.popMatrix();

//MK
		if (gRLenabled &&
			(gRLInterface.mContainsShownames ||
			 gRLInterface.mContainsShownametags))
		{
			return;
		}
//mk
		LLVector3 offset = gAgentAvatarp->mHeadp->getWorldPosition() -
						   lookerpos;
		if (offset.length() <= (F32)sMaxLookAtBroadcastDist)
		{
			// Render name
			LLWString text(utf8str_to_wstring(avatarp->getFullname(true)));
			offset = (gAgent.getCameraPositionAgent() - target) * 0.5f;
			offset.normalize();
			LLVector3 shadow_offset = offset * 0.99f;
			F32 delta_x = -0.5f * font->getWidthF32(text.c_str());
			LLGLEnable gl_blend(GL_BLEND);
			gGL.pushMatrix();
			gViewerWindowp->setupViewport();
			hud_render_text(text, target + shadow_offset, *font,
							LLFontGL::NORMAL, delta_x + 1.f, -1.f,
							LLColor4::black, false);
			hud_render_text(text, target + offset, *font,
							LLFontGL::NORMAL, delta_x, 0.f,
							(*mAttentions)[mTargetType].mColor, false);
			gGL.popMatrix();
		}
	}
}

static void looked_at_cb(const LLUUID& id, const LLAvatarName& av_name)
{
	std::string fullname;
	if (LLAvatarNameCache::useDisplayNames())
	{
		if (LLAvatarNameCache::useDisplayNames() == 2)
		{
			fullname = av_name.mDisplayName;
		}
		else
		{
			fullname = av_name.getNames();
		}
	}
	else
	{
		fullname = av_name.getLegacyName();
	}

	LLSD args;
	args["NAME"] = fullname;
	gNotifications.add("LookedAt", args);
}

void LLHUDEffectLookAt::update()
{
	if (!isAgentAvatarValid()) return;

	// If the target object is dead, set the target object to NULL
	if (mTargetObject.notNull() && mTargetObject->isDead())
	{
		clearLookAtTarget();
	}

	// If source avatar is null or dead, mark self as dead and return
	if (mSourceObject.isNull() || mSourceObject->isDead())
	{
		markDead();
		return;
	}

	// Make sure the proper set of avatar attention are currently being used.
	LLVOAvatar* avatarp = (LLVOAvatar*)((LLViewerObject*)mSourceObject);
	// For now the first cut will just switch on sex. future development could
	// adjust timeouts according to avatar age and/or other features.
	mAttentions = avatarp->getSex() == SEX_MALE ? &sBoyAttentions
												: &sGirlAttentions;

	F32 time = mTimer.getElapsedTimeF32();

	// Clear out the effect if time is up
	if (mKillTime != 0.f && time > mKillTime &&
		mTargetType != LOOKAT_TARGET_NONE)
	{
		clearLookAtTarget();
		// Look at timed out (only happens on own avatar), so tell everyone
		setNeedsSendToSim(true);
	}

	if (mTargetType != LOOKAT_TARGET_NONE && calcTargetPosition())
	{
		LLMotion* head_motion = avatarp->findMotion(ANIM_AGENT_HEAD_ROT);
		if (!head_motion || head_motion->isStopped())
		{
			avatarp->startMotion(ANIM_AGENT_HEAD_ROT);
		}
	}

	LLViewerObject* agentp = (LLViewerObject*)gAgentAvatarp;
	if (!mNotified && mTargetType == LOOKAT_TARGET_FOCUS &&
		mSourceObject != agentp && mTargetObject &&
		(mTargetObject == agentp ||
		 (mTargetObject->isAttachment() &&
		  mTargetObject->getRoot() == agentp)))
	{
		if (mNotifyTime == 0.f)
		{
//MK
			if (!gRLenabled || !gRLInterface.mVisionRestricted)
//mk
			{
				static LLCachedControl<U32> delay(gSavedSettings,
												  "LookAtNotifyDelay");
				if (delay > 0)
				{
					mNotifyTime = time + (F32)delay;
				}
			}
		}
		else if (time >= mNotifyTime)
		{
			LLVector3 offset = gAgentAvatarp->mHeadp->getWorldPosition() -
							   avatarp->mHeadp->getWorldPosition();
			if (offset.length() <= (F32)sMaxLookAtBroadcastDist &&
//MK
				(!gRLenabled ||
				 (!gRLInterface.mContainsShownames &&
				  !gRLInterface.mContainsShownametags &&
				  !gRLInterface.mVisionRestricted)))
//mk
			{
				mNotified = true;
				LLAvatarNameCache::get(avatarp->getID(),
									   boost::bind(&looked_at_cb, _1, _2));
			}
		}
	}

	if (sDebugLookAt)
	{
//MK
		if (gRLenabled && gRLInterface.mVisionRestricted)
		{
			return;
		}
//mk
		avatarp->addDebugText((*mAttentions)[mTargetType].mName);
	}
}

// Initializes the mTargetPos member from the current mSourceObject,
// mTargetObject and possibly mTargetOffsetGlobal. When mTargetObject is
// another avatar, it sets mTargetPos to be their eyes.
// Has the side-effect of also calling setAnimationData("LookAtPoint") with the
// new mTargetPos on the source object which is assumed to be an avatar.
// Returns whether we successfully calculated a finite target position.
bool LLHUDEffectLookAt::calcTargetPosition()
{
	LLVector3 local_offset;
	LLViewerObject* target_obj = (LLViewerObject*)mTargetObject;
	if (target_obj)
	{
		local_offset.set(mTargetOffsetGlobal);
	}
	else
	{
		local_offset = gAgent.getPosAgentFromGlobal(mTargetOffsetGlobal);
	}

	LLVOAvatar* avatarp = (LLVOAvatar*)mSourceObject.get();

	if (target_obj && target_obj->mDrawable.notNull())
	{
		LLQuaternion target_rot;
		if (target_obj->isAvatar())
		{
			LLVOAvatar* target_av = (LLVOAvatar*)target_obj;

			bool looking_at_self = avatarp->isSelf() && target_av->isSelf();

			// If selecting self, stare forward
			if (looking_at_self &&
				mTargetOffsetGlobal.lengthSquared() < MIN_TARGET_OFFSET_SQUARED)
			{
				// Set the lookat point in front of the avatar
				mTargetOffsetGlobal.set(5.0, 0.0, 0.0);
				local_offset.set(mTargetOffsetGlobal);
			}

			// Look the other avatar in the eye. note: what happens if target
			// is self ? -MG
			mTargetPos = target_av->mHeadp->getWorldPosition();
			if (mTargetType == LOOKAT_TARGET_MOUSELOOK ||
				mTargetType == LOOKAT_TARGET_FREELOOK)
			{
				// mouselook and freelook target offsets are absolute
				target_rot = LLQuaternion::DEFAULT;
			}
			else if (looking_at_self && gAgent.cameraCustomizeAvatar())
			{
				// *NOTE: We have to do this because animation overrides do not
				// set lookat behavior.
				// *TODO: animation overrides for lookat behavior.
				target_rot = target_av->mPelvisp->getWorldRotation();
			}
			else
			{
				target_rot = target_av->mRoot->getWorldRotation();
			}
		}
		else // Target obj is not an avatar
		{
			if (target_obj->mDrawable->getGeneration() == -1)
			{
				mTargetPos = target_obj->getPositionAgent();
				target_rot = target_obj->getWorldRotation();
			}
			else
			{
				mTargetPos = target_obj->getRenderPosition();
				target_rot = target_obj->getRenderRotation();
			}
		}

		mTargetPos += local_offset * target_rot;
	}
	else // No target obj or it is not drawable
	{
		mTargetPos = local_offset;
	}

	mTargetPos -= avatarp->mHeadp->getWorldPosition();

	if (!mTargetPos.isFinite())
	{
		return false;
	}

	avatarp->setAnimationData("LookAtPoint", (void*)&mTargetPos);

	return true;
}
