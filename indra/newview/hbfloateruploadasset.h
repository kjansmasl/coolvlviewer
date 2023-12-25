/**
 * @file hbfloateruploadasset.h
 * @brief HBFloaterUploadAsset class definition
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

#ifndef LL_HBFLOATERUPLOADASSET_H
#define LL_HBFLOATERUPLOADASSET_H

#include "llfloater.h"

class LLButton;
class LLLineEditor;

class HBFloaterUploadAsset : public LLFloater
{
protected:
	LOG_CLASS(HBFloaterUploadAsset);

public:
	// Note inventory_type is to pick in the LLInventoryType::Etype enum;
	// passed here as a S32 (since this is also what it is), to avoid including
	// llinventorytype.h here... It is currently only used to determine the
	// expected cost of the upload.
	HBFloaterUploadAsset(const std::string& filename, S32 inventory_type);

	bool postBuild() override;

protected:
	// This method uploads the file as an inventory asset, which will be
	// charged for mCost. Override if needed, like for image uploads to deal
	// with temporary (free) assets in OpenSim (mTempAsset is set true if
	// needed in the override), and with inventory thumbnails since they are
	// not inventory assets (the upload is then handed over to the thumbnail
	// floater). HB
	virtual void uploadAsset();

private:
	static void	onBtnOK(void*);
	static void	onBtnCancel(void*);

protected:
	LLButton*				mUploadButton;
	LLLineEditor*			mNameEditor;
	LLLineEditor*			mDescEditor;
	std::string				mFilenameAndPath;
	std::string				mFilename;
	S32						mCost;
	bool					mTempAsset;
};

// HBFloaterUploadSound derived class, in which only the constructor differs
// from the base class (it just passes the adequate inventory type and uses the
// floater XML definition for sounds upload).

class HBFloaterUploadSound final : HBFloaterUploadAsset
{
protected:
	LOG_CLASS(HBFloaterUploadSound);

public:
	HBFloaterUploadSound(const std::string& filename);
};

#endif  // LL_HBFLOATERUPLOADASSET_H
