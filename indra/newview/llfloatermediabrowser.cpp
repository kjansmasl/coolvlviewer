/**
 * @file llfloatermediabrowser.cpp
 * @brief Web browser floaters
 *
 * $LicenseInfo:firstyear=2006&license=viewergpl$
 *
 * Copyright (c) 2006-2009, Linden Research, Inc.
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

#include "llfloatermediabrowser.h"

#include "llcombobox.h"
#include "llhttpconstants.h"			// For HTTP_CONTENT_TEXT_HTML
#include "llpluginclassmedia.h"
#include "llsdutil.h"
#include "lltextbox.h"
#include "lluictrlfactory.h"
#include "llurlhistory.h"
#include "llwindow.h"

#include "llcommandhandler.h"
#include "llgridmanager.h"
#include "llviewercontrol.h"
#include "llviewerparcelmedia.h"
#include "llviewerparcelmgr.h"
#include "llweb.h"
#include "roles_constants.h"

// Global
LLViewerHtmlHelp gViewerHtmlHelp;

////////////////////////////////////////////////////////////////////////////////
// Command handler for secondlife:///app/help/{TOPIC} SLapps SLURL support
////////////////////////////////////////////////////////////////////////////////

// Note: TOPIC is ignored (it is pretty dumb anyway: only pre and post login
// topics are used in LL's v3 viewer). HB
class LLHelpHandler final : public LLCommandHandler
{
public:
	// Requests will be throttled from a non-trusted browser
	LLHelpHandler()
	:	LLCommandHandler("help", UNTRUSTED_THROTTLE)
	{
	}

	bool handle(const LLSD&, const LLSD&, LLMediaCtrl*) override
	{
		gViewerHtmlHelp.show();
		return true;
	}
};
LLHelpHandler gHelpHandler;

////////////////////////////////////////////////////////////////////////////////
// LLFloaterMediaBrowser class
////////////////////////////////////////////////////////////////////////////////

//static
LLFloaterMediaBrowser::instances_vec_t LLFloaterMediaBrowser::sInstances;

//static
LLFloaterMediaBrowser* LLFloaterMediaBrowser::getInstance(const LLSD& media_url)
{
	std::string url = media_url.asString();

	// Try and find a corresponding open instance
	U32 count = sInstances.size();
	for (U32 i = 0; i < count; ++i)
	{
		LLFloaterMediaBrowser* floaterp = sInstances[i];
		if (floaterp->mInitalUrl == url || floaterp->mCurrentURL == url)
		{
			return floaterp;
		}
	}

	U32 max_count = gSavedSettings.getU32("MaxBrowserInstances");
	if (max_count < 1)
	{
		max_count = 1;
		gSavedSettings.setU32("MaxBrowserInstances", 1);
	}
	if (count >= max_count)
	{
		llinfos << "Maximum Web floaters instances reached, reusing the last one."
				<< llendl;
		// Pick the last instance.
		return sInstances.back();
	}

	return new LLFloaterMediaBrowser(media_url);
}

//static
LLFloaterMediaBrowser* LLFloaterMediaBrowser::showInstance(const LLSD& media_url,
														   bool trusted)
{
	LLFloaterMediaBrowser* floaterp =
		LLFloaterMediaBrowser::getInstance(media_url);
	if (floaterp)	// Paranoia
	{
		floaterp->openMedia(media_url.asString(), trusted);
		gFloaterViewp->bringToFront(floaterp);
	}
	return floaterp;
}

LLFloaterMediaBrowser::LLFloaterMediaBrowser(const LLSD& media_url)
:	mInitalUrl(media_url.asString()),
	mBrowser(NULL),
	mParcel(NULL),
	mBackButton(NULL),
	mForwardButton(NULL),
	mReloadButton(NULL),
	mRewindButton(NULL),
	mPlayButton(NULL),
	mPauseButton(NULL),
	mStopButton(NULL),
	mSeekButton(NULL),
	mGoButton(NULL),
	mCloseButton(NULL),
	mBrowserButton(NULL),
	mAssignButton(NULL),
	mAddressCombo(NULL),
	mLoadingText(NULL)
{
	sInstances.push_back(this);
	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_media_browser.xml");
}

//virtual
LLFloaterMediaBrowser::~LLFloaterMediaBrowser()
{
	for (instances_vec_t::iterator it = sInstances.begin(),
								   end = sInstances.end();
		 it != end; ++it)
	{
		if (*it == this)
		{
			sInstances.erase(it);
			break;
		}
	}
}

bool LLFloaterMediaBrowser::postBuild()
{
	// Note: we use the "build dummy widget if missing" version of getChild<T>
	// so that all pointers are non-NULL and warnings are issued in the log
	// about missing UI elements. All the UI elements are considered mandatory.

	mBrowser = getChild<LLMediaCtrl>("browser");
	mBrowser->addObserver(this);

	mAddressCombo = getChild<LLComboBox>("address");
	mAddressCombo->setCommitCallback(onEnterAddress);
	mAddressCombo->setCallbackUserData(this);

	mBackButton = getChild<LLButton>("back");
	mBackButton->setClickedCallback(onClickBack, this);

	mForwardButton = getChild<LLButton>("forward");
	mForwardButton->setClickedCallback(onClickForward, this);

	mReloadButton = getChild<LLButton>("reload");
	mReloadButton->setClickedCallback(onClickRefresh, this);

	mRewindButton = getChild<LLButton>("rewind");
	mRewindButton->setClickedCallback(onClickRewind, this);

	mPlayButton = getChild<LLButton>("play");
	mPlayButton->setClickedCallback(onClickPlay, this);

	mPauseButton = getChild<LLButton>("pause");
	mPauseButton->setClickedCallback(onClickPlay, this);

	mStopButton = getChild<LLButton>("stop");
	mStopButton->setClickedCallback(onClickStop, this);

	mSeekButton = getChild<LLButton>("seek");
	mSeekButton->setClickedCallback(onClickSeek, this);

	mGoButton = getChild<LLButton>("go");
	mGoButton->setClickedCallback(onClickGo, this);

	mCloseButton = getChild<LLButton>("close");
	mCloseButton->setClickedCallback(onClickClose, this);

	mBrowserButton = getChild<LLButton>("open_browser");
	mBrowserButton->setClickedCallback(onClickOpenWebBrowser, this);

	mAssignButton = getChild<LLButton>("assign");
	mAssignButton->setClickedCallback(onClickAssign, this);

	mLoadingText = getChild<LLTextBox>("loading");

	buildURLHistory();

	return true;
}

void LLFloaterMediaBrowser::geometryChanged(S32 x, S32 y, S32 width,
											S32 height)
{
	// Make sure the layout of the browser control is updated, so this
	// calculation is correct.
	LLLayoutStack::updateClass();

	LLCoordWindow window_size;
	gWindowp->getSize(&window_size);

	// Adjust width and height for the size of the chrome on the Media Browser
	// window.
	width += getRect().getWidth() - mBrowser->getRect().getWidth();
	height += getRect().getHeight() - mBrowser->getRect().getHeight();

	LLRect geom;
	geom.setOriginAndSize(x, window_size.mY - (y + height), width, height);

	LL_DEBUGS("MediaBrowser") << "geometry change: " << geom << LL_ENDL;

	userSetShape(geom);
}

void LLFloaterMediaBrowser::draw()
{
	if (!mBrowser)
	{
		// There is something *very* wrong: abort
		llwarns_once << "Incomplete floater media browser !" << llendl;
		LLFloater::draw();
		return;
	}

	mBackButton->setEnabled(mBrowser->canNavigateBack());
	mForwardButton->setEnabled(mBrowser->canNavigateForward());

	mGoButton->setEnabled(!mAddressCombo->getValue().asString().empty() &&
						  // Forbid changing a trusted browser URL
						  !mBrowser->isTrusted());

	LLParcel* parcelp = gViewerParcelMgr.getAgentParcel();
	if (mParcel != parcelp)
	{
		mParcel = parcelp;
		bool can_change =
			LLViewerParcelMgr::isParcelModifiableByAgent(parcelp,
														 GP_LAND_CHANGE_MEDIA);
		mAssignButton->setVisible(can_change);
		bool not_empty = !mAddressCombo->getValue().asString().empty();
		mAssignButton->setEnabled(not_empty);
	}

	bool show_time_controls = false;
	bool media_playing = false;
	LLPluginClassMedia* pluginp = mBrowser->getMediaPlugin();
	if (pluginp)
	{
		show_time_controls = pluginp->pluginSupportsMediaTime();
		media_playing =
			pluginp->getStatus() == LLPluginClassMediaOwner::MEDIA_PLAYING;
	}

	mRewindButton->setVisible(show_time_controls);
	mPlayButton->setVisible(show_time_controls && !media_playing);
	mPlayButton->setEnabled(!media_playing);
	mPauseButton->setVisible(show_time_controls && media_playing);
	mStopButton->setVisible(show_time_controls);
	mStopButton->setEnabled(media_playing);
	mSeekButton->setVisible(show_time_controls);

	LLFloater::draw();
}

void LLFloaterMediaBrowser::buildURLHistory()
{
	mAddressCombo->operateOnAll(LLComboBox::OP_DELETE);

	// Get all of the entries in the "browser" collection
	LLSD browser_history = LLURLHistory::getURLHistory("browser");

	std::string url;
	for(LLSD::array_iterator iter_history = browser_history.beginArray(),
							 end_history = browser_history.endArray();
		iter_history != end_history; ++iter_history)
	{
		url = iter_history->asString();
		if (!url.empty())
		{
			mAddressCombo->addSimpleElement(url);
		}
	}

	// Initialize URL history in the plugin
	LLPluginClassMedia* pluginp = mBrowser->getMediaPlugin();
	if (pluginp)
	{
		pluginp->initializeUrlHistory(browser_history);
	}
}

void LLFloaterMediaBrowser::onClose(bool app_quitting)
{
	if (mBrowser)
	{
		mBrowser->remObserver(this);
		if (mBrowser->getMediaSource())
		{
			mBrowser->getMediaSource()->cancelMimeTypeProbe();
		}
	}
	destroy();
}

void LLFloaterMediaBrowser::handleMediaEvent(LLPluginClassMedia* self,
											 EMediaEvent event)
{
	if (event == MEDIA_EVENT_LOCATION_CHANGED)
	{
		setCurrentURL(self->getLocation());
		mAddressCombo->setVisible(false);
		mLoadingText->setVisible(true);
	}
	else if (event == MEDIA_EVENT_NAVIGATE_COMPLETE)
	{
		// This is the event these flags are sent with.
		mBackButton->setEnabled(self->getHistoryBackAvailable());
		mForwardButton->setEnabled(self->getHistoryForwardAvailable());
		mAddressCombo->setVisible(true);
		mLoadingText->setVisible(false);
	}
	else if (event == MEDIA_EVENT_CLOSE_REQUEST)
	{
		// The browser instance wants its window closed.
		close();
	}
	else if (event == MEDIA_EVENT_GEOMETRY_CHANGE)
	{
		geometryChanged(self->getGeometryX(), self->getGeometryY(),
						self->getGeometryWidth(), self->getGeometryHeight());
	}
}

void LLFloaterMediaBrowser::setCurrentURL(const std::string& url)
{
	mCurrentURL = url;
	// Redirects will navigate momentarily to about:blank: do not add to
	// history
	if (mCurrentURL != "about:blank")
	{
		mAddressCombo->remove(mCurrentURL);
		mAddressCombo->add(mCurrentURL, ADD_SORTED);
		mAddressCombo->selectByValue(mCurrentURL);

		// Serialize url history
		LLURLHistory::removeURL("browser", mCurrentURL);
		LLURLHistory::addURL("browser", mCurrentURL);
	}

	mBackButton->setEnabled(mBrowser->canNavigateBack());
	mForwardButton->setEnabled(mBrowser->canNavigateForward());
	mReloadButton->setEnabled(true);
}

//static
void LLFloaterMediaBrowser::onEnterAddress(LLUICtrl* ctrl, void* user_data)
{
	LLFloaterMediaBrowser* self = (LLFloaterMediaBrowser*)user_data;
	if (self)
	{
		self->mBrowser->navigateTo(self->mAddressCombo->getValue().asString());
	}
}

//static
void LLFloaterMediaBrowser::onClickRefresh(void* user_data)
{
	LLFloaterMediaBrowser* self = (LLFloaterMediaBrowser*)user_data;
	if (self)
	{
		self->mAddressCombo->remove(0);
		std::string url = self->mCurrentURL;
		// Force a reload by changing the page first
		self->mBrowser->navigateTo("about:blank");
		self->mBrowser->navigateTo(url);
	}
}

//static
void LLFloaterMediaBrowser::onClickForward(void* user_data)
{
	LLFloaterMediaBrowser* self = (LLFloaterMediaBrowser*)user_data;
	if (self)
	{
		self->mBrowser->navigateForward();
	}
}

//static
void LLFloaterMediaBrowser::onClickBack(void* user_data)
{
	LLFloaterMediaBrowser* self = (LLFloaterMediaBrowser*)user_data;
	if (self)
	{
		self->mBrowser->navigateBack();
	}
}

//static
void LLFloaterMediaBrowser::onClickGo(void* user_data)
{
	LLFloaterMediaBrowser* self = (LLFloaterMediaBrowser*)user_data;

	self->mBrowser->navigateTo(self->mAddressCombo->getValue().asString());
}

//static
void LLFloaterMediaBrowser::onClickClose(void* user_data)
{
	LLFloaterMediaBrowser* self = (LLFloaterMediaBrowser*)user_data;
	if (self)
	{
		self->close();
	}
}

//static
void LLFloaterMediaBrowser::onClickOpenWebBrowser(void* user_data)
{
	LLFloaterMediaBrowser* self = (LLFloaterMediaBrowser*)user_data;
	if (self)
	{
		// NOTE: we favour the URL in the combo box, because in case of a page
		// loading failure (SSL handshake failures, for example), mCurrentURL
		// contains about:blank or another URL than the failed page URL...
		std::string url = self->mAddressCombo->getValue().asString();
		if (url.empty())
		{
			url = self->mCurrentURL;
		}
		if (url.empty())
		{
			url = self->mBrowser->getHomePageUrl();
		}
		LLWeb::loadURLExternal(url);
	}
}

void LLFloaterMediaBrowser::onClickAssign(void* user_data)
{
	LLFloaterMediaBrowser* self = (LLFloaterMediaBrowser*)user_data;
	if (!self) return;

	LLParcel* parcel = gViewerParcelMgr.getAgentParcel();
	if (!parcel)
	{
		return;
	}

	std::string media_url = self->mAddressCombo->getValue().asString();
	LLStringUtil::trim(media_url);

	if (parcel->getMediaType() != HTTP_CONTENT_TEXT_HTML)
	{
		parcel->setMediaURL(media_url);
		parcel->setMediaCurrentURL(media_url);
		parcel->setMediaType(HTTP_CONTENT_TEXT_HTML);
		gViewerParcelMgr.sendParcelPropertiesUpdate(parcel, true);
		LLViewerParcelMedia::sendMediaNavigateMessage(media_url);
		LLViewerParcelMedia::stop();
#if 0
		LLViewerParcelMedia::update(parcel);
#endif
	}
	LLViewerParcelMedia::sendMediaNavigateMessage(media_url);
}

//static
void LLFloaterMediaBrowser::onClickRewind(void* user_data)
{
	LLFloaterMediaBrowser* self = (LLFloaterMediaBrowser*)user_data;
	if (!self) return;	// Paranoia

	LLPluginClassMedia* pluginp = self->mBrowser->getMediaPlugin();
	if (pluginp)
	{
		pluginp->start(-2.f);
	}
}

//static
void LLFloaterMediaBrowser::onClickPlay(void* user_data)
{
	LLFloaterMediaBrowser* self = (LLFloaterMediaBrowser*)user_data;
	if (!self) return;	// Paranoia

	LLPluginClassMedia* pluginp = self->mBrowser->getMediaPlugin();
	if (!pluginp) return;

	if (pluginp->getStatus() == LLPluginClassMediaOwner::MEDIA_PLAYING)
	{
		pluginp->pause();
	}
	else
	{
		pluginp->start();
	}
}

//static
void LLFloaterMediaBrowser::onClickStop(void* user_data)
{
	LLFloaterMediaBrowser* self = (LLFloaterMediaBrowser*)user_data;
	if (!self) return;	// Paranoia

	LLPluginClassMedia* pluginp = self->mBrowser->getMediaPlugin();
	if (pluginp)
	{
		pluginp->stop();
	}
}

//static
void LLFloaterMediaBrowser::onClickSeek(void* user_data)
{
	LLFloaterMediaBrowser* self = (LLFloaterMediaBrowser*)user_data;
	if (!self) return;	// Paranoia

	LLPluginClassMedia* pluginp = self->mBrowser->getMediaPlugin();
	if (pluginp)
	{
		pluginp->start(2.f);
	}
}

void LLFloaterMediaBrowser::openMedia(const std::string& media_url,
									  bool trusted)
{
	openMedia(media_url, "", trusted);
}

void LLFloaterMediaBrowser::openMedia(const std::string& media_url,
									  const std::string& target,
									  bool trusted)
{
	mBrowser->setHomePageUrl(media_url);
	mBrowser->setTarget(target);
	mBrowser->setTrusted(trusted);
	mAddressCombo->setEnabled(!trusted);
	mGoButton->setEnabled(!trusted);
	mAddressCombo->setVisible(false);
	mLoadingText->setVisible(true);
	mBrowser->navigateTo(media_url);
	setCurrentURL(media_url);
}

////////////////////////////////////////////////////////////////////////////////
// LLViewerHtmlHelp class
////////////////////////////////////////////////////////////////////////////////

LLViewerHtmlHelp::LLViewerHtmlHelp()
{
	LLUI::setHtmlHelp(this);
}

LLViewerHtmlHelp::~LLViewerHtmlHelp()
{
	LLUI::setHtmlHelp(NULL);
}

void LLViewerHtmlHelp::show()
{
	show("");
}

void LLViewerHtmlHelp::show(std::string url)
{
	if (url.empty())
	{
		url = LLGridManager::getInstance()->getSupportURL();
	}

	if (gSavedSettings.getBool("UseExternalBrowser"))
	{
		LLSD data;
		data["url"] = url;

		gNotifications.add("ClickOpenF1Help", data, LLSD(),
						   onClickF1HelpLoadURL);
		return;
	}

	LLFloaterMediaBrowser* floaterp = LLFloaterMediaBrowser::getInstance(url);
	if (floaterp)
	{
		floaterp->setVisible(true);
		floaterp->openMedia(url);
	}
}

//static
bool LLViewerHtmlHelp::onClickF1HelpLoadURL(const LLSD& notification,
											const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		LLWeb::loadURL(LLGridManager::getInstance()->getSupportURL());
	}
	return false;
}
