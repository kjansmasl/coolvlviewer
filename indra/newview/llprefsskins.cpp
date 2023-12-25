/** 
 * @file llprefsskins.cpp
 * @brief General preferences panel in preferences floater
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

#include "llviewerprecompiledheaders.h"

#include "llprefsskins.h"

#include "llbutton.h"
#include "lldir.h"
#include "llradiogroup.h"
#include "lluictrlfactory.h"

#include "llviewercontrol.h"

LLPrefSkins::LLPrefSkins()
{
	LLUICtrlFactory::getInstance()->buildPanel(this,
											   "panel_preferences_skins.xml");
}

bool LLPrefSkins::postBuild()
{
	mSkinsSelector = getChild<LLRadioGroup>("skin_selection");
	mSkinsSelector->setCommitCallback(onSelectSkin);
	mSkinsSelector->setCallbackUserData(this);

	std::string custom_colors =
		gDirUtilp->getExpandedFilename(LL_PATH_SKINS, "custom",
									   "colors_base.xml");
	if (!LLFile::exists(custom_colors))
	{
		LLRadioCtrl* custom_radio = mSkinsSelector->getRadioButton(3);
		if (custom_radio)
		{
			custom_radio->setEnabled(false);
		}
		if (gSavedSettings.getString("SkinCurrent") == "custom")
		{
			llwarns << "Skin 'custom' does not exist, switching to default skin."
					<< llendl;
			gSavedSettings.setString("SkinCurrent", "default");
		}
	}

	childSetAction("classic_preview", onClickClassic, this);
	childSetAction("silver_preview", onClickSilver, this);
	childSetAction("dark_preview", onClickDark, this);

	refresh();

	return true;
}

void LLPrefSkins::refresh()
{
	mSkin = gSavedSettings.getString("SkinCurrent");
	mSkinsSelector->setValue(mSkin);
}

void LLPrefSkins::apply()
{
	if (mSkin != gSavedSettings.getString("SkinCurrent"))
	{
		gNotifications.add("ChangeSkin");
		refresh();
	}
}

void LLPrefSkins::cancel()
{
	// Reverts any changes to current skin
	gSavedSettings.setString("SkinCurrent", mSkin);
}

//static
void LLPrefSkins::onSelectSkin(LLUICtrl* ctrl, void* data)
{
	std::string skin_selection = ctrl->getValue().asString();
	gSavedSettings.setString("SkinCurrent", skin_selection);
}

//static 
void LLPrefSkins::onClickClassic(void* data)
{
	LLPrefSkins* self = (LLPrefSkins*)data;
	gSavedSettings.setString("SkinCurrent", "default");
	self->mSkinsSelector->setValue("default");
}

//static 
void LLPrefSkins::onClickSilver(void* data)
{
	LLPrefSkins* self = (LLPrefSkins*)data;
	gSavedSettings.setString("SkinCurrent", "silver");
	self->mSkinsSelector->setValue("silver");
}

//static 
void LLPrefSkins::onClickDark(void* data)
{
	LLPrefSkins* self = (LLPrefSkins*)data;
	gSavedSettings.setString("SkinCurrent", "dark");
	self->mSkinsSelector->setValue("dark");
}
