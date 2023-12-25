/**
 * @file llnotify.h
 * @brief Non-blocking notification that doesn't take keyboard focus.
 *
 * $LicenseInfo:firstyear=2003&license=viewergpl$
 *
 * Copyright (c) 2003-2009, Linden Research, Inc.
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

#ifndef LL_LLNOTIFY_H
#define LL_LLNOTIFY_H

#include <map>
#include <vector>

#include "lleventtimer.h"
#include "llinitdestroyclass.h"
#include "llnotifications.h"
#include "llpanel.h"

class LLButton;
class LLColor4U;
class LLFrameTimer;
class LLFontGL;
class LLNotifyBoxView;
class LLNotifyBoxTemplate;
class LLTextEditor;
class LLTimer;

// NotifyBox - for notifications that require a response from the user.
class LLNotifyBox final : public LLPanel, public LLEventTimer,
						  public LLInitClass<LLNotifyBox>,
						  public LLInstanceTracker<LLNotifyBox, LLUUID>
{
	friend class LLNotifyBoxView;

protected:
	LOG_CLASS(LLNotifyBox);

public:
	typedef void (*notify_callback_t)(S32 option, void* data);
	typedef std::vector<std::string> option_list_t;

	static void initClass();
	static void destroyClass();

	LL_INLINE bool isTip() const						{ return mIsTip; }
	LL_INLINE bool isCaution() const					{ return mIsCaution; }

	LL_INLINE void stopAnimation()						{ mAnimating = false; }

	void close();

	LL_INLINE LLNotificationPtr getNotification() const	{ return mNotification; }

	// Used for callbacks
	struct CallbackData
	{
		LLNotifyBox*	mSelf;
		std::string		mButtonName;
	};
	typedef std::vector<CallbackData*> cb_data_vec_t;
	LL_INLINE const cb_data_vec_t& getCallbackData()	{ return mBtnCallbackData; }

	LL_INLINE bool isDefaultBtnAdded()					{ return mAddedDefaultBtn; }

	static void format(std::string& msg,
					   const LLStringUtil::format_map_t& args);

	static void setShowNotifications(bool show);
	LL_INLINE static bool areNotificationsShown()		{ return sShowNotifications; }

	LL_INLINE static S32 getNotifyBoxCount()			{ return sNotifyBoxCount; }
	LL_INLINE static S32 getNotifyTipCount()			{ return sNotifyTipCount; }

	static void substituteSLURL(const LLUUID& id, const std::string& slurl,
								const std::string& substitute);
	static void substitutionDone(const LLUUID& id);

	// To avoid piling restart notifications, we close any old one when a new
	// one arrives, or when TPing or moving away from the restarting sim, which
	// this method allows to do (called from send_complete_agent_movement() in
	// llviewermessage.cpp).
	static void closeLastNotifyRestart();

protected:
	LLNotifyBox(LLNotificationPtr notification, bool script_dialog,
				bool is_ours);

	~LLNotifyBox() override;

	LLButton* addButton(const std::string& name, const std::string& label,
						bool is_option, bool is_default);

	bool handleMouseUp(S32 x, S32 y, MASK mask) override;
	bool handleRightMouseDown(S32 x, S32 y, MASK mask) override;

	// Animate as sliding onto the screen.
	void draw() override;
	bool tick() override;

	void moveToBack(bool getfocus = false);

	// Returns the rect, relative to gNotifyView, where this
	// notify box should be placed.
	static LLRect getNotifyRect(S32 num_options, bool layout_script_dialog,
								bool is_caution);
	static LLRect getNotifyTipRect(const std::string& message,
								   LLFontGL* fontp);

	// Internal handler for button being clicked
	static void onClickButton(void* data);

	// For "next" button
	static void onClickNext(void* data);

private:
	void drawBackground() const;
	static bool onNotification(const LLSD& notify);

protected:
	LLNotificationPtr			mNotification;

	LLTextEditor*				mUserInputBox;
	LLTextEditor*				mTextEditor;
	LLButton*					mNextBtn;

	std::string					mMessage;

	// Time since this notification was displayed.
	// This is an LLTimer not a frame timer because I am concerned
	// that I could be out-of-sync by one frame in the animation.
	LLTimer						mAnimateTimer;

	LLFrameTimer				mNotifyShowingTimer;

	S32							mNumOptions;
	S32							mNumButtons;

	LLColor4U					mBackgroundColor;

	cb_data_vec_t				mBtnCallbackData;

	bool						mIsTip;
	bool						mIsCaution;	// true for a caution notif.
	bool						mAnimating;	// Are we sliding onscreen ?
	bool						mLayoutScriptDialog;
	bool						mIsFromOurObject;
	bool						mAddedDefaultBtn;

	static S32					sNotifyBoxCount;
	static S32					sNotifyTipCount;
	static bool					sShowNotifications;

	typedef std::map<std::string, LLNotifyBox*> unique_map_t;
	static unique_map_t			sOpenUniqueNotifyBoxes;

	typedef std::multimap<LLUUID, LLUUID> name_lookup_map_t;
	static name_lookup_map_t	sNameLookupMap;

	static LLUUID				sLastNotifyRestartId;
};

class LLNotifyBoxView final : public LLUICtrl
{
public:
	LLNotifyBoxView(const std::string& name, const LLRect& rect,
					bool mouse_opaque, U32 follows = FOLLOWS_NONE);
	~LLNotifyBoxView() override;

	void showOnly(LLView* ctrl);
	LLNotifyBox* getFirstNontipBox() const;

	class Matcher
	{
	public:
		Matcher()										{}
		virtual ~Matcher()								{}
		virtual bool matches(const LLNotificationPtr) const = 0;
	};

	// Walks the list and removes any stacked messages for which the given
	// matcher returns true.
	// Useful when muting people and things in order to clear out any similar
	// previously queued messages.
	void purgeMessagesMatching(const Matcher& matcher);

private:
	bool isGroupNotifyBox(const LLView* view) const;
};

// This view contains the stack of notification windows.
extern LLNotifyBoxView* gNotifyBoxViewp;

#endif
