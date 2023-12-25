/**
 * @file lluictrl.h
 * @author James Cook, Richard Nelson, Tom Yedwab
 * @brief Abstract base class for UI controls
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

#ifndef LL_LLUICTRL_H
#define LL_LLUICTRL_H

#include "llview.h"

class LLUICtrl : public LLView
{
protected:
	LOG_CLASS(LLUICtrl);

public:
	typedef void (*LLUICtrlCallback)(LLUICtrl* ctrl, void* userdata);
	typedef bool (*LLUICtrlValidate)(LLUICtrl* ctrl, void* userdata);

	LLUICtrl();
	LLUICtrl(const std::string& name, const LLRect& rect, bool mouse_opaque,
			 LLUICtrlCallback callback, void* callback_userdata,
			 U32 reshape = FOLLOWS_NONE);
	~LLUICtrl() override;

	// LLView interface

	void initFromXML(LLXMLNodePtr node, LLView* parent) override;
	LLXMLNodePtr getXML(bool save_children = true) const override;

	LL_INLINE bool setLabelArg(const std::string& key,
							   const std::string& text) override	{ return false; }

	LL_INLINE bool isCtrl() const override							{ return true; }

	LL_INLINE void setTentative(bool b) override					{ mTentative = b; }
	LL_INLINE bool getTentative() const override					{ return mTentative; }

	bool getIsChrome() const override;

	// From LLFocusableElement

	LL_INLINE bool isUICtrl() override	 							{ return true; }
	void setFocus(bool b) override;
	bool hasFocus() const override;
	void onFocusReceived() override;
	void onFocusLost() override;

	// New virtuals

	LL_INLINE LLSD getValue() const override						{ return LLSD(); }
	LL_INLINE virtual bool setTextArg(const std::string& key,
									  const std::string& text)		{ return false; }

	LL_INLINE virtual void setIsChrome(bool b)						{ mIsChrome = b; }


	LL_INLINE virtual bool acceptsTextInput() const					{ return false; }

	// A control is dirty if the user has modified its value. Editable controls
	// should override this.
	LL_INLINE virtual bool isDirty() const							{ return false; }
	LL_INLINE virtual void resetDirty()								{}

	virtual void onCommit();
	// Called when registered as top ctrl and user clicks elsewhere:
	virtual void onLostTop();

	// Default to no-op:
	LL_INLINE virtual void onTabInto()								{}
	LL_INLINE virtual void clear()									{}

	LL_INLINE virtual void setDoubleClickCallback(void (*cb)(void*))
	{
	}

	LL_INLINE virtual void setColor(const LLColor4& color)			{}
	LL_INLINE virtual void setAlpha(F32 alpha)						{}
	LL_INLINE virtual void setMinValue(LLSD min_value)				{}
	LL_INLINE virtual void setMaxValue(LLSD max_value)				{}

	bool focusNextItem(bool text_entry_only);
	bool focusPrevItem(bool text_entry_only);
	bool focusFirstItem(bool prefer_text_fields = false,
						bool focus_flash = true);
	bool focusLastItem(bool prefer_text_fields = false);

	LL_INLINE void setTabStop(bool b)								{ mTabStop = b; }

	LL_INLINE bool hasTabStop() const								{ return mTabStop; }

	LLUICtrl* getParentUICtrl() const;

	LL_INLINE void* getCallbackUserData() const						{ return mCallbackUserData; }
	LL_INLINE void setCallbackUserData(void* data)					{ mCallbackUserData = data; }

	LL_INLINE void setCommitCallback(void (*cb)(LLUICtrl*, void*))	{ mCommitCallback = cb; }

	LL_INLINE void setValidateBeforeCommit(bool (*cb)(LLUICtrl*, void*))
	{
		mValidateCallback = cb;
	}

	LL_INLINE void setLostTopCallback(void (*cb)(LLUICtrl*, void*))	{ mLostTopCallback = cb; }

	static LLView* fromXML(LLXMLNodePtr node, LLView* parent,
						   class LLUICtrlFactory* factory);

	LLUICtrl* findRootMostFocusRoot();

	class LLTextInputFilter : public LLQueryFilter, public LLSingleton<LLTextInputFilter>
	{
		friend class LLSingleton<LLTextInputFilter>;

		filter_result_t operator()(const LLView* const view,
								   const view_list_t & children) const override
		{
			return filter_result_t(view->isCtrl() &&
								   static_cast<const LLUICtrl*>(view)->acceptsTextInput(),
								   true);
		}
	};

protected:
	void			(*mCommitCallback)(LLUICtrl* ctrl, void* userdata);
	void			(*mLostTopCallback)(LLUICtrl* ctrl, void* userdata);
	bool			(*mValidateCallback)(LLUICtrl* ctrl, void* userdata);

	void*			mCallbackUserData;

private:
	class			DefaultTabGroupFirstSorter;

	bool			mTabStop;
	bool			mIsChrome;
	bool			mTentative;
};

#endif  // LL_LLUICTRL_H
