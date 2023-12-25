/**
 * @file llfloaterpostcard.cpp
 * @brief Postcard send floater, allows setting name, e-mail address, etc.
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

#include <regex>

#include "llfloaterpostcard.h"

#include "llgl.h"
#include "llimagejpeg.h"
#include "lllineeditor.h"
#include "lltexteditor.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "llgridmanager.h"			// For gIsInSecondLife
#include "llviewerassetupload.h"
#include "llviewertexture.h"
#include "llviewerwindow.h"

// static variables
LLFloaterPostcard::instance_list_t LLFloaterPostcard::sInstances;
std::string LLFloaterPostcard::sUserEmail;

class LLPostcardUploadInfo : public LLBufferedAssetUploadInfo
{
public:
	LLPostcardUploadInfo(const std::string& email_from,
						 const std::string& name_from,
						 const std::string& email_to,
						 const std::string& subject,
						 const std::string& message,
						 const LLVector3d& position,
						 LLPointer<LLImageFormatted> image,
						 inv_uploaded_cb_t finish)
	:	LLBufferedAssetUploadInfo(LLUUID::null, image, finish),
		mEmailFrom(email_from),
		mNameFrom(name_from),
		mEmailTo(email_to),
		mSubject(subject),
		mMessage(message),
		mGlobalPosition(position)
	{
	}

    LLSD generatePostBody() override
	{
		LLSD postcard = LLSD::emptyMap();
		postcard["to"] = mEmailTo;
		if (!gIsInSecondLife)
		{
			postcard["from"] = mEmailFrom;
		}
		postcard["name"] = mNameFrom;
		postcard["subject"] = mSubject;
		postcard["msg"] = mMessage;
		postcard["pos-global"] = mGlobalPosition.getValue();
		return postcard;
	}

private:
	std::string	mEmailFrom;
	std::string	mNameFrom;
	std::string	mEmailTo;
	std::string	mSubject;
	std::string	mMessage;
	LLVector3d	mGlobalPosition;
};

//static
LLFloaterPostcard* LLFloaterPostcard::showFromSnapshot(LLImageJPEG* jpeg,
													   LLViewerTexture* img,
													   const LLVector2& scale,
													   const LLVector3d& pos)
{
	// Take the images from the caller. It is now our job to clean them up.
	return new LLFloaterPostcard(jpeg, img, scale, pos);
}

LLFloaterPostcard::LLFloaterPostcard(LLImageJPEG* jpeg, LLViewerTexture* img,
									 const LLVector2& img_scale,
									 const LLVector3d& pos_taken_global)
:	LLFloater("postcard"),
	mJPEGImage(jpeg),
	mViewerImage(img),
	mImageScale(img_scale),
	mPosTakenGlobal(pos_taken_global),
	mHasFirstMsgFocus(false)
{
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_postcard.xml");
	sInstances.insert(this);
}

LLFloaterPostcard::~LLFloaterPostcard()
{
	sInstances.erase(this);
	mJPEGImage = NULL;		// dereferences the image (LLPointer)
}

bool LLFloaterPostcard::postBuild()
{
	if (gIsInSecondLife)
	{
		sUserEmail = getString("undisclosed");
	}
	else if (sUserEmail.empty())
	{
		gAgent.sendAgentUserInfoRequest();
	}

	childSetAction("cancel_btn", onClickCancel, this);
	childSetAction("send_btn", onClickSend, this);

	mFromLine = getChild<LLLineEditor>("from_form");
	mFromLine->setText(sUserEmail);
	mFromLine->setEnabled(sUserEmail.empty());

	LLUIString subject = getString("default_subject");
	subject.setArg("[GRID]", LLGridManager::getInstance()->getGridLabel());
	childSetValue("subject_form", LLSD::String(subject));

	std::string name_string;
	gAgent.buildFullname(name_string);
	childSetValue("name_form", LLSD(name_string));

	mMessageText = getChild<LLTextEditor>("msg_form");
	mMessageText->setWordWrap(true);
	// The first time a user focuses to the msg box, all text will be selected.
	mMessageText->setFocusChangedCallback(onMsgFormFocusReceived, this);

	childSetFocus("to_form", true);

    return true;
}

void LLFloaterPostcard::draw()
{
	LLGLSUIDefault gls_ui;
	LLFloater::draw();

	if (!isMinimized() && mViewerImage.notNull() && mJPEGImage.notNull())
	{
		LLRect rect(getRect());

		// first set the max extents of our preview
		rect.translate(-rect.mLeft, -rect.mBottom);
		rect.mLeft += 280;
		rect.mRight -= 10;
		rect.mTop -= 20;
		rect.mBottom = rect.mTop - 130;

		// then fix the aspect ratio
		F32 ratio = (F32)mJPEGImage->getWidth() / (F32)mJPEGImage->getHeight();
		if ((F32)rect.getWidth() / (F32)rect.getHeight() >= ratio)
		{
			rect.mRight = (S32)((F32)rect.mLeft + (F32)rect.getHeight() * ratio);
		}
		else
		{
			rect.mBottom = (S32)((F32)rect.mTop - (F32)rect.getWidth() / ratio);
		}

		gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);
		gl_rect_2d(rect, LLColor4(0.f, 0.f, 0.f, 1.f));
		rect.stretch(-1);

		gGL.matrixMode(LLRender::MM_TEXTURE);
		gGL.pushMatrix();
		{
			gGL.scalef(mImageScale.mV[VX], mImageScale.mV[VY], 1.f);
			gGL.matrixMode(LLRender::MM_MODELVIEW);
			gl_draw_scaled_image(rect.mLeft, rect.mBottom, rect.getWidth(),
								 rect.getHeight(), mViewerImage,
								 LLColor4::white);
		}
		gGL.matrixMode(LLRender::MM_TEXTURE);
		gGL.popMatrix();
		gGL.matrixMode(LLRender::MM_MODELVIEW);
	}
}

//static
void LLFloaterPostcard::onClickCancel(void* userdata)
{
	LLFloaterPostcard* self = (LLFloaterPostcard*)userdata;
	if (self)
	{
		self->close();
	}
}

//static
void LLFloaterPostcard::onClickSend(void* userdata)
{
	LLFloaterPostcard* self = (LLFloaterPostcard*)userdata;
	if (!self)
	{
		return;
	}

	std::string to = self->childGetValue("to_form").asString();
	if (to.empty())
	{
		gNotifications.add("PromptRecipientEmail");
		return;
	}

	std::string from = self->mFromLine->getText();
	if (!gIsInSecondLife && from.empty())
	{
		self->mFromLine->setEnabled(true);
		gNotifications.add("PromptSelfEmail");
		return;
	}

	try
	{
		std::regex email_format("[A-Za-z0-9.%+-_]+@[A-Za-z0-9.-]+\\.[A-Za-z]{2,}(,[ \t]*[A-Za-z0-9.%+-_]+@[A-Za-z0-9.-]+\\.[A-Za-z]{2,})*");
		if (!std::regex_match(to, email_format))
		{
			gNotifications.add("PromptRecipientEmail");
			return;
		}
		if (!gIsInSecondLife && !std::regex_match(from, email_format))
		{
			self->mFromLine->setEnabled(true);
			gNotifications.add("PromptSelfEmail");
			return;
		}
	}
	catch (std::regex_error& e)
	{
		llwarns << "Regex error: " << e.what() << llendl;
	}

	std::string subject(self->childGetValue("subject_form").asString());
	if (subject.empty() || !self->mHasFirstMsgFocus)
	{
		gNotifications.add("PromptMissingSubjMsg", LLSD(), LLSD(),
						   boost::bind(&LLFloaterPostcard::missingSubjMsgAlertCallback,
									   self, _1, _2));
		return;
	}

	if (self->mJPEGImage.notNull())
	{
		self->sendPostcard();
	}
	else
	{
		gNotifications.add("ErrorProcessingSnapshot");
	}
}

void LLFloaterPostcard::onMsgFormFocusReceived(LLFocusableElement* receiver,
											   void* userdata)
{
	LLFloaterPostcard* self = (LLFloaterPostcard*)userdata;
	if (self)
	{
		if (!self->mHasFirstMsgFocus &&
			receiver == self->mMessageText && self->mMessageText->hasFocus())
		{
			self->mHasFirstMsgFocus = true;
			self->mMessageText->setText(LLStringUtil::null);
		}
	}
}

bool LLFloaterPostcard::missingSubjMsgAlertCallback(const LLSD& notification,
													const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) == 0)
	{
		// User clicked OK
		if ((childGetValue("subject_form").asString()).empty())
		{
			// Stuff the subject back into the form.
			LLUIString subject = getString("default_subject");
			subject.setArg("[GRID]",
						   LLGridManager::getInstance()->getGridLabel());
			childSetValue("subject_form", LLSD::String(subject));
		}

		if (!mHasFirstMsgFocus)
		{
			// The user never switched focus to the messagee window.
			// Using the default string.
			mMessageText->setValue(getString("default_message"));
		}

		sendPostcard();
	}
	return false;
}

//static
void LLFloaterPostcard::sendPostcardFinished(LLSD result, void* userdata)
{
	std::string state = result["state"].asString();
	llinfos << state << llendl;

	LLFloaterPostcard* self = (LLFloaterPostcard*)userdata;
	if (self && sInstances.count(self))
	{
		self->close();
	}
}

void LLFloaterPostcard::sendPostcard()
{
	// Remove any dependency on another floater so that we can be sure to
	// outlive it while we need to.
	LLFloater* dependee = getDependee();
	if (dependee)
	{
		dependee->removeDependentFloater(this);
	}

	// Upload the image
	const std::string& url = gAgent.getRegionCapability("SendPostcard");
	if (!url.empty())
	{
		llinfos << "Sending Postcard via capability" << llendl;

		std::string name_from = childGetValue("name_form").asString();
		std::string email_to = childGetValue("to_form").asString();
		std::string subject = childGetValue("subject_form").asString();
		std::string message = mMessageText->getValue().asString();

		LLResourceUploadInfo::ptr_t
			info(new LLPostcardUploadInfo(mFromLine->getText(), name_from,
										  email_to, subject, message,
										  mPosTakenGlobal, mJPEGImage,
										  boost::bind(&LLFloaterPostcard::sendPostcardFinished,
													  _4, (void*)this)));
		LLViewerAssetUpload::enqueueInventoryUpload(url, info);
		// Give the user some feedback of the event
		gViewerWindowp->playSnapshotAnimAndSound();
	}
	else
	{
		gNotifications.add("PostcardsUnavailable");
		close();
	}
}

//static
void LLFloaterPostcard::updateUserInfo(const std::string& email)
{
	if (gIsInSecondLife) return;

	sUserEmail = email;
	for (instance_list_t::iterator iter = sInstances.begin();
		 iter != sInstances.end(); ++iter)
	{
		LLFloaterPostcard* instance = *iter;
		if (instance->mFromLine->getText().empty())
		{
			// there's no text in this field yet, pre-populate
			instance->mFromLine->setText(email);
			instance->mFromLine->setEnabled(false);
		}
	}
}
