/**
 * @file llfloatersnapshot.cpp
 * @brief Snapshot preview window, allowing saving, e-mailing, etc.
 *
 * $LicenseInfo:firstyear=2004&license=viewergpl$
 *
 * Copyright (c) 2004-2009, Linden Research, Inc.
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

#include "boost/unordered_set.hpp"

#include "llfloatersnapshot.h"

#include "llbutton.h"
#include "llcallbacklist.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "lldir.h"
#include "lleconomy.h"
#include "hbfileselector.h"
#include "llfilesystem.h"
#include "llgl.h"
#include "llimagebmp.h"
#include "llimagej2c.h"
#include "llimagejpeg.h"
#include "llimagepng.h"
#include "lllocale.h"
#include "llsdserialize.h"
#include "llsliderctrl.h"
#include "llspinctrl.h"
#include "llradiogroup.h"
#include "llrender.h"
#include "llsys.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "llfloaterperms.h"
#include "llfloaterpostcard.h"
#include "hbfloaterthumbnail.h"
#include "llpipeline.h"
//MK
#include "mkrlinterface.h"
//mk
#include "lltoolfocus.h"
#include "lltoolmgr.h"
#include "llviewerassetupload.h"	// For upload_new_resource()
#include "llviewercontrol.h"
#include "llviewercamera.h"
#include "llviewerregion.h"
#include "llviewerstats.h"
#include "llviewertexteditor.h"
#include "llviewerwindow.h"
#include "llworld.h"

// Static members
S32 LLFloaterSnapshot::sUIWinHeightLong = 546;
S32 LLFloaterSnapshot::sUIWinHeightShort = LLFloaterSnapshot::sUIWinHeightLong - 250;
S32 LLFloaterSnapshot::sUIWinWidth = 215;
U32 LLFloaterSnapshot::sSavedLastSelectedType = 0;
bool LLFloaterSnapshot::sAspectRatioCheckOff = false;

// Instance created in LLViewerWindow::initBase() and destroyed in
// LLViewerWindow::shutdownViews()
LLSnapshotFloaterView* gSnapshotFloaterViewp = NULL;

constexpr F32 AUTO_SNAPSHOT_TIME_DELAY = 1.f;
constexpr S32 MAX_TEXTURE_SIZE = 1024;

F32 SHINE_TIME = 0.5f;
F32 SHINE_WIDTH = 0.6f;
F32 SHINE_OPACITY = 0.3f;
F32 FALL_TIME = 0.6f;
S32 BORDER_WIDTH = 6;

constexpr S32 MAX_POSTCARD_DATASIZE = 1024 * 1024; // one megabyte

//-----------------------------------------------------------------------------
// Class LLSnapshotLivePreview
//-----------------------------------------------------------------------------
class LLSnapshotLivePreview final : public LLView
{
protected:
	LOG_CLASS(LLSnapshotLivePreview);

public:
	enum ESnapshotType : U32
	{
		SNAPSHOT_POSTCARD,
		SNAPSHOT_TEXTURE,
		SNAPSHOT_LOCAL,
		SNAPSHOT_THUMBNAIL,
		SNAPSHOT_TOTAL_TYPES
	};

	LLSnapshotLivePreview(const LLRect& rect);
	~LLSnapshotLivePreview();

	void draw() override;
	void reshape(S32 width, S32 height, bool called_from_parent) override;

	void setSize(S32 w, S32 h);
	void getSize(S32& w, S32& h) const;
	LL_INLINE S32 getDataSize() const			{ return mDataSize; }
	void setMaxImageSize(S32 size);
	LL_INLINE S32 getMaxImageSize()				{ return mMaxImageSize; }

	LL_INLINE U32 getSnapshotType() const		{ return mSnapshotType; }

	LL_INLINE U32 getSnapshotFormat() const		{ return mSnapshotFormat; }

	LL_INLINE bool getSnapshotUpToDate() const	{ return mSnapshotUpToDate; }
	LL_INLINE bool isSnapshotActive()			{ return mSnapshotActive; }

	LL_INLINE LLViewerTexture* getThumbnailImage() const
	{
		return mThumbnailImage.get();
	}

	LL_INLINE LLImageRaw* getPreviewImage()
	{
		return mPreviewImage.get();
	}

	LL_INLINE S32 getThumbnailWidth() const		{ return mThumbnailWidth; }
	LL_INLINE S32 getThumbnailHeight() const	{ return mThumbnailHeight; }
	LL_INLINE bool getThumbnailLock() const		{ return mThumbnailUpdateLock; }
	LL_INLINE bool getThumbnailUpToDate() const	{ return mThumbnailUpToDate;}
	LLViewerTexture* getCurrentImage();
	F32 getImageAspect();
	F32 getAspect();
	LLRect getImageRect();
	bool isImageScaled();

	LL_INLINE void setSnapshotType(U32 type)	{ mSnapshotType = type; }

	LL_INLINE void setSnapshotFormat(U32 type)
	{
		mSnapshotFormat = type;
	}

	LL_INLINE void setSnapshotBufferType(U32 type)
	{
		mSnapshotBufferType = type;
	}

	void setSnapshotQuality(S32 quality);
	void updateSnapshot(bool new_snapshot, bool new_thumbnail = false,
						F32 delay = 0.f);
	void checkAutoSnapshot(bool update_thumbnail = false);

	LLFloaterPostcard* savePostcard();
	void saveTexture();
	void saveLocal();

	bool setThumbnailImageSize();
	void generateThumbnailImage(bool force_update = false);
	LL_INLINE void resetThumbnailImage()		{ mThumbnailImage = NULL; }
	void drawPreviewRect(S32 offset_x, S32 offset_y);

	bool checkImageSize(S32& width, S32& height, bool width_changed = true);

	// Returns true when snapshot generated, false otherwise.
	bool onIdle();

	static void doSaveLocal(HBFileSelector::ESaveFilter type,
							std::string& filename, void* user_data);

private:
	LLColor4					mColor;

	LLPointer<LLViewerTexture>	mViewerImage[2];
	LLRect						mImageRect[2];
	S32							mWidth[2];
	S32							mHeight[2];
	S32							mMaxImageSize;

	LLPointer<LLViewerTexture>	mThumbnailImage;
	S32							mThumbnailWidth;
	S32							mThumbnailHeight;
	LLRect					  	mPreviewRect;

	U32							mCurImageIndex;
	LLPointer<LLImageRaw>		mPreviewImage;
	LLPointer<LLImageRaw>		mPreviewImageEncoded;
	LLPointer<LLImageFormatted>	mFormattedImage;
	LLFrameTimer				mSnapshotDelayTimer;
	LLFrameTimer				mShineAnimTimer;
	LLFrameTimer				mFallAnimTimer;
	S32							mShineCountdown;
	F32							mFlashAlpha;
	S32							mSnapshotQuality;
	S32							mDataSize;
	LLVector3d					mPosTakenGlobal;
	LLVector3					mCameraPos;
	LLQuaternion				mCameraRot;

	U32							mSnapshotFormat;
	U32							mSnapshotType;
	U32							mSnapshotBufferType;

	bool						mThumbnailUpdateLock;
	bool						mThumbnailUpToDate;
	bool						mNeedsFlash;
	bool						mSnapshotUpToDate;
	bool						mSnapshotActive;
	bool						mImageScaled[2];

public:
	bool						mKeepAspectRatio;
};

LLSnapshotLivePreview::LLSnapshotLivePreview(const LLRect& rect)
:	LLView("snapshot_live_preview", rect, false),
	mColor(1.f, 0.f, 0.f, 0.5f),
	mCurImageIndex(0),
	mPreviewImage(NULL),
	mThumbnailImage(NULL) ,
	mThumbnailWidth(0),
	mThumbnailHeight(0),
	mPreviewImageEncoded(NULL),
	mFormattedImage(NULL),
	mMaxImageSize(MAX_SNAPSHOT_IMAGE_SIZE),
	mShineCountdown(0),
	mFlashAlpha(0.f),
	mNeedsFlash(true),
	mSnapshotQuality(0),
	mDataSize(0),
	mSnapshotType(SNAPSHOT_POSTCARD),
	mSnapshotFormat(gSavedSettings.getU32("SnapshotFormat")),
	mSnapshotUpToDate(false),
	mCameraPos(gViewerCamera.getOrigin()),
	mCameraRot(gViewerCamera.getQuaternion()),
	mSnapshotActive(false),
	mSnapshotBufferType(LLViewerWindow::SNAPSHOT_TYPE_COLOR),
	mKeepAspectRatio(gSavedSettings.getBool("KeepAspectForSnapshot")),
	mThumbnailUpdateLock(false),
	mThumbnailUpToDate(false)
{
	setSnapshotQuality(gSavedSettings.getS32("SnapshotQuality"));
	mSnapshotDelayTimer.setTimerExpirySec(0.f);
	mSnapshotDelayTimer.start();
	setFollowsAll();

	mWidth[0] = gViewerWindowp->getWindowDisplayWidth();
	mWidth[1] = gViewerWindowp->getWindowDisplayWidth();
	mHeight[0] = gViewerWindowp->getWindowDisplayHeight();
	mHeight[1] = gViewerWindowp->getWindowDisplayHeight();
	mImageScaled[0] = false;
	mImageScaled[1] = false;
}

LLSnapshotLivePreview::~LLSnapshotLivePreview()
{
	// Delete images
	mPreviewImage = NULL;
	mPreviewImageEncoded = NULL;
	mFormattedImage = NULL;
}

void LLSnapshotLivePreview::setMaxImageSize(S32 size)
{
	mMaxImageSize = llmin(size, MAX_SNAPSHOT_IMAGE_SIZE);
}

LLViewerTexture* LLSnapshotLivePreview::getCurrentImage()
{
	return mViewerImage[mCurImageIndex];
}

F32 LLSnapshotLivePreview::getAspect()
{
	if (mKeepAspectRatio)
	{
		return (F32)getRect().getWidth() / (F32)getRect().getHeight();
	}

	return (F32)mWidth[mCurImageIndex] / (F32)mHeight[mCurImageIndex];
}

F32 LLSnapshotLivePreview::getImageAspect()
{
	return mViewerImage[mCurImageIndex] ? getAspect() : 0.f;
}

LLRect LLSnapshotLivePreview::getImageRect()
{
	return mImageRect[mCurImageIndex];
}

bool LLSnapshotLivePreview::isImageScaled()
{
	return mImageScaled[mCurImageIndex];
}

void LLSnapshotLivePreview::updateSnapshot(bool new_snapshot,
										   bool new_thumbnail, F32 delay)
{
	if (mSnapshotUpToDate)
	{
		S32 old_image_index = mCurImageIndex;
		mCurImageIndex = (mCurImageIndex + 1) % 2;
		mWidth[mCurImageIndex] = mWidth[old_image_index];
		mHeight[mCurImageIndex] = mHeight[old_image_index];
		mFallAnimTimer.start();
	}
	mSnapshotUpToDate = false;

	LLRect& rect = mImageRect[mCurImageIndex];
	rect.set(0, getRect().getHeight(), getRect().getWidth(), 0);

	F32 image_aspect_ratio = (F32)mWidth[mCurImageIndex] /
							 (F32)mHeight[mCurImageIndex];
	F32 window_aspect_ratio = (F32)getRect().getWidth() /
							  (F32)getRect().getHeight();

	if (mKeepAspectRatio)
	{
		if (image_aspect_ratio > window_aspect_ratio)
		{
			// Trim off top and bottom
			S32 new_height = ll_roundp((F32)getRect().getWidth() /
									   image_aspect_ratio);
			rect.mBottom += (getRect().getHeight() - new_height) / 2;
			rect.mTop -= (getRect().getHeight() - new_height) / 2;
		}
		else if (image_aspect_ratio < window_aspect_ratio)
		{
			// Trim off left and right
			S32 new_width = ll_roundp((F32)getRect().getHeight() *
									  image_aspect_ratio);
			rect.mLeft += (getRect().getWidth() - new_width) / 2;
			rect.mRight -= (getRect().getWidth() - new_width) / 2;
		}
	}

	mShineAnimTimer.stop();
	if (new_snapshot)
	{
		mSnapshotDelayTimer.start();
		mSnapshotDelayTimer.setTimerExpirySec(delay);
	}
	if (new_thumbnail)
	{
		mThumbnailUpToDate = false;
	}
	setThumbnailImageSize();
}

void LLSnapshotLivePreview::checkAutoSnapshot(bool update_thumbnail)
{
	bool autosnap = gSavedSettings.getBool("AutoSnapshot");
	updateSnapshot(autosnap, update_thumbnail,
				   autosnap ? AUTO_SNAPSHOT_TIME_DELAY : 0.f);
}

void LLSnapshotLivePreview::setSnapshotQuality(S32 quality)
{
	llclamp(quality, 0, 100);
	if (mSnapshotQuality != quality)
	{
		mSnapshotQuality = quality;
		gSavedSettings.setS32("SnapshotQuality", quality);
	}
}

bool LLSnapshotLivePreview::checkImageSize(S32& width, S32& height,
										   bool width_changed)
{
	S32 w = width;
	S32 h = height;

	// Do not round texture sizes; textures are commonly stretched in world,
	// profiles, etc and need to be "squashed" during upload, not cropped
	// here. HB
#if 0
	// If texture, ignore aspect ratio setting, round image size to power of 2.
	if (gSavedSettings.getU32("LastSnapshotType") == SNAPSHOT_TEXTURE)
	{
		if (width > mMaxImageSize)
		{
			width = mMaxImageSize;
		}
		if (height > mMaxImageSize)
		{
			height = mMaxImageSize;
		}

		// Round to nearest power of 2 based on the direction of movement
		// i.e. higher power of two if increasing texture resolution
		if (gSavedSettings.getS32("LastSnapshotToInventoryWidth") < width ||
			gSavedSettings.getS32("LastSnapshotToInventoryHeight") < height)
		{
			// Up arrow pressed
			width = get_next_power_two(width, MAX_TEXTURE_SIZE);
			height = get_next_power_two(height, MAX_TEXTURE_SIZE);
		}
		else
		{
			// Down or no change
			width = get_lower_power_two(width, MAX_TEXTURE_SIZE);
			height = get_lower_power_two(height, MAX_TEXTURE_SIZE);
		}
	}
	else
#endif
	if (mKeepAspectRatio)
	{
		S32 disp_width = gViewerWindowp->getWindowDisplayWidth();
		S32 disp_height = gViewerWindowp->getWindowDisplayHeight();
		if (disp_width < 1 || disp_height < 1)
		{
			return false;
		}

		// Aspect ratio of the current window
		F32 aspect_ratio = (F32)disp_width / (F32)disp_height;

		// Change another value proportionally
		if (width_changed)
		{
			height = (S32)(width / aspect_ratio);
		}
		else
		{
			width = (S32)(height * aspect_ratio);
		}

		// Bound w/h by the mMaxImageSize
		if (width > mMaxImageSize || height > mMaxImageSize)
		{
			if (width > height)
			{
				width = mMaxImageSize;
				height = (S32)(width / aspect_ratio);
			}
			else
			{
				height = mMaxImageSize;
				width = (S32)(height * aspect_ratio);
			}
		}
	}

	return w != width || h != height;
}

void LLSnapshotLivePreview::drawPreviewRect(S32 offset_x, S32 offset_y)
{
	F32 line_width;
	glGetFloatv(GL_LINE_WIDTH, &line_width);
	gGL.lineWidth(2.f * line_width);
	gl_rect_2d(mPreviewRect.mLeft + offset_x, mPreviewRect.mTop + offset_y,
			   mPreviewRect.mRight + offset_x, mPreviewRect.mBottom + offset_y,
			   LLColor4::black, false);
	gGL.lineWidth(line_width);

	// Draw four alpha rectangles to cover areas outside of the snapshot image
	if (!mKeepAspectRatio)
	{
		LLColor4 alpha_color(0.5f, 0.5f, 0.5f, 0.8f);
		S32 dwl = 0, dwr = 0;
		if (mThumbnailWidth > mPreviewRect.getWidth())
		{
			dwl = dwr = mThumbnailWidth - mPreviewRect.getWidth();
			dwl >>= 1;
			dwr -= dwl;

			gl_rect_2d(mPreviewRect.mLeft + offset_x - dwl,
					   mPreviewRect.mTop + offset_y,
					   mPreviewRect.mLeft + offset_x,
					   mPreviewRect.mBottom + offset_y, alpha_color, true);
			gl_rect_2d(mPreviewRect.mRight + offset_x,
					   mPreviewRect.mTop + offset_y,
					   mPreviewRect.mRight + offset_x + dwr,
					   mPreviewRect.mBottom + offset_y, alpha_color, true);
		}

		if (mThumbnailHeight > mPreviewRect.getHeight())
		{
			S32 dh = (mThumbnailHeight - mPreviewRect.getHeight()) >> 1;
			gl_rect_2d(mPreviewRect.mLeft + offset_x - dwl,
					   mPreviewRect.mBottom + offset_y,
					   mPreviewRect.mRight + offset_x + dwr,
					   mPreviewRect.mBottom + offset_y - dh, alpha_color,
					   true);

			dh = mThumbnailHeight - mPreviewRect.getHeight() - dh;
			gl_rect_2d(mPreviewRect.mLeft + offset_x - dwl,
					   mPreviewRect.mTop + offset_y + dh,
					   mPreviewRect.mRight + offset_x + dwr,
					   mPreviewRect.mTop + offset_y, alpha_color, true);
		}
	}
}

// Called when the frame is frozen.
void LLSnapshotLivePreview::draw()
{
	LLTexUnit* unit0 = gGL.getTexUnit(0);

	if (mSnapshotUpToDate &&  mViewerImage[mCurImageIndex].notNull() &&
		mPreviewImageEncoded.notNull())
	{
		LLColor4 bg_color(0.f, 0.f, 0.3f, 0.4f);
		gl_rect_2d(getRect(), bg_color);
		LLRect& rect = mImageRect[mCurImageIndex];
		LLRect shadow_rect = mImageRect[mCurImageIndex];
		shadow_rect.stretch(BORDER_WIDTH);
		gl_drop_shadow(shadow_rect.mLeft, shadow_rect.mTop, shadow_rect.mRight,
					   shadow_rect.mBottom,
					   LLColor4(0.f, 0.f, 0.f, mNeedsFlash ? 0.f :0.5f), 10);

		LLColor4 image_color(1.f, 1.f, 1.f, 1.f);
		gGL.color4fv(image_color.mV);

		unit0->bind(mViewerImage[mCurImageIndex]);
		// Calculate UV scale

		F32 uv_width = 1.f;
		F32 uv_height = 1.f;
		if (!mImageScaled[mCurImageIndex])
		{
			uv_width = llmin((F32)mWidth[mCurImageIndex] /
							 (F32)mViewerImage[mCurImageIndex]->getWidth(),
							 1.f);
			uv_height = llmin((F32)mHeight[mCurImageIndex] /
							  (F32)mViewerImage[mCurImageIndex]->getHeight(),
							  1.f);
		}
		gGL.pushMatrix();
		{
			gGL.translatef((F32)rect.mLeft, (F32)rect.mBottom, 0.f);
			gGL.begin(LLRender::TRIANGLE_STRIP);
			{
				gGL.texCoord2f(uv_width, uv_height);
				gGL.vertex2i(rect.getWidth(), rect.getHeight());

				gGL.texCoord2f(0.f, uv_height);
				gGL.vertex2i(0, rect.getHeight());

				gGL.texCoord2f(uv_width, 0.f);
				gGL.vertex2i(rect.getWidth(), 0);

				gGL.texCoord2f(0.f, 0.f);
				gGL.vertex2i(0, 0);
			}
			gGL.end();
		}
		gGL.popMatrix();

		gGL.color4f(1.f, 1.f, 1.f, mFlashAlpha);
		gl_rect_2d(getRect());
		if (mNeedsFlash)
		{
			if (mFlashAlpha < 1.f)
			{
				mFlashAlpha = lerp(mFlashAlpha, 1.f,
								   LLCriticalDamp::getInterpolant(0.02f));
			}
			else
			{
				mNeedsFlash = false;
			}
		}
		else
		{
			mFlashAlpha = lerp(mFlashAlpha, 0.f,
							   LLCriticalDamp::getInterpolant(0.15f));
		}

		if (mShineCountdown > 0)
		{
			if (--mShineCountdown == 0)
			{
				mShineAnimTimer.start();
			}
		}
		else if (mShineAnimTimer.getStarted())
		{
			F32 shine_interp = llmin(1.f, mShineAnimTimer.getElapsedTimeF32() /
										  SHINE_TIME);

			// Draw "shine" effect
			LLLocalClipRect clip(getLocalRect());
			{
				// Draw diagonal stripe with gradient that passes over screen
				S32 x1 = gViewerWindowp->getWindowWidth() *
						 ll_round((clamp_rescale(shine_interp,
												 0.f, 1.f, -1.f - SHINE_WIDTH,
												 1.f)));
				S32 delta = ll_roundp(gViewerWindowp->getWindowWidth() *
									  SHINE_WIDTH);
				S32 x2 = x1 + delta;
				S32 x3 = x2 + delta;
				S32 y1 = 0;
				S32 y2 = gViewerWindowp->getWindowHeight();

				unit0->unbind(LLTexUnit::TT_TEXTURE);
				gGL.begin(LLRender::TRIANGLE_STRIP);
				{
					gGL.color4f(1.f, 1.f, 1.f, 0.f);
					gGL.vertex2i(x1 + gViewerWindowp->getWindowWidth(), y2);
					gGL.vertex2i(x1, y1);
					gGL.color4f(1.f, 1.f, 1.f, SHINE_OPACITY);
					gGL.vertex2i(x2 + gViewerWindowp->getWindowWidth(), y2);
					gGL.vertex2i(x2, y1);

					gGL.color4f(1.f, 1.f, 1.f, SHINE_OPACITY);
					gGL.vertex2i(x2 + gViewerWindowp->getWindowWidth(), y2);
					gGL.vertex2i(x2, y1);
					gGL.color4f(1.f, 1.f, 1.f, 0.f);
					gGL.vertex2i(x3 + gViewerWindowp->getWindowWidth(), y2);
					gGL.vertex2i(x3, y1);
				}
				gGL.end();
			}

			// If we are at the end of the animation, stop
			if (shine_interp >= 1.f)
			{
				mShineAnimTimer.stop();
			}
		}
	}

	// Draw framing rectangle
	{
		unit0->unbind(LLTexUnit::TT_TEXTURE);
		gGL.color4f(1.f, 1.f, 1.f, 1.f);
		LLRect outline_rect = mImageRect[mCurImageIndex];
		gGL.begin(LLRender::TRIANGLE_STRIP);
		{
			gGL.vertex2i(outline_rect.mLeft - BORDER_WIDTH,
						 outline_rect.mTop + BORDER_WIDTH);
			gGL.vertex2i(outline_rect.mLeft, outline_rect.mTop);
			gGL.vertex2i(outline_rect.mRight + BORDER_WIDTH,
						 outline_rect.mTop + BORDER_WIDTH);
			gGL.vertex2i(outline_rect.mRight, outline_rect.mTop);
			gGL.vertex2i(outline_rect.mRight + BORDER_WIDTH,
						 outline_rect.mBottom - BORDER_WIDTH);
			gGL.vertex2i(outline_rect.mRight, outline_rect.mBottom);
			gGL.vertex2i(outline_rect.mLeft - BORDER_WIDTH,
						 outline_rect.mBottom - BORDER_WIDTH);
			gGL.vertex2i(outline_rect.mLeft, outline_rect.mBottom);
			gGL.vertex2i(outline_rect.mLeft - BORDER_WIDTH,
						 outline_rect.mTop + BORDER_WIDTH);
			gGL.vertex2i(outline_rect.mLeft, outline_rect.mTop);
		}
		gGL.end();
	}

	// Draw old image dropping away
	if (mFallAnimTimer.getStarted())
	{
		S32 old_image_index = (mCurImageIndex + 1) % 2;
		if (mViewerImage[old_image_index].notNull() &&
			mFallAnimTimer.getElapsedTimeF32() < FALL_TIME)
		{
			F32 fall_interp = mFallAnimTimer.getElapsedTimeF32() / FALL_TIME;
			F32 alpha = clamp_rescale(fall_interp, 0.f, 1.f, 0.8f, 0.4f);
			LLColor4 image_color(1.f, 1.f, 1.f, alpha);
			gGL.color4fv(image_color.mV);
			unit0->bind(mViewerImage[old_image_index]);
			// Calculate UV scale. *FIX: get this to work with old image
			F32 uv_width = 1.f;
			F32 uv_height = 1.f;
			if (!mImageScaled[old_image_index] &&
				mViewerImage[mCurImageIndex].notNull())
			{
				uv_width = llmin((F32)mWidth[old_image_index] /
								 (F32)mViewerImage[mCurImageIndex]->getWidth(),
								 1.f);
				uv_height = llmin((F32)mHeight[old_image_index] /
								  (F32)mViewerImage[mCurImageIndex]->getHeight(),
								  1.f);
			}
			gGL.pushMatrix();
			{
				LLRect& rect = mImageRect[old_image_index];
				gGL.translatef((F32)rect.mLeft,
							   (F32)rect.mBottom -
							   ll_roundp(getRect().getHeight() * 2.f *
										 fall_interp * fall_interp),
							   0.f);
				gGL.rotatef(-45.f * fall_interp, 0.f, 0.f, 1.f);
				gGL.begin(LLRender::TRIANGLE_STRIP);
				{
					gGL.texCoord2f(uv_width, uv_height);
					gGL.vertex2i(rect.getWidth(), rect.getHeight());

					gGL.texCoord2f(0.f, uv_height);
					gGL.vertex2i(0, rect.getHeight());

					gGL.texCoord2f(uv_width, 0.f);
					gGL.vertex2i(rect.getWidth(), 0);

					gGL.texCoord2f(0.f, 0.f);
					gGL.vertex2i(0, 0);
				}
				gGL.end();
			}
			gGL.popMatrix();
		}
	}
}

//virtual
void LLSnapshotLivePreview::reshape(S32 width, S32 height,
									bool called_from_parent)
{
	LLRect old_rect = getRect();
	LLView::reshape(width, height, called_from_parent);
	if (old_rect.getWidth() != width || old_rect.getHeight() != height)
	{
		updateSnapshot(false, true);
	}
}

bool LLSnapshotLivePreview::setThumbnailImageSize()
{
	if (mWidth[mCurImageIndex] < 10 || mHeight[mCurImageIndex] < 10)
	{
		return false;
	}
	S32 window_width = gViewerWindowp->getWindowDisplayWidth();
	S32 window_height = gViewerWindowp->getWindowDisplayHeight();

	F32 window_aspect_ratio = (F32)window_width / (F32)window_height;

	// UI size for thumbnail
	S32 max_width = LLFloaterSnapshot::getUIWinWidth() - 20;
	S32 max_height = 90;

	if (window_aspect_ratio > (F32)max_width / max_height)
	{
		// Image too wide, shrink to width
		mThumbnailWidth = max_width;
		mThumbnailHeight = ll_roundp((F32)max_width / window_aspect_ratio);
	}
	else
	{
		// Image too tall, shrink to height
		mThumbnailHeight = max_height;
		mThumbnailWidth = ll_roundp((F32)max_height * window_aspect_ratio);
	}

	if (mThumbnailWidth > window_width || mThumbnailHeight > window_height)
	{
		// If the window is too small, ignore thumbnail updating.
		return false;
	}

	S32 left = 0 , top = mThumbnailHeight, right = mThumbnailWidth, bottom = 0;
	if (!mKeepAspectRatio)
	{
		F32 ratio_x = (F32)mWidth[mCurImageIndex] / window_width;
		F32 ratio_y = (F32)mHeight[mCurImageIndex] / window_height;

#if 0
		if (mWidth[mCurImageIndex] > window_width ||
			mHeight[mCurImageIndex] > window_height)
		{
#endif
			if (ratio_x > ratio_y)
			{
				top = (S32)(top * ratio_y / ratio_x);
			}
			else
			{
				right = (S32)(right * ratio_x / ratio_y);
			}
#if 0
		}
		else
		{
			right = (S32)(right * ratio_x);
			top = (S32)(top * ratio_y);
		}
#endif
		left = (S32)((mThumbnailWidth - right) * 0.5f);
		bottom = (S32)((mThumbnailHeight - top) * 0.5f);
		top += bottom;
		right += left;
	}
	mPreviewRect.set(left - 1, top + 1, right + 1, bottom - 1);

	return true;
}

void LLSnapshotLivePreview::generateThumbnailImage(bool force_update)
{
	if (mThumbnailUpdateLock)					// In the process of updating
	{
		return;
	}
	if (mThumbnailUpToDate && !force_update)	// Already updated
	{
		return;
	}
	if (mWidth[mCurImageIndex] < 10 || mHeight[mCurImageIndex] < 10)
	{
		return;
	}

	// Lock updating
	mThumbnailUpdateLock = true;

	if (!setThumbnailImageSize())
	{
		mThumbnailUpdateLock = false;
		mThumbnailUpToDate = true;
		return;
	}

	if (mThumbnailImage)
	{
		resetThumbnailImage();
	}

	static LLCachedControl<bool> render_ui(gSavedSettings,
										   "RenderUIInSnapshot");

	LLPointer<LLImageRaw> raw = new LLImageRaw;
	S32 w = get_lower_power_two(mThumbnailWidth, 512) * 2;
	S32 h = get_lower_power_two(mThumbnailHeight, 512) * 2;
	if (!gViewerWindowp->thumbnailSnapshot(raw, w, h, render_ui, false,
										   mSnapshotBufferType))
	{
		raw = NULL;
	}

	if (raw.notNull())
	{
		mThumbnailImage = LLViewerTextureManager::getLocalTexture(raw.get(),
																  false);
		mThumbnailUpToDate = true;
	}

	// Unlock updating
	mThumbnailUpdateLock = false;
}

// Called often. Checks whether it is time to grab a new snapshot and if so,
// does it. Returns true if new snapshot generated, false otherwise.
bool LLSnapshotLivePreview::onIdle()
{
	// If needed, request a new snapshot whenever the camera moves, with a time
	// delay.
	static LLCachedControl<bool> autosnap(gSavedSettings, "AutoSnapshot");
	if (autosnap || !mSnapshotUpToDate)
	{
		LLVector3 new_cam_pos = gViewerCamera.getOrigin();
		LLQuaternion new_cam_rot = gViewerCamera.getQuaternion();
		if (new_cam_pos != mCameraPos || dot(new_cam_rot, mCameraRot) < 0.995f)
		{
			mCameraPos = new_cam_pos;
			mCameraRot = new_cam_rot;
			// Whether a new snapshot is needed or merely invalidate the
			// existing one:
			updateSnapshot(autosnap, false,
						   // Shutter delay if autosnap is true.
						   autosnap ? AUTO_SNAPSHOT_TIME_DELAY : 0.f);
		}
	}

	// See if it is time yet to snap the shot and bomb out otherwise.
	mSnapshotActive = mSnapshotDelayTimer.getStarted() &&
					  mSnapshotDelayTimer.hasExpired() &&
					  // Do not take snapshots while ALT-zoom active
					  !gToolFocus.hasMouseCapture();
	if (!mSnapshotActive)
	{
		return false;
	}

	// Time to produce a snapshot

	if (!mPreviewImage)
	{
		mPreviewImage = new LLImageRaw;
	}

	if (!mPreviewImageEncoded)
	{
		mPreviewImageEncoded = new LLImageRaw;
	}

	setVisible(false);
	setEnabled(false);

	gWindowp->incBusyCount();
	mImageScaled[mCurImageIndex] = false;

	static LLCachedControl<bool> render_ui(gSavedSettings,
										   "RenderUIInSnapshot");
	// Grab the raw image and encode it into desired format
	if (gViewerWindowp->rawSnapshot(mPreviewImage, mWidth[mCurImageIndex],
									mHeight[mCurImageIndex], mKeepAspectRatio,
									getSnapshotType() == SNAPSHOT_TEXTURE,
									render_ui, false, mSnapshotBufferType,
									getMaxImageSize()))
	{
		mPreviewImageEncoded->resize(mPreviewImage->getWidth(),
									 mPreviewImage->getHeight(),
									 mPreviewImage->getComponents());

		if (getSnapshotType() == SNAPSHOT_TEXTURE)
		{
			LLPointer<LLImageJ2C> formatted = new LLImageJ2C;
			LLPointer<LLImageRaw> scaled =
				new LLImageRaw(mPreviewImage->getData(),
							   mPreviewImage->getWidth(),
							   mPreviewImage->getHeight(),
							   mPreviewImage->getComponents());

			scaled->biasedScaleToPowerOfTwo(512);
			mImageScaled[mCurImageIndex] = true;
			if (formatted->encode(scaled))
			{
				mDataSize = formatted->getDataSize();
				formatted->decode(mPreviewImageEncoded);
			}
		}
		else
		{
			// Delete any existing image
			mFormattedImage = NULL;
			// Now create the new one of the appropriate format.
			// Note: postcards hardcoded to use jpeg always.
			U32 format = getSnapshotType() == SNAPSHOT_POSTCARD ?
							LLFloaterSnapshot::SNAPSHOT_FORMAT_JPEG :
							getSnapshotFormat();
			switch (format)
			{
				case LLFloaterSnapshot::SNAPSHOT_FORMAT_PNG:
					mFormattedImage = new LLImagePNG();
					break;

				case LLFloaterSnapshot::SNAPSHOT_FORMAT_JPEG:
					mFormattedImage = new LLImageJPEG(mSnapshotQuality);
					break;

				case LLFloaterSnapshot::SNAPSHOT_FORMAT_BMP:
					mFormattedImage = new LLImageBMP();
			}
			if (mFormattedImage->encode(mPreviewImage))
			{
				mDataSize = mFormattedImage->getDataSize();
				// Special case BMP to copy instead of decode otherwise decode
				// will crash.
				if (format == LLFloaterSnapshot::SNAPSHOT_FORMAT_BMP)
				{
					mPreviewImageEncoded->copy(mPreviewImage);
				}
				else
				{
					mFormattedImage->decode(mPreviewImageEncoded);
				}
			}
		}

		LLPointer<LLImageRaw> scaled =
			new LLImageRaw(mPreviewImageEncoded->getData(),
						   mPreviewImageEncoded->getWidth(),
						   mPreviewImageEncoded->getHeight(),
						   mPreviewImageEncoded->getComponents());

		if (!scaled->isBufferInvalid())
		{
			// Leave original image dimensions, just scale up texture buffer
			if (mPreviewImageEncoded->getWidth() > 1024 ||
				mPreviewImageEncoded->getHeight() > 1024)
			{
				// Go ahead and shrink image to appropriate power of 2 for
				// display
				scaled->biasedScaleToPowerOfTwo(1024);
				mImageScaled[mCurImageIndex] = true;
			}
			else
			{
				// Expand image but keep original image data intact
				scaled->expandToPowerOfTwo(1024, false);
			}

			mViewerImage[mCurImageIndex] =
				LLViewerTextureManager::getLocalTexture(scaled.get(), false);
			LLPointer<LLViewerTexture> curr_preview_image =
				mViewerImage[mCurImageIndex];
			gGL.getTexUnit(0)->bind(curr_preview_image);
			if (getSnapshotType() != SNAPSHOT_TEXTURE)
			{
				curr_preview_image->setFilteringOption(LLTexUnit::TFO_POINT);
			}
			else
			{
				curr_preview_image->setFilteringOption(LLTexUnit::TFO_ANISOTROPIC);
			}
			curr_preview_image->setAddressMode(LLTexUnit::TAM_CLAMP);

			mSnapshotUpToDate = true;
			generateThumbnailImage(true);

			mPosTakenGlobal = gAgent.getCameraPositionGlobal();
			// Wait a few frames to avoid animation glitch due to readback this
			// frame:
			mShineCountdown = 4;
		}
	}
	gWindowp->decBusyCount();

	// Only show fullscreen preview when in freeze frame mode
	setVisible(LLPipeline::sFreezeTime);

	mSnapshotDelayTimer.stop();
	mSnapshotActive = false;

	if (!getThumbnailUpToDate())
	{
		generateThumbnailImage();
	}

	return true;
}

void LLSnapshotLivePreview::setSize(S32 w, S32 h)
{
	mWidth[mCurImageIndex] = w;
	mHeight[mCurImageIndex] = h;
}

void LLSnapshotLivePreview::getSize(S32& w, S32& h) const
{
	w = mWidth[mCurImageIndex];
	h = mHeight[mCurImageIndex];
}

LLFloaterPostcard* LLSnapshotLivePreview::savePostcard()
{
	if (mViewerImage[mCurImageIndex].isNull())
	{
		// This should never happen !  Out of memory ?
		llwarns << "The snapshot image has not been generated !" << llendl;
		return NULL;
	}

	// Calculate and pass in image scale in case image data only use portion
	// of viewerimage buffer
	LLVector2 image_scale(1.f, 1.f);
	if (!isImageScaled())
	{
		image_scale.set(llmin(1.f,
							  (F32)mWidth[mCurImageIndex] /
							  (F32)getCurrentImage()->getWidth()),
						llmin(1.f,
							  (F32)mHeight[mCurImageIndex] /
							  (F32)getCurrentImage()->getHeight()));
	}

	LLImageJPEG* jpg = dynamic_cast<LLImageJPEG*>(mFormattedImage.get());
	if (!jpg)
	{
		llwarns << "Formatted image not a JPEG" << llendl;
		return NULL;
	}
	LLFloaterPostcard* floater =
		LLFloaterPostcard::showFromSnapshot(jpg, mViewerImage[mCurImageIndex],
											image_scale, mPosTakenGlobal);
	// Relinquish lifetime of jpeg image to postcard floater
	mFormattedImage = NULL;
	mDataSize = 0;
	updateSnapshot(false, false);

	return floater;
}

void LLSnapshotLivePreview::saveTexture()
{
	// Generate a new UUID for this asset
	LLTransactionID tid;
	tid.generate();
	LLAssetID new_asset_id = tid.makeAssetID(gAgent.getSecureSessionID());

	LLPointer<LLImageJ2C> formatted = new LLImageJ2C;
	LLPointer<LLImageRaw> scaled =
		new LLImageRaw(mPreviewImage->getData(), mPreviewImage->getWidth(),
					   mPreviewImage->getHeight(),
					   mPreviewImage->getComponents());

	scaled->biasedScaleToPowerOfTwo(MAX_TEXTURE_SIZE);

	if (formatted->encode(scaled))
	{
		LLFileSystem fmt_file(new_asset_id, LLFileSystem::OVERWRITE);
		fmt_file.write(formatted->getData(), formatted->getDataSize());

		std::string pos_string;
		gAgent.buildLocationString(pos_string);
//MK
		if (gRLenabled && gRLInterface.mContainsShowloc)
		{
			pos_string = "(Region hidden)";
		}
//mk
		std::string name = "Snapshot: " + pos_string;
		std::string who_took_it;
		gAgent.buildFullname(who_took_it);
		std::string desc = "Taken by " + who_took_it + " at " + pos_string;

		S32 expected_upload_cost =
			LLEconomy::getInstance()->getTextureUploadCost();

		// Note: Snapshots to inventory is a special case of content upload:
		U32 perms = PERM_MOVE | LLFloaterPerms::getNextOwnerPerms();
		if (gSavedSettings.getBool("FullPermSnapshots"))
		{
			perms = PERM_ALL;
		}

		LLResourceUploadInfo::ptr_t
			info(new LLResourceUploadInfo(tid, LLAssetType::AT_TEXTURE,
										  name, desc, 0,
										  LLFolderType::FT_SNAPSHOT_CATEGORY,
										  LLInventoryType::IT_SNAPSHOT, perms,
										  LLFloaterPerms::getGroupPerms(),
										  LLFloaterPerms::getEveryonePerms(),
										  expected_upload_cost));
		bool temp_upload = LLFloaterSnapshot::getInstance()->isTempAsset();
		upload_new_resource(info, NULL, NULL, temp_upload);

		gViewerWindowp->playSnapshotAnimAndSound();
	}
	else
	{
		gNotifications.add("ErrorEncodingSnapshot");
		llwarns << "Error encoding snapshot" << llendl;
	}

	gViewerStats.incStat(LLViewerStats::ST_SNAPSHOT_COUNT);

	mDataSize = 0;
}

//static
void LLSnapshotLivePreview::doSaveLocal(HBFileSelector::ESaveFilter type,
										std::string& filename, void* user_data)
{
	LLSnapshotLivePreview* self = (LLSnapshotLivePreview*)user_data;
	if (!self) return;

	LLFloaterSnapshot* floaterp = LLFloaterSnapshot::findInstance();
	if (!floaterp || self != floaterp->mLivePreview)
	{
		gNotifications.add("SnapshotAborted");
		return;
	}

	// Restore the frozen frame preview if we had to disable it for the UI file
	// selector
	if (floaterp->mFreezeFrameCheck->get())
	{
		floaterp->getParent()->setMouseOpaque(true);
		self->setVisible(true);
		self->setEnabled(true);
		self->setMouseOpaque(true);
		gToolMgr.setCurrentToolset(gCameraToolset);
	}

	if (!filename.empty())
	{
		if (!gViewerWindowp->isSnapshotLocSet())
		{
			gViewerWindowp->setSnapshotLoc(filename);
		}
		gViewerWindowp->saveImageNumbered(self->mFormattedImage);
	}

	// Relinquish image memory. Save button will be disabled as a side-effect.
	self->mFormattedImage = NULL;
	self->mDataSize = 0;
	self->updateSnapshot(false, false);

	if (gSavedSettings.getBool("CloseSnapshotOnKeep"))
	{
		floaterp->close();
	}
	else
	{
		bool autosnap = gSavedSettings.getBool("AutoSnapshot");
		self->updateSnapshot(autosnap, false,
							 autosnap ? AUTO_SNAPSHOT_TIME_DELAY : 0.f);
		floaterp->updateControls();
	}
}

void LLSnapshotLivePreview::saveLocal()
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
			mFormattedImage = NULL;
			mDataSize = 0;
			updateSnapshot(false, false);
			return;
	}
	std::string suggestion = gViewerWindowp->getSnapshotBaseName();
	if (gViewerWindowp->isSnapshotLocSet())
	{
		doSaveLocal(type, suggestion, this);
	}
	else
	{
		// Allow to interact with the UI file selector if in frozen frame mode
		LLFloaterSnapshot* floaterp = LLFloaterSnapshot::findInstance();
		if (floaterp && floaterp->mFreezeFrameCheck->get())
		{
			floaterp->getParent()->setMouseOpaque(false);
			setVisible(false);
			setEnabled(false);
			setMouseOpaque(false);
			gToolMgr.setCurrentToolset(gBasicToolset);
		}

		HBFileSelector::saveFile(type, suggestion, doSaveLocal, this);
	}
}

//-----------------------------------------------------------------------------
// Class LLFloaterSnapshot
//-----------------------------------------------------------------------------

// Helper functions

static const char* lastSnapshotWidthName()
{
	switch (gSavedSettings.getU32("LastSnapshotType"))
	{
		case LLSnapshotLivePreview::SNAPSHOT_POSTCARD:
			return "LastSnapshotToEmailWidth";

		case LLSnapshotLivePreview::SNAPSHOT_TEXTURE:
			return "LastSnapshotToInventoryWidth";

		case LLSnapshotLivePreview::SNAPSHOT_THUMBNAIL:
			return "LastSnapshotThumbnailWidth";

		default:
			return "LastSnapshotToDiskWidth";
	}
}

static const char* lastSnapshotHeightName()
{
	switch (gSavedSettings.getU32("LastSnapshotType"))
	{
		case LLSnapshotLivePreview::SNAPSHOT_POSTCARD:
			return "LastSnapshotToEmailHeight";

		case LLSnapshotLivePreview::SNAPSHOT_TEXTURE:
			return "LastSnapshotToInventoryHeight";

		case LLSnapshotLivePreview::SNAPSHOT_THUMBNAIL:
			return "LastSnapshotThumbnailHeight";

		default:
			return "LastSnapshotToDiskHeight";
	}
}

// Static methods to use to open, close or update the snapshot floater

//static
void LLFloaterSnapshot::show(void*)
{
	LLFloaterSnapshot* self = findInstance();
	if (!self)
	{
		self = getInstance();
		// Move snapshot floater to special purpose snapshotfloaterview
		gFloaterViewp->removeChild(self);
		gSnapshotFloaterViewp->addChild(self);

		self->updateLayout();
	}
	else
	{
		// Just refresh the snapshot in the existing floater instance
		self->mLivePreview->updateSnapshot(true);
	}

	self->open();
	self->focusFirstItem(false);
	gSnapshotFloaterViewp->setEnabled(true);
	gSnapshotFloaterViewp->setVisible(true);
	gSnapshotFloaterViewp->adjustToFitScreen(self);
}

//static
void LLFloaterSnapshot::hide(void*)
{
	LLFloaterSnapshot* self = findInstance();
	if (self && !self->isDead())
	{
		self->close();
	}
}

//static
void LLFloaterSnapshot::update()
{
	LLFloaterSnapshot* self = findInstance();
	if (self)
	{
		self->mLivePreview->onIdle();
		self->updateControls();
	}
}

// Floater methods proper

LLFloaterSnapshot::LLFloaterSnapshot(const LLSD&)
:	LLFloater("snapshot"),
	mLastToolset(NULL)
{
	// Create preview window
	mLivePreview = new LLSnapshotLivePreview(getRootView()->getRect());

	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_snapshot.xml",
												 NULL, false);
}

LLFloaterSnapshot::~LLFloaterSnapshot()
{
	delete mLivePreview;

	// Unfreeze everything else
	LLPipeline::sFreezeTime = false;

	if (mLastToolset)
	{
		gToolMgr.setCurrentToolset(mLastToolset);
	}

	// Unpause avatars
	mAvatarPauseHandles.clear();
}

//virtual
bool LLFloaterSnapshot::postBuild()
{
	mSnapshotTypeRadio = getChild<LLRadioGroup>("snapshot_type_radio");
	mSnapshotTypeRadio->setCommitCallback(onCommitSnapshotType);
	mSnapshotTypeRadio->setCallbackUserData(this);

	childSetAction("new_snapshot_btn", onClickNewSnapshot, this);

	mMoreButton = getChild<LLButton>("more_btn");
	mMoreButton->setClickedCallback(onClickMore, this);

	mLessButton = getChild<LLButton>("less_btn");
	mLessButton->setClickedCallback(onClickLess, this);

	mUploadButton = getChild<LLButton>("upload_btn");
	mUploadButton->setClickedCallback(onClickKeep, this);

	mSendButton = getChild<LLButton>("send_btn");
	mSendButton->setClickedCallback(onClickKeep, this);

	mSaveButton = getChild<LLFlyoutButton>("save_btn");
	mSaveButton->setCommitCallback(onCommitSave);
	mSaveButton->setCallbackUserData(this);

	childSetAction("discard_btn", onClickDiscard, this);

	mImageQualitySlider = getChild<LLSliderCtrl>("image_quality_slider");
	mImageQualitySlider->setCommitCallback(onCommitQuality);
	mImageQualitySlider->setCallbackUserData(this);
	mImageQualitySlider->setValue(gSavedSettings.getS32("SnapshotQuality"));

	mImageWidthSpinner = getChild<LLSpinCtrl>("snapshot_width");
	mImageWidthSpinner->setCommitCallback(onCommitCustomResolution);
	mImageWidthSpinner->setCallbackUserData(this);

	mImageHeightSpinner = getChild<LLSpinCtrl>("snapshot_height");
	mImageHeightSpinner->setCommitCallback(onCommitCustomResolution);
	mImageHeightSpinner->setCallbackUserData(this);
	mImageWidthSpinner->setValue(gSavedSettings.getU32(lastSnapshotWidthName()));
	mImageHeightSpinner->setValue(gSavedSettings.getU32(lastSnapshotHeightName()));

	mUICheck = getChild<LLCheckBoxCtrl>("ui_check");
	mUICheck->setCommitCallback(onClickUICheck);
	mUICheck->setCallbackUserData(this);

	mHUDCheck = getChild<LLCheckBoxCtrl>("hud_check");
	mHUDCheck->setCommitCallback(onClickHUDCheck);
	mHUDCheck->setCallbackUserData(this);
//MK
	if (gRLenabled && gRLInterface.mHasLockedHuds)
	{
		gSavedSettings.setBool("RenderHUDInSnapshot", true);
		mHUDCheck->setEnabled(false);
	}
//mk

	mKeepAspectCheck = getChild<LLCheckBoxCtrl>("keep_aspect_check");
	mKeepAspectCheck->setCommitCallback(onClickKeepAspectCheck);
	mKeepAspectCheck->setCallbackUserData(this);

	mAutoCloseCheck = getChild<LLCheckBoxCtrl>("close_after_check");

	mLayerTypeCombo = getChild<LLComboBox>("layer_types");
	mLayerTypeCombo->setCommitCallback(onCommitLayerTypes);
	mLayerTypeCombo->setCallbackUserData(this);
	mLayerTypeCombo->setValue("colors");

	mFreezeFrameCheck = getChild<LLCheckBoxCtrl>("freeze_frame_check");
	mFreezeFrameCheck->setCommitCallback(onCommitFreezeFrame);
	mFreezeFrameCheck->setCallbackUserData(this);

	mAutoSnapCheck = getChild<LLCheckBoxCtrl>("auto_snapshot_check");
	mAutoSnapCheck->setCommitCallback(onClickAutoSnap);
	mAutoSnapCheck->setCallbackUserData(this);

	mTempCheck = getChild<LLCheckBoxCtrl>("temp_check");

	mPostcardSizeCombo = getChild<LLComboBox>("postcard_size_combo");
	mPostcardSizeCombo->setCommitCallback(onCommitResolution);
	mPostcardSizeCombo->setCallbackUserData(this);

	mTextureSizeCombo = getChild<LLComboBox>("texture_size_combo");
	mTextureSizeCombo->setCommitCallback(onCommitResolution);
	mTextureSizeCombo->setCallbackUserData(this);

	mLocalSizeCombo = getChild<LLComboBox>("local_size_combo");
	mLocalSizeCombo->setCommitCallback(onCommitResolution);
	mLocalSizeCombo->setCallbackUserData(this);

	mThumbnailSizeCombo = getChild<LLComboBox>("thumbnail_size_combo");
	mThumbnailSizeCombo->setCommitCallback(onCommitResolution);
	mThumbnailSizeCombo->setCallbackUserData(this);

	mLocalFormatCombo = getChild<LLComboBox>("local_format_combo");
	mLocalFormatCombo->setCommitCallback(onCommitSnapshotFormat);
	mLocalFormatCombo->setCallbackUserData(this);

	mFileSizeLabel = getChild<LLTextBox>("file_size_label");
	mTypeLabel = getChild<LLTextBox>("type_label2");
	mFormatLabel = getChild<LLTextBox>("format_label");
	mLayerLabel = getChild<LLTextBox>("layer_type_label");

	// Make sure preview is below snapshot floater
	getRootView()->removeChild(gSnapshotFloaterViewp);
	getRootView()->addChild(mLivePreview);
	getRootView()->addChild(gSnapshotFloaterViewp);

	updateControls();

	return true;
}

//virtual
void LLFloaterSnapshot::draw()
{
	if (mLivePreview->isSnapshotActive() || mLivePreview->getThumbnailLock())
	{
		// Do not render snapshot window in snapshot, even if "show ui" is
		// turned on
		return;
	}

	LLFloater::draw();

	if (mLivePreview->getThumbnailImage())
	{
		S32 offset_x = (getRect().getWidth() -
						mLivePreview->getThumbnailWidth()) / 2;
		S32 offset_y = getRect().getHeight() - 205 +
					   (90 - mLivePreview->getThumbnailHeight()) / 2;

		gGL.matrixMode(LLRender::MM_MODELVIEW);
		gl_draw_scaled_image(offset_x, offset_y,
							 mLivePreview->getThumbnailWidth(),
							 mLivePreview->getThumbnailHeight(),
							 mLivePreview->getThumbnailImage(),
							 LLColor4::white);

		mLivePreview->drawPreviewRect(offset_x, offset_y);
	}
}

void LLFloaterSnapshot::onClose(bool app_quitting)
{
	if (gSavedSettings.getU32("LastSnapshotType") ==
			LLSnapshotLivePreview::SNAPSHOT_THUMBNAIL)
	{
		gSavedSettings.setU32("LastSnapshotType", sSavedLastSelectedType);
		if (gSavedSettings.getBool("ThumbnailSnapshotFrontView"))
		{
			gSavedSettings.setBool("CameraFrontView", false);
		}
	}
	if (gSnapshotFloaterViewp)
	{
		gSnapshotFloaterViewp->setEnabled(false);
		// Set invisible so it does not eat tooltips. JC
		gSnapshotFloaterViewp->setVisible(false);
	}
	destroy();
}

S32 LLFloaterSnapshot::getTypeIndex()
{
	S32 index = LLSnapshotLivePreview::SNAPSHOT_POSTCARD;

	std::string id = mSnapshotTypeRadio->getValue().asString();
	if (id == "postcard")
	{
		index = LLSnapshotLivePreview::SNAPSHOT_POSTCARD;
	}
	else if (id == "texture")
	{
		index = LLSnapshotLivePreview::SNAPSHOT_TEXTURE;
	}
	else if (id == "local")
	{
		index = LLSnapshotLivePreview::SNAPSHOT_LOCAL;
	}
	else if (id == "thumbnail")
	{
		index = LLSnapshotLivePreview::SNAPSHOT_THUMBNAIL;
	}

	return index;
}

U32 LLFloaterSnapshot::getFormatIndex()
{
	U32 index = SNAPSHOT_FORMAT_PNG;

	std::string id = mLocalFormatCombo->getValue().asString();
	if (id == "PNG")
	{
		index = SNAPSHOT_FORMAT_PNG;
	}
	else if (id == "JPEG")
	{
		index = SNAPSHOT_FORMAT_JPEG;
	}
	else if (id == "BMP")
	{
		index = SNAPSHOT_FORMAT_BMP;
	}

	return index;
}

U32 LLFloaterSnapshot::getLayerType()
{
	U32 type = LLViewerWindow::SNAPSHOT_TYPE_COLOR;

	std::string id = mLayerTypeCombo->getValue().asString();
	if (id == "colors")
	{
		type = LLViewerWindow::SNAPSHOT_TYPE_COLOR;
	}
	else if (id == "depth")
	{
		type = LLViewerWindow::SNAPSHOT_TYPE_DEPTH;
//MK
		// When the vision is restricted, do not render depth since it would
		// allow cheating through the vision spheres
		if (gRLenabled && gRLInterface.mVisionRestricted)
		{
			type = LLViewerWindow::SNAPSHOT_TYPE_COLOR;
		}
//mk
	}
	return type;
}

void LLFloaterSnapshot::checkAspectRatio(S32 index)
{
	// Do not round texture sizes; textures are commonly stretched in world,
	// profiles, etc and need to be "squashed" during upload, not cropped
	// here. HB
#if 0
	if (LLSnapshotLivePreview::SNAPSHOT_TEXTURE == getTypeIndex())
	{
		mLivePreview->mKeepAspectRatio = false;
		return;
	}
#endif

	if (index == 0)			// Current window size
	{
		mImageWidthSpinner->setAllowEdit(false);
		mImageHeightSpinner->setAllowEdit(false);
		sAspectRatioCheckOff = true;
		mKeepAspectCheck->setEnabled(false);
		mLivePreview->mKeepAspectRatio = true;
	}
	else if (index == -1)	// Custom size
	{
		mImageWidthSpinner->setAllowEdit(true);
		mImageHeightSpinner->setAllowEdit(true);
		sAspectRatioCheckOff = false;
#if 0
		if (LLSnapshotLivePreview::SNAPSHOT_TEXTURE !=
				gSavedSettings.getU32("LastSnapshotType"))
#endif
		{
			mKeepAspectCheck->setEnabled(true);

			mLivePreview->mKeepAspectRatio =
				gSavedSettings.getBool("KeepAspectForSnapshot");
		}
	}
	else
	{
		mImageWidthSpinner->setAllowEdit(false);
		mImageHeightSpinner->setAllowEdit(false);
		sAspectRatioCheckOff = true;
		mKeepAspectCheck->setEnabled(false);

		mLivePreview->mKeepAspectRatio = false;
	}
}

void LLFloaterSnapshot::resetSnapshotSizeOnUI(S32 width, S32 height)
{
	mImageWidthSpinner->forceSetValue(width);
	mImageHeightSpinner->forceSetValue(height);
	gSavedSettings.setU32(lastSnapshotWidthName(), width);
	gSavedSettings.setU32(lastSnapshotHeightName(), height);
}

// Sets the size combo to "custom" mode.
void LLFloaterSnapshot::comboSetCustom(LLComboBox* combop)
{
	if (combop == mThumbnailSizeCombo)
	{
		// No custom mode for inventory thumbnails: select 256x256 by default
		gSavedSettings.setS32("SnapshotLocalLastResolution", 0);
		return;
	}

	// "custom" is always the last index in all other combos
	combop->setCurrentByIndex(combop->getItemCount() - 1);

	if (combop == mPostcardSizeCombo)
	{
		gSavedSettings.setS32("SnapshotPostcardLastResolution",
							  combop->getCurrentIndex());
	}
	else if (combop == mTextureSizeCombo)
	{
		gSavedSettings.setS32("SnapshotTextureLastResolution",
							  combop->getCurrentIndex());
	}
	else if (combop == mLocalSizeCombo)
	{
		gSavedSettings.setS32("SnapshotLocalLastResolution",
							  combop->getCurrentIndex());
	}

	checkAspectRatio(-1); // -1 means custom
}

void LLFloaterSnapshot::updateLayout()
{
	S32 delta_height = 0;
	static LLCachedControl<bool> is_advance(gSavedSettings, "AdvanceSnapshot");
	if (!is_advance)
	{
		delta_height = getUIWinHeightShort() - getUIWinHeightLong();
	}

	if (!is_advance)
	{
		// Set to original window resolution
		mLivePreview->mKeepAspectRatio = true;

		mPostcardSizeCombo->setCurrentByIndex(0);
		gSavedSettings.setS32("SnapshotPostcardLastResolution", 0);

		mTextureSizeCombo->setCurrentByIndex(0);
		gSavedSettings.setS32("SnapshotTextureLastResolution", 0);

		mLocalSizeCombo->setCurrentByIndex(0);
		gSavedSettings.setS32("SnapshotLocalLastResolution", 0);

		mThumbnailSizeCombo->setCurrentByIndex(0);
		gSavedSettings.setS32("SnapshotThumbnailLastResolution", 0);

		mLivePreview->setSize(gViewerWindowp->getWindowDisplayWidth(),
							  gViewerWindowp->getWindowDisplayHeight());
	}

	if (mFreezeFrameCheck->get())
	{
		// Stop all mouse events at fullscreen preview layer
		getParent()->setMouseOpaque(true);

		// Shrink to smaller layout
		reshape(getRect().getWidth(), getUIWinHeightLong() + delta_height);

		// Can see and interact with fullscreen preview now
		mLivePreview->setVisible(true);
		mLivePreview->setEnabled(true);

		// RN: freeze all avatars
		for (S32 i = 0, count = LLCharacter::sInstances.size(); i < count; ++i)
		{
			LLCharacter* charp = LLCharacter::sInstances[i];
			mAvatarPauseHandles.emplace_back(charp->requestPause());
		}

		// Freeze everything else
		LLPipeline::sFreezeTime = true;

		if (gToolMgr.getCurrentToolset() != gCameraToolset)
		{
			mLastToolset = gToolMgr.getCurrentToolset();
			gToolMgr.setCurrentToolset(gCameraToolset);
		}
	}
	else // Turning off freeze frame mode
	{
		getParent()->setMouseOpaque(false);
		reshape(getRect().getWidth(), getUIWinHeightLong() + delta_height);
		mLivePreview->setVisible(false);
		mLivePreview->setEnabled(false);

		// RN: thaw all avatars
		mAvatarPauseHandles.clear();

		// Thaw everything else
		LLPipeline::sFreezeTime = false;

		// Restore last tool (e.g. pie menu, etc)
		if (mLastToolset)
		{
			gToolMgr.setCurrentToolset(mLastToolset);
		}
	}
}

void LLFloaterSnapshot::setupForInventoryThumbnail(const LLUUID& inv_obj_id)
{
	mInventoryObjectId = inv_obj_id;
	sSavedLastSelectedType = gSavedSettings.getU32("LastSnapshotType");
	gSavedSettings.setU32("LastSnapshotType",
						  LLSnapshotLivePreview::SNAPSHOT_THUMBNAIL);
	if (gSavedSettings.getBool("ThumbnailSnapshotFrontView"))
	{
		gSavedSettings.setBool("CameraFrontView", true);
	}
	updateControls();
}

// This is the main function that keeps all the UI controls in sync with the
// saved settings. It should be called anytime a setting is changed that could
// affect the controls. No other methods should be changing any of the controls
// directly except for helpers called by this method. The basic pattern for
// programmatically changing the UI settings is to first set the appropriate
// saved settings and then call this method to sync the UI with them.
void LLFloaterSnapshot::updateControls()
{
	static LLCachedControl<U32> snap_type(gSavedSettings, "LastSnapshotType");
	mSnapshotTypeRadio->setSelectedIndex(snap_type);
	U32 shot_type = getTypeIndex();
	bool is_thumbnail = shot_type == LLSnapshotLivePreview::SNAPSHOT_THUMBNAIL;
	bool is_texture = shot_type == LLSnapshotLivePreview::SNAPSHOT_TEXTURE;
	bool is_postcard = shot_type == LLSnapshotLivePreview::SNAPSHOT_POSTCARD;
	bool is_local = shot_type == LLSnapshotLivePreview::SNAPSHOT_LOCAL;

	for (U32 i = 0; i < LLSnapshotLivePreview::SNAPSHOT_TOTAL_TYPES; ++i)
	{
		LLRadioCtrl* buttonp = mSnapshotTypeRadio->getRadioButton(i);
		if (i == LLSnapshotLivePreview::SNAPSHOT_THUMBNAIL)
		{
			buttonp->setVisible(is_thumbnail);
		}
		else
		{
			buttonp->setVisible(!is_thumbnail);
		}
	}

	static LLCachedControl<S32> postcard_res(gSavedSettings,
											 "SnapshotPostcardLastResolution");
	mPostcardSizeCombo->selectNthItem(postcard_res);

	static LLCachedControl<S32> texture_res(gSavedSettings,
											"SnapshotTextureLastResolution");
	mTextureSizeCombo->selectNthItem(texture_res);

	static LLCachedControl<S32> local_res(gSavedSettings,
										  "SnapshotLocalLastResolution");
	mLocalSizeCombo->selectNthItem(local_res);

	static LLCachedControl<S32> thumbnail_res(gSavedSettings,
											  "SnapshotThumbnailLastResolution");
	mThumbnailSizeCombo->selectNthItem(thumbnail_res);

	static LLCachedControl<U32> format(gSavedSettings, "SnapshotFormat");
	bool is_jpeg = format == LLFloaterSnapshot::SNAPSHOT_FORMAT_JPEG;
	mLocalFormatCombo->selectNthItem(format);

	mUploadButton->setVisible(is_texture || is_thumbnail);
	mSendButton->setVisible(is_postcard);
	mSaveButton->setVisible(is_local);
	mKeepAspectCheck->setEnabled(!is_texture && !is_thumbnail &&
								 !sAspectRatioCheckOff);
	mLayerTypeCombo->setEnabled(is_local);

	bool has_temp_upload = !is_thumbnail;
	if (has_temp_upload)
	{
		LLViewerRegion* regionp = gAgent.getRegion();
		has_temp_upload = regionp && regionp->getCentralBakeVersion() == 0;
	}
	if (!has_temp_upload || !is_texture)
	{
		mTempCheck->setValue(false);
	}
	mTempCheck->setEnabled(is_texture);

	static LLCachedControl<bool> is_advance(gSavedSettings, "AdvanceSnapshot");
	mMoreButton->setVisible(!is_advance);
 	mLessButton->setVisible(is_advance);
	mTypeLabel->setVisible(is_advance);
	mFormatLabel->setVisible(is_advance && is_local);
	mLocalFormatCombo->setVisible(is_advance && is_local);
	mLayerTypeCombo->setVisible(is_advance);
	mLayerLabel->setVisible(is_advance);
	mImageWidthSpinner->setVisible(is_advance);
	mImageHeightSpinner->setVisible(is_advance);
	mKeepAspectCheck->setVisible(is_advance);
	mUICheck->setVisible(is_advance);
	mHUDCheck->setVisible(is_advance);
	mAutoCloseCheck->setVisible(is_advance && !is_thumbnail);
	mFreezeFrameCheck->setVisible(is_advance);
	mAutoSnapCheck->setVisible(is_advance);
	mImageQualitySlider->setVisible(is_advance && !is_thumbnail &&
									(is_postcard || (is_local && is_jpeg)));

	bool got_bytes = mLivePreview->getDataSize() > 0;
	bool got_snap = mLivePreview->getSnapshotUpToDate();

	S32 data_size = mLivePreview->getDataSize();
	bool postcard_sized = data_size <= MAX_POSTCARD_DATASIZE;
	mSendButton->setEnabled(got_snap && is_postcard && postcard_sized);
	mUploadButton->setEnabled((is_texture || is_thumbnail) && got_snap);
	mSaveButton->setEnabled(is_local && got_snap);

	LLLocale locale(LLLocale::USER_LOCALE);
	if (got_snap)
	{
		std::string bytes_string;
		LLLocale::getIntegerString(bytes_string, data_size >> 10);
		mFileSizeLabel->setTextArg("[SIZE]", bytes_string);
	}
	else
	{
		static std::string unknown = getString("unknown");
		mFileSizeLabel->setTextArg("[SIZE]", unknown);
	}
	mFileSizeLabel->setColor(got_bytes && is_postcard && !postcard_sized ?
								LLColor4::red : LLUI::sLabelTextColor);

	S32 upload_cost = LLEconomy::getInstance()->getTextureUploadCost();
	childSetLabelArg("texture", "[AMOUNT]", llformat("%d", upload_cost));
	if (is_thumbnail)
	{
		mUploadButton->setLabelArg("[AMOUNT]", "0");
	}
	else
	{
		mUploadButton->setLabelArg("[AMOUNT]", llformat("%d", upload_cost));
	}

	mTempCheck->setVisible(has_temp_upload && is_advance && upload_cost > 0);

//MK
	if (gRLenabled && gRLInterface.mHasLockedHuds)
	{
		gSavedSettings.setBool("RenderHUDInSnapshot", true);
		mHUDCheck->setEnabled(false);
	}
	else
	{
		mHUDCheck->setEnabled(true);
	}
//mk

	U32 layer_type = LLViewerWindow::SNAPSHOT_TYPE_COLOR;

	switch (shot_type)
	{
		case LLSnapshotLivePreview::SNAPSHOT_POSTCARD:
			mLayerTypeCombo->setValue("colors");
			mTextureSizeCombo->setVisible(false);
			mLocalSizeCombo->setVisible(false);
			mThumbnailSizeCombo->setVisible(false);
			mPostcardSizeCombo->setVisible(is_advance);
			if (is_advance)
			{
				updateResolution(mPostcardSizeCombo, this, false);
			}
			break;

		case LLSnapshotLivePreview::SNAPSHOT_TEXTURE:
			mLayerTypeCombo->setValue("colors");
			mPostcardSizeCombo->setVisible(false);
			mLocalSizeCombo->setVisible(false);
			mThumbnailSizeCombo->setVisible(false);
			mTextureSizeCombo->setVisible(is_advance);
			if (is_advance)
			{
				updateResolution(mTextureSizeCombo, this, false);
			}
			break;

		case LLSnapshotLivePreview::SNAPSHOT_LOCAL:
			layer_type = getLayerType();
			mPostcardSizeCombo->setVisible(false);
			mTextureSizeCombo->setVisible(false);
			mThumbnailSizeCombo->setVisible(false);
			mLocalSizeCombo->setVisible(is_advance);
			if (is_advance)
			{
				updateResolution(mLocalSizeCombo, this, false);
			}
			break;

		case LLSnapshotLivePreview::SNAPSHOT_THUMBNAIL:
			mLayerTypeCombo->setValue("colors");
			mPostcardSizeCombo->setVisible(false);
			mTextureSizeCombo->setVisible(false);
			mLocalSizeCombo->setVisible(false);
			mThumbnailSizeCombo->setVisible(is_advance);
			if (is_advance)
			{
				updateResolution(mThumbnailSizeCombo, this, false);
			}
			break;

		default:
			break;
	}

	mLivePreview->setSnapshotType(shot_type);
	mLivePreview->setSnapshotFormat(format);
	mLivePreview->setSnapshotBufferType(layer_type);
}

bool LLFloaterSnapshot::isTempAsset() const
{
	return mTempCheck->getVisible() && mTempCheck->getEnabled() &&
		   mTempCheck->get();
}

//static
void LLFloaterSnapshot::onClickDiscard(void* data)
{
	LLFloaterSnapshot* self = (LLFloaterSnapshot*)data;
	if (self)
	{
		self->close();
	}
}

//static
void LLFloaterSnapshot::onCommitSave(LLUICtrl* ctrl, void* data)
{
	if (!ctrl || !data) return;

	if (ctrl->getValue().asString() == "save as")
	{
		gViewerWindowp->resetSnapshotLoc();
	}
	onClickKeep(data);
}

//static
void LLFloaterSnapshot::onClickKeep(void* data)
{
	LLFloaterSnapshot* self = (LLFloaterSnapshot*)data;
	if (!self) return;

	bool close = gSavedSettings.getBool("CloseSnapshotOnKeep");
	U32 type = self->mLivePreview->getSnapshotType();
	if (type == LLSnapshotLivePreview::SNAPSHOT_THUMBNAIL)
	{
		HBFloaterThumbnail::uploadThumbnail(self->mInventoryObjectId,
											self->mLivePreview->getPreviewImage());
		close = true;
	}
	else if (type == LLSnapshotLivePreview::SNAPSHOT_POSTCARD)
	{
		LLFloaterPostcard* floaterp = self->mLivePreview->savePostcard();
		// If still in snapshot mode, put postcard floater in snapshot
		// floaterview and link it to snapshot floater
		if (floaterp && !close)
		{
			gFloaterViewp->removeChild(floaterp);
			gSnapshotFloaterViewp->addChild(floaterp);
			self->addDependentFloater(floaterp, false);
		}
	}
	else if (type == LLSnapshotLivePreview::SNAPSHOT_TEXTURE)
	{
		self->mLivePreview->saveTexture();
	}
	else
	{
		self->mLivePreview->saveLocal();
		return;
	}

	if (close)
	{
		self->close();
	}
	else
	{
		self->mLivePreview->checkAutoSnapshot();
		self->updateControls();
	}
}

//static
void LLFloaterSnapshot::onClickNewSnapshot(void* data)
{
	LLFloaterSnapshot* self = (LLFloaterSnapshot*)data;
	if (self)
	{
		self->mLivePreview->updateSnapshot(true);
	}
}

//static
void LLFloaterSnapshot::onClickAutoSnap(LLUICtrl*, void* data)
{
	LLFloaterSnapshot* self = (LLFloaterSnapshot*)data;
	if (self)
	{
		self->mLivePreview->checkAutoSnapshot(true);
		self->updateControls();
	}
}

//static
void LLFloaterSnapshot::onClickMore(void* data)
{
	gSavedSettings.setBool("AdvanceSnapshot", true);

	LLFloaterSnapshot* self = (LLFloaterSnapshot*)data;
	if (!self) return;

	self->translate(0,
					self->getUIWinHeightShort() - self->getUIWinHeightLong());
	self->reshape(self->getRect().getWidth(), self->getUIWinHeightLong());
	self->updateControls();
	self->updateLayout();

	self->mLivePreview->setThumbnailImageSize();
}

//static
void LLFloaterSnapshot::onClickLess(void* data)
{
	gSavedSettings.setBool("AdvanceSnapshot", false);

	LLFloaterSnapshot* self = (LLFloaterSnapshot*)data;
	if (!self) return;

	self->translate(0,
					self->getUIWinHeightLong() - self->getUIWinHeightShort());
	self->reshape(self->getRect().getWidth(), self->getUIWinHeightShort());
	self->updateControls();
	self->updateLayout();

	self->mLivePreview->setThumbnailImageSize();
}

//static
void LLFloaterSnapshot::onClickUICheck(LLUICtrl*, void* data)
{
	LLFloaterSnapshot* self = (LLFloaterSnapshot*)data;
	if (self)
	{
		self->mLivePreview->checkAutoSnapshot(true);
		self->updateControls();
	}
}

//static
void LLFloaterSnapshot::onClickHUDCheck(LLUICtrl*, void* data)
{
//MK
	if (gRLenabled && gRLInterface.mHasLockedHuds)
	{
		gSavedSettings.setBool("RenderHUDInSnapshot", true);
	}
//mk

	LLFloaterSnapshot* self = (LLFloaterSnapshot*)data;
	if (self)
	{
		self->mLivePreview->checkAutoSnapshot(true);
		self->updateControls();
	}
}

//static
void LLFloaterSnapshot::onClickKeepAspectCheck(LLUICtrl* ctrl, void* data)
{
	if (!ctrl || !data) return;

	LLFloaterSnapshot* self = (LLFloaterSnapshot*)data;
	LLCheckBoxCtrl* check = (LLCheckBoxCtrl*)ctrl;
	self->mLivePreview->mKeepAspectRatio = check->get();

	S32 w, h;
	self->mLivePreview->getSize(w, h);
	if (self->mLivePreview->checkImageSize(w, h))
	{
		LLFloaterSnapshot* self = (LLFloaterSnapshot*)data;
		self->resetSnapshotSizeOnUI(w, h);
	}
	self->mLivePreview->setSize(w, h);

	self->mLivePreview->updateSnapshot(false, true);
	self->mLivePreview->checkAutoSnapshot(true);
}

//static
void LLFloaterSnapshot::onCommitQuality(LLUICtrl* ctrl, void* data)
{
	if (!ctrl || !data) return;

	LLFloaterSnapshot* self = (LLFloaterSnapshot*)data;
	LLSliderCtrl* slider = (LLSliderCtrl*)ctrl;
	S32 quality_val = llfloor((F32)slider->getValue().asReal());

	self->mLivePreview->setSnapshotQuality(quality_val);
	self->mLivePreview->checkAutoSnapshot(true);
}

//static
void LLFloaterSnapshot::onCommitFreezeFrame(LLUICtrl*, void* data)
{
	LLFloaterSnapshot* self = (LLFloaterSnapshot*)data;
	if (self)
	{
		self->updateLayout();
		self->mLivePreview->checkAutoSnapshot(true);
	}
}

//static
void LLFloaterSnapshot::updateResolution(LLUICtrl* ctrl, void* data,
										 bool do_update)
{
	if (!data || !ctrl) return;

	LLComboBox* combobox = (LLComboBox*)ctrl;
	LLFloaterSnapshot* self = (LLFloaterSnapshot*)data;

	// Save off all selected resolution values
	gSavedSettings.setS32("SnapshotPostcardLastResolution",
						  self->mPostcardSizeCombo->getCurrentIndex());
	gSavedSettings.setS32("SnapshotTextureLastResolution",
						  self->mTextureSizeCombo->getCurrentIndex());
	gSavedSettings.setS32("SnapshotLocalLastResolution",
						  self->mLocalSizeCombo->getCurrentIndex());
	gSavedSettings.setS32("SnapshotThumbnailLastResolution",
						  self->mThumbnailSizeCombo->getCurrentIndex());

	std::string sdstring = combobox->getSelectedValue();
	LLSD sdres;
	std::stringstream sstream(sdstring);
	LLSDSerialize::fromNotation(sdres, sstream, sdstring.size());

	S32 width = sdres[0];
	S32 height = sdres[1];

	if (combobox->getCurrentIndex() >= 0)
	{
		S32 original_width = 0 , original_height = 0;
		self->mLivePreview->getSize(original_width, original_height);

		if (width == 0 || height == 0)
		{
			// Take resolution from current window size
			self->mLivePreview->setSize(gViewerWindowp->getWindowDisplayWidth(),
										gViewerWindowp->getWindowDisplayHeight());
		}
		else if (width == -1 || height == -1)
		{
			// Load last custom value
			self->mLivePreview->setSize(gSavedSettings.getU32(lastSnapshotWidthName()),
										gSavedSettings.getU32(lastSnapshotHeightName()));
		}
		else
		{
			// Use the resolution from the selected pre-canned drop-down choice
			self->mLivePreview->setSize(width, height);
		}

		self->checkAspectRatio(width);

		self->mLivePreview->getSize(width, height);

		if (self->mLivePreview->checkImageSize(width, height))
		{
			self->resetSnapshotSizeOnUI(width, height);
		}

		if (self->mImageWidthSpinner->getValue().asInteger() != width ||
			self->mImageHeightSpinner->getValue().asInteger() != height)
		{
			self->mImageWidthSpinner->setValue(width);
			self->mImageHeightSpinner->setValue(height);
		}

		if (original_width != width || original_height != height)
		{
			self->mLivePreview->setSize(width, height);

			// Hide old preview as the aspect ratio could be wrong
			self->mLivePreview->checkAutoSnapshot();

			self->mLivePreview->updateSnapshot(false, true);
			if (do_update)
			{
				self->updateControls();
			}
		}
	}
}

//static
void LLFloaterSnapshot::onCommitLayerTypes(LLUICtrl* ctrl, void* data)
{
	if (!data || !ctrl) return;

	LLFloaterSnapshot* self = (LLFloaterSnapshot*)data;
	LLComboBox* combo = (LLComboBox*)ctrl;

	U32 type = combo->getCurrentIndex();
	self->mLivePreview->setSnapshotBufferType(type);

	self->mLivePreview->checkAutoSnapshot(true);
}

//static
void LLFloaterSnapshot::onCommitSnapshotType(LLUICtrl* ctrl, void* data)
{
	LLFloaterSnapshot* self = (LLFloaterSnapshot*)data;
	if (self)
	{
		gSavedSettings.setU32("LastSnapshotType", self->getTypeIndex());
		self->mLivePreview->updateSnapshot(true);
		self->updateControls();
	}
}

//static
void LLFloaterSnapshot::onCommitSnapshotFormat(LLUICtrl* ctrl, void* data)
{
	LLFloaterSnapshot* self = (LLFloaterSnapshot*)data;
	if (self)
	{
		gSavedSettings.setU32("SnapshotFormat", self->getFormatIndex());
		self->mLivePreview->updateSnapshot(true);
		self->updateControls();
	}
}

//static
void LLFloaterSnapshot::onCommitCustomResolution(LLUICtrl* ctrl, void* data)
{
	if (!ctrl || !data) return;

	LLFloaterSnapshot* self = (LLFloaterSnapshot*)data;

	S32 w = llfloor((F32)self->mImageWidthSpinner->getValue().asReal());
	S32 h = llfloor((F32)self->mImageHeightSpinner->getValue().asReal());

	S32 curw, curh;
	self->mLivePreview->getSize(curw, curh);
	if (w != curw || h != curh)
	{
		bool update_ = false;

#if 0	// Do not round texture sizes; textures are commonly stretched in
		// world, profiles, etc and need to be "squashed" during upload, not
		// cropped here
		// If to upload a snapshot, process spinner input in a special way.
		if (LLSnapshotLivePreview::SNAPSHOT_TEXTURE ==
				gSavedSettings.getU32("LastSnapshotType"))
		{
			S32 spinner_increment = (S32)((LLSpinCtrl*)ctrl)->getIncrement();
			S32 dw = w - curw;
			S32 dh = h - curh;
			dw = dw == spinner_increment ? 1 : (dw == -spinner_increment ? -1
																		 : 0);
			dh = dh == spinner_increment ? 1 : (dh == -spinner_increment ? -1
																		 : 0);
			if (dw)
			{
				w = dw > 0 ? curw << dw : curw >> -dw;
				update_ = true;
			}
			if (dh)
			{
				h = dh > 0 ? curh << dh : curh >> -dh;
				update_ = true;
			}
		}
#endif
		self->mLivePreview->setMaxImageSize((S32)((LLSpinCtrl*)ctrl)->getMaxValue());

		// Check image size changes the value of height and width
		if (update_ || self->mLivePreview->checkImageSize(w, h, w != curw))
		{
			self->resetSnapshotSizeOnUI(w, h);
		}

		self->mLivePreview->setSize(w, h);
		self->mLivePreview->checkAutoSnapshot();
		self->mLivePreview->updateSnapshot(false, true);
		self->comboSetCustom(self->mPostcardSizeCombo);
		self->comboSetCustom(self->mTextureSizeCombo);
		self->comboSetCustom(self->mLocalSizeCombo);
		self->comboSetCustom(self->mThumbnailSizeCombo);
	}

	gSavedSettings.setU32(lastSnapshotWidthName(), w);
	gSavedSettings.setU32(lastSnapshotHeightName(), h);

	self->updateControls();
}

//-----------------------------------------------------------------------------
// Class LLSnapshotFloaterView
//-----------------------------------------------------------------------------

LLSnapshotFloaterView::LLSnapshotFloaterView(const std::string& name,
											 const LLRect& rect)
:	LLFloaterView(name, rect)
{
	setMouseOpaque(true);
	setEnabled(false);
}

bool LLSnapshotFloaterView::handleKey(KEY key, MASK mask,
									  bool called_from_parent)
{
	// Use the default handler when not in freeze-frame mode or when the file
	// selector is open.
	if (!LLPipeline::sFreezeTime || HBFileSelector::isInUse())
	{
		return LLFloaterView::handleKey(key, mask, called_from_parent);
	}

	if (called_from_parent)
	{
		// Pass all keystrokes down
		LLFloaterView::handleKey(key, mask, called_from_parent);
	}
	else
	{
		// Bounce keystrokes back down
		LLFloaterView::handleKey(key, mask, true);
	}
	return true;
}

bool LLSnapshotFloaterView::handleMouseDown(S32 x, S32 y, MASK mask)
{
	// Use the default handler when not in freeze-frame mode or when the file
	// selector is open.
	if (!LLPipeline::sFreezeTime || HBFileSelector::isInUse())
	{
		return LLFloaterView::handleMouseDown(x, y, mask);
	}
	// give floater a change to handle mouse, else camera tool
	if (childrenHandleMouseDown(x, y, mask) == NULL)
	{
		gToolMgr.getCurrentTool()->handleMouseDown(x, y, mask);
	}
	return true;
}

bool LLSnapshotFloaterView::handleMouseUp(S32 x, S32 y, MASK mask)
{
	// Use the default handler when not in freeze-frame mode or when the file
	// selector is open.
	if (!LLPipeline::sFreezeTime || HBFileSelector::isInUse())
	{
		return LLFloaterView::handleMouseUp(x, y, mask);
	}
	// give floater a change to handle mouse, else camera tool
	if (childrenHandleMouseUp(x, y, mask) == NULL)
	{
		gToolMgr.getCurrentTool()->handleMouseUp(x, y, mask);
	}
	return true;
}

bool LLSnapshotFloaterView::handleHover(S32 x, S32 y, MASK mask)
{
	// Use the default handler when not in freeze-frame mode or when the file
	// selector is open.
	if (!LLPipeline::sFreezeTime || HBFileSelector::isInUse())
	{
		return LLFloaterView::handleHover(x, y, mask);
	}
	// Give the floater a change to handle the mouse, else to the camera tool
	if (childrenHandleHover(x, y, mask) == NULL)
	{
		gToolMgr.getCurrentTool()->handleHover(x, y, mask);
	}
	return true;
}
