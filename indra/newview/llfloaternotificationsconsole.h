/**
 * @file llfloaternotificationsconsole.h
 * @brief Debugging console for unified notifications.
 *
 * $LicenseInfo:firstyear=2003&license=viewergpl$
 *
 * Copyright (c) 2003-2009, Linden Research, Inc.
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

#ifndef LL_LLFLOATERNOTIFICATIONCONSOLE_H
#define LL_LLFLOATERNOTIFICATIONCONSOLE_H

#include "llfloater.h"

class LLComboBox;
class LLLayoutStack;
class LLNotification;

class LLFloaterNotificationConsole
:	public LLFloater, public LLFloaterSingleton<LLFloaterNotificationConsole>
{
public:
	LLFloaterNotificationConsole(const LLSD& key);

	void addChannel(const std::string& type, bool open = false);

	void removeChannel(const std::string& type);
	void updateResizeLimits();

protected:
	void onClose(bool app_quitting) override;
	bool postBuild() override;

private:
	static void onClickAdd(void* userdata);

private:
	LLComboBox*		mNotifTypesCombo;
	LLLayoutStack*	mLayoutStack;
};

// Pop-up debugging view of a generic new notification.
class LLFloaterNotification : public LLFloater
{
public:
	LLFloaterNotification(LLNotification* notifp);

	void respond();

private:
	static void onCommitResponse(LLUICtrl*, void* userdata);

private:
	LLUUID		mNotificationId;
	LLComboBox*	mResponsesCombo;
};

#endif	// LL_LLFLOATERNOTIFICATIONCONSOLE_H
