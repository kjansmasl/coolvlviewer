/**
 * @file hbfloaterrlv.h
 * @brief The HBFloaterRLV and HBFloaterBlacklistRLV classes declarations
 *
 * $LicenseInfo:firstyear=2011&license=viewergpl$
 *
 * Copyright (c) 2011-2020, Henri Beauchamp
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

#ifndef LL_HBFLOATERRLV_H
#define LL_HBFLOATERRLV_H

#include "llfloater.h"

class LLButton;
class LLScrollListCtrl;
class LLTabContainer;

class HBFloaterRLV final : public LLFloater,
						   public LLFloaterSingleton<HBFloaterRLV>
{
	friend class LLUISingleton<HBFloaterRLV, VisibilityPolicy<LLFloater> >;

protected:
	LOG_CLASS(HBFloaterRLV);

	bool postBuild() override;
	void draw() override;
	void onOpen() override;

public:
	static void setDirty();

	// Command status
	enum { QUEUED = -1, FAILED = 0, EXECUTED = 1, IMPLICIT = 2, BLACKLISTED = 3 };

	class LoggedCommand
	{
	public:
		LoggedCommand(const LLUUID& id, const std::string& name,
					  const std::string& command, S32 status);
	public:
		LLUUID		mId;
		std::string	mName;
		std::string	mCommand;
		std::string mTimeStamp;
		S32			mStatus;
		bool		mIsLua;
		bool		mIsGone;
		bool		mIsRoot;
	};

	static void logCommand(const LLUUID& obj_id, const std::string& obj_name,
						   const std::string& command, S32 status = EXECUTED);

private:
	// Open only via LLFloaterSingleton interface, i.e. showInstance() or
	// toggleInstance().
	HBFloaterRLV(const LLSD&);

	void setButtonsStatus();

	static void onTabChanged(void* data, bool);
	static void onButtonHelp(void*);
	static void onButtonRefresh(void* data);
	static void onButtonClear(void* data);
	static void onButtonClose(void* data);
	static void onDoubleClick(void* data);

public:
	static std::string					sQueued;
	static std::string					sFailed;
	static std::string					sExecuted;
	static std::string					sBlacklisted;
	static std::string					sImplicit;
	static std::string					sUnrestrictedEmotes;

private:
	LLButton*							mRefreshButton;
	LLButton*							mClearButton;
	LLTabContainer*						mTabContainer;
	LLScrollListCtrl*					mStatusByObject;
	LLScrollListCtrl*					mRestrictions;
	LLScrollListCtrl*					mExceptions;
	LLScrollListCtrl*					mCommandsLog;
	U32									mLastCommandsLogSize;
	bool								mFirstOpen;
	bool								mIsDirty;

	static std::vector<LoggedCommand>	sLoggedCommands;
};

class HBFloaterBlacklistRLV final
:	public LLFloater, public LLFloaterSingleton<HBFloaterBlacklistRLV>
{
	friend class LLUISingleton<HBFloaterBlacklistRLV,
							   VisibilityPolicy<LLFloater> >;

protected:
	LOG_CLASS(HBFloaterBlacklistRLV);

private:
	// Open only via LLFloaterSingleton interface, i.e. showInstance() or
	// toggleInstance().
	HBFloaterBlacklistRLV(const LLSD&);

	bool postBuild() override;

	static void onButtonApply(void* data);
	static void onButtonCancel(void* data);
};

#endif
