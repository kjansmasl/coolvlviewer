/**
 * @file hbfloaterbump.cpp
 * @brief Floater listing bumps, pushes and hits, and allowing to take actions.
 * @author Henri Beauchamp
 *
 * $LicenseInfo:firstyear=2020&license=viewergpl$
 *
 * Copyright (c) 2020, Henri Beauchamp
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

#include "hbfloaterbump.h"

#include "llbutton.h"
#include "llcachename.h"
#include "llscrolllistctrl.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "llgridmanager.h"
#include "llfloateravatarinfo.h"
#include "llfloaterreporter.h"
//MK
#include "mkrlinterface.h"
//mk

#define COMMENT_PREFIX "\342\200\243 "

///////////////////////////////////////////////////////////////////////////////
// LLMeanCollisionData class (used to be in llmeancollisiondata.h, but now
// only used privately by HBFloaterBump). I also modified and extended it with
// new statistics (number of events, first time, max magnitude, automatic
// time_t to time stamp string (with seconds and date) conversion). HB
///////////////////////////////////////////////////////////////////////////////

class LLMeanCollisionData
{
public:
	LL_INLINE LLMeanCollisionData(const LLUUID& perpetrator_id, time_t time,
								  EMeanCollisionType type, F32 mag)
	:	mPerpetratorId(perpetrator_id),
		mFirstTime(time),
		mType(type),
		mMag(mag),
		mMaxMag(mag),
		mNumber(1)
	{
		setTime(time);
		mFirstTimeStr = mLastTimeStr;
	}

	void setTime(time_t time)
	{
		mLastTime = time;
		mLastTimeStr = LLGridManager::getTimeStamp(time, "%Y-%m-%d %H:%M:%S");
	}

public:
	LLUUID				mPerpetratorId;
	std::string			mFullName;
	std::string			mFirstTimeStr;
	std::string			mLastTimeStr;
	time_t				mFirstTime;
	time_t				mLastTime;
	EMeanCollisionType	mType;
	F32					mMag;
	F32					mMaxMag;
	U32					mNumber;
};

///////////////////////////////////////////////////////////////////////////////
// HBFloaterBump class proper
///////////////////////////////////////////////////////////////////////////////

HBFloaterBump::collisions_list_t HBFloaterBump::sMeanCollisionsList;
bool HBFloaterBump::sListUpdated = false;

HBFloaterBump::HBFloaterBump(const LLSD&)
{
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_bumps.xml");
}

//virtual
bool HBFloaterBump::postBuild()
{
	mBumpsList = getChild<LLScrollListCtrl>("bump_list");

	childSetAction("close_btn", onButtonClose, this);

	mClearButton = getChild<LLButton>("clear_btn");
	mClearButton->setClickedCallback(onButtonClear, this);

	mFocusButton = getChild<LLButton>("focus_btn");
	mFocusButton->setClickedCallback(onButtonFocus, this);

	mProfileButton = getChild<LLButton>("profile_btn");
	mProfileButton->setClickedCallback(onButtonProfile, this);

	mReportButton = getChild<LLButton>("report_btn");
	mReportButton->setClickedCallback(onButtonReport, this);

	sListUpdated = true;	// Force a list refresh on first draw()

	return true;
}

//virtual
void HBFloaterBump::refresh()
{
	sListUpdated = false;

	mBumpsList->deleteAllItems();

	if (sMeanCollisionsList.empty())
	{
		static const std::string none = COMMENT_PREFIX +
										getString("none_detected");
		mBumpsList->addCommentText(none);
		return;
	}

	static const std::string bump_str = getString("bump");
	static const std::string llpushobject_str = getString("llpushobject");
	static const std::string selected_obj_str =
		getString("selected_object_collide");
	static const std::string scripted_obj_str =
		getString("scripted_object_collide");
	static const std::string physical_obj_str =
		getString("physical_object_collide");

	const std::string* type;
	for (collisions_list_t::iterator iter = sMeanCollisionsList.begin(),
									 end = sMeanCollisionsList.end();
		 iter != end; ++iter)
	{
		const LLMeanCollisionData& mcd = *iter;

		switch (mcd.mType)
		{
			case MEAN_BUMP:
				type = &bump_str;
				break;

			case MEAN_LLPUSHOBJECT:
				type = &llpushobject_str;
				break;

			case MEAN_SELECTED_OBJECT_COLLIDE:
				type = &selected_obj_str;
				break;

			case MEAN_SCRIPTED_OBJECT_COLLIDE:
				type = &scripted_obj_str;
				break;

			case MEAN_PHYSICAL_OBJECT_COLLIDE:
				type = &physical_obj_str;
				break;

			default:
				llwarns_once << "Unknown mean collision type: " << mcd.mType
							 << llendl;
				continue;
		}

		LLSD element;
		LLSD& columns = element["columns"];

		columns[0]["column"] = "time_stamp";
		columns[0]["font"] = "SANSSERIF_SMALL";
		columns[0]["value"] = mcd.mLastTimeStr;

		columns[1]["column"] = "name";
		columns[1]["font"] = "SANSSERIF_SMALL";
		columns[1]["value"] = mcd.mFullName;

		columns[2]["column"] = "magnitude";
		columns[2]["font"] = "SANSSERIF_SMALL";
		columns[2]["value"] = llformat("%d/%d", (S32)(mcd.mMag + 0.5f),
									   (S32)(mcd.mMaxMag + 0.5f));

		columns[3]["column"] = "type";
		columns[3]["font"] = "SANSSERIF_SMALL";
		columns[3]["value"] = *type;

		columns[4]["column"] = "number";
		columns[4]["font"] = "SANSSERIF_SMALL";
		columns[4]["value"] = llformat("%d", mcd.mNumber);

		// Hidden column. We do not use element["id"], because the same
		// perpetrator could use several types of aggressions...
		columns[5]["column"] = "perp_id";
		columns[5]["value"] = mcd.mPerpetratorId;

		LLScrollListItem* itemp = mBumpsList->addElement(element);
		if (itemp && mcd.mLastTimeStr != mcd.mFirstTimeStr)
		{
			static const std::string first_event =
				getString("first_such_event");
			itemp->setToolTip(first_event + " " + mcd.mFirstTimeStr);
		}
	}

	// Automatically clamped to last line
	mBumpsList->setScrollPos(S32_MAX);
}

//virtual
void HBFloaterBump::draw()
{
//MK
	if (gRLenabled &&
		(gRLInterface.mContainsShownames ||
		 gRLInterface.mContainsShownametags))
	{
		close();
		return;
	}
//mk
	if (sListUpdated)
	{
		refresh();
	}

	mClearButton->setEnabled(!sMeanCollisionsList.empty());

	bool enabled = mBumpsList->getNumSelected() > 0;
	mFocusButton->setEnabled(enabled);
	mProfileButton->setEnabled(enabled);
	mReportButton->setEnabled(enabled);

	LLFloater::draw();
}

//static
void HBFloaterBump::cleanup()
{
	sMeanCollisionsList.clear();
	sListUpdated = true;
}

//static
void HBFloaterBump::meanNameCallback(const LLUUID& id,
									 const std::string& fullname, bool)
{
	for (collisions_list_t::iterator iter = sMeanCollisionsList.begin(),
									 end = sMeanCollisionsList.end();
		 iter != end; ++iter)
	{
		LLMeanCollisionData& mcd = *iter;
		if (mcd.mPerpetratorId == id)
		{
			mcd.mFullName = fullname;
			sListUpdated = true;
		}
	}
}

//static
void HBFloaterBump::addMeanCollision(const LLUUID& id, U32 time,
									 EMeanCollisionType type, F32 mag)
{
	for (collisions_list_t::iterator iter = sMeanCollisionsList.begin(),
									 end = sMeanCollisionsList.end();
		 iter != end; ++iter)
	{
		LLMeanCollisionData& mcd = *iter;
		if (mcd.mPerpetratorId == id && mcd.mType == type)
		{
			mcd.setTime(time);
			if (mag > mcd.mMaxMag)
			{
				mcd.mMaxMag = mag;
			}
			mcd.mMag = mag;
			++mcd.mNumber;
			sListUpdated = true;
			return;
		}
	}

	sMeanCollisionsList.emplace_back(id, time, type, mag);
	if (gCacheNamep)
	{
		// Note: sListUpdated will be set on name resolution callback
		gCacheNamep->get(id, false, meanNameCallback);
	}
}

//static
std::string HBFloaterBump::getMeanCollisionsStats(const LLUUID& perpetrator_id)
{
	// Gather the statistics about the prepetrator's assault
	S32 total_hits = 0;
	F32 max_mag = 0.f;
	time_t first_time = time_max();
	time_t last_time = 0;
	std::string first, last;
	for (collisions_list_t::iterator iter = sMeanCollisionsList.begin(),
									 end = sMeanCollisionsList.end();
		 iter != end; ++iter)
	{
		const LLMeanCollisionData& mcd = *iter;
		if (mcd.mPerpetratorId == perpetrator_id)
		{
			if (mcd.mFirstTime < first_time)
			{
				first_time = mcd.mFirstTime;
				first = mcd.mFirstTimeStr;
			}
			if (mcd.mLastTime > last_time)
			{
				last_time = mcd.mLastTime;
				last = mcd.mLastTimeStr;
			}
			if (mcd.mMaxMag > max_mag)
			{
				max_mag = mcd.mMaxMag;
			}
			total_hits += mcd.mNumber;
		}
	}

	if (!total_hits)
	{
		return "";
	}

	// Remove the seconds from the time stamps
	size_t i = first.rfind(':');
	if (i != std::string::npos)
	{
		first.erase(i);
	}
	i = last.rfind(':');
	if (i != std::string::npos)
	{
		last.erase(i);
	}

	// Create a description of the assault from the statistics
	static const char* short_assault =
		"Total pushes: %d - Max magnitude: %d - Occured at %s SLT.";
	static const char* long_assault =
		"Total pushes: %d - Max magnitude: %d - Extended over %s to %s, SLT.";

	std::string desc;
	if (last_time - first_time > 60)
	{
		desc = llformat(long_assault, total_hits, (S32)(max_mag + 0.5f),
						first.c_str(), last.c_str());
	}
	else
	{
		desc = llformat(short_assault, total_hits, (S32)(max_mag + 0.5f),
						first.c_str());
	}
	return desc;
}

//static
void HBFloaterBump::onButtonClose(void* data)
{
	HBFloaterBump* self = (HBFloaterBump*)data;
	if (self)
	{
		self->close();
	}
}

//static
void HBFloaterBump::onButtonClear(void*)
{
	cleanup();
}

//static
void HBFloaterBump::onButtonFocus(void* userdata)
{
	HBFloaterBump* self = (HBFloaterBump*)userdata;
	if (self)
	{
	 	LLScrollListItem* itemp = self->mBumpsList->getFirstSelected();
		if (itemp)
		{
			LLUUID perpetrator_id = itemp->getColumn(5)->getValue().asUUID();
			gAgent.lookAtObject(perpetrator_id, CAMERA_POSITION_OBJECT);
		}
	}
}

//static
void HBFloaterBump::onButtonProfile(void* data)
{
	HBFloaterBump* self = (HBFloaterBump*)data;
	if (self)
	{
		LLScrollListItem* itemp = self->mBumpsList->getFirstSelected();
		if (itemp)
		{
			LLUUID perpetrator_id = itemp->getColumn(5)->getValue().asUUID();
			LLFloaterAvatarInfo::showFromDirectory(perpetrator_id);
		}
	}
}

//static
void HBFloaterBump::onButtonReport(void* data)
{
	HBFloaterBump* self = (HBFloaterBump*)data;
	if (!self)
	{
		return;
	}

	LLScrollListItem* itemp = self->mBumpsList->getFirstSelected();
	if (!itemp)
	{
		return;
	}
	
	LLUUID perpetrator_id = itemp->getColumn(5)->getValue().asUUID();
	std::string desc = getMeanCollisionsStats(perpetrator_id);
	if (!desc.empty())
	{
		// Spawn the abuse reporting floater. Note: 35 is the category value
		// for Assault__Safe_area, as defined in floater_report_abuse.xml
		LLFloaterReporter::showFromAvatar(perpetrator_id, desc, 35);
	}
}
