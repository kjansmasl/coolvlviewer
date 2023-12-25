/**
 * @file llfloateravatartextures.h
 * @brief Debugging view showing underlying avatar textures and baked textures.
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

#ifndef LL_LLFLOATERAVATARTEXTURES_H
#define LL_LLFLOATERAVATARTEXTURES_H

#include "llavatarappearancedefines.h"
#include "llfloater.h"

class LLSpinCtrl;
class LLTextureCtrl;
class LLVOAvatar;

class LLFloaterAvatarTextures final : public LLFloater
{
protected:
	LOG_CLASS(LLFloaterAvatarTextures);

public:
	LLFloaterAvatarTextures(const LLUUID& id);
	~LLFloaterAvatarTextures() override;

	bool postBuild() override;
	void draw() override;

	void refresh() override;

	static LLFloaterAvatarTextures*	show(const LLUUID& id);

	static S32 getTextureIds(LLVOAvatar* avatarp,
							 LLAvatarAppearanceDefines::ETextureIndex te,
							 std::string& name, uuid_vec_t& ids);

private:
	static void onClickDump(void*);
	static void onClickRebake(void*);
	static void onCommitLayer(LLUICtrl*, void* userdata);

private:
	LLUUID					mID;
	std::string				mTitle;
	LLSpinCtrl*				mSpinLayer;
	LLTextureCtrl*			mTextures[LLAvatarAppearanceDefines::TEX_NUM_INDICES];
	F32						mLastRefresh;
	bool					mShallClose;

	typedef fast_hmap<LLUUID, LLFloaterAvatarTextures*> instances_map_t;
	static instances_map_t sInstances;
};

#endif
