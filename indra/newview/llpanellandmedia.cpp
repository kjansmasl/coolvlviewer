/**
 * @file llpanellandmedia.cpp
 * @brief Allows configuration of "media" for a land parcel,
 *   for example movies, web pages, and audio.
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

#include "llpanellandmedia.h"

#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "lllineeditor.h"
#include "llmimetypes.h"
#include "llparcel.h"
#include "llradiogroup.h"
#include "llsdutil.h"
#include "llspinctrl.h"
#include "llscrolllistctrl.h"
#include "lluictrlfactory.h"
#include "roles_constants.h"

#include "llfloaterurlentry.h"
#include "lltexturectrl.h"
#include "llviewermedia.h"
#include "llviewerparcelmedia.h"
#include "llviewerparcelmgr.h"
#include "llviewerregion.h"

LLPanelLandMedia::LLPanelLandMedia(LLParcelSelectionHandle& parcel)
:	mParcel(parcel)
{
}

//virtual
bool LLPanelLandMedia::postBuild()
{
	mMediaTextureCtrl = getChild<LLTextureCtrl>("media_texture_ctrl");
	mMediaTextureCtrl->setCommitCallback(onCommitAny);
	mMediaTextureCtrl->setCallbackUserData(this);
	mMediaTextureCtrl->setAllowNoTexture (true);
	mMediaTextureCtrl->setImmediateFilterPermMask(PERM_COPY | PERM_TRANSFER);
	mMediaTextureCtrl->setNonImmediateFilterPermMask(PERM_COPY | PERM_TRANSFER);

	mMediaAutoScaleCheck = getChild<LLCheckBoxCtrl>("media_auto_scale");
	mMediaAutoScaleCheck->setCommitCallback(onCommitAny);
	mMediaAutoScaleCheck->setCallbackUserData(this);

	mMediaLoopCheck = getChild<LLCheckBoxCtrl>("media_loop");
	mMediaLoopCheck->setCommitCallback(onCommitAny);
	mMediaLoopCheck->setCallbackUserData(this);

	mMediaURLEdit = getChild<LLLineEditor>("media_url");
	mMediaURLEdit->setCommitCallback(onCommitAny);
	mMediaURLEdit->setCallbackUserData(this);

	mMediaDescEdit = getChild<LLLineEditor>("url_description");
	mMediaDescEdit->setCommitCallback(onCommitAny);
	mMediaDescEdit->setCallbackUserData(this);

	mMediaTypeCombo = getChild<LLComboBox>("media_type_combo");
	childSetCommitCallback("media_type_combo", onCommitType, this);
	populateMIMECombo();

	mMediaResetCtrl = getChild<LLSpinCtrl>("media_reset_time");
	mMediaResetCtrl->setCommitCallback(onCommitAny);
	mMediaResetCtrl->setCallbackUserData(this);

	mMediaWidthCtrl = getChild<LLSpinCtrl>("media_size_width");
	mMediaWidthCtrl->setCommitCallback(onCommitAny);
	mMediaWidthCtrl->setCallbackUserData(this);

	mMediaHeightCtrl = getChild<LLSpinCtrl>("media_size_height");
	mMediaHeightCtrl->setCommitCallback(onCommitAny);
	mMediaHeightCtrl->setCallbackUserData(this);

	mSetURLButton = getChild<LLButton>("set_media_url");
	mSetURLButton->setClickedCallback(onSetBtn, this);

	mResetURLButton = getChild<LLButton>("reset_media_url");
	mResetURLButton->setClickedCallback(onResetBtn, this);

	mRadioNavigateControl = getChild<LLRadioGroup>("radio_navigate_allow");
	mRadioNavigateControl->setCommitCallback(onCommitAny);
	mRadioNavigateControl->setCallbackUserData(this);

	mCheckObscureMOAP = getChild<LLCheckBoxCtrl>("obscure moap check");
	mCheckObscureMOAP->setCommitCallback(onCommitAny);
	mCheckObscureMOAP->setCallbackUserData(this);

	return true;
}

//virtual
void LLPanelLandMedia::refresh()
{
	LLParcel* parcel = mParcel->getParcel();

	if (!parcel)
	{
		clearCtrls();
	}
	else	// Something selected, hooray !
	{
		// Display options
		bool can_change_media =
			LLViewerParcelMgr::isParcelModifiableByAgent(parcel,
														 GP_LAND_CHANGE_MEDIA);

		mMediaURLEdit->setText(parcel->getMediaURL());
		mMediaURLEdit->setEnabled(false);

		childSetText("current_url", parcel->getMediaCurrentURL());

		mMediaDescEdit->setText(parcel->getMediaDesc());
		mMediaDescEdit->setEnabled(can_change_media);

		std::string mime_type = parcel->getMediaType();
		if (mime_type.empty())
		{
			mime_type = LLMIMETypes::getDefaultMimeType();
		}
		setMediaType(mime_type);
		mMediaTypeCombo->setEnabled(can_change_media);
		childSetText("mime_type", mime_type);

		mMediaAutoScaleCheck->set(parcel->getMediaAutoScale());
		mMediaAutoScaleCheck->setEnabled(can_change_media);

		// Special code to disable looping checkbox for HTML MIME type
		// (DEV-10042 -- Parcel Media: "Loop Media" should be disabled for
		// static media types)
		bool allow_looping = LLMIMETypes::findAllowLooping(mime_type);
		if (allow_looping)
		{
			mMediaLoopCheck->set(parcel->getMediaLoop());
		}
		else
		{
			mMediaLoopCheck->set(false);
		}
		mMediaLoopCheck->setEnabled (can_change_media && allow_looping);

		mMediaResetCtrl->set(parcel->getMediaURLTimeout());
		mMediaResetCtrl->setEnabled(can_change_media);

		// disallow media size change for mime types that don't allow it
		bool allow_resize = LLMIMETypes::findAllowResize(mime_type);
		if (allow_resize)
		{
			mMediaWidthCtrl->setValue(parcel->getMediaWidth());
		}
		else
		{
			mMediaWidthCtrl->setValue(0);
		}
		mMediaWidthCtrl->setEnabled(can_change_media && allow_resize);

		if (allow_resize)
		{
			mMediaHeightCtrl->setValue(parcel->getMediaHeight());
		}
		else
		{
			mMediaHeightCtrl->setValue(0);
		}
		mMediaHeightCtrl->setEnabled(can_change_media && allow_resize);

		mMediaTextureCtrl->setImageAssetID (parcel->getMediaID());
		mMediaTextureCtrl->setEnabled(can_change_media);

		mSetURLButton->setEnabled(can_change_media);
		mResetURLButton->setEnabled(can_change_media);

		LLFloaterURLEntry* floaterp =
			(LLFloaterURLEntry*)mURLEntryFloater.get();
		if (floaterp)
		{
			floaterp->addURLToCombobox(getMediaURL());
		}

		// This radial control is really just an inverse mapping to the boolean
		// allow_navigate value. It is set as a radial merely for user
		// readability.
		mRadioNavigateControl->setSelectedIndex(!parcel->getMediaAllowNavigate());
		mRadioNavigateControl->setEnabled(can_change_media);

		mCheckObscureMOAP->set(parcel->getObscureMOAP());
		mCheckObscureMOAP->setEnabled(can_change_media);
	}
}

void LLPanelLandMedia::populateMIMECombo()
{
	std::string default_mime_type = LLMIMETypes::getDefaultMimeType();
	std::string default_label;
	LLMIMETypes::mime_widget_set_map_t::const_iterator it;
	for (it = LLMIMETypes::sWidgetMap.begin();
		 it != LLMIMETypes::sWidgetMap.end(); ++it)
	{
		const std::string& mime_type = it->first;
		const LLMIMETypes::LLMIMEWidgetSet& info = it->second;
		if (info.mDefaultMimeType == default_mime_type)
		{
			// Add this label at the end to make UI look cleaner
			default_label = info.mLabel;
		}
		else
		{
			mMediaTypeCombo->add(info.mLabel, mime_type);
		}
	}
	// *TODO: The sort order is based on std::map key, which is
	// ASCII-sorted and is wrong in other languages.  TRANSLATE
	mMediaTypeCombo->add(default_label, default_mime_type, ADD_BOTTOM);
}

void LLPanelLandMedia::setMediaType(const std::string& mime_type)
{
	LLParcel *parcel = mParcel->getParcel();
	if (parcel)
	{
		parcel->setMediaType(mime_type);
	}

	std::string media_key = LLMIMETypes::widgetType(mime_type);
	mMediaTypeCombo->setValue(media_key);
	childSetText("mime_type", mime_type);
}

void LLPanelLandMedia::setMediaURL(const std::string& media_url)
{
	mMediaURLEdit->setText(media_url);
	LLParcel* parcel = mParcel->getParcel();
	if (parcel)
	{
		parcel->setMediaCurrentURL(media_url);
	}
	//LLViewerMedia::navigateHome();

	mMediaURLEdit->onCommit();
	//LLViewerParcelMedia::sendMediaNavigateMessage(media_url);
	childSetText("current_url", media_url);
}

std::string LLPanelLandMedia::getMediaURL()
{
	return mMediaURLEdit->getText();
}

//static
void LLPanelLandMedia::onCommitType(LLUICtrl* ctrl, void* userdata)
{
	LLPanelLandMedia* self = (LLPanelLandMedia*)userdata;
	if (!self || !ctrl) return;

	std::string current_type =
		LLMIMETypes::widgetType(self->childGetText("mime_type"));
	std::string new_type = self->mMediaTypeCombo->getValue();
	if (current_type != new_type)
	{
		self->childSetText("mime_type",
						   LLMIMETypes::findDefaultMimeType(new_type));
	}
	onCommitAny(ctrl, userdata);
}

//static
void LLPanelLandMedia::onCommitAny(LLUICtrl*, void* userdata)
{
	LLPanelLandMedia* self = (LLPanelLandMedia*)userdata;
	if (!self) return;

	LLParcel* parcel = self->mParcel->getParcel();
	if (!parcel) return;

	// Extract data from UI
	std::string media_url	= self->mMediaURLEdit->getText();
	std::string media_desc	= self->mMediaDescEdit->getText();
	std::string mime_type	= self->childGetText("mime_type");
	bool media_auto_scale	= self->mMediaAutoScaleCheck->get();
	bool media_loop			= self->mMediaLoopCheck->get();
	F32 media_reset_time	= (F32)self->mMediaResetCtrl->get();
	S32 media_width			= (S32)self->mMediaWidthCtrl->get();
	S32 media_height		= (S32)self->mMediaHeightCtrl->get();
	LLUUID media_id			= self->mMediaTextureCtrl->getImageAssetID();
	U8 navigate_allow       = !self->mRadioNavigateControl->getSelectedIndex();
	bool obscure_moap 		= self->mCheckObscureMOAP->get();

	self->childSetText("mime_type", mime_type);

	// Remove leading/trailing whitespace (common when copying/pasting)
	LLStringUtil::trim(media_url);

	// Push data into current parcel
	parcel->setMediaURL(media_url);
	parcel->setMediaType(mime_type);
	parcel->setMediaDesc(media_desc);
	parcel->setMediaWidth(media_width);
	parcel->setMediaHeight(media_height);
	parcel->setMediaID(media_id);
	parcel->setMediaAutoScale(media_auto_scale);
	parcel->setMediaLoop(media_loop);
	parcel->setMediaAllowNavigate(navigate_allow);
	parcel->setMediaURLTimeout(media_reset_time);
	parcel->setObscureMOAP(obscure_moap);

	// Send current parcel data upstream to server
	gViewerParcelMgr.sendParcelPropertiesUpdate(parcel);

	// Might have changed properties, so let's redraw!
	self->refresh();
}

//static
void LLPanelLandMedia::onSetBtn(void* userdata)
{
	LLPanelLandMedia* self = (LLPanelLandMedia*)userdata;
	if (!self) return;

	self->mURLEntryFloater = LLFloaterURLEntry::show(self->getHandle(),
													 self->getMediaURL());
	if (gFloaterViewp)
	{
		LLFloater* parentp = gFloaterViewp->getParentFloater(self);
		if (parentp)
		{
			parentp->addDependentFloater(self->mURLEntryFloater.get());
		}
	}
}

//static
void LLPanelLandMedia::onResetBtn(void* userdata)
{
	LLPanelLandMedia* self = (LLPanelLandMedia*)userdata;
	if (self)
	{
		LLParcel* parcel = self->mParcel->getParcel();
		//LLViewerMedia::navigateHome();
		self->refresh();
		self->childSetText("current_url", parcel->getMediaURL());
		//LLViewerParcelMedia::sendMediaNavigateMessage(parcel->getMediaURL());
	}
}
