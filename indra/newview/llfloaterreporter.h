/**
 * @file llfloaterreporter.h
 * @author Andrew Meadows
 * @brief Bug and abuse reports.
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

#ifndef LL_LLFLOATERREPORTER_H
#define LL_LLFLOATERREPORTER_H

#include <list>

#include "llextendedstatus.h"
#include "llfloater.h"
#include "llimage.h"
#include "llstring.h"
#include "llvector3.h"

class LLAvatarName;
class LLComboBox;
class LLMessageSystem;
class LLVector3d;
struct LLResourceData;

// These flags are used to label info requests to the server:
#if 0	// Deprecated
constexpr U32 BUG_REPORT_REQUEST 		= 0x01 << 0;
#endif
constexpr U32 COMPLAINT_REPORT_REQUEST 	= 0x01 << 1;
constexpr U32 OBJECT_PAY_REQUEST		= 0x01 << 2;

// ************************************************************
// THESE ENUMS ARE IN THE DATABASE!!!
//
// The process for adding a new report type is to:
// 1. Issue a command to the database to insert the new value:
//    insert into user_report_type (description)
//                values ('${new type name}');
// 2. Record the integer value assigned:
//    select type from user_report_type
//           where description='${new type name}';
// 3. Add it here.
//     ${NEW TYPE NAME}_REPORT = ${type_number};
//
// Failure to follow this process WILL result in incorrect
// queries on user reports.
// ************************************************************
enum EReportType
{
	NULL_REPORT = 0,		// don't use this value anywhere
	UNKNOWN_REPORT = 1,
#if 0
	BUG_REPORT = 2,			// Deprecated
#endif
	COMPLAINT_REPORT = 3,
	CS_REQUEST_REPORT = 4
};

class LLFloaterReporter final : public LLFloater,
								public LLFloaterSingleton<LLFloaterReporter>
{
	friend class LLUISingleton<LLFloaterReporter,
							   VisibilityPolicy<LLFloater> >;

protected:
	LOG_CLASS(LLFloaterReporter);

public:
	~LLFloaterReporter() override;

	bool postBuild() override;

	// Used by LLSelectMgr to pass the selected object properties
	void setPickedObjectProperties(const std::string& object_name,
								   const std::string& owner_name,
								   const LLUUID& owner_id);

	static bool showFromMenu();
	static void showFromAvatar(const LLUUID& avatar_id,
							   const std::string& desc = LLStringUtil::null,
							   S32 abuse_category = -1);
	static void showFromObject(const LLUUID& object_id,
							   const LLUUID& experience_id = LLUUID::null);
	static void showFromExperience(const LLUUID& experience_id);

private:
	// Open only via the show*() methods above
	LLFloaterReporter(const LLSD&);

	void takeScreenshot();
	void uploadImage();
	bool validateReport();
	void setReporterID();
	LLSD gatherReport();

	void sendReportViaLegacy(const LLSD& report);

	void sendReportViaCaps(const std::string& url, const std::string& sshot_url,
						   const LLSD& report);
	static void finishedARPost(const LLSD&);

	void setPosBox(const LLVector3d& pos);
	void getObjectInfo(const LLUUID& object_id);
	void getExperienceInfo(const LLUUID& experience_id);

	void setFromAvatarID(const LLUUID& avatar_id);

	static LLFloaterReporter* createNewReporter();

	static void closePickTool(void* userdata);

	static void onClickSend(void* userdata);
	static void onClickCancel(void* userdata);
	static void onClickObjPicker(void* userdata);
	static void onClickSelectAbuser(void* userdata);

	static void uploadDoneCallback(const LLUUID& uuid, void* userdata,
								   S32 result, LLExtStat ext_status);

	static void onAvatarNameCache(const LLUUID& avatar_id,
								  const LLAvatarName& av_name);

	static void callbackAvatarID(const std::vector<std::string>& names,
								 const std::vector<LLUUID>& ids,
								 void* userdata);

	static void requestAbuseCategoriesCoro(const std::string& url);

private:
	LLPointer<LLImageRaw>			mImageRaw;
	LLComboBox*						mCategoryCombo;
	LLResourceData*					mResourceDatap;
	LLUUID 							mObjectID;
	LLUUID							mScreenID;
	LLUUID							mAbuserID;
	LLUUID							mExperienceID;
	LLVector3						mPosition;
	std::string						mDefaultSummary;
	// Store the real name, not the link, for upstream reporting
	std::string						mOwnerName;
	bool 							mPicking;
	bool							mCopyrightWarningSeen;
};

#endif
