/** 
 * @file llfloatertelehub.h
 * @author James Cook
 * @brief LLFloaterTelehub class definition
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

#ifndef LL_LLFLOATERTELEHUB_H
#define LL_LLFLOATERTELEHUB_H

#include "llfloater.h"
#include "llsafehandle.h"

class LLButton;
class LLMessageSystem;
class LLObjectSelection;
class LLScrollListCtrl;

constexpr S32 MAX_SPAWNPOINTS_PER_TELEHUB = 16;

class LLFloaterTelehub final : public LLFloater,
							   public LLFloaterSingleton<LLFloaterTelehub>
{
	friend class LLUISingleton<LLFloaterTelehub, VisibilityPolicy<LLFloater> >;

public:
	~LLFloaterTelehub() override;

	bool postBuild() override;
	void draw() override;
	void refresh() override;

	LL_INLINE static bool renderBeacons()
	{
		return findInstance() && getInstance()->mTelehubObjectID.notNull();
	}

	static void addBeacons();

private:
	// Open only via LLFloaterSingleton interface, i.e. showInstance() or
	// toggleInstance().
	LLFloaterTelehub(const LLSD&);

	void sendTelehubInfoRequest();

	void unpackTelehubInfo(LLMessageSystem* msg);

	static void processTelehubInfo(LLMessageSystem* msg, void**);

	static void onClickConnect(void*);
	static void onClickDisconnect(void*);
	static void onClickAddSpawnPoint(void*);
	static void onClickRemoveSpawnPoint(void* data);

private:
	LLSafeHandle<LLObjectSelection>	mObjectSelection;

	LLButton*			mConnectBtn;
	LLButton*			mDisconnectBtn;
	LLButton*			mAddSpawnBtn;
	LLButton*			mRemoveSpawnBtn;
	LLScrollListCtrl*	mSpawnPointsList;

	LLUUID				mTelehubObjectID;	// null if no telehub

	// region local, fallback if viewer can't see the object
	LLVector3			mTelehubPos;

	LLQuaternion		mTelehubRot;

	S32					mNumSpawn;
	LLVector3			mSpawnPointPos[MAX_SPAWNPOINTS_PER_TELEHUB];

	std::string			mTelehubObjectName;
};

#endif
