/**
 * @file slfloatermediafilter.h
 * @brief The SLFloaterMediaFilter class declaration
 *
 * $LicenseInfo:firstyear=2011&license=viewergpl$
 *
 * Copyright (c) 2011, Sione Lomu
 * with debugging and improvements by Henri Beauchamp
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

#ifndef LL_SLFLOATERMEDIAFILTER_H
#define LL_SLFLOATERMEDIAFILTER_H

#include "llfloater.h"

class LLScrollListCtrl;

class SLFloaterMediaFilter final
:	public LLFloater, public LLFloaterSingleton<SLFloaterMediaFilter>
{
	friend class LLUISingleton<SLFloaterMediaFilter,
							   VisibilityPolicy<LLFloater> >;

protected:
	LOG_CLASS(SLFloaterMediaFilter);

public:
	bool postBuild() override;
	void draw() override;

	static void setDirty();

private:
	SLFloaterMediaFilter(const LLSD&);

	static void onClearLists(void*);
	static void onShowIPs(void*);
	static void onWhitelistAdd(void* data);
	static void onWhitelistRemove(void* data);
	static void onBlacklistAdd(void* data);
	static void onBlacklistRemove(void* data);
	static void onCommitDomain(void* data);

private:
	LLScrollListCtrl*	mWhitelistSLC;
	LLScrollListCtrl*	mBlacklistSLC;
	bool				mIsDirty;

	static bool			sIsWhitelist;
	static bool			sShowIPs;
};

#endif	// LL_SLFLOATERMEDIAFILTER_H
