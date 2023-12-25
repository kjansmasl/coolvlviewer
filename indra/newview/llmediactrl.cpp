/**
 * @file LLMediaCtrl.cpp
 * @brief Web browser UI control
 *
 * $LicenseInfo:firstyear=2006&license=viewergpl$
 *
 * Copyright (c) 2006-2009, Linden Research, Inc.
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

#include "dullahan_version.h"

#include "llmediactrl.h"

#include "lldir.h"
#include "llfloater.h"
#include "llhttpconstants.h"
#include "llkeyboard.h"
#include "llnotifications.h"
#include "llpluginclassmedia.h"
#include "llrender.h"
#include "llrenderutils.h"		// For gl_rect_2d()
#include "lluictrlfactory.h"
#include "llviewborder.h"

#include "llappviewer.h"		// For gRestoreGL
#include "llcommandhandler.h"
#include "llfloaterworldmap.h"
#include "llslurl.h"			// For SLURL_*_SCHEME
#include "llurldispatcher.h"
#include "llviewercontrol.h"
#include "llviewermedia.h"
#include "llviewertexture.h"
#include "llviewerwindow.h"

//static
LLMediaCtrl::instances_list_t LLMediaCtrl::sMediaCtrlInstances;

//-----------------------------------------------------------------------------
// LLMediaCtrl
//-----------------------------------------------------------------------------

static const std::string LL_WEB_BROWSER_CTRL_TAG = "web_browser";
static LLRegisterWidget<LLMediaCtrl> r(LL_WEB_BROWSER_CTRL_TAG);

LLMediaCtrl::LLMediaCtrl(const std::string& name, const LLRect& rect)
:	LLUICtrl(name, rect, false, NULL, NULL),
	mTextureDepthBytes(4),
	mBorder(NULL),
	mFrequentUpdates(true),
	mForceUpdate(false),
	mTrusted(false),
	mCurrentNavUrl("about:blank"),
	mAlwaysRefresh(false),
	mMediaSource(NULL),
	mStretchToFill(true),
	mDecoupleTextureSize(false),
	mTextureWidth(1024),
	mTextureHeight(1024),
	mMaintainAspectRatio(true),
	mHidingInitialLoad(true)
{
	sMediaCtrlInstances.insert(this);

	LLRect r = getRect();

	S32 screen_width = ll_roundp((F32)r.getWidth() *
								 LLUI::sGLScaleFactor.mV[VX]);
	S32 screen_height = ll_roundp((F32)r.getHeight() *
								  LLUI::sGLScaleFactor.mV[VY]);
	setTextureSize(screen_width, screen_height);

	mMediaTextureID.generate();

	LLRect border_rect(0, r.getHeight() + 2, r.getWidth() + 2, 0);
	mBorder = new LLViewBorder(std::string("web control border"),
							   border_rect, LLViewBorder::BEVEL_IN);
	addChild(mBorder);
}

// Note: this is now a singleton and destruction happens via initClass() now
//virtual
LLMediaCtrl::~LLMediaCtrl()
{
	if (mMediaSource)
	{
		mMediaSource->remObserver(this);
		mMediaSource = NULL;
	}
	sMediaCtrlInstances.erase(this);
}

bool LLMediaCtrl::ensureMediaSourceExists()
{
	if (mMediaSource.notNull())
	{
		return true;
	}

	// If we do not already have a media source, try to create one.
	mMediaSource = LLViewerMedia::newMediaImpl(mMediaTextureID, mTextureWidth,
											   mTextureHeight);
	if (mMediaSource.isNull())
	{
		llwarns << "Media source creation failed for media texture Id: "
				<< mMediaTextureID << llendl;
		return false;
	}

	mMediaSource->setUsedInUI(true);
	mMediaSource->setHomeURL(mHomePageUrl, mHomePageMimeType);
	mMediaSource->setTarget(mTarget);
	mMediaSource->setTrustedBrowser(mTrusted);
	mMediaSource->setVisible(getVisible());
	mMediaSource->addObserver(this);
#if 0
	mMediaSource->setBackgroundColor(getBackgroundColor());
#endif
	static LLCachedControl<F32> scale(gSavedSettings, "CEFScaleFactor");
	F64 sf = llmax((F64)scale, 0.1);
	mMediaSource->setPageZoomFactor((F64)LLUI::sGLScaleFactor.mV[VX] * sf);

	return true;
}

void LLMediaCtrl::setBorderVisible(bool border_visible)
{
	if (mBorder)
	{
		mBorder->setVisible(border_visible);
	}
}

void LLMediaCtrl::setTrusted(bool trusted)
{
	mTrusted = trusted;
	if (mMediaSource)
	{
		mMediaSource->setTrustedBrowser(mTrusted);
	}
}

//virtual
bool LLMediaCtrl::handleHover(S32 x, S32 y, MASK mask)
{
	if (mMediaSource)
	{
		convertInputCoords(x, y);
		mMediaSource->mouseMove(x, y, mask);
		gViewerWindowp->setCursor(mMediaSource->getLastSetCursor());
		return true;
	}
	else
	{
		LL_DEBUGS("Media") << "No media source, passing event to LLUICtrl"
						   << LL_ENDL;
		return LLUICtrl::handleHover(x, y, mask);
	}
}

//virtual
bool LLMediaCtrl::handleScrollWheel(S32 x, S32 y, S32 clicks)
{
	if (mMediaSource && mMediaSource->hasMedia())
	{
		convertInputCoords(x, y);
		MASK mask = gKeyboardp ? gKeyboardp->currentMask(true) : 0;
		mMediaSource->scrollWheel(x, y, 0, clicks, mask);
		return true;
	}
	else
	{
		LL_DEBUGS("Media") << "No active media, passing event to LLUICtrl"
						   << LL_ENDL;
		return LLUICtrl::handleScrollWheel(x, y, clicks);
	}
}

//virtual
bool LLMediaCtrl::handleMouseUp(S32 x, S32 y, MASK mask)
{
	if (mMediaSource)
	{
		convertInputCoords(x, y);
		mMediaSource->mouseUp(x, y, mask);
		gFocusMgr.setMouseCapture(NULL);
		return true;
	}
	else
	{
		LL_DEBUGS("Media") << "No media source, passing event to LLUICtrl"
						   << LL_ENDL;
		return LLUICtrl::handleMouseUp(x, y, mask);
	}
}

//virtual
bool LLMediaCtrl::handleMouseDown(S32 x, S32 y, MASK mask)
{
	if (mMediaSource)
	{
		convertInputCoords(x, y);
		mMediaSource->mouseDown(x, y, mask);

		gFocusMgr.setMouseCapture(this);
		setFocus(true);

		return true;
	}
	else
	{
		LL_DEBUGS("Media") << "No media source, passing event to LLUICtrl"
						   << LL_ENDL;
		return LLUICtrl::handleMouseDown(x, y, mask);
	}
}

//virtual
bool LLMediaCtrl::handleRightMouseUp(S32 x, S32 y, MASK mask)
{
	if (mMediaSource)
	{
		convertInputCoords(x, y);
		mMediaSource->mouseUp(x, y, mask, 1);

		gFocusMgr.setMouseCapture(NULL);

		return true;
	}
	else
	{
		LL_DEBUGS("Media") << "No media source, passing event to LLUICtrl"
						   << LL_ENDL;
		return LLUICtrl::handleRightMouseUp(x, y, mask);
	}
}

//virtual
bool LLMediaCtrl::handleRightMouseDown(S32 x, S32 y, MASK mask)
{
	if (mMediaSource)
	{
		convertInputCoords(x, y);
		mMediaSource->mouseDown(x, y, mask, 1);

		gFocusMgr.setMouseCapture(this);
		setFocus(true);

		return true;
	}
	else
	{
		LL_DEBUGS("Media") << "No media source, passing event to LLUICtrl"
						   << LL_ENDL;
		return LLUICtrl::handleRightMouseDown(x, y, mask);
	}
}

//virtual
bool LLMediaCtrl::handleDoubleClick(S32 x, S32 y, MASK mask)
{
	if (mMediaSource)
	{
		convertInputCoords(x, y);
		mMediaSource->mouseDoubleClick(x, y, mask);

		gFocusMgr.setMouseCapture(this);
		setFocus(true);

		return true;
	}
	else
	{
		LL_DEBUGS("Media") << "No media source, passing event to LLUICtrl"
						   << LL_ENDL;
		return LLUICtrl::handleDoubleClick(x, y, mask);
	}
}

//virtual
void LLMediaCtrl::onFocusReceived()
{
	if (mMediaSource)
	{
		mMediaSource->focus(true);

		// Set focus for edit menu items
		gEditMenuHandlerp = mMediaSource;
	}

	LLUICtrl::onFocusReceived();
}

//virtual
void LLMediaCtrl::onFocusLost()
{
	if (mMediaSource)
	{
		mMediaSource->focus(false);

		if (gEditMenuHandlerp == mMediaSource)
		{
			// Clear focus for edit menu items
			gEditMenuHandlerp = NULL;
		}
	}

	gViewerWindowp->focusClient();

	LLUICtrl::onFocusLost();
}

//virtual
bool LLMediaCtrl::handleKeyHere(KEY key, MASK mask)
{
	bool result = false;

	if (mMediaSource)
	{
		if (LLView::sDebugKeys)
		{
			llinfos << "Key handling passed to the media source" << llendl;
		}
		result = mMediaSource->handleKeyHere(key, mask);
	}

	if (!result)
	{
		if (LLView::sDebugKeys)
		{
			llinfos << "Key handling passed to the UI control" << llendl;
		}
		result = LLUICtrl::handleKeyHere(key, mask);
	}

	return result;
}

//virtual
bool LLMediaCtrl::handleKeyUpHere(KEY key, MASK mask)
{
	bool result = false;

	if (mMediaSource)
	{
		result = mMediaSource->handleKeyUpHere(key, mask);
	}

	if (!result)
	{
		result = LLUICtrl::handleKeyUpHere(key, mask);
	}

	return result;
}

//virtual
bool LLMediaCtrl::handleUnicodeCharHere(llwchar uni_char)
{
	bool result = false;

	// Only accept 'printable' characters, sigh...
	if (uni_char >= 32  &&	// discard 'control' characters
		uni_char != 127)	// SDL thinks this is 'delete' - yuck.
	{
		if (LLView::sDebugKeys)
		{
			llinfos << "Key handling passed to the media source" << llendl;
		}
		if (mMediaSource)
		{
			result = mMediaSource->handleUnicodeCharHere(uni_char);
		}
	}

	if (!result)
	{
		if (LLView::sDebugKeys)
		{
			llinfos << "Key handling passed to the UI control" << llendl;
		}
		result = LLUICtrl::handleUnicodeCharHere(uni_char);
	}

	return result;
}

#if 0	// Not used
//virtual
void LLMediaCtrl::handleVisibilityChange(bool new_visibility)
{
	llinfos << "Visibility changed to " << (new_visibility ? "true" : "false")
			<< llendl;
	if (mMediaSource)
	{
		mMediaSource->setVisible(new_visibility);
	}
}
#endif

//virtual
void LLMediaCtrl::onVisibilityChange(bool new_visibility)
{
	// Set state of frequent updates automatically if visibility changes
	if (new_visibility)
	{
		mFrequentUpdates = true;
	}
	else
	{
		mFrequentUpdates = false;
	}
	LLUICtrl::onVisibilityChange(new_visibility);
}

//virtual
void LLMediaCtrl::reshape(S32 width, S32 height, bool called_from_parent)
{
	if (!mDecoupleTextureSize)
	{
		S32 screen_width = ll_round((F32)width * LLUI::sGLScaleFactor.mV[VX]);
		S32 screen_height = ll_round((F32)height *
									 LLUI::sGLScaleFactor.mV[VY]);

		// When floater is minimized, these sizes are negative
		if (screen_height > 0 && screen_width > 0)
		{
			setTextureSize(screen_width, screen_height);
		}
	}

	LLUICtrl::reshape(width, height, called_from_parent);
}

void LLMediaCtrl::navigateBack()
{
	if (mMediaSource && mMediaSource->hasMedia())
	{
		mMediaSource->getMediaPlugin()->browse_back();
	}
}

void LLMediaCtrl::navigateForward()
{
	if (mMediaSource && mMediaSource->hasMedia())
	{
		mMediaSource->getMediaPlugin()->browse_forward();
	}
}

bool LLMediaCtrl::canNavigateBack()
{
	return mMediaSource && mMediaSource->canNavigateBack();
}

bool LLMediaCtrl::canNavigateForward()
{
	return mMediaSource && mMediaSource->canNavigateForward();
}

void LLMediaCtrl::navigateTo(std::string url_in, std::string mime_type)
{
	// Do not browse to anything that got a SLURL scheme:
	size_t i = url_in.find("://");
	if (i != std::string::npos)
	{
		std::string scheme = url_in.substr(0, i);
		LLStringUtil::toLower(scheme);
		if (scheme == LLSLURL::SLURL_SECONDLIFE_SCHEME ||
			scheme == LLSLURL::SLURL_HOP_SCHEME ||
			scheme == LLSLURL::SLURL_X_GRID_INFO_SCHEME ||
			scheme == LLSLURL::SLURL_X_GRID_LOCATION_INFO_SCHEME)
		{
			llwarns << "Attempted to navigate to a SLURL: " << url_in
					<< " - Aborted." << llendl;
			return;
		}
	}

	if (mCurrentNavUrl == url_in || !ensureMediaSourceExists())
	{
		return;	// Nothing to do...
	}

	mCurrentNavUrl = url_in;
	mMediaSource->setSize(mTextureWidth, mTextureHeight);
	mMediaSource->navigateTo(url_in, mime_type, mime_type.empty(),
							 // Not a server request and not filtering
							 false, false);
}

void LLMediaCtrl::navigateToLocalPage(const std::string& subdir,
									  const std::string& filename_in)
{
	std::string language = LLUI::getLanguage();
	std::string filename = subdir + LL_DIR_DELIM_STR + filename_in;
	std::string expanded_filename = gDirUtilp->findSkinnedFilename("html",
																   language,
																   filename);
	bool found = LLFile::exists(expanded_filename);
	if (!found && language != "en-us")
	{
		expanded_filename = gDirUtilp->findSkinnedFilename("html", "en-us",
														   filename);
		found = LLFile::exists(expanded_filename);
	}
	if (!found)
	{
		llwarns << "File '" << expanded_filename << "' not found" << llendl;
		return;
	}
#if LL_WINDOWS
	// Windows filenames are not recognized "as is", unlike UNICES' (since
	// those start with '/')... Let's be more explicit !
	expanded_filename = "file:///" + expanded_filename;
#else
	// CEF does not accept file names without a "file://" prefix
	expanded_filename = "file://" + expanded_filename;
#endif
	navigateTo(expanded_filename, HTTP_CONTENT_TEXT_HTML);
}

void LLMediaCtrl::navigateHome()
{
	if (ensureMediaSourceExists())
	{
		mMediaSource->setSize(mTextureWidth, mTextureHeight);
		mMediaSource->navigateHome();
	}
}

void LLMediaCtrl::setHomePageUrl(const std::string url_in,
								 const std::string& mime_type)
{
	mHomePageUrl = url_in;
	if (mMediaSource)
	{
		mMediaSource->setHomeURL(mHomePageUrl, mime_type);
	}
}

void LLMediaCtrl::setTarget(const std::string& target)
{
	mTarget = target;
	if (mMediaSource)
	{
		mMediaSource->setTarget(mTarget);
	}
}

void LLMediaCtrl::setTextureSize(S32 width, S32 height)
{
	mTextureWidth = width;
	mTextureHeight = height;

	if (mMediaSource)
	{
		mMediaSource->setSize(mTextureWidth, mTextureHeight);
		mForceUpdate = true;
	}
}

LLPluginClassMedia* LLMediaCtrl::getMediaPlugin()
{
	return mMediaSource.isNull() ? NULL : mMediaSource->getMediaPlugin();
}

//virtual
void LLMediaCtrl::draw()
{
	LLRect r = getRect();

	if (gRestoreGL)
	{
		reshape(r.getWidth(), r.getHeight(), false);
		return;
	}

	// NOTE: optimization needed here - probably only need to do this once
	// unless tearoffs change the parent which they probably do.
	const LLUICtrl* ptr = findRootMostFocusRoot();
	if (ptr && ptr->hasFocus())
	{
		setFrequentUpdates(true);
	}
	else
	{
		setFrequentUpdates(false);
	}

	if (mHidingInitialLoad)
	{
		// If we are hiding loading, draw a black background
		gl_rect_2d(0, r.getHeight(), r.getWidth(), 0, LLColor4::black);
		LLUICtrl::draw();
		return;
	}

	LLPluginClassMedia* media_plugin = NULL;
	LLViewerMediaTexture* media_texture = NULL;

	bool draw_media = false;
	if (mMediaSource && mMediaSource->hasMedia())
	{
		media_plugin = mMediaSource->getMediaPlugin();

		if (media_plugin && media_plugin->textureValid())
		{
			media_texture =
				LLViewerTextureManager::findMediaTexture(mMediaTextureID);
			if (media_texture)
			{
				draw_media = true;
			}
		}
	}

	if (!draw_media)
	{
		// Draw a black background instead...
		gl_rect_2d(0, r.getHeight(), r.getWidth(), 0, LLColor4::black);
		LLUICtrl::draw();
		return;
	}

	F32 media_width = (F32)media_plugin->getWidth();
	F32 media_height = (F32)media_plugin->getHeight();
	F32 texture_width = (F32)media_plugin->getTextureWidth();
	F32 texture_height = (F32)media_plugin->getTextureHeight();
	if (media_width <= 0.f || media_height <= 0.f || texture_width <= 0.f ||
		texture_height <= 0.f || r.getWidth() <= 0 || r.getHeight() <= 0)
	{
		// Avoid divide by zero and negative sizes...
		LLUICtrl::draw();
		return;
	}

	// Alpha off for this
	LLGLSUIDefault gls_ui;

	gGL.pushUIMatrix();
	{
		static LLCachedControl<F32> scale(gSavedSettings, "CEFScaleFactor");
		F64 scale_factor = llmax((F64)scale, 0.1);
		mMediaSource->setPageZoomFactor((F64)LLUI::sGLScaleFactor.mV[VX] *
										scale_factor);

		// Scale texture to fit the space using texture coords
		gGL.getTexUnit(0)->bind(media_texture);
		gGL.color4fv(LLColor4::white.mV);
		F32 max_u = media_width / texture_width;
		F32 max_v = media_height / texture_height;

		S32 width, height;
		S32 x_offset = 0;
		S32 y_offset = 0;

		if (mStretchToFill)
		{
			if (mMaintainAspectRatio)
			{
				F32 media_aspect = media_width / media_height;
				F32 view_aspect = (F32)r.getWidth() / (F32)r.getHeight();
				if (media_aspect > view_aspect)
				{
					// Max width, adjusted height
					width = r.getWidth();
					height = llmin(llmax(S32(width / media_aspect), 0),
								   r.getHeight());
				}
				else
				{
					// Max height, adjusted width
					height = r.getHeight();
					width = llmin(llmax(S32(height * media_aspect), 0),
								  r.getWidth());
				}
			}
			else
			{
				width = r.getWidth();
				height = r.getHeight();
			}
		}
		else
		{
			width = llmin((S32)media_width, r.getWidth());
			height = llmin((S32)media_height, r.getHeight());
		}

		x_offset = (r.getWidth() - width) / 2;
		y_offset = (r.getHeight() - height) / 2;

		// Draw the browser
		gGL.setSceneBlendType(LLRender::BT_REPLACE);
		gGL.begin(LLRender::TRIANGLES);
		if (!media_plugin->getTextureCoordsOpenGL())
		{
			// Render using web browser reported width and height, instead of
			// trying to invert GL scale
			gGL.texCoord2f(max_u, 0.f);
			gGL.vertex2i(x_offset + width, y_offset + height);

			gGL.texCoord2f(0.f, 0.f);
			gGL.vertex2i(x_offset, y_offset + height);

			gGL.texCoord2f(0.f, max_v);
			gGL.vertex2i(x_offset, y_offset);

			gGL.texCoord2f(max_u, 0.f);
			gGL.vertex2i(x_offset + width, y_offset + height);

			gGL.texCoord2f(0.f, max_v);
			gGL.vertex2i(x_offset, y_offset);

			gGL.texCoord2f(max_u, max_v);
			gGL.vertex2i(x_offset + width, y_offset);
		}
		else
		{
			// Render using web browser reported width and height, instead of
			// trying to invert GL scale
			gGL.texCoord2f(max_u, max_v);
			gGL.vertex2i(x_offset + width, y_offset + height);

			gGL.texCoord2f(0.f, max_v);
			gGL.vertex2i(x_offset, y_offset + height);

			gGL.texCoord2f(0.f, 0.f);
			gGL.vertex2i(x_offset, y_offset);

			gGL.texCoord2f(max_u, max_v);
			gGL.vertex2i(x_offset + width, y_offset + height);

			gGL.texCoord2f(0.f, 0.f);
			gGL.vertex2i(x_offset, y_offset);

			gGL.texCoord2f(max_u, 0.f);
			gGL.vertex2i(x_offset + width, y_offset);
		}
		gGL.end();
		gGL.setSceneBlendType(LLRender::BT_ALPHA);
	}
	gGL.popUIMatrix();

	// Highlight if keyboard focus here. *TODO: this needs some work.
	if (mBorder && mBorder->getVisible())
	{
		mBorder->setKeyboardFocusHighlight(gFocusMgr.childHasKeyboardFocus(this));
	}

	LLUICtrl::draw();
}

void LLMediaCtrl::convertInputCoords(S32& x, S32& y)
{
	LLPluginClassMedia* plugin = mMediaSource ? mMediaSource->getMediaPlugin()
											  : NULL;
	bool flipped = plugin && plugin->getTextureCoordsOpenGL();

	x = ll_round((F32)x * LLUI::sGLScaleFactor.mV[VX]);
	if (flipped)
	{
		y = ll_round((F32)(getRect().getHeight() - y) *
			LLUI::sGLScaleFactor.mV[VY]);
	}
	else
	{
		y = ll_round((F32)(y) * LLUI::sGLScaleFactor.mV[VY]);
	}
}

//virtual
void LLMediaCtrl::handleMediaEvent(LLPluginClassMedia* self, EMediaEvent event)
{
	if (!self) return;

	switch (event)
	{
		case MEDIA_EVENT_CONTENT_UPDATED:
		{
#if 0
			LL_DEBUGS("Media") << "Media event: MEDIA_EVENT_CONTENT_UPDATED"
							   << LL_ENDL;
#endif
			break;
		}

		case MEDIA_EVENT_TIME_DURATION_UPDATED:
		{
#if 0
			LL_DEBUGS("Media") << "Media event: MEDIA_EVENT_TIME_DURATION_UPDATED, time is "
							   << self->getCurrentTime() << " of "
							   << self->getDuration() << LL_ENDL;
#endif
			break;
		}

		case MEDIA_EVENT_SIZE_CHANGED:
		{
			LL_DEBUGS("Media") << "Media event: MEDIA_EVENT_SIZE_CHANGED"
							   << LL_ENDL;
			LLRect r = getRect();
			reshape(r.getWidth(), r.getHeight(), false);
			break;
		}

		case MEDIA_EVENT_CURSOR_CHANGED:
		{
			LL_DEBUGS("Media") << "Media event: MEDIA_EVENT_CURSOR_CHANGED, new cursor is "
							   << self->getCursorName() << LL_ENDL;
			break;
		}

		case MEDIA_EVENT_NAVIGATE_BEGIN:
		{
			LL_DEBUGS("Media") << "Media event: MEDIA_EVENT_NAVIGATE_BEGIN, url is "
							   << self->getNavigateURI() << LL_ENDL;
			break;
		}

		case MEDIA_EVENT_NAVIGATE_COMPLETE:
		{
			LL_DEBUGS("Media") << "Media event: MEDIA_EVENT_NAVIGATE_COMPLETE, result string is: "
							   << self->getNavigateResultString() << LL_ENDL;
			if (mHidingInitialLoad)
			{
				mHidingInitialLoad = false;
			}
			break;
		}

		case MEDIA_EVENT_NAVIGATE_ERROR_PAGE:
		{
			LL_DEBUGS("Media") << "Media event: MEDIA_EVENT_NAVIGATE_ERROR_PAGE"
							   << LL_ENDL;
			if (mErrorPageURL.length() > 0)
			{
				navigateTo(mErrorPageURL, HTTP_CONTENT_TEXT_HTML);
			}
			break;
		}

		case MEDIA_EVENT_PROGRESS_UPDATED:
		{
			LL_DEBUGS("Media") << "Media event: MEDIA_EVENT_PROGRESS_UPDATED, loading at "
							   << self->getProgressPercent() << "%" << LL_ENDL;
			break;
		}

		case MEDIA_EVENT_STATUS_TEXT_CHANGED:
		{
			LL_DEBUGS("Media") << "Media event: MEDIA_EVENT_STATUS_TEXT_CHANGED, new status text is: "
							   << self->getStatusText() << LL_ENDL;
			break;
		}

		case MEDIA_EVENT_LOCATION_CHANGED:
		{
			mCurrentNavUrl = self->getLocation();
			LL_DEBUGS("Media") << "Media event: MEDIA_EVENT_LOCATION_CHANGED, new uri is: "
							   << mCurrentNavUrl << LL_ENDL;
			break;
		}

		case MEDIA_EVENT_CLICK_LINK_HREF:
		{
			LL_DEBUGS("Media") << "Media event: MEDIA_EVENT_CLICK_LINK_HREF, target is \""
							   << self->getClickTarget() << "\", uri is "
							   << self->getClickURL() << LL_ENDL;
			break;
		}

		case MEDIA_EVENT_CLICK_LINK_NOFOLLOW:
		{
			LL_DEBUGS("Media") << "Media event: MEDIA_EVENT_CLICK_LINK_NOFOLLOW, uri is "
							   << self->getClickURL() << LL_ENDL;
			break;
		}

		case MEDIA_EVENT_PLUGIN_FAILED:
		{
			LL_DEBUGS("Media") << "Media event: MEDIA_EVENT_PLUGIN_FAILED"
							   << LL_ENDL;
			break;
		}

		case MEDIA_EVENT_PLUGIN_FAILED_LAUNCH:
		{
			LL_DEBUGS("Media") <<  "Media event: MEDIA_EVENT_PLUGIN_FAILED_LAUNCH"
							   << LL_ENDL;
			break;
		}

		case MEDIA_EVENT_NAME_CHANGED:
		{
			LL_DEBUGS("Media") << "Media event: MEDIA_EVENT_NAME_CHANGED"
							   << LL_ENDL;
			break;
		}

		case MEDIA_EVENT_CLOSE_REQUEST:
		{
			LL_DEBUGS("Media") << "Media event: MEDIA_EVENT_CLOSE_REQUEST"
							   << LL_ENDL;
			break;
		}

		case MEDIA_EVENT_PICK_FILE_REQUEST:
		{
			LL_DEBUGS("Media") << "Media event: MEDIA_EVENT_PICK_FILE_REQUEST"
							   << LL_ENDL;
			break;
		}

		case MEDIA_EVENT_GEOMETRY_CHANGE:
		{
			LL_DEBUGS("Media") << "Media event: MEDIA_EVENT_GEOMETRY_CHANGE, uuid is "
							   << self->getClickUUID() << LL_ENDL;
			break;
		}

		case MEDIA_EVENT_AUTH_REQUEST:
		{
			LL_DEBUGS("Media") << "Media event: MEDIA_EVENT_AUTH_REQUEST"
							   << LL_ENDL;
			break;
		}

		case MEDIA_EVENT_LINK_HOVERED:
		{
			LL_DEBUGS("Media") << "Unimplemented media event: MEDIA_EVENT_LINK_HOVERED"
							   << LL_ENDL;
			break;
		}

		case MEDIA_EVENT_FILE_DOWNLOAD:
		{
			LL_DEBUGS("Media") << "Unimplemented media event: MEDIA_EVENT_FILE_DOWNLOAD"
							   << LL_ENDL;
			break;
		}

		case MEDIA_EVENT_DEBUG_MESSAGE:
		{
			llinfos << self->getDebugMessageText() << llendl;
			break;
		}
	}

	// Chain all events to any potential observers of this object.
	emitEvent(self, event);
}

//virtual
LLXMLNodePtr LLMediaCtrl::getXML(bool save_children) const
{
	LLXMLNodePtr node = LLUICtrl::getXML();
	node->setName(LL_WEB_BROWSER_CTRL_TAG);
	return node;
}

//static
LLView* LLMediaCtrl::fromXML(LLXMLNodePtr node, LLView* parent,
							 LLUICtrlFactory* factory)
{
	std::string name = LL_WEB_BROWSER_CTRL_TAG;
	node->getAttributeString("name", name);

	std::string start_url;
	node->getAttributeString("start_url", start_url);

	bool border_visible = true;
	node->getAttributeBool("border_visible", border_visible);

	LLRect rect;
	createRect(node, rect, parent, LLRect());

	LLMediaCtrl* self = new LLMediaCtrl(name, rect);

	bool decouple_texture_size = self->getDecoupleTextureSize();
	node->getAttributeBool("decouple_texture_size", decouple_texture_size);
	self->setDecoupleTextureSize(decouple_texture_size);

	S32 texture_width = -1;
	if (node->hasAttribute("texture_width"))
	{
		node->getAttributeS32("texture_width", texture_width);
	}
	S32 texture_height = -1;
	if (node->hasAttribute("texture_height"))
	{
		node->getAttributeS32("texture_height", texture_height);
	}
	if (texture_width > 0 && texture_height > 0)
	{
		self->setTextureSize(texture_width, texture_height);
	}
	else if (decouple_texture_size)
	{
		// Default size
		self->setTextureSize(1024, 1024);
	}

	self->initFromXML(node, parent);

	self->setHomePageUrl(start_url);

	self->setBorderVisible(border_visible);

	if (!start_url.empty())
	{
		self->navigateHome();
	}

	return self;
}

//static
bool LLMediaCtrl::parseRawCookie(const std::string& raw_cookie,
								 std::string& name, std::string& value,
								 std::string& path)
{
	std::size_t name_pos = raw_cookie.find_first_of('=');
	if (name_pos != std::string::npos)
	{
		name = raw_cookie.substr(0, name_pos);
		std::size_t value_pos = raw_cookie.find_first_of(';', name_pos);
		if (value_pos != std::string::npos)
		{
			value = raw_cookie.substr(name_pos + 1, value_pos - name_pos - 1);
			path = "/";	// assume root path for now
			return true;
		}
	}

	return false;
}

//static
void LLMediaCtrl::setOpenIdCookie(const std::string& url,
								  const std::string& cookie_host,
								  const std::string& cookie)
{
	if (url.empty() || sMediaCtrlInstances.empty())
	{
		return;
	}

	std::string cookie_name, cookie_value, cookie_path;
	if (!parseRawCookie(cookie, cookie_name, cookie_value, cookie_path))
	{
		return;
	}

	LL_DEBUGS("Media") << "Storing the OpenId cookie for media plugins."
					   << LL_ENDL;
	LLPluginClassMedia::setOpenIdCookie(url, cookie_host, cookie_path,
										cookie_name, cookie_value);

	for (instances_list_t::iterator it = sMediaCtrlInstances.begin(),
									end = sMediaCtrlInstances.end();
		 it != end; ++it)
	{
		LLMediaCtrl* ctrl = *it;
		if (!ctrl) continue;	// Paranoia

		LLPluginClassMedia* plugin = ctrl->getMediaPlugin();
		if (plugin)
		{
			plugin->setCookie(url, cookie_name, cookie_value, cookie_host,
							  cookie_path, true, true);
		}
	}
}

//-----------------------------------------------------------------------------
// LLFloaterHandler
// Command handler with support for SLURL control of floaters, such as:
// secondlife:///app/floater/self/close
//-----------------------------------------------------------------------------

class LLFloaterHandler final : public LLCommandHandler
{
public:
	LLFloaterHandler()
	:	LLCommandHandler("floater", UNTRUSTED_BLOCK)
	{
	}

	bool handle(const LLSD& params, const LLSD&, LLMediaCtrl* web) override
	{
		if (!web || params.size() < 2)
		{
			return false;
		}

		LLFloater* floater = NULL;

		// *TODO: add a floater lookup by name

		if (params[0].asString() == "self")
		{
			LLView* parent = web->getParent();
			while (parent)
			{
				floater = parent->asFloater();
				if (floater)
				{
					break;
				}
				parent = parent->getParent();
			}
		}

		if (floater && params[1].asString() == "close")
		{
			floater->close();
			return true;
		}

		return false;
	}
};

// Register with dispatch via global object
LLFloaterHandler gFloaterHandler;
