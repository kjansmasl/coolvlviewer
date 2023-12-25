/**
 * @file llnamelistctrl.h
 * @brief A list of names, automatically refreshing from the name cache.
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

#ifndef LL_LLNAMELISTCTRL_H
#define LL_LLNAMELISTCTRL_H

#include "llcachename.h"
#include "hbfastmap.h"
#include "llscrolllistctrl.h"
#include "lluuid.h"

class LLNameListCtrl : public LLScrollListCtrl
{
public:
	LLNameListCtrl(const std::string& name, const LLRect& rect,
				   LLUICtrlCallback callback, void* userdata,
				   bool allow_multiple_selection, bool draw_border = true,
				   S32 name_column_index = 0,
				   const std::string& tooltip = LLStringUtil::null);
	~LLNameListCtrl() override;

	void draw() override;

	LLXMLNodePtr getXML(bool save_children = true) const override;
	static LLView* fromXML(LLXMLNodePtr node, LLView* parent,
						   LLUICtrlFactory* factory);

	// Add a user to the list by name. It will be added, the name requested
	// from the cache, and updated as necessary.
	bool addNameItem(const LLUUID& agent_id, EAddPosition pos = ADD_BOTTOM,
					 bool enabled = true,
					 const std::string& suffix = LLStringUtil::null);
	bool addNameItem(LLScrollListItem* item, EAddPosition pos = ADD_BOTTOM);

	LLScrollListItem* addElement(const LLSD& value,
								 EAddPosition pos = ADD_BOTTOM,
								 void* userdata = NULL) override;

	// Add a user to the list by name. It will be added, the name requested
	// from the cache, and updated as necessary.
	void addGroupNameItem(const LLUUID& group_id,
						  EAddPosition pos = ADD_BOTTOM, bool enabled = true);
	void addGroupNameItem(LLScrollListItem* item,
						  EAddPosition pos = ADD_BOTTOM);

	void removeNameItem(const LLUUID& agent_id);

	void refresh(const LLUUID& id, const std::string& fullname, bool is_group);

	static void refreshAll(const LLUUID& id, const std::string& fullname,
						   bool is_group);

	void sortByName(bool ascending);

	LLScrollListItem* getItemById(const LLUUID& id);

	bool handleDragAndDrop(S32 x, S32 y, MASK mask, bool drop,
						   EDragAndDropType cargo_type,
						   void* cargo_data, EAcceptance* accept,
						   std::string& tooltip_msg) override;

	LL_INLINE void setAllowCallingCardDrop(bool b)	{ mAllowCallingCardDrop = b; }

	LL_INLINE void setUseDisplayNames(bool b)		{ mUseDisplayNames = b; }

	LL_INLINE void setLazyUpdateInterval(F32 delay)	{ mLazyUpdateInterval = delay; }

private:
	bool getResidentName(const LLUUID& agent_id, std::string& fullname);
	bool getGroupName(const LLUUID& group_id, std::string& name);

	static void onAvatarNameCache(const LLUUID& agent_id,
								  const LLAvatarName& av_name,
								  LLNameListCtrl* self);

	static void onGroupNameCache(const LLUUID& group_id,
								 const std::string& name,
								 LLNameListCtrl* self);

private:
	S32						mNameColumnIndex;
	F32						mLazyUpdateInterval;
	F64						mLastUpdate;
	bool					mAllowCallingCardDrop;
	bool					mUseDisplayNames;

	typedef fast_hmap<LLUUID, std::string> pending_map_t;
	pending_map_t			mPendingUpdates;

	typedef fast_hset<LLNameListCtrl*> instances_list_t;
	static instances_list_t	sInstances;
};

#endif
