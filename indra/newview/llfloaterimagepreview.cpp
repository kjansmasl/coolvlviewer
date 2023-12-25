/**
 * @file llfloaterimagepreview.cpp
 * @brief LLFloaterImagePreview class implementation
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

#include "llfloaterimagepreview.h"

#include "llcombobox.h"
#include "llcheckboxctrl.h"
#include "lldir.h"
#include "llimagebmp.h"
#include "llimagejpeg.h"
#include "llimagepng.h"
#include "llimagetga.h"
#include "llinventorytype.h"
#include "llrender.h"
#include "llresizehandle.h"			// For RESIZE_HANDLE_WIDTH
#include "lluictrlfactory.h"

#include "llagent.h"
#include "lldrawable.h"
#include "lldrawpoolavatar.h"
#include "llface.h"
#include "hbfloaterthumbnail.h"
#include "llpipeline.h"
#include "lltoolmgr.h"
#include "llviewercamera.h"
#include "llviewercontrol.h"
#include "llviewerobjectlist.h"
#include "llviewerregion.h"
#include "llviewershadermgr.h"
#include "llviewertexturelist.h"
#include "llviewerwindow.h"
#include "llvoavatar.h"

constexpr S32 PREVIEW_BORDER_WIDTH = 2;
constexpr S32 PREVIEW_RESIZE_HANDLE_SIZE = S32(RESIZE_HANDLE_WIDTH * OO_SQRT2) +
										   PREVIEW_BORDER_WIDTH;
constexpr S32 PREVIEW_HPAD = PREVIEW_RESIZE_HANDLE_SIZE;
constexpr S32 PREF_BUTTON_HEIGHT = 16 + 7 + 16;
constexpr S32 PREVIEW_TEXTURE_HEIGHT = 300;

//-----------------------------------------------------------------------------
// LLFloaterImagePreview class
//-----------------------------------------------------------------------------

LLFloaterImagePreview::LLFloaterImagePreview(const std::string& filename)
:	HBFloaterUploadAsset(filename, LLInventoryType::IT_TEXTURE)
{
	init();
}

LLFloaterImagePreview::LLFloaterImagePreview(const std::string& filename,
											 const LLUUID& thumb_inv_id)
:	HBFloaterUploadAsset(filename, LLInventoryType::IT_NONE), // Not an asset !
	mThumbnailInventoryId(thumb_inv_id)
{
	init();
}

void LLFloaterImagePreview::init()
{
	mAvatarPreview = NULL;
	mSculptedPreview = NULL;
	mImagep = NULL;
	mLastMouseX = mLastMouseY = 0;
	loadImage(mFilenameAndPath);
	LLUICtrlFactory::getInstance()->buildFloater(this,
												 "floater_image_preview.xml");
}

bool LLFloaterImagePreview::postBuild()
{
	if (!HBFloaterUploadAsset::postBuild())
	{
		return false;
	}

	mUploadButton->setLabelArg("[AMOUNT]", llformat("%d", mCost));

	mClothingCombo = getChild<LLComboBox>("clothing_type_combo");
	mClothingCombo->selectFirstItem();
	if (mThumbnailInventoryId.isNull())
	{
		mClothingCombo->setCommitCallback(onPreviewTypeCommit);
		mClothingCombo->setCallbackUserData(this);
	}

	mPreviewRect.set(PREVIEW_HPAD, PREVIEW_TEXTURE_HEIGHT,
					 getRect().getWidth() - PREVIEW_HPAD,
					 PREVIEW_HPAD + PREF_BUTTON_HEIGHT + PREVIEW_HPAD);
	mPreviewImageRect.set(0.f, 1.f, 1.f, 0.f);

	mTempAssetCheck = getChild<LLCheckBoxCtrl>("temp_check");

	bool show_temp_upload = mCost > 0;
	if (show_temp_upload)
	{
		// Temporary texture assets rely on viewer-side baking (it is therefore
		// now only possible to upload them in OpenSim). HB
		LLViewerRegion* regionp = gAgent.getRegion();
		show_temp_upload = regionp && regionp->getCentralBakeVersion() == 0;
	}

	if (mThumbnailInventoryId.notNull())
	{
		mTempAssetCheck->setVisible(false);
		mClothingCombo->setEnabled(false);
		mDescEditor->setText(getString("thumbnail"));
		mDescEditor->setEnabled(false);
		childSetVisible("lossless_check", false);
	}
	else if (mRawImagep.notNull())
	{
		mAvatarPreview = new LLImagePreviewAvatar(256, 256);
		mAvatarPreview->setPreviewTarget(LL_JOINT_KEY_PELVIS,
										 "mUpperBodyMesh0", 2.f, false);

		mSculptedPreview = new LLImagePreviewSculpted(256, 256);
		mSculptedPreview->setPreviewTarget(mRawImagep, 2.f);

		if ((U32)(mRawImagep->getWidth() * mRawImagep->getHeight()) <=
				LL_IMAGE_REZ_LOSSLESS_CUTOFF * LL_IMAGE_REZ_LOSSLESS_CUTOFF)
		{
			childEnable("lossless_check");
		}
	}
	else
	{
		show_temp_upload = false;
		mClothingCombo->setEnabled(false);
		mUploadButton->setEnabled(false);
		childShow("bad_image_text");
	}

	mTempAssetCheck->setVisible(show_temp_upload);

	return true;
}

LLFloaterImagePreview::~LLFloaterImagePreview()
{
	clearAllPreviewTextures();

	mRawImagep = NULL;
	mAvatarPreview = NULL;
	mSculptedPreview = NULL;
	mImagep = NULL;
}

//virtual
void LLFloaterImagePreview::uploadAsset()
{
	if (mThumbnailInventoryId.notNull())
	{
		// Hand over to the thumbnail floater. HB
		HBFloaterThumbnail::uploadThumbnail(mThumbnailInventoryId, mRawImagep);
	}
	else
	{
		mTempAsset = mTempAssetCheck->getVisible() &&
					 mTempAssetCheck->getEnabled() && mTempAssetCheck->get();
		HBFloaterUploadAsset::uploadAsset();
	}
}

//static
void LLFloaterImagePreview::onPreviewTypeCommit(LLUICtrl* ctrl, void* userdata)
{
	LLFloaterImagePreview* fp =(LLFloaterImagePreview*)userdata;

	if (!fp->mAvatarPreview || !fp->mSculptedPreview)
	{
		return;
	}

	S32 which_mode = fp->mClothingCombo->getFirstSelectedIndex();
	switch(which_mode)
	{
	case 0:
		break;
	case 1:
		fp->mAvatarPreview->setPreviewTarget(LL_JOINT_KEY_SKULL, "mHairMesh0",
											 0.4f, false);
		break;
	case 2:
		fp->mAvatarPreview->setPreviewTarget(LL_JOINT_KEY_SKULL, "mHeadMesh0",
											 0.4f, false);
		break;
	case 3:
		fp->mAvatarPreview->setPreviewTarget(LL_JOINT_KEY_CHEST,
											 "mUpperBodyMesh0", 1.f, false);
		break;
	case 4:
		fp->mAvatarPreview->setPreviewTarget(LL_JOINT_KEY_KNEELEFT,
											 "mLowerBodyMesh0", 1.2f, false);
		break;
	case 5:
		fp->mAvatarPreview->setPreviewTarget(LL_JOINT_KEY_SKULL, "mHeadMesh0",
											 0.4f, true);
		break;
	case 6:
		fp->mAvatarPreview->setPreviewTarget(LL_JOINT_KEY_CHEST,
											 "mUpperBodyMesh0", 1.2f, true);
		break;
	case 7:
		fp->mAvatarPreview->setPreviewTarget(LL_JOINT_KEY_KNEELEFT,
											 "mLowerBodyMesh0", 1.2f, true);
		break;
	case 8:
		fp->mAvatarPreview->setPreviewTarget(LL_JOINT_KEY_KNEELEFT,
											 "mSkirtMesh0", 1.3f, false);
		break;
	case 9:
		fp->mSculptedPreview->setPreviewTarget(fp->mRawImagep, 2.f);
		break;
	default:
		break;
	}

	fp->mAvatarPreview->refresh();
	fp->mSculptedPreview->refresh();
}

void LLFloaterImagePreview::clearAllPreviewTextures()
{
	if (mAvatarPreview)
	{
		mAvatarPreview->clearPreviewTexture("mHairMesh0");
		mAvatarPreview->clearPreviewTexture("mUpperBodyMesh0");
		mAvatarPreview->clearPreviewTexture("mLowerBodyMesh0");
		mAvatarPreview->clearPreviewTexture("mHeadMesh0");
		mAvatarPreview->clearPreviewTexture("mUpperBodyMesh0");
		mAvatarPreview->clearPreviewTexture("mLowerBodyMesh0");
		mAvatarPreview->clearPreviewTexture("mSkirtMesh0");
	}
}

void LLFloaterImagePreview::draw()
{
	LLFloater::draw();

	if (mRawImagep.isNull())
	{
		return;
	}

	LLRect r = getRect();
	LLTexUnit* unit0 = gGL.getTexUnit(0);

	S32 selected = mClothingCombo->getFirstSelectedIndex();
	if (selected <= 0)
	{
		gl_rect_2d_checkerboard(mPreviewRect);

		if (mImagep.notNull())
		{
			unit0->bindManual(LLTexUnit::TT_TEXTURE, mImagep->getTexName());
		}
		else
		{
			mImagep = LLViewerTextureManager::getLocalTexture(mRawImagep.get(),
															  false);

			unit0->unbind(mImagep->getTarget());
			unit0->bindManual(LLTexUnit::TT_TEXTURE, mImagep->getTexName());

			unit0->setTextureFilteringOption(LLTexUnit::TFO_BILINEAR);
			unit0->setTextureAddressMode(LLTexUnit::TAM_CLAMP);

			if (mAvatarPreview)
			{
				mAvatarPreview->setTexture(mImagep->getTexName());
				mSculptedPreview->setTexture(mImagep->getTexName());
			}
		}

		gGL.color3f(1.f, 1.f, 1.f);
		gGL.begin(LLRender::TRIANGLES);
		{
			F32 top = mPreviewImageRect.mTop;
			F32 bottom = mPreviewImageRect.mBottom;
			F32 left = mPreviewImageRect.mLeft;
			F32 right = mPreviewImageRect.mRight;
			gGL.texCoord2f(left, top);
			gGL.vertex2i(PREVIEW_HPAD, PREVIEW_TEXTURE_HEIGHT);
			gGL.texCoord2f(left, bottom);
			gGL.vertex2i(PREVIEW_HPAD,
						 PREVIEW_HPAD + PREF_BUTTON_HEIGHT + PREVIEW_HPAD);
			gGL.texCoord2f(right, bottom);
			gGL.vertex2i(r.getWidth() - PREVIEW_HPAD,
						 PREVIEW_HPAD + PREF_BUTTON_HEIGHT + PREVIEW_HPAD);
			gGL.texCoord2f(left, top);
			gGL.vertex2i(PREVIEW_HPAD, PREVIEW_TEXTURE_HEIGHT);
			gGL.texCoord2f(right, bottom);
			gGL.vertex2i(r.getWidth() - PREVIEW_HPAD,
						 PREVIEW_HPAD + PREF_BUTTON_HEIGHT + PREVIEW_HPAD);
			gGL.texCoord2f(right, top);
			gGL.vertex2i(r.getWidth() - PREVIEW_HPAD, PREVIEW_TEXTURE_HEIGHT);
		}
		gGL.end();

		unit0->unbind(LLTexUnit::TT_TEXTURE);
	}
	else if (mAvatarPreview && mSculptedPreview)
	{
		gGL.color3f(1.f, 1.f, 1.f);

		if (selected == 9)
		{
			unit0->bind(mSculptedPreview);
		}
		else
		{
			unit0->bind(mAvatarPreview);
		}

		gGL.begin(LLRender::TRIANGLES);
		{
			S32 right = r.getWidth() - PREVIEW_HPAD;
			gGL.texCoord2f(0.f, 1.f);
			gGL.vertex2i(PREVIEW_HPAD, PREVIEW_TEXTURE_HEIGHT);
			gGL.texCoord2f(0.f, 0.f);
			gGL.vertex2i(PREVIEW_HPAD,
						 PREVIEW_HPAD + PREF_BUTTON_HEIGHT + PREVIEW_HPAD);
			gGL.texCoord2f(1.f, 0.f);
			gGL.vertex2i(right,
						 PREVIEW_HPAD + PREF_BUTTON_HEIGHT + PREVIEW_HPAD);
			gGL.texCoord2f(0.f, 1.f);
			gGL.vertex2i(PREVIEW_HPAD, PREVIEW_TEXTURE_HEIGHT);
			gGL.texCoord2f(1.f, 0.f);
			gGL.vertex2i(right,
						 PREVIEW_HPAD + PREF_BUTTON_HEIGHT + PREVIEW_HPAD);
			gGL.texCoord2f(1.f, 1.f);
			gGL.vertex2i(right, PREVIEW_TEXTURE_HEIGHT);
		}
		gGL.end();

		unit0->unbind(LLTexUnit::TT_TEXTURE);
	}
}

bool LLFloaterImagePreview::loadImage(const std::string& src_filename)
{
	std::string exten = gDirUtilp->getExtension(src_filename);

	U32 codec = IMG_CODEC_INVALID;
	std::string temp_str;
	if (exten == "bmp")
	{
		codec = IMG_CODEC_BMP;
	}
	else if (exten == "tga")
	{
		codec = IMG_CODEC_TGA;
	}
	else if (exten == "jpg" || exten == "jpeg")
	{
		codec = IMG_CODEC_JPEG;
	}
	else if (exten == "png")
	{
		codec = IMG_CODEC_PNG;
	}

	LLPointer<LLImageRaw> imagep = new LLImageRaw;

	switch (codec)
	{
		case IMG_CODEC_BMP:
		{
			LLPointer<LLImageBMP> bmp_imagep = new LLImageBMP;
			if (!bmp_imagep->load(src_filename))
			{
				return false;
			}
			if (!bmp_imagep->decode(imagep))
			{
				return false;
			}
			break;
		}

		case IMG_CODEC_TGA:
		{
			LLPointer<LLImageTGA> tga_imagep = new LLImageTGA;
			if (!tga_imagep->load(src_filename))
			{
				return false;
			}
			if (!tga_imagep->decode(imagep))
			{
				return false;
			}
			if (tga_imagep->getComponents() != 3 &&
				tga_imagep->getComponents() != 4)
			{
				tga_imagep->setLastError("Image files with less than 3 or more than 4 components are not supported.");
				return false;
			}
			break;
		}

		case IMG_CODEC_JPEG:
		{
			LLPointer<LLImageJPEG> jpeg_imagep = new LLImageJPEG;
			if (!jpeg_imagep->load(src_filename))
			{
				return false;
			}
			if (!jpeg_imagep->decode(imagep))
			{
				return false;
			}
			break;
		}

		case IMG_CODEC_PNG:
		{
			LLPointer<LLImagePNG> png_imagep = new LLImagePNG;
			if (!png_imagep->load(src_filename))
			{
				return false;
			}
			if (!png_imagep->decode(imagep))
			{
				return false;
			}
			break;
		}

		default:
			return false;
	}

	imagep->biasedScaleToPowerOfTwo(1024);
	mRawImagep = imagep;

	return true;
}

bool LLFloaterImagePreview::handleMouseDown(S32 x, S32 y, MASK mask)
{
	if (mPreviewRect.pointInRect(x, y))
	{
		bringToFront(x, y);
		gFocusMgr.setMouseCapture(this);
		gViewerWindowp->hideCursor();
		mLastMouseX = x;
		mLastMouseY = y;
		return true;
	}

	return LLFloater::handleMouseDown(x, y, mask);
}

bool LLFloaterImagePreview::handleMouseUp(S32 x, S32 y, MASK mask)
{
	gFocusMgr.setMouseCapture(NULL);
	gViewerWindowp->showCursor();
	return LLFloater::handleMouseUp(x, y, mask);
}

bool LLFloaterImagePreview::handleHover(S32 x, S32 y, MASK mask)
{
	MASK local_mask = mask & ~MASK_ALT;

	if (mAvatarPreview && hasMouseCapture())
	{
		if (local_mask == MASK_PAN)
		{
			// pan here
			if (mClothingCombo->getFirstSelectedIndex() <= 0)
			{
				mPreviewImageRect.translate((F32)(x - mLastMouseX) * -0.005f *
											mPreviewImageRect.getWidth(),
											(F32)(y - mLastMouseY) * -0.005f *
											mPreviewImageRect.getHeight());
			}
			else
			{
				mAvatarPreview->pan((F32)(x - mLastMouseX) * -0.005f,
									(F32)(y - mLastMouseY) * -0.005f);
				mSculptedPreview->pan((F32)(x - mLastMouseX) * -0.005f,
									  (F32)(y - mLastMouseY) * -0.005f);
			}
		}
		else if (local_mask == MASK_ORBIT)
		{
			F32 yaw_radians = (F32)(x - mLastMouseX) * -0.01f;
			F32 pitch_radians = (F32)(y - mLastMouseY) * 0.02f;

			mAvatarPreview->rotate(yaw_radians, pitch_radians);
			mSculptedPreview->rotate(yaw_radians, pitch_radians);
		}
		else
		{
			if (mClothingCombo->getFirstSelectedIndex() <= 0)
			{
				F32 zoom_amt = (F32)(y - mLastMouseY) * -0.002f;
				mPreviewImageRect.stretch(zoom_amt);
			}
			else
			{
				F32 yaw_radians = (F32)(x - mLastMouseX) * -0.01f;
				F32 zoom_amt = (F32)(y - mLastMouseY) * 0.02f;

				mAvatarPreview->rotate(yaw_radians, 0.f);
				mAvatarPreview->zoom(zoom_amt);
				mSculptedPreview->rotate(yaw_radians, 0.f);
				mSculptedPreview->zoom(zoom_amt);
			}
		}

		if (mClothingCombo->getFirstSelectedIndex() <= 0)
		{
			if (mPreviewImageRect.getWidth() > 1.f)
			{
				mPreviewImageRect.stretch((1.f -
										   mPreviewImageRect.getWidth()) *
										  0.5f);
			}
			else if (mPreviewImageRect.getWidth() < 0.1f)
			{
				mPreviewImageRect.stretch((0.1f -
										   mPreviewImageRect.getWidth()) *
										  0.5f);
			}

			if (mPreviewImageRect.getHeight() > 1.f)
			{
				mPreviewImageRect.stretch((1.f -
										   mPreviewImageRect.getHeight()) *
										  0.5f);
			}
			else if (mPreviewImageRect.getHeight() < 0.1f)
			{
				mPreviewImageRect.stretch((0.1f -
										   mPreviewImageRect.getHeight()) *
										  0.5f);
			}

			if (mPreviewImageRect.mLeft < 0.f)
			{
				mPreviewImageRect.translate(-mPreviewImageRect.mLeft, 0.f);
			}
			else if (mPreviewImageRect.mRight > 1.f)
			{
				mPreviewImageRect.translate(1.f - mPreviewImageRect.mRight,
											0.f);
			}

			if (mPreviewImageRect.mBottom < 0.f)
			{
				mPreviewImageRect.translate(0.f, -mPreviewImageRect.mBottom);
			}
			else if (mPreviewImageRect.mTop > 1.f)
			{
				mPreviewImageRect.translate(0.f, 1.f - mPreviewImageRect.mTop);
			}
		}
		else
		{
			mAvatarPreview->refresh();
			mSculptedPreview->refresh();
		}

		LLUI::setCursorPositionLocal(this, mLastMouseX, mLastMouseY);
	}

	if (!mPreviewRect.pointInRect(x, y) || !mAvatarPreview ||
		!mSculptedPreview)
	{
		return LLFloater::handleHover(x, y, mask);
	}
	else if (local_mask == MASK_ORBIT)
	{
		gViewerWindowp->setCursor(UI_CURSOR_TOOLCAMERA);
	}
	else if (local_mask == MASK_PAN)
	{
		gViewerWindowp->setCursor(UI_CURSOR_TOOLPAN);
	}
	else
	{
		gViewerWindowp->setCursor(UI_CURSOR_TOOLZOOMIN);
	}

	return true;
}

bool LLFloaterImagePreview::handleScrollWheel(S32 x, S32 y, S32 clicks)
{
	if (mPreviewRect.pointInRect(x, y) && mAvatarPreview)
	{
		mAvatarPreview->zoom((F32)clicks * -0.2f);
		mAvatarPreview->refresh();

		mSculptedPreview->zoom((F32)clicks * -0.2f);
		mSculptedPreview->refresh();
	}

	return true;
}

//-----------------------------------------------------------------------------
// LLImagePreviewAvatar class
//-----------------------------------------------------------------------------

LLImagePreviewAvatar::LLImagePreviewAvatar(S32 width, S32 height)
:	LLViewerDynamicTexture(width, height, 3, ORDER_MIDDLE, false)
{
	mNeedsUpdate = true;
	mTargetJoint = NULL;
	mTargetMesh = NULL;
	mCameraDistance = 0.f;
	mCameraYaw = 0.f;
	mCameraPitch = 0.f;
	mCameraZoom = 1.f;
	mTextureName = 0;

	mDummyAvatar =
		(LLVOAvatar*)gObjectList.createObjectViewer(LL_PCODE_LEGACY_AVATAR,
													gAgent.getRegion(),
													LLViewerObject::CO_FLAG_UI_AVATAR);
	if (!mDummyAvatar)
	{
		llwarns << "Cannot create a dummy avatar !" << llendl;
		return;
	}
	mDummyAvatar->mSpecialRenderMode = 2;
}

LLImagePreviewAvatar::~LLImagePreviewAvatar()
{
	if (mDummyAvatar)
	{
		mDummyAvatar->markDead();
	}
}

//virtual
S8 LLImagePreviewAvatar::getType() const
{
	return LLViewerDynamicTexture::LL_IMAGE_PREVIEW_AVATAR;
}

void LLImagePreviewAvatar::setPreviewTarget(U32 joint_key,
											const std::string& mesh,
											F32 distance, bool male)
{
	if (!mDummyAvatar) return;

	mTargetJoint = mDummyAvatar->mRoot->findJoint(joint_key);
	if (mTargetMesh)
	{
		// Clear out existing test texture on current mesh
		mTargetMesh->setTestTexture(0);
	}

	mDummyAvatar->setVisualParamWeight("male", male ? 1.f : 0.f);
	mDummyAvatar->updateVisualParams();
#if 0	// This is a NOP !  HB
	mDummyAvatar->updateGeometry(mDummyAvatar->mDrawable);
#endif

	LLAvatarJoint* rootp = mDummyAvatar->mRoot;
	// Hide all body parts
	rootp->setVisible(false, true);
	mTargetMesh = dynamic_cast<LLViewerJointMesh*>(rootp->findJoint(mesh));
	if (mTargetMesh)
	{
		// Show target only, with our texture
		mTargetMesh->setTestTexture(mTextureName);
		mTargetMesh->setVisible(true, false);
	}

	mCameraDistance = distance;
	mCameraZoom = 1.f;
	mCameraPitch = 0.f;
	mCameraYaw = 0.f;
	mCameraOffset.clear();
}

void LLImagePreviewAvatar::clearPreviewTexture(const std::string& mesh_name)
{
	if (!mDummyAvatar)
	{
		return;
	}
	LLAvatarJoint* rootp = mDummyAvatar->mRoot;
	LLViewerJointMesh* meshp =
		dynamic_cast<LLViewerJointMesh*>(rootp->findJoint(mesh_name));
	if (meshp)
	{
		// Clear out existing test mesh
		meshp->setTestTexture(0);
	}
}

bool LLImagePreviewAvatar::render()
{
	LLVOAvatar* avatarp = mDummyAvatar.get();
	if (!avatarp || avatarp->mDrawable.isNull())
	{
		return true;
	}
	mNeedsUpdate = false;

	gGL.pushUIMatrix();
	gGL.loadUIIdentity();

	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.pushMatrix();
	gGL.loadIdentity();
	gGL.ortho(0.f, mFullWidth, 0.f, mFullHeight, -1.f, 1.f);

	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.pushMatrix();
	gGL.loadIdentity();

	LLGLSUIDefault def;

	gUIProgram.bind();

	gGL.color4f(0.15f, 0.2f, 0.3f, 1.f);
	gl_rect_2d_simple(mFullWidth, mFullHeight);

	gGL.color4f(1.f, 1.f, 1.f, 1.f);

	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.popMatrix();

	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.popMatrix();

	gGL.flush();

	LLVector3 target_pos = mTargetJoint->getWorldPosition();

	LLQuaternion camera_rot = LLQuaternion(mCameraPitch, LLVector3::y_axis) *
							  LLQuaternion(mCameraYaw, LLVector3::z_axis);

	LLQuaternion av_rot = avatarp->mPelvisp->getWorldRotation() * camera_rot;
	gViewerCamera.setOriginAndLookAt(target_pos +
									 ((LLVector3(mCameraDistance, 0.f, 0.f) +
									   mCameraOffset) * av_rot), // Camera
									 LLVector3::z_axis,			 // Up
									 // point of interest
									 target_pos + mCameraOffset * av_rot);

	gViewerCamera.setAspect((F32)mFullWidth / (F32)mFullHeight);
	gViewerCamera.setViewNoBroadcast(gViewerCamera.getDefaultFOV() /
									 mCameraZoom);
	gViewerCamera.setPerspective(false, mOrigin.mX, mOrigin.mY, mFullWidth,
								 mFullHeight, false);

	avatarp->updateLOD();

	LLVertexBuffer::unbind();

	// Do not let environment settings influence our scene lighting.
	LLPreviewLighting preview_light;

	// *FIXME: find out why only previewAvatar() seems to work in PBR mode,
	// while LL's PBR viewer can do renderAvatars() here. HB
	if (gUsePBRShaders)
	{
		gPipeline.previewAvatar(avatarp);
		gGL.popUIMatrix();
		return true;
	}

	LLGLDepthTest gls_depth(GL_TRUE);
	// Make sure alpha=0 shows avatar material color
	LLGLDisable no_blend(GL_BLEND);

	LLFace* facep = avatarp->mDrawable->getFace(0);
	if (facep)	// Paranoia
	{
		LLDrawPoolAvatar* poolp = (LLDrawPoolAvatar*)facep->getPool();
		if (poolp)	// More paranoia !
		{
			// Render only our dummy avatar
			poolp->renderAvatars(avatarp);
		}
	}

	gGL.popUIMatrix();

	return true;
}

void LLImagePreviewAvatar::refresh()
{
	mNeedsUpdate = true;
}

void LLImagePreviewAvatar::rotate(F32 yaw_radians, F32 pitch_radians)
{
	mCameraYaw = mCameraYaw + yaw_radians;

	mCameraPitch = llclamp(mCameraPitch + pitch_radians, F_PI_BY_TWO * -0.8f,
						   F_PI_BY_TWO * 0.8f);
}

void LLImagePreviewAvatar::zoom(F32 zoom_amt)
{
	mCameraZoom	= llclamp(mCameraZoom + zoom_amt, 1.f, 10.f);
}

void LLImagePreviewAvatar::pan(F32 right, F32 up)
{
	mCameraOffset.mV[VY] = llclamp(mCameraOffset.mV[VY] +
								   right * mCameraDistance / mCameraZoom,
								   -1.f, 1.f);
	mCameraOffset.mV[VZ] = llclamp(mCameraOffset.mV[VZ] +
								   up * mCameraDistance / mCameraZoom,
								   -1.f, 1.f);
}

//-----------------------------------------------------------------------------
// LLImagePreviewSculpted class
//-----------------------------------------------------------------------------

LLImagePreviewSculpted::LLImagePreviewSculpted(S32 width, S32 height)
:	LLViewerDynamicTexture(width, height, 3, ORDER_MIDDLE, false)
{
	mNeedsUpdate = true;
	mCameraDistance = 0.f;
	mCameraYaw = 0.f;
	mCameraPitch = 0.f;
	mCameraZoom = 1.f;
	mTextureName = 0;

	LLVolumeParams volume_params;
	volume_params.setType(LL_PCODE_PROFILE_CIRCLE, LL_PCODE_PATH_CIRCLE);
	volume_params.setSculptID(LLUUID::null, LL_SCULPT_TYPE_SPHERE);

	constexpr F32 HIGHEST_LOD = 4.f;
	mVolume = new LLVolume(volume_params,  HIGHEST_LOD);
}

//virtual
S8 LLImagePreviewSculpted::getType() const
{
	return LLViewerDynamicTexture::LL_IMAGE_PREVIEW_SCULPTED;
}

void LLImagePreviewSculpted::setPreviewTarget(LLImageRaw* imagep, F32 distance)
{
	mCameraDistance = distance;
	mCameraZoom = 1.f;
	mCameraPitch = 0.f;
	mCameraYaw = 0.f;
	mCameraOffset.clear();

	if (imagep)
	{
		mVolume->sculpt(imagep->getWidth(), imagep->getHeight(),
						imagep->getComponents(), imagep->getData(), 0);
	}

	const LLVolumeFace& vf = mVolume->getVolumeFace(0);
	U32 num_indices = vf.mNumIndices;
	U32 num_vertices = vf.mNumVertices;

	mVertexBuffer = new LLVertexBuffer(LLVertexBuffer::MAP_VERTEX |
									   LLVertexBuffer::MAP_NORMAL |
									   LLVertexBuffer::MAP_TEXCOORD0);
	if (!mVertexBuffer->allocateBuffer(num_vertices, num_indices))
	{
		llwarns << "Failed to allocate vertex buffer for image preview with "
				<< num_vertices << " vertices and " << num_indices
				<< " indices. Aborting." << llendl;
		return;
	}

	LLStrider<LLVector3> vertex_strider;
	LLStrider<LLVector3> normal_strider;
	LLStrider<LLVector2> tc_strider;
	LLStrider<U16> index_strider;

	if (!mVertexBuffer->getVertexStrider(vertex_strider) ||
		!mVertexBuffer->getNormalStrider(normal_strider) ||
		!mVertexBuffer->getTexCoord0Strider(tc_strider) ||
		!mVertexBuffer->getIndexStrider(index_strider))
	{
		return;
	}

	// Build vertices and normals
	LLStrider<LLVector3> pos;
	pos = (LLVector3*)vf.mPositions;
	pos.setStride(16);
	LLStrider<LLVector3> norm;
	norm = (LLVector3*)vf.mNormals;
	norm.setStride(16);
	LLStrider<LLVector2> tc;
	tc = (LLVector2*)vf.mTexCoords;
	tc.setStride(8);

	for (U32 i = 0; i < num_vertices; i++)
	{
		*(vertex_strider++) = *pos++;
		LLVector3 normal = *norm++;
		normal.normalize();
		*(normal_strider++) = normal;
		*(tc_strider++) = *tc++;
	}

	// Build indices
	for (U16 i = 0; i < num_indices; i++)
	{
		*(index_strider++) = vf.mIndices[i];
	}

	mVertexBuffer->unmapBuffer();
}

bool LLImagePreviewSculpted::render()
{
	mNeedsUpdate = false;

	LLGLSUIDefault def;
	LLGLDisable no_blend(GL_BLEND);
	LLGLEnable cull(GL_CULL_FACE);
	LLGLDepthTest depth(GL_TRUE);

	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.pushMatrix();
	gGL.loadIdentity();
	gGL.ortho(0.f, mFullWidth, 0.f, mFullHeight, -1.f, 1.f);

	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.pushMatrix();
	gGL.loadIdentity();

	gGL.color4f(0.15f, 0.2f, 0.3f, 1.f);

	gUIProgram.bind();

	gl_rect_2d_simple(mFullWidth, mFullHeight);

	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.popMatrix();

	gGL.matrixMode(LLRender::MM_MODELVIEW);
	gGL.popMatrix();

	glClear(GL_DEPTH_BUFFER_BIT);

	LLVector3 target_pos(0, 0, 0);

	LLQuaternion camera_rot = LLQuaternion(mCameraPitch, LLVector3::y_axis) *
							  LLQuaternion(mCameraYaw, LLVector3::z_axis);

	LLQuaternion av_rot = camera_rot;
	gViewerCamera.setOriginAndLookAt(target_pos +
									 ((LLVector3(mCameraDistance, 0.f, 0.f) +
									   mCameraOffset) * av_rot), // camera
									 LLVector3::z_axis,			 // up
									 // point of interest
									 target_pos + mCameraOffset * av_rot);

	gViewerCamera.setAspect((F32)mFullWidth / (F32)mFullHeight);
	gViewerCamera.setViewNoBroadcast(gViewerCamera.getDefaultFOV() /
									 mCameraZoom);
	gViewerCamera.setPerspective(false, mOrigin.mX, mOrigin.mY, mFullWidth,
								 mFullHeight, false);

	const LLVolumeFace& vf = mVolume->getVolumeFace(0);
	U32 num_indices = vf.mNumIndices;

	LLVertexBuffer::unbind();

	gObjectPreviewProgram.bind();

	gPipeline.enableLightsPreview();

	gGL.pushMatrix();
	constexpr F32 SCALE = 1.25f;
	gGL.scalef(SCALE, SCALE, SCALE);
	constexpr F32 BRIGHTNESS = 0.9f;
	gGL.diffuseColor3f(BRIGHTNESS, BRIGHTNESS, BRIGHTNESS);

	// Note: mask ignored in PBR rendering mode
	mVertexBuffer->setBuffer(LLVertexBuffer::MAP_VERTEX |
							 LLVertexBuffer::MAP_NORMAL |
							 LLVertexBuffer::MAP_TEXCOORD0);
	mVertexBuffer->draw(LLRender::TRIANGLES, num_indices, 0);

	gGL.popMatrix();

	gObjectPreviewProgram.unbind();

	stop_glerror();

	return true;
}

void LLImagePreviewSculpted::refresh()
{
	mNeedsUpdate = true;
}

void LLImagePreviewSculpted::rotate(F32 yaw_radians, F32 pitch_radians)
{
	mCameraYaw = mCameraYaw + yaw_radians;

	mCameraPitch = llclamp(mCameraPitch + pitch_radians,
						   F_PI_BY_TWO * -0.8f, F_PI_BY_TWO * 0.8f);
}

void LLImagePreviewSculpted::zoom(F32 zoom_amt)
{
	mCameraZoom	= llclamp(mCameraZoom + zoom_amt, 1.f, 10.f);
}

void LLImagePreviewSculpted::pan(F32 right, F32 up)
{
	mCameraOffset.mV[VY] = llclamp(mCameraOffset.mV[VY] +
								   right * mCameraDistance / mCameraZoom,
								   -1.f, 1.f);
	mCameraOffset.mV[VZ] = llclamp(mCameraOffset.mV[VZ] +
								   up * mCameraDistance / mCameraZoom,
								   -1.f, 1.f);
}
