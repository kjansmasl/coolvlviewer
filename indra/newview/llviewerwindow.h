/** 
 * @file llviewerwindow.h
 * @brief Description of the LLViewerWindow class.
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

//
// A note about X,Y coordinates:
//
// X coordinates are in pixels, from the left edge of the window client area
// Y coordinates are in pixels, from the BOTTOM edge of the window client area
//
// The Y coordinates therefore match OpenGL window coords, not Windows(tm) window coords.
// If Y is from the top, the variable will be called "y_from_top"

#ifndef LL_LLVIEWERWINDOW_H
#define LL_LLVIEWERWINDOW_H

#include "llalertdialog.h"
#include "llmousehandler.h"
#include "llnotifications.h"
#include "llrect.h"
#include "llstat.h"
#include "lltimer.h"
#include "llwindow.h"
#include "llvector2.h"
#include "llvector3d.h"

#include "llprogressview.h"

class LLCubeMapArray;
class LLDebugText;
class LLHUDIcon;
class LLImageRaw;
class LLTextBox;
class LLTool;
class LLUUID;
class LLVelocityBar;
class LLView;
class LLViewerObject;
class LLVOPartGroup;

#define PICK_HALF_WIDTH 5
#define PICK_DIAMETER (2 * PICK_HALF_WIDTH + 1)

class LLPickInfo
{
public:
	LLPickInfo();
	LLPickInfo(const LLCoordGL& mouse_pos,
			   MASK keyboard_mask,
			   bool pick_transparent,
			   bool pick_rigged,
			   bool pick_particle,
			   bool pick_surface_info,
			   void (*pick_callback)(const LLPickInfo&));

	void fetchResults();
	LLPointer<LLViewerObject> getObject() const;
	LL_INLINE LLUUID getObjectID() const				{ return mObjectID; }

	void getSurfaceInfo();

	static bool isFlora(LLViewerObject* object);

	typedef enum
	{
		PICK_OBJECT,
		PICK_FLORA,
		PICK_LAND,
		PICK_ICON,
		PICK_PARCEL_WALL,
		PICK_INVALID
	} EPickType;

private:
	void			updateXYCoords();

public:
	LLCoordGL		mMousePt;
	MASK			mKeyMask;
	void			(*mPickCallback)(const LLPickInfo& pick_info);

	EPickType		mPickType;
	LLCoordGL		mPickPt;
	LLVector3d		mPosGlobal;
	LLVector3		mObjectOffset;
	LLUUID			mObjectID;
	LLUUID			mParticleOwnerID;
	LLUUID			mParticleSourceID;
	S32				mObjectFace;
	LLHUDIcon*		mHUDIcon;
	LLVector3       mIntersection;
	LLVector2		mUVCoords;
	LLVector2       mSTCoords;
	LLCoordScreen	mXYCoords;
	LLVector3		mNormal;
	LLVector4		mTangent;
	LLVector3		mBinormal;
	bool			mPickTransparent;
	bool			mPickRigged;
	bool			mPickParticle;

private:
	// Do we populate mUVCoord, mNormal, mBinormal ?
	bool			mWantSurfaceInfo;
};

// Max snapshot image size = square of 6144 * 6144 pixels
constexpr S32 MAX_SNAPSHOT_IMAGE_SIZE = 6 * 1024;

class LLViewerWindow final : public LLWindowCallbacks
{
protected:
	LOG_CLASS(LLViewerWindow);

public:
	LLViewerWindow(const std::string& title, S32 x, S32 y, U32 width,
				   U32 height, bool fullscreen);
	~LLViewerWindow() override;

	void shutdownViews();
	void shutdownGL();

	void initGLDefaults();
	void initBase();
	void adjustRectanglesForFirstUse();
	void initWorldUI();

	//
	// LLWindowCallback interface implementation
	//
	bool handleTranslatedKeyDown(KEY key, MASK mask, bool repeat) override;
	bool handleTranslatedKeyUp(KEY key, MASK mask) override;
	void handleScanKey(KEY key, bool down, bool up, bool level) override;
	// NOT going to handle extended:
	bool handleUnicodeChar(llwchar uni_char, MASK mask) override;

	bool handleAnyMouseClick(LLWindow* window, LLCoordGL pos, MASK mask,
							 LLMouseHandler::EClickType clicktype,
							 bool down);
	bool handleMouseDown(LLWindow* window, LLCoordGL pos, MASK mask) override;
	bool handleMouseUp(LLWindow* window, LLCoordGL pos, MASK mask) override;
	bool handleRightMouseDown(LLWindow* wn, LLCoordGL pos, MASK mask) override;
	bool handleRightMouseUp(LLWindow* wn, LLCoordGL pos, MASK mask) override;
	bool handleMiddleMouseDown(LLWindow* w, LLCoordGL pos, MASK mask) override;
	bool handleMiddleMouseUp(LLWindow* win, LLCoordGL pos, MASK mask) override;
	void handleMouseMove(LLWindow* window, LLCoordGL pos, MASK mask) override;
	void handleMouseLeave(LLWindow* window) override;
#if LL_DARWIN
	void handleMouseDragged(LLWindow* win, LLCoordGL pos, MASK mask) override;
#endif
	bool handleCloseRequest(LLWindow* window) override;
	void handleQuit(LLWindow* window) override;
	void handleResize(LLWindow* window, S32 x, S32 y) override;

	void handleFocus(LLWindow* window) override;
	void handleFocusLost(LLWindow* window) override;
	bool handleActivate(LLWindow* window, bool activated) override;
	bool handleActivateApp(LLWindow* window, bool activating) override;

	void handleMenuSelect(LLWindow* window, S32 menu_item) override;
	bool handlePaint(LLWindow* w, S32 x, S32 y, S32 wdth, S32 hght) override;
	void handleScrollWheel(LLWindow* window, S32 clicks) override;
	bool handleDoubleClick(LLWindow* win, LLCoordGL pos, MASK mask) override;
	void handleWindowBlock(LLWindow* window) override;
	void handleWindowUnblock(LLWindow* window) override;
	void handleDataCopy(LLWindow* window, S32 data_type, void* data) override;
#if LL_WINDOWS	// Not used for now under Linux and macOS...
	bool handleTimerEvent(LLWindow* window) override;
	bool handleDeviceChange(LLWindow* window) override;
	bool handleDPIChanged(LLWindow* window, F32 ui_scale_factor,
						  S32 window_width, S32 window_height) override;
#endif
	bool handleWindowDidChangeScreen(LLWindow* window) override;

	// ACCESSORS
	//
	LL_INLINE LLView* getRootView() const				{ return mRootView; }

	// Window in raw pixels as seen on screen.
	LL_INLINE const LLRect& getWindowRect() const		{ return mWindowRect; };
	LL_INLINE S32 getWindowDisplayHeight() const		{ return mWindowRect.getHeight(); }
	LL_INLINE S32 getWindowDisplayWidth()	const		{ return mWindowRect.getWidth(); }

	// Window in scaled pixels (via UI scale), use this for UI elements
	// checking size:
	LL_INLINE const LLRect& getVirtualWindowRect() const
	{
		return mVirtualWindowRect;
	}

	LL_INLINE S32 getWindowHeight() const				{ return mVirtualWindowRect.getHeight(); }
	LL_INLINE S32 getWindowWidth() const				{ return mVirtualWindowRect.getWidth(); }

	LL_INLINE void* getPlatformWindow() const			{ return gWindowp->getPlatformWindow(); }
	LL_INLINE void focusClient() const					{ return gWindowp->focusClient(); };

	LL_INLINE LLCoordGL getLastMouse() const			{ return mLastMousePoint; }
	LL_INLINE S32 getLastMouseX() const					{ return mLastMousePoint.mX; }
	LL_INLINE S32 getLastMouseY() const					{ return mLastMousePoint.mY; }
	LL_INLINE LLCoordGL getCurrentMouse() const			{ return mCurrentMousePoint; }
	LL_INLINE S32 getCurrentMouseX() const				{ return mCurrentMousePoint.mX; }
	LL_INLINE S32 getCurrentMouseY() const				{ return mCurrentMousePoint.mY; }
	LL_INLINE S32 getCurrentMouseDX() const				{ return mCurrentMouseDelta.mX; }
	LL_INLINE S32 getCurrentMouseDY() const				{ return mCurrentMouseDelta.mY; }
	LL_INLINE LLCoordGL getCurrentMouseDelta() const	{ return mCurrentMouseDelta; }
	LL_INLINE bool getLeftMouseDown() const				{ return mLeftMouseDown; }
	LL_INLINE bool getMiddleMouseDown() const			{ return mMiddleMouseDown; }
	LL_INLINE bool getRightMouseDown()	const			{ return mRightMouseDown; }

	LL_INLINE static const LLStat& getMouseVelocityStat()
	{
		return sMouseVelocityStat;
	}

	LL_INLINE const LLPickInfo& getLastPick() const		{ return mLastPick; }
	LL_INLINE const LLPickInfo& getHoverPick() const	{ return mHoverPick; }

	void setupViewport(S32 x_offset = 0, S32 y_offset = 0);
	void setup3DRender();
	void setup2DRender();

	LLVector3 mouseDirectionGlobal(S32 x, S32 y) const;
	LLVector3 mouseDirectionCamera(S32 x, S32 y) const;
	LLVector3 mousePointHUD(S32 x, S32 y) const;

	// Is window of our application frontmost ?
	LL_INLINE bool getActive() const					{ return mActive; }

	// The "target" is the size the user wants the window to be set at, in
	// either full screen or windowed modes (set 'full_screen' as appropriate
	// to get the corresponding desired size); this is *not* always the current
	// window size. HB
	static void getTargetWindow(bool full_screen, U32& width, U32& height);

	LL_INLINE const std::string& getInitAlert()			{ return mInitAlert; }

	//
	// MANIPULATORS
	//
	void saveLastMouse(const LLCoordGL& point);

	void setCursor(ECursorType c);
	void showCursor();
	void hideCursor();
	LL_INLINE bool getCursorHidden()					{ return mCursorHidden; }
	void moveCursorToCenter();	// Move to center of window

	void setShowProgress(bool show);
	LL_INLINE bool getShowProgress() const				{ return mProgressView && mProgressView->getVisible(); }
	LL_INLINE LLProgressView* getProgressView() const	{ return mProgressView; }

	void moveProgressViewToFront();
	void setProgressString(const std::string& string);
	void setProgressPercent(F32 percent);
	void setProgressMessage(const std::string& msg);
	void setProgressCancelButtonVisible(bool show,
										const std::string& label =
											LLStringUtil::null);

	void updateObjectUnderCursor();

	// Once per frame, update UI based on mouse position
	bool handlePerFrameHover();

	bool handleKey(KEY key, MASK mask);
	bool handleKeyUp(KEY key, MASK mask);
	void handleScrollWheel(S32 clicks);

	// Hide normal UI when a logon fails, re-show everything when logon is
	// attempted again
	void setNormalControlsVisible(bool visible);
	void setMenuBackgroundColor();

	void reshape(S32 width, S32 height);
	void sendShapeToSim();

	void draw();
	void updateDebugText();
	void drawDebugText();

	static void loadUserImage(void** cb_data, const LLUUID& uuid);

	void resizeWindow(S32 new_width, S32 new_height);

	typedef enum : U32
	{
		SNAPSHOT_TYPE_COLOR,
		SNAPSHOT_TYPE_DEPTH
	} ESnapshotType;

	bool saveSnapshot(const std::string& filename,
					  S32 image_width, S32 image_height,
					  bool show_ui = true, bool do_rebuild = false,
					  U32 type = SNAPSHOT_TYPE_COLOR);

	bool rawSnapshot(LLImageRaw* rawp, S32 image_width, S32 image_height,
					 bool keep_window_aspect = true, bool is_texture = false,
					 bool show_ui = true, bool do_rebuild = false,
					 U32 type = SNAPSHOT_TYPE_COLOR,
					 S32 max_size = MAX_SNAPSHOT_IMAGE_SIZE);

	void cubeSnapshot(const LLVector3& origin, LLCubeMapArray* cubemapp,
					  S32 face, F32 near_clip, bool dynamic_render);

	bool thumbnailSnapshot(LLImageRaw* rawp, S32 width, S32 height,
						   bool show_ui, bool do_rebuild, U32 type);

	LL_INLINE bool isSnapshotLocSet() const				{ return !sSnapshotDir.empty(); }
	LL_INLINE const std::string& getSnapshotBaseName()	{ return sSnapshotBaseName; }
	void setSnapshotLoc(std::string filepath);
	LL_INLINE void resetSnapshotLoc() const				{ sSnapshotDir.clear(); }
	bool saveImageNumbered(LLImageFormatted* image);

	// Reset the directory where snapshots are saved.
	// Client will open directory picker on next snapshot save.
	void resetSnapshotLoc();

	void playSnapshotAnimAndSound();

	// Draws selection boxes around selected objects, must call displayObjects
	// first
	void renderSelections(bool for_gl_pick, bool pick_parcel_walls,
						  bool for_hud);
	void performPick();
	void returnEmptyPicks();

	void pickAsync(S32 x, S32 y_from_bot, MASK mask,
				   void (*callback)(const LLPickInfo&),
				   bool pick_transparent = false, bool pick_rigged = false,
				   bool pick_particle = false, bool get_surface_info = false);

	LLPickInfo pickImmediate(S32 x, S32 y, bool pick_transparent = false);

	static void hoverPickCallback(const LLPickInfo& pick_info);

	LLHUDIcon* cursorIntersectIcon(S32 mouse_x, S32 mouse_y, F32 depth,
								   LLVector4a* intersection);

	LLViewerObject* cursorIntersect(S32 mouse_x = -1, S32 mouse_y = -1,
									F32 depth = 512.f,
									LLViewerObject* this_object = NULL,
									S32 this_face = -1,
									bool pick_transparent = false,
									bool pick_rigged = false,
									S32* face_hit = NULL,
									LLVector4a* intersection = NULL,
									LLVector2* uv = NULL,
									LLVector4a* normal = NULL,
									LLVector4a* tangent = NULL,
									LLVector4a* start = NULL,
									LLVector4a* end = NULL);

	bool mousePointOnLandGlobal(S32 x, S32 y, LLVector3d* land_pos_global);

	bool mousePointOnPlaneGlobal(LLVector3d& point, S32 x, S32 y,
								 const LLVector3d& plane_point,
								 const LLVector3& plane_normal);

	LLVector3d clickPointInWorldGlobal(S32 x, S32 y_from_bot,
									   LLViewerObject* clicked_object) const;

	// Prints window implementation details
	void dumpState();

	// Handle shutting down GL and bringing it back up
	void requestResolutionUpdate();
	bool checkSettings();
	void restartDisplay();

	LL_INLINE bool getIgnoreDestroyWindow() 			{ return mIgnoreActivate; }

	F32 getDisplayAspectRatio() const;

	LL_INLINE const LLVector2& getDisplayScale() const	{ return mDisplayScale; }
	void calcDisplayScale();

	LL_INLINE void resetMouselookFadeTimer()			{ mMouselookTipFadeTimer.reset(); }

public:
	// Made these two public instead of private, to allow rebuilding our avatar
	void stopGL(bool save_state = true);
	void restoreGL(const std::string& progress_message = LLStringUtil::null);

private:
	bool changeDisplaySettings(LLCoordScreen size, bool disable_vsync,
							   bool show_progress_bar);

	bool shouldShowToolTipFor(LLMouseHandler* mh);
	static bool onAlert(const LLSD& notify);

	void switchToolByMask(MASK mask);
	void destroyWindow();
	void drawMouselookInstructions();
	void initFonts(F32 zoom_factor = 1.f);
	void schedulePick(LLPickInfo& pick_info);

	// Vertical padding for child console rect, varied by bottom clutter:
	S32 getChatConsoleBottomPad();

	// Get optimal cosole rect:
	LLRect getChatConsoleRect();

protected:
	// a view of size mWindowRect, containing all child views:
	LLView*				mRootView;

	LLProgressView*		mProgressView;

	LLTextBox*			mToolTip;

	LLTool*				mToolStored;		// The tool we are overriding

	// Internal class for debug text
	LLDebugText*		mDebugText;

	U32					mCurrResolutionIndex;

	LLVector2			mDisplayScale;
	LLVector2			mDisplayScaleDivisor;

	LLCoordGL			mCurrentMousePoint;	// Last mouse position in GL coords
	LLCoordGL			mLastMousePoint;	// Mouse point at last frame.
	LLCoordGL			mCurrentMouseDelta;	// Amount mouse moved this frame

	LLRect				mWindowRect;
	LLRect				mVirtualWindowRect;

	// Once a tool tip is shown, it will stay visible until the mouse leaves
	// this rect;
	LLRect				mToolTipStickyRect;

	// Area of frame buffer for rendering pick frames (generally follows mouse
	// to avoid going offscreen):
	LLRect				mPickScreenRegion;

	LLPickInfo			mLastPick;
	LLPickInfo			mHoverPick;

	typedef std::vector<LLPickInfo> pick_info_list_t;
	pick_info_list_t	mPicks;

	// Timer for scheduling n picks per second
	LLTimer         	mPickTimer;
	// Timer for fading exit mouselook instructions
	LLTimer         	mMouselookTipFadeTimer;

	// Window / GL initialization requires an alert
	std::string			mInitAlert;

	// used to detect changes in modifier mask
	MASK				mLastMask;

#if LL_DARWIN
	LLFrameTimer		mMouseDownTimer;

	bool				mAllowMouseDragging;
#endif
	bool				mLeftMouseDown;
	bool				mMiddleMouseDown;
	bool				mRightMouseDown;

	bool				mActive;

	// True after a key press or a mouse button event.  False once the mouse
	// moves again:
	bool				mToolTipBlocked;

	// True if the mouse is over our window or if we have captured the mouse:
	bool				mMouseInWindow;

	// Sometimes hide the toolbox, despite having a camera tool selected:
	bool				mSuppressToolbox;

	bool				mCursorHidden;

	bool				mIgnoreActivate;

	bool				mResDirty;
	bool				mStatesDirty;
	// Did the user check the fullscreen checkbox in the display settings:
	bool				mIsFullscreenChecked;

protected:
	static std::string	sSnapshotBaseName;
	static std::string	sSnapshotDir;
	static std::string	sMovieBaseName;
	static LLStat		sMouseVelocityStat;
};

class LLBottomPanel final : public LLPanel
{
public:
	LLBottomPanel(const LLRect& rect);
	~LLBottomPanel() override;

	void setFocusIndicator(LLView* indicator);
	LL_INLINE LLView* getFocusIndicator()				{ return mIndicator; }

	void draw() override;

protected:
	LLView* mIndicator;
};

// This class is for temporarily changing the window title, when you cannot use
// a notification or draw any UI element while an operation is in progress and
// the user needs to be made aware of it. It is currently only used during
// shaders (re)compilation, in LLViewerShaderMgr::setShaders(). HB
class HBTempWindowTitle
{
public:
	// Changes the window title for "<viewer name> - <message>".
	HBTempWindowTitle(const std::string& message);
	// Changes the window title back to its original string.
	~HBTempWindowTitle();
};

void toggle_flying(void*);
void toggle_first_person();
void toggle_build(void*);
void reset_viewer_state_on_sim();
void update_saved_window_size(const std::string& control,
							  S32 delta_width, S32 delta_height);

//
// Globals
//

extern LLViewerWindow*	gViewerWindowp;
extern LLBottomPanel*	gBottomPanelp;

// How long has it been since the mouse last moved ?
extern LLFrameTimer		gMouseIdleTimer;
// Tracks time before setting the avatar away state to true
extern LLFrameTimer		gAwayTimer;
// How long the avatar has been away
extern LLFrameTimer		gAwayTriggerTimer;

extern LLViewerObject*	gDebugRaycastObject;
extern LLVector4a		gDebugRaycastIntersection;
extern LLVOPartGroup*	gDebugRaycastParticle;
extern LLVector4a		gDebugRaycastParticleIntersection;
extern LLVector2		gDebugRaycastTexCoord;
extern LLVector4a		gDebugRaycastNormal;
extern LLVector4a		gDebugRaycastTangent;
extern S32				gDebugRaycastFaceHit;
extern LLVector4a		gDebugRaycastStart;
extern LLVector4a		gDebugRaycastEnd;

extern bool				gDisplayCameraPos;
extern bool				gDisplayWindInfo;
extern bool				gDisplayFOV;

// Only relevant to PBR mode
extern bool				gSnapshotNoPost;

#endif
