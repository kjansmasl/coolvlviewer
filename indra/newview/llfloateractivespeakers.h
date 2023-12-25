/**
 * @file llfloateractivespeakers.h
 * @brief Management interface for muting and controlling volume of residents
 * currently speaking
 *
 * $LicenseInfo:firstyear=2005&license=viewergpl$
 *
 * Copyright (c) 2005-2009, Linden Research, Inc.
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

#ifndef LL_LLFLOATERACTIVESPEAKERS_H
#define LL_LLFLOATERACTIVESPEAKERS_H

#include "llavatarnamecache.h"
#include "llevent.h"
#include "llfloater.h"
#include "llframetimer.h"
#include "llpointer.h"
#include "llrefcount.h"
#include "llsingleton.h"

class LLButton;
class LLPanelActiveSpeakers;
class LLScrollListCtrl;
class LLSlider;
class LLSpeakerMgr;
class LLVoiceChannel;

// Data for a given participant in a voice channel
class LLSpeaker : public LLRefCount,
				  public LLOldEvents::LLObservable,
				  public LLHandleProvider<LLSpeaker>
{
public:
	typedef enum e_speaker_type
	{
		SPEAKER_AGENT,
		SPEAKER_OBJECT,
		// Speaker that does not map to an avatar or object (i.e. PSTN caller
		// in a group)
		SPEAKER_EXTERNAL
	} ESpeakerType;

	typedef enum e_speaker_status
	{
		STATUS_SPEAKING,
		STATUS_HAS_SPOKEN,
		STATUS_VOICE_ACTIVE,
		STATUS_TEXT_ONLY,
		STATUS_NOT_IN_CHANNEL,
		STATUS_MUTED
	} ESpeakerStatus;

	LLSpeaker(const LLUUID& id, const std::string& name = LLStringUtil::null,
			  ESpeakerType type = SPEAKER_AGENT,
			  ESpeakerStatus status = LLSpeaker::STATUS_TEXT_ONLY);

	LL_INLINE void setStatus(ESpeakerStatus status)
	{
		if (mStatus != status)
		{
			mStatus = status;
			mNeedsResort = true;
		}
	}

	LL_INLINE void setDisplayName(const std::string& name)
	{
		if (mDisplayName != name)
		{
			mDisplayName = name;
			mNeedsResort = true;
		}
	}

	LL_INLINE void setSpokenTime(F32 time)
	{
		if (mLastSpokeTime != time)
		{
			mLastSpokeTime = time;
			mHasSpoken = mNeedsResort = true;
		}
	}

	void lookupName();

	static void onAvatarNameLookup(const LLUUID& id,
								   const LLAvatarName& avatar_name,
								   void* user_data);

public:
	// Current activity status in speech group
	ESpeakerStatus	mStatus;

	// Timestamp when this speaker last spoke
	F32				mLastSpokeTime;

	// Current speech amplitude (timea average RMS amplitude ?);
	F32				mSpeechVolume;

	// Cache legacy name for this speaker
	std::string		mLegacyName;

	// Cache display name for this speaker
	std::string		mDisplayName;

	// Time out speakers when they are not part of current voice channel
	LLFrameTimer	mActivityTimer;

	LLColor4		mDotColor;
	LLUUID			mID;
	LLUUID			mOwnerID;
	S32				mSortIndex;
	ESpeakerType	mType;

	// Has this speaker said anything this session ?
	bool			mHasSpoken;
	bool			mTyping;
	bool			mIsModerator;
	bool			mModeratorMutedVoice;
	bool			mModeratorMutedText;
	bool			mNeedsResort;
};

class LLSpeakerTextModerationEvent : public LLOldEvents::LLEvent
{
public:
	LLSpeakerTextModerationEvent(LLSpeaker* source);
	LLSD getValue() override;
};

class LLSpeakerVoiceModerationEvent : public LLOldEvents::LLEvent
{
public:
	LLSpeakerVoiceModerationEvent(LLSpeaker* source);
	LLSD getValue() override;
};

class LLSpeakerListChangeEvent : public LLOldEvents::LLEvent
{
public:
	LLSpeakerListChangeEvent(LLSpeakerMgr* source, const LLUUID& speaker_id);
	LLSD getValue() override;

private:
	const LLUUID& mSpeakerID;
};

class LLSpeakerMgr : public LLOldEvents::LLObservable
{
public:
	LLSpeakerMgr(LLVoiceChannel* channelp);

	const LLPointer<LLSpeaker> findSpeaker(const LLUUID& avatar_id);
	void update(bool resort_ok);
	void setSpeakerTyping(const LLUUID& speaker_id, bool typing);
	void speakerChatted(const LLUUID& speaker_id);
	LLPointer<LLSpeaker> setSpeaker(const LLUUID& id,
									const std::string& name = LLStringUtil::null,
									LLSpeaker::ESpeakerStatus status = LLSpeaker::STATUS_TEXT_ONLY,
									LLSpeaker::ESpeakerType = LLSpeaker::SPEAKER_AGENT,
									const LLUUID& owner_id = LLUUID::null);

	bool isVoiceActive();

	typedef std::vector<LLPointer<LLSpeaker> > speaker_list_t;
	void getSpeakerList(speaker_list_t* speaker_list, bool include_text);
	const LLUUID getSessionID();

protected:
	void updateSpeakerList();

protected:
	typedef fast_hmap<LLUUID, LLPointer<LLSpeaker> > speaker_map_t;
	speaker_map_t		mSpeakers;

	speaker_list_t		mSpeakersSorted;
	LLFrameTimer		mSpeechTimer;
	LLVoiceChannel*		mVoiceChannel;
};

class LLIMSpeakerMgr : public LLSpeakerMgr
{
protected:
	LOG_CLASS(LLIMSpeakerMgr);

public:
	LLIMSpeakerMgr(LLVoiceChannel* channel);

	void updateSpeakers(const LLSD& update);
	void setSpeakers(const LLSD& speakers);

protected:
	virtual void updateSpeakerList();
};

class LLActiveSpeakerMgr : public LLSpeakerMgr,
						   public LLSingleton<LLActiveSpeakerMgr>
{
	friend class LLSingleton<LLActiveSpeakerMgr>;

public:
	LLActiveSpeakerMgr();

protected:
	virtual void updateSpeakerList();
};

class LLLocalSpeakerMgr : public LLSpeakerMgr,
						  public LLSingleton<LLLocalSpeakerMgr>
{
	friend class LLSingleton<LLLocalSpeakerMgr>;

public:
	LLLocalSpeakerMgr();

protected:
	virtual void updateSpeakerList();
};

class LLFloaterActiveSpeakers final
:	public LLFloater,
	public LLFloaterSingleton<LLFloaterActiveSpeakers>
{
	// Friend of singleton class to allow construction inside getInstance()
	// since constructor is protected to enforce singleton constraint
	friend class LLUISingleton<LLFloaterActiveSpeakers, VisibilityPolicy<LLFloater> >;

protected:
	LLFloaterActiveSpeakers(const LLSD& seed);

public:
	~LLFloaterActiveSpeakers() override = default;

	bool postBuild() override;
	void onOpen() override;
	void onClose(bool app_quitting) override;
	void draw() override;

	static void* createSpeakersPanel(void* data);

protected:
	LLPanelActiveSpeakers*	mPanel;
};

class LLPanelActiveSpeakers final : public LLPanel
{
protected:
	LOG_CLASS(LLPanelActiveSpeakers);

public:
	LLPanelActiveSpeakers(LLSpeakerMgr* data_source, bool show_text_chatters);

	bool postBuild() override;

	void handleSpeakerSelect();
	void refreshSpeakers(bool force = false);

	void setSpeaker(const LLUUID& id,
					const std::string& name = LLStringUtil::null,
					LLSpeaker::ESpeakerStatus status = LLSpeaker::STATUS_TEXT_ONLY,
					LLSpeaker::ESpeakerType = LLSpeaker::SPEAKER_AGENT,
					const LLUUID& owner_id = LLUUID::null);

	void addSpeaker(const LLUUID& id, bool force = false);

	void setVoiceModerationCtrlMode(const bool& moderated_voice);

	static void onClickMuteVoice(void* user_data);
	static void onClickMuteVoiceCommit(LLUICtrl* ctrl, void* user_data);
	static void onClickMuteTextCommit(LLUICtrl* ctrl, void* user_data);
	static void onVolumeChange(LLUICtrl* source, void* user_data);
	static void onClickProfile(void* user_data);
	static void onDoubleClickSpeaker(void* user_data);
	static void onSelectSpeaker(LLUICtrl* source, void* user_data);
	static void onSortChanged(void* user_data);
	static void	onModeratorMuteVoice(LLUICtrl* ctrl, void* user_data);
	static void	onModeratorMuteText(LLUICtrl* ctrl, void* user_data);
	static void	onChangeModerationMode(LLUICtrl* ctrl, void* user_data);

protected:
	class SpeakerMuteListener : public LLOldEvents::LLSimpleListener
	{
	public:
		SpeakerMuteListener(LLPanelActiveSpeakers* panel) : mPanel(panel) {}

		bool handleEvent(LLPointer<LLOldEvents::LLEvent> event,
						 const LLSD& userdata) override;

	public:
		LLPanelActiveSpeakers* mPanel;
	};

	friend class SpeakerAddListener;
	class SpeakerAddListener : public LLOldEvents::LLSimpleListener
	{
	public:
		SpeakerAddListener(LLPanelActiveSpeakers* panel) : mPanel(panel) {}

		bool handleEvent(LLPointer<LLOldEvents::LLEvent> event,
						 const LLSD& userdata) override;

		LLPanelActiveSpeakers* mPanel;
	};

	friend class SpeakerRemoveListener;
	class SpeakerRemoveListener : public LLOldEvents::LLSimpleListener
	{
	public:
		SpeakerRemoveListener(LLPanelActiveSpeakers* panel) : mPanel(panel) {}

		bool handleEvent(LLPointer<LLOldEvents::LLEvent> event,
						 const LLSD& userdata) override;

	public:
		LLPanelActiveSpeakers* mPanel;
	};

	friend class SpeakerClearListener;
	class SpeakerClearListener : public LLOldEvents::LLSimpleListener
	{
	public:
		SpeakerClearListener(LLPanelActiveSpeakers* panel) : mPanel(panel) {}

		bool handleEvent(LLPointer<LLOldEvents::LLEvent> event,
						 const LLSD& userdata) override;

	public:
		LLPanelActiveSpeakers* mPanel;
	};

	void removeSpeaker(const LLUUID& id);

private:
	static void moderatorActionFailedCallback(const LLSD& result,
											  LLUUID session_id);

protected:
	LLView*								mModerationPanel;
	LLView*								mModerationControls;

	LLScrollListCtrl*					mSpeakerList;
	LLSlider*							mSpeakerVolumeSlider;
	LLUICtrl*							mMuteVoiceCtrl;
	LLUICtrl*							mMuteTextCtrl;
	LLUICtrl*							mModeratorAllowVoiceCtrl;
	LLUICtrl*							mModeratorAllowTextCtrl;
	LLUICtrl*							mModerationModeCtrl;
	LLTextBox*							mModeratorControlsText;
	LLTextBox*							mNameText;
	LLButton*							mProfileBtn;
	LLSpeakerMgr*						mSpeakerMgr;

	LLPointer<SpeakerMuteListener>		mSpeakerMuteListener;
	LLPointer<SpeakerAddListener>		mSpeakerAddListener;
	LLPointer<SpeakerRemoveListener>	mSpeakerRemoveListener;
	LLPointer<SpeakerClearListener>		mSpeakerClearListener;

	LLFrameTimer						mIconAnimationTimer;

	bool								mShowTextChatters;
};

#endif // LL_LLFLOATERACTIVESPEAKERS_H
