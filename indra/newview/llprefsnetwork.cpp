/**
 * @file llprefsnetwork.cpp
 * @brief Network preferences panel
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

#include "llviewerprecompiledheaders.h"

#include "cef/dullahan.h"			// For CHROME_VERSION_MAJOR

#include "llprefsnetwork.h"

#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "lldir.h"
#include "hbfileselector.h"
#include "llpluginclassmedia.h"
#include "llradiogroup.h"
#include "lluictrlfactory.h"

#include "llappviewer.h"			// For isSecondInstanceSiblingViewer()
#include "llgridmanager.h"
#include "llstartup.h"
#include "lltexturefetch.h"
#include "llviewercontrol.h"
#include "llviewermedia.h"

// Static members
LLPrefsNetwork* LLPrefsNetwork::sInstance = NULL;
bool LLPrefsNetwork::sSocksSettingsChanged = false;

LLPrefsNetwork::LLPrefsNetwork()
{
	LLUICtrlFactory* factoryp = LLUICtrlFactory::getInstance();
	factoryp->buildPanel(this, "panel_preferences_network.xml");
	sInstance = this;
}

//virtual
LLPrefsNetwork::~LLPrefsNetwork()
{
	sInstance = NULL;
}

//virtual
bool LLPrefsNetwork::postBuild()
{
	bool logged_in_sl = LLStartUp::isLoggedIn() && gIsInSecondLife;
	bool logged_in_os = LLStartUp::isLoggedIn() && !gIsInSecondLife;
	bool enabled = logged_in_sl ||
				   gSavedSettings.getBool("ImagePipelineUseHTTP");
	// Network connection port, fetch concurrencies and bandwidth
	childSetValue("http_texture_fetch", enabled || gIsInSecondLife);
	childSetEnabled("http_texture_fetch", !logged_in_sl);
	childSetCommitCallback("http_texture_fetch", onHttpTextureFetchToggled,
						   this);

	U32 max_requests =
		llclamp(gSavedSettings.getU32("TextureFetchConcurrency"), 2, 32);
	childSetValue("max_texture_http_concurrency", (F32)max_requests);
	childSetEnabled("max_texture_http_concurrency", enabled);

	max_requests =
		llclamp(gSavedSettings.getU32("MeshMaxConcurrentRequests"), 2, 128);
	childSetValue("max_mesh_http_concurrency", (F32)max_requests);

	max_requests =
		llclamp(gSavedSettings.getU32("Mesh2MaxConcurrentRequests"), 2, 32);
	childSetValue("max_mesh2_http_concurrency", (F32)max_requests);

	childSetValue("sl_http_pipelining_check",
				  gSavedSettings.getBool("HttpPipeliningSL"));
	childSetEnabled("sl_http_pipelining_check", !logged_in_os);
	childSetValue("os_http_pipelining_check",
				  gSavedSettings.getBool("HttpPipeliningOS"));
	childSetEnabled("os_http_pipelining_check", !logged_in_sl);

	enabled = gSavedSettings.getBool("ConnectionPortEnabled");
	childSetEnabled("connection_port", enabled);
	childSetValue("connection_port_enabled", enabled);
	childSetCommitCallback("connection_port_enabled", onCommitPort, this);
	childSetValue("max_bandwidth",
				  S32(gSavedSettings.getU32("ThrottleBandwidthKbps")));
	childSetValue("connection_port",
				  (F32)gSavedSettings.getU32("ConnectionPort"));

	// Cache settings (disabled when cache writes are disabled).
	bool can_write_caches = gAppViewerp &&
							!gAppViewerp->isSecondInstanceSiblingViewer();
	childSetText("cache_path", gDirUtilp->getCacheDir());
	childSetEnabled("cache_path", can_write_caches);
	childSetAction("clear_disk_cache", onClickClearDiskCache, this);
	childSetEnabled("clear_disk_cache", can_write_caches);
	mSetCacheButton = getChild<LLButton>("set_cache");
	mSetCacheButton->setClickedCallback(onClickSetCache, this);
	mSetCacheButton->setEnabled(can_write_caches);
	childSetAction("reset_cache", onClickResetCache, this);
	childSetEnabled("reset_cache", can_write_caches);
	childSetValue("cache_size", (F32)gSavedSettings.getU32("CacheSize"));
	childSetEnabled("cache_size", can_write_caches);

	// Browser settings
	childSetAction("clear_browser_cache", onClickClearBrowserCache, this);
	childSetAction("clear_cookies", onClickClearCookies, this);
	childSetCommitCallback("web_proxy_enabled", onCommitWebProxyEnabled, this);

	std::string value =
		gSavedSettings.getBool("UseExternalBrowser") ? "external" : "internal";
	childSetValue("use_external_browser", value);

	childSetValue("cookies_enabled", gSavedSettings.getBool("CookiesEnabled"));
	childSetValue("javascript_enabled",
				  gSavedSettings.getBool("BrowserJavascriptEnabled"));
	// Plugins support has been entirely gutted out from CEF 100
#if CHROME_VERSION_MAJOR < 100
	childSetValue("plugins_enabled",
				  gSavedSettings.getBool("BrowserPluginsEnabled"));
#else
	childSetVisible("plugins_enabled", false);
#endif

	// Web Proxy settings
	enabled = gSavedSettings.getBool("BrowserProxyEnabled");
	childSetValue("web_proxy_enabled", enabled);
	childSetEnabled("proxy_text_label", enabled);
	childSetEnabled("web_proxy_editor", enabled);
	childSetEnabled("web_proxy_port", enabled);
	childSetEnabled("Web", enabled);

	childSetValue("web_proxy_editor",
				  gSavedSettings.getString("BrowserProxyAddress"));
	childSetValue("web_proxy_port", gSavedSettings.getS32("BrowserProxyPort"));

	// Socks 5 proxy settings, commit callbacks
	childSetCommitCallback("socks5_proxy_enabled", onCommitSocks5ProxyEnabled,
						   this);
	childSetCommitCallback("socks5_auth", onSocksAuthChanged, this);

	// Socks 5 proxy settings, saved data
	enabled = gSavedSettings.getBool("Socks5ProxyEnabled");
	childSetValue("socks5_proxy_enabled", enabled);

	childSetValue("socks5_proxy_host",
				  gSavedSettings.getString("Socks5ProxyHost"));
	childSetValue("socks5_proxy_port",
				  (F32)gSavedSettings.getU32("Socks5ProxyPort"));
	childSetValue("socks5_proxy_username",
				  gSavedSettings.getString("Socks5Username"));
	childSetValue("socks5_proxy_password",
				  gSavedSettings.getString("Socks5Password"));
	std::string auth_type = gSavedSettings.getString("Socks5AuthType");
	childSetValue("socks5_auth", auth_type);

	// Other HTTP connections proxy setting
	childSetValue("http_proxy_type",
				  gSavedSettings.getString("HttpProxyType"));

	// Socks 5 proxy settings, check if settings modified callbacks
	childSetCommitCallback("socks5_proxy_host", onSocksSettingsModified, this);
	childSetCommitCallback("socks5_proxy_port", onSocksSettingsModified, this);
	childSetCommitCallback("socks5_proxy_username", onSocksSettingsModified,
						   this);
	childSetCommitCallback("socks5_proxy_password", onSocksSettingsModified,
						   this);

	// Socks 5 settings, Set all controls and labels enabled state
	updateProxyEnabled(this, enabled, auth_type);

	sSocksSettingsChanged = false;

	return true;
}

//virtual
void LLPrefsNetwork::draw()
{
	bool can_write_caches = gAppViewerp &&
							!gAppViewerp->isSecondInstanceSiblingViewer();
	mSetCacheButton->setEnabled(can_write_caches && !HBFileSelector::isInUse());
	LLPanel::draw();
}

void sendMediaSettings()
{
	LLViewerMedia::setCookiesEnabled(gSavedSettings.getBool("CookiesEnabled"));
	LLViewerMedia::setProxyConfig(gSavedSettings.getBool("BrowserProxyEnabled"),
								  gSavedSettings.getString("BrowserProxyAddress"),
								  gSavedSettings.getS32("BrowserProxyPort"));
}

void LLPrefsNetwork::apply()
{
	if (!gIsInSecondLife || !LLStartUp::isLoggedIn())
	{
		gSavedSettings.setBool("ImagePipelineUseHTTP",
							   childGetValue("http_texture_fetch"));
	}
	gSavedSettings.setU32("TextureFetchConcurrency",
						  childGetValue("max_texture_http_concurrency").asInteger());

	gSavedSettings.setU32("MeshMaxConcurrentRequests",
						  childGetValue("max_mesh_http_concurrency").asInteger());

	gSavedSettings.setU32("Mesh2MaxConcurrentRequests",
						  childGetValue("max_mesh2_http_concurrency").asInteger());

	gSavedSettings.setBool("HttpPipeliningSL",
						   childGetValue("sl_http_pipelining_check"));
	gSavedSettings.setBool("HttpPipeliningOS",
						   childGetValue("os_http_pipelining_check"));

	U32 cache_size = (U32)childGetValue("cache_size").asInteger();
	if (gSavedSettings.getU32("CacheSize") != cache_size)
	{
		onClickClearDiskCache(this);
		gSavedSettings.setU32("CacheSize", cache_size);
	}
	gSavedSettings.setU32("ThrottleBandwidthKbps",
						  childGetValue("max_bandwidth").asInteger());
	gSavedSettings.setBool("ConnectionPortEnabled",
						   childGetValue("connection_port_enabled"));
	gSavedSettings.setU32("ConnectionPort",
						  childGetValue("connection_port").asInteger());

	gSavedSettings.setBool("Socks5ProxyEnabled",
						   childGetValue("socks5_proxy_enabled"));
	gSavedSettings.setString("Socks5ProxyHost",
							 childGetValue("socks5_proxy_host"));
	gSavedSettings.setU32("Socks5ProxyPort",
						  childGetValue("socks5_proxy_port").asInteger());

	gSavedSettings.setString("Socks5AuthType",
							 childGetValue("socks5_auth"));
	gSavedSettings.setString("Socks5Username",
							 childGetValue("socks5_proxy_username"));
	gSavedSettings.setString("Socks5Password",
							 childGetValue("socks5_proxy_password"));

	gSavedSettings.setBool("CookiesEnabled",
						   childGetValue("cookies_enabled"));
	gSavedSettings.setBool("BrowserJavascriptEnabled",
						   childGetValue("javascript_enabled"));
#if CHROME_VERSION_MAJOR < 100
	gSavedSettings.setBool("BrowserPluginsEnabled",
						   childGetValue("plugins_enabled"));
#endif
	gSavedSettings.setBool("BrowserProxyEnabled",
						   childGetValue("web_proxy_enabled"));
	gSavedSettings.setString("BrowserProxyAddress",
							 childGetValue("web_proxy_editor"));
	gSavedSettings.setS32("BrowserProxyPort",
						  childGetValue("web_proxy_port"));

	gSavedSettings.setString("HttpProxyType",
							 childGetValue("http_proxy_type"));

	bool value =
		childGetValue("use_external_browser").asString() == "external";
	gSavedSettings.setBool("UseExternalBrowser", value);

	sendMediaSettings();

	if (sSocksSettingsChanged &&
		LLStartUp::getStartupState() != STATE_LOGIN_WAIT)
	{
		gNotifications.add("ProxyNeedRestart");
		sSocksSettingsChanged = false;
	}
}

void LLPrefsNetwork::cancel()
{
	sendMediaSettings();
}

// static
void LLPrefsNetwork::onHttpTextureFetchToggled(LLUICtrl* ctrl, void* data)
{
	LLPrefsNetwork* self = (LLPrefsNetwork*)data;
	LLCheckBoxCtrl* check = (LLCheckBoxCtrl*)ctrl;
	if (self && check)
	{
		self->childSetEnabled("max_texture_http_concurrency", check->get());
	}
}

// static
void LLPrefsNetwork::onClickClearDiskCache(void*)
{
	// Flag client cache for clearing next time the client runs
	gSavedSettings.setBool("PurgeCacheOnNextStartup", true);
	gNotifications.add("CachesWillClear");
}

// static
void LLPrefsNetwork::setCacheCallback(std::string& dir_name, void* data)
{
	LLPrefsNetwork* self = (LLPrefsNetwork*)data;
	if (!self || self != sInstance)
	{
		gNotifications.add("PreferencesClosed");
		return;
	}
	std::string cur_name = gSavedSettings.getString("CacheLocation");
	if (!dir_name.empty() && dir_name != cur_name)
	{
		self->childSetText("cache_path", dir_name);
		gNotifications.add("CacheWillBeMoved");
		gSavedSettings.setString("NewCacheLocation", dir_name);
	}
}

// static
void LLPrefsNetwork::onClickSetCache(void* data)
{
	std::string suggestion = gDirUtilp->getExpandedFilename(LL_PATH_CACHE, "");
	HBFileSelector::pickDirectory(suggestion, setCacheCallback, data);
}

// static
void LLPrefsNetwork::onClickResetCache(void* data)
{
 	LLPrefsNetwork* self = (LLPrefsNetwork*)data;
	if (!gSavedSettings.getString("CacheLocation").empty())
	{
		gSavedSettings.setString("NewCacheLocation", "");
		gNotifications.add("CacheWillBeMoved");
	}
	self->childSetText("cache_path", gDirUtilp->getCacheDir(true));
}

// static
void LLPrefsNetwork::onCommitPort(LLUICtrl* ctrl, void* data)
{
  LLPrefsNetwork* self = (LLPrefsNetwork*)data;
  LLCheckBoxCtrl* check = (LLCheckBoxCtrl*)ctrl;

  if (!self || !check) return;
  self->childSetEnabled("connection_port", check->get());
  gNotifications.add("ChangeConnectionPort");
}

// static
void LLPrefsNetwork::onCommitSocks5ProxyEnabled(LLUICtrl* ctrl, void* data)
{
	LLPrefsNetwork* self  = (LLPrefsNetwork*)data;
	LLCheckBoxCtrl* check = (LLCheckBoxCtrl*)ctrl;

	if (!self || !check) return;

	sSocksSettingsChanged = true;

	updateProxyEnabled(self, check->get(), self->childGetValue("socks5_auth"));
}

// static
void LLPrefsNetwork::onSocksSettingsModified(LLUICtrl* ctrl, void* data)
{
	sSocksSettingsChanged = true;
}

// static
void LLPrefsNetwork::onSocksAuthChanged(LLUICtrl* ctrl, void* data)
{
	LLRadioGroup* radio  = static_cast<LLRadioGroup*>(ctrl);
	LLPrefsNetwork* self = static_cast<LLPrefsNetwork*>(data);

	sSocksSettingsChanged = true;

	std::string selection = radio->getValue().asString();
	updateProxyEnabled(self, self->childGetValue("socks5_proxy_enabled"),
					   selection);
}

// static
void LLPrefsNetwork::updateProxyEnabled(LLPrefsNetwork* self, bool enabled,
										std::string authtype)
{
	// Manage all the enable/disable of the socks5 options from this single function
	// to avoid code duplication

	// Update all socks labels and controls except auth specific ones
	self->childSetEnabled("socks5_proxy_port",  enabled);
	self->childSetEnabled("socks5_proxy_host",  enabled);
	self->childSetEnabled("socks5_host_label",  enabled);
	self->childSetEnabled("socks5_proxy_port",  enabled);
	self->childSetEnabled("socks5_auth",        enabled);

	if (!enabled && self->childGetValue("http_proxy_type").asString() == "Socks")
	{
		self->childSetValue("http_proxy_type", "None");
	}
	self->childSetEnabled("Socks", enabled);

	// Hide the auth specific lables if authtype is none or
	// we are not enabled.
	if (!enabled || authtype.compare("None") == 0)
	{
		self->childSetEnabled("socks5_username_label", false);
		self->childSetEnabled("socks5_password_label", false);
		self->childSetEnabled("socks5_proxy_username", false);
		self->childSetEnabled("socks5_proxy_password", false);
	}

	// Only show the username and password boxes if we are enabled
	// and authtype is username pasword.
	if (enabled && authtype.compare("UserPass") == 0)
	{
		self->childSetEnabled("socks5_username_label", true);
		self->childSetEnabled("socks5_password_label", true);
		self->childSetEnabled("socks5_proxy_username", true);
		self->childSetEnabled("socks5_proxy_password", true);
	}
}

// static
void LLPrefsNetwork::onClickClearBrowserCache(void*)
{
	gNotifications.add("ConfirmClearBrowserCache", LLSD(), LLSD(),
					   callback_clear_browser_cache);
}

//static
bool LLPrefsNetwork::callback_clear_browser_cache(const LLSD& notification,
												  const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) == 0) // YES
	{
		LLViewerMedia::clearAllCaches();
	}
	return false;
}

// static
void LLPrefsNetwork::onClickClearCookies(void*)
{
	gNotifications.add("ConfirmClearCookies", LLSD(), LLSD(),
					   callback_clear_cookies);
}

//static
bool LLPrefsNetwork::callback_clear_cookies(const LLSD& notification,
											const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) == 0) // YES
	{
		LLViewerMedia::clearAllCookies();
	}
	return false;
}

// static
void LLPrefsNetwork::onCommitWebProxyEnabled(LLUICtrl* ctrl, void* data)
{
	LLPrefsNetwork* self = (LLPrefsNetwork*)data;
	LLCheckBoxCtrl* check = (LLCheckBoxCtrl*)ctrl;
	if (!self || !check) return;

	bool enabled = check->get();
	self->childSetEnabled("web_proxy_editor", enabled);
	self->childSetEnabled("web_proxy_port", enabled);
	self->childSetEnabled("proxy_text_label", enabled);
	self->childSetEnabled("Web", enabled);
	if (!enabled && self->childGetValue("http_proxy_type").asString() == "Web")
	{
		self->childSetValue("http_proxy_type", "None");
	}
}
