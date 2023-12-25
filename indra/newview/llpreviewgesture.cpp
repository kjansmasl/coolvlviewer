/**
 * @file llpreviewgesture.cpp
 * @brief Editing UI for inventory-based gestures.
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

#include "llpreviewgesture.h"

#include "llanimationstates.h"
#include "llbutton.h"
#include "llcheckboxctrl.h"
#include "llcombobox.h"
#include "lldatapacker.h"
#include "lldir.h"
#include "llfilesystem.h"
#include "llkeyboard.h"
#include "lllineeditor.h"
#include "lllocale.h"
#include "llmultigesture.h"
#include "llnotifications.h"
#include "llradiogroup.h"
#include "llscrolllistctrl.h"
#include "lltextbox.h"
#include "lltrans.h"					// LLAnimStateLabels::getStateLabel()
#include "lluictrlfactory.h"

#include "llagent.h"
#include "llappviewer.h"
#include "llfloatergesture.h"			// For some label constants
#include "llgesturemgr.h"
#include "llinventorymodel.h"
#include "llinventorymodelfetch.h"
#include "llselectmgr.h"				// For dialog_refresh_all()
#include "llviewerassetupload.h"
#include "llviewerinventory.h"
#include "llviewerobjectlist.h"
#include "llviewerstats.h"

// *TODO: Translate ?
const std::string NONE_LABEL = "---";
const std::string SHIFT_LABEL = "Shift";
const std::string CTRL_LABEL = "Ctrl";

class LLInventoryGestureAvailable final : public LLInventoryCompletionObserver
{
public:
	LLInventoryGestureAvailable() {}

protected:
	void done() override;
};

void LLInventoryGestureAvailable::done()
{
	for (U32 i = 0, count = mComplete.size(); i < count; ++i)
	{
		LLPreview* preview = LLPreview::find(mComplete[i]);
		if (preview)
		{
			preview->refresh();
		}
	}
	gInventory.removeObserver(this);
	delete this;
}

// Used for sorting
struct SortItemPtrsByName
{
	bool operator()(const LLInventoryItem* i1, const LLInventoryItem* i2)
	{
		return (LLStringUtil::compareDict(i1->getName(), i2->getName()) < 0);
	}
};

//static
LLPreviewGesture* LLPreviewGesture::show(const std::string& title,
										 const LLUUID& item_id,
										 const LLUUID& object_id,
										 bool take_focus)
{
	LLPreviewGesture* previewp = (LLPreviewGesture*)LLPreview::find(item_id);
	if (previewp)
	{
		previewp->open();
		if (take_focus)
		{
			previewp->setFocus(true);
		}
		return previewp;
	}

	LLPreviewGesture* self = new LLPreviewGesture();

	// Finish internal construction
	self->init(item_id, object_id);

	// Builds and adds to gFloaterViewp
	LLUICtrlFactory::getInstance()->buildFloater(self,
												 "floater_preview_gesture.xml");
	self->setTitle(title);

	// Move window to top-left of screen
	LLMultiFloater* hostp = self->getHost();
	if (!hostp)
	{
		LLRect r = self->getRect();
		LLRect screen = gFloaterViewp->getRect();
		r.setLeftTopAndSize(0, screen.getHeight(), r.getWidth(), r.getHeight());
		self->setRect(r);
	}
	else
	{
		// re-add to host to update title
		hostp->addFloater(self, true);
	}

	// Start speculative download of sounds and animations
	const LLUUID animation_folder_id =
		gInventory.findCategoryUUIDForType(LLFolderType::FT_ANIMATION);
	LLInventoryModelFetch::getInstance()->start(animation_folder_id);

	const LLUUID sound_folder_id =
		gInventory.findCategoryUUIDForType(LLFolderType::FT_SOUND);
	LLInventoryModelFetch::getInstance()->start(sound_folder_id);

	// This will call refresh when we have everything.
	LLViewerInventoryItem* item = (LLViewerInventoryItem*)self->getItem();
	if (item && !item->isFinished())
	{
		LLInventoryGestureAvailable* observer;
		observer = new LLInventoryGestureAvailable();
		observer->watchItem(item_id);
		gInventory.addObserver(observer);
		item->fetchFromServer();
	}
	else
	{
		// not sure this is necessary.
		self->refresh();
	}

	if (take_focus)
	{
		self->setFocus(true);
	}

	return self;
}

// virtual
bool LLPreviewGesture::handleKeyHere(KEY key, MASK mask)
{
	if (key == 'S' && (mask & MASK_CONTROL))
	{
		saveIfNeeded();
		return true;
	}

	return LLPreview::handleKeyHere(key, mask);
}

// virtual
bool LLPreviewGesture::handleDragAndDrop(S32 x, S32 y, MASK mask, bool drop,
										 EDragAndDropType cargo_type,
										 void* cargo_data,
										 EAcceptance* accept,
										 std::string& tooltip_msg)
{
	switch (cargo_type)
	{
		case DAD_ANIMATION:
		case DAD_SOUND:
		{
			// Make a script step
			LLInventoryItem* itemp = (LLInventoryItem*)cargo_data;
			if (itemp && gInventory.getItem(itemp->getUUID()))
			{
				if (!itemp->getPermissions().unrestricted())
				{
					*accept = ACCEPT_NO;
					if (tooltip_msg.empty())
					{
						tooltip_msg.assign("Only animations and sounds\n"
											"with unrestricted permissions\n"
											"can be added to a gesture.");
					}
					break;
				}
				if (drop)
				{
					LLScrollListItem* linep = NULL;
					if (cargo_type == DAD_ANIMATION)
					{
						linep = addStep(STEP_ANIMATION);
						LLGestureStepAnimation* animp =
							(LLGestureStepAnimation*)linep->getUserdata();
						animp->mAnimAssetID = itemp->getAssetUUID();
						animp->mAnimName = itemp->getName();
					}
					else if (cargo_type == DAD_SOUND)
					{
						linep = addStep(STEP_SOUND);
						LLGestureStepSound* soundp =
							(LLGestureStepSound*)linep->getUserdata();
						soundp->mSoundAssetID = itemp->getAssetUUID();
						soundp->mSoundName = itemp->getName();
					}
					updateLabel(linep);
					mDirty = true;
					refresh();
				}
				*accept = ACCEPT_YES_COPY_MULTI;
			}
			else
			{
				// Not in user's inventory means it was in object inventory
				*accept = ACCEPT_NO;
			}
			break;
		}

		default:
		{
			*accept = ACCEPT_NO;
			if (tooltip_msg.empty())
			{
				tooltip_msg.assign("Only animations and sounds can be added to a gesture.");
			}
			break;
		}
	}

	return true;
}

// virtual
bool LLPreviewGesture::canClose()
{
	if (!mDirty || mForceClose)
	{
		return true;
	}
	else
	{
		if (!mSaveDialogShown)
		{
			mSaveDialogShown = true;
			// Bring up view-modal dialog: Save changes ? Yes, No, Cancel
			gNotifications.add("SaveChanges", LLSD(), LLSD(),
							   boost::bind(&LLPreviewGesture::handleSaveChangesDialog,
										   this, _1, _2));
		}
		return false;
	}
}

// virtual
void LLPreviewGesture::onClose(bool app_quitting)
{
	gGestureManager.stopGesture(mPreviewGesture);
	LLPreview::onClose(app_quitting);
}

// virtual
void LLPreviewGesture::onUpdateSucceeded()
{
	refresh();
}

// virtual
void LLPreviewGesture::setMinimized(bool minimize)
{
	if (minimize != isMinimized())
	{
		LLFloater::setMinimized(minimize);

		// We're being restored
		if (!minimize)
		{
			refresh();
		}
	}
}

bool LLPreviewGesture::handleSaveChangesDialog(const LLSD& notification,
											   const LLSD& response)
{
	mSaveDialogShown = false;

	S32 option = LLNotification::getSelectedOption(notification, response);
	switch (option)
	{
		case 0:  // "Yes"
			gGestureManager.stopGesture(mPreviewGesture);
			mCloseAfterSave = true;
			onClickSave(this);
			break;

		case 1:  // "No"
			gGestureManager.stopGesture(mPreviewGesture);
			// Force the dirty flag because user has clicked NO on confirm save
			// dialog...
			mDirty = false;
			close();
			break;

		case 2: // "Cancel"
		default:
			// If we were quitting, we didn't really mean it.
			gAppViewerp->abortQuit();
	}

	return false;
}

LLPreviewGesture::LLPreviewGesture()
:	LLPreview("Gesture Preview"),
	mTriggerEditor(NULL),
	mModifierCombo(NULL),
	mKeyCombo(NULL),
	mLibraryList(NULL),
	mAddBtn(NULL),
	mUpBtn(NULL),
	mDownBtn(NULL),
	mDeleteBtn(NULL),
	mStepList(NULL),
	mOptionsText(NULL),
	mAnimationRadio(NULL),
	mAnimationCombo(NULL),
	mSoundCombo(NULL),
	mChatEditor(NULL),
	mSaveBtn(NULL),
	mPreviewBtn(NULL),
	mPreviewGesture(NULL),
	mDirty(false)
{
}

LLPreviewGesture::~LLPreviewGesture()
{
	// Userdata for all steps is a LLGestureStep we need to clean up
	std::vector<LLScrollListItem*> data_list = mStepList->getAllData();
	for (S32 i = 0, count = data_list.size(); i < count; ++i)
	{
		LLScrollListItem* item = data_list[i];
		if (item)	// Paranoia
		{
			LLGestureStep* step = (LLGestureStep*)item->getUserdata();
			if (step)
			{
				delete step;
			}
		}
	}
}

bool LLPreviewGesture::postBuild()
{
	mTriggerEditor = getChild<LLLineEditor>("trigger_editor");
	mTriggerEditor->setKeystrokeCallback(onKeystrokeCommit);
	mTriggerEditor->setCommitCallback(onCommitSetDirty);
	mTriggerEditor->setCommitOnFocusLost(true);
	mTriggerEditor->setCallbackUserData(this);
	mTriggerEditor->setIgnoreTab(true);

	mReplaceText = getChild<LLTextBox>("replace_text");
	mReplaceText->setEnabled(false);

	mReplaceEditor = getChild<LLLineEditor>("replace_editor");
	mReplaceEditor->setEnabled(false);
	mReplaceEditor->setKeystrokeCallback(onKeystrokeCommit);
	mReplaceEditor->setCommitCallback(onCommitSetDirty);
	mReplaceEditor->setCommitOnFocusLost(true);
	mReplaceEditor->setCallbackUserData(this);
	mReplaceEditor->setIgnoreTab(true);

	mModifierCombo = getChild<LLComboBox>("modifier_combo");
	mModifierCombo->setCommitCallback(onCommitSetDirty);
	mModifierCombo->setCallbackUserData(this);

	mKeyCombo = getChild<LLComboBox>("key_combo");
	mKeyCombo->setCommitCallback(onCommitSetDirty);
	mKeyCombo->setCallbackUserData(this);

	mLibraryList = getChild<LLScrollListCtrl>("library_list");
	mLibraryList->setCommitCallback(onCommitLibrary);
	mLibraryList->setDoubleClickCallback(onClickAdd);
	mLibraryList->setCallbackUserData(this);

	mAddBtn = getChild<LLButton>("add_btn");
	mAddBtn->setClickedCallback(onClickAdd);
	mAddBtn->setCallbackUserData(this);
	mAddBtn->setEnabled(false);

	mUpBtn = getChild<LLButton>("up_btn");
	mUpBtn->setClickedCallback(onClickUp);
	mUpBtn->setCallbackUserData(this);
	mUpBtn->setEnabled(false);

	mDownBtn = getChild<LLButton>("down_btn");
	mDownBtn->setClickedCallback(onClickDown);
	mDownBtn->setCallbackUserData(this);
	mDownBtn->setEnabled(false);

	mDeleteBtn = getChild<LLButton>("delete_btn");
	mDeleteBtn->setClickedCallback(onClickDelete);
	mDeleteBtn->setCallbackUserData(this);
	mDeleteBtn->setEnabled(false);

	mStepList = getChild<LLScrollListCtrl>("step_list");
	mStepList->setCommitCallback(onCommitStep);
	mStepList->setCallbackUserData(this);

	// Options
	mOptionsText = getChild<LLTextBox>("options_text");
	mOptionsText->setBorderVisible(true);

	mAnimationCombo = getChild<LLComboBox>("animation_list");
	mAnimationCombo->setVisible(false);
	mAnimationCombo->setCommitCallback(onCommitAnimation);
	mAnimationCombo->setCallbackUserData(this);

	mAnimationRadio = getChild<LLRadioGroup>("animation_trigger_type");
	mAnimationRadio->setVisible(false);
	mAnimationRadio->setCommitCallback(onCommitAnimationTrigger);
	mAnimationRadio->setCallbackUserData(this);

	mSoundCombo = getChild<LLComboBox>("sound_list");
	mSoundCombo->setVisible(false);
	mSoundCombo->setCommitCallback(onCommitSound);
	mSoundCombo->setCallbackUserData(this);

	mChatEditor = getChild<LLLineEditor>("chat_editor");
	mChatEditor->setVisible(false);
	mChatEditor->setCommitCallback(onCommitChat);
	//mChatEditor->setKeystrokeCallback(onKeystrokeCommit);
	mChatEditor->setCommitOnFocusLost(true);
	mChatEditor->setCallbackUserData(this);
	mChatEditor->setIgnoreTab(true);

	mWaitAnimCheck = getChild<LLCheckBoxCtrl>("wait_anim_check");
	mWaitAnimCheck->setVisible(false);
	mWaitAnimCheck->setCommitCallback(onCommitWait);
	mWaitAnimCheck->setCallbackUserData(this);

	mWaitTimeCheck = getChild<LLCheckBoxCtrl>("wait_time_check");
	mWaitTimeCheck->setVisible(false);
	mWaitTimeCheck->setCommitCallback(onCommitWait);
	mWaitTimeCheck->setCallbackUserData(this);

	mWaitTimeEditor = getChild<LLLineEditor>("wait_time_editor");
	mWaitTimeEditor->setEnabled(false);
	mWaitTimeEditor->setVisible(false);
	mWaitTimeEditor->setPrevalidate(LLLineEditor::prevalidateFloat);
	//mWaitTimeEditor->setKeystrokeCallback(onKeystrokeCommit);
	mWaitTimeEditor->setCommitOnFocusLost(true);
	mWaitTimeEditor->setCommitCallback(onCommitWaitTime);
	mWaitTimeEditor->setCallbackUserData(this);
	mWaitTimeEditor->setIgnoreTab(true);

	// Buttons at the bottom
	mActiveCheck = getChild<LLCheckBoxCtrl>("active_check");
	mActiveCheck->setCommitCallback(onCommitActive);
	mActiveCheck->setCallbackUserData(this);

	mSaveBtn = getChild<LLButton>("save_btn");
	mSaveBtn->setClickedCallback(onClickSave);
	mSaveBtn->setCallbackUserData(this);

	mPreviewBtn = getChild<LLButton>("preview_btn");
	mPreviewBtn->setClickedCallback(onClickPreview);
	mPreviewBtn->setCallbackUserData(this);

	// Populate the combo boxes
	addModifiers();
	addKeys();
	addAnimations();
	addSounds();

	const LLInventoryItem* item = getItem();
	if (item)
	{
		childSetCommitCallback("desc", LLPreview::onText, this);
		childSetText("desc", item->getDescription());
		childSetPrevalidate("desc",
							&LLLineEditor::prevalidatePrintableNotPipe);
	}

	return true;
}

void LLPreviewGesture::addModifiers()
{
	mModifierCombo->add(NONE_LABEL,  ADD_BOTTOM);
	mModifierCombo->add(SHIFT_LABEL, ADD_BOTTOM);
	mModifierCombo->add(CTRL_LABEL,  ADD_BOTTOM);
	mModifierCombo->setCurrentByIndex(0);
}

void LLPreviewGesture::addKeys()
{
	mKeyCombo->add(NONE_LABEL);
	for (KEY key = KEY_F2; key <= KEY_F12; key++)
	{
		mKeyCombo->add(LLKeyboard::stringFromKey(key), ADD_BOTTOM);
	}
	mKeyCombo->setCurrentByIndex(0);
}

// TODO: Sort the legacy and non-legacy together?
void LLPreviewGesture::addAnimations()
{
	mAnimationCombo->removeall();

	std::string none_text = getString("none_text");

	mAnimationCombo->add(none_text, LLUUID::null);

	std::string label;
	// Add all the default (legacy) animations
	for (S32 i = 0; i < gUserAnimStatesCount; ++i)
	{
		// Use the user-readable name
		label = LLAnimStateLabels::getStateLabel(gUserAnimStates[i].mName);
		mAnimationCombo->add(label, gUserAnimStates[i].mID);
	}

	// Get all inventory items that are animations
	LLViewerInventoryCategory::cat_array_t cats;
	LLViewerInventoryItem::item_array_t items;
	LLIsTypeWithPermissions is_copyable_animation(LLAssetType::AT_ANIMATION,
												  PERM_ITEM_UNRESTRICTED,
												  gAgentID,
												  gAgent.getGroupID());
	gInventory.collectDescendentsIf(gInventory.getRootFolderID(), cats, items,
									LLInventoryModel::EXCLUDE_TRASH,
									is_copyable_animation);

	// Copy into something we can sort
	std::vector<LLInventoryItem*> animations;
	for (S32 i = 0, count = items.size(); i < count; ++i)
	{
		animations.push_back(items[i]);
	}

	// Do the sort
	std::sort(animations.begin(), animations.end(), SortItemPtrsByName());

	// And load up the combobox
	for (S32 i = 0, count = animations.size(); i < count; ++i)
	{
		LLInventoryItem* item = animations[i];
		if (item)	// Paranoia
		{
			mAnimationCombo->add(item->getName(), item->getAssetUUID(),
								 ADD_BOTTOM);
		}
	}
}

void LLPreviewGesture::addSounds()
{
	mSoundCombo->removeall();

	std::string none_text = getString("none_text");

	mSoundCombo->add(none_text, LLUUID::null);

	// Get all inventory items that are sounds
	LLViewerInventoryCategory::cat_array_t cats;
	LLViewerInventoryItem::item_array_t items;
	LLIsTypeWithPermissions is_copyable_sound(LLAssetType::AT_SOUND,
											  PERM_ITEM_UNRESTRICTED,
											  gAgentID, gAgent.getGroupID());
	gInventory.collectDescendentsIf(gInventory.getRootFolderID(),
									cats, items,
									LLInventoryModel::EXCLUDE_TRASH,
									is_copyable_sound);

	// Copy sounds into something we can sort
	std::vector<LLInventoryItem*> sounds;
	for (S32 i = 0, count = items.size(); i < count; ++i)
	{
		sounds.push_back(items[i]);
	}

	// Do the sort
	std::sort(sounds.begin(), sounds.end(), SortItemPtrsByName());

	// And load up the combo box
	for (S32 i = 0, count = sounds.size(); i < count; ++i)
	{
		LLInventoryItem* item = sounds[i];
		if (item)	// Paranoia
		{
			mSoundCombo->add(item->getName(), item->getAssetUUID(),
							 ADD_BOTTOM);
		}
	}
}

void LLPreviewGesture::init(const LLUUID& item_id, const LLUUID& object_id)
{
	// Sets ID and adds to instance list
	setItemID(item_id);
	setObjectID(object_id);
}

void LLPreviewGesture::refresh()
{
	// If previewing or item is incomplete, all controls are disabled
	LLViewerInventoryItem* item = (LLViewerInventoryItem*)getItem();
	if (mPreviewGesture || !item || !item->isFinished())
	{
		childSetEnabled("desc", false);
		//mDescEditor->setEnabled(false);
		mTriggerEditor->setEnabled(false);
		mReplaceText->setEnabled(false);
		mReplaceEditor->setEnabled(false);
		mModifierCombo->setEnabled(false);
		mKeyCombo->setEnabled(false);
		mLibraryList->setEnabled(false);
		mAddBtn->setEnabled(false);
		mUpBtn->setEnabled(false);
		mDownBtn->setEnabled(false);
		mDeleteBtn->setEnabled(false);
		mStepList->setEnabled(false);
		mOptionsText->setEnabled(false);
		mAnimationCombo->setEnabled(false);
		mAnimationRadio->setEnabled(false);
		mSoundCombo->setEnabled(false);
		mChatEditor->setEnabled(false);
		mWaitAnimCheck->setEnabled(false);
		mWaitTimeCheck->setEnabled(false);
		mWaitTimeEditor->setEnabled(false);
		mActiveCheck->setEnabled(false);
		mSaveBtn->setEnabled(false);

		// Make sure preview button is enabled, so we can stop it
		mPreviewBtn->setEnabled(true);
		return;
	}

	bool modifiable = item->getPermissions().allowModifyBy(gAgentID);

	childSetEnabled("desc", modifiable);
	mTriggerEditor->setEnabled(true);
	mLibraryList->setEnabled(modifiable);
	mStepList->setEnabled(modifiable);
	mOptionsText->setEnabled(modifiable);
	mAnimationCombo->setEnabled(modifiable);
	mAnimationRadio->setEnabled(modifiable);
	mSoundCombo->setEnabled(modifiable);
	mChatEditor->setEnabled(modifiable);
	mWaitAnimCheck->setEnabled(modifiable);
	mWaitTimeCheck->setEnabled(modifiable);
	mWaitTimeEditor->setEnabled(modifiable);
	mActiveCheck->setEnabled(true);

	const std::string& trigger = mTriggerEditor->getText();
	bool have_trigger = !trigger.empty();

	const std::string& replace = mReplaceEditor->getText();
	bool have_replace = !replace.empty();

	LLScrollListItem* library_item = mLibraryList->getFirstSelected();
	bool have_library = library_item != NULL;

	LLScrollListItem* step_item = mStepList->getFirstSelected();
	S32 step_index = mStepList->getFirstSelectedIndex();
	S32 step_count = mStepList->getItemCount();
	bool have_step = step_item != NULL;

	mReplaceText->setEnabled(have_trigger || have_replace);
	mReplaceEditor->setEnabled(have_trigger || have_replace);

	mModifierCombo->setEnabled(true);
	mKeyCombo->setEnabled(true);

	mAddBtn->setEnabled(modifiable && have_library);
	mUpBtn->setEnabled(modifiable && have_step && step_index > 0);
	mDownBtn->setEnabled(modifiable && have_step &&
						 step_index < step_count - 1);
	mDeleteBtn->setEnabled(modifiable && have_step);

	// Assume all not visible
	mAnimationCombo->setVisible(false);
	mAnimationRadio->setVisible(false);
	mSoundCombo->setVisible(false);
	mChatEditor->setVisible(false);
	mWaitAnimCheck->setVisible(false);
	mWaitTimeCheck->setVisible(false);
	mWaitTimeEditor->setVisible(false);

	std::string optionstext;

	if (have_step)
	{
		// Figure out the type, show proper options, update text
		LLGestureStep* step = (LLGestureStep*)step_item->getUserdata();
		EStepType type = step->getType();

		switch (type)
		{
			case STEP_ANIMATION:
			{
				LLGestureStepAnimation* anim_step = (LLGestureStepAnimation*)step;
				optionstext = getString("step_anim");
				mAnimationCombo->setVisible(true);
				mAnimationRadio->setVisible(true);
				mAnimationRadio->setSelectedIndex((anim_step->mFlags & ANIM_FLAG_STOP) ? 1 : 0);
				mAnimationCombo->setCurrentByID(anim_step->mAnimAssetID);
				break;
			}

			case STEP_SOUND:
			{
				LLGestureStepSound* sound_step = (LLGestureStepSound*)step;
				optionstext = getString("step_sound");
				mSoundCombo->setVisible(true);
				mSoundCombo->setCurrentByID(sound_step->mSoundAssetID);
				break;
			}

			case STEP_CHAT:
			{
				LLGestureStepChat* chat_step = (LLGestureStepChat*)step;
				optionstext = getString("step_chat");
				mChatEditor->setVisible(true);
				mChatEditor->setText(chat_step->mChatText);
				break;
			}

			case STEP_WAIT:
			{
				LLGestureStepWait* wait_step = (LLGestureStepWait*)step;
				optionstext = getString("step_wait");
				mWaitAnimCheck->setVisible(true);
				mWaitAnimCheck->set(wait_step->mFlags & WAIT_FLAG_ALL_ANIM);
				mWaitTimeCheck->setVisible(true);
				mWaitTimeCheck->set(wait_step->mFlags & WAIT_FLAG_TIME);
				mWaitTimeEditor->setVisible(true);
				std::string buffer = llformat("%.1f",
											  (double)wait_step->mWaitSeconds);
				mWaitTimeEditor->setText(buffer);
				break;
			}

			default:
				break;
		}
	}

	mOptionsText->setText(optionstext);

	bool active = gGestureManager.isGestureActive(mItemUUID);
	mActiveCheck->set(active);

	// Can only preview if there are steps
	mPreviewBtn->setEnabled(step_count > 0);

	// And can only save if changes have been made
	mSaveBtn->setEnabled(mDirty);
	addAnimations();
	addSounds();
}

void LLPreviewGesture::initDefaultGesture()
{
	LLScrollListItem* item = addStep(STEP_ANIMATION);
	LLGestureStepAnimation* anim =
		(LLGestureStepAnimation*)item->getUserdata();
	anim->mAnimAssetID = ANIM_AGENT_HELLO;
	anim->mAnimName = "Wave";
	updateLabel(item);

	item = addStep(STEP_WAIT);
	LLGestureStepWait* wait = (LLGestureStepWait*)item->getUserdata();
	wait->mFlags = WAIT_FLAG_ALL_ANIM;
	updateLabel(item);

	item = addStep(STEP_CHAT);
	LLGestureStepChat* chat_step = (LLGestureStepChat*)item->getUserdata();
	chat_step->mChatText = "Hello, avatar!";
	updateLabel(item);

	// Start with item list selected
	mStepList->selectFirstItem();

	// This is *new* content, so we are dirty
	mDirty = true;
}

void LLPreviewGesture::loadAsset()
{
	const LLInventoryItem* item = getItem();
	if (!item) return;

	LLUUID asset_id = item->getAssetUUID();
	if (asset_id.isNull())
	{
		// Freshly created gesture, don't need to load asset. Blank gesture
		// will be fine.
		initDefaultGesture();
		refresh();
		return;
	}

	// *TODO: Based on item->getPermissions().allow* could enable/disable UI.

	// Copy the UUID, because the user might close the preview window if the
	// download gets stalled.
	LLUUID* item_idp = new LLUUID(mItemUUID);

	gAssetStoragep->getAssetData(asset_id, LLAssetType::AT_GESTURE,
								 onLoadComplete, (void**)item_idp,
								 true);	// high priority
	mAssetStatus = PREVIEW_ASSET_LOADING;
}

//static
void LLPreviewGesture::onLoadComplete(const LLUUID& asset_uuid,
									  LLAssetType::EType type, void* user_data,
									  S32 status, LLExtStat)
{
	LLUUID* item_idp = (LLUUID*)user_data;
	LLPreview* preview = LLPreview::find(*item_idp);
	if (preview)
	{
		LLPreviewGesture* self = (LLPreviewGesture*)preview;

		if (status == 0)
		{
			LLFileSystem file(asset_uuid);
			S32 size = file.getSize();

			char* buffer = new char[size + 1];
			file.read((U8*)buffer, size);
			buffer[size] = '\0';

			LLMultiGesture* gesture = new LLMultiGesture();

			LLDataPackerAsciiBuffer dp(buffer, size+1);
			bool ok = gesture->deserialize(dp);
			if (ok)
			{
				// Everything has been successful.  Load up the UI.
				self->loadUIFromGesture(gesture);

				self->mStepList->selectFirstItem();

				self->mDirty = false;
				self->refresh();
			}
			else
			{
				llwarns << "Unable to load gesture" << llendl;
			}

			delete gesture;
			gesture = NULL;

			delete[] buffer;
			buffer = NULL;

			self->mAssetStatus = PREVIEW_ASSET_LOADED;
		}
		else
		{
			gViewerStats.incStat(LLViewerStats::ST_DOWNLOAD_FAILED);
			LLGestureManager::notifyLoadFailed(*item_idp, status);
			llwarns << "Problem loading gesture: " << status << llendl;
			self->mAssetStatus = PREVIEW_ASSET_ERROR;
		}
	}
	delete item_idp;
	item_idp = NULL;
}

void LLPreviewGesture::loadUIFromGesture(LLMultiGesture* gesture)
{
#if 0
	LLInventoryItem* item = getItem();
	if (item)
	{
		LLLineEditor* descEditor = getChild<LLLineEditor>("desc");
		descEditor->setText(item->getDescription());
	}
#endif

	mTriggerEditor->setText(gesture->mTrigger);

	mReplaceEditor->setText(gesture->mReplaceText);

	switch (gesture->mMask)
	{
		case MASK_SHIFT:
			mModifierCombo->setSimple(SHIFT_LABEL);
			break;

		case MASK_CONTROL:
			mModifierCombo->setSimple(CTRL_LABEL);
			break;

		case MASK_NONE:
		default:
			mModifierCombo->setSimple(NONE_LABEL);
	}

	mKeyCombo->setCurrentByIndex(0);
	if (gesture->mKey != KEY_NONE)
	{
		mKeyCombo->setSimple(LLKeyboard::stringFromKey(gesture->mKey));
	}

	// Make UI steps for each gesture step
	for (S32 i = 0, count = gesture->mSteps.size(); i < count; ++i)
	{
		LLGestureStep* step = gesture->mSteps[i];
		if (!step) continue;	// Paranoia

		LLGestureStep* new_step = NULL;

		switch (step->getType())
		{
			case STEP_ANIMATION:
			{
				LLGestureStepAnimation* anim_step =
					(LLGestureStepAnimation*)step;
				LLGestureStepAnimation* new_anim_step;
				new_anim_step = new LLGestureStepAnimation(*anim_step);
				new_step = new_anim_step;
				break;
			}

			case STEP_SOUND:
			{
				LLGestureStepSound* sound_step = (LLGestureStepSound*)step;
				LLGestureStepSound* new_sound_step;
				new_sound_step = new LLGestureStepSound(*sound_step);
				new_step = new_sound_step;
				break;
			}

			case STEP_CHAT:
			{
				LLGestureStepChat* chat_step = (LLGestureStepChat*)step;
				LLGestureStepChat* new_chat_step;
				new_chat_step = new LLGestureStepChat(*chat_step);
				new_step = new_chat_step;
				break;
			}

			case STEP_WAIT:
			{
				LLGestureStepWait* wait_step = (LLGestureStepWait*)step;
				LLGestureStepWait* new_wait_step;
				new_wait_step = new LLGestureStepWait(*wait_step);
				new_step = new_wait_step;
				break;
			}

			default:
				break;
		}

		if (!new_step) continue;

		// Create an enabled item with this step
		LLSD row;
		row["columns"][0]["value"] = new_step->getLabel();
		row["columns"][0]["font"] = "SANSSERIF_SMALL";
		LLScrollListItem* item = mStepList->addElement(row);
		item->setUserdata(new_step);
	}
}

// Helpful structure so we can look up the inventory item
// after the save finishes.
struct LLSaveInfo
{
	LLSaveInfo(const LLUUID& item_id, const LLUUID& object_id,
			   const std::string& desc, const LLTransactionID tid)
	:	mItemUUID(item_id),
		mObjectUUID(object_id),
		mDesc(desc),
		mTransactionID(tid)
	{
	}

	LLUUID mItemUUID;
	LLUUID mObjectUUID;
	LLTransactionID mTransactionID;
	std::string mDesc;
};

void LLPreviewGesture::saveIfNeeded()
{
	if (!gAssetStoragep)
	{
		llwarns << "Cannot save gesture: no asset storage system." << llendl;
		return;
	}

	if (!mDirty)
	{
		return;
	}

	// Copy the UI into a gesture
	LLMultiGesture* gesture = createGesture();

	// Serialize the gesture
	S32 max_size = gesture->getMaxSerialSize();
	char* buffer = new char[max_size];

	LLDataPackerAsciiBuffer dp(buffer, max_size);

	bool ok = gesture->serialize(dp);

	if (dp.getCurrentSize() > 1000)
	{
		gNotifications.add("GestureSaveFailedTooManySteps");
		delete gesture;
		gesture = NULL;
	}
	else if (!ok)
	{
		gNotifications.add("GestureSaveFailedTryAgain");
		delete gesture;
		gesture = NULL;
	}
	else
	{
		LLAssetID asset_id;
		bool delayed_upload = false;

		// Upload that asset to the database
		LLViewerInventoryItem* item = (LLViewerInventoryItem*) getItem();
		if (item)
		{
			const std::string& agent_url =
				gAgent.getRegionCapability("UpdateGestureAgentInventory");
			const std::string& task_url =
				gAgent.getRegionCapability("UpdateGestureTaskInventory");
			if (!agent_url.empty() && !task_url.empty())
			{
				const std::string* url = NULL;
				LLResourceUploadInfo::ptr_t info;

				if (mObjectUUID.isNull() && !agent_url.empty())
				{
					// Saving into agent inventory. We need to disable the
					// preview floater so item is not re-saved before the new
					// asset arrives; fake out a refresh.
					item->setComplete(false);
					refresh();
					item->setComplete(true);

					info = std::make_shared<LLBufferedAssetUploadInfo>(mItemUUID,
																	   LLAssetType::AT_GESTURE,
																	   buffer,
																	   boost::bind(&LLPreviewGesture::finishInventoryUpload,
																				   _1, _2));
					url = &agent_url;
					delayed_upload = true;
				}
				else if (mObjectUUID.notNull() && !task_url.empty())
				{
					// Saving into task inventory
					// NOTE: what looks like a bug in std::make_shared prevents
					// from using it here, because of the NULL boost::function
					// pointer passed... And no, passing nullptr won't change a
					// thing. HB
					info = LLResourceUploadInfo::ptr_t(new LLBufferedAssetUploadInfo(mObjectUUID,
																					 mItemUUID,
																					 LLAssetType::AT_GESTURE,
																					 buffer,
																					 NULL));
					url = &task_url;
				}
				if (url && info)
				{
					LLViewerAssetUpload::enqueueInventoryUpload(*url, info);
				}
			}
			else if (gAssetStoragep)
			{
				// Every save gets a new UUID. Yup.
				LLTransactionID tid;
				tid.generate();
				asset_id = tid.makeAssetID(gAgent.getSecureSessionID());

				LLFileSystem file(asset_id, LLFileSystem::APPEND);

				S32 size = dp.getCurrentSize();
				file.write((U8*)buffer, size);

				LLLineEditor* descEditor = getChild<LLLineEditor>("desc");
				LLSaveInfo* info = new LLSaveInfo(mItemUUID, mObjectUUID,
												  descEditor->getText(), tid);
				gAssetStoragep->storeAssetData(tid, LLAssetType::AT_GESTURE,
											   onSaveComplete, info, false);
			}
			else
			{
				llwarns << "No capability neither asset storage system. Could not save gesture: "
						<< mItemUUID << llendl;
				delete[] buffer;
				buffer = NULL;
				return;
			}
		}

		// If this gesture is active, then we need to update the in-memory
		// active map with the new pointer.
		if (!delayed_upload && gGestureManager.isGestureActive(mItemUUID))
		{
			// Gesture manager now owns the pointer
			gGestureManager.replaceGesture(mItemUUID, gesture, asset_id);

			// replaceGesture() may deactivate other gestures so let the
			// inventory know.
			gInventory.notifyObservers();
		}
		else
		{
			// We are done with this gesture
			delete gesture;
			gesture = NULL;
		}

		mDirty = false;
		// Refresh will be called when the callback happens if triggered when
		// delayed_upload == true
		if (!delayed_upload)
		{
			refresh();
		}
	}

	delete[] buffer;
	buffer = NULL;
}

//static
void LLPreviewGesture::finishInventoryUpload(LLUUID item_id,
											 LLUUID new_asset_id)
{
	if (item_id.isNull()) return;

	// If this gesture is active, then we need to update the in-memory active
	// map with the new pointer.
	if (gGestureManager.isGestureActive(item_id) && new_asset_id.notNull())
	{
		gGestureManager.replaceGesture(item_id, new_asset_id);
		gInventory.notifyObservers();
	}

	// Gesture will have a new asset_id
	LLPreviewGesture* self = (LLPreviewGesture*)LLPreview::find(item_id);
	if (self)
	{
		self->setAssetId(new_asset_id);
		self->onUpdateSucceeded();
	}
}

// StoreAssetData callback (fixed)
// *TODO: This is very similar to LLPreviewNotecard::onSaveComplete. Could
// merge this code.
//static
void LLPreviewGesture::onSaveComplete(const LLUUID& asset_uuid,
									  void* user_data, S32 status,
									  LLExtStat)
{
	LLSaveInfo* info = (LLSaveInfo*)user_data;
	if (info && status == 0)
	{
		if (info->mObjectUUID.isNull())
		{
			// Saving into user inventory
			LLViewerInventoryItem* item;
			item = (LLViewerInventoryItem*)gInventory.getItem(info->mItemUUID);
			if (item)
			{
				LLPointer<LLViewerInventoryItem> new_item = new LLViewerInventoryItem(item);
				new_item->setDescription(info->mDesc);
				new_item->setTransactionID(info->mTransactionID);
				new_item->setAssetUUID(asset_uuid);
				new_item->updateServer(false);
				gInventory.updateItem(new_item);
				gInventory.notifyObservers();
			}
			else
			{
				llwarns << "Inventory item for gesture " << info->mItemUUID
						<< " is no longer in agent inventory." << llendl;
			}
		}
		else
		{
			// Saving into in-world object inventory
			LLViewerObject* object = gObjectList.findObject(info->mObjectUUID);
			LLViewerInventoryItem* item = NULL;
			if (object)
			{
				item = (LLViewerInventoryItem*)object->getInventoryObject(info->mItemUUID);
			}
			if (object && item)
			{
				item->setDescription(info->mDesc);
				item->setAssetUUID(asset_uuid);
				item->setTransactionID(info->mTransactionID);
				object->updateInventory(item);
				dialog_refresh_all();
			}
			else
			{
				gNotifications.add("GestureSaveFailedObjectNotFound");
			}
		}

		// Find our window and close it if requested.
		LLPreviewGesture* previewp;
		previewp = (LLPreviewGesture*)LLPreview::find(info->mItemUUID);
		if (previewp && previewp->mCloseAfterSave)
		{
			previewp->close();
		}
	}
	else
	{
		llwarns << "Problem saving gesture: " << status << llendl;
		LLSD args;
		args["REASON"] = std::string(LLAssetStorage::getErrorString(status));
		gNotifications.add("GestureSaveFailedReason", args);
	}
	delete info;
	info = NULL;
}

LLMultiGesture* LLPreviewGesture::createGesture()
{
	LLMultiGesture* gesture = new LLMultiGesture();

	gesture->mTrigger = mTriggerEditor->getText();
	gesture->mReplaceText = mReplaceEditor->getText();

	const std::string& modifier = mModifierCombo->getSimple();
	if (modifier == CTRL_LABEL)
	{
		gesture->mMask = MASK_CONTROL;
	}
	else if (modifier == SHIFT_LABEL)
	{
		gesture->mMask = MASK_SHIFT;
	}
	else
	{
		gesture->mMask = MASK_NONE;
	}

	if (mKeyCombo->getCurrentIndex() == 0)
	{
		gesture->mKey = KEY_NONE;
	}
	else
	{
		const std::string& key_string = mKeyCombo->getSimple();
		LLKeyboard::keyFromString(key_string.c_str(), &(gesture->mKey));
	}

	std::vector<LLScrollListItem*> data_list = mStepList->getAllData();
	for (S32 i = 0, count = data_list.size(); i < count; ++i)
	{
		LLScrollListItem* item = data_list[i];
		if (!item) continue;	// Paranoia

		LLGestureStep* step = (LLGestureStep*)item->getUserdata();
		if (!step) continue;	// Paranoia

		switch (step->getType())
		{
			case STEP_ANIMATION:
			{
				// Copy UI-generated step into actual gesture step
				LLGestureStepAnimation* anim_step =
					(LLGestureStepAnimation*)step;
				LLGestureStepAnimation* new_anim_step =
					new LLGestureStepAnimation(*anim_step);
				gesture->mSteps.push_back(new_anim_step);
				break;
			}

			case STEP_SOUND:
			{
				// Copy UI-generated step into actual gesture step
				LLGestureStepSound* sound_step = (LLGestureStepSound*)step;
				LLGestureStepSound* new_sound_step =
					new LLGestureStepSound(*sound_step);
				gesture->mSteps.push_back(new_sound_step);
				break;
			}

			case STEP_CHAT:
			{
				// Copy UI-generated step into actual gesture step
				LLGestureStepChat* chat_step = (LLGestureStepChat*)step;
				LLGestureStepChat* new_chat_step =
					new LLGestureStepChat(*chat_step);
				gesture->mSteps.push_back(new_chat_step);
				break;
			}

			case STEP_WAIT:
			{
				// Copy UI-generated step into actual gesture step
				LLGestureStepWait* wait_step = (LLGestureStepWait*)step;
				LLGestureStepWait* new_wait_step =
					new LLGestureStepWait(*wait_step);
				gesture->mSteps.push_back(new_wait_step);
				break;
			}

			default:
				break;
		}
	}

	return gesture;
}

//static
void LLPreviewGesture::updateLabel(LLScrollListItem* item)
{
	LLGestureStep* step = (LLGestureStep*)item->getUserdata();
	if (!step) return;	// Paranoia

	LLScrollListCell* cell = item->getColumn(0);
	if (!cell) return;	// Paranoia

	LLScrollListText* text_cell = (LLScrollListText*)cell;
	std::string label = step->getLabel();
	text_cell->setText(label);
}

//static
void LLPreviewGesture::onCommitSetDirty(LLUICtrl* ctrl, void* data)
{
	LLPreviewGesture* self = (LLPreviewGesture*)data;
	if (self)
	{
		self->mDirty = true;
		self->refresh();
	}
}

//static
void LLPreviewGesture::onCommitLibrary(LLUICtrl* ctrl, void* data)
{
	LLPreviewGesture* self = (LLPreviewGesture*)data;
	if (!self) return;

	LLScrollListItem* library_item = self->mLibraryList->getFirstSelected();
	if (library_item)
	{
		self->mStepList->deselectAllItems();
		self->refresh();
	}
}

//static
void LLPreviewGesture::onCommitStep(LLUICtrl* ctrl, void* data)
{
	LLPreviewGesture* self = (LLPreviewGesture*)data;
	if (!self) return;

	LLScrollListItem* step_item = self->mStepList->getFirstSelected();
	if (!step_item) return;

	self->mLibraryList->deselectAllItems();
	self->refresh();
}

//static
void LLPreviewGesture::onCommitAnimation(LLUICtrl* ctrl, void* data)
{
	LLPreviewGesture* self = (LLPreviewGesture*)data;
	if (!self) return;

	LLScrollListItem* step_item = self->mStepList->getFirstSelected();
	if (step_item)
	{
		LLGestureStep* step = (LLGestureStep*)step_item->getUserdata();
		if (step->getType() == STEP_ANIMATION)
		{
			// Assign the animation name
			LLGestureStepAnimation* anim_step = (LLGestureStepAnimation*)step;
			if (self->mAnimationCombo->getCurrentIndex() == 0)
			{
				anim_step->mAnimName.clear();
				anim_step->mAnimAssetID.setNull();
			}
			else
			{
				anim_step->mAnimName = self->mAnimationCombo->getSimple();
				anim_step->mAnimAssetID =
					self->mAnimationCombo->getCurrentID();
			}
#if 0
			anim_step->mFlags = 0x0;
#endif

			// Update the UI label in the list
			updateLabel(step_item);

			self->mDirty = true;
			self->refresh();
		}
	}
}

//static
void LLPreviewGesture::onCommitAnimationTrigger(LLUICtrl* ctrl, void *data)
{
	LLPreviewGesture* self = (LLPreviewGesture*)data;
	if (!self) return;

	LLScrollListItem* step_item = self->mStepList->getFirstSelected();
	if (step_item)
	{
		LLGestureStep* step = (LLGestureStep*)step_item->getUserdata();
		if (step->getType() == STEP_ANIMATION)
		{
			LLGestureStepAnimation* anim_step = (LLGestureStepAnimation*)step;
			if (self->mAnimationRadio->getSelectedIndex() == 0)
			{
				// Start
				anim_step->mFlags &= ~ANIM_FLAG_STOP;
			}
			else
			{
				// Stop
				anim_step->mFlags |= ANIM_FLAG_STOP;
			}
			// Update the UI label in the list
			updateLabel(step_item);

			self->mDirty = true;
			self->refresh();
		}
	}
}

//static
void LLPreviewGesture::onCommitSound(LLUICtrl* ctrl, void* data)
{
	LLPreviewGesture* self = (LLPreviewGesture*)data;
	if (!self) return;

	LLScrollListItem* step_item = self->mStepList->getFirstSelected();
	if (step_item)
	{
		LLGestureStep* step = (LLGestureStep*)step_item->getUserdata();
		if (step->getType() == STEP_SOUND)
		{
			// Assign the sound name
			LLGestureStepSound* sound_step = (LLGestureStepSound*)step;
			sound_step->mSoundName = self->mSoundCombo->getSimple();
			sound_step->mSoundAssetID = self->mSoundCombo->getCurrentID();
			sound_step->mFlags = 0x0;

			// Update the UI label in the list
			updateLabel(step_item);

			self->mDirty = true;
			self->refresh();
		}
	}
}

//static
void LLPreviewGesture::onCommitChat(LLUICtrl* ctrl, void* data)
{
	LLPreviewGesture* self = (LLPreviewGesture*)data;
	if (!self) return;

	LLScrollListItem* step_item = self->mStepList->getFirstSelected();
	if (!step_item) return;

	LLGestureStep* step = (LLGestureStep*)step_item->getUserdata();
	if (step->getType() != STEP_CHAT) return;

	LLGestureStepChat* chat_step = (LLGestureStepChat*)step;
	chat_step->mChatText = self->mChatEditor->getText();
	chat_step->mFlags = 0x0;

	// Update the UI label in the list
	updateLabel(step_item);

	self->mDirty = true;
	self->refresh();
}

//static
void LLPreviewGesture::onCommitWait(LLUICtrl* ctrl, void* data)
{
	LLPreviewGesture* self = (LLPreviewGesture*)data;
	if (!self) return;

	LLScrollListItem* step_item = self->mStepList->getFirstSelected();
	if (!step_item) return;

	LLGestureStep* step = (LLGestureStep*)step_item->getUserdata();
	if (step->getType() != STEP_WAIT) return;

	LLGestureStepWait* wait_step = (LLGestureStepWait*)step;
	U32 flags = 0x0;
	if (self->mWaitAnimCheck->get()) flags |= WAIT_FLAG_ALL_ANIM;
	if (self->mWaitTimeCheck->get()) flags |= WAIT_FLAG_TIME;
	wait_step->mFlags = flags;

	{
		LLLocale locale(LLLocale::USER_LOCALE);

		wait_step->mWaitSeconds =
			llclamp((F32)atof(self->mWaitTimeEditor->getText().c_str()), 0.f,
					3600.f);
	}

	// Enable the input area if necessary
	self->mWaitTimeEditor->setEnabled(self->mWaitTimeCheck->get());

	// Update the UI label in the list
	updateLabel(step_item);

	self->mDirty = true;
	self->refresh();
}

//static
void LLPreviewGesture::onCommitWaitTime(LLUICtrl* ctrl, void* data)
{
	LLPreviewGesture* self = (LLPreviewGesture*)data;
	if (!self) return;

	LLScrollListItem* step_item = self->mStepList->getFirstSelected();
	if (!step_item) return;

	LLGestureStep* step = (LLGestureStep*)step_item->getUserdata();
	if (step && step->getType() == STEP_WAIT)
	{
		self->mWaitTimeCheck->set(true);
		onCommitWait(ctrl, data);
	}
}

//static
void LLPreviewGesture::onKeystrokeCommit(LLLineEditor* caller,
										 void* data)
{
	// Just commit every keystroke
	onCommitSetDirty(caller, data);
}

//static
void LLPreviewGesture::onClickAdd(void* data)
{
	LLPreviewGesture* self = (LLPreviewGesture*)data;
	if (!self) return;

	LLScrollListItem* library_item = self->mLibraryList->getFirstSelected();
	if (!library_item) return;

	S32 library_item_index = self->mLibraryList->getFirstSelectedIndex();

	const LLScrollListCell* library_cell = library_item->getColumn(0);
	const std::string& library_text = library_cell->getValue().asString();

	if (library_item_index >= STEP_EOF)
	{
		llerrs << "Unknown step type: " << library_text << llendl;
	}

	self->addStep((EStepType)library_item_index);
	self->mDirty = true;
	self->refresh();
}

LLScrollListItem* LLPreviewGesture::addStep(const EStepType step_type)
{
	// Order of enum EStepType MUST match the library_list element in
	// floater_preview_gesture.xml

	LLGestureStep* step = NULL;
	switch (step_type)
	{
		case STEP_ANIMATION:
			step = new LLGestureStepAnimation();
			break;
		case STEP_SOUND:
			step = new LLGestureStepSound();
			break;
		case STEP_CHAT:
			step = new LLGestureStepChat();
			break;
		case STEP_WAIT:
			step = new LLGestureStepWait();
			break;
		default:
			llerrs << "Unknown step type: " << (S32)step_type << llendl;
	}

	// Create an enabled item with this step
	LLSD row;
	row["columns"][0]["value"] = step->getLabel();
	row["columns"][0]["font"] = "SANSSERIF_SMALL";
	LLScrollListItem* step_item = mStepList->addElement(row);
	if (step_item)	// Out of memory...
	{
		step_item->setUserdata(step);

		// And move selection to the list on the right
		mLibraryList->deselectAllItems();
		mStepList->deselectAllItems();

		step_item->setSelected(true);
	}

	return step_item;
}

//static
void LLPreviewGesture::onClickUp(void* data)
{
	LLPreviewGesture* self = (LLPreviewGesture*)data;
	if (!self) return;

	S32 selected_index = self->mStepList->getFirstSelectedIndex();
	if (selected_index > 0)
	{
		self->mStepList->swapWithPrevious(selected_index);
		self->mDirty = true;
		self->refresh();
	}
}

//static
void LLPreviewGesture::onClickDown(void* data)
{
	LLPreviewGesture* self = (LLPreviewGesture*)data;
	if (!self) return;

	S32 selected_index = self->mStepList->getFirstSelectedIndex();
	if (selected_index < 0) return;

	S32 count = self->mStepList->getItemCount();
	if (selected_index < count-1)
	{
		self->mStepList->swapWithNext(selected_index);
		self->mDirty = true;
		self->refresh();
	}
}

//static
void LLPreviewGesture::onClickDelete(void* data)
{
	LLPreviewGesture* self = (LLPreviewGesture*)data;
	if (!self) return;

	LLScrollListItem* item = self->mStepList->getFirstSelected();
	S32 selected_index = self->mStepList->getFirstSelectedIndex();
	if (item && selected_index >= 0)
	{
		LLGestureStep* step = (LLGestureStep*)item->getUserdata();
		if (step)
		{
			delete step;
		}

		self->mStepList->deleteSingleItem(selected_index);

		self->mDirty = true;
		self->refresh();
	}
}

//static
void LLPreviewGesture::onCommitActive(LLUICtrl* ctrl, void* data)
{
	LLPreviewGesture* self = (LLPreviewGesture*)data;
	if (!self) return;

	if (!gGestureManager.isGestureActive(self->mItemUUID))
	{
		gGestureManager.activateGesture(self->mItemUUID);
	}
	else
	{
		gGestureManager.deactivateGesture(self->mItemUUID);
	}

	// Make sure the (active) label in the inventory gets updated.
	LLViewerInventoryItem* item = gInventory.getItem(self->mItemUUID);
	if (item)
	{
		gInventory.updateItem(item);
		gInventory.notifyObservers();
	}

	self->refresh();
}

//static
void LLPreviewGesture::onClickSave(void* data)
{
	LLPreviewGesture* self = (LLPreviewGesture*)data;
	if (self)
	{
		self->saveIfNeeded();
	}
}

//static
void LLPreviewGesture::onClickPreview(void* data)
{
	LLPreviewGesture* self = (LLPreviewGesture*)data;
	if (!self) return;

	if (!self->mPreviewGesture)
	{
		// Make temporary gesture
		self->mPreviewGesture = self->createGesture();

		// Add a callback
		self->mPreviewGesture->mDoneCallback = onDonePreview;
		self->mPreviewGesture->mCallbackData = self;

		// Set the button title
		self->mPreviewBtn->setLabel(self->getString("stop_txt"));

		// Play it, and delete when done
		gGestureManager.playGesture(self->mPreviewGesture);

		self->refresh();
	}
	else
	{
		// Will call onDonePreview() below
		gGestureManager.stopGesture(self->mPreviewGesture);

		self->refresh();
	}
}

//static
void LLPreviewGesture::onDonePreview(LLMultiGesture* gesture, void* data)
{
	LLPreviewGesture* self = (LLPreviewGesture*)data;
	if (self)
	{
		self->mPreviewBtn->setLabel(self->getString("preview_txt"));

		delete self->mPreviewGesture;
		self->mPreviewGesture = NULL;

		self->refresh();
	}
}
