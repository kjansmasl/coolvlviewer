/**
 * @file lltracker.cpp
 * @brief Container for objects user is tracking.
 *
 * $LicenseInfo:firstyear=2003&license=viewergpl$
 *
 * Copyright (c) 2003-2009, Linden Research, Inc.
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

#include "lltracker.h"

#include "llgl.h"
#include "llrender.h"

#include "llagent.h"
#include "llappviewer.h"
#include "llavatartracker.h"
#include "llchatbar.h"
#include "llfloaterworldmap.h"
#include "llhudtext.h"
#include "llhudview.h"
#include "llinventorymodel.h"
#include "lllandmarklist.h"
#include "llpanelworldmap.h"
//MK
#include "mkrlinterface.h"
//mk
#include "llsky.h"
#include "lltoolbar.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerinventory.h"
#include "llworld.h"

constexpr F32 DESTINATION_REACHED_RADIUS = 3.f;
constexpr F32 DESTINATION_VISITED_RADIUS = 6.f;

// this last one is useful for when the landmark is
// very close to agent when tracking is turned on
constexpr F32 DESTINATION_UNVISITED_RADIUS = 12.f;

constexpr S32 ARROW_OFF_RADIUS_SQRD = 100;

constexpr S32 HUD_ARROW_SIZE = 32;

// Global
LLTracker gTracker;

LLTracker::LLTracker()
:	mTrackingStatus(TRACKING_NOTHING),
	mTrackingLocationType(LOCATION_NOTHING),
	mHUDArrowCenterX(0),
	mHUDArrowCenterY(0),
	mHasReachedLandmark(false),
	mHasLandmarkPosition(false),
	mLandmarkHasBeenVisited(false),
	mIsTrackingLocation(false)
{
}

LLTracker::~LLTracker()
{
	purgeBeaconText();
}

void LLTracker::drawHUDArrow()
{
	switch (mTrackingStatus)
	{
		case TRACKING_AVATAR:
			// Tracked avatar
			if (gAvatarTracker.haveTrackingInfo())
			{
				drawMarker(gAvatarTracker.getGlobalPos(), LLUI::sTrackColor);
			}
			break;

		case TRACKING_LANDMARK:
			drawMarker(getTrackedPositionGlobal(), LLUI::sTrackColor);
			break;

		case TRACKING_LOCATION:
			drawMarker(mTrackedPositionGlobal, LLUI::sTrackColor);
			break;

		default:
			break;
	}
}

void LLTracker::render3D()
{
	if (!gFloaterWorldMapp)
	{
		return;
	}

//MK
	if (gRLenabled && gRLInterface.mContainsShowloc)
	{
		mTrackedLocationName.clear();
	}
//mk

	if (mIsTrackingLocation)
 	{
		// Arbitary location beacon
		if (!mBeaconText)
		{
			mBeaconText =
				(LLHUDText*)LLHUDObject::addHUDObject(LLHUDObject::LL_HUD_TEXT);
			mBeaconText->setDoFade(false);
		}

		if (gFloaterWorldMapp->getDistanceToDestination(mTrackedPositionGlobal)
				< DESTINATION_REACHED_RADIUS)
		{
			stopTrackingLocation();
		}
		else
		{
			renderBeacon(mTrackedPositionGlobal, LLUI::sTrackColor,
						 mBeaconText, mTrackedLocationName);
		}
	}
	else if (mTrackedLandmarkAssetID.notNull())
	{
		// Landmark beacon
		if (!mBeaconText)
		{
			mBeaconText =
				(LLHUDText*)LLHUDObject::addHUDObject(LLHUDObject::LL_HUD_TEXT);
			mBeaconText->setDoFade(false);
		}

		if (!mHasLandmarkPosition)
		{
			// Maybe we just finished downloading the asset...
			cacheLandmarkPosition();
			return;
		}

		bool close =
			gFloaterWorldMapp->getDistanceToDestination(mTrackedPositionGlobal,
														1.f) < DESTINATION_VISITED_RADIUS;

		if (!mLandmarkHasBeenVisited && close)
		{
			// It is close enough: flag as visited
			setLandmarkVisited();
		}

		if (!mHasReachedLandmark && close)
		{
			// It is VERY close: automatically stop tracking
			stopTrackingLandmark();
			return;
		}

		if (mHasReachedLandmark && !close)
		{
			// This is so that landmark beacons do not immediately disappear
			// when they are created only a few meters away, yet disappear when
			// the agent wanders away and back again
			mHasReachedLandmark = false;
		}
		renderBeacon(mTrackedPositionGlobal, LLUI::sTrackColor, mBeaconText,
					 mTrackedLandmarkName);
	}
	// Avatar beacon
	else if (gAvatarTracker.haveTrackingInfo())
	{
		if (!mBeaconText)
		{
			mBeaconText =
				(LLHUDText*)LLHUDObject::addHUDObject(LLHUDObject::LL_HUD_TEXT);
			mBeaconText->setDoFade(false);
		}

		F32 dist =
			gFloaterWorldMapp->getDistanceToDestination(mTrackedPositionGlobal,
														0.f);
		if (dist < DESTINATION_REACHED_RADIUS)
		{
			stopTrackingAvatar();
		}
		else
		{
			renderBeacon(gAvatarTracker.getGlobalPos(), LLUI::sTrackColor,
						 mBeaconText, gAvatarTracker.getName());
		}
	}
	else
	{
		bool stop_tracking = false;
		const LLUUID& avatar_id = gAvatarTracker.getAvatarID();
		if (avatar_id.isNull())
		{
			stop_tracking = true;
		}
		else if (!gAgent.isGodlike())
		{
			const LLRelationship* buddy =
				gAvatarTracker.getBuddyInfo(avatar_id);
			stop_tracking = !buddy || !buddy->isOnline();
		}
		if (stop_tracking)
		{
			stopTrackingAvatar();
		}
	}
}

void LLTracker::trackAvatar(const LLUUID& avatar_id, const std::string& name)
{
	stopTrackingLandmark();
	stopTrackingLocation();
//MK
	if (gRLenabled &&
		(gRLInterface.mContainsShownames ||
		 gRLInterface.mContainsShownametags))
	{
		stopTrackingAvatar(true);
		return;
	}
//mk

	gAvatarTracker.track(avatar_id, name);
	mTrackingStatus = TRACKING_AVATAR;
	mLabel = name;
	mToolTip.clear();
}

void LLTracker::trackLandmark(const LLUUID& asset_id, const LLUUID& item_id,
							  const std::string& name)
{
	stopTrackingAvatar();
	stopTrackingLocation();
//MK
	if (gRLenabled &&
		(gRLInterface.mContainsShowminimap ||
		 gRLInterface.mContainsShowworldmap))
	{
		stopTrackingLandmark(true);
		return;
	}
//mk

	mTrackedLandmarkAssetID = asset_id;
	mTrackedLandmarkItemID = item_id;
	mTrackedLandmarkName = name;
	cacheLandmarkPosition();
	mTrackingStatus = TRACKING_LANDMARK;
	mLabel = name;
	mToolTip.clear();
}

void LLTracker::trackLocation(const LLVector3d& pos_global,
							  const std::string& full_name,
							  const std::string& tooltip,
							  ETrackingLocationType location_type)
{
	stopTrackingAvatar();
	stopTrackingLandmark();

	mTrackedPositionGlobal = pos_global;
	F32 clamped_z =
		llmax((F32)mTrackedPositionGlobal.mdV[VZ],
			  gWorld.resolveLandHeightGlobal(mTrackedPositionGlobal) + 1.5f);
	mTrackedPositionGlobal.mdV[VZ] = clamped_z;

	mTrackedLocationName = full_name;
	mIsTrackingLocation = true;
	mTrackingStatus = TRACKING_LOCATION;
	mTrackingLocationType = location_type;
	mLabel = full_name;
	mToolTip = tooltip;
}

bool LLTracker::handleMouseDown(S32 x, S32 y)
{
	bool eat_mouse_click = false;
	// Fortunately, we can always compute the tracking arrow center
	S32 dist_sqrd = (x - mHUDArrowCenterX) * (x - mHUDArrowCenterX) +
					(y - mHUDArrowCenterY) * (y - mHUDArrowCenterY);
	if (dist_sqrd < ARROW_OFF_RADIUS_SQRD)
	{
		if (mTrackingStatus)
		{
			stopTracking();
			eat_mouse_click = true;
		}
	}
	return eat_mouse_click;
}

LLVector3d LLTracker::getTrackedPositionGlobal()
{
	switch (mTrackingStatus)
	{
		case TRACKING_AVATAR:
		{
			if (gAvatarTracker.haveTrackingInfo())
			{
				return gAvatarTracker.getGlobalPos();
			}
			break;
		}

		case TRACKING_LANDMARK:
		{
			if (mHasLandmarkPosition)
			{
				return mTrackedPositionGlobal;
			}
			break;
		}

		case TRACKING_LOCATION:
		{
			return mTrackedPositionGlobal;
			break;
		}

		default:
			break;
	}
	return LLVector3d::zero;
}

bool LLTracker::hasLandmarkPosition()
{
	if (!mHasLandmarkPosition)
	{
		// Maybe we just received the landmark position info
		cacheLandmarkPosition();
	}
	return mHasLandmarkPosition;
}

LL_INLINE F32 pulse_func(F32 t, F32 z)
{
	z -= t * F_PI * 64.f - 256.f;
	F32 a = cosf(z * F_PI / 512.f) * 10.f;
	a = (llmax(a, 9.9f) - 9.9f) * 10.f;
	return a;
}

LL_INLINE void draw_shockwave(F32 center_z, F32 t, S32 steps, LLColor4 color)
{
	t *= 0.6284f / F_PI;

	t -= (F32)((S32)t);

	t = llmax(t, 0.5f);
	t -= 0.5f;
	t *= 2.f;

	F32 radius = t * 16536.f;

	// Inexact, but reasonably fast.
	F32 delta = F_TWO_PI / steps;
	F32 sin_delta = sinf(delta);
	F32 cos_delta = cosf(delta);
	F32 x = radius;
	F32 y = 0.f;

	LLColor4 ccol = LLColor4(1.f, 1.f, 1.f, (1.f - t) * 0.25f);
	gGL.begin(LLRender::TRIANGLE_FAN);
	gGL.color4fv(ccol.mV);
	gGL.vertex3f(0.f, 0.f, center_z);
	// make sure circle is complete
	steps += 1;

	color.mV[3] = 1.f - t * t;

	gGL.color4fv(color.mV);
	while (steps--)
	{
		// Successive rotations
		gGL.vertex3f(x, y, center_z);
		F32 x_new = x * cos_delta - y * sin_delta;
		y = x * sin_delta +  y * cos_delta;
		x = x_new;
	}
	gGL.end();
}

void LLTracker::renderBeacon(LLVector3d pos_global, const LLColor4& color,
							 LLHUDText* hud_textp, const std::string& label)
{
	LLVector3d to_vec = pos_global - gAgent.getCameraPositionGlobal();

	F32 dist = (F32)to_vec.length();
	F32 color_frac = 1.f;
	if (dist > 0.99f * gViewerCamera.getFar())
	{
		color_frac = 0.4f;
#if 0
		pos_global = gAgent.getCameraPositionGlobal() +
					 0.99f * (gViewerCamera.getFar() / dist) * to_vec;
#endif
	}
	else
	{
		color_frac = 1.f - 0.6f * (dist / gViewerCamera.getFar());
	}

	static LLCachedControl<bool> cheesy_beacon(gSavedSettings, "CheesyBeacon");

	LLColor4 fogged_color = color_frac * color +
							(1.f - color_frac) * gSky.getSkyFogColor();

	constexpr F32 FADE_DIST = 3.f;
	fogged_color.mV[3] = llmax(0.2f,
							   llmin(0.5f, (dist - FADE_DIST) / FADE_DIST));

	LLVector3 pos_agent = gAgent.getPosAgentFromGlobal(pos_global);

	LLGLSTracker gls_tracker;

	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
	LLGLDisable cull_face(GL_CULL_FACE);
	LLGLDepthTest gls_depth(GL_TRUE, GL_FALSE);

	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.pushMatrix();
	{
		gGL.translatef(pos_agent.mV[0], pos_agent.mV[1], pos_agent.mV[2]);

		if (cheesy_beacon)
		{
			draw_shockwave(1024.f, gRenderStartTime.getElapsedTimeF32(), 32,
						   fogged_color);
		}

		gGL.color4fv(fogged_color.mV);
		constexpr U32 BEACON_VERTS = 256;
		constexpr F32 step = 1024.f / BEACON_VERTS;

		LLVector3 x_axis = gViewerCamera.getLeftAxis();
		F32 t = gRenderStartTime.getElapsedTimeF32();
		F32 dr = dist / gViewerCamera.getFar();

		for (U32 i = 0; i < BEACON_VERTS; ++i)
		{
			F32 x = x_axis.mV[0];
			F32 y = x_axis.mV[1];

			F32 z = (F32)i * step;
			F32 z_next = z + step;

			F32 a = 0.f;
			F32 an = 0.f;
			if (cheesy_beacon)
			{
				a = pulse_func(t, z);
				an = pulse_func(t, z_next);
			}

			LLColor4 c_col = fogged_color + LLColor4(a, a, a, a);
			LLColor4 col_next = fogged_color + LLColor4(an, an, an, an);
			LLColor4 col_edge = fogged_color * LLColor4(a, a, a, 0.f);
			LLColor4 col_edge_next = fogged_color * LLColor4(an, an, an, 0.f);

			a *= 2.f;
			a += 1.f + dr;

			an *= 2.f;
			an += 1.f + dr;

			gGL.begin(LLRender::TRIANGLE_STRIP);
			gGL.color4fv(col_edge.mV);
			gGL.vertex3f(-x * a, -y * a, z);
			gGL.color4fv(col_edge_next.mV);
			gGL.vertex3f(-x * an, -y * an, z_next);

			gGL.color4fv(c_col.mV);
			gGL.vertex3f(0.f, 0.f, z);
			gGL.color4fv(col_next.mV);
			gGL.vertex3f(0.f, 0.f, z_next);

			gGL.color4fv(col_edge.mV);
			gGL.vertex3f(x * a, y * a, z);
			gGL.color4fv(col_edge_next.mV);
			gGL.vertex3f(x * an, y * an, z_next);

			gGL.end();
		}
	}
	gGL.popMatrix();

	LLWString wstr = utf8str_to_wstring(label);
	wstr += '\n';
	wstr += utf8str_to_wstring(llformat("%.0f m",
										dist_vec(pos_global,
												 gAgent.getPositionGlobal())));

	static LLFontGL* font = LLFontGL::getFontSansSerif();
	hud_textp->setFont(font);
	hud_textp->setZCompare(false);
	hud_textp->setColor(LLColor4(1.f, 1.f, 1.f,
								 llclamp((dist - FADE_DIST) / FADE_DIST, 0.2f, 1.f)));

	hud_textp->setString(wstr);
	hud_textp->setVertAlignment(LLHUDText::ALIGN_VERT_CENTER);
	hud_textp->setPositionAgent(pos_agent);
}

void LLTracker::stopTracking(bool clear_ui)
{
	switch (mTrackingStatus)
	{
		case TRACKING_AVATAR :
			stopTrackingAvatar(clear_ui);
			break;

		case TRACKING_LANDMARK :
			stopTrackingLandmark(clear_ui);
			break;

		case TRACKING_LOCATION :
			stopTrackingLocation(clear_ui);
			break;

		default:
			mTrackingStatus = TRACKING_NOTHING;
	}
}

void LLTracker::stopTrackingAvatar(bool clear_ui)
{
	if (gAvatarTracker.getAvatarID().notNull())
	{
		gAvatarTracker.untrack(gAvatarTracker.getAvatarID());
	}

	purgeBeaconText();
	if (gFloaterWorldMapp)
	{
		gFloaterWorldMapp->clearAvatarSelection(clear_ui);
	}
	mTrackingStatus = TRACKING_NOTHING;
}

void LLTracker::stopTrackingLandmark(bool clear_ui)
{
	purgeBeaconText();
	mTrackedLandmarkAssetID.setNull();
	mTrackedLandmarkItemID.setNull();
	mTrackedLandmarkName.clear();
	mTrackedPositionGlobal.setZero();
	mHasLandmarkPosition = false;
	mHasReachedLandmark = false;
	mLandmarkHasBeenVisited = true;
	if (gFloaterWorldMapp)
	{
		gFloaterWorldMapp->clearLandmarkSelection(clear_ui);
	}
	mTrackingStatus = TRACKING_NOTHING;
}

void LLTracker::stopTrackingLocation(bool clear_ui)
{
	purgeBeaconText();
	mTrackedLocationName.clear();
	mIsTrackingLocation = false;
	mTrackedPositionGlobal.setZero();
	if (gFloaterWorldMapp)
	{
		gFloaterWorldMapp->clearLocationSelection(clear_ui);
	}
	mTrackingStatus = TRACKING_NOTHING;
	mTrackingLocationType = LOCATION_NOTHING;
}

void LLTracker::drawMarker(const LLVector3d& pos_global, const LLColor4& color)
{
	if (!gHUDViewp) return;

	// Get our agent position
	LLVector3 pos_local = gAgent.getPosAgentFromGlobal(pos_global);

	// Check in frustum
	LLCoordGL screen;
	S32 x = 0;
	S32 y = 0;

	if (gViewerCamera.projectPosAgentToScreen(pos_local, screen, true) ||
		gViewerCamera.projectPosAgentToScreenEdge(pos_local, screen))
	{
		gHUDViewp->screenPointToLocal(screen.mX, screen.mY, &x, &y);

		// The center of the rendered position of the arrow obeys
		// the following rules:
		// (1) it lies on an ellipse centered on the target position
		// (2) it lies on the line between the target and the window center
		// (3) right now the radii of the ellipse are fixed, but eventually
		//     they will be a function of the target text
		//
		// From those rules we can compute the position of the lower left
		// corner of the image
		LLRect rect = gHUDViewp->getRect();
		S32 x_center = lltrunc(0.5f * (F32)rect.getWidth());
		S32 y_center = lltrunc(0.5f * (F32)rect.getHeight());
		x = x - x_center;	// x and y relative to center
		y = y - y_center;
		F32 dist = sqrtf((F32)(x * x + y * y));
		S32 half_arrow_size = HUD_ARROW_SIZE / 2;
		if (dist > 0.f)
		{
			constexpr F32 ARROW_ELLIPSE_RADIUS_X = 2 * HUD_ARROW_SIZE;
			constexpr F32 ARROW_ELLIPSE_RADIUS_Y = HUD_ARROW_SIZE;

			// compute where the arrow should be
			F32 x_target = (F32)(x + x_center) -
						   ARROW_ELLIPSE_RADIUS_X * ((F32)x / dist);
			F32 y_target = (F32)(y + y_center) -
						   ARROW_ELLIPSE_RADIUS_Y * ((F32)y / dist);

			// keep the arrow within the window
			F32 margin = 0.0;
			if (gToolBarp && gToolBarp->getVisible() &&
				gChatBarp && gChatBarp->getVisible())
			{
				margin = (F32)(gChatBarp->getRect().getHeight());
			}
			F32 x_clamped = llclamp(x_target, (F32)half_arrow_size,
									(F32)(rect.getWidth() - half_arrow_size));
			F32 y_clamped = llclamp(y_target, (F32)half_arrow_size + margin,
									(F32)(rect.getHeight() - half_arrow_size));

			F32 slope = (F32)(y) / (F32)(x);
			F32 window_ratio = (F32)(rect.getHeight() - HUD_ARROW_SIZE) /
							   (F32)(rect.getWidth() - HUD_ARROW_SIZE);

			// If the arrow has been clamped on one axis then we need to
			// compute the other axis
			if (fabsf(slope) > window_ratio)
			{
				if (y_clamped != (F32)y_target)
				{
					// Clamp by y
					x_clamped = (y_clamped - (F32)y_center) / slope +
								(F32)x_center;
				}
			}
			else if (x_clamped != (F32)x_target)
			{
				// Clamp by x
				y_clamped = (x_clamped - (F32)x_center) * slope +
							(F32)y_center;
			}
			mHUDArrowCenterX = lltrunc(x_clamped);
			mHUDArrowCenterY = lltrunc(y_clamped);
		}
		else
		{
			// recycle the old values
			x = mHUDArrowCenterX - x_center;
			y = mHUDArrowCenterY - y_center;
		}

		F32 angle = atan2f((F32)y, (F32)x);
		gl_draw_scaled_rotated_image(mHUDArrowCenterX - half_arrow_size,
									 mHUDArrowCenterY - half_arrow_size,
									 HUD_ARROW_SIZE, HUD_ARROW_SIZE,
									 RAD_TO_DEG * angle,
									 LLPanelWorldMap::sTrackArrowImage->getImage(),
									 color);
	}
}

void LLTracker::setLandmarkVisited()
{
	if (mTrackedLandmarkItemID.isNull())
	{
		return;
	}

	LLViewerInventoryItem* item =
		(LLViewerInventoryItem*)gInventory.getItem(mTrackedLandmarkItemID);
	if (!item)
	{
		return;
	}

	U32 flags = item->getFlags();
	if ((flags & LLInventoryItem::II_FLAGS_LANDMARK_VISITED))
	{
		return;
	}
	flags |= LLInventoryItem::II_FLAGS_LANDMARK_VISITED;
	item->setFlags(flags);

	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessage("ChangeInventoryItemFlags");
	msg->nextBlock("AgentData");
	msg->addUUID("AgentID", gAgentID);
	msg->addUUID("SessionID", gAgentSessionID);
	msg->nextBlock("InventoryData");
	msg->addUUID("ItemID", mTrackedLandmarkItemID);
	msg->addU32("Flags", flags);
	gAgent.sendReliableMessage();

	LLInventoryModel::LLCategoryUpdate up(item->getParentUUID(), 0);
	gInventory.accountForUpdate(up);

	// Need to communicate that the icon needs to change...
	gInventory.addChangedMask(LLInventoryObserver::REBUILD, item->getUUID());
	gInventory.notifyObservers();
}

void LLTracker::cacheLandmarkPosition()
{
	// The landmark asset download may have finished, in which case we will now
	// be able to figure out where we are trying to go
	bool found_landmark = false;
	if (mTrackedLandmarkAssetID == LLFloaterWorldMap::getHomeID())
	{
		LLVector3d pos_global;
		if (gAgent.getHomePosGlobal(&mTrackedPositionGlobal))
		{
			found_landmark = true;
		}
		else
		{
			llwarns << "Could not find home position" << llendl;
			mTrackedLandmarkAssetID.setNull();
			mTrackedLandmarkItemID.setNull();
		}
	}
	else
	{
		LLLandmark* landmark = gLandmarkList.getAsset(mTrackedLandmarkAssetID);
		if (landmark && landmark->getGlobalPos(mTrackedPositionGlobal))
		{
			found_landmark = true;

			// cache the object's visitation status
			mLandmarkHasBeenVisited = false;
			LLInventoryItem* item = gInventory.getItem(mTrackedLandmarkItemID);
			if (item
				&& item->getFlags()&LLInventoryItem::II_FLAGS_LANDMARK_VISITED)
			{
				mLandmarkHasBeenVisited = true;
			}
		}
	}
	if (found_landmark && gFloaterWorldMapp)
	{
		mHasReachedLandmark = false;
		F32 dist =
			gFloaterWorldMapp->getDistanceToDestination(mTrackedPositionGlobal,
														1.f);
		if (dist < DESTINATION_UNVISITED_RADIUS)
		{
			mHasReachedLandmark = true;
		}
		mHasLandmarkPosition = true;
	}
	mHasLandmarkPosition = found_landmark;
}

void LLTracker::purgeBeaconText()
{
	if (mBeaconText.notNull())
	{
		mBeaconText->markDead();
		mBeaconText = NULL;
	}
}
