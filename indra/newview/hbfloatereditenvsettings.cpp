/**
 * @file hbfloatereditenvsettings.cpp
 * @brief Environment settings editor floater class implementation
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

#include "llviewerprecompiledheaders.h"

#include "hbfloatereditenvsettings.h"

#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "hbfileselector.h"
#include "lllineeditor.h"
#include "llnotifications.h"
#include "llsliderctrl.h"
#include "lltrans.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "llappviewer.h"				// For gFrameTimeSeconds
#include "llenvsettings.h"
#include "llinventorymodel.h"
#include "hbfloaterinvitemspicker.h"
#include "llpanelenvsettings.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llviewercontrol.h"
#include "llviewerinventory.h"
#include "llviewerparcelmgr.h"

// Helper class

class HBSettingsCopiedCallback final : public LLInventoryCallback
{
public:
	HBSettingsCopiedCallback(LLHandle<LLFloater> handle)
	:	mHandle(handle)
	{
	}

	void fire(const LLUUID& inv_item_id) override
	{
		if (!mHandle.isDead() && gInventory.getItem(inv_item_id))
		{
			HBFloaterEditEnvSettings* floaterp =
				(HBFloaterEditEnvSettings*)mHandle.get();
			floaterp->onInventoryCreated(inv_item_id, true);
		}
	}

private:
	LLHandle<LLFloater>	mHandle;
};

///////////////////////////////////////////////////////////////////////////////
// HBFloaterEditEnvSettings class proper
///////////////////////////////////////////////////////////////////////////////

//static
HBFloaterEditEnvSettings::instances_map_t HBFloaterEditEnvSettings::sInstances;

//static
HBFloaterEditEnvSettings* HBFloaterEditEnvSettings::show(LLUUID inv_id)
{
	if (inv_id.isNull())
	{
		llwarns << "Null item Id passed. Floater not created." << llendl;
		return NULL;
	}

	// Make sure we are not trying to edit a link and get the linked item Id
	// in that case.
	inv_id = gInventory.getLinkedItemID(inv_id);

	HBFloaterEditEnvSettings* self = NULL;
	instances_map_t::iterator it = sInstances.find(inv_id);
	if (it == sInstances.end())
	{
		LLViewerInventoryItem* itemp = gInventory.getItem(inv_id);
		if (!itemp || itemp->getIsBrokenLink())
		{
			llwarns << "Could not find inventory item, Id: " << inv_id
					<< llendl;
			return NULL;
		}
		if (!itemp->isSettingsType())
		{
			llwarns << "Inventory item " << inv_id
					<< " is not an environment settings item. Floater not created."
					<< llendl;
			return NULL;
		}
		LLSettingsType::EType type = itemp->getSettingsType();
		if (type != LLSettingsType::ST_SKY &&
			type != LLSettingsType::ST_WATER &&
			type != LLSettingsType::ST_DAYCYCLE)
		{
			llwarns << "Invalid environment settings type: " << type
					<< ". Floater not created." << llendl;
			return NULL;
		}
		self = new HBFloaterEditEnvSettings(inv_id, type);
		sInstances[inv_id] = self;
	}
	else
	{
		self = it->second;
	}

	if (self)
	{
		self->open();
		self->setFocus(true);
	}

	return self;
}

//static
HBFloaterEditEnvSettings* HBFloaterEditEnvSettings::create(LLSettingsType::EType type)
{
	if (type != LLSettingsType::ST_SKY && type != LLSettingsType::ST_WATER &&
		type != LLSettingsType::ST_DAYCYCLE)
	{
		llwarns << "Invalid environment settings type: " << type
				<< ". Floater not created." << llendl;
		return NULL;
	}
	HBFloaterEditEnvSettings* self = new HBFloaterEditEnvSettings(LLUUID::null,
																  type);
	if (self)
	{
		self->open();
		self->setFocus(true);
	}
	return self;
}

//static
void HBFloaterEditEnvSettings::destroy(const LLUUID& inv_id)
{
	instances_map_t::iterator it = sInstances.find(inv_id);
	if (it != sInstances.end())
	{
		// Let it no chance to save anything...
		HBFloaterEditEnvSettings* self = it->second;
		self->setDirty(false);
		self->close();
	}
}

//static
void* HBFloaterEditEnvSettings::createSettingsPanel(void* userdata)
{
	HBFloaterEditEnvSettings* self = (HBFloaterEditEnvSettings*)userdata;
	switch (self->mSettingsType)
	{
		case LLSettingsType::ST_SKY:
			self->mEditPanel = new LLPanelEnvSettingsSky();
			break;

		case LLSettingsType::ST_WATER:
			self->mEditPanel = new LLPanelEnvSettingsWater();
			break;

		case LLSettingsType::ST_DAYCYCLE:
			self->mEditPanel = new LLPanelEnvSettingsDay();
			break;

		default:
			llerrs << "Unknown settings type !" << llendl;
	}
	return self->mEditPanel;
}

HBFloaterEditEnvSettings::HBFloaterEditEnvSettings(const LLUUID& inv_id,
												   LLSettingsType::EType type)
:	mInventoryId(inv_id),
	mSettingsType(type),
	mInventoryItem(NULL),
	mSaveAsNewCounter(0),
	mCanSave(false),
	mCanCopy(false),
	mCanModify(false),
	mCanTransfer(false)
{
	LLCallbackMap::map_t factory_map;
	factory_map["settings_panel"] = LLCallbackMap(createSettingsPanel, this);
	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_edit_settings.xml",
												 &factory_map, false);
}

//virtual
HBFloaterEditEnvSettings::~HBFloaterEditEnvSettings()
{
	if (mInventoryId.notNull())
	{
		sInstances.erase(mInventoryId);
	}
}

//virtual
bool HBFloaterEditEnvSettings::postBuild()
{
	std::string title = getTitle();
	switch (mSettingsType)
	{
		case LLSettingsType::ST_SKY:
			title = getString("edit_sky");
			break;

		case LLSettingsType::ST_WATER:
			title = getString("edit_water");
			break;

		case LLSettingsType::ST_DAYCYCLE:
			title = getString("edit_daycycle");
			break;

		default:
			// This shall never happen because of the llerrs in
			// createSettingsPanel() default path
			break;
	}
	setTitle(title);

	mLoadBtn = getChild<LLButton>("btn_load");
	mLoadBtn->setClickedCallback(onButtonLoad, this);

	mImportBtn = getChild<LLButton>("btn_import");
	mImportBtn->setClickedCallback(onButtonImport, this);

	mCancelBtn = getChild<LLButton>("btn_cancel");
	mCancelBtn->setClickedCallback(onButtonCancel, this);

	mApplyBtn = getChild<LLFlyoutButton>("btn_apply");
	mApplyBtn->setCommitCallback(onButtonApply);
	mApplyBtn->setCallbackUserData(this);

	mSaveBtn = getChild<LLButton>("btn_save");
	mSaveBtn->setClickedCallback(onButtonSave, this);

	mSaveAsNewBtn = getChild<LLButton>("btn_save_as_new");
	mSaveAsNewBtn->setClickedCallback(onButtonSaveAsNew, this);

	mNameEditor = getChild<LLLineEditor>("settings_name");
	mNameEditor->setPrevalidate(LLLineEditor::prevalidateASCII);
	mNameEditor->setCommitOnFocusLost(true);
	mNameEditor->setCommitCallback(onNameChanged);
	mNameEditor->setCallbackUserData(this);

	// Reduce the floater size for Water and Sky settings, that got a smaller
	// panel. The height difference is kept as a "string" element in the
	// floater xml file.
	if (mSettingsType != LLSettingsType::ST_DAYCYCLE)
	{
		const LLRect& rect = getRect();
		std::string delta = getString("DELTA_HEIGHT");
		reshape(rect.getWidth(), rect.getHeight() - atoi(delta.c_str()));
	}

	// Place it in a smart way, like preview floaters...
	S32 left, top;
	gFloaterViewp->getNewFloaterPosition(&left, &top);
	translate(left - getRect().mLeft, top - getRect().mTop);

	gFloaterViewp->adjustToFitScreen(this);

	loadInventoryItem(mInventoryId);

	return true;
}

static void close_confirm_cb(const LLSD& notification, const LLSD& response,
							 void* userdata)
{
	if (userdata &&
		LLNotification::getSelectedOption(notification, response) == 0)
	{
		HBFloaterEditEnvSettings* floaterp = (HBFloaterEditEnvSettings*)userdata;
		floaterp->setDirty(false);
		floaterp->close();
	}
}

//virtual
void HBFloaterEditEnvSettings::onClose(bool app_quitting)
{
	if (!app_quitting)
	{
		if (isDirty())
		{
			gNotifications.add("SettingsConfirmLoss", LLSD(), LLSD(),
							   boost::bind(&close_confirm_cb, _1, _2, this));
			return;
		}

		gEnvironment.setSelectedEnvironment(LLEnvironment::ENV_LOCAL);
		gEnvironment.setCurrentEnvironmentSelection(LLEnvironment::ENV_LOCAL);
		gEnvironment.clearEnvironment(LLEnvironment::ENV_EDIT);
	}
	LLFloater::onClose(app_quitting);
}

//virtual
void HBFloaterEditEnvSettings::onFocusReceived()
{
	if (isInVisibleChain())
	{
		gSavedSettings.setBool("UseParcelEnvironment", true);
		updateEditEnvironment();
		gEnvironment.setSelectedEnvironment(LLEnvironment::ENV_EDIT,
											LLEnvironment::TRANSITION_FAST);
	}
	LLFloater::onFocusReceived();
}

//virtual
void HBFloaterEditEnvSettings::refresh()
{
	if (!mEditPanel->settingsValid())
	{
		mNameEditor->setEnabled(false);
		mApplyBtn->setEnabled(false);
		mSaveAsNewBtn->setEnabled(false);
		return;
	}

	if (mEditPanel->getEditContext() < LLPanelEnvSettings::CONTEXT_PARCEL)
	{
		mNameEditor->setText(mEditPanel->getSettingsName());
		mNameEditor->setEnabled(mCanModify);
		mApplyBtn->setEnabled(true);
		mSaveAsNewBtn->setEnabled(mCanCopy && mCanSave);
	}
	else
	{
		mNameEditor->setEnabled(false);
	}

	mEditPanel->setCanEdit(mCanModify);
	mEditPanel->refresh();

	LLFloater::refresh();
}

//virtual
void HBFloaterEditEnvSettings::draw()
{
	mSaveBtn->setEnabled(mCanModify && mCanSave && isDirty() &&
						 mNameEditor->getLength() > 0);

	// Refresh the state of the buttons that depend on the file selector
	// availability, whenever the latter changed.
	static bool can_use_file_selector = false;
	bool available = !HBFileSelector::isInUse();
	if (available != can_use_file_selector)
	{
		can_use_file_selector = available;
		mImportBtn->setEnabled(available);
		mEditPanel->setFileLoadingAvailable(available);
	}

	LLFloater::draw();
}

bool HBFloaterEditEnvSettings::isDirty() const
{
	return mEditPanel->settingsValid() && mEditPanel->isDirty();
}

void HBFloaterEditEnvSettings::setDirty(bool dirty)
{
	mEditPanel->setDirty(dirty);
}

void HBFloaterEditEnvSettings::setEditContextInventory()
{
	mEditPanel->setEditContext(LLPanelEnvSettings::CONTEXT_INVENTORY);
	mSaveBtn->setToolTip(getString("tip_save_inventory"));
	mApplyBtn->setVisible(true);
	mSaveAsNewBtn->setVisible(true);
}

void HBFloaterEditEnvSettings::setEditContextParcel()
{
	mEditPanel->setEditContext(LLPanelEnvSettings::CONTEXT_PARCEL);
	mSaveBtn->setToolTip(getString("tip_save_parcel"));
	mApplyBtn->setVisible(false);
	mSaveAsNewBtn->setVisible(false);
	mNameEditor->setText(getString("parcel_settings"));
}

void HBFloaterEditEnvSettings::setEditContextRegion()
{
	mEditPanel->setEditContext(LLPanelEnvSettings::CONTEXT_REGION);
	mSaveBtn->setToolTip(getString("tip_save_region"));
	mApplyBtn->setVisible(false);
	mSaveAsNewBtn->setVisible(false);
	mNameEditor->setText(getString("region_settings"));
}

void HBFloaterEditEnvSettings::setDayLength(S32 seconds)
{
	LLPanelEnvSettingsDay* panelp =
		dynamic_cast<LLPanelEnvSettingsDay*>(mEditPanel);
	if (panelp)
	{
		panelp->setDayLength(seconds);
	}
}

HBFloaterEditEnvSettings::connection_t HBFloaterEditEnvSettings::setCommitCB(commit_cb_t cb)
{
	return mCommitSignal.connect(cb);
}

void HBFloaterEditEnvSettings::loadInventoryItem(LLUUID inv_id,
												 const std::string& notify)
{
	// Make sure we are not trying to edit a link and get the linked item Id
	// in that case.
	if (inv_id.notNull())
	{
		inv_id = gInventory.getLinkedItemID(inv_id);
	}

	if (mInventoryId != inv_id &&
		// Do not register our floater when we are editing parcel or region
		// settings
		mEditPanel->getEditContext() <= LLPanelEnvSettings::CONTEXT_INVENTORY)
	{
		// Remove any old instance
		if (mInventoryId.notNull() && sInstances.count(mInventoryId))
		{
			sInstances.erase(mInventoryId);
		}
		// Register our instance as associated with the new inventory item, if
		// any.
		if (inv_id.notNull())
		{
			if (sInstances.count(inv_id))
			{
				llwarns << "Another floater is opened for inventory item: "
						<< inv_id << ". Closing this floater." << llendl;
				// Do not remove the other instance entry in destructor for
				// this floater...
				mInventoryId.setNull();
				setDirty(false);
				close();
				return;
			}
			sInstances[inv_id] = this;
		}
		mInventoryId = inv_id;
	}

	if (inv_id.isNull())
	{
		// This is an import of legacy Windlight settings, or a floater opened
		// from HBPanelLandEnvironment for a custom environment.
		mInventoryId.setNull();
		mInventoryItem = NULL;
		mCanSave = mCanCopy = mCanModify = mCanTransfer = true;
		return;
	}

	mInventoryItem = gInventory.getItem(inv_id);
	if (!mInventoryItem || mInventoryItem->getIsBrokenLink())
	{
		llwarns << "Could not find inventory item: " << inv_id
				<< ". Closing floater." << llendl;
		gNotifications.add("CantFindInvItem");
		setDirty(false);
		close();
		return;
	}

	if (!mInventoryItem->isSettingsType())
	{
		llwarns << "Inventory item " << inv_id
				<< " is not an environment settings item. Closing floater."
				<< llendl;
		gNotifications.add("UnableEditItem");
		setDirty(false);
		close();
		return;
	}

	LLSettingsType::EType type = mInventoryItem->getSettingsType();
	if (type != mSettingsType)
	{
		llwarns << "Bad environment settings type for inventory item: "
				<< inv_id << ". Was expecting type " << mSettingsType
				<< " and got type " << type << ". Closing floater." << llendl;
		gNotifications.add("UnableEditItem");
		setDirty(false);
		close();
		return;
	}

	const LLUUID& asset_id = mInventoryItem->getAssetUUID();
	if (asset_id.isNull())
	{
		llwarns << "Null asset Id for inventory item: " << inv_id
				<< ". Closing floater." << llendl;
		gNotifications.add("UnableEditItem");
		setDirty(false);
		close();
		return;
	}

	if (!notify.empty())
	{
		LLSD args;
		args["NAME"] = mInventoryItem->getName();
		gNotifications.add(notify, args);
	}

	// *TODO: should we restrict parcel and region settings to full-perm inventory
	// settings ?  This does not seem to be the case in LL's viewer-eep code, but
	// what is LL's actual policy on it ???  HB
	mCanSave = true;
	const LLPermissions& perms = mInventoryItem->getPermissions();
	mCanCopy = perms.allowCopyBy(gAgentID);
	mCanModify = perms.allowModifyBy(gAgentID);
	mCanTransfer = perms.allowTransferBy(gAgentID);
	mEditPanel->setEnabled(false);

	LLHandle<LLFloater> handle = getHandle();
	LLEnvSettingsBase::getSettingsAsset(asset_id,
										[handle](LLUUID id,
												 LLSettingsBase::ptr_t settings,
												 S32 status, LLExtStat)
										{
											HBFloaterEditEnvSettings* self =
												(HBFloaterEditEnvSettings*)handle.get();
											if (self)
											{
												self->onAssetLoaded(id,
																	settings,
																	status);
											}
										});
}

void HBFloaterEditEnvSettings::loadDefaultSettings()
{
	if (mInventoryItem || mInventoryId.notNull())
	{
		llwarns << "A settings asset is alreadly loaded. Aborting." << llendl;
		return;
	}

	LLUUID asset_id;
	switch (mSettingsType)
	{
		case LLSettingsType::ST_SKY:
			asset_id = LLSettingsSky::getDefaultAssetId();
			break;

		case LLSettingsType::ST_WATER:
			asset_id = LLSettingsWater::getDefaultAssetId();
			break;

		case LLSettingsType::ST_DAYCYCLE:
			asset_id = LLSettingsDay::getDefaultAssetId();

		default:	// Cannot happen; just here to keep the compilers happy
			break;
	}

	LLHandle<LLFloater> handle = getHandle();
	LLEnvSettingsBase::getSettingsAsset(asset_id,
										[handle](LLUUID id,
												 LLSettingsBase::ptr_t settings,
												 S32 status, LLExtStat)
										{
											HBFloaterEditEnvSettings* self =
												(HBFloaterEditEnvSettings*)handle.get();
											if (self)
											{
												self->onAssetLoaded(id,
																	settings,
																	status);
											}
										});
}

void HBFloaterEditEnvSettings::onAssetLoaded(const LLUUID& asset_id,
											 LLSettingsBase::ptr_t settings,
											 S32 status)
{
	if (mInventoryItem && mInventoryItem->getAssetUUID() != asset_id)
	{
		llwarns << "Ignoring stale callback for asset Id: " << asset_id
				<< llendl;
		return;
	}

	if (!settings || status)
	{
		gNotifications.add("CantFindInvItem");
		setDirty(false);
		close();
		return;
	}

	if (mInventoryItem)
	{
		settings->setName(mInventoryItem->getName());
	}

	bool land_context =
		mEditPanel->getEditContext() >= LLPanelEnvSettings::CONTEXT_PARCEL;
	// Forget the inventory item when editing parcel or region settings
	if (land_context)
	{
		mInventoryItem = NULL;
	}

	if (settings->getFlag(LLSettingsBase::FLAG_NOSAVE))
	{
		mCanSave = mCanCopy = mCanModify = mCanTransfer = false;
	}
	else
	{
		if (mCanCopy)
		{
			settings->clearFlag(LLSettingsBase::FLAG_NOCOPY);
		}
		else
		{
			settings->setFlag(LLSettingsBase::FLAG_NOCOPY);
		}
		if (mCanModify || land_context)
		{
			settings->clearFlag(LLSettingsBase::FLAG_NOMOD);
		}
		else
		{
			settings->setFlag(LLSettingsBase::FLAG_NOMOD);
		}
		if (mCanTransfer)
		{
			settings->clearFlag(LLSettingsBase::FLAG_NOTRANS);
		}
		else
		{
			settings->setFlag(LLSettingsBase::FLAG_NOTRANS);
		}
	}
	setSettings(settings);

	if (land_context)
	{
		// Set dirty since we just changed the edited settings from the
		// parcel/region current settings.
		setDirty();
	}

	gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_FAST);
}

void HBFloaterEditEnvSettings::setSettings(const LLSettingsBase::ptr_t& settings)
{
	mEditPanel->setSettings(settings);
	mCanSave = !settings->getFlag(LLSettingsBase::FLAG_NOSAVE);
	if (mCanSave)
	{
		mCanCopy = !settings->getFlag(LLSettingsBase::FLAG_NOCOPY);
		mCanModify = !settings->getFlag(LLSettingsBase::FLAG_NOMOD);
		mCanTransfer = !settings->getFlag(LLSettingsBase::FLAG_NOTRANS);
	}
	else
	{
		mCanCopy = mCanModify = mCanTransfer = false;
	}
	refresh();
}

void HBFloaterEditEnvSettings::updateEditEnvironment()
{
	mEditPanel->updateEditEnvironment();
}

void HBFloaterEditEnvSettings::doApplyCreateNewInventory(const std::string& settings_name,
														 const LLSettingsBase::ptr_t& settings)
{
	LLHandle<LLFloater> handle = getHandle();
	if (mInventoryItem)
	{
		const LLUUID& parent_id = mInventoryItem->getParentUUID();
		U32 permission = mInventoryItem->getPermissions().getMaskNextOwner();
		LLEnvSettingsBase::createInventoryItem(settings, permission, parent_id,
											   settings_name,
											   [handle](LLUUID, LLUUID inv_id,
														LLUUID, LLSD results)
											   {
													HBFloaterEditEnvSettings* self =
														(HBFloaterEditEnvSettings*)handle.get();
													if (self)
													{
														self->onInventoryCreated(inv_id,
																				 results);
													}
											   });
	}
	else
	{
		const LLUUID& folder_id =
			gInventory.findCategoryUUIDForType(LLFolderType::FT_SETTINGS);
			LLEnvSettingsBase::createInventoryItem(settings, folder_id,
												   settings_name,
											   [handle](LLUUID, LLUUID inv_id,
														LLUUID, LLSD results)
											   {
													HBFloaterEditEnvSettings* self =
														(HBFloaterEditEnvSettings*)handle.get();
													if (self)
													{
														self->onInventoryCreated(inv_id,
																				 results);
													}
											   });
	}
}

void HBFloaterEditEnvSettings::doApplyUpdateInventory(const LLSettingsBase::ptr_t& settings)
{
	LLHandle<LLFloater> handle = getHandle();
	if (mInventoryId.isNull())
	{
		const LLUUID& folder_id =
			gInventory.findCategoryUUIDForType(LLFolderType::FT_SETTINGS);
		LLEnvSettingsBase::createInventoryItem(settings, folder_id, "",
											   [handle](LLUUID, LLUUID inv_id,
														LLUUID, LLSD results)
											   {
													HBFloaterEditEnvSettings* self =
														(HBFloaterEditEnvSettings*)handle.get();
													if (self)
													{
														self->onInventoryCreated(inv_id,
																				 results);
													}
											   });
	}
	else
	{
		LLEnvSettingsBase::updateInventoryItem(settings, mInventoryId,
											   [handle](LLUUID, LLUUID inv_id,
														LLUUID, LLSD)
											   {
													HBFloaterEditEnvSettings* self =
														(HBFloaterEditEnvSettings*)handle.get();
													if (self)
													{
														self->onInventoryUpdated(inv_id);
													}
											   });
	}
}

void HBFloaterEditEnvSettings::onInventoryCreated(const LLUUID& inv_id,
											   bool copied)
{
	if (mInventoryItem)
	{
		const LLPermissions& perms = mInventoryItem->getPermissions();
		LLViewerInventoryItem* created_itemp = gInventory.getItem(inv_id);
		if (created_itemp)
		{
			created_itemp->setPermissions(perms);
			created_itemp->updateServer(false);
		}
		else
		{
			llwarns << "Cound not find the newly created inventory item, Id: "
					<< inv_id << llendl;
		}
	}
	loadInventoryItem(inv_id, copied ? "SettingsCopied" : "SettingsCreated");
	setFocus(true);
}

void HBFloaterEditEnvSettings::onInventoryCreated(const LLUUID& inv_id,
												  const LLSD& results)
{
	if (inv_id.notNull() && results.has("success") &&
		results["success"].asBoolean())
	{
		onInventoryCreated(inv_id);
	}
	else
	{
		gNotifications.add("CantCreateInventory");
	}
}

void HBFloaterEditEnvSettings::onInventoryUpdated(const LLUUID& inv_id)
{
	if (inv_id != mInventoryId)
	{
		loadInventoryItem(inv_id, "SettingsCreated");
	}
	else
	{
		// No need to reload settings data, but we need to reset the dirty flag
		setDirty(false);
	}
}

void HBFloaterEditEnvSettings::importFromFile(const std::string& filename)
{
	LLSD messages;
	switch (mSettingsType)
	{
		case LLSettingsType::ST_SKY:
		{
			 LLSettingsSky::ptr_t skyp =
				LLEnvironment::createSkyFromLegacyPreset(filename, messages);
			if (!skyp)
			{
				gNotifications.add("WLImportFail", messages);
				return;
			}
			loadInventoryItem(LLUUID::null);
			gEnvironment.setEnvironment(LLEnvironment::ENV_EDIT, skyp);
			setSettings(skyp);
			setDirty();
			break;
		}

		case LLSettingsType::ST_WATER:
		{
			LLSettingsWater::ptr_t waterp =
				LLEnvironment::createWaterFromLegacyPreset(filename, messages);
			if (!waterp)
			{
				gNotifications.add("WLImportFail", messages);
				return;
			}
			loadInventoryItem(LLUUID::null);
			gEnvironment.setEnvironment(LLEnvironment::ENV_EDIT, waterp);
			setSettings(waterp);
			setDirty();
			break;
		}

		case LLSettingsType::ST_DAYCYCLE:
		{
			LLSettingsDay::ptr_t dayp =
				LLEnvironment::createDayCycleFromLegacyPreset(filename,
															  messages);
			if (!dayp)
			{
				gNotifications.add("WLImportFail", messages);
				return;
			}
			loadInventoryItem(LLUUID::null);
			gEnvironment.setEnvironment(LLEnvironment::ENV_EDIT, dayp);
			setSettings(dayp);
			setDirty();
			break;
		}

		default:	// Should never happen...
			return;
	}
	gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_FAST, true);
	refresh();
	setFocus(true);
}

//static
void HBFloaterEditEnvSettings::onNameChanged(LLUICtrl*, void* userdata)
{
	HBFloaterEditEnvSettings* self = (HBFloaterEditEnvSettings*)userdata;
	if (self && self->mEditPanel->settingsValid())
	{
		self->mEditPanel->setSettingsName(self->mNameEditor->getText());
		self->setDirty();
	}
}

static void inv_items_picker_cb(const std::vector<std::string>&,
								const uuid_vec_t& ids, void* userdata, bool)
{
	HBFloaterEditEnvSettings* floaterp = (HBFloaterEditEnvSettings*)userdata;
	if (floaterp && !ids.empty())
	{
		floaterp->loadInventoryItem(ids[0]);
	}
}

static void load_confirm_cb(const LLSD& notification, const LLSD& response,
							S32 sub_type, void* data)
{
	if (data && LLNotification::getSelectedOption(notification, response) == 0)
	{
		HBFloaterInvItemsPicker* pickerp =
			new HBFloaterInvItemsPicker((LLView*)data, inv_items_picker_cb,
										data);
		if (pickerp)
		{
			pickerp->setExcludeLibrary();
			pickerp->setAssetType(LLAssetType::AT_SETTINGS, sub_type);
		}
	}
}

//static
void HBFloaterEditEnvSettings::onButtonLoad(void* userdata)
{
	HBFloaterEditEnvSettings* self = (HBFloaterEditEnvSettings*)userdata;
	if (self)
	{
		S32 sub_type = self->mSettingsType;
		if (self->isDirty())
		{
			gNotifications.add("SettingsConfirmLoss", LLSD(), LLSD(),
							   boost::bind(&load_confirm_cb, _1, _2, sub_type,
										   self));
			return;
		}
		HBFloaterInvItemsPicker* pickerp =
			new HBFloaterInvItemsPicker(self, inv_items_picker_cb, self);
		if (pickerp)
		{
			pickerp->setExcludeLibrary();
			pickerp->setAssetType(LLAssetType::AT_SETTINGS, sub_type);
		}
	}
}

static void do_import_cb(HBFileSelector::ELoadFilter, std::string& filename,
						void* userdata)
{
	HBFloaterEditEnvSettings* floaterp = (HBFloaterEditEnvSettings*)userdata;
	if (floaterp && !filename.empty())
	{
		floaterp->importFromFile(filename);
	}
}

static void import_confirm_cb(const LLSD& notification, const LLSD& response,
							  void* userdata)
{
	if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		HBFileSelector::loadFile(HBFileSelector::FFLOAD_XML, do_import_cb,
								userdata);
	}
}

//static
void HBFloaterEditEnvSettings::onButtonImport(void* userdata)
{
	HBFloaterEditEnvSettings* self = (HBFloaterEditEnvSettings*)userdata;
	if (!self) return;	// Paranoia

	if (self->isDirty())
	{
		gNotifications.add("SettingsConfirmLoss", LLSD(), LLSD(),
						   boost::bind(&import_confirm_cb, _1, _2, userdata));
	}
	else
	{
		HBFileSelector::loadFile(HBFileSelector::FFLOAD_XML, do_import_cb,
								userdata);
	}
}

//static
void HBFloaterEditEnvSettings::onButtonApply(LLUICtrl* ctrl, void* userdata)
{
	HBFloaterEditEnvSettings* self = (HBFloaterEditEnvSettings*)userdata;
	if (!self || !ctrl || !self->mEditPanel->settingsValid()) return;

	if (self->mInventoryId.notNull() && self->mInventoryItem &&
		gInventory.getItem(self->mInventoryId) != self->mInventoryItem)
	{
		LLSD args;
		args["MESSAGE"] = self->getString("inventory_gone");
		gNotifications.add("GenericAlert", args);
		self->setDirty(false);
		self->close();
		return;
	}

	U32 flags = 0;
	if (self->mInventoryItem)
	{
		const LLPermissions& perms = self->mInventoryItem->getPermissions();
		if (!perms.allowModifyBy(gAgentID))
		{
			flags |= LLSettingsBase::FLAG_NOMOD;
		}
		if (!perms.allowTransferBy(gAgentID))
		{
			flags |= LLSettingsBase::FLAG_NOTRANS;
		}
	}

	std::string operation = ctrl->getValue().asString();
	if (operation == "apply_parcel")
	{
		if (!self->mInventoryItem || self->isDirty())
		{
			gNotifications.add("SaveSettingsFirst");
			return;
		}
		LLParcel* parcel = gViewerParcelMgr.getSelectedOrAgentParcel();
		if (!parcel || parcel->getLocalID() == INVALID_PARCEL_ID ||
			!LLEnvironment::canAgentUpdateParcelEnvironment(parcel))
		{
			gNotifications.add("WLParcelApplyFail");
			return;
		}
		gEnvironment.updateParcel(parcel->getLocalID(),
								  self->mInventoryItem->getAssetUUID(),
								  self->mInventoryItem->getName(),
								  LLEnvironment::NO_TRACK, -1, -1, flags);
		self->mEditPanel->updateParcel(parcel->getLocalID());
	}
	else if (operation == "apply_region")
	{
		if (!self->mInventoryItem || self->isDirty())
		{
			gNotifications.add("SaveSettingsFirst");
			return;
		}
		if (!LLEnvironment::canAgentUpdateRegionEnvironment())
		{
			LLSD args;
			args["FAIL_REASON"] = LLTrans::getString("no_permission");
			gNotifications.add("WLRegionApplyFail", args);
			return;
		}
		gEnvironment.updateRegion(self->mInventoryItem->getAssetUUID(),
								  self->mInventoryItem->getName(),
								  LLEnvironment::NO_TRACK, -1, -1, flags);
		self->mEditPanel->updateRegion();
	}
	else	// "apply_local" in pull-down list or direct click on the button
	{
		self->mEditPanel->updateLocal();
	}
}

//static
void HBFloaterEditEnvSettings::onButtonSave(void* userdata)
{
	HBFloaterEditEnvSettings* self = (HBFloaterEditEnvSettings*)userdata;
	if (!self || !self->mEditPanel->settingsValid()) return;

	LLSD args;
	if (self->mEditPanel->hasLocalTextures(args))
	{
		gNotifications.add("WLLocalTextureFixedBlock", args);
		return;
	}

	// If we are editing parcel or region settings, call any configured
	// callback and close.
	LLPanelEnvSettings::EEditContext ctx = self->mEditPanel->getEditContext();
	if (ctx >= LLPanelEnvSettings::CONTEXT_PARCEL)
	{
		if (self->mCommitSignal.empty())
		{
			llwarns << "No active callback found for this "
					<< (ctx == LLPanelEnvSettings::CONTEXT_PARCEL ? "parcel"
																  : "region")
					<< " update. Changes are lost." << llendl;
		}
		else
		{
			self->mCommitSignal(self->mEditPanel->getSettingsClone());
		}
		self->setDirty(false);
		self->close();
		return;
	}

	if (!gAgent.hasInventorySettings())
	{
		return;
	}

	if (self->mInventoryId.notNull() && self->mInventoryItem &&
		gInventory.getItem(self->mInventoryId) != self->mInventoryItem)
	{
		args["MESSAGE"] = self->getString("inventory_gone");
		gNotifications.add("GenericAlert", args);
		self->setDirty(false);
		self->close();
		return;
	}

	if (!self->mCanModify)
	{
		args["MESSAGE"] = self->getString("no_mod_settings");
		gNotifications.add("GenericAlert", args);
		return;
	}

	self->doApplyUpdateInventory(self->mEditPanel->getSettingsClone());
}

//static
void HBFloaterEditEnvSettings::onButtonSaveAsNew(void* userdata)
{
	HBFloaterEditEnvSettings* self = (HBFloaterEditEnvSettings*)userdata;
	if (!self || !self->mEditPanel->settingsValid()) return;

	if (!gAgent.hasInventorySettings())
	{
		return;
	}

	if (self->mInventoryId.notNull() && self->mInventoryItem &&
		gInventory.getItem(self->mInventoryId) != self->mInventoryItem)
	{
		LLSD args;
		args["MESSAGE"] = self->getString("inventory_gone");
		gNotifications.add("GenericAlert", args);
		self->setDirty(false);
		self->close();
		return;
	}

	LLSD args;
	if (!self->mCanCopy)
	{
		args["MESSAGE"] = self->getString("no_copy_settings");
		gNotifications.add("GenericAlert", args);
		return;
	}
	if (self->mEditPanel->hasLocalTextures(args))
	{
		if (self->mSettingsType == LLSettingsType::ST_DAYCYCLE)
		{
			gNotifications.add("WLLocalTextureDayBlock", args);
		}
		else
		{
			gNotifications.add("WLLocalTextureFixedBlock", args);
		}
		return;
	}

	LLViewerInventoryItem* itemp = self->mInventoryItem;
	if (!itemp)
	{
		if (!self->mCanModify)
		{
			gNotifications.add("CantCreateInventory");
			return;
		}
		self->doApplyCreateNewInventory(self->mEditPanel->getSettingsName(),
										self->mEditPanel->getSettingsClone());
		return;
	}

	const LLUUID& marketplace_id =
		gInventory.findCategoryUUIDForType(LLFolderType::FT_MARKETPLACE_LISTINGS,
										   false);
	const LLUUID& library_id = gInventory.getLibraryRootFolderID();
	LLUUID parent_id = itemp->getParentUUID();
	if (gInventory.isObjectDescendentOf(itemp->getUUID(), marketplace_id) ||
		gInventory.isObjectDescendentOf(itemp->getUUID(), library_id))
	{
		parent_id =
			gInventory.findCategoryUUIDForType(LLFolderType::FT_SETTINGS);
	}

	// Create a new name for the settings inventory item. We try and keep track
	// of former versions with the same base name, and incerement the version
	// each time.
	std::string name = self->mEditPanel->getSettingsName();
	if (!self->mOriginalName.empty() &&
		name == self->mOriginalName + llformat(" %d", self->mSaveAsNewCounter))
	{
		name = self->mOriginalName +
			   llformat(" %d", ++self->mSaveAsNewCounter);
	}
	else
	{
		self->mSaveAsNewCounter = 1;
		self->mOriginalName = name;
		name += " 1";
	}

	LLPointer<LLInventoryCallback> cb =
		new HBSettingsCopiedCallback(self->getHandle());
	copy_inventory_item(itemp->getPermissions().getOwner(), itemp->getUUID(),
						parent_id, name, cb);
}

//static
void HBFloaterEditEnvSettings::onButtonCancel(void* userdata)
{
	HBFloaterEditEnvSettings* self = (HBFloaterEditEnvSettings*)userdata;
	if (self)	// Paranoia
	{
		self->close();
	}
}

///////////////////////////////////////////////////////////////////////////////
// HBFloaterLocalEnv class
///////////////////////////////////////////////////////////////////////////////

constexpr S32 FLOATER_LOCAL_ENV_UPDATE = -2;

//static
void HBFloaterLocalEnv::closeInstance()
{
	HBFloaterLocalEnv* self = HBFloaterLocalEnv::findInstance();
	if (self)
	{
		self->close();
	}
}

//static
void* HBFloaterLocalEnv::createSkySettingsPanel(void* userdata)
{
	HBFloaterLocalEnv* self = (HBFloaterLocalEnv*)userdata;
	self->mEditSkyPanel = new LLPanelEnvSettingsSky();
	return self->mEditSkyPanel;
}

//static
void* HBFloaterLocalEnv::createWaterSettingsPanel(void* userdata)
{
	HBFloaterLocalEnv* self = (HBFloaterLocalEnv*)userdata;
	self->mEditWaterPanel = new LLPanelEnvSettingsWater();
	return self->mEditWaterPanel;
}

HBFloaterLocalEnv::HBFloaterLocalEnv(const LLSD&)
{
	LLCallbackMap::map_t factory_map;
	factory_map["sky_panel"] = LLCallbackMap(createSkySettingsPanel, this);
	factory_map["water_panel"] = LLCallbackMap(createWaterSettingsPanel, this);
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_local_env.xml",
												 &factory_map);
}

//virtual
bool HBFloaterLocalEnv::postBuild()
{
	mResetBtn = getChild<LLButton>("btn_reset");
	mResetBtn->setClickedCallback(onButtonReset, this);

	mFixedTimeCheck = getChild<LLCheckBoxCtrl>("fixed_time_check");
	mFixedTimeCheck->setCommitCallback(onCheckFixedTime);
	mFixedTimeCheck->setCallbackUserData(this);

	mFixedTimeSlider = getChild<LLSliderCtrl>("fixed_time_slider");
	mFixedTimeSlider->setCommitCallback(onCommitFixedTime);
	mFixedTimeSlider->setCallbackUserData(this);

	childSetAction("btn_close", onButtonClose, this);

	gSavedSettings.setBool("UseLocalEnvironment", true);

	captureCurrentEnvironment();

	mEventConnection = gEnvironment.setEnvironmentChanged(
							[this](LLEnvironment::EEnvSelection env,
								   S32 version)
							{
								if (env == LLEnvironment::ENV_LOCAL &&
									version != FLOATER_LOCAL_ENV_UPDATE)
								{
									captureCurrentEnvironment();
								}
							});

	return true;
}

//virtual
void HBFloaterLocalEnv::onClose(bool app_quitting)
{
	if (mEventConnection.connected())
	{
		mEventConnection.disconnect();
	}
	mLiveSky.reset();
	mLiveWater.reset();
	LLFloater::onClose(app_quitting);
}

//virtual
void HBFloaterLocalEnv::refresh()
{
	bool enabled = mLiveSky && mLiveWater;

	mResetBtn->setEnabled(enabled);
	mEditSkyPanel->setCanEdit(enabled);
	mEditWaterPanel->setCanEdit(enabled);
}

//MK
//virtual
void HBFloaterLocalEnv::draw()
{
	// Fast enough that it can be kept here
	if (gRLenabled && gRLInterface.mContainsSetenv)
	{
		close();
		return;
	}
	LLFloater::draw();
}
//mk

void HBFloaterLocalEnv::captureCurrentEnvironment()
{
	constexpr LLEnvironment::EEnvSelection PARCEL = LLEnvironment::ENV_PARCEL;
	constexpr LLEnvironment::EEnvSelection LOCAL = LLEnvironment::ENV_LOCAL;

	bool update_local = true;

	if (!gEnvironment.hasEnvironment(LOCAL))
	{
		mLiveSky =
			gEnvironment.getEnvironmentFixedSky(PARCEL, true)->buildClone();
		mLiveWater =
			gEnvironment.getEnvironmentFixedWater(PARCEL, true)->buildClone();
	}
	else if (gEnvironment.getEnvironmentDay(LOCAL))
	{
		// We have a full day cycle in the local environment: freeze the sky.
		mLiveSky = gEnvironment.getEnvironmentFixedSky(LOCAL)->buildClone();
		mLiveWater =
			gEnvironment.getEnvironmentFixedWater(LOCAL)->buildClone();
	}
	else
	{
		// Otherwise we can just use the sky.
		mLiveSky = gEnvironment.getEnvironmentFixedSky(LOCAL);
		mLiveWater = gEnvironment.getEnvironmentFixedWater(LOCAL);
		update_local = false;
	}

	mEditSkyPanel->setSettings(mLiveSky);
	mEditWaterPanel->setSettings(mLiveWater);

	if (update_local)
	{
		gEnvironment.setEnvironment(LOCAL, mLiveSky, FLOATER_LOCAL_ENV_UPDATE);
		gEnvironment.setEnvironment(LOCAL, mLiveWater,
									FLOATER_LOCAL_ENV_UPDATE);
	}
	gEnvironment.setSelectedEnvironment(LOCAL);
	gEnvironment.setCurrentEnvironmentSelection(LOCAL);
	gEnvironment.updateEnvironment(LLEnvironment::TRANSITION_INSTANT);

	refresh();
}

//static
void HBFloaterLocalEnv::onCheckFixedTime(LLUICtrl*, void* userdata)
{
	HBFloaterLocalEnv* self = (HBFloaterLocalEnv*)userdata;
	if (self)
	{
		bool checked = self->mFixedTimeCheck->get();
		self->mFixedTimeCheck->setEnabled(!checked);
		self->mFixedTimeSlider->setEnabled(checked);
	}
}

//static
void HBFloaterLocalEnv::onCommitFixedTime(LLUICtrl*, void* userdata)
{
	HBFloaterLocalEnv* self = (HBFloaterLocalEnv*)userdata;
	if (self)
	{
		gEnvironment.setFixedTimeOfDay(self->mFixedTimeSlider->getValueF32());
	}
}

//static
void HBFloaterLocalEnv::onButtonReset(void* userdata)
{
	HBFloaterLocalEnv* self = (HBFloaterLocalEnv*)userdata;
	if (self)
	{
		self->mFixedTimeCheck->set(false);
		self->mFixedTimeCheck->setEnabled(true);
		self->mFixedTimeSlider->setEnabled(false);
		gEnvironment.clearEnvironment(LLEnvironment::ENV_LOCAL);
		gEnvironment.setSelectedEnvironment(LLEnvironment::ENV_LOCAL);
		gEnvironment.setCurrentEnvironmentSelection(LLEnvironment::ENV_LOCAL);
		gEnvironment.updateEnvironment();
	}
}

//static
void HBFloaterLocalEnv::onButtonClose(void* userdata)
{
	HBFloaterLocalEnv* self = (HBFloaterLocalEnv*)userdata;
	if (self)
	{
		self->close();
	}
}
