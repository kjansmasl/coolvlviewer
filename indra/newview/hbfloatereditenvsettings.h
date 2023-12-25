/**
 * @file hbfloatereditenvsettings.h
 * @brief Environment settings editor floater class definition
 *
 * $LicenseInfo:firstyear=2018&license=viewergpl$
 *
 * Copyright (c) 2019-2023 Henri Beauchamp
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

#ifndef LL_HBFLOATEREDITENVSETTINGS_H
#define LL_HBFLOATEREDITENVSETTINGS_H

#include "llfloater.h"

#include "llenvironment.h"

class LLButton;
class LLCheckBoxCtrl;
class LLFlyoutButton;
class LLLineEditor;
class LLPanelEnvSettings;
class LLSliderCtrl;
class LLViewerInventoryItem;

class HBFloaterEditEnvSettings final : public LLFloater
{
	friend class HBSettingsCopiedCallback;

protected:
	LOG_CLASS(HBFloaterEditEnvSettings);

	// Do not call directly: use the static, show() or create() methods instead
	HBFloaterEditEnvSettings(const LLUUID& inv_id, LLSettingsType::EType type);

public:
	~HBFloaterEditEnvSettings() override;

	void refresh() override;
	bool isDirty() const override;
	void setDirty(bool dirty = true);

	void setEditSettings(const LLSettingsBase::ptr_t& settings);

	LL_INLINE const LLUUID& getInventoryId() const		{ return mInventoryId; }
	LL_INLINE LLSettingsType::EType getType() const		{ return mSettingsType; }

	void setSettings(const LLSettingsBase::ptr_t& settings);

	void importFromFile(const std::string& filename);

	void loadInventoryItem(LLUUID inv_id,
						   const std::string& notify = LLStringUtil::null);

	// Can be used to load the default settings asset for the appropriate
	// settings type, when no inventory item has been loaded so far. Used when
	// the editor is opened for importing legacy Windlight settings or when
	// editing empty environments for regions or parcels.
	void loadDefaultSettings();

	void setEditContextInventory();
	void setEditContextParcel();
	void setEditContextRegion();
	void setDayLength(S32 seconds);

	typedef boost::signals2::signal<void(LLSettingsBase::ptr_t)> commit_signal_t;
	typedef commit_signal_t::slot_type commit_cb_t;
	typedef boost::signals2::connection connection_t;
	connection_t setCommitCB(commit_cb_t cb);

	// For editing inventory settings items
	static HBFloaterEditEnvSettings* show(LLUUID inv_id);
	// For importing legacy Windlight seetings from XML files
	static HBFloaterEditEnvSettings* create(LLSettingsType::EType type);
	// For closing any edit floater of destroyed inventory item.
	static void destroy(const LLUUID& inv_id);

protected:
	bool postBuild() override;
	void draw() override;
	void onClose(bool app_quitting) override;
	void onFocusReceived() override;

	void onAssetLoaded(const LLUUID& asset_id, LLSettingsBase::ptr_t settings,
					   S32 status);

	void updateEditEnvironment();

	void doApplyCreateNewInventory(const std::string& settings_name,
								   const LLSettingsBase::ptr_t& settings);
	void onInventoryCreated(const LLUUID& inv_id, bool copied = false);
	void onInventoryCreated(const LLUUID& inv_id, const LLSD& results);

	void doApplyUpdateInventory(const LLSettingsBase::ptr_t& settings);
	void onInventoryUpdated(const LLUUID& inv_id);

private:
	static void* createSettingsPanel(void* userdata);
	static void onButtonLoad(void* userdata);
	static void onButtonImport(void* userdata);
	static void onButtonCancel(void* userdata);
	static void onButtonSave(void* userdata);
	static void onButtonSaveAsNew(void* userdata);
	static void onButtonApply(LLUICtrl* ctrl, void* userdata);
	static void onNameChanged(LLUICtrl*, void* userdata);

private:
	LLButton*				mLoadBtn;
	LLButton*				mImportBtn;
	LLButton*				mCancelBtn;
	LLButton*				mSaveBtn;
	LLButton*				mSaveAsNewBtn;
	LLFlyoutButton*			mApplyBtn;
	LLLineEditor*			mNameEditor;
	LLPanelEnvSettings*		mEditPanel;

	LLViewerInventoryItem*	mInventoryItem;
	LLUUID					mInventoryId;
	LLSettingsType::EType	mSettingsType;

	// Used for the "save as new" feature
	U32						mSaveAsNewCounter;
	std::string				mOriginalName;

	commit_signal_t			mCommitSignal;

	bool					mCanSave;
	bool					mCanCopy;
	bool					mCanModify;
	bool					mCanTransfer;

	typedef fast_hmap<LLUUID, HBFloaterEditEnvSettings*> instances_map_t;
	static instances_map_t	sInstances;
};

class HBFloaterLocalEnv final : public LLFloater,
								public LLFloaterSingleton<HBFloaterLocalEnv>
{
	friend class LLUISingleton<HBFloaterLocalEnv,
							   VisibilityPolicy<LLFloater> >;

protected:
	LOG_CLASS(HBFloaterLocalEnv);

public:
	static void closeInstance();

private:
	// Open only via LLFloaterSingleton interface, i.e. showInstance() or
	// toggleInstance().
	HBFloaterLocalEnv(const LLSD&);

	bool postBuild() override;
	void refresh() override;
//MK
	void draw() override;
//mk
	void onClose(bool app_quitting) override;

	void captureCurrentEnvironment();

	static void* createSkySettingsPanel(void* userdata);
	static void* createWaterSettingsPanel(void* userdata);

	static void onCheckFixedTime(LLUICtrl*, void* userdata);
	static void onCommitFixedTime(LLUICtrl*, void* userdata);
	static void onButtonReset(void* userdata);
	static void onButtonClose(void* userdata);

private:
	LLButton*					mResetBtn;
	LLCheckBoxCtrl*				mFixedTimeCheck;
	LLSliderCtrl*				mFixedTimeSlider;

	LLPanelEnvSettings*			mEditSkyPanel;
	LLPanelEnvSettings*			mEditWaterPanel;

	LLSettingsSky::ptr_t		mLiveSky;
	LLSettingsWater::ptr_t		mLiveWater;

	LLEnvironment::connection_t	mEventConnection;
};

#endif // LL_HBFLOATEREDITENVSETTINGS_H
