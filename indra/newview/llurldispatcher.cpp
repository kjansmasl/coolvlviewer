/**
 * @file llurldispatcher.cpp
 * @brief Central registry for all URL handlers
 *
 * $LicenseInfo:firstyear=2007&license=viewergpl$
 *
 * Copyright (c) 2007-2009, Linden Research, Inc.
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

#include "llurldispatcher.h"

#include "llnotifications.h"
#include "llregionhandle.h"
#include "llsd.h"
#include "lluri.h"

#include "llagent.h"			// teleportViaLocation()
#include "llcommandhandler.h"
#include "llfloaterurldisplay.h"
#include "llfloaterworldmap.h"
#include "llgridmanager.h"
#include "llpanellogin.h"
#include "llslurl.h"
#include "llstartup.h"
#include "llweb.h"
#include "llworldmap.h"

//-----------------------------------------------------------------------------
// LLURLDispatcherImpl class
//-----------------------------------------------------------------------------

class LLURLDispatcherImpl
{
	friend class LLTeleportHandler;

protected:
	LOG_CLASS(LLURLDispatcherImpl);

public:
	// returns true if handled or explicitly blocked.
	static bool dispatch(const LLSLURL& slurl, const std::string& nav_type,
						 LLMediaCtrl* web, bool trusted_browser);

	static bool dispatchRightClick(const LLSLURL& url);

private:
	// handles both left and right click
	static bool dispatchCore(const LLSLURL& slurl, const std::string& nav_type,
							 bool right_mouse, LLMediaCtrl* web,
							 bool trusted_browser);

	// Handles secondlife:///app/agent/<agent_id>/about and similar by showing
	// panel in Search floater. Returns true if handled or explicitly blocked.
	static bool dispatchApp(const LLSLURL& slurl, const std::string& nav_type,
							bool right_mouse, LLMediaCtrl* web,
							bool trusted_browser);

	// Handles secondlife://Ahern/123/45/67/. Returns true if handled.
	static bool dispatchRegion(const LLSLURL& slurl,
							   const std::string& nav_type, bool right_mouse);

	// Called by LLWorldMap when a location has been resolved to a region name
	static void regionHandleCallback(U64 handle, const std::string& url,
									 const LLUUID& snapshot_id, bool teleport);

	// Called by LLWorldMap when a region name has been resolved to a location
	// in-world, used by places-panel display.
	static void regionNameCallback(U64 handle, const std::string& url,
								   const LLUUID& snapshot_id, bool teleport);
};

//static
bool LLURLDispatcherImpl::dispatchCore(const LLSLURL& slurl,
									   const std::string& nav_type,
									   bool right_mouse, LLMediaCtrl* web,
									   bool trusted_browser)
{
	switch (slurl.getType())
	{
		case LLSLURL::APP:
			return dispatchApp(slurl, nav_type, right_mouse, web, trusted_browser);

		case LLSLURL::LOCATION:
			return dispatchRegion(slurl, nav_type, right_mouse);

		default:
			return false;
	}
}

//static
bool LLURLDispatcherImpl::dispatch(const LLSLURL& slurl,
								   const std::string& nav_type,
								   LLMediaCtrl* web, bool trusted_browser)
{
	llinfos << "slurl: " << slurl.getSLURLString() << llendl;
	return dispatchCore(slurl, nav_type, false, web, trusted_browser);
}

//static
bool LLURLDispatcherImpl::dispatchRightClick(const LLSLURL& slurl)
{
	llinfos << "slurl: " << slurl.getSLURLString() << llendl;
	LLMediaCtrl* web = NULL;
	return dispatchCore(slurl, "clicked", true, web, false);
}

//static
bool LLURLDispatcherImpl::dispatchApp(const LLSLURL& slurl,
									  const std::string& nav_type,
									  bool right_mouse, LLMediaCtrl* web,
									  bool trusted_browser)
{
	llinfos << "cmd: " << slurl.getAppCmd() << " path: " << slurl.getAppPath()
			<< " query: " << slurl.getAppQuery() << llendl;

	const LLSD& query_map = LLURI::queryMap(slurl.getAppQuery());

	bool handled = LLCommandHandler::dispatch(slurl.getAppCmd(),
											  slurl.getAppPath(),
											  query_map, web, nav_type,
											  trusted_browser);

	// Alert if we did not handle this secondlife:///app/ SLURL (but still
	// return true because it is a valid app SLURL)
	if (!handled)
	{
		std::string url = slurl.getSLURLString();
		size_t len = url.length();
		if (len > 0)
		{
			char& last_char = url[len - 1];
			const std::string separators = ".,;:()[]{}\"'`%\\/-+*=|#~&@!?\t";
			if (separators.find(last_char) != std::string::npos)
			{
				// Try again, with one less character in the slurl...
				url = url.substr(0, len - 1);
				return dispatchApp(LLSLURL(url), nav_type, right_mouse, web,
								   trusted_browser);
			}
		}
		gNotifications.add("UnsupportedCommandSLURL");
	}

	return handled;
}

//static
bool LLURLDispatcherImpl::dispatchRegion(const LLSLURL& slurl,
										 const std::string& nav_type,
										 bool right_mouse)
{
	if (slurl.getType() != LLSLURL::LOCATION)
	{
		return false;
	}

	// Before we are logged in, need to update the startup screen to tell the
	// user where they are going.
	if (LLStartUp::getStartupState() < STATE_LOGIN_CLEANUP)
	{
		LLStartUp::setStartSLURL(slurl);
		LLPanelLogin::refreshLocation();
		return true;
	}

	std::string region_name = slurl.getRegion();

	LLFloaterURLDisplay* url_displayp = LLFloaterURLDisplay::getInstance(LLSD());
	url_displayp->setName(region_name);

	// Request a region handle by name (false = do not TP).
	gWorldMap.sendNamedRegionRequest(region_name, regionNameCallback,
									 slurl.getSLURLString(), false);
	return true;
}

//static
void LLURLDispatcherImpl::regionNameCallback(U64 region_handle,
											 const std::string& url,
											 const LLUUID& snapshot_id,
											 bool teleport)
{
	LLSLURL slurl(url);
	if (slurl.getType() == LLSLURL::LOCATION)
	{
		std::string region_name = slurl.getRegion();
		LLVector3 local_pos = slurl.getPosition();

		// Determine whether the point is in this region

		S32 max_x = REGION_WIDTH_UNITS;
		S32 max_y = REGION_WIDTH_UNITS;

		// Variable region size support
		LLSimInfo* sim = gWorldMap.simInfoFromName(region_name);
		if (sim)
		{
			max_x = (S32)sim->getSizeX();
			max_y = (S32)sim->getSizeY();
		}

		if (local_pos.mV[VX] >= 0 && local_pos.mV[VX] < max_x &&
			local_pos.mV[VY] >= 0 && local_pos.mV[VY] < max_y)
		{
			// If point in region, we are done
			regionHandleCallback(region_handle, url, snapshot_id, teleport);
		}
		else	// Otherwise find the new region from the location
		{
			// Add the position to get the new region
			LLVector3d global_pos = from_region_handle(region_handle) +
									LLVector3d(local_pos);
			U64 new_region_handle = to_region_handle(global_pos);
			gWorldMap.sendHandleRegionRequest(new_region_handle,
											  LLURLDispatcherImpl::regionHandleCallback,
											  url, teleport);
		}
	}
}

//static
void LLURLDispatcherImpl::regionHandleCallback(U64 region_handle,
											   const std::string& url,
											   const LLUUID& snapshot_id,
											   bool teleport)
{
	LL_DEBUGS("Teleport") << "Region handle = " <<  region_handle
						  << " - Teleport URI: " << url << LL_ENDL;

	LLSLURL slurl(url);

	LLGridManager* gm = LLGridManager::getInstance();
	// We cannot teleport cross grid at this point
	if (gm->getGridHost(slurl.getGrid()) != gm->getGridHost())
	{
		LLSD args;
		args["SLURL"] = slurl.getLocationString();
		args["CURRENT_GRID"] = gm->getGridHost();
		std::string grid_label = gm->getGridHost(slurl.getGrid());
		if (!grid_label.empty())
		{
			args["GRID"] = grid_label;
		}
		else
		{
			args["GRID"] = slurl.getGrid();
		}
		gNotifications.add("CantTeleportToGrid", args);
		return;
	}

	LLVector3 local_pos = slurl.getPosition();
	if (teleport)
	{
		LLVector3d global_pos = from_region_handle(region_handle);
		global_pos += LLVector3d(local_pos);
		gAgent.teleportViaLocation(global_pos);
		if (gFloaterWorldMapp)
		{
			gFloaterWorldMapp->trackLocation(global_pos);
		}
	}
	else
	{
		// Display informational floater, allow user to click teleport button
		LLFloaterURLDisplay* url_displayp;
		url_displayp = LLFloaterURLDisplay::getInstance(LLSD());
		url_displayp->displayParcelInfo(region_handle, local_pos);
		if (snapshot_id.notNull())
		{
			url_displayp->setSnapshotDisplay(snapshot_id);
		}
		std::string loc_str = llformat("%s %d, %d, %d",
									   slurl.getRegion().c_str(),
									   (S32)local_pos.mV[VX],
									   (S32)local_pos.mV[VY],
									   (S32)local_pos.mV[VZ]);
		url_displayp->setLocationString(loc_str);
	}
}

//-----------------------------------------------------------------------------
// Command handler
// Teleportation links are handled here because they are tightly coupled to URL
// parsing and sim-fragment parsing
//-----------------------------------------------------------------------------

class LLTeleportHandler final : public LLCommandHandler
{
protected:
	LOG_CLASS(LLTeleportHandler);

public:
	// Teleport requests *must* come from a trusted browser inside the app,
	// otherwise a malicious web page could cause a constant teleport loop. JC
	LLTeleportHandler()
	:	LLCommandHandler("teleport", UNTRUSTED_CLICK_ONLY)
	{
	}

	bool handle(const LLSD& tokens, const LLSD&, LLMediaCtrl*) override
	{
		// Construct a "normal" SLURL, resolve the region to a global position,
		// and teleport to it
		size_t ts = tokens.size();
		if (ts < 1) return false;

		LLVector3 coords(128.f, 128.f, 0.f);
		if (ts >= 2)
		{
			coords.mV[VX] = tokens[1].asReal();
		}
		if (ts >= 3)
		{
			coords.mV[VY] = tokens[2].asReal();
		}
		if (ts >= 4)
		{
			coords.mV[VZ] = tokens[3].asReal();
		}

		// Region names may be %20 escaped.
		std::string region_name = LLURI::unescape(tokens[0]);

		// build secondlife://De%20Haro/123/45/67 for use in callback
		std::string url = LLSLURL(region_name, coords).getSLURLString();
		LL_DEBUGS("Teleport") << "Region name: " << region_name
							  << " - Coordinates: " << coords
							  << " - Teleport URI: " << url << LL_ENDL;

		gWorldMap.sendNamedRegionRequest(region_name,
										 LLURLDispatcherImpl::regionHandleCallback,
										 url, true);	// true = teleport
		return true;
	}
};

LLTeleportHandler gTeleportHandler;

//-----------------------------------------------------------------------------
// LLURLDispatcher class proper
//-----------------------------------------------------------------------------

//static
bool LLURLDispatcher::dispatch(const std::string& url,
							   const std::string& nav_type,
							   LLMediaCtrl* web, bool trusted_browser)
{
	return LLURLDispatcherImpl::dispatch(LLSLURL(url), nav_type, web,
										 trusted_browser);
}

//static
bool LLURLDispatcher::dispatchRightClick(const std::string& url)
{
	return LLURLDispatcherImpl::dispatchRightClick(url);
}

//static
bool LLURLDispatcher::dispatchFromTextEditor(const std::string& url)
{
	// NOTE: text editors are considered sources of trusted URLs in order to
	// make objectim and avatar profile links in chat history work. While a
	// malicious resident could chat an app SLURL, the receiving resident will
	// see it and must affirmatively click on it.
	// *TODO: Make this trust model more refined.  JC
	LLMediaCtrl* web = NULL;
	return LLURLDispatcherImpl::dispatch(LLSLURL(url), "clicked", web, true);
}
