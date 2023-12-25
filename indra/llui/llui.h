/**
 * @file llui.h
 * @brief General static UI services definitions.
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

#ifndef LL_LLUI_H
#define LL_LLUI_H

#include <stack>

#include "llcontrol.h"
#include "llcoord.h"
#include "llgltexture.h"
#include "llpointer.h"
#include "llrefcount.h"
#include "llrenderutils.h"
#include "llsd.h"
#include "llcolor4.h"

class LLImageProviderInterface;
class LLColor4;
class LLHtmlHelp;
class LLUIImage;
class LLVector2;
class LLVector3;
class LLView;
class LLWindow;

typedef LLPointer<LLUIImage> LLUIImagePtr;

// Reasons for drags to be denied. Ordered by priority for multi-drag.
enum EAcceptance
{
	ACCEPT_POSTPONED,		// We are asynchronously determining acceptance
	ACCEPT_NO,				// Uninformative, general purpose denial.
	ACCEPT_NO_LOCKED,		// Operation would be valid, but perms forbid it.
	ACCEPT_YES_COPY_SINGLE,	// We will take a copy of a single item
	ACCEPT_YES_SINGLE,		// It is OK to drag and drop single item here.
	ACCEPT_YES_COPY_MULTI,	// Wewill take a copy of multiple items
	ACCEPT_YES_MULTI		// It is OK to drag and drop multiple items here.
};

enum EAddPosition
{
	ADD_TOP,
	ADD_SORTED,
	ADD_BOTTOM
};

// Used to hide the flashing text cursor when window doesn't have focus.
extern bool gShowTextEditCursor;

void make_ui_sound(const char* name, bool force = false);

// UI-specific rendering functions that cannot go into llrenderutils.h since
// their implementation needs llui stuff

void gl_rect_2d_offset_local(S32 left, S32 top, S32 right, S32 bottom,
							 S32 pixel_offset = 0, bool filled = true);

LL_INLINE void gl_rect_2d_offset_local(const LLRect& rect,
									   S32 pixel_offset = 0,
									   bool filled = true)
{
	gl_rect_2d_offset_local(rect.mLeft, rect.mTop, rect.mRight, rect.mBottom,
							pixel_offset, filled);
}

LL_INLINE void gl_rect_2d_offset_local(S32 left, S32 top,
									   S32 right, S32 bottom,
									   const LLColor4& color,
									   S32 pixel_offset = 0,
									   bool filled = true)
{
	gGL.color4fv(color.mV);
	gl_rect_2d_offset_local(left, top, right, bottom, pixel_offset, filled);
}

void gl_line_3d(const LLVector3& start, const LLVector3& end,
				const LLColor4& color, F32 phase = 0.f);

void gl_rect_2d_checkerboard(const LLRect& rect);

//
// Classes
//

typedef	void (*LLUIAudioCallback)(const LLUUID& uuid);

// Purely static class (singleton, for no valid reason, in LL's code).
class LLUI
{
	LLUI() = delete;
	~LLUI() = delete;

protected:
	LOG_CLASS(LLUI);

public:
	static void initClass(LLControlGroup* config,
						  LLControlGroup* ignores,
						  LLControlGroup* colors,
						  LLImageProviderInterface* image_provider,
						  LLUIAudioCallback audio_callback = NULL,
						  const LLVector2* scale_factor = NULL,
						  const std::string& language = LLStringUtil::null);
	static void cleanupClass();

	static void pushMatrix();
	static void popMatrix();
	static void loadIdentity();
	static void translate(F32 x, F32 y, F32 z = 0.0f);

	// Return the ISO639 language name ("en", "ko", etc.) for the viewer UI.
	// http://www.loc.gov/standards/iso639-2/php/code_list.php
	static std::string getLanguage();

	// Helper functions (should probably move free standing rendering helper
	// functions here)
	static std::string locateSkin(const std::string& filename);
	static void setCursorPositionScreen(S32 x, S32 y);
	static void setCursorPositionLocal(const LLView* viewp, S32 x, S32 y);
	static void getCursorPositionLocal(const LLView* viewp, S32* x, S32* y);
	static void setLineWidth(F32 width);
	static LLUIImagePtr getUIImageByID(const LLUUID& image_id);
	static LLUIImagePtr getUIImage(const std::string& name);
	static LLVector2 getWindowSize();
	static void screenPointToGL(S32 screen_x, S32 screen_y, S32* gl_x, S32* gl_y);
	static void glPointToScreen(S32 gl_x, S32 gl_y, S32* screen_x, S32* screen_y);
	static void screenRectToGL(const LLRect& screen, LLRect* gl);
	static void glRectToScreen(const LLRect& gl, LLRect* screen);
	static void setHtmlHelp(LLHtmlHelp* html_help);

private:
	static void connectRefreshSettingsSafe(const char* name);
	static void refreshSettings();

public:
	static LLControlGroup*				sConfigGroup;
	static LLControlGroup*				sIgnoresGroup;
	static LLControlGroup*				sColorsGroup;
	static LLImageProviderInterface*	sImageProvider;
	static LLUIAudioCallback			sAudioCallback;
	static LLVector2					sGLScaleFactor;
	static LLHtmlHelp*					sHtmlHelp;

	static LLColor4						sAlertBoxColor;
	static LLColor4						sAlertCautionBoxColor;
	static LLColor4						sAlertCautionTextColor;
	static LLColor4						sAlertTextColor;
	static LLColor4						sButtonFlashBgColor;
	static LLColor4						sButtonImageColor;
	static LLColor4						sButtonLabelColor;
	static LLColor4						sButtonLabelDisabledColor;
	static LLColor4						sButtonLabelSelectedColor;
	static LLColor4						sButtonLabelSelectedDisabledColor;
	static LLColor4						sColorDropShadow;
	static LLColor4						sDefaultBackgroundColor;
	static LLColor4						sDefaultHighlightDark;
	static LLColor4						sDefaultHighlightLight;
	static LLColor4						sDefaultShadowDark;
	static LLColor4						sDefaultShadowLight;
	static LLColor4						sFloaterButtonImageColor;
	static LLColor4						sFloaterFocusBorderColor;
	static LLColor4						sFloaterUnfocusBorderColor;
	static LLColor4						sFocusBackgroundColor;
	static LLColor4						sHTMLLinkColor;
	static LLColor4						sLabelDisabledColor;
	static LLColor4						sLabelSelectedColor;
	static LLColor4						sLabelTextColor;
	static LLColor4						sLoginProgressBarBgColor;
	static LLColor4						sMenuDefaultBgColor;
	static LLColor4						sMultiSliderThumbCenterColor;
	static LLColor4						sMultiSliderThumbCenterSelectedColor;
	static LLColor4						sMultiSliderTrackColor;
	static LLColor4						sMultiSliderTriangleColor;
	static LLColor4						sPieMenuBgColor;
	static LLColor4						sPieMenuLineColor;
	static LLColor4						sPieMenuSelectedColor;
	static LLColor4						sScrollbarThumbColor;
	static LLColor4						sScrollbarTrackColor;
	static LLColor4						sScrollBgReadOnlyColor;
	static LLColor4						sScrollBGStripeColor;
	static LLColor4						sScrollBgWriteableColor;
	static LLColor4						sScrollDisabledColor;
	static LLColor4						sScrollHighlightedColor;
	static LLColor4						sScrollSelectedBGColor;
	static LLColor4						sScrollSelectedFGColor;
	static LLColor4						sScrollUnselectedColor;
	static LLColor4						sSliderThumbCenterColor;
	static LLColor4						sSliderThumbOutlineColor;
	static LLColor4						sSliderTrackColor;
	static LLColor4						sTextBgFocusColor;
	static LLColor4						sTextBgReadOnlyColor;
	static LLColor4						sTextBgWriteableColor;
	static LLColor4						sTextCursorColor;
	static LLColor4						sTextDefaultColor;
	static LLColor4						sTextEmbeddedItemColor;
	static LLColor4						sTextEmbeddedItemReadOnlyColor;
	static LLColor4						sTextFgColor;
	static LLColor4						sTextFgReadOnlyColor;
	static LLColor4						sTextFgTentativeColor;
	static LLColor4						sTitleBarFocusColor;
	static LLColor4						sTrackColor;
	static LLColor4						sDisabledTrackColor;

	static S32							sButtonFlashCount;
	static F32							sButtonFlashRate;
	static F32							sColumnHeaderDropDownDelay;
	static S32							sDropShadowButton;
	static S32							sDropShadowFloater;
	static S32							sDropShadowTooltip;
	static F32							sMenuAccessKeyTime;
	static F32							sPieMenuLineWidth;
	static S32							sSnapMargin;
	static F32							sTypeAheadTimeout;

	static bool							sShowXUINames;
	static bool							sConsoleBoxPerMessage;
	static bool							sDisableMessagesSpacing;
	static bool							sTabToTextFieldsOnly;
	static bool							sUseAltKeyForMenus;
};

// FactoryPolicy is a static class that controls the creation and lookup of UI
// elements such as floaters. The key parameter is used to provide a unique
// identifier and/or associated construction parameters for a given UI
// instance.
//
// Specialize this traits for different types, or provide a class with an
// identical interface in the place of the traits parameter.
//
//	For example:
//
//	template <>
//	class FactoryPolicy<MyClass> /* FactoryPolicy specialized for MyClass */
//	{
//	public:
//		static MyClass* findInstance(const LLSD& key = LLSD())
//		{
//			// return instance of MyClass associated with key
//		}
//
//		static MyClass* createInstance(const LLSD& key = LLSD())
//		{
//			// create new instance of MyClass using key for
//			// construction parameters
//		}
//	}
//
//	class MyClass : public LLUIFactory<MyClass>
//	{
//		// uses FactoryPolicy<MyClass> by default
//	}

