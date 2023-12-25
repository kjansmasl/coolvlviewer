/**
 * @file llpanelminimap.h (was llnetmap.h)
 * @brief Displays agent and surrounding regions, objects, and avatars.
 *
 * $LicenseInfo:firstyear=2001&license=viewergpl$
 *
 * Original code by James Cook. Copyright (c) 2001-2009, Linden Research, Inc.
 * Copyright (c) 2009-2022, Henri Beauchamp.
 * Changes by Henri Beauchamp:
 *  - Rewrote and optimized part of the code.
 *  - Added unknown altitude avatar plot type (the dash-like dot).
 *  - Allowed both old style (T-like dots) and new style (V-like tiny icons)
 *    for above/below-agent avatar plots.
 *  - Added per-avatar dots colouring (via Lua).
 *  - Added animesh, path-finding and physical objects plotting.
 *  - Added agent sim borders drawing.
 *  - _Mostly_ fixed terrain textures never rendering (via changes done in
 *    llvlcomposition.cpp and in the texture cache) and added a terrain texture
 *    manual refreshing feature to deal with the cases when they still fail to
 *    render.
 *  - Backported (and improved) the optional rendering of banned parcels (from
 *    LL's viewer) and parcel borders (from LL's viewer for the nice but slow
 *    algorithm and from Singularity+Catznip for the fast but less pretty one).
 *  - Added parcel info ("About land" floater) for parcels in the context menu.
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

#ifndef LL_LLPANELMINIMAP_H
#define LL_LLPANELMINIMAP_H

#include "llimage.h"
#include "llmemberlistener.h"
#include "llpanel.h"
#include "llvector3.h"
#include "llvector3d.h"
#include "llcolor4.h"

class LLTextBox;
class LLViewerTexture;
class LLViewerRegion;

typedef enum e_minimap_center
{
	MAP_CENTER_NONE = 0,
	MAP_CENTER_CAMERA = 1
} EMiniMapCenter;

class LLPanelMiniMap final : public LLPanel
{
	friend class LLDrawObjects;
	friend class LLPlotPuppets;
	friend class LLPlotChars;
	friend class LLPlotPhysical;
	friend class LLDrawParcels;
	friend class LLShowParcelInfo;
	friend class LLEnableParcelInfo;

public:
	LLPanelMiniMap(const std::string& name);

	bool postBuild() override;
	void draw() override;
	void reshape(S32 width, S32 height, bool from_parent = true) override;
	bool handleMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleMouseUp(S32 x, S32 y, MASK mask) override;
	bool handleHover(S32 x, S32 y, MASK mask) override;
	bool handleDoubleClick(S32 x, S32 y, MASK mask) override;
	bool handleRightMouseDown(S32 x, S32 y, MASK mask) override;
	bool handleScrollWheel(S32 x, S32 y, S32 clicks) override;
	bool handleToolTip(S32 x, S32 y, std::string& msg, LLRect* rect) override;

	void renderScaledPointGlobal(const LLVector3d& pos, const LLColor4U& color,
								 F32 radius);

	LL_INLINE void addPathFindingCharacter(const LLVector3d& global_pos)
	{
		mPathfindingCharsPos.emplace_back(global_pos);
	}

	LL_INLINE void addPhysicalObject(const LLVector3d& global_pos)
	{
		mPhysicalObjectsPos.emplace_back(global_pos);
	}

private:
	void setScale(F32 scale);

	LL_INLINE void setPan(F32 x, F32 y)		{ mTargetPanX = x; mTargetPanY = y; }

	void renderPoint(const LLVector3 &pos, const LLColor4U& color,
					 S32 diameter, S32 relative_height = 0);
	LLVector3 globalPosToView(const LLVector3d& glob_pos, bool rotated) const;
	LLVector3d viewPosToGlobal(S32 x,S32 y, bool rotated) const;

	void drawTracking(const LLVector3d& pos_global, bool rotated,
					  const LLColor4& color, bool draw_arrow = true);

	// Note: pos.mV[VZ] is the relative altitude
	void drawAvatar(const LLColor4& color, const LLVector3& pos);

	void setDirectionPos(LLTextBox* text_box, F32 rotation);
	void updateMinorDirections();

	LLVector3d getPosCenterGlobal() const;

	bool createRawImage(LLPointer<LLImageRaw>& rawimagep);
	void createObjectImage();
	void createParcelImage();

	void updateObjectImage(const LLVector3d& pos_center_global);

	void updateParcelImage(const LLVector3d& pos_center_global, LLColor4U c);
	void renderParcelBorders(const LLViewerRegion* regionp, const LLColor4U& c,
							 S32 img_width, S32 img_height);

	LL_INLINE bool isAgentUnderCursor()		{ return mClosestAgentToCursor.notNull(); }

	LL_INLINE static bool isAgentUnderCursor(void*)
	{
		return sInstance && sInstance->mClosestAgentToCursor.notNull();
	}

	static bool outsideSlop(S32 x, S32 y, S32 start_x, S32 start_y, S32 slop);

	static void showAgentProfile(void*);
	class LLScaleMap final : public LLMemberListener<LLPanelMiniMap>
	{
	public:
		bool handleEvent(LLPointer<LLOldEvents::LLEvent>,
						 const LLSD& userdata) override;
	};

	class LLCenterMap final : public LLMemberListener<LLPanelMiniMap>
	{
	public:
		bool handleEvent(LLPointer<LLOldEvents::LLEvent>,
						 const LLSD& userdata) override;
	};

	class LLCheckCenterMap final : public LLMemberListener<LLPanelMiniMap>
	{
	public:
		bool handleEvent(LLPointer<LLOldEvents::LLEvent>,
						 const LLSD& userdata) override;
	};

	class LLRotateMap final : public LLMemberListener<LLPanelMiniMap>
	{
	public:
		bool handleEvent(LLPointer<LLOldEvents::LLEvent>,
						 const LLSD&) override;
	};

	class LLCheckRotateMap final : public LLMemberListener<LLPanelMiniMap>
	{
	public:
		bool handleEvent(LLPointer<LLOldEvents::LLEvent>,
						 const LLSD& userdata) override;
	};

	class LLDrawWater final : public LLMemberListener<LLPanelMiniMap>
	{
	public:
		bool handleEvent(LLPointer<LLOldEvents::LLEvent>,
						 const LLSD&) override;
	};

	class LLCheckDrawWater final : public LLMemberListener<LLPanelMiniMap>
	{
	public:
		bool handleEvent(LLPointer<LLOldEvents::LLEvent>,
						 const LLSD& userdata) override;
	};

	class LLDrawObjects final : public LLMemberListener<LLPanelMiniMap>
	{
	public:
		bool handleEvent(LLPointer<LLOldEvents::LLEvent>,
						 const LLSD&) override;
	};

	class LLCheckDrawObjects final : public LLMemberListener<LLPanelMiniMap>
	{
	public:
		bool handleEvent(LLPointer<LLOldEvents::LLEvent>,
						 const LLSD& userdata) override;
	};

	class LLPlotPuppets final : public LLMemberListener<LLPanelMiniMap>
	{
	public:
		bool handleEvent(LLPointer<LLOldEvents::LLEvent>,
						 const LLSD&) override;
	};

	class LLCheckPlotPuppets final : public LLMemberListener<LLPanelMiniMap>
	{
	public:
		bool handleEvent(LLPointer<LLOldEvents::LLEvent>,
						 const LLSD& userdata) override;
	};

	class LLPlotChars final : public LLMemberListener<LLPanelMiniMap>
	{
	public:
		bool handleEvent(LLPointer<LLOldEvents::LLEvent>,
						 const LLSD&) override;
	};

	class LLCheckPlotChars final : public LLMemberListener<LLPanelMiniMap>
	{
	public:
		bool handleEvent(LLPointer<LLOldEvents::LLEvent>,
						 const LLSD& userdata) override;
	};

	class LLEnablePlotChars final : public LLMemberListener<LLPanelMiniMap>
	{
	public:
		bool handleEvent(LLPointer<LLOldEvents::LLEvent>,
						 const LLSD& userdata) override;
	};

	class LLPlotPhysical final : public LLMemberListener<LLPanelMiniMap>
	{
	public:
		bool handleEvent(LLPointer<LLOldEvents::LLEvent>,
						 const LLSD&) override;
	};

	class LLCheckPlotPhysical final : public LLMemberListener<LLPanelMiniMap>
	{
	public:
		bool handleEvent(LLPointer<LLOldEvents::LLEvent>,
						 const LLSD& userdata) override;
	};

	class LLEnablePlotPhysical final : public LLMemberListener<LLPanelMiniMap>
	{
	public:
		bool handleEvent(LLPointer<LLOldEvents::LLEvent>,
						 const LLSD& userdata) override;
	};

	class LLDrawBorders final : public LLMemberListener<LLPanelMiniMap>
	{
	public:
		bool handleEvent(LLPointer<LLOldEvents::LLEvent>,
						 const LLSD&) override;
	};

	class LLCheckDrawBorders final : public LLMemberListener<LLPanelMiniMap>
	{
	public:
		bool handleEvent(LLPointer<LLOldEvents::LLEvent>,
						 const LLSD& userdata) override;
	};

	class LLDrawBans final : public LLMemberListener<LLPanelMiniMap>
	{
	public:
		bool handleEvent(LLPointer<LLOldEvents::LLEvent>,
						 const LLSD&) override;
	};

	class LLCheckDrawBans final : public LLMemberListener<LLPanelMiniMap>
	{
	public:
		bool handleEvent(LLPointer<LLOldEvents::LLEvent>,
						 const LLSD& userdata) override;
	};

	class LLDrawParcels final : public LLMemberListener<LLPanelMiniMap>
	{
	public:
		bool handleEvent(LLPointer<LLOldEvents::LLEvent>,
						 const LLSD&) override;
	};

	class LLCheckDrawParcels final : public LLMemberListener<LLPanelMiniMap>
	{
	public:
		bool handleEvent(LLPointer<LLOldEvents::LLEvent>,
						 const LLSD& userdata) override;
	};

	class LLShowParcelInfo final : public LLMemberListener<LLPanelMiniMap>
	{
	public:
		bool handleEvent(LLPointer<LLOldEvents::LLEvent>,
						 const LLSD&) override;
	};

	class LLEnableParcelInfo final : public LLMemberListener<LLPanelMiniMap>
	{
	public:
		bool handleEvent(LLPointer<LLOldEvents::LLEvent>,
						 const LLSD& userdata) override;
	};

	class LLRefreshTerrain final : public LLMemberListener<LLPanelMiniMap>
	{
	public:
		bool handleEvent(LLPointer<LLOldEvents::LLEvent>,
						 const LLSD&) override;
	};

	class LLStopTracking final : public LLMemberListener<LLPanelMiniMap>
	{
	public:
		bool handleEvent(LLPointer<LLOldEvents::LLEvent>,
						 const LLSD&) override;
	};

	class LLEnableTracking final : public LLMemberListener<LLPanelMiniMap>
	{
	public:
		bool handleEvent(LLPointer<LLOldEvents::LLEvent>,
						 const LLSD& userdata) override;
	};

	class LLShowAgentProfile final : public LLMemberListener<LLPanelMiniMap>
	{
	public:
		bool handleEvent(LLPointer<LLOldEvents::LLEvent>,
						 const LLSD&) override;
	};

	class LLEnableProfile final : public LLMemberListener<LLPanelMiniMap>
	{
	public:
		bool handleEvent(LLPointer<LLOldEvents::LLEvent>,
						 const LLSD& userdata) override;
	};

public:
	static S32					sMiniMapCenter;
	static bool					sRotateMap;
	static bool					sMiniMapRotate;

private:
	LLHandle<LLView>			mPopupMenuHandle;

	LLVector3d					mPosGlobaltAtLastRightClick;
	LLVector3d					mObjectImageCenterGlobal;
	LLVector3d					mParcelImageCenterGlobal;
	LLPointer<LLImageRaw>		mObjectRawImagep;
	LLPointer<LLImageRaw>		mParcelRawImagep;
	LLPointer<LLViewerTexture>	mObjectImagep;
	LLPointer<LLViewerTexture>	mParcelImagep;

	LLTextBox*					mNorthLabel;
	LLTextBox*					mSouthLabel;
	LLTextBox*					mWestLabel;
	LLTextBox*					mEastLabel;
	LLTextBox*					mNorthWestLabel;
	LLTextBox*					mNorthEastLabel;
	LLTextBox*					mSouthWestLabel;
	LLTextBox*					mSouthEastLabel;

	typedef std::vector<LLVector3d> objs_pos_vec_t;
	objs_pos_vec_t				mPathfindingCharsPos;
	objs_pos_vec_t				mPhysicalObjectsPos;

	LLUUID						mClosestAgentToCursor;
	LLUUID						mClosestAgentAtLastRightClick;

	LLColor4					mBackgroundColor;

	std::string					mMapToolTip;
	std::string					mRegionPrefix;
	std::string					mParcelPrefix;
	std::string					mOwnerPrefix;

	// Size of a region in pixels
	F32							mScale;
	// World meters to map pixels
	F32							mPixelsPerMeter;
	// Texels per meter on map
	F32							mObjectMapTPM;
	// Width of object map in pixels
	F32							mObjectMapPixels;
	F32							mDotRadius;			// Size of avatar markers
	F32							mTargetPanX;
	F32							mTargetPanY;
	F32							mCurPanX;
	F32							mCurPanY;

	S32							mMouseDownPanX;		// Value at start of drag
	S32							mMouseDownPanY;		// Value at start of drag
	S32							mMouseDownX;
	S32							mMouseDownY;
	bool						mPanning;			// Map has been dragged

	bool						mUpdateObjectImage;
	bool						mUpdateParcelImage;

	bool						mHasDrawnParcels;

	static LLPanelMiniMap*		sInstance;
};

#endif
