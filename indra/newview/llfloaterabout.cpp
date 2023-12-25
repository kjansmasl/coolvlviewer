/**
 * @file llfloaterabout.cpp
 * @author James Cook
 * @brief The about box from Help->About
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

#include <sstream>

#if LL_JEMALLOC
# include "jemalloc/jemalloc.h"
#elif LL_MIMALLOC
# include "mimalloc/mimalloc.h"			// For MI_MALLOC_VERSION
#endif

#include "llfloaterabout.h"

#include "llaudioengine.h"
#include "llcorehttputil.h"
#include "llimagej2c.h"
#include "llpluginprocessparent.h"
#include "llsdserialize.h"
#include "llsys.h"
#include "lltexteditor.h"
#include "lltrans.h"
#include "lluictrlfactory.h"
#include "lluri.h"
#include "llversionviewer.h"
#include "llwindow.h"
#if LL_LINUX
# include "llwindowsdl.h"				// For gXlibThreadSafe and gXWayland
#elif LL_WINDOWS
# include "lldxhardware.h"				// For gDXHardware
#endif

#include "llagent.h"
#include "llappviewer.h"
#include "llgridmanager.h"
#include "llmediactrl.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llviewercontrol.h"
#include "llviewerregion.h"
#include "llviewerstats.h"
#include "llweb.h"

#if LL_WINDOWS
std::string gDriverVersionInfo;
#endif

LLFloaterAbout::LLFloaterAbout(const LLSD&)
{
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_about.xml");
}

//virtual
bool LLFloaterAbout::postBuild()
{
	center();

	childSetAction("copy_button", onClickCopyToClipboard, this);
	childSetAction("close_button", onClickClose, this);

	mSupportTextEditor = getChild<LLTextEditor>("support");

	LLTextEditor* text = getChild<LLTextEditor>("credits");
	text->setCursorPos(0);
	text->setEnabled(false);
	text->setHandleEditKeysDirectly(true);

	text = getChild<LLTextEditor>("licenses");

	// Build-specific copyrights
#if LL_LINUX
	text->replaceTextAll("elfio", getString("elfio"), false);
	text->replaceTextAll("fontconfig", getString("fontconfig"), false);
	text->replaceTextAll("libglib", getString("libglib"), false);
	text->replaceTextAll("libsdl", getString("libsdl"), false);
#else
	text->replaceTextAll("elfio\n", "", false);
	text->replaceTextAll("fontconfig\n", "", false);
	text->replaceTextAll("libglib\n", "", false);
	text->replaceTextAll("libsdl\n", "", false);
#endif
#if LL_JEMALLOC
	text->replaceTextAll("jemalloc", getString("jemalloc"), false);
#else
	text->replaceTextAll("jemalloc\n", "", false);
#endif
#if LL_MIMALLOC
	text->replaceTextAll("mimalloc", getString("mimalloc"), false);
#else
	text->replaceTextAll("mimalloc\n", "", false);
#endif
#if LL_NGHTTP2
	text->replaceTextAll("nghttp2", getString("nghttp2"), false);
#else
	text->replaceTextAll("nghttp2\n", "", false);
#endif
#if LL_FMOD
	text->replaceTextAll("fmod", getString("fmodstudio"), false);
#else
	text->replaceTextAll("fmod\n", "", false);
#endif
#if LL_OPENAL
	text->replaceTextAll("openal", getString("openal"), false);
#else
	text->replaceTextAll("openal\n", "", false);
#endif
#if LL_NO_PHMAP
	text->replaceTextAll("phmap\n", "", false);
#else
	text->replaceTextAll("phmap", getString("phmap"), false);
#endif
#if SSE2NEON
	text->replaceTextAll("sse2neon", getString("sse2neon"), false);
#else
	text->replaceTextAll("sse2neon\n", "", false);
#endif
#if TRACY_ENABLE
	text->replaceTextAll("tracy", getString("tracy"), false);
#else
	text->replaceTextAll("tracy\n", "", false);
#endif

	// Plugins specific copyrights, based on the actual presence of the
	// corresponding plugin in the viewer distribution.
	if (gHasGstreamer)
	{
		text->replaceTextAll("gstreamer", getString("gstreamer"), false);
	}
	else
	{
		text->replaceTextAll("gstreamer\n", "", false);
	}

	text->setCursorPos(0);
	text->setEnabled(false);
	text->setHandleEditKeysDirectly(true);

	LLMediaCtrl* web_browser = getChild<LLMediaCtrl>("tos");
	if (web_browser)
	{
		web_browser->navigateToLocalPage("tpv", "policy.html");
	}

	LLViewerRegion* regionp = gAgent.getRegion();
	if (regionp)
	{
		const std::string& url = regionp->getCapability("ServerReleaseNotes");
		if (!url.empty())
		{
			if (url.find("/cap/") != std::string::npos)
			{
				// The URL is itself a capability URL: start fetching the
				// actual server release notes URL
				LL_DEBUGS("About") << "Fetching release notes URL from cap: "
								   << url << LL_ENDL;
				mServerReleaseNotesUrl = LLTrans::getString("RetrievingData");
				startFetchServerReleaseNotes(url);
			}
			else
			{
				// On OpenSim grids, we could still get a direct URL
				LL_DEBUGS("About") << "Got release notes URL: " << url
								   << LL_ENDL;
				mServerReleaseNotesUrl = url;
			}
		}
	}

	// Note: the media browser should load at least once as the About floater
	// is opened (thanks to the Usage policy tab), so the media browser version
	// will get refreshed at some point, which is checked in the draw() method.
	mLastBrowserVersion = LLPluginProcessParent::getMediaBrowserVersion();
	if (mLastBrowserVersion.empty())	// This should never happen...
	{
		mLastBrowserVersion = LLTrans::getString("LoadingData");
	}

#if LL_WINDOWS
	if (gDriverVersionInfo.empty())
	{
		LLSD driver_info = gDXHardware.getDisplayInfo();
		if (driver_info.has("DriverVersion"))
		{
			gDriverVersionInfo = driver_info["DriverVersion"].asString();
		}
	}
#endif

	setSupportText();

	return true;
}

//virtual
void LLFloaterAbout::draw()
{
	if (mLastBrowserVersion != LLPluginProcessParent::getMediaBrowserVersion())
	{
		mLastBrowserVersion = LLPluginProcessParent::getMediaBrowserVersion();
		setSupportText();
	}
	LLFloater::draw();
}

void LLFloaterAbout::updateServerReleaseNotesURL(const std::string& url)
{
	mServerReleaseNotesUrl = url;
	setSupportText();
}

void LLFloaterAbout::setSupportText()
{
	mSupportTextEditor->clear();
	mSupportTextEditor->setParseHTML(true);

	// Text styles for release notes hyperlinks
	LLStyleSP viewer_link_style(new LLStyle);
	viewer_link_style->setVisible(true);
	viewer_link_style->setFontName(LLStringUtil::null);
	viewer_link_style->setLinkHREF(getString("rel_notes"));
	viewer_link_style->setColor(gSavedSettings.getColor4("HTMLLinkColor"));

	// Version string
	std::string text = gSecondLife;
#if LL_DEBUG || LL_NO_FORCE_INLINE
	text += " [DEVEL]";
#endif
	text += llformat(" v%d.%d.%d.%d, %s %s", LL_VERSION_MAJOR,
					 LL_VERSION_MINOR, LL_VERSION_BRANCH, LL_VERSION_RELEASE,
					 __DATE__, __TIME__) + "\n";
	LLUIString ui_str;
	std::string channel = gSavedSettings.getString("VersionChannelName");
	if (channel != gSecondLife)
	{
		ui_str = getString("channel");
		ui_str.setArg("[CHANNEL]", channel);
		text.append(ui_str);
		text += '\n';
	}
//MK
	if (gRLenabled)
	{
		text += gRLInterface.getVersion2() + "\n";
	}
//mk

	const LLColor4& fg_color = LLUI::sTextFgReadOnlyColor;
	mSupportTextEditor->appendColoredText(text, false, false, fg_color);
	mSupportTextEditor->appendStyledText(getString("ReleaseNotes"), false,
										 false, viewer_link_style);

	text = "\n\n";
	// Position
	LLViewerRegion* regionp = gAgent.getRegion();
	if (regionp)
	{
		LLStyleSP server_link_style(new LLStyle);
		if (mServerReleaseNotesUrl.find("http") == 0)
		{
			server_link_style->setVisible(true);
			server_link_style->setFontName(LLStringUtil::null);
			server_link_style->setLinkHREF(mServerReleaseNotesUrl);
			server_link_style->setColor(gSavedSettings.getColor4("HTMLLinkColor"));
		}

		ui_str = getString("you_are_at");
//MK
		if (gRLenabled && gRLInterface.mContainsShowloc)
		{
			ui_str.setArg("[POSITION]",
						  LLTrans::getString("position_hidden").c_str());
			ui_str.setArg("[REGION]",
						  LLTrans::getString("region_hidden").c_str());
		}
		else
//mk
		{
			const LLVector3d& pos = gAgent.getPositionGlobal();
			ui_str.setArg("[POSITION]",
						  llformat("%.1f, %.1f, %.1f ",
								   pos.mdV[VX], pos.mdV[VY], pos.mdV[VZ]));
			ui_str.setArg("[REGION]", regionp->getName().c_str());
		}
		text.append(ui_str);
		text += '\n';

//MK
		if (gRLenabled && gRLInterface.mContainsShowloc)
		{
			text += getString("server_info_hiddden") + "\n";
		}
		else
//mk
		{
			const LLHost& host = regionp->getHost();
			std::string hostname = regionp->getSimHostName();
			text += hostname + " (" + host.getIPandPort() + ")\n";
			if (hostname != host.getHostName())
			{
				text += "Alias: " + host.getHostName() + "\n";
			}
			text += gLastVersionChannel + "\n";
		}

		mSupportTextEditor->appendColoredText(text, false, false, fg_color);

		if (!mServerReleaseNotesUrl.empty())
		{
			if (mServerReleaseNotesUrl.find("http") == 0)
			{
				text = getString("ReleaseNotes") + "\n";
				mSupportTextEditor->appendStyledText(text, false, false,
													 server_link_style);
			}
			else
			{
				text = getString("ReleaseNotes") + ": " +
					   mServerReleaseNotesUrl + "\n";
				mSupportTextEditor->appendColoredText(text, false, false,
													  fg_color);
			}
		}
	}
	else
	{
		mSupportTextEditor->appendColoredText(" \n", false, false, fg_color);
	}

	// *NOTE: Do not translate text like GPU, Graphics Card, etc; most PC users
	// that know what these mean will be used to the english versions and this
	// info sometimes gets sent to support.

	// CPU
	text = "CPU: ";
	text += LLCPUInfo::getInstance()->getCPUString(true) + "\n";

	// Moved hack adjustment to Windows memory size into llsys.cpp
	U32 memory = LLMemory::getPhysicalMemoryKB() / 1024;
	ui_str = getString("memory");
	ui_str.setArg("[AMOUNT]", llformat("%d", memory));
	text.append(ui_str);
	text += '\n';

	ui_str = getString("os_version");
	ui_str.setArg("[VERSION]", LLOSInfo::getInstance()->getOSString());
	text.append(ui_str);
	text += '\n';

#if LL_JEMALLOC
	std::string manager;
	{
		std::string git;
		const char* version;
		size_t i = sizeof(version);
		mallctl("version", &version, &i, NULL, 0);
		manager.assign(version);
		i = manager.find("-g");
		if (i != std::string::npos)
		{
			git = "-" + manager.substr(i + 2);
			if (git.length() > 9)
			{
				git = git.substr(0, 9);
			}
		}
		i = manager.find('-');
		if (i != std::string::npos)
		{
			manager = manager.substr(0, i);
		}
		manager = "jemalloc v" + manager + git;
	}
#elif LL_MIMALLOC
	std::string manager = llformat("mimalloc v%.2f",
								   F32(MI_MALLOC_VERSION) / 100.f);
#else
	std::string manager = getString("native_manager");
#endif
	ui_str = getString("memory_manager");
	ui_str.setArg("[VERSION]", manager);
	text.append(ui_str);
	text += '\n';

	ui_str = getString("graphics_card");
	ui_str.setArg("[MODEL]", gGLManager.mGLRenderer);
	text.append(ui_str);
	text += '\n';

#if LL_WINDOWS
	ui_str = getString("windows_graphics");
	if (gDriverVersionInfo.empty())
	{
		ui_str.setArg("[VERSION]", LLTrans::getString("LoadingData"));
	}
	else
	{
		ui_str.setArg("[VERSION]", gDriverVersionInfo);
	}
	text.append(ui_str);
	text += '\n';
#endif

	ui_str = getString("opengl_version");
	ui_str.setArg("[VERSION]", gGLManager.mGLVersionString);
	text.append(ui_str);
	text += '\n';

	ui_str = getString("vram");
	ui_str.setArg("[AMOUNT]", llformat("%d", gGLManager.mVRAM));
	text.append(ui_str);
	text += '\n';

#if LL_LINUX
	if (!gXlibThreadSafe)
	{
		ui_str = getString("xlib-not-threaded");
		text.append(ui_str);
		text += '\n';
	}
	if (gXWayland)
	{
		ui_str = getString("xwayland");
		text.append(ui_str);
		text += '\n';
	}
#endif

	ui_str = getString("j2c_decoder");
	ui_str.setArg("[VERSION]", LLImageJ2C::getEngineInfo());
	text.append(ui_str);
	text += '\n';

	ui_str = getString("audio_driver");
	if (gAudiop)
	{
		ui_str.setArg("[VERSION]", gAudiop->getDriverName(true));
	}
	else
	{
		ui_str.setArg("[VERSION]", getString("none"));
	}
	text.append(ui_str);
	text += '\n';

	ui_str = getString("networking");
	ui_str.setArg("[VERSION]", LLCore::LLHttp::getCURLVersion());
	text.append(ui_str);
	text += '\n';

	ui_str = getString("browser");
	ui_str.setArg("[VERSION]", mLastBrowserVersion);
	text.append(ui_str);
	text += '\n';

	if (gPacketsIn > 0)
	{
		ui_str = getString("packets_loss");
		ui_str.setArg("[STATS]",
					  llformat("%d/%d (%.1f%%)",
							   (S32)gViewerStats.mPacketsLostStat.getCurrent(),
							   gPacketsIn,
							   100.f *
							   gViewerStats.mPacketsLostStat.getCurrent() /
							   (F32)gPacketsIn));
		text.append(ui_str);
		text += '\n';
	}

	text += '\n';

	ui_str = getString("compiler");
#if LL_MSVC
	ui_str.setArg("[COMPILER]", llformat("MSVC v%d", _MSC_VER));
#elif LL_CLANG
	ui_str.setArg("[COMPILER]",
				  llformat("Clang/LLVM v%d.%d.%d", __clang_major__,
						   __clang_minor__, __clang_patchlevel__));
#elif LL_GNUC
	ui_str.setArg("[COMPILER]",
				  llformat("GCC v%d.%d.%d", __GNUC__, __GNUC_MINOR__,
						   __GNUC_PATCHLEVEL__));
#else
	ui_str.setArg("[COMPILER]", LLTrans::getString("unknown"));
#endif
	text.append(ui_str);
	text += '\n';

	ui_str = getString("maths");
	LLStringUtil::format_map_t args;
#if SSE2NEON
	ui_str.setArg("[MATHS]", "NEON");
#elif defined(__AVX2__)
	ui_str.setArg("[MATHS]", "AVX2");
#elif defined(__AVX__)
	ui_str.setArg("[MATHS]", "AVX");
#elif defined(__SSE4_1__)
	ui_str.setArg("[MATHS]", "SSE4.1");
#elif defined(__SSE3__)
	ui_str.setArg("[MATHS]", "SSE3");
#elif defined(__SSE2__)
	ui_str.setArg("[MATHS]", "SSE2");
#elif defined(__SSE__)
	ui_str.setArg("[MATHS]", "SSE");
#else
	ui_str.setArg("[MATHS]", LLTrans::getString("unknown"));
#endif
	text.append(ui_str);
	text += "\n\n";

	text += getString("compile_flags") + "\n";

	// *HACK: to get around unwanted macro expansions (due to compile flags
	// containing "errno" in their name) caused by the hack below:
#ifdef errno
# undef errno
#endif
	// *HACK: for string quotation issues in macros:
#define make_string2(x) #x
#define make_string(s) make_string2(s)
	// The double parenthesis prevents issues in macro expansion when the
	// compile flags contain commas (such as with: -Wl,some-linker-option).
#define FLAGS make_string((LL_COMPILE_FLAGS))
	std::string flags = FLAGS;
	// Remove the parenthesis enclosing the flags:
	LLStringUtil::replaceString(flags, "(", "");
	LLStringUtil::replaceString(flags, ")", "");
	// Remove the double, double-quotes enclosing the flags:
	LLStringUtil::replaceString(flags, "\"\"", "");

	// Remove the irrelevant (code-generation-wise) warning-related flags.
	size_t i;
#if LL_WINDOWS
	// Note: clang may be used for Windows builds, so we must check for both
	// types of compiler command line formats...
	while ((i = flags.find("-W")) != std::string::npos ||
		   (i = flags.find("/W")) != std::string::npos)
#else
	while ((i = flags.find("-W")) != std::string::npos)
#endif
	{
		size_t j = flags.find(' ', i);
		if (j > i)
		{
			flags = flags.erase(i, j - i);
		}
	}

	// Remove double-spaces introduced by above flags editing.
	LLStringUtil::replaceString(flags, "  ", " ");

	text += flags + "\n";

	mSupportTextEditor->appendColoredText(text, false, true, fg_color);

	// Fix views
	mSupportTextEditor->setCursorPos(0);
	mSupportTextEditor->setEnabled(false);
	mSupportTextEditor->setHandleEditKeysDirectly(true);
}

//static
void LLFloaterAbout::onClickCopyToClipboard(void* userdata)
{
	LLFloaterAbout* self = (LLFloaterAbout*)userdata;
	if (self)
	{
		self->mSupportTextEditor->selectAll();
		self->mSupportTextEditor->copy();
		self->mSupportTextEditor->deselect();
	}
}

//static
void LLFloaterAbout::onClickClose(void* userdata)
{
	LLFloaterAbout* self = (LLFloaterAbout*)userdata;
	if (self)
	{
		self->close();
	}
}

// Try to build a hard-coded release note URL for the SL Wiki...
//static
std::string LLFloaterAbout::getHardCodedURL()
{
	std::string url;
	if (gIsInSecondLifeProductionGrid)
	{
		// For the SL main grid, gLastVersionChannel should be in the form:
		// "Second Life Server YYYY-MM-DD.build". We extract "YYYY-MM-DD.build"
		// since it is the filename for the HTML release note file.
		size_t i = gLastVersionChannel.rfind(' ');
		if (i != std::string::npos)
		{
			LLStringUtil::format_map_t subs;
			subs["[SRVVER]"] = gLastVersionChannel.substr(i + 1);
			url = gSavedSettings.getString("AgniServerReleaseNotesURL");
			url = LLWeb::expandURLSubstitutions(url, subs);
			LL_DEBUGS("About") << "Using a hard-coded URL: " << url << LL_ENDL;
		}
	}
	return url;
}

//static
void LLFloaterAbout::startFetchServerReleaseNotes(const std::string& cap_url)
{
	// We cannot display the URL returned by the ServerReleaseNotes capability
	// because opening it in an external browser will trigger a warning about
	// untrusted SSL certificate.
	// So we query the URL ourselves, expecting to find an URL suitable for
	// external browsers in the "Location:" HTTP header.
	LLCoreHttpUtil::HttpCoroutineAdapter::callbackHttpGet(cap_url,
														  &LLFloaterAbout::handleServerReleaseNotes,
														  &LLFloaterAbout::handleServerReleaseNotes);
}

//static
void LLFloaterAbout::handleServerReleaseNotes(const LLSD& results)
{
	LLFloaterAbout* self = findInstance();
	if (!self) return;	// Floater has been closed...

	LLSD http_headers;
	if (results.has(LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS))
	{
		const LLSD& http_results =
			results[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS];
		http_headers =
			http_results[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS_HEADERS];
	}
	else
	{
		http_headers =
			results[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS_HEADERS];
	}
	LL_DEBUGS("About") << "HTTP headers:\n";
	std::stringstream str;
	LLSDSerialize::toPrettyXML(http_headers, str);
	LL_CONT << str.str() << LL_ENDL;

	std::string url = http_headers[HTTP_IN_HEADER_LOCATION].asString();
	if (url.empty())
	{
		url = getHardCodedURL();
	}
	if (url.empty())
	{
		url = self->getString("ErrorFetchingServerReleaseNotesURL");
	}

	self->updateServerReleaseNotesURL(url);
}