template <class T>
class FactoryPolicy
{
public:
	// Basic factory methods, unimplemented, provide specialisation
	static T* findInstance(const LLSD& key);
	static T* createInstance(const LLSD& key);
};

// VisibilityPolicy controls the visibility of UI elements, such as floaters.
// The key parameter is used to store the unique identifier of a given UI instance
//
// Specialize this traits for different types, or duplicate this interface for
// specific instances (see above).

template <class T>
class VisibilityPolicy
{
public:
	// visibility methods, unimplemented, provide specialisation
	static bool visible(T* instance, const LLSD& key);
	static void show(T* instance, const LLSD& key);
	static void hide(T* instance, const LLSD& key);
};

// Manages generation of UI elements by LLSD, such that (generally) there is a
// unique instance per distinct LLSD parameter. Class T is the instance type
// being managed, and the FACTORY_POLICY and VISIBILITY_POLICY. Classes provide
// static methods for creating, accessing, showing and hiding the associated
// element T.
template <class T,
		  class FACTORY_POLICY = FactoryPolicy<T>,
		  class VISIBILITY_POLICY = VisibilityPolicy<T> >
class LLUIFactory
{
public:
	// Give names to the template parameters so derived classes can refer to
	// them except this does not work in gcc
	typedef FACTORY_POLICY factory_policy_t;
	typedef VISIBILITY_POLICY visibility_policy_t;

