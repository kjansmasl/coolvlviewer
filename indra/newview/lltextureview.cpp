/**
 * @file lltextureview.cpp
 * @brief LLTextureView class implementation
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

#include "llviewerprecompiledheaders.h"

#include "lltextureview.h"

#include "llconsole.h"				// For CONSOLE_PADDING_LEFT
#include "llimagedecodethread.h"
#include "llimagegl.h"
#include "llrender.h"
#include "llvertexbuffer.h"

#include "llappviewer.h"
#include "llhoverview.h"
#include "llselectmgr.h"
#include "lltexturecache.h"
#include "lltexturefetch.h"
#include "llviewercontrol.h"
#include "llviewerobject.h"
#include "llviewertexturelist.h"
#include "llviewerwindow.h"
#include "llvovolume.h"

LLTextureView* gTextureViewp = NULL;

#define HIGH_PRIORITY 100000000.f

///////////////////////////////////////////////////////////////////////////////

static const char* mem_format_str =
	"Mem (MB): GL tex: %d/%d  Bound: %d/%d  VB: %d  Free VRAM: %d/%d  Cache: %d/%d";
static const char* tex_format_str =
	"Tex(Raw): %d(%d)  Fetches: %d(%d)  HTTP: %d UDP BW: %.0f  Cache R/W: %d/%d  Decodes: %d  Bias: %.3f";
static const char* fetcher_format_str =
	"Fetch boost factor: %.1f - Upd/frame: %d - GL img created: immediate: %d / threaded: %d";
static const char* fetch1_format_str = "%s %7.0f %d(%d) 0x%08x(%8.0f)";
static const char* fetch2_format_str = "%s %7.0f %d(%d) %8.0f(0x%08x) %1.2f";
static const char* image_format_str1 = "%4dx%4d (%d) %7d";
static const char* image_format_str2 = "%4dx%4d (%d) %6d";
static const std::string title_string1a("Tex UUID  Area DDis(Req)  DecodePri(Fetch)      [download]");
static const std::string title_string1b("Tex UUID  Area DDis(Req)  Fetch(DecodePri)      [download]");
static const std::string title_string2("State");
static const std::string title_string3("Pkt Bnd");
static const std::string title_string4("  W  x  H (Dis)   Mem");
static const std::string exclam_string("!");
static const std::string star_string("*");

// State. *HACK: mirrored from lltexturefetch.cpp
static struct { const std::string desc; LLColor4* color; } fetch_state_desc[] = {
	{ "---", &LLColor4::red },		// INVALID
	{ "INI", &LLColor4::white },	// INIT
	{ "DSK", &LLColor4::cyan },		// LOAD_FROM_TEXTURE_CACHE
	{ "DSK", &LLColor4::blue },		// CACHE_POST
	{ "NET", &LLColor4::green },	// LOAD_FROM_NETWORK
	{ "SIM", &LLColor4::green },	// LOAD_FROM_SIMULATOR
	{ "HTW", &LLColor4::green },	// WAIT_HTTP_RESOURCE
	{ "HTW", &LLColor4::green },	// WAIT_HTTP_RESOURCE2
	{ "REQ", &LLColor4::yellow },	// SEND_HTTP_REQ
	{ "HTP", &LLColor4::green },	// WAIT_HTTP_REQ
	{ "DEC", &LLColor4::yellow },	// DECODE_IMAGE
	{ "DEC", &LLColor4::green },	// DECODE_IMAGE_UPDATE
	{ "WRT", &LLColor4::purple },	// WRITE_TO_CACHE
	{ "WRT", &LLColor4::orange },	// WAIT_ON_WRITE
	{ "END", &LLColor4::red },		// DONE
#define LAST_STATE 14
	{ "CRE", &LLColor4::magenta },	// LAST_STATE+1
	{ "FUL", &LLColor4::green },	// LAST_STATE+2
	{ "BAD", &LLColor4::red },		// LAST_STATE+3
	{ "MIS", &LLColor4::red },		// LAST_STATE+4
	{ "---", &LLColor4::white },	// LAST_STATE+5
};
constexpr S32 fetch_state_desc_size = (S32)LL_ARRAY_SIZE(fetch_state_desc);

constexpr S32 TEXTUREVIEW_WIDTH = 648;
constexpr S32 TEXTUREVIEW_TOP_DELTA = 50;

constexpr S32 TITLE_X1 = 0;
constexpr S32 BAR_LEFT = TITLE_X1 + 290;
constexpr S32 BAR_WIDTH = 100;
constexpr S32 BAR_HEIGHT = 8;
constexpr S32 TITLE_X2 = BAR_LEFT + BAR_WIDTH + 10;
constexpr S32 TITLE_X3 = TITLE_X2 + 40;
constexpr S32 TITLE_X4 = TITLE_X3 + 50;

///////////////////////////////////////////////////////////////////////////////

class LLTextureBar final : public LLView
{
protected:
	LOG_CLASS(LLTextureBar);

public:
	LLTextureBar(const std::string& name, const LLRect& r, LLTextureView* view)
	:	LLView(name, r, false),
		mHilite(0),
		mTextureView(view)
	{
	}

	void draw() override;

	// Returns the height of this object, given the set options.
	LL_INLINE LLRect getRequiredRect() override
	{
		LLRect rect;
		rect.mTop = BAR_HEIGHT;
		return rect;
	}

	// Used for sorting
	struct sort
	{
		LL_INLINE bool operator()(const LLView* i1, const LLView* i2)
		{
			LLViewerFetchedTexture* i1p = ((LLTextureBar*)i1)->mImagep;
			LLViewerFetchedTexture* i2p = ((LLTextureBar*)i2)->mImagep;
			F32 pri1 = i1p->getDecodePriority();
			F32 pri2 = i2p->getDecodePriority();
			if (pri1 > pri2)
			{
				return true;
			}
			if (pri2 > pri1)
			{
				return false;
			}
			return i1p->getID() < i2p->getID();
		}
	};

	struct sort_fetch
	{
		LL_INLINE bool operator()(const LLView* i1, const LLView* i2)
		{
			LLViewerFetchedTexture* i1p = ((LLTextureBar*)i1)->mImagep;
			LLViewerFetchedTexture* i2p = ((LLTextureBar*)i2)->mImagep;
			U32 pri1 = i1p->getFetchPriority();
			U32 pri2 = i2p->getFetchPriority();
			if (pri1 > pri2)
			{
				return true;
			}
			if (pri2 > pri1)
			{
				return false;
			}
			return i1p->getID() < i2p->getID();
		}
	};

private:
	LLTextureView*						mTextureView;

public:
	LLPointer<LLViewerFetchedTexture>	mImagep;
	S32									mHilite;
};

void LLTextureBar::draw()
{
	static LLFontGL* fontp = LLFontGL::getFontMonospace();
	if (!fontp)
	{
		llwarns_sparse << "No monospace font !" << llendl;
		return;
	}
	if (!mImagep || !gTextureFetchp)
	{
		return;
	}

	LLColor4 color;
	if (mHilite)
	{
		S32 idx = llclamp(mHilite, 1, 3);
		if (idx == 1)
		{
			color = LLColor4::orange;
		}
		else if (idx == 2)
		{
			color = LLColor4::yellow;
		}
		else
		{
			color = LLColor4::pink2;
		}
	}
	else if (mImagep->mDontDiscard)
	{
		color = LLColor4::green4;
	}
	else if (mImagep->getBoostLevel() > LLGLTexture::BOOST_ALM)
	{
		color = LLColor4::magenta;
	}
	else if (mImagep->getDecodePriority() <= 0.f)
	{
		color = LLColor4::grey;
		color[VALPHA] = 0.7f;
	}
	else
	{
		color = LLColor4::white;
		color[VALPHA] = 0.7f;
	}

	// We need to draw the texture UUID or name, the progress bar for the
	// texture (highlighted if it is being downloaded) and various numerical
	// stats.

	LLGLSUIDefault gls_ui;

	static char uuid_str[UUID_STR_LENGTH];
	mImagep->mID.toCString(uuid_str);
	uuid_str[7] = '\0';	// Keep only the first six digits

	std::string tex_str;
	if (mTextureView->mOrderFetch)
	{
		tex_str = llformat(fetch1_format_str, uuid_str,
						   mImagep->mMaxVirtualSize,
						   mImagep->mDesiredDiscardLevel,
						   mImagep->mRequestedDiscardLevel,
						   mImagep->mFetchPriority,
						   mImagep->getDecodePriority());
	}
	else
	{
		tex_str = llformat(fetch2_format_str, uuid_str,
						   mImagep->mMaxVirtualSize,
						   mImagep->mDesiredDiscardLevel,
						   mImagep->mRequestedDiscardLevel,
						   mImagep->getDecodePriority(),
						   mImagep->mFetchPriority,
						   mImagep->mDownloadProgress);
	}

	fontp->renderUTF8(tex_str, 0, TITLE_X1, getRect().getHeight(), color,
					  LLFontGL::LEFT, LLFontGL::TOP);

	S32 state = mImagep->mNeedsCreateTexture ? LAST_STATE + 1 :
					mImagep->mFullyLoaded ? LAST_STATE + 2 :
						mImagep->mMinDiscardLevel > 0 ? LAST_STATE + 3 :
							mImagep->mIsMissingAsset ? LAST_STATE + 4 :
								!mImagep->mIsFetching ? LAST_STATE + 5 :
									mImagep->mFetchState;
	state = llclamp(state, 0, fetch_state_desc_size - 1);

	fontp->renderUTF8(fetch_state_desc[state].desc, 0, TITLE_X2,
					  getRect().getHeight(),
					  *(fetch_state_desc[state].color),
					  LLFontGL::LEFT, LLFontGL::TOP);
	gGL.getTexUnit(0)->unbind(LLTexUnit::TT_TEXTURE);

	// Draw the progress bar.
	S32 left = BAR_LEFT;
	S32 right = left + BAR_WIDTH;
	S32 top = 0;
	S32 bottom = top + 6;
	gGL.color4f(0.f, 0.f, 0.f, 0.75f);
	gl_rect_2d(left, top, right, bottom);

	F32 data_progress = mImagep->mDownloadProgress;

	if (data_progress > 0.f && data_progress <= 1.f)
	{
		// Downloaded bytes
		right = left + llfloor(data_progress * (F32)BAR_WIDTH);
		if (right > left)
		{
			gGL.color4f(0.f, 0.f, 1.f, 0.75f);
			gl_rect_2d(left, top, right, bottom);
		}
	}
	else if (data_progress > 1.f)
	{
		// Small cached textures generate this oddity. SNOW-168
		right = left + BAR_WIDTH;
		gGL.color4f(0.f, 0.33f, 0.f, 0.75f);
		gl_rect_2d(left, top, right, bottom);
	}

	S32 pip_width = 6;
	S32 pip_space = 14;
	S32 pip_x = TITLE_X3 + pip_space / 2;

	// Draw the packet pip
	LLColor4 clr;
	constexpr F32 pip_max_time = 5.f;
	F32 last_event = gFrameTimeSeconds - mImagep->mLastPacketTime;
	if (last_event < pip_max_time)
	{
		clr = LLColor4::white;
	}
	else
	{
		last_event = mImagep->mRequestDeltaTime;
		if (last_event < pip_max_time)
		{
			clr = LLColor4::green;
		}
		else
		{
			last_event = mImagep->mFetchDeltaTime;
			if (last_event < pip_max_time)
			{
				clr = LLColor4::yellow;
			}
		}
	}
	if (last_event < pip_max_time)
	{
		clr.setAlpha(1.f - last_event / pip_max_time);
		gGL.color4fv(clr.mV);
		gl_rect_2d(pip_x, top, pip_x + pip_width, bottom);
	}
	pip_x += pip_width + pip_space;

	// We do not want to show bind/resident pips for textures using the default
	// texture
	if (mImagep->hasGLTexture())
	{
		// Draw the bound pip
		last_event = mImagep->getTimePassedSinceLastBound();
		if (last_event < 1.f)
		{
			clr = LLColor4::magenta1;
			clr.setAlpha(1.f - last_event);
			gGL.color4fv(clr.mV);
			gl_rect_2d(pip_x, top, pip_x + pip_width, bottom);
		}
	}
	pip_x += pip_width + pip_space;


	{
		LLGLSUIDefault gls_ui;
		// Draw the image size at the end
		S32 discard = mImagep->getDiscardLevel();
		std::string num_str =
			llformat(discard >= 0 ? image_format_str1 : image_format_str2,
					 mImagep->getWidth(), mImagep->getHeight(), discard,
					 mImagep->hasGLTexture() ? mImagep->getTextureMemory()
											 : 0);
		fontp->renderUTF8(num_str, 0, TITLE_X4, getRect().getHeight(), color,
						  LLFontGL::LEFT, LLFontGL::TOP);
	}
}

////////////////////////////////////////////////////////////////////////////

class LLGLTexMemBar final : public LLView
{
protected:
	LOG_CLASS(LLGLTexMemBar);

public:
	LLGLTexMemBar(const std::string& name, LLTextureView* texview)
	:	LLView(name, false),
		mTextureView(texview)
	{
		S32 line_height =
			(S32)(LLFontGL::getFontMonospace()->getLineHeight() + .5f);
		setRect(LLRect(0, 0, 100, line_height * 5));
	}

	void draw() override;

	LL_INLINE bool handleMouseDown(S32, S32, MASK) override
	{
		return false;
	}

	// Returns the height of this object, given the set options.
	LL_INLINE LLRect getRequiredRect() override
	{
		LLRect rect;
		// Room for four lines of text
		rect.mTop = (7 * BAR_HEIGHT) / 2;
		return rect;
	}

private:
	LLTextureView* mTextureView;
};

void LLGLTexMemBar::draw()
{
	static LLFontGL* fontp = LLFontGL::getFontMonospace();
	if (!fontp)
	{
		llwarns_sparse << "No monospace font !" << llendl;
		return;
	}
	static S32 line_height = S32(fontp->getLineHeight() + 0.5f);

	if (!gTextureFetchp || !gTextureCachep)
	{
		return;
	}

	S32 cache_usage = BYTES2MEGABYTES(gTextureCachep->getUsage());
	S32 cache_max_usage = BYTES2MEGABYTES(LLTextureCache::getMaxUsage());

	LLGLSUIDefault gls_ui;
	LLColor4 text_color(1.f, 1.f, 1.f, 0.75f);
	LLColor4 color;
	std::string text = llformat(mem_format_str,
								LLViewerTexture::sTotalTexMemoryMB,
								LLViewerTexture::sMaxTotalTexMemMB,
								LLViewerTexture::sBoundTexMemoryMB,
								LLViewerTexture::sMaxBoundTexMemMB,
								LLVertexBuffer::getVRAMMegabytes(),
								LLImageGLThread::getFreeVRAMMegabytes(),
								gGLManager.mTexVRAM, cache_usage,
								cache_max_usage);

	fontp->renderUTF8(text, 0, 0, line_height * 4, text_color, LLFontGL::LEFT,
					  LLFontGL::TOP);

	S32 raw_count = LLImageRaw::sRawImageCount;
	text = llformat(tex_format_str, gTextureList.getNumImages(), raw_count,
					gTextureFetchp->getApproxNumRequests(),
					gTextureFetchp->getNumDeletes(),
					gTextureFetchp->getNumHTTPRequests(),
					gTextureFetchp->getTextureBandwidth(),
					gTextureCachep->getNumReads(),
					gTextureCachep->getNumWrites(),
					gImageDecodeThreadp->getPending(),
					LLViewerTexture::sDesiredDiscardBias);

	fontp->renderUTF8(text, 0, 0, line_height * 3, text_color, LLFontGL::LEFT,
					  LLFontGL::TOP);

	text = llformat(fetcher_format_str,
					LLViewerTextureList::sFetchingBoostFactor,
					(S32)LLViewerTextureList::sNumUpdatesStat.getMean(),
					LLViewerFetchedTexture::sMainThreadCreations,
					LLViewerFetchedTexture::sImageThreadCreations);
	if (LLViewerFetchedTexture::sImageThreadCreationsCapped)
	{
		text += " (queue full)";
	}
	else
	{
		text += llformat(" (%d)",
						 LLViewerFetchedTexture::sImageThreadQueueSize);
	}

	fontp->renderUTF8(text, 0, 0, line_height * 2, text_color, LLFontGL::LEFT,
					  LLFontGL::TOP);

	S32 dx1 = 0;
	if (gTextureFetchp->mDebugPause)
	{
		fontp->renderUTF8(exclam_string, 0, TITLE_X1, line_height, text_color,
						  LLFontGL::LEFT, LLFontGL::TOP);
		dx1 += 8;
	}
	if (mTextureView->mFreezeView)
	{
		fontp->renderUTF8(star_string, 0, TITLE_X1, line_height, text_color,
						  LLFontGL::LEFT, LLFontGL::TOP);
		dx1 += 8;
	}
	if (mTextureView->mOrderFetch)
	{
		fontp->renderUTF8(title_string1b, 0, TITLE_X1 + dx1, line_height,
						  text_color, LLFontGL::LEFT, LLFontGL::TOP);
	}
	else
	{
		fontp->renderUTF8(title_string1a, 0, TITLE_X1 + dx1, line_height,
						  text_color, LLFontGL::LEFT, LLFontGL::TOP);
	}

	fontp->renderUTF8(title_string2, 0, TITLE_X2, line_height, text_color,
					  LLFontGL::LEFT, LLFontGL::TOP);

	fontp->renderUTF8(title_string3, 0, TITLE_X3, line_height, text_color,
					  LLFontGL::LEFT, LLFontGL::TOP);

	fontp->renderUTF8(title_string4, 0, TITLE_X4, line_height, text_color,
					  LLFontGL::LEFT, LLFontGL::TOP);
}

////////////////////////////////////////////////////////////////////////////

LLTextureView::LLTextureView(const std::string& name)
:	LLContainerView(name, LLRect()),
	mFreezeView(false),
	mOrderFetch(false),
	mPrintList(false),
	mNumTextureBars(0),
	mGLTexMemBar(NULL)
{
	llassert(gTextureViewp == NULL);
	gTextureViewp = this;

	setVisible(false);
	setFollowsTop();
	setFollowsLeft();

	// NOTE: we must ensure the initial rect got a valid (non-zero) size, else
	// draw() is never called, and the rect is never resized (and stays
	// invisible)... HB
	S32 cur_height = gViewerWindowp->getVirtualWindowRect().getHeight();
	LLRect rect;
	rect.setLeftTopAndSize(CONSOLE_PADDING_LEFT,
						   cur_height - TEXTUREVIEW_TOP_DELTA,
						   TEXTUREVIEW_WIDTH, cur_height / 2);
	setRect(rect);
	reshape(rect.getWidth(), rect.getHeight(), false);
}

LLTextureView::~LLTextureView()
{
	for_each(mTextureBars.begin(), mTextureBars.end(), DeletePointer());
	mTextureBars.clear();
	// Children all cleaned up by default view destructor.
	if (mGLTexMemBar)
	{
		delete mGLTexMemBar;
		mGLTexMemBar = NULL;
	}
	gTextureViewp = NULL;
}

typedef std::pair<F32, LLViewerFetchedTexture*> decode_pair_t;
struct compare_decode_pair
{
	LL_INLINE bool operator()(const decode_pair_t& a,
							  const decode_pair_t& b) const
	{
		return a.first > b.first;
	}
};

//virtual
void LLTextureView::draw()
{
	static S32 window_height = -1;
	S32 cur_height = gViewerWindowp->getVirtualWindowRect().getHeight();
	if (cur_height != window_height)
	{
		window_height = cur_height;
		LLRect rect;
		rect.setLeftTopAndSize(CONSOLE_PADDING_LEFT,
							   cur_height - TEXTUREVIEW_TOP_DELTA,
							   TEXTUREVIEW_WIDTH, cur_height / 2);
		setRect(rect);
		reshape(rect.getWidth(), rect.getHeight(), false);
	}

	if (!mFreezeView)
	{
		for_each(mTextureBars.begin(), mTextureBars.end(), DeletePointer());
		mTextureBars.clear();

		if (mGLTexMemBar)
		{
			delete mGLTexMemBar;
			mGLTexMemBar = NULL;
		}

		typedef std::multiset<decode_pair_t,
							  compare_decode_pair> display_list_t;
		display_list_t display_image_list;

		if (mPrintList)
		{
			llinfos << "ID\tMEM\tBOOST\tPRI\tWIDTH\tHEIGHT\tDISCARD" << llendl;
		}

		for (LLViewerTextureList::priority_list_t::iterator
				iter = gTextureList.mImageList.begin(),
				end = gTextureList.mImageList.end();
			 iter != end; ++iter)
		{
			LLPointer<LLViewerFetchedTexture> imagep = *iter;
			if (!imagep->hasFetcher())
			{
				continue;
			}

			S32 cur_discard = imagep->getDiscardLevel();
			S32 desired_discard = imagep->mDesiredDiscardLevel;

			if (mPrintList)
			{
				S32 tex_mem =
					imagep->hasGLTexture() ? imagep->getTextureMemory() : 0;
				llinfos << imagep->getID() << "\t" << tex_mem
						<< "\t" << imagep->getBoostLevel()
						<< "\t" << imagep->getDecodePriority()
						<< "\t" << imagep->getWidth()
						<< "\t" << imagep->getHeight()
						<< "\t" << cur_discard << llendl;
			}

#if 0
			if (imagep->getDontDiscard() || imagep->isMissingAsset())
			{
				continue;
			}
#endif

			F32 pri;
			if (mOrderFetch)
			{
				pri = (F32)imagep->mFetchPriority / 256.f;
			}
			else
			{
				pri = imagep->getDecodePriority();
			}
			pri = llclamp(pri, 0.f, HIGH_PRIORITY - 1.f);

			if (!mOrderFetch)
			{
				if (pri < HIGH_PRIORITY)
				{
					struct f final : public LLSelectedTEFunctor
					{
						LLViewerFetchedTexture* mImage;

						f(LLViewerFetchedTexture* image) : mImage(image)
						{
						}

						bool apply(LLViewerObject* object, S32 te) override
						{
							return (mImage == object->getTEImage(te));
						}
					} func(imagep);
					bool match = gSelectMgr.getSelection()->applyToTEs(&func,
																	   true);
					if (match)
					{
						pri += 3 * HIGH_PRIORITY;
					}
				}

				if (gHoverViewp && pri < HIGH_PRIORITY &&
					(cur_discard< 0 || desired_discard < cur_discard))
				{
					LLViewerObject* objectp =
						gHoverViewp->getLastHoverObject();
					if (objectp)
					{
						S32 tex_count = objectp->getNumTEs();
						for (S32 i = 0; i < tex_count; ++i)
						{
							if (imagep == objectp->getTEImage(i))
							{
								pri += 2 * HIGH_PRIORITY;
								break;
							}
						}
					}
				}

				if (pri > 0.f && pri < HIGH_PRIORITY)
				{
					if (gFrameTimeSeconds - imagep->mLastPacketTime < 1.f ||
						imagep->mFetchDeltaTime < 0.25f)
					{
						pri += HIGH_PRIORITY;
					}
				}
			}

	 		if (pri > 0.f)
			{
				display_image_list.emplace(pri, imagep);
			}
		}
		mPrintList = false;

		static S32 max_count = 50;
		S32 count = 0;
		for (display_list_t::iterator iter2 = display_image_list.begin(),
									  end2 = display_image_list.end();
			 iter2 != end2; ++iter2)
		{
			LLViewerFetchedTexture* imagep = iter2->second;
			S32 hilite = 0;
			F32 pri = iter2->first;
			if (pri >= 1 * HIGH_PRIORITY)
			{
				hilite = (S32)((pri + 1) / HIGH_PRIORITY) - 1;
			}
			if ((hilite || count < max_count - 10) && count < max_count)
			{
				if (addBar(imagep, hilite))
				{
					++count;
				}
			}
		}

		if (mOrderFetch)
		{
			sortChildren(LLTextureBar::sort_fetch());
		}
		else
		{
			sortChildren(LLTextureBar::sort());
		}

		mGLTexMemBar = new LLGLTexMemBar("gl texmem bar", this);
		addChild(mGLTexMemBar);

		reshape(getRect().getWidth(), getRect().getHeight(), true);
		LLUI::popMatrix();
		LLUI::pushMatrix();
		LLUI::translate((F32)getRect().mLeft, (F32)getRect().mBottom);

		for (child_list_const_iter_t child_iter = getChildList()->begin(),
									 child_end = getChildList()->end();
			 child_iter != child_end; ++child_iter)
		{
			LLView* viewp = *child_iter;
			if (viewp && viewp->getRect().mBottom < 0)
			{
				viewp->setVisible(false);
			}
		}
	}

	LLContainerView::draw();
}

bool LLTextureView::addBar(LLViewerFetchedTexture* imagep, S32 hilite)
{
	llassert(imagep);

	LLRect r;
	LLTextureBar* barp = new LLTextureBar("texture bar", r, this);
	if (barp)
	{
		barp->mImagep = imagep;
		barp->mHilite = hilite;
		addChild(barp);

		++mNumTextureBars;
		mTextureBars.push_back(barp);
		return true;
	}

	llwarns << "Could not create a new bar !  Out of memory ?" << llendl;
	return false;
}

bool LLTextureView::handleMouseDown(S32 x, S32 y, MASK mask)
{
	if ((mask & (MASK_CONTROL | MASK_SHIFT | MASK_ALT)) ==
			(MASK_ALT | MASK_SHIFT))
	{
		mPrintList = true;
		return true;
	}
	if ((mask & (MASK_CONTROL | MASK_SHIFT | MASK_ALT)) ==
			(MASK_CONTROL | MASK_SHIFT))
	{
		if (gTextureFetchp)
		{
			gTextureFetchp->mDebugPause = !gTextureFetchp->mDebugPause;
		}
		return true;
	}
	if (mask & MASK_SHIFT)
	{
		mFreezeView = !mFreezeView;
		return true;
	}
	if (mask & MASK_CONTROL)
	{
		mOrderFetch = !mOrderFetch;
		return true;
	}
	return LLView::handleMouseDown(x, y, mask);
}

bool LLTextureView::handleMouseUp(S32, S32, MASK)
{
	return false;
}

bool LLTextureView::handleKey(KEY, MASK, bool)
{
	return false;
}
