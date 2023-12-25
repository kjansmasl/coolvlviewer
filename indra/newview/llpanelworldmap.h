/**
 * @file llpanelworldmap.h
 * @brief LLPanelWorldMap class header file
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 *
 * Copyright (c) 2001-2009, Linden Research, Inc.
 * Copyright (c) 2009-2021, Henri Beauchamp.
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

// View of the global map of the world.
// The data (model) for the global map is in LLWorldMap.

#ifndef LL_LLPANELWORLDMAP_H
#define LL_LLPANELWORLDMAP_H

#include "llpanel.h"
#include "llvector3.h"
#include "llvector3d.h"
#include "llcolor4.h"

#include "llworldmap.h"

constexpr S32 DEFAULT_TRACKING_ARROW_SIZE = 16;

class LLColor4;
class LLItemInfo;
class LLTextBox;

class LLPanelWorldMap final : public LLPanel
{
public:
	LLPanelWorldMap(const std::string& name, const LLRect& rect, U32 layer);

	static void initClass();
	static void cleanupClass();

	// Scale and pan are shared across all instances.
	static void setScale(F32 scale);
	static void setPan(S32 x, S32 y, bool snap = true);

	void reshape(S32 width, S32 height, bool call_from_parent = true) override;
	void setVisible(bool visible) override;

	bool handleMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleMouseUp(S32 x, S32 y, MASK mask) override;
	bool handleDoubleClick(S32 x, S32 y, MASK mask) override;
	bool handleHover(S32 x, S32 y, MASK mask) override;
	bool handleToolTip(S32 x, S32 y, std::string& msg, LLRect* rect) override;

	void draw() override;

	void drawGenericItems(const LLWorldMap::item_info_list_t& items,
						  LLUIImagePtr image);

	void drawGenericItem(const LLItemInfo& item, LLUIImagePtr image);

	void drawImage(const LLVector3d& global_pos, LLUIImagePtr image,
				   const LLColor4& color = LLColor4::white);

	void drawImageStack(const LLVector3d& global_pos, LLUIImagePtr image,
						U32 count, F32 offset, const LLColor4& color);

	void drawAgents();
	void drawEvents();
	void drawFrustum();

	U32 updateVisibleBlocks();

	LLVector3 globalPosToView(const LLVector3d& global_pos);
	LLVector3d viewPosToGlobal(S32 x, S32 y);

	// Draw the tracking indicator, doing the right thing if it's outside
	// the view area.
	void drawTracking(const LLVector3d& pos_global, const LLColor4& color,
					  bool draw_arrow = true,
					  const std::string& label = std::string(),
					  const std::string& tooltip = std::string(),
					  S32 vert_offset = 0);

	static void drawTrackingArrow(const LLRect& view_rect, S32 x, S32 y,
								  const LLColor4& color,
								  S32 arrow_size = DEFAULT_TRACKING_ARROW_SIZE);

	static void drawTrackingCircle(const LLRect& rect, S32 x, S32 y,
								   const LLColor4& color,
								   S32 min_thickness, S32 overlap);

	static void drawIconName(F32 x_pixels, F32 y_pixels, const LLColor4& color,
							 const std::string& first_line,
							 const std::string& second_line);

	// Prevents accidental double clicks
	LL_INLINE static void clearLastClick()		{ sHandledLastClick = false; }

	LL_INLINE static void setDefaultZ(F32 z)	{ sDefaultZ = z; }

protected:
	bool checkItemHit(S32 x, S32 y, LLItemInfo& item, LLUUID& id, bool track);
	void handleClick(S32 x, S32 y, MASK mask, S32& hit_type, LLUUID& id);

	void setDirectionPos(LLTextBox* text_box, F32 rotation);
	void updateDirections();

public:
	LLTextBox*		mTextBoxEast;
	LLTextBox*		mTextBoxNorth;
	LLTextBox*		mTextBoxWest;
	LLTextBox*		mTextBoxSouth;

	LLTextBox*		mTextBoxSouthEast;
	LLTextBox*		mTextBoxNorthEast;
	LLTextBox*		mTextBoxNorthWest;
	LLTextBox*		mTextBoxSouthWest;
	LLTextBox*		mTextBoxScrollHint;

	LLColor4		mBackgroundColor;

	U32				mLayer;

	S32				mSelectIDStart;

	S32				mMouseDownPanX;		// value at start of drag
	S32				mMouseDownPanY;		// value at start of drag
	S32				mMouseDownX;
	S32				mMouseDownY;
	bool			mPanning;			// Are we mid-pan from a user drag ?

	bool			mItemPicked;

	typedef std::vector<U64> handle_list_t;
	handle_list_t	mVisibleRegions;	// set every frame

	static LLUIImagePtr	sAvatarSmallImage;
	static LLUIImagePtr	sAvatarYouImage;
	static LLUIImagePtr	sAvatarYouLargeImage;
	static LLUIImagePtr	sAvatarLevelImage;
	static LLUIImagePtr	sAvatarAboveImage;
	static LLUIImagePtr	sAvatarBelowImage;
	static LLUIImagePtr	sTelehubImage;
	static LLUIImagePtr	sInfohubImage;
	static LLUIImagePtr	sHomeImage;
	static LLUIImagePtr	sEventImage;
	static LLUIImagePtr	sEventMatureImage;
	static LLUIImagePtr	sEventAdultImage;
	static LLUIImagePtr	sTrackCircleImage;
	static LLUIImagePtr	sTrackArrowImage;
	static LLUIImagePtr	sClassifiedsImage;
	static LLUIImagePtr	sForSaleImage;
	static LLUIImagePtr	sForSaleAdultImage;

	static F32		sThreshold;
	static F32		sPixelsPerMeter;	// world meters to map pixels

	static F32		sMapScale;			// scale = size of a region in pixels

	static F32		sPanX;				// in pixels
	static F32		sPanY;				// in pixels
	static F32		sTargetPanX;		// in pixels
	static F32		sTargetPanY;		// in pixels
	static S32		sTrackingArrowX;
	static S32		sTrackingArrowY;

	static F32		sDefaultZ;			// in meters

	static bool		sHandledLastClick;
};

#endif
