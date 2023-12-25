/**
 * @file llfloatermarketplace.h
 * @brief LLFloaterMarketplaceValidation and LLFloaterAssociateListing classes
 *
 * $LicenseInfo:firstyear=2014&license=viewergpl$
 *
 * Copyright (c) 2014, Linden Research, Inc.
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

#ifndef LL_LLFLOATERMARKETPLACE_H
#define LL_LLFLOATERMARKETPLACE_H

#include "llfloater.h"
#include "llstyle.h"

class LLLineEditor;
class LLTextEditor;

class LLFloaterMarketplaceValidation final : public LLFloater
{
protected:
	LOG_CLASS(LLFloaterMarketplaceValidation);

public:
	~LLFloaterMarketplaceValidation() override;

	bool postBuild() override;
	void onOpen() override;

	void appendMessage(std::string& message, S32 depth, LLError::ELevel level);

	static void show(const LLUUID& folder_id);

private:
	// Open only via the show() method defined above
	LLFloaterMarketplaceValidation(const LLUUID& folder_id);

	static void onButtonOK(void* userdata);

private:
	LLUUID			mFolderId;
	LLTextEditor*	mEditor;
	LLStyleSP		mBoldStyle;
	bool			mTitleSet;
	bool			mGotMessages;

	typedef fast_hmap<LLUUID, LLFloaterMarketplaceValidation*> instances_map_t;
	static instances_map_t sInstances;
};

class LLFloaterAssociateListing final : public LLFloater
{
protected:
	LOG_CLASS(LLFloaterAssociateListing);

public:
	~LLFloaterAssociateListing() override;

	bool postBuild() override;
	bool handleKeyHere(KEY key, MASK mask) override;

	void apply(bool user_confirm = true);

	static void show(const LLUUID& folder_id);
	static LLFloaterAssociateListing* getInstance(const LLUUID& folder_id);

private:
	// Open only via the show() method defined above
	LLFloaterAssociateListing(const LLUUID& folder_id);

	static void onButtonOK(void* userdata);
	static void onButtonCancel(void* userdata);

private:
	LLUUID					mFolderId;
	LLLineEditor*			mInputLine;

	typedef fast_hmap<LLUUID, LLFloaterAssociateListing*> instances_map_t;
	static instances_map_t	sInstances;
};

#endif	// LL_LLFLOATERMARKETPLACE_H
