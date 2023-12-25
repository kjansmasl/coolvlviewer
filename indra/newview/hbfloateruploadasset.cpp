/**
 * @file hbfloateruploadasset.cpp
 * @brief HBFloaterUploadAsset class implementation
 *        This is a full rewrite of LL's LLFloaterNameDesc class.
 *
 * $LicenseInfo:firstyear=2023&license=viewergpl$
 *
 * Copyright (c) 2023, Henri Beauchamp.
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

#include "hbfloateruploadasset.h"

#include "llbutton.h"
#include "lldir.h"
#include "lleconomy.h"
#include "lllineeditor.h"
#include "lluictrlfactory.h"

#include "llfloaterimagepreview.h"
#include "llfloaterperms.h"
#include "llviewerassetupload.h"
#include "llviewercontrol.h"

HBFloaterUploadAsset::HBFloaterUploadAsset(const std::string& filename,
										   S32 inventory_type)
:	LLFloater("asset upload"),
	mFilenameAndPath(filename),
	mFilename(gDirUtilp->getBaseFileName(filename, false)),
	mTempAsset(false)
{
	switch (inventory_type)
	{
		case LLInventoryType::IT_TEXTURE:
			mCost = LLEconomy::getInstance()->getTextureUploadCost();
			break;

		case LLInventoryType::IT_SOUND:
			mCost = LLEconomy::getInstance()->getSoundUploadCost();
			break;

		case LLInventoryType::IT_ANIMATION:
			mCost = LLEconomy::getInstance()->getAnimationUploadCost();
			break;

		default:
			mCost = 0;
	}
}

bool HBFloaterUploadAsset::postBuild()
{
	setTitle(mFilename);

	std::string asset_name = mFilename;
	LLStringUtil::replaceNonstandardASCII(asset_name, '?');
	LLStringUtil::replaceChar(asset_name, '|', '?');
	LLStringUtil::stripNonprintable(asset_name);
	LLStringUtil::trim(asset_name);
	mNameEditor = getChild<LLLineEditor>("name_form");
	mNameEditor->setText(gDirUtilp->getBaseFileName(asset_name, true));
	mNameEditor->setMaxTextLength(DB_INV_ITEM_NAME_STR_LEN);
	mNameEditor->setPrevalidate(&LLLineEditor::prevalidatePrintableNotPipe);

	mDescEditor = getChild<LLLineEditor>("description_form");
	mDescEditor->setMaxTextLength(DB_INV_ITEM_DESC_STR_LEN);
	mDescEditor->setPrevalidate(&LLLineEditor::prevalidatePrintableNotPipe);

	// OK button
	mUploadButton = getChild<LLButton>("ok_btn");
	mUploadButton->setClickedCallback(onBtnOK, this);
	mUploadButton->setLabelArg("[AMOUNT]", llformat("%d", mCost));
	setDefaultBtn(mUploadButton);

	// Cancel button
	childSetAction("cancel_btn", onBtnCancel, this);

	center();

	return true;
}

//virtual
void HBFloaterUploadAsset::uploadAsset()
{
	// Upload a chargeable asset.
	LLResourceUploadInfo::ptr_t
		info(new LLNewFileResourceUploadInfo(mFilenameAndPath,
											 mNameEditor->getText(),
											 mDescEditor->getText(), 0,
											 LLFolderType::FT_NONE,
											 LLInventoryType::IT_NONE,
											 LLFloaterPerms::getNextOwnerPerms(),
											 LLFloaterPerms::getGroupPerms(),
											 LLFloaterPerms::getEveryonePerms(),
											 mCost));
	upload_new_resource(info, NULL, NULL, mTempAsset);
}

//static
void HBFloaterUploadAsset::onBtnOK(void* userdata)
{
	HBFloaterUploadAsset* self = (HBFloaterUploadAsset*)userdata;
	if (self)
	{
		// Do not allow inadvertent duplicate uploads
		self->mUploadButton->setEnabled(false);
		// This is potentially overridden. HB
		self->uploadAsset();
		// Whatever the result, we are done: close the floater.
		self->close();
	}
}

//static
void HBFloaterUploadAsset::onBtnCancel(void* userdata)
{
	HBFloaterUploadAsset* self = (HBFloaterUploadAsset*)userdata;
	if (self)
	{
		self->close();
	}
}

///////////////////////////////////////////////////////////////////////////////
// HBFloaterUploadSound class
///////////////////////////////////////////////////////////////////////////////

HBFloaterUploadSound::HBFloaterUploadSound(const std::string& filename)
:	HBFloaterUploadAsset(filename, LLInventoryType::IT_SOUND)
{
	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_sound_preview.xml");
}
