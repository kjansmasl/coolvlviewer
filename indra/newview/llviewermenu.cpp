/**
 * @file llviewermenu.cpp
 * @brief Builds menus out of items.
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

#include "llviewerprecompiledheaders.h"

#include "boost/tokenizer.hpp"
#include "cef/dullahan.h"			// For CHROME_VERSION_MAJOR
#include "curl/curlver.h"

#include "llviewermenu.h"

#include "imageids.h"
#include "llaudioengine.h"
#include "llavatarjoint.h"
#include "llassetstorage.h"
#include "llcachename.h"
#include "llcallbacklist.h"
#include "llclipboard.h"
#include "llconsole.h"
#include "lldir.h"
#include "lleconomy.h"
#include "hbexternaleditor.h"
#include "llfeaturemanager.h"
#include "hbfileselector.h"
#include "llimagebmp.h"
#include "llimagej2c.h"
#include "llimagejpeg.h"
#include "llimagepng.h"
#include "llimagetga.h"
#include "llinstantmessage.h"
#include "llkeyboard.h"
#include "lllocale.h"
#include "llmemberlistener.h"
#include "llmenugl.h"
#include "llmimetypes.h"
#include "llmotioncontroller.h"
#include "llnotifications.h"
#include "llparcel.h"
#include "llpermissionsflags.h"
#include "llprimitive.h"
#include "llregionhandle.h"
#include "llrender.h"					// For LLRender::sGLCoreProfile
#include "llsdserialize.h"
#include "llsdutil.h"
#include "lltrans.h"
#include "lltransactiontypes.h"
#include "lluictrl.h"
#include "lluictrlfactory.h"
#include "lluploaddialog.h"
#include "llview.h"
#include "llvolume.h"
#include "llvolumemgr.h"
#include "llvorbisencode.h"
#include "llwindow.h"					// For gDebugClicks & gDebugWindowProc
#include "llxfermanager.h"
#include "object_flags.h"

#include "llagent.h"
#include "llagentpilot.h"
#include "llagentwearables.h"
#include "llappearancemgr.h"
#include "llappviewer.h"
#include "llavataractions.h"
#include "llavatartracker.h"
#include "llchatbar.h"
#include "llcommandhandler.h"
#include "lldebugview.h"
#include "lldrawable.h"
#include "lldrawpoolalpha.h"
#include "lldrawpooltree.h"
#include "llenvironment.h"
#include "llface.h"
#include "llfasttimerview.h"			// For HBTracyProfiler
#include "llfirstuse.h"
#include "llfloaterabout.h"
#include "llfloateractivespeakers.h"
#include "llfloateranimpreview.h"
#include "hbfloaterareasearch.h"
#include "llfloateravatarinfo.h"
#include "llfloateravatartextures.h"
#include "llfloaterbeacons.h"
#include "hbfloaterbump.h"
#include "llfloaterbuy.h"
#include "llfloaterbuycontents.h"
#include "llfloaterbuycurrency.h"
#include "llfloaterbuyland.h"
#include "llfloatercamera.h"
#include "llfloaterchat.h"
#include "llfloaterchatterbox.h"
#include "llfloatercustomize.h"
#include "llfloaterdebugsettings.h"
#include "hbfloaterdebugtags.h"
#include "llfloaterdisplayname.h"
#include "hbfloatereditenvsettings.h"
#include "llfloatereditui.h"
#include "llfloaterexperiences.h"
#include "llfloaterfriends.h"
#include "llfloatergesture.h"
#include "llfloatergodtools.h"
#include "llfloatergroupinfo.h"
#include "llfloatergroupinvite.h"
#include "llfloatergroups.h"
#include "hbfloatergrouptitles.h"
#include "llfloaterimagepreview.h"
#include "llfloaterinspect.h"
#include "llfloaterinventory.h"
#include "llfloaterlagmeter.h"
#include "llfloaterland.h"
#include "llfloaterlandholdings.h"
#include "llfloatermediabrowser.h"
#include "slfloatermediafilter.h"
#include "llfloaterminimap.h"
#include "llfloatermodelpreview.h"
#include "llfloatermove.h"
#include "llfloatermute.h"
#include "llfloaternearbymedia.h"
#include "llfloaternotificationsconsole.h"
#include "llfloateropenobject.h"
#include "llfloaterpathfindingcharacters.h"
#include "llfloaterpathfindinglinksets.h"
#include "llfloaterpay.h"
#include "llfloaterperms.h"
#include "llfloaterpreference.h"
#include "hbfloaterradar.h"
#include "llfloaterregiondebugconsole.h"
#include "llfloaterregioninfo.h"
#include "llfloaterreporter.h"
#include "hbfloaterrlv.h"
#include "llfloaterscriptdebug.h"
#include "llfloaterscriptqueue.h"
#include "hbfloatersearch.h"
#include "llfloatersnapshot.h"
#include "hbfloatersoundslist.h"
#include "llfloaterstats.h"
#include "hbfloaterteleporthistory.h"
#include "llfloatertools.h"
#include "hbfloateruploadasset.h"				// For HBFloaterUploadSound
#include "llfloaterwindlight.h"
#include "llfloaterworldmap.h"
#include "llfolderview.h"
#include "llgltfmateriallist.h"
#include "llgridmanager.h"
#include "llgroupmgr.h"
#include "llhoverview.h"
#include "llhudeffectspiral.h"
#include "llimmgr.h"
#include "llmeshrepository.h"
#include "llmorphview.h"
#include "llmutelist.h"
#include "hbobjectbackup.h"
#include "llpanellogin.h"
#include "llpanelobject.h"
#include "llpathfindingmanager.h"
#include "llpipeline.h"
#include "llpreviewmaterial.h"
#include "llpuppetmodule.h"
#include "llpuppetmotion.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llselectmgr.h"
#include "llsky.h"
#include "llstartup.h"
#include "llstatusbar.h"
#include "llsurfacepatch.h"
#include "lltexturecache.h"
#include "lltextureview.h"
#include "lltool.h"
#include "lltoolbar.h"
#include "lltoolcomp.h"
#include "lltoolface.h"
#include "lltoolfocus.h"
#include "lltoolgrab.h"
#include "lltoolmgr.h"
#include "lltoolpie.h"
#include "lltoolplacer.h"
#include "lltoolselectland.h"
#include "lluserauth.h"
#include "llvelocitybar.h"				// For gVelocityBarp
#include "llviewerassetupload.h"		// upload_new_resource(), gUploadQueue
#include "llvieweraudio.h"
#include "hbviewerautomation.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerdisplay.h"			// For gShaderProfileFrame
#include "llviewergesture.h"
#include "llviewerinventory.h"
#include "llviewerjoystick.h"
#include "llviewermessage.h"			// send_generic_message(), give_money()
#include "llviewerobjectexport.h"
#include "llviewerobjectlist.h"
#include "llviewerparcelmgr.h"
#include "llviewerparceloverlay.h"
#include "llviewerregion.h"
#include "llviewerstats.h"
#include "llviewertexturelist.h"		// For gTextureList
#include "llviewerwindow.h"
#include "llvoavatarself.h"
#include "llvotree.h"
#include "llvocache.h"					// For HB_AJUSTED_VOCACHE_PARAMETERS
#include "llvovolume.h"
#include "llweb.h"
#include "llwlskyparammgr.h"
#include "llworld.h"
#include "roles_constants.h"

// When set to non-zero, adds a "Test llerrs crash" entry to the Advanced menu
// (for error handling debugging only). HB
#define LL_ENABLE_CRASH_TEST 0

using namespace LLOldEvents;
using namespace LLAvatarAppearanceDefines;

//
// Globals
//

bool gMenusInitialized = false;

LLViewerMenuHolderGL* gMenuHolderp = NULL;
LLMenuBarGL* gMenuBarViewp = NULL;
LLMenuBarGL* gLoginMenuBarViewp = NULL;

// Pie menus
LLPieMenu* gPieSelfp = NULL;
LLPieMenu* gPieAvatarp = NULL;
LLPieMenu* gPieObjectp = NULL;
LLPieMenu* gPieAttachmentp = NULL;
LLPieMenu* gPieLandp = NULL;
LLPieMenu* gPieParticlep = NULL;
// Pie sub-menus
LLPieMenu* gAttachScreenPieMenup = NULL;
LLPieMenu* gAttachPieMenup = NULL;
LLPieMenu* gDetachPieMenup = NULL;
LLPieMenu* gDetachScreenPieMenup = NULL;
LLPieMenu* gMutesPieMenup = NULL;
LLPieMenu* gPieObjectMutep = NULL;

// Sub-menus
LLMenuGL* gAttachSubMenup = NULL;
LLMenuGL* gDetachSubMenup = NULL;

// Local constants.
static const std::string CLIENT_MENU_NAME("Advanced");
static const std::string SERVER_MENU_NAME("Admin");

typedef LLMemberListener<LLView> view_listener_t;

// Local prototypes (forward declarations)
void initialize_menus();
void init_client_menu(LLMenuGL* menu);
void init_server_menu(LLMenuGL* menu);

class LLMenuParcelObserver final : public LLParcelSelectionObserver
{
public:
	LLMenuParcelObserver();
	~LLMenuParcelObserver() override;
	void changed() override;

private:
	LLView* mLandBuyPass;
	LLView* mLanBuy;
	LLView* mBuyLand;
};

static LLMenuParcelObserver* gMenuParcelObserver = NULL;

LLMenuParcelObserver::LLMenuParcelObserver()
{
	mLandBuyPass = gMenuHolderp->getChild<LLView>("Land Buy Pass");
	mLanBuy = gMenuHolderp->getChild<LLView>("Land Buy");
	mBuyLand = gMenuHolderp->getChild<LLView>("Buy Land...");
	gViewerParcelMgr.addSelectionObserver(this);
}

LLMenuParcelObserver::~LLMenuParcelObserver()
{
	gViewerParcelMgr.removeSelectionObserver(this);
}

bool enable_buy_land(void*)
{
	LLParcel* parcelp = gViewerParcelMgr.getParcelSelection()->getParcel();
	return gViewerParcelMgr.canAgentBuyParcel(parcelp, false);
}

void LLMenuParcelObserver::changed()
{
	if (mLandBuyPass)
	{
		mLandBuyPass->setEnabled(LLPanelLandGeneral::enableBuyPass(NULL));
	}

	bool buyable = enable_buy_land(NULL);
	if (mLanBuy)
	{
		mLanBuy->setEnabled(buyable);
	}
	if (mBuyLand)
	{
		mBuyLand->setEnabled(buyable);
	}
}

// Called from llstartup.cpp
void set_underclothes_menu_options()
{
#if LL_TEEN_WERABLE_RESTRICTIONS
	if (!gAgent.isTeen())
	{
		return;
	}
	if (gMenuHolderp)
	{
		gMenuHolderp->getChild<LLView>("Self Underpants")->setVisible(false);
		gMenuHolderp->getChild<LLView>("Self Undershirt")->setVisible(false);
	}
	if (gMenuBarViewp)
	{
		gMenuBarViewp->getChild<LLView>("Menu Underpants")->setVisible(false);
		gMenuBarViewp->getChild<LLView>("Menu Undershirt")->setVisible(false);
	}
#endif
}

// Returns a pointer to the avatar give the UUID of the avatar OR of an
// attachment the avatar is wearing. Returns NULL on failure.
LLVOAvatar* find_avatar_from_object(LLViewerObject* object)
{
	if (object)
	{
		if (object->isAttachment())
		{
			do
			{
				object = (LLViewerObject*) object->getParent();
			}
			while (object && !object->isAvatar());
		}
		else if (!object->isAvatar())
		{
			object = NULL;
		}
	}

	return (LLVOAvatar*)object;
}

// Returns a pointer to the avatar give the UUID of the avatar OR of an
// attachment the avatar is wearing. Returns NULL on failure.
LLVOAvatar* find_avatar_from_object(const LLUUID& object_id)
{
	return find_avatar_from_object(gObjectList.findObject(object_id));
}

// Code required to calculate anything about the menus
void pre_init_menus()
{
	// static information
	LLColor4 color;
	color = gColors.getColor("MenuDefaultBgColor");
	LLMenuGL::setDefaultBackgroundColor(color);
	color = gColors.getColor("MenuItemEnabledColor");
	LLMenuItemGL::setEnabledColor(color);
	color = gColors.getColor("MenuItemDisabledColor");
	LLMenuItemGL::setDisabledColor(color);
	color = gColors.getColor("MenuItemHighlightBgColor");
	LLMenuItemGL::setHighlightBGColor(color);
	color = gColors.getColor("MenuItemHighlightFgColor");
	LLMenuItemGL::setHighlightFGColor(color);
}

bool enable_picker_actions(void*)
{
	return !HBFileSelector::isInUse();
}

class LLSampleFloater final : public LLFloater
{
public:
	LLSampleFloater(const std::string& name)
	:	LLFloater(name),
		mPanelp(NULL)
	{
	}

	~LLSampleFloater() override
	{
		if (mPanelp)
		{
			delete mPanelp;
			mPanelp = NULL;
		}
	}

public:
	LLPanel* mPanelp;
};

void load_from_xml_callback(HBFileSelector::ELoadFilter type,
							std::string& filename,
							void* user_data)
{
	if (!filename.empty())
	{
		LLSampleFloater* floater = new LLSampleFloater("sample_floater");
		if (LLUICtrlFactory::getInstance()->buildFloater(floater, filename))
		{
			// Make sure the floater can be closed !
			floater->setCanClose(true);
		}
		else
		{
			// It is not a floater... Maybe a panel ?
			delete floater;	// do not keep the failed build attempt
			floater = new LLSampleFloater("sample_floater");
			LLPanel* panel = new LLPanel("sample_panel");
			floater->mPanelp = panel;
			if (LLUICtrlFactory::getInstance()->buildPanel(panel,
														   filename,
														   &panel->getFactoryMap()))
			{
				if (!panel->hasBorder())
				{
					panel->addBorder();
				}
				panel->setUseBoundingRect(true);
				panel->updateBoundingRect();
				LLRect rect = panel->getBoundingRect();
				rect.setOriginAndSize(0, 0, rect.getWidth() + 64,
									  rect.getHeight() + 64 +
									  LLFLOATER_HEADER_SIZE);
				floater->initFloater(filename, false,
									 rect.getWidth(), rect.getHeight(),
									 false, true, true);
				floater->setRect(rect);
				floater->setTitleVisible(true);
				floater->addChild(panel);
				rect.mTop -= LLFLOATER_HEADER_SIZE;
				panel->centerWithin(rect);
				panel->setBorderVisible(true);
				floater->center();
				floater->open();
			}
			else
			{
				gNotifications.add("NotAFloater");
				delete floater;
			}
		}
	}
}

void handle_load_from_xml(void*)
{
	HBFileSelector::loadFile(HBFileSelector::FFLOAD_XUI,
							 load_from_xml_callback);
}

void handle_debug_tags(void*)
{
	HBFloaterDebugTags::showInstance();
}

void update_upload_costs_in_menus()
{
	if (!gMenuHolderp) return;

	LLEconomy* economyp = LLEconomy::getInstance();

	S32 upload_cost = economyp->getTextureUploadCost();
	if (upload_cost >= 0)
	{
		std::string cost = llformat("%d", upload_cost);
		gMenuHolderp->childSetLabelArg("Upload Image", "[COST]", cost);
		gMenuHolderp->childSetLabelArg("Upload Material", "[COST]", cost);
	}
	else
	{
		gMenuHolderp->childSetLabelArg("Upload Image", "[COST]", "?");
		gMenuHolderp->childSetLabelArg("Upload Material", "[COST]", "?");
	}

	upload_cost = economyp->getSoundUploadCost();
	if (upload_cost >= 0)
	{
		gMenuHolderp->childSetLabelArg("Upload Sound", "[COST]",
									   llformat("%d", upload_cost));
	}
	else
	{
		gMenuHolderp->childSetLabelArg("Upload Sound", "[COST]", "?");
	}

	upload_cost = economyp->getAnimationUploadCost();
	if (upload_cost >= 0)
	{
		gMenuHolderp->childSetLabelArg("Upload Animation", "[COST]",
									   llformat("%d", upload_cost));
	}
	else
	{
		gMenuHolderp->childSetLabelArg("Upload Animation", "[COST]", "?");
	}

	upload_cost = economyp->getPriceUpload();
	if (upload_cost >= 0)
	{
		gMenuHolderp->childSetLabelArg("Bulk Upload", "[COST]",
									   llformat("%d", upload_cost));
	}
	else
	{
		gMenuHolderp->childSetLabelArg("Bulk Upload", "[COST]", "?");
	}
}

void init_menus()
{
	S32 top = gViewerWindowp->getRootView()->getRect().getHeight();
	S32 width = gViewerWindowp->getRootView()->getRect().getWidth();

	//
	// Main menu bar
	//

	gMenuHolderp = new LLViewerMenuHolderGL();
	gMenuHolderp->setRect(LLRect(0, top, width, 0));
	gMenuHolderp->setFollowsAll();

	LLMenuGL::sMenuContainer = gMenuHolderp;

	// Initialize actions
	initialize_menus();

	LLUICtrlFactory* ui_factory = LLUICtrlFactory::getInstance();

	//
	// Pie menus
	//

	gPieSelfp = ui_factory->buildPieMenu("menu_pie_self.xml", gMenuHolderp);
	gDetachScreenPieMenup =
		gMenuHolderp->getChild<LLPieMenu>("Object Detach HUD");
	gDetachPieMenup = gMenuHolderp->getChild<LLPieMenu>("Object Detach");

	gPieAvatarp = ui_factory->buildPieMenu("menu_pie_avatar.xml",
										   gMenuHolderp);
	gMutesPieMenup = gMenuHolderp->getChild<LLPieMenu>("Mutes", true, false);

	gPieObjectp = ui_factory->buildPieMenu("menu_pie_object.xml",
										   gMenuHolderp);
	gPieObjectMutep = gMenuHolderp->getChild<LLPieMenu>("Mute Object Menu",
														true, false);
	gAttachScreenPieMenup =
		gMenuHolderp->getChild<LLPieMenu>("Object Attach HUD");
	gAttachPieMenup = gMenuHolderp->getChild<LLPieMenu>("Object Attach");

	gPieAttachmentp = ui_factory->buildPieMenu("menu_pie_attachment.xml",
											   gMenuHolderp);

	gPieLandp = ui_factory->buildPieMenu("menu_pie_land.xml", gMenuHolderp);

	gPieParticlep = ui_factory->buildPieMenu("menu_pie_particle.xml",
											 gMenuHolderp);
	new HBLuaPieMenu();

	//
	// Set up the colors
	//

	LLColor4 color = LLUI::sPieMenuBgColor;
	gPieSelfp->setBackgroundColor(color);
	gPieAvatarp->setBackgroundColor(color);
	gPieObjectp->setBackgroundColor(color);
	gPieAttachmentp->setBackgroundColor(color);
	gPieLandp->setBackgroundColor(color);
	gPieParticlep->setBackgroundColor(color);
	gLuaPiep->setBackgroundColor(color);

	color = gColors.getColor("MenuPopupBgColor");

	// If we are not in production, use a different color to make it apparent.
	if (gIsInProductionGrid)
	{
		color = gColors.getColor("MenuBarBgColor");
	}
	else
	{
		color = gColors.getColor("MenuNonProductionBgColor");
	}
	gMenuBarViewp = (LLMenuBarGL*)ui_factory->buildMenu("menu_viewer.xml",
													   gMenuHolderp);
	gMenuBarViewp->setRect(LLRect(0, top, 0, top - gMenuBarHeight));
	gMenuBarViewp->setBackgroundColor(color);

	gMenuBarViewp->arrange();

	gMenuHolderp->addChild(gMenuBarViewp);

	// Menu holder appears on top of menu bar so you can see the menu title
	// flash when an item is triggered (the flash occurs in the holder)
	gViewerWindowp->getRootView()->addChild(gMenuHolderp);

	gViewerWindowp->setMenuBackgroundColor();

	update_upload_costs_in_menus();

	gAttachSubMenup = gMenuBarViewp->getChildMenuByName("Attach Object", true);
	gDetachSubMenup = gMenuBarViewp->getChildMenuByName("Detach Object", true);

	LLMenuGL* menu = new LLMenuGL(CLIENT_MENU_NAME);
	init_client_menu(menu);
	gMenuBarViewp->appendMenu(menu);
	menu->updateParent(gMenuHolderp);

	menu = new LLMenuGL(SERVER_MENU_NAME);
	init_server_menu(menu);
	gMenuBarViewp->appendMenu(menu);
	menu->updateParent(gMenuHolderp);

	gMenuBarViewp->createJumpKeys();

	// Let land based option enable when parcel changes
	gMenuParcelObserver = new LLMenuParcelObserver();

	// Debug menu visiblity
	show_debug_menus();

	gLoginMenuBarViewp = (LLMenuBarGL*)ui_factory->buildMenu("menu_login.xml",
															gMenuHolderp);

	LLRect rect = gLoginMenuBarViewp->getRect();
	gLoginMenuBarViewp->setRect(LLRect(rect.mLeft, rect.mTop,
								gViewerWindowp->getRootView()->getRect().getWidth() -
								rect.mLeft, rect.mBottom));

	gLoginMenuBarViewp->setBackgroundColor(color);

	gMenuHolderp->addChild(gLoginMenuBarViewp);

	gMenusInitialized = true;
}

void handle_rebake_textures(void*)
{
	if (isAgentAvatarValid())
	{
		gAgentAvatarp->forceBakeAllTextures(true);
		if (LLVOAvatarSelf::canUseServerBaking())
		{
			gAgentAvatarp->computeBodySize(true);
			static LLCachedControl<bool> aggressive_rebake(gSavedSettings,
												 		   "AvatarAggressiveRebake");
			if (aggressive_rebake)
			{
				gAppearanceMgr.incrementCofVersion();
			}
			else
			{
				gAppearanceMgr.mNeedsSyncWearables = true;
				gAppearanceMgr.mNeedsSyncAttachments = true;
				// This trick will force a rebake even if no wearable or
				// attachment link gets updated.
				gAgentAvatarp->mLastUpdateRequestCOFVersion =
					LLViewerInventoryCategory::VERSION_UNKNOWN;
			}
		}
	}
}

void toggle_visibility(void* user_data)
{
	LLView* viewp = (LLView*)user_data;
	if (viewp)
	{
		viewp->setVisible(!viewp->getVisible());
	}
}

bool get_visibility(void* user_data)
{
	LLView* viewp = (LLView*)user_data;
	return viewp && viewp->getVisible();
}

void menu_toggle_control(void* user_data)
{
	std::string setting((char*)user_data);
	gSavedSettings.setBool(setting.c_str(),
						   !gSavedSettings.getBool(setting.c_str()));
}

bool menu_check_control(void* user_data)
{
	return gSavedSettings.getBool((char*)user_data);
}

void handle_show_debug_settings(void*)
{
	LLFloaterDebugSettings::showInstance();
}

#if TRACY_ENABLE
void handle_tracy_profiler(void*)
{
	if (!TracyIsConnected)
	{
		HBTracyProfiler::launch();
	}
}

bool tracy_not_connected(void*)
{
	return !TracyIsConnected;
}
#endif

void handle_show_notifications_console(void*)
{
	LLFloaterNotificationConsole::showInstance();
}

void handle_region_debug_console(void*)
{
	LLFloaterRegionDebugConsole::showInstance();
}

void handle_region_dump_settings(void*)
{
	LLViewerRegion* regionp = gAgent.getRegion();
	if (regionp)
	{
		regionp->dumpSettings();
	}
}

void handle_dump_capabilities_info(void*)
{
	LLViewerRegion* regionp = gAgent.getRegion();
	if (regionp)
	{
		regionp->logActiveCapabilities();
	}
}

void handle_dump_group_info(void*)
{
	llinfos << "Group:  " << gAgent.mGroupName << llendl;
	llinfos << "Id:	 " << gAgent.mGroupID << llendl;
	llinfos << "Powers: " << gAgent.mGroupPowers << llendl;
	llinfos << "Title:  " << gAgent.mGroupTitle << llendl;
}

void handle_dump_focus(void*)
{
	LLUICtrl* ctrl = gFocusMgr.getKeyboardFocusUICtrl();
	llinfos << "Keyboard focus " << (ctrl ? ctrl->getName() : "(none)")
			<< llendl;
}

void print_packets_lost(void*)
{
	gWorld.printPacketsLost();
}

void print_object_info(void*)
{
	gSelectMgr.selectionDump();
}

void dump_select_mgr(void*)
{
	gSelectMgr.dump();
}

void dump_cmd_handlers(void*)
{
	LLCommandHandler::dump();
}

void dump_stale_images(void*)
{
	LLImageGL::dumpStaleList();
}

void dump_inventory(void*)
{
	gInventory.dumpInventory();
}

void print_agent_nvpairs(void*)
{
	llinfos << "Agent name-value pairs:" << llendl;

	LLViewerObject* objectp = gObjectList.findObject(gAgentID);
	if (objectp)
	{
		objectp->printNameValuePairs();
	}
	else
	{
		llinfos << "Cannot find agent object" << llendl;
	}

	llinfos << "Camera at " << gAgent.getCameraPositionGlobal() << llendl;
}

void velocity_interpolate(void* data)
{
	bool toggle = gSavedSettings.getBool("VelocityInterpolate");
	LLMessageSystem* msg = gMessageSystemp;
	if (!toggle)
	{
		msg->newMessageFast(_PREHASH_VelocityInterpolateOn);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
		msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
		gAgent.sendReliableMessage();
		llinfos << "Velocity Interpolation On" << llendl;
	}
	else
	{
		msg->newMessageFast(_PREHASH_VelocityInterpolateOff);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
		msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
		gAgent.sendReliableMessage();
		llinfos << "Velocity Interpolation Off" << llendl;
	}
	if (data)
	{
		gSavedSettings.setBool((char*)data, !toggle);
	}
}

bool check_admin_override(void*)
{
	return gAgent.getAdminOverride();
}

void handle_admin_override_toggle(void*)
{
	gAgent.setAdminOverride(!gAgent.getAdminOverride());

	// The above may have affected which debug menus are visible
	show_debug_menus();
	if (gStatusBarp)
	{
		gStatusBarp->setDirty();
	}
}

void handle_god_mode(void*)
{
	gAgent.requestEnterGodMode();
}

void handle_leave_god_mode(void*)
{
	gAgent.requestLeaveGodMode();
}

bool enable_god_options(void*)
{
	bool may_be_linden = true;	// Linden or OpenSim admin
	if (isAgentAvatarValid() && gIsInSecondLife)
	{
		LLNameValue* lastname = gAgentAvatarp->getNVPair("LastName");
		if (lastname)
		{
			std::string name = lastname->getString();
			may_be_linden = name == "Linden";
		}
	}

	return may_be_linden;
}

bool enable_non_faked_god(void*)
{
	return gAgent.isGodlikeWithoutAdminMenuFakery();
}

bool enable_god_customer_service(void*)
{
	return gAgent.getGodLevel() >= GOD_CUSTOMER_SERVICE	&&
		   enable_god_options(NULL);
}

void handle_god_tools(void*)
{
	LLFloaterGodTools::showInstance();
}

bool enable_god_basic(void*)
{
	return gAgent.getGodLevel() > GOD_NOT;
}

bool check_message_logging(void*)
{
	return gMessageSystemp->mVerboseLog;
}

void handle_viewer_toggle_message_log(void*)
{
	LLMessageSystem* msg = gMessageSystemp;
	if (msg->mVerboseLog)
	{
		msg->stopLogging();
	}
	else
	{
		msg->startLogging();
	}
}

void save_settings_to_xml_callback(HBFileSelector::ESaveFilter filter,
								   std::string& filename,
								   void* user_data)
{
	S32 type = (S32)(intptr_t)user_data;
	if (!filename.empty())
	{
		if (type < 2)
		{
			gSavedSettings.saveToFile(filename, false, type == 1);
		}
		else
		{
			gSavedPerAccountSettings.saveToFile(filename, false, type == 3);
		}
	}
}

void handle_save_settings_to_xml(void* user_data)
{
	S32 type = (S32)(intptr_t)user_data;
	std::string suggestion;
	switch (type)
	{
		case 0:
			suggestion = "settings_coolvlviewer.xml";
			break;

		case 1:
			suggestion = "settings.xml";
			break;

		case 2:
			suggestion = "settings_per_account_coolvlviewer.xml";
			break;

		case 3:
			suggestion = "settings_per_account.xml";
			break;

		default:
			llwarns << "Bad type: " << type << llendl;
			return;
	}

	// Open the file save dialog
	HBFileSelector::saveFile(HBFileSelector::FFSAVE_XML, suggestion,
							 save_settings_to_xml_callback, user_data);
}

bool object_cache_enabled(void*)
{
	return LLVOCache::getInstance()->isEnabled();
}

bool object_cache_read_checked(void*)
{
	static LLCachedControl<bool> reads(gSavedSettings, "ObjectDiskCacheReads");
	return reads && object_cache_enabled(NULL);
}

bool object_cache_threaded_reads_checked(void*)
{
	static LLCachedControl<bool> threaded(gSavedSettings,
										  "ThreadedObjectCacheReads");
	return threaded && object_cache_enabled(NULL);
}

bool object_cache_write_enabled(void*)
{
	LLVOCache* cachep = LLVOCache::getInstance();
	return cachep->isEnabled() && !cachep->isReadOnly();
}

bool object_cache_write_checked(void*)
{
	static LLCachedControl<bool> writes(gSavedSettings,
										"ObjectDiskCacheWrites");
	return writes && object_cache_write_enabled(NULL);
}

bool object_cache_threaded_writes_checked(void*)
{
	static LLCachedControl<bool> threaded(gSavedSettings,
										  "ThreadedObjectCacheWrites");
	return threaded && object_cache_write_enabled(NULL);
}

bool in_sl(void*)
{
	return gIsInSecondLife;
}

bool not_in_sl(void*)
{
	return !gIsInSecondLife;
}

bool large_bakes_checked(void*)
{
	static LLCachedControl<bool> large_bakes(gSavedPerAccountSettings,
											 "OSUseLargeAvatarBakes");
	return !gIsInSecondLife && large_bakes;
}

void toggle_large_bakes(void*)
{
	bool b = gSavedPerAccountSettings.getBool("OSUseLargeAvatarBakes");
	gSavedPerAccountSettings.setBool("OSUseLargeAvatarBakes", !b);
}

bool http_inventory_checked(void*)
{
	static LLCachedControl<bool> http_inv(gSavedSettings, "UseHTTPInventory");
	return gIsInSecondLife || http_inv;
}

bool getmesh2_checked(void*)
{
	static LLCachedControl<bool> getmesh2(gSavedSettings, "UseGetMesh2Cap");
	return gIsInSecondLife || getmesh2;
}

bool viewerasset_checked(void*)
{
	static LLCachedControl<bool> viewerasset(gSavedSettings,
											 "UseViewerAssetCap");
	return gIsInSecondLife || viewerasset;
}

bool ais3_enabled(void*)
{
	return http_inventory_checked(NULL) &&
		   gAgent.hasRegionCapability("InventoryAPIv3");
}

bool ais3_checked(void*)
{
	static LLCachedControl<bool> use_ais(gSavedSettings, "UseAISForInventory");
	return use_ais && ais3_enabled(NULL);
}

bool ais3_fetch_checked(void*)
{
	static LLCachedControl<bool> use_ais(gSavedSettings, "UseAISForFetching");
	return use_ais && ais3_checked(NULL);
}

bool ais3_links_enabled(void*)
{
	static LLCachedControl<bool> use_ais(gSavedSettings, "UseAISForInventory");
	return gIsInSecondLife && !use_ais && ais3_enabled(NULL);
}

bool ais3_links_checked(void*)
{
	static LLCachedControl<bool> ais_links(gSavedSettings, "UseAISForLinksInSL");
	return gIsInSecondLife && ais_links && ais3_enabled(NULL);
}

bool agent_profile_enabled(void*)
{
	return gAgent.hasRegionCapability("AgentProfile");
}

bool agent_profile_checked(void*)
{
	static LLCachedControl<bool> use_cap(gSavedSettings, "UseAgentProfileCap");
	return use_cap && agent_profile_enabled(NULL);
}

#if LIBCURL_VERSION_MAJOR > 7 || LIBCURL_VERSION_MINOR >= 54
bool pipelining_enabled(void*)
{
	if (gIsInSecondLife)
	{
		static LLCachedControl<bool> sl_ok(gSavedSettings, "HttpPipeliningSL");
		return sl_ok;
	}
	static LLCachedControl<bool> os_ok(gSavedSettings, "HttpPipeliningOS");
	return os_ok;
}

bool http2_checked(void*)
{
	static LLCachedControl<bool> http2(gSavedSettings, "EnableHTTP2");
	return http2 && pipelining_enabled(NULL);
}
#endif

void restart_audio_engine(void*)
{
	gSavedSettings.setBool("NoAudio", false);
	LLStartUp::startAudioEngine();
}

#if LL_LINUX && LL_FMOD
bool fmod_enabled(void*)
{
	static LLCachedControl<bool> no_fmod(gSavedSettings, "AudioDisableFMOD");
	return !no_fmod;
}
#endif

void clear_asset_cache(void*)
{
	gSavedSettings.setBool("ClearAssetCache", true);
	gNotifications.add("AssetCacheWillClear");
}

void clear_inventory_cache(void*)
{
	gSavedPerAccountSettings.setBool("ClearInventoryCache", true);
	gNotifications.add("InventoryCacheWillClear");
}

void clear_texture_cache(void*)
{
	gSavedSettings.setBool("ClearTextureCache", true);
	gNotifications.add("TextureCacheWillClear");
}

void clear_object_cache(void*)
{
	gSavedSettings.setBool("ClearObjectCache", true);
	gNotifications.add("ObjectCacheWillClear");
}

bool can_write_caches(void*)
{
	return gAppViewerp && !gAppViewerp->isSecondInstanceSiblingViewer();
}

void load_automation_script_callback(HBFileSelector::ELoadFilter,
									 std::string& filename, void*)
{
	if (!filename.empty())
	{
		HBViewerAutomation::start(filename);
	}
}

void load_automation_script(void*)
{
	HBFileSelector::loadFile(HBFileSelector::FFLOAD_LUA,
							 load_automation_script_callback);
}

void reload_automation_script(void*)
{
	HBViewerAutomation::start();
}

void stop_automation(void*)
{
	HBViewerAutomation::cleanup();
}

void execute_lua_script_callback(HBFileSelector::ELoadFilter,
								 std::string& filename, void*)
{
	if (!filename.empty())
	{
		HBViewerAutomation::execute(filename);
	}
}

void execute_lua_script(void*)
{
	HBFileSelector::loadFile(HBFileSelector::FFLOAD_LUA,
							 execute_lua_script_callback);
}

void edit_automation_script(void*)
{
	// A simple editor launcher, without live file tracking. Declaring it here
	// as a static object will allow for self-cleaning on viewer exit.
	static HBExternalEditor editor(NULL);
	editor.kill();

	std::string error = "No Lua automation script found/configured.";
	std::string lua_script = gSavedSettings.getString("LuaAutomationScript");
	if (!lua_script.empty() && gDirUtilp)
	{
		lua_script = gDirUtilp->getExpandedFilename(LL_PATH_USER_SETTINGS,
													lua_script);
		if (LLFile::exists(lua_script))
		{
			if (editor.open(lua_script))
			{
				return;
			}
			error = editor.getErrorMessage();
		}
	}

	LLSD args;
	args["MESSAGE"]= error;
	gNotifications.add("GenericAlert", args);
}

bool hud_info_bg_enabled(void*)
{
	return !(gVelocityBarp && gVelocityBarp->getVisible());
}

bool hud_info_bg_checked(void*)
{
	static LLCachedControl<bool> hud_bg(gSavedSettings, "HUDInfoBackground");
	return hud_bg && hud_info_bg_enabled(NULL);
}

void handle_dump_followcam(void*)
{
	LLFollowCamMgr::dump();
}

void handle_dump_region_object_cache(void*)
{
	LLViewerRegion* regionp = gAgent.getRegion();
	if (regionp)
	{
		regionp->dumpCache();
	}
}

#if LL_ENABLE_CRASH_TEST
void handle_llerrs_test(void*)
{
	llerrs << "This is a volontary crash test..." << llendl;
}
#endif

void init_debug_console_menu(LLMenuGL* sub)
{
	sub->append(new LLMenuItemCheckGL("Texture console", toggle_visibility,
									  NULL, get_visibility,
									  (void*)gTextureViewp,
									  '3', MASK_CONTROL|MASK_SHIFT));
#if LL_FAST_TIMERS_ENABLED
	sub->append(new LLMenuItemCheckGL("Fast timers view", toggle_visibility,
									  NULL, get_visibility,
									  (void*)gFastTimerViewp,
									  '9', MASK_CONTROL|MASK_SHIFT));
	sub->append(new LLMenuItemCheckGL("Fast timers always enabled",
									   menu_toggle_control, NULL,
									   menu_check_control,
									   (void*)"FastTimersAlwaysEnabled"));
#endif
#if TRACY_ENABLE
	sub->append(new LLMenuItemCallGL("Launch Tracy profiler",
									  handle_tracy_profiler,
									  tracy_not_connected, NULL,
									  '8', MASK_CONTROL|MASK_SHIFT));
#endif
	sub->appendSeparator();

	LLView* debugview = gDebugViewp ? gDebugViewp->mDebugConsolep : NULL;
	sub->append(new LLMenuItemCheckGL("Debug console", toggle_visibility,
									  NULL, get_visibility, debugview,
									  '4', MASK_CONTROL|MASK_SHIFT));
	sub->append(new LLMenuItemToggleGL("Allow DEBUG messages",
									   &LLError::Log::sDebugMessages));
	sub->append(new LLMenuItemCheckGL("Precise timestamps in log file",
									  menu_toggle_control, NULL,
									  menu_check_control,
									  (void*)"PreciseLogTimestamps"));
	sub->append(new LLMenuItemCallGL("Debug tags", handle_debug_tags, NULL));

	LLMenuGL* sub2 = new LLMenuGL("Info to debug console");
	sub->appendMenu(sub2);
	sub2->append(new LLMenuItemCallGL("Region info",
									  handle_region_dump_settings));
	sub2->append(new LLMenuItemCallGL("Region object cache stats",
									  handle_dump_region_object_cache, NULL));
	sub2->append(new LLMenuItemCallGL("Capabilities info",
									  handle_dump_capabilities_info));
	sub2->append(new LLMenuItemCallGL("Group Info", handle_dump_group_info));
	sub2->append(new LLMenuItemCallGL("Packets lost info",
									  print_packets_lost));
	sub2->append(new LLMenuItemCallGL("Dump inventory", dump_inventory));
	sub2->append(new LLMenuItemCallGL("Dump selection manager",
									  dump_select_mgr));
	sub2->append(new LLMenuItemCallGL("Dump focus holder",
									  handle_dump_focus, NULL, NULL, 'F',
									  MASK_ALT | MASK_CONTROL | MASK_SHIFT));
	sub2->append(new LLMenuItemCallGL("Dump scripted camera",
									  handle_dump_followcam, NULL));
	sub2->append(new LLMenuItemCallGL("Selected object info",
									  print_object_info, NULL, NULL, 'P',
									  MASK_CONTROL | MASK_SHIFT));
	sub2->append(new LLMenuItemCallGL("Agent info", print_agent_nvpairs));
	sub2->append(new LLMenuItemCallGL("Registered command handlers",
									  dump_cmd_handlers));
	sub2->append(new LLMenuItemCallGL("Memory stats", output_statistics));
	sub2->append(new LLMenuItemCheckGL("Server UDP messages (spammy)",
				 &handle_viewer_toggle_message_log, NULL,
				 &check_message_logging, NULL));
	sub2->append(new LLMenuItemCallGL("Stale images list", dump_stale_images));
	sub2->createJumpKeys();

	sub->appendSeparator();

	// Debugging view for unified notifications
	sub->append(new LLMenuItemCallGL("Notifications console...",
				handle_show_notifications_console, NULL, NULL, '5',
				MASK_CONTROL | MASK_SHIFT));
	sub->append(new LLMenuItemCallGL("Region debug console",
				handle_region_debug_console, NULL, NULL, 'C',
				MASK_CONTROL | MASK_SHIFT));

	sub->createJumpKeys();
}

void init_hud_info_menu(LLMenuGL* sub)
{
	sub->append(new LLMenuItemCheckGL("Show velocity info", toggle_visibility,
									  NULL, get_visibility,
									  (void*)gVelocityBarp));

	sub->append(new LLMenuItemCheckGL("Show mesh queue", menu_toggle_control,
									  NULL, menu_check_control,
									  (void*)"DebugShowMeshQueue"));
	sub->append(new LLMenuItemToggleGL("Show camera info",
									   &gDisplayCameraPos));
	sub->append(new LLMenuItemToggleGL("Show FOV Info", &gDisplayFOV));
	sub->append(new LLMenuItemCheckGL("Show matrices", menu_toggle_control,
									  NULL, menu_check_control,
									  (void*)"DebugShowRenderMatrices"));
	sub->append(new LLMenuItemCheckGL("Show avatars render info",
									  menu_toggle_control, NULL,
									  menu_check_control,
									  (void*)"DebugShowAvatarRenderInfo"));
	sub->append(new LLMenuItemCheckGL("Show render info", menu_toggle_control,
									  NULL, menu_check_control,
									  (void*)"DebugShowRenderInfo"));
	sub->append(new LLMenuItemToggleGL("Show wind info", &gDisplayWindInfo));
	sub->append(new LLMenuItemCheckGL("Show time", menu_toggle_control, NULL,
									  menu_check_control,
								 	  (void*)"DebugShowTime"));
	sub->append(new LLMenuItemCheckGL("Show poll request age",
									  menu_toggle_control, NULL,
									  menu_check_control,
								 	  (void*)"DebugShowPollRequestAge"));
	sub->append(new LLMenuItemCheckGL("Show frame rate", menu_toggle_control,
									  NULL, menu_check_control,
									  (void*)"DebugShowFPS"));
	sub->append(new LLMenuItemCheckGL("Show color under cursor",
									  menu_toggle_control, NULL,
									  menu_check_control,
									 (void*)"DebugShowColor"));
	sub->appendSeparator();
	sub->append(new LLMenuItemCheckGL("Show info HUD background",
									  menu_toggle_control, hud_info_bg_enabled,
									  hud_info_bg_checked,
									  (void*)"HUDInfoBackground"));
	sub->createJumpKeys();
}

void init_lua_scripting_menu(LLMenuGL* sub)
{

	sub->append(new LLMenuItemCallGL("Load new automation script...",
									  load_automation_script));
	sub->append(new LLMenuItemCallGL("Re-load current automation script",
									  reload_automation_script));
	sub->append(new LLMenuItemCallGL("Stop current automation script",
									  stop_automation));
	sub->append(new LLMenuItemCallGL("Edit the automation script",
									  edit_automation_script));
	sub->append(new LLMenuItemCallGL("Execute a Lua script file...",
									  execute_lua_script));
	sub->append(new LLMenuItemCheckGL("Accept Lua from LSL scripts",
									   menu_toggle_control, NULL,
									   menu_check_control,
									   (void*)"LuaAcceptScriptCommands"));
# if LL_LINUX
	sub->append(new LLMenuItemCheckGL("Accept Lua commands from D-Bus",
									   menu_toggle_control, NULL,
									   menu_check_control,
									   (void*)"LuaAcceptDbusCommands"));
# endif
	sub->createJumpKeys();
}

bool tp_race_checked(void*)
{
	static LLCachedControl<bool> enabled(gSavedSettings,
										 "TPRaceWorkAroundInSL");
	return gIsInSecondLife && enabled;
}

void init_network_menu(LLMenuGL* sub)
{
	sub->append(new LLMenuItemCheckGL("Use web map tiles", menu_toggle_control,
									  NULL, menu_check_control,
									  (void*)"UseWebMapTiles"));

	sub->append(new LLMenuItemCheckGL("Use HTTP inventory fetches",
									  menu_toggle_control, not_in_sl,
									  http_inventory_checked,
									  (void*)"UseHTTPInventory"));
	sub->append(new LLMenuItemCheckGL("Use AISv3 protocol for inventory",
									  menu_toggle_control, ais3_enabled,
									  ais3_checked,
									  (void*)"UseAISForInventory"));
	sub->append(new LLMenuItemCheckGL("Use AISv3 for inventory fetches",
									  menu_toggle_control, ais3_checked,
									  ais3_fetch_checked,
									  (void*)"UseAISForFetching"));
	sub->append(new LLMenuItemCheckGL("Always use AISv3 to create links",
									  menu_toggle_control, ais3_links_enabled,
									  ais3_links_checked,
									  (void*)"UseAISForLinksInSL"));

	sub->append(new LLMenuItemCheckGL("Use HTTP group data fetches",
									  menu_toggle_control, NULL,
									  menu_check_control,
									  (void*)"UseHTTPGroupDataFetch"));

	sub->append(new LLMenuItemCheckGL("Use the AgentProfile capability",
									  menu_toggle_control,
									  agent_profile_enabled,
									  agent_profile_checked,
									  (void*)"UseAgentProfileCap"));
	sub->append(new LLMenuItemCheckGL("Use offline IMs fetch capability",
									  menu_toggle_control, NULL,
									  menu_check_control,
									  (void*)"UseOfflineIMsCapability"));
	sub->append(new LLMenuItemCheckGL("Use GetMesh2 capability for meshes",
									  menu_toggle_control, not_in_sl,
									  getmesh2_checked,
									  (void*)"UseGetMesh2Cap"));

	sub->append(new LLMenuItemCheckGL("Use ViewerAsset capability for assets",
									  menu_toggle_control, not_in_sl,
									  viewerasset_checked,
									  (void*)"UseViewerAssetCap"));

	sub->append(new LLMenuItemCheckGL("Get meshes retry delay from HTTP header",
									  menu_toggle_control, NULL,
									  menu_check_control,
									  (void*)"MeshUseHttpRetryAfter"));

	sub->append(new LLMenuItemCheckGL("Get textures retry delay from HTTP header",
									  menu_toggle_control, NULL,
									  menu_check_control,
									  (void*)"TextureRetryDelayFromHeader"));
#if LIBCURL_VERSION_MAJOR > 7 || LIBCURL_VERSION_MINOR >= 54
	sub->append(new LLMenuItemCheckGL("Use the HTTP/2 protocol",
									  menu_toggle_control, pipelining_enabled,
									  http2_checked, (void*)"EnableHTTP2"));
#endif
	sub->append(new LLMenuItemCheckGL("Disable HTTP range requests",
									  menu_toggle_control, NULL,
									  menu_check_control,
									  (void*)"HttpRangeRequestsDisable"));

	sub->appendSeparator();

	sub->append(new LLMenuItemCheckGL("Staged sim disabling",
									  menu_toggle_control, NULL,
									  menu_check_control,
									  (void*)"StagedSimDisabling"));

	sub->append(new LLMenuItemCheckGL("Clear stale texture fetches on TP",
									  menu_toggle_control, NULL,
									  menu_check_control,
									  (void*)"ClearStaleTextureFetchesOnTP"));
#if !LL_PENDING_MESH_REQUEST_SORTING
	sub->append(new LLMenuItemCheckGL("Delay pending mesh fetches on TP",
									  menu_toggle_control, NULL,
									  menu_check_control,
									  (void*)"DelayPendingMeshFetchesOnTP"));
#endif

	sub->append(new LLMenuItemCheckGL("TP race workaround",
									  menu_toggle_control, in_sl,
									  tp_race_checked,
									  (void*)"TPRaceWorkAroundInSL"));
	sub->appendSeparator();

	sub->append(new LLMenuItemCheckGL("Velocity interpolate objects",
									  velocity_interpolate, NULL,
									  menu_check_control,
									  (void*)"VelocityInterpolate"));
	sub->append(new LLMenuItemCheckGL("Ping interpolate object positions",
									  menu_toggle_control, NULL,
									  menu_check_control,
									  (void*)"PingInterpolate"));

	sub->append(new LLMenuItemCheckGL("Auto-kill bogus objects",
									  menu_toggle_control, NULL,
									  menu_check_control,
									  (void*)"KillBogusObjects"));

	sub->append(new LLMenuItemCheckGL("Ignore bogus kill-attachment messages",
									  menu_toggle_control,
									  NULL, menu_check_control,
									  (void*)"IgnoreOuterRegionAttachKill"));

	sub->appendSeparator();

	sub->append(new LLMenuItemToggleGL("Show object updates",
									   &gShowObjectUpdates));

	sub->createJumpKeys();
}

void init_caches_menu(LLMenuGL* sub)
{
	sub->append(new LLMenuItemCallGL("Clear group cache",
									 LLGroupMgr::debugClearAllGroups));

	sub->append(new LLMenuItemCallGL("Clear texture cache (after restart)",
									 clear_texture_cache, can_write_caches));

	sub->append(new LLMenuItemCallGL("Clear object cache (after restart)",
									 clear_object_cache, can_write_caches));

	sub->append(new LLMenuItemCallGL("Clear asset cache (after restart)",
									 clear_asset_cache, can_write_caches));

	sub->append(new LLMenuItemCallGL("Clear inventory cache (after restart)",
									 clear_inventory_cache));

	sub->appendSeparator();
	sub->append(new LLMenuItemCheckGL("Time-sliced texture cache purges",
									  menu_toggle_control, NULL,
									  menu_check_control,
									  (void*)"CachePurgeTimeSliced"));
	sub->appendSeparator();
	sub->append(new LLMenuItemCheckGL("Full region caching (after restart)",
									  menu_toggle_control, NULL,
									  menu_check_control,
									  (void*)"RequestFullRegionCache"));
	sub->append(new LLMenuItemCheckGL("Use object cache occlusion",
									  menu_toggle_control, NULL,
									  menu_check_control,
									  (void*)"UseObjectCacheOcclusion"));
#if HB_AJUSTED_VOCACHE_PARAMETERS
	sub->append(new LLMenuItemCheckGL("Bias objects retention",
									  menu_toggle_control, NULL,
									  menu_check_control,
									  (void*)"BiasedObjectRetention"));
#endif
	sub->append(new LLMenuItemToggleGL("Balance object cache",
									   &gBalanceObjectCache));
	sub->append(new LLMenuItemCheckGL("Force 360 degrees interest list",
									  menu_toggle_control, NULL,
									  menu_check_control,
									  (void*)"Use360InterestList"));
	sub->append(new LLMenuItemCheckGL("Object cache (after restart)",
									  menu_toggle_control, NULL,
									  menu_check_control,
									  (void*)"ObjectDiskCacheEnabled"));
	sub->append(new LLMenuItemCheckGL("Object cache disk reads",
									  menu_toggle_control,
									  object_cache_enabled,
									  object_cache_read_checked,
									  (void*)"ObjectDiskCacheReads"));
	sub->append(new LLMenuItemCheckGL("Threaded object cache reads",
									  menu_toggle_control,
									  object_cache_enabled,
									  object_cache_threaded_reads_checked,
									  (void*)"ThreadedObjectCacheReads"));
	sub->append(new LLMenuItemCheckGL("Object cache disk writes",
									  menu_toggle_control,
									  object_cache_write_enabled,
									  object_cache_write_checked,
									  (void*)"ObjectDiskCacheWrites"));
	sub->append(new LLMenuItemCheckGL("Threaded object cache writes",
									  menu_toggle_control,
									  object_cache_write_enabled,
									  object_cache_threaded_writes_checked,
									  (void*)"ThreadedObjectCacheWrites"));
#if LL_WINDOWS
	sub->appendSeparator();
	sub->append(new LLMenuItemCheckGL("Flush on asset write (for Wine)",
									  menu_toggle_control, NULL,
									  menu_check_control,
									  (void*)"FSFlushOnWrite"));
#endif
	sub->createJumpKeys();
}

void init_media_menu(LLMenuGL* sub)
{
	sub->append(new LLMenuItemCallGL("Restart audio engine",
									 &restart_audio_engine));
#if LL_OPENAL && LL_FMOD
	sub->append(new LLMenuItemCheckGL("Disable OpenAL", menu_toggle_control,
									  NULL, menu_check_control,
									  (void*)"AudioDisableOpenAL"));
	sub->append(new LLMenuItemCheckGL("Disable FMOD", menu_toggle_control,
									  NULL, menu_check_control,
									  (void*)"AudioDisableFMOD"));
#endif
#if LL_LINUX && LL_FMOD
	sub->append(new LLMenuItemCheckGL("Disable ALSA for FMOD",
									  menu_toggle_control, fmod_enabled,
									  menu_check_control,
									  (void*)"FMODDisableALSA"));
	sub->append(new LLMenuItemCheckGL("Disable PulseAudio for FMOD",
									  menu_toggle_control, fmod_enabled,
									  menu_check_control,
									  (void*)"FMODDisablePulseAudio"));
#endif
	sub->appendSeparator();

	sub->append(new LLMenuItemCallGL("Reload MIME types",
									 LLMIMETypes::reload));
	sub->appendSeparator();

	sub->append(new LLMenuItemCheckGL("Use a read thread for plugins",
									  menu_toggle_control, NULL,
									  menu_check_control,
									  (void*)"PluginUseReadThread"));

	sub->createJumpKeys();
}

void init_debug_world_menu(LLMenuGL* sub)
{
	sub->append(new LLMenuItemCheckGL("Sparse classic clouds updates",
									  menu_toggle_control, NULL,
									  menu_check_control,
									  (void*)"SparseClassicClouds"));
	sub->append(new LLMenuItemCheckGL("Show wind vectors",
									  LLPipeline::toggleRenderDebug, NULL,
									  LLPipeline::toggleRenderDebugControl,
									  (void*)LLPipeline::RENDER_DEBUG_WIND_VECTORS));
	sub->createJumpKeys();
}


//MK
void handle_rlv_status(void*)
{
	HBFloaterRLV::showInstance();
}

void toggle_rlv_focus(void* user_data)
{
	if (isAgentAvatarValid())
	{
		LLJoint* joint;
		S32 joint_code = (intptr_t)user_data;
		switch (joint_code)
		{
			case 1:
				joint = gAgentAvatarp->mTorsop;
				break;

			case 2:
				joint = gAgentAvatarp->mWristLeftp;
				break;

			case 3:
				joint = gAgentAvatarp->mWristRightp;
				break;

			case 4:
				joint = gAgentAvatarp->mFootLeftp;
				break;

			case 5:
				joint = gAgentAvatarp->mFootRightp;
				break;

			default:
				joint = gAgentAvatarp->mHeadp;
		}
		gRLInterface.setCamDistDrawFromJoint(joint);
	}
}

bool check_rlv_focus(void* user_data)
{
	if (isAgentAvatarValid())
	{
		LLJoint* joint = gRLInterface.getCamDistDrawFromJoint();
		S32 joint_code = (intptr_t)user_data;
		switch (joint_code)
		{
			case 0:
				return joint == gAgentAvatarp->mHeadp;

			case 1:
				return joint == gAgentAvatarp->mTorsop;

			case 2:
				return joint == gAgentAvatarp->mWristLeftp;

			case 3:
				return joint == gAgentAvatarp->mWristRightp;

			case 4:
				return joint == gAgentAvatarp->mFootLeftp;

			case 5:
				return joint == gAgentAvatarp->mFootRightp;

			default:
				break;
		}
	}

	return false;
}

void init_restrained_love_menu(LLMenuGL* menu)
{
	menu->append(new LLMenuItemCallGL("Restrictions and commands log",
									  handle_rlv_status, NULL));
	menu->appendSeparator();
	menu->append(new LLMenuItemCheckGL("Show '...' for muted text when deafened",
									   menu_toggle_control, NULL,
									   menu_check_control,
									   (void*)"RestrainedLoveShowEllipsis"));
	menu->append(new LLMenuItemCheckGL("Allow 'Wear' & 'Add to/Replace outfit'",
									   menu_toggle_control, NULL,
									   menu_check_control,
									   (void*)"RestrainedLoveAllowWear"));
	menu->append(new LLMenuItemCheckGL("Forbid give to #RLV/",
									   menu_toggle_control, NULL,
									   menu_check_control,
									   (void*)"RestrainedLoveForbidGiveToRLV"));
	menu->append(new LLMenuItemCheckGL("Add joint name to attachments in #RLV/",
									   menu_toggle_control, NULL,
									   menu_check_control,
									   (void*)"RestrainedLoveAutomaticRenameItems"));
	menu->append(new LLMenuItemCheckGL("@acceptpermission allows temp-attachments",
									   menu_toggle_control, NULL,
									   menu_check_control,
									   (void*)"RestrainedLoveRelaxedTempAttach"));
	menu->append(new LLMenuItemCheckGL("Skip blacklist checks for Lua scripts",
									   menu_toggle_control, NULL,
									   menu_check_control,
									   (void*)"RestrainedLoveLuaNoBlacklist"));

	LLMenuGL* sub = new LLMenuGL("Blindfold point of view");
	sub->append(new LLMenuItemCheckGL("Head",
									   toggle_rlv_focus, NULL,
									   check_rlv_focus,
									   (void*)0));
	sub->append(new LLMenuItemCheckGL("Pelvis",
									   toggle_rlv_focus, NULL,
									   check_rlv_focus,
									   (void*)1));
	sub->append(new LLMenuItemCheckGL("Left hand",
									   toggle_rlv_focus, NULL,
									   check_rlv_focus,
									   (void*)2));
	sub->append(new LLMenuItemCheckGL("Right hand",
									   toggle_rlv_focus, NULL,
									   check_rlv_focus,
									   (void*)3));
	sub->append(new LLMenuItemCheckGL("Left foot",
									   toggle_rlv_focus, NULL,
									   check_rlv_focus,
									   (void*)4));
	sub->append(new LLMenuItemCheckGL("Right foot",
									   toggle_rlv_focus, NULL,
									   check_rlv_focus,
									   (void*)5));
	sub->createJumpKeys();
	menu->appendMenu(sub);
}
//mk

#if 0	// Does not work particularly well at the moment
void reload_ui(void*)
{
	LLUICtrlFactory::getInstance()->rebuild();
}

void handle_reload_settings(void*)
{
	gSavedSettings.resetToDefaults();
	gSavedSettings.loadFromFile(gSavedSettings.getString("ClientSettingsFile"));

	llinfos << "Loading colors from colors_base.xml" << llendl;
	std::string color_file = gDirUtilp->getExpandedFilename(LL_PATH_APP_SETTINGS,
															"colors_base.xml");
	gColors.resetToDefaults();
	gColors.loadFromFileLegacy(color_file, false, TYPE_COL4U);
}
#endif

void fit_all_open_floaters(void*)
{
	gFloaterViewp->fitAllToScreen();
}

void decode_ui_sounds(void*)
{
	audio_preload_ui_sounds(true);
}

void clear_ui_sounds(void*)
{
	gSavedSettings.setBool("ClearSavedUISounds", true);
	gNotifications.add("SoundsWillClear");
}

void handle_font_test_floater(void*)
{
	LLFloater* floater = new LLFloater("font test");
	LLUICtrlFactory::getInstance()->buildFloater(floater,
												 "floater_font_test.xml");
	floater->center();
}

void handle_skin_preview_floater(void*)
{
	LLFloater* floater = new LLFloater("skin preview");
	LLUICtrlFactory* factoryp = LLUICtrlFactory::getInstance();
	factoryp->buildFloater(floater, "floater_skin_preview_template.xml");
}

void toggle_show_xui_names(void*)
{
	gSavedSettings.setBool("ShowXUINames",
						   !gSavedSettings.getBool("ShowXUINames"));
}

bool check_show_xui_names(void*)
{
	static LLCachedControl<bool> show_names(gSavedSettings, "ShowXUINames");
	return show_names;
}

void export_menus_to_xml_callback(HBFileSelector::ESaveFilter type,
								  std::string& filename, void* user_data)
{
	if (!filename.empty())
	{
		llofstream out(filename.c_str());
		if (out.is_open())
		{
			LLXMLNodePtr node = gMenuBarViewp->getXML();
			node->writeToOstream(out);
			out.close();
		}
		else
		{
			llwarns << "Could not open file '" << filename << "' for wirting."
					<< llendl;
		}
	}
}

void handle_export_menus_to_xml(void*)
{
	// Open the file save dialog
	HBFileSelector::saveFile(HBFileSelector::FFSAVE_XUI, "menu_bar.xml",
							 export_menus_to_xml_callback);
}

void save_to_xml_callback(HBFileSelector::ESaveFilter type,
						  std::string& filename,
						  void* user_data)
{
	LLFloater* frontmost = (LLFloater*)user_data;
	if (!filename.empty())
	{
		if (gFloaterViewp->bringToFront(frontmost))
		{
			LLUICtrlFactory::getInstance()->saveToXML(frontmost, filename);
		}
		else
		{
			gNotifications.add("NoFrontmostFloater");
		}
	}
}

void handle_save_to_xml(void*)
{
	LLFloater* frontmost = gFloaterViewp->getFrontmost();
	if (!frontmost)
	{
		gNotifications.add("NoFrontmostFloater");
		return;
	}

	std::string default_name = "floater_";
	default_name += frontmost->getTitle();
	default_name += ".xml";

	LLStringUtil::toLower(default_name);
	LLStringUtil::replaceChar(default_name, ' ', '_');
	LLStringUtil::replaceChar(default_name, '/', '_');
	LLStringUtil::replaceChar(default_name, ':', '_');
	LLStringUtil::replaceChar(default_name, '"', '_');

	// Open the file save dialog
	HBFileSelector::saveFile(HBFileSelector::FFSAVE_XUI, default_name,
							 save_to_xml_callback, frontmost);
}

bool buy_currency_enabled(void*)
{
	static LLCachedControl<bool> show_balance(gSavedSettings, "ShowBalance");
	return !show_balance;
}

bool buy_currency_checked(void*)
{
	static LLCachedControl<bool> checked(gSavedSettings, "ShowBuyCurrency");
	return checked && buy_currency_enabled(NULL);
}

bool script_anti_spam_enabled(void*)
{
	static LLCachedControl<bool> no_spam(gSavedSettings,
										 "ScriptDialogAntiSpam");
	return no_spam;
}

bool script_dialog_uniq_checked(void*)
{
	static LLCachedControl<bool> unique(gSavedSettings, "ScriptDialogUnique");
	return unique && script_anti_spam_enabled(NULL);
}

void init_debug_ui_menu(LLMenuGL* menu)
{
#if 0	// Neither of these works particularly well at the moment
	menu->append(new LLMenuItemCallGL("Reload UI XML", reload_ui, NULL, NULL));
	menu->append(new LLMenuItemCallGL("Reload settings/colors",
				 handle_reload_settings, NULL, NULL));
#endif

	menu->append(new LLMenuItemCallGL("Fit all open floaters in screen",
									  fit_all_open_floaters));
	menu->append(new LLMenuItemCheckGL("Show floater size while resizing",
									   menu_toggle_control, NULL,
									   menu_check_control,
									  (void*)"DebugShowResizing"));
	menu->appendSeparator();
	menu->append(new LLMenuItemCallGL("Decode all UI sounds",
									  decode_ui_sounds));
	menu->append(new LLMenuItemCallGL("Save decoded UI sounds",
									  copy_pre_decoded_ui_sounds));
	menu->append(new LLMenuItemCallGL("Clear saved UI sounds (after restart)",
									  clear_ui_sounds));
	menu->appendSeparator();
	menu->append(new LLMenuItemCheckGL("Search input field in status bar",
									   menu_toggle_control, NULL,
									   menu_check_control,
									  (void*)"ShowSearchBar"));
	menu->append(new LLMenuItemCheckGL("Money balance in status bar",
									   menu_toggle_control, NULL,
									   menu_check_control,
									  (void*)"ShowBalance"));
	menu->append(new LLMenuItemCheckGL("Buy currency button in status bar",
									   menu_toggle_control,
									   buy_currency_enabled,
									   buy_currency_checked,
									  (void*)"ShowBuyCurrency"));
	menu->appendSeparator();
	menu->append(new LLMenuItemCheckGL("Zoom dependent resize handles",
									   menu_toggle_control, NULL,
									   menu_check_control,
									   (void*)"ZoomDependentResizeHandles"));
	menu->appendSeparator();
	menu->append(new LLMenuItemCheckGL("Script dialogs anti-spam",
									   menu_toggle_control, NULL,
									   menu_check_control,
									   (void*)"ScriptDialogAntiSpam"));
	menu->append(new LLMenuItemCheckGL("Only one script dialog per object",
									   menu_toggle_control,
									   script_anti_spam_enabled,
									   script_dialog_uniq_checked,
									   (void*)"ScriptDialogUnique"));
	menu->appendSeparator();
	menu->append(new LLMenuItemCheckGL("Lua side-bar on left",
									   menu_toggle_control, NULL,
									   menu_check_control,
									   (void*)"LuaSideBarOnLeft"));	
	menu->appendSeparator();
	menu->append(new LLMenuItemCheckGL("Hide TP-related floaters on TP success",
				 menu_toggle_control, NULL, menu_check_control,
				 (void*)"HideFloatersOnTPSuccess"));

	menu->appendSeparator();
	menu->append(new LLMenuItemCheckGL("Persistent file selector paths",
									   menu_toggle_control, NULL,
									   menu_check_control,
									   (void*)"SaveFileSelectorPaths"));
	menu->appendSeparator();

	LLMenuGL* sub = new LLMenuGL("Debug");
	sub->append(new LLMenuItemCheckGL("Selection manager", menu_toggle_control,
									  NULL, menu_check_control,
									  (void*)"DebugSelectMgr"));
	sub->append(new LLMenuItemToggleGL("Clicks", &gDebugClicks));
	sub->append(new LLMenuItemToggleGL("Views", &LLView::sDebugRects));
	sub->append(new LLMenuItemToggleGL("Mouse events",
									   &LLView::sDebugMouseHandling));
	sub->append(new LLMenuItemToggleGL("Keys", &LLView::sDebugKeys));
	sub->append(new LLMenuItemToggleGL("WindowProc", &gDebugWindowProc));
	sub->createJumpKeys();
	menu->appendMenu(sub);

	sub = new LLMenuGL("XUI");
	sub->append(new LLMenuItemCallGL("Font test...",
									 handle_font_test_floater));
	sub->append(new LLMenuItemCallGL("Skin preview...",
									 handle_skin_preview_floater));
	sub->append(new LLMenuItemCallGL("Export menus to XML...",
									 handle_export_menus_to_xml,
									 enable_picker_actions, NULL));
	sub->append(new LLMenuItemCallGL("Edit UI...", LLFloaterEditUI::show));
	sub->append(new LLMenuItemCallGL("Load floater/panel from XML...",
									 handle_load_from_xml,
									 enable_picker_actions, NULL));
	sub->append(new LLMenuItemCallGL("Save frontmost floater to XML...",
									 handle_save_to_xml, enable_picker_actions,
									 NULL));
	sub->append(new LLMenuItemCheckGL("Show XUI names", toggle_show_xui_names,
									  NULL, check_show_xui_names, NULL));
	sub->createJumpKeys();
	menu->appendMenu(sub);

	menu->createJumpKeys();
}

bool deferred_rendering_enabled(void*)
{
	return LLPipeline::sRenderDeferred;
}

bool can_toggle_deferred(void*)
{
	return !gUsePBRShaders;
}

bool deferred_check_control(void*)
{
	return LLPipeline::sRenderDeferred;
}

bool wireframe_enabled(void*)
{
	return !gRLenabled || !gRLInterface.mContainsDetach;
}

bool wireframe_check(void*)
{
	return gUseWireframe;
}

void handle_toggle_wireframe(void*)
{
//MK
	if (!gUseWireframe && gRLenabled &&
		(gRLInterface.mHasLockedHuds || gRLInterface.mVisionRestricted))
	{
		// Do not toggle on !
		return;
	}
//mk
	gUseWireframe = !gUseWireframe;
	LLPipeline::refreshCachedSettings();
	gPipeline.resetVertexBuffers();
	// Rebuild objects to make sure all will properly show up... HB
	handle_objects_visibility(NULL);
}

void reset_vertex_buffers(void*)
{
	gPipeline.clearRebuildGroups();
	gPipeline.resetVertexBuffers();
}

void force_restart_gl(void*)
{
	if (gViewerWindowp)
	{
		gViewerWindowp->restartDisplay();
	}
}

bool force_restart_enabled(void*)
{
	static LLCachedControl<bool> allow(gSavedSettings,
									   "AllowGLRestartInCoreProfile");
	return !LLRender::sGLCoreProfile || allow;
}

void clear_derendered(void*)
{
	LLViewerObjectList::sBlackListedObjects.clear();
	// Update the derendered status in the radar.
	HBFloaterRadar::setRenderStatusDirty();
}

void boost_texture_fetches_now(void*)
{
	LLViewerTexture::resetLowMemCondition();
	LLViewerTextureList::sLastTeleportTime = gFrameTimeSeconds;
	LLSD args;
	args["DURATION"] =
		llformat("%d", gSavedSettings.getU32("TextureFetchBoostTimeAfterTP"));
	gNotifications.add("TextureFetchesBoosted", args);
}

static bool got_proper_rights(LLSelectNode* nodep)
{
	if (gAgent.isGodlikeWithoutAdminMenuFakery())
	{
		return true;
	}
	if (nodep->mPermissions->getOwner() != gAgentID)
	{
		return false;
	}
	if (gIsInSecondLife)
	{
		return nodep->mPermissions->getCreator() == gAgentID;
	}
	U32 perm_owner = nodep->mPermissions->getMaskOwner();
	return (perm_owner & PERM_ITEM_UNRESTRICTED) == PERM_ITEM_UNRESTRICTED;
}

static void emit_chat_text(const std::string& msg)
{
	LLChat chat(msg);
	LLFloaterChat::addChat(chat);
}

void handle_selected_texture_info(void*)
{
	for (LLObjectSelection::valid_iterator
			iter = gSelectMgr.getSelection()->valid_begin(),
			end = gSelectMgr.getSelection()->valid_end();
		 iter != end; ++iter)
	{
		LLSelectNode* nodep = *iter;

		bool can_see_id = got_proper_rights(nodep);

		std::string msg = "Texture info for primitive \"" + nodep->mName;
		msg += "\" (UUID: " + nodep->getObject()->getID().asString() + "):";
		emit_chat_text(msg);

		U8 te_count = nodep->getObject()->getNumTEs();
		// map from texture ID to list of faces using it
		typedef std::map< LLUUID, std::vector<U8> > map_t;
		map_t faces_per_texture;
		for (U8 i = 0; i < te_count; ++i)
		{
			if (!nodep->isTESelected(i)) continue;

			LLViewerTexture* texp = nodep->getObject()->getTEImage(i);
			LLUUID image_id = texp->getID();
			faces_per_texture[image_id].push_back(i);
		}

		// Per-texture, dump which faces are using it.
		std::string image_id_string;
		for (map_t::iterator it = faces_per_texture.begin(),
							 end = faces_per_texture.end(); it != end; ++it)
		{
			LLUUID image_id = it->first;
			U8 te = it->second[0];
			LLViewerTexture* texp = nodep->getObject()->getTEImage(te);
			S32 height = texp->getHeight();
			S32 width = texp->getWidth();
			S32 components = texp->getComponents();
			if (can_see_id)
			{
				image_id_string = image_id.asString();
			}
			else
			{
				image_id_string = "texture";
			}
			msg = llformat("%s %dx%d %s on face ", image_id_string.c_str(),
						   width, height,
						   (components == 4 ? "(alpha)" : "(opaque)"));
			for (U8 i = 0, count = it->second.size(); i < count; ++i)
			{
				msg += llformat("%d ", (S32)(it->second[i]));
			}
			emit_chat_text(msg);
		}
	}
}

void handle_selected_material_info(void*)
{
	// Map from material ID to list of faces using it
	typedef fast_hmap<LLMaterialID, std::vector<U8> > te_map_t;
	typedef fast_hmap<LLMaterialID, LLMaterial*> mat_map_t;
	mat_map_t materials;
	std::string msg;
	for (LLObjectSelection::valid_iterator
			iter = gSelectMgr.getSelection()->valid_begin(),
			end = gSelectMgr.getSelection()->valid_end();
		 iter != end; ++iter)
	{
		LLSelectNode* nodep = *iter;
		if (!nodep) continue;	// Paranoia

		LLViewerObject* object = nodep->getObject();
		if (!object) continue;	// Paranoia

		bool can_see_id = got_proper_rights(nodep);

		U8 te_count = object->getNumTEs();
		te_map_t faces_per_material;
		for (U8 i = 0; i < te_count; ++i)
		{
			if (!nodep->isTESelected(i)) continue;

			LLTextureEntry* tep = object->getTE(i);
			if (!tep) continue;
			
			const LLMaterialID& mat_id = tep->getMaterialID();
			if (mat_id.isNull()) continue;	// No material

			faces_per_material[mat_id].push_back(i);
			if (can_see_id)
			{
				materials.emplace(mat_id, tep->getMaterialParams().get());
			}
		}

		if (faces_per_material.empty())
		{
			msg = "No material on primitive: " + nodep->mName;
			emit_chat_text(msg);
			continue;
		}

		// Per-material, dump which faces are using it.
		msg = "Material info for primitive: " + nodep->mName;
		emit_chat_text(msg);

		for (te_map_t::iterator it = faces_per_material.begin(),
								end = faces_per_material.end();
			 it != end; ++it)
		{
			const LLMaterialID& mat_id = it->first;
			// Note: the material Id does not give any useful information to
			// find out what is the actual composition of the material, so we
			// can give it up without any copyright issue. HB
			msg = mat_id.asString() + " on face ";
			for (U8 i = 0; i < it->second.size(); ++i)
			{
				msg += llformat("%d ", (S32)(it->second[i]));
			}
			emit_chat_text(msg);
		}
	}

	if (materials.empty())
	{
		return;
	}
	emit_chat_text("List of legacy materials:");
	for (mat_map_t::iterator it = materials.begin(), end = materials.end();
		 it != end; ++it)
	{
		LLMaterial* matp = it->second;
		const LLUUID& norm_id = matp->getNormalID();
		const LLUUID& spec_id = matp->getSpecularID();
		msg = "Material " + it->first.asString() + " got ";
		msg += norm_id.notNull() ? norm_id.asString() + " as" : "no";
		msg += " normal map and ";
		msg += spec_id.notNull() ? spec_id.asString() + " as" : "no";
		msg += " specular map.";
		emit_chat_text(msg);
	}
	emit_chat_text("End of legacy materials list.");
}

void handle_selected_pbr_info(void*)
{
	// Map from material ID to list of faces using it
	typedef fast_hmap<LLUUID, std::vector<U8> > te_map_t;
	typedef fast_hmap<LLUUID, LLGLTFMaterial*> mat_map_t;
	mat_map_t materials;
	std::string msg;
	for (LLObjectSelection::valid_iterator
			iter = gSelectMgr.getSelection()->valid_begin(),
			end = gSelectMgr.getSelection()->valid_end();
		 iter != end; ++iter)
	{
		LLSelectNode* nodep = *iter;
		if (!nodep) continue;	// Paranoia

		LLViewerObject* object = nodep->getObject();
		if (!object) continue;	// Paranoia

		bool can_see_id = got_proper_rights(nodep);

		U8 te_count = object->getNumTEs();
		te_map_t faces_per_material;
		for (U8 i = 0; i < te_count; ++i)
		{
			if (!nodep->isTESelected(i)) continue;

			LLTextureEntry* tep = object->getTE(i);
			if (!tep) continue;

			LLGLTFMaterial* matp = tep->getGLTFMaterial();
			if (!matp) continue;	// No material

			const LLUUID& mat_id = matp->getHash();
			faces_per_material[mat_id].push_back(i);
			if (can_see_id)
			{
				materials.emplace(mat_id, matp);
			}
		}

		if (faces_per_material.empty())
		{
			msg = "No PBR material on primitive: " + nodep->mName;
			emit_chat_text(msg);
			continue;
		}

		// Per-material, dump which faces are using it.
		msg = "GLTF material info for primitive: " + nodep->mName;
		emit_chat_text(msg);

		for (te_map_t::iterator it = faces_per_material.begin(),
								end = faces_per_material.end();
			 it != end; ++it)
		{
			const LLUUID& mat_id = it->first;
			// Note: the hash does not give any useful information to find out
			// what is the actual composition of the GLTF material, so we can
			// give it up without any copyright issue. HB
			msg = mat_id.asString() + " on face ";
			for (U8 i = 0; i < it->second.size(); ++i)
			{
				msg += llformat("%d ", (S32)(it->second[i]));
			}
			emit_chat_text(msg);
		}
	}

	if (materials.empty())
	{
		return;
	}
	emit_chat_text("List of PBR materials:");
	for (mat_map_t::iterator it = materials.begin(), end = materials.end();
		 it != end; ++it)
	{
		LLGLTFMaterial* matp = it->second;
		const LLGLTFMaterial::uuid_array_t& textures = matp->mTextureId;
		const LLUUID& basecol = textures[BASECOLIDX];
		const LLUUID& normal = textures[NORMALIDX];
		const LLUUID& mrough = textures[MROUGHIDX];
		const LLUUID& emissive = textures[EMISSIVEIDX];
		msg = "Material " + it->first.asString() + " got ";
		msg += basecol.notNull() ? basecol.asString() + " as" : "no";
		msg += " base color map, ";
		msg += normal.notNull() ? normal.asString() + " as" : "no";
		msg += " normal map, ";
		msg += mrough.notNull() ? mrough.asString() + " as" : "no";
		msg += " metallic/roughness map and ";
		msg += emissive.notNull() ? emissive.asString() + " as" : "no";
		msg += " emissive map.";
		emit_chat_text(msg);
	}
	emit_chat_text("End of PBR materials list.");
}

void reload_selected_texture(void*)
{
	std::set<LLUUID> reloaded;
	LLViewerTexture* default_texp =
		(LLViewerTexture*)LLViewerFetchedTexture::sDefaultImagep;
	LLViewerFetchedTexture* texp;
	for (LLObjectSelection::valid_iterator iter =
			gSelectMgr.getSelection()->valid_begin();
		 iter != gSelectMgr.getSelection()->valid_end(); ++iter)
	{
		LLSelectNode* nodep = *iter;
		LLViewerObject* objectp = nodep->getObject();
		if (!objectp) continue;

		// Allow to reload linden trees' texture. HB
		LLVOTree* treevobjp = dynamic_cast<LLVOTree*>(objectp);
		if (treevobjp)
		{
			texp = treevobjp->getTreeTexture();
			if (texp)
			{
				const LLUUID& texid = texp->getID();
				if (!reloaded.count(texid))
				{
					// Force a reload of the raw image
					texp->forceRefetch();
					reloaded.emplace(texid);		// Mark as reloaded
				}
			}
			continue;	// Nothing else needed for Linden trees.
		}

		// Does this object have a sculpt map texture ?
		const LLSculptParams* sculptp = objectp->getSculptParams();
		if (sculptp &&
			(sculptp->getSculptType() &
			 LL_SCULPT_TYPE_MASK) != LL_SCULPT_TYPE_MESH)
		{
			const LLUUID& texid = sculptp->getSculptTexture();
			if (texid.notNull())
			{
				if (!reloaded.count(texid))
				{
					texp =
						LLViewerTextureManager::getFetchedTexture(texid,
																  FTT_DEFAULT,
																  true,
																  LLGLTexture::BOOST_NONE,
																  LLViewerTexture::LOD_TEXTURE);
					if (texp)
					{
						S32 count = texp->getNumVolumes(LLRender::SCULPT_TEX);
						const LLViewerTexture::ll_volume_list_t* volumesp =
							texp->getVolumeList(LLRender::SCULPT_TEX);

						// Force a reload of the raw image
						texp->forceRefetch();

						for (S32 i = 0; i < count; ++i)
						{
							LLVOVolume* volp = (*volumesp)[i];
							if (volp)
							{
								volp->notifyMeshLoaded();
							}
						}
					}
					reloaded.emplace(texid);		// Mark as reloaded
				}
				// Force an object geometry rebuild
				objectp->markForUpdate();
			}
		}

		if (gUsePBRShaders)
		{
			// Re-apply object cache overrides if any.
			LLViewerRegion* regionp = objectp->getRegion();
			if (regionp)
			{
				regionp->loadCacheMiscExtras(objectp);
				objectp->markForUpdate(false);
			}
		}

		// Now deal with the other textures, per face.
		for (U8 i = 0, count = objectp->getNumTEs(); i < count; ++i)
		{
			if (!nodep->isTESelected(i)) continue;

			LLViewerTexture* imgp = objectp->getTEImage(i);
			if (imgp)
			{
				const LLUUID& texid = imgp->getID();
				if (texid.notNull())
				{
					// To flag as texture changed:
					objectp->setTETexture(i, IMG_DEFAULT);
					if (!reloaded.count(texid))
					{
						LLViewerFetchedTexture* tex =
							LLViewerTextureManager::staticCast(imgp);
						if (tex)
						{
							// Force a reload of the raw image
							tex->forceRefetch();
						}
						reloaded.emplace(texid);	// Mark as reloaded
					}
					// Will rebind the texture in GL:
					objectp->setTETexture(i, texid);
				}
			}
			imgp = objectp->getTENormalMap(i);
			if (imgp && imgp != default_texp)
			{
				const LLUUID& texid = imgp->getID();
				if (!reloaded.count(texid))
				{
					texp =
						LLViewerTextureManager::staticCast(imgp);
					if (texp)
					{
						// Force a reload of the raw image
						texp->forceRefetch();
					}
					reloaded.emplace(texid);		// Mark as reloaded
				}
			}
			imgp = objectp->getTESpecularMap(i);
			if (imgp && imgp != default_texp)
			{
				const LLUUID& texid = imgp->getID();
				if (!reloaded.count(texid))
				{
					texp =
						LLViewerTextureManager::staticCast(imgp);
					if (texp)
					{
						// Force a reload of the raw image
						texp->forceRefetch();
					}
					reloaded.emplace(texid);		// Mark as reloaded
				}
			}

			if (!gUsePBRShaders) continue;	// Skip any GLTF material textures

			// We also need to reload any GLTF render material textures. HB
			LLTextureEntry* tep = objectp->getTE(i);
			if (!tep) continue;	// Paranoia

			LLFetchedGLTFMaterial* gltfp =
				(LLFetchedGLTFMaterial*)tep->getGLTFRenderMaterial();
			if (!gltfp) continue;	// No GLTF material on this face.

			texp = gltfp->mBaseColorTexture;
			if (texp)
			{
				const LLUUID& texid = texp->getID();
				if (!reloaded.count(texid))
				{
					// Force a reload of the raw image
					texp->forceRefetch();
					reloaded.emplace(texid);		// Mark as reloaded
				}
			}
			texp = gltfp->mNormalTexture;
			if (texp)
			{
				const LLUUID& texid = texp->getID();
				if (!reloaded.count(texid))
				{
					// Force a reload of the raw image
					texp->forceRefetch();
					reloaded.emplace(texid);		// Mark as reloaded
				}
			}
			texp = gltfp->mMetallicRoughnessTexture;
			if (texp)
			{
				const LLUUID& texid = texp->getID();
				if (!reloaded.count(texid))
				{
					// Force a reload of the raw image
					texp->forceRefetch();
					reloaded.emplace(texid);		// Mark as reloaded
				}
			}
			texp = gltfp->mEmissiveTexture;
			if (texp)
			{
				const LLUUID& texid = texp->getID();
				if (!reloaded.count(texid))
				{
					// Force a reload of the raw image
					texp->forceRefetch();
					reloaded.emplace(texid);		// Mark as reloaded
				}
			}
		}
	}
}

void handle_dump_image_list(void*)
{
	gTextureList.dump();
}

void meshopt_toggle(void* userdata)
{
	U32 method = (U32)(intptr_t)userdata;
	gSavedSettings.setU32("RenderMeshVertexCacheOptimize", method);
}

bool meshopt_check_control(void* userdata)
{
	static LLCachedControl<U32> method(gSavedSettings,
									   "RenderMeshVertexCacheOptimize");
	U32 check = (U32)(intptr_t)userdata;
	return check == method;
}

void menu_toggle_attached_lights(void* user_data)
{
	menu_toggle_control(user_data);
	LLPipeline::sRenderAttachedLights =
		gSavedSettings.getBool("RenderAttachedLights");
}

void menu_toggle_attached_particles(void* user_data)
{
	menu_toggle_control(user_data);
	LLPipeline::sRenderAttachedParticles =
		gSavedSettings.getBool("RenderAttachedParticles");
}

void frame_render_profile(void*)
{
	gShaderProfileFrame = true;
}

void shadows_toggle(void* userdata)
{
	gSavedSettings.setU32("RenderShadowDetail", (U32)(intptr_t)userdata);
}

bool shadows_check_control(void* userdata)
{
	static LLCachedControl<U32> shadows(gSavedSettings, "RenderShadowDetail");
	return (U32)shadows == (U32)(intptr_t)userdata;
}

void ssao_toggle(void* userdata)
{
	gSavedSettings.setU32("RenderDeferredSSAO", (U32)(intptr_t)userdata);
}

bool ssao_check_control(void* userdata)
{
	static LLCachedControl<U32> ssao(gSavedSettings, "RenderDeferredSSAO");
	return (U32)ssao == (U32)(intptr_t)userdata;
}

bool invisprim_enabled(void*)
{
	return LLPipeline::sRenderDeferred && !gUsePBRShaders;
}

bool invisprim_check_control(void*)
{
	static LLCachedControl<bool> invisiprims(gSavedSettings,
											 "RenderDeferredInvisible");
	return !gUsePBRShaders && (invisiprims || !LLPipeline::sRenderDeferred);
}

void handle_objects_visibility(void*)
{
	llinfos << "Refreshing objects visibility" << llendl;

	for (S32 i = 0, count = gObjectList.getNumObjects(); i < count; ++i)
	{
		LLViewerObject* objectp = gObjectList.getObject(i);
		if (objectp && !objectp->isDead())
		{
#if 0		// Also resets rotation of target-omega objects
			objectp->setSelected(false);
#else
			objectp->markForUpdate(true);
#endif
		}
	}
}

bool debuggl_checked(void*)
{
	return gDebugGL;
}

void handle_debug_gl(void*)
{
	gDebugGL = !gDebugGL;
	clear_glerror();
	llinfos << "GL debugging turned " << (gDebugGL ? "on." : "off.") << llendl;
}

void schedule_objects_visibility_refresh(U32 type)
{
	static LLCachedControl<U32> delay(gSavedSettings,
									  "ObjectsVisibilityAutoRefreshDelay");
	static LLCachedControl<U32> refresh_mask(gSavedSettings,
										  	 "ObjectsVisibilityAutoRefreshMask");
	// Skip if purposely disabled, or when not yet rendering the world.
	if (!delay || !(type & U32(refresh_mask)) || !LLStartUp::isLoggedIn())
	{
		return;
	}
	doAfterInterval(boost::bind(handle_objects_visibility, nullptr),
					// Clamp to a reasonnable delay...
					llmin((F32)delay, 10.f));
}

bool vb_cache_check_control(void*)
{
	static LLCachedControl<bool> vbcache(gSavedSettings, "RenderGLUseVBCache");
	return gUsePBRShaders || vbcache;
}

void init_debug_rendering_menu(LLMenuGL* menu)
{
	///////////////////////////
	// Debug menu for types/pools

	LLMenuGL* sub = new LLMenuGL("Types");

	sub->append(new LLMenuItemCheckGL("Simple",
									  &LLPipeline::toggleRenderTypeControl, NULL,
									  &LLPipeline::hasRenderTypeControl,
									  (void*)LLPipeline::RENDER_TYPE_SIMPLE,
									  '1', MASK_CONTROL|MASK_ALT|MASK_SHIFT));
	sub->append(new LLMenuItemCheckGL("Alpha",
									  &LLPipeline::toggleRenderTypeControl, NULL,
									  &LLPipeline::hasRenderTypeControl,
									  (void*)LLPipeline::RENDER_TYPE_ALPHA,
									  '2', MASK_CONTROL|MASK_ALT|MASK_SHIFT));
	sub->append(new LLMenuItemCheckGL("Tree",
									  &LLPipeline::toggleRenderTypeControl, NULL,
									  &LLPipeline::hasRenderTypeControl,
									  (void*)LLPipeline::RENDER_TYPE_TREE,
									  '3', MASK_CONTROL|MASK_ALT|MASK_SHIFT));
	sub->append(new LLMenuItemCheckGL("Avatar",
									  &LLPipeline::toggleRenderTypeControl, NULL,
									  &LLPipeline::hasRenderTypeControl,
									  (void*)LLPipeline::RENDER_TYPE_AVATAR,
									  '4', MASK_CONTROL|MASK_ALT|MASK_SHIFT));
	sub->append(new LLMenuItemCheckGL("Animesh",
									   &LLPipeline::toggleRenderTypeControl, NULL,
									   &LLPipeline::hasRenderTypeControl,
									   (void*)LLPipeline::RENDER_TYPE_PUPPET,
									   '+', MASK_CONTROL|MASK_ALT|MASK_SHIFT));
	sub->append(new LLMenuItemCheckGL("SurfacePatch",
									  &LLPipeline::toggleRenderTypeControl, NULL,
									  &LLPipeline::hasRenderTypeControl,
									  (void*)LLPipeline::RENDER_TYPE_TERRAIN,
									  '5', MASK_CONTROL|MASK_ALT|MASK_SHIFT));
	sub->append(new LLMenuItemCheckGL("Sky",
									  &LLPipeline::toggleRenderTypeControl, NULL,
									  &LLPipeline::hasRenderTypeControl,
									  (void*)LLPipeline::RENDER_TYPE_SKY,
									  '6', MASK_CONTROL|MASK_ALT|MASK_SHIFT));
	sub->append(new LLMenuItemCheckGL("Water",
									  &LLPipeline::toggleRenderTypeControl, NULL,
									  &LLPipeline::hasRenderTypeControl,
									  (void*)LLPipeline::RENDER_TYPE_WATER,
									  '7', MASK_CONTROL|MASK_ALT|MASK_SHIFT));
	sub->append(new LLMenuItemCheckGL("Volume",
									  &LLPipeline::toggleRenderTypeControl, NULL,
									  &LLPipeline::hasRenderTypeControl,
									  (void*)LLPipeline::RENDER_TYPE_VOLUME,
									  '9', MASK_CONTROL|MASK_ALT|MASK_SHIFT));
	sub->append(new LLMenuItemCheckGL("Grass",
									  &LLPipeline::toggleRenderTypeControl, NULL,
									  &LLPipeline::hasRenderTypeControl,
									  (void*)LLPipeline::RENDER_TYPE_GRASS,
									  '0', MASK_CONTROL|MASK_ALT|MASK_SHIFT));
	sub->append(new LLMenuItemCheckGL("Clouds",
									  &LLPipeline::toggleRenderTypeControl, NULL,
									  &LLPipeline::hasRenderTypeControl,
									  (void*)LLPipeline::RENDER_TYPE_CLOUDS,
									  '-', MASK_CONTROL|MASK_ALT|MASK_SHIFT));
	sub->append(new LLMenuItemCheckGL("Particles",
									  &LLPipeline::toggleRenderTypeControl, NULL,
									  &LLPipeline::hasRenderTypeControl,
									  (void*)LLPipeline::RENDER_TYPE_PARTICLES,
									  '*', MASK_CONTROL|MASK_ALT|MASK_SHIFT));
	sub->append(new LLMenuItemCheckGL("Bump",
									  &LLPipeline::toggleRenderTypeControl, NULL,
									  &LLPipeline::hasRenderTypeControl,
									  (void*)LLPipeline::RENDER_TYPE_BUMP,
									  '/', MASK_CONTROL|MASK_ALT|MASK_SHIFT));
	sub->append(new LLMenuItemCheckGL("PBR materials",
									  &LLPipeline::toggleRenderTypeControl, NULL,
									  &LLPipeline::hasRenderTypeControl,
									  (void*)LLPipeline::RENDER_TYPE_MAT_PBR));
	sub->createJumpKeys();
	menu->appendMenu(sub);

	sub = new LLMenuGL("Features");
	sub->append(new LLMenuItemCheckGL("UI",
									  LLPipeline::toggleRenderDebugFeature, NULL,
									  LLPipeline::toggleRenderDebugFeatureControl,
									  (void*)LLPipeline::RENDER_DEBUG_FEATURE_UI,
									  KEY_F1, MASK_SHIFT|MASK_CONTROL));
	sub->append(new LLMenuItemCheckGL("Selected",
									  LLPipeline::toggleRenderDebugFeature, NULL,
									  LLPipeline::toggleRenderDebugFeatureControl,
									  (void*)LLPipeline::RENDER_DEBUG_FEATURE_SELECTED,
									  KEY_F2, MASK_SHIFT|MASK_CONTROL));
	sub->append(new LLMenuItemCheckGL("Dynamic textures",
									  LLPipeline::toggleRenderDebugFeature, NULL,
									  LLPipeline::toggleRenderDebugFeatureControl,
									  (void*)LLPipeline::RENDER_DEBUG_FEATURE_DYNAMIC_TEXTURES,
									  KEY_F3, MASK_SHIFT|MASK_CONTROL));
	sub->append(new LLMenuItemCheckGL("Water fog",
									  LLPipeline::toggleRenderDebugFeature, NULL,
									  LLPipeline::toggleRenderDebugFeatureControl,
									  (void*)LLPipeline::RENDER_DEBUG_FEATURE_FOG,
									  KEY_F4, MASK_SHIFT|MASK_CONTROL));
	sub->append(new LLMenuItemCheckGL("Flexible objects",
									  LLPipeline::toggleRenderDebugFeature, NULL,
									  LLPipeline::toggleRenderDebugFeatureControl,
									  (void*)LLPipeline::RENDER_DEBUG_FEATURE_FLEXIBLE,
									  KEY_F5, MASK_SHIFT|MASK_CONTROL));
	sub->createJumpKeys();
	menu->appendMenu(sub);

	/////////////////////////////
	// Debug menu for info displays

	sub = new LLMenuGL("Info displays");

	sub->append(new LLMenuItemCheckGL("World axes", menu_toggle_control, NULL,
									  menu_check_control, (void*)"ShowAxes"));
	sub->append(new LLMenuItemCheckGL("Hit boxes", menu_toggle_control, NULL,
									  menu_check_control,
									  (void*)"RenderDebugHitBox"));
	sub->append(new LLMenuItemCheckGL("Bounding boxes",
									  LLPipeline::toggleRenderDebug, NULL,
									  LLPipeline::toggleRenderDebugControl,
									  (void*)LLPipeline::RENDER_DEBUG_BBOXES));
	sub->append(new LLMenuItemCheckGL("Normals",
									  LLPipeline::toggleRenderDebug, NULL,
									  LLPipeline::toggleRenderDebugControl,
									  (void*)LLPipeline::RENDER_DEBUG_NORMALS));
	sub->append(new LLMenuItemCheckGL("Points",
									  LLPipeline::toggleRenderDebug, NULL,
									  LLPipeline::toggleRenderDebugControl,
									  (void*)LLPipeline::RENDER_DEBUG_POINTS));
	sub->append(new LLMenuItemCheckGL("Octree",
									  LLPipeline::toggleRenderDebug, NULL,
									  LLPipeline::toggleRenderDebugControl,
									  (void*)LLPipeline::RENDER_DEBUG_OCTREE));
	sub->append(new LLMenuItemCheckGL("Shadow frusta",
									  LLPipeline::toggleRenderDebug, NULL,
									  LLPipeline::toggleRenderDebugControl,
									  (void*)LLPipeline::RENDER_DEBUG_SHADOW_FRUSTA));
	sub->append(new LLMenuItemCheckGL("Reflection probes",
									  LLPipeline::toggleRenderDebug, NULL,
									  LLPipeline::toggleRenderDebugControl,
									  (void*)LLPipeline::RENDER_DEBUG_REFLECTION_PROBES));
	sub->append(new LLMenuItemCheckGL("Physics shapes",
									  LLPipeline::toggleRenderDebug, NULL,
									  LLPipeline::toggleRenderDebugControl,
									  (void*)LLPipeline::RENDER_DEBUG_PHYSICS_SHAPES));
	sub->append(new LLMenuItemCheckGL("Occlusion",
									  LLPipeline::toggleRenderDebug, NULL,
									  LLPipeline::toggleRenderDebugControl,
									  (void*)LLPipeline::RENDER_DEBUG_OCCLUSION));
	sub->append(new LLMenuItemCheckGL("Render batches",
									  LLPipeline::toggleRenderDebug, NULL,
									  LLPipeline::toggleRenderDebugControl,
									  (void*)LLPipeline::RENDER_DEBUG_BATCH_SIZE));
	sub->append(new LLMenuItemCheckGL("Update type",
									  LLPipeline::toggleRenderDebug, NULL,
									  LLPipeline::toggleRenderDebugControl,
									  (void*)LLPipeline::RENDER_DEBUG_UPDATE_TYPE));
	sub->append(new LLMenuItemCheckGL("Animated textures",
									  LLPipeline::toggleRenderDebug, NULL,
									  LLPipeline::toggleRenderDebugControl,
									  (void*)LLPipeline::RENDER_DEBUG_TEXTURE_ANIM));
	sub->append(new LLMenuItemCheckGL("Texture priority",
									  LLPipeline::toggleRenderDebug, NULL,
									  LLPipeline::toggleRenderDebugControl,
									  (void*)LLPipeline::RENDER_DEBUG_TEXTURE_PRIORITY));
	sub->append(new LLMenuItemCheckGL("Avatar complexity/visibility rank",
									  LLPipeline::toggleRenderDebug, NULL,
									  LLPipeline::toggleRenderDebugControl,
									  (void*)LLPipeline::RENDER_DEBUG_AVATAR_DRAW_INFO,
									  'C', MASK_CONTROL|MASK_ALT));
	sub->append(new LLMenuItemCheckGL("Attachments memory/area",
									  LLPipeline::toggleRenderDebug, NULL,
									  LLPipeline::toggleRenderDebugControl,
									  (void*)LLPipeline::RENDER_DEBUG_ATTACHMENT_INFO,
									  'A', MASK_CONTROL|MASK_ALT));
	sub->append(new LLMenuItemCheckGL("Texture area (sqrt(A))",
									  LLPipeline::toggleRenderDebug, NULL,
									  LLPipeline::toggleRenderDebugControl,
									  (void*)LLPipeline::RENDER_DEBUG_TEXTURE_AREA));
	sub->append(new LLMenuItemCheckGL("Texture size",
									  LLPipeline::toggleRenderDebug, NULL,
									  LLPipeline::toggleRenderDebugControl,
									  (void*)LLPipeline::RENDER_DEBUG_TEXTURE_SIZE));
	sub->append(new LLMenuItemCheckGL("Face area (sqrt(A))",
									  LLPipeline::toggleRenderDebug, NULL,
									  LLPipeline::toggleRenderDebugControl,
									  (void*)LLPipeline::RENDER_DEBUG_FACE_AREA));
	sub->append(new LLMenuItemCheckGL("LOD info",
									  LLPipeline::toggleRenderDebug, NULL,
									  LLPipeline::toggleRenderDebugControl,
									  (void*)LLPipeline::RENDER_DEBUG_LOD_INFO));
	sub->append(new LLMenuItemCheckGL("Lights",
									  LLPipeline::toggleRenderDebug, NULL,
									  LLPipeline::toggleRenderDebugControl,
									  (void*)LLPipeline::RENDER_DEBUG_LIGHTS));
	sub->append(new LLMenuItemCheckGL("Particles",
									  LLPipeline::toggleRenderDebug, NULL,
									  LLPipeline::toggleRenderDebugControl,
									  (void*)LLPipeline::RENDER_DEBUG_PARTICLES));
	sub->append(new LLMenuItemCheckGL("Composition",
									  LLPipeline::toggleRenderDebug, NULL,
									  LLPipeline::toggleRenderDebugControl,
									  (void*)LLPipeline::RENDER_DEBUG_COMPOSITION));
	sub->append(new LLMenuItemCheckGL("Raycasting",
									  LLPipeline::toggleRenderDebug, NULL,
									  LLPipeline::toggleRenderDebugControl,
									  (void*)LLPipeline::RENDER_DEBUG_RAYCAST));
	sub->append(new LLMenuItemCheckGL("Sculpt",
									  LLPipeline::toggleRenderDebug, NULL,
									  LLPipeline::toggleRenderDebugControl,
									  (void*)LLPipeline::RENDER_DEBUG_SCULPTED));
	sub->append(new LLMenuItemCheckGL("Verify",
									  LLPipeline::toggleRenderDebug, NULL,
									  LLPipeline::toggleRenderDebugControl,
									  (void*)LLPipeline::RENDER_DEBUG_VERIFY));

	menu->appendMenu(sub);

	/////////////////////////////
	// Render tests sub-menu

	sub = new LLMenuGL("Render tests");

	sub->append(new LLMenuItemCheckGL("Camera offset", menu_toggle_control,
									  NULL, menu_check_control,
									  (void*)"CameraOffset"));

	sub->append(new LLMenuItemToggleGL("Frame test",
									   &LLPipeline::sRenderFrameTest));

	sub->append(new LLMenuItemCallGL("Frame profile", frame_render_profile));

	sub->append(new LLMenuItemCheckGL("Debug GL", handle_debug_gl, NULL,
									  debuggl_checked, NULL));

	sub->createJumpKeys();
	menu->appendMenu(sub);

	/////////////////////////////
	// Deferred rendering sub-menu

	sub = new LLMenuGL("Deferred rendering");

	sub->append(new LLMenuItemCheckGL("Deferred rendering",
									  menu_toggle_control, can_toggle_deferred,
									  deferred_check_control,
									  (void*)"RenderDeferred",
									  'D', MASK_CONTROL|MASK_ALT));

	sub->append(new LLMenuItemCheckGL("No shadow", shadows_toggle,
									  deferred_rendering_enabled,
									  shadows_check_control, NULL));

	sub->append(new LLMenuItemCheckGL("Sun and Moon shadows",
									  shadows_toggle,
									  deferred_rendering_enabled,
									  shadows_check_control, (void*)1));

	sub->append(new LLMenuItemCheckGL("All lights shadows", shadows_toggle,
									  deferred_rendering_enabled,
									  shadows_check_control, (void*)2));

	sub->append(new LLMenuItemCheckGL("Never use SSAO", ssao_toggle,
									  deferred_rendering_enabled,
									  ssao_check_control, NULL));

	sub->append(new LLMenuItemCheckGL("SSAO only with shadows", ssao_toggle,
									  deferred_rendering_enabled,
									  ssao_check_control, (void*)1));

	sub->append(new LLMenuItemCheckGL("Always use SSAO", ssao_toggle,
									  deferred_rendering_enabled,
									  ssao_check_control, (void*)2));

	sub->append(new LLMenuItemCheckGL("Render invisiprims",
									  menu_toggle_control,
									  invisprim_enabled,
									  invisprim_check_control,
									  (void*)"RenderDeferredInvisible"));

	sub->append(new LLMenuItemCheckGL("Depth of field", menu_toggle_control,
									  deferred_rendering_enabled,
									  menu_check_control,
									  (void*)"RenderDepthOfField"));

	sub->createJumpKeys();
	menu->appendMenu(sub);

	/////////////////////////////
	// Textures rendering sub-menu

	sub = new LLMenuGL("Textures");

	sub->append(new LLMenuItemToggleGL("Animate textures",
									   &LLVOVolume::sAnimateTextures));

	sub->append(new LLMenuItemToggleGL("Disable textures",
									   &LLViewerTexture::sDontLoadVolumeTextures));

	sub->append(new LLMenuItemCheckGL("Scale down fetched textures",
									  menu_toggle_control, NULL,
									  menu_check_control,
									  (void*)"TextureRescaleFetched"));
	sub->append(new LLMenuItemCheckGL("Load boosted textures at full res",
									  menu_toggle_control, NULL,
									  menu_check_control,
									 (void*)"FullResBoostedTextures"));
	sub->append(new LLMenuItemCheckGL("Boost fetches with speed",
									  menu_toggle_control, NULL,
									  menu_check_control,
									  (void*)"TextureFetchBoostWithSpeed",
									  'B', MASK_CONTROL|MASK_SHIFT));
	sub->append(new LLMenuItemCheckGL("Boost proportional to active fetches",
									  menu_toggle_control, NULL,
									  menu_check_control,
									  (void*)"TextureFetchBoostWithFetches"));
	sub->append(new LLMenuItemCallGL("Boost textures fetches now",
									 boost_texture_fetches_now, NULL, NULL,
									 'B', MASK_CONTROL));

	sub->createJumpKeys();
	menu->appendMenu(sub);

	/////////////////////////////
	//
	// Miscellaenous
	//
	menu->appendSeparator();
	menu->append(new LLMenuItemCheckGL("Animate trees", menu_toggle_control,
									   NULL, menu_check_control,
									   (void*)"RenderAnimateTrees"));

	menu->appendSeparator();
	menu->append(new LLMenuItemCheckGL("Hide selected objects",
									   menu_toggle_control, NULL,
									   menu_check_control,
									   (void*)"HideSelectedObjects"));
	menu->append(new LLMenuItemCallGL("Clear derendered objects list",
									  clear_derendered, NULL, NULL));
	menu->append(new LLMenuItemCallGL("Refresh visibility of objects",
									  handle_objects_visibility, NULL, NULL,
									  'R', MASK_SHIFT|MASK_ALT));
	menu->appendSeparator();
	menu->append(new LLMenuItemCheckGL("Tangent basis", menu_toggle_control,
									   NULL, menu_check_control,
									   (void*)"ShowTangentBasis"));
	menu->append(new LLMenuItemCallGL("Selected texture info",
									  handle_selected_texture_info, NULL, NULL,
									  'T', MASK_CONTROL|MASK_SHIFT|MASK_ALT));
	menu->append(new LLMenuItemCallGL("Selected legacy material info",
									  handle_selected_material_info, NULL,
									  NULL, 'A',
									  MASK_CONTROL|MASK_SHIFT|MASK_ALT));
	menu->append(new LLMenuItemCallGL("Selected GLTF material info",
									  handle_selected_pbr_info, NULL,
									  NULL, 'G',
									  MASK_CONTROL|MASK_SHIFT|MASK_ALT));
	menu->append(new LLMenuItemCallGL("Reload selected texture",
									  reload_selected_texture, NULL, NULL, 'U',
									  MASK_CONTROL|MASK_SHIFT));
#if 0
	menu->append(new LLMenuItemCallGL("Dump image list",
									  handle_dump_image_list, NULL, NULL, 'I',
									  MASK_CONTROL|MASK_SHIFT));
#endif

	menu->append(new LLMenuItemCallGL("Reset vertex buffers",
									  reset_vertex_buffers, NULL, NULL));

	menu->append(new LLMenuItemCheckGL("Cache vertex buffers",
									   menu_toggle_control,
									   can_toggle_deferred,
									   vb_cache_check_control,
									   (void*)"RenderGLUseVBCache"));

	menu->append(new LLMenuItemCheckGL("Optimize mesh vertex cache",
									   menu_toggle_control, NULL,
									   menu_check_control,
									   (void*)"RenderOptimizeMeshVertexCache"));

	// Note: disabled when core GL profile is enabled, because then terrain
	// fails to render properly after GL restart... HB
	menu->append(new LLMenuItemCallGL("Force-restart GL", force_restart_gl,
									  force_restart_enabled, NULL));

	menu->appendSeparator();

	menu->append(new LLMenuItemCheckGL("Wireframe", handle_toggle_wireframe,
									   wireframe_enabled, wireframe_check,
									   NULL, 'R', MASK_CONTROL|MASK_SHIFT));

	menu->append(new LLMenuItemCheckGL("Automatic alpha masks (non-deferred)",
									   menu_toggle_control, NULL,
									   menu_check_control,
									   (void*)"RenderAutoMaskAlphaNonDeferred"));

	menu->append(new LLMenuItemCheckGL("Automatic alpha masks (deferred)",
									   menu_toggle_control, NULL,
									   menu_check_control,
									   (void*)"RenderAutoMaskAlphaDeferred"));
	menu->append(new LLMenuItemCheckGL("Font glyphs batching",
									   menu_toggle_control, NULL,
									   menu_check_control,
									   (void*)"RenderBatchedGlyphs"));
	menu->append(new LLMenuItemCheckGL("Cheesy beacon", menu_toggle_control,
									   NULL, menu_check_control,
									   (void*)"CheesyBeacon"));

	menu->appendSeparator();

	menu->append(new LLMenuItemCheckGL("Attached lights",
									   menu_toggle_attached_lights, NULL,
									   menu_check_control,
									   (void*)"RenderAttachedLights"));
	menu->append(new LLMenuItemCheckGL("Attached particles",
									   menu_toggle_attached_particles, NULL,
									   menu_check_control,
									   (void*)"RenderAttachedParticles"));
	menu->createJumpKeys();
}

void handle_rebuild_avatar(void*)
{
	if (isAgentAvatarValid())
	{
		uuid_vec_t anim_ids;
		for (LLVOAvatar::anim_it_t
				it = gAgentAvatarp->mPlayingAnimations.begin(),
				end = gAgentAvatarp->mPlayingAnimations.end();
			 it != end; ++it)
		{
			const LLUUID& id = it->first;
			// Do not cancel a ground-sit anim, as viewers use this animation's
			// status in determining whether we are sitting.
			if (id != ANIM_AGENT_SIT_GROUND_CONSTRAINED)
			{
				// Stop this animation locally
				gAgentAvatarp->stopMotion(id, true);
				// ...and tell the server to tell everyone.
				anim_ids.emplace_back(id);
			}
		}
		gAgent.sendAnimationRequests(anim_ids, ANIM_REQUEST_STOP);

		gAgentAvatarp->resetSkeleton();

		gPipeline.resetVertexBuffers();
		gAgentAvatarp->startMotion(ANIM_AGENT_STAND, 5.f);
		gAgentAvatarp->startDefaultMotions();

		// Dirty all attachments' spatial groups to force a rebuild.
		gAgentAvatarp->refreshAttachments();

		gNotifications.add("CharacterRebuilt");
	}
}

// This entry should not be enabled if the customize appearance floater is
// visible
bool local_appearance_enabled(void*)
{
	return isAgentAvatarValid() && !LLFloaterCustomize::isVisible();
}

bool local_appearance_check(void*)
{
	return isAgentAvatarValid() && gAgentAvatarp->isEditingAppearance();
}

void handle_toggle_local_appearance(void*)
{
	if (isAgentAvatarValid() && !LLFloaterCustomize::isVisible())
	{
		if (gAgentAvatarp->isEditingAppearance())
		{
			LLVOAvatarSelf::onCustomizeEnd();
		}
		else
		{
			LLVOAvatarSelf::onCustomizeStart();
		}
	}
}

bool outfit_from_cof_enabled(void*)
{
	static LLCachedControl<bool> os_use_cof(gSavedSettings, "OSUseCOF");
	return gIsInSecondLife || os_use_cof;
}

bool outfit_from_cof_check(void*)
{
	static LLCachedControl<bool> from_cof(gSavedSettings,
										  "RestoreOutfitFromCOF");
	return from_cof && outfit_from_cof_enabled(NULL);
}

void handle_toggle_outfit_from_cof(void*)
{
	bool enabled = gSavedSettings.getBool("RestoreOutfitFromCOF");
	if (enabled)
	{
		gNotifications.add("DisablingRestoreFromCOF");
	}
	gSavedSettings.setBool("RestoreOutfitFromCOF", !enabled);
}

// *HACK for easily testing new avatar geometry
void handle_god_request_avatar_geometry(void*)
{
	if (gAgent.isGodlike())
	{
		gSelectMgr.sendGodlikeRequest("avatar toggle", NULL);
	}
}

void set_all_animation_time_factors(F32 time_factor)
{
	LLMotionController::setTimeFactorMultiplier(time_factor);
	for (S32 i = 0, count = LLCharacter::sInstances.size(); i < count; ++i)
	{
		LLCharacter* charp = LLCharacter::sInstances[i];
		if (charp)	// Paranoia
		{
			charp->setAnimTimeFactorMultiplier(time_factor);
		}
	}
}

bool is_slow_mo_animations(void*)
{
	if (!isAgentAvatarValid()) return false;

	return gAgentAvatarp->getAnimTimeFactorMultiplier() == 0.2f;
}

void slow_mo_animations(void*)
{
	if (!isAgentAvatarValid()) return;

	if (is_slow_mo_animations(NULL))
	{
		gAgentAvatarp->setAnimTimeFactorMultiplier(1.f);
	}
	else
	{
		gAgentAvatarp->setAnimTimeFactorMultiplier(0.2f);
	}
}

void handle_reset_animations_speed(void*)
{
	set_all_animation_time_factors(1.f);
}

void handle_slower_animations(void*)
{
	F32 time_factor = LLMotionController::getTimeFactorMultiplier();
	// Lower limit is at 10% of normal speed
	time_factor = llmax(time_factor - 0.1f, 0.1f);
	set_all_animation_time_factors(time_factor);
}

void handle_faster_animations(void*)
{
	F32 time_factor = LLMotionController::getTimeFactorMultiplier();
	// Upper limit is 200% speed
	time_factor = llmin(time_factor + 0.1f, 2.f);
	set_all_animation_time_factors(time_factor);
}

void handle_reset_avatars_animations(void*)
{
	// Get the list of avatars from the characters list which is much smaller
	// than the objects list. HB
	for (S32 i = 0, count = LLCharacter::sInstances.size(); i < count; ++i)
	{
		LLVOAvatar* avatarp = (LLVOAvatar*)LLCharacter::sInstances[i];
		if (avatarp && !avatarp->isDead() && !avatarp->mIsDummy &&
			!avatarp->isOrphaned())
		{
			for (LLVOAvatar::anim_it_t
					it = avatarp->mPlayingAnimations.begin(),
					end = avatarp->mPlayingAnimations.end();
				 it != end; ++it)
			{
				const LLUUID& anim_id = it->first;
				avatarp->stopMotion(anim_id, true);
				avatarp->startMotion(anim_id);
			}
		}
	}
}

void handle_test_male(void*)
{
//MK
	if (gRLenabled &&
		(gRLInterface.mContainsDetach || gRLInterface.contains("remoutfit") ||
		 gRLInterface.contains("addoutfit")))
	{
		return;
	}
//mk
	gAppearanceMgr.wearOutfitByName("Male Shape & Outfit");
#if 0
	gGestureList.requestResetFromServer(true);
#endif
}

void handle_test_female(void*)
{
//MK
	if (gRLenabled &&
		(gRLInterface.mContainsDetach || gRLInterface.contains("remoutfit") ||
		 gRLInterface.contains("addoutfit")))
	{
		return;
	}
//mk
	gAppearanceMgr.wearOutfitByName("Female Shape & Outfit");
#if 0
	gGestureList.requestResetFromServer(false);
#endif
}

void handle_toggle_pg(void*)
{
	gAgent.setTeen(!gAgent.isTeen());

	LLFloaterWorldMap::reloadIcons(NULL);

	llinfos << "PG status set to " << (S32)gAgent.isTeen() << llendl;
}

void handle_dump_attachments(void*)
{
	if (!isAgentAvatarValid())
	{
		return;
	}

	LLViewerJointAttachment* attachment;
	LLViewerObject* object;
	for (S32 i = 0, count = gAgentAvatarp->mAttachedObjectsVector.size();
		 i < count; ++i)
	{
		attachment = gAgentAvatarp->mAttachedObjectsVector[i].second;
		if (!attachment)
		{
			llwarns << "NULL attachment point detected !" << llendl;
			continue;
		}

		object = gAgentAvatarp->mAttachedObjectsVector[i].first;
		if (!object)
		{
			llwarns << attachment->getName() << ": NULL object attached"
					<< llendl;
		}
		else if (object->mDrawable.isNull())
		{
			llwarns << attachment->getName() << ": "
					<< object->getAttachmentItemID() << " got a NULL drawable"
					<< " - object position = " << object->getPosition()
					<< llendl;
		}
		else
		{
			llinfos << attachment->getName() << ": "
					<< object->getAttachmentItemID()
					<< (object->mDrawable->isRenderType(0) ? " - invisible"
														   : " - visible")
					<< " - drawable position = "
					<< object->mDrawable->getPosition()
					<< " - object position = " << object->getPosition()
					<< llendl;
		}
	}
}

void handle_dump_avatar_local_textures(void*)
{
	if (isAgentAvatarValid())
	{
		gAgentAvatarp->dumpLocalTextures();
	}
}

void handle_avatar_textures(void*)
{
	LLFloaterAvatarTextures::show(gAgentID);
}

bool enable_avatar_textures(void*)
{
	if (gAgent.isGodlikeWithoutAdminMenuFakery())
	{
		return true;
	}
	// Only allow to examine textures if every worn wearable can be exported
	// by the agent. We reuse HBObjectBackup's logic which implements both the
	// "created by agent" condition for Second Life and the "exportable" (or
	// full perm) condition for OpenSIM grids.
	for (S32 t = (S32)LLWearableType::WT_SKIN;
		 t  < (S32)LLWearableType::WT_COUNT; ++t)
	{
		LLWearableType::EType type = (LLWearableType::EType)t;
		U32 count = gAgentWearables.getWearableCount(type);
		for (U32 index = 0; index < count; ++index)
		{
			LLViewerWearable* wearable =
				gAgentWearables.getViewerWearable(type, index);
			if (!wearable) continue;

			LLViewerInventoryItem* itemp =
				gInventory.getItem(wearable->getItemID());
			if (itemp &&
				!HBObjectBackup::validatePerms(&itemp->getPermissions()))
			{
				return false;
			}
		}
	}
	return true;
}

void dump_avatar_xml_callback(HBFileSelector::ESaveFilter,
							  std::string& filename, void* userdata)
{
	if (isAgentAvatarValid())
	{
		gAgentAvatarp->dumpArchetypeXML(filename);
	}
}

void handle_dump_avatar_xml(void*)
{
	// Open the file save dialog
	HBFileSelector::saveFile(HBFileSelector::FFSAVE_XML, "archetype.xml",
							 dump_avatar_xml_callback);
}

#if LL_EXPORT_AVATAR_OBJ
void handle_export_avatar(void*)
{
	ALWavefrontSaver::exportAvatar();
}

void handle_export_avatar_with_attachments(void*)
{
	ALWavefrontSaver::exportAvatar(true);
}
#endif

bool enable_autopilot_begin_record(void*)
{
	return !gAgentPilot.isRecording();
}

bool enable_autopilot_end_record(void*)
{
	return gAgentPilot.isRecording();
}

bool enable_autopilot_start_playback(void*)
{
	return gAgentPilot.hasRecord() && !gAgentPilot.isPlaying() &&
		   !gAgentPilot.isRecording();
}

bool enable_autopilot_stop_playback(void*)
{
	return gAgentPilot.isPlaying();
}

// Puppetry sub-menu

bool enable_launch_puppetry(void*)
{
	return !HBFileSelector::isInUse() && LLPuppetMotion::enabled() &&
		   !LLPuppetModule::getInstance()->havePuppetModule();
}

void launch_leap_callback(HBFileSelector::ELoadFilter,
						  std::string& filename, void*)
{
	if (!filename.empty())
	{
		LLPuppetModule::getInstance()->launchLeapPlugin(filename);
	}
}

void handle_launch_puppetry(void*)
{
	if (enable_launch_puppetry(NULL))
	{
		HBFileSelector::loadFile(HBFileSelector::FFLOAD_ALL,
								 launch_leap_callback);
	}
}

bool enable_launch_prev_puppetry(void*)
{
	LLCachedControl<std::string> cmd(gSavedSettings, "PuppetryLastCommand");
	return LLPuppetMotion::enabled() &&
		   !LLPuppetModule::getInstance()->havePuppetModule() &&
		   !std::string(cmd).empty();
}

void handle_launch_prev_puppetry(void*)
{
	std::string command = gSavedSettings.getString("PuppetryLastCommand");
	if (!command.empty() && LLPuppetMotion::enabled())
	{
		LLPuppetModule* modulep = LLPuppetModule::getInstance();
		if (!modulep->havePuppetModule() &&
			!modulep->launchLeapCommand(command))
		{
			// Clear the command, since it is obviously invalid... HB
			gSavedSettings.setString("PuppetryLastCommand", "");
		}
	}
}

bool enable_puppetry_actions(void*)
{
	return LLPuppetMotion::enabled() &&
		   LLPuppetModule::getInstance()->havePuppetModule();
}

void handle_stop_puppetry(void*)
{
	if (enable_puppetry_actions(NULL))
	{
		LLPuppetModule* modulep = LLPuppetModule::getInstance();
		modulep->setSending(false);
		modulep->setEcho(false);
		modulep->clearLeapModule();
	}
}

void handle_puppetry_toggle_send(void*)
{
	if (enable_puppetry_actions(NULL))
	{
		LLPuppetModule* modulep = LLPuppetModule::getInstance();
		modulep->setSending(!modulep->isSending());
	}
}

bool puppetry_send_check(void*)
{
	return enable_puppetry_actions(NULL) &&
		   LLPuppetModule::getInstance()->isSending();
}

bool enable_puppetry_receive(void*)
{
	return LLPuppetMotion::enabled();
}

void handle_puppetry_toggle_receive(void*)
{
	if (LLPuppetMotion::enabled())
	{
		LLPuppetModule* modulep = LLPuppetModule::getInstance();
		modulep->setReceiving(!modulep->isReceiving());
	}
}

bool puppetry_receive_check(void*)
{
	return LLPuppetMotion::enabled() &&
		   LLPuppetModule::getInstance()->isReceiving();
}

bool puppetry_echo_check(void*)
{
	return enable_puppetry_actions(NULL) &&
		   LLPuppetModule::getInstance()->getEcho();
}

void puppetry_toggle_part(void* user_data)
{
	if (enable_puppetry_actions(NULL))
	{
		S32 part = S32(intptr_t(user_data));
		LLPuppetModule* modulep = LLPuppetModule::getInstance();
		modulep->setEnabledPart(part, !modulep->getEnabledPart(part));
	}
}

bool puppetry_check_part(void* user_data)
{
	S32 part = S32(intptr_t(user_data));
	return enable_puppetry_actions(NULL) &&
		   LLPuppetModule::getInstance()->getEnabledPart(part);
}

void init_puppetry_menu(LLMenuGL* menu)
{
	menu->append(new LLMenuItemCheckGL("Use puppetry when available",
									   menu_toggle_control, NULL,
									   menu_check_control,
									   (void*)"PuppetryAllowed"));
	menu->appendSeparator();
	menu->append(new LLMenuItemCallGL("Launch plug-in...",
									  handle_launch_puppetry,
									  enable_launch_puppetry));
	menu->append(new LLMenuItemCallGL("Launch previous plug-in",
									  handle_launch_prev_puppetry,
									  enable_launch_prev_puppetry));
	menu->append(new LLMenuItemCallGL("Stop running plug-in",
									  handle_stop_puppetry,
									  enable_puppetry_actions));
	menu->append(new LLMenuItemCheckGL("Send data",
									   handle_puppetry_toggle_send,
									   enable_puppetry_actions,
									   puppetry_send_check, NULL));
	menu->append(new LLMenuItemCheckGL("Receive data",
									   handle_puppetry_toggle_receive,
									   enable_puppetry_receive,
									   puppetry_receive_check, NULL));
	menu->append(new LLMenuItemCheckGL("Use server echo on self",
									   menu_toggle_control,
									   enable_puppetry_actions,
									   puppetry_echo_check,
									   (void*)"PuppetryUseServerEcho"));
	menu->append(new LLMenuItemCheckGL("Send binary LLSD data to plugin",
									   menu_toggle_control,
									   enable_launch_puppetry,
									   menu_check_control,
									   (void*)"PuppetryBinaryOutputStream"));
#if LL_USE_NEW_DESERIALIZE
	menu->append(new LLMenuItemCheckGL("Get binary LLSD data from plugin (BROKEN)",
									   menu_toggle_control,
									   enable_launch_puppetry,
									   menu_check_control,
									   (void*)"PuppetryBinaryInputStream"));
#endif
	menu->append(new LLMenuItemCheckGL("Send attachments data to server",
									   menu_toggle_control, NULL,
									   menu_check_control,
									   (void*)"PuppetrySendAttachmentsData"));

	LLMenuGL* sub = new LLMenuGL("Puppeteered parts");
	sub->append(new LLMenuItemCheckGL("Head", puppetry_toggle_part,
									  enable_puppetry_actions,
									  puppetry_check_part, (void*)1));
	sub->append(new LLMenuItemCheckGL("Face", puppetry_toggle_part,
									  enable_puppetry_actions,
									  puppetry_check_part, (void*)2));
	sub->append(new LLMenuItemCheckGL("Left hand", puppetry_toggle_part,
									  enable_puppetry_actions,
									  puppetry_check_part, (void*)4));
	sub->append(new LLMenuItemCheckGL("Right hand", puppetry_toggle_part,
									  enable_puppetry_actions,
									  puppetry_check_part, (void*)8));
	sub->append(new LLMenuItemCheckGL("Fingers", puppetry_toggle_part,
									  enable_puppetry_actions,
									  puppetry_check_part, (void*)16));
	sub->createJumpKeys();
	menu->appendMenu(sub);

	menu->createJumpKeys();
}

// End of puppetry sub-menu

void handle_grab_baked_texture(void* data)
{
	EBakedTextureIndex index = (EBakedTextureIndex)((intptr_t)data);
	if (isAgentAvatarValid())
	{
		const LLUUID& asset_id = gAgentAvatarp->grabBakedTexture(index);
		llinfos << "Adding baked texture " << asset_id << " to inventory."
				<< llendl;
		LLAssetType::EType asset_type = LLAssetType::AT_TEXTURE;
		LLInventoryType::EType inv_type = LLInventoryType::IT_TEXTURE;
		LLUUID folder_id(gInventory.findChoosenCategoryUUIDForType(LLFolderType::FT_TEXTURE));
		if (folder_id.notNull())
		{
			std::string name;
			name = "Baked " +
				   gAvatarAppDictp->getBakedTexture(index)->mNameCapitalized +
				   " Texture";

			LLUUID item_id;
			item_id.generate();
			LLPermissions perm;
			perm.init(gAgentID, gAgentID, LLUUID::null, LLUUID::null);
			U32 next_owner_perm = PERM_MOVE | PERM_TRANSFER;
			perm.initMasks(PERM_ALL, PERM_ALL, PERM_NONE, PERM_NONE,
						   next_owner_perm);
			time_t creation_date_now = time_corrected();
			LLPointer<LLViewerInventoryItem> item =
				new LLViewerInventoryItem(item_id, folder_id, perm, asset_id,
										  asset_type, inv_type, name,
										  LLStringUtil::null,
										  LLSaleInfo::DEFAULT,
										  LLInventoryItem::II_FLAGS_NONE,
										  creation_date_now);

			item->updateServer(true);
			gInventory.updateItem(item);
			gInventory.notifyObservers();

			// Show the preview panel for textures to let user know that the
			// image is now in inventory.
			LLFloaterInventory* inv = LLFloaterInventory::getActiveFloater();
			if (inv)
			{
				// Save focused element
				LLFocusableElement* focus_ctrl = gFocusMgr.getKeyboardFocus();

				inv->getPanel()->setSelection(item_id, TAKE_FOCUS_NO);
				inv->getPanel()->openSelected();

				// Restore keyboard focus
				gFocusMgr.setKeyboardFocus(focus_ctrl);
			}
		}
		else
		{
			llwarns << "Cannot find a folder to put the texture in" << llendl;
		}
	}
}

bool enable_grab_baked_texture(void* data)
{
	if (isAgentAvatarValid())
	{
		EBakedTextureIndex index = (EBakedTextureIndex)((intptr_t)data);
		return gAgentAvatarp->canGrabBakedTexture(index);
	}
	else
	{
		return false;
	}
}

void init_debug_baked_texture_menu(LLMenuGL* menu)
{
	menu->append(new LLMenuItemCallGL("Hair", handle_grab_baked_texture,
									  enable_grab_baked_texture,
									  (void*)BAKED_HAIR));
	menu->append(new LLMenuItemCallGL("Iris", handle_grab_baked_texture,
									  enable_grab_baked_texture,
									  (void*)BAKED_EYES));
	menu->append(new LLMenuItemCallGL("Head", handle_grab_baked_texture,
									  enable_grab_baked_texture,
									  (void*)BAKED_HEAD));
	menu->append(new LLMenuItemCallGL("Upper body", handle_grab_baked_texture,
									  enable_grab_baked_texture,
									  (void*)BAKED_UPPER));
	menu->append(new LLMenuItemCallGL("Lower body", handle_grab_baked_texture,
									  enable_grab_baked_texture,
									  (void*)BAKED_LOWER));
	menu->append(new LLMenuItemCallGL("Skirt", handle_grab_baked_texture,
									  enable_grab_baked_texture,
									  (void*)BAKED_SKIRT));
	menu->createJumpKeys();
}

void init_debug_character_menu(LLMenuGL* menu)
{
	LLMenuGL* sub = new LLMenuGL("Auto-pilot recorder");

	sub->append(new LLMenuItemCallGL("Begin record", LLAgentPilot::beginRecord,
									 enable_autopilot_begin_record));
	sub->append(new LLMenuItemCallGL("End record", LLAgentPilot::endRecord,
									 enable_autopilot_end_record));
	sub->append(new LLMenuItemCallGL("Forget record",
									 LLAgentPilot::forgetRecord,
									 enable_autopilot_start_playback));
	sub->appendSeparator();
	sub->append(new LLMenuItemCallGL("Start playback",
									 LLAgentPilot::startPlayback,
									 enable_autopilot_start_playback));
	sub->append(new LLMenuItemCallGL("Stop playback",
									 LLAgentPilot::stopPlayback,
									 enable_autopilot_stop_playback));
	sub->append(new LLMenuItemToggleGL("Loop playback", &LLAgentPilot::sLoop));
	sub->append(new LLMenuItemToggleGL("Allow flying",
									   &LLAgentPilot::sAllowFlying));

	sub->createJumpKeys();
	menu->appendMenu(sub);

	sub = new LLMenuGL("Character tests");

	// HACK for easy testing of avatar geometry
	sub->append(new LLMenuItemCallGL("Toggle character geometry",
									 handle_god_request_avatar_geometry,
									 enable_god_customer_service, NULL));

	sub->append(new LLMenuItemCallGL("Test male", handle_test_male));

	sub->append(new LLMenuItemCallGL("Test female", handle_test_female));

	sub->append(new LLMenuItemCallGL("Force visual params to default",
									 LLAgent::clearVisualParams, NULL));

	sub->append(new LLMenuItemCallGL("Toggle PG", handle_toggle_pg));
#if 0	// This does not work at all...
	sub->append(new LLMenuItemCheckGL("Allow select avatar",
									  menu_toggle_control, NULL,
									  menu_check_control,
									 (void*)"AllowSelectAvatar"));
#endif
	sub->createJumpKeys();
	menu->appendMenu(sub);

	sub = new LLMenuGL("Character debugging");
	sub->append(new LLMenuItemCheckGL("Show collision skeleton",
									  LLPipeline::toggleRenderDebug, NULL,
									  LLPipeline::toggleRenderDebugControl,
									  (void*)LLPipeline::RENDER_DEBUG_AVATAR_VOLUME));
	sub->append(new LLMenuItemCheckGL("Show avatar joints",
									  LLPipeline::toggleRenderDebug, NULL,
									  LLPipeline::toggleRenderDebugControl,
									  (void*)LLPipeline::RENDER_DEBUG_AVATAR_JOINTS));
	sub->append(new LLMenuItemCheckGL("Show agent target",
									  LLPipeline::toggleRenderDebug, NULL,
									  LLPipeline::toggleRenderDebugControl,
									  (void*)LLPipeline::RENDER_DEBUG_AGENT_TARGET));
	sub->append(new LLMenuItemCheckGL("Show above for self only",
									  menu_toggle_control, NULL,
									  menu_check_control,
									  (void*)"ShowAvatarDebugForSelfOnly"));
#if 0	// This does not seem to produce any visible result...
	sub->appendSeparator();
	sub->append(new LLMenuItemToggleGL("Show attachment points",
									   &LLVOAvatar::sShowAttachmentPoints));
#endif
	sub->appendSeparator();
	sub->append(new LLMenuItemToggleGL("Debug joint updates",
									   &LLVOAvatar::sJointDebug));
	sub->append(new LLMenuItemToggleGL("Debug character visibility",
									   &LLVOAvatar::sDebugInvisible));
	sub->append(new LLMenuItemCallGL("Dump attachments",
									 handle_dump_attachments));
	sub->append(new LLMenuItemCallGL("Dump local textures",
									 handle_dump_avatar_local_textures,
									 enable_non_faked_god, NULL));
	sub->createJumpKeys();
	menu->appendMenu(sub);

	menu->appendSeparator();
	menu->append(new LLMenuItemCallGL("Appearance to XML...",
									  handle_dump_avatar_xml,
									  enable_picker_actions, NULL));
#if LL_EXPORT_AVATAR_OBJ
	menu->append(new LLMenuItemCallGL("Export as a Wavefront OBJ file...",
									  handle_export_avatar,
									  enable_picker_actions, NULL));
	menu->append(new LLMenuItemCallGL("Export with attachments as OBJ...",
									  handle_export_avatar_with_attachments,
									  enable_picker_actions, NULL));
#endif
	menu->append(new LLMenuItemCallGL("Reset avatar skeleton",
									  handle_rebuild_avatar));
	menu->append(new LLMenuItemCheckGL("Restore outfit from COF",
									   handle_toggle_outfit_from_cof,
									   outfit_from_cof_enabled,
									   outfit_from_cof_check, NULL));

	menu->appendSeparator();
	menu->append(new LLMenuItemCallGL("Rebake textures",
									  handle_rebake_textures, NULL, NULL, 'R',
									  MASK_ALT | MASK_CONTROL));
	menu->append(new LLMenuItemCheckGL("Aggressive avatar rebakes",
									   menu_toggle_control, NULL,
									   menu_check_control,
									   (void*)"AvatarAggressiveRebake"));
	menu->append(new LLMenuItemCheckGL("Local baking/appearance",
									   handle_toggle_local_appearance,
									   local_appearance_enabled,
									   local_appearance_check, NULL, 'L',
									   MASK_CONTROL | MASK_ALT| MASK_SHIFT));
	menu->append(new LLMenuItemCheckGL("Use large bakes (after restart)",
									   toggle_large_bakes, not_in_sl,
									   large_bakes_checked, NULL));

	sub = new LLMenuGL("Grab baked texture");
	init_debug_baked_texture_menu(sub);
	menu->appendMenu(sub);

	menu->append(new LLMenuItemCallGL("View avatar textures",
									  handle_avatar_textures,
									  enable_avatar_textures, NULL));
	menu->append(new LLMenuItemCheckGL("Report complexity changes",
									   menu_toggle_control, NULL,
									   menu_check_control,
									   (void*)"ShowMyComplexityChanges"));
	menu->append(new LLMenuItemCheckGL("Customize appearance lighting",
									   menu_toggle_control, NULL,
									   menu_check_control,
									   (void*)"AvatarCustomizeLighting"));
	menu->appendSeparator();
	menu->append(new LLMenuItemToggleGL("Tap-tap-hold to run",
										&gAllowTapTapHoldRun));
	menu->append(new LLMenuItemCheckGL("Spoof mouse-look mode",
									   menu_toggle_control, NULL,
									   menu_check_control,
									   (void*)"SpoofMouseLook",
									   'M',
									   MASK_SHIFT | MASK_ALT | MASK_CONTROL));
	menu->appendSeparator();
	menu->append(new LLMenuItemToggleGL("Animation info",
										&LLVOAvatar::sShowAnimationDebug));
	menu->append(new LLMenuItemCheckGL("Use new walk and run animations",
									   menu_toggle_control, NULL,
									   menu_check_control,
									   (void*)"UseNewWalkRun"));
	menu->append(new LLMenuItemCheckGL("Slow motion animations (self)",
									   &slow_mo_animations, NULL,
									   &is_slow_mo_animations, NULL));
	sub = new LLMenuGL("All avatars' animations speed");
	sub->append(new LLMenuItemCallGL("10% slower",
										  handle_slower_animations));
	sub->append(new LLMenuItemCallGL("10% faster",
										  handle_faster_animations));
	sub->append(new LLMenuItemCallGL("Reset to normal speed",
										  handle_reset_animations_speed));
	sub->createJumpKeys();
	menu->appendMenu(sub);
	menu->append(new LLMenuItemCallGL("Reset visible avatars' animations",
									   handle_reset_avatars_animations));

	sub = new LLMenuGL("Puppetry");
	init_puppetry_menu(sub);
	menu->appendMenu(sub);

	menu->appendSeparator();
	menu->append(new LLMenuItemToggleGL("Show look at",
										&LLHUDEffectLookAt::sDebugLookAt));
	menu->append(new LLMenuItemToggleGL("Show point at",
										&LLHUDEffectPointAt::sDebugPointAt));
	menu->appendSeparator();
	menu->append(new LLMenuItemToggleGL("Disable LOD",
										&LLAvatarJoint::sDisableLOD));
	menu->createJumpKeys();
}

void handle_region_dump_temp_asset_data(void*)
{
	llinfos << "Dumping temporary asset data to simulator logs" << llendl;
	std::vector<std::string> strings;
	LLUUID invoice;
	send_generic_message("dumptempassetdata", strings, invoice);
}

#if 0
void handle_region_clear_temp_asset_data(void*)
{
	llinfos << "Clearing temporary asset data" << llendl;
	std::vector<std::string> strings;
	LLUUID invoice;
	send_generic_message("cleartempassetdata", strings, invoice);
}
#endif

void handle_object_owner_permissive(void*)
{
	// only send this if they're a god.
	if (gAgent.isGodlike())
	{
		// do the objects.
		gSelectMgr.selectionSetObjectPermissions(PERM_BASE, true, PERM_ALL,
												 true);
		gSelectMgr.selectionSetObjectPermissions(PERM_OWNER, true, PERM_ALL,
												 true);
	}
}

void handle_object_owner_self(void*)
{
	// only send this if they're a god.
	if (gAgent.isGodlike())
	{
		gSelectMgr.sendOwner(gAgentID, gAgent.getGroupID(), true);
	}
}

// Shortcut to set owner permissions to not editable.
void handle_object_lock(void*)
{
	gSelectMgr.selectionSetObjectPermissions(PERM_OWNER, false, PERM_MODIFY);
}

void handle_object_asset_ids(void*)
{
	// only send this if they're a god.
	if (gAgent.isGodlike())
	{
		gSelectMgr.sendGodlikeRequest("objectinfo", "assetids");
	}
}

void derez_objects(EDeRezDestination dest, const LLUUID& dest_id)
{
	if (gAgent.cameraMouselook())
	{
		gAgent.changeCameraToDefault();
	}

	std::string error;
	std::vector<LLViewerObject*> objects_list;

	// Check conditions that we can't deal with, building a list of
	// everything that we'll actually be derezzing.
	LLViewerRegion* first_region = NULL;
	for (LLObjectSelection::valid_root_iterator
			iter = gSelectMgr.getSelection()->valid_root_begin(),
			end = gSelectMgr.getSelection()->valid_root_end();
		 iter != end; ++iter)
	{
		LLSelectNode* node = *iter;
		if (!node) continue;	// Paranoia

		LLViewerObject* object = node->getObject();
		if (!object) continue;	// Paranoia

		LLViewerRegion* region = object->getRegion();
		if (!first_region)
		{
			first_region = region;
		}
		else if (region != first_region)
		{
			// Derez does not work at all if the some of the objects are in
			// regions besides the first object selected.
			// ...crosses region boundaries
			error = "AcquireErrorObjectSpan";
			break;
		}
		if (object->isAvatar())
		{
			// ...Do not acquire avatars
			continue;
		}

		if (object->getNVPair("AssetContainer") && dest != DRD_RETURN_TO_OWNER)
		{
			llwarns << "Attempt to derez deprecated AssetContainer object type not supported."
					<< llendl;
			continue;
		}

		bool can_derez_current = false;
		switch (dest)
		{
			case DRD_TAKE_INTO_AGENT_INVENTORY:
			case DRD_TRASH:
				if (!object->isPermanentEnforced() &&
					((node->mPermissions->allowTransferTo(gAgentID) &&
					  object->permModify()) ||
					 node->allowOperationOnNode(PERM_OWNER,
												GP_OBJECT_MANIPULATE)))
				{
					can_derez_current = true;
				}
				break;

			case DRD_RETURN_TO_OWNER:
				can_derez_current = true;
				break;

			default:
				if (gAgent.isGodlike() ||
					(object->permCopy() &&
					 node->mPermissions->allowTransferTo(gAgentID)))
				{
					can_derez_current = true;
				}
		}
		if (can_derez_current)
		{
			objects_list.push_back(object);
		}
	}

	// This constant is based on (1200 - HEADER_SIZE) / 4 bytes per
	// root.  I lopped off a few (33) to provide a bit
	// pad. HEADER_SIZE is currently 67 bytes, most of which is UUIDs.
	// This gives us a maximum of 63500 root objects - which should
	// satisfy anybody.
	constexpr S32 MAX_ROOTS_PER_PACKET = 250;
	constexpr S32 MAX_PACKET_COUNT = 254;
	F32 packets = ceil((F32)objects_list.size() / (F32)MAX_ROOTS_PER_PACKET);
	if (packets > (F32)MAX_PACKET_COUNT)
	{
		error = "AcquireErrorTooManyObjects";
	}

	if (error.empty() && objects_list.size() > 0)
	{
		U8 d = (U8)dest;
		LLUUID tid;
		tid.generate();
		U8 packet_count = (U8)packets;
		S32 object_index = 0;
		S32 objects_in_packet = 0;
		LLMessageSystem* msg = gMessageSystemp;
		for (U8 packet_number = 0; packet_number < packet_count; ++packet_number)
		{
			msg->newMessageFast(_PREHASH_DeRezObject);
			msg->nextBlockFast(_PREHASH_AgentData);
			msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
			msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
			msg->nextBlockFast(_PREHASH_AgentBlock);
			msg->addUUIDFast(_PREHASH_GroupID, gAgent.getGroupID());
			msg->addU8Fast(_PREHASH_Destination, d);
			msg->addUUIDFast(_PREHASH_DestinationID, dest_id);
			msg->addUUIDFast(_PREHASH_TransactionID, tid);
			msg->addU8Fast(_PREHASH_PacketCount, packet_count);
			msg->addU8Fast(_PREHASH_PacketNumber, packet_number);
			objects_in_packet = 0;
			while (object_index < (S32)objects_list.size()  &&
				   objects_in_packet++ < MAX_ROOTS_PER_PACKET)

			{
				LLViewerObject* objectp = objects_list[object_index++];
				msg->nextBlockFast(_PREHASH_ObjectData);
				msg->addU32Fast(_PREHASH_ObjectLocalID, objectp->getLocalID());
				// VEFFECT: DerezObject
				LLHUDEffectSpiral::swirlAtPosition(objectp->getPositionGlobal());
			}
			msg->sendReliable(first_region->getHost());
		}
		make_ui_sound("UISndObjectRezOut");

		// Busy count decremented by inventory update, so only increment
		// if will be causing an update.
		if (dest != DRD_RETURN_TO_OWNER)
		{
			gWindowp->incBusyCount();
		}
	}
	else if (!error.empty())
	{
		gNotifications.add(error);
	}
}

void force_take_copy(void*)
{
	if (gSelectMgr.getSelection()->isEmpty()) return;
	const LLUUID& category_id =
		gInventory.findCategoryUUIDForType(LLFolderType::FT_OBJECT);
	derez_objects(DRD_FORCE_TO_GOD_INVENTORY, category_id);
}

void handle_force_parcel_owner_to_me(void*)
{
	gViewerParcelMgr.sendParcelGodForceOwner(gAgentID);
}

void handle_force_parcel_to_content(void*)
{
	gViewerParcelMgr.sendParcelGodForceToContent();
}

void handle_claim_public_land(void*)
{
	if (gViewerParcelMgr.getSelectionRegion() != gAgent.getRegion())
	{
		gNotifications.add("ClaimPublicLand");
		return;
	}

	LLVector3d west_south_global;
	LLVector3d east_north_global;
	gViewerParcelMgr.getSelection(west_south_global, east_north_global);
	LLVector3 west_south = gAgent.getPosAgentFromGlobal(west_south_global);
	LLVector3 east_north = gAgent.getPosAgentFromGlobal(east_north_global);

	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessage("GodlikeMessage");
	msg->nextBlock("AgentData");
	msg->addUUID("AgentID", gAgentID);
	msg->addUUID("SessionID", gAgentSessionID);
	msg->addUUIDFast(_PREHASH_TransactionID, LLUUID::null); //not used
	msg->nextBlock("MethodData");
	msg->addString("Method", "claimpublicland");
	msg->addUUID("Invoice", LLUUID::null);
	std::string buffer;
	buffer = llformat("%f", west_south.mV[VX]);
	msg->nextBlock("ParamList");
	msg->addString("Parameter", buffer);
	buffer = llformat("%f", west_south.mV[VY]);
	msg->nextBlock("ParamList");
	msg->addString("Parameter", buffer);
	buffer = llformat("%f", east_north.mV[VX]);
	msg->nextBlock("ParamList");
	msg->addString("Parameter", buffer);
	buffer = llformat("%f", east_north.mV[VY]);
	msg->nextBlock("ParamList");
	msg->addString("Parameter", buffer);
	gAgent.sendReliableMessage();
}

void handle_force_delete(void*)
{
	gSelectMgr.selectForceDelete();
}

void init_server_menu(LLMenuGL* menu)
{
	LLMenuGL* sub = new LLMenuGL("Object");
	sub->append(new LLMenuItemCallGL("Take copy", force_take_copy,
									 enable_god_customer_service, NULL, 'O',
									 MASK_SHIFT | MASK_ALT | MASK_CONTROL));
	sub->append(new LLMenuItemCallGL("Force owner to me",
									 handle_object_owner_self,
									 enable_god_customer_service));
	sub->append(new LLMenuItemCallGL("Force owner permissive",
									 handle_object_owner_permissive,
									 enable_god_customer_service));
	sub->append(new LLMenuItemCallGL("Delete", handle_force_delete,
									 enable_god_customer_service, NULL,
									 KEY_DELETE,
									 MASK_SHIFT | MASK_ALT | MASK_CONTROL));
	sub->append(new LLMenuItemCallGL("Lock", handle_object_lock,
									 enable_god_customer_service, NULL, 'L',
									 MASK_SHIFT | MASK_ALT | MASK_CONTROL));
	sub->append(new LLMenuItemCallGL("Get asset IDs", handle_object_asset_ids,
									 enable_god_customer_service, NULL, 'I',
									 MASK_SHIFT | MASK_ALT | MASK_CONTROL));
	sub->createJumpKeys();
	menu->appendMenu(sub);

	sub = new LLMenuGL("Parcel");
	sub->append(new LLMenuItemCallGL("Owner to me",
									 handle_force_parcel_owner_to_me,
									 enable_god_customer_service, NULL));
	sub->append(new LLMenuItemCallGL("Set to Linden contents",
									 handle_force_parcel_to_content,
									 enable_god_customer_service, NULL, 'C',
									 MASK_SHIFT | MASK_ALT | MASK_CONTROL));
	sub->appendSeparator();
	sub->append(new LLMenuItemCallGL("Claim public land",
									 handle_claim_public_land,
									 enable_god_customer_service));
	sub->createJumpKeys();
	menu->appendMenu(sub);

	sub = new LLMenuGL("Region");
	sub->append(new LLMenuItemCallGL("Dump temp asset data",
									 handle_region_dump_temp_asset_data,
									 enable_god_customer_service, NULL));
	sub->createJumpKeys();
	menu->appendMenu(sub);

	menu->append(new LLMenuItemCallGL("God tools...", handle_god_tools,
									  enable_god_basic, NULL));

	menu->appendSeparator();

	menu->append(new LLMenuItemCallGL("Save region state",
				 					  LLPanelRegionTools::onSaveState,
									  enable_god_customer_service, NULL));

	menu->createJumpKeys();
}

bool can_toggle_snapshot_post_proc(void*)
{
	return gUsePBRShaders;
}

bool no_post_proc_check_control(void*)
{
	static LLCachedControl<bool> no_post(gSavedSettings,
										 "RenderSnapshotNoPost");
	return gUsePBRShaders && no_post;
}

void init_client_menu(LLMenuGL* menu)
{
	LLMenuGL* sub = new LLMenuGL("Consoles");
	init_debug_console_menu(sub);
	menu->appendMenu(sub);

	sub = new LLMenuGL("HUD info");
	init_hud_info_menu(sub);
	menu->appendMenu(sub);

	sub = new LLMenuGL("User interface");
	init_debug_ui_menu(sub);
	menu->appendMenu(sub);

	sub = new LLMenuGL("Rendering");
	init_debug_rendering_menu(sub);
	menu->appendMenu(sub);

	sub = new LLMenuGL("World");
	init_debug_world_menu(sub);
	menu->appendMenu(sub);

	sub = new LLMenuGL("Character");
	init_debug_character_menu(sub);
	menu->appendMenu(sub);

//MK
	if (gRLenabled)
	{
		sub = new LLMenuGL("RestrainedLove");
		init_restrained_love_menu(sub);
		menu->appendMenu(sub);
	}
//mk

	sub = new LLMenuGL("Lua scripting");
	init_lua_scripting_menu(sub);
	menu->appendMenu(sub);

	sub = new LLMenuGL("Network");
	init_network_menu(sub);
	menu->appendMenu(sub);

	sub = new LLMenuGL("Caches");
	init_caches_menu(sub);
	menu->appendMenu(sub);

	sub = new LLMenuGL("Media");
	init_media_menu(sub);
	menu->appendMenu(sub);

	menu->appendSeparator();

	menu->append(new LLMenuItemCheckGL("High-res snapshot",
									   menu_toggle_control, NULL,
									   menu_check_control,
									   (void*)"HighResSnapshot"));

	menu->append(new LLMenuItemCheckGL("No post-processing for snapshots",
									   menu_toggle_control,
									   can_toggle_snapshot_post_proc,
									   no_post_proc_check_control,
									   (void*)"RenderSnapshotNoPost"));

	menu->append(new LLMenuItemCheckGL("Quiet snapshots to disk",
									   menu_toggle_control, NULL,
									   menu_check_control,
									   (void*)"QuietSnapshotsToDisk"));

	menu->append(new LLMenuItemCallGL("Compress images to JPEG2000...",
									  handle_compress_image,
									  enable_picker_actions, NULL));

	menu->append(new LLMenuItemCheckGL("Debug permissions",
									   menu_toggle_control, NULL,
									   menu_check_control,
									   (void*)"DebugPermissions"));
	menu->appendSeparator();

#if LL_WINDOWS
	menu->append(new LLMenuItemCheckGL("Console window (after restart)",
									   menu_toggle_control, NULL,
									   menu_check_control,
									   (void*)"ShowConsoleWindow"));
	menu->appendSeparator();
#endif

	menu->append(new LLMenuItemCallGL("Debug settings...",
									  handle_show_debug_settings, NULL, NULL,
									  'S', MASK_ALT | MASK_CONTROL));

	menu->append(new LLMenuItemCallGL("Save current settings to file...",
									  handle_save_settings_to_xml,
									  enable_picker_actions, NULL));
	menu->append(new LLMenuItemCallGL("Save default settings to file...",
									  handle_save_settings_to_xml,
									  enable_picker_actions, (void*)1));
	menu->append(new LLMenuItemCallGL("Save current account settings to...",
									  handle_save_settings_to_xml,
									  enable_picker_actions, (void*)2));
	menu->append(new LLMenuItemCallGL("Save default account settings to...",
									  handle_save_settings_to_xml,
									  enable_picker_actions, (void*)3));
	menu->appendSeparator();

	menu->append(new LLMenuItemCheckGL("View admin options",
									   handle_admin_override_toggle, NULL,
									   check_admin_override, NULL,
									   'V', MASK_CONTROL | MASK_ALT));

	menu->append(new LLMenuItemCallGL("Request admin status",
									  handle_god_mode,
									  enable_god_options, NULL,
									  'G', MASK_ALT | MASK_CONTROL));

	menu->append(new LLMenuItemCallGL("Leave admin status",
									  handle_leave_god_mode,
									  enable_god_options, NULL, 'G',
									  MASK_ALT | MASK_SHIFT | MASK_CONTROL));
#if LL_ENABLE_CRASH_TEST
	menu->appendSeparator();
	menu->append(new LLMenuItemCallGL("Test llerrs crash",
									  handle_llerrs_test));
#endif

	menu->createJumpKeys();
}

static std::vector<LLPointer<view_listener_t> > sMenus;

void cleanup_menus()
{
	delete gMenuHolderp;
	gMenuHolderp = NULL;
	LLMenuGL::sMenuContainer = NULL;

	// NULLifiy menu and menu children pointers (all got deleted automatically
	// as children of gMenuHolderp).
	gMenuParcelObserver = NULL;
	gPieSelfp = NULL;
	gPieAvatarp = NULL;
	gPieObjectp = NULL;
	gPieAttachmentp = NULL;
	gPieLandp = NULL;
	gPieParticlep = NULL;
	gLoginMenuBarViewp = NULL;
	gMenuBarViewp = NULL;
	gDetachScreenPieMenup = NULL;
	gDetachPieMenup = NULL;
	gAttachScreenPieMenup = NULL;
	gAttachPieMenup = NULL;
	gMutesPieMenup = NULL;
	gPieObjectMutep = NULL;

	sMenus.clear();
}

//-----------------------------------------------------------------------------
// Object pie menu
//-----------------------------------------------------------------------------

class LLObjectReportAbuse final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		LLViewerObject* objectp = gSelectMgr.getSelection()->getPrimaryObject();
		if (objectp)
		{
			LLFloaterReporter::showFromObject(objectp->getID());
		}
		return true;
	}
};

// Enabled it you clicked an object
class LLObjectEnableReportAbuse final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		bool new_value = gSelectMgr.getSelection()->getObjectCount() != 0;
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

class LLObjectTouch final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		LLViewerObject* object = gSelectMgr.getSelection()->getPrimaryObject();
		if (!object) return true;

		LLPickInfo pick = gToolPie.getPick();

//MK
		if (gRLenabled && !gRLInterface.canTouch(object, pick.mIntersection))
		{
			return true;
		}
//mk

		LLMessageSystem* msg = gMessageSystemp;

		msg->newMessageFast(_PREHASH_ObjectGrab);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
		msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
		msg->nextBlockFast(_PREHASH_ObjectData);
		msg->addU32Fast(_PREHASH_LocalID, object->mLocalID);
		msg->addVector3Fast(_PREHASH_GrabOffset, LLVector3::zero);
		msg->nextBlock("SurfaceInfo");
		msg->addVector3("UVCoord", LLVector3(pick.mUVCoords));
		msg->addVector3("STCoord", LLVector3(pick.mSTCoords));
		msg->addS32Fast(_PREHASH_FaceIndex, pick.mObjectFace);
		msg->addVector3("Position", pick.mIntersection);
		msg->addVector3("Normal", pick.mNormal);
		msg->addVector3("Binormal", pick.mBinormal);
		msg->sendMessage(object->getRegion()->getHost());

		// *NOTE: Hope the packets arrive safely and in order or else
		// there will be some problems.
		// *TODO: Just fix this bad assumption.
		msg->newMessageFast(_PREHASH_ObjectDeGrab);
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
		msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
		msg->nextBlockFast(_PREHASH_ObjectData);
		msg->addU32Fast(_PREHASH_LocalID, object->mLocalID);
		msg->nextBlock("SurfaceInfo");
		msg->addVector3("UVCoord", LLVector3(pick.mUVCoords));
		msg->addVector3("STCoord", LLVector3(pick.mSTCoords));
		msg->addS32Fast(_PREHASH_FaceIndex, pick.mObjectFace);
		msg->addVector3("Position", pick.mIntersection);
		msg->addVector3("Normal", pick.mNormal);
		msg->addVector3("Binormal", pick.mBinormal);
		msg->sendMessage(object->getRegion()->getHost());

		return true;
	}
};

// One object must have touch sensor
class LLObjectEnableTouch final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		// Do not run this before the menu are fully initialized because the
		// static LLView pointer to attachment_touch cannot be properly
		// initialized while the pie menus are built which happens before the
		// main menu items are defined.
		if (!gMenusInitialized) return true;

		bool new_value = false;
		LLViewerObject* obj = gSelectMgr.getSelection()->getPrimaryObject();
		if (obj)
		{
			new_value |= obj->flagHandleTouch() || !obj->flagsLoaded();
			LLViewerObject* parent = (LLViewerObject*)obj->getParent();
			if (parent)
			{
				new_value |= parent->flagHandleTouch() ||
							 !parent->flagsLoaded();
			}
		}
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);

		// Update label based on the node touch name if available.
		LLSelectNode* node = gSelectMgr.getSelection()->getFirstRootNode();

		LLSD label;
		if (node && node->mValid && !node->mTouchName.empty())
		{
			label =  LLSD(node->mTouchName);
		}
		else
		{
			label = userdata["data"];
		}

		// Using static pointers prevents thousands of recursive calls to
		// getChild<T>() each time a menu is pulled down !
		static LLView* object_touch =
			gMenuHolderp->getChild<LLView>("Object Touch");
		object_touch->setValue(label);

		static LLView* attachment_touch =
			gMenuHolderp->getChild<LLView>("Attachment Object Touch");
		attachment_touch->setValue(label);

		return true;
	}
};

bool handle_object_open()
{
	LLViewerObject* objp = gSelectMgr.getSelection()->getPrimaryObject();
	if (!objp) return true;
//MK
	if (gRLenabled)
	{
		if (!gRLInterface.canEdit(objp))
		{
			return true;
		}
		if (!gRLInterface.canTouchFar(objp, gToolPie.getPick().mIntersection))
		{
			return true;
		}
	}
//mk

	LLFloaterOpenObject::show();
	return true;
}

class LLObjectOpen final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		return handle_object_open();
	}
};

class LLObjectEnableOpen final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		// Look for contents in root object, which is all the
		// LLFloaterOpenObject understands.
		LLViewerObject* objp = gSelectMgr.getSelection()->getPrimaryObject();
		bool new_value = objp != NULL;
		if (new_value)
		{
			LLViewerObject* rootp = objp->getRootEdit();
			if (!rootp)
			{
				new_value = false;
			}
			else
			{
				new_value = rootp->allowOpen();
			}
//MK
			if (new_value && gRLenabled)
			{
				if (!gRLInterface.canEdit(objp))
				{
					new_value = false;
				}
				else
				{
					new_value =
						gRLInterface.canTouchFar(objp,
												 gToolPie.getPick().mIntersection);
				}
			}
//mk
		}
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

class LLViewCheckCameraFrontView final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		ECameraMode mode = gAgent.getCameraMode();
		bool new_value = mode != CAMERA_MODE_MOUSELOOK &&
						 mode != CAMERA_MODE_CUSTOMIZE_AVATAR;
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

class LLToolsCheckBuildMode final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		bool new_value = gToolMgr.inEdit();
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

class LLToolsBuildMode final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		gToolMgr.toggleBuildMode();
		return true;
	}
};

//MK
void handle_toggle_flycam()
{
	LLViewerJoystick* joystick = LLViewerJoystick::getInstance();
	joystick->toggleFlycam();
	// Do not allow it if our camera distance is restricted
	if (gRLenabled && gRLInterface.mCamDistMax < EXTREMUM * 0.75f &&
		joystick->getOverrideCamera())
	{
		joystick->toggleFlycam();
	}
}
//mk

class LLViewJoystickFlycam final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		handle_toggle_flycam();
		return true;
	}
};

class LLViewCheckJoystickFlycam final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		bool new_val = LLViewerJoystick::getInstance()->getOverrideCamera();
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_val);
		return true;
	}
};

bool handle_object_edit()
{
//MK
	if (gRLenabled)
	{
		if (gRLInterface.mContainsRez)
		{
			return false;
		}
		if (gRLInterface.mContainsEdit)
		{
			LLViewerObject* objp = gSelectMgr.getSelection()->getFirstObject();
			if (!gRLInterface.canEdit(objp))
			{
				return false;
			}
		}	
	}
//mk

	gToolMgr.setCurrentToolset(gBasicToolset);
	gBasicToolset->selectTool(&gToolCompTranslate);

	// Could be first use
	LLFirstUse::useBuild();

	return true;
}

class LLObjectBuild final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
//MK
		if (gRLenabled)
		{
			if (gRLInterface.mContainsRez)
			{
				return false;
			}
			if (gRLInterface.mContainsEdit)
			{
				LLViewerObject* objp =
					gSelectMgr.getSelection()->getFirstObject();
				if (!gRLInterface.canEdit(objp))
				{
					return false;
				}
			}
		}
//mk
		if (gAgent.getFocusOnAvatar() && !gToolMgr.inEdit() &&
			gSavedSettings.getBool("EditCameraMovement"))
		{
			// Zoom in if we are looking at the avatar
			gAgent.setFocusOnAvatar(false);
			gAgent.setFocusGlobal(gToolPie.getPick());
			gAgent.cameraZoomIn(0.666f);
			gAgent.cameraOrbitOver(30.f * DEG_TO_RAD);
			gViewerWindowp->moveCursorToCenter();
		}
		else if (gSavedSettings.getBool("EditCameraMovement"))
		{
			gAgent.setFocusGlobal(gToolPie.getPick());
			gViewerWindowp->moveCursorToCenter();
		}

		gToolMgr.setCurrentToolset(gBasicToolset);
		gBasicToolset->selectTool(&gToolCompCreate);

		LLViewerJoystick* joystick = LLViewerJoystick::getInstance();
		joystick->moveObjects(true);
		joystick->setNeedsReset(true);

		// Could be first use
		LLFirstUse::useBuild();

		return true;
	}
};

class LLObjectEdit final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		if (!gFloaterToolsp) return false;
//MK
		if (gRLenabled)
		{
			if (gRLInterface.mContainsRez)
			{
				return false;
			}
			LLViewerObject* objp = gSelectMgr.getSelection()->getFirstObject();
			if (!gRLInterface.canEdit(objp))
			{
				return false;
			}
			if (!gRLInterface.canTouchFar(objp,
										  gToolPie.getPick().mIntersection))
			{
				return false;
			}
		}
//mk
		gViewerParcelMgr.deselectLand();

		if (gAgent.getFocusOnAvatar() && !gToolMgr.inEdit())
		{
			LLObjectSelectionHandle selection = gSelectMgr.getSelection();

			if (selection->getSelectType() == SELECT_TYPE_HUD ||
				!gSavedSettings.getBool("EditCameraMovement"))
			{
				// Always freeze camera in space, even if camera does not move
				// so, for example, follow-cam scripts cannot affect you when
				// in build mode
				gAgent.setFocusGlobal(gAgent.calcFocusPositionTargetGlobal(),
									  LLUUID::null);
				gAgent.setFocusOnAvatar(false);
			}
			else
			{
				gAgent.setFocusOnAvatar(false);
				LLViewerObject* selected_objectp = selection->getFirstRootObject();
				if (selected_objectp)
				{
					// zoom in on object center instead of where we clicked, as
					// we need to see the manipulator handles
					gAgent.setFocusGlobal(selected_objectp->getPositionGlobal(),
										  selected_objectp->getID());
					gAgent.cameraZoomIn(0.666f);
					gAgent.cameraOrbitOver(30.f * DEG_TO_RAD);
					gViewerWindowp->moveCursorToCenter();
				}
			}
		}

		gFloaterToolsp->open();

		gToolMgr.setCurrentToolset(gBasicToolset);
		gFloaterToolsp->setEditTool(&gToolCompTranslate);

		LLViewerJoystick* joystick = LLViewerJoystick::getInstance();
		joystick->moveObjects(true);
		joystick->setNeedsReset(true);

		// Could be first use
		LLFirstUse::useBuild();
		return true;
	}
};

bool handle_object_inspect()
{
//MK
	if (gRLenabled)
	{
		if (gRLInterface.mContainsShownames ||
			gRLInterface.mContainsShownametags)
		{
			return false;
		}
		LLViewerObject* objp = gSelectMgr.getSelection()->getPrimaryObject();
		if (!gRLInterface.canTouchFar(objp))
		{
			return false;
		}
	}
//mk
	LLViewerObject* vobj = gSelectMgr.getSelection()->getFirstRootObject(true);
	if (vobj)
	{
		LLVOAvatar* avatar = vobj->asAvatar();
		if (avatar && !avatar->mIsDummy)
		{
			HBFloaterInspectAvatar::show(avatar->getID());
			return true;
		}
	}

	LLFloaterInspect::show();
	return true;
}

class LLObjectInspect final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		return handle_object_inspect();
	}
};

class LLSelfInspect final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		HBFloaterInspectAvatar::show(gAgentID);
		return true;
	}
};

class LLObjectToggleMaxLOD final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		LLViewerObject* vobj =
			gSelectMgr.getSelection()->getFirstRootObject(true);
		if (vobj)
		{
			vobj->recursiveSetMaxLOD(!vobj->isLockedAtMaxLOD());
		}
		return true;
	}
};

class LLObjectEnableMaxLOD final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		LLViewerObject* vobj = gSelectMgr.getSelection()->getPrimaryObject();
		bool new_value = vobj && !vobj->isLockedAtMaxLOD();
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

class LLObjectEnableNormalLOD final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		LLViewerObject* vobj = gSelectMgr.getSelection()->getPrimaryObject();
		bool new_value = vobj && vobj->isLockedAtMaxLOD();
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

bool derender_object(const LLUUID& object_id)
{
	if (object_id.isNull()) return false;

	LLViewerObject* vobj = gObjectList.findObject(object_id);
//MK
	LLViewerObject* root = vobj ? vobj->getRootEdit() : NULL;
//mk

	// Do not derender ourselves neither our attachments
	if (find_avatar_from_object(vobj) == gAgentAvatarp ||
//MK
		// Do not derender seats when prevented to unsit
		(gRLenabled && gRLInterface.mContainsUnsit &&
		 ((vobj && vobj->isAgentSeat()) || (root && root->isAgentSeat()))))
//mk
	{
		return false;
	}

	// Remove object from selection, if part of it.
	gSelectMgr.removeObjectFromSelections(object_id);
	// Insert the object into the black list.
	LLViewerObjectList::sBlackListedObjects.emplace(object_id);

	// Update the derendered status in the radar.
	if (!vobj || vobj->asAvatar())
	{
		HBFloaterRadar::setRenderStatusDirty(object_id);
	}

	if (vobj)
	{
		// Derender by killing the object.
		gObjectList.killObject(vobj);
	}

	return true;
}

class LLObjectDerender final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		struct LLGetSelectedObjectsId final : public LLSelectedObjectFunctor
		{
			LLGetSelectedObjectsId(uuid_vec_t& ids)
			:	mIDs(ids)
			{
			}

			bool apply(LLViewerObject* objectp) override
			{
				if (objectp)
				{
					mIDs.emplace_back(objectp->getID());
				}
				return true;
			}

			uuid_vec_t& mIDs;
		};

		// Note: we cannot derender from inside the functor: this would
		// invalidate objects in the iterated selection and cause a crash. HB
		uuid_vec_t ids;
		LLGetSelectedObjectsId func(ids);
		gSelectMgr.getSelection()->applyToObjects(&func);
		for (U32 i = 0, count = ids.size(); i < count; ++i)
		{
			derender_object(ids[i]);
		}

		return true;
	}
};

class LLObjectEnableDerender final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		bool enable = true;
//MK
		if (gRLenabled && gRLInterface.mContainsUnsit &&
			gRLInterface.isSittingOnAnySelectedObject())
		{
			// Do not allow to derender an object we are sitting on when
			// RestrainedLove is enabled and we are forbidden to unsit.
			enable = false;
		}
		else
//mk
		{
			struct f final : public LLSelectedObjectFunctor
			{
				bool apply(LLViewerObject* objectp) override
				{
					// Do not allow to derender our own attachments
					return objectp &&
						   find_avatar_from_object(objectp) != gAgentAvatarp;
				}
			} func;
			enable = gSelectMgr.getSelection()->applyToObjects(&func);
		}
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(enable);
		return true;
	}
};

bool handle_go_to()
{
//MK
	if (gRLenabled && gAgent.forwardGrabbed())
	{
		// When llTakeControls() has been performed on CONTROL_FWD, do not
		// allow the go to action to prevent overriding any speed limitation or
		// movement restriction.
		return true;
	}
//mk
	// JAMESDEBUG try simulator autopilot
	std::vector<std::string> strings;
	std::string val;
	LLVector3d pos = gToolPie.getPick().mPosGlobal;
	val = llformat("%g", pos.mdV[VX]);
	strings.emplace_back(val);
	val = llformat("%g", pos.mdV[VY]);
	strings.emplace_back(val);
	val = llformat("%g", pos.mdV[VZ]);
	strings.emplace_back(val);
	send_generic_message("autopilot", strings);

	gViewerParcelMgr.deselectLand();

	if (isAgentAvatarValid() &&
		!gSavedSettings.getBool("AutoPilotLocksCamera"))
	{
		gAgent.setFocusGlobal(gAgent.getFocusTargetGlobal(),
							  gAgentAvatarp->getID());
	}
	else
	{
		// Snap camera back to behind avatar
		gAgent.setFocusOnAvatar();
	}

	return true;
}

class LLGoToObject final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		return handle_go_to();
	}
};

//---------------------------------------------------------------------------
// Land pie menu
//---------------------------------------------------------------------------

class LLLandBuild final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
//MK
		if (gRLenabled && gRLInterface.mContainsEdit)
		{
			return false;
		}
//mk
		gViewerParcelMgr.deselectLand();

		if (gAgent.getFocusOnAvatar() && !gToolMgr.inEdit() &&
			gSavedSettings.getBool("EditCameraMovement"))
		{
			// zoom in if we're looking at the avatar
			gAgent.setFocusOnAvatar(false);
			gAgent.setFocusGlobal(gToolPie.getPick());
			gAgent.cameraZoomIn(0.666f);
			gAgent.cameraOrbitOver(30.f * DEG_TO_RAD);
			gViewerWindowp->moveCursorToCenter();
		}
		else if (gSavedSettings.getBool("EditCameraMovement"))
		{
			// otherwise just move focus
			gAgent.setFocusGlobal(gToolPie.getPick());
			gViewerWindowp->moveCursorToCenter();
		}

		gToolMgr.setCurrentToolset(gBasicToolset);
		gBasicToolset->selectTool(&gToolCompCreate);

		// Could be first use
		LLFirstUse::useBuild();
		return true;
	}
};

class LLLandBuyPass final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		LLPanelLandGeneral::onClickBuyPass((void*)false);
		return true;
	}
};

class LLLandEnableBuyPass final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		bool new_value = LLPanelLandGeneral::enableBuyPass(NULL);
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

class LLEnableEdit final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		bool enable = false;
		LLViewerObject* objp = gSelectMgr.getSelection()->getPrimaryObject();
		if (objp)
		{
			enable = true;
//MK
			if (gRLenabled && !gRLInterface.canEdit(objp))
			{
				enable = false;
			}
//mk
		}
		else
		{
			// *HACK: See LLViewerParcelMgr::allowAgentBuild() for the "false"
			// flag.
			enable = gViewerParcelMgr.allowAgentBuild(false);
		}
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(enable);
		return true;
	}
};

class LLSelfRemoveAllAttachments final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
//MK
		if (gRLenabled && gRLInterface.mContainsDetach)
		{
			return false;
		}
//mk
		LLAgentWearables::userRemoveAllAttachments();
		return true;
	}
};

class LLSelfEnableRemoveAllAttachments final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
//MK
		if (gRLenabled && gRLInterface.mContainsDetach)
		{
			return false;
		}
//mk
		bool new_value = isAgentAvatarValid() &&
						 gAgentAvatarp->mAttachedObjectsVector.size() > 0;
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

class LLRemoveAllTempAttachments final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
//MK
		if (gRLenabled && gRLInterface.mContainsDetach)
		{
			return false;
		}
//mk
		LLAgentWearables::userRemoveAllAttachments(true);
		return true;
	}
};

class LLEnableRemoveAllTempAttachments final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
//MK
		if (gRLenabled && gRLInterface.mContainsDetach)
		{
			return false;
		}
//mk
		bool new_value = false;
		if (isAgentAvatarValid())
		{
			for (S32 i = 0,
					 count = gAgentAvatarp->mAttachedObjectsVector.size();
				 i < count; ++i)
			{
				LLViewerObject* object =
					gAgentAvatarp->mAttachedObjectsVector[i].first;
				if (object && object->isTempAttachment())
				{
					new_value = true;
					break;
				}
			}
		}
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

//---------------------------------------------------------------------------
// Avatar pie menu
//---------------------------------------------------------------------------

class LLObjectEnableMute final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
//MK
		if (gRLenabled &&
			(gRLInterface.mContainsShownames ||
			 gRLInterface.mContainsShownametags))
		{
			return false;
		}
//mk
		LLViewerObject* object = gSelectMgr.getSelection()->getPrimaryObject();
		// Do not mute our own objects
		bool new_value = object && !object->permYouOwner();
		if (new_value)
		{
			LLVOAvatar* avatar = find_avatar_from_object(object);
			if (avatar)
			{
				// It is an avatar
				LLNameValue* lastname = avatar->getNVPair("LastName");
				bool is_linden = lastname &&
								 !LLStringUtil::compareStrings(lastname->getString(),
															   "Linden");
				bool is_self = avatar->isSelf();
				new_value = !is_linden && !is_self;
			}
		}
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

class LLObjectMute final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		LLViewerObject* object = gSelectMgr.getSelection()->getPrimaryObject();
		if (!object) return true;

		std::string data = userdata.asString();

		LLUUID id;
		std::string name;
		LLMute::EType type;
		U32 flags = 0;
		LLVOAvatar* avatar = find_avatar_from_object(object);
		if (avatar)
		{
//MK
			if (gRLenabled &&
				(gRLInterface.mContainsShownames ||
				 gRLInterface.mContainsShownametags))
			{
				return false;
			}
//mk
			if (data == "chat")
			{
				flags = LLMute::flagTextChat;
			}
			else if (data == "voice")
			{
				flags = LLMute::flagVoiceChat;
			}
			else if (data == "sounds")
			{
				flags = LLMute::flagObjectSounds;
			}
			else if (data == "particles")
			{
				flags = LLMute::flagParticles;
			}

			id = avatar->getID();

			LLNameValue* firstname = avatar->getNVPair("FirstName");
			LLNameValue* lastname = avatar->getNVPair("LastName");
			if (firstname && lastname)
			{
				name = firstname->getString();
				name += " ";
				name += lastname->getString();
			}

			type = LLMute::AGENT;
		}
		else	// It is an object
		{
			if (data == "by_name")
			{
				type = LLMute::BY_NAME;
			}
			else
			{
				type = LLMute::OBJECT;
				id = object->getID();
			}
			LLSelectNode* node = gSelectMgr.getSelection()->getFirstRootNode();
			if (node)
			{
				name = node->mName;
			}
		}

		LLMute mute(id, name, type);
		if (LLMuteList::isMuted(mute.mID, mute.mName, flags))
		{
			LLMuteList::remove(mute, flags);
		}
		else if (LLMuteList::add(mute, flags))
		{
			LLFloaterMute::selectMute(mute.mID);
		}

		return true;
	}
};

class LLAvatarToggleMaxLOD final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		LLViewerObject* vobj = gSelectMgr.getSelection()->getPrimaryObject();
		if (!vobj) return true;

		LLVOAvatar* avatarp = find_avatar_from_object(vobj);
		if (!avatarp) return true;

		bool lock = false;
		for (S32 i = 0, count = avatarp->mAttachedObjectsVector.size();
			 i < count; ++i)
		{
			vobj = avatarp->mAttachedObjectsVector[i].first;
			if (!vobj) continue;	// Paranoia

			if (i == 0)
			{
				lock = !vobj->isLockedAtMaxLOD();
			}
			vobj->recursiveSetMaxLOD(lock);
		}

		return true;
	}
};

class LLAvatarEnableMaxLOD final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		bool new_value = false;
		LLViewerObject* vobj = gSelectMgr.getSelection()->getPrimaryObject();
		if (vobj)
		{
			LLVOAvatar* avatarp = find_avatar_from_object(vobj);
			if (avatarp && avatarp->mAttachedObjectsVector.size())
			{
				vobj = avatarp->mAttachedObjectsVector[0].first;
				new_value = !vobj->isLockedAtMaxLOD();
			}
		}
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

class LLAvatarEnableNormalLOD final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		bool new_value = false;
		LLViewerObject* vobj = gSelectMgr.getSelection()->getPrimaryObject();
		if (vobj)
		{
			LLVOAvatar* avatarp = find_avatar_from_object(vobj);
			if (avatarp && avatarp->mAttachedObjectsVector.size())
			{
				vobj = avatarp->mAttachedObjectsVector[0].first;
				new_value = vobj->isLockedAtMaxLOD();
			}
		}
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

//---------------------------------------------------------------------------
// Particles pie menu
//---------------------------------------------------------------------------

class LLParticleEnableEntry final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		std::string data = userdata.asString();
		const LLPickInfo& pick = gToolPie.getPick();
		bool new_value = pick.mParticleOwnerID.notNull() &&
						 pick.mParticleOwnerID != gAgentID &&
						 (data == "owner" || pick.mParticleSourceID.notNull());
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

class LLMuteParticle final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		std::string data = userdata.asString();
		LLUUID id = gToolPie.getPick().mParticleOwnerID;
		if (!data.empty() && id.notNull() && id != gAgentID)
		{
			std::string name;
			if (gCacheNamep)
			{
				gCacheNamep->getFullName(id, name);
			}

			LLMute::EType type;
			U32 flags = 0;

			const LLUUID& source_id = gToolPie.getPick().mParticleSourceID;
			if (data == "object" && source_id.notNull())
			{
				id = source_id;
				name += "'s object";
				type = LLMute::OBJECT;
			}
			else if (data == "owner")
			{
				flags = LLMute::flagParticles;
				type = LLMute::AGENT;
			}
			else
			{
				return true;
			}

			bool muted = true;
			LLMute mute(id, name, type);
			if (!LLMuteList::isMuted(mute.mID, mute.mName, flags))
			{
				muted = LLMuteList::add(mute, flags);
			}
			if (muted)
			{
				LLFloaterMute::selectMute(mute.mID);
			}
		}

		return true;
	}
};

class LLReportParticleAbuse final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		const LLUUID& owner_id = gToolPie.getPick().mParticleOwnerID;
		const LLUUID& source_id = gToolPie.getPick().mParticleSourceID;
		if (source_id.notNull() && owner_id.notNull() && owner_id != gAgentID)
		{
			LLFloaterReporter::showFromObject(source_id);
		}

		return true;
	}
};

class LLParticleRefreshTexture final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		const LLUUID& source_id = gToolPie.getPick().mParticleSourceID;
		if (source_id.isNull())
		{
			return true;
		}
		LLViewerObject* objectp = gObjectList.findObject(source_id);
		if (!objectp)
		{
			return true;
		}
		LLViewerPartSource* psrcp = objectp->getPartSource();
		if (!psrcp)
		{
			return true;
		}
		LLViewerTexture* imagep = psrcp->getImage();
		if (!imagep)
		{
			return true;
		}
		LLViewerFetchedTexture* texp =
			LLViewerTextureManager::staticCast(imagep);
		if (texp)
		{
			// Force a reload of the raw image
			texp->forceRefetch();
		}
		return true;
	}
};

//---------------------------------------------------------------------------
// Lua pie menu
//---------------------------------------------------------------------------

class LLPieLuaCall final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		S32 slice = userdata.asInteger();
		const LLPickInfo& pick = gToolPie.getPick();
		if (gLuaPiep)
		{
			gLuaPiep->onPieSliceClick(slice, pick);
		}

		return true;
	}
};

//---------------------------------------------------------------------------
// Formerly defined in the now removed llmenucommands.cpp
//---------------------------------------------------------------------------

void handle_chat(void*)
{
	if (!gChatBarp) return;

	// Give focus to chatbar if it is open but not focused
	if (gSavedSettings.getBool("ChatVisible") &&
		gFocusMgr.childHasKeyboardFocus(gChatBarp))
	{
		LLChatBar::stopChat();
	}
	else
	{
		LLChatBar::startChat(NULL);
	}
}

void handle_inventory(void*)
{
	LLFirstUse::useInventory();
	LLFloaterInventory::toggleVisibility(NULL);
}

//---------------------------------------------------------------------------

class LLAvatarEnableDebug final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		bool new_value = false;
		if (gMenuHolderp->getChild<LLView>("Debug", true, false))
		{
			// Allow true admins to debug avatars (when admin overrides is on),
			// but also to refresh avatars (when admin overrides is off)
			std::string label =
				gAgent.isGodlikeWithoutAdminMenuFakery() &&
				gAgent.getAdminOverride() ? "Debug" : "Refresh";
			gMenuHolderp->childSetText("Debug", label);
			new_value = true;
		}

		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

void reload_avatar_texture(LLVOAvatar* avatar, ETextureIndex idx)
{
	LLTextureEntry* tep = avatar->getTE(idx);
	if (!tep) return;	// Paranoia

	LLViewerFetchedTexture* tex =
		LLViewerTextureManager::getFetchedTexture(tep->getID());
	if (tex)
	{
		// Force a reload of the raw image
		tex->forceRefetch();
	}
}

// static
void handle_refresh_avatar(LLVOAvatar* avatar, bool refresh_all)
{
	if (refresh_all)
	{
		avatar->resetSkeleton();
	}

	// Force-reload the avatar's known baked textures
	reload_avatar_texture(avatar, TEX_HAIR_BAKED);
	reload_avatar_texture(avatar, TEX_EYES_BAKED);
	reload_avatar_texture(avatar, TEX_HEAD_BAKED);
	reload_avatar_texture(avatar, TEX_UPPER_BAKED);
	reload_avatar_texture(avatar, TEX_LOWER_BAKED);
	reload_avatar_texture(avatar, TEX_SKIRT_BAKED);
	// Request again the baked textures in case we would have missed a refresh
	// (new baked texture UUID missed due to a lost packet, for example).
	avatar->sendAvatarTexturesRequest(true);

	if (!refresh_all) return;

	avatar->updateVisualComplexity();

	// Set all mesh attachments LOD to a different LOD than the current one
	// (this will only stay at this LOD till the next LLVOVolume::updateLOD()
	// call for each mesh), so to force the mesh refresh.
	for (S32 i = 0, count = avatar->mAttachedObjectsVector.size();
		 i < count; ++i)
	{
		LLViewerObject* objectp = avatar->mAttachedObjectsVector[i].first;
		if (!objectp) continue;	// Paranoia

		if (objectp->getPCode() == LL_PCODE_VOLUME)
		{
			LLVOVolume* volp = objectp->asVolume();
			if (volp)
			{
				if (volp->getLOD() == LLModel::LOD_HIGH)
				{
					volp->tempSetLOD(LLModel::LOD_MEDIUM);
				}
				else
				{
					volp->tempSetLOD(LLModel::LOD_HIGH);
				}
			}
		}
		// Process all children
		LLViewerObject::const_child_list_t& children = objectp->getChildren();
		for (LLViewerObject::const_child_list_t::const_iterator
				it = children.begin(), end = children.end();
			 it != end; ++it)
		{
			LLViewerObject* childp = *it;
			if (childp && childp->getPCode() == LL_PCODE_VOLUME)
			{
				LLVOVolume* volp = childp->asVolume();
				if (volp)
				{
					if (volp->getLOD() == LLModel::LOD_HIGH)
					{
						volp->tempSetLOD(LLModel::LOD_MEDIUM);
					}
					else
					{
						volp->tempSetLOD(LLModel::LOD_HIGH);
					}
				}
			}
		}
	}

	// Also restart the avatar's animations
	for (LLVOAvatar::anim_it_t it = avatar->mPlayingAnimations.begin(),
							   end = avatar->mPlayingAnimations.end();
		 it != end; ++it)
	{
		const LLUUID& anim_id = it->first;
		avatar->stopMotion(anim_id, true);
		avatar->startMotion(anim_id);
	}
}

class LLAvatarDebug final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		LLVOAvatar* avatar =
			find_avatar_from_object(gSelectMgr.getSelection()->getPrimaryObject());
		if (avatar)
		{
			// Allow true admins to debug avatars (when admin overrides is on),
			// but also to refresh avatars (when admin overrides is off)
			if (gAgent.isGodlikeWithoutAdminMenuFakery() &&
				gAgent.getAdminOverride())
			{
				((LLVOAvatarSelf*)avatar)->dumpLocalTextures();
				llinfos << "Dumping temporary asset data to simulator logs for avatar "
						<< avatar->getID() << llendl;
				std::vector<std::string> strings;
				strings.emplace_back(avatar->getID().asString());
				LLUUID invoice;
				send_generic_message("dumptempassetdata", strings, invoice);
				LLFloaterAvatarTextures::show(avatar->getID());
			}
			else
			{
				handle_refresh_avatar(avatar, true);
			}
		}
		return true;
	}
};

//---------------------------------------------------------------------------
// Parcel freeze, eject, etc.
//---------------------------------------------------------------------------

bool callback_freeze(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	if (option == 0 || option == 1)
	{
		LLUUID avatar_id = notification["payload"]["avatar_id"].asUUID();
		LLAvatarActions::sendFreeze(avatar_id, option == 0);
	}
	return false;
}

class LLAvatarFreeze final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		LLVOAvatar* avatarp =
			find_avatar_from_object(gSelectMgr.getSelection()->getPrimaryObject());
		if (!avatarp)
		{
			return true;
		}

		LLSD payload;
		payload["avatar_id"] = avatarp->getID();

		std::string fullname = avatarp->getFullname();
//MK
		if (gRLenabled &&
			(gRLInterface.mContainsShownames ||
			 gRLInterface.mContainsShownametags))
		{
			fullname = gRLInterface.getDummyName(fullname);
		}
//mk
		if (fullname.empty())
		{
			fullname = LLTrans::getString("this_resident");
		}
		LLSD args;
		args["AVATAR_NAME"] = fullname;

		gNotifications.add("FreezeAvatarFullname", args, payload,
						   callback_freeze);

		return true;
	}
};

bool callback_eject(const LLSD& notification, const LLSD& response)
{
	S32 option = LLNotification::getSelectedOption(notification, response);
	if (option == 2)
	{
		// Cancel button.
		return false;
	}

	LLUUID avatar_id = notification["payload"]["avatar_id"].asUUID();
	if (option == 0)
	{
		// Eject button
		LLAvatarActions::sendEject(avatar_id, false);
	}
	else if (notification["payload"]["ban_enabled"].asBoolean())
	{
		// This is tricky. It is similar to say if it is not an 'Eject' button,
		// and it is also not an 'Cancel' button, and ban_enabled is true, it
		// should be the 'Eject and Ban' button.
		LLAvatarActions::sendEject(avatar_id, true);
	}

	return false;
}

class LLAvatarEject final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		LLVOAvatar* avatarp =
			find_avatar_from_object(gSelectMgr.getSelection()->getPrimaryObject());
		if (!avatarp)
		{
			return true;
		}

		LLSD payload;
		payload["avatar_id"] = avatarp->getID();

		std::string fullname = avatarp->getFullname();
//MK
		if (gRLenabled &&
			(gRLInterface.mContainsShownames ||
			 gRLInterface.mContainsShownametags))
		{
			fullname = gRLInterface.getDummyName(fullname);
		}
//mk
		if (fullname.empty())
		{
			fullname = LLTrans::getString("this_resident");
		}
		LLSD args;
		args["AVATAR_NAME"] = fullname;
 
		const LLVector3d& pos = avatarp->getPositionGlobal();
		LLParcel* parcelp = gViewerParcelMgr.selectParcelAt(pos)->getParcel();
		if (parcelp &&
			gViewerParcelMgr.isParcelOwnedByAgent(parcelp,
												  GP_LAND_MANAGE_BANNED))
		{
			payload["ban_enabled"] = true;
			gNotifications.add("EjectAvatarFullname", args, payload,
							   callback_eject);
		}
		else
		{
			payload["ban_enabled"] = false;
			gNotifications.add("EjectAvatarFullnameNoBan", args, payload,
							   callback_eject);
		}

		return true;
	}
};

class LLAvatarEnableFreezeEject final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		LLVOAvatar* avatarp =
			find_avatar_from_object(gSelectMgr.getSelection()->getPrimaryObject());
		bool new_value = avatarp &&
						 LLAvatarActions::canEjectOrFreeze(avatarp->getID());
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

//---------------------------------------------------------------------------

class LLAvatarGiveCard final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
//MK
		if (gRLenabled &&
			(gRLInterface.mContainsShownames ||
			 gRLInterface.mContainsShownametags))
		{
			return false;
		}
//mk
		llinfos << "handle_give_card()" << llendl;
		LLViewerObject* dest = gSelectMgr.getSelection()->getPrimaryObject();
		if (dest && dest->isAvatar())
		{
			bool found_name = false;
			LLSD args;
			LLNameValue* nvfirst = dest->getNVPair("FirstName");
			LLNameValue* nvlast = dest->getNVPair("LastName");
			if (nvfirst && nvlast)
			{
				args["NAME"] = LLCacheName::buildFullName(nvfirst->getString(),
														  nvlast->getString());
				found_name = true;
			}
			LLViewerRegion* region = dest->getRegion();
			LLHost dest_host;
			if (region)
			{
				dest_host = region->getHost();
			}
			if (found_name && dest_host.isOk())
			{
				LLMessageSystem* msg = gMessageSystemp;
				msg->newMessage(_PREHASH_OfferCallingCard);
				msg->nextBlockFast(_PREHASH_AgentData);
				msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
				msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
				msg->nextBlockFast(_PREHASH_AgentBlock);
				msg->addUUIDFast(_PREHASH_DestID, dest->getID());
				LLUUID transaction_id;
				transaction_id.generate();
				msg->addUUIDFast(_PREHASH_TransactionID, transaction_id);
				msg->sendReliable(dest_host);
				gNotifications.add("OfferedCard", args);
			}
			else
			{
				gNotifications.add("CantOfferCallingCard", args);
			}
		}
		return true;
	}
};

void login_done(S32 which, void *user)
{
	llinfos << "Login done " << which << llendl;

	LLPanelLogin::close();
}

bool enable_buy()
{
	// In order to buy, there must only be 1 purchaseable object in the
	// selection manger.
	if (gSelectMgr.getSelection()->getRootObjectCount() != 1)
	{
		return false;
	}

	LLViewerObject* obj = NULL;
	LLSelectNode* node = gSelectMgr.getSelection()->getFirstRootNode();
	if (node)
	{
		obj = node->getObject();
		if (obj && obj->permAnyOwner() && node->mSaleInfo.isForSale() &&
			(node->mPermissions->getMaskOwner() & PERM_TRANSFER) &&
			((node->mPermissions->getMaskOwner() & PERM_COPY) ||
			 node->mSaleInfo.getSaleType() != LLSaleInfo::FS_COPY))
		{
			return true;
		}
	}
	return false;
}

class LLObjectEnableBuy final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		bool new_value = enable_buy();
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

// Note: This will only work if the data of the selected object has already
// been received by the viewer and cached in the selection manager.
void handle_buy_object(LLSaleInfo sale_info)
{
	if (!gSelectMgr.selectGetAllRootsValid())
	{
		gNotifications.add("UnableToBuyWhileDownloading");
		return;
	}

	LLUUID owner_id;
	std::string owner_name;
	bool owners_identical = gSelectMgr.selectGetOwner(owner_id, owner_name);
	if (!owners_identical)
	{
		gNotifications.add("CannotBuyObjectsFromDifferentOwners");
		return;
	}

	LLPermissions perm;
	LLAggregatePermissions ag_perm;
	bool valid = gSelectMgr.selectGetPermissions(perm) &&
				 gSelectMgr.selectGetAggregatePermissions(ag_perm);
	if (!valid || !sale_info.isForSale() || !perm.allowTransferTo(gAgentID))
	{
		gNotifications.add("ObjectNotForSale");
		return;
	}

	S32 price = sale_info.getSalePrice();
	if (can_afford_transaction(price))
	{
		LLFloaterBuy::show(sale_info);
	}
	else
	{
		LLFloaterBuyCurrency::buyCurrency("This object costs", price);
	}
}

class HBSelfGroupTitles final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		HBFloaterGroupTitles::showInstance();
		return true;
	}
};

bool stand_up()
{
	if (isAgentAvatarValid() && gAgentAvatarp->mIsSitting)
	{

//MK
		if (gRLenabled && gRLInterface.mContainsUnsit)
		{
			// Set it to false because we are currently prevented from standing
			// up and we do not want to force a sit ground once the restriction
			// is lifted later on.
			gRLInterface.mSitGroundOnStandUp = false;
			return false;
		}
//mk
		LL_DEBUGS("AgentSit") << "Sending agent unsit request" << LL_ENDL;
		gAgent.setControlFlags(AGENT_CONTROL_STAND_UP);
//MK
		if (gRLenabled && gRLInterface.mContainsStandtp)
		{
			gRLInterface.backToLastStandingLoc();
		}
//mk
	}

	return true;
}

bool sit_on_ground()
{
//MK
	if (gRLenabled &&
		(gRLInterface.contains("sit") || gRLInterface.mContainsInteract))
	{
		return false;
	}
//mk

	if (isAgentAvatarValid() && !gAgentAvatarp->mIsSitting)
	{
		gAgent.setFlying(false);
		LL_DEBUGS("AgentSit") << "Sending agent sit on ground request"
							  << LL_ENDL;
		gAgent.clearControlFlags(AGENT_CONTROL_STAND_UP);
		gAgent.setControlFlags(AGENT_CONTROL_SIT_ON_GROUND);
//MK
		if (gRLenabled)
		{
			// Store our current location so that we can snap back here
			// when we stand up, if under @standtp
			gRLInterface.storeLastStandingLoc(true);
		}
//mk
		// Might be our first sit
		LLFirstUse::useSit();

		return true;
	}

	return false;
}

bool sit_on_object(LLViewerObject* object, const LLVector3& offset)
{
	if (!object || object->getPCode() != LL_PCODE_VOLUME ||
		!object->getRegion())
	{
		return false;
	}

//MK
	if (gRLenabled)
	{
		if (gRLInterface.contains("sit") || gRLInterface.mContainsInteract)
		{
			return false;
		}
		if (gRLInterface.mSittpMax < EXTREMUM)
		{
			LLVector3 pos = object->getPositionRegion() + offset;
			pos -= gAgent.getPositionAgent();
			if (pos.length () >= gRLInterface.mSittpMax)
			{
				return false;
			}
		}
		// We are now standing, and we want to sit down => store our current
		// location so that we can snap back here when we stand up, if under
		// @standtp
		gRLInterface.storeLastStandingLoc();
	}
//mk

	LL_DEBUGS("AgentSit") << "Sending agent sit on object request" << LL_ENDL;
	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessageFast(_PREHASH_AgentRequestSit);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);
	msg->nextBlockFast(_PREHASH_TargetObject);
	msg->addUUIDFast(_PREHASH_TargetID, object->mID);
	msg->addVector3Fast(_PREHASH_Offset, offset);

	object->getRegion()->sendReliableMessage();

	return true;
}

class LLSelfSitOrStand final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		if (!isAgentAvatarValid()) return true;

		if (gAgentAvatarp->mIsSitting)
		{
			stand_up();
		}
		else
		{
			sit_on_ground();
		}

		return true;
	}
};

class LLSelfEnableSitOrStand final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		bool new_value = isAgentAvatarValid() && !gAgent.getFlying();
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);

		return true;
	}
};

// Enable a menu item when you don't have someone's card.
class LLAvatarEnableAddFriend final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
//MK
		if (gRLenabled &&
			(gRLInterface.mContainsShownames ||
			 gRLInterface.mContainsShownametags))
		{
			return false;
		}
//mk
		LLVOAvatar* avatar =
			find_avatar_from_object(gSelectMgr.getSelection()->getPrimaryObject());
		bool new_value = avatar && !LLAvatarTracker::isAgentFriend(avatar->getID());
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

class LLEditEnableCustomizeAvatar final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		bool new_value = (isAgentAvatarValid() &&
						  gAgentAvatarp->isFullyLoaded() &&
						  gAgentWearables.areWearablesLoaded());
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

class LLEditEnableOutfitPicker final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(gIsInSecondLife);
		return true;
	}
};

class LLEditEnableDisplayName final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		bool new_value = (LLAvatarNameCache::useDisplayNames() != 0);
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

bool sitting_on_selection()
{
	LLSelectNode* node = gSelectMgr.getSelection()->getFirstRootNode();
	if (!node || !node->mValid)
	{
		return false;
	}

	LLViewerObject* root_object = node->getObject();
	if (!root_object)
	{
		return false;
	}

	// Need to determine if avatar is sitting on this object
	if (!isAgentAvatarValid())
	{
		return false;
	}

	return gAgentAvatarp->mIsSitting &&
		   gAgentAvatarp->getRoot() == root_object;
}

// only works on pie menu
bool handle_sit_or_stand()
{
	LLPickInfo pick = gToolPie.getPick();
	LLViewerObject* object = pick.getObject();;
	if (!object || pick.mPickType == LLPickInfo::PICK_FLORA)
	{
		return true;
	}

//MK
	if (gRLenabled && gRLInterface.mContainsUnsit &&
		isAgentAvatarValid() && gAgentAvatarp->mIsSitting)
	{
		return true;
	}
//mk

	if (sitting_on_selection())
	{
		stand_up();
		return true;
	}

	sit_on_object(object, pick.mObjectOffset);
	return true;
}

class LLObjectSitOrStand final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		return handle_sit_or_stand();
	}
};

void near_sit_down_point(bool success, void*)
{
	if (success)
	{
		sit_on_ground();
	}
}

class LLLandSit final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		if (!stand_up())
		{
			return true;
		}

		gViewerParcelMgr.deselectLand();

		LLVector3d pos = gToolPie.getPick().mPosGlobal;

		LLQuaternion target_rot;
		if (isAgentAvatarValid())
		{
			target_rot = gAgentAvatarp->getRotation();
		}
		else
		{
			target_rot = gAgent.getFrameAgent().getQuaternion();
		}
		gAgentPilot.startAutoPilotGlobal(pos, "Sit", &target_rot,
										 near_sit_down_point, NULL, 0.7f,
										 gAgent.getFlying());
		return true;
	}
};

class LLLandCanSit final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		LLVector3d pos = gToolPie.getPick().mPosGlobal;
		// Can sit only if the position is valid (not beyond draw distance)
		return !pos.isExactlyZero();
	}
};

class LLCreateLandmarkCallback final : public LLInventoryCallback
{
public:
	void fire(const LLUUID& inv_item) override
	{
		llinfos << "Created landmark with inventory Id: " << inv_item
				<< llendl;
	}
};

class LLWorldFly final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		gAgent.toggleFlying();
		return true;
	}
};

class LLWorldEnableFly final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		bool sitting = false;
		if (isAgentAvatarValid())
		{
			sitting = gAgentAvatarp->mIsSitting;
		}
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(!sitting);
		return true;
	}
};

//
// Major mode switching
//

// Note: extra parameters allow this function to be called from dialog.
void reset_view_final(bool proceed, void*)
{
	if (proceed)
	{
		gAgent.resetView(true, true);
	}
}

bool handle_reset_view()
{
	if (gFloaterCustomizep &&
		gAgent.getCameraMode() == CAMERA_MODE_CUSTOMIZE_AVATAR)
	{
		// Show dialog box if needed.
		gFloaterCustomizep->askToSaveIfDirty(reset_view_final, NULL);
		return false;
	}
//MK
	// We should not have to do this here, but when we hit SHIFT ESC, we need
	// to prevent exiting mouselook if the max cam distance is zero.
	else if (gRLenabled && gAgent.cameraMouselook() &&
			 gRLInterface.mCamDistMax <= 0.f)
	{
		return false;
	}
//mk

	gAgent.resetView(true, true);
	return true;
}

class LLViewResetView final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		handle_reset_view();
		return true;
	}
};

class LLViewReleaseCamera final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		// WARNING: should this method get modified to do anything else than
		// removing all follow-camera constraints data, it would be necessary
		// to make a new method for calling it from here.
		LLFollowCamMgr::cleanupClass();
		return true;
	}
};

class LLViewEnableReleaseCamera final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		bool followcam = LLFollowCamMgr::getActiveFollowCamParams() != NULL;
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(followcam);
		return true;
	}
};

class LLViewLookAtLastChatter final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		gAgent.lookAtLastChat();
		return true;
	}
};

class LLViewMouselook final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		if (!gAgent.cameraMouselook())
		{
			gAgent.changeCameraToMouselook();
		}
		else
		{
			gAgent.changeCameraToDefault();
		}
		return true;
	}
};

class LLViewDefaultUISize final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		gSavedSettings.setF32("UIScaleFactor", 1.0f);
		gSavedSettings.setBool("UIAutoScale", false);
		gViewerWindowp->reshape(gViewerWindowp->getWindowDisplayWidth(),
								gViewerWindowp->getWindowDisplayHeight());
		return true;
	}
};

class LLEditDuplicate final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
//MK
		if (gRLenabled && gRLInterface.mContainsRez)
		{
			return true;
		}
//mk
		if (gEditMenuHandlerp && gEditMenuHandlerp->canDuplicate())
		{
			gEditMenuHandlerp->duplicate();
		}
		return true;
	}
};

class LLEditEnableDuplicate final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		bool new_value = gEditMenuHandlerp &&
						 gEditMenuHandlerp->canDuplicate();
//MK
		if (gRLenabled && gRLInterface.mContainsRez)
		{
			new_value = false;
		}
//mk
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

// Also called in llfloaterpathfindingobjects.
void handle_take_copy()
{
	if (gSelectMgr.getSelection()->isEmpty()) return;

	const LLUUID category_id =
		gInventory.findCategoryUUIDForType(LLFolderType::FT_OBJECT);
	derez_objects(DRD_ACQUIRE_TO_AGENT_INVENTORY, category_id);
}

class LLToolsTakeCopy final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		handle_take_copy();
		return true;
	}
};

static void return_objects(const LLSD& notification,
						   const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		// Ignore category ID for this derez destination.
		derez_objects(DRD_RETURN_TO_OWNER, LLUUID::null);
	}
}

// Also called in llfloaterpathfindingobjects.
void handle_object_return()
{
		if (gSelectMgr.getSelection()->isEmpty()) return;
//MK
		if (gRLenabled &&
			(gRLInterface.mContainsRez ||
			 (gRLInterface.mContainsUnsit &&
			  gRLInterface.isSittingOnAnySelectedObject())))
		{
			return;
		}
//mk
		gNotifications.add("ReturnToOwner", LLSD(), LLSD(), return_objects);
}

// You can return an object to its owner if it is on your land.
class LLObjectReturn final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		handle_object_return();
		return true;
	}
};

// Also called in llfloaterpathfindingobjects.
bool enable_object_return()
{
//MK
	if (gRLenabled &&
		(gRLInterface.mContainsRez ||
		 (gRLInterface.mContainsUnsit &&
		  gRLInterface.isSittingOnAnySelectedObject())))
	{
		return false;
	}
//mk
	bool can_return = false;
	if (gAgent.isGodlike())
	{
		can_return = true;
	}
	else
	{
		LLViewerRegion* region = gAgent.getRegion();
		if (region)
		{
			// Estate owners and managers can always return objects.
			if (region->canManageEstate())
			{
				can_return = true;
			}
			else
			{
				struct f final : public LLSelectedObjectFunctor
				{
					bool apply(LLViewerObject* obj) override
					{
//MK
						if (gRLenabled && gRLInterface.mContainsUnsit &&
							obj->isAgentSeat())
						{
							return false;
						}
//mk
						return obj->permModify() || obj->isReturnable();
					}
				} func;
				can_return = gSelectMgr.getSelection()->applyToRootObjects(&func, true);
			}
		}
	}
	return can_return;
}

// Allow return to owner if one or more of the selected items is
// over land you own.
class LLObjectEnableReturn final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		bool new_value = enable_object_return();
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

// You can take an item when it is public and transferrable, or when you own
// it. We error out on the side of enabling the item when at least one item
// selected can be copied to inventory.
bool enable_take()
{
	if (sitting_on_selection())
	{
		return false;
	}
//MK
	if (gRLenabled && gRLInterface.mContainsRez)
	{
		return false;
	}
//mk
	for (LLObjectSelection::valid_root_iterator
			iter = gSelectMgr.getSelection()->valid_root_begin(),
			end = gSelectMgr.getSelection()->valid_root_end();
		 iter != end; ++iter)
	{
		LLSelectNode* node = *iter;
		if (!node) continue;	// Paranoia

		LLViewerObject* object = node->getObject();
		if (!object || object->isAvatar())
		{
			// ...don't acquire avatars
			continue;
		}

		if (!object->isPermanentEnforced() &&
			(node->mPermissions->getOwner() == gAgentID ||
			 (object->permModify() &&
			  node->mPermissions->allowTransferTo(gAgentID))))
		{
			return true;
		}
	}

	return false;
}

bool confirm_take(const LLSD& notification, const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) == 0 &&
		enable_take())
	{
		derez_objects(DRD_TAKE_INTO_AGENT_INVENTORY,
					  notification["payload"]["folder_id"].asUUID());
	}
	return false;
}

void handle_take()
{
	// We want to use the folder this was derezzed from if it is available.
	// Otherwise, derez to the normal place.
	if (gSelectMgr.getSelection()->isEmpty())
	{
		return;
	}
//MK
	if (gRLenabled &&
		(gRLInterface.mContainsRez ||
		 (gRLInterface.mContainsUnsit &&
		  gRLInterface.isSittingOnAnySelectedObject())))
	{
		return;
	}
//mk
	bool you_own_everything = true;
	bool locked_but_takeable_object = false;
	bool ambiguous_destination = false;
	LLUUID category_id, new_cat_id;
	const LLUUID& trash = gInventory.getTrashID();
	const LLUUID& library = gInventory.getLibraryRootFolderID();

	for (LLObjectSelection::root_iterator
			iter = gSelectMgr.getSelection()->root_begin(),
			end = gSelectMgr.getSelection()->root_end();
		 iter != end; ++iter)
	{
		LLSelectNode* node = *iter;
		if (!node) continue;	// Paranoia

		LLViewerObject* object = node->getObject();
		if (object)
		{
			if (!object->permYouOwner())
			{
				you_own_everything = false;
			}

			if (!object->permMove())
			{
				locked_but_takeable_object = true;
			}
		}
		new_cat_id = node->mFolderID;
		// Check that the category exists and is not inside the trash
		// neither inside the library...
		if (!ambiguous_destination && new_cat_id.notNull() &&
			gInventory.getCategory(new_cat_id) && new_cat_id != trash &&
			!gInventory.isObjectDescendentOf(new_cat_id, trash) &&
			!gInventory.isObjectDescendentOf(new_cat_id, library))
		{
			if (category_id.isNull())
			{
				category_id = new_cat_id;
			}
			else if (category_id != new_cat_id)
			{
				// We have found two potential destinations.
				ambiguous_destination = true;
			}
		}
	}
	if (ambiguous_destination || category_id.isNull())
	{
		// Use the default "Objects" category.
		category_id = gInventory.findCategoryUUIDForType(LLFolderType::FT_OBJECT);
	}

	LLSD payload;
	payload["folder_id"] = category_id;
	LLNotification::Params params("ConfirmObjectTakeLock");
	params.payload(payload).functor(confirm_take);
	if (locked_but_takeable_object || !you_own_everything)
	{
		if (locked_but_takeable_object && you_own_everything)
		{
			params.name("ConfirmObjectTakeLock");

		}
		else if (!locked_but_takeable_object && !you_own_everything)
		{
			params.name("ConfirmObjectTakeNoOwn");
		}
		else
		{
			params.name("ConfirmObjectTakeLockNoOwn");
		}

		gNotifications.add(params);
	}
	else
	{
		gNotifications.forceResponse(params, 0);
	}
}

// This is a small helper function to determine if we have a buy or a take in
// the selection. This method is to help with the aliasing problems of putting
// buy and take in the same pie menu space. After a fair amont of discussion,
// it was determined to prefer buy over take. The reasoning follows from the
// fact that when users walk up to buy something, they will click on one or
// more items. Thus, if anything is for sale, it becomes a buy operation, and
// the server will group all of the buy items, and copyable/modifiable items
// into one package and give the end user as much as the permissions will
// allow. If the user wanted to take something, they will select fewer and
// fewer items until only 'takeable' items are left. The one exception is if
// you own everything in the selection that is for sale, in this case, you
// cannot buy stuff from yourself, so you can take it.
// Returns true if selection is a 'buy', false if selection is a 'take'
bool is_selection_buy_not_take()
{
	for (LLObjectSelection::root_iterator
			iter = gSelectMgr.getSelection()->root_begin(),
			end = gSelectMgr.getSelection()->root_end();
		 iter != end; ++iter)
	{
		LLSelectNode* node = *iter;
		if (!node) continue;	// Paranoia

		LLViewerObject* obj = node->getObject();
		if (obj && !obj->permYouOwner() && node->mSaleInfo.isForSale())
		{
			// You do not own the object and it is for sale thus, it is a buy
			return true;
		}
	}
	return false;
}

S32 selection_price()
{
	S32 total_price = 0;
	for (LLObjectSelection::root_iterator
			iter = gSelectMgr.getSelection()->root_begin(),
			end = gSelectMgr.getSelection()->root_end();
		 iter != end; ++iter)
	{
		LLSelectNode* node = *iter;
		if (!node) continue;	// Paranoia

		LLViewerObject* obj = node->getObject();
		if (obj && !(obj->permYouOwner()) && (node->mSaleInfo.isForSale()))
		{
			// you do not own the object and it is for sale.
			// Add its price.
			total_price += node->mSaleInfo.getSalePrice();
		}
	}

	return total_price;
}

void handle_buy_contents(LLSaleInfo sale_info)
{
	LLFloaterBuyContents::show(sale_info);
}

// Also called from lltoolpie.cpp
void handle_buy(void*)
{
	if (gSelectMgr.getSelection()->isEmpty()) return;

	LLSaleInfo sale_info;
	if (!gSelectMgr.selectGetSaleInfo(sale_info))
	{
		return;
	}

	if (sale_info.getSaleType() == LLSaleInfo::FS_CONTENTS)
	{
		handle_buy_contents(sale_info);
	}
	else
	{
		handle_buy_object(sale_info);
	}
}

class LLToolsBuyOrTake final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		if (gSelectMgr.getSelection()->isEmpty())
		{
			return true;
		}

		if (is_selection_buy_not_take())
		{
			S32 total_price = selection_price();
			if (can_afford_transaction(total_price))
			{
				handle_buy(NULL);
			}
			else
			{
				LLFloaterBuyCurrency::buyCurrency("Buying this costs",
												  total_price);
			}
		}
		else
		{
			handle_take();
		}
		return true;
	}
};

// Also called in llfloaterpathfindingobjects.cpp
bool visible_take_object()
{
	return !is_selection_buy_not_take() && enable_take();
}

class LLToolsEnableBuyOrTake final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		// Do not run this before the menu are fully initialized because the
		// static LLView pointer to menu_object_take can't be properly
		// initialized while the pie menus are built which happens before the
		// main menu items are defined.
		if (!gMenusInitialized) return true;

		bool is_buy = is_selection_buy_not_take();
		bool new_value = is_buy ? enable_buy() : enable_take();
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);

		// Update label
		LLSD label;
		std::string buy_text;
		std::string take_text;
		std::string param = userdata["data"].asString();
		std::string::size_type offset = param.find(",");
		if (offset != param.npos)
		{
			buy_text = param.substr(0, offset);
			take_text = param.substr(offset + 1);
		}
		if (is_buy)
		{
			label = LLSD(buy_text);
		}
		else
		{
			label = LLSD(take_text);
		}

		// Using static pointers prevents thousands of recursive calls to
		// getChild<T>() each time a menu is pulled down !
		static LLView* pie_object_take =
			gMenuHolderp->getChild<LLView>("Pie Object Take");
		pie_object_take->setValue(label);

		static LLView* menu_object_take =
			gMenuHolderp->getChild<LLView>("Menu Object Take");
		menu_object_take->setValue(label);

		return true;
	}
};

bool callback_show_buy_currency(const LLSD& notification, const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		llinfos << "Loading page " << BUY_CURRENCY_URL << llendl;
		LLWeb::loadURL(BUY_CURRENCY_URL);
	}
	return false;
}

void show_buy_currency(const char* extra)
{
	std::ostringstream mesg;
	if (extra)
	{
		mesg << extra << "\n \n";
	}
	mesg << "Go to " << BUY_CURRENCY_URL
		 << "\nfor information on purchasing currency ?";

	LLSD args;
	if (extra)
	{
		args["EXTRA"] = extra;
	}
	args["URL"] = BUY_CURRENCY_URL;
	gNotifications.add("PromptGoToCurrencyPage", args, LLSD(),
					   callback_show_buy_currency);
}

class LLObjectBuy final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		handle_buy(NULL);
		return true;
	}
};

class LLToolsSaveToObjectInventory final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		LLSelectNode* node = gSelectMgr.getSelection()->getFirstRootNode();
		if (node && node->mValid && node->mFromTaskID.notNull())
		{
			// *TODO: check to see if the fromtaskid object exists.
			derez_objects(DRD_SAVE_INTO_TASK_INVENTORY, node->mFromTaskID);
		}
		return true;
	}
};

// Round the position of all root objects to the grid
class LLToolsSnapObjectXY final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		F64 snap_size = (F64)gSavedSettings.getF32("GridResolution");

		for (LLObjectSelection::root_iterator
				iter = gSelectMgr.getSelection()->root_begin(),
				end = gSelectMgr.getSelection()->root_end();
			 iter != end; ++iter)
		{
			LLSelectNode* node = *iter;
			if (!node) continue;	// Paranoia

			LLViewerObject* obj = node->getObject();
			if (obj->permModify())
			{
				LLVector3d pos_global = obj->getPositionGlobal();
				F64 round_x = fmod(pos_global.mdV[VX], snap_size);
				if (round_x < snap_size * 0.5)
				{
					// closer to round down
					pos_global.mdV[VX] -= round_x;
				}
				else
				{
					// closer to round up
					pos_global.mdV[VX] -= round_x;
					pos_global.mdV[VX] += snap_size;
				}

				F64 round_y = fmod(pos_global.mdV[VY], snap_size);
				if (round_y < snap_size * 0.5)
				{
					pos_global.mdV[VY] -= round_y;
				}
				else
				{
					pos_global.mdV[VY] -= round_y;
					pos_global.mdV[VY] += snap_size;
				}

				obj->setPositionGlobal(pos_global, false);
			}
		}
		gSelectMgr.sendMultipleUpdate(UPD_POSITION);
		return true;
	}
};

// Determine if the option to cycle between linked prims is shown
class LLToolsEnableSelectNextPart final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		bool new_value = (!gSelectMgr.getSelection()->isEmpty() &&
						  gSavedSettings.getBool("EditLinkedParts")) ||
						  gToolMgr.isCurrentTool(&gToolFace);
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

void select_face_or_linked_prim(const std::string& action)
{
	bool cycle_faces = gToolMgr.isCurrentTool(&gToolFace);
	bool cycle_linked = gSavedSettings.getBool("EditLinkedParts");
	if (!cycle_faces && !cycle_linked) return;

	bool fwd = action == "next";
	bool prev = action == "previous";
	bool ifwd = action == "includenext";
	bool iprev = action == "includeprevious";

	LLViewerObject* to_select = NULL;
	bool restart_face_on_part = !cycle_faces;
	S32 new_te = 0;
	if (cycle_faces)
	{
		// Cycle through faces of current selection, if end is reached, switch
		// to next part (if present)
		LLSelectNode* nodep = gSelectMgr.getSelection()->getFirstNode();
		if (!nodep) return;

		to_select = nodep->getObject();
		if (!to_select) return;

		S32 te_count = to_select->getNumTEs();
		S32 selected_te = nodep->getLastOperatedTE();
		if (fwd || ifwd)
		{
			if (selected_te < 0)
			{
				new_te = 0;
			}
			else if (selected_te + 1 < te_count)
			{
				// Select next face
				new_te = selected_te + 1;
			}
			else
			{
				// Restart from first face on next part
				restart_face_on_part = true;
			}
		}
		else if (prev || iprev)
		{
			if (selected_te > te_count)
			{
				new_te = te_count - 1;
			}
			else if (selected_te > 0)
			{
				// Select previous face
				new_te = selected_te - 1;
			}
			else
			{
				// Restart from last face on next part
				restart_face_on_part = true;
			}
		}
	}

	S32 object_count = gSelectMgr.getSelection()->getObjectCount();
	if (cycle_linked && object_count && restart_face_on_part)
	{
		LLViewerObject* selected = gSelectMgr.getSelection()->getFirstObject();
		if (selected && selected->getRootEdit())
		{
			LLViewerObject::child_list_t children =
				selected->getRootEdit()->getChildren();
			// We need root in the list too
			children.push_front(selected->getRootEdit());

			for (LLViewerObject::child_list_t::iterator iter = children.begin();
				 iter != children.end(); ++iter)
			{
				if ((*iter)->isSelected())
				{
					if (object_count > 1 && (fwd || prev))
					{
						// Multiple selection, find first or last selected if
						// not include
						to_select = *iter;
						if (fwd)
						{
							// Stop searching if going forward; repeat to get
							// last hit if backward
							break;
						}
					}
					else if (object_count == 1 || ifwd || iprev)
					{
						// Single selection or include
						if (fwd || ifwd)
						{
							++iter;
							while (iter != children.end() &&
								   ((*iter)->isAvatar() ||
									(ifwd && (*iter)->isSelected())))
							{
								// Skip sitting avatars and selected if include
								++iter;
							}
						}
						else // Backward
						{
							if (iter == children.begin())
							{
								iter = children.end();
							}
							--iter;
							while (iter != children.begin() &&
								   ((*iter)->isAvatar() ||
									(iprev && (*iter)->isSelected())))
							{
								// Skip sitting avatars and selected if include
								--iter;
							}
						}
						if (iter == children.end())
						{
							iter = children.begin();
						}
						to_select = *iter;
						break;
					}
				}
			}
		}
	}

	if (to_select)
	{
		if (gFloaterToolsp && gFocusMgr.childHasKeyboardFocus(gFloaterToolsp))
		{
			// Force edit toolbox to commit any changes
			gFocusMgr.setKeyboardFocus(NULL);
		}
		if (fwd || prev)
		{
			gSelectMgr.deselectAll();
		}
		if (cycle_faces)
		{
			if (restart_face_on_part)
			{
				new_te = fwd || ifwd ? 0 : to_select->getNumTEs() - 1;
			}
			gSelectMgr.addAsIndividual(to_select, new_te, false);
		}
		else
		{
			gSelectMgr.selectObjectOnly(to_select);
		}
	}
}

// Cycle selection through linked children in selected object.
// *FIXME: Order of children list is not always the same as sim's idea of link
// order. Need link position added to sim messages to address this.
class LLToolsSelectNextPartFace final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		select_face_or_linked_prim(userdata.asString());
		return true;
	}
};

class LLToolsEnableLink final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		bool new_value = gSelectMgr.enableLinkObjects();
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

class LLToolsLink final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		gSelectMgr.linkObjects();
		return true;
	}
};

class LLToolsEnableUnlink final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		bool new_value = gSelectMgr.enableUnlinkObjects();
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

class LLToolsUnlink final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		gSelectMgr.unlinkObjects();
		return true;
	}
};

class LLToolsEnablePathfinding final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		bool newvalue =
			LLPathfindingManager::getInstance()->isPathfindingEnabledForCurrentRegion();
//MK
		if (gRLenabled && gRLInterface.mContainsEdit)
		{
			newvalue = false;
		}
//mk
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(newvalue);
		return true;
	}
};

class LLWorldStopAllAnimations final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		gAgent.stopCurrentAnimations();
		return true;
	}
};

class LLWorldReleaseKeys final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
//MK
		if (gRLenabled && gRLInterface.mContainsDetach)
		{
			return false;
		}
//mk
		gAgent.forceReleaseControls();

		return true;
	}
};

class LLWorldEnableReleaseKeys final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(gAgent.anyControlGrabbed());
		return true;
	}
};

#ifdef SEND_HINGES
void handle_hinge(void*)
{
	gSelectMgr.sendHinge(1);
}

void handle_ptop(void*)
{
	gSelectMgr.sendHinge(2);
}

void handle_lptop(void*)
{
	gSelectMgr.sendHinge(3);
}

void handle_wheel(void*)
{
	gSelectMgr.sendHinge(4);
}

void handle_dehinge(void*)
{
	gSelectMgr.sendDehinge();
}

bool enable_dehinge(void*)
{
	LLViewerObject* obj = gSelectMgr.getSelection()->getFirstEditableObject();
	return obj && !obj->isAttachment();
}
#endif

class LLEditEnableCut final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		bool new_value = gEditMenuHandlerp && gEditMenuHandlerp->canCut();
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

class LLEditCut final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		if (gEditMenuHandlerp && gEditMenuHandlerp->canCut())
		{
			gEditMenuHandlerp->cut();
		}
		return true;
	}
};

class LLEditEnableCopy final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		bool new_value = gEditMenuHandlerp && gEditMenuHandlerp->canCopy();
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

class LLEditCopy final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		if (gEditMenuHandlerp && gEditMenuHandlerp->canCopy())
		{
			gEditMenuHandlerp->copy();
		}
		return true;
	}
};

class LLEditEnablePaste final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		bool new_value = gEditMenuHandlerp && gEditMenuHandlerp->canPaste();
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

class LLEditPaste final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		if (gEditMenuHandlerp && gEditMenuHandlerp->canPaste())
		{
			gEditMenuHandlerp->paste();
		}
		return true;
	}
};

class LLEditEnableDelete final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		bool new_value = gEditMenuHandlerp && gEditMenuHandlerp->canDoDelete();
//MK
		if (gRLenabled && gRLInterface.mContainsRez &&
			// the Delete key must not be inhibited for text:
			gEditMenuHandlerp == &gSelectMgr)
		{
			new_value = false;
		}
//mk
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

class LLEditDelete final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		// If a text field can do a deletion, it gets precedence over deleting
		// an object in the world.
		if (gEditMenuHandlerp && gEditMenuHandlerp->canDoDelete())
		{
			gEditMenuHandlerp->doDelete();
		}

		// Close any pie/context menus when done
		gMenuHolderp->hideMenus();

		// When deleting an object we may not actually be done. Keep selection
		// so we know what to delete when confirmation is needed about the
		// delete.
		gPieObjectp->hide(true);
		return true;
	}
};

// Also called in llfloaterpathfindingobjects.
bool enable_object_delete()
{
	bool can_delete = gSelectMgr.canDoDelete();
//MK
	if (gRLenabled && gRLInterface.mContainsRez)
	{
		can_delete = false;
	}
//mk
	return can_delete;
}

class LLObjectEnableDelete final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		bool new_value = enable_object_delete();
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

class LLEditSearch final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		HBFloaterSearch::toggle();
		return true;
	}
};

// Also called in llfloaterpathfindingobjects.
void handle_object_delete()
{
	gSelectMgr.doDelete();

	// and close any pie/context menus when done
	gMenuHolderp->hideMenus();

	// When deleting an object we may not actually be done. Keep selection so
	// we know what to delete when confirmation is needed about the delete
	gPieObjectp->hide(true);
	return;
}

class LLObjectDelete final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		handle_object_delete();
		return true;
	}
};

class LLViewEnableJoystickFlycam final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		bool new_value = gSavedSettings.getBool("JoystickEnabled") &&
						 gSavedSettings.getBool("JoystickFlycamEnabled");
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

class LLViewEnableLastChatter final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		// *TODO: add check that last chatter is in range
		bool new_value = gAgent.cameraThirdPerson() &&
						 gAgent.getLastChatter().notNull();
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

class LLViewEnableNearbyMedia final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		static LLCachedControl<bool> media_on(gSavedSettings,
											  "EnableStreamingMedia");
		static LLCachedControl<bool> music_on(gSavedSettings,
											  "EnableStreamingMusic");
		bool new_value = media_on || music_on;
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

class LLWorldToggleRadar: public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		HBFloaterRadar::toggleInstance();
		return true;
	}
};

//MK
class LLViewEnableBeacons final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		bool new_value = !gRLenabled || !gRLInterface.mContainsEdit;
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};
//mk

class LLEditEnableDeselect final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		bool new_value = gEditMenuHandlerp &&
						 gEditMenuHandlerp->canDeselect();
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

class LLEditDeselect final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		if (gEditMenuHandlerp && gEditMenuHandlerp->canDeselect())
		{
			gEditMenuHandlerp->deselect();
		}
		return true;
	}
};

class LLEditEnableSelectAll final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		bool new_value = gEditMenuHandlerp &&
						 gEditMenuHandlerp->canSelectAll();
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

class LLEditSelectAll final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		if (gEditMenuHandlerp && gEditMenuHandlerp->canSelectAll())
		{
			gEditMenuHandlerp->selectAll();
		}
		return true;
	}
};

class LLEditEnableUndo final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		bool new_value = gEditMenuHandlerp && gEditMenuHandlerp->canUndo();
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

class LLEditUndo final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		if (gEditMenuHandlerp && gEditMenuHandlerp->canUndo())
		{
			gEditMenuHandlerp->undo();
		}
		return true;
	}
};

class LLEditEnableRedo final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		bool new_value = gEditMenuHandlerp && gEditMenuHandlerp->canRedo();
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

class LLEditRedo final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		if (gEditMenuHandlerp && gEditMenuHandlerp->canRedo())
		{
			gEditMenuHandlerp->redo();
		}
		return true;
	}
};

void show_debug_menus()
{
	// This can get called at login screen where there is no menu so only
	// toggle it if one exists
	if (gMenuBarViewp)
	{
		gMenuBarViewp->setItemVisible(CLIENT_MENU_NAME, true);
		gMenuBarViewp->setItemEnabled(CLIENT_MENU_NAME, true);

		// Server ('Admin') menu hidden when not in godmode.
		bool show_server_menu = gAgent.getGodLevel() > GOD_NOT;
		gMenuBarViewp->setItemVisible(SERVER_MENU_NAME, show_server_menu);
		gMenuBarViewp->setItemEnabled(SERVER_MENU_NAME, show_server_menu);

		gMenuBarViewp->arrange(); // clean-up positioning
	}
}

#if 0
LLUUID gExporterRequestID;
std::string gExportDirectory;

LLUploadDialog* gExportDialog = NULL;

void handle_export_selected(void*)
{
	LLObjectSelectionHandle selection = gSelectMgr.getSelection();
	if (selection->isEmpty() || !gAgent.getRegion())
	{
		return;
	}
	llinfos << "Exporting selected objects:" << llendl;

	gExporterRequestID.generate();
	gExportDirectory = "";

	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessageFast(_PREHASH_ObjectExportSelected);
	msg->nextBlockFast(_PREHASH_AgentData);
	msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
	msg->addUUIDFast(_PREHASH_RequestID, gExporterRequestID);
	msg->addS16Fast(_PREHASH_VolumeDetail, 4);

	for (LLObjectSelection::root_iterator
			iter = gSelectMgr.getSelection()->root_begin(),
			end = gSelectMgr.getSelection()->root_end();
		 iter != end; ++iter)
	{
		LLSelectNode* node = *iter;
		if (!node) continue;	// Paranoia

		LLViewerObject* object = node->getObject();
		msg->nextBlockFast(_PREHASH_ObjectData);
		msg->addUUIDFast(_PREHASH_ObjectID, object->getID());
		llinfos << "Object: " << object->getID() << llendl;
	}
	msg->sendReliable(gAgent.getRegionHost());

	gExportDialog =
		LLUploadDialog::modalUploadDialog("Exporting selected objects...");
}
#endif

class LLWorldSetHomeLocation final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		// We just send the message and let the server check for failure cases
		// server will echo back a "Home position set." alert if it succeeds
		// and the home location screencapture happens when that alert is
		// received
		gAgent.setStartPosition(START_LOCATION_ID_HOME);
		return true;
	}
};

class LLWorldTeleportHome final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		gAgent.teleportHome();
		return true;
	}
};

class LLWorldTPtoGround final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		if (isAgentAvatarValid() && gAgent.getRegion())
		{
			LLVector3 pos = gAgent.getPositionAgent();
			pos.mV[VZ] = gWorld.resolveLandHeightAgent(pos);
			LLVector3d pos_global =
				from_region_handle(gAgent.getRegionHandle());
			pos_global += LLVector3d((F64)pos.mV[VX], (F64)pos.mV[VY],
									 (F64)pos.mV[VZ]);
			gAgent.teleportViaLocation(pos_global);
		}
		return true;
	}
};

class LLWorldAlwaysRun final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		// as well as altering the default walk-vs-run state,
		// we also change the *current* walk-vs-run state.
		if (gAgent.getAlwaysRun())
		{
			gAgent.clearAlwaysRun();
			gAgent.clearRunning();
		}
//MK
		else if (!gRLenabled || !gRLInterface.mContainsAlwaysRun)
//mk
////	else
		{
			gAgent.setAlwaysRun();
			gAgent.setRunning();
		}

		// tell the simulator.
		gAgent.sendWalkRun(gAgent.getAlwaysRun());

		return true;
	}
};

class LLWorldCheckAlwaysRun final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		bool new_value = gAgent.getAlwaysRun();
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

class LLWorldSitOnGround final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		sit_on_ground();
		return true;
	}
};

class LLWorldEnableSitOnGround final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		bool new_value = isAgentAvatarValid() && !gAgentAvatarp->mIsSitting;
//MK
		if (gRLenabled &&
			(gRLInterface.contains("sit") || gRLInterface.mContainsInteract))
		{
			new_value = false;
		}
//mk
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

class LLWorldSetAway final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		if (gAgent.getAFK())
		{
			gAgent.clearAFK();
		}
		else
		{
			gAgent.setAFK();
		}
		return true;
	}
};

class LLWorldSetBusy final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		if (gAgent.getBusy())
		{
			gAgent.clearBusy();
		}
		else
		{
			gAgent.setBusy();
			gNotifications.add("BusyModeSet");
		}
		return true;
	}
};

class LLWorldSetAutoReply final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		if (gAgent.getAutoReply())
		{
			gAgent.clearAutoReply();
		}
		else
		{
			gAgent.setAutoReply();
		}
		return true;
	}
};

class LLWorldCreateLandmark final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
//MK
		if (gRLenabled && gRLInterface.mContainsShowloc)
		{
			return true;
		}
//mk
		LLViewerRegion* agent_region = gAgent.getRegion();
		if (!agent_region)
		{
			llwarns << "No agent region" << llendl;
			return true;
		}
		LLParcel* agent_parcel = gViewerParcelMgr.getAgentParcel();
		if (!agent_parcel)
		{
			llwarns << "No agent parcel" << llendl;
			return true;
		}
		if (!agent_parcel->getAllowLandmark() &&
			!LLViewerParcelMgr::isParcelOwnedByAgent(agent_parcel,
													 GP_LAND_ALLOW_LANDMARK))
		{
			gNotifications.add("CannotCreateLandmarkNotOwner");
			return true;
		}

		LLUUID folder_id;
		folder_id = gInventory.findCategoryUUIDForType(LLFolderType::FT_LANDMARK);
		std::string pos_string;
		gAgent.buildLocationString(pos_string);

		create_inventory_item(folder_id, LLTransactionID::tnull,
							  pos_string, pos_string, // name, desc
							  LLAssetType::AT_LANDMARK,
							  LLInventoryType::IT_LANDMARK,
							  NO_INV_SUBTYPE, PERM_ALL,
							  new LLCreateLandmarkCallback);
		return true;
	}
};

class LLToolsLookAtSelection final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		constexpr F32 PADDING_FACTOR = 2.f;
		bool zoom = userdata.asString() == "zoom";
		if (!gSelectMgr.getSelection()->isEmpty())
		{
			gAgent.setFocusOnAvatar(false);

			LLBBox selection_bbox = gSelectMgr.getBBoxOfSelection();
			F32 cam_wiew = gViewerCamera.getView();
			F32 cam_aspect = gViewerCamera.getAspect();
			F32 angle_of_view = llmax(0.1f,
									  cam_aspect > 1.f ? cam_wiew * cam_aspect
													   : cam_wiew);
			F32 distance = selection_bbox.getExtentLocal().length() *
						   PADDING_FACTOR / atanf(angle_of_view);

			LLVector3 obj_to_cam = gViewerCamera.getOrigin() -
								   selection_bbox.getCenterAgent();
			obj_to_cam.normalize();

			LLUUID object_id;
			if (gSelectMgr.getSelection()->getPrimaryObject())
			{
				object_id = gSelectMgr.getSelection()->getPrimaryObject()->mID;
			}
			if (zoom)
			{
				gAgent.setCameraPosAndFocusGlobal(gSelectMgr.getSelectionCenterGlobal() +
												  LLVector3d(obj_to_cam * distance),
												  gSelectMgr.getSelectionCenterGlobal(),
												  object_id);
			}
			else
			{
				gAgent.setFocusGlobal(gSelectMgr.getSelectionCenterGlobal(),
									  object_id);
			}
		}
		return true;
	}
};

void callback_invite_to_group(LLUUID group_id, void *user_data)
{
	std::vector<LLUUID> agent_ids;
	agent_ids.emplace_back(*(LLUUID *)user_data);

	LLFloaterGroupInvite::showForGroup(group_id, &agent_ids);
}

void invite_to_group(const LLUUID& dest_id)
{
	LLVOAvatar* dest = gObjectList.findAvatar(dest_id);
	if (dest)
	{
		LLFloaterGroupPicker* widget;
		widget = LLFloaterGroupPicker::show(callback_invite_to_group,
											(void*)&dest_id);
		if (widget)
		{
			widget->center();
			widget->setPowersMask(GP_MEMBER_INVITE);
		}
	}
}

class LLAvatarInviteToGroup final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
//MK
		if (gRLenabled &&
			(gRLInterface.mContainsShownames ||
			 gRLInterface.mContainsShownametags))
		{
			return false;
		}
//mk
		LLVOAvatar* avatar =
			find_avatar_from_object(gSelectMgr.getSelection()->getPrimaryObject());
		if (avatar)
		{
			invite_to_group(avatar->getID());
		}
		return true;
	}
};

class LLAvatarRender final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		std::string data = userdata.asString();

		LLViewerObject* object = gSelectMgr.getSelection()->getPrimaryObject();

		LLVOAvatar* avatar = find_avatar_from_object(object);
		if (avatar)
		{
			if (data == "normal")
			{
				avatar->setVisualMuteSettings(LLVOAvatar::AV_RENDER_NORMALLY);
			}
			else if (data == "never")
			{
				avatar->setVisualMuteSettings(LLVOAvatar::AV_DO_NOT_RENDER);
			}
			else if (data == "always")
			{
				avatar->setVisualMuteSettings(LLVOAvatar::AV_ALWAYS_RENDER);
			}
		}
#if 0	// Avatar puppets "jelly-dollifying" does not work anyway...
		LLVOAvatarPuppet* puppet = object->getPuppetAvatar();
		if (puppet)
		{
			if (data == "normal")
			{
				puppet->setVisualMuteSettings(LLVOAvatar::AV_RENDER_NORMALLY);
			}
			else if (data == "never")
			{
				puppet->setVisualMuteSettings(LLVOAvatar::AV_DO_NOT_RENDER);
			}
			else if (data == "always")
			{
				puppet->setVisualMuteSettings(LLVOAvatar::AV_ALWAYS_RENDER);
			}		
		}
#endif
		return true;
	}
};

class LLAvatarAddFriend final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
//MK
		if (gRLenabled &&
			(gRLInterface.mContainsShownames ||
			 gRLInterface.mContainsShownametags))
		{
			return false;
		}
//mk
		LLVOAvatar* avatar =
			find_avatar_from_object(gSelectMgr.getSelection()->getPrimaryObject());
		if (avatar && !LLAvatarTracker::isAgentFriend(avatar->getID()))
		{
			LLAvatarActions::requestFriendshipDialog(avatar->getID());
		}
		return true;
	}
};

bool complete_give_money(const LLSD& notification, const LLSD& response,
						 LLObjectSelectionHandle handle)
{
	if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		gAgent.clearBusy();
	}

	LLViewerObject* objectp = handle->getPrimaryObject();

	// Show avatar's name if paying attachment
	if (objectp && objectp->isAttachment())
	{
		while (objectp && !objectp->isAvatar())
		{
			objectp = (LLViewerObject*)objectp->getParent();
		}
	}

	if (objectp)
	{
		if (objectp->isAvatar())
		{
//MK
			if (gRLenabled &&
				(gRLInterface.mContainsShownames ||
				 gRLInterface.mContainsShownametags))
			{
				return false;
			}
//mk
			LLFloaterPay::payDirectly(&give_money, objectp->getID(), false);
		}
		else
		{
			LLFloaterPay::payViaObject(&give_money, objectp->getID());
		}
	}

	return false;
}

bool handle_give_money_dialog()
{
	LLNotification::Params params("BusyModePay");
	params.functor(boost::bind(complete_give_money, _1, _2,
							   gSelectMgr.getSelection()));
	if (gAgent.getBusy())
	{
		// warn users of being in busy mode during a transaction
		gNotifications.add(params);
	}
	else
	{
		gNotifications.forceResponse(params, 1);
	}

	return true;
}

class LLPayObject final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		return handle_give_money_dialog();
	}
};

class LLEnablePayObject final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		LLVOAvatar* avatar =
			find_avatar_from_object(gSelectMgr.getSelection()->getPrimaryObject());
		bool new_value = (avatar != NULL);
		if (!new_value)
		{
			LLViewerObject* object =
				gSelectMgr.getSelection()->getPrimaryObject();
			if (object)
			{
				LLViewerObject* parent = (LLViewerObject*)object->getParent();
				if (object->flagTakesMoney() ||
					(parent && parent->flagTakesMoney()))
				{
					new_value = true;
				}
			}
		}
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

class LLObjectEnableSitOrStand final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		// Do not run this before the menu are fully initialized because the
		// static LLView pointer to object_sit can't be properly initialized
		// while the pie menus are built which happens before the main menu
		// items are defined.
		if (!gMenusInitialized) return true;

		bool new_value = false;

		LLViewerObject* dest_object =
			gSelectMgr.getSelection()->getPrimaryObject();
		if (dest_object)
		{
			if (dest_object->getPCode() == LL_PCODE_VOLUME)
			{
				new_value = true;
			}
//MK
			if (gRLenabled)
			{
				if (gRLInterface.contains("sit") ||
					gRLInterface.mContainsInteract)
				{
					new_value = false;
				}
				if (gRLInterface.mSittpMax < EXTREMUM)
				{
					LLPickInfo pick = gToolPie.getPick();
					LLVector3 pos = dest_object->getPositionRegion() + pick.mObjectOffset;
					pos -= gAgent.getPositionAgent();
					if (pos.length() >= gRLInterface.mSittpMax)
					{
						new_value = false;
					}
				}
			}
//mk
		}
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);

		// Update label
		LLSD label;
		std::string sit_text;
		std::string stand_text;
		std::string param = userdata["data"].asString();
		std::string::size_type offset = param.find(",");
		if (offset != param.npos)
		{
			sit_text = param.substr(0, offset);
			stand_text = param.substr(offset + 1);
		}
		if (sitting_on_selection())
		{
			label = LLSD(stand_text);
		}
		else
		{
			LLSelectNode* node = gSelectMgr.getSelection()->getFirstRootNode();
			if (node && node->mValid && !node->mSitName.empty())
			{
				label = LLSD(node->mSitName);
			}
			else
			{
				label = LLSD(sit_text);
			}
		}
		// Using a static pointer prevents thousands of recursive calls to
		// getChild<T>() each time a menu is pulled down !
		static LLView* object_sit =
			gMenuHolderp->getChild<LLView>("Object Sit");
		object_sit->setValue(label);

		return true;
	}
};

class LLShowFloater final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		std::string floater_name = userdata.asString();
		if (floater_name == "gestures")
		{
			LLFloaterGesture::toggleInstance();
		}
		else if (floater_name == "appearance")
		{
			if (gAgentWearables.areWearablesLoaded())
			{
				gAgent.changeCameraToCustomizeAvatar();
			}
		}
		else if (floater_name == "outfitpicker")
		{
			if (gIsInSecondLife && gAgentWearables.areWearablesLoaded())
			{
				std::string url = gSavedSettings.getString("OutfitPickerURL");
				if (!url.empty())
				{
					std::string grid = gIsInSecondLifeProductionGrid ? "agni"
																	 : "aditi";
					LLStringUtil::replaceString(url, "[GRID_LOWERCASE]", grid);
					LLFloaterMediaBrowser::showInstance(url, true);
				}
			}
		}
		else if (floater_name == "friends")
		{
			LLFloaterFriends::toggleInstance();
		}
		else if (floater_name == "groups")
		{
			LLFloaterGroups::toggleInstance();
		}
		else if (floater_name == "preferences")
		{
			LLFloaterPreference::showInstance();
		}
		else if (floater_name == "toolbar")
		{
			LLToolBar::toggle();
		}
		else if (floater_name == "displayname")
		{
			LLFloaterDisplayName::showInstance();
		}
		else if (floater_name == "chat history")
		{
			LLFloaterChat::toggleInstance();
		}
		else if (floater_name == "teleport history")
		{
			gFloaterTeleportHistoryp->toggle();
		}
		else if (floater_name == "im")
		{
			LLIMMgr::toggle(NULL);
		}
		else if (floater_name == "inventory")
		{
			LLFloaterInventory::toggleVisibility();
		}
		else if (floater_name == "mute list")
		{
			LLFloaterMute::toggleInstance();
		}
		else if (floater_name == "media filter")
		{
			SLFloaterMediaFilter::toggleInstance();
		}
		else if (floater_name == "nearby media")
		{
			LLFloaterNearByMedia::toggleInstance();
		}
		else if (floater_name == "camera controls")
		{
			LLFloaterCamera::toggleInstance();
		}
		else if (floater_name == "movement controls")
		{
			LLFloaterMove::toggleInstance();
		}
		else if (floater_name == "world map")
		{
			LLFloaterWorldMap::toggle(NULL);
		}
		else if (floater_name == "mini map")
		{
			LLFloaterMiniMap::toggleInstance();
		}
		else if (floater_name == "stat bar")
		{
			LLFloaterStats::toggleInstance();
		}
		else if (floater_name == "my land")
		{
			LLFloaterLandHoldings::showInstance();
		}
		else if (floater_name == "about land")
		{
			if (gViewerParcelMgr.selectionEmpty())
			{
				gViewerParcelMgr.selectParcelAt(gAgent.getPositionGlobal());
			}
//MK
			if (!gRLenabled || !gRLInterface.mContainsShowloc)
			{
//mk
				LLFloaterLand::showInstance();
//MK
			}
//mk
		}
		else if (floater_name == "buy land")
		{
			if (gViewerParcelMgr.selectionEmpty())
			{
				gViewerParcelMgr.selectParcelAt(gAgent.getPositionGlobal());
			}
//MK
			if (!gRLenabled || !gRLInterface.mContainsShowloc)
			{
//mk
				gViewerParcelMgr.startBuyLand();
//MK
			}
//mk
		}
		else if (floater_name == "about region")
		{
//MK
			if (!gRLenabled || !gRLInterface.mContainsShowloc)
			{
//mk
				LLFloaterRegionInfo::showInstance();
//MK
			}
//mk
		}
		else if (floater_name == "experiences")
		{
			LLFloaterExperiences::showInstance();
		}
		else if (floater_name == "areasearch")
		{
			HBFloaterAreaSearch::toggleInstance();
		}
		else if (floater_name == "soundslist")
		{
			HBFloaterSoundsList::toggleInstance();
		}
		else if (floater_name == "grid options")
		{
			LLFloaterBuildOptions::showInstance();
		}
		else if (floater_name == "characters")
		{
			LLFloaterPathfindingCharacters::openCharactersWithSelectedObjects();
		}
		else if (floater_name == "linksets")
		{
			LLFloaterPathfindingLinksets::openLinksetsWithSelectedObjects();
		}
		else if (floater_name == "script errors")
		{
			LLFloaterScriptDebug::show(LLUUID::null);
		}
		else if (floater_name == "help f1")
		{
			llinfos << "Spawning HTML help window" << llendl;
			gViewerHtmlHelp.show();
		}
		else if (floater_name == "complaint reporter")
		{
			// Prevent menu from appearing in screen shot.
			gMenuHolderp->hideMenus();
			LLFloaterReporter::showFromMenu();
		}
		else if (floater_name == "mean events")
		{
			HBFloaterBump::showInstance();
		}
		else if (floater_name == "lag meter")
		{
			LLFloaterLagMeter::toggleInstance();
		}
		else if (floater_name == "buy currency")
		{
			LLFloaterBuyCurrency::buyCurrency();
		}
		else if (floater_name == "about")
		{
			LLFloaterAbout::showInstance();
		}
		else if (floater_name == "active speakers")
		{
			LLFloaterActiveSpeakers::toggleInstance();
		}
		else if (floater_name == "beacons")
		{
			LLFloaterBeacons::toggleInstance();
		}
		else if (floater_name == "perm prefs")
		{
			LLFloaterPerms::toggleInstance();
		}
		else if (floater_name == "debug settings")
		{
			LLFloaterDebugSettings::showInstance();
		}
		else if (floater_name == "debug tags")
		{
			HBFloaterDebugTags::showInstance();
		}
		return true;
	}
};

class LLFloaterVisible final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		std::string control_name = userdata["control"].asString();
		std::string floater_name = userdata["data"].asString();
		bool new_value = false;
		if (floater_name == "friends")
		{
			new_value = LLFloaterFriends::instanceVisible();
		}
		else if (floater_name == "groups")
		{
			new_value = LLFloaterGroups::instanceVisible();
		}
		else if (floater_name == "communicate")
		{
			new_value = LLFloaterChatterBox::instanceVisible();
		}
		else if (floater_name == "toolbar")
		{
			new_value = LLToolBar::isVisible();
		}
		else if (floater_name == "chat history")
		{
			new_value = LLFloaterChat::instanceVisible();
		}
		else if (floater_name == "teleport history")
		{
			new_value = gFloaterTeleportHistoryp->getVisible();
		}
		else if (floater_name == "im")
		{
			new_value = LLFloaterChatterBox::instanceVisible(0);
		}
		else if (floater_name == "mute list")
		{
			new_value = LLFloaterMute::instanceVisible();
		}
		else if (floater_name == "media filter")
		{
			new_value = SLFloaterMediaFilter::instanceVisible();
		}
		else if (floater_name == "nearby media")
		{
			new_value = LLFloaterNearByMedia::instanceVisible();
		}
		else if (floater_name == "camera controls")
		{
			new_value = LLFloaterCamera::instanceVisible();
		}
		else if (floater_name == "movement controls")
		{
			new_value = LLFloaterMove::instanceVisible();
		}
		else if (floater_name == "stat bar")
		{
			new_value = LLFloaterStats::instanceVisible();
		}
		else if (floater_name == "lag meter")
		{
			new_value = LLFloaterLagMeter::instanceVisible();
		}
		else if (floater_name == "active speakers")
		{
			new_value = LLFloaterActiveSpeakers::instanceVisible();
		}
		else if (floater_name == "beacons")
		{
			new_value = LLFloaterBeacons::instanceVisible();
		}
		else if (floater_name == "inventory")
		{
			LLFloaterInventory* inv = LLFloaterInventory::getActiveFloater();
			new_value = inv && inv->getVisible();
		}
		else if (floater_name == "radar")
		{
			new_value = HBFloaterRadar::instanceVisible();
		}
		else if (floater_name == "areasearch")
		{
			new_value = HBFloaterAreaSearch::instanceVisible();
		}
		else if (floater_name == "soundslist")
		{
			new_value = HBFloaterSoundsList::instanceVisible();
		}
		gMenuHolderp->findControl(control_name)->setValue(new_value);
		return true;
	}
};

bool callback_show_url(const LLSD& notification, const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		LLWeb::loadURL(notification["payload"]["url"].asString());
	}
	return false;
}

class LLPromptShowURL final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		std::string param = userdata.asString();
		std::string::size_type offset = param.find(",");
		if (offset == std::string::npos)
		{
			llwarns << "PromptShowURL invalid parameters !  Expecting \"ALERT,URL\"."
					<< llendl;
			return true;
		}

		std::string alert = param.substr(0, offset);
		std::string url = param.substr(offset + 1);
		LLSD payload;
		payload["url"] = url;
		gNotifications.add(alert, LLSD(), payload, callback_show_url);
		return true;
	}
};

class LLPromptShowOneOfURLs final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		std::string params = userdata.asString();
		typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
		boost::char_separator<char> sep(",");
		tokenizer tokens(params, sep);
		if (std::distance(tokens.begin(), tokens.end()) != 4)
		{
			llwarns << "PromptShowOneOfURLs invalid parameters !  Expecting \"ALERT,BooleanDebugSettingName,URLWhenSettingTrue,URLWhenSettingFalse\"."
					<< llendl;
			return true;
		}

		tokenizer::iterator it = tokens.begin();
		++it;
		// Under Windoze, the internal representation of the boost token string
		// is not a standard string, but a wide characters string, and there is
		// an implicit conversion done on iterator pointer affectation to a
		// std::string; so for Windows we cannot prevent a string copy and use
		// 'const std::string& setting = *it++;' here, like I first implemented
		// it for Linux, and I am not too sure for macOS either... It is not a
		// time-critical operation anyway, so let's always copy the string. HB
		std::string setting = *it++;
		// Plugins support has been entirely gutted out from CEF 100, and the
		// PDF viewer is now part of the browser (it is not considered a plugin
		// any more). *TODO: remove entirely LLPromptShowOneOfURLs once all
		// viewer builds (i.e. macOS and Windows ones) will use CEF 100 or
		// newer. HB
#if CHROME_VERSION_MAJOR >= 100
		if (setting != "BrowserPluginsEnabled")
#endif
		{
			LLControlVariable* ctrlp =
				gSavedSettings.getControl(setting.c_str());
			if (!ctrlp)
			{
				llwarns << "Could not find any setting named: " << setting
						<< llendl;
				return true;
			}
			if (!ctrlp->getValue().asBoolean())
			{
				++it;
			}
		}
		LLSD payload;
		payload["url"] = *it;
		gNotifications.add(*tokens.begin(), LLSD(), payload,
						   callback_show_url);
		return true;
	}
};

bool callback_show_url_internal(const LLSD& notification, const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		LLWeb::loadURLInternal(notification["payload"]["url"].asString());
	}
	return false;
}

class LLPromptShowURLInternal final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		std::string param = userdata.asString();
		std::string::size_type offset = param.find(",");
		if (offset == std::string::npos)
		{
			llwarns << "PromptShowURLInternal invalid parameters !  Expecting \"ALERT,URL\"."
					<< llendl;
			return true;
		}

		std::string alert = param.substr(0, offset);
		std::string url = param.substr(offset + 1);
		LLSD payload;
		payload["url"] = url;
		gNotifications.add(alert, LLSD(), payload, callback_show_url_internal);
		return true;
	}
};

bool callback_show_file(const LLSD& notification, const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		LLWeb::loadURL(notification["payload"]["url"]);
	}
	return false;
}

class LLShowAgentProfile final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		LLUUID agent_id;
		if (userdata.asString() == "agent")
		{
			agent_id = gAgentID;
		}
		else if (userdata.asString() == "hit object")
		{
//MK
			if (gRLenabled &&
				(gRLInterface.mContainsShownames ||
				 gRLInterface.mContainsShownametags))
			{
				return false;
			}
//mk
			LLViewerObject* objectp =
				gSelectMgr.getSelection()->getPrimaryObject();
			if (objectp)
			{
				agent_id = objectp->getID();
			}
		}
		else
		{
			agent_id = userdata.asUUID();
		}

		LLVOAvatar* avatar = find_avatar_from_object(agent_id);
		if (avatar)
		{
			LLFloaterAvatarInfo::show(avatar->getID());
		}
		return true;
	}
};

class LLLandEdit final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		if (!gFloaterToolsp) return false;
//MK
		if (gRLenabled &&
			(gRLInterface.mContainsRez || gRLInterface.mContainsEdit))
		{
			return false;
		}
//mk
		if (gAgent.getFocusOnAvatar() &&
			gSavedSettings.getBool("EditCameraMovement"))
		{
			// Zoom in if we are looking at the avatar
			gAgent.setFocusOnAvatar(false);
			gAgent.setFocusGlobal(gToolPie.getPick());

			gAgent.cameraOrbitOver(F_PI * 0.25f);
			gViewerWindowp->moveCursorToCenter();
		}
		else if (gSavedSettings.getBool("EditCameraMovement"))
		{
			gAgent.setFocusGlobal(gToolPie.getPick());
			gViewerWindowp->moveCursorToCenter();
		}

		gViewerParcelMgr.selectParcelAt(gToolPie.getPick().mPosGlobal);

		gFloaterViewp->bringToFront(gFloaterToolsp);

		// Switch to land edit toolset
		gToolMgr.getCurrentToolset()->selectTool(&gToolSelectLand);
		return true;
	}
};

class LLWorldEnableBuyLand final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		bool new_value =
			gViewerParcelMgr.canAgentBuyParcel(gViewerParcelMgr.selectionEmpty() ?
												gViewerParcelMgr.getAgentParcel() :
												gViewerParcelMgr.getParcelSelection()->getParcel(),
											   false);
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

class LLWorldEnableAvatarList final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		bool new_value = !gRLenabled ||
						 (!gRLInterface.mContainsShownames &&
						  !gRLInterface.mContainsShownametags);
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

class LLWorldEnableExperiences final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		bool new_value = gAgent.hasRegionCapability("GetExperiences");
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

class LLWorldEnableIfInSL final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(gIsInSecondLife);
		return true;
	}
};

class LLWorldEnableIfNotInSL final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(!gIsInSecondLife);
		return true;
	}
};

class LLWorldEnableWindlightRegionTime final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		// Case of OpenSim (no EE)
		bool ok = !gAgent.hasExtendedEnvironment();
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(ok);
		return true;
	}
};

class LLWorldEnableParcelEnv final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		bool ok = !gWLSkyParamMgr.mHasLightshareOverride &&
				  !LLFloaterWindlight::instanceVisible() &&
				  gAgent.hasExtendedEnvironment();
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(ok);
		return true;
	}
};

class LLWorldEnableLocalEnv final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		static LLCachedControl<bool>local_env(gSavedSettings,
											  "UseLocalEnvironment");
		bool ok = local_env &&
				  !gWLSkyParamMgr.mHasLightshareOverride &&
				  !LLFloaterWindlight::instanceVisible() &&
				  gAgent.hasExtendedEnvironment();
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(ok);
		return true;
	}
};

class LLWorldPbrAdjustHDR final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		static LLCachedControl<bool>auto_hdr(gSavedSettings,
											 "RenderSkyAutoAdjustLegacy");
		bool new_value = gUsePBRShaders && auto_hdr;
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

class LLWorldPbrActive final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(gUsePBRShaders);
		return true;
	}
};

class LLObjectAttachToAvatar final : public view_listener_t
{
public:
	static void setObjectSelection(LLObjectSelectionHandle selection)
	{
		sObjectSelection = selection;
	}

private:
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
//MK
		if (gRLenabled && gRLInterface.mContainsRez)
		{
			return false; // we can't take objects when unable to rez
		}
//mk
		setObjectSelection(gSelectMgr.getSelection());
		LLViewerObject* object = sObjectSelection->getFirstRootObject();
		if (object)
		{
			S32 index = userdata.asInteger();
			LLViewerJointAttachment* attachment_point = NULL;
			if (index > 0)
			{
				attachment_point =
					get_ptr_in_map(gAgentAvatarp->mAttachmentPoints, index);
			}
//MK
			if (gRLenabled)
			{
				if (index == 0 && gRLInterface.mContainsDetach)
				{
					// Something is locked and we are attempting a "Wear"
					// in-world
					setObjectSelection(NULL);
					return false;
				}
				if (attachment_point &&
					!gRLInterface.canAttach(NULL, attachment_point->getName()))
				{
					setObjectSelection(NULL);
					return false;
				}
			}
//mk
			confirm_replace_attachment(0, attachment_point);
		}
		return true;
	}

protected:
	static LLObjectSelectionHandle sObjectSelection;
};

LLObjectSelectionHandle LLObjectAttachToAvatar::sObjectSelection;

void near_attach_object(bool success, void* user_data)
{
	LLViewerJointAttachment* attachment = (LLViewerJointAttachment*)user_data;
#if 0	// We do not care if we got close "enough", just attach the object !
	if (success)
#endif
	{
#if 0	// Stay in whatever state the auto-pilot left us...
		gAgent.setFlying(false);
#endif
		U8 attachment_id = 0;
		if (attachment)
		{
			for (LLVOAvatar::attachment_map_t::iterator
					iter = gAgentAvatarp->mAttachmentPoints.begin();
				 iter != gAgentAvatarp->mAttachmentPoints.end(); ++iter)
			{
				if (iter->second == attachment)
				{
					attachment_id = iter->first;
					break;
				}
			}
		}
		else
		{
			// interpret 0 as "default location"
			attachment_id = 0;
		}
		gSelectMgr.sendAttach(attachment_id);
	}
	LLObjectAttachToAvatar::setObjectSelection(NULL);
}

void confirm_replace_attachment(S32 option, void* user_data)
{
	if (option != 0)	// Not yes
	{
		return;
	}

	LLViewerObject* object = gSelectMgr.getSelection()->getFirstRootObject();
	if (!object)
	{
		llwarns << "Object is gone..." << llendl;
		return;
	}

	// Distances in meters
	constexpr F32 MIN_STOP_DISTANCE = 1.f;
	constexpr F32 ARM_LENGTH = 0.5f;
	constexpr F32 SCALE_FUDGE = 1.5f;

	F32 stop_dist = SCALE_FUDGE * object->getMaxScale() + ARM_LENGTH;
	if (stop_dist < MIN_STOP_DISTANCE)
	{
		stop_dist = MIN_STOP_DISTANCE;
	}

	LLVector3 dest = object->getPositionAgent();
	// Make sure we stop in front of the object
	LLVector3 delta = dest - gAgent.getPositionAgent();
	delta.normalize();
	delta = delta * 0.5f;
	dest -= delta;
	gAgentPilot.startAutoPilotGlobal(gAgent.getPosGlobalFromAgent(dest),
									 "Attach", NULL, near_attach_object,
									 user_data, stop_dist, 0.1f,
									 gAgent.getFlying());
	gAgent.clearFocusObject();
}

class LLAttachmentDrop final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		// Called when the user clicked on an object attached to them
		// and selected "Drop".
		LLViewerObject* object = gSelectMgr.getSelection()->getPrimaryObject();
		if (!object)
		{
			llwarns << "No object to drop" << llendl;
			return true;
		}

		LLViewerObject* parent = (LLViewerObject*)object->getParent();
		while (parent)
		{
			if (parent->isAvatar())
			{
				break;
			}
			object = parent;
			parent = (LLViewerObject*)parent->getParent();
		}

		if (!object || object->isAvatar())
		{
			llwarns << "No object to detach" << llendl;
			return true;
		}

		// The sendDropAttachment() method works on the list of selected
		// objects. Thus we need to clear the list, make sure it only contains
		// the object the user clicked, send the message, then clear the list.
		gSelectMgr.sendDropAttachment();
		return true;
	}
};

// Called from avatar pie menu and Edit menu
void handle_detach_from_avatar(void* user_data)
{
	LLViewerJointAttachment* attachment = (LLViewerJointAttachment*)user_data;
	if (attachment && attachment->getNumObjects() > 0)
	{
//MK
		if (gRLenabled &&
			!gRLInterface.canDetachAllObjectsFromAttachment(attachment))
		{
			return;
		}
//mk
		LLMessageSystem* msg = gMessageSystemp;
		msg->newMessage("ObjectDetach");
		msg->nextBlockFast(_PREHASH_AgentData);
		msg->addUUIDFast(_PREHASH_AgentID, gAgentID);
		msg->addUUIDFast(_PREHASH_SessionID, gAgentSessionID);

		for (LLViewerJointAttachment::attachedobjs_vec_t::const_iterator
				iter = attachment->mAttachedObjects.begin(),
				end = attachment->mAttachedObjects.end();
			 iter != end; ++iter)
		{
			LLViewerObject* object = iter->get();
			if (object)
			{
				msg->nextBlockFast(_PREHASH_ObjectData);
				msg->addU32Fast(_PREHASH_ObjectLocalID, object->getLocalID());
			}
		}

		msg->sendReliable(gAgent.getRegionHost());
	}
}

void attach_label(std::string& label, void* user_data)
{
	LLViewerJointAttachment* attachment = (LLViewerJointAttachment*)user_data;
	if (attachment)
	{
		label = LLTrans::getString(attachment->getName());
		for (LLViewerJointAttachment::attachedobjs_vec_t::const_iterator
				iter = attachment->mAttachedObjects.begin(),
				end = attachment->mAttachedObjects.end();
			 iter != end; ++iter)
		{
			const LLViewerObject* object = iter->get();
			if (object)
			{
				LLViewerInventoryItem* itemp =
					gInventory.getItem(object->getAttachmentItemID());
				if (itemp)
				{
					label += " (" + itemp->getName() + ")";
					break;
				}
			}
		}
	}
}

void detach_label(std::string& label, void* user_data)
{
	LLViewerJointAttachment* attachment = (LLViewerJointAttachment*)user_data;
	if (attachment)
	{
		label = LLTrans::getString(attachment->getName());
		for (LLViewerJointAttachment::attachedobjs_vec_t::const_iterator
				iter = attachment->mAttachedObjects.begin(),
				end = attachment->mAttachedObjects.end();
			 iter != end; ++iter)
		{
			const LLViewerObject* object = iter->get();
			if (object)
			{
				LLViewerInventoryItem* itemp;
				itemp = gInventory.getItem(object->getAttachmentItemID());
				if (itemp)
				{
					label += " (" + itemp->getName() + ")";
					break;
				}
			}
		}
	}
}

class LLAttachmentDetach final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		// Called when the user clicked on an object attached to them
		// and selected "Detach".
		LLViewerObject* object = gSelectMgr.getSelection()->getPrimaryObject();
		if (!object)
		{
			llwarns << "No object to detach" << llendl;
			return true;
		}
//MK
		if (gRLenabled && !gRLInterface.canDetachAllSelectedObjects())
		{
			return true;
		}
//mk
		LLViewerObject* parent = (LLViewerObject*)object->getParent();
		while (parent)
		{
			if (parent->isAvatar())
			{
				break;
			}
			object = parent;
			parent = (LLViewerObject*)parent->getParent();
		}

		if (!object || object->isAvatar())
		{
			llwarns << "No object to detach" << llendl;
			return true;
		}

		// The sendDetach() method works on the list of selected objects. Thus
		// we need to clear the list, make sure it only contains the object the
		// user clicked, send the message, then clear the list.
		// We use deselectAll to update the simulator's notion of what is
		// selected, and removeAll just to change things locally.
		// RN: I thought it was more useful to detach everything that was
		// selected.
		if (gSelectMgr.getSelection()->isAttachment())
		{
			gSelectMgr.sendDetach();
		}
		return true;
	}
};

// Adding an observer for a JIRA-2422 and needs to be a fetch observer for
// JIRA-3119
class LLWornItemFetchedObserver final : public LLInventoryFetchObserver
{
public:
	LLWornItemFetchedObserver()				{}
	~LLWornItemFetchedObserver() override	{}

protected:
	void done() override
	{
		gPieAttachmentp->buildDrawLabels();
		gInventory.removeObserver(this);
		delete this;
	}
};

bool enable_detach(void*)
{
	LLViewerObject* object = gSelectMgr.getSelection()->getPrimaryObject();
	if (!object || !object->isAttachment())
	{
		return false;
	}
//MK
	if (gRLenabled)
	{
		if (!gRLInterface.canDetach(object))
		{
			return false;
		}

		// Prevent a clever workaround that allowed to detach several objects
		// at the same time by selecting them
		if (gRLInterface.mContainsDetach &&
			gSelectMgr.getSelection()->getRootObjectCount() > 1)
		{
			return false;
		}
	}
//mk
	// Find the avatar who owns this attachment
	LLViewerObject* avatar = object;
	while (avatar)
	{
		// ...if it is you, good to detach
		if (avatar->getID() == gAgentID)
		{
			return true;
		}
		avatar = (LLViewerObject*)avatar->getParent();
	}

	return false;
}

// You can only drop items on parcels where you can build.
class LLAttachmentEnableDrop final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		// Add an inventory observer to only allow dropping the newly attached
		// item once it exists in your inventory. Look at JIRA-2422. -jwolk

		// A bug occurs when you wear/drop an item before it actively is added
		// to your inventory if this is the case (you're on a slow sim, etc),
		// a copy of the object, well, a newly created object with the same
		// properties, is placed in your inventory. Therefore, we disable the
		// drop option until the item is in your inventory

		LLViewerObject* object = gSelectMgr.getSelection()->getPrimaryObject();
		LLViewerJointAttachment* attachment_pt = NULL;
		LLInventoryItem* item = NULL;

		if (object)
		{
			S32 attach_id =
				ATTACHMENT_ID_FROM_STATE(object->getAttachmentState());
			attachment_pt = get_ptr_in_map(gAgentAvatarp->mAttachmentPoints,
										   attach_id);

			if (attachment_pt)
			{
				for (LLViewerJointAttachment::attachedobjs_vec_t::iterator
						iter = attachment_pt->mAttachedObjects.begin(),
						end = attachment_pt->mAttachedObjects.end();
					 iter != end; ++iter)
				{
					LLViewerObject* attached_object = iter->get();
					if (!attached_object) break;

					// Make sure item is in your inventory (it could be a
					// delayed attach message being sent from the sim) so check
					// to see if the item is in the inventory already
					item = gInventory.getItem(attached_object->getAttachmentItemID());
					if (!item)
					{
#if 0					// Disabled code, because, when applied on temporary
						// attachments (that never appear in inventory), it
						// causes an infinite number of observers to be added,
						// causing memory exhaustion and crash ! - HB
						// Item does not exist, make an observer to enable the
						// pie menu when the item finishes fetching worst case
						// scenario if a fetch is already out there (being sent
						// from a slow sim) we refetch and there are 2 fetches
						LLWornItemFetchedObserver* worn_item_fetched =
							new LLWornItemFetchedObserver();
						// Add item to the inventory item to be fetched:
						uuid_vec_t items;
						items.emplace_back((*iter)->getAttachmentItemID());
						worn_item_fetched->fetchItems(items);
						gInventory.addObserver(worn_item_fetched);
#else
						// Just exit with item == NULL, which disables the pie
						// slice (and is appropriate for temporary attachments
						// too).
						break;
#endif
					}
				}
			}
		}

		// Now check to make sure that the item is actually in the inventory
		// before we enable dropping it
		bool new_value = enable_detach(NULL) && item &&
						 gViewerParcelMgr.allowAgentBuild();

		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

class LLAttachmentEnableDetach final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		bool new_value = enable_detach(NULL);
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

// Used to tell if the selected object can be attached to your avatar.
bool object_selected_and_point_valid(void*)
{
	LLObjectSelectionHandle selection = gSelectMgr.getSelection();
	for (LLObjectSelection::root_iterator iter = selection->root_begin(),
										  end = selection->root_end();
		 iter != end; ++iter)
	{
		LLSelectNode* node = *iter;
		if (!node) continue;	// Paranoia
		LLViewerObject* object = node->getObject();
		if (!object) continue;	// Paranoia
		LLViewerObject::const_child_list_t& child_list = object->getChildren();
		for (LLViewerObject::child_list_t::const_iterator
				iter2 = child_list.begin(), end2 = child_list.end();
			 iter2 != end2; ++iter2)
		{
			LLViewerObject* child = *iter2;
			if (child->isAvatar())
			{
				return false;
			}
		}
	}

	return selection->getRootObjectCount() == 1 &&
		   selection->getFirstRootObject()->getPCode() == LL_PCODE_VOLUME &&
		   selection->getFirstRootObject()->permYouOwner() &&
		   !selection->getFirstRootObject()->flagObjectPermanent() &&
		   !((LLViewerObject*)selection->getFirstRootObject()->getRoot())->isAvatar() &&
		   selection->getFirstRootObject()->getNVPair("AssetContainer") == NULL;
}

// Also for seeing if object can be attached.  See above.
class LLObjectEnableWear final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
//MK
		if (gRLenabled && gRLInterface.mContainsDetach)
		{
			return false;
		}
//mk
		bool is_wearable = object_selected_and_point_valid(NULL);
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(is_wearable);
		return true;
	}
};

bool object_attached(void* user_data)
{
	LLViewerJointAttachment* attachment = (LLViewerJointAttachment*)user_data;
	return attachment && attachment->getNumObjects() > 0;
}

class LLAvatarSendIM final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
//MK
		if (gRLenabled &&
			(gRLInterface.mContainsShownames ||
			 gRLInterface.mContainsShownametags))
		{
			return false;
		}
//mk
		LLVOAvatar* avatar =
			find_avatar_from_object(gSelectMgr.getSelection()->getPrimaryObject());
		if (avatar)
		{
			LLAvatarActions::startIM(avatar->getID());
		}
		return true;
	}
};

namespace
{
	struct QueueObjects final : public LLSelectedObjectFunctor
	{
		bool scripted;
		bool modifiable;
		LLFloaterScriptQueue* mQueue;

		QueueObjects(LLFloaterScriptQueue* q)
		:	mQueue(q),
			scripted(false),
			modifiable(false)
		{
		}

		bool apply(LLViewerObject* obj) override
		{
			scripted = obj->flagScripted();
			modifiable = obj->permModify();
			return !(scripted && modifiable);
		}
	};
}

void queue_actions(LLFloaterScriptQueue* q, const std::string& noscriptmsg,
				   const std::string& nomodmsg)
{
	QueueObjects func(q);
	bool fail = gSelectMgr.getSelection()->applyToObjects(&func);
	if (fail)
	{
		if (!func.scripted)
		{
			gNotifications.add(noscriptmsg);
		}
		else if (!func.modifiable)
		{
			gNotifications.add(nomodmsg);
		}
		else
		{
			llerrs << "Bad logic." << llendl;
		}
	}
	else if (!q || !q->start())
	{
		llwarns << "Unexpected script compile failure." << llendl;
	}
}

void handle_compile_queue(std::string to_lang)
{
	LLFloaterCompileQueue* queue;
	if (to_lang == "mono")
	{
		queue = LLFloaterCompileQueue::create(true);
	}
	else
	{
		queue = LLFloaterCompileQueue::create(false);
	}
	queue_actions(queue, "CannotRecompileSelectObjectsNoScripts",
				  "CannotRecompileSelectObjectsNoPermission");
}

void handle_reset_selection()
{
	LLFloaterResetQueue* queue = LLFloaterResetQueue::create();
	queue_actions(queue, "CannotResetSelectObjectsNoScripts",
				  "CannotResetSelectObjectsNoPermission");
}

void handle_set_run_selection()
{
	LLFloaterRunQueue* queue = LLFloaterRunQueue::create();
	queue_actions(queue, "CannotSetRunningSelectObjectsNoScripts",
				  "CannotSerRunningSelectObjectsNoPermission");
}

void handle_set_not_run_selection()
{
	LLFloaterStopQueue* queue = LLFloaterStopQueue::create();
	queue_actions(queue, "CannotSetRunningNotSelectObjectsNoScripts",
				  "CannotSerRunningNotSelectObjectsNoPermission");
}

class LLToolsSelectedScriptAction final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
//MK
		// If there is at least one object locked in the selection, don't allow
		// anything
		if (gRLenabled && !gRLInterface.canDetachAllSelectedObjects())
		{
			return true;
		}
//mk
		std::string action = userdata.asString();
		if (action == "compile mono")
		{
			handle_compile_queue("mono");
		}
		if (action == "compile lsl")
		{
			handle_compile_queue("lsl");
		}
		else if (action == "reset")
		{
			handle_reset_selection();
		}
		else if (action == "start")
		{
			handle_set_run_selection();
		}
		else if (action == "stop")
		{
			handle_set_not_run_selection();
		}
		return true;
	}
};

//---------------------------------------------------------------------
// Callbacks for enabling/disabling items
//---------------------------------------------------------------------

// This is used in the GL menus to set control values.
class LLToggleControl final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		std::string control_name = userdata.asString();
		bool checked = gSavedSettings.getBool(control_name.c_str());
		if (control_name == "HighResSnapshot" && !checked)
		{
			// High Res Snapshot active, must uncheck RenderUIInSnapshot
			gSavedSettings.setBool("RenderUIInSnapshot", false);
		}
		gSavedSettings.setBool(control_name.c_str(), !checked);
		return true;
	}
};

class LLSomethingSelected final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		bool new_value = !(gSelectMgr.getSelection()->isEmpty());
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

class LLSomethingSelectedNoHUD final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		LLObjectSelectionHandle selection = gSelectMgr.getSelection();
		bool new_value = selection && !selection->isEmpty() &&
						 selection->getSelectType() != SELECT_TYPE_HUD;
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

static bool is_editable_selected()
{
	return gSelectMgr.getSelection()->getFirstEditableObject() != NULL;
}

class LLEditableSelected final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(is_editable_selected());
		return true;
	}
};

class LLEditableSelectedMono final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		if (gMenuHolderp &&
			gMenuHolderp->findControl(userdata["control"].asString()))
		{
			bool selected = is_editable_selected() &&
							!gAgent.getRegionCapability("UpdateScriptTask").empty();
			gMenuHolderp->findControl(userdata["control"].asString())->setValue(selected);
			return true;
		}
		return false;
	}
};

// Also called in llfloaterpathfindingobjects.
bool enable_object_take_copy()
{
	bool success = false;
	if (!gSelectMgr.getSelection()->isEmpty())
	{
		struct f final : public LLSelectedObjectFunctor
		{
			bool apply(LLViewerObject* obj) override
			{
//MK
				if (gRLenabled && gRLInterface.mContainsUnsit &&
					obj->isAgentSeat())
				{
					return true;
				}
//mk
				return !obj->permCopy() || obj->isAttachment();
			}
		} func;
		success = !gSelectMgr.getSelection()->applyToRootObjects(&func, true);
	}

	return success;
}

class LLToolsEnableTakeCopy final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		bool new_value = enable_object_take_copy();
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

bool enable_save_into_task_inventory(void*)
{
	LLSelectNode* node = gSelectMgr.getSelection()->getFirstRootNode();
	if (node && node->mValid && node->mFromTaskID.notNull())
	{
		// *TODO: check to see if the fromtaskid object exists.
		LLViewerObject* obj = node->getObject();
		if (obj && !obj->isAttachment())
		{
			return true;
		}
	}
	return false;
}

class LLToolsEnableSaveToObjectInventory final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		bool new_value = enable_save_into_task_inventory(NULL);
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

class LLViewEnableMouselook final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		// You cannot go directly from customize avatar to mouselook.
		// *TODO: write code with appropriate dialogs to handle this
		// transition.
		bool new_value = !LLPipeline::sFreezeTime &&
						 gAgent.getCameraMode() != CAMERA_MODE_CUSTOMIZE_AVATAR;
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

class LLToolsEnableToolNotPie final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		bool new_value = gToolMgr.getBaseTool() != &gToolPie;
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

class LLWorldEnableCreateLandmark final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
//MK
		if (gRLenabled && gRLInterface.mContainsShowloc)
		{
			return false;
		}
//mk
		bool new_value = gAgent.isGodlike() ||
						 (gAgent.getRegion() &&
						  gAgent.getRegion()->getAllowLandmark());
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

class LLWorldEnableSetHomeLocation final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		bool new_value = gAgent.isGodlike() ||
						 (gAgent.getRegion() &&
						  gAgent.getRegion()->getAllowSetHome());
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

class LLWorldEnableTeleportHome final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		LLViewerRegion* regionp = gAgent.getRegion();
		bool agent_on_prelude = regionp && regionp->isPrelude();
		bool enable_teleport_home = gAgent.isGodlike() || !agent_on_prelude;
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(enable_teleport_home);
		return true;
	}
};

class LLToolsSetSelectionsPolicy final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		U32 policy = userdata.asInteger();
		gSavedSettings.setU32("RenderHighlightSelectionsPolicy", policy);
		return true;
	}
};

class LLToolsShowSelectionsPolicy final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		static LLCachedControl<U32> policy(gSavedSettings,
										   "RenderHighlightSelectionsPolicy");
		bool checked = (U32)policy == (U32)userdata["data"].asInteger();
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(checked);
		return true;
	}
};

class LLToolsEditLinkedParts final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		bool select_individuals = gSavedSettings.getBool("EditLinkedParts");
		if (select_individuals)
		{
			gSelectMgr.demoteSelectionToIndividuals();
		}
		else
		{
			gSelectMgr.promoteSelectionToRoot();
		}
		return true;
	}
};

class LLToolsUseSelectionForGrid final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		gSelectMgr.clearGridObjects();
		struct f final : public LLSelectedObjectFunctor
		{
			bool apply(LLViewerObject* objectp) override
			{
				gSelectMgr.addGridObject(objectp);
				return true;
			}
		} func;
		gSelectMgr.getSelection()->applyToRootObjects(&func);
		gSelectMgr.setGridMode(GRID_MODE_REF_OBJECT);
		if (gFloaterToolsp)
		{
			gFloaterToolsp->setGridMode((S32)GRID_MODE_REF_OBJECT);
		}
		return true;
	}
};

//
// LLViewerMenuHolderGL
//

LLViewerMenuHolderGL::LLViewerMenuHolderGL() : LLMenuHolderGL()
{
}

bool LLViewerMenuHolderGL::hideMenus()
{
	bool handled = LLMenuHolderGL::hideMenus();

	// Drop pie menu selection
	mParcelSelection = NULL;
	mObjectSelection = NULL;

	gMenuBarViewp->clearHoverItem();
	gMenuBarViewp->resetMenuTrigger();

	return handled;
}

void LLViewerMenuHolderGL::setParcelSelection(LLSafeHandle<LLParcelSelection> selection)
{
	mParcelSelection = selection;
}

void LLViewerMenuHolderGL::setObjectSelection(LLSafeHandle<LLObjectSelection> selection)
{
	mObjectSelection = selection;
}

const LLRect LLViewerMenuHolderGL::getMenuRect() const
{
	return LLRect(0, getRect().getHeight() - gMenuBarHeight,
				  getRect().getWidth(), gStatusBarHeight);
}

// TomY TODO: Get rid of these?
class LLViewShowHoverTips final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		LLHoverView::sShowHoverTips = !LLHoverView::sShowHoverTips;
		return true;
	}
};

class LLViewCheckShowHoverTips final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		bool new_value = LLHoverView::sShowHoverTips;
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

class LLViewHighlightTransparent final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
//MK
		if (gRLenabled && gRLInterface.mContainsEdit)
		{
			return true;
		}
//mk
		LLDrawPoolAlpha::sShowDebugAlpha = !LLDrawPoolAlpha::sShowDebugAlpha;

		// Invisible objects skip building their render batches unless
		// sShowDebugAlpha is true, so rebuild batches whenever toggling this
		// flag.
		gPipeline.rebuildDrawInfo();

		return true;
	}
};

class LLViewCheckHighlightTransparent final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		bool new_value = LLDrawPoolAlpha::sShowDebugAlpha;
//MK
		if (gRLenabled && gRLInterface.mContainsEdit)
		{
			new_value = false;
		}
//mk
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

class LLViewToggleRenderType final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		std::string type = userdata.asString();
		if (type == "hideparticles")
		{
			LLPipeline::toggleRenderType(LLPipeline::RENDER_TYPE_PARTICLES);
		}
		return true;
	}
};

class LLViewCheckRenderType final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		std::string type = userdata["data"].asString();
		bool new_value = false;
		if (type == "hideparticles")
		{
			new_value =
				LLPipeline::toggleRenderTypeControlNegated((void*)LLPipeline::RENDER_TYPE_PARTICLES);
		}
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

class LLViewShowHUDAttachments final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		LLPipeline::sShowHUDAttachments = !LLPipeline::sShowHUDAttachments ||
//MK
										  (gRLenabled &&
										   gRLInterface.mHasLockedHuds);
//mk
		return true;
	}
};

class LLViewCheckHUDAttachments final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		bool new_value = LLPipeline::sShowHUDAttachments ||
//MK
						 (gRLenabled && gRLInterface.mHasLockedHuds);
//mk
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

//MK
class LLViewEnableHUDAttachments final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		bool new_value = !gRLenabled || !gRLInterface.mHasLockedHuds;
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};
//mk

class LLEditEnableTakeOff final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		std::string control_name = userdata["control"].asString();
		std::string clothing = userdata["data"].asString();
		bool new_value = false;
//MK
		if (gRLenabled && (gRLInterface.contains("remoutfit") ||
						   gRLInterface.contains("remoutfit:" + clothing)))
		{
			return false;
		}
//mk
		LLWearableType::EType type = LLWearableType::typeNameToType(clothing);
		if (type >= LLWearableType::WT_SHAPE &&
			type < LLWearableType::WT_COUNT)
		{
			new_value = LLAgentWearables::selfHasWearable(type);
		}
		gMenuHolderp->findControl(control_name)->setValue(new_value);
		return true;
	}
};

class LLEditTakeOff final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		std::string clothing = userdata.asString();
		if (clothing == "all")
		{
			LLAgentWearables::userRemoveAllClothes();
		}
		else
		{
			LLWearableType::EType type =
				LLWearableType::typeNameToType(clothing);
			LLAgentWearables::userRemoveWearablesOfType(type);
		}
		return true;
	}
};

class LLWorldChat final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		handle_chat(NULL);
		return true;
	}
};

class LLToolsSelectTool final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		std::string tool_name = userdata.asString();
//MK
		if (gRLenabled && gRLInterface.mContainsEdit)
		{
			LLViewerObject* objp =
				gSelectMgr.getSelection()->getPrimaryObject();
			if (!gRLInterface.canEdit(objp))
			{
				return true;
			}
		}
//mk
		if (tool_name == "focus")
		{
			gToolMgr.getCurrentToolset()->selectToolByIndex(1);
		}
		else if (tool_name == "move")
		{
			gToolMgr.getCurrentToolset()->selectToolByIndex(2);
		}
		else if (tool_name == "edit")
		{
			gToolMgr.getCurrentToolset()->selectToolByIndex(3);
		}
		else if (tool_name == "create")
		{
			gToolMgr.getCurrentToolset()->selectToolByIndex(4);
		}
		else if (tool_name == "land")
		{
			gToolMgr.getCurrentToolset()->selectToolByIndex(5);
		}
		else
		{
			llwarns << "Invalid tool name: " << tool_name << llendl;
		}

		return true;
	}
};

// Environment callbacks
class LLWorldEnvSettings final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
//MK
		if (gRLenabled && gRLInterface.mContainsSetenv)
		{
			return true;
		}
//mk

		std::string tod = userdata.asString();
		if (tod == "editor")
		{
			LLFloaterWindlight::showInstance();
			tod.clear();
		}
		else if (tod == "sunrise")
		{
			LLEnvironment::setSunrise();
		}
		else if (tod == "noon")
		{
			LLEnvironment::setMidday();
		}
		else if (tod == "sunset")
		{
			LLEnvironment::setSunset();
		}
		else if (tod == "midnight")
		{
			LLEnvironment::setMidnight();
		}
		else if (tod == "local")
		{
			HBFloaterLocalEnv::showInstance();
		}
		else	// "animate"
		{
			// The onWindlightChange() automation call will be done from
			// the proper callback in llviewercontrol.cpp...
			tod.clear();

			LLEnvironment::setRegion();
		}
		if (gAutomationp && !tod.empty())
		{
			gAutomationp->onWindlightChange(tod, "", "");
		}
		return true;
	}
};

///////////////////////////////////////////////////////////////////////////////
// Code formerly held in llviewermenufile.cpp
///////////////////////////////////////////////////////////////////////////////

typedef LLMemberListener<LLView> view_listener_t;

class LLFileEnableSaveAs final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		bool new_value = !HBFileSelector::isInUse() &&
						 gFloaterViewp->getFrontmost() &&
						 gFloaterViewp->getFrontmost()->canSaveAs();
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

class LLFileEnableUpload final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		S32 cost = LLEconomy::getInstance()->getPriceUpload();
		bool new_value = !HBFileSelector::isInUse() &&
						 can_afford_transaction(cost);
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

class LLFileEnableUploadAnim final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		S32 cost = LLEconomy::getInstance()->getAnimationUploadCost();
		bool new_value = !HBFileSelector::isInUse() &&
						 can_afford_transaction(cost);
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

class LLFileEnableUploadSound final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		S32 cost = LLEconomy::getInstance()->getSoundUploadCost();
		bool new_value = !HBFileSelector::isInUse() &&
						 can_afford_transaction(cost);
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

class LLFileEnableUploadImage final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		S32 cost = LLEconomy::getInstance()->getTextureUploadCost();
		bool new_value = !HBFileSelector::isInUse() &&
						 can_afford_transaction(cost);
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

class LLFileEnableUploadMaterial final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		bool new_value =
			!HBFileSelector::isInUse() && 
			gAgent.hasRegionCapability("UpdateMaterialAgentInventory");
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

class LLFileEnableUploadModel final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		bool new_value = !HBFileSelector::isInUse() &&
						 gMeshRepo.meshUploadEnabled() &&
						 LLFloaterModelPreview::findInstance() == NULL;
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

void upload_bulk_callback(HBFileSelector::ELoadFilter,
						  std::deque<std::string>& files, void*)
{
	if (files.empty())
	{
		return;
	}

	gUploadQueueMutex.lock();
	// First remember if there are ongoing uploads already in progress
	bool no_upload = gUploadQueue.empty();

	while (!files.empty())
	{
		gUploadQueue.emplace_back(files.front());
		files.pop_front();
	}
	gUploadQueueMutex.unlock();

	if (no_upload)
	{
		// Initiate bulk uploads.
		process_bulk_upload_queue();
	}
}

static std::string TEXT_EXTENSIONS = "txt";
static std::string SOUND_EXTENSIONS = "wav dsf";
static std::string IMAGE_EXTENSIONS = "tga png jpg jpeg bmp";
static std::string MATERIAL_EXTENSIONS = "gltf glb";
static std::string ANIM_EXTENSIONS =  "bvh anim";
static std::string XML_EXTENSIONS = "xml";
static std::string LSL_EXTENSIONS = "lsl";
static std::string MODEL_EXTENSIONS = "dae";
static std::string ALL_FILE_EXTENSIONS = "*.*";

std::string build_extensions_string(HBFileSelector::ELoadFilter filter)
{
	switch (filter)
	{
		case HBFileSelector::FFLOAD_ALL:
		default:
			return ALL_FILE_EXTENSIONS;

		case HBFileSelector::FFLOAD_TEXT:
			return TEXT_EXTENSIONS;

		case HBFileSelector::FFLOAD_XML:
		case HBFileSelector::FFLOAD_XUI:
			return XML_EXTENSIONS;

		case HBFileSelector::FFLOAD_SCRIPT:
			return LSL_EXTENSIONS;

		case HBFileSelector::FFLOAD_SOUND:
			return SOUND_EXTENSIONS;

		case HBFileSelector::FFLOAD_ANIM:
			return ANIM_EXTENSIONS;

		case HBFileSelector::FFLOAD_MODEL:
			return MODEL_EXTENSIONS;

		case HBFileSelector::FFLOAD_IMAGE:
			return IMAGE_EXTENSIONS;

		case HBFileSelector::FFLOAD_GLTF:
			return MATERIAL_EXTENSIONS;
	}
}

bool callback_anim_upload(const LLSD& notification, const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) == 0) // OK
	{
		std::deque<std::string> files;
		files.emplace_back(notification["payload"]["filename"].asString());
		upload_bulk_callback(HBFileSelector::FFLOAD_ANIM, files, NULL);
	}
	return false;
}

void upload_pick_callback(HBFileSelector::ELoadFilter type,
						  std::string& filename, void*)
{
	if (filename.empty())
	{
		return;
	}

	std::string ext = gDirUtilp->getExtension(filename);

	// strincmp() does not like NULL pointers
	if (ext.empty())
	{
		std::string short_name = gDirUtilp->getBaseFileName(filename);

		// No extension
		LLSD args;
		args["FILE"] = short_name;
		gNotifications.add("NoFileExtension", args);
		return;
	}
	else
	{
		// There is an extension: loop over the valid extensions and compare
		// to see if the extension is valid

		// Now grab the set of valid file extensions
		std::string valid_extensions = build_extensions_string(type);

		typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
		boost::char_separator<char> sep(" ");
		tokenizer tokens(valid_extensions, sep);
		tokenizer::iterator token_iter;

		// Now loop over all valid file extensions and compare them to the
		// extension of the file to be uploaded
		bool ext_valid = false;
		for (token_iter = tokens.begin();
			 token_iter != tokens.end() && !ext_valid;
			 ++token_iter)
		{
			const std::string& cur_token = *token_iter;

			if (cur_token == ext || cur_token == "*.*")
			{
				// Valid extension or the acceptable extension is any
				ext_valid = true;
			}
		}

		if (!ext_valid)
		{
			// Should only get here if the extension exists but is invalid
			LLSD args;
			args["EXTENSION"] = ext;
			args["VALIDS"] = valid_extensions;
			gNotifications.add("InvalidFileExtension", args);
			return;
		}
	}

	if (type == HBFileSelector::FFLOAD_IMAGE)
	{
		new LLFloaterImagePreview(filename);
	}
	else if (type == HBFileSelector::FFLOAD_GLTF)
	{
		LLPreviewMaterial::loadFromFile(filename);
	}
	else if (type == HBFileSelector::FFLOAD_SOUND)
	{
		// Pre-qualify wavs to make sure the format is acceptable
		F32 max_duration = 0.f; // 0 means using SL maximum duration default
		if (!gIsInSecondLife)
		{
			max_duration = gSavedSettings.getF32("OSMaxSoundDuration");
		}
		std::string error_msg;
		if (check_for_invalid_wav_formats(filename, error_msg, max_duration))
		{
			llinfos << error_msg << ": " << filename << llendl;
			LLSD args;
			args["FILE"] = filename;
			gNotifications.add(error_msg, args);
			return;
		}
		new HBFloaterUploadSound(filename);
	}
	else if (type == HBFileSelector::FFLOAD_ANIM)
	{
		if (ext == "bvh")
		{
			new LLFloaterAnimPreview(filename);
		}
		else	// *.anim files can only be bulk-uploaded...
		{
			LLSD payload;
			payload["filename"] = filename;
			LLSD args;
			args["FILE"] = filename;
			args["COST"] = LLEconomy::getInstance()->getAnimationUploadCost();
			gNotifications.add("ConfirmAnimUpload", args, payload,
							   callback_anim_upload);
		}
	}
}

void upload_pick(HBFileSelector::ELoadFilter type)
{
 	if (gAgent.cameraMouselook())
	{
		gAgent.changeCameraToDefault();
	}

	HBFileSelector::loadFile(type, upload_pick_callback);
}

class LLFileUploadImage final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		upload_pick(HBFileSelector::FFLOAD_IMAGE);
		return true;
	}
};

class LLFileUploadMaterial final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		if (gAgent.hasRegionCapability("UpdateMaterialAgentInventory"))
		{
			upload_pick(HBFileSelector::FFLOAD_GLTF);
		}
		return true;
	}
};

class LLFileUploadSound final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		upload_pick(HBFileSelector::FFLOAD_SOUND);
		return true;
	}
};

class LLFileUploadAnim final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		upload_pick(HBFileSelector::FFLOAD_ANIM);
		return true;
	}
};

class LLFileUploadBulk final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		if (gAgent.cameraMouselook())
		{
			gAgent.changeCameraToDefault();
		}

		HBFileSelector::loadFiles(HBFileSelector::FFLOAD_ALL,
								  upload_bulk_callback);
		return true;
	}
};

class LLFileUploadModel final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		LLFloaterModelPreview* fmp = LLFloaterModelPreview::getInstance();
		if (fmp)
		{
			fmp->loadModel(3);
		}
		return true;
	}
};

class LLFileEnableCloseWindow final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		// *HACK: before STATE_LOGIN_WAIT, the code below can cause an infinite
		// loop (for example when a "Display settings have been set to
		// recommended..." dialog is shown), thus the reason for enabling
		// the close option always before STATE_LOGIN_WAIT... Note that before
		// STATE_LOGIN_WAIT, the user got no chance whatsoever to see and
		// select the File -> Quit menu item anyway (it is not yet drawn)...
		bool new_value = LLStartUp::getStartupState() < STATE_LOGIN_WAIT ||
						 LLFloater::getClosableFloaterFromFocus() != NULL;
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

class LLFileCloseWindow final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		LLFloater::closeFocusedFloater();

		return true;
	}
};

class LLFileEnableCloseAllWindows final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		bool open_children = gFloaterViewp->allChildrenClosed();
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(!open_children);
		return true;
	}
};

class LLFileCloseAllWindows final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		bool app_quitting = false;
		gFloaterViewp->closeAllChildren(app_quitting);

		return true;
	}
};

class LLFileSaveTexture final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		LLFloater* top = gFloaterViewp->getFrontmost();
		if (top)
		{
			top->saveAs();
		}
		return true;
	}
};

class LLFileTakeSnapshot final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		LLFloaterSnapshot::show(NULL);
		return true;
	}
};

void snapshot_to_disk(HBFileSelector::ESaveFilter type, std::string& filename,
					  void*)
{
	if (filename.empty()) return;

	if (!gViewerWindowp->isSnapshotLocSet())
	{
		gViewerWindowp->setSnapshotLoc(filename);
	}

	LLPointer<LLImageRaw> raw = new LLImageRaw;

	S32 width = gViewerWindowp->getWindowDisplayWidth();
	S32 height = gViewerWindowp->getWindowDisplayHeight();

	if (gSavedSettings.getBool("HighResSnapshot"))
	{
		width *= 2;
		height *= 2;
	}

	if (gViewerWindowp->rawSnapshot(raw, width, height, true, false,
									gSavedSettings.getBool("RenderUIInSnapshot"),
									false))
	{
		gViewerWindowp->playSnapshotAnimAndSound();

		LLImageBase::setSizeOverride(true);
		LLPointer<LLImageFormatted> formatted;
		switch (type)
		{
			case HBFileSelector::FFSAVE_JPG:
				formatted = new LLImageJPEG(gSavedSettings.getS32("SnapshotQuality"));
				break;
			case HBFileSelector::FFSAVE_PNG:
				formatted = new LLImagePNG;
				break;
			case HBFileSelector::FFSAVE_BMP:
				formatted = new LLImageBMP;
				break;
			default:
				llwarns << "Unknown local snapshot format" << llendl;
				LLImageBase::setSizeOverride(false);
				return;
		}

		formatted->encode(raw);
		LLImageBase::setSizeOverride(false);
		gViewerWindowp->saveImageNumbered(formatted);
	}
}

class LLFileTakeSnapshotToDisk final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		HBFileSelector::ESaveFilter type;
		switch (gSavedSettings.getU32("SnapshotFormat"))
		{
			case LLFloaterSnapshot::SNAPSHOT_FORMAT_JPEG:
				type = HBFileSelector::FFSAVE_JPG;
				break;

			case LLFloaterSnapshot::SNAPSHOT_FORMAT_PNG:
				type = HBFileSelector::FFSAVE_PNG;
				break;

			case LLFloaterSnapshot::SNAPSHOT_FORMAT_BMP:
				type = HBFileSelector::FFSAVE_BMP;
				break;

			default:
				llwarns << "Unknown Local Snapshot format" << llendl;
				return true;
		}
		std::string suggestion = gViewerWindowp->getSnapshotBaseName();
		if (gViewerWindowp->isSnapshotLocSet())
		{
			snapshot_to_disk(type, suggestion, NULL);
		}
		else
		{
			HBFileSelector::saveFile(type, suggestion, snapshot_to_disk);
		}
		return true;
	}
};

class LLFileQuit final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		gAppViewerp->userQuit();
		return true;
	}
};

void compress_image_callback(HBFileSelector::ELoadFilter type,
							 std::deque<std::string>& files, void*)
{
	if (files.empty())
	{
		return;
	}

	LLSD args;
	std::string infile, extension, outfile, report;

	while (!files.empty())
	{
		infile = files.front();
		extension = gDirUtilp->getExtension(infile);
		EImageCodec codec = LLImageBase::getCodecFromExtension(extension);
		if (codec == IMG_CODEC_INVALID)
		{
			llinfos << "Error compressing image: " << infile
					<< " - Unknown codec !" << llendl;
		}

		outfile = gDirUtilp->getDirName(infile) + LL_DIR_DELIM_STR +
				  gDirUtilp->getBaseFileName(infile, true) + ".j2c";

		llinfos << "Compressing image... Input: " << infile << " - Output: "
				<< outfile << llendl;

		if (LLViewerTextureList::createUploadFile(infile, outfile, codec))
		{
			llinfos << "Compression complete" << llendl;
			report = infile + " successfully compressed to " + outfile;
		}
		else
		{
			report = LLImage::getLastError();
			llinfos << "Compression failed: " << report << llendl;
			report = " Failed to compress " + infile + " - " + report;
		}
		args["MESSAGE"] = report;
 		gNotifications.add("SystemMessageTip", args);

		files.pop_front();
	}
}

void handle_compress_image(void*)
{
	HBFileSelector::loadFiles(HBFileSelector::FFLOAD_IMAGE,
							  compress_image_callback);
}

//---------------------------------------------------------------------------
// Object backup/import and export functions.
//---------------------------------------------------------------------------

// When using the file selector, we open the build floater to be sure that the
// object(s) to export will stay selected during the file selection since the
// export methods return just after the file selector is opened and the right-
// clicked object gets auto-deselected while the file selector callback is
// still to come...
void open_tools_floater()
{
	if (gFloaterToolsp)
	{
		gFloaterToolsp->open();
		gToolMgr.setCurrentToolset(gBasicToolset);
		gFloaterToolsp->setEditTool(&gToolCompTranslate);
	}
}

class LLFileEnableBackupObject final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		LLViewerObject* object = gSelectMgr.getSelection()->getPrimaryObject();
		bool new_value = object && !HBFileSelector::isInUse() &&
						 !HBObjectBackup::findInstance();
//MK
		if (new_value && gRLenabled)
		{
			new_value = !gRLInterface.mContainsRez &&
						!gRLInterface.mContainsEdit;
		}
//mk
		if (new_value)
		{
			struct ff final : public LLSelectedNodeFunctor
			{
				ff(const LLSD& data)
				:	LLSelectedNodeFunctor(),
					userdata(data)
				{
				}

				bool apply(LLSelectNode* node) override
				{
					// Note: the actual permission checking algorithm depends
					// on the grid TOS and must be performed for each prim and
					// texture. This is done later in hbobjectbackup.cpp.
					// This means that even if the item is enabled in the menu,
					// the export may fail should the permissions not be met
					// for each exported asset. The permissions check below
					// therefore only corresponds to the minimal permissions
					// requirement common to all grids.
					LLPermissions *item_permissions = node->mPermissions;
					return (gAgentID == item_permissions->getOwner() &&
							(gAgentID == item_permissions->getCreator() ||
							 (item_permissions->getMaskOwner() &
							  PERM_ITEM_UNRESTRICTED) == PERM_ITEM_UNRESTRICTED));
				}

				const LLSD& userdata;
			};
			ff* the_ff = new ff(userdata);
			new_value = gSelectMgr.getSelection()->applyToNodes(the_ff, false);
		}
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

class LLFileBackupObject final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		LLViewerObject* object = gSelectMgr.getSelection()->getPrimaryObject();
		if (object)
		{
			open_tools_floater();
			HBObjectBackup::exportObject();
		}
		return true;
	}
};

class LLFileEnableImportObject final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		bool new_value = !HBFileSelector::isInUse() &&
						 gViewerParcelMgr.allowAgentBuild() &&
						 !HBObjectBackup::findInstance();
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

class LLFileImportObject final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		HBObjectBackup::importObject(false);
		return true;
	}
};

class LLFileUpLoadImportObject final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		HBObjectBackup::importObject(true);
		return true;
	}
};

class LLFileExportOBJ final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		LLViewerObject* object = gSelectMgr.getSelection()->getPrimaryObject();
		if (object)
		{
			open_tools_floater();
			ALWavefrontSaver::exportSelection();
		}
		return true;
	}
};

class LLFileExportDAE final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		LLViewerObject* object = gSelectMgr.getSelection()->getPrimaryObject();
		if (object)
		{
			//open_tools_floater();
			LKFloaterColladaExport::showInstance();
		}
		return true;
	}
};

class LLFileImportSettings final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		LLSettingsType::EType type = LLSettingsType::ST_NONE;
		std::string param = userdata.asString();
		if (param == "sky")
		{
			type = LLSettingsType::ST_SKY;
		}
		else if (param == "water")
		{
			type = LLSettingsType::ST_WATER;
		}
		else if (param == "day")
		{
			type = LLSettingsType::ST_DAYCYCLE;
		}
		if (type != LLSettingsType::ST_NONE)
		{
			HBFloaterEditEnvSettings* floaterp =
				HBFloaterEditEnvSettings::create(type);
			if (floaterp)
			{
				floaterp->setEditContextInventory();
				floaterp->loadDefaultSettings();
			}
		}
		return true;
	}
};

class LLFileEnableImportSettings final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		bool new_value = !HBFileSelector::isInUse() &&
						 gAgent.hasInventorySettings();
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

class LLAdvancedEnableLoadFromXML final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		bool new_value = !HBFileSelector::isInUse();
		gMenuHolderp->findControl(userdata["control"].asString())->setValue(new_value);
		return true;
	}
};

class LLAdvancedLoadFromXML final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		handle_load_from_xml(NULL);
		return true;
	}
};

// A parameterized event handler used as ctrl-8/9/0 zoom controls below.
class LLZoomer final : public view_listener_t
{
public:
	// The "mult" parameter says whether "val" is a multiplier or used to
	// set the value.
	LLZoomer(F32 val, bool mult = true)
	:	mVal(val),
		mMult(mult)
	{
	}

	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		F32 new_fov_rad = mMult ? gViewerCamera.getDefaultFOV() * mVal :  mVal;
		gViewerCamera.setDefaultFOV(new_fov_rad);
		// setView may have clamped it.
		gSavedSettings.setF32("CameraAngle", gViewerCamera.getView());
		return true;
	}

private:
	F32		mVal;
	bool	mMult;
};

class LLAvatarReportAbuse final : public view_listener_t
{
	bool handleEvent(LLPointer<LLEvent>, const LLSD& userdata) override
	{
		LLVOAvatar* avatarp =
			find_avatar_from_object(gSelectMgr.getSelection()->getPrimaryObject());
		if (avatarp)
		{
			LLFloaterReporter::showFromObject(avatarp->getID());
		}
		return true;
	}
};

///////////////////////////////////////////////////////////////////////////////

static void addMenu(view_listener_t* menu, const std::string& name)
{
	sMenus.push_back(menu);
	menu->registerListener(gMenuHolderp, name);
}

void initialize_menus()
{
	// File menu
	addMenu(new LLFileBackupObject(), "File.BackupObject");
	addMenu(new LLFileExportOBJ(), "File.ExportOBJ");
	addMenu(new LLFileExportDAE(), "File.ExportDAE");
	addMenu(new LLFileEnableBackupObject(), "File.EnableBackupObject");
	addMenu(new LLFileImportObject(), "File.ImportObject");
	addMenu(new LLFileUpLoadImportObject(), "File.UploadImportObject");
	addMenu(new LLFileEnableImportObject(), "File.EnableImportObject");
	addMenu(new LLFileImportSettings(), "File.ImportSettings");
	addMenu(new LLFileEnableImportSettings(), "File.EnableImportSettings");
	addMenu(new LLFileUploadImage(), "File.UploadImage");
	addMenu(new LLFileUploadMaterial(), "File.UploadMaterial");
	addMenu(new LLFileUploadSound(), "File.UploadSound");
	addMenu(new LLFileUploadAnim(), "File.UploadAnim");
	addMenu(new LLFileUploadBulk(), "File.UploadBulk");
	addMenu(new LLFileEnableUpload(), "File.EnableUpload");
	addMenu(new LLFileEnableUploadAnim(), "File.EnableUploadAnim");
	addMenu(new LLFileEnableUploadSound(), "File.EnableUploadSound");
	addMenu(new LLFileEnableUploadImage(), "File.EnableUploadImage");
	addMenu(new LLFileEnableUploadMaterial(), "File.EnableUploadMaterial");
	addMenu(new LLFileUploadModel(), "File.UploadModel");
	addMenu(new LLFileEnableUploadModel(), "File.EnableUploadModel");
	addMenu(new LLFileEnableSaveAs(), "File.EnableSaveAs");
	addMenu(new LLFileSaveTexture(), "File.SaveTexture");
	addMenu(new LLFileTakeSnapshot(), "File.TakeSnapshot");
	addMenu(new LLFileTakeSnapshotToDisk(), "File.TakeSnapshotToDisk");
	addMenu(new LLFileCloseWindow(), "File.CloseWindow");
	addMenu(new LLFileEnableCloseWindow(), "File.EnableCloseWindow");
	addMenu(new LLFileCloseAllWindows(), "File.CloseAllWindows");
	addMenu(new LLFileEnableCloseAllWindows(), "File.EnableCloseAllWindows");
	addMenu(new LLFileQuit(), "File.Quit");

	// Edit menu
	addMenu(new LLEditUndo(), "Edit.Undo");
	addMenu(new LLEditRedo(), "Edit.Redo");
	addMenu(new LLEditCut(), "Edit.Cut");
	addMenu(new LLEditCopy(), "Edit.Copy");
	addMenu(new LLEditPaste(), "Edit.Paste");
	addMenu(new LLEditDelete(), "Edit.Delete");
	addMenu(new LLEditSearch(), "Edit.Search");
	addMenu(new LLEditSelectAll(), "Edit.SelectAll");
	addMenu(new LLEditDeselect(), "Edit.Deselect");
	addMenu(new LLEditDuplicate(), "Edit.Duplicate");
	addMenu(new LLEditTakeOff(), "Edit.TakeOff");
	addMenu(new LLRemoveAllTempAttachments(), "Edit.RemoveAllTempAttachments");

	addMenu(new LLEditEnableUndo(), "Edit.EnableUndo");
	addMenu(new LLEditEnableRedo(), "Edit.EnableRedo");
	addMenu(new LLEditEnableCut(), "Edit.EnableCut");
	addMenu(new LLEditEnableCopy(), "Edit.EnableCopy");
	addMenu(new LLEditEnablePaste(), "Edit.EnablePaste");
	addMenu(new LLEditEnableDelete(), "Edit.EnableDelete");
	addMenu(new LLEditEnableSelectAll(), "Edit.EnableSelectAll");
	addMenu(new LLEditEnableDeselect(), "Edit.EnableDeselect");
	addMenu(new LLEditEnableDuplicate(), "Edit.EnableDuplicate");
	addMenu(new LLEditEnableTakeOff(), "Edit.EnableTakeOff");
	addMenu(new LLEditEnableCustomizeAvatar(), "Edit.EnableCustomizeAvatar");
	addMenu(new LLEditEnableOutfitPicker(), "Edit.EditEnableOutfitPicker");
	addMenu(new LLEditEnableDisplayName(), "Edit.EnableDisplayName");
	addMenu(new LLEnableRemoveAllTempAttachments(),
			"Edit.EnableRemoveAllTempAttachments");

	// View menu
	addMenu(new LLViewMouselook(), "View.Mouselook");
	addMenu(new LLViewJoystickFlycam(), "View.JoystickFlycam");
	addMenu(new LLViewResetView(), "View.ResetView");
	addMenu(new LLViewReleaseCamera(), "View.ReleaseCamera");
	addMenu(new LLViewEnableReleaseCamera(), "View.EnableReleaseCamera");

	addMenu(new LLViewLookAtLastChatter(), "View.LookAtLastChatter");
	addMenu(new LLViewShowHoverTips(), "View.ShowHoverTips");
	addMenu(new LLViewHighlightTransparent(), "View.HighlightTransparent");
	addMenu(new LLViewToggleRenderType(), "View.ToggleRenderType");
	addMenu(new LLViewShowHUDAttachments(), "View.ShowHUDAttachments");
	addMenu(new LLZoomer(1.2f), "View.ZoomOut");
	addMenu(new LLZoomer(1.f / 1.2f), "View.ZoomIn");
	addMenu(new LLZoomer(DEFAULT_FIELD_OF_VIEW, false), "View.ZoomDefault");
	addMenu(new LLViewDefaultUISize(), "View.DefaultUISize");

	addMenu(new LLViewEnableMouselook(), "View.EnableMouselook");
	addMenu(new LLViewEnableJoystickFlycam(), "View.EnableJoystickFlycam");
	addMenu(new LLViewEnableLastChatter(), "View.EnableLastChatter");
	addMenu(new LLViewEnableNearbyMedia(), "View.EnableNearbyMedia");
//MK
	addMenu(new LLViewEnableBeacons(), "View.EnableBeacons");
	addMenu(new LLViewEnableHUDAttachments(), "View.EnableHUDAttachments");
//mk

	addMenu(new LLViewCheckCameraFrontView(), "View.CheckCameraFrontView");
	addMenu(new LLViewCheckJoystickFlycam(), "View.CheckJoystickFlycam");
	addMenu(new LLViewCheckShowHoverTips(), "View.CheckShowHoverTips");
	addMenu(new LLViewCheckShowHoverTips(), "View.CheckShowHoverTips");
	addMenu(new LLViewCheckHighlightTransparent(),
			"View.CheckHighlightTransparent");
	addMenu(new LLViewCheckRenderType(), "View.CheckRenderType");
	addMenu(new LLViewCheckHUDAttachments(), "View.CheckHUDAttachments");

	// World menu
	addMenu(new LLWorldChat(), "World.Chat");
	addMenu(new LLWorldAlwaysRun(), "World.AlwaysRun");
	addMenu(new LLWorldSitOnGround(), "World.SitOnGround");
	addMenu(new LLWorldEnableSitOnGround(), "World.EnableSitOnGround");
	addMenu(new LLWorldFly(), "World.Fly");
	addMenu(new LLWorldEnableFly(), "World.EnableFly");
	addMenu(new LLWorldCreateLandmark(), "World.CreateLandmark");
	addMenu(new LLWorldSetHomeLocation(), "World.SetHomeLocation");
	addMenu(new LLWorldTeleportHome(), "World.TeleportHome");
	addMenu(new LLWorldTPtoGround(), "World.TPtoGround");
	addMenu(new LLWorldToggleRadar(), "World.ToggleAvatarList");
	addMenu(new LLWorldSetAway(), "World.SetAway");
	addMenu(new LLWorldSetBusy(), "World.SetBusy");
	addMenu(new LLWorldSetAutoReply(), "World.SetAutoReply");
	addMenu(new LLWorldStopAllAnimations(), "World.StopAllAnimations");
	addMenu(new LLWorldReleaseKeys(), "World.ReleaseKeys");
	addMenu(new LLWorldEnableReleaseKeys(), "World.EnableReleaseKeys");

	addMenu(new LLWorldEnableCreateLandmark(), "World.EnableCreateLandmark");
	addMenu(new LLWorldEnableSetHomeLocation(), "World.EnableSetHomeLocation");
	addMenu(new LLWorldEnableTeleportHome(), "World.EnableTeleportHome");
	addMenu(new LLWorldEnableBuyLand(), "World.EnableBuyLand");
	addMenu(new LLWorldEnableAvatarList(), "World.EnableAvatarList");
	addMenu(new LLWorldEnableExperiences(), "World.EnableExperiences");
	addMenu(new LLWorldEnableIfInSL(), "World.EnableInSL");
	addMenu(new LLWorldEnableIfNotInSL(), "World.EnableNotInSL");
	addMenu(new LLWorldEnableWindlightRegionTime(),
			"World.EnableWindlightRegionTime");
	addMenu(new LLWorldEnableParcelEnv(), "World.EnableParcelEnv");
	addMenu(new LLWorldEnableLocalEnv(), "World.EnableLocalEnv");
	addMenu(new LLWorldPbrAdjustHDR(), "World.PbrAdjustHDR");
	addMenu(new LLWorldPbrActive(), "World.PbrActive");

	addMenu(new LLWorldCheckAlwaysRun(), "World.CheckAlwaysRun");

	(new LLWorldEnvSettings())->registerListener(gMenuHolderp,
												 "World.EnvSettings");

	// Tools menu
	addMenu(new LLToolsBuildMode(), "Tools.BuildMode");
	addMenu(new LLToolsSelectTool(), "Tools.SelectTool");
	addMenu(new LLToolsSetSelectionsPolicy(), "Tools.SetSelectionsPolicy");
	addMenu(new LLToolsShowSelectionsPolicy(), "Tools.ShowSelectionsPolicy");
	addMenu(new LLToolsEditLinkedParts(), "Tools.EditLinkedParts");
	addMenu(new LLToolsSnapObjectXY(), "Tools.SnapObjectXY");
	addMenu(new LLToolsUseSelectionForGrid(), "Tools.UseSelectionForGrid");
	addMenu(new LLToolsSelectNextPartFace(), "Tools.SelectNextPart");
	addMenu(new LLToolsLink(), "Tools.Link");
	addMenu(new LLToolsUnlink(), "Tools.Unlink");
	addMenu(new LLToolsLookAtSelection(), "Tools.LookAtSelection");
	addMenu(new LLToolsBuyOrTake(), "Tools.BuyOrTake");
	addMenu(new LLToolsTakeCopy(), "Tools.TakeCopy");
	addMenu(new LLToolsSaveToObjectInventory(), "Tools.SaveToObjectInventory");
	addMenu(new LLToolsSelectedScriptAction(), "Tools.SelectedScriptAction");
	addMenu(new LLToolsEnablePathfinding(), "Tools.EnablePathfinding");

	addMenu(new LLToolsCheckBuildMode(), "Tools.CheckBuildMode");
	addMenu(new LLToolsEnableToolNotPie(), "Tools.EnableToolNotPie");
	addMenu(new LLToolsEnableSelectNextPart(), "Tools.EnableSelectNextPart");
	addMenu(new LLToolsEnableLink(), "Tools.EnableLink");
	addMenu(new LLToolsEnableUnlink(), "Tools.EnableUnlink");
	addMenu(new LLToolsEnableBuyOrTake(), "Tools.EnableBuyOrTake");
	addMenu(new LLToolsEnableTakeCopy(), "Tools.EnableTakeCopy");
	addMenu(new LLToolsEnableSaveToObjectInventory(),
			"Tools.SaveToObjectInventory");

	// Help menu
	// most items use the ShowFloater method

	// Advanced menu
	addMenu(new LLAdvancedLoadFromXML(), "Advanced.LoadFromXML");
	addMenu(new LLAdvancedEnableLoadFromXML(), "Advanced.EnableLoadFromXML");

	// Self pie menu
	addMenu(new HBSelfGroupTitles(), "Self.GroupTitles");
	addMenu(new LLSelfSitOrStand(), "Self.SitOrStand");
	addMenu(new LLSelfRemoveAllAttachments(), "Self.RemoveAllAttachments");
	addMenu(new LLRemoveAllTempAttachments(), "Self.RemoveAllTempAttachments");

	addMenu(new LLSelfEnableSitOrStand(), "Self.EnableSitOrStand");
	addMenu(new LLSelfEnableRemoveAllAttachments(),
			"Self.EnableRemoveAllAttachments");
	addMenu(new LLEnableRemoveAllTempAttachments(),
			"Self.EnableRemoveAllTempAttachments");

	 // Avatar pie menu
	addMenu(new LLObjectMute(), "Avatar.Mute");
	addMenu(new LLAvatarRender(), "Avatar.Render");
	addMenu(new LLAvatarToggleMaxLOD(), "Avatar.ToggleMaxLOD");
	addMenu(new LLAvatarEnableMaxLOD(), "Avatar.EnableMaxLOD");
	addMenu(new LLAvatarEnableNormalLOD(), "Avatar.EnableNormalLOD");
	addMenu(new LLAvatarAddFriend(), "Avatar.AddFriend");
	addMenu(new LLAvatarFreeze(), "Avatar.Freeze");
	addMenu(new LLAvatarDebug(), "Avatar.Debug");
	addMenu(new LLAvatarEnableDebug(), "Avatar.EnableDebug");
	addMenu(new LLAvatarInviteToGroup(), "Avatar.InviteToGroup");
	addMenu(new LLAvatarGiveCard(), "Avatar.GiveCard");
	addMenu(new LLAvatarEject(), "Avatar.Eject");
	addMenu(new LLAvatarSendIM(), "Avatar.SendIM");
	addMenu(new LLAvatarReportAbuse(), "Avatar.ReportAbuse");

	addMenu(new LLObjectEnableMute(), "Avatar.EnableMute");
	addMenu(new LLAvatarEnableAddFriend(), "Avatar.EnableAddFriend");
	addMenu(new LLAvatarEnableFreezeEject(), "Avatar.EnableFreezeEject");

	// Object pie menu
	addMenu(new LLObjectOpen(), "Object.Open");
	addMenu(new LLObjectBuild(), "Object.Build");
	addMenu(new LLObjectTouch(), "Object.Touch");
	addMenu(new LLObjectSitOrStand(), "Object.SitOrStand");
	addMenu(new LLObjectDelete(), "Object.Delete");
	addMenu(new LLObjectAttachToAvatar(), "Object.AttachToAvatar");
	addMenu(new LLObjectReturn(), "Object.Return");
	addMenu(new LLObjectReportAbuse(), "Object.ReportAbuse");
	addMenu(new LLObjectMute(), "Object.Mute");
	addMenu(new LLObjectToggleMaxLOD(), "Object.ToggleMaxLOD");
	addMenu(new LLObjectEnableMaxLOD(), "Object.EnableMaxLOD");
	addMenu(new LLObjectEnableNormalLOD(), "Object.EnableNormalLOD");
	addMenu(new LLObjectDerender(), "Object.Derender");
	addMenu(new LLObjectEnableDerender(), "Object.EnableDerender");
	addMenu(new LLObjectBuy(), "Object.Buy");
	addMenu(new LLObjectEdit(), "Object.Edit");
	addMenu(new LLObjectInspect(), "Object.Inspect");
	addMenu(new LLSelfInspect(), "Self.Inspect");

	addMenu(new LLObjectEnableOpen(), "Object.EnableOpen");
	addMenu(new LLObjectEnableTouch(), "Object.EnableTouch");
	addMenu(new LLObjectEnableSitOrStand(), "Object.EnableSitOrStand");
	addMenu(new LLObjectEnableDelete(), "Object.EnableDelete");
	addMenu(new LLObjectEnableWear(), "Object.EnableWear");
	addMenu(new LLObjectEnableReturn(), "Object.EnableReturn");
	addMenu(new LLObjectEnableReportAbuse(), "Object.EnableReportAbuse");
	addMenu(new LLObjectEnableMute(), "Object.EnableMute");
	addMenu(new LLObjectEnableBuy(), "Object.EnableBuy");

	// Attachment pie menu
	addMenu(new LLAttachmentDrop(), "Attachment.Drop");
	addMenu(new LLAttachmentDetach(), "Attachment.Detach");

	addMenu(new LLAttachmentEnableDrop(), "Attachment.EnableDrop");
	addMenu(new LLAttachmentEnableDetach(), "Attachment.EnableDetach");

	// Land pie menu
	addMenu(new LLLandBuild(), "Land.Build");
	addMenu(new LLLandSit(), "Land.Sit");
	addMenu(new LLLandBuyPass(), "Land.BuyPass");
	addMenu(new LLLandEdit(), "Land.Edit");

	addMenu(new LLLandEnableBuyPass(), "Land.EnableBuyPass");
	addMenu(new LLLandCanSit(), "Land.CanSit");

	// Particle pie menu
	addMenu(new LLMuteParticle(), "Particle.Mute");
	addMenu(new LLReportParticleAbuse(), "Particle.ReportAbuse");
	addMenu(new LLParticleRefreshTexture(), "Particle.RefreshTexture");
	addMenu(new LLParticleEnableEntry(), "Particle.EnableEntry");

	// Lua pie menu
	addMenu(new LLPieLuaCall(), "PieLua.Call");

	// Generic actions
	addMenu(new LLShowFloater(), "ShowFloater");
	addMenu(new LLPromptShowURL(), "PromptShowURL");
	addMenu(new LLPromptShowOneOfURLs(), "PromptShowOneOfURLs");
	addMenu(new LLPromptShowURLInternal(), "PromptShowURLInternal");
	addMenu(new LLShowAgentProfile(), "ShowAgentProfile");
	addMenu(new LLToggleControl(), "ToggleControl");

	addMenu(new LLGoToObject(), "GoToObject");
	addMenu(new LLPayObject(), "PayObject");

	addMenu(new LLEnablePayObject(), "EnablePayObject");
	addMenu(new LLEnableEdit(), "EnableEdit");

	addMenu(new LLFloaterVisible(), "FloaterVisible");
	addMenu(new LLSomethingSelected(), "SomethingSelected");
	addMenu(new LLSomethingSelectedNoHUD(), "SomethingSelectedNoHUD");
	addMenu(new LLEditableSelected(), "EditableSelected");
	addMenu(new LLEditableSelectedMono(), "EditableSelectedMono");
}
