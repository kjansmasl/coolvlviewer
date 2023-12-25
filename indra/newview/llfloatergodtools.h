/**
 * @file llfloatergodtools.h
 * @brief The on-screen rectangle with tool options.
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 *
 * Copyright (c) 2002-2009, Linden Research, Inc.
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

#ifndef LL_LLFLOATERGODTOOLS_H
#define LL_LLFLOATERGODTOOLS_H

#include "llfloater.h"
#include "llframetimer.h"
#include "llhost.h"

class LLLineEditor;
class LLMessageSystem;
class LLPanelGridTools;
class LLPanelObjectTools;
class LLPanelRegionTools;
class LLPanelRequestTools;

class LLFloaterGodTools final : public LLFloater,
								public LLFloaterSingleton<LLFloaterGodTools>
{
	friend class LLUISingleton<LLFloaterGodTools,
							   VisibilityPolicy<LLFloater> >;
	friend class LLPanelGridTools;
	friend class LLPanelObjectTools;
	friend class LLPanelRegionTools;
	friend class LLPanelRequestTools;

public:
	bool postBuild() override;
	void onOpen() override;
	void draw() override;

	// Processes received region info when the floater exists.
	static void updateFromRegionInfo();

private:
	// Open only via LLFloaterSingleton interface, i.e. showInstance() or
	// toggleInstance().
	LLFloaterGodTools(const LLSD&);

	static void* createPanelGrid(void* userdata);
	static void* createPanelRegion(void* userdata);
	static void* createPanelObjects(void* userdata);
	static void* createPanelRequest(void* userdata);

	static void onTabChanged(void* userdata, bool from_click);

protected:
	// Send possibly changed values to simulator.
	void sendGodUpdateRegionInfo();

	// Get data to populate UI.
	void sendRegionInfoRequest();

	U64 computeRegionFlags() const;

	// When the floater is going away, reset any options that need to be
	// cleared.
	void resetToolState();

protected:
	LLPanelRegionTools*	mPanelRegionTools;
	LLPanelObjectTools*	mPanelObjectTools;

	LLHost				mCurrentHost;
	LLFrameTimer		mUpdateTimer;
};

//-----------------------------------------------------------------------------
// LLPanelRegionTools
//-----------------------------------------------------------------------------

class LLPanelRegionTools final : public LLPanel
{
public:
	LLPanelRegionTools(const std::string& name);

	bool postBuild() override;

	// Set internal checkboxes/spinners/combos
	const std::string getSimName() const;
	U32 getEstateID() const;
	U32 getParentEstateID() const;
	U64 getRegionFlags() const;
	U64 getRegionFlagsMask() const;
	F32 getBillableFactor() const;
	S32 getPricePerMeter() const;
	S32 getGridPosX() const;
	S32 getGridPosY() const;
	S32 getRedirectGridX() const;
	S32 getRedirectGridY() const;

	// set internal checkboxes/spinners/combos
	void setSimName(const std::string& name);
	void setEstateID(U32 id);
	void setParentEstateID(U32 id);
	void setCheckFlags(U64 flags);
	void setBillableFactor(F32 billable_factor);
	void setPricePerMeter(S32 price);
	void setGridPosX(S32 pos);
	void setGridPosY(S32 pos);
	void setRedirectGridX(S32 pos);
	void setRedirectGridY(S32 pos);

	U64 computeRegionFlags(U64 initial_flags) const;
	void clearAllWidgets();
	void enableAllWidgets();

	// Used as a menu callback in llviewermenu.cpp
	static void onSaveState(void* userdata);

private:
	// Gets from internal checkboxes/spinners/combos
	void updateCurrentRegion() const;

	static void onChangeAnything(LLUICtrl*, void* userdata);
	static void onChangePrelude(LLUICtrl* ctrl, void* userdata);
	static void onChangeSimName(LLLineEditor* caller, void* userdata);
	static void onApplyChanges(void* userdata);
	static void onBakeTerrain(void* userdata);
	static void onRevertTerrain(void* userdata);
	static void onSwapTerrain(void* userdata);
	static void onSelectRegion(void* userdata);
	static void onRefresh(void* userdata);
};

//-----------------------------------------------------------------------------
// LLPanelGridTools
//-----------------------------------------------------------------------------

class LLPanelGridTools final : public LLPanel
{
public:
	LLPanelGridTools(const std::string& name);

	bool postBuild() override;

private:
	static void onClickKickAll(void* userdata);
	static bool confirmKick(const LLSD& notification, const LLSD& response);
	static bool finishKick(const LLSD& notification, const LLSD& response);
	static void onDragSunPhase(LLUICtrl* ctrl, void* userdata);
	static void onClickFlushMapVisibilityCaches(void* userdata);
	static bool flushMapVisibilityCachesConfirm(const LLSD& notification,
												const LLSD& response);
};

//-----------------------------------------------------------------------------
// LLPanelObjectTools
//-----------------------------------------------------------------------------

class LLPanelObjectTools final : public LLPanel
{
public:
	LLPanelObjectTools(const std::string& name);

	bool postBuild() override;
	void refresh() override;

	void setTargetAvatar(const LLUUID& target_id);
	U64 computeRegionFlags(U64 initial_flags) const;
	void clearAllWidgets();
	void enableAllWidgets();
	void setCheckFlags(U64 flags);

private:
	static void onChangeAnything(LLUICtrl* ctrl, void* userdata);
	static void onApplyChanges(void* userdata);
	static void onClickSet(void* userdata);
	static void callbackAvatarID(const std::vector<std::string>& names,
								 const uuid_vec_t& ids, void* userdata);
	static void onClickDeletePublicOwnedBy(void* userdata);
	static void onClickDeleteAllScriptedOwnedBy(void* userdata);
	static void onClickDeleteAllOwnedBy(void* userdata);
	static bool callbackSimWideDeletes(const LLSD& notification,
									   const LLSD& response);
	static void onGetTopColliders(void* userdata);
	static void onGetTopScripts(void* userdata);
	static void onGetScriptDigest(void* userdata);
	static void onClickSetBySelection(void* userdata);

private:
	LLUUID mTargetAvatar;

	// For all delete dialogs, store flags here for message.
	U32 mSimWideDeletesFlags;
};

//-----------------------------------------------------------------------------
// LLPanelRequestTools
//-----------------------------------------------------------------------------

class LLPanelRequestTools final : public LLPanel
{
public:
	LLPanelRequestTools(const std::string& name);

	bool postBuild() override;
	void refresh() override;

	static void sendRequest(const std::string& request,
							const std::string& parameter,
							const LLHost& host);

private:
	static void onClickRequest(void* userdata);
	void sendRequest(const LLHost& host);
};

// Also used by llfloaterregioninfo.cpp
void send_sim_wide_deletes(const LLUUID& owner_id, U32 flags);

#endif
