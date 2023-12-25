/**
 * @file llfloaterregioninfo.h
 * @author Aaron Brashears
 * @brief Declaration of the region info and controls floater and panels.
 *
 * $LicenseInfo:firstyear=2004&license=viewergpl$
 *
 * Copyright (c) 2004-2009, Linden Research, Inc.
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

#ifndef LL_LLFLOATERREGIONINFO_H
#define LL_LLFLOATERREGIONINFO_H

#include "llextendedstatus.h"
#include "hbfileselector.h"
#include "llfloater.h"
#include "llhost.h"
#include "llpanel.h"
#include "llregionflags.h"

class LLButton;
class LLDispatcher;
class LLInventoryItem;
class LLMessageSystem;
class LLNameListCtrl;
class LLPanelExperienceListEditor;
class HBPanelLandEnvironment;
class LLTabContainer;
class LLViewerRegion;
class LLViewerTextEditor;
class LLTextBox;

/////////////////////////////////////////////////////////////////////////////

// Purely static class (a singleton in LL's viewer, but LL coders love to
// complicate and slow down things for no reason)... It is used to store data
// for the last estate info request and its member variables are filled up by
// LLDispatchEstateUpdateInfo::operator(). HB
class LLEstateInfoModel
{
	LLEstateInfoModel() = delete;
	~LLEstateInfoModel() = delete;

public:

	LL_INLINE static bool getUseFixedSun()
	{
		return (sEstateFlags & REGION_FLAGS_SUN_FIXED) != 0;
	}

	LL_INLINE static bool getAllowEnvironmentOverride()
	{
		return (sEstateFlags & REGION_FLAGS_ALLOW_ENVIRONMENT_OVERRIDE) != 0;
	}

	LL_INLINE static void setAllowEnvironmentOverride(bool b)
	{
		setFlag(REGION_FLAGS_ALLOW_ENVIRONMENT_OVERRIDE, b);
	}

	LL_INLINE static bool getDenyScriptedAgents()
	{
		return (sEstateFlags & REGION_FLAGS_DENY_BOTS) != 0;
	}

	LL_INLINE static void setDenyScriptedAgents(bool b)
	{
		setFlag(REGION_FLAGS_DENY_BOTS, b);
	}

	LL_INLINE static void setFlag(U64 flag, bool b)
	{
		if (b)
		{
			sEstateFlags |= flag;
		}
		else
		{
			sEstateFlags &= ~flag;
		}
	}

public:
	static U64			sEstateFlags;
	static U32			sEstateId;
	static F32			sSunHour;
	static std::string	sEstateName;
	static LLUUID		sOwnerId;
};

/////////////////////////////////////////////////////////////////////////////
// Base class for all region information panels.
/////////////////////////////////////////////////////////////////////////////

class LLPanelRegionInfo : public LLPanel
{
protected:
	LOG_CLASS(LLPanelRegionInfo);

public:
	LLPanelRegionInfo();

	virtual bool refreshFromRegion(LLViewerRegion* region);
	virtual bool estateUpdate(LLMessageSystem* msg)		{ return true; }

	bool postBuild() override;

	void enableApplyBtn(bool enable = true);
	void disableApplyBtn();

protected:
	void initCtrl(const char* name);
	void initHelpBtn(const char*, const std::string& xml_alert);

	static void onBtnSet(void* user_data);
	static void onChangeAnything(LLUICtrl* ctrl, void* user_data);

	// Callback for all help buttons, data is name of XML alert to show.
	static void onClickHelp(void* data);

	// Returns true if update sent and apply button should be disabled.
	virtual bool sendUpdate()							{ return true; }

	typedef std::vector<std::string> strings_t;
	void sendEstateOwnerMessage(const std::string& request,
								const strings_t& strings);

protected:
	LLButton*	mApplyBtn;
	LLHost		mHost;
};

/////////////////////////////////////////////////////////////////////////////
// Actual panels start here
/////////////////////////////////////////////////////////////////////////////

class LLPanelRegionGeneralInfo final : public LLPanelRegionInfo
{
protected:
	LOG_CLASS(LLPanelRegionGeneralInfo);

public:
	bool refreshFromRegion(LLViewerRegion* region) override;

	bool postBuild() override;

private:
	bool sendUpdate() override;

	static void onClickKick(void* userdata);
	static void onKickCommit(const std::vector<std::string>& names,
							 const std::vector<LLUUID>& ids, void* userdata);
	static void onClickKickAll(void* userdata);
	bool onKickAllCommit(const LLSD& notification, const LLSD& response);
	static void onClickMessage(void* userdata);
	bool onMessageCommit(const LLSD& notification, const LLSD& response);
	static void onClickManageTelehub(void* data);
};

class LLPanelRegionDebugInfo final : public LLPanelRegionInfo
{
protected:
	LOG_CLASS(LLPanelRegionDebugInfo);

public:
	bool postBuild() override;

	bool refreshFromRegion(LLViewerRegion* region) override;

private:
	bool sendUpdate() override;

	static void onClickChooseAvatar(void*);
	static void callbackAvatarID(const std::vector<std::string>& names,
								 const std::vector<LLUUID>& ids, void* data);
	static void onClickReturn(void *);
	bool callbackReturn(const LLSD& notification, const LLSD& response);
	static void onClickTopColliders(void*);
	static void onClickTopScripts(void*);
	static void onClickRestart(void* data);
	bool callbackRestart(const LLSD& notification, const LLSD& response);
	static void onClickCancelRestart(void* data);

private:
	LLUUID mTargetAvatar;
};

class LLPanelRegionTextureInfo final : public LLPanelRegionInfo
{
protected:
	LOG_CLASS(LLPanelRegionTextureInfo);

public:
	LLPanelRegionTextureInfo();

	bool refreshFromRegion(LLViewerRegion* region) override;

	bool postBuild() override;

private:
	bool sendUpdate() override;

	bool validateTextureSizes();
};

class LLPanelRegionTerrainInfo final : public LLPanelRegionInfo
{
protected:
	LOG_CLASS(LLPanelRegionTerrainInfo);

public:
	bool postBuild() override;

	bool refreshFromRegion(LLViewerRegion* region) override;

private:
	bool sendUpdate() override;

	bool callbackBakeTerrain(const LLSD& notification, const LLSD& response);

	static void onChangeUseEstateTime(LLUICtrl* ctrl, void* user_data);
	static void onChangeFixedSun(LLUICtrl* ctrl, void* user_data);
	static void onChangeSunHour(LLUICtrl* ctrl, void*);

	static void downloadRawCallback(HBFileSelector::ESaveFilter filter,
									std::string& filepath, void* data);

	static void uploadRawCallback(HBFileSelector::ELoadFilter filter,
								  std::string& filepath, void* data);

	static void onClickDownloadRaw(void*);
	static void onClickUploadRaw(void*);
	static void onClickBakeTerrain(void*);
};

class LLPanelEstateInfo final : public LLPanelRegionInfo
{
	friend class HBPanelLandEnvironment;

protected:
	LOG_CLASS(LLPanelEstateInfo);

public:
	static void initDispatch(LLDispatcher& dispatch);

	bool kickUserConfirm(const LLSD& notification, const LLSD& response);

	bool onMessageCommit(const LLSD& notification, const LLSD& response);

	LLPanelEstateInfo();

	void updateControls(LLViewerRegion* region);

	bool refreshFromRegion(LLViewerRegion* region) override;
	bool estateUpdate(LLMessageSystem* msg) override;

	bool postBuild() override;
	void refresh() override;

	U32 computeEstateFlags();
	void setEstateFlags(U32 flags);

	bool getGlobalTime();
	void setGlobalTime(bool b);

	bool getFixedSun();

	F32 getSunHour();
	void setSunHour(F32 sun_hour);

	const std::string getEstateName() const;
	void setEstateName(const std::string& name);

	LL_INLINE U32 getEstateID() const					{ return mEstateID; }
	LL_INLINE void setEstateID(U32 estate_id)			{ mEstateID = estate_id; }

	static bool isLindenEstate();

	const std::string getOwnerName() const;
	void setOwnerName(const std::string& name);

	static void updateEstateOwnerName(const std::string& name);
	static void updateEstateName(const std::string& name);

	// This must have the same function signature as llmessage/llcachename.h
	// LLCacheNameCallback
	static void callbackCacheName(const LLUUID& id,
								  const std::string& full_name, bool is_group);

protected:
	bool sendUpdate() override;

private:
	// Confirmation dialog callback
	bool callbackChangeLindenEstate(const LLSD& notification,
									const LLSD& response);

	void commitEstateInfoDataserver();
	bool commitEstateInfoCaps();
	void commitEstateInfoCapsCoro(const std::string& url);
	void commitEstateAccess();
	void commitEstateManagers();

	bool checkSunHourSlider(LLUICtrl* child_ctrl);

	static void updateChild(LLUICtrl* ctrl, void* user_data);

	static void onKickUserCommit(const std::vector<std::string>& names,
								 const std::vector<LLUUID>& ids,
								 void* userdata);
	static void onClickMessageEstate(void* data);
	static void onChangeFixedSun(LLUICtrl* ctrl, void* user_data);
	static void onChangeUseGlobalTime(LLUICtrl* ctrl, void* user_data);
	static void onClickKickUser(void* userdata);

private:
	U32 mEstateID;
};

class LLPanelEstateAccess final : public LLPanelRegionInfo
{
protected:
	LOG_CLASS(LLPanelEstateAccess);

public:
	LLPanelEstateAccess();

	bool postBuild() override;

	bool refreshFromRegion(LLViewerRegion* region) override;

	void updateControls(LLViewerRegion* region);
	void updateLists();

	LL_INLINE void setPendingUpdate(bool pending)		{ mPendingUpdate = pending; }
	LL_INLINE bool getPendingUpdate()					{ return mPendingUpdate; }

	// If visible from mainland, allowed agent and allowed groups are ignored,
	// so must disable UI.
	void setAccessAllowedEnabled(bool enable_agent, bool enable_group,
								 bool enable_ban);

	static std::string allEstatesText();

private:
	bool checkRemovalButton(std::string name);

	static void onTabChanged(void* user_data, bool);

	static void updateChild(LLUICtrl* ctrl, void* user_data);

	static void onClickAddAllowedAgent(void* user_data);
	static void onClickRemoveAllowedAgent(void* user_data);
	static void onClickAddAllowedGroup(void* user_data);
	static void onClickRemoveAllowedGroup(void* user_data);
	static void onClickAddBannedAgent(void* user_data);
	static void onClickRemoveBannedAgent(void* user_data);
	static void onClickAddEstateManager(void* user_data);
	static void onClickRemoveEstateManager(void* user_data);
#if 0	// *TODO: implement (backport from LL's viewer-release)
	static void onClickCopyAllowedList(void* user_data);
	static void onClickCopyAllowedGroupList(void* user_data);
 	static void onClickCopyBannedList(void* user_data);
	static void onAllowedSearchEdit(const std::string& search_string,
									void* user_data);
	static void onAllowedGroupsSearchEdit(const std::string& search_string,
										  void* user_data);
	static void onBannedSearchEdit(const std::string& search_string,
								   void* user_data);
	void searchAgent(LLNameListCtrl* list, const std::string& search_string);
	void copyListToClipboard(std::string list_name);
#endif

	// Core methods for all above add/remove button clicks
	static void accessAddCore(U32 operation_flag);
	static bool accessAddCore2(const LLSD& notification, const LLSD& response);
	static void accessAddCore3(const std::vector<std::string>& names,
							   const std::vector<LLUUID>& ids, void* data);

	static void accessRemoveCore(U32 operation_flag);
	static bool accessRemoveCore2(const LLSD& notification,
								  const LLSD& response);

	static void addAllowedGroup2(LLUUID id, void* data);

	// Used for both add and remove operations
	static bool accessCoreConfirm(const LLSD& notification,
								  const LLSD& response);
	// Send the actual EstateOwnerRequest "estateaccessdelta" message
	static void sendEstateAccessDelta(U32 flags, const LLUUID& agent_id);

	// Group picker callback is different: cannot use core methods above
	bool addAllowedGroup(const LLSD& notification, const LLSD& response);
	void addAllowedGroup2(LLUUID id);

	static void requestEstateGetAccessCoro(const std::string& url);

public:
	LLNameListCtrl*	mEstateManagers;
	LLNameListCtrl*	mAllowedGroups;
	LLNameListCtrl*	mAllowedAvatars;
	LLNameListCtrl*	mBannedAvatars;

private:
	LLTabContainer*	mTabContainer;
	bool			mPendingUpdate;
	bool			mCtrlsEnabled;

	static S32		sLastActiveTab;
};

class LLPanelEstateCovenant final : public LLPanelRegionInfo
{
protected:
	LOG_CLASS(LLPanelEstateCovenant);

	bool sendUpdate() override;

public:
	LLPanelEstateCovenant();

	bool postBuild() override;

	bool refreshFromRegion(LLViewerRegion* region) override;
	bool estateUpdate(LLMessageSystem* msg) override;

	// LLView overrides
	bool handleDragAndDrop(S32 x, S32 y, MASK mask, bool drop,
						   EDragAndDropType cargo_type,
						   void* cargo_data, EAcceptance* accept,
						   std::string& tooltip_msg) override;
	void sendChangeCovenantID(const LLUUID& asset_id);
	void loadInvItem(LLInventoryItem* itemp);

	// Accessor functions
	static void updateCovenantText(const std::string& string, const LLUUID& asset_id);
	static void updateEstateName(const std::string& name);
	static void updateLastModified(const std::string& text);
	static void updateEstateOwnerName(const std::string& name);

	LL_INLINE const LLUUID& getCovenantID() const		{ return mCovenantID; }
	LL_INLINE void setCovenantID(const LLUUID& id)		{ mCovenantID = id; }

	const std::string& getEstateName() const;
	void setEstateName(const std::string& name);
	const std::string& getOwnerName() const;
	void setOwnerName(const std::string& name);
	void setCovenantTextEditor(const std::string& text);

	typedef enum e_asset_status
	{
		ASSET_ERROR,
		ASSET_UNLOADED,
		ASSET_LOADING,
		ASSET_LOADED
	} EAssetStatus;

private:
	static void onLoadComplete(const LLUUID& asset_uuid,
							   LLAssetType::EType type, void* user_data,
							   S32 status, LLExtStat);
	static bool confirmChangeCovenantCallback(const LLSD& notification,
											  const LLSD& response);
	static void resetCovenantID(void* userdata);
	static bool confirmResetCovenantCallback(const LLSD& notification,
											 const LLSD& response);

private:
	LLTextBox*				mEstateNameText;
	LLTextBox*				mEstateOwnerText;
	LLTextBox*				mLastModifiedText;
	// CovenantID from sim
	LLUUID					mCovenantID;
	LLViewerTextEditor*		mEditor;
	EAssetStatus			mAssetStatus;
};

class LLPanelRegionExperiences final : public LLPanelRegionInfo
{
protected:
	LOG_CLASS(LLPanelRegionExperiences);

public:
	LLPanelRegionExperiences();

	bool postBuild() override;

	bool sendUpdate() override;

	static bool experienceCoreConfirm(const LLSD& notification,
									  const LLSD& response);

	static void sendEstateExperienceDelta(U32 flags,
										  const LLUUID& agent_id);

	static void infoCallback(LLHandle<LLPanelRegionExperiences> handle,
							 const LLSD& content);

	bool refreshFromRegion(LLViewerRegion* region) override;
	void sendPurchaseRequest()const;
	void processResponse(const LLSD& content);

private:
	static void* createAllowedExperiencesPanel(void* data);
	static void* createTrustedExperiencesPanel(void* data);
	static void* createBlockedExperiencesPanel(void* data);

	static LLSD addIds(LLPanelExperienceListEditor* panel);

	void refreshRegionExperiences();
	static const std::string& regionCapabilityQuery(LLViewerRegion* region,
													const char* cap);

	void setupList(LLPanelExperienceListEditor* panel,
				   const std::string& control_name,
				   U32 add_id, U32 remove_id);

	void itemChanged(U32 event_type, const LLUUID& id);

private:
	LLPanelExperienceListEditor*	mTrusted;
	LLPanelExperienceListEditor*	mAllowed;
	LLPanelExperienceListEditor*	mBlocked;
	LLUUID							mDefaultExperience;
};

/////////////////////////////////////////////////////////////////////////////
// Floater class proper
/////////////////////////////////////////////////////////////////////////////

class LLFloaterRegionInfo final
:	public LLFloater,
	public LLFloaterSingleton<LLFloaterRegionInfo>
{
	friend class LLUISingleton<LLFloaterRegionInfo, VisibilityPolicy<LLFloater> >;

protected:
	LOG_CLASS(LLFloaterRegionInfo);

public:
	~LLFloaterRegionInfo() override;

	void onOpen() override;
	bool postBuild() override;

	static void processEstateOwnerRequest(LLMessageSystem* msg, void**);

	// Processes received region info when the floater exists.
	static void updateFromRegionInfo();

	LL_INLINE static const LLUUID& getLastInvoice()		{ return sRequestInvoice; }
	LL_INLINE static void nextInvoice()					{ sRequestInvoice.generate(); }

	static LLPanelRegionGeneralInfo* getPanelGeneral();
	static LLPanelRegionDebugInfo* getPanelDebug();
	static LLPanelEstateInfo* getPanelEstate();
	static LLPanelEstateAccess* getPanelAccess();
	static LLPanelEstateCovenant* getPanelCovenant();
	static LLPanelRegionTerrainInfo* getPanelTerrain();
	static LLPanelRegionExperiences* getPanelExperiences();
	static HBPanelLandEnvironment* getPanelEnvironment();

	void refresh() override;

	static void requestRegionInfo();

protected:
	LLFloaterRegionInfo(const LLSD& seed);
	void refreshFromRegion(LLViewerRegion* region);

private:
	LLTabContainer*				mTabs;
	HBPanelLandEnvironment*		mPanelEnvironment;

	typedef std::vector<LLPanelRegionInfo*> info_panels_t;
	info_panels_t				mInfoPanels;

	static LLUUID				sRequestInvoice;
	static S32					sLastTab;
};

#endif
