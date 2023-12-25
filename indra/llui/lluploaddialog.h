/**
 * @file lluploaddialog.h
 * @brief LLUploadDialog class header file
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

#ifndef LL_UPLOADDIALOG_H
#define LL_UPLOADDIALOG_H

#include "llpanel.h"

class LLFontGL;
class LLTextBox;

class LLUploadDialog : public LLPanel
{
public:
	// msg = message to display
	static LLUploadDialog*	modalUploadDialog(const std::string& msg);
	static void				modalUploadFinished();

	static bool				modalUploadIsFinished()	{ return sDialog == NULL; }

	void setMessage(const std::string& msg);

private:
	LLUploadDialog(const std::string& msg);
	// No you cannot kill it. It can only kill itself.
	~LLUploadDialog() override;

private:
	LLTextBox*				mLabelBox[16];
	const LLFontGL*			mFont;

	// Hidden singleton instance, created and destroyed as needed.
	static LLUploadDialog*	sDialog;
};

#endif  // LL_UPLOADDIALOG_H
