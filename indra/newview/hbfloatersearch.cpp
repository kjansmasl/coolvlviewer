/**
 * @file hbfloatersearch.cpp
 * @brief The "Search" floater and its Web tab implementation.
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 *
 * Copyright (c) 2002-2009, Linden Research, Inc. (c) 2009-2021, Henri Beauchamp.
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

// This used to be LL's v1 viewer "Directory" floater (llfloaterdirectory.cpp),
// and was modified/expanded by Henri Beauchamp to add web search, showcase and
// Marketplace when LL added them to their own v2+ viewers, as well as support
// for OpenSim grids (optional) web search.

#include "llviewerprecompiledheaders.h"

#include "hbfloatersearch.h"

#include "llbutton.h"
#include "llkeyboard.h"
#include "lldir.h"
#include "llpluginclassmedia.h"
#include "llradiogroup.h"
#include "llresizehandle.h"
#include "llscrollcontainer.h"
#include "llscrollbar.h"
#include "lltabcontainer.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "llcommandhandler.h"
#include "llfloateravatarinfo.h"
#include "llgridmanager.h"				// For gIsInSecondLife
#include "llmediactrl.h"
#include "llpanelavatar.h"
#include "llpanelevent.h"
#include "llpanelclassified.h"
#include "llpaneldirclassified.h"
#include "llpaneldirevents.h"
#include "llpaneldirfind.h"
#include "llpaneldirgroups.h"
#include "llpaneldirland.h"
#include "llpaneldirpeople.h"
#include "llpaneldirplaces.h"
#include "llpanelgroup.h"
#include "llpanelpick.h"
#include "llpanelplace.h"
#include "llviewercontrol.h"
#include "llviewerparcelmgr.h"
#include "llweb.h"

///////////////////////////////////////////////////////////////////////////////
// Command handler for search SLURLs
///////////////////////////////////////////////////////////////////////////////

// Support secondlife:///app/search/{CATEGORY}/{QUERY} SLapps
class LLSearchHandler final : public LLCommandHandler
{
public:
	LLSearchHandler()
	:	LLCommandHandler("search", UNTRUSTED_THROTTLE)
	{
	}

	bool handle(const LLSD& tokens, const LLSD&, LLMediaCtrl*) override
	{
		const size_t parts = tokens.size();

#if 0	// The category is now (11-2021) ignored since not possible to pass
		// "as is" to the newest SL web-based search
		// Get the (optional) category for the search
		std::string category = "all";
		if (parts > 0)
		{
			category = tokens[0].asString();
		}
#endif
		// Get the (optional) search string
		std::string search_text;
		if (parts > 1)
		{
			search_text = tokens[1].asString();
		}

		HBFloaterSearch::showFindAll(search_text);

		return true;
	}
};

LLSearchHandler gSearchHandler;

///////////////////////////////////////////////////////////////////////////////
// HBPanelWebSearch class, for the Web search panel. Implemented in this module
// since no other module is using it... This tab merely displays a web browser.
///////////////////////////////////////////////////////////////////////////////

class HBPanelWebSearch : public LLPanelDirBrowser, public LLViewerMediaObserver
{
protected:
	LOG_CLASS(HBPanelWebSearch);

public:
	HBPanelWebSearch(const std::string& name, HBFloaterSearch* floater);
	~HBPanelWebSearch() override;

	bool postBuild() override;
	void draw() override;
	void onVisibilityChange(bool new_visibility) override;
	void reshape(S32 width, S32 height, bool call_from_parent = true) override;

	void search(const std::string& text = LLStringUtil::null);

	LL_INLINE static void setOSSearchURL(const std::string& url)
	{
		sOSSearchURL = url;
	}

	LL_INLINE static std::string getOSSearchURL()
	{
		return sOSSearchURL;
	}

protected:
	static void onClickBack(void* data);
	static void onClickForward(void* data);
	static void onClickReload(void* data);
	static void onCommitSelectionRadio(LLUICtrl* ctrl, void* data);

	// Inherited from LLViewerMediaObserver
	void handleMediaEvent(LLPluginClassMedia*, EMediaEvent event) override;

protected:
	LLMediaCtrl*		mWebBrowser;

	LLButton* 			mBackButton;
	LLButton* 			mForwardButton;
	LLButton* 			mReloadButton;

	LLRadioGroup*		mSelectionRadio;

	bool				mReloading;

	static std::string	sOSSearchURL;
};

//static
std::string	HBPanelWebSearch::sOSSearchURL;

HBPanelWebSearch::HBPanelWebSearch(const std::string& name,
								   HBFloaterSearch* floater)
:	LLPanelDirBrowser(name, floater),
	// NOTE: a reshape() event occurs before mWebBrowser is created, so we must
	// check for NULL whenever reshape() is called...
	mWebBrowser(NULL),
	mReloading(false)
{
}

bool HBPanelWebSearch::postBuild()
{
	LLPanelDirBrowser::postBuild();

	mBackButton = getChild<LLButton>("back_btn");
	mBackButton->setClickedCallback(onClickBack, this);

	mForwardButton = getChild<LLButton>("forward_btn");
	mForwardButton->setClickedCallback(onClickForward, this);

	mReloadButton = getChild<LLButton>("reload_btn");
	mReloadButton->setClickedCallback(onClickReload, this);

	mSelectionRadio = getChild<LLRadioGroup>("web_site");
	if (gIsInSecondLife)
	{
		U32 selection = gSavedSettings.getU32("WebSearchSiteSelection");
		mSelectionRadio->selectNthItem(selection);
		mSelectionRadio->setCommitCallback(onCommitSelectionRadio);
		mSelectionRadio->setCallbackUserData(this);
	}
	else
	{
		mSelectionRadio->setVisible(false);
	}

	mWebBrowser = getChild<LLMediaCtrl>("find_browser");
	mWebBrowser->addObserver(this);

	// We need to handle secondlife:///app/ URLs for direct teleports
	mWebBrowser->setTrusted(true);

	// Redirect 404 pages from S3 somewhere else
	mWebBrowser->setErrorPageURL(getString("redirect_404_url"));

	search();

	return true;
}

HBPanelWebSearch::~HBPanelWebSearch()
{
	if (mWebBrowser)
	{
		mWebBrowser->remObserver(this);
	}
}

//virtual
void HBPanelWebSearch::draw()
{
	if (mWebBrowser)
	{
		// Enable/disable buttons depending on state
		mBackButton->setEnabled(mWebBrowser->canNavigateBack());
		mForwardButton->setEnabled(mWebBrowser->canNavigateForward());
	}
	LLPanelDirBrowser::draw();
}

//virtual
void HBPanelWebSearch::reshape(S32 width, S32 height, bool called_from_parent)
{
#if 0
	handleMediaEvent(NULL, MEDIA_EVENT_NAVIGATE_BEGIN);
#endif
	if (mWebBrowser)
	{
		mWebBrowser->navigateTo(mWebBrowser->getCurrentNavUrl());
	}
	LLUICtrl::reshape(width, height, called_from_parent);
}

// When we show any web browser-based view, we want to hide all the right-side
// XUI detail panels.
//virtual
void HBPanelWebSearch::onVisibilityChange(bool new_visibility)
{
	if (new_visibility)
	{
		mFloaterSearch->hideAllDetailPanels();
	}
	LLPanel::onVisibilityChange(new_visibility);
}

// Note: we actually do not use any more the search_text in the viewer code
// calling this method (which is now limited to this panel code). This is
// because LL broke simple category/query searches in its latest (Nov 2021) web
// search engine version... Searches from the status bar (which was the only
// consumer of this method outside this module) are now done via the old (but
// universal and stable for the past 15 years) non-web interface. HB
void HBPanelWebSearch::search(const std::string& search_text)
{
	std::string url;
	if (gIsInSecondLife)
	{
		S32 selection = mSelectionRadio->getSelectedIndex();
		if (selection == 1)
		{
			url = getString("showcase_url");
			mWebBrowser->navigateTo(url);
		}
		else if (selection == 2)
		{
			url = getString("marketplace_url");
		}
		else
		{
			url = gSavedSettings.getString("SearchURL");
		}
	}
	else
	{
		url = sOSSearchURL;
	}
	if (url.empty())
	{
		// This happens when the panel is created while logged in an OpenSim
		// grid without a search URL.
		return;
	}
	LLStringUtil::format_map_t subs;
#if 0	// Not used: see the note above. HB
	if (!search_text.empty() || url.find("[QUERY]" != std::string::npos))
	{
		if (!gIsInSecondLife)
		{
			// By default, do a standard search in places...
			subs["[SEARCH_TYPE]"] = "standard";
			subs["[COLLECTION]"] = "places";
		}
		else if (url.find("[QUERY]") == std::string::npos)
		{
			// Missing query field in the search url (seen in Speculoos), let's
			// add one (assumption is made that that field is "q=<querry>",
			// which is reasonnable enough):
			llwarns_sparse << "Missing query field in the search URL... Using SL's OLD search engine query format..."
						   << llendl;
			if (url.find('?') == std::string::npos)
			{
				url += "?";
			}
			else if (url.rfind('&') != url.length() - 1)
			{
				url += "&";
			}
			url += "q=[QUERY]";
		}
		subs["[QUERY]"] = LLURI::escape(search_text);
	}
#else	// Make any query parameter a no-operation
	if (!gIsInSecondLife && url.find("[QUERY]") != std::string::npos)
	{
		// Make sure we will not search for "[QUERY]" in OpenSim grids !
		subs["[QUERY]"] = LLStringUtil::null;
	}
#endif
	if (!gIsInSecondLife && url.find("[CATEGORY]") != std::string::npos)
	{
		subs["[CATEGORY]"] = "search";	// Means "everything"
		// Warn when not in SL, since we do not have a documented list of
		// allowed categories in search queries...
		llwarns_sparse << "There is a category field in the search URL, but valid categories for this grid are unknown: using SL's old search engine global category..."
					   << llendl;
	}
	if (url.find("[MATURITY]") != std::string::npos)
	{
		std::string maturity;
		if (gAgent.prefersAdult())
		{
			maturity = gIsInSecondLife ? "gma" : "42";  // PG, Mature, Adult
		}
		else if (gAgent.prefersMature())
		{
			maturity = gIsInSecondLife ? "gm" : "21";  // PG, Mature
		}
		else
		{
			maturity = gIsInSecondLife ? "g" : "13";  // PG
		}
		// Add the user's maturity preferences/ranking
		subs["[MATURITY]"] = maturity;

		// Warn when not in SL, since we do not have a documented way to encode
		// the maturity rating in search queries...
		if (!gIsInSecondLife)
		{
			llwarns_sparse << "There is a maturity field in the search URL, but its encoding for this grid is unknown: using SL's old search engine encoding conventions..."
						   << llendl;
		}
	}
	if (url.find("[TEEN]") != std::string::npos)
	{
		// Add the agent's teen status
		subs["[TEEN]"] = gAgent.isTeen() ? "y" : "n";
	}
	// Expand all our substitutions and also [LANGUAGE], [VERSION], etc...
	url = LLWeb::expandURLSubstitutions(url, subs);
	handleMediaEvent(NULL, MEDIA_EVENT_NAVIGATE_BEGIN);
	mWebBrowser->navigateTo(url);
}

//static
void HBPanelWebSearch::onClickBack(void* data)
{
	HBPanelWebSearch* self = (HBPanelWebSearch*)data;
	if (self)
	{
		self->handleMediaEvent(NULL, MEDIA_EVENT_NAVIGATE_BEGIN);
		self->mWebBrowser->navigateBack();
	}
}

//static
void HBPanelWebSearch::onClickForward(void* data)
{
	HBPanelWebSearch* self = (HBPanelWebSearch*)data;
	if (self)
	{
		self->handleMediaEvent(NULL, MEDIA_EVENT_NAVIGATE_BEGIN);
		self->mWebBrowser->navigateForward();
	}
}

//static
void HBPanelWebSearch::onClickReload(void* data)
{
	HBPanelWebSearch* self = (HBPanelWebSearch*)data;
	if (self)
	{
		std::string url = self->mWebBrowser->getCurrentNavUrl();
		self->mReloading = true;
		self->mWebBrowser->navigateTo("about:blank");
		self->mWebBrowser->navigateTo(url);
		self->handleMediaEvent(NULL, MEDIA_EVENT_NAVIGATE_BEGIN);
	}
}

//static
void HBPanelWebSearch::onCommitSelectionRadio(LLUICtrl* ctrl, void* data)
{
	HBPanelWebSearch* self = (HBPanelWebSearch*)data;
	if (self)
	{
		S32 selection = self->mSelectionRadio->getSelectedIndex();
		if (selection >= 0)
		{
			gSavedSettings.setU32("WebSearchSiteSelection", selection);
		}
		self->search();
	}
}

//virtual
void HBPanelWebSearch::handleMediaEvent(LLPluginClassMedia*,
										EMediaEvent event)
{
	switch (event)
	{
		case MEDIA_EVENT_NAVIGATE_BEGIN:
		{
			mReloadButton->setEnabled(false);
			childSetText("status_text", getString("loading_text"));
			break;
		}

		case MEDIA_EVENT_NAVIGATE_COMPLETE:
		{
			std::string url = mWebBrowser->getCurrentNavUrl();
			if (!mReloading || url != "about:blank")
			{
				mReloading = false;
				mReloadButton->setEnabled(true);
				childSetText("status_text", getString("done_text"));
			}
			break;
		}

		default:	// Let's ignore other events
			break;
	}
}

///////////////////////////////////////////////////////////////////////////////
// HBFloaterSearch class proper
///////////////////////////////////////////////////////////////////////////////

//static
bool HBFloaterSearch::sSearchURLSetOnLogin = false;

HBFloaterSearch::HBFloaterSearch(const LLSD&)
:	mSearchWebPanel(NULL),
	mFindAllPanel(NULL),
	mEventsPanel(NULL),
	mLandPanel(NULL),
	mPanelAvatarp(NULL),
	mPanelEventp(NULL),
	mPanelGroupp(NULL),
	mPanelGroupHolderp(NULL),
	mPanelPlacep(NULL),
	mPanelPlaceSmallp(NULL),
	mPanelClassifiedp(NULL)
{
	// Build the floater with our tab panel classes
	LLCallbackMap::map_t factory_map;
	factory_map["find_all_panel"] = LLCallbackMap(createFindAll, this);
	factory_map["classified_panel"] = LLCallbackMap(createClassified, this);
	factory_map["events_panel"] = LLCallbackMap(createEvents, this);
	factory_map["places_panel"] = LLCallbackMap(createPlaces, this);
	factory_map["land_sales_panel"] = LLCallbackMap(createLand, this);
	factory_map["people_panel"] = LLCallbackMap(createPeople, this);
	factory_map["groups_panel"] = LLCallbackMap(createGroups, this);
	factory_map["web_search_panel"] = LLCallbackMap(createWebSearch, this);

	factory_map["classified_details_panel"] =
		LLCallbackMap(createClassifiedDetail, this);
	factory_map["event_details_panel"] =
		LLCallbackMap(createEventDetail, this);
	factory_map["group_details_panel"] =
		LLCallbackMap(createGroupDetail, this);
	factory_map["group_details_panel_holder"] =
		LLCallbackMap(createGroupDetailHolder, this);
	factory_map["place_details_panel"] =
		LLCallbackMap(createPlaceDetail, this);
	factory_map["place_details_small_panel"] =
		LLCallbackMap(createPlaceDetailSmall, this);

	factory_map["Panel Avatar"] = LLCallbackMap(createPanelAvatar, this);

	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_directory.xml",
												 &factory_map);
	moveResizeHandlesToFront();

	mTabsContainer = getChild<LLTabContainer>("Directory Tabs");

	// There is not always a web search URL in OpenSim drids...
	if (!gIsInSecondLife && mSearchWebPanel &&
		HBPanelWebSearch::getOSSearchURL().empty())
	{
		mTabsContainer->removeTabPanel(mSearchWebPanel);
		delete mSearchWebPanel;
		mSearchWebPanel = NULL;
	}

	if (mPanelAvatarp)
	{
		mPanelAvatarp->selectTab(0);
	}

	childSetTabChangeCallback("Directory Tabs", "find_all_panel",
							  onTabChanged, this);
	childSetTabChangeCallback("Directory Tabs", "classified_panel",
							  onTabChanged, this);
	childSetTabChangeCallback("Directory Tabs", "events_panel",
							  onTabChanged, this);
	childSetTabChangeCallback("Directory Tabs", "places_panel",
							  onTabChanged, this);
	childSetTabChangeCallback("Directory Tabs", "land_sales_panel",
							  onTabChanged, this);
	childSetTabChangeCallback("Directory Tabs", "people_panel",
							  onTabChanged, this);
	childSetTabChangeCallback("Directory Tabs", "groups_panel",
							  onTabChanged, this);
	childSetTabChangeCallback("Directory Tabs", "web_search_panel",
							  onTabChanged, this);

	mTeleportArrivingConnection =
		gViewerParcelMgr.setTPArrivingCallback(boost::bind(&HBFloaterSearch::onTeleportArriving));
}

//virtual
HBFloaterSearch::~HBFloaterSearch()
{
	mTeleportArrivingConnection.disconnect();

	// Note: this function is defined in the class LLFloater. However, it
	// causes crash if this line is postponed to ~LLFloater() because it uses
	// some pointers deleted below. That is, those pointers are used again
	// after deleting.
	setMinimized(false);

	delete mPanelAvatarp;
	mPanelAvatarp = NULL;
	delete mPanelEventp;
	mPanelEventp = NULL;
	delete mPanelGroupp;
	mPanelGroupp = NULL;
	delete mPanelGroupHolderp;
	mPanelGroupHolderp = NULL;
	delete mPanelPlacep;
	mPanelPlacep = NULL;
	delete mPanelPlaceSmallp;
	mPanelPlaceSmallp = NULL;
	delete mPanelClassifiedp;
	mPanelClassifiedp = NULL;
	gSavedSettings.setBool("ShowSearch", false);
}

//virtual
void HBFloaterSearch::setVisible(bool visible)
{
	gSavedSettings.setBool("ShowSearch", visible);
	LLFloater::setVisible(visible);
}

void HBFloaterSearch::focusCurrentPanel()
{
	LLPanel* panel = mTabsContainer->getCurrentPanel();
	if (panel)
	{
		panel->setFocus(true);
	}
}

//static
void* HBFloaterSearch::createFindAll(void* userdata)
{
	HBFloaterSearch* self = (HBFloaterSearch*)userdata;
	self->mFindAllPanel = new LLPanelDirFind("find_all_panel", self);
	return self->mFindAllPanel;
}

//static
void* HBFloaterSearch::createClassified(void* userdata)
{
	HBFloaterSearch* self = (HBFloaterSearch*)userdata;
	self->mClassifiedPanel = new LLPanelDirClassified("classified_panel",
													  self);
	return self->mClassifiedPanel;
}

//static
void* HBFloaterSearch::createEvents(void* userdata)
{
	HBFloaterSearch* self = (HBFloaterSearch*)userdata;
	self->mEventsPanel = new LLPanelDirEvents("events_panel", self);
	return self->mEventsPanel;
}

//static
void* HBFloaterSearch::createPlaces(void* userdata)
{
	HBFloaterSearch* self = (HBFloaterSearch*)userdata;
	return new LLPanelDirPlaces("places_panel", self);
}

//static
void* HBFloaterSearch::createLand(void* userdata)
{
	HBFloaterSearch* self = (HBFloaterSearch*)userdata;
	self->mLandPanel = new LLPanelDirLand("land_panel", self);
	return self->mLandPanel;
}

//static
void* HBFloaterSearch::createPeople(void* userdata)
{
	HBFloaterSearch* self = (HBFloaterSearch*)userdata;
	return new LLPanelDirPeople("people_panel", self);
}

//static
void* HBFloaterSearch::createGroups(void* userdata)
{
	HBFloaterSearch* self = (HBFloaterSearch*)userdata;
	return new LLPanelDirGroups("groups_panel", self);
}

//static
void* HBFloaterSearch::createWebSearch(void* userdata)
{
	HBFloaterSearch* self = (HBFloaterSearch*)userdata;
	self->mSearchWebPanel = new HBPanelWebSearch("web_search_panel", self);
	return self->mSearchWebPanel;
}

//static
void* HBFloaterSearch::createClassifiedDetail(void* userdata)
{
	HBFloaterSearch* self = (HBFloaterSearch*)userdata;
	self->mPanelClassifiedp = new LLPanelClassified(true, false);
	self->mPanelClassifiedp->setVisible(false);
	return self->mPanelClassifiedp;
}

//static
void* HBFloaterSearch::createPanelAvatar(void* userdata)
{
	HBFloaterSearch* self = (HBFloaterSearch*)userdata;
	// Note: false to disallow editing in search context (SL-48632):
	self->mPanelAvatarp = new LLPanelAvatar("Avatar", LLRect(), false);
	self->mPanelAvatarp->setVisible(false);
	return self->mPanelAvatarp;
}

//static
void* HBFloaterSearch::createEventDetail(void* userdata)
{
	HBFloaterSearch* self = (HBFloaterSearch*)userdata;
	self->mPanelEventp = new LLPanelEvent();
	LLUICtrlFactory::getInstance()->buildPanel(self->mPanelEventp,
											   "panel_event.xml");
	self->mPanelEventp->setVisible(false);
	return self->mPanelEventp;
}

//static
void* HBFloaterSearch::createGroupDetail(void* userdata)
{
	HBFloaterSearch* self = (HBFloaterSearch*)userdata;
	self->mPanelGroupp = new LLPanelGroup("panel_group.xml",
										  "PanelGroup",
										  gAgent.getGroupID());
	// Gods can always edit panels
	self->mPanelGroupp->setAllowEdit(gAgent.isGodlike());
	self->mPanelGroupp->setVisible(false);
	return self->mPanelGroupp;
}

//static
void* HBFloaterSearch::createGroupDetailHolder(void* userdata)
{
	HBFloaterSearch* self = (HBFloaterSearch*)userdata;
	self->mPanelGroupHolderp = new LLPanel(std::string("PanelGroupHolder"));
	self->mPanelGroupHolderp->setVisible(false);
	return self->mPanelGroupHolderp;
}

//static
void* HBFloaterSearch::createPlaceDetail(void* userdata)
{
	HBFloaterSearch* self = (HBFloaterSearch*)userdata;
	self->mPanelPlacep = new LLPanelPlace(false);
	LLUICtrlFactory::getInstance()->buildPanel(self->mPanelPlacep,
											   "panel_place.xml");
	self->mPanelPlacep->setVisible(false);
	return self->mPanelPlacep;
}

//static
void* HBFloaterSearch::createPlaceDetailSmall(void* userdata)
{
	HBFloaterSearch* self = (HBFloaterSearch*)userdata;
	self->mPanelPlaceSmallp = new LLPanelPlace(false);
	LLUICtrlFactory::getInstance()->buildPanel(self->mPanelPlaceSmallp,
											   "panel_place_small.xml");
	self->mPanelPlaceSmallp->setVisible(false);
	return self->mPanelPlaceSmallp;
}

//static
void HBFloaterSearch::setSearchURL(const std::string& url, bool on_login)
{
	sSearchURLSetOnLogin = on_login;
	if (gIsInSecondLife)
	{
		// Nothing else to do: SL got its own, hard-coded search URL...
		return;
	}
	HBPanelWebSearch::setOSSearchURL(url);

	HBFloaterSearch* self = findInstance();
	if (!self)
	{
		// Nothing else to do: the tabs will be created as needed on floater
		// opening...
		return;
	}

	bool url_is_empty = url.empty();
	if (url_is_empty && self->mSearchWebPanel)
	{
		// No web search available... Remove the tab.
		self->mTabsContainer->removeTabPanel(self->mSearchWebPanel);
		delete self->mSearchWebPanel;
		self->mSearchWebPanel = NULL;
	}
	else if (!url_is_empty && !self->mSearchWebPanel)
	{
		// No "Web search" tab while the URL is not empty... We need to destroy
		// the floater in order to get back the tab after the floater will be
		// re-opened...
		self->close();
	}
}

//static
bool HBFloaterSearch::wasSearchURLSetOnLogin()
{
	return sSearchURLSetOnLogin;
}

//static
void HBFloaterSearch::requestClassifieds()
{
	HBFloaterSearch* self = findInstance();
	if (self && self->mClassifiedPanel)
	{
		self->mClassifiedPanel->performQuery();
	}
}

//static
void HBFloaterSearch::showFindAll(const std::string& text)
{
	showPanel("find_all_panel");
	HBFloaterSearch* self = findInstance();
	if (self && self->mFindAllPanel)
	{
		self->mFindAllPanel->search(text);
	}
}

//static
void HBFloaterSearch::showClassified(const LLUUID& classified_id)
{
	showPanel("classified_panel");
	HBFloaterSearch* self = findInstance();
	if (self && self->mClassifiedPanel)
	{
		self->mClassifiedPanel->selectByUUID(classified_id);
	}
}

//static
void HBFloaterSearch::showEvents(S32 event_id)
{
	showPanel("events_panel");
	HBFloaterSearch* self = findInstance();
	if (self && self->mEventsPanel)
	{
		if (event_id != 0)
		{
			self->mEventsPanel->selectEventByID(event_id);
		}
		else
		{
			// Force a query for today's events
			self->mEventsPanel->setDay(0);
			self->mEventsPanel->performQuery();
		}
	}
}

//static
void HBFloaterSearch::showLandForSale(const LLUUID& parcel_id)
{
	showPanel("land_sales_panel");
	HBFloaterSearch* self = findInstance();
	if (self && self->mLandPanel)
	{
		self->mLandPanel->selectByUUID(parcel_id);
	}
}

//static
void HBFloaterSearch::showGroups()
{
	showPanel("groups_panel");
}

//static
void HBFloaterSearch::refreshGroup(const LLUUID& group_id)
{
	HBFloaterSearch* self = findInstance();
	if (self && self->mPanelGroupp && self->mPanelGroupp->getID() == group_id)
	{
		self->mPanelGroupp->refreshData();
	}
}

//static
void HBFloaterSearch::showPanel(const std::string& tabname)
{
	// This function gets called when web browser clicks are processed, so we
	// do not delete the existing panel, which would delete the web browser
	// instance currently handling the click. JC
	// Get current instance or create a new one if none exist yet.
	HBFloaterSearch* self = getInstance();
	if (self)	// Paranoia (could happen when out of memory)
	{
		self->open();
		self->childShowTab("Directory Tabs", tabname);
		self->focusCurrentPanel();
	}
}

//static
void HBFloaterSearch::toggle()
{
	HBFloaterSearch* self = findInstance();
	if (self)
	{
		if (self->getVisible())
		{
			self->setVisible(false);
		}
		else
		{
			self->open();
			self->focusCurrentPanel();
		}
		return;
	}

	std::string panel = gSavedSettings.getString("LastFindPanel");

	// These panels got renamed...
	if (panel == "find_all_old_panel")
	{
		panel = "find_all_panel";
	}
	if (panel == "sl_panel")
	{
		panel = "web_search_panel";
	}

	if (!gIsInSecondLife && panel == "web_search_panel" &&
		HBPanelWebSearch::getOSSearchURL().empty())
	{
		panel = "find_all_panel";
	}

	showPanel(panel);	// Creates a new instance

	// *HACK: force query for today's events
	self = findInstance();
	if (self && self->mEventsPanel)
	{
		self->mEventsPanel->setDay(0);
	}
}

//static
void HBFloaterSearch::onTeleportArriving()
{
	HBFloaterSearch* self = findInstance();
	if (self && !self->isMinimized() &&
		gSavedSettings.getBool("HideFloatersOnTPSuccess"))
	{
		self->setVisible(false);
	}
}

//static
void HBFloaterSearch::onTabChanged(void* data, bool from_click)
{
	HBFloaterSearch* self = (HBFloaterSearch*)data;
	if (!self) return;

	LLPanel* panel = self->childGetVisibleTab("Directory Tabs");
	if (panel)
	{
		gSavedSettings.setString("LastFindPanel", panel->getName());
	}
}

void HBFloaterSearch::hideAllDetailPanels()
{
	if (mPanelAvatarp)
	{
		mPanelAvatarp->setVisible(false);
	}
	if (mPanelEventp)
	{
		mPanelEventp->setVisible(false);
	}
	if (mPanelGroupp)
	{
		mPanelGroupp->setVisible(false);
	}
	if (mPanelGroupHolderp)
	{
		mPanelGroupHolderp->setVisible(false);
	}
	if (mPanelPlacep)
	{
		mPanelPlacep->setVisible(false);
	}
	if (mPanelPlaceSmallp)
	{
		mPanelPlaceSmallp->setVisible(false);
	}
	if (mPanelClassifiedp)
	{
		mPanelClassifiedp->setVisible(false);
	}
}