	LLUIFactory() = default;

 	virtual ~LLUIFactory() = default;

	// Default show and hide methods
	static T* showInstance(const LLSD& key = LLSD())
	{
		T* instance = getInstance(key);
		if (instance != NULL)
		{
			VISIBILITY_POLICY::show(instance, key);
		}
		return instance;
	}

	static void hideInstance(const LLSD& key = LLSD())
	{
		T* instance = FACTORY_POLICY::findInstance(key);
		if (instance != NULL)
		{
			VISIBILITY_POLICY::hide(instance, key);
		}
	}

	static void toggleInstance(const LLSD& key = LLSD())
	{
		if (instanceVisible(key))
		{
			hideInstance(key);
		}
		else
		{
			showInstance(key);
		}
	}

	static bool instanceVisible(const LLSD& key = LLSD())
	{
		T* instance = FACTORY_POLICY::findInstance(key);
		return instance != NULL && VISIBILITY_POLICY::visible(instance, key);
	}

	static T* getInstance(const LLSD& key = LLSD())
	{
		T* instance = FACTORY_POLICY::findInstance(key);
		if (instance == NULL)
		{
			instance = FACTORY_POLICY::createInstance(key);
		}
		return instance;
	}
};

// Creates a UI singleton by ignoring the identifying parameter and always
// generating the same instance via the LLUIFactory interface. Note that since
// UI elements can be destroyed by their hierarchy, this singleton pattern uses
// a static pointer to an instance that will be re-created as needed.
//
//	Usage Pattern:
//
//	class LLFloaterFoo : public LLFloater, public LLUISingleton<LLFloaterFoo>
//	{
//		friend class LLUISingleton<LLFloaterFoo>;
//		private:
//			LLFloaterFoo(const LLSD& key);
//	};
//
// Note that LLUISingleton takes an option VisibilityPolicy parameter that
// defines how showInstance(), hideInstance(), etc. work.

