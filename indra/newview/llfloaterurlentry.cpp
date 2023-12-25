/**
 * @file llfloaterurlentry.cpp
 * @brief LLFloaterURLEntry class implementation
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

#include "llfloaterurlentry.h"

#include "llcombobox.h"
#include "llcorehttputil.h"
#include "llmimetypes.h"
#include "lluictrlfactory.h"
#include "llurlhistory.h"
#include "llwindow.h"

#include "llpanelface.h"
#include "llpanellandmedia.h"

static LLFloaterURLEntry* sInstance = NULL;

LLFloaterURLEntry::LLFloaterURLEntry(LLHandle<LLPanel> parent)
:	LLFloater(),
	mParentPanelHandle(parent)
{
	LLUICtrlFactory::getInstance()->buildFloater(this, "floater_url_entry.xml");

	mMediaURLCombo = getChild<LLComboBox>("media_entry");

	// Cancel button
	childSetAction("cancel_btn", onBtnCancel, this);

	// Cancel button
	childSetAction("clear_btn", onBtnClear, this);

	// clear media list button
	LLSD parcel_history = LLURLHistory::getURLHistory("parcel");
	bool enable_clear_button = parcel_history.size() > 0 ? true : false;
	childSetEnabled("clear_btn", enable_clear_button);

	// OK button
	childSetAction("ok_btn", onBtnOK, this);

	setDefaultBtn("ok_btn");
	buildURLHistory();

	sInstance = this;
}

LLFloaterURLEntry::~LLFloaterURLEntry()
{
	sInstance = NULL;
}

void LLFloaterURLEntry::buildURLHistory()
{
	mMediaURLCombo->operateOnAll(LLComboBox::OP_DELETE);

	// Get all of the entries in the "parcel" collection
	LLSD parcel_history = LLURLHistory::getURLHistory("parcel");
	for (LLSD::array_iterator iter = parcel_history.beginArray(),
							  end = parcel_history.endArray();
		 iter != end; ++iter)
	{
		mMediaURLCombo->addSimpleElement(iter->asString());
	}
}

void LLFloaterURLEntry::headerFetchComplete(S32 status,
											const std::string& mime_type)
{
	LLPanelLandMedia* panel_media =
		dynamic_cast<LLPanelLandMedia*>(mParentPanelHandle.get());
	if (panel_media)
	{
		// 'status' is ignored for now -- error = "none/none"
		panel_media->setMediaType(mime_type);
		panel_media->setMediaURL(mMediaURLCombo->getValue().asString());
	}
	else
	{
		LLPanelFace* panel_face =
			dynamic_cast<LLPanelFace*>(mParentPanelHandle.get());
		if (panel_face)
		{
			panel_face->setMediaType(mime_type);
			panel_face->setMediaURL(mMediaURLCombo->getValue().asString());
		}
	}

	// Decrement the cursor
	gWindowp->decBusyCount();
	childSetVisible("loading_label", false);
	close();
}

//static
LLHandle<LLFloater> LLFloaterURLEntry::show(LLHandle<LLPanel> parent,
											const std::string media_url)
{
	if (sInstance)
	{
		sInstance->open();
	}
	else
	{
		sInstance = new LLFloaterURLEntry(parent);
	}
	sInstance->addURLToCombobox(media_url);
	return sInstance->getHandle();
}

bool LLFloaterURLEntry::addURLToCombobox(const std::string& media_url)
{
	if (!mMediaURLCombo->setSimple(media_url) && ! media_url.empty())
	{
		mMediaURLCombo->add(media_url);
		mMediaURLCombo->setSimple(media_url);
		return true;
	}

	// URL was not added for whatever reason (either it was empty or already
	// existed)
	return false;
}

//static
void LLFloaterURLEntry::onBtnOK(void* userdata)
{
	LLFloaterURLEntry* self =(LLFloaterURLEntry*)userdata;
	if (!self) return;

	std::string media_url = self->mMediaURLCombo->getValue().asString();
	self->mMediaURLCombo->remove(media_url);
	LLURLHistory::removeURL("parcel", media_url);
	if (self->addURLToCombobox(media_url))
	{
		// Add this url to the parcel collection
		LLURLHistory::addURL("parcel", media_url);
	}

	// show progress bar here?
	gWindowp->incBusyCount();
	self->childSetVisible("loading_label", true);

	// Leading whitespace causes problems with the MIME-type detection so strip
	// it
	LLStringUtil::trim(media_url);

	// First check the URL scheme
	LLURI url(media_url);
	std::string scheme = url.scheme();

	// We assume that an empty scheme is an http url, as this is how we will
	// treat it.
	if (scheme == "")
	{
		scheme = "http";
	}

	// Discover the MIME type only for "http" scheme.
	if (scheme == "http" || scheme == "https")
	{
		gCoros.launch("LLFloaterURLEntry::getMediaTypeCoro",
					  boost::bind(&LLFloaterURLEntry::getMediaTypeCoro,
								  media_url, self->getHandle()));
	}
	else
	{
		self->headerFetchComplete(0, scheme);
	}

	// Grey the buttons until we get the header response
	self->childSetEnabled("ok_btn", false);
	self->childSetEnabled("cancel_btn", false);
	self->mMediaURLCombo->setEnabled(false);
}

//static
void LLFloaterURLEntry::getMediaTypeCoro(const std::string& url,
										 LLHandle<LLFloater> handle)
{
	LLCore::HttpOptions::ptr_t options(new LLCore::HttpOptions);
	options->setHeadersOnly(true);

	LLCoreHttpUtil::HttpCoroutineAdapter adapter("getMediaTypeCoro");
	LLSD result = adapter.getAndSuspend(url, options);

	LLFloaterURLEntry* self;
	self = handle.isDead() ? NULL
						   : dynamic_cast<LLFloaterURLEntry*>(handle.get());
	if (!self)
	{
		llwarns << "Floater closed before response." << llendl;
		return;
	}

	std::string resolved_mime_type = LLMIMETypes::getDefaultMimeType();

	const LLSD& http_results =
		result[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS];
	LLCore::HttpStatus status =
		LLCoreHttpUtil::HttpCoroutineAdapter::getStatusFromLLSD(http_results);
	if (status)
	{
		const LLSD& headers =
			http_results[LLCoreHttpUtil::HttpCoroutineAdapter::HTTP_RESULTS_HEADERS];
		if (headers.has(HTTP_IN_HEADER_CONTENT_TYPE))
		{
			std::string media_type = headers[HTTP_IN_HEADER_CONTENT_TYPE];
			size_t idx = media_type.find_first_of(";");
			if (idx != std::string::npos)
			{
				media_type = media_type.substr(0, idx);
			}
			if (!media_type.empty())
			{
				resolved_mime_type = media_type;
			}
		}
	}

	self->headerFetchComplete(status.getType(), resolved_mime_type);
}

//static
void LLFloaterURLEntry::onBtnCancel(void* userdata)
{
	LLFloaterURLEntry* self = (LLFloaterURLEntry*)userdata;
	if (self)
	{
		self->close();
	}
}

//static
void LLFloaterURLEntry::onBtnClear(void* userdata)
{
	gNotifications.add("ConfirmClearMediaUrlList", LLSD(), LLSD(),
					   boost::bind(&LLFloaterURLEntry::callback_clear_url_list,
								   (LLFloaterURLEntry*)userdata, _1, _2));
}

bool LLFloaterURLEntry::callback_clear_url_list(const LLSD& notification,
												const LLSD& response)
{
	if (LLNotification::getSelectedOption(notification, response) == 0) // YES
	{
		// Clear saved list
		mMediaURLCombo->operateOnAll(LLComboBox::OP_DELETE);

		// Clear current contents of combo box
		mMediaURLCombo->clear();

		// Clear stored version of list
		LLURLHistory::clear("parcel");

		// Cleared the list so disable Clear button
		childSetEnabled("clear_btn", false);
	}
	return false;
}
