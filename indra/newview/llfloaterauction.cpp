/**
 * @file llfloaterauction.cpp
 * @author James Cook, Ian Wilkes
 * @brief Implementation of the auction floater.
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
#include "llfloaterauction.h"

#include "llfilesystem.h"
#include "llgl.h"
#include "llimagej2c.h"
#include "llimagetga.h"
#include "llparcel.h"
#include "llrender.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "llviewercontrol.h"
#include "llviewerdisplay.h"
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"
#include "llviewertexturelist.h"
#include "llviewerwindow.h"

//-----------------------------------------------------------------------------
// Local function definitions
//-----------------------------------------------------------------------------

// StoreAssetData callback (fixed)
void auction_tga_upload_done(const LLUUID& asset_id, void* user_data,
							 S32 status, LLExtStat ext_status)
{
	std::string* name = (std::string*)(user_data);
	llinfos << "Upload of asset '" << *name << "' " << asset_id << " returned "
			<< status << llendl;
	delete name;

	gWindowp->decBusyCount();

	if (0 == status)
	{
		gNotifications.add("UploadWebSnapshotDone");
	}
	else
	{
		LLSD args;
		args["REASON"] = std::string(LLAssetStorage::getErrorString(status));
		gNotifications.add("UploadAuctionSnapshotFail", args);
	}
}

// StoreAssetData callback (fixed)
void auction_j2c_upload_done(const LLUUID& asset_id, void* user_data,
							 S32 status, LLExtStat ext_status)
{
	std::string* name = (std::string*)(user_data);
	llinfos << "Upload of asset '" << *name << "' " << asset_id << " returned "
			<< status << llendl;
	delete name;

	gWindowp->decBusyCount();

	if (status == 0)
	{
		gNotifications.add("UploadSnapshotDone");
	}
	else
	{
		LLSD args;
		args["REASON"] = std::string(LLAssetStorage::getErrorString(status));
		gNotifications.add("UploadAuctionSnapshotFail", args);
	}
}

//-----------------------------------------------------------------------------
// LLFloaterAuction class proper
//-----------------------------------------------------------------------------

LLFloaterAuction::LLFloaterAuction(const LLSD&)
:	mParcelID(-1)
{
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_auction.xml");

	childSetAction("snapshot_btn", onClickSnapshot, this);
	childSetAction("ok_btn", onClickOK, this);
}

//virtual
void LLFloaterAuction::onOpen()
{
	mParcelp = gViewerParcelMgr.getParcelSelection();
	LLViewerRegion* regionp = gViewerParcelMgr.getSelectionRegion();
	LLParcel* parcelp = mParcelp->getParcel();
	if (parcelp && regionp && !parcelp->getForSale())
	{
		mParcelHost = regionp->getHost();
		mParcelID = parcelp->getLocalID();

		childSetText("parcel_text", parcelp->getName());
		childEnable("snapshot_btn");
		childEnable("ok_btn");
	}
	else
	{
		mParcelHost.invalidate();
		if (parcelp && parcelp->getForSale())
		{
			childSetText("parcel_text", getString("already for sale"));
		}
		else
		{
			childSetText("parcel_text", LLStringUtil::null);
		}
		mParcelID = -1;
		childSetEnabled("snapshot_btn", false);
		childSetEnabled("ok_btn", false);
	}
	mImageID.setNull();
	mImage = NULL;
}

void LLFloaterAuction::draw()
{
	LLFloater::draw();

	if (!isMinimized() && mImage.notNull())
	{
		LLRect rect;
		if (childGetRect("snapshot_icon", rect))
		{
			{
				gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
				gl_rect_2d(rect, LLColor4(0.f, 0.f, 0.f, 1.f));
				rect.stretch(-1);
			}
			{
				LLGLSUIDefault gls_ui;
				gGL.color3f(1.f, 1.f, 1.f);
				gl_draw_scaled_image(rect.mLeft, rect.mBottom,
									 rect.getWidth(), rect.getHeight(),
									 mImage);
			}
		}
	}
}

// static
void LLFloaterAuction::onClickSnapshot(void* data)
{
	LLFloaterAuction* self = (LLFloaterAuction*)(data);
	if (!self) return;

	LLPointer<LLImageRaw> raw = new LLImageRaw;

	gForceRenderLandFence = self->childGetValue("fence_check").asBoolean();
	bool success = gViewerWindowp->rawSnapshot(raw,
											  gViewerWindowp->getWindowWidth(),
											  gViewerWindowp->getWindowHeight(),
											  true, false, false, false);
	gForceRenderLandFence = false;

	if (success)
	{
		self->mTransactionID.generate();
		self->mImageID = self->mTransactionID.makeAssetID(gAgent.getSecureSessionID());

		if (!gSavedSettings.getBool("QuietSnapshotsToDisk"))
		{
			gViewerWindowp->playSnapshotAnimAndSound();
		}
		llinfos << "Writing TGA..." << llendl;

		LLPointer<LLImageTGA> tga = new LLImageTGA;
		tga->encode(raw);

		LLFileSystem tga_file(self->mImageID, LLFileSystem::OVERWRITE);
		tga_file.write(tga->getData(), tga->getDataSize());

		raw->biasedScaleToPowerOfTwo(LLViewerTexture::MAX_IMAGE_SIZE_DEFAULT);

		llinfos << "Writing J2C..." << llendl;

		LLPointer<LLImageJ2C> j2c = new LLImageJ2C;
		j2c->encode(raw);

		LLFileSystem j2c_file(self->mImageID, LLFileSystem::OVERWRITE);
		j2c_file.write(j2c->getData(), j2c->getDataSize());

		self->mImage = LLViewerTextureManager::getLocalTexture((LLImageRaw*)raw,
															   false);
		gGL.getTexUnit(0)->bind(self->mImage);
		self->mImage->setAddressMode(LLTexUnit::TAM_CLAMP);
	}
	else
	{
		llwarns << "Unable to take snapshot" << llendl;
	}
}

// static
void LLFloaterAuction::onClickOK(void* data)
{
	LLFloaterAuction* self = (LLFloaterAuction*)(data);
	if (!self) return;

	if (self->mImageID.notNull())
	{
		if (!gAssetStoragep)
		{
			llwarns << "No valid asset storage. Aborted." << llendl;
			return;
		}
		LLSD parcel_name = self->childGetValue("parcel_text");

		// create the asset
		std::string* name = new std::string(parcel_name.asString());
		gAssetStoragep->storeAssetData(self->mTransactionID,
									   LLAssetType::AT_IMAGE_TGA,
									   auction_tga_upload_done, (void*)name,
									   false);
		gWindowp->incBusyCount();

		std::string* j2c_name = new std::string(parcel_name.asString());
		gAssetStoragep->storeAssetData(self->mTransactionID,
									   LLAssetType::AT_TEXTURE,
									   auction_j2c_upload_done,
									   (void*)j2c_name, false);
		gWindowp->incBusyCount();

		gNotifications.add("UploadingAuctionSnapshot");
	}

	LLMessageSystem* msg = gMessageSystemp;
	msg->newMessage("ViewerStartAuction");

	msg->nextBlock("AgentData");
	msg->addUUID("AgentID", gAgentID);
	msg->addUUID("SessionID", gAgentSessionID);
	msg->nextBlock("ParcelData");
	msg->addS32("LocalID", self->mParcelID);
	msg->addUUID("SnapshotID", self->mImageID);
	msg->sendReliable(self->mParcelHost);

	// Clean up floater, and get out
	self->mImageID.setNull();
	self->mImage = NULL;
	self->mParcelID = -1;
	self->mParcelHost.invalidate();
	self->close();
}