template <class T, class VISIBILITY_POLICY = VisibilityPolicy<T> >
class LLUISingleton : public LLUIFactory<T,
										 LLUISingleton<T, VISIBILITY_POLICY>,
										 VISIBILITY_POLICY>
{
protected:
	// T must derive from LLUISingleton<T>
	LL_INLINE LLUISingleton()						{ sInstance = static_cast<T*>(this); }

	LL_INLINE ~LLUISingleton()						{ sInstance = NULL; }

public:
	static T* findInstance(const LLSD& key = LLSD())
	{
		return sInstance;
	}

	static T* createInstance(const LLSD& key = LLSD())
	{
		if (sInstance == NULL)
		{
			sInstance = new T(key);
		}
		return sInstance;
	}

private:
	static T*	sInstance;
};

template <class T, class U> T* LLUISingleton<T, U>::sInstance = NULL;

class LLScreenClipRect
{
public:
	LLScreenClipRect(const LLRect& rect, bool enabled = true);
	virtual ~LLScreenClipRect();

private:
	static void pushClipRect(const LLRect& rect);
	static void popClipRect();
	static void updateScissorRegion();

private:
	LLGLState		mScissorState;
	bool			mEnabled;

	static std::stack<LLRect> sClipRectStack;
};

class LLLocalClipRect : public LLScreenClipRect
{
public:
	LLLocalClipRect(const LLRect& rect, bool enabled = true);
};

class LLUIImage : public LLRefCount
{
protected:
	LOG_CLASS(LLUIImage);

public:
	LLUIImage(const std::string& name, LLPointer<LLGLTexture> image);

	void setClipRegion(const LLRectf& region);
	void setScaleRegion(const LLRectf& region);

	LLPointer<LLGLTexture> getImage()				{ return mImage; }
	const LLPointer<LLGLTexture>& getImage() const	{ return mImage; }

	void draw(S32 x, S32 y, S32 width, S32 height,
			  const LLColor4& color = UI_VERTEX_COLOR) const;

	void draw(S32 x, S32 y, const LLColor4& color = UI_VERTEX_COLOR) const;

	void draw(const LLRect& rect,
			  const LLColor4& color = UI_VERTEX_COLOR) const
	{
		draw(rect.mLeft, rect.mBottom, rect.getWidth(), rect.getHeight(),
			 color);
	}

	void drawSolid(S32 x, S32 y, S32 width, S32 height,
				   const LLColor4& color) const;

	void drawSolid(const LLRect& rect, const LLColor4& color) const
	{
		drawSolid(rect.mLeft, rect.mBottom, rect.getWidth(), rect.getHeight(),
				  color);
	}

	void drawSolid(S32 x, S32 y, const LLColor4& color) const
	{
		drawSolid(x, y, mImage->getWidth(0), mImage->getHeight(0), color);
	}

	void drawBorder(S32 x, S32 y, S32 width, S32 height, const LLColor4& color,
					S32 border_width) const;

	void drawBorder(const LLRect& rect, const LLColor4& color,
					S32 border_width) const
	{
		drawBorder(rect.mLeft, rect.mBottom, rect.getWidth(), rect.getHeight(),
				   color, border_width);
	}

	void drawBorder(S32 x, S32 y, const LLColor4& color,
					S32 border_width) const
	{
		drawBorder(x, y, mImage->getWidth(0), mImage->getHeight(0), color,
				   border_width);
	}

	const std::string& getName() const				{ return mName; }

	S32 getWidth() const;
	S32 getHeight() const;

	// Returns dimensions of underlying textures, which might not be equal to
	// UI image portion
	S32 getTextureWidth() const;
	S32 getTextureHeight() const;

	// Used to load static UI image pointers. Must be called once the texture
	// fetcher has been fully initialized. Called from llviewertexturelist.cpp.
	static void initClass();
	// Used to cleanup static UI image pointers on viewer shutdown. Called from
	// llviewertexturelist.cpp.
	static void cleanupClass();

public:
	static LLUIImagePtr		sRoundedSquare;
	static S32				sRoundedSquareWidth;
	static S32				sRoundedSquareHeight;

protected:
	std::string				mName;
	LLRectf					mScaleRegion;
	LLRectf					mClipRegion;
	LLPointer<LLGLTexture>	mImage;
	bool					mUniformScaling;
	bool					mNoClip;
};

