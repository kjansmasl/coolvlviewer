/**
 * @file llfloaterregioninfo.cpp
 * @author Aaron Brashears
 * @brief Implementation of the region info and controls floater and panels.
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

#include "llviewerprecompiledheaders.h"

#include "llfloaterregioninfo.h"

#include "llalertdialog.h"
#include "llbutton.h"
#include "llcachename.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "llcorehttputil.h"
#include "lldir.h"
#include "lldispatcher.h"
#include "llexperiencecache.h"
#include "llfilesystem.h"
#include "llglheaders.h"
#include "llgridmanager.h"					// For gIsInSecondLife
#include "llinventory.h"
#include "lllineeditor.h"
#include "llnamelistctrl.h"
#include "llnotifications.h"
#include "llsliderctrl.h"
#include "llspinctrl.h"
#include "lltabcontainer.h"
#include "lltextbox.h"
#include "lltrans.h"
#include "lluictrlfactory.h"
#include "llxfermanager.h"
#include "llmessage.h"

#include "llagent.h"
#include "llappviewer.h"
#include "llfloateravatarpicker.h"
#include "llfloatergodtools.h"				// For send_sim_wide_deletes()
#include "llfloatergroups.h"
#include "llfloatertelehub.h"
#include "llfloatertopobjects.h"
#include "llinventorymodel.h"
#include "llpanelexperiencelisteditor.h"
#include "hbpanellandenvironment.h"
#include "lltexturectrl.h"
#include "llviewercontrol.h"
#include "llviewerregion.h"
#include "llviewerstats.h"
#include "llviewertexteditor.h"
#include "llviewertexturelist.h"
#include "llvlcomposition.h"

#define ELAR_ENABLED 0 // Enable when server support is implemented

constexpr S32 TERRAIN_TEXTURE_COUNT = 4;
constexpr S32 CORNER_COUNT = 4;
constexpr U32 MAX_LISTED_NAMES = 100;

bool gEstateDispatchInitialized = false;

std::string LLEstateInfoModel::sEstateName;
LLUUID LLEstateInfoModel::sOwnerId;
U32 LLEstateInfoModel::sEstateId = 0;
U64 LLEstateInfoModel::sEstateFlags = 0;
F32 LLEstateInfoModel::sSunHour = 0.f;

///----------------------------------------------------------------------------
/// Local classes
///----------------------------------------------------------------------------

class LLDispatchEstateUpdateInfo final : public LLDispatchHandler
{
public:
	LLDispatchEstateUpdateInfo() = default;

	bool operator()(const LLDispatcher*, const std::string& key,
					const LLUUID& invoice, const sparam_t& strings) override;
};

// key = "estateupdateinfo"
// strings[0] = estate name
// strings[1] = str(owner_id)
// strings[2] = str(estate_id)
// strings[3] = str(estate_flags)
// strings[4] = str((S32)(sun_hour * 1024))
// strings[5] = str(parent_estate_id)
// strings[6] = str(covenant_id)
// strings[7] = str(covenant_timestamp)
// strings[8] = str(send_to_agent_only)
// strings[9] = str(abuse_email_addr)
bool LLDispatchEstateUpdateInfo::operator()(const LLDispatcher*,
											const std::string& key,
											const LLUUID&,
											const sparam_t& strings)
{
	// Unconditionnally fill-up the LLEstateInfoModel member variables.

	// NOTE: LLDispatcher extracts strings with an extra \0 at the end. If we
	// pass the std::string direct to the UI/renderer it draws with a weird
	// character at the end of the string. Therefore do preserve the c_str()
	// calls !
	LLEstateInfoModel::sEstateName = strings[0].c_str();
	LLEstateInfoModel::sOwnerId.set(strings[1]);
	LLEstateInfoModel::sEstateId = strtoul(strings[2].c_str(), NULL, 10);
	U64 flags = strtoul(strings[3].c_str(), NULL, 10);
	LLEstateInfoModel::sEstateFlags = flags;
	LLEstateInfoModel::sSunHour = F32(strtod(strings[4].c_str(),
											 NULL)) / 1024.0f;

	// Then update the agent region if any (and if none, we got disconnected,
	// so give up)

	LLViewerRegion* regionp = gAgent.getRegion();
	if (!regionp) return true;
	regionp->setOwner(LLEstateInfoModel::sOwnerId);

	// And finally, update the region info floater if any (else, give-up)

	LLPanelEstateInfo* panel = LLFloaterRegionInfo::getPanelEstate();
	if (!panel) return true;

	panel->setEstateName(LLEstateInfoModel::sEstateName);

	// Update estate owner name in UI
	if (gCacheNamep)
	{
		gCacheNamep->get(LLEstateInfoModel::sOwnerId, false,
						 LLPanelEstateInfo::callbackCacheName);
	}

	panel->setEstateID(LLEstateInfoModel::sEstateId);

	panel->setEstateFlags(flags);

	if (LLEstateInfoModel::sSunHour == 0 &&
		(flags & REGION_FLAGS_SUN_FIXED) == 0)
	{
		panel->setGlobalTime(true);
	}
	else
	{
		panel->setGlobalTime(false);
		panel->setSunHour(LLEstateInfoModel::sSunHour);
	}

	bool visible_from_mainland = (bool)(flags & REGION_FLAGS_EXTERNALLY_VISIBLE);
	bool god = gAgent.isGodlike();
	bool linden_estate = LLEstateInfoModel::sEstateId <= ESTATE_LAST_LINDEN;

	LLPanelEstateAccess* panel2 = LLFloaterRegionInfo::getPanelAccess();
	if (!panel2) return true;

	// If visible from mainland, disable the access allowed UI, as anyone can
	// teleport there. However, gods need to be able to edit the access list
	// for linden estates, regardless of visibility, to allow object and L$
	// transfers. In OpenSim, ignore linden estate flag.
	bool enable_agent = !visible_from_mainland || (god && linden_estate) ||
						!gIsInSecondLife;
	bool enable_ban = !linden_estate || !gIsInSecondLife;
	panel2->setAccessAllowedEnabled(enable_agent, enable_agent, enable_ban);
	panel2->updateControls(regionp);

	HBPanelLandEnvironment* panel3 =
		LLFloaterRegionInfo::getPanelEnvironment();
	if (panel3)
	{
		panel3->refresh();
	}

	return true;
}

class LLDispatchSetEstateAccess final : public LLDispatchHandler
{
public:
	LLDispatchSetEstateAccess() = default;

	bool operator()(const LLDispatcher*, const std::string& key,
					const LLUUID& invoice, const sparam_t& strings) override;
};


// key = "setaccess"
// strings[0] = str(estate_id)
// strings[1] = str(packed_access_lists)
// strings[2] = str(num allowed agent ids)
// strings[3] = str(num allowed group ids)
// strings[4] = str(num banned agent ids)
// strings[5] = str(num estate manager agent ids)
// strings[6...] = bin(uuid)
bool LLDispatchSetEstateAccess::operator()(const LLDispatcher*,
										   const std::string& key,
										   const LLUUID&,
										   const sparam_t& strings)
{
	LLPanelEstateAccess* panel = LLFloaterRegionInfo::getPanelAccess();
	if (!panel) return true;	// We are since gone !

	if (gAgent.hasRegionCapability("EstateAccess"))
	{
		if (panel->getPendingUpdate())
		{
			panel->setPendingUpdate(false);
			panel->updateLists();
		}
		return true;
	}

	// Old, non-capability based code, kept for OpenSIM compatibility

	S32 index = 1;	// skip estate_id
	U32 access_flags = strtoul(strings[index++].c_str(), NULL, 10);
	S32 num_allowed_agents = strtol(strings[index++].c_str(), NULL, 10);
	S32 num_allowed_groups = strtol(strings[index++].c_str(), NULL, 10);
	S32 num_banned_agents = strtol(strings[index++].c_str(), NULL, 10);
	S32 num_estate_managers = strtol(strings[index++].c_str(), NULL, 10);

	// sanity ckecks
	if (num_allowed_agents > 0 &&
		!(access_flags & ESTATE_ACCESS_ALLOWED_AGENTS))
	{
		llwarns << "non-zero count for allowed agents, but no corresponding flag"
				<< llendl;
	}
	if (num_allowed_groups > 0 &&
		!(access_flags & ESTATE_ACCESS_ALLOWED_GROUPS))
	{
		llwarns << "non-zero count for allowed groups, but no corresponding flag"
				<< llendl;
	}
	if (num_banned_agents > 0 && !(access_flags & ESTATE_ACCESS_BANNED_AGENTS))
	{
		llwarns << "non-zero count for banned agents, but no corresponding flag"
				<< llendl;
	}
	if (num_estate_managers > 0 && !(access_flags & ESTATE_ACCESS_MANAGERS))
	{
		llwarns << "non-zero count for managers, but no corresponding flag"
				<< llendl;
	}

	LLNameListCtrl* name_list;
	// Grab the UUIDs out of the string fields
	if (access_flags & ESTATE_ACCESS_ALLOWED_AGENTS)
	{
		name_list = panel->mAllowedAvatars;

		S32 total = num_allowed_agents;

		if (name_list)
		{
			total += name_list->getItemCount();
		}

		std::string msg = llformat("Allowed residents: (%d, max %d)", total,
								   ESTATE_MAX_ACCESS_IDS);
		panel->childSetValue("allow_resident_label", LLSD(msg));

		if (name_list)
		{
#if 0
			name_list->deleteAllItems();
#endif
			for (S32 i = 0;
				 i < num_allowed_agents && i < ESTATE_MAX_ACCESS_IDS; ++i)
			{
				LLUUID id;
				memcpy(id.mData, strings[index++].data(), UUID_BYTES);
				name_list->addNameItem(id);
			}
			panel->childSetEnabled("remove_allowed_avatar_btn",
								   name_list->getFirstSelected() != NULL);
			name_list->sortByName(true);
		}
	}

	if (access_flags & ESTATE_ACCESS_ALLOWED_GROUPS)
	{
		name_list = panel->mAllowedGroups;

		std::string msg = llformat("Allowed groups: (%d, max %d)",
									num_allowed_groups,
									(S32)ESTATE_MAX_GROUP_IDS);
		panel->childSetValue("allow_group_label", LLSD(msg));

		if (name_list)
		{
			name_list->deleteAllItems();
			for (S32 i = 0; i < num_allowed_groups && i < ESTATE_MAX_GROUP_IDS;
				 ++i)
			{
				LLUUID id;
				memcpy(id.mData, strings[index++].data(), UUID_BYTES);
				name_list->addGroupNameItem(id);
			}
			panel->childSetEnabled("remove_allowed_group_btn",
								   name_list->getFirstSelected() != NULL);
			name_list->sortByName(true);
		}
	}

	if (access_flags & ESTATE_ACCESS_BANNED_AGENTS)
	{
		name_list = panel->mBannedAvatars;

		S32 total = num_banned_agents;

		if (name_list)
		{
			total += name_list->getItemCount();
		}

		std::string msg = llformat("Banned residents: (%d, max %d)", total,
								   ESTATE_MAX_BANNED_IDS);
		panel->childSetValue("ban_resident_label", LLSD(msg));

		if (name_list)
		{
#if 0
			name_list->deleteAllItems();
#endif
			std::string na = LLTrans::getString("na");
			for (S32 i = 0; i < num_banned_agents && i < ESTATE_MAX_BANNED_IDS;
				 ++i)
			{
				LLUUID id;
				memcpy(id.mData, strings[index++].data(), UUID_BYTES);

				LLSD item;
				item["id"] = id;

				LLSD& columns = item["columns"];

				columns[0]["column"] = "name"; // value is auto-populated

				columns[1]["column"] = "last_login_date";
				columns[1]["value"] = na;

				columns[2]["column"] = "ban_date";
				columns[2]["value"] = na;

				columns[3]["column"] = "bannedby";
				columns[3]["value"] = na;

				name_list->addElement(item);
			}
			panel->childSetEnabled("remove_banned_avatar_btn",
								   name_list->getFirstSelected() != NULL);
			name_list->sortByName(true);
		}
	}

	if (access_flags & ESTATE_ACCESS_MANAGERS)
	{
		std::string msg = llformat("Estate Managers: (%d, max %d)",
								   num_estate_managers,
								   ESTATE_MAX_MANAGERS);
		panel->childSetValue("estate_manager_label", LLSD(msg));

		name_list = panel->mEstateManagers;
		if (name_list)
		{
			// Clear existing entries
			name_list->deleteAllItems();

			// There should be only ESTATE_MAX_MANAGERS people in the list,
			// but if the database gets more (SL-46107) don't truncate the
			// list unless it's really big.  Go ahead and show the extras so
			// the user doesn't get confused, and they can still remove them.
			for (S32 i = 0;
				 i < num_estate_managers && i < ESTATE_MAX_MANAGERS * 4; ++i)
			{
				LLUUID id;
				memcpy(id.mData, strings[index++].data(), UUID_BYTES);
				name_list->addNameItem(id);
			}
			panel->childSetEnabled("remove_estate_manager_btn",
								   name_list->getFirstSelected() != NULL);
			name_list->sortByName(true);
		}
	}

	return true;
}

class LLEstateAccessChangeInfo
{
public:
	LLEstateAccessChangeInfo(const LLSD& sd)
	{
		mDialogName = sd["dialog_name"].asString();
		mOperationFlag = (U32)sd["operation"].asInteger();
		for (LLSD::array_const_iterator it = sd["allowed_ids"].beginArray(),
										end = sd["allowed_ids"].endArray();
			 it != end; ++it)
		{
			mAgentOrGroupIDs.emplace_back(it->asUUID());
		}
	}

	const LLSD asLLSD() const
	{
		LLSD sd;
		sd["name"] = mDialogName;
		sd["operation"] = (S32)mOperationFlag;
		for (U32 i = 0, count = mAgentOrGroupIDs.size(); i < count; ++i)
		{
			sd["allowed_ids"].append(mAgentOrGroupIDs[i]);
		}
		return sd;
	}

public:
	// ESTATE_ACCESS_BANNED_AGENT_ADD, _REMOVE, etc.
	U32							mOperationFlag;

	std::string					mDialogName;

	// List of agent IDs to apply to this change
	uuid_vec_t					mAgentOrGroupIDs;
};

class LLDispatchSetEstateExperience final : public LLDispatchHandler
{
public:
	LLDispatchSetEstateExperience() = default;

	bool operator()(const LLDispatcher*, const std::string& key,
					const LLUUID& invoice, const sparam_t& strings) override;

	static LLSD getIDs(sparam_t::const_iterator it,
					   sparam_t::const_iterator end, S32 count);
};

//static
LLSD LLDispatchSetEstateExperience::getIDs(sparam_t::const_iterator it,
										   sparam_t::const_iterator end,
										   S32 count)
{
	LLSD ids = LLSD::emptyArray();
	LLUUID id;
	while (count-- > 0 && it < end)
	{
		memcpy(id.mData, (*(it++)).data(), UUID_BYTES);
		ids.append(id);
	}
	return ids;
}

// key = "setexperience"
// strings[0] = str(estate_id)
// strings[1] = str(send_to_agent_only)
// strings[2] = str(num blocked)
// strings[3] = str(num trusted)
// strings[4] = str(num allowed)
// strings[5] = bin(uuid) ...
bool LLDispatchSetEstateExperience::operator()(const LLDispatcher*,
											   const std::string& key,
											   const LLUUID&,
											   const sparam_t& strings)
{
	LLPanelRegionExperiences* panelp =
		LLFloaterRegionInfo::getPanelExperiences();
	if (!panelp)
	{
		return true;
	}

	constexpr size_t MIN_SIZE = 5;
	if (strings.size() < MIN_SIZE)
	{
		return true;
	}

	// Skip 2 parameters
	sparam_t::const_iterator it = strings.begin();
	++it; // U32 estate_id = strtol((*it).c_str(), NULL, 10);
	++it; // U32 send_to_agent_only = strtoul((*(++it)).c_str(), NULL, 10);

	// Read 3 parameters
	S32 blocked = strtol((*(it++)).c_str(), NULL, 10);
	S32 trusted = strtol((*(it++)).c_str(), NULL, 10);
	S32 allowed = strtol((*(it++)).c_str(), NULL, 10);

	LLSD ids = LLSD::emptyMap()
		.with("blocked", getIDs(it, strings.end(),  blocked))
		.with("trusted", getIDs(it + blocked, strings.end(), trusted))
		.with("allowed", getIDs(it + blocked + trusted, strings.end(),
			  allowed));

	panelp->processResponse(ids);

	return true;
}

///////////////////////////////////////////////////////////////////////////////
// LLFloaterRegionInfo
///////////////////////////////////////////////////////////////////////////////

LLUUID LLFloaterRegionInfo::sRequestInvoice;
S32 LLFloaterRegionInfo::sLastTab = 0;

LLFloaterRegionInfo::LLFloaterRegionInfo(const LLSD& seed)
:	mPanelEnvironment(NULL)
{
	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_region_info.xml",
												 NULL, false);
}

bool LLFloaterRegionInfo::postBuild()
{
	mTabs = getChild<LLTabContainer>("region_panels");

	// Construct the panels
	LLPanelRegionInfo* panel = new LLPanelRegionGeneralInfo;
	mInfoPanels.push_back(panel);
	LLUICtrlFactory::getInstance()->buildPanel(panel,
											   "panel_region_general.xml");
	mTabs->addTabPanel(panel, panel->getLabel(), true);

	panel = new LLPanelRegionDebugInfo;
	mInfoPanels.push_back(panel);
	LLUICtrlFactory::getInstance()->buildPanel(panel,
											   "panel_region_debug.xml");
	mTabs->addTabPanel(panel, panel->getLabel());

	panel = new LLPanelRegionTextureInfo;
	mInfoPanels.push_back(panel);
	LLUICtrlFactory::getInstance()->buildPanel(panel,
											   "panel_region_texture.xml");
	mTabs->addTabPanel(panel, panel->getLabel());

	panel = new LLPanelRegionTerrainInfo;
	mInfoPanels.push_back(panel);
	LLUICtrlFactory::getInstance()->buildPanel(panel,
											   "panel_region_terrain.xml");
	mTabs->addTabPanel(panel, panel->getLabel());

	panel = new LLPanelEstateInfo;
	mInfoPanels.push_back(panel);
	LLUICtrlFactory::getInstance()->buildPanel(panel,
											   "panel_region_estate.xml");
	mTabs->addTabPanel(panel, panel->getLabel());

	panel = new LLPanelEstateAccess;
	mInfoPanels.push_back(panel);
	LLUICtrlFactory::getInstance()->buildPanel(panel,
											   "panel_region_access.xml");
	mTabs->addTabPanel(panel, panel->getLabel());

	panel = new LLPanelEstateCovenant;
	mInfoPanels.push_back(panel);
	LLUICtrlFactory::getInstance()->buildPanel(panel,
											   "panel_region_covenant.xml");
	mTabs->addTabPanel(panel, panel->getLabel());

	if (gAgent.hasRegionCapability("RegionExperiences"))
	{
		panel = new LLPanelRegionExperiences;
		mInfoPanels.push_back(panel);
		mTabs->addTabPanel(panel, panel->getLabel());
	}

	// Add the environment tab if needed
	if (gAgent.hasInventorySettings())
	{
		LLViewerRegion* regionp = gAgent.getRegion();
		mPanelEnvironment =
			new HBPanelLandEnvironment(regionp ? regionp->getHandle() : 0);
		mTabs->addTabPanel(mPanelEnvironment, mPanelEnvironment->getLabel());
	}

	gMessageSystemp->setHandlerFunc(_PREHASH_EstateOwnerMessage,
									&processEstateOwnerRequest);

	if (sLastTab < mTabs->getTabCount())
	{
		mTabs->selectTab(sLastTab);
	}
	else
	{
		sLastTab = 0;
	}

	return true;
}

LLFloaterRegionInfo::~LLFloaterRegionInfo()
{
	sLastTab = mTabs->getCurrentPanelIndex();
}

void LLFloaterRegionInfo::onOpen()
{
	LLRect rect = gSavedSettings.getRect("FloaterRegionInfoRect");
	S32 left, top;
	gFloaterViewp->getNewFloaterPosition(&left, &top);
	rect.translate(left, top);

	LLViewerRegion* regionp = gAgent.getRegion();
	if (regionp)
	{
		refreshFromRegion(gAgent.getRegion());
		requestRegionInfo();
	}
	LLFloater::onOpen();
}

//static
void LLFloaterRegionInfo::requestRegionInfo()
{
	LLFloaterRegionInfo* self = findInstance();
	if (!self) return;

	// Disable all but Covenant panels
	LLPanel* panel = (LLPanel*)getPanelGeneral();
	if (panel)
	{
		panel->setCtrlsEnabled(false);
	}
	panel = (LLPanel*)getPanelDebug();
	if (panel)
	{
		panel->setCtrlsEnabled(false);
	}
	panel = (LLPanel*)getPanelTerrain();
	if (panel)
	{
		panel->setCtrlsEnabled(false);
	}
	panel = (LLPanel*)getPanelEstate();
	if (panel)
	{
		panel->setCtrlsEnabled(false);
	}
	panel = (LLPanel*)getPanelAccess();
	if (panel)
	{
		panel->setCtrlsEnabled(false);
	}
	panel = (LLPanel*)getPanelExperiences();
	if (panel)
	{
		panel->setCtrlsEnabled(false);
	}
	if (self->mPanelEnvironment)
	{
		self->mPanelEnvironment->setEnabled(false);
	}

	// Must allow anyone to request the RegionInfo data so non-owners/non-gods
	// can see the values. We therefore cannot use an EstateOwnerMessage. JC
	LLMessageSystem* msg = gMessageSystemp;
	if (msg)	// Paranoia
	{
		msg->newMessage(_PREHASH_RequestRegionInfo);
		msg->nextBlock(_PREHASH_AgentData);
		msg->addUUID(_PREHASH_AgentID, gAgentID);
		msg->addUUID(_PREHASH_SessionID, gAgentSessionID);
		gAgent.sendReliableMessage();
	}
}

//static
void LLFloaterRegionInfo::processEstateOwnerRequest(LLMessageSystem* msg,
													void**)
{
	static LLDispatcher dispatch;

	if (!findInstance()) return;

	if (!gEstateDispatchInitialized)
	{
		LLPanelEstateInfo::initDispatch(dispatch);
	}

	// Unpack the message
	std::string request;
	LLUUID invoice;
	LLDispatcher::sparam_t strings;
	LLDispatcher::unpackMessage(msg, request, invoice, strings);
	if (invoice != getLastInvoice())
	{
		LL_DEBUGS("RegionInfo") << "Mismatched estate message: " << request
								<< " - Invoice: " << invoice << LL_ENDL;
		return;
	}

	// Dispatch the message
	dispatch.dispatch(request, invoice, strings);

	LLPanelEstateInfo* panel = getPanelEstate();
	if (panel)
	{
		panel->updateControls(gAgent.getRegion());
	}
}

//static
void LLFloaterRegionInfo::updateFromRegionInfo()
{
	LLFloaterRegionInfo* self = findInstance();
	if (!self) return;

	LLViewerRegion* region = gAgent.getRegion();
	if (!region) return;

	bool allow_modify = gAgent.isGodlike() || region->canManageEstate();
	U64 region_flags = LLRegionInfoModel::sRegionFlags;

	// GENERAL PANEL
	LLPanel* panel = (LLPanel*)getPanelGeneral();
	if (panel)
	{
		panel->childSetValue("region_text", LLSD(LLRegionInfoModel::sSimName));
		panel->childSetValue("region_type", LLSD(LLRegionInfoModel::sSimType));
		panel->childSetValue("version_channel_text", gLastVersionChannel);

		panel->childSetValue("block_terraform_check",
							 (region_flags &
							  REGION_FLAGS_BLOCK_TERRAFORM) != 0);
		panel->childSetValue("block_fly_check",
							 (region_flags & REGION_FLAGS_BLOCK_FLY) != 0);
		panel->childSetValue("block_fly_over_check",
							 (region_flags & REGION_FLAGS_BLOCK_FLYOVER) != 0);
		panel->childSetValue("allow_damage_check",
							 (region_flags & REGION_FLAGS_ALLOW_DAMAGE) != 0);
		panel->childSetValue("restrict_pushobject",
							 (region_flags &
							  REGION_FLAGS_RESTRICT_PUSHOBJECT) != 0);
		panel->childSetValue("allow_land_resell_check",
							 (region_flags &
							  REGION_FLAGS_BLOCK_LAND_RESELL) == 0);
		panel->childSetValue("allow_parcel_changes_check",
							 (region_flags &
							  REGION_FLAGS_ALLOW_PARCEL_CHANGES) != 0);
		panel->childSetValue("block_parcel_search_check",
							 (region_flags &
							  REGION_FLAGS_BLOCK_PARCEL_SEARCH) != 0);

		LLSpinCtrl* spinctrl = panel->getChild<LLSpinCtrl>("agent_limit_spin");
		spinctrl->setMaxValue(LLRegionInfoModel::sHardAgentLimit);
		spinctrl->setValue(LLSD((F32)LLRegionInfoModel::sAgentLimit));

		panel->childSetValue("object_bonus_spin",
							 LLSD(LLRegionInfoModel::sObjectBonusFactor));
		panel->childSetValue("access_combo",
							 LLSD(LLRegionInfoModel::sSimAccess));

	 	// Detect teen grid for maturity
		// *TODO add field to estate table and test that
		bool teen_grid = LLRegionInfoModel::sParentEstateID == 5;
		panel->childSetEnabled("access_combo",
							   gAgent.isGodlike() ||
							   (!teen_grid &&
								region && region->canManageEstate()));
		panel->setCtrlsEnabled(allow_modify);
	}

	// DEBUG PANEL
	panel = (LLPanel*)getPanelDebug();
	if (panel)
	{
		panel->childSetValue("region_text", LLSD(LLRegionInfoModel::sSimName));
		panel->childSetValue("disable_scripts_check",
							 LLSD((region_flags &
								   REGION_FLAGS_SKIP_SCRIPTS) != 0));
		panel->childSetValue("disable_collisions_check",
							 LLSD((region_flags &
								   REGION_FLAGS_SKIP_COLLISIONS) != 0));
		panel->childSetValue("disable_physics_check",
							 LLSD((region_flags &
								   REGION_FLAGS_SKIP_PHYSICS) != 0));
		panel->setCtrlsEnabled(allow_modify);
	}

	// TERRAIN PANEL
	panel = (LLPanel*)getPanelTerrain();
	if (panel)
	{
		panel->childSetValue("region_text", LLSD(LLRegionInfoModel::sSimName));
		panel->childSetValue("water_height_spin",
							 LLSD(LLRegionInfoModel::sWaterHeight));
		panel->childSetValue("terrain_raise_spin",
							 LLSD(LLRegionInfoModel::sTerrainRaiseLimit));
		panel->childSetValue("terrain_lower_spin",
							 LLSD(LLRegionInfoModel::sTerrainLowerLimit));
		panel->childSetValue("use_estate_sun_check",
							 LLSD(LLRegionInfoModel::sUseEstateSun));

		panel->childSetValue("fixed_sun_check",
							 LLSD((region_flags &
								   REGION_FLAGS_SUN_FIXED) != 0));
		panel->childSetEnabled("fixed_sun_check",
							   allow_modify &&
							   !LLRegionInfoModel::sUseEstateSun);
		panel->childSetValue("sun_hour_slider",
							 LLSD(LLRegionInfoModel::sSunHour));
		panel->childSetEnabled("sun_hour_slider",
							   allow_modify &&
							   !LLRegionInfoModel::sUseEstateSun);
		panel->setCtrlsEnabled(allow_modify);
	}

	self->refreshFromRegion(region);
}

//static
LLPanelRegionGeneralInfo* LLFloaterRegionInfo::getPanelGeneral()
{
	LLFloaterRegionInfo* self = LLFloaterRegionInfo::findInstance();
	if (self)
	{
		LLPanel* panel = self->mTabs->getChild<LLPanel>("General",
														true, false);
		return (LLPanelRegionGeneralInfo*)panel;
	}
	return NULL;
}

//static
LLPanelRegionDebugInfo* LLFloaterRegionInfo::getPanelDebug()
{
	LLFloaterRegionInfo* self = LLFloaterRegionInfo::findInstance();
	if (self)
	{
		LLPanel* panel = self->mTabs->getChild<LLPanel>("Debug",
														true, false);
		return (LLPanelRegionDebugInfo*)panel;
	}
	return NULL;
}

//static
LLPanelEstateInfo* LLFloaterRegionInfo::getPanelEstate()
{
	LLFloaterRegionInfo* self = LLFloaterRegionInfo::findInstance();
	if (self)
	{
		LLPanel* panel = self->mTabs->getChild<LLPanel>("Estate",
														true, false);
		return (LLPanelEstateInfo*)panel;
	}
	return NULL;
}

//static
LLPanelEstateAccess* LLFloaterRegionInfo::getPanelAccess()
{
	LLFloaterRegionInfo* self = LLFloaterRegionInfo::findInstance();
	if (self)
	{
		LLPanel* panel = self->mTabs->getChild<LLPanel>("Access",
														true, false);
		return (LLPanelEstateAccess*)panel;
	}
	return NULL;
}

//static
LLPanelEstateCovenant* LLFloaterRegionInfo::getPanelCovenant()
{
	LLFloaterRegionInfo* self = LLFloaterRegionInfo::findInstance();
	if (self)
	{
		LLPanel* panel = self->mTabs->getChild<LLPanel>("Covenant",
														true, false);
		return (LLPanelEstateCovenant*)panel;
	}
	return NULL;
}

//static
LLPanelRegionTerrainInfo* LLFloaterRegionInfo::getPanelTerrain()
{
	LLFloaterRegionInfo* self = LLFloaterRegionInfo::findInstance();
	if (self)
	{
		LLPanel* panel = self->mTabs->getChild<LLPanel>("Terrain",
														true, false);
		return (LLPanelRegionTerrainInfo*)panel;
	}
	return NULL;
}

//static
LLPanelRegionExperiences* LLFloaterRegionInfo::getPanelExperiences()
{
	LLFloaterRegionInfo* self = LLFloaterRegionInfo::findInstance();
	if (self)
	{
		LLPanel* panel = self->mTabs->getChild<LLPanel>("Experiences",
														true, false);
		return (LLPanelRegionExperiences*)panel;
	}
	return NULL;
}

//static
HBPanelLandEnvironment* LLFloaterRegionInfo::getPanelEnvironment()
{
	LLFloaterRegionInfo* self = LLFloaterRegionInfo::findInstance();
	return self ? self->mPanelEnvironment : NULL;
}

void LLFloaterRegionInfo::refreshFromRegion(LLViewerRegion* regionp)
{
	if (!regionp) return;

	// Call refresh from region on all panels
	for (S32 i = 0, count = mInfoPanels.size(); i < count; ++i)
	{
		LLPanelRegionInfo* panelp = mInfoPanels[i];
		panelp->refreshFromRegion(regionp);
	}

	if (mPanelEnvironment)
	{
		mPanelEnvironment->setRegionHandle(regionp->getHandle());
	}
}

void LLFloaterRegionInfo::refresh()
{
	for (info_panels_t::iterator iter = mInfoPanels.begin();
		 iter != mInfoPanels.end(); ++iter)
	{
		(*iter)->refresh();
	}
	if (mPanelEnvironment)
	{
		mPanelEnvironment->refresh();
	}
}

///////////////////////////////////////////////////////////////////////////////
// LLPanelRegionInfo
///////////////////////////////////////////////////////////////////////////////

LLPanelRegionInfo::LLPanelRegionInfo()
:	LLPanel(),
	mApplyBtn(NULL)
{
}

//virtual
bool LLPanelRegionInfo::postBuild()
{
	mApplyBtn = getChild<LLButton>("apply_btn", true, false);
	if (mApplyBtn)
	{
		mApplyBtn->setClickedCallback(onBtnSet, this);
		mApplyBtn->setEnabled(false);
	}

	refresh();

	return true;
}

//virtual
bool LLPanelRegionInfo::refreshFromRegion(LLViewerRegion* region)
{
	if (region)
	{
		mHost = region->getHost();
	}
	return true;
}

void LLPanelRegionInfo::enableApplyBtn(bool enable)
{
	if (mApplyBtn)
	{
		mApplyBtn->setEnabled(enable);
	}
}

void LLPanelRegionInfo::disableApplyBtn()
{
	if (mApplyBtn)
	{
		mApplyBtn->setEnabled(false);
	}
}

void LLPanelRegionInfo::initCtrl(const char* name)
{
	childSetCommitCallback(name, onChangeAnything, this);
}

void LLPanelRegionInfo::initHelpBtn(const char* name,
									const std::string& xml_alert)
{
	childSetAction(name, onClickHelp, new std::string(xml_alert));
}

void LLPanelRegionInfo::sendEstateOwnerMessage(const std::string& request,
											   const strings_t& strings)
{
	LLMessageSystem* msg = gMessageSystemp;
	if (!msg) return;	// Paranoia

	llinfos << "Sending estate request '" << request << "' - Invoice: "
			<< LLFloaterRegionInfo::getLastInvoice() << llendl;
	msg->newMessage(_PREHASH_EstateOwnerMessage);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->addUUIDFast(_PREHASH_TransactionID, LLUUID::null); // Not used
	msg->nextBlock(_PREHASH_MethodData);
	msg->addString(_PREHASH_Method, request);
	msg->addUUID(_PREHASH_Invoice, LLFloaterRegionInfo::getLastInvoice());
	if (strings.empty())
	{
		msg->nextBlock(_PREHASH_ParamList);
		msg->addString(_PREHASH_Parameter, NULL);
	}
	else
	{
		strings_t::const_iterator it = strings.begin();
		strings_t::const_iterator end = strings.end();
		for ( ; it != end; ++it)
		{
			msg->nextBlock(_PREHASH_ParamList);
			msg->addString(_PREHASH_Parameter, *it);
		}
	}
	msg->sendReliable(mHost);
}

//static
void LLPanelRegionInfo::onBtnSet(void* user_data)
{
	LLPanelRegionInfo* panel = (LLPanelRegionInfo*)user_data;
	if (panel && panel->sendUpdate())
	{
		panel->disableApplyBtn();
	}
}

// Enables the "Apply" button if it is not already enabled
//static
void LLPanelRegionInfo::onChangeAnything(LLUICtrl* ctrl, void* user_data)
{
	LLPanelRegionInfo* panel = (LLPanelRegionInfo*)user_data;
	if (panel)
	{
		panel->enableApplyBtn();
		panel->refresh();
	}
}

//static
void LLPanelRegionInfo::onClickHelp(void* data)
{
	std::string* xml_alert = (std::string*)data;
	gNotifications.add(*xml_alert);
}

///////////////////////////////////////////////////////////////////////////////
// LLPanelRegionGeneralInfo
///////////////////////////////////////////////////////////////////////////////

//virtual
bool LLPanelRegionGeneralInfo::refreshFromRegion(LLViewerRegion* region)
{
	bool allow_modify = gAgent.isGodlike() ||
						(region && region->canManageEstate());
	setCtrlsEnabled(allow_modify);
	disableApplyBtn();
	childSetEnabled("access_text", allow_modify);
	// childSetEnabled("access_combo", allow_modify);
	// now set in processRegionInfo for teen grid detection
	childSetEnabled("kick_btn", allow_modify);
	childSetEnabled("kick_all_btn", allow_modify);
	childSetEnabled("im_btn", allow_modify);
	childSetEnabled("manage_telehub_btn", allow_modify);

	// Data gets filled in by processRegionInfo

	// Call the parent for common book-keeping
	return LLPanelRegionInfo::refreshFromRegion(region);
}

bool LLPanelRegionGeneralInfo::postBuild()
{
	// Enable the "Apply" button if something is changed. JC
	initCtrl("block_terraform_check");
	initCtrl("block_fly_check");
	initCtrl("block_fly_over_check");
	initCtrl("allow_damage_check");
	initCtrl("allow_land_resell_check");
	initCtrl("allow_parcel_changes_check");
	initCtrl("agent_limit_spin");
	initCtrl("object_bonus_spin");
	initCtrl("access_combo");
	initCtrl("restrict_pushobject");
	initCtrl("block_parcel_search_check");

	initHelpBtn("terraform_help",			"HelpRegionBlockTerraform");
	initHelpBtn("fly_help",					"HelpRegionBlockFly");
	initHelpBtn("damage_help",				"HelpRegionAllowDamage");
	initHelpBtn("agent_limit_help",			"HelpRegionAgentLimit");
	initHelpBtn("object_bonus_help",		"HelpRegionObjectBonus");
	initHelpBtn("access_help",				"HelpRegionMaturity");
	initHelpBtn("restrict_pushobject_help",	"HelpRegionRestrictPushObject");
	initHelpBtn("land_resell_help",			"HelpRegionLandResell");
	initHelpBtn("parcel_changes_help",		"HelpParcelChanges");
	initHelpBtn("parcel_search_help",		"HelpRegionSearch");

	childSetAction("kick_btn", onClickKick, this);
	childSetAction("kick_all_btn", onClickKickAll, this);
	childSetAction("im_btn", onClickMessage, this);
	childSetAction("manage_telehub_btn", onClickManageTelehub, this);

	return LLPanelRegionInfo::postBuild();
}

//static
void LLPanelRegionGeneralInfo::onClickKick(void* userdata)
{
	LLPanelRegionGeneralInfo* panelp = (LLPanelRegionGeneralInfo*)userdata;
	if (!panelp) return;

	LLFloater* childp = LLFloaterAvatarPicker::show(onKickCommit, userdata,
													false, true);
	if (childp && gFloaterViewp)
	{
		// This depends on the grandparent view being a floater in order to set
		// up floater dependency
		LLFloater* parentp = gFloaterViewp->getParentFloater(panelp);
		if (parentp)
		{
			parentp->addDependentFloater(childp);
		}
	}
}

//static
void LLPanelRegionGeneralInfo::onKickCommit(const std::vector<std::string>& names,
											const uuid_vec_t& ids,
											void* userdata)
{
	LLPanelRegionGeneralInfo* self = (LLPanelRegionGeneralInfo*)userdata;
	if (self && !names.empty() && !ids.empty() && ids[0].notNull())
	{
		strings_t strings;
		// [0] = our agent id
		// [1] = target agent id
		std::string buffer;
		gAgentID.toString(buffer);
		strings.push_back(buffer);

		ids[0].toString(buffer);
		strings.push_back(strings_t::value_type(buffer));

		self->sendEstateOwnerMessage("teleporthomeuser", strings);
	}
}

//static
void LLPanelRegionGeneralInfo::onClickKickAll(void* userdata)
{
	gNotifications.add("KickUsersFromRegion", LLSD(), LLSD(),
					   boost::bind(&LLPanelRegionGeneralInfo::onKickAllCommit,
								   (LLPanelRegionGeneralInfo*)userdata, _1,
								   _2));
}

bool LLPanelRegionGeneralInfo::onKickAllCommit(const LLSD& notification,
											   const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		strings_t strings;
		// [0] = our agent id
		std::string buffer;
		gAgentID.toString(buffer);
		strings.push_back(buffer);

		// Historical message name
		sendEstateOwnerMessage("teleporthomeallusers", strings);
	}
	return false;
}

//static
void LLPanelRegionGeneralInfo::onClickMessage(void* userdata)
{
	gNotifications.add("MessageRegion", LLSD(), LLSD(),
					   boost::bind(&LLPanelRegionGeneralInfo::onMessageCommit,
								   (LLPanelRegionGeneralInfo*)userdata, _1,
								   _2));
}

//static
bool LLPanelRegionGeneralInfo::onMessageCommit(const LLSD& notification,
											   const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) != 0)
	{
		return false;
	}

	std::string text = response["message"].asString();
	if (text.empty()) return false;

	llinfos << "Message to everyone: " << text << llendl;
	strings_t strings;
	// [0] grid_x, unused here
	// [1] grid_y, unused here
	// [2] agent_id of sender
	// [3] sender name
	// [4] message
	strings.emplace_back("-1");
	strings.emplace_back("-1");
	std::string buffer;
	gAgentID.toString(buffer);
	strings.emplace_back(buffer);
	std::string name;
	gAgent.buildFullname(name);
	strings.emplace_back(name);
	strings.emplace_back(text);
	sendEstateOwnerMessage("simulatormessage", strings);
	return false;
}

//static
void LLPanelRegionGeneralInfo::onClickManageTelehub(void* data)
{
	LLFloaterTelehub::showInstance();
	LLFloaterRegionInfo::getInstance()->close();
}

#define YESORNO(C) llformat("%s", childGetValue(C).asBoolean() ? "Y" : "N")
#define ASFLOAT(C) llformat("%f", childGetValue(C).asReal())
#define ASINTEGER(C) llformat("%d", childGetValue(C).asInteger())

// setregioninfo
// strings[0] = 'Y' - block terraform, 'N' - not
// strings[1] = 'Y' - block fly, 'N' - not
// strings[2] = 'Y' - allow damage, 'N' - not
// strings[3] = 'Y' - allow land sale, 'N' - not
// strings[4] = agent limit
// strings[5] = object bonus
// strings[6] = sim access (0 = unknown, 13 = PG, 21 = Mature, 42 = Adult)
// strings[7] = restrict pushobject
// strings[8] = 'Y' - allow parcel subdivide, 'N' - not
// strings[9] = 'Y' - block parcel search, 'N' - allow
bool LLPanelRegionGeneralInfo::sendUpdate()
{
	// First try using a Cap. If that fails use the old method.
	LLSD body;
	std::string url = gAgent.getRegionCapability("DispatchRegionInfo");
	if (!url.empty())
	{
		body["block_terraform"] = childGetValue("block_terraform_check");
		body["block_fly"] = childGetValue("block_fly_check");
		body["block_fly_over"] = childGetValue("block_fly_over_check");
		body["allow_damage"] = childGetValue("allow_damage_check");
		body["allow_land_resell"] = childGetValue("allow_land_resell_check");
		body["agent_limit"] = childGetValue("agent_limit_spin");
		body["prim_bonus"] = childGetValue("object_bonus_spin");
		body["sim_access"] = childGetValue("access_combo");
		body["restrict_pushobject"] = childGetValue("restrict_pushobject");
		body["allow_parcel_changes"] = childGetValue("allow_parcel_changes_check");
		body["block_parcel_search"] = childGetValue("block_parcel_search_check");

		LLCoreHttpUtil::HttpCoroutineAdapter::messageHttpPost(url, body,
															  "Region info update posted.",
															  "Failure to post region info update.");
	}
	else
	{
		strings_t strings;
		strings.emplace_back(YESORNO("block_terraform_check"));
		strings.emplace_back(YESORNO("block_fly_check"));
		strings.emplace_back(YESORNO("allow_damage_check"));
		strings.emplace_back(YESORNO("allow_land_resell_check"));
		strings.emplace_back(ASFLOAT("agent_limit_spin"));
		strings.emplace_back(ASFLOAT("object_bonus_spin"));
		strings.emplace_back(ASINTEGER("access_combo"));
		strings.emplace_back(YESORNO("restrict_pushobject"));
		strings.emplace_back(YESORNO("allow_parcel_changes_check"));

		sendEstateOwnerMessage("setregioninfo", strings);
	}

	// If we changed access levels, tell user about it
	LLViewerRegion* region = gAgent.getRegion();
	if (region &&
		childGetValue("access_combo").asInteger() != region->getSimAccess())
	{
		gNotifications.add("RegionMaturityChange");
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////
// LLPanelRegionDebugInfo
///////////////////////////////////////////////////////////////////////////////

bool LLPanelRegionDebugInfo::postBuild()
{
	LLPanelRegionInfo::postBuild();
	initCtrl("disable_scripts_check");
	initCtrl("disable_collisions_check");
	initCtrl("disable_physics_check");

	initHelpBtn("disable_scripts_help", "HelpRegionDisableScripts");
	initHelpBtn("disable_collisions_help", "HelpRegionDisableCollisions");
	initHelpBtn("disable_physics_help", "HelpRegionDisablePhysics");
	initHelpBtn("top_colliders_help", "HelpRegionTopColliders");
	initHelpBtn("top_scripts_help", "HelpRegionTopScripts");
	initHelpBtn("restart_help", "HelpRegionRestart");

	childSetAction("choose_avatar_btn", onClickChooseAvatar, this);
	childSetAction("return_btn", onClickReturn, this);
	childSetAction("top_colliders_btn", onClickTopColliders, this);
	childSetAction("top_scripts_btn", onClickTopScripts, this);
	childSetAction("restart_btn", onClickRestart, this);
	childSetAction("cancel_restart_btn", onClickCancelRestart, this);

	return true;
}

//virtual
bool LLPanelRegionDebugInfo::refreshFromRegion(LLViewerRegion* region)
{
	bool allow_modify = gAgent.isGodlike() ||
						(region && region->canManageEstate());
	bool got_target_avatar = mTargetAvatar.notNull();

	setCtrlsEnabled(allow_modify);
	disableApplyBtn();
	childDisable("target_avatar_name");

	childSetEnabled("choose_avatar_btn", allow_modify);
	childSetEnabled("return_scripts", allow_modify && got_target_avatar);
	childSetEnabled("return_other_land", allow_modify && got_target_avatar);
	childSetEnabled("return_estate_wide", allow_modify && got_target_avatar);
	childSetEnabled("return_btn", allow_modify && got_target_avatar);
	childSetEnabled("top_colliders_btn", allow_modify);
	childSetEnabled("top_scripts_btn", allow_modify);
	childSetEnabled("restart_btn", allow_modify);
	childSetEnabled("cancel_restart_btn", allow_modify);

	// Call the parent for common book-keeping
	return LLPanelRegionInfo::refreshFromRegion(region);
}

//virtual
bool LLPanelRegionDebugInfo::sendUpdate()
{
	strings_t strings;
	strings.emplace_back(YESORNO("disable_scripts_check"));
	strings.emplace_back(YESORNO("disable_collisions_check"));
	strings.emplace_back(YESORNO("disable_physics_check"));
	sendEstateOwnerMessage("setregiondebug", strings);
	return true;
}

void LLPanelRegionDebugInfo::onClickChooseAvatar(void* data)
{
	LLFloaterAvatarPicker::show(callbackAvatarID, data, false, true);
}

//static
void LLPanelRegionDebugInfo::callbackAvatarID(const std::vector<std::string>& names,
											  const uuid_vec_t& ids,
											  void* data)
{
	LLPanelRegionDebugInfo* self = (LLPanelRegionDebugInfo*)data;
	if (!self || ids.empty() || names.empty()) return;
	self->mTargetAvatar = ids[0];
	self->childSetValue("target_avatar_name", LLSD(names[0]));
	self->refreshFromRegion(gAgent.getRegion());
}

//static
void LLPanelRegionDebugInfo::onClickReturn(void* data)
{
	LLPanelRegionDebugInfo* panelp = (LLPanelRegionDebugInfo*)data;
	if (!panelp || panelp->mTargetAvatar.isNull()) return;

	LLSD args;
	args["USER_NAME"] = panelp->childGetValue("target_avatar_name").asString();
	LLSD payload;
	payload["avatar_id"] = panelp->mTargetAvatar;

	U32 flags = SWD_ALWAYS_RETURN_OBJECTS;

	if (panelp->childGetValue("return_scripts").asBoolean())
	{
		flags |= SWD_SCRIPTED_ONLY;
	}

	if (panelp->childGetValue("return_other_land").asBoolean())
	{
		flags |= SWD_OTHERS_LAND_ONLY;
	}
	payload["flags"] = LLSD::Integer(flags);
	payload["return_estate_wide"] =
		panelp->childGetValue("return_estate_wide").asBoolean();
	gNotifications.add("EstateObjectReturn", args, payload,
					   boost::bind(&LLPanelRegionDebugInfo::callbackReturn,
								   panelp, _1, _2));
}

bool LLPanelRegionDebugInfo::callbackReturn(const LLSD& notification,
											const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response))
	{
		return false;
	}

	LLUUID target_avatar = notification["payload"]["avatar_id"].asUUID();
	if (target_avatar.notNull())
	{
		U32 flags = notification["payload"]["flags"].asInteger();
		bool return_estate_wide = notification["payload"]["return_estate_wide"];
		if (return_estate_wide)
		{
			// Send as estate message - routed by spaceserver to all regions in
			// estate
			strings_t strings;
			strings.emplace_back(llformat("%d", flags));
			strings.emplace_back(target_avatar.asString());
			sendEstateOwnerMessage("estateobjectreturn", strings);
		}
		else
		{
			// Send to this simulator only
  			send_sim_wide_deletes(target_avatar, flags);
  		}
	}
	return false;
}

//static
void LLPanelRegionDebugInfo::onClickTopColliders(void* data)
{
	LLPanelRegionDebugInfo* self = (LLPanelRegionDebugInfo*)data;
	if (!self) return;

	strings_t strings;
	strings.emplace_back("1");	// one physics step
	LLFloaterTopObjects::showInstance();
	LLFloaterTopObjects::clearList();
	self->sendEstateOwnerMessage("colliders", strings);
}

//static
void LLPanelRegionDebugInfo::onClickTopScripts(void* data)
{
	LLPanelRegionDebugInfo* self = (LLPanelRegionDebugInfo*)data;
	if (!self) return;

	strings_t strings;
	strings.emplace_back("6");	// top 5 scripts
	LLFloaterTopObjects::showInstance();
	LLFloaterTopObjects::clearList();
	self->sendEstateOwnerMessage("scripts", strings);
}

//static
void LLPanelRegionDebugInfo::onClickRestart(void* data)
{
	gNotifications.add("ConfirmRestart", LLSD(), LLSD(),
					   boost::bind(&LLPanelRegionDebugInfo::callbackRestart,
								   (LLPanelRegionDebugInfo*)data, _1, _2));
}

bool LLPanelRegionDebugInfo::callbackRestart(const LLSD& notification,
											 const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		strings_t strings;
		strings.emplace_back("120");
		sendEstateOwnerMessage("restart", strings);
	}
	return false;
}

//static
void LLPanelRegionDebugInfo::onClickCancelRestart(void* data)
{
	LLPanelRegionDebugInfo* self = (LLPanelRegionDebugInfo*)data;
	if (!self) return;

	strings_t strings;
	strings.emplace_back("-1");
	self->sendEstateOwnerMessage("restart", strings);
}

///////////////////////////////////////////////////////////////////////////////
// LLPanelRegionTextureInfo
///////////////////////////////////////////////////////////////////////////////

LLPanelRegionTextureInfo::LLPanelRegionTextureInfo()
:	LLPanelRegionInfo()
{
}

bool LLPanelRegionTextureInfo::refreshFromRegion(LLViewerRegion* region)
{
	bool allow_modify = gAgent.isGodlike() ||
						(region && region->canManageEstate());
	setCtrlsEnabled(allow_modify);
	disableApplyBtn();

	if (region)
	{
		childSetValue("region_text", LLSD(region->getName()));
	}
	else
	{
		childSetValue("region_text", LLSD(""));
		return true;
	}

	LLVLComposition* compp = region->getComposition();
	LLTextureCtrl* texture_ctrl;
	std::string buffer;
	for (S32 i = 0; i < TERRAIN_TEXTURE_COUNT; ++i)
	{
		buffer = llformat("texture_detail_%d", i);
		texture_ctrl = getChild<LLTextureCtrl>(buffer.c_str(), true, false);
		if (texture_ctrl)
		{
			LL_DEBUGS("RegionTexture") << "Detail Texture " << i << ": "
									   << compp->getDetailTextureID(i)
									   << LL_ENDL;
			LLUUID tmp_id(compp->getDetailTextureID(i));
			texture_ctrl->setImageAssetID(tmp_id);
		}
	}

	for (S32 i = 0; i < CORNER_COUNT; ++i)
    {
		buffer = llformat("height_start_spin_%d", i);
		childSetValue(buffer.c_str(), LLSD(compp->getStartHeight(i)));
		buffer = llformat("height_range_spin_%d", i);
		childSetValue(buffer.c_str(), LLSD(compp->getHeightRange(i)));
	}

	// Call the parent for common book-keeping
	return LLPanelRegionInfo::refreshFromRegion(region);
}

bool LLPanelRegionTextureInfo::postBuild()
{
	LLPanelRegionInfo::postBuild();
	std::string buffer;
	for (S32 i = 0; i < TERRAIN_TEXTURE_COUNT; ++i)
	{
		buffer = llformat("texture_detail_%d", i);
		initCtrl(buffer.c_str());
	}

	for (S32 i = 0; i < CORNER_COUNT; ++i)
	{
		buffer = llformat("height_start_spin_%d", i);
		initCtrl(buffer.c_str());
		buffer = llformat("height_range_spin_%d", i);
		initCtrl(buffer.c_str());
	}

	return LLPanelRegionInfo::postBuild();
}

bool LLPanelRegionTextureInfo::sendUpdate()
{
	// Make sure user hasn't chosen wacky textures.
	if (!validateTextureSizes())
	{
		return false;
	}

	LLTextureCtrl* texture_ctrl;
	std::string buffer;
	std::string id_str;
	strings_t strings;

	for (S32 i = 0; i < TERRAIN_TEXTURE_COUNT; ++i)
	{
		buffer = llformat("texture_detail_%d", i);
		texture_ctrl = getChild<LLTextureCtrl>(buffer.c_str(), true, false);
		if (texture_ctrl)
		{
			LLUUID tmp_id(texture_ctrl->getImageAssetID());
			tmp_id.toString(id_str);
			buffer = llformat("%d %s", i, id_str.c_str());
			strings.emplace_back(buffer);
		}
	}
	sendEstateOwnerMessage("texturedetail", strings);
	strings.clear();
	for (S32 i = 0; i < CORNER_COUNT; ++i)
	{
		buffer = llformat("height_start_spin_%d", i);
		std::string buffer2 = llformat("height_range_spin_%d", i);
		std::string buffer3 =
			llformat("%d %f %f", i,
					 (F32)childGetValue(buffer.c_str()).asReal(),
					 (F32)childGetValue(buffer2.c_str()).asReal());
		strings.emplace_back(buffer3);
	}
	sendEstateOwnerMessage("textureheights", strings);
	strings.clear();
	sendEstateOwnerMessage("texturecommit", strings);
	return true;
}

bool LLPanelRegionTextureInfo::validateTextureSizes()
{
	for (S32 i = 0; i < TERRAIN_TEXTURE_COUNT; ++i)
	{
		std::string buffer;
		buffer = llformat("texture_detail_%d", i);
		LLTextureCtrl* texture_ctrl = getChild<LLTextureCtrl>(buffer.c_str(),
															  true, false);
		if (!texture_ctrl) continue;

		LLUUID image_asset_id = texture_ctrl->getImageAssetID();
		LLViewerTexture* img =
			LLViewerTextureManager::getFetchedTexture(image_asset_id);
		if (!img) return false;

		S32 components = img->getComponents();
		if (components != 3)
		{
			LLSD args;
			args["TEXTURE_NUM"] = i + 1;
			args["TEXTURE_BIT_DEPTH"] = llformat("%d", components * 8);
			gNotifications.add("InvalidTerrainBitDepth", args);
			return false;
		}

		// Must ask for highest resolution version's width. JC
		S32 width = img->getFullWidth();
		S32 height = img->getFullHeight();
		if (width > 1024 || height > 1024)
		{
			LLSD args;
			args["TEXTURE_NUM"] = i + 1;
			args["TEXTURE_SIZE_X"] = width;
			args["TEXTURE_SIZE_Y"] = height;
			gNotifications.add("InvalidTerrainSize", args);
			return false;
		}
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////
// LLPanelRegionTerrainInfo
///////////////////////////////////////////////////////////////////////////////

bool LLPanelRegionTerrainInfo::postBuild()
{
	LLPanelRegionInfo::postBuild();

	initHelpBtn("water_height_help", "HelpRegionWaterHeight");
	initHelpBtn("terrain_raise_help", "HelpRegionTerrainRaise");
	initHelpBtn("terrain_lower_help", "HelpRegionTerrainLower");
	initHelpBtn("upload_raw_help", "HelpRegionUploadRaw");
	initHelpBtn("download_raw_help", "HelpRegionDownloadRaw");
	initHelpBtn("use_estate_sun_help", "HelpRegionUseEstateSun");
	initHelpBtn("fixed_sun_help", "HelpRegionFixedSun");
	initHelpBtn("bake_terrain_help", "HelpRegionBakeTerrain");

	initCtrl("water_height_spin");
	initCtrl("terrain_raise_spin");
	initCtrl("terrain_lower_spin");

	initCtrl("fixed_sun_check");
	childSetCommitCallback("fixed_sun_check", onChangeFixedSun, this);
	childSetCommitCallback("use_estate_sun_check", onChangeUseEstateTime, this);
	childSetCommitCallback("sun_hour_slider", onChangeSunHour, this);

	childSetAction("download_raw_btn", onClickDownloadRaw, this);
	childSetAction("upload_raw_btn", onClickUploadRaw, this);
	childSetAction("bake_terrain_btn", onClickBakeTerrain, this);

	return true;
}

//virtual
bool LLPanelRegionTerrainInfo::refreshFromRegion(LLViewerRegion* region)
{
	bool owner_or_god = gAgent.isGodlike() ||
						(region && region->getOwner() == gAgentID);
	bool owner_or_god_or_manager = owner_or_god ||
								   (region && region->isEstateManager());
	setCtrlsEnabled(owner_or_god_or_manager);
	disableApplyBtn();

	childSetEnabled("download_raw_btn", owner_or_god);
	childSetEnabled("upload_raw_btn", owner_or_god);
	childSetEnabled("bake_terrain_btn", owner_or_god);

	// Call the parent for common book-keeping
	return LLPanelRegionInfo::refreshFromRegion(region);
}

//virtual
bool LLPanelRegionTerrainInfo::sendUpdate()
{
	strings_t strings;

	LLRegionInfoModel::sWaterHeight =
		childGetValue("water_height_spin").asReal();
	strings.emplace_back(llformat("%f", LLRegionInfoModel::sWaterHeight));

	LLRegionInfoModel::sTerrainRaiseLimit =
		childGetValue("terrain_raise_spin").asReal();
	strings.emplace_back(llformat("%f",
								  LLRegionInfoModel::sTerrainRaiseLimit));

	LLRegionInfoModel::sTerrainLowerLimit =
		childGetValue("terrain_lower_spin").asReal();
	strings.emplace_back(llformat("%f", LLRegionInfoModel::sTerrainLowerLimit));

	LLRegionInfoModel::sUseEstateSun =
		childGetValue("use_estate_sun_check").asBoolean();
	strings.emplace_back(llformat("%s",
								  LLRegionInfoModel::sUseEstateSun ? "Y"
																   : "N"));

	bool fixed_sun = childGetValue("fixed_sun_check").asBoolean();
	LLRegionInfoModel::setUseFixedSun(fixed_sun);
	strings.emplace_back(llformat("%s", fixed_sun ? "Y" : "N"));

	LLRegionInfoModel::sSunHour = childGetValue("sun_hour_slider").asReal();
	strings.emplace_back(llformat("%f", LLRegionInfoModel::sSunHour));

	// Grab estate information in case the user decided to set the region back
	// to estate time. JC
	LLPanelEstateInfo* panel = LLFloaterRegionInfo::getPanelEstate();
	if (!panel) return true;

	bool estate_global_time = panel->getGlobalTime();
	bool estate_fixed_sun = panel->getFixedSun();
	F32 estate_sun_hour;
	if (estate_global_time)
	{
		estate_sun_hour = 0.f;
	}
	else
	{
		estate_sun_hour = panel->getSunHour();
	}

	strings.emplace_back(llformat("%s", estate_global_time ? "Y" : "N"));
	strings.emplace_back(llformat("%s", estate_fixed_sun ? "Y" : "N"));
	strings.emplace_back(llformat("%f", estate_sun_hour));

	sendEstateOwnerMessage("setregionterrain", strings);
	return true;
}

//static
void LLPanelRegionTerrainInfo::onChangeUseEstateTime(LLUICtrl* ctrl,
													 void* user_data)
{
	LLPanelRegionTerrainInfo* panel = (LLPanelRegionTerrainInfo*)user_data;
	if (!panel) return;

	bool use_estate_sun = panel->childGetValue("use_estate_sun_check").asBoolean();
	panel->childSetEnabled("fixed_sun_check", !use_estate_sun);
	panel->childSetEnabled("sun_hour_slider", !use_estate_sun);
	if (use_estate_sun)
	{
		panel->childSetValue("fixed_sun_check", LLSD(false));
		panel->childSetValue("sun_hour_slider", LLSD(0.f));
	}
	panel->enableApplyBtn();
}

//static
void LLPanelRegionTerrainInfo::onChangeFixedSun(LLUICtrl* ctrl, void* data)
{
	LLPanelRegionTerrainInfo* panel = (LLPanelRegionTerrainInfo*)data;
	if (panel)
	{
		// Just enable the apply button. We let the sun-hour slider be enabled
		// for both fixed-sun and non-fixed-sun. JC
		panel->enableApplyBtn();
	}
}

//static
void LLPanelRegionTerrainInfo::onChangeSunHour(LLUICtrl* ctrl, void*)
{
	// Cannot use userdata to get panel, slider uses it internally
	LLPanelRegionTerrainInfo* panel = (LLPanelRegionTerrainInfo*)ctrl->getParent();
	if (panel)
	{
		panel->enableApplyBtn();
	}
}

//static
void LLPanelRegionTerrainInfo::downloadRawCallback(HBFileSelector::ESaveFilter filter,
												   std::string& filepath,
												   void* data)
{
	LLPanelRegionTerrainInfo* self = LLFloaterRegionInfo::getPanelTerrain();
	if (!self || data != self || !gXferManagerp) return;

	gXferManagerp->expectFileForRequest(filepath);
	strings_t strings;
	strings.emplace_back("download filename");
	strings.emplace_back(filepath);
	self->sendEstateOwnerMessage("terrain", strings);
}

//static
void LLPanelRegionTerrainInfo::onClickDownloadRaw(void* data)
{
	HBFileSelector::saveFile(HBFileSelector::FFSAVE_RAW, "terrain.raw",
							 downloadRawCallback, data);
}

//static
void LLPanelRegionTerrainInfo::uploadRawCallback(HBFileSelector::ELoadFilter filter,
												 std::string& filepath,
												 void* data)
{
	LLPanelRegionTerrainInfo* self = LLFloaterRegionInfo::getPanelTerrain();
	if (!self || data != self || !gXferManagerp) return;

	gXferManagerp->expectFileForTransfer(filepath);
	strings_t strings;
	strings.emplace_back("upload filename");
	strings.emplace_back(filepath);
	self->sendEstateOwnerMessage("terrain", strings);

	gNotifications.add("RawUploadStarted");
}

//static
void LLPanelRegionTerrainInfo::onClickUploadRaw(void* data)
{
	HBFileSelector::loadFile(HBFileSelector::FFLOAD_TERRAIN,
							 uploadRawCallback, data);
}

//static
void LLPanelRegionTerrainInfo::onClickBakeTerrain(void* data)
{
	gNotifications.add(LLNotification::Params("ConfirmBakeTerrain").functor(
		boost::bind(&LLPanelRegionTerrainInfo::callbackBakeTerrain,
					(LLPanelRegionTerrainInfo*)data, _1, _2)));
}

bool LLPanelRegionTerrainInfo::callbackBakeTerrain(const LLSD& notification,
												   const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response))
	{
		return false;
	}

	strings_t strings;
	strings.emplace_back("bake");
	sendEstateOwnerMessage("terrain", strings);
	return false;
}

///////////////////////////////////////////////////////////////////////////////
// LLPanelEstateInfo
///////////////////////////////////////////////////////////////////////////////

LLPanelEstateInfo::LLPanelEstateInfo()
:	LLPanelRegionInfo(),
	mEstateID(0)			// 0 = invalid
{
}

//virtual
bool LLPanelEstateInfo::postBuild()
{
	// Set up the callbacks for the generic controls
	initCtrl("public_access_check");
	initCtrl("use_global_time_check");
	initCtrl("fixed_sun_check");
	initCtrl("allow_direct_teleport");
	initCtrl("limit_payment");
	initCtrl("limit_age_verified");
	initCtrl("voice_chat_check");
	initCtrl("parcel_access_override");
	initCtrl("limit_bots");

	initHelpBtn("use_global_time_help", "HelpEstateUseGlobalTime");
	initHelpBtn("fixed_sun_help", "HelpEstateFixedSun");
	initHelpBtn("public_access_help", "HelpEstatePublicAccess");
	initHelpBtn("allow_direct_teleport_help", "HelpEstateAllowDirectTeleport");
	initHelpBtn("voice_chat_help", "HelpEstateVoiceChat");

	// Set up the use global time checkbox
	childSetCommitCallback("use_global_time_check", onChangeUseGlobalTime,
						   this);
	childSetCommitCallback("fixed_sun_check", onChangeFixedSun, this);
	childSetCommitCallback("sun_hour_slider", updateChild, this);

	childSetAction("message_estate_btn", onClickMessageEstate, this);
	childSetAction("kick_user_from_estate_btn", onClickKickUser, this);

	return LLPanelRegionInfo::postBuild();
}

//static
void LLPanelEstateInfo::updateChild(LLUICtrl* ctrl, void* user_data)
{
	LLPanelEstateInfo* self = (LLPanelEstateInfo*)user_data;
	if (self && ctrl)
	{
		self->checkSunHourSlider(ctrl);
		// Ensure appropriate state of the management ui.
		self->updateControls(gAgent.getRegion());
	}
}

//virtual
bool LLPanelEstateInfo::estateUpdate(LLMessageSystem*)
{
	llinfos << "No operation..." << llendl;
	return false;
}

void LLPanelEstateInfo::updateControls(LLViewerRegion* region)
{
	bool god_or_owner = gAgent.isGodlike() ||
						(region && region->getOwner() == gAgentID);
	bool manager = region && region->isEstateManager();
	setCtrlsEnabled(god_or_owner || manager);

	disableApplyBtn();
	childSetEnabled("message_estate_btn", god_or_owner || manager);
	childSetEnabled("kick_user_from_estate_btn", god_or_owner || manager);
}

//virtual
bool LLPanelEstateInfo::refreshFromRegion(LLViewerRegion* region)
{
	updateControls(region);

	// We want estate info. To make sure it works across region boundaries and
	// multiple packets, we add a serial number to the integers and track
	// against that on update.
	LLFloaterRegionInfo::nextInvoice();
	strings_t strings;
	sendEstateOwnerMessage("getinfo", strings);

	refresh();

	// Call the parent for common book-keeping
	return LLPanelRegionInfo::refreshFromRegion(region);
}

void LLPanelEstateInfo::refresh()
{
	bool public_access = childGetValue("public_access_check").asBoolean();
	childSetEnabled("Only Allow", public_access);
	childSetEnabled("limit_payment", public_access);
	childSetEnabled("limit_age_verified", public_access);
	childSetEnabled("limit_bots", public_access);
	if (!public_access)
	{
		// If not public access, then the limit fields are meaningless and
		// should be turned off
		childSetValue("limit_payment", false);
		childSetValue("limit_age_verified", false);
		childSetValue("limit_bots", false);
	}
}

bool LLPanelEstateInfo::sendUpdate()
{
	LLNotification::Params params("ChangeLindenEstate");
	params.functor(boost::bind(&LLPanelEstateInfo::callbackChangeLindenEstate,
							   this, _1, _2));

	if (getEstateID() <= ESTATE_LAST_LINDEN)
	{
		// Trying to change reserved estate, warn
		gNotifications.add(params);
	}
	else
	{
		// For normal estates, just make the change
		gNotifications.forceResponse(params, 0);
	}
	return true;
}

bool LLPanelEstateInfo::callbackChangeLindenEstate(const LLSD& notification,
												   const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		// Send the update
		if (!commitEstateInfoCaps())
		{
			// The caps method failed, try the old way
			LLFloaterRegionInfo::nextInvoice();
			commitEstateInfoDataserver();
		}
#if 0	// We do not want to do this because we will get it automatically from
		// the sim after the spaceserver processes it
		else
		{
			// Caps method does not automatically send this info
			LLFloaterRegionInfo::requestRegionInfo();
		}
#endif
	}
	else	// Cancelling action
	{
		HBPanelLandEnvironment* panelp =
			LLFloaterRegionInfo::getPanelEnvironment();
		if (panelp)
		{
			// This will (re)set the environment override check to its
			// former (or last) value
			panelp->resetOverride();
		}
	}
	return false;
}

#if 0
// Request = "getowner"
// SParam[0] = "" (empty string)
// IParam[0] = serial
void LLPanelEstateInfo::getEstateOwner()
{
	// *TODO: disable the panel and call this method whenever we cross a region
	// boundary; re-enable when owner matches, and get new estate info.
	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessageFast(_PREHASH_EstateOwnerRequest);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);

	msg->nextBlockFast(_PREHASH_RequestData);
	msg->addStringFast(_PREHASH_Request, "getowner");

	// We send an empty string so that the variable block is not empty
	msg->nextBlockFast(_PREHASH_StringData);
	msg->addStringFast(_PREHASH_SParam, "");

	msg->nextBlockFast(_PREHASH_IntegerData);
	msg->addS32Fast(_PREHASH_IParam, LLFloaterRegionInfo::getSerial());

	gAgent.sendMessage();
}
#endif

// Tries to send estate info using a cap; returns true if it succeeded
bool LLPanelEstateInfo::commitEstateInfoCaps()
{
	std::string url = gAgent.getRegionCapability("EstateChangeInfo");
	if (url.empty())
	{
		// Whoops, could not find the capability, so bail out
		return false;
	}

	gCoros.launch("LLPanelEstateInfo::commitEstateInfoCaps",
				  boost::bind(&LLPanelEstateInfo::commitEstateInfoCapsCoro,
							  this, url));
	return true;
}

void LLPanelEstateInfo::commitEstateInfoCapsCoro(const std::string& url)
{
	LLSD body;
	body["estate_name"] = getEstateName();
	body["is_externally_visible"] = childGetValue("public_access_check").asBoolean();
	body["allow_direct_teleport"] = childGetValue("allow_direct_teleport").asBoolean();
	body["is_sun_fixed"] = childGetValue("fixed_sun_check").asBoolean();
	body["deny_anonymous"] = childGetValue("limit_payment").asBoolean();
	body["deny_age_unverified"] = childGetValue("limit_age_verified").asBoolean();
	body["block_bots"] = childGetValue("limit_bots").asBoolean();
	body["allow_voice_chat"] = childGetValue("voice_chat_check").asBoolean();
	body["override_public_access"] = childGetValue("override_public_access").asBoolean();
	// For potential EE support in OpenSIM. This is not in LLPanelEstateInfo's
	// UI: it is (re)set by HBPanelLandEnvironment directly in sEstateFlags...
	body["override_environment"] = LLEstateInfoModel::getAllowEnvironmentOverride();
	body["invoice"] = LLFloaterRegionInfo::getLastInvoice();

#if 0	// Block fly is in estate database but not in estate UI, so we are not
		// supporting it
	body["block_fly"] = childGetValue("").asBoolean();
#endif

	F32 sun_hour = getSunHour();
	if (childGetValue("use_global_time_check").asBoolean())
	{
		sun_hour = 0.f;			// 0 = global time
	}
	body["sun_hour"] = sun_hour;

	LLCoreHttpUtil::HttpCoroutineAdapter adapter("EstateChangeInfo");
	LLSD result = adapter.postAndSuspend(url, body);

	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);
	if (status)
	{
		refresh();
	}
	else
	{
		llwarns << "Failed to commit estate info: " << status.toString()
				<< llendl;
	}
}

// This is the old way of doing things, is deprecated, and should be deleted
// when the dataserver model can be removed.
// key = "estatechangeinfo"
// strings[0] = str(estate_id) (added by simulator before relay - not here)
// strings[1] = estate_name
// strings[2] = str(estate_flags)
// strings[3] = str((S32)(sun_hour * 1024.f))
void LLPanelEstateInfo::commitEstateInfoDataserver()
{
	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessage(_PREHASH_EstateOwnerMessage);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->addUUIDFast(_PREHASH_TransactionID, LLUUID::null); // not used

	msg->nextBlock(_PREHASH_MethodData);
	msg->addString(_PREHASH_Method, "estatechangeinfo");
	msg->addUUID(_PREHASH_Invoice, LLFloaterRegionInfo::getLastInvoice());

	msg->nextBlock(_PREHASH_ParamList);
	msg->addString(_PREHASH_Parameter, getEstateName());

	std::string buffer;
	buffer = llformat("%u", computeEstateFlags());
	msg->nextBlock(_PREHASH_ParamList);
	msg->addString(_PREHASH_Parameter, buffer);

	F32 sun_hour = getSunHour();
	if (childGetValue("use_global_time_check").asBoolean())
	{
		sun_hour = 0.f;	// 0 = global time
	}

	buffer = llformat("%d", (S32)(sun_hour * 1024.f));
	msg->nextBlock(_PREHASH_ParamList);
	msg->addString(_PREHASH_Parameter, buffer);

	gAgent.sendMessage();
}

void LLPanelEstateInfo::setEstateFlags(U32 flags)
{
	childSetValue("public_access_check",
				  LLSD((flags & REGION_FLAGS_EXTERNALLY_VISIBLE) != 0));
	childSetValue("fixed_sun_check",
				  LLSD((flags & REGION_FLAGS_SUN_FIXED) != 0));
	childSetValue("voice_chat_check",
				  LLSD((flags & REGION_FLAGS_ALLOW_VOICE) != 0));
	childSetValue("allow_direct_teleport",
				  LLSD((flags & REGION_FLAGS_ALLOW_DIRECT_TELEPORT) != 0));
	childSetValue("limit_payment",
				  LLSD((flags & REGION_FLAGS_DENY_ANONYMOUS) != 0));
	childSetValue("limit_age_verified",
				  LLSD((flags & REGION_FLAGS_DENY_AGEUNVERIFIED) != 0));
	childSetValue("parcel_access_override",
				  LLSD((flags & REGION_FLAGS_ALLOW_ACCESS_OVERRIDE) != 0));
	childSetValue("limit_bots", LLSD((flags & REGION_FLAGS_DENY_BOTS) != 0));

	refresh();
}

U32 LLPanelEstateInfo::computeEstateFlags()
{
	U32 flags = 0;

	// This is not in LLPanelEstateInfo's UI: it is (re)set by
	// HBPanelLandEnvironment directly in sEstateFlags...
	if (LLEstateInfoModel::getAllowEnvironmentOverride())
	{
		flags |= REGION_FLAGS_ALLOW_ENVIRONMENT_OVERRIDE;
	}

	if (childGetValue("public_access_check").asBoolean())
	{
		flags |= REGION_FLAGS_EXTERNALLY_VISIBLE;
	}

	if (childGetValue("voice_chat_check").asBoolean())
	{
		flags |= REGION_FLAGS_ALLOW_VOICE;
	}

	if (childGetValue("parcel_access_override").asBoolean())
	{
		flags |= REGION_FLAGS_ALLOW_ACCESS_OVERRIDE;
	}

	if (childGetValue("allow_direct_teleport").asBoolean())
	{
		flags |= REGION_FLAGS_ALLOW_DIRECT_TELEPORT;
	}

	if (childGetValue("fixed_sun_check").asBoolean())
	{
		flags |= REGION_FLAGS_SUN_FIXED;
	}

	if (childGetValue("limit_payment").asBoolean())
	{
		flags |= REGION_FLAGS_DENY_ANONYMOUS;
	}

	if (childGetValue("limit_age_verified").asBoolean())
	{
		flags |= REGION_FLAGS_DENY_AGEUNVERIFIED;
	}

	if (childGetValue("limit_bots").asBoolean())
	{
		flags |= REGION_FLAGS_DENY_BOTS;
	}

	// Store in LLEstateInfoModel
	LLEstateInfoModel::sEstateFlags = flags;

	return flags;
}

bool LLPanelEstateInfo::getGlobalTime()
{
	return childGetValue("use_global_time_check").asBoolean();
}

void LLPanelEstateInfo::setGlobalTime(bool b)
{
	childSetValue("use_global_time_check", LLSD(b));
	childSetEnabled("fixed_sun_check", LLSD(!b));
	childSetEnabled("sun_hour_slider", LLSD(!b));
	if (b)
	{
		childSetValue("sun_hour_slider", LLSD(0.f));
	}
}

bool LLPanelEstateInfo::getFixedSun()
{
	return childGetValue("fixed_sun_check").asBoolean();
}

void LLPanelEstateInfo::setSunHour(F32 sun_hour)
{
	if (sun_hour < 6.f)
	{
		sun_hour = 24.f + sun_hour;
	}
	childSetValue("sun_hour_slider", LLSD(sun_hour));
}

F32 LLPanelEstateInfo::getSunHour()
{
	if (childIsEnabled("sun_hour_slider"))
	{
		return (F32)childGetValue("sun_hour_slider").asReal();
	}
	return 0.f;
}

const std::string LLPanelEstateInfo::getEstateName() const
{
	return childGetValue("estate_name").asString();
}

void LLPanelEstateInfo::setEstateName(const std::string& name)
{
	childSetValue("estate_name", LLSD(name));
}

const std::string LLPanelEstateInfo::getOwnerName() const
{
	return childGetValue("estate_owner").asString();
}

void LLPanelEstateInfo::setOwnerName(const std::string& name)
{
	childSetValue("estate_owner", LLSD(name));
}

bool LLPanelEstateInfo::checkSunHourSlider(LLUICtrl* child_ctrl)
{
	bool found_child_ctrl = false;
	if (child_ctrl->getName() == "sun_hour_slider")
	{
		enableApplyBtn();
		found_child_ctrl = true;
	}
	return found_child_ctrl;
}

bool LLPanelEstateInfo::kickUserConfirm(const LLSD& notification,
										const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		// Kick User
		strings_t strings;
		strings.emplace_back(notification["payload"]["agent_id"].asString());
		sendEstateOwnerMessage("kickestate", strings);
	}
	return false;
}


//static
void LLPanelEstateInfo::onClickMessageEstate(void* userdata)
{
	gNotifications.add("MessageEstate", LLSD(), LLSD(),
					   boost::bind(&LLPanelEstateInfo::onMessageCommit,
								   (LLPanelEstateInfo*)userdata, _1, _2));
}

//static
bool LLPanelEstateInfo::onMessageCommit(const LLSD& notification,
										const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	std::string text = response["message"].asString();
	if (option != 0) return false;
	if (text.empty()) return false;
	llinfos << "Message to everyone: " << text << llendl;
	strings_t strings;
	//integers_t integers;
	std::string name;
	gAgent.buildFullname(name);
	strings.emplace_back(strings_t::value_type(name));
	strings.emplace_back(strings_t::value_type(text));
	sendEstateOwnerMessage("instantmessage", strings);
	return false;
}

//static
void LLPanelEstateInfo::initDispatch(LLDispatcher& dispatch)
{
	std::string name = "estateupdateinfo";
	static LLDispatchEstateUpdateInfo estate_update_info;
	dispatch.addHandler(name, &estate_update_info);

	name = "setaccess";
	static LLDispatchSetEstateAccess set_access;
	dispatch.addHandler(name, &set_access);

	name = "setexperience";
	static LLDispatchSetEstateExperience set_experience;
	dispatch.addHandler(name, &set_experience);

	gEstateDispatchInitialized = true;
}

//static
void LLPanelEstateInfo::callbackCacheName(const LLUUID& id,
										  const std::string& full_name,
										  bool is_group)
{
	LLPanelEstateInfo* self = LLFloaterRegionInfo::getPanelEstate();
	if (!self) return;

	std::string name;

	if (id.isNull())
	{
		name = "(none)";
	}
	else
	{
		name = full_name;
	}

	self->setOwnerName(name);
}

// Disables the sun-hour slider and the use fixed time check if the use global
// time is check
//static
void LLPanelEstateInfo::onChangeUseGlobalTime(LLUICtrl* ctrl, void* user_data)
{
	LLPanelEstateInfo* self = (LLPanelEstateInfo*)user_data;
	if (!self) return;

	bool enabled = !self->childGetValue("use_global_time_check").asBoolean();
	self->childSetEnabled("sun_hour_slider", enabled);
	self->childSetEnabled("fixed_sun_check", enabled);
	self->childSetValue("fixed_sun_check", LLSD(false));
	self->enableApplyBtn();
}

// Enables the sun-hour slider if the fixed-sun checkbox is set
//static
void LLPanelEstateInfo::onChangeFixedSun(LLUICtrl* ctrl, void* user_data)
{
	LLPanelEstateInfo* self = (LLPanelEstateInfo*) user_data;
	if (!self) return;

	bool enabled = !self->childGetValue("fixed_sun_check").asBoolean();
	self->childSetEnabled("use_global_time_check", enabled);
	self->childSetValue("use_global_time_check", LLSD(false));
	self->enableApplyBtn();
}

//static
void LLPanelEstateInfo::onClickKickUser(void* user_data)
{
	LLPanelEstateInfo* self = (LLPanelEstateInfo*)user_data;
	if (!self) return;

	LLFloater* picker = LLFloaterAvatarPicker::show(onKickUserCommit,
													user_data, false, true);
	if (picker && gFloaterViewp)
	{
		// This depends on the grandparent view being a floater in order to set
		// up floater dependency
		LLFloater* parentp = gFloaterViewp->getParentFloater(self);
		if (parentp)
		{
			parentp->addDependentFloater(picker);
		}
	}
}

struct LLKickFromEstateInfo
{
	LLPanelEstateInfo*	mEstatePanelp;
	LLUUID      		mAgentID;
};

//static
void LLPanelEstateInfo::onKickUserCommit(const std::vector<std::string>& names,
										 const uuid_vec_t& ids,
										 void* userdata)
{
	LLPanelEstateInfo* self = (LLPanelEstateInfo*)userdata;
	if (!self || names.empty() || ids.empty() ||
		// Check to make sure there is one valid user and id
		ids[0].isNull() || names[0].length() == 0)
	{
		return;
	}

	// Keep track of what user they want to kick and other misc info
	LLKickFromEstateInfo* kick_info = new LLKickFromEstateInfo();
	kick_info->mEstatePanelp = self;
	kick_info->mAgentID = ids[0];

	// Bring up a confirmation dialog
	LLSD args;
	args["EVIL_USER"] = names[0];
	LLSD payload;
	payload["agent_id"] = ids[0];
	gNotifications.add("EstateKickUser", args, payload,
					   boost::bind(&LLPanelEstateInfo::kickUserConfirm, self,
								   _1, _2));

}

//static
bool LLPanelEstateInfo::isLindenEstate()
{
	LLPanelEstateInfo* self = LLFloaterRegionInfo::getPanelEstate();
	return self && self->getEstateID() <= ESTATE_LAST_LINDEN;
}

//static
void LLPanelEstateInfo::updateEstateOwnerName(const std::string& name)
{
	LLPanelEstateInfo* self = LLFloaterRegionInfo::getPanelEstate();
	if (self)
	{
		self->setOwnerName(name);
	}
}

//static
void LLPanelEstateInfo::updateEstateName(const std::string& name)
{
	LLPanelEstateInfo* self = LLFloaterRegionInfo::getPanelEstate();
	if (self)
	{
		self->setEstateName(name);
	}
}

///////////////////////////////////////////////////////////////////////////////
// LLPanelEstateAccess
///////////////////////////////////////////////////////////////////////////////

//static
S32 LLPanelEstateAccess::sLastActiveTab = 0;

LLPanelEstateAccess::LLPanelEstateAccess()
:	LLPanelRegionInfo(),
	mPendingUpdate(false),
	mCtrlsEnabled(false)
{
}

//virtual
bool LLPanelEstateAccess::postBuild()
{
	mTabContainer = getChild<LLTabContainer>("access_tabs");
	LLPanel* tab = mTabContainer->getChild<LLPanel>("estate_managers");
	mTabContainer->setTabChangeCallback(tab, onTabChanged);
	mTabContainer->setTabUserData(tab, this);
	tab = mTabContainer->getChild<LLPanel>("allowed_groups");
	mTabContainer->setTabChangeCallback(tab, onTabChanged);
	mTabContainer->setTabUserData(tab, this);
	tab = mTabContainer->getChild<LLPanel>("allowed_resident");
	mTabContainer->setTabChangeCallback(tab, onTabChanged);
	mTabContainer->setTabUserData(tab, this);
	tab = mTabContainer->getChild<LLPanel>("banned_residents");
	mTabContainer->setTabChangeCallback(tab, onTabChanged);
	mTabContainer->setTabUserData(tab, this);

	mTabContainer->selectTab(sLastActiveTab);

	initHelpBtn("estate_manager_help", "HelpEstateEstateManager");
	initHelpBtn("allow_group_help", "HelpEstateAllowGroup");
	initHelpBtn("allow_resident_help", "HelpEstateAllowResident");
	initHelpBtn("ban_resident_help", "HelpEstateBanResident");

	mEstateManagers = getChild<LLNameListCtrl>("estate_manager_name_list");
	mEstateManagers->setCommitCallback(updateChild);
	mEstateManagers->setCallbackUserData(this);
	mEstateManagers->setCommitOnSelectionChange(true);
	// Allow extras for dupe issue
	mEstateManagers->setMaxItemCount(ESTATE_MAX_MANAGERS * 4);

	childSetAction("add_estate_manager_btn", onClickAddEstateManager, this);
	childSetAction("remove_estate_manager_btn", onClickRemoveEstateManager,
				   this);

	mAllowedGroups = getChild<LLNameListCtrl>("allowed_group_name_list");
	mAllowedGroups->setCommitCallback(updateChild);
	mAllowedGroups->setCallbackUserData(this);
	mAllowedGroups->setCommitOnSelectionChange(true);
	mAllowedGroups->setMaxItemCount(ESTATE_MAX_ACCESS_IDS);

	childSetAction("add_allowed_group_btn", onClickAddAllowedGroup, this);
	childSetAction("remove_allowed_group_btn", onClickRemoveAllowedGroup,
				   this);

	mAllowedAvatars = getChild<LLNameListCtrl>("allowed_avatar_name_list");
	mAllowedAvatars->setCommitCallback(updateChild);
	mAllowedAvatars->setCallbackUserData(this);
	mAllowedAvatars->setCommitOnSelectionChange(true);
	mAllowedAvatars->setMaxItemCount(ESTATE_MAX_ACCESS_IDS);

	childSetAction("add_allowed_avatar_btn", onClickAddAllowedAgent, this);
	childSetAction("remove_allowed_avatar_btn", onClickRemoveAllowedAgent,
				   this);

	mBannedAvatars = getChild<LLNameListCtrl>("banned_avatar_name_list");
	mBannedAvatars->setCommitCallback(updateChild);
	mBannedAvatars->setCallbackUserData(this);
	mBannedAvatars->setCommitOnSelectionChange(true);
	mBannedAvatars->setMaxItemCount(ESTATE_MAX_BANNED_IDS);

	childSetAction("add_banned_avatar_btn", onClickAddBannedAgent, this);
	childSetAction("remove_banned_avatar_btn", onClickRemoveBannedAgent, this);

#if 0	// *TODO: implement (backport from LL's viewer-release)
	childSetCommitCallback("allowed_search_input", onAllowedSearchEdit, this);
	childSetCommitCallback("banned_search_input", onBannedSearchEdit, this);
	childSetCommitCallback("allowed_group_search_input",
						   onAllowedGroupsSearchEdit, this);
	childSetAction("copy_allowed_list_btn", onClickCopyAllowedList, this);
	childSetAction("copy_allowed_group_list_btn", onClickCopyAllowedGroupList,
				   this);
	childSetAction("copy_banned_list_btn", onClickCopyBannedList, this);
#endif

	// Note: no apply button, so we do not call LLPanelRegionInfo::postBuild()
	return true;
}

//static
void LLPanelEstateAccess::updateChild(LLUICtrl* ctrl, void* user_data)
{
	LLPanelEstateAccess* self = (LLPanelEstateAccess*)user_data;
	if (self && ctrl)
	{
		self->checkRemovalButton(ctrl->getName());
		// Ensure appropriate state of the management ui.
		self->updateControls(gAgent.getRegion());
	}
}

//virtual
bool LLPanelEstateAccess::refreshFromRegion(LLViewerRegion* region)
{
	LL_DEBUGS("RegionInfo") << "Refreshing from region..." << LL_ENDL;
	updateLists();
	// Call the parent for common book-keeping
	return LLPanelRegionInfo::refreshFromRegion(region);
}

void LLPanelEstateAccess::updateLists()
{
	std::string cap_url = gAgent.getRegionCapability("EstateAccess");
	if (cap_url.empty())
	{
		strings_t strings;
		LLFloaterRegionInfo::nextInvoice();
		sendEstateOwnerMessage("getinfo", strings);
		return;
	}

	// Use the capability
	gCoros.launch("LLPanelEstateAccess::requestEstateGetAccessCoro",
				  boost::bind(LLPanelEstateAccess::requestEstateGetAccessCoro,
							  cap_url));
}

void LLPanelEstateAccess::requestEstateGetAccessCoro(const std::string& url)
{
	LLCoreHttpUtil::HttpCoroutineAdapter adapter("requestEstateGetAccessoCoro");

	LLSD result = adapter.getAndSuspend(url);

	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(result);

	LLPanelEstateAccess* self = LLFloaterRegionInfo::getPanelAccess();
	if (!self) return;		// We have since been closed...

	LLNameListCtrl* name_list;

	if (result.has("AllowedAgents"))
	{
		name_list = self->mAllowedAvatars;
		const LLSD& allowed = result["AllowedAgents"];

		LLStringUtil::format_map_t args;
		args["[ALLOWEDAGENTS]"] = llformat("%d", allowed.size());
		args["[MAXACCESS]"] = llformat("%d", ESTATE_MAX_ACCESS_IDS);
		std::string msg = self->getString("RegionInfoAllowedResidents", args);
		self->getChild<LLUICtrl>("allow_resident_label")->setValue(LLSD(msg));

		name_list->clearSortOrder();
		name_list->deleteAllItems();
		for (LLSD::array_const_iterator it = allowed.beginArray(),
										end = allowed.endArray();
			 it != end; ++it)
		{
			LLUUID id = (*it)["id"].asUUID();
			name_list->addNameItem(id);
		}
		name_list->sortByName(true);
	}

	if (result.has("BannedAgents"))
	{
		name_list = self->mBannedAvatars;
		const LLSD& banned = result["BannedAgents"];

		LLStringUtil::format_map_t args;
		args["[BANNEDAGENTS]"] = llformat("%d", banned.size());
		args["[MAXBANNED]"] = llformat("%d", ESTATE_MAX_BANNED_IDS);
		std::string msg = self->getString("RegionInfoBannedResidents", args);
		self->getChild<LLUICtrl>("ban_resident_label")->setValue(LLSD(msg));

		name_list->clearSortOrder();
		name_list->deleteAllItems();
		std::string fullname;
		std::string na = LLTrans::getString("na");
		for (LLSD::array_const_iterator it = banned.beginArray(),
										end = banned.endArray();
			 it != end; ++it)
		{
			LLSD item;
			item["id"] = (*it)["id"].asUUID();
			LLSD& columns = item["columns"];

			columns[0]["column"] = "name"; // value is auto-populated

			columns[1]["column"] = "last_login_date";
			columns[1]["value"] =
													// Cut the seconds
				(*it)["last_login_date"].asString().substr(0, 16);

			std::string ban_date = (*it)["ban_date"].asString();
			columns[2]["column"] = "ban_date";
			 // The server returns the "0000-00-00 00:00:00" date in case it
			// does not know it
			columns[2]["value"] = ban_date[0] ? ban_date.substr(0, 16) : na;

			columns[3]["column"] = "bannedby";
			LLUUID banning_id = (*it)["banning_id"].asUUID();
			if (banning_id.isNull())
			{
				columns[3]["value"] = na;
			}
			else if (gCacheNamep &&
					 gCacheNamep->getFullName(banning_id, fullname))
			{
				// *TODO: fetch the name if it was not cached
				columns[3]["value"] = fullname;
			}

			name_list->addElement(item);
		}
		name_list->sortByName(true);
	}

	if (result.has("AllowedGroups"))
	{
		name_list = self->mAllowedGroups;
		const LLSD& groups = result["AllowedGroups"];

		LLStringUtil::format_map_t args;
		args["[ALLOWEDGROUPS]"] = llformat("%d", groups.size());
		args["[MAXACCESS]"] = llformat("%d", ESTATE_MAX_GROUP_IDS);
		std::string msg = self->getString("RegionInfoAllowedGroups", args);
		self->getChild<LLUICtrl>("allow_group_label")->setValue(LLSD(msg));

		name_list->clearSortOrder();
		name_list->deleteAllItems();
		for (LLSD::array_const_iterator it = groups.beginArray(),
										end = groups.endArray();
			 it != end; ++it)
		{
			LLUUID id = (*it)["id"].asUUID();
			name_list->addGroupNameItem(id);
		}
		name_list->sortByName(true);
	}

	if (result.has("Managers"))
	{
		name_list = self->mEstateManagers;
		const LLSD& managers = result["Managers"];

		LLStringUtil::format_map_t args;
		args["[ESTATEMANAGERS]"] = llformat("%d", managers.size());
		args["[MAXMANAGERS]"] = llformat("%d", ESTATE_MAX_MANAGERS);
		std::string msg = self->getString("RegionInfoEstateManagers", args);
		self->getChild<LLUICtrl>("estate_manager_label")->setValue(LLSD(msg));

		name_list->clearSortOrder();
		name_list->deleteAllItems();
		for (LLSD::array_const_iterator it = managers.beginArray(),
										end = managers.endArray();
			 it != end; ++it)
		{
			LLUUID id = (*it)["agent_id"].asUUID();
			name_list->addNameItem(id);
		}
		name_list->sortByName(true);
	}

	self->updateControls(gAgent.getRegion());
}

//static
void LLPanelEstateAccess::onTabChanged(void* user_data, bool)
{
	LLPanelEstateAccess* self = (LLPanelEstateAccess*)user_data;
	if (self)
	{
		sLastActiveTab = self->mTabContainer->getCurrentPanelIndex();
	}
}

//static
void LLPanelEstateAccess::onClickAddAllowedAgent(void* user_data)
{
	LLPanelEstateAccess* self = (LLPanelEstateAccess*)user_data;
	if (!self) return;

	if (self->mAllowedAvatars->getItemCount() >= ESTATE_MAX_ACCESS_IDS)
	{
		LLSD args;
		args["MAX_AGENTS"] = llformat("%d",ESTATE_MAX_ACCESS_IDS);
		gNotifications.add("MaxAllowedAgentOnRegion", args);
	}
	else
	{
		accessAddCore(ESTATE_ACCESS_ALLOWED_AGENT_ADD);
	}
}

//static
void LLPanelEstateAccess::onClickRemoveAllowedAgent(void* user_data)
{
	accessRemoveCore(ESTATE_ACCESS_ALLOWED_AGENT_REMOVE);
}

//static
void LLPanelEstateAccess::onClickAddAllowedGroup(void* user_data)
{
	LLPanelEstateAccess* self = (LLPanelEstateAccess*)user_data;
	if (!self) return;

	if (self->mAllowedGroups->getItemCount() >= ESTATE_MAX_ACCESS_IDS)
	{
		LLSD args;
		args["MAX_GROUPS"] = llformat("%d",ESTATE_MAX_ACCESS_IDS);
		gNotifications.add("MaxAllowedGroupsOnRegion", args);
		return;
	}

	LLNotification::Params params("ChangeLindenAccess");
	params.functor(boost::bind(&LLPanelEstateAccess::addAllowedGroup,
							   self, _1, _2));
	if (LLPanelEstateInfo::isLindenEstate())
	{
		gNotifications.add(params);
	}
	else
	{
		gNotifications.forceResponse(params, 0);
	}
}

//static
bool LLPanelEstateAccess::addAllowedGroup(const LLSD& notification,
										  const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response))
	{
		return false;
	}

	LLFloaterGroupPicker* picker = LLFloaterGroupPicker::show(addAllowedGroup2,
															  NULL);
	LLFloater* parentp = gFloaterViewp->getParentFloater(this);
	if (picker && parentp && gFloaterViewp)
	{
		LLRect new_rect = gFloaterViewp->findNeighboringPosition(parentp,
																 picker);
		picker->setOrigin(new_rect.mLeft, new_rect.mBottom);
		parentp->addDependentFloater(picker);
	}

	return false;
}

//static
void LLPanelEstateAccess::onClickRemoveAllowedGroup(void* user_data)
{
	accessRemoveCore(ESTATE_ACCESS_ALLOWED_GROUP_REMOVE);
}

//static
void LLPanelEstateAccess::onClickAddBannedAgent(void* user_data)
{
	LLPanelEstateAccess* self = (LLPanelEstateAccess*)user_data;
	if (!self) return;

	if (self->mBannedAvatars->getItemCount() >= ESTATE_MAX_BANNED_IDS)
	{
		LLSD args;
		args["MAX_BANNED"] = llformat("%d",ESTATE_MAX_BANNED_IDS);
		gNotifications.add("MaxBannedAgentsOnRegion", args);
	}
	else
	{
		accessAddCore(ESTATE_ACCESS_BANNED_AGENT_ADD);
	}
}

//static
void LLPanelEstateAccess::onClickRemoveBannedAgent(void* user_data)
{
	accessRemoveCore(ESTATE_ACCESS_BANNED_AGENT_REMOVE);
}

//static
void LLPanelEstateAccess::onClickAddEstateManager(void* user_data)
{
	LLPanelEstateAccess* self = (LLPanelEstateAccess*)user_data;
	if (!self) return;

	if (self->mEstateManagers->getItemCount() >= ESTATE_MAX_MANAGERS)
	{
		// Tell user they cannot add more managers
		LLSD args;
		args["MAX_MANAGER"] = llformat("%d", ESTATE_MAX_MANAGERS);
		gNotifications.add("MaxManagersOnRegion", args);
	}
	else
	{
		// Go pick managers to add
		accessAddCore(ESTATE_ACCESS_MANAGER_ADD);
	}
}

//static
void LLPanelEstateAccess::onClickRemoveEstateManager(void* user_data)
{
	accessRemoveCore(ESTATE_ACCESS_MANAGER_REMOVE);
}

//static
std::string LLPanelEstateAccess::allEstatesText()
{
	LLPanelEstateAccess* self = LLFloaterRegionInfo::getPanelAccess();
	LLPanelEstateInfo* panel_info = LLFloaterRegionInfo::getPanelEstate();
	LLViewerRegion* region = gAgent.getRegion();
	if (!self || !panel_info || !region)
	{
		return "(error)";
	}

	if (gAgent.isGodlike())
	{
		LLStringUtil::format_map_t args;
		args["[OWNER]"] = panel_info->getOwnerName();
		return self->getString("all_estates_owned_by", args);
	}
	else if (region->getOwner() == gAgentID)
	{
		return self->getString("all_estates_you_own");
	}
	else if (region->isEstateManager())
	{
		LLStringUtil::format_map_t args;
		args["[OWNER]"] = panel_info->getOwnerName();
		return self->getString("all_estates_you_manage_for", args);
	}

	return self->getString("error");
}

// Special case callback for groups, since it has different callback format
// than names
//static
void LLPanelEstateAccess::addAllowedGroup2(LLUUID id, void* user_data)
{
	LLPanelEstateAccess* self = LLFloaterRegionInfo::getPanelAccess();
	if (!self) return;

	LLScrollListItem* item = self->mAllowedGroups->getItemById(id);
	if (item)
	{
		LLSD args;
		args["GROUP"] = item->getColumn(0)->getValue().asString();
		gNotifications.add("GroupIsAlreadyInList", args);
		return;
	}

	LLSD payload;
	payload["operation"] = (S32)ESTATE_ACCESS_ALLOWED_GROUP_ADD;
	payload["dialog_name"] = "EstateAllowedGroupAdd";
	payload["allowed_ids"].append(id);

	LLSD args;
	args["ALL_ESTATES"] = allEstatesText();

	LLNotification::Params params("EstateAllowedGroupAdd");
	params.payload(payload).substitutions(args).functor(accessCoreConfirm);
	if (LLPanelEstateInfo::isLindenEstate())
	{
		gNotifications.forceResponse(params, 0);
	}
	else
	{
		gNotifications.add(params);
	}
}

//static
void LLPanelEstateAccess::accessAddCore(U32 operation_flag)
{
	std::string dialog_name;
	switch (operation_flag)
	{
		case ESTATE_ACCESS_MANAGER_ADD:
			dialog_name = "EstateManagerAdd";
			break;

		case ESTATE_ACCESS_ALLOWED_AGENT_ADD:
			dialog_name = "EstateAllowedAgentAdd";
			break;

		case ESTATE_ACCESS_BANNED_AGENT_ADD:
			dialog_name = "EstateBannedAgentAdd";
			break;

		default:
			llwarns << "Invalid remove operation requested: " << operation_flag
					<< llendl;
			llassert(false);
			return;
	}

	LLSD payload;
	payload["operation"] = (S32)operation_flag;
	payload["dialog_name"] = dialog_name;
	// Avatar id filled in after avatar picker

	LLNotification::Params params("ChangeLindenAccess");
	params.payload(payload).functor(accessAddCore2);

	if (LLPanelEstateInfo::isLindenEstate())
	{
		gNotifications.add(params);
	}
	else
	{
		// Same as clicking "OK"
		gNotifications.forceResponse(params, 0);
	}
}

//static
bool LLPanelEstateAccess::accessAddCore2(const LLSD& notification,
										 const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		LLEstateAccessChangeInfo* change_info =
			new LLEstateAccessChangeInfo(notification["payload"]);
		// Avatar picker yes multi-select, yes close-on-select
		LLFloaterAvatarPicker::show(accessAddCore3, (void*)change_info, true,
									true);
	}
	return false;
}

//static
void LLPanelEstateAccess::accessAddCore3(const std::vector<std::string>& names,
										 const uuid_vec_t& ids, void* data)
{
	LLEstateAccessChangeInfo* change_info = (LLEstateAccessChangeInfo*)data;
	if (!change_info) return;

	LLPanelEstateAccess* self = LLFloaterRegionInfo::getPanelAccess();
	LLViewerRegion* region = gAgent.getRegion();
	if (!self || !region || ids.empty())
	{
		delete change_info;
		return;
	}

	// User did select a name. Note: cannot put estate owner on ban list.
	change_info->mAgentOrGroupIDs = ids;

	if (change_info->mOperationFlag & ESTATE_ACCESS_ALLOWED_AGENT_ADD)
	{
		LLNameListCtrl*	name_list = self->mAllowedAvatars;
		S32 list_count = name_list->getItemCount();
		S32 total = ids.size() + list_count;
		if (total > ESTATE_MAX_ACCESS_IDS)
		{
			LLSD args;
			args["NUM_ADDED"] = llformat("%d", ids.size());
			args["MAX_AGENTS"] = llformat("%d", ESTATE_MAX_ACCESS_IDS);
			args["LIST_TYPE"] = "Allowed Residents";
			args["NUM_EXCESS"] = llformat("%d", total - ESTATE_MAX_ACCESS_IDS);
			gNotifications.add("MaxAgentOnRegionBatch", args);
			delete change_info;
			return;
		}

		uuid_vec_t ids_allowed;
		std::string already_allowed, first, last;
		bool single = true;
		for (U32 i = 0, count = ids.size(); i < count; ++i)
		{
			const LLUUID& id = ids[i];
			LLScrollListItem* item = name_list->getItemById(id);
			if (item)
			{
				if (!already_allowed.empty())
				{
					already_allowed += ", ";
					single = false;
				}
				already_allowed += item->getColumn(0)->getValue().asString();
			}
			else
			{
				ids_allowed.emplace_back(id);
				// Used to trigger a name caching request, in anticipation
				// for confirmation dialogs.
				gCacheNamep->getName(id, first, last);
			}
		}
		if (!already_allowed.empty())
		{
			LLSD args;
			args["AGENT"] = already_allowed;
			args["LIST_TYPE"] =
				self->getString("RegionInfoListTypeAllowedAgents");
			std::string dialog = single ? "AgentIsAlreadyInList"
										: "AgentsAreAlreadyInList";
			gNotifications.add(dialog, args);
			if (ids_allowed.empty())
			{
				delete change_info;
				return;
			}
		}
		change_info->mAgentOrGroupIDs = ids_allowed;
	}

	if (change_info->mOperationFlag & ESTATE_ACCESS_BANNED_AGENT_ADD)
	{
		LLNameListCtrl*	name_list = self->mBannedAvatars;
		S32 list_count = name_list->getItemCount();
		S32 total = ids.size() + list_count;
		if (total > ESTATE_MAX_BANNED_IDS)
		{
			LLSD args;
			args["NUM_ADDED"] = llformat("%d", ids.size());
			args["MAX_AGENTS"] = llformat("%d", ESTATE_MAX_BANNED_IDS);
			args["LIST_TYPE"] = "Banned Residents";
			args["NUM_EXCESS"] = llformat("%d", total - ESTATE_MAX_BANNED_IDS);
			gNotifications.add("MaxAgentOnRegionBatch", args);
			delete change_info;
			return;
		}

		LLNameListCtrl*	em_list = self->mEstateManagers;
		uuid_vec_t ids_banned;
		std::string already_banned, em_ban, first, last;
		bool single = true;
		for (U32 i = 0, count = ids.size(); i < count; ++i)
		{
			const LLUUID& id = ids[i];
			bool can_ban = true;
			LLScrollListItem* em_item = em_list->getItemById(id);
			if (em_item)
			{
				if (!em_ban.empty())
				{
					em_ban += ", ";
				}
				em_ban += em_item->getColumn(0)->getValue().asString();
				can_ban = false;
			}

			LLScrollListItem* item = name_list->getItemById(id);
			if (item)
			{
				if (!already_banned.empty())
				{
					already_banned += ", ";
					single = false;
				}
				already_banned += item->getColumn(0)->getValue().asString();
				can_ban = false;
			}

			if (can_ban)
			{
				ids_banned.emplace_back(id);
				// Used to trigger a name caching request, in anticipation
				// for confirmation dialogs.
				gCacheNamep->getName(id, first, last);
			}
		}
		if (!em_ban.empty())
		{
			LLSD args;
			args["AGENT"] = em_ban;
			gNotifications.add("ProblemBanningEstateManager", args);
			if (ids_banned.empty())
			{
				delete change_info;
				return;
			}
		}
		if (!already_banned.empty())
		{
			LLSD args;
			args["AGENT"] = already_banned;
			args["LIST_TYPE"] =
				self->getString("RegionInfoListTypeBannedAgents");
			std::string dialog = single ? "AgentIsAlreadyInList"
										: "AgentsAreAlreadyInList";
			gNotifications.add(dialog, args);
			if (ids_banned.empty())
			{
				delete change_info;
				return;
			}
		}
		change_info->mAgentOrGroupIDs = ids_banned;
	}

	LLSD args;
	args["ALL_ESTATES"] = allEstatesText();
	LLNotification::Params params(change_info->mDialogName);
	params.substitutions(args).payload(change_info->asLLSD()).functor(accessCoreConfirm);
	if (LLPanelEstateInfo::isLindenEstate())
	{
		// Just apply to this estate
		gNotifications.forceResponse(params, 0);
	}
	else
	{
		// Ask if this estate or all estates with this owner
		gNotifications.add(params);
	}
}

//static
void LLPanelEstateAccess::accessRemoveCore(U32 operation_flag)
{
	LLPanelEstateAccess* self = LLFloaterRegionInfo::getPanelAccess();
	if (!self) return;

	std::string dialog_name;
	LLNameListCtrl* name_list;
	switch (operation_flag)
	{
		case ESTATE_ACCESS_MANAGER_REMOVE:
			dialog_name = "EstateManagerRemove";
			name_list = self->mEstateManagers;
			break;

		case ESTATE_ACCESS_ALLOWED_GROUP_REMOVE:
			dialog_name = "EstateAllowedGroupRemove";
			name_list = self->mAllowedGroups;
			break;

		case ESTATE_ACCESS_ALLOWED_AGENT_REMOVE:
			dialog_name = "EstateAllowedAgentRemove";
			name_list = self->mAllowedAvatars;
			break;

		case ESTATE_ACCESS_BANNED_AGENT_REMOVE:
			dialog_name = "EstateBannedAgentRemove";
			name_list = self->mBannedAvatars;
			break;

		default:
			llwarns << "Invalid remove operation requested: " << operation_flag
					<< llendl;
			llassert(false);
			return;
	}

	std::vector<LLScrollListItem*> list_vector = name_list->getAllSelected();
	if (list_vector.size() == 0)
	{
		return;
	}

	LLSD payload;
	payload["operation"] = (S32)operation_flag;
	payload["dialog_name"] = dialog_name;

	for (U32 i = 0, count = list_vector.size(); i < count; ++i)
	{
		LLScrollListItem* item = list_vector[i];
		if (item)	// Paranoia
		{
			payload["allowed_ids"].append(item->getUUID());
		}
	}

	LLNotification::Params params("ChangeLindenAccess");
	params.payload(payload).functor(accessRemoveCore2);

	if (LLPanelEstateInfo::isLindenEstate())
	{
		// Warn on change linden estate
		gNotifications.add(params);
	}
	else
	{
		// Just proceed, as if clicking OK
		gNotifications.forceResponse(params, 0);
	}
}

//static
bool LLPanelEstateAccess::accessRemoveCore2(const LLSD& notification,
											const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response))
	{
		return false;
	}

	// If Linden estate, can only apply to "this" estate, not all estates
	// owned by NULL.
	if (LLPanelEstateInfo::isLindenEstate())
	{
		accessCoreConfirm(notification, response);
	}
	else
	{
		LLSD args;
		args["ALL_ESTATES"] = allEstatesText();
		gNotifications.add(notification["payload"]["dialog_name"], args,
						   notification["payload"], accessCoreConfirm);
	}

	return false;
}

// Used for both access add and remove operations, depending on the
// mOperationFlag passed in (ESTATE_ACCESS_BANNED_AGENT_ADD,
// ESTATE_ACCESS_ALLOWED_AGENT_REMOVE, etc.)
//static
bool LLPanelEstateAccess::accessCoreConfirm(const LLSD& notification,
											const LLSD& response)
{
	LLPanelEstateAccess* self = LLFloaterRegionInfo::getPanelAccess();
	LLViewerRegion* region = gAgent.getRegion();
	if (!self || !region || !gCacheNamep) return false;

	S32 option = LLNotification::getSelectedOption(notification, response);
	if (option == 2)	// Cancel button
	{
		return false;
	}

	const LLSD& payload = notification["payload"];
	const U32 orig_flags = (U32)payload["operation"].asInteger();
	U32 flags = orig_flags;

	if (option == 1)
	{
		// All estates, either than I own or manage for this owner; this will
		// be verified on simulator. JC
		if (region->getOwner() == gAgentID || gAgent.isGodlike())
		{
			flags |= ESTATE_ACCESS_APPLY_TO_ALL_ESTATES;
		}
		else if (region->isEstateManager())
		{
			flags |= ESTATE_ACCESS_APPLY_TO_MANAGED_ESTATES;
		}
	}

	std::string names, fullname;
	U32 listed_names = 0;
	const LLSD& allowed_ids = payload["allowed_ids"];
	for (U32 i = 0, count = allowed_ids.size(); i < count; ++i)
	{
		if (i != count - 1)
		{
			flags |= ESTATE_ACCESS_NO_REPLY;
		}
		else
		{
			flags &= ~ESTATE_ACCESS_NO_REPLY;
		}

		const LLUUID id = allowed_ids[i].asUUID();
		if ((orig_flags & ESTATE_ACCESS_BANNED_AGENT_ADD) &&
			region->getOwner() == id)
		{
			gNotifications.add("OwnerCanNotBeDenied");
			break;
		}

		sendEstateAccessDelta(flags, id);

		if ((flags & (ESTATE_ACCESS_ALLOWED_GROUP_ADD |
					  ESTATE_ACCESS_ALLOWED_GROUP_REMOVE)) == 0)
		{
			// Fill the name list for confirmation
			if (listed_names < MAX_LISTED_NAMES)
			{
				if (!names.empty())
				{
					names += ", ";
				}
				gCacheNamep->getFullName(id, fullname);
				names += fullname;
			}
			++listed_names;
		}
	}

	if (listed_names > MAX_LISTED_NAMES)
	{
		LLStringUtil::format_map_t args;
		args["EXTRA_COUNT"] = llformat("%d", listed_names - MAX_LISTED_NAMES);
		names += " " + self->getString("AndNMore", args);
	}

	if (!names.empty())
	{
		// Show the confirmation
		LLSD args;
		args["AGENT"] = names;
		if (flags & (ESTATE_ACCESS_ALLOWED_AGENT_ADD |
					 ESTATE_ACCESS_ALLOWED_AGENT_REMOVE))
		{
			args["LIST_TYPE"] =
				self->getString("RegionInfoListTypeAllowedAgents");
		}
		else if (flags & (ESTATE_ACCESS_BANNED_AGENT_ADD |
						  ESTATE_ACCESS_BANNED_AGENT_REMOVE))
		{
			args["LIST_TYPE"] =
				self->getString("RegionInfoListTypeBannedAgents");
		}

		if (flags & ESTATE_ACCESS_APPLY_TO_ALL_ESTATES)
		{
			args["ESTATE"] = self->getString("RegionInfoAllEstates");
		}
		else if (flags & ESTATE_ACCESS_APPLY_TO_MANAGED_ESTATES)
		{
			args["ESTATE"] = self->getString("RegionInfoManagedEstates");
		}
		else
		{
			args["ESTATE"] = self->getString("RegionInfoThisEstate");
		}

		std::string dialog;
		if (flags & (ESTATE_ACCESS_ALLOWED_AGENT_ADD |
					 ESTATE_ACCESS_BANNED_AGENT_ADD))
		{
			dialog = listed_names == 1 ? "AgentWasAddedToList"
									   : "AgentsWereAddedToList";
		}
		else if (flags & (ESTATE_ACCESS_ALLOWED_AGENT_REMOVE |
						  ESTATE_ACCESS_BANNED_AGENT_REMOVE))
		{
			dialog = listed_names == 1 ? "AgentWasRemovedFromList"
									   : "AgentsWereRemovedFromList";
		}
		if (!dialog.empty())
		{
			gNotifications.add(dialog, args);
		}
	}

	self->setPendingUpdate(true);

	return false;
}

// key = "estateaccessdelta"
// str(estate_id) will be added to front of list by
//                forward_EstateOwnerRequest_to_dataserver
// str[0] = str(agent_id) requesting the change
// str[1] = str(flags) (ESTATE_ACCESS_DELTA_*)
// str[2] = str(agent_id) to add or remove
//static
void LLPanelEstateAccess::sendEstateAccessDelta(U32 flags, const LLUUID& id)
{
	LLMessageSystem* msg = gMessageSystemp;
	if (!msg) return;

	msg->newMessage(_PREHASH_EstateOwnerMessage);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->addUUIDFast(_PREHASH_TransactionID, LLUUID::null); // not used

	msg->nextBlock(_PREHASH_MethodData);
	msg->addString(_PREHASH_Method, "estateaccessdelta");
	msg->addUUID(_PREHASH_Invoice, LLFloaterRegionInfo::getLastInvoice());

	std::string buf;
	gAgentID.toString(buf);
	msg->nextBlock(_PREHASH_ParamList);
	msg->addString(_PREHASH_Parameter, buf);

	buf = llformat("%u", flags);
	msg->nextBlock(_PREHASH_ParamList);
	msg->addString(_PREHASH_Parameter, buf);

	id.toString(buf);
	msg->nextBlock(_PREHASH_ParamList);
	msg->addString(_PREHASH_Parameter, buf);

	gAgent.sendReliableMessage();

	// This was part of the old pre-capability code, so do it when the
	// capability is not in use; the deleteAllItems() are disabled in the
	// LLDispatchSetEstateAccess() code for the capability-less case (likely
	// because we could receive several UDP packets, each contaning a part of
	// the full list), so we need to deleteAllItems() here instead... HB.
	if ((flags & (ESTATE_ACCESS_ALLOWED_AGENT_ADD |
				  ESTATE_ACCESS_ALLOWED_AGENT_REMOVE |
		          ESTATE_ACCESS_BANNED_AGENT_ADD |
				  ESTATE_ACCESS_BANNED_AGENT_REMOVE)) &&
		!gAgent.hasRegionCapability("EstateAccess"))
	{

		LLPanelEstateAccess* self = LLFloaterRegionInfo::getPanelAccess();
		if (self)
		{
			self->mAllowedAvatars->deleteAllItems();
			self->mBannedAvatars->deleteAllItems();
		}
	}
}

void LLPanelEstateAccess::updateControls(LLViewerRegion* region)
{
	bool god_or_owner = gAgent.isGodlike() ||
						(region && region->getOwner() == gAgentID);
	bool can_control = god_or_owner || (region && region->isEstateManager());
	LL_DEBUGS("RegionInfo") << " - god_or_owner = "
							<< (god_or_owner ? "true" : "false")
			  				<< " - can_control = "
							<< (can_control ? "true" : "false") << LL_ENDL;
	setCtrlsEnabled(can_control);

	childSetEnabled("add_allowed_group_btn", can_control);
	childSetEnabled("remove_allowed_group_btn",
					can_control && mAllowedGroups->getFirstSelected());
	childSetEnabled("add_allowed_avatar_btn", can_control);
	childSetEnabled("remove_allowed_avatar_btn",
					can_control && mAllowedAvatars->getFirstSelected());
	childSetEnabled("add_banned_avatar_btn",  can_control);
	childSetEnabled("remove_banned_avatar_btn",
					can_control && mBannedAvatars->getFirstSelected());

	// Estate managers cannot add estate managers
	childSetEnabled("add_estate_manager_btn", god_or_owner);
	childSetEnabled("remove_estate_manager_btn",
					god_or_owner && mEstateManagers->getFirstSelected());
	childSetEnabled("estate_manager_name_list", god_or_owner);

	if (mCtrlsEnabled != can_control)
	{
		mCtrlsEnabled = can_control;
		// Update the lists on the agent's access level change
		updateLists();
	}
}

void LLPanelEstateAccess::setAccessAllowedEnabled(bool enable_agent,
												  bool enable_group,
												  bool enable_ban)
{
	LL_DEBUGS("RegionInfo") << "enable_agent = "
							<< (enable_agent ? "true" : "false")
							<< " - enable_group = "
							<< (enable_group ? "true" : "false")
							<< " - enable_ban = "
							<< (enable_ban ? "true" : "false") << LL_ENDL;
	childSetEnabled("allow_group_label", enable_group);
	childSetEnabled("add_allowed_group_btn", enable_group);
	childSetEnabled("remove_allowed_group_btn", enable_group);
	mAllowedGroups->setEnabled(enable_group);

	childSetEnabled("allow_resident_label", enable_agent);
	childSetEnabled("add_allowed_avatar_btn", enable_agent);
	childSetEnabled("remove_allowed_avatar_btn", enable_agent);
	mAllowedAvatars->setEnabled(enable_agent);

	childSetEnabled("ban_resident_label", enable_ban);
	childSetEnabled("add_banned_avatar_btn", enable_ban);
	childSetEnabled("remove_banned_avatar_btn", enable_ban);
	mBannedAvatars->setEnabled(enable_ban);

	// Update removal buttons if needed
	if (enable_group)
	{
		checkRemovalButton("allowed_group_name_list");
	}
	if (enable_agent)
	{
		checkRemovalButton("allowed_avatar_name_list");
	}
	if (enable_ban)
	{
		checkRemovalButton("banned_avatar_name_list");
	}
}

// Enables/disables the "remove" button for the various allow/ban lists
bool LLPanelEstateAccess::checkRemovalButton(std::string name)
{
	std::string btn_name;
	if (name == "allowed_avatar_name_list")
	{
		btn_name = "remove_allowed_avatar_btn";
	}
	else if (name == "allowed_group_name_list")
	{
		btn_name = "remove_allowed_group_btn";
	}
	else if (name == "banned_avatar_name_list")
	{
		btn_name = "remove_banned_avatar_btn";
	}
	else if (name == "estate_manager_name_list")
	{
		// ONLY OWNER CAN ADD / DELETE ESTATE MANAGER
		LLViewerRegion* region = gAgent.getRegion();
		if (region && region->getOwner() == gAgentID)
		{
			btn_name = "remove_estate_manager_btn";
		}
	}

	// Enable the remove button if something is selected
	LLNameListCtrl* name_list = getChild<LLNameListCtrl>(name.c_str(),
														 true, false);
	if (name_list && !btn_name.empty())
	{
		childSetEnabled(btn_name.c_str(),
						name_list->getFirstSelected() != NULL);
	}

	return !btn_name.empty();
}

///////////////////////////////////////////////////////////////////////////////
// LLPanelEstateCovenant
///////////////////////////////////////////////////////////////////////////////

LLPanelEstateCovenant::LLPanelEstateCovenant()
:	LLPanelRegionInfo(),
	mCovenantID(LLUUID::null)
{
}

//virtual
bool LLPanelEstateCovenant::postBuild()
{
	initHelpBtn("covenant_help", "HelpEstateCovenant");
	mEstateNameText = getChild<LLTextBox>("estate_name_text");
	mEstateOwnerText = getChild<LLTextBox>("estate_owner_text");
	mLastModifiedText = getChild<LLTextBox>("covenant_timestamp_text");
	mEditor = getChild<LLViewerTextEditor>("covenant_editor");
	mEditor->setHandleEditKeysDirectly(true);
	LLButton* reset_button = getChild<LLButton>("reset_covenant");
	reset_button->setEnabled(gAgent.canManageEstate());
	reset_button->setClickedCallback(resetCovenantID, this);

	return LLPanelRegionInfo::postBuild();
}

//virtual
bool LLPanelEstateCovenant::refreshFromRegion(LLViewerRegion* region)
{
	if (!region) return false;

	LLTextBox* region_name = getChild<LLTextBox>("region_name_text",
												 true, false);
	if (region_name)
	{
		region_name->setText(region->getName());
	}

	LLTextBox* resellable_clause = getChild<LLTextBox>("resellable_clause",
													   true, false);
	if (resellable_clause)
	{
		if (region->getRegionFlag(REGION_FLAGS_BLOCK_LAND_RESELL))
		{
			resellable_clause->setText(getString("can_not_resell"));
		}
		else
		{
			resellable_clause->setText(getString("can_resell"));
		}
	}

	LLTextBox* changeable_clause = getChild<LLTextBox>("changeable_clause",
													   true, false);
	if (changeable_clause)
	{
		if (region->getRegionFlag(REGION_FLAGS_ALLOW_PARCEL_CHANGES))
		{
			changeable_clause->setText(getString("can_change"));
		}
		else
		{
			changeable_clause->setText(getString("can_not_change"));
		}
	}

	LLTextBox* region_maturity = getChild<LLTextBox>("region_maturity_text",
													 true, false);
	if (region_maturity)
	{
		region_maturity->setText(region->getSimAccessString());
	}

	LLTextBox* region_landtype = getChild<LLTextBox>("region_landtype_text",
													 true, false);
	if (region_landtype)
	{
		region_landtype->setText(region->getSimProductName());
	}

	region->sendEstateCovenantRequest();

	// Call the parent for common book-keeping
	return LLPanelRegionInfo::refreshFromRegion(region);
}

//virtual
bool LLPanelEstateCovenant::estateUpdate(LLMessageSystem* msg)
{
	llinfos << "No operation..." << llendl;
	return true;
}

//virtual
bool LLPanelEstateCovenant::handleDragAndDrop(S32 x, S32 y, MASK mask,
											  bool drop,
											  EDragAndDropType cargo_type,
											  void* cargo_data,
											  EAcceptance* accept,
											  std::string& tooltip_msg)
{
	if (!gAgent.canManageEstate())
	{
		*accept = ACCEPT_NO;
		return true;
	}

	if (cargo_type == DAD_NOTECARD)
	{
		*accept = ACCEPT_YES_COPY_SINGLE;
		LLInventoryItem* item = (LLInventoryItem*)cargo_data;
		if (item && drop)
		{
			LLSD payload;
			payload["item_id"] = item->getUUID();
			gNotifications.add("EstateChangeCovenant", LLSD(), payload,
							   confirmChangeCovenantCallback);
		}
	}
	else
	{
		*accept = ACCEPT_NO;
	}

	return true;
}

//static
bool LLPanelEstateCovenant::confirmChangeCovenantCallback(const LLSD& notification,
														  const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	LLInventoryItem* item = gInventory.getItem(notification["payload"]["item_id"].asUUID());
	LLPanelEstateCovenant* self = LLFloaterRegionInfo::getPanelCovenant();
	if (!item || !self) return false;

	if (option == 0)
	{
		self->loadInvItem(item);
	}
	return false;
}

//static
void LLPanelEstateCovenant::resetCovenantID(void*)
{
	gNotifications.add("EstateChangeCovenant", LLSD(), LLSD(),
					   confirmResetCovenantCallback);
}

//static
bool LLPanelEstateCovenant::confirmResetCovenantCallback(const LLSD& notification,
														 const LLSD& response)
{
	LLPanelEstateCovenant* self = LLFloaterRegionInfo::getPanelCovenant();
	if (!self) return false;

	S32 option = LLNotification::getSelectedOption(notification, response);
	if (option == 0)
	{
		self->loadInvItem(NULL);
	}
	return false;
}

void LLPanelEstateCovenant::loadInvItem(LLInventoryItem* itemp)
{
	if (!gAssetStoragep)
	{
		llwarns << "No valid asset storage. Aborted." << llendl;
		return;
	}
	if (itemp)
	{
		gAssetStoragep->getInvItemAsset(gAgent.getRegionHost(),
										gAgentID, gAgentSessionID,
										itemp->getPermissions().getOwner(),
										LLUUID::null, itemp->getUUID(),
										itemp->getAssetUUID(),
										itemp->getType(), onLoadComplete,
										(void*)this, true); // high priority
		mAssetStatus = ASSET_LOADING;
	}
	else
	{
		mAssetStatus = ASSET_LOADED;
		setCovenantTextEditor("There is no Covenant provided for this Estate.");
		sendChangeCovenantID(LLUUID::null);
	}
}

//static
void LLPanelEstateCovenant::onLoadComplete(const LLUUID& asset_id,
										   LLAssetType::EType type,
										   void* user_data,
										   S32 status, LLExtStat)
{
	LLPanelEstateCovenant* panelp = (LLPanelEstateCovenant*)user_data;
	if (panelp)
	{
		if (0 == status)
		{
			LLFileSystem file(asset_id);

			S32 file_length = file.getSize();

			char* buffer = new char[file_length+1];
			if (!buffer)
			{
				llerrs << "Memory Allocation Failed" << llendl;
				return;
			}

			file.read((U8*)buffer, file_length);
			// put a EOS at the end
			buffer[file_length] = 0;

			if (file_length > 19 && !strncmp(buffer, "Linden text version", 19))
			{
				if (!panelp->mEditor->importBuffer(buffer, file_length + 1))
				{
					llwarns << "Problem importing estate covenant." << llendl;
					gNotifications.add("ProblemImportingEstateCovenant");
				}
				else
				{
					panelp->sendChangeCovenantID(asset_id);
				}
			}
			else
			{
				// Version 0 (just text, doesn't include version number)
				panelp->sendChangeCovenantID(asset_id);
			}
			delete[] buffer;
		}
		else
		{
			gViewerStats.incStat(LLViewerStats::ST_DOWNLOAD_FAILED);

			if (LL_ERR_ASSET_REQUEST_NOT_IN_DATABASE == status ||
				LL_ERR_FILE_EMPTY == status)
			{
				gNotifications.add("MissingNotecardAssetID");
			}
			else if (LL_ERR_INSUFFICIENT_PERMISSIONS == status)
			{
				gNotifications.add("NotAllowedToViewNotecard");
			}
			else
			{
				gNotifications.add("UnableToLoadNotecardAsset");
			}

			llwarns << "Problem loading notecard: " << status << llendl;
		}
		panelp->mAssetStatus = ASSET_LOADED;
		panelp->setCovenantID(asset_id);
	}
}

// key = "estatechangecovenantid"
// strings[0] = str(estate_id) (added by simulator before relay - not here)
// strings[1] = str(covenant_id)
void LLPanelEstateCovenant::sendChangeCovenantID(const LLUUID& asset_id)
{
	if (asset_id != getCovenantID())
	{
        setCovenantID(asset_id);

		LLMessageSystem* msg = gMessageSystemp;
		msg->newMessage(_PREHASH_EstateOwnerMessage);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
		msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
		msg->addUUIDFast(_PREHASH_TransactionID, LLUUID::null); // Not used

		msg->nextBlock(_PREHASH_MethodData);
		msg->addString(_PREHASH_Method, "estatechangecovenantid");
		msg->addUUID(_PREHASH_Invoice, LLFloaterRegionInfo::getLastInvoice());

		msg->nextBlock(_PREHASH_ParamList);
		msg->addString(_PREHASH_Parameter, getCovenantID().asString());
		gAgent.sendReliableMessage();
	}
}

//virtual
bool LLPanelEstateCovenant::sendUpdate()
{
	return true;
}

const std::string& LLPanelEstateCovenant::getEstateName() const
{
	return mEstateNameText->getText();
}

void LLPanelEstateCovenant::setEstateName(const std::string& name)
{
	mEstateNameText->setText(name);
}

//static
void LLPanelEstateCovenant::updateCovenantText(const std::string& string,
											   const LLUUID& asset_id)
{
	LLPanelEstateCovenant* panelp = LLFloaterRegionInfo::getPanelCovenant();
	if (panelp)
	{
		panelp->mEditor->setText(string);
		panelp->setCovenantID(asset_id);
	}
}

//static
void LLPanelEstateCovenant::updateEstateName(const std::string& name)
{
	LLPanelEstateCovenant* panelp = LLFloaterRegionInfo::getPanelCovenant();
	if (panelp)
	{
		panelp->mEstateNameText->setText(name);
	}
}

//static
void LLPanelEstateCovenant::updateLastModified(const std::string& text)
{
	LLPanelEstateCovenant* panelp = LLFloaterRegionInfo::getPanelCovenant();
	if (panelp)
	{
		panelp->mLastModifiedText->setText(text);
	}
}

//static
void LLPanelEstateCovenant::updateEstateOwnerName(const std::string& name)
{
	LLPanelEstateCovenant* panelp = LLFloaterRegionInfo::getPanelCovenant();
	if (panelp)
	{
		panelp->mEstateOwnerText->setText(name);
	}
}

const std::string& LLPanelEstateCovenant::getOwnerName() const
{
	return mEstateOwnerText->getText();
}

void LLPanelEstateCovenant::setOwnerName(const std::string& name)
{
	mEstateOwnerText->setText(name);
}

void LLPanelEstateCovenant::setCovenantTextEditor(const std::string& text)
{
	mEditor->setText(text);
}

///////////////////////////////////////////////////////////////////////////////
// LLPanelRegionExperiences
///////////////////////////////////////////////////////////////////////////////

// static
void* LLPanelRegionExperiences::createAllowedExperiencesPanel(void* data)
{
	LLPanelRegionExperiences* self = (LLPanelRegionExperiences*)data;
	self->mAllowed = new LLPanelExperienceListEditor();
	return self->mAllowed;
}

// static
void* LLPanelRegionExperiences::createTrustedExperiencesPanel(void* data)
{
	LLPanelRegionExperiences* self = (LLPanelRegionExperiences*)data;
	self->mTrusted = new LLPanelExperienceListEditor();
	return self->mTrusted;
}

// static
void* LLPanelRegionExperiences::createBlockedExperiencesPanel(void* data)
{
	LLPanelRegionExperiences* self = (LLPanelRegionExperiences*)data;
	self->mBlocked = new LLPanelExperienceListEditor();
	return self->mBlocked;
}

LLPanelRegionExperiences::LLPanelRegionExperiences()
:	LLPanelRegionInfo()
{
	LLCallbackMap::map_t factory_map;
	factory_map["panel_allowed"] = LLCallbackMap(createAllowedExperiencesPanel,
												 this);
	factory_map["panel_trusted"] = LLCallbackMap(createTrustedExperiencesPanel,
												 this);
	factory_map["panel_blocked"] = LLCallbackMap(createBlockedExperiencesPanel,
												 this);
	LLUICtrlFactory::getInstance()->buildPanel(this,
											   "panel_region_experiences.xml",
											   &factory_map);
}

bool LLPanelRegionExperiences::postBuild()
{
	if (!mAllowed || !mTrusted || !mBlocked) return false;

	setupList(mAllowed, "panel_allowed",
			  ESTATE_EXPERIENCE_ALLOWED_ADD, ESTATE_EXPERIENCE_ALLOWED_REMOVE);
	setupList(mTrusted, "panel_trusted",
			  ESTATE_EXPERIENCE_TRUSTED_ADD, ESTATE_EXPERIENCE_TRUSTED_REMOVE);
	setupList(mBlocked, "panel_blocked",
			  ESTATE_EXPERIENCE_BLOCKED_ADD, ESTATE_EXPERIENCE_BLOCKED_REMOVE);

	getChild<LLPanel>("help_text_layout_panel")->setVisible(true);
	getChild<LLPanel>("trusted_layout_panel")->setVisible(true);
	mTrusted->getChild<LLTextBox>("text_name")->setToolTip(getString("trusted_estate_text"));
	mAllowed->getChild<LLTextBox>("text_name")->setToolTip(getString("allowed_estate_text"));
	mBlocked->getChild<LLTextBox>("text_name")->setToolTip(getString("blocked_estate_text"));

	// Note: no apply button, so we do not call LLPanelRegionInfo::postBuild()
	return true;
}

void LLPanelRegionExperiences::setupList(LLPanelExperienceListEditor* panel,
										 const std::string& control_name,
										 U32 add_id, U32 remove_id)
{
	if (!panel) return;

	panel->getChild<LLTextBox>("text_name")->setText(panel->getString(control_name));
	panel->setMaxExperienceIDs(ESTATE_MAX_EXPERIENCE_IDS);
	panel->setAddedCallback(boost::bind(&LLPanelRegionExperiences::itemChanged,
										this, add_id, _1));
	panel->setRemovedCallback(boost::bind(&LLPanelRegionExperiences::itemChanged,
										  this, remove_id, _1));
}

void LLPanelRegionExperiences::processResponse(const LLSD& content)
{
	if (content.has("default"))
	{
		mDefaultExperience = content["default"].asUUID();
	}

	mAllowed->setExperienceIds(content["allowed"]);
	mBlocked->setExperienceIds(content["blocked"]);

	LLSD trusted = content["trusted"];
	if (mDefaultExperience.notNull())
	{
		mTrusted->setStickyFunction(boost::bind(LLExperienceCache::FilterMatching,
												_1, mDefaultExperience));
		trusted.append(mDefaultExperience);
	}

	mTrusted->setExperienceIds(trusted);

	mAllowed->refreshExperienceCounter();
	mBlocked->refreshExperienceCounter();
	mTrusted->refreshExperienceCounter();
}

// Used for both access add and remove operations, depending on the flag passed
// in (ESTATE_EXPERIENCE_ALLOWED_ADD, ESTATE_EXPERIENCE_ALLOWED_REMOVE, etc.)
//static
bool LLPanelRegionExperiences::experienceCoreConfirm(const LLSD& notification,
													 const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	const U32 orig_flags = (U32)notification["payload"]["operation"].asInteger();

	LLViewerRegion* region = gAgent.getRegion();

	for (LLSD::array_const_iterator
			iter = notification["payload"]["allowed_ids"].beginArray(),
			end_it = notification["payload"]["allowed_ids"].endArray();
		 iter != end_it; ++iter)
	{
		U32 flags = orig_flags;
		if (iter + 1 != end_it)
		{
			flags |= ESTATE_ACCESS_NO_REPLY;
		}

		const LLUUID id = iter->asUUID();
		switch (option)
		{
			case 0:
			    // This estate
			    sendEstateExperienceDelta(flags, id);
			    break;

			case 1:
			{
				// All estates, either than I own or manage for this owner.
				// This will be verified on simulator. JC
				if (!region) break;
				if (region->getOwner() == gAgentID || gAgent.isGodlike())
				{
					flags |= ESTATE_ACCESS_APPLY_TO_ALL_ESTATES;
					sendEstateExperienceDelta(flags, id);
				}
				else if (region->isEstateManager())
				{
					flags |= ESTATE_ACCESS_APPLY_TO_MANAGED_ESTATES;
					sendEstateExperienceDelta(flags, id);
				}
				break;
			}

			default:
			    break;
		}
	}
	return false;
}

// Send the actual "estateexperiencedelta" message
void LLPanelRegionExperiences::sendEstateExperienceDelta(U32 flags,
														 const LLUUID& experience_id)
{
	LLPanelRegionExperiences* panel =
		LLFloaterRegionInfo::getPanelExperiences();
	if (panel)
	{
		strings_t str(3, std::string());
		gAgentID.toString(str[0]);
		str[1] = llformat("%u", flags);
		experience_id.toString(str[2]);
		panel->sendEstateOwnerMessage("estateexperiencedelta", str);
	}
}

void LLPanelRegionExperiences::infoCallback(LLHandle<LLPanelRegionExperiences> handle,
											const LLSD& content)
{
	if (handle.isDead()) return;

	LLPanelRegionExperiences* floater = handle.get();
	if (floater)
	{
		floater->processResponse(content);
	}
}

//static
const std::string& LLPanelRegionExperiences::regionCapabilityQuery(LLViewerRegion* region,
																   const char* cap)
{
	return region ? region->getCapability(cap) : LLStringUtil::null;
}

//virtual
bool LLPanelRegionExperiences::refreshFromRegion(LLViewerRegion* region)
{
	if (!region) return false;

	bool allow_modify = gAgent.isGodlike() ||
						(region && region->canManageEstate());

	mAllowed->setDisabled(false);
	mAllowed->setReadonly(!allow_modify);
	mAllowed->loading();
	// Remove grid-wide experiences
	mAllowed->addFilter(boost::bind(LLExperienceCache::FilterWithProperty,
									_1, LLExperienceCache::PROPERTY_GRID));
	// Remove default experience
	mAllowed->addFilter(boost::bind(LLExperienceCache::FilterMatching,
						_1, mDefaultExperience));

	mBlocked->setDisabled(false);
	mBlocked->setReadonly(!allow_modify);
	mBlocked->loading();
	// Only grid-wide experiences
	mBlocked->addFilter(boost::bind(LLExperienceCache::FilterWithoutProperty,
									_1, LLExperienceCache::PROPERTY_GRID));
	// But not privileged ones
	mBlocked->addFilter(boost::bind(LLExperienceCache::FilterWithProperty,
									_1, LLExperienceCache::PROPERTY_PRIVILEGED));
	// Remove default experience
	mBlocked->addFilter(boost::bind(LLExperienceCache::FilterMatching,
									_1, mDefaultExperience));

	mTrusted->setDisabled(false);
	mTrusted->setReadonly(!allow_modify);
	mTrusted->loading();

	LLExperienceCache* exp = LLExperienceCache::getInstance();
	exp->getRegionExperiences(boost::bind(&LLPanelRegionExperiences::regionCapabilityQuery,
										  region, _1),
							  boost::bind(&LLPanelRegionExperiences::infoCallback,
										  getDerivedHandle<LLPanelRegionExperiences>(),
										  _1));

	// Call the parent for common book-keeping
	return LLPanelRegionInfo::refreshFromRegion(region);
}

LLSD LLPanelRegionExperiences::addIds(LLPanelExperienceListEditor* panel)
{
	LLSD ids;
	const uuid_list_t& id_list = panel->getExperienceIds();
	for (uuid_list_t::const_iterator it = id_list.begin(),
									 end = id_list.end();
		 it != end; ++it)
	{
		ids.append(*it);
	}
	return ids;
}

bool LLPanelRegionExperiences::sendUpdate()
{
	if (!gAgent.hasRegionCapability("RegionExperiences")) return false;

	LLSD content;
	content["allowed"] = addIds(mAllowed);
	content["blocked"] = addIds(mBlocked);
	content["trusted"] = addIds(mTrusted);
	LLExperienceCache* exp = LLExperienceCache::getInstance();
	exp->setRegionExperiences(boost::bind(&LLPanelRegionExperiences::regionCapabilityQuery,
										 gAgent.getRegion(), _1), content,
							  boost::bind(&LLPanelRegionExperiences::infoCallback,
										  getDerivedHandle<LLPanelRegionExperiences>(),
										  _1));
	return true;
}

void LLPanelRegionExperiences::itemChanged(U32 event_type, const LLUUID& id)
{
	std::string dialog_name;
	switch (event_type)
	{
		case ESTATE_EXPERIENCE_ALLOWED_ADD:
			dialog_name = "EstateAllowedExperienceAdd";
			break;

		case ESTATE_EXPERIENCE_ALLOWED_REMOVE:
			dialog_name = "EstateAllowedExperienceRemove";
			break;

		case ESTATE_EXPERIENCE_TRUSTED_ADD:
			dialog_name = "EstateTrustedExperienceAdd";
			break;

		case ESTATE_EXPERIENCE_TRUSTED_REMOVE:
			dialog_name = "EstateTrustedExperienceRemove";
			break;

		case ESTATE_EXPERIENCE_BLOCKED_ADD:
			dialog_name = "EstateBlockedExperienceAdd";
			break;

		case ESTATE_EXPERIENCE_BLOCKED_REMOVE:
			dialog_name = "EstateBlockedExperienceRemove";
			break;

		default:
			return;
	}

	LLSD payload;
	payload["operation"] = (S32)event_type;
	payload["dialog_name"] = dialog_name;
	payload["allowed_ids"].append(id);

	LLSD args;
	args["ALL_ESTATES"] = LLPanelEstateAccess::allEstatesText();

	LLNotification::Params p(dialog_name);
	p.substitutions(args).payload(payload)
						 .functor(boost::bind(&LLPanelRegionExperiences::experienceCoreConfirm,
											  _1, _2));
	if (LLPanelEstateInfo::isLindenEstate())
	{
		gNotifications.forceResponse(p, 0);
	}
	else
	{
		gNotifications.add(p);
	}

	onChangeAnything(NULL, this);
}
