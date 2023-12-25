/**
 * @file llprogressview.cpp
 * @brief LLProgressView class implementation
 *
 * $LicenseInfo:firstyear=2002&license=viewergpl$
 *
 * Copyright (c) 2002-2009, Linden Research, Inc.
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

#include "llprogressview.h"

#include "llbutton.h"
#include "llgl.h"
#include "llimagegl.h"
#include "llprogressbar.h"
#include "llrender.h"
#include "lluictrlfactory.h"

#include "llagent.h"
#include "llappviewer.h"
#include "llstartup.h"
#include "llviewercontrol.h"
#include "llviewertexturelist.h"
#include "llviewerwindow.h"
#include "llweb.h"

LLProgressView* LLProgressView::sInstance = NULL;

S32 gStartImageWidth = 1;
S32 gStartImageHeight = 1;

constexpr F32 FADE_IN_TIME = 1.f;
const std::string ANIMATION_FILENAME = "Login Sequence ";
const std::string ANIMATION_SUFFIX = ".jpg";

LLProgressView::LLProgressView(const std::string& name, const LLRect& rect)
:	LLPanel(name, rect, false),
	mPercentDone(0.f),
	mURLInMessage(false),
	mMouseDownInActiveArea(false)
{
	LLUICtrlFactory::getInstance()->buildPanel(this, "panel_progress.xml");
	reshape(rect.getWidth(), rect.getHeight());
	sInstance = this;
}

bool LLProgressView::postBuild()
{
	mProgressBar = getChild<LLProgressBar>("login_progress_bar");

	mCancelBtn = getChild<LLButton>("cancel_btn");
	mCancelBtn->setClickedCallback(LLProgressView::onCancelButtonClicked);
	mFadeTimer.stop();

	getChild<LLTextBox>("title_text")->setText(gSecondLife);

	getChild<LLTextBox>("message_text")->setClickedCallback(onClickMessage,
															this);

	return true;
}

LLProgressView::~LLProgressView()
{
	gFocusMgr.releaseFocusIfNeeded(this);
	sInstance = NULL;
}

bool LLProgressView::handleHover(S32 x, S32 y, MASK mask)
{
	if (!childrenHandleHover(x, y, mask))
	{
		gViewerWindowp->setCursor(UI_CURSOR_WAIT);
	}
	return true;
}

bool LLProgressView::handleKeyHere(KEY key, MASK mask)
{
	// Suck up all keystokes except CTRL-Q.
	if (key == 'Q' && mask == MASK_CONTROL)
	{
		gAppViewerp->userQuit();
	}
	return true;
}

void LLProgressView::setVisible(bool visible)
{
	if (!visible && getVisible())
	{
		mFadeTimer.start();
	}
	else if (visible && !getVisible())
	{
		gFocusMgr.setTopCtrl(this);
		setFocus(true);
		mFadeTimer.stop();
		mProgressTimer.start();
		LLPanel::setVisible(visible);
	}
}

void LLProgressView::draw()
{
	static LLTimer timer;

	LLTexUnit* unit0 = gGL.getTexUnit(0);

	// Paint bitmap if we have got one
	gGL.pushMatrix();
	if (gStartTexture)
	{
		LLGLSUIDefault gls_ui;
		unit0->bind(gStartTexture);
		gGL.color4f(1.f, 1.f, 1.f,
					mFadeTimer.getStarted() ?
						clamp_rescale(mFadeTimer.getElapsedTimeF32(),
									  0.f, FADE_IN_TIME, 1.f, 0.f)
											: 1.f);
		F32 image_aspect = (F32)gStartImageWidth / (F32)gStartImageHeight;
		S32 width = getRect().getWidth();
		S32 height = getRect().getHeight();
		F32 view_aspect = (F32)width / (F32)height;
		// stretch image to maintain aspect ratio
		if (image_aspect > view_aspect)
		{
			gGL.translatef(-0.5f * (image_aspect / view_aspect - 1.f) * width,
						   0.f, 0.f);
			gGL.scalef(image_aspect / view_aspect, 1.f, 1.f);
		}
		else
		{
			gGL.translatef(0.f,
						   -0.5f * (view_aspect / image_aspect - 1.f) * height,
						   0.f);
			gGL.scalef(1.f, view_aspect / image_aspect, 1.f);
		}
		gl_rect_2d_simple_tex(getRect().getWidth(), getRect().getHeight());
		unit0->unbind(LLTexUnit::TT_TEXTURE);
	}
	else
	{
		unit0->unbind(LLTexUnit::TT_TEXTURE);
		gGL.color4f(0.f, 0.f, 0.f, 1.f);
		gl_rect_2d(getRect());
	}
	gGL.popMatrix();

	// Handle fade-in animation
	if (mFadeTimer.getStarted())
	{
		LLPanel::draw();
		if (mFadeTimer.getElapsedTimeF32() > FADE_IN_TIME)
		{
			gFocusMgr.removeTopCtrlWithoutCallback(this);
			LLPanel::setVisible(false);
			gStartTexture = NULL;
		}
		return;
	}

	LLPanel::draw();
}

void LLProgressView::setText(const std::string& text)
{
	getChild<LLTextBox>("progress_text")->setWrappedText(text);
}

void LLProgressView::setPercent(F32 percent)
{
	mProgressBar->setPercent(percent);
}

void LLProgressView::setMessage(const std::string& msg)
{
	mMessage = msg;
	mURLInMessage = mMessage.find("https://") != std::string::npos ||
					mMessage.find("http://") != std::string::npos ||
					mMessage.find("ftp://") != std::string::npos;

	getChild<LLTextBox>("message_text")->setWrappedText(mMessage);
	getChild<LLTextBox>("message_text")->setHoverActive(mURLInMessage);
}

void LLProgressView::setCancelButtonVisible(bool b, const std::string& label)
{
	mCancelBtn->setVisible(b);
	mCancelBtn->setEnabled(b);
	mCancelBtn->setLabelSelected(label);
	mCancelBtn->setLabelUnselected(label);
}

//static
void LLProgressView::onCancelButtonClicked(void*)
{
	if (gAgent.teleportInProgress())
	{
		gAgent.teleportCancel();
		sInstance->mCancelBtn->setEnabled(false);
		sInstance->setVisible(false);
	}
	else
	{
		llinfos << "User requested quit during login." << llendl;
		gAppViewerp->requestQuit();
	}
}

//static
void LLProgressView::onClickMessage(void* data)
{
	LLProgressView* viewp = (LLProgressView*)data;
	if (!viewp || viewp->mMessage.empty())
	{
		return;
	}

	size_t start_pos = viewp->mMessage.find("https://");
	if (start_pos == std::string::npos)
	{
		start_pos = viewp->mMessage.find("http://");
	}
	if (start_pos == std::string::npos)
	{
		start_pos = viewp->mMessage.find("ftp://");
	}
	if (start_pos == std::string::npos)
	{
		return;
	}

	std::string url_to_open;

	size_t end_pos = viewp->mMessage.find_first_of(" \n\r\t", start_pos);
	if (end_pos != std::string::npos)
	{
		url_to_open = viewp->mMessage.substr(start_pos, end_pos - start_pos);
	}
	else
	{
		url_to_open = viewp->mMessage.substr(start_pos);
	}

	LLWeb::loadURLExternal(url_to_open);
}
