/**
 * @file llfloaterpostcard.h
 * @brief Postcard send floater, allows setting name, e-mail address, etc.
 *
 * $LicenseInfo:firstyear=2004&license=viewergpl$
 *
 * Copyright (c) 2004-2009, Linden Research, Inc.
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

#ifndef LL_LLFLOATERPOSTCARD_H
#define LL_LLFLOATERPOSTCARD_H

#include "llfloater.h"
#include "llpointer.h"

class LLImageJPEG;
class LLLineEditor;
class LLTextEditor;
class LLViewerTexture;

class LLFloaterPostcard final : public LLFloater
{
protected:
	LOG_CLASS(LLFloaterPostcard);

public:
	LLFloaterPostcard(LLImageJPEG* jpeg, LLViewerTexture* img,
					  const LLVector2& img_scale,
					  const LLVector3d& pos_taken_global);

	~LLFloaterPostcard() override;

	bool postBuild() override;
	void draw() override;

	static LLFloaterPostcard* showFromSnapshot(LLImageJPEG* jpeg,
											   LLViewerTexture* img,
											   const LLVector2& img_scale,
											   const LLVector3d& pos_taken_global);

	static void onClickCancel(void* userdata);
	static void onClickSend(void* userdata);

	static void updateUserInfo(const std::string& email);

	static void onMsgFormFocusReceived(LLFocusableElement* receiver,
									   void* userdata);

	bool missingSubjMsgAlertCallback(const LLSD& notification,
									 const LLSD& response);

	void sendPostcard();

protected:
	static void sendPostcardFinished(LLSD result, void* userdata);

protected:
	LLLineEditor*				mFromLine;
	LLTextEditor*				mMessageText;

	LLPointer<LLImageJPEG>		mJPEGImage;
	LLPointer<LLViewerTexture>	mViewerImage;
	LLVector2					mImageScale;
	LLVector3d					mPosTakenGlobal;
	bool						mHasFirstMsgFocus;

	typedef std::set<LLFloaterPostcard*> instance_list_t;
	static instance_list_t		sInstances;
	static std::string			sUserEmail;
};

#endif // LL_LLFLOATERPOSTCARD_H
