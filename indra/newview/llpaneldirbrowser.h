/**
 * @file llpaneldirbrowser.h
 * @brief LLPanelDirBrowser class definition
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

// Base class for the various search panels/results browsers in the Search
// floater. For example, Search > Places is derived from this.

#ifndef LL_LLPANELDIRBROWSER_H
#define LL_LLPANELDIRBROWSER_H

#include "llframetimer.h"
#include "llpanel.h"

class LLButton;
class LLCheckBoxCtrl;
class HBFloaterSearch;
class LLMessageSystem;
class LLScrollListCtrl;

class LLPanelDirBrowser: public LLPanel
{
protected:
	LOG_CLASS(LLPanelDirBrowser);

public:
	LLPanelDirBrowser(const std::string& name, HBFloaterSearch* floater);
	virtual ~LLPanelDirBrowser();

	virtual bool postBuild();

	// Use to get periodic updates.
	virtual void draw();

	virtual void onVisibilityChange(bool curVisibilityIn);

	// Redo your search for the prev/next page of results
	virtual void prevPage();
	virtual void nextPage();
	void resetSearchStart();
	// Do the current query (used by prevPage/nextPage)
	virtual void performQuery()					{}

	LL_INLINE const LLUUID& getSearchID() const	{ return mSearchID; }

	// Select the line in the scroll list control with this ID, either now or
	// when data arrives from the server.
	void selectByUUID(const LLUUID& id);

	void selectEventByID(S32 event_id);

	U32 getSelectedEventID() const;
	void getSelectedInfo(LLUUID* id, S32* type);

	// 'type' is EVENT_CODE, PLACE_CODE, etc. from below. 'item_id' is integer
	// for events, UUID for all others.
	void showDetailPanel(S32 type, LLSD item_id);

	// From llpaneldirbase
	void setupNewSearch();

	// Default handler for clicking the search button resets the next/previous
	// state and performs the query. Expects a pointer to an LLPanelDirBrowser
	// object.
	static void onClickSearchCore(void* userdata);

	// 'query_start' indicates the first result row to return, usually 0 or 100
	// or 200 because the searches return a max of 100 rows
	static void sendDirFindQuery(LLMessageSystem* msg, const LLUUID& query_id,
								 const std::string& text, U32 flags,
								 S32 query_start);

	void newClassified();

	// Prev/Next page buttons
	static void onClickNext(void* data);
	static void onClickPrev(void* data);

	static void onCommitList(LLUICtrl* ctrl, void* data);

	static void processDirPeopleReply(LLMessageSystem* msg, void**);
	static void processDirPlacesReply(LLMessageSystem* msg, void**);
	static void processDirEventsReply(LLMessageSystem* msg, void**);
	static void processDirGroupsReply(LLMessageSystem* msg, void**);
	static void processDirClassifiedReply(LLMessageSystem* msg, void**);
	static void processDirLandReply(LLMessageSystem* msg, void**);

	std::string filterShortWords(const std::string source_str,
								 S32 shortest_word_length, bool& was_filtered);

	// Logic to control maturity checkboxes in Classified/Events/Places/
	// "Land for Sale" tabs.
	void updateMaturityCheckbox(bool force = false);

protected:
	void updateResultCount();

	void addClassified(LLScrollListCtrl* listp, const LLUUID& classified_id,
					   const std::string& name, U32 creation_date,
					   S32 price_for_listing);

	LLSD createLandSale(const LLUUID& parcel_id, bool is_auction,
						bool is_for_sale, const std::string& name, S32* type);

	static void onSearchEdit(const std::string& text, void* data);

	// If this is a search for a panel like "people_panel" (and not the "all"
	// panel) optionally show the "Next" button.  Return the actual number of
	// rows to display.
	S32 showNextButton(S32 rows);

protected:
	HBFloaterSearch*	mFloaterSearch;

	LLCheckBoxCtrl*		mIncAdultCheck;
	LLCheckBoxCtrl*		mIncMatureCheck;
	LLCheckBoxCtrl*		mIncPGCheck;

	LLButton*			mPrevButton;
	LLButton*			mNextButton;

	LLScrollListCtrl* 	mResultsList;

	LLUUID				mSearchID;		// Unique ID for a pending search
	LLUUID				mWantSelectID;	// scroll item to select on arrival
	std::string     	mCurrentSortColumn;

	// Some searches return a max of 100 items per page, so we can start the
	// search from the 100th item rather than the 0th, etc.
	S32					mSearchStart;

	// Places is 100 per page, events is 200 per page
	S32					mResultsPerPage;
	S32					mResultsReceived;

	U32					mMinSearchChars;

	LLFrameTimer		mLastResultTimer;

	bool            	mCurrentSortAscending;
	bool				mHaveSearchResults;
	bool				mDidAutoSelect;
	bool				mLastWantPGOnly;
	bool				mLastCanAccessMature;
	bool				mLastCanAccessAdult;

	LLSD				mResultsContents;

	std::string			mControlNameAdult;
	std::string			mControlNameMature;
	std::string			mControlNamePG;

	typedef fast_hmap<LLUUID, LLPanelDirBrowser*> instances_map_t;
	static instances_map_t sInstances;
};

// Codes used for sorting by type.
constexpr S32 INVALID_CODE = -1;
constexpr S32 EVENT_CODE = 0;
constexpr S32 PLACE_CODE = 1;
#if 0	// We no longer show online vs. offline in search result icons.
constexpr S32 ONLINE_CODE = 2;
constexpr S32 OFFLINE_CODE = 3;
#endif
constexpr S32 AVATAR_CODE = 3;
constexpr S32 GROUP_CODE = 4;
constexpr S32 CLASSIFIED_CODE = 5;
constexpr S32 FOR_SALE_CODE = 6;	// For sale place
constexpr S32 AUCTION_CODE = 7;		// For auction place
constexpr S32 POPULAR_CODE = 8;		// Popular by dwell

// Mask values for search flags

// Should try not to send this to the search engine:
constexpr S32 SEARCH_NONE = 0;
constexpr S32 SEARCH_PG = 1;
constexpr S32 SEARCH_MATURE = 2;
constexpr S32 SEARCH_ADULT = 4;

#endif // LL_LLPANELDIRBROWSER_H
