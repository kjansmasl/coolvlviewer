/**
 * @file llpanelclassified.h
 * @brief LLPanelClassified class definition
 *
 * $LicenseInfo:firstyear=2005&license=viewergpl$
 *
 * Copyright (c) 2005-2009, Linden Research, Inc.
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

// Display of a classified used both for the global view in the
// Find directory, and also for each individual user's classified in their
// profile.

#ifndef LL_LLPANELCLASSIFIED_H
#define LL_LLPANELCLASSIFIED_H

#include "hbfastset.h"
#include "llfloater.h"
#include "llvector3d.h"

#include "llavatarproperties.h"

class LLButton;
class LLCheckBoxCtrl;
class LLComboBox;
class LLLineEditor;
class LLTextBox;
class LLTextEditor;
class LLTextureCtrl;

// Purely static class
class LLClassifiedInfo
{
	LLClassifiedInfo() = delete;
	~LLClassifiedInfo() = delete;

public:
	static void loadCategories(const LLSD& options);

public:
	typedef std::map<U32, std::string> map_t;
	static map_t sCategories;
};

class LLPanelClassified final : public LLPanel, LLAvatarPropertiesObserver
{
protected:
	LOG_CLASS(LLPanelClassified);

public:
    LLPanelClassified(bool in_finder, bool from_search);
    ~LLPanelClassified() override;

	void reset();

    bool postBuild() override;

    void draw() override;

	void refresh() override;

	void apply();

	// If can close, return true. If cannot close, pop save/discard dialog
	// and return false.
	bool canClose();

	// LLAvatarPropertiesObserver override
	void processProperties(S32 type, void* data) override;

	// Setup a new classified, including creating an id, giving a sane
	// initial position, etc.
	void initNewClassified();

	LL_INLINE void setClassifiedID(const LLUUID& id)	{ mClassifiedID = id; }
	LL_INLINE const LLUUID& getClassifiedID() const		{ return mClassifiedID; }

	void setClickThroughText(const std::string& text);
	static void setClickThrough(const LLUUID& classified_id,
								S32 teleport, S32 map, S32 profile,
								bool from_new_table);

	// Checks that the title is valid (e.g. starts with a number or letter)
	bool titleIsValid();

	// Schedules the panel to request data from the server next time it is
	// drawn.
	void markForServerRequest();

	std::string getClassifiedName();

    void sendClassifiedInfoRequest();
	void sendClassifiedInfoUpdate();
	void resetDirty() override;

	// Confirmation dialogs flow in this order
	bool confirmMature(const LLSD& notification, const LLSD& response);
	void gotMature();
	static void callbackGotPriceForListing(S32 option, std::string text,
										   void* data);
	bool confirmPublish(const LLSD& notification, const LLSD& response);

	void sendClassifiedClickMessage(const std::string& type);

protected:
	bool saveCallback(const LLSD& notification, const LLSD& response);

	static void handleSearchStatResponse(LLUUID id, LLSD result);

	static void onClickUpdate(void* data);
    static void onClickTeleport(void* data);
    static void onClickMap(void* data);
	static void onClickProfile(void* data);
    static void onClickSet(void* data);

	static void focusReceived(LLFocusableElement* ctrl, void* data);
	static void onCommitAny(LLUICtrl* ctrl, void* data);

	// Default Mature and PG regions to proper classified access
	void setDefaultAccessCombo();
	
	bool checkDirty();		// Update and return mDirty

protected:
	LLUUID				mClassifiedID;
	LLUUID				mRequestedID;
	LLUUID				mCreatorID;
	LLUUID				mParcelID;
	S32					mPriceForListing;

	LLTextureCtrl*		mSnapshotCtrl;
	LLLineEditor*		mNameEditor;
	LLTextEditor*		mDescEditor;
	LLLineEditor*		mLocationEditor;
	LLComboBox*			mCategoryCombo;
	LLComboBox*			mMatureCombo;
	LLCheckBoxCtrl*		mAutoRenewCheck;

	LLButton*    		mUpdateBtn;
	LLButton*    		mTeleportBtn;
	LLButton*    		mMapBtn;
	LLButton*	 		mProfileBtn;

	LLTextBox*			mInfoText;
	LLButton*			mSetBtn;
	LLTextBox*			mClickThroughText;

	LLRect				mSnapshotSize;

	// Needed for stat tracking
	S32					mTeleportClicksOld;
	S32					mMapClicksOld;
	S32					mProfileClicksOld;
	S32					mTeleportClicksNew;
	S32					mMapClicksNew;
	S32					mProfileClicksNew;

	std::string			mSimName;
	LLVector3d			mPosGlobal;

	bool				mInFinder;
	bool				mFromSearch;	// from web-based "All" search sidebar
	bool				mDirty;
	bool				mForceClose;
	bool				mLocationChanged;

	// Data will be requested on first draw
	bool				mDataRequested;

	// For avatar panel classifieds only, has the user been charged yet for
	// this classified ?  That is, have they saved it once ?
	bool				mPaidFor;

	typedef fast_hset<LLPanelClassified*> panel_list_t;
	static panel_list_t	sInstances;
};

class LLFloaterPriceForListing final : public LLFloater
{
public:
	LLFloaterPriceForListing();

	bool postBuild() override;

	static void show(void (*callback)(S32, std::string, void*),
					 void* userdata);

private:
	static void onClickSetPrice(void*);
	static void onClickCancel(void*);
	static void buttonCore(S32 button, void* data);

private:
	void	(*mCallback)(S32 option, std::string value, void* userdata);
	void*	mUserData;
};

#endif // LL_LLPANELCLASSIFIED_H