// RN: maybe this needs to moved elsewhere ?
class LLImageProviderInterface
{
public:
	LLImageProviderInterface() = default;
	virtual ~LLImageProviderInterface() = default;

	virtual LLUIImagePtr getUIImage(const std::string& name) = 0;
	virtual LLUIImagePtr getUIImageByID(const LLUUID& id) = 0;
	virtual void cleanUp() = 0;
};

template <typename DERIVED>
class LLParamBlock
{
protected:
	LLParamBlock()									{ sBlock = (DERIVED*)this; }

	typedef typename boost::add_const<DERIVED>::type Tconst;

	template <typename T>
	class LLMandatoryParam
	{
	public:
		typedef typename boost::add_const<T>::type T_const;

		LLMandatoryParam(T_const initial_val)
		:	mVal(initial_val),
			mBlock(sBlock)
		{
		}

		LLMandatoryParam(const LLMandatoryParam<T>& other)
		:	mVal(other.mVal)
		{
		}

		DERIVED& operator()(T_const set_value)		{ mVal = set_value; return *mBlock; }
		operator T() const							{ return mVal; }
		T operator=(T_const set_value)				{ mVal = set_value; return mVal; }

	private:
		T			mVal;
		DERIVED*	mBlock;
	};

	template <typename T>
	class LLOptionalParam
	{
	public:
		typedef typename boost::add_const<T>::type T_const;

		LLOptionalParam(T_const initial_val)
		:	mVal(initial_val),
			mBlock(sBlock)
		{
		}

		LLOptionalParam()
		:	mBlock(sBlock)
		{
		}

		LLOptionalParam(const LLOptionalParam<T>& other)
		:	mVal(other.mVal)
		{
		}

		DERIVED& operator()(T_const set_value)		{ mVal = set_value; return *mBlock; }
		operator T() const							{ return mVal; }
		T operator=(T_const set_value)				{ mVal = set_value; return mVal; }

	private:
		T			mVal;
		DERIVED*	mBlock;
	};

	// specialization that requires initialization for reference types
	template <typename T>
	class LLOptionalParam <T&>
	{
	public:
		typedef typename boost::add_const<T&>::type T_const;

		LLOptionalParam(T_const initial_val)
		:	mVal(initial_val), mBlock(sBlock)
		{
		}

		LLOptionalParam(const LLOptionalParam<T&>& other)
		:	mVal(other.mVal)
		{
		}

		DERIVED& operator ()(T_const set_value)		{ mVal = set_value; return *mBlock; }
		operator T&() const							{ return mVal; }
		T& operator=(T_const set_value)				{ mVal = set_value; return mVal; }

	private:
		T&	mVal;
		DERIVED* mBlock;
	};

	// specialization that initializes pointer params to NULL
	template<typename T>
	class LLOptionalParam<T*>
	{
	public:
		typedef typename boost::add_const<T*>::type T_const;

		LLOptionalParam(T_const initial_val)
		:	mVal(initial_val),
			mBlock(sBlock)
		{
		}

		LLOptionalParam()
		:	mVal(NULL),
			mBlock(sBlock)
		{
		}

		LLOptionalParam(const LLOptionalParam<T*>& other)
		:	mVal(other.mVal)
		{
		}

		DERIVED& operator ()(T_const set_value)		{ mVal = set_value; return *mBlock; }
		operator T*() const							{ return mVal; }
		T* operator=(T_const set_value)				{ mVal = set_value; return mVal; }

	private:
		T*			mVal;
		DERIVED*	mBlock;
	};

	static DERIVED* sBlock;
};

template <typename T> T* LLParamBlock<T>::sBlock = NULL;

extern LLGLSLShader gSolidColorProgram;
extern LLGLSLShader gUIProgram;

// UI constants. Formerly in the now removed lluiconstants.h
constexpr S32 VPAD = 4;					// Vertical padding
constexpr S32 HPAD = 4;					// Horizontal padding
// Spacing for small font lines of text, like LLTextBoxes
constexpr S32 LINE = 16;

#endif
