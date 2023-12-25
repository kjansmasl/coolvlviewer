/** 
 * @file llfloatertelehub.cpp
 * @author James Cook
 * @brief LLFloaterTelehub class implementation
 *
 * $LicenseInfo:firstyear=2005&license=viewergpl$
 * 
 * Copyright (c) 2005-2009, Linden Research, Inc.
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

#include "llfloatertelehub.h"

#include "llbutton.h"
#include "llscrolllistctrl.h"
#include "lluictrlfactory.h"
#include "llmessage.h"

#include "llagent.h"
#include "llfloatertools.h"
#include "llselectmgr.h"
#include "lltoolcomp.h"
#include "lltoolmgr.h"
#include "llviewerobjectlist.h"

LLFloaterTelehub::LLFloaterTelehub(const LLSD&)
:	mTelehubObjectID(),
	mTelehubObjectName(),
	mTelehubPos(),
	mTelehubRot(),
	mNumSpawn(0)
{
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_telehub.xml");
}

LLFloaterTelehub::~LLFloaterTelehub()
{
	// no longer interested in this message
	gMessageSystemp->setHandlerFunc("TelehubInfo", NULL);
}

bool LLFloaterTelehub::postBuild()
{
	gMessageSystemp->setHandlerFunc("TelehubInfo", processTelehubInfo);

	mConnectBtn = getChild<LLButton>("connect_btn");
	mConnectBtn->setClickedCallback(onClickConnect, this);

	mDisconnectBtn = getChild<LLButton>("disconnect_btn");
	mDisconnectBtn->setClickedCallback(onClickDisconnect, this);

	mAddSpawnBtn = getChild<LLButton>("add_spawn_point_btn");
	mAddSpawnBtn->setClickedCallback(onClickAddSpawnPoint, this);

	mRemoveSpawnBtn = getChild<LLButton>("remove_spawn_point_btn");
	mRemoveSpawnBtn->setClickedCallback(onClickRemoveSpawnPoint, this);

	mSpawnPointsList = getChild<LLScrollListCtrl>("spawn_points_list");
	// otherwise you can't walk with arrow keys while floater is up:
	mSpawnPointsList->setAllowKeyboardMovement(false);

	mObjectSelection = gSelectMgr.getEditSelection();

	// Show tools floater by selecting translate (select) tool
	gToolMgr.setCurrentToolset(gBasicToolset);
	gToolMgr.getCurrentToolset()->selectTool(&gToolCompTranslate);

	// Find tools floater, glue to bottom
	if (gFloaterToolsp)
	{
		LLRect tools_rect = gFloaterToolsp->getRect();
		S32 our_width = getRect().getWidth();
		S32 our_height = getRect().getHeight();
		LLRect our_rect;
		our_rect.setLeftTopAndSize(tools_rect.mLeft, tools_rect.mBottom,
								   our_width, our_height);
		setRect(our_rect);
	}

	sendTelehubInfoRequest();

	return true;
}

void LLFloaterTelehub::draw()
{
	if (!isMinimized())
	{
		refresh();
	}
	LLFloater::draw();
}

// Per-frame updates, because we don't have a selection manager observer.
void LLFloaterTelehub::refresh()
{
	bool have_selection = mObjectSelection->getFirstRootObject(true) != NULL;
	bool all_volume = gSelectMgr.selectionAllPCode(LL_PCODE_VOLUME);
	mConnectBtn->setEnabled(have_selection && all_volume);

	bool have_telehub = mTelehubObjectID.notNull();
	mDisconnectBtn->setEnabled(have_telehub);

	bool space_avail = (mNumSpawn < MAX_SPAWNPOINTS_PER_TELEHUB);
	mAddSpawnBtn->setEnabled(have_selection && all_volume && space_avail);

	bool enable_remove = mSpawnPointsList->getFirstSelected() != NULL;
	mRemoveSpawnBtn->setEnabled(enable_remove);
}

void LLFloaterTelehub::unpackTelehubInfo(LLMessageSystem* msg)
{
	msg->getUUID("TelehubBlock", "ObjectID", mTelehubObjectID);
	msg->getString("TelehubBlock", "ObjectName", mTelehubObjectName);
	msg->getVector3("TelehubBlock", "TelehubPos", mTelehubPos);
	msg->getQuat("TelehubBlock", "TelehubRot", mTelehubRot);

	mNumSpawn = msg->getNumberOfBlocks("SpawnPointBlock");
	for (S32 i = 0; i < mNumSpawn; ++i)
	{
		msg->getVector3("SpawnPointBlock", "SpawnPointPos",
						mSpawnPointPos[i], i);
	}

	// Update parts of the UI that change only when message received.

	if (mTelehubObjectID.isNull())
	{
		childSetVisible("status_text_connected", false);
		childSetVisible("status_text_not_connected", true);
		childSetVisible("help_text_connected", false);
		childSetVisible("help_text_not_connected", true);
	}
	else
	{
		childSetTextArg("status_text_connected", "[OBJECT]",
						mTelehubObjectName);
		childSetVisible("status_text_connected", true);
		childSetVisible("status_text_not_connected", false);
		childSetVisible("help_text_connected", true);
		childSetVisible("help_text_not_connected", false);
	}

	mSpawnPointsList->deleteAllItems();
	for (S32 i = 0; i < mNumSpawn; ++i)
	{
		std::string pos = llformat("%.1f, %.1f, %.1f", 
								   mSpawnPointPos[i].mV[VX],
								   mSpawnPointPos[i].mV[VY],
								   mSpawnPointPos[i].mV[VZ]);
		mSpawnPointsList->addSimpleElement(pos);
	}
	mSpawnPointsList->selectNthItem(mNumSpawn - 1);
}

// static
void LLFloaterTelehub::addBeacons()
{
	LLFloaterTelehub* self = findInstance();
	if (!self) return;

	// Find the telehub position, either our cached old position, or an updated
	// one based on the actual object position.
	LLVector3 hub_pos_region = self->mTelehubPos;
	LLQuaternion hub_rot = self->mTelehubRot;
	LLViewerObject* obj = gObjectList.findObject(self->mTelehubObjectID);
	if (obj)
	{
		hub_pos_region = obj->getPositionRegion();
		hub_rot = obj->getRotationRegion();
	}
	// Draw nice thick 3-pixel lines.
	gObjectList.addDebugBeacon(hub_pos_region, "", LLColor4::yellow,
							   LLColor4::white, 4);

	S32 spawn_index = self->mSpawnPointsList->getFirstSelectedIndex();
	if (spawn_index >= 0)
	{
		LLVector3 spawn_pos = hub_pos_region +
							  self->mSpawnPointPos[spawn_index] * hub_rot;
		gObjectList.addDebugBeacon(spawn_pos, "", LLColor4::orange,
								   LLColor4::white, 4);
	}
}

void LLFloaterTelehub::sendTelehubInfoRequest()
{
	gSelectMgr.sendGodlikeRequest("telehub", "info ui");
}

// static 
void LLFloaterTelehub::onClickConnect(void*)
{
	gSelectMgr.sendGodlikeRequest("telehub", "connect");
}

// static 
void LLFloaterTelehub::onClickDisconnect(void*)
{
	gSelectMgr.sendGodlikeRequest("telehub", "delete");
}

// static 
void LLFloaterTelehub::onClickAddSpawnPoint(void*)
{
	gSelectMgr.sendGodlikeRequest("telehub", "spawnpoint add");
	gSelectMgr.deselectAll();
}

// static 
void LLFloaterTelehub::onClickRemoveSpawnPoint(void* data)
{
	LLFloaterTelehub* self = (LLFloaterTelehub*)data;
	if (!self) return;

	S32 spawn_index = self->mSpawnPointsList->getFirstSelectedIndex();
	if (spawn_index < 0) return;  // nothing selected

	LLMessageSystem* msg = gMessageSystemp;
	if (!msg) return;

	// Could be god or estate owner.  If neither, server will reject message.
	if (gAgent.isGodlike())
	{
		msg->newMessage("GodlikeMessage");
	}
	else
	{
		msg->newMessage("EstateOwnerMessage");
	}
	msg->nextBlock("AgentData");
	msg->addUUID("AgentID", gAgentID);
	msg->addUUID("SessionID", gAgentSessionID);
	msg->addUUIDFast(_PREHASH_TransactionID, LLUUID::null); // not used
	msg->nextBlock("MethodData");
	msg->addString("Method", "telehub");
	msg->addUUID("Invoice", LLUUID::null);

	msg->nextBlock("ParamList");
	msg->addString("Parameter", "spawnpoint remove");

	std::string buffer = llformat("%d", spawn_index);
	msg->nextBlock("ParamList");
	msg->addString("Parameter", buffer);

	gAgent.sendReliableMessage();
}

// static 
void LLFloaterTelehub::processTelehubInfo(LLMessageSystem* msg, void**)
{
	LLFloaterTelehub* self = findInstance();
	if (self && msg)
	{
		self->unpackTelehubInfo(msg);
	}
}
